/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/msm_hsusb.h>

#include "core.h"
#include "gadget.h"

/**
 *  USB DBM Hardware registers.
 *
 */
#define DBM_EP_CFG(n)		(0x00 + 4 * (n))
#define DBM_DATA_FIFO(n)	(0x10 + 4 * (n))
#define DBM_DATA_FIFO_SIZE(n)	(0x20 + 4 * (n))
#define DBM_DATA_FIFO_EN	(0x30)
#define DBM_GEVNTADR		(0x34)
#define DBM_GEVNTSIZ		(0x38)
#define DBM_DBG_CNFG		(0x3C)
#define DBM_HW_TRB0_EP(n)	(0x40 + 4 * (n))
#define DBM_HW_TRB1_EP(n)	(0x50 + 4 * (n))
#define DBM_HW_TRB2_EP(n)	(0x60 + 4 * (n))
#define DBM_HW_TRB3_EP(n)	(0x70 + 4 * (n))
#define DBM_PIPE_CFG		(0x80)
#define DBM_SOFT_RESET		(0x84)

/**
 *  USB DBM  Hardware registers bitmask.
 *
 */
/* DBM_EP_CFG */
#define DBM_EN_EP		0x00000000
#define DBM_USB3_EP_NUM		0x0000003E
#define DBM_BAM_PIPE_NUM	0x000000C0
#define DBM_PRODUCER		0x00000100
#define DBM_DISABLE_WB		0x00000200
#define DBM_INT_RAM_ACC		0x00000400

/* DBM_DATA_FIFO_SIZE */
#define DBM_DATA_FIFO_SIZE_MASK	0x0000ffff

/* DBM_GEVNTSIZ */
#define DBM_GEVNTSIZ_MASK	0x0000ffff

/* DBM_DBG_CNFG */
#define DBM_ENABLE_IOC_MASK	0x0000000f

/* DBM_SOFT_RESET */
#define DBM_SFT_RST_EP0		0x00000001
#define DBM_SFT_RST_EP1		0x00000002
#define DBM_SFT_RST_EP2		0x00000004
#define DBM_SFT_RST_EP3		0x00000008
#define DBM_SFT_RST_EPS		0x0000000F
#define DBM_SFT_RST		0x80000000

#define DBM_MAX_EPS		4

struct dwc3_msm_req_complete {
	struct list_head list_item;
	struct usb_request *req;
	void (*orig_complete)(struct usb_ep *ep,
			      struct usb_request *req);
};

struct dwc3_msm {
	struct platform_device *dwc3;
	struct device *dev;
	void __iomem *base;
	u32 resource_size;
	int dbm_num_eps;
	u8 ep_num_mapping[DBM_MAX_EPS];
	const struct usb_ep_ops *original_ep_ops[DWC3_ENDPOINTS_NUM];
	struct list_head req_complete_list;
};

static struct dwc3_msm *context;

/**
 *
 * Read register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg(void *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	return val;
}

/**
 * Read register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 *
 * @return u32
 */
static inline u32 dwc3_msm_read_reg_field(void *base,
					  u32 offset,
					  const u32 mask)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 val = ioread32(base + offset);
	val &= mask;		/* clear other bits */
	val >>= shift;
	return val;
}

/**
 *
 * Write register with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

/**
 * Write register masked field with debug info.
 *
 * @base - DWC3 base virtual address.
 * @offset - register offset.
 * @mask - register bitmask.
 * @val - value to write.
 *
 */
static inline void dwc3_msm_write_reg_field(void *base, u32 offset,
					    const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = ioread32(base + offset);

	tmp &= ~mask;		/* clear written bits */
	val = tmp | (val << shift);
	iowrite32(val, base + offset);
}

/**
 * Return DBM EP number which is not already configured.
 *
 */
static int dwc3_msm_find_avail_dbm_ep(void)
{
	int i;

	for (i = 0; i < context->dbm_num_eps; i++)
		if (!context->ep_num_mapping[i])
			return i;

	return -ENODEV; /* Not found */
}

/**
 * Return DBM EP number according to usb endpoint number.
 *
 */
static int dwc3_msm_find_matching_dbm_ep(u8 usb_ep)
{
	int i;

	for (i = 0; i < context->dbm_num_eps; i++)
		if (context->ep_num_mapping[i] == usb_ep)
			return i;

	return -ENODEV; /* Not found */
}

