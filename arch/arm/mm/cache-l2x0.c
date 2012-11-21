/*
 * arch/arm/mm/cache-l2x0.c - L210/L220 cache controller support
 *
 * Copyright (C) 2007 ARM Limited
 * Copyright (c) 2009, 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/irq_regs.h>
#include <asm/pmu.h>

#define CACHE_LINE_SIZE		32

static void __iomem *l2x0_base;
static DEFINE_RAW_SPINLOCK(l2x0_lock);
static u32 l2x0_way_mask;	/* Bitmask of active ways */
static u32 l2x0_size;
static u32 l2x0_cache_id;
static unsigned int l2x0_sets;
static unsigned int l2x0_ways;
static unsigned long sync_reg_offset = L2X0_CACHE_SYNC;
static void pl310_save(void);
static void pl310_resume(void);
static void l2x0_resume(void);

static inline bool is_pl310_rev(int rev)
{
	return (l2x0_cache_id &
		(L2X0_CACHE_ID_PART_MASK | L2X0_CACHE_ID_REV_MASK)) ==
			(L2X0_CACHE_ID_PART_L310 | rev);
}

struct l2x0_regs l2x0_saved_regs;

struct l2x0_of_data {
	void (*setup)(const struct device_node *, u32 *, u32 *);
	void (*save)(void);
	void (*resume)(void);
};

static inline void cache_wait_way(void __iomem *reg, unsigned long mask)
{
	/* wait for cache operation by line or way to complete */
	while (readl_relaxed(reg) & mask)
		cpu_relax();
}

#ifdef CONFIG_CACHE_PL310
static inline void cache_wait(void __iomem *reg, unsigned long mask)
{
	/* cache operations by line are atomic on PL310 */
}
#else
#define cache_wait	cache_wait_way
#endif

static inline void cache_sync(void)
{
	void __iomem *base = l2x0_base;

	writel_relaxed(0, base + sync_reg_offset);
	cache_wait(base + L2X0_CACHE_SYNC, 1);
}

static inline void l2x0_clean_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_LINE_PA);
}

static inline void l2x0_inv_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_INV_LINE_PA);
}

#if defined(CONFIG_PL310_ERRATA_588369) || defined(CONFIG_PL310_ERRATA_727915)
static inline void debug_writel(unsigned long val)
{
	if (outer_cache.set_debug)
		outer_cache.set_debug(val);
}

static void pl310_set_debug(unsigned long val)
{
	writel_relaxed(val, l2x0_base + L2X0_DEBUG_CTRL);
}
#else
/* Optimised out for non-errata case */
static inline void debug_writel(unsigned long val)
{
}

#define pl310_set_debug	NULL
#endif

#ifdef CONFIG_PL310_ERRATA_588369
static inline void l2x0_flush_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;

	/* Clean by PA followed by Invalidate by PA */
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_LINE_PA);
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_INV_LINE_PA);
}
#else

static inline void l2x0_flush_line(unsigned long addr)
{
	void __iomem *base = l2x0_base;
	cache_wait(base + L2X0_CLEAN_INV_LINE_PA, 1);
	writel_relaxed(addr, base + L2X0_CLEAN_INV_LINE_PA);
}
#endif

void l2x0_cache_sync(void)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2x0_lock, flags);
	cache_sync();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

#ifdef CONFIG_PL310_ERRATA_727915
static void l2x0_for_each_set_way(void __iomem *reg)
{
	int set;
	int way;
	unsigned long flags;

	for (way = 0; way < l2x0_ways; way++) {
		raw_spin_lock_irqsave(&l2x0_lock, flags);
		for (set = 0; set < l2x0_sets; set++)
			writel_relaxed((way << 28) | (set << 5), reg);
		cache_sync();
		raw_spin_unlock_irqrestore(&l2x0_lock, flags);
	}
}
#endif

static void __l2x0_flush_all(void)
{
	debug_writel(0x03);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_CLEAN_INV_WAY);
	cache_wait_way(l2x0_base + L2X0_CLEAN_INV_WAY, l2x0_way_mask);
	cache_sync();
	debug_writel(0x00);
}

