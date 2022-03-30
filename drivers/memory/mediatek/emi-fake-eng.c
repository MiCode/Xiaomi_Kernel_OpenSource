// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Copyright (c) 2021 Eric Lin <tesheng.lin@mediatek.com>
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>


#define FAKE_ENG_EN				0x0
#define FAKE_ENG_RST			0x4
#define FAKE_ENG_DONE			0x8
#define FAKE_ENG_CON0			0xc
#define FAKE_ENG_CON1			0x10
#define FAKE_ENG_CON2			0x14
#define FAKE_ENG_CON3			0x18
#define FAKE_ENG_START_ADDR		0x1c
#define FAKE_ENG_START_ADDR_2ND		0x20
#define FAKE_ENG_ADDR		0x24
#define FAKE_ENG_INIT_PAT0	0x28
#define FAKE_ENG_INIT_PAT1	0x2c
#define FAKE_ENG_INIT_PAT2	0x30
#define FAKE_ENG_INIT_PAT3	0x34
#define FAKE_ENG_INIT_PAT4	0x38
#define FAKE_ENG_INIT_PAT5	0x3c
#define FAKE_ENG_INIT_PAT6	0x40
#define FAKE_ENG_INIT_PAT7	0x44
#define FAKE_ENG_CMP_RESULT	0x48
#define FAKE_ENG_FREEZE_RESULT		0xb0
#define FAKE_ENG_IDLE		0xb4
#define FAKE_ENG_HASH		0xb8
#define FAKE_ENG_START_ADDR_RD		0xbc
#define FAKE_ENG_START_ADDR_RD_2ND	0xc0
#define FAKE_ENG_ADDR_RD			0xc4

#define SIZE 256

struct fake_eng_set {
	unsigned int chn_number;
	unsigned int rd_dis;
	unsigned int wr_dis;
	unsigned int loop_en;
	unsigned int cross_rk_en;
	unsigned int data_cmp_en;
	unsigned int pat_mode;
	unsigned int rd_wr_joint_en;
	unsigned int rd_amount;
	unsigned int wr_amount;
	unsigned int slow_down;
	unsigned int slow_down_grp;
	unsigned int burst_len;
	unsigned int burst_size;
	unsigned int aw_slc;
	unsigned int ar_slc;
	unsigned int grp_aomunt;
	unsigned int start_addr_wr;
	unsigned int start_addr_rd;
	unsigned int start_addr_wr_2nd;
	unsigned int start_addr_rd_2nd;
	unsigned int start_addr_wr_extend;
	unsigned int start_addr_rd_extend;
	unsigned int start_addr_wr_2nd_extend;
	unsigned int start_addr_rd_2nd_extend;
	unsigned int addr_offset1;
	unsigned int addr_offset2;
	unsigned int init_pat0;
	unsigned int init_pat1;
	unsigned int init_pat2;
	unsigned int init_pat3;
	unsigned int init_pat4;
	unsigned int init_pat5;
	unsigned int init_pat6;
	unsigned int init_pat7;
	unsigned int freeze_en;
};

struct emi_fake_eng {
	void __iomem **fake_eng_base;
	void **k_addr;
	unsigned int emi_fake_eng_cnt;
	unsigned int bitmap;
	struct fake_eng_set feng_arg;
};

/* global pointer for sysfs operations*/
static struct emi_fake_eng *fakeng;

