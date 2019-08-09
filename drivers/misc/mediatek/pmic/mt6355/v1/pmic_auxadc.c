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
#include <linux/device.h>
#include <linux/pm_wakeup.h>
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
#include <linux/ratelimit.h>

#include "include/pmic.h"
#include "include/mtk_pmic_common.h"
#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_auxadc_intf.h>

static int count_time_out = 100;

static struct wakeup_source  mt6355_auxadc_wake_lock;
static struct mutex mt6355_adc_mutex;
static DEFINE_MUTEX(auxadc_ch3_mutex);

/*--Monitor MTS Thread Start--*/
static int mts_adc;
static struct wakeup_source  adc_monitor_wake_lock;
static struct mutex adc_monitor_mutex;
static struct task_struct *adc_thread_handle;

const char *pmic_auxadc_channel_name[] = {
	/* mt6355 */
	"BATADC",
	"VCDT",
	"BAT TEMP",
	"BATID",
	"VBIF",
	"MT6355 CHIP TEMP",
	"DCXO",
	"ACCDET",
	"TSX",
	"HP",
};

void wake_up_auxadc_detect(void)
{
	PMICLOG("[%s]\n", __func__);
	if (adc_thread_handle != NULL) {
		__pm_stay_awake(&adc_monitor_wake_lock);
		wake_up_process(adc_thread_handle);
	} else
		pr_info(PMICTAG "[%s] adc_thread_handle not ready\n"
			, __func__);
}
/*--Monitor MTS Thread End--*/

unsigned int wk_auxadc_ch3_bif_on(unsigned char en)
{
	if (en < 2) {
		pmic_set_register_value(PMIC_RG_LDO_VBIF28_EN, en);
		pmic_set_register_value(PMIC_RG_LDO_VBIF28_SW_OP_EN, 1);
		if (!en)
			pmic_set_register_value(PMIC_RG_LDO_VBIF28_SW_OP_EN, 0);
	}

	return pmic_get_register_value(PMIC_DA_QI_VBIF28_EN);
}

unsigned int wk_auxadc_vsen_tdet_ctrl(unsigned char en_check)
{
	unsigned int ret = 0;

	if (en_check) {
		if ((!pmic_get_register_value(PMIC_RG_ADCIN_VSEN_MUX_EN)) &&
			(pmic_get_register_value(PMIC_BATON_TDET_EN))) {
			PMICLOG("[%s] vbif %d\n"
				, __func__, g_pmic_pad_vbif28_vol);
			return g_pmic_pad_vbif28_vol;
		}
		pr_info("[%s] baton switch off! vsen_mux_en = %d, tdet_en = %d\n"
			, __func__
			, pmic_get_register_value(PMIC_RG_ADCIN_VSEN_MUX_EN)
			, pmic_get_register_value(PMIC_BATON_TDET_EN));
		ret = 0;
	} else {
		pmic_set_register_value(PMIC_RG_ADCIN_VSEN_MUX_EN, 0);
		pmic_set_register_value(PMIC_BATON_TDET_EN, 1);
		pr_info("[%s] baton switch on! vsen_mux_en = %d, tdet_en = %d\n"
			, __func__
			, pmic_get_register_value(PMIC_RG_ADCIN_VSEN_MUX_EN)
			, pmic_get_register_value(PMIC_BATON_TDET_EN));
		ret = 1;
	}
	return ret;
}

void wk_auxadc_bgd_ctrl(unsigned char en)
{
	if (en) {
		/*--Enable BATON TDET EN--*/
		pmic_config_interface_nolock(MT6355_PCHR_VREF_ANA_DA0
					     , 0x1, 0x1, 2);
		/*--BAT TEMP MAX DET EN--*/
		pmic_config_interface_nolock(MT6355_AUXADC_BAT_TEMP_4
					     , 0x3, 0x3, 12);
		/*--BAT TEMP MIN DET EN--*/
		pmic_config_interface_nolock(MT6355_AUXADC_BAT_TEMP_5
					     , 0x3, 0x3, 12);
		/*--BAT TEMP DET EN--*/
		pmic_config_interface_nolock(MT6355_INT_CON1_SET
					     , 0x00C0, 0xffff, 0);
	} else {
		/*--BAT TEMP MAX DET EN--*/
		pmic_config_interface_nolock(MT6355_AUXADC_BAT_TEMP_4
					     , 0x0, 0x3, 12);
		/*--BAT TEMP MIN DET EN--*/
		pmic_config_interface_nolock(MT6355_AUXADC_BAT_TEMP_5
					     , 0x0, 0x3, 12);
		/*--BAT TEMP DET EN--*/
		pmic_config_interface_nolock(MT6355_INT_CON1_CLR
					     , 0x00C0, 0xffff, 0);
		/*--Disable BATON TDET EN--*/
		pmic_config_interface_nolock(MT6355_PCHR_VREF_ANA_DA0
					     , 0x0, 0x1, 2);
	}
}

