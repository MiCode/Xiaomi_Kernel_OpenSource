/**
 * gadget.c - DesignWare USB3 DRD Controller Gadget Framework Link
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/ratelimit.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>

#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "debug.h"
#include "core.h"
#include "gadget.h"
#include "debug.h"
#include "io.h"

static void dwc3_gadget_wakeup_interrupt(struct dwc3 *dwc, bool remote_wakeup);
static int dwc3_gadget_wakeup_int(struct dwc3 *dwc);

/**
 * dwc3_gadget_set_test_mode - Enables USB2 Test Modes
 * @dwc: pointer to our context structure
 * @mode: the mode to set (J, K SE0 NAK, Force Enable)
 *
 * Caller should take care of locking. This function will
 * return 0 on success or -EINVAL if wrong Test Selector
 * is passed
 */
int dwc3_gadget_set_test_mode(struct dwc3 *dwc, int mode)
{
	u32		reg;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;

	switch (mode) {
	case TEST_J:
	case TEST_K:
	case TEST_SE0_NAK:
	case TEST_PACKET:
	case TEST_FORCE_EN:
		reg |= mode << 1;
		break;
	default:
		return -EINVAL;
	}

	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	return 0;
}

/**
 * dwc3_gadget_get_link_state - Gets current state of USB Link
 * @dwc: pointer to our context structure
 *
 * Caller should take care of locking. This function will
 * return the link state on success (>= 0) or -ETIMEDOUT.
 */
int dwc3_gadget_get_link_state(struct dwc3 *dwc)
{
	u32		reg;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);

	return DWC3_DSTS_USBLNKST(reg);
}

/**
 * dwc3_gadget_set_link_state - Sets USB Link to a particular State
 * @dwc: pointer to our context structure
 * @state: the state to put link into
 *
 * Caller should take care of locking. This function will
 * return 0 on success or -ETIMEDOUT.
 */
int dwc3_gadget_set_link_state(struct dwc3 *dwc, enum dwc3_link_state state)
{
	int		retries = 10000;
	u32		reg;

	/*
	 * Wait until device controller is ready. Only applies to 1.94a and
	 * later RTL.
	 */
	if (dwc->revision >= DWC3_REVISION_194A) {
		while (--retries) {
			reg = dwc3_readl(dwc->regs, DWC3_DSTS);
			if (reg & DWC3_DSTS_DCNRD)
				udelay(5);
			else
				break;
		}

		if (retries <= 0)
			return -ETIMEDOUT;
	}

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;

	/* set requested state */
	reg |= DWC3_DCTL_ULSTCHNGREQ(state);
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	/*
	 * The following code is racy when called from dwc3_gadget_wakeup,
	 * and is not needed, at least on newer versions
	 */
	if (dwc->revision >= DWC3_REVISION_194A)
		return 0;

	/* wait for a change in DSTS */
	retries = 10000;
	while (--retries) {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);

		if (DWC3_DSTS_USBLNKST(reg) == state)
			return 0;

		udelay(5);
	}

	dev_vdbg(dwc->dev, "link state change request timed out\n");

	return -ETIMEDOUT;
}

/**
 * dwc3_gadget_resize_tx_fifos - reallocate fifo spaces for current use-case
 * @dwc: pointer to our context structure
 *
 * This function will a best effort FIFO allocation in order
 * to improve FIFO usage and throughput, while still allowing
 * us to enable as many endpoints as possible.
 *
 * Keep in mind that this operation will be highly dependent
 * on the configured size for RAM1 - which contains TxFifo -,
 * the amount of endpoints enabled on coreConsultant tool, and
 * the width of the Master Bus.
 *
 * In the ideal world, we would always be able to satisfy the
 * following equation:
 *
 * ((512 + 2 * MDWIDTH-Bytes) + (Number of IN Endpoints - 1) * \
 * (3 * (1024 + MDWIDTH-Bytes) + MDWIDTH-Bytes)) / MDWIDTH-Bytes
 *
 * Unfortunately, due to many variables that's not always the case.
 */
int dwc3_gadget_resize_tx_fifos(struct dwc3 *dwc)
{
	int		last_fifo_depth = 0;
	int		ram1_depth;
	int		fifo_size;
	int		mdwidth;
	int		num;
	int		num_eps;
	int		max_packet = 1024;
	struct usb_composite_dev *cdev = get_gadget_data(&dwc->gadget);

	if (!(cdev && cdev->config) || !dwc->needs_fifo_resize)
		return 0;

	num_eps = dwc->num_in_eps;
	ram1_depth = DWC3_RAM1_DEPTH(dwc->hwparams.hwparams7);
	mdwidth = DWC3_MDWIDTH(dwc->hwparams.hwparams0);

	/* MDWIDTH is represented in bits, we need it in bytes */
	mdwidth >>= 3;
	last_fifo_depth = (dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(0)) & 0xFFFF);
	dev_dbg(dwc->dev, "%s: num eps:%d max_packet:%d last_fifo_depth:%04x\n",
				__func__, num_eps, max_packet, last_fifo_depth);

	/* Don't resize ep0IN TxFIFO, start with ep1IN only. */
	for (num = 1; num < num_eps; num++) {
		/* bit0 indicates direction; 1 means IN ep */
		struct dwc3_ep	*dep = dwc->eps[(num << 1) | 1];
		int		mult = 1;
		int		tmp;

		tmp = max_packet + mdwidth;
		/*
		 * Interfaces like MBIM or ECM is having multiple data
		 * interfaces. SET_CONFIG() happens before set_alt with
		 * data interface 1 which results into calling this API
		 * before GSI endpoint enabled. This results no txfifo
		 * resize with GSI endpoint causing low throughput. Hence
		 * use mult as 3 for GSI IN endpoint always irrespective
		 * USB speed.
		 */
		if (dep->endpoint.ep_type == EP_TYPE_GSI)
			mult = 3;

		if (!(dep->flags & DWC3_EP_ENABLED)) {
			dev_dbg(dwc->dev, "ep%dIn not enabled", num);
			goto resize_fifo;
		}

		if (((dep->endpoint.maxburst > 1) &&
				usb_endpoint_xfer_bulk(dep->endpoint.desc))
				|| usb_endpoint_xfer_isoc(dep->endpoint.desc))
			mult = 3;

resize_fifo:
		tmp *= mult;
		tmp += mdwidth;

		fifo_size = DIV_ROUND_UP(tmp, mdwidth);

		fifo_size |= (last_fifo_depth << 16);

		dev_dbg(dwc->dev, "%s: Fifo Addr %04x Size %d\n",
				dep->name, last_fifo_depth, fifo_size & 0xffff);

		last_fifo_depth += (fifo_size & 0xffff);
		if (dwc->tx_fifo_size &&
				(last_fifo_depth >= dwc->tx_fifo_size)) {
			/*
			 * Fifo size allocated exceeded available RAM size.
			 * Hence return error.
			 */
			dev_err(dwc->dev, "Fifosize(%d) > available RAM(%d)\n",
					last_fifo_depth, dwc->tx_fifo_size);
			return -ENOMEM;
		}

		dwc3_writel(dwc->regs, DWC3_GTXFIFOSIZ(num), fifo_size);

	}

	return 0;
}

void dwc3_gadget_giveback(struct dwc3_ep *dep, struct dwc3_request *req,
		int status)
{
	struct dwc3			*dwc = dep->dwc;
	int				i;

	if (req->queued) {
		i = 0;
		do {
			dep->busy_slot++;
			/*
			 * Skip LINK TRB. We can't use req->trb and check for
			 * DWC3_TRBCTL_LINK_TRB because it points the TRB we
			 * just completed (not the LINK TRB).
			 */
			if (((dep->busy_slot & DWC3_TRB_MASK) ==
				DWC3_TRB_NUM- 1) &&
				usb_endpoint_xfer_isoc(dep->endpoint.desc))
				dep->busy_slot++;
		} while(++i < req->request.num_mapped_sgs);
		req->queued = false;

		if (req->request.zero && req->ztrb) {
			dep->busy_slot++;
			req->ztrb = NULL;
			if (((dep->busy_slot & DWC3_TRB_MASK) ==
				DWC3_TRB_NUM - 1) &&
				usb_endpoint_xfer_isoc(dep->endpoint.desc))
				dep->busy_slot++;
		}
	}
	list_del(&req->list);
	req->trb = NULL;

	if (req->request.status == -EINPROGRESS)
		req->request.status = status;

	if (dwc->ep0_bounced && dep->number == 0)
		dwc->ep0_bounced = false;
	else
		usb_gadget_unmap_request(&dwc->gadget, &req->request,
				req->direction);

	dev_dbg(dwc->dev, "request %p from %s completed %d/%d ===> %d\n",
			req, dep->name, req->request.actual,
			req->request.length, status);
	trace_dwc3_gadget_giveback(req);

	dbg_done(dep->number, req->request.actual, req->request.status);
	spin_unlock(&dwc->lock);
	usb_gadget_giveback_request(&dep->endpoint, &req->request);
	spin_lock(&dwc->lock);
}

int dwc3_send_gadget_generic_command(struct dwc3 *dwc, unsigned cmd, u32 param)
{
	u32		timeout = 500;
	u32		reg;

	trace_dwc3_gadget_generic_cmd(cmd, param);

	dwc3_writel(dwc->regs, DWC3_DGCMDPAR, param);
	dwc3_writel(dwc->regs, DWC3_DGCMD, cmd | DWC3_DGCMD_CMDACT);

	do {
		reg = dwc3_readl(dwc->regs, DWC3_DGCMD);
		if (!(reg & DWC3_DGCMD_CMDACT)) {
			dev_vdbg(dwc->dev, "Command Complete --> %d\n",
					DWC3_DGCMD_STATUS(reg));
			if (DWC3_DGCMD_STATUS(reg))
				return -EINVAL;
			return 0;
		}

		/*
		 * We can't sleep here, because it's also called from
		 * interrupt context.
		 */
		timeout--;
		if (!timeout)
			return -ETIMEDOUT;
		udelay(1);
	} while (1);
}

int dwc3_send_gadget_ep_cmd(struct dwc3 *dwc, unsigned ep,
		unsigned cmd, struct dwc3_gadget_ep_cmd_params *params)
{
	struct dwc3_ep		*dep = dwc->eps[ep];
	u32			timeout = 1500;
	u32			reg;

	trace_dwc3_gadget_ep_cmd(dep, cmd, params);

	dwc3_writel(dwc->regs, DWC3_DEPCMDPAR0(ep), params->param0);
	dwc3_writel(dwc->regs, DWC3_DEPCMDPAR1(ep), params->param1);
	dwc3_writel(dwc->regs, DWC3_DEPCMDPAR2(ep), params->param2);

	dwc3_writel(dwc->regs, DWC3_DEPCMD(ep), cmd | DWC3_DEPCMD_CMDACT);
	do {
		reg = dwc3_readl(dwc->regs, DWC3_DEPCMD(ep));
		if (!(reg & DWC3_DEPCMD_CMDACT)) {
			dev_vdbg(dwc->dev, "Command Complete --> %d\n",
					DWC3_DEPCMD_STATUS(reg));

			/* SW issues START TRANSFER command to isochronous ep
			 * with future frame interval. If future interval time
			 * has already passed when core recieves command, core
			 * will respond with an error(bit13 in Command complete
			 * event. Hence return error in this case.
			 */
			if (reg & 0x2000)
				return -EAGAIN;
			else if (DWC3_DEPCMD_STATUS(reg))
				return -EINVAL;
			return 0;
		}

		/*
		 * We can't sleep here, because it is also called from
		 * interrupt context.
		 */
		timeout--;
		if (!timeout) {
			dev_err(dwc->dev, "%s command timeout for %s\n",
				dwc3_gadget_ep_cmd_string(cmd), dep->name);
			return -ETIMEDOUT;
		}

		udelay(1);
	} while (1);
}

static int dwc3_alloc_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;

	if (dep->trb_pool)
		return 0;

	if (dep->number == 0 || dep->number == 1)
		return 0;

	dep->trb_pool = dma_zalloc_coherent(dwc->dev,
			sizeof(struct dwc3_trb) * DWC3_TRB_NUM,
			&dep->trb_pool_dma, GFP_ATOMIC);
	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "failed to allocate trb pool for %s\n",
				dep->name);
		return -ENOMEM;
	}

	return 0;
}

static void dwc3_free_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;

	/* Freeing of GSI EP TRBs are handled by GSI EP ops. */
	if (dep->endpoint.ep_type == EP_TYPE_GSI)
		return;

	if (dep->trb_pool && dep->trb_pool_dma) {
		dma_free_coherent(dwc->dev,
			sizeof(struct dwc3_trb) * DWC3_TRB_NUM, dep->trb_pool,
			dep->trb_pool_dma);

		dep->trb_pool = NULL;
		dep->trb_pool_dma = 0;
	}
}

static int dwc3_gadget_start_config(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;
	u32			cmd;

	memset(&params, 0x00, sizeof(params));

	if (dep->number != 1) {
		cmd = DWC3_DEPCMD_DEPSTARTCFG;
		/* XferRscIdx == 0 for ep0 and 2 for the remaining */
		if (dep->number > 1) {
			if (dwc->start_config_issued)
				return 0;
			dwc->start_config_issued = true;
			cmd |= DWC3_DEPCMD_PARAM(2);
		}

		return dwc3_send_gadget_ep_cmd(dwc, 0, cmd, &params);
	}

	return 0;
}

static int dwc3_gadget_set_ep_config(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct usb_endpoint_descriptor *desc,
		const struct usb_ss_ep_comp_descriptor *comp_desc,
		bool ignore, bool restore)
{
	struct dwc3_gadget_ep_cmd_params params;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* Burst size is only needed in SuperSpeed mode */
	if (dwc->gadget.speed == USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst - 1;

		params.param0 |= DWC3_DEPCFG_BURST_SIZE(burst);
	}

	if (ignore)
		params.param0 |= DWC3_DEPCFG_IGN_SEQ_NUM;

	if (restore) {
		params.param0 |= DWC3_DEPCFG_ACTION_RESTORE;
		params.param2 |= dep->saved_state;
	}

	if (!dep->endpoint.endless) {
		pr_debug("%s(): enable xfer_complete_int for %s\n",
				__func__, dep->endpoint.name);
		params.param1 = DWC3_DEPCFG_XFER_COMPLETE_EN
				| DWC3_DEPCFG_XFER_NOT_READY_EN;
	} else {
		pr_debug("%s(): disable xfer_complete_int for %s\n",
				 __func__, dep->endpoint.name);
	}

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params.param1 |= DWC3_DEPCFG_STREAM_CAPABLE
			| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	if (usb_endpoint_xfer_isoc(desc))
		params.param1 |= DWC3_DEPCFG_XFER_IN_PROGRESS_EN;

	/*
	 * We are doing 1:1 mapping for endpoints, meaning
	 * Physical Endpoints 2 maps to Logical Endpoint 2 and
	 * so on. We consider the direction bit as part of the physical
	 * endpoint number. So USB endpoint 0x81 is 0x03.
	 */
	params.param1 |= DWC3_DEPCFG_EP_NUMBER(dep->number);

	/*
	 * We must use the lower 16 TX FIFOs even though
	 * HW might have more
	 */
	if (dep->direction)
		params.param0 |= DWC3_DEPCFG_FIFO_NUMBER(dep->number >> 1);

	if (desc->bInterval) {
		params.param1 |= DWC3_DEPCFG_BINTERVAL_M1(desc->bInterval - 1);
		dep->interval = 1 << (desc->bInterval - 1);
	}

	return dwc3_send_gadget_ep_cmd(dwc, dep->number,
			DWC3_DEPCMD_SETEPCONFIG, &params);
}

