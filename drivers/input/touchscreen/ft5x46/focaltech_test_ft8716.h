/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT8716.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test item for FT8716
*
************************************************************************/
#ifndef _TEST_FT8716_H
#define _TEST_FT8716_H
unsigned char FT8716_TestItem_OpenTest(struct i2c_client *client);
unsigned char FT8716_TestItem_ShortCircuitTest(struct i2c_client *client);
#endif
