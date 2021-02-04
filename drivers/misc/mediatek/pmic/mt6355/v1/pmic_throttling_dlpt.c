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
#include <linux/device.h>
#include <linux/pm_wakeup.h>
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
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_auxadc_intf.h>
#include "include/pmic.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_irq.h"
#if defined(CONFIG_MTK_PMIC_WRAP_HAL)
#include <mach/mtk_pmic_wrap.h>
#endif
#include <mt-plat/mtk_rtc.h>
/*#include <mach/mtk_spm_mtcmos.h>*/

#include <linux/time.h>

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_boot_common.h>
/*#include <mach/system.h> TBD*/
#include <mt-plat/mtk_gpt.h>
#endif

#if (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/mtk_battery.h>
#include <mach/mtk_battery_property.h>
#include <linux/reboot.h>
#include <mtk_battery_internal.h>
#else
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mach/mtk_battery_meter.h>
#endif

#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_reboot.h>

#ifdef CONFIG_MACH_MT6757
#include "mtk_spm_dpidle_mt6757.h"
#endif

#if defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758)
#if defined(CONFIG_MTK_BASE_POWER)
#include "mtk_idle.h"
#endif
#endif
#include "include/mt635x-auxadc-internal.h"

/****************************************************************************
 * PMIC related define
 *****************************************************************************/
#define PMIC_THROTTLING_DLPT_UT 0
#define UNIT_FGCURRENT     (381470)
/****************************************************************************
 * PMIC read/write APIs
 *****************************************************************************/

#if PMIC_THROTTLING_DLPT_UT
/* UT test code TBD */
void low_bat_test(LOW_BATTERY_LEVEL level_val)
{
	pr_info("[%s] get %d\n", __func__, level_val);
}

void bat_oc_test(BATTERY_OC_LEVEL level_val)
{
	pr_info("[%s] get %d\n", __func__, level_val);
}

void bat_per_test(BATTERY_PERCENT_LEVEL level_val)
{
	pr_info("[%s] get %d\n", __func__, level_val);
}
#endif

/***************************************************************************
 * Low battery call back function
 ****************************************************************************/
#define LBCB_NUM 16

#ifndef DISABLE_LOW_BATTERY_PROTECT
#define LOW_BATTERY_PROTECT
#endif

int g_lowbat_int_bottom;

#ifdef LOW_BATTERY_PROTECT
/* ex. 3400/5400*4096*/
#define BAT_HV_THD   (POWER_INT0_VOLT*4096/5400)	/*ex: 3400mV */
#define BAT_LV_1_THD (POWER_INT1_VOLT*4096/5400)	/*ex: 3250mV */
#define BAT_LV_2_THD (POWER_INT2_VOLT*4096/5400)	/*ex: 3000mV */

int g_low_battery_level;
int g_low_battery_stop;
/* give one change to ignore DLPT power off. battery voltage
 * may return to 3.25 or higher because loading become light.
 */
int g_low_battery_if_power_off;

struct low_battery_callback_table {
	void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif
void (*low_battery_callback)(LOW_BATTERY_LEVEL level_tag);

void register_low_battery_notify(
		void (*low_battery_callback)(LOW_BATTERY_LEVEL level_tag),
		LOW_BATTERY_PRIO prio_val)
{
#ifdef LOW_BATTERY_PROTECT
	PMICLOG("[%s] start\n", __func__);

	lbcb_tb[prio_val].lbcb = low_battery_callback;

	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
#endif				/*end of #ifdef LOW_BATTERY_PROTECT */
}
#ifdef LOW_BATTERY_PROTECT
void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level)
{				/*0:no limit */
	int i = 0;

	if (g_low_battery_stop == 1) {
		pr_info("[%s] g_low_battery_stop=%d\n"
			, __func__, g_low_battery_stop);
	} else {
		PMICLOG("[%s] prio_val=%d,low_battery=%d\n"
			, __func__, i, low_battery_level);
		for (i = 0; i < LBCB_NUM; i++) {
			if (lbcb_tb[i].lbcb != NULL) {
				low_battery_callback = lbcb_tb[i].lbcb;
				low_battery_callback(low_battery_level);
			}
		}
	}
}

static void lbat_min_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN, en_val);
	pmic_enable_interrupt(INT_BAT_L, en_val, "pmic_throttling_dlpt");
}

static void lbat_max_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX, en_val);
	pmic_enable_interrupt(INT_BAT_H, en_val, "pmic_throttling_dlpt");
}

void low_battery_protect_init(void)
{
	/*default setting */
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN, 0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX, 0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0, 15);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16, 0);

	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX, BAT_HV_THD);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN, BAT_LV_1_THD);

	lbat_min_en_setting(1);
	lbat_max_en_setting(0);

	pr_info("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n"
		, PMIC_AUXADC_LBAT_VOLT_MAX_ADDR
		, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR)
		, PMIC_AUXADC_LBAT_VOLT_MIN_ADDR
		, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR)
		, PMIC_RG_INT_EN_BAT_L_ADDR
		, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR));

	pr_info("[%s] %d mV, %d mV, %d mV\n"
		, __func__, POWER_INT0_VOLT
		, POWER_INT1_VOLT, POWER_INT2_VOLT);
	PMICLOG("[%s] Done\n", __func__);

}

#endif				/*#ifdef LOW_BATTERY_PROTECT */

/*******************************************************************
 * Battery OC call back function
 *******************************************************************/
#define OCCB_NUM 16

#ifndef DISABLE_BATTERY_OC_PROTECT
#define BATTERY_OC_PROTECT
#endif

#ifdef BATTERY_OC_PROTECT
#if defined(CONFIG_MTK_SMART_BATTERY)

#if (CONFIG_MTK_GAUGE_VERSION == 30)
/*
 * I(0.1mA)=reg *UNIT_FGCURRENT /100000 * 100 / fg_cust_data.r_fg_value *
 * fg_cust_data.car_tune_value /1000
 *
 * I(1mA)=reg *UNIT_FGCURRENT /100000 *100 /fg_cust_data.r_fg_value *
 * fg_cust_data.car_tune_value /1000 /10
 *
 * Reg=I /UNIT_FGCURRENT *100000 /100 *fg_cust_data.r_fg_value /
 * fg_cust_data.car_tune_value *10000 *95 /100
 *
 * Ricky:need *0.95
 * 65535-reg
 * (65535-(I *fg_cust_data.r_fg_value *1000 /UNIT_FGCURRENT *95 *100
 *  /fg_cust_data.car_tune_value))
 */
#define BAT_OC_H_THD   \
(65535-(POWER_BAT_OC_CURRENT_H*fg_cust_data.r_fg_value*1000/UNIT_FGCURRENT* \
	95*100/fg_cust_data.car_tune_value))
#define BAT_OC_L_THD   \
(65535-(POWER_BAT_OC_CURRENT_L*fg_cust_data.r_fg_value*1000/UNIT_FGCURRENT* \
	95*100/fg_cust_data.car_tune_value))
#define BAT_OC_H_THD_RE   \
(65535-(POWER_BAT_OC_CURRENT_H_RE*fg_cust_data.r_fg_value*1000 \
	/UNIT_FGCURRENT*95*100/fg_cust_data.car_tune_value))
#define BAT_OC_L_THD_RE   \
(65535-(POWER_BAT_OC_CURRENT_L_RE*fg_cust_data.r_fg_value*1000 \
	/UNIT_FGCURRENT*95*100/fg_cust_data.car_tune_value))
#else
/* ex. Ireg = 65535 - (I * 950000uA / 2 / 158.122 / CAR_TUNE_VALUE * 100)*/
/* (950000/2/158.122)*100~=300400*/
#define BAT_OC_H_THD   \
	(65535-((300400*POWER_BAT_OC_CURRENT_H/1000)/	\
	batt_meter_cust_data.car_tune_value))		/*ex: 4670mA */
#define BAT_OC_L_THD   \
	(65535-((300400*POWER_BAT_OC_CURRENT_L/1000)/	\
	batt_meter_cust_data.car_tune_value))		/*ex: 5500mA */
