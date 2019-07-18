/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
*
* File Name: focaltech_test_ini.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-01
*
* Abstract: parsing function of INI file
*
************************************************************************/
#include "focaltech_test.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
/* '[' ']':Section Symbol-Can be defined according to the special need to change */
const char CFG_SSL = '[';
const char CFG_SSR = ']';
const char CFG_NIS = ':';  /* Separator between name and index */
const char CFG_NTS = '#';  /* annotator */
const char CFG_EQS = '=';  /* The equal sign */

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
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
		if (strlen(key) == test_data.ini_data[i].key_name_len) {
			if (fts_strncmp(key, test_data.ini_data[i].key_name,  test_data.ini_data[i].key_name_len) == 0) {
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
	char tmp[MAX_CFG_BUF_LINE_LEN + 1] = { 0 };

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);
	if (len > MAX_CFG_BUF_LINE_LEN) {
		FTS_TEST_SAVE_ERR("%s:buf len(%d) fail\n", __func__, len);
		return NULL;
	}

	memset(tmp, 0x00, len);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}
	if (i < len) {
		strlcpy(tmp, (buf + i), (len - i) + 1);
	}
	strlcpy(buf, tmp, len + 1);

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
	char tmp[MAX_CFG_BUF_LINE_LEN + 1] = { 0 };

	memset(tmp, 0, sizeof(tmp));
	len = strlen(buf);
	if (len > MAX_CFG_BUF_LINE_LEN) {
		FTS_TEST_SAVE_ERR("%s:buf len(%d) fail\n", __func__, len);
		return NULL;
	}

	memset(tmp, 0x00, len);

	for (i = 0; i < len; i++) {
		if (buf[len - i - 1] != ' ')
			break;
	}
	if (i < len) {
		strlcpy(tmp, buf, len - i + 1);
	}
	strlcpy(buf, tmp, len + 1);

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
		if (ch1 == '\n' || ch1 == '\r') {
			ch1 = filedata[j + 1];
			if (ch1 == '\n' || ch1 == '\r') {
				iRetNum++;
			}

			break;
		} else if (ch1 == 0x00) {
			iRetNum = -1;
			break;
		} else {
			buffer[i++] = ch1;
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
	char buf1[MAX_CFG_BUF_LINE_LEN + 1] = {0};
	int n = 0;
	int ret = 0;
	int dataoff = 0;
	int iEqualSign = 0;
	int i = 0;
	char tmsection_name[MAX_CFG_BUF_LINE_LEN + 1] = {0};

	FTS_TEST_FUNC_ENTER();
	ret = init_key_data();
	if (ret < 0) {
		FTS_TEST_ERROR("init key data failed");
		return -EPERM;
	}

	test_data.ini_keyword_num = 0;
	while (1) { /*find section */
		ret = CFG_ERR_READ_FILE;
		n = ini_file_get_line(filedata + dataoff, buf1, MAX_CFG_BUF_LINE_LEN);
		if (n < -1)
			goto cfg_scts_end;
		if (n < 0)
			break;/* file end */
		if (n >= MAX_CFG_BUF_LINE_LEN) {
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
			goto cfg_scts_end;
		}

		if (buf1[0] == CFG_SSL) {
			test_data.ini_data[test_data.ini_keyword_num].section_name_len = n - 2;
			if (MAX_KEYWORD_NAME_LEN < test_data.ini_data[test_data.ini_keyword_num].section_name_len) {
				ret = CFG_ERR_OUT_OF_LEN;
				FTS_TEST_ERROR("MAX_KEYWORD_NAME_LEN: CFG_ERR_OUT_OF_LEN\n");
				goto cfg_scts_end;
			}
			buf1[n - 1] = 0x00;
			strlcpy((char *)tmsection_name, buf1 + 1, MAX_CFG_BUF_LINE_LEN + 1);

			continue;
		}
		strlcpy(test_data.ini_data[test_data.ini_keyword_num].section_name, tmsection_name, strlen(tmsection_name) + 1);
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

int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile)
{
	char value[MAX_KEYWORD_VALUE_LEN] = { 0 };
	int len = 0;

	if (NULL == returnValue) {
		FTS_TEST_DBG("return Value==NULL");
		return 0;
	}
	if (ini_get_key(IniFile, section, ItemName, value) < 0) {
		if (NULL != defaultvalue)
			memcpy(value, defaultvalue, strlen(defaultvalue));
		snprintf(returnValue, PAGE_SIZE, "%s", value);
		return 0;
	} else {
		len = snprintf(returnValue, PAGE_SIZE, "%s", value);
	}

	return len;
}

int fts_test_get_ini_size(char *config_name)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	if (config_name == NULL) {
		FTS_TEST_ERROR("config name is null.");
		return 0;
	}
	dev = fts_get_dev();
	if (dev == NULL) {
		FTS_TEST_ERROR("get dev error.");
		return 0;
	}
	ret = request_firmware(&fw, config_name, dev);
	if (ret == 0) {
		return fw->size;
		release_firmware(fw);
	} else {
		FTS_TEST_ERROR("request ini file error.");
		return 0;
	}

	FTS_TEST_FUNC_EXIT();
}

