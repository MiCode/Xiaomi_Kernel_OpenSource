/*
 * drivers/video/tegra/host/gk20a/gr_gk20a.c
 *
 * GK20A Graphics
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

#include <linux/delay.h>	/* for udelay */
#include <linux/mm.h>		/* for totalram_pages */
#include <linux/scatterlist.h>
#include <linux/nvmap.h>
#include <linux/tegra-soc.h>
#include <linux/nvhost_dbg_gpu_ioctl.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>

#include "../dev.h"
#include "bus_client.h"

#include "gk20a.h"
#include "gr_ctx_gk20a.h"

#include "hw_ccsr_gk20a.h"
#include "hw_ctxsw_prog_gk20a.h"
#include "hw_fifo_gk20a.h"
#include "hw_gr_gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_ram_gk20a.h"
#include "hw_pri_ringmaster_gk20a.h"
#include "hw_pri_ringstation_sys_gk20a.h"
#include "hw_pri_ringstation_gpc_gk20a.h"
#include "hw_pri_ringstation_fbp_gk20a.h"
#include "hw_proj_gk20a.h"
#include "hw_top_gk20a.h"
#include "hw_ltc_gk20a.h"
#include "hw_fb_gk20a.h"
#include "hw_therm_gk20a.h"
#include "hw_pbdma_gk20a.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"
#include "gk20a_gating_reglist.h"
#include "gr_pri_gk20a.h"
#include "regops_gk20a.h"
#include "dbg_gpu_gk20a.h"

#define BLK_SIZE (256)

static int gr_gk20a_commit_inst(struct channel_gk20a *c, u64 gpu_va);
static int gr_gk20a_ctx_patch_write(struct gk20a *g, struct channel_ctx_gk20a *ch_ctx,
				    u32 addr, u32 data, bool patch);

/* global ctx buffer */
static int  gr_gk20a_alloc_global_ctx_buffers(struct gk20a *g);
static void gr_gk20a_free_global_ctx_buffers(struct gk20a *g);
static int  gr_gk20a_map_global_ctx_buffers(struct gk20a *g,
					    struct channel_gk20a *c);
static void gr_gk20a_unmap_global_ctx_buffers(struct channel_gk20a *c);

/* channel gr ctx buffer */
static int  gr_gk20a_alloc_channel_gr_ctx(struct gk20a *g,
					struct channel_gk20a *c);
static void gr_gk20a_free_channel_gr_ctx(struct channel_gk20a *c);

/* channel patch ctx buffer */
static int  gr_gk20a_alloc_channel_patch_ctx(struct gk20a *g,
					struct channel_gk20a *c);
static void gr_gk20a_free_channel_patch_ctx(struct channel_gk20a *c);

/* golden ctx image */
static int gr_gk20a_init_golden_ctx_image(struct gk20a *g,
					  struct channel_gk20a *c);
static int gr_gk20a_load_golden_ctx_image(struct gk20a *g,
					  struct channel_gk20a *c);

void gk20a_fecs_dump_falcon_stats(struct gk20a *g)
{
	int i;

	nvhost_err(dev_from_gk20a(g), "gr_fecs_os_r : %d",
		gk20a_readl(g, gr_fecs_os_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_cpuctl_r : 0x%x",
		gk20a_readl(g, gr_fecs_cpuctl_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_idlestate_r : 0x%x",
		gk20a_readl(g, gr_fecs_idlestate_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_mailbox0_r : 0x%x",
		gk20a_readl(g, gr_fecs_mailbox0_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_mailbox1_r : 0x%x",
		gk20a_readl(g, gr_fecs_mailbox1_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_irqstat_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqstat_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_irqmode_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqmode_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_irqmask_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqmask_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_irqdest_r : 0x%x",
		gk20a_readl(g, gr_fecs_irqdest_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_debug1_r : 0x%x",
		gk20a_readl(g, gr_fecs_debug1_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_debuginfo_r : 0x%x",
		gk20a_readl(g, gr_fecs_debuginfo_r()));

	for (i = 0; i < gr_fecs_ctxsw_mailbox__size_1_v(); i++)
		nvhost_err(dev_from_gk20a(g), "gr_fecs_ctxsw_mailbox_r(%d) : 0x%x",
			i, gk20a_readl(g, gr_fecs_ctxsw_mailbox_r(i)));

	nvhost_err(dev_from_gk20a(g), "gr_fecs_engctl_r : 0x%x",
		gk20a_readl(g, gr_fecs_engctl_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_curctx_r : 0x%x",
		gk20a_readl(g, gr_fecs_curctx_r()));
	nvhost_err(dev_from_gk20a(g), "gr_fecs_nxtctx_r : 0x%x",
		gk20a_readl(g, gr_fecs_nxtctx_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_IMB));
	nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_IMB : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_DMB));
	nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_DMB : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_CSW));
	nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_CSW : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_CTX));
	nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_CTX : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	gk20a_writel(g, gr_fecs_icd_cmd_r(),
		gr_fecs_icd_cmd_opc_rreg_f() |
		gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_EXCI));
	nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_EXCI : 0x%x",
		gk20a_readl(g, gr_fecs_icd_rdata_r()));

	for (i = 0; i < 4; i++) {
		gk20a_writel(g, gr_fecs_icd_cmd_r(),
			gr_fecs_icd_cmd_opc_rreg_f() |
			gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_PC));
		nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_PC : 0x%x",
			gk20a_readl(g, gr_fecs_icd_rdata_r()));

		gk20a_writel(g, gr_fecs_icd_cmd_r(),
			gr_fecs_icd_cmd_opc_rreg_f() |
			gr_fecs_icd_cmd_idx_f(PMU_FALCON_REG_SP));
		nvhost_err(dev_from_gk20a(g), "FECS_FALCON_REG_SP : 0x%x",
			gk20a_readl(g, gr_fecs_icd_rdata_r()));
	}
}

static void gr_gk20a_load_falcon_dmem(struct gk20a *g)
{
	u32 i, ucode_u32_size;
	const u32 *ucode_u32_data;
	u32 checksum;

	nvhost_dbg_fn("");

	gk20a_writel(g, gr_gpccs_dmemc_r(0), (gr_gpccs_dmemc_offs_f(0) |
					      gr_gpccs_dmemc_blk_f(0)  |
					      gr_gpccs_dmemc_aincw_f(1)));

	ucode_u32_size = g->gr.ctx_vars.ucode.gpccs.data.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.gpccs.data.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		gk20a_writel(g, gr_gpccs_dmemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	gk20a_writel(g, gr_fecs_dmemc_r(0), (gr_fecs_dmemc_offs_f(0) |
					     gr_fecs_dmemc_blk_f(0)  |
					     gr_fecs_dmemc_aincw_f(1)));

	ucode_u32_size = g->gr.ctx_vars.ucode.fecs.data.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.fecs.data.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		gk20a_writel(g, gr_fecs_dmemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}
	nvhost_dbg_fn("done");
}

static void gr_gk20a_load_falcon_imem(struct gk20a *g)
{
	u32 cfg, fecs_imem_size, gpccs_imem_size, ucode_u32_size;
	const u32 *ucode_u32_data;
	u32 tag, i, pad_start, pad_end;
	u32 checksum;

	nvhost_dbg_fn("");

	cfg = gk20a_readl(g, gr_fecs_cfg_r());
	fecs_imem_size = gr_fecs_cfg_imem_sz_v(cfg);

	cfg = gk20a_readl(g, gr_gpc0_cfg_r());
	gpccs_imem_size = gr_gpc0_cfg_imem_sz_v(cfg);

	/* Use the broadcast address to access all of the GPCCS units. */
	gk20a_writel(g, gr_gpccs_imemc_r(0), (gr_gpccs_imemc_offs_f(0) |
					      gr_gpccs_imemc_blk_f(0) |
					      gr_gpccs_imemc_aincw_f(1)));

	/* Setup the tags for the instruction memory. */
	tag = 0;
	gk20a_writel(g, gr_gpccs_imemt_r(0), gr_gpccs_imemt_tag_f(tag));

	ucode_u32_size = g->gr.ctx_vars.ucode.gpccs.inst.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.gpccs.inst.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		if (i && ((i % (256/sizeof(u32))) == 0)) {
			tag++;
			gk20a_writel(g, gr_gpccs_imemt_r(0),
				      gr_gpccs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_gpccs_imemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	pad_start = i*4;
	pad_end = pad_start+(256-pad_start%256)+256;
	for (i = pad_start;
	     (i < gpccs_imem_size * 256) && (i < pad_end);
	     i += 4) {
		if (i && ((i % 256) == 0)) {
			tag++;
			gk20a_writel(g, gr_gpccs_imemt_r(0),
				      gr_gpccs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_gpccs_imemd_r(0), 0);
	}

	gk20a_writel(g, gr_fecs_imemc_r(0), (gr_fecs_imemc_offs_f(0) |
					     gr_fecs_imemc_blk_f(0) |
					     gr_fecs_imemc_aincw_f(1)));

	/* Setup the tags for the instruction memory. */
	tag = 0;
	gk20a_writel(g, gr_fecs_imemt_r(0), gr_fecs_imemt_tag_f(tag));

	ucode_u32_size = g->gr.ctx_vars.ucode.fecs.inst.count;
	ucode_u32_data = (const u32 *)g->gr.ctx_vars.ucode.fecs.inst.l;

	for (i = 0, checksum = 0; i < ucode_u32_size; i++) {
		if (i && ((i % (256/sizeof(u32))) == 0)) {
			tag++;
			gk20a_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_fecs_imemd_r(0), ucode_u32_data[i]);
		checksum += ucode_u32_data[i];
	}

	pad_start = i*4;
	pad_end = pad_start+(256-pad_start%256)+256;
	for (i = pad_start; (i < fecs_imem_size * 256) && i < pad_end; i += 4) {
		if (i && ((i % 256) == 0)) {
			tag++;
			gk20a_writel(g, gr_fecs_imemt_r(0),
				      gr_fecs_imemt_tag_f(tag));
		}
		gk20a_writel(g, gr_fecs_imemd_r(0), 0);
	}
}

static int gr_gk20a_wait_idle(struct gk20a *g, unsigned long end_jiffies,
		u32 expect_delay)
{
	u32 delay = expect_delay;
	bool gr_enabled;
	bool ctxsw_active;
	bool gr_busy;

	nvhost_dbg_fn("");

	do {
		/* fmodel: host gets fifo_engine_status(gr) from gr
		   only when gr_status is read */
		gk20a_readl(g, gr_status_r());

		gr_enabled = gk20a_readl(g, mc_enable_r()) &
			mc_enable_pgraph_enabled_f();

		ctxsw_active = gk20a_readl(g,
			fifo_engine_status_r(ENGINE_GR_GK20A)) &
			fifo_engine_status_ctxsw_in_progress_f();

		gr_busy = gk20a_readl(g, gr_engine_status_r()) &
			gr_engine_status_value_busy_f();

		if (!gr_enabled || (!gr_busy && !ctxsw_active)) {
			nvhost_dbg_fn("done");
			return 0;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies));

	nvhost_err(dev_from_gk20a(g),
		"timeout, ctxsw busy : %d, gr busy : %d",
		ctxsw_active, gr_busy);

	return -EAGAIN;
}

static int gr_gk20a_ctx_reset(struct gk20a *g, u32 rst_mask)
{
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 reg;

	nvhost_dbg_fn("");

	/* Force clocks on */
	gk20a_writel(g, gr_fe_pwr_mode_r(),
		     gr_fe_pwr_mode_req_send_f() |
		     gr_fe_pwr_mode_mode_force_on_f());

	/* Wait for the clocks to indicate that they are on */
	do {
		reg = gk20a_readl(g, gr_fe_pwr_mode_r());

		if (gr_fe_pwr_mode_req_v(reg) == gr_fe_pwr_mode_req_done_v())
			break;

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies));

	if (!time_before(jiffies, end_jiffies)) {
		nvhost_err(dev_from_gk20a(g),
			   "failed to force the clocks on\n");
		WARN_ON(1);
	}

	if (rst_mask) {
		gk20a_writel(g, gr_fecs_ctxsw_reset_ctl_r(), rst_mask);
	} else {
		gk20a_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			     gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			     gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			     gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f()  |
			     gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			     gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
			     gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f()  |
			     gr_fecs_ctxsw_reset_ctl_sys_context_reset_enabled_f() |
			     gr_fecs_ctxsw_reset_ctl_gpc_context_reset_enabled_f() |
			     gr_fecs_ctxsw_reset_ctl_be_context_reset_enabled_f());
	}

	/* we need to read the reset register *and* wait for a moment to ensure
	 * reset propagation */

	gk20a_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	udelay(20);

	gk20a_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
		     gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f()  |
		     gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f()  |
		     gr_fecs_ctxsw_reset_ctl_sys_context_reset_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_gpc_context_reset_disabled_f() |
		     gr_fecs_ctxsw_reset_ctl_be_context_reset_disabled_f());

	/* we need to readl the reset and then wait a small moment after that */
	gk20a_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	udelay(20);

	/* Set power mode back to auto */
	gk20a_writel(g, gr_fe_pwr_mode_r(),
		     gr_fe_pwr_mode_req_send_f() |
		     gr_fe_pwr_mode_mode_auto_f());

	/* Wait for the request to complete */
	end_jiffies = jiffies + msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	do {
		reg = gk20a_readl(g, gr_fe_pwr_mode_r());

		if (gr_fe_pwr_mode_req_v(reg) == gr_fe_pwr_mode_req_done_v())
			break;

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies));

	if (!time_before(jiffies, end_jiffies)) {
		nvhost_err(dev_from_gk20a(g),
			   "failed to set power mode to auto\n");
		WARN_ON(1);
	}

	return 0;
}

static int gr_gk20a_ctx_wait_ucode(struct gk20a *g, u32 mailbox_id,
				   u32 *mailbox_ret, u32 opc_success,
				   u32 mailbox_ok, u32 opc_fail,
				   u32 mailbox_fail)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	u32 check = WAIT_UCODE_LOOP;
	u32 reg;

	nvhost_dbg_fn("");

	while (check == WAIT_UCODE_LOOP) {
		if (!time_before(jiffies, end_jiffies) &&
				tegra_platform_is_silicon())
			check = WAIT_UCODE_TIMEOUT;

		reg = gk20a_readl(g, gr_fecs_ctxsw_mailbox_r(mailbox_id));

		if (mailbox_ret)
			*mailbox_ret = reg;

		switch (opc_success) {
		case GR_IS_UCODE_OP_EQUAL:
			if (reg == mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_NOT_EQUAL:
			if (reg != mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_AND:
			if (reg & mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_LESSER:
			if (reg < mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_LESSER_EQUAL:
			if (reg <= mailbox_ok)
				check = WAIT_UCODE_OK;
			break;
		case GR_IS_UCODE_OP_SKIP:
			/* do no success check */
			break;
		default:
			nvhost_err(dev_from_gk20a(g),
				   "invalid success opcode 0x%x", opc_success);

			check = WAIT_UCODE_ERROR;
			break;
		}

		switch (opc_fail) {
		case GR_IS_UCODE_OP_EQUAL:
			if (reg == mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_NOT_EQUAL:
			if (reg != mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_AND:
			if (reg & mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_LESSER:
			if (reg < mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_LESSER_EQUAL:
			if (reg <= mailbox_fail)
				check = WAIT_UCODE_ERROR;
			break;
		case GR_IS_UCODE_OP_SKIP:
			/* do no check on fail*/
			break;
		default:
			nvhost_err(dev_from_gk20a(g),
				   "invalid fail opcode 0x%x", opc_fail);
			check = WAIT_UCODE_ERROR;
			break;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);
	}

	if (check == WAIT_UCODE_TIMEOUT) {
		nvhost_err(dev_from_gk20a(g),
			   "timeout waiting on ucode response");
		gk20a_fecs_dump_falcon_stats(g);
		return -1;
	} else if (check == WAIT_UCODE_ERROR) {
		nvhost_err(dev_from_gk20a(g),
			   "ucode method failed on mailbox=%d value=0x%08x",
			   mailbox_id, reg);
		gk20a_fecs_dump_falcon_stats(g);
		return -1;
	}

	nvhost_dbg_fn("done");
	return 0;
}

/* The following is a less brittle way to call gr_gk20a_submit_fecs_method(...)
 * We should replace most, if not all, fecs method calls to this instead. */
struct fecs_method_op_gk20a {
	struct {
		u32 addr;
		u32 data;
	} method;

	struct {
		u32 id;
		u32 data;
		u32 clr;
		u32 *ret;
		u32 ok;
		u32 fail;
	} mailbox;

	struct {
		u32 ok;
		u32 fail;
	} cond;

};

int gr_gk20a_submit_fecs_method_op(struct gk20a *g,
				   struct fecs_method_op_gk20a op)
{
	struct gr_gk20a *gr = &g->gr;
	int ret;

	mutex_lock(&gr->fecs_mutex);

	if (op.mailbox.id != 0)
		gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(op.mailbox.id),
			     op.mailbox.data);

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0),
		gr_fecs_ctxsw_mailbox_clear_value_f(op.mailbox.clr));

	gk20a_writel(g, gr_fecs_method_data_r(), op.method.data);
	gk20a_writel(g, gr_fecs_method_push_r(),
		gr_fecs_method_push_adr_f(op.method.addr));

	/* op.mb.id == 4 cases require waiting for completion on
	 * for op.mb.id == 0 */
	if (op.mailbox.id == 4)
		op.mailbox.id = 0;

	ret = gr_gk20a_ctx_wait_ucode(g, op.mailbox.id, op.mailbox.ret,
				      op.cond.ok, op.mailbox.ok,
				      op.cond.fail, op.mailbox.fail);

	mutex_unlock(&gr->fecs_mutex);

	return ret;
}

int gr_gk20a_ctrl_ctxsw(struct gk20a *g, u32 fecs_method, u32 *ret)
{
	return gr_gk20a_submit_fecs_method_op(g,
	      (struct fecs_method_op_gk20a) {
		      .method.addr = fecs_method,
		      .method.data = ~0,
		      .mailbox = { .id   = 1, /*sideband?*/
				   .data = ~0, .clr = ~0, .ret = ret,
				   .ok   = gr_fecs_ctxsw_mailbox_value_pass_v(),
				   .fail = gr_fecs_ctxsw_mailbox_value_fail_v(), },
		      .cond.ok = GR_IS_UCODE_OP_EQUAL,
		      .cond.fail = GR_IS_UCODE_OP_EQUAL });
}

/* Stop processing (stall) context switches at FECS.
 * The caller must hold the dbg_sessions_lock, else if mutliple stop methods
 * are sent to the ucode in sequence, it can get into an undefined state. */
int gr_gk20a_disable_ctxsw(struct gk20a *g)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");
	return gr_gk20a_ctrl_ctxsw(g, gr_fecs_method_push_adr_stop_ctxsw_v(), 0);
}

/* Start processing (continue) context switches at FECS */
int gr_gk20a_enable_ctxsw(struct gk20a *g)
{
	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");
	return gr_gk20a_ctrl_ctxsw(g, gr_fecs_method_push_adr_start_ctxsw_v(), 0);
}


static int gr_gk20a_commit_inst(struct channel_gk20a *c, u64 gpu_va)
{
	u32 addr_lo;
	u32 addr_hi;
	void *inst_ptr = NULL;

	nvhost_dbg_fn("");

	/* flush gpu_va before commit */
	gk20a_mm_fb_flush(c->g);
	gk20a_mm_l2_flush(c->g, true);

	inst_ptr = c->inst_block.cpuva;
	if (!inst_ptr)
		return -ENOMEM;

	addr_lo = u64_lo32(gpu_va) >> 12;
	addr_hi = u64_hi32(gpu_va);

	mem_wr32(inst_ptr, ram_in_gr_wfi_target_w(),
		 ram_in_gr_cs_wfi_f() | ram_in_gr_wfi_mode_virtual_f() |
		 ram_in_gr_wfi_ptr_lo_f(addr_lo));

	mem_wr32(inst_ptr, ram_in_gr_wfi_ptr_hi_w(),
		 ram_in_gr_wfi_ptr_hi_f(addr_hi));

	gk20a_mm_l2_invalidate(c->g);

	return 0;
}

/*
 * Context state can be written directly or "patched" at times.
 * So that code can be used in either situation it is written
 * using a series _ctx_patch_write(..., patch) statements.
 * However any necessary cpu map/unmap and gpu l2 invalidates
 * should be minimized (to avoid doing it once per patch write).
 * Before a sequence of these set up with "_ctx_patch_write_begin"
 * and close with "_ctx_patch_write_end."
 */
static int gr_gk20a_ctx_patch_write_begin(struct gk20a *g,
					  struct channel_ctx_gk20a *ch_ctx)
{
	/* being defensive still... */
	if (ch_ctx->patch_ctx.cpu_va) {
		nvhost_err(dev_from_gk20a(g), "nested ctx patch begin?");
		return -EBUSY;
	}

	ch_ctx->patch_ctx.cpu_va = vmap(ch_ctx->patch_ctx.pages,
			PAGE_ALIGN(ch_ctx->patch_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));

	if (!ch_ctx->patch_ctx.cpu_va)
		return -ENOMEM;

	return 0;
}

static int gr_gk20a_ctx_patch_write_end(struct gk20a *g,
					struct channel_ctx_gk20a *ch_ctx)
{
	/* being defensive still... */
	if (!ch_ctx->patch_ctx.cpu_va) {
		nvhost_err(dev_from_gk20a(g), "dangling ctx patch end?");
		return -EINVAL;
	}

	vunmap(ch_ctx->patch_ctx.cpu_va);
	ch_ctx->patch_ctx.cpu_va = NULL;

	gk20a_mm_l2_invalidate(g);
	return 0;
}

static int gr_gk20a_ctx_patch_write(struct gk20a *g,
				    struct channel_ctx_gk20a *ch_ctx,
				    u32 addr, u32 data, bool patch)
{
	u32 patch_slot = 0;
	void *patch_ptr = NULL;
	bool mapped_here = false;

	BUG_ON(patch != 0 && ch_ctx == NULL);

	if (patch) {
		if (!ch_ctx)
			return -EINVAL;
		/* we added an optimization prolog, epilog
		 * to get rid of unnecessary maps and l2 invals.
		 * but be defensive still... */
		if (!ch_ctx->patch_ctx.cpu_va) {
			int err;
			nvhost_err(dev_from_gk20a(g),
				   "per-write ctx patch begin?");
			/* yes, gr_gk20a_ctx_patch_smpc causes this one */
			err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
			if (err)
				return err;
			mapped_here = true;
		} else
			mapped_here = false;

		patch_ptr = ch_ctx->patch_ctx.cpu_va;
		patch_slot = ch_ctx->patch_ctx.data_count * 2;

		mem_wr32(patch_ptr, patch_slot++, addr);
		mem_wr32(patch_ptr, patch_slot++, data);

		ch_ctx->patch_ctx.data_count++;

		if (mapped_here)
			gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	} else
		gk20a_writel(g, addr, data);

	return 0;
}

static int gr_gk20a_fecs_ctx_bind_channel(struct gk20a *g,
					struct channel_gk20a *c)
{
	u32 inst_base_ptr = u64_lo32(c->inst_block.cpu_pa
				     >> ram_in_base_shift_v());
	u32 ret;

	nvhost_dbg_info("bind channel %d inst ptr 0x%08x",
		   c->hw_chid, inst_base_ptr);

	ret = gr_gk20a_submit_fecs_method_op(g,
		     (struct fecs_method_op_gk20a) {
		     .method.addr = gr_fecs_method_push_adr_bind_pointer_v(),
		     .method.data = (gr_fecs_current_ctx_ptr_f(inst_base_ptr) |
				     gr_fecs_current_ctx_target_vid_mem_f() |
				     gr_fecs_current_ctx_valid_f(1)),
		     .mailbox = { .id = 0, .data = 0,
				  .clr = 0x30,
				  .ret = NULL,
				  .ok = 0x10,
				  .fail = 0x20, },
		     .cond.ok = GR_IS_UCODE_OP_AND,
		     .cond.fail = GR_IS_UCODE_OP_AND});
	if (ret)
		nvhost_err(dev_from_gk20a(g),
			"bind channel instance failed");

	return ret;
}

static int gr_gk20a_ctx_zcull_setup(struct gk20a *g, struct channel_gk20a *c,
				    bool disable_fifo)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = f->engine_info + ENGINE_GR_GK20A;
	u32 va_lo, va_hi, va;
	int ret = 0;
	void *ctx_ptr = NULL;

	nvhost_dbg_fn("");

	ctx_ptr = vmap(ch_ctx->gr_ctx.pages,
			PAGE_ALIGN(ch_ctx->gr_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!ctx_ptr)
		return -ENOMEM;

	if (ch_ctx->zcull_ctx.gpu_va == 0 &&
	    ch_ctx->zcull_ctx.ctx_sw_mode ==
		ctxsw_prog_main_image_zcull_mode_separate_buffer_v()) {
		ret = -EINVAL;
		goto clean_up;
	}

	va_lo = u64_lo32(ch_ctx->zcull_ctx.gpu_va);
	va_hi = u64_hi32(ch_ctx->zcull_ctx.gpu_va);
	va = ((va_lo >> 8) & 0x00FFFFFF) | ((va_hi << 24) & 0xFF000000);

	if (disable_fifo) {
		ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
		if (ret) {
			nvhost_err(dev_from_gk20a(g),
				"failed to disable gr engine activity\n");
			goto clean_up;
		}
	}

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, true);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_zcull_o(), 0,
		 ch_ctx->zcull_ctx.ctx_sw_mode);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_zcull_ptr_o(), 0, va);

	if (disable_fifo) {
		ret = gk20a_fifo_enable_engine_activity(g, gr_info);
		if (ret) {
			nvhost_err(dev_from_gk20a(g),
				"failed to enable gr engine activity\n");
			goto clean_up;
		}
	}
	gk20a_mm_l2_invalidate(g);

clean_up:
	vunmap(ctx_ptr);

	return ret;
}

static int gr_gk20a_commit_global_cb_manager(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = NULL;
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 temp;
	u32 cbm_cfg_size1, cbm_cfg_size2;

	nvhost_dbg_fn("");

	if (patch) {
		int err;
		ch_ctx = &c->ch_ctx;
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			return err;
	}

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_tga_constraintlogic_r(),
		gr_ds_tga_constraintlogic_beta_cbsize_f(gr->attrib_cb_default_size) |
		gr_ds_tga_constraintlogic_alpha_cbsize_f(gr->alpha_cb_default_size),
		patch);

	pd_ab_max_output = (gr->alpha_cb_default_size *
		gr_gpc0_ppc0_cbm_cfg_size_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	alpha_offset_in_chunk = attrib_offset_in_chunk +
		gr->tpc_count * gr->attrib_cb_size;

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		temp = proj_gpc_stride_v() * gpc_index;
		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
		     ppc_index++) {
			cbm_cfg_size1 = gr->attrib_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
			cbm_cfg_size2 = gr->alpha_cb_default_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_cfg_r() + temp +
				proj_ppc_in_gpc_stride_v() * ppc_index,
				gr_gpc0_ppc0_cbm_cfg_timeslice_mode_f(gr->timeslice_mode) |
				gr_gpc0_ppc0_cbm_cfg_start_offset_f(attrib_offset_in_chunk) |
				gr_gpc0_ppc0_cbm_cfg_size_f(cbm_cfg_size1), patch);

			attrib_offset_in_chunk += gr->attrib_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];

			gr_gk20a_ctx_patch_write(g, ch_ctx,
				gr_gpc0_ppc0_cbm_cfg2_r() + temp +
				proj_ppc_in_gpc_stride_v() * ppc_index,
				gr_gpc0_ppc0_cbm_cfg2_start_offset_f(alpha_offset_in_chunk) |
				gr_gpc0_ppc0_cbm_cfg2_size_f(cbm_cfg_size2), patch);

			alpha_offset_in_chunk += gr->alpha_cb_size *
				gr->pes_tpc_count[ppc_index][gpc_index];
		}
	}

	if (patch)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	return 0;
}

static int gr_gk20a_commit_global_ctx_buffers(struct gk20a *g,
			struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u64 addr;
	u32 size;
	u32 data;

	nvhost_dbg_fn("");
	if (patch) {
		int err;
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			return err;
	}

	/* global pagepool buffer */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[PAGEPOOL_VA]) >>
		gr_scc_pagepool_base_addr_39_8_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[PAGEPOOL_VA]) <<
		 (32 - gr_scc_pagepool_base_addr_39_8_align_bits_v()));

	size = gr->global_ctx_buffer[PAGEPOOL].size /
		gr_scc_pagepool_total_pages_byte_granularity_v();

	if (size == gr_scc_pagepool_total_pages_hwmax_value_v())
		size = gr_scc_pagepool_total_pages_hwmax_v();

	nvhost_dbg_info("pagepool buffer addr : 0x%016llx, size : %d",
		addr, size);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_pagepool_base_r(),
		gr_scc_pagepool_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_pagepool_r(),
		gr_scc_pagepool_total_pages_f(size) |
		gr_scc_pagepool_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gcc_pagepool_base_r(),
		gr_gpcs_gcc_pagepool_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gcc_pagepool_r(),
		gr_gpcs_gcc_pagepool_total_pages_f(size), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_pagepool_r(),
		gr_pd_pagepool_total_pages_f(size) |
		gr_pd_pagepool_valid_true_f(), patch);

	/* global bundle cb */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[CIRCULAR_VA]) >>
		gr_scc_bundle_cb_base_addr_39_8_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[CIRCULAR_VA]) <<
		 (32 - gr_scc_bundle_cb_base_addr_39_8_align_bits_v()));

	size = gr->bundle_cb_default_size;

	nvhost_dbg_info("bundle cb addr : 0x%016llx, size : %d",
		addr, size);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_base_r(),
		gr_scc_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_scc_bundle_cb_size_r(),
		gr_scc_bundle_cb_size_div_256b_f(size) |
		gr_scc_bundle_cb_size_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_bundle_cb_base_r(),
		gr_gpcs_setup_bundle_cb_base_addr_39_8_f(addr), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_bundle_cb_size_r(),
		gr_gpcs_setup_bundle_cb_size_div_256b_f(size) |
		gr_gpcs_setup_bundle_cb_size_valid_true_f(), patch);

	/* data for state_limit */
	data = (gr->bundle_cb_default_size *
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v()) /
		gr_pd_ab_dist_cfg2_state_limit_scc_bundle_granularity_v();

	data = min_t(u32, data, gr->min_gpm_fifo_depth);

	nvhost_dbg_info("bundle cb token limit : %d, state limit : %d",
		   gr->bundle_cb_token_limit, data);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg2_r(),
		gr_pd_ab_dist_cfg2_token_limit_f(gr->bundle_cb_token_limit) |
		gr_pd_ab_dist_cfg2_state_limit_f(data), patch);

	/* global attrib cb */
	addr = (u64_lo32(ch_ctx->global_ctx_buffer_va[ATTRIBUTE_VA]) >>
		gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()) |
		(u64_hi32(ch_ctx->global_ctx_buffer_va[ATTRIBUTE_VA]) <<
		 (32 - gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v()));

	nvhost_dbg_info("attrib cb addr : 0x%016llx", addr);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_setup_attrib_cb_base_r(),
		gr_gpcs_setup_attrib_cb_base_addr_39_12_f(addr) |
		gr_gpcs_setup_attrib_cb_base_valid_true_f(), patch);

	gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pe_pin_cb_global_base_addr_r(),
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_v_f(addr) |
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_valid_true_f(), patch);

	if (patch)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	return 0;
}

