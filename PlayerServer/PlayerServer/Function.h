#pragma once
#include <functional>
#include <unistd.h>
#include <sys/types.h>

/*
 * CSocketBase����
 * 1.��ҪĿ����ʹ�ö�̬�ԣ�
 * 
 * CFunction��
 * 
 * ��������ҪĿ���ǣ�
 * 1.�����࣬��Ҫ�Ƿ�װ��m_binder��ʵ�ֺ�������������ͺ������ã���Ҫ��ʵ����Ϊ�̺߳����ͽ��̺�����
 * 2.ͨ������ģ�����ģ��ʵ���˲�ͬ�������Դ�����ͬ���� =�� ���ú���ģ���еĲ���������
 * 3.���÷º���ʵ�ֺ�����ݵ���
 * 4.��װ��m_binder��ʵ���˺��������뺯���İ󶨣��Ա����÷º������� =�� ��������ת�� =�� ��Ϊ��ģ�����δ��������Ҫʹ������ת��
 * 
*/

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