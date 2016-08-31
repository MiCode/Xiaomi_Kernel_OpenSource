/*
 * drivers/video/tegra/host/t20/debug_gk20a.c
 *
 * Copyright (C) 2011-2013 NVIDIA Corporation.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_memmgr.h"
#include "nvhost_cdma.h"
#include "nvhost_acm.h"

#include "gk20a.h"
#include "hw_ram_gk20a.h"
#include "hw_fifo_gk20a.h"
#include "hw_ccsr_gk20a.h"
#include "hw_pbdma_gk20a.h"

static const char * const ccsr_chan_status_str[] = {
	"idle",
	"pending",
	"pending_ctx_reload",
	"pending_acquire",
	"pending_acq_ctx_reload",
	"on_pbdma",
	"on_pbdma_and_eng",
	"on_eng",
	"on_eng_pending_acquire",
	"on_eng_pending",
	"on_pbdma_ctx_reload",
	"on_pbdma_and_eng_ctx_reload",
	"on_eng_ctx_reload",
	"on_eng_pending_ctx_reload",
	"on_eng_pending_acq_ctx_reload",
};

static const char * const chan_status_str[] = {
	"invalid",
	"valid",
	"chsw_load",
	"chsw_save",
	"chsw_switch",
};

static const char * const ctx_status_str[] = {
	"invalid",
	"valid",
	NULL,
	NULL,
	NULL,
	"ctxsw_load",
	"ctxsw_save",
	"ctxsw_switch",
};

static void gk20a_debug_show_channel(struct output *o,
		struct gk20a *g, struct channel_gk20a *ch)
{
	u32 channel = gk20a_readl(g, ccsr_channel_r(ch->hw_chid));
	u32 status = ccsr_channel_status_v(channel);
	void *inst_ptr;

	inst_ptr = ch->inst_block.cpuva;
	if (!inst_ptr)
		return;

	nvhost_debug_output(o, "%d-%s, pid %d: ", ch->hw_chid,
			ch->ch->dev->name,
			ch->pid);
	nvhost_debug_output(o, "%s in use %s %s\n",
			ccsr_channel_enable_v(channel) ? "" : "not",
			ccsr_chan_status_str[status],
			ccsr_channel_busy_v(channel) ? "busy" : "not busy");
	nvhost_debug_output(o, "TOP: %016llx PUT: %016llx GET: %016llx "
			"FETCH: %016llx\nHEADER: %08x COUNT: %08x\n"
			"SYNCPOINT %08x %08x SEMAPHORE %08x %08x %08x %08x\n",
		(u64)mem_rd32(inst_ptr, ram_fc_pb_top_level_get_w()) +
		((u64)mem_rd32(inst_ptr,
			ram_fc_pb_top_level_get_hi_w()) << 32ULL),
		(u64)mem_rd32(inst_ptr, ram_fc_pb_put_w()) +
		((u64)mem_rd32(inst_ptr, ram_fc_pb_put_hi_w()) << 32ULL),
		(u64)mem_rd32(inst_ptr, ram_fc_pb_get_w()) +
		((u64)mem_rd32(inst_ptr, ram_fc_pb_get_hi_w()) << 32ULL),
		(u64)mem_rd32(inst_ptr, ram_fc_pb_fetch_w()) +
		((u64)mem_rd32(inst_ptr, ram_fc_pb_fetch_hi_w()) << 32ULL),
		mem_rd32(inst_ptr, ram_fc_pb_header_w()),
		mem_rd32(inst_ptr, ram_fc_pb_count_w()),
		mem_rd32(inst_ptr, ram_fc_syncpointa_w()),
		mem_rd32(inst_ptr, ram_fc_syncpointb_w()),
		mem_rd32(inst_ptr, ram_fc_semaphorea_w()),
		mem_rd32(inst_ptr, ram_fc_semaphoreb_w()),
		mem_rd32(inst_ptr, ram_fc_semaphorec_w()),
		mem_rd32(inst_ptr, ram_fc_semaphored_w()));

	nvhost_debug_output(o, "\n");
}

void gk20a_debug_show_channel_cdma(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int _chid)
{
	struct gk20a *g = get_gk20a(ch->dev);
	struct fifo_gk20a *f = &g->fifo;
	u32 chid;
	int i;

	gk20a_busy(ch->dev);
	for (i = 0; i < fifo_pbdma_status__size_1_v(); i++) {
		u32 status = gk20a_readl(g, fifo_pbdma_status_r(i));
		u32 chan_status = fifo_pbdma_status_chan_status_v(status);

		nvhost_debug_output(o, "%s pbdma %d: ", ch->dev->name, i);
		nvhost_debug_output(o,
				"id: %d (%s), next_id: %d (%s) status: %s\n",
				fifo_pbdma_status_id_v(status),
				fifo_pbdma_status_id_type_v(status) ?
					"tsg" : "channel",
				fifo_pbdma_status_next_id_v(status),
				fifo_pbdma_status_next_id_type_v(status) ?
					"tsg" : "channel",
				chan_status_str[chan_status]);
		nvhost_debug_output(o, "PUT: %016llx GET: %016llx "
				"FETCH: %08x HEADER: %08x\n",
			(u64)gk20a_readl(g, pbdma_put_r(i)) +
			((u64)gk20a_readl(g, pbdma_put_hi_r(i)) << 32ULL),
			(u64)gk20a_readl(g, pbdma_get_r(i)) +
			((u64)gk20a_readl(g, pbdma_get_hi_r(i)) << 32ULL),
			gk20a_readl(g, pbdma_gp_fetch_r(i)),
			gk20a_readl(g, pbdma_pb_header_r(i)));
	}
	nvhost_debug_output(o, "\n");

	for (i = 0; i < fifo_engine_status__size_1_v(); i++) {
		u32 status = gk20a_readl(g, fifo_engine_status_r(i));
		u32 ctx_status = fifo_engine_status_ctx_status_v(status);

		nvhost_debug_output(o, "%s eng %d: ",
				ch->dev->name, i);
		nvhost_debug_output(o,
				"id: %d (%s), next_id: %d (%s), ctx: %s ",
				fifo_engine_status_id_v(status),
				fifo_engine_status_id_type_v(status) ?
					"tsg" : "channel",
				fifo_engine_status_next_id_v(status),
				fifo_engine_status_next_id_type_v(status) ?
					"tsg" : "channel",
				ctx_status_str[ctx_status]);

		if (fifo_engine_status_faulted_v(status))
			nvhost_debug_output(o, "faulted ");
		if (fifo_engine_status_engine_v(status))
			nvhost_debug_output(o, "busy ");
		nvhost_debug_output(o, "\n");
	}
	nvhost_debug_output(o, "\n");

	for (chid = 0; chid < f->num_channels; chid++) {
		if (f->channel[chid].in_use) {
			struct channel_gk20a *gpu_ch = &f->channel[chid];
			gk20a_debug_show_channel(o, g, gpu_ch);
		}
	}
	gk20a_idle(ch->dev);
}

void gk20a_debug_show_channel_fifo(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
}
