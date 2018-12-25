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

#include <generated/autoconf.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/preempt.h>
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_debugfs.h"
#if defined(CONFIG_MTK_PMIC_WRAP_HAL)
#include "pwrap_hal.h"
#endif
#ifdef CONFIG_MTK_AUXADC_INTF
#include <mt-plat/mtk_auxadc_intf.h>
#endif /* CONFIG_MTK_AUXADC_INTF */

#if defined(CONFIG_MTK_EXTBUCK)
#include "include/extbuck/fan53526.h"
#endif

static unsigned int vmd1_trim;

void vmd1_pmic_trim_setting(bool enable)
{
	unsigned int hwcid = pmic_get_register_value(PMIC_HWCID);

	PMICLOG("%s HWCID: 0x%X\n", __func__, hwcid);
	if (hwcid != 0x5520)
		return;

	if (vmd1_trim == 0)
		vmd1_trim = pmic_get_register_value(PMIC_RG_VMODEM_ZXOS_TRIM);

	/*Set Trim Value*/
	if (enable) {
		pmic_set_register_value(PMIC_RG_VMODEM_ZXOS_TRIM, vmd1_trim);
		PMICLOG("%s OFF, zxos trim 0x%X, __func__, new_value: 0x%X\n"
			, __func__, vmd1_trim, vmd1_trim);
	} else {
		unsigned int vmd1_trim_new = vmd1_trim+9;

		pmic_set_register_value(
				PMIC_RG_VMODEM_ZXOS_TRIM, vmd1_trim_new);
		PMICLOG("%s ON, zxos trim 0x%X, new_value: 0x%X\n"
			, __func__, vmd1_trim, vmd1_trim_new);
	}
}

void vmd1_pmic_setting_on(void)
{
	unsigned int ret = 0;

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758)
	ret = pmic_buck_vmodem_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_ldo_vsram_md_lp(SRCLKEN0, 1, HW_LP);
	ret = pmic_buck_vmodem_lp(SRCLKEN2, 1, HW_LP);
	ret = pmic_ldo_vsram_md_lp(SRCLKEN2, 1, HW_LP);
#endif
	if (pmic_get_register_value(PMIC_DA_VMODEM_EN) &&
			pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN) &&
			pmic_get_register_value(PMIC_RG_LDO_VSRAM_MD_EN) &&
			pmic_get_register_value(PMIC_RG_BUCK_VMODEM_EN))
		return;

#if defined(CONFIG_MACH_MT6759)
	/* 1.configure VMODEM voltage as 0.75V (0.4+0.00625*step)*/
	/* set to 0.75V */
	ret = pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL, 0x38);
	/* 2.configure VSRAM_MD voltage as 0.88125V (0.51875+0.00625*step)*/
	/* set to 0.88125V */
	ret = pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_VOSEL, 0x3A);
#elif defined(CONFIG_MACH_MT6758)
	/* 1.configure VMODEM voltage as 0.9V (0.4+0.00625*step)*/
	/* set to 0.9V */
	ret = pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL, 0x50);
	/* 2.configure VSRAM_MD voltage as 0.95V (0.51875+0.00625*step)*/
	/* set to 0.95V */
	ret = pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_VOSEL, 0x45);
#else
	/* 1.configure VMODEM voltage as 0.8V */
	/* set to 0.8V */
	ret = pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL, 0x40);
	/* 2.configure VSRAM_MD voltage as 0.93125V */
	/* set to 0.93125V */
	ret = pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_VOSEL, 0x42);
#endif

	/* Apply new trim value */
	vmd1_pmic_trim_setting(true);
	/* Enable FPFM before enable BUCK */
	/* SW workaround to avoid VMODEM overshoot */
	pmic_config_interface(0x128E, 0x1, 0x1, 12);	/* 0x128E[12] = 1 */
	PMICLOG("%s vmodem fpfm %d\n", __func__,
		(pmic_get_register_value(PMIC_RG_VMODEM_TRAN_BST) & 0x20) >> 5);
	pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_EN, 1);
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_EN, 1);
	udelay(500);

	/* Disable FPFM after enable BUCK */
	/* SW workaround to avoid VMODME overshoot */
	pmic_config_interface(0x128E, 0x0, 0x1, 12);	/* 0x128E[12] = 0 */
	PMICLOG("%s vmodem fpfm %d\n", __func__,
		(pmic_get_register_value(PMIC_RG_VMODEM_TRAN_BST) & 0x20) >> 5);
	/* Recover new trim value */
	vmd1_pmic_trim_setting(false);

#if defined(CONFIG_MACH_MT6759)
	if (pmic_get_register_value(PMIC_DA_VMODEM_VOSEL) != 0x38)
