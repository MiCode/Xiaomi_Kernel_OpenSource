// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "cmdq-util.h"

#define mminfra_read(addr)			readl_relaxed(addr)
#define mminfra_write(addr, val)		writel(val, addr)
#define mminfra_crit(fmt, ...)        pr_info("[MMInfra] " fmt, ##__VA_ARGS__)

enum {
	DISPSYS_BASE,
	DISPSYS1_BASE,
	MDPSYS_BASE,
	MDPSYS1_BASE,
	DISP_LARB_0,
	DISP_LARB_1,
	DISP_LARB_2,
	DISP_LARB_3,
	MDP_LARB_0,
	MDP_LARB_1,
};

static void __iomem *dispsys_base, *dispsys1_base;
static void __iomem *mdpsys_base, *mdpsys1_base;
static void __iomem *disp_larb_0_base, *disp_larb_1_base, *disp_larb_2_base, *disp_larb_3_base;
static void __iomem *mdp_larb_0_base, *mdp_larb_1_base, *mdp_larb_2_base;
static unsigned int mm_sram_base;
static unsigned int disp_larb_0_fake, disp_larb_1_fake, disp_larb_2_fake, disp_larb_3_fake;
static unsigned int mdp_larb_0_fake, mdp_larb_1_fake, mdp_larb_2_fake;

static struct device *dev;
static struct platform_device *g_pdev;
static bool is_init;

static void init_mmsys(void)
{
	if (dispsys_base) {
		mminfra_write(dispsys_base + 0x108, 0xffffffff);
		mminfra_write(dispsys_base + 0x118, 0xffffffff);
		mminfra_write(dispsys_base + 0x1a8, 0xffffffff);
	}

	if (dispsys1_base) {
		mminfra_write(dispsys1_base + 0x108, 0xffffffff);
		mminfra_write(dispsys1_base + 0x118, 0xffffffff);
		mminfra_write(dispsys1_base + 0x1a8, 0xffffffff);
	}

	if (mdpsys_base) {
		mminfra_write(mdpsys_base + 0x108, 0xffffffff);
		mminfra_write(mdpsys_base + 0x118, 0xffffffff);
		mminfra_write(mdpsys_base + 0x128, 0xffffffff);
		mminfra_write(mdpsys_base + 0x138, 0xffffffff);
		mminfra_write(mdpsys_base + 0x148, 0xffffffff);
	}

	if (mdpsys1_base) {
		mminfra_write(mdpsys1_base + 0x108, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x118, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x128, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x138, 0xffffffff);
		mminfra_write(mdpsys1_base + 0x148, 0xffffffff);
	}
}

static void init_mdp_smi(int id, void __iomem *mdp_larb_base, unsigned int fake_port)
{
	/* NON_SEC_CON */
	mminfra_write(mdp_larb_base + 0x380 + 0x4 * fake_port,
		mminfra_read(mdp_larb_base + 0x380 + 0x4 * fake_port) | 0x000f0000);

	mminfra_crit("%s %d NON_SEC_CON=0x%x\n", __func__, id,
		mminfra_read(mdp_larb_base + 0x380 + 0x4 * fake_port));
}

static void init_disp_smi(int id, void __iomem *disp_larb_base, unsigned int fake_port)
{
	/* NON_SEC_CON */
	mminfra_write(disp_larb_base + 0x380 + 0x4 * fake_port,
		mminfra_read(disp_larb_base + 0x380 + 0x4 * fake_port) & 0xfff0fffe);

	mminfra_crit("%s %d NON_SEC_CON=0x%x\n", __func__, id,
		mminfra_read(disp_larb_base + 0x380 + 0x4 * fake_port));
}

static void init_smi(void)
{
	if (mdp_larb_0_base)
		init_mdp_smi(0, mdp_larb_0_base, mdp_larb_0_fake);
	if (mdp_larb_1_base)
		init_mdp_smi(1, mdp_larb_1_base, mdp_larb_1_fake);
	if (mdp_larb_2_base)
		init_mdp_smi(1, mdp_larb_2_base, mdp_larb_2_fake);

	if (disp_larb_0_base)
		init_disp_smi(0, disp_larb_0_base, disp_larb_0_fake);
	if (disp_larb_1_base)
		init_disp_smi(1, disp_larb_1_base, disp_larb_1_fake);
	if (disp_larb_2_base)
		init_disp_smi(2, disp_larb_2_base, disp_larb_2_fake);
	if (disp_larb_3_base)
		init_disp_smi(3, disp_larb_3_base, disp_larb_3_fake);
}

