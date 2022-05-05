// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <apusys_device.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>
#include <utilities/mdla_profile.h>

#include <common/mdla_power_ctrl.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_ioctl.h>

#include <platform/mdla_plat_api.h>

#include "mdla_pmu_v1_x.h"
#include "mdla_hw_reg_v1_x.h"


#define biu_read(id, ofs) \
	mdla_util_io_ops_get()->biu.read(id, ofs)
#define biu_write(id, ofs, val) \
	mdla_util_io_ops_get()->biu.write(id, ofs, val)
#define biu_set_b(id, ofs, val) \
	mdla_util_io_ops_get()->biu.set_b(id, ofs, val)
#define biu_clr_b(id, ofs, val) \
	mdla_util_io_ops_get()->biu.clr_b(id, ofs, val)

struct mdla_pmu_dev {
	spinlock_t lock;
	DECLARE_BITMAP(pmu_bitmap, MDLA_PMU_COUNTERS);
	u32 cfg_event[MDLA_PMU_COUNTERS];
};

struct mdla_pmu_data {
	u8 cfg_percmd_mode;
	u16 l_cmd_cnt;
	u16 l_cmd_id;
	u32 l_start_t;
	u32 l_end_t;
	u32 l_cycle;
	u32 l_counters[MDLA_PMU_COUNTERS];
	u32 event_handle[MDLA_PMU_COUNTERS];
};

/* pmu info handle (sync with NN)*/
struct mdla_pmu_hnd {
	u32 graph_id;//need confirm apusys mid data type
	u32 offset_to_PMU_res_buf0;//base addr: pmu_kva
	u32 offset_to_PMU_res_buf1;
	u8 mode;
	u8 number_of_event;
	u16 event[MDLA_PMU_COUNTERS];
} __attribute__((__packed__));

struct mdla_pmu_info {
	u64 cmd_id;
	u64 PMU_res_buf_addr;
	u32 PMU_res_buf_size;
	struct mdla_pmu_hnd *pmu_hnd;
	struct mdla_pmu_data data;
	u32 number_of_event;
	u8 pmu_mode;
};

static u32 mdla_pmu_get_perf_end(struct mdla_pmu_info *pmu)
{
	return pmu->data.l_end_t;
}

static u32 mdla_pmu_get_perf_cycle(struct mdla_pmu_info *pmu)
{
	return pmu->data.l_cycle;
}

static u16 mdla_pmu_get_perf_cmdid(struct mdla_pmu_info *pmu)
{
	return pmu->data.l_cmd_id;
}

static u16 mdla_pmu_get_perf_cmdcnt(struct mdla_pmu_info *pmu)
{
	return pmu->data.l_cmd_cnt;
}

static u32 mdla_pmu_get_counter(struct mdla_pmu_info *pmu, u32 idx)
{
	return pmu->data.l_counters[idx];
}

static int mdla_pmu_get_mode(struct mdla_pmu_info *pmu)
{
	return (int)pmu->pmu_mode;
}

static u32 mdla_pmu_get_hnd_evt(struct mdla_pmu_info *pmu, u32 counter_idx)
{
	return pmu->pmu_hnd->event[counter_idx];
}

static u32 mdla_pmu_get_hnd_evt_num(struct mdla_pmu_info *pmu)
{
	return pmu->pmu_hnd->number_of_event;
}

static u32 mdla_pmu_get_hnd_mode(struct mdla_pmu_info *pmu)
{
	return pmu->pmu_hnd->mode;
}

static u64 mdla_pmu_get_hnd_buf_addr(struct mdla_pmu_info *pmu)
{
	return pmu->PMU_res_buf_addr;
}

static u32 mdla_pmu_get_hnd_buf_size(struct mdla_pmu_info *pmu)
{
	return pmu->PMU_res_buf_size;
}

static void mdla_pmu_set_evt_handle(struct mdla_pmu_info *pmu,
		u32 counter_idx, u32 val)
{
	pmu->data.event_handle[counter_idx] = val;
}

static struct mdla_pmu_info *mdla_pmu_get_info(u32 core_id, u16 priority)
{
	return &mdla_get_device(core_id)->pmu_info[priority];
}

