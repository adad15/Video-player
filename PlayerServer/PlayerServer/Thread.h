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
 * �ӿڣ��̹߳�������
 *
*/

/*
 * ��̬��Աm_mapThread�� �߳�id���߳�CThread���ӳ��
 * 
 * CThread�ࣺ
 * 
 * 1.������װ�˵����߳�
 * 2.���캯����
 *		Ĭ�Ϲ��캯�����ÿձ���
 *		ģ�幹�캯����new CFunction�����̹߳������������������̺߳�����
 * 3.SetThreadFunc��Ա������
 *		new CFunction�����̹߳������������������̺߳�����
 * 4.Start ��Ա������
 *		��װpthread_create�������̺߳�������Ϊ��̬��Ա��������Ϊ��ķǾ�̬��Ա����ֻ��ͨ����Ķ���ȥ���ã�
���Ǵ����߳�ʱ�������ܻ����Ķ����ȥ������ĳ�Ա������? �����̺߳�������Ϊ��̬��
 *		��̬����û��CThread�����thisָ�룬���Ե��ĸ�������thisָ�룬��CThread����ָ�봫���̺߳���
 *		�̺߳����ı��ʾ��ǻص�����
 *		���߳�id���߳����ӳ����뾲̬ӳ���
 * 5.Pause ��Ա����
 *		��װpthread_kill�����������Զ���SIGUSR1�źţ����߳̽��뵽�ź���Ӧ����
 * 6.Stop ��Ա����
 *		��װpthread_detach�������������߳� + pthread_kill�������Զ����ź�SIGUSR2
 * 7.ThreadEntry�̺߳���
 *		ͨ�������thisָ�룬�����������̳߳�Ա����
 *		�����źż� + ע���źŻص����� + �ȴ�SIGUSR1��SIGUSR2�ź� + �Ӿ�̬ӳ�����ɾ��ӳ��
 * 8.�źŻص���������̬��
 *		��Ϊ�Ǿ�̬�ģ�ȫ��ֻ��һ���źŻص�������������Ҫ֮ǰ����ľ�̬ӳ����õ��̶߳��� + �����źŴ���
 * 9.EnterThread�������̺߳���
 *		�����̹߳���������CFunctionBase�ࣩ
*/

class CThread {
public:
	CThread() {
		m_function = NULL; 
		m_thread = 0;
		m_bpaused = false;
	}

	/*ģ�幹�캯����new CFunction�����̹߳������������������̺߳�����*/
	template<typename _FUNCTION_,typename..._ARGS_>
	CThread(_FUNCTION_ func, _ARGS_...args) :
		m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) 
	{
		m_thread = 0;
	}
	~CThread() {}
public:
	/*�߳��ǲ��ܱ����Ƶģ�����Ϊ�˰�ȫ���������Ҫɾ��*/
	CThread(const CThread&) = delete;
	CThread& operator=(const CThread&) = delete;
public:
	/*�����̹߳�������*/
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
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE/*���������Ҫ���߳���joinһ��*/);
		if (ret != 0) return -2;
		//pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);/*�����ھ���  Ĭ�Ͼ���*/
		//if (ret != 0) return -3;
		pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
		if (ret != 0) return -4;
		m_mapThread[m_thread] = this;/*�����߳�id���߳����ӳ��*/
		pthread_attr_destroy(&attr);
		if (ret != 0) return -5;
		return 0;
	}
	/*�߳���ͣ����*/
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
	/*�߳̽�������*/
	int Stop() {
		if (m_thread != 0) {
			pthread_t thread = m_thread;
			m_thread = 0;
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 1000000;/*100ms*/
			int ret = pthread_timedjoin_np(thread, NULL, &ts);
			if (ret == ETIMEDOUT) {/*�ȴ���ʱ �߳���Ч*/
				/*���ã���״̬��ʵ���̷߳��룬ע�ⲻ��ָ���̶߳���ռ�õ�ַ�ռ䡣
				�̷߳���״̬��ָ����״̬���߳������������̶߳Ͽ���ϵ���߳̽����󣨲��������ʬ�̣߳������˳�״̬���������̻߳�ȡ��
				��ֱ���Լ��Զ��ͷţ��Լ������PCB�Ĳ�����Դ�������硢���̷߳��������á�
				�������иû��ƣ������������ʬ���̡���ʬ���̵Ĳ�����Ҫ���ڽ������󣬴󲿷���Դ���ͷţ�һ�������Դ�Դ���ϵͳ�У�
				�����ں���Ϊ�ý����Դ��ڡ���ע�����û����һ���ƣ�*/
				pthread_detach(thread);
				/*���̷߳���һ��signal*/
				pthread_kill(thread, SIGUSR2);/*�û��Զ����ź���*/
			}
		}
		return 0;
	}
	bool isValid()const { return m_thread != 0; }
private:
	/*__stdcall ������thisָ��*/
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
		/*sigemptyset()�����������ǳ�ʼ��һ���Զ����źż������������źŶ���գ�Ҳ���ǽ��źż��е����еı�־λ��Ϊ0��
		ʹ��������ϲ������κ��źţ�Ҳ���ǲ������κ��źš�*/
		sigemptyset(&act.sa_mask);/*sa_mask ���������ڴ�����ź�ʱ��ʱ��sa_mask ָ�����źż�����*/
		act.sa_flags = SA_SIGINFO;/*���ڻص�����*/
		act.sa_sigaction = &CThread::Sigaction;/*�ص�����*/
		/*int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
		sigaction�����Ĺ����Ǽ����޸���ָ���ź�������Ĵ���������ͬʱ���ֲ�����
		signum����ָ��Ҫ������ź����ͣ�act����ָ���µ��źŴ���ʽ��oldact���������ǰ�źŵĴ���ʽ�������ΪNULL�Ļ�����
		*/
		sigaction(SIGUSR1, &act, NULL);/*��ͣ�ź�*/
		sigaction(SIGUSR2, &act, NULL);/*�����ź�*/

		thiz->EnterThread();

		if (thiz->m_thread) thiz->m_thread = 0;
		pthread_t thread = pthread_self();/*�������� ���ܱ�stop������m_thread������*/
		auto it = m_mapThread.find(thread);
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL;
		pthread_detach(thread);
		pthread_exit(NULL);
	}
	/*�źŻص�����*/
	static void Sigaction(int signo,siginfo_t* info,void* context) {
		if (signo == SIGUSR1) {
			/*����pause*/
			pthread_t thread = pthread_self();/*�õ����̵߳��߳�id*/
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
			/*�߳��˳�*/
			pthread_exit(NULL);
		}
	}

	/*__thiscall ����thisָ��,�Ϳ���ʹ�����еĳ�Ա�ͷ���*/
	void EnterThread() {
		if (m_function != NULL) {
			int ret = (*m_function)();/*ͨ��CFunction���װ���������������������*/
			if (ret != 0) {
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}

	}
private:
	CFunctionBase* m_function;/*�̹߳�������*/
	pthread_t m_thread;
	bool m_bpaused;/*true ��ʾ��ͣ false ��ʾ������*/
	static std::map<pthread_t, CThread*>m_mapThread;
};