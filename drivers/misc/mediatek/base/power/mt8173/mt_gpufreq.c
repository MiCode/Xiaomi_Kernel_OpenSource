/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "[GPU_DVFS] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>

#include <asm/uaccess.h>

#include "mt_gpufreq.h"
#include "mt-plat/sync_write.h"
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mach/mt_freqhopping.h"
#endif
#include "mt_static_power.h"
#include "mach/mt_thermal.h"
#include "mach/upmu_sw.h"

static struct regulator *g_reg_vgpu;


/**************************************************
* GPU DVFS touch boost feature
***************************************************/
#define MT_GPUFREQ_INPUT_BOOST

#if defined(CONFIG_MTK_BATTERY_PROTECT)
/**************************************************
* Define low battery voltage support
***************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT

/**************************************************
* Define low battery volume support
***************************************************/
/* #define MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT */

/**************************************************
* Define oc support
***************************************************/
/* #define MT_GPUFREQ_OC_PROTECT */
#endif


#define GPU_DVFS_PTPOD_DISABLE_VOLT    GPU_DVFS_VOLT2
#define GPU_DVFS_DEFAULT_FREQ          GPU_DVFS_FREQ4

static sampler_func g_pFreqSampler;
static sampler_func g_pVoltSampler;

static gpufreq_power_limit_notify g_pGpufreq_power_limit_notify;
#ifdef MT_GPUFREQ_INPUT_BOOST
static gpufreq_input_boost_notify g_pGpufreq_input_boost_notify;
#endif

static gpufreq_mfgclock_notify g_pGpufreq_mfgclock_enable_notify;
static gpufreq_mfgclock_notify g_pGpufreq_mfgclock_disable_notify;

/***************************
* GPU Frequency Table
****************************/
/* 500MHz */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_0[] = {
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ3, GPU_DVFS_VOLT1, 0),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 1),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 2),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 3),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};
/* 600MHz */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_1[] = {
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 0),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 1),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 2),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ6, GPU_DVFS_VOLT2, 3),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};
/* 700MHz */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_2[] = {
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1, 0),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1, 1),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ4, GPU_DVFS_VOLT2, 2),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ5, GPU_DVFS_VOLT2, 3),
	GPUOP_FREQ_TBL(GPU_DVFS_FREQ7, GPU_DVFS_VOLT2, 4),
};

/**************************
* enable GPU DVFS count
***************************/
static int g_gpufreq_dvfs_disable_count;

static unsigned int g_cur_gpu_freq = GPU_DVFS_FREQ4;
static unsigned int g_cur_gpu_volt = GPU_DVFS_VOLT2;
static unsigned int g_cur_gpu_idx  = 0xFF;

static bool mt_gpufreq_ready;

/* In default settiing, freq_table[0] is max frequency, freq_table[num-1] is min frequency */
static unsigned int g_gpufreq_max_id;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id;
static unsigned int g_limited_min_id;

static bool         mt_gpufreq_debug;
static bool         mt_gpufreq_pause;
static bool         mt_gpufreq_keep_max_frequency_state;
static bool         mt_gpufreq_keep_opp_frequency_state;
static unsigned int mt_gpufreq_keep_opp_frequency;
static unsigned int mt_gpufreq_keep_opp_index;
static bool         mt_gpufreq_fixed_freq_volt_state;
static unsigned int mt_gpufreq_fixed_frequency;
static unsigned int mt_gpufreq_fixed_voltage;

static unsigned int mt_gpufreq_volt_enable_state;

#ifdef MT_GPUFREQ_INPUT_BOOST
static unsigned int mt_gpufreq_input_boost_state = 1;
#endif

static bool         g_limited_thermal_ignore_state;

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static bool g_limited_low_batt_volt_ignore_state;
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static bool g_limited_low_batt_volume_ignore_state;
#endif

#ifdef MT_GPUFREQ_OC_PROTECT
static bool g_limited_oc_ignore_state;
#endif

static unsigned int mt_gpufreq_dvfs_table_type;
static unsigned int mt_gpufreq_dvfs_mmpll_spd_bond;

static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);

static unsigned int mt_gpufreqs_num;
static struct mt_gpufreq_table_info *mt_gpufreqs;
static struct mt_gpufreq_table_info *mt_gpufreqs_default;
static struct mt_gpufreq_power_table_info *mt_gpufreqs_power;

static bool mt_gpufreq_ptpod_disable;
static int  mt_gpufreq_ptpod_disable_idx;

static void mt_gpu_clock_switch(unsigned int freq_new);
static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new);

#define GPU_DEFAULT_MAX_FREQ_MHZ    500
#define GPU_DEFAULT_TYPE    0

/*************************************************************************************
* Check GPU DVFS Efuse
**************************************************************************************/
static unsigned int mt_gpufreq_check_dvfs_efuse(void)
{
	unsigned int mmpll_spd_bond = 0, mmpll_spd_bond_2 = 0;
	unsigned int type = GPU_DEFAULT_TYPE;

	mmpll_spd_bond = (get_devinfo_with_index(3) >> 30) & 0x3;
	mmpll_spd_bond_2 = (get_devinfo_with_index(3) >> 23) & 0x1;
	mt_gpufreq_dvfs_mmpll_spd_bond = mmpll_spd_bond_2 << 2 | mmpll_spd_bond;

	pr_debug("mt_gpufreq_dvfs_mmpll_spd_bond = 0x%x ([2]=%x, [1:0]=%x)\n",
		mt_gpufreq_dvfs_mmpll_spd_bond, mmpll_spd_bond_2, mmpll_spd_bond);

	/*
	   No efuse,  use frequency in device tree to determine GPU table type!
	   device tree located in kernel\mediatek\mt8173\3.10\arch\arm64\boot\dts
	*/
	if (mt_gpufreq_dvfs_mmpll_spd_bond == 0) {
		static const struct of_device_id gpu_ids[] = {
			{ .compatible = "mediatek,mt8173-han" },
			{ /* sentinel */ }
		};

		struct device_node *node;
		unsigned int gpu_speed = 0;

		node = of_find_matching_node(NULL, gpu_ids);
		if (!node) {
			pr_debug("@%s: find GPU node failed\n", __func__);
			gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;
		} else {
			if (!of_property_read_u32(node, "clock-frequency", &gpu_speed))
				gpu_speed = gpu_speed / 1000 / 1000;
			else {
				pr_debug("@%s: missing clock-frequency property, use default GPU level\n", __func__);
				gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;
			}
		}
		pr_info("GPU clock-frequency from DT = %d MHz\n", gpu_speed);

		if (gpu_speed > 600)
			type = 2;
		else if (gpu_speed == 600)
			type = 1;
		else
			type = GPU_DEFAULT_TYPE;

		return type;
	}

	switch (mt_gpufreq_dvfs_mmpll_spd_bond) {
	case 0b011:
		type = 2;
		break;

	case 0b110:
		type = GPU_DEFAULT_TYPE;
		break;

	default:
		type = 1;
		break;
	}

	return type;
}

#ifdef MT_GPUFREQ_INPUT_BOOST
static struct task_struct *mt_gpufreq_up_task;