static int fake_eng_init(unsigned int chn_id)
{
	unsigned long phy_addr;
	u32 val;

	/* Basic setting for fake engine */
	fakeng->feng_arg.chn_number = 0;
	fakeng->feng_arg.rd_dis = 1;
	fakeng->feng_arg.wr_dis = 1;
	fakeng->feng_arg.loop_en = 0;
	fakeng->feng_arg.cross_rk_en = 0;
	fakeng->feng_arg.data_cmp_en = 0;
	fakeng->feng_arg.pat_mode = 5;
	fakeng->feng_arg.rd_wr_joint_en = 0;
	fakeng->feng_arg.rd_amount = 30;
	fakeng->feng_arg.wr_amount = 30;
	fakeng->feng_arg.slow_down = 0;
	fakeng->feng_arg.slow_down_grp = 0;
	fakeng->feng_arg.burst_len = 7;
	fakeng->feng_arg.burst_size = 4;
	fakeng->feng_arg.aw_slc = 0;
	fakeng->feng_arg.ar_slc = 0;
	fakeng->feng_arg.grp_aomunt = 15;
	fakeng->feng_arg.start_addr_wr = 0x40000000;
	fakeng->feng_arg.start_addr_rd = 0x80000000;
	fakeng->feng_arg.start_addr_wr_2nd = 0x40000000;
	fakeng->feng_arg.start_addr_rd_2nd = 0x80000000;
	fakeng->feng_arg.start_addr_wr_extend = 0x00000000;
	fakeng->feng_arg.start_addr_rd_extend = 0x00000000;
	fakeng->feng_arg.start_addr_wr_2nd_extend = 0x0;
	fakeng->feng_arg.start_addr_rd_2nd_extend = 0x0;
	fakeng->feng_arg.addr_offset1 = 0x60;
	fakeng->feng_arg.addr_offset2 = 0;
	fakeng->feng_arg.init_pat0 = 0x0000ffff;
	fakeng->feng_arg.init_pat1 = 0x0000ffff;
	fakeng->feng_arg.init_pat2 = 0x0000ffff;
	fakeng->feng_arg.init_pat3 = 0x0000ffff;
	fakeng->feng_arg.init_pat4 = 0xffffffff;
	fakeng->feng_arg.init_pat5 = 0xffffffff;
	fakeng->feng_arg.init_pat6 = 0xffffffff;
	fakeng->feng_arg.init_pat7 = 0xffffffff;
	fakeng->feng_arg.freeze_en = 0;

	fakeng->feng_arg.chn_number = chn_id;

	fakeng->k_addr[chn_id] = kzalloc(PAGE_SIZE * SIZE, GFP_KERNEL | GFP_DMA);
	if (!fakeng->k_addr[chn_id])
		return -ENOMEM;

	phy_addr = virt_to_phys(fakeng->k_addr[chn_id]);
	fakeng->feng_arg.start_addr_wr = phy_addr;
	fakeng->feng_arg.start_addr_rd = phy_addr;
	fakeng->feng_arg.start_addr_wr_2nd = phy_addr;
	fakeng->feng_arg.start_addr_rd_2nd = phy_addr;

	fakeng->feng_arg.start_addr_wr_extend = phy_addr >> 32;
	fakeng->feng_arg.start_addr_rd_extend = phy_addr >> 32;
	fakeng->feng_arg.start_addr_wr_2nd_extend = phy_addr >> 32;
	fakeng->feng_arg.start_addr_rd_2nd_extend = phy_addr >> 32;

	/* Disable fake engine*/
	writel(0x0, fakeng->fake_eng_base[chn_id] + FAKE_ENG_EN);
	/* Reset fake engine*/
	writel(0x1, fakeng->fake_eng_base[chn_id] + FAKE_ENG_RST);
	writel(0x0, fakeng->fake_eng_base[chn_id] + FAKE_ENG_RST);

	/* FAKE_ENG_CON0 Mode enable control */
	writel((fakeng->feng_arg.rd_wr_joint_en << 17) |
			(fakeng->feng_arg.pat_mode << 13) |
			(fakeng->feng_arg.data_cmp_en << 4) |
			(fakeng->feng_arg.cross_rk_en << 3) |
			(fakeng->feng_arg.loop_en << 2) |
			(fakeng->feng_arg.wr_dis << 1) |
			(fakeng->feng_arg.rd_dis << 0),
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);

	/* FAKE_ENG_CON1 Command control */
	writel((fakeng->feng_arg.slow_down_grp << 20) |
			(fakeng->feng_arg.slow_down << 10) |
			(fakeng->feng_arg.wr_amount << 5) |
			(fakeng->feng_arg.rd_amount << 0),
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON1);

	/* FAKE_ENG_CON2 AXI protocal */
	writel((fakeng->feng_arg.ar_slc << 19) |
			(fakeng->feng_arg.aw_slc << 14) |
			(fakeng->feng_arg.burst_size << 4) |
			(fakeng->feng_arg.burst_len << 0),
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON2);

	/* FAKE_ENG_CON3 Command control */
	writel(fakeng->feng_arg.grp_aomunt,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON3);

	/* FAKE_EN_START_ADDR */
	writel(fakeng->feng_arg.start_addr_wr,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_START_ADDR);

	/* FAKE_EN_START_ADDR_RD */
	writel(fakeng->feng_arg.start_addr_rd,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_START_ADDR_RD);

	/* FAKE_EN_START_ADDR_2ND */
	writel(fakeng->feng_arg.start_addr_wr_2nd,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_START_ADDR_2ND);

	/* FAKE_EN_START_ADDR_RD_2ND */
	writel(fakeng->feng_arg.start_addr_rd_2nd,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_START_ADDR_RD_2ND);

	/* FAKE_ENG_ADDR */
	writel((fakeng->feng_arg.addr_offset2 << 18) |
			(fakeng->feng_arg.addr_offset1 << 8) |
			(fakeng->feng_arg.start_addr_wr_2nd_extend << 4) |
			(fakeng->feng_arg.start_addr_wr_extend << 0),
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_ADDR);

	/* FAKE_ENG_ADDR_RD */
	writel((fakeng->feng_arg.start_addr_rd_2nd_extend << 4) |
			(fakeng->feng_arg.start_addr_rd_extend << 0),
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_ADDR_RD);

	/* INIT_PAT0 */
	writel(fakeng->feng_arg.init_pat0,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT0);

	/* INIT_PAT1 */
	writel(fakeng->feng_arg.init_pat1,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT1);

	/* INIT_PAT2 */
	writel(fakeng->feng_arg.init_pat2,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT2);

	/* INIT_PAT3 */
	writel(fakeng->feng_arg.init_pat3,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT3);

	/* INIT_PAT4 */
	writel(fakeng->feng_arg.init_pat4,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT4);

	/* INIT_PAT5 */
	writel(fakeng->feng_arg.init_pat5,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT5);

	/* INIT_PAT6 */
	writel(fakeng->feng_arg.init_pat6,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT6);

	/* INIT_PAT7 */
	writel(fakeng->feng_arg.init_pat7,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_INIT_PAT7);

	/* compare result freeze */
	writel(fakeng->feng_arg.freeze_en,
			fakeng->fake_eng_base[chn_id] + FAKE_ENG_FREEZE_RESULT);

	/* HASH */
	val = (0x0 << 24) | (0x1 << 20) | (0x1 << 16) | (0x2 << 12) |
			(0x1 << 8) | (fakeng->feng_arg.chn_number << 4) | 0x1;
	writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_HASH);

	return 0;
}

