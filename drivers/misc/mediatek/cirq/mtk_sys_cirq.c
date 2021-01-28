// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/irqchip/arm-gic.h>
#include "mtk_sys_cirq.h"
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
#include <linux/list.h>
#include <linux/bitops.h>
#endif
//#include <linux/irqchip/arm-gic-v3.h>
#include <linux/irqchip/mtk-gic-extend.h>

void __iomem *SYS_CIRQ_BASE;
static unsigned int CIRQ_IRQ_NUM;
static unsigned int CIRQ_SPI_START;
static unsigned int sw_reset;
#ifdef LATENCY_CHECK
unsigned long long clone_t1;
unsigned long long clone_t2;
unsigned long long flush_t1;
unsigned long long flush_t2;
#endif




#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
static struct cirq_events cirq_all_events;
static unsigned int already_cloned;
#endif

/*
 *Define Data Structure
 */
struct mt_cirq_driver {
	struct platform_driver driver;
	const struct platform_device_id *id_table;
};

/*
 * Define Global Variable
 */
static struct mt_cirq_driver mt_cirq_drv = {
	.driver = {
		.driver = {
		   .name = "cirq",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   },
	},
	.id_table = NULL,
};

static unsigned long cirq_clone_flush_check_val;
static unsigned long cirq_pattern_clone_flush_check_val;
static unsigned long cirq_pattern_list;

/*
 * mt_cirq_get_mask: Get the specified SYS_CIRQ mask
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is masked
 *    0: this cirq is umasked
 *    -1: cirq num is out of range
 */
static int mt_cirq_get_mask(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			__func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_MASK_BASE));
	return !!(st & bit);
}

static int mt_cirq_get_mask_vec(int i)
{
	return readl(i*4 + CIRQ_MASK_BASE);
}

/*
 * mt_cirq_mask_all: Mask all interrupts on SYS_CIRQ.
 */


/*
 * mt_cirq_ack_all: Ack all the interrupt on SYS_CIRQ
 */
void mt_cirq_ack_all(void)
{
	u32 ack_vec, pend_vec, mask_vec;
	int i;

	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++) {
		/* if a irq is pending & not masked, don't ack it
		 * , since cirq start irq might not be 32 aligned with gic,
		 * need an exotic API to get proper vector of pending irq
		 */
		pend_vec = mt_irq_get_pending_vec(CIRQ_SPI_START+(i+1)*32);
		mask_vec = mt_cirq_get_mask_vec(i);
		/* those should be acked are: "not (pending & not masked)",
		 */
		ack_vec = (~pend_vec) | mask_vec;
		writel_relaxed(ack_vec, CIRQ_ACK_BASE + (i * 4));
	}

	/* make sure all cirq setting take effect
	 * before doing other things
	 */
	mb();
}
void mt_cirq_mask_all(void)
{
	unsigned int i;

	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++)
		writel_relaxed(0xFFFFFFFF, CIRQ_MASK_SET_BASE + (i * 4));

	/* make sure all cirq setting take effect before doing other things */
	mb();
}

/*
 * mt_cirq_unmask_all: Unmask all interrupts on SYS_CIRQ.
 */
void mt_cirq_unmask_all(void)
{
	unsigned int i;

	for (i = 0; i < CIRQ_CTRL_REG_NUM; i++)
		writel_relaxed(0xFFFFFFFF, CIRQ_MASK_CLR_BASE + (i * 4));
	/* make sure all cirq setting take effect before doing other things */
	mb();
}

/*
 * mt_cirq_mask: Mask the specified SYS_CIRQ.
 * @cirq_num: the SYS_CIRQ number to mask
 * @return:
 *    0: mask success
 *   -1: cirq num is out of range
 */
static int mt_cirq_mask(unsigned int cirq_num)
{
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	mt_reg_sync_writel(bit, (cirq_num / 32) * 4 + CIRQ_MASK_SET_BASE);
	return 0;
}

/*
 * mt_cirq_unmask: Unmask the specified SYS_CIRQ.
 * @cirq_num: the SYS_CIRQ number to unmask
 * @return:
 *    0: umask success
 *   -1: cirq num is out of range
 */
static int mt_cirq_unmask(unsigned int cirq_num)
{
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	mt_reg_sync_writel(bit, (cirq_num / 32) * 4 + CIRQ_MASK_CLR_BASE);
	return 0;
}

/*
 * mt_cirq_get_sens: Get the specified SYS_CIRQ sensitivity
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is MT_LEVEL_SENSITIVE
 *    0: this cirq is MT_EDGE_SENSITIVE
 *   -1: cirq num is out of range
 */