static void l2x0_flush_all(void)
{
	unsigned long flags;

#ifdef CONFIG_PL310_ERRATA_727915
	if (is_pl310_rev(REV_PL310_R2P0)) {
		l2x0_for_each_set_way(l2x0_base + L2X0_CLEAN_INV_LINE_IDX);
		return;
	}
#endif

	/* clean all ways */
	raw_spin_lock_irqsave(&l2x0_lock, flags);
	__l2x0_flush_all();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_clean_all(void)
{
	unsigned long flags;

#ifdef CONFIG_PL310_ERRATA_727915
	if (is_pl310_rev(REV_PL310_R2P0)) {
		l2x0_for_each_set_way(l2x0_base + L2X0_CLEAN_LINE_IDX);
		return;
	}
#endif

	/* clean all ways */
	raw_spin_lock_irqsave(&l2x0_lock, flags);
	debug_writel(0x03);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_CLEAN_WAY);
	cache_wait_way(l2x0_base + L2X0_CLEAN_WAY, l2x0_way_mask);
	cache_sync();
	debug_writel(0x00);
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_inv_all(void)
{
	unsigned long flags;

	/* invalidate all ways */
	raw_spin_lock_irqsave(&l2x0_lock, flags);
	/* Invalidating when L2 is enabled is a nono */
	BUG_ON(readl(l2x0_base + L2X0_CTRL) & 1);
	writel_relaxed(l2x0_way_mask, l2x0_base + L2X0_INV_WAY);
	cache_wait_way(l2x0_base + L2X0_INV_WAY, l2x0_way_mask);
	cache_sync();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_inv_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;

	raw_spin_lock_irqsave(&l2x0_lock, flags);
	if (start & (CACHE_LINE_SIZE - 1)) {
		start &= ~(CACHE_LINE_SIZE - 1);
		debug_writel(0x03);
		l2x0_flush_line(start);
		debug_writel(0x00);
		start += CACHE_LINE_SIZE;
	}

	if (end & (CACHE_LINE_SIZE - 1)) {
		end &= ~(CACHE_LINE_SIZE - 1);
		debug_writel(0x03);
		l2x0_flush_line(end);
		debug_writel(0x00);
	}

	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		while (start < blk_end) {
			l2x0_inv_line(start);
			start += CACHE_LINE_SIZE;
		}

		if (blk_end < end) {
			raw_spin_unlock_irqrestore(&l2x0_lock, flags);
			raw_spin_lock_irqsave(&l2x0_lock, flags);
		}
	}
	cache_wait(base + L2X0_INV_LINE_PA, 1);
	cache_sync();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_clean_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;

	if ((end - start) >= l2x0_size) {
		l2x0_clean_all();
		return;
	}

	raw_spin_lock_irqsave(&l2x0_lock, flags);
	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		while (start < blk_end) {
			l2x0_clean_line(start);
			start += CACHE_LINE_SIZE;
		}

		if (blk_end < end) {
			raw_spin_unlock_irqrestore(&l2x0_lock, flags);
			raw_spin_lock_irqsave(&l2x0_lock, flags);
		}
	}
	cache_wait(base + L2X0_CLEAN_LINE_PA, 1);
	cache_sync();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_flush_range(unsigned long start, unsigned long end)
{
	void __iomem *base = l2x0_base;
	unsigned long flags;

	if ((end - start) >= l2x0_size) {
		l2x0_flush_all();
		return;
	}

	raw_spin_lock_irqsave(&l2x0_lock, flags);
	start &= ~(CACHE_LINE_SIZE - 1);
	while (start < end) {
		unsigned long blk_end = start + min(end - start, 4096UL);

		debug_writel(0x03);
		while (start < blk_end) {
			l2x0_flush_line(start);
			start += CACHE_LINE_SIZE;
		}
		debug_writel(0x00);

		if (blk_end < end) {
			raw_spin_unlock_irqrestore(&l2x0_lock, flags);
			raw_spin_lock_irqsave(&l2x0_lock, flags);
		}
	}
	cache_wait(base + L2X0_CLEAN_INV_LINE_PA, 1);
	cache_sync();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_disable(void)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2x0_lock, flags);
	__l2x0_flush_all();
	writel_relaxed(0, l2x0_base + L2X0_CTRL);
	dsb();
	raw_spin_unlock_irqrestore(&l2x0_lock, flags);
}

