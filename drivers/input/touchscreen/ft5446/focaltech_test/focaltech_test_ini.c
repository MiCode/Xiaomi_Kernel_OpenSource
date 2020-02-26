/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
*
* File Name: focaltech_test_ini.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: parsing function of INI file
*
************************************************************************/
#include "focaltech_test.h"


/* '[' ']':Section Symbol-Can be defined according to the special need to change */
const char CFG_SSL = '[';
const char CFG_SSR = ']';
const char CFG_NIS = ':';  /* Separator between name and index */
const char CFG_NTS = '#';  /* annotator */
const char CFG_EQS = '=';  /* The equal sign */

/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)
int fts_strncmp(const char *cs, const char *ct, size_t count)
{
	u8 c1 = 0, c2 = 0;

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
Function:  Get the value of key
Input: char * filedata; char * section; char * key
Output: char * value¡¡key
Return: 0	   SUCCESS
		-1	  can not find section
		-2	  can not find key
		-10	 File open failed
		-12	 File read  failed
		-14	 File format error
		-22	 Out of buffer size
Note:
*************************************************************/
int ini_get_key(char *filedata, char *section, char *key, char *value)
{
	int i = 0;
	int ret = -2;
	for (i = 0; i < test_data.ini_keyword_num; i++) {
		if (fts_strncmp(section, test_data.ini_data[i].section_name,
						test_data.ini_data[i].section_name_len) != 0)
			continue;
		/*FTS_TEST_DBG("Section Name:%s, Len:%d\n",  test_data.ini_data[i].section_name, test_data.ini_data[i].section_name_len);*/
		if (strlen(key) == test_data.ini_data[i].key_name_len) {
			if (fts_strncmp(key, test_data.ini_data[i].key_name,  test_data.ini_data[i].key_name_len) == 0)
				/*test_data.ini_data[i].key_name_len) == 0)*/
			{
				memcpy(value, test_data.ini_data[i].key_value, test_data.ini_data[i].key_value_len);
				ret = 0;
				break;
			}
		}
	}

	return ret;
}

/*************************************************************
Function: Remove empty character on the right side of the string
Input:  char * buf --String pointer
Output:
Return: String pointer
Note:
*************************************************************/
char *ini_str_trim_r(char *buf)
{
	int len, i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);
	/*tmp = (char *)malloc(len);*/

	memset(tmp, 0x00, len);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp, (buf + i), (len - i));
	}
	strncpy(buf, tmp, len);
	/*free(tmp);*/
	return buf;
}

/*************************************************************
Function: Remove empty character on the left side of the string
Input: char * buf --String pointer
Output:
Return: String pointer
Note:
*************************************************************/
char *ini_str_trim_l(char *buf)
{
	int len, i;
	char tmp[512];

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);
	/*tmp = (char *)malloc(len);*/

	memset(tmp, 0x00, len);

	for (i = 0; i < len; i++) {
		if (buf[len - i - 1] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp, buf, len - i);
	}
	strncpy(buf, tmp, len);
	/*free(tmp);*/
	return buf;
}

/*************************************************************
Function: Read a line from file
Input:  FILE *fp; int maxlen-- Maximum length of buffer
Output: char *buffer --  A string
Return: >0	  Actual read length
		-1	  End of file
		-2	  Error reading file
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
		iRetNum = j + 1;
		if (ch1 == '\n' || ch1 == '\r') { /*line end*/
			ch1 = filedata[j + 1];
			if (ch1 == '\n' || ch1 == '\r') {
				iRetNum++;
			}

			break; /*line breaks*/
		} else if (ch1 == 0x00) {
			iRetNum = -1;
			break; /*file end*/
		} else {
			buffer[i++] = ch1;	/* ignore carriage return */
		}
	}
	buffer[i] = '\0';

	return iRetNum;
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
	if (x <= '9' && x >= '0')
		return 1;
	else
		return 0;
}

