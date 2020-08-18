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
 * LC898212AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "LC898212AF_DRV"
#define AF_I2C_SLAVE_ADDR 0xE4
#define EEPROM_I2C_SLAVE_ADDR 0xA0

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;

static int s4EEPROM_ReadReg(u16 addr, u16 *data)
{
	u8 u8data = 0;
	u8 pu_send_cmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};

	g_pstAF_I2Cclient->addr = (EEPROM_I2C_SLAVE_ADDR) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmd, 2) < 0) {
		LOG_INF("read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
		LOG_INF("failed!!\n");
		return -1;
	}
	*data = u8data;
	LOG_INF("EEPROM 0x%x, 0x%x\n", addr, *data);

	return 0;
}

static int s4AF_ReadReg(u8 length, u8 addr, u16 *data)
{
	u8 pBuff[2];
	u8 u8data = 0;

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, &addr, 1) < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return -1;
	}

	if (length == 0) {
		if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
			LOG_INF("Read Reg failed!!\n");
			return -1;
		}
		*data = u8data;
	} else if (length == 1) {
		if (i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2) < 0) {
			LOG_INF("Read Reg2 failed!!\n");
			return -1;
		}

		*data = (((u16)pBuff[0]) << 8) + ((u16)pBuff[1]);
	}
	LOG_INF("Read Reg 0x%x, 0x%x, 0x%x\n", length, addr, *data);

	return 0;
}

static int s4AF_WriteReg(u8 length, u8 addr, u16 data)
{
	u8 puSendCmd[2] = {addr, (u8)(data & 0xFF)};
	u8 puSendCmd2[3] = {addr, (u8)((data >> 8) & 0xFF), (u8)(data & 0xFF)};

	LOG_INF("WriteReg 0x%x, 0x%x, 0x%x\n", length, addr, data);

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR) >> 1;
	if (length == 0) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2) < 0) {
			LOG_INF("WriteReg failed!!\n");
			return -1;
		}
	} else if (length == 1) {
		if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 3) < 0) {
			LOG_INF("WriteReg 2 failed!!\n");
			return -1;
		}
	}

	return 0;
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

		u16 Reg_0x85 = 0;
		u16 Reg_0x3C = 0;
		u16 eepdata1 = 0, eepdata2 = 0;
		u16 posh = 0, posl = 0, max_pos = 0, min_pos = 0;
		u16 No_eeprom = 0;

		s4EEPROM_ReadReg(0x0000, &posl);
		s4EEPROM_ReadReg(0x0001, &posh);
		min_pos = (posh << 8) + posl;
		s4EEPROM_ReadReg(0x0002, &posl);
		s4EEPROM_ReadReg(0x0003, &posh);
		max_pos = (posh << 8) + posl;

		s4EEPROM_ReadReg(0x0004, &eepdata1);
		No_eeprom = s4EEPROM_ReadReg(0x0005, &eepdata2);

		LOG_INF("min %d, max %x, offset 0x%x, gain 0x%x\n",
			min_pos, max_pos, eepdata1, eepdata2);

		s4AF_WriteReg(0, 0x80, 0x34);
		s4AF_WriteReg(0, 0x81, 0x20);
		s4AF_WriteReg(0, 0x84, 0xe0);
		s4AF_WriteReg(0, 0x87, 0x05);
		s4AF_WriteReg(0, 0xA4, 0x24);
		s4AF_WriteReg(1, 0x3a, 0x0000);
		s4AF_WriteReg(1, 0x04, 0x0000);
		s4AF_WriteReg(1, 0x02, 0x0000);
		s4AF_WriteReg(1, 0x18, 0x0000);
		s4AF_WriteReg(0, 0x88, 0x70);
		if (No_eeprom == 0) {
			s4AF_WriteReg(0, 0x28, eepdata1);
			s4AF_WriteReg(0, 0x29, eepdata2);
		} else {
			s4AF_WriteReg(0, 0x28, 0x80);
			s4AF_WriteReg(0, 0x29, 0x80);
		}
		s4AF_WriteReg(1, 0x4c, 0x4000);
		s4AF_WriteReg(0, 0x83, 0x2c);
		s4AF_WriteReg(0, 0x85, 0xc0);

		msleep(20);
		s4AF_ReadReg(0, 0x85, &Reg_0x85);
		while (Reg_0x85 != 0x00) {
			msleep(20);
			s4AF_ReadReg(0, 0x85, &Reg_0x85);
		}
		s4AF_WriteReg(0, 0x84, 0xe3);
		s4AF_WriteReg(0, 0x97, 0x00);
		s4AF_WriteReg(0, 0x98, 0x42);
		s4AF_WriteReg(0, 0x99, 0x00);
		s4AF_WriteReg(0, 0x9a, 0x00);
		s4AF_WriteReg(0, 0x86, 0x40);
		s4AF_WriteReg(1, 0x40, 0x8010);
		s4AF_WriteReg(1, 0x42, 0x7570);
		s4AF_WriteReg(1, 0x44, 0x8b50);
		s4AF_WriteReg(1, 0x46, 0x6a10);
		s4AF_WriteReg(1, 0x48, 0x5a90);
		s4AF_WriteReg(1, 0x4a, 0x2030);
		s4AF_WriteReg(1, 0x4c, 0x32f0);
		s4AF_WriteReg(1, 0x4e, 0x7ff0);
		s4AF_WriteReg(1, 0x50, 0x04f0);
		s4AF_WriteReg(1, 0x52, 0x7610);
		s4AF_WriteReg(1, 0x54, 0x1450);
		s4AF_WriteReg(1, 0x56, 0x0000);
		s4AF_WriteReg(1, 0x58, 0x7ff0);
		s4AF_WriteReg(1, 0x5a, 0x0680);
		s4AF_WriteReg(1, 0x5c, 0x72f0);
		s4AF_WriteReg(1, 0x5e, 0x7f70);
		s4AF_WriteReg(1, 0x60, 0x7ed0);
		s4AF_WriteReg(1, 0x62, 0x7ff0);
		s4AF_WriteReg(1, 0x64, 0x0000);
		s4AF_WriteReg(1, 0x66, 0x0000);
		s4AF_WriteReg(1, 0x68, 0x5130);
		s4AF_WriteReg(1, 0x6a, 0x72f0);
		s4AF_WriteReg(1, 0x6c, 0x8010);
		s4AF_WriteReg(1, 0x6e, 0x0000);
		s4AF_WriteReg(1, 0x70, 0x0000);
		s4AF_WriteReg(1, 0x72, 0x18e0);
		s4AF_WriteReg(1, 0x74, 0x4e30);
		s4AF_WriteReg(1, 0x30, 0x0000);
		s4AF_WriteReg(1, 0x76, 0x0c50);
		s4AF_WriteReg(1, 0x78, 0x4000);
		s4AF_ReadReg(1, 0x3C, &Reg_0x3C);
		s4AF_WriteReg(1, 0x04, Reg_0x3C);
		s4AF_WriteReg(1, 0x18, Reg_0x3C);
		s4AF_WriteReg(1, 0x5A, 0x0800);
		s4AF_WriteReg(0, 0x83, 0xAC);
		s4AF_WriteReg(0, 0xA0, 0x02);
		s4AF_WriteReg(1, 0x7A, 0x7000);
		s4AF_WriteReg(1, 0x7E, 0x7E00);
		s4AF_WriteReg(0, 0x93, 0x40);
		s4AF_WriteReg(0, 0x86, 0x60);
		s4AF_WriteReg(0, 0x87, 0x85);
		msleep(30);

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