static int mt_cirq_get_sens(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_SENS_BASE));
	return !!(st & bit);
}

/*
 * mt_cirq_set_sens: Set the sensitivity for the specified SYS_CIRQ number.
 * @cirq_num: the SYS_CIRQ number to set
 * @sens: sensitivity to set
 * @return:
 *    0: set sens success
 *   -1: cirq num is out of range
 */
static int mt_cirq_set_sens(unsigned int cirq_num, unsigned int sens)
{
	void __iomem *base;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	if (sens == MT_EDGE_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_CLR_BASE;
	} else if (sens == MT_LEVEL_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_SET_BASE;
	} else {
		pr_debug("[CIRQ] set_sens invalid value %d\n",
			 sens);
		return -1;
	}

	mt_reg_sync_writel(bit, base);
	return 0;
}

/*
 * mt_cirq_get_pol: Get the specified SYS_CIRQ polarity
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is MT_CIRQ_POL_POS
 *    0: this cirq is MT_CIRQ_POL_NEG
 *   -1: cirq num is out of range
 */
static int mt_cirq_get_pol(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_POL_BASE));
	return !!(st & bit);
}

/*
 * mt_cirq_set_pol: Set the polarity for the specified SYS_CIRQ number.
 * @cirq_num: the SYS_CIRQ number to set
 * @pol: polarity to set
 * @return:
 *    0: set pol success
 *   -1: cirq num is out of range
 */
static int mt_cirq_set_pol(unsigned int cirq_num, unsigned int pol)
{
	void __iomem *base;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	if (pol == MT_CIRQ_POL_NEG) {
		base = (cirq_num / 32) * 4 + CIRQ_POL_CLR_BASE;
	} else if (pol == MT_CIRQ_POL_POS) {
		base = (cirq_num / 32) * 4 + CIRQ_POL_SET_BASE;
	} else {
		pr_debug("[CIRQ] set_pol invalid polarity value %d\n", pol);
		return -1;
	}

	mt_reg_sync_writel(bit, base);
	return 0;
}

/*
 * CIRQ register, which is under infra power down domain,
 * will be corrupted after exiting suspend/resume flow.
 * Due to the HW change, so we need reset the cirq by SW.
 */
void mt_cirq_sw_reset(void)
{
	unsigned int st;

	if (sw_reset) {
		st = readl(IOMEM(CIRQ_CON));
		st |= (CIRQ_SW_RESET << CIRQ_CON_SW_RST_BITS);
		mt_reg_sync_writel(st, CIRQ_CON);
	}
}
EXPORT_SYMBOL(mt_cirq_sw_reset);

/*
 * mt_cirq_enable: Enable SYS_CIRQ
 */
void mt_cirq_enable(void)
{
	unsigned int st;


	mt_cirq_ack_all();

	st = readl(IOMEM(CIRQ_CON));
	st |=
	    (CIRQ_CON_EN << CIRQ_CON_EN_BITS) | (CIRQ_CON_EDGE_ONLY <<
						 CIRQ_CON_EDGE_ONLY_BITS);
	mt_reg_sync_writel((st & CIRQ_CON_BITS_MASK), CIRQ_CON);
}
EXPORT_SYMBOL(mt_cirq_enable);


#ifndef CONFIG_FAST_CIRQ_CLONE_FLUSH
/*
 * mt_cirq_get_pending: Get the specified SYS_CIRQ pending
 * @cirq_num: the SYS_CIRQ number to get
 * @return:
 *    1: this cirq is pending
 *    0: this cirq is not pending
 */
static bool mt_cirq_get_pending(unsigned int cirq_num)
{
	unsigned int st;
	unsigned int bit = 1 << (cirq_num % 32);

	if (cirq_num >= CIRQ_IRQ_NUM) {
		pr_debug("[CIRQ] %s: invalid cirq num %d\n",
			 __func__, cirq_num);
		return -1;
	}

	st = readl(IOMEM((cirq_num / 32) * 4 + CIRQ_STA_BASE));
	st = st & bit;
	return st;
}
#endif

/*
 * mt_cirq_disable: Disable SYS_CIRQ
 */
void mt_cirq_disable(void)
{
	unsigned int st;

	st = readl(IOMEM(CIRQ_CON));
	st &= ~(CIRQ_CON_EN << CIRQ_CON_EN_BITS);
	mt_reg_sync_writel((st & CIRQ_CON_BITS_MASK), CIRQ_CON);
}
EXPORT_SYMBOL(mt_cirq_disable);


