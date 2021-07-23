/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
 * BU63169AF voice coil motor driver
 * BU63169 : OIS driver
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

/* kernel standard for PMIC*/
#if !defined(CONFIG_MTK_LEGACY)
#include <linux/regulator/consumer.h>
#endif

#include "OIS_head.h"
#include "lens_info.h"

#define AF_DRVNAME "BU63169AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x1c
#define EEPROM_I2C_SLAVE_ADDR 0xa4
#define AK7377AF_I2C_SLAVE_ADDR 0x1E

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;
unsigned char g_module_id_OIS = 0;

/* PMIC */
#if !defined(CONFIG_MTK_LEGACY)
static struct regulator *regVCAMAF;
static struct device *lens_device;
#endif

static void TimeoutHandle(void)
{
	LOG_INF("BU63169 Timeout Handle Flow\n");

#if !defined(CONFIG_MTK_LEGACY)
	lens_device = &g_pstAF_I2Cclient->dev;

	if (regVCAMAF == NULL)
		regVCAMAF = regulator_get(lens_device, "vcamaf");

	if (regulator_is_enabled(regVCAMAF)) {
		LOG_INF("Camera Power enable\n");

		if (regulator_is_enabled(regVCAMAF)) {
			if (regulator_disable(regVCAMAF) != 0)
				LOG_INF("Fail to regulator_disable\n");
			if (regulator_disable(regVCAMAF) != 0)
				LOG_INF("Fail to regulator_disable\n");
		}

		msleep(20);

		if (!regulator_is_enabled(regVCAMAF)) {
			LOG_INF("AF Power off\n");
			if (regulator_set_voltage(regVCAMAF, 2800000,
						  2800000) != 0)
				LOG_INF("regulator_set_voltage fail\n");
			if (regulator_set_voltage(regVCAMAF, 2800000,
						  2800000) != 0)
				LOG_INF("regulator_set_voltage fail\n");

			LOG_INF("AF Power On\n");
			if (regulator_enable(regVCAMAF) != 0)
				LOG_INF("regulator_enable fail\n");
			if (regulator_enable(regVCAMAF) != 0)
				LOG_INF("regulator_enable fail\n");
		}
	} else {
		LOG_INF("Camera Power disable\n");
	}
#endif
}

static int s4AK7377AF_WriteReg(unsigned short a_u2Addr, unsigned short a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)a_u2Addr, (char)a_u2Data};

	g_pstAF_I2Cclient->addr = AK7377AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	return 0;
}
#if 0
static int s4AK7377AF_ReadReg(u8 a_uAddr, u16 *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff;
	char puSendCmd[1];

	puSendCmd[0] = a_uAddr;

	g_pstAF_I2Cclient->addr = AK7377AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 1);

	if (i4RetValue < 0) {
		LOG_INF("I2C read - send failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &pBuff, 1);

	if (i4RetValue < 0) {
		LOG_INF("I2C read - recv failed!!\n");
		return -1;
	}
	*a_pu2Result = pBuff;

	return 0;
}
#endif
unsigned char Read_Eeprom_Module_ID_BU63169AF(unsigned short addr)
{
	unsigned char u8data;

	unsigned char pu_send_cmd[2] = { (unsigned char)(addr >> 8),
					(unsigned char)(addr & 0xFF) };

	g_pstAF_I2Cclient->addr = (EEPROM_I2C_SLAVE_ADDR) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmd, 2) < 0) {
		LOG_INF("read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0) {
		LOG_INF("EEPROM_ReadReg failed!!\n");
		return -1;
	}

	LOG_INF("u8data = 0x%x\n", u8data);

	return u8data;
}

static inline int setAK7377AFPos(unsigned long a_u4Position)
{
	int i4RetValue = 0;

	LOG_INF("jianlong a_u4Position = %lu", a_u4Position);
	i4RetValue = s4AK7377AF_WriteReg(
		0x0, (unsigned short)((a_u4Position >> 2) & 0xff));

	if (i4RetValue < 0)
		return -1;

	i4RetValue = s4AK7377AF_WriteReg(
		0x1, (unsigned short)((a_u4Position & 0x3) << 6));

	return i4RetValue;
}

