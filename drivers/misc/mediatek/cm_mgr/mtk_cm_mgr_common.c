// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/sched/rt.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <linux/math64.h>

#include <linux/fb.h>
#include <linux/notifier.h>

#include "mtk_cm_mgr_common.h"
#include <linux/soc/mediatek/mtk-pm-qos.h>

#ifdef CONFIG_MTK_DVFSRC
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <dvfsrc-exp.h>
#endif /* CONFIG_MTK_DVFSRC */

#if defined(CONFIG_MACH_MT6761) || defined(CONFIG_MACH_MT6765)
#undef USE_CM_MGR_AT_SSPM
#else
#define USE_CM_MGR_AT_SSPM
#endif

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
#include <sspm_ipi.h>
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */

static struct kobject *cm_mgr_kobj;
struct platform_device *cm_mgr_pdev;
void __iomem *cm_mgr_base;
int cm_mgr_num_perf;
int *cm_mgr_perfs;
int *cm_mgr_cpu_opp_to_dram;
int cm_mgr_num_array;
int *cm_mgr_buf;
int cm_mgr_cpu_opp_size;

int cm_mgr_blank_status;
int cm_mgr_disable_fb = 1;
int cm_mgr_emi_demand_check = 1;
int cm_mgr_enable = 1;
int cm_mgr_loading_enable;
int cm_mgr_loading_level = 1000;
int cm_mgr_opp_enable = 1;
int cm_mgr_perf_enable = 1;
int cm_mgr_perf_force_enable;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
int cm_mgr_sspm_enable = 1;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
int *cpu_power_ratio_down;
int *cpu_power_ratio_up;
int *debounce_times_down_adb;
int debounce_times_perf_down = 50;
int debounce_times_perf_force_down = 100;
int debounce_times_reset_adb;
int *debounce_times_up_adb;
int light_load_cps = 1000;
int *vcore_power_ratio_down;
int *vcore_power_ratio_up;

int debounce_times_perf_down_local = -1;
int debounce_times_perf_down_force_local = -1;
int pm_qos_update_request_status;
int cm_mgr_dram_opp_base = -1;
int cm_mgr_dram_opp = -1;

int cm_mgr_loop_count;
static int cm_mgr_dram_level;
int total_bw_value;

/* setting in DTS */
int cm_mgr_use_bcpu_weight;
int cm_mgr_use_cpu_to_dram_map;
int cm_mgr_use_cpu_to_dram_map_new;
int cpu_power_bcpu_weight_max = 100;
int cpu_power_bcpu_weight_min = 100;
int cm_mgr_cpu_map_dram_enable = 1;
int cm_mgr_cpu_map_emi_opp = 1;
int cm_mgr_cpu_map_skip_cpu_opp = 2;

static int cm_mgr_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		pr_info("#@# %s(%d) SCREEN ON\n", __func__, __LINE__);
		cm_mgr_blank_status = 0;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 0);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
		break;
	case FB_BLANK_POWERDOWN:
		pr_info("#@# %s(%d) SCREEN OFF\n", __func__, __LINE__);
		cm_mgr_blank_status = 1;
		cm_mgr_dram_opp_base = -1;
		cm_mgr_perf_platform_set_status(0);
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_BLANK, 1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block cm_mgr_fb_notifier = {
	.notifier_call = cm_mgr_fb_notifier_callback,
};

void cm_mgr_perf_set_status(int enable)
{
	if (cm_mgr_disable_fb == 1 && cm_mgr_blank_status == 1)
		enable = 0;

	cm_mgr_perf_platform_set_force_status(enable);

	if (cm_mgr_perf_force_enable)
		return;

	cm_mgr_perf_platform_set_status(enable);
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_status);

