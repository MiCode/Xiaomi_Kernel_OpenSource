/*
 * Copyright (c) 2015, 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clock-generic.h>
#include <linux/debugfs.h>

#define CMD_RCGR_REG		0x0
#define CMD_UPDATE_EN		BIT(0)
/* Async_clk_en */
#define CMD_ROOT_EN		BIT(1)

struct rcgwr {
	void __iomem *base;
	void __iomem *rcg_base;
	int *dfs_sid_offset;
	int *dfs_sid_value;
	int  dfs_sid_len;
	int *link_sid_offset;
	int *link_sid_value;
	int  link_sid_len;
	int *lmh_sid_offset;
	int *lmh_sid_value;
	int  lmh_sid_len;
	bool inited;
};

static struct rcgwr **rcgwr;
static struct platform_device *cpu_clock_dev;
static u32 num_clusters;

#define DFS_SID_1_2		0x10
#define DFS_SID_3_4		0x14
#define DFS_SID_5_6		0x18
#define DFS_SID_7_8		0x1C
#define DFS_SID_9_10		0x20
#define DFS_SID_11_12		0x24
#define DFS_SID_13_14		0x28
#define DFS_SID_15		0x2C
#define LMH_SID_1_2		0x30
#define LMH_SID_3_4		0x34
#define LMH_SID_5		0x38
#define DCVS_CFG_CTL		0x50
#define LMH_CFG_CTL		0x54
#define RC_CFG_CTL		0x58
#define RC_CFG_DBG		0x5C
#define RC_CFG_UPDATE		0x60

#define RC_CFG_UPDATE_EN_BIT	8
#define RC_CFG_ACK_BIT		16

#define UPDATE_CHECK_MAX_LOOPS  500

#define DFS_SID_START		0xE
#define LMH_SID_START		0x6
#define DCVS_CONFIG		0x2
#define LINK_SID		0x3

/* Sequence for enable */
static int ramp_en[] = { 0x800, 0xC00, 0x400};

static int check_rcg_config(void __iomem *base)
{
	u32 cmd_rcgr_regval, count;

	cmd_rcgr_regval = readl_relaxed(base + CMD_RCGR_REG);
	cmd_rcgr_regval |= CMD_ROOT_EN;
	writel_relaxed(cmd_rcgr_regval, (base + CMD_RCGR_REG));

	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		cmd_rcgr_regval = readl_relaxed(base + CMD_RCGR_REG);
		cmd_rcgr_regval &= CMD_UPDATE_EN;
		if (!(cmd_rcgr_regval)) {
			pr_debug("cmd_rcgr state on update bit cleared 0x%x, cmd 0x%x\n",
					readl_relaxed(base + CMD_RCGR_REG),
					cmd_rcgr_regval);
			return 0;
		}
		udelay(1);
	}

	WARN_ON(count == 0);

	return -EINVAL;
}

static int rc_config_update(void __iomem *base, u32 rc_value, u32 rc_ack_bit)
{
	u32 count, ret = 0, regval;

	regval = readl_relaxed(base + RC_CFG_UPDATE);
	regval |= rc_value;
	writel_relaxed(regval, base + RC_CFG_UPDATE);
	regval |= BIT(RC_CFG_UPDATE_EN_BIT);
	writel_relaxed(regval, base + RC_CFG_UPDATE);

	/* Poll for update ack */
	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		regval = readl_relaxed((base + RC_CFG_UPDATE))
						  >> RC_CFG_ACK_BIT;
		if (regval == BIT(rc_ack_bit)) {
			ret = 0;
			break;
		}
		udelay(1);
	}
	WARN_ON(count == 0);

	/* Clear RC_CFG_UPDATE_EN */
	writel_relaxed(0 << RC_CFG_UPDATE_EN_BIT, (base + RC_CFG_UPDATE));
	/* Poll for update ack */
	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		regval = readl_relaxed((base + RC_CFG_UPDATE))
						>> RC_CFG_ACK_BIT;
		if (!regval)
			return ret;
		udelay(1);
	}
	WARN_ON(count == 0);

	return -EINVAL;
}


static int ramp_control_enable(struct platform_device *pdev,
		struct rcgwr *rcgwr)
{
	int i = 0, ret = 0;

