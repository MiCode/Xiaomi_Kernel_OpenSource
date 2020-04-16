/*
 * Copyright (C) 2017 MediaTek Inc.
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

#define DEBUG 1
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"

#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif


/* #include <mach/mt_wtd.h> */
#include <mach/wd_api.h>
#include <mtk_gpu_utility.h>
#include <linux/time.h>

#include <tscpu_settings.h>
#include <mtk_ts_setting.h>

#if THERMAL_LT_SET_HPM
#include <mtk_vcorefs_manager.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#define __MT_MTK_TS_CPU_C__

#if MTK_TS_CPU_RT
#include <linux/sched.h>
#include <linux/kthread.h>
#endif

#if defined(ATM_USES_PPM)
#include "mtk_ppm_api.h"
#else
#include "mt_cpufreq.h"
#endif

#include <linux/uidgid.h>
#include "mtk_auxadc.h"
#include <ap_thermal_limit.h>

/*=============================================================
 *Local variable definition
 *=============================================================
 */
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);
static int isTimerCancelled;

#if !defined(CONFIG_MTK_CLKMGR)
struct clk *therm_main;		/* main clock for Thermal */
#endif

void __iomem  *therm_clk_infracfg_ao_base;

#ifdef CONFIG_OF
u32 thermal_irq_number;
void __iomem *thermal_base;
void __iomem *auxadc_ts_base;
void __iomem *infracfg_ao_base;


#if defined(CONFIG_MACH_MT6755) || defined(CONFIG_MACH_MT6757)	\
|| defined(CONFIG_MACH_KIBOPLUS)
void __iomem *th_apmixed_base;
#else
void __iomem *apmixed_base;
#endif
void __iomem *INFRACFG_AO_base;

int thermal_phy_base;
int auxadc_ts_phy_base;
int apmixed_phy_base;
int pericfg_phy_base;
#endif

#if defined(TZCPU_SET_INIT_CFG)
/* mseconds, 0 : no auto polling */
static unsigned int interval = TZCPU_INITCFG_INTERVAL;
#else
static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
#endif

int tscpu_g_curr_temp;
int tscpu_g_prev_temp;
static int g_max_temp = 50000;	/* default=50 deg */

#if defined(CONFIG_ARCH_MT6753)
/*For MT6753 PMIC 5A throttle patch*/
static int thermal5A_TH = 55000;
static int thermal5A_status;
#endif

#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
int thermal_6353_5A_status;
int thermal_5A_limit_H = 85000; /*85C*/
int thermal_5A_limit_L = 83000; /*83C*/
#endif

#if THERMAL_LT_SET_HPM
int enter_hpm_temp = 10000;
int leave_hpm_temp = 15000;
int enable_hpm_temp = 1;
#endif
static int tc_mid_trip = -275000;
/* trip_temp[0] must be initialized to the thermal HW protection point. */
#if defined(TZCPU_SET_INIT_CFG)
static int trip_temp[10] = {
	TZCPU_INITCFG_TRIP_0_TEMP,
	TZCPU_INITCFG_TRIP_1_TEMP,
	TZCPU_INITCFG_TRIP_2_TEMP,
	TZCPU_INITCFG_TRIP_3_TEMP,
	TZCPU_INITCFG_TRIP_4_TEMP,
	TZCPU_INITCFG_TRIP_5_TEMP,
	TZCPU_INITCFG_TRIP_6_TEMP,
	TZCPU_INITCFG_TRIP_7_TEMP,
	TZCPU_INITCFG_TRIP_8_TEMP,
	TZCPU_INITCFG_TRIP_9_TEMP };
#else
static int trip_temp[10] = { 117000, 100000, 85000, 75000, 65000,
				55000, 45000, 35000, 25000, 15000 };
#endif
int tscpu_read_curr_temp;

static bool talking_flag;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#if defined(TZCPU_SET_INIT_CFG)
static int num_trip = TZCPU_INITCFG_NUM_TRIPS;
#else
static int num_trip = 5;
#endif
static int tscpu_num_opp;
static struct mtk_cpu_power_info *mtk_cpu_power;

static int g_tc_resume;	/* default=0,read temp */
static int proc_write_flag;

static struct thermal_zone_device *thz_dev;
#if defined(TZCPU_SET_INIT_CFG)
static char g_bind0[20] = TZCPU_INITCFG_TRIP_0_COOLER;
static char g_bind1[20] = TZCPU_INITCFG_TRIP_1_COOLER;
static char g_bind2[20] = TZCPU_INITCFG_TRIP_2_COOLER;
static char g_bind3[20] = TZCPU_INITCFG_TRIP_3_COOLER;
static char g_bind4[20] = TZCPU_INITCFG_TRIP_4_COOLER;
static char g_bind5[20] = TZCPU_INITCFG_TRIP_5_COOLER;
static char g_bind6[20] = TZCPU_INITCFG_TRIP_6_COOLER;
static char g_bind7[20] = TZCPU_INITCFG_TRIP_7_COOLER;
static char g_bind8[20] = TZCPU_INITCFG_TRIP_8_COOLER;
static char g_bind9[20] = TZCPU_INITCFG_TRIP_9_COOLER;
#else
static char g_bind0[20] = "mtktscpu-sysrst";
static char g_bind1[20] = "cpu02";
static char g_bind2[20] = "cpu15";
static char g_bind3[20] = "cpu22";
static char g_bind4[20] = "cpu28";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";
#endif

struct mt_gpufreq_power_table_info *mtk_gpu_power;
/* max GPU opp idx from GPU DVFS driver, default is 0 */
int gpu_max_opp;
#if 0
int Num_of_GPU_OPP = 1;		/* Set this value =1 for non-DVS GPU */
#else				/* DVFS GPU */
int Num_of_GPU_OPP;
#endif

/*=============================================================
 * Local function definition
 *=============================================================
 */
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
static void _mt_thermal_aee_init(void)
{
	int i;

	aee_rr_init_thermal_temp(TS_ENUM_MAX);
	for (i = 0; i < TS_ENUM_MAX; i++)
		aee_rr_rec_thermal_temp(i, 0xFF);

	aee_rr_rec_thermal_status(0xFF);
	aee_rr_rec_thermal_ATM_status(0xFF);
	aee_rr_rec_thermal_ktime(0xFFFFFFFFFFFFFFFF);
}
#endif
static int tscpu_thermal_probe(struct platform_device *dev);
static int tscpu_register_thermal(void);
static void tscpu_unregister_thermal(void);

#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
static int a_tscpu_all_temp[MTK_THERMAL_SENSOR_CPU_COUNT] = { 0 };

static DEFINE_MUTEX(MET_GET_TEMP_LOCK);
static met_thermalsampler_funcMET g_pThermalSampler;
void mt_thermalsampler_registerCB(met_thermalsampler_funcMET pCB)
{
	g_pThermalSampler = pCB;
}
EXPORT_SYMBOL(mt_thermalsampler_registerCB);

static DEFINE_SPINLOCK(tscpu_met_spinlock);
void tscpu_met_lock(unsigned long *flags)
{
	spin_lock_irqsave(&tscpu_met_spinlock, *flags);
}
EXPORT_SYMBOL(tscpu_met_lock);

void tscpu_met_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&tscpu_met_spinlock, *flags);
}
EXPORT_SYMBOL(tscpu_met_unlock);

#endif
static int g_is_temp_valid;
static void temp_valid_lock(unsigned long *flags);
static void temp_valid_unlock(unsigned long *flags);
/*=============================================================
 *Weak functions
 *=============================================================
 */

unsigned int  __attribute__((weak))
mt_gpufreq_get_max_power(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

	int __attribute__ ((weak))
IMM_IsAdcInitReady(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
#if 0
#if defined(ATM_USES_PPM)
	void __attribute__ ((weak))
mt_ppm_cpu_thermal_protect(unsigned int limited_power)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}
#else
	void __attribute__ ((weak))
mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}
#endif
#endif

	bool __attribute__ ((weak))
mtk_get_gpu_loading(unsigned int *pLoading)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

	void __attribute__ ((weak))
mt_ptp_lock(unsigned long *flags)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}

	void __attribute__ ((weak))
mt_ptp_unlock(unsigned long *flags)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}

	void __attribute__ ((weak))
mt_cpufreq_thermal_5A_limit(bool enable)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}

	unsigned int __attribute__ ((weak))
