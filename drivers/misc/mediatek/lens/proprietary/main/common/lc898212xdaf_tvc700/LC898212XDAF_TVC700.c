// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



/*
 * LC898212XD_TVC700 voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "212XD_TVC700_DRV"
#define AF_I2C_SLAVE_ADDR 0xE4
#define EEPROM_I2C_SLAVE_ADDR 0xA0

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

#define Min_Pos 0
#define Max_Pos 1023

/* LiteOn : Hall calibration range : 0xA800 - 0x5800 */
static signed short Hall_Max =
	0x5800; /* Please read INF position from EEPROM or OTP */
static signed short Hall_Min =
	0xA800; /* Please read MACRO position from EEPROM or OTP */

static int s4AF_ReadReg_LC898212XD_TVC820(u8 *a_pSendData, u16 a_sizeSendData,
					  u8 *a_pRecvData, u16 a_sizeRecvData,
					  u16 i2cId)
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

static int s4AF_WriteReg_LC898212XD_TVC820(u8 *a_pSendData, u16 a_sizeSendData,
					   u16 i2cId)
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

/* ////////////////////////////////////////////////////////////// */

#define INI_MSSET_211 ((unsigned char)0x00)
#define CHTGX_THRESHOLD ((unsigned short)0x0200)
#define CHTGOKN_TIME ((unsigned char)0x80)
#define CHTGOKN_WAIT 3

#define STMV_SIZE ((unsigned short)0x0180)
#define STMV_INTERVAL ((unsigned char)0x01)

#define STMCHTG_ON ((unsigned char)0x08)
#define STMSV_ON ((unsigned char)0x04)
#define STMLFF_ON ((unsigned char)0x02)
#define STMVEN_ON ((unsigned char)0x01)
#define STMCHTG_OFF ((unsigned char)0x00)
#define STMSV_OFF ((unsigned char)0x00)
#define STMLFF_OFF ((unsigned char)0x00)
#define STMVEN_OFF ((unsigned char)0x00)

#define STMCHTG_SET STMCHTG_ON
#define STMSV_SET STMSV_ON
#define STMLFF_SET STMLFF_OFF

struct stSmvPar {
	unsigned short UsSmvSiz;
	unsigned char UcSmvItv;
	unsigned char UcSmvEnb;
};

/* *********************************************************** */
/* LC89821x Initialize Data Table */
/* *********************************************************** */

#define WAIT 0xFF /* Wait command */

struct INIDATAT {
	unsigned short addr;
	unsigned short data;
} IniDataT;

/* Camera Module Big */
/* 2L7 + LC898212XD_TVC700 */
static const struct INIDATAT Init_Table_TVC820[] = {
	/* Addr,   Data */

	/* TDK_CL-ACT_ 212_SPtype_Ini_130114 */
	{0x0080, 0x34}, /* CLKSEL 1/1, CLKON */
	{0x0081, 0x20}, /* AD 4Time */
	{0x0084, 0xE0}, /* STBY   AD ON,DA ON,OP ON */
	{0x0087, 0x05}, /* PIDSW OFF,AF ON,MS2 ON */
	{0x00A4, 0x24}, /* Internal OSC Setup (No01=24.18MHz) */

	{0x003A, 0x0000}, /* OFFSET Clear */
	{0x0004, 0x0000}, /* RZ Clear(Target Value) */
	{0x0002, 0x0000}, /* PIDZO Clear */
	{0x0018, 0x0000}, /* MS1Z22 Clear(STMV Target Value) */

	/* Filter Setting: ST140911-1.h For TVC-820 */
	{0x0088, 0x70},
	{0x0028, 0x8080},
	{0x004C, 0x4000},
	{0x0083, 0x2C},
	{0x0085, 0xC0},
	{WAIT, 1}, /* Wait 1 ms */

	/* {0x0085, 0x00}, */
	{0x0084, 0xE3},
	{0x0097, 0x00},
	{0x0098, 0x42},
	{0x0099, 0x00},
	{0x009A, 0x00},

	{0x0086, 0x40},
	{0x0040, 0x4030},
	{0x0042, 0x7150},
	{0x0044, 0x8F90},
	{0x0046, 0x61B0},
	{0x0048, 0x7FF0},
	{0x004A, 0x3930},
	{0x004C, 0x4030},
	{0x004E, 0x8010},
	{0x0050, 0x04f0},
	{0x0052, 0x7610},
	{0x0054, 0x2030},
	{0x0056, 0x0000},
	{0x0058, 0x7FF0},
	{0x005A, 0x0680},
	{0x005C, 0x72f0},
	{0x005E, 0x7f70},
	{0x0060, 0x7ed0},
	{0x0062, 0x7ff0},
	{0x0064, 0x0000},
	{0x0066, 0x0000},
	{0x0068, 0x5130},
	{0x006A, 0x72f0},
	{0x006C, 0x8010},
	{0x006E, 0x0000},
	{0x0070, 0x0000},
	{0x0072, 0x18e0},
	{0x0074, 0x4e30},
	{0x0030, 0x0000},
	{0x0076, 0x0C50},
	{0x0078, 0x4000},
	{WAIT, 5}, /* Wait 5 ms */

	{0x0086, 0x60},

	{0x0087, 0x85}

};

