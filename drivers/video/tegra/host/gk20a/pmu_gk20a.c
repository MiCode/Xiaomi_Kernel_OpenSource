/*
 * drivers/video/tegra/host/gk20a/pmu_gk20a.c
 *
 * GK20A PMU (aka. gPMU outside gk20a context)
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/delay.h>	/* for mdelay */
#include <linux/firmware.h>
#include <linux/nvmap.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "../dev.h"
#include "../bus_client.h"
#include "nvhost_memmgr.h"
#include "nvhost_acm.h"

#include "gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_pwr_gk20a.h"
#include "hw_top_gk20a.h"
#include "chip_support.h"

#define GK20A_PMU_UCODE_IMAGE	"gpmu_ucode.bin"

#define nvhost_dbg_pmu(fmt, arg...) \
	nvhost_dbg(dbg_pmu, fmt, ##arg)

static void pmu_dump_falcon_stats(struct pmu_gk20a *pmu);
static int gk20a_pmu_get_elpg_residency_gating(struct gk20a *g,
		u32 *ingating_time, u32 *ungating_time, u32 *gating_cnt);
static void ap_callback_init_and_enable_ctrl(
		struct gk20a *g, struct pmu_msg *msg,
		void *param, u32 seq_desc, u32 status);
static int gk20a_pmu_ap_send_command(struct gk20a *g,
			union pmu_ap_cmd *p_ap_cmd, bool b_block);

static void pmu_copy_from_dmem(struct pmu_gk20a *pmu,
			u32 src, u8* dst, u32 size, u8 port)
{
	struct gk20a *g = pmu->g;
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *dst_u32 = (u32*)dst;

	if (size == 0) {
		nvhost_err(dev_from_gk20a(g),
			"size is zero");
		return;
	}

	if (src & 0x3) {
		nvhost_err(dev_from_gk20a(g),
			"src (0x%08x) not 4-byte aligned", src);
		return;
	}

	mutex_lock(&pmu->pmu_copy_lock);

	words = size >> 2;
	bytes = size & 0x3;

	addr_mask = pwr_falcon_dmemc_offs_m() |
		    pwr_falcon_dmemc_blk_m();

	src &= addr_mask;

	gk20a_writel(g, pwr_falcon_dmemc_r(port),
		src | pwr_falcon_dmemc_aincr_f(1));

	for (i = 0; i < words; i++)
		dst_u32[i] = gk20a_readl(g, pwr_falcon_dmemd_r(port));

	if (bytes > 0) {
		data = gk20a_readl(g, pwr_falcon_dmemd_r(port));
		for (i = 0; i < bytes; i++) {
			dst[(words << 2) + i] = ((u8 *)&data)[i];
			nvhost_dbg_pmu("read: dst_u8[%d]=0x%08x",
					i, dst[(words << 2) + i]);
		}
	}
	mutex_unlock(&pmu->pmu_copy_lock);
	return;
}

static void pmu_copy_to_dmem(struct pmu_gk20a *pmu,
			u32 dst, u8* src, u32 size, u8 port)
{
	struct gk20a *g = pmu->g;
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *src_u32 = (u32*)src;

	if (size == 0) {
		nvhost_err(dev_from_gk20a(g),
			"size is zero");
		return;
	}

	if (dst & 0x3) {
		nvhost_err(dev_from_gk20a(g),
			"dst (0x%08x) not 4-byte aligned", dst);
		return;
	}

	mutex_lock(&pmu->pmu_copy_lock);

	words = size >> 2;
	bytes = size & 0x3;

	addr_mask = pwr_falcon_dmemc_offs_m() |
		    pwr_falcon_dmemc_blk_m();

	dst &= addr_mask;

	gk20a_writel(g, pwr_falcon_dmemc_r(port),
		dst | pwr_falcon_dmemc_aincw_f(1));

	for (i = 0; i < words; i++)
		gk20a_writel(g, pwr_falcon_dmemd_r(port), src_u32[i]);

	if (bytes > 0) {
		data = 0;
		for (i = 0; i < bytes; i++)
			((u8 *)&data)[i] = src[(words << 2) + i];
		gk20a_writel(g, pwr_falcon_dmemd_r(port), data);
	}

	data = gk20a_readl(g, pwr_falcon_dmemc_r(port)) & addr_mask;
	size = ALIGN(size, 4);
	if (data != dst + size) {
		nvhost_err(dev_from_gk20a(g),
			"copy failed. bytes written %d, expected %d",
			data - dst, size);
	}
	mutex_unlock(&pmu->pmu_copy_lock);
	return;
}

static int pmu_idle(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(2000);
	u32 idle_stat;

	/* wait for pmu idle */
	do {
		idle_stat = gk20a_readl(g, pwr_falcon_idlestate_r());

		if (pwr_falcon_idlestate_falcon_busy_v(idle_stat) == 0 &&
		    pwr_falcon_idlestate_ext_busy_v(idle_stat) == 0) {
			break;
		}

		if (time_after_eq(jiffies, end_jiffies)) {
			nvhost_err(dev_from_gk20a(g),
				"timeout waiting pmu idle : 0x%08x",
				idle_stat);
			return -EBUSY;
		}
		usleep_range(100, 200);
	} while (1);

	nvhost_dbg_fn("done");
	return 0;
}

static void pmu_enable_irq(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = pmu->g;

	nvhost_dbg_fn("");

	gk20a_writel(g, mc_intr_mask_0_r(),
		gk20a_readl(g, mc_intr_mask_0_r()) &
		~mc_intr_mask_0_pmu_enabled_f());
	gk20a_writel(g, mc_intr_mask_1_r(),
		gk20a_readl(g, mc_intr_mask_1_r()) &
		~mc_intr_mask_1_pmu_enabled_f());

	gk20a_writel(g, pwr_falcon_irqmclr_r(),
		pwr_falcon_irqmclr_gptmr_f(1)  |
		pwr_falcon_irqmclr_wdtmr_f(1)  |
		pwr_falcon_irqmclr_mthd_f(1)   |
		pwr_falcon_irqmclr_ctxsw_f(1)  |
		pwr_falcon_irqmclr_halt_f(1)   |
		pwr_falcon_irqmclr_exterr_f(1) |
		pwr_falcon_irqmclr_swgen0_f(1) |
		pwr_falcon_irqmclr_swgen1_f(1) |
		pwr_falcon_irqmclr_ext_f(0xff));

	if (enable) {
		/* dest 0=falcon, 1=host; level 0=irq0, 1=irq1 */
		gk20a_writel(g, pwr_falcon_irqdest_r(),
			pwr_falcon_irqdest_host_gptmr_f(0)    |
			pwr_falcon_irqdest_host_wdtmr_f(1)    |
			pwr_falcon_irqdest_host_mthd_f(0)     |
			pwr_falcon_irqdest_host_ctxsw_f(0)    |
			pwr_falcon_irqdest_host_halt_f(1)     |
			pwr_falcon_irqdest_host_exterr_f(0)   |
			pwr_falcon_irqdest_host_swgen0_f(1)   |
			pwr_falcon_irqdest_host_swgen1_f(0)   |
			pwr_falcon_irqdest_host_ext_f(0xff)   |
			pwr_falcon_irqdest_target_gptmr_f(1)  |
			pwr_falcon_irqdest_target_wdtmr_f(0)  |
			pwr_falcon_irqdest_target_mthd_f(0)   |
			pwr_falcon_irqdest_target_ctxsw_f(0)  |
			pwr_falcon_irqdest_target_halt_f(0)   |
			pwr_falcon_irqdest_target_exterr_f(0) |
			pwr_falcon_irqdest_target_swgen0_f(0) |
			pwr_falcon_irqdest_target_swgen1_f(0) |
			pwr_falcon_irqdest_target_ext_f(0xff));

		/* 0=disable, 1=enable */
		gk20a_writel(g, pwr_falcon_irqmset_r(),
			pwr_falcon_irqmset_gptmr_f(1)  |
			pwr_falcon_irqmset_wdtmr_f(1)  |
			pwr_falcon_irqmset_mthd_f(0)   |
			pwr_falcon_irqmset_ctxsw_f(0)  |
			pwr_falcon_irqmset_halt_f(1)   |
			pwr_falcon_irqmset_exterr_f(1) |
			pwr_falcon_irqmset_swgen0_f(1) |
			pwr_falcon_irqmset_swgen1_f(1));

		gk20a_writel(g, mc_intr_mask_0_r(),
			gk20a_readl(g, mc_intr_mask_0_r()) |
			mc_intr_mask_0_pmu_enabled_f());
	}

	nvhost_dbg_fn("done");
}

static void pmu_enable_hw(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = pmu->g;

	nvhost_dbg_fn("");

	if (enable)
		gk20a_enable(g, mc_enable_pwr_enabled_f());
	else
		gk20a_disable(g, mc_enable_pwr_enabled_f());
}

static int pmu_enable(struct pmu_gk20a *pmu, bool enable)
{
	struct gk20a *g = pmu->g;
	u32 pmc_enable;
	int err;

	nvhost_dbg_fn("");

	if (!enable) {
		pmc_enable = gk20a_readl(g, mc_enable_r());
		if (mc_enable_pwr_v(pmc_enable) !=
		    mc_enable_pwr_disabled_v()) {

			pmu_enable_irq(pmu, false);
			pmu_enable_hw(pmu, false);
		}
	} else {
		pmu_enable_hw(pmu, true);

		/* TBD: post reset */

		err = pmu_idle(pmu);
		if (err)
			return err;

		pmu_enable_irq(pmu, true);
	}

	nvhost_dbg_fn("done");
	return 0;
}

static int pmu_reset(struct pmu_gk20a *pmu)
{
	int err;

	err = pmu_idle(pmu);
	if (err)
		return err;

	/* TBD: release pmu hw mutex */

	err = pmu_enable(pmu, false);
	if (err)
		return err;

	/* TBD: cancel all sequences */
	/* TBD: init all sequences and state tables */
	/* TBD: restore pre-init message handler */

	err = pmu_enable(pmu, true);
	if (err)
		return err;

	return 0;
}

static int pmu_bootstrap(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct nvhost_device_data *pdata = platform_get_drvdata(g->dev);
	struct mm_gk20a *mm = &g->mm;
	struct pmu_ucode_desc *desc = pmu->desc;
	u64 addr_code, addr_data, addr_load;
	u32 i, blocks, addr_args;

	nvhost_dbg_fn("");

	gk20a_writel(g, pwr_falcon_itfen_r(),
		gk20a_readl(g, pwr_falcon_itfen_r()) |
		pwr_falcon_itfen_ctxen_enable_f());
	gk20a_writel(g, pwr_pmu_new_instblk_r(),
		pwr_pmu_new_instblk_ptr_f(
			mm->pmu.inst_block.cpu_pa >> 12) |
		pwr_pmu_new_instblk_valid_f(1) |
		pwr_pmu_new_instblk_target_sys_coh_f());

	/* TBD: load all other surfaces */

	pmu->args.cpu_freq_HZ = clk_get_rate(pdata->clk[1]);

	addr_args = (pwr_falcon_hwcfg_dmem_size_v(
		gk20a_readl(g, pwr_falcon_hwcfg_r()))
			<< GK20A_PMU_DMEM_BLKSIZE2) -
		sizeof(struct pmu_cmdline_args);

	pmu_copy_to_dmem(pmu, addr_args, (u8 *)&pmu->args,
			sizeof(struct pmu_cmdline_args), 0);

	gk20a_writel(g, pwr_falcon_dmemc_r(0),
		pwr_falcon_dmemc_offs_f(0) |
		pwr_falcon_dmemc_blk_f(0)  |
		pwr_falcon_dmemc_aincw_f(1));

	addr_code = u64_lo32((pmu->ucode.pmu_va +
			desc->app_start_offset +
			desc->app_resident_code_offset) >> 8) ;
	addr_data = u64_lo32((pmu->ucode.pmu_va +
			desc->app_start_offset +
			desc->app_resident_data_offset) >> 8);
	addr_load = u64_lo32((pmu->ucode.pmu_va +
			desc->bootloader_start_offset) >> 8);

	gk20a_writel(g, pwr_falcon_dmemd_r(0), GK20A_PMU_DMAIDX_UCODE);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_code_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_imem_entry);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_data);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), desc->app_resident_data_size);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_code);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), 0x1);
	gk20a_writel(g, pwr_falcon_dmemd_r(0), addr_args);

	gk20a_writel(g, pwr_falcon_dmatrfbase_r(),
		addr_load - (desc->bootloader_imem_offset >> 8));

	blocks = ((desc->bootloader_size + 0xFF) & ~0xFF) >> 8;

	for (i = 0; i < blocks; i++) {
		gk20a_writel(g, pwr_falcon_dmatrfmoffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrffboffs_r(),
			desc->bootloader_imem_offset + (i << 8));
		gk20a_writel(g, pwr_falcon_dmatrfcmd_r(),
			pwr_falcon_dmatrfcmd_imem_f(1)  |
			pwr_falcon_dmatrfcmd_write_f(0) |
			pwr_falcon_dmatrfcmd_size_f(6)  |
			pwr_falcon_dmatrfcmd_ctxdma_f(GK20A_PMU_DMAIDX_UCODE));
	}

	gk20a_writel(g, pwr_falcon_bootvec_r(),
		pwr_falcon_bootvec_vec_f(desc->bootloader_entry_point));

	gk20a_writel(g, pwr_falcon_cpuctl_r(),
		pwr_falcon_cpuctl_startcpu_f(1));

	gk20a_writel(g, pwr_falcon_os_r(), desc->app_version);

	return 0;
}

