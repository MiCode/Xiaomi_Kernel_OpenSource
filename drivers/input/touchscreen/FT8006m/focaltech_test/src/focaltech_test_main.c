/************************************************************************
* Copyright (C) 2010-2017, Focaltech Systems (R)£¬All Rights Reserved.
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

#include "../../focaltech_core.h"
#include "../include/focaltech_test_main.h"
#include "../focaltech_test_config.h"
#include "../include/focaltech_test_supported_ic.h"
#include "../include/focaltech_test_ini.h"
#include "../include/focaltech_ic_table.h"


#define FTS_TEST_STORE_DATA_SIZE        80*1024



FTS_I2C_READ_FUNCTION ft8006m_i2c_read_test;
FTS_I2C_WRITE_FUNCTION ft8006m_i2c_write_test;

char *ft8006m_g_testparamstring = NULL;

struct StTestFuncs ft8006m_g_stTestFuncs;


int ft8006m_init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read)
{
	unsigned char value = 0;
	unsigned char recode = 0;

	FTS_TEST_FUNC_ENTER();

	ft8006m_i2c_read_test = fpI2C_Read;
	if (NULL == ft8006m_i2c_read_test) {
		FTS_TEST_DBG("[focal] %s ft8006m_i2c_read_test == NULL ",  __func__);
	}


	recode = Ft8006m_ReadReg(0xa6, &value);
	if (recode != ERROR_CODE_OK) {
		FTS_TEST_ERROR("[focal] Ft8006m_ReadReg Error, code: %d ",  recode);
	} else {
		FTS_TEST_DBG("[focal] Ft8006m_ReadReg successed, Addr: 0xa6, value: 0x%02x ",  value);
	}


	FTS_TEST_FUNC_EXIT();
	return 0;
}

int ft8006m_init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_i2c_write_test = fpI2C_Write;
	if (NULL == ft8006m_i2c_write_test) {
		FTS_TEST_ERROR("[focal] ft8006m_i2c_read_test == NULL ");
	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}



/************************************************************************
* Name: ft8006m_set_param_data
* Brief:  load Config. Set IC series, init test items, init basic threshold, int detailThreshold, and set order of test items
* Input: TestParamData, from ini file.
* Output: none
* Return: 0. No sense, just according to the old format.
***********************************************************************/
int ft8006m_set_param_data(char *TestParamData)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	ft8006m_g_testparamstring = TestParamData;
	ret = ft8006m_ini_get_key_data(ft8006m_g_testparamstring);
	if (ret < 0) {
		FTS_TEST_ERROR("ft8006m_ini_get_key_data error.");
		return ret;
	}



	Ft8006m_OnInit_InterfaceCfg(ft8006m_g_testparamstring);

	/*Get IC Name*/
	ft8006m_ic_table_get_ic_name_from_ic_code(ft8006m_g_ScreenSetParam.iSelectedIC, ft8006m_g_strIcName);



	if (ft8006m_g_stTestFuncs.OnInit_TestItem) {
		ft8006m_g_stTestFuncs.OnInit_TestItem(ft8006m_g_testparamstring);

	}

	if (ft8006m_g_stTestFuncs.OnInit_BasicThreshold) {
		ft8006m_g_stTestFuncs.OnInit_BasicThreshold(ft8006m_g_testparamstring);


	}

	if (IC_Capacitance_Type == Self_Capacitance) {
		Ft8006m_OnInit_SCap_DetailThreshold(ft8006m_g_testparamstring);
	} else if (IC_Capacitance_Type == Mutual_Capacitance) {
		Ft8006m_OnInit_MCap_DetailThreshold(ft8006m_g_testparamstring);
	} else if (IC_Capacitance_Type == IDC_Capacitance) {
		Ft8006m_OnInit_Incell_DetailThreshold(ft8006m_g_testparamstring);
	}

	if (ft8006m_g_stTestFuncs.SetTestItem) {

		ft8006m_g_stTestFuncs.SetTestItem();

	}

	FTS_TEST_FUNC_EXIT();
	return 0;
}

/************************************************************************
* Name: ft8006m_start_test_tp
* Brief:  Test entry. Select test items based on IC series
* Input: none
* Output: none
* Return: Test Result, PASS or FAIL
***********************************************************************/

boolean ft8006m_start_test_tp(void)
{
	boolean bFt8006m_TestResult = false;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_DBG("IC_%s Test",  ft8006m_g_strIcName);

	if (ft8006m_g_stTestFuncs.Start_Test) {

		bFt8006m_TestResult = ft8006m_g_stTestFuncs.Start_Test();

	} else {
		FTS_TEST_DBG("[Focal]Start_Test func null!\n");
		bFt8006m_TestResult = false;
	}

	Ft8006m_EnterWork();

	FTS_TEST_FUNC_EXIT();

	return bFt8006m_TestResult;
}
/************************************************************************
* Name: m_get_test_data
* Brief:  Get test data based on IC series
* Input: none
* Output: pTestData, External application for memory, buff size >= 1024*8
* Return: the length of test data. if length > 0, got data;else ERR.
***********************************************************************/
int m_get_test_data(char *pTestData)
{
	int iLen = 0;
	FTS_TEST_DBG("[focal] %s start ",  __func__);


	if (ft8006m_g_stTestFuncs.Get_test_data) {

		iLen = ft8006m_g_stTestFuncs.Get_test_data(pTestData);

	} else {
		FTS_TEST_DBG("[Focal]Get_test_data func null!\n");
	}

	FTS_TEST_FUNC_EXIT();
	return iLen;
}

int ft8006m_focaltech_test_main_init(void)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();

	/*Allocate memory, storage test results*/
	ft8006m_g_pStoreAllData = NULL;
	if (NULL == ft8006m_g_pStoreAllData)
		ft8006m_g_pStoreAllData = Ft8006m_fts_malloc(FTS_TEST_STORE_DATA_SIZE);
	if (NULL == ft8006m_g_pStoreAllData)
		return -EPERM;

	/*  Allocate memory,  assigned to detail threshold structure*/
	ret = ft8006m_malloc_struct_DetailThreshold();

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
int ft8006m_focaltech_test_main_exit(void)
{

	FTS_TEST_DBG("[focal] release memory -start.");

	Ft8006m_TestResultLen = 0;


	/* Release memory test results */
	if (NULL != ft8006m_g_pStoreAllData) {
		FTS_TEST_DBG("[FTS] release memory ft8006m_g_pStoreAllData.");
		Ft8006m_fts_free(ft8006m_g_pStoreAllData);
		ft8006m_g_pStoreAllData = NULL;
	}


	if (NULL != Ft8006m_TestResult) {
		FTS_TEST_DBG(" release memory Ft8006m_TestResult.");
		Ft8006m_fts_free(Ft8006m_TestResult);
		Ft8006m_TestResult = NULL;
	}


	/* Releasing the memory of the detailed threshold structure */
	FTS_TEST_DBG("[FTS] release memory  ft8006m_free_struct_DetailThreshold.");
	ft8006m_free_struct_DetailThreshold();

	/* release memory of key data for ini file */
	ft8006m_release_key_data();
	FTS_TEST_DBG("[focal] release memory -end.");
	return 0;
}

