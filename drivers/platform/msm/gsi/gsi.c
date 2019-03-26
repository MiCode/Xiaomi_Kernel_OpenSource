/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/msm_gsi.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "gsi.h"
#include "gsi_reg.h"
#include "gsi_emulation.h"

#define GSI_CMD_TIMEOUT (5*HZ)
#define GSI_START_CMD_TIMEOUT_MS 1000
#define GSI_CMD_POLL_CNT 5
#define GSI_STOP_CMD_TIMEOUT_MS 200
#define GSI_MAX_CH_LOW_WEIGHT 15

#define GSI_STOP_CMD_POLL_CNT 4
#define GSI_STOP_IN_PROC_CMD_POLL_CNT 2

#define GSI_RESET_WA_MIN_SLEEP 1000
#define GSI_RESET_WA_MAX_SLEEP 2000
#define GSI_CHNL_STATE_MAX_RETRYCNT 10

#define GSI_STTS_REG_BITS 32

#ifndef CONFIG_DEBUG_FS
void gsi_debugfs_init(void)
{
}
#endif

static const struct of_device_id msm_gsi_match[] = {
	{ .compatible = "qcom,msm_gsi", },
	{ },
};


#if defined(CONFIG_IPA_EMULATION)
static bool running_emulation = true;
#else
static bool running_emulation;
#endif

struct gsi_ctx *gsi_ctx;

static void __gsi_config_type_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_TYPE_IRQ_MSK_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_TYPE_IRQ_MSK_OFFS(ee));
}

static void __gsi_config_ch_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_MSK_OFFS(ee));
}

static void __gsi_config_evt_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_MSK_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_MSK_OFFS(ee));
}

static void __gsi_config_ieob_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_OFFS(ee));
	GSIDBG("current IEO_IRQ_MSK: 0x%x, change to: 0x%x\n",
		curr, ((curr & ~mask) | (val & mask)));
}

static void __gsi_config_glob_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_OFFS(ee));
}

static void __gsi_config_gen_irq(int ee, uint32_t mask, uint32_t val)
{
	uint32_t curr;

	curr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_GSI_IRQ_EN_OFFS(ee));
	gsi_writel((curr & ~mask) | (val & mask), gsi_ctx->base +
			GSI_EE_n_CNTXT_GSI_IRQ_EN_OFFS(ee));
}

static void gsi_channel_state_change_wait(unsigned long chan_hdl,
	struct gsi_chan_ctx *ctx,
	uint32_t tm, enum gsi_ch_cmd_opcode op)
{
	int poll_cnt;
	int gsi_pending_intr;
	int res;
	uint32_t type;
	uint32_t val;
	int ee = gsi_ctx->per.ee;
	enum gsi_chan_state curr_state = GSI_CHAN_STATE_NOT_ALLOCATED;
	int stop_in_proc_retry = 0;
	int stop_retry = 0;

	/*
	 * Start polling the GSI channel for
	 * duration = tm * GSI_CMD_POLL_CNT.
	 * We need to do polling of gsi state for improving debugability
	 * of gsi hw state.
	 */

	for (poll_cnt = 0;
		poll_cnt < GSI_CMD_POLL_CNT;
		poll_cnt++) {
		res = wait_for_completion_timeout(&ctx->compl,
			msecs_to_jiffies(tm));

		/* Interrupt received, return */
		if (res != 0)
			return;

		type = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_TYPE_IRQ_OFFS(gsi_ctx->per.ee));

		gsi_pending_intr = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_OFFS(ee));

		/* Update the channel state only if interrupt was raised
		 * on praticular channel and also checking global interrupt
		 * is raised for channel control.
		 */
		if ((type & GSI_EE_n_CNTXT_TYPE_IRQ_CH_CTRL_BMSK) &&
				((gsi_pending_intr >> chan_hdl) & 1)) {
			/*
			 * Check channel state here in case the channel is
			 * already started but interrupt is not yet received.
			 */
			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(chan_hdl,
					gsi_ctx->per.ee));
			curr_state = (val &
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_SHFT;
		}

		if (op == GSI_CH_START) {
			if (curr_state == GSI_CHAN_STATE_STARTED) {
				ctx->state = curr_state;
				return;
			}
		}

		if (op == GSI_CH_STOP) {
			if (curr_state == GSI_CHAN_STATE_STOPPED)
				stop_retry++;
			else if (curr_state == GSI_CHAN_STATE_STOP_IN_PROC)
				stop_in_proc_retry++;
		}

		/* if interrupt marked reg after poll count reaching to max
		 * keep loop to continue reach max stop proc and max stop count.
		 */
		if (stop_retry == 1 || stop_in_proc_retry == 1)
			poll_cnt = 0;

		/* If stop channel retry reached to max count
		 * clear the pending interrupt, if channel already stopped.
		 */
		if (stop_retry == GSI_STOP_CMD_POLL_CNT) {
			gsi_writel(gsi_pending_intr, gsi_ctx->base +
				GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_CLR_OFFS(ee));
			ctx->state = curr_state;
			return;
		}

		/* If channel state stop in progress case no need
		 * to wait for long time.
		 */
		if (stop_in_proc_retry == GSI_STOP_IN_PROC_CMD_POLL_CNT) {
			ctx->state = curr_state;
			return;
		}

		GSIDBG("GSI wait on chan_hld=%lu irqtyp=%lu state=%u intr=%u\n",
			chan_hdl,
			type,
			ctx->state,
			gsi_pending_intr);
	}

	GSIDBG("invalidating the channel state when timeout happens\n");
	ctx->state = curr_state;
}

static void gsi_handle_ch_ctrl(int ee)
{
	uint32_t ch;
	int i;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	ch = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_OFFS(ee));
	gsi_writel(ch, gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_CLR_OFFS(ee));
	GSIDBG("ch %x\n", ch);
	for (i = 0; i < GSI_STTS_REG_BITS; i++) {
		if ((1 << i) & ch) {
			if (i >= gsi_ctx->max_ch || i >= GSI_CHAN_MAX) {
				GSIERR("invalid channel %d\n", i);
				break;
			}

			ctx = &gsi_ctx->chan[i];
			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(i, ee));
			ctx->state = (val &
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_SHFT;
			GSIDBG("ch %u state updated to %u\n", i, ctx->state);
			complete(&ctx->compl);
			gsi_ctx->ch_dbg[i].cmd_completed++;
		}
	}
}

static void gsi_handle_ev_ctrl(int ee)
{
	uint32_t ch;
	int i;
	uint32_t val;
	struct gsi_evt_ctx *ctx;

	ch = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_OFFS(ee));
	gsi_writel(ch, gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_CLR_OFFS(ee));
	GSIDBG("ev %x\n", ch);
	for (i = 0; i < GSI_STTS_REG_BITS; i++) {
		if ((1 << i) & ch) {
			if (i >= gsi_ctx->max_ev || i >= GSI_EVT_RING_MAX) {
				GSIERR("invalid event %d\n", i);
				break;
			}

			ctx = &gsi_ctx->evtr[i];
			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_EV_CH_k_CNTXT_0_OFFS(i, ee));
			ctx->state = (val &
				GSI_EE_n_EV_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_EV_CH_k_CNTXT_0_CHSTATE_SHFT;
			GSIDBG("evt %u state updated to %u\n", i, ctx->state);
			complete(&ctx->compl);
		}
	}
}

static void gsi_handle_glob_err(uint32_t err)
{
	struct gsi_log_err *log;
	struct gsi_chan_ctx *ch;
	struct gsi_evt_ctx *ev;
	struct gsi_chan_err_notify chan_notify;
	struct gsi_evt_err_notify evt_notify;
	struct gsi_per_notify per_notify;
	uint32_t val;
	enum gsi_err_type err_type;

	log = (struct gsi_log_err *)&err;
	GSIERR("log err_type=%u ee=%u idx=%u\n", log->err_type, log->ee,
			log->virt_idx);
	GSIERR("code=%u arg1=%u arg2=%u arg3=%u\n", log->code, log->arg1,
			log->arg2, log->arg3);

	err_type = log->err_type;
	/*
	 * These are errors thrown by hardware. We need
	 * BUG_ON() to capture the hardware state right
	 * when it is unexpected.
	 */
	switch (err_type) {
	case GSI_ERR_TYPE_GLOB:
		per_notify.evt_id = GSI_PER_EVT_GLOB_ERROR;
		per_notify.user_data = gsi_ctx->per.user_data;
		per_notify.data.err_desc = err & 0xFFFF;
		gsi_ctx->per.notify_cb(&per_notify);
		break;
	case GSI_ERR_TYPE_CHAN:
		if (WARN_ON(log->virt_idx >= gsi_ctx->max_ch)) {
			GSIERR("Unexpected ch %d\n", log->virt_idx);
			return;
		}

		ch = &gsi_ctx->chan[log->virt_idx];
		chan_notify.chan_user_data = ch->props.chan_user_data;
		chan_notify.err_desc = err & 0xFFFF;
		if (log->code == GSI_INVALID_TRE_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}

			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(log->virt_idx,
					gsi_ctx->per.ee));
			ch->state = (val &
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_SHFT;
			GSIDBG("ch %u state updated to %u\n", log->virt_idx,
					ch->state);
			ch->stats.invalid_tre_error++;
			if (ch->state == GSI_CHAN_STATE_ERROR) {
				GSIERR("Unexpected channel state %d\n",
					ch->state);
				BUG();
			}
			chan_notify.evt_id = GSI_CHAN_INVALID_TRE_ERR;
		} else if (log->code == GSI_OUT_OF_BUFFERS_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			chan_notify.evt_id = GSI_CHAN_OUT_OF_BUFFERS_ERR;
		} else if (log->code == GSI_OUT_OF_RESOURCES_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			chan_notify.evt_id = GSI_CHAN_OUT_OF_RESOURCES_ERR;
			complete(&ch->compl);
		} else if (log->code == GSI_UNSUPPORTED_INTER_EE_OP_ERR) {
			chan_notify.evt_id =
				GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR;
		} else if (log->code == GSI_NON_ALLOCATED_EVT_ACCESS_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			chan_notify.evt_id =
				GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR;
		} else if (log->code == GSI_HWO_1_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			chan_notify.evt_id = GSI_CHAN_HWO_1_ERR;
		} else {
			GSIERR("unexpected event log code %d\n", log->code);
			BUG();
		}
		ch->props.err_cb(&chan_notify);
		break;
	case GSI_ERR_TYPE_EVT:
		if (WARN_ON(log->virt_idx >= gsi_ctx->max_ev)) {
			GSIERR("Unexpected ev %d\n", log->virt_idx);
			return;
		}

		ev = &gsi_ctx->evtr[log->virt_idx];
		evt_notify.user_data = ev->props.user_data;
		evt_notify.err_desc = err & 0xFFFF;
		if (log->code == GSI_OUT_OF_BUFFERS_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			evt_notify.evt_id = GSI_EVT_OUT_OF_BUFFERS_ERR;
		} else if (log->code == GSI_OUT_OF_RESOURCES_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			evt_notify.evt_id = GSI_EVT_OUT_OF_RESOURCES_ERR;
			complete(&ev->compl);
		} else if (log->code == GSI_UNSUPPORTED_INTER_EE_OP_ERR) {
			evt_notify.evt_id = GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR;
		} else if (log->code == GSI_EVT_RING_EMPTY_ERR) {
			if (log->ee != gsi_ctx->per.ee) {
				GSIERR("unexpected EE in event %d\n", log->ee);
				BUG();
			}
			evt_notify.evt_id = GSI_EVT_EVT_RING_EMPTY_ERR;
		} else {
			GSIERR("unexpected event log code %d\n", log->code);
			BUG();
		}
		ev->props.err_cb(&evt_notify);
		break;
	}
}

static void gsi_handle_gp_int1(void)
{
	complete(&gsi_ctx->gen_ee_cmd_compl);
}

static void gsi_handle_glob_ee(int ee)
{
	uint32_t val;
	uint32_t err;
	struct gsi_per_notify notify;
	uint32_t clr = ~0;

	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_GLOB_IRQ_STTS_OFFS(ee));

	notify.user_data = gsi_ctx->per.user_data;

	if (val & GSI_EE_n_CNTXT_GLOB_IRQ_STTS_ERROR_INT_BMSK) {
		err = gsi_readl(gsi_ctx->base +
			GSI_EE_n_ERROR_LOG_OFFS(ee));
		if (gsi_ctx->per.ver >= GSI_VER_1_2)
			gsi_writel(0, gsi_ctx->base +
				GSI_EE_n_ERROR_LOG_OFFS(ee));
		gsi_writel(clr, gsi_ctx->base +
			GSI_EE_n_ERROR_LOG_CLR_OFFS(ee));
		gsi_handle_glob_err(err);
	}

	if (val & GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK)
		gsi_handle_gp_int1();

	if (val & GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT2_BMSK) {
		notify.evt_id = GSI_PER_EVT_GLOB_GP2;
		gsi_ctx->per.notify_cb(&notify);
	}

	if (val & GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT3_BMSK) {
		notify.evt_id = GSI_PER_EVT_GLOB_GP3;
		gsi_ctx->per.notify_cb(&notify);
	}

	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_CNTXT_GLOB_IRQ_CLR_OFFS(ee));
}

static void gsi_incr_ring_wp(struct gsi_ring_ctx *ctx)
{
	ctx->wp_local += ctx->elem_sz;
	if (ctx->wp_local == ctx->end)
		ctx->wp_local = ctx->base;
}

static void gsi_incr_ring_rp(struct gsi_ring_ctx *ctx)
{
	ctx->rp_local += ctx->elem_sz;
	if (ctx->rp_local == ctx->end)
		ctx->rp_local = ctx->base;
}

uint16_t gsi_find_idx_from_addr(struct gsi_ring_ctx *ctx, uint64_t addr)
{
	WARN_ON(addr < ctx->base || addr >= ctx->end);
	return (uint32_t)(addr - ctx->base) / ctx->elem_sz;
}

static uint16_t gsi_get_complete_num(struct gsi_ring_ctx *ctx, uint64_t addr1,
		uint64_t addr2)
{
	uint32_t addr_diff;

	GSIDBG_LOW("gsi base addr 0x%llx end addr 0x%llx\n",
		ctx->base, ctx->end);

	if (addr1 < ctx->base || addr1 >= ctx->end) {
		GSIERR("address = 0x%llx not in range\n", addr1);
		BUG();
	}

	if (addr2 < ctx->base || addr2 >= ctx->end) {
		GSIERR("address = 0x%llx not in range\n", addr2);
		BUG();
	}

	addr_diff = (uint32_t)(addr2 - addr1);
	if (addr1 < addr2)
		return addr_diff / ctx->elem_sz;
	else
		return (addr_diff + ctx->len) / ctx->elem_sz;
}

static void gsi_process_chan(struct gsi_xfer_compl_evt *evt,
		struct gsi_chan_xfer_notify *notify, bool callback)
{
	uint32_t ch_id;
	struct gsi_chan_ctx *ch_ctx;
	uint16_t rp_idx;
	uint64_t rp;

	ch_id = evt->chid;
	if (WARN_ON(ch_id >= gsi_ctx->max_ch)) {
		GSIERR("Unexpected ch %d\n", ch_id);
		return;
	}

	ch_ctx = &gsi_ctx->chan[ch_id];
	if (WARN_ON(ch_ctx->props.prot != GSI_CHAN_PROT_GPI &&
		ch_ctx->props.prot != GSI_CHAN_PROT_GCI))
		return;

	if (evt->type != GSI_XFER_COMPL_TYPE_GCI) {
		rp = evt->xfer_ptr;

		if (ch_ctx->ring.rp_local != rp) {
			ch_ctx->stats.completed +=
				gsi_get_complete_num(&ch_ctx->ring,
				ch_ctx->ring.rp_local, rp);
			ch_ctx->ring.rp_local = rp;
		}


		/* the element at RP is also processed */
		gsi_incr_ring_rp(&ch_ctx->ring);

		ch_ctx->ring.rp = ch_ctx->ring.rp_local;
		rp_idx = gsi_find_idx_from_addr(&ch_ctx->ring, rp);
		notify->veid = GSI_VEID_DEFAULT;
	} else {
		rp_idx = evt->cookie;
		notify->veid = evt->veid;
	}

