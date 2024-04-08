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
    int ret = sqlite3_exec(m_db, sql, NULL/*�ص����������ﲻ��Ҫ���ݵĴ��������ûص�����*/, this, NULL);
    if (ret != SQLITE_OK) {
        TRACEE("sql={%s}", sql);
		TRACEE("Exec1 failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table)
{
    char* errmsg = NULL;/*������Ϣ*/
    if (m_db == NULL) return -1;
    ExecParam param(this, result, table);
    /*������  ֻ�е��ص�����ִ����ɲŻ�ִ������Ĵ��룬��ʱ�ֲ�����������ǰ���Ѿ�ʹ�������*/
    /*ÿ�鵽һ�еĽ���ͻ���ûص��������������ã����еĽ���������result����*/
    int ret = sqlite3_exec(m_db, sql, &CSqlite3Client::ExecCallback, (void*)&param/*���뵽�ص�����*/, &errmsg);
	if (ret != SQLITE_OK) {
		TRACEE("sql={%s}", sql);
		TRACEE("Exec1 failed:%d [%s]", ret, errmsg);
        if (errmsg)sqlite3_free(errmsg);
        return -2;
	}
	if (errmsg)sqlite3_free(errmsg);

    return 0;
}
/*��ʼ����*/
int CSqlite3Client::StartTransaction()
{
    if (m_db == NULL) return -1;
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL);/*ִ������sql���*/
    if (ret != SQLITE_OK) {
        TRACEE("sql={BEGIN TRANSACTION}");
        TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
        return -2;
    }
    return 0;
}
/*�ύ����*/
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
/*�ع�����*/
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
/*                                �գ���Ž��  �����ݿⷵ�ص����ݽ��в���        �Զ����ɵģ�������������*/
int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
    PTable pTable = table.Copy();
    if (pTable == nullptr) {
		printf("table %s error!\n", (const char*)(Buffer)table);
		return -1;
    }
    /*��ÿ���е��������*/
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
    /*��¼���*/
	result.push_back(pTable);
	return 0;
}
