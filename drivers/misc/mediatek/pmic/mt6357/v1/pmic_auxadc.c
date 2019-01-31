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
#include <linux/timekeeping.h>

#include "include/pmic.h"
#include "include/mtk_pmic_common.h"
#include "include/pmic_auxadc.h"
#include <mt-plat/aee.h>
#include <mt-plat/upmu_common.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_auxadc_intf.h>

#ifdef CONFIG_MTK_PMIC_WRAP_HAL
#include <mach/mtk_pmic_wrap.h>
#endif

#if (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/mtk_battery.h>
#endif

#define AEE_DBG 0

static int count_time_out = 100;
static int battmp;
#if AEE_DBG
static unsigned int aee_count;
#endif
static unsigned int dbg_count;
static unsigned char dbg_flag;

static struct wakeup_source  pmic_auxadc_wake_lock;
static struct mutex pmic_adc_mutex;
static DEFINE_MUTEX(auxadc_ch3_mutex);

#define DBG_REG_SIZE 384
struct pmic_adc_dbg_st {
	int ktime_sec;
	unsigned short reg[DBG_REG_SIZE];
};
static unsigned int adc_dbg_addr[DBG_REG_SIZE];
static struct pmic_adc_dbg_st pmic_adc_dbg[4];
static unsigned char dbg_stamp;

const char *pmic_auxadc_channel_name[] = {
	/* mt6357 */
	"BATADC",
	"VCDT",
	"BAT TEMP",
	"BATID",
	"VBIF",
	"MT6357 CHIP TEMP",
	"DCXO",
	"ACCDET",
	"TSX",
	"HP",
	"ISENSE",
	"BUCK1_TEMP",
	"BUCK2_TEMP",
};

/*
 * The vbif28 variable and function are defined in upmu_regulator
 * to prevent from building error, define weak attribute here
 */
unsigned int __attribute__ ((weak)) g_pmic_pad_vbif28_vol = 1;
unsigned int __attribute__ ((weak)) pmic_get_vbif28_volt(void)
{
	return 0;
}

void wk_auxadc_bgd_ctrl(unsigned char en)
{
/*--MT6357 not support Battmp BGD--*/
}

void pmic_auxadc_suspend(void)
{
	wk_auxadc_bgd_ctrl(0);
	battmp = 0;
}

void pmic_auxadc_resume(void)
{
	wk_auxadc_bgd_ctrl(1);
}

void wk_auxadc_bgd_ctrl_dbg(void)
{
	HKLOG("BATON_TDET_EN: %d\n"
	      , pmic_get_register_value(PMIC_BATON_TDET_EN));
}

int bat_temp_filter(int *arr, unsigned short size)
{
	unsigned char i, i_max, i_min = 0;
	int arr_max = 0, arr_min = arr[0];
	int sum = 0;

	for (i = 0; i < size; i++) {
		sum += arr[i];
		if (arr[i] > arr_max) {
			arr_max = arr[i];
			i_max = i;
		} else if (arr[i] < arr_min) {
			arr_min = arr[i];
			i_min = i;
		}
	}
	sum = sum - arr_max - arr_min;
	return (sum/(size - 2));
}

void wk_auxadc_dbg_dump(void)
{
	unsigned char reg_log[861] = "", reg_str[21] = "";
	unsigned char i;
	unsigned short j;

	for (i = 0; i < 4; i++) {
		if (pmic_adc_dbg[dbg_stamp].ktime_sec == 0) {
			dbg_stamp++;
			if (dbg_stamp >= 4)
				dbg_stamp = 0;
			continue;
		}
		for (j = 0; adc_dbg_addr[j] != 0; j++) {
			if (j % 43 == 0) {
				HKLOG("%d %s\n"
				      , pmic_adc_dbg[dbg_stamp].ktime_sec
				      , reg_log);
				strncpy(reg_log, "", 860);
			}
			snprintf(reg_str, 20, "Reg[0x%x]=0x%x, "
				 , adc_dbg_addr[j]
				 , pmic_adc_dbg[dbg_stamp].reg[j]);
			strncat(reg_log, reg_str, 860);
		}
		pr_notice("[%s] %d %d %s\n"
			  , __func__
			  , dbg_stamp
			  , pmic_adc_dbg[dbg_stamp].ktime_sec
			  , reg_log);
		strncpy(reg_log, "", 860);
		dbg_stamp++;
		if (dbg_stamp >= 4)
			dbg_stamp = 0;
	}
}