static long fts_atol(char *nptr)
{
	int c; /* current char */
	long total; /* current total */
	int sign; /* if ''-'', then negative, otherwise positive */
	/* skip whitespace */
	while (isspace((int)(unsigned char)*nptr))
		++nptr;
	c = (int)(unsigned char) *nptr++;
	sign = c; /* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char) *nptr++; /* skip sign */
	total = 0;
	while (isdigit(c)) {
		total = 10 * total + (c - '0'); /* accumulate digit */
		c = (int)(unsigned char) *nptr++; /* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total; /* return result, negated if necessary */
}

int fts_atoi(char *nptr)
{
	return (int)fts_atol(nptr);
}

int init_key_data(void)
{
	int i = 0;

	FTS_TEST_FUNC_ENTER();

	test_data.ini_keyword_num = 0;

	for (i = 0; i < MAX_KEYWORD_NUM; i++) {
		memset(test_data.ini_data[i].section_name, 0, MAX_KEYWORD_NAME_LEN);
		memset(test_data.ini_data[i].key_name, 0, MAX_KEYWORD_NAME_LEN);
		memset(test_data.ini_data[i].key_value, 0, MAX_KEYWORD_VALUE_LEN);
		test_data.ini_data[i].section_name_len = 0;
		test_data.ini_data[i].key_name_len = 0;
		test_data.ini_data[i].key_value_len = 0;
	}

	FTS_TEST_FUNC_EXIT();
	return 1;
}

int print_key_data(void)
{
	int i = 0;

	FTS_TEST_DBG("test_data.ini_keyword_num = %d",  test_data.ini_keyword_num);
	for (i = 0; i < MAX_KEYWORD_NUM; i++) {

		FTS_TEST_DBG("section_name_%d:%s, key_name_%d:%s\n,key_value_%d:%s",
					 i, test_data.ini_data[i].section_name,
					 i, test_data.ini_data[i].key_name,
					 i, test_data.ini_data[i].key_value
					);
	}

	return 0;
}

/*************************************************************
Function: Read all the parameters and values to the structure.
Return: Returns the number of key. If you go wrong, return a negative number.
		-10		 File open failed
		-12		 File read  failed
		-14		 File format error
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
	char tmsection_name[MAX_CFG_BUF + 1] = {0};

	FTS_TEST_FUNC_ENTER();
	ret = init_key_data();
	if (ret < 0) {
		FTS_TEST_ERROR("init key data failed");
		return -EIO;
	}

	test_data.ini_keyword_num = 0;
	while (1) { /*find section */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata + dataoff, buf1, MAX_CFG_BUF);
		if (n < -1)
			goto cfg_scts_end;
		if (n < 0)
			break;/* file end */
		if (n >= MAX_CFG_BUF) {
			FTS_TEST_ERROR("Error Length:%d\n",  n);
			goto cfg_scts_end;
		}
		dataoff += n;
		n = strlen(ini_str_trim_l(ini_str_trim_r(buf1)));
		if (n == 0 || buf1[0] == CFG_NTS)
			continue;	   /* A blank line or a comment line */
		ret = CFG_ERR_FILE_FORMAT;
		/*get section name*/
		if (n > 2 && ((buf1[0] == CFG_SSL && buf1[n - 1] != CFG_SSR))) {
			FTS_TEST_ERROR("Bad Section:%s\n",  buf1);
			goto cfg_scts_end;/*bad section*/
		}

		if (buf1[0] == CFG_SSL) {
			test_data.ini_data[test_data.ini_keyword_num].section_name_len = n - 2;
			if (MAX_KEYWORD_NAME_LEN < test_data.ini_data[test_data.ini_keyword_num].section_name_len) {
				ret = CFG_ERR_OUT_OF_LEN;
				FTS_TEST_ERROR("MAX_KEYWORD_NAME_LEN: CFG_ERR_OUT_OF_LEN\n");
				goto cfg_scts_end;
			}
			buf1[n - 1] = 0x00;
			strcpy((char *)tmsection_name, buf1 + 1);

			continue;
		}
		/*get section name end*/
		strcpy(test_data.ini_data[test_data.ini_keyword_num].section_name, tmsection_name);
		test_data.ini_data[test_data.ini_keyword_num].section_name_len = strlen(tmsection_name);

		iEqualSign = 0;
		for (i = 0; i < n; i++) {
			if (buf1[i] == CFG_EQS) {
				iEqualSign = i;
				break;
			}
		}
		if (0 == iEqualSign)
			continue;
		/* before equal sign is assigned to the key name*/
		test_data.ini_data[test_data.ini_keyword_num].key_name_len = iEqualSign;
		if (MAX_KEYWORD_NAME_LEN < test_data.ini_data[test_data.ini_keyword_num].key_name_len) {
			ret = CFG_ERR_OUT_OF_LEN;
			FTS_TEST_ERROR("MAX_KEYWORD_NAME_LEN: CFG_ERR_OUT_OF_LEN\n");
			goto cfg_scts_end;
		}
		memcpy(test_data.ini_data[test_data.ini_keyword_num].key_name,
			   buf1, test_data.ini_data[test_data.ini_keyword_num].key_name_len);

		/* After equal sign is assigned to the key value*/
		test_data.ini_data[test_data.ini_keyword_num].key_value_len = n - iEqualSign - 1;
		if (MAX_KEYWORD_VALUE_LEN < test_data.ini_data[test_data.ini_keyword_num].key_value_len) {
			ret = CFG_ERR_OUT_OF_LEN;
			FTS_TEST_ERROR("MAX_KEYWORD_VALUE_LEN: CFG_ERR_OUT_OF_LEN\n");
			goto cfg_scts_end;
		}
		memcpy(test_data.ini_data[test_data.ini_keyword_num].key_value,
			   buf1 + iEqualSign + 1, test_data.ini_data[test_data.ini_keyword_num].key_value_len);

		ret = test_data.ini_keyword_num;
		test_data.ini_keyword_num++;   /*Parameter number accumulation*/
		if (MAX_KEYWORD_NUM < test_data.ini_keyword_num) {
			ret = CFG_ERR_TOO_MANY_KEY_NUM;
			FTS_TEST_ERROR("MAX_KEYWORD_NUM: CFG_ERR_TOO_MANY_KEY_NUM\n");
			goto cfg_scts_end;
		}
	}

	FTS_TEST_FUNC_EXIT();

	return 0;

