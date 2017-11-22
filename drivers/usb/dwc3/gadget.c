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

	dwc3_trace(trace_dwc3_gadget,
			"link state change request timed out");

	return -ETIMEDOUT;
}

/**
 * dwc3_ep_inc_trb() - Increment a TRB index.
 * @index - Pointer to the TRB index to increment.
 *
 * The index should never point to the link TRB. After incrementing,
 * if it is point to the link TRB, wrap around to the beginning. The
 * link TRB is always at the last TRB entry.
 */
static void dwc3_ep_inc_trb(u8 *index)
{
	(*index)++;
	if (*index == (DWC3_TRB_NUM - 1))
		*index = 0;
}

void dwc3_ep_inc_enq(struct dwc3_ep *dep)
{
	dwc3_ep_inc_trb(&dep->trb_enqueue);
}

void dwc3_ep_inc_deq(struct dwc3_ep *dep)
{
	dwc3_ep_inc_trb(&dep->trb_dequeue);
}

/*
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
int dwc3_gadget_resize_tx_fifos(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	int		fifo_size, mdwidth, max_packet = 1024;
	int		tmp, mult = 1;

	if (!dwc->needs_fifo_resize)
		return 0;

	/* resize IN endpoints excepts ep0 */
	if (!usb_endpoint_dir_in(dep->endpoint.desc) ||
			dep->endpoint.ep_num == 0)
		return 0;

	/* Don't resize already resized IN endpoint */
	if (dep->fifo_depth) {
		dev_dbg(dwc->dev, "%s fifo_depth:%d is already set\n",
				dep->endpoint.name, dep->fifo_depth);
		return 0;
	}

	mdwidth = DWC3_MDWIDTH(dwc->hwparams.hwparams0);
	/* MDWIDTH is represented in bits, we need it in bytes */
	mdwidth >>= 3;

	if (dep->endpoint.ep_type == EP_TYPE_GSI || dep->endpoint.endless)
		mult = 3;

	if (((dep->endpoint.maxburst > 1) &&
			usb_endpoint_xfer_bulk(dep->endpoint.desc))
			|| usb_endpoint_xfer_isoc(dep->endpoint.desc))
		mult = 3;

	tmp = ((max_packet + mdwidth) * mult) + mdwidth;
	fifo_size = DIV_ROUND_UP(tmp, mdwidth);
	dep->fifo_depth = fifo_size;
	fifo_size |= (dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(0)) & 0xffff0000)
						+ (dwc->last_fifo_depth << 16);
	dwc->last_fifo_depth += (fifo_size & 0xffff);

	dev_dbg(dwc->dev, "%s ep_num:%d last_fifo_depth:%04x fifo_depth:%d\n",
		dep->endpoint.name, dep->endpoint.ep_num, dwc->last_fifo_depth,
		dep->fifo_depth);

	dbg_event(0xFF, "resize_fifo", dep->number);
	dbg_event(0xFF, "fifo_depth", dep->fifo_depth);
	/* Check fifo size allocation doesn't exceed available RAM size. */
	if (dwc->tx_fifo_size &&
		((dwc->last_fifo_depth * mdwidth) >= dwc->tx_fifo_size)) {
		dev_err(dwc->dev, "Fifosize(%d) > RAM size(%d) %s depth:%d\n",
			(dwc->last_fifo_depth * mdwidth), dwc->tx_fifo_size,
			dep->endpoint.name, fifo_size);
		dwc->last_fifo_depth -= (fifo_size & 0xffff);
		dep->fifo_depth = 0;
		WARN_ON(1);
		return -ENOMEM;
	}

	dwc3_writel(dwc->regs, DWC3_GTXFIFOSIZ(dep->endpoint.ep_num),
							fifo_size);
	return 0;
}

void dwc3_gadget_giveback(struct dwc3_ep *dep, struct dwc3_request *req,
		int status)
{
	struct dwc3			*dwc = dep->dwc;
	unsigned int			unmap_after_complete = false;

	req->started = false;
	list_del(&req->list);
	req->trb = NULL;

	if (req->request.status == -EINPROGRESS)
		req->request.status = status;

	/*
	 * NOTICE we don't want to unmap before calling ->complete() if we're
	 * dealing with a bounced ep0 request. If we unmap it here, we would end
	 * up overwritting the contents of req->buf and this could confuse the
	 * gadget driver.
	 */
	if (dwc->ep0_bounced && dep->number <= 1) {
		dwc->ep0_bounced = false;
		unmap_after_complete = true;
	} else {
		usb_gadget_unmap_request_by_dev(dwc->sysdev,
				&req->request, req->direction);
	}

	trace_dwc3_gadget_giveback(req);

	spin_unlock(&dwc->lock);
	usb_gadget_giveback_request(&dep->endpoint, &req->request);
	spin_lock(&dwc->lock);

	if (unmap_after_complete)
		usb_gadget_unmap_request_by_dev(dwc->sysdev,
				&req->request, req->direction);
}

int dwc3_send_gadget_generic_command(struct dwc3 *dwc, unsigned cmd, u32 param)
{
	u32		timeout = 500;
	int		status = 0;
	int		ret = 0;
	u32		reg;

	dwc3_writel(dwc->regs, DWC3_DGCMDPAR, param);
	dwc3_writel(dwc->regs, DWC3_DGCMD, cmd | DWC3_DGCMD_CMDACT);

	do {
		reg = dwc3_readl(dwc->regs, DWC3_DGCMD);
		if (!(reg & DWC3_DGCMD_CMDACT)) {
			status = DWC3_DGCMD_STATUS(reg);
			if (status)
				ret = -EINVAL;
			break;
		}
	} while (timeout--);

	if (!timeout) {
		ret = -ETIMEDOUT;
		status = -ETIMEDOUT;
	}

	trace_dwc3_gadget_generic_cmd(cmd, param, status);

	return ret;
}

static int __dwc3_gadget_wakeup(struct dwc3 *dwc);

int dwc3_send_gadget_ep_cmd(struct dwc3_ep *dep, unsigned cmd,
		struct dwc3_gadget_ep_cmd_params *params)
{
	struct dwc3		*dwc = dep->dwc;
	u32			timeout = 3000;
	u32			reg;

	int			cmd_status = 0;
	int			susphy = false;
	int			ret = -EINVAL;

	/*
	 * Synopsys Databook 2.60a states, on section 6.3.2.5.[1-8], that if
	 * we're issuing an endpoint command, we must check if
	 * GUSB2PHYCFG.SUSPHY bit is set. If it is, then we need to clear it.
	 *
	 * We will also set SUSPHY bit to what it was before returning as stated
	 * by the same section on Synopsys databook.
	 */
	if (dwc->gadget.speed <= USB_SPEED_HIGH) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		if (unlikely(reg & DWC3_GUSB2PHYCFG_SUSPHY)) {
			susphy = true;
			reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;
			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
		}
	}

	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_STARTTRANSFER) {
		int		needs_wakeup;

		needs_wakeup = (dwc->link_state == DWC3_LINK_STATE_U1 ||
				dwc->link_state == DWC3_LINK_STATE_U2 ||
				dwc->link_state == DWC3_LINK_STATE_U3);

		if (unlikely(needs_wakeup)) {
			ret = __dwc3_gadget_wakeup(dwc);
			dev_WARN_ONCE(dwc->dev, ret, "wakeup failed --> %d\n",
					ret);
		}
	}

	dwc3_writel(dep->regs, DWC3_DEPCMDPAR0, params->param0);
	dwc3_writel(dep->regs, DWC3_DEPCMDPAR1, params->param1);
	dwc3_writel(dep->regs, DWC3_DEPCMDPAR2, params->param2);

	dwc3_writel(dep->regs, DWC3_DEPCMD, cmd | DWC3_DEPCMD_CMDACT);
	do {
		reg = dwc3_readl(dep->regs, DWC3_DEPCMD);
		if (!(reg & DWC3_DEPCMD_CMDACT)) {
			cmd_status = DWC3_DEPCMD_STATUS(reg);

			switch (cmd_status) {
			case 0:
				ret = 0;
				break;
			case DEPEVT_TRANSFER_NO_RESOURCE:
				ret = -EINVAL;
				break;
			case DEPEVT_TRANSFER_BUS_EXPIRY:
				/*
				 * SW issues START TRANSFER command to
				 * isochronous ep with future frame interval. If
				 * future interval time has already passed when
				 * core receives the command, it will respond
				 * with an error status of 'Bus Expiry'.
				 *
				 * Instead of always returning -EINVAL, let's
				 * give a hint to the gadget driver that this is
				 * the case by returning -EAGAIN.
				 */
				ret = -EAGAIN;
				break;
			default:
				dev_WARN(dwc->dev, "UNKNOWN cmd status\n");
			}

			break;
		}
	} while (--timeout);

	if (timeout == 0) {
		ret = -ETIMEDOUT;
		dwc3_trace(trace_dwc3_gadget, "Command Timed Out");
		dev_err(dwc->dev, "%s command timeout for %s\n",
			dwc3_gadget_ep_cmd_string(cmd), dep->name);
		if (!(cmd & DWC3_DEPCMD_ENDTRANSFER)) {
			dwc->ep_cmd_timeout_cnt++;
			dwc3_notify_event(dwc,
				DWC3_CONTROLLER_RESTART_USB_SESSION);
		}
		cmd_status = -ETIMEDOUT;
	}

	trace_dwc3_gadget_ep_cmd(dep, cmd, params, cmd_status);

	if (ret == 0) {
		switch (DWC3_DEPCMD_CMD(cmd)) {
		case DWC3_DEPCMD_STARTTRANSFER:
			dep->flags |= DWC3_EP_TRANSFER_STARTED;
			break;
		case DWC3_DEPCMD_ENDTRANSFER:
			dep->flags &= ~DWC3_EP_TRANSFER_STARTED;
			break;
		default:
			/* nothing */
			break;
		}
	}

	if (unlikely(susphy)) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg |= DWC3_GUSB2PHYCFG_SUSPHY;
		dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
	}

	return ret;
}

