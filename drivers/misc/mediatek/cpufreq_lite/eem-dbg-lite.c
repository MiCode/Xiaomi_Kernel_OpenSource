// SPDX-License-Identifier: GPL-2.0
/*
 * eem-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */

/* system includes */
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/pm_opp.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "../mcupm/include/mcupm_driver.h"
#include "cpufreq-dbg-lite.h"
#include "eem-dbg-lite.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[EEM]: " fmt

static struct eemsn_log *eemsn_log; /* eem data from csram */

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current voltage after eem
 */
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	unsigned char lock;
	unsigned int locklimit = 0;
	unsigned int bank_id = 0;
	int i;

	while (1) {
		lock = eemsn_log->lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		lock = eemsn_log->lock;
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break;
		/* if unlock, break out while loop, read next det*/
	}

	for (bank_id = 0; bank_id < NR_EEMSN_DET; bank_id++) {
		seq_printf(m, "Cluster:%d, DVFS_TABLE\n", bank_id);
		for (i = 0; i < MAX_NR_FREQ; i++) {
			if (eemsn_log->det_log[bank_id].freq_tbl[i] == 0)
				break;
			seq_printf(m, "[%d],freq = [%hu], eem_volt = [%u]\n",
			i,
			eemsn_log->det_log[bank_id].freq_tbl[i],
			(unsigned int) eemsn_log->det_log[bank_id].volt_tbl_init2[i] * VOLT_STEP);
		}
	}

	return 0;

}

PROC_FOPS_RO(eem_cur_volt);

static int create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *eem_dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};


	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_cur_volt),
	};

	eem_dir = proc_mkdir("eem_lite", NULL);
	for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
		if (!proc_create(eem_entries[i].name, 0664,
					eem_dir, eem_entries[i].fops)) {
			pr_info("[%s]: create /proc/eem_lite/%s failed\n",
					__func__,
					eem_entries[i].name);
		}
	}

	return 0;
}

int mtk_eem_init(struct platform_device *pdev)
{
	int err = 0;
	struct resource *eem_res;

	eem_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (eem_res)
		eemsn_log = ioremap(eem_res->start, resource_size(eem_res));
	else {
		pr_info("%s can't get resource, ret:%d\n", __func__, err);
		return 0;
	}

	return create_debug_fs();
}

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver Helper v0.1.1");
MODULE_AUTHOR("Tungchen Shih <tungchen.shih@mediatek.com>");
MODULE_LICENSE("GPL v2");