	ch_ctx->stats.completed++;

	WARN_ON(!ch_ctx->user_data[rp_idx].valid);
	notify->xfer_user_data = ch_ctx->user_data[rp_idx].p;
	ch_ctx->user_data[rp_idx].valid = false;

	notify->chan_user_data = ch_ctx->props.chan_user_data;
	notify->evt_id = evt->code;
	notify->bytes_xfered = evt->len;

	if (callback) {
		if (atomic_read(&ch_ctx->poll_mode)) {
			GSIERR("Calling client callback in polling mode\n");
			WARN_ON(1);
		}
		ch_ctx->props.xfer_cb(notify);
	}
}

static void gsi_process_evt_re(struct gsi_evt_ctx *ctx,
		struct gsi_chan_xfer_notify *notify, bool callback)
{
	struct gsi_xfer_compl_evt *evt;

	evt = (struct gsi_xfer_compl_evt *)(ctx->ring.base_va +
			ctx->ring.rp_local - ctx->ring.base);
	gsi_process_chan(evt, notify, callback);
	gsi_incr_ring_rp(&ctx->ring);
	/* recycle this element */
	gsi_incr_ring_wp(&ctx->ring);
	ctx->stats.completed++;
}

static void gsi_ring_evt_doorbell(struct gsi_evt_ctx *ctx)
{
	uint32_t val;

	ctx->ring.wp = ctx->ring.wp_local;
	val = (ctx->ring.wp_local &
			GSI_EE_n_EV_CH_k_DOORBELL_0_WRITE_PTR_LSB_BMSK) <<
			GSI_EE_n_EV_CH_k_DOORBELL_0_WRITE_PTR_LSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_DOORBELL_0_OFFS(ctx->id,
				gsi_ctx->per.ee));
}

static void gsi_ring_chan_doorbell(struct gsi_chan_ctx *ctx)
{
	uint32_t val;

	/*
	 * allocate new events for this channel first
	 * before submitting the new TREs.
	 * for TO_GSI channels the event ring doorbell is rang as part of
	 * interrupt handling.
	 */
	if (ctx->evtr && ctx->props.dir == GSI_CHAN_DIR_FROM_GSI)
		gsi_ring_evt_doorbell(ctx->evtr);
	ctx->ring.wp = ctx->ring.wp_local;

	val = (ctx->ring.wp_local &
			GSI_EE_n_GSI_CH_k_DOORBELL_0_WRITE_PTR_LSB_BMSK) <<
			GSI_EE_n_GSI_CH_k_DOORBELL_0_WRITE_PTR_LSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_DOORBELL_0_OFFS(ctx->props.ch_id,
				gsi_ctx->per.ee));
}

static void gsi_handle_ieob(int ee)
{
	uint32_t ch;
	int i;
	uint64_t rp;
	struct gsi_evt_ctx *ctx;
	struct gsi_chan_xfer_notify notify;
	unsigned long flags;
	unsigned long cntr;
	uint32_t msk;

	ch = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_OFFS(ee));
	msk = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_MSK_OFFS(ee));
	gsi_writel(ch & msk, gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(ee));

	for (i = 0; i < GSI_STTS_REG_BITS; i++) {
		if ((1 << i) & ch & msk) {
			if (i >= gsi_ctx->max_ev || i >= GSI_EVT_RING_MAX) {
				GSIERR("invalid event %d\n", i);
				break;
			}
			ctx = &gsi_ctx->evtr[i];

			/*
			 * Don't handle MSI interrupts, only handle IEOB
			 * IRQs
			 */
			if (ctx->props.intr == GSI_INTR_MSI)
				continue;

			if (ctx->props.intf != GSI_EVT_CHTYPE_GPI_EV) {
				GSIERR("Unexpected irq intf %d\n",
					ctx->props.intf);
				BUG();
			}
			spin_lock_irqsave(&ctx->ring.slock, flags);
check_again:
			cntr = 0;
			rp = gsi_readl(gsi_ctx->base +
				GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(i, ee));
			rp |= ctx->ring.rp & 0xFFFFFFFF00000000;

			ctx->ring.rp = rp;
			while (ctx->ring.rp_local != rp) {
				++cntr;
				if (ctx->props.exclusive &&
					atomic_read(&ctx->chan->poll_mode)) {
					cntr = 0;
					break;
				}
				gsi_process_evt_re(ctx, &notify, true);
			}
			gsi_ring_evt_doorbell(ctx);
			if (cntr != 0)
				goto check_again;
			spin_unlock_irqrestore(&ctx->ring.slock, flags);
		}
	}
}

static void gsi_handle_inter_ee_ch_ctrl(int ee)
{
	uint32_t ch;
	int i;

	ch = gsi_readl(gsi_ctx->base +
		GSI_INTER_EE_n_SRC_GSI_CH_IRQ_OFFS(ee));
	gsi_writel(ch, gsi_ctx->base +
		GSI_INTER_EE_n_SRC_GSI_CH_IRQ_CLR_OFFS(ee));
	for (i = 0; i < GSI_STTS_REG_BITS; i++) {
		if ((1 << i) & ch) {
			/* not currently expected */
			GSIERR("ch %u was inter-EE changed\n", i);
		}
	}
}

static void gsi_handle_inter_ee_ev_ctrl(int ee)
{
	uint32_t ch;
	int i;

	ch = gsi_readl(gsi_ctx->base +
		GSI_INTER_EE_n_SRC_EV_CH_IRQ_OFFS(ee));
	gsi_writel(ch, gsi_ctx->base +
		GSI_INTER_EE_n_SRC_EV_CH_IRQ_CLR_OFFS(ee));
	for (i = 0; i < GSI_STTS_REG_BITS; i++) {
		if ((1 << i) & ch) {
			/* not currently expected */
			GSIERR("evt %u was inter-EE changed\n", i);
		}
	}
}

static void gsi_handle_general(int ee)
{
	uint32_t val;
	struct gsi_per_notify notify;

	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_GSI_IRQ_STTS_OFFS(ee));

	notify.user_data = gsi_ctx->per.user_data;

	if (val & GSI_EE_n_CNTXT_GSI_IRQ_CLR_GSI_MCS_STACK_OVRFLOW_BMSK)
		notify.evt_id = GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW;

	if (val & GSI_EE_n_CNTXT_GSI_IRQ_CLR_GSI_CMD_FIFO_OVRFLOW_BMSK)
		notify.evt_id = GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW;

	if (val & GSI_EE_n_CNTXT_GSI_IRQ_CLR_GSI_BUS_ERROR_BMSK)
		notify.evt_id = GSI_PER_EVT_GENERAL_BUS_ERROR;

	if (val & GSI_EE_n_CNTXT_GSI_IRQ_CLR_GSI_BREAK_POINT_BMSK)
		notify.evt_id = GSI_PER_EVT_GENERAL_BREAK_POINT;

	if (gsi_ctx->per.notify_cb)
		gsi_ctx->per.notify_cb(&notify);

	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_CNTXT_GSI_IRQ_CLR_OFFS(ee));
}

#define GSI_ISR_MAX_ITER 50

static void gsi_handle_irq(void)
{
	uint32_t type;
	int ee = gsi_ctx->per.ee;
	unsigned long cnt = 0;

	while (1) {
		type = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_TYPE_IRQ_OFFS(ee));

		if (!type)
			break;

		GSIDBG_LOW("type 0x%x\n", type);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_CH_CTRL_BMSK)
			gsi_handle_ch_ctrl(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_EV_CTRL_BMSK)
			gsi_handle_ev_ctrl(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_GLOB_EE_BMSK)
			gsi_handle_glob_ee(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_IEOB_BMSK)
			gsi_handle_ieob(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_INTER_EE_CH_CTRL_BMSK)
			gsi_handle_inter_ee_ch_ctrl(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_INTER_EE_EV_CTRL_BMSK)
			gsi_handle_inter_ee_ev_ctrl(ee);

		if (type & GSI_EE_n_CNTXT_TYPE_IRQ_GENERAL_BMSK)
			gsi_handle_general(ee);

		if (++cnt > GSI_ISR_MAX_ITER) {
			/*
			 * Max number of spurious interrupts from hardware.
			 * Unexpected hardware state.
			 */
			GSIERR("Too many spurious interrupt from GSI HW\n");
			BUG();
		}

	}
}

static irqreturn_t gsi_isr(int irq, void *ctxt)
{
	if (gsi_ctx->per.req_clk_cb) {
		bool granted = false;

		gsi_ctx->per.req_clk_cb(gsi_ctx->per.user_data, &granted);
		if (granted) {
			gsi_handle_irq();
			gsi_ctx->per.rel_clk_cb(gsi_ctx->per.user_data);
		}
	} else if (!gsi_ctx->per.clk_status_cb()) {
		return IRQ_HANDLED;
	} else {
		gsi_handle_irq();
	}
	return IRQ_HANDLED;

}

static uint32_t gsi_get_max_channels(enum gsi_ver ver)
{
	uint32_t reg = 0;

	switch (ver) {
	case GSI_VER_ERR:
	case GSI_VER_MAX:
		GSIERR("GSI version is not supported %d\n", ver);
		WARN_ON(1);
		break;
	case GSI_VER_1_0:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_0_EE_n_GSI_HW_PARAM_OFFS(gsi_ctx->per.ee));
		reg = (reg & GSI_V1_0_EE_n_GSI_HW_PARAM_GSI_CH_NUM_BMSK) >>
			GSI_V1_0_EE_n_GSI_HW_PARAM_GSI_CH_NUM_SHFT;
		break;
	case GSI_VER_1_2:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_2_EE_n_GSI_HW_PARAM_0_OFFS(gsi_ctx->per.ee));
		reg = (reg & GSI_V1_2_EE_n_GSI_HW_PARAM_0_GSI_CH_NUM_BMSK) >>
			GSI_V1_2_EE_n_GSI_HW_PARAM_0_GSI_CH_NUM_SHFT;
		break;
	case GSI_VER_1_3:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_BMSK) >>
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_SHFT;
		break;
	case GSI_VER_2_0:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_BMSK) >>
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_SHFT;
		break;
	case GSI_VER_2_2:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_BMSK) >>
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_SHFT;
		break;
	case GSI_VER_2_5:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_BMSK) >>
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_GSI_NUM_CH_PER_EE_SHFT;
		break;
	}

	GSIDBG("max channels %d\n", reg);

	return reg;
}