#define BAT_OC_H_THD_RE   \
	(65535-((300400*POWER_BAT_OC_CURRENT_H_RE/1000)/ \
	batt_meter_cust_data.car_tune_value))		/*ex: 3400mA */
#define BAT_OC_L_THD_RE   \
	(65535-((300400*POWER_BAT_OC_CURRENT_L_RE/1000)/ \
	batt_meter_cust_data.car_tune_value))		/*ex: 4000mA */
#endif

#else
#define BAT_OC_H_THD   0xc047
#define BAT_OC_L_THD   0xb4f4
#define BAT_OC_H_THD_RE  0xc047
#define BAT_OC_L_THD_RE   0xb4f4
#endif			/* end of #if defined(CONFIG_MTK_SMART_BATTERY) */
int g_battery_oc_level;
int g_battery_oc_stop;

struct battery_oc_callback_table {
	void *occb;
};

struct battery_oc_callback_table occb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif				/*end of #ifdef BATTERY_OC_PROTECT */
void (*battery_oc_callback)(BATTERY_OC_LEVEL level_tag);

void register_battery_oc_notify(
		void (*battery_oc_callback)(BATTERY_OC_LEVEL level_tag),
		BATTERY_OC_PRIO prio_val)
{
#ifdef BATTERY_OC_PROTECT
	PMICLOG("[%s] start\n", __func__);

	occb_tb[prio_val].occb = battery_oc_callback;

	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
#endif
}

void exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level)
{				/*0:no limit */
#ifdef BATTERY_OC_PROTECT
	int i = 0;

	if (g_battery_oc_stop == 1) {
		pr_info("[%s] g_battery_oc_stop=%d\n"
			, __func__, g_battery_oc_stop);
	} else {
		for (i = 0; i < OCCB_NUM; i++) {
			if (occb_tb[i].occb != NULL) {
				battery_oc_callback = occb_tb[i].occb;
				battery_oc_callback(battery_oc_level);
				pr_info
				    ("[%s] prio_val=%d,battery_oc_level=%d\n"
				     , __func__, i, battery_oc_level);
			}
		}
	}
#endif
}
#ifdef BATTERY_OC_PROTECT
void bat_oc_h_en_setting(int en_val)
{
	pmic_enable_interrupt(INT_FG_CUR_H, en_val, "pmic_throttling_dlpt");
	/* mt6325_upmu_set_rg_int_en_fg_cur_h(en_val); */
}

void bat_oc_l_en_setting(int en_val)
{
	pmic_enable_interrupt(INT_FG_CUR_L, en_val, "pmic_throttling_dlpt");
	/*mt6325_upmu_set_rg_int_en_fg_cur_l(en_val); */
}

void battery_oc_protect_init(void)
{
	pmic_set_register_value(PMIC_FG_CUR_HTH, BAT_OC_H_THD);
	/*mt6325_upmu_set_fg_cur_hth(BAT_OC_H_THD); */
	pmic_set_register_value(PMIC_FG_CUR_LTH, BAT_OC_L_THD);
	/*mt6325_upmu_set_fg_cur_lth(BAT_OC_L_THD); */

	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(1);

	pr_info("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n"
		, PMIC_FG_CUR_HTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR)
		, PMIC_FG_CUR_LTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR)
		, PMIC_RG_INT_EN_FG_BAT0_H_ADDR
		, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));

	pr_info("[%s] %d mA, %d mA\n"
		, __func__
		, POWER_BAT_OC_CURRENT_H, POWER_BAT_OC_CURRENT_L);
	PMICLOG("[%s] Done\n", __func__);
}

void battery_oc_protect_reinit(void)
{
#ifdef BATTERY_OC_PROTECT
	pmic_set_register_value(PMIC_FG_CUR_HTH, BAT_OC_H_THD_RE);
	/*mt6325_upmu_set_fg_cur_hth(BAT_OC_H_THD_RE); */
	pmic_set_register_value(PMIC_FG_CUR_LTH, BAT_OC_L_THD_RE);
	/*mt6325_upmu_set_fg_cur_lth(BAT_OC_L_THD_RE); */

	pr_info("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n"
		, PMIC_FG_CUR_HTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR)
		, PMIC_FG_CUR_LTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR)
		, PMIC_RG_INT_EN_FG_BAT0_H_ADDR
		, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));

	pr_info("[%s] %d mA, %d mA\n"
		, __func__
		, POWER_BAT_OC_CURRENT_H_RE, POWER_BAT_OC_CURRENT_L_RE);
	pr_info("[%s] Done\n", __func__);
#else
	pr_info("[%s] no define BATTERY_OC_PROTECT\n", __func__);
#endif
}
#endif				/* #ifdef BATTERY_OC_PROTECT */


/*******************************************************************
 * 15% notify service
 *******************************************************************/
#ifndef DISABLE_BATTERY_PERCENT_PROTECT
#define BATTERY_PERCENT_PROTECT
#endif

#ifdef BATTERY_PERCENT_PROTECT
static struct hrtimer bat_percent_notify_timer;
static struct task_struct *bat_percent_notify_thread;
static bool bat_percent_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
#endif
struct wakeup_source bat_percent_notify_lock;

static DEFINE_MUTEX(bat_percent_notify_mutex);

#ifdef BATTERY_PERCENT_PROTECT
/*extern unsigned int bat_get_ui_percentage(void);*/

#define BPCB_NUM 16

int g_battery_percent_level;
int g_battery_percent_stop;

#define BAT_PERCENT_LINIT 15

struct battery_percent_callback_table {
	void *bpcb;
};

struct battery_percent_callback_table bpcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif				/* end of #ifdef BATTERY_PERCENT_PROTECT */
void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL level_tag);

void register_battery_percent_notify(
	void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL level_tag),
	BATTERY_PERCENT_PRIO prio_val)
{
#ifdef BATTERY_PERCENT_PROTECT
	PMICLOG("[%s] start\n", __func__);

	bpcb_tb[prio_val].bpcb = battery_percent_callback;

	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if ((g_battery_percent_stop == 0) && (g_battery_percent_level == 1)) {
#ifdef DISABLE_DLPT_FEATURE
		pr_info("[%s] level l happen\n", __func__);
		battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
#else
		if (prio_val == BATTERY_PERCENT_PRIO_FLASHLIGHT) {
			pr_info("[%s at DLPT] level l happen\n", __func__);
			battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
		}
#endif
	}
#endif				/* end of #ifdef BATTERY_PERCENT_PROTECT */
}

#ifdef BATTERY_PERCENT_PROTECT
void exec_battery_percent_callback(BATTERY_PERCENT_LEVEL battery_percent_level)
{				/*0:no limit */
#ifdef DISABLE_DLPT_FEATURE
	int i = 0;
#endif

	if (g_battery_percent_stop == 1) {
		pr_info("[%s] g_battery_percent_stop=%d\n"
			, __func__, g_battery_percent_stop);
	} else {
#ifdef DISABLE_DLPT_FEATURE
		for (i = 0; i < BPCB_NUM; i++) {
			if (bpcb_tb[i].bpcb != NULL) {
				battery_percent_callback = bpcb_tb[i].bpcb;
				battery_percent_callback(battery_percent_level);
				pr_info("[%s] prio_val=%d,battery_percent_level=%d\n"
				     , __func__, i, battery_percent_level);
			}
		}
#else
		battery_percent_callback =
			bpcb_tb[BATTERY_PERCENT_PRIO_FLASHLIGHT].bpcb;
		if (battery_percent_callback != NULL)
			battery_percent_callback(battery_percent_level);
		else
			pr_info("[%s]BATTERY_PERCENT_PRIO_FLASHLIGHT is null\n"
				, __func__);
		pr_info("[%s at DLPT] prio_val=%d,battery_percent_level=%d\n"
			, __func__
			, BATTERY_PERCENT_PRIO_FLASHLIGHT
			, battery_percent_level);
#endif
	}
}

