/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/bitmap.h>

#include "mdla_hw_reg.h"
#include "mdla_pmu.h"
#include "mdla.h"
#include "mdla_trace.h"
#include "mdla_debug.h"

#define COUNTER_CLEAR 0xFFFFFFFF

DECLARE_BITMAP(pmu0_bitmap, MDLA_PMU_COUNTERS);
DECLARE_BITMAP(pmu1_bitmap, MDLA_PMU_COUNTERS);

spinlock_t pmu_lock[MTK_MDLA_MAX_NUM];

/* saved registers, used to restore config after pmu reset */
u32 cfg_pmu_event[MTK_MDLA_MAX_NUM][MDLA_PMU_COUNTERS];

/* used to save event from ioctl */
u32 cfg_pmu_event_trace[MDLA_PMU_COUNTERS];

//static u32 cfg_pmu_clr_mode[MTK_MDLA_MAX_NUM];
static u8 cfg_pmu_percmd_mode[MTK_MDLA_MAX_NUM];

/* lastest register values, since last command end */
static u16 l_cmd_cnt[MTK_MDLA_MAX_NUM];
static u16 l_cmd_id[MTK_MDLA_MAX_NUM];
static u32 l_counters[MTK_MDLA_MAX_NUM][MDLA_PMU_COUNTERS];
static u32 l_start_t[MTK_MDLA_MAX_NUM];
static u32 l_end_t[MTK_MDLA_MAX_NUM];
static u32 l_cycle[MTK_MDLA_MAX_NUM];
//static struct mdla_pmu_event_handle mdla_pmu_event_hnd[MTK_MDLA_MAX_NUM];
static u32 pmu_event_handle[MTK_MDLA_MAX_NUM][MDLA_PMU_COUNTERS];

unsigned int pmu_reg_read_with_mdlaid(u32 mdlaid, u32 offset)
{
	return ioread32(mdla_reg_control[mdlaid].apu_mdla_biu_top + offset);
}

static void pmu_reg_write_with_mdlaid(u32 mdlaid, u32 value, u32 offset)
{
	iowrite32(value, mdla_reg_control[mdlaid].apu_mdla_biu_top + offset);
}

#define pmu_reg_set_with_mdlaid(id, mask, offset) \
	pmu_reg_write_with_mdlaid(id, \
	pmu_reg_read_with_mdlaid(id, offset) | (mask), (offset))

#define pmu_reg_clear_with_mdlaid(id, mask, offset) \
	pmu_reg_write_with_mdlaid(id, \
	pmu_reg_read_with_mdlaid(id, offset) & ~(mask), (offset))

/*
 * API naming rules
 * pmu_xxx_save(): save registers to variables
 * pmu_xxx_get(): load values from saved variables.
 * pmu_xxx_read(): read values from registers.
 * pmu_xxx_write(): write values to registers.
 */

static int pmu_event_write(u32 mdlaid, u32 handle, u32 val)
{
	u32 mask;

	if (handle >= MDLA_PMU_COUNTERS)
		return -EINVAL;

	mask = 1 << (handle+17);

	if (val == COUNTER_CLEAR) {
		mdla_pmu_debug("%s: clear pmu counter[%d]\n",
			__func__, handle);
		pmu_reg_write_with_mdlaid(mdlaid, 0, PMU_EVENT_OFFSET +
			(handle) * PMU_CNT_SHIFT);
		pmu_reg_clear_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
	} else {
		mdla_pmu_debug("%s: set pmu counter[%d] = 0x%x\n",
			__func__, handle, val);
		pmu_reg_write_with_mdlaid(mdlaid, val, PMU_EVENT_OFFSET +
			(handle) * PMU_CNT_SHIFT);
		pmu_reg_set_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
	}

	return 0;
}

int pmu_counter_alloc(u32 mdlaid, u32 interface, u32 event)
{
	unsigned long flags;
	int handle;

	//mutex_lock(&mdla_devices[mdlaid].cmd_lock);
	mutex_lock(&mdla_devices[mdlaid].power_lock);

	spin_lock_irqsave(&pmu_lock[mdlaid], flags);

	if (mdlaid == 0)
		handle = bitmap_find_free_region(pmu0_bitmap,
		MDLA_PMU_COUNTERS, 0);
	else if (mdlaid == 1)
		handle = bitmap_find_free_region(pmu1_bitmap,
		MDLA_PMU_COUNTERS, 0);
	else{
		handle = -EINVAL;
		goto out;
	}

	spin_unlock_irqrestore(&pmu_lock[mdlaid], flags);
	if (unlikely(handle < 0))
		goto out;

	pmu_counter_event_save(mdlaid, handle, ((interface << 16) | event));

out:
	mutex_unlock(&mdla_devices[mdlaid].power_lock);
	//mutex_unlock(&mdla_devices[mdlaid].cmd_lock);
	return handle;
}
EXPORT_SYMBOL(pmu_counter_alloc);