static int dwc3_send_clear_stall_ep_cmd(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd = DWC3_DEPCMD_CLEARSTALL;

	/*
	 * As of core revision 2.60a the recommended programming model
	 * is to set the ClearPendIN bit when issuing a Clear Stall EP
	 * command for IN endpoints. This is to prevent an issue where
	 * some (non-compliant) hosts may not send ACK TPs for pending
	 * IN transfers due to a mishandled error condition. Synopsys
	 * STAR 9000614252.
	 */
	if (dep->direction && (dwc->revision >= DWC3_REVISION_260A) &&
	    (dwc->gadget.speed >= USB_SPEED_SUPER))
		cmd |= DWC3_DEPCMD_CLEARPENDIN;

	memset(&params, 0, sizeof(params));

	return dwc3_send_gadget_ep_cmd(dep, cmd, &params);
}

static int dwc3_alloc_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;
	u32			num_trbs = DWC3_TRB_NUM;

	if (dep->trb_pool)
		return 0;

	dep->trb_pool = dma_zalloc_coherent(dwc->sysdev,
			sizeof(struct dwc3_trb) * num_trbs,
			&dep->trb_pool_dma, GFP_KERNEL);
	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "failed to allocate trb pool for %s\n",
				dep->name);
		return -ENOMEM;
	}
	dep->num_trbs = num_trbs;

	return 0;
}

static void dwc3_free_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;

	/* Freeing of GSI EP TRBs are handled by GSI EP ops. */
	if (dep->endpoint.ep_type == EP_TYPE_GSI)
		return;

	/*
	 * Clean up ep ring to avoid getting xferInProgress due to stale trbs
	 * with HWO bit set from previous composition when update transfer cmd
	 * is issued.
	 */
	if (dep->number > 1 && dep->trb_pool && dep->trb_pool_dma) {
		memset(&dep->trb_pool[0], 0,
			sizeof(struct dwc3_trb) * dep->num_trbs);
		dbg_event(dep->number, "Clr_TRB", 0);
		dev_dbg(dwc->dev, "Clr_TRB ring of %s\n", dep->name);

		dma_free_coherent(dwc->sysdev,
				sizeof(struct dwc3_trb) * DWC3_TRB_NUM,
				dep->trb_pool, dep->trb_pool_dma);
		dep->trb_pool = NULL;
		dep->trb_pool_dma = 0;
	}
}

static int dwc3_gadget_set_xfer_resource(struct dwc3 *dwc, struct dwc3_ep *dep);

/**
 * dwc3_gadget_start_config - Configure EP resources
 * @dwc: pointer to our controller context structure
 * @dep: endpoint that is being enabled
 *
 * The assignment of transfer resources cannot perfectly follow the
 * data book due to the fact that the controller driver does not have
 * all knowledge of the configuration in advance. It is given this
 * information piecemeal by the composite gadget framework after every
 * SET_CONFIGURATION and SET_INTERFACE. Trying to follow the databook
 * programming model in this scenario can cause errors. For two
 * reasons:
 *
 * 1) The databook says to do DEPSTARTCFG for every SET_CONFIGURATION
 * and SET_INTERFACE (8.1.5). This is incorrect in the scenario of
 * multiple interfaces.
 *
 * 2) The databook does not mention doing more DEPXFERCFG for new
 * endpoint on alt setting (8.1.6).
 *
 * The following simplified method is used instead:
 *
 * All hardware endpoints can be assigned a transfer resource and this
 * setting will stay persistent until either a core reset or
 * hibernation. So whenever we do a DEPSTARTCFG(0) we can go ahead and
 * do DEPXFERCFG for every hardware endpoint as well. We are
 * guaranteed that there are as many transfer resources as endpoints.
 *
 * This function is called for each endpoint when it is being enabled
 * but is triggered only when called for EP0-out, which always happens
 * first, and which should only happen in one of the above conditions.
 */
static int dwc3_gadget_start_config(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;
	u32			cmd;
	int			i;
	int			ret;

	if (dep->number)
		return 0;

	memset(&params, 0x00, sizeof(params));
	cmd = DWC3_DEPCMD_DEPSTARTCFG;

	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret)
		return ret;

	for (i = 0; i < DWC3_ENDPOINTS_NUM; i++) {
		struct dwc3_ep *dep = dwc->eps[i];

		if (!dep)
			continue;

		ret = dwc3_gadget_set_xfer_resource(dwc, dep);
		if (ret)
			return ret;
	}

	return 0;
}

static int dwc3_gadget_set_ep_config(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct usb_endpoint_descriptor *desc,
		const struct usb_ss_ep_comp_descriptor *comp_desc,
		bool modify, bool restore)
{
	struct dwc3_gadget_ep_cmd_params params;

	if (dev_WARN_ONCE(dwc->dev, modify && restore,
					"Can't modify and restore\n"))
		return -EINVAL;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* Burst size is only needed in SuperSpeed mode */
	if (dwc->gadget.speed >= USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst;
		params.param0 |= DWC3_DEPCFG_BURST_SIZE(burst - 1);
	}

	if (modify) {
		params.param0 |= DWC3_DEPCFG_ACTION_MODIFY;
	} else if (restore) {
		params.param0 |= DWC3_DEPCFG_ACTION_RESTORE;
		params.param2 |= dep->saved_state;
	} else {
		params.param0 |= DWC3_DEPCFG_ACTION_INIT;
	}

	if (usb_endpoint_xfer_control(desc))
		params.param1 = DWC3_DEPCFG_XFER_COMPLETE_EN;

	if (dep->number <= 1 || usb_endpoint_xfer_isoc(desc))
		params.param1 |= DWC3_DEPCFG_XFER_NOT_READY_EN;

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params.param1 |= DWC3_DEPCFG_STREAM_CAPABLE
			| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	if (!usb_endpoint_xfer_control(desc))
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

	return dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETEPCONFIG, &params);
}

static int dwc3_gadget_set_xfer_resource(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPXFERCFG_NUM_XFER_RES(1);

	return dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETTRANSFRESOURCE,
			&params);
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
		bool modify, bool restore)
{
	struct dwc3		*dwc = dep->dwc;
	u32			reg;
	int			ret;

	dwc3_trace(trace_dwc3_gadget, "Enabling %s", dep->name);

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		dep->endpoint.desc = desc;
		dep->comp_desc = comp_desc;
		dep->type = usb_endpoint_type(desc);
		ret = dwc3_gadget_resize_tx_fifos(dwc, dep);
		if (ret) {
			dep->endpoint.desc = NULL;
			dep->comp_desc = NULL;
			dep->type = 0;
			return ret;
		}

		ret = dwc3_gadget_start_config(dwc, dep);
		if (ret) {
			dev_err(dwc->dev, "start_config() failed for %s\n",
								dep->name);
			return ret;
		}
	}

	ret = dwc3_gadget_set_ep_config(dwc, dep, desc, comp_desc, modify,
			restore);
	if (ret) {
		dev_err(dwc->dev, "set_ep_config() failed for %s\n", dep->name);
		return ret;
	}

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		struct dwc3_trb	*trb_st_hw;
		struct dwc3_trb	*trb_link;

		dep->flags |= DWC3_EP_ENABLED;

		reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);

		if (usb_endpoint_xfer_control(desc))
			return 0;

		/* Initialize the TRB ring */
		dep->trb_dequeue = 0;
		dep->trb_enqueue = 0;
		memset(dep->trb_pool, 0,
		       sizeof(struct dwc3_trb) * DWC3_TRB_NUM);

		/* Link TRB. The HWO bit is never reset */
		trb_st_hw = &dep->trb_pool[0];

		trb_link = &dep->trb_pool[DWC3_TRB_NUM - 1];
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

	dwc3_stop_active_transfer(dwc, dep->number, true);

	/* - giveback all requests to gadget driver */
	while (!list_empty(&dep->started_list)) {
		req = next_request(&dep->started_list);

		dwc3_gadget_giveback(dep, req, -ESHUTDOWN);
	}

	while (!list_empty(&dep->pending_list)) {
		req = next_request(&dep->pending_list);

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

	dwc3_trace(trace_dwc3_gadget, "Disabling %s", dep->name);

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

	/* Keep GSI ep names with "-gsi" suffix */
	if (!strnstr(dep->name, "gsi", 10)) {
		snprintf(dep->name, sizeof(dep->name), "ep%d%s",
			dep->number >> 1,
			(dep->number & 1) ? "in" : "out");
	}

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
		pr_debug("dwc3: invalid parameters\n");
		return -EINVAL;
	}

	if (!desc->wMaxPacketSize) {
		pr_debug("dwc3: missing wMaxPacketSize\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (dev_WARN_ONCE(dwc->dev, dep->flags & DWC3_EP_ENABLED,
					"%s is already enabled\n",
					dep->name))
		return 0;

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

	if (dev_WARN_ONCE(dwc->dev, !(dep->flags & DWC3_EP_ENABLED),
					"%s is already disabled\n",
					dep->name))
		return 0;

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

	dep->allocated_requests++;

	trace_dwc3_alloc_request(req);

	return &req->request;
}

static void dwc3_gadget_ep_free_request(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request		*req = to_dwc3_request(request);
	struct dwc3_ep			*dep = to_dwc3_ep(ep);

	dep->allocated_requests--;
	trace_dwc3_free_request(req);
	kfree(req);
}

