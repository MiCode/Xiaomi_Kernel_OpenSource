// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cpuidle.h>
#include <linux/topology.h>
#include <linux/slab.h>

#include <mtk_cpuidle_sysfs.h>

#include "mtk_cpuidle_status.h"

#define MTK_CPUIDLE_DRIVE_STATE_GET	(0)
#define MTK_CPUIDLE_DRIVE_STATE_SET	(1)
#define ALL_CPU_ID			(100)

#define LPM_CPUIDLE_STATE_OP(op, _priv) ({\
	op.fs_read = lpm_cpuidle_state_read;\
	op.fs_write = lpm_cpuidle_state_write;\
	op.priv = _priv; })

#define LPM_CPUIDLE_STATE_NODE_INIT(_n, _name, _type) ({\
	_n.name = _name;\
	_n.type = _type;\
	LPM_CPUIDLE_STATE_OP(_n.op, &_n); })

enum LPM_CPUIDLE_STATE_NODE_TYPE {
	LPM_CPUIDLE_STATE_NODE_ENABLED,
	LPM_CPUIDLE_STATE_NODE_MAX
};

struct LPM_CPUIDLE_STATE_NODE {
	const char *name;
	enum idle_param type;
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};

struct mtk_lp_sysfs_handle lpm_entry_cpuidle_state;

struct LPM_CPUIDLE_STATE_NODE state_enabled;
struct LPM_CPUIDLE_STATE_NODE state_latency;
struct LPM_CPUIDLE_STATE_NODE state_residency;

struct MTK_CPUIDLE_DRV_INFO {
	int cpu;
	unsigned int type;
	int param;
	unsigned int state_idx;
	unsigned int val;
	char *p;
	size_t *sz;
};

struct lpm_cpu_topology {
	unsigned int id;
	unsigned int cpu_mask;
};
struct lpm_cpu_topology **topology;

static const char *node_string[NF_IDLE_PARAM] = {
	[IDLE_PARAM_EN]		= "Enabled",
	[IDLE_PARAM_LAT]	= "Exit latency",
	[IDLE_PARAM_RES]	= "Target residency",
};

static const char *node_name[NF_IDLE_PARAM] = {
	[IDLE_PARAM_EN]		= "enabled",
	[IDLE_PARAM_LAT]	= "latency",
	[IDLE_PARAM_RES]	= "residency",
};

static long lpm_per_cpuidle_drv_param(void *pData)
{
	struct cpuidle_driver *drv = cpuidle_get_driver();
	struct MTK_CPUIDLE_DRV_INFO *info =
				(struct MTK_CPUIDLE_DRV_INFO *)pData;
	int i = 0;
	size_t sz = *(info->sz);
	char *p = info->p;

	if (!drv)
		return -ENODEV;
	if (info->type == MTK_CPUIDLE_DRIVE_STATE_GET) {
		if (info->cpu == 0) {
			mtk_dbg_cpuidle_log("%-12s:", "state_index");
			for (i = 0; i < drv->state_count; i++)
				mtk_dbg_cpuidle_log("%12d ", i);
			mtk_dbg_cpuidle_log("\n");
		}
		mtk_dbg_cpuidle_log("%11s%d:", "cpu", info->cpu);
		for (i = 0; i < drv->state_count; i++) {
			mtk_dbg_cpuidle_log("%12ld ",
				mtk_cpuidle_get_param(drv, i, info->param));
		}

		mtk_dbg_cpuidle_log("\n");
	} else if (info->type == MTK_CPUIDLE_DRIVE_STATE_SET) {
		mtk_cpuidle_set_param(drv, info->state_idx, info->param,
					info->val);
	}

	*(info->sz) = sz;
	info->p = p;

	return 0;
}

static void cpuidle_state_read_param(char **ToUserBuf, size_t *sz, int param)
{
	int cpu;
	struct MTK_CPUIDLE_DRV_INFO drv_info = {
		.type = MTK_CPUIDLE_DRIVE_STATE_GET,
		.param = param,
		.p = *ToUserBuf,
		.sz = sz,
	};

	for_each_possible_cpu(cpu) {
		drv_info.cpu = cpu;
		work_on_cpu(cpu, lpm_per_cpuidle_drv_param,
				&drv_info);
	}

	*ToUserBuf = drv_info.p;
}

static int idle_proc_state_param_setting(char *cmd, size_t *sz, int param)
{
	char *args;
	unsigned int cpu_mask, state_idx = 0, val = 0;
	int cpu = 0, i = 0;
	struct MTK_CPUIDLE_DRV_INFO drv_info;
	struct cmd_param {
		unsigned int id;
		unsigned int mask;
	};

	args = strsep(&cmd, " ");
	if (!args || kstrtoint(args, 10, &cpu) != 0)
		return -EINVAL;

	args = strsep(&cmd, " ");
	if (!args || kstrtouint(args, 10, &state_idx) != 0)
		return -EINVAL;

	args = strsep(&cmd, " ");
	if (!args || kstrtouint(args, 10, &val) != 0)
		return -EINVAL;

	if (!state_idx)
		return -EINVAL;

	cpu_mask = 0;

	if (cpu >= 0 && cpu < nr_cpu_ids) {
		cpu_mask = (1 << cpu);
	} else {
		while (topology[i]->id != ALL_CPU_ID) {
			if (cpu == topology[i]->id) {
				cpu_mask = topology[i]->cpu_mask;
				break;
			}
			i++;
		}
		if (cpu == topology[i]->id)
			cpu_mask = topology[i]->cpu_mask;
	}

	for_each_possible_cpu(cpu) {
		if ((cpu_mask & (1 << cpu)) == 0)
			continue;

		drv_info.cpu = cpu;
		drv_info.type = MTK_CPUIDLE_DRIVE_STATE_SET;
		drv_info.param = param;
		drv_info.state_idx = state_idx;
		drv_info.val = val;
		drv_info.p = cmd;
		drv_info.sz = sz;

		work_on_cpu(cpu, lpm_per_cpuidle_drv_param,
				&drv_info);
	}

	return 0;
}