#elif defined(CONFIG_MACH_MT6758)
	if (pmic_get_register_value(PMIC_DA_VMODEM_VOSEL) != 0x50)
#else
	if (pmic_get_register_value(PMIC_DA_VMODEM_VOSEL) != 0x40)
#endif
		pr_info("%s vmodem vosel = 0x%x, da_vosel = 0x%x"
			, __func__
			, pmic_get_register_value(PMIC_RG_BUCK_VMODEM_VOSEL)
			, pmic_get_register_value(PMIC_DA_VMODEM_VOSEL));

#if defined(CONFIG_MACH_MT6759)
	if (pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_VOSEL) != 0x3A)
#elif defined(CONFIG_MACH_MT6758)
	if (pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_VOSEL) != 0x45)
#else
	if (pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_VOSEL) != 0x42)
#endif
		pr_info("%s vsram_md vosel = 0x%x, da_vosel = 0x%x"
			, __func__
			, pmic_get_register_value(PMIC_RG_LDO_VSRAM_MD_VOSEL)
			, pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_VOSEL));
}

void vmd1_pmic_setting_off(void)
{
#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758)
	pmic_buck_vmodem_lp(SW, 1, SW_OFF);
	pmic_ldo_vsram_md_lp(SW, 1, SW_OFF);
#endif

	/* 1.Call PMIC driver API configure VMODEM off */
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_EN, 0);
	/* 2.Call PMIC driver API configure VSRAM_MD off */
	pmic_set_register_value(PMIC_RG_LDO_VSRAM_MD_EN, 0);

	PMICLOG("%s vmodem en %d\n", __func__,
		(pmic_get_register_value(PMIC_DA_VMODEM_EN) & 0x1));
	PMICLOG("%s vsram_md en %d\n", __func__,
		(pmic_get_register_value(PMIC_DA_QI_VSRAM_MD_EN) & 0x1));
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

	pmic_set_register_value(PMIC_RG_VPROC11_FPWM, mode);

	ret = pmic_get_register_value(PMIC_RG_VPROC11_FPWM);

	return (ret == mode) ? (0) : (-1);
}

int vgpu_pmic_set_mode(unsigned char mode)
{
	unsigned char ret = 0;

	pmic_set_register_value(PMIC_RG_VGPU_FPWM, mode);

	ret = pmic_get_register_value(PMIC_RG_VGPU_FPWM);

	return (ret == mode) ? (0) : (-1);
}

/* [Export API] */

/* SCP set VCORE voltage */
/* return 0 if success, otherwise return set voltage(uV) */
unsigned int pmic_scp_set_vcore(unsigned int voltage)
{
	const char *name = "SSHUB_VCORE";
	unsigned int max_uV = 1200000;
	unsigned int min_uV = 406250;
	unsigned int uV_step = 6250;
	unsigned short value = 0;
	unsigned short read_val = 0;

	if (voltage > max_uV || voltage < min_uV) {
		pr_info("[PMIC]Set Wrong buck voltage for %s, range (%duV - %duV)\n"
			, name, min_uV, max_uV);
		return voltage;
	}
	value = (voltage - min_uV) / uV_step;
	PMICLOG("%s Expected volt step: 0x%x\n", name, value);

	/*---Make sure BUCK VCORE ON before setting---*/
	if (pmic_get_register_value(PMIC_DA_VCORE_EN)) {
		pmic_set_register_value(PMIC_RG_BUCK_VCORE_SSHUB_VOSEL, value);
		udelay(220);
		read_val = pmic_get_register_value(
					PMIC_RG_BUCK_VCORE_SSHUB_VOSEL);
		if (read_val == value)
			PMICLOG("Set %s Voltage to %duV pass\n", name, voltage);
		else {
			pr_info("[PMIC] Set %s Voltage fail with step = %d, read voltage = %duV\n"
				, name, value, (read_val * uV_step + min_uV));
			return voltage;
		}
	} else {
		pr_info("[PMIC] Set %s Votage to %duV fail, due to buck non-enable\n"
				, name, voltage);
		return voltage;
	}

	return 0;
}

