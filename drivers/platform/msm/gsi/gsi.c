/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define GSI_CMD_TIMEOUT (5*HZ)
#define GSI_STOP_CMD_TIMEOUT_MS 10
#define GSI_MAX_CH_LOW_WEIGHT 15
#define GSI_MHI_ER_START 10
#define GSI_MHI_ER_END 16

#define GSI_RESET_WA_MIN_SLEEP 1000
#define GSI_RESET_WA_MAX_SLEEP 2000
static const struct of_device_id msm_gsi_match[] = {
	{ .compatible = "qcom,msm_gsi", },
	{ },
};

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

static void gsi_handle_ch_ctrl(int ee)
{
	uint32_t ch;
	int i;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	ch = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_OFFS(ee));
	GSIDBG("ch %x\n", ch);
	for (i = 0; i < 32; i++) {
		if ((1 << i) & ch) {
			ctx = &gsi_ctx->chan[i];
			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(i, ee));
			ctx->state = (val &
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_SHFT;
			GSIDBG("ch %u state updated to %u\n", i, ctx->state);
			complete(&ctx->compl);
		}
	}

	gsi_writel(ch, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_GSI_CH_IRQ_CLR_OFFS(ee));
}

static void gsi_handle_ev_ctrl(int ee)
{
	uint32_t ch;
	int i;
	uint32_t val;
	struct gsi_evt_ctx *ctx;

	ch = gsi_readl(gsi_ctx->base +
		GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_OFFS(ee));
	GSIDBG("ev %x\n", ch);
	for (i = 0; i < 32; i++) {
		if ((1 << i) & ch) {
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

	gsi_writel(ch, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_EV_CH_IRQ_CLR_OFFS(ee));
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

	log = (struct gsi_log_err *)&err;
	GSIERR("log err_type=%u ee=%u idx=%u\n", log->err_type, log->ee,
			log->virt_idx);
	GSIERR("code=%u arg1=%u arg2=%u arg3=%u\n", log->code, log->arg1,
			log->arg2, log->arg3);
	switch (log->err_type) {
	case GSI_ERR_TYPE_GLOB:
		per_notify.evt_id = GSI_PER_EVT_GLOB_ERROR;
		per_notify.user_data = gsi_ctx->per.user_data;
		per_notify.data.err_desc = err & 0xFFFF;
		gsi_ctx->per.notify_cb(&per_notify);
		break;
	case GSI_ERR_TYPE_CHAN:
		BUG_ON(log->virt_idx >= GSI_MAX_CHAN);
		ch = &gsi_ctx->chan[log->virt_idx];
		chan_notify.chan_user_data = ch->props.chan_user_data;
		chan_notify.err_desc = err & 0xFFFF;
		if (log->code == GSI_INVALID_TRE_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			val = gsi_readl(gsi_ctx->base +
				GSI_EE_n_GSI_CH_k_CNTXT_0_OFFS(log->virt_idx,
					gsi_ctx->per.ee));
			ch->state = (val &
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_BMSK) >>
				GSI_EE_n_GSI_CH_k_CNTXT_0_CHSTATE_SHFT;
			GSIDBG("ch %u state updated to %u\n", log->virt_idx,
					ch->state);
			ch->stats.invalid_tre_error++;
			BUG_ON(ch->state != GSI_CHAN_STATE_ERROR);
			chan_notify.evt_id = GSI_CHAN_INVALID_TRE_ERR;
		} else if (log->code == GSI_OUT_OF_BUFFERS_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			chan_notify.evt_id = GSI_CHAN_OUT_OF_BUFFERS_ERR;
		} else if (log->code == GSI_OUT_OF_RESOURCES_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			chan_notify.evt_id = GSI_CHAN_OUT_OF_RESOURCES_ERR;
			complete(&ch->compl);
		} else if (log->code == GSI_UNSUPPORTED_INTER_EE_OP_ERR) {
			chan_notify.evt_id =
				GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR;
		} else if (log->code == GSI_NON_ALLOCATED_EVT_ACCESS_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			chan_notify.evt_id =
				GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR;
		} else if (log->code == GSI_HWO_1_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			chan_notify.evt_id = GSI_CHAN_HWO_1_ERR;
		} else {
			BUG();
		}
		if (ch->props.err_cb)
			ch->props.err_cb(&chan_notify);
		else
			WARN_ON(1);
		break;
	case GSI_ERR_TYPE_EVT:
		BUG_ON(log->virt_idx >= GSI_MAX_EVT_RING);
		ev = &gsi_ctx->evtr[log->virt_idx];
		evt_notify.user_data = ev->props.user_data;
		evt_notify.err_desc = err & 0xFFFF;
		if (log->code == GSI_OUT_OF_BUFFERS_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			evt_notify.evt_id = GSI_EVT_OUT_OF_BUFFERS_ERR;
		} else if (log->code == GSI_OUT_OF_RESOURCES_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			evt_notify.evt_id = GSI_EVT_OUT_OF_RESOURCES_ERR;
			complete(&ev->compl);
		} else if (log->code == GSI_UNSUPPORTED_INTER_EE_OP_ERR) {
			evt_notify.evt_id = GSI_EVT_UNSUPPORTED_INTER_EE_OP_ERR;
		} else if (log->code == GSI_EVT_RING_EMPTY_ERR) {
			BUG_ON(log->ee != gsi_ctx->per.ee);
			evt_notify.evt_id = GSI_EVT_EVT_RING_EMPTY_ERR;
		} else {
			BUG();
		}
		if (ev->props.err_cb)
			ev->props.err_cb(&evt_notify);
		else
			WARN_ON(1);
		break;
	default:
		WARN_ON(1);
	}
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
		gsi_writel(clr, gsi_ctx->base +
			GSI_EE_n_ERROR_LOG_CLR_OFFS(ee));
		gsi_handle_glob_err(err);
	}

	if (val & GSI_EE_n_CNTXT_GLOB_IRQ_EN_GP_INT1_BMSK) {
		notify.evt_id = GSI_PER_EVT_GLOB_GP1;
		gsi_ctx->per.notify_cb(&notify);
	}

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
	BUG_ON(addr < ctx->base || addr >= ctx->end);

	return (uint32_t)(addr - ctx->base)/ctx->elem_sz;
}

static void gsi_process_chan(struct gsi_xfer_compl_evt *evt,
		struct gsi_chan_xfer_notify *notify, bool callback)
{
	uint32_t ch_id;
	struct gsi_chan_ctx *ch_ctx;
	uint16_t rp_idx;
	uint64_t rp;

	ch_id = evt->chid;
	BUG_ON(ch_id >= GSI_MAX_CHAN);
	ch_ctx = &gsi_ctx->chan[ch_id];
	BUG_ON(ch_ctx->props.prot != GSI_CHAN_PROT_GPI);
	rp = evt->xfer_ptr;

	while (ch_ctx->ring.rp_local != rp) {
		gsi_incr_ring_rp(&ch_ctx->ring);
		ch_ctx->stats.completed++;
	}

	/* the element at RP is also processed */
	gsi_incr_ring_rp(&ch_ctx->ring);
	ch_ctx->stats.completed++;

	ch_ctx->ring.rp = ch_ctx->ring.rp_local;

	rp_idx = gsi_find_idx_from_addr(&ch_ctx->ring, rp);
	notify->xfer_user_data = ch_ctx->user_data[rp_idx];
	notify->chan_user_data = ch_ctx->props.chan_user_data;
	notify->evt_id = evt->code;
	notify->bytes_xfered = evt->len;
	if (callback)
		ch_ctx->props.xfer_cb(notify);
}

static void gsi_process_evt_re(struct gsi_evt_ctx *ctx,
		struct gsi_chan_xfer_notify *notify, bool callback)
{
	struct gsi_xfer_compl_evt *evt;
	uint16_t idx;

	idx = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.rp_local);
	evt = (struct gsi_xfer_compl_evt *)(ctx->ring.base_va +
			idx * ctx->ring.elem_sz);
	gsi_process_chan(evt, notify, callback);
	gsi_incr_ring_rp(&ctx->ring);
	/* recycle this element */
	gsi_incr_ring_wp(&ctx->ring);
	ctx->stats.completed++;
}