static int dwc3_gadget_set_xfer_resource(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPXFERCFG_NUM_XFER_RES(1);

	return dwc3_send_gadget_ep_cmd(dwc, dep->number,
			DWC3_DEPCMD_SETTRANSFRESOURCE, &params);
}

/**
 * __dwc3_gadget_ep_enable - Initializes a HW endpoint
 * @dep: endpoint to be initialized
 * @desc: USB Endpoint Descriptor
 *
 * Caller should take care of locking
 */
static int __dwc3_gadget_ep_enable(struct dwc3_ep *dep,
		const struct usb_endpoint_descriptor *desc,
		const struct usb_ss_ep_comp_descriptor *comp_desc,
		bool ignore, bool restore)
{
	struct dwc3		*dwc = dep->dwc;
	u32			reg;
	int			ret;

	dev_vdbg(dwc->dev, "Enabling %s\n", dep->name);

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		ret = dwc3_gadget_start_config(dwc, dep);
		if (ret) {
			dev_err(dwc->dev, "start_config() failed for %s\n",
								dep->name);
			return ret;
		}
	}

	ret = dwc3_gadget_set_ep_config(dwc, dep, desc, comp_desc, ignore,
			restore);
	if (ret) {
		dev_err(dwc->dev, "set_ep_config() failed for %s\n", dep->name);
		return ret;
	}

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		struct dwc3_trb	*trb_st_hw;
		struct dwc3_trb	*trb_link;

		ret = dwc3_gadget_set_xfer_resource(dwc, dep);
		if (ret) {
			dev_err(dwc->dev, "set_xfer_resource() failed for %s\n",
								dep->name);
			return ret;
		}

		dep->endpoint.desc = desc;
		dep->comp_desc = comp_desc;
		dep->type = usb_endpoint_type(desc);
		dep->flags |= DWC3_EP_ENABLED;

		reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);

		if (!usb_endpoint_xfer_isoc(desc))
			return 0;

		/* Link TRB for ISOC. The HWO bit is never reset */
		trb_st_hw = &dep->trb_pool[0];

		trb_link = &dep->trb_pool[DWC3_TRB_NUM - 1];
		memset(trb_link, 0, sizeof(*trb_link));

		trb_link->bpl = lower_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->bph = upper_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->ctrl |= DWC3_TRBCTL_LINK_TRB;
		trb_link->ctrl |= DWC3_TRB_CTRL_HWO;
	}

	return 0;
}

static void dwc3_remove_requests(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_request		*req;

	if (!list_empty(&dep->req_queued)) {
		dwc3_stop_active_transfer(dwc, dep->number, true);

		/* - giveback all requests to gadget driver */
		while (!list_empty(&dep->req_queued)) {
			req = next_request(&dep->req_queued);

			dwc3_gadget_giveback(dep, req, -ESHUTDOWN);
		}
	}

	while (!list_empty(&dep->request_list)) {
		req = next_request(&dep->request_list);

		dwc3_gadget_giveback(dep, req, -ESHUTDOWN);
	}
}

/**
 * __dwc3_gadget_ep_disable - Disables a HW endpoint
 * @dep: the endpoint to disable
 *
 * This function also removes requests which are currently processed ny the
 * hardware and those which are not yet scheduled.
 * Caller should take care of locking.
 */
static int __dwc3_gadget_ep_disable(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;
	u32			reg;

	if (dep->endpoint.ep_type == EP_TYPE_NORMAL)
		dwc3_remove_requests(dwc, dep);
	else if (dep->endpoint.ep_type == EP_TYPE_GSI)
		dwc3_stop_active_transfer(dwc, dep->number, true);

	/* make sure HW endpoint isn't stalled */
	if (dep->flags & DWC3_EP_STALL)
		__dwc3_gadget_ep_set_halt(dep, 0, false);

	reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
	reg &= ~DWC3_DALEPENA_EP(dep->number);
	dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);

	dep->stream_capable = false;
	dep->endpoint.desc = NULL;
	dep->comp_desc = NULL;
	dep->type = 0;
	dep->flags = 0;

	/*
	 * Clean up ep ring to avoid getting xferInProgress due to stale trbs
	 * with HWO bit set from previous composition when update transfer cmd
	 * is issued.
	 */
	memset(&dep->trb_pool[0], 0, sizeof(struct dwc3_trb) * DWC3_TRB_NUM);
	dbg_event(dep->number, "Clr_TRB", 0);
	return 0;
}

/* -------------------------------------------------------------------------- */

static int dwc3_gadget_ep0_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	return -EINVAL;
}

static int dwc3_gadget_ep0_disable(struct usb_ep *ep)
{
	return -EINVAL;
}

/* -------------------------------------------------------------------------- */

static int dwc3_gadget_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct dwc3_ep			*dep;
	struct dwc3			*dwc;
	unsigned long			flags;
	int				ret;

	if (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		pr_debug("dwc3: invalid parameters. ep=%p, desc=%p, DT=%d\n",
			ep, desc, desc ? desc->bDescriptorType : 0);
		return -EINVAL;
	}

	if (!desc->wMaxPacketSize) {
		pr_debug("dwc3: missing wMaxPacketSize\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (dep->flags & DWC3_EP_ENABLED) {
		dev_WARN_ONCE(dwc->dev, true, "%s is already enabled\n",
				dep->name);
		return 0;
	}

	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		strlcat(dep->name, "-control", sizeof(dep->name));
		break;
	case USB_ENDPOINT_XFER_ISOC:
		strlcat(dep->name, "-isoc", sizeof(dep->name));
		break;
	case USB_ENDPOINT_XFER_BULK:
		strlcat(dep->name, "-bulk", sizeof(dep->name));
		break;
	case USB_ENDPOINT_XFER_INT:
		strlcat(dep->name, "-int", sizeof(dep->name));
		break;
	default:
		dev_err(dwc->dev, "invalid endpoint transfer type\n");
	}

	ret = dwc3_alloc_trb_pool(dep);
	if (ret)
		return ret;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_enable(dep, desc, ep->comp_desc, false, false);
	dbg_event(dep->number, "ENABLE", ret);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_ep_disable(struct usb_ep *ep)
{
	struct dwc3_ep			*dep;
	struct dwc3			*dwc;
	unsigned long			flags;
	int				ret;

	if (!ep) {
		pr_debug("dwc3: invalid parameters\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		dev_dbg(dwc->dev, "%s is already disabled\n", dep->name);
		dbg_event(dep->number, "ALRDY DISABLED", dep->flags);
		return 0;
	}

	/* Keep GSI ep names with "-gsi" suffix */
	if (!strnstr(dep->name, "gsi", 10)) {
		snprintf(dep->name, sizeof(dep->name), "ep%d%s",
			dep->number >> 1,
			(dep->number & 1) ? "in" : "out");
	}

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_disable(dep);
	dbg_event(dep->number, "DISABLE", ret);
	spin_unlock_irqrestore(&dwc->lock, flags);

	dwc3_free_trb_pool(dep);

	return ret;
}

static struct usb_request *dwc3_gadget_ep_alloc_request(struct usb_ep *ep,
	gfp_t gfp_flags)
{
	struct dwc3_request		*req;
	struct dwc3_ep			*dep = to_dwc3_ep(ep);

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		return NULL;

	req->epnum	= dep->number;
	req->dep	= dep;
	req->request.dma = DMA_ERROR_CODE;

	trace_dwc3_alloc_request(req);

	return &req->request;
}

static void dwc3_gadget_ep_free_request(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request		*req = to_dwc3_request(request);

	trace_dwc3_free_request(req);
	kfree(req);
}

/**
 * dwc3_prepare_one_trb - setup one TRB from one request
 * @dep: endpoint for which this request is prepared
 * @req: dwc3_request pointer
 */
static void dwc3_prepare_one_trb(struct dwc3_ep *dep,
		struct dwc3_request *req, dma_addr_t dma,
		unsigned length, unsigned last, unsigned chain, unsigned node,
		unsigned ioc)
{
	struct dwc3		*dwc = dep->dwc;
	struct dwc3_trb		*trb;
	bool			zlp_appended = false;
	unsigned		rlen;

	dev_vdbg(dwc->dev, "%s: req %p dma %08llx length %d%s%s\n",
			dep->name, req, (unsigned long long) dma,
			length, last ? " last" : "",
			chain ? " chain" : "");


	trb = &dep->trb_pool[dep->free_slot & DWC3_TRB_MASK];

	if (!req->trb) {
		dwc3_gadget_move_request_queued(req);
		req->trb = trb;
		req->trb_dma = dwc3_trb_dma_offset(dep, trb);
		req->start_slot = dep->free_slot & DWC3_TRB_MASK;
	}

	dep->free_slot++;
	/* Skip the LINK-TRB on ISOC */
	if (((dep->free_slot & DWC3_TRB_MASK) == DWC3_TRB_NUM - 1) &&
			usb_endpoint_xfer_isoc(dep->endpoint.desc))
		dep->free_slot++;

update_trb:
	trb->size = DWC3_TRB_SIZE_LENGTH(length);
	trb->bpl = lower_32_bits(dma);
	trb->bph = upper_32_bits(dma);

	switch (usb_endpoint_type(dep->endpoint.desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		trb->ctrl = DWC3_TRBCTL_CONTROL_SETUP;
		break;

	case USB_ENDPOINT_XFER_ISOC:
		if (!node)
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS_FIRST;
		else
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS;

		if (!req->request.no_interrupt && !chain)
			trb->ctrl |= DWC3_TRB_CTRL_IOC;
		break;

	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		trb->ctrl = DWC3_TRBCTL_NORMAL;
		if (ioc)
			trb->ctrl |= DWC3_TRB_CTRL_IOC;
		break;
	default:
		/*
		 * This is only possible with faulty memory because we
		 * checked it already :)
		 */
		BUG();
	}

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		trb->ctrl |= DWC3_TRB_CTRL_ISP_IMI;
		trb->ctrl |= DWC3_TRB_CTRL_CSP;
	}

	if (chain)
		trb->ctrl |= DWC3_TRB_CTRL_CHN;

	if (usb_endpoint_xfer_bulk(dep->endpoint.desc) && dep->stream_capable)
		trb->ctrl |= DWC3_TRB_CTRL_SID_SOFN(req->request.stream_id);

	trb->ctrl |= DWC3_TRB_CTRL_HWO;

	trace_dwc3_prepare_trb(dep, trb);

	rlen = req->request.length;
	if (!zlp_appended && !chain &&
		req->request.zero && rlen &&
		(rlen % usb_endpoint_maxp(dep->endpoint.desc) == 0)) {

		zlp_appended = true;
		/* Skip the LINK-TRB on ISOC */
		if (((dep->free_slot & DWC3_TRB_MASK) == DWC3_TRB_NUM - 1) &&
			usb_endpoint_xfer_isoc(dep->endpoint.desc))
			dep->free_slot++;

		trb->ctrl |= DWC3_TRB_CTRL_CHN;
		trb = &dep->trb_pool[dep->free_slot & DWC3_TRB_MASK];
		dep->free_slot++;

		req->ztrb = trb;
		length = 0;

		goto update_trb;
	}

	if (!usb_endpoint_xfer_isoc(dep->endpoint.desc) && last)
		trb->ctrl |= DWC3_TRB_CTRL_LST;
}

/*
 * dwc3_prepare_trbs - setup TRBs from requests
 * @dep: endpoint for which requests are being prepared
 * @starting: true if the endpoint is idle and no requests are queued.
 *
 * The function goes through the requests list and sets up TRBs for the
 * transfers. The function returns once there are no more TRBs available or
 * it runs out of requests.
 */
static void dwc3_prepare_trbs(struct dwc3_ep *dep, bool starting)
{
	struct dwc3_request	*req, *n;
	u32			trbs_left;
	u32			max;
	unsigned int		last_one = 0;
	int			maxpkt_size;
	bool			isoc;

	maxpkt_size = usb_endpoint_maxp(dep->endpoint.desc);
	isoc = usb_endpoint_xfer_isoc(dep->endpoint.desc);

	BUILD_BUG_ON_NOT_POWER_OF_2(DWC3_TRB_NUM);

	/* the first request must not be queued */
	trbs_left = (dep->busy_slot - dep->free_slot) & DWC3_TRB_MASK;

	/* Can't wrap around on a non-isoc EP since there's no link TRB */
	if (!usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		max = DWC3_TRB_NUM - (dep->free_slot & DWC3_TRB_MASK);
		if (trbs_left > max)
			trbs_left = max;
	}

	/*
	 * If busy & slot are equal than it is either full or empty. If we are
	 * starting to process requests then we are empty. Otherwise we are
	 * full and don't do anything
	 */
	if (!trbs_left) {
		if (!starting)
			return;
		dep->busy_slot = 0;
		dep->free_slot = 0;
		/* For ISOC requests the last TRB is reserved for link TRB */
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc))
			trbs_left = DWC3_TRB_NUM-1;
		else
			trbs_left = DWC3_TRB_NUM;
	}

	/*
	 * If free_slot = DWC3_TRB_MASK-1 and trbs_left > 0 then we have a
	 * wraparound in the TRB buffer. Hence, trbs_left includes the link TRB
	 * and must be reduced by 1.
	 */
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
		(dep->free_slot & DWC3_TRB_MASK) == DWC3_TRB_MASK-1)
			trbs_left--;

	list_for_each_entry_safe(req, n, &dep->request_list, list) {
		unsigned	length;
		dma_addr_t	dma;
		int		num_trbs_required = 0;

		last_one = false;

		/* The last TRB is a link TRB, not used for xfer */
		if (isoc)
			num_trbs_required++;

		if (req->request.num_mapped_sgs)
			num_trbs_required += req->request.num_mapped_sgs;
		else
			num_trbs_required++;

		if (req->request.zero && req->request.length &&
				(req->request.length % maxpkt_size == 0))
			num_trbs_required++;

		if (trbs_left < num_trbs_required)
			break;

		if (req->request.num_mapped_sgs > 0) {
			struct usb_request *request = &req->request;
			struct scatterlist *sg = request->sg;
			struct scatterlist *s;
			int		i;

			for_each_sg(sg, s, request->num_mapped_sgs, i) {
				unsigned chain = true;
				unsigned	ioc = 0;

				length = sg_dma_len(s);
				dma = sg_dma_address(s);

				if (i == (request->num_mapped_sgs - 1) ||
						sg_is_last(s)) {
					unsigned temp = 0;
					unsigned len;
					struct dwc3_request *nreq = n;
					struct usb_request *ureq;
					bool mpkt = false;

					chain = false;
					if (list_empty(&dep->request_list)) {
						last_one = true;
						goto start_trb_queuing;
					}

					/*
					 * check if trbs sufficient for next
					 * request to set the last bit
					 */
					ureq = &nreq->request;
					len  = ureq->length;

					if (len % maxpkt_size == 0)
						mpkt = true;

					if (ureq->zero && len && mpkt)
						temp++;

					if (ureq->num_mapped_sgs)
						temp +=
						ureq->num_mapped_sgs;
					else
						temp++;

					if (isoc)
						temp++;

					if (trbs_left <= temp)
						last_one = true;

				}

start_trb_queuing:
				trbs_left--;
				if (!trbs_left)
					last_one = true;

				if (last_one)
					chain = false;

				if (!last_one && !chain &&
					!request->no_interrupt)
					ioc = 1;

				dwc3_prepare_one_trb(dep, req, dma, length,
						last_one, chain, i, ioc);

				if (last_one)
					break;
			}
			dbg_queue(dep->number, &req->request, trbs_left);

			if (last_one)
				break;
		} else {
			struct dwc3_request	*req1;
			int maxpkt_size = usb_endpoint_maxp(dep->endpoint.desc);

			dma = req->request.dma;
			length = req->request.length;
			trbs_left--;

			if (req->request.zero && length &&
						(length % maxpkt_size == 0))
				trbs_left--;

			if (!trbs_left) {
				last_one = 1;
			} else if (dep->direction && (trbs_left <= 1)) {
				req1 = next_request(&req->list);
				if (req1->request.zero && req1->request.length
				 && (req1->request.length % maxpkt_size == 0))
					last_one = 1;
			}

			/* Is this the last request? */
			if (list_is_last(&req->list, &dep->request_list))
				last_one = 1;

			dwc3_prepare_one_trb(dep, req, dma, length,
					last_one, false, 0, 0);

			dbg_queue(dep->number, &req->request, 0);
			if (last_one)
				break;
		}
	}
}