static int mt_gpufreq_input_boost_task(void *data)
{
	while (1) {
		if (NULL != g_pGpufreq_input_boost_notify) {
			pr_debug("mt_gpufreq_input_boost_task, g_pGpufreq_input_boost_notify\n");
			g_pGpufreq_input_boost_notify(g_gpufreq_max_id);
		}

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static void mt_gpufreq_input_event(struct input_handle *handle, unsigned int type,
			unsigned int code, int value)
{
	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_input_event, GPU DVFS not ready!\n");
		return;
	}

	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1) && (mt_gpufreq_input_boost_state == 1))	{
			pr_debug("mt_gpufreq_input_event, accept.\n");
			wake_up_process(mt_gpufreq_up_task);
	}
}

static int mt_gpufreq_input_connect(struct input_handler *handler,
			struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "gpufreq_ib";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void mt_gpufreq_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id mt_gpufreq_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler mt_gpufreq_input_handler = {
	.event		= mt_gpufreq_input_event,
	.connect	= mt_gpufreq_input_connect,
	.disconnect	= mt_gpufreq_input_disconnect,
	.name		= "gpufreq_ib",
	.id_table	= mt_gpufreq_ids,
};
#endif

static void mt_gpufreq_power_calculation(unsigned int oppidx, unsigned int temp)
{
#define GPU_ACT_REF_POWER               (653)    /* mW  */
#define GPU_ACT_REF_FREQ                (455000) /* KHz */
#define GPU_ACT_REF_VOLT                (1000)   /* mV  */

	unsigned int p_total = 0, p_dynamic = 0, p_leakage = 0, ref_freq = 0, ref_volt = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq  = GPU_ACT_REF_FREQ;
	ref_volt  = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
			((mt_gpufreqs[oppidx].gpufreq_khz * 100) / ref_freq) *
			((mt_gpufreqs[oppidx].gpufreq_volt * 100) / ref_volt) *
			((mt_gpufreqs[oppidx].gpufreq_volt * 100) / ref_volt) /
			(100 * 100 * 100);

#if 0
	p_leakage = mt_spower_get_leakage(MT_SPOWER_GPU, (mt_gpufreqs[oppidx].gpufreq_volt / 100), temp);
#endif
	p_total = p_dynamic + p_leakage;

	mt_gpufreqs_power[oppidx].gpufreq_khz = mt_gpufreqs[oppidx].gpufreq_khz;
	mt_gpufreqs_power[oppidx].gpufreq_power = p_total;

}

/* Set frequency and voltage at driver probe function */
static void mt_gpufreq_set_initial(unsigned int index)
{
	int volt;

	mutex_lock(&mt_gpufreq_lock);

	/* get default voltage settings */
	volt = regulator_get_voltage(g_reg_vgpu);

	mt_gpu_volt_switch(volt/1000, mt_gpufreqs[index].gpufreq_volt);
	mt_gpu_clock_switch(mt_gpufreqs[index].gpufreq_khz);

	g_cur_gpu_idx = mt_gpufreqs[index].gpufreq_idx;

	mutex_unlock(&mt_gpufreq_lock);
}

/* Set VGPU enable/disable when GPU clock be switched on/off */
unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	int ret = 0;
	unsigned int delay = 0;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		pr_debug("GPU DVFS not ready!\n");

		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if (enable == 1) {
		ret = regulator_enable(g_reg_vgpu);
		if (ret != 0) {
			pr_err("Failed to enable reg-vgpu: %d\n", ret);

			mutex_unlock(&mt_gpufreq_lock);
			return ret;
		}
	} else {
		ret = regulator_disable(g_reg_vgpu);
		if (ret != 0) {
			pr_err("Failed to disable reg-vgpu: %d\n", ret);

			mutex_unlock(&mt_gpufreq_lock);
			return ret;
		}
	}

	mt_gpufreq_volt_enable_state = enable;
	pr_debug("mt_gpufreq_voltage_enable_set, enable = %x\n", enable);

	delay = GPU_DVFS_VOLT_SETTLE_TIME(0, g_cur_gpu_volt);
	udelay(delay);

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_enable_set);

/************************************************
* DVFS enable API for PTPOD
*************************************************/
void mt_gpufreq_enable_by_ptpod(void)
{
	if (mt_gpufreq_ready == false) {
		pr_err("mt_gpufreq_enable_by_ptpod: GPU DVFS not ready!\n");
		return;
	}

	mt_gpufreq_ptpod_disable = false;
	pr_debug("mt_gpufreq enabled by ptpod\n");

	if (g_pGpufreq_mfgclock_disable_notify)
		g_pGpufreq_mfgclock_disable_notify();
	else
		pr_err("mt_gpufreq_enable_by_ptpod: no callback!\n");

	/* pmic auto mode: the variance of voltage is wide but saves more power. */
	regulator_set_mode(g_reg_vgpu, REGULATOR_MODE_NORMAL);

	if (regulator_get_mode(g_reg_vgpu) != REGULATOR_MODE_NORMAL)
		pr_err("Vgpu should be REGULATOR_MODE_NORMAL(%d), but mode = %d\n",
		     REGULATOR_MODE_NORMAL, regulator_get_mode(g_reg_vgpu));
}
EXPORT_SYMBOL(mt_gpufreq_enable_by_ptpod);

/************************************************
* DVFS disable API for PTPOD
*************************************************/
void mt_gpufreq_disable_by_ptpod(void)
{
	int i = 0, found = 0;

	if (mt_gpufreq_ready == false) {
		pr_err("mt_gpufreq_disable_by_ptpod: GPU DVFS not ready!\n");
		return;
	}

	mt_gpufreq_ptpod_disable = true;
	pr_debug("mt_gpufreq disabled by ptpod\n");

	for (i = mt_gpufreqs_num-1; i >= 0; i--) {
		if (mt_gpufreqs_default[i].gpufreq_volt == GPU_DVFS_PTPOD_DISABLE_VOLT) {
			found = 1;
			break;
		}
	}

	if (g_pGpufreq_mfgclock_enable_notify)
		g_pGpufreq_mfgclock_enable_notify();
	else
		pr_err("mt_gpufreq_disable_by_ptpod: no callback!\n");

	/* pmic PWM mode: the variance of voltage is narrow but consumes more power. */
	regulator_set_mode(g_reg_vgpu, REGULATOR_MODE_FAST);

	if (regulator_get_mode(g_reg_vgpu) != REGULATOR_MODE_FAST)
		pr_err("Vgpu should be REGULATOR_MODE_FAST(%d), but mode = %d\n",
		     REGULATOR_MODE_FAST, regulator_get_mode(g_reg_vgpu));

	if (found == 1)	{
		mt_gpufreq_ptpod_disable_idx = i;
		mt_gpufreq_target(i);
	} else {
		mt_gpufreq_ptpod_disable_idx = 2;
		mt_gpufreq_target(2);

		/* Force to DISABLE_VOLT for PTPOD */
		mutex_lock(&mt_gpufreq_lock);
		mt_gpu_volt_switch(g_cur_gpu_volt, GPU_DVFS_PTPOD_DISABLE_VOLT);
		mutex_unlock(&mt_gpufreq_lock);
	}
}
EXPORT_SYMBOL(mt_gpufreq_disable_by_ptpod);

