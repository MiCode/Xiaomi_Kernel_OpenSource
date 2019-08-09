/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <generated/autoconf.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/preempt.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>

#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_debugfs.h"
#include "include/pmic_api_buck.h"
#include "include/pmic_bif.h"
#include "include/pmic_auxadc.h"

/**********************************************************
 * PMIC related define
 ***********************************************************/
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_MTK_PMIC_WRAP_HAL)
#include "pwrap_hal.h"
#define CONFIG_PMIC_HW_ACCESS_EN
#endif

/*---IPI Mailbox define---*/
#if defined(IPIMB)
#include <mach/mtk_pmic_ipi.h>
#endif

/**********************************************************
 * PMIC read/write APIs
 ***********************************************************/
static DEFINE_MUTEX(pmic_access_mutex);
static DEFINE_MUTEX(pmic_hk_mutex);

static unsigned int pmic_read_device(unsigned int RegNum,
				     unsigned int *val,
				     unsigned int MASK,
				     unsigned int SHIFT)
{
	unsigned int return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	return_value = pwrap_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_info("[%s] err ret=%d, Reg[0x%x] pmic_wrap read data fail, MASK=0x%x, SHIFT=%d\n"
			, __func__, return_value, RegNum, MASK, SHIFT);
		return return_value;
	}

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);
	PMICLOG("[pmic_read_interface] (0x%x,0x%x,0x%x,0x%x)\n",
			RegNum, *val, MASK, SHIFT);
#else
	PMICLOG("[pmic_read_interface] Can not access HW PMIC(FPGA?/PWRAP?)\n");
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

static unsigned int pmic_write_device(unsigned int RegNum,
				      unsigned int val,
				      unsigned int MASK,
				      unsigned int SHIFT)
{
	unsigned int return_value = 0;
#if defined(CONFIG_PMIC_HW_ACCESS_EN)
#ifndef IPIMB
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	return_value = pwrap_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_info("[%s] err ret=%d, Reg[0x%x] pmic_wrap read data fail, val=0x%x, MASK=0x%x, SHIFT=%d\n"
			, __func__, return_value, RegNum, val, MASK, SHIFT);
		return return_value;
	}

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	PMICLOG("[pmic_config_interface] (0x%x,0x%x,0x%x,0x%x,0x%x)\n",
					RegNum, val, MASK, SHIFT, pmic_reg);

	return_value = pwrap_write((RegNum), pmic_reg);
	if (return_value != 0) {
		pr_info("[%s] err ret=%d, Reg[0x%x]=0x%x pmic_wrap write data fail, val=0x%x, MASK=0x%x, SHIFT=%d\n"
			, __func__, return_value, RegNum, pmic_reg,
			 val, MASK, SHIFT);
		return return_value;
	}

#else /*---IPIMB---*/
	return_value = pmic_ipi_config_interface(RegNum, val, MASK, SHIFT, 1);
	if (return_value)
		pr_info("[%s]IPIMB write data fail with ret = %d, (0x%x,0x%x,0x%x,%d)\n"
			, __func__, return_value, RegNum, val, MASK, SHIFT);
	else
		PMICLOG("[%s]IPIMB write data =(0x%x,0x%x,0x%x,%d)\n"
			, __func__, RegNum, val, MASK, SHIFT);

#endif /*---!IPIMB---*/

#else
	PMICLOG("[%s] Can not access HW PMIC(FPGA?/PWRAP?)\n", __func__);
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

unsigned int pmic_read_interface(unsigned int RegNum,
				 unsigned int *val,
				 unsigned int MASK,
				 unsigned int SHIFT)
{
	return pmic_read_device(RegNum, val, MASK, SHIFT);
}

unsigned int pmic_config_interface(unsigned int RegNum,
				   unsigned int val,
				   unsigned int MASK,
				   unsigned int SHIFT)
{
	unsigned int return_value = 0;

	if (preempt_count() > 0 ||
	    irqs_disabled() ||
	    system_state != SYSTEM_RUNNING ||
	    oops_in_progress)
		return pmic_config_interface_nolock(RegNum, val, MASK, SHIFT);
	mutex_lock(&pmic_access_mutex);
	return_value = pmic_write_device(RegNum, val, MASK, SHIFT);
	mutex_unlock(&pmic_access_mutex);

	return return_value;
}