/**
 * Return number of configured DBM endpoints.
 *
 */
static int dwc3_msm_configured_dbm_ep_num(void)
{
	int i;
	int count = 0;

	for (i = 0; i < context->dbm_num_eps; i++)
		if (context->ep_num_mapping[i])
			count++;

	return count;
}

/**
 * Configure the DBM with the USB3 core event buffer.
 * This function is called by the SNPS UDC upon initialization.
 *
 * @addr - address of the event buffer.
 * @size - size of the event buffer.
 *
 */
static int dwc3_msm_event_buffer_config(u32 addr, u16 size)
{
	dev_dbg(context->dev, "%s\n", __func__);

	dwc3_msm_write_reg(context->base, DBM_GEVNTADR, addr);
	dwc3_msm_write_reg_field(context->base, DBM_GEVNTSIZ,
		DBM_GEVNTSIZ_MASK, size);

	return 0;
}

/**
 * Reset the DBM registers upon initialization.
 *
 */
static int dwc3_msm_dbm_soft_reset(void)
{
	dev_dbg(context->dev, "%s\n", __func__);

	dwc3_msm_write_reg_field(context->base, DBM_SOFT_RESET,
		DBM_SFT_RST, 1);

	return 0;
}

/**
 * Soft reset specific DBM ep.
 * This function is called by the function driver upon events
 * such as transfer aborting, USB re-enumeration and USB
 * disconnection.
 *
 * @dbm_ep - DBM ep number.
 * @enter_reset - should we enter a reset state or get out of it.
 *
 */
static int dwc3_msm_dbm_ep_soft_reset(u8 dbm_ep, bool enter_reset)
{
	dev_dbg(context->dev, "%s\n", __func__);

	if (dbm_ep >= context->dbm_num_eps) {
		dev_err(context->dev,
				"%s: Invalid DBM ep index\n", __func__);
		return -ENODEV;
	}

	if (enter_reset) {
		dwc3_msm_write_reg_field(context->base, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS, 1 << dbm_ep);
	} else {
		dwc3_msm_write_reg_field(context->base, DBM_SOFT_RESET,
			DBM_SFT_RST_EPS, 0);
	}

	return 0;
}

/**
 * Configure a USB DBM ep to work in BAM mode.
 *
 *
 * @usb_ep - USB physical EP number.
 * @producer - producer/consumer.
 * @disable_wb - disable write back to system memory.
 * @internal_mem - use internal USB memory for data fifo.
 * @ioc - enable interrupt on completion.
 *
 * @return int - DBM ep number.
 */
static int dwc3_msm_dbm_ep_config(u8 usb_ep, u8 bam_pipe,
				  bool producer, bool disable_wb,
				  bool internal_mem, bool ioc)
{
	u8 dbm_ep;
	u8 ioc_mask;

	dev_dbg(context->dev, "%s\n", __func__);

	dbm_ep = dwc3_msm_find_avail_dbm_ep();
	if (dbm_ep < 0) {
		dev_err(context->dev, "%s: No more DBM eps\n", __func__);
		return -ENODEV;
	}

	context->ep_num_mapping[dbm_ep] = usb_ep;

	/* First, reset the dbm endpoint */
	dwc3_msm_dbm_ep_soft_reset(dbm_ep, false);

	ioc_mask = dwc3_msm_read_reg_field(context->base, DBM_DBG_CNFG,
		DBM_ENABLE_IOC_MASK);
	ioc_mask &= ~(ioc << dbm_ep); /* Clear ioc bit for dbm_ep */
	/* Set ioc bit for dbm_ep if needed */
	dwc3_msm_write_reg_field(context->base, DBM_DBG_CNFG,
		DBM_ENABLE_IOC_MASK, ioc_mask | (ioc << dbm_ep));

	dwc3_msm_write_reg(context->base, DBM_EP_CFG(dbm_ep),
		producer | disable_wb | internal_mem);
	dwc3_msm_write_reg_field(context->base, DBM_EP_CFG(dbm_ep),
		DBM_USB3_EP_NUM, usb_ep);
	dwc3_msm_write_reg_field(context->base, DBM_EP_CFG(dbm_ep),
		DBM_BAM_PIPE_NUM, bam_pipe);
	dwc3_msm_write_reg_field(context->base, DBM_EP_CFG(dbm_ep),
		DBM_EN_EP, 1);

	return dbm_ep;
}