	for (i = 0; i < ARRAY_SIZE(ramp_en); i++) {
		ret = check_rcg_config(rcgwr->rcg_base);
		if (ret) {
			dev_err(&pdev->dev, "Failed to update config!!!\n");
			return ret;
		}
		writel_relaxed(ramp_en[i], rcgwr->base + DCVS_CFG_CTL);
		ret = rc_config_update(rcgwr->base, DCVS_CONFIG, DCVS_CONFIG);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to config update for 0x2 and ACK 0x4\n");
			break;
		}
	}

	return ret;
}

static int ramp_down_disable(struct platform_device *pdev,
		struct rcgwr *rcgwr)
{
	int ret = 0;

	ret = check_rcg_config(rcgwr->rcg_base);
	if (ret) {
		dev_err(&pdev->dev, "Failed to update config!!!\n");
		return ret;
	}

	writel_relaxed(0x200, rcgwr->base + DCVS_CFG_CTL);
	ret = rc_config_update(rcgwr->base, DCVS_CONFIG, DCVS_CONFIG);
	if (ret)
		dev_err(&pdev->dev,
			"Failed to config update for 0x2 and ACK 0x4\n");

	return ret;
}

static int ramp_control_disable(struct platform_device *pdev,
		struct rcgwr *rcgwr)
{
	int ret = 0;

	if (!rcgwr->inited)
		return 0;

	ret = check_rcg_config(rcgwr->rcg_base);
	if  (ret) {
		dev_err(&pdev->dev, "Failed to update config!!!\n");
		return ret;
	}

	writel_relaxed(0x0, rcgwr->base + DCVS_CFG_CTL);

	ret = rc_config_update(rcgwr->base, DCVS_CONFIG, DCVS_CONFIG);
	if (ret)
		dev_err(&pdev->dev,
			"Failed to config update for 0x2 and ACK 0x4\n");

	rcgwr->inited = false;

	return ret;
}

static int ramp_link_sid(struct platform_device *pdev, struct rcgwr *rcgwr)
{
	int ret = 0, i;

	if (!rcgwr->link_sid_len) {
		pr_err("Use Default Link SID\n");
		return 0;
	}

	ret = check_rcg_config(rcgwr->rcg_base);
	if  (ret) {
		dev_err(&pdev->dev, "Failed to update config!!!\n");
		return ret;
	}

	for (i = 0; i < rcgwr->link_sid_len; i++)
		writel_relaxed(rcgwr->link_sid_value[i],
				rcgwr->base + rcgwr->link_sid_offset[i]);

	ret = rc_config_update(rcgwr->base, LINK_SID, LINK_SID);
	if (ret)
		dev_err(&pdev->dev,
			"Failed to config update for 0x3 and ACK 0x8\n");

	return ret;
}

static int ramp_lmh_sid(struct platform_device *pdev, struct rcgwr *rcgwr)
{
	int ret = 0, i, j;

	if (!rcgwr->lmh_sid_len) {
		pr_err("Use Default LMH SID\n");
		return 0;
	}

	ret = check_rcg_config(rcgwr->rcg_base);
	if  (ret) {
		dev_err(&pdev->dev, "Failed to update config!!!\n");
		return ret;
	}

	for (i = 0; i < rcgwr->lmh_sid_len; i++)
		writel_relaxed(rcgwr->lmh_sid_value[i],
				rcgwr->base + rcgwr->lmh_sid_offset[i]);

	for (i = LMH_SID_START, j = 0; j < rcgwr->lmh_sid_len; i--, j++) {
		ret = rc_config_update(rcgwr->base, i, i);
		if (ret) {
			dev_err(&pdev->dev,
			"Failed to update config for DFSSID-0x%x and ack 0x%lx\n",
					i, BIT(i));
			break;
		}
	}

	return ret;
}

static int ramp_dfs_sid(struct platform_device *pdev, struct rcgwr *rcgwr)
{
	int ret = 0, i, j;

	if (!rcgwr->dfs_sid_len) {
		pr_err("Use Default DFS SID\n");
		return 0;
	}

	ret = check_rcg_config(rcgwr->rcg_base);
	if  (ret) {
		dev_err(&pdev->dev, "Failed to update config!!!\n");
		return ret;
	}

	for (i = 0; i < rcgwr->dfs_sid_len; i++)
		writel_relaxed(rcgwr->dfs_sid_value[i],
				rcgwr->base + rcgwr->dfs_sid_offset[i]);

	for (i = DFS_SID_START, j = 0; j < rcgwr->dfs_sid_len; i--, j++) {
		ret = rc_config_update(rcgwr->base, i, i);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to update config for DFSSID-0x%x and ack 0x%lx\n",
					i, BIT(i));
			break;
		}
	}

	return ret;
}

