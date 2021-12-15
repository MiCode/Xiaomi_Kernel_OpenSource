// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#define MN MTK_MDLA_MAX_NUM
#define PL PRIORITY_LEVEL
#define MPC MDLA_PMU_COUNTERS

/* for core id 0 */
DECLARE_BITMAP(pmu0_bitmap, MPC);
/* for core id 1 */
DECLARE_BITMAP(pmu1_bitmap, MPC);

spinlock_t pmu_lock[MN];

/* saved registers, used to restore config after pmu reset */
u32 cfg_pmu_event[MN][MPC];

/* used to save event from ioctl */
u32 cfg_pmu_event_trace[MPC];

//static u32 cfg_pmu_clr_mode[MN];
static u8 cfg_pmu_percmd_mode[MN][PL];
/* lastest register values, since last command end */
static u16 l_cmd_cnt[MN][PL];
static u16 l_cmd_id[MN][PL];
static u32 l_counters[MN][PL][MPC];
static u32 l_start_t[MN][PL];
static u32 l_end_t[MN][PL];
static u32 l_cycle[MN][PL];
static u32 number_of_event[PL];
//static struct mdla_pmu_event_handle mdla_pmu_event_hnd[MN];
static u32 pmu_event_handle[MN][PL][MPC];

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

	if (handle >= MPC)
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

int pmu_event_write_all(u32 mdlaid, u16 priority)
{
	int i;

	if (!cfg_apusys_trace) {
		for (i = 0; i < number_of_event[priority]; i++) {
			pmu_event_write(mdlaid, i,
				pmu_event_handle[mdlaid][priority][i]);
		}
	} else {
		for (i = 0; i < MPC; i++) {
			pmu_event_write(mdlaid, i,
				pmu_event_handle[mdlaid][priority][i]);
		}
	}
	return 0;
}