static uint32_t gsi_get_max_event_rings(enum gsi_ver ver)
{
	uint32_t reg = 0;

	switch (ver) {
	case GSI_VER_ERR:
	case GSI_VER_MAX:
		GSIERR("GSI version is not supported %d\n", ver);
		WARN_ON(1);
		break;
	case GSI_VER_1_0:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_0_EE_n_GSI_HW_PARAM_OFFS(gsi_ctx->per.ee));
		reg = (reg & GSI_V1_0_EE_n_GSI_HW_PARAM_GSI_EV_CH_NUM_BMSK) >>
			GSI_V1_0_EE_n_GSI_HW_PARAM_GSI_EV_CH_NUM_SHFT;
		break;
	case GSI_VER_1_2:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_2_EE_n_GSI_HW_PARAM_0_OFFS(gsi_ctx->per.ee));
		reg = (reg & GSI_V1_2_EE_n_GSI_HW_PARAM_0_GSI_EV_CH_NUM_BMSK) >>
			GSI_V1_2_EE_n_GSI_HW_PARAM_0_GSI_EV_CH_NUM_SHFT;
		break;
	case GSI_VER_1_3:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_BMSK) >>
			GSI_V1_3_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_SHFT;
		break;
	case GSI_VER_2_0:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_BMSK) >>
			GSI_V2_0_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_SHFT;
		break;
	case GSI_VER_2_2:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_BMSK) >>
			GSI_V2_2_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_SHFT;
		break;
	case GSI_VER_2_5:
		reg = gsi_readl(gsi_ctx->base +
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_OFFS(gsi_ctx->per.ee));
		reg = (reg &
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_BMSK) >>
			GSI_V2_5_EE_n_GSI_HW_PARAM_2_GSI_NUM_EV_PER_EE_SHFT;
		break;
	}

	GSIDBG("max event rings %d\n", reg);

	return reg;
}
int gsi_complete_clk_grant(unsigned long dev_hdl)
{
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->per_registered) {
		GSIERR("no client registered\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%pK\n", dev_hdl,
				gsi_ctx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	spin_lock_irqsave(&gsi_ctx->slock, flags);
	gsi_handle_irq();
	gsi_ctx->per.rel_clk_cb(gsi_ctx->per.user_data);
	spin_unlock_irqrestore(&gsi_ctx->slock, flags);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_complete_clk_grant);

int gsi_map_base(phys_addr_t gsi_base_addr, u32 gsi_size)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	gsi_ctx->base = devm_ioremap_nocache(
		gsi_ctx->dev, gsi_base_addr, gsi_size);

	if (!gsi_ctx->base) {
		GSIERR("failed to map access to GSI HW\n");
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	GSIDBG("GSI base(%pa) mapped to (%pK) with len (0x%x)\n",
		&gsi_base_addr,
		gsi_ctx->base,
		gsi_size);

	return 0;
}
EXPORT_SYMBOL(gsi_map_base);

int gsi_unmap_base(void)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->base) {
		GSIERR("access to GSI HW has not been mapped\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	devm_iounmap(gsi_ctx->dev, gsi_ctx->base);

	gsi_ctx->base = NULL;

	return 0;
}
EXPORT_SYMBOL(gsi_unmap_base);

int gsi_register_device(struct gsi_per_props *props, unsigned long *dev_hdl)
{
	int res;
	uint32_t val;
	int needed_reg_ver;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !dev_hdl) {
		GSIERR("bad params props=%pK dev_hdl=%pK\n", props, dev_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->ver <= GSI_VER_ERR || props->ver >= GSI_VER_MAX) {
		GSIERR("bad params gsi_ver=%d\n", props->ver);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!props->notify_cb) {
		GSIERR("notify callback must be provided\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->req_clk_cb && !props->rel_clk_cb) {
		GSIERR("rel callback  must be provided\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_ctx->per_registered) {
		GSIERR("per already registered\n");
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	switch (props->ver) {
	case GSI_VER_1_0:
	case GSI_VER_1_2:
	case GSI_VER_1_3:
	case GSI_VER_2_0:
	case GSI_VER_2_2:
		needed_reg_ver = GSI_REGISTER_VER_1;
		break;
	case GSI_VER_2_5:
		needed_reg_ver = GSI_REGISTER_VER_2;
		break;
	case GSI_VER_ERR:
	case GSI_VER_MAX:
	default:
		GSIERR("GSI version is not supported %d\n", props->ver);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (needed_reg_ver != GSI_REGISTER_VER_CURRENT) {
		GSIERR("Invalid register version. current=%d, needed=%d\n",
			GSI_REGISTER_VER_CURRENT, needed_reg_ver);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}
	GSIDBG("gsi ver %d register ver %d needed register ver %d\n",
		props->ver, GSI_REGISTER_VER_CURRENT, needed_reg_ver);

	spin_lock_init(&gsi_ctx->slock);
	if (props->intr == GSI_INTR_IRQ) {
		if (!props->irq) {
			GSIERR("bad irq specified %u\n", props->irq);
			return -GSI_STATUS_INVALID_PARAMS;
		}
		/*
		 * On a real UE, there are two separate interrupt
		 * vectors that get directed toward the GSI/IPA
		 * drivers.  They are handled by gsi_isr() and
		 * (ipa_isr() or ipa3_isr()) respectively.  In the
		 * emulation environment, this is not the case;
		 * instead, interrupt vectors are routed to the
		 * emualation hardware's interrupt controller, which
		 * in turn, forwards a single interrupt to the GSI/IPA
		 * driver.  When the new interrupt vector is received,
		 * the driver needs to probe the interrupt
		 * controller's registers so see if one, the other, or
		 * both interrupts have occurred.  Given the above, we
		 * now need to handle both situations, namely: the
		 * emulator's and the real UE.
		 */
		if (running_emulation) {
			/*
			 * New scheme involving the emulator's
			 * interrupt controller.
			 */
			res = devm_request_threaded_irq(
				gsi_ctx->dev,
				props->irq,
				/* top half handler to follow */
				emulator_hard_irq_isr,
				/* threaded bottom half handler to follow */
				emulator_soft_irq_isr,
				IRQF_SHARED,
				"emulator_intcntrlr",
				gsi_ctx);
		} else {
			/*
			 * Traditional scheme used on the real UE.
			 */
			res = devm_request_irq(gsi_ctx->dev, props->irq,
				gsi_isr,
				props->req_clk_cb ? IRQF_TRIGGER_RISING :
					IRQF_TRIGGER_HIGH,
				"gsi",
				gsi_ctx);
		}
		if (res) {
			GSIERR(
			 "failed to register isr for %u\n",
			 props->irq);
			return -GSI_STATUS_ERROR;
		}
		GSIDBG(
			"succeeded to register isr for %u\n",
			props->irq);

		res = enable_irq_wake(props->irq);
		if (res)
			GSIERR("failed to enable wake irq %u\n", props->irq);
		else
			GSIERR("GSI irq is wake enabled %u\n", props->irq);

	} else {
		GSIERR("do not support interrupt type %u\n", props->intr);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	/*
	 * If base not previously mapped via gsi_map_base(), map it
	 * now...
	 */
	if (!gsi_ctx->base) {
		res = gsi_map_base(props->phys_addr, props->size);
		if (res)
			return res;
	}

	if (running_emulation) {
		GSIDBG("GSI SW ver register value 0x%x\n",
		       gsi_readl(gsi_ctx->base +
		       GSI_EE_n_GSI_SW_VERSION_OFFS(0)));
		gsi_ctx->intcntrlr_mem_size =
		    props->emulator_intcntrlr_size;
		gsi_ctx->intcntrlr_base =
		    devm_ioremap_nocache(
			gsi_ctx->dev,
			props->emulator_intcntrlr_addr,
			props->emulator_intcntrlr_size);
		if (!gsi_ctx->intcntrlr_base) {
			GSIERR(
			  "failed to remap emulator's interrupt controller HW\n");
			gsi_unmap_base();
			devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
			return -GSI_STATUS_RES_ALLOC_FAILURE;
		}

		GSIDBG(
		    "Emulator's interrupt controller base(%pa) mapped to (%pK) with len (0x%lx)\n",
		    &(props->emulator_intcntrlr_addr),
		    gsi_ctx->intcntrlr_base,
		    props->emulator_intcntrlr_size);

		gsi_ctx->intcntrlr_gsi_isr = gsi_isr;
		gsi_ctx->intcntrlr_client_isr =
		    props->emulator_intcntrlr_client_isr;
	}

	gsi_ctx->per = *props;
	gsi_ctx->per_registered = true;
	mutex_init(&gsi_ctx->mlock);
	atomic_set(&gsi_ctx->num_chan, 0);
	atomic_set(&gsi_ctx->num_evt_ring, 0);
	gsi_ctx->max_ch = gsi_get_max_channels(gsi_ctx->per.ver);
	if (gsi_ctx->max_ch == 0) {
		gsi_unmap_base();
		if (running_emulation)
			devm_iounmap(gsi_ctx->dev, gsi_ctx->intcntrlr_base);
		gsi_ctx->base = gsi_ctx->intcntrlr_base = NULL;
		devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
		GSIERR("failed to get max channels\n");
		return -GSI_STATUS_ERROR;
	}
	gsi_ctx->max_ev = gsi_get_max_event_rings(gsi_ctx->per.ver);
	if (gsi_ctx->max_ev == 0) {
		gsi_unmap_base();
		if (running_emulation)
			devm_iounmap(gsi_ctx->dev, gsi_ctx->intcntrlr_base);
		gsi_ctx->base = gsi_ctx->intcntrlr_base = NULL;
		devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
		GSIERR("failed to get max event rings\n");
		return -GSI_STATUS_ERROR;
	}

	if (gsi_ctx->max_ev > GSI_EVT_RING_MAX) {
		GSIERR("max event rings are beyond absolute maximum\n");
		return -GSI_STATUS_ERROR;
	}

	if (props->mhi_er_id_limits_valid &&
	    props->mhi_er_id_limits[0] > (gsi_ctx->max_ev - 1)) {
		gsi_unmap_base();
		if (running_emulation)
			devm_iounmap(gsi_ctx->dev, gsi_ctx->intcntrlr_base);
		gsi_ctx->base = gsi_ctx->intcntrlr_base = NULL;
		devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
		GSIERR("MHI event ring start id %u is beyond max %u\n",
			props->mhi_er_id_limits[0], gsi_ctx->max_ev);
		return -GSI_STATUS_ERROR;
	}

	gsi_ctx->evt_bmap = ~((1 << gsi_ctx->max_ev) - 1);

	/* exclude reserved mhi events */
	if (props->mhi_er_id_limits_valid)
		gsi_ctx->evt_bmap |=
			((1 << (props->mhi_er_id_limits[1] + 1)) - 1) ^
			((1 << (props->mhi_er_id_limits[0])) - 1);

	/*
	 * enable all interrupts but GSI_BREAK_POINT.
	 * Inter EE commands / interrupt are no supported.
	 */
	__gsi_config_type_irq(props->ee, ~0, ~0);
	__gsi_config_ch_irq(props->ee, ~0, ~0);
	__gsi_config_evt_irq(props->ee, ~0, ~0);
	__gsi_config_ieob_irq(props->ee, ~0, ~0);
	__gsi_config_glob_irq(props->ee, ~0, ~0);
	__gsi_config_gen_irq(props->ee, ~0,
		~GSI_EE_n_CNTXT_GSI_IRQ_CLR_GSI_BREAK_POINT_BMSK);

	if (gsi_ctx->per.ver == GSI_VER_2_2)
		__gsi_config_glob_irq(props->ee,
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK, 0);
	gsi_writel(props->intr, gsi_ctx->base +
			GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));
	/* set GSI_TOP_EE_n_CNTXT_MSI_BASE_LSB/MSB to 0 */
	if ((gsi_ctx->per.ver >= GSI_VER_2_0) &&
		(props->intr != GSI_INTR_MSI)) {
		gsi_writel(0, gsi_ctx->base +
			GSI_EE_n_CNTXT_MSI_BASE_LSB(gsi_ctx->per.ee));
		gsi_writel(0, gsi_ctx->base +
			GSI_EE_n_CNTXT_MSI_BASE_MSB(gsi_ctx->per.ee));
	}

	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_GSI_STATUS_OFFS(gsi_ctx->per.ee));
	if (val & GSI_EE_n_GSI_STATUS_ENABLED_BMSK)
		gsi_ctx->enabled = true;
	else
		GSIERR("Manager EE has not enabled GSI, GSI un-usable\n");

	if (gsi_ctx->per.ver >= GSI_VER_1_2)
		gsi_writel(0, gsi_ctx->base +
			GSI_EE_n_ERROR_LOG_OFFS(gsi_ctx->per.ee));

	if (running_emulation) {
		/*
		 * Set up the emulator's interrupt controller...
		 */
		res = setup_emulator_cntrlr(
		    gsi_ctx->intcntrlr_base, gsi_ctx->intcntrlr_mem_size);
		if (res != 0) {
			gsi_unmap_base();
			devm_iounmap(gsi_ctx->dev, gsi_ctx->intcntrlr_base);
			gsi_ctx->base = gsi_ctx->intcntrlr_base = NULL;
			devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
			GSIERR("setup_emulator_cntrlr() failed\n");
			return res;
		}
	}

	*dev_hdl = (uintptr_t)gsi_ctx;

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_register_device);

int gsi_write_device_scratch(unsigned long dev_hdl,
		struct gsi_device_scratch *val)
{
	unsigned int max_usb_pkt_size = 0;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->per_registered) {
		GSIERR("no client registered\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%pK\n", dev_hdl,
				gsi_ctx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (val->max_usb_pkt_size_valid &&
			val->max_usb_pkt_size != 1024 &&
			val->max_usb_pkt_size != 512 &&
			val->max_usb_pkt_size != 64) {
		GSIERR("bad USB max pkt size dev_hdl=0x%lx sz=%u\n", dev_hdl,
				val->max_usb_pkt_size);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	mutex_lock(&gsi_ctx->mlock);
	if (val->mhi_base_chan_idx_valid)
		gsi_ctx->scratch.word0.s.mhi_base_chan_idx =
			val->mhi_base_chan_idx;

	if (val->max_usb_pkt_size_valid) {
		max_usb_pkt_size = 2;
		if (val->max_usb_pkt_size > 64)
			max_usb_pkt_size =
				(val->max_usb_pkt_size == 1024) ? 1 : 0;
		gsi_ctx->scratch.word0.s.max_usb_pkt_size = max_usb_pkt_size;
	}

	gsi_writel(gsi_ctx->scratch.word0.val,
			gsi_ctx->base +
			GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	mutex_unlock(&gsi_ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_write_device_scratch);

int gsi_deregister_device(unsigned long dev_hdl, bool force)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->per_registered) {
		GSIERR("no client registered\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%pK\n", dev_hdl,
				gsi_ctx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!force && atomic_read(&gsi_ctx->num_chan)) {
		GSIERR("cannot deregister %u channels are still connected\n",
				atomic_read(&gsi_ctx->num_chan));
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (!force && atomic_read(&gsi_ctx->num_evt_ring)) {
		GSIERR("cannot deregister %u events are still connected\n",
				atomic_read(&gsi_ctx->num_evt_ring));
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	/* disable all interrupts */
	__gsi_config_type_irq(gsi_ctx->per.ee, ~0, 0);
	__gsi_config_ch_irq(gsi_ctx->per.ee, ~0, 0);
	__gsi_config_evt_irq(gsi_ctx->per.ee, ~0, 0);
	__gsi_config_ieob_irq(gsi_ctx->per.ee, ~0, 0);
	__gsi_config_glob_irq(gsi_ctx->per.ee, ~0, 0);
	__gsi_config_gen_irq(gsi_ctx->per.ee, ~0, 0);

	devm_free_irq(gsi_ctx->dev, gsi_ctx->per.irq, gsi_ctx);
	gsi_unmap_base();
	memset(gsi_ctx, 0, sizeof(*gsi_ctx));

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_deregister_device);

static void gsi_program_evt_ring_ctx(struct gsi_evt_ring_props *props,
		uint8_t evt_id, unsigned int ee)
{
	uint32_t val;

	GSIDBG("intf=%u intr=%u re=%u\n", props->intf, props->intr,
			props->re_size);

	val = (((props->intf << GSI_EE_n_EV_CH_k_CNTXT_0_CHTYPE_SHFT) &
			GSI_EE_n_EV_CH_k_CNTXT_0_CHTYPE_BMSK) |
		((props->intr << GSI_EE_n_EV_CH_k_CNTXT_0_INTYPE_SHFT) &
			GSI_EE_n_EV_CH_k_CNTXT_0_INTYPE_BMSK) |
		((props->re_size << GSI_EE_n_EV_CH_k_CNTXT_0_ELEMENT_SIZE_SHFT)
			& GSI_EE_n_EV_CH_k_CNTXT_0_ELEMENT_SIZE_BMSK));

	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_0_OFFS(evt_id, ee));

	val = (props->ring_len & GSI_EE_n_EV_CH_k_CNTXT_1_R_LENGTH_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_1_R_LENGTH_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_1_OFFS(evt_id, ee));

	val = (props->ring_base_addr &
			GSI_EE_n_EV_CH_k_CNTXT_2_R_BASE_ADDR_LSBS_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_2_R_BASE_ADDR_LSBS_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_2_OFFS(evt_id, ee));

	val = ((props->ring_base_addr >> 32) &
		GSI_EE_n_EV_CH_k_CNTXT_3_R_BASE_ADDR_MSBS_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_3_R_BASE_ADDR_MSBS_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_3_OFFS(evt_id, ee));

	val = (((props->int_modt << GSI_EE_n_EV_CH_k_CNTXT_8_INT_MODT_SHFT) &
		GSI_EE_n_EV_CH_k_CNTXT_8_INT_MODT_BMSK) |
		((props->int_modc << GSI_EE_n_EV_CH_k_CNTXT_8_INT_MODC_SHFT) &
		 GSI_EE_n_EV_CH_k_CNTXT_8_INT_MODC_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_8_OFFS(evt_id, ee));

	val = (props->intvec & GSI_EE_n_EV_CH_k_CNTXT_9_INTVEC_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_9_INTVEC_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_9_OFFS(evt_id, ee));

	val = (props->msi_addr & GSI_EE_n_EV_CH_k_CNTXT_10_MSI_ADDR_LSB_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_10_MSI_ADDR_LSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_10_OFFS(evt_id, ee));

	val = ((props->msi_addr >> 32) &
		GSI_EE_n_EV_CH_k_CNTXT_11_MSI_ADDR_MSB_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_11_MSI_ADDR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_11_OFFS(evt_id, ee));

	val = (props->rp_update_addr &
			GSI_EE_n_EV_CH_k_CNTXT_12_RP_UPDATE_ADDR_LSB_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_12_RP_UPDATE_ADDR_LSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_12_OFFS(evt_id, ee));

	val = ((props->rp_update_addr >> 32) &
		GSI_EE_n_EV_CH_k_CNTXT_13_RP_UPDATE_ADDR_MSB_BMSK) <<
		GSI_EE_n_EV_CH_k_CNTXT_13_RP_UPDATE_ADDR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_13_OFFS(evt_id, ee));
}

static void gsi_init_evt_ring(struct gsi_evt_ring_props *props,
		struct gsi_ring_ctx *ctx)
{
	ctx->base_va = (uintptr_t)props->ring_base_vaddr;
	ctx->base = props->ring_base_addr;
	ctx->wp = ctx->base;
	ctx->rp = ctx->base;
	ctx->wp_local = ctx->base;
	ctx->rp_local = ctx->base;
	ctx->len = props->ring_len;
	ctx->elem_sz = props->re_size;
	ctx->max_num_elem = ctx->len / ctx->elem_sz - 1;
	ctx->end = ctx->base + (ctx->max_num_elem + 1) * ctx->elem_sz;
}

static void gsi_prime_evt_ring(struct gsi_evt_ctx *ctx)
{
	unsigned long flags;
	uint32_t val;

	spin_lock_irqsave(&ctx->ring.slock, flags);
	memset((void *)ctx->ring.base_va, 0, ctx->ring.len);
	ctx->ring.wp_local = ctx->ring.base +
		ctx->ring.max_num_elem * ctx->ring.elem_sz;

	/* write order MUST be MSB followed by LSB */
	val = ((ctx->ring.wp_local >> 32) &
		GSI_EE_n_EV_CH_k_DOORBELL_1_WRITE_PTR_MSB_BMSK) <<
		GSI_EE_n_EV_CH_k_DOORBELL_1_WRITE_PTR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
		GSI_EE_n_EV_CH_k_DOORBELL_1_OFFS(ctx->id,
		gsi_ctx->per.ee));

	gsi_ring_evt_doorbell(ctx);
	spin_unlock_irqrestore(&ctx->ring.slock, flags);
}

static void gsi_prime_evt_ring_wdi(struct gsi_evt_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->ring.slock, flags);
	if (ctx->ring.base_va)
		memset((void *)ctx->ring.base_va, 0, ctx->ring.len);
	ctx->ring.wp_local = ctx->ring.base +
		((ctx->ring.max_num_elem + 2) * ctx->ring.elem_sz);
	gsi_ring_evt_doorbell(ctx);
	spin_unlock_irqrestore(&ctx->ring.slock, flags);
}

static int gsi_validate_evt_ring_props(struct gsi_evt_ring_props *props)
{
	uint64_t ra;

	if ((props->re_size == GSI_EVT_RING_RE_SIZE_4B &&
				props->ring_len % 4) ||
			(props->re_size == GSI_EVT_RING_RE_SIZE_8B &&
				 props->ring_len % 8) ||
			(props->re_size == GSI_EVT_RING_RE_SIZE_16B &&
				 props->ring_len % 16)) {
		GSIERR("bad params ring_len %u not a multiple of RE size %u\n",
				props->ring_len, props->re_size);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ra = props->ring_base_addr;
	do_div(ra, roundup_pow_of_two(props->ring_len));

	if (props->ring_base_addr != ra * roundup_pow_of_two(props->ring_len)) {
		GSIERR("bad params ring base not aligned 0x%llx align 0x%lx\n",
				props->ring_base_addr,
				roundup_pow_of_two(props->ring_len));
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->intf == GSI_EVT_CHTYPE_GPI_EV &&
			!props->ring_base_vaddr) {
		GSIERR("protocol %u requires ring base VA\n", props->intf);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->intf == GSI_EVT_CHTYPE_MHI_EV &&
			(!props->evchid_valid ||
			props->evchid > gsi_ctx->per.mhi_er_id_limits[1] ||
			props->evchid < gsi_ctx->per.mhi_er_id_limits[0])) {
		GSIERR("MHI requires evchid valid=%d val=%u\n",
				props->evchid_valid, props->evchid);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->intf != GSI_EVT_CHTYPE_MHI_EV &&
			props->evchid_valid) {
		GSIERR("protocol %u cannot specify evchid\n", props->intf);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!props->err_cb) {
		GSIERR("err callback must be provided\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	return GSI_STATUS_SUCCESS;
}

/**
 * gsi_cleanup_xfer_user_data: cleanup the user data array using callback passed
 *	by IPA driver. Need to do this in GSI since only GSI knows which TRE
 *	are being used or not. However, IPA is the one that does cleaning,
 *	therefore we pass a callback from IPA and call it using params from GSI
 *
 * @chan_hdl: hdl of the gsi channel user data array to be cleaned
 * @cleanup_cb: callback used to clean the user data array. takes 2 inputs
 *	@chan_user_data: ipa_sys_context of the gsi_channel
 *	@xfer_uder_data: user data array element (rx_pkt wrapper)
 *
 * Returns: 0 on success, negative on failure
 */
static int gsi_cleanup_xfer_user_data(unsigned long chan_hdl,
	void (*cleanup_cb)(void *chan_user_data, void *xfer_user_data))
{
	struct gsi_chan_ctx *ctx;
	uint64_t i;
	uint16_t rp_idx;

	ctx = &gsi_ctx->chan[chan_hdl];
	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	/* for coalescing, traverse the whole array */
	if (ctx->props.prot == GSI_CHAN_PROT_GCI) {
		size_t user_data_size =
			ctx->ring.max_num_elem + 1 + GSI_VEID_MAX;
		for (i = 0; i < user_data_size; i++) {
			if (ctx->user_data[i].valid)
				cleanup_cb(ctx->props.chan_user_data,
					ctx->user_data[i].p);
		}
	} else {
		/* for non-coalescing, clean between RP and WP */
		while (ctx->ring.rp_local != ctx->ring.wp_local) {
			rp_idx = gsi_find_idx_from_addr(&ctx->ring,
				ctx->ring.rp_local);
			WARN_ON(!ctx->user_data[rp_idx].valid);
			cleanup_cb(ctx->props.chan_user_data,
				ctx->user_data[rp_idx].p);
			gsi_incr_ring_rp(&ctx->ring);
		}
	}
	return 0;
}

int gsi_alloc_evt_ring(struct gsi_evt_ring_props *props, unsigned long dev_hdl,
		unsigned long *evt_ring_hdl)
{
	unsigned long evt_id;
	enum gsi_evt_ch_cmd_opcode op = GSI_EVT_ALLOCATE;
	uint32_t val;
	struct gsi_evt_ctx *ctx;
	int res;
	int ee;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !evt_ring_hdl || dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params props=%pK dev_hdl=0x%lx evt_ring_hdl=%pK\n",
				props, dev_hdl, evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_validate_evt_ring_props(props)) {
		GSIERR("invalid params\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!props->evchid_valid) {
		mutex_lock(&gsi_ctx->mlock);
		evt_id = find_first_zero_bit(&gsi_ctx->evt_bmap,
				sizeof(unsigned long) * BITS_PER_BYTE);
		if (evt_id == sizeof(unsigned long) * BITS_PER_BYTE) {
			GSIERR("failed to alloc event ID\n");
			mutex_unlock(&gsi_ctx->mlock);
			return -GSI_STATUS_RES_ALLOC_FAILURE;
		}
		set_bit(evt_id, &gsi_ctx->evt_bmap);
		mutex_unlock(&gsi_ctx->mlock);
	} else {
		evt_id = props->evchid;
	}
	GSIDBG("Using %lu as virt evt id\n", evt_id);

	ctx = &gsi_ctx->evtr[evt_id];
	memset(ctx, 0, sizeof(*ctx));
	mutex_init(&ctx->mlock);
	init_completion(&ctx->compl);
	atomic_set(&ctx->chan_ref_cnt, 0);
	ctx->props = *props;

	mutex_lock(&gsi_ctx->mlock);
	val = (((evt_id << GSI_EE_n_EV_CH_CMD_CHID_SHFT) &
			GSI_EE_n_EV_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_EV_CH_CMD_OPCODE_SHFT) &
			GSI_EE_n_EV_CH_CMD_OPCODE_BMSK));
	ee = gsi_ctx->per.ee;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_CMD_OFFS(ee));
	res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
	if (res == 0) {
		GSIERR("evt_id=%lu timed out\n", evt_id);
		if (!props->evchid_valid)
			clear_bit(evt_id, &gsi_ctx->evt_bmap);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_TIMED_OUT;
	}

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("evt_id=%lu allocation failed state=%u\n",
				evt_id, ctx->state);
		if (!props->evchid_valid)
			clear_bit(evt_id, &gsi_ctx->evt_bmap);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	gsi_program_evt_ring_ctx(props, evt_id, gsi_ctx->per.ee);

	spin_lock_init(&ctx->ring.slock);
	gsi_init_evt_ring(props, &ctx->ring);

	ctx->id = evt_id;
	*evt_ring_hdl = evt_id;
	atomic_inc(&gsi_ctx->num_evt_ring);
	if (props->intf == GSI_EVT_CHTYPE_GPI_EV)
		gsi_prime_evt_ring(ctx);
	else if (props->intf == GSI_EVT_CHTYPE_WDI2_EV)
		gsi_prime_evt_ring_wdi(ctx);
	mutex_unlock(&gsi_ctx->mlock);

	spin_lock_irqsave(&gsi_ctx->slock, flags);
	gsi_writel(1 << evt_id, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(ee));

	/* enable ieob interrupts for GPI, enable MSI interrupts */
	if ((props->intf != GSI_EVT_CHTYPE_GPI_EV) &&
		(props->intr != GSI_INTR_MSI))
		__gsi_config_ieob_irq(gsi_ctx->per.ee, 1 << evt_id, 0);
	else
		__gsi_config_ieob_irq(gsi_ctx->per.ee, 1 << ctx->id, ~0);
	spin_unlock_irqrestore(&gsi_ctx->slock, flags);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_alloc_evt_ring);

static void __gsi_write_evt_ring_scratch(unsigned long evt_ring_hdl,
		union __packed gsi_evt_scratch val)
{
	gsi_writel(val.data.word1, gsi_ctx->base +
		GSI_EE_n_EV_CH_k_SCRATCH_0_OFFS(evt_ring_hdl,
			gsi_ctx->per.ee));
	gsi_writel(val.data.word2, gsi_ctx->base +
		GSI_EE_n_EV_CH_k_SCRATCH_1_OFFS(evt_ring_hdl,
			gsi_ctx->per.ee));
}

int gsi_write_evt_ring_scratch(unsigned long evt_ring_hdl,
		union __packed gsi_evt_scratch val)
{
	struct gsi_evt_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n",
				gsi_ctx->evtr[evt_ring_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&ctx->mlock);
	ctx->scratch = val;
	__gsi_write_evt_ring_scratch(evt_ring_hdl, val);
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_write_evt_ring_scratch);

int gsi_dealloc_evt_ring(unsigned long evt_ring_hdl)
{
	uint32_t val;
	enum gsi_evt_ch_cmd_opcode op = GSI_EVT_DE_ALLOC;
	struct gsi_evt_ctx *ctx;
	int res;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev ||
			evt_ring_hdl >= GSI_EVT_RING_MAX) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (atomic_read(&ctx->chan_ref_cnt)) {
		GSIERR("%d channels still using this event ring\n",
			atomic_read(&ctx->chan_ref_cnt));
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	reinit_completion(&ctx->compl);
	val = (((evt_ring_hdl << GSI_EE_n_EV_CH_CMD_CHID_SHFT) &
			GSI_EE_n_EV_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_EV_CH_CMD_OPCODE_SHFT) &
			 GSI_EE_n_EV_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_CMD_OFFS(gsi_ctx->per.ee));
	res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
	if (res == 0) {
		GSIERR("evt_id=%lu timed out\n", evt_ring_hdl);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_TIMED_OUT;
	}

	if (ctx->state != GSI_EVT_RING_STATE_NOT_ALLOCATED) {
		GSIERR("evt_id=%lu unexpected state=%u\n", evt_ring_hdl,
				ctx->state);
		/*
		 * IPA Hardware returned GSI RING not allocated, which is
		 * unexpected hardware state.
		 */
		BUG();
	}
	mutex_unlock(&gsi_ctx->mlock);

	if (!ctx->props.evchid_valid) {
		mutex_lock(&gsi_ctx->mlock);
		clear_bit(evt_ring_hdl, &gsi_ctx->evt_bmap);
		mutex_unlock(&gsi_ctx->mlock);
	}
	atomic_dec(&gsi_ctx->num_evt_ring);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_dealloc_evt_ring);

int gsi_query_evt_ring_db_addr(unsigned long evt_ring_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb)
{
	struct gsi_evt_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!db_addr_wp_msb || !db_addr_wp_lsb) {
		GSIERR("bad params msb=%pK lsb=%pK\n", db_addr_wp_msb,
				db_addr_wp_lsb);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n",
				gsi_ctx->evtr[evt_ring_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	*db_addr_wp_lsb = gsi_ctx->per.phys_addr +
		GSI_EE_n_EV_CH_k_DOORBELL_0_OFFS(evt_ring_hdl, gsi_ctx->per.ee);
	*db_addr_wp_msb = gsi_ctx->per.phys_addr +
		GSI_EE_n_EV_CH_k_DOORBELL_1_OFFS(evt_ring_hdl, gsi_ctx->per.ee);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_query_evt_ring_db_addr);

int gsi_ring_evt_ring_db(unsigned long evt_ring_hdl, uint64_t value)
{
	struct gsi_evt_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n",
				gsi_ctx->evtr[evt_ring_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	ctx->ring.wp_local = value;
	gsi_ring_evt_doorbell(ctx);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_ring_evt_ring_db);

int gsi_ring_ch_ring_db(unsigned long chan_hdl, uint64_t value)
{
	struct gsi_chan_ctx *ctx;
	uint32_t val;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_STARTED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	ctx->ring.wp_local = value;

	/* write MSB first */
	val = ((ctx->ring.wp_local >> 32) &
		GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_BMSK) <<
		GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_DOORBELL_1_OFFS(ctx->props.ch_id,
			gsi_ctx->per.ee));

	gsi_ring_chan_doorbell(ctx);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_ring_ch_ring_db);

int gsi_reset_evt_ring(unsigned long evt_ring_hdl)
{
	uint32_t val;
	enum gsi_evt_ch_cmd_opcode op = GSI_EVT_RESET;
	struct gsi_evt_ctx *ctx;
	int res;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	reinit_completion(&ctx->compl);
	val = (((evt_ring_hdl << GSI_EE_n_EV_CH_CMD_CHID_SHFT) &
			GSI_EE_n_EV_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_EV_CH_CMD_OPCODE_SHFT) &
			 GSI_EE_n_EV_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_CMD_OFFS(gsi_ctx->per.ee));
	res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
	if (res == 0) {
		GSIERR("evt_id=%lu timed out\n", evt_ring_hdl);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_TIMED_OUT;
	}

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("evt_id=%lu unexpected state=%u\n", evt_ring_hdl,
				ctx->state);
		/*
		 * IPA Hardware returned GSI RING not allocated, which is
		 * unexpected. Indicates hardware instability.
		 */
		BUG();
	}

	gsi_program_evt_ring_ctx(&ctx->props, evt_ring_hdl, gsi_ctx->per.ee);
	gsi_init_evt_ring(&ctx->props, &ctx->ring);

	/* restore scratch */
	__gsi_write_evt_ring_scratch(evt_ring_hdl, ctx->scratch);

	if (ctx->props.intf == GSI_EVT_CHTYPE_GPI_EV)
		gsi_prime_evt_ring(ctx);
	if (ctx->props.intf == GSI_EVT_CHTYPE_WDI2_EV)
		gsi_prime_evt_ring_wdi(ctx);
	mutex_unlock(&gsi_ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_reset_evt_ring);

int gsi_get_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr)
{
	struct gsi_evt_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !scr) {
		GSIERR("bad params props=%pK scr=%pK\n", props, scr);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state == GSI_EVT_RING_STATE_NOT_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&ctx->mlock);
	*props = ctx->props;
	*scr = ctx->scratch;
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_get_evt_ring_cfg);

int gsi_set_evt_ring_cfg(unsigned long evt_ring_hdl,
		struct gsi_evt_ring_props *props, union gsi_evt_scratch *scr)
{
	struct gsi_evt_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || gsi_validate_evt_ring_props(props)) {
		GSIERR("bad params props=%pK\n", props);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= gsi_ctx->max_ev) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->props.exclusive != props->exclusive) {
		GSIERR("changing immutable fields not supported\n");
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&ctx->mlock);
	ctx->props = *props;
	if (scr)
		ctx->scratch = *scr;
	mutex_unlock(&ctx->mlock);

	return gsi_reset_evt_ring(evt_ring_hdl);
}
EXPORT_SYMBOL(gsi_set_evt_ring_cfg);

static void gsi_program_chan_ctx_qos(struct gsi_chan_props *props,
	unsigned int ee)
{
	uint32_t val;

	val =
	(((props->low_weight <<
		GSI_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_SHFT) &
		GSI_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_BMSK) |
	((props->max_prefetch <<
		 GSI_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_SHFT) &
		 GSI_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_BMSK) |
	((props->use_db_eng <<
		GSI_EE_n_GSI_CH_k_QOS_USE_DB_ENG_SHFT) &
		 GSI_EE_n_GSI_CH_k_QOS_USE_DB_ENG_BMSK));
	if (gsi_ctx->per.ver >= GSI_VER_2_0)
		val |= ((props->prefetch_mode <<
			GSI_EE_n_GSI_CH_k_QOS_USE_ESCAPE_BUF_ONLY_SHFT)
			& GSI_EE_n_GSI_CH_k_QOS_USE_ESCAPE_BUF_ONLY_BMSK);

	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_QOS_OFFS(props->ch_id, ee));
}

static void gsi_program_chan_ctx_qos_v2_5(struct gsi_chan_props *props,
	unsigned int ee)
{
	uint32_t val;

	val =
	(((props->low_weight <<
		GSI_V2_5_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_SHFT) &
		GSI_V2_5_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_BMSK) |
	((props->max_prefetch <<
		 GSI_V2_5_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_SHFT) &
		 GSI_V2_5_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_BMSK) |
	((props->use_db_eng <<
		GSI_V2_5_EE_n_GSI_CH_k_QOS_USE_DB_ENG_SHFT) &
		GSI_V2_5_EE_n_GSI_CH_k_QOS_USE_DB_ENG_BMSK) |
	((props->prefetch_mode <<
		GSI_V2_5_EE_n_GSI_CH_k_QOS_PREFETCH_MODE_SHFT) &
		GSI_V2_5_EE_n_GSI_CH_k_QOS_PREFETCH_MODE_BMSK) |
	((props->empty_lvl_threshold <<
		GSI_V2_5_EE_n_GSI_CH_k_QOS_EMPTY_LVL_THRSHOLD_SHFT) &
		GSI_V2_5_EE_n_GSI_CH_k_QOS_EMPTY_LVL_THRSHOLD_BMSK));

	gsi_writel(val, gsi_ctx->base +
			GSI_V2_5_EE_n_GSI_CH_k_QOS_OFFS(props->ch_id, ee));
}

static void gsi_program_chan_ctx(struct gsi_chan_props *props, unsigned int ee,
		uint8_t erindex)
{
	uint32_t val;
	uint32_t prot;
	uint32_t prot_msb;

	switch (props->prot) {
	case GSI_CHAN_PROT_MHI:
	case GSI_CHAN_PROT_XHCI:
	case GSI_CHAN_PROT_GPI:
	case GSI_CHAN_PROT_XDCI:
	case GSI_CHAN_PROT_WDI2:
	case GSI_CHAN_PROT_WDI3:
	case GSI_CHAN_PROT_GCI:
	case GSI_CHAN_PROT_MHIP:
		prot_msb = 0;
		break;
	case GSI_CHAN_PROT_AQC:
	case GSI_CHAN_PROT_11AD:
		prot_msb = 1;
		break;
	default:
		GSIERR("Unsupported protocol %d\n", props->prot);
		WARN_ON(1);
		return;
	}
	prot = props->prot;

	val = ((prot <<
		GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_SHFT) &
		GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_BMSK);
	if (gsi_ctx->per.ver >= GSI_VER_2_5) {
		val |= ((prot_msb <<
		GSI_V2_5_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_MSB_SHFT) &
		GSI_V2_5_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_MSB_BMSK);
	}

	val |= (((props->dir << GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_DIR_SHFT) &
			 GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_DIR_BMSK) |
		((erindex << GSI_EE_n_GSI_CH_k_CNTXT_0_ERINDEX_SHFT) &
			 GSI_EE_n_GSI_CH_k_CNTXT_0_ERINDEX_BMSK) |
		((props->re_size << GSI_EE_n_GSI_CH_k_CNTXT_0_ELEMENT_SIZE_SHFT)
			 & GSI_EE_n_GSI_CH_k_CNTXT_0_ELEMENT_SIZE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(props->ch_id, ee));

	val = (props->ring_len & GSI_EE_n_GSI_CH_k_CNTXT_1_R_LENGTH_BMSK) <<
		GSI_EE_n_GSI_CH_k_CNTXT_1_R_LENGTH_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_1_OFFS(props->ch_id, ee));

	val = (props->ring_base_addr &
			GSI_EE_n_GSI_CH_k_CNTXT_2_R_BASE_ADDR_LSBS_BMSK) <<
		GSI_EE_n_GSI_CH_k_CNTXT_2_R_BASE_ADDR_LSBS_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_2_OFFS(props->ch_id, ee));

	val = ((props->ring_base_addr >> 32) &
		GSI_EE_n_GSI_CH_k_CNTXT_3_R_BASE_ADDR_MSBS_BMSK) <<
		GSI_EE_n_GSI_CH_k_CNTXT_3_R_BASE_ADDR_MSBS_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_3_OFFS(props->ch_id, ee));

	if (gsi_ctx->per.ver >= GSI_VER_2_5)
		gsi_program_chan_ctx_qos_v2_5(props, ee);
	else
		gsi_program_chan_ctx_qos(props, ee);
}

static void gsi_init_chan_ring(struct gsi_chan_props *props,
		struct gsi_ring_ctx *ctx)
{
	ctx->base_va = (uintptr_t)props->ring_base_vaddr;
	ctx->base = props->ring_base_addr;
	ctx->wp = ctx->base;
	ctx->rp = ctx->base;
	ctx->wp_local = ctx->base;
	ctx->rp_local = ctx->base;
	ctx->len = props->ring_len;
	ctx->elem_sz = props->re_size;
	ctx->max_num_elem = ctx->len / ctx->elem_sz - 1;
	ctx->end = ctx->base + (ctx->max_num_elem + 1) *
		ctx->elem_sz;
}

static int gsi_validate_channel_props(struct gsi_chan_props *props)
{
	uint64_t ra;
	uint64_t last;

	if (props->ch_id >= gsi_ctx->max_ch) {
		GSIERR("ch_id %u invalid\n", props->ch_id);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if ((props->re_size == GSI_CHAN_RE_SIZE_4B &&
				props->ring_len % 4) ||
			(props->re_size == GSI_CHAN_RE_SIZE_8B &&
				 props->ring_len % 8) ||
			(props->re_size == GSI_CHAN_RE_SIZE_16B &&
				 props->ring_len % 16) ||
			(props->re_size == GSI_CHAN_RE_SIZE_32B &&
				 props->ring_len % 32)) {
		GSIERR("bad params ring_len %u not a multiple of re size %u\n",
				props->ring_len, props->re_size);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ra = props->ring_base_addr;
	do_div(ra, roundup_pow_of_two(props->ring_len));

	if (props->ring_base_addr != ra * roundup_pow_of_two(props->ring_len)) {
		GSIERR("bad params ring base not aligned 0x%llx align 0x%lx\n",
				props->ring_base_addr,
				roundup_pow_of_two(props->ring_len));
		return -GSI_STATUS_INVALID_PARAMS;
	}

	last = props->ring_base_addr + props->ring_len - props->re_size;

	/* MSB should stay same within the ring */
	if ((props->ring_base_addr & 0xFFFFFFFF00000000ULL) !=
	    (last & 0xFFFFFFFF00000000ULL)) {
		GSIERR("MSB is not fixed on ring base 0x%llx size 0x%x\n",
			props->ring_base_addr,
			props->ring_len);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->prot == GSI_CHAN_PROT_GPI &&
			!props->ring_base_vaddr) {
		GSIERR("protocol %u requires ring base VA\n", props->prot);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->low_weight > GSI_MAX_CH_LOW_WEIGHT) {
		GSIERR("invalid channel low weight %u\n", props->low_weight);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->prot == GSI_CHAN_PROT_GPI && !props->xfer_cb) {
		GSIERR("xfer callback must be provided\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!props->err_cb) {
		GSIERR("err callback must be provided\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	return GSI_STATUS_SUCCESS;
}

int gsi_alloc_channel(struct gsi_chan_props *props, unsigned long dev_hdl,
		unsigned long *chan_hdl)
{
	struct gsi_chan_ctx *ctx;
	uint32_t val;
	int res;
	int ee;
	enum gsi_ch_cmd_opcode op = GSI_CH_ALLOCATE;
	uint8_t erindex;
	struct gsi_user_data *user_data;
	size_t user_data_size;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !chan_hdl || dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params props=%pK dev_hdl=0x%lx chan_hdl=%pK\n",
				props, dev_hdl, chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_validate_channel_props(props)) {
		GSIERR("bad params\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->evt_ring_hdl != ~0) {
		if (props->evt_ring_hdl >= gsi_ctx->max_ev) {
			GSIERR("invalid evt ring=%lu\n", props->evt_ring_hdl);
			return -GSI_STATUS_INVALID_PARAMS;
		}

		if (atomic_read(
			&gsi_ctx->evtr[props->evt_ring_hdl].chan_ref_cnt) &&
			gsi_ctx->evtr[props->evt_ring_hdl].props.exclusive &&
			gsi_ctx->evtr[props->evt_ring_hdl].chan->props.prot !=
			GSI_CHAN_PROT_GCI) {
			GSIERR("evt ring=%lu exclusively used by ch_hdl=%pK\n",
				props->evt_ring_hdl, chan_hdl);
			return -GSI_STATUS_UNSUPPORTED_OP;
		}
	}

	ctx = &gsi_ctx->chan[props->ch_id];
	if (ctx->allocated) {
		GSIERR("chan %d already allocated\n", props->ch_id);
		return -GSI_STATUS_NODEV;
	}
	memset(ctx, 0, sizeof(*ctx));

	/* For IPA offloaded WDI channels not required user_data pointer */
	if (props->prot != GSI_CHAN_PROT_WDI2 &&
		props->prot != GSI_CHAN_PROT_WDI3)
		user_data_size = props->ring_len / props->re_size;
	else
		user_data_size = props->re_size;
	/*
	 * GCI channels might have OOO event completions up to GSI_VEID_MAX.
	 * user_data needs to be large enough to accommodate those.
	 * TODO: increase user data size if GSI_VEID_MAX is not enough
	 */
	if (props->prot == GSI_CHAN_PROT_GCI)
		user_data_size += GSI_VEID_MAX;

	user_data = devm_kzalloc(gsi_ctx->dev,
		user_data_size * sizeof(*user_data),
		GFP_KERNEL);
	if (user_data == NULL) {
		GSIERR("context not allocated\n");
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	mutex_init(&ctx->mlock);
	init_completion(&ctx->compl);
	atomic_set(&ctx->poll_mode, GSI_CHAN_MODE_CALLBACK);
	ctx->props = *props;

	if (gsi_ctx->per.ver != GSI_VER_2_2) {
		mutex_lock(&gsi_ctx->mlock);
		ee = gsi_ctx->per.ee;
		gsi_ctx->ch_dbg[props->ch_id].ch_allocate++;
		val = (((props->ch_id << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
					GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
				((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
				 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
		gsi_writel(val, gsi_ctx->base +
				GSI_EE_n_GSI_CH_CMD_OFFS(ee));
		res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
		if (res == 0) {
			GSIERR("chan_hdl=%u timed out\n", props->ch_id);
			mutex_unlock(&gsi_ctx->mlock);
			devm_kfree(gsi_ctx->dev, user_data);
			return -GSI_STATUS_TIMED_OUT;
		}
		if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
			GSIERR("chan_hdl=%u allocation failed state=%d\n",
					props->ch_id, ctx->state);
			mutex_unlock(&gsi_ctx->mlock);
			devm_kfree(gsi_ctx->dev, user_data);
			return -GSI_STATUS_RES_ALLOC_FAILURE;
		}
		mutex_unlock(&gsi_ctx->mlock);
	} else {
		mutex_lock(&gsi_ctx->mlock);
		ctx->state = GSI_CHAN_STATE_ALLOCATED;
		mutex_unlock(&gsi_ctx->mlock);
	}
	erindex = props->evt_ring_hdl != ~0 ? props->evt_ring_hdl :
		GSI_NO_EVT_ERINDEX;
	if (erindex != GSI_NO_EVT_ERINDEX && erindex >= GSI_EVT_RING_MAX) {
		GSIERR("invalid erindex %u\n", erindex);
		devm_kfree(gsi_ctx->dev, user_data);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (erindex < GSI_EVT_RING_MAX) {
		ctx->evtr = &gsi_ctx->evtr[erindex];
		atomic_inc(&ctx->evtr->chan_ref_cnt);
		if (props->prot != GSI_CHAN_PROT_GCI &&
			ctx->evtr->props.exclusive &&
			atomic_read(&ctx->evtr->chan_ref_cnt) == 1)
			ctx->evtr->chan = ctx;
	}

	gsi_program_chan_ctx(props, gsi_ctx->per.ee, erindex);

	spin_lock_init(&ctx->ring.slock);
	gsi_init_chan_ring(props, &ctx->ring);
	if (!props->max_re_expected)
		ctx->props.max_re_expected = ctx->ring.max_num_elem;
	ctx->user_data = user_data;
	*chan_hdl = props->ch_id;
	ctx->allocated = true;
	ctx->stats.dp.last_timestamp = jiffies_to_msecs(jiffies);
	atomic_inc(&gsi_ctx->num_chan);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_alloc_channel);

static int gsi_alloc_ap_channel(unsigned int chan_hdl)
{
	struct gsi_chan_ctx *ctx;
	uint32_t val;
	int res;
	int ee;
	enum gsi_ch_cmd_opcode op = GSI_CH_ALLOCATE;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	ctx = &gsi_ctx->chan[chan_hdl];
	if (ctx->allocated) {
		GSIERR("chan %d already allocated\n", chan_hdl);
		return -GSI_STATUS_NODEV;
	}

	memset(ctx, 0, sizeof(*ctx));

	mutex_init(&ctx->mlock);
	init_completion(&ctx->compl);
	atomic_set(&ctx->poll_mode, GSI_CHAN_MODE_CALLBACK);

	mutex_lock(&gsi_ctx->mlock);
	ee = gsi_ctx->per.ee;
	gsi_ctx->ch_dbg[chan_hdl].ch_allocate++;
	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
				GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
			((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
			 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(ee));
	res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
	if (res == 0) {
		GSIERR("chan_hdl=%u timed out\n", chan_hdl);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_TIMED_OUT;
	}
	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("chan_hdl=%u allocation failed state=%d\n",
				chan_hdl, ctx->state);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}
	mutex_unlock(&gsi_ctx->mlock);

	return GSI_STATUS_SUCCESS;
}

static void __gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val)
{
	gsi_writel(val.data.word1, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	gsi_writel(val.data.word2, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	gsi_writel(val.data.word3, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	gsi_writel(val.data.word4, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
}

int gsi_write_channel_scratch3_reg(unsigned long chan_hdl,
		union __packed gsi_wdi_channel_scratch3_reg val)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	mutex_lock(&ctx->mlock);

	ctx->scratch.wdi.endp_metadatareg_offset =
				val.wdi.endp_metadatareg_offset;
	ctx->scratch.wdi.qmap_id = val.wdi.qmap_id;

	gsi_writel(val.data.word1, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	mutex_unlock(&ctx->mlock);
	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_write_channel_scratch3_reg);

static void __gsi_read_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch * val)
{
	val->data.word1 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	val->data.word2 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	val->data.word3 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	val->data.word4 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
}

static union __packed gsi_channel_scratch __gsi_update_mhi_channel_scratch(
	unsigned long chan_hdl, struct __packed gsi_mhi_channel_scratch mscr)
{
	union __packed gsi_channel_scratch scr;

	/* below sequence is not atomic. assumption is sequencer specific fields
	 * will remain unchanged across this sequence
	 */

	/* READ */
	scr.data.word1 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	scr.data.word2 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	scr.data.word3 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	scr.data.word4 = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	/* UPDATE */
	scr.mhi.mhi_host_wp_addr = mscr.mhi_host_wp_addr;
	scr.mhi.assert_bit40 = mscr.assert_bit40;
	scr.mhi.polling_configuration = mscr.polling_configuration;
	scr.mhi.burst_mode_enabled = mscr.burst_mode_enabled;
	scr.mhi.polling_mode = mscr.polling_mode;
	scr.mhi.oob_mod_threshold = mscr.oob_mod_threshold;

	if (gsi_ctx->per.ver < GSI_VER_2_5) {
		scr.mhi.max_outstanding_tre = mscr.max_outstanding_tre;
		scr.mhi.outstanding_threshold = mscr.outstanding_threshold;
	}

	/* WRITE */
	gsi_writel(scr.data.word1, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	gsi_writel(scr.data.word2, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	gsi_writel(scr.data.word3, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	gsi_writel(scr.data.word4, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));

	return scr;
}

int gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_ALLOCATED &&
		gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_STOPPED) {
		GSIERR("bad state %d\n",
				gsi_ctx->chan[chan_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	mutex_lock(&ctx->mlock);
	ctx->scratch = val;
	__gsi_write_channel_scratch(chan_hdl, val);
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_write_channel_scratch);

int gsi_read_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch *val)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_ALLOCATED &&
		gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_STARTED &&
		gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_STOPPED) {
		GSIERR("bad state %d\n",
				gsi_ctx->chan[chan_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	mutex_lock(&ctx->mlock);
	__gsi_read_channel_scratch(chan_hdl, val);
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_read_channel_scratch);

int gsi_update_mhi_channel_scratch(unsigned long chan_hdl,
		struct __packed gsi_mhi_channel_scratch mscr)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_ALLOCATED &&
		gsi_ctx->chan[chan_hdl].state != GSI_CHAN_STATE_STOPPED) {
		GSIERR("bad state %d\n",
				gsi_ctx->chan[chan_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	mutex_lock(&ctx->mlock);
	ctx->scratch = __gsi_update_mhi_channel_scratch(chan_hdl, mscr);
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_update_mhi_channel_scratch);

int gsi_query_channel_db_addr(unsigned long chan_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!db_addr_wp_msb || !db_addr_wp_lsb) {
		GSIERR("bad params msb=%pK lsb=%pK\n", db_addr_wp_msb,
				db_addr_wp_lsb);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_ctx->chan[chan_hdl].state == GSI_CHAN_STATE_NOT_ALLOCATED) {
		GSIERR("bad state %d\n",
				gsi_ctx->chan[chan_hdl].state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	*db_addr_wp_lsb = gsi_ctx->per.phys_addr +
		GSI_EE_n_GSI_CH_k_DOORBELL_0_OFFS(chan_hdl, gsi_ctx->per.ee);
	*db_addr_wp_msb = gsi_ctx->per.phys_addr +
		GSI_EE_n_GSI_CH_k_DOORBELL_1_OFFS(chan_hdl, gsi_ctx->per.ee);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_query_channel_db_addr);

int gsi_start_channel(unsigned long chan_hdl)
{
	enum gsi_ch_cmd_opcode op = GSI_CH_START;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_ALLOCATED &&
		ctx->state != GSI_CHAN_STATE_STOP_IN_PROC &&
		ctx->state != GSI_CHAN_STATE_STOPPED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	reinit_completion(&ctx->compl);

	/* check if INTSET is in IRQ mode for GPI channel */
	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));
	if (ctx->evtr->props.intf == GSI_EVT_CHTYPE_GPI_EV &&
		val != GSI_INTR_IRQ) {
		GSIERR("GSI_EE_n_CNTXT_INTSET_OFFS %d\n", val);
		BUG();
	}

	gsi_ctx->ch_dbg[chan_hdl].ch_start++;
	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
			GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
		 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));

	GSIDBG("GSI Channel Start, waiting for completion\n");
	gsi_channel_state_change_wait(chan_hdl,
		ctx,
		GSI_START_CMD_TIMEOUT_MS, op);

	if (ctx->state != GSI_CHAN_STATE_STARTED) {
		/*
		* Hardware returned unexpected status, unexpected
		* hardware state.
		*/
		GSIERR("chan=%lu timed out, unexpected state=%u\n",
			chan_hdl, ctx->state);
		BUG();
	}

	GSIDBG("GSI Channel=%lu Start success\n", chan_hdl);

	/* write order MUST be MSB followed by LSB */
	val = ((ctx->ring.wp_local >> 32) &
		GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_BMSK) <<
		GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_DOORBELL_1_OFFS(ctx->props.ch_id,
		gsi_ctx->per.ee));

	mutex_unlock(&gsi_ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_start_channel);

int gsi_stop_channel(unsigned long chan_hdl)
{
	enum gsi_ch_cmd_opcode op = GSI_CH_STOP;
	int res;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state == GSI_CHAN_STATE_STOPPED) {
		GSIDBG("chan_hdl=%lu already stopped\n", chan_hdl);
		return GSI_STATUS_SUCCESS;
	}

	if (ctx->state != GSI_CHAN_STATE_STARTED &&
		ctx->state != GSI_CHAN_STATE_STOP_IN_PROC &&
		ctx->state != GSI_CHAN_STATE_ERROR) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	reinit_completion(&ctx->compl);

	/* check if INTSET is in IRQ mode for GPI channel */
	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));
	if (ctx->evtr->props.intf == GSI_EVT_CHTYPE_GPI_EV &&
		val != GSI_INTR_IRQ) {
		GSIERR("GSI_EE_n_CNTXT_INTSET_OFFS %d\n", val);
		BUG();
	}

	gsi_ctx->ch_dbg[chan_hdl].ch_stop++;
	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
			GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
		 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));

	GSIDBG("GSI Channel Stop, waiting for completion\n");
	gsi_channel_state_change_wait(chan_hdl,
		ctx,
		GSI_STOP_CMD_TIMEOUT_MS, op);

	if (ctx->state != GSI_CHAN_STATE_STOPPED &&
		ctx->state != GSI_CHAN_STATE_STOP_IN_PROC) {
		GSIERR("chan=%lu unexpected state=%u\n", chan_hdl, ctx->state);
		res = -GSI_STATUS_BAD_STATE;
		BUG();
		goto free_lock;
	}

	if (ctx->state == GSI_CHAN_STATE_STOP_IN_PROC) {
		GSIERR("chan=%lu busy try again\n", chan_hdl);
		res = -GSI_STATUS_AGAIN;
		goto free_lock;
	}

	res = GSI_STATUS_SUCCESS;

free_lock:
	mutex_unlock(&gsi_ctx->mlock);
	return res;
}
EXPORT_SYMBOL(gsi_stop_channel);

int gsi_stop_db_channel(unsigned long chan_hdl)
{
	enum gsi_ch_cmd_opcode op = GSI_CH_DB_STOP;
	int res;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state == GSI_CHAN_STATE_STOPPED) {
		GSIDBG("chan_hdl=%lu already stopped\n", chan_hdl);
		return GSI_STATUS_SUCCESS;
	}

	if (ctx->state != GSI_CHAN_STATE_STARTED &&
		ctx->state != GSI_CHAN_STATE_STOP_IN_PROC) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	reinit_completion(&ctx->compl);

	gsi_ctx->ch_dbg[chan_hdl].ch_db_stop++;
	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
			GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
		 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));
	res = wait_for_completion_timeout(&ctx->compl,
			msecs_to_jiffies(GSI_STOP_CMD_TIMEOUT_MS));
	if (res == 0) {
		GSIERR("chan_hdl=%lu timed out\n", chan_hdl);
		res = -GSI_STATUS_TIMED_OUT;
		goto free_lock;
	}

	if (ctx->state != GSI_CHAN_STATE_STOPPED &&
		ctx->state != GSI_CHAN_STATE_STOP_IN_PROC) {
		GSIERR("chan=%lu unexpected state=%u\n", chan_hdl, ctx->state);
		res = -GSI_STATUS_BAD_STATE;
		goto free_lock;
	}

	if (ctx->state == GSI_CHAN_STATE_STOP_IN_PROC) {
		GSIERR("chan=%lu busy try again\n", chan_hdl);
		res = -GSI_STATUS_AGAIN;
		goto free_lock;
	}

	res = GSI_STATUS_SUCCESS;

free_lock:
	mutex_unlock(&gsi_ctx->mlock);
	return res;
}
EXPORT_SYMBOL(gsi_stop_db_channel);

int gsi_reset_channel(unsigned long chan_hdl)
{
	enum gsi_ch_cmd_opcode op = GSI_CH_RESET;
	int res;
	uint32_t val;
	struct gsi_chan_ctx *ctx;
	bool reset_done = false;
	uint32_t retry_cnt = 0;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	/*
	 * In WDI3 case, if SAP enabled but no client connected,
	 * GSI will be in allocated state. When SAP disabled,
	 * gsi_reset_channel will be called and reset is needed.
	 */
	if (ctx->state != GSI_CHAN_STATE_STOPPED &&
		ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);

reset:
	reinit_completion(&ctx->compl);
	gsi_ctx->ch_dbg[chan_hdl].ch_reset++;
	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
			GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
		 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));
	res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
	if (res == 0) {
		GSIERR("chan_hdl=%lu timed out\n", chan_hdl);
		mutex_unlock(&gsi_ctx->mlock);
		return -GSI_STATUS_TIMED_OUT;
	}

revrfy_chnlstate:
	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("chan_hdl=%lu unexpected state=%u\n", chan_hdl,
				ctx->state);
		/* GSI register update state not sync with gsi channel
		 * context state not sync, need to wait for 1ms to sync.
		 */
		retry_cnt++;
		if (retry_cnt <= GSI_CHNL_STATE_MAX_RETRYCNT) {
			usleep_range(GSI_RESET_WA_MIN_SLEEP,
				GSI_RESET_WA_MAX_SLEEP);
			goto revrfy_chnlstate;
		}
		/*
		 * Hardware returned incorrect state, unexpected
		 * hardware state.
		 */
		BUG();
	}

	/* Hardware issue fixed from GSI 2.0 and no need for the WA */
	if (gsi_ctx->per.ver >= GSI_VER_2_0)
		reset_done = true;

	/* workaround: reset GSI producers again */
	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI && !reset_done) {
		usleep_range(GSI_RESET_WA_MIN_SLEEP, GSI_RESET_WA_MAX_SLEEP);
		reset_done = true;
		goto reset;
	}

	if (ctx->props.cleanup_cb)
		gsi_cleanup_xfer_user_data(chan_hdl, ctx->props.cleanup_cb);

	gsi_program_chan_ctx(&ctx->props, gsi_ctx->per.ee,
			ctx->evtr ? ctx->evtr->id : GSI_NO_EVT_ERINDEX);
	gsi_init_chan_ring(&ctx->props, &ctx->ring);

	/* restore scratch */
	__gsi_write_channel_scratch(chan_hdl, ctx->scratch);

	mutex_unlock(&gsi_ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_reset_channel);

int gsi_dealloc_channel(unsigned long chan_hdl)
{
	enum gsi_ch_cmd_opcode op = GSI_CH_DE_ALLOC;
	int res;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	/*In GSI_VER_2_2 version deallocation channel not supported*/
	if (gsi_ctx->per.ver != GSI_VER_2_2) {
		mutex_lock(&gsi_ctx->mlock);
		reinit_completion(&ctx->compl);

		gsi_ctx->ch_dbg[chan_hdl].ch_de_alloc++;
		val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
					GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
				((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
				 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
		gsi_writel(val, gsi_ctx->base +
				GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));
		res = wait_for_completion_timeout(&ctx->compl, GSI_CMD_TIMEOUT);
		if (res == 0) {
			GSIERR("chan_hdl=%lu timed out\n", chan_hdl);
			mutex_unlock(&gsi_ctx->mlock);
			return -GSI_STATUS_TIMED_OUT;
		}
		if (ctx->state != GSI_CHAN_STATE_NOT_ALLOCATED) {
			GSIERR("chan_hdl=%lu unexpected state=%u\n", chan_hdl,
					ctx->state);
			/* Hardware returned incorrect value */
			BUG();
		}

		mutex_unlock(&gsi_ctx->mlock);
	} else {
		mutex_lock(&gsi_ctx->mlock);
		GSIDBG("In GSI_VER_2_2 channel deallocation not supported\n");
		ctx->state = GSI_CHAN_STATE_NOT_ALLOCATED;
		GSIDBG("chan_hdl=%lu Channel state = %u\n", chan_hdl,
								ctx->state);
		mutex_unlock(&gsi_ctx->mlock);
	}
	devm_kfree(gsi_ctx->dev, ctx->user_data);
	ctx->allocated = false;
	if (ctx->evtr)
		atomic_dec(&ctx->evtr->chan_ref_cnt);
	atomic_dec(&gsi_ctx->num_chan);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_dealloc_channel);

void gsi_update_ch_dp_stats(struct gsi_chan_ctx *ctx, uint16_t used)
{
	unsigned long now = jiffies_to_msecs(jiffies);
	unsigned long elapsed;

	if (used == 0) {
		elapsed = now - ctx->stats.dp.last_timestamp;
		if (ctx->stats.dp.empty_time < elapsed)
			ctx->stats.dp.empty_time = elapsed;
	}

	if (used <= ctx->props.max_re_expected / 3)
		++ctx->stats.dp.ch_below_lo;
	else if (used <= 2 * ctx->props.max_re_expected / 3)
		++ctx->stats.dp.ch_below_hi;
	else
		++ctx->stats.dp.ch_above_hi;
	ctx->stats.dp.last_timestamp = now;
}

static void __gsi_query_channel_free_re(struct gsi_chan_ctx *ctx,
		uint16_t *num_free_re)
{
	uint16_t start;
	uint16_t end;
	uint64_t rp;
	int ee = gsi_ctx->per.ee;
	uint16_t used;

	WARN_ON(ctx->props.prot != GSI_CHAN_PROT_GPI);

	if (!ctx->evtr) {
		rp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(ctx->props.ch_id, ee));
		rp |= ctx->ring.rp & 0xFFFFFFFF00000000;

		ctx->ring.rp = rp;
	} else {
		rp = ctx->ring.rp_local;
	}

	start = gsi_find_idx_from_addr(&ctx->ring, rp);
	end = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.wp_local);

	if (end >= start)
		used = end - start;
	else
		used = ctx->ring.max_num_elem + 1 - (start - end);

	*num_free_re = ctx->ring.max_num_elem - used;
}

int gsi_query_channel_info(unsigned long chan_hdl,
		struct gsi_chan_info *info)
{
	struct gsi_chan_ctx *ctx;
	spinlock_t *slock;
	unsigned long flags;
	uint64_t rp;
	uint64_t wp;
	int ee;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch || !info) {
		GSIERR("bad params chan_hdl=%lu info=%pK\n", chan_hdl, info);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];
	if (ctx->evtr) {
		slock = &ctx->evtr->ring.slock;
		info->evt_valid = true;
	} else {
		slock = &ctx->ring.slock;
		info->evt_valid = false;
	}

	spin_lock_irqsave(slock, flags);

	ee = gsi_ctx->per.ee;
	rp = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(ctx->props.ch_id, ee));
	rp |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS(ctx->props.ch_id, ee))) << 32;
	ctx->ring.rp = rp;
	info->rp = rp;

	wp = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS(ctx->props.ch_id, ee));
	wp |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_7_OFFS(ctx->props.ch_id, ee))) << 32;
	ctx->ring.wp = wp;
	info->wp = wp;

	if (info->evt_valid) {
		rp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(ctx->evtr->id, ee));
		rp |= ((uint64_t)gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_5_OFFS(ctx->evtr->id, ee)))
			<< 32;
		info->evt_rp = rp;

		wp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_6_OFFS(ctx->evtr->id, ee));
		wp |= ((uint64_t)gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_7_OFFS(ctx->evtr->id, ee)))
			<< 32;
		info->evt_wp = wp;
	}

	spin_unlock_irqrestore(slock, flags);

	GSIDBG("ch=%lu RP=0x%llx WP=0x%llx ev_valid=%d ERP=0x%llx EWP=0x%llx\n",
			chan_hdl, info->rp, info->wp,
			info->evt_valid, info->evt_rp, info->evt_wp);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_query_channel_info);