static void l2x0_unlock(u32 cache_id)
{
	int lockregs;
	int i;

	if (cache_id == L2X0_CACHE_ID_PART_L310)
		lockregs = 8;
	else
		/* L210 and unknown types */
		lockregs = 1;

	for (i = 0; i < lockregs; i++) {
		writel_relaxed(0x0, l2x0_base + L2X0_LOCKDOWN_WAY_D_BASE +
			       i * L2X0_LOCKDOWN_STRIDE);
		writel_relaxed(0x0, l2x0_base + L2X0_LOCKDOWN_WAY_I_BASE +
			       i * L2X0_LOCKDOWN_STRIDE);
	}
}

void __init l2x0_init(void __iomem *base, u32 aux_val, u32 aux_mask)
{
	u32 aux;
	u32 way_size = 0;
	const char *type;

	l2x0_base = base;

	l2x0_cache_id = readl_relaxed(l2x0_base + L2X0_CACHE_ID);
	aux = readl_relaxed(l2x0_base + L2X0_AUX_CTRL);

	aux &= aux_mask;
	aux |= aux_val;

	/* Determine the number of ways */
	switch (l2x0_cache_id & L2X0_CACHE_ID_PART_MASK) {
	case L2X0_CACHE_ID_PART_L310:
		if (aux & (1 << 16))
			l2x0_ways = 16;
		else
			l2x0_ways = 8;
		type = "L310";
#ifdef CONFIG_PL310_ERRATA_753970
		/* Unmapped register. */
		sync_reg_offset = L2X0_DUMMY_REG;
#endif
		outer_cache.set_debug = pl310_set_debug;
		outer_cache.resume = pl310_resume;
		break;
	case L2X0_CACHE_ID_PART_L210:
		l2x0_ways = (aux >> 13) & 0xf;
		type = "L210";
		outer_cache.resume = l2x0_resume;
		break;
	default:
		/* Assume unknown chips have 8 ways */
		l2x0_ways = 8;
		type = "L2x0 series";
		outer_cache.resume = l2x0_resume;
		break;
	}

	l2x0_way_mask = (1 << l2x0_ways) - 1;

	/*
	 * L2 cache Size =  Way size * Number of ways
	 */
	way_size = (aux & L2X0_AUX_CTRL_WAY_SIZE_MASK) >> 17;
	way_size = SZ_1K << (way_size + 3);
	l2x0_size = l2x0_ways * way_size;
	l2x0_sets = way_size / CACHE_LINE_SIZE;

	/*
	 * Check if l2x0 controller is already enabled.
	 * If you are booting from non-secure mode
	 * accessing the below registers will fault.
	 */
	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {
		/* Make sure that I&D is not locked down when starting */
		l2x0_unlock(l2x0_cache_id);

		/* l2x0 controller is disabled */
		writel_relaxed(aux, l2x0_base + L2X0_AUX_CTRL);

		l2x0_saved_regs.aux_ctrl = aux;

		l2x0_inv_all();

		/* enable L2X0 */
		writel_relaxed(1, l2x0_base + L2X0_CTRL);
	}

		outer_cache.inv_range = l2x0_inv_range;
		outer_cache.clean_range = l2x0_clean_range;
		outer_cache.flush_range = l2x0_flush_range;
	outer_cache.sync = l2x0_cache_sync;
	outer_cache.flush_all = l2x0_flush_all;
	outer_cache.inv_all = l2x0_inv_all;
	outer_cache.disable = l2x0_disable;

	printk(KERN_INFO "%s cache controller enabled\n", type);
	printk(KERN_INFO "l2x0: %d ways, CACHE_ID 0x%08x, AUX_CTRL 0x%08x, Cache size: %d B\n",
			l2x0_ways, l2x0_cache_id, aux, l2x0_size);

	/* Save the L2X0 contents, as they are not modified else where */
	pl310_save();
}

#ifdef CONFIG_OF
static void __init l2x0_of_setup(const struct device_node *np,
				 u32 *aux_val, u32 *aux_mask)
{
	u32 data[2] = { 0, 0 };
	u32 tag = 0;
	u32 dirty = 0;
	u32 val = 0, mask = 0;

	of_property_read_u32(np, "arm,tag-latency", &tag);
	if (tag) {
		mask |= L2X0_AUX_CTRL_TAG_LATENCY_MASK;
		val |= (tag - 1) << L2X0_AUX_CTRL_TAG_LATENCY_SHIFT;
	}

	of_property_read_u32_array(np, "arm,data-latency",
				   data, ARRAY_SIZE(data));
	if (data[0] && data[1]) {
		mask |= L2X0_AUX_CTRL_DATA_RD_LATENCY_MASK |
			L2X0_AUX_CTRL_DATA_WR_LATENCY_MASK;
		val |= ((data[0] - 1) << L2X0_AUX_CTRL_DATA_RD_LATENCY_SHIFT) |
		       ((data[1] - 1) << L2X0_AUX_CTRL_DATA_WR_LATENCY_SHIFT);
	}

	of_property_read_u32(np, "arm,dirty-latency", &dirty);
	if (dirty) {
		mask |= L2X0_AUX_CTRL_DIRTY_LATENCY_MASK;
		val |= (dirty - 1) << L2X0_AUX_CTRL_DIRTY_LATENCY_SHIFT;
	}

	*aux_val &= ~mask;
	*aux_val |= val;
	*aux_mask &= ~mask;
}

