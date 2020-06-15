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

enum MDLA_DEBUG_FS_PROF {
	PROF_PMU_TIMER_STOP,
	PROF_PMU_TIMER_START,
};

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

static struct mdla_dbgfs_file ull_dbgfs_file[NF_MDLA_DEBUG_FS_U64] = {
	[FS_CFG_PERIOD] = { .mode = 0660, .str = "period"},
};

static struct mdla_dbgfs_file u_dbgfs_file[NF_MDLA_DEBUG_FS_U32] = {
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
	[FS_CFG_CMD_TRACE]     = { .mode = 0660, .str = "cmd_trace"},
	[FS_CFG_ENG0]          = { .mode = 0660, .str = "eng0"},
	[FS_CFG_ENG1]          = { .mode = 0660, .str = "eng1"},
	[FS_CFG_ENG2]          = { .mode = 0660, .str = "eng2"},
	[FS_CFG_ENG11]         = { .mode = 0660, .str = "eng11"},
	[FS_CFG_OP_TRACE]      = { .mode = 0660, .str = "op_trace"},
	//[FS_CFG_TIMER_EN]      = { .mode = 0660, .str = "prof_start"},
	[FS_CFG_TIMER_EN]	   = { .mode = 0440, .str = "pmu_timer_st"},
	[FS_DUMP_CMDBUF]       = { .mode = 0660, .str = "dump_cmdbuf_en"},
	[FS_DVFS_RAND]         = { .mode = 0660, .str = "dvfs_rand"},
	[FS_NN_PMU_POLLING]    = { .mode = 0660, .str = "nn_pmu_polling"},
	[FS_KLOG]              = { .mode = 0660, .hex = 1, .str = "klog"},
	[FS_POWEROFF_TIME]     = { .mode = 0660, .str = "poweroff_time"},
	[FS_TIMEOUT]           = { .mode = 0660, .str = "timeout"},
	[FS_TIMEOUT_DBG]       = { .mode = 0660, .str = "timeout_dbg"},
	[FS_BATCH_NUM]         = { .mode = 0660, .str = "batch_number"},
	[FS_PREEMPTION_TIMES]  = { .mode = 0660, .str = "preemption_times"},
	[FS_PREEMPTION_DBG]   = { .mode = 0660, .str = "preemption_debug"},
};

static const char *reason_str[REASON_MAX+1] = {
	"others",
	"driver_init",
	"command_timeout",
	"power_on",
	"preemption",
	"-"
};

const char *mdla_dbg_get_reason_str(int res)
{
	if ((res < 0) || (res > REASON_MAX))
		res = REASON_MAX;

	return reason_str[res];
}

static void mdla_dbg_dummy_destroy(struct mdla_dev *a0) {}
static int mdla_dbg_dummy_create(struct mdla_dev *a0, struct command_entry *a1)
{
	return -1;
}
static void mdla_dbg_dummy_dump(unsigned int core_id,
		struct seq_file *s)
{
}
static void mdla_dbg_dummy_mem_show(struct seq_file *s) {}
static bool mdla_dbg_dummy_enable(unsigned int node)
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


void mdla_dbg_write_u64(unsigned int node, u64 val)
{
	if (node < NF_MDLA_DEBUG_FS_U64)
		ull_dbgfs_file[node].u64_var = val;
}

void mdla_dbg_write_u32(unsigned int node, u32 val)
{
	if (node < NF_MDLA_DEBUG_FS_U32)
		u_dbgfs_file[node].u32_var = val;
}

u64 mdla_dbg_read_u64(unsigned int node)
{
	return node < NF_MDLA_DEBUG_FS_U64 ? ull_dbgfs_file[node].u64_var : ~0;
}

u32 mdla_dbg_read_u32(unsigned int node)
{
	return node < NF_MDLA_DEBUG_FS_U32 ? u_dbgfs_file[node].u32_var : ~0;
}

void mdla_dbg_sub_u64(unsigned int node, u64 val)
{
	if (likely(node < NF_MDLA_DEBUG_FS_U64))
		ull_dbgfs_file[node].u64_var -= val;
}

void mdla_dbg_sub_u32(unsigned int node, u32 val)
{
	if (likely(node < NF_MDLA_DEBUG_FS_U32))
		u_dbgfs_file[node].u32_var -= val;
}

void mdla_dbg_add_u64(unsigned int node, u64 val)
{
	if (likely(node < NF_MDLA_DEBUG_FS_U64))
		ull_dbgfs_file[node].u64_var += val;
}