int pmu_counter_free(u32 mdlaid, int handle)
{
	int ret = 0;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	//mutex_lock(&mdla_devices[mdlaid].cmd_lock);
	mutex_lock(&mdla_devices[mdlaid].power_lock);

	if (mdlaid == 0)
		bitmap_release_region(pmu0_bitmap, handle, 0);
	else if (mdlaid == 1)
		bitmap_release_region(pmu1_bitmap, handle, 0);
	else{
		handle = -EINVAL;
		goto out;
	}

	if (get_power_on_status(mdlaid))
		pmu_event_write(mdlaid, handle, COUNTER_CLEAR);

out:
	mutex_unlock(&mdla_devices[mdlaid].power_lock);
	//mutex_unlock(&mdla_devices[mdlaid].cmd_lock);

	return ret;
}
EXPORT_SYMBOL(pmu_counter_free);

int pmu_counter_event_save(u32 mdlaid, u32 handle, u32 val)
{
	if (handle >= MDLA_PMU_COUNTERS)
		return -EINVAL;

	cfg_pmu_event[mdlaid][handle] = val;

	if (!get_power_on_status(mdlaid))
		return 0;

	return pmu_event_write(mdlaid, handle, val);
}

int pmu_counter_event_get(u32 mdlaid, int handle)
{
	u32 event;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	event = cfg_pmu_event[mdlaid][handle];

	return (event == COUNTER_CLEAR) ? -ENOENT : event;
}

int pmu_counter_event_get_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS])
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = cfg_pmu_event_trace[i];

	return 0;
}

void pmu_counter_read_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS])
{
	int i;
	u32 offset;
	u32 reg;

	offset = PMU_CNT_OFFSET;
	reg = pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR);

	if ((1<<PMU_CLR_CMDE_SHIFT) & reg)
		offset = offset + 4;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = pmu_reg_read_with_mdlaid(mdlaid,
		offset + (i * PMU_CNT_SHIFT));
}

u32 pmu_counter_get(u32 mdlaid, int handle)
{
	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	return l_counters[mdlaid][handle];
}

void pmu_counter_get_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS])
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		out[i] = l_counters[mdlaid][i];
}

u32 pmu_get_perf_start(u32 mdlaid)
{
	return l_start_t[mdlaid];
}

u32 pmu_get_perf_end(u32 mdlaid)
{
	return l_end_t[mdlaid];
}

u32 pmu_get_perf_cycle(u32 mdlaid)
{
	return l_cycle[mdlaid];
}

u16 pmu_get_perf_cmdid(u32 mdlaid)
{
	return l_cmd_id[mdlaid];
}

static void pmu_reset_counter(u32 mdlaid)
{
	mdla_pmu_debug("mdla: %s\n", __func__);

	if (!get_power_on_status(mdlaid))
		return;
	/*Reset Clock counter to zero. Return to zero when reset done.*/
	pmu_reg_set_with_mdlaid(mdlaid, PMU_PMCR_CNT_RST, PMU_CFG_PMCR);
	while (pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR) &
		PMU_PMCR_CNT_RST) {
	}
}

static void pmu_reset_cycle(u32 mdlaid)
{
	mdla_pmu_debug("mdla: %s\n", __func__);

	if (!get_power_on_status(mdlaid))
		return;

	pmu_reg_set_with_mdlaid(mdlaid,
		(PMU_PMCR_CCNT_EN | PMU_PMCR_CCNT_RST), PMU_CFG_PMCR);
	while (pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR) &
		PMU_PMCR_CCNT_RST) {
	}
}

void pmu_reset_saved_counter(u32 mdlaid)
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		l_counters[mdlaid][i] = 0;

	l_cmd_id[mdlaid] = 0;
	l_cmd_cnt[mdlaid] = 0;
	pmu_reset_counter(mdlaid);
}

void pmu_reset_saved_cycle(u32 mdlaid)
{

	l_cycle[mdlaid] = 0;
	pmu_reset_cycle(mdlaid);
}

static void pmu_percmd_mode_write(u32 mdlaid, u32 mode)
{
	u32 mask = (1 << PMU_CLR_CMDE_SHIFT);

	if (!get_power_on_status(mdlaid))
		return;

	if (mode)
		pmu_reg_set_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
	else
		pmu_reg_clear_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
}