static void pmu_seq_init(struct pmu_gk20a *pmu)
{
	u32 i;

	memset(pmu->seq, 0,
		sizeof(struct pmu_sequence) * PMU_MAX_NUM_SEQUENCES);
	memset(pmu->pmu_seq_tbl, 0,
		sizeof(pmu->pmu_seq_tbl));

	for (i = 0; i < PMU_MAX_NUM_SEQUENCES; i++)
		pmu->seq[i].id = i;
}

static int pmu_seq_acquire(struct pmu_gk20a *pmu,
			struct pmu_sequence **pseq)
{
	struct gk20a *g = pmu->g;
	struct pmu_sequence *seq;
	u32 index;

	mutex_lock(&pmu->pmu_seq_lock);
	index = find_first_zero_bit(pmu->pmu_seq_tbl,
				sizeof(pmu->pmu_seq_tbl));
	if (index >= sizeof(pmu->pmu_seq_tbl)) {
		nvhost_err(dev_from_gk20a(g),
			"no free sequence available");
		mutex_unlock(&pmu->pmu_seq_lock);
		return -EAGAIN;
	}
	set_bit(index, pmu->pmu_seq_tbl);
	mutex_unlock(&pmu->pmu_seq_lock);

	seq = &pmu->seq[index];
	seq->state = PMU_SEQ_STATE_PENDING;

	*pseq = seq;
	return 0;
}

static void pmu_seq_release(struct pmu_gk20a *pmu,
			struct pmu_sequence *seq)
{
	seq->state	= PMU_SEQ_STATE_FREE;
	seq->desc	= PMU_INVALID_SEQ_DESC;
	seq->callback	= NULL;
	seq->cb_params	= NULL;
	seq->msg	= NULL;
	seq->out_payload = NULL;
	seq->in.alloc.dmem.size	= 0;
	seq->out.alloc.dmem.size = 0;

	clear_bit(seq->id, pmu->pmu_seq_tbl);
}

static int pmu_queue_init(struct pmu_queue *queue,
			u32 id, struct pmu_init_msg_pmu *init)
{
	queue->id	= id;
	queue->index	= init->queue_info[id].index;
	queue->offset	= init->queue_info[id].offset;
	queue->size	= init->queue_info[id].size;

	queue->mutex_id = id;
	mutex_init(&queue->mutex);

	nvhost_dbg_pmu("queue %d: index %d, offset 0x%08x, size 0x%08x",
		id, queue->index, queue->offset, queue->size);

	return 0;
}

static int pmu_queue_head(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			u32 *head, bool set)
{
	struct gk20a *g = pmu->g;

	BUG_ON(!head);

	if (PMU_IS_COMMAND_QUEUE(queue->id)) {

		if (queue->index >= pwr_pmu_queue_head__size_1_v())
			return -EINVAL;

		if (!set)
			*head = pwr_pmu_queue_head_address_v(
				gk20a_readl(g,
					pwr_pmu_queue_head_r(queue->index)));
		else
			gk20a_writel(g,
				pwr_pmu_queue_head_r(queue->index),
				pwr_pmu_queue_head_address_f(*head));
	} else {
		if (!set)
			*head = pwr_pmu_msgq_head_val_v(
				gk20a_readl(g, pwr_pmu_msgq_head_r()));
		else
			gk20a_writel(g,
				pwr_pmu_msgq_head_r(),
				pwr_pmu_msgq_head_val_f(*head));
	}

	return 0;
}

static int pmu_queue_tail(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			u32 *tail, bool set)
{
	struct gk20a *g = pmu->g;

	BUG_ON(!tail);

	if (PMU_IS_COMMAND_QUEUE(queue->id)) {

		if (queue->index >= pwr_pmu_queue_tail__size_1_v())
			return -EINVAL;

		if (!set)
			*tail = pwr_pmu_queue_tail_address_v(
				gk20a_readl(g,
					pwr_pmu_queue_tail_r(queue->index)));
		else
			gk20a_writel(g,
				pwr_pmu_queue_tail_r(queue->index),
				pwr_pmu_queue_tail_address_f(*tail));
	} else {
		if (!set)
			*tail = pwr_pmu_msgq_tail_val_v(
				gk20a_readl(g, pwr_pmu_msgq_tail_r()));
		else
			gk20a_writel(g,
				pwr_pmu_msgq_tail_r(),
				pwr_pmu_msgq_tail_val_f(*tail));
	}

	return 0;
}

static inline void pmu_queue_read(struct pmu_gk20a *pmu,
			u32 offset, u8 *dst, u32 size)
{
	pmu_copy_from_dmem(pmu, offset, dst, size, 0);
}

static inline void pmu_queue_write(struct pmu_gk20a *pmu,
			u32 offset, u8 *src, u32 size)
{
	pmu_copy_to_dmem(pmu, offset, src, size, 0);
}

int pmu_mutex_acquire(struct pmu_gk20a *pmu, u32 id, u32 *token)
{
	struct gk20a *g = pmu->g;
	struct pmu_mutex *mutex;
	u32 data, owner, max_retry;

	if (!pmu->initialized)
		return 0;

	BUG_ON(!token);
	BUG_ON(!PMU_MUTEX_ID_IS_VALID(id));
	BUG_ON(id > pmu->mutex_cnt);

	mutex = &pmu->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	if (*token != PMU_INVALID_MUTEX_OWNER_ID && *token == owner) {
		BUG_ON(mutex->ref_cnt == 0);
		nvhost_dbg_pmu("already acquired by owner : 0x%08x", *token);
		mutex->ref_cnt++;
		return 0;
	}

	max_retry = 40;
	do {
		data = pwr_pmu_mutex_id_value_v(
			gk20a_readl(g, pwr_pmu_mutex_id_r()));
		if (data == pwr_pmu_mutex_id_value_init_v() ||
		    data == pwr_pmu_mutex_id_value_not_avail_v()) {
			nvhost_warn(dev_from_gk20a(g),
				"fail to generate mutex token: val 0x%08x",
				owner);
			usleep_range(20, 40);
			continue;
		}

		owner = data;
		gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
			pwr_pmu_mutex_value_f(owner));

		data = pwr_pmu_mutex_value_v(
			gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

		if (owner == data) {
			mutex->ref_cnt = 1;
			nvhost_dbg_pmu("mutex acquired: id=%d, token=0x%x",
				mutex->index, *token);
			*token = owner;
			return 0;
		} else {
			nvhost_dbg_info("fail to acquire mutex idx=0x%08x",
				mutex->index);

			data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
			data = set_field(data,
				pwr_pmu_mutex_id_release_value_m(),
				pwr_pmu_mutex_id_release_value_f(owner));
			gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

			usleep_range(20, 40);
			continue;
		}
	} while (max_retry-- > 0);

	return -EBUSY;
}

int pmu_mutex_release(struct pmu_gk20a *pmu, u32 id, u32 *token)
{
	struct gk20a *g = pmu->g;
	struct pmu_mutex *mutex;
	u32 owner, data;

	if (!pmu->initialized)
		return 0;

	BUG_ON(!token);
	BUG_ON(!PMU_MUTEX_ID_IS_VALID(id));
	BUG_ON(id > pmu->mutex_cnt);

	mutex = &pmu->mutex[id];

	owner = pwr_pmu_mutex_value_v(
		gk20a_readl(g, pwr_pmu_mutex_r(mutex->index)));

	if (*token != owner) {
		nvhost_err(dev_from_gk20a(g),
			"requester 0x%08x NOT match owner 0x%08x",
			*token, owner);
		return -EINVAL;
	}

	if (--mutex->ref_cnt == 0) {
		gk20a_writel(g, pwr_pmu_mutex_r(mutex->index),
			pwr_pmu_mutex_value_initial_lock_f());

		data = gk20a_readl(g, pwr_pmu_mutex_id_release_r());
		data = set_field(data, pwr_pmu_mutex_id_release_value_m(),
			pwr_pmu_mutex_id_release_value_f(owner));
		gk20a_writel(g, pwr_pmu_mutex_id_release_r(), data);

		nvhost_dbg_pmu("mutex released: id=%d, token=0x%x",
			mutex->index, *token);
	}

	return 0;
}

static int pmu_queue_lock(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	if (PMU_IS_MESSAGE_QUEUE(queue->id))
		return 0;

	if (PMU_IS_SW_COMMAND_QUEUE(queue->id)) {
		mutex_lock(&queue->mutex);
		queue->locked = true;
		return 0;
	}

	err = pmu_mutex_acquire(pmu, queue->mutex_id,
			&queue->mutex_lock);
	if (err == 0)
		queue->locked = true;

	return err;
}

static int pmu_queue_unlock(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	if (PMU_IS_MESSAGE_QUEUE(queue->id))
		return 0;

	if (PMU_IS_SW_COMMAND_QUEUE(queue->id)) {
		mutex_unlock(&queue->mutex);
		queue->locked = false;
		return 0;
	}

	if (queue->locked) {
		err = pmu_mutex_release(pmu, queue->mutex_id,
				&queue->mutex_lock);
		if (err == 0)
			queue->locked = false;
	}

	return 0;
}

/* called by pmu_read_message, no lock */
static bool pmu_queue_is_empty(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	u32 head, tail;

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	if (queue->opened && queue->oflag == OFLAG_READ)
		tail = queue->position;
	else
		pmu_queue_tail(pmu, queue, &tail, QUEUE_GET);

	return head == tail;
}

static bool pmu_queue_has_room(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, u32 size, bool *need_rewind)
{
	u32 head, tail, free;
	bool rewind = false;

	BUG_ON(!queue->locked);

	size = ALIGN(size, QUEUE_ALIGNMENT);

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	pmu_queue_tail(pmu, queue, &tail, QUEUE_GET);

	if (head >= tail) {
		free = queue->offset + queue->size - head;
		free -= PMU_CMD_HDR_SIZE;

		if (size > free) {
			rewind = true;
			head = queue->offset;
		}
	}

	if (head < tail)
		free = tail - head - 1;

	if (need_rewind)
		*need_rewind = rewind;

	return size <= free;
}

static int pmu_queue_push(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, void *data, u32 size)
{
	nvhost_dbg_fn("");

	if (!queue->opened && queue->oflag == OFLAG_WRITE){
		nvhost_err(dev_from_gk20a(pmu->g),
			"queue not opened for write");
		return -EINVAL;
	}

	pmu_queue_write(pmu, queue->position, data, size);
	queue->position += ALIGN(size, QUEUE_ALIGNMENT);
	return 0;
}

static int pmu_queue_pop(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, void *data, u32 size,
			u32 *bytes_read)
{
	u32 head, tail, used;

	*bytes_read = 0;

	if (!queue->opened && queue->oflag == OFLAG_READ){
		nvhost_err(dev_from_gk20a(pmu->g),
			"queue not opened for read");
		return -EINVAL;
	}

	pmu_queue_head(pmu, queue, &head, QUEUE_GET);
	tail = queue->position;

	if (head == tail)
		return 0;

	if (head > tail)
		used = head - tail;
	else
		used = queue->offset + queue->size - tail;

	if (size > used) {
		nvhost_warn(dev_from_gk20a(pmu->g),
			"queue size smaller than request read");
		size = used;
	}

	pmu_queue_read(pmu, tail, data, size);
	queue->position += ALIGN(size, QUEUE_ALIGNMENT);
	*bytes_read = size;
	return 0;
}