void cm_mgr_perf_set_force_status(int enable)
{
	if (enable != cm_mgr_perf_force_enable) {
		cm_mgr_perf_force_enable = enable;
		if (!enable)
			cm_mgr_perf_platform_set_force_status(enable);
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_perf_set_force_status);

void cm_mgr_enable_fn(int enable)
{
	cm_mgr_enable = enable;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE,
			cm_mgr_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
}
EXPORT_SYMBOL_GPL(cm_mgr_enable_fn);

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
/* FIXME: */
#if defined(USE_SSPM_VER_V2)
int cm_mgr_to_sspm_command(u32 cmd, int val)
{
	unsigned int ret = 0;
	struct cm_mgr_data cm_mgr_d;

	if (cm_sspm_ready != 1) {
		pr_info("#@# %s(%d) sspm not ready(%d) to receive cmd(%d)\n",
			__func__, __LINE__, cm_sspm_ready, cmd);
		ret = -1;
		return ret;
	}
	cm_ipi_ackdata = 0;

	switch (cmd) {
	case IPI_CM_MGR_INIT:
	case IPI_CM_MGR_ENABLE:
	case IPI_CM_MGR_OPP_ENABLE:
	case IPI_CM_MGR_SSPM_ENABLE:
	case IPI_CM_MGR_BLANK:
	case IPI_CM_MGR_DISABLE_FB:
	case IPI_CM_MGR_DRAM_TYPE:
	case IPI_CM_MGR_CPU_POWER_RATIO_UP:
	case IPI_CM_MGR_CPU_POWER_RATIO_DOWN:
	case IPI_CM_MGR_VCORE_POWER_RATIO_UP:
	case IPI_CM_MGR_VCORE_POWER_RATIO_DOWN:
	case IPI_CM_MGR_DEBOUNCE_UP:
	case IPI_CM_MGR_DEBOUNCE_DOWN:
	case IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB:
	case IPI_CM_MGR_DRAM_LEVEL:
	case IPI_CM_MGR_LIGHT_LOAD_CPS:
	case IPI_CM_MGR_LOADING_ENABLE:
	case IPI_CM_MGR_LOADING_LEVEL:
	case IPI_CM_MGR_EMI_DEMAND_CHECK:
	case IPI_CM_MGR_OPP_FREQ_SET:
	case IPI_CM_MGR_OPP_VOLT_SET:
	case IPI_CM_MGR_BCPU_WEIGHT_MAX_SET:
	case IPI_CM_MGR_BCPU_WEIGHT_MIN_SET:
		cm_mgr_d.cmd = cmd;
		cm_mgr_d.arg = val;
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_CM,
		IPI_SEND_POLLING, &cm_mgr_d, CM_MGR_D_LEN, 2000);
		if (ret != 0) {
			pr_info("#@# %s(%d) cmd(%d) error, return %d\n",
					__func__, __LINE__, cmd, ret);
		} else if (!cm_ipi_ackdata) {
			ret = cm_ipi_ackdata;
			pr_info("#@# %s(%d) cmd(%d) ack fail %d\n",
					__func__, __LINE__, cmd, ret);
		}
	break;
	default:
		pr_info("#@# %s(%d) wrong cmd(%d)!!!\n",
			__func__, __LINE__, cmd);
	break;
	}

	return ret;
}
#else
int cm_mgr_to_sspm_command(u32 cmd, int val)
{
	unsigned int ret = 0;
	struct cm_mgr_data cm_mgr_d;
	int ack_data;

	switch (cmd) {
	case IPI_CM_MGR_INIT:
	case IPI_CM_MGR_ENABLE:
	case IPI_CM_MGR_OPP_ENABLE:
	case IPI_CM_MGR_SSPM_ENABLE:
	case IPI_CM_MGR_BLANK:
	case IPI_CM_MGR_DISABLE_FB:
	case IPI_CM_MGR_DRAM_TYPE:
	case IPI_CM_MGR_CPU_POWER_RATIO_UP:
	case IPI_CM_MGR_CPU_POWER_RATIO_DOWN:
	case IPI_CM_MGR_VCORE_POWER_RATIO_UP:
	case IPI_CM_MGR_VCORE_POWER_RATIO_DOWN:
	case IPI_CM_MGR_DEBOUNCE_UP:
	case IPI_CM_MGR_DEBOUNCE_DOWN:
	case IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB:
	case IPI_CM_MGR_DRAM_LEVEL:
	case IPI_CM_MGR_LIGHT_LOAD_CPS:
	case IPI_CM_MGR_LOADING_ENABLE:
	case IPI_CM_MGR_LOADING_LEVEL:
	case IPI_CM_MGR_EMI_DEMAND_CHECK:
	case IPI_CM_MGR_BCPU_WEIGHT_MAX_SET:
	case IPI_CM_MGR_BCPU_WEIGHT_MIN_SET:
		cm_mgr_d.cmd = cmd;
		cm_mgr_d.arg = val;
		ret = sspm_ipi_send_sync(IPI_ID_CM, IPI_OPT_POLLING,
				&cm_mgr_d, CM_MGR_D_LEN, &ack_data, 1);
		if (ret != 0) {
			pr_info("#@# %s(%d) cmd(%d) error, return %d\n",
					__func__, __LINE__, cmd, ret);
		} else if (ack_data < 0) {
			ret = ack_data;
			pr_info("#@# %s(%d) cmd(%d) return %d\n",
					__func__, __LINE__, cmd, ret);
		}
		break;
	default:
		pr_info("#@# %s(%d) wrong cmd(%d)!!!\n",
				__func__, __LINE__, cmd);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cm_mgr_to_sspm_command);
#endif /* USE_SSPM_VER_V2 */
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */

void __weak dbg_cm_mgr_platform_show(struct seq_file *m) {}

void __weak dbg_cm_mgr_platform_write(int len, char *cmd, u32 val_1, u32 val_2)
{}

void cm_mgr_cpu_map_update_table(void)
{
	int i;

	for (i = 0; i < cm_mgr_cpu_opp_size; i++) {
		if (i < cm_mgr_cpu_map_skip_cpu_opp)
			cm_mgr_cpu_opp_to_dram[i] = cm_mgr_cpu_map_emi_opp;
		else
			cm_mgr_cpu_opp_to_dram[i] =
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE;
	}
}
EXPORT_SYMBOL_GPL(cm_mgr_cpu_map_update_table);

static ssize_t dbg_cm_mgr_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buff)
{
	int i;
	int len = 0;

#define cm_mgr_print(...) \
	snprintf(buff + len, (4096 - len) > 0 ? (4096 - len) : 0, __VA_ARGS__)

	len += cm_mgr_print("cm_mgr_opp_enable %d\n", cm_mgr_opp_enable);
	len += cm_mgr_print("cm_mgr_enable %d\n", cm_mgr_enable);
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	len += cm_mgr_print("cm_mgr_sspm_enable %d\n", cm_mgr_sspm_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	len += cm_mgr_print("cm_mgr_perf_enable %d\n",
			cm_mgr_perf_enable);
	len += cm_mgr_print("cm_mgr_perf_force_enable %d\n",
			cm_mgr_perf_force_enable);
	if (cm_mgr_use_cpu_to_dram_map) {
		len += cm_mgr_print("cm_mgr_cpu_map_dram_enable %d\n",
				cm_mgr_cpu_map_dram_enable);
	}
	if (cm_mgr_use_cpu_to_dram_map_new) {
		len += cm_mgr_print("cm_mgr_cpu_map_emi_opp %d\n",
				cm_mgr_cpu_map_emi_opp);
		len += cm_mgr_print("cm_mgr_cpu_map_skip_cpu_opp %d\n",
				cm_mgr_cpu_map_skip_cpu_opp);

		len += cm_mgr_print("cm_mgr_cpu_opp_to_dram table");
		for (i = 0; i < cm_mgr_cpu_opp_size; i++)
			len += cm_mgr_print(" %d", cm_mgr_cpu_opp_to_dram[i]);
		len += cm_mgr_print("\n");
	}

	len += cm_mgr_print("cm_mgr_disable_fb %d\n", cm_mgr_disable_fb);
	len += cm_mgr_print("light_load_cps %d\n", light_load_cps);
	len += cm_mgr_print("total_bw_value %d\n", total_bw_value);
	len += cm_mgr_print("cm_mgr_loop_count %d\n", cm_mgr_loop_count);
	len += cm_mgr_print("cm_mgr_dram_level %d\n", cm_mgr_dram_level);
	len += cm_mgr_print("cm_mgr_loading_level %d\n", cm_mgr_loading_level);
	len += cm_mgr_print("cm_mgr_loading_enable %d\n",
			cm_mgr_loading_enable);
	len += cm_mgr_print("cm_mgr_emi_demand_check %d\n",
			cm_mgr_emi_demand_check);

	len += cm_mgr_print("cpu_power_ratio_up");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", cpu_power_ratio_up[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("cpu_power_ratio_down");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", cpu_power_ratio_down[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("vcore_power_ratio_up");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", vcore_power_ratio_up[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("vcore_power_ratio_down");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", vcore_power_ratio_down[i]);
	len += cm_mgr_print("\n");

	if (cm_mgr_use_bcpu_weight) {
		len += cm_mgr_print("cpu_power_bcpu_weight_max %d\n",
				cpu_power_bcpu_weight_max);
		len += cm_mgr_print("cpu_power_bcpu_weight_min %d\n",
				cpu_power_bcpu_weight_min);
	}

	len += cm_mgr_print("debounce_times_up_adb");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", debounce_times_up_adb[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("debounce_times_down_adb");
	for (i = 0; i < cm_mgr_num_array; i++)
		len += cm_mgr_print(" %d", debounce_times_down_adb[i]);
	len += cm_mgr_print("\n");

	len += cm_mgr_print("debounce_times_reset_adb %d\n",
			debounce_times_reset_adb);
	len += cm_mgr_print("debounce_times_perf_down %d\n",
			debounce_times_perf_down);
	len += cm_mgr_print("debounce_times_perf_force_down %d\n",
			debounce_times_perf_force_down);

	len += cm_mgr_print("\n");

	return (len > 4096) ? 4096 : len;
}

static ssize_t dbg_cm_mgr_store(struct  kobject *kobj,
		struct kobj_attribute *attr, const char *buff, size_t count)
{

	int ret;
	char cmd[64];
	u32 val_1;
	u32 val_2;

	ret = sscanf(buff, "%63s %d %d", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "cm_mgr_enable")) {
		cm_mgr_enable = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE,
				cm_mgr_enable);
	} else if (!strcmp(cmd, "cm_mgr_sspm_enable")) {
		cm_mgr_sspm_enable = val_1;
		cm_mgr_to_sspm_command(IPI_CM_MGR_SSPM_ENABLE,
				cm_mgr_sspm_enable);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cm_mgr_perf_enable")) {
		cm_mgr_perf_enable = val_1;
	} else if (!strcmp(cmd, "cm_mgr_perf_force_enable")) {
		cm_mgr_perf_force_enable = val_1;
		cm_mgr_perf_set_force_status(val_1);
	} else if (!strcmp(cmd, "cm_mgr_disable_fb")) {
		cm_mgr_disable_fb = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DISABLE_FB,
				cm_mgr_disable_fb);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "light_load_cps")) {
		light_load_cps = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_LIGHT_LOAD_CPS, val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "total_bw_value")) {
		total_bw_value = val_1;
	} else if (!strcmp(cmd, "cm_mgr_loop_count")) {
		cm_mgr_loop_count = val_1;
	} else if (!strcmp(cmd, "cm_mgr_dram_level")) {
		cm_mgr_dram_level = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_LEVEL, val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cm_mgr_loading_level")) {
		cm_mgr_loading_level = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_LEVEL, val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cm_mgr_loading_enable")) {
		cm_mgr_loading_enable = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_ENABLE, val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cm_mgr_emi_demand_check")) {
		cm_mgr_emi_demand_check = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_EMI_DEMAND_CHECK, val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cpu_power_ratio_up")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			cpu_power_ratio_up[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_UP,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cpu_power_ratio_down")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			cpu_power_ratio_down[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_CPU_POWER_RATIO_DOWN,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "vcore_power_ratio_up")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			vcore_power_ratio_up[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_UP,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "vcore_power_ratio_down")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			vcore_power_ratio_down[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_VCORE_POWER_RATIO_DOWN,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "debounce_times_up_adb")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			debounce_times_up_adb[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_UP,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "debounce_times_down_adb")) {
		if (ret == 3 && val_1 < cm_mgr_num_array)
			debounce_times_down_adb[val_1] = val_2;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_DOWN,
				val_1 << 16 | val_2);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "debounce_times_reset_adb")) {
		debounce_times_reset_adb = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB,
				debounce_times_reset_adb);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */
	} else if (!strcmp(cmd, "cpu_power_bcpu_weight_max")) {
		if (cpu_power_bcpu_weight_max < cpu_power_bcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bcpu_weight_max = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MAX_SET,
					val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		}
	} else if (!strcmp(cmd, "cpu_power_bcpu_weight_min")) {
		if (cpu_power_bcpu_weight_max < cpu_power_bcpu_weight_min) {
			ret = -1;
		} else {
			cpu_power_bcpu_weight_min = val_1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
			cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MIN_SET,
					val_1);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		}
	} else if (!strcmp(cmd, "debounce_times_perf_down")) {
		debounce_times_perf_down = val_1;
	} else if (!strcmp(cmd, "debounce_times_perf_force_down")) {
		debounce_times_perf_force_down = val_1;
	} else if (!strcmp(cmd, "1")) {
		/* cm_mgr_perf_force_enable */
		cm_mgr_perf_force_enable = 1;
		cm_mgr_perf_set_force_status(cm_mgr_perf_force_enable);
	} else if (!strcmp(cmd, "0")) {
		/* cm_mgr_perf_force_enable */
		cm_mgr_perf_force_enable = 0;
		cm_mgr_perf_set_force_status(cm_mgr_perf_force_enable);
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_dram_enable")) {
		cm_mgr_cpu_map_dram_enable = !!val_1;
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_skip_cpu_opp")) {
		cm_mgr_cpu_map_skip_cpu_opp = val_1;
		cm_mgr_cpu_map_update_table();
	} else if (!strcmp(cmd, "cm_mgr_cpu_map_emi_opp")) {
		cm_mgr_cpu_map_emi_opp = val_1;
		cm_mgr_cpu_map_update_table();
	} else {
		dbg_cm_mgr_platform_write(ret, cmd, val_1, val_2);
	}