static int gr_gk20a_commit_global_timeslice(struct gk20a *g, struct channel_gk20a *c, bool patch)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = NULL;
	u32 gpm_pd_cfg;
	u32 pd_ab_dist_cfg0;
	u32 ds_debug;
	u32 mpc_vtg_debug;
	u32 pe_vaf;
	u32 pe_vsc_vpc;

	nvhost_dbg_fn("");

	gpm_pd_cfg = gk20a_readl(g, gr_gpcs_gpm_pd_cfg_r());
	pd_ab_dist_cfg0 = gk20a_readl(g, gr_pd_ab_dist_cfg0_r());
	ds_debug = gk20a_readl(g, gr_ds_debug_r());
	mpc_vtg_debug = gk20a_readl(g, gr_gpcs_tpcs_mpc_vtg_debug_r());

	if (patch) {
		int err;
		ch_ctx = &c->ch_ctx;
		err = gr_gk20a_ctx_patch_write_begin(g, ch_ctx);
		if (err)
			return err;
	}

	if (gr->timeslice_mode == gr_gpcs_ppcs_cbm_cfg_timeslice_mode_enable_v()) {
		pe_vaf = gk20a_readl(g, gr_gpcs_tpcs_pe_vaf_r());
		pe_vsc_vpc = gk20a_readl(g, gr_gpcs_tpcs_pes_vsc_vpc_r());

		gpm_pd_cfg = gr_gpcs_gpm_pd_cfg_timeslice_mode_enable_f() | gpm_pd_cfg;
		pe_vaf = gr_gpcs_tpcs_pe_vaf_fast_mode_switch_true_f() | pe_vaf;
		pe_vsc_vpc = gr_gpcs_tpcs_pes_vsc_vpc_fast_mode_switch_true_f() | pe_vsc_vpc;
		pd_ab_dist_cfg0 = gr_pd_ab_dist_cfg0_timeslice_enable_en_f() | pd_ab_dist_cfg0;
		ds_debug = gr_ds_debug_timeslice_mode_enable_f() | ds_debug;
		mpc_vtg_debug = gr_gpcs_tpcs_mpc_vtg_debug_timeslice_mode_enabled_f() | mpc_vtg_debug;

		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gpm_pd_cfg_r(), gpm_pd_cfg, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pe_vaf_r(), pe_vaf, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_pes_vsc_vpc_r(), pe_vsc_vpc, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg0_r(), pd_ab_dist_cfg0, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_debug_r(), ds_debug, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_debug_r(), mpc_vtg_debug, patch);
	} else {
		gpm_pd_cfg = gr_gpcs_gpm_pd_cfg_timeslice_mode_disable_f() | gpm_pd_cfg;
		pd_ab_dist_cfg0 = gr_pd_ab_dist_cfg0_timeslice_enable_dis_f() | pd_ab_dist_cfg0;
		ds_debug = gr_ds_debug_timeslice_mode_disable_f() | ds_debug;
		mpc_vtg_debug = gr_gpcs_tpcs_mpc_vtg_debug_timeslice_mode_disabled_f() | mpc_vtg_debug;

		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_gpm_pd_cfg_r(), gpm_pd_cfg, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_pd_ab_dist_cfg0_r(), pd_ab_dist_cfg0, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_ds_debug_r(), ds_debug, patch);
		gr_gk20a_ctx_patch_write(g, ch_ctx, gr_gpcs_tpcs_mpc_vtg_debug_r(), mpc_vtg_debug, patch);
	}

	if (patch)
		gr_gk20a_ctx_patch_write_end(g, ch_ctx);

	return 0;
}

static int gr_gk20a_setup_rop_mapping(struct gk20a *g,
				struct gr_gk20a *gr)
{
	u32 norm_entries, norm_shift;
	u32 coeff5_mod, coeff6_mod, coeff7_mod, coeff8_mod, coeff9_mod, coeff10_mod, coeff11_mod;
	u32 map0, map1, map2, map3, map4, map5;

	if (!gr->map_tiles)
		return -1;

	nvhost_dbg_fn("");

	gk20a_writel(g, gr_crstr_map_table_cfg_r(),
		     gr_crstr_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_crstr_map_table_cfg_num_entries_f(gr->tpc_count));

	map0 =  gr_crstr_gpc_map0_tile0_f(gr->map_tiles[0]) |
		gr_crstr_gpc_map0_tile1_f(gr->map_tiles[1]) |
		gr_crstr_gpc_map0_tile2_f(gr->map_tiles[2]) |
		gr_crstr_gpc_map0_tile3_f(gr->map_tiles[3]) |
		gr_crstr_gpc_map0_tile4_f(gr->map_tiles[4]) |
		gr_crstr_gpc_map0_tile5_f(gr->map_tiles[5]);

	map1 =  gr_crstr_gpc_map1_tile6_f(gr->map_tiles[6]) |
		gr_crstr_gpc_map1_tile7_f(gr->map_tiles[7]) |
		gr_crstr_gpc_map1_tile8_f(gr->map_tiles[8]) |
		gr_crstr_gpc_map1_tile9_f(gr->map_tiles[9]) |
		gr_crstr_gpc_map1_tile10_f(gr->map_tiles[10]) |
		gr_crstr_gpc_map1_tile11_f(gr->map_tiles[11]);

	map2 =  gr_crstr_gpc_map2_tile12_f(gr->map_tiles[12]) |
		gr_crstr_gpc_map2_tile13_f(gr->map_tiles[13]) |
		gr_crstr_gpc_map2_tile14_f(gr->map_tiles[14]) |
		gr_crstr_gpc_map2_tile15_f(gr->map_tiles[15]) |
		gr_crstr_gpc_map2_tile16_f(gr->map_tiles[16]) |
		gr_crstr_gpc_map2_tile17_f(gr->map_tiles[17]);

	map3 =  gr_crstr_gpc_map3_tile18_f(gr->map_tiles[18]) |
		gr_crstr_gpc_map3_tile19_f(gr->map_tiles[19]) |
		gr_crstr_gpc_map3_tile20_f(gr->map_tiles[20]) |
		gr_crstr_gpc_map3_tile21_f(gr->map_tiles[21]) |
		gr_crstr_gpc_map3_tile22_f(gr->map_tiles[22]) |
		gr_crstr_gpc_map3_tile23_f(gr->map_tiles[23]);

	map4 =  gr_crstr_gpc_map4_tile24_f(gr->map_tiles[24]) |
		gr_crstr_gpc_map4_tile25_f(gr->map_tiles[25]) |
		gr_crstr_gpc_map4_tile26_f(gr->map_tiles[26]) |
		gr_crstr_gpc_map4_tile27_f(gr->map_tiles[27]) |
		gr_crstr_gpc_map4_tile28_f(gr->map_tiles[28]) |
		gr_crstr_gpc_map4_tile29_f(gr->map_tiles[29]);

	map5 =  gr_crstr_gpc_map5_tile30_f(gr->map_tiles[30]) |
		gr_crstr_gpc_map5_tile31_f(gr->map_tiles[31]) |
		gr_crstr_gpc_map5_tile32_f(0) |
		gr_crstr_gpc_map5_tile33_f(0) |
		gr_crstr_gpc_map5_tile34_f(0) |
		gr_crstr_gpc_map5_tile35_f(0);

	gk20a_writel(g, gr_crstr_gpc_map0_r(), map0);
	gk20a_writel(g, gr_crstr_gpc_map1_r(), map1);
	gk20a_writel(g, gr_crstr_gpc_map2_r(), map2);
	gk20a_writel(g, gr_crstr_gpc_map3_r(), map3);
	gk20a_writel(g, gr_crstr_gpc_map4_r(), map4);
	gk20a_writel(g, gr_crstr_gpc_map5_r(), map5);

	switch (gr->tpc_count) {
	case 1:
		norm_shift = 4;
		break;
	case 2:
	case 3:
		norm_shift = 3;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		norm_shift = 2;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		norm_shift = 1;
		break;
	default:
		norm_shift = 0;
		break;
	}

	norm_entries = gr->tpc_count << norm_shift;
	coeff5_mod = (1 << 5) % norm_entries;
	coeff6_mod = (1 << 6) % norm_entries;
	coeff7_mod = (1 << 7) % norm_entries;
	coeff8_mod = (1 << 8) % norm_entries;
	coeff9_mod = (1 << 9) % norm_entries;
	coeff10_mod = (1 << 10) % norm_entries;
	coeff11_mod = (1 << 11) % norm_entries;

	gk20a_writel(g, gr_ppcs_wwdx_map_table_cfg_r(),
		     gr_ppcs_wwdx_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_ppcs_wwdx_map_table_cfg_normalized_num_entries_f(norm_entries) |
		     gr_ppcs_wwdx_map_table_cfg_normalized_shift_value_f(norm_shift) |
		     gr_ppcs_wwdx_map_table_cfg_coeff5_mod_value_f(coeff5_mod) |
		     gr_ppcs_wwdx_map_table_cfg_num_entries_f(gr->tpc_count));

	gk20a_writel(g, gr_ppcs_wwdx_map_table_cfg2_r(),
		     gr_ppcs_wwdx_map_table_cfg2_coeff6_mod_value_f(coeff6_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff7_mod_value_f(coeff7_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff8_mod_value_f(coeff8_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff9_mod_value_f(coeff9_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff10_mod_value_f(coeff10_mod) |
		     gr_ppcs_wwdx_map_table_cfg2_coeff11_mod_value_f(coeff11_mod));

	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map0_r(), map0);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map1_r(), map1);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map2_r(), map2);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map3_r(), map3);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map4_r(), map4);
	gk20a_writel(g, gr_ppcs_wwdx_map_gpc_map5_r(), map5);

	gk20a_writel(g, gr_rstr2d_map_table_cfg_r(),
		     gr_rstr2d_map_table_cfg_row_offset_f(gr->map_row_offset) |
		     gr_rstr2d_map_table_cfg_num_entries_f(gr->tpc_count));

	gk20a_writel(g, gr_rstr2d_gpc_map0_r(), map0);
	gk20a_writel(g, gr_rstr2d_gpc_map1_r(), map1);
	gk20a_writel(g, gr_rstr2d_gpc_map2_r(), map2);
	gk20a_writel(g, gr_rstr2d_gpc_map3_r(), map3);
	gk20a_writel(g, gr_rstr2d_gpc_map4_r(), map4);
	gk20a_writel(g, gr_rstr2d_gpc_map5_r(), map5);

	return 0;
}

static inline u32 count_bits(u32 mask)
{
	u32 temp = mask;
	u32 count;
	for (count = 0; temp != 0; count++)
		temp &= temp - 1;

	return count;
}

static inline u32 clear_count_bits(u32 num, u32 clear_count)
{
	u32 count = clear_count;
	for (; (num != 0) && (count != 0); count--)
		num &= num - 1;

	return num;
}

static int gr_gk20a_setup_alpha_beta_tables(struct gk20a *g,
					struct gr_gk20a *gr)
{
	u32 table_index_bits = 5;
	u32 rows = (1 << table_index_bits);
	u32 row_stride = gr_pd_alpha_ratio_table__size_1_v() / rows;

	u32 row;
	u32 index;
	u32 gpc_index;
	u32 gpcs_per_reg = 4;
	u32 pes_index;
	u32 tpc_count_pes;
	u32 num_pes_per_gpc = proj_scal_litter_num_pes_per_gpc_v();

	u32 alpha_target, beta_target;
	u32 alpha_bits, beta_bits;
	u32 alpha_mask, beta_mask, partial_mask;
	u32 reg_offset;
	bool assign_alpha;

	u32 map_alpha[gr_pd_alpha_ratio_table__size_1_v()];
	u32 map_beta[gr_pd_alpha_ratio_table__size_1_v()];
	u32 map_reg_used[gr_pd_alpha_ratio_table__size_1_v()];

	nvhost_dbg_fn("");

	memset(map_alpha, 0, gr_pd_alpha_ratio_table__size_1_v() * sizeof(u32));
	memset(map_beta, 0, gr_pd_alpha_ratio_table__size_1_v() * sizeof(u32));
	memset(map_reg_used, 0, gr_pd_alpha_ratio_table__size_1_v() * sizeof(u32));

	for (row = 0; row < rows; ++row) {
		alpha_target = max_t(u32, gr->tpc_count * row / rows, 1);
		beta_target = gr->tpc_count - alpha_target;

		assign_alpha = (alpha_target < beta_target);

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			reg_offset = (row * row_stride) + (gpc_index / gpcs_per_reg);
			alpha_mask = beta_mask = 0;

			for (pes_index = 0; pes_index < num_pes_per_gpc; pes_index++) {
				tpc_count_pes = gr->pes_tpc_count[pes_index][gpc_index];

				if (assign_alpha) {
					alpha_bits = (alpha_target == 0) ? 0 : tpc_count_pes;
					beta_bits = tpc_count_pes - alpha_bits;
				} else {
					beta_bits = (beta_target == 0) ? 0 : tpc_count_pes;
					alpha_bits = tpc_count_pes - beta_bits;
				}

				partial_mask = gr->pes_tpc_mask[pes_index][gpc_index];
				partial_mask = clear_count_bits(partial_mask, tpc_count_pes - alpha_bits);
				alpha_mask |= partial_mask;

				partial_mask = gr->pes_tpc_mask[pes_index][gpc_index] ^ partial_mask;
				beta_mask |= partial_mask;

				alpha_target -= min(alpha_bits, alpha_target);
				beta_target -= min(beta_bits, beta_target);

				if ((alpha_bits > 0) || (beta_bits > 0))
					assign_alpha = !assign_alpha;
			}

			switch (gpc_index % gpcs_per_reg) {
			case 0:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n0_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n0_mask_f(beta_mask);
				break;
			case 1:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n1_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n1_mask_f(beta_mask);
				break;
			case 2:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n2_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n2_mask_f(beta_mask);
				break;
			case 3:
				map_alpha[reg_offset] |= gr_pd_alpha_ratio_table_gpc_4n3_mask_f(alpha_mask);
				map_beta[reg_offset] |= gr_pd_beta_ratio_table_gpc_4n3_mask_f(beta_mask);
				break;
			}
			map_reg_used[reg_offset] = true;
		}
	}

	for (index = 0; index < gr_pd_alpha_ratio_table__size_1_v(); index++) {
		if (map_reg_used[index]) {
			gk20a_writel(g, gr_pd_alpha_ratio_table_r(index), map_alpha[index]);
			gk20a_writel(g, gr_pd_beta_ratio_table_r(index), map_beta[index]);
		}
	}

	return 0;
}

static int gr_gk20a_ctx_state_floorsweep(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	u32 tpc_index, gpc_index;
	u32 tpc_offset, gpc_offset;
	u32 sm_id = 0, gpc_id = 0;
	u32 sm_id_to_gpc_id[proj_scal_max_gpcs_v() * proj_scal_max_tpc_per_gpc_v()];
	u32 tpc_per_gpc;
	u32 max_ways_evict = INVALID_MAX_WAYS;

	nvhost_dbg_fn("");

	for (tpc_index = 0; tpc_index < gr->max_tpc_per_gpc_count; tpc_index++) {
		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			gpc_offset = proj_gpc_stride_v() * gpc_index;
			if (tpc_index < gr->gpc_tpc_count[gpc_index]) {
				tpc_offset = proj_tpc_in_gpc_stride_v() * tpc_index;

				gk20a_writel(g, gr_gpc0_tpc0_sm_cfg_r() + gpc_offset + tpc_offset,
					     gr_gpc0_tpc0_sm_cfg_sm_id_f(sm_id));
				gk20a_writel(g, gr_gpc0_tpc0_l1c_cfg_smid_r() + gpc_offset + tpc_offset,
					     gr_gpc0_tpc0_l1c_cfg_smid_value_f(sm_id));
				gk20a_writel(g, gr_gpc0_gpm_pd_sm_id_r(tpc_index) + gpc_offset,
					     gr_gpc0_gpm_pd_sm_id_id_f(sm_id));
				gk20a_writel(g, gr_gpc0_tpc0_pe_cfg_smid_r() + gpc_offset + tpc_offset,
					     gr_gpc0_tpc0_pe_cfg_smid_value_f(sm_id));

				sm_id_to_gpc_id[sm_id] = gpc_index;
				sm_id++;
			}

			gk20a_writel(g, gr_gpc0_gpm_pd_active_tpcs_r() + gpc_offset,
				     gr_gpc0_gpm_pd_active_tpcs_num_f(gr->gpc_tpc_count[gpc_index]));
			gk20a_writel(g, gr_gpc0_gpm_sd_active_tpcs_r() + gpc_offset,
				     gr_gpc0_gpm_sd_active_tpcs_num_f(gr->gpc_tpc_count[gpc_index]));
		}
	}

	for (tpc_index = 0, gpc_id = 0;
	     tpc_index < gr_pd_num_tpc_per_gpc__size_1_v();
	     tpc_index++, gpc_id += 8) {

		if (gpc_id >= gr->gpc_count)
			gpc_id = 0;

		tpc_per_gpc =
			gr_pd_num_tpc_per_gpc_count0_f(gr->gpc_tpc_count[gpc_id + 0]) |
			gr_pd_num_tpc_per_gpc_count1_f(gr->gpc_tpc_count[gpc_id + 1]) |
			gr_pd_num_tpc_per_gpc_count2_f(gr->gpc_tpc_count[gpc_id + 2]) |
			gr_pd_num_tpc_per_gpc_count3_f(gr->gpc_tpc_count[gpc_id + 3]) |
			gr_pd_num_tpc_per_gpc_count4_f(gr->gpc_tpc_count[gpc_id + 4]) |
			gr_pd_num_tpc_per_gpc_count5_f(gr->gpc_tpc_count[gpc_id + 5]) |
			gr_pd_num_tpc_per_gpc_count6_f(gr->gpc_tpc_count[gpc_id + 6]) |
			gr_pd_num_tpc_per_gpc_count7_f(gr->gpc_tpc_count[gpc_id + 7]);

		gk20a_writel(g, gr_pd_num_tpc_per_gpc_r(tpc_index), tpc_per_gpc);
		gk20a_writel(g, gr_ds_num_tpc_per_gpc_r(tpc_index), tpc_per_gpc);
	}

	/* gr__setup_pd_mapping stubbed for gk20a */
	gr_gk20a_setup_rop_mapping(g, gr);
	gr_gk20a_setup_alpha_beta_tables(g, gr);

	if (gr->num_fbps == 1)
		max_ways_evict = 9;

	if (max_ways_evict != INVALID_MAX_WAYS)
		gk20a_writel(g, ltc_ltcs_ltss_tstg_set_mgmt_r(),
			     ((gk20a_readl(g, ltc_ltcs_ltss_tstg_set_mgmt_r()) &
			       ~(ltc_ltcs_ltss_tstg_set_mgmt_max_ways_evict_last_f(~0))) |
			      ltc_ltcs_ltss_tstg_set_mgmt_max_ways_evict_last_f(max_ways_evict)));

	for (gpc_index = 0;
	     gpc_index < gr_pd_dist_skip_table__size_1_v() * 4;
	     gpc_index += 4) {

		gk20a_writel(g, gr_pd_dist_skip_table_r(gpc_index/4),
			     gr_pd_dist_skip_table_gpc_4n0_mask_f(gr->gpc_skip_mask[gpc_index]) ||
			     gr_pd_dist_skip_table_gpc_4n1_mask_f(gr->gpc_skip_mask[gpc_index + 1]) ||
			     gr_pd_dist_skip_table_gpc_4n2_mask_f(gr->gpc_skip_mask[gpc_index + 2]) ||
			     gr_pd_dist_skip_table_gpc_4n3_mask_f(gr->gpc_skip_mask[gpc_index + 3]));
	}

	gk20a_writel(g, gr_cwd_fs_r(),
		     gr_cwd_fs_num_gpcs_f(gr->gpc_count) |
		     gr_cwd_fs_num_tpcs_f(gr->tpc_count));

	gk20a_writel(g, gr_bes_zrop_settings_r(),
		     gr_bes_zrop_settings_num_active_fbps_f(gr->num_fbps));
	gk20a_writel(g, gr_bes_crop_settings_r(),
		     gr_bes_crop_settings_num_active_fbps_f(gr->num_fbps));

	return 0;
}

static int gr_gk20a_fecs_ctx_image_save(struct channel_gk20a *c, u32 save_type)
{
	struct gk20a *g = c->g;
	int ret;

	u32 inst_base_ptr =
		u64_lo32(c->inst_block.cpu_pa
		>> ram_in_base_shift_v());


	nvhost_dbg_fn("");

	ret = gr_gk20a_submit_fecs_method_op(g,
		(struct fecs_method_op_gk20a) {
		.method.addr = save_type,
		.method.data = (gr_fecs_current_ctx_ptr_f(inst_base_ptr) |
				gr_fecs_current_ctx_target_vid_mem_f() |
				gr_fecs_current_ctx_valid_f(1)),
		.mailbox = {.id = 0, .data = 0, .clr = 3, .ret = NULL,
			.ok = 1, .fail = 2,
		},
		.cond.ok = GR_IS_UCODE_OP_AND,
		.cond.fail = GR_IS_UCODE_OP_AND,
		 });

	if (ret)
		nvhost_err(dev_from_gk20a(g), "save context image failed");

	return ret;
}

/* init global golden image from a fresh gr_ctx in channel ctx.
   save a copy in local_golden_image in ctx_vars */
static int gr_gk20a_init_golden_ctx_image(struct gk20a *g,
					  struct channel_gk20a *c)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 ctx_header_bytes = ctxsw_prog_fecs_header_v();
	u32 ctx_header_words;
	u32 i;
	u32 data;
	void *ctx_ptr = NULL;
	void *gold_ptr = NULL;
	u32 err = 0;

	nvhost_dbg_fn("");

	/* golden ctx is global to all channels. Although only the first
	   channel initializes golden image, driver needs to prevent multiple
	   channels from initializing golden ctx at the same time */
	mutex_lock(&gr->ctx_mutex);

	if (gr->ctx_vars.golden_image_initialized)
		goto clean_up;

	err = gr_gk20a_fecs_ctx_bind_channel(g, c);
	if (err)
		goto clean_up;

	err = gr_gk20a_elpg_protected_call(g,
			gr_gk20a_commit_global_ctx_buffers(g, c, false));
	if (err)
		goto clean_up;

	gold_ptr = nvhost_memmgr_mmap(gr->global_ctx_buffer[GOLDEN_CTX].ref);
	if (!gold_ptr)
		goto clean_up;

	ctx_ptr = vmap(ch_ctx->gr_ctx.pages,
			PAGE_ALIGN(ch_ctx->gr_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!ctx_ptr)
		goto clean_up;

	ctx_header_words =  roundup(ctx_header_bytes, sizeof(u32));
	ctx_header_words >>= 2;

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush before cpu read. */
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, false);

	for (i = 0; i < ctx_header_words; i++) {
		data = mem_rd32(ctx_ptr, i);
		mem_wr32(gold_ptr, i, data);
	}

	mem_wr32(gold_ptr + ctxsw_prog_main_image_zcull_o(), 0,
		 ctxsw_prog_main_image_zcull_mode_no_ctxsw_v());

	mem_wr32(gold_ptr + ctxsw_prog_main_image_zcull_ptr_o(), 0, 0);

	gr_gk20a_commit_inst(c, ch_ctx->global_ctx_buffer_va[GOLDEN_CTX_VA]);

	gr_gk20a_fecs_ctx_image_save(c, gr_fecs_method_push_adr_wfi_golden_save_v());

	if (gr->ctx_vars.local_golden_image == NULL) {

		gr->ctx_vars.local_golden_image =
			kzalloc(gr->ctx_vars.golden_image_size, GFP_KERNEL);

		if (gr->ctx_vars.local_golden_image == NULL) {
			err = -ENOMEM;
			goto clean_up;
		}

		for (i = 0; i < gr->ctx_vars.golden_image_size / 4; i++)
			gr->ctx_vars.local_golden_image[i] =
				mem_rd32(gold_ptr, i);
	}

	gr_gk20a_commit_inst(c, ch_ctx->gr_ctx.gpu_va);

	gr->ctx_vars.golden_image_initialized = true;

	gk20a_mm_l2_invalidate(g);

	gk20a_writel(g, gr_fecs_current_ctx_r(),
		gr_fecs_current_ctx_valid_false_f());

clean_up:
	if (err)
		nvhost_err(dev_from_gk20a(g), "fail");
	else
		nvhost_dbg_fn("done");

	if (gold_ptr)
		nvhost_memmgr_munmap(gr->global_ctx_buffer[GOLDEN_CTX].ref,
				     gold_ptr);
	if (ctx_ptr)
		vunmap(ctx_ptr);

	mutex_unlock(&gr->ctx_mutex);
	return err;
}

int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				    struct channel_gk20a *c,
				    bool enable_smpc_ctxsw)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	void *ctx_ptr = NULL;
	u32 data;

	/*XXX caller responsible for making sure the channel is quiesced? */

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, true);

	ctx_ptr = vmap(ch_ctx->gr_ctx.pages,
			PAGE_ALIGN(ch_ctx->gr_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!ctx_ptr)
		return -ENOMEM;

	data = mem_rd32(ctx_ptr + ctxsw_prog_main_image_pm_o(), 0);
	data = data & ~ctxsw_prog_main_image_pm_smpc_mode_m();
	data |= enable_smpc_ctxsw ?
		ctxsw_prog_main_image_pm_smpc_mode_ctxsw_f() :
		ctxsw_prog_main_image_pm_smpc_mode_no_ctxsw_f();
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_pm_o(), 0,
		 data);

	vunmap(ctx_ptr);

	gk20a_mm_l2_invalidate(g);

	return 0;
}

/* load saved fresh copy of gloden image into channel gr_ctx */
static int gr_gk20a_load_golden_ctx_image(struct gk20a *g,
					struct channel_gk20a *c)
{
	struct gr_gk20a *gr = &g->gr;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	u32 virt_addr_lo;
	u32 virt_addr_hi;
	u32 i, v, data;
	int ret = 0;
	void *ctx_ptr = NULL;

	nvhost_dbg_fn("");

	if (gr->ctx_vars.local_golden_image == NULL)
		return -1;

	/* Channel gr_ctx buffer is gpu cacheable.
	   Flush and invalidate before cpu update. */
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, true);

	ctx_ptr = vmap(ch_ctx->gr_ctx.pages,
			PAGE_ALIGN(ch_ctx->gr_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!ctx_ptr)
		return -ENOMEM;

	for (i = 0; i < gr->ctx_vars.golden_image_size / 4; i++)
		mem_wr32(ctx_ptr, i, gr->ctx_vars.local_golden_image[i]);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_num_save_ops_o(), 0, 0);
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_num_restore_ops_o(), 0, 0);

	virt_addr_lo = u64_lo32(ch_ctx->patch_ctx.gpu_va);
	virt_addr_hi = u64_hi32(ch_ctx->patch_ctx.gpu_va);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_patch_count_o(), 0,
		 ch_ctx->patch_ctx.data_count);
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_patch_adr_lo_o(), 0,
		 virt_addr_lo);
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_patch_adr_hi_o(), 0,
		 virt_addr_hi);

	/* no user for client managed performance counter ctx */
	ch_ctx->pm_ctx.ctx_sw_mode =
		ctxsw_prog_main_image_pm_mode_no_ctxsw_f();
	data = mem_rd32(ctx_ptr + ctxsw_prog_main_image_pm_o(), 0);
	data = data & ~ctxsw_prog_main_image_pm_mode_m();
	data |= ch_ctx->pm_ctx.ctx_sw_mode;
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_pm_o(), 0,
		 data);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_pm_ptr_o(), 0, 0);

	/* set priv access map */
	virt_addr_lo =
		 u64_lo32(ch_ctx->global_ctx_buffer_va[PRIV_ACCESS_MAP_VA]);
	virt_addr_hi =
		 u64_hi32(ch_ctx->global_ctx_buffer_va[PRIV_ACCESS_MAP_VA]);

	mem_wr32(ctx_ptr + ctxsw_prog_main_image_priv_access_map_config_o(), 0,
		 ctxsw_prog_main_image_priv_access_map_config_mode_use_map_f());
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_priv_access_map_addr_lo_o(), 0,
		 virt_addr_lo);
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_priv_access_map_addr_hi_o(), 0,
		 virt_addr_hi);
	/* disable verif features */
	v = mem_rd32(ctx_ptr + ctxsw_prog_main_image_misc_options_o(), 0);
	v = v & ~(ctxsw_prog_main_image_misc_options_verif_features_m());
	v = v | ctxsw_prog_main_image_misc_options_verif_features_disabled_f();
	mem_wr32(ctx_ptr + ctxsw_prog_main_image_misc_options_o(), 0, v);


	vunmap(ctx_ptr);

	gk20a_mm_l2_invalidate(g);

	if (tegra_platform_is_linsim()) {
		u32 inst_base_ptr =
			u64_lo32(c->inst_block.cpu_pa
			>> ram_in_base_shift_v());

		ret = gr_gk20a_submit_fecs_method_op(g,
			  (struct fecs_method_op_gk20a) {
				  .method.data =
					  (gr_fecs_current_ctx_ptr_f(inst_base_ptr) |
					   gr_fecs_current_ctx_target_vid_mem_f() |
					   gr_fecs_current_ctx_valid_f(1)),
				  .method.addr =
					  gr_fecs_method_push_adr_restore_golden_v(),
				  .mailbox = {
					  .id = 0, .data = 0,
					  .clr = ~0, .ret = NULL,
					  .ok = gr_fecs_ctxsw_mailbox_value_pass_v(),
					  .fail = 0},
				  .cond.ok = GR_IS_UCODE_OP_EQUAL,
				  .cond.fail = GR_IS_UCODE_OP_SKIP});

		if (ret)
			nvhost_err(dev_from_gk20a(g),
				   "restore context image failed");
	}

	return ret;
}