mt_gpufreq_get_cur_freq(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

	unsigned int __attribute__ ((weak))
mt_ppm_thermal_get_max_power(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

	unsigned int  __attribute__((weak))
mt_gpufreq_get_seg_max_opp_index(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

	unsigned int  __attribute__((weak))
mt_gpufreq_get_dvfs_table_num(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

/*=============================================================*/
long long thermal_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

static void tscpu_fast_initial_sw_workaround(void)
{
	int i = 0;
	unsigned long flags;
	/* tscpu_printk("tscpu_fast_initial_sw_workaround\n"); */

	/* tscpu_thermal_clock_on(); */


	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		mt_ptp_lock(&flags);

		tscpu_switch_bank(i);
		tscpu_thermal_fast_init();

		mt_ptp_unlock(&flags);
	}

	temp_valid_lock(&flags);
	g_is_temp_valid = 0;
	temp_valid_unlock(&flags);
}

void tscpu_thermal_tempADCPNP(int adc, int order)
{

	tscpu_dprintk("%s adc %x, order %d\n", __func__, adc, order);

	switch (order) {
	case 0:
		mt_reg_sync_writel(adc, TEMPADCPNP0);
		break;
	case 1:
		mt_reg_sync_writel(adc, TEMPADCPNP1);
		break;
	case 2:
		mt_reg_sync_writel(adc, TEMPADCPNP2);
		break;
	case 3:
		mt_reg_sync_writel(adc, TEMPADCPNP3);
		break;
	default:
		mt_reg_sync_writel(adc, TEMPADCPNP0);
		break;
	}
}

static int max_temperature_in_bank(enum thermal_bank_name bank)
{
	int j = 0;
	int max_in_bank = 0;

	for (j = 0; j < tscpu_g_bank[bank].ts_number; j++) {
		if (tscpu_bank_ts[bank][tscpu_g_bank[bank].ts[j].type]
		> max_in_bank)
			max_in_bank =
				tscpu_bank_ts[bank]
					[tscpu_g_bank[bank].ts[j].type];

		tscpu_dprintk("tscpu_get_temp CPU bank%d T%d=%d\n", bank, j,
			tscpu_bank_ts[bank][tscpu_g_bank[bank].ts[j].type]);
	}

	return max_in_bank;
}


int tscpu_max_temperature(void)
{
	int i = 0;
	int max = 0;
	int max_in_bank = 0;

	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {

		max_in_bank = max_temperature_in_bank(i);
		if (max_in_bank > max)
			max = max_in_bank;
	}

	return max;
}

void tscpu_print_all_temperature(int isDprint)
{
	int i = 0, j = 0;

	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		for (j = 0; j < tscpu_g_bank[i].ts_number; j++) {
			if (isDprint)
				tscpu_dprintk("%d ",
					tscpu_bank_ts[i]
						[tscpu_g_bank[i].ts[j].type]);
			else
				tscpu_printk("%d ",
					tscpu_bank_ts[i]
						[tscpu_g_bank[i].ts[j].type]);
		}
	}
	if (isDprint)
		tscpu_dprintk("\n");
	else
		tscpu_printk("\n");

}

void set_taklking_flag(bool flag)
{
	talking_flag = flag;
	tscpu_printk("talking_flag=%d\n", talking_flag);
}

int mtk_gpufreq_register(struct mt_gpufreq_power_table_info *freqs, int num)
{
	int i = 0;

	tscpu_dprintk("%s\n", __func__);
	mtk_gpu_power = kzalloc((num) * sizeof(
			struct mt_gpufreq_power_table_info), GFP_KERNEL);

	if (mtk_gpu_power == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mtk_gpu_power[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mtk_gpu_power[i].gpufreq_power = freqs[i].gpufreq_power;

		tscpu_dprintk("[%d].gpufreq_khz=%u, .gpufreq_power=%u\n", i,
				freqs[i].gpufreq_khz, freqs[i].gpufreq_power);
	}

	gpu_max_opp = mt_gpufreq_get_seg_max_opp_index();
	Num_of_GPU_OPP = gpu_max_opp + mt_gpufreq_get_dvfs_table_num();
	/* error check */
	if (gpu_max_opp >= num || Num_of_GPU_OPP > num || !Num_of_GPU_OPP) {
		gpu_max_opp = 0;
		Num_of_GPU_OPP = num;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_gpufreq_register);

static int tscpu_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		/* only when a valid cooler is tried to bind here,
		 * we set tc_mid_trip to trip_temp[1];
		 */
		tc_mid_trip = trip_temp[1];
		tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		/* tscpu_dprintk("tscpu_bind %s\n", cdev->type); */
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		tscpu_warn("%s error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	tscpu_printk("%s binding OK, %d\n", __func__, table_val);
	return 0;
}

static int tscpu_unbind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		/* only when a valid cooler is tried to bind here,
		 * we set tc_mid_trip to trip_temp[1];
		 */
		tc_mid_trip = -275000;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		/* tscpu_dprintk("tscpu_unbind %s\n", cdev->type); */
	} else
		return 0;


	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		tscpu_warn("%s error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	tscpu_printk("%s unbinding OK\n", __func__);
	return 0;
}

static int tscpu_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int tscpu_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int tscpu_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int tscpu_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int tscpu_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKTSCPU_TEMP_CRIT;
	return 0;
}

static int tscpu_get_temp(struct thermal_zone_device *thermal, int *t)
{
	int ret = 0;
	int curr_temp;
#if ENALBE_SW_FILTER
	int temp_temp;
	static int last_cpu_real_temp;
#endif
#if THERMAL_LT_SET_HPM
	int ts_temp, r;
#endif

#ifdef FAST_RESPONSE_ATM
	curr_temp = tscpu_get_curr_max_ts_temp();
#else
	curr_temp = tscpu_get_curr_temp();
#endif
	tscpu_dprintk("%s CPU T=%d\n", __func__, curr_temp);

	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000)
	|| (curr_temp > 85000)) {
		printk_ratelimited(TSCPU_LOG_TAG " %u %u CPU T=%d\n",
				apthermolmt_get_cpu_power_limit(),
				apthermolmt_get_gpu_power_limit(), curr_temp);
	}

#if ENALBE_SW_FILTER
	temp_temp = curr_temp;
	if (curr_temp != 0) {/* not resumed from suspensio... */
		/* invalid range */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {
			tscpu_warn("CPU temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_cpu_real_temp != 0) {
			if ((curr_temp - last_cpu_real_temp > 40000)
			|| (last_cpu_real_temp - curr_temp > 40000)) {
				/* delta 40C, invalid change */
				tscpu_warn(
				"CPU temp float hugely temp=%d, lasttemp=%d\n",
						curr_temp, last_cpu_real_temp);
				/* tscpu_printk(
				 * "RAW_TS2 = %d,RAW_TS3 = %d,RAW_TS4 = %d\n",
				 * RAW_TS2,RAW_TS3,RAW_TS4);
				 */
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_cpu_real_temp = curr_temp;
	curr_temp = temp_temp;
#endif

#if THERMAL_LT_SET_HPM
	if (enable_hpm_temp) {
		ts_temp = get_immediate_ts4_wrap();
		if (ts_temp > leave_hpm_temp && ts_temp != CLEAR_TEMP) {
			if (vcorefs_get_kicker_opp(KIR_THERMAL) != OPPI_UNREQ) {
				tscpu_warn("ts4: temp=%d leave HPM\n", ts_temp);
				r = vcorefs_request_dvfs_opp(KIR_THERMAL,
								OPPI_UNREQ);
			}
		} else if (ts_temp < enter_hpm_temp && ts_temp != CLEAR_TEMP) {
			if (vcorefs_get_kicker_opp(KIR_THERMAL) != OPPI_PERF) {
				tscpu_warn("ts4: temp=%d enter HPM\n", ts_temp);
				r = vcorefs_request_dvfs_opp(KIR_THERMAL,
								OPPI_PERF);
			}
		}
	}
#endif
	tscpu_read_curr_temp = curr_temp;
	*t = (unsigned long)curr_temp;

#if MTKTSCPU_FAST_POLLING
	tscpu_cur_fp_factor = tscpu_next_fp_factor;

	if (curr_temp >= fast_polling_trip_temp) {
		tscpu_next_fp_factor = fast_polling_factor;
		/* it means next timeout will be in
		 * interval/fast_polling_factor
		 */
		thermal->polling_delay = interval / fast_polling_factor;
	} else {
		tscpu_next_fp_factor = 1;
		thermal->polling_delay = interval;
	}
#endif

	/* for low power */
	if ((int)*t >= tscpu_polling_trip_temp1)
		;
	else if ((int)*t < tscpu_polling_trip_temp2)
		thermal->polling_delay = interval * tscpu_polling_factor2;
	else
		thermal->polling_delay = interval * tscpu_polling_factor1;

	/* tscpu_dprintk("tscpu_get_temp:thermal->polling_delay=%d\n",
	 * thermal->polling_delay);
	 */
#if CPT_ADAPTIVE_AP_COOLER
	tscpu_g_prev_temp = tscpu_g_curr_temp;
	tscpu_g_curr_temp = curr_temp;
#endif

#if THERMAL_GPIO_OUT_TOGGLE
	/*for output signal monitor */
	tscpu_set_GPIO_toggle_for_monitor();
#endif

#if defined(CONFIG_ARCH_MT6753)
	/*For MT6753 PMIC 5A throttle patch*/
	if (curr_temp >= thermal5A_TH && thermal5A_status == 0) {
		mt_cpufreq_thermal_5A_limit(1);
		thermal5A_status = 1;
	} else if (curr_temp < thermal5A_TH && thermal5A_status == 1) {
		mt_cpufreq_thermal_5A_limit(0);
		thermal5A_status = 0;
	}
#endif


#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	if (curr_temp >= thermal_5A_limit_H && thermal_6353_5A_status == 0) {
		mt_ppm_set_5A_limit_throttle(1);
		thermal_6353_5A_status = 1;
		/*tscpu_warn("mt_ppm_set_5A_limit_throttle(1)\n");*/
	} else if (curr_temp < thermal_5A_limit_L
	&& thermal_6353_5A_status == 1) {
		mt_ppm_set_5A_limit_throttle(0);
		thermal_6353_5A_status = 0;
		/*tscpu_warn("mt_ppm_set_5A_limit_throttle(0)\n");*/
	}
#endif

	g_max_temp = curr_temp;

	return ret;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscpu_dev_ops = {
	.bind = tscpu_bind,
	.unbind = tscpu_unbind,
	.get_temp = tscpu_get_temp,
	.get_mode = tscpu_get_mode,
	.set_mode = tscpu_set_mode,
	.get_trip_type = tscpu_get_trip_type,
	.get_trip_temp = tscpu_get_trip_temp,
	.get_crit_temp = tscpu_get_crit_temp,
};

static int tscpu_read_Tj_out(struct seq_file *m, void *v)
{

	int ts_con0 = 0;

	/* TS_CON0[19:16] = 0x8: Tj sensor Analog signal output via HW pin
	 */
	ts_con0 = readl(TS_CON0_TM);

	seq_printf(m, "TS_CON0:0x%x\n", ts_con0);


	return 0;
}


static ssize_t tscpu_write_Tj_out(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int lv_Tj_out_flag;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &lv_Tj_out_flag) == 0) {
		if (lv_Tj_out_flag == 1) {
			/* TS_CON0[19:16] = 0x8:
			 * Tj sensor Analog signal output via HW pin
			 */
			mt_reg_sync_writel(readl(TS_CON0_TM) | 0x00010000,
								TS_CON0_TM);
		} else {
			/* TS_CON0[19:16] = 0x8:
			 * Tj sensor Analog signal output via HW pin
			 */
			mt_reg_sync_writel(readl(TS_CON0_TM) & 0xfffeffff,
								TS_CON0_TM);
		}

		tscpu_dprintk("%s lv_Tj_out_flag=%d\n", __func__,
								lv_Tj_out_flag);
		return count;
	}
	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}


#if THERMAL_GPIO_OUT_TOGGLE
static int g_trigger_temp = 95000;	/* default 95 deg */
static int g_GPIO_out_enable;	/* 0:disable */
static int g_GPIO_already_set;

#define GPIO118_MODE           (GPIO_BASE + 0x0770)
#define GPIO118_DIR            (GPIO_BASE + 0x0070)
#define GPIO118_DOUT           (GPIO_BASE + 0x0470)

void tscpu_set_GPIO_toggle_for_monitor(void)
{
	int lv_GPIO118_MODE, lv_GPIO118_DIR, lv_GPIO118_DOUT;

	tscpu_dprintk(
		"%s,g_GPIO_out_enable=%d\n", __func__,
		g_GPIO_out_enable);

	if (g_GPIO_out_enable == 1) {
		if (g_max_temp > g_trigger_temp) {

			tscpu_printk("g_max_temp %d > g_trigger_temp %d\n",
						g_max_temp, g_trigger_temp);

			g_GPIO_out_enable = 0;	/* only can enter once */
			g_GPIO_already_set = 1;

			lv_GPIO118_MODE = readl(GPIO118_MODE);
			lv_GPIO118_DIR = readl(GPIO118_DIR);
			lv_GPIO118_DOUT = readl(GPIO118_DOUT);

			tscpu_printk(
				"%s:lv_GPIO118_MODE=0x%x,", __func__,
				lv_GPIO118_MODE);

			tscpu_printk(
				"lv_GPIO118_DIR=0x%x,lv_GPIO118_DOUT=0x%x,\n",
				lv_GPIO118_DIR, lv_GPIO118_DOUT);


			/* //clear GPIO118_MODE[11:9]
			 * thermal_clrl(GPIO118_MODE,0x00000E00);
			 */
			/* //set GPIO118_DIR[6]=1
			 * thermal_setl(GPIO118_DIR, 0x00000040);
			 */

			/* set GPIO118_DOUT[6]=0 Low */
			thermal_clrl(GPIO118_DOUT, 0x00000040);
			udelay(200);
			/* set GPIO118_DOUT[6]=1 Hiht */
			thermal_setl(GPIO118_DOUT, 0x00000040);
		} else {
			if (g_GPIO_already_set == 1) {
				/* restore */
				g_GPIO_already_set = 0;
				/* set GPIO118_DOUT[6]=0 Low */
				thermal_clrl(GPIO118_DOUT, 0x00000040);

			}
		}
	}

}

/*
 *For example:
 *GPIO_BASE :0x10005000
 *
 *GPIO118_MODE = 0 (change to GPIO mode)
 *0x0770	GPIO_MODE24	16	GPIO Mode Control Register 24
 *11	9	GPIO118_MODE	RW	Public	3'd1	"0: GPIO118
 *						(IO)1: UTXD3 (O)2: URXD3
 *						(I)3: MD_UTXD (O)
 *4: LTE_UTXD (O)5: TDD_TXD (O)6: Reserved7: DBG_MON_A_10_ (IO)"
 *							Selects GPIO 118 mode
 *
 *GPIO118_DIR =1  (output)
 *0x0070	GPIO_DIR8	16	GPIO Direction Control Register 8
 *6	6	GPIO118_DIR	RW	Public	1'b0	"0: Input1: Output"
 *						GPIO 118 direction control
 *
 *GPIO118_DOUT=1/0 (hi or low)
 *0x0470	GPIO_DOUT8	16	GPIO Data Output Register 8
 *6	6	GPIO118_DOUT	RW	Public	1'b0	"0: Output 01: Output 1"
 *						GPIO 118 data output value
 *
 */
static int tscpu_read_GPIO_out(struct seq_file *m, void *v)
{

	seq_printf(m, "GPIO out enable:%d, trigger temperature=%d\n",
							g_GPIO_out_enable,
							g_trigger_temp);

	return 0;
}


static ssize_t tscpu_write_GPIO_out(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[512];
	char TEMP[10], ENABLE[10];
	unsigned int valTEMP, valENABLE;


	int len = 0;

	int lv_GPIO118_MODE, lv_GPIO118_DIR;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%9s %d %9s %d ", TEMP, &valTEMP,
		ENABLE, &valENABLE) == 4) {
		/* tscpu_printk("XXXXXXXXX\n"); */

		if (!strcmp(TEMP, "TEMP")) {
			g_trigger_temp = valTEMP;
			tscpu_printk("g_trigger_temp=%d\n", valTEMP);
		} else {
			tscpu_printk(
				"%s TEMP bad argument\n", __func__);
			return -EINVAL;
		}

		if (!strcmp(ENABLE, "ENABLE")) {
			g_GPIO_out_enable = valENABLE;
			tscpu_printk(
				"g_GPIO_out_enable=%d,g_GPIO_already_set=%d\n",
						valENABLE, g_GPIO_already_set);
		} else {
			tscpu_printk(
				"%s ENABLE bad argument\n", __func__);
			return -EINVAL;
		}

		lv_GPIO118_MODE = readl(GPIO118_MODE);
		lv_GPIO118_DIR = readl(GPIO118_DIR);

		/* clear GPIO118_MODE[11:9],
		 * GPIO118_MODE = 0 (change to GPIO mode)
		 */
		thermal_clrl(GPIO118_MODE, 0x00000E00);

		/* set GPIO118_DIR[6]=1,GPIO118_DIR =1  (output) */
		thermal_setl(GPIO118_DIR, 0x00000040);

		/* set GPIO118_DOUT[6]=0 Low */
		thermal_clrl(GPIO118_DOUT, 0x00000040);
		return count;
	}

	tscpu_printk("%s bad argument\n", __func__);
	return -EINVAL;
}
#endif


