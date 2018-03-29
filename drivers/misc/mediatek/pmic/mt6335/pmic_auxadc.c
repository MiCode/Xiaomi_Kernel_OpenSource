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

#include "include/pmic.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_auxadc_intf.h>

static int count_time_out = 100;

static struct wake_lock  mt6335_auxadc_wake_lock;
static struct mutex mt6335_adc_mutex;

void mt6335_auxadc_lock(void)
{
	wake_lock(&mt6335_auxadc_wake_lock);
	mutex_lock(&mt6335_adc_mutex);
}

void mt6335_auxadc_unlock(void)
{
	mutex_unlock(&mt6335_adc_mutex);
	wake_unlock(&mt6335_auxadc_wake_lock);
}

struct pmic_auxadc_channel mt6335_auxadc_channel[] = {
	{15, 3, PMIC_AUXADC_RQST_CH0, /* BATADC */
		PMIC_AUXADC_ADC_RDY_CH0_BY_AP, PMIC_AUXADC_ADC_OUT_CH0_BY_AP},
	{12, 1, PMIC_AUXADC_RQST_CH2, /* VCDT */
		PMIC_AUXADC_ADC_RDY_CH2, PMIC_AUXADC_ADC_OUT_CH2},
	{12, 2, PMIC_AUXADC_RQST_CH3, /* BAT TEMP */
		PMIC_AUXADC_ADC_RDY_CH3, PMIC_AUXADC_ADC_OUT_CH3},
	{12, 2, PMIC_AUXADC_RQST_BATID, /* BATID */
		PMIC_AUXADC_ADC_RDY_BATID, PMIC_AUXADC_ADC_OUT_BATID},
	{12, 1, PMIC_AUXADC_RQST_CH11, /* VBIF */
		PMIC_AUXADC_ADC_RDY_CH11, PMIC_AUXADC_ADC_OUT_CH11},
	{12, 1, PMIC_AUXADC_RQST_CH4, /* CHIP TEMP */
		PMIC_AUXADC_ADC_RDY_CH4, PMIC_AUXADC_ADC_OUT_CH4},
	{12, 1, PMIC_AUXADC_RQST_CH4, /* DCXO */
		PMIC_AUXADC_ADC_RDY_CH4, PMIC_AUXADC_ADC_OUT_CH4},
	{15, 1, PMIC_AUXADC_RQST_CH7, /* TSX */
		PMIC_AUXADC_ADC_RDY_CH7_BY_AP, PMIC_AUXADC_ADC_OUT_CH7_BY_AP},
};
#define MT6335_AUXADC_CHANNEL_MAX	ARRAY_SIZE(mt6335_auxadc_channel)

int mt6335_get_auxadc_value(u8 channel)
{
	int count = 0;
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel *auxadc_channel;

	if (channel - AUXADC_LIST_MT6335_START < 0 ||
			channel - AUXADC_LIST_MT6335_END > 0) {
		pr_err("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}
	auxadc_channel =
		&mt6335_auxadc_channel[channel-AUXADC_LIST_MT6335_START];

	mt6335_auxadc_lock();

	if (channel == AUXADC_LIST_DCXO)
		pmic_set_register_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 1);
	if (channel == AUXADC_LIST_MT6335_CHIP_TEMP)
		pmic_set_register_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 0);

	pmic_set_register_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (pmic_get_register_value(auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_err("[%s] (%d) Time out!\n", __func__, channel);
			break;
		}
	}
	reg_val = pmic_get_register_value(auxadc_channel->channel_out);

	mt6335_auxadc_unlock();

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

void mt6335_auxadc_init(void)
{
	pr_err("%s\n", __func__);
	wake_lock_init(&mt6335_auxadc_wake_lock,
			WAKE_LOCK_SUSPEND, "MT6335 AuxADC wakelock");
	mutex_init(&mt6335_adc_mutex);

	/* set channel 0, 7 as 15 bits, others = 12 bits  000001000001*/
	pmic_set_register_value(PMIC_RG_STRUP_AUXADC_RSTB_SEL, 1);
	pmic_set_register_value(PMIC_RG_STRUP_AUXADC_RSTB_SW, 1);

	/* 4/11, Ricky, Remove initial setting due to MT6353 issue */
	/* pmic_set_register_value(PMIC_RG_STRUP_AUXADC_START_SEL, 1); */

	pmic_set_register_value(PMIC_AUXADC_MDBG_DET_EN, 0);
	pmic_set_register_value(PMIC_AUXADC_MDBG_DET_PRD, 0x40);
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_EN, 1);
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_PRD, 0x40);
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN, 1);
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_SRCLKEN_IND, 0);
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_START_SEL, 1);
	pmic_set_register_value(PMIC_AUXADC_CK_AON, 0);
	pmic_set_register_value(PMIC_AUXADC_DATA_REUSE_SEL, 0);
	pmic_set_register_value(PMIC_AUXADC_DATA_REUSE_EN, 1);
	pmic_set_register_value(PMIC_AUXADC_TRIM_CH0_SEL, 0);

	pr_info("****[%s] DONE\n", __func__);
}
EXPORT_SYMBOL(mt6335_auxadc_init);

#define MT6335_AUXADC_DEBUG(_reg)                                       \
{                                                                       \
	value = pmic_get_register_value(_reg);				\
	snprintf(buf+strlen(buf), 1024, "%s = 0x%x\n", #_reg, value);	\
	pr_err("[%s] %s = 0x%x\n", __func__, #_reg,			\
		pmic_get_register_value(_reg));			\
}

void mt6335_auxadc_dump_regs(char *buf)
{
	int value;

	snprintf(buf+strlen(buf), 1024, "====| %s |====\n", __func__);
	MT6335_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SEL);
	MT6335_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SW);
	MT6335_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_START_SEL);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_EN);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_PRD);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_WKUP_EN);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_SRCLKEN_IND);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_CK_AON);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_SEL);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_EN);
	MT6335_AUXADC_DEBUG(PMIC_AUXADC_TRIM_CH0_SEL);
}
EXPORT_SYMBOL(mt6335_auxadc_dump_regs);