/*
 * mt_cirq_disable: Flush interrupt from SYS_CIRQ to GIC
 */
void mt_cirq_flush(void)
{
#ifdef LATENCY_CHECK
	flush_t1 = sched_clock();
#endif

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	cirq_fast_sw_flush();
	mt_cirq_mask_all();
	mt_cirq_ack_all();
#else
	unsigned int i;
	unsigned char cirq_p_val = 0;
	unsigned char irq_p_val = 0;
	unsigned int irq_p = 0;
	unsigned char pass = 1;
	unsigned int first_cirq_found = 0;
	unsigned int first_flushed_cirq;
	unsigned int first_irq_flushedto;
	unsigned int last_fluashed_cirq;
	unsigned int last_irq_flushedto;

	if (cirq_pattern_clone_flush_check_val == 1) {
		if (cirq_pattern_list < CIRQ_IRQ_NUM) {
			mt_cirq_unmask(cirq_pattern_list);
			mt_cirq_set_sens(cirq_pattern_list, MT_EDGE_SENSITIVE);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_NEG);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_POS);
			mt_cirq_set_pol(cirq_pattern_list, MT_CIRQ_POL_NEG);
		} else {
			pr_debug
			("[CIRQ] no pattern to test, input pattern first\n");
		}
		pr_debug("[CIRQ] cirq_pattern %ld, cirq_p %d,",
		cirq_pattern_list, mt_cirq_get_pending(cirq_pattern_list));
		pr_debug("cirq_s %d, cirq_con 0x%x\n",
		mt_cirq_get_sens(cirq_pattern_list), readl(IOMEM(CIRQ_CON)));
	}

	mt_cirq_unmask_all();

	for (i = 0; i < CIRQ_IRQ_NUM; i++) {
		cirq_p_val = mt_cirq_get_pending(i);
		if (cirq_p_val)
			mt_irq_set_pending_hw(CIRQ_TO_IRQ_NUM(i));

		if (cirq_clone_flush_check_val == 1) {
			if (cirq_p_val == 0)
				continue;
			irq_p = CIRQ_TO_IRQ_NUM(i);
			irq_p_val = mt_irq_get_pending_hw(irq_p);
			if (cirq_p_val != irq_p_val) {
				pr_debug
			("[CIRQ] CIRQ Flush Failed %d(cirq %d) != %d(gic %d)\n",
				     cirq_p_val, i, irq_p_val,
				     CIRQ_TO_IRQ_NUM(i));
				pass = 0;
			} else {
				pr_debug
			("[CIRQ] CIRQ Flush Pass %d(cirq %d) = %d(gic %d)\n",
				     cirq_p_val, i, irq_p_val,
				     CIRQ_TO_IRQ_NUM(i));
			}
			if (!first_cirq_found) {
				first_flushed_cirq = i;
				first_irq_flushedto = irq_p;
				first_cirq_found = 1;
			}
			last_fluashed_cirq = i;
			last_irq_flushedto = irq_p;
		}
	}

	if (cirq_clone_flush_check_val == 1) {
		if (first_cirq_found) {
			pr_debug("[CIRQ] The first flush : CIRQ%d to IRQ%d\n",
				  first_flushed_cirq, first_irq_flushedto);
			pr_debug("[CIRQ] The last flush : CIRQ%d to IRQ%d\n",
				  last_fluashed_cirq, last_irq_flushedto);
		} else {
			pr_debug
			    ("[CIRQ] There are no pending interrupt in CIRQ");
			pr_debug("so no flush operation happened\n");
		}
		pr_debug
	("[CIRQ] The Flush Max Range : CIRQ%d to IRQ%d ~ CIRQ%d to IRQ%d\n",
		     0, CIRQ_TO_IRQ_NUM(0), CIRQ_IRQ_NUM - 1,
		     CIRQ_TO_IRQ_NUM(CIRQ_IRQ_NUM - 1));
		pr_debug
		    ("[CIRQ] Flush Check %s, Confirm:SPI_START_OFFSET:%d\n",
		     pass == 1 ? "Pass" : "Failed", CIRQ_SPI_START);
	}
	mt_cirq_mask_all();
	mt_cirq_ack_all();
#endif

#ifdef LATENCY_CHECK
	flush_t2 = sched_clock();
	pr_notice("[CIRQ] clone takes %llu ns\n", clone_t2 - clone_t1);
	pr_notice("[CIRQ] flush takes %llu ns\n", flush_t2 - flush_t1);