int wk_auxadc_battmp_dbg(int bat_temp)
{
	unsigned short i;
	int vbif = 0, bat_temp2 = 0, bat_temp3 = 0, bat_id = 0;
	int arr_bat_temp[5];

	if (dbg_flag)
		HKLOG("Another dbg is running\n");
	dbg_flag = 1;
	for (i = 0; adc_dbg_addr[i] != 0; i++)
		pmic_adc_dbg[dbg_stamp].reg[i] =
			upmu_get_reg_value(adc_dbg_addr[i]);
	pmic_adc_dbg[dbg_stamp].ktime_sec = (int)get_monotonic_coarse().tv_sec;
	dbg_stamp++;
	if (dbg_stamp >= 4)
		dbg_stamp = 0;
	/* Re-read AUXADC */
	bat_temp2 = pmic_get_auxadc_value(AUXADC_LIST_BATTEMP);
	vbif = pmic_get_auxadc_value(AUXADC_LIST_VBIF);
	bat_temp3 = pmic_get_auxadc_value(AUXADC_LIST_BATTEMP);
	bat_id = pmic_get_auxadc_value(AUXADC_LIST_BATID);
	pr_notice("BAT_TEMP1: %d, BAT_TEMP2:%d, VBIF:%d, BAT_TEMP3:%d, BATID:%d\n"
		  , bat_temp, bat_temp2, vbif, bat_temp3, bat_id);

	if (bat_temp < 200 ||
	    (battmp != 0 &&
	     (bat_temp - battmp > 100 || battmp - bat_temp > 100))) {
		wk_auxadc_dbg_dump();
		for (i = 0; i < 5; i++)
			arr_bat_temp[i] =
				pmic_get_auxadc_value(AUXADC_LIST_BATTEMP);
		bat_temp = bat_temp_filter(arr_bat_temp, 5);
	}
	dbg_flag = 0;
	return bat_temp;
}

void pmic_auxadc_lock(void)
{
	__pm_stay_awake(&pmic_auxadc_wake_lock);
	mutex_lock(&pmic_adc_mutex);
}

void lockadcch3(void)
{
	mutex_lock(&auxadc_ch3_mutex);
}

void unlockadcch3(void)
{
	mutex_unlock(&auxadc_ch3_mutex);
}


void pmic_auxadc_unlock(void)
{
	mutex_unlock(&pmic_adc_mutex);
	__pm_relax(&pmic_auxadc_wake_lock);
}

struct pmic_auxadc_channel_new {
	u8 resolution;
	u8 r_val;
	u8 ch_num;
	unsigned int channel_rqst;
	unsigned int channel_rdy;
	unsigned int channel_out;
};

struct pmic_auxadc_channel_new mt6357_auxadc_channel[] = {
	{15, 3, 0, PMIC_AUXADC_RQST_CH0, /* BATADC */
	PMIC_AUXADC_ADC_RDY_CH0_BY_AP, PMIC_AUXADC_ADC_OUT_CH0_BY_AP},
	{12, 1, 2, PMIC_AUXADC_RQST_CH2, /* VCDT */
	PMIC_AUXADC_ADC_RDY_CH2, PMIC_AUXADC_ADC_OUT_CH2},
	{12, 1, 3, PMIC_AUXADC_RQST_CH3, /* BAT TEMP */
	PMIC_AUXADC_ADC_RDY_CH3, PMIC_AUXADC_ADC_OUT_CH3},
	{12, 1, 3, PMIC_AUXADC_RQST_BATID, /* BATID */
	PMIC_AUXADC_ADC_RDY_BATID, PMIC_AUXADC_ADC_OUT_BATID},
	{12, 1, 11, PMIC_AUXADC_RQST_CH11, /* VBIF */
	PMIC_AUXADC_ADC_RDY_CH11, PMIC_AUXADC_ADC_OUT_CH11},
	{12, 1, 4, PMIC_AUXADC_RQST_CH4, /* CHIP TEMP */
	PMIC_AUXADC_ADC_RDY_CH4, PMIC_AUXADC_ADC_OUT_CH4},
	{15, 1, 4, PMIC_AUXADC_RQST_CH4, /* DCXO */
	PMIC_AUXADC_ADC_RDY_DCXO_BY_AP, PMIC_AUXADC_ADC_OUT_DCXO_BY_AP},
	{12, 1, 5, PMIC_AUXADC_RQST_CH5, /* ACCDET MULTI-KEY */
	PMIC_AUXADC_ADC_RDY_CH5, PMIC_AUXADC_ADC_OUT_CH5},
	{15, 1, 7, PMIC_AUXADC_RQST_CH7, /* TSX */
	PMIC_AUXADC_ADC_RDY_CH7_BY_AP, PMIC_AUXADC_ADC_OUT_CH7_BY_AP},
	{15, 1, 9, PMIC_AUXADC_RQST_CH9, /* HP OFFSET CAL */
	PMIC_AUXADC_ADC_RDY_CH9, PMIC_AUXADC_ADC_OUT_CH9},
	{15, 3, 1, PMIC_AUXADC_RQST_CH1, /* ISENSE */
	PMIC_AUXADC_ADC_RDY_CH1_BY_AP, PMIC_AUXADC_ADC_OUT_CH1_BY_AP},
	{12, 1, 4, PMIC_AUXADC_RQST_CH4_BY_THR1, /* TS_BUCK1 */
	PMIC_AUXADC_ADC_RDY_CH4_BY_THR1, PMIC_AUXADC_ADC_OUT_CH4_BY_THR1},
	{12, 1, 4, PMIC_AUXADC_RQST_CH4_BY_THR2, /* TS_BUCK2 */
	PMIC_AUXADC_ADC_RDY_CH4_BY_THR2, PMIC_AUXADC_ADC_OUT_CH4_BY_THR2},
};

