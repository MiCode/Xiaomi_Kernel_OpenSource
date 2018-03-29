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
#include <linux/uaccess.h>

#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_throttling_dlpt.h"
#include "include/pmic_irq.h"
/*#include <mach/eint.h> TBD*/
#include <mach/mt_pmic_wrap.h>
#if defined CONFIG_MTK_LEGACY
#include <mt-plat/mt_gpio.h>
#endif
#include <mt-plat/mtk_rtc.h>
#include <mach/mt_spm_mtcmos.h>

#include <linux/time.h>

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
/*****************************************************************************
 * PMIC related define
 ******************************************************************************/
#define PMIC_THROTTLING_DLPT_UT 0
/*****************************************************************************
 * PMIC read/write APIs
 ******************************************************************************/

#if PMIC_THROTTLING_DLPT_UT
/* UT test code TBD */
void low_bat_test(LOW_BATTERY_LEVEL level_val)
{
	pr_err("[low_bat_test] get %d\n", level_val);
}

void bat_oc_test(BATTERY_OC_LEVEL level_val)
{
	pr_err("[bat_oc_test] get %d\n", level_val);
}

void bat_per_test(BATTERY_PERCENT_LEVEL level_val)
{
	pr_err("[bat_per_test] get %d\n", level_val);
}
#endif

/*****************************************************************************
 * Low battery call back function
 ******************************************************************************/
#define LBCB_NUM 16

#ifndef DISABLE_LOW_BATTERY_PROTECT
#define LOW_BATTERY_PROTECT
#endif

int g_lowbat_int_bottom = 0;

#ifdef LOW_BATTERY_PROTECT
#if 0 /* fast stress test lbat_H/L on phone, phone UT only */ /*PMIC_THROTTLING_DLPT_UT*/
/* ex. 3400/5400*4096*/
#define BAT_HV_THD   (POWER_INT0_VOLT*4096/5400)	/*ex: 3400mV*/
#define BAT_LV_1_THD (4200*4096/5400)	/*ex: fake, 4200mV to trigger lbat */
#define BAT_LV_2_THD (4200*4096/5400)	/*ex: fake, 4200mV to trigger lbat */
#else
/* ex. 3400/5400*4096*/
#define BAT_HV_THD   (POWER_INT0_VOLT*4096/5400)	/*ex: 3400mV*/
#define BAT_LV_1_THD (POWER_INT1_VOLT*4096/5400)	/*ex: 3250mV*/
#define BAT_LV_2_THD (POWER_INT2_VOLT*4096/5400)	/*ex: 3000mV*/
#endif
int g_low_battery_level = 0;
int g_low_battery_stop = 0;
/*give one change to ignore DLPT power off. battery voltage may return to 3.25 or higher
because loading become light. */
int g_low_battery_if_power_off = 0;

struct low_battery_callback_table {
	void *lbcb;
};

struct low_battery_callback_table lbcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif
void (*low_battery_callback)(LOW_BATTERY_LEVEL);

void register_low_battery_notify(void (*low_battery_callback) (LOW_BATTERY_LEVEL),
				 LOW_BATTERY_PRIO prio_val)
{
#ifdef LOW_BATTERY_PROTECT
	PMICLOG("[register_low_battery_notify] start\n");

	lbcb_tb[prio_val].lbcb = low_battery_callback;

	pr_err("[register_low_battery_notify] prio_val=%d\n", prio_val);
#endif /*end of #ifdef LOW_BATTERY_PROTECT */
}
#ifdef LOW_BATTERY_PROTECT
void exec_low_battery_callback(LOW_BATTERY_LEVEL low_battery_level)
{				/*0:no limit */
	int i = 0;

	if (g_low_battery_stop == 1) {
		pr_err("[exec_low_battery_callback] g_low_battery_stop=%d\n", g_low_battery_stop);
	} else {
		pr_debug("[exec_low_battery_callback] prio_val=%d,low_battery=%d\n", i, low_battery_level);
		for (i = 0; i < LBCB_NUM; i++) {
			if (lbcb_tb[i].lbcb != NULL) {
				low_battery_callback = lbcb_tb[i].lbcb;
				low_battery_callback(low_battery_level);
				/*
					pr_debug
					    ("[exec_low_battery_callback] prio_val=%d,low_battery=%d\n",
					     i, low_battery_level);
					     */
			}
		}
	}
}

void lbat_min_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN, en_val);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_L, en_val);
}

void lbat_max_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX, en_val);
	pmic_set_register_value(PMIC_RG_INT_EN_BAT_H, en_val);
}

void low_battery_protect_init(void)
{
	/*default setting */
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN, 0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX, 0);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0, 1);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DET_PRD_19_16, 0);

	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX, BAT_HV_THD);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN, BAT_LV_1_THD);

	lbat_min_en_setting(1);
	lbat_max_en_setting(0);

	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_AUXADC_LBAT_VOLT_MAX_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
		PMIC_AUXADC_LBAT_VOLT_MIN_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
		PMIC_RG_INT_EN_BAT_L_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR)
	    );
	pr_err("[low_battery_protect_init] %d mV, %d mV, %d mV\n",
		POWER_INT0_VOLT, POWER_INT1_VOLT, POWER_INT2_VOLT);
	PMICLOG("[low_battery_protect_init] Done\n");

}

#endif				/*#ifdef LOW_BATTERY_PROTECT*/

/*****************************************************************************
 * Battery OC call back function
 ******************************************************************************/
#define OCCB_NUM 16

#ifndef DISABLE_BATTERY_OC_PROTECT
#define BATTERY_OC_PROTECT
#endif

#ifdef BATTERY_OC_PROTECT
/* ex. Ireg = 65535 - (I * 950000uA / 2 / 158.122 / CAR_TUNE_VALUE * 100)*/
/* (950000/2/158.122)*100~=300400*/

#if defined(CONFIG_MTK_SMART_BATTERY)
#define BAT_OC_H_THD   \
(65535-((300400*POWER_BAT_OC_CURRENT_H/1000)/batt_meter_cust_data.car_tune_value))	/*ex: 4670mA*/
#define BAT_OC_L_THD   \
(65535-((300400*POWER_BAT_OC_CURRENT_L/1000)/batt_meter_cust_data.car_tune_value))	/*ex: 5500mA*/

#define BAT_OC_H_THD_RE   \
(65535-((300400*POWER_BAT_OC_CURRENT_H_RE/1000)/batt_meter_cust_data.car_tune_value))	/*ex: 3400mA*/
#define BAT_OC_L_THD_RE   \
(65535-((300400*POWER_BAT_OC_CURRENT_L_RE/1000)/batt_meter_cust_data.car_tune_value))	/*ex: 4000mA*/
#else
#define BAT_OC_H_THD   0xc047
#define BAT_OC_L_THD   0xb4f4
#define BAT_OC_H_THD_RE  0xc047
#define BAT_OC_L_THD_RE   0xb4f4
#endif /* end of #if defined(CONFIG_MTK_SMART_BATTERY) */
int g_battery_oc_level = 0;
int g_battery_oc_stop = 0;

struct battery_oc_callback_table {
	void *occb;
};

struct battery_oc_callback_table occb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif /*end of #ifdef BATTERY_OC_PROTECT*/
void (*battery_oc_callback)(BATTERY_OC_LEVEL);

void register_battery_oc_notify(void (*battery_oc_callback) (BATTERY_OC_LEVEL),
				BATTERY_OC_PRIO prio_val)
{
#ifdef BATTERY_OC_PROTECT
	PMICLOG("[register_battery_oc_notify] start\n");

	occb_tb[prio_val].occb = battery_oc_callback;

	pr_err("[register_battery_oc_notify] prio_val=%d\n", prio_val);
#endif
}

void exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level)
{				/*0:no limit */
#ifdef BATTERY_OC_PROTECT
	int i = 0;

	if (g_battery_oc_stop == 1) {
		pr_err("[exec_battery_oc_callback] g_battery_oc_stop=%d\n", g_battery_oc_stop);
	} else {
		for (i = 0; i < OCCB_NUM; i++) {
			if (occb_tb[i].occb != NULL) {
				battery_oc_callback = occb_tb[i].occb;
				battery_oc_callback(battery_oc_level);
				pr_err
				    ("[exec_battery_oc_callback] prio_val=%d,battery_oc_level=%d\n",
				     i, battery_oc_level);
			}
		}
	}
#endif
}
#ifdef BATTERY_OC_PROTECT
void bat_oc_h_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_RG_INT_EN_FG_CUR_H, en_val);
	/* mt6325_upmu_set_rg_int_en_fg_cur_h(en_val); */
}

void bat_oc_l_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_RG_INT_EN_FG_CUR_L, en_val);
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

	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );

	pr_err("[battery_oc_protect_init] %d mA, %d mA\n",
		POWER_BAT_OC_CURRENT_H, POWER_BAT_OC_CURRENT_L);
	PMICLOG("[battery_oc_protect_init] Done\n");
}

void battery_oc_protect_reinit(void)
{
#ifdef BATTERY_OC_PROTECT
	pmic_set_register_value(PMIC_FG_CUR_HTH, BAT_OC_H_THD_RE);
	/*mt6325_upmu_set_fg_cur_hth(BAT_OC_H_THD_RE); */
	pmic_set_register_value(PMIC_FG_CUR_LTH, BAT_OC_L_THD_RE);
	/*mt6325_upmu_set_fg_cur_lth(BAT_OC_L_THD_RE); */

	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );

	pr_err("[battery_oc_protect_reinit] %d mA, %d mA\n",
		POWER_BAT_OC_CURRENT_H_RE, POWER_BAT_OC_CURRENT_L_RE);
	pr_err("[battery_oc_protect_reinit] Done\n");
#else
	pr_warn("[battery_oc_protect_reinit] no define BATTERY_OC_PROTECT\n");
#endif
}
#endif /* #ifdef BATTERY_OC_PROTECT */


/*****************************************************************************
 * 15% notify service
 ******************************************************************************/
#ifndef DISABLE_BATTERY_PERCENT_PROTECT
#define BATTERY_PERCENT_PROTECT
#endif

#ifdef BATTERY_PERCENT_PROTECT
static struct hrtimer bat_percent_notify_timer;
static struct task_struct *bat_percent_notify_thread;
static bool bat_percent_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
#endif
#if !defined CONFIG_HAS_WAKELOCKS
struct wakeup_source bat_percent_notify_lock;
#else
struct wake_lock bat_percent_notify_lock;
#endif

static DEFINE_MUTEX(bat_percent_notify_mutex);

#ifdef BATTERY_PERCENT_PROTECT
/*extern unsigned int bat_get_ui_percentage(void);*/

#define BPCB_NUM 16

int g_battery_percent_level = 0;
int g_battery_percent_stop = 0;

#define BAT_PERCENT_LINIT 15

struct battery_percent_callback_table {
	void *bpcb;
};

struct battery_percent_callback_table bpcb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};
#endif /* end of #ifdef BATTERY_PERCENT_PROTECT */
void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL);

void register_battery_percent_notify(void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL),
				     BATTERY_PERCENT_PRIO prio_val)
{
#ifdef BATTERY_PERCENT_PROTECT
	PMICLOG("[register_battery_percent_notify] start\n");

	bpcb_tb[prio_val].bpcb = battery_percent_callback;

	pr_err("[register_battery_percent_notify] prio_val=%d\n", prio_val);

	if ((g_battery_percent_stop == 0) && (g_battery_percent_level == 1)) {
#ifdef DISABLE_DLPT_FEATURE
		pr_err("[register_battery_percent_notify] level l happen\n");
		battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
#else
		if (prio_val == BATTERY_PERCENT_PRIO_FLASHLIGHT) {
			pr_err("[register_battery_percent_notify at DLPT] level l happen\n");
			battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
		}
#endif
	}
#endif /* end of #ifdef BATTERY_PERCENT_PROTECT */
}

#ifdef BATTERY_PERCENT_PROTECT
void exec_battery_percent_callback(BATTERY_PERCENT_LEVEL battery_percent_level)
{				/*0:no limit */
#ifdef DISABLE_DLPT_FEATURE
	int i = 0;
#endif

	if (g_battery_percent_stop == 1) {
		pr_err("[exec_battery_percent_callback] g_battery_percent_stop=%d\n",
			g_battery_percent_stop);
	} else {
#ifdef DISABLE_DLPT_FEATURE
		for (i = 0; i < BPCB_NUM; i++) {
			if (bpcb_tb[i].bpcb != NULL) {
				battery_percent_callback = bpcb_tb[i].bpcb;
				battery_percent_callback(battery_percent_level);
				pr_err
				    ("[exec_battery_percent_callback] prio_val=%d,battery_percent_level=%d\n",
				     i, battery_percent_level);
			}
		}
#else
		battery_percent_callback = bpcb_tb[BATTERY_PERCENT_PRIO_FLASHLIGHT].bpcb;
		battery_percent_callback(battery_percent_level);
		pr_err
		    ("[exec_battery_percent_callback at DLPT] prio_val=%d,battery_percent_level=%d\n",
		     BATTERY_PERCENT_PRIO_FLASHLIGHT, battery_percent_level);
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

#if !defined CONFIG_HAS_WAKELOCKS
		__pm_stay_awake(&bat_percent_notify_lock);
#else
		wake_lock(&bat_percent_notify_lock);
#endif
		mutex_lock(&bat_percent_notify_mutex);

#if defined(CONFIG_MTK_SMART_BATTERY)
		bat_per_val = bat_get_ui_percentage();
#endif
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if ((upmu_get_rgs_chrdet() == 0)
			&& (g_battery_percent_level == 0)
				&& (bat_per_val <= BAT_PERCENT_LINIT)) {
#else
		if ((g_battery_percent_level == 0) && (bat_per_val <= BAT_PERCENT_LINIT)) {
#endif
				g_battery_percent_level = 1;
				exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_1);
		} else if ((g_battery_percent_level == 1) && (bat_per_val > BAT_PERCENT_LINIT)) {
				g_battery_percent_level = 0;
				exec_battery_percent_callback(BATTERY_PERCENT_LEVEL_0);
		} else {
		}
		bat_percent_notify_flag = false;

		PMICLOG("bat_per_level=%d,bat_per_val=%d\n", g_battery_percent_level, bat_per_val);

		mutex_unlock(&bat_percent_notify_mutex);
#if !defined CONFIG_HAS_WAKELOCKS
		__pm_relax(&bat_percent_notify_lock);
#else
		wake_unlock(&bat_percent_notify_lock);
#endif

		hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart bat_percent_notify_task(struct hrtimer *timer)
{
	bat_percent_notify_flag = true;
	wake_up_interruptible(&bat_percent_notify_waiter);
	PMICLOG("bat_percent_notify_task is called\n");

	return HRTIMER_NORESTART;
}

void bat_percent_notify_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(20, 0);
	hrtimer_init(&bat_percent_notify_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bat_percent_notify_timer.function = bat_percent_notify_task;
	hrtimer_start(&bat_percent_notify_timer, ktime, HRTIMER_MODE_REL);

	bat_percent_notify_thread =
	    kthread_run(bat_percent_notify_handler, 0, "bat_percent_notify_thread");
	if (IS_ERR(bat_percent_notify_thread))
		pr_err("Failed to create bat_percent_notify_thread\n");
	else
		pr_err("Create bat_percent_notify_thread : done\n");
}
#endif /* #ifdef BATTERY_PERCENT_PROTECT */

/*****************************************************************************
 * DLPT service
 ******************************************************************************/
#ifndef DISABLE_DLPT_FEATURE
#define DLPT_FEATURE_SUPPORT
#endif

#ifdef DLPT_FEATURE_SUPPORT

unsigned int ptim_bat_vol = 0;
signed int ptim_R_curr = 0;
int ptim_imix = 0;
int ptim_rac_val_avg = 0;

signed int pmic_ptimretest = 0;



unsigned int ptim_cnt = 0;

signed int count_time_out_adc_imp = 36;
unsigned int count_adc_imp = 0;

int do_ptim(bool isSuspend)
{
	unsigned int vbat_reg;
	int ret = 0;

	count_adc_imp = 0;
	/*PMICLOG("[do_ptim] start\n"); */
	if (isSuspend == false)
		pmic_auxadc_lock();

	/*MT6353 only */
	pmic_set_register_value(PMIC_RG_ADCIN_VBAT_EN, 1);

	pmic_set_register_value(PMIC_AUXADC_SPL_NUM_LARGE, 0x0006);

	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_PRD, 6);
	pmic_set_register_value(PMIC_CLK_AUXADC_SMPS_CK_PDN, 0);
	pmic_set_register_value(PMIC_CLK_AUXADC_SMPS_CK_PDN_HWEN, 0);

	pmic_set_register_value(PMIC_CLK_AUXADC_CK_PDN_HWEN, 0);
	pmic_set_register_value(PMIC_CLK_AUXADC_CK_PDN, 0);

	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 1);

	/*restore to initial state */
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 0);

	/*set issue interrupt */
	/*pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,1); */

