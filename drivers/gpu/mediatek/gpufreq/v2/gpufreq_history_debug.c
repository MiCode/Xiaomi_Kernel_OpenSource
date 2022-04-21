// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_debug.c
 * @brief   History log for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <gpufreq_v2.h>
#include <gpu_misc.h>
#include <gpufreq_history_debug.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */

static struct dentry *DebugFSEntryDir;
static void __iomem *start_log_offs;

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */

static int gpufreq_get_regname_order(struct device_node *node,
	const char *reg_id_name)
{
	int idx = -1;

	idx = of_property_match_string(node, "reg-names", reg_id_name);
	GPUFREQ_LOGI("gpufreq idx:%d", idx);
	if (idx < 0) {
		GPUFREQ_LOGE("gpufreq reg-names:%s not found", reg_id_name);
		return -1;
	}
	return idx;
}

static unsigned int gpufreq_check_reg_num(struct device_node *node)
{
	unsigned int gpufreq_reg_num = 0;

	gpufreq_reg_num = of_property_count_u32_elems(
		node, "reg");
	GPUFREQ_LOGI("gpufreq gpufreq_reg_num:%d", gpufreq_reg_num);
	if (gpufreq_reg_num <= 0) {
		GPUFREQ_LOGE("gpufreq reg not found");
		gpufreq_reg_num = 0;
	}
	gpufreq_reg_num = gpufreq_reg_num >> 2;

	return gpufreq_reg_num;
}


static void __iomem *gpufreq_history_of_ioremap(const char *node_name,
	const char *reg_id_name)
{
	struct device_node *node;
	void __iomem *base;
	unsigned int gpufreq_reg_num = 0;
	int idx = -1;

	node = of_find_compatible_node(NULL, NULL, node_name);
	gpufreq_reg_num = gpufreq_check_reg_num(node);
	idx = gpufreq_get_regname_order(node, reg_id_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

static unsigned int gpufreq_bit_reverse(unsigned int x)
{
	x = ((x >> 8) & 0x00ff00ff) | ((x << 8) & 0xff00ff00);
	x = ((x >> 16) & 0x0000ffff) | ((x << 16) & 0xffff0000);
	return x;
}

static int history_debug_show(struct seq_file *sfile, void *v)
{
	int i = 0;
	unsigned int val;

	start_log_offs = gpufreq_history_of_ioremap("mediatek,gpufreq",
		"sysram_mfg_history");
	GPUFREQ_LOGI("gpufreq start_log_offs: %x", start_log_offs);
	if (unlikely(!start_log_offs)) {
		GPUFREQ_LOGE("Fail to ioremap sysram_base");
		return GPUFREQ_ENOMEM;
	}

	for (i = 0; i < (HISTORY_TOTAL_SIZE>>2); i++) {
		val = readl(start_log_offs + (i<<2));
		seq_printf(sfile, "%08x", gpufreq_bit_reverse(val));
	}

	iounmap(start_log_offs);

	return GPUFREQ_SUCCESS;
}


/* DEBUGFS : initialization */
DEBUG_FOPS_RO(history);


int gpufreq_create_debugfs(void)
{
	int i = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry default_entries[] = {
		DEBUG_ENTRY(history),
	};

	DebugFSEntryDir = debugfs_create_dir(GPUFREQ_DEBUGFS_DIR_NAME, NULL);
	if (!DebugFSEntryDir) {
		GPUFREQ_LOGE("Failed to create /sys/kernel/debug/%s",
			GPUFREQ_DEBUGFS_DIR_NAME);
		return GPUFREQ_ENOMEM;
	}


	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!debugfs_create_file(default_entries[i].name, 0444,
			DebugFSEntryDir, NULL, default_entries[i].fops))
			GPUFREQ_LOGE("fail to create /sys/kernel/debug/%s/%s",
				GPUFREQ_DEBUGFS_DIR_NAME,
				default_entries[i].name);
	}

	return GPUFREQ_SUCCESS;
}


void gpufreq_debugFS_exit(struct dentry *psDir)
{
	debugfs_remove_recursive(psDir);
	DebugFSEntryDir = NULL;
}