out:
	if (ret < 0)
		return ret;
	return count;
}

static struct kobj_attribute dbg_cm_mgr_attribute =
__ATTR(dbg_cm_mgr, 0644, dbg_cm_mgr_show, dbg_cm_mgr_store);

static struct attribute *attrs[] = {
	&dbg_cm_mgr_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

int cm_mgr_check_dts_setting(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	const char *buf;
	int ret;
	int opp_count;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cm_mgr_base");
	cm_mgr_base = devm_ioremap_resource(dev, res);

	if (IS_ERR((void const *) cm_mgr_base)) {
		pr_info("[CM_MGR] Unable to ioremap registers\n");
		return -1;
	}

	pr_info("[CM_MGR] platform-cm_mgr cm_mgr_base=%p\n",
			cm_mgr_base);

	/* cm_mgr_cpu_opp_to_dram */
	opp_count = of_count_phandle_with_args(node,
			"cm_mgr_cpu_opp_to_dram", NULL);
	pr_info("#@# %s(%d) opp_count %d\n",
			__func__, __LINE__, opp_count);

	if (opp_count > 0)
		cm_mgr_cpu_opp_size = opp_count;
	else
		cm_mgr_cpu_opp_size = 16;

	cm_mgr_cpu_opp_to_dram = devm_kzalloc(dev,
			sizeof(int) * cm_mgr_cpu_opp_size, GFP_KERNEL);

	if (!cm_mgr_cpu_opp_to_dram) {
		ret = -ENOMEM;
		goto ERROR;
	}

	if (opp_count > 0) {
		ret = of_property_read_u32_array(node, "cm_mgr_cpu_opp_to_dram",
				cm_mgr_cpu_opp_to_dram, cm_mgr_cpu_opp_size);
	}

	ret = of_property_read_string(node,
			"status", (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_enable = 1;
		else
			cm_mgr_enable = 0;
	}
	pr_info("#@# %s(%d) cm_mgr_enable %d\n",
			__func__, __LINE__, cm_mgr_enable);

	ret = of_property_read_string(node,
			"use_bcpu_weight", (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_use_bcpu_weight = 1;
		else
			cm_mgr_use_bcpu_weight = 0;
	}
	pr_info("#@# %s(%d) cm_mgr_use_bcpu_weight %d\n",
			__func__, __LINE__, cm_mgr_use_bcpu_weight);

	ret = of_property_read_string(node,
			"use_cpu_to_dram_map", (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_use_cpu_to_dram_map = 1;
		else
			cm_mgr_use_cpu_to_dram_map = 0;
	}
	pr_info("#@# %s(%d) cm_mgr_use_cpu_to_dram_map %d\n",
			__func__, __LINE__, cm_mgr_use_cpu_to_dram_map);

	ret = of_property_read_string(node,
			"use_cpu_to_dram_map_new", (const char **)&buf);
	if (!ret) {
		if (!strcmp(buf, "enable"))
			cm_mgr_use_cpu_to_dram_map_new = 1;
		else
			cm_mgr_use_cpu_to_dram_map_new = 0;
	}
	pr_info("#@# %s(%d) cm_mgr_use_cpu_to_dram_map_new %d\n",
			__func__, __LINE__, cm_mgr_use_cpu_to_dram_map_new);

	ret = of_property_read_s32(node, "cpu_power_bcpu_weight_max",
			&cpu_power_bcpu_weight_max);
	if (!ret)
		cpu_power_bcpu_weight_max = 100;
	pr_info("#@# %s(%d) cpu_power_bcpu_weight_max %d\n",
			__func__, __LINE__, cpu_power_bcpu_weight_max);

	ret = of_property_read_s32(node, "cpu_power_bcpu_weight_min",
			&cpu_power_bcpu_weight_min);
	if (!ret)
		cpu_power_bcpu_weight_min = 100;
	pr_info("#@# %s(%d) cpu_power_bcpu_weight_min %d\n",
			__func__, __LINE__, cpu_power_bcpu_weight_min);

	/* cm_mgr args */
	cm_mgr_buf = devm_kzalloc(dev, sizeof(int) * 6 * cm_mgr_num_array,
			GFP_KERNEL);

	if (!cm_mgr_buf) {
		ret = -ENOMEM;
		goto ERROR1;
	}

	cpu_power_ratio_down = cm_mgr_buf;
	cpu_power_ratio_up = cpu_power_ratio_down + cm_mgr_num_array;
	debounce_times_down_adb = cpu_power_ratio_up + cm_mgr_num_array;
	debounce_times_up_adb = debounce_times_down_adb + cm_mgr_num_array;
	vcore_power_ratio_down = debounce_times_up_adb + cm_mgr_num_array;
	vcore_power_ratio_up = vcore_power_ratio_down + cm_mgr_num_array;

	ret = of_property_read_u32_array(node, "cm_mgr,cp_down",
			cpu_power_ratio_down, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,cp_up",
			cpu_power_ratio_up, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,dt_down",
			debounce_times_down_adb, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,dt_up",
			debounce_times_up_adb, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,vp_down",
			vcore_power_ratio_down, cm_mgr_num_array);
	ret = of_property_read_u32_array(node, "cm_mgr,vp_up",
			vcore_power_ratio_up, cm_mgr_num_array);

	return 0;

ERROR1:
	kfree(cm_mgr_cpu_opp_to_dram);
ERROR:
	return ret;
}
EXPORT_SYMBOL_GPL(cm_mgr_check_dts_setting);

int cm_mgr_common_init(void)
{
	int ret;

	cm_mgr_kobj = kobject_create_and_add("cm_mgr", kernel_kobj);
	if (!cm_mgr_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(cm_mgr_kobj, &attr_group);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO CREATE FILESYSTEM (%d)\n", ret);
		kobject_put(cm_mgr_kobj);

		return ret;
	}

	ret = fb_register_client(&cm_mgr_fb_notifier);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO REGISTER FB CLIENT (%d)\n", ret);
		return ret;
	}

#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
	cm_mgr_to_sspm_command(IPI_CM_MGR_INIT, 0);

	cm_mgr_to_sspm_command(IPI_CM_MGR_ENABLE,
			cm_mgr_enable);

	cm_mgr_to_sspm_command(IPI_CM_MGR_SSPM_ENABLE,
			cm_mgr_sspm_enable);

	cm_mgr_to_sspm_command(IPI_CM_MGR_EMI_DEMAND_CHECK,
			cm_mgr_emi_demand_check);

	cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_LEVEL,
			cm_mgr_loading_level);

	cm_mgr_to_sspm_command(IPI_CM_MGR_LOADING_ENABLE,
			cm_mgr_loading_enable);

	cm_mgr_to_sspm_command(IPI_CM_MGR_DEBOUNCE_TIMES_RESET_ADB,
			debounce_times_reset_adb);

	if (cm_mgr_use_bcpu_weight) {
		cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MAX_SET,
				cpu_power_bcpu_weight_max);

		cm_mgr_to_sspm_command(IPI_CM_MGR_BCPU_WEIGHT_MIN_SET,
				cpu_power_bcpu_weight_min);
	}

#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT && defined(USE_CM_MGR_AT_SSPM) */

	return 0;
}
EXPORT_SYMBOL_GPL(cm_mgr_common_init);

void cm_mgr_common_exit(void)
{
	int ret;

	kobject_put(cm_mgr_kobj);

	ret = fb_unregister_client(&cm_mgr_fb_notifier);
	if (ret)
		pr_info("[CM_MGR] FAILED TO UNREGISTER FB CLIENT (%d)\n", ret);
}
EXPORT_SYMBOL_GPL(cm_mgr_common_exit);