static void idle_proc_state_uasge_print(char **ToUserBuf, size_t *size,
						enum idle_param type)
{
	char *p = *ToUserBuf;
	size_t sz = *size;
	u32 num_present_cpus = 0;
	int i = 0;

	num_present_cpus = num_present_cpus();

	if (type > IDLE_PARAM_RES)
		type = IDLE_PARAM_RES;

	mtk_dbg_cpuidle_log("\n======== Command Usage ========\n");
	mtk_dbg_cpuidle_log("%s > /proc/mtk_lpm/cpuidle/state/%s\n",
			"echo [cpu_id] [state_index] [val(dec)]",
			node_name[type]);
	mtk_dbg_cpuidle_log("\t cpu_id: 0~%u -> cpu number\n",
				num_present_cpus);

	while (topology[i]->id != ALL_CPU_ID) {
		mtk_dbg_cpuidle_log("\t         %3d -> all cluster%d CPU\n",
					topology[i]->id, i);
		i++;
	}
	mtk_dbg_cpuidle_log("\t         %3d -> all CPU\n",
					topology[i]->id);

	mtk_dbg_cpuidle_log("\t state_index msut > 0 (index 0 can't be modified)\n");
	mtk_dbg_cpuidle_log("\n");

	*ToUserBuf = p;
}

static ssize_t lpm_cpuidle_state_read(char *ToUserBuf,
					    size_t sz, void *priv)
{
	char *p = ToUserBuf;
	struct LPM_CPUIDLE_STATE_NODE *node =
			(struct LPM_CPUIDLE_STATE_NODE *)priv;

	if (!p || !node)
		return -EINVAL;

		mtk_dbg_cpuidle_log("==== CPU idle state: %s ====\n",
					node_string[node->type]);
		cpuidle_state_read_param(&p, &sz, node->type);
		idle_proc_state_uasge_print(&p, &sz, node->type);

	return p - ToUserBuf;
}

static ssize_t lpm_cpuidle_state_write(char *FromUserBuf,
					  size_t sz, void *priv)
{
	struct LPM_CPUIDLE_STATE_NODE *node =
			(struct LPM_CPUIDLE_STATE_NODE *)priv;
	char cmd[128];
	int parm;

	if (!FromUserBuf || !node)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		idle_proc_state_param_setting(FromUserBuf, &sz, node->type);
		return sz;
	}

	return -EINVAL;
}

static int lpm_topology_init(void)
{
	unsigned int package_id_map = 0, package_num = 0;
	int package_id = 0, cpu = 0;

	for_each_present_cpu(cpu) {
		package_id = cpu_topology[cpu].package_id;
		package_id_map |= (1 << package_id);
	}

	package_num = hweight32(package_id_map);

	topology = kzalloc(sizeof(struct lpm_cpu_topology *) *
				(package_num + 1),
				GFP_KERNEL);
	if (!topology)
		return -ENOMEM;

	for (package_id = 0; package_id <= package_num; package_id++) {
		topology[package_id] = kzalloc(sizeof(struct lpm_cpu_topology),
						GFP_KERNEL);
		if (!topology[package_id]) {
			kfree(topology);
			return -ENOMEM;
		}
	}

	for_each_present_cpu(cpu) {
		package_id = cpu_topology[cpu].package_id;
		topology[package_id]->id = (package_id + 1) * 10;
		topology[package_id]->cpu_mask |= (1 << cpu);
		topology[package_num]->id |= ALL_CPU_ID;
		topology[package_num]->cpu_mask |= (1 << cpu);
	}

	return 0;
}

void lpm_cpuidle_state_init(void)
{
	int ret = 0;

	ret = lpm_topology_init();
	if (ret) {
		pr_info("[name:mtk_lpm][P] - lpm_topology_init error (%s:%d)\n",
		__func__, __LINE__);
		return;
	}

	mtk_cpuidle_sysfs_sub_entry_add("state", MTK_CPUIDLE_SYS_FS_MODE,
				NULL, &lpm_entry_cpuidle_state);

	LPM_CPUIDLE_STATE_NODE_INIT(state_enabled, node_name[IDLE_PARAM_EN],
				    IDLE_PARAM_EN);
	mtk_cpuidle_sysfs_sub_entry_node_add(state_enabled.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&state_enabled.op,
					&lpm_entry_cpuidle_state,
					&state_enabled.handle);

	LPM_CPUIDLE_STATE_NODE_INIT(state_latency, node_name[IDLE_PARAM_LAT],
				    IDLE_PARAM_LAT);
	mtk_cpuidle_sysfs_sub_entry_node_add(state_latency.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&state_latency.op,
					&lpm_entry_cpuidle_state,
					&state_latency.handle);

	LPM_CPUIDLE_STATE_NODE_INIT(state_residency, node_name[IDLE_PARAM_RES],
				    IDLE_PARAM_RES);
	mtk_cpuidle_sysfs_sub_entry_node_add(state_residency.name,
					MTK_CPUIDLE_SYS_FS_MODE,
					&state_residency.op,
					&lpm_entry_cpuidle_state,
					&state_residency.handle);
}
