/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>

#include <dbgtop.h>

static void __iomem *DBGTOP_BASE;

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

module_init(mtk_dbgtop_init);
module_exit(mtk_dbgtop_exit);