#endif

	return;

}
EXPORT_SYMBOL(mt_cirq_flush);

/*
 * mt_cirq_clone_pol: Copy the polarity setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_pol(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;


	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 32 == 0)
			st = mt_irq_get_pol_hw(irq_num);

		bit = 0x1 << ((irq_num - GIC_PRIVATE_SIGNALS) % 32);

		if (st & bit)
			mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_NEG);
		else
			mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_POS);
	}
}

/*
 * mt_cirq_clone_sens: Copy the sensitivity setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_sens(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;

	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 16 == 0) {
			st = readl(IOMEM
				   (GIC_DIST_BASE + GIC_DIST_CONFIG +
				    (irq_num / 16 * 4)));
		}

		bit = 0x2 << ((irq_num % 16) * 2);

		if (st & bit)
			mt_cirq_set_sens(cirq_num, MT_EDGE_SENSITIVE);
		else
			mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	}
}

/*
 * mt_cirq_clone_mask: Copy the mask setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_mask(void)
{
	unsigned int cirq_num, irq_num;
	unsigned int st;
	unsigned int bit;


	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		irq_num = CIRQ_TO_IRQ_NUM(cirq_num);

		if (cirq_num == 0 || irq_num % 32 == 0) {
			st = readl(IOMEM
				   (GIC_DIST_BASE + GIC_DIST_ENABLE_SET +
				    (irq_num / 32 * 4)));
		}

		bit = 0x1 << (irq_num % 32);

		if (st & bit)
			mt_cirq_unmask(cirq_num);
		else
			mt_cirq_mask(cirq_num);
	}
}

#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
#ifdef FAST_CIRQ_DEBUG
static void dump_cirq_reg(struct cirq_reg *r)
{
	pr_info("[CIRQ] reg_num:%d, used:%d, m:0x%x, ",
		r->reg_num, r->used, r->mask);
	pr_info("p:0x%x, s:0x%x, pend:0x%lx, prev:%p,",
		r->pol, r->sen, r->pending,
		r->the_link.prev);
	pr_info("next:%p\n", r->the_link.next);
}
static void dump_cirq_events_mgr(struct cirq_events *events)
{
	int i;
	struct list_head *cur;
	struct cirq_reg *event;

	if (events->num_of_events > 0) {
		pr_info("[CIRQ]num of source %d",
			events->num_of_events);
		for (i = 0; i < events->num_of_events; i++)
			pr_info(", %d",
				events->wakeup_events[i]);
		pr_info("\n");
	}

	if (events->table != 0) {
		for (i = 0; i < events->num_reg; i++)
			dump_cirq_reg(&events->table[i]);
	}

	if (events->used_reg_head.next != &events->used_reg_head) {
		list_for_each(cur, &events->used_reg_head) {
			event = list_entry(cur, struct cirq_reg, the_link);
			dump_cirq_reg(event);
		}
	}

}
#endif

static int setup_cirq_settings(void)
{
	cirq_all_events.num_reg = (CIRQ_IRQ_NUM >> 5) + 1;
	cirq_all_events.spi_start = CIRQ_SPI_START;
	INIT_LIST_HEAD(&cirq_all_events.used_reg_head);
	cirq_all_events.table =
		kcalloc(cirq_all_events.num_reg,
			sizeof(struct cirq_reg), GFP_KERNEL);
	if (cirq_all_events.table == NULL) {
		pr_info("[CIRQ] failed to alloc table\n");
		return -ENOSPC;
	}
	cirq_all_events.cirq_base = SYS_CIRQ_BASE;
	cirq_all_events.dist_base = get_dist_base();
	if (cirq_all_events.dist_base == NULL) {
		pr_info("[CIRQ] get dist base failed\n");
		return -ENXIO;
	}
	mt_cirq_mask_all();

	return 0;
}

void set_wakeup_sources(u32 *list, u32 num_of_events)
{
	cirq_all_events.num_of_events = num_of_events;
	cirq_all_events.wakeup_events = list;
}
EXPORT_SYMBOL(set_wakeup_sources);

static void collect_all_wakeup_events(void)
{
	unsigned int i;
	unsigned int gic_irq;
	unsigned int cirq;
	unsigned int cirq_reg;
	unsigned int cirq_offset;
	unsigned int mask;
	unsigned int pol_mask;
	unsigned int irq_offset;
	unsigned int irq_mask;

	if (cirq_all_events.wakeup_events == NULL ||
			cirq_all_events.num_of_events == 0)
		return;
	for (i = 0; i < cirq_all_events.num_of_events; i++) {
		if (cirq_all_events.wakeup_events[i] > 0) {
			unsigned int w = cirq_all_events.wakeup_events[i];

			gic_irq = virq_to_hwirq(w);
			cirq = gic_irq - cirq_all_events.spi_start -
			       GIC_PRIVATE_SIGNALS;
			cirq_reg = cirq / 32;
			cirq_offset = cirq % 32;
			mask = 0x1 << cirq_offset;
			irq_offset = gic_irq % 32;
			irq_mask = 0x1 << irq_offset;
			/*
			 * CIRQ default masks all
			 */
			cirq_all_events.table[cirq_reg].mask |= mask;
			/*
			 * CIRQ default pol is low
			 */
			pol_mask = mt_irq_get_pol(
					cirq_all_events.wakeup_events[i])
					& irq_mask;
			if (pol_mask == 0)
				cirq_all_events.table[cirq_reg].pol |= mask;
			/*
			 * CIRQ only monitor edge trigger
			 */
			cirq_all_events.table[cirq_reg].sen |= mask;

			if (!cirq_all_events.table[cirq_reg].used) {
				list_add(
				    &cirq_all_events.table[cirq_reg].the_link,
				    &cirq_all_events.used_reg_head);
				cirq_all_events.table[cirq_reg].used = 1;
				cirq_all_events.table[cirq_reg].reg_num =
								cirq_reg;
			}
		}
	}
}