static void gsi_ring_evt_doorbell(struct gsi_evt_ctx *ctx)
{
	uint32_t val;

	/* write order MUST be MSB followed by LSB */
	val = ((ctx->ring.wp_local >> 32) &
			GSI_EE_n_EV_CH_k_DOORBELL_1_WRITE_PTR_MSB_BMSK) <<
			GSI_EE_n_EV_CH_k_DOORBELL_1_WRITE_PTR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_EV_CH_k_DOORBELL_1_OFFS(ctx->id,
				gsi_ctx->per.ee));

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

	/* write order MUST be MSB followed by LSB */
	val = ((ctx->ring.wp_local >> 32) &
			GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_BMSK) <<
			GSI_EE_n_GSI_CH_k_DOORBELL_1_WRITE_PTR_MSB_SHFT;
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_DOORBELL_1_OFFS(ctx->props.ch_id,
				gsi_ctx->per.ee));

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

	for (i = 0; i < 32; i++) {
		if ((1 << i) & ch & msk) {
			ctx = &gsi_ctx->evtr[i];
			BUG_ON(ctx->props.intf != GSI_EVT_CHTYPE_GPI_EV);
			spin_lock_irqsave(&ctx->ring.slock, flags);
check_again:
			cntr = 0;
			rp = gsi_readl(gsi_ctx->base +
				GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(i, ee));
			rp |= ((uint64_t)gsi_readl(gsi_ctx->base +
				GSI_EE_n_EV_CH_k_CNTXT_5_OFFS(i, ee))) << 32;
			ctx->ring.rp = rp;
			while (ctx->ring.rp_local != rp) {
				++cntr;
				gsi_process_evt_re(ctx, &notify, true);
				if (ctx->props.exclusive &&
					atomic_read(&ctx->chan->poll_mode)) {
					cntr = 0;
					break;
				}
			}
			gsi_ring_evt_doorbell(ctx);
			if (cntr != 0)
				goto check_again;
			spin_unlock_irqrestore(&ctx->ring.slock, flags);
		}
	}

	gsi_writel(ch & msk, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(ee));
}

