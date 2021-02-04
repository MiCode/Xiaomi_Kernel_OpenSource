/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <clocksource/arm_arch_timer.h>

#include <mt-plat/mtk_gpt.h>
#include <mt-plat/sync_write.h>

#include <linux/irqchip/mtk-gic-extend.h> /* for aee dump */
#include <linux/sched.h> /* for aee dump */

#define APXGPT_OF_COMPTIBLE_NAME "mediatek,apxgpt"

/*
 * CONFIG_MTK_TIMER_BC_IRQ_FORCE_CPU0:
 *
 * Always force irq affinity to cpu 0. For debugging purpose only.
 */

/* #define CONFIG_MTK_TIMER_BC_IRQ_FORCE_CPU0 */

/*
 * CONFIG_MTK_TIMER_DEBUG:
 *
 * Enable debugging mechanism.
 */

/* #define CONFIG_MTK_TIMER_DEBUG */

/*
 * CONFIG_MTK_TIMER_AEE_DUMP:
 *
 * Enable dump during debugging flow, for example, HWT.
 * The debugging information will be collected to DB.
 */
#define CONFIG_MTK_TIMER_AEE_DUMP

#ifdef CONFIG_MTK_TIMER_APXGPT
#ifdef CONFIG_MTK_TIMER_AEE_DUMP
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>

static char     gpt_clkevt_aee_dump_buf[128];
static uint64_t gpt_time_clkevt_handler_entry;
static uint64_t gpt_time_clkevt_handler_exit;
static uint64_t gpt_time_clkevt_set_next_event_entry;
static uint64_t gpt_time_clkevt_set_next_event_exit;
static uint64_t gpt_time_int_handler_entry;
static uint64_t gpt_time_int_handler_exit;

#define _MTK_TIMER_DBG_AEE_DUMP

#endif
#endif
#endif

/*
 * Common apxpgt definition and functions used when,
 * 1. CONFIG_MTK_TIMER_APXGPT on: apxgpt will be used.
 * 2. CONFIG_MTK_TIMER_SYSTIMER on: apxgpt will be un-initialized
 *    in init stage.
 */
#if defined(CONFIG_MTK_TIMER_APXGPT) || defined(CONFIG_MTK_TIMER_SYSTIMER)

#define GPT_CLKEVT_ID       (GPT1)
#define GPT_CLKSRC_ID       (GPT2)

#define GPT_IRQEN           (AP_XGPT_BASE + 0x0000)
#define GPT_IRQSTA          (AP_XGPT_BASE + 0x0004)
#define GPT_IRQACK          (AP_XGPT_BASE + 0x0008)
#define GPT1_BASE           (AP_XGPT_BASE + 0x0010)

#define GPT_CON             (0x00)
#define GPT_CLK             (0x04)
#define GPT_CNT             (0x08)
#define GPT_CMP             (0x0C)
#define GPT_CNTH            (0x18)
#define GPT_CMPH            (0x1C)

#define GPT_CON_ENABLE      (0x1 << 0)
#define GPT_CON_CLRCNT      (0x1 << 1)
#define GPT_CON_OPMODE      (0x3 << 4)

#define GPT_OPMODE_MASK     (0x3)
#define GPT_CLKDIV_MASK     (0xf)
#define GPT_CLKSRC_MASK     (0x1)

#define GPT_OPMODE_OFFSET   (4)
#define GPT_CLKSRC_OFFSET   (4)

#define GPT_FEAT_64_BIT     (0x0001)
#define GPT_ISR             (0x0010)
#define GPT_IN_USE          (0x0100)

/************define this for 32/64 compatible**************/
#define GPT_BIT_MASK_L 0x00000000FFFFFFFF
#define GPT_BIT_MASK_H 0xFFFFFFFF00000000
/****************************************************/

struct mt_gpt_timers {
	int tmr_irq;
	void __iomem *tmr_regs;
};

struct mt_gpt_device {
	unsigned int id;
	unsigned int mode;
	unsigned int clksrc;
	unsigned int clkdiv;
	unsigned int cmp[2];
	void (*func)(unsigned long);
	int flags;
	int features;
	void __iomem *base_addr;
};

