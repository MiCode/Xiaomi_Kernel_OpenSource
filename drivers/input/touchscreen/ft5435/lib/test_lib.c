/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_lib.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test entry for all IC
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "test_lib.h"
#include "Global.h"
#include "Config_FT8606.h"
#include "Test_FT8606.h"
#include "Config_FT5X46.h"
#include "Test_FT5X46.h"
#include "Config_FT5822.h"
#include "Test_FT5822.h"

#include "Config_FT6X36.h"
#include "Test_FT6X36.h"

#include "Config_FT3C47.h"
#include "Test_FT3C47.h"

#include "ini.h"

#define FTS_DRIVER_LIB_INFO  "Test_Lib_Version  V1.4.0 2015-12-03"

FTS_I2C_READ_FUNCTION fts_i2c_read_test;
FTS_I2C_WRITE_FUNCTION fts_i2c_write_test;

char *g_testparamstring = NULL;

int init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read)
{
	fts_i2c_read_test = fpI2C_Read;
	return 0;
}

int init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write)
{
	fts_i2c_write_test = fpI2C_Write;
	return 0;
}

/************************************************************************
* Name: set_param_data
* Brief:  load Config. Set IC series, init test items, init basic threshold, int detailThreshold, and set order of test items
* Input: TestParamData, from ini file.
* Output: none
* Return: 0. No sense, just according to the old format.
***********************************************************************/
int set_param_data(char *TestParamData)
{


	printk("Enter  set_param_data \n");
	g_testparamstring = TestParamData;
	ini_get_key_data(g_testparamstring);


	OnInit_InterfaceCfg(g_testparamstring);


	get_ic_name(g_ScreenSetParam.iSelectedIC, g_strIcName);

	if (IC_FT5X46>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		OnInit_FT5X22_TestItem(g_testparamstring);
		OnInit_FT5X22_BasicThreshold(g_testparamstring);
		OnInit_MCap_DetailThreshold(g_testparamstring);
		SetTestItem_FT5X22();
	} else if (IC_FT8606>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		OnInit_FT8606_TestItem(g_testparamstring);
		OnInit_FT8606_BasicThreshold(g_testparamstring);
		OnInit_MCap_DetailThreshold(g_testparamstring);
		SetTestItem_FT8606();
	} else if (IC_FT5822>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		OnInit_FT5822_TestItem(g_testparamstring);
		OnInit_FT5822_BasicThreshold(g_testparamstring);
		OnInit_MCap_DetailThreshold(g_testparamstring);
		SetTestItem_FT5822();
	} else if (IC_FT6X36>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		OnInit_FT6X36_TestItem(g_testparamstring);
		OnInit_FT6X36_BasicThreshold(g_testparamstring);
		OnInit_SCap_DetailThreshold(g_testparamstring);
		SetTestItem_FT6X36();
	} else if (IC_FT3C47U>>4 == g_ScreenSetParam.iSelectedIC>>4) {
		OnInit_FT3C47_TestItem(g_testparamstring);
		OnInit_FT3C47_BasicThreshold(g_testparamstring);
		OnInit_MCap_DetailThreshold(g_testparamstring);
		SetTestItem_FT3C47();
	}

	return 0;
}

/************************************************************************
* Name: start_test_tp
* Brief:  Test entry. Select test items based on IC series
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/

boolean start_test_tp(void)
{
	boolean bTestResult = false;
	printk("[focal] %s \n", FTS_DRIVER_LIB_INFO);
	printk("[focal] %s start \n", __func__);
	printk("IC_%s Test\n", g_strIcName);

	switch (g_ScreenSetParam.iSelectedIC>>4) {
	case IC_FT8606>>4:
		bTestResult = FT8606_StartTest();
		break;
	case IC_FT5X46>>4:
		bTestResult = FT5X46_StartTest();
		break;
	case IC_FT5822>>4:
		bTestResult = FT5822_StartTest();
		break;
	case IC_FT6X36>>4:
		bTestResult = FT6X36_StartTest();
		break;
	case IC_FT3C47U>>4:
		bTestResult = FT3C47_StartTest();
		break;
	default:
		printk("[focal]  Error IC, IC Name: %s, IC Code:  %d\n", g_strIcName, g_ScreenSetParam.iSelectedIC);
		break;
	}

	EnterWork();
	return bTestResult;
}
/************************************************************************
* Name: get_test_data
* Brief:  Get test data based on IC series
* Input: none
* Output: pTestData, External application for memory, buff size >= 1024*8
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int get_test_data(char *pTestData)
{
	int iLen = 0;
	printk("[focal] %s start \n", __func__);
	switch (g_ScreenSetParam.iSelectedIC>>4) {
	case IC_FT8606>>4:
		iLen = FT8606_get_test_data(pTestData);
		break;
	case IC_FT5X46>>4:
		iLen = FT5X46_get_test_data(pTestData);
		break;
	case IC_FT5822>>4:
		iLen = FT5822_get_test_data(pTestData);
		break;
	case IC_FT6X36>>4:
		iLen = FT6X36_get_test_data(pTestData);
		break;
	case IC_FT3C47U>>4:
		iLen = FT3C47_get_test_data(pTestData);
		break;
	default:
		printk("[focal]  Error IC, IC Name: %s, IC Code:  %d\n", g_strIcName, g_ScreenSetParam.iSelectedIC);
		break;
	}


	return iLen;
}
/************************************************************************
* Name: free_test_param_data
* Brief:  release printer memory
* Input: none
* Output: none
* Return: none.
***********************************************************************/
void free_test_param_data(void)
{
	if (g_testparamstring)


	g_testparamstring = NULL;
}

/************************************************************************
* Name: show_lib_ver
* Brief:  get lib version
* Input: none
* Output: pLibVer
* Return: the length of lib version.
***********************************************************************/
int show_lib_ver(char *pLibVer)
{
	int num_read_chars = 0;

	num_read_chars = snprintf(pLibVer, 128, "%s \n", FTS_DRIVER_LIB_INFO);

	return num_read_chars;
}


