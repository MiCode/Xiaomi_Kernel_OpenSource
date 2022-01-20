// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>

#include <dbgtop.h>

static void __iomem *DBGTOP_BASE;
static unsigned int LATCH_CTL2_OFFSET;

static int mtk_dbgtop_probe(struct platform_device *pdev);

static int mtk_dbgtop_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtk_dbgtop_of_ids[] = {
	{.compatible = "mediatek,dbgtop",},
	{}
};
#endif

static struct platform_driver mtk_dbgtop = {
	.probe = mtk_dbgtop_probe,
	.remove = mtk_dbgtop_remove,
	.driver = {
		.name = "mtk_dbgtop",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mtk_dbgtop_of_ids,
#endif
	},
};

static int mtk_dbgtop_probe(struct platform_device *pdev)
{
	struct resource *res;

	if (DBGTOP_BASE) {
		pr_info("%s: already got the base addr\n", __func__);
		return 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	DBGTOP_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(DBGTOP_BASE)) {
		pr_info("[DBGTOP] unable to map DBGTOP_BASE");
		return -EINVAL;
	}

	return 0;
}

static ssize_t dbgtop_config_show(struct device_driver *driver, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"%s,0x%x\n%s,0x%x\n%s,0x%x\n%s,0x%x\n",
		"MTK_DBGTOP_MODE", readl(IOMEM(MTK_DBGTOP_MODE)),
		"MTK_DBGTOP_LATCH_CTL", readl(IOMEM(MTK_DBGTOP_LATCH_CTL)),
		"MTK_DBGTOP_DEBUG_CTL", readl(IOMEM(MTK_DBGTOP_DEBUG_CTL)),
		"MTK_DBGTOP_DEBUG_CTL2", readl(IOMEM(MTK_DBGTOP_DEBUG_CTL2)));

	return strlen(buf);
}

