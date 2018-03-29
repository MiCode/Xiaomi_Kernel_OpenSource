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

#ifdef HDMI_MT8193_SUPPORT

#include "mt8193_ctrl.h"
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/div64.h>

/* example to set mt8193 clk */
void vSetClk(void)
{
	/* mt8193_i2c_write(); */
	/* mt8193_i2c_write(); */
	/* mt8193_i2c_write(); */
}

unsigned char fgMT8193DDCByteWrite(unsigned char ui1Device, unsigned char ui1Data_Addr, unsigned char u1Data)
{
	unsigned char fgResult = 0;

	/* fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data); */


	if (fgResult == TRUE)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgMT8193DDCDataWrite(unsigned char ui1Device,
	unsigned char ui1Data_Addr, unsigned char u1Count, const unsigned char *pr_u1Data)
{
	unsigned char fgResult = 0;

	/* fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data); */


	if (fgResult == TRUE)
		return TRUE;
	else
		return FALSE;

}

unsigned char fgMT8193DDCByteRead(unsigned char ui1Device,
	unsigned char ui1Data_Addr, unsigned char *pu1Data)
{
	unsigned char fgResult = 0;


	/* fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data); */


	if (fgResult == TRUE)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgMT8193DDCDataRead(unsigned char ui1Device,
	unsigned char ui1Data_Addr, unsigned char u1Count, unsigned char *pu1Data)
{
	unsigned char fgResult = 0;


	/* fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data); */


	if (fgResult == TRUE)
		return TRUE;
	else
		return FALSE;
}

#endif