static void __init pl310_of_setup(const struct device_node *np,
				  u32 *aux_val, u32 *aux_mask)
{
	u32 data[3] = { 0, 0, 0 };
	u32 tag[3] = { 0, 0, 0 };
	u32 filter[2] = { 0, 0 };

	of_property_read_u32_array(np, "arm,tag-latency", tag, ARRAY_SIZE(tag));
	if (tag[0] && tag[1] && tag[2])
		writel_relaxed(
			((tag[0] - 1) << L2X0_LATENCY_CTRL_RD_SHIFT) |
			((tag[1] - 1) << L2X0_LATENCY_CTRL_WR_SHIFT) |
			((tag[2] - 1) << L2X0_LATENCY_CTRL_SETUP_SHIFT),
			l2x0_base + L2X0_TAG_LATENCY_CTRL);

	of_property_read_u32_array(np, "arm,data-latency",
				   data, ARRAY_SIZE(data));
	if (data[0] && data[1] && data[2])
		writel_relaxed(
			((data[0] - 1) << L2X0_LATENCY_CTRL_RD_SHIFT) |
			((data[1] - 1) << L2X0_LATENCY_CTRL_WR_SHIFT) |
			((data[2] - 1) << L2X0_LATENCY_CTRL_SETUP_SHIFT),
			l2x0_base + L2X0_DATA_LATENCY_CTRL);

	of_property_read_u32_array(np, "arm,filter-ranges",
				   filter, ARRAY_SIZE(filter));
	if (filter[1]) {
		writel_relaxed(ALIGN(filter[0] + filter[1], SZ_1M),
			       l2x0_base + L2X0_ADDR_FILTER_END);
		writel_relaxed((filter[0] & ~(SZ_1M - 1)) | L2X0_ADDR_FILTER_EN,
			       l2x0_base + L2X0_ADDR_FILTER_START);
	}
}
#endif

static void pl310_save(void)
{
	u32 l2x0_revision = readl_relaxed(l2x0_base + L2X0_CACHE_ID) &
		L2X0_CACHE_ID_RTL_MASK;

	l2x0_saved_regs.tag_latency = readl_relaxed(l2x0_base +
		L2X0_TAG_LATENCY_CTRL);
	l2x0_saved_regs.data_latency = readl_relaxed(l2x0_base +
		L2X0_DATA_LATENCY_CTRL);
	l2x0_saved_regs.filter_end = readl_relaxed(l2x0_base +
		L2X0_ADDR_FILTER_END);
	l2x0_saved_regs.filter_start = readl_relaxed(l2x0_base +
		L2X0_ADDR_FILTER_START);

	if (l2x0_revision >= L2X0_CACHE_ID_RTL_R2P0) {
		/*
		 * From r2p0, there is Prefetch offset/control register
		 */
		l2x0_saved_regs.prefetch_ctrl = readl_relaxed(l2x0_base +
			L2X0_PREFETCH_CTRL);
		/*
		 * From r3p0, there is Power control register
		 */
		if (l2x0_revision >= L2X0_CACHE_ID_RTL_R3P0)
			l2x0_saved_regs.pwr_ctrl = readl_relaxed(l2x0_base +
				L2X0_POWER_CTRL);
	}
}

static void l2x0_resume(void)
{
	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {
		/* restore aux ctrl and enable l2 */
		l2x0_unlock(readl_relaxed(l2x0_base + L2X0_CACHE_ID));

		writel_relaxed(l2x0_saved_regs.aux_ctrl, l2x0_base +
			L2X0_AUX_CTRL);

		l2x0_inv_all();

		writel_relaxed(1, l2x0_base + L2X0_CTRL);
	}
}

