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
#include <linux/preempt.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/soc/mediatek/pmic_wrap.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6358/core.h>

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
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#define CONFIG_PMIC_HW_ACCESS_EN
#endif

/*---IPI Mailbox define---*/
#if defined(IPIMB)
#include <mach/mtk_pmic_ipi.h>
#endif

/**********************************************************
 * PMIC read/write APIs
 ***********************************************************/
static struct mt6358_chip *chip;
struct regmap *pmic_nolock_regmap;

static int pmic_read_device(struct regmap *map,
			    unsigned int RegNum,
			    unsigned int *val,
			    unsigned int MASK,
			    unsigned int SHIFT)
{
	int ret = 0;
#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	if (!map) {
		pr_notice("[%s] regmap not ready\n", __func__);
		return -ENODEV;
	}
	ret = regmap_read(map, RegNum, val);
	if (ret) {
		dev_notice(chip->dev,
			"[%s]ret=%d Reg=0x%x MASK=0x%x SHIFT=%d\n",
			__func__, ret, RegNum, MASK, SHIFT);
		return ret;
	}
	*val &= (MASK << SHIFT);
	*val >>= SHIFT;
	PMICLOG("[%s] (0x%x,0x%x,0x%x,0x%x)\n",
		__func__, RegNum, *val, MASK, SHIFT);
#else
	pr_info("[%s] Can not access HW PMIC(FPGA?/PWRAP?)\n", __func__);
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return ret;
}

static int pmic_write_device(struct regmap *map,
			     unsigned int RegNum,
			     unsigned int val,
			     unsigned int MASK,
			     unsigned int SHIFT)
{
	int ret = 0;
#if defined(CONFIG_PMIC_HW_ACCESS_EN)
	if (!map) {
		pr_notice("[%s] regmap not ready\n", __func__);
		return -ENODEV;
	}
	ret = regmap_update_bits(map, RegNum, (MASK << SHIFT), (val << SHIFT));
	if (ret) {
		dev_notice(chip->dev,
			"[%s]ret=%d Reg=0x%x val=0x%x MASK=0x%x SHIFT=%d\n",
			__func__, ret, RegNum, val, MASK, SHIFT);
		return ret;
	}
	PMICLOG("[%s] (0x%x,0x%x,0x%x,%d)\n",
		__func__, RegNum, val, MASK, SHIFT);
#else
	PMICLOG("[%s] Can not access HW PMIC(FPGA?/PWRAP?)\n", __func__);
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return ret;
}

unsigned int pmic_read_interface(unsigned int RegNum,
				 unsigned int *val,
				 unsigned int MASK,
				 unsigned int SHIFT)
{
	int ret;

	if (!chip) {
		pr_notice("[%s] PMIC not ready\n", __func__);
		return -ENODEV;
	}
	ret = pmic_read_device(chip->regmap, RegNum, val, MASK, SHIFT);
	return abs(ret);
}

unsigned int pmic_config_interface(unsigned int RegNum,
				   unsigned int val,
				   unsigned int MASK,
				   unsigned int SHIFT)
{
	int ret;

	if (preempt_count() > 0 ||
	    irqs_disabled() ||
	    system_state != SYSTEM_RUNNING ||
	    oops_in_progress)
		return pmic_config_interface_nolock(RegNum, val, MASK, SHIFT);

	if (!chip) {
		pr_notice("[%s] PMIC not ready\n", __func__);
		return -ENODEV;
	}
	ret = pmic_write_device(chip->regmap, RegNum, val, MASK, SHIFT);
	return ret;
}

unsigned int pmic_read_interface_nolock(unsigned int RegNum,
					unsigned int *val,
					unsigned int MASK,
					unsigned int SHIFT)
{
	int ret;

	ret = pmic_read_device(pmic_nolock_regmap, RegNum, val, MASK, SHIFT);
	return abs(ret);
}