static void gr_gk20a_start_falcon_ucode(struct gk20a *g)
{
	nvhost_dbg_fn("");

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0),
		     gr_fecs_ctxsw_mailbox_clear_value_f(~0));

	gk20a_writel(g, gr_gpccs_dmactl_r(), gr_gpccs_dmactl_require_ctx_f(0));
	gk20a_writel(g, gr_fecs_dmactl_r(), gr_fecs_dmactl_require_ctx_f(0));

	gk20a_writel(g, gr_gpccs_cpuctl_r(), gr_gpccs_cpuctl_startcpu_f(1));
	gk20a_writel(g, gr_fecs_cpuctl_r(), gr_fecs_cpuctl_startcpu_f(1));

	nvhost_dbg_fn("done");
}

static int gr_gk20a_init_ctxsw_ucode_vaspace(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct device *d = dev_from_gk20a(g);
	struct gk20a_ctxsw_ucode_info *p_ucode_info = &g->ctxsw_ucode_info;
	void *inst_ptr;
	u32 pde_addr_lo;
	u32 pde_addr_hi;
	u64 pde_addr;

	/* Alloc mem of inst block */
	p_ucode_info->inst_blk_desc.size = ram_in_alloc_size_v();
	p_ucode_info->inst_blk_desc.cpuva = dma_alloc_coherent(d,
					p_ucode_info->inst_blk_desc.size,
					&p_ucode_info->inst_blk_desc.iova,
					GFP_KERNEL);
	if (!p_ucode_info->inst_blk_desc.cpuva) {
		nvhost_err(d, "failed to allocate memory\n");
		return -ENOMEM;
	}

	p_ucode_info->inst_blk_desc.cpu_pa = gk20a_get_phys_from_iova(d,
					p_ucode_info->inst_blk_desc.iova);

	inst_ptr = p_ucode_info->inst_blk_desc.cpuva;

	/* Set inst block */
	mem_wr32(inst_ptr, ram_in_adr_limit_lo_w(),
		 u64_lo32(vm->va_limit) | 0xFFF);
	mem_wr32(inst_ptr, ram_in_adr_limit_hi_w(),
		ram_in_adr_limit_hi_f(u64_hi32(vm->va_limit)));

	pde_addr = gk20a_mm_iova_addr(vm->pdes.sgt->sgl);
	pde_addr_lo = u64_lo32(pde_addr >> 12);
	pde_addr_hi = u64_hi32(pde_addr);
	mem_wr32(inst_ptr, ram_in_page_dir_base_lo_w(),
		ram_in_page_dir_base_target_vid_mem_f() |
		ram_in_page_dir_base_vol_true_f() |
		ram_in_page_dir_base_lo_f(pde_addr_lo));
	mem_wr32(inst_ptr, ram_in_page_dir_base_hi_w(),
		ram_in_page_dir_base_hi_f(pde_addr_hi));

	/* Map ucode surface to GMMU */
	p_ucode_info->ucode_gpuva = gk20a_gmmu_map(vm,
					&p_ucode_info->surface_desc.sgt,
					p_ucode_info->surface_desc.size,
					0, /* flags */
					mem_flag_read_only);
	if (!p_ucode_info->ucode_gpuva) {
		nvhost_err(d, "failed to update gmmu ptes\n");
		return -ENOMEM;
	}

	return 0;
}

static void gr_gk20a_init_ctxsw_ucode_segment(
	struct gk20a_ctxsw_ucode_segment *p_seg, u32 *p_offset, u32 size)
{
	p_seg->offset = *p_offset;
	p_seg->size = size;
	*p_offset = ALIGN(*p_offset + size, BLK_SIZE);
}

static void gr_gk20a_init_ctxsw_ucode_inst(
	struct gk20a_ctxsw_ucode_inst *p_inst, u32 *p_offset,
	struct gk20a_ctxsw_bootloader_desc *p_bootdesc,
	u32 code_size, u32 data_size)
{
	u32 boot_size = ALIGN(p_bootdesc->bootloader_size, sizeof(u32));
	p_inst->boot_entry = p_bootdesc->bootloader_entry_point;
	p_inst->boot_imem_offset = p_bootdesc->bootloader_imem_offset;
	gr_gk20a_init_ctxsw_ucode_segment(&p_inst->boot, p_offset, boot_size);
	gr_gk20a_init_ctxsw_ucode_segment(&p_inst->code, p_offset, code_size);
	gr_gk20a_init_ctxsw_ucode_segment(&p_inst->data, p_offset, data_size);
}

static int gr_gk20a_copy_ctxsw_ucode_inst(
	u8 *p_buf,
	struct gk20a_ctxsw_ucode_inst *p_inst,
	struct gk20a_ctxsw_bootloader_desc *p_bootdesc, u32 *p_bootimage,
	u32 *p_code, u32 *p_data)
{
	memcpy(p_buf + p_inst->boot.offset, p_bootimage, p_inst->boot.size);
	memcpy(p_buf + p_inst->code.offset, p_code, p_inst->code.size);
	memcpy(p_buf + p_inst->data.offset, p_data, p_inst->data.size);
	return 0;
}

static int gr_gk20a_init_ctxsw_ucode(struct gk20a *g)
{
	struct device *d = dev_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = &mm->pmu.vm;
	struct gk20a_ctxsw_bootloader_desc *p_fecs_boot_desc;
	struct gk20a_ctxsw_bootloader_desc *p_gpcs_boot_desc;
	const struct firmware *fecs_fw;
	const struct firmware *gpccs_fw;
	u32 *p_fecs_boot_image;
	u32 *p_gpcs_boot_image;
	struct gk20a_ctxsw_ucode_info *p_ucode_info = &g->ctxsw_ucode_info;
	u8 *p_buf;
	u32 ucode_size;
	int err = 0;
	DEFINE_DMA_ATTRS(attrs);

	fecs_fw = nvhost_client_request_firmware(g->dev,
					GK20A_FECS_UCODE_IMAGE);
	if (!fecs_fw) {
		nvhost_err(d, "failed to load fecs ucode!!");
		return -ENOENT;
	}

	p_fecs_boot_desc = fecs_fw->data;
	p_fecs_boot_image = fecs_fw->data +
				sizeof(struct gk20a_ctxsw_bootloader_desc);

	gpccs_fw = nvhost_client_request_firmware(g->dev,
					GK20A_GPCCS_UCODE_IMAGE);
	if (!gpccs_fw) {
		release_firmware(fecs_fw);
		nvhost_err(d, "failed to load gpccs ucode!!");
		return -ENOENT;
	}

	p_gpcs_boot_desc = gpccs_fw->data;
	p_gpcs_boot_image = gpccs_fw->data +
				sizeof(struct gk20a_ctxsw_bootloader_desc);

	ucode_size = 0;
	gr_gk20a_init_ctxsw_ucode_inst(&p_ucode_info->fecs, &ucode_size,
		p_fecs_boot_desc,
		g->gr.ctx_vars.ucode.fecs.inst.count * sizeof(u32),
		g->gr.ctx_vars.ucode.fecs.data.count * sizeof(u32));
	gr_gk20a_init_ctxsw_ucode_inst(&p_ucode_info->gpcs, &ucode_size,
		p_gpcs_boot_desc,
		g->gr.ctx_vars.ucode.gpccs.inst.count * sizeof(u32),
		g->gr.ctx_vars.ucode.gpccs.data.count * sizeof(u32));

	p_ucode_info->surface_desc.size = ucode_size;
	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);
	p_ucode_info->surface_desc.cpuva = dma_alloc_attrs(d,
					p_ucode_info->surface_desc.size,
					&p_ucode_info->surface_desc.iova,
					GFP_KERNEL,
					&attrs);
	if (!p_ucode_info->surface_desc.cpuva) {
		nvhost_err(d, "memory allocation failed\n");
		err = -ENOMEM;
		goto clean_up;
	}

	err = gk20a_get_sgtable(d, &p_ucode_info->surface_desc.sgt,
				p_ucode_info->surface_desc.cpuva,
				p_ucode_info->surface_desc.iova,
				p_ucode_info->surface_desc.size);
	if (err) {
		nvhost_err(d, "failed to create sg table\n");
		goto clean_up;
	}

	p_buf = (u8 *)p_ucode_info->surface_desc.cpuva;
	if (!p_buf) {
		nvhost_err(d, "failed to map surface desc buffer");
		err = -ENOMEM;
		goto clean_up;
	}

	gr_gk20a_copy_ctxsw_ucode_inst(p_buf, &p_ucode_info->fecs,
		p_fecs_boot_desc, p_fecs_boot_image,
		g->gr.ctx_vars.ucode.fecs.inst.l,
		g->gr.ctx_vars.ucode.fecs.data.l);

	release_firmware(fecs_fw);
	fecs_fw = NULL;

	gr_gk20a_copy_ctxsw_ucode_inst(p_buf, &p_ucode_info->gpcs,
		p_gpcs_boot_desc, p_gpcs_boot_image,
		g->gr.ctx_vars.ucode.gpccs.inst.l,
		g->gr.ctx_vars.ucode.gpccs.data.l);

	release_firmware(gpccs_fw);
	gpccs_fw = NULL;

	err = gr_gk20a_init_ctxsw_ucode_vaspace(g);
	if (err)
		goto clean_up;

	gk20a_free_sgtable(&p_ucode_info->surface_desc.sgt);

	return 0;

 clean_up:
	if (p_ucode_info->ucode_gpuva)
		gk20a_gmmu_unmap(vm, p_ucode_info->ucode_gpuva,
			p_ucode_info->surface_desc.size, mem_flag_none);
	if (p_ucode_info->surface_desc.sgt)
		gk20a_free_sgtable(&p_ucode_info->surface_desc.sgt);
	if (p_ucode_info->surface_desc.cpuva)
		dma_free_attrs(d, p_ucode_info->surface_desc.size,
				p_ucode_info->surface_desc.cpuva,
				p_ucode_info->surface_desc.iova,
				&attrs);
	p_ucode_info->surface_desc.cpuva = NULL;
	p_ucode_info->surface_desc.iova = 0;

	release_firmware(gpccs_fw);
	gpccs_fw = NULL;
	release_firmware(fecs_fw);
	fecs_fw = NULL;

	return err;
}

static void gr_gk20a_load_falcon_bind_instblk(struct gk20a *g)
{
	struct gk20a_ctxsw_ucode_info *p_ucode_info = &g->ctxsw_ucode_info;
	int retries = 20;
	phys_addr_t inst_ptr;
	u32 val;

	while ((gk20a_readl(g, gr_fecs_ctxsw_status_1_r()) &
			gr_fecs_ctxsw_status_1_arb_busy_m()) && retries) {
		udelay(2);
		retries--;
	}
	if (!retries)
		nvhost_err(dev_from_gk20a(g), "arbiter idle timeout");

	gk20a_writel(g, gr_fecs_arb_ctx_adr_r(), 0x0);

	inst_ptr = p_ucode_info->inst_blk_desc.cpu_pa;
	gk20a_writel(g, gr_fecs_new_ctx_r(),
			gr_fecs_new_ctx_ptr_f(inst_ptr >> 12) |
			gr_fecs_new_ctx_target_m() |
			gr_fecs_new_ctx_valid_m());

	gk20a_writel(g, gr_fecs_arb_ctx_ptr_r(),
			gr_fecs_arb_ctx_ptr_ptr_f(inst_ptr >> 12) |
			gr_fecs_arb_ctx_ptr_target_m());

	gk20a_writel(g, gr_fecs_arb_ctx_cmd_r(), 0x7);

	/* Wait for arbiter command to complete */
	retries = 20;
	val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	while (gr_fecs_arb_ctx_cmd_cmd_v(val) && retries) {
		udelay(2);
		retries--;
		val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	}
	if (!retries)
		nvhost_err(dev_from_gk20a(g), "arbiter complete timeout");

	gk20a_writel(g, gr_fecs_current_ctx_r(),
			gr_fecs_current_ctx_ptr_f(inst_ptr >> 12) |
			gr_fecs_current_ctx_target_m() |
			gr_fecs_current_ctx_valid_m());
	/* Send command to arbiter to flush */
	gk20a_writel(g, gr_fecs_arb_ctx_cmd_r(), gr_fecs_arb_ctx_cmd_cmd_s());

	retries = 20;
	val = (gk20a_readl(g, gr_fecs_arb_ctx_cmd_r()));
	while (gr_fecs_arb_ctx_cmd_cmd_v(val) && retries) {
		udelay(2);
		retries--;
		val = gk20a_readl(g, gr_fecs_arb_ctx_cmd_r());
	}
	if (!retries)
		nvhost_err(dev_from_gk20a(g), "arbiter complete timeout");
}

static int gr_gk20a_load_ctxsw_ucode_inst(struct gk20a *g, u64 addr_base,
	struct gk20a_ctxsw_ucode_inst *p_inst, u32 reg_offset)
{
	u32 addr_code32;
	u32 addr_data32;
	u32 addr_load32;
	u32 dst = 0;
	u32 blocks;
	u32 b;

	addr_code32 = u64_lo32((addr_base + p_inst->code.offset) >> 8);
	addr_data32 = u64_lo32((addr_base + p_inst->data.offset) >> 8);
	addr_load32 = u64_lo32((addr_base + p_inst->boot.offset) >> 8);

	gk20a_writel(g, reg_offset + gr_fecs_dmactl_r(),
			gr_fecs_dmactl_require_ctx_f(0));

	/*
	 * Copy falcon bootloader header into dmem at offset 0.
	 * Configure dmem port 0 for auto-incrementing writes starting at dmem
	 * offset 0.
	 */
	gk20a_writel(g, reg_offset + gr_fecs_dmemc_r(0),
			gr_fecs_dmemc_offs_f(0) |
			gr_fecs_dmemc_blk_f(0) |
			gr_fecs_dmemc_aincw_f(1));

	/* Write out the actual data */
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), addr_code32);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), p_inst->code.size);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), addr_data32);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), p_inst->data.size);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), addr_code32);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);
	gk20a_writel(g, reg_offset + gr_fecs_dmemd_r(0), 0);

	blocks = ((p_inst->boot.size + 0xFF) & ~0xFF) >> 8;

	/*
	 * Set the base FB address for the DMA transfer. Subtract off the 256
	 * byte IMEM block offset such that the relative FB and IMEM offsets
	 * match, allowing the IMEM tags to be properly created.
	 */

	dst = p_inst->boot_imem_offset;
	gk20a_writel(g, reg_offset + gr_fecs_dmatrfbase_r(),
			(addr_load32 - (dst >> 8)));

	for (b = 0; b < blocks; b++) {
		/* Setup destination IMEM offset */
		gk20a_writel(g, reg_offset + gr_fecs_dmatrfmoffs_r(),
				dst + (b << 8));

		/* Setup source offset (relative to BASE) */
		gk20a_writel(g, reg_offset + gr_fecs_dmatrffboffs_r(),
				dst + (b << 8));

		gk20a_writel(g, reg_offset + gr_fecs_dmatrfcmd_r(),
				gr_fecs_dmatrfcmd_imem_f(0x01) |
				gr_fecs_dmatrfcmd_write_f(0x00) |
				gr_fecs_dmatrfcmd_size_f(0x06) |
				gr_fecs_dmatrfcmd_ctxdma_f(0));
	}

	/* Specify the falcon boot vector */
	gk20a_writel(g, reg_offset + gr_fecs_bootvec_r(),
			gr_fecs_bootvec_vec_f(p_inst->boot_entry));

	/* Write to CPUCTL to start the falcon */
	gk20a_writel(g, reg_offset + gr_fecs_cpuctl_r(),
			gr_fecs_cpuctl_startcpu_f(0x01));

	return 0;
}

static void gr_gk20a_load_falcon_with_bootloader(struct gk20a *g)
{
	struct gk20a_ctxsw_ucode_info *p_ucode_info = &g->ctxsw_ucode_info;
	u64 addr_base = p_ucode_info->ucode_gpuva;

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0), 0x0);

	gr_gk20a_load_falcon_bind_instblk(g);

	gr_gk20a_load_ctxsw_ucode_inst(g, addr_base,
		&g->ctxsw_ucode_info.fecs, 0);

	gr_gk20a_load_ctxsw_ucode_inst(g, addr_base,
		&g->ctxsw_ucode_info.gpcs,
		gr_gpcs_gpccs_falcon_hwcfg_r() -
		gr_fecs_falcon_hwcfg_r());
}

static int gr_gk20a_load_ctxsw_ucode(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 ret;

	nvhost_dbg_fn("");

	if (tegra_platform_is_linsim()) {
		gk20a_writel(g, gr_fecs_ctxsw_mailbox_r(7),
			gr_fecs_ctxsw_mailbox_value_f(0xc0de7777));
		gk20a_writel(g, gr_gpccs_ctxsw_mailbox_r(7),
			gr_gpccs_ctxsw_mailbox_value_f(0xc0de7777));
	}

	/*
	 * In case the gPMU falcon is not being used, revert to the old way of
	 * loading gr ucode, without the faster bootstrap routine.
	 */
	if (!support_gk20a_pmu()) {
		gr_gk20a_load_falcon_dmem(g);
		gr_gk20a_load_falcon_imem(g);
		gr_gk20a_start_falcon_ucode(g);
	} else {
		if (!gr->skip_ucode_init)
			gr_gk20a_init_ctxsw_ucode(g);
		gr_gk20a_load_falcon_with_bootloader(g);
		gr->skip_ucode_init = true;
	}

	ret = gr_gk20a_ctx_wait_ucode(g, 0, 0,
				      GR_IS_UCODE_OP_EQUAL,
				      eUcodeHandshakeInitComplete,
				      GR_IS_UCODE_OP_SKIP, 0);
	if (ret) {
		nvhost_err(dev_from_gk20a(g), "falcon ucode init timeout");
		return ret;
	}

	if (support_gk20a_pmu())
		gk20a_writel(g, gr_fecs_current_ctx_r(),
			gr_fecs_current_ctx_valid_false_f());

	gk20a_writel(g, gr_fecs_ctxsw_mailbox_clear_r(0), 0xffffffff);
	gk20a_writel(g, gr_fecs_method_data_r(), 0x7fffffff);
	gk20a_writel(g, gr_fecs_method_push_r(),
		     gr_fecs_method_push_adr_set_watchdog_timeout_f());

	nvhost_dbg_fn("done");
	return 0;
}

static int gr_gk20a_init_ctx_state(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 golden_ctx_image_size = 0;
	u32 zcull_ctx_image_size = 0;
	u32 pm_ctx_image_size = 0;
	u32 ret;
	struct fecs_method_op_gk20a op = {
		.mailbox = { .id = 0, .data = 0,
			     .clr = ~0, .ok = 0, .fail = 0},
		.method.data = 0,
		.cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
		.cond.fail = GR_IS_UCODE_OP_SKIP,
		};

	nvhost_dbg_fn("");
	op.method.addr = gr_fecs_method_push_adr_discover_image_size_v();
	op.mailbox.ret = &golden_ctx_image_size;
	ret = gr_gk20a_submit_fecs_method_op(g, op);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			   "query golden image size failed");
		return ret;
	}
	op.method.addr = gr_fecs_method_push_adr_discover_zcull_image_size_v();
	op.mailbox.ret = &zcull_ctx_image_size;
	ret = gr_gk20a_submit_fecs_method_op(g, op);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			   "query zcull ctx image size failed");
		return ret;
	}
	op.method.addr = gr_fecs_method_push_adr_discover_pm_image_size_v();
	op.mailbox.ret = &pm_ctx_image_size;
	ret = gr_gk20a_submit_fecs_method_op(g, op);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			   "query pm ctx image size failed");
		return ret;
	}

	if (!g->gr.ctx_vars.golden_image_size &&
	    !g->gr.ctx_vars.zcull_ctxsw_image_size) {
		g->gr.ctx_vars.golden_image_size = golden_ctx_image_size;
		g->gr.ctx_vars.zcull_ctxsw_image_size = zcull_ctx_image_size;
	} else {
		/* hw is different after railgating? */
		BUG_ON(g->gr.ctx_vars.golden_image_size != golden_ctx_image_size);
		BUG_ON(g->gr.ctx_vars.zcull_ctxsw_image_size != zcull_ctx_image_size);
	}

	g->gr.ctx_vars.priv_access_map_size = 512 * 1024;

	nvhost_dbg_fn("done");
	return 0;
}

static int gr_gk20a_alloc_global_ctx_buffers(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct mem_mgr *memmgr = mem_mgr_from_g(g);
	struct mem_handle *mem;
	u32 i, attr_buffer_size;

	u32 cb_buffer_size = gr_scc_bundle_cb_size_div_256b__prod_v() *
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v();

	u32 pagepool_buffer_size = gr_scc_pagepool_total_pages_hwmax_value_v() *
		gr_scc_pagepool_total_pages_byte_granularity_v();

	u32 attr_cb_default_size = gr_gpc0_ppc0_cbm_cfg_size_default_v();
	u32 alpha_cb_default_size = gr_gpc0_ppc0_cbm_cfg2_size_default_v();

	u32 attr_cb_size =
		attr_cb_default_size + (attr_cb_default_size >> 1);
	u32 alpha_cb_size =
		alpha_cb_default_size + (alpha_cb_default_size >> 1);

	u32 num_tpcs_per_pes = proj_scal_litter_num_tpcs_per_pes_v();
	u32 attr_max_size_per_tpc =
		gr_gpc0_ppc0_cbm_cfg_size_v(~0) / num_tpcs_per_pes;
	u32 alpha_max_size_per_tpc =
		gr_gpc0_ppc0_cbm_cfg2_size_v(~0) / num_tpcs_per_pes;


	nvhost_dbg_fn("");

	attr_cb_size =
		(attr_cb_size > attr_max_size_per_tpc) ?
			attr_max_size_per_tpc : attr_cb_size;
	attr_cb_default_size =
		(attr_cb_default_size > attr_cb_size) ?
			attr_cb_size : attr_cb_default_size;
	alpha_cb_size =
		(alpha_cb_size > alpha_max_size_per_tpc) ?
			alpha_max_size_per_tpc : alpha_cb_size;
	alpha_cb_default_size =
		(alpha_cb_default_size > alpha_cb_size) ?
			alpha_cb_size : alpha_cb_default_size;

	attr_buffer_size =
		(gr_gpc0_ppc0_cbm_cfg_size_granularity_v() * alpha_cb_size +
		 gr_gpc0_ppc0_cbm_cfg2_size_granularity_v() * alpha_cb_size) *
		 gr->gpc_count;

	nvhost_dbg_info("cb_buffer_size : %d", cb_buffer_size);

	mem = nvhost_memmgr_alloc(memmgr, cb_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  0);
	if (IS_ERR(mem))
		goto clean_up;

	gr->global_ctx_buffer[CIRCULAR].ref = mem;
	gr->global_ctx_buffer[CIRCULAR].size = cb_buffer_size;

	mem = nvhost_memmgr_alloc(memmgr, cb_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  NVMAP_HEAP_CARVEOUT_VPR);
	if (!IS_ERR(mem)) {
		gr->global_ctx_buffer[CIRCULAR_VPR].ref = mem;
		gr->global_ctx_buffer[CIRCULAR_VPR].size = cb_buffer_size;
	}

	nvhost_dbg_info("pagepool_buffer_size : %d", pagepool_buffer_size);

	mem = nvhost_memmgr_alloc(memmgr, pagepool_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  0);
	if (IS_ERR(mem))
		goto clean_up;

	gr->global_ctx_buffer[PAGEPOOL].ref = mem;
	gr->global_ctx_buffer[PAGEPOOL].size = pagepool_buffer_size;

	mem = nvhost_memmgr_alloc(memmgr, pagepool_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  NVMAP_HEAP_CARVEOUT_VPR);
	if (!IS_ERR(mem)) {
		gr->global_ctx_buffer[PAGEPOOL_VPR].ref = mem;
		gr->global_ctx_buffer[PAGEPOOL_VPR].size = pagepool_buffer_size;
	}

	nvhost_dbg_info("attr_buffer_size : %d", attr_buffer_size);

	mem = nvhost_memmgr_alloc(memmgr, attr_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  0);
	if (IS_ERR(mem))
		goto clean_up;

	gr->global_ctx_buffer[ATTRIBUTE].ref = mem;
	gr->global_ctx_buffer[ATTRIBUTE].size = attr_buffer_size;

	mem = nvhost_memmgr_alloc(memmgr, attr_buffer_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  NVMAP_HEAP_CARVEOUT_VPR);
	if (!IS_ERR(mem)) {
		gr->global_ctx_buffer[ATTRIBUTE_VPR].ref = mem;
		gr->global_ctx_buffer[ATTRIBUTE_VPR].size = attr_buffer_size;
	}

	nvhost_dbg_info("golden_image_size : %d",
		   gr->ctx_vars.golden_image_size);

	mem = nvhost_memmgr_alloc(memmgr, gr->ctx_vars.golden_image_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  0);
	if (IS_ERR(mem))
		goto clean_up;

	gr->global_ctx_buffer[GOLDEN_CTX].ref = mem;
	gr->global_ctx_buffer[GOLDEN_CTX].size =
		gr->ctx_vars.golden_image_size;

	nvhost_dbg_info("priv_access_map_size : %d",
		   gr->ctx_vars.priv_access_map_size);

	mem = nvhost_memmgr_alloc(memmgr, gr->ctx_vars.priv_access_map_size,
				  DEFAULT_ALLOC_ALIGNMENT,
				  DEFAULT_ALLOC_FLAGS,
				  0);
	if (IS_ERR(mem))
		goto clean_up;

	gr->global_ctx_buffer[PRIV_ACCESS_MAP].ref = mem;
	gr->global_ctx_buffer[PRIV_ACCESS_MAP].size =
		gr->ctx_vars.priv_access_map_size;

	nvhost_dbg_fn("done");
	return 0;

 clean_up:
	nvhost_err(dev_from_gk20a(g), "fail");
	for (i = 0; i < NR_GLOBAL_CTX_BUF; i++) {
		if (gr->global_ctx_buffer[i].ref) {
			nvhost_memmgr_put(memmgr,
					  gr->global_ctx_buffer[i].ref);
			memset(&gr->global_ctx_buffer[i],
				0, sizeof(struct mem_desc));
		}
	}
	return -ENOMEM;
}

static void gr_gk20a_free_global_ctx_buffers(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct mem_mgr *memmgr = mem_mgr_from_g(g);
	u32 i;

	for (i = 0; i < NR_GLOBAL_CTX_BUF; i++) {
		nvhost_memmgr_put(memmgr, gr->global_ctx_buffer[i].ref);
		memset(&gr->global_ctx_buffer[i], 0, sizeof(struct mem_desc));
	}

	nvhost_dbg_fn("done");
}

static int gr_gk20a_map_global_ctx_buffers(struct gk20a *g,
					struct channel_gk20a *c)
{
	struct vm_gk20a *ch_vm = c->vm;
	struct mem_mgr *memmgr = mem_mgr_from_g(g);
	struct mem_handle *handle_ref;
	u64 *g_bfr_va = c->ch_ctx.global_ctx_buffer_va;
	struct gr_gk20a *gr = &g->gr;
	u64 gpu_va;
	u32 i;
	nvhost_dbg_fn("");

	/* Circular Buffer */
	if (!c->vpr || (gr->global_ctx_buffer[CIRCULAR_VPR].ref == NULL))
		handle_ref = gr->global_ctx_buffer[CIRCULAR].ref;
	else
		handle_ref = gr->global_ctx_buffer[CIRCULAR_VPR].ref;

	gpu_va = gk20a_vm_map(ch_vm, memmgr, handle_ref,
			      /*offset_align, flags, kind*/
			      0, NVHOST_MAP_BUFFER_FLAGS_CACHEABLE_TRUE, 0,
			      NULL, false, mem_flag_none);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[CIRCULAR_VA] = gpu_va;

	/* Attribute Buffer */
	if (!c->vpr || (gr->global_ctx_buffer[ATTRIBUTE_VPR].ref == NULL))
		handle_ref = gr->global_ctx_buffer[ATTRIBUTE].ref;
	else
		handle_ref = gr->global_ctx_buffer[ATTRIBUTE_VPR].ref;

	gpu_va = gk20a_vm_map(ch_vm, memmgr, handle_ref,
			      /*offset_align, flags, kind*/
			      0, NVHOST_MAP_BUFFER_FLAGS_CACHEABLE_TRUE, 0,
			      NULL, false, mem_flag_none);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[ATTRIBUTE_VA] = gpu_va;

	/* Page Pool */
	if (!c->vpr || (gr->global_ctx_buffer[PAGEPOOL_VPR].ref == NULL))
		handle_ref = gr->global_ctx_buffer[PAGEPOOL].ref;
	else
		handle_ref = gr->global_ctx_buffer[PAGEPOOL_VPR].ref;

	gpu_va = gk20a_vm_map(ch_vm, memmgr, handle_ref,
			      /*offset_align, flags, kind*/
			      0, NVHOST_MAP_BUFFER_FLAGS_CACHEABLE_TRUE, 0,
			      NULL, false, mem_flag_none);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[PAGEPOOL_VA] = gpu_va;

	/* Golden Image */
	gpu_va = gk20a_vm_map(ch_vm, memmgr,
			      gr->global_ctx_buffer[GOLDEN_CTX].ref,
			      /*offset_align, flags, kind*/
			      0, 0, 0, NULL, false, mem_flag_none);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[GOLDEN_CTX_VA] = gpu_va;

	/* Priv register Access Map */
	gpu_va = gk20a_vm_map(ch_vm, memmgr,
			      gr->global_ctx_buffer[PRIV_ACCESS_MAP].ref,
			      /*offset_align, flags, kind*/
			      0, 0, 0, NULL, false,
			      mem_flag_none);
	if (!gpu_va)
		goto clean_up;
	g_bfr_va[PRIV_ACCESS_MAP_VA] = gpu_va;

	c->ch_ctx.global_ctx_buffer_mapped = true;
	return 0;