static int tscpu_read_opp(struct seq_file *m, void *v)
{
	unsigned int cpu_power, gpu_power;
	unsigned int gpu_loading = 0;

	cpu_power = apthermolmt_get_cpu_power_limit();
	gpu_power = apthermolmt_get_gpu_power_limit();

#if CPT_ADAPTIVE_AP_COOLER

	if (!mtk_get_gpu_loading(&gpu_loading))
		gpu_loading = 0;

	seq_printf(m, "%d,%d,%d,%d,%d\n",
			(int)((cpu_power != 0x7FFFFFFF) ? cpu_power : 0),
			(int)((gpu_power != 0x7FFFFFFF) ? gpu_power : 0),
			/* ((NULL == mtk_thermal_get_gpu_loading_fp) ?
			 *	0 : mtk_thermal_get_gpu_loading_fp()),
			 */
			(int)gpu_loading, (int)mt_gpufreq_get_cur_freq(),
			get_target_tj());

#else
	seq_printf(m, "%d,%d,0,%d\n",
			(int)((cpu_power != 0x7FFFFFFF) ? cpu_power : 0),
			(int)((gpu_power != 0x7FFFFFFF) ? gpu_power : 0),
			(int)mt_gpufreq_get_cur_freq());
#endif

	return 0;
}

static int tscpu_talking_flag_read(struct seq_file *m, void *v)
{

	seq_printf(m, "%d\n", talking_flag);

	return 0;
}

static ssize_t tscpu_talking_flag_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int lv_talking_flag;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &lv_talking_flag) == 0) {
		talking_flag = lv_talking_flag;
		tscpu_dprintk("%s talking_flag=%d\n", __func__,
								talking_flag);
		return count;
	}

	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}

#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
static int tscpu_pmic_current_limit_read(struct seq_file *m, void *v)
{
	seq_printf(m, "H:%d L:%d T:%d\n",
			thermal_5A_limit_H, thermal_5A_limit_L,
			fast_polling_trip_temp);
	return 0;
}


static ssize_t tscpu_pmic_current_limit_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int lv_thermal_5A_limit_H;
	int lv_thermal_5A_limit_L;
	int lv_fast_polling_trip_temp;
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	tscpu_dprintk("%s\n", __func__);


	if (sscanf(desc, "%d %d %d", &lv_thermal_5A_limit_H,
		&lv_thermal_5A_limit_L, &lv_fast_polling_trip_temp) == 3) {

		thermal_5A_limit_H = lv_thermal_5A_limit_H;
		thermal_5A_limit_L = lv_thermal_5A_limit_L;
		fast_polling_trip_temp = lv_fast_polling_trip_temp;

		tscpu_dprintk(
			"5A_limit_H=%d,5A_limit_L=%d,Fast polling_trip_temp=%d\n",
			thermal_5A_limit_H, thermal_5A_limit_L,
			fast_polling_trip_temp);

		return count;
	}

	tscpu_warn("%s bad argument\n", __func__);

	return -EINVAL;
}
#endif