static int __dwc3_gadget_kick_transfer(struct dwc3_ep *dep, u16 cmd_param,
		int start_new)
{
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3_request		*req, *req1, *n;
	struct dwc3			*dwc = dep->dwc;
	int				ret;
	u32				cmd;

	if (start_new && (dep->flags & DWC3_EP_BUSY)) {
		dev_vdbg(dwc->dev, "%s: endpoint busy\n", dep->name);
		return -EBUSY;
	}
	dep->flags &= ~DWC3_EP_PENDING_REQUEST;

	/*
	 * If we are getting here after a short-out-packet we don't enqueue any
	 * new requests as we try to set the IOC bit only on the last request.
	 */
	if (start_new) {
		if (list_empty(&dep->req_queued))
			dwc3_prepare_trbs(dep, start_new);

		/* req points to the first request which will be sent */
		req = next_request(&dep->req_queued);
	} else {
		dwc3_prepare_trbs(dep, start_new);

		/*
		 * req points to the first request where HWO changed from 0 to 1
		 */
		req = next_request(&dep->req_queued);
	}
	if (!req) {
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		dbg_event(dep->number, "NO REQ", 0);
		return 0;
	}

	memset(&params, 0, sizeof(params));

	if (start_new) {
		params.param0 = upper_32_bits(req->trb_dma);
		params.param1 = lower_32_bits(req->trb_dma);
		cmd = DWC3_DEPCMD_STARTTRANSFER;
	} else {
		cmd = DWC3_DEPCMD_UPDATETRANSFER;
	}

	cmd |= DWC3_DEPCMD_PARAM(cmd_param);
	ret = dwc3_send_gadget_ep_cmd(dwc, dep->number, cmd, &params);
	if (ret < 0) {
		dev_dbg(dwc->dev, "failed to send STARTTRANSFER command\n");

		if ((ret == -EAGAIN) && start_new &&
				usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			/* If bit13 in Command complete event is set, software
			 * must issue ENDTRANDFER command and wait for
			 * Xfernotready event to queue the requests again.
			 */
			if (!dep->resource_index) {
				dep->resource_index =
					 dwc3_gadget_ep_get_transfer_index(dwc,
								dep->number);
				WARN_ON_ONCE(!dep->resource_index);
			}
			dwc3_stop_active_transfer(dwc, dep->number, true);
			list_for_each_entry_safe_reverse(req1, n,
						 &dep->req_queued, list) {
				req1->trb = NULL;
				dwc3_gadget_move_request_list_front(req1);
				if (req->request.num_mapped_sgs)
					dep->busy_slot +=
						 req->request.num_mapped_sgs;
				else
					dep->busy_slot++;
				if ((dep->busy_slot & DWC3_TRB_MASK) ==
							DWC3_TRB_NUM - 1)
					dep->busy_slot++;
			}
			return ret;
		} else {
			/*
			 * FIXME we need to iterate over the list of requests
			 * here and stop, unmap, free and del each of the linked
			 * requests instead of what we do now.
			 */
			usb_gadget_unmap_request(&dwc->gadget, &req->request,
				req->direction);
			list_del(&req->list);
			return ret;
		}
	}

	dep->flags |= DWC3_EP_BUSY;

	if (start_new) {
		dep->resource_index = dwc3_gadget_ep_get_transfer_index(dwc,
				dep->number);
		WARN_ON_ONCE(!dep->resource_index);
	}

	return 0;
}

static void __dwc3_gadget_start_isoc(struct dwc3 *dwc,
		struct dwc3_ep *dep, u32 cur_uf)
{
	u32 uf;
	int ret;

	dep->current_uf = cur_uf;

	if (list_empty(&dep->request_list)) {
		dev_vdbg(dwc->dev, "ISOC ep %s run out for requests.\n",
			dep->name);
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		return;
	}

	/*
	 * WORKAROUND: DWC3 Revisions <= 2.50a have an issue which can result
	 * in dropping of ISOC packets.
	 *
	 * Due to this problem, we might experience a glitch in the associated
	 * audio or video application. The workaround is to disable DCTL[10:9]
	 * when an ISOC transfer is in progress.
	 *
	 * This is the first half of that workaround.
	 *
	 * Refers to:
	 *
	 * STAR#9000627217: For a Duration of 10ms, If All ITPs Are Received
	 * With Delay Bit Set or No ITPs Are Received, Device Drops ISOC
	 * Packets.
	 */
	if (dwc->revision <= DWC3_REVISION_250A) {
		u32	u1;
		u32	reg;

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		u1 = reg & (DWC3_DCTL_INITU1ENA | DWC3_DCTL_ACCEPTU1ENA);

		if (!dwc->u1)
			dwc->u1 = u1;

		reg &= ~u1;

		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}

	/* 4 micro frames in the future */
	uf = cur_uf + dep->interval * 4;

	ret = __dwc3_gadget_kick_transfer(dep, uf, 1);
	if (ret < 0)
		dbg_event(dep->number, "ISOC QUEUE", ret);
}

static void dwc3_gadget_start_isoc(struct dwc3 *dwc,
		struct dwc3_ep *dep, const struct dwc3_event_depevt *event)
{
	u32 cur_uf, mask;

	mask = ~(dep->interval - 1);
	cur_uf = event->parameters & mask;

	__dwc3_gadget_start_isoc(dwc, dep, cur_uf);
}

static int __dwc3_gadget_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3		*dwc = dep->dwc;
	int			ret;

	if (req->request.status == -EINPROGRESS) {
		ret = -EBUSY;
		dev_err(dwc->dev, "%s: %p request already in queue",
					dep->name, req);
		return ret;
	}

	req->request.actual	= 0;
	req->request.status	= -EINPROGRESS;
	req->direction		= dep->direction;
	req->epnum		= dep->number;

	/*
	 * We only add to our list of requests now and
	 * start consuming the list once we get XferNotReady
	 * IRQ.
	 *
	 * That way, we avoid doing anything that we don't need
	 * to do now and defer it until the point we receive a
	 * particular token from the Host side.
	 *
	 * This will also avoid Host cancelling URBs due to too
	 * many NAKs.
	 */
	ret = usb_gadget_map_request(&dwc->gadget, &req->request,
			dep->direction);
	if (ret)
		return ret;

	list_add_tail(&req->list, &dep->request_list);

	/*
	 * There are a few special cases:
	 *
	 * 1. XferNotReady with empty list of requests. We need to kick the
	 *    transfer here in that situation, otherwise we will be NAKing
	 *    forever. If we get XferNotReady before gadget driver has a
	 *    chance to queue a request, we will ACK the IRQ but won't be
	 *    able to receive the data until the next request is queued.
	 *    The following code is handling exactly that.
	 *
	 */
	if (dep->flags & DWC3_EP_PENDING_REQUEST) {
		/*
		 * If xfernotready is already elapsed and it is a case
		 * of isoc transfer, then issue END TRANSFER, so that
		 * you can receive xfernotready again and can have
		 * notion of current microframe.
		 */
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			/* If xfernotready event is recieved before issuing
			 * START TRANSFER command, don't issue END TRANSFER.
			 * Rather start queueing the requests by issuing START
			 * TRANSFER command.
			 */
			if (list_empty(&dep->req_queued) && dep->resource_index)
				dwc3_stop_active_transfer(dwc, dep->number, true);
			else
				__dwc3_gadget_start_isoc(dwc, dep,
							dep->current_uf);
			dep->flags &= ~DWC3_EP_PENDING_REQUEST;
			return 0;
		}

		ret = __dwc3_gadget_kick_transfer(dep, 0, true);
		if (ret && ret != -EBUSY) {
			dbg_event(dep->number, "XfNR QUEUE", ret);
			dev_dbg(dwc->dev, "%s: failed to kick transfers\n",
					dep->name);
		}
		return ret;
	}

	/*
	 * 2. XferInProgress on Isoc EP with an active transfer. We need to
	 *    kick the transfer here after queuing a request, otherwise the
	 *    core may not see the modified TRB(s).
	 */
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
			(dep->flags & DWC3_EP_BUSY) &&
			!(dep->flags & DWC3_EP_MISSED_ISOC)) {
		WARN_ON_ONCE(!dep->resource_index);
		ret = __dwc3_gadget_kick_transfer(dep, dep->resource_index,
				false);
		if (ret && ret != -EBUSY) {
			dbg_event(dep->number, "XfIP QUEUE", ret);
			dev_dbg(dwc->dev, "%s: failed to kick transfers\n",
					dep->name);
		}
		return ret;
	}

	/*
	 * 4. Stream Capable Bulk Endpoints. We need to start the transfer
	 * right away, otherwise host will not know we have streams to be
	 * handled.
	 */
	if (dep->stream_capable) {
		int	ret;

		ret = __dwc3_gadget_kick_transfer(dep, 0, true);
		if (ret && ret != -EBUSY) {
			struct dwc3	*dwc = dep->dwc;

			dev_dbg(dwc->dev, "%s: failed to kick transfers\n",
					dep->name);
		}
	}

	return 0;
}

static int dwc3_gadget_wakeup(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	schedule_work(&dwc->wakeup_work);
	return 0;
}

static inline enum dwc3_link_state dwc3_get_link_state(struct dwc3 *dwc)
{
	u32 reg;
	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	return DWC3_DSTS_USBLNKST(reg);
}

static bool dwc3_gadget_is_suspended(struct dwc3 *dwc)
{
	if (atomic_read(&dwc->in_lpm) ||
		dwc->link_state == DWC3_LINK_STATE_U3)
		return true;
	return false;
}

