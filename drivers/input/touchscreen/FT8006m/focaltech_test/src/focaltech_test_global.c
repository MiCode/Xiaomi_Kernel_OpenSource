/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_global.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: global function for test
*
************************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/focaltech_test_ini.h"
#include "../focaltech_test_config.h"
#include "../include/focaltech_test_main.h"
#include "../include/focaltech_ic_table.h"

#define DEVIDE_MODE_ADDR    0x00

/*buff length*/
#define BUFF_LEN_STORE_MSG_AREA     1024*10
#define BUFF_LEN_MSG_AREA_LINE2     1024*4
#define BUFF_LEN_STORE_DATA_AREA        1024*80
#define BUFF_LEN_TMP_BUFFER             1024*16


char *ft8006m_g_pTmpBuff = NULL;
char *ft8006m_g_pStoreMsgArea = NULL;
int ft8006m_g_lenStoreMsgArea = 0;
char *ft8006m_g_pMsgAreaLine2 = NULL;
int ft8006m_g_lenMsgAreaLine2 = 0;
char *ft8006m_g_pStoreDataArea = NULL;
int ft8006m_g_lenStoreDataArea = 0;
unsigned char ft8006m_m_ucTestItemCode = 0;
int ft8006m_m_iStartLine = 0;
int ft8006m_m_iTestDataCount = 0;

char *Ft8006m_TestResult = NULL;
int Ft8006m_TestResultLen = 0;

#define FTS_MALLOC_TYPE         1
enum enum_malloc_mode {
	kmalloc_mode = 0,
	vmalloc_mode = 1,
};

struct StruScreenSeting ft8006m_g_ScreenSetParam;
struct stTestItem ft8006m_g_stTestItem[1][MAX_TEST_ITEM];
struct structSCapConfEx ft8006m_g_stSCapConfEx;

int ft8006m_g_TestItemNum = 0;
char ft8006m_g_strIcName[20] = {0};
char *ft8006m_g_pStoreAllData = NULL;

int Ft8006m_GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile)
{
	char value[512] = {0};
	int len = 0;

	if (NULL == returnValue) {
		FTS_TEST_DBG("[FTS] returnValue==NULL in function %s.", __func__);
		return 0;
	}

	if (ft8006m_ini_get_key(IniFile, section, ItemName, value) < 0) {
		if (NULL != defaultvalue)
			memcpy(value, defaultvalue, strlen(defaultvalue));
		sprintf(returnValue, "%s", value);
		return 0;
	} else {
		len = sprintf(returnValue, "%s", value);
	}

	return len;
}

void ft8006m_focal_msleep(int ms)
{
	msleep(ms);
}

void Ft8006m_SysDelay(int ms)
{
	msleep(ms);
}

int ft8006m_focal_abs(int value)
{
	if (value < 0)
		value = 0 - value;

	return value;
}

void *Ft8006m_fts_malloc(size_t size)
{
	if (FTS_MALLOC_TYPE == kmalloc_mode) {
		return kmalloc(size, GFP_ATOMIC);
	} else if (FTS_MALLOC_TYPE == vmalloc_mode) {
		return vmalloc(size);
	} else {
		FTS_TEST_DBG("invalid malloc. \n");
		return NULL;
	}

	return NULL;
}

void Ft8006m_fts_free(void *p)
{
	if (FTS_MALLOC_TYPE == kmalloc_mode) {
		return kfree(p);
	} else if (FTS_MALLOC_TYPE == vmalloc_mode) {
		return vfree(p);
	} else {
		FTS_TEST_DBG("invalid free. \n");
		return ;
	}

	return ;
}

