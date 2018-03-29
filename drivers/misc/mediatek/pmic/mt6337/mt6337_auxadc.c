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
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "mt6337.h"
#include "mt6337_upmu_hw.h"
#include <mt-plat/mtk_auxadc_intf.h>

static int count_time_out = 100;

static struct wake_lock  mt6337_auxadc_wake_lock;
static struct mutex mt6337_adc_mutex;

static void mt6337_auxadc_lock(void)
{
	wake_lock(&mt6337_auxadc_wake_lock);
	mutex_lock(&mt6337_adc_mutex);
}

static void mt6337_auxadc_unlock(void)
{
	mutex_unlock(&mt6337_adc_mutex);
	wake_unlock(&mt6337_auxadc_wake_lock);
}

struct pmic_auxadc_channel mt6337_auxadc_channel[] = {
	{15, 3, MT6337_PMIC_AUXADC_RQST_CH0, /* BATSNS */
		MT6337_PMIC_AUXADC_ADC_RDY_CH0, MT6337_PMIC_AUXADC_ADC_OUT_CH0},
	{12, 1, MT6337_PMIC_AUXADC_RQST_CH4, /* PMIC TEMP */
		MT6337_PMIC_AUXADC_ADC_RDY_CH4, MT6337_PMIC_AUXADC_ADC_OUT_CH4},
	{12, 1, MT6337_PMIC_AUXADC_RQST_CH5, /* ACCDET */
		MT6337_PMIC_AUXADC_ADC_RDY_CH5, MT6337_PMIC_AUXADC_ADC_OUT_CH5},
	{15, 1, MT6337_PMIC_AUXADC_RQST_CH9, /* HP Offset Cal */
		MT6337_PMIC_AUXADC_ADC_RDY_CH9, MT6337_PMIC_AUXADC_ADC_OUT_CH9},
};
#define MT6337_AUXADC_CHANNEL_MAX	ARRAY_SIZE(mt6337_auxadc_channel)

int mt6337_get_auxadc_value(u8 channel)
{
	int count = 0;
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel *auxadc_channel;

	if (channel - AUXADC_LIST_MT6337_START < 0 ||
		channel - AUXADC_LIST_MT6337_END > 0) {
		pr_err("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}
	auxadc_channel =
		&mt6337_auxadc_channel[channel-AUXADC_LIST_MT6337_START];

	mt6337_auxadc_lock();

	mt6337_set_register_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (mt6337_get_register_value(auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_err("[%s] (%d) Time out!\n", __func__, channel);
			break;
		}
	}
	reg_val = mt6337_get_register_value(auxadc_channel->channel_out);

	mt6337_auxadc_unlock();

	/* Audio request HPOPS to return raw data */
	if (channel == AUXADC_LIST_HPOFS_CAL)
		return reg_val * auxadc_channel->r_val;

	if (auxadc_channel->resolution == 12)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 4096;
	else if (auxadc_channel->resolution == 15)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 32768;

	pr_info("[%s] reg_val = 0x%x, adc_result = %d\n",
				__func__, reg_val, adc_result);
	return adc_result;
}

void mt6337_auxadc_init(void)
{
	pr_err("%s\n", __func__);
	wake_lock_init(&mt6337_auxadc_wake_lock,
			WAKE_LOCK_SUSPEND, "MT6337 AuxADC wakelock");
	mutex_init(&mt6337_adc_mutex);

	pr_info("****[%s] DONE\n", __func__);
}
EXPORT_SYMBOL(mt6337_auxadc_init);

#define MT6337_AUXADC_DEBUG(_reg)                                       \
{                                                                       \
	value = mt6337_get_register_value(_reg);			\
	snprintf(buf+strlen(buf), 1024, "%s = 0x%x\n", #_reg, value);	\
	pr_err("[%s] %s = 0x%x\n", __func__, #_reg, value);		\
}

void mt6337_auxadc_dump_regs(char *buf)
{
	int value;

	value = 0;
	snprintf(buf+strlen(buf), 1024, "====| %s |====\n", __func__);
}
EXPORT_SYMBOL(mt6337_auxadc_dump_regs);
