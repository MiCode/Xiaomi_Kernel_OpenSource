/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"


#define AF_DRVNAME "GT9768AF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18 //1C

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

/* if use ISRC mode, should modify variables in init_setting */


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;
static long g_i4MotorStatus;
static long g_i4Dir;
static int g_sr = 3;


static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16) pBuff[0]) << 2) + (pBuff[1]);

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[3] = { 0x03, (char)(a_u2Data >> 8), (char)(a_u2Data & 0xFF) };

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static int gt9768_init(void)
{
	int i4RetValue = 0;
	char puSendCmd[3];


	LOG_INF("[GT9768AF] gt9768_init - beg\n");
	puSendCmd[0] = 0x02;
	puSendCmd[1] = 0x02;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   LOG_INF("[GT9768AF] I2C send failed!! \n");
	}
	mdelay(5);
	puSendCmd[0] = 0x06;
	puSendCmd[1] = 0x60;   //AAC4
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   LOG_INF("[GT9768AF] I2C send failed!! \n");
	}
	puSendCmd[0] = 0x07;
	puSendCmd[1] = 0x03;   //
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0)
	{
	   LOG_INF("[GT9768AF] I2C send failed!! \n");
	}

	LOG_INF("[GT9768AF] GT97772 SRC Init End!! \n");
	return i4RetValue;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	if (g_i4MotorStatus == 1)
		stMotorInfo.bIsMotorMoving = 1;
	else
		stMotorInfo.bIsMotorMoving = 0;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;


		gt9768_init();


		ret = s4AF_ReadReg(&InitPos);

		if (ret == 0) {
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

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(g_pAF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(g_pAF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	/* LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition, g_u4TargetPosition); */

	spin_lock(g_pAF_SpinLock);
	g_sr = 3;
	g_i4MotorStatus = 0;
	spin_unlock(g_pAF_SpinLock);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");

		spin_lock(g_pAF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(g_pAF_SpinLock);
	}

	return 0;
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
long GT9768AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *) (a_u4Param));
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
int GT9768AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {

		LOG_INF("Wait\n");
		s4AF_WriteReg(300);
		msleep(20);
		s4AF_WriteReg(250);
		msleep(20);
		s4AF_WriteReg(150);
		msleep(10);
		s4AF_WriteReg(100);
		msleep(10);
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

int GT9768AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}

int GT9768AF_GetFileName_Main(unsigned char *pFileName)
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

