// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpueb_debug.c
 * @brief   Debug mechanism for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "gpueb_helper.h"
#include "gpueb_debug.h"

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
enum gpueb_smc_op {
	GPUEB_SMC_OP_TRIGGER_WDT = 0,
	GPUEB_SMC_OP_NUMBER      = 1,
};

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
void gpueb_trigger_wdt(const char *name)
{
	struct arm_smccc_res res;

	gpueb_pr_info("@%s: GPUEB WDT is triggered by %s\n", __func__, name);

	arm_smccc_smc(
		MTK_SIP_KERNEL_GPUEB_CONTROL,  /* a0 */
		GPUEB_SMC_OP_TRIGGER_WDT,      /* a1 */
		0, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(gpueb_trigger_wdt);

#if defined(CONFIG_PROC_FS)
/* PROCFS: trigger GPUEB WDT */
static int force_trigger_wdt_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[GPUEB-DEBUG] Force trigger GPUEB WDT\n");
	return 0;
}

static ssize_t force_trigger_wdt_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	int ret = 0;
	char buf[64];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len)) {
		ret = -1;
		goto done;
	}
	buf[len] = '\0';

	if (sysfs_streq(buf, WDT_EXCEPTION_EN))
		gpueb_trigger_wdt("GPUEB_DEBUG");

done:
	return (ret < 0) ? ret : count;
}

/* PROCFS : initialization */
PROC_FOPS_RW(force_trigger_wdt);

static int gpueb_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry default_entries[] = {
		PROC_ENTRY(force_trigger_wdt),
	};

	dir = proc_mkdir("gpueb", NULL);
	if (!dir) {
		gpueb_pr_info("@%s: fail to create /proc/gpueb (ENOMEM)\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(default_entries); i++) {
		if (!proc_create(default_entries[i].name, 0660,
			dir, default_entries[i].fops))
			gpueb_pr_info("@%s: fail to create /proc/gpueb/%s\n",
				__func__, default_entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

void gpueb_debug_init(void)
{
	int ret = 0;

#if defined(CONFIG_PROC_FS)
	ret = gpueb_create_procfs();
	if (ret)
		gpueb_pr_info("@%s: fail to create procfs (%d)\n", __func__, ret);
#endif
}