int fts_test_read_ini_data(char *config_name, char *config_buf)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	if (config_name == NULL) {
		FTS_TEST_ERROR("config name is null.");
		return -EINVAL;
	}
	dev = fts_get_dev();
	if (dev == NULL) {
		FTS_TEST_ERROR("get dev error.");
		return -EINVAL;
	}
	ret = request_firmware(&fw, config_name, dev);
	if (ret == 0) {
		FTS_TEST_INFO("start copy ini file.");
		memcpy(config_buf, (char *)fw->data, fw->size);
		release_firmware(fw);
		return ret;
	} else {
		FTS_TEST_ERROR("request ini file error.");
		return -EINVAL;
	}
}

int init_interface(char *ini)
{
	char str[MAX_KEYWORD_VALUE_LEN] = { 0 };
	u32 ic_type = 0xFF;

	FTS_TEST_FUNC_ENTER();

	/* IC type */
	GetPrivateProfileString("Interface", "IC_Type", "FT5X36", str, ini);
	ic_type = fts_ic_table_get_ic_code_from_ic_name(str);
	if (0 == ic_type) {
		FTS_TEST_ERROR("get ic code fail");
		return -EINVAL;
	}
	test_data.screen_param.selected_ic = ic_type;
	/*Get IC Name*/
	snprintf(test_data.ini_ic_name, PAGE_SIZE, "%s", str);
	FTS_TEST_INFO("IC Name:%s IC Code:0x%02x.", test_data.ini_ic_name, test_data.screen_param.selected_ic);

	/* Normalize Type */
	GetPrivateProfileString("Interface", "Normalize_Type", 0, str, ini);
	test_data.screen_param.normalize = fts_atoi(str);

	FTS_TEST_FUNC_EXIT();
	return 0;
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
	ret = init_interface(test_param);
	if (ret < 0) {
		FTS_TEST_ERROR("IC_Type in ini is not supported");
		return ret;
	}

	/* Get test function */
	ret = init_test_funcs(test_data.screen_param.selected_ic);
	if (ret < 0) {
		FTS_TEST_ERROR("no ic series match");
		return ret;
	}

	if (NULL == test_data.func) {
		FTS_TEST_ERROR("test function is NULL");
		return -ENODATA;
	}

	/* test configuration */
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
		FTS_TEST_ERROR("%s Get firmware size failed",  __func__);
		return -EIO;
	}

	ini_file_data = fts_malloc(inisize + 1);
	if (NULL == ini_file_data) {
		FTS_TEST_ERROR("fts_malloc failed in function:%s",  __func__);
		return -ENOMEM;
	}
	memset(ini_file_data, 0, inisize + 1);

	ret = fts_test_read_ini_data(config_name, ini_file_data);
	if (ret) {
		FTS_TEST_ERROR(" - fts_test_read_ini_data failed");
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
