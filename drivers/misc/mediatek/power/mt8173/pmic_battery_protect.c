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

#include <linux/irq.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <mt-plat/mt_pmic_wrap.h>
#include <mt-plat/upmu_common.h>
#include "pmic_mt6397.h"
#include <linux/mfd/mt6397/core.h>

#define LBCB_NUM 16

#define HV_THD   0x2D5		/* 3.4V  -> 0x2D5 */
#define LV_1_THD 0x2B5		/* 3.25V -> 0x2B5 */
#define LV_2_THD 0x298		/* 3.1V  -> 0x298 */

static int low_battery_level;
static int low_battery_stop;

struct low_battery_callback_table {
	void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};

void (*low_battery_callback)(LOW_BATTERY_LEVEL);

void register_low_battery_notify(void (*low_battery_callback) (LOW_BATTERY_LEVEL),
				 LOW_BATTERY_PRIO prio_val)
{
	lbcb_tb[prio_val].lbcb = low_battery_callback;
	pr_debug("[Power/PMIC][register_low_battery_notify] prio_val=%d\n", prio_val);
}

void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level)
{				/* 0:no limit */
	int i = 0;

	if (low_battery_stop == 1) {
		pr_debug("[Power/PMIC]" "[exec_low_battery_callback] low_battery_stop=%d\n", low_battery_stop);
	} else {
		for (i = 0; i < LBCB_NUM; i++) {
			if (lbcb_tb[i].lbcb != NULL) {
				low_battery_callback = lbcb_tb[i].lbcb;
				low_battery_callback(low_battery_level);
				pr_debug("[Power/PMIC]" "[exec_low_battery_callback] prio_val=%d,low_battery=%d\n",
					i, low_battery_level);
			}
		}
	}
}

static inline void upmu_set_rg_lbat_en_min_nolock(u32 val)
{
	pmic_config_interface_nolock((u32) (AUXADC_CON6),
				     (u32) (val),
				     (u32) (PMIC_RG_LBAT_EN_MIN_MASK),
				     (u32) (PMIC_RG_LBAT_EN_MIN_SHIFT)
	    );
}

static inline void upmu_set_rg_lbat_irq_en_min_nolock(u32 val)
{
	pmic_config_interface_nolock((u32) (AUXADC_CON6),
				     (u32) (val),
				     (u32) (PMIC_RG_LBAT_IRQ_EN_MIN_MASK),
				     (u32) (PMIC_RG_LBAT_IRQ_EN_MIN_SHIFT)
	    );
}

static void upmu_set_rg_lbat_en_max_nolock(u32 val)
{
	pmic_config_interface_nolock((u32) (AUXADC_CON5),
				     (u32) (val),
				     (u32) (PMIC_RG_LBAT_EN_MAX_MASK),
				     (u32) (PMIC_RG_LBAT_EN_MAX_SHIFT)
	    );
}

static void upmu_set_rg_lbat_irq_en_max_nolock(u32 val)
{
	pmic_config_interface_nolock((u32) (AUXADC_CON5),
				     (u32) (val),
				     (u32) (PMIC_RG_LBAT_IRQ_EN_MAX_MASK),
				     (u32) (PMIC_RG_LBAT_IRQ_EN_MAX_SHIFT)
	    );
}

void lbat_min_en_setting_nolock(int en_val)
{
	upmu_set_rg_lbat_en_min_nolock(en_val);
	upmu_set_rg_lbat_irq_en_min_nolock(en_val);
}

void lbat_max_en_setting_nolock(int en_val)
{
	upmu_set_rg_lbat_en_max_nolock(en_val);
	upmu_set_rg_lbat_irq_en_max_nolock(en_val);
}

void lbat_min_en_setting(int en_val)
{
	upmu_set_rg_lbat_en_min(en_val);
	upmu_set_rg_lbat_irq_en_min(en_val);
}

void lbat_max_en_setting(int en_val)
{
	upmu_set_rg_lbat_en_max(en_val);
	upmu_set_rg_lbat_irq_en_max(en_val);
}

irqreturn_t bat_l_int_handler(int irq, void *dev_id)
{
	pr_warn("[bat_protect] %s:\n", __func__);

	low_battery_level++;
	if (low_battery_level > 2)
		low_battery_level = 2;

	if (low_battery_level == 1)
		exec_low_battery_callback(LOW_BATTERY_LEVEL_1);
	else if (low_battery_level == 2)
		exec_low_battery_callback(LOW_BATTERY_LEVEL_2);

	upmu_set_rg_lbat_volt_min(LV_2_THD);
	mdelay(1);

	pr_debug("[Power/PMIC] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", AUXADC_CON6,
		 upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5, upmu_get_reg_value(AUXADC_CON5));

	return IRQ_HANDLED;
}

