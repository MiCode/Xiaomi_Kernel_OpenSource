/*
 * Copyright (C) 2019 MediaTek Inc.
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

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/io.h>

#include "mnoc_drv.h"
#include "mnoc_hw.h"
#include "mnoc_pmu.h"
#define CREATE_TRACE_POINTS
#include "mnoc_met_events.h"

/* in micro-seconds (us) */
#define PERIOD_DEFAULT 1000
bool mnoc_cfg_timer_en;
static bool mnoc_cfg_timer_en_copy;
static u64 cfg_period;
static struct hrtimer hr_timer;

struct pmu_reg_list pmu_reg_list;


void enque_pmu_reg(unsigned int addr, unsigned int val)
{
	struct pmu_reg *pmu_reg = NULL, *pos;

	LOG_DEBUG("+\n");

	mutex_lock(&(pmu_reg_list.list_mtx));

	/* if addr already exist, just update value */
	list_for_each_entry(pos, &(pmu_reg_list.list), list) {
		if (pos->addr == addr) {
			pmu_reg = pos;
			break;
		}
	}
	if (pmu_reg != NULL) {
		pmu_reg->val = val;
		mutex_unlock(&(pmu_reg_list.list_mtx));
		return;
	}

	pmu_reg = kzalloc(sizeof(struct pmu_reg), GFP_KERNEL);
	if (pmu_reg == NULL) {
		LOG_ERR("alloc pmu_reg(%d/%d) fail\n", addr, val);
		return;
	};

	pmu_reg->addr = addr;
	pmu_reg->val = val;

	list_add_tail(&pmu_reg->list, &(pmu_reg_list.list));

	mutex_unlock(&(pmu_reg_list.list_mtx));

	LOG_DEBUG("-\n");
}

void clear_pmu_reg_list(void)
{
	struct pmu_reg *pmu_reg, *pos;
	void *addr = 0;
	unsigned long flags;

	LOG_DEBUG("+\n");

	mutex_lock(&(pmu_reg_list.list_mtx));

	list_for_each_entry_safe(pmu_reg, pos, &(pmu_reg_list.list), list) {
		spin_lock_irqsave(&mnoc_spinlock, flags);
		addr = (void *) ((uintptr_t) mnoc_base +
					(pmu_reg->addr - APU_NOC_TOP_ADDR));
		if (mnoc_reg_valid)
			mnoc_write(addr, 0);
		spin_unlock_irqrestore(&mnoc_spinlock, flags);

		list_del(&pmu_reg->list);
		kfree(pmu_reg);
	}

	mutex_unlock(&(pmu_reg_list.list_mtx));

	LOG_DEBUG("-\n");
}

void mnoc_pmu_reg_init(void)
{
	struct pmu_reg *pmu_reg = NULL;
	void *addr = 0;
	unsigned long flags;

	LOG_DEBUG("+\n");

	mutex_lock(&(pmu_reg_list.list_mtx));

	spin_lock_irqsave(&mnoc_spinlock, flags);
	list_for_each_entry(pmu_reg, &(pmu_reg_list.list), list) {
		addr = (void *) ((uintptr_t) mnoc_base +
					(pmu_reg->addr - APU_NOC_TOP_ADDR));
		mnoc_write(addr, pmu_reg->val);
		LOG_DEBUG("Write Reg[%08X] to 0x%08X\n",
			pmu_reg->addr, pmu_reg->val);
	}
	spin_unlock_irqrestore(&mnoc_spinlock, flags);

	mutex_unlock(&(pmu_reg_list.list_mtx));

	LOG_DEBUG("-\n");
}

void print_pmu_reg_list(struct seq_file *m)
{
	struct pmu_reg *pmu_reg = NULL;

	mutex_lock(&(pmu_reg_list.list_mtx));
	list_for_each_entry(pmu_reg, &(pmu_reg_list.list), list) {
		seq_printf(m, "pmu_reg(0x%08x/0x%08x)\n",
			pmu_reg->addr, pmu_reg->val);
	}
	mutex_unlock(&(pmu_reg_list.list_mtx));
}

/*
 * MNoC PMU Polling Function
 */
static enum hrtimer_restart mnoc_pmu_polling(struct hrtimer *timer)
{
	unsigned int mnoc_pmu_buf[NR_MNOC_PMU_CNTR];

	LOG_DEBUG("+\n");

	if (!cfg_period || !mnoc_cfg_timer_en)
		return HRTIMER_NORESTART;

	/* call functions need to be called periodically */
	memset(mnoc_pmu_buf, 0, NR_MNOC_PMU_CNTR * sizeof(unsigned int));
	mnoc_get_pmu_counter(mnoc_pmu_buf);
	trace_mnoc_pmu_polling(mnoc_pmu_buf);

	hrtimer_forward_now(&hr_timer, ns_to_ktime(cfg_period * 1000));

	LOG_DEBUG("-\n");

	return HRTIMER_RESTART;
}

void mnoc_pmu_timer_start(void)
{
	LOG_DEBUG("+\n");

	hrtimer_start(&hr_timer, ns_to_ktime(cfg_period * 1000),
			HRTIMER_MODE_REL);

	LOG_DEBUG("-\n");
}

void mnoc_pmu_suspend(void)
{
	LOG_DEBUG("+\n");

	mnoc_cfg_timer_en_copy = mnoc_cfg_timer_en;
	if (mnoc_cfg_timer_en_copy)
		mnoc_cfg_timer_en = false;

	LOG_DEBUG("-\n");
}

void mnoc_pmu_resume(void)
{
	LOG_DEBUG("+\n");

	if (mnoc_cfg_timer_en_copy) {
		mnoc_cfg_timer_en = true;
		mnoc_pmu_timer_start();
	}

	LOG_DEBUG("-\n");
}

void mnoc_pmu_init(void)
{
	LOG_DEBUG("+\n");

	/* init pmu_reg_list's list */
	INIT_LIST_HEAD(&(pmu_reg_list.list));
	mutex_init(&(pmu_reg_list.list_mtx));

	cfg_period = PERIOD_DEFAULT;
	mnoc_cfg_timer_en = false;
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = mnoc_pmu_polling;

	LOG_DEBUG("-\n");
}

void mnoc_pmu_exit(void)
{
	LOG_DEBUG("+\n");

	hrtimer_cancel(&hr_timer);
	clear_pmu_reg_list();

	LOG_DEBUG("-\n");
}