 clean_up:
	for (i = 0; i < NR_GLOBAL_CTX_BUF_VA; i++) {
		if (g_bfr_va[i]) {
			gk20a_vm_unmap(ch_vm, g_bfr_va[i]);
			g_bfr_va[i] = 0;
		}
	}
	return -ENOMEM;
}

static void gr_gk20a_unmap_global_ctx_buffers(struct channel_gk20a *c)
{
	struct vm_gk20a *ch_vm = c->vm;
	u64 *g_bfr_va = c->ch_ctx.global_ctx_buffer_va;
	u32 i;

	nvhost_dbg_fn("");

	for (i = 0; i < NR_GLOBAL_CTX_BUF_VA; i++) {
		if (g_bfr_va[i]) {
			gk20a_vm_unmap(ch_vm, g_bfr_va[i]);
			g_bfr_va[i] = 0;
		}
	}
	c->ch_ctx.global_ctx_buffer_mapped = false;
}

static int gr_gk20a_alloc_channel_gr_ctx(struct gk20a *g,
				struct channel_gk20a *c)
{
	struct gr_gk20a *gr = &g->gr;
	struct gr_ctx_desc *gr_ctx = &c->ch_ctx.gr_ctx;
	struct vm_gk20a *ch_vm = c->vm;
	struct device *d = dev_from_gk20a(g);
	struct sg_table *sgt;
	DEFINE_DMA_ATTRS(attrs);
	int err = 0;

	nvhost_dbg_fn("");

	if (gr->ctx_vars.buffer_size == 0)
		return 0;

	/* alloc channel gr ctx buffer */
	gr->ctx_vars.buffer_size = gr->ctx_vars.golden_image_size;
	gr->ctx_vars.buffer_total_size = gr->ctx_vars.golden_image_size;

	gr_ctx->size = gr->ctx_vars.buffer_total_size;
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	gr_ctx->pages = dma_alloc_attrs(d, gr_ctx->size,
				&gr_ctx->iova, GFP_KERNEL, &attrs);
	if (!gr_ctx->pages)
		return -ENOMEM;

	err = gk20a_get_sgtable_from_pages(d, &sgt, gr_ctx->pages,
			gr_ctx->iova, gr_ctx->size);
	if (err)
		goto err_free;

	gr_ctx->gpu_va = gk20a_gmmu_map(ch_vm, &sgt, gr_ctx->size,
				NVHOST_MAP_BUFFER_FLAGS_CACHEABLE_TRUE,
				mem_flag_none);
	if (!gr_ctx->gpu_va)
		goto err_free_sgt;

	gk20a_free_sgtable(&sgt);

	return 0;

 err_free_sgt:
	gk20a_free_sgtable(&sgt);
 err_free:
	dma_free_attrs(d, gr_ctx->size,
		gr_ctx->pages, gr_ctx->iova, &attrs);
	gr_ctx->pages = NULL;
	gr_ctx->iova = 0;

	return err;
}

static void gr_gk20a_free_channel_gr_ctx(struct channel_gk20a *c)
{
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	struct vm_gk20a *ch_vm = c->vm;
	struct gk20a *g = c->g;
	struct device *d = dev_from_gk20a(g);
	DEFINE_DMA_ATTRS(attrs);

	nvhost_dbg_fn("");

	if (!ch_ctx->gr_ctx.gpu_va)
		return;

	gk20a_gmmu_unmap(ch_vm, ch_ctx->gr_ctx.gpu_va,
			ch_ctx->gr_ctx.size, mem_flag_none);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	dma_free_attrs(d, ch_ctx->gr_ctx.size,
		ch_ctx->gr_ctx.pages, ch_ctx->gr_ctx.iova, &attrs);
	ch_ctx->gr_ctx.pages = NULL;
	ch_ctx->gr_ctx.iova = 0;
}

static int gr_gk20a_alloc_channel_patch_ctx(struct gk20a *g,
				struct channel_gk20a *c)
{
	struct patch_desc *patch_ctx = &c->ch_ctx.patch_ctx;
	struct device *d = dev_from_gk20a(g);
	struct vm_gk20a *ch_vm = c->vm;
	DEFINE_DMA_ATTRS(attrs);
	struct sg_table *sgt;
	int err = 0;

	nvhost_dbg_fn("");

	patch_ctx->size = 128 * sizeof(u32);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
	patch_ctx->pages = dma_alloc_attrs(d, patch_ctx->size,
				&patch_ctx->iova, GFP_KERNEL,
				&attrs);
	if (!patch_ctx->pages)
		return -ENOMEM;

	err = gk20a_get_sgtable_from_pages(d, &sgt, patch_ctx->pages,
			patch_ctx->iova, patch_ctx->size);
	if (err)
		goto err_free;

	patch_ctx->gpu_va = gk20a_gmmu_map(ch_vm, &sgt, patch_ctx->size,
					0, mem_flag_none);
	if (!patch_ctx->gpu_va)
		goto err_free_sgtable;

	gk20a_free_sgtable(&sgt);

	nvhost_dbg_fn("done");
	return 0;

 err_free_sgtable:
	gk20a_free_sgtable(&sgt);
 err_free:
	dma_free_attrs(d, patch_ctx->size,
		patch_ctx->pages, patch_ctx->iova, &attrs);
	patch_ctx->pages = NULL;
	patch_ctx->iova = 0;
	nvhost_err(dev_from_gk20a(g), "fail");
	return err;
}

static void gr_gk20a_unmap_channel_patch_ctx(struct channel_gk20a *c)
{
	struct patch_desc *patch_ctx = &c->ch_ctx.patch_ctx;
	struct vm_gk20a *ch_vm = c->vm;

	nvhost_dbg_fn("");

	if (patch_ctx->gpu_va)
		gk20a_gmmu_unmap(ch_vm, patch_ctx->gpu_va,
			patch_ctx->size, mem_flag_none);
	patch_ctx->gpu_va = 0;
	patch_ctx->data_count = 0;
}

static void gr_gk20a_free_channel_patch_ctx(struct channel_gk20a *c)
{
	struct patch_desc *patch_ctx = &c->ch_ctx.patch_ctx;
	struct gk20a *g = c->g;
	struct device *d = dev_from_gk20a(g);
	DEFINE_DMA_ATTRS(attrs);

	nvhost_dbg_fn("");

	gr_gk20a_unmap_channel_patch_ctx(c);

	if (patch_ctx->pages) {
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
		dma_free_attrs(d, patch_ctx->size,
			patch_ctx->pages, patch_ctx->iova, &attrs);
		patch_ctx->pages = NULL;
		patch_ctx->iova = 0;
	}
}

void gk20a_free_channel_ctx(struct channel_gk20a *c)
{
	gr_gk20a_unmap_global_ctx_buffers(c);
	gr_gk20a_free_channel_patch_ctx(c);
	gr_gk20a_free_channel_gr_ctx(c);

	/* zcull_ctx, pm_ctx */

	memset(&c->ch_ctx, 0, sizeof(struct channel_ctx_gk20a));

	c->num_objects = 0;
	c->first_init = false;
}

int gk20a_alloc_obj_ctx(struct channel_gk20a  *c,
			struct nvhost_alloc_obj_ctx_args *args)
{
	struct gk20a *g = c->g;
	struct channel_ctx_gk20a *ch_ctx = &c->ch_ctx;
	bool change_to_compute_mode = false;
	int err = 0;

	nvhost_dbg_fn("");

	/* an address space needs to have been bound at this point.*/
	if (!gk20a_channel_as_bound(c)) {
		nvhost_err(dev_from_gk20a(g),
			   "not bound to address space at time"
			   " of grctx allocation");
		return -EINVAL;
	}

	switch (args->class_num) {
	case KEPLER_COMPUTE_A:
		/* tbd: NV2080_CTRL_GPU_COMPUTE_MODE_RULES_EXCLUSIVE_COMPUTE */
		/* tbd: PDB_PROP_GRAPHICS_DISTINCT_3D_AND_COMPUTE_STATE_DEF  */
		change_to_compute_mode = true;
		break;
	case KEPLER_C:
	case FERMI_TWOD_A:
	case KEPLER_DMA_COPY_A:
		break;

	default:
		nvhost_err(dev_from_gk20a(g),
			   "invalid obj class 0x%x", args->class_num);
		err = -EINVAL;
		goto out;
	}

	/* allocate gr ctx buffer */
	if (ch_ctx->gr_ctx.pages == NULL) {
		err = gr_gk20a_alloc_channel_gr_ctx(g, c);
		if (err) {
			nvhost_err(dev_from_gk20a(g),
				"fail to allocate gr ctx buffer");
			goto out;
		}
		c->obj_class = args->class_num;
	} else {
		/*TBD: needs to be more subtle about which is being allocated
		* as some are allowed to be allocated along same channel */
		nvhost_err(dev_from_gk20a(g),
			"too many classes alloc'd on same channel");
		err = -EINVAL;
		goto out;
	}

	/* commit gr ctx buffer */
	err = gr_gk20a_commit_inst(c, ch_ctx->gr_ctx.gpu_va);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to commit gr ctx buffer");
		goto out;
	}

	/* allocate patch buffer */
	if (ch_ctx->patch_ctx.pages == NULL) {
		err = gr_gk20a_alloc_channel_patch_ctx(g, c);
		if (err) {
			nvhost_err(dev_from_gk20a(g),
				"fail to allocate patch buffer");
			goto out;
		}
	}

	/* map global buffer to channel gpu_va and commit */
	if (!ch_ctx->global_ctx_buffer_mapped) {
		err = gr_gk20a_map_global_ctx_buffers(g, c);
		if (err) {
			nvhost_err(dev_from_gk20a(g),
				"fail to map global ctx buffer");
			goto out;
		}
		gr_gk20a_elpg_protected_call(g,
			gr_gk20a_commit_global_ctx_buffers(g, c, true));
	}

	/* init golden image, ELPG enabled after this is done */
	err = gr_gk20a_init_golden_ctx_image(g, c);
	if (err) {
		nvhost_err(dev_from_gk20a(g),
			"fail to init golden ctx image");
		goto out;
	}

	/* load golden image */
	if (!c->first_init) {
		err = gr_gk20a_elpg_protected_call(g,
			gr_gk20a_load_golden_ctx_image(g, c));
		if (err) {
			nvhost_err(dev_from_gk20a(g),
				"fail to load golden ctx image");
			goto out;
		}
		c->first_init = true;
	}
	gk20a_mm_l2_invalidate(g);

	c->num_objects++;

	nvhost_dbg_fn("done");
	return 0;
out:
	/* 1. gr_ctx, patch_ctx and global ctx buffer mapping
	   can be reused so no need to release them.
	   2. golden image init and load is a one time thing so if
	   they pass, no need to undo. */
	nvhost_err(dev_from_gk20a(g), "fail");
	return err;
}

int gk20a_free_obj_ctx(struct channel_gk20a  *c,
		       struct nvhost_free_obj_ctx_args *args)
{
	unsigned long timeout = gk20a_get_gr_idle_timeout(c->g);

	nvhost_dbg_fn("");

	if (c->num_objects == 0)
		return 0;

	c->num_objects--;

	if (c->num_objects == 0) {
		c->first_init = false;
		gk20a_disable_channel(c,
			!c->hwctx->has_timedout,
			timeout);
		gr_gk20a_unmap_channel_patch_ctx(c);
	}

	return 0;
}

static void gk20a_remove_gr_support(struct gr_gk20a *gr)
{
	struct gk20a *g = gr->g;
	struct mem_mgr *memmgr = mem_mgr_from_g(g);
	struct device *d = dev_from_gk20a(g);

	nvhost_dbg_fn("");

	gr_gk20a_free_global_ctx_buffers(g);

	dma_free_coherent(d, gr->mmu_wr_mem.size,
		gr->mmu_wr_mem.cpuva, gr->mmu_wr_mem.iova);
	gr->mmu_wr_mem.cpuva = NULL;
	gr->mmu_wr_mem.iova = 0;
	dma_free_coherent(d, gr->mmu_rd_mem.size,
		gr->mmu_rd_mem.cpuva, gr->mmu_rd_mem.iova);
	gr->mmu_rd_mem.cpuva = NULL;
	gr->mmu_rd_mem.iova = 0;

	nvhost_memmgr_put(memmgr, gr->compbit_store.mem.ref);

	memset(&gr->mmu_wr_mem, 0, sizeof(struct mmu_desc));
	memset(&gr->mmu_rd_mem, 0, sizeof(struct mmu_desc));
	memset(&gr->compbit_store, 0, sizeof(struct compbit_store_desc));

	kfree(gr->gpc_tpc_count);
	kfree(gr->gpc_zcb_count);
	kfree(gr->gpc_ppc_count);
	kfree(gr->pes_tpc_count[0]);
	kfree(gr->pes_tpc_count[1]);
	kfree(gr->pes_tpc_mask[0]);
	kfree(gr->pes_tpc_mask[1]);
	kfree(gr->gpc_skip_mask);
	kfree(gr->map_tiles);
	gr->gpc_tpc_count = NULL;
	gr->gpc_zcb_count = NULL;
	gr->gpc_ppc_count = NULL;
	gr->pes_tpc_count[0] = NULL;
	gr->pes_tpc_count[1] = NULL;
	gr->pes_tpc_mask[0] = NULL;
	gr->pes_tpc_mask[1] = NULL;
	gr->gpc_skip_mask = NULL;
	gr->map_tiles = NULL;

	kfree(gr->ctx_vars.ucode.fecs.inst.l);
	kfree(gr->ctx_vars.ucode.fecs.data.l);
	kfree(gr->ctx_vars.ucode.gpccs.inst.l);
	kfree(gr->ctx_vars.ucode.gpccs.data.l);
	kfree(gr->ctx_vars.sw_bundle_init.l);
	kfree(gr->ctx_vars.sw_method_init.l);
	kfree(gr->ctx_vars.sw_ctx_load.l);
	kfree(gr->ctx_vars.sw_non_ctx_load.l);
	kfree(gr->ctx_vars.ctxsw_regs.sys.l);
	kfree(gr->ctx_vars.ctxsw_regs.gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.tpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.zcull_gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.ppc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_sys.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_gpc.l);
	kfree(gr->ctx_vars.ctxsw_regs.pm_tpc.l);

	kfree(gr->ctx_vars.local_golden_image);
	gr->ctx_vars.local_golden_image = NULL;

	nvhost_allocator_destroy(&gr->comp_tags);
}

static int gr_gk20a_init_gr_config(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 gpc_index, pes_index;
	u32 pes_tpc_mask;
	u32 pes_tpc_count;
	u32 pes_heavy_index;
	u32 gpc_new_skip_mask;
	u32 tmp;

	tmp = gk20a_readl(g, pri_ringmaster_enum_fbp_r());
	gr->num_fbps = pri_ringmaster_enum_fbp_count_v(tmp);

	tmp = gk20a_readl(g, top_num_gpcs_r());
	gr->max_gpc_count = top_num_gpcs_value_v(tmp);

	tmp = gk20a_readl(g, top_num_fbps_r());
	gr->max_fbps_count = top_num_fbps_value_v(tmp);

	tmp = gk20a_readl(g, top_tpc_per_gpc_r());
	gr->max_tpc_per_gpc_count = top_tpc_per_gpc_value_v(tmp);

	gr->max_tpc_count = gr->max_gpc_count * gr->max_tpc_per_gpc_count;

	tmp = gk20a_readl(g, top_num_fbps_r());
	gr->sys_count = top_num_fbps_value_v(tmp);

	tmp = gk20a_readl(g, pri_ringmaster_enum_gpc_r());
	gr->gpc_count = pri_ringmaster_enum_gpc_count_v(tmp);

	gr->pe_count_per_gpc = proj_scal_litter_num_pes_per_gpc_v();
	gr->max_zcull_per_gpc_count = proj_scal_litter_num_zcull_banks_v();

	if (!gr->gpc_count) {
		nvhost_err(dev_from_gk20a(g), "gpc_count==0!");
		goto clean_up;
	}

	gr->gpc_tpc_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_zcb_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_ppc_count = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->pes_tpc_count[0] = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->pes_tpc_count[1] = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->pes_tpc_mask[0] = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->pes_tpc_mask[1] = kzalloc(gr->gpc_count * sizeof(u32), GFP_KERNEL);
	gr->gpc_skip_mask =
		kzalloc(gr_pd_dist_skip_table__size_1_v() * 4 * sizeof(u32),
			GFP_KERNEL);

	if (!gr->gpc_tpc_count || !gr->gpc_zcb_count || !gr->gpc_ppc_count ||
	    !gr->pes_tpc_count[0] || !gr->pes_tpc_count[1] ||
	    !gr->pes_tpc_mask[0] || !gr->pes_tpc_mask[1] || !gr->gpc_skip_mask)
		goto clean_up;

	gr->ppc_count = 0;
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		tmp = gk20a_readl(g, gr_gpc0_fs_gpc_r());

		gr->gpc_tpc_count[gpc_index] =
			gr_gpc0_fs_gpc_num_available_tpcs_v(tmp);
		gr->tpc_count += gr->gpc_tpc_count[gpc_index];

		gr->gpc_zcb_count[gpc_index] =
			gr_gpc0_fs_gpc_num_available_zculls_v(tmp);
		gr->zcb_count += gr->gpc_zcb_count[gpc_index];

		gr->gpc_ppc_count[gpc_index] = gr->pe_count_per_gpc;
		gr->ppc_count += gr->gpc_ppc_count[gpc_index];
		for (pes_index = 0; pes_index < gr->pe_count_per_gpc; pes_index++) {

			tmp = gk20a_readl(g,
				gr_gpc0_gpm_pd_pes_tpc_id_mask_r(pes_index) +
				gpc_index * proj_gpc_stride_v());

			pes_tpc_mask = gr_gpc0_gpm_pd_pes_tpc_id_mask_mask_v(tmp);
			pes_tpc_count = count_bits(pes_tpc_mask);

			gr->pes_tpc_count[pes_index][gpc_index] = pes_tpc_count;
			gr->pes_tpc_mask[pes_index][gpc_index] = pes_tpc_mask;
		}

		gpc_new_skip_mask = 0;
		if (gr->pes_tpc_count[0][gpc_index] +
		    gr->pes_tpc_count[1][gpc_index] == 5) {
			pes_heavy_index =
				gr->pes_tpc_count[0][gpc_index] >
				gr->pes_tpc_count[1][gpc_index] ? 0 : 1;

			gpc_new_skip_mask =
				gr->pes_tpc_mask[pes_heavy_index][gpc_index] ^
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] &
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] - 1));

		} else if ((gr->pes_tpc_count[0][gpc_index] +
			    gr->pes_tpc_count[1][gpc_index] == 4) &&
			   (gr->pes_tpc_count[0][gpc_index] !=
			    gr->pes_tpc_count[1][gpc_index])) {
				pes_heavy_index =
				    gr->pes_tpc_count[0][gpc_index] >
				    gr->pes_tpc_count[1][gpc_index] ? 0 : 1;

			gpc_new_skip_mask =
				gr->pes_tpc_mask[pes_heavy_index][gpc_index] ^
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] &
				   (gr->pes_tpc_mask[pes_heavy_index][gpc_index] - 1));
		}
		gr->gpc_skip_mask[gpc_index] = gpc_new_skip_mask;
	}

	nvhost_dbg_info("fbps: %d", gr->num_fbps);
	nvhost_dbg_info("max_gpc_count: %d", gr->max_gpc_count);
	nvhost_dbg_info("max_fbps_count: %d", gr->max_fbps_count);
	nvhost_dbg_info("max_tpc_per_gpc_count: %d", gr->max_tpc_per_gpc_count);
	nvhost_dbg_info("max_zcull_per_gpc_count: %d", gr->max_zcull_per_gpc_count);
	nvhost_dbg_info("max_tpc_count: %d", gr->max_tpc_count);
	nvhost_dbg_info("sys_count: %d", gr->sys_count);
	nvhost_dbg_info("gpc_count: %d", gr->gpc_count);
	nvhost_dbg_info("pe_count_per_gpc: %d", gr->pe_count_per_gpc);
	nvhost_dbg_info("tpc_count: %d", gr->tpc_count);
	nvhost_dbg_info("ppc_count: %d", gr->ppc_count);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		nvhost_dbg_info("gpc_tpc_count[%d] : %d",
			   gpc_index, gr->gpc_tpc_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		nvhost_dbg_info("gpc_zcb_count[%d] : %d",
			   gpc_index, gr->gpc_zcb_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		nvhost_dbg_info("gpc_ppc_count[%d] : %d",
			   gpc_index, gr->gpc_ppc_count[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		nvhost_dbg_info("gpc_skip_mask[%d] : %d",
			   gpc_index, gr->gpc_skip_mask[gpc_index]);
	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		for (pes_index = 0;
		     pes_index < gr->pe_count_per_gpc;
		     pes_index++)
			nvhost_dbg_info("pes_tpc_count[%d][%d] : %d",
				   pes_index, gpc_index,
				   gr->pes_tpc_count[pes_index][gpc_index]);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
		for (pes_index = 0;
		     pes_index < gr->pe_count_per_gpc;
		     pes_index++)
			nvhost_dbg_info("pes_tpc_mask[%d][%d] : %d",
				   pes_index, gpc_index,
				   gr->pes_tpc_mask[pes_index][gpc_index]);

	gr->bundle_cb_default_size = gr_scc_bundle_cb_size_div_256b__prod_v();
	gr->min_gpm_fifo_depth = gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
	gr->bundle_cb_token_limit = gr_pd_ab_dist_cfg2_token_limit_init_v();
	gr->attrib_cb_default_size = gr_gpc0_ppc0_cbm_cfg_size_default_v();
	/* gk20a has a fixed beta CB RAM, don't alloc more */
	gr->attrib_cb_size = gr->attrib_cb_default_size;
	gr->alpha_cb_default_size = gr_gpc0_ppc0_cbm_cfg2_size_default_v();
	gr->alpha_cb_size = gr->alpha_cb_default_size + (gr->alpha_cb_default_size >> 1);
	gr->timeslice_mode = gr_gpcs_ppcs_cbm_cfg_timeslice_mode_enable_v();

	nvhost_dbg_info("bundle_cb_default_size: %d",
		   gr->bundle_cb_default_size);
	nvhost_dbg_info("min_gpm_fifo_depth: %d", gr->min_gpm_fifo_depth);
	nvhost_dbg_info("bundle_cb_token_limit: %d", gr->bundle_cb_token_limit);
	nvhost_dbg_info("attrib_cb_default_size: %d",
		   gr->attrib_cb_default_size);
	nvhost_dbg_info("attrib_cb_size: %d", gr->attrib_cb_size);
	nvhost_dbg_info("alpha_cb_default_size: %d", gr->alpha_cb_default_size);
	nvhost_dbg_info("alpha_cb_size: %d", gr->alpha_cb_size);
	nvhost_dbg_info("timeslice_mode: %d", gr->timeslice_mode);

	return 0;

clean_up:
	return -ENOMEM;
}

static int gr_gk20a_init_mmu_sw(struct gk20a *g, struct gr_gk20a *gr)
{
	struct device *d = dev_from_gk20a(g);

	gr->mmu_wr_mem_size = gr->mmu_rd_mem_size = 0x1000;

	gr->mmu_wr_mem.size = gr->mmu_wr_mem_size;
	gr->mmu_wr_mem.cpuva = dma_zalloc_coherent(d, gr->mmu_wr_mem_size,
					&gr->mmu_wr_mem.iova, GFP_KERNEL);
	if (!gr->mmu_wr_mem.cpuva)
		goto err;

	gr->mmu_rd_mem.size = gr->mmu_rd_mem_size;
	gr->mmu_rd_mem.cpuva = dma_zalloc_coherent(d, gr->mmu_rd_mem_size,
					&gr->mmu_rd_mem.iova, GFP_KERNEL);
	if (!gr->mmu_rd_mem.cpuva)
		goto err_free_wr_mem;
	return 0;

 err_free_wr_mem:
	dma_free_coherent(d, gr->mmu_wr_mem.size,
		gr->mmu_wr_mem.cpuva, gr->mmu_wr_mem.iova);
	gr->mmu_wr_mem.cpuva = NULL;
	gr->mmu_wr_mem.iova = 0;
 err:
	return -ENOMEM;
}

static u32 prime_set[18] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61 };

static int gr_gk20a_init_map_tiles(struct gk20a *g, struct gr_gk20a *gr)
{
	s32 comm_denom;
	s32 mul_factor;
	s32 *init_frac = NULL;
	s32 *init_err = NULL;
	s32 *run_err = NULL;
	s32 *sorted_num_tpcs = NULL;
	s32 *sorted_to_unsorted_gpc_map = NULL;
	u32 gpc_index;
	u32 gpc_mark = 0;
	u32 num_tpc;
	u32 max_tpc_count = 0;
	u32 swap;
	u32 tile_count;
	u32 index;
	bool delete_map = false;
	bool gpc_sorted;
	int ret = 0;

	init_frac = kzalloc(proj_scal_max_gpcs_v() * sizeof(s32), GFP_KERNEL);
	init_err = kzalloc(proj_scal_max_gpcs_v() * sizeof(s32), GFP_KERNEL);
	run_err = kzalloc(proj_scal_max_gpcs_v() * sizeof(s32), GFP_KERNEL);
	sorted_num_tpcs =
		kzalloc(proj_scal_max_gpcs_v() *
			proj_scal_max_tpc_per_gpc_v() * sizeof(s32),
			GFP_KERNEL);
	sorted_to_unsorted_gpc_map =
		kzalloc(proj_scal_max_gpcs_v() * sizeof(s32), GFP_KERNEL);

	if (!(init_frac && init_err && run_err && sorted_num_tpcs &&
	      sorted_to_unsorted_gpc_map)) {
		ret = -ENOMEM;
		goto clean_up;
	}

	gr->map_row_offset = INVALID_SCREEN_TILE_ROW_OFFSET;

	if (gr->tpc_count == 3)
		gr->map_row_offset = 2;
	else if (gr->tpc_count < 3)
		gr->map_row_offset = 1;
	else {
		gr->map_row_offset = 3;

		for (index = 1; index < 18; index++) {
			u32 prime = prime_set[index];
			if ((gr->tpc_count % prime) != 0) {
				gr->map_row_offset = prime;
				break;
			}
		}
	}

	switch (gr->tpc_count) {
	case 15:
		gr->map_row_offset = 6;
		break;
	case 14:
		gr->map_row_offset = 5;
		break;
	case 13:
		gr->map_row_offset = 2;
		break;
	case 11:
		gr->map_row_offset = 7;
		break;
	case 10:
		gr->map_row_offset = 6;
		break;
	case 7:
	case 5:
		gr->map_row_offset = 1;
		break;
	default:
		break;
	}

	if (gr->map_tiles) {
		if (gr->map_tile_count != gr->tpc_count)
			delete_map = true;

		for (tile_count = 0; tile_count < gr->map_tile_count; tile_count++) {
			if ((u32)gr->map_tiles[tile_count] >= gr->tpc_count)
				delete_map = true;
		}

		if (delete_map) {
			kfree(gr->map_tiles);
			gr->map_tiles = NULL;
			gr->map_tile_count = 0;
		}
	}

	if (gr->map_tiles == NULL) {
		gr->map_tile_count = proj_scal_max_gpcs_v();

		gr->map_tiles = kzalloc(proj_scal_max_gpcs_v() * sizeof(u8), GFP_KERNEL);
		if (gr->map_tiles == NULL) {
			ret = -ENOMEM;
			goto clean_up;
		}

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			sorted_num_tpcs[gpc_index] = gr->gpc_tpc_count[gpc_index];
			sorted_to_unsorted_gpc_map[gpc_index] = gpc_index;
		}

		gpc_sorted = false;
		while (!gpc_sorted) {
			gpc_sorted = true;
			for (gpc_index = 0; gpc_index < gr->gpc_count - 1; gpc_index++) {
				if (sorted_num_tpcs[gpc_index + 1] > sorted_num_tpcs[gpc_index]) {
					gpc_sorted = false;
					swap = sorted_num_tpcs[gpc_index];
					sorted_num_tpcs[gpc_index] = sorted_num_tpcs[gpc_index + 1];
					sorted_num_tpcs[gpc_index + 1] = swap;
					swap = sorted_to_unsorted_gpc_map[gpc_index];
					sorted_to_unsorted_gpc_map[gpc_index] =
						sorted_to_unsorted_gpc_map[gpc_index + 1];
					sorted_to_unsorted_gpc_map[gpc_index + 1] = swap;
				}
			}
		}

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++)
			if (gr->gpc_tpc_count[gpc_index] > max_tpc_count)
				max_tpc_count = gr->gpc_tpc_count[gpc_index];

		mul_factor = gr->gpc_count * max_tpc_count;
		if (mul_factor & 0x1)
			mul_factor = 2;
		else
			mul_factor = 1;

		comm_denom = gr->gpc_count * max_tpc_count * mul_factor;

		for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
			num_tpc = sorted_num_tpcs[gpc_index];

			init_frac[gpc_index] = num_tpc * gr->gpc_count * mul_factor;

			if (num_tpc != 0)
				init_err[gpc_index] = gpc_index * max_tpc_count * mul_factor - comm_denom/2;
			else
				init_err[gpc_index] = 0;

			run_err[gpc_index] = init_frac[gpc_index] + init_err[gpc_index];
		}

		while (gpc_mark < gr->tpc_count) {
			for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
				if ((run_err[gpc_index] * 2) >= comm_denom) {
					gr->map_tiles[gpc_mark++] = (u8)sorted_to_unsorted_gpc_map[gpc_index];
					run_err[gpc_index] += init_frac[gpc_index] - comm_denom;
				} else
					run_err[gpc_index] += init_frac[gpc_index];
			}
		}
	}

clean_up:
	kfree(init_frac);
	kfree(init_err);
	kfree(run_err);
	kfree(sorted_num_tpcs);
	kfree(sorted_to_unsorted_gpc_map);

	if (ret)
		nvhost_err(dev_from_gk20a(g), "fail");
	else
		nvhost_dbg_fn("done");

	return ret;
}