void Ft8006m_OnInit_InterfaceCfg(char *strIniFile)
{
	char str[128] = {0};

	FTS_TEST_FUNC_ENTER();

	Ft8006m_GetPrivateProfileString("Interface", "IC_Type", "FT5X36", str, strIniFile);
	ft8006m_g_ScreenSetParam.iSelectedIC = ft8006m_ic_table_get_ic_code_from_ic_name(str);
	FTS_TEST_INFO(" IC code :0x%02x. ", ft8006m_g_ScreenSetParam.iSelectedIC);


	Ft8006m_GetPrivateProfileString("Interface", "Normalize_Type", 0, str, strIniFile);
	ft8006m_g_ScreenSetParam.isNormalize = ft8006m_atoi(str);

	FTS_TEST_FUNC_EXIT();

}
/************************************************************************
* Name: Ft8006m_ReadReg(Same function name as FT_MultipleTest)
* Brief:  Read Register
* Input: RegAddr
* Output: RegData
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int Ft8006m_ReadReg(unsigned char RegAddr, unsigned char *RegData)
{
	int iRet;

	if (NULL == ft8006m_i2c_read_test) {
		FTS_TEST_DBG("[focal] %s ft8006m_i2c_read_test == NULL  !!! ", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
	}

	iRet = ft8006m_i2c_read_test(&RegAddr, 1, RegData, 1);

	if (iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}

/************************************************************************
* Name: Ft8006m_WriteReg(Same function name as FT_MultipleTest)
* Brief:  Write Register
* Input: RegAddr, RegData
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
int Ft8006m_WriteReg(unsigned char RegAddr, unsigned char RegData)
{
	int iRet;
	unsigned char cmd[2] = {0};

	if (NULL == ft8006m_i2c_write_test) {
		FTS_TEST_DBG("[focal] %s ft8006m_i2c_write_test == NULL  !!!", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
	}

	cmd[0] = RegAddr;
	cmd[1] = RegData;
	iRet = ft8006m_i2c_write_test(cmd, 2);

	if (iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}
/************************************************************************
* Name: Ft8006m_Comm_Base_IIC_IO(Same function name as FT_MultipleTest)
* Brief:  Write/Read Data by IIC
* Input: pWriteBuffer, iBytesToWrite, iBytesToRead
* Output: pReadBuffer
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char Ft8006m_Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int  iBytesToWrite, unsigned char *pReadBuffer, int iBytesToRead)
{
	int iRet;

	if (NULL == ft8006m_i2c_read_test) {
		FTS_TEST_DBG("[focal] %s ft8006m_i2c_read_test == NULL  !!! ", __func__);
		return (ERROR_CODE_INVALID_COMMAND);
	}

	if (iBytesToRead > 0) {
		iRet = ft8006m_i2c_read_test(pWriteBuffer, iBytesToWrite, pReadBuffer, iBytesToRead);
	} else {
		iRet = ft8006m_i2c_write_test(pWriteBuffer, iBytesToWrite);
	}

	if (iRet >= 0)
		return (ERROR_CODE_OK);
	else
		return (ERROR_CODE_COMM_ERROR);
}
/************************************************************************
* Name: Ft8006m_EnterWork(Same function name as FT_MultipleTest)
* Brief:  Enter Work Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0x00 is OK, else fail.
***********************************************************************/
unsigned char Ft8006m_EnterWork(void)
{
	unsigned char RunState = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	FTS_TEST_FUNC_ENTER();
	ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if (ReCode == ERROR_CODE_OK) {
		if (((RunState>>4)&0x07) == 0x00) {
			ReCode = ERROR_CODE_OK;
		} else {
			ReCode = Ft8006m_WriteReg(DEVIDE_MODE_ADDR, 0);
			if (ReCode == ERROR_CODE_OK) {
				ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RunState);
				if (ReCode == ERROR_CODE_OK) {
					if (((RunState>>4)&0x07) == 0x00) {
						ReCode = ERROR_CODE_OK;
					} else {
						ReCode = ERROR_CODE_COMM_ERROR;
					}
				} else
					FTS_TEST_ERROR("Ft8006m_EnterWork read DEVIDE_MODE_ADDR error 3.");
			} else
				FTS_TEST_ERROR("Ft8006m_EnterWork write DEVIDE_MODE_ADDR error 2.");
		}
	} else
		FTS_TEST_ERROR("Ft8006m_EnterWork read DEVIDE_MODE_ADDR error 1.");

	FTS_TEST_FUNC_EXIT();

	return ReCode;

}
/************************************************************************
* Name: Ft8006m_EnterFactory
* Brief:  enter Fcatory Mode
* Input: null
* Output: null
* Return: Comm Code. Code = 0 is OK, else fail.
***********************************************************************/
unsigned char Ft8006m_EnterFactory(void)
{
	unsigned char RunState = 0;
	int index = 0;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	FTS_TEST_FUNC_ENTER();
	ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RunState);
	if (ReCode == ERROR_CODE_OK) {
		if (((RunState>>4)&0x07) == 0x04) {
			ReCode = ERROR_CODE_OK;
		} else {
			ReCode = Ft8006m_WriteReg(DEVIDE_MODE_ADDR, 0x40);
			if (ReCode == ERROR_CODE_OK) {
				for (index = 0; index < 20; ++index) {
					ReCode = Ft8006m_ReadReg(DEVIDE_MODE_ADDR, &RunState);
					if (ReCode == ERROR_CODE_OK) {
						if (((RunState>>4)&0x07) == 0x04) {
							ReCode = ERROR_CODE_OK;
							break;
						} else {
							ReCode = ERROR_CODE_COMM_ERROR;
						}
					}
					Ft8006m_SysDelay(50);
				}
				if (ReCode != ERROR_CODE_OK)
					FTS_TEST_ERROR("Ft8006m_EnterFactory read DEVIDE_MODE_ADDR error 3.");
			} else
				FTS_TEST_ERROR("Ft8006m_EnterFactory write DEVIDE_MODE_ADDR error 2.");
		}
	} else
		FTS_TEST_ERROR("Ft8006m_EnterFactory read DEVIDE_MODE_ADDR error 1.");

	FTS_TEST_FUNC_EXIT();
	return ReCode;
}