#ifdef FAST_CIRQ_DEBUG
void debug_setting_dump(void)
{

	struct list_head *cur;
	struct cirq_reg *event;

	list_for_each(cur, &cirq_all_events.used_reg_head) {
		event = list_entry(cur, struct cirq_reg, the_link);
		pr_info("[CIRQ] reg%d,  write cirq pol 0x%x, sen 0x%x, mask 0x%x",
			 event->reg_num, event->pol, event->sen, event->mask);
		pr_info("[CIRQ] &%p = 0x%x, &%p = 0x%x, &%p = 0x%x\n",
			CIRQ_POL_SET_BASE + (event->reg_num << 2),
			readl(CIRQ_POL_BASE + (event->reg_num << 2)),
			CIRQ_SENS_CLR_BASE + (event->reg_num << 2),
			readl(CIRQ_SENS_BASE + (event->reg_num << 2)),
			CIRQ_MASK_CLR_BASE + (event->reg_num << 2),
			readl(CIRQ_MASK_BASE + (event->reg_num << 2)));
		pr_info("[CIRQ] CIRQ CON &%p = 0x%x\n",
			CIRQ_CON, readl(CIRQ_CON));
	}
}
EXPORT_SYMBOL(debug_setting_dump);
#endif


static void __cirq_fast_clone(void)
{
	struct list_head *cur;
	struct cirq_reg *event;
	unsigned int cirq_id;
	unsigned int irq_id;
	unsigned int pol, en;
	unsigned int bit;
	unsigned int cur_bit;

	list_for_each(cur, &cirq_all_events.used_reg_head) {
		event = list_entry(cur, struct cirq_reg, the_link);
		writel(event->sen, CIRQ_SENS_CLR_BASE + (event->reg_num << 2));

		for_each_set_bit(cur_bit, (unsigned long *) &event->mask, 32) {
			cirq_id = (event->reg_num << 5) + cur_bit;
#ifdef FAST_CIRQ_DEBUG
			pr_info("[CIRQ] reg_num: %d, bit:%d, cirq_id %d\n",
				event->reg_num, cur_bit, cirq_id);
#endif
			irq_id = CIRQ_TO_IRQ_NUM(cirq_id);
			bit = 0x1 << ((irq_id - GIC_PRIVATE_SIGNALS) % 32);
			pol = mt_irq_get_pol_hw(irq_id) & bit;
			if (pol)
				mt_cirq_set_pol(cirq_id, MT_CIRQ_POL_NEG);
			else
				mt_cirq_set_pol(cirq_id, MT_CIRQ_POL_POS);

			en = mt_irq_get_en_hw(irq_id);
			if (en)
				mt_cirq_unmask(cirq_id);
			else
				mt_cirq_mask(cirq_id);
#ifdef FAST_CIRQ_DEBUG
			pr_info("[CIRQ] c:%d,i:%d, irq pol:%d,m:%d\n",
				cirq_id, irq_id, pol, en);
#endif
		}
	}
}