static int gr_gk20a_init_comptag(struct gk20a *g, struct gr_gk20a *gr)
{
	struct mem_mgr *memmgr = mem_mgr_from_g(g);

	/* max memory size (MB) to cover */
	u32 max_size = gr->max_comptag_mem;
	/* one tag line covers 128KB */
	u32 max_comptag_lines = max_size << 3;

	u32 hw_max_comptag_lines =
		ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_init_v();

	u32 cbc_param =
		gk20a_readl(g, ltc_ltcs_ltss_cbc_param_r());
	u32 comptags_per_cacheline =
		ltc_ltcs_ltss_cbc_param_comptags_per_cache_line_v(cbc_param);
	u32 slices_per_fbp =
		ltc_ltcs_ltss_cbc_param_slices_per_fbp_v(cbc_param);
	u32 cacheline_size =
		512 << ltc_ltcs_ltss_cbc_param_cache_line_size_v(cbc_param);

	u32 compbit_backing_size;
	int ret = 0;

	nvhost_dbg_fn("");

	if (max_comptag_lines == 0) {
		gr->compbit_store.mem.size = 0;
		return 0;
	}

	if (max_comptag_lines > hw_max_comptag_lines)
		max_comptag_lines = hw_max_comptag_lines;

	/* no hybird fb */
	compbit_backing_size =
		DIV_ROUND_UP(max_comptag_lines, comptags_per_cacheline) *
		cacheline_size * slices_per_fbp * gr->num_fbps;

	/* aligned to 2KB * num_fbps */
	compbit_backing_size +=
		gr->num_fbps << ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	/* must be a multiple of 64KB */
	compbit_backing_size = roundup(compbit_backing_size, 64*1024);

	max_comptag_lines =
		(compbit_backing_size * comptags_per_cacheline) /
		cacheline_size * slices_per_fbp * gr->num_fbps;

	if (max_comptag_lines > hw_max_comptag_lines)
		max_comptag_lines = hw_max_comptag_lines;

	nvhost_dbg_info("compbit backing store size : %d",
		compbit_backing_size);
	nvhost_dbg_info("max comptag lines : %d",
		max_comptag_lines);

	gr->compbit_store.mem.ref =
		nvhost_memmgr_alloc(memmgr, compbit_backing_size,
				    DEFAULT_ALLOC_ALIGNMENT,
				    DEFAULT_ALLOC_FLAGS,
				    0);
	if (IS_ERR(gr->compbit_store.mem.ref)) {
		nvhost_err(dev_from_gk20a(g), "failed to allocate"
			   "backing store for compbit : size %d",
			   compbit_backing_size);
		return PTR_ERR(gr->compbit_store.mem.ref);
	}
	gr->compbit_store.mem.size = compbit_backing_size;

	gr->compbit_store.mem.sgt =
		nvhost_memmgr_pin(memmgr, gr->compbit_store.mem.ref,
				dev_from_gk20a(g), mem_flag_none);
	if (IS_ERR(gr->compbit_store.mem.sgt)) {
		ret = PTR_ERR(gr->compbit_store.mem.sgt);
		goto clean_up;
	}
	gr->compbit_store.base_pa =
		gk20a_mm_iova_addr(gr->compbit_store.mem.sgt->sgl);

	nvhost_allocator_init(&gr->comp_tags, "comptag",
			      1, /* start */
			      max_comptag_lines - 1, /* length*/
			      1); /* align */

	return 0;

clean_up:
	if (gr->compbit_store.mem.sgt)
		nvhost_memmgr_free_sg_table(memmgr, gr->compbit_store.mem.ref,
				gr->compbit_store.mem.sgt);
	nvhost_memmgr_put(memmgr, gr->compbit_store.mem.ref);
	return ret;
}

enum gk20a_cbc_op {
	gk20a_cbc_op_clear,
	gk20a_cbc_op_clean,
	gk20a_cbc_op_invalidate,
};

static int gk20a_gr_cbc_ctrl(struct gk20a *g, enum gk20a_cbc_op op,
		u32 min, u32 max)
{
	int err = 0;
	struct gr_gk20a *gr = &g->gr;
	u32 fbp, slice, ctrl1, val, hw_op = 0;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	u32 slices_per_fbp =
		ltc_ltcs_ltss_cbc_param_slices_per_fbp_v(
			gk20a_readl(g, ltc_ltcs_ltss_cbc_param_r()));

	nvhost_dbg_fn("");

	if (gr->compbit_store.mem.size == 0)
		return 0;

	mutex_lock(&g->mm.l2_op_lock);

	if (op == gk20a_cbc_op_clear) {
		gk20a_writel(g, ltc_ltcs_ltss_cbc_ctrl2_r(),
			     ltc_ltcs_ltss_cbc_ctrl2_clear_lower_bound_f(min));
		gk20a_writel(g, ltc_ltcs_ltss_cbc_ctrl3_r(),
			     ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_f(max));
		hw_op = ltc_ltcs_ltss_cbc_ctrl1_clear_active_f();
	} else if (op == gk20a_cbc_op_clean) {
		hw_op = 1; /* TODO: register spec */
	} else if (op == gk20a_cbc_op_invalidate) {
		hw_op = 2; /* TODO: register spec */
	} else {
		BUG_ON(1);
	}

	gk20a_writel(g, ltc_ltcs_ltss_cbc_ctrl1_r(),
		     gk20a_readl(g, ltc_ltcs_ltss_cbc_ctrl1_r()) | hw_op);

	for (fbp = 0; fbp < gr->num_fbps; fbp++) {
		for (slice = 0; slice < slices_per_fbp; slice++) {

			delay = GR_IDLE_CHECK_DEFAULT;

			ctrl1 = ltc_ltc0_lts0_cbc_ctrl1_r() +
				fbp * proj_ltc_stride_v() +
				slice * proj_lts_stride_v();

			do {
				val = gk20a_readl(g, ctrl1);
				if (!(val & hw_op))
					break;

				usleep_range(delay, delay * 2);
				delay = min_t(u32, delay << 1,
					GR_IDLE_CHECK_MAX);

			} while (time_before(jiffies, end_jiffies) |
					!tegra_platform_is_silicon());

			if (!time_before(jiffies, end_jiffies)) {
				nvhost_err(dev_from_gk20a(g),
					   "comp tag clear timeout\n");
				err = -EBUSY;
				goto out;
			}
		}
	}

out:
	mutex_unlock(&g->mm.l2_op_lock);
	return 0;
}

int gk20a_gr_clear_comptags(struct gk20a *g, u32 min, u32 max)
{
	return gk20a_gr_cbc_ctrl(g, gk20a_cbc_op_clear, min, max);
}

int gk20a_gr_flush_comptags(struct gk20a *g, bool invalidate)
{
	int err = gk20a_gr_cbc_ctrl(g, gk20a_cbc_op_clean, 0, 0);
	if (invalidate)
		err |= gk20a_gr_cbc_ctrl(g, gk20a_cbc_op_invalidate, 0, 0);
	return err;
}

static int gr_gk20a_init_zcull(struct gk20a *g, struct gr_gk20a *gr)
{
	struct gr_zcull_gk20a *zcull = &gr->zcull;

	zcull->aliquot_width = gr->tpc_count * 16;
	zcull->aliquot_height = 16;

	zcull->width_align_pixels = gr->tpc_count * 16;
	zcull->height_align_pixels = 32;

	zcull->aliquot_size =
		zcull->aliquot_width * zcull->aliquot_height;

	/* assume no floor sweeping since we only have 1 tpc in 1 gpc */
	zcull->pixel_squares_by_aliquots =
		gr->zcb_count * 16 * 16 * gr->tpc_count /
		(gr->gpc_count * gr->gpc_tpc_count[0]);

	zcull->total_aliquots =
		gr_gpc0_zcull_total_ram_size_num_aliquots_f(
			gk20a_readl(g, gr_gpc0_zcull_total_ram_size_r()));

	return 0;
}

u32 gr_gk20a_get_ctxsw_zcull_size(struct gk20a *g, struct gr_gk20a *gr)
{
	/* assuming gr has already been initialized */
	return gr->ctx_vars.zcull_ctxsw_image_size;
}

int gr_gk20a_bind_ctxsw_zcull(struct gk20a *g, struct gr_gk20a *gr,
			struct channel_gk20a *c, u64 zcull_va, u32 mode)
{
	struct zcull_ctx_desc *zcull_ctx = &c->ch_ctx.zcull_ctx;

	zcull_ctx->ctx_sw_mode = mode;
	zcull_ctx->gpu_va = zcull_va;

	/* TBD: don't disable channel in sw method processing */
	return gr_gk20a_ctx_zcull_setup(g, c, true);
}

int gr_gk20a_get_zcull_info(struct gk20a *g, struct gr_gk20a *gr,
			struct gr_zcull_info *zcull_params)
{
	struct gr_zcull_gk20a *zcull = &gr->zcull;

	zcull_params->width_align_pixels = zcull->width_align_pixels;
	zcull_params->height_align_pixels = zcull->height_align_pixels;
	zcull_params->pixel_squares_by_aliquots =
		zcull->pixel_squares_by_aliquots;
	zcull_params->aliquot_total = zcull->total_aliquots;

	zcull_params->region_byte_multiplier =
		gr->gpc_count * gr_zcull_bytes_per_aliquot_per_gpu_v();
	zcull_params->region_header_size =
		proj_scal_litter_num_gpcs_v() *
		gr_zcull_save_restore_header_bytes_per_gpc_v();

	zcull_params->subregion_header_size =
		proj_scal_litter_num_gpcs_v() *
		gr_zcull_save_restore_subregion_header_bytes_per_gpc_v();

	zcull_params->subregion_width_align_pixels =
		gr->tpc_count * gr_gpc0_zcull_zcsize_width_subregion__multiple_v();
	zcull_params->subregion_height_align_pixels =
		gr_gpc0_zcull_zcsize_height_subregion__multiple_v();
	zcull_params->subregion_count = gr_zcull_subregion_qty_v();

	return 0;
}

static int gr_gk20a_add_zbc_color(struct gk20a *g, struct gr_gk20a *gr,
				struct zbc_entry *color_val, u32 index)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = f->engine_info + ENGINE_GR_GK20A;
	u32 i;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret;

	ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to disable gr engine activity\n");
		return ret;
	}

	ret = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to idle graphics\n");
		goto clean_up;
	}

	/* update l2 table */
	gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
			(gk20a_readl(g, ltc_ltcs_ltss_dstg_zbc_index_r()) &
			 ~ltc_ltcs_ltss_dstg_zbc_index_address_f(~0)) |
				ltc_ltcs_ltss_dstg_zbc_index_address_f(index +
					GK20A_STARTOF_ZBC_TABLE));

	for (i = 0; i < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(); i++)
		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(i),
			color_val->color_l2[i]);

	/* update ds table */
	gk20a_writel(g, gr_ds_zbc_color_r_r(),
		gr_ds_zbc_color_r_val_f(color_val->color_ds[0]));
	gk20a_writel(g, gr_ds_zbc_color_g_r(),
		gr_ds_zbc_color_g_val_f(color_val->color_ds[1]));
	gk20a_writel(g, gr_ds_zbc_color_b_r(),
		gr_ds_zbc_color_b_val_f(color_val->color_ds[2]));
	gk20a_writel(g, gr_ds_zbc_color_a_r(),
		gr_ds_zbc_color_a_val_f(color_val->color_ds[3]));

	gk20a_writel(g, gr_ds_zbc_color_fmt_r(),
		gr_ds_zbc_color_fmt_val_f(color_val->format));

	gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index + GK20A_STARTOF_ZBC_TABLE));

	/* trigger the write */
	gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_c_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

	/* update local copy */
	for (i = 0; i < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(); i++) {
		gr->zbc_col_tbl[index].color_l2[i] = color_val->color_l2[i];
		gr->zbc_col_tbl[index].color_ds[i] = color_val->color_ds[i];
	}
	gr->zbc_col_tbl[index].format = color_val->format;
	gr->zbc_col_tbl[index].ref_cnt++;

clean_up:
	ret = gk20a_fifo_enable_engine_activity(g, gr_info);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to enable gr engine activity\n");
	}

	return ret;
}

static int gr_gk20a_add_zbc_depth(struct gk20a *g, struct gr_gk20a *gr,
				struct zbc_entry *depth_val, u32 index)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = f->engine_info + ENGINE_GR_GK20A;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret;

	ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to disable gr engine activity\n");
		return ret;
	}

	ret = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to idle graphics\n");
		goto clean_up;
	}

	/* update l2 table */
	gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
			(gk20a_readl(g, ltc_ltcs_ltss_dstg_zbc_index_r()) &
			 ~ltc_ltcs_ltss_dstg_zbc_index_address_f(~0)) |
				ltc_ltcs_ltss_dstg_zbc_index_address_f(index +
					GK20A_STARTOF_ZBC_TABLE));

	gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(),
			depth_val->depth);

	/* update ds table */
	gk20a_writel(g, gr_ds_zbc_z_r(),
		gr_ds_zbc_z_val_f(depth_val->depth));

	gk20a_writel(g, gr_ds_zbc_z_fmt_r(),
		gr_ds_zbc_z_fmt_val_f(depth_val->format));

	gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index + GK20A_STARTOF_ZBC_TABLE));

	/* trigger the write */
	gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_z_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

	/* update local copy */
	gr->zbc_dep_tbl[index].depth = depth_val->depth;
	gr->zbc_dep_tbl[index].format = depth_val->format;
	gr->zbc_dep_tbl[index].ref_cnt++;

clean_up:
	ret = gk20a_fifo_enable_engine_activity(g, gr_info);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to enable gr engine activity\n");
	}

	return ret;
}

int gr_gk20a_add_zbc(struct gk20a *g, struct gr_gk20a *gr,
		     struct zbc_entry *zbc_val)
{
	struct zbc_color_table *c_tbl;
	struct zbc_depth_table *d_tbl;
	u32 i, ret = -ENOMEM;
	bool added = false;
	u32 entries;

	/* no endian swap ? */

	switch (zbc_val->type) {
	case GK20A_ZBC_TYPE_COLOR:
		/* search existing tables */
		for (i = 0; i < gr->max_used_color_index; i++) {

			c_tbl = &gr->zbc_col_tbl[i];

			if (c_tbl->ref_cnt && c_tbl->format == zbc_val->format &&
			    memcmp(c_tbl->color_ds, zbc_val->color_ds,
				sizeof(zbc_val->color_ds)) == 0) {

				if (memcmp(c_tbl->color_l2, zbc_val->color_l2,
				    sizeof(zbc_val->color_l2))) {
					nvhost_err(dev_from_gk20a(g),
						"zbc l2 and ds color don't match with existing entries");
					return -EINVAL;
				}
				added = true;
				c_tbl->ref_cnt++;
				ret = 0;
				break;
			}
		}
		/* add new table */
		if (!added &&
		    gr->max_used_color_index < GK20A_ZBC_TABLE_SIZE) {

			c_tbl =
			    &gr->zbc_col_tbl[gr->max_used_color_index];
			WARN_ON(c_tbl->ref_cnt != 0);

			ret = gr_gk20a_add_zbc_color(g, gr,
				zbc_val, gr->max_used_color_index);

			if (!ret)
				gr->max_used_color_index++;
		}
		break;
	case GK20A_ZBC_TYPE_DEPTH:
		/* search existing tables */
		for (i = 0; i < gr->max_used_depth_index; i++) {

			d_tbl = &gr->zbc_dep_tbl[i];

			if (d_tbl->ref_cnt &&
			    d_tbl->depth == zbc_val->depth &&
			    d_tbl->format == zbc_val->format) {
				added = true;
				d_tbl->ref_cnt++;
				ret = 0;
				break;
			}
		}
		/* add new table */
		if (!added &&
		    gr->max_used_depth_index < GK20A_ZBC_TABLE_SIZE) {

			d_tbl =
			    &gr->zbc_dep_tbl[gr->max_used_depth_index];
			WARN_ON(d_tbl->ref_cnt != 0);

			ret = gr_gk20a_add_zbc_depth(g, gr,
				zbc_val, gr->max_used_depth_index);

			if (!ret)
				gr->max_used_depth_index++;
		}
		break;
	default:
		nvhost_err(dev_from_gk20a(g),
			"invalid zbc table type %d", zbc_val->type);
		return -EINVAL;
	}

	if (!added && ret == 0) {
		/* update zbc for elpg only when new entry is added */
		entries = max(gr->max_used_color_index,
					gr->max_used_depth_index);
		pmu_save_zbc(g, entries);
	}

	return ret;
}

int gr_gk20a_clear_zbc_table(struct gk20a *g, struct gr_gk20a *gr)
{
	struct fifo_gk20a *f = &g->fifo;
	struct fifo_engine_info_gk20a *gr_info = f->engine_info + ENGINE_GR_GK20A;
	u32 i, j;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret;

	ret = gk20a_fifo_disable_engine_activity(g, gr_info, true);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to disable gr engine activity\n");
		return ret;
	}

	ret = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to idle graphics\n");
		goto clean_up;
	}

	for (i = 0; i < GK20A_ZBC_TABLE_SIZE; i++) {
		gr->zbc_col_tbl[i].format = 0;
		gr->zbc_col_tbl[i].ref_cnt = 0;

		gk20a_writel(g, gr_ds_zbc_color_fmt_r(),
			gr_ds_zbc_color_fmt_val_invalid_f());
		gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
			gr_ds_zbc_tbl_index_val_f(i + GK20A_STARTOF_ZBC_TABLE));

		/* trigger the write */
		gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
			gr_ds_zbc_tbl_ld_select_c_f() |
			gr_ds_zbc_tbl_ld_action_write_f() |
			gr_ds_zbc_tbl_ld_trigger_active_f());

		/* clear l2 table */
		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
			(gk20a_readl(g, ltc_ltcs_ltss_dstg_zbc_index_r()) &
			 ~ltc_ltcs_ltss_dstg_zbc_index_address_f(~0)) |
				ltc_ltcs_ltss_dstg_zbc_index_address_f(i +
					GK20A_STARTOF_ZBC_TABLE));

		for (j = 0; j < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(); j++) {
			gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(j), 0);
			gr->zbc_col_tbl[i].color_l2[j] = 0;
			gr->zbc_col_tbl[i].color_ds[j] = 0;
		}
	}
	gr->max_used_color_index = 0;
	gr->max_default_color_index = 0;

	for (i = 0; i < GK20A_ZBC_TABLE_SIZE; i++) {
		gr->zbc_dep_tbl[i].depth = 0;
		gr->zbc_dep_tbl[i].format = 0;
		gr->zbc_dep_tbl[i].ref_cnt = 0;

		gk20a_writel(g, gr_ds_zbc_z_fmt_r(),
			gr_ds_zbc_z_fmt_val_invalid_f());
		gk20a_writel(g, gr_ds_zbc_tbl_index_r(),
			gr_ds_zbc_tbl_index_val_f(i + GK20A_STARTOF_ZBC_TABLE));

		/* trigger the write */
		gk20a_writel(g, gr_ds_zbc_tbl_ld_r(),
			gr_ds_zbc_tbl_ld_select_z_f() |
			gr_ds_zbc_tbl_ld_action_write_f() |
			gr_ds_zbc_tbl_ld_trigger_active_f());

		/* clear l2 table */
		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
			(gk20a_readl(g, ltc_ltcs_ltss_dstg_zbc_index_r()) &
			 ~ltc_ltcs_ltss_dstg_zbc_index_address_f(~0)) |
				ltc_ltcs_ltss_dstg_zbc_index_address_f(i +
					GK20A_STARTOF_ZBC_TABLE));

		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(), 0);
	}
	gr->max_used_depth_index = 0;
	gr->max_default_depth_index = 0;

clean_up:
	ret = gk20a_fifo_enable_engine_activity(g, gr_info);
	if (ret) {
		nvhost_err(dev_from_gk20a(g),
			"failed to enable gr engine activity\n");
	}

	/* elpg stuff */

	return ret;
}

/* get a zbc table entry specified by index
 * return table size when type is invalid */
int gr_gk20a_query_zbc(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_query_params *query_params)
{
	u32 index = query_params->index_size;
	u32 i;

	switch (query_params->type) {
	case GK20A_ZBC_TYPE_INVALID:
		query_params->index_size = GK20A_ZBC_TABLE_SIZE;
		break;
	case GK20A_ZBC_TYPE_COLOR:
		if (index >= GK20A_ZBC_TABLE_SIZE) {
			nvhost_err(dev_from_gk20a(g),
				"invalid zbc color table index\n");
			return -EINVAL;
		}
		for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
			query_params->color_l2[i] =
				gr->zbc_col_tbl[index].color_l2[i];
			query_params->color_ds[i] =
				gr->zbc_col_tbl[index].color_ds[i];
		}
		query_params->format = gr->zbc_col_tbl[index].format;
		query_params->ref_cnt = gr->zbc_col_tbl[index].ref_cnt;
		break;
	case GK20A_ZBC_TYPE_DEPTH:
		if (index >= GK20A_ZBC_TABLE_SIZE) {
			nvhost_err(dev_from_gk20a(g),
				"invalid zbc depth table index\n");
			return -EINVAL;
		}
		query_params->depth = gr->zbc_dep_tbl[index].depth;
		query_params->format = gr->zbc_dep_tbl[index].format;
		query_params->ref_cnt = gr->zbc_dep_tbl[index].ref_cnt;
		break;
	default:
		nvhost_err(dev_from_gk20a(g),
				"invalid zbc table type\n");
		return -EINVAL;
	}

	return 0;
}

static int gr_gk20a_load_zbc_default_table(struct gk20a *g, struct gr_gk20a *gr)
{
	struct zbc_entry zbc_val;
	u32 i, err;

	/* load default color table */
	zbc_val.type = GK20A_ZBC_TYPE_COLOR;

	zbc_val.format = gr_ds_zbc_color_fmt_val_zero_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0;
		zbc_val.color_l2[i] = 0;
	}
	err = gr_gk20a_add_zbc(g, gr, &zbc_val);

	zbc_val.format = gr_ds_zbc_color_fmt_val_unorm_one_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0xffffffff;
		zbc_val.color_l2[i] = 0x3f800000;
	}
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	zbc_val.format = gr_ds_zbc_color_fmt_val_rf32_gf32_bf32_af32_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0;
		zbc_val.color_l2[i] = 0;
	}
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	zbc_val.format = gr_ds_zbc_color_fmt_val_rf32_gf32_bf32_af32_v();
	for (i = 0; i < GK20A_ZBC_COLOR_VALUE_SIZE; i++) {
		zbc_val.color_ds[i] = 0x3f800000;
		zbc_val.color_l2[i] = 0x3f800000;
	}
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	if (!err)
		gr->max_default_color_index = 4;
	else {
		nvhost_err(dev_from_gk20a(g),
			   "fail to load default zbc color table\n");
		return err;
	}

	/* load default depth table */
	zbc_val.type = GK20A_ZBC_TYPE_DEPTH;

	zbc_val.format = gr_ds_zbc_z_fmt_val_fp32_v();
	zbc_val.depth = 0;
	err = gr_gk20a_add_zbc(g, gr, &zbc_val);

	zbc_val.format = gr_ds_zbc_z_fmt_val_fp32_v();
	zbc_val.depth = 0x3f800000;
	err |= gr_gk20a_add_zbc(g, gr, &zbc_val);

	if (!err)
		gr->max_default_depth_index = 2;
	else {
		nvhost_err(dev_from_gk20a(g),
			   "fail to load default zbc depth table\n");
		return err;
	}

	return 0;
}

static int gr_gk20a_init_zbc(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 i, j;

	/* reset zbc clear */
	for (i = 0; i < GK20A_SIZEOF_ZBC_TABLE -
	    GK20A_STARTOF_ZBC_TABLE; i++) {
		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
			(gk20a_readl(g, ltc_ltcs_ltss_dstg_zbc_index_r()) &
			 ~ltc_ltcs_ltss_dstg_zbc_index_address_f(~0)) |
				ltc_ltcs_ltss_dstg_zbc_index_address_f(
					i + GK20A_STARTOF_ZBC_TABLE));
		for (j = 0; j < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v(); j++)
			gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(j), 0);
		gk20a_writel(g, ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(), 0);
	}

	gr_gk20a_clear_zbc_table(g, gr);

	gr_gk20a_load_zbc_default_table(g, gr);

	return 0;
}

int gk20a_gr_zbc_set_table(struct gk20a *g, struct gr_gk20a *gr,
			struct zbc_entry *zbc_val)
{
	nvhost_dbg_fn("");

	return gr_gk20a_elpg_protected_call(g,
		gr_gk20a_add_zbc(g, gr, zbc_val));
}

void gr_gk20a_init_blcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl;

	gate_ctrl = gk20a_readl(g, therm_gate_ctrl_r(engine));

	switch (mode) {
	case BLCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_run_f());
		break;
	case BLCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_auto_f());
		break;
	default:
		nvhost_err(dev_from_gk20a(g),
			"invalid blcg mode %d", mode);
		return;
	}

	gk20a_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);
}

void gr_gk20a_init_elcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl, idle_filter;

	gate_ctrl = gk20a_readl(g, therm_gate_ctrl_r(engine));

	switch (mode) {
	case ELCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_run_f());
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_pwr_m(),
				/* set elpg to auto to meet hw expectation */
				therm_gate_ctrl_eng_pwr_auto_f());
		break;
	case ELCG_STOP:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_stop_f());
		break;
	case ELCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_auto_f());
		break;
	default:
		nvhost_err(dev_from_gk20a(g),
			"invalid elcg mode %d", mode);
	}

	if (tegra_platform_is_linsim()) {
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_delay_after_m(),
			therm_gate_ctrl_eng_delay_after_f(4));
	}

	/* 2 * (1 << 9) = 1024 clks */
	gate_ctrl = set_field(gate_ctrl,
		therm_gate_ctrl_eng_idle_filt_exp_m(),
		therm_gate_ctrl_eng_idle_filt_exp_f(9));
	gate_ctrl = set_field(gate_ctrl,
		therm_gate_ctrl_eng_idle_filt_mant_m(),
		therm_gate_ctrl_eng_idle_filt_mant_f(2));
	gk20a_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);

	/* default fecs_idle_filter to 0 */
	idle_filter = gk20a_readl(g, therm_fecs_idle_filter_r());
	idle_filter &= ~therm_fecs_idle_filter_value_m();
	gk20a_writel(g, therm_fecs_idle_filter_r(), idle_filter);
	/* default hubmmu_idle_filter to 0 */
	idle_filter = gk20a_readl(g, therm_hubmmu_idle_filter_r());
	idle_filter &= ~therm_hubmmu_idle_filter_value_m();
	gk20a_writel(g, therm_hubmmu_idle_filter_r(), idle_filter);
}

