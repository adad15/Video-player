#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Logger.h"

/*
 * 网络服务器
 * 
 * 程序主进程中完成：所以线程池和epoll都在主进程
 * 线程池中使用epoll来处理客户端的网络连接  =》 第一个线程池
 *
*/

/*
 * CReceivedFunction类和CConnectedFunction类继承CFunctionBase：
 * 1.和CFunction不同的是重写了int operator()
 * 2.是为了更方便使用连接、接收回调函数
 * 
 * CBusiness类：=》 抽象类封装了连接、接收回调函数
 * 1.BusinessProcess 成员函数：不停的接收来自主进程的fd
 * 2.setConnectedCallback、setRecvCallback 成员函数：设置连接、接收回调函数
 * 
 * CServer类：子进程之一  =》 网络服务器进程， 处理连接业务
 * 1.Init 成员函数：
 *		程序主进程调用SetEntryFunction函数设置网络服务器子进程接口函数 + 
 *		程序主进程调用CreatSubProcess函数创建网络服务器子进程，并运行网络服务器子进程接口函数BusinessProcess + 
 *		开线程池，包含2个初始线程 + 开epoll，设置2个接口 + 设置服务端socket + Init(socket + bind + listen) + 
 *		将服务器加入到epoll红黑树中 + 给线程池中每个线程添加一个任务（实际处理函数ThreadFunc）
 * 
 * 2.ThreadFunc 成员函数：=》 线程池线程实际工作函数
 *		程序主进程调用
 *		封装WaitEvent函数
 *		服务器收到输入请求 =》 服务器accpet + 利用accpet得到的客户端文件描述符 + 进程函数SendSocket将客户端套接字信息发送给网络服务器子进程
 * 
 * 3.Run 成员函数：无限等待
 * 
 * 4.Close 成员函数：
 *		关闭epoll
 *		关闭线程池
 *		调用SendFD给程序主进程发送文件描述符，表示关闭网络服务器子进程
*/



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
	int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 9999);
	int Run();
	int Close();
private:
	/*线程池线程实际工作函数*/
	/*主要处理与客户端的网络连接，将客户端文件描述符发送给处理线程*/
	int ThreadFunc();
private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll;
	CProcess m_process;
	CBusiness* m_business;/*业务模块*/
};