/************************************************
* API to switch back default voltage setting for GPU PTPOD disabled
*************************************************/
void mt_gpufreq_return_default_DVS_by_ptpod(void)
{
	int i;

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_return_default_DVS_by_ptpod: GPU DVFS not ready!\n");
		return;
	}

	mutex_lock(&mt_gpufreq_lock);

	for (i = 0; i < mt_gpufreqs_num; i++) {
		mt_gpufreqs[i].gpufreq_volt = mt_gpufreqs_default[i].gpufreq_volt;
		pr_debug("mt_gpufreq_return_default_DVS_by_ptpod: mt_gpufreqs[%d].gpufreq_volt = %x\n",
			i, mt_gpufreqs[i].gpufreq_volt);
	}

	mt_gpu_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_idx].gpufreq_volt);

	mutex_unlock(&mt_gpufreq_lock);
}
EXPORT_SYMBOL(mt_gpufreq_return_default_DVS_by_ptpod);

/* Set voltage because PTP-OD modified voltage table by PMIC wrapper */
unsigned int mt_gpufreq_voltage_set_by_ptpod(unsigned int volt[], unsigned int array_size)
{
	int i;

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_voltage_set_by_ptpod: GPU DVFS not ready!\n");
		return -ENOSYS;
	}

	if (array_size > mt_gpufreqs_num) {
		pr_err("mt_gpufreq_voltage_set_by_ptpod: array_size = %d, Over-Boundary!\n", array_size);
		return -ENOSYS;
	}

	mutex_lock(&mt_gpufreq_lock);

	for (i = 0; i < array_size; i++) {
		mt_gpufreqs[i].gpufreq_volt = GPU_VOLT_TO_MV(volt[i]);
		pr_debug("mt_gpufreqs[%d].gpufreq_volt = %x\n", i, mt_gpufreqs[i].gpufreq_volt);
	}

	mt_gpu_volt_switch(g_cur_gpu_volt, mt_gpufreqs[g_cur_gpu_idx].gpufreq_volt);

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_set_by_ptpod);

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return mt_gpufreqs_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_get_freq_by_idx: GPU DVFS not ready!\n");
		return -ENOSYS;
	}

	if (idx < mt_gpufreqs_num) {
		pr_debug("mt_gpufreq_get_freq_by_idx: num = %d, frequency= %d\n",
			idx, mt_gpufreqs[idx].gpufreq_khz);

		return mt_gpufreqs[idx].gpufreq_khz;
	}

	pr_debug("mt_gpufreq_get_freq_by_idx: num = %d, NOT found! return 0!\n", idx);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

static void mt_setup_gpufreqs_power_table(int num)
{
	int i = 0, temp = 0;

	mt_gpufreqs_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);
	if (mt_gpufreqs_power == NULL)
		return;

	temp = get_immediate_ts1_wrap() / 1000;

	pr_debug("mt_setup_gpufreqs_power_table, temp = %d\n", temp);

	if ((temp < 0) || (temp > 125)) {
		pr_debug("mt_setup_gpufreqs_power_table, temp < 0 or temp > 125!\n");
		temp = 65;
	}

	for (i = 0; i < num; i++) {
		mt_gpufreq_power_calculation(i, temp);

		pr_debug("mt_gpufreqs_power[%d]: gpufreq_khz = %u, gpufreq_power = %u\n",
			i, mt_gpufreqs_power[i].gpufreq_khz, mt_gpufreqs_power[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_power, num);
#endif
}

/***********************************************
* register frequency table to gpufreq subsystem
************************************************/
static int mt_setup_gpufreqs_table(struct mt_gpufreq_table_info *freqs, int num)
{
	int i = 0;

	mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	mt_gpufreqs_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	if (mt_gpufreqs == NULL || mt_gpufreqs_default == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs[i].gpufreq_idx = freqs[i].gpufreq_idx;

		mt_gpufreqs_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs_default[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs_default[i].gpufreq_idx = freqs[i].gpufreq_idx;

		pr_debug("mt_gpufreqs[%d]: gpufreq_khz = %u, gpufreq_volt = %u, gpufreq_idx = %u\n",
			i, mt_gpufreqs[i].gpufreq_khz, mt_gpufreqs[i].gpufreq_volt, mt_gpufreqs[i].gpufreq_idx);
	}

	mt_gpufreqs_num = num;

	g_limited_max_id = 0;
	g_limited_min_id = mt_gpufreqs_num - 1;

	mt_setup_gpufreqs_power_table(num);

	return 0;
}

/**************************************
* check if maximum frequency is needed
***************************************/
static int mt_gpufreq_keep_max_freq(unsigned int freq_old, unsigned int freq_new)
{
	if (mt_gpufreq_keep_max_frequency_state == true)
		return 1;

	return 0;
}

/*****************************
* set GPU DVFS status
******************************/
int mt_gpufreq_state_set(int enabled)
{
	if (enabled) {
		if (!mt_gpufreq_pause) {
			pr_debug("gpufreq already enabled\n");
			return 0;
		}

		/*****************
		* enable GPU DVFS
		******************/
		g_gpufreq_dvfs_disable_count--;
		pr_debug("g_gpufreq_dvfs_disable_count = %d\n", g_gpufreq_dvfs_disable_count);

		/***********************************************
		* enable DVFS if no any module still disable it
		************************************************/
		if (g_gpufreq_dvfs_disable_count <= 0)
			mt_gpufreq_pause = false;
		else
			pr_debug("someone still disable gpufreq, cannot enable it\n");
	} else	{
		/******************
		* disable GPU DVFS
		*******************/
		g_gpufreq_dvfs_disable_count++;
		pr_debug("disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
			g_gpufreq_dvfs_disable_count);

		if (mt_gpufreq_pause) {
			pr_debug("gpufreq already disabled\n");
			return 0;
		}

		mt_gpufreq_pause = true;
	}

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_state_set);

static unsigned int mt_gpufreq_dds_calc(unsigned int khz)
{
	unsigned int dds = 0;

#if 0
	if ((khz >= 250250) && (khz <= 747500))
		dds = 0x0209A000 + ((khz - 250250) * 4 / 13000) * 0x2000;
	else if ((khz > 747500) && (khz <= 793000))
		dds = 0x010E6000 + ((khz - 747500) * 2 / 13000) * 0x2000;
	else {
		pr_err("target khz(%d) out of range!\n", khz);
		BUG();
	}
#else
	dds = khz*4;
#endif

	return dds;
}

static void mt_gpu_clock_switch(unsigned int freq_new)
{
	unsigned int dds;

	if (freq_new == g_cur_gpu_freq)
		return;

	dds = mt_gpufreq_dds_calc(freq_new);

#ifdef CONFIG_MTK_FREQ_HOPPING
	mt_dfs_mmpll(dds);
#endif

	g_cur_gpu_freq = freq_new;

	if (NULL != g_pFreqSampler)
		g_pFreqSampler(freq_new);

	pr_debug("mt_gpu_clock_switch, freq_new = %d (KHz)\n", freq_new);
}


static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new)
{
	unsigned int delay = 0;
	int ret;

	if (volt_new == g_cur_gpu_volt)
		return;

	ret = regulator_set_voltage(g_reg_vgpu,
			GPU_VOLT_TO_EXTBUCK_VAL(volt_new), GPU_VOLT_TO_EXTBUCK_MAXVAL(volt_new));
	if (ret != 0) {
		pr_err("mt_gpu_volt_switch: fail to set voltage 0x%X (%d -> %d)\n",
			ret, volt_old, volt_new);
		return;
	}

	delay = GPU_DVFS_VOLT_SETTLE_TIME(volt_old, volt_new);
	udelay(delay);

	g_cur_gpu_volt = volt_new;

	if (NULL != g_pVoltSampler)
		g_pVoltSampler(volt_new);

	pr_debug("mt_gpu_volt_switch, volt_new = %d (mV)\n", volt_new);
}


/*****************************************
* frequency ramp up and ramp down handler
******************************************/
/***********************************************************
* [note]
* 1. frequency ramp up need to wait voltage settle
* 2. frequency ramp down do not need to wait voltage settle
************************************************************/
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new, unsigned int volt_old, unsigned int volt_new)
{
	if (freq_new > freq_old) {
		mt_gpu_volt_switch(volt_old, volt_new);
		mt_gpu_clock_switch(freq_new);
	} else {
		mt_gpu_clock_switch(freq_new);
		mt_gpu_volt_switch(volt_old, volt_new);
	}
}