#if defined(SWCHR_POWER_PATH)
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CHSEL, 1);
#else
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CHSEL, 0);
#endif
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_CNT, 3);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE, 1);

/*      MT6351_PMICLOG("[do_ptim] end %d %d\n",pmic_get_register_value
(MT6351_PMIC_RG_AUXADC_SMPS_CK_PDN),pmic_get_register_value(
MT6351_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN));*/

	while (pmic_get_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_STATUS) == 0) {
		/*MT6351_PMICLOG("[do_ptim] MT6351_PMIC_AUXADC_IMPEDANCE_IRQ_STATUS= %d\n",
		   pmic_get_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_STATUS)); */
		if ((count_adc_imp++) > count_time_out_adc_imp) {
			pr_err("do_ptim over %d times/ms\n", count_adc_imp);
			ret = 1;
			break;
		}
		mdelay(1);
	}

	/*disable */
	pmic_set_register_value(PMIC_AUXADC_IMP_AUTORPT_EN, 0);	/*typo */
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_MODE, 0);

	/*clear irq */
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 1);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 1);
	pmic_set_register_value(PMIC_AUXADC_CLR_IMP_CNT_STOP, 0);
	pmic_set_register_value(PMIC_AUXADC_IMPEDANCE_IRQ_CLR, 0);

	/*MT6353 only */
	pmic_set_register_value(PMIC_RG_ADCIN_VBAT_EN, 0);

	if (isSuspend == false)
		pmic_auxadc_unlock();
	/*PMICLOG("[do_ptim2] 0xee8=0x%x  0x2c6=0x%x\n", upmu_get_reg_value
	(0xee8),upmu_get_reg_value(0x2c6));*/


	/*pmic_set_register_value(PMIC_RG_INT_STATUS_AUXADC_IMP,1);write 1 to clear ! */
	/*pmic_set_register_value(PMIC_RG_INT_EN_AUXADC_IMP,0); */


	vbat_reg = pmic_get_register_value(PMIC_AUXADC_ADC_OUT_IMP_AVG);
	ptim_bat_vol = (vbat_reg * 3 * 18000) / 32768;

#if defined(CONFIG_MTK_SMART_BATTERY)
	fgauge_read_IM_current((void *)&ptim_R_curr);
#endif
	return ret;
}


#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
/* for jade minus */
void enable_dummy_load(unsigned int en)
{

	if (en == 1) {
		/*1. disable isink pdn */
		pmic_set_register_value(PMIC_CLK_DRV_ISINK3_CK_PDN, 0);
		pmic_set_register_value(PMIC_CLK_DRV_ISINK2_CK_PDN, 0);

		pmic_set_register_value(PMIC_CLK_DRV_32K_CK_PDN, 0);

		/*1. disable isink pdn */
		pmic_set_register_value(PMIC_CLK_DRV_CHRIND_CK_PDN, 0);

		/* enable isink step */
		pmic_set_register_value(PMIC_ISINK_CH2_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH3_STEP, 0x7);

		/* enable isink double */
		pmic_set_register_value(PMIC_RG_ISINK2_DOUBLE_EN, 1);
		pmic_set_register_value(PMIC_RG_ISINK3_DOUBLE_EN, 1);

		/*enable isink */
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0x1);
		/*PMICLOG("[enable dummy load]\n"); */
	} else {
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0);

		/* disable isink double */
		pmic_set_register_value(PMIC_RG_ISINK2_DOUBLE_EN, 0);
		pmic_set_register_value(PMIC_RG_ISINK3_DOUBLE_EN, 0);

		/*1. enable isink pdn */
		pmic_set_register_value(PMIC_CLK_DRV_ISINK3_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_CLK_DRV_ISINK2_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_CLK_DRV_32K_CK_PDN, 0x1);

		/*1. enable isink pdn */
		pmic_set_register_value(PMIC_CLK_DRV_CHRIND_CK_PDN, 0x1);

		/*PMICLOG("[disable dummy load]\n"); */
	}

}
#else
/* for jade/everest/olympus */
void enable_dummy_load(unsigned int en)
{

	if (en == 1) {
		/*1. disable isink pdn */
		pmic_set_register_value(PMIC_RG_DRV_ISINK3_CK_PDN, 0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK2_CK_PDN, 0);
		/*
		pmic_set_register_value(PMIC_RG_DRV_ISINK1_CK_PDN, 0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK0_CK_PDN, 0);
		*/
		pmic_set_register_value(PMIC_RG_DRV_32K_CK_PDN, 0);

		/*1. disable isink pdn */
		pmic_set_register_value(PMIC_RG_DRV_CHRIND_CK_PDN, 0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK7_CK_PDN, 0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK6_CK_PDN, 0);
		/*
		pmic_set_register_value(PMIC_RG_DRV_ISINK5_CK_PDN, 0);
		pmic_set_register_value(PMIC_RG_DRV_ISINK4_CK_PDN, 0);
		*/

		/* enable isink step */
		pmic_set_register_value(PMIC_ISINK_CH2_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH3_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH6_STEP, 0x7);
		pmic_set_register_value(PMIC_ISINK_CH7_STEP, 0x7);

		/*enable isink */
		pmic_set_register_value(PMIC_ISINK_CH7_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH6_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP7_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP6_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH7_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH6_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0x1);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0x1);
		/*PMICLOG("[enable dummy load]\n"); */
	} else {
		/*upmu_set_reg_value(0x828,0x0cc0); */
#if defined MT6328
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0);
#endif
		pmic_set_register_value(PMIC_ISINK_CH7_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH6_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH3_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP7_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP6_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP3_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CHOP2_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH7_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH6_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH3_BIAS_EN, 0);
		pmic_set_register_value(PMIC_ISINK_CH2_BIAS_EN, 0);

		/*1. enable isink pdn */
		pmic_set_register_value(PMIC_RG_DRV_ISINK3_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_RG_DRV_ISINK2_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_RG_DRV_32K_CK_PDN, 0x1);

		/*1. enable isink pdn */
		pmic_set_register_value(PMIC_RG_DRV_CHRIND_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_RG_DRV_ISINK7_CK_PDN, 0x1);
		pmic_set_register_value(PMIC_RG_DRV_ISINK6_CK_PDN, 0x1);

		/*pmic_set_register_value(PMIC_RG_VIBR_EN,0); */
		/*PMICLOG("[disable dummy load]\n"); */
	}

}
#endif /* End of #if defined (CONFIG_MTK_PMIC_CHIP_MT6353) */