/* extract pmu_hnd form pmu_kva and set mdla_info->pmu */
static int mdla_pmu_cmd_prepare(struct mdla_dev *mdla_info,
	struct apusys_cmd_handle *apusys_hd, u16 priority)
{
	u32 buf_idx;
	struct mdla_pmu_info *pmu;

	pmu = &mdla_info->pmu_info[priority];

	if (pmu->pmu_mode >= CMD_MODE_MAX)
		return -1;

	if (apusys_hd->cmdbufs[CMD_PMU_INFO_IDX].size < sizeof(struct mdla_pmu_hnd))
		return -1;

	pmu->pmu_hnd =
		(struct mdla_pmu_hnd *)apusys_hd->cmdbufs[CMD_PMU_INFO_IDX].kva;

	pmu->cmd_id = apusys_hd->kid;

	if ((mdla_info->mdla_id == 0) || (apusys_hd->multicore_total != 2))
		buf_idx = CMD_PMU_BUF_0_IDX;
	else if (mdla_info->mdla_id == 1)
		buf_idx = CMD_PMU_BUF_1_IDX;
	else
		return -1;

	pmu->PMU_res_buf_addr = (u64)apusys_hd->cmdbufs[buf_idx].kva;
	pmu->PMU_res_buf_size = apusys_hd->cmdbufs[buf_idx].size;

	if (pmu->pmu_mode == PER_CMD)
		mdla_util_pmu_cmd_timer(true);

	mdla_pmu_debug("pmu addr: %08llx\n", pmu->PMU_res_buf_addr);

	return 0;
}

static int mdla_pmu_event_write(u32 core_id, u32 handle, u32 val)
{
	if (val == COUNTER_CLEAR) {
		mdla_pmu_debug("%s: clear pmu counter[%d]\n",
					__func__, handle);

		biu_write(core_id, PMU(handle), 0);
		biu_clr_b(core_id, CFG_PMCR, PMU_CNT_EN(handle));
	} else {
		mdla_pmu_debug("%s: set pmu counter[%d] = 0x%x\n",
				__func__, handle, val);

		biu_write(core_id, PMU(handle), val);
		biu_set_b(core_id, CFG_PMCR, PMU_CNT_EN(handle));
	}

	return 0;
}

static void mdla_pmu_event_write_all(u32 core_id, u16 priority)
{
	int i, cnt;
	u32 val;
	struct mdla_pmu_info *pmu;

	pmu = &(mdla_get_device(core_id)->pmu_info[priority]);

	cnt = mdla_prof_use_dbgfs_pmu_event(core_id)
		? MDLA_PMU_COUNTERS : pmu->number_of_event;

	for (i = 0; i < cnt; i++) {
		val = i < MDLA_PMU_COUNTERS ?
				pmu->data.event_handle[i] : COUNTER_CLEAR;
		mdla_pmu_event_write(core_id, i, val);
	}
}

static u32 mdla_pmu_get_num_evt(u32 core_id, int priority)
{
	if (mdla_prof_use_dbgfs_pmu_event(core_id))
		return MDLA_PMU_COUNTERS;

	if (priority >= PRIORITY_LEVEL)
		return 0;

	return mdla_get_device(core_id)->pmu_info[priority].number_of_event;
}

static void mdla_pmu_set_num_evt(u32 core_id, int prio, int val)
{
	if (prio < PRIORITY_LEVEL)
		mdla_get_device(core_id)->pmu_info[prio].number_of_event = val;
}

#define pmu_is_percmd_mode(i)\
	(biu_read(i, CFG_PMCR) & PMU_PERCMD_MODE)

/* for mdla_trace and apusys_hnd */
static void mdla_pmu_counter_read_all(u32 core_id, u32 out[MDLA_PMU_COUNTERS])
{
	int i;

	if (pmu_is_percmd_mode(core_id)) {
		for (i = 0; i < MDLA_PMU_COUNTERS; i++)
			out[i] += biu_read(core_id, PMU_CNT_LATCH(i));
	} else {
		for (i = 0; i < MDLA_PMU_COUNTERS; i++)
			out[i] += biu_read(core_id, PMU_CNT(i));
	}
}

