// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <mboot_params.h>

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

#include "gpueb_ipi.h"
#include "gpueb_helper.h"
#include "gpueb_hwvoter_dbg.h"

struct hw_voter_ipi_test_t {
	unsigned short cmd;
	unsigned char type;
	unsigned char op;
	unsigned short clk_id;
	unsigned short val;
};

#if IPI_SUPPORT
static unsigned char ipi_ackdata[16];
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
/**********************************
 * hw_voter debug
 ***********************************/
static int gpueb_hw_voter_dbg_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static ssize_t gpueb_hw_voter_dbg_proc_write(
					struct file *file,
					const char __user *buffer,
					size_t count,
					loff_t *data)
{
	char desc[64], type[32];
	int len = 0;
#if IPI_SUPPORT
	int ret = 0;
#endif
	int n;
	unsigned int op, clk_id, val;
	struct hw_voter_ipi_test_t ipi_data;
	int channel_id = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	pr_notice("%s: %s\n", __func__, desc);

	n = sscanf(desc, "%31s %d %d %d", type, &op, &clk_id, &val);
	if (n != 3 && n != 4) {
		pr_notice("invalid cmd length %d\n", n);
		return count;
	}

	ipi_data.cmd = HW_VOTER_DBG_CMD_TEST;
	ipi_data.op = op;
	ipi_data.clk_id = clk_id;
	ipi_data.val = val;

	if (!strcmp(type, "pd")) {
		ipi_data.type = HW_VOTER_TEST_TYPE_PD;
	} else if (!strcmp(type, "pll")) {
		ipi_data.type = HW_VOTER_TEST_TYPE_PLL;
	} else if (!strcmp(type, "mux")) {
		ipi_data.type = HW_VOTER_TEST_TYPE_MUX;
	} else if (!strcmp(type, "cg")) {
		ipi_data.type = HW_VOTER_TEST_TYPE_CG;
	} else {
		pr_notice("invalid type %s\n", type);
		return count;
	}

	channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_CCF");
#if IPI_SUPPORT
	ret = mtk_ipi_send_compl(
			&gpueb_ipidev,
			channel_id,
			IPI_SEND_WAIT,
			&ipi_data,
			4, /* 4 slots message = 4 * 4 = 16 bytes */
			500);
	if (ret != IPI_ACTION_DONE)
		pr_info("[%s]: SCP send IPI failed - %d\n",
			__func__, ret);
#endif

	return count;
}

#define PROC_FOPS_RW(name) \
static int gpueb_ ## name ## _proc_open(\
				struct inode *inode, \
				struct file *file) \
{ \
	return single_open(file, \
					gpueb_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	gpueb_ ## name ## _proc_fops = {\
	.proc_open		= gpueb_ ## name ## _proc_open, \
	.proc_read		= seq_read, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= gpueb_ ## name ## _proc_write, \
}

#define PROC_FOPS_RO(name) \
static int gpueb_ ## name ## _proc_open(\
				struct inode *inode,\
				struct file *file)\
{\
	return single_open(file, \
					gpueb_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	gpueb_ ## name ## _proc_fops = {\
	.proc_open		= gpueb_ ## name ## _proc_open,\
	.proc_read		= seq_read,\
	.proc_lseek		= seq_lseek,\
	.proc_release	= single_release,\
}

#define PROC_FOPS_WO(name) \
static int gpueb_ ## name ## _proc_open(\
				struct inode *inode, \
				struct file *file) \
{ \
	return single_open(file, \
					gpueb_ ## name ## _proc_show, \
					PDE_DATA(inode)); \
} \
static const struct proc_ops \
	gpueb_ ## name ## _proc_fops = {\
	.proc_open		= gpueb_ ## name ## _proc_open, \
	.proc_lseek		= seq_lseek, \
	.proc_release	= single_release, \
	.proc_write		= gpueb_ ## name ## _proc_write, \
}

#define PROC_ENTRY(name) {__stringify(name), &gpueb_ ## name ## _proc_fops}

PROC_FOPS_RW(hw_voter_dbg);

int gpueb_hw_voter_create_procfs(void)
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

	dir = proc_mkdir("gpueb_hw_voter", NULL);
	if (!dir) {
		pr_notice("fail to create /proc/gpueb_hw_voter\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
						0664,
						dir,
						entries[i].fops)) {
			pr_notice("%s: create /proc/gpueb_hw_voter/%s failed\n",
						__func__, entries[i].name);
			ret = -ENOMEM;
		}
	}

	return ret;
}
#endif /* CONFIG_PROC_FS */

int gpueb_hw_voter_dbg_init(void)
{
#if IS_ENABLED(CONFIG_PROC_FS)
	int ret = 0;
#if IPI_SUPPORT
	int channel_id = 0;

	channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_CCF");
	ret = mtk_ipi_register(
			&gpueb_ipidev,
			channel_id,
			NULL, NULL,
			(void *)ipi_ackdata);
	if (ret) {
		pr_notice("gpueb hwvoter mtk_ipi_register fail, %d\n",
				ret);
		WARN_ON(1);
	}
#endif

	if (gpueb_hw_voter_create_procfs()) {
		pr_notice("gpueb_hw_voter_create_procfs fail, %d\n",
				ret);
		WARN_ON(1);
	}
#endif

	return 0;
}
