#pragma once
#include "Logger.h"
#include "Server.h"
#include <map>
#include "HttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"

#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret,errno, strerror(errno)); return err; }
#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret,errno, strerror(errno)); continue; }

/*定义数据库*/ 
DECLARE_TABLE_CLASS(login_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")//QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "18888888888", "")//手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")//姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")//昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "LOCALTIME()", "")
DECLARE_TABLE_CLASS_END()

/*
 * 1. 客户端的地址问题
 * 2. 连接回调的参数问题
 * 3. 接收回调的参数问题
*/

/*
 * 第二个线程池在网络服务器子进程：用来处理客户端发送来的数据
 * 第一个线程池在程序的主进程：用来将连接好的客户端信息发送给网络服务器子进程
 * 
*/

/*
 * CMyPlayerServer类：网络服务器子进程 工作进程
 * 
 * 1.BusinessProcess静态函数：网络服务器子进程接口函数
 *		创建数据库对象 + 设置数据库基本信息，连接数据库 + 设置连接、接收回调函数 Connected Received + 创建epoll和线程池
 *		+ 设置线程池中实际工作函数ThraedFunc + 进入死循环中
 *		死循环工作逻辑：
 *		调用RecvSocket接收主进程发送过来的客户端套接字的文件描述符 + 将接受到的客户端套接字初始化 + 将收到的客户端加入到epoll中
 *		+ 调用连接回调函数
 * 
 * 2.Connected 回调函数：没有什么特别的意义。
 * 3.ThraedFunc 线程池的实际工作函数：封装了epoll
 *		客户端套接字收到输入请求：接收客户端给服务器的数据 + 调用m_recvcallback（绑定Received回调函数）处理收到的数据
 * 
 * 4.Received 回调函数：主要业务在此处理
 *		http解析
 *		应答函数
 *		将处理完的数据发送给外面的客户端
*/

/*网络服务器处理子进程*/
class CMyPlayerServer :public CBusiness
{
public:
	CMyPlayerServer(unsigned count) :CBusiness() {
		m_count = count;
	}
	~CMyPlayerServer() {
		if (m_db) {
			CDataBaseClient* db = m_db;
			m_db = NULL;
			db->Close();
			delete db;
		}
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {
			if (it.second) {
				delete it.second;
			}
		}
		m_mapClients.clear();
	}
	/*网络服务器子进程接口函数 -> 服务器客户端处理模块主进程接口函数*/
	virtual int BusinessProcess(CProcess* proc) {
		using namespace std::placeholders;
		int ret = 0;
		m_db = new CMysqlClient();
		if (m_db == NULL) {
			TRACEE("no more memory!");
			return -1;
		}
		KeyValue args;
		args["host"] = "0.0.0.0";
		args["user"] = "root";
		args["password"] = "123456";
		args["port"] = 3306;
		args["db"] = "hello_db";
		ret = m_db->Connect(args);
		ERR_RETURN(ret, -2);
		login_user_mysql user;
		ret = m_db->Exec(user.Create());
		ERR_RETURN(ret, -3);
		/*设置连接回调函数和参数*/
		ret = setConnectedCallback(&CMyPlayerServer::Connected, this/*成员函数要传this*/, _1);/*std::placeholders::_1告诉函数模板现在这个参数我们还不知道，占位符*/
		ERR_RETURN(ret, -4);
		/*设置接收回调函数和参数*/
		ret = setRecvCallback(&CMyPlayerServer::Received, this/*成员函数要传this*/, _1, _2);
		ERR_RETURN(ret, -5);
		ret = m_epoll.Create(m_count);
		ERR_RETURN(ret, -6);
		ret = m_pool.Start(m_count);
		ERR_RETURN(ret, -7);
		for (unsigned i{}; i < m_count; i++) {
			ret = m_pool.AddTask(&CMyPlayerServer::ThraedFunc, this);
			ERR_RETURN(ret, -8);
		}
		int sock{};
		sockaddr_in addrin;
		while (m_epoll != -1) {
			ret = proc->RecvSocket(sock, &addrin);
			if (ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);
			if (pClient == NULL)continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));/*客户端初始化*/
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollDate((void*)pClient));
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient);/*调用连接回调函数*/
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}
private:
	int Connected(CSocketBase* pClient) {
		/*客户端连接处理  简单打印一下客户信息*/
		sockaddr_in* paddr = *pClient;
		TRACEI("client connect addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}
	int Received(CSocketBase* pClient, const Buffer& data) {
		/*主要业务在此处理*/
		/*http解析*/
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data);
		/*验证结果的反馈*/
		if (ret != 0) {/*验证失败*/
			TRACEE("http parser failed!%d", ret);	
		}
		response = MakeResponse(ret);
		ret = pClient->Send(response);
		if (ret != 0) {
			TRACEE("http response failed!%d [%s]", ret, (char*)response);
		}
		else
		{
			TRACEI("http response success!%d", ret);
		}
		return 0;
	}
	/*http解析*/
	int HttpParser(const Buffer&data) {
		CHttpParser parser;
		size_t size = parser.Parser(data);/*开始解析*/
		if (size == 0 || (parser.Errno() != 0)) {
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}
		if (parser.Method() == HTTP_GET) {
			/*get处理   进一步处理得到的url*/
			UrlParser url("https://127.0.0.1" + parser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret = %d url[%s]", ret, "https://127.0.0.1" + parser.Url());
				return -2;
			}
			Buffer uri = url.Uri();
			if (uri == "login") {
				/*处理登陆*/
				Buffer time = url["time"];
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				/*数据库查询*/
				login_user_mysql dbuser;
				Result result;
				Buffer sql = dbuser.Query("user_name=\"" + user + "\"");
				ret = m_db->Exec(sql, result, dbuser);
				if (ret != 0) {
					TRACEE("sql=%s ret=%d", (char*)sql, ret);
					return -3;
				}
				if (result.size() == 0) {
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1) {
					TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}
				auto user1 = result.front();
				Buffer pwd = *user1->Fields["user_password"]->Value.String;
				TRACEI("password = %s", (char*)pwd);
				/*登陆请求的验证*/
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = Crypto::MD5(md5str);/*服务器计算出来的md5*/
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign)/*服务器计算出的md5和客户端发送的md5进行对比*/
				{
					return 0;
				}
				return -6;
			}
		}
		else if (parser.Method() == HTTP_POST) {
			/*post处理*/
		}
		return -1;
	}
	/*应答函数*/
	Buffer MakeResponse(int ret) {
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "登录失败，可能是用户名或者密码错误！";
		}
		else
		{
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();/*转成json类型的字符串*/
		/*拼接http响应报文*/
		Buffer result = "HTTP/1.1 200 OK\r\n";
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b % G % T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server:Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		/*json长度*/
		Buffer Length = Buffer("Content-Length:") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		result += Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)result);
		return result;
	}
private:
	/*任务线程*/
	int ThraedFunc() {
		int ret{};
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t size = m_epoll.WaitEvent(events);
			if (size < 0)break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++)
				{
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient -> Recv(data);
							if (ret <= 0) {
								TRACEW("ret = %d errno = %d msg = [%s]", ret, errno, strerror(errno));
								m_epoll.Del(*pClient);
								continue;
							}
							if (m_recvcallback) {
								(*m_recvcallback)(pClient,data);
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
	std::map<int, CSocketBase*>m_mapClients;
	CThreadPool m_pool;
	unsigned m_count;/*epoll接口和线程池线程的个数*/
	CDataBaseClient* m_db;
};