static int tscpu_read_log(struct seq_file *m, void *v)
{

	seq_printf(m, "[ %s] log = %d\n", __func__, tscpu_debug_log);


	return 0;
}

static int tscpu_read_cal(struct seq_file *m, void *v)
{
	/* seq_printf(m, "mtktscpu cal:\n devinfo index(16)=0x%x,
	 *	devinfo index(17)=0x%x, devinfo index(18)=0x%x\n",
	 *	get_devinfo_with_index(16), get_devinfo_with_index(17),
	 *	get_devinfo_with_index(18));
	 */
	return 0;
}

static int tscpu_read(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m,
		"[%s]%d\ntrip_0=%d %d %s\ntrip_1=%d %d %s\ntrip_2=%d %d %s\ntrip_3=%d %d %s\ntrip_4=%d %d %s\ntrip_5=%d %d %s\ntrip_6=%d %d %s\ntrip_7=%d %d %s\ntrip_8=%d %d %s\ntrip_9=%d %d %s\ninterval=%d\n",
		__func__,
			num_trip,
			trip_temp[0], g_THERMAL_TRIP[0], g_bind0,
			trip_temp[1], g_THERMAL_TRIP[1], g_bind1,
			trip_temp[2], g_THERMAL_TRIP[2], g_bind2,
			trip_temp[3], g_THERMAL_TRIP[3], g_bind3,
			trip_temp[4], g_THERMAL_TRIP[4], g_bind4,
			trip_temp[5], g_THERMAL_TRIP[5], g_bind5,
			trip_temp[6], g_THERMAL_TRIP[6], g_bind6,
			trip_temp[7], g_THERMAL_TRIP[7], g_bind7,
			trip_temp[8], g_THERMAL_TRIP[8], g_bind8,
			trip_temp[9], g_THERMAL_TRIP[9], g_bind9, interval);

	for (i = gpu_max_opp; i < Num_of_GPU_OPP; i++)
		seq_printf(m, "g %d %d %d\n", i, mtk_gpu_power[i].gpufreq_khz,
				mtk_gpu_power[i].gpufreq_power);

	for (i = 0; i < tscpu_num_opp; i++)
		seq_printf(m, "c %d %d %d %d\n", i,
					mtk_cpu_power[i].cpufreq_khz,
					mtk_cpu_power[i].cpufreq_ncpu,
					mtk_cpu_power[i].cpufreq_power);

	for (i = 0; i < CPU_COOLER_NUM; i++)
		seq_printf(m, "d %d %d\n", i, tscpu_cpu_dmips[i]);


	return 0;
}

static ssize_t tscpu_write_log(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int log_switch;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &log_switch) == 0)
		/* if (5 <= sscanf(desc, "%d %d %d %d %d", &log_switch,
		 * &hot, &normal, &low, &lv_offset))
		 */
	{
		tscpu_debug_log = log_switch;

		return count;
	}

	tscpu_warn("%s bad argument\n", __func__);
	return -EINVAL;
}

#if MTKTSCPU_FAST_POLLING
static int tscpu_read_fastpoll(struct seq_file *m, void *v)
{
	seq_printf(m, "trip %d factor %d\n", fast_polling_trip_temp,
							fast_polling_factor);
	return 0;
}

static ssize_t tscpu_write_fastpoll(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;

	int trip = -1, factor = -1;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &trip, &factor) >= 2) {
		tscpu_printk("%s input %d %d\n", __func__,
							trip, factor);

		if ((trip >= 0) && (factor > 0)) {
			fast_polling_trip_temp = trip;
			fast_polling_factor = factor;

			tscpu_printk("%s applied %d %d\n", __func__,
							fast_polling_trip_temp,
							fast_polling_factor);
		} else {
			tscpu_dprintk("%s out of range\n", __func__);
		}

		return count;
	}

	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}
#endif

#if THERMAL_LT_SET_HPM
static int tscpu_read_ts4(struct seq_file *m, void *v)
{
	int ts_temp = get_immediate_ts4_wrap();

	seq_printf(m, "ts4_temp: %d\n", ts_temp);
	seq_printf(m, "enter_hpm: %d leave_hpm: %d enable: %d\n",
			enter_hpm_temp, leave_hpm_temp, enable_hpm_temp);
	return 0;
}

static ssize_t tscpu_write_ts4(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;

	int set_enable = 0;
	int trip1 = -1, trip2 = -1, enable = 1;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &trip1, &trip2, &enable) == 3) {
		set_enable = 1;
	} else if (sscanf(desc, "%d %d", &trip1, &trip2) == 2) {
		set_enable = 0;
	} else {
		tscpu_warn("%s bad argument\n", __func__);
		return -EINVAL;
	}

	if ((trip1 >= 0) && (trip2 >= 0)) {
		enter_hpm_temp = trip1;
		leave_hpm_temp = trip2;
		tscpu_warn(
			"%s applied enter_hpm: %d , leave_hpm: %d\n", __func__,
			enter_hpm_temp, leave_hpm_temp);
	} else {
		tscpu_warn("%s out of range\n", __func__);
	}

	if (set_enable) {
		enable_hpm_temp = !!(enable);
		tscpu_warn("%s enable: %d (%d)\n", __func__,
						enable_hpm_temp, enable);
	}

	return count;
}
#endif

static ssize_t tscpu_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	int i;
	struct mtktscpu_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktscpu_data *ptr_mtktscpu_data = kmalloc(sizeof(
					*ptr_mtktscpu_data), GFP_KERNEL);

	if (ptr_mtktscpu_data == NULL)
		return -ENOMEM;



	len = (count < (sizeof(ptr_mtktscpu_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktscpu_data->desc) - 1);

	if (copy_from_user(ptr_mtktscpu_data->desc, buffer, len)) {
		kfree(ptr_mtktscpu_data);
		return -ENOMEM;
	}

	ptr_mtktscpu_data->desc[len] = '\0';

	if (sscanf(ptr_mtktscpu_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktscpu_data->trip[0], &ptr_mtktscpu_data->t_type[0],
		ptr_mtktscpu_data->bind0,
		&ptr_mtktscpu_data->trip[1], &ptr_mtktscpu_data->t_type[1],
		ptr_mtktscpu_data->bind1,
		&ptr_mtktscpu_data->trip[2], &ptr_mtktscpu_data->t_type[2],
		ptr_mtktscpu_data->bind2,
		&ptr_mtktscpu_data->trip[3], &ptr_mtktscpu_data->t_type[3],
		ptr_mtktscpu_data->bind3,
		&ptr_mtktscpu_data->trip[4], &ptr_mtktscpu_data->t_type[4],
		ptr_mtktscpu_data->bind4,
		&ptr_mtktscpu_data->trip[5], &ptr_mtktscpu_data->t_type[5],
		ptr_mtktscpu_data->bind5,
		&ptr_mtktscpu_data->trip[6], &ptr_mtktscpu_data->t_type[6],
		ptr_mtktscpu_data->bind6,
		&ptr_mtktscpu_data->trip[7], &ptr_mtktscpu_data->t_type[7],
		ptr_mtktscpu_data->bind7,
		&ptr_mtktscpu_data->trip[8], &ptr_mtktscpu_data->t_type[8],
		ptr_mtktscpu_data->bind8,
		&ptr_mtktscpu_data->trip[9], &ptr_mtktscpu_data->t_type[9],
		ptr_mtktscpu_data->bind9,
		&ptr_mtktscpu_data->time_msec) == 32) {
		/* modify for PTPOD, if disable Thermal,
		 * PTPOD still need to use this function
		 * for getting temperature
		 */

#if defined(CONFIG_ARCH_MT6797)
		apthermolmt_set_general_cpu_power_limit(900);
#endif

		down(&sem_mutex);
		tscpu_dprintk("%s tscpu_unregister_thermal\n", __func__);
		tscpu_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT, "tscpu_write",
						"Bad argument");
#endif
			tscpu_dprintk("%s bad argument\n", __func__);
			kfree(ptr_mtktscpu_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] =  ptr_mtktscpu_data->t_type[i];


		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktscpu_data->bind0[i];
			g_bind1[i] = ptr_mtktscpu_data->bind1[i];
			g_bind2[i] = ptr_mtktscpu_data->bind2[i];
			g_bind3[i] = ptr_mtktscpu_data->bind3[i];
			g_bind4[i] = ptr_mtktscpu_data->bind4[i];
			g_bind5[i] = ptr_mtktscpu_data->bind5[i];
			g_bind6[i] = ptr_mtktscpu_data->bind6[i];
			g_bind7[i] = ptr_mtktscpu_data->bind7[i];
			g_bind8[i] = ptr_mtktscpu_data->bind8[i];
			g_bind9[i] = ptr_mtktscpu_data->bind9[i];
		}