/**
 * Configure a USB DBM ep to work in normal mode.
 *
 * @usb_ep - USB ep number.
 *
 */
static int dwc3_msm_dbm_ep_unconfig(u8 usb_ep)
{
	u8 dbm_ep;

	dev_dbg(context->dev, "%s\n", __func__);

	dbm_ep = dwc3_msm_find_matching_dbm_ep(usb_ep);

	if (dbm_ep < 0) {
		dev_err(context->dev,
				"%s: Invalid usb ep index\n", __func__);
		return -ENODEV;
	}

	context->ep_num_mapping[dbm_ep] = 0;

	dwc3_msm_write_reg(context->base, DBM_EP_CFG(dbm_ep), 0);

	/* Reset the dbm endpoint */
	dwc3_msm_dbm_ep_soft_reset(dbm_ep, true);

	return 0;
}

/**
 * Configure the DBM with the BAM's data fifo.
 * This function is called by the USB BAM Driver
 * upon initialization.
 *
 * @ep - pointer to usb endpoint.
 * @addr - address of data fifo.
 * @size - size of data fifo.
 *
 */
int msm_data_fifo_config(struct usb_ep *ep, u32 addr, u32 size)
{
	u8 dbm_ep;
	struct dwc3_ep *dep = to_dwc3_ep(ep);

	dev_dbg(context->dev, "%s\n", __func__);

	dbm_ep = dwc3_msm_find_matching_dbm_ep(dep->number);

	if (dbm_ep >= context->dbm_num_eps) {
		dev_err(context->dev,
				"%s: Invalid DBM ep index\n", __func__);
		return -ENODEV;
	}

	dwc3_msm_write_reg(context->base, DBM_DATA_FIFO(dbm_ep), addr);
	dwc3_msm_write_reg_field(context->base, DBM_DATA_FIFO_SIZE(dbm_ep),
		DBM_DATA_FIFO_SIZE_MASK, size);

	return 0;
}

/**
* Cleanups for msm endpoint on request complete.
*
* Also call original request complete.
*
* @usb_ep - pointer to usb_ep instance.
* @request - pointer to usb_request instance.
*
* @return int - 0 on success, negetive on error.
*/
static void dwc3_msm_req_complete_func(struct usb_ep *ep,
				       struct usb_request *request)
{
	struct dwc3_request *req = to_dwc3_request(request);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3_msm_req_complete *req_complete = NULL;

	/* Find original request complete function and remove it from list */
	list_for_each_entry(req_complete,
				&context->req_complete_list,
				list_item) {
		if (req_complete->req == request)
			break;
	}
	if (!req_complete || req_complete->req != request) {
		dev_err(dep->dwc->dev, "%s: could not find the request\n",
					__func__);
		return;
	}
	list_del(&req_complete->list_item);

	/*
	 * Release another one TRB to the pool since DBM queue took 2 TRBs
	 * (normal and link), and the dwc3/gadget.c :: dwc3_gadget_giveback
	 * released only one.
	 */
	if (req->queued)
		dep->busy_slot++;

	/* Unconfigure dbm ep */
	dwc3_msm_dbm_ep_unconfig(dep->number);

	/*
	 * If this is the last endpoint we unconfigured, than reset also
	 * the event buffers.
	 */
	if (0 == dwc3_msm_configured_dbm_ep_num())
		dwc3_msm_event_buffer_config(0, 0);

	/*
	 * Call original complete function, notice that dwc->lock is already
	 * taken by the caller of this function (dwc3_gadget_giveback()).
	 */
	request->complete = req_complete->orig_complete;
	request->complete(ep, request);

	kfree(req_complete);
}