#endif /* #ifdef DLPT_FEATURE_SUPPORT */

#ifdef DLPT_FEATURE_SUPPORT
static struct hrtimer dlpt_notify_timer;
static struct task_struct *dlpt_notify_thread;
static bool dlpt_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(dlpt_notify_waiter);
#endif
#if !defined CONFIG_HAS_WAKELOCKS
struct wakeup_source dlpt_notify_lock;
#else
struct wake_lock dlpt_notify_lock;
#endif
static DEFINE_MUTEX(dlpt_notify_mutex);

#ifdef DLPT_FEATURE_SUPPORT
#define DLPT_NUM 16
/* This define is used to filter smallest and largest voltage and current.
To avoid auxadc measurement interference issue. */
#define DLPT_SORT_IMIX_VOLT_CURR 1


int g_dlpt_stop = 0;
unsigned int g_dlpt_val = 0;

int g_dlpt_start = 0;


int g_imix_val = 0;
int g_imix_val_pre = 0;
int g_low_per_timer = 0;
int g_low_per_timeout_val = 60;


int g_lbatInt1 = POWER_INT2_VOLT * 10;

struct dlpt_callback_table {
	void *dlpt_cb;
};

struct dlpt_callback_table dlpt_cb_tb[] = {
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL},
	{NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}, {NULL}
};

void (*dlpt_callback)(unsigned int);

void register_dlpt_notify(void (*dlpt_callback) (unsigned int), DLPT_PRIO prio_val)
{
	PMICLOG("[register_dlpt_notify] start\n");

	dlpt_cb_tb[prio_val].dlpt_cb = dlpt_callback;

	pr_err("[register_dlpt_notify] prio_val=%d\n", prio_val);

	if ((g_dlpt_stop == 0) && (g_dlpt_val != 0)) {
		pr_err("[register_dlpt_notify] dlpt happen\n");
		dlpt_callback(g_dlpt_val);
	}
}

void exec_dlpt_callback(unsigned int dlpt_val)
{
	int i = 0;

	g_dlpt_val = dlpt_val;

	if (g_dlpt_stop == 1) {
		pr_err("[exec_dlpt_callback] g_dlpt_stop=%d\n", g_dlpt_stop);
	} else {
		for (i = 0; i < DLPT_NUM; i++) {
			if (dlpt_cb_tb[i].dlpt_cb != NULL) {
				dlpt_callback = dlpt_cb_tb[i].dlpt_cb;
				dlpt_callback(g_dlpt_val);
				pr_debug("[exec_dlpt_callback] g_dlpt_val=%d\n", g_dlpt_val);
			}
		}
	}
}

/*
int get_dlpt_iavg(int is_use_zcv)
{
	int bat_cap_val = 0;
	int zcv_val = 0;
	int vsys_min_2_val = POWER_INT2_VOLT;
	int rbat_val = 0;
	int rdc_val = 0;
	int iavg_val = 0;

	bat_cap_val = bat_get_ui_percentage();

	if(is_use_zcv == 1)
		zcv_val = fgauge_read_v_by_d(100-bat_cap_val);
	else
	{
	#if defined(SWCHR_POWER_PATH)
		zcv_val = PMIC_IMM_GetOneChannelValue(MT6351_PMIC_AUX_ISENSE_AP,5,1);
	#else
		zcv_val = PMIC_IMM_GetOneChannelValue(MT6351_PMIC_AUX_BATSNS_AP,5,1);
	#endif
	}
	rbat_val = fgauge_read_r_bat_by_v(zcv_val);
	rdc_val = CUST_R_FG_OFFSET+R_FG_VALUE+rbat_val;

	if(rdc_val==0)
		rdc_val=1;
	iavg_val = ((zcv_val-vsys_min_2_val)*1000)/rdc_val;

	return iavg_val;
}

int get_real_volt(int val)*/ /*0.1mV*/
/*
{
    int ret = 0;
    ret = val&0x7FFF;
    ret = (ret*4*1800*10)/32768;
    return ret;
}

int get_real_curr(int val)*/ /*0.1mA*/
/*
{

	int ret=0;

	if ( val > 32767 ) {
		ret = val-65535;
		ret = ret-(ret*2);
	} else
		ret = val;
	ret = ret*158122;
	do_div(ret, 100000);
	ret = (ret*20)/R_FG_VALUE;
	ret = ((ret*CAR_TUNE_VALUE)/100);

	return ret;
}
*/
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
		/*adc and fg-------------------------------------------------------- */
		do_ptim(true);

		pmic_spm_crit2("[1,Trigger ADC PTIM mode] volt1=%d, curr_1=%d\n", ptim_bat_vol,
			       ptim_R_curr);
		volt_1 = ptim_bat_vol;
		curr_1 = ptim_R_curr;

		pmic_spm_crit2("[2,enable dummy load]");
		enable_dummy_load(1);
		mdelay(50);
		/*Wait --------------------------------------------------------------*/

		/*adc and fg-------------------------------------------------------- */
		do_ptim(true);

		pmic_spm_crit2("[3,Trigger ADC PTIM mode again] volt2=%d, curr_2=%d\n",
			       ptim_bat_vol, ptim_R_curr);
		volt_2 = ptim_bat_vol;
		curr_2 = ptim_R_curr;

		/*Disable dummy load-------------------------------------------------*/
		enable_dummy_load(0);

		/*Calculate Rac------------------------------------------------------ */
		if ((curr_2 - curr_1) >= 700 && (curr_2 - curr_1) <= 1200
		    && (volt_1 - volt_2) >= 80) {
			/*40.0mA */
			rac_cal = ((volt_1 - volt_2) * 1000) / (curr_2 - curr_1);	/*m-ohm */

			if (rac_cal < 0)
				ret = (rac_cal - (rac_cal * 2)) * 1;
			else
				ret = rac_cal * 1;

		} else {
			ret = -1;
			pmic_spm_crit2("[4,Calculate Rac] bypass due to (curr_x-curr_y) < 40mA\n");
		}

		pmic_spm_crit2
		    ("volt_1 = %d,volt_2 = %d,curr_1 = %d,curr_2 = %d,rac_cal = %d,ret = %d,retry_count = %d\n",
		     volt_1, volt_2, curr_1, curr_2, rac_cal, ret, retry_count);

		pmic_spm_crit2(" %d,%d,%d,%d,%d,%d,%d\n",
			       volt_1, volt_2, curr_1, curr_2, rac_cal, ret, retry_count);


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
		pmic_spm_crit2("[dlpt_R] pre_SOC=%d SOC=%d skip\n", pre_ui_soc, ui_soc);
		return 0;
	}


	for (i = 0; i < 2; i++) {
		rac_val[i] = get_rac_val();
		if (rac_val[i] == -1)
			return -1;
	}

	/*rac_val_avg=rac_val[0]+rac_val[1]+rac_val[2]+rac_val[3]+rac_val[4];*/
	/*rac_val_avg=rac_val_avg/5;*/
	/*PMICLOG("[dlpt_R] %d,%d,%d,%d,%d %d\n",rac_val[0],rac_val[1],rac_val[2],rac_val[3],rac_val[4],rac_val_avg);*/

	rac_val_avg = rac_val[0] + rac_val[1];
	rac_val_avg = rac_val_avg / 2;
	pmic_spm_crit2("[dlpt_R] %d,%d,%d\n", rac_val[0], rac_val[1], rac_val_avg);

	if (rac_val_avg > 100)
		ptim_rac_val_avg = rac_val_avg;