/*
 * Return GPT4 count(before init clear) to record
 * kernel start time between LK and kernel
 */

/* 1000000 / 76.92ns = 13000.520 */
#define GPT4_1MS_TICK       ((u32)(13000))
#define GPT4_BASE           (AP_XGPT_BASE + 0x0040)

#define mt_gpt_set_reg(val, addr) \
	mt_reg_sync_writel(__raw_readl(addr)|(val), addr)
#define mt_gpt_clr_reg(val, addr) \
	mt_reg_sync_writel(__raw_readl(addr)&~(val), addr)

static struct mt_gpt_timers gpt_timers;
static struct mt_gpt_device gpt_devs[NR_GPTS];

#define AP_XGPT_BASE gpt_timers.tmr_regs

static void __gpt_disable_irq(struct mt_gpt_device *dev)
{
	mt_gpt_clr_reg(0x1 << (dev->id), GPT_IRQEN);
}

static void __gpt_ack_irq(struct mt_gpt_device *dev)
{
	mt_reg_sync_writel(0x1 << (dev->id), GPT_IRQACK);
}

static void __gpt_reset(struct mt_gpt_device *dev)
{
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CON);
	__gpt_disable_irq(dev);
	__gpt_ack_irq(dev);
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CLK);
	mt_reg_sync_writel(0x2, dev->base_addr + GPT_CON);
	mt_reg_sync_writel(0x0, dev->base_addr + GPT_CMP);
	if (dev->features & GPT_FEAT_64_BIT)
		mt_reg_sync_writel(0, dev->base_addr + GPT_CMPH);
}

static void gpt_devs_init(void)
{
	int i;

	for (i = 0; i < NR_GPTS; i++) {
		gpt_devs[i].id = i;
		gpt_devs[i].base_addr = GPT1_BASE + 0x10 * i;
		pr_info("gpt%d, base=0x%lx\n",
			i + 1, (unsigned long)gpt_devs[i].base_addr);
	}

	gpt_devs[GPT6].features |= GPT_FEAT_64_BIT;
}

#endif

#ifdef CONFIG_MTK_TIMER_APXGPT

static DEFINE_SPINLOCK(gpt_lock);

static unsigned int boot_time_value;

static unsigned int gpt_boot_up_time(void)
{
	unsigned int tick;

	tick = __raw_readl(GPT4_BASE + GPT_CNT);
	return ((tick + (GPT4_1MS_TICK - 1)) / GPT4_1MS_TICK);
}
/*********************************************************/

static struct mt_gpt_device *id_to_dev(unsigned int id)
{
	return id < NR_GPTS ? gpt_devs + id : NULL;
}

#define gpt_update_lock(flags)  spin_lock_irqsave(&gpt_lock, flags)

#define gpt_update_unlock(flags)  spin_unlock_irqrestore(&gpt_lock, flags)

static inline void noop(unsigned long data) { }
static void(*handlers[])(unsigned long) = {
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
	noop,
};

static struct tasklet_struct task[NR_GPTS];
static void task_sched(unsigned long data)
{
	unsigned int id = (unsigned int)data;

	tasklet_schedule(&task[id]);
}

static irqreturn_t gpt_handler(int irq, void *dev_id);
static void __gpt_ack_irq(struct mt_gpt_device *dev);
static cycle_t mt_gpt_read(struct clocksource *cs);
static int mt_gpt_clkevt_next_event(unsigned long cycles,
				   struct clock_event_device *evt);
static int mt_gpt_clkevt_shutdown(struct clock_event_device *clk);
static int mt_gpt_clkevt_oneshot(struct clock_event_device *clk);
static int mt_gpt_clkevt_resume(struct clock_event_device *clk);
static int mt_gpt_set_periodic(struct clock_event_device *clk);

