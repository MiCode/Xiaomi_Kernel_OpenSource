// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpufreq_debug.c
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

#include <mtk_gpufreq_v2.h>
#include <mtk_gpufreq_debug.h>
#include <mtk_gpufreq_common.h>
#include <mtk_gpuppm.h>
#if defined(CONFIG_GPU_MT6893)
#include <mt6893/mtk_gpufreq_core.h>
#endif

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static bool g_sudo_mode;
static bool g_stress_test_enable;
static bool g_aging_enable;
static struct gpufreq_debug_status g_debug_gpu;
static struct gpufreq_debug_status g_debug_gstack;
static DEFINE_MUTEX(gpufreq_debug_lock);

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
#if defined(CONFIG_PROC_FS)
static int gpufreq_status_proc_show(struct seq_file *m, void *v)
{
	struct gpufreq_debug_opp_info gpu_opp_info;
	struct gpufreq_debug_limit_info gpu_limit_info;
#if defined(GPUFREQ_DUAL_BUCK)
	struct gpufreq_debug_opp_info gstack_opp_info;
	struct gpufreq_debug_limit_info gstack_limit_info;
#endif

	gpu_opp_info = __gpufreq_get_debug_opp_info_gpu();
	gpu_limit_info = gpuppm_get_debug_limit_info_gpu();
#if defined(GPUFREQ_DUAL_BUCK)
	gstack_opp_info = __gpufreq_get_debug_opp_info_gstack();
	gstack_limit_info = gpuppm_get_debug_limit_info_gstack();
#endif

	seq_puts(m, "[GPUFREQ-DEBUG] Current Status of GPU-DVFS Platform\n");
	seq_printf(m,
		"%-13s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
		"[GPU-OPP]",
		gpu_opp_info.cur_oppidx,
		gpu_opp_info.cur_freq,
		gpu_opp_info.cur_volt,
		gpu_opp_info.cur_vsram);
	seq_printf(m,
		"%-13s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
		"[GPU-REALOPP]",
		gpu_opp_info.fmeter_freq,
		gpu_opp_info.con1_freq,
		gpu_opp_info.regulator_volt,
		gpu_opp_info.regulator_vsram);
	seq_printf(m,
		"%-13s PowerCount: %d, CG: %d, MTCMOS: %d, BUCK: %d\n",
		"[GPU-Power]",
		gpu_opp_info.power_count,
		gpu_opp_info.cg_count,
		gpu_opp_info.mtcmos_count,
		gpu_opp_info.buck_count);
	seq_printf(m,
		"%-13s SegmentID: %d, UpperBound: %d, LowerBound: %d\n",
		"[GPU-Segment]",
		gpu_opp_info.segment_id,
		gpu_opp_info.segment_upbound,
		gpu_opp_info.segment_lowbound);
	seq_printf(m,
		"%-13s CeilingIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPU-LimitC]",
		gpu_limit_info.ceiling,
		gpu_limit_info.c_limiter,
		gpu_limit_info.c_priority);
	seq_printf(m,
		"%-13s FloorIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPU-LimitF]",
		gpu_limit_info.floor,
		gpu_limit_info.f_limiter,
		gpu_limit_info.f_priority);
	seq_printf(m,
		"%-13s DVFSState: 0x%08x, ShaderPresent: 0x%08x\n",
		"[GPU-Misc]",
		gpu_opp_info.dvfs_state,
		gpu_opp_info.shader_present);
	seq_printf(m,
		"%-13s Aging: %s, StressTest: %s\n",
		"[GPU-Misc]",
		gpu_opp_info.aging_enable ? "Enable" : "Disable",
		gpu_opp_info.stress_test_enable ? "Enable" : "Disable");

#if defined(GPUFREQ_DUAL_BUCK)
	seq_printf(m,
		"%-18s Index: %d, Freq: %d, Volt: %d, Vsram: %d\n",
		"[GPUSTACK-OPP]",
		gstack_opp_info.cur_oppidx,
		gstack_opp_info.cur_freq,
		gstack_opp_info.cur_volt,
		gstack_opp_info.cur_vsram);
	seq_printf(m,
		"%-18s FmeterFreq: %d, Con1Freq: %d, ReguVolt: %d, ReguVsram: %d\n",
		"[GPUSTACK-REALOPP]",
		gstack_opp_info.fmeter_freq,
		gstack_opp_info.con1_freq,
		gstack_opp_info.regulator_volt,
		gstack_opp_info.regulator_vsram);
	seq_printf(m,
		"%-18s PowerCount: %d, CG: %d, MTCMOS: %d, BUCK: %d\n",
		"[GPUSTACK-Power]",
		gstack_opp_info.power_count,
		gstack_opp_info.cg_count,
		gstack_opp_info.mtcmos_count,
		gstack_opp_info.buck_count);
	seq_printf(m,
		"%-18s SegmentID: %d, UpperBound: %d, LowerBound: %d\n",
		"[GPUSTACK-Segment]",
		gstack_opp_info.segment_id,
		gstack_opp_info.segment_upbound,
		gstack_opp_info.segment_lowbound);
	seq_printf(m,
		"%-18s CeilingIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPUSTACK-LimitC]",
		gstack_limit_info.ceiling,
		gstack_limit_info.c_limiter,
		gstack_limit_info.c_priority);
	seq_printf(m,
		"%-18s FloorIndex: %d, Limiter: %d, Priority: %d\n",
		"[GPUSTACK-LimitF]",
		gstack_limit_info.floor,
		gstack_limit_info.f_limiter,
		gstack_limit_info.f_priority);
	seq_printf(m,
		"%-18s DVFSState: 0x%08x, ShaderPresent: 0x%08x\n",
		"[GPUSTACK-Misc]",
		gstack_opp_info.dvfs_state,
		gstack_opp_info.shader_present);
	seq_printf(m,
		"%-18s Aging: %s, StressTest: %s\n",
		"[GPUSTACK-Misc]",
		gstack_opp_info.aging_enable ? "Enable" : "Disable",
		gstack_opp_info.stress_test_enable ? "Enable" : "Disable");
#endif

	return GPUFREQ_SUCCESS;
}

