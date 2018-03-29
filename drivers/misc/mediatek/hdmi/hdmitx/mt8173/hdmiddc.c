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

 /******************************************************************************
*[File]             tx9134ddc.c
*[Version]          v0.1
*[Revision Date]    2008-04-18
*[Author]           Kenny Hsieh
*[Description]
*    source file for hdmi general control routine
*
*
******************************************************************************/
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include "hdmi_ctrl.h"
#include "hdmiddc.h"
/* #include <mach/mt_pmic_wrap.h> */

/* #define hdmi_reg[HDMI_DDC]    (0xF1013000) */

void internal_ddc_read(unsigned long u4Reg, unsigned int *p4Data)
{
	*p4Data = (*(unsigned int *)(u4Reg));
}

void internal_ddc_write(unsigned long u4Reg, unsigned int u4data)
{
	*(unsigned int *)(u4Reg) = (u4data);
}

unsigned int hdmi_ddc_read(unsigned short u2Reg)
{
	unsigned int u4Data;

	internal_ddc_read(hdmi_reg[HDMI_DDC] + u2Reg, &u4Data);

	HDMI_REG_LOG("[R]ddc = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void hdmi_ddc_write(unsigned short u2Reg, unsigned int u4Data)
{
	internal_ddc_write(hdmi_reg[HDMI_DDC] + u2Reg, u4Data);

	HDMI_REG_LOG("[W]ddc = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
}

#define SIF_READ32(u4Addr)          (hdmi_ddc_read(u4Addr))
#define SIF_WRITE32(u4Addr, u4Val)  (hdmi_ddc_write(u4Addr, u4Val))

#define SIF_SET_BIT(u4Addr, u4Val)  SIF_WRITE32((u4Addr), (SIF_READ32(u4Addr) | (u4Val)))
#define SIF_CLR_BIT(u4Addr, u4Val)  SIF_WRITE32((u4Addr), (SIF_READ32(u4Addr) & (~(u4Val))))

#define IS_SIF_BIT(u4Addr, u4Val)   ((SIF_READ32(u4Addr) & (u4Val)) == (u4Val))

#define SIF_WRITE_MASK(u4Addr, u4Mask, u4Offset, u4Val) \
	SIF_WRITE32(u4Addr, ((SIF_READ32(u4Addr) & (~(u4Mask))) | (((u4Val) << (u4Offset)) & (u4Mask))))
#define SIF_READ_MASK(u4Addr, u4Mask, u4Offset)  ((SIF_READ32(u4Addr) & (u4Mask)) >> (u4Offset))

#define DDCM_DATA0_READ()   SIF_READ_MASK(DDC_DDCMD0, DDCM_DATA0, 0)
#define DDCM_DATA1_READ()   SIF_READ_MASK(DDC_DDCMD0, DDCM_DATA1, 8)
#define DDCM_DATA2_READ()   SIF_READ_MASK(DDC_DDCMD0, DDCM_DATA2, 16)
#define DDCM_DATA3_READ()   SIF_READ_MASK(DDC_DDCMD0, DDCM_DATA3, 24)
#define DDCM_DATA4_READ()   SIF_READ_MASK(DDC_DDCMD1, DDCM_DATA4, 0)
#define DDCM_DATA5_READ()   SIF_READ_MASK(DDC_DDCMD1, DDCM_DATA5, 8)
#define DDCM_DATA6_READ()   SIF_READ_MASK(DDC_DDCMD1, DDCM_DATA6, 16)
#define DDCM_DATA7_READ()   SIF_READ_MASK(DDC_DDCMD1, DDCM_DATA7, 24)

#define DDCM_DATA0_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD0, DDCM_DATA0, 0,  u4Val)
#define DDCM_DATA1_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD0, DDCM_DATA1, 8,  u4Val)
#define DDCM_DATA2_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD0, DDCM_DATA2, 16, u4Val)
#define DDCM_DATA3_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD0, DDCM_DATA3, 24, u4Val)
#define DDCM_DATA4_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD1, DDCM_DATA4, 0,  u4Val)
#define DDCM_DATA5_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD1, DDCM_DATA5, 8,  u4Val)
#define DDCM_DATA6_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD1, DDCM_DATA6, 16, u4Val)
#define DDCM_DATA7_WRITE(u4Val)   SIF_WRITE_MASK(DDC_DDCMD1, DDCM_DATA7, 24, u4Val)

#define DDCM_CLK_DIV_READ()        SIF_READ_MASK(DDC_DDCMCTL0, DDCM_CLK_DIV_MASK, DDCM_CLK_DIV_OFFSET)
#define DDCM_CLK_DIV_WRITE(u4Val)  SIF_WRITE_MASK(DDC_DDCMCTL0, DDCM_CLK_DIV_MASK, DDCM_CLK_DIV_OFFSET, u4Val)

#define DDCM_ACK_READ()            SIF_READ_MASK(DDC_DDCMCTL1, DDCM_ACK_MASK, DDCM_ACK_OFFSET)

#define DDCM_PGLEN_READ()          SIF_READ_MASK(DDC_DDCMCTL1, DDCM_PGLEN_MASK, DDCM_PGLEN_OFFSET)
#define DDCM_PGLEN_WRITE(u4Val)    SIF_WRITE_MASK(DDC_DDCMCTL1, DDCM_PGLEN_MASK, DDCM_PGLEN_OFFSET, u4Val)

#define DDCM_SIF_MODE_READ()          SIF_READ_MASK(DDC_DDCMCTL1, DDCM_SIF_MODE_MASK, DDCM_SIF_MODE_OFFSET)
#define DDCM_SIF_MODE_WRITE(u4Val)    SIF_WRITE_MASK(DDC_DDCMCTL1, DDCM_SIF_MODE_MASK, DDCM_SIF_MODE_OFFSET, u4Val)


int i4DDCM_Suspend(void *param)
{
	HDMI_DDC_FUNC();
	/* SIF_CLR_BIT(SIF_INTEN, DDCCI_INTEN); */
	SIF_CLR_BIT(DDC_DDCMCTL0, DDCM_SM0EN);

	return 0;
}

int i4DDCM_Resume(void *param)
{
	HDMI_DDC_FUNC();

	SIF_SET_BIT(DDC_DDCMCTL0, DDCM_SM0EN);
	/* SIF_SET_BIT(SIF_INTEN, DDCCI_INTEN); */

	return 0;
}

unsigned int DDCM_Init(void)
{
	HDMI_DDC_FUNC();
	SIF_SET_BIT(DDC_DDCMCTL0, DDCM_SCL_STRECH);
	SIF_SET_BIT(DDC_DDCMCTL0, DDCM_SM0EN);
	SIF_CLR_BIT(DDC_DDCMCTL0, DDCM_ODRAIN);

	return 1;
}

static unsigned int DDCM_TrigMode(unsigned int u4Mode)
{
	int timeout = 20 * 1000;
	HDMI_DDC_FUNC();

	DDCM_SIF_MODE_WRITE(u4Mode);
	SIF_SET_BIT(DDC_DDCMCTL1, DDCM_TRI);

	while (IS_SIF_BIT(DDC_DDCMCTL1, DDCM_TRI)) {
		udelay(2);
		timeout -= 2;
		if (timeout <= 0)
			break;
	}

	return 0;
}

static unsigned char _DDCMRead(unsigned char ucCurAddrMode, unsigned int u4ClkDiv,
			       unsigned char ucDev, unsigned int u4Addr, enum SIF_BIT_T ucAddrType,
			       unsigned char *pucValue, unsigned int u4Count)
{
	unsigned int u4Ack;
	unsigned char ucReadCount, ucIdx, ucAckCount, ucAckFinal, ucTmpCount;

	HDMI_DDC_FUNC();
	DDCM_Init();
	if ((pucValue == NULL) || (u4Count == 0) || (u4ClkDiv == 0))
		return 0;


	ucIdx = 0;

	/* check busy/trigger bit */
	if (IS_SIF_BIT(DDC_DDCMCTL1, DDCM_TRI))
		return 0;



	DDCM_CLK_DIV_WRITE(u4ClkDiv);

	DDCM_TrigMode(DDCM_START);

	/* Max'0619'04, 4-block EEDID reading */
	if (ucDev > EDID_ID) {
		DDCM_DATA0_WRITE(0x60);

		DDCM_DATA1_WRITE(ucDev - EDID_ID);
		DDCM_PGLEN_WRITE(0x01);
		DDCM_TrigMode(DDCM_WRITE_DATA);
		u4Ack = DDCM_ACK_READ();
		if (u4Ack != 0x3)
			goto ddc_master_read_end;

		DDCM_TrigMode(DDCM_START);
		ucDev = EDID_ID;
	}

	if (ucCurAddrMode == 0) {
		DDCM_DATA0_WRITE((ucDev << 1));

		if (ucAddrType == SIF_8_BIT) {
			DDCM_DATA1_WRITE(u4Addr);
			DDCM_PGLEN_WRITE(0x01);
			DDCM_TrigMode(DDCM_WRITE_DATA);
			u4Ack = DDCM_ACK_READ();
			if (u4Ack != 0x3)
				goto ddc_master_read_end;

		} else if (ucAddrType == SIF_16_BIT) {
			DDCM_DATA1_WRITE((u4Addr >> 8));
			DDCM_DATA2_WRITE(u4Addr);
			DDCM_PGLEN_WRITE(0x02);
			DDCM_TrigMode(DDCM_WRITE_DATA);
			u4Ack = DDCM_ACK_READ();
			if (u4Ack != 0x7)
				goto ddc_master_read_end;
		}

		DDCM_TrigMode(DDCM_START);
	}

	DDCM_DATA0_WRITE(((ucDev << 1) + 1));
	DDCM_PGLEN_WRITE(0x00);
	DDCM_TrigMode(DDCM_WRITE_DATA);
	u4Ack = DDCM_ACK_READ();
	if (u4Ack != 0x1)
		goto ddc_master_read_end;


	ucAckCount = (u4Count - 1) / 8;
	ucAckFinal = 0;
	while (u4Count > 0) {
		if (ucAckCount > 0) {
			ucReadCount = 8;
			ucAckFinal = 0;
			ucAckCount--;
		} else {
			ucReadCount = (unsigned char)u4Count;
			ucAckFinal = 1;
		}

		DDCM_PGLEN_WRITE((ucReadCount - 1));
		DDCM_TrigMode((ucAckFinal == 1) ? DDCM_READ_DATA_NO_ACK : DDCM_READ_DATA_ACK);

		/* ack, killua */
		u4Ack = DDCM_ACK_READ();
		for (ucTmpCount = 0; ((u4Ack & (1 << ucTmpCount)) != 0) && (ucTmpCount < 8);
		     ucTmpCount++)
				;
		if (((ucAckFinal == 1) && (ucTmpCount != (ucReadCount - 1)))
		    || ((ucAckFinal == 0) && (ucTmpCount != ucReadCount))) {
			HDMI_DDC_LOG("[DDC] Device(0x%x)/Word(0x%x) Address NACK! ACK(0x%x)\n",
				     ucDev, u4Addr, u4Ack);
			break;
		}

		switch (ucReadCount) {
		case 8:
			pucValue[ucIdx + 7] = DDCM_DATA7_READ();
			pucValue[ucIdx + 6] = DDCM_DATA6_READ();
			pucValue[ucIdx + 5] = DDCM_DATA5_READ();
			pucValue[ucIdx + 4] = DDCM_DATA4_READ();
			pucValue[ucIdx + 3] = DDCM_DATA3_READ();
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();

			break;
		case 7:
			pucValue[ucIdx + 6] = DDCM_DATA6_READ();
			pucValue[ucIdx + 5] = DDCM_DATA5_READ();
			pucValue[ucIdx + 4] = DDCM_DATA4_READ();
			pucValue[ucIdx + 3] = DDCM_DATA3_READ();
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 6:
			pucValue[ucIdx + 5] = DDCM_DATA5_READ();
			pucValue[ucIdx + 4] = DDCM_DATA4_READ();
			pucValue[ucIdx + 3] = DDCM_DATA3_READ();
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 5:
			pucValue[ucIdx + 4] = DDCM_DATA4_READ();
			pucValue[ucIdx + 3] = DDCM_DATA3_READ();
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 4:
			pucValue[ucIdx + 3] = DDCM_DATA3_READ();
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 3:
			pucValue[ucIdx + 2] = DDCM_DATA2_READ();
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 2:
			pucValue[ucIdx + 1] = DDCM_DATA1_READ();
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		case 1:
			pucValue[ucIdx + 0] = DDCM_DATA0_READ();
			break;
		default:
			break;
		}

		u4Count -= ucReadCount;
		ucIdx += ucReadCount;
	}

 ddc_master_read_end:
	DDCM_TrigMode(DDCM_STOP);
	return ucIdx;
}

static unsigned char _DDCMWrite(unsigned char ucCurAddrMode, unsigned int u4ClkDiv,
				unsigned char ucDev, unsigned int u4Addr, enum SIF_BIT_T ucAddrType,
				const unsigned char *pucValue, unsigned int u4Count)
{
	unsigned int u4Ack;
	unsigned char ucWriteCount, ucIdx, ucTmpCount;

	HDMI_DDC_FUNC();
	DDCM_Init();
	if ((pucValue == NULL) || (u4Count == 0) || (u4ClkDiv == 0))
		return 0;


	ucIdx = 0;

	/* check busy/trigger bit */
	if (IS_SIF_BIT(DDC_DDCMCTL1, DDCM_TRI))
		return 0;

	DDCM_CLK_DIV_WRITE(u4ClkDiv);
	DDCM_TrigMode(DDCM_START);

	DDCM_DATA0_WRITE((ucDev << 1));
	if (ucAddrType == SIF_8_BIT) {
		DDCM_DATA1_WRITE(u4Addr);
		DDCM_PGLEN_WRITE(0x01);
		DDCM_TrigMode(DDCM_WRITE_DATA);
		u4Ack = DDCM_ACK_READ();
		if (u4Ack != 0x3)
			goto ddc_master_write_end;

	} else if (ucAddrType == SIF_16_BIT) {
		DDCM_DATA1_WRITE((u4Addr >> 8));
		DDCM_DATA2_WRITE(u4Addr);
		DDCM_PGLEN_WRITE(0x02);
		DDCM_TrigMode(DDCM_WRITE_DATA);
		u4Ack = DDCM_ACK_READ();
		if (u4Ack != 0x7)
			goto ddc_master_write_end;

	}

	while (u4Count > 0) {
		ucWriteCount = (u4Count > 8) ? 8 : ((unsigned char)u4Count);

		switch (ucWriteCount) {
		case 8:
			DDCM_DATA7_WRITE(pucValue[ucIdx + 7]);
			DDCM_DATA6_WRITE(pucValue[ucIdx + 6]);
			DDCM_DATA5_WRITE(pucValue[ucIdx + 5]);
			DDCM_DATA4_WRITE(pucValue[ucIdx + 4]);
			DDCM_DATA3_WRITE(pucValue[ucIdx + 3]);
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 7:
			DDCM_DATA6_WRITE(pucValue[ucIdx + 6]);
			DDCM_DATA5_WRITE(pucValue[ucIdx + 5]);
			DDCM_DATA4_WRITE(pucValue[ucIdx + 4]);
			DDCM_DATA3_WRITE(pucValue[ucIdx + 3]);
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 6:
			DDCM_DATA5_WRITE(pucValue[ucIdx + 5]);
			DDCM_DATA4_WRITE(pucValue[ucIdx + 4]);
			DDCM_DATA3_WRITE(pucValue[ucIdx + 3]);
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 5:
			DDCM_DATA4_WRITE(pucValue[ucIdx + 4]);
			DDCM_DATA3_WRITE(pucValue[ucIdx + 3]);
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 4:
			DDCM_DATA3_WRITE(pucValue[ucIdx + 3]);
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 3:
			DDCM_DATA2_WRITE(pucValue[ucIdx + 2]);
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 2:
			DDCM_DATA1_WRITE(pucValue[ucIdx + 1]);
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		case 1:
			DDCM_DATA0_WRITE(pucValue[ucIdx + 0]);
			break;
		default:
			break;
		}

		DDCM_PGLEN_WRITE((ucWriteCount - 1));
		DDCM_TrigMode(DDCM_WRITE_DATA);

		/* ack, killua */
		u4Ack = DDCM_ACK_READ();
		for (ucTmpCount = 0; ((u4Ack & (1 << ucTmpCount)) != 0) && (ucTmpCount < 8);
		     ucTmpCount++)
			;

		if (ucTmpCount != ucWriteCount) {
			HDMI_DDC_LOG("[DDC] Device(0x%x)/Word(0x%x) Address NACK! ACK(0x%x)\n",
				     ucDev, u4Addr, u4Ack);
			break;
		}

		u4Count -= ucWriteCount;
		ucIdx += ucWriteCount;
	}

 ddc_master_write_end:
	DDCM_TrigMode(DDCM_STOP);
	return ucIdx;
}

unsigned int DDCM_RanAddr_Write(unsigned int u4ClkDiv, unsigned char ucDev, unsigned int u4Addr,
				enum SIF_BIT_T ucAddrType, const unsigned char *pucValue,
				unsigned int u4Count)
{
	unsigned int u4WriteCount1;
	unsigned char ucReturnVaule;

	HDMI_DDC_FUNC();

	if ((pucValue == NULL) ||
	    (u4Count == 0) ||
	    (u4ClkDiv == 0) ||
	    (ucAddrType > SIF_16_BIT) ||
	    ((ucAddrType == SIF_8_BIT) && (u4Addr > 255)) ||
	    ((ucAddrType == SIF_16_BIT) && (u4Addr > 65535)))
		return 0;



	if (ucAddrType == SIF_8_BIT)
		u4WriteCount1 = ((255 - u4Addr) + 1);
	else if (ucAddrType == SIF_16_BIT)
		u4WriteCount1 = ((65535 - u4Addr) + 1);

	u4WriteCount1 = (u4WriteCount1 > u4Count) ? u4Count : u4WriteCount1;
	ucReturnVaule = _DDCMWrite(0, u4ClkDiv, ucDev, u4Addr, ucAddrType, pucValue, u4WriteCount1);

	return (unsigned int)ucReturnVaule;
}

unsigned int DDCM_CurAddr_Read(unsigned int u4ClkDiv, unsigned char ucDev, unsigned char *pucValue,
			       unsigned int u4Count)
{
	unsigned char ucReturnVaule;

	HDMI_DDC_FUNC();

	if ((pucValue == NULL) || (u4Count == 0) || (u4ClkDiv == 0))
		return 0;


	ucReturnVaule = _DDCMRead(1, u4ClkDiv, ucDev, 0, SIF_8_BIT, pucValue, u4Count);

	return (__u32) ucReturnVaule;
}

unsigned char DDCM_RanAddr_Read(unsigned int u4ClkDiv, unsigned char ucDev, unsigned int u4Addr,
				enum SIF_BIT_T ucAddrType, unsigned char *pucValue, unsigned int u4Count)
{
	unsigned int u4ReadCount;
	unsigned char ucReturnVaule;

	HDMI_DDC_FUNC();
	if ((pucValue == NULL) ||
	    (u4Count == 0) ||
	    (u4ClkDiv == 0) ||
	    (ucAddrType > SIF_16_BIT) ||
	    ((ucAddrType == SIF_8_BIT) && (u4Addr > 255)) ||
	    ((ucAddrType == SIF_16_BIT) && (u4Addr > 65535)))
		return 0;


	if (ucAddrType == SIF_8_BIT)
		u4ReadCount = ((255 - u4Addr) + 1);
	else if (ucAddrType == SIF_16_BIT)
		u4ReadCount = ((65535 - u4Addr) + 1);

	u4ReadCount = (u4ReadCount > u4Count) ? u4Count : u4ReadCount;
	ucReturnVaule = _DDCMRead(0, u4ClkDiv, ucDev, u4Addr, ucAddrType, pucValue, u4ReadCount);


	return ucReturnVaule;
}

unsigned char fgDDCDataRead(unsigned char bDevice, unsigned char bData_Addr,
			    unsigned char bDataCount, unsigned char *prData)
{
	HDMI_DDC_FUNC();
	if (DDCM_RanAddr_Read
	    (SIF1_CLOK, (unsigned char)bDevice, (unsigned int)bData_Addr, SIF_8_BIT,
	     (unsigned char *)prData, (unsigned int)bDataCount) == bDataCount)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgDDCDataWrite(unsigned char bDevice, unsigned char bData_Addr,
			     unsigned char bDataCount, unsigned char *prData)
{
	HDMI_DDC_FUNC();
	if ((DDCM_RanAddr_Write
	     (SIF1_CLOK, (unsigned char)bDevice, (unsigned int)bData_Addr, SIF_8_BIT,
	      (unsigned char *)prData, (unsigned int)bDataCount)) == bDataCount)
		return TRUE;
	else
		return FALSE;
}


#endif