/*
	for(i=0;i<5;i++)
	{
		do_ptim();

		volt[i]=ptim_bat_vol;
		curr[i]=ptim_R_curr;
		volt_avg+=ptim_bat_vol;
		curr_avg+=ptim_R_curr;
	}

	volt_avg=volt_avg/5;
	curr_avg=curr_avg/5;

	imix=curr_avg+(volt_avg-g_lbatInt1)*1000/ptim_rac_val_avg;

		pmic_spm_crit2("[dlpt_Imix] %d,%d,%d,%d,%d,%d,%d\n",volt_avg,curr_avg,g_lbatInt1,
		ptim_rac_val_avg,imix,BMT_status.SOC,bat_get_ui_percentage());

	ptim_imix=imix;
*/
#endif /* end of #if defined(CONFIG_MTK_SMART_BATTERY) */
	return 0;

}

#if DLPT_SORT_IMIX_VOLT_CURR
void pmic_swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}


void pmic_quicksort(int *data, int left, int right)
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
#endif /* end of DLPT_SORT_IMIX_VOLT_CURR*/
int get_dlpt_imix(void)
{
	/* int rac_val[5], rac_val_avg; */
	int volt[5], curr[5], volt_avg = 0, curr_avg = 0;
	int imix;
#if 0 /* debug only */
	int val, val1, val2, val3, val4, val5, ret_val;
#endif
	int i, count_do_ptim = 0;

	for (i = 0; i < 5; i++) {
		/*adc and fg-------------------------------------------------------- */
		/* do_ptim(false); */
		while (do_ptim(false)) {
			if ((count_do_ptim >= 2) && (count_do_ptim < 4))
				pr_err("do_ptim more than twice times\n");
			else if (count_do_ptim > 3) {
				pr_err("do_ptim more than five times\n");
				BUG_ON(1);
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
	PMICLOG("[get_dlpt_imix:%d] %d,%d,%d,%d\n", i, volt[i], curr[i], volt_avg, curr_avg);
#if 0 /* debug only */
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_ADC29), (&val), (0xffff), 0);
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_ADC30), (&val1), (0xffff), 0);
	ret_val = pmic_read_interface((unsigned int)(MT6353_FGADC_CON25), (&val2), (0xffff), 0);
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_IMP0), (&val3), (0xffff), 0);
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_CON2), (&val4), (0xffff), 0);
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_LBAT1), (&val5), (0xffff), 0);
	pr_debug("[get_dlpt_imix after ptim]MT6353_AUXADC_ADC29 0x%x:0x%x, MT6353_AUXADC_ADC30 0x%x:0x%x, MT6353_FGADC_CON25 0x%x:0x%x\n",
		MT6353_AUXADC_ADC29, val, MT6353_AUXADC_ADC30, val1, MT6353_FGADC_CON25, val2);
	pr_debug("[get_dlpt_imix after ptim]MT6353_AUXADC_IMP0 0x%x:0x%x, MT6353_AUXADC_CON2 0x%x:0x%x, MT6353_AUXADC_LBAT1 0x%x:0x%x\n",
		MT6353_AUXADC_IMP0, val3, MT6353_AUXADC_CON2, val4, MT6353_AUXADC_LBAT1, val5);
	ret_val = pmic_read_interface((unsigned int)(MT6353_AUXADC_LBAT2), (&val), (0xffff), 0);
	pr_debug("[get_dlpt_imix after ptim]MT6353_AUXADC_LBAT2 0x%x:0x%x\n",
		MT6353_AUXADC_LBAT2, val);
#endif
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
#endif /* end of DLPT_SORT_IMIX_VOLT_CURR*/
	imix = (curr_avg + (volt_avg - g_lbatInt1) * 1000 / ptim_rac_val_avg) / 10;

#if defined(CONFIG_MTK_SMART_BATTERY)
	pr_debug("[get_dlpt_imix] %d,%d,%d,%d,%d,%d,%d\n", volt_avg, curr_avg, g_lbatInt1,
		ptim_rac_val_avg, imix, BMT_status.SOC, bat_get_ui_percentage());
#endif

	ptim_imix = imix;

	return ptim_imix;

}



int get_dlpt_imix_charging(void)
{

	int zcv_val = 0;
	int vsys_min_1_val = POWER_INT2_VOLT;
	int imix_val = 0;
#if defined(SWCHR_POWER_PATH)
	zcv_val = PMIC_IMM_GetOneChannelValue(PMIC_AUX_ISENSE_AP, 5, 1);
#else
	zcv_val = PMIC_IMM_GetOneChannelValue(PMIC_AUX_BATSNS_AP, 5, 1);
#endif

	imix_val = (zcv_val - vsys_min_1_val) * 1000 / ptim_rac_val_avg * 9 / 10;
	PMICLOG("[dlpt] get_dlpt_imix_charging %d %d %d %d\n",
		imix_val, zcv_val, vsys_min_1_val, ptim_rac_val_avg);

	return imix_val;
}


