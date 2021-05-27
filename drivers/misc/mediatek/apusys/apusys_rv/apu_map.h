/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_MAP_H_
#define APU_MAP_H_

#include "apu.h"

int apu_memmap_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		pr_info("%s: apu_mbox get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_mbox = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->apu_mbox)) {
		pr_info("%s: apu_mbox remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "md32_sysctrl");
	if (res == NULL) {
		pr_info("%s: md32_sysctrl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->md32_sysctrl = ioremap(
		res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->md32_sysctrl)) {
		pr_info("%s: md32_sysctrl remap base fail\n", __func__);
		return -ENOMEM;
	}

	apu->md32_debug_apb = ioremap(0x0d19c000, 0x1000);
	if (IS_ERR((void const *)apu->md32_debug_apb)) {
		pr_info("%s: md32_debug_apb remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_wdt");
	if (res == NULL) {
		pr_info("%s: apu_wdt get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_wdt = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->apu_wdt)) {
		pr_info("%s: apu_wdt remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "apu_sctrl_reviser");
	if (res == NULL) {
		pr_info("%s: apu_sctrl_reviser get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_sctrl_reviser = ioremap(
		res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->apu_sctrl_reviser)) {
		pr_info("%s: apu_sctrl_reviser remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_ao_ctl");
	if (res == NULL) {
		pr_info("%s: apu_ao_ctl get resource fail\n", __func__);
		return -ENODEV;
	}
	apu->apu_ao_ctl = ioremap(
		res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->apu_ao_ctl)) {
		pr_info("%s: apu_ao_ctl remap base fail\n", __func__);
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "md32_tcm");
	if (res == NULL) {
		pr_info("%s: md32_tcm get resource fail\n", __func__);
		return -ENODEV;
	}
	/* no cache? write combined(bufferable) */
	apu->md32_tcm = ioremap_wc(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu->md32_tcm)) {
		pr_info("%s: md32_tcm remap base fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

void apu_memmap_remove(struct mtk_apu *apu)
{
	iounmap(apu->md32_tcm);
	iounmap(apu->apu_ao_ctl);
	iounmap(apu->apu_wdt);
	iounmap(apu->md32_sysctrl);
	iounmap(apu->md32_debug_apb);
	iounmap(apu->apu_sctrl_reviser);
	iounmap(apu->apu_mbox);
}

#endif /* APU_MAP_H_ */
