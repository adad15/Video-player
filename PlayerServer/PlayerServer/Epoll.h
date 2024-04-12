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
 *                    1.��־������ =�� ʹ��epoll���ñ���socketʵ�ֽ��̼�ͨ��
 * epoll �����ô�
 *					  2.��������� =�� ʹ��epoll��������socket����ʵ�ֶ��߳�ͨ�ţ�������м�������Ч���½�
 *
*/

/* 
 * EpollDate�ࣺ
 * 1.��װ��epoll_data_t�ṹ�壬��������epoll_event�ṹ��
 * 2.epoll_data_t�ṹ��������
 *		u64��u32��fd��ptr
 * 3.����operator ת��������EpollDate��ʵ��ת��
 * 
 * EPEvents������vector����ά��epoll�з������¼�
 * 
 * CEpoll�ࣺ
 * 1.��װ��epoll��ɾ���˿������캯���͸�ֵ���غ���
 * 2.����operator ת��������CEpoll��ʵ��ת������m_epoll
 * 3.Create ��Ա������
 *      ��װepoll_create������epoll
 * 4.WaitEvent ��Ա������
 *      ��װepoll_wait�ȴ��¼����������ط������¼�����
 * 5.Add��Modify��Del ��Ա������
 *		��װepoll_ctl�����¼�����������ƴװepoll_event
 * 
*/		

#define EVENT_SIZE 128
using EPEvents = std::vector<epoll_event>;

class EpollDate {
public:
	EpollDate() { m_data.u64 = 0;}/*�����干��һ���ڴ棬ֻ������ı�����Ϊ0*/
	EpollDate(void* ptr) { m_data.ptr = ptr; }
	explicit EpollDate(int fd) { m_data.fd = fd; }/*ʹ��explicitǿ����ʾ��ʾ����ֹ����������ʽת����null����0����֮����*/
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
	/*��������Ϊʲôû�е�һ��const�ᱨ��������*/
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
	/*�ȴ��¼� С��0��ʾ����  ����0��ʾû�����鷢��  ����0��ʾ�ɹ��õ��¼�*/
	ssize_t WaitEvent(EPEvents& events, int timeout = 10) {
		if (m_epoll == -1) return -1;
		EPEvents evs(EVENT_SIZE);
		/*evs ���д�ŷ������¼�*/
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
		/*�ѵõ�����Ϣ��Ϊ�����������ȥ*/
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret);/*����������vecotr��ָ�������ڴ�ĵ�һ��Ԫ�ص�ָ�롣*/
		return ret;
	}
	/*����¼�*/
	int Add(int fd, const EpollDate& data = EpollDate((void*)0), uint32_t events = EPOLLIN) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data/*������ת�� operator epoll_data_t()*/ };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
		printf("%s(%d):[%s] ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret == -1) return -2;
		return 0;
	}
	/*�޸��¼�*/
	int Modify(int fd, const EpollDate& data = EpollDate((void*)0), uint32_t events = EPOLLIN) {
		if (m_epoll == -1) return -1;
		epoll_event ev = { events,data/*������ת�� operator epoll_data_t()*/ };
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
		if (ret == -1) return -2;
		return 0;
	}
	/*ɾ���¼�*/
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