/* for ioctrl */
int pmu_counter_alloc(u32 mdlaid, u32 interface, u32 event)
{
	unsigned long flags;
	int handle;

	//mutex_lock(&mdla_devices[mdlaid].cmd_lock);
	mutex_lock(&mdla_devices[mdlaid].power_lock);

	spin_lock_irqsave(&pmu_lock[mdlaid], flags);

	if (mdlaid == 0)
		handle = bitmap_find_free_region(pmu0_bitmap,
		MPC, 0);
	else if (mdlaid == 1)
		handle = bitmap_find_free_region(pmu1_bitmap,
		MPC, 0);
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

/* for ioctrl */
int pmu_counter_free(u32 mdlaid, int handle)
{
	int ret = 0;

	if ((handle >= MPC) || (handle < 0))
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

/* for ioctrl */
int pmu_counter_event_save(u32 mdlaid, u32 handle, u32 val)
{
	if (handle >= MPC)
		return -EINVAL;

	cfg_pmu_event[mdlaid][handle] = val;

	if (!get_power_on_status(mdlaid))
		return -1;

	return pmu_event_write(mdlaid, handle, val);
}

/* for ioctrl */
int pmu_counter_event_get(u32 mdlaid, int handle)
{
	u32 event;

	if ((handle >= MPC) || (handle < 0))
		return -EINVAL;

	event = cfg_pmu_event[mdlaid][handle];

	return (event == COUNTER_CLEAR) ? -ENOENT : event;
}

/* for mdla_trace */
int pmu_counter_event_get_all(u32 mdlaid, u32 out[MPC])
{
	int i;

	for (i = 0; i < MPC; i++)
		out[i] = cfg_pmu_event_trace[i];

	return 0;
}

/* for mdla_trace and apusys_hnd */
void pmu_counter_read_all(u32 mdlaid, u32 out[MPC])
{
	int i;
	u32 offset;
	u32 reg;

	offset = PMU_CNT_OFFSET;
	reg = pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR);

	if ((1<<PMU_CLR_CMDE_SHIFT) & reg)
		offset = offset + 4;

	for (i = 0; i < MPC; i++)
		out[i] += pmu_reg_read_with_mdlaid(mdlaid,
			offset + (i * PMU_CNT_SHIFT));
}

u32 pmu_counter_get(u32 mdlaid, int handle, u16 priority)
{
	if ((handle >= MPC) || (handle < 0))
		return -EINVAL;

	return l_counters[mdlaid][priority][handle];
}

void pmu_counter_get_all(u32 mdlaid, u32 out[MPC], u16 priority)
{
	int i;

	for (i = 0; i < MPC; i++)
		out[i] = l_counters[mdlaid][priority][i];
}

u32 pmu_get_perf_start(u32 mdlaid, u16 priority)
{
	return l_start_t[mdlaid][priority];
}

u32 pmu_get_perf_end(u32 mdlaid, u16 priority)
{
	return l_end_t[mdlaid][priority];
}

u32 pmu_get_perf_cycle(u32 mdlaid, u16 priority)
{
	return l_cycle[mdlaid][priority];
}

u16 pmu_get_perf_cmdid(u32 mdlaid, u16 priority)
{
	return l_cmd_id[mdlaid][priority];
}

void pmu_reset_counter(u32 mdlaid)
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

void pmu_reset_cycle(u32 mdlaid)
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

/* it set mode to register */
void pmu_percmd_mode_write(u32 mdlaid, u16 priority)
{
	u32 mask = (1 << PMU_CLR_CMDE_SHIFT);
	u32 mode = cfg_pmu_percmd_mode[mdlaid][priority];

	if (!get_power_on_status(mdlaid))
		return;

	if (mode)
		pmu_reg_set_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
	else
		pmu_reg_clear_with_mdlaid(mdlaid, mask, PMU_CFG_PMCR);
}

/* it save mode setting to local variable */
void pmu_percmd_mode_save(u32 mdlaid, u32 mode, u16 priority)
{
	cfg_pmu_percmd_mode[mdlaid][priority] = mode;
}

/* save pmu registers for query after power off */
void pmu_reg_save(u32 mdlaid, u16 priority)
{
	u32 val = 0;

	if (mdla_devices[mdlaid].pmu[priority].pmu_mode == NORMAL)
		l_cmd_cnt[mdlaid][priority] = 1;
	else {
		val = pmu_reg_read_with_mdlaid(mdlaid, PMU_CMDID_LATCH);
		if (val != l_cmd_id[mdlaid][priority])
			l_cmd_cnt[mdlaid][priority]++;
	}

	l_cmd_id[mdlaid][priority] = (u16)val;
	l_cycle[mdlaid][priority] +=
		pmu_reg_read_with_mdlaid(mdlaid, PMU_CYCLE);
	l_end_t[mdlaid][priority] +=
		pmu_reg_read_with_mdlaid(mdlaid, PMU_END_TSTAMP);
	l_start_t[mdlaid][priority] =
		pmu_reg_read_with_mdlaid(mdlaid, PMU_START_TSTAMP);

	pmu_counter_read_all(mdlaid, l_counters[mdlaid][priority]);
}

void pmu_reset(u32 mdlaid)
{
	int i;

	pmu_reg_write_with_mdlaid(mdlaid, (CFG_PMCR_DEFAULT|
		PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST), PMU_CFG_PMCR);

	while (pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR) &
		(PMU_PMCR_CCNT_RST|PMU_PMCR_CNT_RST)) {
	}
	/* reset to 0 */
	for (i = 0; i < MPC; i++)
		pmu_event_write(mdlaid, i, COUNTER_CLEAR);
	/*reset to normal */
	pmu_percmd_mode_write(mdlaid, NORMAL);

	mdla_pmu_debug("mdla: %s, PMU_CFG_PMCR: 0x%x\n",
		__func__, pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR));

}

void pmu_reset_counter_variable(u32 mdlaid, u16 priority)
{
	int i;

	for (i = 0; i < MPC; i++)
		l_counters[mdlaid][priority][i] = 0;

	l_cmd_id[mdlaid][priority] = 0;
	l_cmd_cnt[mdlaid][priority] = 0;
}

void pmu_reset_cycle_variable(u32 mdlaid, u16 priority)
{
	l_cycle[mdlaid][priority] = 0;
	l_end_t[mdlaid][priority] = 0;
	l_start_t[mdlaid][priority] = 0;
}

void pmu_init(u32 mdlaid)
{
	int i;

	for (i = 0; i < PL; i++)
		cfg_pmu_percmd_mode[mdlaid][i] = NORMAL;
	spin_lock_init(&pmu_lock[mdlaid]);
}

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int pmu_apusys_pmu_addr_check(struct apusys_cmd_hnd *apusys_hd)
{
	int ret = 0;
	struct mdla_pmu_hnd *pmu_hnd;

	if ((apusys_hd == NULL) ||
		(apusys_hd->pmu_kva == apusys_hd->cmd_entry) ||
		(apusys_hd->pmu_kva == 0))
		return ret = -1;
	pmu_hnd = (struct mdla_pmu_hnd *)apusys_hd->pmu_kva;
	if (pmu_hnd->number_of_event > MPC)
		return -1;
	mdla_pmu_debug("command entry:%08llx, pmu kva: %08llx\n",
		apusys_hd->cmd_entry,
		apusys_hd->pmu_kva);
	return ret;
}