#define MT6357_AUXADC_CHANNEL_MAX	ARRAY_SIZE(mt6357_auxadc_channel)

int pmic_get_auxadc_channel_max(void)
{
	return MT6357_AUXADC_CHANNEL_MAX;
}

bool is_isense_supported(void)
{
	/* PMIC MT6357 supports ISENSE */
	return true;
}

static int mts_timestamp;
static unsigned int mts_count;
static unsigned int mts_adc;
static struct wakeup_source mts_monitor_wake_lock;
static struct mutex mts_monitor_mutex;
static struct task_struct *mts_thread_handle;
/*--wake up thread to polling MTS data--*/
void wake_up_mts_monitor(void)
{
	HKLOG("[%s]\n", __func__);
	if (mts_thread_handle != NULL) {
		__pm_stay_awake(&mts_monitor_wake_lock);
		wake_up_process(mts_thread_handle);
	} else
		pr_notice(PMICTAG "[%s] mts_thread_handle not ready\n"
			  , __func__);
}

/*--Monitor MTS reg--*/
static void mt6357_mts_reg_dump(void)
{
	/* AUXADC_ADC_RDY_MDRT & AUXADC_ADC_OUT_MDRT */
	pr_notice("AUXADC_ADC16 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_ADC16));
	pr_notice("AUXADC_ADC17 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_ADC17));
	pr_notice("AUXADC_ADC18 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_ADC18));
	pr_notice("AUXADC_ADC36 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_ADC36));
	pr_notice("AUXADC_MDRT_0 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_MDRT_0));
	pr_notice("AUXADC_MDRT_1 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_MDRT_1));
	pr_notice("AUXADC_MDRT_2 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_MDRT_2));
	pr_notice("AUXADC_MDRT_3 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_MDRT_3));
	pr_notice("AUXADC_MDRT_4 = 0x%x\n"
		  , upmu_get_reg_value(MT6357_AUXADC_MDRT_4));
	/*--AUXADC CLK--*/
	pr_notice("RG_AUXADC_CK_PDN = 0x%x\n"
		  , pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN));
	pr_notice("RG_AUXADC_CK_PDN_HWEN = 0x%x\n"
		  , pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN));
}

void mt6357_auxadc_monitor_mts_regs(void)
{
	int count = 0;
	int mts_timestamp_cur = 0;
	unsigned int mts_adc_tmp = 0;

	if (mts_adc == 0)
		return;
	mts_timestamp_cur = (int)get_monotonic_coarse().tv_sec;
	if ((mts_timestamp_cur - mts_timestamp) < 5)
		return;
	mts_timestamp = mts_timestamp_cur;
	mts_adc_tmp = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	pr_notice("[MTS_ADC] OLD = 0x%x, NOW = 0x%x, CNT = %d\n"
		  , mts_adc, mts_adc_tmp, mts_count);

	if (mts_adc ==  mts_adc_tmp)
		mts_count++;
	else
		mts_count = 0;

	if (mts_count >= 7 && mts_count < 9) {
#if defined(CONFIG_PMIC_HW_ACCESS_EN)
		pwrap_dump_all_register();
#endif
		mt6357_mts_reg_dump();
		/*--AUXADC CH7--*/
		pmic_get_auxadc_value(AUXADC_LIST_TSX);
		pmic_set_hk_reg_value(PMIC_AUXADC_RQST_CH7_BY_GPS, 1);
		udelay(10);
		while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_GPS)
									!= 1) {
			usleep_range(1300, 1500);
			if ((count++) > count_time_out)
				break;
		}
		pmic_set_hk_reg_value(PMIC_AUXADC_RQST_CH7_BY_MD, 1);
		udelay(10);
		while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_CH7_BY_MD)
									!= 1) {
			usleep_range(1300, 1500);
			if ((count++) > count_time_out)
				break;
		}
		pr_notice("AUXADC_ADC16 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC16));
		pr_notice("AUXADC_ADC17 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC17));
		pr_notice("AUXADC_ADC18 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC18));
	}
	if (mts_count > 15) {
#if defined(CONFIG_PMIC_HW_ACCESS_EN)
		pwrap_dump_all_register();
#endif
		pr_notice("DEW_READ_TEST = 0x%x\n"
			  , pmic_get_register_value(PMIC_DEW_READ_TEST));
		/*--AUXADC--*/
		mt6357_mts_reg_dump();
		/*--AUXADC CH7--*/
		pmic_get_auxadc_value(AUXADC_LIST_TSX);
		pr_notice("AUXADC_ADC16 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC16));
		pr_notice("AUXADC_ADC17 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC17));
		pr_notice("AUXADC_ADC18 = 0x%x\n"
			  , upmu_get_reg_value(MT6357_AUXADC_ADC18));
		mts_count = 0;
		wake_up_mts_monitor();
	}
	mts_adc = mts_adc_tmp;
}

int mts_kthread(void *x)
{
	unsigned int i = 0;
	unsigned int polling_cnt = 0;
	unsigned int ktrhead_mts_adc = 0;

	/* Run on a process content */
	while (1) {
		mutex_lock(&mts_monitor_mutex);
		i = 0;
		polling_cnt = 0;
		ktrhead_mts_adc =
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
		while (mts_adc == ktrhead_mts_adc) {
			while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
									== 1) {
				i++;
				mdelay(1);
			}
			while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT)
									== 0) {
				i++;
				mdelay(1);
			}
			ktrhead_mts_adc =
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
			if (polling_cnt % 20 == 0) {
				pr_notice("[MTS_ADC] RDY=1 at %dms, AUXADC_ADC36=0x%x, MDRT_OUT=%d\n"
				, i
				, upmu_get_reg_value(MT6357_AUXADC_ADC36)
				, ktrhead_mts_adc);
			}
			if (polling_cnt >= 312) { /* 312 * 32ms ~= 10s*/
				mt6357_mts_reg_dump();
				aee_kernel_warning("PMIC AUXADC:MDRT", "MDRT");
				break;
			}
			polling_cnt++;
		}
		mutex_unlock(&mts_monitor_mutex);
		__pm_relax(&mts_monitor_wake_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}
/*--Monitor MTS reg End--*/

static void mt6357_auxadc_timeout_dump(unsigned short ch_num)
{
	pr_notice("[%s] (%d) Time out! STA0=0x%x, STA1=0x%x, STA2=0x%x\n",
		__func__,
		ch_num,
		upmu_get_reg_value(MT6357_AUXADC_STA0),
		upmu_get_reg_value(MT6357_AUXADC_STA1),
		upmu_get_reg_value(MT6357_AUXADC_STA2));
	pr_notice("[%s] Reg[0x%x]=0x%x, RG_AUXADC_CK_PDN_HWEN=%d, RG_AUXADC_CK_TSTSEL=%d, RG_SMPS_CK_TSTSEL=%d\n"
		  , __func__
		  , MT6357_STRUP_CON6
		  , upmu_get_reg_value(MT6357_STRUP_CON6)
		  , pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN)
		  , pmic_get_register_value(PMIC_RG_AUXADC_CK_TSTSEL)
		  , pmic_get_register_value(PMIC_RG_SMPS_CK_TSTSEL));
}

static int get_device_adc_out(unsigned short channel,
	struct pmic_auxadc_channel_new *auxadc_channel)
{
	int count = 0, tdet_tmp = 0;
	unsigned short reg_val = 0;

	switch (channel) {
	case AUXADC_LIST_VBIF:
		tdet_tmp = 1;
		HKLOG("pmic auxadc baton_tdet_en should be zero\n");
		pmic_set_hk_reg_value(PMIC_BATON_TDET_EN, 0);
		break;
	case AUXADC_LIST_MT6357_CHIP_TEMP:
		pmic_set_hk_reg_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 0);
		break;
	case AUXADC_LIST_DCXO:
		pmic_set_hk_reg_value(PMIC_AUXADC_DCXO_CH4_MUX_AP_SEL, 1);
		break;
	}

	pmic_set_hk_reg_value(auxadc_channel->channel_rqst, 1);
	udelay(10);

	while (pmic_get_register_value(auxadc_channel->channel_rdy) != 1) {
		usleep_range(1300, 1500);
		if ((count++) > count_time_out) {
			mt6357_auxadc_timeout_dump(auxadc_channel->ch_num);
			break;
		}
	}

	reg_val = pmic_get_register_value(auxadc_channel->channel_out);
	return (int)reg_val;
}

int mt6357_get_auxadc_nolock(u8 channel)
{
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel_new *auxadc_channel;

	if (channel - AUXADC_LIST_START < 0 ||
			channel - AUXADC_LIST_END > 0) {
		pr_notice("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}
	auxadc_channel =
		&mt6357_auxadc_channel[channel-AUXADC_LIST_START];

	/*
	 * be careful when access AUXADC_LIST_BATTEMP
	 * because this API don't hold auxadc_ch3_mutex
	 */
	reg_val = get_device_adc_out(channel, auxadc_channel);

	/* Audio request HPOFS to return raw data */
	if (channel == AUXADC_LIST_HPOFS_CAL)
		adc_result =  reg_val;
	else if (auxadc_channel->resolution == 12)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 4096;
	else if (auxadc_channel->resolution == 15)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 32768;
	pr_notice("[%s] ch_idx = %d, channel = %d, reg_val = 0x%x, adc_result = %d\n"
		  , __func__
		  , channel
		  , auxadc_channel->ch_num
		  , reg_val
		  , adc_result);

	return adc_result;
}

int mt_get_auxadc_value(u8 channel)
{
	int bat_cur = 0, is_charging = 0;
	signed int adc_result = 0, reg_val = 0;
	struct pmic_auxadc_channel_new *auxadc_channel;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	if (channel - AUXADC_LIST_START < 0 ||
			channel - AUXADC_LIST_END > 0) {
		pr_notice("[%s] Invalid channel(%d)\n", __func__, channel);
		return -EINVAL;
	}
	auxadc_channel =
		&mt6357_auxadc_channel[channel-AUXADC_LIST_START];

	pmic_auxadc_lock();
	if (channel == AUXADC_LIST_BATTEMP)
		mutex_lock(&auxadc_ch3_mutex);

	reg_val = get_device_adc_out(channel, auxadc_channel);

	if (channel == AUXADC_LIST_BATTEMP)
		mutex_unlock(&auxadc_ch3_mutex);
	pmic_auxadc_unlock();

	/* Audio request HPOFS to return raw data */
	if (channel == AUXADC_LIST_HPOFS_CAL)
		adc_result =  reg_val;
	else if (auxadc_channel->resolution == 12)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 4096;
	else if (auxadc_channel->resolution == 15)
		adc_result = (reg_val * auxadc_channel->r_val *
					VOLTAGE_FULL_RANGE) / 32768;

	if (channel != AUXADC_LIST_MT6357_BUCK1_TEMP &&
	    channel != AUXADC_LIST_MT6357_BUCK2_TEMP &&
	    __ratelimit(&ratelimit)) {
		if (channel == AUXADC_LIST_BATTEMP) {
#if defined(CONFIG_MTK_SMART_BATTERY)
			is_charging = gauge_get_current(&bat_cur);
#endif
			if (is_charging == 0)
				bat_cur = 0 - bat_cur;
			pr_notice("[%s]ch_idx=%d,channel=%d,bat_cur=%d,reg_val=0x%x,adc_result=%d\n"
				  , __func__
				  , channel
				  , auxadc_channel->ch_num
				  , bat_cur
				  , reg_val
				  , adc_result);
		} else {
			pr_notice("[%s]ch_idx=%d,channel=%d,reg_val=0x%x,adc_result=%d\n"
				  , __func__
				  , channel
				  , auxadc_channel->ch_num
				  , reg_val
				  , adc_result);
		}
	} else {
		HKLOG("[%s]ch_idx=%d,channel=%d,reg_val=0x%x,adc_result=%d\n"
		      ,	__func__
		      , channel
		      , auxadc_channel->ch_num
		      ,	reg_val
		      , adc_result);
	}

	if (channel == AUXADC_LIST_BATTEMP && dbg_flag == 0) {
		dbg_count++;
		if (battmp != 0 &&
		    (adc_result < 200 ||
		     ((adc_result - battmp) > 100) ||
		     ((battmp - adc_result) > 100))) {
			/* dump debug log when VBAT being abnormal */
			pr_notice("old: %d, new: %d\n", battmp, adc_result);
			adc_result = wk_auxadc_battmp_dbg(adc_result);
#if AEE_DBG
			if (aee_count < 2)
				aee_kernel_warning("PMIC AUXADC:BAT TEMP"
						   , "BAT TEMP");
			else
				pr_notice("aee_count=%d\n", aee_count);
			aee_count++;
#endif
		} else if (dbg_count % 50 == 0)
			/* dump debug log in normal case */
			wk_auxadc_battmp_dbg(adc_result);
		battmp = adc_result;
	}
	/*--Monitor MTS Thread--*/
	if (channel == AUXADC_LIST_BATADC)
		mt6357_auxadc_monitor_mts_regs();

	return adc_result;
}

void adc_dbg_init(void)
{
	unsigned short i;
	unsigned int addr = 0x1000;

	/* All of AUXADC */
	for (i = 0; addr <= 0x1236; i++) {
		adc_dbg_addr[i] = addr;
		addr += 0x2;
	}
	/* Clock related */
	adc_dbg_addr[i++] = 0xF8C;
	adc_dbg_addr[i++] = 0xF8E;
	/* RST related */
	adc_dbg_addr[i++] = 0xF90;
	/* Others */
	adc_dbg_addr[i++] = 0xE08;
}

static void mts_thread_init(void)
{
	mts_thread_handle = kthread_create(mts_kthread
					   , (void *)NULL, "mts_thread");
	if (IS_ERR(mts_thread_handle)) {
		mts_thread_handle = NULL;
		pr_notice(PMICTAG "[adc_kthread] creation fails\n");
	} else {
		HKLOG("[adc_kthread] kthread_create Done\n");
	}
}

void pmic_auxadc_init(void)
{
	HKLOG("%s\n", __func__);
	wakeup_source_init(&pmic_auxadc_wake_lock, "PMIC AuxADC wakelock");
	wakeup_source_init(&mts_monitor_wake_lock, "PMIC MTS Monitor wakelock");
	mutex_init(&pmic_adc_mutex);
	mutex_init(&mts_monitor_mutex);
	/* Remove register setting which is set by PMIC initial setting in PL */

	battmp = pmic_get_auxadc_value(AUXADC_LIST_BATTEMP);
	mts_adc = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
#if 0 /*TBD*/
	/* update VBIF28 by AUXADC */
	g_pmic_pad_vbif28_vol = pmic_get_auxadc_value(AUXADC_LIST_VBIF);
	pr_info("****[%s] VBIF28 = %d, BAT TEMP = %d, MTS_ADC = 0x%x\n",
		__func__, pmic_get_vbif28_volt(), battmp, mts_adc);
#endif
	adc_dbg_init();
	mts_thread_init();
	pr_info("****[%s] DONE, BAT TEMP = %d, MTS_ADC = 0x%x\n",
		__func__, battmp, mts_adc);
}
EXPORT_SYMBOL(pmic_auxadc_init);

#define MT6357_AUXADC_DEBUG(_reg)                                       \
{                                                                       \
	value = pmic_get_register_value(_reg);				\
	snprintf(buf+strlen(buf), 1024, "%s = 0x%x\n", #_reg, value);	\
	pr_notice("[%s] %s = 0x%x\n", __func__, #_reg,			\
		pmic_get_register_value(_reg));			\
}

void pmic_auxadc_dump_regs(char *buf)
{
	int value;

	snprintf(buf+strlen(buf), 1024, "====| %s |====\n", __func__);
	MT6357_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SEL);
	MT6357_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_RSTB_SW);
	MT6357_AUXADC_DEBUG(PMIC_RG_STRUP_AUXADC_START_SEL);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_EN);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_PRD);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_WKUP_EN);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_MDRT_DET_SRCLKEN_IND);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_CK_AON);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_SEL);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_DATA_REUSE_EN);
	MT6357_AUXADC_DEBUG(PMIC_AUXADC_TRIM_CH0_SEL);
}
EXPORT_SYMBOL(pmic_auxadc_dump_regs);
