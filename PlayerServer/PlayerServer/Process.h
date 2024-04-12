#pragma once
#include "Function.h"
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

/*
 * CProcess类：
 * 
 * 这个类的主要目的是：
 * 1.进程类，这个类主要是创建进程，调用进程函数
 * 2.成员方法实现将 套接字fd 发送给其他进程
 * 
 * SetEntryFunction成员函数：
 * 1.设置进程入口函数
 * 2.利用可变参数函数模板，来实现不同进程可以将不同参数传入到这个类里面
 * 
 * CreatSubProcess成员函数：
 * 1.创建子进程，并运行子进程函数
 * 2.利用socketpair实现进程间通信 =》 管道 + socketpair（设置本地套接字）+ sendmsg/recvmsg
 * 3.fork()创建子进程 =》 关闭写管道 =》 启动进程入口函数
 *			   主进程 =》 关闭读管道 =》 继续执行下面的函数
 * 
 * SendFD/RecvFD成员函数：
 * 1.封装sendmsg/recvmsg 函数，将套接字fd 发送给别的进程
 * 
 * SendSocket/RecvSocket成员函数：
 * 1.封装sendmsg/recvmsg 函数，将网络套接字地址信息发送给别的进程
 * 
 * SwitchDeamon：
 * 1.创建守护进程
*/

/*进程类*/
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
	/*设置进程入口函数  变成模版函数，不清楚使用什么类型的函数*/
	template<typename _FUNCTION_/*初始化函数类型*/, typename... _ARGS_/*_ARGS_为不定参数类型*/>
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args) {
		m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		return 0;
	}

	int CreatSubProcess() {
		if (m_func == NULL) return -1;
		/*进程间通信 socket*/
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);/*本地套接字*/
		if (ret == -1) return -2;
		pid_t pid = fork();
		if (pid == -1) return -3;
		if (pid == 0) {/*子进程*/
			close(pipes[1]);/*关闭掉写*/
			pipes[1] = 0;
			/*启动进程入口函数*/
			ret = (*m_func)();/*调用仿函数 将参数填入入口函数并调用  并且子进程退出函数*/
			exit(0);
		}
		/*父进程继续向下执行  记录子进程pid*/
		close(pipes[0]);
		pipes[0] = 0;
		m_pid = pid;
		return 0;
	}

	int SendFD(int fd) {
		struct msghdr msg;
		/*没有实际用途*/
		struct iovec iov[2];
		char buf[2][10] = { "edoyun","jueding" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		/*大坑！！！ 这个必须要有*/
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		/*实际要传输的数据(文件描述符)*/
		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));/*追加一个len*/
		cmsg->cmsg_level = SOL_SOCKET;/*套接字连接*/
		cmsg->cmsg_type = SCM_RIGHTS;/*拿到权限*/
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

	int RecvFD(int& fd) {/*返回值表状态，输出参数用来传递修改后参数*/
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
		/*没有实际用途*/
		struct iovec iov[2];
		char buf[2][10] = { "edoyun","jueding" };
		iov[0].iov_base = (void*)addrin;
		iov[0].iov_len = sizeof(sockaddr_in);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		/*大坑！！！ 这个必须要有*/
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		cmsghdr* cmsg = new cmsghdr;
		bzero(cmsg, sizeof(cmsghdr));
		if (cmsg == NULL) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));/*追加一个len*/
		cmsg->cmsg_level = SOL_SOCKET;/*套接字连接*/
		cmsg->cmsg_type = SCM_RIGHTS;/*拿到权限*/
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

	int RecvSocket(int& fd, sockaddr_in* addrin) {/*返回值表状态，输出参数用来传递修改后参数*/
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

	/*守护进程*/
	static int SwitchDeamon() {
		pid_t ret = fork();
		if (ret == -1) return -1;
		if (ret > 0) exit(0);/*主进程到此为止*/
		/*子进程如下*/
		ret = setsid();
		if (ret == -1) return -2;
		ret = fork();
		if (ret > 0) exit(0);/*子进程到此为止*/
		/*孙进程的内容如下 进入守护状态*/
		for (int i{}; i < 3; i++) {
			/*关闭标准输入输出*/
			close(i);
		}
		umask(0);/*清除文件屏蔽位*/
		signal(SIGCHLD, SIG_IGN);/*关闭SIGCHLD信号*/
		return 0;
	}

private:
	/*会报错，因为在构造的时候没有办法确认CFunction，只有调用SetEntryFunction才能确认*/
	//CFunction* m_func;/*存放初始化函数*/
	CFunctionBase* m_func;/*存放初始化函数   引入基类指针*/
	pid_t m_pid;
	int pipes[2];
};