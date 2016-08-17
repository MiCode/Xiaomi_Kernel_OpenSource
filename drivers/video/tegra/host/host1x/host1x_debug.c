/*
 * drivers/video/tegra/host/host1x/host1x_debug.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2011 NVIDIA Corporation
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
#include <linux/mm.h>
#include <linux/scatterlist.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_cdma.h"
#include "nvhost_channel.h"
#include "nvhost_job.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"

#define NVHOST_DEBUG_MAX_PAGE_OFFSET 102400

enum {
	NVHOST_DBG_STATE_CMD = 0,
	NVHOST_DBG_STATE_DATA = 1,
	NVHOST_DBG_STATE_GATHER = 2
};

static void do_show_channel_gather(struct output *o,
		phys_addr_t phys_addr,
		u32 words, struct nvhost_cdma *cdma,
		phys_addr_t pin_addr, u32 *map_addr)
{
	/* Map dmaget cursor to corresponding nvmap_handle */
	u32 offset;
	int state, i;

	offset = phys_addr - pin_addr;
	/*
	 * Sometimes we're given different hardware address to the same
	 * page - in these cases the offset will get an invalid number and
	 * we just have to bail out.
	 */
	if (offset > NVHOST_DEBUG_MAX_PAGE_OFFSET) {
		nvhost_debug_output(o, "[address mismatch]\n");
	} else {
		/* GATHER buffer starts always with commands */
		state = NVHOST_DBG_STATE_CMD;
		for (i = 0; i < words; i++)
			nvhost_debug_output(o,
					"%08x ", *(map_addr + offset/4 + i));
		nvhost_debug_output(o, "\n");
	}
}

static void show_channel_gathers(struct output *o, struct nvhost_cdma *cdma)
{
	struct nvhost_job *job =
		list_first_entry(&cdma->sync_queue, struct nvhost_job, list);
	int i;
	nvhost_debug_output(o, "\n%p: JOB, syncpt_id=%d, syncpt_val=%d,"
			" first_get=%08x, timeout=%d, ctx=%p,"
			" num_slots=%d, num_handles=%d\n",
			job,
			job->syncpt_id,
			job->syncpt_end,
			job->first_get,
			job->timeout,
			job->hwctx,
			job->num_slots,
			job->num_unpins);

	for (i = 0; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];
		u32 *mapped = mem_op().mmap(g->ref);
		if (!mapped) {
			nvhost_debug_output(o, "[could not mmap]\n");
			continue;
		}

		nvhost_debug_output(o,
			"    GATHER at %08x+%04x, %d words\n",
			g->mem_base, g->offset, g->words);

		do_show_channel_gather(o, g->mem_base + g->offset,
				g->words, cdma, g->mem_base, mapped);
		mem_op().munmap(g->ref, mapped);
	}
}

static void t20_debug_show_channel_cdma(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	struct nvhost_channel *channel = ch;
	struct nvhost_cdma *cdma = &channel->cdma;
	u32 dmaput, dmaget, dmactrl;
	u32 cbstat, cbread;
	u32 val, base, baseval;
	struct nvhost_device_data *pdata = platform_get_drvdata(channel->dev);

	dmaput = readl(channel->aperture + host1x_channel_dmaput_r());
	dmaget = readl(channel->aperture + host1x_channel_dmaget_r());
	dmactrl = readl(channel->aperture + host1x_channel_dmactrl_r());
	cbread = readl(m->sync_aperture + host1x_sync_cbread0_r() + 4 * chid);
	cbstat = readl(m->sync_aperture + host1x_sync_cbstat_0_r() + 4 * chid);

	nvhost_debug_output(o, "%d-%s (%d): ", chid,
			    channel->dev->name,
			    pdata->refcount);

	if (host1x_channel_dmactrl_dmastop_v(dmactrl)
		|| !channel->cdma.push_buffer.mapped) {
		nvhost_debug_output(o, "inactive\n\n");
		return;
	}

	switch (cbstat) {
	case 0x00010008:
		nvhost_debug_output(o, "waiting on syncpt %d val %d\n",
			cbread >> 24, cbread & 0xffffff);
		break;

	case 0x00010009:
		base = (cbread >> 16) & 0xff;
		baseval = readl(m->sync_aperture +
				host1x_sync_syncpt_base_0_r() + 4 * base);
		val = cbread & 0xffff;
		nvhost_debug_output(o, "waiting on syncpt %d val %d "
			  "(base %d = %d; offset = %d)\n",
			cbread >> 24, baseval + val,
			base, baseval, val);
		break;

	default:
		nvhost_debug_output(o,
				"active class %02x, offset %04x, val %08x\n",
				host1x_sync_cbstat_0_cbclass0_v(cbstat),
				host1x_sync_cbstat_0_cboffset0_v(cbstat),
				cbread);
		break;
	}

	nvhost_debug_output(o, "DMAPUT %08x, DMAGET %08x, DMACTL %08x\n",
		dmaput, dmaget, dmactrl);
	nvhost_debug_output(o, "CBREAD %08x, CBSTAT %08x\n", cbread, cbstat);

	show_channel_gathers(o, cdma);
	nvhost_debug_output(o, "\n");
}

