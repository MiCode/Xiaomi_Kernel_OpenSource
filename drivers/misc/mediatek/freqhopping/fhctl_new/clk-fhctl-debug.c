// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "clk-fhctl.h"
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"

enum FH_DEBUG_CMD_ID {
	FH_DBG_CMD_DVFS_API = 0x1002,
	FH_DBG_CMD_SSC_ENABLE = 0x1004,
	FH_DBG_CMD_SSC_DISABLE = 0x1005,
};

static int __fh_ctrl_cmd_hdlr(struct pll_dts *array,
			unsigned int cmd,
			char *pll_name,
			unsigned int arg)
{
	int i;
	struct fh_hdlr *hdlr = NULL;
	int num_pll = array->num_pll;

	/* pll_name to pll_id */
	for (i = 0; i < num_pll; i++, array++) {
		FHDBG("<%s,%s>\n", pll_name, array->pll_name);
		if (strcmp(pll_name,
					array->pll_name) == 0) {
			hdlr = array->hdlr;
			FHDBG("hdlr<%x>\n", hdlr);
			break;
		}
	}

	if (!hdlr) {
		FHDBG("\n");
		return -1;
	}

	FHDBG("perms<%x,%x,%x>\n",
			array->perms, PERM_DBG_HOP, PERM_DBG_SSC);
	switch (cmd) {
	case FH_DBG_CMD_DVFS_API:
		if (array->perms & PERM_DBG_HOP)
			hdlr->ops->hopping(hdlr->data,
					array->domain,
					array->fh_id,
					arg, 0);
		break;
	case FH_DBG_CMD_SSC_ENABLE:
		if (array->perms & PERM_DBG_SSC)
			hdlr->ops->ssc_enable(hdlr->data,
					array->domain,
					array->fh_id,
					arg);
		break;
	case FH_DBG_CMD_SSC_DISABLE:
		if (array->perms & PERM_DBG_SSC)
			hdlr->ops->ssc_disable(hdlr->data,
					array->domain,
					array->fh_id);
		break;
	default:
		FHDBG(" Not Support CMD:%x\n", cmd);
		break;
	}
	return 0;
}
static bool has_perms;
static ssize_t fh_ctrl_proc_write(struct file *file,
				const char *buffer, size_t count, loff_t *data)
{
	int ret, n;
	char kbuf[256];
	char pll_name[32];
	size_t len = 0;
	unsigned int cmd, arg;
	struct pll_dts *array = file->f_inode->i_private;

	FHDBG("array<%x>\n", array);
	len = min(count, (sizeof(kbuf) - 1));

	FHDBG("count: %ld", count);
	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	/* permission control */
	if (!has_perms &&
			strstr(kbuf, array->comp)) {
		has_perms = true;
		FHDBG("has_perms to true\n");
		return count;
	} else if (!has_perms) {
		FHDBG("!has_perms\n");
		return count;
	}

	n = sscanf(kbuf, "%x %31s %x", &cmd, pll_name, &arg);
	if ((n != 3) && (n != 2)) {
		FHDBG("error input format\n");
		return -EINVAL;
	}

	FHDBG("pll:%s cmd:0x%x arg:0x%x", pll_name, cmd, arg);
	__fh_ctrl_cmd_hdlr(array, cmd, pll_name, arg);

	return count;
}
static int fh_ctrl_proc_read(struct seq_file *m, void *v)
{
	int i;
	struct pll_dts *array = m->private;
	int num_pll = array->num_pll;

	seq_printf(m, "====== FHCTL CTRL, has_perms<%d>======\n",
			has_perms);

	for (i = 0; i < num_pll; i++, array++) {
		seq_printf(m, "<%s,%d,%d,%x,%d>,<%s,%s>,<%lx>\n",
				array->pll_name,
				array->pll_id, array->fh_id,
				array->perms, array->ssc_rate,
				array->domain, array->method,
				(unsigned long)array->hdlr);
	}
	return 0;
}
static int fh_ctrl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fh_ctrl_proc_read, inode->i_private);
}

static const struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = fh_ctrl_proc_open,
	.read = seq_read,
	.write = fh_ctrl_proc_write,
	.release = single_release,
};