#define ADHXI_211H 0x00
#define ADHXI_211L 0x01
#define PIDZO_211H 0x02
#define PIDZO_211L 0x03
#define RZ_211H 0x04
#define RZ_211L 0x05
#define DZ1_211H 0x06
#define DZ1_211L 0x07
#define DZ2_211H 0x08
#define DZ2_211L 0x09
#define UZ1_211H 0x0A
#define UZ1_211L 0x0B
#define UZ2_211H 0x0C
#define UZ2_211L 0x0D
#define IZ1_211H 0x0E
#define IZ1_211L 0x0F
#define IZ2_211H 0x10
#define IZ2_211L 0x11
#define MS1Z01_211H 0x12
#define MS1Z01_211L 0x13
#define MS1Z11_211H 0x14
#define MS1Z11_211L 0x15
#define MS1Z12_211H 0x16
#define MS1Z12_211L 0x17
#define MS1Z22_211H 0x18
#define MS1Z22_211L 0x19
#define MS2Z01_211H 0x1A
#define MS2Z01_211L 0x1B
#define MS2Z11_211H 0x1C
#define MS2Z11_211L 0x1D
#define MS2Z12_211H 0x1E
#define MS2Z12_211L 0x1F
#define MS2Z22_211H 0x20
#define MS2Z22_211L 0x21
#define MS2Z23_211H 0x22
#define MS2Z23_211L 0x23
#define OZ1_211H 0x24
#define OZ1_211L 0x25
#define OZ2_211H 0x26
#define OZ2_211L 0x27
#define DAHLXO_211H 0x28
#define DAHLXO_211L 0x29
#define OZ3_211H 0x2A
#define OZ3_211L 0x2B
#define OZ4_211H 0x2C
#define OZ4_211L 0x2D
#define OZ5_211H 0x2E
#define OZ5_211L 0x2F
#define oe_211H 0x30
#define oe_211L 0x31
#define MSR1CMAX_211H 0x32
#define MSR1CMAX_211L 0x33
#define MSR1CMIN_211H 0x34
#define MSR1CMIN_211L 0x35
#define MSR2CMAX_211H 0x36
#define MSR2CMAX_211L 0x37
#define MSR2CMIN_211H 0x38
#define MSR2CMIN_211L 0x39
#define OFFSET_211H 0x3A
#define OFFSET_211L 0x3B
#define ADOFFSET_211H 0x3C
#define ADOFFSET_211L 0x3D
#define EZ_211H 0x3E
#define EZ_211L 0x3F

