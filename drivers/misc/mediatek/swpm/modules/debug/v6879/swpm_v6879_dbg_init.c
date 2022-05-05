// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_module_ext.h>
#include <swpm_v6879.h>
#include <swpm_v6879_ext.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


#define SWPM_EXT_DBG (0)

static ssize_t enable_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("echo <type or 65535> <0 or 1> > /proc/swpm/enable\n");
	swpm_dbg_log("SWPM status = 0x%x\n", swpm_status);

	return p - ToUser;
}

static ssize_t enable_write(char *FromUser, size_t sz, void *priv)
{
	int type, enable;

	if (!FromUser)
		return -EINVAL;

	if (sscanf(FromUser, "%d %d", &type, &enable) == 2) {
		swpm_lock(&swpm_mutex);
		swpm_set_enable(type, enable);
		if (swpm_status)
			mod_timer(&swpm_timer, jiffies +
				msecs_to_jiffies(swpm_log_interval_ms));
		else
			del_timer(&swpm_timer);
		swpm_unlock(&swpm_mutex);
	}

	return sz;
}

static const struct mtk_swpm_sysfs_op enable_fops = {
	.fs_read = enable_read,
	.fs_write = enable_write,
};

static ssize_t dump_power_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	size_t mSize = 0;
	int i;

	for (i = 0; i < NR_POWER_RAIL; i++) {
		p += scnprintf(p + mSize, sz - mSize, "%s",
			       swpm_power_rail[i].name);
		if (i != NR_POWER_RAIL - 1)
			p += sprintf(p, "/");
		else
			p += sprintf(p, " = ");
	}

	for (i = 0; i < NR_POWER_RAIL; i++) {
		p += scnprintf(p + mSize, sz - mSize, "%d",
			       swpm_power_rail[i].avg_power);
		if (i != NR_POWER_RAIL - 1)
			p += sprintf(p, "/");
		else
			p += sprintf(p, " uA\n");
	}

	WARN_ON(sz - mSize <= 0);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op dump_power_fops = {
	.fs_read = dump_power_read,
};

static ssize_t dram_bw_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	unsigned long flags;

	if (!ToUser)
		return -EINVAL;

	spin_lock_irqsave(&swpm_snap_spinlock, flags);
	swpm_dbg_log("DRAM BW R/W=%d/%d\n",
		mem_idx_snap.read_bw[0],
		mem_idx_snap.write_bw[0]);
	spin_unlock_irqrestore(&swpm_snap_spinlock, flags);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op dram_bw_fops = {
	.fs_read = dram_bw_read,
};

static unsigned int pmu_ms_mode;
static ssize_t pmu_ms_mode_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("echo <0/1> > /proc/swpm/pmu_ms_mode\n");
	swpm_dbg_log("%d\n", pmu_ms_mode);

	return p - ToUser;
}

static ssize_t pmu_ms_mode_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int enable = 0;

	if (!FromUser)
		return -EINVAL;

	if (!kstrtouint(FromUser, 0, &enable)) {
		pmu_ms_mode = enable;

		/* TODO: remove this path after qos commander ready */
		swpm_set_update_cnt(0, (0x1 << SWPM_CODE_USER_BIT) |
				    pmu_ms_mode);
	}

	return sz;
}

static const struct mtk_swpm_sysfs_op pmu_ms_mode_fops = {
	.fs_read = pmu_ms_mode_read,
	.fs_write = pmu_ms_mode_write,
};

static unsigned int swpm_pmsr_en = 1;
static ssize_t swpm_pmsr_en_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("swpm_pmsr only support disable cmd\n");
	swpm_dbg_log("%d\n", swpm_pmsr_en);

	return p - ToUser;
}

static ssize_t swpm_pmsr_en_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int enable = 0;

	if (!FromUser)
		return -EINVAL;

	if (!kstrtouint(FromUser, 0, &enable)) {
		if (!enable) {
			swpm_pmsr_en = enable;
			swpm_set_update_cnt(0, 9696 << SWPM_CODE_USER_BIT);
		}
	}

	return sz;
}

static const struct mtk_swpm_sysfs_op swpm_pmsr_en_fops = {
	.fs_read = swpm_pmsr_en_read,
	.fs_write = swpm_pmsr_en_write,
};

static unsigned int swpm_pmsr_trigger;
static ssize_t swpm_pmsr_trigger_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("swpm_pmsr_trigger 0/1/2/3:high/low/rising/faling\n");
	swpm_dbg_log("%d\n", swpm_pmsr_trigger);

	return p - ToUser;
}