static void pmu_queue_rewind(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	struct pmu_cmd cmd;

	nvhost_dbg_fn("");

	if (!queue->opened) {
		nvhost_err(dev_from_gk20a(pmu->g),
			"queue not opened");
		return;
	}

	if (queue->oflag == OFLAG_WRITE) {
		cmd.hdr.unit_id = PMU_UNIT_REWIND;
		cmd.hdr.size = PMU_CMD_HDR_SIZE;
		pmu_queue_push(pmu, queue, &cmd, cmd.hdr.size);
		nvhost_dbg_pmu("queue %d rewinded", queue->id);
	}

	queue->position = queue->offset;
	return;
}

/* open for read and lock the queue */
static int pmu_queue_open_read(struct pmu_gk20a *pmu,
			struct pmu_queue *queue)
{
	int err;

	err = pmu_queue_lock(pmu, queue);
	if (err)
		return err;

	if (queue->opened)
		BUG();

	pmu_queue_tail(pmu, queue, &queue->position, QUEUE_GET);
	queue->oflag = OFLAG_READ;
	queue->opened = true;

	return 0;
}

/* open for write and lock the queue
   make sure there's enough free space for the write */
static int pmu_queue_open_write(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, u32 size)
{
	bool rewind = false;
	int err;

	err = pmu_queue_lock(pmu, queue);
	if (err)
		return err;

	if (queue->opened)
		BUG();

	if (!pmu_queue_has_room(pmu, queue, size, &rewind)) {
		nvhost_err(dev_from_gk20a(pmu->g), "queue full");
		return -EAGAIN;
	}

	pmu_queue_head(pmu, queue, &queue->position, QUEUE_GET);
	queue->oflag = OFLAG_WRITE;
	queue->opened = true;

	if (rewind)
		pmu_queue_rewind(pmu, queue);

	return 0;
}

/* close and unlock the queue */
static int pmu_queue_close(struct pmu_gk20a *pmu,
			struct pmu_queue *queue, bool commit)
{
	if (!queue->opened)
		return 0;

	if (commit) {
		if (queue->oflag == OFLAG_READ) {
			pmu_queue_tail(pmu, queue,
				&queue->position, QUEUE_SET);
		}
		else {
			pmu_queue_head(pmu, queue,
				&queue->position, QUEUE_SET);
		}
	}

	queue->opened = false;

	pmu_queue_unlock(pmu, queue);

	return 0;
}

static void gk20a_save_pmu_sw_state(struct pmu_gk20a *pmu,
			struct gk20a_pmu_save_state *save)
{
	save->seq = pmu->seq;
	save->next_seq_desc = pmu->next_seq_desc;
	save->mutex = pmu->mutex;
	save->mutex_cnt = pmu->mutex_cnt;
	save->desc = pmu->desc;
	save->ucode = pmu->ucode;
	save->elpg_enable = pmu->elpg_enable;
	save->pg_wq = pmu->pg_wq;
	save->seq_buf = pmu->seq_buf;
	save->pg_buf = pmu->pg_buf;
	save->sw_ready = pmu->sw_ready;
}

static void gk20a_restore_pmu_sw_state(struct pmu_gk20a *pmu,
			struct gk20a_pmu_save_state *save)
{
	pmu->seq = save->seq;
	pmu->next_seq_desc = save->next_seq_desc;
	pmu->mutex = save->mutex;
	pmu->mutex_cnt = save->mutex_cnt;
	pmu->desc = save->desc;
	pmu->ucode = save->ucode;
	pmu->elpg_enable = save->elpg_enable;
	pmu->pg_wq = save->pg_wq;
	pmu->seq_buf = save->seq_buf;
	pmu->pg_buf = save->pg_buf;
	pmu->sw_ready = save->sw_ready;
}

void gk20a_remove_pmu_support(struct pmu_gk20a *pmu)
{
	struct gk20a_pmu_save_state save;

	nvhost_dbg_fn("");

	nvhost_allocator_destroy(&pmu->dmem);

	/* Save the stuff you don't want to lose */
	gk20a_save_pmu_sw_state(pmu, &save);

	/* this function is also called by pmu_destory outside gk20a deinit that
	   releases gk20a struct so fill up with zeros here. */
	memset(pmu, 0, sizeof(struct pmu_gk20a));

	/* Restore stuff you want to keep */
	gk20a_restore_pmu_sw_state(pmu, &save);
}

int gk20a_init_pmu_reset_enable_hw(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;

	nvhost_dbg_fn("");

	pmu_enable_hw(pmu, true);

	return 0;
}

static void pmu_elpg_enable_allow(struct work_struct *work);

int gk20a_init_pmu_setup_sw(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	int i, err = 0;
	u8 *ptr;
	void *ucode_ptr;
	struct sg_table *sgt_pmu_ucode;
	struct sg_table *sgt_seq_buf;
	DEFINE_DMA_ATTRS(attrs);

	nvhost_dbg_fn("");

	if (pmu->sw_ready) {
		for (i = 0; i < pmu->mutex_cnt; i++) {
			pmu->mutex[i].id    = i;
			pmu->mutex[i].index = i;
		}
		pmu_seq_init(pmu);

		nvhost_dbg_fn("skip init");
		goto skip_init;
	}

	/* no infoRom script from vbios? */

	/* TBD: sysmon subtask */

	pmu->mutex_cnt = pwr_pmu_mutex__size_1_v();
	pmu->mutex = kzalloc(pmu->mutex_cnt *
		sizeof(struct pmu_mutex), GFP_KERNEL);
	if (!pmu->mutex) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < pmu->mutex_cnt; i++) {
		pmu->mutex[i].id    = i;
		pmu->mutex[i].index = i;
	}

	pmu->seq = kzalloc(PMU_MAX_NUM_SEQUENCES *
		sizeof(struct pmu_sequence), GFP_KERNEL);
	if (!pmu->seq) {
		err = -ENOMEM;
		goto err_free_mutex;
	}

	pmu_seq_init(pmu);

	if (!g->pmu_fw) {
		g->pmu_fw = nvhost_client_request_firmware(g->dev,
					GK20A_PMU_UCODE_IMAGE);
		if (!g->pmu_fw) {
			nvhost_err(d, "failed to load pmu ucode!!");
			err = -ENOENT;
			goto err_free_seq;
		}
	}

	nvhost_dbg_fn("firmware loaded");

	pmu->desc = (struct pmu_ucode_desc *)g->pmu_fw->data;
	pmu->ucode_image = (u32 *)((u8 *)pmu->desc +
			pmu->desc->descriptor_size);


	INIT_DELAYED_WORK(&pmu->elpg_enable, pmu_elpg_enable_allow);

	gk20a_init_pmu_vm(mm);

	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);
	pmu->ucode.cpuva = dma_alloc_attrs(d, GK20A_PMU_UCODE_SIZE_MAX,
					&pmu->ucode.iova,
					GFP_KERNEL,
					&attrs);
	if (!pmu->ucode.cpuva) {
		nvhost_err(d, "failed to allocate memory\n");
		err = -ENOMEM;
		goto err_release_fw;
	}

	pmu->seq_buf.cpuva = dma_alloc_coherent(d, GK20A_PMU_SEQ_BUF_SIZE,
					&pmu->seq_buf.iova,
					GFP_KERNEL);
	if (!pmu->seq_buf.cpuva) {
		nvhost_err(d, "failed to allocate memory\n");
		err = -ENOMEM;
		goto err_free_pmu_ucode;
	}

	init_waitqueue_head(&pmu->pg_wq);

	err = gk20a_get_sgtable(d, &sgt_pmu_ucode,
				pmu->ucode.cpuva,
				pmu->ucode.iova,
				GK20A_PMU_UCODE_SIZE_MAX);
	if (err) {
		nvhost_err(d, "failed to allocate sg table\n");
		goto err_free_seq_buf;
	}

	pmu->ucode.pmu_va = gk20a_gmmu_map(vm, &sgt_pmu_ucode,
					GK20A_PMU_UCODE_SIZE_MAX,
					0, /* flags */
					mem_flag_read_only);
	if (!pmu->ucode.pmu_va) {
		nvhost_err(d, "failed to map pmu ucode memory!!");
		goto err_free_ucode_sgt;
	}

	err = gk20a_get_sgtable(d, &sgt_seq_buf,
				pmu->seq_buf.cpuva,
				pmu->seq_buf.iova,
				GK20A_PMU_SEQ_BUF_SIZE);
	if (err) {
		nvhost_err(d, "failed to allocate sg table\n");
		goto err_unmap_ucode;
	}

	pmu->seq_buf.pmu_va = gk20a_gmmu_map(vm, &sgt_seq_buf,
					GK20A_PMU_SEQ_BUF_SIZE,
					0, /* flags */
					mem_flag_none);
	if (!pmu->seq_buf.pmu_va) {
		nvhost_err(d, "failed to map pmu ucode memory!!");
		goto err_free_seq_buf_sgt;
	}

	ptr = (u8 *)pmu->seq_buf.cpuva;
	if (!ptr) {
		nvhost_err(d, "failed to map cpu ptr for zbc buffer");
		goto err_unmap_seq_buf;
	}

	/* TBD: remove this if ZBC save/restore is handled by PMU
	 * end an empty ZBC sequence for now */
	ptr[0] = 0x16; /* opcode EXIT */
	ptr[1] = 0; ptr[2] = 1; ptr[3] = 0;
	ptr[4] = 0; ptr[5] = 0; ptr[6] = 0; ptr[7] = 0;

	pmu->seq_buf.size = GK20A_PMU_SEQ_BUF_SIZE;

	ucode_ptr = pmu->ucode.cpuva;

	for (i = 0; i < (pmu->desc->app_start_offset +
			pmu->desc->app_size) >> 2; i++)
		mem_wr32(ucode_ptr, i, pmu->ucode_image[i]);

	gk20a_free_sgtable(&sgt_pmu_ucode);
	gk20a_free_sgtable(&sgt_seq_buf);

skip_init:
	mutex_init(&pmu->elpg_mutex);
	mutex_init(&pmu->isr_mutex);
	mutex_init(&pmu->pmu_copy_lock);
	mutex_init(&pmu->pmu_seq_lock);
	mutex_init(&pmu->pg_init_mutex);

	pmu->perfmon_counter.index = 3; /* GR & CE2 */
	pmu->perfmon_counter.group_id = PMU_DOMAIN_GROUP_PSTATE;

	pmu->remove_support = gk20a_remove_pmu_support;

	nvhost_dbg_fn("done");
	return 0;

 err_unmap_seq_buf:
	gk20a_gmmu_unmap(vm, pmu->seq_buf.pmu_va,
		GK20A_PMU_SEQ_BUF_SIZE, mem_flag_none);
 err_free_seq_buf_sgt:
	gk20a_free_sgtable(&sgt_seq_buf);
 err_unmap_ucode:
	gk20a_gmmu_unmap(vm, pmu->ucode.pmu_va,
		GK20A_PMU_UCODE_SIZE_MAX, mem_flag_none);
 err_free_ucode_sgt:
	gk20a_free_sgtable(&sgt_pmu_ucode);
 err_free_seq_buf:
	dma_free_coherent(d, GK20A_PMU_SEQ_BUF_SIZE,
		pmu->seq_buf.cpuva, pmu->seq_buf.iova);
	pmu->seq_buf.cpuva = NULL;
	pmu->seq_buf.iova = 0;
 err_free_pmu_ucode:
	dma_free_attrs(d, GK20A_PMU_UCODE_SIZE_MAX,
		pmu->ucode.cpuva, pmu->ucode.iova, &attrs);
	pmu->ucode.cpuva = NULL;
	pmu->ucode.iova = 0;
 err_release_fw:
	release_firmware(g->pmu_fw);
 err_free_seq:
	kfree(pmu->seq);
 err_free_mutex:
	kfree(pmu->mutex);
 err:
	nvhost_dbg_fn("fail");
	return err;
}

static void pmu_handle_pg_elpg_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status);

static void pmu_handle_pg_buf_config_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	struct pmu_pg_msg_eng_buf_stat *eng_buf_stat = &msg->msg.pg.eng_buf_stat;

	nvhost_dbg_fn("");

	if (status != 0) {
		nvhost_err(dev_from_gk20a(g), "PGENG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	if (eng_buf_stat->status == PMU_PG_MSG_ENG_BUF_FAILED) {
		nvhost_err(dev_from_gk20a(g), "failed to load PGENG buffer");
	}

	pmu->buf_loaded = (eng_buf_stat->status == PMU_PG_MSG_ENG_BUF_LOADED);
	wake_up(&pmu->pg_wq);
}

