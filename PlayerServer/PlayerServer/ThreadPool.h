#pragma once

#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"

class CThreadPool {
public:
	CThreadPool() {
		m_server = NULL;
		/*为了更改路径m_path保证每个线程池不一样引入这个时钟函数*/
		timespec tp = { 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf = NULL;
		/*asprintf 系统分配内存   得到文件名*/
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000);
		if (buf != NULL) {
			m_path = buf;
			free(buf);
		}/*有问题的话，在start接口里面判断m_path来解决问题*/
		usleep(1);/*防止多个线程池一起创建，m_path会重复*/
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;
public:
	int Start(unsigned count) {
		int ret = 0;
		if (m_server != NULL) return -1;/*已经初始化了*/
		if (m_path.size() == 0) return -2;/*构造函数失败*/
		m_server = new CLocalSocket();
		if (m_server == NULL)return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));
		if (ret != 0)return -4;
		ret = m_epoll.Create(count);/*还是只有一个epoll，可以理解为一个epoll开了4个接口*/
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollDate((void*)m_server));
		if (ret != 0) return - 6;
		m_threads.resize(count);
		for (unsigned i{}; i < count; i++) {
			/*通过CFunction类实现的自动this填入函数TaskDispatch中,即填入到m_binder中，等待调用*/
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);
			if (m_threads[i] == NULL)return -7;
			ret = m_threads[i]->Start();/*在这里调用TaskDispatch函数*/
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
		unlink(m_path);/*关闭连接文件*/
	}
	template<typename _FUNCTION_, typename..._ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_...args) {/*多个不同的线程调用*/
		static thread_local CLocalSocket client;/*每个线程的client都不一样，同一个线程一样，不同线程client不一样*/
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));
			if (ret != 0)return -1;
			ret = client.Link();
			if (ret != 0) return -2;
		}
		CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if (base == NULL) return -3;
		/*把指针发送过去  TaskDispatch来接受  跨进程通信*/
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data);
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
private:
	int TaskDispatch() {
		while (m_epoll != -1) {
			EPEvents events;
			int ret{};
			size_t esize = m_epoll.WaitEvent(events);
			if (esize > 0) {
				for (ssize_t i{}; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {/*客户端请求连接*/
							ret = m_server->Link(&pClient);
							if(ret!=0)continue;
							ret = m_epoll.Add(*pClient, EpollDate((void*)pClient));
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else/*客户端的数据来了*/
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
									(*base)();/*通过m_bind调用任务函数*/
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
	/*！！大坑！！容器里面对象一定要有复制构造函数，不能复制怎么放容器里面。线程不能复制，所以这里是指针*/
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;/*本地套接字通信使用*/
}; 