/**
* Helper function.
* See the header of the dwc3_msm_ep_queue function.
*
* @dwc3_ep - pointer to dwc3_ep instance.
* @req - pointer to dwc3_request instance.
*
* @return int - 0 on success, negetive on error.
*/
static int __dwc3_msm_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3_trb_hw *trb_hw;
	struct dwc3_trb_hw *trb_link_hw;
	struct dwc3_trb trb;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd;
	int ret = 0;

	if ((req->request.udc_priv & MSM_IS_FINITE_TRANSFER) &&
	    (req->request.length > 0)) {
		/* Map the request to a DMA. */
		dwc3_map_buffer_to_dma(req);
	}

	/* We push the request to the dep->req_queued list to indicate that
	 * this request is issued with start transfer. The request will be out
	 * from this list in 2 cases. The first is that the transfer will be
	 * completed (not if the transfer is endless using a circular TRBs with
	 * with link TRB). The second case is an option to do stop stransfer,
	 * this can be initiated by the function driver when calling dequeue.
	 */
	req->queued = true;
	list_add_tail(&req->list, &dep->req_queued);

	/* First, prepare a normal TRB, point to the fake buffer */
	trb_hw = &dep->trb_pool[dep->free_slot & DWC3_TRB_MASK];
	dep->free_slot++;
	memset(&trb, 0, sizeof(trb));

	req->trb = trb_hw;

	trb.bplh = req->request.dma;
	trb.lst = 0;
	trb.trbctl = DWC3_TRBCTL_NORMAL;
	trb.length = req->request.length;
	trb.hwo = true;

	dwc3_trb_to_hw(&trb, trb_hw);
	req->trb_dma = dep->trb_pool_dma;

	/* Second, prepare a Link TRB that points to the first TRB*/
	trb_link_hw = &dep->trb_pool[dep->free_slot & DWC3_TRB_MASK];
	dep->free_slot++;
	memset(&trb, 0, sizeof(trb));

	trb.bplh = dep->trb_pool_dma;
	trb.trbctl = DWC3_TRBCTL_LINK_TRB;
	trb.hwo = true;

	dwc3_trb_to_hw(&trb, trb_link_hw);

	/*
	 * Now start the transfer
	 */
	memset(&params, 0, sizeof(params));
	params.param0 = upper_32_bits(req->trb_dma);
	params.param1 = lower_32_bits(req->trb_dma);
	cmd = DWC3_DEPCMD_STARTTRANSFER;
	ret = dwc3_send_gadget_ep_cmd(dep->dwc, dep->number, cmd, &params);
	if (ret < 0) {
		dev_dbg(dep->dwc->dev,
			"%s: failed to send STARTTRANSFER command\n",
			__func__);

		dwc3_unmap_buffer_from_dma(req);
		list_del(&req->list);
		return ret;
	}

	return ret;
}

/**
* Queue a usb request to the DBM endpoint.
* This function should be called after the endpoint
* was enabled by the ep_enable.
*
* This function prepares special structure of TRBs which
* is familier with the DBM HW, so it will possible to use
* this endpoint in DBM mode.
*
* The TRBs prepared by this function, is one normal TRB
* which point to a fake buffer, followed by a link TRB
* that points to the first TRB.
*
* The API of this function follow the regular API of
* usb_ep_queue (see usb_ep_ops in include/linuk/usb/gadget.h).
*
* @usb_ep - pointer to usb_ep instance.
* @request - pointer to usb_request instance.
* @gfp_flags - possible flags.
*
* @return int - 0 on success, negetive on error.
*/
static int dwc3_msm_ep_queue(struct usb_ep *ep,
			     struct usb_request *request, gfp_t gfp_flags)
{
	struct dwc3_request *req = to_dwc3_request(request);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_msm_req_complete *req_complete;
	unsigned long flags;
	int ret = 0;
	u8 bam_pipe;
	bool producer;
	bool disable_wb;
	bool internal_mem;
	bool ioc;

	if (!(request->udc_priv & MSM_SPS_MODE)) {
		/* Not SPS mode, call original queue */
		dev_vdbg(dwc->dev, "%s: not sps mode, use regular queue\n",
					__func__);

		return (context->original_ep_ops[dep->number])->queue(ep,
								request,
								gfp_flags);
	}

	if (!dep->endpoint.desc) {
		dev_err(dwc->dev,
			"%s: trying to queue request %p to disabled ep %s\n",
			__func__, request, ep->name);
		return -EPERM;
	}

	if (dep->number == 0 || dep->number == 1) {
		dev_err(dwc->dev,
			"%s: trying to queue dbm request %p to control ep %s\n",
			__func__, request, ep->name);
		return -EPERM;
	}

