#pragma once
#include <functional>
#include <unistd.h>
#include <sys/types.h>
/*һ���������麯�����Ժ�ģ�庯�����ԣ�����ͬʱ����*/
/*һ��ģ����������麯��*/
/*������ں���ģ��*/

class CSocketBase;/*��Ϊʹ�õ���ָ������ã������������� ָ�������ռ�õĿռ��ǹ̶���  �����ǿ��Թ�ȥ��*/
class Buffer;

class CFunctionBase {
public:
	virtual~CFunctionBase() {}
	//virtual int operator()() = 0;/*���麯�� ǿ���������඼ʵ��*/
	virtual int operator()() { return -1; };
	virtual int operator()(CSocketBase*) { return -1; };
	virtual int operator()(CSocketBase*, const Buffer&) { return -1; };
};

/*ģ����*/
template<typename _FUNCTION_/*��ʼ����������*/, typename... _ARGS_/*_ARGS_Ϊ������������*/>
class CFunction :public CFunctionBase {
public:
	CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*����ת��*/, std::forward<_ARGS_>(args).../*��ʾԭ��չ������ÿһ��������һ��forward*/)
	{}
	virtual ~CFunction() {}/*����ʲô�����ɣ����ǻᴥ��m_binder������*/
	virtual int operator()() {
		return m_binder();/*�����ǰѲ������뵽�����в�����  �������������*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};