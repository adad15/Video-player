#pragma once
#include "Function.h"
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/*
 * CProcess�ࣺ
 * 
 * ��������ҪĿ���ǣ�
 * 1.�����࣬�������Ҫ�Ǵ������̣����ý��̺���
 * 2.��Ա����ʵ�ֽ� �׽���fd ���͸���������
 * 
 * SetEntryFunction��Ա������
 * 1.���ý�����ں���
 * 2.���ÿɱ��������ģ�壬��ʵ�ֲ�ͬ���̿��Խ���ͬ�������뵽���������
 * 
 * CreatSubProcess��Ա������
 * 1.�����ӽ��̣��������ӽ��̺���
 * 2.����socketpairʵ�ֽ��̼�ͨ�� =�� �ܵ� + socketpair�����ñ����׽��֣�+ sendmsg/recvmsg
 * 3.fork()�����ӽ��� =�� �ر�д�ܵ� =�� ����������ں���
 *			   ������ =�� �رն��ܵ� =�� ����ִ������ĺ���
 * 
 * SendFD/RecvFD��Ա������
 * 1.��װsendmsg/recvmsg ���������׽���fd ���͸���Ľ���
 * 
 * SendSocket/RecvSocket��Ա������
 * 1.��װsendmsg/recvmsg �������������׽��ֵ�ַ��Ϣ���͸���Ľ���
 * 
 * SwitchDeamon��
 * 1.�����ػ�����
*/

/*������*/
class CProcess {
public:
	CProcess() {
		m_func = NULL;
		memset(pipes, 0, sizeof(pipes));
	}
	~CProcess() {
		if (m_func != NULL) {
			delete m_func;
			m_func = NULL;
		}
	}
	/*���ý�����ں���  ���ģ�溯���������ʹ��ʲô���͵ĺ���*/
	template<typename _FUNCTION_/*��ʼ����������*/, typename... _ARGS_/*_ARGS_Ϊ������������*/>
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args) {
		m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		return 0;
	}

	int CreatSubProcess() {
		if (m_func == NULL) return -1;
		/*���̼�ͨ�� socket*/
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);/*�����׽���*/
		if (ret == -1) return -2;
		pid_t pid = fork();
		if (pid == -1) return -3;
		if (pid == 0) {/*�ӽ���*/
			close(pipes[1]);/*�رյ�д*/
			pipes[1] = 0;
			/*����������ں���*/
			ret = (*m_func)();/*���÷º��� ������������ں���������  �����ӽ����˳�����*/
			exit(0);
		}
		/*�����̼�������ִ��  ��¼�ӽ���pid*/
		close(pipes[0]);
		pipes[0] = 0;
		m_pid = pid;
		return 0;
	}

	int SendFD(int fd) {
		struct msghdr msg;
		/*û��ʵ����;*/
		struct iovec iov[2];
		char buf[2][10] = { "edoyun","jueding" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		/*��ӣ����� �������Ҫ��*/
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		/*ʵ��Ҫ���������(�ļ�������)*/
		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));/*׷��һ��len*/
		cmsg->cmsg_level = SOL_SOCKET;/*�׽�������*/
		cmsg->cmsg_type = SCM_RIGHTS;/*�õ�Ȩ��*/
		*(int*)CMSG_DATA(cmsg) = fd;
		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		ssize_t ret = sendmsg(pipes[1], &msg, 0);
		printf("%s(%d):<%s> ret=%d errno:%d msg:%s\n",
			__FILE__, __LINE__, __FUNCTION__, ret, errno, strerror(errno));
		if (ret == -1) {
			delete cmsg;
			return -2;
		}
		delete cmsg;
		return 0;
	}

	int RecvFD(int& fd) {/*����ֵ��״̬������������������޸ĺ����*/
		msghdr msg;
		iovec iov[2];
		char buf[][10] = { "","" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		ssize_t ret = recvmsg(pipes[0], &msg, 0);
//  		printf("%s(%d):<%s> pid=%d errno:%d msg:%s\n",
//  			__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno));
		if (ret == -1) {
			delete cmsg;
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg);
		/*printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);*/
		delete cmsg;
		return 0;
	}

	int SendSocket(int fd, const sockaddr_in* addrin) {
		struct msghdr msg;
		/*û��ʵ����;*/
		struct iovec iov[2];
		char buf[2][10] = { "edoyun","jueding" };
		iov[0].iov_base = (void*)addrin;
		iov[0].iov_len = sizeof(sockaddr_in);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		/*��ӣ����� �������Ҫ��*/
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));/*׷��һ��len*/
		cmsg->cmsg_level = SOL_SOCKET;/*�׽�������*/
		cmsg->cmsg_type = SCM_RIGHTS;/*�õ�Ȩ��*/
		*(int*)CMSG_DATA(cmsg) = fd;
		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		ssize_t ret = sendmsg(pipes[1], &msg, 0);
		printf("%s(%d):<%s> ret=%d errno:%d msg:%s\n",
			__FILE__, __LINE__, __FUNCTION__, ret, errno, strerror(errno));
		if (ret == -1) {
			delete cmsg;
			return -2;
		}
		delete cmsg;
		return 0;
	}

	int RecvSocket(int& fd, sockaddr_in* addrin) {/*����ֵ��״̬������������������޸ĺ����*/
		msghdr msg;
		iovec iov[2];
		char buf[][10] = { "","" };
		iov[0].iov_base = addrin;
		iov[0].iov_len = sizeof(sockaddr_in);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		ssize_t ret = recvmsg(pipes[0], &msg, 0);
		printf("%s(%d):<%s> pid=%d errno:%d msg:%s\n",
			__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno));
		if (ret == -1) {
			delete cmsg;
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg);
		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		delete cmsg;
		return 0;
	}

	/*�ػ�����*/
	static int SwitchDeamon() {
		pid_t ret = fork();
		if (ret == -1) return -1;
		if (ret > 0) exit(0);/*�����̵���Ϊֹ*/
		/*�ӽ�������*/
		ret = setsid();
		if (ret == -1) return -2;
		ret = fork();
		if (ret > 0) exit(0);/*�ӽ��̵���Ϊֹ*/
		/*����̵��������� �����ػ�״̬*/
		for (int i{}; i < 3; i++) {
			/*�رձ�׼�������*/
			close(i);
		}
		umask(0);/*����ļ�����λ*/
		signal(SIGCHLD, SIG_IGN);/*�ر�SIGCHLD�ź�*/
		return 0;
	}

private:
	/*�ᱨ����Ϊ�ڹ����ʱ��û�а취ȷ��CFunction��ֻ�е���SetEntryFunction����ȷ��*/
	//CFunction* m_func;/*��ų�ʼ������*/
	CFunctionBase* m_func;/*��ų�ʼ������   �������ָ��*/
	pid_t m_pid;
	int pipes[2];
};