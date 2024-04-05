#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>

class Buffer :public std::string /*默认const char*/ {
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	operator char* () { return (char*)c_str(); }
	operator char* ()const { return (char*)c_str(); }
	operator const char* ()const { return c_str(); }
};

enum SockAttr {
	SOCK_ISSERVER = 1,/*是否为服务器  1表示是  0表示客户端*/
	SOCK_ISNONBLOCK = 2,/*是否阻塞  1表示非阻塞  0表示阻塞*/
	SOCK_ISUDP = 4,/*是否为udp  1表示udp  0表示tcp*/
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
		addr_in.sin_port = port;
		addr_in.sin_addr.s_addr = inet_addr(ip);/*调用operator const char* ()const*/
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
			if (m_param.attr & SOCK_ISSERVER)/*所以要加上这个逻辑*/
				unlink(m_param.ip);/*这里不对客户端析构的时候，会把进程通信的连接释放，应该是服务端析构的时候才释放连接。*/
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return 0;
	}
	virtual operator int() { return m_socket; }
	virtual operator int()const { return m_socket; }
protected:/*设置为保护这样外部不能使用，但是子类可以使用*/
	/*套接字描述符 默认为-1*/
	int m_socket;
	/*状态 0初始化未完成 1初始化完成 2连接完成 3已经关闭*/
	int m_status;
	/*初始化参数*/
	CSockParam m_param;
};

/*本地套接字*/
class CLocalSocket :public CSocketBase
{
public:
	CLocalSocket():CSocketBase() {}
	CLocalSocket(int sock) :CSocketBase() {
		m_socket = sock;
	}
	virtual ~CLocalSocket() {
		Close();
	};
public:
	/*初始化 服务端 套接字建立 bind listen 客户端 套接字建立*/
	virtual int Init(const CSockParam& param) {
		if (m_status != 0) return -1;
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;
		if (m_socket == -1)
			m_socket = socket(PF_LOCAL, type, 0);
		else
			m_status = 2;/*accept 来的套接字，已经处于连接状态*/
		if (m_socket == -1) return -2;
		int ret = 0;
		/*如果是服务器执行下面的代码，客户端不执行*/
		if (m_param.attr & SOCK_ISSERVER) {
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
			CSockParam param;
			socklen_t len = sizeof(sockaddr_un);
			int fd = accept(m_socket, param.addrun(), &len);
			if (fd == -1)return -3;
			*pClient = new CLocalSocket(fd);
			if (*pClient == NULL) return -4;
			ret = (*pClient)->Init(param);/*本地套接字  这里也需要客户端初始化*/
			if (ret != 0) {
				delete(*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else {
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