/************************************************************************
* Name: ft8006m_SetTestItemCodeName
* Brief:  set test item code and name
* Input: null
* Output: null
* Return:
**********************************************************************/

void ft8006m_SetTestItemCodeName(unsigned char ucitemcode)
{
	ft8006m_g_stTestItem[0][ft8006m_g_TestItemNum].ItemCode = ucitemcode;

	ft8006m_g_stTestItem[0][ft8006m_g_TestItemNum].TestNum = ft8006m_g_TestItemNum;
	ft8006m_g_stTestItem[0][ft8006m_g_TestItemNum].TestResult = RESULT_NULL;
	ft8006m_g_TestItemNum++;
}

/************************************************************************
* Name: Ft8006m_InitTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
int Ft8006m_InitTest(void)
{
	int ret = 0;
	ret = Ft8006m_AllocateMemory();
	if (ret < 0)
		return -EPERM;

	Ft8006m_InitStoreParamOfTestData();

	ft8006m_g_stSCapConfEx.ChannelXNum = 0;
	ft8006m_g_stSCapConfEx.ChannelYNum = 0;
	ft8006m_g_stSCapConfEx.KeyNum = 0;
	ft8006m_g_stSCapConfEx.KeyNumTotal = 6;

	return 0;

}

/************************************************************************
* Name: Ft8006m_FinishTest
* Brief:  Init all param before test
* Input: none
* Output: none
* Return: none
***********************************************************************/
void Ft8006m_FinishTest(void)
{
	Ft8006m_MergeAllTestData();
	Ft8006m_FreeMemory();
}