#if CPT_ADAPTIVE_AP_COOLER
		/* initialize... */
		for (i = 0; i < MAX_CPT_ADAPTIVE_COOLERS; i++)
			TARGET_TJS[i] = 117000;

		if (!strncmp(&ptr_mtktscpu_data->bind0[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind0[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind0[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind0[13] - '0')]
					= ptr_mtktscpu_data->trip[0];

		if (!strncmp(&ptr_mtktscpu_data->bind1[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind1[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind1[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind1[13] - '0')]
					= ptr_mtktscpu_data->trip[1];

		if (!strncmp(&ptr_mtktscpu_data->bind2[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind2[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind2[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind2[13] - '0')]
					= ptr_mtktscpu_data->trip[2];

		if (!strncmp(&ptr_mtktscpu_data->bind3[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind3[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind3[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind3[13] - '0')]
					= ptr_mtktscpu_data->trip[3];

		if (!strncmp(&ptr_mtktscpu_data->bind4[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind4[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind4[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind4[13] - '0')]
					= ptr_mtktscpu_data->trip[4];

		if (!strncmp(&ptr_mtktscpu_data->bind5[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind5[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind5[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind5[13] - '0')]
					= ptr_mtktscpu_data->trip[5];

		if (!strncmp(&ptr_mtktscpu_data->bind6[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind6[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind6[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind6[13] - '0')]
					= ptr_mtktscpu_data->trip[6];

		if (!strncmp(&ptr_mtktscpu_data->bind7[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind7[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind7[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind7[13] - '0')]
					= ptr_mtktscpu_data->trip[7];

		if (!strncmp(&ptr_mtktscpu_data->bind8[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind8[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind8[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind8[13] - '0')]
					= ptr_mtktscpu_data->trip[8];

		if (!strncmp(&ptr_mtktscpu_data->bind9[0],
			adaptive_cooler_name, 13))
			if ((ptr_mtktscpu_data->bind9[13] - '0') >= 0
			&& (ptr_mtktscpu_data->bind9[13] - '0')
				< MAX_CPT_ADAPTIVE_COOLERS)
				TARGET_TJS[(ptr_mtktscpu_data->bind9[13] - '0')]
					= ptr_mtktscpu_data->trip[9];

		tscpu_dprintk("%s TTJ0=%d, TTJ1=%d, TTJ2=%d\n", __func__,
				TARGET_TJS[0], TARGET_TJS[1], TARGET_TJS[2]);
#endif

		tscpu_dprintk(
			"%s g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		tscpu_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		tscpu_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		tscpu_dprintk(
			"%s cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		tscpu_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);


		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktscpu_data->trip[i];

		interval = ptr_mtktscpu_data->time_msec;

		tscpu_dprintk(
			"%s trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2],
			trip_temp[3], trip_temp[4]);

		tscpu_dprintk(
			"trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,",
			trip_temp[5], trip_temp[6], trip_temp[7],
			trip_temp[8], trip_temp[9]);

		tscpu_dprintk("time_ms=%d, num_trip=%d\n", interval, num_trip);

		/* get temp, set high low threshold */
		/*
		 *  curr_temp = get_immediate_temp();
		 *  for(i=0; i<num_trip; i++)
		 *  {
		 *  if(curr_temp>trip_temp[i])
		 *  break;
		 *  }
		 *  if(i==0)
		 *  {
		 *  tscpu_printk("tscpu_write setting error");
		 *  }
		 *  else if(i==num_trip)
		 *  set_high_low_threshold(trip_temp[i-1], 10000);
		 *  else
		 *  set_high_low_threshold(trip_temp[i-1], trip_temp[i]);
		 */
		tscpu_dprintk("%s tscpu_register_thermal\n", __func__);
		tscpu_register_thermal();
		up(&sem_mutex);

#if defined(CONFIG_ARCH_MT6797)
		apthermolmt_set_general_cpu_power_limit(0);
#endif
		proc_write_flag = 1;
		kfree(ptr_mtktscpu_data);
		return count;
	}

	tscpu_dprintk("%s bad argument\n", __func__);
#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"tscpu_write",
							"Bad argument");
#endif
	kfree(ptr_mtktscpu_data);
	return -EINVAL;
}

static int tscpu_register_thermal(void)
{

	tscpu_dprintk("%s\n", __func__);

	/* trips : trip 0~3 */
	thz_dev = mtk_thermal_zone_device_register("mtktscpu", num_trip, NULL,
			&mtktscpu_dev_ops, 0, 0, 0, interval);

	return 0;
}

static void tscpu_unregister_thermal(void)
{

	tscpu_dprintk("%s\n", __func__);
	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}

}

/* pause ALL periodoc temperature sensing point */
static void thermal_pause_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;
	/* tscpu_printk("thermal_pause_all_periodoc_temp_sensing\n"); */

	/*config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		mt_ptp_lock(&flags);

		tscpu_switch_bank(i);
		temp = readl(TEMPMSRCTL1);
		/* set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3 */
		mt_reg_sync_writel((temp | 0x10E), TEMPMSRCTL1);

		mt_ptp_unlock(&flags);
	}
}


/* release ALL periodoc temperature sensing point */
static void thermal_release_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;

	/* tscpu_printk("thermal_release_all_periodoc_temp_sensing\n"); */

	/*config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		mt_ptp_lock(&flags);

		tscpu_switch_bank(i);
		temp = readl(TEMPMSRCTL1);
		/* set bit1=bit2=bit3=0 to release sensing point 0,1,2 */
		mt_reg_sync_writel(((temp & (~0x10E))), TEMPMSRCTL1);

		mt_ptp_unlock(&flags);
	}
}

void tscpu_thermal_enable_all_periodoc_sensing_point(
enum thermal_bank_name bank_num)
{

	switch (tscpu_g_bank[bank_num].ts_number) {
	case 1:
		/* enable periodoc temperature sensing point 0 */
		mt_reg_sync_writel(0x00000001, TEMPMONCTL0);
		break;
	case 2:
		/* enable periodoc temperature sensing point 0,1 */
		mt_reg_sync_writel(0x00000003, TEMPMONCTL0);
		break;
	case 3:
		/* enable periodoc temperature sensing point 0,1,2 */
		mt_reg_sync_writel(0x00000007, TEMPMONCTL0);
		break;
	default:
		tscpu_printk("Error at %s\n", __func__);
		break;
	}
}

/* disable ALL periodoc temperature sensing point */
static void thermal_disable_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	/* tscpu_printk("thermal_disable_all_periodoc_temp_sensing\n"); */

	/* config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		mt_ptp_lock(&flags);

		tscpu_switch_bank(i);
		/* tscpu_printk(
		 *	"thermal_disable_all_periodoc_temp_sensing:Bank_%d\n",
		 *	i);
		 */
		mt_reg_sync_writel(0x00000000, TEMPMONCTL0);

		mt_ptp_unlock(&flags);
	}

}

static void tscpu_clear_all_temp(void)
{
	/* 26111 to avoid ptpod judge <25deg will not update voltage. */
	/* CPU_TS_MCU2_T=26111; */
	/* GPU_TS_MCU1_T=26111; */
	/* LTE_TS_MCU3_T=26111; */
	int i = 0;
	int j = 0;

	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		for (j = 0; j < tscpu_g_bank[i].ts_number; j++)
			tscpu_bank_ts[i][tscpu_g_bank[i].ts[j].type] =
								CLEAR_TEMP;
	}
}

/*tscpu_thermal_suspend spend 1000us~1310us*/
static int tscpu_thermal_suspend(
struct platform_device *dev, pm_message_t state)
{
	int cnt = 0;
	int temp = 0;

	tscpu_printk("%s\n", __func__);
#if THERMAL_PERFORMANCE_PROFILE
	struct timeval begin, end;
	unsigned long val;

	do_gettimeofday(&begin);
#endif

	g_tc_resume = 1;	/* set "1", don't read temp during suspend */

	if (talking_flag == false) {
		tscpu_dprintk("%s no talking\n", __func__);

#if (CONFIG_THERMAL_AEE_RR_REC == 1)
		aee_rr_rec_thermal_status(TSCPU_SUSPEND);
#endif

#if defined(CONFIG_ARCH_MT6797)
		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */

		do {
			temp = (readl(THAHBST0) >> 16);
			if ((cnt + 1) % 10 == 0)
				pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n",
							temp, cnt, __LINE__);

			udelay(50);
			cnt++;
		} while (temp != 0x0 && cnt < 50);
#else
		thermal_pause_all_periodoc_temp_sensing(); /* TEMPMSRCTL1 */

		do {
			temp = (readl(THAHBST0) >> 16);
			if ((cnt + 1) % 10 == 0)
				pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n",
							temp, cnt, __LINE__);

			udelay(2);
			cnt++;
		} while (temp != 0x0 && cnt < 50);

		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */
#endif
		/* tscpu_thermal_clock_off(); */

		/*TSCON1[5:4]=2'b11, Buffer off */
		/* turn off the sensor buffer to save power */
		mt_reg_sync_writel(readl(TS_CONFIGURE) | TS_TURN_OFF,
								TS_CONFIGURE);
	}
#if THERMAL_PERFORMANCE_PROFILE
	do_gettimeofday(&end);

	/* Get milliseconds */
	pr_debug("suspend time spent, sec : %lu , usec : %lu\n",
						(end.tv_sec - begin.tv_sec),
						(end.tv_usec - begin.tv_usec));
#endif
	return 0;
}

/*tscpu_thermal_suspend spend 3000us~4000us*/
static int tscpu_thermal_resume(struct platform_device *dev)
{
	int temp = 0;
	int cnt = 0;

	tscpu_printk("%s\n", __func__);

	g_tc_resume = 1; /* set "1", don't read temp during start resume */

	if (talking_flag == false) {
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
		aee_rr_rec_thermal_status(TSCPU_RESUME);
#endif

		tscpu_reset_thermal();
		/*
		 *  TS_CON1 default is 0x30, this is buffer off
		 *  we should turn on this buffer berore we use thermal sensor,
		 *  or this buffer off will let TC read a very small value
		 *  from auxadc and this small value will trigger thermal reboot
		 */
		temp = readl(TS_CONFIGURE);

		/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
		temp &= ~(TS_TURN_OFF);
		mt_reg_sync_writel(temp, TS_CONFIGURE);	/* read abb need */
		/* RG_TS2AUXADC < set from 2'b11 to 2'b00
		 *when resume.wait 100uS than turn on thermal controller.
		 */
		udelay(200);

		WARN_ON_ONCE((readl(TS_CONFIGURE) & TS_TURN_OFF) != 0x0);

		/* Add this function to read all temp first to avoid
		 * write TEMPPROTTC first time will issue an fake signal to RGU
		 */
		tscpu_fast_initial_sw_workaround();

#if defined(CONFIG_ARCH_MT6797)
		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */

		do {
			temp = (readl(THAHBST0) >> 16);
			if ((cnt + 1) % 10 == 0)
				pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n",
							temp, cnt, __LINE__);

			udelay(50);
			cnt++;
		} while (temp != 0x0 && cnt < 50);
#else
		thermal_pause_all_periodoc_temp_sensing(); /* TEMPMSRCTL1 */

		do {
			temp = (readl(THAHBST0) >> 16);
			if ((cnt + 1) % 10 == 0)
				pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n",
							temp, cnt, __LINE__);

			udelay(2);
			cnt++;
		} while (temp != 0x0 && cnt < 50);

		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */
#endif
		tscpu_thermal_initial_all_bank();

		/* must release before start */
		thermal_release_all_periodoc_temp_sensing();

		tscpu_clear_all_temp();

		tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);
	}

	g_tc_resume = 2;	/* set "2", resume finish,can read temp */

	return 0;
}

static struct platform_driver mtk_thermal_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = tscpu_thermal_probe,
	.suspend = tscpu_thermal_suspend,
	.resume = tscpu_thermal_resume,
	.driver = {
		.name = THERMAL_NAME,
#ifdef CONFIG_OF
		.of_match_table = mt_thermal_of_match,
#endif
	},
};

#if MTK_TS_CPU_RT
static int ktp_limited = -275000;

static int ktp_thread(void *arg)
{
	int max_temp = 0;

	int bank0_T;



	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	tscpu_printk("%s 1st run\n", __func__);


	schedule();

	for (;;) {
		int temp_tc_mid_trip = tc_mid_trip;
		int temp_ktp_limited = ktp_limited;

		tscpu_printk("%s awake,tc_mid_trip=%d\n",
			__func__, tc_mid_trip);
		if (kthread_should_stop())
			break;


		/* bank0_T =
		 *	MAX(MAX(CPU_TS_MCU2_T,GPU_TS_MCU1_T),LTE_TS_MCU3_T);
		 */
		bank0_T = tscpu_max_temperature();
		max_temp = bank0_T;

		tscpu_warn("%s temp=%d\n", __func__, max_temp);

		if ((temp_tc_mid_trip > -275000)
		&& (max_temp >= (temp_tc_mid_trip - 5000))) {
			/* trip_temp[1] should be shutdown point... */
			/* Do what ever we want */
			tscpu_dprintk("%s overheat %d\n", __func__, max_temp);

			/* freq/volt down or cpu down or backlight down
			 * or charging down...
			 */
			apthermolmt_set_general_cpu_power_limit(600);
			apthermolmt_set_general_gpu_power_limit(600);

			ktp_limited = temp_tc_mid_trip;

			msleep(20 * 1000);
		} else if ((temp_ktp_limited > -275000)
		&& (max_temp < temp_ktp_limited)) {
			/*
			 *unsigned int final_limit;
			 *
			 *final_limit = MIN(static_cpu_power_limit,
			 *			adaptive_cpu_power_limit);
			 *tscpu_dprintk("ktp_thread unlimit cpu=%d\n",
			 *					final_limit);
			 */
			apthermolmt_set_general_cpu_power_limit(0);

			/*
			 *final_limit = MIN(static_gpu_power_limit,
			 *			adaptive_gpu_power_limit);
			 *tscpu_dprintk("ktp_thread unlimit gpu=%d\n",
			 *					final_limit);
			 *mt_gpufreq_thermal_protect(
			 *		(final_limit != 0x7FFFFFFF) ?
			 *				final_limit : 0);
			 */
			apthermolmt_set_general_gpu_power_limit(0);

			ktp_limited = -275000;

			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		} else {
			tscpu_dprintk(
				"%s else temp=%d, trip=%d, ltd=%d\n", __func__,
				max_temp, temp_tc_mid_trip, temp_ktp_limited);

			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}
	}

	tscpu_dprintk("%s stopped\n", __func__);

	return 0;
}
#endif

int tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	int bank_T = 0;

	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);
	if (ts_bank < ARRAY_SIZE(tscpu_g_bank))
		bank_T = max_temperature_in_bank(ts_bank);
	else
		panic("Bank number out of range\n");

	return bank_T;
}

#if THERMAL_GPIO_OUT_TOGGLE
static int tscpu_GPIO_out(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_GPIO_out, NULL);
}

static const struct file_operations mtktscpu_GPIO_out_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_GPIO_out,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_GPIO_out,
	.release = single_release,
};
#endif



static int tscpu_Tj_out(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_Tj_out, NULL);
}

static const struct file_operations mtktscpu_Tj_out_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_Tj_out,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_Tj_out,
	.release = single_release,
};



static int tscpu_open_opp(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_opp, NULL);
}

static const struct file_operations mtktscpu_opp_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_open_opp,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int tscpu_open_log(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_log, NULL);
}

static const struct file_operations mtktscpu_log_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_open_log,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_log,
	.release = single_release,
};

static int tscpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read, NULL);
}