cfg_scts_end:

	FTS_TEST_FUNC_EXIT();
	return ret;
}

/*
 * fts_ic_table_get_ic_code_from_ic_name - Get IC NAME£¬From IC CODE
 */
unsigned int fts_ic_table_get_ic_code_from_ic_name(char *strIcName)
{

	if (strncmp(strIcName, "FT5X36", 6) == 0)
		return IC_FT5X36;
	if (strncmp(strIcName, "FT5X36i", 7) == 0)
		return IC_FT5X36i;
	if (strncmp(strIcName, "FT3X16", 6) == 0)
		return IC_FT3X16;
	if (strncmp(strIcName, "FT3X26", 6) == 0)
		return IC_FT3X26;
	if (strncmp(strIcName, "FT5X22", 6) == 0)
		return IC_FT5X46;
	if (strncmp(strIcName, "FT5X46", 6) == 0)
		return IC_FT5X46;
	if (strncmp(strIcName, "FT5X46i", 7) == 0)
		return IC_FT5X46i;
	if (strncmp(strIcName, "FT5526", 6) == 0)
		return IC_FT5526;
	if (strncmp(strIcName, "FT3X17", 6) == 0)
		return IC_FT3X17;
	if (strncmp(strIcName, "FT5436", 6) == 0)
		return IC_FT5436;
	if (strncmp(strIcName, "FT3X27", 6) == 0)
		return IC_FT3X27;
	if (strncmp(strIcName, "FT5526i", 7) == 0)
		return IC_FT5526I;
	if (strncmp(strIcName, "FT5416", 6) == 0)
		return IC_FT5416;
	if (strncmp(strIcName, "FT5426", 6) == 0)
		return IC_FT5426;
	if (strncmp(strIcName, "FT5435", 6) == 0)
		return IC_FT5435;
	if (strncmp(strIcName, "FT7681", 6) == 0)
		return IC_FT7681;
	if (strncmp(strIcName, "FT7661", 6) == 0)
		return IC_FT7661;
	if (strncmp(strIcName, "FT7511", 6) == 0)
		return IC_FT7511;
	if (strncmp(strIcName, "FT7421", 6) == 0)
		return IC_FT7421;
	if (strncmp(strIcName, "FT7311", 6) == 0)
		return IC_FT7311;
	if (strncmp(strIcName, "FT6X06", 6) == 0)
		return IC_FT6X06;
	if (strncmp(strIcName, "FT3X06", 6) == 0)
		return IC_FT3X06;
	if (strncmp(strIcName, "FT6X36", 6) == 0)
		return IC_FT6X36;
	if (strncmp(strIcName, "FT3X07", 6) == 0)
		return IC_FT3X07;
	if (strncmp(strIcName, "FT6416", 6) == 0)
		return IC_FT6416;
	if (strncmp(strIcName, "FT6336G/U", 9) == 0)
		return IC_FT6426;
	if (strncmp(strIcName, "FT6236U", 7) == 0)
		return IC_FT6236U;
	if (strncmp(strIcName, "FT6436U", 7) == 0)
		return IC_FT6436U;
	if (strncmp(strIcName, "FT3267", 6) == 0)
		return IC_FT3267;
	if (strncmp(strIcName, "FT3367", 6) == 0)
		return IC_FT3367;
	if (strncmp(strIcName, "FT7401", 6) == 0)
		return IC_FT7401;
	if (strncmp(strIcName, "FT3407U", 7) == 0)
		return IC_FT3407U;
	if (strncmp(strIcName, "FT5X16", 6) == 0)
		return IC_FT5X16;
	if (strncmp(strIcName, "FT5X12", 6) == 0)
		return IC_FT5X12;
	if (strncmp(strIcName, "FT5506", 6) == 0)
		return IC_FT5506;
	if (strncmp(strIcName, "FT5606", 6) == 0)
		return IC_FT5606;
	if (strncmp(strIcName, "FT5816", 6) == 0)
		return IC_FT5816;
	if (strncmp(strIcName, "FT5822", 6) == 0)
		return IC_FT5822;
	if (strncmp(strIcName, "FT5626", 6) == 0)
		return IC_FT5626;
	if (strncmp(strIcName, "FT5726", 6) == 0)
		return IC_FT5726;
	if (strncmp(strIcName, "FT5826B", 7) == 0)
		return IC_FT5826B;
	if (strncmp(strIcName, "FT3617", 6) == 0)
		return IC_FT3617;
	if (strncmp(strIcName, "FT3717", 6) == 0)
		return IC_FT3717;
	if (strncmp(strIcName, "FT7811", 6) == 0)
		return IC_FT7811;
	if (strncmp(strIcName, "FT5826S", 7) == 0)
		return IC_FT5826S;
	if (strncmp(strIcName, "FT3517U", 7) == 0)
		return IC_FT3517U;
	if (strncmp(strIcName, "FT5306", 6) == 0)
		return IC_FT5306;
	if (strncmp(strIcName, "FT5406", 6) == 0)
		return IC_FT5406;
	if (strncmp(strIcName, "FT8606", 6) == 0)
		return IC_FT8606;
	if (strncmp(strIcName, "FT8716U", 7) == 0)
		return IC_FT8716U;
	if (strncmp(strIcName, "FT8716", 6) == 0)
		return IC_FT8716;
	if (strncmp(strIcName, "FT8613", 6) == 0)
		return IC_FT8613;
	if (strncmp(strIcName, "FT3C47U", 7) == 0)
		return IC_FT3C47U;
	if (strncmp(strIcName, "FT8607U", 7) == 0)
		return IC_FT8607U;
	if (strncmp(strIcName, "FT8607", 6) == 0)
		return IC_FT8607;
	if (strncmp(strIcName, "FT8707", 6) == 0)
		return IC_FT8707;
	if (strncmp(strIcName, "FT8736", 6) == 0)
		return IC_FT8736;
	if (strncmp(strIcName, "FT3D47", 6) == 0)
		return IC_FT3D47;
	if (strncmp(strIcName, "FTE716", 6) == 0)
		return IC_FTE716;
	if (strncmp(strIcName, "FT5442", 6) == 0)
		return IC_FT5442;
	if (strncmp(strIcName, "FT3428U", 7) == 0)
		return IC_FT3428U;
	if (strncmp(strIcName, "FT8006M", 7) == 0)
		return IC_FT8006M;
	if (strncmp(strIcName, "FT8201", 6) == 0)
		return IC_FT8201;
	if (strncmp(strIcName, "FTE736", 6) == 0)
		return IC_FTE736;
	if (strncmp(strIcName, "FT8006U", 7) == 0)
		return IC_FT8006U;
	printk("%s.  can NOT get ic code.  ERROR  !!!  \n", __func__);

	return 0xff;
}

