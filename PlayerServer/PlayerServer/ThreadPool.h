#pragma once

#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"

/*
 * 
 * 接口：AddTask 不定参数的函数模板
 * 
 * std::vector<CThread*> m_threads;
 * 通过vector容器维护线程队列
 * 
 * 线程间通信：自定义信号 来控制线程的休眠与关闭
 * 
 * 线程池就相当于服务端，各个线程就相当于客户端
 * 
 * 分层结构：
 * 线程接口函数（线程类） =》线程工作函数（线程池类） =》实际工作处理函数（main，最外层）
 * 
 */

/*
 * CThreadPool线程池类：
 * 1.构造函数：
 *		创建进程间通信的连接文件.sock文件（m_path）
 * 2.Start 成员函数：
 *		创建本地套接字 + Init 通过.sock文件建立服务器的本地套接字连接 + 创建epoll，并将服务器套接字加入到epoll红黑树中
 *		+ 将 TaskDispatch 函数设置为每个线程的工作函数 + 启动线程（设置信号，运行线程工作函数 TaskDispatch）
 * 3.TaskDispatch 成员函数：
 *											服务端服务器收到输入请求：服务器 Link 得到新连接的客户端 + 将新连接的客户端加入到epoll
 *		封装了 WaitEvent 函数，进入循环  
 *											客户端服务器收到输入请求：events[i].data.ptr得到有事件相应的客户端 + 调用pClient的Recv得到线程工作函数
 *											+ 运行传入的线程工作函数
 * 4.AddTask 函数模板：=》 线程池得到最外层实际的处理函数函数
 *		通过 thread_local 每个线程得到不同客户端socket + Init + Link + 将得到的实际处理函数指针发送给 TaskDispatch 成员函数
 * 5.Close 成员函数：
 *		m_epoll关闭
 *		关闭线程池里的每一个thread
 *		清空线程队列
 *		关闭连接文件m_path
 * 
*/

class CThreadPool {
public:
	CThreadPool() {/*创建进程间通信的连接文件*/
		m_server = NULL;
		/*为了更改路径m_path保证每个线程池不一样引入这个时钟函数*/
		timespec tp = { 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = NULL;
		/*asprintf 系统分配内存   得到文件名*/
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000);
		if (buf != NULL) {
			m_path = buf;
			free(buf);
		}/*有问题的话，在start接口里面判断m_path来解决问题*/
		usleep(1);/*防止多个线程池一起创建，m_path会重复*/
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;
public:
	int Start(unsigned count) {
		int ret = 0;
		if (m_server != NULL) return -1;/*已经初始化了*/
		if (m_path.size() == 0) return -2;/*构造函数失败*/
		m_server = new CSocket();
		if (m_server == NULL)return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));
		if (ret != 0)return -4;
		ret = m_epoll.Create(count);
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollDate((void*)m_server));
		if (ret != 0) return - 6;
		m_threads.resize(count);
		for (unsigned i{}; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);/*通过CFunction类实现的自动this填入函数TaskDispatch中，用运行该函数*/
			if (m_threads[i] == NULL)return -7;
			ret = m_threads[i]->Start();
			if (ret != 0) return -8;
		}
		return 0;
	}
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads) {
			if (thread)delete thread;
		}
		m_threads.clear();
		unlink(m_path);/*关闭连接文件*/
	}
	template<typename _FUNCTION_, typename..._ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_...args) {/*多个不同的线程调用*/
		static thread_local CSocket client;/*每个线程的client都不一样，同一个线程一样，不同线程client不一样*/
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));
			if (ret != 0)return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (base == NULL) return -3;
		/*把指针发送过去  TaskDispatch来接受  跨线程通信*/
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data);
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
	size_t Size() const { return m_threads.size(); }
private:
	/*每个线程的线程工作函数*/
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret{};
			ssize_t esize = m_epoll.WaitEvent(events);
			if (esize > 0) {
				for (ssize_t i{}; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {/*客户端请求连接*/
							ret = m_server->Link(&pClient);
							if(ret!=0)continue;
							ret = m_epoll.Add(*pClient, EpollDate((void*)pClient));
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else/*客户端的数据来了*/
						{
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)();/*调用运行实际处理函数*/
									delete base;
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;
	/*！！大坑！！容器里面对象一定要有复制构造函数，不能复制怎么放容器里面。线程不能复制，所以这里是指针*/
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;/*本地套接字通信使用*/
}; 