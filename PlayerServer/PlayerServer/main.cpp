#include <cstdio>
#include <stdlib.h>
#include <errno.h>
#include "MyPlayerServer.h"
#include "HttpParser.h"
#include "Sqlite3Client.h"
#include "MysqlClient.h"

/*入口函数*/
int CreatLogServer(CProcess* proc) {
    //printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    CLoggerServer server;
    int ret = server.Start();
    if (ret != 0) {
        printf("%s(%d):<%s> pid=%d errno:%d msg:%s\n",
            __FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno));
    }
    int fd = 0;
    while(true){
        printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
        /*等待主进程的信号*/
        ret = proc->RecvFD(fd);
        printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
        if (fd <= 0)break;
    }
    ret = server.Close();
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    return 0;
}

int CreatClientServer(CProcess* proc) {
    //printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    int fd = -1;
    int ret = proc->RecvFD(fd);
    //printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    //printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
    sleep(1);
    char buf[10] = "";
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, sizeof(buf));
	//printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf);
    close(fd);

    return 0;
}

int LogTest()
{
	char buffer[] = "hello edoyun! 冯老师";
	usleep(1000 * 100);
	TRACEI("here is log %d %c %f %g %s 哈哈 嘻嘻 易道云", 10, 'A', 1.0f, 2.0, buffer);
	DUMPD((void*)buffer, (size_t)sizeof(buffer));
    LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.23456789 << " " << buffer << " 易道云编程";
    return 0;
}

int oldtest() {
	//CProcess::SwitchDeamon();
	CProcess proclog, procclients;
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proclog.SetEntryFunction(CreatLogServer, &proclog);
	int ret = proclog.CreatSubProcess();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__,
			__LINE__, __FUNCTION__, getpid());
		return -1;
	}
	LogTest();
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CThread thread(LogTest);
	thread.Start();
	procclients.SetEntryFunction(CreatClientServer, &procclients);
	ret = procclients.CreatSubProcess();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -2;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());

	usleep(100 * 000);
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	if (fd == -1)return -3;
	ret = procclients.SendFD(fd);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	write(fd, "edoyun", 6);
	close(fd);
	proclog.SendFD(-1);

	(void)getchar();
	return 0;
}

int oldtest1() {
	int ret{};
	CProcess proclog;
	ret = proclog.SetEntryFunction(CreatLogServer, &proclog);
	ERR_RETURN(ret, -1);
	ret = proclog.CreatSubProcess();
	ERR_RETURN(ret, -2);
	CMyPlayerServer business(2);
	CServer server;
	ret = server.Init(&business);
	ERR_RETURN(ret, -3);
	ret = server.Run();
	ERR_RETURN(ret, -4);


	return 0;
}

int http_test() {
	Buffer str = "GET /sample.jsp HTTP/1.1\r\n"
		"Accept: image/gif.image/jpeg\r\n"
		"Accept-Language: zh-cn\r\n"
		"Connection: Keep-Alive\r\n"
		"Host: localhost\r\n"
		"User-Agent: Mozila/4.0(compatible;MSIE5.01;Window NT5.0)\r\n"
		"Accept-Encoding: gzip,deflate\r\n"
		"\r\n";
	CHttpParser parser;
	size_t size = parser.Parser(str);
	if (size == 0) {
		printf("failed1\n");
	}
	if (parser.Errno() != 0) {
		printf("errno %d\n", parser.Errno());
		return -1;
	}
// 	if (size != 365) {
// 		printf("size errno:%lld\n", size);
// 		return -2;
// 	}
	printf("Method %d url %s\n", parser.Method(), (char*)parser.Url());

	UrlParser url1("https://cn.bing.com/search?q=http%e8%af%b7%e6%b1%82%e4%bd%93&qs=RI&pq=http%e4%bd%93&sc=8-5&cvid=6CDB814315094235A46D35B1335D2FA2&FORM=QBRE&sp=1&lq=0");
	int ret = url1.Parser();
	if (ret != 0) {
		printf("UrlParser failed:%d\n", ret);
	}
	printf("q = %s\n", (char*)url1["q"]);
	printf("qs = %s\n", (char*)url1["qs"]);
	printf("pq = %s\n", (char*)url1["pq"]);
	printf("sc = %s\n", (char*)url1["sc"]);
	printf("cvid = %s\n", (char*)url1["cvid"]);
	printf("FORM = %s\n", (char*)url1["FORM"]);
	printf("sp = %s\n", (char*)url1["sp"]);
	printf("lq = %s\n", (char*)url1["lq"]);
	return 0;
}