/*
 * fts_ic_table_get_ic_name_from_ic_code - Get IC CODE£¬From IC NAME
 */
void fts_ic_table_get_ic_name_from_ic_code(unsigned int ucIcCode, char *strIcName)
{
	if (NULL == strIcName) {
		FTS_TEST_ERROR("strIcName is null");
		return ;
	}

	sprintf(strIcName, "%s", "NA");/*if can't find IC , set 'NA'*/
	if (ucIcCode == IC_FT5X36)
		sprintf(strIcName, "%s", "FT5X36");
	if (ucIcCode == IC_FT5X36i)
		sprintf(strIcName, "%s",  "FT5X36i");
	if (ucIcCode == IC_FT3X16)
		sprintf(strIcName, "%s",  "FT3X16");
	if (ucIcCode == IC_FT3X26)
		sprintf(strIcName, "%s",  "FT3X26");

	/*if(ucIcCode == IC_FT5X46)sprintf(strIcName, "%s",  "FT5X46");*/
	if (ucIcCode == IC_FT5X46)
		sprintf(strIcName, "%s",  "FT5X46");
	if (ucIcCode == IC_FT5X46i)
		sprintf(strIcName, "%s",  "FT5X46i");
	if (ucIcCode == IC_FT5526)
		sprintf(strIcName, "%s",  "FT5526");
	if (ucIcCode == IC_FT3X17)
		sprintf(strIcName, "%s",  "FT3X17");
	if (ucIcCode == IC_FT5436)
		sprintf(strIcName, "%s",  "FT5436");
	if (ucIcCode == IC_FT3X27)
		sprintf(strIcName, "%s",  "FT3X27");
	if (ucIcCode == IC_FT5526I)
		sprintf(strIcName, "%s",  "FT5526i");
	if (ucIcCode == IC_FT5416)
		sprintf(strIcName, "%s",  "FT5416");
	if (ucIcCode == IC_FT5426)
		sprintf(strIcName, "%s",  "FT5426");
	if (ucIcCode == IC_FT5435)
		sprintf(strIcName, "%s",  "FT5435");
	if (ucIcCode == IC_FT7681)
		sprintf(strIcName, "%s",  "FT7681");
	if (ucIcCode == IC_FT7661)
		sprintf(strIcName, "%s",  "FT7661");
	if (ucIcCode == IC_FT7511)
		sprintf(strIcName, "%s",  "FT7511");
	if (ucIcCode == IC_FT7421)
		sprintf(strIcName, "%s",  "FT7421");

	if (ucIcCode == IC_FT6X06)
		sprintf(strIcName, "%s",  "FT6X06");
	if (ucIcCode == IC_FT3X06)
		sprintf(strIcName, "%s",  "FT3X06");

	if (ucIcCode == IC_FT6X36)
		sprintf(strIcName, "%s",  "FT6X36");
	if (ucIcCode == IC_FT3X07)
		sprintf(strIcName, "%s",  "FT3X07");
	if (ucIcCode == IC_FT6416)
		sprintf(strIcName, "%s",  "FT6416");
	if (ucIcCode == IC_FT6426)
		sprintf(strIcName, "%s",  "FT6336G/U");
	if (ucIcCode == IC_FT6236U)
		sprintf(strIcName, "%s",  "FT6236U");
	if (ucIcCode == IC_FT6436U)
		sprintf(strIcName, "%s",  "FT6436U");
	if (ucIcCode == IC_FT3267)
		sprintf(strIcName, "%s",  "FT3267");
	if (ucIcCode == IC_FT3367)
		sprintf(strIcName, "%s",  "FT3367");
	if (ucIcCode == IC_FT7401)
		sprintf(strIcName, "%s",  "FT7401");
	if (ucIcCode == IC_FT3407U)
		sprintf(strIcName, "%s",  "FT3407U");

	if (ucIcCode == IC_FT5X16)
		sprintf(strIcName, "%s",  "FT5X16");
	if (ucIcCode == IC_FT5X12)
		sprintf(strIcName, "%s",  "FT5X12");

	if (ucIcCode == IC_FT5506)
		sprintf(strIcName, "%s",  "FT5506");
	if (ucIcCode == IC_FT5606)
		sprintf(strIcName, "%s",  "FT5606");
	if (ucIcCode == IC_FT5816)
		sprintf(strIcName, "%s",  "FT5816");

	if (ucIcCode == IC_FT5822)
		sprintf(strIcName, "%s",  "FT5822");
	if (ucIcCode == IC_FT5626)
		sprintf(strIcName, "%s",  "FT5626");
	if (ucIcCode == IC_FT5726)
		sprintf(strIcName, "%s",  "FT5726");
	if (ucIcCode == IC_FT5826B)
		sprintf(strIcName, "%s",  "FT5826B");
	if (ucIcCode == IC_FT3617)
		sprintf(strIcName, "%s",  "FT3617");
	if (ucIcCode == IC_FT3717)
		sprintf(strIcName, "%s",  "FT3717");
	if (ucIcCode == IC_FT7811)
		sprintf(strIcName, "%s",  "FT7811");
	if (ucIcCode == IC_FT5826S)
		sprintf(strIcName, "%s",  "FT5826S");

	if (ucIcCode == IC_FT5306)
		sprintf(strIcName, "%s",  "FT5306");
	if (ucIcCode == IC_FT5406)
		sprintf(strIcName, "%s",  "FT5406");

	if (ucIcCode == IC_FT8606)
		sprintf(strIcName, "%s",  "FT8606");
	if (ucIcCode == IC_FT8716)
		sprintf(strIcName, "%s",  "FT8716");

	if (ucIcCode == IC_FT3C47U)
		sprintf(strIcName, "%s",  "FT3C47U");

	if (ucIcCode == IC_FT8607)
		sprintf(strIcName, "%s",  "FT8607");
	if (ucIcCode == IC_FT8707)
		sprintf(strIcName, "%s",  "FT8707");
	if (ucIcCode == IC_FT8736)
		sprintf(strIcName, "%s",  "FT8736");

	if (ucIcCode == IC_FT3D47)
		sprintf(strIcName, "%s",  "FT3D47");

	if (ucIcCode == IC_FTE716)
		sprintf(strIcName, "%s",  "FTE716");

	if (ucIcCode == IC_FT5442)
		sprintf(strIcName, "%s",  "FT5442");

	if (ucIcCode == IC_FT3428U)
		sprintf(strIcName, "%s",  "FT3428U");

	if (ucIcCode == IC_FT8006M)
		sprintf(strIcName, "%s",  "FT8006M");

	if (ucIcCode == IC_FT8201)
		sprintf(strIcName, "%s",  "FT8201");

	if (ucIcCode == IC_FTE736)
		sprintf(strIcName, "%s",  "FTE736");

	if (ucIcCode == IC_FT8716U)
		sprintf(strIcName, "%s",  "FT8716U");

	if (ucIcCode == IC_FT8607U)
		sprintf(strIcName, "%s",  "FT8607U");

	if (ucIcCode == IC_FT8613)
		sprintf(strIcName, "%s",  "FT8613");

	if (ucIcCode == IC_FT7311)
		sprintf(strIcName, "%s",  "FT7311");

	if (ucIcCode == IC_FT3517U)
		sprintf(strIcName, "%s",  "FT3517U");

	if (ucIcCode == IC_FT8006U)
		sprintf(strIcName, "%s",  "FT8006U");

	return ;
}

