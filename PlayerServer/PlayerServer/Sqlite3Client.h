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
	/*连接*/
	virtual int Connect(const KeyValue& args);
	/*开始执行*/
	virtual int Exec(const Buffer& sql);
	/*开始执行，带结果*/
	virtual int Exec(const Buffer& sql, Result& result/*查询结果 表的类型*/, const _Table_& table);
	/*开始事务*/
	virtual int StartTransaction();
	/*提交事务*/
	virtual int CommitTransaction();
	/*回滚事务*/
	virtual int RollbackTransaction();
	/*关闭连接*/
	virtual int Close();
	/*是否连接  true表示连接中  false表示未连接*/
	virtual bool IsConnected();
private:
	/*回调函数*/
	/*回调函数的设计思想 使用静态成员函数转到成员函数*/
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