static int __sample_dds(struct fh_pll_regs *regs,
		struct fh_pll_data *data,
		unsigned int *dds_max,
		unsigned int *dds_min)
{
	int i, ssc_rate = 0;
	unsigned int mon_dds;
	unsigned int dds;

	mon_dds = readl(regs->reg_mon) & data->dds_mask;
	dds = readl(regs->reg_dds) & data->dds_mask;

	*dds_max = dds;
	*dds_min = mon_dds;

	/* Sample 200*10us */
	for (i = 0 ; i < 200 ; i++) {
		mon_dds = readl(regs->reg_mon) & data->dds_mask;

		if (mon_dds > *dds_max)
			*dds_max = mon_dds;

		if (mon_dds < *dds_min)
			*dds_min = mon_dds;

		udelay(10);
	}

	if ((*dds_max == 0) ||
		(*dds_min == 0))
		ssc_rate = 0;
	else {
		int diff = (*dds_max - *dds_min);

		ssc_rate = (diff * 1000) / *dds_max;
	}

	return ssc_rate;
}
static int fh_dumpregs_read(struct seq_file *m, void *v)
{
	int i;
	struct pll_dts *array = m->private;
	int num_pll = array->num_pll;

	FHDBG("array<%x>\n", array);
	seq_puts(m, "FHCTL dumpregs Read\n");

	for (i = 0; i < num_pll ; i++, array++) {
		struct fh_pll_domain *domain;
		struct fh_pll_regs *regs;
		struct fh_pll_data *data;
		int fh_id,  pll_id;
		char *pll_name;
		int ssc_rate;
		unsigned int dds_max, dds_min;

		if (!(array->perms & PERM_DBG_DUMP))
			continue;

		fh_id = array->fh_id;
		pll_id = array->pll_id;
		pll_name = array->pll_name;
		domain = get_fh_domain(array->domain);
		regs = &domain->regs[fh_id];
		data = &domain->data[fh_id];

		ssc_rate = __sample_dds(regs, data,
				&dds_max, &dds_min);

		seq_printf(m, "PLL_ID:%d FH_ID:%d NAME:%s",
			pll_id, fh_id, pll_name);
		seq_printf(m, " PCW:%x SSC_RATE:%d\n",
			readl(regs->reg_con_pcw) & data->dds_mask,
			ssc_rate);

		seq_puts(m, "HP_EN, CLK_CON, SLOPE0, SLOPE1\n");
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(regs->reg_hp_en), readl(regs->reg_clk_con),
			readl(regs->reg_slope0), readl(regs->reg_slope1));

		seq_puts(m, "CFG, UPDNLMT, DDS, DVFS, MON\n");
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			readl(regs->reg_cfg), readl(regs->reg_updnlmt),
			readl(regs->reg_dds), readl(regs->reg_dvfs),
			readl(regs->reg_mon));
		seq_puts(m, "CON_PCW\n");
		seq_printf(m, "0x%08x\n",
			readl(regs->reg_con_pcw));

		seq_printf(m,
			"dds_max:0x%08x dds_mix:0x%08x ssc(1/1000):%d\n\n",
			dds_max, dds_min, ssc_rate);
	}
	return 0;
}
static int fh_dumpregs_open(struct inode *inode, struct file *file)
{
	return single_open(file, fh_dumpregs_read, inode->i_private);
}

static const struct file_operations dumpregs_fops = {
	.owner = THIS_MODULE,
	.open = fh_dumpregs_open,
	.read = seq_read,
	.release = single_release,
};

int fhctl_debugfs_init(struct platform_device *pdev,
		struct pll_dts *array)
{
	struct dentry *root;
	struct dentry *fh_dumpregs_dir;
	struct dentry *fh_ctrl_dir;
	int i;
	int num_pll = array->num_pll;

	FHDBG("array<%x>\n", array);

	root = debugfs_create_dir("fhctl", NULL);
	if (IS_ERR(root)) {
		FHDBG("\n");
		return -1;
	}

	/* /sys/kernel/debug/fhctl/dumpregs */
	fh_dumpregs_dir = debugfs_create_file("dumpregs", 0664,
						root, array, &dumpregs_fops);
	if (IS_ERR(fh_dumpregs_dir)) {
		FHDBG("\n");
		return -1;
	}

	/* /sys/kernel/debug/fhctl/ctrl */
	fh_ctrl_dir = debugfs_create_file("ctrl", 0664,
						root, array, &ctrl_fops);
	if (IS_ERR(fh_ctrl_dir)) {
		FHDBG("\n");
		return -1;
	}

	/* init resources */
	for (i = 0; i < num_pll; i++, array++) {
		init_fh_domain(array->domain,
				array->comp,
				array->fhctl_base,
				array->apmixed_base);
	}

	return 0;
}
