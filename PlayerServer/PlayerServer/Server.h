#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Logger.h"

class CBusiness
{
public:
	/*要不停的接收来自主进程的fd，要起线程池*/
	virtual int BusinessProcess() = 0;

	template<typename _FUNCTION_, typename..._ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_...args) {
		m_connectedcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedcallback == NULL)return -1;
		return 0;
	}
	template<typename _FUNCTION_, typename..._ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_...args) {
		m_recvcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvcallback == NULL)return -1;
		return 0;
	}
private:
	CFunctionBase* m_connectedcallback;
	CFunctionBase* m_recvcallback;
};

class CServer
{
public:
	CServer();
	~CServer() { Close(); }
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;
public:
	int Init(CBusiness* business, const Buffer& ip = "127.0.0.1", short port = 9999);
	int Run();
	int Close();
private:
	int ThreadFunc();
private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll;
	CProcess m_process;
	CBusiness* m_business;/*业务模块*/
};