/**********************************
* gpufreq target callback function
***********************************/
/*************************************************
* [note]
* 1. handle frequency change request
* 2. call mt_gpufreq_set to set target frequency
**************************************************/
unsigned int mt_gpufreq_target(unsigned int idx)
{
	unsigned int target_freq, target_volt, target_idx;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		pr_debug("GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

	if (mt_gpufreq_volt_enable_state == 0) {
		pr_debug("mt_gpufreq_volt_enable_state == 0! return\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -ENOSYS;
	}

    /**********************************
    * look up for the target GPU OPP
    ***********************************/
	target_freq = mt_gpufreqs[idx].gpufreq_khz;
	target_volt = mt_gpufreqs[idx].gpufreq_volt;
	target_idx = idx;

    /************************************************
    * Thermal limit
    *************************************************/
	if (g_limited_max_id != 0) {
		if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz) {
			/* target_freq > limited_freq, need to adjust */
			target_freq = mt_gpufreqs[g_limited_max_id].gpufreq_khz;
			target_volt = mt_gpufreqs[g_limited_max_id].gpufreq_volt;
			target_idx = mt_gpufreqs[g_limited_max_id].gpufreq_idx;

			pr_debug("Limit! Thermal limit frequency %d\n",
				mt_gpufreqs[g_limited_max_id].gpufreq_khz);
		}
	}

    /**********************************
    * Check if need to keep max frequency
    ***********************************/
	if (mt_gpufreq_keep_max_freq(g_cur_gpu_freq, target_freq)) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		target_idx = mt_gpufreqs[g_gpufreq_max_id].gpufreq_idx;

		pr_debug("Keep MAX frequency %d\n", target_freq);
	}

    /************************************************
    * If /proc command keep opp frequency.
    *************************************************/
	if (mt_gpufreq_keep_opp_frequency_state == true) {
		target_freq = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz;
		target_volt = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_idx;

		pr_debug("Keep opp! opp frequency %d, opp voltage %d, opp idx %d\n",
			target_freq, target_volt, target_idx);
	}

    /************************************************
    * If /proc command fix the frequency.
    *************************************************/
	if (mt_gpufreq_fixed_freq_volt_state == true) {
		target_freq = mt_gpufreq_fixed_frequency;
		target_volt = mt_gpufreq_fixed_voltage;
		target_idx = 0;

		pr_debug("Fixed! fixed frequency %d, fixed voltage %d\n",
			target_freq, target_volt);
	}

    /************************************************
    * DVFS keep at max freq when PTPOD initial
    *************************************************/
	if (mt_gpufreq_ptpod_disable == true) {
		target_freq = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_khz;
		target_volt = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_volt;
		target_idx = mt_gpufreqs[mt_gpufreq_ptpod_disable_idx].gpufreq_idx;

		pr_debug("PTPOD disable dvfs, mt_gpufreq_ptpod_disable_idx = %d\n",
			mt_gpufreq_ptpod_disable_idx);
	}

    /************************************************
    * target frequency == current frequency, skip it
    *************************************************/
	if (g_cur_gpu_freq == target_freq) {
		mutex_unlock(&mt_gpufreq_lock);

		pr_debug("GPU frequency from %d KHz to %d KHz (skipped) due to same frequency\n",
			g_cur_gpu_freq, target_freq);
		return 0;
	}

	pr_debug("GPU current frequency %d KHz, target frequency %d KHz\n", g_cur_gpu_freq, target_freq);

    /******************************
    * set to the target frequency
    *******************************/
	mt_gpufreq_set(g_cur_gpu_freq, target_freq, g_cur_gpu_volt, target_volt);

	g_cur_gpu_idx = target_idx;

	mutex_unlock(&mt_gpufreq_lock);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_target);

#ifdef MT_GPUFREQ_OC_PROTECT
static unsigned int mt_gpufreq_oc_level;

#define MT_GPUFREQ_OC_LIMIT_FREQ_1                  GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_oc_limited_index_0; /* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_oc_limited_index_1;
static unsigned int mt_gpufreq_oc_limited_index;   /* Limited frequency index for oc */
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static unsigned int mt_gpufreq_low_battery_volume;

#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1     GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_low_bat_volume_limited_index_0; /* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_low_bat_volume_limited_index_1;
static unsigned int mt_gpufreq_low_batt_volume_limited_index;  /* Limited frequency index for low battery volume */
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static unsigned int mt_gpufreq_low_battery_level;

#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1       GPU_DVFS_FREQ4
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2       GPU_DVFS_FREQ4
static unsigned int mt_gpufreq_low_bat_volt_limited_index_0; /* unlimit frequency, index = 0. */
static unsigned int mt_gpufreq_low_bat_volt_limited_index_1;
static unsigned int mt_gpufreq_low_bat_volt_limited_index_2;
static unsigned int mt_gpufreq_low_batt_volt_limited_index;  /* Limited frequency index for low battery voltage */
#endif

#define MT_GPUFREQ_POWER_LIMITED_MAX_NUM            4
#define MT_GPUFREQ_THERMAL_LIMITED_INDEX            0
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMITED_INDEX      1
#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMITED_INDEX    2
#define MT_GPUFREQ_OC_LIMITED_INDEX                 3

/* limit frequency index array */
static unsigned int mt_gpufreq_power_limited_index_array[MT_GPUFREQ_POWER_LIMITED_MAX_NUM];

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
static void mt_gpufreq_power_throttle_protect(void)
{
	int i = 0;
	unsigned int limited_index = 0;

	/* Check lowest frequency in all limitation */
	for (i = 0; i < MT_GPUFREQ_POWER_LIMITED_MAX_NUM; i++) {
		if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index == 0)
			limited_index = mt_gpufreq_power_limited_index_array[i];
		else if (mt_gpufreq_power_limited_index_array[i] != 0 && limited_index != 0) {
			if (mt_gpufreq_power_limited_index_array[i] > limited_index)
				limited_index = mt_gpufreq_power_limited_index_array[i];
		}

		pr_debug("mt_gpufreq_power_limited_index_array[%d] = %d\n",
			i, mt_gpufreq_power_limited_index_array[i]);
	}

	g_limited_max_id = limited_index;
	pr_debug("Final limit frequency upper bound to id = %d, frequency = %d\n",
		g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	if (g_pGpufreq_power_limit_notify)
		g_pGpufreq_power_limit_notify(g_limited_max_id);
}

