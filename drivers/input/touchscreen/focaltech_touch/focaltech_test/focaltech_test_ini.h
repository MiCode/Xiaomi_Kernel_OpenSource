/************************************************************************
* Copyright (C) 2012-2018, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
*
* File Name: focaltech_test_ini.h
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-01
*
* Abstract: parsing function of INI file
*
************************************************************************/
#ifndef _INI_H
#define _INI_H
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define MAX_KEYWORD_NUM						500
#define MAX_KEYWORD_NAME_LEN					50
#define MAX_KEYWORD_VALUE_LEN				   512
#define MAX_CFG_BUF_LINE_LEN	  (MAX_KEYWORD_NAME_LEN + MAX_KEYWORD_VALUE_LEN)

#define SUCCESS								 0

#define CFG_ERR_OPEN_FILE					   -10
#define CFG_ERR_CREATE_FILE					 -11
#define CFG_ERR_READ_FILE					   -12
#define CFG_ERR_WRITE_FILE					  -13
#define CFG_ERR_FILE_FORMAT					 -14
#define CFG_ERR_TOO_MANY_KEY_NUM				-15
#define CFG_ERR_OUT_OF_LEN					  -16

/*****************************************************************************
* enumerations, structures and unions
*****************************************************************************/
struct _ini_data {
	char section_name[MAX_KEYWORD_NAME_LEN];
	char key_name[MAX_KEYWORD_NAME_LEN];
	char key_value[MAX_KEYWORD_VALUE_LEN];
	int section_name_len;
	int key_name_len;
	int key_value_len;
};

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
int fts_atoi(char *nptr);
char *ini_str_trim_r(char *buf);
char *ini_str_trim_l(char *buf);
int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile);
int fts_test_get_testparam_from_ini(char *config_name);
int fts_test_get_ini_size(char *config_name);
int fts_test_read_ini_data(char *config_name, char *config_buf);

#endif /* _INI_H */
