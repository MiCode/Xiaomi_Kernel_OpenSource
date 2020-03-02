/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * LC898217AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "LC898217AF_DRV"
#define AF_I2C_SLAVE_ADDR 0xE4

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define POWER_ALWAYS_ON 0

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;
#define Min_Pos 0
#define Max_Pos 1023

#if POWER_ALWAYS_ON
static int g_TimeOutChk;
static unsigned int g_PreTime;
static struct timespec g_TSAFOpen;
static struct timespec g_TSAFClose;
static unsigned int g_SkipAFUninit;
#endif

static int s4AF_ReadReg(u8 a_uAddr, u8 *a_uData)
{
	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR) >> 1;

	if (i2c_master_send(g_pstAF_I2Cclient, &a_uAddr, 1) < 0) {
		LOG_INF("ReadI2C send failed!!\n");
		return -1;
	}

	if (i2c_master_recv(g_pstAF_I2Cclient, a_uData, 1) < 0) {
		LOG_INF("ReadI2C recv failed!!\n");
		return -1;
	}

	/* LOG_INF("RDI2C 0x%x, 0x%x\n", a_uAddr, *a_uData); */

	return 0;
}

static int s4AF_WriteReg(u8 a_uLength, u8 a_uAddr, u16 a_u2Data)
{
	u8 puSendCmd[2] = {a_uAddr, (u8)(a_u2Data & 0xFF)};
	u8 puSendCmd2[3] = {a_uAddr, (u8)((a_u2Data >> 8) & 0xFF),
			    (u8)(a_u2Data & 0xFF)};

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR) >> 1;

	/* LOG_INF("WRI2C 0x%04x, 0x%x\n", a_uAddr, a_u2Data); */

	if (a_uLength == 0) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2) < 0) {
			LOG_INF("WriteI2C failed!!\n");
			return -1;
		}
	} else if (a_uLength == 1) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 3) < 0) {
			LOG_INF("WriteI2C 2 failed!!\n");
			return -1;
		}
	}

	return 0;
}

static int setPosition(unsigned short UsPosition)
{
	unsigned short TarPos;
	unsigned char UcPosH;
	unsigned char UcPosL;
	unsigned int i4RetValue = 0;

	UsPosition = 1023 - UsPosition;
	TarPos = UsPosition;

	/* LOG_INF("DAC(%04d) -> %03x\n", UsPosition, TarPos); */

	UcPosH = (unsigned char)(TarPos >> 8);
	UcPosL = (unsigned char)(TarPos & 0x00FF);
	i4RetValue = s4AF_WriteReg(0, 0x84, UcPosH);
	if (i4RetValue != 0)
		return -1;
	i4RetValue = s4AF_WriteReg(0, 0x85, UcPosL); /*	set target position */

	return i4RetValue;
}

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

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	LOG_INF("+\n");

	if (*g_pAF_Opened == 1) {

		int i4RetValue = 0;
		int ret = 0;
		int cnt = 0;
		unsigned char Temp;

		#if POWER_ALWAYS_ON
		if (g_SkipAFUninit == 1) {
			LOG_INF("Skip init driver\n");
			g_SkipAFUninit = 0;
			return 1;
		}
		#endif

		s4AF_WriteReg(0, 0xF6, 0x00);
		s4AF_WriteReg(0, 0x96, 0x20);
		s4AF_WriteReg(0, 0x98, 0x00);

		s4AF_ReadReg(0xF0, &Temp);

		if (Temp == 0x72) {
			s4AF_WriteReg(0, 0xE0, 0x01);
			while (1) {
				mdelay(20);
				ret = s4AF_ReadReg(0xB3, &Temp);

				if (Temp == 0 && ret == 0) {
					i4RetValue = 1;
					break;
				}

				if (cnt >= 20)
					break;
				cnt++;
			}
			s4AF_WriteReg(0, 0xA1, 0x02);
			mdelay(2);
		} else {
			LOG_INF("Check HW version: %x\n", Temp);
		}


		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (setPosition((unsigned short)a_u4Position) == 0) {
		g_u4CurrPosition = a_u4Position;
		ret = 0;
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
long LC898217AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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
int LC898217AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	int Ret = 0;

	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		#if POWER_ALWAYS_ON
		unsigned long long start_ms, end_ms;
		unsigned int diff_ms;

		g_TSAFClose = CURRENT_TIME;
		start_ms = (g_TSAFOpen.tv_sec * NSEC_PER_SEC +
				g_TSAFOpen.tv_nsec) / 1000000;
		end_ms = (g_TSAFClose.tv_sec * NSEC_PER_SEC +
				g_TSAFClose.tv_nsec) / 1000000;
		diff_ms = end_ms - start_ms;

		LOG_INF("Wait - Excute Time %d\n", diff_ms);

		if (diff_ms < 600) {
			g_SkipAFUninit = 1;
			LOG_INF("Wait - skip uninit\n");
		} else {
			Ret = s4AF_WriteReg(0, 0x98, 0xC0);
			if (Ret == 0)
				Ret = s4AF_WriteReg(0, 0x96, 0x28);
			if (Ret == 0)
				Ret = s4AF_WriteReg(0, 0xF6, 0x80);
			LOG_INF("Wait - power down\n");
		}
		#else
		Ret = s4AF_WriteReg(0, 0x98, 0xC0);
		if (Ret == 0)
			Ret = s4AF_WriteReg(0, 0x96, 0x28);
		if (Ret == 0)
			Ret = s4AF_WriteReg(0, 0xF6, 0x80);
		LOG_INF("Wait - power down\n");
		#endif
		LOG_INF("Close\n");
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return Ret;
}

int LC898217AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
			int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_Opened = pAF_Opened;

	LOG_INF("+\n");
	if (*g_pAF_Opened == 0) {
		int Ret = 0;
		#if POWER_ALWAYS_ON
		struct timespec mTS;
		unsigned int CurTime;

		mTS = CURRENT_TIME;
		CurTime = (unsigned int)mTS.tv_sec;

		if (g_TimeOutChk == 0) {
			Ret = s4AF_WriteReg(0, 0x98, 0xC0);
			if (Ret == 0)
				Ret = s4AF_WriteReg(0, 0x96, 0x28);
			if (Ret == 0)
				Ret = s4AF_WriteReg(0, 0xF6, 0x80);

			if (Ret < 0) {
				g_PreTime = CurTime;
				g_TimeOutChk = 1;
				return -1;
			}

			LOG_INF("LC898217AF Power Down = %d\n", CurTime);
		} else {
			if (CurTime - g_PreTime > 60) {
				g_PreTime = CurTime;
				g_TimeOutChk = 0;
			}
		}
		#else
		Ret = s4AF_WriteReg(0, 0x98, 0xC0);
		if (Ret == 0)
			Ret = s4AF_WriteReg(0, 0x96, 0x28);
		if (Ret == 0)
			Ret = s4AF_WriteReg(0, 0xF6, 0x80);
		#endif
	}
	LOG_INF("-\n");

	return 0;
}

int LC898217AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			    spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	#if POWER_ALWAYS_ON
	g_TSAFOpen = CURRENT_TIME;
	#endif
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int LC898217AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