static void mdla_pmu_counter_disable_all(u32 core_id)
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		biu_clr_b(core_id, CFG_PMCR, PMU_CNT_EN(i));
}

static void mdla_pmu_counter_enable_all(u32 core_id)
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++) {
		if (biu_read(core_id, PMU(i)))
			biu_set_b(core_id, CFG_PMCR, PMU_CNT_EN(i));
	}
}

/* save pmu registers for query after power off */
static void mdla_pmu_counter_save(u32 core_id, struct mdla_pmu_info *pmu)
{
	if (pmu->pmu_mode == NORMAL) {
		pmu->data.l_cmd_cnt = 1;
		pmu->data.l_cmd_id = 0;
	} else {
		u32 val = biu_read(core_id, PMU_CMDID_LATCH);

		if (val != pmu->data.l_cmd_id) {
			pmu->data.l_cmd_cnt++;
			pmu->data.l_cmd_id = (u16)val;
		}
	}

	pmu->data.l_cycle +=
		biu_read(core_id, PMU_CLK_CNT);
	pmu->data.l_end_t +=
		biu_read(core_id, PMU_END_TSTAMP);
	pmu->data.l_start_t =
		biu_read(core_id, PMU_START_TSTAMP);

	mdla_pmu_counter_read_all(core_id, pmu->data.l_counters);
}

/* it save mode setting to local variable */
static void mdla_pmu_percmd_mode_set(struct mdla_pmu_info *pmu, u32 mode)
{
	pmu->data.cfg_percmd_mode = mode;
}

/* for ioctrl */
static void mdla_pmu_percmd_mode_write(u32 core_id, struct mdla_pmu_info *pmu)
{
	if (pmu->data.cfg_percmd_mode)
		biu_set_b(core_id, CFG_PMCR, PMU_PERCMD_MODE);
	else
		biu_clr_b(core_id, CFG_PMCR, PMU_PERCMD_MODE);
}

/* for ioctrl */
static int mdla_pmu_counter_event_set(u32 core_id, int handle, u32 val)
{
	struct mdla_pmu_dev *pmu_dev = mdla_get_device(core_id)->pmu_dev;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	if (handle < MDLA_PMU_COUNTERS)
		pmu_dev->cfg_event[handle] = val;

	mdla_pmu_event_write(core_id, handle, val);

	return 0;
}

/* for ioctrl */
static int mdla_pmu_counter_event_get(u32 core_id, int handle)
{
	u32 event;
	struct mdla_pmu_dev *pmu_dev = mdla_get_device(core_id)->pmu_dev;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	event = pmu_dev->cfg_event[handle];

	return (event == COUNTER_CLEAR) ? -ENOENT : event;
}

static void mdla_pmu_reset_and_write_event(u32 core_id, u16 priority)
{
	biu_set_b(core_id,
		CFG_PMCR,
		(CFG_PMCR_DEFAULT | PMU_CNT_RESET
			| PMU_CCNT_EN | PMU_CCNT_RESET));

	while ((biu_read(core_id, CFG_PMCR) &
		(PMU_CNT_RESET | PMU_CCNT_RESET)) != 0) {
	}

	mdla_pmu_percmd_mode_write(core_id,
		&(mdla_get_device(core_id)->pmu_info[priority]));
	mdla_pmu_event_write_all(core_id, priority);
}

static void mdla_pmu_reset_counter_variable(struct mdla_pmu_info *pmu)
{
	int i;

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		pmu->data.l_counters[i] = 0;

	pmu->data.l_cmd_id = 0;
	pmu->data.l_cmd_cnt = 0;
}

static void mdla_pmu_reset_cycle_variable(struct mdla_pmu_info *pmu)
{
	pmu->data.l_cycle = 0;
	pmu->data.l_end_t = 0;
	pmu->data.l_start_t = 0;
}

static void mdla_pmu_reset_counter_no_lock(u32 core_id)
{
	mdla_pmu_debug("mdla: %s\n", __func__);

	/*Reset Clock counter to zero. Return to zero when reset done.*/
	biu_set_b(core_id, CFG_PMCR, PMU_CNT_RESET);
	while ((biu_read(core_id, CFG_PMCR) & PMU_CNT_RESET)
				!= 0) {
	}
}