int gsi_is_channel_empty(unsigned long chan_hdl, bool *is_empty)
{
	struct gsi_chan_ctx *ctx;
	spinlock_t *slock;
	unsigned long flags;
	uint64_t rp;
	uint64_t wp;
	uint64_t rp_local;
	int ee;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch || !is_empty) {
		GSIERR("bad params chan_hdl=%lu is_empty=%pK\n",
				chan_hdl, is_empty);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];
	ee = gsi_ctx->per.ee;

	if (ctx->props.prot != GSI_CHAN_PROT_GPI &&
		ctx->props.prot != GSI_CHAN_PROT_GCI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->evtr)
		slock = &ctx->evtr->ring.slock;
	else
		slock = &ctx->ring.slock;

	spin_lock_irqsave(slock, flags);

	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI && ctx->evtr) {
		rp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(ctx->evtr->id, ee));
		rp |= ctx->evtr->ring.rp & 0xFFFFFFFF00000000;
		ctx->evtr->ring.rp = rp;

		wp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_6_OFFS(ctx->evtr->id, ee));
		wp |= ctx->evtr->ring.wp & 0xFFFFFFFF00000000;
		ctx->evtr->ring.wp = wp;

		rp_local = ctx->evtr->ring.rp_local;
	} else {
		rp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(ctx->props.ch_id, ee));
		rp |= ctx->ring.rp & 0xFFFFFFFF00000000;
		ctx->ring.rp = rp;

		wp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS(ctx->props.ch_id, ee));
		wp |= ctx->ring.wp & 0xFFFFFFFF00000000;
		ctx->ring.wp = wp;

		rp_local = ctx->ring.rp_local;
	}

	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI)
		*is_empty = (rp_local == rp) ? true : false;
	else
		*is_empty = (wp == rp) ? true : false;

	spin_unlock_irqrestore(slock, flags);

	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI && ctx->evtr)
		GSIDBG("ch=%lu ev=%lu RP=0x%llx WP=0x%llx RP_LOCAL=0x%llx\n",
			chan_hdl, ctx->evtr->id, rp, wp, rp_local);
	else
		GSIDBG("ch=%lu RP=0x%llx WP=0x%llx RP_LOCAL=0x%llx\n",
			chan_hdl, rp, wp, rp_local);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_is_channel_empty);

