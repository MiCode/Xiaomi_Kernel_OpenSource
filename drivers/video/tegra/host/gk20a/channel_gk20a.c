/*
 * drivers/video/tegra/host/gk20a/channel_gk20a.c
 *
 * GK20A Graphics channel
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/list.h>
#include <linux/delay.h>
#include <linux/highmem.h> /* need for nvmap.h*/
#include <trace/events/nvhost.h>
#include <linux/scatterlist.h>

#include "dev.h"
#include "nvhost_as.h"
#include "debug.h"
#include "nvhost_sync.h"

#include "gk20a.h"
#include "dbg_gpu_gk20a.h"

#include "hw_ram_gk20a.h"
#include "hw_fifo_gk20a.h"
#include "hw_pbdma_gk20a.h"
#include "hw_ccsr_gk20a.h"
#include "hw_ltc_gk20a.h"
#include "chip_support.h"

#define NVMAP_HANDLE_PARAM_SIZE 1

static struct channel_gk20a *acquire_unused_channel(struct fifo_gk20a *f);
static void release_used_channel(struct fifo_gk20a *f, struct channel_gk20a *c);

static int alloc_priv_cmdbuf(struct channel_gk20a *c, u32 size,
			     struct priv_cmd_entry **entry);
static void free_priv_cmdbuf(struct channel_gk20a *c,
			     struct priv_cmd_entry *e);
static void recycle_priv_cmdbuf(struct channel_gk20a *c);

static int channel_gk20a_alloc_priv_cmdbuf(struct channel_gk20a *c);
static void channel_gk20a_free_priv_cmdbuf(struct channel_gk20a *c);

static int channel_gk20a_commit_userd(struct channel_gk20a *c);
static int channel_gk20a_setup_userd(struct channel_gk20a *c);
static int channel_gk20a_setup_ramfc(struct channel_gk20a *c,
			u64 gpfifo_base, u32 gpfifo_entries);

static void channel_gk20a_bind(struct channel_gk20a *ch_gk20a);
static void channel_gk20a_unbind(struct channel_gk20a *ch_gk20a);

static int channel_gk20a_alloc_inst(struct gk20a *g,
				struct channel_gk20a *ch);
static void channel_gk20a_free_inst(struct gk20a *g,
				struct channel_gk20a *ch);

static int channel_gk20a_update_runlist(struct channel_gk20a *c,
					bool add);

static struct channel_gk20a *acquire_unused_channel(struct fifo_gk20a *f)
{
	struct channel_gk20a *ch = NULL;
	int chid;

	mutex_lock(&f->ch_inuse_mutex);
	for (chid = 0; chid < f->num_channels; chid++) {
		if (!f->channel[chid].in_use) {
			f->channel[chid].in_use = true;
			ch = &f->channel[chid];
			break;
		}
	}
	mutex_unlock(&f->ch_inuse_mutex);

	return ch;
}

static void release_used_channel(struct fifo_gk20a *f, struct channel_gk20a *c)
{
	mutex_lock(&f->ch_inuse_mutex);
	f->channel[c->hw_chid].in_use = false;
	mutex_unlock(&f->ch_inuse_mutex);
}

