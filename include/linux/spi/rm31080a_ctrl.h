/*
 * Raydium RM31080 touchscreen header
 *
 * Copyright (C) 2012-2013, Raydium Semiconductor Corporation.
 * Copyright (C) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _RM31080A_CTRL_H_
#define _RM31080A_CTRL_H_

#define FILTER_NONTHRESHOLD_MODE		0x00

#define REPEAT_1						0x00

struct rm_tch_ctrl_para {

	unsigned short u16DataLength;
	unsigned short bICVersion;
	unsigned short u16ResolutionX;
	unsigned short u16ResolutionY;
	unsigned char bActiveDigitalRepeatTimes;
	unsigned char bAnalogRepeatTimes;
	unsigned char bIdleDigitalRepeatTimes;
	unsigned char bTime2Idle;
	unsigned char bfPowerMode;
	unsigned char bKernelMsg;
	unsigned char bTimerTriggerScale;
	unsigned char bfIdleModeCheck;
	unsigned char bWatchDogNormalCnt;
	unsigned char bNsFucnEnable;
};

extern struct rm_tch_ctrl_para g_stCtrl;

int rm_tch_ctrl_clear_int(void);
int rm_tch_ctrl_scan_start(void);
void rm_tch_ctrl_wait_for_scan_finish(void);

void rm_tch_ctrl_init(void);
unsigned char rm_tch_ctrl_get_idle_mode(unsigned char *p);
void rm_tch_ctrl_set_parameter(void *arg);
void rm_set_repeat_times(u8 u8Times);
#endif				/*_RM31080A_CTRL_H_*/