static u32 dwc3_calc_trbs_left(struct dwc3_ep *dep);

/**
 * dwc3_prepare_one_trb - setup one TRB from one request
 * @dep: endpoint for which this request is prepared
 * @req: dwc3_request pointer
 */
static void dwc3_prepare_one_trb(struct dwc3_ep *dep,
		struct dwc3_request *req, dma_addr_t dma,
		unsigned length, unsigned chain, unsigned node)
{
	struct dwc3_trb		*trb;
	struct dwc3		*dwc = dep->dwc;
	struct usb_gadget	*gadget = &dwc->gadget;
	enum usb_device_speed	speed = gadget->speed;

	dwc3_trace(trace_dwc3_gadget, "%s: req %p dma %08llx length %d%s",
			dep->name, req, (unsigned long long) dma,
			length, chain ? " chain" : "");

	trb = &dep->trb_pool[dep->trb_enqueue];

	if (!req->trb) {
		dwc3_gadget_move_started_request(req);
		req->trb = trb;
		req->trb_dma = dwc3_trb_dma_offset(dep, trb);
		req->first_trb_index = dep->trb_enqueue;
		dep->queued_requests++;
	}

	dwc3_ep_inc_enq(dep);

	trb->size = DWC3_TRB_SIZE_LENGTH(length);
	trb->bpl = lower_32_bits(dma);
	trb->bph = upper_32_bits(dma);

	switch (usb_endpoint_type(dep->endpoint.desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		trb->ctrl = DWC3_TRBCTL_CONTROL_SETUP;
		break;

	case USB_ENDPOINT_XFER_ISOC:
		if (!node) {
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS_FIRST;

			/*
			 * USB Specification 2.0 Section 5.9.2 states that: "If
			 * there is only a single transaction in the microframe,
			 * only a DATA0 data packet PID is used.  If there are
			 * two transactions per microframe, DATA1 is used for
			 * the first transaction data packet and DATA0 is used
			 * for the second transaction data packet.  If there are
			 * three transactions per microframe, DATA2 is used for
			 * the first transaction data packet, DATA1 is used for
			 * the second, and DATA0 is used for the third."
			 *
			 * IOW, we should satisfy the following cases:
			 *
			 * 1) length <= maxpacket
			 *	- DATA0
			 *
			 * 2) maxpacket < length <= (2 * maxpacket)
			 *	- DATA1, DATA0
			 *
			 * 3) (2 * maxpacket) < length <= (3 * maxpacket)
			 *	- DATA2, DATA1, DATA0
			 */
			if (speed == USB_SPEED_HIGH) {
				struct usb_ep *ep = &dep->endpoint;
				unsigned int mult = ep->mult - 1;
				unsigned int maxp;

				maxp = usb_endpoint_maxp(ep->desc) & 0x07ff;

				if (length <= (2 * maxp))
					mult--;

				if (length <= maxp)
					mult--;

				trb->size |= DWC3_TRB_SIZE_PCM1(mult);
			}
		} else {
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS;
		}

		/* always enable Interrupt on Missed ISOC */
		trb->ctrl |= DWC3_TRB_CTRL_ISP_IMI;
		break;

	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		trb->ctrl = DWC3_TRBCTL_NORMAL;
		break;
	default:
		/*
		 * This is only possible with faulty memory because we
		 * checked it already :)
		 */
		BUG();
	}

	/* always enable Continue on Short Packet */
	if (usb_endpoint_dir_out(dep->endpoint.desc)) {
		trb->ctrl |= DWC3_TRB_CTRL_CSP;

		if (req->request.short_not_ok)
			trb->ctrl |= DWC3_TRB_CTRL_ISP_IMI;
	}

	if ((!req->request.no_interrupt && !chain) ||
			(dwc3_calc_trbs_left(dep) == 0))
		trb->ctrl |= DWC3_TRB_CTRL_IOC;

	if (chain)
		trb->ctrl |= DWC3_TRB_CTRL_CHN;

	if (usb_endpoint_xfer_bulk(dep->endpoint.desc) && dep->stream_capable)
		trb->ctrl |= DWC3_TRB_CTRL_SID_SOFN(req->request.stream_id);

	trb->ctrl |= DWC3_TRB_CTRL_HWO;

	trace_dwc3_prepare_trb(dep, trb);
}

/**
 * dwc3_ep_prev_trb() - Returns the previous TRB in the ring
 * @dep: The endpoint with the TRB ring
 * @index: The index of the current TRB in the ring
 *
 * Returns the TRB prior to the one pointed to by the index. If the
 * index is 0, we will wrap backwards, skip the link TRB, and return
 * the one just before that.
 */
static struct dwc3_trb *dwc3_ep_prev_trb(struct dwc3_ep *dep, u8 index)
{
	u8 tmp = index;

	if (!tmp)
		tmp = DWC3_TRB_NUM - 1;

	return &dep->trb_pool[tmp - 1];
}

static u32 dwc3_calc_trbs_left(struct dwc3_ep *dep)
{
	struct dwc3_trb		*tmp;
	u8			trbs_left;

	/*
	 * If enqueue & dequeue are equal than it is either full or empty.
	 *
	 * One way to know for sure is if the TRB right before us has HWO bit
	 * set or not. If it has, then we're definitely full and can't fit any
	 * more transfers in our ring.
	 */
	if (dep->trb_enqueue == dep->trb_dequeue) {
		tmp = dwc3_ep_prev_trb(dep, dep->trb_enqueue);
		if (tmp->ctrl & DWC3_TRB_CTRL_HWO)
			return 0;

		return DWC3_TRB_NUM - 1;
	}

	trbs_left = dep->trb_dequeue - dep->trb_enqueue;
	trbs_left &= (DWC3_TRB_NUM - 1);

	if (dep->trb_dequeue < dep->trb_enqueue)
		trbs_left--;

	return trbs_left;
}

static void dwc3_prepare_one_trb_sg(struct dwc3_ep *dep,
		struct dwc3_request *req)
{
	struct scatterlist *sg = req->sg;
	struct scatterlist *s;
	unsigned int	length;
	dma_addr_t	dma;
	int		i;

	for_each_sg(sg, s, req->num_pending_sgs, i) {
		unsigned chain = true;

		length = sg_dma_len(s);
		dma = sg_dma_address(s);

		if (sg_is_last(s))
			chain = false;

		dwc3_prepare_one_trb(dep, req, dma, length,
				chain, i);

		if (!dwc3_calc_trbs_left(dep))
			break;
	}
}

static void dwc3_prepare_one_trb_linear(struct dwc3_ep *dep,
		struct dwc3_request *req)
{
	unsigned int	length;
	dma_addr_t	dma;

	dma = req->request.dma;
	length = req->request.length;

	dwc3_prepare_one_trb(dep, req, dma, length,
			false, 0);
}

/*
 * dwc3_prepare_trbs - setup TRBs from requests
 * @dep: endpoint for which requests are being prepared
 *
 * The function goes through the requests list and sets up TRBs for the
 * transfers. The function returns once there are no more TRBs available or
 * it runs out of requests.
 */
static void dwc3_prepare_trbs(struct dwc3_ep *dep)
{
	struct dwc3_request	*req, *n;

	BUILD_BUG_ON_NOT_POWER_OF_2(DWC3_TRB_NUM);

	if (!dwc3_calc_trbs_left(dep))
		return;

	list_for_each_entry_safe(req, n, &dep->pending_list, list) {
		if (req->num_pending_sgs > 0)
			dwc3_prepare_one_trb_sg(dep, req);
		else
			dwc3_prepare_one_trb_linear(dep, req);

		if (!dwc3_calc_trbs_left(dep))
			return;
	}
}

static int __dwc3_gadget_kick_transfer(struct dwc3_ep *dep, u16 cmd_param)
{
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3_request		*req, *req1, *n;
	struct dwc3			*dwc = dep->dwc;
	int				starting;
	int				ret;
	u32				cmd;

	starting = !(dep->flags & DWC3_EP_BUSY);

	dwc3_prepare_trbs(dep);
	req = next_request(&dep->started_list);
	if (!req) {
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		dbg_event(dep->number, "NO REQ", 0);
		return 0;
	}

	memset(&params, 0, sizeof(params));

	if (starting) {
		params.param0 = upper_32_bits(req->trb_dma);
		params.param1 = lower_32_bits(req->trb_dma);
		cmd = DWC3_DEPCMD_STARTTRANSFER |
			DWC3_DEPCMD_PARAM(cmd_param);
	} else {
		cmd = DWC3_DEPCMD_UPDATETRANSFER |
			DWC3_DEPCMD_PARAM(dep->resource_index);
	}

	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0) {
		if ((ret == -EAGAIN) && starting &&
				usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			/* If bit13 in Command complete event is set, software
			 * must issue ENDTRANDFER command and wait for
			 * Xfernotready event to queue the requests again.
			 */
			if (!dep->resource_index) {
				dep->resource_index =
					 dwc3_gadget_ep_get_transfer_index(dep);
				WARN_ON_ONCE(!dep->resource_index);
			}
			dwc3_stop_active_transfer(dwc, dep->number, true);

			list_for_each_entry_safe_reverse(req1, n,
						&dep->started_list, list) {
				req1->trb->ctrl &= ~DWC3_TRB_CTRL_HWO;
				req1->trb = NULL;
				dwc3_gadget_move_pending_list_front(req1);
				dwc3_ep_inc_deq(dep);
			}

			return ret;
		}

		/*
		 * FIXME we need to iterate over the list of requests
		 * here and stop, unmap, free and del each of the linked
		 * requests instead of what we do now.
		 */
		usb_gadget_unmap_request_by_dev(dwc->sysdev,
				&req->request, req->direction);
		list_del(&req->list);
		return ret;
	}

	dep->flags |= DWC3_EP_BUSY;

	if (starting) {
		dep->resource_index = dwc3_gadget_ep_get_transfer_index(dep);
		WARN_ON_ONCE(!dep->resource_index);
	}

	return 0;
}