static int dwc3_gadget_ep_queue(struct usb_ep *ep, struct usb_request *request,
	gfp_t gfp_flags)
{
	struct dwc3_request		*req = to_dwc3_request(request);
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;
	int				ret;

	spin_lock_irqsave(&dwc->lock, flags);
	if (!dep->endpoint.desc) {
		dev_dbg(dwc->dev, "trying to queue request %p to disabled %s\n",
				request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -ESHUTDOWN;
	}

	/*
	 * Queuing endless request to USB endpoint through generic ep queue
	 * API should not be allowed.
	 */
	if (dep->endpoint.endless) {
		dev_dbg(dwc->dev, "trying to queue endless request %p to %s\n",
				request, ep->name);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EPERM;
	}

	if (dwc3_gadget_is_suspended(dwc)) {
		if (dwc->gadget.remote_wakeup)
			dwc3_gadget_wakeup(&dwc->gadget);
		spin_unlock_irqrestore(&dwc->lock, flags);
		return dwc->gadget.remote_wakeup ? -EAGAIN : -ENOTSUPP;
	}

	dev_vdbg(dwc->dev, "queing request %p to %s length %d\n",
			request, ep->name, request->length);
	trace_dwc3_ep_queue(req);

	WARN(!dep->direction && (request->length % ep->desc->wMaxPacketSize),
		"trying to queue unaligned request (%d)\n", request->length);

	ret = __dwc3_gadget_ep_queue(dep, req);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_ep_dequeue(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request		*req = to_dwc3_request(request);
	struct dwc3_request		*r = NULL;

	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;
	int				ret = 0;

	if (atomic_read(&dwc->in_lpm)) {
		dev_err(dwc->dev, "%s: Unable to dequeue while in LPM\n",
				dep->name);
		return -EAGAIN;
	}

	trace_dwc3_ep_dequeue(req);

	spin_lock_irqsave(&dwc->lock, flags);

	list_for_each_entry(r, &dep->request_list, list) {
		if (r == req)
			break;
	}

	if (r != req) {
		list_for_each_entry(r, &dep->req_queued, list) {
			if (r == req)
				break;
		}
		if (r == req) {
			/* wait until it is processed */
			dwc3_stop_active_transfer(dwc, dep->number, true);
			goto out1;
		}
		dev_err(dwc->dev, "request %p was not queued to %s\n",
				request, ep->name);
		ret = -EINVAL;
		goto out0;
	}

out1:
	dbg_event(dep->number, "DEQUEUE", 0);
	/* giveback the request */
	dwc3_gadget_giveback(dep, req, -ECONNRESET);

out0:
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

int __dwc3_gadget_ep_set_halt(struct dwc3_ep *dep, int value, int protocol)
{
	struct dwc3_gadget_ep_cmd_params	params;
	struct dwc3				*dwc = dep->dwc;
	int					ret;

	memset(&params, 0x00, sizeof(params));

	if (value) {
		if (!protocol && ((dep->direction && dep->flags & DWC3_EP_BUSY) ||
				(!list_empty(&dep->req_queued) ||
				 !list_empty(&dep->request_list)))) {
			dev_dbg(dwc->dev, "%s: pending request, cannot halt\n",
					dep->name);
			return -EAGAIN;
		}

		ret = dwc3_send_gadget_ep_cmd(dwc, dep->number,
			DWC3_DEPCMD_SETSTALL, &params);
		if (ret)
			dev_err(dwc->dev, "failed to set STALL on %s\n",
					dep->name);
		else
			dep->flags |= DWC3_EP_STALL;
	} else {
		ret = dwc3_send_gadget_ep_cmd(dwc, dep->number,
			DWC3_DEPCMD_CLEARSTALL, &params);
		if (ret)
			dev_err(dwc->dev, "failed to clear STALL on %s\n",
					dep->name);
		else
			dep->flags &= ~(DWC3_EP_STALL | DWC3_EP_WEDGE);
	}

	return ret;
}

static int dwc3_gadget_ep_set_halt(struct usb_ep *ep, int value)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;

	int				ret;

	if (!ep->desc) {
		dev_err(dwc->dev, "(%s)'s desc is NULL.\n", dep->name);
		return -EINVAL;
	}

	spin_lock_irqsave(&dwc->lock, flags);
	dbg_event(dep->number, "HALT", value);
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dev_err(dwc->dev, "%s is of Isochronous type\n", dep->name);
		ret = -EINVAL;
		goto out;
	}

	ret = __dwc3_gadget_ep_set_halt(dep, value, false);
out:
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_ep_set_wedge(struct usb_ep *ep)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;
	unsigned long			flags;
	int				ret;

	spin_lock_irqsave(&dwc->lock, flags);
	dbg_event(dep->number, "WEDGE", 0);
	dep->flags |= DWC3_EP_WEDGE;

	if (dep->number == 0 || dep->number == 1)
		ret = __dwc3_gadget_ep0_set_halt(ep, 1);
	else
		ret = __dwc3_gadget_ep_set_halt(dep, 1, false);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

/* -------------------------------------------------------------------------- */

static struct usb_endpoint_descriptor dwc3_gadget_ep0_desc = {
	.bLength	= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes	= USB_ENDPOINT_XFER_CONTROL,
};

static const struct usb_ep_ops dwc3_gadget_ep0_ops = {
	.enable		= dwc3_gadget_ep0_enable,
	.disable	= dwc3_gadget_ep0_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep0_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep0_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

static const struct usb_ep_ops dwc3_gadget_ep_ops = {
	.enable		= dwc3_gadget_ep_enable,
	.disable	= dwc3_gadget_ep_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

/* -------------------------------------------------------------------------- */

static int dwc3_gadget_get_frame(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	return DWC3_DSTS_SOFFN(reg);
}

#define DWC3_PM_RESUME_RETRIES		20    /* Max Number of retries */
#define DWC3_PM_RESUME_DELAY		100   /* 100 msec */

static void dwc3_gadget_wakeup_work(struct work_struct *w)
{
	struct dwc3		*dwc;
	int			ret;
	static int		retry_count;

	dwc = container_of(w, struct dwc3, wakeup_work);

	ret = pm_runtime_get_sync(dwc->dev);
	if (ret) {
		/* pm_runtime_get_sync returns -EACCES error between
		 * late_suspend and early_resume, wait for system resume to
		 * finish and queue work again
		 */
		pr_debug("PM runtime get sync failed, ret %d\n", ret);
		if (ret == -EACCES) {
			pm_runtime_put_noidle(dwc->dev);
			if (retry_count == DWC3_PM_RESUME_RETRIES) {
				retry_count = 0;
				pr_err("pm_runtime_get_sync timed out\n");
				return;
			}
			msleep(DWC3_PM_RESUME_DELAY);
			retry_count++;
			schedule_work(&dwc->wakeup_work);
			return;
		}
	}
	retry_count = 0;
	dbg_event(0xFF, "Gdgwake gsyn",
		atomic_read(&dwc->dev->power.usage_count));

	ret = dwc3_gadget_wakeup_int(dwc);

	if (ret)
		pr_err("Remote wakeup failed. ret = %d.\n", ret);
	else
		pr_debug("Remote wakeup succeeded.\n");

	pm_runtime_put_noidle(dwc->dev);
	dbg_event(0xFF, "Gdgwake put",
		atomic_read(&dwc->dev->power.usage_count));
}

static int dwc3_gadget_wakeup_int(struct dwc3 *dwc)
{
	bool			link_recover_only = false;

	u32			reg;
	int			ret = 0;
	u8			link_state;
	unsigned long		flags;

	pr_debug("%s(): Entry\n", __func__);
	disable_irq(dwc->irq);
	spin_lock_irqsave(&dwc->lock, flags);
	/*
	 * According to the Databook Remote wakeup request should
	 * be issued only when the device is in early suspend state.
	 *
	 * We can check that via USB Link State bits in DSTS register.
	 */
	link_state = dwc3_get_link_state(dwc);

	switch (link_state) {
	case DWC3_LINK_STATE_RX_DET:	/* in HS, means Early Suspend */
	case DWC3_LINK_STATE_U3:	/* in HS, means SUSPEND */
		break;
	case DWC3_LINK_STATE_U1:
		if (dwc->gadget.speed != USB_SPEED_SUPER) {
			link_recover_only = true;
			break;
		}
		/* Intentional fallthrough */
	default:
		dev_dbg(dwc->dev, "can't wakeup from link state %d\n",
				link_state);
		ret = -EINVAL;
		goto out;
	}

	/* Enable LINK STATUS change event */
	reg = dwc3_readl(dwc->regs, DWC3_DEVTEN);
	reg |= DWC3_DEVTEN_ULSTCNGEN;
	dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
	/*
	 * memory barrier is required to make sure that required events
	 * with core is enabled before performing RECOVERY mechnism.
	 */
	mb();

	ret = dwc3_gadget_set_link_state(dwc, DWC3_LINK_STATE_RECOV);
	if (ret < 0) {
		dev_err(dwc->dev, "failed to put link in Recovery\n");
		/* Disable LINK STATUS change */
		reg = dwc3_readl(dwc->regs, DWC3_DEVTEN);
		reg &= ~DWC3_DEVTEN_ULSTCNGEN;
		dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
		/* Required to complete this operation before returning */
		mb();
		goto out;
	}

	/* Recent versions do this automatically */
	if (dwc->revision < DWC3_REVISION_194A) {
		/* write zeroes to Link Change Request */
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}

	spin_unlock_irqrestore(&dwc->lock, flags);
	enable_irq(dwc->irq);

	/*
	 * Have bigger value (16 sec) for timeout since some host PCs driving
	 * resume for very long time (e.g. 8 sec)
	 */
	ret = wait_event_interruptible_timeout(dwc->wait_linkstate,
			(dwc->link_state < DWC3_LINK_STATE_U3) ||
			(dwc->link_state == DWC3_LINK_STATE_SS_DIS),
			msecs_to_jiffies(16000));

	spin_lock_irqsave(&dwc->lock, flags);
	/* Disable link status change event */
	reg = dwc3_readl(dwc->regs, DWC3_DEVTEN);
	reg &= ~DWC3_DEVTEN_ULSTCNGEN;
	dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
	/*
	 * Complete this write before we go ahead and perform resume
	 * as we don't need link status change notificaiton anymore.
	 */
	mb();

	if (!ret) {
		dev_dbg(dwc->dev, "Timeout moving into state(%d)\n",
							dwc->link_state);
		ret = -EINVAL;
		spin_unlock_irqrestore(&dwc->lock, flags);
		goto out1;
	} else {
		ret = 0;
		/*
		 * If USB is disconnected OR received RESET from host,
		 * don't perform resume
		 */
		if (dwc->link_state == DWC3_LINK_STATE_SS_DIS ||
				dwc->gadget.state == USB_STATE_DEFAULT)
			link_recover_only = true;
	}

	/*
	 * According to DWC3 databook, the controller does not
	 * trigger a wakeup event when remote-wakeup is used.
	 * Hence, after remote-wakeup sequence is complete, and
	 * the device is back at U0 state, it is required that
	 * the resume sequence is initiated by SW.
	 */
	if (!link_recover_only)
		dwc3_gadget_wakeup_interrupt(dwc, true);

	spin_unlock_irqrestore(&dwc->lock, flags);
	pr_debug("%s: Exit\n", __func__);
	return ret;

out:
	spin_unlock_irqrestore(&dwc->lock, flags);
	enable_irq(dwc->irq);

out1:
	return ret;
}

static int dwc_gadget_func_wakeup(struct usb_gadget *g, int interface_id)
{
	int ret = 0;
	struct dwc3 *dwc = gadget_to_dwc(g);

	if (!g || (g->speed != USB_SPEED_SUPER))
		return -ENOTSUPP;

	if (dwc3_gadget_is_suspended(dwc)) {
		pr_debug("USB bus is suspended. Scheduling wakeup and returning -EAGAIN.\n");
		dwc3_gadget_wakeup(&dwc->gadget);
		return -EAGAIN;
	}

	if (dwc->revision < DWC3_REVISION_220A) {
		ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_XMIT_FUNCTION, interface_id);
	} else {
		ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_XMIT_DEV, 0x1 | (interface_id << 4));
	}

	if (ret)
		pr_err("Function wakeup HW command failed.\n");
	else
		pr_debug("Function wakeup HW command succeeded.\n");

	return ret;
}

static int dwc3_gadget_set_selfpowered(struct usb_gadget *g,
		int is_selfpowered)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->is_selfpowered = !!is_selfpowered;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

#define DWC3_SOFT_RESET_TIMEOUT	10  /* 10 msec */
static int dwc3_gadget_run_stop(struct dwc3 *dwc, int is_on, int suspend)
{
	u32			reg;
	u32			timeout = 500;
	ktime_t start, diff;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	if (is_on) {
		if (dwc->revision <= DWC3_REVISION_187A) {
			reg &= ~DWC3_DCTL_TRGTULST_MASK;
			reg |= DWC3_DCTL_TRGTULST_RX_DET;
		}

		if (dwc->revision >= DWC3_REVISION_194A)
			reg &= ~DWC3_DCTL_KEEP_CONNECT;

		start = ktime_get();
		/* issue device SoftReset */
		dwc3_writel(dwc->regs, DWC3_DCTL, reg | DWC3_DCTL_CSFTRST);
		do {
			reg = dwc3_readl(dwc->regs, DWC3_DCTL);
			if (!(reg & DWC3_DCTL_CSFTRST))
				break;

			diff = ktime_sub(ktime_get(), start);
			/* poll for max. 10ms */
			if (ktime_to_ms(diff) > DWC3_SOFT_RESET_TIMEOUT) {
				printk_ratelimited(KERN_ERR
					"%s:core Reset Timed Out\n", __func__);
				break;
			}
			cpu_relax();
		} while (true);


		dwc3_event_buffers_setup(dwc);
		dwc3_gadget_restart(dwc);
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation)
			reg |= DWC3_DCTL_KEEP_CONNECT;

		dwc->pullups_connected = true;
	} else {
		dwc3_gadget_disable_irq(dwc);
		__dwc3_gadget_ep_disable(dwc->eps[0]);
		__dwc3_gadget_ep_disable(dwc->eps[1]);

		reg &= ~DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation && !suspend)
			reg &= ~DWC3_DCTL_KEEP_CONNECT;

		dwc->pullups_connected = false;
		usb_gadget_set_state(&dwc->gadget, USB_STATE_NOTATTACHED);
	}

	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	do {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);
		if (is_on) {
			if (!(reg & DWC3_DSTS_DEVCTRLHLT))
				break;
		} else {
			if (reg & DWC3_DSTS_DEVCTRLHLT)
				break;
		}
		timeout--;
		if (!timeout) {
			dev_err(dwc->dev, "failed to %s controller\n",
						is_on ? "start" : "stop");
			if (is_on)
				dbg_event(0xFF, "STARTTOUT", reg);
			else
				dbg_event(0xFF, "STOPTOUT", reg);
			return -ETIMEDOUT;
		}
		udelay(1);
	} while (1);

	dev_vdbg(dwc->dev, "gadget %s data soft-%s\n",
			dwc->gadget_driver
			? dwc->gadget_driver->function : "no-function",
			is_on ? "connect" : "disconnect");

	return 0;
}

static int dwc3_gadget_vbus_draw(struct usb_gadget *g, unsigned mA)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	dwc->vbus_draw = mA;
	dev_dbg(dwc->dev, "Notify controller from %s. mA = %d\n", __func__, mA);
	dwc3_notify_event(dwc, DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT, 0);
	return 0;
}

static int dwc3_gadget_pullup(struct usb_gadget *g, int is_on)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret;

	is_on = !!is_on;

	dwc->softconnect = is_on;

	if ((dwc->is_drd && !dwc->vbus_active) || !dwc->gadget_driver) {
		/*
		 * Need to wait for vbus_session(on) from otg driver or to
		 * the udc_start.
		 */
		return 0;
	}

	pm_runtime_get_sync(dwc->dev);
	dbg_event(0xFF, "Pullup gsync",
		atomic_read(&dwc->dev->power.usage_count));

	spin_lock_irqsave(&dwc->lock, flags);

	/*
	 * If we are here after bus suspend notify otg state machine to
	 * increment pm usage count of dwc to prevent pm_runtime_suspend
	 * during enumeration.
	 */
	dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT, 0);


	ret = dwc3_gadget_run_stop(dwc, is_on, false);

	spin_unlock_irqrestore(&dwc->lock, flags);

	pm_runtime_mark_last_busy(dwc->dev);
	pm_runtime_put_autosuspend(dwc->dev);
	dbg_event(0xFF, "Pullup put",
		atomic_read(&dwc->dev->power.usage_count));

	return ret;
}