int gk20a_init_pmu_setup_hw1(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err;

	nvhost_dbg_fn("");

	pmu_reset(pmu);

	/* setup apertures - virtual */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_UCODE),
		pwr_fbif_transcfg_mem_type_virtual_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_VIRT),
		pwr_fbif_transcfg_mem_type_virtual_f());
	/* setup apertures - physical */
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_VID),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_local_fb_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_COH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_coherent_sysmem_f());
	gk20a_writel(g, pwr_fbif_transcfg_r(GK20A_PMU_DMAIDX_PHYS_SYS_NCOH),
		pwr_fbif_transcfg_mem_type_physical_f() |
		pwr_fbif_transcfg_target_noncoherent_sysmem_f());

	/* TBD: load pmu ucode */
	err = pmu_bootstrap(pmu);
	if (err)
		return err;

	return 0;

}

static int gk20a_aelpg_init(struct gk20a *g);
static int gk20a_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id);

int gk20a_init_pmu_setup_hw2(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	struct pmu_cmd cmd;
	u32 desc;
	long remain;
	int err;
	bool status;
	u32 size;
	struct sg_table *sgt_pg_buf;

	nvhost_dbg_fn("");

	if (!support_gk20a_pmu())
		return 0;

	size = 0;
	err = gr_gk20a_fecs_get_reglist_img_size(g, &size);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to query fecs pg buffer size");
		return err;
	}

	if (!pmu->sw_ready) {
		pmu->pg_buf.cpuva = dma_alloc_coherent(d, size,
						&pmu->pg_buf.iova,
						GFP_KERNEL);
		if (!pmu->pg_buf.cpuva) {
			nvhost_err(d, "failed to allocate memory\n");
			err = -ENOMEM;
			goto err;
		}

		pmu->pg_buf.size = size;

		err = gk20a_get_sgtable(d, &sgt_pg_buf,
					pmu->pg_buf.cpuva,
					pmu->pg_buf.iova,
					size);
		if (err) {
			nvhost_err(d, "failed to create sg table\n");
			goto err_free_pg_buf;
		}

		pmu->pg_buf.pmu_va = gk20a_gmmu_map(vm,
					&sgt_pg_buf,
					size,
					0, /* flags */
					mem_flag_none);
		if (!pmu->pg_buf.pmu_va) {
			nvhost_err(d, "failed to map fecs pg buffer");
			err = -ENOMEM;
			goto err_free_sgtable;
		}

		gk20a_free_sgtable(&sgt_pg_buf);
	}

	/*
	 * This is the actual point at which sw setup is complete, so set the
	 * sw_ready flag here.
	 */
	pmu->sw_ready = true;

	/* TBD: acquire pmu hw mutex */

	/* TBD: post reset again? */

	/* PMU_INIT message handler will send PG_INIT */
	remain = wait_event_timeout(
			pmu->pg_wq,
			(status = (pmu->elpg_ready &&
				pmu->stat_dmem_offset != 0 &&
				pmu->elpg_stat == PMU_ELPG_STAT_OFF)),
			msecs_to_jiffies(gk20a_get_gr_idle_timeout(g)));
	if (status == 0) {
		nvhost_err(dev_from_gk20a(g),
			"PG_INIT_ACK failed, remaining timeout : 0x%lx", remain);
		pmu_dump_falcon_stats(pmu);
		return -EBUSY;
	}

	err = gr_gk20a_fecs_set_reglist_bind_inst(g, mm->pmu.inst_block.cpu_pa);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to bind pmu inst to gr");
		return err;
	}

	err = gr_gk20a_fecs_set_reglist_virual_addr(g, pmu->pg_buf.pmu_va);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to set pg buffer pmu va");
		return err;
	}

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_eng_buf_load);
	cmd.cmd.pg.eng_buf_load.cmd_type = PMU_PG_CMD_ID_ENG_BUF_LOAD;
	cmd.cmd.pg.eng_buf_load.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.eng_buf_load.buf_idx = PMU_PGENG_GR_BUFFER_IDX_FECS;
	cmd.cmd.pg.eng_buf_load.buf_size = pmu->pg_buf.size;
	cmd.cmd.pg.eng_buf_load.dma_base = u64_lo32(pmu->pg_buf.pmu_va >> 8);
	cmd.cmd.pg.eng_buf_load.dma_offset = (u8)(pmu->pg_buf.pmu_va & 0xFF);
	cmd.cmd.pg.eng_buf_load.dma_idx = PMU_DMAIDX_VIRT;

	pmu->buf_loaded = false;
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu, &desc, ~0);

	remain = wait_event_timeout(
			pmu->pg_wq,
			pmu->buf_loaded,
			msecs_to_jiffies(gk20a_get_gr_idle_timeout(g)));
	if (!pmu->buf_loaded) {
		nvhost_err(dev_from_gk20a(g),
			"PGENG FECS buffer load failed, remaining timeout : 0x%lx",
			remain);
		return -EBUSY;
	}

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_eng_buf_load);
	cmd.cmd.pg.eng_buf_load.cmd_type = PMU_PG_CMD_ID_ENG_BUF_LOAD;
	cmd.cmd.pg.eng_buf_load.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.eng_buf_load.buf_idx = PMU_PGENG_GR_BUFFER_IDX_ZBC;
	cmd.cmd.pg.eng_buf_load.buf_size = pmu->seq_buf.size;
	cmd.cmd.pg.eng_buf_load.dma_base = u64_lo32(pmu->seq_buf.pmu_va >> 8);
	cmd.cmd.pg.eng_buf_load.dma_offset = (u8)(pmu->seq_buf.pmu_va & 0xFF);
	cmd.cmd.pg.eng_buf_load.dma_idx = PMU_DMAIDX_VIRT;

	pmu->buf_loaded = false;
	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu, &desc, ~0);

	remain = wait_event_timeout(
			pmu->pg_wq,
			pmu->buf_loaded,
			msecs_to_jiffies(gk20a_get_gr_idle_timeout(g)));
	if (!pmu->buf_loaded) {
		nvhost_err(dev_from_gk20a(g),
			"PGENG ZBC buffer load failed, remaining timeout 0x%lx",
			remain);
		return -EBUSY;
	}

	/*
	 * FIXME: To enable ELPG, we increase the PMU ext2priv timeout unit to
	 * 7. This prevents PMU stalling on Host register accesses. Once the
	 * cause for this hang is discovered and fixed, this WAR should be
	 * removed.
	 */
	gk20a_writel(g, 0x10a164, 0x109ff);

	mutex_lock(&pmu->pg_init_mutex);
	pmu->initialized = true;

	/*
	 * We can't guarantee that gr code to enable ELPG will be
	 * invoked, so we explicitly call disable-enable here
	 * to enable elpg.
	 */
	gk20a_pmu_disable_elpg(g);

	pmu->zbc_ready = true;
	/* Save zbc table after PMU is initialized. */
	pmu_save_zbc(g, 0xf);

	if (g->elpg_enabled)
		gk20a_pmu_enable_elpg(g);
	mutex_unlock(&pmu->pg_init_mutex);

	udelay(50);

	/* Enable AELPG */
	if (g->aelpg_enabled) {
		gk20a_aelpg_init(g);
		gk20a_aelpg_init_and_enable(g, PMU_AP_CTRL_ID_GRAPHICS);
	}

	return 0;

 err_free_sgtable:
	gk20a_free_sgtable(&sgt_pg_buf);
 err_free_pg_buf:
	dma_free_coherent(d, size,
		pmu->pg_buf.cpuva, pmu->pg_buf.iova);
	pmu->pg_buf.cpuva = NULL;
	pmu->pg_buf.iova = 0;
 err:
	return err;
}

int gk20a_init_pmu_support(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32 err;

	nvhost_dbg_fn("");

	if (pmu->initialized)
		return 0;

	pmu->g = g;

	err = gk20a_init_pmu_reset_enable_hw(g);
	if (err)
		return err;

	if (support_gk20a_pmu()) {
		err = gk20a_init_pmu_setup_sw(g);
		if (err)
			return err;

		err = gk20a_init_pmu_setup_hw1(g);
		if (err)
			return err;
	}

	return err;
}

static void pmu_handle_pg_elpg_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	struct pmu_pg_msg_elpg_msg *elpg_msg = &msg->msg.pg.elpg_msg;

	nvhost_dbg_fn("");

	if (status != 0) {
		nvhost_err(dev_from_gk20a(g), "ELPG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	switch (elpg_msg->msg) {
	case PMU_PG_ELPG_MSG_INIT_ACK:
		nvhost_dbg_pmu("INIT_PG is acknowledged from PMU");
		pmu->elpg_ready = true;
		wake_up(&pmu->pg_wq);
		break;
	case PMU_PG_ELPG_MSG_ALLOW_ACK:
		nvhost_dbg_pmu("ALLOW is acknowledged from PMU");
		pmu->elpg_stat = PMU_ELPG_STAT_ON;
		wake_up(&pmu->pg_wq);
		break;
	case PMU_PG_ELPG_MSG_DISALLOW_ACK:
		nvhost_dbg_pmu("DISALLOW is acknowledged from PMU");
		pmu->elpg_stat = PMU_ELPG_STAT_OFF;
		wake_up(&pmu->pg_wq);
		break;
	default:
		nvhost_err(dev_from_gk20a(g),
			"unsupported ELPG message : 0x%04x", elpg_msg->msg);
	}

	return;
}

static void pmu_handle_pg_stat_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;

	nvhost_dbg_fn("");

	if (status != 0) {
		nvhost_err(dev_from_gk20a(g), "ELPG cmd aborted");
		/* TBD: disable ELPG */
		return;
	}

	switch (msg->msg.pg.stat.sub_msg_id) {
	case PMU_PG_STAT_MSG_RESP_DMEM_OFFSET:
		nvhost_dbg_pmu("ALLOC_DMEM_OFFSET is acknowledged from PMU");
		pmu->stat_dmem_offset = msg->msg.pg.stat.data;
		wake_up(&pmu->pg_wq);
		break;
	default:
		break;
	}
}

static int pmu_init_powergating(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_cmd cmd;
	u32 seq;

	nvhost_dbg_fn("");

	if (tegra_cpu_is_asim()) {
		/* TBD: calculate threshold for silicon */
		gk20a_writel(g, pwr_pmu_pg_idlefilth_r(ENGINE_GR_GK20A),
				PMU_PG_IDLE_THRESHOLD_SIM);
		gk20a_writel(g, pwr_pmu_pg_ppuidlefilth_r(ENGINE_GR_GK20A),
				PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM);
	} else {
		/* TBD: calculate threshold for silicon */
		gk20a_writel(g, pwr_pmu_pg_idlefilth_r(ENGINE_GR_GK20A),
				PMU_PG_IDLE_THRESHOLD);
		gk20a_writel(g, pwr_pmu_pg_ppuidlefilth_r(ENGINE_GR_GK20A),
				PMU_PG_POST_POWERUP_IDLE_THRESHOLD);
	}

	/* init ELPG */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_INIT;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	/* alloc dmem for powergating state log */
	pmu->stat_dmem_offset = 0;
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_stat);
	cmd.cmd.pg.stat.cmd_type = PMU_PG_CMD_ID_PG_STAT;
	cmd.cmd.pg.stat.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.stat.sub_cmd_id = PMU_PG_STAT_CMD_ALLOC_DMEM;
	cmd.cmd.pg.stat.data = 0;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_stat_msg, pmu, &seq, ~0);

	/* disallow ELPG initially
	   PMU ucode requires a disallow cmd before allow cmd */
	pmu->elpg_stat = PMU_ELPG_STAT_ON; /* set for wait_event PMU_ELPG_STAT_OFF */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_DISALLOW;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	/* start with elpg disabled until first enable call */
	pmu->elpg_refcnt = 1;

	return 0;
}

