/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <linux/timekeeping.h>
#include <linux/math64.h>

#include <linux/iio/consumer.h>
#include <linux/iio/adc/mt635x-auxadc-internal.h>

#include "include/pmic.h"
#include "include/pmic_auxadc.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_auxadc_intf.h>

#ifndef CONFIG_MTK_GAUGE_VERSION
#define CONFIG_MTK_GAUGE_VERSION 0
#endif
#if (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/v1/mtk_battery.h>
#include <mtk_battery_internal.h>
#endif

static struct device *pmic_auxadc_dev;
static int auxadc_bat_temp_cali(int bat_temp, int precision_factor);

/*********************************
 * PMIC AUXADC Exported API
 *********************************/
static DEFINE_MUTEX(auxadc_ch3_mutex);
static unsigned int g_pmic_pad_vbif28_vol;

unsigned int pmic_get_vbif28_volt(void)
{
	return g_pmic_pad_vbif28_vol;
}

bool is_isense_supported(void)
{
	/* PMIC MT6359 does not support ISENSE */
	return false;
}

void pmic_auxadc_suspend(void)
{
	/* only enable MDRT wakeup when enter suspend */
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN, 1);

	/* no need to disable BAT_TEMP detect, remove wk_auxadc_bgd_ctrl */
	/* special call to restore bat_temp_prev when enter suspend */
	auxadc_bat_temp_cali(-1, -1);
}

void pmic_auxadc_resume(void)
{
	/* disable MDRT wakeup when leave suspend */
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN, 0);

	/* no need to disable BAT_TEMP detect, remove wk_auxadc_bgd_ctrl */

}

void lockadcch3(void)
{
	mutex_lock(&auxadc_ch3_mutex);
}

void unlockadcch3(void)
{
	mutex_unlock(&auxadc_ch3_mutex);
}

void wk_auxadc_reset(void)
{
	pmic_set_register_value(PMIC_HK_AUXADC_KEY, 0x6359);
	pmic_set_register_value(PMIC_RG_AUXADC_RST, 1);
	pmic_set_register_value(PMIC_RG_AUXADC_RST, 0);
	pmic_set_register_value(PMIC_BANK_AUXADC_SWRST, 1);
	pmic_set_register_value(PMIC_BANK_AUXADC_SWRST, 0);
	pmic_set_register_value(PMIC_HK_AUXADC_KEY, 0);
	/* avoid GPS can't receive AUXADC ready after reset, request again */
	if (PMIC_CHIP_VER() != 0x5910)
		pmic_set_register_value(PMIC_AUXADC_RQST_CH7, 1);
	pmic_set_register_value(PMIC_AUXADC_RQST_DCXO_BY_GPS, 1);
	pr_notice("reset AUXADC done\n");
	/* special call to restore bat_temp_prev after reset AUXADC */
	auxadc_bat_temp_cali(-1, -1);
}
/*********************************
 * PMIC AUXADC debug register dump
 *********************************/
#define DBG_REG_ENABLE		1
#define DBG_REG_SIZE		384
#define BAT_TEMP_AEE_DBG	0

struct pmic_adc_dbg_st {
	int ktime_sec;
	unsigned short reg[DBG_REG_SIZE];
};
static unsigned int adc_dbg_addr[DBG_REG_SIZE];