void dwc3_gadget_enable_irq(struct dwc3 *dwc)
{
	u32			reg;

	/* Enable all but Start and End of Frame IRQs */
	reg = (DWC3_DEVTEN_VNDRDEVTSTRCVEDEN |
			DWC3_DEVTEN_EVNTOVERFLOWEN |
			DWC3_DEVTEN_CMDCMPLTEN |
			DWC3_DEVTEN_ERRTICERREN |
			DWC3_DEVTEN_WKUPEVTEN |
			DWC3_DEVTEN_CONNECTDONEEN |
			DWC3_DEVTEN_USBRSTEN |
			DWC3_DEVTEN_DISCONNEVTEN);

	/*
	 * Enable SUSPENDEVENT(BIT:6) for version 230A and above
	 * else enable USB Link change event (BIT:3) for older version
	 */
	if (dwc->revision < DWC3_REVISION_230A)
		reg |= DWC3_DEVTEN_ULSTCNGEN;
	else
		reg |= DWC3_DEVTEN_SUSPEND;

	dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
}

void dwc3_gadget_disable_irq(struct dwc3 *dwc)
{
	/* mask all interrupts */
	dwc3_writel(dwc->regs, DWC3_DEVTEN, 0x00);
}

static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc);
static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc);

static int dwc3_gadget_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct dwc3 *dwc = gadget_to_dwc(_gadget);
	unsigned long flags;

	if (!dwc->is_drd)
		return -EPERM;

	is_active = !!is_active;

	spin_lock_irqsave(&dwc->lock, flags);

	/* Mark that the vbus was powered */
	dwc->vbus_active = is_active;

	/*
	 * Check if upper level usb_gadget_driver was already registerd with
	 * this udc controller driver (if dwc3_gadget_start was called)
	 */
	if (dwc->gadget_driver && dwc->softconnect) {
		if (dwc->vbus_active) {
			/*
			 * Both vbus was activated by otg and pullup was
			 * signaled by the gadget driver.
			 */
			dwc3_gadget_run_stop(dwc, 1, false);
		} else {
			dwc3_gadget_run_stop(dwc, 0, false);
		}
	}

	/*
	 * Clearing run/stop bit might occur before disconnect event is seen.
	 * Make sure to let gadget driver know in that case.
	 */
	if (!dwc->vbus_active) {
		dev_dbg(dwc->dev, "calling disconnect from %s\n", __func__);
		dwc3_gadget_disconnect_interrupt(dwc);
	}

	spin_unlock_irqrestore(&dwc->lock, flags);
	return 0;
}

static int __dwc3_gadget_start(struct dwc3 *dwc)
{
	struct dwc3_ep		*dep;
	int			ret = 0;
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~(DWC3_DCFG_SPEED_MASK);

	/**
	 * WORKAROUND: DWC3 revision < 2.20a have an issue
	 * which would cause metastability state on Run/Stop
	 * bit if we try to force the IP to USB2-only mode.
	 *
	 * Because of that, we cannot configure the IP to any
	 * speed other than the SuperSpeed
	 *
	 * Refers to:
	 *
	 * STAR#9000525659: Clock Domain Crossing on DCTL in
	 * USB 2.0 Mode
	 */
	if (dwc->revision < DWC3_REVISION_220A) {
		reg |= DWC3_DCFG_SUPERSPEED;
	} else {
		switch (dwc->maximum_speed) {
		case USB_SPEED_LOW:
			reg |= DWC3_DSTS_LOWSPEED;
			break;
		case USB_SPEED_FULL:
			reg |= DWC3_DSTS_FULLSPEED1;
			break;
		case USB_SPEED_HIGH:
			reg |= DWC3_DSTS_HIGHSPEED;
			break;
		case USB_SPEED_SUPER:	/* FALLTHROUGH */
		case USB_SPEED_UNKNOWN:	/* FALTHROUGH */
		default:
			reg |= DWC3_DSTS_SUPERSPEED;
		}
	}
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);

	/* Programs the number of outstanding pipelined transfer requests
	 * the AXI master pushes to the AXI slave.
	 */
	if (dwc->revision >= DWC3_REVISION_270A) {
		reg = dwc3_readl(dwc->regs, DWC3_GSBUSCFG1);
		reg &= ~DWC3_GSBUSCFG1_PIPETRANSLIMIT_MASK;
		reg |= DWC3_GSBUSCFG1_PIPETRANSLIMIT(0xe);
		dwc3_writel(dwc->regs, DWC3_GSBUSCFG1, reg);
	}

	dwc->start_config_issued = false;

	/* Start with SuperSpeed Default */
	dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);

	dwc->delayed_status = false;
	/* reinitialize physical ep0-1 */
	dep = dwc->eps[0];
	dep->flags = 0;
	dep->endpoint.maxburst = 1;
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		return ret;
	}

	dep = dwc->eps[1];
	dep->flags = 0;
	dep->endpoint.maxburst = 1;
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		__dwc3_gadget_ep_disable(dwc->eps[0]);
		return ret;
	}

	/* begin to receive SETUP packets */
	dwc->ep0state = EP0_SETUP_PHASE;
	dwc3_ep0_out_start(dwc);

	dwc3_gadget_enable_irq(dwc);

	return ret;
}

/* Required gadget re-initialization before switching to gadget in OTG mode */
void dwc3_gadget_restart(struct dwc3 *dwc)
{
	__dwc3_gadget_start(dwc);
}

static int dwc3_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret = 0;

	g->interrupt_num = dwc->irq;
	spin_lock_irqsave(&dwc->lock, flags);

	if (dwc->gadget_driver) {
		dev_err(dwc->dev, "%s is already bound to %s\n",
				dwc->gadget.name,
				dwc->gadget_driver->driver.name);
		ret = -EBUSY;
		goto err0;
	}

	dwc->gadget_driver	= driver;

	/*
	 * For DRD, this might get called by gadget driver during bootup
	 * even though host mode might be active. Don't actually perform
	 * device-specific initialization until device mode is activated.
	 * In that case dwc3_gadget_restart() will handle it.
	 */
	spin_unlock_irqrestore(&dwc->lock, flags);
	return 0;

err0:
	spin_unlock_irqrestore(&dwc->lock, flags);
	return ret;
}

static int dwc3_gadget_stop(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			irq;

	dwc3_gadget_disable_irq(dwc);

	tasklet_kill(&dwc->bh);

	spin_lock_irqsave(&dwc->lock, flags);

	__dwc3_gadget_ep_disable(dwc->eps[0]);
	__dwc3_gadget_ep_disable(dwc->eps[1]);

	dwc->gadget_driver	= NULL;

	spin_unlock_irqrestore(&dwc->lock, flags);

	irq = platform_get_irq(to_platform_device(dwc->dev), 0);
	free_irq(irq, dwc);

	return 0;
}

static int dwc3_gadget_restart_usb_session(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	return dwc3_notify_event(dwc, DWC3_CONTROLLER_RESTART_USB_SESSION, 0);
}

static const struct usb_gadget_ops dwc3_gadget_ops = {
	.get_frame		= dwc3_gadget_get_frame,
	.wakeup			= dwc3_gadget_wakeup,
	.func_wakeup		= dwc_gadget_func_wakeup,
	.set_selfpowered	= dwc3_gadget_set_selfpowered,
	.vbus_session		= dwc3_gadget_vbus_session,
	.vbus_draw		= dwc3_gadget_vbus_draw,
	.pullup			= dwc3_gadget_pullup,
	.udc_start		= dwc3_gadget_start,
	.udc_stop		= dwc3_gadget_stop,
	.restart		= dwc3_gadget_restart_usb_session,
};

/* -------------------------------------------------------------------------- */

#define NUM_GSI_OUT_EPS	1
#define NUM_GSI_IN_EPS	2

static int dwc3_gadget_init_hw_endpoints(struct dwc3 *dwc,
		u8 num, u32 direction)
{
	struct dwc3_ep			*dep;
	u8				i, gsi_ep_count, gsi_ep_index = 0;

	/* Read number of event buffers to check if we need
	 * to update gsi_ep_count. For non GSI targets this
	 * will be 0 and we will skip reservation of GSI eps.
	 * There is one event buffer for each GSI EP.
	 */
	gsi_ep_count = dwc->num_gsi_event_buffers;
	/* OUT GSI EPs based on direction field */
	if (gsi_ep_count && !direction)
		gsi_ep_count = NUM_GSI_OUT_EPS;
	/* IN GSI EPs */
	else if (gsi_ep_count && direction)
		gsi_ep_count = NUM_GSI_IN_EPS;

	for (i = 0; i < num; i++) {
		u8 epnum = (i << 1) | (!!direction);

		dep = kzalloc(sizeof(*dep), GFP_KERNEL);
		if (!dep)
			return -ENOMEM;

		dep->dwc = dwc;
		dep->number = epnum;
		dep->direction = !!direction;
		dwc->eps[epnum] = dep;

		/* Reserve EPs at the end for GSI based on gsi_ep_count */
		if ((gsi_ep_index < gsi_ep_count) &&
				(i > (num - 1 - gsi_ep_count))) {
			gsi_ep_index++;
			/* For GSI EPs, name eps as "gsi-epin" or "gsi-epout" */
			snprintf(dep->name, sizeof(dep->name), "%s",
				(epnum & 1) ? "gsi-epin" : "gsi-epout");
			/* Set ep type as GSI */
			dep->endpoint.ep_type = EP_TYPE_GSI;
		} else {
			snprintf(dep->name, sizeof(dep->name), "ep%d%s",
				epnum >> 1, (epnum & 1) ? "in" : "out");
		}

		dep->endpoint.ep_num = epnum >> 1;
		dep->endpoint.name = dep->name;

		dev_vdbg(dwc->dev, "initializing %s %d\n",
				dep->name, epnum >> 1);

		if (epnum == 0 || epnum == 1) {
			usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
			dep->endpoint.maxburst = 1;
			dep->endpoint.ops = &dwc3_gadget_ep0_ops;
			if (!epnum)
				dwc->gadget.ep0 = &dep->endpoint;
		} else {
			usb_ep_set_maxpacket_limit(&dep->endpoint, 1024);
			dep->endpoint.max_streams = 15;
			dep->endpoint.ops = &dwc3_gadget_ep_ops;
			list_add_tail(&dep->endpoint.ep_list,
					&dwc->gadget.ep_list);
		}

		INIT_LIST_HEAD(&dep->request_list);
		INIT_LIST_HEAD(&dep->req_queued);
	}

	return 0;
}

static int dwc3_gadget_init_endpoints(struct dwc3 *dwc)
{
	int				ret;

	INIT_LIST_HEAD(&dwc->gadget.ep_list);

	ret = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_out_eps, 0);
	if (ret < 0) {
		dev_vdbg(dwc->dev, "failed to allocate OUT endpoints\n");
		return ret;
	}

	ret = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_in_eps, 1);
	if (ret < 0) {
		dev_vdbg(dwc->dev, "failed to allocate IN endpoints\n");
		return ret;
	}

	return 0;
}

static void dwc3_gadget_free_endpoints(struct dwc3 *dwc)
{
	struct dwc3_ep			*dep;
	u8				epnum;

	for (epnum = 0; epnum < DWC3_ENDPOINTS_NUM; epnum++) {
		dep = dwc->eps[epnum];
		if (!dep)
			continue;
		/*
		 * Physical endpoints 0 and 1 are special; they form the
		 * bi-directional USB endpoint 0.
		 *
		 * For those two physical endpoints, we don't allocate a TRB
		 * pool nor do we add them the endpoints list. Due to that, we
		 * shouldn't do these two operations otherwise we would end up
		 * with all sorts of bugs when removing dwc3.ko.
		 */
		if (epnum != 0 && epnum != 1) {
			if (dep->trb_pool)
				dwc3_free_trb_pool(dep);
			list_del(&dep->endpoint.ep_list);
		}

		kfree(dep);
	}
}

/* -------------------------------------------------------------------------- */

static int __dwc3_cleanup_done_trbs(struct dwc3 *dwc, struct dwc3_ep *dep,
		struct dwc3_request *req, struct dwc3_trb *trb, unsigned length,
		const struct dwc3_event_depevt *event, int status)
{
	unsigned int		count;
	unsigned int		s_pkt = 0;
	unsigned int		trb_status;

	trace_dwc3_complete_trb(dep, trb);

	if ((trb->ctrl & DWC3_TRB_CTRL_HWO) && status != -ESHUTDOWN)
		/*
		 * We continue despite the error. There is not much we
		 * can do. If we don't clean it up we loop forever. If
		 * we skip the TRB then it gets overwritten after a
		 * while since we use them in a ring buffer. A BUG()
		 * would help. Lets hope that if this occurs, someone
		 * fixes the root cause instead of looking away :)
		 */
		dev_err(dwc->dev, "%s's TRB (%p) still owned by HW\n",
				dep->name, trb);
	count = trb->size & DWC3_TRB_SIZE_MASK;

	if (dep->direction) {
		if (count) {
			trb_status = DWC3_TRB_SIZE_TRBSTS(trb->size);
			if (trb_status == DWC3_TRBSTS_MISSED_ISOC) {
				dev_dbg(dwc->dev, "incomplete IN transfer %s\n",
						dep->name);
				/*
				 * If missed isoc occurred and there is
				 * no request queued then issue END
				 * TRANSFER, so that core generates
				 * next xfernotready and we will issue
				 * a fresh START TRANSFER.
				 * If there are still queued request
				 * then wait, do not issue either END
				 * or UPDATE TRANSFER, just attach next
				 * request in request_list during
				 * giveback.If any future queued request
				 * is successfully transferred then we
				 * will issue UPDATE TRANSFER for all
				 * request in the request_list.
				 */
				dep->flags |= DWC3_EP_MISSED_ISOC;
				dbg_event(dep->number, "MISSED ISOC", status);
			} else {
				dev_err(dwc->dev, "incomplete IN transfer %s\n",
						dep->name);
				status = -ECONNRESET;
			}
		} else {
			dep->flags &= ~DWC3_EP_MISSED_ISOC;
		}
	} else {
		if (count && (event->status & DEPEVT_STATUS_SHORT))
			s_pkt = 1;
	}

	/*
	 * We assume here we will always receive the entire data block
	 * which we should receive. Meaning, if we program RX to
	 * receive 4K but we receive only 2K, we assume that's all we
	 * should receive and we simply bounce the request back to the
	 * gadget driver for further processing.
	 */
	req->request.actual += length - count;
	if (s_pkt)
		return 1;
	if ((event->status & DEPEVT_STATUS_LST) &&
			(trb->ctrl & (DWC3_TRB_CTRL_LST |
				DWC3_TRB_CTRL_HWO)))
		return 1;
	if ((event->status & DEPEVT_STATUS_IOC) &&
			(trb->ctrl & DWC3_TRB_CTRL_IOC))
		return 1;
	return 0;
}