static int __dwc3_gadget_get_frame(struct dwc3 *dwc)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	return DWC3_DSTS_SOFFN(reg);
}

static void __dwc3_gadget_start_isoc(struct dwc3 *dwc,
		struct dwc3_ep *dep, u32 cur_uf)
{
	u32 uf;
	int ret;

	if (list_empty(&dep->pending_list)) {
		dwc3_trace(trace_dwc3_gadget,
				"ISOC ep %s run out for requests",
				dep->name);
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		return;
	}

	/*
	 * Schedule the first trb for one interval in the future or at
	 * least 4 microframes.
	 */
	uf = cur_uf + max_t(u32, 16, dep->interval);

	ret = __dwc3_gadget_kick_transfer(dep, uf);
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

	if (!dep->endpoint.desc) {
		dwc3_trace(trace_dwc3_gadget,
				"trying to queue request %p to disabled %s",
				&req->request, dep->endpoint.name);
		return -ESHUTDOWN;
	}

	if (WARN(req->dep != dep, "request %pK belongs to '%s'\n",
				&req->request, req->dep->name)) {
		dwc3_trace(trace_dwc3_gadget, "request %pK belongs to '%s'",
				&req->request, req->dep->name);
		return -EINVAL;
	}

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

	trace_dwc3_ep_queue(req);

	ret = usb_gadget_map_request_by_dev(dwc->sysdev, &req->request,
					    dep->direction);
	if (ret)
		return ret;

	req->sg			= req->request.sg;
	req->num_pending_sgs	= req->request.num_mapped_sgs;

	list_add_tail(&req->list, &dep->pending_list);

	/*
	 * NOTICE: Isochronous endpoints should NEVER be prestarted. We must
	 * wait for a XferNotReady event so we will know what's the current
	 * (micro-)frame number.
	 *
	 * Without this trick, we are very, very likely gonna get Bus Expiry
	 * errors which will force us issue EndTransfer command.
	 */
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		if ((dep->flags & DWC3_EP_PENDING_REQUEST)) {
			if (dep->flags & DWC3_EP_TRANSFER_STARTED) {
				dwc3_stop_active_transfer(dwc, dep->number, true);
				dep->flags = DWC3_EP_ENABLED;
			} else {
				u32 cur_uf;

				cur_uf = __dwc3_gadget_get_frame(dwc);
				__dwc3_gadget_start_isoc(dwc, dep, cur_uf);
				dep->flags &= ~DWC3_EP_PENDING_REQUEST;
			}
			return 0;
		}

		if ((dep->flags & DWC3_EP_BUSY) &&
		    !(dep->flags & DWC3_EP_MISSED_ISOC)) {
			WARN_ON_ONCE(!dep->resource_index);
			ret = __dwc3_gadget_kick_transfer(dep,
							  dep->resource_index);
		}

		goto out;
	}

	if (!dwc3_calc_trbs_left(dep))
		return 0;

	ret = __dwc3_gadget_kick_transfer(dep, 0);
	if (ret && ret != -EBUSY)
		dwc3_trace(trace_dwc3_gadget,
				"%s: failed to kick transfers",
				dep->name);
out:
	if (ret == -EBUSY)
		ret = 0;

	return ret;
}

static int dwc3_gadget_wakeup(struct usb_gadget *g)
{
	struct dwc3	*dwc = gadget_to_dwc(g);

	schedule_work(&dwc->wakeup_work);
	return 0;
}

static bool dwc3_gadget_is_suspended(struct dwc3 *dwc)
{
	if (atomic_read(&dwc->in_lpm) ||
			dwc->link_state == DWC3_LINK_STATE_U3)
		return true;
	return false;
}

static void __dwc3_gadget_ep_zlp_complete(struct usb_ep *ep,
		struct usb_request *request)
{
	dwc3_gadget_ep_free_request(ep, request);
}

static int __dwc3_gadget_ep_queue_zlp(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_request		*req;
	struct usb_request		*request;
	struct usb_ep			*ep = &dep->endpoint;

	dwc3_trace(trace_dwc3_gadget, "queueing ZLP");
	request = dwc3_gadget_ep_alloc_request(ep, GFP_ATOMIC);
	if (!request)
		return -ENOMEM;

	request->length = 0;
	request->buf = dwc->zlp_buf;
	request->complete = __dwc3_gadget_ep_zlp_complete;

	req = to_dwc3_request(request);

	return __dwc3_gadget_ep_queue(dep, req);
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
		ret = -ESHUTDOWN;
		goto out;
	}

	if (WARN(req->dep != dep, "request %p belongs to '%s'\n",
				request, req->dep->name)) {
		ret = -EINVAL;
		goto out;
	}

	if (dwc3_gadget_is_suspended(dwc)) {
		if (dwc->gadget.remote_wakeup)
			dwc3_gadget_wakeup(&dwc->gadget);
		ret =  dwc->gadget.remote_wakeup ? -EAGAIN : -ENOTSUPP;
		goto out;
	}

	WARN(!dep->direction && (request->length % ep->desc->wMaxPacketSize),
		"trying to queue unaligned request (%d) with %s\n",
		request->length, ep->name);

	ret = __dwc3_gadget_ep_queue(dep, req);

	/*
	 * Okay, here's the thing, if gadget driver has requested for a ZLP by
	 * setting request->zero, instead of doing magic, we will just queue an
	 * extra usb_request ourselves so that it gets handled the same way as
	 * any other request.
	 */
	if (ret == 0 && request->zero && request->length &&
	    (request->length % ep->maxpacket == 0))
		ret = __dwc3_gadget_ep_queue_zlp(dwc, dep);

out:
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
		dev_err(dwc->dev, "Unable to dequeue while in LPM\n");
		return -EAGAIN;
	}

	trace_dwc3_ep_dequeue(req);

	spin_lock_irqsave(&dwc->lock, flags);

	list_for_each_entry(r, &dep->pending_list, list) {
		if (r == req)
			break;
	}

	if (r != req) {
		list_for_each_entry(r, &dep->started_list, list) {
			if (r == req)
				break;
		}
		if (r == req) {
			/* wait until it is processed */
			dwc3_stop_active_transfer(dwc, dep->number, true);

			/*
			 * If request was already started, this means we had to
			 * stop the transfer. With that we also need to ignore
			 * all TRBs used by the request, however TRBs can only
			 * be modified after completion of END_TRANSFER
			 * command. So what we do here is that we wait for
			 * END_TRANSFER completion and only after that, we jump
			 * over TRBs by clearing HWO and incrementing dequeue
			 * pointer.
			 *
			 * Note that we have 2 possible types of transfers here:
			 *
			 * i) Linear buffer request
			 * ii) SG-list based request
			 *
			 * SG-list based requests will have r->num_pending_sgs
			 * set to a valid number (> 0). Linear requests,
			 * normally use a single TRB.
			 *
			 * All of these cases need to be taken into
			 * consideration so we don't mess up our TRB ring
			 * pointers.
			 */
			if (!r->trb)
				goto out1;

			if (r->num_pending_sgs) {
				struct dwc3_trb *trb;
				int i = 0;

				for (i = 0; i < r->num_pending_sgs; i++) {
					trb = r->trb + i;
					trb->ctrl &= ~DWC3_TRB_CTRL_HWO;
					dwc3_ep_inc_deq(dep);
				}
			} else {
				struct dwc3_trb *trb = r->trb;

				trb->ctrl &= ~DWC3_TRB_CTRL_HWO;
				dwc3_ep_inc_deq(dep);
			}
			goto out1;
		}
		dev_err(dwc->dev, "request %pK was not queued to %s\n",
				request, ep->name);
		ret = -EINVAL;
		goto out0;
	}

out1:
	dbg_event(dep->number, "DEQUEUE", 0);
	/* giveback the request */
	dep->queued_requests--;
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

	if (!dep->endpoint.desc) {
		dev_dbg(dwc->dev, "(%s)'s desc is NULL.\n", dep->name);
		return -EINVAL;
	}

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dev_err(dwc->dev, "%s is of Isochronous type\n", dep->name);
		return -EINVAL;
	}

	memset(&params, 0x00, sizeof(params));
	dbg_event(dep->number, "HALT", value);
	if (value) {
		struct dwc3_trb *trb;

		unsigned transfer_in_flight;
		unsigned started;

		if (dep->number > 1)
			trb = dwc3_ep_prev_trb(dep, dep->trb_enqueue);
		else
			trb = &dwc->ep0_trb[dep->trb_enqueue];

		transfer_in_flight = trb->ctrl & DWC3_TRB_CTRL_HWO;
		started = !list_empty(&dep->started_list);

		if (!protocol && ((dep->direction && transfer_in_flight) ||
				(!dep->direction && started))) {
			dwc3_trace(trace_dwc3_gadget,
					"%s: pending request, cannot halt",
					dep->name);
			return -EAGAIN;
		}

		ret = dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETSTALL,
				&params);
		if (ret)
			dev_err(dwc->dev, "failed to set STALL on %s\n",
					dep->name);
		else
			dep->flags |= DWC3_EP_STALL;
	} else {

		ret = dwc3_send_clear_stall_ep_cmd(dep);
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
	ret = __dwc3_gadget_ep_set_halt(dep, value, false);
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

	return __dwc3_gadget_get_frame(dwc);
}

