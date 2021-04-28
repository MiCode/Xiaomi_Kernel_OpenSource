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

#include <linux/delay.h>
#include "LC89821x_STMV.h"

#ifdef DEBUG_LOG
#include <linux/fs.h>
#endif

#ifdef DEBUG_LOG
#define AF_REGDUMP "REGDUMP"
#define LOG_INF(format, args...) pr_info(AF_REGDUMP " " format, ##args)
#endif
/* /////////////////////////////////////// */

#define	ABS_STMV(x)	((x) < 0 ? -(x) : (x))

#define		DeviceAddr		0xE4

static void RamWriteA(unsigned short addr, unsigned short data)
{
	u8 puSendCmd[3] = {(u8)(addr & 0xFF), (u8)(data >> 8),
			   (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XDAF_F(puSendCmd, sizeof(puSendCmd), DeviceAddr);

	#ifdef DEBUG_LOG
	LOG_INF("RAMW\t%x\t%x\n", addr, data);
	#endif
}

static void RamReadA(unsigned short addr, unsigned short *data)
{
	u8 buf[2];
	u8 puSendCmd[1] = { (u8)(addr & 0xFF)};

	s4AF_ReadReg_LC898212XDAF_F(puSendCmd, sizeof(puSendCmd), buf, 2,
				    DeviceAddr);
	*data = (buf[0] << 8) | (buf[1] & 0x00FF);

	#ifdef DEBUG_LOG
	LOG_INF("RAMR\t%x\t%x\n", addr, *data);
	#endif
}

static void RegWriteA(unsigned short addr, unsigned char data)
{
	u8 puSendCmd[2] = {(u8)(addr & 0xFF), (u8)(data & 0xFF)};

	s4AF_WriteReg_LC898212XDAF_F(puSendCmd, sizeof(puSendCmd), DeviceAddr);

	#ifdef DEBUG_LOG
	LOG_INF("REGW\t%x\t%x\n", addr, data);
	#endif
}

static void RegReadA(unsigned short addr, unsigned char *data)
{
	u8 puSendCmd[1] = {(u8)(addr & 0xFF) };

	s4AF_ReadReg_LC898212XDAF_F(puSendCmd, sizeof(puSendCmd), data, 1,
				    DeviceAddr);

	#ifdef DEBUG_LOG
	LOG_INF("REGR\t%x\t%x\n", addr, *data);
	#endif
}

static void WaitTime(unsigned short msec)
{
	    usleep_range(msec * 1000, (msec + 1) * 1000);
}

/* /////////////////////////////////////// */

/* ************************** */
/* Definations */
/* ************************** */

#define		REG_ADDR_START		0x80	/* REG Start address */

static struct stSmvPar StSmvPar;

static void StmvSet(struct stSmvPar StSetSmv)
{
	unsigned char UcSetEnb;
	unsigned char UcSetSwt;
	unsigned short UsParSiz;
	unsigned char UcParItv;
	short SsParStt;		/* StepMove Start Position */

	StSmvPar.UsSmvSiz = StSetSmv.UsSmvSiz;
	StSmvPar.UcSmvItv = StSetSmv.UcSmvItv;
	StSmvPar.UcSmvEnb = StSetSmv.UcSmvEnb;

	RegWriteA(AFSEND_211, 0x00);	/* StepMove Enable Bit Clear */

	RegReadA(ENBL_211, &UcSetEnb);
	UcSetEnb &= (unsigned char)0xFD;
	RegWriteA(ENBL_211, UcSetEnb);	/* Measuremenet Circuit1 Off */

	RegReadA(SWTCH_211, &UcSetSwt);
	UcSetSwt &= (unsigned char)0x7F;
	RegWriteA(SWTCH_211, UcSetSwt);	/* RZ1 Switch Cut Off */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt);	/* Get Start Position */
	UsParSiz = StSetSmv.UsSmvSiz;	/* Get StepSize */
	UcParItv = StSetSmv.UcSmvItv;	/* Get StepInterval */

	RamWriteA(ms11a_211H, (unsigned short)0x0800);
	RamWriteA(MS1Z22_211H, (unsigned short)SsParStt);
	RamWriteA(MS1Z12_211H, UsParSiz);	/* Set StepSize */
	RegWriteA(STMINT_211, UcParItv);	/* Set StepInterval */

	 UcSetSwt |= (unsigned char)0x80;
	RegWriteA(SWTCH_211, UcSetSwt);	/* RZ1 Switch ON */
}

static unsigned char StmvTo(short SsSmvEnd)
{
	unsigned short UsSmvDpl;
	short SsParStt;		/* StepMove Start Position */

	/* PIOA_SetOutput(_PIO_PA29);   // Monitor I/O Port */

	RamReadA(RZ_211H, (unsigned short *)&SsParStt);	/* Get Start Position */
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
		if (SsParStt < SsSmvEnd) {	/* Check StepMove Direction */
			RamWriteA(MS1Z12_211H, StSmvPar.UsSmvSiz);
		} else if (SsParStt > SsSmvEnd) {
			RamWriteA(MS1Z12_211H, -StSmvPar.UsSmvSiz);
		}

		RamWriteA(STMVENDH_211,
			  SsSmvEnd); /* Set StepMove Target Position */
		StSmvPar.UcSmvEnb |= STMVEN_ON;
		RegWriteA(STMVEN_211, StSmvPar.UcSmvEnb);
	}

	return 0;
}

static void AfInit(unsigned char hall_off, unsigned char hall_bias)
{
	unsigned int DataLen;
	unsigned short i;
	unsigned short pos;

	/* IMX318, IMX230, OV23850 */
	DataLen = sizeof(Init_Table_F) / sizeof(IniData_F);

	for (i = 0; i < DataLen; i++) {
		if (Init_Table_F[i].addr == WAIT) {
			WaitTime(Init_Table_F[i].data);
			continue;
		}

		if (Init_Table_F[i].addr >= REG_ADDR_START)
			RegWriteA(
				Init_Table_F[i].addr,
				(unsigned char)(Init_Table_F[i].data & 0x00ff));
		else
			RamWriteA(Init_Table_F[i].addr,
				(unsigned short)Init_Table_F[i].data);
	}

	RegWriteA(0x28, hall_off);	/* Hall Offset */
	RegWriteA(0x29, hall_bias);	/* Hall Bias */

	RamReadA(0x3C, &pos);
	RamWriteA(0x04, pos);	/* Direct move target position */
	RamWriteA(0x18, pos);	/* Step move start position */

	/* WaitTime(5); */
	/* RegWriteA( 0x87, 0x85 );              // Servo ON */
}

static void ServoOn(void)
{
	RegWriteA(0x85, 0x80);	/* Clear PID Ram Data */
	WaitTime(1);
	RegWriteA(0x87, 0x85);	/* Servo ON */
}

void LC898212XDAF_F_MONO_init(unsigned char Hall_Off, unsigned char Hall_Bias)
{
	struct stSmvPar StSmvPar;

	AfInit(Hall_Off, Hall_Bias);	/* Initialize driver IC */

	/* Step move parameter set */
	StSmvPar.UsSmvSiz = STMV_SIZE;
	StSmvPar.UcSmvItv = STMV_INTERVAL;
	StSmvPar.UcSmvEnb = STMCHTG_SET | STMSV_SET | STMLFF_SET;
	StmvSet(StSmvPar);
	ServoOn();		/* Close loop ON */
}

unsigned char LC898212XDAF_F_MONO_moveAF(short SsSmvEnd)
{
	return StmvTo(SsSmvEnd);
}