void pmu_percmd_mode_save(u32 mdlaid, u32 mode)
{
	cfg_pmu_percmd_mode[mdlaid] = mode;
	pmu_percmd_mode_write(mdlaid, mode);
}

/* save pmu registers for query after power off */
void pmu_reg_save(u32 mdlaid)
{
	u32 val = 0;

	if (mdla_devices[mdlaid].pmu.pmu_mode == NORMAL)
		l_cmd_cnt[mdlaid] = 1;
	else {
		val = pmu_reg_read_with_mdlaid(mdlaid, PMU_CMDID_LATCH);
		if (val != l_cmd_id[mdlaid])
			l_cmd_cnt[mdlaid]++;
	}

	l_cmd_id[mdlaid] = (u16)val;
	l_cycle[mdlaid] = pmu_reg_read_with_mdlaid(mdlaid, PMU_CYCLE);
	l_end_t[mdlaid] = pmu_reg_read_with_mdlaid(mdlaid, PMU_END_TSTAMP);
	l_start_t[mdlaid] = pmu_reg_read_with_mdlaid(mdlaid, PMU_START_TSTAMP);

	pmu_counter_read_all(mdlaid, l_counters[mdlaid]);
	mdla_perf_debug("cmd_id: %d, cmd_cnt: %d\n",
			l_cmd_id[mdlaid], l_cmd_cnt[mdlaid]);
}

void pmu_reset(u32 mdlaid)
{
	int i;

	pmu_reg_write_with_mdlaid(mdlaid, (CFG_PMCR_DEFAULT|
		PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST), PMU_CFG_PMCR);

	while (pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR) &
		(PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST)) {
	}

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		pmu_event_write(mdlaid, i, cfg_pmu_event[mdlaid][i]);

	pmu_percmd_mode_write(mdlaid, cfg_pmu_percmd_mode[mdlaid]);

	mdla_pmu_debug("mdla: %s, PMU_CFG_PMCR: 0x%x\n",
		__func__, pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR));

}

void pmu_init(u32 mdlaid)
{
	cfg_pmu_percmd_mode[mdlaid] = NORMAL;
	spin_lock_init(&pmu_lock[mdlaid]);
}

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int pmu_apusys_pmu_addr_check(struct apusys_cmd_hnd *apusys_hd)
{
	int ret = 0;

	if ((apusys_hd == NULL) ||
		(apusys_hd->pmu_kva == apusys_hd->cmd_entry) ||
		(apusys_hd->pmu_kva == 0))
		return ret = -1;
	mdla_pmu_debug("command entry:%08llx, pmu kva: %08llx\n",
		apusys_hd->cmd_entry,
		apusys_hd->pmu_kva);
	return ret;
}

int pmu_cmd_handle(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd)
{
	int i;

	pmu_reset_saved_counter(mdla_info->mdlaid);
	pmu_reset_saved_cycle(mdla_info->mdlaid);

	if (!pmu_apusys_pmu_addr_check(apusys_hd)) {
		pmu_percmd_mode_save(mdla_info->mdlaid,
			mdla_info->pmu.pmu_mode);
		mdla_pmu_debug("PMU number_of_event:%d, mode: %d\n",
			mdla_info->pmu.pmu_hnd->number_of_event,
			mdla_info->pmu.pmu_mode);
	}

	if (!cfg_apusys_trace) {
		if (pmu_apusys_pmu_addr_check(apusys_hd))
			return -1;
		for (i = 0; i < mdla_info->pmu.pmu_hnd->number_of_event; i++) {
			pmu_event_handle[mdla_info->mdlaid][i] =
				pmu_counter_alloc(mdla_info->mdlaid,
				(mdla_info->pmu.pmu_hnd->event[i]&0x1f00)>>8,
				mdla_info->pmu.pmu_hnd->event[i]&0xf);
		}
	} else {
		for (i = 0; i < MDLA_PMU_COUNTERS; i++) {
			pmu_event_handle[mdla_info->mdlaid][i] =
				pmu_counter_alloc(mdla_info->mdlaid,
				(cfg_pmu_event_trace[i]&0x1f0000)>>16,
				cfg_pmu_event_trace[i]&0xf);
		}
	}
	return 0;
}

