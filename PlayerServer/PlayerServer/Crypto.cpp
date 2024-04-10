#include "Crypto.h"

Buffer Crypto::MD5(const Buffer& text)
{
    Buffer result;
    Buffer data(16);
    MD5_CTX md5;
    /*��ʼ��*/
    MD5_Init(&md5);
    MD5_Update(&md5, text, text.size());
    /*����md5*/
    MD5_Final(data, &md5);
    /*��������ת��*/
    char temp[3] = "";
    for (size_t i{}; i < data.size(); i++) {
        snprintf(temp, sizeof(temp), "%02X", data[i] & 0xFF);
        result += temp;
    }
    return result;
}