static void cirq_fast_clone(void)
{
	if (!already_cloned) {
		collect_all_wakeup_events();
		already_cloned = 1;
	}
	__cirq_fast_clone();
#ifdef FAST_CIRQ_DEBUG
	debug_setting_dump();
#endif
}


static void cirq_fast_sw_flush(void)
{
	struct list_head *cur;
	struct cirq_reg *event;
	unsigned int cur_bit;
	unsigned int cirq_id;

	list_for_each(cur, &cirq_all_events.used_reg_head) {
		event = list_entry(cur, struct cirq_reg, the_link);
		event->pending = readl(CIRQ_STA_BASE + (event->reg_num << 2));

		if (event->pending == 0)
			continue;

		/*
		 * We mask the enable mask to guarantee that
		 * we only flush the wakeup sources.
		 */
		event->pending &= event->mask;
		for_each_set_bit(cur_bit,
				(unsigned long *) &event->pending, 32) {
			cirq_id = (event->reg_num << 5) + cur_bit;
#ifdef FAST_CIRQ_DEBUG
			pr_debug("[CIRQ] reg%d, curbit=%d, fcirq=%d, mask=0x%x\n",
				event->reg_num, cur_bit, cirq_id, event->mask);
#endif
			mt_irq_set_pending_hw(CIRQ_TO_IRQ_NUM(cirq_id));
		}
	}
}

#endif


/*
 * mt_cirq_clone_gic: Copy the setting from GIC to SYS_CIRQ
 */
void mt_cirq_clone_gic(void)
{
#ifdef LATENCY_CHECK
	clone_t1 = sched_clock();
#endif
#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	cirq_fast_clone();
#else
	mt_cirq_clone_pol();
	mt_cirq_clone_sens();
	mt_cirq_clone_mask();
	if (cirq_clone_flush_check_val)
		mt_cirq_dump_reg();
#endif

#ifdef LATENCY_CHECK
	clone_t2 = sched_clock();
#endif
}
EXPORT_SYMBOL(mt_cirq_clone_gic);


#if defined(LDVT)
/*
 * cirq_dvt_show: To show usage.
 */
static ssize_t cirq_dvt_show(struct device_driver *driver, char *buf)
{
	const char *list = "1.regs\n2.tests\n3.disable\n";

	return snprintf(buf, PAGE_SIZE, list);
}

/*
 * mci_dvt_store: To select mci test case.
 */
static ssize_t cirq_dvt_store(struct device_driver *driver, const char *buf,
			      size_t count)
{
	char *p = (char *)buf;
	unsigned long num;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&num);

	switch (num) {
	case 1:
		mt_cirq_clone_gic();
		mt_cirq_dump_reg();
		break;
	case 2:
		mt_cirq_test();
		break;
	case 3:
		mt_cirq_disable();
		break;
	default:
		break;
	}

	return count;
}

DRIVER_ATTR_RW(cirq_dvt);
#endif

/*
 * cirq_clone_flush_check_show:
 * To show if we do cirq clone/flush value's check.
 */
static ssize_t cirq_clone_flush_check_show(struct device_driver *driver,
					   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n",
			cirq_clone_flush_check_val);
}

/*
 * cirq_clone_flush_check_store:
 * set 1 if we need to enable clone/flush value's check
 */
static ssize_t cirq_clone_flush_check_store(struct device_driver *driver,
					    const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_clone_flush_check_val = value;
	return count;
}

DRIVER_ATTR_RW(cirq_clone_flush_check);

/*
 * cirq_pattern_clone_flush_check_show:
 * To show if we do need to do pattern test.
 */
static ssize_t cirq_pattern_clone_flush_check_show(struct device_driver *driver,
						   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n",
			cirq_pattern_clone_flush_check_val);
}

/*
 * cirq_pattern_clone_flush_check_show:  set 1 if we need to do pattern test.
 */
static ssize_t cirq_pattern_clone_flush_check_store(struct device_driver
						    *driver, const char *buf,
						    size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_pattern_clone_flush_check_val = value;
	return count;
}

DRIVER_ATTR_RW(cirq_pattern_clone_flush_check);

/*
 * cirq_pattern_clone_flush_check_show:
 * To show if we do need to do pattern test.
 */
static ssize_t cirq_pattern_list_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%ld\n", cirq_pattern_list);
}

/*
 * cirq_pattern_clone_flush_check_show:  set 1 if we need to do pattern test.
 */
static ssize_t cirq_pattern_list_store(struct device_driver *driver,
				       const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned long value;
	int rc;

	rc = kstrtoul(p, 10, (unsigned long *)&value);
	cirq_pattern_list = value;
	return count;
}

