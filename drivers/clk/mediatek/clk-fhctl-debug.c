// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "clk-fhctl.h"
#include "clk-mtk.h"


enum FH_DEBUG_CMD_ID {
	FH_DBG_CMD_ID = 0x1000,
	FH_DBG_CMD_DVFS = 0x1001,
	FH_DBG_CMD_DVFS_API = 0x1002,
	FH_DBG_CMD_DVFS_SSC_ENABLE = 0x1003,
	FH_DBG_CMD_SSC_ENABLE = 0x1004,
	FH_DBG_CMD_SSC_DISABLE = 0x1005,

	FH_DBG_CMD_MAX
};

void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl)
{
	debugfs_remove_recursive(fhctl->debugfs_root);
}


static int __fh_ctrl_cmd_handler(struct clk_mt_fhctl *fh,
			unsigned int cmd,
			int pll_id,
			unsigned int p1)

{
	int ret;

	pr_info("pll_id:0x%x cmd: %x p1:%x", pll_id, cmd, p1);

	if (fh == NULL) {
		pr_info("Error: fh is null!");
		return 0;
	}

	switch (cmd) {
	case FH_DBG_CMD_SSC_ENABLE:
		ret = fh->hal_ops->pll_ssc_enable(fh, p1);
		break;
	case FH_DBG_CMD_SSC_DISABLE:
		ret = fh->hal_ops->pll_ssc_disable(fh);
		break;
	case FH_DBG_CMD_DVFS:
		ret = fh->hal_ops->pll_hopping(fh, p1, -1);
		break;
	case FH_DBG_CMD_DVFS_API:
		ret = !(mtk_fh_set_rate(pll_id, p1, -1));
		break;
	default:
		pr_info(" Not Support CMD:%x\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		pr_info(" Debug CMD fail err:%d\n", ret);

	return ret;
}


/***************************************************************************
 * FHCTL Debug CTRL OPS
 ***************************************************************************/
static ssize_t fh_ctrl_proc_write(struct file *file,
				const char *buffer, size_t count, loff_t *data)
{
	int ret, n;
	char kbuf[256];
	int pll_id;
	size_t len = 0;
	unsigned int cmd, p1;
	struct clk_mt_fhctl *fh;
	struct mtk_fhctl *fhctl = file->f_inode->i_private;

	len = min(count, (sizeof(kbuf) - 1));

	pr_info("count: %ld", count);
	if (count == 0)
		return -1;

	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	n = sscanf(kbuf, "%x %x %x", &cmd, &pll_id, &p1);
	pr_info("pll:0x%x cmd:%x p1:%x", pll_id, cmd, p1);

	if ((cmd < FH_DBG_CMD_ID) && (cmd > FH_DBG_CMD_MAX)) {
		pr_info("cmd not support:%x", cmd);
		return -EINVAL;
	}

	if (pll_id >= fhctl->pll_num) {
		pr_info("pll_id is illegal:%d", pll_id);
		return -EINVAL;
	}

	fh = mtk_fh_get_fh_obj_tbl(fhctl, pll_id);

	__fh_ctrl_cmd_handler(fh, cmd, pll_id, p1);

	pr_debug("reg_cfg:0x%08x", readl(fh->fh_regs->reg_cfg));
	pr_debug("reg_updnlmt:0x%08x", readl(fh->fh_regs->reg_updnlmt));
	pr_debug("reg_dds:0x%08x", readl(fh->fh_regs->reg_dds));
	pr_debug("reg_dvfs:0x%08x", readl(fh->fh_regs->reg_dvfs));
	pr_debug("reg_mon:0x%08x", readl(fh->fh_regs->reg_mon));
	pr_debug("reg_con0:0x%08x", readl(fh->fh_regs->reg_con0));
	pr_debug("reg_con1:0x%08x", readl(fh->fh_regs->reg_con1));

	return count;
}

static int fh_ctrl_proc_read(struct seq_file *m, void *v)
{
	int i;
	struct mtk_fhctl *fhctl = m->private;

	seq_puts(m, "====== FHCTL CTRL Description ======\n");

	seq_puts(m, "[PLL Name and ID Table]\n");
	for (i = 0 ; i < fhctl->pll_num ; i++)
		seq_printf(m, "PLL_ID:%d PLL_NAME: %s\n",
				i, fhctl->fh_tbl[i]->pll_data->pll_name);

	seq_puts(m, "\n[Command Description]\n");
	seq_puts(m, "	 [SSC Enable]\n");
	seq_puts(m, "	 /> echo '1004 <PLL-ID> <SSC-Rate>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1004 2 2' > ctrl\n");
	seq_puts(m, "	 [SSC Disable]\n");
	seq_puts(m, "	 /> echo '1005 <PLL-ID>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1005 2' > ctrl\n");
	seq_puts(m, "	 [SSC Hopping]\n");
	seq_puts(m, "	 /> echo '1001 <PLL-ID> <DDS>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1001 2 ec200' > ctrl\n");
	seq_puts(m, "	 [CLK API Hopping]\n");
	seq_puts(m, "	 /> echo '1002 <PLL-ID> <DDS>' > ctrl\n");
	seq_puts(m, "	 Example: echo '1002 2 ec200' > ctrl\n");

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

static int __sample_period_dds(struct clk_mt_fhctl *fh)
{
	int i, ssc_rate = 0;
	struct clk_mt_fhctl_regs *fh_regs;
	unsigned int mon_dds;
	unsigned int dds;

	fh_regs = fh->fh_regs;

	mon_dds = readl(fh_regs->reg_mon) & fh->pll_data->dds_mask;
	dds = readl(fh_regs->reg_dds) & fh->pll_data->dds_mask;

	fh->pll_data->dds_max = dds;
	fh->pll_data->dds_min = mon_dds;


	/* Sample 200*10us */
	for (i = 0 ; i < 200 ; i++) {
		mon_dds = readl(fh_regs->reg_mon) & fh->pll_data->dds_mask;

		if (mon_dds > fh->pll_data->dds_max)
			fh->pll_data->dds_max = mon_dds;

		if (mon_dds < fh->pll_data->dds_min)
			fh->pll_data->dds_min = mon_dds;

		udelay(10);
	}

	if ((fh->pll_data->dds_max == 0) ||
		(fh->pll_data->dds_min == 0))
		ssc_rate = 0;
	else {
		int diff = (fh->pll_data->dds_max - fh->pll_data->dds_min);

		ssc_rate = (diff * 1000) / fh->pll_data->dds_max;
	}

	return ssc_rate;
}

static int mt_fh_dumpregs_read(struct seq_file *m, void *data)
{
	struct mtk_fhctl *fhctl = dev_get_drvdata(m->private);
	int i, ssc_rate;
	struct clk_mt_fhctl *fh;
	struct clk_mt_fhctl_regs *fh_regs;

	if (fhctl == NULL) {
		seq_puts(m, "Cannot Get FHCTL driver data\n");
		return 0;
	}

	seq_puts(m, "FHCTL dumpregs Read\n");

	for (i = 0; i < fhctl->pll_num ; i++) {
		fh = mtk_fh_get_fh_obj_tbl(fhctl, i);
		if (fh == NULL) {
			pr_info(" fh:NULL pll_id:%d", i);
			seq_printf(m, "ERROR PLL_ID:%d clk_mt_fhctl is NULL\r\n",
						i);
			return 0;
		}

		if (fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
			pr_debug(" Not support: %s", fh->pll_data->pll_name);
			continue;
		}

		fh_regs = fh->fh_regs;
		if (fh_regs == NULL) {
			pr_info("%s Not support dumpregs!",
				fh->pll_data->pll_name);
			seq_printf(m, "PLL_%d: %s Not support dumpregs!\n",
						i, fh->pll_data->pll_name);
			continue;
		}

		pr_debug("fh:0x%p fh_regs:0x%p", fh, fh_regs);

		if (i == 0) {
			seq_printf(m, "\r\nFHCTL_HP_EN:\r\n0x%08x\r\n",
				readl(fh_regs->reg_hp_en));
			seq_printf(m, "\r\nFHCTL_CLK_CON:\r\n0x%08x\r\n",
				readl(fh_regs->reg_clk_con));
			seq_printf(m, "\r\nFHCTL_SLOPE0:\r\n0x%08x\r\n",
				readl(fh_regs->reg_slope0));
			seq_printf(m, "\r\nFHCTL_SLOPE1:\r\n0x%08x\r\n\n",
				readl(fh_regs->reg_slope1));
		}

		ssc_rate = __sample_period_dds(fh);

		seq_printf(m, "PLL_ID:%d (%s) type:%d \r\n",
			i, fh->pll_data->pll_name, fh->pll_data->pll_type);

		seq_puts(m, "CFG, UPDNLMT, DDS, DVFS, MON\r\n");
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			readl(fh_regs->reg_cfg), readl(fh_regs->reg_updnlmt),
			readl(fh_regs->reg_dds), readl(fh_regs->reg_dvfs),
			readl(fh_regs->reg_mon));
		seq_puts(m, "CON0, CON1\r\n");
		seq_printf(m, "0x%08x 0x%08x\r\n",
			readl(fh_regs->reg_con0), readl(fh_regs->reg_con1));

		seq_printf(m,
			"DDS max:0x%08x min:0x%08x ssc(1/1000):%d\r\n\r\n",
			fh->pll_data->dds_max,
			fh->pll_data->dds_min,
			ssc_rate);


		pr_debug("pll_id:%d", i);
		pr_debug("pll_type:%d", fh->pll_data->pll_type);
		pr_debug("reg_hp_en:0x%08x", readl(fh_regs->reg_hp_en));
		pr_debug("reg_clk_con:0x%08x", readl(fh_regs->reg_clk_con));
		pr_debug("reg_rst_con:0x%08x", readl(fh_regs->reg_rst_con));
		pr_debug("reg_slope0:0x%08x", readl(fh_regs->reg_slope0));
		pr_debug("reg_slope1:0x%08x", readl(fh_regs->reg_slope1));

		pr_debug("reg_cfg:0x%08x", readl(fh_regs->reg_cfg));
		pr_debug("reg_updnlmt:0x%08x", readl(fh_regs->reg_updnlmt));
		pr_debug("reg_dds:0x%08x", readl(fh_regs->reg_dds));
		pr_debug("reg_dvfs:0x%08x", readl(fh_regs->reg_dvfs));
		pr_debug("reg_mon:0x%08x", readl(fh_regs->reg_mon));
		pr_debug("reg_con0:0x%08x", readl(fh_regs->reg_con0));
		pr_debug("reg_con1:0x%08x", readl(fh_regs->reg_con1));

	}
	return 0;
}

void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl)
{
	struct dentry *root;
	struct dentry *fh_dumpregs_dir;
	struct dentry *fh_ctrl_dir;
	struct device *dev = fhctl->dev;

	root = debugfs_create_dir("fhctl", NULL);
	if (IS_ERR(root)) {
		dev_info(dev, "create debugfs fail");
		return;
	}

	fhctl->debugfs_root = root;

	/* /sys/kernel/debug/fhctl/dumpregs */
	fh_dumpregs_dir = debugfs_create_devm_seqfile(dev,
				"dumpregs", root, mt_fh_dumpregs_read);
	if (IS_ERR(fh_dumpregs_dir)) {
		dev_info(dev, "failed to create dumpregs debugfs");
		return;
	}

	/* /sys/kernel/debug/fhctl/ctrl */
	fh_ctrl_dir = debugfs_create_file("ctrl", 0664,
						root, fhctl, &ctrl_fops);
	if (IS_ERR(fh_ctrl_dir)) {
		dev_info(dev, "failed to create ctrl debugfs");
		return;
	}

	dev_dbg(dev, "Create debugfs success!");
}


