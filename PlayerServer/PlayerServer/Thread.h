#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include "Function.h"
#include <cstdio>
#include <signal.h>
#include <map>
#include <errno.h>

/*
 * 接口：线程工作函数
 *
*/

/*
 * 静态成员m_mapThread： 线程id与线程CThread类的映射
 * 
 * CThread类：
 * 
 * 1.这个类封装了单个线程
 * 2.构造函数：
 *		默认构造函数：置空变量
 *		模板构造函数：new CFunction设置线程工作函数，将参数和线程函数绑定
 * 3.SetThreadFunc成员函数：
 *		new CFunction设置线程工作函数，将参数和线程函数绑定
 * 4.Start 成员函数：
 *		封装pthread_create函数，线程函数必须为静态成员函数，因为类的非静态成员函数只能通过类的对象去调用，
但是创建线程时从哪里能获得类的对象而去调用类的成员函数呢? 所以线程函数必须为静态。
 *		静态函数没有CThread对象的this指针，所以第四个参数是this指针，将CThread对象指针传给线程函数
 *		线程函数的本质就是回调函数
 *		将线程id与线程类的映射加入静态映射表
 * 5.Pause 成员函数
 *		封装pthread_kill函数，发送自定义SIGUSR1信号，该线程进入到信号响应函数
 * 6.Stop 成员函数
 *		封装pthread_detach函数，销毁子线程 + pthread_kill，发送自定义信号SIGUSR2
 * 7.ThreadEntry线程函数
 *		通过传入的this指针，调用真正的线程成员函数
 *		设置信号集 + 注册信号回调函数 + 等待SIGUSR1、SIGUSR2信号 + 从静态映射表中删除映射
 * 8.信号回调函数（静态）
 *		因为是静态的，全局只有一个信号回调函数，所以需要之前定义的静态映射表拿到线程对象 + 进行信号处理
 * 9.EnterThread真正的线程函数
 *		调用线程工作函数（CFunctionBase类）
*/

class CThread {
public:
	CThread() {
		m_function = NULL; 
		m_thread = 0;
		m_bpaused = false;
	}

	/*模板构造函数：new CFunction设置线程工作函数，将参数和线程函数绑定*/
	template<typename _FUNCTION_,typename..._ARGS_>
	CThread(_FUNCTION_ func, _ARGS_...args) :
		m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) 
	{
		m_thread = 0;
	}
	~CThread() {}
public:
	/*线程是不能被复制的，所以为了安全性这个函数要删除*/
	CThread(const CThread&) = delete;
	CThread& operator=(const CThread&) = delete;
public:
	/*设置线程工作函数*/
	template<typename _FUNCTION_, typename..._ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_...args) {
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (m_function == NULL) return -1;
		return 0;
	}
	int Start() {
		pthread_attr_t attr;
		int ret = 0;
		pthread_attr_init(&attr);
		if (ret != 0) return -1;
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE/*设置这个需要主线程在join一下*/);
		if (ret != 0) return -2;
		//pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);/*进程内竞争  默认就是*/
		//if (ret != 0) return -3;
		pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
		if (ret != 0) return -4;
		m_mapThread[m_thread] = this;/*建立线程id与线程类的映射*/
		pthread_attr_destroy(&attr);
		if (ret != 0) return -5;
		return 0;
	}
	/*线程暂停函数*/
	int Pause() {
		if (m_thread != 0) return -1;
		if (m_bpaused) {
			m_bpaused = false;
			return 0;
		}
		m_bpaused = true;
		int ret = pthread_kill(m_thread, SIGUSR1);
		if (ret != 0) {
			m_bpaused = false;
			return -2;
		} 
		return 0;
	}
	/*线程结束函数*/
	int Stop() {
		if (m_thread != 0) {
			pthread_t thread = m_thread;
			m_thread = 0;
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 1000000;/*100ms*/
			int ret = pthread_timedjoin_np(thread, NULL, &ts);
			if (ret == ETIMEDOUT) {/*等待超时 线程有效*/
				/*作用：从状态上实现线程分离，注意不是指该线程独自占用地址空间。
				线程分离状态：指定该状态，线程主动与主控线程断开关系。线程结束后（不会产生僵尸线程），其退出状态不由其他线程获取，
				而直接自己自动释放（自己清理掉PCB的残留资源）。网络、多线程服务器常用。
				进程若有该机制，将不会产生僵尸进程。僵尸进程的产生主要由于进程死后，大部分资源被释放，一点残留资源仍存于系统中，
				导致内核认为该进程仍存在。（注意进程没有这一机制）*/
				pthread_detach(thread);
				/*向线程发送一个signal*/
				pthread_kill(thread, SIGUSR2);/*用户自定义信号量*/
			}
		}
		return 0;
	}
	bool isValid()const { return m_thread != 0; }
private:
	/*__stdcall 不传递this指针*/
	static void* ThreadEntry(void* arg) {
		CThread* thiz = (CThread*)arg;

		/*struct sigaction {
			void (*sa_handler)(int);
			void (*sa_sigaction)(int, siginfo_t *, void *);
			sigset_t sa_mask;
			int sa_flags;
			void (*sa_restorer)(void);
		}*/
		struct sigaction act = { 0 };
		/*sigemptyset()函数的作用是初始化一个自定义信号集，将其所有信号都清空，也就是将信号集中的所有的标志位置为0，
		使得这个集合不包含任何信号，也就是不阻塞任何信号。*/
		sigemptyset(&act.sa_mask);/*sa_mask 用来设置在处理该信号时暂时将sa_mask 指定的信号集搁置*/
		act.sa_flags = SA_SIGINFO;/*存在回调函数*/
		act.sa_sigaction = &CThread::Sigaction;/*回调函数*/
		/*int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
		sigaction函数的功能是检查或修改与指定信号相关联的处理动作（可同时两种操作）
		signum参数指出要捕获的信号类型，act参数指定新的信号处理方式，oldact参数输出先前信号的处理方式（如果不为NULL的话）。
		*/
		sigaction(SIGUSR1, &act, NULL);/*暂停信号*/
		sigaction(SIGUSR2, &act, NULL);/*结束信号*/

		thiz->EnterThread();

		if (thiz->m_thread) thiz->m_thread = 0;
		pthread_t thread = pthread_self();/*不是冗余 可能被stop函数把m_thread清零了*/
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		pthread_detach(thread);
		pthread_exit(NULL);
	}
	/*信号回调函数*/
	static void Sigaction(int signo,siginfo_t* info,void* context) {
		if (signo == SIGUSR1) {
			/*留给pause*/
			pthread_t thread = pthread_self();/*拿到本线程的线程id*/
			auto it = m_mapThread.find(thread);
			if (it != m_mapThread.end()) {
				if (it->second) {
					while (it->second->m_bpaused){
						if (it->second->m_thread == 0) {
							pthread_exit(NULL);
						}
						usleep(1000);/*1ms*/
					}
				}
			}
		}
		else if(signo == SIGUSR2)
		{
			/*线程退出*/
			pthread_exit(NULL);
		}
	}

	/*__thiscall 传递this指针,就可以使用类中的成员和方法*/
	void EnterThread() {
		if (m_function != NULL) {
			int ret = (*m_function)();/*通过CFunction类封装的适配器填入参数并调用*/
			if (ret != 0) {
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}

	}
private:
	CFunctionBase* m_function;/*线程工作函数*/
	pthread_t m_thread;
	bool m_bpaused;/*true 表示暂停 false 表示运行中*/
	static std::map<pthread_t, CThread*>m_mapThread;
};