static const struct file_operations mtktscpu_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write,
	.release = single_release,
};

#if defined(CONFIG_ARCH_MT6753)
/*For MT6753 PMIC 5A throttle patch*/
static int tzcpu_cpufreq5A_read(struct seq_file *m, void *v)
{
	seq_printf(m, "Thermal 5A threshold= %d\n", thermal5A_TH);

	return 0;
}

static ssize_t tzcpu_cpufreq5A_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int th;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &th) == 0) {
		thermal5A_TH = th;
		return count;
	}

	tscpu_printk(" bad argument\n");
	return -EINVAL;
}

static int tzcpu_cpufreq5A_open(struct inode *inode, struct file *file)
{
	return single_open(file, tzcpu_cpufreq5A_read, NULL);
}

static const struct file_operations tzcpu_cpufreq5A_fops = {
	.owner = THIS_MODULE,
	.open = tzcpu_cpufreq5A_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tzcpu_cpufreq5A_write,
	.release = single_release,
};
#endif

static int tscpu_cal_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_cal, NULL);
}

static const struct file_operations mtktscpu_cal_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_cal_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int tscpu_read_temperature_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_temperature_info, NULL);
}

static const struct file_operations mtktscpu_read_temperature_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_read_temperature_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write,
	.release = single_release,
};

#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
static int tscpu_pmic_current_limit_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_pmic_current_limit_read, NULL);
}
static const struct file_operations mtktscpu_pmic_current_limit_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_pmic_current_limit_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_pmic_current_limit_write,
	.release = single_release,
};
#endif

static int tscpu_talking_flag_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_talking_flag_read, NULL);
}

static const struct file_operations mtktscpu_talking_flag_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_talking_flag_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_talking_flag_write,
	.release = single_release,
};


#if MTKTSCPU_FAST_POLLING
static int tscpu_fastpoll_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_fastpoll, NULL);
}

static const struct file_operations mtktscpu_fastpoll_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_fastpoll_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_fastpoll,
	.release = single_release,
};
#endif


#if THERMAL_LT_SET_HPM
static int tscpu_ts4_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_ts4, NULL);
}

static const struct file_operations mtktscpu_ts4_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_ts4_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_ts4,
	.release = single_release,
};
#endif

static int tscpu_read_ttpct(struct seq_file *m, void *v)
{
	unsigned int cpu_power, gpu_power, max_cpu_pwr, max_gpu_pwr;

#ifdef ATM_USES_PPM
	max_cpu_pwr = mt_ppm_thermal_get_max_power() + 1;
#else
	max_cpu_pwr = 3000;
#endif
	max_gpu_pwr = mt_gpufreq_get_max_power() + 1;
	cpu_power = apthermolmt_get_cpu_power_limit();
	gpu_power = apthermolmt_get_gpu_power_limit();
	cpu_power = (cpu_power == 0x7FFFFFFF || cpu_power == 0) ?
							max_cpu_pwr : cpu_power;

	cpu_power = (cpu_power < max_cpu_pwr) ? cpu_power : max_cpu_pwr;
	gpu_power = (gpu_power == 0x7FFFFFFF || gpu_power == 0) ?
							max_gpu_pwr : gpu_power;

	gpu_power = (gpu_power < max_gpu_pwr) ? gpu_power : max_gpu_pwr;
	cpu_power = (max_cpu_pwr - cpu_power)*100/max_cpu_pwr;
	gpu_power = (max_gpu_pwr - gpu_power)*100/max_gpu_pwr;
	seq_printf(m, "%d,%d\n", cpu_power, gpu_power);

	return 0;
}

static int tscpu_ttpct_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_ttpct, NULL);
}

static const struct file_operations mtktscpu_ttpct_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_ttpct_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
int tscpu_get_cpu_temp_met(enum mtk_thermal_sensor_cpu_id_met id)
{
	unsigned long flags;
	int ret;

	if (id < 0 || id >= MTK_THERMAL_SENSOR_CPU_COUNT)
		return -127000;

	if (id == ATM_CPU_LIMIT)
		return (adaptive_cpu_power_limit != 0x7FFFFFFF) ?
						adaptive_cpu_power_limit : 0;

	if (id == ATM_GPU_LIMIT)
		return (adaptive_gpu_power_limit != 0x7FFFFFFF) ?
						adaptive_gpu_power_limit : 0;

	tscpu_met_lock(&flags);
	if (a_tscpu_all_temp[id] == 0) {
		tscpu_met_unlock(&flags);
		return -127000;
	}
	ret = a_tscpu_all_temp[id];
	tscpu_met_unlock(&flags);
	return ret;
}
EXPORT_SYMBOL(tscpu_get_cpu_temp_met);
#endif