int bat_percent_notify_handler(void *unused)
{
	ktime_t ktime;
	int bat_per_val = 0;

	do {
		ktime = ktime_set(10, 0);

		wait_event_interruptible(bat_percent_notify_waiter,
					 (bat_percent_notify_flag == true));

		__pm_stay_awake(&bat_percent_notify_lock);
		mutex_lock(&bat_percent_notify_mutex);

#if defined(CONFIG_MTK_SMART_BATTERY)
		bat_per_val = bat_get_ui_percentage();
#endif
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if ((upmu_get_rgs_chrdet() == 0) &&
		    (g_battery_percent_level == 0) &&
		    (bat_per_val <= BAT_PERCENT_LINIT)) {
#else
		if ((g_battery_percent_level == 0) &&
		    (bat_per_val <= BAT_PERCENT_LINIT)) {
#endif
			g_battery_percent_level = 1;
			exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
		} else if ((g_battery_percent_level == 1) &&
			   (bat_per_val > BAT_PERCENT_LINIT)) {
			g_battery_percent_level = 0;
			exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_0);
		} else {
		}
		bat_percent_notify_flag = false;

		PMICLOG("bat_per_level=%d,bat_per_val=%d\n",
			g_battery_percent_level, bat_per_val);

		mutex_unlock(&bat_percent_notify_mutex);
		__pm_relax(&bat_percent_notify_lock);

		hrtimer_start(&bat_percent_notify_timer, ktime,
			      HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart bat_percent_notify_task(struct hrtimer *timer)
{
	bat_percent_notify_flag = true;
	wake_up_interruptible(&bat_percent_notify_waiter);
	PMICLOG("%s is called\n", __func__);

	return HRTIMER_NORESTART;
}

void bat_percent_notify_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(20, 0);
	hrtimer_init(&bat_percent_notify_timer
			, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bat_percent_notify_timer.function = bat_percent_notify_task;
	hrtimer_start(&bat_percent_notify_timer
		      , ktime, HRTIMER_MODE_REL);

	bat_percent_notify_thread =
	    kthread_run(bat_percent_notify_handler
			, 0, "bat_percent_notify_thread");
	if (IS_ERR(bat_percent_notify_thread))
		pr_info("Failed to create bat_percent_notify_thread\n");
	else
		pr_info("Create bat_percent_notify_thread : done\n");
}
#endif				/* #ifdef BATTERY_PERCENT_PROTECT */

/*******************************************************************
 * AuxADC Impedence Measurement
 *******************************************************************/
static unsigned int count_time_out_adc_imp = 36;
static struct wakeup_source ptim_wake_lock;
static struct mutex ptim_mutex;

static void ptim_lock(void)
{
	__pm_stay_awake(&ptim_wake_lock);
	mutex_lock(&ptim_mutex);
}

static void ptim_unlock(void)
{
	mutex_unlock(&ptim_mutex);
	__pm_relax(&ptim_wake_lock);
}

void pmic_dump_adc_impedance(void)
{
	pr_info("Dump Impedence Value\n");
	pr_info("AUXADC_IMP0:   0x%x\n"
		, upmu_get_reg_value(MT6355_AUXADC_IMP0));
	pr_info("AUXADC_IMP1:   0x%x\n"
		, upmu_get_reg_value(MT6355_AUXADC_IMP1));
	pr_info("AUXADC_ADC33:  0x%x\n"
		, upmu_get_reg_value(MT6355_AUXADC_ADC33));
	pr_info("AUXADC_ADC34:  0x%x\n"
		, upmu_get_reg_value(MT6355_AUXADC_ADC34));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_AUXADC_IMP_CG0
		, upmu_get_reg_value(MT6355_AUXADC_IMP_CG0));
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
		, MT6355_AUXADC_CON0
		, upmu_get_reg_value(MT6355_AUXADC_CON0));
	pr_info("[0x%x] 0x%x\n"
		, MT6355_STRUP_CON6
		, upmu_get_reg_value(MT6355_STRUP_CON6));
}

int do_ptim_internal(
	bool isSuspend, unsigned int *bat, signed int *cur, bool *is_charging)
{
	unsigned int vbat_reg;
	unsigned int count_adc_imp = 0;
	int ret = 0;

	/* initial setting */
	pmic_set_register_value(PMIC_AUXADC_SPL_NUM_LARGE, 0x6);
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_PRD, 0x6);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CHSEL, 0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CNT, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE, 1);

	/* enable setting */
	pmic_set_register_value(PMIC_RG_AUXADC_IMP_CK_SW_EN, 1);
	pmic_set_register_value(PMIC_RG_AUXADC_IMP_CK_SW_MODE, 1);

	/* start setting */
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN, 1);

	/* polling IRQ status */
	while (pmic_get_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_STATUS) == 0) {
		if ((count_adc_imp++) > count_time_out_adc_imp) {
			pr_info("do_ptim over %d times/ms\n", count_adc_imp);
			pmic_dump_adc_impedance();
			ret = 1;
			break;
		}
		mdelay(1);
	}
	vbat_reg = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_IMP);

	/*clear irq */
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 1);
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 0);

	/* stop setting */
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN, 0);

	/* disable setting */
	pmic_set_register_value(PMIC_RG_AUXADC_IMP_CK_SW_MODE, 0);

	/*disabled, may removed*/
	/*pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE, 0);*/
	/*pmic_set_register_value(PMIC_RG_AUXADC_IMP_CK_SW_EN, 1);*/

	if (count_adc_imp <= count_time_out_adc_imp)
		*bat = (vbat_reg * 3 * 18000) >> 15;
	else if (isSuspend)
		*bat = auxadc_priv_read_channel(AUXADC_BATADC) * 10;
	else
		*bat = pmic_get_auxadc_value(AUXADC_LIST_BATADC) * 10;

#if defined(CONFIG_MTK_SMART_BATTERY)
	gauge_get_ptim_current(cur, is_charging);
#else
	*cur = 0;
#endif
	if (ret) {
		pmic_set_register_value(PMIC_RG_AUXADC_RST, 1);
		pmic_set_register_value(PMIC_RG_AUXADC_RST, 0);
	}
	pr_info("%s : bat %d cur %d\n", __func__, *bat, *cur);

	return ret;
}

int do_ptim_gauge(
	bool isSuspend, unsigned int *bat, signed int *cur, bool *is_charging)
{
	int ret;

	if (isSuspend == false)
		ptim_lock();

	ret = do_ptim_internal(isSuspend, bat, cur, is_charging);

	if (isSuspend == false)
		ptim_unlock();
	return ret;
}

/*******************************************************************
 * DLPT service
 *******************************************************************/
#ifndef DISABLE_DLPT_FEATURE
#define DLPT_FEATURE_SUPPORT
#endif

#ifdef DLPT_FEATURE_SUPPORT

unsigned int ptim_bat_vol;
signed int ptim_R_curr;
int ptim_imix;
int ptim_rac_val_avg;
static int g_imix_val;

void get_ptim_value(bool isSuspend, unsigned int *bat, signed int *cur)
{
	if (isSuspend == false)
		ptim_lock();
	*bat = ptim_bat_vol;
	*cur = ptim_R_curr;
	if (isSuspend == false)
		ptim_unlock();
}

int get_rac(void) { return ptim_rac_val_avg; }

int get_imix(void) { return g_imix_val; }

int do_ptim(bool isSuspend)
{
	int ret;
	bool is_charging;

	if (isSuspend == false)
		ptim_lock();

	ret = do_ptim_internal(isSuspend, &ptim_bat_vol, &ptim_R_curr,
			       &is_charging);

	if (isSuspend == false)
		ptim_unlock();
	return ret;
}

