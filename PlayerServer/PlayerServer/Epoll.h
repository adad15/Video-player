#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <errno.h>
#include <sys/signal.h>
#include <memory.h>
#include <cstdio>

/*
 *	
 *                    1.日志服务器 =》 使用epoll调用本地socket实现进程间通信
 * epoll 两大用处
 *					  2.网络服务器 =》 使用epoll调用网络socket更好实现多线程通信，避免队列加锁导致效率下降
 *
*/

/* 
 * EpollDate类：
 * 1.封装了epoll_data_t结构体，用来构造epoll_event结构体
 * 2.epoll_data_t结构体里面有
 *		u64、u32、fd、ptr
 * 3.利用operator 转换函数对EpollDate类实现转换
 * 
 * EPEvents：利用vector容器维护epoll中发生的事件
 * 
 * CEpoll类：
 * 1.封装了epoll，删除了拷贝构造函数和赋值重载函数
 * 2.利用operator 转换函数对CEpoll类实现转换返回m_epoll
 * 3.Create 成员函数：
 *      封装epoll_create，创建epoll
 * 4.WaitEvent 成员函数：
 *      封装epoll_wait等待事件函数，返回发生的事件数组
 * 5.Add、Modify、Del 成员函数：
 *		封装epoll_ctl处理事件函数，并且拼装epoll_event
 * 
*/		

#define EVENT_SIZE 128
using EPEvents = std::vector<epoll_event>;

class EpollDate {
public:
	EpollDate() { m_data.u64 = 0;}/*联合体共享一块内存，只需把最大的变量赋为0*/
	EpollDate(void* ptr) { m_data.ptr = ptr; }
	explicit EpollDate(int fd) { m_data.fd = fd; }/*使用explicit强制显示表示，禁止变量进行隐式转换，null就是0，反之误用*/
	explicit EpollDate(uint32_t u32) { m_data.u32 = u32; }
	explicit EpollDate(uint64_t u64) { m_data.u64 = u64; }
	EpollDate(const EpollDate& data) { m_data.u64 = data.m_data.u64; }
public:
	EpollDate& operator=(const EpollDate& data) {
		if (this != &data)
			m_data.u64 = data.m_data.u64;
		return *this;
	}
	EpollDate& operator=(void* data) {
		m_data.ptr = data;
		return *this;
	}
	EpollDate& operator=(int data) {
		m_data.fd = data;
		return *this;
	}
	EpollDate& operator=(uint32_t data) {
		m_data.u32 = data;
		return *this;
	}
	EpollDate& operator=(uint64_t data) {
		m_data.u64 = data;
		return *this;
	}
	operator epoll_data_t() { return m_data; }
	operator epoll_data_t()const { return m_data; }
	operator epoll_data_t*() { return &m_data; }
	/*？？？？为什么没有第一个const会报错？？？？*/
	operator const epoll_data_t* ()const { return &m_data; }
private:
	epoll_data_t m_data;
};

class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1;
	}
	~CEpoll() {
		Close();
	}
public:
	CEpoll(const CEpoll&) = delete;
	CEpoll& operator=(const CEpoll&) = delete;
public:
	operator int()const { return m_epoll; }
public:
	int Create(unsigned count) {
		if (m_epoll != -1) return -1;
		m_epoll = epoll_create(count);
		if (m_epoll == -1) return -1;
		return 0;
	}
	/*等待事件 小于0表示错误  等于0表示没有事情发生  大于0表示成功拿到事件*/
	ssize_t WaitEvent(EPEvents& events, int timeout = 10) {
		if (m_epoll == -1) return -1;
		EPEvents evs(EVENT_SIZE);
		/*evs 队列存放发生的事件*/
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout);
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) {
				return 0;
			}
			return -2;
		}
		if (ret > (int)events.size()) {
			events.resize(ret);
		}
		/*把得到的信息作为输出参数传出去*/
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret);/*它返回内置vecotr所指的数组内存的第一个元素的指针。*/
		return ret;
	}
	/*添加事件*/
	int Add(int fd, const EpollDate& data = EpollDate((void*)0), uint32_t events = EPOLLIN) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data/*会类型转换 operator epoll_data_t()*/ };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		printf("%s(%d):[%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret == -1) return -2;
		return 0;
	}
	/*修改事件*/
	int Modify(int fd, const EpollDate& data = EpollDate((void*)0), uint32_t events = EPOLLIN) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data/*会类型转换 operator epoll_data_t()*/ };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	/*删除事件*/
	int Del(int fd) {
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL);
		if (ret == -1) return -2;
		return 0;
	}
	void Close() {
		if (m_epoll != -1) {
			int fd = m_epoll;
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll;
};