static void wk_auxadc_dbg_dump(void)
{
	unsigned char reg_log[861] = "", reg_str[21] = "";
	unsigned short i, j;
	unsigned int len = 0;
	static unsigned char dbg_stamp;
	static struct pmic_adc_dbg_st pmic_adc_dbg[4];

	if (adc_dbg_addr[0] == 0) {
		/* DBG_REG_ENABLE is 0 */
		return;
	}
	for (i = 0; adc_dbg_addr[i] != 0; i++)
		pmic_adc_dbg[dbg_stamp].reg[i] =
			upmu_get_reg_value(adc_dbg_addr[i]);
	pmic_adc_dbg[dbg_stamp].ktime_sec = (int)get_monotonic_coarse().tv_sec;
	dbg_stamp++;
	if (dbg_stamp >= 4)
		dbg_stamp = 0;

	for (i = 0; i < 4; i++) {
		if (pmic_adc_dbg[dbg_stamp].ktime_sec == 0) {
			dbg_stamp++;
			if (dbg_stamp >= 4)
				dbg_stamp = 0;
			continue;
		}
		for (j = 0; adc_dbg_addr[j] != 0; j++) {
			if (j != 0 && j % 43 == 0) {
				pr_notice("%d %s\n",
					pmic_adc_dbg[dbg_stamp].ktime_sec,
					reg_log);
				strncpy(reg_log, "", 860);
			}
			len += snprintf(reg_str, 20, "Reg[0x%x]=0x%x, ",
					adc_dbg_addr[j],
					pmic_adc_dbg[dbg_stamp].reg[j]);
			strncat(reg_log, reg_str, 860);
		}
		pr_notice("%d %s\n\n",
			pmic_adc_dbg[dbg_stamp].ktime_sec,
			reg_log);
		strncpy(reg_log, "", 860);
		dbg_stamp++;
		if (dbg_stamp >= 4)
			dbg_stamp = 0;
	}
}

/* BAT_TEMP filter Maxima and minima then average */
static int bat_temp_filter(int *arr, unsigned short size)
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

static int wk_bat_temp_dbg(int bat_temp_prev, int bat_temp)
{
	int vbif28, bat_temp_new = bat_temp;
	int arr_bat_temp[5], vbat;
	unsigned short i;

	vbif28 = auxadc_priv_read_channel(pmic_auxadc_dev, AUXADC_VBIF);
	pr_notice("BAT_TEMP_PREV:%d,BAT_TEMP:%d,VBIF28:%d\n",
		bat_temp_prev, bat_temp, vbif28);
	if (bat_temp < 200 || abs(bat_temp_prev - bat_temp) > 100) {
		wk_auxadc_dbg_dump();
		for (i = 0; i < 5; i++) {
			arr_bat_temp[i] =
				auxadc_priv_read_channel(pmic_auxadc_dev,
							 AUXADC_BAT_TEMP);
		}
		bat_temp_new = bat_temp_filter(arr_bat_temp, 5);
		pr_notice("%d,%d,%d,%d,%d, BAT_TEMP_NEW:%d\n",
			arr_bat_temp[0], arr_bat_temp[1], arr_bat_temp[2],
			arr_bat_temp[3], arr_bat_temp[4], bat_temp_new);
		/* Reset AuxADC to observe VBAT/IBAT/BAT_TEMP */
		wk_auxadc_reset();
		for (i = 0; i < 5; i++) {
			vbat = auxadc_priv_read_channel(pmic_auxadc_dev,
							AUXADC_BATADC);
			arr_bat_temp[i] =
				auxadc_priv_read_channel(pmic_auxadc_dev,
							 AUXADC_BAT_TEMP);
			pr_notice("[CH3_DBG] %d,%d\n",
				  vbat, arr_bat_temp[i]);
		}
		bat_temp_new = bat_temp_filter(arr_bat_temp, 5);
		pr_notice("Final BAT_TEMP_NEW:%d\n", bat_temp_new);
	}
	return bat_temp_new;
}

static void wk_auxadc_dbg_init(void)
{
#if DBG_REG_ENABLE
	unsigned short i;
	unsigned int addr = 0xFA2;

	/* All of AUXADC */
	for (i = 0; addr <= 0x127A; i++) {
		adc_dbg_addr[i] = addr;
		addr += 0x2;
	}
	/* Clock related */
	adc_dbg_addr[i++] = MT6359_HK_TOP_CLK_CON0;
	adc_dbg_addr[i++] = MT6359_HK_TOP_CLK_CON1;
	/* RST related */
	adc_dbg_addr[i++] = MT6359_HK_TOP_RST_CON0;
	/* Others */
	adc_dbg_addr[i++] = MT6359_BATON_ANA_CON0;
	adc_dbg_addr[i++] = MT6359_PCHR_VREF_ELR_0;
#endif
}

/*********************************
 * PMIC AUXADC MDRT debug(MTS=Modem Temp Share)
 *********************************/
