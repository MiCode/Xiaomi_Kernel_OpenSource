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
 * LC898122AF voice coil motor driver
 *
 *
 */

#include "Ois.h"
#include "OisDef.h"
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "LC898122AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x48

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
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

void RegWriteA_LC898122AF(unsigned short RegAddr, unsigned char RegData)
{
	int i4RetValue = 0;
	char puSendCmd[3] = {(char)((RegAddr >> 8) & 0xFF),
			     (char)(RegAddr & 0xFF), RegData};
	/* LOG_INF("I2C w (%x %x)\n", RegAddr, RegData); */

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return;
	}
}

void RegReadA_LC898122AF(unsigned short RegAddr, unsigned char *RegData)
{
	int i4RetValue = 0;
	char pBuff[2] = {(char)(RegAddr >> 8), (char)(RegAddr & 0xFF)};

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, pBuff, 2);
	if (i4RetValue < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8 *)RegData, 1);

	/* LOG_INF("I2C r (%x %x)\n", RegAddr, *RegData); */
	if (i4RetValue != 1) {
		LOG_INF("[CAMERA SENSOR] I2C read failed!!\n");
		return;
	}
}

void RamWriteA_LC898122AF(unsigned short RamAddr, unsigned short RamData)
{
	int i4RetValue = 0;
	char puSendCmd[4] = {
		(char)((RamAddr >> 8) & 0xFF), (char)(RamAddr & 0xFF),
		(char)((RamData >> 8) & 0xFF), (char)(RamData & 0xFF)};
	/* LOG_INF("I2C w2 (%x %x)\n", RamAddr, RamData); */

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 4);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return;
	}
}

void RamReadA_LC898122AF(unsigned short RamAddr, void *ReadData)
{
	int i4RetValue = 0;
	char pBuff[2] = {(char)(RamAddr >> 8), (char)(RamAddr & 0xFF)};
	unsigned short vRcvBuff = 0;
	unsigned long *pRcvBuff;

	pRcvBuff = (unsigned long *)ReadData;

	/* g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1); */

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, pBuff, 2);
	if (i4RetValue < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8 *)&vRcvBuff, 2);
	if (i4RetValue != 2) {
		LOG_INF("[CAMERA SENSOR] I2C read failed!!\n");
		return;
	}
	*pRcvBuff = ((vRcvBuff & 0xFF) << 8) + ((vRcvBuff >> 8) & 0xFF);

	/* LOG_INF("I2C r2 (%x %x)\n", RamAddr, (unsigned int)*pRcvBuff); */
}

void RamWrite32A_LC898122AF(unsigned short RamAddr, unsigned long RamData)
{
	int i4RetValue = 0;
	char puSendCmd[6] = {
		(char)((RamAddr >> 8) & 0xFF),  (char)(RamAddr & 0xFF),
		(char)((RamData >> 24) & 0xFF), (char)((RamData >> 16) & 0xFF),
		(char)((RamData >> 8) & 0xFF),  (char)(RamData & 0xFF)};
	/* LOG_INF("I2C w4 (%x %x)\n", RamAddr, (unsigned int)RamData); */

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 6);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return;
	}
}

void RamRead32A_LC898122AF(unsigned short RamAddr, void *ReadData)
{
	int i4RetValue = 0;
	char pBuff[2] = {(char)(RamAddr >> 8), (char)(RamAddr & 0xFF)};
	unsigned long *pRcvBuff, vRcvBuff = 0;

	pRcvBuff = (unsigned long *)ReadData;

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, pBuff, 2);
	if (i4RetValue < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8 *)&vRcvBuff, 4);
	if (i4RetValue != 4) {
		LOG_INF("[CAMERA SENSOR] I2C read failed!!\n");
		return;
	}
	*pRcvBuff =
		((vRcvBuff & 0xFF) << 24) + (((vRcvBuff >> 8) & 0xFF) << 16) +
		(((vRcvBuff >> 16) & 0xFF) << 8) + (((vRcvBuff >> 24) & 0xFF));

	/* LOG_INF("I2C r4 (%x %x)\n", RamAddr, (unsigned int)*pRcvBuff); */
}

void WitTim_LC898122AF(unsigned short UsWitTim)
{
	msleep(UsWitTim);
}

void LC898prtvalue(unsigned short prtvalue)
{
	LOG_INF("printvalue ======%x\n", prtvalue);
}

static unsigned char s4LC898OTP_ReadReg(unsigned short RegAddr)
{
	int i4RetValue = 0;
	unsigned char pBuff = (unsigned char)RegAddr;
	unsigned char RegData = 0xFF;

	g_pstAF_I2Cclient->addr = (0xA0 >> 1);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, &pBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
		return 0xff;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &RegData, 1);

	LOG_INF("OTPI2C r (%x %x)\n", RegAddr, RegData);
	if (i4RetValue != 1) {
		LOG_INF("[CAMERA SENSOR] I2C read failed!!\n");
		return 0xff;
	}
	return RegData;
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

