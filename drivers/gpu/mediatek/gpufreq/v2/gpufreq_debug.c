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

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static unsigned int g_dual_buck;
static unsigned int g_gpueb_support;
static unsigned int g_sudo_mode;
static unsigned int g_stress_test_enable;
static unsigned int g_aging_enable;
static unsigned int g_gpm_enable;
static struct gpufreq_debug_status g_debug_gpu;
static struct gpufreq_debug_status g_debug_stack;
static DEFINE_MUTEX(gpufreq_debug_lock);
static struct gpufreq_platform_fp *gpufreq_fp;
static struct gpuppm_platform_fp *gpuppm_fp;

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

	if (gpufreq_fp && gpufreq_fp->get_debug_opp_info_gpu)
		gpu_opp_info = gpufreq_fp->get_debug_opp_info_gpu();
	if (gpuppm_fp && gpuppm_fp->get_debug_limit_info_gpu)
		gpu_limit_info = gpuppm_fp->get_debug_limit_info_gpu();
	if (gpufreq_fp && gpufreq_fp->get_debug_opp_info_stack)
		stack_opp_info = gpufreq_fp->get_debug_opp_info_stack();
	if (gpuppm_fp && gpuppm_fp->get_debug_limit_info_stack)
		stack_limit_info = gpuppm_fp->get_debug_limit_info_stack();

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
		"%-15s DVFSState: 0x%08x, ShaderPresent: 0x%08x\n",
		"[Common-Status]",
		gpu_opp_info.dvfs_state,
		gpu_opp_info.shader_present);
	seq_printf(m,
		"%-15s Aging: %s, StressTest: %s, GPM1.0: %s\n",
		"[Common-Status]",
		gpu_opp_info.aging_enable ? "Enable" : "Disable",
		g_stress_test_enable ? "Enable" : "Disable",
		gpu_opp_info.gpm_enable ? "Enable" : "Disable");

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
	const struct gpufreq_opp_info *opp_table = NULL;
	int opp_num = 0;
	int i = 0;

	if (gpufreq_fp && gpufreq_fp->get_working_table_gpu)
		opp_table = gpufreq_fp->get_working_table_gpu();
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

static int stack_working_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int opp_num = 0;
	int i = 0;

	if (gpufreq_fp && gpufreq_fp->get_working_table_stack)
		opp_table = gpufreq_fp->get_working_table_stack();
	if (!opp_table) {
		GPUFREQ_LOGE("fail to get STACK working OPP table (ENOENT)");
		return GPUFREQ_ENOENT;
	}
	opp_num = g_debug_stack.opp_num;

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
	const struct gpufreq_opp_info *opp_table = NULL;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_sudo_mode) {
		if (gpufreq_fp && gpufreq_fp->get_signed_table_gpu)
			opp_table = gpufreq_fp->get_signed_table_gpu();
		if (!opp_table) {
			GPUFREQ_LOGE("fail to get GPU signed OPP table (ENOENT)");
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

static int stack_signed_opp_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int opp_num = 0;
	int i = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_sudo_mode) {
		if (gpufreq_fp && gpufreq_fp->get_signed_table_stack)
			opp_table = gpufreq_fp->get_signed_table_stack();
		if (!opp_table) {
			GPUFREQ_LOGE("fail to get STACK signed OPP table (ENOENT)");
			mutex_unlock(&gpufreq_debug_lock);
			return GPUFREQ_ENOENT;
		}
		opp_num = g_debug_stack.signed_opp_num;

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

static int limit_table_proc_show(struct seq_file *m, void *v)
{
	const struct gpuppm_limit_info *limit_table = NULL;
	int i = 0;

	if (g_dual_buck && gpuppm_fp && gpuppm_fp->get_limit_table_stack)
		limit_table = gpuppm_fp->get_limit_table_stack();
	else if (!g_dual_buck && gpuppm_fp && gpuppm_fp->get_limit_table_gpu)
		limit_table = gpuppm_fp->get_limit_table_gpu();
	if (!limit_table) {
		GPUFREQ_LOGE("fail to get limit table (ENOENT)");
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
			limit_table[i].c_enable,
			limit_table[i].f_enable);
	}

	return GPUFREQ_SUCCESS;
}

static ssize_t limit_table_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	unsigned int len = 0;
	char cmd[64];
	int limiter = 0, ceiling = 0, floor = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%s %d %d %d", cmd, &limiter, &ceiling, &floor) == 4) {
		if (sysfs_streq(cmd, "set")) {
			if (g_dual_buck && gpuppm_fp && gpuppm_fp->set_limit_stack)
				ret = gpuppm_fp->set_limit_stack(LIMIT_DEBUG, ceiling, floor);
			else if (!g_dual_buck && gpuppm_fp && gpuppm_fp->set_limit_gpu)
				ret = gpuppm_fp->set_limit_gpu(LIMIT_DEBUG, ceiling, floor);
			else
				ret = GPUFREQ_ENOENT;
			if (ret) {
				GPUFREQ_LOGE("fail to set debug limit index (%d)",
					ret);
				goto done_unlock;
			}
		}
		else if (sysfs_streq(cmd, "switch")) {
			if (g_dual_buck && gpuppm_fp && gpuppm_fp->switch_limit_stack)
				ret = gpuppm_fp->switch_limit_stack(limiter, ceiling, floor);
			else if (!g_dual_buck && gpuppm_fp && gpuppm_fp->switch_limit_gpu)
				ret = gpuppm_fp->switch_limit_gpu(limiter, ceiling, floor);
			else
				ret = GPUFREQ_ENOENT;
			if (ret) {
				GPUFREQ_LOGE("fail to set debug limit switch (%d)",
					ret);
				goto done_unlock;
			}
		}
	}