static int dwc3_cleanup_done_reqs(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event, int status)
{
	struct dwc3_request	*req;
	struct dwc3_trb		*trb;
	unsigned int		slot;
	unsigned int		i;
	unsigned int		trb_len;
	int			ret;

	do {
		req = next_request(&dep->req_queued);
		if (!req) {
			dev_err(dwc->dev, "%s: evt sts %x for no req queued",
				dep->name, event->status);
			return 1;
		}

		/* Make sure that not to queue any TRB if HWO bit is set. */
		if (req->trb->ctrl & DWC3_TRB_CTRL_HWO)
			return 0;

		i = 0;
		do {
			slot = req->start_slot + i;
			if ((slot == DWC3_TRB_NUM - 1) &&
				usb_endpoint_xfer_isoc(dep->endpoint.desc))
				slot++;
			slot %= DWC3_TRB_NUM;
			trb = &dep->trb_pool[slot];

			if (req->request.num_mapped_sgs)
				trb_len = sg_dma_len(&req->request.sg[i]);
			else
				trb_len = req->request.length;

			ret = __dwc3_cleanup_done_trbs(dwc, dep, req, trb,
					trb_len, event, status);
			if (ret)
				break;
		}while (++i < req->request.num_mapped_sgs);

		if (req->ztrb) {
			trb = req->ztrb;
			if ((event->status & DEPEVT_STATUS_LST) &&
				(trb->ctrl & (DWC3_TRB_CTRL_LST |
					DWC3_TRB_CTRL_HWO)))
				ret = 1;

			if ((event->status & DEPEVT_STATUS_IOC) &&
					(trb->ctrl & DWC3_TRB_CTRL_IOC))
				ret = 1;
		}
		dwc3_gadget_giveback(dep, req, status);

		/* EP possibly disabled during giveback? */
		if (!(dep->flags & DWC3_EP_ENABLED)) {
			dev_dbg(dwc->dev, "%s disabled while handling ep event\n",
					dep->name);
			return 0;
		}

		if (ret)
			break;
	} while (1);

	dwc->gadget.xfer_isr_count++;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
			list_empty(&dep->req_queued)) {
		if (list_empty(&dep->request_list))
			/*
			 * If there is no entry in request list then do
			 * not issue END TRANSFER now. Just set PENDING
			 * flag, so that END TRANSFER is issued when an
			 * entry is added into request list.
			 */
			dep->flags |= DWC3_EP_PENDING_REQUEST;
		else
			dwc3_stop_active_transfer(dwc, dep->number, true);
		dep->flags &= ~DWC3_EP_MISSED_ISOC;
		return 1;
	}

	if ((event->status & DEPEVT_STATUS_IOC) &&
			(trb->ctrl & DWC3_TRB_CTRL_IOC))
		return 0;
	return 1;
}

static void dwc3_endpoint_transfer_complete(struct dwc3 *dwc,
		struct dwc3_ep *dep, const struct dwc3_event_depevt *event)
{
	unsigned		status = 0;
	int			clean_busy;
	u32			is_xfer_complete;

	is_xfer_complete = (event->endpoint_event == DWC3_DEPEVT_XFERCOMPLETE);

	if (event->status & DEPEVT_STATUS_BUSERR)
		status = -ECONNRESET;

	clean_busy = dwc3_cleanup_done_reqs(dwc, dep, event, status);
	if (clean_busy && (is_xfer_complete ||
				usb_endpoint_xfer_isoc(dep->endpoint.desc)))
		dep->flags &= ~DWC3_EP_BUSY;

	/*
	 * WORKAROUND: This is the 2nd half of U1/U2 -> U0 workaround.
	 * See dwc3_gadget_linksts_change_interrupt() for 1st half.
	 */
	if (dwc->revision < DWC3_REVISION_183A) {
		u32		reg;
		int		i;

		for (i = 0; i < DWC3_ENDPOINTS_NUM; i++) {
			dep = dwc->eps[i];

			if (!(dep->flags & DWC3_EP_ENABLED))
				continue;

			if (!list_empty(&dep->req_queued))
				goto isoc_workaround;
		}

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= (dwc->u1u2 & ~(dwc->u1));
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);

		dwc->u1u2 = 0;
	}

isoc_workaround:
	/*
	 * WORKAROUND: This is the 2nd half ISOC packet drop workaround.
	 * See __dwc3_gadget_start_isoc() for 1st half.
	 */
	if (dwc->revision <= DWC3_REVISION_250A) {
		u32		reg;
		int		i;

		for (i = 0; i < DWC3_ENDPOINTS_NUM; i++) {
			dep = dwc->eps[i];

			if (!(dep->flags & DWC3_EP_ENABLED) ||
				!usb_endpoint_xfer_isoc(dep->endpoint.desc))
				continue;

			if (!list_empty(&dep->req_queued))
				return;
		}

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= dwc->u1;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);

		dwc->u1 = 0;
	}
}

static void dwc3_endpoint_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_depevt *event)
{
	struct dwc3_ep		*dep;
	u8			epnum = event->endpoint_number;

	dep = dwc->eps[epnum];

	if (!(dep->flags & DWC3_EP_ENABLED))
		return;

	if (epnum == 0 || epnum == 1) {
		dwc3_ep0_interrupt(dwc, event);
		return;
	}

	dep->dbg_ep_events.total++;

	switch (event->endpoint_event) {
	case DWC3_DEPEVT_XFERCOMPLETE:
		dep->resource_index = 0;
		dep->dbg_ep_events.xfercomplete++;

		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dev_dbg(dwc->dev, "%s is an Isochronous endpoint\n",
					dep->name);
			return;
		}

		dwc3_endpoint_transfer_complete(dwc, dep, event);
		break;
	case DWC3_DEPEVT_XFERINPROGRESS:
		dep->dbg_ep_events.xferinprogress++;
		if (!usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dev_dbg(dwc->dev, "%s is not an Isochronous endpoint\n",
					dep->name);
			return;
		}

		dwc3_endpoint_transfer_complete(dwc, dep, event);
		break;
	case DWC3_DEPEVT_XFERNOTREADY:
		dep->dbg_ep_events.xfernotready++;
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dwc3_gadget_start_isoc(dwc, dep, event);
		} else {
			int ret;

			dev_vdbg(dwc->dev, "%s: reason %s\n",
					dep->name, event->status &
					DEPEVT_STATUS_TRANSFER_ACTIVE
					? "Transfer Active"
					: "Transfer Not Active");

			/*
			 * If XFERNOTREADY interrupt is received with event
			 * status as TRANSFER ACTIVE, don't kick next transfer.
			 * otherwise data stall is seen on that endpoint.
			 */
			if (event->status & DEPEVT_STATUS_TRANSFER_ACTIVE)
				return;

			ret = __dwc3_gadget_kick_transfer(dep, 0, 1);
			if (!ret || ret == -EBUSY)
				return;

			dev_dbg(dwc->dev, "%s: failed to kick transfers\n",
					dep->name);
		}

		break;
	case DWC3_DEPEVT_STREAMEVT:
		dep->dbg_ep_events.streamevent++;
		if (!usb_endpoint_xfer_bulk(dep->endpoint.desc)) {
			dev_err(dwc->dev, "Stream event for non-Bulk %s\n",
					dep->name);
			return;
		}

		switch (event->status) {
		case DEPEVT_STREAMEVT_FOUND:
			dev_vdbg(dwc->dev, "Stream %d found and started\n",
					event->parameters);

			break;
		case DEPEVT_STREAMEVT_NOTFOUND:
			/* FALLTHROUGH */
		default:
			dev_dbg(dwc->dev, "Couldn't find suitable stream\n");
		}
		break;
	case DWC3_DEPEVT_RXTXFIFOEVT:
		dev_dbg(dwc->dev, "%s FIFO Overrun\n", dep->name);
		dep->dbg_ep_events.rxtxfifoevent++;
		break;
	case DWC3_DEPEVT_EPCMDCMPLT:
		dev_vdbg(dwc->dev, "Endpoint Command Complete\n");
		dep->dbg_ep_events.epcmdcomplete++;
		break;
	}
}

static void dwc3_disconnect_gadget(struct dwc3 *dwc)
{
	if (dwc->gadget_driver && dwc->gadget_driver->disconnect) {
		spin_unlock(&dwc->lock);
		dwc->gadget_driver->disconnect(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
	dwc->gadget.xfer_isr_count = 0;
}

static void dwc3_suspend_gadget(struct dwc3 *dwc)
{
	if (dwc->gadget_driver && dwc->gadget_driver->suspend) {
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "SUSPEND", 0);
		dwc->gadget_driver->suspend(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_resume_gadget(struct dwc3 *dwc)
{
	if (dwc->gadget_driver && dwc->gadget_driver->resume) {
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "RESUME", 0);
		dwc->gadget_driver->resume(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

void dwc3_stop_active_transfer(struct dwc3 *dwc, u32 epnum, bool force)
{
	struct dwc3_ep *dep;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd;
	int ret;

	dep = dwc->eps[epnum];

	if (!dep->resource_index)
		return;

	if (dep->endpoint.endless)
		dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_DISABLE_UPDXFER,
								dep->number);

	/*
	 * NOTICE: We are violating what the Databook says about the
	 * EndTransfer command. Ideally we would _always_ wait for the
	 * EndTransfer Command Completion IRQ, but that's causing too
	 * much trouble synchronizing between us and gadget driver.
	 *
	 * We have discussed this with the IP Provider and it was
	 * suggested to giveback all requests here, but give HW some
	 * extra time to synchronize with the interconnect. We're using
	 * an arbitraty 100us delay for that.
	 *
	 * Note also that a similar handling was tested by Synopsys
	 * (thanks a lot Paul) and nothing bad has come out of it.
	 * In short, what we're doing is:
	 *
	 * - Issue EndTransfer WITH CMDIOC bit set
	 * - Wait 100us
	 */

	cmd = DWC3_DEPCMD_ENDTRANSFER;
	cmd |= force ? DWC3_DEPCMD_HIPRI_FORCERM : 0;
	cmd |= DWC3_DEPCMD_CMDIOC;
	cmd |= DWC3_DEPCMD_PARAM(dep->resource_index);
	memset(&params, 0, sizeof(params));
	ret = dwc3_send_gadget_ep_cmd(dwc, dep->number, cmd, &params);
	WARN_ON_ONCE(ret);
	dep->resource_index = 0;
	dep->flags &= ~DWC3_EP_BUSY;
	udelay(100);
}

static void dwc3_stop_active_transfers(struct dwc3 *dwc)
{
	u32 epnum;

	for (epnum = 2; epnum < DWC3_ENDPOINTS_NUM; epnum++) {
		struct dwc3_ep *dep;

		dep = dwc->eps[epnum];
		if (!dep)
			continue;

		if (!(dep->flags & DWC3_EP_ENABLED))
			continue;

		dwc3_remove_requests(dwc, dep);
	}
}

static void dwc3_clear_stall_all_ep(struct dwc3 *dwc)
{
	u32 epnum;

	for (epnum = 1; epnum < DWC3_ENDPOINTS_NUM; epnum++) {
		struct dwc3_ep *dep;
		struct dwc3_gadget_ep_cmd_params params;
		int ret;

		dep = dwc->eps[epnum];
		if (!dep)
			continue;

		if (!(dep->flags & DWC3_EP_STALL))
			continue;

		dep->flags &= ~DWC3_EP_STALL;

		memset(&params, 0, sizeof(params));
		ret = dwc3_send_gadget_ep_cmd(dwc, dep->number,
				DWC3_DEPCMD_CLEARSTALL, &params);
		if (ret) {
			dev_dbg(dwc->dev, "%s; send ep cmd CLEARSTALL failed",
				dep->name);
			dbg_event(dep->number, "ECLRSTALL", ret);
		}
	}
}

static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc)
{
	int			reg;

	dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT, 0);

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_INITU1ENA;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	reg &= ~DWC3_DCTL_INITU2ENA;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	dbg_event(0xFF, "DISCONNECT", 0);
	dwc3_disconnect_gadget(dwc);
	dwc->start_config_issued = false;

	dwc->gadget.speed = USB_SPEED_UNKNOWN;
	dwc->setup_packet_pending = false;
	dwc->link_state = DWC3_LINK_STATE_SS_DIS;
	wake_up_interruptible(&dwc->wait_linkstate);
}

void dwc3_gadget_usb3_phy_suspend(struct dwc3 *dwc, int suspend)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));

	if (suspend)
		reg |= DWC3_GUSB3PIPECTL_SUSPHY;
	else
		reg &= ~DWC3_GUSB3PIPECTL_SUSPHY;

	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);
}

static void dwc3_gadget_reset_interrupt(struct dwc3 *dwc)
{
	u32			reg;

	/*
	 * WORKAROUND: DWC3 revisions <1.88a have an issue which
	 * would cause a missing Disconnect Event if there's a
	 * pending Setup Packet in the FIFO.
	 *
	 * There's no suggested workaround on the official Bug
	 * report, which states that "unless the driver/application
	 * is doing any special handling of a disconnect event,
	 * there is no functional issue".
	 *
	 * Unfortunately, it turns out that we _do_ some special
	 * handling of a disconnect event, namely complete all
	 * pending transfers, notify gadget driver of the
	 * disconnection, and so on.
	 *
	 * Our suggested workaround is to follow the Disconnect
	 * Event steps here, instead, based on a setup_packet_pending
	 * flag. Such flag gets set whenever we have a XferNotReady
	 * event on EP0 and gets cleared on XferComplete for the
	 * same endpoint.
	 *
	 * Refers to:
	 *
	 * STAR#9000466709: RTL: Device : Disconnect event not
	 * generated if setup packet pending in FIFO
	 */
	if (dwc->revision < DWC3_REVISION_188A) {
		if (dwc->setup_packet_pending)
			dwc3_gadget_disconnect_interrupt(dwc);
	}

	dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT, 0);

	dbg_event(0xFF, "BUS RST", 0);
	/* after reset -> Default State */
	usb_gadget_set_state(&dwc->gadget, USB_STATE_DEFAULT);

	dwc3_gadget_usb3_phy_suspend(dwc, false);

	usb_gadget_vbus_draw(&dwc->gadget, 0);

	if (dwc->gadget.speed != USB_SPEED_UNKNOWN)
		dwc3_disconnect_gadget(dwc);

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	dwc->test_mode = false;

	dwc3_stop_active_transfers(dwc);
	dwc3_clear_stall_all_ep(dwc);
	dwc->start_config_issued = false;

	/* bus reset issued due to missing status stage of a control transfer */
	dwc->resize_fifos = 0;

	/* Reset device address to zero */
	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~(DWC3_DCFG_DEVADDR_MASK);
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
	dwc->gadget.speed = USB_SPEED_UNKNOWN;
	dwc->link_state = DWC3_LINK_STATE_U0;
	wake_up_interruptible(&dwc->wait_linkstate);
}