static void pl310_resume(void)
{
	u32 l2x0_revision;

	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {
		/* restore pl310 setup */
		writel_relaxed(l2x0_saved_regs.tag_latency,
			l2x0_base + L2X0_TAG_LATENCY_CTRL);
		writel_relaxed(l2x0_saved_regs.data_latency,
			l2x0_base + L2X0_DATA_LATENCY_CTRL);
		writel_relaxed(l2x0_saved_regs.filter_end,
			l2x0_base + L2X0_ADDR_FILTER_END);
		writel_relaxed(l2x0_saved_regs.filter_start,
			l2x0_base + L2X0_ADDR_FILTER_START);

		l2x0_revision = readl_relaxed(l2x0_base + L2X0_CACHE_ID) &
			L2X0_CACHE_ID_RTL_MASK;

		if (l2x0_revision >= L2X0_CACHE_ID_RTL_R2P0) {
			writel_relaxed(l2x0_saved_regs.prefetch_ctrl,
				l2x0_base + L2X0_PREFETCH_CTRL);
			if (l2x0_revision >= L2X0_CACHE_ID_RTL_R3P0)
				writel_relaxed(l2x0_saved_regs.pwr_ctrl,
					l2x0_base + L2X0_POWER_CTRL);
		}
	}

	l2x0_resume();
}

#ifdef CONFIG_OF
static const struct l2x0_of_data pl310_data = {
	pl310_of_setup,
	pl310_save,
	pl310_resume,
};

static const struct l2x0_of_data l2x0_data = {
	l2x0_of_setup,
	NULL,
	l2x0_resume,
};

static const struct of_device_id l2x0_ids[] __initconst = {
	{ .compatible = "arm,pl310-cache", .data = (void *)&pl310_data },
	{ .compatible = "arm,l220-cache", .data = (void *)&l2x0_data },
	{ .compatible = "arm,l210-cache", .data = (void *)&l2x0_data },
	{}
};

int __init l2x0_of_init(u32 aux_val, u32 aux_mask)
{
	struct device_node *np;
	struct l2x0_of_data *data;
	struct resource res;

	np = of_find_matching_node(NULL, l2x0_ids);
	if (!np)
		return -ENODEV;

	if (of_address_to_resource(np, 0, &res))
		return -ENODEV;

	l2x0_base = ioremap(res.start, resource_size(&res));
	if (!l2x0_base)
		return -ENOMEM;

	l2x0_saved_regs.phy_base = res.start;

	data = of_match_node(l2x0_ids, np)->data;

	/* L2 configuration can only be changed if the cache is disabled */
	if (!(readl_relaxed(l2x0_base + L2X0_CTRL) & 1)) {
		if (data->setup)
			data->setup(np, &aux_val, &aux_mask);
	}

	if (data->save)
		data->save();

	l2x0_init(l2x0_base, aux_val, aux_mask);

	outer_cache.resume = data->resume;
	return 0;
}
#endif

void l2cc_suspend(void)
{
	l2x0_disable();
	dmb();
}

void l2cc_resume(void)
{
	pl310_resume();
	dmb();
}

#ifdef CONFIG_HW_PERF_EVENTS
/*
 * L220/PL310 PMU-specific functionality.
 * TODO: Put this in a separate file and get the l2x0 driver to register
 * the PMU from l2x0_{of}_init.
 */

static struct arm_pmu l2x0_pmu;

static u64 l2x0pmu_max_event_id;

static struct perf_event *events[2];
static unsigned long used_mask[BITS_TO_LONGS(2)];
static struct pmu_hw_events hw_events = {
	.events = events,
	.used_mask = used_mask,
	.pmu_lock = __RAW_SPIN_LOCK_UNLOCKED(l2x0pmu_hw_events.pmu_lock),
};

#define COUNTER_CFG_ADDR(idx)	(l2x0_base + L2X0_EVENT_CNT0_CFG - 4*idx)

#define COUNTER_CTRL_ADDR	(l2x0_base + L2X0_EVENT_CNT_CTRL)

#define COUNTER_ADDR(idx)	(l2x0_base + L2X0_EVENT_CNT0_VAL - 4*idx)

static u32 l2x0_read_intr_mask(void)
{
	return readl_relaxed(l2x0_base + L2X0_INTR_MASK);
}