static void fake_eng_set(unsigned int id, void __iomem *subsys_base, unsigned int eng_id,
	unsigned int rd_addr, unsigned int wr_addr, unsigned int wr_pat, unsigned int length,
			unsigned int burst, unsigned int dis_rd, unsigned int dis_wr,
			unsigned int latency, unsigned int loop)
{
	unsigned int shift = 0;

	if (subsys_base) {
		if (eng_id == 1)
			shift = 0x20;

		/* SUBSYS_FAKE_ENG_RD_ADDR */
		mminfra_write(subsys_base + 0x210 + shift, rd_addr);
		/* SUBSYS_FAKE_ENG_WR_ADDR */
		mminfra_write(subsys_base + 0x214 + shift, wr_addr);
		/* SUBSYS_FAKE_ENG_CON0 */
		mminfra_write(subsys_base + 0x208 + shift,
			(wr_pat << 24) | (loop << 22) | length);
		/* SUBSYS_FAKE_ENG_CON1 */
		mminfra_write(subsys_base + 0x20c + shift,
			(burst << 12) | (dis_wr << 11) | (dis_rd << 10) | latency);
		mminfra_write(subsys_base + 0x204 + shift, 0x1); /* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + 0x204 + shift, 0x0); /* SUBSYS_FAKE_ENG_RST */
		mminfra_write(subsys_base + 0x200 + shift, 0x3); /* SUBSYS_FAKE_ENG_EN */

		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_RD_ADDR=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x210 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_WR_ADDR=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x214 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_CON0=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x208 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_CON1=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x20c + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_RST=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x204 + shift));
		mminfra_crit("%s id:%u eng:%u SUBSYS_FAKE_ENG_EN=0x%x\n", __func__,
			id, eng_id, mminfra_read(subsys_base + 0x200 + shift));

		/* SUBSYS_FAKE_ENG_STATE */
		if ((mminfra_read(subsys_base + 0x218) & 0x1) != 0)
			mminfra_crit("%s id:%u eng:%u FAKE_ENG_STATE is busy\n",
				__func__, id, eng_id);
		else
			mminfra_crit("%s id:%u eng:%u FAKE_ENG_STATE is idle\n",
				__func__, id, eng_id);
	}
}