void wk_auxadc_bgd_ctrl_dbg(void)
{
	pr_info("EN_BAT_TEMP_L: %d\n"
		, pmic_get_register_value(PMIC_RG_INT_EN_BAT_TEMP_L));
	pr_info("EN_BAT_TEMP_H: %d\n"
		, pmic_get_register_value(PMIC_RG_INT_EN_BAT_TEMP_H));
	pr_info("BAT_TEMP_IRQ_EN_MAX: %d\n"
		, pmic_get_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MAX));
	pr_info("BAT_TEMP_IRQ_EN_MIN: %d\n"
		, pmic_get_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MIN));
	pr_info("BAT_TEMP_EN_MAX: %d\n"
		, pmic_get_register_value(PMIC_AUXADC_BAT_TEMP_EN_MAX));
	pr_info("BAT_TEMP_EN_MIN: %d\n"
		, pmic_get_register_value(PMIC_AUXADC_BAT_TEMP_EN_MIN));
	pr_info("BATON_TDET_EN: %d\n"
		, pmic_get_register_value(PMIC_BATON_TDET_EN));
}

void mt6355_auxadc_lock(void)
{
	__pm_stay_awake(&mt6355_auxadc_wake_lock);
	mutex_lock(&mt6355_adc_mutex);
}

void lockadcch3(void)
{
	mutex_lock(&auxadc_ch3_mutex);
}

void unlockadcch3(void)
{
	mutex_unlock(&auxadc_ch3_mutex);
}


void mt6355_auxadc_unlock(void)
{
	mutex_unlock(&mt6355_adc_mutex);
	__pm_relax(&mt6355_auxadc_wake_lock);
}

struct pmic_auxadc_channel mt6355_auxadc_channel[] = {
	{15, 3, PMIC_AUXADC_RQST_CH0, /* BATADC */
		PMIC_AUXADC_ADC_RDY_CH0_BY_AP, PMIC_AUXADC_ADC_OUT_CH0_BY_AP},
	{12, 1, PMIC_AUXADC_RQST_CH2, /* VCDT */
		PMIC_AUXADC_ADC_RDY_CH2, PMIC_AUXADC_ADC_OUT_CH2},
	{12, 2, PMIC_AUXADC_RQST_CH3, /* BAT TEMP */
		PMIC_AUXADC_ADC_RDY_CH3, PMIC_AUXADC_ADC_OUT_CH3},
	{12, 2, PMIC_AUXADC_RQST_BATID, /* BATID */
		PMIC_AUXADC_ADC_RDY_BATID, PMIC_AUXADC_ADC_OUT_BATID},
	{12, 2, PMIC_AUXADC_RQST_CH11, /* VBIF */
		PMIC_AUXADC_ADC_RDY_CH11, PMIC_AUXADC_ADC_OUT_CH11},
	{12, 1, PMIC_AUXADC_RQST_CH4, /* CHIP TEMP */
		PMIC_AUXADC_ADC_RDY_CH4, PMIC_AUXADC_ADC_OUT_CH4},
	{12, 1, PMIC_AUXADC_RQST_CH4, /* DCXO */
		PMIC_AUXADC_ADC_RDY_CH4, PMIC_AUXADC_ADC_OUT_CH4},
	{12, 1, PMIC_AUXADC_RQST_CH5, /* ACCDET MULTI-KEY */
		PMIC_AUXADC_ADC_RDY_CH5, PMIC_AUXADC_ADC_OUT_CH5},
	{15, 1, PMIC_AUXADC_RQST_CH7, /* TSX */
		PMIC_AUXADC_ADC_RDY_CH7_BY_AP, PMIC_AUXADC_ADC_OUT_CH7_BY_AP},
	{15, 1, PMIC_AUXADC_RQST_CH9, /* HP OFFSET CAL */
		PMIC_AUXADC_ADC_RDY_CH9, PMIC_AUXADC_ADC_OUT_CH9},
};
#define MT6355_AUXADC_CHANNEL_MAX	ARRAY_SIZE(mt6355_auxadc_channel)

