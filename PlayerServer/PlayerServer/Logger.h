#pragma once

#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>/*�õ�������*/
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

/*mapClients��ʲô�ã�������������������������������*/

/*
 * ���̼�ͨ�ţ�
 * 1.Process.h�� socketpair �󶨱���socket�͹ܵ� + sendmsg/recvmsg ������Ϣ msg
 * 2.Socket.h��  �����׽��� write + read
 *
*/

/*
 * ��־��������ֻ��һ���̣߳����߳̽��п��ƣ����߳̽��д���
 * 
 * LogInfo�ࣺ
 * 1.������<<�����
 * 2.��Ҫ��Ϊ�����־
 * 
 * CLoggerServer�ࣺ
 * 1.���캯����
 *		ע���̹߳�������m_thread + ��ȡ·��������Ź�����־
 * 2.Start ��Ա���� =�� ��־�����������̵��ã�Ϊ���̣߳�
 *		���������Ҫ�ǽ������ӵȿ��Ʋ���
 *		������־�ļ� + ����epoll(1���ӿ�) + Init ��ʼ���ͻ��˱����׽���(socket+bind+listen) =�� ./log/server.sock ����ͨ�������ļ�
 + ������������epoll 
 + �����̺߳���m_thread�������źż� + ע���źŻص����� + �ȴ��źţ������� + �����̹߳�������ThreadFunc��
 * 
 * 3.ThreadFunc ���̹߳���������
 *		�����¼����� + �����ļ����������׽��ֵ�ӳ���mapClients + ѭ�������¼�
 *	ѭ������WaitEvent + ����WaitEvent���ص�����Ӧ���¼���
 *		�������յ���������(������accept�Ϳͻ��˽������� + ����accept������Ϣ�����ͻ����׽���) + �������ӵĿͻ��˼�������
 *			+ ���ӳ����Ѿ���Ÿÿͻ��˽��ÿͻ��˴�ӳ�����ɾ��  + �������ӵĿͻ��˼��뵽ӳ���mapClients
 *		�ͻ�����������������epoll���ص�events[i].data.ptr�����ͻ����׽��� + Recv �õ��������̵���Trace���͵���־ + ����WriteLog����Ϣд�뵽������־�ļ�
 *		
 * 4.WriteLog ��Ա������
 *		����������ͨ�������׽��ִ������Ϣд�뵽������־�ļ�
 * 5.Trace ��̬������
 *		�����������ṩ�Ľӿ� =�� ͨ��thread_local��ÿ���̷߳���ͻ����׽��� =�� Init ��ʼ���ͻ����׽��֣�./log/server.sock ����ͨ�������ļ�
 *		=�� Link connect �ͻ��� =�� �����̵߳���Send��������־�������̷߳�����־��Ϣ
 * 6.Close ��Ա������=�� ��־�������������е��ӽ��̵���Close����
 *		�ر�m_server
 *		�ر�m_epoll
 *		�ر�m_thread
 * 
*/

enum {
	LOG_INFO,/*��ͨ��Ϣ*/
	LOG_DEBUG,/*����*/
	LOG_WARNING,/*����*/
	LOG_ERROR,/*����*/
	LOG_FATAL/*��������*/
};

class LogInfo {
public:
	LogInfo(/*Trace ���͵�*/
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...);
	LogInfo(/*�Լ��������͵�  ��ʽ����־*/
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);
	LogInfo(/*dump ���͵�*/
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
	bool bAuto;/*Ĭ��false ��ʽ��־��Ϊtrue*/
	Buffer m_buf;
};

class CLoggerServer {
public:
	CLoggerServer() :m_thread(&CLoggerServer::ThreadFunc, this) {
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof(curpath));/*��ȡ��ǰ·��*/
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
	/*��־����������*/
	int Start() {
		if (m_server != NULL)return -1;
		if (access("log", W_OK | R_OK) != 0) {/*����û�ж�д��Ȩ��*/
			/*˵���ļ��в����� �����ļ���*/
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);		
		}
		m_file = fopen(m_path, "w+");
		if (m_file == NULL) return -2;
		int ret = m_epoll.Create(1);/*����epoll*/
		if (ret != 0) return -3;
		/*��������ʼ���׽���+bind+listen*/
		m_server = new CSocket();
		if (m_server == NULL) {
			Close();
			return -4;
		}
		/*SOCK_ISSERVERΪʲôֻ��һ��������������������*/
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
		/*�ڹ��캯�����Ѿ�������m_thread*/
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
	/*����������־���̵Ľ��̺��߳�ʹ��*/
	static void Trace(const LogInfo& info) {
		/*ÿһ���̵߳�������������ͻ��˶������һ�ι��캯��  Ҳ����ÿ���̶߳�Ӧһ���ͻ���*/
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
		tm* pTm = localtime(&tmb.time);/*ת�ɱ���ʱ��*/
		int nSize = snprintf(result, result.size(), "%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm);
		result.resize(nSize);
		return result;
	}
private:
	/*�߳���ں���*/
	int ThreadFunc() {
		printf("%s(%d):[%s]%d\n", __FILE__, __LINE__, __FUNCTION__, m_thread.isValid());
		printf("%s(%d):[%s] %d\n", __FILE__, __LINE__, __FUNCTION__, (int)m_epoll);
		printf("%s(%d):[%s] %p\n", __FILE__, __LINE__, __FUNCTION__, m_server);
		EPEvents events;
		std::map<int, CSocketBase*> mapClients;/*�ļ����������׽��ֵ�ӳ����������� ��������ȿ��������ͷ�*/
		/*      �߳��Ƿ���Ч             �Ƿ�ر�     �����׽����Ƿ���Ч*/
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL))
		{
			ssize_t ret = m_epoll.WaitEvent(events, 1);
			//printf("%s(%d):[%s] ret %d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret < 0) break;
			if (ret > 0) {
				/*��������Ӧ���¼�*/
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						/*������*/
						if (events[i].data.ptr == m_server) {/*�������յ���������,����������*/
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient);/*������accept + ����accept������Ϣ�����ͻ����׽���*/
							printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) continue;
							/*�������ӵĿͻ��˼�������*/
							r = m_epoll.Add(*pClient, EpollDate((void*)pClient), EPOLLIN | EPOLLERR);
							printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
							if (r < 0) {
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient);	
							/*��bug����һ�β���ʱ��ָ��end��Ҳ��Ϊ�� ����������������ӽ��̵ģ�liunx�е�gdb��û�а취�ܵ������*/
							if (it != mapClients.end()) {
								if (it->second) delete it->second;
							}
// 							if (it->second != NULL) { 		
// 								delete it->second; 								
// 							}
							mapClients[*pClient] = pClient;
						}
						else {
							/*�ͻ�������������  ��ʾ��Ҫ����*/
							printf("%s(%d):[%s] events[i].data.ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient != NULL) {
								Buffer data(1024 * 1024);
								int r = pClient->Recv(data);
								printf("%s(%d):[%s] r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);
								if (r <= 0) {
									mapClients[*pClient] = NULL;/*���ﲻ�ܷ������棬��Ϊ*pClient��Կ�ָ�������*/
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
					break;/*�����߳�break��*/
				}
			}
		}
		/*��ֹ�ڴ�й©*/
		for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
			if (it->second) {/*�ǿ�*/
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
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__,__FUNCTION__, getpid(), pthread_self(),LOG_INFO, __VA_ARGS__/*ǰ���...�������*/))
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

//�ڴ浼��
//00 01 02 65���� ; ...a ����
//
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__,__LINE__, __FUNCTION__, getpid(),pthread_self(), LOG_FATAL, data, size))
#endif