static void mdla_pmu_reset_counter(u32 core_id)
{
	unsigned long flags;
	struct mdla_pmu_dev *pmu_dev;

	pmu_dev = mdla_get_device(core_id)->pmu_dev;

	spin_lock_irqsave(&pmu_dev->lock, flags);
	mdla_pmu_reset_counter_no_lock(core_id);
	spin_unlock_irqrestore(&pmu_dev->lock, flags);
}

static void mdla_pmu_reset_cycle(u32 core_id)
{
	mdla_pmu_debug("mdla: %s\n", __func__);
	biu_set_b(core_id,
			CFG_PMCR,
			(PMU_CCNT_EN | PMU_CCNT_RESET));
	while ((biu_read(core_id, CFG_PMCR) & PMU_CCNT_RESET)
				!= 0) {
	}
}

static void mdla_pmu_reset(u32 core_id)
{
	int i;

	biu_write(core_id, CFG_PMCR,
		(CFG_PMCR_DEFAULT | PMU_CCNT_RESET | PMU_CNT_RESET));

	while ((biu_read(core_id, CFG_PMCR) &
		(PMU_CCNT_RESET | PMU_CNT_RESET)) != 0) {
	}
	/* reset to 0 */
	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		mdla_pmu_event_write(core_id, i, COUNTER_CLEAR);

	/* reset to normal */
	biu_clr_b(core_id, CFG_PMCR, PMU_PERCMD_MODE);

	mdla_pmu_debug("mdla: %s, CFG_PMCR: 0x%x\n",
		__func__, biu_read(core_id, CFG_PMCR));
}

/* for ioctrl */
static int mdla_pmu_counter_alloc(u32 core_id)
{
	unsigned long flags;
	int handle;
	struct mdla_pmu_dev *pmu_dev = mdla_get_device(core_id)->pmu_dev;

	if (unlikely(!pmu_dev))
		return -ENODEV;

	spin_lock_irqsave(&pmu_dev->lock, flags);

	handle = bitmap_find_free_region(pmu_dev->pmu_bitmap,
				MDLA_PMU_COUNTERS, 0);

	spin_unlock_irqrestore(&pmu_dev->lock, flags);

	if (unlikely(handle < 0))
		return -EINVAL;

	return handle;
}

/* for ioctrl */
static int mdla_pmu_counter_free(u32 core_id, int handle)
{
	unsigned long flags;
	int ret = 0;
	struct mdla_pmu_dev *pmu_dev = mdla_get_device(core_id)->pmu_dev;

	if ((handle >= MDLA_PMU_COUNTERS) || (handle < 0))
		return -EINVAL;

	spin_lock_irqsave(&pmu_dev->lock, flags);

	bitmap_release_region(pmu_dev->pmu_bitmap, handle, 0);
	mdla_pmu_event_write(core_id, handle, COUNTER_CLEAR);

	spin_unlock_irqrestore(&pmu_dev->lock, flags);

	return ret;
}