#ifdef MT_GPUFREQ_OC_PROTECT
/************************************************
* GPU frequency adjust interface for oc protect
*************************************************/
static void mt_gpufreq_oc_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	pr_info("mt_gpufreq_oc_protect, limited_index = %d\n", limited_index);

	mt_gpufreq_power_limited_index_array[MT_GPUFREQ_OC_LIMITED_INDEX] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_oc_callback(BATTERY_OC_LEVEL oc_level)
{
	pr_debug("mt_gpufreq_oc_callback: oc_level = %d\n", oc_level);

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_oc_callback, GPU DVFS not ready!\n");
		return;
	}

	if (g_limited_oc_ignore_state == true) {
		pr_debug("mt_gpufreq_oc_callback, g_limited_oc_ignore_state == true!\n");
		return;
	}

	mt_gpufreq_oc_level = oc_level;

	/* BATTERY_OC_LEVEL_1: >= 7A, BATTERY_OC_LEVEL_0: < 7A */
	if (oc_level == BATTERY_OC_LEVEL_1)	{
		if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_1) {
			mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_1;
			mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_1);
		}
	} else {  /* unlimit gpu */
		if (mt_gpufreq_oc_limited_index != mt_gpufreq_oc_limited_index_0) {
			mt_gpufreq_oc_limited_index = mt_gpufreq_oc_limited_index_0;
			mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_0);
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/************************************************
* GPU frequency adjust interface for low bat_volume protect
*************************************************/
static void mt_gpufreq_low_batt_volume_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	pr_info("mt_gpufreq_low_batt_volume_protect, limited_index = %d\n", limited_index);

	mt_gpufreq_power_limited_index_array[MT_GPUFREQ_LOW_BATT_VOLUME_LIMITED_INDEX] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);

}

void mt_gpufreq_low_batt_volume_callback(BATTERY_PERCENT_LEVEL low_battery_volume)
{
	pr_debug("mt_gpufreq_low_batt_volume_callback: low_battery_volume = %d\n", low_battery_volume);

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_low_batt_volume_callback, GPU DVFS not ready!\n");
		return;
	}

	if (g_limited_low_batt_volume_ignore_state == true) {
		pr_debug("mt_gpufreq_low_batt_volume_callback, g_limited_low_batt_volume_ignore_state == true!\n");
		return;
	}

	mt_gpufreq_low_battery_volume = low_battery_volume;

	/* LOW_BATTERY_VOLUME_1: <= 15%, LOW_BATTERY_VOLUME_0: >15% */
	if (low_battery_volume == BATTERY_PERCENT_LEVEL_1) {
		if (mt_gpufreq_low_batt_volume_limited_index != mt_gpufreq_low_bat_volume_limited_index_1) {
			mt_gpufreq_low_batt_volume_limited_index = mt_gpufreq_low_bat_volume_limited_index_1;
			mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_1);
		}
	} else {  /* unlimit gpu */
		if (mt_gpufreq_low_batt_volume_limited_index != mt_gpufreq_low_bat_volume_limited_index_0) {
			mt_gpufreq_low_batt_volume_limited_index = mt_gpufreq_low_bat_volume_limited_index_0;
			mt_gpufreq_low_batt_volume_protect(mt_gpufreq_low_bat_volume_limited_index_0);
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/************************************************
* GPU frequency adjust interface for low bat_volt protect
*************************************************/
static void mt_gpufreq_low_batt_volt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	pr_info("mt_gpufreq_low_batt_volt_protect, limited_index = %d\n", limited_index);

	mt_gpufreq_power_limited_index_array[MT_GPUFREQ_LOW_BATT_VOLT_LIMITED_INDEX] = limited_index;
	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

/******************************************************
* parameter: low_battery_level
*******************************************************/
void mt_gpufreq_low_batt_volt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	pr_debug("mt_gpufreq_low_batt_volt_callback: low_battery_level = %d\n", low_battery_level);

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_low_batt_volt_callback, GPU DVFS not ready!\n");
		return;
	}

	if (g_limited_low_batt_volt_ignore_state == true) {
		pr_debug("mt_gpufreq_low_batt_volt_callback, g_limited_low_batt_volt_ignore_state == true!\n");
		return;
	}

	mt_gpufreq_low_battery_level = low_battery_level;

	/* is_low_battery=1:need limit HW, is_low_battery=0:no limit
	  3.25V HW issue int and is_low_battery=1,
	  3.0V HW issue int and is_low_battery=2,
	  3.5V HW issue int and is_low_battery=0 */
	if (low_battery_level == LOW_BATTERY_LEVEL_1) {
		if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_1) {
			mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_1;
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_1);
		}
	} else if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_2) {
			mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_2;
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_2);
		}
	} else { /* unlimit gpu */
		if (mt_gpufreq_low_batt_volt_limited_index != mt_gpufreq_low_bat_volt_limited_index_0) {
			mt_gpufreq_low_batt_volt_limited_index = mt_gpufreq_low_bat_volt_limited_index_0;
			mt_gpufreq_low_batt_volt_protect(mt_gpufreq_low_bat_volt_limited_index_0);
		}
	}
}

#endif

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;
	unsigned int found = 0;

	mutex_lock(&mt_gpufreq_power_lock);

	if (mt_gpufreq_ready == false) {
		pr_debug("mt_gpufreq_thermal_protect, GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (mt_gpufreqs_num == 0) {
		pr_debug("mt_gpufreq_thermal_protect, mt_gpufreqs_num == 0!\n");
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (g_limited_thermal_ignore_state == true) {
		pr_debug("mt_gpufreq_thermal_protect, g_limited_thermal_ignore_state == true!\n");
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (limited_power == 0)
		mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX] = 0;
	else {
		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs_power[i].gpufreq_power <= limited_power) {
				limited_freq = mt_gpufreqs_power[i].gpufreq_khz;
				found = 1;
				break;
			}
		}

		if (found == 0)
			limited_freq = mt_gpufreqs[mt_gpufreqs_num - 1].gpufreq_khz;

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
				mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX] = i;
				break;
			}
		}
	}

	pr_info("Thermal limit frequency upper bound to id = %d, limited_power = %d\n",
		mt_gpufreq_power_limited_index_array[MT_GPUFREQ_THERMAL_LIMITED_INDEX], limited_power);

	mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);


/************************************************
* return current GPU thermal limit index
*************************************************/
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	return g_limited_max_id;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_index);

/************************************************
* return current GPU thermal limit frequency
*************************************************/
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	return mt_gpufreqs[g_limited_max_id].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/************************************************
* return current GPU frequency index
*************************************************/
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	return g_cur_gpu_idx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/************************************************
* return current GPU frequency
*************************************************/
unsigned int mt_gpufreq_get_cur_freq(void)
{
	return g_cur_gpu_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/************************************************
* return current GPU voltage
*************************************************/
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return g_cur_gpu_volt;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

#ifdef MT_GPUFREQ_INPUT_BOOST
/************************************************
* register / unregister GPU input boost notifiction CB
*************************************************/
void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB)
{
	g_pGpufreq_input_boost_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_input_boost_notify_registerCB);
#endif