static void gsi_handle_inter_ee_ch_ctrl(int ee)
{
	uint32_t ch;
	int i;

	ch = gsi_readl(gsi_ctx->base +
		GSI_INTER_EE_n_SRC_GSI_CH_IRQ_OFFS(ee));
	for (i = 0; i < 32; i++) {
		if ((1 << i) & ch) {
			/* not currently expected */
			GSIERR("ch %u was inter-EE changed\n", i);
		}
	}

	gsi_writel(ch, gsi_ctx->base +
			GSI_INTER_EE_n_SRC_GSI_CH_IRQ_CLR_OFFS(ee));
}

static void gsi_handle_inter_ee_ev_ctrl(int ee)
{
	uint32_t ch;
	int i;

	ch = gsi_readl(gsi_ctx->base +
		GSI_INTER_EE_n_SRC_EV_CH_IRQ_OFFS(ee));
	for (i = 0; i < 32; i++) {
		if ((1 << i) & ch) {
			/* not currently expected */
			GSIERR("evt %u was inter-EE changed\n", i);
		}
	}

	gsi_writel(ch, gsi_ctx->base +
			GSI_INTER_EE_n_SRC_EV_CH_IRQ_CLR_OFFS(ee));
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

		GSIDBG("type %x\n", type);

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

		if (++cnt > GSI_ISR_MAX_ITER)
			BUG();
	}
}

