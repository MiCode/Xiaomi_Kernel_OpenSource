#include "base64.h"
#include <linux/vmalloc.h>
#include <linux/memory.h>

static const char *g_szV64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *EncodeBase64A(const unsigned char *pData, unsigned int nCount)
{
	int nIndex = 0;
	unsigned int i = 0;
	unsigned char val = 0;
	int nSize = nCount*8/6+4;
	int nM = nCount % 3;
	char *pResult = vmalloc(nSize);
	memset(pResult, 0, sizeof(char)*nSize);

	for (; i + 2 < nCount; i+=3)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		val |= (pData[i+1] >> 4);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+1] & 0xF) << 2;
		val |= (pData[i+2] >> 6);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+2] & 0x3F);
		pResult[nIndex++] = g_szV64[val];
	}

	if (nM == 1)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		pResult[nIndex++] = g_szV64[val];

		pResult[nIndex++] = '=';
		pResult[nIndex++] = '=';
	}
	else if (nM == 2)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		val |= (pData[i+1] >> 4);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+1] & 0xF) << 2;
		pResult[nIndex++] = g_szV64[val];

		pResult[nIndex++] = '=';
	}

	return pResult;
}

wchar_t *EncodeBase64W(const unsigned char *pData, unsigned int nCount)
{
	int nIndex = 0;
	unsigned int i = 0;
	unsigned char val = 0;
	int nSize = nCount*8/6+4;
	int nM = nCount % 3;
	wchar_t *pResult = (wchar_t*)vmalloc(nSize  *sizeof(wchar_t));
	memset(pResult, 0, sizeof(wchar_t)*nSize);

	for (; i + 2 < nCount; i+=3)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		val |= (pData[i+1] >> 4);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+1] & 0xF) << 2;
		val |= (pData[i+2] >> 6);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+2] & 0x3F);
		pResult[nIndex++] = g_szV64[val];
	}

	if (nM == 1)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		pResult[nIndex++] = g_szV64[val];

		pResult[nIndex++] = '=';
		pResult[nIndex++] = '=';
	}
	else if (nM == 2)
	{
		val = (pData[i] >> 2);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i] & 0x3) << 4;
		val |= (pData[i+1] >> 4);
		pResult[nIndex++] = g_szV64[val];

		val = (pData[i+1] & 0xF) << 2;
		pResult[nIndex++] = g_szV64[val];

		pResult[nIndex++] = '=';
	}

	return pResult;
}

unsigned char Base64Val(wchar_t chr)
{
	unsigned char i;

	for (i=0; i<64; i++)
	{
		if (g_szV64[i] == chr)
			return i;
	}

	if (chr == '=')
		return 64;

	return (unsigned char)255;
}

int DecodeBase64T(const void *pBase64Data, unsigned int nCount, int bIsWideChar, unsigned char* *ppRet)
{
	int nSize;
	int nIndex = 0;
	unsigned int i, loop;
	unsigned char val;
	wchar_t item;
	const char *pAnsiData = 0;
	const wchar_t *pUnicodeData = 0;

	if (bIsWideChar)
		pUnicodeData = (const wchar_t*)pBase64Data;
	else
		pAnsiData = (const char*)pBase64Data;

	if (nCount == 0)
	{
		if (pUnicodeData)
			while(pUnicodeData[nCount++] != '\0');
		else
			while(pAnsiData[nCount++] != '\0');

		nCount--;
	}

	nSize = nCount * 6/8 + 1;

	*ppRet = (unsigned char*)vmalloc(nSize * sizeof(unsigned char));
	memset(*ppRet, 0, nSize);

	for (i = 0; i < nCount; i+=4)
	{
		for (loop=0; loop<4; loop++)
		{
			if (pUnicodeData)
				item = pUnicodeData[i + loop];
			else
				item = pAnsiData[i + loop];

			if ( (i + loop >= nCount) || (item == '='))
			{
				return nIndex;
			}

			val = Base64Val(item);
			if (val >= 64)
			{
				vfree(*ppRet);
				*ppRet = 0;
				return 0;
			}

			if (loop == 0)
			{
				(*ppRet)[nIndex] = val<<2;
			}
			else if (loop == 1)
			{
				(*ppRet)[nIndex] |= (val >> 4);
				(*ppRet)[++nIndex] = (val & 0xF) << 4;
			}
			else if (loop == 2)
			{
				(*ppRet)[nIndex] |= (val >> 2);
				(*ppRet)[++nIndex] = ((val & 0x3) << 6);
			}
			else
			{
				(*ppRet)[nIndex++] |= val;
			}
		}
	}

	return nIndex;
}

int DecodeBase64A(const char *pData, unsigned int nCount, unsigned char* *ppRet)
{
	return DecodeBase64T(pData, nCount, 0, ppRet);
}

int DecodeBase64W(const wchar_t *pData, unsigned int nCount, unsigned char* *ppRet)
{
	return DecodeBase64T(pData, nCount, 1, ppRet);
}