int __gsi_get_gci_cookie(struct gsi_chan_ctx *ctx, uint16_t idx)
{
	int i;
	int end;

	if (!ctx->user_data[idx].valid) {
		ctx->user_data[idx].valid = true;
		return idx;
	}

	/*
	 * at this point we need to find an "escape buffer" for the cookie
	 * as the userdata in this spot is in use. This happens if the TRE at
	 * idx is not completed yet and it is getting reused by a new TRE.
	 */
	ctx->stats.userdata_in_use++;
	for (i = 0; i < GSI_VEID_MAX; i++) {
		end = ctx->ring.max_num_elem + 1;
		if (!ctx->user_data[end + i].valid) {
			ctx->user_data[end + i].valid = true;
			return end + i;
		}
	}

	/* TODO: Increase escape buffer size if we hit this */
	GSIERR("user_data is full\n");
	return -EPERM;
}

int __gsi_populate_gci_tre(struct gsi_chan_ctx *ctx,
	struct gsi_xfer_elem *xfer)
{
	struct gsi_gci_tre gci_tre;
	struct gsi_gci_tre *tre_gci_ptr;
	uint16_t idx;

	memset(&gci_tre, 0, sizeof(gci_tre));
	if (xfer->addr & 0xFFFFFF0000000000) {
		GSIERR("chan_hdl=%u add too large=%llx\n",
			ctx->props.ch_id, xfer->addr);
		return -EINVAL;
	}

