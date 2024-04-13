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

/*�������ݿ�*/ 
DECLARE_TABLE_CLASS(login_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")//QQ��
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "18888888888", "")//�ֻ�
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")//����
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")//�ǳ�
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
 * 1. �ͻ��˵ĵ�ַ����
 * 2. ���ӻص��Ĳ�������
 * 3. ���ջص��Ĳ�������
*/

/*
 * �ڶ����̳߳�������������ӽ��̣���������ͻ��˷�����������
 * ��һ���̳߳��ڳ���������̣����������ӺõĿͻ�����Ϣ���͸�����������ӽ���
 * 
*/

/*
 * CMyPlayerServer�ࣺ����������ӽ��� ��������
 * 
 * 1.BusinessProcess��̬����������������ӽ��̽ӿں���
 *		�������ݿ���� + �������ݿ������Ϣ���������ݿ� + �������ӡ����ջص����� Connected Received + ����epoll���̳߳�
 *		+ �����̳߳���ʵ�ʹ�������ThraedFunc + ������ѭ����
 *		��ѭ�������߼���
 *		����RecvSocket���������̷��͹����Ŀͻ����׽��ֵ��ļ������� + �����ܵ��Ŀͻ����׽��ֳ�ʼ�� + ���յ��Ŀͻ��˼��뵽epoll��
 *		+ �������ӻص�����
 * 
 * 2.Connected �ص�������û��ʲô�ر�����塣
 * 3.ThraedFunc �̳߳ص�ʵ�ʹ�����������װ��epoll
 *		�ͻ����׽����յ��������󣺽��տͻ��˸������������� + ����m_recvcallback����Received�ص������������յ�������
 * 
 * 4.Received �ص���������Ҫҵ���ڴ˴���
 *		http����
 *		Ӧ����
 *		������������ݷ��͸�����Ŀͻ���
*/

/*��������������ӽ���*/
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
	/*����������ӽ��̽ӿں��� -> �������ͻ��˴���ģ�������̽ӿں���*/
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
		/*�������ӻص������Ͳ���*/
		ret = setConnectedCallback(&CMyPlayerServer::Connected, this/*��Ա����Ҫ��this*/, _1);/*std::placeholders::_1���ߺ���ģ����������������ǻ���֪����ռλ��*/
		ERR_RETURN(ret, -4);
		/*���ý��ջص������Ͳ���*/
		ret = setRecvCallback(&CMyPlayerServer::Received, this/*��Ա����Ҫ��this*/, _1, _2);
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
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));/*�ͻ��˳�ʼ��*/
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollDate((void*)pClient));
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient);/*�������ӻص�����*/
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}
private:
	int Connected(CSocketBase* pClient) {
		/*�ͻ������Ӵ���  �򵥴�ӡһ�¿ͻ���Ϣ*/
		sockaddr_in* paddr = *pClient;
		TRACEI("client connect addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}
	int Received(CSocketBase* pClient, const Buffer& data) {
		/*��Ҫҵ���ڴ˴���*/
		/*http����*/
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data);
		/*��֤����ķ���*/
		if (ret != 0) {/*��֤ʧ��*/
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
	/*http����*/
	int HttpParser(const Buffer&data) {
		CHttpParser parser;
		size_t size = parser.Parser(data);/*��ʼ����*/
		if (size == 0 || (parser.Errno() != 0)) {
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}
		if (parser.Method() == HTTP_GET) {
			/*get����   ��һ������õ���url*/
			UrlParser url("https://127.0.0.1" + parser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret = %d url[%s]", ret, "https://127.0.0.1" + parser.Url());
				return -2;
			}
			Buffer uri = url.Uri();
			if (uri == "login") {
				/*�����½*/
				Buffer time = url["time"];
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				/*���ݿ��ѯ*/
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
				/*��½�������֤*/
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = Crypto::MD5(md5str);/*���������������md5*/
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign)/*�������������md5�Ϳͻ��˷��͵�md5���жԱ�*/
				{
					return 0;
				}
				return -6;
			}
		}
		else if (parser.Method() == HTTP_POST) {
			/*post����*/
		}
		return -1;
	}
	/*Ӧ����*/
	Buffer MakeResponse(int ret) {
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "��¼ʧ�ܣ��������û��������������";
		}
		else
		{
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();/*ת��json���͵��ַ���*/
		/*ƴ��http��Ӧ����*/
		Buffer result = "HTTP/1.1 200 OK\r\n";
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b % G % T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server:Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		/*json����*/
		Buffer Length = Buffer("Content-Length:") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		result += Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)result);
		return result;
	}
private:
	/*�����߳�*/
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
	unsigned m_count;/*epoll�ӿں��̳߳��̵߳ĸ���*/
	CDataBaseClient* m_db;
};