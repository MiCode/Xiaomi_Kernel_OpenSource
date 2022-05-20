// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

/**
 * @file	mkt_et.c
 * @brief   Driver for et
 *
 */

#define __MTK_ET_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/arm-smccc.h>

#include <linux/io.h>

#ifdef __KERNEL__
	#include <linux/topology.h>
	#include <linux/soc/mediatek/mtk_sip_svc.h>
	#include "mtk_et.h"
#endif

#if IS_ENABLED(CONFIG_OF)
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
#endif

/************************************************
 * Debug print
 ************************************************/

//#define ET_DEBUG
#define ET_TAG	 "[ET]"

#define et_err(fmt, args...)	\
	pr_info(ET_TAG"[ERROR][%s():%d]" fmt, __func__, __LINE__, ##args)
#define et_msg(fmt, args...)	\
	pr_info(ET_TAG"[INFO][%s():%d]" fmt, __func__, __LINE__, ##args)

#ifdef ET_DEBUG
#define et_debug(fmt, args...)	\
	pr_debug(ET_TAG"[DEBUG][%s():%d]" fmt, __func__, __LINE__, ##args)
#else
#define et_debug(fmt, args...)
#endif

/************************************************
 * SMC between kernel and atf
 ************************************************/
unsigned int et_smc_handle(
	unsigned int feature, unsigned int x2, unsigned int x3, unsigned int x4)
{
	unsigned int ret = 0;

#if defined(_MTK_SECURE_API_H_)

	/* update atf via smc */
	ret = mt_secure_call(MTK_SIP_KERNEL_PTP3_CONTROL,
		feature,
		x2,
		x3,
		x4);

#elif defined(__LINUX_ARM_SMCCC_H)

	/* update TFA via smc call */
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_PTP3_CONTROL,
					feature,
					x2,
					x3,
					x4,
					0, 0, 0, &res);
	if (res.a0 == -1)
		ret = -1;
	else
		ret = (unsigned int)res.a0;

#else
	ret = 0;
#endif

	return ret;
}


/************************************************
 * static function
 ************************************************/

/* ET_EN procfs interface */
static ssize_t et_en_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		et_debug("bad argument!!\n");
		goto out;
	}

	enable = enable ? 1 : 0;
	et_smc_handle(PTP3_FEATURE_ET, ET_W_EN, enable, 0);
out:
	free_page((unsigned long)buf);
	return count;
}

static int et_en_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = et_smc_handle(PTP3_FEATURE_ET, ET_R_EN, 0, 0);
	seq_printf(m, "ET_ENABLE = 0x%x\n", status);

	return 0;
}


/* ET_CFG procfs interface */
static ssize_t et_cfg_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int index = 0, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &index, &value) != 2) {
		et_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	if ((index < 0) || (index >= ET_INDEX_NUM)) {
		et_debug("index out of range!!\n");
		goto out;
	}

	et_smc_handle(PTP3_FEATURE_ET, ET_W_CFG, index, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int et_cfg_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int index = 0;

	for (index = 0; index <= ET_INDEX_NUM; index++) {
		status = et_smc_handle(PTP3_FEATURE_ET, ET_R_CFG, index, 0);
		seq_printf(m, "ET_index_%d = 0x%x\n", index, status);
	}

	return 0;
}

static int et_state_proc_show(struct seq_file *m, void *v)
{
	int status = 0;
	unsigned int index = 0;

	for (index = 0; index < ET_NUM; index++) {
		status = et_smc_handle(PTP3_FEATURE_ET, ET_R_STATE, index, 0);
		seq_printf(m, "ET_state_%d = 0x%x\n", index, status);
	}

	return 0;
}


/************************************************
 * Kernel driver nodes
 ************************************************/
PROC_FOPS_RW(et_en);
PROC_FOPS_RW(et_cfg);
PROC_FOPS_RO(et_state);
static int create_procfs(void)
{
	int i;
	const char *proc_name = "et";
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	struct pentry et_entries[] = {
		PROC_ENTRY(et_en),
		PROC_ENTRY(et_cfg),
		PROC_ENTRY(et_state),
	};

	/* create proc dir */
	dir = proc_mkdir(proc_name, NULL);
	if (!dir) {
		et_err(
			"[%s]: mkdir /proc/%s failed\n", __func__, proc_name);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(et_entries); i++) {
		if (!proc_create(et_entries[i].name,
			0664,
			dir,
			et_entries[i].fops)) {
			et_debug("[%s]: create /proc/%s/iglre/%s failed\n",
				__func__,
				proc_name,
				et_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int et_probe(struct platform_device *pdev)
{
	struct device_node *node = NULL;

	node = pdev->dev.of_node;
	if (!node) {
		et_debug("get et device node err\n");
		return -ENODEV;
	}



	return 0;
}

static int et_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int et_resume(struct platform_device *pdev)
{
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id et_of_match[] = {
	{ .compatible = "mediatek,et", },
	{},
};
#endif

static struct platform_driver et_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= et_probe,
	.suspend	= et_suspend,
	.resume		= et_resume,
	.driver		= {
		.name   = "et",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = et_of_match,
#endif
	},
};

static int __init __et_init(void)
{
	int err = 0;

	create_procfs();

	err = platform_driver_register(&et_driver);
	if (err) {
		et_err("et driver callback register failed..\n");
		return err;
	}

	return 0;
}

static void __exit __et_exit(void)
{
	et_msg("et de-initialization\n");
}


late_initcall(__et_init);
module_exit(__et_exit);

MODULE_DESCRIPTION("MediaTek et Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_ET_C__
