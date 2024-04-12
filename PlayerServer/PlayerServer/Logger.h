#pragma once

#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>/*拿到毫秒数*/
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

/*mapClients有什么用？？？？？？？？？？？？？？？？*/

/*
 * 进程间通信：
 * 1.Process.h： socketpair 绑定本地socket和管道 + sendmsg/recvmsg 发送消息 msg
 * 2.Socket.h：  本地套接字 write + read
 *
*/

/*
 * 日志服务器：只有一个线程，主线程进行控制，子线程进行处理
 * 
 * LogInfo类：
 * 1.重载了<<运算符
 * 2.主要是为存放日志
 * 
 * CLoggerServer类：
 * 1.构造函数：
 *		注册线程工作函数m_thread + 获取路径用来存放工作日志
 * 2.Start 成员函数 =》 日志服务器主进程调用，为主线程：
 *		这个函数主要是进行连接等控制操作
 *		创建日志文件 + 创建epoll(1个接口) + Init 初始化客户端本地套接字(socket+bind+listen) =》 ./log/server.sock 进程通信链接文件
 + 将服务器加入epoll 
 + 启动线程函数m_thread（设置信号集 + 注册信号回调函数 + 等待信号，非阻塞 + 运行线程工作函数ThreadFunc）
 * 
 * 3.ThreadFunc 子线程工作函数：
 *		创建事件队列 + 建立文件描述符和套接字的映射表mapClients + 循环处理事件
 *	循环调用WaitEvent + 遍历WaitEvent返回的有相应的事件：
 *		服务器收到输入请求：(服务器accept和客户端建立连接 + 利用accept返回信息创建客户端套接字) + 将新连接的客户端加入红黑树
 *			+ 如果映射表已经存放该客户端将该客户端从映射表中删除  + 将新连接的客户端加入到映射表mapClients
 *		客户端有输入请求：利用epoll返回的events[i].data.ptr创建客户端套接字 + Recv 得到其他进程调用Trace发送的日志 + 调用WriteLog将信息写入到本地日志文件
 *		
 * 4.WriteLog 成员函数：
 *		将其他进程通过本地套接字传入的信息写入到本地日志文件
 * 5.Trace 静态函数：
 *		给其他进程提供的接口 =》 通过thread_local给每个线程分配客户端套接字 =》 Init 初始化客户端套接字，./log/server.sock 进程通信链接文件
 *		=》 Link connect 客户端 =》 其余线程调用Send函数给日志处理子线程发送日志信息
 * 6.Close 成员函数：=》 日志服务器主进程中的子进程调用Close函数
 *		关闭m_server
 *		关闭m_epoll
 *		关闭m_thread
 * 
*/

enum {
	LOG_INFO,/*普通信息*/
	LOG_DEBUG,/*调试*/
	LOG_WARNING,/*警告*/
	LOG_ERROR,/*错误*/
	LOG_FATAL/*致命错误*/
};

