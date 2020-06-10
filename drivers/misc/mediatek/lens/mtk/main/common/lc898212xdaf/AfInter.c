// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifdef DEBUG_LOG
#include <linux/fs.h>
#endif
#include <linux/delay.h>
#include <linux/i2c.h>
#define DeviceAddr 0xE4

#include "AfSTMV.h"

#ifdef DEBUG_LOG
#define AF_REGDUMP "REGDUMP"
#define LOG_INF(format, args...) pr_info(AF_REGDUMP " " format, ##args)
#endif

void RamWriteA(unsigned short addr, unsigned short data)
{
	u8 puSendCmd[3] = {(u8)(addr & 0xFF), (u8)(data >> 8),
			   (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("RAMW\t%x\t%x\n", addr, data);
#endif
}

void RamReadA(unsigned short addr, unsigned short *data)
{
	u8 buf[2];
	u8 puSendCmd[1] = {(u8)(addr & 0xFF)};

	s4AF_ReadReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), buf, 2,
				  DeviceAddr);
	*data = (buf[0] << 8) | (buf[1] & 0x00FF);

#ifdef DEBUG_LOG
	LOG_INF("RAMR\t%x\t%x\n", addr, *data);
#endif
}

void RegWriteA(unsigned short addr, unsigned char data)
{
	u8 puSendCmd[2] = {(u8)(addr & 0xFF), (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("REGW\t%x\t%x\n", addr, data);
#endif
}

void RegReadA(unsigned short addr, unsigned char *data)
{
	u8 puSendCmd[1] = {(u8)(addr & 0xFF)};

	s4AF_ReadReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), data, 1,
				  DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("REGR\t%x\t%x\n", addr, *data);
#endif
}

void WaitTime(unsigned short msec)
{
	usleep_range(msec * 1000, (msec + 1) * 1000);
}
