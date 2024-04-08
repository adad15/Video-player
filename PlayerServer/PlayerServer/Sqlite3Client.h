#pragma once
#include "Public.h"
#include "DataBaseHelper.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client:public CDataBaseClient
{
public:
	CSqlite3Client(const CSqlite3Client&) = delete;
	CSqlite3Client& operator=(CSqlite3Client&) = delete;
public:
	CSqlite3Client() { 
		m_db = NULL; 
		m_stmt = NULL; 
	}
	virtual ~CSqlite3Client() {
		Close();
	}
public:
	/*����*/
	virtual int Connect(const KeyValue& args);
	/*��ʼִ��*/
	virtual int Exec(const Buffer& sql);
	/*��ʼִ�У������*/
	virtual int Exec(const Buffer& sql, Result& result/*��ѯ��� �������*/, const _Table_& table);
	/*��ʼ����*/
	virtual int StartTransaction();
	/*�ύ����*/
	virtual int CommitTransaction();
	/*�ع�����*/
	virtual int RollbackTransaction();
	/*�ر�����*/
	virtual int Close();
	/*�Ƿ�����  true��ʾ������  false��ʾδ����*/
	virtual bool IsConnected();
private:
	/*�ص�����*/
	/*�ص����������˼�� ʹ�þ�̬��Ա����ת����Ա����*/
	static int ExecCallback(void* arg, int count, char** names, char** values);
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;
	sqlite3* m_db;
	class ExecParam
	{
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table) 
			:obj(obj), result(result), table(table)
		{}
		~ExecParam();

		CSqlite3Client* obj;
		Result& result;
		const _Table_& table;
	};
};