unsigned int pmic_config_interface_nolock(unsigned int RegNum,
					  unsigned int val,
					  unsigned int MASK,
					  unsigned int SHIFT)
{
	int ret = 0;

	ret = pmic_write_device(pmic_nolock_regmap, RegNum, val, MASK, SHIFT);
	return abs(ret);
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
	return pmic_set_register_value(flagname, val);
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
	void __user *user_data = (void __user *)arg;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	/* adc_in_data is used to check userspace data size */
	if (copy_from_user(adc_in_data, user_data, sizeof(adc_in_data)))
		return -EFAULT;
	switch (cmd) {
	case Get_IS_EXT_BUCK_EXIST:
#ifdef CONFIG_MTK_EXTBUCK
		adc_out_data[0] = is_ext_buck_exist();
#else
		adc_out_data[0] = 0;
#endif
		PMICLOG("[%s] Get_IS_EXT_BUCK_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	case Get_IS_EXT_VBAT_BOOST_EXIST:
		adc_out_data[0] = is_ext_vbat_boost_exist();
		PMICLOG("[%s] Get_IS_EXT_VBAT_BOOST_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	case Get_IS_EXT_SWCHR_EXIST:
		adc_out_data[0] = is_ext_swchr_exist();
		PMICLOG("[%s] Get_IS_EXT_SWCHR_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	case Get_IS_EXT_BUCK2_EXIST:
#ifdef CONFIG_MTK_EXTBUCK
		adc_out_data[0] = is_ext_buck2_exist();
#else
		adc_out_data[0] = 0;
#endif
		PMICLOG("[%s] Get_IS_EXT_BUCK2_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	case Get_IS_EXT_BUCK3_EXIST:
#ifdef CONFIG_MTK_EXTBUCK
		/* just for UI showing CONNECTED */
		adc_out_data[0] = is_ext_buck2_exist();
#else
		adc_out_data[0] = 0;
#endif
		PMICLOG("[%s] Get_IS_EXT_BUCK3_EXIST:%d\n"
				, __func__, adc_out_data[0]);
		break;
	default:
		PMICLOG("[%s] Error ID\n", __func__);
		return -EINVAL;
	}
	if (copy_to_user(user_data, adc_out_data, sizeof(adc_out_data)))
		return -EFAULT;

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

}

#ifdef IPIMB
static struct regmap *pmic_read_regmap;

static int pmic_ipi_reg_write(void *context, const void *data,
			      size_t count)
{
	unsigned int ret;
	unsigned short *dout = (unsigned short *)data;
	unsigned short reg = dout[0], val = dout[1];

	if (count != 4) {
		pr_notice("%s: reg=0x%x, val=0x%x, count=%zu\n",
			__func__, reg, val, count);
		return -EINVAL;
	}
	ret = pmic_ipi_config_interface(reg, val, 0xFFFF, 0, 1);
	if (ret) {
		pr_info("[%s]fail with ret=%d, reg=0x%x val=0x%x\n",
			__func__, ret, reg, val);
		return -EINVAL;
	}
	return 0;
}

static int pmic_ipi_reg_read(void *context,
			     const void *reg_buf, size_t reg_size,
			     void *val_buf, size_t val_size)
{
	unsigned short reg = *(unsigned short *)reg_buf;

	if (reg_size != 2 || val_size != 2) {
		pr_notice("%s: reg=0x%x, reg_size=%zu, val_size=%zu\n",
			__func__, reg, reg_size, val_size);
		return -EINVAL;
	}

	return regmap_read(pmic_read_regmap, reg, val_buf);
}

static int pmic_ipi_reg_update_bits(void *context, unsigned int reg,
			      unsigned int mask, unsigned int val)
{
	unsigned int ret;

	ret = pmic_ipi_config_interface(reg, val, mask, 0, 1);
	if (ret) {
		pr_info("[%s]fail with ret=%d, reg=0x%x mask=0x%x val=0x%x\n",
			__func__, ret, reg, mask, val);
		return -EINVAL;
	}
	return 0;
}

static void pmic_ipi_regmap_unlock_empty(void *__map)
{

}

static const struct regmap_config pmic_ipi_regmap_config = {
	.name = "pmic_ipi",
	.reg_bits = 16,
	.val_bits = 16,
	.reg_stride = 2,
	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};


static const struct regmap_config pmic_ipi_nolock_regmap_config = {
	.name = "pmic_ipi_nolock",
	.reg_bits = 16,
	.val_bits = 16,
	.lock = pmic_ipi_regmap_unlock_empty,
	.unlock = pmic_ipi_regmap_unlock_empty,
	.reg_stride = 2,
	.max_register = 0xffff,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static struct regmap_bus regmap_pmic_ipi_bus = {
	.write = pmic_ipi_reg_write,
	.read = pmic_ipi_reg_read,
	.reg_update_bits = pmic_ipi_reg_update_bits,
	.fast_io = true,
};
#endif

static int pmic_mt_probe(struct platform_device *pdev)
{
	int ret = 0;

	chip = dev_get_drvdata(pdev->dev.parent);
#ifdef IPIMB
	pmic_read_regmap = dev_get_regmap(chip->dev->parent, NULL);
	chip->regmap = devm_regmap_init(&pdev->dev, &regmap_pmic_ipi_bus,
					   NULL, &pmic_ipi_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_notice(&pdev->dev, "failed to init IPI regmap\n");
		chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	}


	pmic_nolock_regmap = devm_regmap_init(&pdev->dev,
					      &regmap_pmic_ipi_bus,
					      NULL,
					      &pmic_ipi_nolock_regmap_config);
	if (IS_ERR(pmic_nolock_regmap)) {
		ret = PTR_ERR(pmic_nolock_regmap);
		dev_notice(&pdev->dev, "failed to init nolock regmap\n");
		pmic_nolock_regmap = chip->regmap;
	}
#else
	pmic_nolock_regmap = chip->regmap;
#endif

	pr_info("******** MT pmic driver probe!! ********\n");
	/*get PMIC CID */
	PMICLOG("PMIC CID = 0x%x\n", pmic_get_register_value(PMIC_SWCID));

#if defined(CONFIG_MACH_MT6781)
	record_is_pmic_new_power_grid(pdev);
#endif
	record_md_vosel();

	PMIC_INIT_SETTING_V1();
	PMICLOG("[PMIC_INIT_SETTING_V1] Done\n");

	/*PMIC Interrupt Service*/
	PMIC_EINT_SETTING(pdev);
	PMICLOG("[PMIC_EINT_SETTING] Done\n");

	mtk_regulator_init(to_platform_device(pdev->dev.parent));
	PMICLOG("[PMIC] mtk_regulator_init : done.\n");

	pmic_throttling_dlpt_init(pdev);

	PMICLOG("[PMIC] pmic_throttling_dlpt_init : done.\n");

	pmic_debug_init(pdev);
	PMICLOG("[PMIC] pmic_debug_init : done.\n");

	pmic_ftm_init();

	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT))
		pmic_bif_init();

	return ret;
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
	{.compatible = "mediatek,mt-pmic"},
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

	ret = platform_driver_register(&pmic_mt_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n",
			__func__, ret);
		return ret;
	}
	pr_info("****[%s] Initialization : DONE !!\n", __func__);
	return 0;
}
fs_initcall(pmic_mt_init);

static void __exit pmic_mt_exit(void)
{
	platform_driver_unregister(&pmic_mt_driver);
}
module_exit(pmic_mt_exit);

MODULE_AUTHOR("Jimmy-YJ Huang");
MODULE_DESCRIPTION("MTK PMIC COMMON Interface Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0_M");
