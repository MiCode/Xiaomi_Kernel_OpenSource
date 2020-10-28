// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


/*
 * DW9839AF voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"


#define AF_DRVNAME "DW9839AF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " \
format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

#if defined(CONFIG_MACH_MT6771)
static unsigned int g_ACKErrorCnt = 5;
#else
static unsigned int g_ACKErrorCnt = 100;
#endif

static int check_status;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static inline void AFI2CSendFormat(struct stAF_MotorI2CSendCmd *pstMotor)
{
	pstMotor->Resolution = 10;
	pstMotor->SlaveAddr  = 0x18;
	pstMotor->I2CSendNum = 2;

	pstMotor->I2CFmt[0].AddrNum = 1;
	pstMotor->I2CFmt[0].DataNum = 1;
	/* Addr Format */
	pstMotor->I2CFmt[0].Addr[0]  = 0x00;
	/* Data Format : CtrlData | ( ( Data >> BitRR ) */
	/* & Mask1 ) << BitRL ) & Mask2 */
	pstMotor->I2CFmt[0].CtrlData[0] = 0x00; /* Control Data */
	pstMotor->I2CFmt[0].BitRR[0] = 2;
	pstMotor->I2CFmt[0].Mask1[0] = 0xFF;
	pstMotor->I2CFmt[0].BitRL[0] = 0;
	pstMotor->I2CFmt[0].Mask2[0] = 0xFF;


	pstMotor->I2CFmt[1].AddrNum = 1;
	pstMotor->I2CFmt[1].DataNum = 1;
	/* Addr Format */
	pstMotor->I2CFmt[1].Addr[0]  = 0x01;
	/* Data Format : CtrlData | ( ( Data >> BitRR ) */
	/* & Mask1 ) << BitRL ) & Mask2 */
	pstMotor->I2CFmt[1].CtrlData[0] = 0x00; /* Control Data */
	pstMotor->I2CFmt[1].BitRR[0] = 0;
	pstMotor->I2CFmt[1].Mask1[0] = 0x03;
	pstMotor->I2CFmt[1].BitRL[0] = 6;
	pstMotor->I2CFmt[1].Mask2[0] = 0xC0;
}

static inline int getAFI2CSendFormat(__user struct stAF_MotorI2CSendCmd
*pstMotorI2CSendCmd)
{
	struct stAF_MotorI2CSendCmd stMotor;

	AFI2CSendFormat(&stMotor);

	if (copy_to_user(pstMotorI2CSendCmd, &stMotor, sizeof(struct
	stAF_MotorI2CSendCmd)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static int s4AF_ReadReg(u8 a_uAddr, u16 *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff;
	char puSendCmd[1];

	if (g_ACKErrorCnt == 0)
		return 0;

	puSendCmd[0] = a_uAddr;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 1);

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C read - send failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &pBuff, 1);

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C read - recv failed!!\n");
		return -1;
	}
	*a_pu2Result = pBuff;

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Addr, u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = { (char)a_u2Addr, (char)a_u2Data };

	if (g_ACKErrorCnt == 0)
		return 0;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	/* LOG_INF("I2C Addr[0] = 0x%x , Data[0] = 0x%x\n",*/
	/*puSendCmd[0], puSendCmd[1]); */

	if (i4RetValue < 0) {
		if (g_ACKErrorCnt > 0)
			g_ACKErrorCnt--;

		LOG_INF("I2C write failed!!\n");
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

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct
	stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int setVCMPos(unsigned long a_u4Position)
{
	int i4RetValue = 0;


	i4RetValue = s4AF_WriteReg(0x0, (u16) ((a_u4Position >> 2) & 0xff));

	if (i4RetValue < 0)
		return -1;

	i4RetValue = s4AF_WriteReg(0x1, (u16) ((a_u4Position & 0x3) << 6));


	return i4RetValue;
}

static inline int initdrv(void)
{
	int i4RetValue = 0;
	int ret = 0;
	unsigned short data = 0;

	LOG_INF("check_status=%d\n", check_status);

	/* 00:active mode , 10:Standby mode , x1:Sleep mode */
	ret = s4AF_WriteReg(0x03, 0x11); /* from Standby mode to Active mode */

	msleep(20);

	if (ret == 0) {
		ret = s4AF_ReadReg(0x03, &data);


		if ((ret == 0) && ((data & 0x10) != 0))
			i4RetValue = 1;

	}

	return i4RetValue;
}
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}


	if (*g_pAF_Opened == 1) {
		unsigned short InitPos, InitPosM, InitPosL;

		if (initdrv() == 1) {
			spin_lock(g_pAF_SpinLock);
			*g_pAF_Opened = 2;
			spin_unlock(g_pAF_SpinLock);
		} else {
			LOG_INF("InitDrv Fail!! I2C error occurred");
		}

		s4AF_ReadReg(0x0, &InitPosM);
		ret = s4AF_ReadReg(0x1, &InitPosL);
		InitPos = ((InitPosM & 0xFF) << 2) + ((InitPosL >> 6) & 0x3);

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
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	/* LOG_INF("move [curr] %d [target] %d\n", g_u4CurrPosition,*/
	/*g_u4TargetPosition); */

	/* s4AF_WriteReg(0x02, 0x00); */

	if (setVCMPos(g_u4TargetPosition) == 0) {
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
long DW9839AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *)
		(a_u4Param));
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
int DW9839AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		s4AF_WriteReg(0x02, 0x20);
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

int DW9839AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
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

			s4AF_WriteReg(0x02, 0x20);

			s4AF_ReadReg(0x02, &data);

			LOG_INF("Addr : 0x02 , Data : %x\n", data);

			if (data == 0x20 || cnt == 1)
				break;

			cnt++;
		}
	} else if (*g_pAF_Opened == 2) {
		*g_pAF_Opened = 1;
		LOG_INF("reopen driver init\n");
	}
	LOG_INF("-\n");

	return 0;
}

int DW9839AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t
*pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	#if defined(CONFIG_MACH_MT6771)
	g_ACKErrorCnt = 5;
	#else
	g_ACKErrorCnt = 100;
	#endif

	return 1;
}

int DW9839AF_GetFileName(unsigned char *pFileName)
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



