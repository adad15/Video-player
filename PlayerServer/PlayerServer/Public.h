#pragma once
#include <string.h>
#include <string>

class Buffer :public std::string /*默认const char*/ {
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	Buffer(const char* str, size_t length) :std::string() {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;/*包含begin不包含end*/
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	operator void* () { return (char*)c_str(); }
	operator char* () { return (char*)c_str(); }
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* ()const { return (char*)c_str(); }
	operator const char* ()const { return c_str(); }
	operator const void* ()const { return c_str(); } 
};
