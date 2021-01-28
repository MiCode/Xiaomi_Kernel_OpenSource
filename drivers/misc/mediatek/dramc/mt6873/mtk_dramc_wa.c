/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <memory/mediatek/dramc.h>
#include <linux/bug.h>
#include <mt-plat/sync_write.h>
#include "mtk_dramc_wa.h"

#define APXGPT_OF_COMPTIBLE_NAME "mediatek,apxgpt"
static struct mt_gpt_timers gpt_timers;

/*
 * dramc_apxgpt3_irq_handler - DRAM 4266 S0,S1 WA:
 *
 * when dram 4266 enter s0 or s1, use apxgpt3 to
 * wakeup the system, the interval is 1S
 */
static irqreturn_t dramc_apxgpt3_irq_handler(int irq, void *dev_id)
{
	/* pr_info("%s\n", __func__); */

	/* 1. Stop and clear GPT  */
	mt_reg_sync_writel(GPT3_STOP_CLEAR, GPT3_CON);

	/* 2. Clear pending irq */
	mt_reg_sync_writel(GPT3_IRQ_BIT, GPT_IRQ_ACK);

	/* 3. Configure GPT divider to 1 and using 32K clock source */
	mt_reg_sync_writel(GPT3_RTC_CLK, GPT3_CLK);

	/* 4. Calculate and Set compare value  */
	mt_reg_sync_writel(32768*1000/1000, GPT3_CMP_L);

	/* 5. Set GTP3 SPE      */
	mt_reg_sync_writel(GPT3_IRQ_SPE, GPT3_SPE);

	/* 6. Enabel IRQ En       */
	mt_reg_sync_writel(GPT3_IRQ_BIT, GPT_IRQ_EN);

	return IRQ_HANDLED;
}

static int dramc_apxgpt3_init(void)
{
	struct device_node *node;
	int ret = 0;
	const struct of_device_id apxgpt_of_match[] = {
		{ .compatible = APXGPT_OF_COMPTIBLE_NAME, },
	};

	node = of_find_compatible_node(NULL, NULL,
		apxgpt_of_match[0].compatible);

	if (!node) {
		pr_info("%s: node not found\n", __func__);
		return -1;
	}

	/* Setup IRQ numbers */
	gpt_timers.tmr_irq = irq_of_parse_and_map(node, 0);

	/* Setup IO addresses */
	gpt_timers.tmr_regs = of_iomap(node, 0);

	pr_info("%s: base=0x%lx, irq=%d\n", __func__,
		(unsigned long)gpt_timers.tmr_regs, gpt_timers.tmr_irq);

	/* 1. Stop and clear GPT */
	mt_reg_sync_writel(GPT3_STOP_CLEAR, GPT3_CON);

	/* 2. Clear pending irq  */
	mt_reg_sync_writel(GPT3_IRQ_BIT, GPT_IRQ_ACK);

	/* 3. Configure GPT divider to 1 and using 32K clock source  */
	mt_reg_sync_writel(GPT3_RTC_CLK, GPT3_CLK);

	/* 4. Calculate and Set compare value  */
	mt_reg_sync_writel(32768*1000/1000, GPT3_CMP_L);

	/* 5. Set GPT3 SPE  */
	mt_reg_sync_writel(GPT3_IRQ_SPE, GPT3_SPE);

	/* 6. Enabel IRQ En  */
	mt_reg_sync_writel(GPT3_IRQ_BIT, GPT_IRQ_EN);

	ret = request_irq(gpt_timers.tmr_irq, dramc_apxgpt3_irq_handler,
			IRQF_TIMER | IRQF_IRQPOLL | IRQF_PERCPU,
				"dramc_apxgpt3", NULL);
	if (ret) {
		pr_info("%s: apxgpt3 request irq Failed ret =%d\n",
			__func__, ret);
		return ret;
	}

	/* pr_info("%s: enable apxgpt3 one-shot", __func__);  */
	/* 7. Start GPT one-shot  */
	/* mt_reg_sync_writel(GPT3_ONE_SHOT_EN, GPT3_CON);  */
	return 0;
}

late_initcall(dramc_apxgpt3_init);
MODULE_DESCRIPTION("MediaTek DRAMC WA Driver v0.1");