int pmu_command_prepare(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd)
{
	mdla_info->pmu.pmu_hnd = (struct mdla_pmu_hnd *)apusys_hd->pmu_kva;

	/*mdla pmu mode switch from ioctl or apusys cmd*/
	if (cfg_pmu_percmd_mode[mdla_info->mdlaid] < CMD_MODE_MAX)
		mdla_info->pmu.pmu_mode =
		cfg_pmu_percmd_mode[mdla_info->mdlaid];
	else
		mdla_info->pmu.pmu_mode = mdla_info->pmu.pmu_hnd->mode;

	if (mdla_info->pmu.pmu_mode >= CMD_MODE_MAX)
		return -1;

	mdla_info->pmu.cmd_id = apusys_hd->cmd_id;

	mdla_info->pmu.PMU_res_buf_addr0 = apusys_hd->cmd_entry +
		mdla_info->pmu.pmu_hnd->offset_to_PMU_res_buf0;
	mdla_info->pmu.PMU_res_buf_addr1 = apusys_hd->cmd_entry +
		mdla_info->pmu.pmu_hnd->offset_to_PMU_res_buf1;

	if (mdla_info->pmu.pmu_mode == PER_CMD)
		cfg_timer_en = 1;

	mdla_pmu_debug("pmu addr0: %08llx, pmu addr1: %08llx\n",
		mdla_info->pmu.PMU_res_buf_addr0,
		mdla_info->pmu.PMU_res_buf_addr1);
	return 0;
}
#endif

void pmu_command_counter_prt(struct mdla_dev *mdla_info)
{
	int i;
	struct mdla_pmu_result result;
	struct mdla_pmu_result check;
	void *base = NULL, *desc = NULL, *src = NULL;
	int sz = 0, event_num = 0;
	int offset = 0;
	u16 final_len = 0;

	result.cmd_len = l_cmd_cnt[mdla_info->mdlaid];
	result.cmd_id = pmu_get_perf_cmdid(mdla_info->mdlaid);
	event_num = mdla_info->pmu.pmu_hnd->number_of_event + 1;

	sz = sizeof(u16) * 2 + sizeof(u32) * event_num;

	if (mdla_info->mdlaid == 0) {
		base = (void *)mdla_info->pmu.PMU_res_buf_addr0;
	} else if (mdla_info->mdlaid == 1) {
		base = (void *)mdla_info->pmu.PMU_res_buf_addr1;
	} else {
		mdla_pmu_debug("unknown mdlaid: %d\n", mdla_info->mdlaid);
		return;
	}

	mdla_pmu_debug("mode: %d, cmd_len: %d, cmd_id: %d, sz: %d\n",
		       mdla_info->pmu.pmu_hnd->mode,
		       result.cmd_len, result.cmd_id, sz);

	if (mdla_info->pmu.pmu_mode == PER_CMD)
		result.pmu_val[0] = pmu_get_perf_end(mdla_info->mdlaid);
	else
		result.pmu_val[0] = pmu_get_perf_cycle(mdla_info->mdlaid);

	mdla_pmu_debug("global counter:%08x\n", result.pmu_val[0]);

	for (i = 0; i < mdla_info->pmu.pmu_hnd->number_of_event; i++) {
		result.pmu_val[i + 1] =
			pmu_counter_get(mdla_info->mdlaid,
					pmu_event_handle[mdla_info->mdlaid][i]);
		mdla_pmu_debug("event %d cnt :%08x\n",
			       (i + 1), result.pmu_val[i + 1]);
	}

	/* update pmu result buffer */
	if (result.cmd_len == 1) {
		desc = base;
		src = &result;
		memcpy(desc, src, sz);
	} else if (result.cmd_len > 1) {
		offset = sz + (result.cmd_len - 2) * (sz - sizeof(u16));
		desc = (void *)(base + offset);
		src = (void *)&(result.cmd_id);
		memcpy(desc, src, sz - sizeof(u16));
	}

	if (result.cmd_id == mdla_info->max_cmd_id) {
		final_len = result.cmd_len;
		desc = base;
		memcpy(desc, &final_len, sizeof(u16));

		memcpy(&check, desc, sz);

		mdla_pmu_debug("[-] check cmd_len: %d\n", check.cmd_len);
		mdla_pmu_debug("[-] check cmd_id: %d\n", check.cmd_id);
		mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
			       check.pmu_val[1]);

		if (result.cmd_len > 1) {
			offset = sz + (result.cmd_len - 2) * (sz - sizeof(u16));
			desc = (void *)(base + offset);
			memcpy((void *)&(check.cmd_id), desc, sz - sizeof(u16));

			mdla_pmu_debug("[-] offset: %d\n", offset);
			mdla_pmu_debug("[-] check cmd_id: %d\n", check.cmd_id);
			mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
				       check.pmu_val[1]);
		}
	}
}