#define ag_211H 0x40
#define ag_211L 0x41
#define da_211H 0x42
#define da_211L 0x43
#define db_211H 0x44
#define db_211L 0x45
#define dc_211H 0x46
#define dc_211L 0x47
#define dg_211H 0x48
#define dg_211L 0x49
#define pg_211H 0x4A
#define pg_211L 0x4B
#define gain1_211H 0x4C
#define gain1_211L 0x4D
#define gain2_211H 0x4E
#define gain2_211L 0x4F
#define ua_211H 0x50
#define ua_211L 0x51
#define uc_211H 0x52
#define uc_211L 0x53
#define ia_211H 0x54
#define ia_211L 0x55
#define ib_211H 0x56
#define ib_211L 0x57
#define ic_211H 0x58
#define ic_211L 0x59
#define ms11a_211H 0x5A
#define ms11a_211L 0x5B
#define ms11c_211H 0x5C
#define ms11c_211L 0x5D
#define ms12a_211H 0x5E
#define ms12a_211L 0x5F
#define ms12c_211H 0x60
#define ms12c_211L 0x61
#define ms21a_211H 0x62
#define ms21a_211L 0x63
#define ms21b_211H 0x64
#define ms21b_211L 0x65
#define ms21c_211H 0x66
#define ms21c_211L 0x67
#define ms22a_211H 0x68
#define ms22a_211L 0x69
#define ms22c_211H 0x6A
#define ms22c_211L 0x6B
#define ms22d_211H 0x6C
#define ms22d_211L 0x6D
#define ms22e_211H 0x6E
#define ms22e_211L 0x6F
#define ms23p_211H 0x70
#define ms23p_211L 0x71
#define oa_211H 0x72
#define oa_211L 0x73
#define oc_211H 0x74
#define oc_211L 0x75
#define PX12_211H 0x76
#define PX12_211L 0x77
#define PX3_211H 0x78
#define PX3_211L 0x79
#define MS2X_211H 0x7A
#define MS2X_211L 0x7B
#define CHTGX_211H 0x7C
#define CHTGX_211L 0x7D
#define CHTGN_211H 0x7E
#define CHTGN_211L 0x7F

#define CLKSEL_211 0x80
#define ADSET_211 0x81
#define PWMSEL_211 0x82
#define SWTCH_211 0x83
#define STBY_211 0x84
#define CLR_211 0x85
#define DSSEL_211 0x86
#define ENBL_211 0x87
#define ANA1_211 0x88
#define AFSEND_211 0x8A
#define STMVEN_211 0x8A
#define STPT_211 0x8B
#define SWFC_211 0x8C
#define SWEN_211 0x8D
#define MSNUM_211 0x8E
#define MSSET_211 0x8F
#define DLYMON_211 0x90
#define MONA_211 0x91
#define PWMLIMIT_211 0x92
#define PINSEL_211 0x93
#define PWMSEL2_211 0x94
#define SFTRST_211 0x95
#define TEST_211 0x96
#define PWMZONE2_211 0x97
#define PWMZONE1_211 0x98
#define PWMZONE0_211 0x99
#define ZONE3_211 0x9A
#define ZONE2_211 0x9B
#define ZONE1_211 0x9C
#define ZONE0_211 0x9D
#define GCTIM_211 0x9E
#define GCTIM_211NU 0x9F
#define STMINT_211 0xA0
#define STMVENDH_211 0xA1
#define STMVENDL_211 0xA2
#define MSNUMR_211 0xA3
#define ANA2_211 0xA4

/* ////////////////////////////////////////////////////////////// */

/****************************/
/*	Definations         */
/****************************/
#define ABS_STMV(x) ((x) < 0 ? -(x) : (x))
#define LC898211_fs 234375

#ifdef DEBUG_LOG
#define AF_REGDUMP "REGDUMP"
#define LOG_INF(format, args...) pr_info(AF_REGDUMP " " format, ##args)
#endif

#define REG_ADDR_START 0x80 /* REG Start address */
#define DeviceAddr 0xE4     /* Device address of driver IC */

static struct stSmvPar StSmvPar;