static ssize_t swpm_pmsr_trigger_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int mode = 0;

	if (!FromUser)
		return -EINVAL;

	if (!kstrtouint(FromUser, 0, &mode)) {
		if (mode < 4) {
			swpm_pmsr_trigger = mode;
			swpm_set_update_cnt(0, 0x2 << SWPM_CODE_USER_BIT |
					    swpm_pmsr_trigger);
		}
	}

	return sz;
}

static const struct mtk_swpm_sysfs_op swpm_pmsr_trigger_fops = {
	.fs_read = swpm_pmsr_trigger_read,
	.fs_write = swpm_pmsr_trigger_write,
};

#if SWPM_EXT_DBG
static ssize_t swpm_sp_test_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	int i, j;
	int32_t core_vol_num, core_ip_num;

	struct ip_stats *core_ip_stats_ptr;
	struct vol_duration *core_duration_ptr;

	if (!ToUser)
		return -EINVAL;

	core_vol_num = get_vcore_vol_num();
	core_ip_num = get_vcore_ip_num();

	core_duration_ptr =
	kmalloc_array(core_vol_num, sizeof(struct vol_duration), GFP_KERNEL);
	core_ip_stats_ptr =
	kmalloc_array(core_ip_num, sizeof(struct ip_stats), GFP_KERNEL);
	for (i = 0; i < core_ip_num; i++)
		core_ip_stats_ptr[i].vol_times =
		kmalloc_array(core_vol_num,
			      sizeof(struct ip_vol_times), GFP_KERNEL);

	sync_latest_data();

	if (!core_duration_ptr) {
		swpm_dbg_log("core_duration_idx failure\n");
		goto End;
	} else if (!core_ip_stats_ptr) {
		swpm_dbg_log("core_ip_stats_idx failure\n");
		goto End;
	}

	get_vcore_vol_duration(core_vol_num, core_duration_ptr);
	get_vcore_ip_vol_stats(core_ip_num, core_vol_num,
			       core_ip_stats_ptr);

	swpm_dbg_log("VCORE_VOL_NUM = %d\n", core_vol_num);
	swpm_dbg_log("VCORE_IP_NUM = %d\n", core_ip_num);


	for (i = 0; i < core_vol_num; i++) {
		swpm_dbg_log("VCORE %d mV : %lld ms\n",
			     core_duration_ptr[i].vol,
			     core_duration_ptr[i].duration);
	}
	for (i = 0; i < core_ip_num; i++) {
		swpm_dbg_log("VCORE IP %s\n",
			     core_ip_stats_ptr[i].ip_name);
		for (j = 0; j < core_vol_num; j++) {
			swpm_dbg_log("%d mV",
			core_ip_stats_ptr[i].vol_times[j].vol);
			swpm_dbg_log("\t active_time : %lld ms",
			core_ip_stats_ptr[i].vol_times[j].active_time);
			swpm_dbg_log("\t idle_time : %lld ms",
			core_ip_stats_ptr[i].vol_times[j].idle_time);
			swpm_dbg_log("\t off_time : %lld ms\n",
			core_ip_stats_ptr[i].vol_times[j].off_time);
		}
	}
End:
	kfree(core_duration_ptr);

	for (i = 0; i < core_ip_num; i++)
		kfree(core_ip_stats_ptr[i].vol_times);
	kfree(core_ip_stats_ptr);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op swpm_sp_test_fops = {
	.fs_read = swpm_sp_test_read,
};

