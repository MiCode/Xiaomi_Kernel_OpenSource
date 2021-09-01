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

#define SIZE (4096*15)

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
	unsigned int start;
	unsigned int flag;
	struct fake_eng_set feng_arg;
};

/* global pointer for sysfs operations*/
static struct emi_fake_eng *fakeng;

static int fake_eng_init(void)
{
	unsigned long phy_addr;
	unsigned int i;
	u32 val;
	/* 4 channel chn_num correspond to fake engine */
	unsigned int chn_num[4] = {0, 2, 1, 3};

	/* Basic settign for fake engine */
	fakeng->feng_arg.chn_number = 0;
	fakeng->feng_arg.rd_dis = 1;
	fakeng->feng_arg.wr_dis = 0;
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
	fakeng->feng_arg.addr_offset1 = 0;
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

	for (i = 0; i < fakeng->emi_fake_eng_cnt; ++i) {
		/*
		 * 2 channel chn_num sequence corresponding
		 * to fake engine is 0 1; 4 channel is 0 2 1 3
		 */
		if ((i == 1) && (fakeng->emi_fake_eng_cnt == 2))
			fakeng->feng_arg.chn_number = chn_num[i] - 1;
		else
			fakeng->feng_arg.chn_number = chn_num[i];

		fakeng->k_addr[i] = kmalloc(SIZE, GFP_KERNEL);
		if (!fakeng->k_addr[i])
			return -ENOMEM;

		phy_addr = virt_to_phys(fakeng->k_addr[i]);
		fakeng->feng_arg.start_addr_wr = phy_addr;
		fakeng->feng_arg.start_addr_rd = phy_addr;
		fakeng->feng_arg.start_addr_wr_2nd = phy_addr;
		fakeng->feng_arg.start_addr_rd_2nd = phy_addr;

		/* Disable fake engine*/
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_EN);
		/* Reset fake engine*/
		writel(0x1, fakeng->fake_eng_base[i] + FAKE_ENG_RST);
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_RST);

		/* FAKE_ENG_CON0 Mode enable control */
		writel((fakeng->feng_arg.rd_wr_joint_en << 17) |
				(fakeng->feng_arg.pat_mode << 13) |
				(fakeng->feng_arg.data_cmp_en << 4) |
				(fakeng->feng_arg.cross_rk_en << 3) |
				(fakeng->feng_arg.loop_en << 2) |
				(fakeng->feng_arg.wr_dis << 1) |
				(fakeng->feng_arg.rd_dis << 0),
				fakeng->fake_eng_base[i] + FAKE_ENG_CON0);

		/* FAKE_ENG_CON1 Command control */
		writel((fakeng->feng_arg.slow_down_grp << 20) |
				(fakeng->feng_arg.slow_down << 10) |
				(fakeng->feng_arg.wr_amount << 5) |
				(fakeng->feng_arg.rd_amount << 0),
				fakeng->fake_eng_base[i] + FAKE_ENG_CON1);

		/* FAKE_ENG_CON2 AXI protocal */
		writel((fakeng->feng_arg.ar_slc << 19) |
				(fakeng->feng_arg.aw_slc << 14) |
				(fakeng->feng_arg.burst_size << 4) |
				(fakeng->feng_arg.burst_len << 0),
				fakeng->fake_eng_base[i] + FAKE_ENG_CON2);

		/* FAKE_ENG_CON3 Command control */
		writel(fakeng->feng_arg.grp_aomunt,
				fakeng->fake_eng_base[i] + FAKE_ENG_CON3);

		/* FAKE_EN_START_ADDR */
		writel(fakeng->feng_arg.start_addr_wr,
				fakeng->fake_eng_base[i] + FAKE_ENG_START_ADDR);

		/* FAKE_EN_START_ADDR_RD */
		writel(fakeng->feng_arg.start_addr_rd,
				fakeng->fake_eng_base[i] + FAKE_ENG_START_ADDR_RD);

		/* FAKE_EN_START_ADDR_2ND */
		writel(fakeng->feng_arg.start_addr_wr_2nd,
				fakeng->fake_eng_base[i] + FAKE_ENG_START_ADDR_2ND);

		/* FAKE_EN_START_ADDR_RD_2ND */
		writel(fakeng->feng_arg.start_addr_rd_2nd,
				fakeng->fake_eng_base[i] + FAKE_ENG_START_ADDR_RD_2ND);

		/* FAKE_ENG_ADDR */
		writel((fakeng->feng_arg.addr_offset2 << 18) |
				(fakeng->feng_arg.addr_offset1 << 8) |
				(fakeng->feng_arg.start_addr_wr_2nd_extend << 4) |
				(fakeng->feng_arg.start_addr_wr_extend << 0),
				fakeng->fake_eng_base[i] + FAKE_ENG_ADDR);

		/* FAKE_ENG_ADDR_RD */
		writel((fakeng->feng_arg.start_addr_rd_2nd_extend << 4) |
				(fakeng->feng_arg.start_addr_rd_extend << 0),
				fakeng->fake_eng_base[i] + FAKE_ENG_ADDR_RD);

		/* INIT_PAT0 */
		writel(fakeng->feng_arg.init_pat0,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT0);

		/* INIT_PAT1 */
		writel(fakeng->feng_arg.init_pat1,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT1);

		/* INIT_PAT2 */
		writel(fakeng->feng_arg.init_pat2,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT2);

		/* INIT_PAT3 */
		writel(fakeng->feng_arg.init_pat3,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT3);

		/* INIT_PAT4 */
		writel(fakeng->feng_arg.init_pat4,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT4);

		/* INIT_PAT5 */
		writel(fakeng->feng_arg.init_pat5,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT5);

		/* INIT_PAT6 */
		writel(fakeng->feng_arg.init_pat6,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT6);

		/* INIT_PAT7 */
		writel(fakeng->feng_arg.init_pat7,
				fakeng->fake_eng_base[i] + FAKE_ENG_INIT_PAT7);

		/* compare result freeze */
		writel(fakeng->feng_arg.freeze_en,
				fakeng->fake_eng_base[i] + FAKE_ENG_FREEZE_RESULT);

		/* HASH */
		val = (0x0 << 24) | (0x1 << 20) | (0x1 << 16) | (0x2 << 12) |
				(0x1 << 8) | (fakeng->feng_arg.chn_number << 4) | 0x1;
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_HASH);

	}
	return 0;
}