#if defined(CONFIG_MACH_MT6755)
#if 0
static int thermal_auxadc_get_data(int times, int channel)
{
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0;

	pr_notice("Thermal_auxadc_get_data\n");

	if (IMM_IsAdcInitReady() == 0) {
		pr_notice("[%s]: AUXADC is not ready\n", __func__);
		return 0;
	}

	for (i = 0; i < times; i++) {
		ret_value = IMM_GetOneChannelValue(channel, data, &ret_temp);
		pr_notice("[%s]: raw%d= %d\n", __func__, i,
								ret_temp);
		ret += ret_temp;
	}

	ret = ret / times;
	return ret;
}
#endif
/*Patch to pause thermal controller and turn off auxadc GC.
 *For mt6755 only
 */
static void tscpu_thermal_pause(void)
{
	int cnt = 0;
	int temp = 0;

	aee_rr_rec_thermal_status(TSCPU_PAUSE);

#if defined(CONFIG_ARCH_MT6797)
	/* disable periodic temp measurement on sensor 0~2 */
	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

	do {
		temp = (readl(THAHBST0) >> 16);
		if ((cnt + 1) % 10 == 0)
			pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n", temp, cnt,
								__LINE__);

		udelay(50);
		cnt++;
	} while (temp != 0x0 && cnt < 50);
#else
	thermal_pause_all_periodoc_temp_sensing();	/* TEMPMSRCTL1 */

	do {
		temp = (readl(THAHBST0) >> 16);
		if ((cnt + 1) % 10 == 0)
			pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n", temp, cnt,
								__LINE__);

		udelay(2);
		cnt++;
	} while (temp != 0x0 && cnt < 50);
#endif
	/* disable periodic temp measurement on sensor 0~2 */
	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */
}

static void tscpu_thermal_release(void)
{
	int temp = 0;
	int cnt = 0;

	aee_rr_rec_thermal_status(TSCPU_RELEASE);

	WARN_ON_ONCE((~__raw_readl((infracfg_ao_base + 0x0094)) & 0x400)
								!= 0x400);
	/*
	 *  TS_CON1 default is 0x30, this is buffer off
	 *  we should turn on this buffer berore we use thermal sensor,
	 *  or this buffer off will let TC read a very small value from auxadc
	 *  and this small value will trigger thermal reboot
	 */
	temp = readl(TS_CONFIGURE);

	/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	temp &= ~(TS_TURN_OFF);
	mt_reg_sync_writel(temp, TS_CONFIGURE);	/* read abb need */
	/* RG_TS2AUXADC < set from 2'b11 to 2'b00
	 *when resume.wait 100uS than turn on thermal controller.
	 */
	udelay(200);

	WARN_ON_ONCE((readl(TS_CONFIGURE) & TS_TURN_OFF) != 0x0);

	/*thermal_auxadc_get_data(2, 11);*/
	/* must release before start */
	thermal_release_all_periodoc_temp_sensing();

	tscpu_fast_initial_sw_workaround();

#if defined(CONFIG_ARCH_MT6797)
	/* disable periodic temp measurement on sensor 0~2 */
	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

	do {
		temp = (readl(THAHBST0) >> 16);
		if ((cnt + 1) % 10 == 0)
			pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n", temp, cnt,
								__LINE__);

		udelay(50);
		cnt++;
	} while (temp != 0x0 && cnt < 50);
#else
	thermal_pause_all_periodoc_temp_sensing();	/* TEMPMSRCTL1 */

	do {
		temp = (readl(THAHBST0) >> 16);
		if ((cnt + 1) % 10 == 0)
			pr_notice("THAHBST0 = 0x%x, cnt = %d, %d\n", temp, cnt,
								__LINE__);

		udelay(2);
		cnt++;
	} while (temp != 0x0 && cnt < 50);

	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */
#endif

	tscpu_thermal_initial_all_bank();

	/* must release before start */
	thermal_release_all_periodoc_temp_sensing();
}
#endif

static DEFINE_SPINLOCK(temp_valid_spinlock);
static void temp_valid_lock(unsigned long *flags)
{
	spin_lock_irqsave(&temp_valid_spinlock, *flags);
}

static void temp_valid_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&temp_valid_spinlock, *flags);
}

static void check_all_temp_valid(void)
{
	int i, j, raw;

	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		for (j = 0; j < tscpu_g_bank[i].ts_number; j++) {
			raw = tscpu_bank_ts_r[i][tscpu_g_bank[i].ts[j].type];

			if (raw == THERMAL_INIT_VALUE)
				return;	/* The temperature is not valid. */
		}
	}

	g_is_temp_valid = 1;
}

int tscpu_is_temp_valid(void)
{
	int is_valid = 0;
	unsigned long flags;

	temp_valid_lock(&flags);
	if (g_is_temp_valid == 0)
		check_all_temp_valid();

	is_valid = g_is_temp_valid;
	temp_valid_unlock(&flags);

	return is_valid;
}

static void read_all_bank_temperature(void)
{
	int i = 0;
	int j = 0;
	unsigned long flags;

	for (i = 0; i < ARRAY_SIZE(tscpu_g_bank); i++) {
		mt_ptp_lock(&flags);

		tscpu_switch_bank(i);
		for (j = 0; j < tscpu_g_bank[i].ts_number; j++)
			tscpu_thermal_read_bank_temp(i,
						tscpu_g_bank[i].ts[j].type, j);

		mt_ptp_unlock(&flags);
	}

	tscpu_is_temp_valid();
}

void tscpu_update_tempinfo(void)
{
	unsigned long flags, i;
	ktime_t now;

	now = ktime_get();
	if (g_tc_resume == 0)
		read_all_bank_temperature();
	else if (g_tc_resume == 2) /* resume ready */
		g_tc_resume = 0;

#if (CONFIG_THERMAL_AEE_RR_REC == 1)
	for (i = 0; i < TS_ENUM_MAX; i++)
		aee_rr_rec_thermal_temp(i, get_immediate_tsX[i]() / 1000);
	aee_rr_rec_thermal_status(TSCPU_NORMAL);
	aee_rr_rec_thermal_ktime(ktime_to_us(now));
#endif

#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET

	tscpu_met_lock(&flags);

	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);
	for (i = 0; i < TS_ENUM_MAX; i++)
		a_tscpu_all_temp[i] = get_immediate_tsX[i]();

	tscpu_met_unlock(&flags);

	if (g_pThermalSampler != NULL)
		g_pThermalSampler();

#endif


#if 0
	pr_debug("\n\n");
	tscpu_printk("\n %s, T=%d,%d,%d,%d,%d,%d\n", __func__,
				CPU_TS_MCU1_T, CPU_TS_MCU2_T, GPU_TS_MCU3_T,
				SOC_TS_MCU4_T, SOC_TS_MCU2_T, SOC_TS_MCU3_T);

	pr_debug("Bank 0 : CPU  (TS_MCU1 = %d,TS_MCU2 = %d)\n", CPU_TS_MCU1_T,
			CPU_TS_MCU2_T);
	pr_debug("Bank 1 : GPU  (TS_MCU3 = %d)\n", GPU_TS_MCU3_T);
	pr_debug("Bank 2 : SOC  (TS_MCU4 = %d,TS_MCU2 = %d,TS_MCU3 = %d)\n",
				SOC_TS_MCU4_T, SOC_TS_MCU2_T, SOC_TS_MCU3_T);

#endif
}

#if defined(FAST_RESPONSE_ATM)
DEFINE_SPINLOCK(timer_lock);
int is_worktimer_en = 1;
#endif

void tscpu_workqueue_cancel_timer(void)
{
#ifdef FAST_RESPONSE_ATM
	if (down_trylock(&sem_mutex))
		return;

	if (is_worktimer_en && thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
		tscpu_dprintk("[tTimer] workqueue stopping\n");
		spin_lock(&timer_lock);
		is_worktimer_en = 0;
		spin_unlock(&timer_lock);
	}

	up(&sem_mutex);
#else
	if (down_trylock(&sem_mutex))
		return;

	if (thz_dev) {
		cancel_delayed_work(&(thz_dev->poll_queue));
		isTimerCancelled = 1;
	}

	up(&sem_mutex);
#endif
}

void tscpu_workqueue_start_timer(void)
{
#ifdef FAST_RESPONSE_ATM
	if (!isTimerCancelled)
		return;


	if (down_trylock(&sem_mutex))
		return;

	if (!is_worktimer_en && thz_dev != NULL && interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
						&(thz_dev->poll_queue), 0);
		isTimerCancelled = 0;

		tscpu_dprintk("[tTimer] workqueue starting\n");
		spin_lock(&timer_lock);
		is_worktimer_en = 1;
		spin_unlock(&timer_lock);
	}

	up(&sem_mutex);
#else
	if (!isTimerCancelled)
		return;


	if (down_trylock(&sem_mutex))
		return;

	/* resume thermal framework polling when leaving deep idle */
	if (thz_dev != NULL && interval != 0) {
		mod_delayed_work(system_freezable_power_efficient_wq,
					&(thz_dev->poll_queue),
					round_jiffies(msecs_to_jiffies(1000)));
		isTimerCancelled = 0;
	}
	up(&sem_mutex);
#endif

}

