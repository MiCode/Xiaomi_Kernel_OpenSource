/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: ini.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: parsing function of INI file
*
************************************************************************/
#include <linux/string.h>
#include <linux/kernel.h>
#include <asm/unistd.h>

#include "ini.h"


char CFG_SSL = '[';  /* 项标志符Section Symbol --可根据特殊需要进行定义更改，如 { }等*/
char CFG_SSR = ']';  /* 项标志符Section Symbol --可根据特殊需要进行定义更改，如 { }等*/
char CFG_NIS = ':';  /* name 与 index 之间的分隔符 */
char CFG_NTS = '#';  /* 注释符*/
char CFG_EQS = '=';  /*等号*/

ST_INI_FILE_DATA g_st_ini_file_data[MAX_KEY_NUM];
int g_used_key_num = 0;

char *ini_str_trim_r(char *buf);
char *ini_str_trim_l(char *buf);
static int ini_file_get_line(char *filedata, char *buffer, int maxlen);

static long atol(char *nptr);


/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

int fts_strncmp(const char *cs, const char *ct, size_t count)
{
	unsigned char c1 = 0, c2 = 0;

	while (count) {
		c1 = TOLOWER(*cs++);
		c2 = TOLOWER(*ct++);
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
		if (!c1)
			break;
		count--;
	}
	return 0;
}

/*************************************************************
Function: 获得key的值
Input: char * filedata　文件；char * section　项值；char * key　键值
Output: char * value　key的值
Return: 0		SUCCESS
		-1		未找到section
		-2		未找到key
		-10		文件打开失败
		-12		读取文件失败
		-14		文件格式错误
		-22		超出缓冲区大小
Note:
*************************************************************/
int ini_get_key(char *filedata, char *section, char *key, char *value)
{
	int i = 0;
	int ret = -2;
	for (i = 0; i < g_used_key_num; i++)
	{
		if (fts_strncmp(section, g_st_ini_file_data[i].pSectionName,
			 g_st_ini_file_data[i].iSectionNameLen) != 0)
			 continue;

		if (fts_strncmp(key, g_st_ini_file_data[i].pKeyName,  strlen(key)) == 0)

		{
			memcpy(value, g_st_ini_file_data[i].pKeyValue, g_st_ini_file_data[i].iKeyValueLen);
			ret = 0;
			break;
		}
	}

	return ret;
}
/*************************************************************
Function: 获得所有section
Input:  char *filename　文件,int max 最大可返回的section的个数
Output: char *sections[]　存放section名字
Return: 返回section个数。若出错，返回负数。
		-10			文件打开出错
		-12			文件读取错误
		-14			文件格式错误
Note:
*************************************************************/
int ini_get_sections(char *filedata, unsigned char *sections[], int max)
{

	char buf1[MAX_CFG_BUF + 1];
	int n, n_sections = 0, ret;
	int dataoff = 0;




	while(1) {/*搜找项section */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata+dataoff, buf1, MAX_CFG_BUF);
		dataoff += n;
		if (n < -1)
			goto cfg_scts_end;
		if (n < 0)
			break;/* 文件尾 */
		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if (n == 0 || buf1[0] == CFG_NTS)
			continue;       /* 空行 或 注释行 */
		ret = CFG_ERR_FILE_FORMAT;
		if (n > 2 && ((buf1[0] == CFG_SSL && buf1[n-1] != CFG_SSR)))
			goto cfg_scts_end;
		if (buf1[0] == CFG_SSL) {
			if (max != 0){
				buf1[n-1] = 0x00;
				strcpy((char *)sections[n_sections], buf1+1);
				if (n_sections>=max)
					break;		/* 超过可返回最大个数 */
			}
			n_sections++;
		}

	}
	ret = n_sections;
cfg_scts_end:


	return ret;
}


/*************************************************************
Function: 去除字符串右边的空字符
Input:  char * buf 字符串指针
Output:
Return: 字符串指针
Note:
*************************************************************/
char *ini_str_trim_r(char *buf)
{
	int len,i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);


	memset(tmp,0x00,len);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp,(buf+i),(len-i));
	}
	strncpy(buf,tmp,len);

	return buf;
}