static int gr_gk20a_zcull_init_hw(struct gk20a *g, struct gr_gk20a *gr)
{
	u32 gpc_index, gpc_tpc_count, gpc_zcull_count;
	u32 *zcull_map_tiles, *zcull_bank_counters;
	u32 map_counter;
	u32 rcp_conserv;
	u32 offset;
	bool floorsweep = false;

	if (!gr->map_tiles)
		return -1;

	zcull_map_tiles = kzalloc(proj_scal_max_gpcs_v() *
			proj_scal_max_tpc_per_gpc_v() * sizeof(u32), GFP_KERNEL);
	if (!zcull_map_tiles) {
		nvhost_err(dev_from_gk20a(g),
			"failed to allocate zcull temp buffers");
		return -ENOMEM;
	}
	zcull_bank_counters = kzalloc(proj_scal_max_gpcs_v() *
			proj_scal_max_tpc_per_gpc_v() * sizeof(u32), GFP_KERNEL);

	if (!zcull_bank_counters) {
		nvhost_err(dev_from_gk20a(g),
			"failed to allocate zcull temp buffers");
		kfree(zcull_map_tiles);
		return -ENOMEM;
	}

	for (map_counter = 0; map_counter < gr->tpc_count; map_counter++) {
		zcull_map_tiles[map_counter] =
			zcull_bank_counters[gr->map_tiles[map_counter]];
		zcull_bank_counters[gr->map_tiles[map_counter]]++;
	}

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map0_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_0_f(zcull_map_tiles[0]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_1_f(zcull_map_tiles[1]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_2_f(zcull_map_tiles[2]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_3_f(zcull_map_tiles[3]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_4_f(zcull_map_tiles[4]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_5_f(zcull_map_tiles[5]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_6_f(zcull_map_tiles[6]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_7_f(zcull_map_tiles[7]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map1_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_8_f(zcull_map_tiles[8]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_9_f(zcull_map_tiles[9]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_10_f(zcull_map_tiles[10]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_11_f(zcull_map_tiles[11]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_12_f(zcull_map_tiles[12]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_13_f(zcull_map_tiles[13]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_14_f(zcull_map_tiles[14]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_15_f(zcull_map_tiles[15]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map2_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_16_f(zcull_map_tiles[16]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_17_f(zcull_map_tiles[17]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_18_f(zcull_map_tiles[18]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_19_f(zcull_map_tiles[19]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_20_f(zcull_map_tiles[20]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_21_f(zcull_map_tiles[21]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_22_f(zcull_map_tiles[22]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_23_f(zcull_map_tiles[23]));

	gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map3_r(),
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_24_f(zcull_map_tiles[24]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_25_f(zcull_map_tiles[25]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_26_f(zcull_map_tiles[26]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_27_f(zcull_map_tiles[27]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_28_f(zcull_map_tiles[28]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_29_f(zcull_map_tiles[29]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_30_f(zcull_map_tiles[30]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_31_f(zcull_map_tiles[31]));

	kfree(zcull_map_tiles);
	kfree(zcull_bank_counters);

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		gpc_tpc_count = gr->gpc_tpc_count[gpc_index];
		gpc_zcull_count = gr->gpc_zcb_count[gpc_index];

		if (gpc_zcull_count != gr->max_zcull_per_gpc_count &&
		    gpc_zcull_count < gpc_tpc_count) {
			nvhost_err(dev_from_gk20a(g),
				"zcull_banks (%d) less than tpcs (%d) for gpc (%d)",
				gpc_zcull_count, gpc_tpc_count, gpc_index);
			return -EINVAL;
		}
		if (gpc_zcull_count != gr->max_zcull_per_gpc_count &&
		    gpc_zcull_count != 0)
			floorsweep = true;
	}

	/* 1.0f / 1.0f * gr_gpc0_zcull_sm_num_rcp_conservative__max_v() */
	rcp_conserv = gr_gpc0_zcull_sm_num_rcp_conservative__max_v();

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		offset = gpc_index * proj_gpc_stride_v();

		if (floorsweep) {
			gk20a_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(gr->map_row_offset) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					gr->max_zcull_per_gpc_count));
		} else {
			gk20a_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(gr->map_row_offset) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					gr->gpc_tpc_count[gpc_index]));
		}

		gk20a_writel(g, gr_gpc0_zcull_fs_r() + offset,
			gr_gpc0_zcull_fs_num_active_banks_f(gr->gpc_zcb_count[gpc_index]) |
			gr_gpc0_zcull_fs_num_sms_f(gr->tpc_count));

		gk20a_writel(g, gr_gpc0_zcull_sm_num_rcp_r() + offset,
			gr_gpc0_zcull_sm_num_rcp_conservative_f(rcp_conserv));
	}

	gk20a_writel(g, gr_gpcs_ppcs_wwdx_sm_num_rcp_r(),
		gr_gpcs_ppcs_wwdx_sm_num_rcp_conservative_f(rcp_conserv));

	return 0;
}

static void gk20a_gr_enable_gpc_exceptions(struct gk20a *g)
{
	/* enable tpc exception forwarding */
	gk20a_writel(g, gr_gpc0_tpc0_tpccs_tpc_exception_en_r(),
		gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f());

	/* enable gpc exception forwarding */
	gk20a_writel(g, gr_gpc0_gpccs_gpc_exception_en_r(),
		gr_gpc0_gpccs_gpc_exception_en_tpc_0_enabled_f());
}

static int gk20a_init_gr_setup_hw(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct aiv_list_gk20a *sw_ctx_load = &g->gr.ctx_vars.sw_ctx_load;
	struct av_list_gk20a *sw_bundle_init = &g->gr.ctx_vars.sw_bundle_init;
	struct av_list_gk20a *sw_method_init = &g->gr.ctx_vars.sw_method_init;
	u32 data;
	u32 addr_lo, addr_hi;
	u64 addr;
	u32 compbit_base_post_divide;
	u64 compbit_base_post_multiply64;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 fe_go_idle_timeout_save;
	u32 last_bundle_data = 0;
	u32 last_method_data = 0;
	u32 i, err;
	u32 l1c_dbg_reg_val;

	nvhost_dbg_fn("");

	/* slcg prod values */
	gr_gk20a_slcg_gr_load_gating_prod(g, g->slcg_enabled);
	gr_gk20a_slcg_perf_load_gating_prod(g, g->slcg_enabled);

	/* init mmu debug buffer */
	addr = NV_MC_SMMU_VADDR_TRANSLATE(gr->mmu_wr_mem.iova);
	addr_lo = u64_lo32(addr);
	addr_hi = u64_hi32(addr);
	addr = (addr_lo >> fb_mmu_debug_wr_addr_alignment_v()) |
		(addr_hi << (32 - fb_mmu_debug_wr_addr_alignment_v()));

	gk20a_writel(g, fb_mmu_debug_wr_r(),
		     fb_mmu_debug_wr_aperture_vid_mem_f() |
		     fb_mmu_debug_wr_vol_false_f() |
		     fb_mmu_debug_wr_addr_v(addr));

	addr = NV_MC_SMMU_VADDR_TRANSLATE(gr->mmu_rd_mem.iova);
	addr_lo = u64_lo32(addr);
	addr_hi = u64_hi32(addr);
	addr = (addr_lo >> fb_mmu_debug_rd_addr_alignment_v()) |
		(addr_hi << (32 - fb_mmu_debug_rd_addr_alignment_v()));

	gk20a_writel(g, fb_mmu_debug_rd_r(),
		     fb_mmu_debug_rd_aperture_vid_mem_f() |
		     fb_mmu_debug_rd_vol_false_f() |
		     fb_mmu_debug_rd_addr_v(addr));

	/* load gr floorsweeping registers */
	data = gk20a_readl(g, gr_gpc0_ppc0_pes_vsc_strem_r());
	data = set_field(data, gr_gpc0_ppc0_pes_vsc_strem_master_pe_m(),
			gr_gpc0_ppc0_pes_vsc_strem_master_pe_true_f());
	gk20a_writel(g, gr_gpc0_ppc0_pes_vsc_strem_r(), data);

	gr_gk20a_zcull_init_hw(g, gr);

	gr_gk20a_blcg_gr_load_gating_prod(g, g->blcg_enabled);
	gr_gk20a_pg_gr_load_gating_prod(g, true);

	if (g->elcg_enabled) {
		gr_gk20a_init_elcg_mode(g, ELCG_AUTO, ENGINE_GR_GK20A);
		gr_gk20a_init_elcg_mode(g, ELCG_AUTO, ENGINE_CE2_GK20A);
	} else {
		gr_gk20a_init_elcg_mode(g, ELCG_RUN, ENGINE_GR_GK20A);
		gr_gk20a_init_elcg_mode(g, ELCG_RUN, ENGINE_CE2_GK20A);
	}

	/* Bug 1340570: increase the clock timeout to avoid potential
	 * operation failure at high gpcclk rate. Default values are 0x400.
	 */
	gk20a_writel(g, pri_ringstation_sys_master_config_r(0x15), 0x800);
	gk20a_writel(g, pri_ringstation_gpc_master_config_r(0xa), 0x800);
	gk20a_writel(g, pri_ringstation_fbp_master_config_r(0x8), 0x800);

	/* enable fifo access */
	gk20a_writel(g, gr_gpfifo_ctl_r(),
		     gr_gpfifo_ctl_access_enabled_f() |
		     gr_gpfifo_ctl_semaphore_access_enabled_f());

	/* TBD: reload gr ucode when needed */

	/* enable interrupts */
	gk20a_writel(g, gr_intr_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_intr_en_r(), 0xFFFFFFFF);

	/* enable fecs error interrupts */
	gk20a_writel(g, gr_fecs_host_int_enable_r(),
		     gr_fecs_host_int_enable_fault_during_ctxsw_enable_f() |
		     gr_fecs_host_int_enable_umimp_firmware_method_enable_f() |
		     gr_fecs_host_int_enable_umimp_illegal_method_enable_f() |
		     gr_fecs_host_int_enable_watchdog_enable_f());

	/* enable exceptions */
	gk20a_writel(g, gr_fe_hww_esr_r(),
		     gr_fe_hww_esr_en_enable_f() |
		     gr_fe_hww_esr_reset_active_f());
	gk20a_writel(g, gr_memfmt_hww_esr_r(),
		     gr_memfmt_hww_esr_en_enable_f() |
		     gr_memfmt_hww_esr_reset_active_f());
	gk20a_writel(g, gr_scc_hww_esr_r(),
		     gr_scc_hww_esr_en_enable_f() |
		     gr_scc_hww_esr_reset_active_f());
	gk20a_writel(g, gr_mme_hww_esr_r(),
		     gr_mme_hww_esr_en_enable_f() |
		     gr_mme_hww_esr_reset_active_f());
	gk20a_writel(g, gr_pd_hww_esr_r(),
		     gr_pd_hww_esr_en_enable_f() |
		     gr_pd_hww_esr_reset_active_f());
	gk20a_writel(g, gr_sked_hww_esr_r(), /* enabled by default */
		     gr_sked_hww_esr_reset_active_f());
	gk20a_writel(g, gr_ds_hww_esr_r(),
		     gr_ds_hww_esr_en_enabled_f() |
		     gr_ds_hww_esr_reset_task_f());
	gk20a_writel(g, gr_ds_hww_report_mask_r(),
		     gr_ds_hww_report_mask_sph0_err_report_f() |
		     gr_ds_hww_report_mask_sph1_err_report_f() |
		     gr_ds_hww_report_mask_sph2_err_report_f() |
		     gr_ds_hww_report_mask_sph3_err_report_f() |
		     gr_ds_hww_report_mask_sph4_err_report_f() |
		     gr_ds_hww_report_mask_sph5_err_report_f() |
		     gr_ds_hww_report_mask_sph6_err_report_f() |
		     gr_ds_hww_report_mask_sph7_err_report_f() |
		     gr_ds_hww_report_mask_sph8_err_report_f() |
		     gr_ds_hww_report_mask_sph9_err_report_f() |
		     gr_ds_hww_report_mask_sph10_err_report_f() |
		     gr_ds_hww_report_mask_sph11_err_report_f() |
		     gr_ds_hww_report_mask_sph12_err_report_f() |
		     gr_ds_hww_report_mask_sph13_err_report_f() |
		     gr_ds_hww_report_mask_sph14_err_report_f() |
		     gr_ds_hww_report_mask_sph15_err_report_f() |
		     gr_ds_hww_report_mask_sph16_err_report_f() |
		     gr_ds_hww_report_mask_sph17_err_report_f() |
		     gr_ds_hww_report_mask_sph18_err_report_f() |
		     gr_ds_hww_report_mask_sph19_err_report_f() |
		     gr_ds_hww_report_mask_sph20_err_report_f() |
		     gr_ds_hww_report_mask_sph21_err_report_f() |
		     gr_ds_hww_report_mask_sph22_err_report_f() |
		     gr_ds_hww_report_mask_sph23_err_report_f());

	/* setup sm warp esr report masks */
	gk20a_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_error_report_f()	|
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_api_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_ret_empty_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_wrap_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_pc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_overflow_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_immc_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_sph_instr_combo_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param2_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f());

	/* setup sm global esr report mask */
	gk20a_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_l1_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_physical_stack_overflow_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_int_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_pause_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_single_step_complete_report_f());

	/* enable per GPC exceptions */
	gk20a_gr_enable_gpc_exceptions(g);

	/* TBD: ECC for L1/SM */
	/* TBD: enable per BE exceptions */

	/* reset and enable all exceptions */
	gk20a_writel(g, gr_exception_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception_en_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception1_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception1_en_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception2_r(), 0xFFFFFFFF);
	gk20a_writel(g, gr_exception2_en_r(), 0xFFFFFFFF);

	/* ignore status from some units */
	data = gk20a_readl(g, gr_status_mask_r());
	gk20a_writel(g, gr_status_mask_r(), data & gr->status_disable_mask);

	gr_gk20a_init_zbc(g, gr);

	{
		u64 compbit_base_post_divide64 = (gr->compbit_store.base_pa >>
				ltc_ltcs_ltss_cbc_base_alignment_shift_v());
		do_div(compbit_base_post_divide64, gr->num_fbps);
		compbit_base_post_divide = u64_lo32(compbit_base_post_divide64);
	}

	compbit_base_post_multiply64 = ((u64)compbit_base_post_divide *
		gr->num_fbps) << ltc_ltcs_ltss_cbc_base_alignment_shift_v();

	if (compbit_base_post_multiply64 < gr->compbit_store.base_pa)
		compbit_base_post_divide++;

	gk20a_writel(g, ltc_ltcs_ltss_cbc_base_r(),
		compbit_base_post_divide);

	nvhost_dbg(dbg_info | dbg_map | dbg_pte,
		   "compbit base.pa: 0x%x,%08x cbc_base:0x%08x\n",
		   (u32)(gr->compbit_store.base_pa>>32),
		   (u32)(gr->compbit_store.base_pa & 0xffffffff),
		   compbit_base_post_divide);

	/* load ctx init */
	for (i = 0; i < sw_ctx_load->count; i++)
		gk20a_writel(g, sw_ctx_load->l[i].addr,
			     sw_ctx_load->l[i].value);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

	/* save and disable fe_go_idle */
	fe_go_idle_timeout_save =
		gk20a_readl(g, gr_fe_go_idle_timeout_r());
	gk20a_writel(g, gr_fe_go_idle_timeout_r(),
		(fe_go_idle_timeout_save & gr_fe_go_idle_timeout_count_f(0)) |
		gr_fe_go_idle_timeout_count_disabled_f());

	/* override a few ctx state registers */
	gr_gk20a_commit_global_cb_manager(g, NULL, false);
	gr_gk20a_commit_global_timeslice(g, NULL, false);

	/* floorsweep anything left */
	gr_gk20a_ctx_state_floorsweep(g);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto restore_fe_go_idle;

	/* enable pipe mode override */
	gk20a_writel(g, gr_pipe_bundle_config_r(),
		gr_pipe_bundle_config_override_pipe_mode_enabled_f());

	/* load bundle init */
	err = 0;
	for (i = 0; i < sw_bundle_init->count; i++) {

		if (i == 0 || last_bundle_data != sw_bundle_init->l[i].value) {
			gk20a_writel(g, gr_pipe_bundle_data_r(),
				sw_bundle_init->l[i].value);
			last_bundle_data = sw_bundle_init->l[i].value;
		}

		gk20a_writel(g, gr_pipe_bundle_address_r(),
			     sw_bundle_init->l[i].addr);

		if (gr_pipe_bundle_address_value_v(sw_bundle_init->l[i].addr) ==
		    GR_GO_IDLE_BUNDLE)
			err |= gr_gk20a_wait_idle(g, end_jiffies,
					GR_IDLE_CHECK_DEFAULT);
		else if (0) { /* IS_SILICON */
			u32 delay = GR_IDLE_CHECK_DEFAULT;
			do {
				u32 gr_status = gk20a_readl(g, gr_status_r());

				if (gr_status_fe_method_lower_v(gr_status) ==
				    gr_status_fe_method_lower_idle_v())
					break;

				usleep_range(delay, delay * 2);
				delay = min_t(u32, delay << 1,
					GR_IDLE_CHECK_MAX);

			} while (time_before(jiffies, end_jiffies) |
					!tegra_platform_is_silicon());
		}
	}

	/* disable pipe mode override */
	gk20a_writel(g, gr_pipe_bundle_config_r(),
		     gr_pipe_bundle_config_override_pipe_mode_disabled_f());

restore_fe_go_idle:
	/* restore fe_go_idle */
	gk20a_writel(g, gr_fe_go_idle_timeout_r(), fe_go_idle_timeout_save);

	if (err || gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT))
		goto out;

	/* load method init */
	if (sw_method_init->count) {
		gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
			     sw_method_init->l[0].value);
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			     gr_pri_mme_shadow_raw_index_write_trigger_f() |
			     sw_method_init->l[0].addr);
		last_method_data = sw_method_init->l[0].value;
	}
	for (i = 1; i < sw_method_init->count; i++) {
		if (sw_method_init->l[i].value != last_method_data) {
			gk20a_writel(g, gr_pri_mme_shadow_raw_data_r(),
				sw_method_init->l[i].value);
			last_method_data = sw_method_init->l[i].value;
		}
		gk20a_writel(g, gr_pri_mme_shadow_raw_index_r(),
			gr_pri_mme_shadow_raw_index_write_trigger_f() |
			sw_method_init->l[i].addr);
	}

	gk20a_mm_l2_invalidate(g);

	/* turn on cya15 bit for a default val that missed the cut */
	l1c_dbg_reg_val = gk20a_readl(g, gr_gpc0_tpc0_l1c_dbg_r());
	l1c_dbg_reg_val |= gr_gpc0_tpc0_l1c_dbg_cya15_en_f();
	gk20a_writel(g, gr_gpc0_tpc0_l1c_dbg_r(), l1c_dbg_reg_val);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

out:
	nvhost_dbg_fn("done");
	return 0;
}

static int gk20a_init_gr_prepare(struct gk20a *g)
{
	u32 gpfifo_ctrl, pmc_en;
	u32 err = 0;

	/* disable fifo access */
	pmc_en = gk20a_readl(g, mc_enable_r());
	if (pmc_en & mc_enable_pgraph_enabled_f()) {
		gpfifo_ctrl = gk20a_readl(g, gr_gpfifo_ctl_r());
		gpfifo_ctrl &= ~gr_gpfifo_ctl_access_enabled_f();
		gk20a_writel(g, gr_gpfifo_ctl_r(), gpfifo_ctrl);
	}

	/* reset gr engine */
	gk20a_reset(g, mc_enable_pgraph_enabled_f()
			| mc_enable_blg_enabled_f()
			| mc_enable_perfmon_enabled_f());

	/* enable fifo access */
	gk20a_writel(g, gr_gpfifo_ctl_r(),
		gr_gpfifo_ctl_access_enabled_f() |
		gr_gpfifo_ctl_semaphore_access_enabled_f());

	if (!g->gr.ctx_vars.valid) {
		err = gr_gk20a_init_ctx_vars(g, &g->gr);
		if (err)
			nvhost_err(dev_from_gk20a(g),
				"fail to load gr init ctx");
	}
	return err;
}

static int gk20a_init_gr_reset_enable_hw(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct av_list_gk20a *sw_non_ctx_load = &g->gr.ctx_vars.sw_non_ctx_load;
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 i, err = 0;

	nvhost_dbg_fn("");

	/* enable interrupts */
	gk20a_writel(g, gr_intr_r(), ~0);
	gk20a_writel(g, gr_intr_en_r(), ~0);

	/* reset ctx switch state */
	gr_gk20a_ctx_reset(g, 0);

	/* clear scc ram */
	gk20a_writel(g, gr_scc_init_r(),
		gr_scc_init_ram_trigger_f());

	/* load non_ctx init */
	for (i = 0; i < sw_non_ctx_load->count; i++)
		gk20a_writel(g, sw_non_ctx_load->l[i].addr,
			sw_non_ctx_load->l[i].value);

	err = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (err)
		goto out;

	err = gr_gk20a_load_ctxsw_ucode(g, gr);
	if (err)
		goto out;

	/* this appears query for sw states but fecs actually init
	   ramchain, etc so this is hw init */
	err = gr_gk20a_init_ctx_state(g, gr);
	if (err)
		goto out;

out:
	if (err)
		nvhost_err(dev_from_gk20a(g), "fail");
	else
		nvhost_dbg_fn("done");

	return 0;
}

/*
 * XXX Merge this list with the debugger/profiler
 * session regops whitelists?
 */
static u32 wl_addr_gk20a[] = {
	/* this list must be sorted (low to high) */
	0x404468, /* gr_pri_mme_max_instructions       */
	0x418800, /* gr_pri_gpcs_setup_debug           */
	0x419a04, /* gr_pri_gpcs_tpcs_tex_lod_dbg      */
	0x419a08, /* gr_pri_gpcs_tpcs_tex_samp_dbg     */
	0x419e10, /* gr_pri_gpcs_tpcs_sm_dbgr_control0 */
	0x419f78, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
};

static int gr_gk20a_init_access_map(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	struct mem_handle *mem;
	void *data;
	u32 w, page, nr_pages =
		DIV_ROUND_UP(gr->ctx_vars.priv_access_map_size,
			     PAGE_SIZE);

	mem = gr->global_ctx_buffer[PRIV_ACCESS_MAP].ref;

	for (page = 0; page < nr_pages; page++) {
		data = nvhost_memmgr_kmap(mem, page);
		if (!data) {
			nvhost_err(dev_from_gk20a(g),
				   "failed to map priv access map memory");
			return -ENOMEM;
		}
		memset(data, 0x0, PAGE_SIZE);

		/* no good unless ARRAY_SIZE(w) == something small */
		for (w = 0; w < ARRAY_SIZE(wl_addr_gk20a); w++) {
			u32 map_bit, map_byte, map_shift;
			u32 map_page, pb_idx;
			map_bit = wl_addr_gk20a[w] >> 2;
			map_byte = map_bit >> 3;
			map_page = map_byte >> PAGE_SHIFT;
			if (map_page != page)
				continue;
			map_shift = map_bit & 0x7; /* i.e. 0-7 */
			pb_idx = (map_byte & ~PAGE_MASK);
			nvhost_dbg_info(
				"access map addr:0x%x pg:%d pb:%d bit:%d",
				wl_addr_gk20a[w], map_page, pb_idx, map_shift);
			((u8 *)data)[pb_idx] |= (1 << map_shift);
		}
		/* uncached on cpu side, so no need to flush? */
		nvhost_memmgr_kunmap(mem, page, data);
	}

	return 0;
}

static int gk20a_init_gr_setup_sw(struct gk20a *g)
{
	struct gr_gk20a *gr = &g->gr;
	int err;

	nvhost_dbg_fn("");

	if (gr->sw_ready) {
		nvhost_dbg_fn("skip init");
		return 0;
	}

	gr->g = g;

	err = gr_gk20a_init_gr_config(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_mmu_sw(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_map_tiles(g, gr);
	if (err)
		goto clean_up;

	if (tegra_cpu_is_asim())
		gr->max_comptag_mem = 1; /* MBs worth of comptag coverage */
	else {
		nvhost_dbg_info("total ram pages : %lu", totalram_pages);
		gr->max_comptag_mem = totalram_pages
					 >> (10 - (PAGE_SHIFT - 10));
	}
	err = gr_gk20a_init_comptag(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_zcull(g, gr);
	if (err)
		goto clean_up;

	err = gr_gk20a_alloc_global_ctx_buffers(g);
	if (err)
		goto clean_up;

	err = gr_gk20a_init_access_map(g);
	if (err)
		goto clean_up;

	mutex_init(&gr->ctx_mutex);
	spin_lock_init(&gr->ch_tlb_lock);

	gr->remove_support = gk20a_remove_gr_support;
	gr->sw_ready = true;

	nvhost_dbg_fn("done");
	return 0;

clean_up:
	nvhost_err(dev_from_gk20a(g), "fail");
	gk20a_remove_gr_support(gr);
	return err;
}

int gk20a_init_gr_support(struct gk20a *g)
{
	u32 err;

	nvhost_dbg_fn("");

	err = gk20a_init_gr_prepare(g);
	if (err)
		return err;

	/* this is required before gr_gk20a_init_ctx_state */
	mutex_init(&g->gr.fecs_mutex);

	err = gk20a_init_gr_reset_enable_hw(g);
	if (err)
		return err;

	err = gk20a_init_gr_setup_sw(g);
	if (err)
		return err;

	err = gk20a_init_gr_setup_hw(g);
	if (err)
		return err;

	return 0;
}

#define NVA297_SET_ALPHA_CIRCULAR_BUFFER_SIZE	0x02dc
#define NVA297_SET_CIRCULAR_BUFFER_SIZE		0x1280
#define NVA297_SET_SHADER_EXCEPTIONS		0x1528
#define NVA0C0_SET_SHADER_EXCEPTIONS		0x1528

#define NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE 0

struct gr_isr_data {
	u32 addr;
	u32 data_lo;
	u32 data_hi;
	u32 curr_ctx;
	u32 chid;
	u32 offset;
	u32 sub_chan;
	u32 class_num;
};

static void gk20a_gr_set_shader_exceptions(struct gk20a *g,
					   struct gr_isr_data *isr_data)
{
	u32 val;

	nvhost_dbg_fn("");

	if (isr_data->data_lo ==
	    NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE)
		val = 0;
	else
		val = ~0;

	gk20a_writel(g,
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
		val);
	gk20a_writel(g,
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		val);
}

static void gk20a_gr_set_circular_buffer_size(struct gk20a *g,
			struct gr_isr_data *isr_data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val, offset;
	u32 cb_size = isr_data->data_lo * 4;

	nvhost_dbg_fn("");

	if (cb_size > gr->attrib_cb_size)
		cb_size = gr->attrib_cb_size;

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size));

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = proj_gpc_stride_v() * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index);

			offset = gr_gpc0_ppc0_cbm_cfg_start_offset_v(val);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_size_m(),
				gr_gpc0_ppc0_cbm_cfg_size_f(cb_size *
					gr->pes_tpc_count[ppc_index][gpc_index]));
			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_start_offset_m(),
				(offset + 1));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_cfg_start_offset_m(),
				offset);

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);
		}
	}
}

static void gk20a_gr_set_alpha_circular_buffer_size(struct gk20a *g,
						struct gr_isr_data *isr_data)
{
	struct gr_gk20a *gr = &g->gr;
	u32 gpc_index, ppc_index, stride, val;
	u32 pd_ab_max_output;
	u32 alpha_cb_size = isr_data->data_lo * 4;

	nvhost_dbg_fn("");
	/* if (NO_ALPHA_BETA_TIMESLICE_SUPPORT_DEF)
		return; */

	if (alpha_cb_size > gr->alpha_cb_size)
		alpha_cb_size = gr->alpha_cb_size;

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
		 ~gr_ds_tga_constraintlogic_alpha_cbsize_f(~0)) |
		 gr_ds_tga_constraintlogic_alpha_cbsize_f(alpha_cb_size));

	pd_ab_max_output = alpha_cb_size *
		gr_gpc0_ppc0_cbm_cfg_size_granularity_v() /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	gk20a_writel(g, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output));

	for (gpc_index = 0; gpc_index < gr->gpc_count; gpc_index++) {
		stride = proj_gpc_stride_v() * gpc_index;

		for (ppc_index = 0; ppc_index < gr->gpc_ppc_count[gpc_index];
			ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_cfg2_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index);

			val = set_field(val, gr_gpc0_ppc0_cbm_cfg2_size_m(),
					gr_gpc0_ppc0_cbm_cfg2_size_f(alpha_cb_size *
						gr->pes_tpc_count[ppc_index][gpc_index]));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_cfg2_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);
		}
	}
}

void gk20a_gr_reset(struct gk20a *g)
{
	int err;
	err = gk20a_init_gr_prepare(g);
	BUG_ON(err);
	err = gk20a_init_gr_reset_enable_hw(g);
	BUG_ON(err);
	err = gk20a_init_gr_setup_hw(g);
	BUG_ON(err);
}

static int gk20a_gr_handle_illegal_method(struct gk20a *g,
					  struct gr_isr_data *isr_data)
{
	nvhost_dbg_fn("");

	if (isr_data->class_num == KEPLER_COMPUTE_A) {
		switch (isr_data->offset << 2) {
		case NVA0C0_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, isr_data);
			break;
		default:
			goto fail;
		}
	}

	if (isr_data->class_num == KEPLER_C) {
		switch (isr_data->offset << 2) {
		case NVA297_SET_SHADER_EXCEPTIONS:
			gk20a_gr_set_shader_exceptions(g, isr_data);
			break;
		case NVA297_SET_CIRCULAR_BUFFER_SIZE:
			gk20a_gr_set_circular_buffer_size(g, isr_data);
			break;
		case NVA297_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			gk20a_gr_set_alpha_circular_buffer_size(g, isr_data);
			break;
		default:
			goto fail;
		}
	}
	return 0;

fail:
	nvhost_err(dev_from_gk20a(g), "invalid method class 0x%08x"
		", offset 0x%08x address 0x%08x\n",
		isr_data->class_num, isr_data->offset, isr_data->addr);
	return -EINVAL;
}

static int gk20a_gr_handle_semaphore_timeout_pending(struct gk20a *g,
		  struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	nvhost_dbg_fn("");
	gk20a_set_error_notifier(ch->hwctx,
				NVHOST_CHANNEL_GR_SEMAPHORE_TIMEOUT);
	nvhost_err(dev_from_gk20a(g),
		   "gr semaphore timeout\n");
	return -EINVAL;
}

static int gk20a_gr_intr_illegal_notify_pending(struct gk20a *g,
		  struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	nvhost_dbg_fn("");
	gk20a_set_error_notifier(ch->hwctx,
				NVHOST_CHANNEL_GR_ILLEGAL_NOTIFY);
	/* This is an unrecoverable error, reset is needed */
	nvhost_err(dev_from_gk20a(g),
		   "gr semaphore timeout\n");
	return -EINVAL;
}

static int gk20a_gr_handle_illegal_class(struct gk20a *g,
					  struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	nvhost_dbg_fn("");
	gk20a_set_error_notifier(ch->hwctx,
				NVHOST_CHANNEL_GR_ERROR_SW_NOTIFY);
	nvhost_err(dev_from_gk20a(g),
		   "invalid class 0x%08x, offset 0x%08x",
		   isr_data->class_num, isr_data->offset);
	return -EINVAL;
}

static int gk20a_gr_handle_class_error(struct gk20a *g,
					  struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];
	nvhost_dbg_fn("");

	gk20a_set_error_notifier(ch->hwctx,
			NVHOST_CHANNEL_GR_ERROR_SW_NOTIFY);
	nvhost_err(dev_from_gk20a(g),
		   "class error 0x%08x, offset 0x%08x",
		   isr_data->class_num, isr_data->offset);
	return -EINVAL;
}

static int gk20a_gr_handle_semaphore_pending(struct gk20a *g,
					     struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];

	wake_up(&ch->semaphore_wq);

	return 0;
}

#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
static inline bool is_valid_cyclestats_bar0_offset_gk20a(struct gk20a *g,
							 u32 offset)
{
	/* support only 24-bit 4-byte aligned offsets */
	bool valid = !(offset & 0xFF000003);
	/* whitelist check */
	valid = valid &&
		is_bar0_global_offset_whitelisted_gk20a(offset);
	/* resource size check in case there was a problem
	 * with allocating the assumed size of bar0 */
	valid = valid &&
		offset < resource_size(g->reg_mem);
	return valid;
}
#endif

static int gk20a_gr_handle_notify_pending(struct gk20a *g,
					  struct gr_isr_data *isr_data)
{
	struct fifo_gk20a *f = &g->fifo;
	struct channel_gk20a *ch = &f->channel[isr_data->chid];

#if defined(CONFIG_TEGRA_GPU_CYCLE_STATS)
	void *virtual_address;
	u32 buffer_size;
	u32 offset;
	u32 new_offset;
	bool exit;
	struct share_buffer_head *sh_hdr;
	u32 raw_reg;
	u64 mask_orig;
	u64 v = 0;
	struct gk20a_cyclestate_buffer_elem *op_elem;
	/* GL will never use payload 0 for cycle state */
	if ((ch->cyclestate.cyclestate_buffer == NULL) || (isr_data->data_lo == 0))
		return 0;

	mutex_lock(&ch->cyclestate.cyclestate_buffer_mutex);

	virtual_address = ch->cyclestate.cyclestate_buffer;
	buffer_size = ch->cyclestate.cyclestate_buffer_size;
	offset = isr_data->data_lo;
	exit = false;
	while (!exit) {
		if (offset >= buffer_size) {
			WARN_ON(1);
			break;
		}

		sh_hdr = (struct share_buffer_head *)
			((char *)virtual_address + offset);

		if (sh_hdr->size < sizeof(struct share_buffer_head)) {
			WARN_ON(1);
			break;
		}
		new_offset = offset + sh_hdr->size;

		switch (sh_hdr->operation) {
		case OP_END:
			exit = true;
			break;

		case BAR0_READ32:
		case BAR0_WRITE32:
		{
			bool valid;
			op_elem =
				(struct gk20a_cyclestate_buffer_elem *)
					sh_hdr;
			valid = is_valid_cyclestats_bar0_offset_gk20a(g,
							op_elem->offset_bar0);
			if (!valid) {
				nvhost_err(dev_from_gk20a(g),
					   "invalid cycletstats op offset: 0x%x\n",
					   op_elem->offset_bar0);

				sh_hdr->failed = exit = true;
				break;
			}


			mask_orig =
				((1ULL <<
				  (op_elem->last_bit + 1))
				 -1)&~((1ULL <<
					op_elem->first_bit)-1);

			raw_reg =
				gk20a_readl(g,
					    op_elem->offset_bar0);

			switch (sh_hdr->operation) {
			case BAR0_READ32:
				op_elem->data =
					(raw_reg & mask_orig)
					>> op_elem->first_bit;
				break;

			case BAR0_WRITE32:
				v = 0;
				if ((unsigned int)mask_orig !=
				    (unsigned int)~0) {
					v = (unsigned int)
						(raw_reg & ~mask_orig);
				}

				v |= ((op_elem->data
				       << op_elem->first_bit)
				      & mask_orig);

				gk20a_writel(g,
					     op_elem->offset_bar0,
					     (unsigned int)v);
				break;
			default:
				/* nop ok?*/
				break;
			}
		}
		break;

		default:
			/* no operation content case */
			exit = true;
			break;
		}
		sh_hdr->completed = true;
		offset = new_offset;
	}
	mutex_unlock(&ch->cyclestate.cyclestate_buffer_mutex);
#endif
	nvhost_dbg_fn("");
	wake_up(&ch->notifier_wq);
	return 0;
}

/* Used by sw interrupt thread to translate current ctx to chid.
 * For performance, we don't want to go through 128 channels every time.
 * A small tlb is used here to cache translation */
static int gk20a_gr_get_chid_from_ctx(struct gk20a *g, u32 curr_ctx)
{
	struct fifo_gk20a *f = &g->fifo;
	struct gr_gk20a *gr = &g->gr;
	u32 chid = -1;
	u32 i;

	spin_lock(&gr->ch_tlb_lock);

	/* check cache first */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (gr->chid_tlb[i].curr_ctx == curr_ctx) {
			chid = gr->chid_tlb[i].hw_chid;
			goto unlock;
		}
	}

	/* slow path */
	for (chid = 0; chid < f->num_channels; chid++)
		if (f->channel[chid].in_use) {
			if ((u32)(f->channel[chid].inst_block.cpu_pa >>
				ram_in_base_shift_v()) ==
				gr_fecs_current_ctx_ptr_v(curr_ctx))
				break;
	}

	if (chid >= f->num_channels) {
		chid = -1;
		goto unlock;
	}

	/* add to free tlb entry */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (gr->chid_tlb[i].curr_ctx == 0) {
			gr->chid_tlb[i].curr_ctx = curr_ctx;
			gr->chid_tlb[i].hw_chid = chid;
			goto unlock;
		}
	}

	/* no free entry, flush one */
	gr->chid_tlb[gr->channel_tlb_flush_index].curr_ctx = curr_ctx;
	gr->chid_tlb[gr->channel_tlb_flush_index].hw_chid = chid;

	gr->channel_tlb_flush_index =
		(gr->channel_tlb_flush_index + 1) &
		(GR_CHANNEL_MAP_TLB_SIZE - 1);

unlock:
	spin_unlock(&gr->ch_tlb_lock);
	return chid;
}

static int gk20a_gr_lock_down_sm(struct gk20a *g, u32 global_esr_mask)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 delay = GR_IDLE_CHECK_DEFAULT;
	bool mmu_debug_mode_enabled = gk20a_mm_mmu_debug_mode_enabled(g);
	u32 dbgr_control0;

	nvhost_dbg(dbg_intr | dbg_gpu_dbg, "locking down SM");

	/* assert stop trigger */
	dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());
	dbgr_control0 |= gr_gpc0_tpc0_sm_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r(), dbgr_control0);

	/* wait for the sm to lock down */
	do {
		u32 global_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r());
		u32 warp_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_warp_esr_r());
		u32 dbgr_status0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_status0_r());
		bool locked_down =
			(gr_gpc0_tpc0_sm_dbgr_status0_locked_down_v(dbgr_status0) ==
			 gr_gpc0_tpc0_sm_dbgr_status0_locked_down_true_v());
		bool error_pending =
			(gr_gpc0_tpc0_sm_hww_warp_esr_error_v(warp_esr) !=
			 gr_gpc0_tpc0_sm_hww_warp_esr_error_none_v()) ||
			((global_esr & ~global_esr_mask) != 0);

		if (locked_down || !error_pending) {
			nvhost_dbg(dbg_intr | dbg_gpu_dbg, "locked down SM");

			/* de-assert stop trigger */
			dbgr_control0 &= ~gr_gpc0_tpc0_sm_dbgr_control0_stop_trigger_enable_f();
			gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r(), dbgr_control0);

			return 0;
		}

		/* if an mmu fault is pending and mmu debug mode is not
		 * enabled, the sm will never lock down. */
		if (!mmu_debug_mode_enabled && gk20a_fifo_mmu_fault_pending(g)) {
			nvhost_err(dev_from_gk20a(g), "mmu fault pending, sm will"
				   " never lock down!");
			return -EFAULT;
		}

		usleep_range(delay, delay * 2);
		delay = min_t(u32, delay << 1, GR_IDLE_CHECK_MAX);

	} while (time_before(jiffies, end_jiffies));

	nvhost_err(dev_from_gk20a(g), "timed out while trying to lock down SM");

	return -EAGAIN;
}