DRIVER_ATTR_RW(cirq_pattern_list);

#if defined(__CHECK_IRQ_TYPE)
#define X_DEFINE_IRQ(__name, __num, __polarity, __sensitivity) \
	{ .num = __num, .polarity = __polarity, .sensitivity = __sensitivity, },
#define L 0
#define H 1
#define EDGE MT_EDGE_SENSITIVE
#define LEVEL MT_LEVEL_SENSITIVE
struct __check_irq_type {
	int num;
	int polarity;
	int sensitivity;
};
#undef __X_DEFINE_IRQ
struct __check_irq_type __check_irq_type[] = {
#include <x_define_irq.h>
	{.num = -1,},
};

#undef X_DEFINE_IRQ
#undef L
#undef H
#undef EDGE
#undef LEVEL
#endif

void mt_cirq_dump_reg(void)
{
	int cirq_num;
	int pol, sens, mask;
	int pen;
#if defined(__CHECK_IRQ_TYPE)
	int irq_iter;
	unsigned char pass = 1;
#endif

	pr_debug("[CIRQ] IRQ:\tPOL\tSENS\tMASK\tGIC_PENDING\n");
	for (cirq_num = 0; cirq_num < CIRQ_IRQ_NUM; cirq_num++) {
		pol = mt_cirq_get_pol(cirq_num);
		sens = mt_cirq_get_sens(cirq_num);
		mask = mt_cirq_get_mask(cirq_num);
		pen =  mt_irq_get_pending_hw(CIRQ_TO_IRQ_NUM(cirq_num));
#if defined(__CHECK_IRQ_TYPE)
		if (mask == 0) {
			pr_debug("[CIRQ] IRQ:%d\t%d\t%d\t%d\t%d\n",
				 CIRQ_TO_IRQ_NUM(cirq_num),
				 pol,
				 sens,
				 mask,
				 pen);
			irq_iter = cirq_num + CIRQ_SPI_START;
			if (__check_irq_type[irq_iter].num ==
			    CIRQ_TO_IRQ_NUM(cirq_num)) {
				if (__check_irq_type[irq_iter].sensitivity !=
				    sens) {
					pr_debug
					    ("[CIRQ] Error sens in irq:%d\n",
					     __check_irq_type[irq_iter].num);
					pass = 0;
				}
				if (__check_irq_type[irq_iter].polarity
					!= pol) {
					pr_debug
					    ("[CIRQ]Err polarity in irq:%d\n",
					     __check_irq_type[irq_iter].num);
					pass = 0;
				}
			} else {
				pr_debug
				    ("[CIRQ] Error CIRQ num %d",
					__check_irq_type[irq_iter].num);
				pr_debug("Mapping to wrong GIC num %d\n",
					CIRQ_TO_IRQ_NUM(cirq_num));
				pass = 0;
			}
		}
#endif
	}
#if defined __CHECK_IRQ_TYPE
	pr_debug("[CIRQ] CIRQ Clone To GIC Verfication %s !\n",
		  pass == 1 ? "Pass" : "Failed");
#else
	pr_debug
	    ("[CIRQ] Plz enable __CHECK_IRQ_TYE");
	pr_debug("and update x_define.h for enable CIRQ Clone checking\n");
#endif
}

#ifdef LDVT
int mt_cirq_test(void)
{
	int cirq_num = 126;


	/*test polarity */
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_NEG);
	if (mt_cirq_get_pol(cirq_num) != MT_CIRQ_POL_NEG)
		pr_debug("[CIRQ] mt_cirq_set_pol clear test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_pol clear test passed!!\n");
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_POS);
	if (mt_cirq_get_pol(cirq_num) != MT_CIRQ_POL_POS)
		pr_debug("[CIRQ] mt_cirq_set_pol set test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_pol set test passed!!\n");

	/*test sensitivity */
	mt_cirq_set_sens(cirq_num, MT_EDGE_SENSITIVE);
	if (mt_cirq_get_sens(cirq_num) != MT_EDGE_SENSITIVE)
		pr_debug("[CIRQ] mt_cirq_set_sens clear test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_sens clear test passed!!\n");
	mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	if (mt_cirq_get_sens(cirq_num) != MT_LEVEL_SENSITIVE)
		pr_debug("[CIRQ] mt_cirq_set_sens set test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_set_sens set test passed!!\n");

	/*test mask */
	mt_cirq_mask(cirq_num);
	if (mt_cirq_get_mask(cirq_num) != 1)
		pr_debug("[CIRQ] mt_cirq_mask test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_mask test passed!!\n");
	mt_cirq_unmask(cirq_num);
	if (mt_cirq_get_mask(cirq_num) != 0)
		pr_debug("[CIRQ] mt_cirq_unmask test failed!!\n");
	else
		pr_debug("[CIRQ] mt_cirq_unmask test passed!!\n");

	mt_cirq_clone_gic();
	mt_cirq_dump_reg();

	return 0;
}
#endif