unsigned int pmic_read_interface_nolock(unsigned int RegNum,
					unsigned int *val,
					unsigned int MASK,
					unsigned int SHIFT)
{
	return pmic_read_device(RegNum, val, MASK, SHIFT);
}

unsigned int pmic_config_interface_nolock(unsigned int RegNum,
					  unsigned int val,
					  unsigned int MASK,
					  unsigned int SHIFT)
{
	unsigned int return_value = 0;

	return_value = pmic_write_device(RegNum, val, MASK, SHIFT);
	return return_value;
}

unsigned short pmic_set_register_value(PMU_FLAGS_LIST_ENUM flagname,
				       unsigned int val)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag = &pmu_flags_table[flagname];
	unsigned int ret = 0;

	if (pFlag->flagname != flagname) {
		pr_info("[%s]pmic flag idx error\n", __func__);
		return 1;
	}

	ret = pmic_config_interface((unsigned int)(pFlag->offset), val,
		(unsigned int)(pFlag->mask), (unsigned int)(pFlag->shift));
	if (ret != 0) {
		pr_info("[%s] error ret: %d when set Reg[0x%x]=0x%x\n",
			__func__, ret, (pFlag->offset), val);
		return ret;
	}

	return 0;
}

unsigned short pmic_get_register_value(PMU_FLAGS_LIST_ENUM flagname)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag = &pmu_flags_table[flagname];
	unsigned int val = 0;
	unsigned int ret = 0;

	ret = pmic_read_interface((unsigned int)pFlag->offset, &val,
		(unsigned int)(pFlag->mask), (unsigned int)(pFlag->shift));
	if (ret != 0) {
		pr_info("[%s] error ret: %d when get Reg[0x%x]\n", __func__,
			ret, (pFlag->offset));
		return ret;
	}

	return val;
}

unsigned short pmic_set_register_value_nolock(PMU_FLAGS_LIST_ENUM flagname,
					      unsigned int val)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag = &pmu_flags_table[flagname];
	unsigned int ret = 0;

	if (pFlag->flagname != flagname) {
		pr_info("[%s]pmic flag idx error\n", __func__);
		return 1;
	}

	ret = pmic_config_interface_nolock((unsigned int)(pFlag->offset), val,
		(unsigned int)(pFlag->mask), (unsigned int)(pFlag->shift));
	if (ret != 0) {
		pr_info("[%s] error ret: %d when set Reg[0x%x]=0x%x\n",
			__func__, ret, (pFlag->offset), val);
		return ret;
	}

	return 0;
}

unsigned short pmic_get_register_value_nolock(PMU_FLAGS_LIST_ENUM flagname)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag = &pmu_flags_table[flagname];
	unsigned int val = 0;
	unsigned int ret = 0;

	ret = pmic_read_interface_nolock((unsigned int)pFlag->offset, &val,
		(unsigned int)(pFlag->mask), (unsigned int)(pFlag->shift));
	if (ret != 0) {
		pr_info("[%s] error ret: %d when get Reg[0x%x]\n", __func__,
			ret, (pFlag->offset));
		return ret;
	}

	return val;
}

unsigned short bc11_set_register_value(PMU_FLAGS_LIST_ENUM flagname,
				       unsigned int val)
{
	return pmic_set_register_value(flagname, val);
}

unsigned short bc11_get_register_value(PMU_FLAGS_LIST_ENUM flagname)
{
	return pmic_get_register_value(flagname);
}

unsigned short pmic_set_hk_reg_value(PMU_FLAGS_LIST_ENUM flagname,
				     unsigned int val)
{
	const PMU_FLAG_TABLE_ENTRY *pFlag = &pmu_flags_table[flagname];
	unsigned int ret = 0;

	if (pFlag->flagname != flagname) {
		pr_notice("[%s]pmic flag idx error\n", __func__);
		return 1;
	}

	if (preempt_count() > 0 || irqs_disabled() ||
	    system_state != SYSTEM_RUNNING || oops_in_progress) {
		ret = pmic_write_device(
			(unsigned int)(pFlag->offset), val,
			(unsigned int)(pFlag->mask),
			(unsigned int)(pFlag->shift));
	} else {
		mutex_lock(&pmic_hk_mutex);
		ret = pmic_write_device(
			(unsigned int)(pFlag->offset), val,
			(unsigned int)(pFlag->mask),
			(unsigned int)(pFlag->shift));
		mutex_unlock(&pmic_hk_mutex);
	}
	if (ret != 0) {
		pr_notice("[%s] error ret: %d when set Reg[0x%x]=0x%x\n"
		       , __func__
		       , ret
		       , (pFlag->offset)
		       , val);
		return ret;
	}

	return 0;
}