static void emi_fake_eng_stop(unsigned int chn_id)
{
	unsigned int val;

	if (!(fakeng->bitmap & (0x1 << chn_id))) {
		pr_info("%s: fake eng:%d no start, do nothing here\n", __func__, chn_id);
		return;
	}

	/* set loop_en = 0 */
	val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
	val &= ~(0x1 << 2);
	writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
	/* wait for loop mode disable*/
	udelay(5);

    /* Wait fake engine done */
	while (1) {
		val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_DONE);
		pr_debug_ratelimited("%s: fake eng %d, val: %d\n",
							__func__, chn_id, val);

		if (val)
			break;
	}

	/* Disable fake engine */
	writel(0x0, fakeng->fake_eng_base[chn_id] + FAKE_ENG_EN);
	/* Reset fake engine */
	writel(0x1, fakeng->fake_eng_base[chn_id] + FAKE_ENG_RST);
	writel(0x0, fakeng->fake_eng_base[chn_id] + FAKE_ENG_RST);

	/* free k_addr */
	kfree(fakeng->k_addr[chn_id]);

	/* Clear chn_id */
	fakeng->bitmap &= ~(0x1 << chn_id);

	pr_info("%s: disable fake eng:%d done, fake eng bitmap:0x%x\n",
				__func__, chn_id, fakeng->bitmap);
}

static void emi_fake_eng_start(unsigned int chn_id,
								unsigned int wr_en,
								unsigned int rd_en,
								unsigned int latency)
{
	u32 loop_en = 1;
	u32 slow_down, slow_down_grp, val;
	int ret;

	if (fakeng->bitmap & (0x1 << chn_id)) {
		pr_info("%s:Please disable fake eng:%d first\n", __func__, chn_id);
		return;
	}

	/* set chn_id bitmap */
	fakeng->bitmap |= (0x1 << chn_id);

	/* Fake engine basic setting */
	ret = fake_eng_init(chn_id);
	if (ret) {
		pr_info("%s: Cannot allocate memory, stop trigger fake engine\n", __func__);
		return;
	}

	/* set slow_down */
	slow_down = latency;

	/* Start to incease BW,  */
	slow_down_grp = slow_down;

	if (rd_en) {
		/* set rd_dis = 0 to enable read command*/
		val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
		val &= ~(0x1 << 0);
		writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
	}

	if (wr_en) {
		/* set wr_dis = 0 to enable write command*/
		val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
		val &= ~(0x1 << 1);
		writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
	}

	/* set slow_down = 0 */
	val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON1);
	val &= ~(0xfffff << 10);
	writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON1);

	/* set slow_down */
	val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON1);
	val |= (slow_down_grp << 20) | (slow_down << 10);
	writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON1);

	/* set loop_en = 1 */
	val = readl(fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);
	val |= (loop_en << 2);
	writel(val, fakeng->fake_eng_base[chn_id] + FAKE_ENG_CON0);

	/* Enable fake engine */
	writel(0x1, fakeng->fake_eng_base[chn_id] + FAKE_ENG_EN);

	pr_info("%s: chn_id:%d, wr_en:%d, rd_en:%d, latency:%d\n",
			__func__, chn_id, wr_en, rd_en, slow_down);
	pr_info("%s: fake eng bitmap:0x%x\n", __func__, fakeng->bitmap);
}