/* PROCFS: show current mode of GPUFREQ-DEBUG */
static int gpufreq_pikachu_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_sudo_mode)
		seq_puts(m, "[GPUFREQ-DEBUG] this is a SUPER pikachu\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] this is a pikachu\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: enable sudo mode of GPUFREQ-DEBUG
 * GPUFREQ_DBG_KEY: enable sudo mode
 * others: disable sudo mode
 */
static ssize_t gpufreq_pikachu_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, GPUFREQ_DBG_KEY))
		g_sudo_mode = true;
	else
		g_sudo_mode = false;
	ret = GPUFREQ_SUCCESS;

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

static int gpu_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table;
	unsigned int opp_num = 0;
	int i = 0;

	opp_table = __gpufreq_get_working_table_gpu();
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPU working OPP table (ENOENT)");
		return GPUFREQ_ENOENT;
	}
	opp_num = g_debug_gpu.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, postdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].postdiv,
			opp_table[i].vaging, opp_table[i].power);
	}

	return GPUFREQ_SUCCESS;
}

static int gstack_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table;
	unsigned int opp_num = 0;
	int i = 0;

	opp_table = __gpufreq_get_working_table_gstack();
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get GPUSTACK working OPP table (ENOENT)");
		return GPUFREQ_ENOENT;
	}
	opp_num = g_debug_gstack.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m,
			"[%02d] freq: %d, volt: %d, vsram: %d, postdiv: %d, vaging: %d, power: %d\n",
			i, opp_table[i].freq, opp_table[i].volt,
			opp_table[i].vsram, opp_table[i].postdiv,
			opp_table[i].vaging, opp_table[i].power);
	}

	return GPUFREQ_SUCCESS;
}

