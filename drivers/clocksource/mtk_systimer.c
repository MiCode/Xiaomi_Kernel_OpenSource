/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>
#include <linux/spinlock.h>
#include <clocksource/arm_arch_timer.h>
#include <mt-plat/sync_write.h>

#ifdef CONFIG_MTK_TIMER_SYSTIMER

#define MTK_TIMER_DBG_AEE_DUMP

#ifdef MTK_TIMER_DBG_AEE_DUMP
#ifdef CONFIG_MTK_RAM_CONSOLE

#include <linux/irqchip/mtk-gic-extend.h>
#include <mt-plat/mtk_ram_console.h>

static char         dbg_buf[128];
static unsigned int dbg_setevt_cpu;
static uint64_t     t_clkevt_in;
static uint64_t     t_clkevt_out;
static uint64_t     t_setevt_in;
static uint64_t     t_setevt_out;
static uint64_t     t_hdl_in;
static uint64_t     t_hdl_out;
static uint64_t     t_setevt_ticks;

static DEFINE_SPINLOCK(systimer_lock);

#define aee_log(fmt, ...) \
do { \
	memset(dbg_buf, 0, sizeof(dbg_buf)); \
	snprintf(dbg_buf, sizeof(dbg_buf), fmt, ##__VA_ARGS__); \
	aee_sram_fiq_log(dbg_buf); \
} while (0)

#endif
#endif

#define STMR0             (0)
#define NR_STMRS          (1)

#define STMR_CLKEVT_ID    (STMR0)

/* timer bases */
#define STMR_BASE         stmrs.tmr_regs
#define STMR0_BASE        (STMR_BASE + 0x0040)

/* registers */
#define STMR_CON          (0x0)
#define STMR_VAL          (0x4)

/* STMR_CON */
#define STMR_CON_EN       (1 << 0)
#define STMR_CON_IRQ_EN   (1 << 1)
#define STMR_CON_IRQ_CLR  (1 << 4)

/* STMR_VAL */
#define STMR_VAL_MAX      (0xFFFFFFFF)

/* apxgpt */
#define APXGPT_IRQEN      (0x0)
#define APXGPT_IRQACK     (0x8)
#define APXGPT_TMR_START  (0x10)
#define APXGPT_TMR_SIZE   (0x10)
#define APXGPT_TMR_CNT    (6)
#define APXGPT_TMR_MASK   (0x3F)

struct mtk_stmrs {
	int tmr_irq;
	void __iomem *tmr_regs;
};

struct mtk_stmr_device {
	unsigned int id;
	void (*func)(unsigned long data);
	void __iomem *base_addr;
};

static struct mtk_stmrs stmrs;
static struct mtk_stmr_device stmr_devs[NR_STMRS];
static inline void noop(unsigned long data) { }
static void(*stmr_handlers[])(unsigned long) = {
	noop,
};

static DEFINE_SPINLOCK(stmr_spinlock);

static struct mtk_stmr_device *mtk_stmr_id_to_dev(unsigned int id)
{
	return id < NR_STMRS ? stmr_devs + id : NULL;
}

#define mtk_stmr_lock(flags) \
	spin_lock_irqsave(&stmr_spinlock, flags)

#define mtk_stmr_unlock(flags) \
	spin_unlock_irqrestore(&stmr_spinlock, flags)

static void mtk_stmr_ack_irq(struct mtk_stmr_device *dev);
static irqreturn_t mtk_stmr_handler(int irq, void *dev_id);
static int mtk_stmr_clkevt_next_event(unsigned long cycles,
				   struct clock_event_device *evt);
static int mtk_stmr_clkevt_shutdown(struct clock_event_device *clk);
static int mtk_stmr_clkevt_oneshot(struct clock_event_device *clk);
static int mtk_stmr_clkevt_resume(struct clock_event_device *clk);

static struct clock_event_device mtk_stmr_clkevt = {
	.name = "mtk-clkevt",
	/*
	 * CLOCK_EVT_FEAT_DYNIRQ: Core shall set the interrupt affinity
	 *                        dynamically in broadcast mode.
	 * CLOCK_EVT_FEAT_ONESHOT: Use one-shot mode for tick broadcast.
	 */
	.features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_DYNIRQ,
	.shift          = 32,
	.rating         = 300,
	.set_next_event = mtk_stmr_clkevt_next_event,
	.set_state_shutdown = mtk_stmr_clkevt_shutdown,
	.set_state_oneshot = mtk_stmr_clkevt_oneshot,
	.tick_resume = mtk_stmr_clkevt_resume,
};

static struct irqaction mtk_stmr_irq = {
	.name = "mtk-clkevt",
	.flags = IRQF_TIMER | IRQF_IRQPOLL | IRQF_TRIGGER_HIGH | IRQF_PERCPU,
	.handler = mtk_stmr_handler,
	.dev_id = &mtk_stmr_clkevt,
};

void mtk_timer_clkevt_aee_dump(void)
{
#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	/*
	 * Notice: print function cannot be used during AEE
	 * flow to avoid lock issues.
	 */
	int cpu_bound;
	struct mtk_stmr_device *dev =
		mtk_stmr_id_to_dev(STMR_CLKEVT_ID);

	/* interrupt, clkevt handler and set next time */

	aee_log("[timer-mtk]\n");

	aee_log("int handler entry:    %llu\n",
		t_hdl_in);

	aee_log("clkevt handler entry: %llu\n",
		t_clkevt_in);

	aee_log("set next event entry: %llu\n",
		t_setevt_in);

	aee_log("set next event ticks: %llu\n",
		t_setevt_ticks);

	aee_log("set next event exit:  %llu\n",
		t_setevt_out);

	aee_log("clkevt handler exit:  %llu\n",
		t_clkevt_out);

	aee_log("int handler exit:     %llu\n",
		t_hdl_out);

	aee_log("set next event cpu:   %u\n",
		dbg_setevt_cpu);

	aee_log("CON: 0x%x\n",
		__raw_readl(dev->base_addr + STMR_CON));

	aee_log("VAL: 0x%x\n",
		__raw_readl(dev->base_addr + STMR_VAL));

	cpu_bound = mt_irq_dump_cpu(mtk_stmr_clkevt.irq);

	aee_log("irq affinity (bc, gic): %d, %d\n",
		mtk_stmr_clkevt.irq_affinity_on, cpu_bound);
#endif
}

/*
 * mtk_stmr_handler users are listed as below,
 *
 * STMR0: SoC timer for tick-broadcasting (oneshot)
 */
static irqreturn_t mtk_stmr_handler(int irq, void *dev_id)
{
	unsigned int id = STMR_CLKEVT_ID;
	struct mtk_stmr_device *dev;

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_hdl_in = sched_clock();
#endif

	dev = mtk_stmr_id_to_dev(id);

	if (likely(dev)) {
		spin_lock(&systimer_lock);

		mtk_stmr_ack_irq(dev);

		spin_unlock(&systimer_lock);

		if (likely(stmr_handlers[id]))
			stmr_handlers[id]((unsigned long)dev_id);
		else
			pr_info("timer_mtk: no handler for irq %d\n", irq);
	} else
		pr_info("timer_mtk: invalid interrupt %d\n", irq);

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_hdl_out = sched_clock();
#endif

	return IRQ_HANDLED;
}

static void mtk_stmr_reset(struct mtk_stmr_device *dev)
{
	/* Clear IRQ */
	mt_reg_sync_writel(STMR_CON_IRQ_CLR | STMR_CON_EN,
		dev->base_addr + STMR_CON);

	/* Reset counter */
	mt_reg_sync_writel(0, dev->base_addr + STMR_VAL);

	/* Disable timer */
	mt_reg_sync_writel(0, dev->base_addr + STMR_CON);
}

static void mtk_stmr_ack_irq(struct mtk_stmr_device *dev)
{
	mtk_stmr_reset(dev);
}

static void mtk_stmr_set_handler(
	struct mtk_stmr_device *dev, void (*func)(unsigned long))
{
	if (func)
		stmr_handlers[dev->id] = func;

	dev->func = func;
}

static void mtk_stmr_dev_init(void)
{
	int i;

	for (i = 0; i < NR_STMRS; i++) {
		stmr_devs[i].id = i;
		stmr_devs[i].base_addr = STMR0_BASE + 0x08 * i;
		pr_info("stmr%d, base=0x%lx\n",
			i, (unsigned long)stmr_devs[i].base_addr);
	}
}

static void mtk_stmr_dev_setup(struct mtk_stmr_device *dev,
	void (*func)(unsigned long))
{
	if (func)
		mtk_stmr_set_handler(dev, func);
}

static int mtk_stmr_clkevt_next_event(unsigned long ticks,
				   struct clock_event_device *evt)
{
	struct mtk_stmr_device *dev = mtk_stmr_id_to_dev(STMR_CLKEVT_ID);

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_setevt_in = sched_clock();
	t_setevt_ticks = ticks;
#endif

	/*
	 * stmr spinlock is not required here since spinlock
	 * "tick_broadcast_lock" shall be held before.
	 */

	/*
	 * reset timer first because we do not expect interrupt is triggered
	 * by old compare value.
	 */

	spin_lock(&systimer_lock);

	mtk_stmr_reset(dev);

	mt_reg_sync_writel(STMR_CON_EN, dev->base_addr + STMR_CON);

	mt_reg_sync_writel(ticks, dev->base_addr + STMR_VAL);

	mt_reg_sync_writel(STMR_CON_EN | STMR_CON_IRQ_EN,
		dev->base_addr + STMR_CON);

	spin_unlock(&systimer_lock);

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_setevt_out = sched_clock();
	dbg_setevt_cpu = smp_processor_id();
#endif

	return 0;
}

static int mtk_stmr_clkevt_shutdown(struct clock_event_device *clk)
{
	struct mtk_stmr_device *dev = mtk_stmr_id_to_dev(STMR_CLKEVT_ID);

	mtk_stmr_reset(dev);

	return 0;
}

static int mtk_stmr_clkevt_resume(struct clock_event_device *clk)
{
	return mtk_stmr_clkevt_shutdown(clk);
}

static int mtk_stmr_clkevt_oneshot(struct clock_event_device *clk)
{
	return 0;
}

static void mtk_stmr_clkevt_handler(unsigned long data)
{
	struct clock_event_device *evt = (struct clock_event_device *)data;

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_clkevt_in = sched_clock();
#endif

	evt->event_handler(evt);

#if (defined MTK_TIMER_DBG_AEE_DUMP && defined CONFIG_MTK_RAM_CONSOLE)
	t_clkevt_out = sched_clock();
#endif
}

static inline void mtk_stmr_setup_clkevt(u32 freq, int irq)
{
	struct clock_event_device *evt = &mtk_stmr_clkevt;
	struct mtk_stmr_device *dev = mtk_stmr_id_to_dev(STMR_CLKEVT_ID);

	/* ensure to provide irq number for tick_broadcast_set_affinity() */
	evt->irq = irq;
	evt->mult = div_sc(freq, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(0xffffffff, evt);
	evt->min_delta_ns = clockevent_delta2ns(1300, evt);
	evt->cpumask = cpu_possible_mask;

	mtk_stmr_dev_setup(dev, mtk_stmr_clkevt_handler);

	pr_info("stmr%d, mult=%u, shift=%u, hz=%d, freq=%d\n",
		STMR_CLKEVT_ID, evt->mult, evt->shift, HZ, freq);

	clockevents_register_device(evt);
}

static void __init mtk_stmr_init_clkevt(struct device_node *node)
{
	u32 freq;
	struct clk *clk_evt;

	clk_evt = of_clk_get(node, 0);
	if (IS_ERR(clk_evt)) {
		pr_info("can't get timer clk_evt\n");
		return;
	}

	if (clk_prepare_enable(clk_evt)) {
		pr_info("can't prepare clk_evt\n");
		clk_put(clk_evt);
		return;
	}

	freq = (u32)clk_get_rate(clk_evt);

	WARN(!freq, "can't get freq of clk_evt\n");

	mtk_stmr_setup_clkevt(freq, stmrs.tmr_irq);

	pr_info("clkevt, freq=%d\n", freq);

}

static int __init mtk_stmr_init(struct device_node *node)
{
	int i;
	unsigned long save_flags;

	mtk_stmr_lock(save_flags);

	/* Setup IRQ numbers */
	stmrs.tmr_irq = irq_of_parse_and_map(node, 0);

	/* Setup IO addresses */
	stmrs.tmr_regs = of_iomap(node, 0);

	pr_info("base=0x%lx, irq=%d\n",
		(unsigned long)stmrs.tmr_regs, stmrs.tmr_irq);

	/* setup gpt itself */
	mtk_stmr_dev_init();

	for (i = 0; i < NR_STMRS; i++)
		mtk_stmr_reset(&stmr_devs[i]);

	setup_irq(stmrs.tmr_irq, &mtk_stmr_irq);

	mtk_stmr_init_clkevt(node);

	mtk_stmr_unlock(save_flags);

	return 0;
}

CLOCKSOURCE_OF_DECLARE(mtk_timer_systimer, "mediatek,sys_timer", mtk_stmr_init);
MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");

#endif /* CONFIG_MTK_TIMER_SYSTIMER */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek Clock Event Timer");

