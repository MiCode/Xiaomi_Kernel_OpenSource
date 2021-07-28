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

static int mt6983_rproc_init(struct mtk_apu *apu)
{
	return 0;
}

static int mt6983_rproc_exit(struct mtk_apu *apu)
{
	return 0;
}

static void apu_setup_reviser(struct mtk_apu *apu, int boundary, int ns, int domain)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		/* todo: add smc call */
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* setup boundary */
		iowrite32(0x4 | boundary,
			apu->apu_sctrl_reviser + USERFW_CTXT);
		iowrite32(0x4 | boundary,
			apu->apu_sctrl_reviser + SECUREFW_CTXT);

		/* setup iommu ctrl(mmu_ctrl | mmu_en) */
		if (apu->platdata->flags & F_VDRAM_BOOT)
			iowrite32(0x2, apu->apu_sctrl_reviser + UP_IOMMU_CTRL);
		else
			iowrite32(0x3, apu->apu_sctrl_reviser + UP_IOMMU_CTRL);

		/* setup ns/domain */
		iowrite32(ns << 4 | domain,
			apu->apu_sctrl_reviser + UP_NORMAL_DOMAIN_NS);
		iowrite32(ns << 4 | domain,
			apu->apu_sctrl_reviser + UP_PRI_DOMAIN_NS);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		dev_info(dev, "%s: UP_NORMAL_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_NORMAL_DOMAIN_NS));
		dev_info(dev, "%s: UP_PRI_DOMAIN_NS = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + UP_PRI_DOMAIN_NS));
		dev_info(dev, "%s: USERFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + USERFW_CTXT));
		dev_info(dev, "%s: SECUREFW_CTXT = 0x%x\n",
			__func__,
			ioread32(apu->apu_sctrl_reviser + SECUREFW_CTXT));

		if (apu->platdata->flags & F_VDRAM_BOOT) {
			spin_lock_irqsave(&apu->reg_lock, flags);
			/* vld=1, partial_enable=1 */
			iowrite32(0x7,
				apu->apu_sctrl_reviser + UP_CORE0_VABASE0);
			/* for 34 bit mva */
			iowrite32(0x1 | (u32) (apu->code_da >> 2),
				apu->apu_sctrl_reviser + UP_CORE0_MVABASE0);

			/* vld=1, partial_enable=1 */
			iowrite32(0x3,
				apu->apu_sctrl_reviser + UP_CORE0_VABASE1);
			/* for 34 bit mva */
			iowrite32(0x1 | (u32) (apu->code_da >> 2),
				apu->apu_sctrl_reviser + UP_CORE0_MVABASE1);
			spin_unlock_irqrestore(&apu->reg_lock, flags);

			dev_info(dev, "%s: UP_CORE0_VABASE0 = 0x%x\n",
				__func__,
				ioread32(apu->apu_sctrl_reviser + UP_CORE0_VABASE0));
			dev_info(dev, "%s: UP_CORE0_MVABASE0 = 0x%x\n",
				__func__,
				ioread32(apu->apu_sctrl_reviser + UP_CORE0_MVABASE0));
			dev_info(dev, "%s: UP_CORE0_VABASE1 = 0x%x\n",
				__func__,
				ioread32(apu->apu_sctrl_reviser + UP_CORE0_VABASE1));
			dev_info(dev, "%s: UP_CORE0_MVABASE1 = 0x%x\n",
				__func__,
				ioread32(apu->apu_sctrl_reviser + UP_CORE0_MVABASE1));
		}
	}
}

static void apu_reset_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		/* todo: add smc call */
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* reset uP */
		iowrite32(0, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		mdelay(100);
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32_g2b_cg_en | md32_dbg_en | md32_soft_rstn */
		iowrite32(0xc01, apu->md32_sysctrl + MD32_SYS_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		dev_info(dev, "%s: MD32_SYS_CTRL = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_SYS_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* md32 clk enable */
		iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
		/* set up_wake_host_mask0 for wdt/mbox irq */
		iowrite32(0x1c0001, apu->md32_sysctrl + UP_WAKE_HOST_MASK0);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		dev_info(dev, "%s: MD32_CLK_EN = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + MD32_CLK_EN));
		dev_info(dev, "%s: UP_WAKE_HOST_MASK0 = 0x%x\n",
			__func__, ioread32(apu->md32_sysctrl + UP_WAKE_HOST_MASK0));
	}
}

static void apu_setup_boot(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned long flags;
	int boot_from_tcm;

	if (TCM_OFFSET == 0)
		boot_from_tcm = 1;
	else
		boot_from_tcm = 0;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		/* todo: add smc call */
	} else {
		/* Set uP boot addr to DRAM.
		 * If boot from tcm == 1, boot addr will always map to
		 * 0x1d000000 no matter what value boot_addr is
		 */
		spin_lock_irqsave(&apu->reg_lock, flags);
		if (apu->platdata->flags & F_VDRAM_BOOT)
			iowrite32((u32)apu->code_da,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		else
			iowrite32((u32)CODE_BUF_DA | boot_from_tcm,
				apu->apu_ao_ctl + MD32_BOOT_CTRL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		dev_info(dev, "%s: MD32_BOOT_CTRL = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_BOOT_CTRL));

		spin_lock_irqsave(&apu->reg_lock, flags);
		/* set predefined MPU region for cache access */
		iowrite32(0xAB, apu->apu_ao_ctl + MD32_PRE_DEFINE);
		spin_unlock_irqrestore(&apu->reg_lock, flags);
		dev_info(dev, "%s: MD32_PRE_DEFINE = 0x%x\n",
			__func__, ioread32(apu->apu_ao_ctl + MD32_PRE_DEFINE));
	}
}

static void apu_start_mp(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	int i;
	unsigned long flags;

	if (apu->platdata->flags & F_SECURE_BOOT) {
		/* todo: add smc call */
	} else {
		spin_lock_irqsave(&apu->reg_lock, flags);
		/* Release runstall */
		iowrite32(0x0, apu->apu_ao_ctl + MD32_RUNSTALL);
		spin_unlock_irqrestore(&apu->reg_lock, flags);

		usleep_range(0, 1000);
		for (i = 0; i < 20; i++) {
			dev_info(dev, "apu boot: pc=%08x, sp=%08x\n",
			ioread32(apu->md32_sysctrl + 0x838),
					ioread32(apu->md32_sysctrl+0x840));
			usleep_range(0, 1000);
		}
	}
}

static int mt6983_rproc_start(struct mtk_apu *apu)
{
	int ns = 1; /* Non Secure */
	int domain = 0;
	int boundary = (u32) upper_32_bits(apu->code_da);

	apu_setup_reviser(apu, boundary, ns, domain);

	apu_reset_mp(apu);

	apu_setup_boot(apu);

	apu_start_mp(apu);

	return 0;
}

static int mt6983_rproc_stop(struct mtk_apu *apu)
{
	/* Hold runstall */
	iowrite32(0x1, apu->apu_ao_ctl + 8);

	return 0;
}

static int mt6983_apu_memmap_init(struct mtk_apu *apu)
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

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_debug_apb");
	if (res == NULL) {
		dev_info(dev, "%s: md32_debug_apb get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_debug_apb = devm_ioremap_resource(dev, res);
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_cache_dump");
	if (res == NULL) {
		dev_info(dev, "%s: md32_cache_dump get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_cache_dump = devm_ioremap_resource(dev, res);
	if (IS_ERR((void const *)apu->md32_cache_dump)) {
		dev_info(dev, "%s: md32_cache_dump remap base fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void mt6983_apu_memmap_remove(struct mtk_apu *apu)
{
}

static void mt6983_rv_cg_gating(struct mtk_apu *apu)
{
	iowrite32(0x0, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6983_rv_cg_ungating(struct mtk_apu *apu)
{
	iowrite32(0x1, apu->md32_sysctrl + MD32_CLK_EN);
}

static void mt6983_rv_cachedump(struct mtk_apu *apu)
{
	int offset;
	unsigned long flags;

	struct apu_coredump *coredump =
		(struct apu_coredump *) apu->coredump_buf;

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* set APU_UP_SYS_DBG_EN for cache dump enable through normal APB */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) |
		APU_UP_SYS_DBG_EN, apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);

	for (offset = 0; offset < CACHE_DUMP_SIZE/sizeof(uint32_t); offset++)
		coredump->cachedump[offset] =
			ioread32(apu->md32_cache_dump + offset*sizeof(uint32_t));

	spin_lock_irqsave(&apu->reg_lock, flags);
	/* clear APU_UP_SYS_DBG_EN */
	iowrite32(ioread32(apu->md32_sysctrl + DBG_BUS_SEL) &
		~(APU_UP_SYS_DBG_EN), apu->md32_sysctrl + DBG_BUS_SEL);
	spin_unlock_irqrestore(&apu->reg_lock, flags);
}

const struct mtk_apu_platdata mt6983_platdata = {
	.flags		= 0,
	.ops		= {
		.init	= mt6983_rproc_init,
		.exit	= mt6983_rproc_exit,
		.start	= mt6983_rproc_start,
		.stop	= mt6983_rproc_stop,
		.apu_memmap_init = mt6983_apu_memmap_init,
		.apu_memmap_remove = mt6983_apu_memmap_remove,
		.cg_gating = mt6983_rv_cg_gating,
		.cg_ungating = mt6983_rv_cg_ungating,
		.rv_cachedump = mt6983_rv_cachedump,
	},
};
