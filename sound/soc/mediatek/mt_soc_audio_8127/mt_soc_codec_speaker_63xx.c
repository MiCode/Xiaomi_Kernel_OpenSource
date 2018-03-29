/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_analog_type.h"
#if 0
#include <mach/pmic_mt6323_sw.h>	/* no this */
#endif

#include "mt_soc_pcm_common.h"
#include "mt_soc_codec_63xx.h"

/* static DEFINE_MUTEX(Speaker_Ctrl_Mutex); */
/* static DEFINE_SPINLOCK(Speaker_lock); */
/* static int Speaker_Counter = 0; */
/* static bool  Speaker_Trim_init = false; */

void Speaker_ClassD_Open(void)
{
	pr_debug("%s\n", __func__);
	pmic_set_ana_reg(SPK_CON2, 0x0214, 0xffff);	/* enable classAB OC function */
	pmic_set_ana_reg(SPK_CON9, 0x0400, 0xffff);	/* Set Spk 6dB gain */

	/* enable SPK-Amp with 0dB gain, enable SPK amp offset triming, select class D mode */
	pmic_set_ana_reg(SPK_CON0, 0x3008, 0xffff);

	pmic_set_ana_reg(SPK_CON0, 0x3009, 0xffff);	/* Enable Class ABD */
	mdelay(5);
	pmic_set_ana_reg(SPK_CON0, 0x3001, 0xffff);	/* enable SPK AMP with 0dB gain, select Class D. enable Amp. */
}

void Speaker_ClassD_close(void)
{
	pr_debug("%s\n", __func__);
	/* Mute Spk amp, select to original class AB mode. disable class-D Amp */
	pmic_set_ana_reg(SPK_CON0, 0x0004, 0xffff);
}


void Speaker_ClassAB_Open(void)
{
	pr_debug("%s\n", __func__);
	pmic_set_ana_reg(SPK_CON2, 0x0214, 0xffff);	/* enable classAB OC function */
	pmic_set_ana_reg(SPK_CON9, 0x0400, 0xffff);	/* Set Spk 6dB gain */

	/* enable SPK-Amp with 0dB gain, enable SPK amp offset triming, select class D mode */
	pmic_set_ana_reg(SPK_CON0, 0x3008, 0xffff);

	pmic_set_ana_reg(SPK_CON0, 0x3009, 0xffff);	/* Enable Class ABD */
	mdelay(5);
	pmic_set_ana_reg(SPK_CON0, 0x3000, 0xffff);	/* disable amp before switch to ClassAB */
	pmic_set_ana_reg(SPK_CON0, 0x3005, 0xffff);	/* enable SPK AMP with 0dB gain, select Class AB. enable Amp. */
}

void Speaker_ClassAB_close(void)
{
	pr_debug("%s\n", __func__);
	/* Mute Spk amp, select to original class D mode. disable class-AB Amp */
	pmic_set_ana_reg(SPK_CON0, 0x0000, 0xffff);
}

void Speaker_ReveiverMode_Open(void)
{
	pr_debug("%s\n", __func__);
	/* enable classAB OC function, enable speaker L receiver mode[6] */
	pmic_set_ana_reg(SPK_CON2, 0x0614, 0xffff);

	pmic_set_ana_reg(SPK_CON9, 0x0100, 0xffff);
	/* pmic_set_ana_reg(SPK_CON9, 0x0400, 0xffff); // Set Spk 6dB gain */

	/* enable SPK AMP with -6dB gain for 2in1 speaker, select Class AB. enable Amp. */
	pmic_set_ana_reg(SPK_CON0, 0x1005, 0xffff);
}

void Speaker_ReveiverMode_close(void)
{
	/* Mute Spk amp, select to original class D mode. disable class-AB Amp */
	pmic_set_ana_reg(SPK_CON0, 0x0000, 0xffff);
}

bool GetSpeakerOcFlag(void)
{
	unsigned int OCregister = 0;
	unsigned int bitmask = 1;
	bool DmodeFlag = false;
	bool ABmodeFlag = false;
#if 0				/* no this */
	pmic_set_ana_reg(TOP_CKPDN_CON2_CLR, 0x3, 0xffff);
#endif
	OCregister = pmic_get_ana_reg(SPK_CON6);
	DmodeFlag = OCregister & (bitmask << 14);	/* no.14 bit is SPK_D_OC_L_DEG */
	ABmodeFlag = OCregister & (bitmask << 15);	/* no.15 bit is SPK_AB_OC_L_DEG */
	pr_debug("OCregister = %d\n", OCregister);
	return (DmodeFlag | ABmodeFlag);
}