/************************************************
* register / unregister GPU power limit notifiction CB
*************************************************/
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	g_pGpufreq_power_limit_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

/************************************************
* register / unregister set GPU freq CB
*************************************************/
void mt_gpufreq_setfreq_registerCB(sampler_func pCB)
{
	g_pFreqSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setfreq_registerCB);

/************************************************
* register / unregister set GPU volt CB
*************************************************/
void mt_gpufreq_setvolt_registerCB(sampler_func pCB)
{
	g_pVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setvolt_registerCB);

/************************************************
* for ptpod used to open gpu external/internal power.
*************************************************/
void mt_gpufreq_mfgclock_notify_registerCB(gpufreq_mfgclock_notify pEnableCB, gpufreq_mfgclock_notify pDisableCB)
{
	g_pGpufreq_mfgclock_enable_notify = pEnableCB;
	g_pGpufreq_mfgclock_disable_notify = pDisableCB;
}
EXPORT_SYMBOL(mt_gpufreq_mfgclock_notify_registerCB);

bool mt_gpufreq_dvfs_ready(void)
{
	return mt_gpufreq_ready;
}
EXPORT_SYMBOL(mt_gpufreq_dvfs_ready);


/***************************
* show current touch boost status
****************************/
static int mt_gpufreq_input_boost_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_input_boost_state == 1)
		seq_puts(m, "gpufreq debug enabled\n");
	else
		seq_puts(m, "gpufreq debug disabled\n");

	return 0;
}

/***********************
* enable touch boost
************************/
static ssize_t mt_gpufreq_input_boost_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &debug)) {
		if (debug == 0)
			mt_gpufreq_input_boost_state = 0;
		else if (debug == 1)
			mt_gpufreq_input_boost_state = 1;
		else
			pr_err("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		pr_err("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}

/***************************
* show current debug status
****************************/
static int mt_gpufreq_debug_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_debug)
		seq_puts(m, "gpufreq debug enabled\n");
	else
		seq_puts(m, "gpufreq debug disabled\n");

	return 0;
}

/***********************
* enable debug message
************************/
static ssize_t mt_gpufreq_debug_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &debug)) {
		if (debug == 0)
			mt_gpufreq_debug = 0;
		else if (debug == 1)
			mt_gpufreq_debug = 1;
		else
			pr_err("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		pr_err("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/****************************
* show current limited by low batt volume
*****************************/
static int mt_gpufreq_limited_oc_ignore_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_oc_ignore_state = %d\n",
		g_limited_max_id, g_limited_oc_ignore_state);

	return 0;
}

/**********************************
* limited for low batt volume protect
***********************************/
static ssize_t mt_gpufreq_limited_oc_ignore_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &ignore)) {
		if (ignore == 1) {
			g_limited_oc_ignore_state = true;
			g_limited_max_id = 0;
		} else if (ignore == 0)
			g_limited_oc_ignore_state = false;
		else
			pr_err("bad argument!! please provide the maximum limited power\n");
	} else
		pr_err("bad argument!! please provide the maximum limited power\n");

	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/****************************
* show current limited by low batt volume
*****************************/
static int mt_gpufreq_limited_low_batt_volume_ignore_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volume_ignore_state = %d\n",
		g_limited_max_id, g_limited_low_batt_volume_ignore_state);

	return 0;
}

/**********************************
* limited for low batt volume protect
***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volume_ignore_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &ignore)) {
		if (ignore == 1) {
			g_limited_low_batt_volume_ignore_state = true;
			g_limited_max_id = 0;
		} else if (ignore == 0)
			g_limited_low_batt_volume_ignore_state = false;
		else
			pr_err("bad argument!! please provide the maximum limited power\n");
	} else
		pr_err("bad argument!! please provide the maximum limited power\n");

	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/****************************
* show current limited by low batt volt
*****************************/
static int mt_gpufreq_limited_low_batt_volt_ignore_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volt_ignore_state = %d\n",
		g_limited_max_id, g_limited_low_batt_volt_ignore_state);

	return 0;
}

/**********************************
* limited for low batt volt protect
***********************************/
static ssize_t mt_gpufreq_limited_low_batt_volt_ignore_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &ignore)) {
		if (ignore == 1) {
			g_limited_low_batt_volt_ignore_state = true;
			g_limited_max_id = 0;
		} else if (ignore == 0)
			g_limited_low_batt_volt_ignore_state = false;
		else
			pr_err("bad argument!! please provide the maximum limited power\n");
	} else
		pr_err("bad argument!! please provide the maximum limited power\n");

	return count;
}
#endif

/****************************
* show current limited by thermal
*****************************/
static int mt_gpufreq_limited_thermal_ignore_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_thermal_ignore_state = %d\n",
		g_limited_max_id, g_limited_thermal_ignore_state);

	return 0;
}

/**********************************
* limited for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_thermal_ignore_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &ignore)) {
		if (ignore == 1) {
			g_limited_thermal_ignore_state = true;
			g_limited_max_id = 0;
		} else if (ignore == 0)
			g_limited_thermal_ignore_state = false;
		else
			pr_err("bad argument!! please provide the maximum limited power\n");
	} else
		pr_err("bad argument!! please provide the maximum limited power\n");

	return count;
}

/****************************
* show current limited power
*****************************/
static int mt_gpufreq_limited_power_read(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, thermal want to limit frequency = %d\n",
		g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	return 0;
}

/**********************************
* limited power for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_power_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &power))
		mt_gpufreq_thermal_protect(power);
	else
		pr_err("bad argument!! please provide the maximum limited power\n");

	return count;
}

/******************************
* show current GPU DVFS stauts
*******************************/
static int mt_gpufreq_state_read(struct seq_file *m, void *v)
{
	if (!mt_gpufreq_pause)
		seq_puts(m, "GPU DVFS enabled\n");
	else
		seq_puts(m, "GPU DVFS disabled\n");

	return 0;
}

/****************************************
* set GPU DVFS stauts by sysfs interface
*****************************************/
static ssize_t mt_gpufreq_state_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int enabled = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &enabled)) {
		if (enabled == 1) {
			mt_gpufreq_keep_max_frequency_state = false;
			mt_gpufreq_state_set(1);
		} else if (enabled == 0) {
			/* Keep MAX frequency when GPU DVFS disabled. */
			mt_gpufreq_keep_max_frequency_state = true;
			mt_gpufreq_target(g_gpufreq_max_id);
			mt_gpufreq_state_set(0);
		} else
			pr_err("bad argument!! argument should be \"1\" or \"0\"\n");
	} else
		pr_err("bad argument!! argument should be \"1\" or \"0\"\n");

	return count;
}

/********************
* show GPU OPP table
*********************/
static int mt_gpufreq_opp_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		seq_printf(m, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
		seq_printf(m, "idx = %d\n", mt_gpufreqs[i].gpufreq_idx);
	}

	return 0;
}

