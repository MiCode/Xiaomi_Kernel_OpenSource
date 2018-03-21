/************************************************************************
* Copyright (C) 2012-2016, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_main.c
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: test entry for all IC
*
************************************************************************/
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/slab.h>



#include "../include/focaltech_test_main.h"
#include "../focaltech_test_config.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../include/focaltech_test_ini.h"
#include "../../focaltech_global/focaltech_ic_table.h"


#define FTS_TEST_STORE_DATA_SIZE        80*1024

FTS_I2C_READ_FUNCTION fts_i2c_read_test;
FTS_I2C_WRITE_FUNCTION fts_i2c_write_test;

char *g_testparamstring = NULL;

struct StTestFuncs g_stTestFuncs;


int init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read)
{
	unsigned char value = 0;
	unsigned char recode = 0;

	FTS_TEST_FUNC_ENTER();

	fts_i2c_read_test = fpI2C_Read;
	if (NULL == fts_i2c_read_test) {
		FTS_TEST_DBG("[focal] %s fts_i2c_read_test == NULL ",  __func__);
	}


	recode = ReadReg(0xa6, &value);
	if (recode != ERROR_CODE_OK) {
		FTS_TEST_ERROR("[focal] ReadReg Error, code: %d ",  recode);
	} else {
		FTS_TEST_DBG("[focal] ReadReg successed, Addr: 0xa6, value: 0x%02x ",  value);
	}


	FTS_TEST_FUNC_EXIT();
	return 0;
}

int init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write)
{
	FTS_TEST_FUNC_ENTER();

	fts_i2c_write_test = fpI2C_Write;
	if (NULL == fts_i2c_write_test) {
		FTS_TEST_ERROR("[focal] fts_i2c_read_test == NULL ");
	}

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
int set_param_data(char *TestParamData)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	g_testparamstring = TestParamData;

	ret = ini_get_key_data(g_testparamstring);
	if (ret < 0) {
		FTS_TEST_ERROR("ini_get_key_data error.");
		return ret;
	}



	OnInit_InterfaceCfg(g_testparamstring);

	/*Get IC Name*/
	fts_ic_table_get_ic_name_from_ic_code(g_ScreenSetParam.iSelectedIC, g_strIcName);



	if (g_stTestFuncs.OnInit_TestItem) {
		g_stTestFuncs.OnInit_TestItem(g_testparamstring);

	}

	if (g_stTestFuncs.OnInit_BasicThreshold) {
		g_stTestFuncs.OnInit_BasicThreshold(g_testparamstring);


	}

	if (IC_Capacitance_Type == Self_Capacitance) {
		OnInit_SCap_DetailThreshold(g_testparamstring);
	} else if (IC_Capacitance_Type == Mutual_Capacitance) {
		OnInit_MCap_DetailThreshold(g_testparamstring);
	} else if (IC_Capacitance_Type == IDC_Capacitance) {
		OnInit_Incell_DetailThreshold(g_testparamstring);
	}

	if (g_stTestFuncs.SetTestItem) {
		g_stTestFuncs.SetTestItem();
	}

	FTS_TEST_FUNC_EXIT();

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

	FTS_TEST_FUNC_ENTER();

	FTS_TEST_DBG("IC_%s Test",  g_strIcName);

	if (g_stTestFuncs.Start_Test) {

		bTestResult = g_stTestFuncs.Start_Test();

	} else {
		FTS_TEST_DBG("[Focal]Start_Test func null!\n");
		bTestResult = false;
	}

	EnterWork();

	FTS_TEST_FUNC_EXIT();

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
	FTS_TEST_DBG("[focal] %s start ",  __func__);


	if (g_stTestFuncs.Get_test_data) {

		iLen = g_stTestFuncs.Get_test_data(pTestData);

	} else {
		FTS_TEST_DBG("[Focal]Get_test_data func null!\n");
	}

	FTS_TEST_FUNC_EXIT();
	return iLen;
}

int focaltech_test_main_init(void)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	/*Allocate memory, storage test results*/
	g_pStoreAllData = NULL;
	if (NULL == g_pStoreAllData)
		g_pStoreAllData = fts_malloc(FTS_TEST_STORE_DATA_SIZE);
	if (NULL == g_pStoreAllData)
		return -EPERM;

	/*  Allocate memory,  assigned to detail threshold structure*/
	ret = malloc_struct_DetailThreshold();

	FTS_TEST_FUNC_EXIT();

	if (ret < 0)
		return ret;

	return 0;
}
/************************************************************************
* Name: free_test_param_data
* Brief:  release printer memory
* Input: none
* Output: none
* Return: none.
***********************************************************************/
int focaltech_test_main_exit(void)
{

	FTS_TEST_DBG("[focal] release memory -start.");

	TestResultLen = 0;

	/* Release memory test results */
	if (NULL != g_pStoreAllData) {
		FTS_TEST_DBG("[FTS] release memory g_pStoreAllData.");
		fts_free(g_pStoreAllData);
		g_pStoreAllData = NULL;
	}


	if (NULL != TestResult) {
		fts_free(TestResult);
		TestResult = NULL;
	}


	/* Releasing the memory of the detailed threshold structure */
	FTS_TEST_DBG("[FTS] release memory  free_struct_DetailThreshold.");
	free_struct_DetailThreshold();


	/* release memory of key data for ini file */
	release_key_data();
	FTS_TEST_DBG("[focal] release memory -end.");
	return 0;
}