static int parse_dt_rcgwr(struct platform_device *pdev, char *prop_name,
				int **off, int **val, int *len)
{
	struct device_node *node = pdev->dev.of_node;
	int prop_len, i;
	u32 *array;

	if (!of_find_property(node, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % 2) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= 2;

	*off = devm_kzalloc(&pdev->dev, prop_len * sizeof(u32), GFP_KERNEL);
	if (!*off)
		return -ENOMEM;

	*val = devm_kzalloc(&pdev->dev, prop_len * sizeof(u32), GFP_KERNEL);
	if (!*val)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * 2, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(node, prop_name, array, prop_len * 2);
	for (i = 0; i < prop_len; i++) {
		*(*off + i) = array[i * 2];
		*(*val + i) = array[2 * i + 1];
	}

	*len = prop_len;

	return 0;
}

static int rcgwr_init_bases(struct platform_device *pdev, struct rcgwr *rcgwr,
		const char *name)
{
	struct resource *res;
	char rcg_name[] = "rcgwr-xxx-base";
	char rcg_mux[] = "xxx-mux";

	snprintf(rcg_name, ARRAY_SIZE(rcg_name), "rcgwr-%s-base", name);
	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, rcg_name);
	if (!res) {
		dev_err(&pdev->dev, "missing %s\n", rcg_name);
		return -EINVAL;
	}

	rcgwr->base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!rcgwr->base) {
		dev_err(&pdev->dev, "ioremap failed for %s\n",
					rcg_name);
		return -ENOMEM;
	}

	snprintf(rcg_mux, ARRAY_SIZE(rcg_mux), "%s-mux", name);
	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, rcg_mux);
	if (!res) {
		dev_err(&pdev->dev, "missing %s\n", rcg_mux);
		return -EINVAL;
	}

	rcgwr->rcg_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!rcgwr->rcg_base) {
		dev_err(&pdev->dev, "ioremap failed for %s\n",
					rcg_name);
		return -ENOMEM;
	}

	return 0;
}

/*
 * Disable the RCG ramp controller.
 */
int clock_rcgwr_disable(struct platform_device *pdev)
{
	int i, ret = 0;

	for (i = 0; i < num_clusters; i++) {
		if (!rcgwr[i])
			return -ENOMEM;
		ret = ramp_control_disable(pdev, rcgwr[i]);
		if (ret)
			dev_err(&pdev->dev,
			"Ramp controller disable failed for Cluster-%d\n", i);
	}

	return ret;
}

static int clock_rcgwr_disable_set(void *data, u64 val)
{
	if (val) {
		pr_err("Enabling not supported!!\n");
		return -EINVAL;
	} else
		return clock_rcgwr_disable(cpu_clock_dev);
}

DEFINE_SIMPLE_ATTRIBUTE(rcgwr_enable_fops, NULL,
			clock_rcgwr_disable_set, "%lld\n");

static int clock_debug_enable_show(struct seq_file *m, void *v)
{
	int i = 0;

	seq_puts(m, "Cluster\t\tEnable\n");

	for (i = 0; i < num_clusters; i++)
		seq_printf(m, "%d\t\t%d\n", i, rcgwr[i]->inited);

	return 0;
}

static int clock_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, clock_debug_enable_show, inode->i_private);
}

