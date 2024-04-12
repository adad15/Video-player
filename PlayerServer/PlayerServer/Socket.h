#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Public.h"
#include <fcntl.h>

/*
 * SockAttr枚举
 * 
 * CSockParam类：
 * 1.封装了网络套接字和本地套接字的地址、端口、ip、参数
 * 2.构造函数 =》 为网络套接字和本地套接字初始化，赋值
 * 3.转换函数 =》 sockaddr_in转换成sockaddr
 * 
 * CSocketBase抽象类：
 * 1.本类的作用是实现多态
 * 2.Close()成员函数：关闭服务器的本地套接字（删除进程间的链接文件）和网络套接字
 * 3.其余成员函数都是纯虚函数
 * 
 * CSocket类：
 * 1.封装了网络套接字和本地套接字
 * 2.Init 成员函数：
 *		传入的参数是赋值好的套接字
 *		封装了客户端和服务器网络套接字和本地套接字的
 *		服务器： socket + bind + listen
 *		客户端： socket
 * 3.Link 成员函数：
 *		服务器：accept + 利用accept得到的客户端信息，调用Init创建客户端套接字
 *		客户端：connect
 * 4.Send/Recv 成员函数：
 *		封装了read和write函数
 * 
*/

enum SockAttr {
	SOCK_ISSERVER = 1,/*是否为服务器  1表示是  0表示客户端*/
	SOCK_ISNONBLOCK = 2,/*是否阻塞  1表示非阻塞  0表示阻塞*/
	SOCK_ISUDP = 4,/*是否为udp  1表示udp  0表示tcp*/
	SOCK_ISIP = 8,/*是否为ip协议  1表示ip协议  0表示本地套接字*/
};
/*套接字参数类*/
class CSockParam {
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0;/*默认是客户端、阻塞、tcp*/
	}
	/*网络套接字初始化*/
	CSockParam(const Buffer& ip, short port, int arrt) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port);/*host 主机 net网络  shor  主机字节序转为网络字节序*/
		addr_in.sin_addr.s_addr = inet_addr(ip);/*调用operator const char* ()const*/
	}
	CSockParam(const sockaddr_in* addrin, int arrt) {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
	}
	/*本地套接字初始化  参数地址*/
	CSockParam(const Buffer& path, int attr) {
		ip = path;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
	~CSockParam() {}
	CSockParam(const CSockParam& param) {
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
public:
	CSockParam& operator=(const CSockParam& param) {
		if (this != &param) {
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	/*转换函数 sockaddr_in转换成sockaddr*/
	sockaddr* addrin() { return (sockaddr*)&addr_in; }
	sockaddr* addrun() { return (sockaddr*)&addr_un; }
public:
	/*地址*/
	sockaddr_in addr_in;/*网络*/
	sockaddr_un addr_un;/*本地*/
	/*ip*/
	Buffer ip;
	/*端口*/
	short port;
	/*参考SockAttr*/
	int attr;
};

/*接口类  设置抽象函数之后该类没有办法实例化*/
class CSocketBase {
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0;
	}
	virtual ~CSocketBase() {
		Close();
	};
public:
	/*初始化 服务端 套接字建立 bind listen 客户端 套接字建立*/
	virtual int Init(const CSockParam& param) = 0;/*纯虚函数*/
	/*连接 服务器 accept 客户端 connect 对于udp这里可以忽略*/
	virtual int Link(CSocketBase** pClient = NULL) = 0;/*二级指针是因为我们想把一级指针传出来（多态性->使用基类指针），客户端不填参数  服务端填参数*/
	/*发送数据*/
	virtual int Send(const Buffer& data) = 0;
	/*接收数据*/
	virtual int Recv(Buffer& data) = 0;
	/*关闭连接*/
	virtual int Close() {
		m_status = 3;
		if (m_socket != -1) {
			if ((m_param.attr & SOCK_ISSERVER) &&/*服务器*/
				((m_param.attr & SOCK_ISIP) == 0)/*非IP*/)/*所以要加上这个逻辑*/
				unlink(m_param.ip);/*这里不对客户端析构的时候，会把进程通信的连接释放，应该是服务端析构的时候才释放连接。*/
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return 0;
	}
	virtual operator int() { return m_socket; }
	virtual operator int()const { return m_socket; }
	virtual operator const sockaddr_in*() const{ return &m_param.addr_in; }
	virtual operator sockaddr_in* () { return &m_param.addr_in; }
protected:/*设置为保护这样外部不能使用，但是子类可以使用*/
	/*套接字描述符 默认为-1*/
	int m_socket;
	/*状态 0初始化未完成 1初始化完成 2连接完成 3已经关闭*/
	int m_status;
	/*初始化参数*/
	CSockParam m_param;
};

/*本地套接字   在本地套接字的基础上进行修改增加网络套接字的功能*/ 
class CSocket :public CSocketBase
{
public:
	CSocket():CSocketBase() {}
	CSocket(int sock) :CSocketBase() {
		m_socket = sock;
	}
	virtual ~CSocket() {
		Close();
	};
public:
	/*初始化 服务端 套接字建立 bind listen 客户端 套接字建立*/
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1;
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;
		if (m_socket == -1)
			if (m_param.attr & SOCK_ISIP)
				m_socket = socket(PF_INET, type, 0);/*新加网络分支*/
			else
				m_socket = socket(PF_LOCAL, type, 0);
		else
			m_status = 2;/*accept 来的套接字，已经处于连接状态*/
		if (m_socket == -1) return -2;
		int ret = 0;
		/*如果是服务器执行下面的代码，客户端不执行*/
		if (m_param.attr & SOCK_ISSERVER) {
			if (m_param.attr & SOCK_ISIP)
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if (ret == -1) return -3;
			ret = listen(m_socket, 32);
			if (ret == -1) return -4;
		}
		if (m_param.attr & SOCK_ISNONBLOCK) {
			int option = fcntl(m_socket, F_GETFL);
			if (option == -1) return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL, option);/*设置非阻塞*/
			if (ret == -1) return -6;
		}
		if (m_status == 0) {
			m_status = 1;
		}
		return 0;
	}
	/*连接 服务器 accept 客户端 connect 对于udp这里可以忽略*/
	virtual int Link(CSocketBase** pClient = NULL) {
		if (m_status <= 0 || (m_socket == -1)) return -1;
		int ret{};
		if (m_param.attr & SOCK_ISSERVER) {
			if (pClient == NULL) return -2;
			CSockParam param;/*存放客户端套接字信息*/
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP) {
				param.attr |= SOCK_ISIP;/*将客户端表示设置为ip协议套接字*/
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len);
			}
			else {/*param.attr默认为本地套接字*/
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len);
			}
			if (fd == -1)return -3;
			/*创建客户端套接字*/
			*pClient = new CSocket(fd);
			if (*pClient == NULL) return -4;
			/*利用accept得到的客户端套接字信息创建客户端套接字*/
			ret = (*pClient)->Init(param);/*本地套接字  这里也需要客户端初始化*/
			if (ret != 0) {
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else {
			if (m_param.attr & SOCK_ISIP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			printf("%s(%d):<%s> ret=%d errno:%d msg:%s\n",
				__FILE__, __LINE__, __FUNCTION__, ret, errno, strerror(errno));
			if (ret != 0) return -6;
		}
		m_status = 2;
		return 0;
	}
	/*发送数据*/
	virtual int Send(const Buffer& data) {
		if (m_status < 2 || (m_socket == -1)) return -1;
		ssize_t index = 0;
		while (index < (ssize_t)data.size())
		{
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index);
			if (len == 0) return -2;
			if (len < 0)return -3;
			index += len;
		}
		return 0;
	}
	/*接收数据   大于零，表示接收成功  小于  表示失败  等于0  表示没有收到数据，但没有错误*/
	virtual int Recv(Buffer& data) {
		if (m_status < 2 || (m_socket == -1)) return -1;
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) {
			data.resize(len);
			return (int)len;/*收到数据*/
		}
		if (len < 0) {
			if (errno == EINTR || (errno == EAGAIN)) {
				data.clear();
				return 0;/*没有数据收到*/
			}
			return -2;/*发送错误*/
		}
		return -3;/*套接字被关闭*/
	}
	/*关闭连接*/
	virtual int Close() {
		return CSocketBase::Close();
	}
};