	if (dep->free_slot > 0 || dep->busy_slot > 0 ||
		!list_empty(&dep->request_list) ||
		!list_empty(&dep->req_queued)) {

		dev_err(dwc->dev,
			"%s: trying to queue dbm request %p tp ep %s\n",
			__func__, request, ep->name);
		return -EPERM;
	}

	/*
	 * Override req->complete function, but before doing that,
	 * store it's original pointer in the req_complete_list.
	 */
	req_complete = kzalloc(sizeof(*req_complete), GFP_KERNEL);
	if (!req_complete) {
		dev_err(dep->dwc->dev, "%s: not enough memory\n", __func__);
		return -ENOMEM;
	}
	req_complete->req = request;
	req_complete->orig_complete = request->complete;
	list_add_tail(&req_complete->list_item, &context->req_complete_list);
	request->complete = dwc3_msm_req_complete_func;

	/*
	 * Configure dbm event buffers if this is the first
	 * dbm endpoint we about to configure.
	 */
	if (0 == dwc3_msm_configured_dbm_ep_num())
		dwc3_msm_event_buffer_config(dwc->ev_buffs[0]->dma,
					     dwc->ev_buffs[0]->length);

	/*
	 * Configure the DBM endpoint
	 */
	bam_pipe = (request->udc_priv & MSM_PIPE_ID_MASK);
	producer = ((request->udc_priv & MSM_PRODUCER) ? true : false);
	disable_wb = ((request->udc_priv & MSM_DISABLE_WB) ? true : false);
	internal_mem = ((request->udc_priv & MSM_INTERNAL_MEM) ? true : false);
	ioc = ((request->udc_priv & MSM_ETD_IOC) ? true : false);

	ret = dwc3_msm_dbm_ep_config(dep->number,
					bam_pipe, producer,
					disable_wb, internal_mem, ioc);
	if (ret < 0) {
		dev_err(context->dev,
			"error %d after calling dwc3_msm_dbm_ep_config\n",
			ret);
		return ret;
	}

	dev_vdbg(dwc->dev, "%s: queing request %p to ep %s length %d\n",
			__func__, request, ep->name, request->length);

	/*
	 * We must obtain the lock of the dwc3 core driver,
	 * including disabling interrupts, so we will be sure
	 * that we are the only ones that configure the HW device
	 * core and ensure that we queuing the request will finish
	 * as soon as possible so we will release back the lock.
	 */
	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_msm_ep_queue(dep, req);
	spin_unlock_irqrestore(&dwc->lock, flags);
	if (ret < 0) {
		dev_err(context->dev,
			"error %d after calling __dwc3_msm_ep_queue\n", ret);
		return ret;
	}

	return 0;
}

/**
 * Configure MSM endpoint.
 * This function do specific configurations
 * to an endpoint which need specific implementaion
 * in the MSM architecture.
 *
 * This function should be called by usb function/class
 * layer which need a support from the specific MSM HW
 * which wrap the USB3 core. (like DBM specific endpoints)
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negetive on error.
 */
int msm_ep_config(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct usb_ep_ops *new_ep_ops;

	/* Save original ep ops for future restore*/
	if (context->original_ep_ops[dep->number]) {
		dev_err(context->dev,
			"ep [%s,%d] already configured as msm endpoint\n",
			ep->name, dep->number);
		return -EPERM;
	}
	context->original_ep_ops[dep->number] = ep->ops;

	/* Set new usb ops as we like */
	new_ep_ops = kzalloc(sizeof(struct usb_ep_ops), GFP_KERNEL);
	if (!new_ep_ops) {
		dev_err(context->dev,
			"%s: unable to allocate mem for new usb ep ops\n",
			__func__);
		return -ENOMEM;
	}
	(*new_ep_ops) = (*ep->ops);
	new_ep_ops->queue = dwc3_msm_ep_queue;
	ep->ops = new_ep_ops;

	/*
	 * Do HERE more usb endpoint configurations
	 * which are specific to MSM.
	 */

	return 0;
}
EXPORT_SYMBOL(msm_ep_config);