static void dwc3_update_ram_clk_sel(struct dwc3 *dwc, u32 speed)
{
	u32 reg;
	u32 usb30_clock = DWC3_GCTL_CLK_BUS;

	/*
	 * We change the clock only at SS but I dunno why I would want to do
	 * this. Maybe it becomes part of the power saving plan.
	 */

	if (speed != DWC3_DSTS_SUPERSPEED)
		return;

	/*
	 * RAMClkSel is reset to 0 after USB reset, so it must be reprogrammed
	 * each time on Connect Done.
	 */
	if (!usb30_clock)
		return;

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg |= DWC3_GCTL_RAMCLKSEL(usb30_clock);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

static void dwc3_gadget_conndone_interrupt(struct dwc3 *dwc)
{
	struct dwc3_ep		*dep;
	int			ret;
	u32			reg;
	u8			speed;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	speed = reg & DWC3_DSTS_CONNECTSPD;
	dwc->speed = speed;
	dwc->gadget.l1_supported = true;

	dwc3_update_ram_clk_sel(dwc, speed);

	switch (speed) {
	case DWC3_DCFG_SUPERSPEED:
		/*
		 * WORKAROUND: DWC3 revisions <1.90a have an issue which
		 * would cause a missing USB3 Reset event.
		 *
		 * In such situations, we should force a USB3 Reset
		 * event by calling our dwc3_gadget_reset_interrupt()
		 * routine.
		 *
		 * Refers to:
		 *
		 * STAR#9000483510: RTL: SS : USB3 reset event may
		 * not be generated always when the link enters poll
		 */
		if (dwc->revision < DWC3_REVISION_190A)
			dwc3_gadget_reset_interrupt(dwc);

		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);
		dwc->gadget.ep0->maxpacket = 512;
		dwc->gadget.speed = USB_SPEED_SUPER;
		break;
	case DWC3_DCFG_HIGHSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget.ep0->maxpacket = 64;
		dwc->gadget.l1_supported = false;
		dwc->gadget.speed = USB_SPEED_HIGH;
		break;
	case DWC3_DCFG_FULLSPEED2:
	case DWC3_DCFG_FULLSPEED1:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget.ep0->maxpacket = 64;
		dwc->gadget.speed = USB_SPEED_FULL;
		break;
	case DWC3_DCFG_LOWSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(8);
		dwc->gadget.ep0->maxpacket = 8;
		dwc->gadget.speed = USB_SPEED_LOW;
		break;
	}

	/* Enable USB2 LPM Capability */

	if ((dwc->revision > DWC3_REVISION_194A)
			&& (speed != DWC3_DCFG_SUPERSPEED)) {
		reg = dwc3_readl(dwc->regs, DWC3_DCFG);
		reg |= DWC3_DCFG_LPM_CAP;
		dwc3_writel(dwc->regs, DWC3_DCFG, reg);

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~(DWC3_DCTL_HIRD_THRES_MASK | DWC3_DCTL_L1_HIBER_EN);

		reg |= DWC3_DCTL_HIRD_THRES(dwc->hird_threshold);

		/*
		 * When dwc3 revisions >= 2.40a, LPM Erratum is enabled and
		 * DCFG.LPMCap is set, core responses with an ACK and the
		 * BESL value in the LPM token is less than or equal to LPM
		 * NYET threshold.
		 */
		WARN_ONCE(dwc->revision < DWC3_REVISION_240A
				&& dwc->has_lpm_erratum,
				"LPM Erratum not available on dwc3 revisisions < 2.40a\n");

		if (dwc->has_lpm_erratum && dwc->revision >= DWC3_REVISION_240A)
			reg |= DWC3_DCTL_LPM_ERRATA(dwc->lpm_nyet_threshold);

		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	} else {
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~DWC3_DCTL_HIRD_THRES_MASK;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}

	/*
	 * In HS mode this allows SS phy suspend. In SS mode this allows ss phy
	 * suspend in P3 state and generates IN_P3 power event irq.
	 */
	dwc3_gadget_usb3_phy_suspend(dwc, true);

	dep = dwc->eps[0];
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, true,
			false);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		return;
	}

	dep = dwc->eps[1];
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, true,
			false);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		return;
	}

	dwc3_notify_event(dwc, DWC3_CONTROLLER_CONNDONE_EVENT, 0);

	/*
	 * Configure PHY via GUSB3PIPECTLn if required.
	 *
	 * Update GTXFIFOSIZn
	 *
	 * In both cases reset values should be sufficient.
	 */
}

static void dwc3_gadget_wakeup_interrupt(struct dwc3 *dwc, bool remote_wakeup)
{
	bool perform_resume = true;

	dev_dbg(dwc->dev, "%s\n", __func__);

	/*
	 * Identify if it is called from wakeup_interrupt() context for bus
	 * resume or as part of remote wakeup. And based on that check for
	 * U3 state. as we need to handle case of L1 resume i.e. where we
	 * don't want to perform resume.
	 */
	if (!remote_wakeup && dwc->link_state != DWC3_LINK_STATE_U3)
		perform_resume = false;

	/* Only perform resume from L2 or Early Suspend states */
	if (perform_resume) {
		dbg_event(0xFF, "WAKEUP", 0);

		/*
		 * In case of remote wake up dwc3_gadget_wakeup_work()
		 * is doing pm_runtime_get_sync().
		 */
		dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
		dwc->b_suspend = false;
		dwc3_notify_event(dwc,
				DWC3_CONTROLLER_NOTIFY_OTG_EVENT, 0);

		/*
		 * set state to U0 as function level resume is trying to queue
		 * notification over USB interrupt endpoint which would fail
		 * due to state is not being updated.
		 */
		dwc->link_state = DWC3_LINK_STATE_U0;
		dwc3_resume_gadget(dwc);
		return;
	}

	dwc->link_state = DWC3_LINK_STATE_U0;
}

static void dwc3_gadget_linksts_change_interrupt(struct dwc3 *dwc,
		unsigned int evtinfo)
{
	enum dwc3_link_state	next = evtinfo & DWC3_LINK_STATE_MASK;
	unsigned int		pwropt;

	/*
	 * WORKAROUND: DWC3 <= 2.50a have an issue when configured without
	 * Hibernation mode enabled which would show up when device detects
	 * host-initiated U3 exit.
	 *
	 * In that case, device will generate a Link State Change Interrupt
	 * from U3 to RESUME which is only necessary if Hibernation is
	 * configured in.
	 *
	 * There are no functional changes due to such spurious event and we
	 * just need to ignore it.
	 *
	 * Refers to:
	 *
	 * STAR#9000570034 RTL: SS Resume event generated in non-Hibernation
	 * operational mode
	 */
	pwropt = DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1);
	if ((dwc->revision <= DWC3_REVISION_250A) &&
			(pwropt != DWC3_GHWPARAMS1_EN_PWROPT_HIB)) {
		if ((dwc->link_state == DWC3_LINK_STATE_U3) &&
				(next == DWC3_LINK_STATE_RESUME)) {
			dev_vdbg(dwc->dev, "ignoring transition U3 -> Resume\n");
			return;
		}
	}

	/*
	 * WORKAROUND: DWC3 Revisions <1.83a have an issue which, depending
	 * on the link partner, the USB session might do multiple entry/exit
	 * of low power states before a transfer takes place.
	 *
	 * Due to this problem, we might experience lower throughput. The
	 * suggested workaround is to disable DCTL[12:9] bits if we're
	 * transitioning from U1/U2 to U0 and enable those bits again
	 * after a transfer completes and there are no pending transfers
	 * on any of the enabled endpoints.
	 *
	 * This is the first half of that workaround.
	 *
	 * Refers to:
	 *
	 * STAR#9000446952: RTL: Device SS : if U1/U2 ->U0 takes >128us
	 * core send LGO_Ux entering U0
	 */
	if (dwc->revision < DWC3_REVISION_183A) {
		if (next == DWC3_LINK_STATE_U0) {
			u32	u1u2;
			u32	reg;

			switch (dwc->link_state) {
			case DWC3_LINK_STATE_U1:
			case DWC3_LINK_STATE_U2:
				reg = dwc3_readl(dwc->regs, DWC3_DCTL);
				u1u2 = reg & (DWC3_DCTL_INITU2ENA
						| DWC3_DCTL_ACCEPTU2ENA
						| DWC3_DCTL_INITU1ENA
						| DWC3_DCTL_ACCEPTU1ENA);

				if (!dwc->u1u2)
					dwc->u1u2 = reg & u1u2;

				reg &= ~u1u2;

				dwc3_writel(dwc->regs, DWC3_DCTL, reg);
				break;
			default:
				/* do nothing */
				break;
			}
		}
	}

	switch (next) {
	case DWC3_LINK_STATE_U1:
		if (dwc->speed == USB_SPEED_SUPER)
			dwc3_suspend_gadget(dwc);
		break;
	case DWC3_LINK_STATE_U2:
	case DWC3_LINK_STATE_U3:
		dwc3_suspend_gadget(dwc);
		break;
	case DWC3_LINK_STATE_RESUME:
		dwc3_resume_gadget(dwc);
		break;
	default:
		/* do nothing */
		break;
	}

	dev_dbg(dwc->dev, "Going from (%d)--->(%d)\n", dwc->link_state, next);
	dwc->link_state = next;
	wake_up_interruptible(&dwc->wait_linkstate);
}

static void dwc3_gadget_hibernation_interrupt(struct dwc3 *dwc,
		unsigned int evtinfo)
{
	unsigned int is_ss = evtinfo & BIT(4);

	/**
	 * WORKAROUND: DWC3 revison 2.20a with hibernation support
	 * have a known issue which can cause USB CV TD.9.23 to fail
	 * randomly.
	 *
	 * Because of this issue, core could generate bogus hibernation
	 * events which SW needs to ignore.
	 *
	 * Refers to:
	 *
	 * STAR#9000546576: Device Mode Hibernation: Issue in USB 2.0
	 * Device Fallback from SuperSpeed
	 */
	if (is_ss ^ (dwc->speed == USB_SPEED_SUPER))
		return;

	/* enter hibernation here */
}

static void dwc3_gadget_suspend_interrupt(struct dwc3 *dwc,
			unsigned int evtinfo)
{
	enum dwc3_link_state    next = evtinfo & DWC3_LINK_STATE_MASK;

	dev_dbg(dwc->dev, "%s Entry\n", __func__);

	if (dwc->link_state != next && next == DWC3_LINK_STATE_U3) {
		/*
		 * When first connecting the cable, even before the initial
		 * DWC3_DEVICE_EVENT_RESET or DWC3_DEVICE_EVENT_CONNECT_DONE
		 * events, the controller sees a DWC3_DEVICE_EVENT_SUSPEND
		 * event. In such a case, ignore.
		 * Ignore suspend event until device side usb is not into
		 * CONFIGURED state.
		 */
		if (dwc->gadget.state != USB_STATE_CONFIGURED) {
			pr_err("%s(): state:%d. Ignore SUSPEND.\n",
						__func__, dwc->gadget.state);
			return;
		}

		dwc3_suspend_gadget(dwc);

		dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
		dwc->b_suspend = true;
		dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT, 0);
	}

	dwc->link_state = next;
	dev_vdbg(dwc->dev, "%s link %d\n", __func__, dwc->link_state);
}

static void dwc3_dump_reg_info(struct dwc3 *dwc)
{
	dbg_event(0xFF, "REGDUMP", 0);

	dbg_print_reg("GUSB3PIPCTL", dwc3_readl(dwc->regs,
							DWC3_GUSB3PIPECTL(0)));
	dbg_print_reg("GUSB2PHYCONFIG", dwc3_readl(dwc->regs,
							DWC3_GUSB2PHYCFG(0)));
	dbg_print_reg("GCTL", dwc3_readl(dwc->regs, DWC3_GCTL));
	dbg_print_reg("GUCTL", dwc3_readl(dwc->regs, DWC3_GUCTL));
	dbg_print_reg("GDBGLTSSM", dwc3_readl(dwc->regs, DWC3_GDBGLTSSM));
	dbg_print_reg("DCFG", dwc3_readl(dwc->regs, DWC3_DCFG));
	dbg_print_reg("DCTL", dwc3_readl(dwc->regs, DWC3_DCTL));
	dbg_print_reg("DEVTEN", dwc3_readl(dwc->regs, DWC3_DEVTEN));
	dbg_print_reg("DSTS", dwc3_readl(dwc->regs, DWC3_DSTS));
	dbg_print_reg("DALPENA", dwc3_readl(dwc->regs, DWC3_DALEPENA));
	dbg_print_reg("DGCMD", dwc3_readl(dwc->regs, DWC3_DGCMD));

	dbg_print_reg("OCFG", dwc3_readl(dwc->regs, DWC3_OCFG));
	dbg_print_reg("OCTL", dwc3_readl(dwc->regs, DWC3_OCTL));
	dbg_print_reg("OEVT", dwc3_readl(dwc->regs, DWC3_OEVT));
	dbg_print_reg("OSTS", dwc3_readl(dwc->regs, DWC3_OSTS));
}

