// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



/*
 * LC898212XDAF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#include "AfInit.h"
#include "AfSTMV.h"

#define AF_DRVNAME "LC898212XDAF_DRV"
#define AF_I2C_SLAVE_ADDR 0xE4
#define EEPROM_I2C_SLAVE_ADDR_OV23850 0xA8
#define EEPROM_I2C_SLAVE_ADDR_IMX258 0xA0

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

static unsigned int g_SelectEEPROM;
unsigned int g_LC898212_SearchDir;

#define Min_Pos 0
#define Max_Pos 1023

/* LiteOn : Hall calibration range : 0xA800 - 0x5800 */
static signed short Hall_Max =
	0x5800; /* Please read INF position from EEPROM or OTP */
static signed short Hall_Min =
	0xA800; /* Please read MACRO position from EEPROM or OTP */

int s4AF_ReadReg_LC898212XDAF(u8 *a_pSendData, u16 a_sizeSendData,
			      u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId)
{
	int i4RetValue = 0;

	g_pstAF_I2Cclient->addr = i2cId >> 1;

	i4RetValue =
		i2c_master_send(g_pstAF_I2Cclient, a_pSendData, a_sizeSendData);

	if (i4RetValue != a_sizeSendData) {
		LOG_INF("I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8 *)a_pRecvData,
				     a_sizeRecvData);

	if (i4RetValue != a_sizeRecvData) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	return 0;
}

int s4AF_WriteReg_LC898212XDAF(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId)
{
	int i4RetValue = 0;

	g_pstAF_I2Cclient->addr = i2cId >> 1;

	i4RetValue =
		i2c_master_send(g_pstAF_I2Cclient, a_pSendData, a_sizeSendData);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!, Addr = 0x%x, Data = 0x%x\n",
			a_pSendData[0], a_pSendData[1]);
		return -1;
	}

	return 0;
}

static int s4EEPROM_ReadReg_LC898212XDAF_OV23850(u16 addr, u8 *data)
{
	int i4RetValue = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};

	i4RetValue =
		s4AF_ReadReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), data, 1,
					  EEPROM_I2C_SLAVE_ADDR_OV23850);
	if (i4RetValue < 0)
		LOG_INF("I2C read e2prom failed!!\n");

	return i4RetValue;
}

static int s4EEPROM_ReadReg_LC898212XDAF_IMX258(u16 addr, u8 *data)
{
	int i4RetValue = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};

	i4RetValue =
		s4AF_ReadReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd), data, 1,
					  EEPROM_I2C_SLAVE_ADDR_IMX258);
	if (i4RetValue < 0)
		LOG_INF("I2C read e2prom failed!!\n");

	return i4RetValue;
}

static void s4AF_WriteReg(unsigned short addr, unsigned char data)
{
	u8 puSendCmd[2] = {(u8)(addr & 0xFF), (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XDAF(puSendCmd, sizeof(puSendCmd),
				   AF_I2C_SLAVE_ADDR);
}

static int convertAF_DAC(short ReadData)
{
	int DacVal = ((ReadData - Hall_Min) * (Max_Pos - Min_Pos)) /
			     ((unsigned short)(Hall_Max - Hall_Min)) +
		     Min_Pos;

	return DacVal;
}

static void LC898212XD_init(void)
{
	struct stSmvPar StSmvPar;
	u8 val1 = 0, val2 = 0;

	int Hall_Off = 0x80;  /* Please Read Offset from EEPROM or OTP */
	int Hall_Bias = 0x80; /* Please Read Bias from EEPROM or OTP */

	unsigned short HallMinCheck = 0;
	unsigned short HallMaxCheck = 0;

	g_SelectEEPROM = 1;

	if (s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x001A, &val1) < 0)
		g_SelectEEPROM = 0;

	g_LC898212_SearchDir = 1;

	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F63, &val2);
	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F64, &val1);
	HallMinCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F65, &val2);
	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F66, &val1);
	HallMaxCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F67, &val1);
	s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0F68, &val2);

	if ((val1 != 0) && (val2 != 0) &&
	    (HallMaxCheck >= 0x1FFF && HallMaxCheck <= 0x7FFF) &&
	    (HallMinCheck >= 0x8001 && HallMinCheck <= 0xEFFF)) {

		Hall_Min = HallMinCheck;
		Hall_Max = HallMaxCheck;

		Hall_Off = val1;
		Hall_Bias = val2;
	}

	/* Range Protection : Min = 0x8001 , Max = 0x7FFF */
	if (!(Hall_Max >= 0 && Hall_Max <= 0x7FFF)) {
		signed short Temp;

		Temp = Hall_Min;
		Hall_Min = Hall_Max;
		Hall_Max = Temp;
	}

	LOG_INF("hallmax:0x%x, hallmin:0x%x, halloff:0x%x, hallbias:0x%x\n",
		Hall_Max, Hall_Min, Hall_Off, Hall_Bias);

	/* Wake up */
	s4AF_WriteReg(0x80, 0x68);
	s4AF_WriteReg(0x80, 0x64);
	s4AF_WriteReg(0x95, 0x00);

	AfInit(Hall_Off, Hall_Bias); /* Initialize driver IC */

	/* Step move parameter set */
	StSmvPar.UsSmvSiz = STMV_SIZE;
	StSmvPar.UcSmvItv = STMV_INTERVAL;
	StSmvPar.UcSmvEnb = STMCHTG_SET | STMSV_SET | STMLFF_SET;
	StmvSet(StSmvPar);

	ServoOn(); /* Close loop ON */
}

