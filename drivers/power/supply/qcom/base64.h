#ifndef _BASE64_H_
#define _BASE64_H_

typedef unsigned short wchar_t;

char *EncodeBase64A(const unsigned char *pData, unsigned int nCount);
wchar_t *EncodeBase64W(const unsigned char *pData, unsigned int nCount);

int DecodeBase64A(const char *szBase64Data, unsigned int nCount, unsigned char* *ppRet);
int DecodeBase64W(const wchar_t *szBase64Data, unsigned int nCount, unsigned char* *ppRet);

#ifdef UNICODE
#define EncodeBase64 EncodeBase64W
#define DecodeBase64 DecodeBase64W
#else
#define EncodeBase64 EncodeBase64A
#define DecodeBase64 DecodeBase64A
#endif

#endif