static irqreturn_t gsi_isr(int irq, void *ctxt)
{
	BUG_ON(ctxt != gsi_ctx);

	if (gsi_ctx->per.req_clk_cb) {
		bool granted = false;

		gsi_ctx->per.req_clk_cb(gsi_ctx->per.user_data, &granted);
		if (granted) {
			gsi_handle_irq();
			gsi_ctx->per.rel_clk_cb(gsi_ctx->per.user_data);
		}
	} else {
		gsi_handle_irq();
	}

	return IRQ_HANDLED;
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
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%p\n", dev_hdl,
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

int gsi_register_device(struct gsi_per_props *props, unsigned long *dev_hdl)
{
	int res;
	uint32_t val;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !dev_hdl) {
		GSIERR("bad params props=%p dev_hdl=%p\n", props, dev_hdl);
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

	spin_lock_init(&gsi_ctx->slock);
	if (props->intr == GSI_INTR_IRQ) {
		if (!props->irq) {
			GSIERR("bad irq specified %u\n", props->irq);
			return -GSI_STATUS_INVALID_PARAMS;
		}

		res = devm_request_irq(gsi_ctx->dev, props->irq,
				(irq_handler_t) gsi_isr,
				props->req_clk_cb ? IRQF_TRIGGER_RISING :
					IRQF_TRIGGER_HIGH,
				"gsi",
				gsi_ctx);
		if (res) {
			GSIERR("failed to register isr for %u\n", props->irq);
			return -GSI_STATUS_ERROR;
		}

		res = enable_irq_wake(props->irq);
		if (res)
			GSIERR("failed to enable wake irq %u\n", props->irq);
		else
			GSIERR("GSI irq is wake enabled %u\n", props->irq);

	} else {
		GSIERR("do not support interrupt type %u\n", props->intr);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	gsi_ctx->base = devm_ioremap_nocache(gsi_ctx->dev, props->phys_addr,
				props->size);
	if (!gsi_ctx->base) {
		GSIERR("failed to remap GSI HW\n");
		devm_free_irq(gsi_ctx->dev, props->irq, gsi_ctx);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	gsi_ctx->per = *props;
	gsi_ctx->per_registered = true;
	mutex_init(&gsi_ctx->mlock);
	atomic_set(&gsi_ctx->num_chan, 0);
	atomic_set(&gsi_ctx->num_evt_ring, 0);
	/* only support 16 un-reserved + 7 reserved event virtual IDs */
	gsi_ctx->evt_bmap = ~0x7E03FF;

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

	gsi_writel(props->intr, gsi_ctx->base +
			GSI_EE_n_CNTXT_INTSET_OFFS(gsi_ctx->per.ee));

	val = gsi_readl(gsi_ctx->base +
			GSI_EE_n_GSI_STATUS_OFFS(gsi_ctx->per.ee));
	if (val & GSI_EE_n_GSI_STATUS_ENABLED_BMSK)
		gsi_ctx->enabled = true;
	else
		GSIERR("Manager EE has not enabled GSI, GSI un-usable\n");

	*dev_hdl = (uintptr_t)gsi_ctx;

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_register_device);

int gsi_write_device_scratch(unsigned long dev_hdl,
		struct gsi_device_scratch *val)
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
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%p\n", dev_hdl,
				gsi_ctx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (val->max_usb_pkt_size_valid &&
			val->max_usb_pkt_size != 1024 &&
			val->max_usb_pkt_size != 512) {
		GSIERR("bad USB max pkt size dev_hdl=0x%lx sz=%u\n", dev_hdl,
				val->max_usb_pkt_size);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	mutex_lock(&gsi_ctx->mlock);
	if (val->mhi_base_chan_idx_valid)
		gsi_ctx->scratch.word0.s.mhi_base_chan_idx =
			val->mhi_base_chan_idx;
	if (val->max_usb_pkt_size_valid)
		gsi_ctx->scratch.word0.s.max_usb_pkt_size =
			(val->max_usb_pkt_size == 1024) ? 1 : 0;
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
		GSIERR("bad params dev_hdl=0x%lx gsi_ctx=0x%p\n", dev_hdl,
				gsi_ctx);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (!force && atomic_read(&gsi_ctx->num_chan)) {
		GSIERR("%u channels are allocated\n",
				atomic_read(&gsi_ctx->num_chan));
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (!force && atomic_read(&gsi_ctx->num_evt_ring)) {
		GSIERR("%u evt rings are allocated\n",
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
	devm_iounmap(gsi_ctx->dev, gsi_ctx->base);
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

	spin_lock_irqsave(&ctx->ring.slock, flags);
	memset((void *)ctx->ring.base_va, 0, ctx->ring.len);
	ctx->ring.wp_local = ctx->ring.base +
		ctx->ring.max_num_elem * ctx->ring.elem_sz;
	gsi_ring_evt_doorbell(ctx);
	spin_unlock_irqrestore(&ctx->ring.slock, flags);
}

static int gsi_validate_evt_ring_props(struct gsi_evt_ring_props *props)
{
	uint64_t ra;

	if ((props->re_size == GSI_EVT_RING_RE_SIZE_4B &&
				props->ring_len % 4) ||
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
			props->evchid > GSI_MHI_ER_END ||
			props->evchid < GSI_MHI_ER_START)) {
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

int gsi_alloc_evt_ring(struct gsi_evt_ring_props *props, unsigned long dev_hdl,
		unsigned long *evt_ring_hdl)
{
	unsigned long evt_id;
	enum gsi_evt_ch_cmd_opcode op = GSI_EVT_ALLOCATE;
	uint32_t val;
	struct gsi_evt_ctx *ctx;
	int res;
	int ee = gsi_ctx->per.ee;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !evt_ring_hdl || dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params props=%p dev_hdl=0x%lx evt_ring_hdl=%p\n",
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
	mutex_unlock(&gsi_ctx->mlock);

	spin_lock_irqsave(&gsi_ctx->slock, flags);
	gsi_writel(1 << evt_id, gsi_ctx->base +
			GSI_EE_n_CNTXT_SRC_IEOB_IRQ_CLR_OFFS(ee));
	if (props->intf != GSI_EVT_CHTYPE_GPI_EV)
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

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
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

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (atomic_read(&ctx->chan_ref_cnt)) {
		GSIERR("%d channels still using this event ring\n",
			atomic_read(&ctx->chan_ref_cnt));
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	/* TODO: add check for ERROR state */
	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
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
		GSIERR("bad params msb=%p lsb=%p\n", db_addr_wp_msb,
				db_addr_wp_lsb);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
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

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
		GSIERR("bad params evt_ring_hdl=%lu\n", evt_ring_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->evtr[evt_ring_hdl];

	if (ctx->state != GSI_EVT_RING_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
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
		BUG();
	}

	gsi_program_evt_ring_ctx(&ctx->props, evt_ring_hdl, gsi_ctx->per.ee);
	gsi_init_evt_ring(&ctx->props, &ctx->ring);

	/* restore scratch */
	__gsi_write_evt_ring_scratch(evt_ring_hdl, ctx->scratch);

	if (ctx->props.intf == GSI_EVT_CHTYPE_GPI_EV)
		gsi_prime_evt_ring(ctx);
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
		GSIERR("bad params props=%p scr=%p\n", props, scr);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
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
		GSIERR("bad params props=%p\n", props);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (evt_ring_hdl >= GSI_MAX_EVT_RING) {
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

static void gsi_program_chan_ctx(struct gsi_chan_props *props, unsigned int ee,
		uint8_t erindex)
{
	uint32_t val;

	val = (((props->prot << GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_SHFT)
			& GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_PROTOCOL_BMSK) |
		((props->dir << GSI_EE_n_GSI_CH_k_CNTXT_0_CHTYPE_DIR_SHFT) &
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

	val = (((props->low_weight << GSI_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_SHFT) &
				GSI_EE_n_GSI_CH_k_QOS_WRR_WEIGHT_BMSK) |
		((props->max_prefetch <<
			 GSI_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_SHFT) &
			 GSI_EE_n_GSI_CH_k_QOS_MAX_PREFETCH_BMSK) |
		((props->use_db_eng << GSI_EE_n_GSI_CH_k_QOS_USE_DB_ENG_SHFT) &
			 GSI_EE_n_GSI_CH_k_QOS_USE_DB_ENG_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_k_QOS_OFFS(props->ch_id, ee));
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

	if (props->ch_id >= GSI_MAX_CHAN) {
		GSIERR("ch_id %u invalid\n", props->ch_id);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if ((props->re_size == GSI_CHAN_RE_SIZE_4B &&
				props->ring_len % 4) ||
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
	int ee = gsi_ctx->per.ee;
	enum gsi_ch_cmd_opcode op = GSI_CH_ALLOCATE;
	uint8_t erindex;
	void **user_data;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!props || !chan_hdl || dev_hdl != (uintptr_t)gsi_ctx) {
		GSIERR("bad params props=%p dev_hdl=0x%lx chan_hdl=%p\n",
				props, dev_hdl, chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (gsi_validate_channel_props(props)) {
		GSIERR("bad params\n");
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (props->evt_ring_hdl != ~0 &&
		atomic_read(&gsi_ctx->evtr[props->evt_ring_hdl].chan_ref_cnt) &&
		gsi_ctx->evtr[props->evt_ring_hdl].props.exclusive) {
		GSIERR("evt ring=%lu already in exclusive use chan_hdl=%p\n",
				props->evt_ring_hdl, chan_hdl);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}


	ctx = &gsi_ctx->chan[props->ch_id];
	if (ctx->allocated) {
		GSIERR("chan %d already allocated\n", props->ch_id);
		return -GSI_STATUS_NODEV;
	}

	memset(ctx, 0, sizeof(*ctx));
	user_data = devm_kzalloc(gsi_ctx->dev,
		(props->ring_len / props->re_size) * sizeof(void *),
		GFP_KERNEL);
	if (user_data == NULL) {
		GSIERR("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	mutex_init(&ctx->mlock);
	init_completion(&ctx->compl);
	atomic_set(&ctx->poll_mode, GSI_CHAN_MODE_CALLBACK);
	ctx->props = *props;

	mutex_lock(&gsi_ctx->mlock);
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

	erindex = props->evt_ring_hdl != ~0 ? props->evt_ring_hdl :
		GSI_NO_EVT_ERINDEX;
	if (erindex != GSI_NO_EVT_ERINDEX) {
		ctx->evtr = &gsi_ctx->evtr[erindex];
		atomic_inc(&ctx->evtr->chan_ref_cnt);
		if (ctx->evtr->props.exclusive)
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

static void __gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val)
{
	uint32_t reg;

	gsi_writel(val.data.word1, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_0_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	gsi_writel(val.data.word2, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_1_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	gsi_writel(val.data.word3, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_2_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	/* below sequence is not atomic. assumption is sequencer specific fields
	 * will remain unchanged across this sequence
	 */
	reg = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
	reg &= 0xFFFF;
	reg |= (val.data.word4 & 0xFFFF0000);
	gsi_writel(reg, gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_SCRATCH_3_OFFS(chan_hdl,
			gsi_ctx->per.ee));
}

int gsi_write_channel_scratch(unsigned long chan_hdl,
		union __packed gsi_channel_scratch val)
{
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
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

int gsi_query_channel_db_addr(unsigned long chan_hdl,
		uint32_t *db_addr_wp_lsb, uint32_t *db_addr_wp_msb)
{
	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (!db_addr_wp_msb || !db_addr_wp_lsb) {
		GSIERR("bad params msb=%p lsb=%p\n", db_addr_wp_msb,
				db_addr_wp_lsb);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
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
	int res;
	uint32_t val;
	struct gsi_chan_ctx *ctx;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
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
	init_completion(&ctx->compl);

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
	if (ctx->state != GSI_CHAN_STATE_STARTED) {
		GSIERR("chan=%lu unexpected state=%u\n", chan_hdl, ctx->state);
		BUG();
	}

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

	if (chan_hdl >= GSI_MAX_CHAN) {
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
	init_completion(&ctx->compl);

	val = (((chan_hdl << GSI_EE_n_GSI_CH_CMD_CHID_SHFT) &
			GSI_EE_n_GSI_CH_CMD_CHID_BMSK) |
		((op << GSI_EE_n_GSI_CH_CMD_OPCODE_SHFT) &
		 GSI_EE_n_GSI_CH_CMD_OPCODE_BMSK));
	gsi_writel(val, gsi_ctx->base +
			GSI_EE_n_GSI_CH_CMD_OFFS(gsi_ctx->per.ee));
	res = wait_for_completion_timeout(&ctx->compl,
			msecs_to_jiffies(GSI_STOP_CMD_TIMEOUT_MS));
	if (res == 0) {
		GSIDBG("chan_hdl=%lu timed out\n", chan_hdl);
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

	if (chan_hdl >= GSI_MAX_CHAN) {
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
	init_completion(&ctx->compl);

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

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_STOPPED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);

reset:
	init_completion(&ctx->compl);
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

	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("chan_hdl=%lu unexpected state=%u\n", chan_hdl,
				ctx->state);
		BUG();
	}

	/* workaround: reset GSI producers again */
	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI && !reset_done) {
		usleep_range(GSI_RESET_WA_MIN_SLEEP, GSI_RESET_WA_MAX_SLEEP);
		reset_done = true;
		goto reset;
	}

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

	if (chan_hdl >= GSI_MAX_CHAN) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->state != GSI_CHAN_STATE_ALLOCATED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	mutex_lock(&gsi_ctx->mlock);
	init_completion(&ctx->compl);

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
		BUG();
	}

	mutex_unlock(&gsi_ctx->mlock);

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
	uint16_t start_hw;
	uint16_t end;
	uint64_t rp;
	uint64_t rp_hw;
	int ee = gsi_ctx->per.ee;
	uint16_t used;
	uint16_t used_hw;

	rp_hw = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(ctx->props.ch_id, ee));
	rp_hw |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS(ctx->props.ch_id, ee)))
		<< 32;

	if (!ctx->evtr) {
		rp = rp_hw;
		ctx->ring.rp = rp;
	} else {
		rp = ctx->ring.rp_local;
	}

	start = gsi_find_idx_from_addr(&ctx->ring, rp);
	start_hw = gsi_find_idx_from_addr(&ctx->ring, rp_hw);
	end = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.wp_local);

	if (end >= start)
		used = end - start;
	else
		used = ctx->ring.max_num_elem + 1 - (start - end);

	if (end >= start_hw)
		used_hw = end - start_hw;
	else
		used_hw = ctx->ring.max_num_elem + 1 - (start_hw - end);

	*num_free_re = ctx->ring.max_num_elem - used;
	gsi_update_ch_dp_stats(ctx, used_hw);
}

int gsi_query_channel_info(unsigned long chan_hdl,
		struct gsi_chan_info *info)
{
	struct gsi_chan_ctx *ctx;
	spinlock_t *slock;
	unsigned long flags;
	uint64_t rp;
	uint64_t wp;
	int ee = gsi_ctx->per.ee;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN || !info) {
		GSIERR("bad params chan_hdl=%lu info=%p\n", chan_hdl, info);
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
	int ee = gsi_ctx->per.ee;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN || !is_empty) {
		GSIERR("bad params chan_hdl=%lu is_empty=%p\n",
				chan_hdl, is_empty);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->evtr)
		slock = &ctx->evtr->ring.slock;
	else
		slock = &ctx->ring.slock;

	spin_lock_irqsave(slock, flags);

	rp = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_4_OFFS(ctx->props.ch_id, ee));
	rp |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_5_OFFS(ctx->props.ch_id, ee))) << 32;
	ctx->ring.rp = rp;

	wp = gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_6_OFFS(ctx->props.ch_id, ee));
	wp |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_GSI_CH_k_CNTXT_7_OFFS(ctx->props.ch_id, ee))) << 32;
	ctx->ring.wp = wp;

	if (ctx->props.dir == GSI_CHAN_DIR_FROM_GSI)
		*is_empty = (ctx->ring.rp_local == rp) ? true : false;
	else
		*is_empty = (wp == rp) ? true : false;

	spin_unlock_irqrestore(slock, flags);

	GSIDBG("ch=%lu RP=0x%llx WP=0x%llx RP_LOCAL=0x%llx\n",
			chan_hdl, rp, wp, ctx->ring.rp_local);

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_is_channel_empty);

int gsi_queue_xfer(unsigned long chan_hdl, uint16_t num_xfers,
		struct gsi_xfer_elem *xfer, bool ring_db)
{
	struct gsi_chan_ctx *ctx;
	uint16_t free;
	struct gsi_tre tre;
	struct gsi_tre *tre_ptr;
	uint16_t idx;
	uint64_t wp_rollback;
	int i;
	spinlock_t *slock;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN || !num_xfers || !xfer) {
		GSIERR("bad params chan_hdl=%lu num_xfers=%u xfer=%p\n",
				chan_hdl, num_xfers, xfer);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->evtr)
		slock = &ctx->evtr->ring.slock;
	else
		slock = &ctx->ring.slock;

	spin_lock_irqsave(slock, flags);
	__gsi_query_channel_free_re(ctx, &free);

	if (num_xfers > free) {
		GSIERR("chan_hdl=%lu num_xfers=%u free=%u\n",
				chan_hdl, num_xfers, free);
		spin_unlock_irqrestore(slock, flags);
		return -GSI_STATUS_RING_INSUFFICIENT_SPACE;
	}

	wp_rollback = ctx->ring.wp_local;
	for (i = 0; i < num_xfers; i++) {
		memset(&tre, 0, sizeof(tre));
		tre.buffer_ptr = xfer[i].addr;
		tre.buf_len = xfer[i].len;
		if (xfer[i].type == GSI_XFER_ELEM_DATA) {
			tre.re_type = GSI_RE_XFER;
		} else if (xfer[i].type == GSI_XFER_ELEM_IMME_CMD) {
			tre.re_type = GSI_RE_IMMD_CMD;
		} else {
			GSIERR("chan_hdl=%lu bad RE type=%u\n", chan_hdl,
				xfer[i].type);
			break;
		}
		tre.bei = (xfer[i].flags & GSI_XFER_FLAG_BEI) ? 1 : 0;
		tre.ieot = (xfer[i].flags & GSI_XFER_FLAG_EOT) ? 1 : 0;
		tre.ieob = (xfer[i].flags & GSI_XFER_FLAG_EOB) ? 1 : 0;
		tre.chain = (xfer[i].flags & GSI_XFER_FLAG_CHAIN) ? 1 : 0;

		idx = gsi_find_idx_from_addr(&ctx->ring, ctx->ring.wp_local);
		tre_ptr = (struct gsi_tre *)(ctx->ring.base_va +
				idx * ctx->ring.elem_sz);

		/* write the TRE to ring */
		*tre_ptr = tre;
		ctx->user_data[idx] = xfer[i].xfer_user_data;
		gsi_incr_ring_wp(&ctx->ring);
	}

	if (i != num_xfers) {
		/* reject all the xfers */
		ctx->ring.wp_local = wp_rollback;
		spin_unlock_irqrestore(slock, flags);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx->stats.queued += num_xfers;

	/* ensure TRE is set before ringing doorbell */
	wmb();

	if (ring_db)
		gsi_ring_chan_doorbell(ctx);

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

	if (chan_hdl >= GSI_MAX_CHAN) {
		GSIERR("bad params chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (ctx->state != GSI_CHAN_STATE_STARTED) {
		GSIERR("bad state %d\n", ctx->state);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	gsi_ring_chan_doorbell(ctx);

	return GSI_STATUS_SUCCESS;
};
EXPORT_SYMBOL(gsi_start_xfer);

int gsi_poll_channel(unsigned long chan_hdl,
		struct gsi_chan_xfer_notify *notify)
{
	struct gsi_chan_ctx *ctx;
	uint64_t rp;
	int ee = gsi_ctx->per.ee;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN || !notify) {
		GSIERR("bad params chan_hdl=%lu notify=%p\n", chan_hdl, notify);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI) {
		GSIERR("op not supported for protocol %u\n", ctx->props.prot);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	if (!ctx->evtr) {
		GSIERR("no event ring associated chan_hdl=%lu\n", chan_hdl);
		return -GSI_STATUS_UNSUPPORTED_OP;
	}

	spin_lock_irqsave(&ctx->evtr->ring.slock, flags);
	rp = gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_4_OFFS(ctx->evtr->id, ee));
	rp |= ((uint64_t)gsi_readl(gsi_ctx->base +
		GSI_EE_n_EV_CH_k_CNTXT_5_OFFS(ctx->evtr->id, ee))) << 32;
	ctx->evtr->ring.rp = rp;
	if (rp == ctx->evtr->ring.rp_local) {
		spin_unlock_irqrestore(&ctx->evtr->ring.slock, flags);
		ctx->stats.poll_empty++;
		return GSI_STATUS_POLL_EMPTY;
	}

	gsi_process_evt_re(ctx->evtr, notify, false);
	gsi_ring_evt_doorbell(ctx->evtr);
	spin_unlock_irqrestore(&ctx->evtr->ring.slock, flags);
	ctx->stats.poll_ok++;

	return GSI_STATUS_SUCCESS;
}
EXPORT_SYMBOL(gsi_poll_channel);

int gsi_config_channel_mode(unsigned long chan_hdl, enum gsi_chan_mode mode)
{
	struct gsi_chan_ctx *ctx;
	enum gsi_chan_mode curr;
	unsigned long flags;

	if (!gsi_ctx) {
		pr_err("%s:%d gsi context not allocated\n", __func__, __LINE__);
		return -GSI_STATUS_NODEV;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
		GSIERR("bad params chan_hdl=%lu mode=%u\n", chan_hdl, mode);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	ctx = &gsi_ctx->chan[chan_hdl];

	if (ctx->props.prot != GSI_CHAN_PROT_GPI) {
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
		ctx->stats.callback_to_poll++;
	}

	if (curr == GSI_CHAN_MODE_POLL &&
			mode == GSI_CHAN_MODE_CALLBACK) {
		__gsi_config_ieob_irq(gsi_ctx->per.ee, 1 << ctx->evtr->id, ~0);
		ctx->stats.poll_to_callback++;
	}
	atomic_set(&ctx->poll_mode, mode);
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
		GSIERR("bad params props=%p scr=%p\n", props, scr);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
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
		GSIERR("bad params props=%p\n", props);
		return -GSI_STATUS_INVALID_PARAMS;
	}

	if (chan_hdl >= GSI_MAX_CHAN) {
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

static void gsi_configure_ieps(void *base)
{
	void __iomem *gsi_base = (void __iomem *)base;

	gsi_writel(1, gsi_base + GSI_GSI_IRAM_PTR_CH_CMD_OFFS);
	gsi_writel(2, gsi_base + GSI_GSI_IRAM_PTR_CH_DB_OFFS);
	gsi_writel(3, gsi_base + GSI_GSI_IRAM_PTR_CH_DIS_COMP_OFFS);
	gsi_writel(4, gsi_base + GSI_GSI_IRAM_PTR_CH_EMPTY_OFFS);
	gsi_writel(5, gsi_base + GSI_GSI_IRAM_PTR_EE_GENERIC_CMD_OFFS);
	gsi_writel(6, gsi_base + GSI_GSI_IRAM_PTR_EVENT_GEN_COMP_OFFS);
	gsi_writel(7, gsi_base + GSI_GSI_IRAM_PTR_INT_MOD_STOPED_OFFS);
	gsi_writel(8, gsi_base + GSI_GSI_IRAM_PTR_IPA_IF_DESC_PROC_COMP_OFFS);
	gsi_writel(9, gsi_base + GSI_GSI_IRAM_PTR_IPA_IF_RESET_COMP_OFFS);
	gsi_writel(10, gsi_base + GSI_GSI_IRAM_PTR_IPA_IF_STOP_COMP_OFFS);
	gsi_writel(11, gsi_base + GSI_GSI_IRAM_PTR_NEW_RE_OFFS);
	gsi_writel(12, gsi_base + GSI_GSI_IRAM_PTR_READ_ENG_COMP_OFFS);
	gsi_writel(13, gsi_base + GSI_GSI_IRAM_PTR_TIMER_EXPIRED_OFFS);
}

static void gsi_configure_bck_prs_matrix(void *base)
{
	void __iomem *gsi_base = (void __iomem *)base;
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
	gsi_writel(0x00ffffff, gsi_base + GSI_IC_TLV_STOP_BCK_PRS_LSB_OFFS);
	gsi_writel(0xffffffff, gsi_base + GSI_IC_TLV_STOP_BCK_PRS_MSB_OFFS);
	gsi_writel(0xfdffffff, gsi_base + GSI_IC_TLV_RESET_BCK_PRS_LSB_OFFS);
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

int gsi_configure_regs(phys_addr_t gsi_base_addr, u32 gsi_size,
		phys_addr_t per_base_addr)
{
	void __iomem *gsi_base;

	gsi_base = ioremap_nocache(gsi_base_addr, gsi_size);
	if (!gsi_base) {
		GSIERR("ioremap failed for 0x%pa\n", &gsi_base_addr);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}
	gsi_writel(0, gsi_base + GSI_GSI_PERIPH_BASE_ADDR_MSB_OFFS);
	gsi_writel(per_base_addr,
			gsi_base + GSI_GSI_PERIPH_BASE_ADDR_LSB_OFFS);
	gsi_configure_bck_prs_matrix((void *)gsi_base);
	gsi_configure_ieps((void *)gsi_base);
	iounmap(gsi_base);

	return 0;
}
EXPORT_SYMBOL(gsi_configure_regs);

int gsi_enable_fw(phys_addr_t gsi_base_addr, u32 gsi_size)
{
	void __iomem *gsi_base;
	uint32_t value;

	gsi_base = ioremap_nocache(gsi_base_addr, gsi_size);
	if (!gsi_base) {
		GSIERR("ioremap failed for 0x%pa\n", &gsi_base_addr);
		return -GSI_STATUS_RES_ALLOC_FAILURE;
	}

	/* Enable the MCS and set to x2 clocks */
	value = (((1 << GSI_GSI_CFG_GSI_ENABLE_SHFT) &
			GSI_GSI_CFG_GSI_ENABLE_BMSK) |
		((1 << GSI_GSI_CFG_MCS_ENABLE_SHFT) &
			GSI_GSI_CFG_MCS_ENABLE_BMSK) |
		((1 << GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_SHFT) &
			GSI_GSI_CFG_DOUBLE_MCS_CLK_FREQ_BMSK) |
		((0 << GSI_GSI_CFG_UC_IS_MCS_SHFT) &
			GSI_GSI_CFG_UC_IS_MCS_BMSK));
	gsi_writel(value, gsi_base + GSI_GSI_CFG_OFFS);

	iounmap(gsi_base);

	return 0;

}
EXPORT_SYMBOL(gsi_enable_fw);

static int msm_gsi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_debug("gsi_probe\n");
	gsi_ctx = devm_kzalloc(dev, sizeof(*gsi_ctx), GFP_KERNEL);
	if (!gsi_ctx) {
		dev_err(dev, "failed to allocated gsi context\n");
		return -ENOMEM;
	}

	gsi_ctx->dev = dev;
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

/**
 * Module Init.
 */
static int __init gsi_init(void)
{
	pr_debug("gsi_init\n");
	return platform_driver_register(&msm_gsi_driver);
}

arch_initcall(gsi_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Generic Software Interface (GSI)");
