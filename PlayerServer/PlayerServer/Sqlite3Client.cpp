#include "Sqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args)
{
    auto it = args.find("host");
    if (it == args.end()) return -1;
    if (m_db != NULL) return -2;
    int ret = sqlite3_open(it->second, &m_db);
    if (ret != 0) {
        TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -3;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql)
{
    if (m_db == NULL) return -1;
    int ret = sqlite3_exec(m_db, sql, NULL/*回调函数，这里不需要数据的处理，不设置回调函数*/, this, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={%s}", sql);
		TRACEE("Exec1 failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table)
{
    char* errmsg = NULL;/*错误信息*/
    if (m_db == NULL) return -1;
    ExecParam param(this, result, table);
    /*阻塞的  只有当回调函数执行完成才会执行下面的代码，此时局部变量在销毁前就已经使用完成了*/
    /*每查到一行的结果就会调用回调函数，反复调用，所有的结果都存放在result里面*/
    int ret = sqlite3_exec(m_db, sql, &CSqlite3Client::ExecCallback, (void*)&param/*传入到回调函数*/, &errmsg);
	if (ret != SQLITE_OK) {
		TRACEE("sql={%s}", sql);
		TRACEE("Exec1 failed:%d [%s]", ret, errmsg);
        if (errmsg)sqlite3_free(errmsg);
        return -2;
	}
	if (errmsg)sqlite3_free(errmsg);

    return 0;
}
/*开始事务*/
int CSqlite3Client::StartTransaction()
{
    if (m_db == NULL) return -1;
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL);/*执行这条sql语句*/
    if (ret != SQLITE_OK) {
        TRACEE("sql={BEGIN TRANSACTION}");
        TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}
/*提交事务*/
int CSqlite3Client::CommitTransaction()
{
	if (m_db == NULL)return -1;
    int ret = sqlite3_exec(m_db, "COMMIT TRANSACTION", 0, 0, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={COMMIT TRANSACTION}");
        TRACEE("COMMIT failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}
/*回滚事务*/
int CSqlite3Client::RollbackTransaction()
{
    if (m_db == NULL)return -1;
    int ret = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", 0, 0, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={ROLLBACK TRANSACTION}");
        TRACEE("ROLLBACK failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
	return 0;
}

int CSqlite3Client::Close()
{
    if (m_db == NULL)return -1;
    int ret = sqlite3_close(m_db);
    if (ret != SQLITE_OK) {
        TRACEE("Close failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    m_db = NULL;
	return 0;
}

bool CSqlite3Client::IsConnected()
{
    return m_db != NULL;
}

int CSqlite3Client::ExecCallback(void* arg, int count, char** names, char** values)
{
    ExecParam* param = (ExecParam*)arg;
    return param->obj->ExecCallback(param->result, param->table, count, names, values);
}
/*                                空，存放结果  对数据库返回的数据进行操作        自动生成的？？？？？？？*/
int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
    PTable pTable = table.Copy();
    if (pTable == nullptr) {
		printf("table %s error!\n", (const char*)(Buffer)table);
		return -1;
    }
    /*把每个列的数据填好*/
	for (int i = 0; i < count; i++) {
		Buffer name = names[i];
		auto it = pTable->Fields.find(name);
		if (it == pTable->Fields.end()) {
            printf("table %s error!\n", (const char*)(Buffer)table);
			return -2;
		}
		if (values[i] != NULL)
			it->second->LoadFromStr(values[i]);
	}
    /*记录结果*/
	result.push_back(pTable);
	return 0;
}