static int mdla_pmu_ioctl(struct file *filp,
		u32 command, unsigned long arg, bool need_pwr_on)
{
	long retval = 0;
	struct mdla_dev *mdla_device;
	struct mdla_pmu_info *pmu_info;
	struct ioctl_perf perf_data;
	u32 core_id;
	int i;

	if (copy_from_user(&perf_data, (void *) arg, sizeof(perf_data)))
		return -EFAULT;

	core_id = perf_data.mdlaid;

	if (core_id_is_invalid(core_id))
		return -EINVAL;

	mdla_device = mdla_get_device(core_id);

	/* set priority level = 1 for ioctl */
	pmu_info = &mdla_device->pmu_info[0];

	if (need_pwr_on) {
		mdla_pwr_ops_get()->lock(core_id);
		if (mdla_device->power_is_on == false) {
			mdla_err("mdla%d power off, pmu ioctl FAIL\n", core_id);
			goto out;
		}
	}

	switch (command) {
	case IOCTL_PERF_SET_EVENT:
		perf_data.handle = mdla_pmu_counter_alloc(core_id);
		if ((int)perf_data.handle > 0)
			mdla_pmu_counter_event_set(core_id,
				perf_data.handle,
				(perf_data.interface << 16) | perf_data.event);
		break;
	case IOCTL_PERF_UNSET_EVENT:
		mdla_pmu_counter_free(core_id, perf_data.handle);
		break;
	case IOCTL_PERF_GET_EVENT:
		perf_data.event = mdla_pmu_counter_event_get(
					core_id, perf_data.handle);
		break;
	case IOCTL_PERF_GET_CNT:
		if (perf_data.handle < MDLA_PMU_COUNTERS)
			perf_data.counter = pmu_info->data.l_counters[perf_data.handle];
		break;
	case IOCTL_PERF_GET_START:
		perf_data.start = pmu_info->data.l_start_t;
		break;
	case IOCTL_PERF_GET_END:
		perf_data.end = pmu_info->data.l_end_t;
		break;
	case IOCTL_PERF_GET_CYCLE:
		perf_data.start = pmu_info->data.l_cycle;
		break;
	case IOCTL_PERF_RESET_CNT:
		mdla_cmd_ops_get()->lock(mdla_device);

		for (i = 0; i < PRIORITY_LEVEL; i++)
			mdla_pmu_reset_counter_variable(
					&mdla_device->pmu_info[i]);
		mdla_pmu_reset_counter(core_id);

		mdla_cmd_ops_get()->unlock(mdla_device);
		break;
	case IOCTL_PERF_RESET_CYCLE:
		mdla_cmd_ops_get()->lock(mdla_device);

		for (i = 0; i < PRIORITY_LEVEL; i++)
			mdla_pmu_reset_cycle_variable(
					&mdla_device->pmu_info[i]);
		mdla_pmu_reset_cycle(core_id);

		mdla_cmd_ops_get()->unlock(mdla_device);
		break;
	case IOCTL_PERF_SET_MODE:
		mdla_cmd_ops_get()->lock(mdla_device);

		mdla_pmu_percmd_mode_set(pmu_info, perf_data.mode);
		mdla_pmu_percmd_mode_write(core_id, pmu_info);

		mdla_cmd_ops_get()->unlock(mdla_device);
		break;
	default:
		break;
	}

	if (copy_to_user((void *) arg, &perf_data, sizeof(perf_data)))
		retval = -EFAULT;

out:
	if (need_pwr_on)
		mdla_pwr_ops_get()->unlock(core_id);

	return retval;
}

void mdla_v1_x_pmu_info_show(struct seq_file *s)
{
	int i, p, c;
	struct mdla_pmu_info *pmu;

	for_each_mdla_core(i) {

		seq_printf(s, "%d: CFG_PMCR(0x%08x)=0x%x\n",
			i, CFG_PMCR,
			biu_read(i, CFG_PMCR));
		seq_printf(s, "%d: PMU_CLK_CNT(0x%08x)=0x%x\n",
			i, PMU_CLK_CNT,
			biu_read(i, PMU_CLK_CNT));
		seq_printf(s, "%d: PMU_START_TSTAMP(0x%08x)=0x%x\n",
			i, PMU_START_TSTAMP,
			biu_read(i, PMU_START_TSTAMP));
		seq_printf(s, "%d: PMU_END_TSTAMP(0x%08x)=0x%x\n",
			i, PMU_END_TSTAMP,
			biu_read(i, PMU_END_TSTAMP));
		seq_printf(s, "%d: PMU_CMDID_LATCH(0x%08x)=0x%x\n",
			i, PMU_CMDID_LATCH,
			biu_read(i, PMU_CMDID_LATCH));

		for (c = 0; c < MDLA_PMU_COUNTERS; c++) {
			seq_printf(s, "%d: counter %2d, 0x%04x=0x%-8x, 0x%04x=%d, 0x%04x=%d\n",
				i, c,
				PMU(c),
				biu_read(c, PMU(c)),
				PMU_CNT(c),
				biu_read(c, PMU_CNT(c)),
				PMU_CNT_LATCH(c),
				biu_read(c, PMU_CNT_LATCH(c)));
		}

		for (p = 0; p < PRIORITY_LEVEL; p++) {

			pmu = &mdla_get_device(i)->pmu_info[p];

			seq_printf(s, "%d: priority = %d, number_of_event = %d\n",
				i, p, pmu->number_of_event);

			for (c = 0; c < MDLA_PMU_COUNTERS; c++)
				seq_printf(s, "%d: priority=%d, counter %2d : event=0x%-8x, count=%d\n",
						i, p, c,
						pmu->data.event_handle[c],
						pmu->data.l_counters[c]);
		}
	}
	seq_printf(s, "set event by %s\n",
		mdla_dbg_read_u32(FS_PMU_EVT_BY_APU) ? "apusys" : "debugfs");
}