static struct clocksource gpt_clocksource = {
	.name	= "mtk-timer",
	.rating	= 300,
	.read	= mt_gpt_read,
	.mask	= CLOCKSOURCE_MASK(32),
	.shift  = 25,
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct clock_event_device gpt_clockevent = {
	.name = "mtk_tick",
	/*
	 * CLOCK_EVT_FEAT_DYNIRQ: Core shall set the interrupt affinity
	 *                        dynamically in broadcast mode.
	 * CLOCK_EVT_FEAT_ONESHOT: Use one-shot mode for tick broadcast.
	 */
#ifdef CONFIG_MTK_TIMER_BC_IRQ_FORCE_CPU0
	.features = CLOCK_EVT_FEAT_ONESHOT,
#else
	.features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_DYNIRQ,
#endif
	.shift          = 32,
	.rating         = 300,
	.set_next_event = mt_gpt_clkevt_next_event,
	.set_state_shutdown = mt_gpt_clkevt_shutdown,
	.set_state_periodic = mt_gpt_set_periodic,
	.set_state_oneshot = mt_gpt_clkevt_oneshot,
	.tick_resume = mt_gpt_clkevt_resume,
};

static struct irqaction gpt_irq = {
	.name = "mt-gpt",
	.flags = IRQF_TIMER | IRQF_IRQPOLL | IRQF_TRIGGER_LOW | IRQF_PERCPU,
	.handler = gpt_handler,
	.dev_id = &gpt_clockevent,
};

void mtk_timer_clkevt_aee_dump(void)
{
#ifdef _MTK_TIMER_DBG_AEE_DUMP

	/*
	 * Notice: print function cannot be used during AEE flow
	 * to avoid lock issues.
	 */

	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	/* interrupt time */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] int handler entry: %llu\n", gpt_time_int_handler_entry);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] int handler exit: %llu\n", gpt_time_int_handler_exit);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* clkevt handler time */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] clkevt handler entry: %llu\n",
		gpt_time_clkevt_handler_entry);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] clkevt handler exit: %llu\n",
		gpt_time_clkevt_handler_exit);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* set next event */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] set next event entry: %llu\n",
		gpt_time_clkevt_set_next_event_entry);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] set next event exit: %llu\n",
		gpt_time_clkevt_set_next_event_exit);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* global gpt status */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] IRQEN: 0x%x\n", __raw_readl(GPT_IRQEN));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT] IRQSTA: 0x%x\n", __raw_readl(GPT_IRQSTA));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/* gpt1 status */

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CON: 0x%x\n", __raw_readl(dev->base_addr + GPT_CON));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CLK: 0x%x\n", __raw_readl(dev->base_addr + GPT_CLK));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CNT: 0x%x\n", __raw_readl(dev->base_addr + GPT_CNT));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] CMP: 0x%x\n", __raw_readl(dev->base_addr + GPT_CMP));
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	memset(gpt_clkevt_aee_dump_buf, 0, sizeof(gpt_clkevt_aee_dump_buf));
	snprintf(gpt_clkevt_aee_dump_buf, sizeof(gpt_clkevt_aee_dump_buf),
		"[GPT1] irq affinity: %d\n", gpt_clockevent.irq_affinity_on);
	aee_sram_fiq_log(gpt_clkevt_aee_dump_buf);

	/*
	 * TODO: dump apxgpt irq status
	 *
	 * Since print function cannot be used during AEE flow, we may need to
	 * change print way in mt_irq_dump_status().
	 */

	/* mt_irq_dump_status(gpt_timers.tmr_irq); */

#endif
}

static inline unsigned int gpt_get_and_ack_irq(void)
{
	unsigned int id;
	unsigned int mask;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	for (id = GPT1; id < NR_GPTS; id++) {
		mask = 0x1 << id;
		if (status & mask) {
			mt_reg_sync_writel(mask, GPT_IRQACK);
			break;
		}
	}
	return id;
}

/*
 * gpt_handler users are listed as below,
 *
 * For ACAO project:
 *   GPT1: SoC timer for tick-broadcasting (oneshot)
 *
 * For HPS project:
 *   GPT4: Wakeup source for MTK idle framework (oneshot)
 */