bool gk20a_gr_sm_debugger_attached(struct gk20a *g)
{
	u32 dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r());

	/* check if an sm debugger is attached */
	if (gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_v(dbgr_control0) ==
			gr_gpc0_tpc0_sm_dbgr_control0_debugger_mode_on_v())
		return true;

	return false;
}

static void gk20a_gr_clear_sm_hww(struct gk20a *g, u32 global_esr)
{
	gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r(), global_esr);

	/* clear the warp hww */
	gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_r(),
			gr_gpc0_tpc0_sm_hww_warp_esr_error_none_f());
}

static struct channel_gk20a *
channel_from_hw_chid(struct gk20a *g, u32 hw_chid)
{
	return g->fifo.channel+hw_chid;
}

static int gk20a_gr_handle_sm_exception(struct gk20a *g,
		struct gr_isr_data *isr_data)
{
	int ret = 0;
	bool do_warp_sync = false;
	/* these three interrupts don't require locking down the SM. They can
	 * be handled by usermode clients as they aren't fatal. Additionally,
	 * usermode clients may wish to allow some warps to execute while others
	 * are at breakpoints, as opposed to fatal errors where all warps should
	 * halt. */
	u32 global_mask = gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f()   |
			  gr_gpc0_tpc0_sm_hww_global_esr_bpt_pause_pending_f() |
			  gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f();
	u32 global_esr, warp_esr;
	bool sm_debugger_attached = gk20a_gr_sm_debugger_attached(g);
	struct channel_gk20a *fault_ch;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "");

	global_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r());
	warp_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_warp_esr_r());

	/* if an sm debugger is attached, disable forwarding of tpc exceptions.
	 * the debugger will reenable exceptions after servicing them. */
	if (sm_debugger_attached) {
		u32 tpc_exception_en = gk20a_readl(g, gr_gpc0_tpc0_tpccs_tpc_exception_en_r());
		tpc_exception_en &= ~gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f();
		gk20a_writel(g, gr_gpc0_tpc0_tpccs_tpc_exception_en_r(), tpc_exception_en);
		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "SM debugger attached");
	}

	/* if a debugger is present and an error has occurred, do a warp sync */
	if (sm_debugger_attached && ((warp_esr != 0) || ((global_esr & ~global_mask) != 0))) {
		nvhost_dbg(dbg_intr, "warp sync needed");
		do_warp_sync = true;
	}

	if (do_warp_sync) {
		ret = gk20a_gr_lock_down_sm(g, global_mask);
		if (ret) {
			nvhost_err(dev_from_gk20a(g), "sm did not lock down!\n");
			return ret;
		}
	}

	/* finally, signal any client waiting on an event */
	fault_ch = channel_from_hw_chid(g, isr_data->chid);
	if (fault_ch)
		gk20a_dbg_gpu_post_events(fault_ch);

	return ret;
}

static int gk20a_gr_handle_tpc_exception(struct gk20a *g,
		struct gr_isr_data *isr_data)
{
	int ret = 0;
	u32 tpc_exception = gk20a_readl(g, gr_gpcs_tpcs_tpccs_tpc_exception_r());

	nvhost_dbg(dbg_intr | dbg_gpu_dbg, "");

	/* check if an sm exeption is pending  */
	if (gr_gpcs_tpcs_tpccs_tpc_exception_sm_v(tpc_exception) ==
			gr_gpcs_tpcs_tpccs_tpc_exception_sm_pending_v()) {
		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "SM exception pending");
		ret = gk20a_gr_handle_sm_exception(g, isr_data);
	}

	return ret;
}

static int gk20a_gr_handle_gpc_exception(struct gk20a *g,
		struct gr_isr_data *isr_data)
{
	int ret = 0;
	u32 gpc_exception = gk20a_readl(g, gr_gpcs_gpccs_gpc_exception_r());

	nvhost_dbg(dbg_intr | dbg_gpu_dbg, "");

	/* check if tpc 0 has an exception */
	if (gr_gpcs_gpccs_gpc_exception_tpc_v(gpc_exception) ==
			gr_gpcs_gpccs_gpc_exception_tpc_0_pending_v()) {
		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "TPC exception pending");
		ret = gk20a_gr_handle_tpc_exception(g, isr_data);
	}

	return ret;
}

int gk20a_gr_isr(struct gk20a *g)
{
	struct gr_isr_data isr_data;
	u32 grfifo_ctl;
	u32 obj_table;
	int need_reset = 0;
	u32 gr_intr = gk20a_readl(g, gr_intr_r());

	nvhost_dbg_fn("");
	nvhost_dbg(dbg_intr, "pgraph intr %08x", gr_intr);

	if (!gr_intr)
		return 0;

	grfifo_ctl = gk20a_readl(g, gr_gpfifo_ctl_r());
	grfifo_ctl &= ~gr_gpfifo_ctl_semaphore_access_f(1);
	grfifo_ctl &= ~gr_gpfifo_ctl_access_f(1);

	gk20a_writel(g, gr_gpfifo_ctl_r(),
		grfifo_ctl | gr_gpfifo_ctl_access_f(0) |
		gr_gpfifo_ctl_semaphore_access_f(0));

	isr_data.addr = gk20a_readl(g, gr_trapped_addr_r());
	isr_data.data_lo = gk20a_readl(g, gr_trapped_data_lo_r());
	isr_data.data_hi = gk20a_readl(g, gr_trapped_data_hi_r());
	isr_data.curr_ctx = gk20a_readl(g, gr_fecs_current_ctx_r());
	isr_data.offset = gr_trapped_addr_mthd_v(isr_data.addr);
	isr_data.sub_chan = gr_trapped_addr_subch_v(isr_data.addr);
	obj_table = gk20a_readl(g,
		gr_fe_object_table_r(isr_data.sub_chan));
	isr_data.class_num = gr_fe_object_table_nvclass_v(obj_table);

	isr_data.chid =
		gk20a_gr_get_chid_from_ctx(g, isr_data.curr_ctx);
	if (isr_data.chid == -1) {
		nvhost_err(dev_from_gk20a(g), "invalid channel ctx 0x%08x",
			   isr_data.curr_ctx);
		goto clean_up;
	}

	nvhost_dbg(dbg_intr | dbg_gpu_dbg,
		"channel %d: addr 0x%08x, "
		"data 0x%08x 0x%08x,"
		"ctx 0x%08x, offset 0x%08x, "
		"subchannel 0x%08x, class 0x%08x",
		isr_data.chid, isr_data.addr,
		isr_data.data_hi, isr_data.data_lo,
		isr_data.curr_ctx, isr_data.offset,
		isr_data.sub_chan, isr_data.class_num);

	if (gr_intr & gr_intr_notify_pending_f()) {
		gk20a_gr_handle_notify_pending(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_notify_reset_f());
		gr_intr &= ~gr_intr_notify_pending_f();
	}

	if (gr_intr & gr_intr_semaphore_pending_f()) {
		gk20a_gr_handle_semaphore_pending(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_semaphore_reset_f());
		gr_intr &= ~gr_intr_semaphore_pending_f();
	}

	if (gr_intr & gr_intr_semaphore_timeout_pending_f()) {
		need_reset |= gk20a_gr_handle_semaphore_timeout_pending(g,
			&isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_semaphore_reset_f());
		gr_intr &= ~gr_intr_semaphore_pending_f();
	}

	if (gr_intr & gr_intr_illegal_notify_pending_f()) {
		need_reset |= gk20a_gr_intr_illegal_notify_pending(g,
			&isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_notify_reset_f());
		gr_intr &= ~gr_intr_illegal_notify_pending_f();
	}

	if (gr_intr & gr_intr_illegal_method_pending_f()) {
		need_reset |= gk20a_gr_handle_illegal_method(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_method_reset_f());
		gr_intr &= ~gr_intr_illegal_method_pending_f();
	}

	if (gr_intr & gr_intr_illegal_class_pending_f()) {
		need_reset |= gk20a_gr_handle_illegal_class(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_illegal_class_reset_f());
		gr_intr &= ~gr_intr_illegal_class_pending_f();
	}

	if (gr_intr & gr_intr_class_error_pending_f()) {
		need_reset |= gk20a_gr_handle_class_error(g, &isr_data);
		gk20a_writel(g, gr_intr_r(),
			gr_intr_class_error_reset_f());
		gr_intr &= ~gr_intr_class_error_pending_f();
	}

	/* this one happens if someone tries to hit a non-whitelisted
	 * register using set_falcon[4] */
	if (gr_intr & gr_intr_firmware_method_pending_f()) {
		need_reset |= true;
		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "firmware method intr pending\n");
		gk20a_writel(g, gr_intr_r(),
			gr_intr_firmware_method_reset_f());
		gr_intr &= ~gr_intr_firmware_method_pending_f();
	}

	if (gr_intr & gr_intr_exception_pending_f()) {
		u32 exception = gk20a_readl(g, gr_exception_r());
		struct fifo_gk20a *f = &g->fifo;
		struct channel_gk20a *ch = &f->channel[isr_data.chid];

		nvhost_dbg(dbg_intr | dbg_gpu_dbg, "exception %08x\n", exception);

		if (exception & gr_exception_fe_m()) {
			u32 fe = gk20a_readl(g, gr_fe_hww_esr_r());
			nvhost_dbg(dbg_intr, "fe warning %08x\n", fe);
			gk20a_writel(g, gr_fe_hww_esr_r(), fe);
		}

		/* check if a gpc exception has occurred */
		if (exception & gr_exception_gpc_m() && need_reset == 0) {
			u32 exception1 = gk20a_readl(g, gr_exception1_r());
			u32 global_esr = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r());

			nvhost_dbg(dbg_intr | dbg_gpu_dbg, "GPC exception pending");

			/* if no sm debugger is present, clean up the channel */
			if (!gk20a_gr_sm_debugger_attached(g)) {
				nvhost_dbg(dbg_intr | dbg_gpu_dbg,
					   "SM debugger not attached, clearing interrupt");
				need_reset |= -EFAULT;
			} else {
				/* check if gpc 0 has an exception */
				if (exception1 & gr_exception1_gpc_0_pending_f())
					need_reset |= gk20a_gr_handle_gpc_exception(g, &isr_data);
				/* clear the hwws, also causes tpc and gpc
				 * exceptions to be cleared */
				gk20a_gr_clear_sm_hww(g, global_esr);
			}

			if (need_reset)
				gk20a_set_error_notifier(ch->hwctx,
					NVHOST_CHANNEL_GR_ERROR_SW_NOTIFY);
		}

		gk20a_writel(g, gr_intr_r(), gr_intr_exception_reset_f());
		gr_intr &= ~gr_intr_exception_pending_f();
	}

	if (need_reset)
		gk20a_fifo_recover(g, BIT(ENGINE_GR_GK20A), true);

clean_up:
	gk20a_writel(g, gr_gpfifo_ctl_r(),
		grfifo_ctl | gr_gpfifo_ctl_access_f(1) |
		gr_gpfifo_ctl_semaphore_access_f(1));

	if (gr_intr)
		nvhost_err(dev_from_gk20a(g),
			   "unhandled gr interrupt 0x%08x", gr_intr);

	return 0;
}

int gk20a_gr_nonstall_isr(struct gk20a *g)
{
	u32 gr_intr = gk20a_readl(g, gr_intr_nonstall_r());
	u32 clear_intr = 0;

	nvhost_dbg(dbg_intr, "pgraph nonstall intr %08x", gr_intr);

	if (gr_intr & gr_intr_nonstall_trap_pending_f()) {
		gk20a_channel_semaphore_wakeup(g);
		clear_intr |= gr_intr_nonstall_trap_pending_f();
	}

	gk20a_writel(g, gr_intr_nonstall_r(), clear_intr);

	return 0;
}

int gr_gk20a_fecs_get_reglist_img_size(struct gk20a *g, u32 *size)
{
	BUG_ON(size == NULL);
	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a) {
			   .mailbox.id = 0,
			   .mailbox.data = 0,
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_discover_reglist_image_size_v(),
			   .mailbox.ret = size,
			   .cond.ok = GR_IS_UCODE_OP_NOT_EQUAL,
			   .mailbox.ok = 0,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0});
}

int gr_gk20a_fecs_set_reglist_bind_inst(struct gk20a *g, phys_addr_t addr)
{
	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a){
			   .mailbox.id = 4,
			   .mailbox.data = (gr_fecs_current_ctx_ptr_f(addr >> 12) |
					    gr_fecs_current_ctx_valid_f(1) |
					    gr_fecs_current_ctx_target_vid_mem_f()),
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_set_reglist_bind_instance_v(),
			   .mailbox.ret = NULL,
			   .cond.ok = GR_IS_UCODE_OP_EQUAL,
			   .mailbox.ok = 1,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0});
}

int gr_gk20a_fecs_set_reglist_virual_addr(struct gk20a *g, u64 pmu_va)
{
	return gr_gk20a_submit_fecs_method_op(g,
		   (struct fecs_method_op_gk20a) {
			   .mailbox.id = 4,
			   .mailbox.data = u64_lo32(pmu_va >> 8),
			   .mailbox.clr = ~0,
			   .method.data = 1,
			   .method.addr = gr_fecs_method_push_adr_set_reglist_virtual_address_v(),
			   .mailbox.ret = NULL,
			   .cond.ok = GR_IS_UCODE_OP_EQUAL,
			   .mailbox.ok = 1,
			   .cond.fail = GR_IS_UCODE_OP_SKIP,
			   .mailbox.fail = 0});
}

int gk20a_gr_suspend(struct gk20a *g)
{
	unsigned long end_jiffies = jiffies +
		msecs_to_jiffies(gk20a_get_gr_idle_timeout(g));
	u32 ret = 0;

	nvhost_dbg_fn("");

	ret = gr_gk20a_wait_idle(g, end_jiffies, GR_IDLE_CHECK_DEFAULT);
	if (ret)
		return ret;

	gk20a_writel(g, gr_gpfifo_ctl_r(),
		gr_gpfifo_ctl_access_disabled_f());

	/* disable gr intr */
	gk20a_writel(g, gr_intr_r(), 0);
	gk20a_writel(g, gr_intr_en_r(), 0);

	/* disable all exceptions */
	gk20a_writel(g, gr_exception_r(), 0);
	gk20a_writel(g, gr_exception_en_r(), 0);
	gk20a_writel(g, gr_exception1_r(), 0);
	gk20a_writel(g, gr_exception1_en_r(), 0);
	gk20a_writel(g, gr_exception2_r(), 0);
	gk20a_writel(g, gr_exception2_en_r(), 0);

	gk20a_gr_flush_channel_tlb(&g->gr);

	nvhost_dbg_fn("done");
	return ret;
}

static int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       bool is_quad, u32 quad,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset);

/* This function will decode a priv address and return the partition type and numbers. */
int gr_gk20a_decode_priv_addr(struct gk20a *g, u32 addr,
			      int  *addr_type, /* enum ctxsw_addr_type */
			      u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *be_num,
			      u32 *broadcast_flags)
{
	u32 gpc_addr;
	u32 ppc_address;
	u32 ppc_broadcast_addr;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

	/* setup defaults */
	ppc_address = 0;
	ppc_broadcast_addr = 0;
	*addr_type = CTXSW_ADDR_TYPE_SYS;
	*broadcast_flags = PRI_BROADCAST_FLAGS_NONE;
	*gpc_num = 0;
	*tpc_num = 0;
	*ppc_num = 0;
	*be_num  = 0;

	if (pri_is_gpc_addr(addr)) {
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		gpc_addr = pri_gpccs_addr_mask(addr);
		if (pri_is_gpc_addr_shared(addr)) {
			*addr_type = CTXSW_ADDR_TYPE_GPC;
			*broadcast_flags |= PRI_BROADCAST_FLAGS_GPC;
		} else
			*gpc_num = pri_get_gpc_num(addr);

		if (pri_is_tpc_addr(gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_TPC;
			if (pri_is_tpc_addr_shared(gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_TPC;
				return 0;
			}
			*tpc_num = pri_get_tpc_num(gpc_addr);
		}
		return 0;
	} else if (pri_is_be_addr(addr)) {
		*addr_type = CTXSW_ADDR_TYPE_BE;
		if (pri_is_be_addr_shared(addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_BE;
			return 0;
		}
		*be_num = pri_get_be_num(addr);
		return 0;
	} else {
		*addr_type = CTXSW_ADDR_TYPE_SYS;
		return 0;
	}
	/* PPC!?!?!?! */

	/*NOTREACHED*/
	return -EINVAL;
}

static int gr_gk20a_split_ppc_broadcast_addr(struct gk20a *g, u32 addr,
				      u32 gpc_num,
				      u32 *priv_addr_table, u32 *t)
{
    u32 ppc_num;

    nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

    for (ppc_num = 0; ppc_num < g->gr.pe_count_per_gpc; ppc_num++)
	    priv_addr_table[(*t)++] = pri_ppc_addr(pri_ppccs_addr_mask(addr),
						   gpc_num, ppc_num);

    return 0;
}

/*
 * The context buffer is indexed using BE broadcast addresses and GPC/TPC
 * unicast addresses. This function will convert a BE unicast address to a BE
 * broadcast address and split a GPC/TPC broadcast address into a table of
 * GPC/TPC addresses.  The addresses generated by this function can be
 * successfully processed by gr_gk20a_find_priv_offset_in_buffer
 */
static int gr_gk20a_create_priv_addr_table(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers)
{
	int addr_type; /*enum ctxsw_addr_type */
	u32 gpc_num, tpc_num, ppc_num, be_num;
	u32 broadcast_flags;
	u32 t;
	int err;

	t = 0;
	*num_registers = 0;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

	err = gr_gk20a_decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &be_num,
					&broadcast_flags);
	nvhost_dbg(dbg_gpu_dbg, "addr_type = %d", addr_type);
	if (err)
		return err;

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		/* The BE broadcast registers are included in the compressed PRI
		 * table. Convert a BE unicast address to a broadcast address
		 * so that we can look up the offset. */
		if ((addr_type == CTXSW_ADDR_TYPE_BE) &&
		    !(broadcast_flags & PRI_BROADCAST_FLAGS_BE))
			priv_addr_table[t++] = pri_be_shared_addr(addr);
		else
			priv_addr_table[t++] = addr;

		*num_registers = t;
		return 0;
	}

	/* The GPC/TPC unicast registers are included in the compressed PRI
	 * tables. Convert a GPC/TPC broadcast address to unicast addresses so
	 * that we can look up the offsets. */
	if (broadcast_flags & PRI_BROADCAST_FLAGS_GPC) {
		for (gpc_num = 0; gpc_num < g->gr.gpc_count; gpc_num++) {

			if (broadcast_flags & PRI_BROADCAST_FLAGS_TPC)
				for (tpc_num = 0;
				     tpc_num < g->gr.gpc_tpc_count[gpc_num];
				     tpc_num++)
					priv_addr_table[t++] =
						pri_tpc_addr(pri_tpccs_addr_mask(addr),
							     gpc_num, tpc_num);

			else if (broadcast_flags & PRI_BROADCAST_FLAGS_PPC) {
				err = gr_gk20a_split_ppc_broadcast_addr(g, addr, gpc_num,
							       priv_addr_table, &t);
				if (err)
					return err;
			} else
				priv_addr_table[t++] =
					pri_gpc_addr(pri_gpccs_addr_mask(addr),
						     gpc_num);
		}
	} else {
		if (broadcast_flags & PRI_BROADCAST_FLAGS_TPC)
			for (tpc_num = 0;
			     tpc_num < g->gr.gpc_tpc_count[gpc_num];
			     tpc_num++)
				priv_addr_table[t++] =
					pri_tpc_addr(pri_tpccs_addr_mask(addr),
						     gpc_num, tpc_num);
		else if (broadcast_flags & PRI_BROADCAST_FLAGS_PPC)
			err = gr_gk20a_split_ppc_broadcast_addr(g, addr, gpc_num,
						       priv_addr_table, &t);
		else
			priv_addr_table[t++] = addr;
	}

	*num_registers = t;
	return 0;
}

int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr,
				    u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets,
				    bool is_quad, u32 quad)
{
	u32 i;
	u32 priv_offset = 0;
	u32 *priv_registers;
	u32 num_registers = 0;
	int err = 0;
	u32 potential_offsets = proj_scal_litter_num_gpcs_v() *
		proj_scal_litter_num_tpc_per_gpc_v();

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

	/* implementation is crossed-up if either of these happen */
	if (max_offsets > potential_offsets)
		return -EINVAL;

	if (!g->gr.ctx_vars.golden_image_initialized)
		return -ENODEV;

	priv_registers = kzalloc(sizeof(u32) * potential_offsets, GFP_KERNEL);
	if (IS_ERR_OR_NULL(priv_registers)) {
		nvhost_dbg_fn("failed alloc for potential_offsets=%d", potential_offsets);
		err = PTR_ERR(priv_registers);
		goto cleanup;
	}
	memset(offsets,      0, sizeof(u32) * max_offsets);
	memset(offset_addrs, 0, sizeof(u32) * max_offsets);
	*num_offsets = 0;

	gr_gk20a_create_priv_addr_table(g, addr, &priv_registers[0], &num_registers);

	if ((max_offsets > 1) && (num_registers > max_offsets)) {
		err = -EINVAL;
		goto cleanup;
	}

	if ((max_offsets == 1) && (num_registers > 1))
		num_registers = 1;

	if (!g->gr.ctx_vars.local_golden_image) {
		nvhost_dbg_fn("no context switch header info to work with");
		err = -EINVAL;
		goto cleanup;
	}

	for (i = 0; i < num_registers; i++) {
		err = gr_gk20a_find_priv_offset_in_buffer(g,
						  priv_registers[i],
						  is_quad, quad,
						  g->gr.ctx_vars.local_golden_image,
						  g->gr.ctx_vars.golden_image_size,
						  &priv_offset);
		if (err) {
			nvhost_dbg_fn("Could not determine priv_offset for addr:0x%x",
				      addr); /*, grPriRegStr(addr)));*/
			goto cleanup;
		}

		offsets[i] = priv_offset;
		offset_addrs[i] = priv_registers[i];
	}

    *num_offsets = num_registers;

 cleanup:

    if (!IS_ERR_OR_NULL(priv_registers))
	    kfree(priv_registers);

    return err;
}

/* Setup some register tables.  This looks hacky; our
 * register/offset functions are just that, functions.
 * So they can't be used as initializers... TBD: fix to
 * generate consts at least on an as-needed basis.
 */
static const u32 _num_ovr_perf_regs = 17;
static u32 _ovr_perf_regs[17] = { 0, };
/* Following are the blocks of registers that the ucode
 stores in the extended region.*/
/* ==  ctxsw_extended_sm_dsm_perf_counter_register_stride_v() ? */
static const u32 _num_sm_dsm_perf_regs = 5;
/* ==  ctxsw_extended_sm_dsm_perf_counter_control_register_stride_v() ?*/
static const u32 _num_sm_dsm_perf_ctrl_regs = 4;
static u32 _sm_dsm_perf_regs[5];
static u32 _sm_dsm_perf_ctrl_regs[4];

static void init_sm_dsm_reg_info(void)
{
	if (_ovr_perf_regs[0] != 0)
		return;

	_ovr_perf_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel0_r();
	_ovr_perf_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control_sel1_r();
	_ovr_perf_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control0_r();
	_ovr_perf_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control5_r();
	_ovr_perf_regs[4] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_status1_r();
	_ovr_perf_regs[5] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter0_control_r();
	_ovr_perf_regs[6] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter1_control_r();
	_ovr_perf_regs[7] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter2_control_r();
	_ovr_perf_regs[8] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter3_control_r();
	_ovr_perf_regs[9] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_control_r();
	_ovr_perf_regs[10] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_control_r();
	_ovr_perf_regs[11] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_control_r();
	_ovr_perf_regs[12] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_control_r();
	_ovr_perf_regs[13] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter4_r();
	_ovr_perf_regs[14] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter5_r();
	_ovr_perf_regs[15] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter6_r();
	_ovr_perf_regs[16] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter7_r();


	_sm_dsm_perf_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_status_r();
	_sm_dsm_perf_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter0_r();
	_sm_dsm_perf_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter1_r();
	_sm_dsm_perf_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter2_r();
	_sm_dsm_perf_regs[4] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter3_r();

	_sm_dsm_perf_ctrl_regs[0] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control1_r();
	_sm_dsm_perf_ctrl_regs[1] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control2_r();
	_sm_dsm_perf_ctrl_regs[2] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control3_r();
	_sm_dsm_perf_ctrl_regs[3] = gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control4_r();

}

/* TBD: would like to handle this elsewhere, at a higher level.
 * these are currently constructed in a "test-then-write" style
 * which makes it impossible to know externally whether a ctx
 * write will actually occur. so later we should put a lazy,
 *  map-and-hold system in the patch write state */