/********************
* show GPU power table
*********************/
static int mt_gpufreq_power_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", mt_gpufreqs_power[i].gpufreq_khz);
		seq_printf(m, "power = %d\n", mt_gpufreqs_power[i].gpufreq_power);
	}

	return 0;
}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_opp_freq_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_keep_opp_frequency_state) {
		seq_puts(m, "gpufreq keep opp frequency enabled\n");
		seq_printf(m, "freq = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq keep opp frequency disabled\n");

	return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_opp_freq_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int i = 0;
	int fixed_freq = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &fixed_freq)) {
		if (fixed_freq == 0)
			mt_gpufreq_keep_opp_frequency_state = false;
		else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (fixed_freq == mt_gpufreqs[i].gpufreq_khz) {
					mt_gpufreq_keep_opp_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1)	{
				mt_gpufreq_keep_opp_frequency_state = true;
				mt_gpufreq_keep_opp_frequency = fixed_freq;

				mt_gpufreq_target(mt_gpufreq_keep_opp_index);
			}
		}
	} else
		pr_err("bad argument!! should be [enable fixed_freq fixed_volt]\n");

	return count;
}


/********************
* show variable dump
*********************/
static int mt_gpufreq_var_dump_read(struct seq_file *m, void *v)
{
	int i = 0;

	seq_printf(m, "g_cur_gpu_freq = %d, g_cur_gpu_volt = %d, g_cur_gpu_idx = %d\n",
		g_cur_gpu_freq, g_cur_gpu_volt, g_cur_gpu_idx);

	seq_printf(m, "g_limited_max_id = %d\n", g_limited_max_id);
	for (i = 0; i < MT_GPUFREQ_POWER_LIMITED_MAX_NUM; i++)
		seq_printf(m, "mt_gpufreq_power_limited_index_array[%d] = %d\n",
			i, mt_gpufreq_power_limited_index_array[i]);

	seq_printf(m, "mt_gpufreq_volt_enable_state = %d\n", mt_gpufreq_volt_enable_state);
	seq_printf(m, "mt_gpufreq_dvfs_table_type = %d\n", mt_gpufreq_dvfs_table_type);
	seq_printf(m, "mt_gpufreq_ptpod_disable_idx = %d\n", mt_gpufreq_ptpod_disable_idx);

	return 0;
}

/***************************
* show current voltage enable status
****************************/
static int mt_gpufreq_volt_enable_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_volt_enable_state)
		seq_puts(m, "gpufreq voltage enabled\n");
	else
		seq_puts(m, "gpufreq voltage disabled\n");

	return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_volt_enable_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int enable = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &enable)) {
		if (enable == 0)
			mt_gpufreq_voltage_enable_set(0);
		else if (enable == 1)
			mt_gpufreq_voltage_enable_set(1);
		else
			pr_err("bad argument!! should be [enable fixed_freq fixed_volt]\n");
	} else
		pr_err("bad argument!! should be [enable fixed_freq fixed_volt]\n");

	return count;
}

/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_fixed_freq_volt_read(struct seq_file *m, void *v)
{
	if (mt_gpufreq_fixed_freq_volt_state) {
		seq_puts(m, "gpufreq fixed frequency enabled\n");
		seq_printf(m, "fixed frequency = %d\n", mt_gpufreq_fixed_frequency);
		seq_printf(m, "fixed voltage = %d\n", mt_gpufreq_fixed_voltage);
	} else
		seq_puts(m, "gpufreq fixed frequency disabled\n");

	return 0;
}

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_fixed_freq_volt_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int fixed_freq = 0;
	int fixed_volt = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		if ((fixed_freq == 0) || (fixed_volt == 0))
			mt_gpufreq_fixed_freq_volt_state = false;
		else {
			if ((fixed_freq >= GPU_DVFS_MIN_FREQ) && (fixed_freq <= GPU_DVFS_MAX_FREQ)) {
				mt_gpufreq_fixed_frequency = fixed_freq;
				mt_gpufreq_fixed_voltage = fixed_volt;
				mt_gpufreq_fixed_freq_volt_state = true;

				mt_gpufreq_target(0);
			} else
				pr_err("bad argument!! should be [fixed_freq fixed_volt]\n");
		}
	} else
		pr_err("bad argument!! should be [fixed_freq fixed_volt]\n");

	return count;
}

/* early suspend / late resume interface */
static void mt_gpufreq_lcm_status_switch(int onoff)
{
	pr_info("@%s: LCM is %s\n", __func__, (onoff) ? "on" : "off");
}

static int mt_gpufreq_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* skip if it's not a blank event */
	if (event != FB_EVENT_BLANK)
		return 0;

	if (evdata == NULL)
		return 0;

	if (evdata->data == NULL)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		mt_gpufreq_lcm_status_switch(1);
		break;
	/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		mt_gpufreq_lcm_status_switch(0);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block mt_gpufreq_fb_notifier = {
	.notifier_call = mt_gpufreq_fb_notifier_callback,
};

static int mt_gpufreq_pm_restore_early(struct device *dev)
{
	return 0;
}

static int mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
	int ret, i = 0, init_idx = 0;
#ifdef MT_GPUFREQ_INPUT_BOOST
	int rc;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO-1};
#endif

	if (pdev->dev.of_node) {
		g_reg_vgpu = devm_regulator_get(&pdev->dev, "reg-vgpu");
	} else {
		pr_err("mt_gpufreq_pdrv_probe: no of_node\n");
		return -ENOSYS;
	}

	if (IS_ERR(g_reg_vgpu)) {
		ret = PTR_ERR(g_reg_vgpu);
		return ret;
	}

	mt_gpufreq_dvfs_table_type = mt_gpufreq_check_dvfs_efuse();

    /* setup gpufreq table */
	if (mt_gpufreq_dvfs_table_type == 2)
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_2, ARRAY_SIZE(mt_gpufreq_opp_tbl_2));
	else if (mt_gpufreq_dvfs_table_type == 1)
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_1, ARRAY_SIZE(mt_gpufreq_opp_tbl_1));
	else
		mt_setup_gpufreqs_table(mt_gpufreq_opp_tbl_0, ARRAY_SIZE(mt_gpufreq_opp_tbl_0));

	mt_gpufreq_volt_enable_state = 0;

    /* setup initial frequency */
	if (mt_gpufreqs[0].gpufreq_khz >= GPU_DVFS_DEFAULT_FREQ) {
		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz == GPU_DVFS_DEFAULT_FREQ) {
				init_idx = i;
				break;
			}
		}
	} else
		init_idx = 0;

	mt_gpufreq_set_initial(init_idx);
	mt_gpufreq_ready = true;

#ifdef MT_GPUFREQ_INPUT_BOOST
	mt_gpufreq_up_task = kthread_create(mt_gpufreq_input_boost_task, NULL, "mt_gpufreq_input_boost_task");
	if (IS_ERR(mt_gpufreq_up_task))
		return PTR_ERR(mt_gpufreq_up_task);

	sched_setscheduler_nocheck(mt_gpufreq_up_task, SCHED_FIFO, &param);
	get_task_struct(mt_gpufreq_up_task);

	rc = input_register_handler(&mt_gpufreq_input_handler);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1) {
			mt_gpufreq_low_bat_volt_limited_index_1 = i;
			break;
		}
	}

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2) {
			mt_gpufreq_low_bat_volt_limited_index_2 = i;
			break;
		}
	}

	register_low_battery_notify(&mt_gpufreq_low_batt_volt_callback, LOW_BATTERY_PRIO_GPU);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1) {
			mt_gpufreq_low_bat_volume_limited_index_1 = i;
			break;
		}
	}

	register_battery_percent_notify(&mt_gpufreq_low_batt_volume_callback, BATTERY_PERCENT_PRIO_GPU);