static void tscpu_cancel_thermal_timer(void)
{
	/* stop thermal framework polling when entering deep idle */

#ifdef FAST_RESPONSE_ATM
	atm_cancel_hrtimer();
#endif
	tscpu_workqueue_cancel_timer();

#if defined(CONFIG_MACH_MT6755)
	/*Patch to pause thermal controller and turn off auxadc GC.
	 *For mt6755 only
	 */
	tscpu_thermal_pause();
#endif
}


static void tscpu_start_thermal_timer(void)
{

#ifdef FAST_RESPONSE_ATM
	atm_restart_hrtimer();
#else
	tscpu_workqueue_start_timer();
#endif

#if defined(CONFIG_MACH_MT6755)
	/*Patch to pause thermal controller and turn off auxadc GC.
	 * For mt6755 only
	 */
	tscpu_thermal_release();
#endif
}

static void init_thermal(void)
{
	int temp = 0;
	int cnt = 0;

#if (CONFIG_THERMAL_AEE_RR_REC == 1)
	_mt_thermal_aee_init();

	aee_rr_rec_thermal_status(TSCPU_INIT);
#endif

	tscpu_thermal_cal_prepare();
	tscpu_thermal_cal_prepare_2(0);

	tscpu_reset_thermal();

	/* thermal spm verification */
#if 0
	/* unmask bit21 for thermal wake up source */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~(1U << 21));
	tscpu_printk("SPM_SLEEP_WAKEUP_EVENT_MASK =0x%08x\n",
			spm_read(SPM_SLEEP_WAKEUP_EVENT_MASK));
#endif
	/*
	 *  TS_CON1 default is 0x30, this is buffer off
	 *   we should turn on this buffer berore we use thermal sensor,
	 *   or this buffer off will let TC read a very small value from auxadc
	 *  and this small value will trigger thermal reboot
	 */
	temp = readl(TS_CONFIGURE);

	/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	temp &= ~(TS_TURN_OFF);
	mt_reg_sync_writel(temp, TS_CONFIGURE);	/* read abb need */
	/* RG_TS2AUXADC < set from 2'b11 to 2'b00
	 *when resume.wait 100uS than turn on thermal controller.
	 */
	udelay(200);

	WARN_ON_ONCE((readl(TS_CONFIGURE) & TS_TURN_OFF) != 0x0);

	WARN_ON_ONCE(IMM_IsAdcInitReady() != 1);

	/*add this function to read all temp first to avoid
	 *  write TEMPPROTTC first will issue an fake signal to RGU
	 */
	tscpu_fast_initial_sw_workaround();

	while (cnt < 50) {
		temp = (readl(THAHBST0) >> 16);
		if ((cnt + 1) % 10 == 0)
			pr_debug("THAHBST0 = 0x%x,cnt=%d, %d\n", temp, cnt,
								__LINE__);

		if (temp == 0x0) {
			/* pause all periodoc temperature sensing point 0~2 */
			/* TEMPMSRCTL1 */
			thermal_pause_all_periodoc_temp_sensing();
			break;
		}
		udelay(2);
		cnt++;
	}
	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

	/* pr_debug(KERN_CRIT "cnt = %d, %d\n",cnt,__LINE__); */

	/*Normal initial */
	tscpu_thermal_initial_all_bank();

	/* TEMPMSRCTL1 must release before start */
	thermal_release_all_periodoc_temp_sensing();

	read_all_bank_temperature();
}

static void tscpu_create_fs(void)
{

	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscpu_dir = NULL;

	mtktscpu_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscpu_dir) {
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzcpu", 0664, mtktscpu_dir,
							&mtktscpu_fops);

		if (entry)
			proc_set_user(entry, uid, gid);

#if defined(CONFIG_ARCH_MT6753)
		/*For MT6753 PMIC 5A throttle patch*/
		entry = proc_create("tzcpu_cpufreq5A", 0664,
					mtktscpu_dir, &tzcpu_cpufreq5A_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif

		entry = proc_create("tzcpu_log", 0644, mtktscpu_dir,
							&mtktscpu_log_fops);

		entry = proc_create("thermlmt", 0444, NULL, &mtktscpu_opp_fops);

		entry = proc_create("ttpct", 0444, NULL, &mtktscpu_ttpct_fops);

		entry = proc_create("tzcpu_cal", 0400, mtktscpu_dir,
							&mtktscpu_cal_fops);

		entry = proc_create("tzcpu_read_temperature", 0444,
					mtktscpu_dir,
					&mtktscpu_read_temperature_fops);

		entry = proc_create("tzcpu_talking_flag", 0644, mtktscpu_dir,
						&mtktscpu_talking_flag_fops);

#if MTKTSCPU_FAST_POLLING
		entry = proc_create("tzcpu_fastpoll", 0664, mtktscpu_dir,
						&mtktscpu_fastpoll_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif				/* #if MTKTSCPU_FAST_POLLING */

		entry = proc_create("tzcpu_Tj_out_via_HW_pin", 0644,
							mtktscpu_dir,
							&mtktscpu_Tj_out_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#if THERMAL_GPIO_OUT_TOGGLE
		entry = proc_create("tzcpu_GPIO_out_monitor", 0644,
						mtktscpu_dir,
						&mtktscpu_GPIO_out_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)

		entry = proc_create("pmic_current_limit", 0644, mtktscpu_dir,
					&mtktscpu_pmic_current_limit_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

#endif
#if THERMAL_LT_SET_HPM
		entry = proc_create("tzcpu_ts4", 0664, mtktscpu_dir,
							&mtktscpu_ts4_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif
	}
}

/*must wait until AUXADC initial ready*/
static int tscpu_thermal_probe(struct platform_device *dev)
{
	int err = 0;

	tscpu_printk("thermal_prob\n");

	/*
	 *  default is dule mode(irq/reset),
	 *  if not to config this and hot happen,
	 *  system will reset after 30 secs
	 *
	 *  Thermal need to config to direct reset mode
	 *  this API provide by Weiqi Fu(RGU SW owner).
	 */
	if (get_io_reg_base() == 0)
		return 0;

#if !defined(CONFIG_MTK_CLKMGR)
	therm_main = devm_clk_get(&dev->dev, "therm-main");
	if (IS_ERR(therm_main)) {
		tscpu_printk("cannot get thermal clock.\n");
		return PTR_ERR(therm_main);
	}
	tscpu_dprintk("therm-main Ptr=%p", therm_main);
#endif

	tscpu_thermal_clock_on();
	init_thermal();

#if MTK_TS_CPU_RT
	{
		tscpu_dprintk("tscpu_register_thermal creates kthermp\n");
		ktp_thread_handle = kthread_create(ktp_thread, (void *)NULL,
								"kthermp");
		if (IS_ERR(ktp_thread_handle)) {
			ktp_thread_handle = NULL;
			tscpu_printk(
				"tscpu_register_thermal kthermp creation fails\n");
			goto err_unreg;
		}
		wake_up_process(ktp_thread_handle);
	}
#endif

#ifdef CONFIG_OF
	err = request_irq(thermal_irq_number,
				tscpu_thermal_all_bank_interrupt_handler,
				IRQF_TRIGGER_LOW, THERMAL_NAME, NULL);
	if (err)
		tscpu_warn("tscpu_init IRQ register fail\n");
#else
	err =
		request_irq(THERM_CTRL_IRQ_BIT_ID,
				tscpu_thermal_all_bank_interrupt_handler,
				IRQF_TRIGGER_LOW, THERMAL_NAME, NULL);
	if (err)
		tscpu_warn("tscpu_init IRQ register fail\n");
#endif

	tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);

#if THERMAL_GET_AHB_BUS_CLOCK
	thermal_get_AHB_clk_info();
#endif

	mtkTTimer_register("mtktscpu", tscpu_start_thermal_timer,
						tscpu_cancel_thermal_timer);

	return err;
}

#if defined(CONFIG_ARCH_MT6753)
/*For MT6753 PMIC 5A throttle patch*/
int isMT6753T(void)
{
	unsigned int cpu_spd_bond, efuse_spare2, is53T = 0;

	cpu_spd_bond = (get_devinfo_with_index(3) & _BITMASK_(2:0));
	efuse_spare2 = (get_devinfo_with_index(5) & _BITMASK_(21:20)) >> 20;

	switch (cpu_spd_bond) {
	case 0:
		if (efuse_spare2 == 3)
			is53T = 1;
		break;
	case 1:
	case 2:
		is53T = 1;
		break;
	default:
		break;
	}

	return is53T;
}
#endif

static int __init tscpu_init(void)
{
	int err = 0;

	tscpu_printk("%s\n", __func__);
	/*set init val*/
	tscpu_g_curr_temp = 0;
	tscpu_g_prev_temp = 0;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	thermal_6353_5A_status = 0;
#endif
	err = platform_driver_register(&mtk_thermal_driver);
	if (err) {
		tscpu_warn("thermal driver callback register failed..\n");
		return err;
	}

	err = tscpu_register_thermal();
	if (err) {
		tscpu_warn("tscpu_register_thermal fail\n");
		goto err_unreg;
	}

#if defined(CONFIG_ARCH_MT6753)
	/*For MT6753 PMIC 5A throttle patch*/
	if (isMT6753T() == 0)
		fast_polling_trip_temp = 40000;
#endif
	tscpu_create_fs();


	return 0;

err_unreg:
	return err;
}


static void __exit tscpu_exit(void)
{

	tscpu_dprintk("%s\n", __func__);

#if MTK_TS_CPU_RT
	if (ktp_thread_handle)
		kthread_stop(ktp_thread_handle);
#endif

	tscpu_unregister_thermal();

#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
	mt_thermalsampler_registerCB(NULL);
#endif

	mtkTTimer_unregister("mtktscpu");
}
module_init(tscpu_init);
module_exit(tscpu_exit);
