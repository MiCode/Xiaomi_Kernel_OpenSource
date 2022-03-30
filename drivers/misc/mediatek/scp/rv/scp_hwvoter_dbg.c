// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/bits.h>
#include <linux/build_bug.h>
#include <clk-fmeter.h>

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif

#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp.h"
#include "scp_hwvoter_dbg.h"

static unsigned char ipi_ackdata[16];

#if IS_ENABLED(CONFIG_PROC_FS)
/**********************************
 * hw_voter debug
 ***********************************/
static int scp_hw_voter_dbg_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t scp_hw_voter_dbg_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64];
	unsigned int len = 0;
	int ret = 0;
	int n;
	struct hwvoter_ipi_test_t ipi_data;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	pr_notice("%s: %s\n", __func__, desc);

	n = sscanf(desc, "%d %d %d %d %d",
			&ipi_data.type,
			&ipi_data.op,
			&ipi_data.clk_category,
			&ipi_data.clk_id,
			&ipi_data.val);
	if (n != 4 && n != 5) {
		pr_notice("invalid cmd length %d\n", n);
		return count;
	}

	ipi_data.cmd = HW_VOTER_DBG_CMD_TEST;

	ret = mtk_ipi_send_compl(
			&scp_ipidev,
			IPI_OUT_SCP_HWVOTER_DEBUG,
			IPI_SEND_WAIT,
			&ipi_data,
			PIN_OUT_SIZE_SCP_HWVOTER_DEBUG,
			500);
	if (ret != IPI_ACTION_DONE)
		pr_notice("[%s]: SCP send IPI failed - %d\n",
			__func__, ret);

	return count;
}

#define PROC_FOPS_RW(name) \
static int scp_ ## name ## _proc_open(\
				struct inode *inode, \
				struct file *file) \
{ \
	return single_open(file, \
					scp_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	scp_ ## name ## _proc_fops = {\
	.proc_open		= scp_ ## name ## _proc_open, \
	.proc_read		= seq_read, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= scp_ ## name ## _proc_write, \
}

#define PROC_FOPS_RO(name) \
static int scp_ ## name ## _proc_open(\
				struct inode *inode,\
				struct file *file)\
{\
	return single_open(file, \
					scp_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	scp_ ## name ## _proc_fops = {\
	.proc_open		= scp_ ## name ## _proc_open,\
	.proc_read		= seq_read,\
	.proc_lseek		= seq_lseek,\
	.proc_release	= single_release,\
}

#define PROC_FOPS_WO(name) \
static int scp_ ## name ## _proc_open(\
				struct inode *inode, \
				struct file *file) \
{ \
	return single_open(file, \
					scp_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	scp_ ## name ## _proc_fops = {\
	.proc_open		= scp_ ## name ## _proc_open, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= scp_ ## name ## _proc_write, \
}

#define PROC_ENTRY(name) {__stringify(name), &scp_ ## name ## _proc_fops}

PROC_FOPS_RW(hw_voter_dbg);

int scp_hw_voter_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(hw_voter_dbg)
	};

	dir = proc_mkdir("scp_hw_voter", NULL);
	if (!dir) {
		pr_notice("fail to create /proc/scp_hw_voter\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
						0664,
						dir,
						entries[i].fops)) {
			pr_notice("%s: create /proc/scp_hw_voter/%s failed\n",
						__func__, entries[i].name);
			ret = -ENOMEM;
		}
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

int scp_hw_voter_dbg_init(void)
{
#if IS_ENABLED(CONFIG_PROC_FS)
	int ret;

	pr_notice("call %s\n", __func__);

	ret = mtk_ipi_register(
			&scp_ipidev,
			IPI_OUT_SCP_HWVOTER_DEBUG,
			NULL, NULL,
			(void *)ipi_ackdata);
	if (ret) {
		pr_notice("scp hwvoter mtk_ipi_register fail, %d\n",
				ret);
		WARN_ON(1);
	}

	if (scp_hw_voter_create_procfs()) {
		pr_notice("scp_hw_voter_create_procfs fail, %d\n",
				ret);
		WARN_ON(1);
	}
#else
	pr_notice("no %s due to CONFIG_PROC_FS not defined\n",
			__func__);
#endif

	return 0;
}
