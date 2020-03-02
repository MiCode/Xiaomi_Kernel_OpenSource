/*
 * Mediatek SoCs General-Purpose Timer handling.
 *
 * Copyright (C) 2014 Matthias Brugger
 *
 * Matthias Brugger <matthias.bgg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>
#include <linux/slab.h>

#define CONFIG_MTK_TIMER_AEE_DUMP

#ifdef CONFIG_MTK_TIMER_AEE_DUMP
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>

static char gpt_clkevt_aee_dump_buf[128];
#endif
#endif

#define GPT_IRQ_EN_REG		0x00
#define GPT_IRQ_STA_REG         0x04

#define GPT_IRQ_ENABLE(val)	BIT((val) - 1)
#define GPT_IRQ_ACK_REG		0x08
#define GPT_IRQ_ACK(val)	BIT((val) - 1)

#define TIMER_CTRL_REG(val)	(0x10 * (val))
#define TIMER_CTRL_OP(val)	(((val) & 0x3) << 4)
#define TIMER_CTRL_OP_ONESHOT	(0)
#define TIMER_CTRL_OP_REPEAT	(1)
#define TIMER_CTRL_OP_FREERUN	(3)
#define TIMER_CTRL_CLEAR	(2)
#define TIMER_CTRL_ENABLE	(1)
#define TIMER_CTRL_DISABLE	(0)

#define TIMER_CLK_REG(val)	(0x04 + (0x10 * (val)))
#define TIMER_CLK_SRC(val)	(((val) & 0x1) << 4)
#define TIMER_CLK_SRC_SYS13M	(0)
#define TIMER_CLK_SRC_RTC32K	(1)
#define TIMER_CLK_DIV1		(0x0)
#define TIMER_CLK_DIV2		(0x1)

#define TIMER_CNT_REG(val)	(0x08 + (0x10 * (val)))
#define TIMER_CMP_REG(val)	(0x0C + (0x10 * (val)))

#define GPT_CLK_EVT	1
#define GPT_CLK_SRC	2

struct mtk_clock_event_device {
	void __iomem *gpt_base;
	u32 ticks_per_jiffy;
	bool clk32k_exist;
	struct clock_event_device dev;
};
static struct mtk_clock_event_device *gpt_devs;

static inline struct mtk_clock_event_device *to_mtk_clk(
				struct clock_event_device *c)
{
	return container_of(c, struct mtk_clock_event_device, dev);
}
#if defined(CONFIG_MTK_TIMER_AEE_DUMP)
static uint64_t gpt_clkevt_last_interrupt_time;
static uint64_t gpt_clkevt_last_setting_next_event_time;
#endif

void mt_gpt_clkevt_aee_dump(void)
{
#if defined(CONFIG_MTK_RAM_CONSOLE) && defined(CONFIG_MTK_TIMER_AEE_DUMP)

	/*
	 * Notice: printk cannot be used during AEE flow to avoid lock issues.
	 */
	struct clock_event_device dev = gpt_devs->dev;
	/* last interrupt time */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] last interrupt time: %llu\n",
		gpt_clkevt_last_interrupt_time);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* last time of setting next event */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] last setting next event time: %llu\n",
		gpt_clkevt_last_setting_next_event_time);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* global gpt status */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] IRQEN: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + GPT_IRQ_EN_REG));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] IRQSTA: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + GPT_IRQ_STA_REG));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* gpt1 status */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CON: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + TIMER_CTRL_REG(GPT_CLK_EVT)));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CLK: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + TIMER_CLK_REG(GPT_CLK_EVT)));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CNT: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + TIMER_CNT_REG(GPT_CLK_EVT)));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CMP: 0x%x\n",
		__raw_readl(gpt_devs->gpt_base + TIMER_CMP_REG(GPT_CLK_EVT)));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] irq affinity: %d\n", dev.irq_affinity_on);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/*
	 * TODO: dump apxgpt irq status
	 *
	 * Since printk cannot be used during AEE flow, we may need to
	 * change printk way in mt_irq_dump_status().
	 */

	/* mt_irq_dump_status(xgpt_timers.tmr_irq); */

#endif
}

static void mtk_clkevt_time_stop(struct mtk_clock_event_device *evt,
				u8 timer)
{
	u32 val;

	/*
	 * support 32k clock when deepidle, should first use 13m clock config
	 * timer, then second use 32k clock trigger timer.
	 */
	if (evt->clk32k_exist)
		writel(TIMER_CLK_SRC(TIMER_CLK_SRC_SYS13M) | TIMER_CLK_DIV1,
				evt->gpt_base + TIMER_CLK_REG(timer));

	val = readl(evt->gpt_base + TIMER_CTRL_REG(timer));
	writel(val & ~TIMER_CTRL_ENABLE, evt->gpt_base +
			TIMER_CTRL_REG(timer));
}

static void mtk_clkevt_time_setup(struct mtk_clock_event_device *evt,
				unsigned long delay, u8 timer)
{
	writel(delay, evt->gpt_base + TIMER_CMP_REG(timer));
}