	if (xfer->type != GSI_XFER_ELEM_DATA) {
		GSIERR("chan_hdl=%u bad RE type=%u\n", ctx->props.ch_id,
			xfer->type);
		return -EINVAL;
	}

	idx = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.wp_local);
	tre_gci_ptr = (struct gsi_gci_tre *)(ctx->ring.base_va +
		idx * ctx->ring.elem_sz);

	gci_tre.buffer_ptr = xfer->addr;
	gci_tre.buf_len = xfer->len;
	gci_tre.re_type = GSI_RE_COAL;
	gci_tre.cookie = __gsi_get_gci_cookie(ctx, idx);
	if (gci_tre.cookie < 0)
		return -EPERM;

	/* write the TRE to ring */
	*tre_gci_ptr = gci_tre;
	ctx->user_data[idx].p = xfer->xfer_user_data;

	return 0;
}

int __gsi_populate_tre(struct gsi_chan_ctx *ctx,
	struct gsi_xfer_elem *xfer)
{
	struct gsi_tre tre;
	struct gsi_tre *tre_ptr;
	uint16_t idx;

	memset(&tre, 0, sizeof(tre));
	tre.buffer_ptr = xfer->addr;
	tre.buf_len = xfer->len;
	if (xfer->type == GSI_XFER_ELEM_DATA) {
		tre.re_type = GSI_RE_XFER;
	} else if (xfer->type == GSI_XFER_ELEM_IMME_CMD) {
		tre.re_type = GSI_RE_IMMD_CMD;
	} else if (xfer->type == GSI_XFER_ELEM_NOP) {
		tre.re_type = GSI_RE_NOP;
	} else {
		GSIERR("chan_hdl=%u bad RE type=%u\n", ctx->props.ch_id,
			xfer->type);
		return -EINVAL;
	}