static const struct file_operations rcgwr_enable_show = {
	.owner		= THIS_MODULE,
	.open		= clock_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * Program the DFS Sequence ID.
 * Program the Link Sequence ID.
 * Enable RCG with ramp controller.
 */
int clock_rcgwr_init(struct platform_device *pdev)
{
	int ret = 0, i;
	char link_sid[] = "qcom,link-sid-xxx";
	char dfs_sid[]  = "qcom,dfs-sid-xxx";
	char lmh_sid[]  = "qcom,lmh-sid-xxx";
	char ramp_dis[] = "qcom,ramp-dis-xxx";
	char names[] = "cxxx";
	struct dentry *debugfs_base;

	ret = of_property_read_u32(pdev->dev.of_node, "qcom,num-clusters",
						&num_clusters);
	if (ret)
		panic("Cannot read num-clusters from dt (ret:%d)\n", ret);

	rcgwr = devm_kzalloc(&pdev->dev, sizeof(struct rcgwr) * num_clusters,
				GFP_KERNEL);
	if (!rcgwr)
		return -ENOMEM;

	for (i = 0; i < num_clusters; i++) {
		rcgwr[i] = devm_kzalloc(&pdev->dev, sizeof(struct rcgwr),
				GFP_KERNEL);
		if (!rcgwr[i])
			goto fail_mem;

		snprintf(names, ARRAY_SIZE(names), "c%d", i);

		ret = rcgwr_init_bases(pdev, rcgwr[i], names);
		if (ret) {
			dev_err(&pdev->dev, "Failed to init_bases for RCGwR\n");
			goto fail_mem;
		}

		snprintf(dfs_sid, ARRAY_SIZE(dfs_sid),
					"qcom,dfs-sid-%s", names);
		ret = parse_dt_rcgwr(pdev, dfs_sid, &(rcgwr[i]->dfs_sid_offset),
			&(rcgwr[i]->dfs_sid_value), &(rcgwr[i]->dfs_sid_len));
		if (ret)
			dev_err(&pdev->dev,
				"No DFS SID tables found for Cluster-%d\n", i);

		snprintf(link_sid, ARRAY_SIZE(link_sid),
					"qcom,link-sid-%s", names);
		ret = parse_dt_rcgwr(pdev, link_sid,
			&(rcgwr[i]->link_sid_offset),
			&(rcgwr[i]->link_sid_value), &(rcgwr[i]->link_sid_len));
		if (ret)
			dev_err(&pdev->dev,
				"No Link SID tables found for Cluster-%d\n", i);

		snprintf(lmh_sid, ARRAY_SIZE(lmh_sid),
					"qcom,lmh-sid-%s", names);
		ret = parse_dt_rcgwr(pdev, lmh_sid,
			&(rcgwr[i]->lmh_sid_offset),
			&(rcgwr[i]->lmh_sid_value), &(rcgwr[i]->lmh_sid_len));
		if (ret)
			dev_err(&pdev->dev,
				"No LMH SID tables found for Cluster-%d\n", i);

		ret = ramp_lmh_sid(pdev, rcgwr[i]);
		if (ret)
			goto fail_mem;

		ret = ramp_dfs_sid(pdev, rcgwr[i]);
		if (ret)
			goto fail_mem;

		ret = ramp_link_sid(pdev, rcgwr[i]);
		if (ret)
			goto fail_mem;

		ret = ramp_control_enable(pdev, rcgwr[i]);
		if (ret)
			goto fail_mem;

		snprintf(ramp_dis, ARRAY_SIZE(ramp_dis),
					"qcom,ramp-dis-%s", names);
		if (of_property_read_bool(pdev->dev.of_node, ramp_dis)) {
			ret = ramp_down_disable(pdev, rcgwr[i]);
			if (ret)
				goto fail_mem;
		}

		rcgwr[i]->inited = true;
	}

	cpu_clock_dev = pdev;

	debugfs_base = debugfs_create_dir("rcgwr", NULL);
	if (debugfs_base) {
		if (!debugfs_create_file("enable", 0444, debugfs_base, NULL,
				&rcgwr_enable_fops)) {
			pr_err("Unable to create `enable` debugfs entry\n");
			debugfs_remove(debugfs_base);
		}

		if (!debugfs_create_file("status", 0444, debugfs_base, NULL,
					&rcgwr_enable_show)) {
			pr_err("Unable to create `status` debugfs entry\n");
			debugfs_remove_recursive(debugfs_base);
		}
	} else
		pr_err("Unable to create debugfs dir\n");

	pr_info("RCGwR  Init Completed\n");

	return ret;

fail_mem:
	--i;
	for (; i >= 0 ; i--) {
		devm_kfree(&pdev->dev, rcgwr[i]);
		rcgwr[i] = NULL;
	}
	devm_kfree(&pdev->dev, rcgwr);
	panic("RCGwR failed to Initialize\n");
}