int dlpt_check_power_off(void)
{
	int ret = 0;

	ret = 0;
	if (g_dlpt_start == 0) {
		PMICLOG("[dlpt_check_power_off] not start\n");
	} else {
		if (g_low_battery_level == 2 && g_lowbat_int_bottom == 1) {
			/*1st time receive battery voltage < 3.1V, record it */
			if (g_low_battery_if_power_off == 0) {
				g_low_battery_if_power_off++;
				pr_err("[dlpt_check_power_off] %d\n", g_low_battery_if_power_off);
			} else {
				/*2nd time receive battery voltage < 3.1V, wait FG to call power off */
				ret = 1;
				pr_err("[dlpt_check_power_off] %d %d\n", ret, g_low_battery_if_power_off);
			}
		} else {
			ret = 0;
			/* battery voltage > 3.1V, ignore it */
			g_low_battery_if_power_off = 0;
		}

		PMICLOG("[dlpt_check_power_off]");
		PMICLOG("ptim_imix=%d, POWEROFF_BAT_CURRENT=%d", ptim_imix, POWEROFF_BAT_CURRENT);
		PMICLOG(" g_low_battery_level=%d,ret=%d,g_lowbat_int_bottom=%d\n", g_low_battery_level, ret,
			g_lowbat_int_bottom);
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
		ktime = ktime_set(10, 0);

		wait_event_interruptible(dlpt_notify_waiter, (dlpt_notify_flag == true));

#if !defined CONFIG_HAS_WAKELOCKS
		__pm_stay_awake(&dlpt_notify_lock);
#else
		wake_lock(&dlpt_notify_lock);
#endif
		mutex_lock(&dlpt_notify_mutex);
		/*---------------------------------*/

#if defined(CONFIG_MTK_SMART_BATTERY)
		cur_ui_soc = bat_get_ui_percentage();
#endif

		if (cur_ui_soc <= 1) {
			g_low_per_timer += 10;
			if (g_low_per_timer > g_low_per_timeout_val)
				g_low_per_timer = 0;
			PMICLOG("[DLPT] g_low_per_timer=%d,g_low_per_timeout_val=%d\n",
				g_low_per_timer, g_low_per_timeout_val);
		} else {
			g_low_per_timer = 0;
		}

		PMICLOG("[dlpt_notify_handler] %d %d %d %d %d\n", pre_ui_soc, cur_ui_soc,
			g_imix_val, g_low_per_timer, g_low_per_timeout_val);
/* can enable for ricky's request for >1% battery change TBD
	if( ((pre_ui_soc-cur_ui_soc)>=diff_ui_soc) ||
	    ((cur_ui_soc-pre_ui_soc)>=diff_ui_soc) ||
	    (g_imix_val == 0)                      ||
	    (g_imix_val == -1)                     ||
	    ( (cur_ui_soc <= 1) && (g_low_per_timer >= g_low_per_timeout_val) )

	  )
*/
		{

			PMICLOG("[DLPT] is running\n");
			if (ptim_rac_val_avg == 0)
				pr_err("[DLPT] ptim_rac_val_avg=0 , skip\n");
			else {

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
				if (upmu_get_rgs_chrdet()) {
					g_imix_val = get_dlpt_imix_charging();
				} else {
#else
				{
#endif
					g_imix_val = get_dlpt_imix();

/*
	if(g_imix_val != -1) {
		if(g_imix_val_pre <= 0)
			g_imix_val_pre=g_imix_val;

				if(g_imix_val > g_imix_val_pre) {
					PMICLOG("[DLPT] g_imix_val=%d,g_imix_val_pre=%d\n", g_imix_val, g_imix_val_pre);
					g_imix_val=g_imix_val_pre;
				}
				else
					g_imix_val_pre=g_imix_val;
				}
*/
				}

				/*Notify*/
				if (g_imix_val >= 1) {
					if (g_imix_val > IMAX_MAX_VALUE)
						g_imix_val = IMAX_MAX_VALUE;
					exec_dlpt_callback(g_imix_val);
				} else {
					exec_dlpt_callback(1);
					PMICLOG("[DLPT] return 1\n");
				}

				pre_ui_soc = cur_ui_soc;

				pr_err("[DLPT_final] %d,%d,%d,%d,%d,%d\n",
					g_imix_val, g_imix_val_pre, pre_ui_soc, cur_ui_soc,
					diff_ui_soc, IMAX_MAX_VALUE);
			}

		}

		g_dlpt_start = 1;
		dlpt_notify_flag = false;

		/*---------------------------------*/
		mutex_unlock(&dlpt_notify_mutex);
#if !defined CONFIG_HAS_WAKELOCKS
		__pm_relax(&dlpt_notify_lock);
#else
		wake_unlock(&dlpt_notify_lock);
#endif

		hrtimer_start(&dlpt_notify_timer, ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

enum hrtimer_restart dlpt_notify_task(struct hrtimer *timer)
{
	dlpt_notify_flag = true;
	wake_up_interruptible(&dlpt_notify_waiter);
	PMICLOG("dlpt_notify_task is called\n");

	return HRTIMER_NORESTART;
}

int get_system_loading_ma(void)
{
	int fg_val = 0;

	if (g_dlpt_start == 0)
		PMICLOG("get_system_loading_ma not ready\n");
	else {
#if defined(CONFIG_MTK_SMART_BATTERY)
		fg_val = battery_meter_get_battery_current();
		fg_val = fg_val / 10;
		if (battery_meter_get_battery_current_sign() == 1)
			fg_val = 0 - fg_val;	/* charging*/
		PMICLOG("[get_system_loading_ma] fg_val=%d\n", fg_val);
#endif
	}

	return fg_val;
}


void dlpt_notify_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(30, 0);
	hrtimer_init(&dlpt_notify_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dlpt_notify_timer.function = dlpt_notify_task;
	hrtimer_start(&dlpt_notify_timer, ktime, HRTIMER_MODE_REL);

	dlpt_notify_thread = kthread_run(dlpt_notify_handler, 0, "dlpt_notify_thread");
	if (IS_ERR(dlpt_notify_thread))
		pr_err("Failed to create dlpt_notify_thread\n");
	else
		pr_err("Create dlpt_notify_thread : done\n");

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
	pr_err("POWER_UVLO_VOLT_LEVEL=%d, [0x%x]=0x%x\n",
		POWER_UVLO_VOLT_LEVEL, PMIC_RG_UVLO_VH_LAT_ADDR, upmu_get_reg_value(PMIC_RG_UVLO_VH_LAT_ADDR));
}

#else
int get_dlpt_imix_spm(void)
{
	return 1;
}


#endif				/*#ifdef DLPT_FEATURE_SUPPORT */

#ifdef LOW_BATTERY_PROTECT
/*****************************************************************************
 * low battery protect UT
 ******************************************************************************/
static ssize_t show_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	PMICLOG("[show_low_battery_protect_ut] g_low_battery_level=%d\n", g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_ut(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_err("[store_low_battery_protect_ut]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_low_battery_protect_ut] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 2) {
			pr_err("[store_low_battery_protect_ut] your input is %d\n", val);
			exec_low_battery_callback(val);
		} else {
			pr_err("[store_low_battery_protect_ut] wrong number (%d)\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_ut, 0664, show_low_battery_protect_ut, store_low_battery_protect_ut);	/*664*/

/*****************************************************************************
 * low battery protect stop
 ******************************************************************************/
static ssize_t show_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	PMICLOG("[show_low_battery_protect_stop] g_low_battery_stop=%d\n", g_low_battery_stop);
	return sprintf(buf, "%u\n", g_low_battery_stop);
}

static ssize_t store_low_battery_protect_stop(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_err("[store_low_battery_protect_stop]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_low_battery_protect_stop] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_low_battery_stop = val;
		pr_err("[store_low_battery_protect_stop] g_low_battery_stop=%d\n",
			g_low_battery_stop);
	}
	return size;
}

static DEVICE_ATTR(low_battery_protect_stop, 0664,
			show_low_battery_protect_stop, store_low_battery_protect_stop);	/*664*/

/*
 * low battery protect level
 */
static ssize_t show_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	PMICLOG("[show_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);
	return sprintf(buf, "%u\n", g_low_battery_level);
}

static ssize_t store_low_battery_protect_level(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t size)
{
	PMICLOG("[store_low_battery_protect_level] g_low_battery_level=%d\n", g_low_battery_level);

	return size;
}

static DEVICE_ATTR(low_battery_protect_level, 0664,
			show_low_battery_protect_level, store_low_battery_protect_level);	/*664*/
#endif

#ifdef BATTERY_OC_PROTECT
/*****************************************************************************
 * battery OC protect UT
 ******************************************************************************/
static ssize_t show_battery_oc_protect_ut(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	PMICLOG("[show_battery_oc_protect_ut] g_battery_oc_level=%d\n", g_battery_oc_level);
	return sprintf(buf, "%u\n", g_battery_oc_level);
}

static ssize_t store_battery_oc_protect_ut(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_err("[store_battery_oc_protect_ut]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_battery_oc_protect_ut] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 1) {
			pr_err("[store_battery_oc_protect_ut] your input is %d\n", val);
			exec_battery_oc_callback(val);
		} else {
			pr_err("[store_battery_oc_protect_ut] wrong number (%d)\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(battery_oc_protect_ut, 0664,
			show_battery_oc_protect_ut, store_battery_oc_protect_ut);	/*664*/

/*****************************************************************************
 * battery OC protect stop
 ******************************************************************************/
static ssize_t show_battery_oc_protect_stop(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	PMICLOG("[show_battery_oc_protect_stop] g_battery_oc_stop=%d\n", g_battery_oc_stop);
	return sprintf(buf, "%u\n", g_battery_oc_stop);
}

static ssize_t store_battery_oc_protect_stop(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;

	pr_err("[store_battery_oc_protect_stop]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_battery_oc_protect_stop] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_battery_oc_stop = val;
		pr_err("[store_battery_oc_protect_stop] g_battery_oc_stop=%d\n",
			g_battery_oc_stop);
	}
	return size;
}

static DEVICE_ATTR(battery_oc_protect_stop, 0664,
			show_battery_oc_protect_stop, store_battery_oc_protect_stop);	/*664*/

/*****************************************************************************
 * battery OC protect level
 ******************************************************************************/
static ssize_t show_battery_oc_protect_level(struct device *dev, struct device_attribute *attr,
					     char *buf)
{
	PMICLOG("[show_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);
	return sprintf(buf, "%u\n", g_battery_oc_level);
}

static ssize_t store_battery_oc_protect_level(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t size)
{
	PMICLOG("[store_battery_oc_protect_level] g_battery_oc_level=%d\n", g_battery_oc_level);

	return size;
}

static DEVICE_ATTR(battery_oc_protect_level, 0664,
			show_battery_oc_protect_level, store_battery_oc_protect_level);	/*664*/
#endif

#ifdef BATTERY_PERCENT_PROTECT
/*****************************************************************************
 * battery percent protect UT
 ******************************************************************************/
static ssize_t show_battery_percent_ut(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	/*show_battery_percent_protect_ut*/
	PMICLOG("[show_battery_percent_protect_ut] g_battery_percent_level=%d\n",
		g_battery_percent_level);
	return sprintf(buf, "%u\n", g_battery_percent_level);
}

static ssize_t store_battery_percent_ut(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;
	/*store_battery_percent_protect_ut*/
	pr_err("[store_battery_percent_protect_ut]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_battery_percent_protect_ut] buf is %s and size is %zu\n", buf,
			size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if (val <= 1) {
			pr_err("[store_battery_percent_protect_ut] your input is %d\n", val);
			exec_battery_percent_callback(val);
		} else {
			pr_err("[store_battery_percent_protect_ut] wrong number (%d)\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(battery_percent_protect_ut, 0664,
			show_battery_percent_ut, store_battery_percent_ut);	/*664*/

/*
 * battery percent protect stop
 ******************************************************************************/
static ssize_t show_battery_percent_stop(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	/*show_battery_percent_protect_stop*/
	PMICLOG("[show_battery_percent_protect_stop] g_battery_percent_stop=%d\n",
		g_battery_percent_stop);
	return sprintf(buf, "%u\n", g_battery_percent_stop);
}

static ssize_t store_battery_percent_stop(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int val = 0;
	/*store_battery_percent_protect_stop*/
	pr_err("[store_battery_percent_protect_stop]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_battery_percent_protect_stop] buf is %s and size is %zu\n", buf,
			size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_battery_percent_stop = val;
		pr_err("[store_battery_percent_protect_stop] g_battery_percent_stop=%d\n",
			g_battery_percent_stop);
	}
	return size;
}

static DEVICE_ATTR(battery_percent_protect_stop, 0664,
			show_battery_percent_stop, store_battery_percent_stop);	/*664*/

/*
 * battery percent protect level
 ******************************************************************************/
static ssize_t show_battery_percent_level(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	/*show_battery_percent_protect_level*/
	PMICLOG("[show_battery_percent_protect_level] g_battery_percent_level=%d\n",
		g_battery_percent_level);
	return sprintf(buf, "%u\n", g_battery_percent_level);
}

static ssize_t store_battery_percent_level(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
	/*store_battery_percent_protect_level*/
	PMICLOG("[store_battery_percent_protect_level] g_battery_percent_level=%d\n",
		g_battery_percent_level);

	return size;
}

static DEVICE_ATTR(battery_percent_protect_level, 0664,
			show_battery_percent_level, store_battery_percent_level);	/*664*/
#endif

#ifdef DLPT_FEATURE_SUPPORT
/*****************************************************************************
 * DLPT UT
 ******************************************************************************/
static ssize_t show_dlpt_ut(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[show_dlpt_ut] g_dlpt_val=%d\n", g_dlpt_val);
	return sprintf(buf, "%u\n", g_dlpt_val);
}

static ssize_t store_dlpt_ut(struct device *dev, struct device_attribute *attr, const char *buf,
			     size_t size)
{
	char *pvalue = NULL;
	unsigned int val = 0;
	int ret = 0;

	pr_err("[store_dlpt_ut]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_dlpt_ut] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 10);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 10, (unsigned int *)&val);

		pr_err("[store_dlpt_ut] your input is %d\n", val);
		exec_dlpt_callback(val);
	}
	return size;
}

static DEVICE_ATTR(dlpt_ut, 0664, show_dlpt_ut, store_dlpt_ut);	/*664*/

/*****************************************************************************
 * DLPT stop
 ******************************************************************************/
static ssize_t show_dlpt_stop(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[show_dlpt_stop] g_dlpt_stop=%d\n", g_dlpt_stop);
	return sprintf(buf, "%u\n", g_dlpt_stop);
}

static ssize_t store_dlpt_stop(struct device *dev, struct device_attribute *attr, const char *buf,
			       size_t size)
{
	char *pvalue = NULL;
	unsigned int val = 0;
	int ret = 0;

	pr_err("[store_dlpt_stop]\n");

	if (buf != NULL && size != 0) {
		pr_err("[store_dlpt_stop] buf is %s and size is %zu\n", buf, size);
		/*val = simple_strtoul(buf, &pvalue, 16);*/
		pvalue = (char *)buf;
		ret = kstrtou32(pvalue, 16, (unsigned int *)&val);
		if ((val != 0) && (val != 1))
			val = 0;
		g_dlpt_stop = val;
		pr_err("[store_dlpt_stop] g_dlpt_stop=%d\n", g_dlpt_stop);
	}
	return size;
}

static DEVICE_ATTR(dlpt_stop, 0664, show_dlpt_stop, store_dlpt_stop);	/*664 */

/*****************************************************************************
 * DLPT level
 ******************************************************************************/
static ssize_t show_dlpt_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	PMICLOG("[show_dlpt_level] g_dlpt_val=%d\n", g_dlpt_val);
	return sprintf(buf, "%u\n", g_dlpt_val);
}

static ssize_t store_dlpt_level(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t size)
{
	PMICLOG("[store_dlpt_level] g_dlpt_val=%d\n", g_dlpt_val);

	return size;
}

static DEVICE_ATTR(dlpt_level, 0664, show_dlpt_level, store_dlpt_level);	/*664*/
#endif

/*****************************************************************************
 * Low battery call back function
 ******************************************************************************/
void bat_h_int_handler(void)
{
	g_lowbat_int_bottom = 0;

	PMICLOG("[bat_h_int_handler]....\n");

	/*sub-task*/
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

	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_AUXADC_LBAT_VOLT_MAX_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
		PMIC_AUXADC_LBAT_VOLT_MIN_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
		PMIC_RG_INT_EN_BAT_L_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR)
	    );
#endif

}

void bat_l_int_handler(void)
{
	PMICLOG("[bat_l_int_handler]....\n");

	/*sub-task*/
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
		PMICLOG("[bat_l_int_handler]err,g_low_battery_level=%d\n", g_low_battery_level);

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

	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_AUXADC_LBAT_VOLT_MAX_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
		PMIC_AUXADC_LBAT_VOLT_MIN_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
		PMIC_RG_INT_EN_BAT_L_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR)
	    );
#endif

}

/*****************************************************************************
 * Battery OC call back function
 ******************************************************************************/

void fg_cur_h_int_handler(void)
{
	PMICLOG("[fg_cur_h_int_handler]....\n");

	/*sub-task*/
#ifdef BATTERY_OC_PROTECT
	g_battery_oc_level = 0;
	exec_battery_oc_callback(BATTERY_OC_LEVEL_0);
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);
	bat_oc_l_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );
#endif
}

void fg_cur_l_int_handler(void)
{
	PMICLOG("[fg_cur_l_int_handler]....\n");

	/*sub-task*/
#ifdef BATTERY_OC_PROTECT
	g_battery_oc_level = 1;
	exec_battery_oc_callback(BATTERY_OC_LEVEL_1);
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);
	bat_oc_h_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );
#endif
}

/*****************************************************************************
 * system function
 ******************************************************************************/
#ifdef DLPT_FEATURE_SUPPORT
static unsigned long pmic_node;

static int fb_early_init_dt_get_chosen(unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;
	pmic_node = node;
	return 1;
}
#endif /*end of #ifdef DLPT_FEATURE_SUPPORT*/

#if 0 /*argus TBD*/
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

/* for jade minus */
	pr_err("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_AUXADC_LBAT_VOLT_MAX_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MAX_ADDR),
		PMIC_AUXADC_LBAT_VOLT_MIN_ADDR, upmu_get_reg_value(PMIC_AUXADC_LBAT_VOLT_MIN_ADDR),
		PMIC_RG_INT_EN_BAT_L_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_BAT_L_ADDR)
	    );
#endif

#ifdef BATTERY_OC_PROTECT
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );
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
		/*lbat_min_en_setting(0);*/
		lbat_max_en_setting(1);
	} else {		/*0*/
		lbat_min_en_setting(1);
		/*lbat_max_en_setting(0);*/
	}

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		MT6353_AUXADC_LBAT3, upmu_get_reg_value(MT6353_AUXADC_LBAT3),
		MT6353_AUXADC_LBAT4, upmu_get_reg_value(MT6353_AUXADC_LBAT4),
		MT6353_INT_CON0, upmu_get_reg_value(MT6353_INT_CON0)
	    );