int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile)
{
	char value[512] = {0};
	int len = 0;

	if (NULL == returnValue) {
		FTS_TEST_DBG("return Value==NULL");
		return 0;
	}
	if (ini_get_key(IniFile, section, ItemName, value) < 0) {
		if (NULL != defaultvalue)
			memcpy(value, defaultvalue, strlen(defaultvalue));
		sprintf(returnValue, "%s", value);
		return 0;
	} else {
		len = sprintf(returnValue, "%s", value);
	}

	return len;
}

int fts_test_get_ini_size(char *config_name)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	off_t fsize = 0;
	char filepath[128];

	FTS_TEST_FUNC_ENTER();

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	fsize = inode->i_size;
	filp_close(pfile, NULL);

	FTS_TEST_FUNC_ENTER();

	return fsize;
}

/*Read configuration to memory*/
int fts_test_read_ini_data(char *config_name, char *config_buf)
{
	struct file *pfile = NULL;
	struct inode *inode = NULL;
	off_t fsize = 0;
	char filepath[128];
	loff_t pos = 0;
	mm_segment_t old_fs;

	FTS_TEST_FUNC_ENTER();

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s%s", FTS_INI_FILE_PATH, config_name);
	if (NULL == pfile) {
		pfile = filp_open(filepath, O_RDONLY, 0);
	}
	if (IS_ERR(pfile)) {
		FTS_TEST_ERROR("error occured while opening file %s.",  filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, config_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	FTS_TEST_FUNC_EXIT();
	return 0;
}

void init_interface(char *ini)
{
	char str[INI_ITEM_VAL_LEN_MAX] = {0};

	FTS_TEST_FUNC_ENTER();

	/*IC_Type*/
	GetPrivateProfileString("Interface", "IC_Type", "FT5X36", str, ini);
	test_data.screen_param.selected_ic = fts_ic_table_get_ic_code_from_ic_name(str);
	FTS_TEST_INFO(" IC code :0x%02x. ", test_data.screen_param.selected_ic);
	/*Normalize Type*/
	GetPrivateProfileString("Interface", "Normalize_Type" , 0 , str, ini);
	test_data.screen_param.normalize = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
}

/************************************************************************
* Name: set_param_data
* Brief:  load Config. Set IC series, init test items, init basic threshold, int detailThreshold, and set order of test items
* Input: TestParamData, from ini file.
* Output: none
* Return: 0. No sense, just according to the old format.
***********************************************************************/
int set_param_data(char *test_param)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	/* Get functin pointer */
	test_data.func = &test_func;

	test_data.ini_data = (struct _ini_data *)fts_malloc(sizeof(struct _ini_data) * MAX_KEYWORD_NUM);
	if (NULL == test_data.ini_data) {
		FTS_ERROR("mallock memory for ini_data fail");
		goto SET_PARAM_ERR;
	}

	ret = ini_get_key_data(test_param);
	if (ret < 0) {
		FTS_TEST_ERROR("ini_get_key_data error.");
		return ret;
	}

	/* Read the selected chip from the configuration */
	init_interface(test_param);

	/*Get IC Name*/
	fts_ic_table_get_ic_name_from_ic_code(test_data.screen_param.selected_ic, test_data.ini_ic_name);

	/* test configuration*/
	if (test_data.func->init_testitem) {
		test_data.func->init_testitem(test_param);
	}
	if (test_data.func->init_basicthreshold) {
		test_data.func->init_basicthreshold(test_param);
	}

	if (test_data.func->init_detailthreshold)
		test_data.func->init_detailthreshold(test_param);

	if (test_data.func->set_testitem_sequence) {
		test_data.func->set_testitem_sequence();
	}

	if (test_data.ini_data) {
		fts_free(test_data.ini_data);
	}
	FTS_TEST_FUNC_EXIT();
	return 0;

SET_PARAM_ERR:
	if (test_data.ini_data) {
		fts_free(test_data.ini_data);
	}
	return ret;
}

/*
 * fts_test_get_testparam_from_ini - get test parameters from ini
 *
 * read, parse the configuration file, initialize the test variable
 *
 * return 0 if succuss, else errro code
 */
int fts_test_get_testparam_from_ini(char *config_name)
{
	int ret = 0;
	char *ini_file_data = NULL;
	int inisize = 0;

	FTS_TEST_FUNC_ENTER();

	inisize = fts_test_get_ini_size(config_name);
	FTS_TEST_DBG("ini_size = %d ", inisize);
	if (inisize <= 0) {
		FTS_TEST_ERROR("%s ERROR:Get firmware size failed",  __func__);
		return -EIO;
	}

	ini_file_data = fts_malloc(inisize + 1); /* 1: end mark*/
	if (NULL == ini_file_data) {
		FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		return -ENOMEM;
	}
	memset(ini_file_data, 0, inisize + 1);

	ret = fts_test_read_ini_data(config_name, ini_file_data);
	if (ret) {
		FTS_TEST_ERROR(" - ERROR: fts_test_read_ini_data failed");
		goto GET_INI_DATA_ERR;
	} else {
		FTS_TEST_DBG("fts_test_read_ini_data successful");
	}

	ret = set_param_data(ini_file_data);
	if (ret) {
		FTS_TEST_ERROR("set param data fail");
		goto GET_INI_DATA_ERR;
	}

	if (ini_file_data) {
		fts_free(ini_file_data);
	}
	FTS_TEST_FUNC_EXIT();
	return 0;

GET_INI_DATA_ERR:
	if (ini_file_data) {
		fts_free(ini_file_data);
	}
	return ret;
}
