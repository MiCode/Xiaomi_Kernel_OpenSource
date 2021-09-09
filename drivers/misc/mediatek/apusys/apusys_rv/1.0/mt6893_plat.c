// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/sched/clock.h>

#include "apusys_power.h"
#include "../apu.h"
#include "../apu_config.h"
#include "../apu_hw.h"

static int mt6893_rproc_init(struct mtk_apu *apu)
{
	return 0;
}

static int mt6893_rproc_exit(struct mtk_apu *apu)
{
	return 0;
}

static int mt6893_rproc_start(struct mtk_apu *apu)
{
	int ns = 1; /* Non Secure */
	int domain = 0;
	int boundary = (u32) upper_32_bits(apu->code_da);
	int boot_from_tcm;

	/*Setup IOMMU */
	iowrite32(boundary, apu->apu_sctrl_reviser + 0x300);
	iowrite32(boundary, apu->apu_sctrl_reviser + 0x304);
	iowrite32((ns << 4) | domain, apu->apu_ao_ctl + 0x10);

	/* reset uP */
	iowrite32(0, apu->md32_sysctrl);
	mdelay(100);
	/* Enable IOMMU only(iommu_tr_en = 1/acp_en = 0) */
	iowrite32(0xEA9, apu->md32_sysctrl);

	if (TCM_OFFSET == 0)
		boot_from_tcm = 1;
	else
		boot_from_tcm = 0;

	/* Set uP boot addr to DRAM.
	 * If boot from tcm == 1, boot addr will always map to
	 * 0x1d700000 no matter what value boot_addr is
	 */
	iowrite32((u32)CODE_BUF_DA|boot_from_tcm, apu->apu_ao_ctl + 0x4);

	/* set predefined MPU region for cache access */
	iowrite32(0xAB, apu->apu_ao_ctl);

	/* Release runstall */
	iowrite32(0x0, apu->apu_ao_ctl + 8);

	return 0;
}

static int mt6893_rproc_stop(struct mtk_apu *apu)
{
	/* Hold runstall */
	iowrite32(0x1, apu->apu_ao_ctl + 8);

	return 0;
}

static int mt6893_apu_power_init(struct mtk_apu *apu)
{
	return 0;
}

static int mt6893_apu_power_on(struct mtk_apu *apu)
{
	return 0;
}

static int mt6893_apu_power_off(struct mtk_apu *apu)
{
	return 0;
}

static int mt6893_apu_memmap_init(struct mtk_apu *apu)
{
	struct platform_device *pdev = apu->pdev;
	struct device *dev = apu->dev;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		dev_info(dev, "%s: apu_mbox get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_mbox = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_mbox)) {
		dev_info(dev, "%s: apu_mbox remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_sysctrl");
	if (res == NULL) {
		dev_info(dev, "%s: md32_sysctrl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_sysctrl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_sysctrl)) {
		dev_info(dev, "%s: md32_sysctrl remap base fail\n", __func__);
		return -ENOMEM;
	}

	apu->md32_debug_apb = devm_ioremap(dev, 0x0d19c000, 0x1000);
	if (IS_ERR((void const *)apu->md32_debug_apb)) {
		dev_info(dev, "%s: md32_debug_apb remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_wdt");
	if (res == NULL) {
		dev_info(dev, "%s: apu_wdt get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_wdt = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_wdt)) {
		dev_info(dev, "%s: apu_wdt remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "apu_sctrl_reviser");
	if (res == NULL) {
		dev_info(dev, "%s: apu_sctrl_reviser get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_sctrl_reviser = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_sctrl_reviser)) {
		dev_info(dev, "%s: apu_sctrl_reviser remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_ao_ctl");
	if (res == NULL) {
		dev_info(dev, "%s: apu_ao_ctl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_ao_ctl = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->apu_ao_ctl)) {
		dev_info(dev, "%s: apu_ao_ctl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_tcm");
	if (res == NULL) {
		dev_info(dev, "%s: md32_tcm get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_tcm = devm_ioremap_wc(dev, res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->md32_tcm)) {
		dev_info(dev, "%s: md32_tcm remap base fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt6893_apu_memmap_remove(struct mtk_apu *apu)
{
}

static void mt6893_rv_cg_gating(struct mtk_apu *apu)
{
}

static void mt6893_rv_cg_ungating(struct mtk_apu *apu)
{
}

static void mt6893_rv_cachedump(struct mtk_apu *apu)
{
}

const struct mtk_apu_platdata mt6893_platdata = {
	.flags		= 0,
	.ops		= {
		.init	= mt6893_rproc_init,
		.exit	= mt6893_rproc_exit,
		.start	= mt6893_rproc_start,
		.stop	= mt6893_rproc_stop,
		.apu_memmap_init = mt6893_apu_memmap_init,
		.apu_memmap_remove = mt6893_apu_memmap_remove,
		.cg_gating = mt6893_rv_cg_gating,
		.cg_ungating = mt6893_rv_cg_ungating,
		.rv_cachedump = mt6893_rv_cachedump,
		.power_init = mt6893_apu_power_init,
		.power_on = mt6893_apu_power_on,
		.power_off = mt6893_apu_power_off,
	},
};