static int pmu_init_perfmon(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_cmd cmd;
	struct pmu_payload payload;
	u32 seq;
	u32 data;
	int err;

	nvhost_dbg_fn("");

	pmu->perfmon_ready = 0;

	/* use counter #3 for GR && CE2 busy cycles */
	gk20a_writel(g, pwr_pmu_idle_mask_r(3),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	/* disable idle filtering for counters 3 and 6 */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(3));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(3), data);

	/* use counter #6 for total cycles */
	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(6));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(6), data);

	/*
	 * We don't want to disturb counters #3 and #6, which are used by
	 * perfmon, so we add wiring also to counters #1 and #2 for
	 * exposing raw counter readings.
	 */
	gk20a_writel(g, pwr_pmu_idle_mask_r(1),
		pwr_pmu_idle_mask_gr_enabled_f() |
		pwr_pmu_idle_mask_ce_2_enabled_f());

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(1));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_busy_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(1), data);

	data = gk20a_readl(g, pwr_pmu_idle_ctrl_r(2));
	data = set_field(data, pwr_pmu_idle_ctrl_value_m() |
			pwr_pmu_idle_ctrl_filter_m(),
			pwr_pmu_idle_ctrl_value_always_f() |
			pwr_pmu_idle_ctrl_filter_disabled_f());
	gk20a_writel(g, pwr_pmu_idle_ctrl_r(2), data);

	pmu->sample_buffer = 0;
	err = pmu->dmem.alloc(&pmu->dmem, &pmu->sample_buffer, 2 * sizeof(u16));
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"failed to allocate perfmon sample buffer");
		return -ENOMEM;
	}

	/* init PERFMON */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PERFMON;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_perfmon_cmd_init);
	cmd.cmd.perfmon.cmd_type = PMU_PERFMON_CMD_ID_INIT;
	/* buffer to save counter values for pmu perfmon */
	cmd.cmd.perfmon.init.sample_buffer = (u16)pmu->sample_buffer;
	/* number of sample periods below lower threshold
	   before pmu triggers perfmon decrease event
	   TBD: = 15 */
	cmd.cmd.perfmon.init.to_decrease_count = 15;
	/* index of base counter, aka. always ticking counter */
	cmd.cmd.perfmon.init.base_counter_id = 6;
	/* microseconds interval between pmu polls perf counters */
	cmd.cmd.perfmon.init.sample_period_us = 16700;
	/* number of perfmon counters
	   counter #3 (GR and CE2) for gk20a */
	cmd.cmd.perfmon.init.num_counters = 1;
	/* moving average window for sample periods
	   TBD: = 3000000 / sample_period_us = 17 */
	cmd.cmd.perfmon.init.samples_in_moving_avg = 17;

	memset(&payload, 0, sizeof(struct pmu_payload));
	payload.in.buf = &pmu->perfmon_counter;
	payload.in.size = sizeof(struct pmu_perfmon_counter);
	payload.in.offset =
		offsetof(struct pmu_perfmon_cmd_init, counter_alloc);

	gk20a_pmu_cmd_post(g, &cmd, NULL, &payload, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);

	return 0;
}

static int pmu_process_init_msg(struct pmu_gk20a *pmu,
			struct pmu_msg *msg)
{
	struct gk20a *g = pmu->g;
	struct pmu_init_msg_pmu *init;
	struct pmu_sha1_gid_data gid_data;
	u32 i, tail = 0;

	tail = pwr_pmu_msgq_tail_val_v(
		gk20a_readl(g, pwr_pmu_msgq_tail_r()));

	pmu_copy_from_dmem(pmu, tail,
		(u8 *)&msg->hdr, PMU_MSG_HDR_SIZE, 0);

	if (msg->hdr.unit_id != PMU_UNIT_INIT) {
		nvhost_err(dev_from_gk20a(g),
			"expecting init msg");
		return -EINVAL;
	}

	pmu_copy_from_dmem(pmu, tail + PMU_MSG_HDR_SIZE,
		(u8 *)&msg->msg, msg->hdr.size - PMU_MSG_HDR_SIZE, 0);

	if (msg->msg.init.msg_type != PMU_INIT_MSG_TYPE_PMU_INIT) {
		nvhost_err(dev_from_gk20a(g),
			"expecting init msg");
		return -EINVAL;
	}

	tail += ALIGN(msg->hdr.size, PMU_DMEM_ALIGNMENT);
	gk20a_writel(g, pwr_pmu_msgq_tail_r(),
		pwr_pmu_msgq_tail_val_f(tail));

	if (!pmu->gid_info.valid) {

		pmu_copy_from_dmem(pmu,
			msg->msg.init.pmu_init.sw_managed_area_offset,
			(u8 *)&gid_data,
			sizeof(struct pmu_sha1_gid_data), 0);

		pmu->gid_info.valid =
			(*(u32 *)gid_data.signature == PMU_SHA1_GID_SIGNATURE);

		if (pmu->gid_info.valid) {

			BUG_ON(sizeof(pmu->gid_info.gid) !=
				sizeof(gid_data.gid));

			memcpy(pmu->gid_info.gid, gid_data.gid,
				sizeof(pmu->gid_info.gid));
		}
	}

	init = &msg->msg.init.pmu_init;
	for (i = 0; i < PMU_QUEUE_COUNT; i++)
		pmu_queue_init(&pmu->queue[i], i, init);

	nvhost_allocator_init(&pmu->dmem, "gk20a_pmu_dmem",
			msg->msg.init.pmu_init.sw_managed_area_offset,
			msg->msg.init.pmu_init.sw_managed_area_size,
			PMU_DMEM_ALLOC_ALIGNMENT);

	pmu->pmu_ready = true;

	return 0;
}

static bool pmu_read_message(struct pmu_gk20a *pmu, struct pmu_queue *queue,
			struct pmu_msg *msg, int *status)
{
	struct gk20a *g = pmu->g;
	u32 read_size, bytes_read;
	int err;

	*status = 0;

	if (pmu_queue_is_empty(pmu, queue))
		return false;

	err = pmu_queue_open_read(pmu, queue);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to open queue %d for read", queue->id);
		*status = err;
		return false;
	}

	err = pmu_queue_pop(pmu, queue, &msg->hdr,
			PMU_MSG_HDR_SIZE, &bytes_read);
	if (err || bytes_read != PMU_MSG_HDR_SIZE) {
		nvhost_err(dev_from_gk20a(g),
			"fail to read msg from queue %d", queue->id);
		*status = err | -EINVAL;
		goto clean_up;
	}

	if (msg->hdr.unit_id == PMU_UNIT_REWIND) {
		pmu_queue_rewind(pmu, queue);
		/* read again after rewind */
		err = pmu_queue_pop(pmu, queue, &msg->hdr,
				PMU_MSG_HDR_SIZE, &bytes_read);
		if (err || bytes_read != PMU_MSG_HDR_SIZE) {
			nvhost_err(dev_from_gk20a(g),
				"fail to read msg from queue %d", queue->id);
			*status = err | -EINVAL;
			goto clean_up;
		}
	}

	if (!PMU_UNIT_ID_IS_VALID(msg->hdr.unit_id)) {
		nvhost_err(dev_from_gk20a(g),
			"read invalid unit_id %d from queue %d",
			msg->hdr.unit_id, queue->id);
			*status = -EINVAL;
			goto clean_up;
	}

	if (msg->hdr.size > PMU_MSG_HDR_SIZE) {
		read_size = msg->hdr.size - PMU_MSG_HDR_SIZE;
		err = pmu_queue_pop(pmu, queue, &msg->msg,
			read_size, &bytes_read);
		if (err || bytes_read != read_size) {
			nvhost_err(dev_from_gk20a(g),
				"fail to read msg from queue %d", queue->id);
			*status = err;
			goto clean_up;
		}
	}

	err = pmu_queue_close(pmu, queue, true);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to close queue %d", queue->id);
		*status = err;
		return false;
	}

	return true;

clean_up:
	err = pmu_queue_close(pmu, queue, false);
	if (err)
		nvhost_err(dev_from_gk20a(g),
			"fail to close queue %d", queue->id);
	return false;
}

static int pmu_response_handle(struct pmu_gk20a *pmu,
			struct pmu_msg *msg)
{
	struct gk20a *g = pmu->g;
	struct pmu_sequence *seq;
	int ret = 0;

	nvhost_dbg_fn("");

	seq = &pmu->seq[msg->hdr.seq_id];
	if (seq->state != PMU_SEQ_STATE_USED &&
	    seq->state != PMU_SEQ_STATE_CANCELLED) {
		nvhost_err(dev_from_gk20a(g),
			"msg for an unknown sequence %d", seq->id);
		return -EINVAL;
	}

	if (msg->hdr.unit_id == PMU_UNIT_RC &&
	    msg->msg.rc.msg_type == PMU_RC_MSG_TYPE_UNHANDLED_CMD) {
		nvhost_err(dev_from_gk20a(g),
			"unhandled cmd: seq %d", seq->id);
	}
	else if (seq->state != PMU_SEQ_STATE_CANCELLED) {
		if (seq->msg) {
			if (seq->msg->hdr.size >= msg->hdr.size) {
				memcpy(seq->msg, msg, msg->hdr.size);
				if (seq->out.alloc.dmem.size != 0) {
					pmu_copy_from_dmem(pmu,
						seq->out.alloc.dmem.offset,
						seq->out_payload,
						seq->out.alloc.dmem.size,
						0);
				}
			} else {
				nvhost_err(dev_from_gk20a(g),
					"sequence %d msg buffer too small",
					seq->id);
			}
		}
	} else
		seq->callback = NULL;

	if (seq->in.alloc.dmem.size != 0)
		pmu->dmem.free(&pmu->dmem, seq->in.alloc.dmem.offset,
			seq->in.alloc.dmem.size);
	if (seq->out.alloc.dmem.size != 0)
		pmu->dmem.free(&pmu->dmem, seq->out.alloc.dmem.offset,
			seq->out.alloc.dmem.size);

	if (seq->callback)
		seq->callback(g, msg, seq->cb_params, seq->desc, ret);

	pmu_seq_release(pmu, seq);

	/* TBD: notify client waiting for available dmem */

	nvhost_dbg_fn("done");

	return 0;
}

static int pmu_wait_message_cond(struct pmu_gk20a *pmu, u32 timeout,
				 u32 *var, u32 val);

static void pmu_handle_zbc_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 handle, u32 status)
{
	struct pmu_gk20a *pmu = param;
	pmu->zbc_save_done = 1;
}

void pmu_save_zbc(struct gk20a *g, u32 entries)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;

	if (!pmu->pmu_ready || !entries || !pmu->zbc_ready)
		return;

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_zbc_cmd);
	cmd.cmd.zbc.cmd_type = PMU_PG_CMD_ID_ZBC_TABLE_UPDATE;
	cmd.cmd.zbc.entry_mask = ZBC_MASK(entries);

	pmu->zbc_save_done = 0;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			   pmu_handle_zbc_msg, pmu, &seq, ~0);
	pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
			      &pmu->zbc_save_done, 1);
	if (!pmu->zbc_save_done)
		nvhost_err(dev_from_gk20a(g), "ZBC save timeout");
}

static int pmu_perfmon_start_sampling(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_cmd cmd;
	struct pmu_payload payload;
	u32 current_rate = 0;
	u32 seq;

	/* PERFMON Start */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PERFMON;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_perfmon_cmd_start);
	cmd.cmd.perfmon.start.cmd_type = PMU_PERFMON_CMD_ID_START;
	cmd.cmd.perfmon.start.group_id = PMU_DOMAIN_GROUP_PSTATE;
	cmd.cmd.perfmon.start.state_id = pmu->perfmon_state_id[PMU_DOMAIN_GROUP_PSTATE];

	current_rate = rate_gpu_to_gpc2clk(gk20a_clk_get_rate(g));
	if (current_rate >= gpc_pll_params.max_freq)
		cmd.cmd.perfmon.start.flags =
			PMU_PERFMON_FLAG_ENABLE_DECREASE;
	else if (current_rate <= gpc_pll_params.min_freq)
		cmd.cmd.perfmon.start.flags =
			PMU_PERFMON_FLAG_ENABLE_INCREASE;
	else
		cmd.cmd.perfmon.start.flags =
			PMU_PERFMON_FLAG_ENABLE_INCREASE |
			PMU_PERFMON_FLAG_ENABLE_DECREASE;

	cmd.cmd.perfmon.start.flags |= PMU_PERFMON_FLAG_CLEAR_PREV;

	memset(&payload, 0, sizeof(struct pmu_payload));

	/* TBD: PMU_PERFMON_PCT_TO_INC * 100 */
	pmu->perfmon_counter.upper_threshold = 3000; /* 30% */
	/* TBD: PMU_PERFMON_PCT_TO_DEC * 100 */
	pmu->perfmon_counter.lower_threshold = 1000; /* 10% */
	pmu->perfmon_counter.valid = true;

	payload.in.buf = &pmu->perfmon_counter;
	payload.in.size = sizeof(pmu->perfmon_counter);
	payload.in.offset =
		offsetof(struct pmu_perfmon_cmd_start, counter_alloc);

	gk20a_pmu_cmd_post(g, &cmd, NULL, &payload, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);

	return 0;
}

