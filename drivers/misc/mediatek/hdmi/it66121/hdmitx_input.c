/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "hdmitx.h"
#include "hdmitx_drv.h"

#ifdef HDMITX_INPUT_INFO

#define InitCEC() HDMITX_SetI2C_Byte(0x0F, 0x08, 0x00)
#define DisableCEC() HDMITX_SetI2C_Byte(0x0F, 0x08, 0x08)

LONG CalcAudFS(void)
{
	/* LONG RCLK ; */
	LONG Cnt;
	LONG FS;

	/* RCLK = CalcRCLK(); */
	Switch_HDMITX_Bank(0);
	Cnt = (LONG)HDMITX_ReadI2C_Byte(0x60);
	FS = hdmiTxDev[0].RCLK / 2;
	FS /= Cnt;
	HDMITX_DEBUG_PRINTF1(("FS = %ld RCLK = %ld, Cnt = %ld\n", FS,
			      hdmiTxDev[0].RCLK, Cnt));
	return FS;
}

LONG CalcPCLK(void)
{
	unsigned char uc, div;
	int i;
	long sum, count, PCLK;

	Switch_HDMITX_Bank(0);
	uc = HDMITX_ReadI2C_Byte(0x5F) & 0x80;

	if (!uc)
		return 0;

	/* InitCEC(); */
	/* // uc = CEC_ReadI2C_Byte(0x09) & 0xFE ; */
	/* CEC_WriteI2C_Byte(0x09, 1); */
	/* delay1ms(100); */
	/* CEC_WriteI2C_Byte(0x09, 0); */
	/* RCLK = CEC_ReadI2C_Byte(0x47); */
	/* RCLK <<= 8 ; */
	/* RCLK |= CEC_ReadI2C_Byte(0x46); */
	/* RCLK <<= 8 ; */
	/* RCLK |= CEC_ReadI2C_Byte(0x45); */
	/* DisableCEC(); */
	/* // RCLK *= 160 ; // RCLK /= 100 ; */
	/* // RCLK in KHz. */

	HDMITX_SetI2C_Byte(0xD7, 0xF0, 0x80);
	delay1ms(1);
	HDMITX_SetI2C_Byte(0xD7, 0x80, 0x00);

	count = HDMITX_ReadI2C_Byte(0xD7) & 0xF;
	count <<= 8;
	count |= HDMITX_ReadI2C_Byte(0xD8);

	for (div = 7; div > 0; div--) {
		/* IT66121_LOG("div = %d\n",(int)div) ; */
		if (count < (1 << (11 - div)))
			break;
	}
	HDMITX_SetI2C_Byte(0xD7, 0x70, div << 4);

	uc = HDMITX_ReadI2C_Byte(0xD7) & 0x7F;
	for (i = 0, sum = 0; i < 100; i++) {
		HDMITX_WriteI2C_Byte(0xD7, uc | 0x80);
		delay1ms(1);
		HDMITX_WriteI2C_Byte(0xD7, uc);

		count = HDMITX_ReadI2C_Byte(0xD7) & 0xF;
		count <<= 8;
		count |= HDMITX_ReadI2C_Byte(0xD8);
		sum += count;
	}
	sum /= 100;
	count = sum;

	HDMITX_DEBUG_PRINTF1(("RCLK(in GetPCLK) = %ld\n", hdmiTxDev[0].RCLK));
	HDMITX_DEBUG_PRINTF1(("div = %d, count = %d\n", (int)div, (int)count));
	HDMITX_DEBUG_PRINTF1(("count = %ld\n", count));

	PCLK = hdmiTxDev[0].RCLK * 128 / count * 16;
	PCLK *= (1 << div);

	if (HDMITX_ReadI2C_Byte(0x70) & 0x10)
		PCLK /= 2;

	HDMITX_DEBUG_PRINTF1(("PCLK = %ld\n", PCLK));
	return PCLK;
}

LONG CalcRCLK(void)
{
	/* unsigned char uc ; */
	int i;
	long sum, RCLKCNT;

	InitCEC();
	sum = 0;
	for (i = 0; i < 5; i++) {
		/* uc = CEC_ReadI2C_Byte(0x09) & 0xFE ; */
		CEC_WriteI2C_Byte(0x09, 1);
		delay1ms(100);
		CEC_WriteI2C_Byte(0x09, 0);
		RCLKCNT = CEC_ReadI2C_Byte(0x47);
		RCLKCNT <<= 8;
		RCLKCNT |= CEC_ReadI2C_Byte(0x46);
		RCLKCNT <<= 8;
		RCLKCNT |= CEC_ReadI2C_Byte(0x45);
		/* HDMITX_DEBUG_PRINTF1(("RCLK = %ld\n",RCLKCNT) ); */
		sum += RCLKCNT;
	}
	DisableCEC();
	RCLKCNT = sum * 32;
	HDMITX_DEBUG_PRINTF("RCLK = %ld,%03ld,%03ld\n", RCLKCNT / 1000000,
			    (RCLKCNT % 1000000) / 1000, RCLKCNT % 1000);
	return RCLKCNT;
}

unsigned short hdmitx_getInputHTotal(void)
{
	unsigned char uc;
	unsigned short hTotal;

	HDMITX_SetI2C_Byte(0x0F, 1, 0);
	HDMITX_SetI2C_Byte(0xA8, 8, 8);

	uc = HDMITX_ReadI2C_Byte(0xB2);
	hTotal = (uc & 1) ? (1 << 12) : 0;
	uc = HDMITX_ReadI2C_Byte(0x91);
	hTotal |= ((unsigned short)uc) << 4;
	uc = HDMITX_ReadI2C_Byte(0x90);
	hTotal |= (uc & 0xF0) >> 4;
	HDMITX_SetI2C_Byte(0xA8, 8, 0);
	return hTotal;
}

unsigned short hdmitx_getInputVTotal(void)
{
	unsigned char uc;
	unsigned short vTotal;

	HDMITX_SetI2C_Byte(0x0F, 1, 0);
	HDMITX_SetI2C_Byte(0xA8, 8, 8);

	uc = HDMITX_ReadI2C_Byte(0x99);
	vTotal = ((unsigned short)uc & 0xF) << 8;
	uc = HDMITX_ReadI2C_Byte(0x98);
	vTotal |= uc;
	HDMITX_SetI2C_Byte(0xA8, 8, 0);
	return vTotal;
}

bool hdmitx_isInputInterlace(void)
{
	unsigned char uc;

	HDMITX_SetI2C_Byte(0x0F, 1, 0);
	HDMITX_SetI2C_Byte(0xA8, 8, 8);

	uc = HDMITX_ReadI2C_Byte(0xA5);
	HDMITX_SetI2C_Byte(0xA8, 8, 0);
	return uc & (1 << 4) ? TRUE : FALSE;
}

unsigned char hdmitx_getAudioCount(void)
{
	return HDMITX_ReadI2C_Byte(REG_TX_AUD_COUNT);
}
#endif
