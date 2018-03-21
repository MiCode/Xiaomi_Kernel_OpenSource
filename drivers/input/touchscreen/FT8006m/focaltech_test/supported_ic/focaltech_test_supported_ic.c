/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
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

/*****************************************************************************
* Included header files
*****************************************************************************/


#include "../include/focaltech_test_supported_ic.h"
#include "../focaltech_test_config.h"



#if (FTS_CHIP_TEST_TYPE == FT8607_TEST)
void OnInit_FT8607_TestItem(char *strIniFile);
void OnInit_FT8607_BasicThreshold(char *strIniFile);
void SetTestItem_FT8607(void);
boolean FT8607_StartTest(void);
int FT8607_ft8006m_get_test_data(char *pTestData);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT8607_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8607_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT8607;
	ft8006m_g_stTestFuncs.Start_Test = FT8607_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT8607_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}


#elif (FTS_CHIP_TEST_TYPE == FT8716_TEST)

boolean FT8006m_StartTest(void);
int FT8006M_get_test_data(char *pTestData);

void OnInit_FT8006m_TestItem(char *strIniFile);
void OnInit_FT8006M_BasicThreshold(char *strIniFile);
void SetTestItem_FT8006m(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT8006m_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8006M_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT8006m;
	ft8006m_g_stTestFuncs.Start_Test = FT8006m_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT8006M_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT3D47_TEST)
boolean FT3D47_StartTest(void);

int FT3D47_ft8006m_get_test_data(char *pTestData);

void OnInit_FT3D47_TestItem(char *strIniFile);
void OnInit_FT3D47_BasicThreshold(char *strIniFile);
void SetTestItem_FT3D47(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT3D47_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT3D47_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT3D47;
	ft8006m_g_stTestFuncs.Start_Test = FT3D47_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT3D47_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT5X46_TEST)

boolean FT5X46_StartTest(void);
int FT5X46_ft8006m_get_test_data(char *pTestData);

void OnInit_FT5X46_TestItem(char *strIniFile);
void OnInit_FT5X46_BasicThreshold(char *strIniFile);
void SetTestItem_FT5X46(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT5X46_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT5X46_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT5X46;
	ft8006m_g_stTestFuncs.Start_Test = FT5X46_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT5X46_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT6X36_TEST)

boolean FT6X36_StartTest(void);
int FT6X36_ft8006m_get_test_data(char *pTestData);

void OnInit_FT6X36_TestItem(char *strIniFile);
void OnInit_FT6X36_BasicThreshold(char *strIniFile);
void SetTestItem_FT6X36(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT6X36_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT6X36_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT6X36;
	ft8006m_g_stTestFuncs.Start_Test = FT6X36_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT6X36_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT5822_TEST)

boolean FT5822_StartTest(void);
int FT5822_ft8006m_get_test_data(char *pTestData);

void OnInit_FT5822_TestItem(char *strIniFile);
void OnInit_FT5822_BasicThreshold(char *strIniFile);
void SetTestItem_FT5822(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT5822_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT5822_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT5822;
	ft8006m_g_stTestFuncs.Start_Test = FT5822_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT5822_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT8006_TEST)

boolean FT8006_StartTest(void);
int FT8006M_get_test_data(char *pTestData);

void OnInit_FT8006_TestItem(char *strIniFile);
void OnInit_FT8006_BasicThreshold(char *strIniFile);
void SetTestItem_FT8006(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT8006_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8006_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT8006;
	ft8006m_g_stTestFuncs.Start_Test = FT8006_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT8006M_get_test_data;

	FTS_TEST_FUNC_EXIT();
}


#elif (FTS_CHIP_TEST_TYPE == FT8736_TEST)

boolean FT8736_StartTest(void);
int FT8736_ft8006m_get_test_data(char *pTestData);

void OnInit_FT8736_TestItem(char *strIniFile);
void OnInit_FT8736_BasicThreshold(char *strIniFile);
void SetTestItem_FT8736(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT8736_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8736_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT8736;
	ft8006m_g_stTestFuncs.Start_Test = FT8736_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT8736_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FTE716_TEST)

boolean FTE716_StartTest(void);
int FTE716_ft8006m_get_test_data(char *pTestData);

void OnInit_FTE716_TestItem(char *strIniFile);
void OnInit_FTE716_BasicThreshold(char *strIniFile);
void SetTestItem_FTE716(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FTE716_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FTE716_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FTE716;
	ft8006m_g_stTestFuncs.Start_Test = FTE716_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FTE716_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT8606_TEST)

boolean FT8606_StartTest(void);
int FT8606_ft8006m_get_test_data(char *pTestData);

void OnInit_FT8606_TestItem(char *strIniFile);
void OnInit_FT8606_BasicThreshold(char *strIniFile);
void SetTestItem_FT8606(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT8606_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT8606_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT8606;
	ft8006m_g_stTestFuncs.Start_Test = FT8606_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT8606_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}

#elif (FTS_CHIP_TEST_TYPE == FT3C47_TEST)

boolean FT3C47_StartTest(void);
int FT3C47_ft8006m_get_test_data(char *pTestData);

void OnInit_FT3C47_TestItem(char *strIniFile);
void OnInit_FT3C47_BasicThreshold(char *strIniFile);
void SetTestItem_FT3C47(void);

void ft8006m_test_funcs(void)
{
	FTS_TEST_FUNC_ENTER();

	ft8006m_g_stTestFuncs.OnInit_TestItem = OnInit_FT3C47_TestItem;
	ft8006m_g_stTestFuncs.OnInit_BasicThreshold = OnInit_FT3C47_BasicThreshold;
	ft8006m_g_stTestFuncs.SetTestItem  = SetTestItem_FT3C47;
	ft8006m_g_stTestFuncs.Start_Test = FT3C47_StartTest;
	ft8006m_g_stTestFuncs.Get_test_data = FT3C47_ft8006m_get_test_data;

	FTS_TEST_FUNC_EXIT();
}
#endif