/*************************************************************
Function: 去除字符串左边的空字符
Input:  char * buf 字符串指针
Output:
Return: 字符串指针
Note:
*************************************************************/
char *ini_str_trim_l(char *buf)
{
	int len,i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);


	memset(tmp,0x00,len);

	for (i = 0; i < len; i++) {
		if (buf[len-i-1] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp,buf,len-i);
	}
	strncpy(buf,tmp,len);

	return buf;
}
/*************************************************************
Function: 从文件中读取一行
Input:  FILE *fp 文件句柄；int maxlen 缓冲区最大长度
Output: char *buffer 一行字符串
Return: >0		实际读的长度
		-1		文件结束
		-2		读文件出错
Note:
*************************************************************/
static int ini_file_get_line(char *filedata, char *buffer, int maxlen)
{
	int i = 0;
	int j = 0;
	int iRetNum = -1;
	char ch1 = '\0';

	for (i = 0, j = 0; i < maxlen; j++) {
		ch1 = filedata[j];
		iRetNum = j+1;
		if (ch1 == '\n' || ch1 == '\r')
		{
			ch1 = filedata[j+1];
			if (ch1 == '\n' || ch1 == '\r')
			{
				iRetNum++;
			}

			break; 换行
		}else if (ch1 == 0x00)
		{
			iRetNum = -1;
			break;
		}
		else
		{
			buffer[i++] = ch1;    /* 忽略回车符 */
		}
	}
	buffer[i] = '\0';

	return iRetNum;

	/*for(i=0, j=0; i<maxlen; j++) {
		ch1 = filedata[j];
		if(ch1 == '\n' || ch1 == 0x00)
			break; // 换行
		if(ch1 == '\f' || ch1 == 0x1A) {      //'\f':换页符也算有效字符
			buffer[i++] = ch1;
			break;
		}
		if(ch1 != '\r') buffer[i++] = ch1;    // 忽略回车符
	}
	buffer[i] = '\0';


	return i+2; */
}
/*************************************************************
Function: 分离key和value
			key=val
			jack   =   liaoyuewang
			|      |   |
			k1     k2  i
Input:  char *buf
Output: char **key, char **val
Return: 1 --- ok
		0 --- blank line
		-1 --- no key, "= val"
		-2 --- only key, no '='
Note:
*************************************************************/
/*static int  ini_split_key_value(char *buf, char **key, char **val)
{
	int  i, k1, k2, n;

	//FTS_DBG("section = %s \n",section);
	//FTS_DBG("Enter ini_split_key_value() \n");

	if((n = strlen((char *)buf)) < 1)
		return 0;
	for(i = 0; i < n; i++)
		if(buf[i] != ' ' && buf[i] != '\t')
			break;

	if(i >= n)
		return 0;

	if(buf[i] == '=')
		return -1;

	k1 = i;
	for(i++; i < n; i++)
		if(buf[i] == '=')
			break;

	if(i >= n)
		return -2;
	k2 = i;

	for(i++; i < n; i++)
		if(buf[i] != ' ' && buf[i] != '\t')
			break;

	buf[k2] = '\0';

	*key = buf + k1;
	*val = buf + i;
	return 1;
}*/

int my_atoi(const char *str)
{
	int result = 0;
	int signal = 1; /* 默认为正数 */
	if ((*str>='0' && *str<='9') || *str == '-' || *str == '+') {
		if (*str == '-' || *str == '+') {
			if (*str == '-')
				signal = -1; /*输入负数*/
			str++;
		}
	}
	else
		return 0;
	/*开始转换*/
	while(*str>='0' && *str<='9')
	   result = result*10 + (*str++ - '0');

	return signal*result;
}

int isspace(int x)
{
    if (x == ' ' || x == '\t' || x == '\n' || x == '\f' || x == '\b' || x == '\r')
        return 1;
    else
        return 0;
}

int isdigit(int x)
{
    if (x<='9' && x>='0')
        return 1;
    else
        return 0;

}