static irqreturn_t gpt_handler(int irq, void *dev_id)
{
	unsigned int id;
	struct mt_gpt_device *dev;

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_int_handler_entry = sched_clock();
#endif

	id = gpt_get_and_ack_irq();
	dev = id_to_dev(id);

	if (likely(dev)) {
		if (!(dev->flags & GPT_ISR))
			handlers[id](id);
		else
			handlers[id]((unsigned long)dev_id);
	} else
		pr_info("GPT id is %d\n", id);

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_int_handler_exit = sched_clock();
#endif

	return IRQ_HANDLED;
}

static void __gpt_enable_irq(struct mt_gpt_device *dev)
{
	mt_gpt_set_reg(0x1 << (dev->id), GPT_IRQEN);
}

static void __gpt_get_cnt(struct mt_gpt_device *dev, unsigned int *ptr)
{
	*ptr = __raw_readl(dev->base_addr + GPT_CNT);
	if (dev->features & GPT_FEAT_64_BIT)
		*(++ptr) = __raw_readl(dev->base_addr + GPT_CNTH);
}

static void __gpt_get_cmp(struct mt_gpt_device *dev, unsigned int *ptr)
{
	*ptr = __raw_readl(dev->base_addr + GPT_CMP);
	if (dev->features & GPT_FEAT_64_BIT)
		*(++ptr) = __raw_readl(dev->base_addr + GPT_CMPH);
}

static void __gpt_set_mode(struct mt_gpt_device *dev, unsigned int mode)
{
	unsigned int ctl = __raw_readl(dev->base_addr + GPT_CON);

	mode <<= GPT_OPMODE_OFFSET;

	ctl &= ~GPT_CON_OPMODE;
	ctl |= mode;

	mt_reg_sync_writel(ctl, dev->base_addr + GPT_CON);

	dev->mode = mode;
}

static void __gpt_set_clk(struct mt_gpt_device *dev,
	unsigned int clksrc, unsigned int clkdiv)
{
	unsigned int clk = (clksrc << GPT_CLKSRC_OFFSET) | clkdiv;

	mt_reg_sync_writel(clk, dev->base_addr + GPT_CLK);

	dev->clksrc = clksrc;
	dev->clkdiv = clkdiv;
}

static void __gpt_set_cmp(struct mt_gpt_device *dev, unsigned int cmpl,
		unsigned int cmph)
{
	mt_reg_sync_writel(cmpl, dev->base_addr + GPT_CMP);
	dev->cmp[0] = cmpl;

	if (dev->features & GPT_FEAT_64_BIT) {
		mt_reg_sync_writel(cmph, dev->base_addr + GPT_CMPH);
		dev->cmp[1] = cmpl;
	}
}

static void __gpt_clrcnt(struct mt_gpt_device *dev)
{
	mt_gpt_set_reg(GPT_CON_CLRCNT, dev->base_addr + GPT_CON);
	while (__raw_readl(dev->base_addr + GPT_CNT))
		cpu_relax();
}

static void __gpt_start(struct mt_gpt_device *dev)
{
	mt_gpt_set_reg(GPT_CON_ENABLE, dev->base_addr + GPT_CON);
}

static void __gpt_wait_clrcnt(void)
{
	/*
	 * if gpt is running in 32K domain, it needs 3T (~90 us) for clearing
	 * old counter.
	 */
	#define WAIT_CLR_CNT_TIME_NS 100000

	uint64_t start_time = 0, end_time = 0;

	start_time = sched_clock();
	end_time = start_time;

	while ((end_time - start_time) < WAIT_CLR_CNT_TIME_NS)
		end_time = sched_clock();
}

static void __gpt_wait_clrcnt_then_start(struct mt_gpt_device *dev)
{
	__gpt_wait_clrcnt();
	__gpt_start(dev);
}

static void __gpt_stop(struct mt_gpt_device *dev)
{
	mt_gpt_clr_reg(GPT_CON_ENABLE, dev->base_addr + GPT_CON);
}
static void __gpt_set_flags(struct mt_gpt_device *dev, unsigned int flags)
{
	dev->flags |= flags;
}

