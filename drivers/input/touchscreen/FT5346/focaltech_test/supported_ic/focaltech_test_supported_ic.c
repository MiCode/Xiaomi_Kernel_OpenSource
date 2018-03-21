/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_supported_ic.c
*
* Author: Software Development
*
* Created: 2016-08-01
*
* Abstract: test item for FT8716
*
************************************************************************/

/*******************************************************************************
* Included header files
*******************************************************************************/


#include "../include/focaltech_test_supported_ic.h"
#include "../focaltech_test_config.h"



#if (FT8607_TEST)
void OnInit_FT8607_TestItem(char *strIniFile);
void OnInit_FT8607_BasicThreshold(char *strIniFile);
void SetTestItem_FT8607(void);
boolean FT8607_StartTest(void);
int FT8607_get_test_data(char *pTestData);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT8607_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8607_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT8607;
	g_stTestFuncs.Start_Test = FT8607_StartTest;
	g_stTestFuncs.Get_test_data = FT8607_get_test_data;

	FTS_TEST_FUNC_EXIT();
}


#elif (FT8716_TEST)

boolean FT8716_StartTest(void);
int FT8716_get_test_data(char *pTestData);

void OnInit_FT8716_TestItem(char *strIniFile);
void OnInit_FT8716_BasicThreshold(char *strIniFile);
void SetTestItem_FT8716(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT8716_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8716_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT8716;
	g_stTestFuncs.Start_Test = FT8716_StartTest;
	g_stTestFuncs.Get_test_data = FT8716_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT5X46_TEST)

boolean FT5X46_StartTest(void);
int FT5X46_get_test_data(char *pTestData);

void OnInit_FT5X46_TestItem(char *strIniFile);
void OnInit_FT5X46_BasicThreshold(char *strIniFile);
void SetTestItem_FT5X46(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT5X46_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT5X46_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT5X46;
	g_stTestFuncs.Start_Test = FT5X46_StartTest;
	g_stTestFuncs.Get_test_data = FT5X46_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT6X36_TEST)

boolean FT6X36_StartTest(void);
int FT6X36_get_test_data(char *pTestData);

void OnInit_FT6X36_TestItem(char *strIniFile);
void OnInit_FT6X36_BasicThreshold(char *strIniFile);
void SetTestItem_FT6X36(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT6X36_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT6X36_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT6X36;
	g_stTestFuncs.Start_Test = FT6X36_StartTest;
	g_stTestFuncs.Get_test_data = FT6X36_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT5822_TEST)

boolean FT5822_StartTest(void);
int FT5822_get_test_data(char *pTestData);

void OnInit_FT5822_TestItem(char *strIniFile);
void OnInit_FT5822_BasicThreshold(char *strIniFile);
void SetTestItem_FT5822(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT5822_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT5822_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT5822;
	g_stTestFuncs.Start_Test = FT5822_StartTest;
	g_stTestFuncs.Get_test_data = FT5822_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT8006_TEST)

boolean FT8006_StartTest(void);
int FT8006_get_test_data(char *pTestData);

void OnInit_FT8006_TestItem(char *strIniFile);
void OnInit_FT8006_BasicThreshold(char *strIniFile);
void SetTestItem_FT8006(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT8006_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8006_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT8006;
	g_stTestFuncs.Start_Test = FT8006_StartTest;
	g_stTestFuncs.Get_test_data = FT8006_get_test_data;

	FTS_TEST_FUNC_EXIT();
}


#elif (FT8736_TEST)

boolean FT8736_StartTest(void);
int FT8736_get_test_data(char *pTestData);

void OnInit_FT8736_TestItem(char *strIniFile);
void OnInit_FT8736_BasicThreshold(char *strIniFile);
void SetTestItem_FT8736(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT8736_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8736_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT8736;
	g_stTestFuncs.Start_Test = FT8736_StartTest;
	g_stTestFuncs.Get_test_data = FT8736_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTE736_TEST)

boolean FTE736_StartTest(void);
int FTE736_get_test_data(char *pTestData);

void OnInit_FTE736_TestItem(char *strIniFile);
void OnInit_FTE736_BasicThreshold(char *strIniFile);
void SetTestItem_FTE736(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FTE736_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FTE736_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FTE736;
	g_stTestFuncs.Start_Test = FTE736_StartTest;
	g_stTestFuncs.Get_test_data = FTE736_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTE716_TEST)

boolean FTE716_StartTest(void);
int FTE716_get_test_data(char *pTestData);

void OnInit_FTE716_TestItem(char *strIniFile);
void OnInit_FTE716_BasicThreshold(char *strIniFile);
void SetTestItem_FTE716(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FTE716_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FTE716_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FTE716;
	g_stTestFuncs.Start_Test = FTE716_StartTest;
	g_stTestFuncs.Get_test_data = FTE716_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT8606_TEST)

boolean FT8606_StartTest(void);
int FT8606_get_test_data(char *pTestData);

void OnInit_FT8606_TestItem(char *strIniFile);
void OnInit_FT8606_BasicThreshold(char *strIniFile);
void SetTestItem_FT8606(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT8606_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8606_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT8606;
	g_stTestFuncs.Start_Test = FT8606_StartTest;
	g_stTestFuncs.Get_test_data = FT8606_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FT3C47_TEST)

boolean FT3C47_StartTest(void);
int FT3C47_get_test_data(char *pTestData);

void OnInit_FT3C47_TestItem(char *strIniFile);
void OnInit_FT3C47_BasicThreshold(char *strIniFile);
void SetTestItem_FT3C47(void);

void fts_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	g_stTestFuncs.OnInit_TestItem = OnInit_FT3C47_TestItem;
	g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT3C47_BasicThreshold;
	g_stTestFuncs.SetTestItem  = SetTestItem_FT3C47;
	g_stTestFuncs.Start_Test = FT3C47_StartTest;
	g_stTestFuncs.Get_test_data = FT3C47_get_test_data;

	FTS_TEST_FUNC_EXIT();
}


#endif