#endif

#ifdef BATTERY_OC_PROTECT
	bat_oc_h_en_setting(0);
	bat_oc_l_en_setting(0);
	mdelay(1);

	if (g_battery_oc_level == 1)
		bat_oc_h_en_setting(1);
	else
		bat_oc_l_en_setting(1);

	PMICLOG("Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
		PMIC_FG_CUR_HTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_HTH_ADDR),
		PMIC_FG_CUR_LTH_ADDR, upmu_get_reg_value(PMIC_FG_CUR_LTH_ADDR),
		PMIC_RG_INT_EN_FG_BAT_H_ADDR, upmu_get_reg_value(PMIC_RG_INT_EN_FG_BAT_H_ADDR)
	    );
#endif
}

void pmic_throttling_dlpt_debug_init(struct platform_device *dev, struct dentry *debug_dir)
{
	int ret_device_file = 0;

#if 0 /* argus TBD */
	struct dentry *mt_pmic_dir = debug_dir;
#endif

#ifdef LOW_BATTERY_PROTECT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_low_battery_protect_level);
#endif

#ifdef BATTERY_OC_PROTECT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_oc_protect_level);
#endif

#ifdef BATTERY_PERCENT_PROTECT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_percent_protect_level);
#endif

#ifdef DLPT_FEATURE_SUPPORT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_ut);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_stop);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dlpt_level);
#endif

#if 0 /* argus TBD */
	debugfs_create_file("low_battery_protect_ut", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &low_battery_protect_ut_fops);
	debugfs_create_file("low_battery_protect_stop", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &low_battery_protect_stop_fops);
	debugfs_create_file("low_battery_protect_level", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &low_battery_protect_level_fops);
	debugfs_create_file("battery_oc_protect_ut", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_oc_protect_ut_fops);
	debugfs_create_file("battery_oc_protect_stop", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_oc_protect_stop_fops);
	debugfs_create_file("battery_oc_protect_level", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_oc_protect_level_fops);
	debugfs_create_file("battery_percent_protect_ut", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_percent_protect_ut_fops);
	debugfs_create_file("battery_percent_protect_stop", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_percent_protect_stop_fops);
	debugfs_create_file("battery_percent_protect_level", S_IRUGO | S_IWUSR,
				mt_pmic_dir, NULL, &battery_percent_protect_level_fops);
	debugfs_create_file("dlpt_ut", S_IRUGO | S_IWUSR, mt_pmic_dir, NULL, &dlpt_ut_fops);
	debugfs_create_file("dlpt_stop", S_IRUGO | S_IWUSR, mt_pmic_dir, NULL, &dlpt_stop_fops);
	debugfs_create_file("dlpt_level", S_IRUGO | S_IWUSR, mt_pmic_dir, NULL, &dlpt_level_fops);
	PMICLOG("proc_create pmic_debug_throttling_dlpt_fops\n");
#endif
}

