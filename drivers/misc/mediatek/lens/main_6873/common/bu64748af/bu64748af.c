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
 * bu64748af voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "bu64748_function.h"
#include "lens_info.h"

#define AF_DRVNAME "bu64748af_main_drv"

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

int main_SOutEx(unsigned char slaveAddress,
		unsigned char *dat, int size)
{
	int i4RetValue = 0;

	g_pstAF_I2Cclient->addr = slaveAddress;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, dat, size);

	if (i4RetValue < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	return 0;
}

int main_SInEx(unsigned char slaveAddress, unsigned char *dat,
		int size, unsigned char *ret, int ret_size)
{
	int ret_value = 0;
	struct i2c_msg msg[2];
	struct i2c_adapter *adap = g_pstAF_I2Cclient->adapter;

	memset(msg, 0, sizeof(msg));

	g_pstAF_I2Cclient->addr = slaveAddress;
	msg[0].addr = slaveAddress;
	msg[0].flags = 0;
	msg[0].len = size;
	msg[0].buf = dat;

	msg[1].addr = slaveAddress;
	msg[1].flags = I2C_M_RD;
	msg[1].len = ret_size;
	msg[1].buf = ret;

	ret_value = i2c_transfer(adap, msg, 2);
	if (ret_value < 0) {
		LOG_INF("I2C read - recv failed!!\n");
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
	LOG_INF("+\n");

	if (*g_pAF_Opened == 1) {

		int ret = 0;

		BU64748_main_soft_power_ctrl(1);

		ret = BU64748_main_Initial();
		if (ret) {
			LOG_INF("bu64748af_main init failed.line:%d.\n",
				__LINE__);
			return -EINVAL;
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	main_AF_TARGET(a_u4Position);
	g_u4CurrPosition = a_u4Position;

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

long bu64748af_Ioctl_Main(struct file *a_pstFile, unsigned int a_u4Command,
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

int bu64748af_Release_Main(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		BU64748_main_soft_power_ctrl(0);
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

static int PowerDown = 1;

int bu64748af_PowerDown_Main(struct i2c_client *pstAF_I2Cclient,
			int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_Opened = pAF_Opened;

	LOG_INF("+\n");

	if (PowerDown == 0)
		return -1;

	if (*g_pAF_Opened == 0) {
		BU64748_main_soft_power_ctrl(0);
		LOG_INF("apply\n");
	}
	LOG_INF("-\n");

	return 0;
}

int bu64748af_SetI2Cclient_Main(struct i2c_client *pstAF_I2Cclient,
				spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	unsigned char out[4] = {0};
	int ret;

	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
	out[0] = _OP_Periphe_RW;
	out[1] = 0xEF;
	out[2] = 0;
	out[3] = 0;

	ret = main_SOutEx(_SLV_FBAF_, out, 4);

	if (ret < 0 && *g_pAF_Opened == 0)
		PowerDown = 0;

	LOG_INF("SetI2Cclient value(0x%x)\n", ret);

	initAF();

	return (ret == 0);
}

int bu64748af_GetFileName_Main(unsigned char *pFileName)
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
