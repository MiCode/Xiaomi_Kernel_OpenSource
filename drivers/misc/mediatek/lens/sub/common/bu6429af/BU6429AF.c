/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * BU6429AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "BU6429AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

/* if use ISRC mode, should modify variables in init_setting */
#define USE_ISRC_MODE_S5K2P8_SENSOR

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;
static int g_i4DriverStatus;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		g_i4DriverStatus++;
		LOG_INF("I2C read failed - %d!!\n", g_i4DriverStatus);
		return -1;
	}

	*a_pu2Result = (((u16)pBuff[0]) << 2) + (pBuff[1]);

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

#ifdef USE_ISRC_MODE_S5K2P8_SENSOR
	char puSendCmd[2] = {(char)(((a_u2Data >> 8) & 0x03) | 0xC4),
			     (char)(a_u2Data & 0xFF)};
#else
	char puSendCmd[2] = {(char)(((a_u2Data >> 8) & 0x03) | 0xC0),
			     (char)(a_u2Data & 0xFF)};
#endif

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		g_i4DriverStatus++;
		LOG_INF("I2C write failed - %d!!\n", g_i4DriverStatus);
		return -1;
	}

	return 0;
}

#ifdef USE_ISRC_MODE_S5K2P8_SENSOR
static int init_setting(void)
{
	int i4RetValue;
	char puSendCmd[2];

	char PS, EN, Wx, Mode, Qfact, MSB, resonant_frequency;

	/* following 1 variables should be modified by module */
	/* resonant_frequency = 106; 1 ~ 255   0.4*106=72.4Hz */
	resonant_frequency = 123; /* 79HZ */
	/* following 1 variables for ISRC mode */
	Mode = 0; /* 0~2  0 0.5x mode ;1 0.8x mode;2 1.0x mode */

	/* following 1 variables for Q fact,modified by module */
	Qfact = 6; /* if Qfact=15 no need to set Q fact */

	PS = 1;   /* power state */
	EN = 0;   /* set to 1 when init */
	Mode = 1; /* 0: direct mode  1: ISRC mode */
	MSB = (char)((PS << 7) & (EN << 6) & (Mode << 2));

	/* convert frequency to register setting */

	/* step1: power on */
	/* puSendCmd[2] = (char)(0xc2), (char)(0x00); */
	puSendCmd[0] = (char)(0xc2);
	puSendCmd[1] = (char)(0x00);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

/* step2: set Q fact adjustment */
#if 0
	if (Mode == 0) {
		char i;
		char puQValue[21][2] = {
			{(char)(0xe8), (char)(0x3f)},
			{(char)(0xe8), (char)(0x3f)},
			{(char)(0xe8), (char)(0x6f)},
			{(char)(0xe8), (char)(0x6f)},
			{(char)(0xe8), (char)(0xc9)},
			{(char)(0xe8), (char)(0xc9)},
			{(char)(0xe9), (char)(0x38)},
			{(char)(0xe9), (char)(0x38)},
			{(char)(0xe9), (char)(0x83)},
			{(char)(0xe9), (char)(0x83)},
			{(char)(0xe9), (char)(0xca)},
			{(char)(0xe9), (char)(0xca)},
			{(char)(0xe9), (char)(0xef)},
			{(char)(0xe9), (char)(0xef)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)},
			{(char)(0xea), (char)(0x00)}
		};

		for (i = 0; i < 21; i++) {
			i4RetValue = i2c_master_send(g_pstAF_I2Cclient,
						puQValue[i], 2);
			if (i4RetValue < 0) {
				LOG_INF("I2C send Q fact %d failed!!\n", i);
				return -1;
			}
		}
	}
#endif

	/* step3: resonant_frequency */
	Wx = 0x2;

	puSendCmd[0] = (char)((MSB) & (Wx << 3));
	puSendCmd[1] = (char)(resonant_frequency & 0xff);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	/* step4: set ISRC mode */
	Wx = 0x1;

	/* puSendCmd[2] = (char)((MSB) & (Wx << 3)), (char)(Mode & 0xff); */
	puSendCmd[0] = (char)((MSB) & (Wx << 3));
	puSendCmd[1] = (char)(Mode & 0xff);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}
#endif

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (g_i4DriverStatus > 2) /* I2C failed */
		return -EINVAL;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;

		ret = s4AF_ReadReg(&InitPos);
#ifdef USE_ISRC_MODE_S5K2P8_SENSOR
		init_setting();
#endif

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long BU6429AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int BU6429AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		char puSendCmd[2];

		puSendCmd[0] = (char)(0x00);
		puSendCmd[1] = (char)(0x00);
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
		LOG_INF("Wait\n");
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	g_i4DriverStatus = 0;

	LOG_INF("End\n");

	return 0;
}

int BU6429AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}