static int gpu_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table;
	unsigned int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_sudo_mode) {
		opp_table = __gpufreq_get_signed_table_gpu();
		if (!opp_table) {
			GPUFREQ_LOGE(
				"fail to get GPU signed OPP table (ENOENT)");
			mutex_unlock(&gpufreq_debug_lock);
			return GPUFREQ_ENOENT;
		}
		opp_num = g_debug_gpu.signed_opp_num;

		for (i = 0; i < opp_num; i++) {
			seq_printf(m,
				"[%02d*] freq: %d, volt: %d, vsram: %d, postdiv: %d, vaging: %d, power: %d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].postdiv,
				opp_table[i].vaging, opp_table[i].power);
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static int gstack_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table;
	unsigned int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_sudo_mode) {
		opp_table = __gpufreq_get_signed_table_gstack();
		if (!opp_table) {
			GPUFREQ_LOGE("fail to get GPUSTACK signed OPP table (ENOENT)");
			mutex_unlock(&gpufreq_debug_lock);
			return GPUFREQ_ENOENT;
		}
		opp_num = g_debug_gstack.signed_opp_num;

		for (i = 0; i < opp_num; i++) {
			seq_printf(m,
				"[%02d*] freq: %d, volt: %d, vsram: %d, postdiv: %d, vaging: %d, power: %d\n",
				i, opp_table[i].freq, opp_table[i].volt,
				opp_table[i].vsram, opp_table[i].postdiv,
				opp_table[i].vaging, opp_table[i].power);
		}
	}

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

static int gpu_limit_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpuppm_limit_info *limit_table;
	int i = 0;

	limit_table = gpuppm_get_limit_table_gpu();
	if (!limit_table) {
		GPUFREQ_LOGE("fail to get GPU limit table (ENOENT)");
		return GPUFREQ_ENOENT;
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
			limit_table[i].ceiling_enable,
			limit_table[i].floor_enable);
	}

	return GPUFREQ_SUCCESS;
}

static int gstack_limit_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpuppm_limit_info *limit_table;
	int i = 0;

	limit_table = gpuppm_get_limit_table_gstack();
	if (!limit_table) {
		GPUFREQ_LOGE("fail to get GPUSTACK limit table (ENOENT)");
		return GPUFREQ_ENOENT;
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
			limit_table[i].ceiling_enable,
			limit_table[i].floor_enable);
	}

	return GPUFREQ_SUCCESS;
}

static int gpu_springboard_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_sb_info *sb_table;
	unsigned int opp_num = 0;
	int i = 0;

	sb_table = __gpufreq_get_sb_table_gpu();
	if (!sb_table) {
		GPUFREQ_LOGE("fail to get GPU springboard table (ENOENT)");
		return GPUFREQ_ENOENT;
	}
	opp_num = g_debug_gpu.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m, "[%02d] springboard up: %d, down: %d\n",
			i, sb_table[i].up, sb_table[i].down);
	}

	return GPUFREQ_SUCCESS;
}

static int gstack_springboard_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_sb_info *sb_table;
	unsigned int opp_num = 0;
	int i = 0;

	sb_table = __gpufreq_get_sb_table_gstack();
	if (!sb_table) {
		GPUFREQ_LOGE("fail to get GPUSTACK springboard table (ENOENT)");
		return GPUFREQ_ENOENT;
	}
	opp_num = g_debug_gstack.opp_num;

	for (i = 0; i < opp_num; i++) {
		seq_printf(m, "[%02d] springboard up: %d, down: %d\n",
			i, sb_table[i].up, sb_table[i].down);
	}

	return GPUFREQ_SUCCESS;
}

/* PROCFS: show current state of kept OPP index */
static int fix_target_opp_index_proc_show(struct seq_file *m, void *v)
{
	enum gpufreq_target target = TARGET_DEFAULT;
	const struct gpufreq_opp_info *opp_table;
	unsigned int opp_num = 0;
	int fixed_idx = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (target == TARGET_GPUSTACK) {
		fixed_idx = g_debug_gstack.fixed_oppidx;
		opp_num = g_debug_gstack.opp_num;
		opp_table = __gpufreq_get_working_table_gstack();
	} else if (target == TARGET_GPU) {
		fixed_idx = g_debug_gpu.fixed_oppidx;
		opp_num = g_debug_gpu.opp_num;
		opp_table = __gpufreq_get_working_table_gpu();
	}

	if (fixed_idx >= 0 && fixed_idx < opp_num) {
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is enabled\n");
		seq_printf(m, "[%02d] freq: %d, volt: %d, vsram: %d\n",
			fixed_idx, opp_table[fixed_idx].freq,
			opp_table[fixed_idx].volt, opp_table[fixed_idx].vsram);
	} else if (fixed_idx == GPUFREQ_DBG_DEFAULT_IDX)
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is disabled\n");
	else
		seq_printf(m, "[GPUFREQ-DEBUG] invalid state of OPP index: %d\n",
			fixed_idx);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep OPP index
 * GPUFREQ_DBG_DEFAULT_IDX: free run
 * others: OPP index to be kept
 */
static ssize_t fix_target_opp_index_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int value = 0;
	enum gpufreq_target target = TARGET_DEFAULT;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%d", &value) == 1) {
		if (target == TARGET_GPUSTACK) {
			ret = __gpufreq_fix_target_oppidx_gstack(value);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPUSTACK OPP index");
				goto done_unlock;
			}
			g_debug_gstack.fixed_oppidx = value;
		} else if (target == TARGET_GPU) {
			ret = __gpufreq_fix_target_oppidx_gpu(value);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPU OPP index");
				goto done_unlock;
			}
			g_debug_gpu.fixed_oppidx = value;
		}
	}