static int init_ctrl_base(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dispsys");
	if (!res) {
		dev_notice(dev, "could not get resource for dispsys\n");
		return -EINVAL;
	}

	dispsys_base = ioremap(res->start, resource_size(res));
	if (IS_ERR(dispsys_base)) {
		dev_notice(dev, "could not ioremap resource for dispsys:%d\n",
			PTR_ERR(dispsys_base));
		dispsys_base = NULL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dispsys1");
	if (!res)
		dev_notice(dev, "could not get resource for dispsys1\n");

	if (res) {
		dispsys1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(dispsys1_base)) {
			dev_notice(dev, "could not ioremap resource for dispsys1\n");
			dispsys1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdpsys");
	if (!res)
		dev_notice(dev, "could not get resource for mdpsys\n");

	if (res) {
		mdpsys_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdpsys_base)) {
			dev_notice(dev, "could not ioremap resource for mdpsys\n");
			mdpsys_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdpsys1");
	if (!res)
		dev_notice(dev, "could not get resource for mdpsys1\n");

	if (res) {
		mdpsys1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdpsys1_base)) {
			dev_notice(dev, "could not ioremap resource for mdpsys1\n");
			mdpsys1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_0");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_0\n");

	if (res) {
		disp_larb_0_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_0_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_0\n");
			disp_larb_0_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_1");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_1\n");

	if (res) {
		disp_larb_1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_1_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_1\n");
			disp_larb_1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_2");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_2\n");

	if (res) {
		disp_larb_2_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_2_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_2\n");
			disp_larb_2_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "disp_larb_3");
	if (!res)
		dev_notice(dev, "could not get resource for disp_larb_3\n");

	if (res) {
		disp_larb_3_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(disp_larb_3_base)) {
			dev_notice(dev, "could not ioremap resource for disp_larb_3\n");
			disp_larb_3_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_0");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_0\n");

	if (res) {
		mdp_larb_0_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_0_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_0\n");
			mdp_larb_0_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_1");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_1\n");

	if (res) {
		mdp_larb_1_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_1_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_1\n");
			mdp_larb_1_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_larb_2");
	if (!res)
		dev_notice(dev, "could not get resource for mdp_larb_2\n");

	if (res) {
		mdp_larb_2_base = ioremap(res->start, resource_size(res));
		if (IS_ERR(mdp_larb_2_base)) {
			dev_notice(dev, "could not ioremap resource for mdp_larb_2\n");
			mdp_larb_2_base = NULL;
		}
	}

	of_property_read_u32(dev->of_node, "disp_larb0_fake_port", &disp_larb_0_fake);
	of_property_read_u32(dev->of_node, "disp_larb1_fake_port", &disp_larb_1_fake);
	of_property_read_u32(dev->of_node, "disp_larb2_fake_port", &disp_larb_2_fake);
	of_property_read_u32(dev->of_node, "disp_larb3_fake_port", &disp_larb_3_fake);

	of_property_read_u32(dev->of_node, "mdp_larb0_fake_port", &mdp_larb_0_fake);
	of_property_read_u32(dev->of_node, "mdp_larb1_fake_port", &mdp_larb_1_fake);
	of_property_read_u32(dev->of_node, "mdp_larb2_fake_port", &mdp_larb_2_fake);

	of_property_read_u32(dev->of_node, "mm_sram_base", &mm_sram_base);
	return 0;
}

static int do_mminfra_imax(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int latency, is_sram;
	void *dram_base;
	dma_addr_t dram_phy_base;

	pr_notice("%s in\n", __func__);
	ret = sscanf(val, "%u %u", &latency, &is_sram);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}

	if (!is_init) {
		init_ctrl_base(g_pdev);
		is_init = true;
	}
	init_mmsys();
	init_smi();
	cmdq_util_mminfra_cmd(2);

	if (mm_sram_base) {
		fake_eng_set(MDPSYS_BASE, mdpsys_base, 0, mm_sram_base, mm_sram_base,
			4, 255, 7, 0, 0, latency, 1);
		fake_eng_set(MDPSYS1_BASE, mdpsys1_base, 0, mm_sram_base, mm_sram_base,
			4, 255, 7, 0, 0, latency, 1);
	}

	dram_base = dma_alloc_attrs(dev, 1024*1024, &dram_phy_base,
				GFP_KERNEL, DMA_ATTR_FORCE_CONTIGUOUS);
	if (!dram_base) {
		mminfra_crit("%s: allocate dram memory failed\n", __func__);
		return -ENOMEM;
	}

	fake_eng_set(DISPSYS_BASE, dispsys_base, 0, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS_BASE, dispsys_base, 1, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS1_BASE, dispsys1_base, 0, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);
	fake_eng_set(DISPSYS1_BASE, dispsys1_base, 1, dram_phy_base, dram_phy_base,
		4, 255, 7, 0, 0, latency, 1);

	return ret;
}

static struct kernel_param_ops mminfra_imax_ops = {
	.set = do_mminfra_imax,
};
module_param_cb(mminfra_imax, &mminfra_imax_ops, NULL, 0644);
MODULE_PARM_DESC(mminfra_imax, "mminfra imax");

static int mminfra_imax_probe(struct platform_device *pdev)
{
	g_pdev = pdev;
	dev = &pdev->dev;
	pr_notice("%s in\n", __func__);
	of_property_read_u32(dev->of_node, "disp_larb0_fake_port", &disp_larb_0_fake);
	of_property_read_u32(dev->of_node, "disp_larb1_fake_port", &disp_larb_1_fake);
	of_property_read_u32(dev->of_node, "disp_larb2_fake_port", &disp_larb_2_fake);
	of_property_read_u32(dev->of_node, "disp_larb3_fake_port", &disp_larb_3_fake);

	of_property_read_u32(dev->of_node, "mdp_larb0_fake_port", &mdp_larb_0_fake);
	of_property_read_u32(dev->of_node, "mdp_larb1_fake_port", &mdp_larb_1_fake);
	of_property_read_u32(dev->of_node, "mdp_larb2_fake_port", &mdp_larb_2_fake);
	pr_notice("%s out\n", __func__);
	return 0;
}

static const struct of_device_id of_mminfra_imax_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-imax",
	},
	{}
};

static struct platform_driver mminfra_imax_drv = {
	.probe = mminfra_imax_probe,
	.driver = {
		.name = "mtk-mminfra-imax",
		.of_match_table = of_mminfra_imax_match_tbl,
	},
};

static int __init mtk_mminfra_imax_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_imax_drv);
	if (status) {
		pr_notice("Failed to register MMInfra imax driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_imax_exit(void)
{
	platform_driver_unregister(&mminfra_imax_drv);
}

module_init(mtk_mminfra_imax_init);
module_exit(mtk_mminfra_imax_exit);
MODULE_DESCRIPTION("MTK MMInfra IMAX driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