/* global variable */
static unsigned int mdrt_adc;
static struct wakeup_source *mdrt_wakelock;
static struct mutex mdrt_mutex;
static struct task_struct *mdrt_thread_handle;

/* wake up the thread to polling MDRT data in ms period */
void wake_up_mdrt_thread(void)
{
	HKLOG("[%s]\n", __func__);
	if (mdrt_thread_handle != NULL) {
		__pm_stay_awake(mdrt_wakelock);
		wake_up_process(mdrt_thread_handle);
	} else
		pr_notice(PMICTAG "[%s] mdrt_thread_handle not ready\n",
			__func__);
}

/* dump MDRT related register */
static void mdrt_reg_dump(void)
{
	pr_notice("AUXADC_ADC15 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_ADC15));
	pr_notice("AUXADC_ADC16 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_ADC16));
	pr_notice("AUXADC_ADC17 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_ADC17));
	pr_notice("AUXADC_MDRT_0 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_0));
	pr_notice("AUXADC_MDRT_1 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_1));
	pr_notice("AUXADC_MDRT_2 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_2));
	pr_notice("AUXADC_MDRT_3 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_3));
	pr_notice("AUXADC_MDRT_4 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_4));
	pr_notice("AUXADC_MDRT_5 = 0x%x\n",
		upmu_get_reg_value(MT6359_AUXADC_MDRT_5));
	/*--AUXADC CLK--*/
	pr_notice("RG_AUXADC_CK_PDN = 0x%x, RG_AUXADC_CK_PDN_HWEN = 0x%x\n",
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN));
}

/* Check MDRT_ADC data has changed or not */
void mdrt_monitor(void)
{
	static unsigned int mdrt_cnt;
	static int mdrt_timestamp;
	int mdrt_timestamp_cur = 0;
	unsigned int temp_mdrt_adc = 0;

	if (mdrt_adc == 0)
		return;

	mdrt_timestamp_cur = (int)get_monotonic_coarse().tv_sec;
	if ((mdrt_timestamp_cur - mdrt_timestamp) < 5)
		return;
	mdrt_timestamp = mdrt_timestamp_cur;

	temp_mdrt_adc = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	pr_notice("[MDRT_ADC] OLD = 0x%x, NOW = 0x%x, CNT = %d\n",
		mdrt_adc, temp_mdrt_adc, mdrt_cnt);

	if (temp_mdrt_adc != mdrt_adc) {
		mdrt_cnt = 0;
		mdrt_adc = temp_mdrt_adc;
		return;
	}
	mdrt_cnt++;
	if (mdrt_cnt >= 7 && mdrt_cnt < 9)
		mdrt_reg_dump();
	if (mdrt_cnt > 15) {
		mdrt_reg_dump();
		mdrt_cnt = 0;
		wake_up_mdrt_thread();
	}
	mdrt_adc = temp_mdrt_adc;
}

static int mdrt_polling_rdy(unsigned int *trig_prd,
			    unsigned int *rdy_time, unsigned int *mdrt_adc)
{
	while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT) == 1) {
		if (*trig_prd > 100) {
			pr_notice("[MDRT_ADC] no trigger\n");
			return -1;
		}
		(*trig_prd)++;
		mdelay(1);
	}
	while (pmic_get_register_value(PMIC_AUXADC_ADC_RDY_MDRT) == 0) {
		if (*rdy_time > 100) {
			pr_notice("[MDRT_ADC] no ready\n");
			wk_auxadc_reset();
			return -2;
		}
		(*rdy_time)++;
		mdelay(1);
	}
	*mdrt_adc = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	return 0;
}

static int mdrt_kthread(void *x)
{
	unsigned int polling_cnt;
	unsigned int trig_prd;
	unsigned int rdy_time;
	unsigned int temp_mdrt_adc;

	/* Run on a process content */
	while (1) {
		mutex_lock(&mdrt_mutex);
		polling_cnt = 0;
		temp_mdrt_adc = pmic_get_register_value(
			PMIC_AUXADC_ADC_OUT_MDRT);
		while (mdrt_adc == temp_mdrt_adc) {
			trig_prd = 0;
			rdy_time = 0;
			mdrt_polling_rdy(&trig_prd,
					 &rdy_time, &temp_mdrt_adc);

			if (polling_cnt % 20 == 0) {
				pr_notice("[MDRT_ADC] trig_prd=%d, rdy_time=%d, MDRT_OUT=%d\n"
					, trig_prd, rdy_time,
					temp_mdrt_adc);
			}
			if (polling_cnt == 156) { /* 156 * 32ms ~= 5s*/
				pr_notice("[MDRT_ADC] (%d) reset AUXADC\n",
					polling_cnt);
				wk_auxadc_reset();
			}
			if (polling_cnt >= 312) { /* 312 * 32ms ~= 10s*/
				mdrt_reg_dump();
#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning("PMIC AUXADC:MDRT", "MDRT");
#endif
				break;
			}
			polling_cnt++;
		}
		mutex_unlock(&mdrt_mutex);
		__pm_relax(mdrt_wakelock);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

static void mdrt_monitor_init(void)
{
	//wakeup_source_init(&mdrt_wakelock, "MDRT Monitor wakelock");
	mdrt_wakelock = wakeup_source_register(NULL, "MDRT Monitor wakelock");
	mutex_init(&mdrt_mutex);
	mdrt_adc = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_MDRT);
	mdrt_thread_handle = kthread_create(mdrt_kthread, NULL, "mdrt_thread");
	if (IS_ERR(mdrt_thread_handle)) {
		mdrt_thread_handle = NULL;
		pr_notice(PMICTAG "[%s] creation fails\n", __func__);
	} else
		HKLOG("[%s] creation Done\n", __func__);
}

/*********************************
 * Legacy API for getting PMIC AUXADC value
 *********************************/
struct legacy_auxadc_t {
	const char *channel_name;
	struct iio_channel *chan;
};

#define LEGACY_AUXADC_GEN(_name)	\
{	\
	.channel_name = "AUXADC_"#_name,\
}

static struct legacy_auxadc_t legacy_auxadc[] = {
	LEGACY_AUXADC_GEN(BATADC),
	LEGACY_AUXADC_GEN(VCDT),
	LEGACY_AUXADC_GEN(BAT_TEMP),
	LEGACY_AUXADC_GEN(CHIP_TEMP),
	LEGACY_AUXADC_GEN(VCORE_TEMP),
	LEGACY_AUXADC_GEN(VPROC_TEMP),
	LEGACY_AUXADC_GEN(VGPU_TEMP),
	LEGACY_AUXADC_GEN(ACCDET),
	LEGACY_AUXADC_GEN(VDCXO),
	LEGACY_AUXADC_GEN(TSX_TEMP),
	LEGACY_AUXADC_GEN(HPOFS_CAL),
	LEGACY_AUXADC_GEN(DCXO_TEMP),
	LEGACY_AUXADC_GEN(VBIF),
};

static void legacy_auxadc_init(struct device *dev)
{
	int i = 0;

	for (i = AUXADC_LIST_START; i <= AUXADC_LIST_END; i++) {
		legacy_auxadc[i].chan =
			devm_iio_channel_get(dev,
					     legacy_auxadc[i].channel_name);
		if (IS_ERR(legacy_auxadc[i].chan))
			pr_notice("%s get fail with list %d, dev_name:%s\n",
				legacy_auxadc[i].channel_name, i,
				dev_name(dev));
	}
}

int pmic_get_auxadc_value(int list)
{
	int bat_cur = 0, is_charging = 0;
	int value = 0, ret = 0;

	if (list < AUXADC_LIST_START || list > AUXADC_LIST_END) {
		pr_notice("[%s] Invalid channel list(%d)\n", __func__, list);
		return -EINVAL;
	}
	if (!(legacy_auxadc[list].chan) || IS_ERR(legacy_auxadc[list].chan)) {
		pr_notice("[%s] iio channel consumer error(%s)\n",
			__func__, legacy_auxadc[list].channel_name);
		return PTR_ERR(legacy_auxadc[list].chan);
	}
	if (list == AUXADC_LIST_BATTEMP) {
		ret = iio_read_channel_processed(
			legacy_auxadc[AUXADC_LIST_VBIF].chan,
			&g_pmic_pad_vbif28_vol);
#if (CONFIG_MTK_GAUGE_VERSION == 30)
		is_charging = gauge_get_current(&bat_cur);
#endif
		if (is_charging == 0)
			bat_cur = 0 - bat_cur;
		pr_notice("[CH3_DBG] bat_cur = %d\n", bat_cur);
	}
	if (list == AUXADC_LIST_HPOFS_CAL) {
		ret = iio_read_channel_raw(
			legacy_auxadc[list].chan, &value);
	} else {
		ret = iio_read_channel_processed(
			legacy_auxadc[list].chan, &value);
	}
	if (ret < 0)
		return ret;
	return value;
}

/*********************************
 * PMIC AUXADC chip setting for IIO ADC driver
 *********************************/
static void auxadc_batadc_convert(unsigned char convert)
{
	/* when user wants to get BATADC, starting monitor MDRT for debugging*/
	if (convert == 0)
		mdrt_monitor();
}

static void auxadc_bat_temp_convert(unsigned char convert)
{
	if (convert == 1)
		mutex_lock(&auxadc_ch3_mutex);
	else if (convert == 0)
		mutex_unlock(&auxadc_ch3_mutex);
}

/* MT6359 no need to set RG_BATON_TDET_EN */

static int auxadc_bat_temp_cali(int bat_temp, int precision_factor)
{
	static int bat_temp_prev;
	static unsigned int dbg_count;
	static unsigned int aee_count;

	if (bat_temp == -1 && precision_factor == -1) {
		bat_temp_prev = 0;
		return 0;
	}
	if (bat_temp_prev == 0)
		goto out;

	dbg_count++;
	if (bat_temp < 200 || abs(bat_temp_prev - bat_temp) > 100) {
		/* dump debug log when BAT_TEMP being abnormal */
		bat_temp = wk_bat_temp_dbg(bat_temp_prev, bat_temp);
#if BAT_TEMP_AEE_DBG
#ifdef CONFIG_MTK_AEE_FEATURE
		if (aee_count < 2)
			aee_kernel_warning("PMIC AUXADC:BAT_TEMP", "BAT_TEMP");
#endif
		pr_notice("PMIC AUXADC BAT_TEMP aee_count=%d\n", aee_count);
#endif
		aee_count++;
	} else if (dbg_count % 50 == 0) {
		/* dump debug log in normal case */
		wk_bat_temp_dbg(bat_temp_prev, bat_temp);
	}
out:
	bat_temp_prev = bat_temp;
	return bat_temp;
}

int pmic_auxadc_chip_init(struct device *dev)
{
	int ret = 0;
	struct iio_channel *chan_vbif = NULL;

	HKLOG("%s\n", __func__);
	pmic_auxadc_dev = dev;

	auxadc_set_convert_fn(AUXADC_BATADC, auxadc_batadc_convert);
	auxadc_set_convert_fn(AUXADC_BAT_TEMP, auxadc_bat_temp_convert);
	auxadc_set_cali_fn(AUXADC_BAT_TEMP, auxadc_bat_temp_cali);

	legacy_auxadc_init(dev);

	wk_auxadc_dbg_init();
	mdrt_monitor_init();
	/* only enable MDRT wakeup when enter suspend */
	pmic_set_register_value(PMIC_AUXADC_MDRT_DET_WKUP_EN, 0);
	/* set MDRT_WAKEUP AVG_NUM to the same with Ch7(128 samples) */
	pmic_set_register_value(PMIC_AUXADC_AVG_NUM_CH7_WAKEUP, 0x6);

	/* update VBIF28 by AUXADC */
	chan_vbif = iio_channel_get(dev, "AUXADC_VBIF");
	if (IS_ERR(chan_vbif)) {
		pr_notice("[%s] iio channel consumer error(AUXADC_VBIF)\n",
			__func__);
	} else {
		ret = iio_read_channel_processed(chan_vbif,
						 &g_pmic_pad_vbif28_vol);
		iio_channel_release(chan_vbif);
	}
	pr_info("****[%s] VBIF28 = %d, MDRT_ADC = 0x%x\n",
		__func__, pmic_get_vbif28_volt(), mdrt_adc);

	return ret;
}