static int pmu_perfmon_stop_sampling(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_cmd cmd;
	u32 seq;

	/* PERFMON Stop */
	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PERFMON;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_perfmon_cmd_stop);
	cmd.cmd.perfmon.stop.cmd_type = PMU_PERFMON_CMD_ID_STOP;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_LPQ,
			NULL, NULL, &seq, ~0);
	return 0;
}

static int pmu_handle_perfmon_event(struct pmu_gk20a *pmu,
			struct pmu_perfmon_msg *msg)
{
	struct gk20a *g = pmu->g;
	u32 rate;

	nvhost_dbg_fn("");

	switch (msg->msg_type) {
	case PMU_PERFMON_MSG_ID_INCREASE_EVENT:
		nvhost_dbg_pmu("perfmon increase event: "
			"state_id %d, ground_id %d, pct %d",
			msg->gen.state_id, msg->gen.group_id, msg->gen.data);
		/* increase gk20a clock freq by 20% */
		rate = gk20a_clk_get_rate(g);
		gk20a_clk_set_rate(g, rate * 6 / 5);
		break;
	case PMU_PERFMON_MSG_ID_DECREASE_EVENT:
		nvhost_dbg_pmu("perfmon decrease event: "
			"state_id %d, ground_id %d, pct %d",
			msg->gen.state_id, msg->gen.group_id, msg->gen.data);
		/* decrease gk20a clock freq by 10% */
		rate = gk20a_clk_get_rate(g);
		gk20a_clk_set_rate(g, (rate / 10) * 7);
		break;
	case PMU_PERFMON_MSG_ID_INIT_EVENT:
		pmu->perfmon_ready = 1;
		nvhost_dbg_pmu("perfmon init event");
		break;
	default:
		break;
	}

	/* restart sampling */
	if (IS_ENABLED(CONFIG_TEGRA_GK20A_PERFMON))
		return pmu_perfmon_start_sampling(pmu);
	return 0;
}


static int pmu_handle_event(struct pmu_gk20a *pmu, struct pmu_msg *msg)
{
	int err;

	nvhost_dbg_fn("");

	switch (msg->hdr.unit_id) {
	case PMU_UNIT_PERFMON:
		err = pmu_handle_perfmon_event(pmu, &msg->msg.perfmon);
		break;
	default:
		break;
	}

	return err;
}

static int pmu_process_message(struct pmu_gk20a *pmu)
{
	struct pmu_msg msg;
	int status;

	if (unlikely(!pmu->pmu_ready)) {
		pmu_process_init_msg(pmu, &msg);
		pmu_init_powergating(pmu);
		pmu_init_perfmon(pmu);
		return 0;
	}

	while (pmu_read_message(pmu,
		&pmu->queue[PMU_MESSAGE_QUEUE], &msg, &status)) {

		nvhost_dbg_pmu("read msg hdr: "
				"unit_id = 0x%08x, size = 0x%08x, "
				"ctrl_flags = 0x%08x, seq_id = 0x%08x",
				msg.hdr.unit_id, msg.hdr.size,
				msg.hdr.ctrl_flags, msg.hdr.seq_id);

		msg.hdr.ctrl_flags &= ~PMU_CMD_FLAGS_PMU_MASK;

		if (msg.hdr.ctrl_flags == PMU_CMD_FLAGS_EVENT) {
			pmu_handle_event(pmu, &msg);
		} else {
			pmu_response_handle(pmu, &msg);
		}
	}

	return 0;
}

static int pmu_wait_message_cond(struct pmu_gk20a *pmu, u32 timeout,
				 u32 *var, u32 val)
{
	struct gk20a *g = pmu->g;
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);
	unsigned long delay = GR_IDLE_CHECK_DEFAULT;

	do {
		if (*var == val)
			return 0;

		if (gk20a_readl(g, pwr_falcon_irqstat_r()))
			gk20a_pmu_isr(g);

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	} while (time_before(jiffies, end_jiffies) |
			!tegra_platform_is_silicon());

	return -ETIMEDOUT;
}

static void pmu_dump_elpg_stats(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	struct pmu_pg_stats stats;

	pmu_copy_from_dmem(pmu, pmu->stat_dmem_offset,
		(u8 *)&stats, sizeof(struct pmu_pg_stats), 0);

	nvhost_dbg_pmu("pg_entry_start_timestamp : 0x%016llx",
		stats.pg_entry_start_timestamp);
	nvhost_dbg_pmu("pg_exit_start_timestamp : 0x%016llx",
		stats.pg_exit_start_timestamp);
	nvhost_dbg_pmu("pg_ingating_start_timestamp : 0x%016llx",
		stats.pg_ingating_start_timestamp);
	nvhost_dbg_pmu("pg_ungating_start_timestamp : 0x%016llx",
		stats.pg_ungating_start_timestamp);
	nvhost_dbg_pmu("pg_avg_entry_time_us : 0x%08x",
		stats.pg_avg_entry_time_us);
	nvhost_dbg_pmu("pg_avg_exit_time_us : 0x%08x",
		stats.pg_avg_exit_time_us);
	nvhost_dbg_pmu("pg_ingating_cnt : 0x%08x",
		stats.pg_ingating_cnt);
	nvhost_dbg_pmu("pg_ingating_time_us : 0x%08x",
		stats.pg_ingating_time_us);
	nvhost_dbg_pmu("pg_ungating_count : 0x%08x",
		stats.pg_ungating_count);
	nvhost_dbg_pmu("pg_ungating_time_us 0x%08x: ",
		stats.pg_ungating_time_us);
	nvhost_dbg_pmu("pg_gating_cnt : 0x%08x",
		stats.pg_gating_cnt);
	nvhost_dbg_pmu("pg_gating_deny_cnt : 0x%08x",
		stats.pg_gating_deny_cnt);

	/*
	   Turn on PG_DEBUG in ucode and locate symbol "ElpgLog" offset
	   in .nm file, e.g. 0x1000066c. use 0x66c.
	u32 i, val[20];
	pmu_copy_from_dmem(pmu, 0x66c,
		(u8 *)val, sizeof(val), 0);
	nvhost_dbg_pmu("elpg log begin");
	for (i = 0; i < 20; i++)
		nvhost_dbg_pmu("0x%08x", val[i]);
	nvhost_dbg_pmu("elpg log end");
	*/

	nvhost_dbg_pmu("pwr_pmu_idle_mask_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_supp_r(3)));
	nvhost_dbg_pmu("pwr_pmu_idle_mask_1_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_mask_1_supp_r(3)));
	nvhost_dbg_pmu("pwr_pmu_idle_ctrl_supp_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_ctrl_supp_r(3)));
	nvhost_dbg_pmu("pwr_pmu_pg_idle_cnt_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_idle_cnt_r(0)));
	nvhost_dbg_pmu("pwr_pmu_pg_intren_r(0): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_intren_r(0)));

	nvhost_dbg_pmu("pwr_pmu_idle_count_r(3): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(3)));
	nvhost_dbg_pmu("pwr_pmu_idle_count_r(4): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(4)));
	nvhost_dbg_pmu("pwr_pmu_idle_count_r(7): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_count_r(7)));

	/*
	 TBD: script can't generate those registers correctly
	nvhost_dbg_pmu("pwr_pmu_idle_status_r(): 0x%08x",
		gk20a_readl(g, pwr_pmu_idle_status_r()));
	nvhost_dbg_pmu("pwr_pmu_pg_ctrl_r(): 0x%08x",
		gk20a_readl(g, pwr_pmu_pg_ctrl_r()));
	*/
}

static void pmu_dump_falcon_stats(struct pmu_gk20a *pmu)
{
	struct gk20a *g = pmu->g;
	int i;

	nvhost_err(dev_from_gk20a(g), "pwr_falcon_os_r : %d",
		gk20a_readl(g, pwr_falcon_os_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_cpuctl_r : 0x%x",
		gk20a_readl(g, pwr_falcon_cpuctl_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_idlestate_r : 0x%x",
		gk20a_readl(g, pwr_falcon_idlestate_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_mailbox0_r : 0x%x",
		gk20a_readl(g, pwr_falcon_mailbox0_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_mailbox1_r : 0x%x",
		gk20a_readl(g, pwr_falcon_mailbox1_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_irqstat_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqstat_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_irqmode_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqmode_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_irqmask_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqmask_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_irqdest_r : 0x%x",
		gk20a_readl(g, pwr_falcon_irqdest_r()));

	for (i = 0; i < pwr_pmu_mailbox__size_1_v(); i++)
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_mailbox_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_mailbox_r(i)));

	for (i = 0; i < pwr_pmu_debug__size_1_v(); i++)
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_debug_r(%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_debug_r(i)));

	for (i = 0; i < 6/*NV_PPWR_FALCON_ICD_IDX_RSTAT__SIZE_1*/; i++) {
		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rstat_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(i));
		nvhost_err(dev_from_gk20a(g), "pmu_rstat (%d) : 0x%x",
			i, gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_error_status_r());
	nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_error_status_r : 0x%x", i);
	if (i != 0) {
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_addr_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_addr_r()));
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_data_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_data_r()));
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_timeout_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_timeout_r()));
		nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_ctl_r : 0x%x",
			gk20a_readl(g, pwr_pmu_bar0_ctl_r()));
	}

	i = gk20a_readl(g, pwr_pmu_bar0_fecs_error_r());
	nvhost_err(dev_from_gk20a(g), "pwr_pmu_bar0_fecs_error_r : 0x%x", i);

	i = gk20a_readl(g, pwr_falcon_exterrstat_r());
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_exterrstat_r : 0x%x", i);
	if (pwr_falcon_exterrstat_valid_v(i) ==
			pwr_falcon_exterrstat_valid_true_v()) {
		nvhost_err(dev_from_gk20a(g), "pwr_falcon_exterraddr_r : 0x%x",
			gk20a_readl(g, pwr_falcon_exterraddr_r()));
		nvhost_err(dev_from_gk20a(g), "top_fs_status_r : 0x%x",
			gk20a_readl(g, top_fs_status_r()));
		nvhost_err(dev_from_gk20a(g), "pmc_enable : 0x%x",
			gk20a_readl(g, mc_enable_r()));
	}

	nvhost_err(dev_from_gk20a(g), "pwr_falcon_engctl_r : 0x%x",
		gk20a_readl(g, pwr_falcon_engctl_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_curctx_r : 0x%x",
		gk20a_readl(g, pwr_falcon_curctx_r()));
	nvhost_err(dev_from_gk20a(g), "pwr_falcon_nxtctx_r : 0x%x",
		gk20a_readl(g, pwr_falcon_nxtctx_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_IMB));
	nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_IMB : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_DMB));
	nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_DMB : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_CSW));
	nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_CSW : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_CTX));
	nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_CTX : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
		pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
		pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_EXCI));
	nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_EXCI : 0x%x",
		gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

	for (i = 0; i < 4; i++) {
		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_PC));
		nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_PC : 0x%x",
			gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));

		gk20a_writel(g, pwr_pmu_falcon_icd_cmd_r(),
			pwr_pmu_falcon_icd_cmd_opc_rreg_f() |
			pwr_pmu_falcon_icd_cmd_idx_f(PMU_FALCON_REG_SP));
		nvhost_err(dev_from_gk20a(g), "PMU_FALCON_REG_SP : 0x%x",
			gk20a_readl(g, pwr_pmu_falcon_icd_rdata_r()));
	}

	/* PMU may crash due to FECS crash. Dump FECS status */
	gk20a_fecs_dump_falcon_stats(g);
}

void gk20a_pmu_isr(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_queue *queue;
	u32 intr, mask;
	bool recheck = false;

	nvhost_dbg_fn("");

	mutex_lock(&pmu->isr_mutex);

	mask = gk20a_readl(g, pwr_falcon_irqmask_r()) &
		gk20a_readl(g, pwr_falcon_irqdest_r());

	intr = gk20a_readl(g, pwr_falcon_irqstat_r()) & mask;

	nvhost_dbg_pmu("received falcon interrupt: 0x%08x", intr);

	if (!intr) {
		mutex_unlock(&pmu->isr_mutex);
		return;
	}

	if (intr & pwr_falcon_irqstat_halt_true_f()) {
		nvhost_err(dev_from_gk20a(g),
			"pmu halt intr not implemented");
		pmu_dump_falcon_stats(pmu);
	}
	if (intr & pwr_falcon_irqstat_exterr_true_f()) {
		nvhost_err(dev_from_gk20a(g),
			"pmu exterr intr not implemented. Clearing interrupt.");
		pmu_dump_falcon_stats(pmu);

		gk20a_writel(g, pwr_falcon_exterrstat_r(),
			gk20a_readl(g, pwr_falcon_exterrstat_r()) &
				~pwr_falcon_exterrstat_valid_m());
	}
	if (intr & pwr_falcon_irqstat_swgen0_true_f()) {
		pmu_process_message(pmu);
		recheck = true;
	}

	gk20a_writel(g, pwr_falcon_irqsclr_r(), intr);

	if (recheck) {
		queue = &pmu->queue[PMU_MESSAGE_QUEUE];
		if (!pmu_queue_is_empty(pmu, queue))
			gk20a_writel(g, pwr_falcon_irqsset_r(),
				pwr_falcon_irqsset_swgen0_set_f());
	}

	mutex_unlock(&pmu->isr_mutex);
}