static ssize_t swpm_sp_ddr_idx_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	int i, j;
	int32_t ddr_freq_num, ddr_bc_ip_num;

	struct ddr_act_times *ddr_act_times_ptr;
	struct ddr_sr_pd_times *ddr_sr_pd_times_ptr;
	struct ddr_ip_bc_stats *ddr_ip_stats_ptr;

	if (!ToUser)
		return -EINVAL;

	ddr_freq_num = get_ddr_freq_num();
	ddr_bc_ip_num = get_ddr_data_ip_num();

	ddr_act_times_ptr =
	kmalloc_array(ddr_freq_num, sizeof(struct ddr_act_times), GFP_KERNEL);
	ddr_sr_pd_times_ptr =
	kmalloc(sizeof(struct ddr_sr_pd_times), GFP_KERNEL);
	ddr_ip_stats_ptr =
	kmalloc_array(ddr_bc_ip_num,
		sizeof(struct ddr_ip_bc_stats), GFP_KERNEL);
	for (i = 0; i < ddr_bc_ip_num; i++)
		ddr_ip_stats_ptr[i].bc_stats =
		kmalloc_array(ddr_freq_num,
			      sizeof(struct ddr_bc_stats), GFP_KERNEL);

	sync_latest_data();

	if (!ddr_act_times_ptr) {
		swpm_dbg_log("ddr_act_times_idx failure\n");
		goto End;
	} else if (!ddr_sr_pd_times_ptr) {
		swpm_dbg_log("ddr_sr_pd_times_idx failure\n");
		goto End;
	} else if (!ddr_ip_stats_ptr) {
		swpm_dbg_log("ddr_ip_idx failure\n");
		goto End;
	}

	get_ddr_act_times(ddr_freq_num, ddr_act_times_ptr);
	get_ddr_sr_pd_times(ddr_sr_pd_times_ptr);
	get_ddr_freq_data_ip_stats(ddr_bc_ip_num,
				   ddr_freq_num,
				   ddr_ip_stats_ptr);

	swpm_dbg_log("SR time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->sr_time);
	swpm_dbg_log("PD time(msec): %lld\n",
		   ddr_sr_pd_times_ptr->pd_time);

	for (i = 0; i < ddr_freq_num; i++) {
		swpm_dbg_log("Freq %dMhz: ",
			     ddr_act_times_ptr[i].freq);
		swpm_dbg_log("Time(msec):%lld ",
			     ddr_act_times_ptr[i].active_time);
		for (j = 0; j < ddr_bc_ip_num; j++) {
			swpm_dbg_log("%llu/",
				     ddr_ip_stats_ptr[j].bc_stats[i].value);
		}
		swpm_dbg_log("\n");
	}
End:
	kfree(ddr_act_times_ptr);
	kfree(ddr_sr_pd_times_ptr);

	for (i = 0; i < ddr_bc_ip_num; i++)
		kfree(ddr_ip_stats_ptr[i].bc_stats);
	kfree(ddr_ip_stats_ptr);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op swpm_sp_ddr_idx_fops = {
	.fs_read = swpm_sp_ddr_idx_read,
};
#endif

static void swpm_v6879_dbg_fs_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("enable"
			, 0644, &enable_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("dump_power"
			, 0444, &dump_power_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("dram_bw"
			, 0444, &dram_bw_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("pmu_ms_mode"
			, 0644, &pmu_ms_mode_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_pmsr_en"
			, 0644, &swpm_pmsr_en_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_pmsr_trigger"
			, 0644, &swpm_pmsr_trigger_fops, NULL, NULL);
#if SWPM_EXT_DBG
	mtk_swpm_sysfs_entry_func_node_add("swpm_sp_ddr_idx"
			, 0444, &swpm_sp_ddr_idx_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("swpm_sp_test"
			, 0444, &swpm_sp_test_fops, NULL, NULL);
#endif
}

static int __init swpm_v6879_dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
subsys_initcall(swpm_v6879_dbg_early_initcall);
#endif

static int __init swpm_v6879_dbg_device_initcall(void)
{
	return 0;
}

static int __init swpm_v6879_dbg_late_initcall(void)
{
	/*
	 * use late init call sync to
	 * ensure qos module is ready
	 */
	swpm_dbg_common_fs_init();
	swpm_v6879_init();
	swpm_v6879_ext_init();
	swpm_v6879_dbg_fs_init();
	pr_notice("swpm init success\n");

	return 0;
}
#ifndef MTK_SWPM_KERNEL_MODULE
late_initcall_sync(swpm_v6879_dbg_late_initcall);
#endif

int __init swpm_v6879_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6879_dbg_early_initcall();
#endif
	if (ret)
		goto swpm_v6879_dbg_init_fail;

	ret = swpm_v6879_dbg_device_initcall();

	if (ret)
		goto swpm_v6879_dbg_init_fail;

#ifdef MTK_SWPM_KERNEL_MODULE
	ret = swpm_v6879_dbg_late_initcall();
#endif

	if (ret)
		goto swpm_v6879_dbg_init_fail;

	return 0;
swpm_v6879_dbg_init_fail:
	return -EAGAIN;
}

void __exit swpm_v6879_dbg_exit(void)
{
	swpm_v6879_exit();
	swpm_v6879_ext_exit();
}

module_init(swpm_v6879_dbg_init);
module_exit(swpm_v6879_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6879 software power model debug module");
MODULE_AUTHOR("MediaTek Inc.");