static long atol(char *nptr)
{
	int c; /* current char */
	long total; /* current total */
	int sign; /* if ''-'', then negative, otherwise positive */
	/* skip whitespace */
	while (isspace((int)(unsigned char)*nptr))
		++nptr;
	c = (int)(unsigned char)*nptr++;
	sign = c; /* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++; /* skip sign */
	total = 0;
	while (isdigit(c)) {
		total = 10 * total + (c - '0'); /* accumulate digit */
		c = (int)(unsigned char)*nptr++; /* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total; /* return result, negated if necessary */
}
/***
*int atoi(char *nptr) - Convert string to long
*
*Purpose:
* Converts ASCII string pointed to by nptr to binary.
* Overflow is not detected. Because of this, we can just use
* atol().
*
*Entry:
* nptr = ptr to string to convert
*
*Exit:
* return int value of the string
*
*Exceptions:
* None - overflow is not detected.
*
*******************************************************************************/
int atoi(char *nptr)
{
	return (int)atol(nptr);
}

int init_key_data(void)
{
	int i = 0;

	g_used_key_num = 0;

	for (i = 0; i < MAX_KEY_NUM; i++)
		{
		memset(g_st_ini_file_data[i].pSectionName, 0, MAX_KEY_NAME_LEN);
		memset(g_st_ini_file_data[i].pKeyName, 0, MAX_KEY_NAME_LEN);
		memset(g_st_ini_file_data[i].pKeyValue, 0, MAX_KEY_VALUE_LEN);
		g_st_ini_file_data[i].iSectionNameLen = 0;
		g_st_ini_file_data[i].iKeyNameLen = 0;
		g_st_ini_file_data[i].iKeyValueLen = 0;
		}

	return 1;
}

/*************************************************************
Function:读取所有的参数及其值到结构体里
Return: 返回key个数。若出错，返回负数。
		-10			文件打开出错
		-12			文件读取错误
		-14			文件格式错误
Note:
*************************************************************/
int ini_get_key_data(char *filedata)
{

	char buf1[MAX_CFG_BUF + 1] = {0};
	int n = 0;
	int ret = 0;
	int dataoff = 0;
	int iEqualSign = 0;
	int i = 0;
	char tmpSectionName[MAX_CFG_BUF + 1] = {0};




	init_key_data();/*init*/

	g_used_key_num = 0;
	while(1) {/*搜找项section */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata+dataoff, buf1, MAX_CFG_BUF);

		if (n < -1)
			goto cfg_scts_end;
		if (n < 0)
			break;/* 文件尾 */

		dataoff += n;

		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if (n == 0 || buf1[0] == CFG_NTS)
			continue;       /* 空行 或 注释行 */
		ret = CFG_ERR_FILE_FORMAT;

		if (n > 2 && ((buf1[0] == CFG_SSL && buf1[n-1] != CFG_SSR)))
			{
			printk("Bad Section:%s\n\n", buf1);
			goto cfg_scts_end;
			}


		if (buf1[0] == CFG_SSL) {
			g_st_ini_file_data[g_used_key_num].iSectionNameLen = n-2;
			if (MAX_KEY_NAME_LEN < g_st_ini_file_data[g_used_key_num].iSectionNameLen)
			{
				ret = CFG_ERR_OUT_OF_LEN;
				printk("MAX_KEY_NAME_LEN: CFG_ERR_OUT_OF_LEN\n\n");
				goto cfg_scts_end;
			}

			buf1[n-1] = 0x00;
			strcpy((char *)tmpSectionName, buf1+1);


			continue;
		}


		strcpy(g_st_ini_file_data[g_used_key_num].pSectionName, tmpSectionName);
		g_st_ini_file_data[g_used_key_num].iSectionNameLen = strlen(tmpSectionName);

		iEqualSign = 0;
		for (i = 0; i < n; i++)
			{
				if (buf1[i] == CFG_EQS)
					{
						iEqualSign = i;
						break;
					}
			}
		if (0 == iEqualSign)
			continue;
		/*等号前一段赋给键名*/
		g_st_ini_file_data[g_used_key_num].iKeyNameLen = iEqualSign;
		if (MAX_KEY_NAME_LEN < g_st_ini_file_data[g_used_key_num].iKeyNameLen)
			{
				ret = CFG_ERR_OUT_OF_LEN;
				printk("MAX_KEY_NAME_LEN: CFG_ERR_OUT_OF_LEN\n\n");
				goto cfg_scts_end;
			}
		memcpy(g_st_ini_file_data[g_used_key_num].pKeyName,
			buf1, g_st_ini_file_data[g_used_key_num].iKeyNameLen);

		/*等号后一段赋给键值*/
		g_st_ini_file_data[g_used_key_num].iKeyValueLen = n-iEqualSign-1;
		if (MAX_KEY_VALUE_LEN < g_st_ini_file_data[g_used_key_num].iKeyValueLen)
			{
				ret = CFG_ERR_OUT_OF_LEN;
				printk("MAX_KEY_VALUE_LEN: CFG_ERR_OUT_OF_LEN\n\n");
				goto cfg_scts_end;
			}
		memcpy(g_st_ini_file_data[g_used_key_num].pKeyValue,
			buf1+ iEqualSign+1, g_st_ini_file_data[g_used_key_num].iKeyValueLen);


		ret = g_used_key_num;

		/*printk("Param Name = %s, Value = %s\n",
			g_st_ini_file_data[g_used_key_num].pKeyName,
			g_st_ini_file_data[g_used_key_num].pKeyValue
			);*/

		g_used_key_num++;/*参数个数累加*/
		if (MAX_KEY_NUM < g_used_key_num)
			{
				ret = CFG_ERR_TOO_MANY_KEY_NUM;
				printk("MAX_KEY_NUM: CFG_ERR_TOO_MANY_KEY_NUM\n\n");
				goto cfg_scts_end;
			}
	}

cfg_scts_end:

	return ret;
}