int pmic_get_auxadc_channel_max(void)
{
	return MT6355_AUXADC_CHANNEL_MAX;
}

int mt_get_auxadc_value(u8 channel)
{
	int count = 0;
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel *auxadc_channel;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	if (channel - AUXADC_LIST_START < 0 ||
			channel - AUXADC_LIST_END > 0) {
		pr_info("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}
	auxadc_channel =
		&mt6355_auxadc_channel[channel-AUXADC_LIST_START];

	if (channel == AUXADC_LIST_VBIF) {
		if (wk_auxadc_vsen_tdet_ctrl(1))
			return g_pmic_pad_vbif28_vol;
	}

	mt6355_auxadc_lock();

	if (channel == AUXADC_LIST_DCXO)
		pmic_set_register_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 1);
	if (channel == AUXADC_LIST_MT6355_CHIP_TEMP)
		pmic_set_register_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 0);
	if (channel == AUXADC_LIST_BATTEMP ||
	    channel == AUXADC_LIST_BATID ||
	    channel == AUXADC_LIST_VBIF) {
		if (!wk_auxadc_ch3_bif_on(2)) {
			pr_info("ch3 bif off abnormal\n");
			wk_auxadc_ch3_bif_on(1);
		}
		if (channel == AUXADC_LIST_BATTEMP) {
			if (!wk_auxadc_vsen_tdet_ctrl(1)) {
				pr_info("ch3 tdet ctrl abnormal\n");
				wk_auxadc_vsen_tdet_ctrl(0);
			}
			mutex_lock(&auxadc_ch3_mutex);
		}
	}

	pmic_set_register_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (pmic_get_register_value(auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_info("[%s] (%d) Time out!\n", __func__, channel);
			break;
		}
	}
	reg_val = pmic_get_register_value(auxadc_channel->channel_out);

	if (channel == AUXADC_LIST_BATTEMP ||
	    channel == AUXADC_LIST_BATID ||
	    channel == AUXADC_LIST_VBIF) {
		if (channel == AUXADC_LIST_BATTEMP)
			mutex_unlock(&auxadc_ch3_mutex);
	}

	mt6355_auxadc_unlock();

	if (channel == AUXADC_LIST_VBIF)
		wk_auxadc_vsen_tdet_ctrl(0);

	if (auxadc_channel->resolution == 12)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 4096;
	else if (auxadc_channel->resolution == 15)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 32768;

	if (channel == AUXADC_LIST_BATTEMP) {
		if (adc_result > 2000 || adc_result < 300) {
			pr_info("[bif] %d\n", wk_auxadc_ch3_bif_on(2));
			pr_info("[baton] vsen_mux_en = %d, tdet_en = %d\n",
				pmic_get_register_value(
						PMIC_RG_ADCIN_VSEN_MUX_EN),
				pmic_get_register_value(PMIC_BATON_TDET_EN));
			if (adc_result < 200 &&
			    wk_auxadc_ch3_bif_on(2) &&
			    wk_auxadc_vsen_tdet_ctrl(1))
				adc_result = mt6355_auxadc_recv_batmp();
		}
	}

	if (__ratelimit(&ratelimit))
		pr_info("[%s] ch = %d, reg_val = 0x%x, adc_result = %d\n",
					__func__, channel, reg_val, adc_result);

	/*--Monitor MTS Thread--*/
	if (channel == AUXADC_LIST_BATADC)
		wake_up_auxadc_detect();

	/* Audio request HPOPS to return raw data */
	if (channel == AUXADC_LIST_HPOFS_CAL)
		return reg_val * auxadc_channel->r_val;
	else
		return adc_result;
}

static int mt6355_auxadc_get_auxadc_value_batmp(void)
{
	int count = 0;
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel *auxadc_channel;
	u8 channel = AUXADC_LIST_BATTEMP;

	auxadc_channel =
		&mt6355_auxadc_channel[channel-AUXADC_LIST_START];

	mt6355_auxadc_lock();
	mutex_lock(&auxadc_ch3_mutex);

	pmic_set_register_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (pmic_get_register_value(auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			pr_info("[%s] (%d) Time out!\n", __func__, channel);
			break;
		}
	}
	reg_val = pmic_get_register_value(auxadc_channel->channel_out);

	mutex_unlock(&auxadc_ch3_mutex);
	mt6355_auxadc_unlock();

	adc_result = (reg_val * auxadc_channel->r_val *
				VOLTAGE_FULL_RANGE) / 4096;

	pr_info("reg_val = 0x%x, adc_result = %d\n", reg_val, adc_result);

	return adc_result;
}