	tre.bei = (xfer->flags & GSI_XFER_FLAG_BEI) ? 1 : 0;
	tre.ieot = (xfer->flags & GSI_XFER_FLAG_EOT) ? 1 : 0;
	tre.ieob = (xfer->flags & GSI_XFER_FLAG_EOB) ? 1 : 0;
	tre.chain = (xfer->flags & GSI_XFER_FLAG_CHAIN) ? 1 : 0;

	idx = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.wp_local);
	tre_ptr = (struct gsi_tre *)(ctx->ring.base_va +
		idx * ctx->ring.elem_sz);

	/* write the TRE to ring */
	*tre_ptr = tre;
	ctx->user_data[idx].valid = true;
	ctx->user_data[idx].p = xfer->xfer_user_data;

	return 0;
}

int gsi_queue_xfer(unsigned long chan_hdl, uint16_t num_xfers,
		struct gsi_xfer_elem *xfer, bool ring_db)
{
	struct gsi_chan_ctx *ctx;
	uint16_t free;
	uint64_t wp_rollback;
	int i;
	spinlock_t *slock;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch || (num_xfers && !xfer)) {
		GSIERR("bad params chan_hdl=%lu num_xfers=%u xfer=%pK\n",
				chan_hdl, num_xfers, xfer);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI &&
			ctx->props.prot != GSI_CHAN_PROT_GCI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->evtr)
		slock = &ctx->evtr->ring.slock;
	else
		slock = &ctx->ring.slock;

	spin_lock_irqsave(slock, flags);

	/* allow only ring doorbell */
	if (!num_xfers)
		goto ring_doorbell;

	/*
	 * for GCI channels the responsibility is on the caller to make sure
	 * there is enough room in the TRE.
	 */
	if (ctx->props.prot != GSI_CHAN_PROT_GCI) {
		__gsi_query_channel_free_re(ctx, &free);
		if (num_xfers > free) {
			GSIERR("chan_hdl=%lu num_xfers=%u free=%u\n",
				chan_hdl, num_xfers, free);
			spin_unlock_irqrestore(slock, flags);
			return -GSI_STATUS_RING_INSUFFICIENT_SPACE;
		}
	}

	wp_rollback = ctx->ring.wp_local;
	for (i = 0; i < num_xfers; i++) {
		if (ctx->props.prot == GSI_CHAN_PROT_GCI) {
			if (__gsi_populate_gci_tre(ctx, &xfer[i]))
				break;
		} else {
			if (__gsi_populate_tre(ctx, &xfer[i]))
				break;
		}
		gsi_incr_ring_wp(&ctx->ring);
	}

	if (i != num_xfers) {
		/* reject all the xfers */
		ctx->ring.wp_local = wp_rollback;
		spin_unlock_irqrestore(slock, flags);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx->stats.queued += num_xfers;

ring_doorbell:
	if (ring_db) {
		/* ensure TRE is set before ringing doorbell */
		wmb();
		gsi_ring_chan_doorbell(ctx);
	}

	spin_unlock_irqrestore(slock, flags);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_queue_xfer);

int gsi_start_xfer(unsigned long chan_hdl)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI &&
		ctx->props.prot != GSI_CHAN_PROT_GCI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->state == GSI_CHAN_STATE_NOT_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->ring.wp == ctx->ring.wp_local)
		return GSI_STATUS_SUCCESS;

	gsi_ring_chan_doorbell(ctx);

	return GSI_STATUS_SUCCESS;
};
EXPORT_SYMBOL(gsi_start_xfer);

int gsi_poll_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify)
{
	int unused_var;

	return gsi_poll_n_channel(chan_hdl, notify, 1, &unused_var);
}
EXPORT_SYMBOL(gsi_poll_channel);

int gsi_poll_n_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify,
		int expected_num, int *actual_num)
{
	struct gsi_chan_ctx *ctx;
	uint64_t rp;
	int ee;
	int i;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch || !notify ||
	    !actual_num || expected_num <= 0) {
		GSIERR("bad params chan_hdl=%lu notify=%pK\n",
			chan_hdl, notify);
		GSIERR("actual_num=%pK expected_num=%d\n",
			actual_num, expected_num);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];
	ee = gsi_ctx->per.ee;

	if (ctx->props.prot != GSI_CHAN_PROT_GPI &&
		ctx->props.prot != GSI_CHAN_PROT_GCI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (!ctx->evtr) {
		GSIERR("no event ring associated chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	spin_lock_irqsave(&ctx->evtr->ring.slock, flags);
	if (ctx->evtr->ring.rp == ctx->evtr->ring.rp_local) {
		/* update rp to see of we have anything new to process */
		gsi_writel(1 << ctx->evtr->id, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(ee));
		rp = gsi_readl(gsi_ctx->base +
			GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(ctx->evtr->id, ee));
		rp |= ctx->ring.rp & 0xFFFFFFFF00000000;

		ctx->evtr->ring.rp = rp;
	}

	if (ctx->evtr->ring.rp == ctx->evtr->ring.rp_local) {
		spin_unlock_irqrestore(&ctx->evtr->ring.slock, flags);
		ctx->stats.poll_empty++;
		return GSI_STATUS_POLL_EMPTY;
	}

	*actual_num = gsi_get_complete_num(&ctx->evtr->ring,
			ctx->evtr->ring.rp_local, ctx->evtr->ring.rp);

	if (*actual_num > expected_num)
		*actual_num = expected_num;

	for (i = 0; i < *actual_num; i++)
		gsi_process_evt_re(ctx->evtr, notify + i, false);

	spin_unlock_irqrestore(&ctx->evtr->ring.slock, flags);
	ctx->stats.poll_ok++;

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_poll_n_channel);

int gsi_config_channel_mode(unsigned long chan_hdl, enum gsi_chan_mode mode)
{
	struct gsi_chan_ctx *ctx;
	enum gsi_chan_mode curr;
	unsigned long flags;
	enum gsi_chan_mode chan_mode;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu mode=%u\n", chan_hdl, mode);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI &&
		ctx->props.prot != GSI_CHAN_PROT_GCI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (!ctx->evtr || !ctx->evtr->props.exclusive) {
		GSIERR("cannot configure mode on chan_hdl=%lu\n",
				chan_hdl);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (atomic_read(&ctx->poll_mode))
		curr = GSI_CHAN_MODE_POLL;
	else
		curr = GSI_CHAN_MODE_CALLBACK;

	if (mode == curr) {
		GSIERR("already in requested mode %u chan_hdl=%lu\n",
				curr, chan_hdl);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}
	spin_lock_irqsave(&gsi_ctx->slock, flags);
	if (curr == GSI_CHAN_MODE_CALLBACK &&
			mode == GSI_CHAN_MODE_POLL) {
		__gsi_config_ieob_irq(gsi_ctx->per.ee, 1 << ctx->evtr->id, 0);
		gsi_writel(1 << ctx->evtr->id, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(gsi_ctx->per.ee));
		atomic_set(&ctx->poll_mode, mode);
		GSIDBG("set gsi_ctx evtr_id %d to %d mode\n",
			ctx->evtr->id, mode);
		ctx->stats.callback_to_poll++;
	}

	if (curr == GSI_CHAN_MODE_POLL &&
			mode == GSI_CHAN_MODE_CALLBACK) {
		atomic_set(&ctx->poll_mode, mode);
		__gsi_config_ieob_irq(gsi_ctx->per.ee, 1 << ctx->evtr->id, ~0);
		GSIDBG("set gsi_ctx evtr_id %d to %d mode\n",
			ctx->evtr->id, mode);

		/*
		 * In GSI 2.2 and 2.5 there is a limitation that can lead
		 * to losing an interrupt. For these versions an
		 * explicit check is needed after enabling the interrupt
		 */
		if (gsi_ctx->per.ver == GSI_VER_2_2 ||
		    gsi_ctx->per.ver == GSI_VER_2_5) {
			u32 src = gsi_readl(gsi_ctx->base +
				GSI_EE_n_CNTXT_SRC_IEOB_IRQ_OFFS(
					gsi_ctx->per.ee));
			if (src & (1 << ctx->evtr->id)) {
				__gsi_config_ieob_irq(
					gsi_ctx->per.ee, 1 << ctx->evtr->id, 0);
				gsi_writel(1 << ctx->evtr->id, gsi_ctx->base +
					GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(
							gsi_ctx->per.ee));
				spin_unlock_irqrestore(&gsi_ctx->slock, flags);
				spin_lock_irqsave(&ctx->evtr->ring.slock,
									flags);
				chan_mode = atomic_xchg(&ctx->poll_mode,
						GSI_CHAN_MODE_POLL);
				spin_unlock_irqrestore(
					&ctx->evtr->ring.slock, flags);
				ctx->stats.poll_pending_irq++;
				GSIDBG("In IEOB WA pnd cnt = %d prvmode = %d\n",
						ctx->stats.poll_pending_irq,
						chan_mode);
				if (chan_mode == GSI_CHAN_MODE_POLL)
					return GSI_STATUS_SUCCESS;
				else
					return -GSI_STATUS_PENDING_IRQ;
			}
		}
		ctx->stats.poll_to_callback++;
	}
	spin_unlock_irqrestore(&gsi_ctx->slock, flags);
	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_config_channel_mode);

int gsi_get_channel_cfg(unsigned long chan_hdl, struct gsi_chan_props *props,
		union gsi_channel_scratch *scr)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !scr) {
		GSIERR("bad params props=%pK scr=%pK\n", props, scr);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state == GSI_CHAN_STATE_NOT_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&ctx->mlock);
	*props = ctx->props;
	*scr = ctx->scratch;
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_get_channel_cfg);

int gsi_set_channel_cfg(unsigned long chan_hdl, struct gsi_chan_props *props,
		union gsi_channel_scratch *scr)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || gsi_validate_channel_props(props)) {
		GSIERR("bad params props=%pK\n", props);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= gsi_ctx->max_ch) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->props.ch_id != props->ch_id ||
		ctx->props.evt_ring_hdl != props->evt_ring_hdl) {
		GSIERR("changing immutable fields not supported\n");
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&ctx->mlock);
	ctx->props = *props;
	if (scr)
		ctx->scratch = *scr;
	gsi_program_chan_ctx(&ctx->props, gsi_ctx->per.ee,
			ctx->evtr ? ctx->evtr->id : GSI_NO_EVT_ERINDEX);
	gsi_init_chan_ring(&ctx->props, &ctx->ring);

	/* restore scratch */
	__gsi_write_channel_scratch(chan_hdl, ctx->scratch);
	mutex_unlock(&ctx->mlock);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_set_channel_cfg);

static void gsi_configure_ieps(void *base, enum gsi_ver ver)
{
	void __iomem *gsi_base = base;

	gsi_writel(1, gsi_base + GSI_GSI_IRAM_PTR_CH_CMD_OFFS);
	gsi_writel(2, gsi_base + GSI_GSI_IRAM_PTR_CH_DB_OFFS);
	gsi_writel(3, gsi_base + GSI_GSI_IRAM_PTR_CH_DIS_COMP_OFFS);
	gsi_writel(4, gsi_base + GSI_GSI_IRAM_PTR_CH_EMPTY_OFFS);
	gsi_writel(5, gsi_base + GSI_GSI_IRAM_PTR_EE_GENERIC_CMD_OFFS);
	gsi_writel(6, gsi_base + GSI_GSI_IRAM_PTR_EVENT_GEN_COMP_OFFS);
	gsi_writel(7, gsi_base + GSI_GSI_IRAM_PTR_INT_MOD_STOPED_OFFS);
	gsi_writel(8, gsi_base + GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_0_OFFS);
	gsi_writel(9, gsi_base + GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_2_OFFS);
	gsi_writel(10, gsi_base + GSI_GSI_IRAM_PTR_PERIPH_IF_TLV_IN_1_OFFS);
	gsi_writel(11, gsi_base + GSI_GSI_IRAM_PTR_NEW_RE_OFFS);
	gsi_writel(12, gsi_base + GSI_GSI_IRAM_PTR_READ_ENG_COMP_OFFS);
	gsi_writel(13, gsi_base + GSI_GSI_IRAM_PTR_TIMER_EXPIRED_OFFS);
	gsi_writel(14, gsi_base + GSI_GSI_IRAM_PTR_EV_DB_OFFS);
	gsi_writel(15, gsi_base + GSI_GSI_IRAM_PTR_UC_GP_INT_OFFS);
	gsi_writel(16, gsi_base + GSI_GSI_IRAM_PTR_WRITE_ENG_COMP_OFFS);

	if (ver >= GSI_VER_2_5)
		gsi_writel(17,
			gsi_base + GSI_V2_5_GSI_IRAM_PTR_TLV_CH_NOT_FULL_OFFS);
}

