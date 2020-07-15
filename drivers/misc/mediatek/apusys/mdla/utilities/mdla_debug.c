// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <utilities/mdla_util.h>
#include <utilities/mdla_debug.h>
#include <utilities/mdla_profile.h>

static struct dentry *mdla_dbg_root;

struct mdla_dbgfs_file {
	union {
		u64 u64_var;
		u32 u32_var;
	};
	umode_t mode;
	char hex;
	char *str;
};

static struct mdla_dbgfs_file ull_dbgfs_file[NF_MDLA_DEBUG_FS_U64 + 1] = {
	[FS_CFG_PMU_PERIOD] = { .mode = 0660, .str = "period"},
	[NF_MDLA_DEBUG_FS_U64] = { .str = "unknown"},
};

static struct mdla_dbgfs_file u_dbgfs_file[NF_MDLA_DEBUG_FS_U32 + 1] = {
	[FS_C1]  = { .mode = 0660, .hex = 1, .str = "c1"},
	[FS_C2]  = { .mode = 0660, .hex = 1, .str = "c2"},
	[FS_C3]  = { .mode = 0660, .hex = 1, .str = "c3"},
	[FS_C4]  = { .mode = 0660, .hex = 1, .str = "c4"},
	[FS_C5]  = { .mode = 0660, .hex = 1, .str = "c5"},
	[FS_C6]  = { .mode = 0660, .hex = 1, .str = "c6"},
	[FS_C7]  = { .mode = 0660, .hex = 1, .str = "c7"},
	[FS_C8]  = { .mode = 0660, .hex = 1, .str = "c8"},
	[FS_C9]  = { .mode = 0660, .hex = 1, .str = "c9"},
	[FS_C10] = { .mode = 0660, .hex = 1, .str = "c10"},
	[FS_C11] = { .mode = 0660, .hex = 1, .str = "c11"},
	[FS_C12] = { .mode = 0660, .hex = 1, .str = "c12"},
	[FS_C13] = { .mode = 0660, .hex = 1, .str = "c13"},
	[FS_C14] = { .mode = 0660, .hex = 1, .str = "c14"},
	[FS_C15] = { .mode = 0660, .hex = 1, .str = "c15"},
	[FS_CFG_ENG0]          = { .mode = 0660, .str = "eng0"},
	[FS_CFG_ENG1]          = { .mode = 0660, .str = "eng1"},
	[FS_CFG_ENG2]          = { .mode = 0660, .str = "eng2"},
	[FS_CFG_ENG11]         = { .mode = 0660, .str = "eng11"},
	[FS_POLLING_CMD_DONE]  = { .mode = 0660, .str = "polling_cmd_period"},
	[FS_DUMP_CMDBUF]       = { .mode = 0660, .str = "dump_cmdbuf_en"},
	[FS_DVFS_RAND]         = { .mode = 0660, .str = "dvfs_rand"},
	[FS_PMU_EVT_BY_APU]    = { .mode = 0660, .str = "pmu_evt_by_apusys"},
	[FS_KLOG]              = { .mode = 0660, .hex = 1, .str = "klog"},
	[FS_POWEROFF_TIME]     = { .mode = 0660, .str = "poweroff_time"},
	[FS_TIMEOUT]           = { .mode = 0660, .str = "timeout"},
	[FS_TIMEOUT_DBG]       = { .mode = 0660, .str = "timeout_dbg"},
	[FS_BATCH_NUM]         = { .mode = 0660, .str = "batch_number"},
	[FS_PREEMPTION_TIMES]  = { .mode = 0660, .str = "preemption_times"},
	[FS_PREEMPTION_DBG]   = { .mode = 0660, .str = "preemption_debug"},
	[NF_MDLA_DEBUG_FS_U32] = { .str = "unknown"},
};

static const char *reason_str[REASON_MAX+1] = {
	"others",
	"driver_init",
	"command_timeout",
	"power_on",
	"preemption",
	"simulator",
	"-"
};

const char *mdla_dbg_get_reason_str(int res)
{
	if ((res < 0) || (res > REASON_MAX))
		res = REASON_MAX;

	return reason_str[res];
}

const char *mdla_dbg_get_u64_node_str(int node)
{
	if (unlikely(node < 0 || node > NF_MDLA_DEBUG_FS_U64))
		node = NF_MDLA_DEBUG_FS_U64;

	return ull_dbgfs_file[node].str;
}

const char *mdla_dbg_get_u32_node_str(int node)
{
	if (unlikely(node < 0 || node > NF_MDLA_DEBUG_FS_U32))
		node = NF_MDLA_DEBUG_FS_U32;

	return u_dbgfs_file[node].str;
}

static void mdla_dbg_dummy_destroy(struct mdla_dev *a0) {}
static int mdla_dbg_dummy_create(struct mdla_dev *a0, struct command_entry *a1)
{
	return -1;
}
static void mdla_dbg_dummy_dump(u32 core_id,
		struct seq_file *s)
{
}
static void mdla_dbg_dummy_mem_show(struct seq_file *s) {}
static bool mdla_dbg_dummy_enable(int node)
{
	return false;
}
static void mdla_dbg_dummy_init(struct device *dev, struct dentry *parent) {}