void enable_dummy_load(unsigned int en)
{
	if (en == 1) {
		/*1. disable isink pdn */ /* no design at 55*/
		/* pmic_set_register_value(PMIC_RG_DRV_32K_CK_PDN, 0); */

		/* enable isink step */
		pmic_set_register_value(PMIC_ISINK_CH0_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH1_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH2_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH3_STEP, 0x7);

		/* double function */
		/*CH3 double on  */
		pmic_set_register_value(PMIC_RG_ISINK3_DOUBLE, 0x1);
		/*CH2 double on  */
		pmic_set_register_value(PMIC_RG_ISINK2_DOUBLE, 0x1);
		/*CH1 double off */
		pmic_set_register_value(PMIC_RG_ISINK1_DOUBLE, 0);
		/*CH0 double off */
		pmic_set_register_value(PMIC_RG_ISINK0_DOUBLE, 0);

		/*enable isink */
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH1_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP1_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP0_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH1_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_EN, 0);
		/*PMICLOG("[enable dummy load]\n"); */
	} else {
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH1_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP1_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CHOP0_EN, 0); /* no chop */
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH1_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH0_BIAS_EN, 0);

		/*1. enable isink pdn */ /* no design at 55*/
		/* pmic_set_register_value(PMIC_RG_DRV_32K_CK_PDN, 0x1); */

		/*PMICLOG("[disable dummy load]\n"); */
	}
}

static struct hrtimer dlpt_notify_timer;
static struct task_struct *dlpt_notify_thread;
static bool dlpt_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(dlpt_notify_waiter);
#endif
struct wakeup_source dlpt_notify_lock;
static DEFINE_MUTEX(dlpt_notify_mutex);

#ifdef DLPT_FEATURE_SUPPORT
#define DLPT_NUM 16
/* This define is used to filter smallest and largest voltage and current.
 * To avoid auxadc measurement interference issue.
 */
#define DLPT_SORT_IMIX_VOLT_CURR 1


int g_dlpt_stop;
unsigned int g_dlpt_val;

int g_dlpt_start;

int g_low_per_timer;
int g_low_per_timeout_val = 60;


int g_lbatInt1 = DLPT_VOLT_MIN * 10;

struct dlpt_callback_table {
	void *dlpt_cb;
};

struct dlpt_callback_table dlpt_cb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};

void (*dlpt_callback)(unsigned int val);

void register_dlpt_notify(
	void (*dlpt_callback)(unsigned int val),
	DLPT_PRIO prio_val)
{
	PMICLOG("[%s] start\n", __func__);

	dlpt_cb_tb[prio_val].dlpt_cb = dlpt_callback;

	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if ((g_dlpt_stop == 0) && (g_dlpt_val != 0)) {
		pr_notice("[%s] dlpt happen\n", __func__);
		if (dlpt_callback != NULL)
			dlpt_callback(g_dlpt_val);
	}
}

void exec_dlpt_callback(unsigned int dlpt_val)
{
	int i = 0;

	g_dlpt_val = dlpt_val;

	if (g_dlpt_stop == 1) {
		pr_info("[%s] g_dlpt_stop=%d\n"
			, __func__, g_dlpt_stop);
	} else {
		for (i = 0; i < DLPT_NUM; i++) {
			if (dlpt_cb_tb[i].dlpt_cb != NULL) {
				dlpt_callback = dlpt_cb_tb[i].dlpt_cb;
				if (dlpt_callback != NULL)
					dlpt_callback(g_dlpt_val);
				PMICLOG("[%s] g_dlpt_val=%d\n", __func__,
					g_dlpt_val);
			}
		}
	}
}

int get_rac_val(void)
{
	int volt_1 = 0;
	int volt_2 = 0;
	int curr_1 = 0;
	int curr_2 = 0;
	int rac_cal = 0;
	int ret = 0;
	bool retry_state = false;
	int retry_count = 0;

	do {
		/*adc and fg------------------------------------------ */
		do_ptim(true);

		pmic_spm_crit2("[1,Trigger ADC PTIM mode] volt1=%d, curr_1=%d\n"
			       , ptim_bat_vol, ptim_R_curr);
		volt_1 = ptim_bat_vol;
		curr_1 = ptim_R_curr;

		pmic_spm_crit2("[2,enable dummy load]");
		enable_dummy_load(1);
		mdelay(50);
		/*Wait ----------------------------------------------- */

		/*adc and fg------------------------------------------ */
		do_ptim(true);

		pmic_spm_crit2("[3,Trig. ADC PTIM mode] volt2= %d, curr_2= %d\n"
			       , ptim_bat_vol, ptim_R_curr);
		volt_2 = ptim_bat_vol;
		curr_2 = ptim_R_curr;

		/*Disable dummy load----------------------------------- */
		enable_dummy_load(0);

		/*Calculate Rac---------------------------------------- */
		if ((curr_2 - curr_1) >= 700 && (curr_2 - curr_1) <= 1200
		    && (volt_1 - volt_2) >= 80 && (volt_1 - volt_2) <= 2000) {
			/*40.0mA */
			/*m-ohm */
			rac_cal = ((volt_1 - volt_2) * 1000) / (curr_2 -
								curr_1);

			if (rac_cal < 0)
				ret = (rac_cal - (rac_cal * 2)) * 1;
			else
				ret = rac_cal * 1;

		} else {
			ret = -1;
			pmic_spm_crit2("[4, Cal.Rac] (curr_x-curr_y) < 40mA\n");
		}

		pmic_spm_crit2("volt_1= %d,volt_2= %d,curr_1= %d,curr_2= %d\n"
		     , volt_1, volt_2, curr_1, curr_2);

		pmic_spm_crit2("rac_cal= %d,ret= %d,retry_count= %d\n"
		     , rac_cal, ret, retry_count);

		pmic_spm_crit2(" %d,%d,%d,%d,%d,%d,%d\n"
			       , volt_1, volt_2, curr_1, curr_2
			       , rac_cal, ret, retry_count);


		/*------------------------*/
		retry_count++;

		if ((retry_count < 3) && (ret == -1))
			retry_state = true;
		else
			retry_state = false;

	} while (retry_state == true);

	return ret;
}

int get_dlpt_imix_spm(void)
{
#if defined(CONFIG_MTK_SMART_BATTERY)
	int rac_val[5], rac_val_avg;
#if 0
	int volt[5], curr[5];
	int volt_avg = 0, curr_avg = 0;
	int imix;
#endif
	int i;
	static unsigned int pre_ui_soc = 101;
	unsigned int ui_soc;

	ui_soc = bat_get_ui_percentage();

	if (ui_soc != pre_ui_soc) {
		pre_ui_soc = ui_soc;
	} else {
		pmic_spm_crit2("[dlpt_R] pre_SOC=%d SOC=%d skip\n", pre_ui_soc,
			       ui_soc);
		return 0;
	}


	for (i = 0; i < 2; i++) {
		rac_val[i] = get_rac_val();
		if (rac_val[i] == -1)
			return -1;
	}

	/*rac_val_avg=rac_val[0]+rac_val[1]+rac_val[2]+rac_val[3]+rac_val[4]; */
	/*rac_val_avg=rac_val_avg/5; */
	/*PMICLOG("[dlpt_R] %d,%d,%d,%d,%d %d\n"
	 * ,rac_val[0],rac_val[1],rac_val[2],rac_val[3],rac_val[4],rac_val_avg);
	 */

	rac_val_avg = rac_val[0] + rac_val[1];
	rac_val_avg = rac_val_avg / 2;
	/*pmic_spm_crit2("[dlpt_R] %d,%d,%d\n"
	 *, rac_val[0], rac_val[1], rac_val_avg);
	 */
	pr_info("[dlpt_R] %d,%d,%d\n", rac_val[0], rac_val[1], rac_val_avg);

	if (rac_val_avg > 100)
		ptim_rac_val_avg = rac_val_avg;

#if 0
	for (i = 0; i < 5; i++) {
		do_ptim();
		volt[i] = ptim_bat_vol;
		curr[i] = ptim_R_curr;
		volt_avg += ptim_bat_vol;
		curr_avg += ptim_R_curr;
	}

	volt_avg = volt_avg / 5;
	curr_avg = curr_avg / 5;
	imix = curr_avg + (volt_avg - g_lbatInt1) * 1000 / ptim_rac_val_avg;

	pmic_spm_crit2("[dlpt_Imix] %d,%d,%d,%d,%d,%d,%d\n"
		       , volt_avg, curr_avg, g_lbatInt1
		       , ptim_rac_val_avg, imix, BMT_status.SOC
		       , bat_get_ui_percentage());

	ptim_imix = imix;
#endif
#endif		/* end of #if defined(CONFIG_MTK_SMART_BATTERY) */
	return 0;
}