static void dwc3_gadget_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_devt *event)
{
	switch (event->type) {
	case DWC3_DEVICE_EVENT_DISCONNECT:
		dwc3_gadget_disconnect_interrupt(dwc);
		dwc->dbg_gadget_events.disconnect++;
		break;
	case DWC3_DEVICE_EVENT_RESET:
		dwc3_gadget_reset_interrupt(dwc);
		dwc->dbg_gadget_events.reset++;
		break;
	case DWC3_DEVICE_EVENT_CONNECT_DONE:
		dwc3_gadget_conndone_interrupt(dwc);
		dwc->dbg_gadget_events.connect++;
		break;
	case DWC3_DEVICE_EVENT_WAKEUP:
		dwc3_gadget_wakeup_interrupt(dwc, false);
		dwc->dbg_gadget_events.wakeup++;
		break;
	case DWC3_DEVICE_EVENT_HIBER_REQ:
		if (dev_WARN_ONCE(dwc->dev, !dwc->has_hibernation,
					"unexpected hibernation event\n"))
			break;

		dwc3_gadget_hibernation_interrupt(dwc, event->event_info);
		break;
	case DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE:
		dwc3_gadget_linksts_change_interrupt(dwc, event->event_info);
		dwc->dbg_gadget_events.link_status_change++;
		break;
	case DWC3_DEVICE_EVENT_SUSPEND:
		if (dwc->revision < DWC3_REVISION_230A) {
			dev_vdbg(dwc->dev, "End of Periodic Frame\n");
			dwc->dbg_gadget_events.eopf++;
		} else {
			dev_vdbg(dwc->dev, "U3/L1-L2 Suspend Event\n");
			dbg_event(0xFF, "GAD SUS", 0);
			dwc->dbg_gadget_events.suspend++;

			/*
			 * Ignore suspend event if usb cable is not connected
			 * and speed is not being detected.
			 */
			if (dwc->gadget.speed != USB_SPEED_UNKNOWN &&
				dwc->vbus_active)
					dwc3_gadget_suspend_interrupt(dwc,
							event->event_info);
		}
		break;
	case DWC3_DEVICE_EVENT_SOF:
		dev_vdbg(dwc->dev, "Start of Periodic Frame\n");
		dwc->dbg_gadget_events.sof++;
		break;
	case DWC3_DEVICE_EVENT_ERRATIC_ERROR:
		if (!dwc->err_evt_seen) {
			dbg_event(0xFF, "ERROR", 0);
			dev_vdbg(dwc->dev, "Erratic Error\n");
			dwc3_dump_reg_info(dwc);
		}
		dwc->dbg_gadget_events.erratic_error++;
		break;
	case DWC3_DEVICE_EVENT_CMD_CMPL:
		dev_vdbg(dwc->dev, "Command Complete\n");
		dwc->dbg_gadget_events.cmdcmplt++;
		break;
	case DWC3_DEVICE_EVENT_OVERFLOW:
		dbg_event(0xFF, "OVERFL", 0);
		dev_vdbg(dwc->dev, "Overflow\n");
		dwc->dbg_gadget_events.overflow++;
		break;
	default:
		dev_dbg(dwc->dev, "UNKNOWN IRQ %d\n", event->type);
		dwc->dbg_gadget_events.unknown_event++;
	}

	dwc->err_evt_seen = (event->type == DWC3_DEVICE_EVENT_ERRATIC_ERROR);
}

static void dwc3_process_event_entry(struct dwc3 *dwc,
		const union dwc3_event *event)
{
	trace_dwc3_event(event->raw);

	/* skip event processing in absence of vbus */
	if (!dwc->vbus_active) {
		dbg_print_reg("SKIP EVT", event->raw);
		return;
	}

	/* If run/stop is cleared don't process any more events */
	if (!dwc->pullups_connected) {
		dbg_print_reg("SKIP_EVT_PULLUP", event->raw);
		return;
	}

	/* Endpoint IRQ, handle it and return early */
	if (event->type.is_devspec == 0) {
		/* depevt */
		return dwc3_endpoint_interrupt(dwc, &event->depevt);
	}

	switch (event->type.type) {
	case DWC3_EVENT_TYPE_DEV:
		dwc3_gadget_interrupt(dwc, &event->devt);
		break;
	/* REVISIT what to do with Carkit and I2C events ? */
	default:
		dev_err(dwc->dev, "UNKNOWN IRQ type %d\n", event->raw);
	}
}

static irqreturn_t dwc3_process_event_buf(struct dwc3 *dwc, u32 buf)
{
	struct dwc3_event_buffer *evt;
	irqreturn_t ret = IRQ_NONE;
	int left;
	u32 reg;

	evt = dwc->ev_buffs[buf];
	left = evt->count;

	if (!(evt->flags & DWC3_EVENT_PENDING))
		return IRQ_NONE;

	while (left > 0) {
		union dwc3_event event;

		event.raw = *(u32 *) (evt->buf + evt->lpos);

		dwc3_process_event_entry(dwc, &event);

		if (dwc->err_evt_seen) {
			/*
			 * if erratic error, skip remaining events
			 * while controller undergoes reset
			 */
			evt->lpos = (evt->lpos + left) %
					DWC3_EVENT_BUFFERS_SIZE;
			dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(buf), left);
			if (dwc3_notify_event(dwc,
						DWC3_CONTROLLER_ERROR_EVENT, 0))
				dwc->err_evt_seen = 0;
			break;
		}

		/*
		 * FIXME we wrap around correctly to the next entry as
		 * almost all entries are 4 bytes in size. There is one
		 * entry which has 12 bytes which is a regular entry
		 * followed by 8 bytes data. ATM I don't know how
		 * things are organized if we get next to the a
		 * boundary so I worry about that once we try to handle
		 * that.
		 */
		evt->lpos = (evt->lpos + 4) % DWC3_EVENT_BUFFERS_SIZE;
		left -= 4;

		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(buf), 4);
	}

	dwc->bh_handled_evt_cnt[dwc->bh_dbg_index] += (evt->count / 4);

	evt->count = 0;
	evt->flags &= ~DWC3_EVENT_PENDING;
	ret = IRQ_HANDLED;

	/* Unmask interrupt */
	reg = dwc3_readl(dwc->regs, DWC3_GEVNTSIZ(buf));
	reg &= ~DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(buf), reg);

	return ret;
}

static void dwc3_interrupt_bh(unsigned long param)
{
	struct dwc3 *dwc = (struct dwc3 *) param;

	dwc3_thread_interrupt(dwc->irq, dwc);
	enable_irq(dwc->irq);
}

static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc)
{
	struct dwc3 *dwc = _dwc;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	int i;
	unsigned temp_time;
	ktime_t start_time;

	start_time = ktime_get();

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->bh_handled_evt_cnt[dwc->bh_dbg_index] = 0;

	for (i = 0; i < dwc->num_normal_event_buffers; i++)
		ret |= dwc3_process_event_buf(dwc, i);

	spin_unlock_irqrestore(&dwc->lock, flags);

	temp_time = ktime_to_us(ktime_sub(ktime_get(), start_time));
	dwc->bh_completion_time[dwc->bh_dbg_index] = temp_time;
	dwc->bh_dbg_index = (dwc->bh_dbg_index + 1) % 10;

	return ret;
}

static irqreturn_t dwc3_check_event_buf(struct dwc3 *dwc, u32 buf)
{
	struct dwc3_event_buffer *evt;
	u32 count;
	u32 reg;

	evt = dwc->ev_buffs[buf];

	count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(buf));
	count &= DWC3_GEVNTCOUNT_MASK;
	if (!count)
		return IRQ_NONE;

	if (count > evt->length) {
		dbg_event(0xFF, "HUGE_EVCNT", count);
		evt->lpos = (evt->lpos + count) % DWC3_EVENT_BUFFERS_SIZE;
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(buf), count);
		return IRQ_HANDLED;
	}

	evt->count = count;
	evt->flags |= DWC3_EVENT_PENDING;

	/* Mask interrupt */
	reg = dwc3_readl(dwc->regs, DWC3_GEVNTSIZ(buf));
	reg |= DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(buf), reg);

	return IRQ_WAKE_THREAD;
}

irqreturn_t dwc3_interrupt(int irq, void *_dwc)
{
	struct dwc3			*dwc = _dwc;
	int				i;
	irqreturn_t			ret = IRQ_NONE;
	unsigned			temp_cnt = 0;
	ktime_t				start_time;

	start_time = ktime_get();
	spin_lock(&dwc->lock);

	dwc->irq_cnt++;

	if (dwc->err_evt_seen) {
		/* controller reset is still pending */
		spin_unlock(&dwc->lock);
		return IRQ_HANDLED;
	}

	for (i = 0; i < dwc->num_normal_event_buffers; i++) {
		irqreturn_t status;

		status = dwc3_check_event_buf(dwc, i);
		if (status == IRQ_WAKE_THREAD)
			ret = status;

		temp_cnt += dwc->ev_buffs[i]->count;
	}

	spin_unlock(&dwc->lock);

	dwc->irq_start_time[dwc->irq_dbg_index] = start_time;
	dwc->irq_completion_time[dwc->irq_dbg_index] =
		ktime_us_delta(ktime_get(), start_time);
	dwc->irq_event_count[dwc->irq_dbg_index] = temp_cnt / 4;
	dwc->irq_dbg_index = (dwc->irq_dbg_index + 1) % MAX_INTR_STATS;

	if (ret == IRQ_WAKE_THREAD) {
		disable_irq_nosync(irq);
		tasklet_schedule(&dwc->bh);
	}

	return IRQ_HANDLED;
}

/**
 * dwc3_gadget_init - Initializes gadget related registers
 * @dwc: pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_gadget_init(struct dwc3 *dwc)
{
	int					ret;

	INIT_WORK(&dwc->wakeup_work, dwc3_gadget_wakeup_work);

	dwc->ctrl_req = dma_alloc_coherent(dwc->dev, sizeof(*dwc->ctrl_req),
			&dwc->ctrl_req_addr, GFP_KERNEL);
	if (!dwc->ctrl_req) {
		dev_err(dwc->dev, "failed to allocate ctrl request\n");
		ret = -ENOMEM;
		goto err0;
	}

	dwc->ep0_trb = dma_alloc_coherent(dwc->dev, sizeof(*dwc->ep0_trb),
			&dwc->ep0_trb_addr, GFP_KERNEL);
	if (!dwc->ep0_trb) {
		dev_err(dwc->dev, "failed to allocate ep0 trb\n");
		ret = -ENOMEM;
		goto err1;
	}

	dwc->setup_buf = kzalloc(DWC3_EP0_BOUNCE_SIZE, GFP_KERNEL);
	if (!dwc->setup_buf) {
		ret = -ENOMEM;
		goto err2;
	}

	dwc->ep0_bounce = dma_alloc_coherent(dwc->dev,
			DWC3_EP0_BOUNCE_SIZE, &dwc->ep0_bounce_addr,
			GFP_KERNEL);
	if (!dwc->ep0_bounce) {
		dev_err(dwc->dev, "failed to allocate ep0 bounce buffer\n");
		ret = -ENOMEM;
		goto err3;
	}

	dwc->bh.func = dwc3_interrupt_bh;
	dwc->bh.data = (unsigned long)dwc;

	dwc->gadget.ops			= &dwc3_gadget_ops;
	dwc->gadget.max_speed		= USB_SPEED_SUPER;
	dwc->gadget.speed		= USB_SPEED_UNKNOWN;
	dwc->gadget.sg_supported	= true;
	dwc->gadget.name		= "dwc3-gadget";

	/*
	 * Per databook, DWC3 needs buffer size to be aligned to MaxPacketSize
	 * on ep out.
	 */
	dwc->gadget.quirk_ep_out_aligned_size = true;

	/*
	 * REVISIT: Here we should clear all pending IRQs to be
	 * sure we're starting from a well known location.
	 */

	ret = dwc3_gadget_init_endpoints(dwc);
	if (ret)
		goto err4;

	ret = usb_add_gadget_udc(dwc->dev, &dwc->gadget);
	if (ret) {
		dev_err(dwc->dev, "failed to register udc\n");
		goto err4;
	}

	if (!dwc->is_drd) {
		pm_runtime_no_callbacks(&dwc->gadget.dev);
		pm_runtime_set_active(&dwc->gadget.dev);
		pm_runtime_enable(&dwc->gadget.dev);
		pm_runtime_get(&dwc->gadget.dev);
	}

	return 0;

err4:
	dwc3_gadget_free_endpoints(dwc);
	dma_free_coherent(dwc->dev, DWC3_EP0_BOUNCE_SIZE,
			dwc->ep0_bounce, dwc->ep0_bounce_addr);

err3:
	kfree(dwc->setup_buf);

err2:
	dma_free_coherent(dwc->dev, sizeof(*dwc->ep0_trb),
			dwc->ep0_trb, dwc->ep0_trb_addr);

err1:
	dma_free_coherent(dwc->dev, sizeof(*dwc->ctrl_req),
			dwc->ctrl_req, dwc->ctrl_req_addr);

err0:
	return ret;
}

/* -------------------------------------------------------------------------- */

void dwc3_gadget_exit(struct dwc3 *dwc)
{
	if (dwc->is_drd) {
		pm_runtime_put(&dwc->gadget.dev);
		pm_runtime_disable(&dwc->gadget.dev);
	}

	usb_del_gadget_udc(&dwc->gadget);

	dwc3_gadget_free_endpoints(dwc);

	dma_free_coherent(dwc->dev, DWC3_EP0_BOUNCE_SIZE,
			dwc->ep0_bounce, dwc->ep0_bounce_addr);

	kfree(dwc->setup_buf);

	dma_free_coherent(dwc->dev, sizeof(*dwc->ep0_trb),
			dwc->ep0_trb, dwc->ep0_trb_addr);

	dma_free_coherent(dwc->dev, sizeof(*dwc->ctrl_req),
			dwc->ctrl_req, dwc->ctrl_req_addr);
}

int dwc3_gadget_prepare(struct dwc3 *dwc)
{
	if (dwc->pullups_connected) {
		dwc3_gadget_disable_irq(dwc);
		dwc3_gadget_run_stop(dwc, true, true);
	}

	return 0;
}

void dwc3_gadget_complete(struct dwc3 *dwc)
{
	if (dwc->pullups_connected) {
		dwc3_gadget_enable_irq(dwc);
		dwc3_gadget_run_stop(dwc, true, false);
	}
}

int dwc3_gadget_suspend(struct dwc3 *dwc)
{
	__dwc3_gadget_ep_disable(dwc->eps[0]);
	__dwc3_gadget_ep_disable(dwc->eps[1]);

	dwc->dcfg = dwc3_readl(dwc->regs, DWC3_DCFG);

	return 0;
}

int dwc3_gadget_resume(struct dwc3 *dwc)
{
	struct dwc3_ep		*dep;
	int			ret;

	/* Start with SuperSpeed Default */
	dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);

	dep = dwc->eps[0];
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret)
		goto err0;

	dep = dwc->eps[1];
	ret = __dwc3_gadget_ep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret)
		goto err1;

	/* begin to receive SETUP packets */
	dwc->ep0state = EP0_SETUP_PHASE;
	dwc3_ep0_out_start(dwc);

	dwc3_writel(dwc->regs, DWC3_DCFG, dwc->dcfg);

	return 0;

err1:
	__dwc3_gadget_ep_disable(dwc->eps[0]);

err0:
	return ret;
}
