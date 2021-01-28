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
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/sched_clock.h>
#include <linux/slab.h>
#include "timer-of.h"

#define TIMER_CLK_EVT           (1)
#define TIMER_CLK_SRC           (2)

#define TIMER_SYNC_TICKS        (3)

/* gpt */
#define GPT_IRQ_EN_REG          0x00
#define GPT_IRQ_ENABLE(val)     BIT((val) - 1)
#define GPT_IRQ_ACK_REG	        0x08
#define GPT_IRQ_ACK(val)        BIT((val) - 1)

#define GPT_CTRL_REG(val)       (0x10 * (val))
#define GPT_CTRL_OP(val)        (((val) & 0x3) << 4)
#define GPT_CTRL_OP_ONESHOT     (0)
#define GPT_CTRL_OP_REPEAT      (1)
#define GPT_CTRL_OP_FREERUN     (3)
#define GPT_CTRL_CLEAR          (2)
#define GPT_CTRL_ENABLE         (1)
#define GPT_CTRL_DISABLE        (0)

#define GPT_CLK_REG(val)        (0x04 + (0x10 * (val)))
#define GPT_CLK_SRC(val)        (((val) & 0x1) << 4)
#define GPT_CLK_SRC_SYS13M      (0)
#define GPT_CLK_SRC_RTC32K      (1)
#define GPT_CLK_DIV1            (0x0)
#define GPT_CLK_DIV2            (0x1)

#define GPT_CNT_REG(val)        (0x08 + (0x10 * (val)))
#define GPT_CMP_REG(val)        (0x0C + (0x10 * (val)))

/* system timer */
#define SYST_BASE               (0x40)

#define SYST_CON                (SYST_BASE + 0x0)
#define SYST_VAL                (SYST_BASE + 0x4)

#define SYST_CON_REG(to)        (timer_of_base(to) + SYST_CON)
#define SYST_VAL_REG(to)        (timer_of_base(to) + SYST_VAL)

/*
 * SYST_CON_EN: Clock enable. Shall be set to
 *   - Start timer countdown.
 *   - Allow timeout ticks being updated.
 *   - Allow changing interrupt functions.
 *
 * SYST_CON_IRQ_EN: Set to allow interrupt.
 *
 * SYST_CON_IRQ_CLR: Set to clear interrupt.
 */
#define SYST_CON_EN              BIT(0)
#define SYST_CON_IRQ_EN          BIT(1)
#define SYST_CON_IRQ_CLR         BIT(4)

#ifdef CONFIG_MTK_AEE_IPANIC
#define SYST_DEBUG
#endif

#ifdef SYST_DEBUG
#include <linux/sched/clock.h>
#include <mt-plat/mboot_params.h>

static char         dbg_buf[128];
static unsigned int dbg_setevt_cpu;
static uint64_t     t_setevt_in;
static uint64_t     t_hdl_in;
static uint64_t     t_setevt_ticks;