static void emi_fake_eng_stop(void)
{
	unsigned int i, val;

	if (!fakeng->flag) {
		pr_info("%s: fake eng no start, do nothing here\n", __func__);
		return;
	}
	/* stop loop mode */
	for (i = 0; i < fakeng->emi_fake_eng_cnt; ++i) {
		/* set loop_en = 0 */
		val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_CON0);
		val &= ~(0x1 << 2);
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_CON0);

		/* wait for loop mode disable*/
		udelay(5);
	}

    /* Wait fake engine done */
	for (i = 0; i < fakeng->emi_fake_eng_cnt; ++i) {
		while (1) {
			val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_DONE);
			pr_debug_ratelimited("%s: fake eng %d, val: %d\n",
								__func__, i, val);

			if (val)
				break;
		}
	}
	pr_info("%s: check fake eng done\n", __func__);

	for (i = 0; i < fakeng->emi_fake_eng_cnt; ++i) {
		/* Disable fake engine*/
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_EN);
		/* Reset fake engine*/
		writel(0x1, fakeng->fake_eng_base[i] + FAKE_ENG_RST);
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_RST);

		/* free k_addr */
		kfree(fakeng->k_addr[i]);
	}

	fakeng->flag = 0;
}

static void emi_fake_eng_start(u32 start)
{
	u32 loop_en = 1;
	u32 slow_down, slow_down_grp, val;
	unsigned int i;
	int ret;

	if (fakeng->flag) {
		pr_info("%s: Please use echo 0 to close fake engine first\n", __func__);
		return;
	}

	/* Fake engine basic setting */
	ret = fake_eng_init();
	if (ret) {
		pr_info("%s: Cannot allocate memory, stop trigger fake engine\n", __func__);
		return;
	}

	/* set flag true */
	fakeng->flag = 1;

	if (start == 1)
		start = 0;

	/*set slow_down */
	slow_down = start;

	/* Start to incease BW,  */
	slow_down_grp = slow_down;

	/* fake engine slow down setting */
	for (i = 0; i < fakeng->emi_fake_eng_cnt; ++i) {
		/* Disable fake engine*/
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_EN);
		/* Reset fake engine*/
		writel(0x1, fakeng->fake_eng_base[i] + FAKE_ENG_RST);
		writel(0x0, fakeng->fake_eng_base[i] + FAKE_ENG_RST);

		/* set rd_dis = 0 to enable read command*/
		val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_CON0);
		val &= ~(0x1 << 0);
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_CON0);

		/* set slow_down = 0 */
		val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_CON1);
		val &= ~(0xfffff << 10);
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_CON1);

		/* set slow_down */
		val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_CON1);
		val |= (slow_down_grp << 20) | (slow_down << 10);
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_CON1);

		/* set loop_en = 1 */
		val = readl(fakeng->fake_eng_base[i] + FAKE_ENG_CON0);
		val |= (loop_en << 2);
		writel(val, fakeng->fake_eng_base[i] + FAKE_ENG_CON0);
	}

	/* Enable fake engine */
	for (i = 0 ; i < fakeng->emi_fake_eng_cnt; ++i)
		writel(0x1, fakeng->fake_eng_base[i] + FAKE_ENG_EN);

	pr_info("%s: enable fake eng slow_down: %d\n", __func__, slow_down);
}

static ssize_t emi_fake_eng_show(struct device_driver *driver, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fakeng->start);
}

static ssize_t emi_fake_eng_store
	(struct device_driver *driver, const char *buf, size_t count)
{
	u32 start;
	int ret;

	ret = kstrtou32(buf, 10, &start);
	if (ret)
		return ret;

	fakeng->start = start;
	pr_info("%s start: %d\n", __func__, start);

	if (start > 0 && start <= 100)
		emi_fake_eng_start(start);
	else if (start == 0)
		emi_fake_eng_stop();
	else
		pr_info("%s: Please enter slow down value between 1 ~ 100\n", __func__);


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

	/* Set default flag value */
	fakeng->flag = 0;

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
