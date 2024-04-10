#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Logger.h"

/*不能放到Function里面去   防止交叉引用*/
template<typename _FUNCTION_/*初始化函数类型*/, typename... _ARGS_/*_ARGS_为不定参数类型*/>
class CReceivedFunction :public CFunctionBase {
public:
	CReceivedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*完美转发*/, std::forward<_ARGS_>(args).../*表示原地展开，对每一个都调用一个forward*/)
	{}
	virtual ~CReceivedFunction() {}/*本身什么都不干，但是会触发m_binder的析构*/
	virtual int operator()(CSocketBase* pClient, const Buffer& data) {
		return m_binder(pClient, data);/*帮我们把参数填入到函数中并调用  对外无需填参数*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

template<typename _FUNCTION_/*初始化函数类型*/, typename... _ARGS_/*_ARGS_为不定参数类型*/>
class CConnectedFunction :public CFunctionBase {
public:
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*完美转发*/, std::forward<_ARGS_>(args).../*表示原地展开，对每一个都调用一个forward*/)
	{}
	virtual ~CConnectedFunction() {}/*本身什么都不干，但是会触发m_binder的析构*/
	virtual int operator()(CSocketBase* pClient) {
		return m_binder(pClient);/*帮我们把参数填入到函数中并调用  对外无需填参数*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

class CBusiness
{
public:
	CBusiness() :m_connectedcallback(NULL), m_recvcallback(NULL)
	{}
	/*要不停的接收来自主进程的fd，要起线程池*/
	virtual int BusinessProcess(CProcess* proc) = 0;

	template<typename _FUNCTION_, typename..._ARGS_>
	int setConnectedCallback(_FUNCTION_ func, _ARGS_...args) {
		m_connectedcallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_connectedcallback == NULL)return -1;
		return 0;
	}
	template<typename _FUNCTION_, typename..._ARGS_>
	int setRecvCallback(_FUNCTION_ func, _ARGS_...args) {
		m_recvcallback = new CReceivedFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_recvcallback == NULL)return -1;
		return 0;
	}
protected:
	CFunctionBase* m_connectedcallback;
	CFunctionBase* m_recvcallback;
};

/*网络服务器主进程， 处理连接业务*/
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

