// SPDX-License-Identifier: GPL-2.0-only
/*
 * Hisilicon SoC reset code
 *
 * Copyright (c) 2014 Hisilicon Ltd.
 * Copyright (c) 2014 Linaro Ltd.
 *
 * Author: Haojian Zhuang <haojian.zhuang@linaro.org>
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <asm/system_misc.h>

#include <asm/proc-fns.h>

static void __iomem *base;
static u32 reboot_offset;
static struct regmap *pmu_regmap;
static struct regmap *sctrl_regmap;

#define REBOOT_REASON_BOOTLOADER (0x01)
#define REBOOT_REASON_COLDBOOT   (0x00)
#define DDR_BYPASS               BIT(31)

#define RST_FLAG_MASK            GENMASK(7, 0)

#define PMU_HRST_OFFSET		((0x101) << 2)
#define SCPEREN1_OFFSET		(0x170)

static int hisi_restart_handler(struct notifier_block *this,
				unsigned long mode, void *cmd)
{
	int ret;
	char reboot_reason;

	if (!cmd || !strcmp(cmd, "bootloader"))
		reboot_reason = REBOOT_REASON_BOOTLOADER;
	else
		reboot_reason = REBOOT_REASON_COLDBOOT;

	if (base) {
		writel_relaxed(0xdeadbeef, base + reboot_offset);
	} else {
		ret = regmap_update_bits(pmu_regmap, PMU_HRST_OFFSET,
					 RST_FLAG_MASK, reboot_reason);
		if (ret)
			return ret;

		ret = regmap_write(sctrl_regmap, SCPEREN1_OFFSET, DDR_BYPASS);
		if (ret)
			return ret;

		ret = regmap_write(sctrl_regmap, reboot_offset, 0xdeadbeef);
		if (ret)
			return ret;
	}

	while (1)
		mdelay(1);

	return NOTIFY_DONE;
}

static struct notifier_block hisi_restart_nb = {
	.notifier_call = hisi_restart_handler,
	.priority = 128,
};

static int hisi_reboot_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int err;

	base = of_iomap(np, 0);
	if (!base) {
		pmu_regmap = syscon_regmap_lookup_by_phandle(np, "pmu-regmap");
		if (!pmu_regmap) {
			WARN(1, "failed to regmap pmu address");
			return -ENODEV;
		}

		sctrl_regmap = syscon_regmap_lookup_by_phandle(np, "sctrl-regmap");
		if (!sctrl_regmap) {
			WARN(1, "failed to regmap sctrl address");
			return -ENODEV;
		}
	}

	if (of_property_read_u32(np, "reboot-offset", &reboot_offset) < 0) {
		pr_err("failed to find reboot-offset property\n");
		iounmap(base);
		return -EINVAL;
	}

	err = register_restart_handler(&hisi_restart_nb);
	if (err) {
		dev_err(&pdev->dev, "cannot register restart handler (err=%d)\n",
			err);
		iounmap(base);
	}

	return err;
}

static const struct of_device_id hisi_reboot_of_match[] = {
	{ .compatible = "hisilicon,sysctrl" },
	{ .compatible = "hisilicon,hi3660-reboot" },
	{}
};

static struct platform_driver hisi_reboot_driver = {
	.probe = hisi_reboot_probe,
	.driver = {
		.name = "hisi-reboot",
		.of_match_table = hisi_reboot_of_match,
	},
};
module_platform_driver(hisi_reboot_driver);

MODULE_DESCRIPTION("Reset driver for HiSi SoCs");
MODULE_LICENSE("GPL v2");