/* SCP set VSRAM_VCORE voltage */
/* return 0 if success, otherwise return set voltage(uV) */
unsigned int pmic_scp_set_vsram_vcore(unsigned int voltage)
{
	const char *name = "VSRAM_VCORE_SSHUB";
	unsigned int max_uV = 1312500;
	unsigned int min_uV = 518750;
	unsigned int uV_step = 6250;
	unsigned short value = 0;
	unsigned short read_val = 0;

	if (voltage > max_uV || voltage < min_uV) {
		pr_info("[PMIC]Set Wrong buck voltage for %s, range (%duV - %duV)\n"
			, name, min_uV, max_uV);
		return voltage;
	}
	value = (voltage - min_uV) / uV_step;
	PMICLOG("%s Expected volt step: 0x%x\n", name, value);

	/*---Make sure BUCK VSRAM_VCORE ON before setting---*/
	if (pmic_get_register_value(PMIC_DA_QI_VSRAM_CORE_EN)) {
		pmic_set_register_value(
				PMIC_RG_LDO_VSRAM_CORE_SSHUB_VOSEL, value);
		udelay(220);
		read_val = pmic_get_register_value(
					PMIC_RG_LDO_VSRAM_CORE_SSHUB_VOSEL);
		if (read_val == value)
			PMICLOG("Set %s Voltage to %duV pass\n", name, voltage);
		else {
			pr_info("[PMIC] Set %s Voltage fail with step = %d, read voltage = %duV\n"
				, name, value, (read_val * uV_step + min_uV));
			return voltage;
		}
	} else {
		pr_info("[PMIC] Set %s Votage to %duV fail, due to buck non-enable\n"
				, name, voltage);
		return voltage;
	}

	return 0;
}

/* enable/disable VSRAM_VCORE HW tracking, return 0 if success */
unsigned int enable_vsram_vcore_hw_tracking(unsigned int en)
{
	unsigned int rdata = 0;
	unsigned int wdata = 0;

	if (en != 1 && en != 0)
		return en;
	if (en)
		wdata = 0x7;
	pmic_config_interface(MT6355_LDO_TRACKING_CON0, wdata, 0x7, 0);
	pmic_read_interface(MT6355_LDO_TRACKING_CON0, &rdata, 0x7, 0);
	if (!(rdata ^ wdata)) {
		pr_info("[PMIC][%s] %s HW TRACKING success\n"
			, __func__, (en == 1) ? "enable" : "disable");
		/*By AP, LP DE Give*/
		/*if (en == 0)*/	/* set VSRAM_VCORE to 1.0V*/
		/*pmic_set_register_value(PMIC_RG_VSRAM_CORE_VOSEL, 0x60);*/
		return 0;
	}
	pr_info("[PMIC][%s] %s HW TRACKING fail\n"
		, __func__, (en == 1) ? "enable" : "disable");
	return 1;
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

#if defined(CONFIG_MACH_MT6758) || defined(CONFIG_MACH_MT6759)
	ret = enable_vsram_vcore_hw_tracking(1);
	PMICLOG("Enable VSRAM_VCORE hw tracking\n");
#else
	ret = enable_vsram_vcore_hw_tracking(0);
	PMICLOG("Disable VSRAM_VCORE hw tracking\n");
#endif
	return ret;
}

/***************************************************************************
 * upmu_interrupt_chrdet_int_en
 ****************************************************************************/
#if 0 /* May not used, marked by Jeter */
void upmu_interrupt_chrdet_int_en(unsigned int val)
{
	PMICLOG("[%s] val=%d\n", __func__, val);
	pmic_enable_interrupt(INT_CHRDET, val, "PMIC");
}
EXPORT_SYMBOL(upmu_interrupt_chrdet_int_en);
#endif

/***************************************************************************
 * PMIC charger detection
 ****************************************************************************/
unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int val = 0;

	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	PMICLOG("[%s] CHRDET status = %d\n", __func__, val);

	return val;
}

int is_ext_buck2_exist(void)
{
#if defined(CONFIG_MTK_EXTBUCK)
	if ((is_fan53526_exist() == 1))
		return 1;
	else
		return 0;
#else
	return 0;
#endif /* End of #if defined(CONFIG_MTK_EXTBUCK) */
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
 * Enternal SWCHR
 ******************************************************************************/

/*****************************************************************************
 * Enternal VBAT Boost status
 ******************************************************************************/

/*****************************************************************************
 * Enternal BUCK status
 ******************************************************************************/

int get_ext_buck_i2c_ch_num(void)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if (is_mt6313_exist() == 1)
		return get_mt6313_i2c_ch_num();
#endif
		return -1;
}

int is_ext_buck_sw_ready(void)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_sw_ready() == 1))
		return 1;
#endif
		return 0;
}

int is_ext_buck_exist(void)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_exist() == 1))
		return 1;
#endif
		return 0;
}

int is_ext_buck_gpio_exist(void)
{
	return pmic_get_register_value(PMIC_RG_STRUP_EXT_PMIC_EN);
}

MODULE_AUTHOR("Jeter Chen");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
