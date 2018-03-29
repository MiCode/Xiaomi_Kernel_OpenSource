/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/uaccess.h>

#include <mt-plat/charging.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>

#if defined(CONFIG_MTK_SMART_BATTERY)
#include <mt-plat/battery_common.h>
#endif

#include "mt6336.h"


CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;

/* ============================================================ // */
/* extern function */
/* ============================================================ // */
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)

int hw_charging_get_charger_type(void)
{
	CHR_Type_num = STANDARD_HOST;
	return CHR_Type_num;
}

#else

/* ============================================================ */
/* static function                                              */
/* ============================================================ */

static unsigned short bc12_set_register_value(MT6336_PMU_FLAGS_LIST_ENUM flagname, unsigned int val)
{
	return mt6336_set_flag_register_value(flagname, val);
}


static unsigned short bc12_get_register_value(MT6336_PMU_FLAGS_LIST_ENUM flagname)
{
	return mt6336_get_flag_register_value(flagname);
}



static void hw_bc12_init(void)
{
	/* RG_BC12_BB_CTRL = 1 */
	bc12_set_register_value(MT6336_RG_A_BC12_BB_CTRL, 1);
	/* RG_BC12_RST = 1 */
	bc12_set_register_value(MT6336_RG_BC12_RST, 1);
	/* RG_A_BC12_IBIAS_EN = 1 */
	bc12_set_register_value(MT6336_RG_A_BC12_IBIAS_EN, 1);
	/* Delay 10ms */
	usleep_range(10000, 20000);
	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 0);
	/* RG_A_BC12_CMP_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 0);
	/* RG_A_BC12_IPU_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 0);
	/* RG_A_BC12_IPD_EN[1:0]= 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 0);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);

	Charger_Detect_Init();

	/* VBUS asserted, wait > 300ms (min.) */
	msleep(300);
}


static unsigned int hw_bc12_step_1(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 1);
	/* RG_A_BC12_CMP_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 2);
	/* RG_A_BC12_IPU_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 2);
	/* RG_A_BC12_IPD_EN[1:0]= 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 1);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Check BC 1.2 comparator output
	 * 0: Jump to 2-a
	 * 1: Jump to 2-b
	 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

static unsigned int hw_bc12_step_2a(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_A_BC12_VSRC_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 2);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 0);
	/* RG_A_BC12_CMP_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 1);
	/* RG_A_BC12_IPU_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 0);
	/* RG_A_BC12_IPD_EN[1:0]= 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 1);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Check BC 1.2 comparator output
	 * 0: SDP
	 * 1: Jump to 3-a
	 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

static unsigned int hw_bc12_step_2b1(void)
{
	unsigned int wChargerAvail = 0;
	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 2);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 2);
	/* RG_A_BC12_CMP_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 2);
	/* RG_A_BC12_IPU_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 2);
	/* RG_A_BC12_IPD_EN[1:0]= 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 2);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 2);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Latch output OUT1 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

static unsigned int hw_bc12_step_2b2(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 0);
	/* RG_A_BC12_CMP_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 1);
	/* RG_A_BC12_IPU_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 0);
	/* RG_A_BC12_IPD_EN[1:0]= 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 1);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Latch output OUT2 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

#if 0
static unsigned int hw_bc12_step_2b3(void)
{
	unsigned int wChargerAvail = 0;
	/* Check OUT1 and OUT2
	 * 1. (OUT1, OUT2) = 00: Step 3-b
	 * 2. (OUT1, OUT2) = 01: Iphone 5V/1A
	 * 3. (OUT1, OUT2) = 1X: IPAD 5V/2A, IPAD4 5V/24A, SS Tablet 5V/2A
	 */

	return wChargerAvail;
}
#endif