static void LC898122AF_init_drv(void)
{
	unsigned short addrotp;
	unsigned long dataotp = 0;

	IniSetAf();
	IniSet();
	RamAccFixMod(ON); /* 16bit Fix mode */
	addrotp = 0x30;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x1479,dataotp);  //Hall offset X */

	addrotp = 0x32;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x14F9,dataotp);  //Hall offset Y */

	addrotp = 0x34;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x147A,dataotp);  //Hall bias X */

	addrotp = 0x36;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x14FA,dataotp);  //Hall bias Y */

	addrotp = 0x38;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x1450,dataotp);  //Hall AD offset X */

	addrotp = 0x3A;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x14D0,dataotp);  //Hall AD offset Y */

	addrotp = 0x3C;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x10D3,dataotp);  //Loop gain X */

	addrotp = 0x3E;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWriteA_LC898122AF(0x11D3,dataotp);  //Loop gain Y */

	RamAccFixMod(OFF); /* 32bit Float mode */
	addrotp = 0x44;
	dataotp = s4LC898OTP_ReadReg(addrotp);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RegWriteA_LC898122AF(0x02a0,dataotp);  //Gyro offset X M */
	addrotp = 0x45;
	dataotp = s4LC898OTP_ReadReg(addrotp);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RegWriteA_LC898122AF(0x02a1,dataotp);  //Gyro offset X L */
	addrotp = 0x46;
	dataotp = s4LC898OTP_ReadReg(addrotp);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RegWriteA_LC898122AF(0x02a2,dataotp);  //Gyro offset Y M */
	addrotp = 0x47;
	dataotp = s4LC898OTP_ReadReg(addrotp);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RegWriteA_LC898122AF(0x02a3,dataotp);  //Gyro offset Y L */

	addrotp = 0x48;
	dataotp = s4LC898OTP_ReadReg(addrotp);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RegWriteA_LC898122AF(0x0257,dataotp);//OSC */

	addrotp = 0x49;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 24) +
		  (s4LC898OTP_ReadReg(addrotp + 1) << 16) +
		  (s4LC898OTP_ReadReg(addrotp + 2) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 3);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWrite32A_LC898122AF(0x1020,dataotp);  //Gyro gain X */

	addrotp = 0x4D;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 24) +
		  (s4LC898OTP_ReadReg(addrotp + 1) << 16) +
		  (s4LC898OTP_ReadReg(addrotp + 2) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 3);
	LOG_INF("[OTP]0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	/* RamWrite32A_LC898122AF(0x1120,dataotp);  //Gyro gain Y */

	RamWriteA_LC898122AF(TCODEH, 100); /* focus position */
	RtnCen(0);
	msleep(100);
	SetPanTiltMode(ON);
	msleep(20);
	OisEna();
	SetH1cMod(MOVMODE); /* movie mode */
	/* SetH1cMod(0);          //still mode */

	addrotp = 0x20;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]AF start current 0x%x 0x%x\n", addrotp,
		(unsigned int)dataotp);
	LOG_INF("[OTP]AF start current 0x%x 0x%x\n", addrotp,
		(unsigned int)dataotp);
	LOG_INF("[OTP]AF start current 0x%x 0x%x\n", addrotp,
		(unsigned int)dataotp);

	addrotp = 0x22;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]AF Infinit 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	LOG_INF("[OTP]AF Infinit 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	LOG_INF("[OTP]AF Infinit 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);

	addrotp = 0x24;
	dataotp = (s4LC898OTP_ReadReg(addrotp) << 8) +
		  s4LC898OTP_ReadReg(addrotp + 1);
	LOG_INF("[OTP]AF Macro 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	LOG_INF("[OTP]AF Macro 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	LOG_INF("[OTP]AF Macro 0x%x 0x%x\n", addrotp, (unsigned int)dataotp);
	LOG_INF("LC898122AF_Open - End\n");
}

static inline int moveAF(unsigned long a_u4Position)
{
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {

		LC898122AF_init_drv();

		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = 0;
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	RamWriteA_LC898122AF(TCODEH, g_u4TargetPosition);

	spin_lock(g_pAF_SpinLock);
	g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
	spin_unlock(g_pAF_SpinLock);

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
long LC898122AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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
int LC898122AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		RamWriteA_LC898122AF(TCODEH, 100); /* focus position */
		msleep(20);
		RamWriteA_LC898122AF(TCODEH, 50); /* focus position */
		msleep(20);

		RtnCen(0);
		SrvCon(X_DIR, OFF);
		SrvCon(Y_DIR, OFF);
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

int LC898122AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			    spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}
