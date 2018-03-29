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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
/*
#if !defined CONFIG_HAS_WAKELOCKS
#include <linux/pm_wakeup.h>  included in linux/device.h
#else
*/
#include <linux/wakelock.h>
/*#endif*/
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <mt-plat/aee.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#endif
/*#include <asm/uaccess.h>*/
#include <linux/uaccess.h>

#if !defined CONFIG_MTK_LEGACY
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#endif
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_irq.h"
#include "include/pmic_throttling_dlpt.h"
/*#include <mach/eint.h> TBD*/
/*#include <mach/mt_pmic_wrap.h>*/
#include "pwrap_hal.h"
#if defined CONFIG_MTK_LEGACY
#include <mt-plat/mt_gpio.h>
#endif
#include <mt-plat/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <linux/time.h>

/*#include "include/pmic_dvt.h"*/

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_boot_common.h>
/*#include <mach/system.h> TBD*/
#include <mt-plat/mt_gpt.h>
#endif

#if defined(CONFIG_MTK_SMART_BATTERY)
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_battery_meter.h>
#endif
#include <mach/mt_pmic.h>
#include <mt-plat/mt_reboot.h>
#ifdef CONFIG_MTK_AUXADC_INTF
#include <mt-plat/mtk_auxadc_intf.h>
#endif /* CONFIG_MTK_AUXADC_INTF */

/*wilma debug*/
#include "include/pmic_efuse.h"

#if defined(CONFIG_MTK_EXTBUCK)
#include "include/extbuck/fan53526.h"
#endif

#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
#include "include/mt6313/mt6313.h"
#endif
/*****************************************************************************
 * PMIC related define
 ******************************************************************************/
static DEFINE_MUTEX(pmic_lock_mutex);
#define PMIC_EINT_SERVICE

/*****************************************************************************
 * PMIC read/write APIs
 ******************************************************************************/
#if 0				/*defined(CONFIG_FPGA_EARLY_PORTING)*/
    /* no CONFIG_PMIC_HW_ACCESS_EN */
#else
#define CONFIG_PMIC_HW_ACCESS_EN
#endif

#ifdef CONFIG_OF
#if !defined CONFIG_MTK_LEGACY
/*
static int pmic_mt_cust_probe(struct platform_device *pdev);
static int pmic_mt_cust_remove(struct platform_device *pdev);
static int pmic_regulator_ldo_init(struct platform_device *pdev);
*/
#endif
#endif				/* End of #ifdef CONFIG_OF */

/*---IPI Mailbox define---*/
/*#define IPIMB*/
#if defined(IPIMB)
#include <mach/mt_pmic_ipi.h>
#endif
static DEFINE_MUTEX(pmic_access_mutex);
/*--- Global suspend state ---*/
static bool pmic_suspend_state;

void vmd1_pmic_setting_on(void)
{
	/*---VMD1, VMODEM, VSRAM_VMD ENABLE---*/
	pmic_set_register_value(PMIC_RG_BUCK_VMD1_EN, 1);
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_EN, 1);
	pmic_set_register_value(PMIC_RG_VSRAM_VMD_SW_EN, 1);
	udelay(220);

	if (!pmic_get_register_value(PMIC_DA_QI_VMD1_EN) ||
		!pmic_get_register_value(PMIC_DA_QI_VMODEM_EN) ||
			!pmic_get_register_value(PMIC_DA_QI_VSRAM_VMD_EN))
			pr_err("[vmd1_pmic_setting_on] VMD1 = %d, VMODEM = %d, VSRAM_VMD = %d\n",
				pmic_get_register_value(PMIC_DA_QI_VMD1_EN),
				pmic_get_register_value(PMIC_DA_QI_VMODEM_EN),
				pmic_get_register_value(PMIC_DA_QI_VSRAM_VMD_EN));

	/*---VMD1, VMODEM, VSRAM_VMD Voltage Select---*/
	/*--0x40 (0x40*0.00625+0.4 =0.8V)--*/
	pmic_set_register_value(PMIC_RG_BUCK_VMD1_VOSEL, 0x40);
	/*--0x10 (0x10*0.00625+0.4 =0.5V) SLEEP_VOLTAGE & VOSEL_SLEEP need the same --*/
	pmic_set_register_value(PMIC_RG_BUCK_VMD1_VOSEL_SLEEP, 0x10);
	/*--0x40 (0x40*0.00625+0.4 =0.8V)--*/
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL, 0x40);
	/*--0x10 (0x10*0.00625+0.4 =0.5V) SLEEP_VOLTAGE & VOSEL_SLEEP need the same --*/
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_VOSEL_SLEEP, 0x10);
	/*--0x50 (0x50*0.00625+0.4 =0.9V)--*/
	pmic_set_register_value(PMIC_RG_VSRAM_VMD_VOSEL, 0x50);
	/*--0x10 (0x10*0.00625+0.4 =0.5V) SLEEP_VOLTAGE & VOSEL_SLEEP need the same --*/
	pmic_set_register_value(PMIC_RG_VSRAM_VMD_VOSEL_SLEEP, 0x10);
}