done_unlock:
	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept freq and volt */
static int fix_custom_freq_volt_proc_show(struct seq_file *m, void *v)
{
	enum gpufreq_target target = TARGET_DEFAULT;
	unsigned int fixed_freq = 0, fixed_volt = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (target == TARGET_GPUSTACK) {
		fixed_freq = g_debug_gstack.fixed_freq;
		fixed_volt = g_debug_gstack.fixed_volt;
	} else if (target == TARGET_GPU) {
		fixed_freq = g_debug_gpu.fixed_freq;
		fixed_volt = g_debug_gpu.fixed_volt;
	}

	if (fixed_freq > 0 && fixed_volt > 0) {
		seq_puts(m, "[GPUFREQ-DEBUG] fixed freq and volt are enabled\n");
		seq_printf(m, "[**] freq: %d, volt: %d\n",
			fixed_freq, fixed_volt);
	} else if (fixed_freq == GPUFREQ_DBG_DEFAULT_FREQ &&
		fixed_volt == GPUFREQ_DBG_DEFAULT_VOLT)
		seq_puts(m, "[GPUFREQ-DEBUG] fixed freq and volt are disabled\n");
	else
		seq_printf(m, "[GPUFREQ-DEBUG] invalid state of freq: %d and volt: %d\n",
			fixed_freq, fixed_volt);

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: keep freq and volt
 * GPUFREQ_DBG_DEFAULT_FREQ VOLT: free run
 * others others: freq and volt to be kept
 */
static ssize_t fix_custom_freq_volt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	enum gpufreq_target target = TARGET_DEFAULT;
	unsigned int fixed_freq = 0, fixed_volt = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);
	if (!g_sudo_mode) {
		ret = GPUFREQ_SUCCESS;
		goto done_unlock;
	}

	if (sscanf(buf, "%d %d", &fixed_freq, &fixed_volt) == 2) {
		if (target == TARGET_GPUSTACK) {
			ret = __gpufreq_fix_custom_freq_volt_gstack(
				fixed_freq, fixed_volt);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPUSTACK freq and volt");
				goto done_unlock;
			}
			g_debug_gstack.fixed_freq = fixed_freq;
			g_debug_gstack.fixed_volt = fixed_volt;
		} else if (target == TARGET_GPU) {
			ret = __gpufreq_fix_custom_freq_volt_gpu(
				fixed_freq, fixed_volt);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPU OPP freq and volt");
				goto done_unlock;
			}
			g_debug_gpu.fixed_freq = fixed_freq;
			g_debug_gpu.fixed_volt = fixed_volt;
		}
	}

done_unlock:
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
	char buf[64];
	unsigned int len = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		__gpufreq_set_stress_test(true);
		g_stress_test_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		__gpufreq_set_stress_test(false);
		g_stress_test_enable = false;
	}
	ret = GPUFREQ_SUCCESS;

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of aging mode */
static int enforced_aging_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&gpufreq_debug_lock);

	if (g_aging_enable)
		seq_puts(m, "[GPUFREQ-DEBUG] enforced aging is enabled\n");
	else
		seq_puts(m, "[GPUFREQ-DEBUG] enforced aging is disabled\n");

	mutex_unlock(&gpufreq_debug_lock);

	return GPUFREQ_SUCCESS;
}

/*
 * PROCFS: enforce aging by subtracting aging volt
 * enable: apply aging setting
 * disable: restore aging setting
 */
