// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include "mdla_debug.h"

#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/bitmap.h>

#include "mdla_hw_reg.h"
#include "mdla_pmu.h"
#include "mdla.h"

#define COUNTER_CLEAR 0xFFFFFFFF

DECLARE_BITMAP(pmu_bitmap, MDLA_PMU_COUNTERS);
DEFINE_SPINLOCK(pmu_lock);

/* saved registers, used to restore config after pmu reset */
u32 cfg_pmu_event[MDLA_PMU_COUNTERS];
static u32 cfg_pmu_clr_mode;
/* lastest register values, since last command end */
static u32 l_counters[MDLA_PMU_COUNTERS];
static u32 l_start_t;
static u32 l_end_t;
static u32 l_cycle;

unsigned int pmu_reg_read(u32 offset)
{
	return ioread32(apu_mdla_biu_top + offset);
}

static void pmu_reg_write(u32 value, u32 offset)
{
	iowrite32(value, apu_mdla_biu_top + offset);
}

#define pmu_reg_set(mask, offset) \
	pmu_reg_write(pmu_reg_read(offset) | (mask), (offset))

#define pmu_reg_clear(mask, offset) \
	pmu_reg_write(pmu_reg_read(offset) & ~(mask), (offset))

/*
 * API naming rules
 * pmu_xxx_save(): save registers to variables
 * pmu_xxx_get(): load values from saved variables.
 * pmu_xxx_read(): read values from registers.
 * pmu_xxx_write(): write values to registers.
 */

static int pmu_event_write(u32 handle, u32 val)
{
	u32 mask;

	if (handle >= MDLA_PMU_COUNTERS)
		return -EINVAL;

	mask = 1 << (handle+17);

	if (val == COUNTER_CLEAR) {
		mdla_pmu_debug("%s: clear pmu counter[%d]\n",
			__func__, handle);
		pmu_reg_write(0, PMU_EVENT_OFFSET +
			(handle) * PMU_CNT_SHIFT);
		pmu_reg_clear(mask, PMU_CFG_PMCR);
	} else {
		mdla_pmu_debug("%s: set pmu counter[%d] = 0x%x\n",
			__func__, handle, val);
		pmu_reg_write(val, PMU_EVENT_OFFSET +
			(handle) * PMU_CNT_SHIFT);
		pmu_reg_set(mask, PMU_CFG_PMCR);
	}

	return 0;
}

int pmu_counter_alloc(u32 interface, u32 event)
{
	unsigned long flags;
	int handle;

	mutex_lock(&cmd_lock);
	mutex_lock(&power_lock);

	spin_lock_irqsave(&pmu_lock, flags);
	handle = bitmap_find_free_region(pmu_bitmap, MDLA_PMU_COUNTERS, 0);
	spin_unlock_irqrestore(&pmu_lock, flags);
	if (unlikely(handle < 0))
		goto out;

	pmu_counter_event_save(handle, ((interface << 16) | event));

out:
	mutex_unlock(&power_lock);
	mutex_unlock(&cmd_lock);
	return handle;
}
EXPORT_SYMBOL(pmu_counter_alloc);

int pmu_counter_free(int handle)
{
	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	mutex_lock(&cmd_lock);
	mutex_lock(&power_lock);

	bitmap_release_region(pmu_bitmap, handle, 0);

	if (get_power_on_status())
		pmu_event_write(handle, COUNTER_CLEAR);

	mutex_unlock(&power_lock);
	mutex_unlock(&cmd_lock);

	return 0;
}
EXPORT_SYMBOL(pmu_counter_free);

int pmu_counter_event_save(u32 handle, u32 val)
{
	if (handle >= MDLA_PMU_COUNTERS)
		return -EINVAL;

	cfg_pmu_event[handle] = val;

	if (!get_power_on_status())
		return 0;

	return pmu_event_write(handle, val);
}

int pmu_counter_event_get(int handle)
{
	u32 event;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	event = cfg_pmu_event[handle];

	return (event == COUNTER_CLEAR) ? -ENOENT : event;
}

int pmu_counter_event_get_all(u32 out[MDLA_PMU_COUNTERS])
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = cfg_pmu_event[i];

	return 0;
}

void pmu_counter_read_all(u32 out[MDLA_PMU_COUNTERS])
{
	int i;
	u32 offset;
	u32 reg;

	offset = PMU_CNT_OFFSET;
	reg = pmu_reg_read(PMU_CFG_PMCR);

	if ((1<<PMU_CLR_CMDE_SHIFT) & reg)
		offset = offset + 4;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = pmu_reg_read(offset + (i * PMU_CNT_SHIFT));
}

u32 pmu_counter_get(int handle)
{
	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	return l_counters[handle];
}

void pmu_counter_get_all(u32 out[MDLA_PMU_COUNTERS])
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = l_counters[i];
}

u32 pmu_get_perf_start(void)
{
	return l_start_t;
}

u32 pmu_get_perf_end(void)
{
	return l_end_t;
}

u32 pmu_get_perf_cycle(void)
{
	return l_cycle;
}

static void pmu_reset_counter(void)
{
	mdla_pmu_debug("mdla: %s\n", __func__);

	if (!get_power_on_status())
		return;

	pmu_reg_set(PMU_PMCR_CNT_RST, PMU_CFG_PMCR);
	while (pmu_reg_read(PMU_CFG_PMCR) &
		PMU_PMCR_CNT_RST) {
	}
}

static void pmu_reset_cycle(void)
{
	mdla_pmu_debug("mdla: %s\n", __func__);

	if (!get_power_on_status())
		return;

	pmu_reg_set((PMU_PMCR_CCNT_EN | PMU_PMCR_CCNT_RST), PMU_CFG_PMCR);
	while (pmu_reg_read(PMU_CFG_PMCR) &
		PMU_PMCR_CCNT_RST) {
	}
}

void pmu_reset_saved_counter(void)
{
	int i;


	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		l_counters[i] = 0;

	pmu_reset_counter();
}

void pmu_reset_saved_cycle(void)
{

	l_cycle = 0;
	pmu_reset_cycle();
}

/* 1: PMU cleary by each command end */
static void pmu_clr_mode_write(u32 mode)
{
	u32 mask = (1 << PMU_CLR_CMDE_SHIFT);

	if (!get_power_on_status())
		return;

	if (mode)
		pmu_reg_set(mask, PMU_CFG_PMCR);
	else
		pmu_reg_clear(mask, PMU_CFG_PMCR);
}

void pmu_clr_mode_save(u32 mode)
{
	cfg_pmu_clr_mode = mode;
	pmu_clr_mode_write(mode);
}

/* save pmu registers for query after power off */
void pmu_reg_save(void)
{
	l_cycle = pmu_reg_read(PMU_CYCLE);
	l_end_t = pmu_reg_read(PMU_END_TSTAMP);
	l_start_t = pmu_reg_read(PMU_START_TSTAMP);

	pmu_counter_read_all(l_counters);
}

void pmu_reset(void)
{
	int i;

	pmu_reg_write((CFG_PMCR_DEFAULT|
		PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST), PMU_CFG_PMCR);

	while (pmu_reg_read(PMU_CFG_PMCR) &
		(PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST)) {
	}

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		pmu_event_write(i, cfg_pmu_event[i]);

	pmu_clr_mode_write(cfg_pmu_clr_mode);
}

void pmu_init(void)
{
	cfg_pmu_clr_mode = 0;
	memset(cfg_pmu_event, 0xFF, sizeof(cfg_pmu_event));
}