int mt6355_auxadc_recv_batmp(void)
{
	int count = 0;
	signed int adc_result = 0, all_adc_result = 0;

	for (count = 0; count < 5; count++)
		all_adc_result += mt6355_auxadc_get_auxadc_value_batmp();

	if (all_adc_result < 1000) {
		pr_info("adc_recv_batmp\n");
		pmic_set_register_value(PMIC_RG_STRUP_AUXADC_RSTB_SW, 0);
		pmic_set_register_value(PMIC_RG_STRUP_AUXADC_RSTB_SW, 1);
		adc_result = mt6355_auxadc_get_auxadc_value_batmp();
		pr_info("adc_recv_batmp %d\n", adc_result);
		return adc_result;
	} else
		return (all_adc_result/5);
}

static unsigned int mts_count;
/*--Monitor MTS Thread Start--*/
void mt6355_auxadc_monitor_mts_regs(void)
{
	int mts_adc_tmp = 0;

	mts_adc_tmp = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	pr_debug("[MTS_ADC] OLD = 0x%x, NOW = 0x%x, CNT = %d\n"
		 , mts_adc, mts_adc_tmp, mts_count);

	if (mts_adc ==  mts_adc_tmp)
		mts_count++;
	else
		mts_count = 0;

	if ((mts_count > 15)) {
		pr_info("DEW_READ_TEST = 0x%x\n"
			, pmic_get_register_value(PMIC_DEW_READ_TEST));
		/*--AUXADC MDRT--*/
		pr_info("AUXADC_ADC36  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_ADC36));
		pr_info("AUXADC_ADC42  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_ADC42));
		pr_info("AUXADC_MDRT_0 = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_MDRT_0));
		pr_info("AUXADC_MDRT_1 = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_MDRT_1));
		pr_info("AUXADC_MDRT_2 = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_MDRT_2));
		pr_info("AUXADC_MDRT_3 = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_MDRT_3));
		pr_info("AUXADC_MDRT_4 = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_MDRT_4));
		/*--AUXADC SPI AVG SEL--*/
		pr_info("AUXADC_CON2  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_CON2));
		pr_info("AUXADC_CON5  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_CON5));
		pr_info("AUXADC_CON8  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_CON8));
		pr_info("AUXADC_CON9  = 0x%x\n"
			, upmu_get_reg_value(MT6355_AUXADC_CON9));
		/*--AUXADC CLK--*/
		pr_info("TOP_CKPDN_CON0  = 0x%x\n"
			, upmu_get_reg_value(MT6355_TOP_CKPDN_CON0));
		pr_info("TOP_CKHWEN_CON0 = 0x%x\n"
			, upmu_get_reg_value(MT6355_TOP_CKHWEN_CON0));
		pr_info("TOP_CKHWEN_CON1 = 0x%x\n"
			, upmu_get_reg_value(MT6355_TOP_CKHWEN_CON1));

		/*--AUXADC CH7--*/
		pr_info("AUXADC_LIST_TSX = %d\n"
			, mt_get_auxadc_value(AUXADC_LIST_TSX));
		mts_count = 0;
	}
	mts_adc = mts_adc_tmp;
}

int mt6355_auxadc_kthread(void *x)
{
	set_current_state(TASK_INTERRUPTIBLE);

	PMICLOG("mt6355 auxadc thread enter\n");

	/* Run on a process content */
	while (1) {
		mutex_lock(&adc_monitor_mutex);

		mt6355_auxadc_monitor_mts_regs();

		mutex_unlock(&adc_monitor_mutex);
		__pm_relax(&adc_monitor_wake_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}

void mt6355_auxadc_thread_init(void)
{
	adc_thread_handle = kthread_create(mt6355_auxadc_kthread
					   , (void *)NULL, "adc_thread");
	if (IS_ERR(adc_thread_handle)) {
		adc_thread_handle = NULL;
		pr_info(PMICTAG "[adc_kthread] creation fails\n");
	} else {
		PMICLOG("[adc_kthread] kthread_create Done\n");
	}
}
/*--Monitor MTS Thread End--*/

void pmic_auxadc_init(void)
{
	pr_info("%s\n", __func__);
	wakeup_source_init(&mt6355_auxadc_wake_lock
			   , "MT6355 AuxADC wakelock");
	wakeup_source_init(&adc_monitor_wake_lock
			   , "MT6355 AuxADC Monitor wakelock");
	mutex_init(&mt6355_adc_mutex);
	mutex_init(&adc_monitor_mutex);

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
	/*IMP DCM WK Peter-SW 12/21--*/
	pmic_config_interface(0x3370, 0x1, 0x1, 0);

	mt6355_auxadc_thread_init();

	pr_info("****[%s] DONE\n", __func__);

	/* update VBIF28 by AUXADC */
	g_pmic_pad_vbif28_vol = mt_get_auxadc_value(AUXADC_LIST_VBIF);
	/* update TSX by AUXADC */
	mts_adc = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	pr_info("****[%s] VBIF28 = %d, MTS_ADC = 0x%x\n"
		, __func__, pmic_get_vbif28_volt(), mts_adc);
}
EXPORT_SYMBOL(pmic_auxadc_init);

#define MT6355_AUXADC_DEBUG(_reg)                                       \
{                                                                       \
	value = pmic_get_register_value(_reg);				\
	snprintf(buf+strlen(buf), 1024, "%s = 0x%x\n", #_reg, value);	\
	pr_info("[%s] %s = 0x%x\n", __func__, #_reg,			\
		pmic_get_register_value(_reg));			\
}

void mt6355_auxadc_dump_setting_regs(void)
{
	pr_info("Dump Basic RG\n");
	pr_info("[0x%x] 0x%x\n"
		, MT6355_STRUP_CON6
		, upmu_get_reg_value(MT6355_STRUP_CON6));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_MDRT_0
		, upmu_get_reg_value(MT6355_AUXADC_MDRT_0));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_MDRT_2
		, upmu_get_reg_value(MT6355_AUXADC_MDRT_2));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_CON0
		, upmu_get_reg_value(MT6355_AUXADC_CON0));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_CON23
		, upmu_get_reg_value(MT6355_AUXADC_CON23));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_CON11
		, upmu_get_reg_value(MT6355_AUXADC_CON11));
	pr_info("Done\n");
}
EXPORT_SYMBOL(mt6355_auxadc_dump_setting_regs);

void mt6355_auxadc_dump_clk_regs(void)
{
	pr_info("Dump Clk RG\n");
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKPDN_CON0
		, upmu_get_reg_value(MT6355_TOP_CKPDN_CON0));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKPDN_CON3
		, upmu_get_reg_value(MT6355_TOP_CKPDN_CON3));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKHWEN_CON0
		, upmu_get_reg_value(MT6355_TOP_CKHWEN_CON0));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKHWEN_CON1
		, upmu_get_reg_value(MT6355_TOP_CKHWEN_CON1));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKTST_CON1
		, upmu_get_reg_value(MT6355_TOP_CKTST_CON1));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_TOP_CKDIVSEL_CON0
		, upmu_get_reg_value(MT6355_TOP_CKDIVSEL_CON0));
	pr_info("Done\n");
}
EXPORT_SYMBOL(mt6355_auxadc_dump_clk_regs);

void mt6355_auxadc_dump_channel_regs(void)
{
/* Enable this only for debug */
#if 0
	u8 list = 0;

	for (list = AUXADC_LIST_START;
	     list <= AUXADC_LIST_END; list++)
		pr_notice("[auxadc list %d] %d\n"
			  , list, mt_get_auxadc_value(list));
#endif
}
EXPORT_SYMBOL(mt6355_auxadc_dump_channel_regs);


void pmic_auxadc_dump_regs(char *buf)
{
	int value;

	snprintf(buf+strlen(buf), 1024, "====| %s |====\n", __func__);
	MT6355_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SEL);
	MT6355_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SW);
	MT6355_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_START_SEL);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_EN);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_PRD);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_WKUP_EN);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_SRCLKEN_IND);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_CK_AON);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_SEL);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_EN);
	MT6355_AUXADC_DEBUG(PMIC_AUXADC_TRIM_CH0_SEL);
}
EXPORT_SYMBOL(pmic_auxadc_dump_regs);