static int __dwc3_gadget_wakeup(struct dwc3 *dwc)
{
	int			retries;

	int			ret;
	u32			reg;

	u8			link_state;
	u8			speed;

	/*
	 * According to the Databook Remote wakeup request should
	 * be issued only when the device is in early suspend state.
	 *
	 * We can check that via USB Link State bits in DSTS register.
	 */
	reg = dwc3_readl(dwc->regs, DWC3_DSTS);

	speed = reg & DWC3_DSTS_CONNECTSPD;
	if ((speed == DWC3_DSTS_SUPERSPEED) ||
	    (speed == DWC3_DSTS_SUPERSPEED_PLUS)) {
		dwc3_trace(trace_dwc3_gadget, "no wakeup on SuperSpeed");
		return 0;
	}

	link_state = DWC3_DSTS_USBLNKST(reg);

	switch (link_state) {
	case DWC3_LINK_STATE_RX_DET:	/* in HS, means Early Suspend */
	case DWC3_LINK_STATE_U3:	/* in HS, means SUSPEND */
		break;
	default:
		dwc3_trace(trace_dwc3_gadget,
				"can't wakeup from '%s'",
				dwc3_gadget_link_string(link_state));
		return -EINVAL;
	}

	ret = dwc3_gadget_set_link_state(dwc, DWC3_LINK_STATE_RECOV);
	if (ret < 0) {
		dev_err(dwc->dev, "failed to put link in Recovery\n");
		return ret;
	}

	/* Recent versions do this automatically */
	if (dwc->revision < DWC3_REVISION_194A) {
		/* write zeroes to Link Change Request */
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}

	/* poll until Link State changes to ON */
	retries = 20000;

	while (retries--) {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);

		/* in HS, means ON */
		if (DWC3_DSTS_USBLNKST(reg) == DWC3_LINK_STATE_U0)
			break;
	}

	if (DWC3_DSTS_USBLNKST(reg) != DWC3_LINK_STATE_U0) {
		dev_err(dwc->dev, "failed to send remote wakeup\n");
		return -EINVAL;
	}

	return 0;
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
	g->is_selfpowered = !!is_selfpowered;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

#define DWC3_SOFT_RESET_TIMEOUT		10  /* 10 msec */
static int dwc3_gadget_run_stop(struct dwc3 *dwc, int is_on, int suspend)
{
	u32			reg;
	u32			timeout = 1500;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	if (is_on) {
		dbg_event(0xFF, "Pullup_enable", is_on);
		if (dwc->revision <= DWC3_REVISION_187A) {
			reg &= ~DWC3_DCTL_TRGTULST_MASK;
			reg |= DWC3_DCTL_TRGTULST_RX_DET;
		}

		if (dwc->revision >= DWC3_REVISION_194A)
			reg &= ~DWC3_DCTL_KEEP_CONNECT;

		dwc3_event_buffers_setup(dwc);
		dwc3_gadget_restart(dwc);
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation)
			reg |= DWC3_DCTL_KEEP_CONNECT;

		dwc->pullups_connected = true;
	} else {
		dbg_event(0xFF, "Pullup_disable", is_on);
		dwc3_gadget_disable_irq(dwc);
		__dwc3_gadget_ep_disable(dwc->eps[0]);
		__dwc3_gadget_ep_disable(dwc->eps[1]);

		reg &= ~DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation && !suspend)
			reg &= ~DWC3_DCTL_KEEP_CONNECT;

		dwc->pullups_connected = false;
	}

	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	do {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);
		reg &= DWC3_DSTS_DEVCTRLHLT;
	} while (--timeout && !(!is_on ^ !reg));

	if (!timeout) {
		dev_err(dwc->dev, "failed to %s controller\n",
				is_on ? "start" : "stop");
		if (is_on)
			dbg_event(0xFF, "STARTTOUT", reg);
		else
			dbg_event(0xFF, "STOPTOUT", reg);
		return -ETIMEDOUT;
	}

	dwc3_trace(trace_dwc3_gadget, "gadget %s data soft-%s",
			dwc->gadget_driver
			? dwc->gadget_driver->function : "no-function",
			is_on ? "connect" : "disconnect");

	return 0;
}

static int dwc3_gadget_vbus_draw(struct usb_gadget *g, unsigned int mA)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	dwc->vbus_draw = mA;
	dev_dbg(dwc->dev, "Notify controller from %s. mA = %u\n", __func__, mA);
	dbg_event(0xFF, "currentDraw", mA);
	dwc3_notify_event(dwc, DWC3_CONTROLLER_SET_CURRENT_DRAW_EVENT);
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
		dbg_event(0xFF, "WaitPullup", 0);
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
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT);
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

	dbg_event(0xFF, "UnmaskINT", 0);
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
	dbg_event(0xFF, "MaskINT", 0);
	/* mask all interrupts */
	dwc3_writel(dwc->regs, DWC3_DEVTEN, 0x00);
}

static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc);
static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc);

/**
 * dwc3_gadget_setup_nump - Calculate and initialize NUMP field of DCFG
 * dwc: pointer to our context structure
 *
 * The following looks like complex but it's actually very simple. In order to
 * calculate the number of packets we can burst at once on OUT transfers, we're
 * gonna use RxFIFO size.
 *
 * To calculate RxFIFO size we need two numbers:
 * MDWIDTH = size, in bits, of the internal memory bus
 * RAM2_DEPTH = depth, in MDWIDTH, of internal RAM2 (where RxFIFO sits)
 *
 * Given these two numbers, the formula is simple:
 *
 * RxFIFO Size = (RAM2_DEPTH * MDWIDTH / 8) - 24 - 16;
 *
 * 24 bytes is for 3x SETUP packets
 * 16 bytes is a clock domain crossing tolerance
 *
 * Given RxFIFO Size, NUMP = RxFIFOSize / 1024;
 */
static void dwc3_gadget_setup_nump(struct dwc3 *dwc)
{
	u32 ram2_depth;
	u32 mdwidth;
	u32 nump;
	u32 reg;

	ram2_depth = DWC3_GHWPARAMS7_RAM2_DEPTH(dwc->hwparams.hwparams7);
	mdwidth = DWC3_GHWPARAMS0_MDWIDTH(dwc->hwparams.hwparams0);

	nump = ((ram2_depth * mdwidth / 8) - 24 - 16) / 1024;
	nump = min_t(u32, nump, 16);

	/* update NumP */
	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~DWC3_DCFG_NUMP_MASK;
	reg |= nump << DWC3_DCFG_NUMP_SHIFT;
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
}

static int dwc3_gadget_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct dwc3 *dwc = gadget_to_dwc(_gadget);
	unsigned long flags;

	if (!dwc->is_drd)
		return -EPERM;

	is_active = !!is_active;

	dbg_event(0xFF, "VbusSess", is_active);
	spin_lock_irqsave(&dwc->lock, flags);

	/* Mark that the vbus was powered */
	dwc->vbus_active = is_active;

	/*
	 * Check if upper level usb_gadget_driver was already registered with
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

	dbg_event(0xFF, "__Gadgetstart", 0);

	/*
	 * Use IMOD if enabled via dwc->imod_interval. Otherwise, if
	 * the core supports IMOD, disable it.
	 */
	if (dwc->imod_interval) {
		dwc3_writel(dwc->regs, DWC3_DEV_IMOD(0), dwc->imod_interval);
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), DWC3_GEVNTCOUNT_EHB);
	} else if (dwc3_has_imod(dwc)) {
		dwc3_writel(dwc->regs, DWC3_DEV_IMOD(0), 0);
	}

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
			reg |= DWC3_DCFG_LOWSPEED;
			break;
		case USB_SPEED_FULL:
			reg |= DWC3_DCFG_FULLSPEED;
			break;
		case USB_SPEED_HIGH:
			reg |= DWC3_DCFG_HIGHSPEED;
			break;
		case USB_SPEED_SUPER_PLUS:
			reg |= DWC3_DCFG_SUPERSPEED_PLUS;
			break;
		default:
			dev_err(dwc->dev, "invalid dwc->maximum_speed (%d)\n",
					dwc->maximum_speed);
			/* fall through */
		case USB_SPEED_SUPER:
			reg |= DWC3_DCFG_SUPERSPEED;
			break;
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

	dwc3_gadget_setup_nump(dwc);

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

static void __dwc3_gadget_stop(struct dwc3 *dwc)
{
	dbg_event(0xFF, "__Gadgetstop", 0);
	dwc3_gadget_disable_irq(dwc);
	__dwc3_gadget_ep_disable(dwc->eps[0]);
	__dwc3_gadget_ep_disable(dwc->eps[1]);
}

static int dwc3_gadget_stop(struct usb_gadget *g)
{
	struct dwc3	*dwc = gadget_to_dwc(g);
	unsigned long	flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->gadget_driver = NULL;
	spin_unlock_irqrestore(&dwc->lock, flags);

	dbg_event(0xFF, "fwq_started", 0);
	flush_workqueue(dwc->dwc_wq);
	dbg_event(0xFF, "fwq_completed", 0);

	return 0;
}