int gr_gk20a_ctx_patch_smpc(struct gk20a *g,
			    struct channel_ctx_gk20a *ch_ctx,
			    u32 addr, u32 data,
			    u8 *context)
{
	u32 num_gpc = g->gr.gpc_count;
	u32 num_tpc;
	u32 tpc, gpc, reg;
	u32 chk_addr;
	u32 vaddr_lo;
	u32 vaddr_hi;
	u32 tmp;

	init_sm_dsm_reg_info();

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

	for (reg = 0; reg < _num_ovr_perf_regs; reg++) {
		for (gpc = 0; gpc < num_gpc; gpc++)  {
			num_tpc = g->gr.gpc_tpc_count[gpc];
			for (tpc = 0; tpc < num_tpc; tpc++) {
				chk_addr = ((proj_gpc_stride_v() * gpc) +
					    (proj_tpc_in_gpc_stride_v() * tpc) +
					    _ovr_perf_regs[reg]);
				if (chk_addr != addr)
					continue;
				/* reset the patch count from previous
				   runs,if ucode has already processed
				   it */
				tmp = mem_rd32(context +
				       ctxsw_prog_main_image_patch_count_o(), 0);

				if (!tmp)
					ch_ctx->patch_ctx.data_count = 0;

				gr_gk20a_ctx_patch_write(g, ch_ctx,
							 addr, data, true);

				vaddr_lo = u64_lo32(ch_ctx->patch_ctx.gpu_va);
				vaddr_hi = u64_hi32(ch_ctx->patch_ctx.gpu_va);

				mem_wr32(context +
					 ctxsw_prog_main_image_patch_count_o(),
					 0, ch_ctx->patch_ctx.data_count);
				mem_wr32(context +
					 ctxsw_prog_main_image_patch_adr_lo_o(),
					 0, vaddr_lo);
				mem_wr32(context +
					 ctxsw_prog_main_image_patch_adr_hi_o(),
					 0, vaddr_hi);

				/* we're not caching these on cpu side,
				   but later watch for it */

				/* the l2 invalidate in the patch_write
				 * would be too early for this? */
				gk20a_mm_l2_invalidate(g);
				return 0;
			}
		}
	}

	return 0;
}


void gr_gk20a_access_smpc_reg(struct gk20a *g, u32 quad, u32 offset)
{
	u32 reg;
	u32 quad_ctrl;
	u32 half_ctrl;
	u32 tpc, gpc;
	u32 gpc_tpc_addr;
	u32 gpc_tpc_stride;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "offset=0x%x", offset);

	gpc = pri_get_gpc_num(offset);
	gpc_tpc_addr = pri_gpccs_addr_mask(offset);
	tpc = pri_get_tpc_num(gpc_tpc_addr);

	quad_ctrl = quad & 0x1; /* first bit tells us quad */
	half_ctrl = (quad >> 1) & 0x1; /* second bit tells us half */

	gpc_tpc_stride = gpc * proj_gpc_stride_v() +
		tpc * proj_tpc_in_gpc_stride_v();
	gpc_tpc_addr = gr_gpc0_tpc0_sm_halfctl_ctrl_r() + gpc_tpc_stride;

	reg = gk20a_readl(g, gpc_tpc_addr);
	reg = set_field(reg,
		gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_read_quad_ctl_m(),
		gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_read_quad_ctl_f(quad_ctrl));

	gk20a_writel(g, gpc_tpc_addr, reg);

	gpc_tpc_addr = gr_gpc0_tpc0_sm_debug_sfe_control_r() + gpc_tpc_stride;
	reg = gk20a_readl(g, gpc_tpc_addr);
	reg = set_field(reg,
		gr_gpcs_tpcs_sm_debug_sfe_control_read_half_ctl_m(),
		gr_gpcs_tpcs_sm_debug_sfe_control_read_half_ctl_f(half_ctrl));
	gk20a_writel(g, gpc_tpc_addr, reg);
}

#define ILLEGAL_ID (~0)

static inline bool check_main_image_header_magic(void *context)
{
	u32 magic = mem_rd32(context +
			     ctxsw_prog_main_image_magic_value_o(), 0);
	nvhost_dbg(dbg_gpu_dbg, "main image magic=0x%x", magic);
	return magic == ctxsw_prog_main_image_magic_value_v_value_v();
}
static inline bool check_local_header_magic(void *context)
{
	u32 magic = mem_rd32(context +
			     ctxsw_prog_local_magic_value_o(), 0);
	nvhost_dbg(dbg_gpu_dbg, "local magic=0x%x",  magic);
	return magic == ctxsw_prog_local_magic_value_v_value_v();

}

/* most likely dupe of ctxsw_gpccs_header__size_1_v() */
static inline int ctxsw_prog_ucode_header_size_in_bytes(void)
{
	return 256;
}

static int gr_gk20a_find_priv_offset_in_ext_buffer(struct gk20a *g,
						   u32 addr,
						   bool is_quad, u32 quad,
						   u32 *context_buffer,
						   u32 context_buffer_size,
						   u32 *priv_offset)
{
	u32 i, data32;
	u32 gpc_num, tpc_num;
	u32 num_gpcs, num_tpcs;
	u32 chk_addr;
	u32 ext_priv_offset, ext_priv_size;
	void *context;
	u32 offset_to_segment, offset_to_segment_end;
	u32 sm_dsm_perf_reg_id = ILLEGAL_ID;
	u32 sm_dsm_perf_ctrl_reg_id = ILLEGAL_ID;
	u32 num_ext_gpccs_ext_buffer_segments;
	u32 inter_seg_offset;
	u32 tpc_gpc_mask = (proj_tpc_in_gpc_stride_v() - 1);
	u32 max_tpc_count;
	u32 *sm_dsm_perf_ctrl_regs = NULL;
	u32 num_sm_dsm_perf_ctrl_regs = 0;
	u32 *sm_dsm_perf_regs = NULL;
	u32 num_sm_dsm_perf_regs = 0;
	u32 buffer_segments_size = 0;
	u32 marker_size = 0;
	u32 control_register_stride = 0;
	u32 perf_register_stride = 0;

	/* Only have TPC registers in extended region, so if not a TPC reg,
	   then return error so caller can look elsewhere. */
	if (pri_is_gpc_addr(addr))   {
		u32 gpc_addr = 0;
		gpc_num = pri_get_gpc_num(addr);
		gpc_addr = pri_gpccs_addr_mask(addr);
		if (pri_is_tpc_addr(gpc_addr))
			tpc_num = pri_get_tpc_num(gpc_addr);
		else
			return -EINVAL;

		nvhost_dbg_info(" gpc = %d tpc = %d",
				gpc_num, tpc_num);
	} else
		return -EINVAL;

	buffer_segments_size = ctxsw_prog_extended_buffer_segments_size_in_bytes_v();
	/* note below is in words/num_registers */
	marker_size = ctxsw_prog_extended_marker_size_in_bytes_v() >> 2;

	context = context_buffer;
	/* sanity check main header */
	if (!check_main_image_header_magic(context)) {
		nvhost_err(dev_from_gk20a(g),
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = mem_rd32(context + ctxsw_prog_main_image_num_gpcs_o(), 0);
	if (gpc_num >= num_gpcs) {
		nvhost_err(dev_from_gk20a(g),
		   "GPC 0x%08x is greater than total count 0x%08x!\n",
			   gpc_num, num_gpcs);
		return -EINVAL;
	}

	data32 = mem_rd32(context + ctxsw_prog_main_extended_buffer_ctl_o(), 0);
	ext_priv_size   = ctxsw_prog_main_extended_buffer_ctl_size_v(data32);
	if (0 == ext_priv_size) {
		nvhost_dbg_info(" No extended memory in context buffer");
		return -EINVAL;
	}
	ext_priv_offset = ctxsw_prog_main_extended_buffer_ctl_offset_v(data32);

	offset_to_segment = ext_priv_offset * ctxsw_prog_ucode_header_size_in_bytes();
	offset_to_segment_end = offset_to_segment +
		(ext_priv_size * buffer_segments_size);

	/* check local header magic */
	context += ctxsw_prog_ucode_header_size_in_bytes();
	if (!check_local_header_magic(context)) {
		nvhost_err(dev_from_gk20a(g),
			   "Invalid local header: magic value\n");
		return -EINVAL;
	}

	/*
	 * See if the incoming register address is in the first table of
	 * registers. We check this by decoding only the TPC addr portion.
	 * If we get a hit on the TPC bit, we then double check the address
	 * by computing it from the base gpc/tpc strides.  Then make sure
	 * it is a real match.
	 */
	num_sm_dsm_perf_regs = _num_sm_dsm_perf_regs;
	sm_dsm_perf_regs = _sm_dsm_perf_regs;
	perf_register_stride = ctxsw_prog_extended_sm_dsm_perf_counter_register_stride_v();

	init_sm_dsm_reg_info();

	for (i = 0; i < num_sm_dsm_perf_regs; i++) {
		if ((addr & tpc_gpc_mask) == (sm_dsm_perf_regs[i] & tpc_gpc_mask)) {
			sm_dsm_perf_reg_id = i;

			nvhost_dbg_info("register match: 0x%08x",
					sm_dsm_perf_regs[i]);

			chk_addr = (proj_gpc_base_v() +
				   (proj_gpc_stride_v() * gpc_num) +
				   proj_tpc_in_gpc_base_v() +
				   (proj_tpc_in_gpc_stride_v() * tpc_num) +
				   (sm_dsm_perf_regs[sm_dsm_perf_reg_id] & tpc_gpc_mask));

			if (chk_addr != addr) {
				nvhost_err(dev_from_gk20a(g),
				   "Oops addr miss-match! : 0x%08x != 0x%08x\n",
					   addr, chk_addr);
				return -EINVAL;
			}
			break;
		}
	}

	/* Didn't find reg in supported group 1.
	 *  so try the second group now */
	num_sm_dsm_perf_ctrl_regs = _num_sm_dsm_perf_ctrl_regs;
	sm_dsm_perf_ctrl_regs     = _sm_dsm_perf_ctrl_regs;
	control_register_stride = ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v();

	if (ILLEGAL_ID == sm_dsm_perf_reg_id) {
		for (i = 0; i < num_sm_dsm_perf_ctrl_regs; i++) {
			if ((addr & tpc_gpc_mask) ==
			    (sm_dsm_perf_ctrl_regs[i] & tpc_gpc_mask)) {
				sm_dsm_perf_ctrl_reg_id = i;

				nvhost_dbg_info("register match: 0x%08x",
						sm_dsm_perf_ctrl_regs[i]);

				chk_addr = (proj_gpc_base_v() +
					   (proj_gpc_stride_v() * gpc_num) +
					   proj_tpc_in_gpc_base_v() +
					   (proj_tpc_in_gpc_stride_v() * tpc_num) +
					   (sm_dsm_perf_ctrl_regs[sm_dsm_perf_ctrl_reg_id] &
					    tpc_gpc_mask));

				if (chk_addr != addr) {
					nvhost_err(dev_from_gk20a(g),
						   "Oops addr miss-match! : 0x%08x != 0x%08x\n",
						   addr, chk_addr);
					return -EINVAL;

				}

				break;
			}
		}
	}

	if ((ILLEGAL_ID == sm_dsm_perf_ctrl_reg_id) &&
	    (ILLEGAL_ID == sm_dsm_perf_reg_id))
		return -EINVAL;

	/* Skip the FECS extended header, nothing there for us now. */
	offset_to_segment += buffer_segments_size;

	/* skip through the GPCCS extended headers until we get to the data for
	 * our GPC.  The size of each gpc extended segment is enough to hold the
	 * max tpc count for the gpcs,in 256b chunks.
	 */

	max_tpc_count = proj_scal_litter_num_tpc_per_gpc_v();

	num_ext_gpccs_ext_buffer_segments = (u32)((max_tpc_count + 1) / 2);

	offset_to_segment += (num_ext_gpccs_ext_buffer_segments *
			      buffer_segments_size * gpc_num);

	num_tpcs = g->gr.gpc_tpc_count[gpc_num];

	/* skip the head marker to start with */
	inter_seg_offset = marker_size;

	if (ILLEGAL_ID != sm_dsm_perf_ctrl_reg_id) {
		/* skip over control regs of TPC's before the one we want.
		 *  then skip to the register in this tpc */
		inter_seg_offset = inter_seg_offset +
			(tpc_num * control_register_stride) +
			sm_dsm_perf_ctrl_reg_id;
	} else {
		/* skip all the control registers */
		inter_seg_offset = inter_seg_offset +
			(num_tpcs * control_register_stride);

		/* skip the marker between control and counter segments */
		inter_seg_offset += marker_size;

		/* skip over counter regs of TPCs before the one we want */
		inter_seg_offset = inter_seg_offset +
			(tpc_num * perf_register_stride) *
			ctxsw_prog_extended_num_smpc_quadrants_v();

		/* skip over the register for the quadrants we do not want.
		 *  then skip to the register in this tpc */
		inter_seg_offset = inter_seg_offset +
			(perf_register_stride * quad) +
			sm_dsm_perf_reg_id;
	}

	/* set the offset to the segment offset plus the inter segment offset to
	 *  our register */
	offset_to_segment += (inter_seg_offset * 4);

	/* last sanity check: did we somehow compute an offset outside the
	 * extended buffer? */
	if (offset_to_segment > offset_to_segment_end) {
		nvhost_err(dev_from_gk20a(g),
			   "Overflow ctxsw buffer! 0x%08x > 0x%08x\n",
			   offset_to_segment, offset_to_segment_end);
		return -EINVAL;
	}

	*priv_offset = offset_to_segment;

	return 0;
}


static int
gr_gk20a_process_context_buffer_priv_segment(struct gk20a *g,
					     int addr_type,/* enum ctxsw_addr_type */
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset)
{
	u32 i;
	u32 address, base_address;
	u32 sys_offset, gpc_offset, tpc_offset, ppc_offset;
	u32 ppc_num, tpc_num, tpc_addr, gpc_addr, ppc_addr;
	struct aiv_gk20a *reg;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "pri_addr=0x%x", pri_addr);

	if (!g->gr.ctx_vars.valid)
		return -EINVAL;

	/* Process the SYS/BE segment. */
	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.sys.count; i++) {
			reg = &g->gr.ctx_vars.ctxsw_regs.sys.l[i];
			address    = reg->addr;
			sys_offset = reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset;
				return 0;
			}
		}
	}

	/* Process the TPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_TPC) {
		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.tpc.count; i++) {
				reg = &g->gr.ctx_vars.ctxsw_regs.tpc.l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(address);
				base_address = proj_gpc_base_v() +
					(gpc_num * proj_gpc_stride_v()) +
					proj_tpc_in_gpc_base_v() +
					(tpc_num * proj_tpc_in_gpc_stride_v());
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					return 0;
				}
			}
		}
	}

	/* Process the PPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_PPC) {
		for (ppc_num = 0; ppc_num < num_ppcs; ppc_num++) {
			for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.ppc.count; i++) {
				reg = &g->gr.ctx_vars.ctxsw_regs.ppc.l[i];
				address = reg->addr;
				ppc_addr = pri_ppccs_addr_mask(address);
				base_address = proj_gpc_base_v() +
					(gpc_num * proj_gpc_stride_v()) +
					proj_ppc_in_gpc_base_v() +
					(ppc_num * proj_ppc_in_gpc_stride_v());
				address = base_address + ppc_addr;
				/*
				 * The data for the PPCs is interleaved in the context buffer.
				 * Example with numPpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				ppc_offset = (reg->index * num_ppcs) + (ppc_num * 4);

				if (pri_addr == address)  {
					*priv_offset = ppc_offset;
					return 0;
				}
			}
		}
	}


	/* Process the GPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_GPC) {
		for (i = 0; i < g->gr.ctx_vars.ctxsw_regs.gpc.count; i++) {
			reg = &g->gr.ctx_vars.ctxsw_regs.gpc.l[i];

			address = reg->addr;
			gpc_addr = pri_gpccs_addr_mask(address);
			gpc_offset = reg->index;

			base_address = proj_gpc_base_v() +
				(gpc_num * proj_gpc_stride_v());
			address = base_address + gpc_addr;

			if (pri_addr == address) {
				*priv_offset = gpc_offset;
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int gr_gk20a_determine_ppc_configuration(struct gk20a *g,
					       void *context,
					       u32 *num_ppcs, u32 *ppc_mask,
					       u32 *reg_ppc_count)
{
	u32 data32;
	u32 litter_num_pes_per_gpc = proj_scal_litter_num_pes_per_gpc_v();

	/*
	 * if there is only 1 PES_PER_GPC, then we put the PES registers
	 * in the GPC reglist, so we can't error out if ppc.count == 0
	 */
	if ((!g->gr.ctx_vars.valid) ||
	    ((g->gr.ctx_vars.ctxsw_regs.ppc.count == 0) &&
	     (litter_num_pes_per_gpc > 1)))
		return -EINVAL;

	data32 = mem_rd32(context + ctxsw_prog_local_image_ppc_info_o(), 0);

	*num_ppcs = ctxsw_prog_local_image_ppc_info_num_ppcs_v(data32);
	*ppc_mask = ctxsw_prog_local_image_ppc_info_ppc_mask_v(data32);

	*reg_ppc_count = g->gr.ctx_vars.ctxsw_regs.ppc.count;

	return 0;
}



/*
 *  This function will return the 32 bit offset for a priv register if it is
 *  present in the context buffer.
 */
static int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       bool is_quad, u32 quad,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset)
{
	struct gr_gk20a *gr = &g->gr;
	u32 i, data32;
	int err;
	int addr_type; /*enum ctxsw_addr_type */
	u32 broadcast_flags;
	u32 gpc_num, tpc_num, ppc_num, be_num;
	u32 num_gpcs, num_tpcs, num_ppcs;
	u32 offset;
	u32 sys_priv_offset, gpc_priv_offset;
	u32 ppc_mask, reg_list_ppc_count;
	void *context;
	u32 offset_to_segment;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "addr=0x%x", addr);

	err = gr_gk20a_decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &be_num,
					&broadcast_flags);
	if (err)
		return err;

	context = context_buffer;
	if (!check_main_image_header_magic(context)) {
		nvhost_err(dev_from_gk20a(g),
			   "Invalid main header: magic value");
		return -EINVAL;
	}
	num_gpcs = mem_rd32(context + ctxsw_prog_main_image_num_gpcs_o(), 0);

	/* Parse the FECS local header. */
	context += ctxsw_prog_ucode_header_size_in_bytes();
	if (!check_local_header_magic(context)) {
		nvhost_err(dev_from_gk20a(g),
			   "Invalid FECS local header: magic value\n");
		return -EINVAL;
	}
	data32 = mem_rd32(context + ctxsw_prog_local_priv_register_ctl_o(), 0);
	sys_priv_offset = ctxsw_prog_local_priv_register_ctl_offset_v(data32);

	/* If found in Ext buffer, ok.
	 * If it failed and we expected to find it there (quad offset)
	 * then return the error.  Otherwise continue on.
	 */
	err = gr_gk20a_find_priv_offset_in_ext_buffer(g,
				      addr, is_quad, quad, context_buffer,
				      context_buffer_size, priv_offset);
	if (!err || (err && is_quad))
		return err;

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_BE)) {
		/* Find the offset in the FECS segment. */
		offset_to_segment = sys_priv_offset *
			ctxsw_prog_ucode_header_size_in_bytes();

		err = gr_gk20a_process_context_buffer_priv_segment(g,
					   addr_type, addr,
					   0, 0, 0, 0,
					   &offset);
		if (err)
			return err;

		*priv_offset = (offset_to_segment + offset);
		return 0;
	}

	if ((gpc_num + 1) > num_gpcs)  {
		nvhost_err(dev_from_gk20a(g),
			   "GPC %d not in this context buffer.\n",
			   gpc_num);
		return -EINVAL;
	}

	/* Parse the GPCCS local header(s).*/
	for (i = 0; i < num_gpcs; i++) {
		context += ctxsw_prog_ucode_header_size_in_bytes();
		if (!check_local_header_magic(context)) {
			nvhost_err(dev_from_gk20a(g),
				   "Invalid GPCCS local header: magic value\n");
			return -EINVAL;

		}
		data32 = mem_rd32(context + ctxsw_prog_local_priv_register_ctl_o(), 0);
		gpc_priv_offset = ctxsw_prog_local_priv_register_ctl_offset_v(data32);

		err = gr_gk20a_determine_ppc_configuration(g, context,
							   &num_ppcs, &ppc_mask,
							   &reg_list_ppc_count);
		if (err)
			return err;

		num_tpcs = mem_rd32(context + ctxsw_prog_local_image_num_tpcs_o(), 0);

		if ((i == gpc_num) && ((tpc_num + 1) > num_tpcs)) {
			nvhost_err(dev_from_gk20a(g),
			   "GPC %d TPC %d not in this context buffer.\n",
				   gpc_num, tpc_num);
			return -EINVAL;
		}

		/* Find the offset in the GPCCS segment.*/
		if (i == gpc_num) {
			offset_to_segment = gpc_priv_offset *
				ctxsw_prog_ucode_header_size_in_bytes();

			if (addr_type == CTXSW_ADDR_TYPE_TPC) {
				/*reg = gr->ctx_vars.ctxsw_regs.tpc.l;*/
			} else if (addr_type == CTXSW_ADDR_TYPE_PPC) {
				/* The ucode stores TPC data before PPC data.
				 * Advance offset past TPC data to PPC data. */
				offset_to_segment +=
					((gr->ctx_vars.ctxsw_regs.tpc.count *
					  num_tpcs) << 2);
			} else if (addr_type == CTXSW_ADDR_TYPE_GPC) {
				/* The ucode stores TPC/PPC data before GPC data.
				 * Advance offset past TPC/PPC data to GPC data. */
				/* note 1 PES_PER_GPC case */
				u32 litter_num_pes_per_gpc =
					proj_scal_litter_num_pes_per_gpc_v();
				if (litter_num_pes_per_gpc > 1) {
					offset_to_segment +=
						(((gr->ctx_vars.ctxsw_regs.tpc.count *
						   num_tpcs) << 2) +
						 ((reg_list_ppc_count * num_ppcs) << 2));
				} else {
					offset_to_segment +=
						((gr->ctx_vars.ctxsw_regs.tpc.count *
						  num_tpcs) << 2);
				}
			} else {
				nvhost_err(dev_from_gk20a(g),
					   " Unknown address type.\n");
				return -EINVAL;
			}
			err = gr_gk20a_process_context_buffer_priv_segment(g,
							   addr_type, addr,
							   i, num_tpcs,
							   num_ppcs, ppc_mask,
							   &offset);
			if (err)
			    return -EINVAL;

			*priv_offset = offset_to_segment + offset;
			return 0;
		}
	}

	return -EINVAL;
}


int gr_gk20a_exec_ctx_ops(struct channel_gk20a *ch,
			  struct nvhost_dbg_gpu_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops)
{
	struct gk20a *g = ch->g;
	struct channel_ctx_gk20a *ch_ctx = &ch->ch_ctx;
	void *ctx_ptr = NULL;
	int curr_gr_chid, curr_gr_ctx;
	bool ch_is_curr_ctx, restart_gr_ctxsw = false;
	u32 i, j, offset, v;
	u32 max_offsets = proj_scal_litter_num_gpcs_v() *
		proj_scal_litter_num_tpc_per_gpc_v();
	u32 *offsets = NULL;
	u32 *offset_addrs = NULL;
	u32 ctx_op_nr, num_ctx_ops[2] = {num_ctx_wr_ops, num_ctx_rd_ops};
	int err, pass;

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "wr_ops=%d rd_ops=%d",
		   num_ctx_wr_ops, num_ctx_rd_ops);

	/* disable channel switching.
	 * at that point the hardware state can be inspected to
	 * determine if the context we're interested in is current.
	 */
	err = gr_gk20a_disable_ctxsw(g);
	if (err) {
		nvhost_err(dev_from_gk20a(g), "unable to stop gr ctxsw");
		/* this should probably be ctx-fatal... */
		goto cleanup;
	}

	restart_gr_ctxsw = true;

	curr_gr_ctx  = gk20a_readl(g, gr_fecs_current_ctx_r());
	curr_gr_chid = gk20a_gr_get_chid_from_ctx(g, curr_gr_ctx);
	ch_is_curr_ctx = (curr_gr_chid != -1) && (ch->hw_chid == curr_gr_chid);

	nvhost_dbg(dbg_fn | dbg_gpu_dbg, "is curr ctx=%d", ch_is_curr_ctx);
	if (ch_is_curr_ctx) {
		for (pass = 0; pass < 2; pass++) {
			ctx_op_nr = 0;
			for (i = 0; (ctx_op_nr < num_ctx_ops[pass]) && (i < num_ops); ++i) {
				/* only do ctx ops and only on the right pass */
				if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
				    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
				     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op))))
					continue;

				/* if this is a quad access, setup for special access*/
				if (ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD))
					gr_gk20a_access_smpc_reg(g, ctx_ops[i].quad,
								 ctx_ops[i].offset);
				offset = ctx_ops[i].offset;

				if (pass == 0) { /* write pass */
					v = gk20a_readl(g, offset);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					gk20a_writel(g, offset, v);

					nvhost_dbg(dbg_gpu_dbg,
						   "direct wr: offset=0x%x v=0x%x",
						   offset, v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = gk20a_readl(g, offset + 4);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						gk20a_writel(g, offset + 4, v);

						nvhost_dbg(dbg_gpu_dbg,
							   "direct wr: offset=0x%x v=0x%x",
							   offset + 4, v);
					}

				} else { /* read pass */
					ctx_ops[i].value_lo =
						gk20a_readl(g, offset);

					nvhost_dbg(dbg_gpu_dbg,
						   "direct rd: offset=0x%x v=0x%x",
						   offset, ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							gk20a_readl(g, offset + 4);

						nvhost_dbg(dbg_gpu_dbg,
							   "direct rd: offset=0x%x v=0x%x",
							   offset, ctx_ops[i].value_lo);
					} else
						ctx_ops[i].value_hi = 0;
				}
				ctx_op_nr++;
			}
		}
		goto cleanup;
	}

	/* they're the same size, so just use one alloc for both */
	offsets = kzalloc(2 * sizeof(u32) * max_offsets, GFP_KERNEL);
	if (!offsets) {
		err = -ENOMEM;
		goto cleanup;
	}
	offset_addrs = offsets + max_offsets;

	/* would have been a variant of gr_gk20a_apply_instmem_overrides */
	/* recoded in-place instead.*/
	ctx_ptr = vmap(ch_ctx->gr_ctx.pages,
			PAGE_ALIGN(ch_ctx->gr_ctx.size) >> PAGE_SHIFT,
			0, pgprot_dmacoherent(PAGE_KERNEL));
	if (!ctx_ptr) {
		err = -ENOMEM;
		goto cleanup;
	}

	/* Channel gr_ctx buffer is gpu cacheable; so flush and invalidate.
	 * There should be no on-going/in-flight references by the gpu now. */
	gk20a_mm_fb_flush(g);
	gk20a_mm_l2_flush(g, true);

	/* write to appropriate place in context image,
	 * first have to figure out where that really is */

	/* first pass is writes, second reads */
	for (pass = 0; pass < 2; pass++) {
		ctx_op_nr = 0;
		for (i = 0; (ctx_op_nr < num_ctx_ops[pass]) && (i < num_ops); ++i) {
			u32 num_offsets;

			/* only do ctx ops and only on the right pass */
			if ((ctx_ops[i].type == REGOP(TYPE_GLOBAL)) ||
			    (((pass == 0) && reg_op_is_read(ctx_ops[i].op)) ||
			     ((pass == 1) && !reg_op_is_read(ctx_ops[i].op))))
				continue;

			err = gr_gk20a_get_ctx_buffer_offsets(g,
						ctx_ops[i].offset,
						max_offsets,
						offsets, offset_addrs,
						&num_offsets,
						ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD),
						ctx_ops[i].quad);
			if (err) {
				nvhost_dbg(dbg_gpu_dbg,
					   "ctx op invalid offset: offset=0x%x",
					   ctx_ops[i].offset);
				ctx_ops[i].status =
					NVHOST_DBG_GPU_REG_OP_STATUS_INVALID_OFFSET;
				continue;
			}

			/* if this is a quad access, setup for special access*/
			if (ctx_ops[i].type == REGOP(TYPE_GR_CTX_QUAD))
				gr_gk20a_access_smpc_reg(g, ctx_ops[i].quad,
							 ctx_ops[i].offset);

			for (j = 0; j < num_offsets; j++) {
				/* sanity check, don't write outside, worst case */
				if (offsets[j] >= g->gr.ctx_vars.golden_image_size)
					continue;
				if (pass == 0) { /* write pass */
					v = mem_rd32(ctx_ptr + offsets[j], 0);
					v &= ~ctx_ops[i].and_n_mask_lo;
					v |= ctx_ops[i].value_lo;
					mem_wr32(ctx_ptr + offsets[j], 0, v);

					nvhost_dbg(dbg_gpu_dbg,
						   "context wr: offset=0x%x v=0x%x",
						   offsets[j], v);

					if (ctx_ops[i].op == REGOP(WRITE_64)) {
						v = mem_rd32(ctx_ptr + offsets[j] + 4, 0);
						v &= ~ctx_ops[i].and_n_mask_hi;
						v |= ctx_ops[i].value_hi;
						mem_wr32(ctx_ptr + offsets[j] + 4, 0, v);

						nvhost_dbg(dbg_gpu_dbg,
							   "context wr: offset=0x%x v=0x%x",
							   offsets[j] + 4, v);
					}

					/* check to see if we need to add a special WAR
					   for some of the SMPC perf regs */
					gr_gk20a_ctx_patch_smpc(g, ch_ctx, offset_addrs[j],
							v, ctx_ptr);

				} else { /* read pass */
					ctx_ops[i].value_lo =
						mem_rd32(ctx_ptr + offsets[0], 0);

					nvhost_dbg(dbg_gpu_dbg, "context rd: offset=0x%x v=0x%x",
						   offsets[0], ctx_ops[i].value_lo);

					if (ctx_ops[i].op == REGOP(READ_64)) {
						ctx_ops[i].value_hi =
							mem_rd32(ctx_ptr + offsets[0] + 4, 0);

						nvhost_dbg(dbg_gpu_dbg,
							   "context rd: offset=0x%x v=0x%x",
							   offsets[0] + 4, ctx_ops[i].value_hi);
					} else
						ctx_ops[i].value_hi = 0;
				}
			}
			ctx_op_nr++;
		}
	}
#if 0
	/* flush cpu caches for the ctx buffer? only if cpu cached, of course.
	 * they aren't, yet */
	if (cached) {
		FLUSH_CPU_DCACHE(ctx_ptr,
			 sg_phys(ch_ctx->gr_ctx.mem.ref), size);
	}
#endif

 cleanup:
	if (offsets)
		kfree(offsets);

	if (ctx_ptr)
		vunmap(ctx_ptr);

	if (restart_gr_ctxsw) {
		int tmp_err = gr_gk20a_enable_ctxsw(g);
		if (tmp_err) {
			nvhost_err(dev_from_gk20a(g), "unable to restart ctxsw!\n");
			err = tmp_err;
		}
	}

	return err;
}