unsigned int upmu_get_reg_value(unsigned int reg)
{
	unsigned int reg_val = 0;
	unsigned int ret = 0;

	ret = pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);
	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(unsigned int reg, unsigned int reg_val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

/*****************************************************************************
 * FTM
 ******************************************************************************/
#define PMIC_DEVNAME "pmic_ftm"
#define Get_IS_EXT_BUCK_EXIST _IOW('k', 20, int)
#define Get_IS_EXT_VBAT_BOOST_EXIST _IOW('k', 21, int)
#define Get_IS_EXT_SWCHR_EXIST _IOW('k', 22, int)
#define Get_IS_EXT_BUCK2_EXIST _IOW('k', 23, int)
#define Get_IS_EXT_BUCK3_EXIST _IOW('k', 24, int)


static struct class *pmic_class;
static struct cdev *pmic_cdev;
static int pmic_major;
static dev_t pmic_devno;

static long pmic_ftm_ioctl(struct file *file,
			   unsigned int cmd,
			   unsigned long arg)
{
	int *user_data_addr;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	switch (cmd) {
		/*#if defined(FTM_EXT_BUCK_CHECK)*/
	case Get_IS_EXT_BUCK_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
#ifdef CONFIG_MTK_EXTBUCK
		adc_out_data[0] = is_ext_buck_exist();
#else
		adc_out_data[0] = 0;
#endif
		/*adc_out_data[0] = is_ext_buck_gpio_exist();*/
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[%s] Get_IS_EXT_BUCK_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
		/*#endif*/

		/*#if defined(FTM_EXT_VBAT_BOOST_CHECK)*/
	case Get_IS_EXT_VBAT_BOOST_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = is_ext_vbat_boost_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[%s] Get_IS_EXT_VBAT_BOOST_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
		/*#endif*/

		/*#if defined(FEATURE_FTM_SWCHR_HW_DETECT)*/
	case Get_IS_EXT_SWCHR_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = is_ext_swchr_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[%s] Get_IS_EXT_SWCHR_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
		/*#endif*/
	case Get_IS_EXT_BUCK2_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
#ifdef CONFIG_MTK_EXTBUCK
		adc_out_data[0] = is_ext_buck2_exist();
#else
		adc_out_data[0] = 0;
#endif
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[%s] Get_IS_EXT_BUCK2_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	case Get_IS_EXT_BUCK3_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = 0;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[%s] Get_IS_EXT_BUCK3_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	default:
		PMICLOG("[%s] Error ID\n", __func__);
		break;
	}

	return 0;
}
#ifdef CONFIG_COMPAT
static long pmic_ftm_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
		/*#if defined(FTM_EXT_BUCK_CHECK) */
	case Get_IS_EXT_BUCK_EXIST:
	case Get_IS_EXT_VBAT_BOOST_EXIST:
	case Get_IS_EXT_SWCHR_EXIST:
	case Get_IS_EXT_BUCK2_EXIST:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	default:
		PMICLOG("[%s] Error ID\n", __func__);
		break;
	}

	return 0;
}
#endif
static int pmic_ftm_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int pmic_ftm_release(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations pmic_ftm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pmic_ftm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = pmic_ftm_compat_ioctl,
#endif
	.open = pmic_ftm_open,
	.release = pmic_ftm_release,
};

void pmic_ftm_init(void)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	ret = alloc_chrdev_region(&pmic_devno, 0, 1, PMIC_DEVNAME);
	if (ret)
		PMICLOG("[%s] Error: Can't Get Major number for pmic_ftm\n"
			, __func__);

	pmic_cdev = cdev_alloc();
	pmic_cdev->owner = THIS_MODULE;
	pmic_cdev->ops = &pmic_ftm_fops;

	ret = cdev_add(pmic_cdev, pmic_devno, 1);
	if (ret)
		PMICLOG("[%s] Error: cdev_add\n", __func__);

	pmic_major = MAJOR(pmic_devno);
	pmic_class = class_create(THIS_MODULE, PMIC_DEVNAME);

	class_dev = (struct class_device *)device_create(pmic_class,
					NULL, pmic_devno, NULL, PMIC_DEVNAME);

	PMICLOG("[%s] Done\n", __func__);
}