static int dwc3_gadget_restart_usb_session(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	dbg_event(0xFF, "RestartUSBSession", 0);
	return dwc3_notify_event(dwc, DWC3_CONTROLLER_RESTART_USB_SESSION);
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

	gsi_ep_count = NUM_GSI_OUT_EPS + NUM_GSI_IN_EPS;
	/* OUT GSI EPs based on direction field */
	if (gsi_ep_count && !direction)
		gsi_ep_count = NUM_GSI_OUT_EPS;
	/* IN GSI EPs */
	else if (gsi_ep_count && direction)
		gsi_ep_count = NUM_GSI_IN_EPS;

	for (i = 0; i < num; i++) {
		u8 epnum = (i << 1) | (direction ? 1 : 0);

		dep = kzalloc(sizeof(*dep), GFP_KERNEL);
		if (!dep)
			return -ENOMEM;

		dep->dwc = dwc;
		dep->number = epnum;
		dep->direction = !!direction;
		dep->regs = dwc->regs + DWC3_DEP_BASE(epnum);
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
		spin_lock_init(&dep->lock);

		dwc3_trace(trace_dwc3_gadget, "initializing %s", dep->name);

		if (epnum == 0 || epnum == 1) {
			usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
			dep->endpoint.maxburst = 1;
			dep->endpoint.ops = &dwc3_gadget_ep0_ops;
			if (!epnum)
				dwc->gadget.ep0 = &dep->endpoint;
		} else {
			int		ret;

			usb_ep_set_maxpacket_limit(&dep->endpoint, 1024);
			dep->endpoint.max_streams = 15;
			dep->endpoint.ops = &dwc3_gadget_ep_ops;
			list_add_tail(&dep->endpoint.ep_list,
					&dwc->gadget.ep_list);

			ret = dwc3_alloc_trb_pool(dep);
			if (ret)
				return ret;
		}

		if (epnum == 0 || epnum == 1) {
			dep->endpoint.caps.type_control = true;
		} else {
			dep->endpoint.caps.type_iso = true;
			dep->endpoint.caps.type_bulk = true;
			dep->endpoint.caps.type_int = true;
		}

		dep->endpoint.caps.dir_in = !!direction;
		dep->endpoint.caps.dir_out = !direction;

		INIT_LIST_HEAD(&dep->pending_list);
		INIT_LIST_HEAD(&dep->started_list);
	}

	return 0;
}

static int dwc3_gadget_init_endpoints(struct dwc3 *dwc)
{
	int				ret;

	INIT_LIST_HEAD(&dwc->gadget.ep_list);

	ret = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_out_eps, 0);
	if (ret < 0) {
		dwc3_trace(trace_dwc3_gadget,
				"failed to allocate OUT endpoints");
		return ret;
	}

	ret = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_in_eps, 1);
	if (ret < 0) {
		dwc3_trace(trace_dwc3_gadget,
				"failed to allocate IN endpoints");
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
			dwc3_free_trb_pool(dep);
			list_del(&dep->endpoint.ep_list);
		}

		kfree(dep);
	}
}

/* -------------------------------------------------------------------------- */

static int __dwc3_cleanup_done_trbs(struct dwc3 *dwc, struct dwc3_ep *dep,
		struct dwc3_request *req, struct dwc3_trb *trb,
		const struct dwc3_event_depevt *event, int status,
		int chain)
{
	unsigned int		count;
	unsigned int		s_pkt = 0;
	unsigned int		trb_status;

	dwc3_ep_inc_deq(dep);

	if (req->trb == trb)
		dep->queued_requests--;

	trace_dwc3_complete_trb(dep, trb);

	/*
	 * If we're in the middle of series of chained TRBs and we
	 * receive a short transfer along the way, DWC3 will skip
	 * through all TRBs including the last TRB in the chain (the
	 * where CHN bit is zero. DWC3 will also avoid clearing HWO
	 * bit and SW has to do it manually.
	 *
	 * We're going to do that here to avoid problems of HW trying
	 * to use bogus TRBs for transfers.
	 */
	if (chain && (trb->ctrl & DWC3_TRB_CTRL_HWO))
		trb->ctrl &= ~DWC3_TRB_CTRL_HWO;

	if ((trb->ctrl & DWC3_TRB_CTRL_HWO) && status != -ESHUTDOWN)
		return 1;

	count = trb->size & DWC3_TRB_SIZE_MASK;
	req->request.actual += count;

	if (dep->direction) {
		if (count) {
			trb_status = DWC3_TRB_SIZE_TRBSTS(trb->size);
			if (trb_status == DWC3_TRBSTS_MISSED_ISOC) {
				dwc3_trace(trace_dwc3_gadget,
						"%s: incomplete IN transfer",
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
				 * request in pending_list during
				 * giveback.If any future queued request
				 * is successfully transferred then we
				 * will issue UPDATE TRANSFER for all
				 * request in the pending_list.
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

	if (s_pkt && !chain)
		return 1;

	if ((event->status & DEPEVT_STATUS_IOC) &&
			(trb->ctrl & DWC3_TRB_CTRL_IOC))
		return 1;

	return 0;
}

static int dwc3_cleanup_done_reqs(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event, int status)
{
	struct dwc3_request	*req, *n;
	struct dwc3_trb		*trb;
	bool			ioc = false;
	int			ret;

	list_for_each_entry_safe(req, n, &dep->started_list, list) {
		unsigned length;
		unsigned actual;
		int chain;

		length = req->request.length;
		chain = req->num_pending_sgs > 0;
		if (chain) {
			struct scatterlist *sg = req->sg;
			struct scatterlist *s;
			unsigned int pending = req->num_pending_sgs;
			unsigned int i;

			for_each_sg(sg, s, pending, i) {
				trb = &dep->trb_pool[dep->trb_dequeue];

				req->sg = sg_next(s);
				req->num_pending_sgs--;

				ret = __dwc3_cleanup_done_trbs(dwc, dep, req, trb,
						event, status, chain);
				if (ret)
					break;
			}
		} else {
			trb = &dep->trb_pool[dep->trb_dequeue];
			ret = __dwc3_cleanup_done_trbs(dwc, dep, req, trb,
					event, status, chain);
		}

		/*
		 * We assume here we will always receive the entire data block
		 * which we should receive. Meaning, if we program RX to
		 * receive 4K but we receive only 2K, we assume that's all we
		 * should receive and we simply bounce the request back to the
		 * gadget driver for further processing.
		 */
		actual = length - req->request.actual;
		req->request.actual = actual;

		if (ret && chain && (actual < length) && req->num_pending_sgs)
			return __dwc3_gadget_kick_transfer(dep, 0);

		dwc3_gadget_giveback(dep, req, status);

		if (ret) {
			if ((event->status & DEPEVT_STATUS_IOC) &&
			    (trb->ctrl & DWC3_TRB_CTRL_IOC))
				ioc = true;
			break;
		}
	}

	/*
	 * Our endpoint might get disabled by another thread during
	 * dwc3_gadget_giveback(). If that happens, we're just gonna return 1
	 * early on so DWC3_EP_BUSY flag gets cleared
	 */
	if (!dep->endpoint.desc)
		return 1;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
			list_empty(&dep->started_list)) {
		if (list_empty(&dep->pending_list))
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

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) && ioc)
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
	if (clean_busy && (!dep->endpoint.desc || is_xfer_complete ||
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

			if (!list_empty(&dep->started_list))
				return;
		}

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= dwc->u1u2;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);

		dwc->u1u2 = 0;
	}

	/*
	 * Our endpoint might get disabled by another thread during
	 * dwc3_gadget_giveback(). If that happens, we're just gonna return 1
	 * early on so DWC3_EP_BUSY flag gets cleared
	 */
	if (!dep->endpoint.desc)
		return;

	if (!usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		int ret;

		ret = __dwc3_gadget_kick_transfer(dep, 0);
		if (!ret || ret == -EBUSY)
			return;
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
			dwc3_trace(trace_dwc3_gadget,
					"%s is an Isochronous endpoint",
					dep->name);
			return;
		}

		dwc3_endpoint_transfer_complete(dwc, dep, event);
		break;
	case DWC3_DEPEVT_XFERINPROGRESS:
		dep->dbg_ep_events.xferinprogress++;
		dwc3_endpoint_transfer_complete(dwc, dep, event);
		break;
	case DWC3_DEPEVT_XFERNOTREADY:
		dep->dbg_ep_events.xfernotready++;
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dwc3_gadget_start_isoc(dwc, dep, event);
		} else {
			int active;
			int ret;

			active = event->status & DEPEVT_STATUS_TRANSFER_ACTIVE;

			dwc3_trace(trace_dwc3_gadget, "%s: reason %s",
					dep->name, active ? "Transfer Active"
					: "Transfer Not Active");

			ret = __dwc3_gadget_kick_transfer(dep, 0);
			if (!ret || ret == -EBUSY)
				return;

			dwc3_trace(trace_dwc3_gadget,
					"%s: failed to kick transfers",
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
			dwc3_trace(trace_dwc3_gadget,
					"Stream %d found and started",
					event->parameters);

			break;
		case DEPEVT_STREAMEVT_NOTFOUND:
			/* FALLTHROUGH */
		default:
			dwc3_trace(trace_dwc3_gadget,
					"unable to find suitable stream");
		}
		break;
	case DWC3_DEPEVT_RXTXFIFOEVT:
		dwc3_trace(trace_dwc3_gadget, "%s FIFO Overrun", dep->name);
		dep->dbg_ep_events.rxtxfifoevent++;
		break;
	case DWC3_DEPEVT_EPCMDCMPLT:
		dwc3_trace(trace_dwc3_gadget, "Endpoint Command Complete");
		dep->dbg_ep_events.epcmdcomplete++;
		break;
	}
}