static void mtk_clkevt_time_start(struct mtk_clock_event_device *evt,
		bool periodic, u8 timer)
{
	u32 val;

	/* Acknowledge interrupt */
	writel(GPT_IRQ_ACK(timer), evt->gpt_base + GPT_IRQ_ACK_REG);

	/*
	 * support 32k clock when deepidle, should first use 13m clock config
	 * timer, then second use 32k clock trigger timer.
	 */
	if (evt->clk32k_exist)
		writel(TIMER_CLK_SRC(TIMER_CLK_SRC_RTC32K) | TIMER_CLK_DIV1,
				evt->gpt_base + TIMER_CLK_REG(timer));

	val = readl(evt->gpt_base + TIMER_CTRL_REG(timer));

	/* Clear 2 bit timer operation mode field */
	val &= ~TIMER_CTRL_OP(0x3);

	if (periodic)
		val |= TIMER_CTRL_OP(TIMER_CTRL_OP_REPEAT);
	else
		val |= TIMER_CTRL_OP(TIMER_CTRL_OP_ONESHOT);

	writel(val | TIMER_CTRL_ENABLE | TIMER_CTRL_CLEAR,
	       evt->gpt_base + TIMER_CTRL_REG(timer));
}

static int mtk_clkevt_shutdown(struct clock_event_device *clk)
{
	mtk_clkevt_time_stop(to_mtk_clk(clk), GPT_CLK_EVT);
	return 0;
}

static int mtk_clkevt_set_periodic(struct clock_event_device *clk)
{
	struct mtk_clock_event_device *evt = to_mtk_clk(clk);

	mtk_clkevt_time_stop(evt, GPT_CLK_EVT);
	mtk_clkevt_time_setup(evt, evt->ticks_per_jiffy, GPT_CLK_EVT);
	mtk_clkevt_time_start(evt, true, GPT_CLK_EVT);
	return 0;
}

static int mtk_clkevt_next_event(unsigned long event,
				   struct clock_event_device *clk)
{
	struct mtk_clock_event_device *evt = to_mtk_clk(clk);

	mtk_clkevt_time_stop(evt, GPT_CLK_EVT);
	mtk_clkevt_time_setup(evt, event, GPT_CLK_EVT);
	mtk_clkevt_time_start(evt, false, GPT_CLK_EVT);
#if defined(CONFIG_MTK_TIMER_AEE_DUMP)
	gpt_clkevt_last_setting_next_event_time = sched_clock();
#endif
	return 0;
}

static irqreturn_t mtk_timer_interrupt(int irq, void *dev_id)
{
	struct mtk_clock_event_device *evt = dev_id;
#if defined(CONFIG_MTK_TIMER_AEE_DUMP)
	gpt_clkevt_last_interrupt_time = sched_clock();
#endif

	/* Acknowledge timer0 irq */
	writel(GPT_IRQ_ACK(GPT_CLK_EVT), evt->gpt_base + GPT_IRQ_ACK_REG);
	evt->dev.event_handler(&evt->dev);

	return IRQ_HANDLED;
}


static void mtk_timer_setup(struct mtk_clock_event_device *evt, u8 timer,
			    u8 option, u8 clk_src, bool enable)
{
	u32 val;

	writel(TIMER_CTRL_CLEAR | TIMER_CTRL_DISABLE,
		evt->gpt_base + TIMER_CTRL_REG(timer));

	writel(TIMER_CLK_SRC(clk_src) | TIMER_CLK_DIV1,
			evt->gpt_base + TIMER_CLK_REG(timer));

	writel(0x0, evt->gpt_base + TIMER_CMP_REG(timer));

	val = TIMER_CTRL_OP(option);
	if (enable)
		val |= TIMER_CTRL_ENABLE;
	writel(val, evt->gpt_base + TIMER_CTRL_REG(timer));
}

static void mtk_timer_enable_irq(struct mtk_clock_event_device *evt,
				u8 timer)
{
	u32 val;

	/* Disable all interrupts */
	writel(0x0, evt->gpt_base + GPT_IRQ_EN_REG);

	/* Acknowledge all spurious pending interrupts */
	writel(0x3f, evt->gpt_base + GPT_IRQ_ACK_REG);

	val = readl(evt->gpt_base + GPT_IRQ_EN_REG);
	writel(val | GPT_IRQ_ENABLE(timer),
			evt->gpt_base + GPT_IRQ_EN_REG);
}