int channel_gk20a_commit_va(struct channel_gk20a *c)
{
	u64 addr;
	u32 addr_lo;
	u32 addr_hi;
	void *inst_ptr;

	nvhost_dbg_fn("");

	inst_ptr = c->inst_block.cpuva;
	if (!inst_ptr)
		return -ENOMEM;

	addr = gk20a_mm_iova_addr(c->vm->pdes.sgt->sgl);
	addr_lo = u64_lo32(addr >> 12);
	addr_hi = u64_hi32(addr);

	nvhost_dbg_info("pde pa=0x%llx addr_lo=0x%x addr_hi=0x%x",
		   (u64)addr, addr_lo, addr_hi);

	mem_wr32(inst_ptr, ram_in_page_dir_base_lo_w(),
		ram_in_page_dir_base_target_vid_mem_f() |
		ram_in_page_dir_base_vol_true_f() |
		ram_in_page_dir_base_lo_f(addr_lo));

	mem_wr32(inst_ptr, ram_in_page_dir_base_hi_w(),
		ram_in_page_dir_base_hi_f(addr_hi));

	mem_wr32(inst_ptr, ram_in_adr_limit_lo_w(),
		 u64_lo32(c->vm->va_limit) | 0xFFF);

	mem_wr32(inst_ptr, ram_in_adr_limit_hi_w(),
		ram_in_adr_limit_hi_f(u64_hi32(c->vm->va_limit)));

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

static int channel_gk20a_commit_userd(struct channel_gk20a *c)
{
	u32 addr_lo;
	u32 addr_hi;
	void *inst_ptr;

	nvhost_dbg_fn("");

	inst_ptr = c->inst_block.cpuva;
	if (!inst_ptr)
		return -ENOMEM;

	addr_lo = u64_lo32(c->userd_iova >> ram_userd_base_shift_v());
	addr_hi = u64_hi32(c->userd_iova);

	nvhost_dbg_info("channel %d : set ramfc userd 0x%16llx",
		c->hw_chid, c->userd_iova);

	mem_wr32(inst_ptr, ram_in_ramfc_w() + ram_fc_userd_w(),
		 pbdma_userd_target_vid_mem_f() |
		 pbdma_userd_addr_f(addr_lo));

	mem_wr32(inst_ptr, ram_in_ramfc_w() + ram_fc_userd_hi_w(),
		 pbdma_userd_target_vid_mem_f() |
		 pbdma_userd_hi_addr_f(addr_hi));

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

static int channel_gk20a_set_schedule_params(struct channel_gk20a *c,
				u32 timeslice_timeout)
{
	void *inst_ptr;
	int shift = 3;
	int value = timeslice_timeout;

	inst_ptr = c->inst_block.cpuva;
	if (!inst_ptr)
		return -ENOMEM;

	/* disable channel */
	gk20a_writel(c->g, ccsr_channel_r(c->hw_chid),
		gk20a_readl(c->g, ccsr_channel_r(c->hw_chid)) |
		ccsr_channel_enable_clr_true_f());

	/* preempt the channel */
	WARN_ON(gk20a_fifo_preempt_channel(c->g, c->hw_chid));

	/* flush GPU cache */
	gk20a_mm_l2_flush(c->g, true);

	/* value field is 8 bits long */
	while (value >= 1 << 8) {
		value >>= 1;
		shift++;
	}

	/* time slice register is only 18bits long */
	if ((value << shift) >= 1<<19) {
		pr_err("Requested timeslice value is clamped to 18 bits\n");
		value = 255;
		shift = 10;
	}

	/* set new timeslice */
	mem_wr32(inst_ptr, ram_fc_eng_timeslice_w(),
		value | (shift << 12) |
		fifo_eng_timeslice_enable_true_f());

	/* enable channel */
	gk20a_writel(c->g, ccsr_channel_r(c->hw_chid),
		gk20a_readl(c->g, ccsr_channel_r(c->hw_chid)) |
		ccsr_channel_enable_set_true_f());

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

static int channel_gk20a_setup_ramfc(struct channel_gk20a *c,
				u64 gpfifo_base, u32 gpfifo_entries)
{
	void *inst_ptr;

	nvhost_dbg_fn("");

	inst_ptr = c->inst_block.cpuva;
	if (!inst_ptr)
		return -ENOMEM;

	memset(inst_ptr, 0, ram_fc_size_val_v());

	mem_wr32(inst_ptr, ram_fc_gp_base_w(),
		pbdma_gp_base_offset_f(
		u64_lo32(gpfifo_base >> pbdma_gp_base_rsvd_s())));

	mem_wr32(inst_ptr, ram_fc_gp_base_hi_w(),
		pbdma_gp_base_hi_offset_f(u64_hi32(gpfifo_base)) |
		pbdma_gp_base_hi_limit2_f(ilog2(gpfifo_entries)));

	mem_wr32(inst_ptr, ram_fc_signature_w(),
		 pbdma_signature_hw_valid_f() | pbdma_signature_sw_zero_f());

	mem_wr32(inst_ptr, ram_fc_formats_w(),
		pbdma_formats_gp_fermi0_f() |
		pbdma_formats_pb_fermi1_f() |
		pbdma_formats_mp_fermi0_f());

	mem_wr32(inst_ptr, ram_fc_pb_header_w(),
		pbdma_pb_header_priv_user_f() |
		pbdma_pb_header_method_zero_f() |
		pbdma_pb_header_subchannel_zero_f() |
		pbdma_pb_header_level_main_f() |
		pbdma_pb_header_first_true_f() |
		pbdma_pb_header_type_inc_f());

	mem_wr32(inst_ptr, ram_fc_subdevice_w(),
		pbdma_subdevice_id_f(1) |
		pbdma_subdevice_status_active_f() |
		pbdma_subdevice_channel_dma_enable_f());

	mem_wr32(inst_ptr, ram_fc_target_w(), pbdma_target_engine_sw_f());

	mem_wr32(inst_ptr, ram_fc_acquire_w(),
		pbdma_acquire_retry_man_2_f() |
		pbdma_acquire_retry_exp_2_f() |
		pbdma_acquire_timeout_exp_max_f() |
		pbdma_acquire_timeout_man_max_f() |
		pbdma_acquire_timeout_en_disable_f());

	mem_wr32(inst_ptr, ram_fc_eng_timeslice_w(),
		fifo_eng_timeslice_timeout_128_f() |
		fifo_eng_timeslice_timescale_3_f() |
		fifo_eng_timeslice_enable_true_f());

	mem_wr32(inst_ptr, ram_fc_pb_timeslice_w(),
		fifo_pb_timeslice_timeout_16_f() |
		fifo_pb_timeslice_timescale_0_f() |
		fifo_pb_timeslice_enable_true_f());

	mem_wr32(inst_ptr, ram_fc_chid_w(), ram_fc_chid_id_f(c->hw_chid));

	/* TBD: alwasy priv mode? */
	mem_wr32(inst_ptr, ram_fc_hce_ctrl_w(),
		 pbdma_hce_ctrl_hce_priv_mode_yes_f());

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

static int channel_gk20a_setup_userd(struct channel_gk20a *c)
{
	BUG_ON(!c->userd_cpu_va);

	nvhost_dbg_fn("");

	mem_wr32(c->userd_cpu_va, ram_userd_put_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_get_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_ref_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_put_hi_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_ref_threshold_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_gp_top_level_get_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_gp_top_level_get_hi_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_get_hi_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_gp_get_w(), 0);
	mem_wr32(c->userd_cpu_va, ram_userd_gp_put_w(), 0);

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

static void channel_gk20a_bind(struct channel_gk20a *ch_gk20a)
{
	struct gk20a *g = get_gk20a(ch_gk20a->ch->dev);
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *engine_info =
		f->engine_info + ENGINE_GR_GK20A;

	u32 inst_ptr = ch_gk20a->inst_block.cpu_pa
		>> ram_in_base_shift_v();

	nvhost_dbg_info("bind channel %d inst ptr 0x%08x",
		ch_gk20a->hw_chid, inst_ptr);

	ch_gk20a->bound = true;

	gk20a_writel(g, ccsr_channel_r(ch_gk20a->hw_chid),
		(gk20a_readl(g, ccsr_channel_r(ch_gk20a->hw_chid)) &
		 ~ccsr_channel_runlist_f(~0)) |
		 ccsr_channel_runlist_f(engine_info->runlist_id));

	gk20a_writel(g, ccsr_channel_inst_r(ch_gk20a->hw_chid),
		ccsr_channel_inst_ptr_f(inst_ptr) |
		ccsr_channel_inst_target_vid_mem_f() |
		ccsr_channel_inst_bind_true_f());

	gk20a_writel(g, ccsr_channel_r(ch_gk20a->hw_chid),
		(gk20a_readl(g, ccsr_channel_r(ch_gk20a->hw_chid)) &
		 ~ccsr_channel_enable_set_f(~0)) |
		 ccsr_channel_enable_set_true_f());
}

static void channel_gk20a_unbind(struct channel_gk20a *ch_gk20a)
{
	struct gk20a *g = get_gk20a(ch_gk20a->ch->dev);

	nvhost_dbg_fn("");

	if (ch_gk20a->bound)
		gk20a_writel(g, ccsr_channel_inst_r(ch_gk20a->hw_chid),
			ccsr_channel_inst_ptr_f(0) |
			ccsr_channel_inst_bind_false_f());

	ch_gk20a->bound = false;
}

static int channel_gk20a_alloc_inst(struct gk20a *g,
				struct channel_gk20a *ch)
{
	struct device *d = dev_from_gk20a(g);
	int err = 0;

	nvhost_dbg_fn("");

	ch->inst_block.size = ram_in_alloc_size_v();
	ch->inst_block.cpuva = dma_alloc_coherent(d,
					ch->inst_block.size,
					&ch->inst_block.iova,
					GFP_KERNEL);
	if (!ch->inst_block.cpuva) {
		nvhost_err(d, "%s: memory allocation failed\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	ch->inst_block.cpu_pa = gk20a_get_phys_from_iova(d,
							ch->inst_block.iova);
	if (!ch->inst_block.cpu_pa) {
		nvhost_err(d, "%s: failed to get physical address\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	nvhost_dbg_info("channel %d inst block physical addr: 0x%16llx",
		ch->hw_chid, ch->inst_block.cpu_pa);

	nvhost_dbg_fn("done");
	return 0;

clean_up:
	nvhost_err(d, "fail");
	channel_gk20a_free_inst(g, ch);
	return err;
}

static void channel_gk20a_free_inst(struct gk20a *g,
				struct channel_gk20a *ch)
{
	struct device *d = dev_from_gk20a(g);

	if (ch->inst_block.cpuva)
		dma_free_coherent(d, ch->inst_block.size,
				ch->inst_block.cpuva, ch->inst_block.iova);
	ch->inst_block.cpuva = NULL;
	ch->inst_block.iova = 0;
	memset(&ch->inst_block, 0, sizeof(struct inst_desc));
}

static int channel_gk20a_update_runlist(struct channel_gk20a *c, bool add)
{
	return gk20a_fifo_update_runlist(c->g, 0, c->hw_chid, add, true);
}

void gk20a_disable_channel_no_update(struct channel_gk20a *ch)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(ch->g->dev);
	struct nvhost_master *host = host_from_gk20a_channel(ch);

	/* ensure no fences are pending */
	nvhost_syncpt_set_min_eq_max(&host->syncpt,
				     ch->hw_chid + pdata->syncpt_base);

	/* disable channel */
	gk20a_writel(ch->g, ccsr_channel_r(ch->hw_chid),
		     gk20a_readl(ch->g,
		     ccsr_channel_r(ch->hw_chid)) |
		     ccsr_channel_enable_clr_true_f());
}

static int gk20a_wait_channel_idle(struct channel_gk20a *ch)
{
	bool channel_idle = false;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(ch->g));

	do {
		mutex_lock(&ch->jobs_lock);
		channel_idle = list_empty(&ch->jobs);
		mutex_unlock(&ch->jobs_lock);
		if (channel_idle)
			break;

		usleep_range(1000, 3000);
	} while (time_before(jiffies, end_jiffies));

	if (!channel_idle)
		nvhost_err(dev_from_gk20a(ch->g), "channel jobs not freed");

	return 0;
}

void gk20a_disable_channel(struct channel_gk20a *ch,
			   bool finish,
			   unsigned long finish_timeout)
{
	if (finish) {
		int err = gk20a_channel_finish(ch, finish_timeout);
		WARN_ON(err);
	}

	/* disable the channel from hw and increment syncpoints */
	gk20a_disable_channel_no_update(ch);

	gk20a_wait_channel_idle(ch);

	/* preempt the channel */
	gk20a_fifo_preempt_channel(ch->g, ch->hw_chid);

	/* remove channel from runlist */
	channel_gk20a_update_runlist(ch, false);
}

#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)

static void gk20a_free_cycle_stats_buffer(struct channel_gk20a *ch)
{
	struct mem_mgr *memmgr = gk20a_channel_mem_mgr(ch);
	/* disable existing cyclestats buffer */
	mutex_lock(&ch->cyclestate.cyclestate_buffer_mutex);
	if (ch->cyclestate.cyclestate_buffer_handler) {
		nvhost_memmgr_munmap(ch->cyclestate.cyclestate_buffer_handler,
				ch->cyclestate.cyclestate_buffer);
		nvhost_memmgr_put(memmgr,
				ch->cyclestate.cyclestate_buffer_handler);
		ch->cyclestate.cyclestate_buffer_handler = NULL;
		ch->cyclestate.cyclestate_buffer = NULL;
		ch->cyclestate.cyclestate_buffer_size = 0;
	}
	mutex_unlock(&ch->cyclestate.cyclestate_buffer_mutex);
}

int gk20a_channel_cycle_stats(struct channel_gk20a *ch,
		       struct nvhost_cycle_stats_args *args)
{
	struct mem_mgr *memmgr = gk20a_channel_mem_mgr(ch);
	struct mem_handle *handle_ref;
	void *virtual_address;
	u64 cyclestate_buffer_size;
	struct platform_device *dev = ch->ch->dev;

	if (args->nvmap_handle && !ch->cyclestate.cyclestate_buffer_handler) {

		/* set up new cyclestats buffer */
		handle_ref = nvhost_memmgr_get(memmgr,
				args->nvmap_handle, dev);
		if (IS_ERR(handle_ref))
			return PTR_ERR(handle_ref);
		virtual_address = nvhost_memmgr_mmap(handle_ref);
		if (!virtual_address)
			return -ENOMEM;

		nvhost_memmgr_get_param(memmgr, handle_ref,
					NVMAP_HANDLE_PARAM_SIZE,
					&cyclestate_buffer_size);

		ch->cyclestate.cyclestate_buffer_handler = handle_ref;
		ch->cyclestate.cyclestate_buffer = virtual_address;
		ch->cyclestate.cyclestate_buffer_size = cyclestate_buffer_size;
		return 0;

	} else if (!args->nvmap_handle &&
			ch->cyclestate.cyclestate_buffer_handler) {
		gk20a_free_cycle_stats_buffer(ch);
		return 0;

	} else if (!args->nvmap_handle &&
			!ch->cyclestate.cyclestate_buffer_handler) {
		/* no requst from GL */
		return 0;

	} else {
		pr_err("channel already has cyclestats buffer\n");
		return -EINVAL;
	}
}
#endif

int gk20a_init_error_notifier(struct nvhost_hwctx *ctx,
		u32 memhandle, u64 offset) {
	struct channel_gk20a *ch = ctx->priv;
	struct platform_device *dev = ch->ch->dev;
	void *va;

	struct mem_mgr *memmgr;
	struct mem_handle *handle_ref;

	if (!memhandle) {
		pr_err("gk20a_init_error_notifier: invalid memory handle\n");
		return -EINVAL;
	}

	memmgr = gk20a_channel_mem_mgr(ch);
	handle_ref = nvhost_memmgr_get(memmgr, memhandle, dev);

	if (ctx->error_notifier_ref)
		gk20a_free_error_notifiers(ctx);

	if (IS_ERR(handle_ref)) {
		pr_err("Invalid handle: %d\n", memhandle);
		return -EINVAL;
	}
	/* map handle */
	va = nvhost_memmgr_mmap(handle_ref);
	if (!va) {
		nvhost_memmgr_put(memmgr, handle_ref);
		pr_err("Cannot map notifier handle\n");
		return -ENOMEM;
	}

	/* set hwctx notifiers pointer */
	ctx->error_notifier_ref = handle_ref;
	ctx->error_notifier = va + offset;
	ctx->error_notifier_va = va;
	memset(ctx->error_notifier, 0, sizeof(struct nvhost_notification));
	return 0;
}

void gk20a_set_error_notifier(struct nvhost_hwctx *ctx, __u32 error)
{
	if (ctx->error_notifier_ref) {
		struct timespec time_data;
		u64 nsec;
		getnstimeofday(&time_data);
		nsec = ((u64)time_data.tv_sec) * 1000000000u +
				(u64)time_data.tv_nsec;
		ctx->error_notifier->time_stamp.nanoseconds[0] =
				(u32)nsec;
		ctx->error_notifier->time_stamp.nanoseconds[1] =
				(u32)(nsec >> 32);
		ctx->error_notifier->info32 = error;
		ctx->error_notifier->status = 0xffff;
		nvhost_err(&ctx->channel->dev->dev,
				"error notifier set to %d\n", error);
	}
}

void gk20a_free_error_notifiers(struct nvhost_hwctx *ctx)
{
	if (ctx->error_notifier_ref) {
		struct channel_gk20a *ch = ctx->priv;
		struct mem_mgr *memmgr = gk20a_channel_mem_mgr(ch);
		nvhost_memmgr_munmap(ctx->error_notifier_ref,
				ctx->error_notifier_va);
		nvhost_memmgr_put(memmgr, ctx->error_notifier_ref);
		ctx->error_notifier_ref = 0;
	}
}

void gk20a_free_channel(struct nvhost_hwctx *ctx, bool finish)
{
	struct channel_gk20a *ch = ctx->priv;
	struct gk20a *g = ch->g;
	struct device *d = dev_from_gk20a(g);
	struct fifo_gk20a *f = &g->fifo;
	struct gr_gk20a *gr = &g->gr;
	struct vm_gk20a *ch_vm = ch->vm;
	unsigned long timeout = gk20a_get_gr_idle_timeout(g);
	struct dbg_session_gk20a *dbg_s;

	nvhost_dbg_fn("");

	/* if engine reset was deferred, perform it now */
	mutex_lock(&f->deferred_reset_mutex);
	if (g->fifo.deferred_reset_pending) {
		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "engine reset was"
			   " deferred, running now");
		fifo_gk20a_finish_mmu_fault_handling(g, g->fifo.mmu_fault_engines);
		g->fifo.mmu_fault_engines = 0;
		g->fifo.deferred_reset_pending = false;
	}
	mutex_unlock(&f->deferred_reset_mutex);

	if (!ch->bound)
		return;

	if (!gk20a_channel_as_bound(ch))
		goto unbind;

	nvhost_dbg_info("freeing bound channel context, timeout=%ld",
			timeout);

	gk20a_disable_channel(ch, finish && !ch->hwctx->has_timedout, timeout);

	gk20a_free_error_notifiers(ctx);

	/* release channel ctx */
	gk20a_free_channel_ctx(ch);

	gk20a_gr_flush_channel_tlb(gr);

	memset(&ch->ramfc, 0, sizeof(struct mem_desc_sub));

	/* free gpfifo */
	if (ch->gpfifo.gpu_va)
		gk20a_gmmu_unmap(ch_vm, ch->gpfifo.gpu_va,
			ch->gpfifo.size, mem_flag_none);
	if (ch->gpfifo.cpu_va)
		dma_free_coherent(d, ch->gpfifo.size,
			ch->gpfifo.cpu_va, ch->gpfifo.iova);
	ch->gpfifo.cpu_va = NULL;
	ch->gpfifo.iova = 0;

	gk20a_mm_l2_invalidate(ch->g);

	memset(&ch->gpfifo, 0, sizeof(struct gpfifo_desc));

#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
	gk20a_free_cycle_stats_buffer(ch);
#endif

	ctx->priv = NULL;
	channel_gk20a_free_priv_cmdbuf(ch);

	/* release hwctx binding to the as_share */
	nvhost_as_release_share(ch_vm->as_share, ctx);

unbind:
	channel_gk20a_unbind(ch);
	channel_gk20a_free_inst(g, ch);

	ch->vpr = false;

	/* unlink all debug sessions */
	mutex_lock(&ch->dbg_s_lock);

	list_for_each_entry(dbg_s, &ch->dbg_s_list, dbg_s_list_node) {
		dbg_s->ch = NULL;
		list_del_init(&dbg_s->dbg_s_list_node);
	}

	mutex_unlock(&ch->dbg_s_lock);

	/* ALWAYS last */
	release_used_channel(f, ch);
}

struct nvhost_hwctx *gk20a_open_channel(struct nvhost_channel *ch,
					 struct nvhost_hwctx *ctx)
{
	struct gk20a *g = get_gk20a(ch->dev);
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch_gk20a;

	ch_gk20a = acquire_unused_channel(f);
	if (ch_gk20a == NULL) {
		/* TBD: we want to make this virtualizable */
		nvhost_err(dev_from_gk20a(g), "out of hw chids");
		return 0;
	}

	ctx->priv = ch_gk20a;
	ch_gk20a->g = g;
	/* note the ch here is the same for *EVERY* gk20a channel */
	ch_gk20a->ch = ch;
	/* but thre's one hwctx per gk20a channel */
	ch_gk20a->hwctx = ctx;

	if (channel_gk20a_alloc_inst(g, ch_gk20a)) {
		ch_gk20a->in_use = false;
		ctx->priv = 0;
		nvhost_err(dev_from_gk20a(g),
			   "failed to open gk20a channel, out of inst mem");

		return 0;
	}
	channel_gk20a_bind(ch_gk20a);
	ch_gk20a->pid = current->pid;

	/* reset timeout counter and update timestamp */
	ch_gk20a->timeout_accumulated_ms = 0;
	ch_gk20a->timeout_gpfifo_get = 0;
	/* set gr host default timeout */
	ch_gk20a->hwctx->timeout_ms_max = gk20a_get_gr_idle_timeout(g);

	/* The channel is *not* runnable at this point. It still needs to have
	 * an address space bound and allocate a gpfifo and grctx. */

	init_waitqueue_head(&ch_gk20a->notifier_wq);
	init_waitqueue_head(&ch_gk20a->semaphore_wq);
	init_waitqueue_head(&ch_gk20a->submit_wq);

	return ctx;
}

#if 0
/* move to debug_gk20a.c ... */
static void dump_gpfifo(struct channel_gk20a *c)
{
	void *inst_ptr;
	u32 chid = c->hw_chid;

	nvhost_dbg_fn("");

	inst_ptr = nvhost_memmgr_mmap(c->inst_block.mem.ref);
	if (!inst_ptr)
		return;

	nvhost_dbg_info("ramfc for channel %d:\n"
		"ramfc: gp_base 0x%08x, gp_base_hi 0x%08x, "
		"gp_fetch 0x%08x, gp_get 0x%08x, gp_put 0x%08x, "
		"pb_fetch 0x%08x, pb_fetch_hi 0x%08x, "
		"pb_get 0x%08x, pb_get_hi 0x%08x, "
		"pb_put 0x%08x, pb_put_hi 0x%08x\n"
		"userd: gp_put 0x%08x, gp_get 0x%08x, "
		"get 0x%08x, get_hi 0x%08x, "
		"put 0x%08x, put_hi 0x%08x\n"
		"pbdma: status 0x%08x, channel 0x%08x, userd 0x%08x, "
		"gp_base 0x%08x, gp_base_hi 0x%08x, "
		"gp_fetch 0x%08x, gp_get 0x%08x, gp_put 0x%08x, "
		"pb_fetch 0x%08x, pb_fetch_hi 0x%08x, "
		"get 0x%08x, get_hi 0x%08x, put 0x%08x, put_hi 0x%08x\n"
		"channel: ccsr_channel 0x%08x",
		chid,
		mem_rd32(inst_ptr, ram_fc_gp_base_w()),
		mem_rd32(inst_ptr, ram_fc_gp_base_hi_w()),
		mem_rd32(inst_ptr, ram_fc_gp_fetch_w()),
		mem_rd32(inst_ptr, ram_fc_gp_get_w()),
		mem_rd32(inst_ptr, ram_fc_gp_put_w()),
		mem_rd32(inst_ptr, ram_fc_pb_fetch_w()),
		mem_rd32(inst_ptr, ram_fc_pb_fetch_hi_w()),
		mem_rd32(inst_ptr, ram_fc_pb_get_w()),
		mem_rd32(inst_ptr, ram_fc_pb_get_hi_w()),
		mem_rd32(inst_ptr, ram_fc_pb_put_w()),
		mem_rd32(inst_ptr, ram_fc_pb_put_hi_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_gp_put_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_gp_get_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_get_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_get_hi_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_put_w()),
		mem_rd32(c->userd_cpu_va, ram_userd_put_hi_w()),
		gk20a_readl(c->g, pbdma_status_r(0)),
		gk20a_readl(c->g, pbdma_channel_r(0)),
		gk20a_readl(c->g, pbdma_userd_r(0)),
		gk20a_readl(c->g, pbdma_gp_base_r(0)),
		gk20a_readl(c->g, pbdma_gp_base_hi_r(0)),
		gk20a_readl(c->g, pbdma_gp_fetch_r(0)),
		gk20a_readl(c->g, pbdma_gp_get_r(0)),
		gk20a_readl(c->g, pbdma_gp_put_r(0)),
		gk20a_readl(c->g, pbdma_pb_fetch_r(0)),
		gk20a_readl(c->g, pbdma_pb_fetch_hi_r(0)),
		gk20a_readl(c->g, pbdma_get_r(0)),
		gk20a_readl(c->g, pbdma_get_hi_r(0)),
		gk20a_readl(c->g, pbdma_put_r(0)),
		gk20a_readl(c->g, pbdma_put_hi_r(0)),
		gk20a_readl(c->g, ccsr_channel_r(chid)));

	nvhost_memmgr_munmap(c->inst_block.mem.ref, inst_ptr);
	gk20a_mm_l2_invalidate(c->g);
}
#endif

/* allocate private cmd buffer.
   used for inserting commands before/after user submitted buffers. */
static int channel_gk20a_alloc_priv_cmdbuf(struct channel_gk20a *c)
{
	struct device *d = dev_from_gk20a(c->g);
	struct vm_gk20a *ch_vm = c->vm;
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	struct priv_cmd_entry *e;
	u32 i = 0, size;
	int err = 0;
	struct sg_table *sgt;

	/* Kernel can insert gpfifos before and after user gpfifos.
	   Before user gpfifos, kernel inserts fence_wait, which takes
	   syncpoint_a (2 dwords) + syncpoint_b (2 dwords) = 4 dwords.
	   After user gpfifos, kernel inserts fence_get, which takes
	   wfi (2 dwords) + syncpoint_a (2 dwords) + syncpoint_b (2 dwords)
	   = 6 dwords.
	   Worse case if kernel adds both of them for every user gpfifo,
	   max size of priv_cmdbuf is :
	   (gpfifo entry number * (2 / 3) * (4 + 6) * 4 bytes */
	size = roundup_pow_of_two(
		c->gpfifo.entry_num * 2 * 10 * sizeof(u32) / 3);

	q->mem.base_cpuva = dma_alloc_coherent(d, size,
					&q->mem.base_iova,
					GFP_KERNEL);
	if (!q->mem.base_cpuva) {
		nvhost_err(d, "%s: memory allocation failed\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	q->mem.size = size;

	err = gk20a_get_sgtable(d, &sgt,
			q->mem.base_cpuva, q->mem.base_iova, size);
	if (err) {
		nvhost_err(d, "%s: failed to create sg table\n", __func__);
		goto clean_up;
	}

	memset(q->mem.base_cpuva, 0, size);

	q->base_gpuva = gk20a_gmmu_map(ch_vm, &sgt,
					size,
					0, /* flags */
					mem_flag_none);
	if (!q->base_gpuva) {
		nvhost_err(d, "ch %d : failed to map gpu va"
			   "for priv cmd buffer", c->hw_chid);
		err = -ENOMEM;
		goto clean_up_sgt;
	}

	q->size = q->mem.size / sizeof (u32);

	INIT_LIST_HEAD(&q->head);
	INIT_LIST_HEAD(&q->free);

	/* pre-alloc 25% of priv cmdbuf entries and put them on free list */
	for (i = 0; i < q->size / 4; i++) {
		e = kzalloc(sizeof(struct priv_cmd_entry), GFP_KERNEL);
		if (!e) {
			nvhost_err(d, "ch %d: fail to pre-alloc cmd entry",
				c->hw_chid);
			err = -ENOMEM;
			goto clean_up_sgt;
		}
		e->pre_alloc = true;
		list_add(&e->list, &q->free);
	}

	gk20a_free_sgtable(&sgt);

	return 0;

clean_up_sgt:
	gk20a_free_sgtable(&sgt);
clean_up:
	channel_gk20a_free_priv_cmdbuf(c);
	return err;
}

static void channel_gk20a_free_priv_cmdbuf(struct channel_gk20a *c)
{
	struct device *d = dev_from_gk20a(c->g);
	struct vm_gk20a *ch_vm = c->vm;
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	struct priv_cmd_entry *e;
	struct list_head *pos, *tmp, *head;

	if (q->size == 0)
		return;

	if (q->base_gpuva)
		gk20a_gmmu_unmap(ch_vm, q->base_gpuva,
				q->mem.size, mem_flag_none);
	if (q->mem.base_cpuva)
		dma_free_coherent(d, q->mem.size,
			q->mem.base_cpuva, q->mem.base_iova);
	q->mem.base_cpuva = NULL;
	q->mem.base_iova = 0;

	/* free used list */
	head = &q->head;
	list_for_each_safe(pos, tmp, head) {
		e = container_of(pos, struct priv_cmd_entry, list);
		free_priv_cmdbuf(c, e);
	}

	/* free free list */
	head = &q->free;
	list_for_each_safe(pos, tmp, head) {
		e = container_of(pos, struct priv_cmd_entry, list);
		e->pre_alloc = false;
		free_priv_cmdbuf(c, e);
	}

	memset(q, 0, sizeof(struct priv_cmd_queue));
}

/* allocate a cmd buffer with given size. size is number of u32 entries */
static int alloc_priv_cmdbuf(struct channel_gk20a *c, u32 orig_size,
			     struct priv_cmd_entry **entry)
{
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	struct priv_cmd_entry *e;
	struct list_head *node;
	u32 free_count;
	u32 size = orig_size;
	bool no_retry = false;

	nvhost_dbg_fn("size %d", orig_size);

	*entry = NULL;

	/* if free space in the end is less than requested, increase the size
	 * to make the real allocated space start from beginning. */
	if (q->put + size > q->size)
		size = orig_size + (q->size - q->put);

	nvhost_dbg_info("ch %d: priv cmd queue get:put %d:%d",
			c->hw_chid, q->get, q->put);

TRY_AGAIN:
	free_count = (q->size - (q->put - q->get) - 1) % q->size;

	if (size > free_count) {
		if (!no_retry) {
			recycle_priv_cmdbuf(c);
			no_retry = true;
			goto TRY_AGAIN;
		} else
			return -EAGAIN;
	}

	if (unlikely(list_empty(&q->free))) {

		nvhost_dbg_info("ch %d: run out of pre-alloc entries",
			c->hw_chid);

		e = kzalloc(sizeof(struct priv_cmd_entry), GFP_KERNEL);
		if (!e) {
			nvhost_err(dev_from_gk20a(c->g),
				"ch %d: fail to allocate priv cmd entry",
				c->hw_chid);
			return -ENOMEM;
		}
	} else  {
		node = q->free.next;
		list_del(node);
		e = container_of(node, struct priv_cmd_entry, list);
	}

	e->size = orig_size;
	e->gp_get = c->gpfifo.get;
	e->gp_put = c->gpfifo.put;
	e->gp_wrap = c->gpfifo.wrap;

	/* if we have increased size to skip free space in the end, set put
	   to beginning of cmd buffer (0) + size */
	if (size != orig_size) {
		e->ptr = q->mem.base_cpuva;
		e->gva = q->base_gpuva;
		q->put = orig_size;
	} else {
		e->ptr = q->mem.base_cpuva + q->put;
		e->gva = q->base_gpuva + q->put * sizeof(u32);
		q->put = (q->put + orig_size) & (q->size - 1);
	}

	/* we already handled q->put + size > q->size so BUG_ON this */
	BUG_ON(q->put > q->size);

	/* add new entry to head since we free from head */
	list_add(&e->list, &q->head);

	*entry = e;

	nvhost_dbg_fn("done");

	return 0;
}

/* Don't call this to free an explict cmd entry.
 * It doesn't update priv_cmd_queue get/put */
static void free_priv_cmdbuf(struct channel_gk20a *c,
			     struct priv_cmd_entry *e)
{
	struct priv_cmd_queue *q = &c->priv_cmd_q;

	if (!e)
		return;

	list_del(&e->list);

	if (unlikely(!e->pre_alloc))
		kfree(e);
	else {
		memset(e, 0, sizeof(struct priv_cmd_entry));
		e->pre_alloc = true;
		list_add(&e->list, &q->free);
	}
}

/* free entries if they're no longer being used */
static void recycle_priv_cmdbuf(struct channel_gk20a *c)
{
	struct priv_cmd_queue *q = &c->priv_cmd_q;
	struct priv_cmd_entry *e, *tmp;
	struct list_head *head = &q->head;
	bool wrap_around, found = false;

	nvhost_dbg_fn("");

	/* Find the most recent free entry. Free it and everything before it */
	list_for_each_entry(e, head, list) {

		nvhost_dbg_info("ch %d: cmd entry get:put:wrap %d:%d:%d "
			"curr get:put:wrap %d:%d:%d",
			c->hw_chid, e->gp_get, e->gp_put, e->gp_wrap,
			c->gpfifo.get, c->gpfifo.put, c->gpfifo.wrap);

		wrap_around = (c->gpfifo.wrap != e->gp_wrap);
		if (e->gp_get < e->gp_put) {
			if (c->gpfifo.get >= e->gp_put ||
			    wrap_around) {
				found = true;
				break;
			} else
				e->gp_get = c->gpfifo.get;
		} else if (e->gp_get > e->gp_put) {
			if (wrap_around &&
			    c->gpfifo.get >= e->gp_put) {
				found = true;
				break;
			} else
				e->gp_get = c->gpfifo.get;
		}
	}

	if (found)
		q->get = (e->ptr - q->mem.base_cpuva) + e->size;
	else {
		nvhost_dbg_info("no free entry recycled");
		return;
	}

	list_for_each_entry_safe_continue(e, tmp, head, list) {
		free_priv_cmdbuf(c, e);
	}

	nvhost_dbg_fn("done");
}


int gk20a_alloc_channel_gpfifo(struct channel_gk20a *c,
			       struct nvhost_alloc_gpfifo_args *args)
{
	struct gk20a *g = c->g;
	struct nvhost_device_data *pdata = nvhost_get_devdata(g->dev);
	struct device *d = dev_from_gk20a(g);
	struct vm_gk20a *ch_vm;
	u32 gpfifo_size;
	int err = 0;
	struct sg_table *sgt;

	/* Kernel can insert one extra gpfifo entry before user submitted gpfifos
	   and another one after, for internal usage. Triple the requested size. */
	gpfifo_size = roundup_pow_of_two(args->num_entries * 3);

	if (args->flags & NVHOST_ALLOC_GPFIFO_FLAGS_VPR_ENABLED)
		c->vpr = true;

	/* an address space needs to have been bound at this point.   */
	if (!gk20a_channel_as_bound(c)) {
		nvhost_err(d,
			    "not bound to an address space at time of gpfifo"
			    " allocation.  Attempting to create and bind to"
			    " one...");
		return -EINVAL;
	}
	ch_vm = c->vm;

	c->cmds_pending = false;

	c->last_submit_fence.valid        = false;
	c->last_submit_fence.syncpt_value = 0;
	c->last_submit_fence.syncpt_id    = c->hw_chid + pdata->syncpt_base;

	c->ramfc.offset = 0;
	c->ramfc.size = ram_in_ramfc_s() / 8;

	if (c->gpfifo.cpu_va) {
		nvhost_err(d, "channel %d :"
			   "gpfifo already allocated", c->hw_chid);
		return -EEXIST;
	}

	c->gpfifo.size = gpfifo_size * sizeof(struct gpfifo);
	c->gpfifo.cpu_va = (struct gpfifo *)dma_alloc_coherent(d,
						c->gpfifo.size,
						&c->gpfifo.iova,
						GFP_KERNEL);
	if (!c->gpfifo.cpu_va) {
		nvhost_err(d, "%s: memory allocation failed\n", __func__);
		err = -ENOMEM;
		goto clean_up;
	}

	c->gpfifo.entry_num = gpfifo_size;

	c->gpfifo.get = c->gpfifo.put = 0;

	err = gk20a_get_sgtable(d, &sgt,
			c->gpfifo.cpu_va, c->gpfifo.iova, c->gpfifo.size);
	if (err) {
		nvhost_err(d, "%s: failed to allocate sg table\n", __func__);
		goto clean_up;
	}

	c->gpfifo.gpu_va = gk20a_gmmu_map(ch_vm,
					&sgt,
					c->gpfifo.size,
					0, /* flags */
					mem_flag_none);
	if (!c->gpfifo.gpu_va) {
		nvhost_err(d, "channel %d : failed to map"
			   " gpu_va for gpfifo", c->hw_chid);
		err = -ENOMEM;
		goto clean_up_sgt;
	}

	nvhost_dbg_info("channel %d : gpfifo_base 0x%016llx, size %d",
		c->hw_chid, c->gpfifo.gpu_va, c->gpfifo.entry_num);

	channel_gk20a_setup_ramfc(c, c->gpfifo.gpu_va, c->gpfifo.entry_num);

	channel_gk20a_setup_userd(c);
	channel_gk20a_commit_userd(c);

	gk20a_mm_l2_invalidate(c->g);

	/* TBD: setup engine contexts */

	err = channel_gk20a_alloc_priv_cmdbuf(c);
	if (err)
		goto clean_up_unmap;

	err = channel_gk20a_update_runlist(c, true);
	if (err)
		goto clean_up_unmap;

	gk20a_free_sgtable(&sgt);

	nvhost_dbg_fn("done");
	return 0;

clean_up_unmap:
	gk20a_gmmu_unmap(ch_vm, c->gpfifo.gpu_va,
		c->gpfifo.size, mem_flag_none);
clean_up_sgt:
	gk20a_free_sgtable(&sgt);
clean_up:
	dma_free_coherent(d, c->gpfifo.size,
		c->gpfifo.cpu_va, c->gpfifo.iova);
	c->gpfifo.cpu_va = NULL;
	c->gpfifo.iova = 0;
	memset(&c->gpfifo, 0, sizeof(struct gpfifo_desc));
	nvhost_err(d, "fail");
	return err;
}

static inline int wfi_cmd_size(void)
{
	return 2;
}
void add_wfi_cmd(struct priv_cmd_entry *cmd, int *i)
{
	/* wfi */
	cmd->ptr[(*i)++] = 0x2001001E;
	/* handle, ignored */
	cmd->ptr[(*i)++] = 0x00000000;
}

static inline bool check_gp_put(struct gk20a *g,
				struct channel_gk20a *c)
{
	u32 put;
	/* gp_put changed unexpectedly since last update? */
	put = gk20a_bar1_readl(g,
	       c->userd_gpu_va + 4 * ram_userd_gp_put_w());
	if (c->gpfifo.put != put) {
		/*TBD: BUG_ON/teardown on this*/
		nvhost_err(dev_from_gk20a(g), "gp_put changed unexpectedly "
			   "since last update");
		c->gpfifo.put = put;
		return false; /* surprise! */
	}
	return true; /* checked out ok */
}

/* Update with this periodically to determine how the gpfifo is draining. */
static inline u32 update_gp_get(struct gk20a *g,
				struct channel_gk20a *c)
{
	u32 new_get = gk20a_bar1_readl(g,
		c->userd_gpu_va + sizeof(u32) * ram_userd_gp_get_w());
	if (new_get < c->gpfifo.get)
		c->gpfifo.wrap = !c->gpfifo.wrap;
	c->gpfifo.get = new_get;
	return new_get;
}

static inline u32 gp_free_count(struct channel_gk20a *c)
{
	return (c->gpfifo.entry_num - (c->gpfifo.put - c->gpfifo.get) - 1) %
		c->gpfifo.entry_num;
}

bool gk20a_channel_update_and_check_timeout(struct channel_gk20a *ch,
		u32 timeout_delta_ms)
{
	u32 gpfifo_get = update_gp_get(ch->g, ch);
	/* Count consequent timeout isr */
	if (gpfifo_get == ch->timeout_gpfifo_get) {
		/* we didn't advance since previous channel timeout check */
		ch->timeout_accumulated_ms += timeout_delta_ms;
	} else {
		/* first timeout isr encountered */
		ch->timeout_accumulated_ms = timeout_delta_ms;
	}

	ch->timeout_gpfifo_get = gpfifo_get;

	return ch->g->timeouts_enabled &&
		ch->timeout_accumulated_ms > ch->hwctx->timeout_ms_max;
}


/* Issue a syncpoint increment *preceded* by a wait-for-idle
 * command.  All commands on the channel will have been
 * consumed at the time the fence syncpoint increment occurs.
 */
int gk20a_channel_submit_wfi_fence(struct gk20a *g,
				   struct channel_gk20a *c,
				   struct nvhost_syncpt *sp,
				   struct nvhost_fence *fence)
{
	struct priv_cmd_entry *cmd = NULL;
	int cmd_size, j = 0;
	u32 free_count;
	int err;

	if (c->hwctx->has_timedout)
		return -ETIMEDOUT;

	cmd_size =  4 + wfi_cmd_size();

	update_gp_get(g, c);
	free_count = gp_free_count(c);
	if (unlikely(!free_count)) {
		nvhost_err(dev_from_gk20a(g),
			   "not enough gpfifo space");
		return -EAGAIN;
	}

	err = alloc_priv_cmdbuf(c, cmd_size, &cmd);
	if (unlikely(err)) {
		nvhost_err(dev_from_gk20a(g),
			   "not enough priv cmd buffer space");
		return err;
	}

	fence->value = nvhost_syncpt_incr_max(sp, fence->syncpt_id, 1);

	c->last_submit_fence.valid        = true;
	c->last_submit_fence.syncpt_value = fence->value;
	c->last_submit_fence.syncpt_id    = fence->syncpt_id;
	c->last_submit_fence.wfi          = true;

	trace_nvhost_ioctl_ctrl_syncpt_incr(fence->syncpt_id);

	add_wfi_cmd(cmd, &j);

	/* syncpoint_a */
	cmd->ptr[j++] = 0x2001001C;
	/* payload, ignored */
	cmd->ptr[j++] = 0;
	/* syncpoint_b */
	cmd->ptr[j++] = 0x2001001D;
	/* syncpt_id, incr */
	cmd->ptr[j++] = (fence->syncpt_id << 8) | 0x1;

	c->gpfifo.cpu_va[c->gpfifo.put].entry0 = u64_lo32(cmd->gva);
	c->gpfifo.cpu_va[c->gpfifo.put].entry1 = u64_hi32(cmd->gva) |
		pbdma_gp_entry1_length_f(cmd->size);

	c->gpfifo.put = (c->gpfifo.put + 1) & (c->gpfifo.entry_num - 1);

	/* save gp_put */
	cmd->gp_put = c->gpfifo.put;

	gk20a_bar1_writel(g,
		c->userd_gpu_va + 4 * ram_userd_gp_put_w(),
		c->gpfifo.put);

	nvhost_dbg_info("post-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	return 0;
}

static u32 get_gp_free_count(struct channel_gk20a *c)
{
	update_gp_get(c->g, c);
	return gp_free_count(c);
}

static void trace_write_pushbuffer(struct channel_gk20a *c, struct gpfifo *g)
{
	void *mem = NULL;
	unsigned int words;
	u64 offset;
	struct mem_handle *r = NULL;

	if (nvhost_debug_trace_cmdbuf) {
		u64 gpu_va = (u64)g->entry0 |
			(u64)((u64)pbdma_gp_entry1_get_hi_v(g->entry1) << 32);
		struct mem_mgr *memmgr = NULL;
		int err;

		words = pbdma_gp_entry1_length_v(g->entry1);
		err = gk20a_vm_find_buffer(c->vm, gpu_va, &memmgr, &r,
					   &offset);
		if (!err)
			mem = nvhost_memmgr_mmap(r);
	}

	if (mem) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += TRACE_MAX_LENGTH) {
			trace_nvhost_cdma_push_gather(
				c->ch->dev->name,
				0,
				min(words - i, TRACE_MAX_LENGTH),
				offset + i * sizeof(u32),
				mem);
		}
		nvhost_memmgr_munmap(r, mem);
	}
}

static int gk20a_channel_add_job(struct channel_gk20a *c,
				 struct nvhost_fence *fence)
{
	struct vm_gk20a *vm = c->vm;
	struct channel_gk20a_job *job = NULL;
	struct mapped_buffer_node **mapped_buffers = NULL;
	int err = 0, num_mapped_buffers;

	/* job needs reference to this vm */
	gk20a_vm_get(vm);

	err = gk20a_vm_get_buffers(vm, &mapped_buffers, &num_mapped_buffers);
	if (err) {
		gk20a_vm_put(vm);
		return err;
	}

	job = kzalloc(sizeof(*job), GFP_KERNEL);
	if (!job) {
		gk20a_vm_put_buffers(vm, mapped_buffers, num_mapped_buffers);
		gk20a_vm_put(vm);
		return -ENOMEM;
	}

	job->num_mapped_buffers = num_mapped_buffers;
	job->mapped_buffers = mapped_buffers;
	job->fence = *fence;

	mutex_lock(&c->jobs_lock);
	list_add_tail(&job->list, &c->jobs);
	mutex_unlock(&c->jobs_lock);

	return 0;
}

void gk20a_channel_update(struct channel_gk20a *c)
{
	struct gk20a *g = c->g;
	struct nvhost_syncpt *sp = syncpt_from_gk20a(g);
	struct vm_gk20a *vm = c->vm;
	struct channel_gk20a_job *job, *n;

	mutex_lock(&c->jobs_lock);
	list_for_each_entry_safe(job, n, &c->jobs, list) {
		bool completed = nvhost_syncpt_is_expired(sp,
			job->fence.syncpt_id, job->fence.value);
		if (!completed)
			break;

		gk20a_vm_put_buffers(vm, job->mapped_buffers,
				job->num_mapped_buffers);

		/* job is done. release its reference to vm */
		gk20a_vm_put(vm);

		list_del_init(&job->list);
		kfree(job);
		nvhost_module_idle(g->dev);
	}
	mutex_unlock(&c->jobs_lock);
}
#ifdef CONFIG_DEBUG_FS
static void gk20a_sync_debugfs(struct gk20a *g)
{
	u32 reg_f = ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f();
	spin_lock(&g->debugfs_lock);
	if (g->mm.ltc_enabled != g->mm.ltc_enabled_debug) {
		u32 reg = gk20a_readl(g, ltc_ltcs_ltss_tstg_set_mgmt_2_r());
		if (g->mm.ltc_enabled_debug)
			/* bypass disabled (normal caching ops)*/
			reg &= ~reg_f;
		else
			/* bypass enabled (no caching) */
			reg |= reg_f;

		gk20a_writel(g, ltc_ltcs_ltss_tstg_set_mgmt_2_r(), reg);
		g->mm.ltc_enabled = g->mm.ltc_enabled_debug;
	}
	spin_unlock(&g->debugfs_lock);
}
#endif

void add_wait_cmd(u32 *ptr, u32 id, u32 thresh)
{
	/* syncpoint_a */
	ptr[0] = 0x2001001C;
	/* payload */
	ptr[1] = thresh;
	/* syncpoint_b */
	ptr[2] = 0x2001001D;
	/* syncpt_id, switch_en, wait */
	ptr[3] = (id << 8) | 0x10;
}

int gk20a_submit_channel_gpfifo(struct channel_gk20a *c,
				struct nvhost_gpfifo *gpfifo,
				u32 num_entries,
				struct nvhost_fence *fence,
				u32 flags)
{
	struct gk20a *g = c->g;
	struct nvhost_device_data *pdata = nvhost_get_devdata(g->dev);
	struct device *d = dev_from_gk20a(g);
	struct nvhost_syncpt *sp = syncpt_from_gk20a(g);
	u32 i, incr_id = ~0, wait_id = ~0, wait_value = 0;
	u32 err = 0;
	int incr_cmd_size;
	bool wfi_cmd;
	int num_wait_cmds = 0;
	struct priv_cmd_entry *wait_cmd = NULL;
	struct priv_cmd_entry *incr_cmd = NULL;
	struct sync_fence *sync_fence = NULL;
	/* we might need two extra gpfifo entries - one for syncpoint
	 * wait and one for syncpoint increment */
	const int extra_entries = 2;

	if (c->hwctx->has_timedout)
		return -ETIMEDOUT;

	if ((flags & (NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT |
		      NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_GET)) &&
	    !fence)
		return -EINVAL;
#ifdef CONFIG_DEBUG_FS
	/* update debug settings */
	gk20a_sync_debugfs(g);
#endif

	nvhost_dbg_info("channel %d", c->hw_chid);

	nvhost_module_busy(g->dev);
	trace_nvhost_channel_submit_gpfifo(c->ch->dev->name,
					   c->hw_chid,
					   num_entries,
					   flags,
					   fence->syncpt_id, fence->value,
					   c->hw_chid + pdata->syncpt_base);
	check_gp_put(g, c);
	update_gp_get(g, c);

	nvhost_dbg_info("pre-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	/* If the caller has requested a fence "get" then we need to be
	 * sure the fence represents work completion.  In that case
	 * issue a wait-for-idle before the syncpoint increment.
	 */
	wfi_cmd = !!(flags & NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_GET)
		&& c->obj_class != KEPLER_C;

	/* Invalidate tlb if it's dirty...                                   */
	/* TBD: this should be done in the cmd stream, not with PRIs.        */
	/* We don't know what context is currently running...                */
	/* Note also: there can be more than one context associated with the */
	/* address space (vm).   */
	gk20a_mm_tlb_invalidate(c->vm);

	/* Make sure we have enough space for gpfifo entries. If not,
	 * wait for signals from completed submits */
	if (gp_free_count(c) < num_entries + extra_entries) {
		err = wait_event_interruptible(c->submit_wq,
			get_gp_free_count(c) >= num_entries + extra_entries ||
			c->hwctx->has_timedout);
	}

	if (c->hwctx->has_timedout) {
		err = -ETIMEDOUT;
		goto clean_up;
	}

	if (err) {
		nvhost_err(d, "not enough gpfifo space");
		err = -EAGAIN;
		goto clean_up;
	}


	if (flags & NVHOST_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE
			&& flags & NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT) {
		sync_fence = nvhost_sync_fdget(fence->syncpt_id);
		if (!sync_fence) {
			nvhost_err(d, "invalid fence fd");
			err = -EINVAL;
			goto clean_up;
		}
		num_wait_cmds = nvhost_sync_num_pts(sync_fence);
	}
	/*
	 * optionally insert syncpt wait in the beginning of gpfifo submission
	 * when user requested and the wait hasn't expired.
	 * validate that the id makes sense, elide if not
	 * the only reason this isn't being unceremoniously killed is to
	 * keep running some tests which trigger this condition
	 */
	else if (flags & NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT) {
		if (fence->syncpt_id >= nvhost_syncpt_nb_pts(sp))
			dev_warn(d,
				"invalid wait id in gpfifo submit, elided");
		if (!nvhost_syncpt_is_expired(sp,
					fence->syncpt_id, fence->value))
			num_wait_cmds = 1;
	}

	if (num_wait_cmds) {
		alloc_priv_cmdbuf(c, 4 * num_wait_cmds, &wait_cmd);
		if (wait_cmd == NULL) {
			nvhost_err(d, "not enough priv cmd buffer space");
			err = -EAGAIN;
			goto clean_up;
		}
	}

	/* always insert syncpt increment at end of gpfifo submission
	   to keep track of method completion for idle railgating */
	/* TODO: we need to find a way to get rid of these wfi on every
	 * submission...
	 */
	incr_cmd_size = 4;
	if (wfi_cmd)
		incr_cmd_size += wfi_cmd_size();
	alloc_priv_cmdbuf(c, incr_cmd_size, &incr_cmd);
	if (incr_cmd == NULL) {
		nvhost_err(d, "not enough priv cmd buffer space");
		err = -EAGAIN;
		goto clean_up;
	}

	if (num_wait_cmds) {
		if (sync_fence) {
			struct sync_pt *pos;
			struct nvhost_sync_pt *pt;
			i = 0;

			list_for_each_entry(pos, &sync_fence->pt_list_head,
					pt_list) {
				pt = to_nvhost_sync_pt(pos);

				wait_id = nvhost_sync_pt_id(pt);
				wait_value = nvhost_sync_pt_thresh(pt);

				add_wait_cmd(&wait_cmd->ptr[i * 4],
						wait_id, wait_value);

				i++;
			}
			sync_fence_put(sync_fence);
			sync_fence = NULL;
		} else {
				wait_id = fence->syncpt_id;
				wait_value = fence->value;
				add_wait_cmd(&wait_cmd->ptr[0],
						wait_id, wait_value);
		}

		c->gpfifo.cpu_va[c->gpfifo.put].entry0 =
			u64_lo32(wait_cmd->gva);
		c->gpfifo.cpu_va[c->gpfifo.put].entry1 =
			u64_hi32(wait_cmd->gva) |
			pbdma_gp_entry1_length_f(wait_cmd->size);
		trace_write_pushbuffer(c, &c->gpfifo.cpu_va[c->gpfifo.put]);

		c->gpfifo.put = (c->gpfifo.put + 1) &
			(c->gpfifo.entry_num - 1);

		/* save gp_put */
		wait_cmd->gp_put = c->gpfifo.put;
	}

	for (i = 0; i < num_entries; i++) {
		c->gpfifo.cpu_va[c->gpfifo.put].entry0 =
			gpfifo[i].entry0; /* cmd buf va low 32 */
		c->gpfifo.cpu_va[c->gpfifo.put].entry1 =
			gpfifo[i].entry1; /* cmd buf va high 32 | words << 10 */
		trace_write_pushbuffer(c, &c->gpfifo.cpu_va[c->gpfifo.put]);
		c->gpfifo.put = (c->gpfifo.put + 1) &
			(c->gpfifo.entry_num - 1);
	}

	if (incr_cmd) {
		int j = 0;
		incr_id = c->hw_chid + pdata->syncpt_base;
		fence->syncpt_id = incr_id;
		fence->value     = nvhost_syncpt_incr_max(sp, incr_id, 1);

		c->last_submit_fence.valid        = true;
		c->last_submit_fence.syncpt_value = fence->value;
		c->last_submit_fence.syncpt_id    = fence->syncpt_id;
		c->last_submit_fence.wfi          = wfi_cmd;

		trace_nvhost_ioctl_ctrl_syncpt_incr(fence->syncpt_id);
		if (c->obj_class == KEPLER_C) {
			/* setobject KEPLER_C */
			incr_cmd->ptr[j++] = 0x20010000;
			incr_cmd->ptr[j++] = KEPLER_C;
			/* syncpt incr */
			incr_cmd->ptr[j++] = 0x200100B2;
			incr_cmd->ptr[j++] = fence->syncpt_id | (0x1 << 20)
				| (0x1 << 16);
		} else {
			if (wfi_cmd)
				add_wfi_cmd(incr_cmd, &j);
			/* syncpoint_a */
			incr_cmd->ptr[j++] = 0x2001001C;
			/* payload, ignored */
			incr_cmd->ptr[j++] = 0;
			/* syncpoint_b */
			incr_cmd->ptr[j++] = 0x2001001D;
			/* syncpt_id, incr */
			incr_cmd->ptr[j++] = (fence->syncpt_id << 8) | 0x1;
		}

		c->gpfifo.cpu_va[c->gpfifo.put].entry0 =
			u64_lo32(incr_cmd->gva);
		c->gpfifo.cpu_va[c->gpfifo.put].entry1 =
			u64_hi32(incr_cmd->gva) |
			pbdma_gp_entry1_length_f(incr_cmd->size);
		trace_write_pushbuffer(c, &c->gpfifo.cpu_va[c->gpfifo.put]);

		c->gpfifo.put = (c->gpfifo.put + 1) &
			(c->gpfifo.entry_num - 1);

		/* save gp_put */
		incr_cmd->gp_put = c->gpfifo.put;

		if (flags & NVHOST_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE) {
			struct nvhost_ctrl_sync_fence_info pts;

			pts.id = fence->syncpt_id;
			pts.thresh = fence->value;

			fence->syncpt_id = 0;
			fence->value = 0;
			err = nvhost_sync_create_fence(sp, &pts, 1, "fence",
					&fence->syncpt_id);
		}
	}

	/* Invalidate tlb if it's dirty...                                   */
	/* TBD: this should be done in the cmd stream, not with PRIs.        */
	/* We don't know what context is currently running...                */
	/* Note also: there can be more than one context associated with the */
	/* address space (vm).   */
	gk20a_mm_tlb_invalidate(c->vm);

	trace_nvhost_channel_submitted_gpfifo(c->ch->dev->name,
					   c->hw_chid,
					   num_entries,
					   flags,
					   wait_id, wait_value,
					   fence->syncpt_id, fence->value);


	/* TODO! Check for errors... */
	gk20a_channel_add_job(c, fence);

	c->cmds_pending = true;
	gk20a_bar1_writel(g,
		c->userd_gpu_va + 4 * ram_userd_gp_put_w(),
		c->gpfifo.put);

	nvhost_dbg_info("post-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	nvhost_dbg_fn("done");
	return err;

clean_up:
	if (sync_fence)
		sync_fence_put(sync_fence);
	nvhost_err(d, "fail");
	free_priv_cmdbuf(c, wait_cmd);
	free_priv_cmdbuf(c, incr_cmd);
	nvhost_module_idle(g->dev);
	return err;
}

void gk20a_remove_channel_support(struct channel_gk20a *c)
{

}

int gk20a_init_channel_support(struct gk20a *g, u32 chid)
{
	struct channel_gk20a *c = g->fifo.channel+chid;
	c->g = g;
	c->in_use = false;
	c->hw_chid = chid;
	c->bound = false;
	c->remove_support = gk20a_remove_channel_support;
	mutex_init(&c->jobs_lock);
	INIT_LIST_HEAD(&c->jobs);
#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
	mutex_init(&c->cyclestate.cyclestate_buffer_mutex);
#endif
	INIT_LIST_HEAD(&c->dbg_s_list);
	mutex_init(&c->dbg_s_lock);

	return 0;
}

int gk20a_channel_init(struct nvhost_channel *ch,
		       struct nvhost_master *host, int index)
{
	return 0;
}

int gk20a_channel_alloc_obj(struct nvhost_channel *channel,
			u32 class_num,
			u32 *obj_id,
			u32 vaspace_share)
{
	nvhost_dbg_fn("");
	return 0;
}

int gk20a_channel_free_obj(struct nvhost_channel *channel, u32 obj_id)
{
	nvhost_dbg_fn("");
	return 0;
}

int gk20a_channel_finish(struct channel_gk20a *ch, unsigned long timeout)
{
	struct nvhost_syncpt *sp = syncpt_from_gk20a(ch->g);
	struct nvhost_device_data *pdata = nvhost_get_devdata(ch->g->dev);
	struct nvhost_fence fence;
	int err = 0;

	if (!ch->cmds_pending)
		return 0;

	/* Do not wait for a timedout channel */
	if (ch->hwctx && ch->hwctx->has_timedout)
		return -ETIMEDOUT;

	if (!(ch->last_submit_fence.valid && ch->last_submit_fence.wfi)) {
		nvhost_dbg_fn("issuing wfi, incr to finish the channel");
		fence.syncpt_id = ch->hw_chid + pdata->syncpt_base;
		err = gk20a_channel_submit_wfi_fence(ch->g, ch,
						     sp, &fence);
	}
	if (err)
		return err;

	BUG_ON(!(ch->last_submit_fence.valid && ch->last_submit_fence.wfi));

	nvhost_dbg_fn("waiting for channel to finish syncpt:%d val:%d",
		      ch->last_submit_fence.syncpt_id,
		      ch->last_submit_fence.syncpt_value);

	err = nvhost_syncpt_wait_timeout(sp,
					 ch->last_submit_fence.syncpt_id,
					 ch->last_submit_fence.syncpt_value,
					 timeout, &fence.value, NULL, false);
	if (WARN_ON(err))
		dev_warn(dev_from_gk20a(ch->g),
			 "timed out waiting for gk20a channel to finish");
	else
		ch->cmds_pending = false;

	return err;
}

static int gk20a_channel_wait_semaphore(struct channel_gk20a *ch,
					ulong id, u32 offset,
					u32 payload, long timeout)
{
	struct platform_device *pdev = ch->ch->dev;
	struct mem_mgr *memmgr = gk20a_channel_mem_mgr(ch);
	struct mem_handle *handle_ref;
	void *data;
	u32 *semaphore;
	int ret = 0;
	long remain;

	/* do not wait if channel has timed out */
	if (ch->hwctx->has_timedout)
		return -ETIMEDOUT;

	handle_ref = nvhost_memmgr_get(memmgr, id, pdev);
	if (IS_ERR(handle_ref)) {
		nvhost_err(&pdev->dev, "invalid notifier nvmap handle 0x%lx",
			   id);
		return -EINVAL;
	}

	data = nvhost_memmgr_kmap(handle_ref, offset >> PAGE_SHIFT);
	if (!data) {
		nvhost_err(&pdev->dev, "failed to map notifier memory");
		ret = -EINVAL;
		goto cleanup_put;
	}

	semaphore = data + (offset & ~PAGE_MASK);

	remain = wait_event_interruptible_timeout(
			ch->semaphore_wq,
			*semaphore == payload || ch->hwctx->has_timedout,
			timeout);

	if (remain == 0 && *semaphore != payload)
		ret = -ETIMEDOUT;
	else if (remain < 0)
		ret = remain;

	nvhost_memmgr_kunmap(handle_ref, offset >> PAGE_SHIFT, data);
cleanup_put:
	nvhost_memmgr_put(memmgr, handle_ref);
	return ret;
}

int gk20a_channel_wait(struct channel_gk20a *ch,
		       struct nvhost_wait_args *args)
{
	struct device *d = dev_from_gk20a(ch->g);
	struct platform_device *dev = ch->ch->dev;
	struct mem_mgr *memmgr = gk20a_channel_mem_mgr(ch);
	struct mem_handle *handle_ref;
	struct notification *notif;
	struct timespec tv;
	u64 jiffies;
	ulong id;
	u32 offset;
	unsigned long timeout;
	int remain, ret = 0;

	nvhost_dbg_fn("");

	if (ch->hwctx->has_timedout)
		return -ETIMEDOUT;

	if (args->timeout == NVHOST_NO_TIMEOUT)
		timeout = MAX_SCHEDULE_TIMEOUT;
	else
		timeout = (u32)msecs_to_jiffies(args->timeout);

	switch (args->type) {
	case NVHOST_WAIT_TYPE_NOTIFIER:
		id = args->condition.notifier.nvmap_handle;
		offset = args->condition.notifier.offset;

		handle_ref = nvhost_memmgr_get(memmgr, id, dev);
		if (IS_ERR(handle_ref)) {
			nvhost_err(d, "invalid notifier nvmap handle 0x%lx",
				   id);
			return -EINVAL;
		}

		notif = nvhost_memmgr_mmap(handle_ref);
		if (!notif) {
			nvhost_err(d, "failed to map notifier memory");
			return -ENOMEM;
		}

		notif = (struct notification *)((uintptr_t)notif + offset);

		/* user should set status pending before
		 * calling this ioctl */
		remain = wait_event_interruptible_timeout(
				ch->notifier_wq,
				notif->status == 0 || ch->hwctx->has_timedout,
				timeout);

		if (remain == 0 && notif->status != 0) {
			ret = -ETIMEDOUT;
			goto notif_clean_up;
		} else if (remain < 0) {
			ret = -EINTR;
			goto notif_clean_up;
		}

		/* TBD: fill in correct information */
		jiffies = get_jiffies_64();
		jiffies_to_timespec(jiffies, &tv);
		notif->timestamp.nanoseconds[0] = tv.tv_nsec;
		notif->timestamp.nanoseconds[1] = tv.tv_sec;
		notif->info32 = 0xDEADBEEF; /* should be object name */
		notif->info16 = ch->hw_chid; /* should be method offset */

notif_clean_up:
		nvhost_memmgr_munmap(handle_ref, notif);
		return ret;

	case NVHOST_WAIT_TYPE_SEMAPHORE:
		ret = gk20a_channel_wait_semaphore(ch,
				args->condition.semaphore.nvmap_handle,
				args->condition.semaphore.offset,
				args->condition.semaphore.payload,
				timeout);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int gk20a_channel_set_priority(struct channel_gk20a *ch,
		u32 priority)
{
	u32 timeslice_timeout;
	/* set priority of graphics channel */
	switch (priority) {
	case NVHOST_PRIORITY_LOW:
		/* 64 << 3 = 512us */
		timeslice_timeout = 64;
		break;
	case NVHOST_PRIORITY_MEDIUM:
		/* 128 << 3 = 1024us */
		timeslice_timeout = 128;
		break;
	case NVHOST_PRIORITY_HIGH:
		/* 255 << 3 = 2048us */
		timeslice_timeout = 255;
		break;
	default:
		pr_err("Unsupported priority");
		return -EINVAL;
	}
	channel_gk20a_set_schedule_params(ch,
			timeslice_timeout);
	return 0;
}

int gk20a_channel_zcull_bind(struct channel_gk20a *ch,
			    struct nvhost_zcull_bind_args *args)
{
	struct gk20a *g = ch->g;
	struct gr_gk20a *gr = &g->gr;

	nvhost_dbg_fn("");

	return gr_gk20a_bind_ctxsw_zcull(g, gr, ch,
				args->gpu_va, args->mode);
}

/* in this context the "channel" is the host1x channel which
 * maps to *all* gk20a channels */
int gk20a_channel_suspend(struct gk20a *g)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;
	struct nvhost_fence fence;
	struct nvhost_syncpt *sp = syncpt_from_gk20a(g);
	struct device *d = dev_from_gk20a(g);
	struct nvhost_device_data *pdata = nvhost_get_devdata(g->dev);
	int err;

	nvhost_dbg_fn("");

	/* idle the engine by submitting WFI on non-KEPLER_C channel */
	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *c = &f->channel[chid];
		if (c->in_use && c->obj_class != KEPLER_C) {
			fence.syncpt_id = chid + pdata->syncpt_base;
			err = gk20a_channel_submit_wfi_fence(g,
					c, sp, &fence);
			if (err) {
				nvhost_err(d, "cannot idle channel %d\n",
						chid);
				return err;
			}

			nvhost_syncpt_wait_timeout(sp,
					fence.syncpt_id, fence.value,
					500000,
					NULL, NULL,
					false);
			break;
		}
	}

	for (chid = 0; chid < f->num_channels; chid++) {
		if (f->channel[chid].in_use) {

			nvhost_dbg_info("suspend channel %d", chid);
			/* disable channel */
			gk20a_writel(g, ccsr_channel_r(chid),
				gk20a_readl(g, ccsr_channel_r(chid)) |
				ccsr_channel_enable_clr_true_f());
			/* preempt the channel */
			gk20a_fifo_preempt_channel(g, chid);

			channels_in_use = true;
		}
	}

	if (channels_in_use) {
		gk20a_fifo_update_runlist(g, 0, ~0, false, true);

		for (chid = 0; chid < f->num_channels; chid++) {
			if (f->channel[chid].in_use)
				channel_gk20a_unbind(&f->channel[chid]);
		}
	}

	nvhost_dbg_fn("done");
	return 0;
}

/* in this context the "channel" is the host1x channel which
 * maps to *all* gk20a channels */
int gk20a_channel_resume(struct gk20a *g)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;

	nvhost_dbg_fn("");

	for (chid = 0; chid < f->num_channels; chid++) {
		if (f->channel[chid].in_use) {
			nvhost_dbg_info("resume channel %d", chid);
			channel_gk20a_bind(&f->channel[chid]);
			channels_in_use = true;
		}
	}

	if (channels_in_use)
		gk20a_fifo_update_runlist(g, 0, ~0, true, true);

	nvhost_dbg_fn("done");
	return 0;
}

void gk20a_channel_semaphore_wakeup(struct gk20a *g)
{
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;

	nvhost_dbg_fn("");

	for (chid = 0; chid < f->num_channels; chid++) {
		struct channel_gk20a *c = g->fifo.channel+chid;
		if (c->in_use)
			wake_up_interruptible_all(&c->semaphore_wq);
	}
}