#endif

#ifdef MT_GPUFREQ_OC_PROTECT
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == MT_GPUFREQ_OC_LIMIT_FREQ_1) {
			mt_gpufreq_oc_limited_index_1 = i;
			break;
		}
	}

	register_battery_oc_notify(&mt_gpufreq_oc_callback, BATTERY_OC_PRIO_GPU);
#endif

	/* Register early suspend / late resume notify */
	if (fb_register_client(&mt_gpufreq_fb_notifier))
		pr_err("@%s: register FB client failed!\n", __func__);

	return 0;
}

/***************************************
* this function should never be called
****************************************/
static int mt_gpufreq_pdrv_remove(struct platform_device *pdev)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	input_unregister_handler(&mt_gpufreq_input_handler);

	kthread_stop(mt_gpufreq_up_task);
	put_task_struct(mt_gpufreq_up_task);
#endif

	kfree(mt_gpufreqs);
	mt_gpufreqs = NULL;

	kfree(mt_gpufreqs_power);
	mt_gpufreqs_power = NULL;

	if (g_reg_vgpu)
		devm_regulator_put(g_reg_vgpu);

	return 0;
}

static const struct of_device_id mtgpufreq_of_match[] = {
	{
		.compatible = "mediatek,mt8173-gpufreq",
	},
	{
	}
};
MODULE_DEVICE_TABLE(of, mtgpufreq_of_match);

static const struct dev_pm_ops mt_gpufreq_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.restore_early = mt_gpufreq_pm_restore_early,
};


static struct platform_driver mt_gpufreq_pdrv = {
	.probe = mt_gpufreq_pdrv_probe,
	.remove = mt_gpufreq_pdrv_remove,
	.driver = {
		.name = "mt-gpufreq",
		.of_match_table = of_match_ptr(mtgpufreq_of_match),
		.pm = &mt_gpufreq_pm_ops,
		.owner = THIS_MODULE,
	},
};

static int mt_gpufreq_input_boost_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_input_boost_read, NULL);
}

static const struct file_operations mt_gpufreq_input_boost_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_input_boost_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_input_boost_write,
	.release = single_release,
};

static int mt_gpufreq_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_debug_read, NULL);
}

static const struct file_operations mt_gpufreq_debug_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_debug_write,
	.release = single_release,
};

static int mt_gpufreq_limited_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_power_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_power_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_power_write,
	.release = single_release,
};

#ifdef MT_GPUFREQ_OC_PROTECT
static int mt_gpufreq_limited_oc_ignore_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_oc_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_oc_ignore_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_oc_ignore_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_oc_ignore_write,
	.release = single_release,
};
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static int mt_gpufreq_limited_low_batt_volume_ignore_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_low_batt_volume_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_low_batt_volume_ignore_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_low_batt_volume_ignore_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_low_batt_volume_ignore_write,
	.release = single_release,
};
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static int mt_gpufreq_limited_low_batt_volt_ignore_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_low_batt_volt_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_low_batt_volt_ignore_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_low_batt_volt_ignore_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_low_batt_volt_ignore_write,
	.release = single_release,
};
#endif

static int mt_gpufreq_limited_thermal_ignore_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_thermal_ignore_read, NULL);
}

static const struct file_operations mt_gpufreq_limited_thermal_ignore_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_limited_thermal_ignore_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_limited_thermal_ignore_write,
	.release = single_release,
};

static int mt_gpufreq_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_state_read, NULL);
}

static const struct file_operations mt_gpufreq_state_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_state_write,
	.release = single_release,
};

static int mt_gpufreq_opp_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_opp_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_opp_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_power_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_power_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_power_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_power_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_opp_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_opp_freq_read, NULL);
}

static const struct file_operations mt_gpufreq_opp_freq_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_opp_freq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_opp_freq_write,
	.release = single_release,
};


static int mt_gpufreq_var_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_var_dump_read, NULL);
}

static const struct file_operations mt_gpufreq_var_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_var_dump_open,
	.read = seq_read,
};

static int mt_gpufreq_volt_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_volt_enable_read, NULL);
}

static const struct file_operations mt_gpufreq_volt_enable_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_volt_enable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_volt_enable_write,
	.release = single_release,
};

static int mt_gpufreq_fixed_freq_volt_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_fixed_freq_volt_read, NULL);
}

static const struct file_operations mt_gpufreq_fixed_freq_volt_fops = {
	.owner = THIS_MODULE,
	.open = mt_gpufreq_fixed_freq_volt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mt_gpufreq_fixed_freq_volt_write,
	.release = single_release,
};

/**********************************
* mediatek gpufreq initialization
***********************************/
static int __init mt_gpufreq_init(void)
{
	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_gpufreq_dir = NULL;
	int ret = 0;

	mt_gpufreq_dir = proc_mkdir("gpufreq", NULL);
	if (!mt_gpufreq_dir) {
		pr_err("mkdir /proc/gpufreq failed\n");
		return -ENOMEM;
	}

	mt_entry =
	proc_create("gpufreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_debug_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_debug failed\n");

	mt_entry =
	proc_create("gpufreq_limited_power", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_limited_power_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_limited_power failed\n");

#ifdef MT_GPUFREQ_OC_PROTECT
	mt_entry =
	proc_create("gpufreq_limited_oc_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_limited_oc_ignore_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_limited_oc_ignore failed\n");
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
	mt_entry =
	proc_create("gpufreq_limited_low_batt_volume_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_limited_low_batt_volume_ignore_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_limited_low_batt_volume_ignore failed\n");
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	mt_entry =
	proc_create("gpufreq_limited_low_batt_volt_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_limited_low_batt_volt_ignore_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_limited_low_batt_volt_ignore failed\n");
#endif

	mt_entry =
	proc_create("gpufreq_limited_thermal_ignore", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_limited_thermal_ignore_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_limited_thermal_ignore failed\n");

	mt_entry =
	proc_create("gpufreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_state_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_state failed\n");

	mt_entry =
	proc_create("gpufreq_opp_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_opp_dump_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_opp_dump failed\n");

	mt_entry =
	proc_create("gpufreq_power_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_power_dump_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_power_dump failed\n");

	mt_entry =
	proc_create("gpufreq_opp_freq", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_opp_freq_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_opp_freq failed\n");

	mt_entry =
	proc_create("gpufreq_var_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_var_dump_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_var_dump failed\n");

	mt_entry =
	proc_create("gpufreq_volt_enable", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
	    &mt_gpufreq_volt_enable_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_volt_enable failed\n");

	mt_entry =
	proc_create("gpufreq_fixed_freq_volt", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_fixed_freq_volt_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_fixed_freq_volt failed\n");

	mt_entry =
	proc_create("gpufreq_input_boost", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
		&mt_gpufreq_input_boost_fops);
	if (!mt_entry)
		pr_err("create /proc/gpufreq/gpufreq_input_boost failed\n");

	ret = platform_driver_register(&mt_gpufreq_pdrv);

	if (ret)
		pr_err("failed to register gpufreq driver\n");

	return ret;
}

static void __exit mt_gpufreq_exit(void)
{
	platform_driver_unregister(&mt_gpufreq_pdrv);
}

module_init(mt_gpufreq_init);
module_exit(mt_gpufreq_exit);
MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