static void gsi_configure_bck_prs_matrix(void *base)
{
	void __iomem *gsi_base = (void __iomem *) base;

	/*
	 * For now, these are default values. In the future, GSI FW image will
	 * produce optimized back-pressure values based on the FW image.
	 */
	gsi_writel(0xfffffffe,
		gsi_base + GSI_IC_DISABLE_CHNL_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff,
		gsi_base + GSI_IC_DISABLE_CHNL_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffffbf, gsi_base + GSI_IC_GEN_EVNT_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_GEN_EVNT_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffefff, gsi_base + GSI_IC_GEN_INT_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_GEN_INT_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffefff,
		gsi_base + GSI_IC_STOP_INT_MOD_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff,
		gsi_base + GSI_IC_STOP_INT_MOD_BCK_PRS_MSB_OFFS);
	gsi_writel(0x00000000,
		gsi_base + GSI_IC_PROCESS_DESC_BCK_PRS_LSB_OFFS);
	gsi_writel(0x00000000,
		gsi_base + GSI_IC_PROCESS_DESC_BCK_PRS_MSB_OFFS);
	gsi_writel(0xf9ffffff, gsi_base + GSI_IC_TLV_STOP_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_TLV_STOP_BCK_PRS_MSB_OFFS);
	gsi_writel(0xf9ffffff, gsi_base + GSI_IC_TLV_RESET_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_TLV_RESET_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_RGSTR_TIMER_BCK_PRS_LSB_OFFS);
	gsi_writel(0xfffffffe, gsi_base + GSI_IC_RGSTR_TIMER_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_READ_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffefff, gsi_base + GSI_IC_READ_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_WRITE_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffdfff, gsi_base + GSI_IC_WRITE_BCK_PRS_MSB_OFFS);
	gsi_writel(0xffffffff,
		gsi_base + GSI_IC_UCONTROLLER_GPR_BCK_PRS_LSB_OFFS);
	gsi_writel(0xff03ffff,
		gsi_base + GSI_IC_UCONTROLLER_GPR_BCK_PRS_MSB_OFFS);
}

int gsi_configure_regs(phys_addr_t per_base_addr, enum gsi_ver ver)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->base) {
		GSIERR("access to GSI HW has not been mapped\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (ver <= GSI_VER_ERR || ver >= GSI_VER_MAX) {
		GSIERR("Incorrect version %d\n", ver);
		return -GSI_STATUS_ERROR;
	}

	gsi_writel(0, gsi_ctx->base + GSI_GSI_PERIPH_BASE_ADDR_MSB_OFFS);
	gsi_writel(per_base_addr,
			gsi_ctx->base + GSI_GSI_PERIPH_BASE_ADDR_LSB_OFFS);
	gsi_configure_bck_prs_matrix((void *)gsi_ctx->base);
	gsi_configure_ieps(gsi_ctx->base, ver);

	return 0;
}
EXPORT_SYMBOL(gsi_configure_regs);

int gsi_enable_fw(phys_addr_t gsi_base_addr, u32 gsi_size, enum gsi_ver ver)
{
	void __iomem *gsi_base;
	uint32_t value;

	if (ver <= GSI_VER_ERR || ver >= GSI_VER_MAX) {
		GSIERR("Incorrect version %d\n", ver);
		return -GSI_STATUS_ERROR;
	}

	gsi_base = ioremap_nocache(gsi_base_addr, gsi_size);
	if (!gsi_base) {
		GSIERR("ioremap failed\n");
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	/* Enable the MCS and set to x2 clocks */
	if (ver >= GSI_VER_1_2) {
		value = ((1 << GSI_GSI_MCS_CFG_MCS_ENABLE_SHFT) &
				GSI_GSI_MCS_CFG_MCS_ENABLE_BMSK);
		gsi_writel(value, gsi_base + GSI_GSI_MCS_CFG_OFFS);

		value = (((1 << GSI_GSI_CFG_GSI_ENABLE_SHFT) &
				GSI_GSI_CFG_GSI_ENABLE_BMSK) |
			((0 << GSI_GSI_CFG_MCS_ENABLE_SHFT) &
				GSI_GSI_CFG_MCS_ENABLE_BMSK) |
			((1 << GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_SHFT) &
				GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_BMSK) |
			((0 << GSI_GSI_CFG_UC_IS_MCS_SHFT) &
				GSI_GSI_CFG_UC_IS_MCS_BMSK) |
			((0 << GSI_GSI_CFG_GSI_PWR_CLPS_SHFT) &
				GSI_GSI_CFG_GSI_PWR_CLPS_BMSK) |
			((0 << GSI_GSI_CFG_BP_MTRIX_DISABLE_SHFT) &
				GSI_GSI_CFG_BP_MTRIX_DISABLE_BMSK));
	} else {
		value = (((1 << GSI_GSI_CFG_GSI_ENABLE_SHFT) &
				GSI_GSI_CFG_GSI_ENABLE_BMSK) |
			((1 << GSI_GSI_CFG_MCS_ENABLE_SHFT) &
				GSI_GSI_CFG_MCS_ENABLE_BMSK) |
			((1 << GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_SHFT) &
				GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_BMSK) |
			((0 << GSI_GSI_CFG_UC_IS_MCS_SHFT) &
				GSI_GSI_CFG_UC_IS_MCS_BMSK));
	}

	/* GSI frequency is peripheral frequency divided by 3 (2+1) */
	if (ver >= GSI_VER_2_5)
		value |= ((2 << GSI_V2_5_GSI_CFG_SLEEP_CLK_DIV_SHFT) &
			GSI_V2_5_GSI_CFG_SLEEP_CLK_DIV_BMSK);
	gsi_writel(value, gsi_base + GSI_GSI_CFG_OFFS);
	iounmap(gsi_base);

	return 0;

}
EXPORT_SYMBOL(gsi_enable_fw);

void gsi_get_inst_ram_offset_and_size(unsigned long *base_offset,
		unsigned long *size, enum gsi_ver ver)
{
	unsigned long maxn;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return;
	}

	switch (ver) {
	case GSI_VER_1_0:
	case GSI_VER_1_2:
	case GSI_VER_1_3:
		maxn = GSI_GSI_INST_RAM_n_MAXn;
		break;
	case GSI_VER_2_0:
		maxn = GSI_V2_0_GSI_INST_RAM_n_MAXn;
		break;
	case GSI_VER_2_2:
		maxn = GSI_V2_2_GSI_INST_RAM_n_MAXn;
		break;
	case GSI_VER_2_5:
		maxn = GSI_V2_5_GSI_INST_RAM_n_MAXn;
		break;
	case GSI_VER_ERR:
	case GSI_VER_MAX:
	default:
		GSIERR("GSI version is not supported %d\n", ver);
		WARN_ON(1);
		return;
	}
	if (size)
		*size = GSI_GSI_INST_RAM_n_WORD_SZ * (maxn + 1);

	if (base_offset) {
		if (ver < GSI_VER_2_5)
			*base_offset = GSI_GSI_INST_RAM_n_OFFS(0);
		else
			*base_offset = GSI_V2_5_GSI_INST_RAM_n_OFFS(0);
	}
}
EXPORT_SYMBOL(gsi_get_inst_ram_offset_and_size);

int gsi_halt_channel_ee(unsigned int chan_idx, unsigned int ee, int *code)
{
	enum gsi_generic_ee_cmd_opcode op = GSI_GEN_EE_CMD_HALT_CHANNEL;
	uint32_t val;
	int res;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_idx >= gsi_ctx->max_ch || !code) {
		GSIERR("bad params chan_idx=%d\n", chan_idx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	mutex_lock(&gsi_ctx->mlock);
	if (gsi_ctx->per.ver == GSI_VER_2_2)
		__gsi_config_glob_irq(gsi_ctx->per.ee,
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK, ~0);
	reinit_completion(&gsi_ctx->gen_ee_cmd_compl);

	/* invalidate the response */
	gsi_ctx->scratch.word0.val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code = 0;
	gsi_writel(gsi_ctx->scratch.word0.val, gsi_ctx->base +
			GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));

	gsi_ctx->gen_ee_cmd_dbg.halt_channel++;
	val = (((op << GSI_EE_n_GSI_EE_GENERIC_CMD_OPCODE_SHFT) &
		GSI_EE_n_GSI_EE_GENERIC_CMD_OPCODE_BMSK) |
		((chan_idx << GSI_EE_n_GSI_EE_GENERIC_CMD_VIRT_CHAN_IDX_SHFT) &
			GSI_EE_n_GSI_EE_GENERIC_CMD_VIRT_CHAN_IDX_BMSK) |
		((ee << GSI_EE_n_GSI_EE_GENERIC_CMD_EE_SHFT) &
			GSI_EE_n_GSI_EE_GENERIC_CMD_EE_BMSK));
	gsi_writel(val, gsi_ctx->base +
		GSI_EE_n_GSI_EE_GENERIC_CMD_OFFS(gsi_ctx->per.ee));

	res = wait_for_completion_timeout(&gsi_ctx->gen_ee_cmd_compl,
		msecs_to_jiffies(GSI_CMD_TIMEOUT));
	if (res == 0) {
		GSIERR("chan_idx=%u ee=%u timed out\n", chan_idx, ee);
		res = -GSI_STATUS_TIMED_OUT;
		goto free_lock;
	}

	gsi_ctx->scratch.word0.val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	if (gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code ==
		GSI_GEN_EE_CMD_RETURN_CODE_RETRY) {
		GSIDBG("chan_idx=%u ee=%u busy try again\n", chan_idx, ee);
		*code = GSI_GEN_EE_CMD_RETURN_CODE_RETRY;
		res = -GSI_STATUS_AGAIN;
		goto free_lock;
	}
	if (gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code == 0) {
		GSIERR("No response received\n");
		res = -GSI_STATUS_ERROR;
		goto free_lock;
	}

	res = GSI_STATUS_SUCCESS;
	*code = gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code;
free_lock:
	if (gsi_ctx->per.ver == GSI_VER_2_2)
		__gsi_config_glob_irq(gsi_ctx->per.ee,
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK, 0);
	mutex_unlock(&gsi_ctx->mlock);

	return res;
}
EXPORT_SYMBOL(gsi_halt_channel_ee);

int gsi_alloc_channel_ee(unsigned int chan_idx, unsigned int ee, int *code)
{
	enum gsi_generic_ee_cmd_opcode op = GSI_GEN_EE_CMD_ALLOC_CHANNEL;
	struct gsi_chan_ctx *ctx;
	uint32_t val;
	int res;

	if (chan_idx >= gsi_ctx->max_ch || !code) {
		GSIERR("bad params chan_idx=%d\n", chan_idx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (ee == 0)
		return gsi_alloc_ap_channel(chan_idx);

	mutex_lock(&gsi_ctx->mlock);
	if (gsi_ctx->per.ver == GSI_VER_2_2)
		__gsi_config_glob_irq(gsi_ctx->per.ee,
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK, ~0);
	reinit_completion(&gsi_ctx->gen_ee_cmd_compl);

	/* invalidate the response */
	gsi_ctx->scratch.word0.val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code = 0;
	gsi_writel(gsi_ctx->scratch.word0.val, gsi_ctx->base +
			GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));

	val = (((op << GSI_EE_n_GSI_EE_GENERIC_CMD_OPCODE_SHFT) &
		GSI_EE_n_GSI_EE_GENERIC_CMD_OPCODE_BMSK) |
		((chan_idx << GSI_EE_n_GSI_EE_GENERIC_CMD_VIRT_CHAN_IDX_SHFT) &
			GSI_EE_n_GSI_EE_GENERIC_CMD_VIRT_CHAN_IDX_BMSK) |
		((ee << GSI_EE_n_GSI_EE_GENERIC_CMD_EE_SHFT) &
			GSI_EE_n_GSI_EE_GENERIC_CMD_EE_BMSK));
	gsi_writel(val, gsi_ctx->base +
		GSI_EE_n_GSI_EE_GENERIC_CMD_OFFS(gsi_ctx->per.ee));

	res = wait_for_completion_timeout(&gsi_ctx->gen_ee_cmd_compl,
		msecs_to_jiffies(GSI_CMD_TIMEOUT));
	if (res == 0) {
		GSIERR("chan_idx=%u ee=%u timed out\n", chan_idx, ee);
		res = -GSI_STATUS_TIMED_OUT;
		goto free_lock;
	}

	gsi_ctx->scratch.word0.val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SCRATCH_0_OFFS(gsi_ctx->per.ee));
	if (gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code ==
		GSI_GEN_EE_CMD_RETURN_CODE_OUT_OF_RESOURCES) {
		GSIDBG("chan_idx=%u ee=%u out of resources\n", chan_idx, ee);
		*code = GSI_GEN_EE_CMD_RETURN_CODE_OUT_OF_RESOURCES;
		res = -GSI_STATUS_RES_ALLOC_FAILURE;
		goto free_lock;
	}
	if (gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code == 0) {
		GSIERR("No response received\n");
		res = -GSI_STATUS_ERROR;
		goto free_lock;
	}
	if (ee == 0) {
		ctx = &gsi_ctx->chan[chan_idx];
		gsi_ctx->ch_dbg[chan_idx].ch_allocate++;
	}
	res = GSI_STATUS_SUCCESS;
	*code = gsi_ctx->scratch.word0.s.generic_ee_cmd_return_code;
free_lock:
	if (gsi_ctx->per.ver == GSI_VER_2_2)
		__gsi_config_glob_irq(gsi_ctx->per.ee,
			GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK, 0);
	mutex_unlock(&gsi_ctx->mlock);

	return res;
}
EXPORT_SYMBOL(gsi_alloc_channel_ee);


int gsi_chk_intset_value(void)
{
	uint32_t val;

	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));
	return val;
}
EXPORT_SYMBOL(gsi_chk_intset_value);

int gsi_map_virtual_ch_to_per_ep(u32 ee, u32 chan_num, u32 per_ep_index)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!gsi_ctx->base) {
		GSIERR("access to GSI HW has not been mapped\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	gsi_writel(per_ep_index,
		gsi_ctx->base +
		GSI_V2_5_GSI_MAP_EE_n_CH_k_VP_TABLE_OFFS(chan_num, ee));

	return 0;
}
EXPORT_SYMBOL(gsi_map_virtual_ch_to_per_ep);

void gsi_wdi3_write_evt_ring_db(unsigned long evt_ring_hdl,
	uint32_t db_addr_low, uint32_t db_addr_high)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return;
	}

	gsi_writel(db_addr_low, gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_12_OFFS(evt_ring_hdl, gsi_ctx->per.ee));

	gsi_writel(db_addr_high, gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_13_OFFS(evt_ring_hdl, gsi_ctx->per.ee));
}
EXPORT_SYMBOL(gsi_wdi3_write_evt_ring_db);

void gsi_wdi3_dump_register(unsigned long chan_hdl)
{
	uint32_t val;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return;
	}
	GSIDBG("reg dump ch id %d\n", chan_hdl);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_1_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_2_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_3_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_7_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_CNTXT_7_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_RE_FETCH_READ_PTR_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_RE_FETCH_READ_PTR_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_RE_FETCH_WRITE_PTR_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_RE_FETCH_WRITE_PTR_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_QOS_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_QOS_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS 0x%x\n", val);
	val = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	GSIDBG("GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS 0x%x\n", val);
}
EXPORT_SYMBOL(gsi_wdi3_dump_register);

static int msm_gsi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_debug("gsi_probe\n");
	gsi_ctx = devm_kzalloc(dev, sizeof(*gsi_ctx), GFP_KERNEL);
	if (!gsi_ctx) {
		dev_err(dev, "failed to allocated gsi context\n");
		return -ENOMEM;
	}

	gsi_ctx->ipc_logbuf = ipc_log_context_create(GSI_IPC_LOG_PAGES,
		"gsi", 0);
	if (gsi_ctx->ipc_logbuf == NULL)
		GSIERR("failed to create IPC log, continue...\n");

	gsi_ctx->dev = dev;
	init_completion(&gsi_ctx->gen_ee_cmd_compl);
	gsi_debugfs_init();

	return 0;
}

static struct platform_driver msm_gsi_driver = {
	.probe          = msm_gsi_probe,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "gsi",
		.of_match_table = msm_gsi_match,
	},
};

static struct platform_device *pdev;

/**
 * Module Init.
 */
static int __init gsi_init(void)
{
	int ret;

	pr_debug("gsi_init\n");

	ret = platform_driver_register(&msm_gsi_driver);
	if (ret < 0)
		goto out;

	if (running_emulation) {
		pdev = platform_device_register_simple("gsi", -1, NULL, 0);
		if (IS_ERR(pdev)) {
			ret = PTR_ERR(pdev);
			platform_driver_unregister(&msm_gsi_driver);
			goto out;
		}
	}

out:
	return ret;
}
arch_initcall(gsi_init);

/*
 * Module exit.
 */
static void __exit gsi_exit(void)
{
	if (running_emulation && pdev)
		platform_device_unregister(pdev);
	platform_driver_unregister(&msm_gsi_driver);
}
module_exit(gsi_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Generic Software Interface (GSI)");