static void __gpt_set_handler(struct mt_gpt_device *dev,
	void (*func)(unsigned long))
{
	if (func) {
		if (dev->flags & GPT_ISR)
			handlers[dev->id] = func;
		else {
			tasklet_init(&task[dev->id], func, 0);
			handlers[dev->id] = task_sched;
		}
	}
	dev->func = func;
}

static void setup_gpt_dev_locked(struct mt_gpt_device *dev, unsigned int mode,
		unsigned int clksrc, unsigned int clkdiv, unsigned int cmp,
		void (*func)(unsigned long), unsigned int flags)
{
	__gpt_set_flags(dev, flags | GPT_IN_USE);

	__gpt_set_mode(dev, mode & GPT_OPMODE_MASK);
	__gpt_set_clk(dev, clksrc & GPT_CLKSRC_MASK, clkdiv & GPT_CLKDIV_MASK);

	if (func)
		__gpt_set_handler(dev, func);

	if (dev->mode != GPT_FREE_RUN) {
		__gpt_set_cmp(dev, cmp, 0);
		if (!(dev->flags & GPT_NOIRQEN))
			__gpt_enable_irq(dev);
	}

	if (!(dev->flags & GPT_NOAUTOEN))
		__gpt_start(dev);
}

static int mt_gpt_clkevt_next_event(unsigned long cycles,
				   struct clock_event_device *evt)
{
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_clkevt_set_next_event_entry = sched_clock();
#endif

	/*
	 * disable irq first because we do not expect interrupt is triggered
	 * by old compare value.
	 */
	__gpt_disable_irq(dev);

	/*
	 * Configure gpt1 to use 13MHz clock during re-configuration.
	 *
	 * Reason: Clock synchronization issue may happen if gpt is in 32KHz
	 *         domain during re-configuration. For example: Updating cmp
	 *         value may need to wait a period of time (e.g., 3.5T) to let
	 *         gpt hw finish jobs: gpt hw will clear counter value
	 *         automatically while setting new cmp value.
	 *
	 *         GPT_EN shall be enabled after gpt hw finishes above job,
	 *         otherwise gpt may work abnormally, e.g., wrong cmp value
	 *         is latched or counter value is not reset.
	 *
	 *         If gpt is running under 13MHz, above waiting is not required
	 *         since gpt hw guarantees that GPT_EN is applied after above
	 *         job is done.
	 */
	__gpt_set_clk(dev, GPT_CLK_SRC_SYS & GPT_CLKSRC_MASK,
		GPT_CLK_DIV_1 & GPT_CLKDIV_MASK);

	__gpt_stop(dev);

	if (cycles < 3) {
		pr_info("[mt_gpt] invalid cycles < 3\n");
		cycles = 3;
	}

	/*
	 * Do cmp first because updating cmp will trigger most
	 * complicated behavior in gpt hw. See above description.
	 */
	__gpt_set_cmp(dev, cycles, 0);

	/* ack irq */
	__gpt_ack_irq(dev);

	/* ensure irq is enabled before next running */
	__gpt_enable_irq(dev);

	/*
	 * Configure gpt1 to use 32KHz clock before enabling.
	 *
	 * Reason: 13MHz clock source may be disabled during some
	 *         low-power scenarios, e.g., SODI. We shall use a
	 *         always-on clock after enabling, e.g., 32KHz.
	 */
	__gpt_set_clk(dev, GPT_CLK_SRC_RTC & GPT_CLKSRC_MASK,
		GPT_CLK_DIV_1 & GPT_CLKDIV_MASK);

	__gpt_start(dev);

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_clkevt_set_next_event_exit = sched_clock();
#endif

	return 0;
}

static int mt_gpt_clkevt_shutdown(struct clock_event_device *clk)
{
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_disable_irq(dev);
	__gpt_ack_irq(dev);

	return 0;
}

static int mt_gpt_clkevt_resume(struct clock_event_device *clk)
{
	return mt_gpt_clkevt_shutdown(clk);
}

static int mt_gpt_clkevt_oneshot(struct clock_event_device *clk)
{
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_set_mode(dev, GPT_ONE_SHOT);
	/* __gpt_enable_irq(dev); */
	/* __gpt_clrcnt_and_start(dev); */

	return 0;
}

