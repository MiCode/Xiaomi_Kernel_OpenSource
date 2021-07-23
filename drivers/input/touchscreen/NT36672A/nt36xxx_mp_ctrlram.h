/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 32206 $
 * $Date: 2018-08-10 19:23:04 +0800 (週五, 10 八月 2018) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#if NVT_TOUCH_MP

extern uint32_t IC_X_CFG_SIZE;
extern uint32_t IC_Y_CFG_SIZE;
extern uint32_t IC_KEY_CFG_SIZE;
extern uint32_t X_Channel;
extern uint32_t Y_Channel;
extern uint32_t Key_Channel;
extern uint8_t AIN_X[40];
extern uint8_t AIN_Y[40];
#if TOUCH_KEY_NUM > 0
extern uint8_t AIN_KEY[8];
#endif /* #if TOUCH_KEY_NUM > 0 */

extern int32_t PS_Config_Lmt_Short_Rawdata_P[40 * 40];

extern int32_t PS_Config_Lmt_Short_Rawdata_N[40 * 40];

extern int32_t PS_Config_Lmt_Open_Rawdata_P[40 * 40];

extern int32_t PS_Config_Lmt_Open_Rawdata_N[40 * 40];

extern int32_t PS_Config_Lmt_FW_Rawdata_P[40 * 40];

extern int32_t PS_Config_Lmt_FW_Rawdata_N[40 * 40];

extern int32_t PS_Config_Lmt_FW_CC_P[40 * 40];

extern int32_t PS_Config_Lmt_FW_CC_N[40 * 40];

extern int32_t PS_Config_Lmt_FW_Diff_P[40 * 40];

extern int32_t PS_Config_Lmt_FW_Diff_N[40 * 40];

extern int32_t PS_Config_Diff_Test_Frame;

#ifndef NVT_SAVE_TESTDATA_IN_FILE
#define	TEST_BUF_LEN	5000

enum test_type
{
	SHORT_TEST = 0,
	OPEN_TEST,
	FWMUTUAL_TEST,
	FWCC_TEST,
	NOISE_MAX_TEST,
	NOISE_MIN_TEST,
	MAX_TEST_TYPE = 6,
};

struct item_buf
{
	uint8_t *buf;
	uint32_t len;
	enum test_type type;
};

struct test_buf
{
//	spinlock_t tbuf_lock;
//	bool update;
	struct item_buf shorttest;
	struct item_buf opentest;
	struct item_buf fwmutualtest;
	struct item_buf fwcctest;
	struct item_buf noisetest_max;
	struct item_buf noisetest_min;
};

int32_t nvt_test_data_proc_init(struct spi_device *client);
void nvt_test_data_proc_deinit(void);

#endif /*#ifndef NVT_SAVE_TESTDATA_IN_FILE*/

#endif /* #if NVT_TOUCH_MP */