static void dwc3_disconnect_gadget(struct dwc3 *dwc)
{
	struct usb_gadget_driver *gadget_driver;

	if (dwc->gadget_driver && dwc->gadget_driver->disconnect) {
		gadget_driver = dwc->gadget_driver;
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "DISCONNECT", 0);
		gadget_driver->disconnect(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_suspend_gadget(struct dwc3 *dwc)
{
	struct usb_gadget_driver *gadget_driver;

	if (dwc->gadget_driver && dwc->gadget_driver->suspend) {
		gadget_driver = dwc->gadget_driver;
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "SUSPEND", 0);
		gadget_driver->suspend(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_resume_gadget(struct dwc3 *dwc)
{
	struct usb_gadget_driver *gadget_driver;

	if (dwc->gadget_driver && dwc->gadget_driver->resume) {
		gadget_driver = dwc->gadget_driver;
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "RESUME", 0);
		gadget_driver->resume(&dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_reset_gadget(struct dwc3 *dwc)
{
	struct usb_gadget_driver *gadget_driver;

	if (!dwc->gadget_driver)
		return;

	if (dwc->gadget.speed != USB_SPEED_UNKNOWN) {
		gadget_driver = dwc->gadget_driver;
		spin_unlock(&dwc->lock);
		dbg_event(0xFF, "UDC RESET", 0);
		usb_gadget_udc_reset(&dwc->gadget, gadget_driver);
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

	/*
	 * NOTICE: We are violating what the Databook says about the
	 * EndTransfer command. Ideally we would _always_ wait for the
	 * EndTransfer Command Completion IRQ, but that's causing too
	 * much trouble synchronizing between us and gadget driver.
	 *
	 * We have discussed this with the IP Provider and it was
	 * suggested to giveback all requests here, but give HW some
	 * extra time to synchronize with the interconnect. We're using
	 * an arbitrary 100us delay for that.
	 *
	 * Note also that a similar handling was tested by Synopsys
	 * (thanks a lot Paul) and nothing bad has come out of it.
	 * In short, what we're doing is:
	 *
	 * - Issue EndTransfer WITH CMDIOC bit set
	 * - Wait 100us
	 *
	 * As of IP version 3.10a of the DWC_usb3 IP, the controller
	 * supports a mode to work around the above limitation. The
	 * software can poll the CMDACT bit in the DEPCMD register
	 * after issuing a EndTransfer command. This mode is enabled
	 * by writing GUCTL2[14]. This polling is already done in the
	 * dwc3_send_gadget_ep_cmd() function so if the mode is
	 * enabled, the EndTransfer command will have completed upon
	 * returning from this function and we don't need to delay for
	 * 100us.
	 *
	 * This mode is NOT available on the DWC_usb31 IP.
	 */

	cmd = DWC3_DEPCMD_ENDTRANSFER;
	cmd |= force ? DWC3_DEPCMD_HIPRI_FORCERM : 0;
	cmd |= DWC3_DEPCMD_CMDIOC;
	cmd |= DWC3_DEPCMD_PARAM(dep->resource_index);
	memset(&params, 0, sizeof(params));
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	WARN_ON_ONCE(ret);
	dep->resource_index = 0;
	dep->flags &= ~DWC3_EP_BUSY;

	if (dwc3_is_usb31(dwc) || dwc->revision < DWC3_REVISION_310A)
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
		int ret;

		dep = dwc->eps[epnum];
		if (!dep)
			continue;

		if (!(dep->flags & DWC3_EP_STALL))
			continue;

		dep->flags &= ~DWC3_EP_STALL;

		ret = dwc3_send_clear_stall_ep_cmd(dep);
		dbg_event(dep->number, "ECLRSTALL", ret);
		WARN_ON_ONCE(ret);
	}
}

static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc)
{
	int			reg;

	dbg_event(0xFF, "DISCONNECT INT", 0);
	dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT);

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_INITU1ENA;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	reg &= ~DWC3_DCTL_INITU2ENA;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	dwc3_disconnect_gadget(dwc);

	dwc->gadget.speed = USB_SPEED_UNKNOWN;
	dwc->setup_packet_pending = false;
	dwc->link_state = DWC3_LINK_STATE_SS_DIS;
	usb_gadget_set_state(&dwc->gadget, USB_STATE_NOTATTACHED);

	dwc->connected = false;
	wake_up_interruptible(&dwc->wait_linkstate);
}

static void dwc3_gadget_reset_interrupt(struct dwc3 *dwc)
{
	u32			reg;

	dwc->connected = true;

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
	 * flag. Such flag gets set whenever we have a SETUP_PENDING
	 * status for EP0 TRBs and gets cleared on XferComplete for the
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

	dbg_event(0xFF, "BUS RESET", 0);
	dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
	dwc->b_suspend = false;
	dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT);

	dwc3_usb3_phy_suspend(dwc, false);
	usb_gadget_vbus_draw(&dwc->gadget, 100);

	dwc3_reset_gadget(dwc);

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	dwc->test_mode = false;

	dwc3_stop_active_transfers(dwc);
	dwc3_clear_stall_all_ep(dwc);

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

	if ((speed != DWC3_DSTS_SUPERSPEED) &&
	    (speed != DWC3_DSTS_SUPERSPEED_PLUS))
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

	dbg_event(0xFF, "CONNECT DONE", 0);
	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	speed = reg & DWC3_DSTS_CONNECTSPD;
	dwc->speed = speed;

	dwc3_update_ram_clk_sel(dwc, speed);

	switch (speed) {
	case DWC3_DSTS_SUPERSPEED_PLUS:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);
		dwc->gadget.ep0->maxpacket = 512;
		dwc->gadget.speed = USB_SPEED_SUPER_PLUS;
		break;
	case DWC3_DSTS_SUPERSPEED:
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
	case DWC3_DSTS_HIGHSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget.ep0->maxpacket = 64;
		dwc->gadget.speed = USB_SPEED_HIGH;
		break;
	case DWC3_DSTS_FULLSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget.ep0->maxpacket = 64;
		dwc->gadget.speed = USB_SPEED_FULL;
		break;
	case DWC3_DSTS_LOWSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(8);
		dwc->gadget.ep0->maxpacket = 8;
		dwc->gadget.speed = USB_SPEED_LOW;
		break;
	}

	/* Enable USB2 LPM Capability */

	if ((dwc->revision > DWC3_REVISION_194A) &&
	    (speed != DWC3_DSTS_SUPERSPEED) &&
	    (speed != DWC3_DSTS_SUPERSPEED_PLUS)) {
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
	dwc3_usb3_phy_suspend(dwc, true);

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

	dwc3_notify_event(dwc, DWC3_CONTROLLER_CONNDONE_EVENT);
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

	dbg_event(0xFF, "WAKEUP", remote_wakeup);
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

		/*
		 * In case of remote wake up dwc3_gadget_wakeup_work()
		 * is doing pm_runtime_get_sync().
		 */
		dev_dbg(dwc->dev, "Notify OTG from %s\n", __func__);
		dwc->b_suspend = false;
		dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT);

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
	 * WORKAROUND: DWC3 < 2.50a have an issue when configured without
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
	if ((dwc->revision < DWC3_REVISION_250A) &&
			(pwropt != DWC3_GHWPARAMS1_EN_PWROPT_HIB)) {
		if ((dwc->link_state == DWC3_LINK_STATE_U3) &&
				(next == DWC3_LINK_STATE_RESUME)) {
			dwc3_trace(trace_dwc3_gadget,
					"ignoring transition U3 -> Resume");
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

static void dwc3_gadget_suspend_interrupt(struct dwc3 *dwc,
			unsigned int evtinfo)
{
	enum dwc3_link_state    next = evtinfo & DWC3_LINK_STATE_MASK;

	dbg_event(0xFF, "SUSPEND INT", 0);
	dev_dbg(dwc->dev, "%s Entry to %d\n", __func__, next);

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
		dwc3_notify_event(dwc, DWC3_CONTROLLER_NOTIFY_OTG_EVENT);
	}

	dwc->link_state = next;
	dwc3_trace(trace_dwc3_gadget, "link state %d", dwc->link_state);
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
			dwc3_trace(trace_dwc3_gadget, "End of Periodic Frame");
			dwc->dbg_gadget_events.eopf++;
		} else {
			dwc3_trace(trace_dwc3_gadget, "U3/L1-L2 Suspend Event");
			dbg_event(0xFF, "GAD SUS", 0);
			dwc->dbg_gadget_events.suspend++;
			/*
			 * Ignore suspend event until the gadget enters into
			 * USB_STATE_CONFIGURED state.
			 */
			if (dwc->gadget.state >= USB_STATE_CONFIGURED)
				dwc3_gadget_suspend_interrupt(dwc,
						event->event_info);
		}
		break;
	case DWC3_DEVICE_EVENT_SOF:
		dwc3_trace(trace_dwc3_gadget, "Start of Periodic Frame");
		dwc->dbg_gadget_events.sof++;
		break;
	case DWC3_DEVICE_EVENT_ERRATIC_ERROR:
		dwc3_trace(trace_dwc3_gadget, "Erratic Error");
		dbg_event(0xFF, "ERROR", 0);
		dwc->dbg_gadget_events.erratic_error++;
		break;
	case DWC3_DEVICE_EVENT_CMD_CMPL:
		dwc3_trace(trace_dwc3_gadget, "Command Complete");
		dwc->dbg_gadget_events.cmdcmplt++;
		break;
	case DWC3_DEVICE_EVENT_OVERFLOW:
		dwc3_trace(trace_dwc3_gadget, "Overflow");
		dwc->dbg_gadget_events.overflow++;
		break;
	default:
		dev_WARN(dwc->dev, "UNKNOWN IRQ %d\n", event->type);
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
		dbg_event(0xFF, "SKIP_EVT", event->raw);
		return;
	}

	/* If run/stop is cleared don't process any more events */
	if (!dwc->pullups_connected) {
		dbg_event(0xFF, "SKIP_EVT_PULLUP", event->raw);
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

static irqreturn_t dwc3_process_event_buf(struct dwc3 *dwc)
{
	struct dwc3_event_buffer *evt;
	irqreturn_t ret = IRQ_NONE;
	int left;
	u32 reg;

	evt = dwc->ev_buf;
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
			dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), left);
			if (dwc3_notify_event(dwc, DWC3_CONTROLLER_ERROR_EVENT))
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
	}

	dwc->bh_handled_evt_cnt[dwc->bh_dbg_index] += (evt->count / 4);
	evt->count = 0;
	evt->flags &= ~DWC3_EVENT_PENDING;
	ret = IRQ_HANDLED;

	/* Unmask interrupt */
	reg = dwc3_readl(dwc->regs, DWC3_GEVNTSIZ(0));
	reg &= ~DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(0), reg);

	if (dwc->imod_interval)
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0),
				DWC3_GEVNTCOUNT_EHB);

	return ret;
}