static int mt_gpt_set_periodic(struct clock_event_device *clk)
{
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	__gpt_stop(dev);
	__gpt_set_mode(dev, GPT_REPEAT);
	__gpt_enable_irq(dev);
	__gpt_wait_clrcnt_then_start(dev);

	return 0;
}

static cycle_t mt_gpt_read(struct clocksource *cs)
{
	cycle_t cycles;
	unsigned int cnt[2] = {0, 0};
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKSRC_ID);

	__gpt_get_cnt(dev, cnt);

	if (GPT_CLKSRC_ID != GPT6) {
		/*
		 * force do mask for high 32-bit to avoid unpredicted
		 * alignment
		 */
		cycles = (GPT_BIT_MASK_L & (cycle_t) (cnt[0]));
	} else {
		cycles = (GPT_BIT_MASK_H & (((cycle_t) (cnt[1])) << 32)) |
			(GPT_BIT_MASK_L&((cycle_t) (cnt[0])));
	}

	return cycles;
}

static void clkevt_handler(unsigned long data)
{
	struct clock_event_device *evt = (struct clock_event_device *)data;

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_clkevt_handler_entry = sched_clock();
#endif

#if defined(CONFIG_MTK_TIMER_DEBUG) && \
		!defined(CONFIG_MTK_TIMER_BC_IRQ_FORCE_CPU0)
	int cpu;
	int err = 0;

	cpu = mt_irq_dump_cpu(evt->irq);

	if (cpu < 0) {
		pr_info("[mt_gpt] invalid irq query! ret %d\n", cpu);
		err = 1;
	} else {
		if (cpu != smp_processor_id()) {
			pr_info("[mt_gpt] wrong irq! irq_cpu %d, cur_cpu %d\n",
				cpu, smp_processor_id());
			err = 1;
		}

		if (cpu != evt->irq_affinity_on) {
			pr_info("[mt_gpt] wrong affinity! irq_cpu %d, affinity %d\n",
				cpu, evt->irq_affinity_on);
			err = 1;
		}
	}

	if (!err)
		mt_irq_dump_status(evt->irq);
#endif

	evt->event_handler(evt);

#ifdef _MTK_TIMER_DBG_AEE_DUMP
	gpt_time_clkevt_handler_exit = sched_clock();
#endif
}

static inline void setup_clksrc(u32 freq)
{
	struct clocksource *cs = &gpt_clocksource;
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKSRC_ID);

	pr_info("setup_clksrc1: dev->base_addr=0x%lx GPT2_CON=0x%x\n",
		(unsigned long)dev->base_addr, __raw_readl(dev->base_addr));

	/* add GPT_NOIRQEN flag to avoid irq asserted because
	 * clksrc is not used
	 */
	setup_gpt_dev_locked(dev, GPT_FREE_RUN, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
		0, NULL, GPT_NOIRQEN);

	/* clocksource_register(cs); */
	clocksource_register_hz(cs, freq);

	pr_info("setup_clksrc2: dev->base_addr=0x%lx GPT2_CON=0x%x\n",
		(unsigned long)dev->base_addr, __raw_readl(dev->base_addr));
}

static inline void setup_clkevt(u32 freq, int irq)
{
	unsigned int cmp[2];
	unsigned int clkdiv;
	struct clock_event_device *evt = &gpt_clockevent;
	struct mt_gpt_device *dev = id_to_dev(GPT_CLKEVT_ID);

	/* ensure to provide irq number for tick_broadcast_set_affinity() */
	evt->irq = irq;

	evt->mult = div_sc(freq, NSEC_PER_SEC, evt->shift);
	evt->max_delta_ns = clockevent_delta2ns(0xffffffff, evt);
	evt->min_delta_ns = clockevent_delta2ns(3, evt);
	evt->cpumask = cpu_possible_mask;

	if (freq == 13000000)
		clkdiv = GPT_CLK_SRC_SYS;
	else
		clkdiv = GPT_CLK_SRC_RTC;

	/*
	 * 1. Use always-on 32K clock since 13M/26M clock will be
	 *    disabled during SODI
	 * 2. Configure this device as ONESHOT mode as tick broadcast device.
	 */
	setup_gpt_dev_locked(dev, GPT_ONE_SHOT, clkdiv, GPT_CLK_DIV_1,
		freq / HZ, clkevt_handler, GPT_ISR);

	__gpt_get_cmp(dev, cmp);

	pr_info("apxgpt%d: clkdiv=%d, cmp=%d, hz=%d, freq=%d\n",
		GPT_CLKEVT_ID + 1, clkdiv, cmp[0], HZ, freq);

	clockevents_register_device(evt);
}

