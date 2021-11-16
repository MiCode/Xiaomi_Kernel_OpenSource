/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __MTK_HDCP_H__
#define __MTK_HDCP_H__

#include <linux/types.h>


#ifdef DPTX_HDCP_ENABLE

struct HDCP1X_INFO {
	int MainStates;
	int SubStates;
	unsigned char uRetryCount;
	bool bEnable:1;
	bool bRepeater:1;
	bool bR0Read:1;
	bool bKSV_READY:1;
	bool bMAX_CASCADE:1;
	bool bMAX_DEVS:1;
	unsigned char ubBstatus;
	unsigned char ubBksv[5];
	unsigned char ubAksv[5];
	unsigned char ubV[20];
	unsigned char ubBinfo[2];
	unsigned char ubKSVFIFO[5*127];
	unsigned char ubDEVICE_COUNT;
};

struct HDCP2_INFO {
	bool bEnable:1;
	bool bRepeater:1;
	bool bReadcertrx:1;
	bool bReadHprime:1;
	bool bReadPairing:1;
	bool bReadLprime:1;
	bool bksExchangeDone:1;
	bool bReadVprime:1;
	uint8_t uRetryCount;
	uint8_t uDeviceCount;
	uint8_t uStreamIDType;
};

enum HDCP_RESULT {
	AUTH_ZERO     = 0,
	AUTH_INIT     = 1,
	AUTH_PASS     = 2,
	AUTH_FAIL     = 3,
};

#endif
#endif