static ssize_t emi_fake_eng_show(struct device_driver *driver, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", fakeng->bitmap);
}

static ssize_t emi_fake_eng_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int chn_id, en, wr_en, rd_en, latency;
	int ret = 0, n;

	n = sscanf(buf, "%u,%u,%u,%u,%u\n",
				&chn_id, &en, &wr_en, &rd_en, &latency);

	if (n != 5) {
		pr_info("%s: Please enter correct input format. n=%d\n", __func__, n);
		goto out;
	}

	pr_info("%s: %u %u %u %u %u\n",
				__func__, chn_id, en, wr_en, rd_en, latency);

	if (chn_id >= fakeng->emi_fake_eng_cnt) {
		pr_info("%s: Please enter channel number between 0 ~ %d\n",
					__func__, fakeng->emi_fake_eng_cnt - 1);
		goto out;
	}

	if (en > 1 || wr_en > 1 || rd_en > 1) {
		pr_info("%s: Please enter en, wr_en, rd_en value for 0 or 1\n", __func__);
		goto out;
	}

	if (wr_en == 0 && rd_en == 0) {
		pr_info("%s: At least one of wr_en or rd_en should be 1\n", __func__);
		goto out;
	}

	if (latency > 100) {
		pr_info("%s: Please enter latency value between 1 ~ 100\n", __func__);
		goto out;
	}

	if (en)
		emi_fake_eng_start(chn_id, wr_en, rd_en, latency);
	else
		emi_fake_eng_stop(chn_id);

out:
	return ret ? : count;
}

static DRIVER_ATTR_RW(emi_fake_eng);

static int emi_fake_eng_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");
	fakeng = NULL;
	return 0;
}

static int emi_fake_eng_probe(struct platform_device *pdev)
{
	struct device_node *emi_feng_node  = pdev->dev.of_node;
	struct emi_fake_eng *feng;

	unsigned int i;
	int ret;

	dev_info(&pdev->dev, "driver probed\n");

	feng = devm_kzalloc(&pdev->dev, sizeof(struct emi_fake_eng), GFP_KERNEL);
	if (!feng)
		return -ENOMEM;

	ret = of_property_count_elems_of_size(emi_feng_node,
		"reg", sizeof(unsigned int) * 4);
	if (ret <= 0) {
		dev_err(&pdev->dev, "No reg\n");
		return -ENXIO;
	}
	feng->emi_fake_eng_cnt = (unsigned int)ret;

	feng->fake_eng_base = devm_kmalloc_array(&pdev->dev,
		feng->emi_fake_eng_cnt, sizeof(phys_addr_t), GFP_KERNEL);
	if (!(feng->fake_eng_base))
		return -ENOMEM;


	for (i = 0; i < feng->emi_fake_eng_cnt; i++) {
		feng->fake_eng_base[i] = of_iomap(emi_feng_node, i);
		if (!feng->fake_eng_base[i])
			return -ENOMEM;
	}

	feng->k_addr = devm_kmalloc_array(&pdev->dev,
		feng->emi_fake_eng_cnt, sizeof(void *), GFP_KERNEL);
	if (!(feng->k_addr))
		return -ENOMEM;

	/* Set to global pointer */
	fakeng = feng;

	/* Initial channel bitmap */
	fakeng->bitmap = 0;

	return 0;
}

static const struct of_device_id emi_fake_eng_of_ids[] = {
	{.compatible = "mediatek,emi-fake-engine",},
	{}
};

static struct platform_driver emi_fake_eng_drv = {
	.probe = emi_fake_eng_probe,
	.remove = emi_fake_eng_remove,
	.driver = {
		.name = "emi_fake_eng_drv",
		.owner = THIS_MODULE,
		.of_match_table = emi_fake_eng_of_ids,
	},
};

static void __exit emi_fake_eng_exit(void)
{
	pr_info("emi fake engine unloaded\n");

	driver_remove_file(&emi_fake_eng_drv.driver,
					&driver_attr_emi_fake_eng);

	platform_driver_unregister(&emi_fake_eng_drv);

}
static int __init emi_fake_eng_init(void)
{
	int ret;

	pr_info("emi fake engine loaded\n");

	ret = platform_driver_register(&emi_fake_eng_drv);
	if (ret) {
		pr_info("emi fake engine: failed to register dirver\n");
		return ret;
	}

	ret = driver_create_file(&emi_fake_eng_drv.driver,
							&driver_attr_emi_fake_eng);
	if (ret) {
		pr_info("emi fake engine: failed to create control file\n");
		return ret;
	}

	return 0;
}

module_init(emi_fake_eng_init);
module_exit(emi_fake_eng_exit);

MODULE_DESCRIPTION("MediaTek EMI Fake Engine Driver");
MODULE_LICENSE("GPL v2");