static void l2x0_write_intr_mask(u32 val)
{
	writel_relaxed(val, l2x0_base + L2X0_INTR_MASK);
}

static void l2x0_enable_counter_interrupt(void)
{
	u32 intr_mask = l2x0_read_intr_mask();
	intr_mask |= L2X0_INTR_MASK_ECNTR;
	l2x0_write_intr_mask(intr_mask);
}

static void l2x0_disable_counter_interrupt(void)
{
	u32 intr_mask = l2x0_read_intr_mask();
	intr_mask &= ~L2X0_INTR_MASK_ECNTR;
	l2x0_write_intr_mask(intr_mask);
}

static void l2x0_clear_interrupts(u32 flags)
{
	writel_relaxed(flags, l2x0_base + L2X0_INTR_CLEAR);
}

static struct pmu_hw_events *l2x0pmu_get_hw_events(void)
{
	return &hw_events;
}

static u32 l2x0pmu_read_ctrl(void)
{
	return readl_relaxed(COUNTER_CTRL_ADDR);
}

static void l2x0pmu_write_ctrl(u32 val)
{
	writel_relaxed(val, COUNTER_CTRL_ADDR);
}

static u32 l2x0pmu_read_cfg(int idx)
{
	return readl_relaxed(COUNTER_CFG_ADDR(idx));
}

static void l2x0pmu_write_cfg(u32 val, int idx)
{
	writel_relaxed(val, COUNTER_CFG_ADDR(idx));
}

static void l2x0pmu_enable_counter(u32 cfg, int idx)
{
	cfg |= L2X0_EVENT_CNT_CFG_INTR_OVERFLOW;
	l2x0pmu_write_cfg(cfg, idx);
}

static u32 l2x0pmu_disable_counter(int idx)
{
	u32 cfg, oldcfg;

	cfg = oldcfg = l2x0pmu_read_cfg(idx);
	cfg &= ~L2X0_EVENT_CNT_CFG_MASK;
	cfg &= ~L2X0_EVENT_CNT_CFG_INTR_MASK;
	l2x0pmu_write_cfg(cfg, idx);

	return oldcfg;
}

static u32 l2x0pmu_read_counter(int idx)
{
	return readl_relaxed(COUNTER_ADDR(idx));
}

static void l2x0pmu_write_counter(int idx, u32 val)
{
	/*
	 * L2X0 counters can only be written to when they are disabled.
	 * As perf core does not disable counters before writing to them
	 * under interrupts, we must do so here.
	 */
	u32 cfg = l2x0pmu_disable_counter(idx);
	writel_relaxed(val, COUNTER_ADDR(idx));
	l2x0pmu_write_cfg(cfg, idx);
}

static int counter_is_saturated(int idx)
{
	return l2x0pmu_read_counter(idx) == 0xFFFFFFFF;
}

static void l2x0pmu_start(void)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&hw_events.pmu_lock, flags);

	l2x0_enable_counter_interrupt();

	val = l2x0pmu_read_ctrl();
	val |= L2X0_EVENT_CNT_ENABLE;
	l2x0pmu_write_ctrl(val);

	raw_spin_unlock_irqrestore(&hw_events.pmu_lock, flags);
}

static void l2x0pmu_stop(void)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&hw_events.pmu_lock, flags);

	val = l2x0pmu_read_ctrl();
	val &= ~L2X0_EVENT_CNT_ENABLE_MASK;
	l2x0pmu_write_ctrl(val);

	l2x0_disable_counter_interrupt();

	raw_spin_unlock_irqrestore(&hw_events.pmu_lock, flags);
}

static void l2x0pmu_enable(struct hw_perf_event *event, int idx, int cpu)
{
	unsigned long flags;
	u32 cfg;

	raw_spin_lock_irqsave(&hw_events.pmu_lock, flags);

	cfg = (event->config_base << L2X0_EVENT_CNT_CFG_SHIFT) &
						L2X0_EVENT_CNT_CFG_MASK;
	l2x0pmu_enable_counter(cfg, idx);

	raw_spin_unlock_irqrestore(&hw_events.pmu_lock, flags);
}

static void l2x0pmu_disable(struct hw_perf_event *event, int idx)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&hw_events.pmu_lock, flags);
	l2x0pmu_disable_counter(idx);
	raw_spin_unlock_irqrestore(&hw_events.pmu_lock, flags);
}

