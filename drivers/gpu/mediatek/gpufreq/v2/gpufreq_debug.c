// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpufreq_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>

#include <gpufreq_v2.h>
#include <gpufreq_mssv.h>
#include <gpufreq_debug.h>
#include <gpufreq_history_debug.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static unsigned int g_stress_test;
static unsigned int g_debug_power_state;
static unsigned int g_debug_margin_mode;
static unsigned int g_test_mode;
static const struct gpufreq_shared_status *g_shared_status;
static DEFINE_MUTEX(gpufreq_debug_lock);

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
#if defined(CONFIG_PROC_FS)
static int gpufreq_status_proc_show(struct seq_file *m, void *v)
{
	unsigned long long power_time = 0;

	mutex_lock(&gpufreq_debug_lock);

	gpufreq_update_debug_opp_info();

	seq_printf(m,
		"[GPUFREQ-DEBUG] Current Status of GPUFREQ\n");
	seq_printf(m,
		"%-15s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
		"[GPU-OPP]",
		g_shared_status->cur_oppidx_gpu,
		g_shared_status->cur_fgpu,
		g_shared_status->cur_vgpu,
		g_shared_status->cur_vsram_gpu);
	seq_printf(m,
		"%-15s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
		"[GPU-REALOPP]",
		g_shared_status->cur_fmeter_fgpu,
		g_shared_status->cur_con1_fgpu,
		g_shared_status->cur_regulator_vgpu,
		g_shared_status->cur_regulator_vsram_gpu);
	seq_printf(m,
		"%-15s SegmentID: %d, WorkingOPPNum: %d, SignedOPPNum: %d\n",
		"[GPU-Segment]",
		g_shared_status->segment_id,
		g_shared_status->opp_num_gpu,
		g_shared_status->signed_opp_num_gpu);

	if (g_dual_buck) {
		seq_printf(m,
			"%-15s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
			"[STACK-OPP]",
			g_shared_status->cur_oppidx_stack,
			g_shared_status->cur_fstack,
			g_shared_status->cur_vstack,
			g_shared_status->cur_vsram_stack);
		seq_printf(m,
			"%-15s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
			"[STACK-REALOPP]",
			g_shared_status->cur_fmeter_fstack,
			g_shared_status->cur_con1_fstack,
			g_shared_status->cur_regulator_vstack,
			g_shared_status->cur_regulator_vsram_stack);
		seq_printf(m,
			"%-15s SegmentID: %d, WorkingOPPNum: %d, SignedOPPNum: %d\n",
			"[STACK-Segment]",
			g_shared_status->segment_id,
			g_shared_status->opp_num_stack,
			g_shared_status->signed_opp_num_stack);
	}

	power_time = g_shared_status->power_time_h;
	power_time = (power_time << 32) | g_shared_status->power_time_l;
	seq_printf(m,
		"%-15s PowerCount: %d, Active: %d, CG: %d, MTCMOS: %d, BUCK: %d, Time: %lld\n",
		"[Common-Power]",
		g_shared_status->power_count,
		g_shared_status->active_count,
		g_shared_status->cg_count,
		g_shared_status->mtcmos_count,
		g_shared_status->buck_count,
		power_time);
	seq_printf(m,
		"%-15s CeilingIndex: %d, Limiter: %d, Priority: %d\n",
		"[Common-LimitC]",
		g_shared_status->cur_ceiling,
		g_shared_status->cur_c_limiter,
		g_shared_status->cur_c_priority);
	seq_printf(m,
		"%-15s FloorIndex: %d, Limiter: %d, Priority: %d\n",
		"[Common-LimitF]",
		g_shared_status->cur_floor,
		g_shared_status->cur_f_limiter,
		g_shared_status->cur_f_priority);
	seq_printf(m,
		"%-15s DualBuck: %s, GPUEBSupport: %s, RandomOPP: %s\n",
		"[Common-Status]",
		g_dual_buck ? "True" : "False",
		g_gpueb_support ? "On" : "Off",
		g_stress_test ? "On" : "Off");
	seq_printf(m,
		"%-15s DVFSState: 0x%04x, ShaderPresent: 0x%08x\n",
		"[Common-Status]",
		g_shared_status->dvfs_state,
		g_shared_status->shader_present);
	seq_printf(m,
		"%-15s AgingMargin: %s, AVSMargin: %s, GPM1.0: %s, GPM3.0: %s\n",
		"[Common-Status]",
		g_shared_status->aging_margin ? "On" : "Off",
		g_shared_status->avs_margin ? "On" : "Off",
		g_shared_status->gpm1_mode ? "On" : "Off",
		g_shared_status->gpm3_mode ? "On" : "Off");
	seq_printf(m,
		"%-15s GPU_SB_Ver: 0x%04x, GPU_PTP_Ver: 0x%04x, TempCompensate: %s (%d'C)\n",
		"[Common-Status]",
		g_shared_status->sb_version,
		g_shared_status->ptp_version,
		g_shared_status->temp_compensate ? "On" : "Off",
		g_shared_status->temperature);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t gpufreq_status_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0, value = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%8x", &value) == 1) {
		ret = gpufreq_set_test_mode(value);
		if (!ret)
			g_test_mode = FEAT_ENABLE;
		else
			g_test_mode = FEAT_DISABLE;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

static int gpu_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_num = g_shared_status->opp_num_gpu;
	opp_table = g_shared_status->working_table_gpu;
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, posdiv: %d, margin: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].margin, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_num = g_shared_status->opp_num_stack;
	opp_table = g_shared_status->working_table_stack;
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, posdiv: %d, margin: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].margin, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int gpu_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_test_mode) {
		opp_num = g_shared_status->signed_opp_num_gpu;
		opp_table = g_shared_status->signed_table_gpu;
	} else {
		opp_num = g_shared_status->opp_num_gpu;
		opp_table = g_shared_status->working_table_gpu;
	}
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d*] freq: %d, volt: %d, vsram: %d, posdiv: %d, margin: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].margin, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_test_mode) {
		opp_num = g_shared_status->signed_opp_num_stack;
		opp_table = g_shared_status->signed_table_stack;
	} else {
		opp_num = g_shared_status->opp_num_stack;
		opp_table = g_shared_status->working_table_stack;
	}
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d*] freq: %d, volt: %d, vsram: %d, posdiv: %d, margin: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].margin, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int limit_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpuppm_limit_info *limit_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	limit_table = g_shared_status->limit_table;
	if (!limit_table) {
		GPUFREQ_LOGE("fail to get limit table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	seq_printf(m, "%4s %15s %11s %10s %8s %11s %11s\n",
		"[id]", "[name]", "[priority]",
		"[ceiling]", "[floor]", "[c_enable]", "[f_enable]");

	for (i = 0; i < LIMIT_NUM; i++) {
		seq_printf(m, "%4d %15s %11d %10d %8d %11d %11d\n",
			i, limit_table[i].name,
			limit_table[i].priority,
			limit_table[i].ceiling,
			limit_table[i].floor,
			limit_table[i].c_enable,
			limit_table[i].f_enable);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static ssize_t limit_table_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], cmd[64];
	unsigned int len = 0;
	int limiter = 0, ceiling = 0, floor = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%6s %2d %2d %2d", cmd, &limiter, &ceiling, &floor) == 4) {
		if (sysfs_streq(cmd, "set")) {
			ret = gpufreq_set_limit(TARGET_DEFAULT, limiter, ceiling, floor);
			if (ret)
				GPUFREQ_LOGE("fail to set debug limit index (%d)", ret);
		}
		else if (sysfs_streq(cmd, "switch")) {
			ret = gpufreq_switch_limit(TARGET_DEFAULT, limiter, ceiling, floor);
			if (ret)
				GPUFREQ_LOGE("fail to switch debug limit option (%d)", ret);
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept OPP index */
static int fix_target_opp_index_proc_show(struct seq_file *m, void *v)
{
	unsigned int dvfs_state = DVFS_FREE;
	int fixed_idx = -1;

	mutex_lock(&gpufreq_debug_lock);

	dvfs_state = g_shared_status->dvfs_state;
	if (g_dual_buck)
		fixed_idx = g_shared_status->cur_oppidx_stack;
	else
		fixed_idx = g_shared_status->cur_oppidx_gpu;

	if (dvfs_state & DVFS_DEBUG_KEEP)
		seq_printf(m, "[GPUFREQ-DEBUG] fix OPP index: %d\n", fixed_idx);
	else
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep OPP index
 * -1: free run
 * others: OPP index to be kept
 */
static ssize_t fix_target_opp_index_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;
	int value = 0, opp_num = -1;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "min")) {
		if (g_dual_buck)
			opp_num = g_shared_status->opp_num_stack;
		else
			opp_num = g_shared_status->opp_num_gpu;
		if (opp_num > 0) {
			value = opp_num - 1;
			ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, value);
			if (ret)
				GPUFREQ_LOGE("fail to fix OPP index (%d)", ret);
		}
	} else if (sscanf(buf, "%2d", &value) == 1) {
		ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, value);
		if (ret)
			GPUFREQ_LOGE("fail to fix OPP index (%d)", ret);
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept freq and volt */
static int fix_custom_freq_volt_proc_show(struct seq_file *m, void *v)
{
	unsigned int dvfs_state = DVFS_FREE;
	unsigned int fixed_freq = 0, fixed_volt = 0;

	mutex_lock(&gpufreq_debug_lock);

	dvfs_state = g_shared_status->dvfs_state;
	if (g_dual_buck) {
		fixed_freq = g_shared_status->cur_fstack;
		fixed_volt = g_shared_status->cur_vstack;
	} else {
		fixed_freq = g_shared_status->cur_fgpu;
		fixed_volt = g_shared_status->cur_vgpu;
	}

	if (dvfs_state & DVFS_DEBUG_KEEP)
		seq_printf(m, "[GPUFREQ-DEBUG] fix freq: %d and volt: %d\n",
			fixed_freq, fixed_volt);
	else
		seq_puts(m, "[GPUFREQ-DEBUG] fixed freq and volt are disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep freq and volt
 * 0 0: free run
 * others others: freq and volt to be kept
 */
static ssize_t fix_custom_freq_volt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;
	unsigned int fixed_freq = 0, fixed_volt = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%7d %6d", &fixed_freq, &fixed_volt) == 2) {
		ret = gpufreq_fix_custom_freq_volt(TARGET_DEFAULT, fixed_freq, fixed_volt);
		if (ret)
			GPUFREQ_LOGE("fail to fix freq and volt (%d)", ret);
	}

	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current power state */
static int mfgsys_power_control_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_debug_power_state)
		seq_puts(m, "[GPUFREQ-DEBUG] MFGSYS is DBG_POWER_ON\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] MFGSYS is DBG_POWER_OFF\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: additionally control power of MFGSYS once to achieve AO state
 * power_on: power on once
 * power_off: power off once
 */
static ssize_t mfgsys_power_control_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "power_on") && g_debug_power_state == POWER_OFF) {
		ret = gpufreq_power_control(POWER_ON);
		if (ret < 0)
			GPUFREQ_LOGE("fail to power on MFGSYS (%d)", ret);
		else
			g_debug_power_state = POWER_ON;
	} else if (sysfs_streq(buf, "power_off") && g_debug_power_state == POWER_ON) {
		ret = gpufreq_power_control(POWER_OFF);
		if (ret < 0)
			GPUFREQ_LOGE("fail to power off MFGSYS (%d)", ret);
		else
			g_debug_power_state = POWER_OFF;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of OPP stress test */
static int opp_stress_test_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_stress_test)
		seq_puts(m, "[GPUFREQ-DEBUG] OPP stress test is enabled\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] OPP stress test is disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: enable OPP stress test by setting random OPP index
 * enable: start stress test
 * disable: stop stress test
 */
static ssize_t opp_stress_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		ret = gpufreq_set_stress_test(true);
		if (ret)
			GPUFREQ_LOGE("fail to enable stress test (%d)", ret);
		else
			g_stress_test = true;
	} else if (sysfs_streq(buf, "disable")) {
		ret = gpufreq_set_stress_test(false);
		if (ret)
			GPUFREQ_LOGE("fail to disable stress test (%d)", ret);
		else
			g_stress_test = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current info of aging*/
static int asensor_info_proc_show(struct seq_file *m, void *v)
{
	struct gpufreq_asensor_info asensor_info = {};

	mutex_lock(&gpufreq_debug_lock);

	asensor_info = g_shared_status->asensor_info;

	seq_printf(m,
		"Aging table index: %d, MOST_AGRRESIVE_AGING_TABLE_ID: %d\n",
		asensor_info.aging_table_idx, asensor_info.aging_table_idx_agrresive);
	seq_printf(m,
		"efuse1(0x%08x): 0x%08x, efuse2(0x%08x): 0x%08x\n",
		asensor_info.efuse1_addr, asensor_info.efuse1,
		asensor_info.efuse2_addr, asensor_info.efuse2);
	seq_printf(m,
		"efuse3(0x%08x): 0x%08x, efuse4(0x%08x): 0x%08x\n",
		asensor_info.efuse3_addr, asensor_info.efuse3,
		asensor_info.efuse4_addr, asensor_info.efuse4);
	seq_printf(m,
		"a_t0_efuse1: %d, a_t0_efuse2: %d, a_t0_efuse3: %d, a_t0_efuse4: %d\n",
		asensor_info.a_t0_efuse1, asensor_info.a_t0_efuse2,
		asensor_info.a_t0_efuse3, asensor_info.a_t0_efuse4);
	seq_printf(m,
		"a_tn_sensor1: %d, a_tn_sensor2: %d, a_tn_sensor3: %d, a_tn_sensor4: %d\n",
		asensor_info.a_tn_sensor1, asensor_info.a_tn_sensor2,
		asensor_info.a_tn_sensor3, asensor_info.a_tn_sensor4);
	seq_printf(m,
		"a_diff1: %d, a_diff2: %d, a_diff3: %d, a_diff4: %d\n",
		asensor_info.a_diff1, asensor_info.a_diff2,
		asensor_info.a_diff3, asensor_info.a_diff4);
	seq_printf(m,
		"tj_max: %d, lvts5_0_y_temperature: %d, leakage_power: %d\n",
		asensor_info.tj_max, asensor_info.lvts5_0_y_temperature,
		asensor_info.leakage_power);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static int mfgsys_config_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_adj_info *aging_table_gpu = NULL;
	const struct gpufreq_adj_info *aging_table_stack = NULL;
	const struct gpufreq_adj_info *avs_table_gpu = NULL;
	const struct gpufreq_adj_info *avs_table_stack = NULL;
	int adj_num = GPUFREQ_MAX_ADJ_NUM, i = 0;

	mutex_lock(&gpufreq_debug_lock);

	aging_table_gpu = g_shared_status->aging_table_gpu;
	aging_table_stack = g_shared_status->aging_table_stack;
	avs_table_gpu = g_shared_status->avs_table_gpu;
	avs_table_stack = g_shared_status->avs_table_stack;

	seq_printf(m, "%-7s AgingSensor: %s, AgingLoad: %s, AgingMargin: %s\n",
		"[AGING]",
		g_shared_status->asensor_enable ? "On" : "Off",
		g_shared_status->aging_load ? "On" : "Off",
		g_shared_status->aging_margin ? "On" : "Off");
	seq_printf(m, "%-7s AVS: %s, AVSMargin: %s\n",
		"[AVS]",
		g_shared_status->avs_enable ? "On" : "Off",
		g_shared_status->avs_margin ? "On" : "Off");
	seq_printf(m, "%-7s GPM1.0: %s, GPM3.0: %s\n",
		"[GPM]",
		g_shared_status->gpm1_mode ? "On" : "Off",
		g_shared_status->gpm3_mode ? "On" : "Off");

	if (aging_table_gpu && avs_table_gpu) {
		seq_puts(m, "\n[##*] [TOP Vaging] [##*] [TOP Vavs]\n");
		for (i = 0; i < adj_num; i++) {
			seq_printf(m, "[%02d*] %12d [%02d*] %10d\n",
				aging_table_gpu[i].oppidx, aging_table_gpu[i].volt,
				avs_table_gpu[i].oppidx, avs_table_gpu[i].volt);
		}
	}
	if (aging_table_stack && avs_table_stack) {
		seq_puts(m, "\n[##*] [STK Vaging] [##*] [STK Vavs]\n");
		for (i = 0; i < adj_num; i++) {
			seq_printf(m, "[%02d*] %12d [%02d*] %10d\n",
				aging_table_stack[i].oppidx, aging_table_stack[i].volt,
				avs_table_stack[i].oppidx, avs_table_stack[i].volt);
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t mfgsys_config_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], cmd[32], conf[32];
	unsigned int len = 0;
	enum gpufreq_feat_mode mode = FEAT_DISABLE;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%10s %6s", cmd, conf) == 2) {
		/* parse mode */
		if (sysfs_streq(cmd, "enable"))
			mode = FEAT_ENABLE;
		else if (sysfs_streq(cmd, "disable"))
			mode = FEAT_DISABLE;
		else if (sysfs_streq(cmd, "force_dump"))
			mode = DFD_FORCE_DUMP;
		else
			goto done;

		/* parse config */
		if (sysfs_streq(conf, "margin")) {
			/* only allow enable/disable once */
			if (g_debug_margin_mode ^ mode) {
				gpufreq_set_margin_mode(mode);
				g_debug_margin_mode = mode;
			}
		} else if (sysfs_streq(conf, "gpm1"))
			gpufreq_set_gpm_mode(1, mode);
		else if (sysfs_streq(conf, "dfd"))
			gpufreq_set_dfd_mode(mode);
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

#if GPUFREQ_MSSV_TEST_MODE
static int mssv_test_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	seq_printf(m, "%-8s DVFSState: 0x%04x\n",
		"[Common]",
		g_shared_status->dvfs_state);
	seq_printf(m, "%-8s Freq: %7d, Volt: %7d, Vsram: %7d\n",
		"[GPU]",
		g_shared_status->cur_fgpu,
		g_shared_status->cur_vgpu,
		g_shared_status->cur_vsram_gpu);
	seq_printf(m, "%-8s Freq: %7d, Volt: %7d, Vsram: %7d\n",
		"[STACK]",
		g_shared_status->cur_fstack,
		g_shared_status->cur_vstack,
		g_shared_status->cur_vsram_stack);
	seq_printf(m, "%-8s STACK_SEL(0x%08x[31]): %d, DEL_SEL(0x%08x[0]): %d\n",
		"[SEL]",
		g_shared_status->reg_stack_sel.addr,
		(g_shared_status->reg_stack_sel.val & BIT(31)) ? 1 : 0,
		g_shared_status->reg_del_sel.addr,
		(g_shared_status->reg_del_sel.val & BIT(0)) ? 1 : 0);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t mssv_test_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64], cmd[32];
	unsigned int len = 0, val = 0;
	unsigned int target = TARGET_MSSV_INVALID;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%8s %7d", cmd, val) == 2) {
		if (sysfs_streq(cmd, "fgpu"))
			target = TARGET_MSSV_FGPU;
		else if (sysfs_streq(cmd, "vgpu"))
			target = TARGET_MSSV_VGPU;
		else if (sysfs_streq(cmd, "fstack"))
			target = TARGET_MSSV_FSTACK;
		else if (sysfs_streq(cmd, "vstack"))
			target = TARGET_MSSV_VSTACK;
		else if (sysfs_streq(cmd, "vsram"))
			target = TARGET_MSSV_VSRAM;
		else if (sysfs_streq(cmd, "stacksel"))
			target = TARGET_MSSV_STACK_SEL;
		else if (sysfs_streq(cmd, "delsel"))
			target = TARGET_MSSV_DEL_SEL;
		else {
			GPUFREQ_LOGE("invalid MSSV cmd: %s", cmd);
			ret = GPUFREQ_EINVAL;
			goto done_unlock;
		}
		ret = gpufreq_mssv_commit(target, val);
		if (ret)
			GPUFREQ_LOGE("fail to commit: %s, val: %d (%d)",
				cmd, val, ret);
	}

done_unlock:
	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}