static void t20_debug_show_channel_fifo(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	u32 val, rd_ptr, wr_ptr, start, end;
	struct nvhost_channel *channel = ch;

	nvhost_debug_output(o, "%d: fifo:\n", chid);

	val = readl(channel->aperture + host1x_channel_fifostat_r());
	nvhost_debug_output(o, "FIFOSTAT %08x\n", val);
	if (host1x_channel_fifostat_cfempty_v(val)) {
		nvhost_debug_output(o, "[empty]\n");
		return;
	}

	writel(0x0, m->sync_aperture + host1x_sync_cfpeek_ctrl_r());
	writel(host1x_sync_cfpeek_ctrl_cfpeek_ena_f(1)
			| host1x_sync_cfpeek_ctrl_cfpeek_channr_f(chid),
		m->sync_aperture + host1x_sync_cfpeek_ctrl_r());

	val = readl(m->sync_aperture + host1x_sync_cfpeek_ptrs_r());
	rd_ptr = host1x_sync_cfpeek_ptrs_cf_rd_ptr_v(val);
	wr_ptr = host1x_sync_cfpeek_ptrs_cf_wr_ptr_v(val);

	val = readl(m->sync_aperture + host1x_sync_cf0_setup_r() + 4 * chid);
	start = host1x_sync_cf0_setup_cf0_base_v(val);
	end = host1x_sync_cf0_setup_cf0_limit_v(val);

	do {
		writel(0x0, m->sync_aperture + host1x_sync_cfpeek_ctrl_r());
		writel(host1x_sync_cfpeek_ctrl_cfpeek_ena_f(1)
				| host1x_sync_cfpeek_ctrl_cfpeek_channr_f(chid)
				| host1x_sync_cfpeek_ctrl_cfpeek_addr_f(rd_ptr),
			m->sync_aperture + host1x_sync_cfpeek_ctrl_r());
		val = readl(m->sync_aperture + host1x_sync_cfpeek_read_r());

		nvhost_debug_output(o, "%08x ", val);

		if (rd_ptr == end)
			rd_ptr = start;
		else
			rd_ptr++;
	} while (rd_ptr != wr_ptr);

	nvhost_debug_output(o, "\n");

	writel(0x0, m->sync_aperture + host1x_sync_cfpeek_ctrl_r());
}

static void t20_debug_show_mlocks(struct nvhost_master *m, struct output *o)
{
	u32 __iomem *mlo_regs = m->sync_aperture +
		host1x_sync_mlock_owner_0_r();
	int i;

	nvhost_debug_output(o, "---- mlocks ----\n");
	for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++) {
		u32 owner = readl(mlo_regs + i);
		if (host1x_sync_mlock_owner_0_mlock_ch_owns_0_v(owner))
			nvhost_debug_output(o, "%d: locked by channel %d\n",
				i,
				host1x_sync_mlock_owner_0_mlock_owner_chid_0_f(
					owner));
		else if (host1x_sync_mlock_owner_0_mlock_cpu_owns_0_v(owner))
			nvhost_debug_output(o, "%d: locked by cpu\n", i);
		else
			nvhost_debug_output(o, "%d: unlocked\n", i);
	}
	nvhost_debug_output(o, "\n");
}

static const struct nvhost_debug_ops host1x_debug_ops = {
	.show_channel_cdma = t20_debug_show_channel_cdma,
	.show_channel_fifo = t20_debug_show_channel_fifo,
	.show_mlocks = t20_debug_show_mlocks,
};
