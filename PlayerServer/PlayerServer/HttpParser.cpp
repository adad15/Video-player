#include "HttpParser.h"
/*解析url*/
CHttpParser::CHttpParser()
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser));
	m_parser.data = this;/*把CHttpParser对象this指针传入到http_parser中*/
	/*http_parser初始化*/
	http_parser_init(&m_parser, HTTP_REQUEST/*客户端请求*/);
	/*struct http_parser_settings {
	http_cb      on_message_begin;
	http_data_cb on_url;
	http_data_cb on_status;
	http_data_cb on_header_field;
	http_data_cb on_header_value;
	http_cb      on_headers_complete;
	http_data_cb on_body;
	http_cb      on_message_complete;
	http_cb      on_chunk_header;
	http_cb      on_chunk_complete;
	}; */
	/*注册回调函数*/
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin;
	m_settings.on_url = &CHttpParser::OnUrl;
	m_settings.on_status = &CHttpParser::OnStatus;
	m_settings.on_header_field = &CHttpParser::OnHeaderField;
	m_settings.on_header_value = &CHttpParser::OnHeaderValue;
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;
	m_settings.on_body = &CHttpParser::OnBody;
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser & http)
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this;/*切换成自己的*/
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http)
{
	if (this != &http) {
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings)); 
		m_status = http.m_status;
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data)
{
	m_complete = false;
	/*开始解析*/
	/*m_parser里面含有CHttpParser对象，解析后的数据放在这里              要解析的url */
	size_t ret = http_parser_execute(&m_parser, &m_settings/*回调函数*/, data, data.size());
	if (m_complete == false) {
		m_parser.http_errno = 0x7F;
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)
{
	/*(CHttpParser*)parser->data是CHttpParser对象的this指针*/
	return ((CHttpParser*)parser->data)->OnMessageBegin();
}
int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at, length);
}
int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at, length);
}
int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at, length);
}
int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at, length);
}
int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}
int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at, length);
}
int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length);
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}
int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);
	return 0;
}
int CHttpParser::OnHeadersComplete()
{
	return 0;
}
int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);
	return 0;
}
int CHttpParser::OnMessageComplete()
{
	m_complete = true;
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()
{
	//分三步：协议、域名和端口、uri、键值对
	//解析协议
	const char* pos = m_url;
	const char* target = strstr(pos, "://");
	if (target == NULL) return -1;
	/*pos 到 target-1这部分都是协议部分*/
	m_protocol = Buffer(pos, target);
	//解析域名和端口
	pos = target + 3;
	target = strchr(pos, '/');
	if (target == NULL) {
		if (m_protocol.size() + 3 >= m_url.size())
			return -2;/*url 不全*/
		m_host = pos;/*设置域名*/
		return 0;
	}
	Buffer value = Buffer(pos, target);/*target:域名+端口*/
	if (value.size() == 0)return -3;
	target = strchr(value, ':');/*target:域名*/
	if (target != NULL) {
		/*说明有端口信息*/
		m_host = Buffer(value, target);
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));
	}
	else {
		m_host = value;
	}
	pos = strchr(pos, '/');/*反斜杠也算进后面的数据了*/
	//解析uri
	target = strchr(pos, '?');
	if (target == NULL) {
		m_uri = pos;/*空*/
		return 0;
	}
	else {
		m_uri = Buffer(pos, target);
		//解析key和value
		pos = target + 1;
		const char* t = NULL;
		do 
		{
			target = strchr(pos, '&');
			if (target == NULL) {
				t = strchr(pos, '=');
				if (t == NULL)return -4;
				m_values[Buffer(pos, t)/*key*/] = Buffer(t + 1);
				/*之后target为空就跳出了while循环*/
			}
			else {
				Buffer kv(pos, target);
				t = strchr(kv, '=');
				if (t == NULL)return -5;
				m_values[Buffer(kv, t)/*key*/] = Buffer(t + 1, (char*)kv + kv.size());/*value*/
				pos = target + 1;
			}
		} while (target != NULL);
	}

	return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const
{
	auto it = m_values.find(name);
	if (it == m_values.end())return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)
{
	m_url = url;
	/*重新设置了要重新去拿*/
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