#endif /* GPUFREQ_MSSV_TEST_MODE */

/* PROCFS : initialization */
PROC_FOPS_RW(gpufreq_status);
PROC_FOPS_RO(gpu_working_opp_table);
PROC_FOPS_RO(gpu_signed_opp_table);
PROC_FOPS_RO(stack_working_opp_table);
PROC_FOPS_RO(stack_signed_opp_table);
PROC_FOPS_RO(asensor_info);
PROC_FOPS_RW(limit_table);
PROC_FOPS_RW(fix_target_opp_index);
PROC_FOPS_RW(fix_custom_freq_volt);
PROC_FOPS_RW(mfgsys_power_control);
PROC_FOPS_RW(mfgsys_config);
PROC_FOPS_RW(opp_stress_test);
#if GPUFREQ_MSSV_TEST_MODE
PROC_FOPS_RW(mssv_test);
#endif /* GPUFREQ_MSSV_TEST_MODE */

static int gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
		PROC_ENTRY(gpufreq_status),
		PROC_ENTRY(gpu_working_opp_table),
		PROC_ENTRY(gpu_signed_opp_table),
		PROC_ENTRY(asensor_info),
		PROC_ENTRY(limit_table),
		PROC_ENTRY(fix_target_opp_index),
		PROC_ENTRY(fix_custom_freq_volt),
		PROC_ENTRY(mfgsys_power_control),
		PROC_ENTRY(opp_stress_test),
		PROC_ENTRY(mfgsys_config),
