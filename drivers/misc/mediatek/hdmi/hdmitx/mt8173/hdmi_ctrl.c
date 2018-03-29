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

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include "hdmi_ctrl.h"
#include <linux/kernel.h>
#include <linux/delay.h>
#include <asm/div64.h>

#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#else
/* #include <mach/devs.h> */
/* #include <mach/mt_typedefs.h> */
#include <linux/types.h>
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_pm_ldo.h> */
#endif



/* example to set hdmi clk */
void vSetClk(void)
{
	/* hdmi_i2c_write(); */
	/* hdmi_i2c_write(); */
	/* hdmi_i2c_write(); */
}

unsigned char fgHDMIDDCByteWrite(unsigned char ui1Device, unsigned char ui1Data_Addr,
				 unsigned char u1Data)
{
	/* fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data); */
		return FALSE;

}

unsigned char fgHDMIDDCDataWrite(unsigned char ui1Device, unsigned char ui1Data_Addr,
				 unsigned char u1Count, const unsigned char *pr_u1Data)
{
	/* fgResult = fgTxDataWrite(ui1Device/2, ui1Data_Addr, 1, &u1Data); */
		return FALSE;

}

unsigned char fgHDMIDDCByteRead(unsigned char ui1Device, unsigned char ui1Data_Addr,
				unsigned char *pu1Data)
{
	/* fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data); */
		return FALSE;

}

unsigned char fgHDMIDDCDataRead(unsigned char ui1Device, unsigned char ui1Data_Addr,
				unsigned char u1Count, unsigned char *pu1Data)
{
	/* fgResult= fgTxDataRead(ui1Device/2, ui1Data_Addr, 1, pu1Data); */
		return FALSE;

}

#endif