#if DLPT_SORT_IMIX_VOLT_CURR
void pmic_swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

static void pmic_quicksort(int *data, int left, int right)
{
	int pivot, i, j;

	if (left >= right)
		return;

	pivot = data[left];

	i = left + 1;
	j = right;

	while (1) {
		while (i <= right) {
			if (data[i] > pivot)
				break;
			i = i + 1;
		}

		while (j > left) {
			if (data[j] < pivot)
				break;
			j = j - 1;
		}

		if (i > j)
			break;

		pmic_swap(&data[i], &data[j]);
	}

	pmic_swap(&data[left], &data[j]);

	pmic_quicksort(data, left, j - 1);
	pmic_quicksort(data, j + 1, right);
}
#endif				/* end of DLPT_SORT_IMIX_VOLT_CURR */

int get_dlpt_imix(void)
{
	/* int rac_val[5], rac_val_avg; */
	int volt[5], curr[5], volt_avg = 0, curr_avg = 0;
	int imix;
#if 0				/* debug only */
	int val, val1, val2, val3, val4, val5, ret_val;
#endif
	int i, count_do_ptim = 0;

	for (i = 0; i < 5; i++) {
		/*adc and fg---------------------------------------------- */
		/* do_ptim(false); */
		while (do_ptim(false)) {
			if ((count_do_ptim >= 2) && (count_do_ptim < 4))
				pr_info("do_ptim more than twice times\n");
			else if (count_do_ptim > 3) {
				pr_info("do_ptim more than five times\n");
				WARN_ON(1);
				break;
			} else
				;
			count_do_ptim++;
		}

		volt[i] = ptim_bat_vol;
		curr[i] = ptim_R_curr;
#if !DLPT_SORT_IMIX_VOLT_CURR
		volt_avg += ptim_bat_vol;
		curr_avg += ptim_R_curr;
#endif
		PMICLOG("[%s:%d] %d,%d,%d,%d\n"
			, __func__
			, i, volt[i], curr[i], volt_avg, curr_avg);
	}

#if !DLPT_SORT_IMIX_VOLT_CURR
	volt_avg = volt_avg / 5;
	curr_avg = curr_avg / 5;
#else
	pmic_quicksort(volt, 0, 4);
	pmic_quicksort(curr, 0, 4);
	volt_avg = volt[1] + volt[2] + volt[3];
	curr_avg = curr[1] + curr[2] + curr[3];
	volt_avg = volt_avg / 3;
	curr_avg = curr_avg / 3;
#endif				/* end of DLPT_SORT_IMIX_VOLT_CURR */
	imix = (curr_avg + (volt_avg - g_lbatInt1) * 1000 / ptim_rac_val_avg)
						/ 10;

#if defined(CONFIG_MTK_SMART_BATTERY)
	PMICLOG("[%s] %d,%d,%d,%d,%d,%d,%d\n"
		, __func__
		, volt_avg, curr_avg, g_lbatInt1
		, ptim_rac_val_avg, imix, battery_get_soc()
		, bat_get_ui_percentage());
#endif

	ptim_imix = imix;

	return ptim_imix;
}



int get_dlpt_imix_charging(void)
{

	int zcv_val = 0;
	int vsys_min_1_val = DLPT_VOLT_MIN;
	int imix_val = 0;

	zcv_val = pmic_get_auxadc_value(AUXADC_LIST_BATADC);

	imix_val = (zcv_val - vsys_min_1_val) * 1000 /
				ptim_rac_val_avg * 9 / 10;
	PMICLOG("[dlpt] %s %d %d %d %d\n"
		, __func__
		, imix_val, zcv_val
		, vsys_min_1_val, ptim_rac_val_avg);

	return imix_val;
}


int dlpt_check_power_off(void)
{
	int ret = 0;

	ret = 0;
	if (g_dlpt_start == 0 || g_low_battery_stop == 1) {
		PMICLOG("[%s] not start\n", __func__);
	} else {
#ifdef LOW_BATTERY_PROTECT
		if (g_low_battery_level == 2 && g_lowbat_int_bottom == 1) {
			/*1st time receive battery voltage < 3.1V, record it */
			if (g_low_battery_if_power_off == 0) {
				g_low_battery_if_power_off++;
				pr_info("[%s] %d\n"
					, __func__
					, g_low_battery_if_power_off);
			} else {
				/* 2nd time receive battery voltage < 3.1V */
				/* Wait FG to call power off */
				ret = 1;
				pr_info("[%s] %d %d\n"
					, __func__
					, ret, g_low_battery_if_power_off);
			}
		} else {
			ret = 0;
			/* battery voltage > 3.1V, ignore it */
			g_low_battery_if_power_off = 0;
		}
#endif

		PMICLOG("[%s]", __func__);
		PMICLOG("ptim_imix=%d, POWEROFF_BAT_CURRENT=%d"
			, ptim_imix, POWEROFF_BAT_CURRENT);
#ifdef LOW_BATTERY_PROTECT
		PMICLOG("g_low_battery_level=%d,ret=%d,g_lowbat_int_bottom=%d\n"
			, g_low_battery_level, ret, g_lowbat_int_bottom);
#endif
	}

	return ret;
}

