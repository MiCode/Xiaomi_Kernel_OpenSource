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

#include "mt6336.h"
#include <mt-plat/mtk_auxadc_intf.h>

#define VOLTAGE_FULL_RANGE	(1800)

static int count_time_out = 100;

static struct wake_lock  mt6336_auxadc_wake_lock;
static struct mutex mt6336_adc_mutex;

struct pmic_auxadc_channel mt6336_auxadc_channel[] = {
	{15, 3, MT6336_AUXADC_RQST_CH0, MT6336_AUXADC_ADC_RDY_CH0,
			MT6336_AUXADC_ADC_OUT_CH0_L},
	{12, 10, MT6336_AUXADC_RQST_CH2, MT6336_AUXADC_ADC_RDY_CH2,
			MT6336_AUXADC_ADC_OUT_CH2_L},
	{12, 2, MT6336_AUXADC_RQST_CH3, MT6336_AUXADC_ADC_RDY_CH3,
			MT6336_AUXADC_ADC_OUT_CH3_L},
	{12, 1, MT6336_AUXADC_RQST_CH4, MT6336_AUXADC_ADC_RDY_CH4,
			MT6336_AUXADC_ADC_OUT_CH4_L},
	{12, 2, MT6336_AUXADC_RQST_CH6, MT6336_AUXADC_ADC_RDY_CH6,
			MT6336_AUXADC_ADC_OUT_CH6_L},
	{12, 1, MT6336_AUXADC_RQST_CH9, MT6336_AUXADC_ADC_RDY_CH9,
			MT6336_AUXADC_ADC_OUT_CH9_L},
	{12, 3, MT6336_AUXADC_RQST_CH10, MT6336_AUXADC_ADC_RDY_CH10,
			MT6336_AUXADC_ADC_OUT_CH10_L},
	{12, 3, MT6336_AUXADC_RQST_CH11, MT6336_AUXADC_ADC_RDY_CH11,
			MT6336_AUXADC_ADC_OUT_CH11_L},
};

/* get AUXADC out data L&H byte */
int mt6336_get_auxadc_out(MT6336_PMU_FLAGS_LIST_ENUM l_byte)
{
	unsigned char l_data, h_data;

	l_data = mt6336_get_flag_register_value(l_byte);
	pr_info("%s l_data = 0x%02x\n", __func__, l_data);
	h_data = mt6336_get_flag_register_value(1+l_byte); /* flag + 1 */
	pr_info("%s h_data = 0x%02x\n", __func__, h_data);

	return l_data|(h_data << 8);
}

void mt6336_auxadc_lock(void)
{
	wake_lock(&mt6336_auxadc_wake_lock);
	mutex_lock(&mt6336_adc_mutex);
}

void mt6336_auxadc_unlock(void)
{
	wake_unlock(&mt6336_auxadc_wake_lock);
	mutex_unlock(&mt6336_adc_mutex);
}

int mt6336_get_auxadc_value_bysw(u8 channel)
{
	int count = 0;
	int adc_result = 0, reg_val = 0;
	int channel_rdy = -1, channel_out = -1, resolution = -1, rqst = -1;
	unsigned char r_val = 0;

	rqst = mt6336_auxadc_channel[channel].channel_rqst;
	channel_rdy = mt6336_auxadc_channel[channel].channel_rdy;
	channel_out = mt6336_auxadc_channel[channel].channel_out;
	resolution = mt6336_auxadc_channel[channel].resolution;
	r_val = mt6336_auxadc_channel[channel].r_val;
	if (rqst < 0) {
		pr_err("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}

	wake_lock(&mt6336_auxadc_wake_lock);
	mutex_lock(&mt6336_adc_mutex);

	mt6336_set_flag_register_value(MT6336_AUXADC_SWCTRL_EN, 1);
	mt6336_set_flag_register_value(MT6336_AUXADC_CHSEL, channel);
	mt6336_config_interface(MT6336_AUXADC_RQST0_SET, 1 << channel, 0xFF, 0);
	udelay(10);

	while (mt6336_get_flag_register_value(channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_err("[%s] chl(%d) Time out\n", __func__, channel);
			mutex_unlock(&mt6336_adc_mutex);
			wake_unlock(&mt6336_auxadc_wake_lock);
			return -EINVAL;
		}
	}

	reg_val = mt6336_get_auxadc_out(channel_out);
	pr_info("%s auxadc out = %d\n", __func__, reg_val);

	mutex_unlock(&mt6336_adc_mutex);
	wake_unlock(&mt6336_auxadc_wake_lock);

	if (resolution == 12)
		adc_result = (reg_val*VOLTAGE_FULL_RANGE)*r_val/4096;/*12 bits*/
	else if (resolution == 15)
		adc_result = (reg_val*VOLTAGE_FULL_RANGE)*r_val/32768; /*15*/

	pr_info("[%s] reg_val = 0x%x, adc_result = %d\n",
				__func__, reg_val, adc_result);
	return adc_result;
}

int mt6336_get_auxadc_value(u8 channel)
{
	int count = 0;
	int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel *auxadc_channel;

	if (channel - AUXADC_LIST_MT6336_START < 0 ||
		channel - AUXADC_LIST_MT6336_END > 0) {
		pr_err("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}

	auxadc_channel =
		&mt6336_auxadc_channel[channel-AUXADC_LIST_MT6337_START];

	mt6336_auxadc_lock();

	/* set AUXADC Request */
	mt6336_set_flag_register_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (mt6336_get_flag_register_value(
				auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_err("[%s] chl(%d) Time out\n", __func__, channel);
			break;
		}
	}

	reg_val = mt6336_get_auxadc_out(auxadc_channel->channel_out);
	pr_info("%s auxadc out = %d\n", __func__, reg_val);

	mt6336_auxadc_unlock();

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
EXPORT_SYMBOL(mt6336_get_auxadc_value);

#define MT6336_AUXADC_DEBUG(_reg)					\
{									\
	value = mt6336_get_flag_register_value(MT6336_##_reg);		\
	snprintf(buf+strlen(buf), 1024, "%s = 0x%02x\n", #_reg, value);	\
	pr_err("[%s] %s = 0x%02x\n", __func__, #_reg, value);	\
}

void mt6336_auxadc_dump_regs(char *buf)
{
	int value;

	value = 0;
	snprintf(buf+strlen(buf), 1024, "====| %s |====\n", __func__);
}
EXPORT_SYMBOL(mt6336_auxadc_dump_regs);

void mt6336_auxadc_init(void)
{
	pr_err("%s\n", __func__);
	wake_lock_init(&mt6336_auxadc_wake_lock,
			WAKE_LOCK_SUSPEND, "MT6336 AuxADC wakelock");
	mutex_init(&mt6336_adc_mutex);
	/*mt6336_set_flag_register_value(MT6336_AUXADC_CK_AON, 0);*/

	pr_info("****[%s] DONE\n", __func__);
}
EXPORT_SYMBOL(mt6336_auxadc_init);