/*****************************************************************************
 * HW Setting
 ******************************************************************************/
unsigned short is_battery_remove;
unsigned short is_wdt_reboot_pmic;
unsigned short is_wdt_reboot_pmic_chk;
unsigned short g_vmodem_vosel;

unsigned short is_battery_remove_pmic(void)
{
	return is_battery_remove;
}

void PMIC_CUSTOM_SETTING_V1(void)
{
}

/*****************************************************************************
 * system function
 ******************************************************************************/
void __attribute__ ((weak)) pmic_auxadc_suspend(void)
{
}

void __attribute__ ((weak)) pmic_auxadc_resume(void)
{
}

void __attribute__ ((weak)) record_md_vosel(void)
{
}

void __attribute__ ((weak)) pmic_enable_smart_reset(unsigned char smart_en,
	unsigned char smart_sdn_en)
{
	pr_notice("[%s] smart reset not support!\n", __func__);
}

void __attribute__ ((weak)) enable_bat_temp_det(bool en)
{
	pr_notice("[%s] smart reset not support!\n", __func__);
}

static int pmic_mt_probe(struct platform_device *pdev)
{
	PMICLOG("******** MT pmic driver probe!! ********\n");
	/*get PMIC CID */
	PMICLOG("PMIC CID = 0x%x\n", pmic_get_register_value(PMIC_SWCID));

	record_md_vosel();

	PMIC_INIT_SETTING_V1();
	PMICLOG("[PMIC_INIT_SETTING_V1] Done\n");

	/*PMIC Interrupt Service*/
	PMIC_EINT_SETTING(pdev);
	PMICLOG("[PMIC_EINT_SETTING] Done\n");

	mtk_regulator_init(pdev);
	PMICLOG("[PMIC] mtk_regulator_init : done.\n");

	pmic_throttling_dlpt_init();

	PMICLOG("[PMIC] pmic_throttling_dlpt_init : done.\n");

	pmic_debug_init(pdev);
	PMICLOG("[PMIC] pmic_debug_init : done.\n");

	pmic_ftm_init();

	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT))
		pmic_bif_init();

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	return 0;
}

static int pmic_mt_remove(struct platform_device *pdev)
{
	PMICLOG("******** MT pmic driver remove!! ********\n");

	return 0;
}

static void pmic_mt_shutdown(struct platform_device *pdev)
{
	PMICLOG("******** MT pmic driver shutdown!! ********\n");
	vmd1_pmic_setting_on();
}

static int pmic_mt_suspend(struct platform_device *pdev, pm_message_t state)
{
	PMICLOG("******** MT pmic driver suspend!! ********\n");

	pmic_throttling_dlpt_suspend();
	pmic_auxadc_suspend();
	return 0;
}

static int pmic_mt_resume(struct platform_device *pdev)
{
	PMICLOG("******** MT pmic driver resume!! ********\n");

	pmic_throttling_dlpt_resume();
	pmic_auxadc_resume();
	return 0;
}

static const struct of_device_id pmic_of_match[] = {
	{.compatible = "mediatek,mt_pmic"},
	{},
};

static struct platform_driver pmic_mt_driver = {
	.probe = pmic_mt_probe,
	.remove = pmic_mt_remove,
	.shutdown = pmic_mt_shutdown,
	.suspend = pmic_mt_suspend,
	.resume = pmic_mt_resume,
	.driver = {
		.name = "mt-pmic",
		.of_match_table = pmic_of_match,
	},
};

/**************************************************************************
 * PMIC mudule init/exit
 ***************************************************************************/
static int __init pmic_mt_init(void)
{
	int ret;

	pmic_init_wake_lock(&pmicThread_lock, "pmicThread_lock wakelock");

	PMICLOG("pmic_regulator_init_OF\n");

	ret = platform_driver_register(&pmic_mt_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n"
			, __func__, ret);
		return ret;
	}

	pr_debug("****[%s] Initialization : DONE !!\n", __func__);

	return 0;
}

static void __exit pmic_mt_exit(void)
{
	platform_driver_unregister(&pmic_mt_driver);
}
fs_initcall(pmic_mt_init);
module_exit(pmic_mt_exit);

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MTK PMIC COMMON Interface Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0_M");