int dlpt_notify_handler(void *unused)
{
	ktime_t ktime;
	int pre_ui_soc = 0;
	int cur_ui_soc = 0;
	int diff_ui_soc = 1;

#if defined(CONFIG_MTK_SMART_BATTERY)
	pre_ui_soc = bat_get_ui_percentage();
#endif
	cur_ui_soc = pre_ui_soc;

	do {
#if defined(CONFIG_MACH_MT6757) || \
	defined(CONFIG_MACH_MT6759) || \
	defined(CONFIG_MACH_MT6758)
#if defined(CONFIG_MTK_BASE_POWER)
		if (dpidle_active_status())
			ktime = ktime_set(20, 0); /* light-loading mode */
		else
#endif
#endif
			ktime = ktime_set(10, 0); /* normal mode */

		wait_event_interruptible(dlpt_notify_waiter,
					 (dlpt_notify_flag == true));

		__pm_stay_awake(&dlpt_notify_lock);
		mutex_lock(&dlpt_notify_mutex);
		/*---------------------------------*/

#if defined(CONFIG_MTK_SMART_BATTERY)
		cur_ui_soc = bat_get_ui_percentage();
#endif

		if (cur_ui_soc <= 1) {
			g_low_per_timer += 10;
			if (g_low_per_timer > g_low_per_timeout_val)
				g_low_per_timer = 0;
			PMICLOG("[DLPT] g_low_per_timer=%d\n"
				, g_low_per_timer);
			PMICLOG("[DLPT] g_low_per_timeout_val%d\n"
				, g_low_per_timeout_val);
		} else {
			g_low_per_timer = 0;
		}

		PMICLOG("[%s] %d %d %d %d %d\n"
			, __func__
			, pre_ui_soc, cur_ui_soc
			, g_imix_val, g_low_per_timer
			, g_low_per_timeout_val);
		{

			PMICLOG("[DLPT] is running\n");
			if (ptim_rac_val_avg == 0)
				pr_info("[DLPT] ptim_rac_val_avg=0 , skip\n");
			else {
				if (upmu_get_rgs_chrdet())
					g_imix_val = get_dlpt_imix_charging();
				else
					g_imix_val = get_dlpt_imix();

				/*Notify */
				if (g_imix_val >= 1) {
					if (g_imix_val > IMAX_MAX_VALUE)
						g_imix_val = IMAX_MAX_VALUE;
					exec_dlpt_callback(g_imix_val);
				} else {
					exec_dlpt_callback(1);
					PMICLOG("[DLPT] return 1\n");
				}
				pre_ui_soc = cur_ui_soc;

				pr_info("[DLPT_final] %d,%d,%d,%d,%d\n",
					g_imix_val, pre_ui_soc,
					cur_ui_soc, diff_ui_soc,
					IMAX_MAX_VALUE);
			}
		}

		g_dlpt_start = 1;
		dlpt_notify_flag = false;

		#if defined(CONFIG_MTK_SMART_BATTERY)
		#if (CONFIG_MTK_GAUGE_VERSION == 30)
			if (cur_ui_soc <= DLPT_POWER_OFF_THD) {
				static unsigned char cnt = 0xff;

				if (cnt == 0xff)
					cnt = 0;

				if (dlpt_check_power_off() == 1) {
					/* notify battery driver
					 * to power off by SOC=0
					 */
					set_shutdown_cond(DLPT_SHUTDOWN);
					cnt++;
					pr_info("[DLPT_POWER_OFF_EN] notify SOC=0 to power off , cnt=%d\n"
						, cnt);

					if (cnt >= 4)
						kernel_restart(
							"DLPT reboot system");

				} else {
					cnt = 0;
				}
			} else {
				pr_info("[DLPT_POWER_OFF_EN] disable(%d)\n"
					, cur_ui_soc);
			}
		#endif
		#endif

		/*---------------------------------*/
		mutex_unlock(&dlpt_notify_mutex);
		__pm_relax(&dlpt_notify_lock);

		hrtimer_start(&dlpt_notify_timer, ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart dlpt_notify_task(struct hrtimer *timer)
{
	dlpt_notify_flag = true;
	wake_up_interruptible(&dlpt_notify_waiter);
	PMICLOG("%s is called\n", __func__);

	return HRTIMER_NORESTART;
}

void dlpt_notify_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(30, 0);
	hrtimer_init(&dlpt_notify_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dlpt_notify_timer.function = dlpt_notify_task;
	hrtimer_start(&dlpt_notify_timer, ktime, HRTIMER_MODE_REL);

	dlpt_notify_thread = kthread_run(dlpt_notify_handler, 0
					 , "dlpt_notify_thread");
	if (IS_ERR(dlpt_notify_thread))
		pr_info("Failed to create dlpt_notify_thread\n");
	else
		pr_info("Create dlpt_notify_thread : done\n");

	pmic_set_register_value(PMIC_RG_UVLO_VTHL, 0);

	/*re-init UVLO volt */
	switch (POWER_UVLO_VOLT_LEVEL) {
	case 2500:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 0);
		break;
	case 2550:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 1);
		break;
	case 2600:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 2);
		break;
	case 2650:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 3);
		break;
	case 2700:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 4);
		break;
	case 2750:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 5);
		break;
	case 2800:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 6);
		break;
	case 2850:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 7);
		break;
	case 2900:
		pmic_set_register_value(PMIC_RG_UVLO_VTHL, 8);
		break;
	default:
		PMICLOG("Invalid value(%d)\n", POWER_UVLO_VOLT_LEVEL);
		break;
	}
	pr_info("POWER_UVLO_VOLT_LEVEL=%d, [0x%x]=0x%x\n",
	       POWER_UVLO_VOLT_LEVEL, PMIC_RG_UVLO_VTHL_ADDR,
	       upmu_get_reg_value(PMIC_RG_UVLO_VTHL_ADDR));
}

#else
int get_dlpt_imix_spm(void) { return 1; }

int get_rac(void) { return 0; }

int get_imix(void) { return 0; }

int do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur)
{
	return 0;
}

#endif				/*#ifdef DLPT_FEATURE_SUPPORT */

#ifdef LOW_BATTERY_PROTECT
/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t show_low_battery_protect_ut(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_low_battery_level=%d\n", __func__, g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_ut(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 2) {
			pr_info("[%s] your input is %d\n", __func__, val);
			exec_low_battery_callback(val);
		} else {
			pr_info("[%s] wrong number (%d)\n", __func__, val);
		}
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_ut, 0664
		, show_low_battery_protect_ut, store_low_battery_protect_ut);

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t show_low_battery_protect_stop(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_low_battery_stop=%d\n", __func__, g_low_battery_stop);
	return sprintf(buf, "%u\n", g_low_battery_stop);
}

static ssize_t store_low_battery_protect_stop(struct device *dev
		, struct device_attribute *attr
		, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_low_battery_stop = val;
		pr_info("[%s] g_low_battery_stop=%d\n"
			, __func__, g_low_battery_stop);
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_stop, 0664
		   , show_low_battery_protect_stop
		   , store_low_battery_protect_stop);

/*
 * low battery protect level
 */
static ssize_t show_low_battery_protect_level(struct device *dev
				, struct device_attribute *attr, char *buf)
{
	PMICLOG("[%s] g_low_battery_level=%d\n"
		, __func__, g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_level(struct device *dev
				, struct device_attribute *attr
				, const char *buf, size_t size)
{
	PMICLOG("[%s] g_low_battery_level=%d\n"
		, __func__, g_low_battery_level);

	return size;
}

static DEVICE_ATTR(low_battery_protect_level, 0664
		, show_low_battery_protect_level
		, store_low_battery_protect_level);
#endif

#ifdef BATTERY_OC_PROTECT
/*****************************************************************************
 * battery OC protect UT
 ******************************************************************************/
static ssize_t show_battery_oc_protect_ut(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_battery_oc_level=%d\n"
		, __func__, g_battery_oc_level);
	return sprintf(buf, "%u\n", g_battery_oc_level);
}

static ssize_t store_battery_oc_protect_ut(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 1) {
			pr_info("[%s] your input is %d\n", __func__, val);
			exec_battery_oc_callback(val);
		} else {
			pr_info("[%s] wrong number (%d)\n", __func__, val);
		}
	}
	return size;
}

static DEVICE_ATTR(battery_oc_protect_ut, 0664
		, show_battery_oc_protect_ut, store_battery_oc_protect_ut);

/*****************************************************************************
 * battery OC protect stop
 ******************************************************************************/
static ssize_t show_battery_oc_protect_stop(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_battery_oc_stop=%d\n"
		, __func__, g_battery_oc_stop);
	return sprintf(buf, "%u\n", g_battery_oc_stop);
}

static ssize_t store_battery_oc_protect_stop(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_info("[_protect_stop]\n");

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_battery_oc_stop = val;
		pr_info("[%s] g_battery_oc_stop=%d\n"
			, __func__, g_battery_oc_stop);
	}
	return size;
}

static DEVICE_ATTR(battery_oc_protect_stop, 0664
		, show_battery_oc_protect_stop, store_battery_oc_protect_stop);

/*****************************************************************************
 * battery OC protect level
 ******************************************************************************/
static ssize_t show_battery_oc_protect_level(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_battery_oc_level=%d\n"
		, __func__, g_battery_oc_level);
	return sprintf(buf, "%u\n", g_battery_oc_level);
}

static ssize_t store_battery_oc_protect_level(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	PMICLOG("[%s] g_battery_oc_level=%d\n"
		, __func__, g_battery_oc_level);

	return size;
}

static DEVICE_ATTR(battery_oc_protect_level, 0664
		, show_battery_oc_protect_level
		, store_battery_oc_protect_level);
#endif

#ifdef BATTERY_PERCENT_PROTECT
/*****************************************************************************
 * battery percent protect UT
 ******************************************************************************/
static ssize_t show_battery_percent_ut(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	/*show_battery_percent_protect_ut */
	PMICLOG("[%s] g_battery_percent_level=%d\n"
		, __func__, g_battery_percent_level);
	return sprintf(buf, "%u\n", g_battery_percent_level);
}

static ssize_t store_battery_percent_ut(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;
	/*store_battery_percent_protect_ut */
	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 1) {
			pr_info("[%s] your input is %d\n", __func__, val);
			exec_battery_percent_callback(val);
		} else {
			pr_info("[%s] wrong number (%d)\n", __func__, val);
		}
	}
	return size;
}

static DEVICE_ATTR(battery_percent_protect_ut, 0664
		, show_battery_percent_ut, store_battery_percent_ut);

/*
 * battery percent protect stop
 ******************************************************************************/
static ssize_t show_battery_percent_stop(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	/*show_battery_percent_protect_stop */
	PMICLOG("[%s] g_battery_percent_stop=%d\n"
		, __func__, g_battery_percent_stop);
	return sprintf(buf, "%u\n", g_battery_percent_stop);
}