/*
 * cirq_irq_handler: SYS_CIRQ interrupt service routine.
 */
static irqreturn_t cirq_irq_handler(int irq, void *dev_id)
{
	pr_debug("[CIRQ] CIRQ_Handler\n");

	mt_cirq_ack_all();

	return IRQ_HANDLED;
}

/*
 * mt_cirq_init: SYS_CIRQ init function
 * always return 0
 */
int __init mt_cirq_init(void)
{
	int ret;
#ifdef CONFIG_OF
	struct device_node *node;
	unsigned int sys_cirq_num = 0;
#endif

	pr_debug("[CIRQ] CIRQ init...\n");

#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "mediatek,sys_cirq");
	if (!node) {
		pr_debug("[CIRQ] find SYS_CIRQ node failed!!!\n");
		return -1;
	}
	SYS_CIRQ_BASE = of_iomap(node, 0);
	pr_debug("[CIRQ] SYS_CIRQ_BASE = 0x%p\n", SYS_CIRQ_BASE);
	WARN(!SYS_CIRQ_BASE,
			"[CIRQ] unable to map SYS_CIRQ base registers!!!\n");

	if (of_property_read_u32(node, "mediatek,cirq_num", &CIRQ_IRQ_NUM))
		return -1;
	pr_debug("[CIRQ] cirq_num = %d\n", CIRQ_IRQ_NUM);

	if (of_property_read_u32(node, "mediatek,spi_start_offset",
				&CIRQ_SPI_START))
		return -1;
	pr_debug("[CIRQ] spi_start_offset = %d\n", CIRQ_SPI_START);

	sys_cirq_num = irq_of_parse_and_map(node, 0);
	pr_debug("[CIRQ] sys_cirq_num = %d\n", sys_cirq_num);

	if (of_property_read_u32(node, "sw_reset", &sw_reset))
		sw_reset = 0;
	pr_debug("[CIRQ] sw_reset = %d\n", sw_reset);
#endif

#ifdef CONFIG_OF
	ret =
		request_irq(sys_cirq_num, cirq_irq_handler, IRQF_TRIGGER_NONE,
				"CIRQ", NULL);
#else
	ret =
		request_irq(SYS_CIRQ_IRQ_BIT_ID, cirq_irq_handler,
				IRQF_TRIGGER_LOW, "CIRQ", NULL);
#endif

	if (ret > 0)
		pr_debug("[CIRQ] CIRQ IRQ LINE NOT AVAILABLE!!\n");
	else
		pr_debug("[CIRQ] CIRQ handler init success.\n");

	ret = platform_driver_register(&mt_cirq_drv.driver);
	if (ret == 0)
		pr_debug("[CIRQ] CIRQ init done...\n");

#ifdef LDVT
	ret = driver_create_file(&mt_cirq_drv.driver.driver,
					&driver_attr_cirq_dvt);
	if (ret == 0)
		pr_debug("[CIRQ] CIRQ create sysfs file for dvt done...\n");
#endif

	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_clone_flush_check);
	if (ret == 0)
		pr_debug
		("[CIRQ] sysfs file for cirq clone flush check done...\n");

	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_pattern_clone_flush_check);
	if (ret == 0)
		pr_debug
		("[CIRQ] sysfs file for pattern clone flush check done...\n");
	cirq_pattern_list = CIRQ_IRQ_NUM;
	ret = driver_create_file(&mt_cirq_drv.driver.driver,
			&driver_attr_cirq_pattern_list);
	if (ret == 0)
		pr_debug
		("[CIRQ] CIRQ create sysfs file for pattern list setup...\n");


#ifdef CONFIG_FAST_CIRQ_CLONE_FLUSH
	setup_cirq_settings();
	pr_debug("[CIRQ] cirq wakeup source structure init done\n");
	pr_debug("[CIRQ] dump init events\n");
#ifdef FAST_CIRQ_DEBUG
	dump_cirq_events_mgr(&cirq_all_events);
#endif
#endif
	pr_debug("### CIRQ init done. ###\n");

	return 0;
}

arch_initcall(mt_cirq_init);