int s4EEPROM_ReadReg_BU63169AF(unsigned short addr, unsigned short *data)
{
	int i4RetValue = 0;
	unsigned short addrl = addr + 1;

	unsigned char u8data[2];
	unsigned char pu_send_cmd[2] = {(unsigned char)(addr >> 8),
					(unsigned char)(addr & 0xFF)};
	unsigned char pu_send_cmdl[2] = {(unsigned char)(addrl >> 8),
					(unsigned char)(addrl & 0xFF)};

	*data = 0;
	g_pstAF_I2Cclient->addr = (EEPROM_I2C_SLAVE_ADDR) >> 1;
	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmd, 2) < 0) {
		LOG_INF("read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data[0], 1) < 0) {
		LOG_INF("EEPROM_ReadReg failed!!\n");
		return -1;
	}

	if (i2c_master_send(g_pstAF_I2Cclient, pu_send_cmdl, 2) < 0) {
		LOG_INF("read I2C send failed!!\n");
		return -1;
	}
	if (i2c_master_recv(g_pstAF_I2Cclient, &u8data[1], 1) < 0) {
		LOG_INF("EEPROM_ReadReg failed!!\n");
		return -1;
	}

	LOG_INF("u8data[0] = 0x%x\n", u8data[0]);
	LOG_INF("u8data[1] = 0x%x\n", u8data[1]);

	*data = u8data[0] << 8 | u8data[1];

	LOG_INF("s4EEPROM_ReadReg2 0x%x, 0x%x\n", addr, *data);

	return i4RetValue;
}

int s4AF_WriteReg_BU63169AF(unsigned short i2c_id, unsigned char *a_pSendData,
			    unsigned short a_sizeSendData)
{
	int i4RetValue = 0;

	LOG_INF("jianlong s4AF_WriteReg\n");
	spin_lock(g_pAF_SpinLock);
	g_pstAF_I2Cclient->addr = i2c_id >> 1;
	spin_unlock(g_pAF_SpinLock);

	i4RetValue =
		i2c_master_send(g_pstAF_I2Cclient, a_pSendData, a_sizeSendData);

	if (i4RetValue == -EIO)
		TimeoutHandle();

	if (i4RetValue != a_sizeSendData) {
		LOG_INF("I2C send failed!!, Addr = 0x%x, Data = 0x%x\n",
			a_pSendData[0], a_pSendData[1]);
		return -1;
	}

	return 0;
}