static ssize_t store_battery_percent_stop(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;
	/*store_battery_percent_protect_stop */
	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_battery_percent_stop = val;
		pr_info("[%s] g_battery_percent_stop=%d\n"
			, __func__, g_battery_percent_stop);
	}
	return size;
}

static DEVICE_ATTR(battery_percent_protect_stop, 0664
		   , show_battery_percent_stop, store_battery_percent_stop);

/*
 * battery percent protect level
 ******************************************************************************/
static ssize_t show_battery_percent_level(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	/*show_battery_percent_protect_level */
	PMICLOG("[%s] g_battery_percent_level=%d\n"
			, __func__, g_battery_percent_level);
	return sprintf(buf, "%u\n", g_battery_percent_level);
}

static ssize_t store_battery_percent_level(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t size)
{
	/*store_battery_percent_protect_level */
	PMICLOG("[%s] g_battery_percent_level=%d\n"
		, __func__, g_battery_percent_level);

	return size;
}

static DEVICE_ATTR(battery_percent_protect_level, 0664
		   , show_battery_percent_level, store_battery_percent_level);
#endif

#ifdef DLPT_FEATURE_SUPPORT
/*****************************************************************************
 * DLPT UT
 ******************************************************************************/
static ssize_t show_dlpt_ut(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	PMICLOG("[%s] g_dlpt_val=%d\n", __func__, g_dlpt_val);
	return sprintf(buf, "%u\n", g_dlpt_val);
}

static ssize_t store_dlpt_ut(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int val = 0;
	int ret = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__
			, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 10); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 10, (unsigned int *)&val);

		pr_info("[%s] your input is %d\n"
			, __func__
			, val);
		exec_dlpt_callback(val);
	}
	return size;
}

static DEVICE_ATTR(dlpt_ut, 0664, show_dlpt_ut, store_dlpt_ut);	/*664 */

/*****************************************************************************
 * DLPT stop
 ******************************************************************************/
static ssize_t show_dlpt_stop(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[%s] g_dlpt_stop=%d\n", __func__, g_dlpt_stop);
	return sprintf(buf, "%u\n", g_dlpt_stop);
}

static ssize_t store_dlpt_stop(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	char *pvalue = NULL;
	unsigned int val = 0;
	int ret = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n"
			, __func__
			, buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16); */
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_dlpt_stop = val;
		pr_info("[%s] g_dlpt_stop=%d\n"
			, __func__
			, g_dlpt_stop);
	}
	return size;
}

static DEVICE_ATTR(dlpt_stop, 0664, show_dlpt_stop, store_dlpt_stop);	/*664 */

/*****************************************************************************
 * DLPT level
 ******************************************************************************/
static ssize_t show_dlpt_level(
		struct device *dev, struct device_attribute *attr,
		char *buf)
{
	PMICLOG("[%s] g_dlpt_val=%d\n", __func__, g_dlpt_val);
	return sprintf(buf, "%u\n", g_dlpt_val);
}

static ssize_t store_dlpt_level(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	PMICLOG("[%s] g_dlpt_val=%d\n", __func__, g_dlpt_val);

	return size;
}

static DEVICE_ATTR(dlpt_level, 0664, show_dlpt_level, store_dlpt_level);
#endif

/*****************************************************************************
 * Low battery call back function
 ******************************************************************************/
void bat_h_int_handler(void)
{
	g_lowbat_int_bottom = 0;

	PMICLOG("[%s]....\n", __func__);

	/*sub-task */
#ifdef LOW_BATTERY_PROTECT
	g_low_battery_level = 0;
	exec_low_battery_callback(LOW_BATTERY_LEVEL_0);

#if 0
	lbat_max_en_setting(0);
	mdelay(1);
	lbat_min_en_setting(1);
#else
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN, BAT_LV_1_THD);

	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
	mdelay(1);
	lbat_min_en_setting(1);
#endif

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
	       PMIC_AUXADC_LBAT_VOLT_MAX_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
	       PMIC_AUXADC_LBAT_VOLT_MIN_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
	       PMIC_RG_INT_EN_BAT_L_ADDR,
	       upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR));
#endif

}

void bat_l_int_handler(void)
{
	PMICLOG("[%s]....\n", __func__);

	/*sub-task */
#ifdef LOW_BATTERY_PROTECT
	g_low_battery_level++;
	if (g_low_battery_level > 2)
		g_low_battery_level = 2;

	if (g_low_battery_level == 1)
		exec_low_battery_callback(LOW_BATTERY_LEVEL_1);
	else if (g_low_battery_level == 2) {
		exec_low_battery_callback(LOW_BATTERY_LEVEL_2);
		g_lowbat_int_bottom = 1;
	} else
		PMICLOG("[%s]err,g_low_battery_level=%d\n"
			, __func__
			, g_low_battery_level);

#if 0
	lbat_min_en_setting(0);
	mdelay(1);
	lbat_max_en_setting(1);
#else

	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN, BAT_LV_2_THD);

	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
	mdelay(1);
	if (g_low_battery_level < 2)
		lbat_min_en_setting(1);
	lbat_max_en_setting(1);
#endif

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
	       PMIC_AUXADC_LBAT_VOLT_MAX_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
	       PMIC_AUXADC_LBAT_VOLT_MIN_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
	       PMIC_RG_INT_EN_BAT_L_ADDR,
	       upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR));
#endif

}

/*****************************************************************************
 * Battery OC call back function
 ******************************************************************************/

void fg_cur_h_int_handler(void)
{
	PMICLOG("[%s]....\n", __func__);

	/*sub-task */
#ifdef BATTERY_OC_PROTECT
	g_battery_oc_level = 0;
	exec_battery_oc_callback(BATTERY_OC_LEVEL_0);
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);
	bat_oc_l_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR,
		upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR,
		upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT0_H_ADDR,
		upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));
#endif
}

void fg_cur_l_int_handler(void)
{
	PMICLOG("[%s]....\n", __func__);

	/*sub-task */
#ifdef BATTERY_OC_PROTECT
	g_battery_oc_level = 1;
	exec_battery_oc_callback(BATTERY_OC_LEVEL_1);
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);
	bat_oc_h_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR,
		upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR,
		upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT0_H_ADDR,
		upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));
#endif
}

/*****************************************************************************
 * system function
 ******************************************************************************/
#ifdef DLPT_FEATURE_SUPPORT
static unsigned long pmic_node;

static int fb_early_init_dt_get_chosen(
		unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 ||
	   (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;
	pmic_node = node;
	return 1;
}
#endif				/*end of #ifdef DLPT_FEATURE_SUPPORT */

#if 0				/*argus TBD */
static int proc_utilization_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_utilization_show, NULL);
}

static const struct file_operations low_battery_protect_ut_fops = {
	.open = proc_utilization_open,
	.read = seq_read,
};
#endif
void pmic_throttling_dlpt_suspend(void)
{
#ifdef LOW_BATTERY_PROTECT
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
	       PMIC_AUXADC_LBAT_VOLT_MAX_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
	       PMIC_AUXADC_LBAT_VOLT_MIN_ADDR,
	       upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
	       PMIC_RG_INT_EN_BAT_L_ADDR,
	       upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR));
#endif

#ifdef BATTERY_OC_PROTECT
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT0_H_ADDR,
		upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));
#endif
}

void pmic_throttling_dlpt_resume(void)
{
#ifdef LOW_BATTERY_PROTECT
	lbat_min_en_setting(0);
	lbat_max_en_setting(0);
	mdelay(1);

	if (g_low_battery_level == 1) {
		lbat_min_en_setting(1);
		lbat_max_en_setting(1);
	} else if (g_low_battery_level == 2) {
		/*lbat_min_en_setting(0); */
		lbat_max_en_setting(1);
	} else {		/*0 */
		lbat_min_en_setting(1);
		/*lbat_max_en_setting(0); */
	}

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n"
		, PMIC_AUXADC_LBAT_VOLT_MAX_ADDR
		, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR)
		, PMIC_AUXADC_LBAT_VOLT_MIN_ADDR
		, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR)
		, PMIC_RG_INT_EN_BAT_L_ADDR
		, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR));