static unsigned short AF_convert(int position)
{
	if (g_LC898212_SearchDir == 0) {
		return (((position - Min_Pos) *
			 (unsigned short)(Hall_Max - Hall_Min) /
			 (Max_Pos - Min_Pos)) +
			Hall_Min) &
		       0xFFFF;
	} else {
		return (((Max_Pos - position) *
			 (unsigned short)(Hall_Max - Hall_Min) /
			 (Max_Pos - Min_Pos)) +
			Hall_Min) &
		       0xFFFF;
	}
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

		LC898212XD_init();

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

	if ((StmvTo(AF_convert((int)a_u4Position)) & 0x1) == 0) {
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

static inline int getAFCalPos(__user struct stAF_MotorCalPos *pstMotorCalPos)
{
	struct stAF_MotorCalPos stMotorCalPos;
	u32 u4AF_CalibData_INF;
	u32 u4AF_CalibData_MACRO;
	u8 val1;

	u4AF_CalibData_INF = 0;
	u4AF_CalibData_MACRO = 0;

	g_SelectEEPROM = 1;

	if (s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x001A, &val1) < 0)
		g_SelectEEPROM = 0;

	if (g_SelectEEPROM == 1) {
		u8 val1 = 0, val2 = 0;
		int AF_Infi = 0x00;
		int AF_Marco = 0x00;

		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0011,
						      &val2); /* low byte */
		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0012, &val1);
		AF_Infi = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0013, &val2);
		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0014, &val1);
		AF_Marco = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0F67, &val1);
		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0F68, &val2);
		Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0F69, &val1);
		s4EEPROM_ReadReg_LC898212XDAF_OV23850(0x0F70, &val2);
		Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		if (!(Hall_Max >= 0 && Hall_Max <= 0x7FFF)) {
			signed short Temp;

			Temp = Hall_Min;
			Hall_Min = Hall_Max;
			Hall_Max = Temp;
		}

		if (AF_Marco > 1023 || AF_Infi > 1023 || AF_Infi > AF_Marco) {
			u4AF_CalibData_INF = convertAF_DAC(AF_Infi);
			u4AF_CalibData_MACRO = convertAF_DAC(AF_Marco);
		}
	} else {
		u8 val1 = 0, val2 = 0;
		int AF_Infi = 0x00;
		int AF_Marco = 0x00;

		s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0011,
						     &val2); /* low byte */
		s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0012, &val1);
		AF_Infi = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0013, &val2);
		s4EEPROM_ReadReg_LC898212XDAF_IMX258(0x0014, &val1);
		AF_Marco = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		LOG_INF("AF_Infi : %x\n", AF_Infi);
		LOG_INF("AF_Marco : %x\n", AF_Marco);

		Hall_Min = 0x8001;
		Hall_Max = 0x7FFF;

		if (AF_Marco > 1023 || AF_Infi > 1023 || AF_Infi > AF_Marco) {
			u4AF_CalibData_INF = convertAF_DAC(AF_Infi);
			u4AF_CalibData_MACRO = convertAF_DAC(AF_Marco);

			if (u4AF_CalibData_INF > u4AF_CalibData_MACRO) {
				u4AF_CalibData_INF = 1023 - u4AF_CalibData_INF;
				u4AF_CalibData_MACRO =
					1023 - u4AF_CalibData_MACRO;
			}
		}
	}

	LOG_INF("AF_CalibData_INF : %d\n", u4AF_CalibData_INF);
	LOG_INF("AF_CalibData_MACRO : %d\n", u4AF_CalibData_MACRO);

	if (u4AF_CalibData_INF > 0 && u4AF_CalibData_MACRO < 1024 &&
	    u4AF_CalibData_INF < u4AF_CalibData_MACRO) {
		stMotorCalPos.u4MacroPos = u4AF_CalibData_MACRO;
		stMotorCalPos.u4InfPos = u4AF_CalibData_INF;
	} else {
		stMotorCalPos.u4MacroPos = 0;
		stMotorCalPos.u4InfPos = 0;
	}

	if (copy_to_user(pstMotorCalPos, &stMotorCalPos, sizeof(stMotorCalPos)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long LC898212XDAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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

	case AFIOC_G_MOTORCALPOS:
		i4RetValue = getAFCalPos(
			(__user struct stAF_MotorCalPos *)(a_u4Param));
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
int LC898212XDAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");

		/* Sleep In */
		s4AF_WriteReg(0x95, 0x80);
		s4AF_WriteReg(0x80, 0x68);
		s4AF_WriteReg(0x80, 0x69);

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

int LC898212XDAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			      spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int LC898212XDAF_GetFileName(unsigned char *pFileName)
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