static int l2x0pmu_get_event_idx(struct pmu_hw_events *events,
					struct hw_perf_event *hwc)
{
	int idx;

	/* Counters are identical. Just grab a free one. */
	for (idx = 0; idx < L2X0_NUM_COUNTERS; ++idx) {
		if (!test_and_set_bit(idx, hw_events.used_mask))
			return idx;
	}

	return -EAGAIN;
}

/*
 * As System PMUs are affine to CPU0, the fact that interrupts are disabled
 * during interrupt handling is enough to serialise our actions and make this
 * safe. We do not need to grab our pmu_lock here.
 */
static irqreturn_t l2x0pmu_handle_irq(int irq, void *dev)
{
	irqreturn_t status = IRQ_NONE;
	struct perf_sample_data data;
	struct pt_regs *regs;
	int idx;

	regs = get_irq_regs();

	for (idx = 0; idx < L2X0_NUM_COUNTERS; ++idx) {
		struct perf_event *event = hw_events.events[idx];
		struct hw_perf_event *hwc;

		if (!counter_is_saturated(idx))
			continue;

		status = IRQ_HANDLED;

		hwc = &event->hw;

		/*
		 * The armpmu_* functions expect counters to overflow, but
		 * L220/PL310 counters saturate instead. Fake the overflow
		 * here so the hardware is in sync with what the framework
		 * expects.
		 */
		l2x0pmu_write_counter(idx, 0);

		armpmu_event_update(event, hwc, idx);
		data.period = event->hw.last_period;

		if (!armpmu_event_set_period(event, hwc, idx))
			continue;

		if (perf_event_overflow(event, &data, regs))
			l2x0pmu_disable_counter(idx);
	}

	l2x0_clear_interrupts(L2X0_INTR_MASK_ECNTR);

	irq_work_run();

	return status;
}

static int map_l2x0_raw_event(u64 config)
{
	return (config <= l2x0pmu_max_event_id) ? config : -ENOENT;
}

static int l2x0pmu_map_event(struct perf_event *event)
{
	u64 config = event->attr.config;
	u64 supported_samples = (PERF_SAMPLE_TIME	|
				 PERF_SAMPLE_ID		|
				 PERF_SAMPLE_PERIOD	|
				 PERF_SAMPLE_STREAM_ID	|
				 PERF_SAMPLE_RAW);

	if (event->attr.type != l2x0_pmu.pmu.type)
		return -ENOENT;

	/*
	 * L2x0 counters are global across CPUs.
	 * If userspace ask perf to monitor from multiple CPUs, each CPU will
	 * report the shared total. When summed, this will be the actual value
	 * multiplied by the number of CPUs. We limit monitoring to a single
	 * CPU (0) to prevent confusion stemming from this.
	 */
	if (event->cpu != 0)
		return -ENOENT;

	if (event->attr.sample_type & ~supported_samples)
		return -ENOENT;

	return map_l2x0_raw_event(config);
}

static struct arm_pmu l2x0_pmu = {
	.id		= ARM_PERF_PMU_ID_L2X0,
	.type		= ARM_PMU_DEVICE_L2CC,
	.name		= "ARM L220/PL310 L2 Cache controller",
	.start		= l2x0pmu_start,
	.stop		= l2x0pmu_stop,
	.handle_irq	= l2x0pmu_handle_irq,
	.enable		= l2x0pmu_enable,
	.disable	= l2x0pmu_disable,
	.get_event_idx	= l2x0pmu_get_event_idx,
	.read_counter	= l2x0pmu_read_counter,
	.write_counter	= l2x0pmu_write_counter,
	.map_event	= l2x0pmu_map_event,
	.num_events	= 2,
	.max_period	= 0xFFFFFFFF,
	.get_hw_events	= l2x0pmu_get_hw_events,
};

static int __devinit l2x0pmu_device_probe(struct platform_device *pdev)
{
	l2x0_pmu.plat_device = pdev;
	/* FIXME: return code? */
	armpmu_register(&l2x0_pmu, "l2x0", -1);
	return 0;
}

static struct platform_driver l2x0pmu_driver = {
	.driver		= {
		.name	= "l2x0-pmu",
	},
	.probe		= l2x0pmu_device_probe,
};

static int __init register_pmu_driver(void)
{
	return platform_driver_register(&l2x0pmu_driver);
}
device_initcall(register_pmu_driver);

#endif /* CONFIG_HW_PERF_EVENTS */
