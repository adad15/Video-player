#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <memory>
#include <vector>

class _Table_;
using PTable = std::shared_ptr<_Table_>;
using KeyValue = std::map<Buffer, Buffer>;
using Result = std::list<PTable>;

class CDataBaseClient
{
public:
	CDataBaseClient(const CDataBaseClient&) = delete;
	CDataBaseClient& operator=(CDataBaseClient&) = delete;
public:
	CDataBaseClient() {}
	virtual ~CDataBaseClient() {}
public:
	/*连接*/
	virtual int Connect(const KeyValue& args) = 0;
	/*开始执行*/
	virtual int Exec(const Buffer& sql) = 0;
	/*开始执行，带结果*/
	virtual int Exec(const Buffer& sql, Result& result/*查询结果 表的类型*/, const _Table_& table) = 0;
	/*开始事务*/
	virtual int StartTransaction() = 0;
	/*提交事务*/
	virtual int CommitTransaction() = 0;
	/*回滚事务*/
	virtual int RollbackTransaction() = 0;
	/*关闭连接*/
	virtual int Close() = 0;
	/*是否连接*/
	virtual bool IsConnected() = 0;
};

/*表和列的基类的实现*/
class _Field_;
using PField = std::shared_ptr<_Field_>;
using FieldArray = std::vector<PField>;
using FiledMap = std::map<Buffer, PField>;

class _Table_ {
public:
	_Table_() {}
	virtual ~_Table_() {}
	/*返回创建的SQL语句*/
	virtual Buffer Create() = 0;
	/*删除表*/
	virtual Buffer Drop() = 0;
	/*数据增删改查*/
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query(const Buffer& condition = "") = 0;
	/*创建一个基于表的对象*/
	virtual PTable Copy()const = 0;
	virtual void ClearFieldUsed() = 0;
public:
	/*获取表的全名*/
	virtual operator const Buffer() const = 0;
public:
	/*表所属的DB的名称  数据库服务器有许多DB 每个DB有多张表*/
	Buffer Database;
	Buffer Name;/*数据库的名称*/
	FieldArray FieldDefine;/*列的定义 存储查询结果*/
	FiledMap Fields;/*列的定义映射表  专门为查找设计的*/
};

enum {
	SQL_INSERT = 1,//插入的列
	SQL_MODIFY = 2,//修改的列
	SQL_CONDITION = 4//查询条件列
};

enum {
	NONE = 0, 
	NOT_NULL = 1,
	DEFAULT = 2,/*默认值*/
	UNIQUE = 4,/*唯一*/
	PRIMARY_KEY = 8,/*主键*/
	CHECK = 16,/*约束*/
	AUTOINCREMENT = 32/*自动增长*/
};

using SqlType = enum {
	TYPE_NULL = 0,
	TYPE_BOOL = 1,
	TYPE_INT = 2,
	TYPE_DATETIME = 4,
	TYPE_REAL = 8,/*小数*/
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64
};

class _Field_ {
public:
	_Field_() {}
	_Field_(const _Field_& field) {
		Name = field.Name;
		Type = field.Type;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field) {
		if (this != &field) {
			Name = field.Name;
			Type = field.Type;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}
	virtual~_Field_() {}
public:
	virtual Buffer Create() = 0;
	virtual void LoadFromStr(const Buffer& str) = 0;
	/*where 语句使用的  生成等于号表达式*/
	virtual Buffer toEqualExp() const = 0;
	virtual Buffer toSqlStr() const = 0;
	/*拿到列的全名*/
	virtual operator const Buffer()const = 0;
public:
	Buffer Name;/*名称*/
	Buffer Type;/*类型*/
	Buffer Size;/*Sql语句里面的size参数*/
	unsigned Attr;/*属性*/
	Buffer Default;/*默认值*/
	Buffer Check;/*约束条件*/
public:
	unsigned Condition;/*操作条件*/
	union {
		bool Bool;
		int Integer;/*整数*/
		double Double;
		Buffer* String;/*联合体不会调用构造，所以这里使用指针*/
	}Value;
	int nType;
};