static ssize_t dbgtop_config_store
	(struct device_driver *driver, const char *buf, size_t count)
{
#if MTK_DBGTOP_TEST
	char *ptr;
	char *command;

	if ((strlen(buf) + 1) > DBGTOP_MAX_CMD_LEN) {
		pr_info("[DBGTOP] store command overflow\n");
		return count;
	}

	command = kmalloc((size_t) DBGTOP_MAX_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;
	strncpy(command, buf, (size_t) DBGTOP_MAX_CMD_LEN);

	ptr = strsep(&command, " ");

	if (!strncmp(buf, "0", strlen("0"))) {
		mtk_dbgtop_dram_reserved(0);
		mtk_dbgtop_cfg_dvfsrc(0);
		mtk_dbgtop_pause_dvfsrc(0);
		goto dbgtop_config_store;
	} else if (!strncmp(buf, "1", strlen("1"))) {
		mtk_dbgtop_dram_reserved(1);
		mtk_dbgtop_cfg_dvfsrc(1);
		mtk_dbgtop_pause_dvfsrc(1);
	}

dbgtop_config_store:
	kfree(command);
#endif

	return count;
}

static DRIVER_ATTR_RW(dbgtop_config);

/*
 * emi_ctrl_init: module init function.
 */
static int __init mtk_dbgtop_init(void)
{
	int ret;

	/* register DBGTOP interface */
	ret = platform_driver_register(&mtk_dbgtop);
	if (ret)
		pr_info("[DBGTOP] fail to register mtk_dbgtop driver");

	ret = driver_create_file(&mtk_dbgtop.driver,
		&driver_attr_dbgtop_config);
	if (ret)
		pr_info("[DBGTOP] fail to create dbgtop_config");

	return 0;
}

/*
 * mtk_dbgtop_exit: module exit function.
 */
static void __exit mtk_dbgtop_exit(void)
{
}

int mtk_dbgtop_dram_reserved(int enable)
{
	unsigned int tmp;

	if (DBGTOP_BASE == NULL)
		return -1;

	if (enable == 1) {
		/* enable DDR reserved mode */
		tmp = readl(IOMEM(MTK_DBGTOP_MODE));
		tmp |= (MTK_DBGTOP_MODE_DDR_RESERVE | MTK_DBGTOP_MODE_KEY);
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MODE);
	} else if (enable == 0) {
		/* disable DDR reserved mode */
		tmp = readl(IOMEM(MTK_DBGTOP_MODE));
		tmp &= (~MTK_DBGTOP_MODE_DDR_RESERVE);
		tmp |= MTK_DBGTOP_MODE_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MODE);
	}
	pr_info("%s: MTK_DBGTOP_MODE(0x%x)\n",
		__func__, readl(IOMEM(MTK_DBGTOP_MODE)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dram_reserved);

int mtk_dbgtop_cfg_dvfsrc(int enable)
{
	unsigned int debug_ctl2, latch_ctl;

	if (DBGTOP_BASE == NULL)
		return -1;

	debug_ctl2 = readl(IOMEM(MTK_DBGTOP_DEBUG_CTL2));
	latch_ctl = readl(IOMEM(MTK_DBGTOP_LATCH_CTL));

	if (enable == 1) {
		/* enable dvfsrc_en */
		debug_ctl2 |= MTK_DBGTOP_DVFSRC_EN;

		/* set dvfsrc_latch */
		latch_ctl |= MTK_DBGTOP_DVFSRC_LATCH_EN;
	} else {
		/* disable is not allowed */
		return -1;
	}

	debug_ctl2 |= MTK_DBGTOP_DEBUG_CTL2_KEY;
	mt_reg_sync_writel(debug_ctl2, MTK_DBGTOP_DEBUG_CTL2);

	latch_ctl |= MTK_DBGTOP_LATCH_CTL_KEY;
	mt_reg_sync_writel(latch_ctl, MTK_DBGTOP_LATCH_CTL);

	pr_info("%s: MTK_DBGTOP_DEBUG_CTL2(0x%x)\n",
		__func__, readl(IOMEM(MTK_DBGTOP_DEBUG_CTL2)));
	pr_info("%s: MTK_DBGTOP_LATCH_CTL(0x%x)\n",
		__func__, readl(IOMEM(MTK_DBGTOP_LATCH_CTL)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_cfg_dvfsrc);

int mtk_dbgtop_pause_dvfsrc(int enable)
{
	unsigned int tmp;
	unsigned int count = 100;

	if (DBGTOP_BASE == NULL)
		return -1;

	if (!(readl(IOMEM(MTK_DBGTOP_DEBUG_CTL2))
		& MTK_DBGTOP_DVFSRC_EN)) {
		pr_info("%s: not enable DVFSRC\n", __func__);
		return 0;
	}

	if (enable == 1) {
		/* enable DVFSRC pause */
		tmp = readl(IOMEM(MTK_DBGTOP_DEBUG_CTL));
		tmp |= MTK_DBGTOP_DVFSRC_PAUSE_PULSE;
		tmp |= MTK_DBGTOP_DEBUG_CTL_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_DEBUG_CTL);
		while (count--) {
			if ((readl(IOMEM(MTK_DBGTOP_DEBUG_CTL))
				& MTK_DBGTOP_DVFSRC_SUCECESS_ACK))
				break;
			udelay(10);
		}

		pr_info("%s: DVFSRC pause result(0x%x)\n",
			__func__, readl(IOMEM(MTK_DBGTOP_DEBUG_CTL)));
	} else if (enable == 0) {
		/* disable DVFSRC pause */
		tmp = readl(IOMEM(MTK_DBGTOP_DEBUG_CTL));
		tmp &= (~MTK_DBGTOP_DVFSRC_PAUSE_PULSE);
		tmp |= MTK_DBGTOP_DEBUG_CTL_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_DEBUG_CTL);
	}

	pr_info("%s: MTK_DBGTOP_DEBUG_CTL(0x%x)\n",
		__func__, readl(IOMEM(MTK_DBGTOP_DEBUG_CTL)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_pause_dvfsrc);

static int __init mtk_dbgtop_get_base_addr(void)
{
	struct device_node *np_dbgtop;
	int found_dbgtop_base = 0;

	for_each_matching_node(np_dbgtop, mtk_dbgtop_of_ids) {
		pr_info("%s: compatible node found: %s\n",
			__func__, np_dbgtop->name);
		found_dbgtop_base = 1;
		break;
	}

	if (!DBGTOP_BASE && found_dbgtop_base) {
		DBGTOP_BASE = of_iomap(np_dbgtop, 0);
		if (!DBGTOP_BASE)
			pr_info("%s: dbgtop iomap failed\n", __func__);
	} else if (!DBGTOP_BASE) {
		np_dbgtop = of_find_compatible_node(NULL, NULL, "mediatek,toprgu");
		DBGTOP_BASE = of_iomap(np_dbgtop, 0);
		if (!DBGTOP_BASE)
			pr_info("%s: dbgtop iomap failed\n", __func__);
		pr_info("use toprgu to setting\n");
	}

	return 0;
}

void get_dfd_base(void __iomem *dfd_base, unsigned int latch_offset)
{
	LATCH_CTL2_OFFSET = latch_offset;

	if (!DBGTOP_BASE)
		pr_info("link RGU base failed.\n");
	pr_info("Linked base: 0x%x\n", readl(IOMEM(DBGTOP_BASE)));
}
EXPORT_SYMBOL(get_dfd_base);

int mtk_dbgtop_dfd_count_en(int value)
{
	unsigned int tmp;

	/* dfd_count_en is obsolete, enable dfd_en only here */

	if (value == 1) {
		/* enable dfd_en */
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp |= (MTK_DBGTOP_DFD_EN | MTK_DBGTOP_LATCH_CTL2_KEY);
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	} else if (value == 0) {
		/* disable dfd_en */
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp &= ~MTK_DBGTOP_DFD_EN;
		tmp |= MTK_DBGTOP_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	}

	pr_info("%s: MTK_DBGTOP_LATCH_CTL2(0x%x)\n", __func__,
		readl(IOMEM(MTK_DBGTOP_LATCH_CTL2)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dfd_count_en);

int mtk_dbgtop_dfd_therm1_dis(int value)
{
	unsigned int tmp;

	if (value == 1) {
		/* enable dfd count */
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp |= MTK_DBGTOP_DFD_THERM1_DIS | MTK_DBGTOP_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	} else if (value == 0) {
		/* disable dfd count */
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp &= ~MTK_DBGTOP_DFD_THERM1_DIS;
		tmp |= MTK_DBGTOP_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	}

	pr_info("%s: MTK_DBGTOP_LATCH_CTL2(0x%x)\n", __func__,
		readl(IOMEM(MTK_DBGTOP_LATCH_CTL2)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dfd_therm1_dis);

int mtk_dbgtop_dfd_therm2_dis(int value)
{
	unsigned int tmp;

	if (value == 1) {
		/* enable dfd count */
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp |= MTK_DBGTOP_DFD_THERM2_DIS | MTK_DBGTOP_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	} else if (value == 0) {
		tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));
		tmp &= ~MTK_DBGTOP_DFD_THERM2_DIS;
		tmp |= MTK_DBGTOP_LATCH_CTL2_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);
	}

	pr_info("%s: MTK_DBGTOP_LATCH_CTL2(0x%x)\n", __func__,
		readl(IOMEM(MTK_DBGTOP_LATCH_CTL2)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dfd_therm2_dis);

int mtk_dbgtop_dfd_timeout(int value)
{
	unsigned int tmp;

	value <<= MTK_DBGTOP_DFD_TIMEOUT_SHIFT;
	value &= MTK_DBGTOP_DFD_TIMEOUT_MASK;
	/* break if dfd timeout >= target value */
	tmp = readl(IOMEM(MTK_DBGTOP_LATCH_CTL2));

	/* set dfd timeout */
	tmp &= ~MTK_DBGTOP_DFD_TIMEOUT_MASK;
	tmp |= value | MTK_DBGTOP_LATCH_CTL2_KEY;
	mt_reg_sync_writel(tmp, MTK_DBGTOP_LATCH_CTL2);

	pr_info("%s: MTK_DBGTOP_LATCH_CTL2(0x%x)\n", __func__,
		readl(IOMEM(MTK_DBGTOP_LATCH_CTL2)));

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_dfd_timeout);

int mtk_dbgtop_mfg_pwr_on(int value)
{
	unsigned int tmp;

	if (!DBGTOP_BASE)
		return -1;

	if (value == 1) {
		/* set mfg pwr on */
		tmp = readl(IOMEM(MTK_DBGTOP_MFG_REG));
		tmp |= MTK_DBGTOP_MFG_PWR_ON;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MFG_REG);
	} else if (value == 0) {
		tmp = readl(IOMEM(MTK_DBGTOP_MFG_REG));
		tmp &= ~MTK_DBGTOP_MFG_PWR_ON;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MFG_REG);
	} else
		return -1;

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_mfg_pwr_on);

int mtk_dbgtop_mfg_pwr_en(int value)
{
	unsigned int tmp;

	if (!DBGTOP_BASE)
		return -1;

	if (value == 1) {
		/* set mfg pwr en */
		tmp = readl(IOMEM(MTK_DBGTOP_MFG_REG));
		tmp |= MTK_DBGTOP_MFG_PWR_EN;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MFG_REG);
	} else if (value == 0) {
		tmp = readl(IOMEM(MTK_DBGTOP_MFG_REG));
		tmp &= ~MTK_DBGTOP_MFG_PWR_EN;
		tmp |= MTK_DBGTOP_MFG_REG_KEY;
		mt_reg_sync_writel(tmp, MTK_DBGTOP_MFG_REG);
	} else
		return -1;

	return 0;
}
EXPORT_SYMBOL(mtk_dbgtop_mfg_pwr_en);

core_initcall(mtk_dbgtop_get_base_addr);
module_init(mtk_dbgtop_init);
module_exit(mtk_dbgtop_exit);