/**
 * Un-configure MSM endpoint.
 * Tear down configurations done in the
 * dwc3_msm_ep_config function.
 *
 * @ep - a pointer to some usb_ep instance
 *
 * @return int - 0 on success, negetive on error.
 */
int msm_ep_unconfig(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct usb_ep_ops *old_ep_ops;

	/* Restore original ep ops */
	if (!context->original_ep_ops[dep->number]) {
		dev_err(context->dev,
			"ep [%s,%d] was not configured as msm endpoint\n",
			ep->name, dep->number);
		return -EINVAL;
	}
	old_ep_ops = (struct usb_ep_ops	*)ep->ops;
	ep->ops = context->original_ep_ops[dep->number];
	context->original_ep_ops[dep->number] = NULL;
	kfree(old_ep_ops);

	/*
	 * Do HERE more usb endpoint un-configurations
	 * which are specific to MSM.
	 */

	return 0;
}
EXPORT_SYMBOL(msm_ep_unconfig);

static int __devinit dwc3_msm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct platform_device *dwc3;
	struct dwc3_msm *msm;
	struct resource *res;
	int ret = 0;

	msm = devm_kzalloc(&pdev->dev, sizeof(*msm), GFP_KERNEL);
	if (!msm) {
		dev_err(&pdev->dev, "not enough memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, msm);
	context = msm;

	INIT_LIST_HEAD(&msm->req_complete_list);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory base resource\n");
		return -ENODEV;
	}

	msm->base = devm_ioremap_nocache(&pdev->dev, res->start,
		resource_size(res));
	if (!msm->base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		return -ENODEV;
	}

	dwc3 = platform_device_alloc("dwc3-msm", -1);
	if (!dwc3) {
		dev_err(&pdev->dev, "couldn't allocate dwc3 device\n");
		return -ENOMEM;
	}

	dma_set_coherent_mask(&dwc3->dev, pdev->dev.coherent_dma_mask);

	dwc3->dev.parent = &pdev->dev;
	dwc3->dev.dma_mask = pdev->dev.dma_mask;
	dwc3->dev.dma_parms = pdev->dev.dma_parms;
	msm->resource_size = resource_size(res);
	msm->dev = &pdev->dev;
	msm->dwc3 = dwc3;

	if (of_property_read_u32(node, "qcom,dwc-usb3-msm-dbm-eps",
				 &msm->dbm_num_eps)) {
		dev_err(&pdev->dev,
			"unable to read platform data num of dbm eps\n");
		msm->dbm_num_eps = DBM_MAX_EPS;
	}

	if (msm->dbm_num_eps > DBM_MAX_EPS) {
		dev_err(&pdev->dev,
			"Driver doesn't support number of DBM EPs. "
			"max: %d, dbm_num_eps: %d\n",
			DBM_MAX_EPS, msm->dbm_num_eps);
		ret = -ENODEV;
		goto err1;
	}

	ret = platform_device_add_resources(dwc3, pdev->resource,
		pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to dwc3 device\n");
		goto err1;
	}

	ret = platform_device_add(dwc3);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dwc3 device\n");
		goto err1;
	}

	/* Reset the DBM */
	dwc3_msm_dbm_soft_reset();

	return 0;

err1:
	platform_device_put(dwc3);

	return ret;
}

static int __devexit dwc3_msm_remove(struct platform_device *pdev)
{
	struct dwc3_msm	*msm = platform_get_drvdata(pdev);

	platform_device_unregister(msm->dwc3);

	return 0;
}

static const struct of_device_id of_dwc3_matach[] = {
	{
		.compatible = "qcom,dwc-usb3-msm",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_matach);

static struct platform_driver dwc3_msm_driver = {
	.probe		= dwc3_msm_probe,
	.remove		= __devexit_p(dwc3_msm_remove),
	.driver		= {
		.name	= "msm-dwc3",
		.of_match_table	= of_dwc3_matach,
	},
};

MODULE_LICENSE("GPLV2");
MODULE_DESCRIPTION("DesignWare USB3 MSM Glue Layer");

static int __devinit dwc3_msm_init(void)
{
	return platform_driver_register(&dwc3_msm_driver);
}
module_init(dwc3_msm_init);

static void __exit dwc3_msm_exit(void)
{
	platform_driver_unregister(&dwc3_msm_driver);
}
module_exit(dwc3_msm_exit);