#if GPUFREQ_MSSV_TEST_MODE
		PROC_ENTRY(mssv_test),
#endif /* GPUFREQ_MSSV_TEST_MODE */
	};

	const struct pentry dualbuck_entries[] = {
		PROC_ENTRY(stack_working_opp_table),
		PROC_ENTRY(stack_signed_opp_table),
	};

	dir = proc_mkdir("gpufreqv2", NULL);
	if (!dir) {
		GPUFREQ_LOGE("fail to create /proc/gpufreqv2 (ENOMEM)");
		return GPUFREQ_ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			GPUFREQ_LOGE("fail to create /proc/gpufreqv2/%s",
				default_entries[i].name);
	}

	if (g_dual_buck){
		for (i = 0; i < ARRAY_SIZE(dualbuck_entries); i++) {
			if (!proc_create(dualbuck_entries[i].name, 0660,
				dir, dualbuck_entries[i].fops))
				GPUFREQ_LOGE("fail to create /proc/gpufreqv2/%s",
					dualbuck_entries[i].name);
		}
	}

	return GPUFREQ_SUCCESS;
}
#endif /* CONFIG_PROC_FS */

void gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support,
	const struct gpufreq_shared_status *shared_status)
{
	int ret = GPUFREQ_SUCCESS;

	g_dual_buck = dual_buck;
	g_gpueb_support = gpueb_support;
	g_debug_power_state = POWER_OFF;
	g_debug_margin_mode = FEAT_ENABLE;
	/* always enable test mode when AP mode */
	if (!g_gpueb_support)
		g_test_mode = FEAT_ENABLE;
	/* take every info of mfgsys from shared status */
	if (shared_status)
		g_shared_status = shared_status;
	else {
		GPUFREQ_LOGE("null gpufreq shared status: 0x%llx", shared_status);
		BUG_ON(1);
	}

#if defined(CONFIG_PROC_FS)
	ret = gpufreq_create_procfs();
	if (ret)
		GPUFREQ_LOGE("fail to create procfs (%d)", ret);

#endif /* CONFIG_PROC_FS */
#if defined(CONFIG_DEBUG_FS)
	ret = gpufreq_create_debugfs();
	if (ret)
		GPUFREQ_LOGE("fail to create debugfs (%d)", ret);
#endif

}
