/*
 * Copyright (C) 2017 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/preempt.h>
#include <linux/uaccess.h>

#include "mtk_devinfo.h"

#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_debugfs.h"

#ifdef CONFIG_MTK_AUXADC_INTF
#include <mt-plat/mtk_auxadc_intf.h>
#endif /* CONFIG_MTK_AUXADC_INTF */

#ifdef CONFIG_MTK_PMIC_WRAP_HAL
#include "pwrap_hal.h"
#endif

void vmd1_pmic_setting_on(void)
{
	/* Vcore: 0x2D, 0.8V  */
	/* Vmodem: 0x30, 0.8V */
	unsigned int vcore_vosel = 0x2D, vmodem_vosel = 0x30;

	if (pmic_get_register_value(PMIC_DA_VCORE_VOSEL) != vcore_vosel)
		pr_notice("vmd1_pmic_setting_on vcore vosel = 0x%x, da_vosel = 0x%x"
		       , pmic_get_register_value(PMIC_RG_BUCK_VCORE_VOSEL)
		       , pmic_get_register_value(PMIC_DA_VCORE_VOSEL));

	/* 1.Call PMIC driver API configure VMODEM voltage */
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL, vmodem_vosel);
	if (pmic_get_register_value(PMIC_DA_VMODEM_VOSEL) != vmodem_vosel)
		pr_notice("vmd1_pmic_setting_on vmodem vosel = 0x%x, da_vosel = 0x%x"
		       , pmic_get_register_value(PMIC_RG_BUCK_VMODEM_VOSEL)
		       , pmic_get_register_value(PMIC_DA_VMODEM_VOSEL));
}

void vmd1_pmic_setting_off(void)
{
	PMICLOG("vmd1_pmic_setting_off\n");
}

void wk_pmic_enable_sdn_delay(void)
{
	pmic_set_register_value(PMIC_TMA_KEY, 0x9CA8);
	pmic_set_register_value(PMIC_RG_SDN_DLY_ENB, 0);
	pmic_set_register_value(PMIC_TMA_KEY, 0);
}

int vcore_pmic_set_mode(unsigned char mode)
{
	unsigned char ret = 0;

	pmic_set_register_value(PMIC_RG_VCORE_FPWM, mode);

	ret = pmic_get_register_value(PMIC_RG_VCORE_FPWM);

	return (ret == mode) ? (0) : (-1);
}

int vproc_pmic_set_mode(unsigned char mode)
{
	unsigned char ret = 0;

	pmic_set_register_value(PMIC_RG_VPROC_FPWM, mode);

	ret = pmic_get_register_value(PMIC_RG_VPROC_FPWM);

	return (ret == mode) ? (0) : (-1);
}
/* [Export API] */
void pmic_enable_smart_reset(unsigned char smart_en, unsigned char smart_sdn_en)
{
	pmic_set_register_value(PMIC_RG_SMART_RST_MODE, smart_en);
	pmic_set_register_value(PMIC_RG_SMART_RST_SDN_EN, smart_sdn_en);
	pr_info("[%s] smart_en:%d, smart_sdn_en:%d\n"
		, __func__
		, smart_en
		, smart_sdn_en);
}

int pmic_tracking_init(void)
{
	int ret = 0;
#if 0
	/* 0.1V */
	pmic_set_register_value(PMIC_RG_VSRAM_VCORE_VOSEL_OFFSET, 0x10);
	/* 0.025V */
	pmic_set_register_value(PMIC_RG_VSRAM_VCORE_VOSEL_DELTA, 0x4);
	/* 1.0V */
	pmic_set_register_value(PMIC_RG_VSRAM_VCORE_VOSEL_ON_HB, 0x60);
	/* 0.8V */
	pmic_set_register_value(PMIC_RG_VSRAM_VCORE_VOSEL_ON_LB, 0x40);
	/* 0.65V */
	pmic_set_register_value(PMIC_RG_VSRAM_VCORE_VOSEL_SLEEP_LB, 0x28);
#endif
#if 0
	ret = enable_vsram_vcore_hw_tracking(1);
	PMICLOG("Enable VSRAM_VCORE hw tracking\n");
#else
	PMICLOG("VSRAM_VCORE hw tracking not support in MT6357\n");
#endif

	return ret;
}


/*****************************************************************************
 * PMIC charger detection
 ******************************************************************************/
unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int val = 0;

	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	PMICLOG("[upmu_get_rgs_chrdet] CHRDET status = %d\n", val);

	return val;
}

int is_ext_vbat_boost_exist(void)
{
	return 0;
}

int is_ext_swchr_exist(void)
{
	return 0;
}

/*****************************************************************************
 * Enternal BUCK status
 ******************************************************************************/

int is_ext_buck_gpio_exist(void)
{
	return pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
