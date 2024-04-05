#pragma once
#include <functional>
#include <unistd.h>
#include <sys/types.h>

/*������ں���ģ��*/
class CFunctionBase {
public:
	virtual~CFunctionBase() {}
	virtual int operator()() = 0;/*���麯�� ǿ���������඼ʵ��*/
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