void dwc3_bh_work(struct work_struct *w)
{
	struct dwc3 *dwc = container_of(w, struct dwc3, bh_work);

	 pm_runtime_get_sync(dwc->dev);
	 dwc3_thread_interrupt(dwc->irq, dwc);
	 pm_runtime_put(dwc->dev);
}

static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc)
{
	struct dwc3 *dwc = _dwc;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	unsigned int temp_time;
	ktime_t start_time;

	start_time = ktime_get();

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->bh_handled_evt_cnt[dwc->bh_dbg_index] = 0;

	ret = dwc3_process_event_buf(dwc);

	spin_unlock_irqrestore(&dwc->lock, flags);

	temp_time = ktime_to_us(ktime_sub(ktime_get(), start_time));
	dwc->bh_completion_time[dwc->bh_dbg_index] = temp_time;
	dwc->bh_dbg_index = (dwc->bh_dbg_index + 1) % 10;

	return ret;
}

static irqreturn_t dwc3_check_event_buf(struct dwc3 *dwc)
{
	struct dwc3_event_buffer *evt;
	u32 count;
	u32 reg;

	evt = dwc->ev_buf;

	/*
	 * With PCIe legacy interrupt, test shows that top-half irq handler can
	 * be called again after HW interrupt deassertion. Check if bottom-half
	 * irq event handler completes before caching new event to prevent
	 * losing events.
	 */
	if (evt->flags & DWC3_EVENT_PENDING)
		return IRQ_HANDLED;

	count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(0));
	count &= DWC3_GEVNTCOUNT_MASK;
	if (!count)
		return IRQ_NONE;

	if (count > evt->length) {
		dev_err(dwc->dev, "HUGE_EVCNT(%d)", count);
		dbg_event(0xFF, "HUGE_EVCNT", count);
		evt->lpos = (evt->lpos + count) % DWC3_EVENT_BUFFERS_SIZE;
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), count);
		return IRQ_HANDLED;
	}

	evt->count = count;
	evt->flags |= DWC3_EVENT_PENDING;

	/* Mask interrupt */
	reg = dwc3_readl(dwc->regs, DWC3_GEVNTSIZ(0));
	reg |= DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(0), reg);

	dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), count);

	return IRQ_WAKE_THREAD;
}

irqreturn_t dwc3_interrupt(int irq, void *_dwc)
{
	struct dwc3			*dwc = _dwc;
	irqreturn_t			ret = IRQ_NONE;
	irqreturn_t			status;
	unsigned int			temp_cnt = 0;
	ktime_t				start_time;

	start_time = ktime_get();
	dwc->irq_cnt++;

	/* controller reset is still pending */
	if (dwc->err_evt_seen)
		return IRQ_HANDLED;

	status = dwc3_check_event_buf(dwc);
	if (status == IRQ_WAKE_THREAD)
		ret = status;

	dwc->irq_start_time[dwc->irq_dbg_index] = start_time;
	dwc->irq_completion_time[dwc->irq_dbg_index] =
		ktime_us_delta(ktime_get(), start_time);
	dwc->irq_event_count[dwc->irq_dbg_index] = temp_cnt / 4;
	dwc->irq_dbg_index = (dwc->irq_dbg_index + 1) % MAX_INTR_STATS;

	if (ret == IRQ_WAKE_THREAD)
		queue_work(dwc->dwc_wq, &dwc->bh_work);

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
	int ret, irq;
	struct platform_device *dwc3_pdev = to_platform_device(dwc->dev);

	irq = platform_get_irq_byname(dwc3_pdev, "peripheral");
	if (irq == -EPROBE_DEFER)
		return irq;

	if (irq <= 0) {
		irq = platform_get_irq_byname(dwc3_pdev, "dwc_usb3");
		if (irq == -EPROBE_DEFER)
			return irq;

		if (irq <= 0) {
			irq = platform_get_irq(dwc3_pdev, 0);
			if (irq <= 0) {
				if (irq != -EPROBE_DEFER) {
					dev_err(dwc->dev,
						"missing peripheral IRQ\n");
				}
				if (!irq)
					irq = -EINVAL;
				return irq;
			}
		}
	}

	dwc->irq_gadget = irq;

	INIT_WORK(&dwc->wakeup_work, dwc3_gadget_wakeup_work);

	dwc->ctrl_req = dma_alloc_coherent(dwc->sysdev, sizeof(*dwc->ctrl_req),
			&dwc->ctrl_req_addr, GFP_KERNEL);
	if (!dwc->ctrl_req) {
		dev_err(dwc->dev, "failed to allocate ctrl request\n");
		ret = -ENOMEM;
		goto err0;
	}

	dwc->ep0_trb = dma_alloc_coherent(dwc->sysdev,
					  sizeof(*dwc->ep0_trb) * 2,
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

	dwc->ep0_bounce = dma_alloc_coherent(dwc->sysdev,
			DWC3_EP0_BOUNCE_SIZE, &dwc->ep0_bounce_addr,
			GFP_KERNEL);
	if (!dwc->ep0_bounce) {
		dev_err(dwc->dev, "failed to allocate ep0 bounce buffer\n");
		ret = -ENOMEM;
		goto err3;
	}

	dwc->zlp_buf = kzalloc(DWC3_ZLP_BUF_SIZE, GFP_KERNEL);
	if (!dwc->zlp_buf) {
		ret = -ENOMEM;
		goto err4;
	}

	dwc->gadget.ops			= &dwc3_gadget_ops;
	dwc->gadget.speed		= USB_SPEED_UNKNOWN;
	dwc->gadget.sg_supported	= true;
	dwc->gadget.name		= "dwc3-gadget";
	dwc->gadget.is_otg		= dwc->dr_mode == USB_DR_MODE_OTG;

	/*
	 * FIXME We might be setting max_speed to <SUPER, however versions
	 * <2.20a of dwc3 have an issue with metastability (documented
	 * elsewhere in this driver) which tells us we can't set max speed to
	 * anything lower than SUPER.
	 *
	 * Because gadget.max_speed is only used by composite.c and function
	 * drivers (i.e. it won't go into dwc3's registers) we are allowing this
	 * to happen so we avoid sending SuperSpeed Capability descriptor
	 * together with our BOS descriptor as that could confuse host into
	 * thinking we can handle super speed.
	 *
	 * Note that, in fact, we won't even support GetBOS requests when speed
	 * is less than super speed because we don't have means, yet, to tell
	 * composite.c that we are USB 2.0 + LPM ECN.
	 */
	if (dwc->revision < DWC3_REVISION_220A)
		dwc3_trace(trace_dwc3_gadget,
				"Changing max_speed on rev %08x",
				dwc->revision);

	dwc->gadget.max_speed		= dwc->maximum_speed;

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
		goto err5;

	ret = usb_add_gadget_udc(dwc->dev, &dwc->gadget);
	if (ret) {
		dev_err(dwc->dev, "failed to register udc\n");
		goto err5;
	}

	if (!dwc->is_drd) {
		pm_runtime_no_callbacks(&dwc->gadget.dev);
		pm_runtime_set_active(&dwc->gadget.dev);
		pm_runtime_enable(&dwc->gadget.dev);
		pm_runtime_get(&dwc->gadget.dev);
	}

	return 0;

err5:
	kfree(dwc->zlp_buf);

err4:
	dwc3_gadget_free_endpoints(dwc);
	dma_free_coherent(dwc->sysdev, DWC3_EP0_BOUNCE_SIZE,
			dwc->ep0_bounce, dwc->ep0_bounce_addr);

err3:
	kfree(dwc->setup_buf);

err2:
	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ep0_trb) * 2,
			dwc->ep0_trb, dwc->ep0_trb_addr);

err1:
	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ctrl_req),
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

	dma_free_coherent(dwc->sysdev, DWC3_EP0_BOUNCE_SIZE,
			dwc->ep0_bounce, dwc->ep0_bounce_addr);

	kfree(dwc->setup_buf);
	kfree(dwc->zlp_buf);

	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ep0_trb) * 2,
			dwc->ep0_trb, dwc->ep0_trb_addr);

	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ctrl_req),
			dwc->ctrl_req, dwc->ctrl_req_addr);
}

int dwc3_gadget_suspend(struct dwc3 *dwc)
{
	int ret;

	if (!dwc->gadget_driver)
		return 0;

	ret = dwc3_gadget_run_stop(dwc, false, false);
	if (ret < 0)
		return ret;

	dwc3_disconnect_gadget(dwc);
	__dwc3_gadget_stop(dwc);

	return 0;
}

int dwc3_gadget_resume(struct dwc3 *dwc)
{
	int			ret;

	if (!dwc->gadget_driver)
		return 0;

	ret = __dwc3_gadget_start(dwc);
	if (ret < 0)
		goto err0;

	ret = dwc3_gadget_run_stop(dwc, true, false);
	if (ret < 0)
		goto err1;

	return 0;

err1:
	__dwc3_gadget_stop(dwc);

err0:
	return ret;
}

void dwc3_gadget_process_pending_events(struct dwc3 *dwc)
{
	if (dwc->pending_events) {
		dwc3_interrupt(dwc->irq_gadget, dwc->ev_buf);
		dwc->pending_events = false;
		enable_irq(dwc->irq_gadget);
	}
}