static bool pmu_validate_cmd(struct pmu_gk20a *pmu, struct pmu_cmd *cmd,
			struct pmu_msg *msg, struct pmu_payload *payload,
			u32 queue_id)
{
	struct gk20a *g = pmu->g;
	struct pmu_queue *queue;
	u32 in_size, out_size;

	if (!PMU_IS_SW_COMMAND_QUEUE(queue_id))
		goto invalid_cmd;

	queue = &pmu->queue[queue_id];
	if (cmd->hdr.size < PMU_CMD_HDR_SIZE)
		goto invalid_cmd;

	if (cmd->hdr.size > (queue->size >> 1))
		goto invalid_cmd;

	if (msg != NULL && msg->hdr.size < PMU_MSG_HDR_SIZE)
		goto invalid_cmd;

	if (!PMU_UNIT_ID_IS_VALID(cmd->hdr.unit_id))
		goto invalid_cmd;

	if (payload == NULL)
		return true;

	if (payload->in.buf == NULL && payload->out.buf == NULL)
		goto invalid_cmd;

	if ((payload->in.buf != NULL && payload->in.size == 0) ||
	    (payload->out.buf != NULL && payload->out.size == 0))
		goto invalid_cmd;

	in_size = PMU_CMD_HDR_SIZE;
	if (payload->in.buf) {
		in_size += payload->in.offset;
		in_size += sizeof(struct pmu_allocation);
	}

	out_size = PMU_CMD_HDR_SIZE;
	if (payload->out.buf) {
		out_size += payload->out.offset;
		out_size += sizeof(struct pmu_allocation);
	}

	if (in_size > cmd->hdr.size || out_size > cmd->hdr.size)
		goto invalid_cmd;


	if ((payload->in.offset != 0 && payload->in.buf == NULL) ||
	    (payload->out.offset != 0 && payload->out.buf == NULL))
		goto invalid_cmd;

	return true;

invalid_cmd:
	nvhost_err(dev_from_gk20a(g), "invalid pmu cmd :\n"
		"queue_id=%d,\n"
		"cmd_size=%d, cmd_unit_id=%d, msg=%p, msg_size=%d,\n"
		"payload in=%p, in_size=%d, in_offset=%d,\n"
		"payload out=%p, out_size=%d, out_offset=%d",
		queue_id, cmd->hdr.size, cmd->hdr.unit_id,
		msg, msg?msg->hdr.unit_id:~0,
		&payload->in, payload->in.size, payload->in.offset,
		&payload->out, payload->out.size, payload->out.offset);

	return false;
}

static int pmu_write_cmd(struct pmu_gk20a *pmu, struct pmu_cmd *cmd,
			u32 queue_id, unsigned long timeout)
{
	struct gk20a *g = pmu->g;
	struct pmu_queue *queue;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(timeout);
	int err;

	nvhost_dbg_fn("");

	queue = &pmu->queue[queue_id];

	do {
		err = pmu_queue_open_write(pmu, queue, cmd->hdr.size);
		if (err == -EAGAIN && time_before(jiffies, end_jiffies))
			usleep_range(1000, 2000);
		else
			break;
	} while (1);

	if (err)
		goto clean_up;

	pmu_queue_push(pmu, queue, cmd, cmd->hdr.size);

	err = pmu_queue_close(pmu, queue, true);

clean_up:
	if (err)
		nvhost_err(dev_from_gk20a(g),
			"fail to write cmd to queue %d", queue_id);
	else
		nvhost_dbg_fn("done");

	return err;
}

int gk20a_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd,
		struct pmu_msg *msg, struct pmu_payload *payload,
		u32 queue_id, pmu_callback callback, void* cb_param,
		u32 *seq_desc, unsigned long timeout)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_sequence *seq;
	struct pmu_allocation *in = NULL, *out = NULL;
	int err;

	nvhost_dbg_fn("");

	BUG_ON(!cmd);
	BUG_ON(!seq_desc);
	BUG_ON(!pmu->pmu_ready);

	if (!pmu_validate_cmd(pmu, cmd, msg, payload, queue_id))
		return -EINVAL;

	err = pmu_seq_acquire(pmu, &seq);
	if (err)
		return err;

	cmd->hdr.seq_id = seq->id;

	cmd->hdr.ctrl_flags = 0;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_STATUS;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_INTR;

	seq->callback = callback;
	seq->cb_params = cb_param;
	seq->msg = msg;
	seq->out_payload = NULL;
	seq->desc = pmu->next_seq_desc++;

	if (payload)
		seq->out_payload = payload->out.buf;

	*seq_desc = seq->desc;

	if (payload && payload->in.offset != 0) {
		in = (struct pmu_allocation *)
			((u8 *)&cmd->cmd + payload->in.offset);

		if (payload->in.buf != payload->out.buf)
			in->alloc.dmem.size = (u16)payload->in.size;
		else
			in->alloc.dmem.size = (u16)max(payload->in.size,
				payload->out.size);

		err = pmu->dmem.alloc(&pmu->dmem, &in->alloc.dmem.offset,
			in->alloc.dmem.size);
		if (err)
			goto clean_up;

		pmu_copy_to_dmem(pmu, in->alloc.dmem.offset,
			payload->in.buf, payload->in.size, 0);

		seq->in.alloc.dmem.size = in->alloc.dmem.size;
		seq->in.alloc.dmem.offset = in->alloc.dmem.offset;
	}

	if (payload && payload->out.offset != 0) {
		out = (struct pmu_allocation *)
			((u8 *)&cmd->cmd + payload->out.offset);

		out->alloc.dmem.size = (u16)payload->out.size;

		if (payload->out.buf != payload->in.buf) {
			err = pmu->dmem.alloc(&pmu->dmem,
				&out->alloc.dmem.offset, out->alloc.dmem.size);
			if (err)
				goto clean_up;
		} else {
			BUG_ON(in == NULL);
			out->alloc.dmem.offset = in->alloc.dmem.offset;
		}

		seq->out.alloc.dmem.size = out->alloc.dmem.size;
		seq->out.alloc.dmem.offset = out->alloc.dmem.offset;
	}

	seq->state = PMU_SEQ_STATE_USED;
	err = pmu_write_cmd(pmu, cmd, queue_id, timeout);
	if (err)
		seq->state = PMU_SEQ_STATE_PENDING;

	nvhost_dbg_fn("done");

	return 0;

clean_up:
	nvhost_dbg_fn("fail");
	if (in)
		pmu->dmem.free(&pmu->dmem, in->alloc.dmem.offset,
			in->alloc.dmem.size);
	if (out)
		pmu->dmem.free(&pmu->dmem, out->alloc.dmem.offset,
			out->alloc.dmem.size);

	pmu_seq_release(pmu, seq);
	return err;
}

static int gk20a_pmu_enable_elpg_locked(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq, status;

	nvhost_dbg_fn("");

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_ALLOW;

	/* no need to wait ack for ELPG enable but set pending to sync
	   with follow up ELPG disable */
	pmu->elpg_stat = PMU_ELPG_STAT_ON_PENDING;

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	BUG_ON(status != 0);

	nvhost_dbg_fn("done");
	return 0;
}

int gk20a_pmu_enable_elpg(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct gr_gk20a *gr = &g->gr;

	int ret = 0;

	nvhost_dbg_fn("");

	if (!pmu->elpg_ready || !pmu->initialized)
		goto exit;

	mutex_lock(&pmu->elpg_mutex);

	pmu->elpg_refcnt++;
	if (pmu->elpg_refcnt <= 0)
		goto exit_unlock;

	/* something is not right if we end up in following code path */
	if (unlikely(pmu->elpg_refcnt > 1)) {
		nvhost_warn(dev_from_gk20a(g), "%s(): possible elpg refcnt mismatch. elpg refcnt=%d",
			    __func__, pmu->elpg_refcnt);
		WARN_ON(1);
	}

	/* do NOT enable elpg until golden ctx is created,
	   which is related with the ctx that ELPG save and restore. */
	if (unlikely(!gr->ctx_vars.golden_image_initialized))
		goto exit_unlock;

	/* return if ELPG is already on or on_pending or off_on_pending */
	if (pmu->elpg_stat != PMU_ELPG_STAT_OFF)
		goto exit_unlock;

	/* if ELPG is not allowed right now, mark that it should be enabled
	 * immediately after it is allowed */
	if (!pmu->elpg_enable_allow) {
		pmu->elpg_stat = PMU_ELPG_STAT_OFF_ON_PENDING;
		goto exit_unlock;
	}

	ret = gk20a_pmu_enable_elpg_locked(g);

exit_unlock:
	mutex_unlock(&pmu->elpg_mutex);
exit:
	nvhost_dbg_fn("done");
	return ret;
}

static void pmu_elpg_enable_allow(struct work_struct *work)
{
	struct pmu_gk20a *pmu = container_of(to_delayed_work(work),
					struct pmu_gk20a, elpg_enable);

	nvhost_dbg_fn("");

	mutex_lock(&pmu->elpg_mutex);

	/* It is ok to enabled powergating now */
	pmu->elpg_enable_allow = true;

	/* do we have pending requests? */
	if (pmu->elpg_stat == PMU_ELPG_STAT_OFF_ON_PENDING) {
		pmu->elpg_stat = PMU_ELPG_STAT_OFF;
		gk20a_pmu_enable_elpg_locked(pmu->g);
	}

	mutex_unlock(&pmu->elpg_mutex);

	nvhost_dbg_fn("done");
}

static int gk20a_pmu_disable_elpg_defer_enable(struct gk20a *g, bool enable)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_cmd cmd;
	u32 seq;
	int ret = 0;

	nvhost_dbg_fn("");

	if (!pmu->elpg_ready || !pmu->initialized)
		return 0;

	/* remove the work from queue */
	cancel_delayed_work_sync(&pmu->elpg_enable);

	mutex_lock(&pmu->elpg_mutex);

	pmu->elpg_refcnt--;
	if (pmu->elpg_refcnt > 0) {
		nvhost_warn(dev_from_gk20a(g), "%s(): possible elpg refcnt mismatch. elpg refcnt=%d",
			    __func__, pmu->elpg_refcnt);
		WARN_ON(1);
		ret = 0;
		goto exit_unlock;
	}

	/* cancel off_on_pending and return */
	if (pmu->elpg_stat == PMU_ELPG_STAT_OFF_ON_PENDING) {
		pmu->elpg_stat = PMU_ELPG_STAT_OFF;
		ret = 0;
		goto exit_reschedule;
	}
	/* wait if on_pending */
	else if (pmu->elpg_stat == PMU_ELPG_STAT_ON_PENDING) {

		pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
				      &pmu->elpg_stat, PMU_ELPG_STAT_ON);

		if (pmu->elpg_stat != PMU_ELPG_STAT_ON) {
			nvhost_err(dev_from_gk20a(g),
				"ELPG_ALLOW_ACK failed, elpg_stat=%d",
				pmu->elpg_stat);
			pmu_dump_elpg_stats(pmu);
			pmu_dump_falcon_stats(pmu);
			ret = -EBUSY;
			goto exit_unlock;
		}
	}
	/* return if ELPG is already off */
	else if (pmu->elpg_stat != PMU_ELPG_STAT_ON) {
		ret = 0;
		goto exit_reschedule;
	}

	memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_pg_cmd_elpg_cmd);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = ENGINE_GR_GK20A;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_DISALLOW;

	pmu->elpg_stat = PMU_ELPG_STAT_OFF_PENDING;

	gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu, &seq, ~0);

	pmu_wait_message_cond(pmu, gk20a_get_gr_idle_timeout(g),
			      &pmu->elpg_stat, PMU_ELPG_STAT_OFF);
	if (pmu->elpg_stat != PMU_ELPG_STAT_OFF) {
		nvhost_err(dev_from_gk20a(g),
			"ELPG_DISALLOW_ACK failed");
		pmu_dump_elpg_stats(pmu);
		pmu_dump_falcon_stats(pmu);
		ret = -EBUSY;
		goto exit_unlock;
	}