void vmd1_pmic_setting_off(void)
{
	/*---VMD1, VMODEM, VSRAM_VMD DISABLE---*/
	pmic_set_register_value(PMIC_RG_BUCK_VMD1_EN, 0);
	pmic_set_register_value(PMIC_RG_BUCK_VMODEM_EN, 0);
	pmic_set_register_value(PMIC_RG_VSRAM_VMD_SW_EN, 0);
	udelay(220);

	if (pmic_get_register_value(PMIC_DA_QI_VMD1_EN) ||
		pmic_get_register_value(PMIC_DA_QI_VMODEM_EN) ||
			pmic_get_register_value(PMIC_DA_QI_VSRAM_VMD_EN))
			pr_err("[vmd1_pmic_setting_off] VMD1 = %d, VMODEM = %d, VSRAM_VMD = %d\n",
				pmic_get_register_value(PMIC_DA_QI_VMD1_EN),
				pmic_get_register_value(PMIC_DA_QI_VMODEM_EN),
				pmic_get_register_value(PMIC_DA_QI_VSRAM_VMD_EN));

}

unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
#ifdef IPIMB
	*val = 0;
	PMICLOG("[pmic_read_interface] IPIMB\n");
#else
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	if ((pmic_suspend_state == true) && irqs_disabled())
		return pmic_read_interface_nolock(RegNum, val, MASK, SHIFT);
	mutex_lock(&pmic_access_mutex);

	/*mt_read_byte(RegNum, &pmic_reg);*/
	/*return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);*/
	return_value = pwrap_wacs2_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/*PMICLOG"[pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);*/

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);
	/*PMICLOG"[pmic_read_interface] val=0x%x\n", *val);*/

	mutex_unlock(&pmic_access_mutex);
#endif /*---IPIMB---*/
#else
	/*PMICLOG("[pmic_read_interface] Can not access HW PMIC\n");*/
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
#ifdef IPIMB

	return_value = pmic_ipi_config_interface(RegNum, val, MASK, SHIFT);

	if (return_value)
		PMICLOG("[pmic_read_interface] IPIMB write data fail\n");
	else
		PMICLOG("[pmic_read_interface] IPIMB write data =(%x,%x,%x,%x)\n", RegNum, val, MASK, SHIFT);