#endif

#ifdef BATTERY_OC_PROTECT
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);

	if (g_battery_oc_level == 1)
		bat_oc_h_en_setting(1);
	else
		bat_oc_l_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n"
		, PMIC_FG_CUR_HTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR)
		, PMIC_FG_CUR_LTH_ADDR
		, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR)
		, PMIC_RG_INT_EN_FG_BAT0_H_ADDR
		, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT0_H_ADDR));
#endif
}

void pmic_throttling_dlpt_debug_init(struct platform_device *dev
				     , struct dentry *debug_dir)
{
#if defined(DLPT_FEATURE_SUPPORT) || defined(LOW_BATTERY_PROTECT) || \
	defined(BATTERY_OC_PROTECT) || defined(BATTERY_PERCENT_PROTECT)
	int ret_device_file = 0;
#endif

#if 0	/* argus TBD */
	struct dentry *mt_pmic_dir = debug_dir;
#endif

#ifdef LOW_BATTERY_PROTECT
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_low_battery_protect_ut);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_low_battery_protect_stop);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_low_battery_protect_level);
#endif

#ifdef BATTERY_OC_PROTECT
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_oc_protect_ut);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_oc_protect_stop);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_oc_protect_level);
#endif

#ifdef BATTERY_PERCENT_PROTECT
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_percent_protect_ut);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_percent_protect_stop);
	ret_device_file = device_create_file(&(dev->dev)
				, &dev_attr_battery_percent_protect_level);
#endif

#ifdef DLPT_FEATURE_SUPPORT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_level);
#endif

#if 0				/* argus TBD */
	debugfs_create_file("low_battery_protect_ut"
			, 0644
			, mt_pmic_dir, NULL, &low_battery_protect_ut_fops);
	debugfs_create_file("low_battery_protect_stop"
			, 0644
			, mt_pmic_dir, NULL, &low_battery_protect_stop_fops);
	debugfs_create_file("low_battery_protect_level"
			, 0644
			, mt_pmic_dir, NULL, &low_battery_protect_level_fops);
	debugfs_create_file("battery_oc_protect_ut"
			, 0644
			, mt_pmic_dir, NULL, &battery_oc_protect_ut_fops);
	debugfs_create_file("battery_oc_protect_stop"
			, 0644
			, mt_pmic_dir, NULL
			, &battery_oc_protect_stop_fops);
	debugfs_create_file("battery_oc_protect_level"
			, 0644
			, mt_pmic_dir, NULL
			, &battery_oc_protect_level_fops);
	debugfs_create_file("battery_percent_protect_ut"
			, 0644
			, mt_pmic_dir, NULL
			, &battery_percent_protect_ut_fops);
	debugfs_create_file("battery_percent_protect_stop"
			, 0644
			, mt_pmic_dir, NULL
			, &battery_percent_protect_stop_fops);
	debugfs_create_file("battery_percent_protect_level"
			, 0644
			, mt_pmic_dir, NULL
			, &battery_percent_protect_level_fops);
	debugfs_create_file("dlpt_ut"
			, 0644
			, mt_pmic_dir, NULL, &dlpt_ut_fops);
	debugfs_create_file("dlpt_stop"
			, 0644
			, mt_pmic_dir, NULL, &dlpt_stop_fops);
	debugfs_create_file("dlpt_level"
			, 0644
			, mt_pmic_dir, NULL, &dlpt_level_fops);
	PMICLOG("proc_create pmic_debug_throttling_dlpt_fops\n");
#endif
}

int pmic_throttling_dlpt_init(void)
{
#if defined(CONFIG_MTK_SMART_BATTERY)
	struct device_node *np;
	u32 val;
	char *path;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
	path = "/bat_gm30";
	np = of_find_node_by_path(path);
	if (of_property_read_u32(np, "CAR_TUNE_VALUE", &val) == 0) {
		fg_cust_data.car_tune_value = (int)val*10;
		pr_info("Get car_tune_value from DT: %d\n"
			, fg_cust_data.car_tune_value);
	} else {
		fg_cust_data.car_tune_value = CAR_TUNE_VALUE*10;
		pr_info("Get default car_tune_value= %d\n"
			, fg_cust_data.car_tune_value);
	}
	if (of_property_read_u32(np, "R_FG_VALUE", &val) == 0) {
		fg_cust_data.r_fg_value = (int)val*10;
		pr_info("Get r_fg_value from DT: %d\n"
			, fg_cust_data.r_fg_value);
	} else {
		fg_cust_data.r_fg_value = R_FG_VALUE*10;
		pr_info("Get default r_fg_value= %d\n"
			, fg_cust_data.r_fg_value);
	}
	pr_info("Get default UNIT_FGCURRENT= %d\n", UNIT_FGCURRENT);
#else
	path = "/bus/BAT_METTER";
	np = of_find_node_by_path(path);
	if (of_property_read_u32(np, "car_tune_value", &val) == 0) {
		batt_meter_cust_data.car_tune_value = (int)val;
		PMICLOG("Get car_tune_value from DT: %d\n"
			, batt_meter_cust_data.car_tune_value);
	} else {
		batt_meter_cust_data.car_tune_value = CAR_TUNE_VALUE;
		PMICLOG("Get car_tune_value from cust header\n");
	}
#endif
#endif

	wakeup_source_init(&ptim_wake_lock, "PTIM_wakelock");
	mutex_init(&ptim_mutex);
	wakeup_source_init(&bat_percent_notify_lock
			   , "bat_percent_notify_lock wakelock");
	wakeup_source_init(&dlpt_notify_lock
			   , "dlpt_notify_lock wakelock");

#ifdef LOW_BATTERY_PROTECT
	low_battery_protect_init();
#else
	pr_info("[PMIC] no define LOW_BATTERY_PROTECT\n");
#endif

#ifdef BATTERY_OC_PROTECT
	battery_oc_protect_init();
#else
	pr_info("[PMIC] no define BATTERY_OC_PROTECT\n");
#endif

#ifdef BATTERY_PERCENT_PROTECT
	bat_percent_notify_init();
#else
	pr_info("[PMIC] no define BATTERY_PERCENT_PROTECT\n");
#endif

#ifdef DLPT_FEATURE_SUPPORT
	dlpt_notify_init();
#else
	pr_info("[PMIC] no define DLPT_FEATURE_SUPPORT\n");
#endif

#if PMIC_THROTTLING_DLPT_UT	/* UT TBD */
#ifdef LOW_BATTERY_PROTECT
	register_low_battery_notify(&low_bat_test, LOW_BATTERY_PRIO_CPU_B);
	PMICLOG("register_low_battery_notify:done\n");
#endif
#ifdef BATTERY_OC_PROTECT
	register_battery_oc_notify(&bat_oc_test, BATTERY_OC_PRIO_CPU_B);
	PMICLOG("register_battery_oc_notify:done\n");
#endif
#ifdef BATTERY_PERCENT_PROTECT
	register_battery_percent_notify(&bat_per_test
					, BATTERY_PERCENT_PRIO_CPU_B);
	PMICLOG("register_battery_percent_notify:done\n");
#endif
#endif
	return 0;
}

static int __init pmic_throttling_dlpt_rac_init(void)
{
#ifdef DLPT_FEATURE_SUPPORT
	const int *pimix = NULL;
	int len = 0;

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		pimix = of_get_flat_dt_prop(pmic_node, "atag,imix_r", &len);
	if (pimix == NULL) {
		pr_notice(" pimix == NULL len = %d\n", len);
	} else {
		pr_info(" pimix = %d\n", *pimix);
		ptim_rac_val_avg = *pimix;
	}

	PMICLOG("******** MT pmic driver probe!! ********%d\n"
		, ptim_rac_val_avg);
#endif /* #ifdef DLPT_FEATURE_SUPPORT */
	return 0;
}

fs_initcall(pmic_throttling_dlpt_rac_init);

MODULE_AUTHOR("Argus Lin");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