void mdla_v1_x_pmu_init(struct mdla_dev *mdla_info)
{
	int i;
	struct mdla_pmu_info *info;
	struct mdla_pmu_dev *dev;
	struct mdla_util_pmu_ops *pmu_ops = mdla_util_pmu_ops_get();

	info = kcalloc(PRIORITY_LEVEL, sizeof(struct mdla_pmu_info),
					GFP_KERNEL);
	if (!info)
		return;

	dev = kzalloc(sizeof(struct mdla_pmu_dev), GFP_KERNEL);

	if (!dev) {
		kfree(info);
		return;
	}

	for (i = 0; i < PRIORITY_LEVEL; i++)
		info[i].data.cfg_percmd_mode = NORMAL;

	spin_lock_init(&dev->lock);

	mdla_info->pmu_dev = dev;
	mdla_info->pmu_info = info;

	mdla_ioctl_register_perf_handle(mdla_pmu_ioctl);

	pmu_ops->reg_counter_save     = mdla_pmu_counter_save;
	pmu_ops->reg_counter_read     = mdla_pmu_counter_read_all;
	pmu_ops->clr_counter_variable = mdla_pmu_reset_counter_variable;
	pmu_ops->clr_cycle_variable   = mdla_pmu_reset_cycle_variable;
	pmu_ops->get_num_evt          = mdla_pmu_get_num_evt;
	pmu_ops->set_num_evt          = mdla_pmu_set_num_evt;
	pmu_ops->set_percmd_mode      = mdla_pmu_percmd_mode_set;
	pmu_ops->get_curr_mode        = mdla_pmu_get_mode;
	pmu_ops->get_perf_end         = mdla_pmu_get_perf_end;
	pmu_ops->get_perf_cycle       = mdla_pmu_get_perf_cycle;
	pmu_ops->get_perf_cmdid       = mdla_pmu_get_perf_cmdid;
	pmu_ops->get_perf_cmdcnt      = mdla_pmu_get_perf_cmdcnt;
	pmu_ops->get_counter          = mdla_pmu_get_counter;
	pmu_ops->reset_counter        = mdla_pmu_reset_counter;
	pmu_ops->disable_counter      = mdla_pmu_counter_disable_all;
	pmu_ops->enable_counter       = mdla_pmu_counter_enable_all;
	pmu_ops->reset                = mdla_pmu_reset;
	pmu_ops->write_evt_exec       = mdla_pmu_event_write_all;
	pmu_ops->reset_write_evt_exec = mdla_pmu_reset_and_write_event;

	pmu_ops->get_hnd_evt          = mdla_pmu_get_hnd_evt;
	pmu_ops->get_hnd_evt_num      = mdla_pmu_get_hnd_evt_num;
	pmu_ops->get_hnd_mode         = mdla_pmu_get_hnd_mode;
	pmu_ops->get_hnd_buf_addr     = mdla_pmu_get_hnd_buf_addr;
	pmu_ops->get_hnd_buf_size     = mdla_pmu_get_hnd_buf_size;
	pmu_ops->set_evt_handle       = mdla_pmu_set_evt_handle;
	pmu_ops->get_info             = mdla_pmu_get_info;
	pmu_ops->apu_cmd_prepare      = mdla_pmu_cmd_prepare;

	if (mdla_plat_nn_pmu_support())
		mdla_util_apusys_pmu_support(true);
	else
		mdla_util_apusys_pmu_support(false);
}

void mdla_v1_x_pmu_deinit(struct mdla_dev *mdla_info)
{
	mdla_util_apusys_pmu_support(false);
	mdla_ioctl_unregister_perf_handle();

	kfree(mdla_info->pmu_dev);
	kfree(mdla_info->pmu_info);
}

