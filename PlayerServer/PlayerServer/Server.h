#pragma once
#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Logger.h"

/*���ܷŵ�Function����ȥ   ��ֹ��������*/
template<typename _FUNCTION_/*��ʼ����������*/, typename... _ARGS_/*_ARGS_Ϊ������������*/>
class CReceivedFunction :public CFunctionBase {
public:
	CReceivedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*����ת��*/, std::forward<_ARGS_>(args).../*��ʾԭ��չ������ÿһ��������һ��forward*/)
	{}
	virtual ~CReceivedFunction() {}/*����ʲô�����ɣ����ǻᴥ��m_binder������*/
	virtual int operator()(CSocketBase* pClient, const Buffer& data) {
		return m_binder(pClient, data);/*�����ǰѲ������뵽�����в�����  �������������*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

template<typename _FUNCTION_/*��ʼ����������*/, typename... _ARGS_/*_ARGS_Ϊ������������*/>
class CConnectedFunction :public CFunctionBase {
public:
	CConnectedFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*����ת��*/, std::forward<_ARGS_>(args).../*��ʾԭ��չ������ÿһ��������һ��forward*/)
	{}
	virtual ~CConnectedFunction() {}/*����ʲô�����ɣ����ǻᴥ��m_binder������*/
	virtual int operator()(CSocketBase* pClient) {
		return m_binder(pClient);/*�����ǰѲ������뵽�����в�����  �������������*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

class CBusiness
{
public:
	CBusiness() :m_connectedcallback(NULL), m_recvcallback(NULL)
	{}
	/*Ҫ��ͣ�Ľ������������̵�fd��Ҫ���̳߳�*/
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

/*��������������̣� ��������ҵ��*/
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
	CBusiness* m_business;/*ҵ��ģ��*/
};