/************************************************************************
* Name: Ft8006m_InitStoreParamOfTestData
* Brief:  Init store param of test data
* Input: none
* Output: none
* Return: none
***********************************************************************/
void Ft8006m_InitStoreParamOfTestData(void)
{
	ft8006m_g_lenStoreMsgArea = 0;

	ft8006m_g_lenStoreMsgArea += sprintf(ft8006m_g_pStoreMsgArea, "ECC, 85, 170, IC Name, %s, IC Code, %x\n",  ft8006m_g_strIcName,  ft8006m_g_ScreenSetParam.iSelectedIC);



	ft8006m_g_lenMsgAreaLine2 = 0;



	ft8006m_g_lenStoreDataArea = 0;
	ft8006m_m_iStartLine = 11;

	ft8006m_m_iTestDataCount = 0;
}
/************************************************************************
* Name: Ft8006m_MergeAllTestData
* Brief:  Merge All Data of test result
* Input: none
* Output: none
* Return: none
***********************************************************************/
void Ft8006m_MergeAllTestData(void)
{
	int iLen = 0;


	iLen = sprintf(ft8006m_g_pTmpBuff, "TestItem, %d, ", ft8006m_m_iTestDataCount);
	memcpy(ft8006m_g_pStoreMsgArea+ft8006m_g_lenStoreMsgArea, ft8006m_g_pTmpBuff, iLen);
	ft8006m_g_lenStoreMsgArea += iLen;


	memcpy(ft8006m_g_pStoreMsgArea+ft8006m_g_lenStoreMsgArea, ft8006m_g_pMsgAreaLine2, ft8006m_g_lenMsgAreaLine2);
	ft8006m_g_lenStoreMsgArea += ft8006m_g_lenMsgAreaLine2;


	iLen = sprintf(ft8006m_g_pTmpBuff, "\n\n\n\n\n\n\n\n\n");
	memcpy(ft8006m_g_pStoreMsgArea+ft8006m_g_lenStoreMsgArea, ft8006m_g_pTmpBuff, iLen);
	ft8006m_g_lenStoreMsgArea += iLen;


	memcpy(ft8006m_g_pStoreAllData, ft8006m_g_pStoreMsgArea, ft8006m_g_lenStoreMsgArea);


	if (0 != ft8006m_g_lenStoreDataArea) {
		memcpy(ft8006m_g_pStoreAllData+ft8006m_g_lenStoreMsgArea, ft8006m_g_pStoreDataArea, ft8006m_g_lenStoreDataArea);
	}

	FTS_TEST_DBG("lenStoreMsgArea=%d,  lenStoreDataArea = %d",  ft8006m_g_lenStoreMsgArea, ft8006m_g_lenStoreDataArea);
}



/************************************************************************
* Name: Ft8006m_AllocateMemory
* Brief:  Allocate pointer Memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
int Ft8006m_AllocateMemory(void)
{

	ft8006m_g_pStoreMsgArea = NULL;
	if (NULL == ft8006m_g_pStoreMsgArea)
		ft8006m_g_pStoreMsgArea = Ft8006m_fts_malloc(BUFF_LEN_STORE_MSG_AREA);
	if (NULL == ft8006m_g_pStoreMsgArea)
		goto ERR;

	ft8006m_g_pMsgAreaLine2 = NULL;
	if (NULL == ft8006m_g_pMsgAreaLine2)
		ft8006m_g_pMsgAreaLine2 = Ft8006m_fts_malloc(BUFF_LEN_MSG_AREA_LINE2);
	if (NULL == ft8006m_g_pMsgAreaLine2)
		goto ERR;

	ft8006m_g_pStoreDataArea = NULL;
	if (NULL == ft8006m_g_pStoreDataArea)
		ft8006m_g_pStoreDataArea = Ft8006m_fts_malloc(BUFF_LEN_STORE_DATA_AREA);
	if (NULL == ft8006m_g_pStoreDataArea)
		goto ERR;

	ft8006m_g_pTmpBuff = NULL;
	if (NULL == ft8006m_g_pTmpBuff)
		ft8006m_g_pTmpBuff = Ft8006m_fts_malloc(BUFF_LEN_TMP_BUFFER);
	if (NULL == ft8006m_g_pTmpBuff)
		goto ERR;

	Ft8006m_TestResult = NULL;
	if (NULL == Ft8006m_TestResult)
		Ft8006m_TestResult = Ft8006m_fts_malloc(BUFF_LEN_TMP_BUFFER);
	if (NULL == Ft8006m_TestResult)
		goto ERR;

	return 0;

ERR:
	FTS_TEST_ERROR("Ft8006m_fts_malloc memory failed in function.");
	return -EPERM;

}

/************************************************************************
* Name: Ft8006m_FreeMemory
* Brief:  Release pointer memory
* Input: none
* Output: none
* Return: none
***********************************************************************/
void Ft8006m_FreeMemory(void)
{

	if (NULL != ft8006m_g_pStoreMsgArea)
		Ft8006m_fts_free(ft8006m_g_pStoreMsgArea);

	if (NULL != ft8006m_g_pMsgAreaLine2)
		Ft8006m_fts_free(ft8006m_g_pMsgAreaLine2);

	if (NULL != ft8006m_g_pStoreDataArea)
		Ft8006m_fts_free(ft8006m_g_pStoreDataArea);

	if (NULL != ft8006m_g_pTmpBuff)
		Ft8006m_fts_free(ft8006m_g_pTmpBuff);
}
