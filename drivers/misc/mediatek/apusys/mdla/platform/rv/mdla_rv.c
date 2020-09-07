// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/of_device.h>
#include <linux/debugfs.h>

#include <utilities/mdla_debug.h>

#define DBGFS_PMU_NAME      "pmu_reg"
#define DBGFS_USAGE_NAME    "help"

/**
 * DTS version definition
 * [0:7]   : minor version number
 * [8:15]  : major version number
 * [16:31] : project
 */
static unsigned int mdla_ver;
#define get_minor_num(v) ((v) & 0xFF)
#define get_major_num(v) (((v) >> 8) & 0xFF)
#define get_proj_code(v) (((v) >> 16) & 0xFFFF)

static int mdla_dbgfs_u64_create[NF_MDLA_DEBUG_FS_U64] = {
	[FS_CFG_PMU_PERIOD] = 1,
};

static bool mdla_dbgfs_u32_create[NF_MDLA_DEBUG_FS_U32] = {
	[FS_C1]                = 1,
	[FS_C2]                = 1,
	[FS_C3]                = 1,
	[FS_C4]                = 1,
	[FS_C5]                = 1,
	[FS_C6]                = 1,
	[FS_C7]                = 1,
	[FS_C8]                = 1,
	[FS_C9]                = 1,
	[FS_C10]               = 1,
	[FS_C11]               = 1,
	[FS_C12]               = 1,
	[FS_C13]               = 1,
	[FS_C14]               = 1,
	[FS_C15]               = 1,
	[FS_CFG_ENG0]          = 0,
	[FS_CFG_ENG1]          = 0,
	[FS_CFG_ENG2]          = 0,
	[FS_CFG_ENG11]         = 0,
	[FS_POLLING_CMD_DONE]  = 0,
	[FS_DUMP_CMDBUF]       = 0,
	[FS_DVFS_RAND]         = 0,
	[FS_PMU_EVT_BY_APU]    = 0,
	[FS_KLOG]              = 0,
	[FS_POWEROFF_TIME]     = 0,
	[FS_TIMEOUT]           = 0,
	[FS_TIMEOUT_DBG]       = 0,
	[FS_BATCH_NUM]         = 0,
	[FS_PREEMPTION_TIMES]  = 0,
	[FS_PREEMPTION_DBG]    = 0,
};

/* platform static functions */

static bool mdla_plat_dbgfs_u64_enable(int node)
{
	return node >= 0 && node < NF_MDLA_DEBUG_FS_U64
			? mdla_dbgfs_u64_create[node] : false;
}

static bool mdla_plat_dbgfs_u32_enable(int node)
{
	return node >= 0 && node < NF_MDLA_DEBUG_FS_U32
			? mdla_dbgfs_u32_create[node] : false;
}

static int mdla_plat_init_fw(int ver)
{
	return 0;
}

static void mdla_plat_memory_show(struct seq_file *s)
{
	seq_puts(s, "Not support!!\n");
}

static int mdla_plat_register_show(struct seq_file *s, void *data)
{
	return 0;
}

static int mdla_plat_dbgfs_usage(struct seq_file *s, void *data)
{
	return 0;
}

static void mdla_plat_dbgfs_init(struct device *dev, struct dentry *parent)
{
	if (!dev || !parent)
		return;

	debugfs_create_devm_seqfile(dev, DBGFS_PMU_NAME, parent,
				mdla_plat_register_show);
	debugfs_create_devm_seqfile(dev, DBGFS_USAGE_NAME, parent,
				mdla_plat_dbgfs_usage);
}


/* platform public functions */

int mdla_rv_init(struct platform_device *pdev)
{
	struct mdla_dbg_cb_func *dbg_cb = mdla_dbg_plat_cb();

	dev_info(&pdev->dev, "%s()\n", __func__);

	if (of_property_read_u32(pdev->dev.of_node, "version", &mdla_ver) < 0)
		return -1;

	dev_info(&pdev->dev, "0x%x v%d.%d\n",
				get_proj_code(mdla_ver),
				get_major_num(mdla_ver),
				get_minor_num(mdla_ver));

	if (get_major_num(mdla_ver) >= 3 && mdla_plat_init_fw(mdla_ver))
		return -1;

	/* set debug callback */
	dbg_cb->memory_show         = mdla_plat_memory_show;
	dbg_cb->dbgfs_u64_enable    = mdla_plat_dbgfs_u64_enable;
	dbg_cb->dbgfs_u32_enable    = mdla_plat_dbgfs_u32_enable;
	dbg_cb->dbgfs_plat_init     = mdla_plat_dbgfs_init;

	return 0;
}

void mdla_rv_deinit(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);
}
