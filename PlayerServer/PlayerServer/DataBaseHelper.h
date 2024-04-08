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
	/*����*/
	virtual int Connect(const KeyValue& args) = 0;
	/*��ʼִ��*/
	virtual int Exec(const Buffer& sql) = 0;
	/*��ʼִ�У������*/
	virtual int Exec(const Buffer& sql, Result& result/*��ѯ��� �������*/, const _Table_& table) = 0;
	/*��ʼ����*/
	virtual int StartTransaction() = 0;
	/*�ύ����*/
	virtual int CommitTransaction() = 0;
	/*�ع�����*/
	virtual int RollbackTransaction() = 0;
	/*�ر�����*/
	virtual int Close() = 0;
	/*�Ƿ�����*/
	virtual bool IsConnected() = 0;
};

/*����еĻ����ʵ��*/
class _Field_;
using PField = std::shared_ptr<_Field_>;
using FieldArray = std::vector<PField>;
using FiledMap = std::map<Buffer, PField>;

class _Table_ {
public:
	_Table_() {}
	virtual ~_Table_() {}
	/*���ش�����SQL���*/
	virtual Buffer Create() = 0;
	/*ɾ����*/
	virtual Buffer Drop() = 0;
	/*������ɾ�Ĳ�*/
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query() = 0;
	/*����һ�����ڱ�Ķ���*/
	virtual PTable Copy()const = 0;
public:
	/*��ȡ���ȫ��*/
	virtual operator const Buffer() const = 0;
public:
	/*��������DB������  ���ݿ�����������DB ÿ��DB�ж��ű�*/
	Buffer Database;
	Buffer Name;/*���ݿ������*/
	FieldArray FieldDefune;/*�еĶ��� �洢��ѯ���*/
	FiledMap Fields;/*�еĶ���ӳ���  ר��Ϊ������Ƶ�*/
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
	/*where ���ʹ�õ�  ���ɵ��ںű��ʽ*/
	virtual Buffer toEqualExp() const = 0;
	virtual Buffer toSqlStr() const = 0;
	/*�õ��е�ȫ��*/
	virtual operator const Buffer()const = 0;
public:
	Buffer Name;/*����*/
	Buffer Type;/*����*/
	Buffer Size;/*Sql��������size����*/
	unsigned Attr;/*����*/
	Buffer Default;/*Ĭ��ֵ*/
	Buffer Check;/*Լ������*/
};