class LogInfo {
public:
	LogInfo(/*Trace 发送的*/
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...);
	LogInfo(/*自己主动发送的  流式的日志*/
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	LogInfo(/*dump 发送的*/
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize);
	
	~LogInfo();
	operator Buffer()const {
		return m_buf;
	}
	template<typename T>
	LogInfo& operator<<(const T& data) {
		std::stringstream stream;
		stream << data;
		m_buf += stream.str();
		return *this;
	}
private:
	bool bAuto;/*默认false 流式日志则为true*/
	Buffer m_buf;
};

class CLoggerServer {
public:
	CLoggerServer() :m_thread(&CLoggerServer::ThreadFunc, this) {
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof(curpath));/*获取当前路径*/
		m_path = curpath;
		m_path += "/log/" + GetTimeStr() + ".log";
		printf("%s(%d):[%s] path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
	}
	~CLoggerServer(){
		Close();
	}
public:
	CLoggerServer(const CLoggerServer&) = delete;
	CLoggerServer& operator=(const CLoggerServer&) = delete;
public:
	/*日志服务器启动*/
	int Start() {
		if (m_server != NULL)return -1;
		if (access("log", W_OK | R_OK) != 0) {/*看有没有读写的权限*/
			/*说明文件夹不存在 创建文件夹*/
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);		
		}
		m_file = fopen(m_path, "w+");
		if (m_file == NULL) return -2;
		int ret = m_epoll.Create(1);/*创建epoll*/
		if (ret != 0) return -3;
		/*创建并初始化套接字+bind+listen*/
		m_server = new CSocket();
		if (m_server == NULL) {
			Close();
			return -4;
		}
		/*SOCK_ISSERVER为什么只有一个参数？？？？？？？*/
		ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER));
		if (ret != 0) {
			Close();
			return -5;
		}
		ret = m_epoll.Add(*m_server, EpollDate((void*)m_server), EPOLLIN | EPOLLERR);
		if (ret != 0) {
			Close();
			return -6;
		}
		/*在构造函数中已经创建了m_thread*/
		m_thread.Start();
		if (ret != 0) {
			Close();
			return -7;
		}
		return 0;
	}
	
	int Close() {
		if (m_server != NULL) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}
	/*给其他非日志进程的进程和线程使用*/
	static void Trace(const LogInfo& info) {
		/*每一个线程调用这个函数，客户端都会调用一次构造函数  也就是每个线程对应一个客户端*/
		static thread_local CSocket client;
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam("./log/server.sock", 0));
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif // _DEBUG
				return;
			}
			printf("%s(%d):[%s] ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link();/*connect*/
			printf("%s(%d):[%s] ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			if (ret != 0)printf("%s(%d):<%s> ret=%d errno:%d msg:%s\n",__FILE__, __LINE__, __FUNCTION__, ret, errno, strerror(errno));
		}
		ret = client.Send(info);
		printf("%s(%d):[%s] ret=%d client=%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
	}

	static Buffer GetTimeStr() {
		Buffer result(128);
		timeb tmb;
		ftime(&tmb);
		tm* pTm = localtime(&tmb.time);/*转成本地时间*/
		int nSize = snprintf(result, result.size(), "%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm);
		result.resize(nSize);
		return result;
	}
private:
	/*线程入口函数*/
	int ThreadFunc() {
		printf("%s(%d):[%s]%d\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isValid());
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll);
		printf("%s(%d):[%s] %p\n", __FILE__, __LINE__, __FUNCTION__, m_server);
		EPEvents events;
		std::map<int, CSocketBase*> mapClients;/*文件描述符和套接字的映射表，储存起来 和链表相比可以随意释放*/
		/*      线程是否有效             是否关闭     本地套接字是否有效*/
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL))
		{
			ssize_t ret = m_epoll.WaitEvent(events, 1);
			//printf("%s(%d):[%s] ret %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break;
			if (ret > 0) {
				/*遍历有响应的事件*/
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						/*有输入*/
						if (events[i].data.ptr == m_server) {/*服务器收到输入请求,表明有连接*/
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient);/*服务器accept + 利用accept返回信息创建客户端套接字*/
							printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) continue;
							/*将新连接的客户端加入红黑树*/
							r = m_epoll.Add(*pClient, EpollDate((void*)pClient), EPOLLIN | EPOLLERR);
							printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) {
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient);	
							/*有bug当第一次查找时，指向end，也不为空 崩溃，但是这个是子进程的，liunx中的gdb是没有办法管到这里的*/
							if (it != mapClients.end()) {
								if (it->second) delete it->second;
							}
// 							if (it->second != NULL) { 		
// 								delete it->second; 								
// 							}
							mapClients[*pClient] = pClient;
						}
						else {
							/*客户端有输入请求  表示需要读了*/
							printf("%s(%d):[%s] events[i].data.ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient != NULL) {
								Buffer data(1024 * 1024);
								int r = pClient->Recv(data);
								printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
								if (r <= 0) {
									mapClients[*pClient] = NULL;/*这里不能放在下面，因为*pClient会对空指针解引用*/
									delete pClient;
								}
								else {
									printf("%s(%d):[%s] data=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)data);
									WriteLog(data);
								}
							}
						}
						printf("%s(%d):[%s]\n", __FILE__, __LINE__, __FUNCTION__);
					}
				}
				if (i != ret) {
					break;/*整个线程break掉*/
				}
			}
		}
		/*防止内存泄漏*/
		for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
			if (it->second) {/*非空*/
				delete it->second;
			}
		}
		mapClients.clear();
		return 0;
	}
	void WriteLog(const Buffer& data) {
		if (m_file != NULL) {
			FILE* pFile = m_file;
			fwrite((char*)data, 1, data.size(), pFile);
			fflush(pFile);
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif 
		}
	}
private:
	CThread m_thread;
	CEpoll m_epoll;
	CSocketBase* m_server;
	Buffer m_path;
	FILE* m_file;
};

#ifndef TRACE
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_INFO, __VA_ARGS__/*前面的...填进里面*/))
#define TRACED(...)CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...)CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_WARNING, __VA_ARGS__))
#define TRACEE(...)CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_ERROR, __VA_ARGS__))
#define TRACEF(...)CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_FATAL, __VA_ARGS__))
	//LOGI<<"hello"<<"how are you";
#define LOGI LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_FATAL)

//内存导出
//00 01 02 65…… ; ...a ……
//
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_FATAL, data, size))
#endif