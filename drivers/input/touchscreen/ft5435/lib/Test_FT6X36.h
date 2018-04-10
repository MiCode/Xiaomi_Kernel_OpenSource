/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT6X36.h
*
* Author: Software Development Team, AE
*
* Created: 2015-10-08
*
* Abstract: test item for FT6X36/FT3X07/FT6416/FT6426
*
************************************************************************/
#ifndef _TEST_FT6X36_H
#define _TEST_FT6X36_H

#include "test_lib.h"

boolean FT6X36_StartTest(void);
int FT6X36_get_test_data(char *pTestData);

unsigned char FT6X36_TestItem_EnterFactoryMode(void);
unsigned char FT6X36_TestItem_RawDataTest(bool *bTestResult);

unsigned char FT6X36_TestItem_CbTest(bool *bTestResult);
unsigned char FT6X36_TestItem_DeltaCbTest(unsigned char *bTestResult);
unsigned char FT6X36_TestItem_ChannelsDeviationTest(unsigned char *bTestResult);
unsigned char FT6X36_TestItem_TwoSidesDeviationTest(unsigned char *bTestResult);

boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);


#endif