static struct mdla_dbg_cb_func mdla_debug_callback = {
	.destroy_dump_cmdbuf    = mdla_dbg_dummy_destroy,
	.create_dump_cmdbuf     = mdla_dbg_dummy_create,
	.dump_reg               = mdla_dbg_dummy_dump,
	.memory_show            = mdla_dbg_dummy_mem_show,
	.dbgfs_u64_enable       = mdla_dbg_dummy_enable,
	.dbgfs_u32_enable       = mdla_dbg_dummy_enable,
	.dbgfs_plat_init        = mdla_dbg_dummy_init,
};

struct mdla_dbg_cb_func *mdla_dbg_plat_cb(void)
{
	return &mdla_debug_callback;
}


void mdla_dbg_write_u64(int node, u64 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U64))
		ull_dbgfs_file[node].u64_var = val;
}

void mdla_dbg_write_u32(int node, u32 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U32))
		u_dbgfs_file[node].u32_var = val;
}

u64 mdla_dbg_read_u64(int node)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U64))
		return ull_dbgfs_file[node].u64_var;
	return ~0;
}

u32 mdla_dbg_read_u32(int node)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U32))
		return u_dbgfs_file[node].u32_var;
	return ~0;
}

void mdla_dbg_sub_u64(int node, u64 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U64))
		ull_dbgfs_file[node].u64_var -= val;
}

void mdla_dbg_sub_u32(int node, u32 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U32))
		u_dbgfs_file[node].u32_var -= val;
}

void mdla_dbg_add_u64(int node, u64 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U64))
		ull_dbgfs_file[node].u64_var += val;
}

void mdla_dbg_add_u32(int node, u32 val)
{
	if (likely(node >= 0 && node < NF_MDLA_DEBUG_FS_U32))
		u_dbgfs_file[node].u32_var += val;
}

void mdla_dbg_dump(struct mdla_dev *mdla_info, struct command_entry *ce)
{
	mdla_debug_callback.dump_reg(mdla_info->mdla_id, NULL);
	mdla_debug_callback.create_dump_cmdbuf(mdla_info, ce);
	/* FIXME: apusys platform code doesn't ready */
	//apusys_reg_dump();
	mdla_aee_warn("MDLA", "MDLA timeout");
}

static int mdla_dbg_register_show(struct seq_file *s, void *data)
{
	int i;

	for_each_mdla_core(i)
		mdla_debug_callback.dump_reg(i, s);
	return 0;
}

static int mdla_dbg_memory_show(struct seq_file *s, void *data)
{
	mdla_debug_callback.memory_show(s);
	return 0;
}

struct dentry *mdla_dbg_get_fs_root(void)
{
	return mdla_dbg_root;
}

void mdla_dbg_fs_setup(struct device *dev)
{
	struct mdla_dbgfs_file *file;
	int i;

	if (!dev) {
		mdla_drv_debug("%s(): Fail. No device!\n", __func__);
		return;
	}

	if (!mdla_dbg_root) {
		mdla_drv_debug("%s(): dentry NOT ready\n", __func__);
		return;
	}

	for (i = 0; i < NF_MDLA_DEBUG_FS_U64; i++) {
		if (mdla_debug_callback.dbgfs_u64_enable(i)) {
			file = &ull_dbgfs_file[i];
			debugfs_create_u64(file->str, file->mode,
						mdla_dbg_root, &file->u64_var);
		}
	}

	for (i = 0; i < NF_MDLA_DEBUG_FS_U32; i++) {
		if (mdla_debug_callback.dbgfs_u32_enable(i)) {
			file = &u_dbgfs_file[i];
			if (file->hex)
				debugfs_create_x32(file->str, file->mode,
						mdla_dbg_root, &file->u32_var);
			else
				debugfs_create_u32(file->str, file->mode,
						mdla_dbg_root, &file->u32_var);
		}
	}

	debugfs_create_devm_seqfile(dev, DBGFS_HW_REG_NAME, mdla_dbg_root,
				mdla_dbg_register_show);
	debugfs_create_devm_seqfile(dev, DBGFS_CMDBUF_NAME, mdla_dbg_root,
				mdla_dbg_memory_show);

	/* Platform debug node */
	mdla_debug_callback.dbgfs_plat_init(dev, mdla_dbg_root);
}

void mdla_dbg_fs_init(struct dentry *droot)
{
	mdla_dbg_root = debugfs_create_dir("mdla", droot);
	if (IS_ERR_OR_NULL(mdla_dbg_root)) {
		mdla_drv_debug("failed to create mdla debugfs\n");
		return;
	}

	/* AP&uP common node */
}

void mdla_dbg_fs_exit(void)
{
	debugfs_remove_recursive(mdla_dbg_root);
}