done_unlock:
	mutex_unlock(&gpufreq_debug_lock);
done:
	return (ret < 0) ? ret : count;
}

/* PROCFS: show current state of kept OPP index */
static int fix_target_opp_index_proc_show(struct seq_file *m, void *v)
{
	const struct gpufreq_opp_info *opp_table = NULL;
	int opp_num = 0;
	int fixed_idx = 0;

	mutex_lock(&gpufreq_debug_lock);

	if (g_dual_buck && gpufreq_fp && gpufreq_fp->get_working_table_stack) {
		fixed_idx = g_debug_stack.fixed_oppidx;
		opp_num = g_debug_stack.opp_num;
		opp_table = gpufreq_fp->get_working_table_stack();
	} else if (!g_dual_buck && gpufreq_fp && gpufreq_fp->get_working_table_gpu) {
		fixed_idx = g_debug_gpu.fixed_oppidx;
		opp_num = g_debug_gpu.opp_num;
		opp_table = gpufreq_fp->get_working_table_gpu();
	}

	if (opp_table && fixed_idx >= 0 && fixed_idx < opp_num) {
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is enabled\n");
		seq_printf(m, "[%02d] freq: %d, volt: %d, vsram: %d\n",
			fixed_idx, opp_table[fixed_idx].freq,
			opp_table[fixed_idx].volt, opp_table[fixed_idx].vsram);
	} else if (fixed_idx == -1)
		seq_puts(m, "[GPUFREQ-DEBUG] fixed OPP index is disabled\n");
	else
		seq_printf(m, "[GPUFREQ-DEBUG] invalid state of OPP index: %d\n",
			fixed_idx);

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
	char buf[64];
	unsigned int len = 0;
	int value = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	mutex_lock(&gpufreq_debug_lock);

	if (sscanf(buf, "%d", &value) == 1) {
		if (g_dual_buck && gpufreq_fp && gpufreq_fp->fix_target_oppidx_stack) {
			ret = gpufreq_fp->fix_target_oppidx_stack(value);
			if (ret) {
				GPUFREQ_LOGE("fail to fix STACK OPP index (%d)",
					ret);
				goto done_unlock;
			}
			g_debug_stack.fixed_oppidx = value;
		} else if (!g_dual_buck && gpufreq_fp && gpufreq_fp->fix_target_oppidx_gpu) {
			ret = gpufreq_fp->fix_target_oppidx_gpu(value);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPU OPP index (%d)",
					ret);
				goto done_unlock;
			}
			g_debug_gpu.fixed_oppidx = value;
		} else
			ret = GPUFREQ_ENOENT;
	}

done_unlock:
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

	if (fixed_freq > 0 && fixed_volt > 0) {
		seq_puts(m, "[GPUFREQ-DEBUG] fixed freq and volt are enabled\n");
		seq_printf(m, "[**] freq: %d, volt: %d\n",
			fixed_freq, fixed_volt);
	} else if (fixed_freq == 0 && fixed_volt == 0)
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
	char buf[64];
	unsigned int len = 0;
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
		if (g_dual_buck && gpufreq_fp && gpufreq_fp->fix_custom_freq_volt_stack) {
			ret = gpufreq_fp->fix_custom_freq_volt_stack(
				fixed_freq, fixed_volt);
			if (ret) {
				GPUFREQ_LOGE("fail to fix STACK freq and volt (%d)",
					ret);
				goto done_unlock;
			}
			g_debug_stack.fixed_freq = fixed_freq;
			g_debug_stack.fixed_volt = fixed_volt;
		} else if (!g_dual_buck && gpufreq_fp && gpufreq_fp->fix_custom_freq_volt_gpu) {
			ret = gpufreq_fp->fix_custom_freq_volt_gpu(
				fixed_freq, fixed_volt);
			if (ret) {
				GPUFREQ_LOGE("fail to fix GPU OPP freq and volt (%d)",
					ret);
				goto done_unlock;
			}
			g_debug_gpu.fixed_freq = fixed_freq;
			g_debug_gpu.fixed_volt = fixed_volt;
		} else
			ret = GPUFREQ_ENOENT;
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

	if (!gpufreq_fp || !gpufreq_fp->set_stress_test) {
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		gpufreq_fp->set_stress_test(true);
		g_stress_test_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		gpufreq_fp->set_stress_test(false);
		g_stress_test_enable = false;
	}
	ret = GPUFREQ_SUCCESS;

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
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
	char buf[64];
	unsigned int len = 0;
	int ret = GPUFREQ_EINVAL;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	if (!gpufreq_fp || !gpufreq_fp->set_aging_mode) {
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		/* prevent from double aging */
		ret = gpufreq_fp->set_aging_mode(true);
		if (ret) {
			GPUFREQ_LOGE("fail to enable aging mode (%d)",
				ret);
			goto done_unlock;
		}
		g_aging_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		/* prevent from double aging */
		ret = gpufreq_fp->set_aging_mode(false);
		if (ret) {
			GPUFREQ_LOGE("fail to disable aging mode (%d)",
				ret);
			goto done_unlock;
		}
		g_aging_enable = false;
	}