static void __init mt_gpt_init_acao(struct device_node *node)
{
	u32 freq;
	struct clk *clk_evt;

	/* inquiry clk_evt, freq is RTC_CLK_RATE (32KHz) */

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

#ifdef CONFIG_MTK_TIMER_BC_IRQ_FORCE_CPU0
	irq_force_affinity(gpt_timers.tmr_irq, cpumask_of(0));
#endif

	setup_clkevt(freq, gpt_timers.tmr_irq);

	pr_info("acao clkevt, freq=%d\n",	freq);

}

static void release_gpt_dev_locked(struct mt_gpt_device *dev)
{
	__gpt_reset(dev);

	handlers[dev->id] = noop;
	dev->func = NULL;

	dev->flags = 0;
}

/* gpt is counting or not */
static int __gpt_get_status(struct mt_gpt_device *dev)
{
	return !!(__raw_readl(dev->base_addr + GPT_CON) & GPT_CON_ENABLE);
}

/**********************	export area *********************/
int request_gpt(unsigned int id, unsigned int mode, unsigned int clksrc,
		unsigned int clkdiv, unsigned int cmp,
		void (*func)(unsigned long), unsigned int flags)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (dev->flags & GPT_IN_USE) {
		pr_info("%s: GPT%d is in use!\n", __func__, (id + 1));
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	setup_gpt_dev_locked(dev, mode, clksrc, clkdiv, cmp, func, flags);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(request_gpt);

int free_gpt(unsigned int id)
{
	unsigned long save_flags;

	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE))
		return 0;

	gpt_update_lock(save_flags);
	release_gpt_dev_locked(dev);
	gpt_update_unlock(save_flags);
	return 0;
}
EXPORT_SYMBOL(free_gpt);

int start_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_clrcnt(dev);
	__gpt_start(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(start_gpt);

int stop_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_stop(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(stop_gpt);

int restart_gpt(unsigned int id)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_start(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(restart_gpt);

int gpt_is_counting(unsigned int id)
{
	unsigned long save_flags;
	int is_counting;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	is_counting = __gpt_get_status(dev);
	gpt_update_unlock(save_flags);

	return is_counting;
}
EXPORT_SYMBOL(gpt_is_counting);

int gpt_set_cmp(unsigned int id, unsigned int val)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (dev->mode == GPT_FREE_RUN)
		return -EINVAL;

	gpt_update_lock(save_flags);
	__gpt_set_cmp(dev, val, 0);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(gpt_set_cmp);

int gpt_get_cmp(unsigned int id, unsigned int *ptr)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev || !ptr)
		return -EINVAL;

	gpt_update_lock(save_flags);
	__gpt_get_cmp(dev, ptr);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(gpt_get_cmp);

int gpt_get_cnt(unsigned int id, unsigned int *ptr)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev || !ptr)
		return -EINVAL;

	if (!(dev->features & GPT_FEAT_64_BIT)) {
		__gpt_get_cnt(dev, ptr);
	} else {
		gpt_update_lock(save_flags);
		__gpt_get_cnt(dev, ptr);
		gpt_update_unlock(save_flags);
	}

	return 0;
}
EXPORT_SYMBOL(gpt_get_cnt);