static int SetVCMPos(u16 _wData)
{
	u16 TargetPos;
	/* u32 tmpcal=0; */
	u16 ExistentPos = 0;
	int i2cret = 0;

	_wData = _wData << 2;

	/* 0~1024 => 0x8010~0x7ff0 */
	if (_wData < 0x800)
		TargetPos = 0x800 - _wData;
	else
		TargetPos = (0x1800 - _wData) & 0xFFF;
	TargetPos = TargetPos << 4;

	s4AF_ReadReg(1, 0x3C, &ExistentPos);
	LOG_INF("move pos 0x%x 0x%x\n", TargetPos, ExistentPos);
	if (TargetPos > ExistentPos) {
		s4AF_WriteReg(1, 0xA1, TargetPos & 0xfff0);
		s4AF_WriteReg(1, 0x16, 0x0180);
		s4AF_WriteReg(0, 0x8F, 0x01);
		i2cret = s4AF_WriteReg(0, 0x8A, 0x8D);
	} else if (TargetPos < ExistentPos) {
		s4AF_WriteReg(1, 0xA1, TargetPos & 0xfff0);
		s4AF_WriteReg(1, 0x16, 0xFE80);
		s4AF_WriteReg(0, 0x8F, 0x01);
		i2cret = s4AF_WriteReg(0, 0x8A, 0x8D);
	}
	return i2cret;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (SetVCMPos((u16)a_u4Position) == 0) {
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
long LC898212AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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
int LC898212AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2)
		LOG_INF("Wait\n");

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int LC898212AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			    spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int LC898212AF_GetFileName(unsigned char *pFileName)
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
