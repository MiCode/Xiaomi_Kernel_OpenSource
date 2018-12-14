/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT5X46.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test item for FT5X46\FT5X46i\FT5526\FT3X17\FT5436\FT3X27\FT5526i\FT5416\FT5426\FT5435
*
************************************************************************/
#ifndef _TEST_FT5X46_H
#define _TEST_FT5X46_H

#include "test_lib.h"

boolean FT5X46_StartTest(void);
int FT5X46_get_test_data(char *pTestData);

unsigned char FT5X46_TestItem_EnterFactoryMode(void);
unsigned char FT5X46_TestItem_RawDataTest(bool *bTestResult);

unsigned char FT5X46_TestItem_SCapRawDataTest(bool *bTestResult);
unsigned char FT5X46_TestItem_SCapCbTest(bool *bTestResult);

boolean GetWaterproofMode(int iTestType, unsigned char ucChannelValue);


#endif