#define aee_log(fmt, ...) \
do { \
	memset(dbg_buf, 0, sizeof(dbg_buf)); \
	if (snprintf(dbg_buf, sizeof(dbg_buf), fmt, ##__VA_ARGS__) > 0) \
		aee_sram_fiq_log(dbg_buf); \
} while (0)
#endif

static void __iomem *gpt_sched_reg __read_mostly;

static void mtk_syst_ack_irq(struct timer_of *to)
{
	/* Clear and disable interrupt */
	writel(SYST_CON_IRQ_CLR | SYST_CON_EN, SYST_CON_REG(to));
}

static irqreturn_t mtk_syst_handler(int irq, void *dev_id)
{
	struct clock_event_device *clkevt = dev_id;
	struct timer_of *to = to_timer_of(clkevt);

#ifdef SYST_DEBUG
	t_hdl_in = sched_clock();
#endif
	mtk_syst_ack_irq(to);
	clkevt->event_handler(clkevt);

	return IRQ_HANDLED;
}

static int mtk_syst_clkevt_next_event(unsigned long ticks,
				      struct clock_event_device *clkevt)
{
	struct timer_of *to = to_timer_of(clkevt);

#ifdef SYST_DEBUG
	t_setevt_in = sched_clock();
	t_setevt_ticks = ticks;
	dbg_setevt_cpu = smp_processor_id();
#endif
	/* Enable clock to allow timeout tick update later */
	writel(SYST_CON_EN, SYST_CON_REG(to));

	/*
	 * Write new timeout ticks. Timer shall start countdown
	 * after timeout ticks are updated.
	 */
	writel(ticks, SYST_VAL_REG(to));

	/* Enable interrupt */
	writel(SYST_CON_EN | SYST_CON_IRQ_EN, SYST_CON_REG(to));

	return 0;
}

static int mtk_syst_clkevt_shutdown(struct clock_event_device *clkevt)
{
	/* Disable timer */
	writel(0, SYST_CON_REG(to_timer_of(clkevt)));

	return 0;
}

static int mtk_syst_clkevt_resume(struct clock_event_device *clkevt)
{
	return mtk_syst_clkevt_shutdown(clkevt);
}

static int mtk_syst_clkevt_oneshot(struct clock_event_device *clkevt)
{
	return 0;
}

static u64 notrace mtk_gpt_read_sched_clock(void)
{
	return readl_relaxed(gpt_sched_reg);
}

static void mtk_gpt_clkevt_time_stop(struct timer_of *to, u8 timer)
{
	u32 val;

	/*
	 * support 32k clock when deepidle, should first use 13m clock config
	 * timer, then second use 32k clock trigger timer.
	 */
	if (to->private_data)
		writel(GPT_CLK_SRC(GPT_CLK_SRC_SYS13M) | GPT_CLK_DIV1,
				timer_of_base(to) + GPT_CLK_REG(timer));

	val = readl(timer_of_base(to) + GPT_CTRL_REG(timer));
	writel(val & ~GPT_CTRL_ENABLE, timer_of_base(to) +
	       GPT_CTRL_REG(timer));
}

static void mtk_gpt_clkevt_time_setup(struct timer_of *to,
				      unsigned long delay, u8 timer)
{
	writel(delay, timer_of_base(to) + GPT_CMP_REG(timer));
}

static void mtk_gpt_clkevt_time_start(struct timer_of *to,
				      bool periodic, u8 timer)
{
	u32 val;

	/* Acknowledge interrupt */
	writel(GPT_IRQ_ACK(timer), timer_of_base(to) + GPT_IRQ_ACK_REG);

	/*
	 * support 32k clock when deepidle, should first use 13m clock config
	 * timer, then second use 32k clock trigger timer.
	 */
	if (to->private_data)
		writel(GPT_CLK_SRC(GPT_CLK_SRC_RTC32K) | GPT_CLK_DIV1,
				timer_of_base(to) + GPT_CLK_REG(timer));

	val = readl(timer_of_base(to) + GPT_CTRL_REG(timer));

	/* Clear 2 bit timer operation mode field */
	val &= ~GPT_CTRL_OP(0x3);

	if (periodic)
		val |= GPT_CTRL_OP(GPT_CTRL_OP_REPEAT);
	else
		val |= GPT_CTRL_OP(GPT_CTRL_OP_ONESHOT);

	writel(val | GPT_CTRL_ENABLE | GPT_CTRL_CLEAR,
	       timer_of_base(to) + GPT_CTRL_REG(timer));
}

static int mtk_gpt_clkevt_shutdown(struct clock_event_device *clk)
{
	mtk_gpt_clkevt_time_stop(to_timer_of(clk), TIMER_CLK_EVT);

	return 0;
}

static int mtk_gpt_clkevt_set_periodic(struct clock_event_device *clk)
{
	struct timer_of *to = to_timer_of(clk);

	mtk_gpt_clkevt_time_stop(to, TIMER_CLK_EVT);
	mtk_gpt_clkevt_time_setup(to, to->of_clk.period, TIMER_CLK_EVT);
	mtk_gpt_clkevt_time_start(to, true, TIMER_CLK_EVT);

	return 0;
}

static int mtk_gpt_clkevt_next_event(unsigned long event,
				     struct clock_event_device *clk)
{
	struct timer_of *to = to_timer_of(clk);

	mtk_gpt_clkevt_time_stop(to, TIMER_CLK_EVT);
	mtk_gpt_clkevt_time_setup(to, event, TIMER_CLK_EVT);
	mtk_gpt_clkevt_time_start(to, false, TIMER_CLK_EVT);

	return 0;
}

static irqreturn_t mtk_gpt_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *clkevt = (struct clock_event_device *)dev_id;
	struct timer_of *to = to_timer_of(clkevt);

	/* Acknowledge timer0 irq */
	writel(GPT_IRQ_ACK(TIMER_CLK_EVT), timer_of_base(to) + GPT_IRQ_ACK_REG);
	clkevt->event_handler(clkevt);

	return IRQ_HANDLED;
}

static void
__init mtk_gpt_setup(struct timer_of *to, u8 timer, u8 option, u8 clksrc,
			bool enable)
{
	u32 val;

	writel(GPT_CTRL_CLEAR | GPT_CTRL_DISABLE,
	       timer_of_base(to) + GPT_CTRL_REG(timer));

	writel(GPT_CLK_SRC(clksrc) | GPT_CLK_DIV1,
	       timer_of_base(to) + GPT_CLK_REG(timer));

	writel(0x0, timer_of_base(to) + GPT_CMP_REG(timer));

	val = GPT_CTRL_OP(option);
	if (enable)
		val |= GPT_CTRL_ENABLE;
	writel(val, timer_of_base(to) + GPT_CTRL_REG(timer));
}

static void mtk_gpt_enable_irq(struct timer_of *to, u8 timer)
{
	u32 val;

	/* Disable all interrupts */
	writel(0x0, timer_of_base(to) + GPT_IRQ_EN_REG);

	/* Acknowledge all spurious pending interrupts */
	writel(0x3f, timer_of_base(to) + GPT_IRQ_ACK_REG);

	val = readl(timer_of_base(to) + GPT_IRQ_EN_REG);
	writel(val | GPT_IRQ_ENABLE(timer),
	       timer_of_base(to) + GPT_IRQ_EN_REG);
}

static struct timer_of to = {
	.flags = TIMER_OF_IRQ | TIMER_OF_BASE | TIMER_OF_CLOCK,

	.clkevt = {
		.name = "mtk-clkevt",
		.rating = 300,
		.cpumask = cpu_possible_mask,
	},

	.of_irq = {
		.flags = IRQF_TIMER | IRQF_IRQPOLL,
	},
};

static int __init mtk_syst_init(struct device_node *node)
{
	int ret;

	to.clkevt.features = CLOCK_EVT_FEAT_DYNIRQ | CLOCK_EVT_FEAT_ONESHOT;
	to.clkevt.set_state_shutdown = mtk_syst_clkevt_shutdown;
	to.clkevt.set_state_oneshot = mtk_syst_clkevt_oneshot;
	to.clkevt.tick_resume = mtk_syst_clkevt_resume;
	to.clkevt.set_next_event = mtk_syst_clkevt_next_event;
	to.of_irq.handler = mtk_syst_handler;

	ret = timer_of_init(node, &to);
	if (ret)
		return ret;

	clockevents_config_and_register(&to.clkevt, timer_of_rate(&to),
					TIMER_SYNC_TICKS, 0xffffffff);

	return 0;
}

static int __init mtk_gpt_init(struct device_node *node)
{
	int ret;
	int has_clk32k;
	struct clk *clk_bus;

	/*
	 * Sometimes, there is a "bus clk" used as GPT clock gate. It must be
	 * guaranteed to be opened so that GPT continues to work.
	 */
	clk_bus = of_clk_get_by_name(node, "bus");
	if (!IS_ERR(clk_bus)) {
		ret = clk_prepare_enable(clk_bus);
		if (ret)
			pr_debug("prepare bus clk fail.\n");
	}

	/*
	 * CLOCK_EVT_FEAT_DYNIRQ: Core shall set the interrupt affinity
	 *                        dynamically in broadcast mode.
	 * CLOCK_EVT_FEAT_ONESHOT: Use one-shot mode for tick broadcast.
	 */
	to.clkevt.features = CLOCK_EVT_FEAT_PERIODIC |
			     CLOCK_EVT_FEAT_ONESHOT |
			     CLOCK_EVT_FEAT_DYNIRQ;
	to.clkevt.set_state_shutdown = mtk_gpt_clkevt_shutdown;
	to.clkevt.set_state_periodic = mtk_gpt_clkevt_set_periodic;
	to.clkevt.set_state_oneshot = mtk_gpt_clkevt_shutdown;
	to.clkevt.tick_resume = mtk_gpt_clkevt_shutdown;
	to.clkevt.set_next_event = mtk_gpt_clkevt_next_event;
	to.of_irq.handler = mtk_gpt_interrupt;

	/* Use rtc-clk as clock source for clk-evt source if it is exists. */
	has_clk32k = of_property_match_string(node, "clock-names", "clk32k");
	if (has_clk32k >= 0)
		to.of_clk.name = "clk32k";

	ret = timer_of_init(node, &to);
	if (ret)
		return ret;

	/* save rtc-clk as .private_data */
	to.private_data = (has_clk32k < 0) ? NULL : (void *)to.of_clk.clk;

	/* Configure clock source */
	mtk_gpt_setup(&to, TIMER_CLK_SRC, GPT_CTRL_OP_FREERUN,
		      (has_clk32k < 0) ? GPT_CLK_SRC_SYS13M :
		      GPT_CLK_SRC_RTC32K, true);
	clocksource_mmio_init(timer_of_base(&to) + GPT_CNT_REG(TIMER_CLK_SRC),
			      node->name, timer_of_rate(&to), 300, 32,
			      clocksource_mmio_readl_up);
	gpt_sched_reg = timer_of_base(&to) + GPT_CNT_REG(TIMER_CLK_SRC);
	sched_clock_register(mtk_gpt_read_sched_clock, 32, timer_of_rate(&to));

	/* Configure clock event */
	mtk_gpt_setup(&to, TIMER_CLK_EVT, GPT_CTRL_OP_REPEAT,
		      (has_clk32k < 0) ? GPT_CLK_SRC_SYS13M :
		      GPT_CLK_SRC_RTC32K, false);
	clockevents_config_and_register(&to.clkevt, timer_of_rate(&to),
					TIMER_SYNC_TICKS, 0xffffffff);

	mtk_gpt_enable_irq(&to, TIMER_CLK_EVT);

	return 0;
}
TIMER_OF_DECLARE(mtk_mt6577, "mediatek,mt6577-timer", mtk_gpt_init);
TIMER_OF_DECLARE(mtk_mt6765, "mediatek,mt6765-timer", mtk_syst_init);

#ifdef SYST_DEBUG
void mtk_timer_clkevt_aee_dump(void)
{
	/* interrupt, clkevt handler and set next time */
	aee_log("[timer-mtk]\n");
	aee_log("int handler entry:    %llu\n", t_hdl_in);
	aee_log("set next event entry: %llu\n", t_setevt_in);
	aee_log("set next event ticks: %llu\n", t_setevt_ticks);
	aee_log("set next event cpu:   %u\n",   dbg_setevt_cpu);
	aee_log("CON: 0x%x\n",  readl(SYST_CON_REG(&to)));
	aee_log("VAL: 0x%x\n",  readl(SYST_VAL_REG(&to)));
}
#endif