u64 mtk_timer_get_cnt(u8 timer)
{
	unsigned long save_flags;
	unsigned int val[2] = {0, 0};
	u64 cnt = 0;
	struct mt_gpt_device *dev = id_to_dev(timer - 1);

	if (!dev || timer <= 0)
		return -EINVAL;

	if (!(dev->features & GPT_FEAT_64_BIT)) {
		__gpt_get_cnt(dev, val);
		cnt = (((u64)val[1] << 32) | (u64)val[0]);
	} else {
		gpt_update_lock(save_flags);
		__gpt_get_cnt(dev, val);
		gpt_update_unlock(save_flags);
		cnt = ((u64)val[0]) & 0x00000000FFFFFFFF;
	}

	return cnt;
}

int gpt_check_irq(unsigned int id)
{
	unsigned int mask = 0x1 << id;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	return (status & mask) ? 1 : 0;
}
EXPORT_SYMBOL(gpt_check_irq);

int gpt_check_and_ack_irq(unsigned int id)
{
	unsigned int mask = 0x1 << id;
	unsigned int status = __raw_readl(GPT_IRQSTA);

	if (status & mask) {
		mt_reg_sync_writel(mask, GPT_IRQACK);
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL(gpt_check_and_ack_irq);

unsigned int gpt_boot_time(void)
{
	return boot_time_value;
}
EXPORT_SYMBOL(gpt_boot_time);

int gpt_set_clk(unsigned int id, unsigned int clksrc, unsigned int clkdiv)
{
	unsigned long save_flags;
	struct mt_gpt_device *dev = id_to_dev(id);

	if (!dev)
		return -EINVAL;

	if (!(dev->flags & GPT_IN_USE)) {
		pr_info("%s: GPT%d is not in use!\n", __func__, id);
		return -EBUSY;
	}

	gpt_update_lock(save_flags);
	__gpt_stop(dev);
	__gpt_set_clk(dev, clksrc, clkdiv);
	__gpt_start(dev);
	gpt_update_unlock(save_flags);

	return 0;
}
EXPORT_SYMBOL(gpt_set_clk);

static int __init mt_gpt_init(struct device_node *node)
{
	int i;
	unsigned long save_flags;

	gpt_update_lock(save_flags);

	/* Setup IRQ numbers */
	gpt_timers.tmr_irq = irq_of_parse_and_map(node, 0);

	/* Setup IO addresses */
	gpt_timers.tmr_regs = of_iomap(node, 0);

	pr_info("base=0x%lx, irq=%d\n",
		(unsigned long)gpt_timers.tmr_regs, gpt_timers.tmr_irq);

	/* setup gpt itself */
	gpt_devs_init();

	for (i = 0; i < NR_GPTS; i++)
		__gpt_reset(&gpt_devs[i]);

	setup_irq(gpt_timers.tmr_irq, &gpt_irq);

	mt_gpt_init_acao(node);

	/* record the time when init GPT */
	boot_time_value = gpt_boot_up_time();

	gpt_update_unlock(save_flags);

	return 0;
}

CLOCKSOURCE_OF_DECLARE(mtk_apxgpt, APXGPT_OF_COMPTIBLE_NAME, mt_gpt_init);

#endif

/*
 * Apxgpt un-initialization used when sys_timer is selected.
 */
#if !defined(CONFIG_MTK_TIMER_APXGPT) && !defined(CONFIG_MTK_TIMER)

static int __init mt_gpt_init(void)
{
	u32 i;
	struct device_node *node;
	const struct of_device_id apxgpt_of_match[] = {
		{ .compatible = APXGPT_OF_COMPTIBLE_NAME, },
	};

	node = of_find_compatible_node(NULL, NULL,
		apxgpt_of_match[0].compatible);

	if (!node) {
		pr_info("node not found\n");
		return 0;
	}

	gpt_timers.tmr_regs = of_iomap(node, 0);

	if (!gpt_timers.tmr_regs) {
		pr_info("base not found\n");
		return 0;
	}

	gpt_devs_init();

	for (i = 0; i < NR_GPTS; i++)
		__gpt_reset(&gpt_devs[i]);

	pr_info("base=0x%lx, disabled\n",
		(unsigned long)gpt_timers.tmr_regs);

	return 0;
}

module_init(mt_gpt_init);

#endif