int s4AF_ReadReg_BU63169AF(unsigned short i2c_id, unsigned char *a_pSendData,
			   unsigned short a_sizeSendData,
			   unsigned char *a_pRecvData,
			   unsigned short a_sizeRecvData)
{
	int i4RetValue;
	struct i2c_msg msg[2];

	spin_lock(g_pAF_SpinLock);
	g_pstAF_I2Cclient->addr = i2c_id >> 1;
	spin_unlock(g_pAF_SpinLock);

	msg[0].addr = g_pstAF_I2Cclient->addr;
	msg[0].flags = 0;
	msg[0].len = a_sizeSendData;
	msg[0].buf = a_pSendData;

	msg[1].addr = g_pstAF_I2Cclient->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = a_sizeRecvData;
	msg[1].buf = a_pRecvData;

	i4RetValue =
		i2c_transfer(g_pstAF_I2Cclient->adapter, msg, ARRAY_SIZE(msg));

	if (i4RetValue == -EIO)
		TimeoutHandle();

	if (i4RetValue != 2) {
		LOG_INF("I2C Read failed!!\n");
		return -1;
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
	//unsigned char module_id = 0;
	LOG_INF("+\n");

	if (*g_pAF_Opened == 1) {

		g_module_id_OIS = Read_Eeprom_Module_ID_BU63169AF(0x0001);
		if (0x01 == g_module_id_OIS)
			s4AK7377AF_WriteReg(0x02, 0x00);

		//LOG_INF("jianlong 825f = 0x%04X\n", I2C_OIS_per__read(0x5F));
		//I2C_OIS_mem_write(0x7F, 0x0C0C);
		//LOG_INF("jianlong 847f = 0x%04X\n", I2C_OIS_mem__read(0x7F));
		Main_OIS();
		//setOISMode(1);

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
	//unsigned char module_id = 0;

	LOG_INF("jianlong moveAF a_u4Position = %lu", a_u4Position);
	//LOG_INF("jianlong 847f = 0x%04X\n", I2C_OIS_mem__read(0x7F));
	//module_id = Read_Eeprom_Module_ID_BU63169AF(0x0001);
	if (0x01 == g_module_id_OIS) {
		if (setAK7377AFPos(a_u4Position) == 0) {
			g_u4CurrPosition = a_u4Position;
			ret = 0;
		} else {
			LOG_INF("set I2C failed when moving the motor\n");
			ret = -1;
		}
	} else {
		if (setVCMPos((unsigned short)a_u4Position) == 0) {
			g_u4CurrPosition = a_u4Position;
			ret = 0;
		} else {
			LOG_INF("set I2C failed when moving the motor\n");
			ret = -1;
		}
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

static inline int setAFPara(__user struct stAF_MotorCmd *pstMotorCmd)
{
	struct stAF_MotorCmd stMotorCmd;

	if (copy_from_user(&stMotorCmd, pstMotorCmd, sizeof(stMotorCmd)))
		LOG_INF("copy to user failed when getting motor command\n");

	LOG_INF("Motor CmdID : %x\n", stMotorCmd.u4CmdID);

	LOG_INF("Motor Param : %x\n", stMotorCmd.u4Param);

	switch (stMotorCmd.u4CmdID) {
	case 1:
		//setOISMode(1);
		setOISMode((int)stMotorCmd.u4Param); /* 1 : disable */
		break;
	case 2:
		if (*g_pAF_Opened == 2 && stMotorCmd.u4Param > 0) {
			unsigned short PosX, PosY;

			PosX = stMotorCmd.u4Param / 10000;
			PosY = stMotorCmd.u4Param - PosX * 10000;

			LOG_INF("OIS %x\n", I2C_OIS_mem__read(0x7F));
			I2C_OIS_mem_write(0x7F, 0x2C0C);
			I2C_OIS_mem_write(0x17, PosX);
			I2C_OIS_mem_write(0x97, PosY);
			LOG_INF("Targe (%d , %d)\n", PosX, PosY);
		}
		break;
	}

	return 0;
}

static inline int getOISInfo(__user struct stAF_MotorOisInfo *pstMotorOisInfo)
{
	struct stAF_MotorOisInfo stMotorOisInfo;

	if (*g_pAF_Opened == 2) {
		stMotorOisInfo.i4OISHallPosXum =
			((short)I2C_OIS_mem__read(0x3F)) * 1000;
		stMotorOisInfo.i4OISHallPosYum =
			((short)I2C_OIS_mem__read(0xBF)) * 1000;
	} else {
		stMotorOisInfo.i4OISHallPosXum = 0;
		stMotorOisInfo.i4OISHallPosYum = 0;
	}
	stMotorOisInfo.i4OISHallFactorX = 26487; /* 26.487 [LSB/um] */
	stMotorOisInfo.i4OISHallFactorY = 26487;
	/* Res(um) = HallPosX / 26.487 */

	if (copy_to_user(pstMotorOisInfo, &stMotorOisInfo,
			 sizeof(struct stAF_MotorOisInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long BU63169AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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

	case AFIOC_S_SETPARA:
		i4RetValue =
			setAFPara((__user struct stAF_MotorCmd *)(a_u4Param));
		break;

	case AFIOC_G_MOTOROISINFO:
		i4RetValue = getOISInfo(
			(__user struct stAF_MotorOisInfo *)(a_u4Param));
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
int BU63169AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	//unsigned char module_id = 0;
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		//module_id = Read_Eeprom_Module_ID_BU63169AF(0x0001);
		OIS_Standby();
		if (0x01 == g_module_id_OIS)
			s4AK7377AF_WriteReg(0x02, 0x20);
		msleep(20);
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

#if 0
int BU63169AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
			int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_Opened = pAF_Opened;

	LOG_INF("+\n");
	if (*g_pAF_Opened == 0) {
		unsigned short data = 0;
		int cnt = 0;

		while (1) {
			data = 0;

			s4AF_WriteReg_BU63169AF(0x02, 0x20);

			s4AF_ReadReg_BU63169AF(0x02, &data);

			LOG_INF("Addr : 0x02 , Data : %x\n", data);

			OIS_Standby();

			if (data == 0x20 || cnt == 1)
				break;

			cnt++;
		}
	}
	LOG_INF("-\n");

	return 0;
}
#endif

int BU63169AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			   spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
#if !defined(CONFIG_MTK_LEGACY)
	regVCAMAF = NULL;
	lens_device = NULL;
#endif

	LOG_INF("SetI2Cclient\n");

	initAF();

	return 1;
}

int BU63169AF_GetFileName(unsigned char *pFileName)
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