exit_reschedule:
	if (enable) {
		pmu->elpg_enable_allow = false;
		schedule_delayed_work(&pmu->elpg_enable,
			msecs_to_jiffies(PMU_ELPG_ENABLE_ALLOW_DELAY_MSEC));
	} else
		pmu->elpg_enable_allow = true;


exit_unlock:
	mutex_unlock(&pmu->elpg_mutex);
	nvhost_dbg_fn("done");
	return ret;
}

int gk20a_pmu_disable_elpg(struct gk20a *g)
{
	return gk20a_pmu_disable_elpg_defer_enable(g, true);
}

int gk20a_pmu_perfmon_enable(struct gk20a *g, bool enable)
{
	struct pmu_gk20a *pmu = &g->pmu;
	int err;

	nvhost_dbg_fn("");

	if (enable)
		err = pmu_perfmon_start_sampling(pmu);
	else
		err = pmu_perfmon_stop_sampling(pmu);

	return err;
}

int gk20a_pmu_destroy(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u32 elpg_ingating_time, elpg_ungating_time, gating_cnt;

	nvhost_dbg_fn("");

	if (!support_gk20a_pmu())
		return 0;

	/* make sure the pending operations are finished before we continue */
	cancel_delayed_work_sync(&pmu->elpg_enable);

	gk20a_pmu_get_elpg_residency_gating(g, &elpg_ingating_time,
		&elpg_ungating_time, &gating_cnt);

	mutex_lock(&pmu->pg_init_mutex);
	gk20a_pmu_disable_elpg_defer_enable(g, false);
	pmu->initialized = false;
	mutex_unlock(&pmu->pg_init_mutex);

	/* update the s/w ELPG residency counters */
	g->pg_ingating_time_us += (u64)elpg_ingating_time;
	g->pg_ungating_time_us += (u64)elpg_ungating_time;
	g->pg_gating_cnt += gating_cnt;

	pmu_enable(pmu, false);

	if (pmu->remove_support) {
		pmu->remove_support(pmu);
		pmu->remove_support = NULL;
	}

	nvhost_dbg_fn("done");
	return 0;
}

int gk20a_pmu_load_norm(struct gk20a *g, u32 *load)
{
	struct pmu_gk20a *pmu = &g->pmu;
	u16 _load = 0;

	if (!pmu->perfmon_ready) {
		*load = 0;
		return 0;
	}

	pmu_copy_from_dmem(pmu, pmu->sample_buffer, (u8 *)&_load, 2, 0);
	*load = _load / 10;

	return 0;
}

void gk20a_pmu_get_load_counters(struct gk20a *g, u32 *busy_cycles,
				 u32 *total_cycles)
{
	struct pmu_gk20a *pmu = &(g->pmu);

	if (!g->power_on) {
		*busy_cycles = 0;
		*total_cycles = 0;
		return;
	}

	gk20a_busy(g->dev);
	*busy_cycles = pwr_pmu_idle_count_value_v(
		gk20a_readl(g, pwr_pmu_idle_count_r(1)));
	rmb();
	*total_cycles = pwr_pmu_idle_count_value_v(
		gk20a_readl(g, pwr_pmu_idle_count_r(2)));
	gk20a_idle(g->dev);
}

void gk20a_pmu_reset_load_counters(struct gk20a *g)
{
	struct pmu_gk20a *pmu = &(g->pmu);
	u32 reg_val = pwr_pmu_idle_count_reset_f(1);

	if (!g->power_on)
		return;

	gk20a_busy(g->dev);
	gk20a_writel(g, pwr_pmu_idle_count_r(2), reg_val);
	wmb();
	gk20a_writel(g, pwr_pmu_idle_count_r(1), reg_val);
	gk20a_idle(g->dev);
}

static int gk20a_pmu_get_elpg_residency_gating(struct gk20a *g,
			u32 *ingating_time, u32 *ungating_time, u32 *gating_cnt)
{
	struct pmu_gk20a *pmu = &g->pmu;
	struct pmu_pg_stats stats;

	if (!pmu->initialized) {
		*ingating_time = 0;
		*ungating_time = 0;
		*gating_cnt = 0;
		return 0;
	}

	pmu_copy_from_dmem(pmu, pmu->stat_dmem_offset,
		(u8 *)&stats, sizeof(struct pmu_pg_stats), 0);

	*ingating_time = stats.pg_ingating_time_us;
	*ungating_time = stats.pg_ungating_time_us;
	*gating_cnt = stats.pg_gating_cnt;

	return 0;
}

/* Send an Adaptive Power (AP) related command to PMU */
static int gk20a_pmu_ap_send_command(struct gk20a *g,
			union pmu_ap_cmd *p_ap_cmd, bool b_block)
{
	struct pmu_gk20a *pmu = &g->pmu;
	/* FIXME: where is the PG structure defined?? */
	u32 status = 0;
	struct pmu_cmd cmd;
	u32 seq;
	pmu_callback p_callback = NULL;

	memset(&cmd, 0, sizeof(struct pmu_cmd));

	/* Copy common members */
	cmd.hdr.unit_id = PMU_UNIT_PG;
	cmd.hdr.size = PMU_CMD_HDR_SIZE + sizeof(union pmu_ap_cmd);

	cmd.cmd.pg.ap_cmd.cmn.cmd_type = PMU_PG_CMD_ID_AP;
	cmd.cmd.pg.ap_cmd.cmn.cmd_id = p_ap_cmd->cmn.cmd_id;

	/* Copy other members of command */
	switch (p_ap_cmd->cmn.cmd_id) {
	case PMU_AP_CMD_ID_INIT:
		cmd.cmd.pg.ap_cmd.init.pg_sampling_period_us =
			p_ap_cmd->init.pg_sampling_period_us;
		p_callback = ap_callback_init_and_enable_ctrl;
		break;

	case PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL:
		cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.ctrl_id =
		p_ap_cmd->init_and_enable_ctrl.ctrl_id;
		memcpy(
		(void *)&(cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.params),
			(void *)&(p_ap_cmd->init_and_enable_ctrl.params),
			sizeof(struct pmu_ap_ctrl_init_params));

		p_callback = ap_callback_init_and_enable_ctrl;
		break;

	case PMU_AP_CMD_ID_ENABLE_CTRL:
		cmd.cmd.pg.ap_cmd.enable_ctrl.ctrl_id =
			p_ap_cmd->enable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_DISABLE_CTRL:
		cmd.cmd.pg.ap_cmd.disable_ctrl.ctrl_id =
			p_ap_cmd->disable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_KICK_CTRL:
		cmd.cmd.pg.ap_cmd.kick_ctrl.ctrl_id =
			p_ap_cmd->kick_ctrl.ctrl_id;
		cmd.cmd.pg.ap_cmd.kick_ctrl.skip_count =
			p_ap_cmd->kick_ctrl.skip_count;
		break;

	default:
		nvhost_dbg_pmu("%s: Invalid Adaptive Power command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		return 0x2f;
	}

	status = gk20a_pmu_cmd_post(g, &cmd, NULL, NULL, PMU_COMMAND_QUEUE_HPQ,
			p_callback, pmu, &seq, ~0);

	if (!status) {
		nvhost_dbg_pmu(
			"%s: Unable to submit Adaptive Power Command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		goto err_return;
	}

	/* TODO: Implement blocking calls (b_block) */

err_return:
	return status;
}

static void ap_callback_init_and_enable_ctrl(
		struct gk20a *g, struct pmu_msg *msg,
		void *param, u32 seq_desc, u32 status)
{
	/* Define p_ap (i.e pointer to pmu_ap structure) */
	WARN_ON(!msg);

	if (!status) {
		switch (msg->msg.pg.ap_msg.cmn.msg_id) {
		case PMU_AP_MSG_ID_INIT_ACK:
			break;

		default:
			nvhost_dbg_pmu(
			"%s: Invalid Adaptive Power Message: %x\n",
			__func__, msg->msg.pg.ap_msg.cmn.msg_id);
			break;
		}
	}
}

static int gk20a_aelpg_init(struct gk20a *g)
{
	int status = 0;

	/* Remove reliance on app_ctrl field. */
	union pmu_ap_cmd ap_cmd;

	/* TODO: Check for elpg being ready? */
	ap_cmd.init.cmd_id = PMU_AP_CMD_ID_INIT;
	ap_cmd.init.pg_sampling_period_us =
		APCTRL_SAMPLING_PERIOD_PG_DEFAULT_US;

	status = gk20a_pmu_ap_send_command(g, &ap_cmd, false);
	return status;
}

static int gk20a_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id)
{
	int status = 0;
	union pmu_ap_cmd ap_cmd;

	/* TODO: Probably check if ELPG is ready? */

	ap_cmd.init_and_enable_ctrl.cmd_id = PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL;
	ap_cmd.init_and_enable_ctrl.ctrl_id = ctrl_id;
	ap_cmd.init_and_enable_ctrl.params.min_idle_filter_us =
		APCTRL_MINIMUM_IDLE_FILTER_DEFAULT_US;
	ap_cmd.init_and_enable_ctrl.params.min_target_saving_us =
		APCTRL_MINIMUM_TARGET_SAVING_DEFAULT_US;
	ap_cmd.init_and_enable_ctrl.params.power_break_even_us =
		APCTRL_POWER_BREAKEVEN_DEFAULT_US;
	ap_cmd.init_and_enable_ctrl.params.cycles_per_sample_max =
		APCTRL_CYCLES_PER_SAMPLE_MAX_DEFAULT;

	switch (ctrl_id) {
	case PMU_AP_CTRL_ID_GRAPHICS:
		break;
	default:
		break;
	}

	status = gk20a_pmu_ap_send_command(g, &ap_cmd, true);
	return status;
}

#if CONFIG_DEBUG_FS
static int elpg_residency_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 ingating_time = 0;
	u32 ungating_time = 0;
	u32 gating_cnt;
	u64 total_ingating, total_ungating, residency, divisor, dividend;

	/* Don't unnecessarily power on the device */
	if (g->power_on) {
		gk20a_busy(g->dev);
		gk20a_pmu_get_elpg_residency_gating(g, &ingating_time,
			&ungating_time, &gating_cnt);
		gk20a_idle(g->dev);
	}
	total_ingating = g->pg_ingating_time_us + (u64)ingating_time;
	total_ungating = g->pg_ungating_time_us + (u64)ungating_time;
	divisor = total_ingating + total_ungating;

	/* We compute the residency on a scale of 1000 */
	dividend = total_ingating * 1000;

	if (divisor)
		residency = div64_u64(dividend, divisor);
	else
		residency = 0;

	seq_printf(s, "Time in ELPG: %llu us\n"
			"Time out of ELPG: %llu us\n"
			"ELPG residency ratio: %llu\n",
			total_ingating, total_ungating, residency);
	return 0;

}

static int elpg_residency_open(struct inode *inode, struct file *file)
{
	return single_open(file, elpg_residency_show, inode->i_private);
}

static const struct file_operations elpg_residency_fops = {
	.open		= elpg_residency_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int elpg_transitions_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 ingating_time, ungating_time, total_gating_cnt;
	u32 gating_cnt = 0;

	if (g->power_on) {
		gk20a_busy(g->dev);
		gk20a_pmu_get_elpg_residency_gating(g, &ingating_time,
			&ungating_time, &gating_cnt);
		gk20a_idle(g->dev);
	}
	total_gating_cnt = g->pg_gating_cnt + gating_cnt;

	seq_printf(s, "%u\n", total_gating_cnt);
	return 0;

}

static int elpg_transitions_open(struct inode *inode, struct file *file)
{
	return single_open(file, elpg_transitions_show, inode->i_private);
}

static const struct file_operations elpg_transitions_fops = {
	.open		= elpg_transitions_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int gk20a_pmu_debugfs_init(struct platform_device *dev)
{
	struct dentry *d;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);

	d = debugfs_create_file(
		"elpg_residency", S_IRUGO|S_IWUSR, pdata->debugfs, g,
						&elpg_residency_fops);
	if (!d)
		goto err_out;

	d = debugfs_create_file(
		"elpg_transitions", S_IRUGO, pdata->debugfs, g,
						&elpg_transitions_fops);
	if (!d)
		goto err_out;

	return 0;

err_out:
	pr_err("%s: Failed to make debugfs node\n", __func__);
	debugfs_remove_recursive(pdata->debugfs);
	return -ENOMEM;
}
#endif
