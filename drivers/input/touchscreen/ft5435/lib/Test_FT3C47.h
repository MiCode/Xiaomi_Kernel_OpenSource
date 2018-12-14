/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT3C47.h
*
* Author: Software Development Team, AE
*
* Created: 2015-12-02
*
* Abstract: test item for FT3C47
*
************************************************************************/
#ifndef _TEST_FT3C47_H
#define _TEST_FT3C47_H

#include "test_lib.h"

boolean FT3C47_StartTest(void);
int FT3C47_get_test_data(char *pTestData);

unsigned char FT3C47_TestItem_EnterFactoryMode(void);
unsigned char FT3C47_TestItem_RawDataTest(bool *bTestResult);

unsigned char FT3C47_TestItem_SCapRawDataTest(bool *bTestResult);
unsigned char FT3C47_TestItem_SCapCbTest(bool *bTestResult);

unsigned char FT3C47_TestItem_ForceTouch_SCapRawDataTest(bool *bTestResult);
unsigned char FT3C47_TestItem_ForceTouch_SCapCbTest(bool *bTestResult);

boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);


#endif