void mdla_dbg_add_u32(unsigned int node, u32 val)
{
	if (likely(node < NF_MDLA_DEBUG_FS_U32))
		u_dbgfs_file[node].u32_var += val;
}

void mdla_dbg_dump_register(struct seq_file *s)
{
	int i;

	for_each_mdla_core(i)
		mdla_debug_callback.dump_reg(i, s);
}

void mdla_dbg_dump(struct mdla_dev *mdla_info, struct command_entry *ce)
{
	mdla_debug_callback.dump_reg(mdla_info->mdla_id, NULL);
	mdla_debug_callback.create_dump_cmdbuf(mdla_info, ce);
	/* FIXME: apusys platform code doesn't exist */
	//apusys_reg_dump();
	mdla_aee_warn("MDLA", "MDLA timeout");
}

static int mdla_dbg_prof_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "period=%llu\n", ull_dbgfs_file[FS_CFG_PERIOD].u64_var);
	seq_printf(s, "op_trace=%u\n", u_dbgfs_file[FS_CFG_OP_TRACE].u32_var);

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		seq_printf(s, "c%d=0x%x\n",
			(i+1), u_dbgfs_file[FS_C1 + i].u32_var);

	mdla_prof_info_show(s);

	seq_puts(s, "==== usage ====\n");
	seq_puts(s, "echo [param] > /d/mdla/prof_start\n");
	seq_puts(s, "param:\n");
	seq_printf(s, " %2d: stop pmu polling timer\n",
			PROF_PMU_TIMER_STOP);
	seq_printf(s, " %2d: start pmu polling timer\n",
			PROF_PMU_TIMER_START);

	return 0;
}

static ssize_t mdla_dbg_prof_write(struct file *flip,
		const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	char *buf;
	u32 param;
	int i, prio, ret = 0;
	struct mdla_util_pmu_ops *pmu_ops;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = copy_from_user(buf, buffer, count);
	if (ret)
		goto out;

	buf[count] = '\0';

	if (kstrtouint(buf, 10, &param) != 0) {
		ret = -EINVAL;
		goto out;
	}

	switch (param) {
	case PROF_PMU_TIMER_STOP:
		u_dbgfs_file[FS_CFG_TIMER_EN].u32_var = 0;
		mdla_prof_pmu_timer_stop();
		break;
	case PROF_PMU_TIMER_START:
		mdla_prof_pmu_timer_stop();
		pmu_ops = mdla_util_pmu_ops_get();

		for_each_mdla_core(i) {
			for (prio = 0; prio < PRIORITY_LEVEL; prio++) {
				struct mdla_pmu_info *pmu;

				pmu = pmu_ops->get_info(i, prio);

				if (!pmu)
					continue;

				pmu_ops->clr_counter_variable(pmu);
				pmu_ops->set_percmd_mode(pmu, NORMAL);
			}

			pmu_ops->reset_counter(i);
		}
		mdla_prof_pmu_timer_start();
		u_dbgfs_file[FS_CFG_TIMER_EN].u32_var = 1;
		break;
	default:
		break;
	}

out:
	kfree(buf);
	return count;
}

static int mdla_dbg_prof_open(struct inode *inode, struct file *file)
{
	return single_open(file, mdla_dbg_prof_show, inode->i_private);
}

static const struct file_operations mdla_dbg_prof_fops = {
	.open = mdla_dbg_prof_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = mdla_dbg_prof_write,
};

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

	debugfs_create_devm_seqfile(dev, "register", mdla_dbg_root,
				mdla_dbg_register_show);
	debugfs_create_devm_seqfile(dev, "mdla_memory", mdla_dbg_root,
				mdla_dbg_memory_show);

	//debugfs_create_file("prof", 0644, mdla_dbg_root,
	//		NULL, &mdla_dbg_prof_fops);
	debugfs_create_file("prof_start", 0644, mdla_dbg_root,
			NULL, &mdla_dbg_prof_fops);

	mdla_debug_callback.dbgfs_plat_init(dev, mdla_dbg_root);
}

void mdla_dbg_fs_init(struct dentry *apusys_dbg_root)
{
	//mdla_dbg_root = debugfs_create_dir("mdla", apusys_dbg_root);
	mdla_dbg_root = debugfs_create_dir("mdla", NULL);
	if (IS_ERR_OR_NULL(mdla_dbg_root)) {
		mdla_drv_debug("failed to create mdla debugfs\n");
		return;
	}
}

void mdla_dbg_fs_exit(void)
{
	debugfs_remove_recursive(mdla_dbg_root);
}