static int __init mtk_timer_init(struct device_node *node)
{
	struct mtk_clock_event_device *evt;
	struct resource res;
	unsigned long rate_src = 0, rate_evt = 0;
	struct clk *clk_src, *clk_evt, *clk_bus;

	evt = kzalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return -ENOMEM;

	gpt_devs = evt;

	evt->clk32k_exist = false;
	evt->dev.name = "mtk_tick";
	evt->dev.rating = 300;
	/*
	 * CLOCK_EVT_FEAT_DYNIRQ: Core shall set the interrupt affinity
	 *                        dynamically in broadcast mode.
	 * CLOCK_EVT_FEAT_ONESHOT: Use one-shot mode for tick broadcast.
	 */
	evt->dev.features = CLOCK_EVT_FEAT_PERIODIC |
			    CLOCK_EVT_FEAT_ONESHOT |
			    CLOCK_EVT_FEAT_DYNIRQ;
	evt->dev.set_state_shutdown = mtk_clkevt_shutdown;
	evt->dev.set_state_periodic = mtk_clkevt_set_periodic;
	evt->dev.set_state_oneshot = mtk_clkevt_shutdown;
	evt->dev.tick_resume = mtk_clkevt_shutdown;
	evt->dev.set_next_event = mtk_clkevt_next_event;
	evt->dev.cpumask = cpu_possible_mask;

	evt->gpt_base = of_io_request_and_map(node, 0, "mtk-timer");
	if (IS_ERR(evt->gpt_base)) {
		pr_err("Can't get resource\n");
		goto err_kzalloc;
	}

	evt->dev.irq = irq_of_parse_and_map(node, 0);
	if (evt->dev.irq <= 0) {
		pr_err("Can't parse IRQ\n");
		goto err_mem;
	}

	clk_bus = of_clk_get_by_name(node, "bus");
	if (!IS_ERR(clk_bus)) {
		if (clk_prepare_enable(clk_bus)) {
			pr_err("Can't prepare clk bus\n");
			goto err_clk_bus;
		}
	}
	clk_src = of_clk_get(node, 0);
	if (IS_ERR(clk_src)) {
		pr_err("Can't get timer clock\n");
		goto err_irq;
	}

	if (clk_prepare_enable(clk_src)) {
		pr_err("Can't prepare clock\n");
		goto err_clk_put_src;
	}
	rate_src = clk_get_rate(clk_src);

	clk_evt = of_clk_get_by_name(node, "clk32k");
	if (!IS_ERR(clk_evt)) {
		evt->clk32k_exist = true;
		if (clk_prepare_enable(clk_evt)) {
			pr_err("Can't prepare clk32k\n");
			goto err_clk_evt;
		}
		rate_evt = clk_get_rate(clk_evt);
	} else {
		rate_evt = rate_src;
	}

	if (request_irq(evt->dev.irq, mtk_timer_interrupt,
			IRQF_TIMER | IRQF_IRQPOLL, "mtk_timer", evt)) {
		pr_err("failed to setup irq %d\n", evt->dev.irq);
		if (evt->clk32k_exist)
			goto err_clk_disable_evt;
		else
			goto err_clk_disable_src;
	}

	evt->ticks_per_jiffy = DIV_ROUND_UP(rate_evt, HZ);

	/* Configure clock source */
	mtk_timer_setup(evt, GPT_CLK_SRC, TIMER_CTRL_OP_FREERUN,
			TIMER_CLK_SRC_SYS13M, true);
	clocksource_mmio_init(evt->gpt_base + TIMER_CNT_REG(GPT_CLK_SRC),
			node->name, rate_src, 300, 32,
			clocksource_mmio_readl_up);

	/* Configure clock event as tick broadcast device */
	if (evt->clk32k_exist)
		mtk_timer_setup(evt, GPT_CLK_EVT, TIMER_CTRL_OP_REPEAT,
				TIMER_CLK_SRC_RTC32K, false);
	else
		mtk_timer_setup(evt, GPT_CLK_EVT, TIMER_CTRL_OP_REPEAT,
				TIMER_CLK_SRC_SYS13M, false);
	clockevents_config_and_register(&evt->dev, rate_evt, 0x3,
					0xffffffff);

	mtk_timer_enable_irq(evt, GPT_CLK_EVT);

	return 0;

err_clk_disable_evt:
	clk_disable_unprepare(clk_evt);
	clk_put(clk_evt);
err_clk_disable_src:
	clk_disable_unprepare(clk_src);
err_clk_evt:
	clk_put(clk_evt);
err_clk_put_src:
	clk_put(clk_src);
err_irq:
	irq_dispose_mapping(evt->dev.irq);
err_clk_bus:
	clk_put(clk_bus);
err_mem:
	iounmap(evt->gpt_base);
	if (of_address_to_resource(node, 0, &res)) {
		pr_info("Failed to parse resource\n");
		goto err_kzalloc;
	}
	release_mem_region(res.start, resource_size(&res));
err_kzalloc:
	kfree(evt);

	return -EINVAL;
}
CLOCKSOURCE_OF_DECLARE(mtk_mt6577, "mediatek,mt6577-timer", mtk_timer_init);
CLOCKSOURCE_OF_DECLARE(mtk_mt6758, "mediatek,mt6758-timer", mtk_timer_init);
CLOCKSOURCE_OF_DECLARE(mtk_apxgpt, "mediatek,apxgpt", mtk_timer_init);