/* initial local variable and extract pmu setting from input */
int pmu_cmd_handle(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority)
{
	int i;
	u32 cid = mdla_info->mdlaid;

	pmu_reset_counter_variable(mdla_info->mdlaid, priority);
	pmu_reset_cycle_variable(mdla_info->mdlaid, priority);

	if (!pmu_apusys_pmu_addr_check(apusys_hd)) {
		pmu_percmd_mode_save(mdla_info->mdlaid,
			mdla_info->pmu[priority].pmu_mode, priority);
		number_of_event[priority] =
			mdla_info->pmu[priority].pmu_hnd->number_of_event;
		mdla_pmu_debug("PMU number_of_event:%d, mode: %d\n",
			mdla_info->pmu[priority].pmu_hnd->number_of_event,
			mdla_info->pmu[priority].pmu_mode);
	} else {
		number_of_event[priority] = MPC;
	}

	if (!cfg_apusys_trace) {
		if (pmu_apusys_pmu_addr_check(apusys_hd)) {
			for (i = 0; i < MPC; i++) {
				pmu_event_handle[cid][priority][i] =
					COUNTER_CLEAR;
			}
			return -1;
		}
		for (i = 0; i < number_of_event[priority]; i++) {
			u32 high =
				mdla_info->pmu[priority].pmu_hnd->event[i];
			u32 low =
				mdla_info->pmu[priority].pmu_hnd->event[i];
			pmu_event_handle[mdla_info->mdlaid][priority][i] =
				((high&0x1f00)<<8) | (low&0xf);
		}
	} else {
		for (i = 0; i < MPC; i++) {
			pmu_event_handle[mdla_info->mdlaid][priority][i] =
				(cfg_pmu_event_trace[i]&0x1f0000) |
				(cfg_pmu_event_trace[i]&0xf);
		}
	}
	return 0;
}

/* extract pmu_hnd form pmu_kva and set mdla_info->pmu */
int pmu_command_prepare(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority)
{
	mdla_info->pmu[priority].pmu_hnd =
		(struct mdla_pmu_hnd *)apusys_hd->pmu_kva;

	/*mdla pmu mode switch from ioctl or apusys cmd*/
	if (cfg_pmu_percmd_mode[mdla_info->mdlaid][priority] < CMD_MODE_MAX)
		mdla_info->pmu[priority].pmu_mode =
			cfg_pmu_percmd_mode[mdla_info->mdlaid][priority];
	else
		mdla_info->pmu[priority].pmu_mode =
			mdla_info->pmu[priority].pmu_hnd->mode;

	if (mdla_info->pmu[priority].pmu_mode >= CMD_MODE_MAX)
		return -1;

	mdla_info->pmu[priority].cmd_id = apusys_hd->cmd_id;

	mdla_info->pmu[priority].PMU_res_buf_addr0 = apusys_hd->cmd_entry +
		mdla_info->pmu[priority].pmu_hnd->offset_to_PMU_res_buf0;
	mdla_info->pmu[priority].PMU_res_buf_addr1 = apusys_hd->cmd_entry +
		mdla_info->pmu[priority].pmu_hnd->offset_to_PMU_res_buf1;

	if (mdla_info->pmu[priority].pmu_mode == PER_CMD)
		cfg_timer_en = 1;

	mdla_pmu_debug("pmu addr0: %08llx, pmu addr1: %08llx\n",
		mdla_info->pmu[priority].PMU_res_buf_addr0,
		mdla_info->pmu[priority].PMU_res_buf_addr1);
	return 0;
}

/* write pmu setting to register */
int pmu_set_reg(u32 mdlaid, u16 priority)
{

	int i;

	if (!get_power_on_status(mdlaid))
		return -1;

	pmu_reg_set_with_mdlaid(mdlaid,
		CFG_PMCR_DEFAULT |
		PMU_PMCR_CNT_RST |
		PMU_PMCR_CCNT_EN |
		PMU_PMCR_CCNT_RST,
		PMU_CFG_PMCR);

	while (pmu_reg_read_with_mdlaid(mdlaid, PMU_CFG_PMCR) &
		(PMU_PMCR_CNT_RST | PMU_PMCR_CCNT_RST)) {
	}

	pmu_percmd_mode_write(mdlaid, priority);

	if (!cfg_apusys_trace) {
		for (i = 0; i < number_of_event[priority]; i++) {
			if (i < MPC)
				pmu_event_write(
					mdlaid,
					i,
					pmu_event_handle[mdlaid][priority][i]);
			else
				pmu_event_write(mdlaid, i, COUNTER_CLEAR);
		}
	} else {
		for (i = 0; i < MPC; i++)
			pmu_event_write(
				mdlaid,
				i,
				pmu_event_handle[mdlaid][priority][i]);
	}

	return 0;
}
#endif