// class user_test :public _sqlite3_table_ {
// public:
// 	virtual PTable Copy()const {
// 		return PTable(new user_test(*this));
// 	}
// 	user_test() :_sqlite3_table_() {
// 		Name = "user_test";
// 		/*添加第一个列*/
// 		{
// 			/*sqlite3_field_(int ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)*/
// 			PField field(new _sqlite3_field_(TYPE_INT, "user_id", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INT", "", "", ""));
// 			FieldDefine.push_back(field);
// 			Fields["user_id"] = field;
// 		}
// 		/*添加第二个列*/
// 		{
// 			PField field(new _sqlite3_field_(TYPE_VARCHAR, "user_qq", NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "VARCHAR", "(15)", "", ""));
// 			FieldDefine.push_back(field);
// 			Fields["user_qq"] = field;
// 		}
// 	}
// };

/*使用宏来简化上述表的创建过程*/
DECLARE_TABLE_CLASS(user_test, _sqlite3_table_)
DECLARE_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
DECLARE_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL|DEFAULT, "VARCHAR", "(12)", "18888888888", "")
DECLARE_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_END()

int sqlite_test() {
	user_test test, value;
	printf("create:%s\n", (char*)test.Create());
	printf("Delete:%s\n", (char*)test.Delete(test));
	value.Fields["user_qq"]->LoadFromStr("3012265452");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	printf("Insert:%s\n", (char*)test.Insert(value));
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	printf("Modify:%s\n", (char*)test.Modify(value));
	printf("Query:%s\n", (char*)test.Query());
	printf("Drop:%s\n", (char*)test.Drop());
	getchar();
	int ret = 0;
	CDataBaseClient* pClient = new CSqlite3Client();
	KeyValue args;
	args["host"] = "test.db";
	ret = pClient->Connect(args);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Create());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Delete(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("3012265452");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	ret = pClient->Exec(test.Insert(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	ret = pClient->Exec(test.Modify(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	Result result;
	ret = pClient->Exec(test.Query(), result, test);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Drop());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Close();
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	getchar();
	return 0;
}

DECLARE_TABLE_CLASS(user_test_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, NOT_NULL | DEFAULT, "VARCHAR", "(12)", "18888888888", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, 0, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_END()

int mysql_test() {
	user_test_mysql test, value;
	printf("create:%s\n", (char*)test.Create());
	printf("Delete:%s\n", (char*)test.Delete(test));
	value.Fields["user_qq"]->LoadFromStr("3012265452");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	printf("Insert:%s\n", (char*)test.Insert(value));
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	printf("Modify:%s\n", (char*)test.Modify(value));
	printf("Query:%s\n", (char*)test.Query());
	printf("Drop:%s\n", (char*)test.Drop());
	getchar();
	int ret = 0;
	CDataBaseClient* pClient = new CMysqlClient();
	KeyValue args;
	args["host"] = "127.0.0.1";
	args["user"] = "root";
	args["password"] = "1218130";
	args["port"] = 3306;
	args["db"] = "hello";
	ret = pClient->Connect(args);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Create());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Delete(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("3012265452");
	value.Fields["user_qq"]->Condition = SQL_INSERT;
	ret = pClient->Exec(test.Insert(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	value.Fields["user_qq"]->LoadFromStr("123456789");
	value.Fields["user_qq"]->Condition = SQL_MODIFY;
	ret = pClient->Exec(test.Modify(value));
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	Result result;
	ret = pClient->Exec(test.Query(), result, test);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Exec(test.Drop());
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	ret = pClient->Close();
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	getchar();
	return 0;
}

int main()
{
	int ret = 0;
	ret = mysql_test();
	printf("ret = %d\n", ret);
	return ret;
}


