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
#include <gpufreq_debug.h>
#include <gpu_misc.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static unsigned int g_stress_test_enable;
static unsigned int g_aging_enable;
static unsigned int g_gpm_enable;
static unsigned int g_debug_power_state;
static unsigned int g_test_mode;
static struct gpufreq_debug_status g_debug_gpu;
static struct gpufreq_debug_status g_debug_stack;
static DEFINE_MUTEX(gpufreq_debug_lock);

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
#if defined(CONFIG_PROC_FS)
static int gpufreq_status_proc_show(struct seq_file *m, void *v)
{
	struct gpufreq_debug_opp_info gpu_opp_info = {};
	struct gpufreq_debug_limit_info gpu_limit_info = {};
	struct gpufreq_debug_opp_info stack_opp_info = {};
	struct gpufreq_debug_limit_info stack_limit_info = {};

	mutex_lock(&gpufreq_debug_lock);

	gpu_opp_info = gpufreq_get_debug_opp_info(TARGET_GPU);
	gpu_limit_info = gpufreq_get_debug_limit_info(TARGET_GPU);
	if (g_dual_buck) {
		stack_opp_info = gpufreq_get_debug_opp_info(TARGET_STACK);
		stack_limit_info = gpufreq_get_debug_limit_info(TARGET_STACK);
	}

	seq_printf(m,
		"[GPUFREQ-DEBUG] Current Status of GPUFREQ\n");
	seq_printf(m,
		"%-15s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
		"[GPU-OPP]",
		gpu_opp_info.cur_oppidx,
		gpu_opp_info.cur_freq,
		gpu_opp_info.cur_volt,
		gpu_opp_info.cur_vsram);
	seq_printf(m,
		"%-15s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
		"[GPU-REALOPP]",
		gpu_opp_info.fmeter_freq,
		gpu_opp_info.con1_freq,
		gpu_opp_info.regulator_volt,
		gpu_opp_info.regulator_vsram);
	seq_printf(m,
		"%-15s PowerCount: %d, CG: %d, MTCMOS: %d, BUCK: %d\n",
		"[GPU-Power]",
		gpu_opp_info.power_count,
		gpu_opp_info.cg_count,
		gpu_opp_info.mtcmos_count,
		gpu_opp_info.buck_count);
	seq_printf(m,
		"%-15s SegmentID: %d, WorkingOPPNum: %d, SignedOPPNum: %d\n",
		"[GPU-Segment]",
		gpu_opp_info.segment_id,
		gpu_opp_info.opp_num,
		gpu_opp_info.signed_opp_num);
	seq_printf(m,
		"%-15s CeilingIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPU-LimitC]",
		gpu_limit_info.ceiling,
		gpu_limit_info.c_limiter,
		gpu_limit_info.c_priority);
	seq_printf(m,
		"%-15s FloorIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPU-LimitF]",
		gpu_limit_info.floor,
		gpu_limit_info.f_limiter,
		gpu_limit_info.f_priority);

	if (g_dual_buck) {
		seq_printf(m,
			"%-15s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
			"[STACK-OPP]",
			stack_opp_info.cur_oppidx,
			stack_opp_info.cur_freq,
			stack_opp_info.cur_volt,
			stack_opp_info.cur_vsram);
		seq_printf(m,
			"%-15s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
			"[STACK-REALOPP]",
			stack_opp_info.fmeter_freq,
			stack_opp_info.con1_freq,
			stack_opp_info.regulator_volt,
			stack_opp_info.regulator_vsram);
		seq_printf(m,
			"%-15s PowerCount: %d, CG: %d, MTCMOS: %d, BUCK: %d\n",
			"[STACK-Power]",
			stack_opp_info.power_count,
			stack_opp_info.cg_count,
			stack_opp_info.mtcmos_count,
			stack_opp_info.buck_count);
		seq_printf(m,
			"%-15s SegmentID: %d, WorkingOPPNum: %d, SignedOPPNum: %d\n",
			"[STACK-Segment]",
			stack_opp_info.segment_id,
			stack_opp_info.opp_num,
			stack_opp_info.signed_opp_num);
		seq_printf(m,
			"%-15s CeilingIndex: %d, Limiter: %d, Priority: %d\n",
			"[STACK-LimitC]",
			stack_limit_info.ceiling,
			stack_limit_info.c_limiter,
			stack_limit_info.c_priority);
		seq_printf(m,
			"%-15s FloorIndex: %d, Limiter: %d, Priority: %d\n",
			"[STACK-LimitF]",
			stack_limit_info.floor,
			stack_limit_info.f_limiter,
			stack_limit_info.f_priority);
	}

	/* common info takes from GPU as it always exists */
	seq_printf(m,
		"%-15s Dual Buck: %s, GPUEB Support: %s\n",
		"[Common-Status]",
		g_dual_buck ? "True" : "False",
		g_gpueb_support ? "On" : "Off");
	seq_printf(m,
		"%-15s DVFSState: 0x%04x, ShaderPresent: 0x%08x\n",
		"[Common-Status]",
		gpu_opp_info.dvfs_state,
		gpu_opp_info.shader_present);
	seq_printf(m,
		"%-15s Aging: %s, AVS: %s, StressTest: %s, GPM1.0: %s\n",
		"[Common-Status]",
		gpu_opp_info.aging_enable ? "Enable" : "Disable",
		gpu_opp_info.avs_enable ? "Enable" : "Disable",
		g_stress_test_enable ? "Enable" : "Disable",
		gpu_opp_info.gpm_enable ? "Enable" : "Disable");
	seq_printf(m,
		"%-15s GPU_SB_Ver: 0x%04x, GPU_PTP_Ver: 0x%04x, Temp_Compensate: %s\n",
		"[Common-Status]",
		gpu_opp_info.sb_version,
		gpu_opp_info.ptp_version,
		gpu_opp_info.temp_compensate ? "Enable" : "Disable");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static ssize_t gpufreq_status_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = GPUFREQ_SUCCESS;
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;

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
			g_test_mode = true;
		else
			g_test_mode = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

static int gpu_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_table = gpufreq_get_working_table(TARGET_GPU);
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	opp_num = g_debug_gpu.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, posdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].vaging, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_table = gpufreq_get_working_table(TARGET_STACK);
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK working OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	opp_num = g_debug_stack.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, posdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].vaging, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int gpu_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_table = gpufreq_get_signed_table(TARGET_GPU);
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	opp_num = g_debug_gpu.signed_opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d*] freq: %d, volt: %d, vsram: %d, posdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].vaging, opp_table[i].power);
	}