void pmu_command_counter_prt(
	struct apusys_cmd_hnd *apusys_hd,
	struct mdla_dev *mdla_info,
	u16 priority,
	struct command_entry *ce)
{
	int i;
	struct mdla_pmu_result result;
	struct mdla_pmu_result check;
	void *base = NULL, *desc = NULL, *src = NULL;
	uint32_t sz = 0;
	uint16_t event_num = 0;
	int offset = 0;
	u16 final_len = 0;
	u16 loop_count =
		mdla_info->pmu[priority].pmu_hnd->number_of_event;
	u32 cid = mdla_info->mdlaid;
	uint32_t repeat_sz = 0;
	uint32_t out_sz = 0;
	uint32_t out_length = 0;

	result.cmd_len = l_cmd_cnt[mdla_info->mdlaid][priority];
	result.cmd_id = pmu_get_perf_cmdid(mdla_info->mdlaid, priority);
	event_num = mdla_info->pmu[priority].pmu_hnd->number_of_event + 1;

	sz = sizeof(u16) * 2 + sizeof(u32) * event_num;
	repeat_sz = sz - sizeof(u16);

	if (mdla_info->mdlaid == 0) {
		base = (void *)mdla_info->pmu[priority].PMU_res_buf_addr0;
		out_sz = apusys_hd->cmd_entry -
				mdla_info->pmu[priority].PMU_res_buf_addr0 +
				apusys_hd->cmd_size;
	} else if (mdla_info->mdlaid == 1) {
		base = (void *)mdla_info->pmu[priority].PMU_res_buf_addr1;
		out_sz = apusys_hd->cmd_entry -
				mdla_info->pmu[priority].PMU_res_buf_addr1 +
				apusys_hd->cmd_size;
	} else {
		mdla_pmu_debug("unknown mdlaid: %d\n", mdla_info->mdlaid);
		return;
	}
	if (apusys_hd->cmd_entry > (uint64_t)base)
		return;
	if ((uint64_t)base > (apusys_hd->cmd_entry + apusys_hd->cmd_size))
		return;
	if (out_sz < sizeof(uint16_t))
		return;
	out_length = (out_sz - sizeof(uint16_t)) / repeat_sz;
	if (loop_count > MDLA_PMU_COUNTERS)
		return;
	if (mdla_info->pmu[priority].pmu_mode == PER_CMD) {
		if (ce->count > out_length)
			return;
	} else if (mdla_info->pmu[priority].pmu_mode == NORMAL) {
		if (out_sz < sz)
			return;
	}

	mdla_pmu_debug("mode: %d, cmd_len: %d, cmd_id: %d, sz: %d\n",
		       mdla_info->pmu[priority].pmu_hnd->mode,
		       result.cmd_len, result.cmd_id, sz);

	if (mdla_info->pmu[priority].pmu_mode == PER_CMD)
		result.pmu_val[0] =
			pmu_get_perf_end(mdla_info->mdlaid, priority);
	else
		result.pmu_val[0] =
			pmu_get_perf_cycle(mdla_info->mdlaid, priority);

	mdla_pmu_debug("global counter:%08x\n", result.pmu_val[0]);

	for (i = 0; i < loop_count; i++) {
		result.pmu_val[i + 1] =
			pmu_counter_get(mdla_info->mdlaid,
				pmu_event_handle[cid][priority][i],
				priority);
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
		if (unlikely(mdla_klog&MDLA_DBG_PMU)) {
			memcpy(&check, desc, sz);

			mdla_pmu_debug("[-] check cmd_len: %d\n",
				check.cmd_len);
			mdla_pmu_debug("[-] check cmd_id: %d\n",
				check.cmd_id);
			mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
			       check.pmu_val[1]);

			if (result.cmd_len > 1) {
				offset =
					sz +
					(result.cmd_len - 2) *
					(sz - sizeof(u16));
				desc = (void *)(base + offset);
				memcpy(
					(void *)&(check.cmd_id),
					desc,
					sz - sizeof(u16));

				mdla_pmu_debug("[-] offset: %d\n", offset);
				mdla_pmu_debug("[-] check cmd_id: %d\n",
					check.cmd_id);
				mdla_pmu_debug("[-] check cmd_val[1]:  %08x\n",
				       check.pmu_val[1]);
			}
		}
	}
}