done_unlock:
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
	int ret = -EFAULT;
	char buf[64];
	unsigned int len = 0;
	unsigned int value = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		goto done;
	buf[len] = '\0';

	if (!gpufreq_fp || !gpufreq_fp->set_gpm_mode) {
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	mutex_lock(&gpufreq_debug_lock);

	if (sysfs_streq(buf, "enable")) {
		gpufreq_fp->set_gpm_mode(true);
		g_gpm_enable = true;
	} else if (sysfs_streq(buf, "disable")) {
		gpufreq_fp->set_gpm_mode(false);
		g_gpm_enable = false;
	}

	mutex_unlock(&gpufreq_debug_lock);

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS : initialization */
PROC_FOPS_RO(gpufreq_status);
PROC_FOPS_RW(gpufreq_pikachu);
PROC_FOPS_RO(gpu_working_opp_table);
PROC_FOPS_RO(gpu_signed_opp_table);
PROC_FOPS_RO(stack_working_opp_table);
PROC_FOPS_RO(stack_signed_opp_table);
PROC_FOPS_RW(limit_table);
PROC_FOPS_RW(fix_target_opp_index);
PROC_FOPS_RW(fix_custom_freq_volt);
PROC_FOPS_RW(opp_stress_test);
PROC_FOPS_RW(aging_mode);
PROC_FOPS_RW(gpm_mode);
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

	const struct pentry default_entries[] = {
		PROC_ENTRY(gpufreq_status),
		PROC_ENTRY(gpufreq_pikachu),
		PROC_ENTRY(gpu_working_opp_table),
		PROC_ENTRY(gpu_signed_opp_table),
		PROC_ENTRY(limit_table),
		PROC_ENTRY(fix_target_opp_index),
		PROC_ENTRY(fix_custom_freq_volt),
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

void gpufreq_debug_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp)
{
	struct gpufreq_debug_opp_info gpu_opp_info = {};
	struct gpufreq_debug_opp_info stack_opp_info = {};

	if (!platform_fp) {
		GPUFREQ_LOGE("null gpufreq platform function pointer (EINVAL)");
		return;
	}

	gpufreq_fp = platform_fp;

	/* init debug info when function pointer is registered */
	if (gpufreq_fp && gpufreq_fp->get_debug_opp_info_gpu)
		gpu_opp_info = gpufreq_fp->get_debug_opp_info_gpu();
	if (gpufreq_fp && gpufreq_fp->get_debug_opp_info_stack)
		stack_opp_info = gpufreq_fp->get_debug_opp_info_stack();

	g_debug_gpu.opp_num = gpu_opp_info.opp_num;
	g_debug_gpu.signed_opp_num = gpu_opp_info.signed_opp_num;
	g_debug_stack.opp_num = stack_opp_info.opp_num;
	g_debug_stack.signed_opp_num = stack_opp_info.signed_opp_num;

	g_aging_enable = gpu_opp_info.aging_enable;
	g_gpm_enable = gpu_opp_info.gpm_enable;
}
EXPORT_SYMBOL(gpufreq_debug_register_gpufreq_fp);

void gpufreq_debug_register_gpuppm_fp(struct gpuppm_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpuppm platform function pointer (EINVAL)");
		return;
	}

	gpuppm_fp = platform_fp;
}
EXPORT_SYMBOL(gpufreq_debug_register_gpuppm_fp);

int gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support)
{
	int ret = GPUFREQ_SUCCESS;

	g_dual_buck = dual_buck;
	g_gpueb_support = gpueb_support;

	g_debug_gpu.opp_num = 0;
	g_debug_gpu.signed_opp_num = 0;
	g_debug_gpu.fixed_oppidx = -1;
	g_debug_gpu.fixed_freq = 0;
	g_debug_gpu.fixed_volt = 0;

	g_debug_stack.opp_num = 0;
	g_debug_stack.signed_opp_num = 0;
	g_debug_stack.fixed_oppidx = -1;
	g_debug_stack.fixed_freq = 0;
	g_debug_stack.fixed_volt = 0;

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
