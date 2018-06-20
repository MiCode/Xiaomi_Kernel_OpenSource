/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_config.h
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: global function for test
*
************************************************************************/
#include "../focaltech_core.h"


/*-----------------------------------------------------------
IC Type Test
-----------------------------------------------------------*/
#define FT5X46_TEST     0x20
#define FT6X36_TEST     0x40
#define FT5822_TEST     0x70
#define FT8606_TEST     0x90
#define FT8716_TEST     0xA0
#define FT3C47_TEST     0xB0
#define FT8607_TEST     0xC0
#define FT8736_TEST     0xE0
#define FT3D47_TEST     0xF0
#define FTE716_TEST     0x100
#define FTE736_TEST     0x140
#define FT8006_TEST     0x130


#ifdef FTS_CHIP_TYPE

#if (FTS_CHIP_TYPE == _FT8716)
#define FTS_CHIP_TEST_TYPE      FT8716_TEST
#elif(FTS_CHIP_TYPE == _FT8736)
#define FTS_CHIP_TEST_TYPE      FT8736_TEST
#elif(FTS_CHIP_TYPE == _FT8006)
#define FTS_CHIP_TEST_TYPE      FT8006_TEST
#elif(FTS_CHIP_TYPE == _FT8606)
#define FTS_CHIP_TEST_TYPE      FT8606_TEST
#elif(FTS_CHIP_TYPE == _FT8607)
#define FTS_CHIP_TEST_TYPE      FT8607_TEST
#elif(FTS_CHIP_TYPE == _FTE716)
#define FTS_CHIP_TEST_TYPE      FTE716_TEST
#elif(FTS_CHIP_TYPE == _FT3D47)
#define FTS_CHIP_TEST_TYPE      FT3D47_TEST
#elif(IC_SERIALS == 0x01)
#define FTS_CHIP_TEST_TYPE      FT5822_TEST
#elif(FTS_CHIP_TYPE == _FT3C47U)
#define FTS_CHIP_TEST_TYPE      FT3C47_TEST
#elif(IC_SERIALS == 0x02)
#define FTS_CHIP_TEST_TYPE      FT5X46_TEST
#elif((IC_SERIALS == 0x03) || (IC_SERIALS == 0x04))
#define FTS_CHIP_TEST_TYPE      FT6X36_TEST
#endif

#else
#define FTS_CHIP_TEST_TYPE          FT8716_TEST

#endif