static ssize_t enforced_aging_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		/* prevent from double aging */
		ret = __gpufreq_set_enforced_aging(true);
		if (ret) {
			GPUFREQ_LOGE("fail to enable enforced aging");
			goto done_unlock;
		}
		g_aging_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		/* prevent from double aging */
		ret = __gpufreq_set_enforced_aging(false);
		if (ret) {
			GPUFREQ_LOGE("fail to disable enforced aging");
			goto done_unlock;
		}
		g_aging_enable = false;
	}

done_unlock:
	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS : initialization */
PROC_FOPS_RO(gpufreq_status);
PROC_FOPS_RW(gpufreq_pikachu);
PROC_FOPS_RO(gpu_working_opp_table);
PROC_FOPS_RO(gpu_signed_opp_table);
PROC_FOPS_RO(gpu_limit_table);
PROC_FOPS_RO(gpu_springboard_table);
PROC_FOPS_RO(gstack_working_opp_table);
PROC_FOPS_RO(gstack_signed_opp_table);
PROC_FOPS_RO(gstack_limit_table);
PROC_FOPS_RO(gstack_springboard_table);
PROC_FOPS_RW(fix_target_opp_index);
PROC_FOPS_RW(fix_custom_freq_volt);
PROC_FOPS_RW(opp_stress_test);
PROC_FOPS_RW(enforced_aging);
// PROC_FOPS_RO(dfd_test);
// PROC_FOPS_RW(dfd_force_dump);

static int gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_status),
		PROC_ENTRY(gpufreq_pikachu),
		PROC_ENTRY(gpu_working_opp_table),
		PROC_ENTRY(gpu_signed_opp_table),
		PROC_ENTRY(gpu_limit_table),
		PROC_ENTRY(gpu_springboard_table),
		PROC_ENTRY(fix_target_opp_index),
		PROC_ENTRY(fix_custom_freq_volt),
		PROC_ENTRY(opp_stress_test),
		PROC_ENTRY(enforced_aging),
#if defined(GPUFREQ_DUAL_BUCK)
		PROC_ENTRY(gstack_working_opp_table),
		PROC_ENTRY(gstack_signed_opp_table),
		PROC_ENTRY(gstack_limit_table),
		PROC_ENTRY(gstack_springboard_table),
#endif
	};

	dir = proc_mkdir("gpufreqv2", NULL);
	if (!dir) {
		GPUFREQ_LOGE("fail to create /proc/gpufreqv2 (ENOMEM)");
		return GPUFREQ_ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0660, dir, entries[i].fops))
			GPUFREQ_LOGE("fail to create /proc/gpufreq/%s",
				entries[i].name);
	}

	return GPUFREQ_SUCCESS;
}
#endif /* CONFIG_PROC_FS */

int gpufreq_debug_init(void)
{
	int ret = GPUFREQ_SUCCESS;
	unsigned int opp_num_gpu = 0, opp_num_gstack = 0;
	unsigned int signed_opp_num_gpu = 0, signed_opp_num_gstack = 0;

	opp_num_gpu = __gpufreq_get_opp_num_gpu();
	opp_num_gstack = __gpufreq_get_opp_num_gstack();

	signed_opp_num_gpu = __gpufreq_get_signed_opp_num_gpu();
	signed_opp_num_gstack = __gpufreq_get_signed_opp_num_gstack();

	g_debug_gpu.opp_num = opp_num_gpu;
	g_debug_gpu.signed_opp_num = signed_opp_num_gpu;
	g_debug_gpu.fixed_oppidx = GPUFREQ_DBG_DEFAULT_IDX;
	g_debug_gpu.fixed_freq = GPUFREQ_DBG_DEFAULT_FREQ;
	g_debug_gpu.fixed_volt = GPUFREQ_DBG_DEFAULT_VOLT;

	g_debug_gstack.opp_num = opp_num_gstack;
	g_debug_gstack.signed_opp_num = signed_opp_num_gstack;
	g_debug_gstack.fixed_oppidx = GPUFREQ_DBG_DEFAULT_IDX;
	g_debug_gstack.fixed_freq = GPUFREQ_DBG_DEFAULT_FREQ;
	g_debug_gstack.fixed_volt = GPUFREQ_DBG_DEFAULT_VOLT;

#if defined(CONFIG_PROC_FS)
	ret = gpufreq_create_procfs();
	if (ret) {
		GPUFREQ_LOGE("fail to create procfs (%d)", ret);
		goto done;
	}
#endif

done:
	return ret;
}
