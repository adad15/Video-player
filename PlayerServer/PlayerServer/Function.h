#pragma once
#include <functional>
#include <unistd.h>
#include <sys/types.h>
/*一个函数：虚函数特性和模板函数特性，不能同时存在*/
/*一个模板类可以有虚函数*/
/*进程入口函数模板*/

class CSocketBase;/*因为使用的是指针和引用，可以这样操作 指针和引用占用的空间是固定的  编译是可以过去的*/
class Buffer;

class CFunctionBase {
public:
	virtual~CFunctionBase() {}
	//virtual int operator()() = 0;/*纯虚函数 强迫所有子类都实现*/
	virtual int operator()() { return -1; };
	virtual int operator()(CSocketBase*) { return -1; };
	virtual int operator()(CSocketBase*, const Buffer&) { return -1; };
};

/*模板类*/
template<typename _FUNCTION_/*初始化函数类型*/, typename... _ARGS_/*_ARGS_为不定参数类型*/>
class CFunction :public CFunctionBase {
public:
	CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func)/*完美转发*/, std::forward<_ARGS_>(args).../*表示原地展开，对每一个都调用一个forward*/)
	{}
	virtual ~CFunction() {}/*本身什么都不干，但是会触发m_binder的析构*/
	virtual int operator()() {
		return m_binder();/*帮我们把参数填入到函数中并调用  对外无需填参数*/
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};