#else
	unsigned int pmic_reg = 0;
	unsigned int rdata;

	if ((pmic_suspend_state == true) && irqs_disabled())
		return pmic_config_interface_nolock(RegNum, val, MASK, SHIFT);
	mutex_lock(&pmic_access_mutex);

	/*1. mt_read_byte(RegNum, &pmic_reg);*/
	/*return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);*/
	return_value = pwrap_wacs2_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/*PMICLOG"[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);*/

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	/*2. mt_write_byte(RegNum, pmic_reg);*/
	/*return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);*/
	return_value = pwrap_wacs2_write((RegNum), pmic_reg);
	if (return_value != 0) {
		pr_err("[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	/*PMICLOG"[pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg);*/
#if 0
	/*3. Double Check*/
	/*mt_read_byte(RegNum, &pmic_reg);*/
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		PMICLOG("[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	PMICLOG("[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
#endif

	mutex_unlock(&pmic_access_mutex);
#endif /*---IPIMB---*/
#else
	/*PMICLOG("[pmic_config_interface] Can not access HW PMIC\n");*/
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

unsigned int pmic_read_interface_nolock(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
#ifdef IPIMB
	*val = 0;
	PMICLOG("[pmic_read_interface_nolock] IPIMB\n");
#else
	unsigned int pmic_reg = 0;
	unsigned int rdata;


	/*mt_read_byte(RegNum, &pmic_reg); */
	/*return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);*/
	return_value = pwrap_wacs2_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[pmic_read_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/*PMICLOG"[pmic_read_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= (MASK << SHIFT);
	*val = (pmic_reg >> SHIFT);
	/*PMICLOG"[pmic_read_interface] val=0x%x\n", *val); */
#endif /*---IPIMB---*/
#else
	/*PMICLOG("[pmic_read_interface] Can not access HW PMIC\n"); */
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

unsigned int pmic_config_interface_nolock(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	unsigned int return_value = 0;

#if defined(CONFIG_PMIC_HW_ACCESS_EN)
#ifdef IPIMB
#else
	unsigned int pmic_reg = 0;
	unsigned int rdata;

    /* pmic wrapper has spinlock protection. pmic do not to do it again */

	/*1. mt_read_byte(RegNum, &pmic_reg); */
	/*return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);*/
	return_value = pwrap_wacs2_read((RegNum), &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		pr_err("[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/*PMICLOG"[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg); */

	pmic_reg &= ~(MASK << SHIFT);
	pmic_reg |= (val << SHIFT);

	/*2. mt_write_byte(RegNum, pmic_reg); */
	/*return_value = pwrap_wacs2(1, (RegNum), pmic_reg, &rdata);*/
	return_value = pwrap_wacs2_write((RegNum), pmic_reg);
	if (return_value != 0) {
		pr_err("[pmic_config_interface] Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}
	/*PMICLOG"[pmic_config_interface] write Reg[%x]=0x%x\n", RegNum, pmic_reg); */

#if 0
	/*3. Double Check */
	/*mt_read_byte(RegNum, &pmic_reg); */
	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic_reg = rdata;
	if (return_value != 0) {
		PMICLOG("[pmic_config_interface] Reg[%x]= pmic_wrap write data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}
	PMICLOG("[pmic_config_interface] Reg[%x]=0x%x\n", RegNum, pmic_reg);
#endif
#endif /*---IPIMB---*/

#else
	/*PMICLOG("[pmic_config_interface] Can not access HW PMIC\n"); */
#endif	/*defined(CONFIG_PMIC_HW_ACCESS_EN)*/

	return return_value;
}

/*****************************************************************************
 * PMIC lock/unlock APIs
 ******************************************************************************/
void pmic_lock(void)
{
	mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
	mutex_unlock(&pmic_lock_mutex);
}

unsigned int upmu_get_reg_value(unsigned int reg)
{
	unsigned int reg_val = 0;

#ifdef IPIMB
	reg_val = 0;
	PMICLOG("[pmic_read_interface] IPIMB\n");
#else
	unsigned int ret = 0;

	ret = pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);
#endif
	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(unsigned int reg, unsigned int reg_val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

unsigned int get_pmic_mt6325_cid(void)
{
	return 0;
}

unsigned int get_mt6325_pmic_chip_version(void)
{
	return 0;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void mt6335_dump_register(void)
{
	unsigned char i = 0;

	PMICLOG("dump PMIC 6335 register\n");

	for (i = 0; i <= 0x0fae; i = i + 10) {
		pr_debug
		    ("Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n",
		     i, upmu_get_reg_value(i), i + 1, upmu_get_reg_value(i + 1), i + 2,
		     upmu_get_reg_value(i + 2), i + 3, upmu_get_reg_value(i + 3), i + 4,
		     upmu_get_reg_value(i + 4));

		pr_debug
		    ("Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x Reg[0x%x]=0x%x\n",
		     i + 5, upmu_get_reg_value(i + 5), i + 6, upmu_get_reg_value(i + 6), i + 7,
		     upmu_get_reg_value(i + 7), i + 8, upmu_get_reg_value(i + 8), i + 9,
		     upmu_get_reg_value(i + 9));
	}

}

/*****************************************************************************
 * upmu_interrupt_chrdet_int_en
 ******************************************************************************/
void upmu_interrupt_chrdet_int_en(unsigned int val)
{
	PMICLOG("[upmu_interrupt_chrdet_int_en] val=%d.\r\n", val);

	/*mt6325_upmu_set_rg_int_en_chrdet(val); */
	pmic_set_register_value(PMIC_RG_INT_EN_CHRDET, val);
}
EXPORT_SYMBOL(upmu_interrupt_chrdet_int_en);


/*****************************************************************************
 * PMIC charger detection
 ******************************************************************************/
unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int val = 0;

	/*val = mt6325_upmu_get_rgs_chrdet();*/
	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	PMICLOG("[upmu_get_rgs_chrdet] CHRDET status = %d\n", val);

	return val;
}

/*****************************************************************************
 * mt-pmic dev_attr APIs
 ******************************************************************************/
unsigned int g_reg_value = 0;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_err("[show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%u\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_err("[store_pmic_access]\n");
	if (buf != NULL && size != 0) {
		pr_err("[store_pmic_access] buf is %s\n", buf);
		/*reg_address = simple_strtoul(buf, &pvalue, 16);*/

		pvalue = (char *)buf;
		if (size > 5) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16, (unsigned int *)&reg_address);

		if (size > 5) {
			/*reg_value = simple_strtoul((pvalue + 1), NULL, 16);*/
			/*pvalue = (char *)buf + 1;*/
			val =  strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			pr_err("[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_err("[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value);
			pr_err("[store_pmic_access] use \"cat pmic_access\" to get value(decimal)\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/*664*/

/*
 * DVT entry
 */
unsigned char g_reg_value_pmic = 0;

static ssize_t show_pmic_dvt(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_err("[show_pmic_dvt] 0x%x\n", g_reg_value_pmic);
	return sprintf(buf, "%u\n", g_reg_value_pmic);
}

static ssize_t store_pmic_dvt(struct device *dev, struct device_attribute *attr, const char *buf,
			      size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int test_item = 0;

	pr_err("[store_pmic_dvt]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_pmic_dvt] buf is %s and size is %zu\n", buf, size);

		/*test_item = simple_strtoul(buf, &pvalue, 10);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&test_item);
		pr_err("[store_pmic_dvt] test_item=%d\n", test_item);

#ifdef MTK_PMIC_DVT_SUPPORT
		pmic_dvt_entry(test_item);
#else
		pr_err("[store_pmic_dvt] no define MTK_PMIC_DVT_SUPPORT\n");
#endif
	}
	return size;
}

static DEVICE_ATTR(pmic_dvt, 0664, show_pmic_dvt, store_pmic_dvt);

/*
 * auxadc
 */
unsigned char g_auxadc_pmic = 0;

static ssize_t show_pmic_auxadc(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_err("[show_pmic_auxadc] 0x%x\n", g_auxadc_pmic);
	return sprintf(buf, "%u\n", g_auxadc_pmic);
}

static ssize_t store_pmic_auxadc(struct device *dev, struct device_attribute *attr, const char *buf,
			      size_t size)
{
	int ret = 0, i, j;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_err("[store_pmic_auxadc]\n");

	if (buf != NULL && size != 0) {
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		for (i = 0; i < val; i++) {
		#ifdef CONFIG_MTK_AUXADC_INTF
			for (j = 0; j < AUXADC_LIST_MAX; j++) {
				pr_err("[PMIC_AUXADC] [%s]=%d\n",
						pmic_get_auxadc_name(j),
						pmic_get_auxadc_value(j));
				mdelay(5);
			}
		#else /* not config CONFIG_MTK_AUXADC_INTF */
			for (j = 0; j < 16; j++) {
				pr_err("[PMIC_AUXADC] [%d]=%d\n", j, PMIC_IMM_GetOneChannelValue(j, 0, 0));
				mdelay(5);
			}
			pr_err("[PMIC_AUXADC] [%d]=%d\n", j, PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH4_DCXO, 0, 0));
		#endif /* CONFIG_MTK_AUXADC_INTF */
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_auxadc_ut, 0664, show_pmic_auxadc, store_pmic_auxadc);

int pmic_rdy = 0, usb_rdy = 0;
void pmic_enable_charger_detection_int(int x)
{

	if (x == 0) {
		pmic_rdy = 1;
		PMICLOG("[pmic_enable_charger_detection_int] PMIC\n");
	} else if (x == 1) {
		usb_rdy = 1;
		PMICLOG("[pmic_enable_charger_detection_int] USB\n");
	}

	PMICLOG("[pmic_enable_charger_detection_int] pmic_rdy=%d usb_rdy=%d\n", pmic_rdy, usb_rdy);
	if (pmic_rdy == 1 && usb_rdy == 1) {
#if defined(CONFIG_MTK_SMART_BATTERY)
		wake_up_bat();
#endif
		PMICLOG("[pmic_enable_charger_detection_int] enable charger detection interrupt\n");
	}
}

bool is_charger_detection_rdy(void)
{

	if (pmic_rdy == 1 && usb_rdy == 1)
		return true;
	else
		return false;
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
#if 0
#if defined(CONFIG_FPGA_EARLY_PORTING)
	return 0;
#else
#if !defined CONFIG_MTK_LEGACY
	return gpiod_get_value(gpio_to_desc(130));
	/*return __gpio_get_value(130);*/
	/*return mt_get_gpio_in(130);*/
#else
	return 0;
#endif
#endif
#endif
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
/*
#ifdef MTK_BQ24261_SUPPORT
extern int is_bq24261_exist(void);
#endif

int is_ext_swchr_exist(void)
{
    #ifdef MTK_BQ24261_SUPPORT
	if (is_bq24261_exist() == 1)
		return 1;
	else
		return 0;
    #else
	PMICLOG("[is_ext_swchr_exist] no define any HW\n");
	return 0;
    #endif
}
*/

/*****************************************************************************
 * Enternal VBAT Boost status
 ******************************************************************************/
/*
extern int is_tps6128x_sw_ready(void);
extern int is_tps6128x_exist(void);

int is_ext_vbat_boost_sw_ready(void)
{
    if( (is_tps6128x_sw_ready()==1) )
	return 1;
    else
	return 0;
}

int is_ext_vbat_boost_exist(void)
{
    if( (is_tps6128x_exist()==1) )
	return 1;
    else
	return 0;
}

*/

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


/*****************************************************************************
 * FTM
 ******************************************************************************/
#define PMIC_DEVNAME "pmic_ftm"
#define Get_IS_EXT_BUCK_EXIST _IOW('k', 20, int)
#define Get_IS_EXT_VBAT_BOOST_EXIST _IOW('k', 21, int)
#define Get_IS_EXT_SWCHR_EXIST _IOW('k', 22, int)
#define Get_IS_EXT_BUCK2_EXIST _IOW('k', 23, int)


static struct class *pmic_class;
static struct cdev *pmic_cdev;
static int pmic_major;
static dev_t pmic_devno;

static long pmic_ftm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
		/*adc_out_data[0] = is_ext_buck_exist();*/
		adc_out_data[0] = is_ext_buck_gpio_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[pmic_ftm_ioctl] Get_IS_EXT_BUCK_EXIST:%d\n", adc_out_data[0]);
		break;
		/*#endif*/

		/*#if defined(FTM_EXT_VBAT_BOOST_CHECK)*/
	case Get_IS_EXT_VBAT_BOOST_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = is_ext_vbat_boost_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[pmic_ftm_ioctl] Get_IS_EXT_VBAT_BOOST_EXIST:%d\n", adc_out_data[0]);
		break;
		/*#endif*/

		/*#if defined(FEATURE_FTM_SWCHR_HW_DETECT)*/
	case Get_IS_EXT_SWCHR_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = is_ext_swchr_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[pmic_ftm_ioctl] Get_IS_EXT_SWCHR_EXIST:%d\n", adc_out_data[0]);
		break;
		/*#endif*/
	case Get_IS_EXT_BUCK2_EXIST:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = is_ext_buck2_exist();
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		PMICLOG("[pmic_ftm_ioctl] Get_IS_EXT_BUCK2_EXIST:%d\n", adc_out_data[0]);
		break;
	default:
		PMICLOG("[pmic_ftm_ioctl] Error ID\n");
		break;
	}

	return 0;
}
#ifdef CONFIG_COMPAT
static long pmic_ftm_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
		PMICLOG("[pmic_ftm_compat_ioctl] Error ID\n");
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
		PMICLOG("[pmic_ftm_init] Error: Can't Get Major number for pmic_ftm\n");

	pmic_cdev = cdev_alloc();
	pmic_cdev->owner = THIS_MODULE;
	pmic_cdev->ops = &pmic_ftm_fops;

	ret = cdev_add(pmic_cdev, pmic_devno, 1);
	if (ret)
		PMICLOG("[pmic_ftm_init] Error: cdev_add\n");

	pmic_major = MAJOR(pmic_devno);
	pmic_class = class_create(THIS_MODULE, PMIC_DEVNAME);

	class_dev = (struct class_device *)device_create(pmic_class,
							 NULL, pmic_devno, NULL, PMIC_DEVNAME);

	PMICLOG("[pmic_ftm_init] Done\n");
}


/*****************************************************************************
 * HW Setting
 ******************************************************************************/
unsigned short is_battery_remove = 0;
unsigned short is_wdt_reboot_pmic = 0;
unsigned short is_wdt_reboot_pmic_chk = 0;

unsigned short is_battery_remove_pmic(void)
{
	return is_battery_remove;
}

void PMIC_CUSTOM_SETTING_V1(void)
{
#if 0
#if defined CONFIG_MTK_LEGACY
#if defined(CONFIG_FPGA_EARLY_PORTING)
#else
	pmu_drv_tool_customization_init();	/* legacy DCT only */
#endif
#endif
#endif
}

static int proc_dump_register_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "********** dump PMIC registers**********\n");

	for (i = 0; i <= 0x0fae; i = i + 10) {
		seq_printf(m, "Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x\n",
			i, upmu_get_reg_value(i), i + 1, upmu_get_reg_value(i + 1), i + 2,
			upmu_get_reg_value(i + 2), i + 3, upmu_get_reg_value(i + 3), i + 4,
			upmu_get_reg_value(i + 4));

		seq_printf(m, "Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x Reg[%x]=0x%x\n",
			i + 5, upmu_get_reg_value(i + 5), i + 6, upmu_get_reg_value(i + 6),
			i + 7, upmu_get_reg_value(i + 7), i + 8, upmu_get_reg_value(i + 8),
			i + 9, upmu_get_reg_value(i + 9));
	}

	return 0;
}

static int proc_dump_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_dump_register_show, NULL);
}

static const struct file_operations pmic_dump_register_proc_fops = {
	.open = proc_dump_register_open,
	.read = seq_read,
};

void pmic_debug_init(struct platform_device *dev)
{
	struct dentry *mtk_pmic_dir;

	mtk_pmic_dir = debugfs_create_dir("mtk_pmic", NULL);
	if (!mtk_pmic_dir) {
		PMICLOG("fail to mkdir /sys/kernel/debug/mtk_pmic\n");
		return;
	}

	debugfs_create_file("dump_pmic_reg", S_IRUGO | S_IWUSR, mtk_pmic_dir,
		NULL, &pmic_dump_register_proc_fops);

	pmic_regulator_debug_init(dev, mtk_pmic_dir);
	pmic_throttling_dlpt_debug_init(dev, mtk_pmic_dir);
	PMICLOG("proc_create pmic_dump_register_proc_fops\n");

}

static bool pwrkey_detect_flag;
static struct hrtimer pwrkey_detect_timer;
static struct task_struct *pwrkey_detect_thread;
static DECLARE_WAIT_QUEUE_HEAD(pwrkey_detect_waiter);

#define BAT_MS_TO_NS(x) (x * 1000 * 1000)

enum hrtimer_restart pwrkey_detect_sw_workaround(struct hrtimer *timer)
{
	pwrkey_detect_flag = true;

	wake_up_interruptible(&pwrkey_detect_waiter);
	return HRTIMER_NORESTART;
}

int pwrkey_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;

	do {
		ktime = ktime_set(3, BAT_MS_TO_NS(1000));



		wait_event_interruptible(pwrkey_detect_waiter, (pwrkey_detect_flag == true));

		/*PMICLOG("=>charger_hv_detect_sw_workaround\n"); */
		if (pmic_get_register_value(PMIC_RG_STRUP_75K_CK_PDN) == 1) {
			PMICLOG("charger_hv_detect_sw_workaround =0x%x\n",
				upmu_get_reg_value(0x24e));
			pmic_set_register_value(PMIC_RG_STRUP_75K_CK_PDN, 0);
		}


		hrtimer_start(&pwrkey_detect_timer, ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;

}



void pwrkey_sw_workaround_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, BAT_MS_TO_NS(2000));
	hrtimer_init(&pwrkey_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pwrkey_detect_timer.function = pwrkey_detect_sw_workaround;
	hrtimer_start(&pwrkey_detect_timer, ktime, HRTIMER_MODE_REL);

	pwrkey_detect_thread =
	    kthread_run(pwrkey_detect_sw_thread_handler, 0, "mtk pwrkey_sw_workaround_init");

	if (IS_ERR(pwrkey_detect_thread))
		pr_err("[%s]: failed to create pwrkey_detect_thread thread\n", __func__);

}

/*****************************************************************************
 * system function
 ******************************************************************************/
static int pmic_mt_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	PMICLOG("******** MT pmic driver probe!! ********\n");
	/*get PMIC CID */
	PMICLOG
	    ("PMIC CID=0x%x PowerGoodStatus = 0x%x OCStatus = 0x%x ThermalStatus = 0x%x rsvStatus = 0x%x\n",
	     pmic_get_register_value(PMIC_SWCID), upmu_get_reg_value(0x21c),
	     upmu_get_reg_value(0x214), upmu_get_reg_value(0x21e), upmu_get_reg_value(0x2a6));

	/* upmu_set_reg_value(0x2a6, 0xff); */ /* TBD */

	/*pmic initial setting */
	PMIC_INIT_SETTING_V1();
	PMICLOG("[PMIC_INIT_SETTING_V1] Done\n");
#if !defined CONFIG_MTK_LEGACY
/*      replace by DTS*/
#else
	PMIC_CUSTOM_SETTING_V1();
	PMICLOG("[PMIC_CUSTOM_SETTING_V1] Done\n");
#endif	/*End of #if !defined CONFIG_MTK_LEGACY */


/*#if defined(CONFIG_FPGA_EARLY_PORTING)*/
#if 0
	PMICLOG("[PMIC_EINT_SETTING] disable when CONFIG_FPGA_EARLY_PORTING\n");
#else
	/*PMIC Interrupt Service*/
	PMIC_EINT_SETTING();
	PMICLOG("[PMIC_EINT_SETTING] Done\n");
#endif

	mtk_regulator_init(dev);

	pmic_throttling_dlpt_init();

#if 0 /* jade do not use 26MHz for auxadc */
	pmic_set_register_value(PMIC_AUXADC_CK_AON, 1);
	pmic_set_register_value(PMIC_RG_CLKSQ_EN_AUX_AP_MODE, 0);
	PMICLOG("[PMIC] auxadc 26M test : Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		MT6351_AUXADC_CON0, upmu_get_reg_value(MT6351_AUXADC_CON0),
		MT6351_TOP_CLKSQ, upmu_get_reg_value(MT6351_TOP_CLKSQ)
	    );
#endif

	pmic_debug_init(dev);
	PMICLOG("[PMIC] pmic_debug_init : done.\n");

	pmic_ftm_init();

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_access);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_dvt);

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_pmic_auxadc_ut);

	PMICLOG("[PMIC] device_create_file for EM : done.\n");

	/*pwrkey_sw_workaround_init(); */
	return 0;
}

static int pmic_mt_remove(struct platform_device *dev)
{
	PMICLOG("******** MT pmic driver remove!! ********\n");

	return 0;
}

static void pmic_mt_shutdown(struct platform_device *dev)
{
	PMICLOG("******** MT pmic driver shutdown!! ********\n");
	vmd1_pmic_setting_on();
}

static int pmic_mt_suspend(struct platform_device *dev, pm_message_t state)
{
	pmic_suspend_state = true;

	PMICLOG("******** MT pmic driver suspend!! ********\n");

	pmic_throttling_dlpt_suspend();
	return 0;
}

static int pmic_mt_resume(struct platform_device *dev)
{
	pmic_suspend_state = false;

	PMICLOG("******** MT pmic driver resume!! ********\n");

	pmic_throttling_dlpt_resume();
	return 0;
}

struct platform_device pmic_mt_device = {
	.name = "mt-pmic",
	.id = -1,
};

static struct platform_driver pmic_mt_driver = {
	.probe = pmic_mt_probe,
	.remove = pmic_mt_remove,
	.shutdown = pmic_mt_shutdown,
	/*#ifdef CONFIG_PM*/
	.suspend = pmic_mt_suspend,
	.resume = pmic_mt_resume,
	/*#endif*/
	.driver = {
		   .name = "mt-pmic",
		   },
};

/*****************************************************************************
 * PMIC mudule init/exit
 ******************************************************************************/
static int __init pmic_mt_init(void)
{
	int ret;

#if !defined CONFIG_HAS_WAKELOCKS
	wakeup_source_init(&pmicThread_lock, "pmicThread_lock_mt6328 wakelock");
#else
	wake_lock_init(&pmicThread_lock, WAKE_LOCK_SUSPEND, "pmicThread_lock_mt6328 wakelock");
#endif

#if !defined CONFIG_MTK_LEGACY
/*#if !defined CONFIG_MTK_LEGACY*//*Jimmy*/
#ifdef CONFIG_OF
	PMICLOG("pmic_regulator_init_OF\n");

	/* PMIC device driver register*/
	ret = platform_device_register(&pmic_mt_device);
	if (ret) {
		pr_err("****[pmic_mt_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&pmic_mt_driver);
	if (ret) {
		pr_err("****[pmic_mt_init] Unable to register driver (%d)\n", ret);
		return ret;
	}
#endif				/* End of #ifdef CONFIG_OF */
#else
	PMICLOG("pmic_regulator_init\n");
	/* PMIC device driver register*/
	ret = platform_device_register(&pmic_mt_device);
	if (ret) {
		pr_err("****[pmic_mt_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&pmic_mt_driver);
	if (ret) {
		pr_err("****[pmic_mt_init] Unable to register driver (%d)\n", ret);
		return ret;
	}
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */


#ifdef CONFIG_MTK_AUXADC_INTF
	mtk_auxadc_init();
#else
	pmic_auxadc_init();
#endif /* CONFIG_MTK_AUXADC_INTF */

	pr_debug("****[pmic_mt_init] Initialization : DONE !!\n");

	return 0;
}

static void __exit pmic_mt_exit(void)
{
#if !defined CONFIG_MTK_LEGACY
#ifdef CONFIG_OF
	/*platform_driver_unregister(&mt_pmic_driver);*/
	platform_driver_unregister(&pmic_mt_driver);
#endif
#endif				/* End of #if !defined CONFIG_MTK_LEGACY */
}
fs_initcall(pmic_mt_init);

/*module_init(pmic_mt_init);*/
module_exit(pmic_mt_exit);

MODULE_AUTHOR("Argus Lin");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
