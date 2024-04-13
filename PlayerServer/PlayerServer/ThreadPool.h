#pragma once

#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"

/*
 * 
 * �ӿڣ�AddTask ���������ĺ���ģ��
 * 
 * std::vector<CThread*> m_threads;
 * ͨ��vector����ά���̶߳���
 * 
 * �̼߳�ͨ�ţ��Զ����ź� �������̵߳�������ر�
 * 
 * �̳߳ؾ��൱�ڷ���ˣ������߳̾��൱�ڿͻ���
 * 
 * �ֲ�ṹ��
 * �߳̽ӿں������߳��ࣩ =���̹߳����������̳߳��ࣩ =��ʵ�ʹ�����������main������㣩
 * 
 */

/*
 * CThreadPool�̳߳��ࣺ
 * 1.���캯����
 *		�������̼�ͨ�ŵ������ļ�.sock�ļ���m_path��
 * 2.Start ��Ա������
 *		���������׽��� + Init ͨ��.sock�ļ������������ı����׽������� + ����epoll�������������׽��ּ��뵽epoll�������
 *		+ �� TaskDispatch ��������Ϊÿ���̵߳Ĺ������� + �����̣߳������źţ������̹߳������� TaskDispatch��
 * 3.TaskDispatch ��Ա������
 *											����˷������յ��������󣺷����� Link �õ������ӵĿͻ��� + �������ӵĿͻ��˼��뵽epoll
 *		��װ�� WaitEvent ����������ѭ��  
 *											�ͻ��˷������յ���������events[i].data.ptr�õ����¼���Ӧ�Ŀͻ��� + ����pClient��Recv�õ��̹߳�������
 *											+ ���д�����̹߳�������
 * 4.AddTask ����ģ�壺=�� �̳߳صõ������ʵ�ʵĴ���������
 *		ͨ�� thread_local ÿ���̵߳õ���ͬ�ͻ���socket + Init + Link + ���õ���ʵ�ʴ�����ָ�뷢�͸� TaskDispatch ��Ա����
 * 5.Close ��Ա������
 *		m_epoll�ر�
 *		�ر��̳߳����ÿһ��thread
 *		����̶߳���
 *		�ر������ļ�m_path
 * 
*/

class CThreadPool {
public:
	CThreadPool() {/*�������̼�ͨ�ŵ������ļ�*/
		m_server = NULL;
		/*Ϊ�˸���·��m_path��֤ÿ���̳߳ز�һ���������ʱ�Ӻ���*/
		timespec tp = { 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = NULL;
		/*asprintf ϵͳ�����ڴ�   �õ��ļ���*/
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000);
		if (buf != NULL) {
			m_path = buf;
			free(buf);
		}/*������Ļ�����start�ӿ������ж�m_path���������*/
		usleep(1);/*��ֹ����̳߳�һ�𴴽���m_path���ظ�*/
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;
public:
	int Start(unsigned count) {
		int ret = 0;
		if (m_server != NULL) return -1;/*�Ѿ���ʼ����*/
		if (m_path.size() == 0) return -2;/*���캯��ʧ��*/
		m_server = new CSocket();
		if (m_server == NULL)return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));
		if (ret != 0)return -4;
		ret = m_epoll.Create(count);
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollDate((void*)m_server));
		if (ret != 0) return - 6;
		m_threads.resize(count);
		for (unsigned i{}; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);/*ͨ��CFunction��ʵ�ֵ��Զ�this���뺯��TaskDispatch�У������иú���*/
			if (m_threads[i] == NULL)return -7;
			ret = m_threads[i]->Start();
			if (ret != 0) return -8;
		}
		return 0;
	}
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads) {
			if (thread)delete thread;
		}
		m_threads.clear();
		unlink(m_path);/*�ر������ļ�*/
	}
	template<typename _FUNCTION_, typename..._ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_...args) {/*�����ͬ���̵߳���*/
		static thread_local CSocket client;/*ÿ���̵߳�client����һ����ͬһ���߳�һ������ͬ�߳�client��һ��*/
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));
			if (ret != 0)return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (base == NULL) return -3;
		/*��ָ�뷢�͹�ȥ  TaskDispatch������  ���߳�ͨ��*/
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data);
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
	size_t Size() const { return m_threads.size(); }
private:
	/*ÿ���̵߳��̹߳�������*/
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret{};
			ssize_t esize = m_epoll.WaitEvent(events);
			if (esize > 0) {
				for (ssize_t i{}; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {/*�ͻ�����������*/
							ret = m_server->Link(&pClient);
							if(ret!=0)continue;
							ret = m_epoll.Add(*pClient, EpollDate((void*)pClient));
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else/*�ͻ��˵���������*/
						{
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data);
								if (ret <= 0) {
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)();/*��������ʵ�ʴ�����*/
									delete base;
								}
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;
	/*������ӣ��������������һ��Ҫ�и��ƹ��캯�������ܸ�����ô���������档�̲߳��ܸ��ƣ�����������ָ��*/
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;/*�����׽���ͨ��ʹ��*/
}; 