static unsigned int hw_bc12_step_3a(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 1);
	/* RG_A_BC12_CMP_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 1);
	/* RG_A_BC12_IPU_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 2);
	/* RG_A_BC12_IPD_EN[1:0]= 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 0);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Check BC 1.2 comparator output
	 * 0: CDP
	 * 1: DCP
	 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

static unsigned int hw_bc12_step_3b(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 0);
	/* RG_A_BC12_CMP_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 1);
	/* RG_A_BC12_IPU_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 0);
	/* RG_A_BC12_IPD_EN[1:0]= 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 1);
	/* RG_A_BC12_IPD_HALF_EN = 1 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 1);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	/* Check BC 1.2 comparator output
	 * 0: Jump to 4
	 * 1: Non-STD Charger
	 */
	wChargerAvail = bc12_get_register_value(MT6336_AD_QI_BC12_CMP_OUT);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}

static unsigned int hw_bc12_step_4(void)
{
	unsigned int wChargerAvail = 0;

	/* RG_A_BC12_VSRC_EN[1:0] = 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_VSRC_EN, 0);
	/* RG_A_BC12_VREF_VTH_EN[1:0] = 01 */
	bc12_set_register_value(MT6336_RG_A_BC12_VREF_VTH_EN, 0);
	/* RG_A_BC12_CMP_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_CMP_EN, 0);
	/* RG_A_BC12_IPU_EN[1:0] = 10 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPU_EN, 0);
	/* RG_A_BC12_IPD_EN[1:0]= 00 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_EN, 0);
	/* RG_A_BC12_IPD_HALF_EN = 0 */
	bc12_set_register_value(MT6336_RG_A_BC12_IPD_HALF_EN, 0);
	/* Wait 200us */
	usleep_range(200, 1000);

	/* RGS_BC12_CMP_OUT */
	wChargerAvail = bc12_set_register_value(MT6336_AD_QI_BC12_CMP_OUT, 0);
	/* Wait 4us */
	udelay(4);

	return wChargerAvail;
}


static void hw_bc12_done(void)
{
	Charger_Detect_Release();
}

static void dump_charger_name(CHARGER_TYPE type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
		pr_err("charger type: %d, CHARGER_UNKNOWN\n", type);
		break;
	case STANDARD_HOST:
		pr_err("charger type: %d, Standard USB Host\n", type);
		break;
	case CHARGING_HOST:
		pr_err("charger type: %d, Charging USB Host\n", type);
		break;
	case NONSTANDARD_CHARGER:
		pr_err("charger type: %d, Non-standard Charger\n", type);
		break;
	case STANDARD_CHARGER:
		pr_err("charger type: %d, Standard Charger\n", type);
		break;
	case APPLE_2_1A_CHARGER:
		pr_err("charger type: %d, APPLE_2_1A_CHARGER\n", type);
		break;
	case APPLE_1_0A_CHARGER:
		pr_err("charger type: %d, APPLE_1_0A_CHARGER\n", type);
		break;
	case APPLE_0_5A_CHARGER:
		pr_err("charger type: %d, APPLE_0_5A_CHARGER\n", type);
		break;
	default:
		pr_err("charger type: %d, Not Defined!!!\n", type);
		break;
	}
}

int hw_charging_get_charger_type(void)
{
	unsigned int out = 0;

	CHR_Type_num = CHARGER_UNKNOWN;

	hw_bc12_init();

	if (hw_bc12_step_1()) {
		if (hw_bc12_step_2b1())
			out = 0x02;
		if (hw_bc12_step_2b2())
			out |= 0x01;
		switch (out) {
		case 0:
			if (hw_bc12_step_3b())
				CHR_Type_num = NONSTANDARD_CHARGER;
			else if (hw_bc12_step_4())
				pr_err("charger type: DP/DM are floating\n");
			else
				pr_err("charger type: SS 5V/2A\n");
			break;
		case 1:
			CHR_Type_num = APPLE_1_0A_CHARGER;
			break;
		case 2:
		case 3:
			CHR_Type_num = APPLE_2_1A_CHARGER;
			break;
		}
	} else {
		if (hw_bc12_step_2a()) {
			if (hw_bc12_step_3a())
				CHR_Type_num = STANDARD_CHARGER;
			else
				CHR_Type_num = CHARGING_HOST;
		} else
			CHR_Type_num = STANDARD_HOST;
	}

	dump_charger_name(CHR_Type_num);

	hw_bc12_done();

	return CHR_Type_num;
}
#endif