done:
	mutex_unlock(&gpufreq_debug_lock);

	return ret;
}

static int stack_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int ret = GPUFREQ_SUCCESS;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	opp_table = gpufreq_get_signed_table(TARGET_STACK);
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK signed OPP table (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}
	if (g_test_mode)
		opp_num = g_debug_stack.signed_opp_num;
	else
		opp_num = g_debug_stack.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d*] freq: %d, volt: %d, vsram: %d, posdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].posdiv,
			opp_table[i].vaging, opp_table[i].power);
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

	limit_table = gpufreq_get_limit_table(TARGET_DEFAULT);
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
	char buf[64];
	unsigned int len = 0;
	char cmd[64];
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
	int opp_num = 0, fixed_idx = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_dual_buck) {
		fixed_idx = g_debug_stack.fixed_oppidx;
		opp_num = g_debug_stack.opp_num;
	} else {
		fixed_idx = g_debug_gpu.fixed_oppidx;
		opp_num = g_debug_gpu.opp_num;
	}

	if (fixed_idx >= 0 && fixed_idx < opp_num)
		seq_printf(m, "[GPUFREQ-DEBUG] fix OPP index: %d\n", fixed_idx);
	else if (fixed_idx == -1)
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is disabled\n");
	else
		seq_printf(m, "[GPUFREQ-DEBUG] invalid state of OPP index: %d\n", fixed_idx);

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
	int value = 0;
	int opp_num = -1;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = GPUFREQ_EINVAL;
		goto done;
	}
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "min")) {
		if (g_dual_buck)
			opp_num = g_debug_stack.opp_num;
		else
			opp_num = g_debug_gpu.opp_num;
		if (opp_num > 0) {
			value = opp_num - 1;
			ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, value);
			if (ret) {
				GPUFREQ_LOGE("fail to fix OPP index (%d)", ret);
			} else {
				if (g_dual_buck)
					g_debug_stack.fixed_oppidx = value;
				else
					g_debug_gpu.fixed_oppidx = value;
			}
		}
	} else if (sscanf(buf, "%2d", &value) == 1) {
		ret = gpufreq_fix_target_oppidx(TARGET_DEFAULT, value);
		if (ret) {
			GPUFREQ_LOGE("fail to fix OPP index (%d)", ret);
		} else {
			if (g_dual_buck)
				g_debug_stack.fixed_oppidx = value;
			else
				g_debug_gpu.fixed_oppidx = value;
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept freq and volt */
static int fix_custom_freq_volt_proc_show(struct seq_file *m, void *v)
{
	unsigned int fixed_freq = 0, fixed_volt = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_dual_buck) {
		fixed_freq = g_debug_stack.fixed_freq;
		fixed_volt = g_debug_stack.fixed_volt;
	} else {
		fixed_freq = g_debug_gpu.fixed_freq;
		fixed_volt = g_debug_gpu.fixed_volt;
	}

	if (fixed_freq > 0 && fixed_volt > 0)
		seq_printf(m, "[GPUFREQ-DEBUG] fix freq: %d and volt: %d\n",
			fixed_freq, fixed_volt);
	else if (fixed_freq == 0 && fixed_volt == 0)
		seq_puts(m, "[GPUFREQ-DEBUG] fixed freq and volt are disabled\n");
	else
		seq_printf(m, "[GPUFREQ-DEBUG] invalid state of freq: %d and volt: %d\n",
			fixed_freq, fixed_volt);

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
		if (ret) {
			GPUFREQ_LOGE("fail to fix freq and volt (%d)", ret);
		} else {
			if (g_dual_buck) {
				g_debug_stack.fixed_freq = fixed_freq;
				g_debug_stack.fixed_volt = fixed_volt;
			} else {
				g_debug_gpu.fixed_freq = fixed_freq;
				g_debug_gpu.fixed_volt = fixed_volt;
			}
		}
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

	if (g_stress_test_enable)
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
			g_stress_test_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		ret = gpufreq_set_stress_test(false);
		if (ret)
			GPUFREQ_LOGE("fail to disable stress test (%d)", ret);
		else
			g_stress_test_enable = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current info of aging*/
static int asensor_info_proc_show(struct seq_file *m, void *v)
{
	struct gpufreq_asensor_info asensor_info = {};

	asensor_info = gpufreq_get_asensor_info();

	mutex_lock(&gpufreq_debug_lock);

	seq_printf(m,
		"[GPUFREQ-DEBUG] Aging: %s\n",
		(g_aging_enable) ? "Enable" : "Disable");
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

/* PROCFS: show current state of aging mode */
static int aging_mode_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_aging_enable)
		seq_puts(m, "[GPUFREQ-DEBUG] aging mode is enabled\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] aging mode is disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: apply/restore Vaging by enable/disable aging mode
 * enable: apply aging volt reduction
 * disable: restore aging volt
 */
static ssize_t aging_mode_proc_write(struct file *file,
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
		/* prevent from double aging */
		ret = gpufreq_set_aging_mode(true);
		if (ret)
			GPUFREQ_LOGE("fail to enable aging mode (%d)", ret);
		else
			g_aging_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		/* prevent from double aging */
		ret = gpufreq_set_aging_mode(false);
		if (ret)
			GPUFREQ_LOGE("fail to disable aging mode (%d)", ret);
		else
			g_aging_enable = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of GPM1.0 */
static int gpm_mode_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_gpm_enable)
		seq_puts(m, "[GPUFREQ-DEBUG] GPM1.0 is enabled\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] GPM1.0 is disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: set GPM1.0 registers when GPU power on
 * enable: apply GPM1.0 setting
 * disable: not apply GPM1.0 setting
 */
static ssize_t gpm_mode_proc_write(struct file *file,
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
		ret = gpufreq_set_gpm_mode(true);
		if (ret)
			GPUFREQ_LOGE("fail to enable GPM1.0 (%d)", ret);
		else
			g_gpm_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		ret = gpufreq_set_gpm_mode(false);
		if (ret)
			GPUFREQ_LOGE("fail to disable GPM1.0 (%d)", ret);
		else
			g_gpm_enable = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

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
PROC_FOPS_RW(opp_stress_test);
PROC_FOPS_RW(aging_mode);
PROC_FOPS_RW(gpm_mode);

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
		PROC_ENTRY(aging_mode),
		PROC_ENTRY(gpm_mode),
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

void gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support)
{
	struct gpufreq_debug_opp_info gpu_opp_info = {};
	struct gpufreq_debug_opp_info stack_opp_info = {};
	int ret = GPUFREQ_SUCCESS;

	g_dual_buck = dual_buck;
	g_gpueb_support = gpueb_support;

	gpu_opp_info = gpufreq_get_debug_opp_info(TARGET_GPU);
	if (g_dual_buck)
		stack_opp_info = gpufreq_get_debug_opp_info(TARGET_STACK);

	g_debug_gpu.opp_num = gpu_opp_info.opp_num;
	g_debug_gpu.signed_opp_num = gpu_opp_info.signed_opp_num;
	g_debug_gpu.fixed_oppidx = -1;
	g_debug_gpu.fixed_freq = 0;
	g_debug_gpu.fixed_volt = 0;

	g_debug_stack.opp_num = stack_opp_info.opp_num;
	g_debug_stack.signed_opp_num = stack_opp_info.signed_opp_num;
	g_debug_stack.fixed_oppidx = -1;
	g_debug_stack.fixed_freq = 0;
	g_debug_stack.fixed_volt = 0;

	g_aging_enable = gpu_opp_info.aging_enable;
	g_gpm_enable = gpu_opp_info.gpm_enable;
	g_debug_power_state = POWER_OFF;
	/* always enable test mode when AP mode */
	if (!g_gpueb_support)
		g_test_mode = true;

#if defined(CONFIG_PROC_FS)
	ret = gpufreq_create_procfs();
	if (ret)
		GPUFREQ_LOGE("fail to create procfs (%d)", ret);
#endif
}