int pmic_throttling_dlpt_init(void)
{
#ifdef DLPT_FEATURE_SUPPORT
	const int *pimix = NULL;
	int len = 0;
#endif
#if defined(CONFIG_MTK_SMART_BATTERY)
	struct device_node *np;
	u32 val;
	char *path = "/bus/BAT_METTER";

	np = of_find_node_by_path(path);
	if (of_property_read_u32(np, "car_tune_value", &val) == 0) {
		batt_meter_cust_data.car_tune_value = (int)val;
		PMICLOG("Get car_tune_value from DT: %d\n",
			 batt_meter_cust_data.car_tune_value);
	} else {
		batt_meter_cust_data.car_tune_value = CAR_TUNE_VALUE;
		PMICLOG("Get car_tune_value from cust header\n");
	}
#endif
#ifdef DLPT_FEATURE_SUPPORT
	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, NULL) > 0)
		pimix = of_get_flat_dt_prop(pmic_node, "atag,imix_r", &len);
	if (pimix == NULL) {
		pr_err(" pimix==NULL len=%d\n", len);
	} else {
		pr_err(" pimix=%d\n", *pimix);
		ptim_rac_val_avg = *pimix;
	}

	PMICLOG("******** MT pmic driver probe!! ********%d\n", ptim_rac_val_avg);
#endif /* #ifdef DLPT_FEATURE_SUPPORT */
#if !defined CONFIG_HAS_WAKELOCKS
	wakeup_source_init(&bat_percent_notify_lock, "bat_percent_notify_lock wakelock");
	wakeup_source_init(&dlpt_notify_lock, "dlpt_notify_lock wakelock");
#else
	wake_lock_init(&bat_percent_notify_lock, WAKE_LOCK_SUSPEND,
		       "bat_percent_notify_lock wakelock");
	wake_lock_init(&dlpt_notify_lock, WAKE_LOCK_SUSPEND, "dlpt_notify_lock wakelock");
#endif

#ifdef LOW_BATTERY_PROTECT
	low_battery_protect_init();
#else
	pr_err("[PMIC] no define LOW_BATTERY_PROTECT\n");
#endif

#ifdef BATTERY_OC_PROTECT
	battery_oc_protect_init();
#else
	pr_err("[PMIC] no define BATTERY_OC_PROTECT\n");
#endif

#ifdef BATTERY_PERCENT_PROTECT
	bat_percent_notify_init();
#else
	pr_err("[PMIC] no define BATTERY_PERCENT_PROTECT\n");
#endif

#ifdef DLPT_FEATURE_SUPPORT
	dlpt_notify_init();
#else
	pr_err("[PMIC] no define DLPT_FEATURE_SUPPORT\n");
#endif

#if PMIC_THROTTLING_DLPT_UT /* UT TBD */
#ifdef LOW_BATTERY_PROTECT
	register_low_battery_notify(&low_bat_test, LOW_BATTERY_PRIO_CPU_B);
	PMICLOG("register_low_battery_notify:done\n");
#endif
#ifdef BATTERY_OC_PROTECT
	register_battery_oc_notify(&bat_oc_test, BATTERY_OC_PRIO_CPU_B);
	PMICLOG("register_battery_oc_notify:done\n");
#endif
#ifdef BATTERY_PERCENT_PROTECT
	register_battery_percent_notify(&bat_per_test, BATTERY_PERCENT_PRIO_CPU_B);
	PMICLOG("register_battery_percent_notify:done\n");
#endif
#endif
	return 0;
}

MODULE_AUTHOR("Argus Lin");
MODULE_DESCRIPTION("MT PMIC Device Driver");
MODULE_LICENSE("GPL");