static void RamWriteA(unsigned short addr, unsigned short data)
{
	u8 puSendCmd[3] = {(u8)(addr & 0xFF), (u8)(data >> 8),
			   (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XD_TVC820(puSendCmd, sizeof(puSendCmd),
					DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("RAMW\t%x\t%x\n", addr, data);
#endif
}

static void RamReadA(unsigned short addr, unsigned short *data)
{
	u8 buf[2];
	u8 puSendCmd[1] = {(u8)(addr & 0xFF)};

	s4AF_ReadReg_LC898212XD_TVC820(puSendCmd, sizeof(puSendCmd), buf, 2,
				       DeviceAddr);
	*data = (buf[0] << 8) | (buf[1] & 0x00FF);

#ifdef DEBUG_LOG
	LOG_INF("RAMR\t%x\t%x\n", addr, *data);
#endif
}

static void RegWriteA(unsigned short addr, unsigned char data)
{
	u8 puSendCmd[2] = {(u8)(addr & 0xFF), (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XD_TVC820(puSendCmd, sizeof(puSendCmd),
					DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("REGW\t%x\t%x\n", addr, data);
#endif
}

static void RegReadA(unsigned short addr, unsigned char *data)
{
	u8 puSendCmd[1] = {(u8)(addr & 0xFF)};

	s4AF_ReadReg_LC898212XD_TVC820(puSendCmd, sizeof(puSendCmd), data, 1,
				       DeviceAddr);

#ifdef DEBUG_LOG
	LOG_INF("REGR\t%x\t%x\n", addr, *data);
#endif
}

static void WaitTime(unsigned short msec)
{
	usleep_range(msec * 1000, (msec + 1) * 1000);
}

static void StmvSet(struct stSmvPar StSetSmv)
{
	unsigned char UcSetEnb;
	unsigned char UcSetSwt;
	unsigned short UsParSiz;
	unsigned char UcParItv;
	short SsParStt; /* StepMove Start Position */

	StSmvPar.UsSmvSiz = StSetSmv.UsSmvSiz;
	StSmvPar.UcSmvItv = StSetSmv.UcSmvItv;
	StSmvPar.UcSmvEnb = StSetSmv.UcSmvEnb;

	RegWriteA(AFSEND_211, 0x00); /* StepMove Enable Bit Clear */

	RegReadA(ENBL_211, &UcSetEnb);
	UcSetEnb &= (unsigned char)0xFD;
	RegWriteA(ENBL_211, UcSetEnb); /* Measuremenet Circuit1 Off */

	RegReadA(SWTCH_211, &UcSetSwt);
	UcSetSwt &= (unsigned char)0x7F;
	RegWriteA(SWTCH_211, UcSetSwt); /* RZ1 Switch Cut Off */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt); /* Get Start Position */
	UsParSiz = StSetSmv.UsSmvSiz;			/* Get StepSize */
	UcParItv = StSetSmv.UcSmvItv;			/* Get StepInterval */

	RamWriteA(ms11a_211H, (unsigned short)0x0800);
	RamWriteA(MS1Z22_211H,
		  (unsigned short)SsParStt); /* Set Start Position */
	RamWriteA(MS1Z12_211H, UsParSiz);    /* Set StepSize */
	RegWriteA(STMINT_211, UcParItv);     /* Set StepInterval */

	UcSetSwt |= (unsigned char)0x80;
	RegWriteA(SWTCH_211, UcSetSwt); /* RZ1 Switch ON */
}

static unsigned char StmvTo(short SsSmvEnd)
{
	unsigned short UsSmvDpl;
	short SsParStt; /* StepMove Start Position */

	/* PIOA_SetOutput(_PIO_PA29);   // Monitor I/O Port */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt); /* Get Start Position */
	UsSmvDpl = ABS_STMV(SsParStt - SsSmvEnd);

	if ((UsSmvDpl <= StSmvPar.UsSmvSiz) &&
	    ((StSmvPar.UcSmvEnb & STMSV_ON) == STMSV_ON)) {
		if (StSmvPar.UcSmvEnb & STMCHTG_ON)
			RegWriteA(MSSET_211,
				  INI_MSSET_211 | (unsigned char)0x01);

		RamWriteA(MS1Z22_211H,
			  SsSmvEnd); /* Handling Single Step For ES1 */
		StSmvPar.UcSmvEnb |= STMVEN_ON;
	} else {
		if (SsParStt < SsSmvEnd) { /* Check StepMove Direction */
			RamWriteA(MS1Z12_211H, StSmvPar.UsSmvSiz);
		} else if (SsParStt > SsSmvEnd) {
			RamWriteA(MS1Z12_211H, -StSmvPar.UsSmvSiz);
		}

		RamWriteA(STMVENDH_211,
			  SsSmvEnd); /* Set StepMove Target Position */
		StSmvPar.UcSmvEnb |= STMVEN_ON;
		RegWriteA(STMVEN_211, StSmvPar.UcSmvEnb); /* Start StepMove */
	}

	return 0;
}

static void AfInit(unsigned char hall_off, unsigned char hall_bias)
{
	unsigned int DataLen;
	unsigned short i;
	unsigned short pos;

	/* IMX318, IMX230, OV23850 */
	DataLen = sizeof(Init_Table_TVC820) / sizeof(IniDataT);

	for (i = 0; i < DataLen; i++) {
		if (Init_Table_TVC820[i].addr == WAIT) {
			WaitTime(Init_Table_TVC820[i].data);
			continue;
		}

		if (Init_Table_TVC820[i].addr >= REG_ADDR_START)
			RegWriteA(Init_Table_TVC820[i].addr,
				  (unsigned char)(Init_Table_TVC820[i].data &
						  0x00ff));
		else
			RamWriteA(Init_Table_TVC820[i].addr,
				  (unsigned short)Init_Table_TVC820[i].data);
	}

	RegWriteA(0x28, hall_off);  /* Hall Offset */
	RegWriteA(0x29, hall_bias); /* Hall Bias */

	RamReadA(0x3C, &pos);
	RamWriteA(0x04, pos); /* Direct move target position */
	RamWriteA(0x18, pos); /* Step move start position */

	/* WaitTime(5); */
	/* RegWriteA( 0x87, 0x85 );              // Servo ON */
}

static void ServoOn(void)
{
	RegWriteA(0x85, 0x80); /* Clear PID Ram Data */
	WaitTime(1);
	RegWriteA(0x87, 0x85); /* Servo ON */
}

/* ////////////////////////////////////////////////////////////// */

static int s4EEPROM_ReadReg(u16 addr, u8 *data)
{
	int i4RetValue = 0;

	u8 puSendCmd[2] = {(u8)(addr >> 8), (u8)(addr & 0xFF)};

	i4RetValue = s4AF_ReadReg_LC898212XD_TVC820(
		puSendCmd, sizeof(puSendCmd), data, 1, EEPROM_I2C_SLAVE_ADDR);
	if (i4RetValue < 0)
		LOG_INF("I2C read e2prom failed!!\n");

	return i4RetValue;
}

static void s4AF_WriteReg(unsigned short addr, unsigned char data)
{
	u8 puSendCmd[2] = {(u8)(addr & 0xFF), (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XD_TVC820(puSendCmd, sizeof(puSendCmd),
					AF_I2C_SLAVE_ADDR);
}

static void LC898212XD_init(void)
{
	struct stSmvPar StSmvPar;
	u8 val1 = 0, val2 = 0;

	int Hall_Off = 0x80;  /* Please Read Offset from EEPROM or OTP */
	int Hall_Bias = 0x80; /* Please Read Bias from EEPROM or OTP */

	s4EEPROM_ReadReg(0xF67, &val1);
	s4EEPROM_ReadReg(0xF68, &val2);

	Hall_Off = val1;
	Hall_Bias = val2;

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
	/* 1: INF -> Macro =  0x8001 -> 0x7FFF */
	return (((position - Min_Pos) * (unsigned short)(Hall_Max - Hall_Min) /
		 (Max_Pos - Min_Pos)) +
		Hall_Min) &
	       0xFFFF;
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

	stMotorCalPos.u4MacroPos = 0;
	stMotorCalPos.u4InfPos = 0;

	if (copy_to_user(pstMotorCalPos, &stMotorCalPos, sizeof(stMotorCalPos)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long LC898212XD_TVC700_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
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
int LC898212XD_TVC700_Release(struct inode *a_pstInode, struct file *a_pstFile)
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

int LC898212XD_TVC700_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int LC898212XD_TVC700_GetFileName(unsigned char *pFileName)
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