irqreturn_t bat_h_int_handler(int irq, void *dev_id)
{
	pr_warn("[bat_protect] %s:\n", __func__);

	low_battery_level = 0;
	exec_low_battery_callback(LOW_BATTERY_LEVEL_0);

	upmu_set_rg_lbat_volt_min(LV_1_THD);
	mdelay(1);

	pr_debug("[Power/PMIC] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n", AUXADC_CON6,
		 upmu_get_reg_value(AUXADC_CON6), AUXADC_CON5, upmu_get_reg_value(AUXADC_CON5));

	return IRQ_HANDLED;
}

static ssize_t show_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	pr_debug("[Power/PMIC]" "[show_low_battery_protect_ut] low_battery_level=%d\n", low_battery_level);
	return sprintf(buf, "%u\n", low_battery_level);
}

static ssize_t store_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	u32 val = 0;
	s32 ret = 0;

	if (buf != NULL && size != 0) {
		pr_debug("[Power/PMIC] buf is %s and size is %lu\n", buf, size);
		ret = kstrtouint(buf, 0, &val);
		if (ret) {
			pr_err("[Power/PMIC]" "[store_low_battery_protect_ut] wrong number (%d)\n", val);
		} else {
			pr_debug("[Power/PMIC]" "[store_low_battery_protect_ut] your input is %d\n", val);
			exec_low_battery_callback(val);
		}
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_ut, 0664, show_low_battery_protect_ut,
							store_low_battery_protect_ut);	/* 664 */


static ssize_t show_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	pr_debug("[Power/PMIC]" "[show_low_battery_protect_stop] low_battery_stop=%d\n", low_battery_stop);
	return sprintf(buf, "%u\n", low_battery_stop);
}

static ssize_t store_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	s32 ret = 0;
	u32 val = 0;

	if (buf != NULL && size != 0) {
		pr_debug("[Power/PMIC] buf is %s and size is %lu\n", buf, size);
		ret = kstrtouint(buf, 0, &val);
		if ((val != 0) && (val != 1))
			val = 0;

		if (ret)
			pr_err("[Power/PMIC][store_low_battery_protect_stop] wrong format!");
		else {
			low_battery_stop = val;
			pr_debug("[Power/PMIC]" "[store_low_battery_protect_stop] low_battery_stop=%d\n",
			low_battery_stop);
		}
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_stop, 0664, show_low_battery_protect_stop,
							store_low_battery_protect_stop);	/* 664 */

static ssize_t show_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	pr_debug("[Power/PMIC]" "[show_low_battery_protect_level] low_battery_level=%d\n", low_battery_level);
	return sprintf(buf, "%u\n", low_battery_level);
}

static ssize_t store_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t size)
{
	pr_debug("[Power/PMIC]" "[store_low_battery_protect_level] low_battery_level=%d\n", low_battery_level);

	return size;
}

static DEVICE_ATTR(low_battery_protect_level, 0664, show_low_battery_protect_level,
								store_low_battery_protect_level);	/* 664 */


void toggle_lbat_irq_src(bool enable, u16 hw_irq)
{
	if (!enable) {
		if (hw_irq == MT6397_IRQ_BAT_L)
			lbat_min_en_setting_nolock(0);
		else if (hw_irq == MT6397_IRQ_BAT_H)
			lbat_max_en_setting_nolock(0);
	} else {
		if (hw_irq == MT6397_IRQ_BAT_L) {
			/* toggle L_BAT to enable L_BAT */
			if (low_battery_level < 2) {
				lbat_min_en_setting_nolock(0);
				lbat_min_en_setting_nolock(1);
			}
			/* enable H_BAT when hit L_BAT */
			lbat_max_en_setting_nolock(0);
			lbat_max_en_setting_nolock(1);
		} else if (hw_irq == MT6397_IRQ_BAT_H) {
			/* enable L_BAT when hit H_BAT */
			lbat_min_en_setting_nolock(0);
			lbat_min_en_setting_nolock(1);
		}
	}
}

void low_battery_protect_enable(void)
{
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
	mdelay(1);

	if (low_battery_level == 1) {
		lbat_min_en_setting(1);
		lbat_max_en_setting(1);
	} else if (low_battery_level == 2) {
		/* lbat_min_en_setting(0); */
		lbat_max_en_setting(1);
	} else {
		lbat_min_en_setting(1);
		/* lbat_max_en_setting(0); */
	}
}

void low_battery_protect_disable(void)
{
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
}

void low_battery_protect_init(struct device *device)
{
	upmu_set_rg_lbat_debt_min(0);
	upmu_set_rg_lbat_debt_max(0);
	upmu_set_rg_lbat_det_prd_15_0(1);
	upmu_set_rg_lbat_det_prd_19_16(0);

	/* init level 1 lbat irq */
	upmu_set_rg_lbat_volt_max(HV_THD);
	upmu_set_rg_lbat_volt_min(LV_1_THD);
	lbat_min_en_setting(1);
	lbat_max_en_setting(0);

	device_create_file(device, &dev_attr_low_battery_protect_ut);
	device_create_file(device, &dev_attr_low_battery_protect_stop);
	device_create_file(device, &dev_attr_low_battery_protect_level);

	pr_debug("[Power/PMIC][low_battery_protect_init] Done\n");
}

