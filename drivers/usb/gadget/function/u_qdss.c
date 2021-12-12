// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb_bam.h>
#include <linux/dma-mapping.h>
#include <linux/usb/dwc3-msm.h>

#include "f_qdss.h"

#define NUM_EBC_IN_BUF	2

int alloc_hw_req(struct usb_ep *data_ep)
{
	struct usb_request *req = NULL;
	struct f_qdss *qdss = data_ep->driver_data;

	pr_debug("send_sps_req\n");

	req = usb_ep_alloc_request(data_ep, GFP_ATOMIC);
	if (!req) {
		pr_err("usb_ep_alloc_request failed\n");
		return -ENOMEM;
	}

	if (qdss->ch.ch_type == USB_EP_BAM)
		req->length = 32*1024;
	else
		req->length = NUM_EBC_IN_BUF * EBC_TRB_SIZE;
	qdss->endless_req = req;

	return 0;
}

static int enable_qdss_ebc_data_connection(struct f_qdss *qdss)
{
	int ret;

	ret = msm_ep_config(qdss->port.data, qdss->endless_req, 1);
	if (ret)
		pr_err("msm_ep_config failed\n");

	return ret;
}

static int enable_qdss_bam_data_connection(struct f_qdss *qdss)
{
	enum usb_ctrl		usb_bam_type;
	int			idx;
	struct usb_qdss_bam_connect_info bam_info;
	struct usb_gadget *gadget;
	struct device *dev;
	int ret;

	gadget = qdss->gadget;
	usb_bam_type = usb_bam_get_bam_type(gadget->name);
	dev = gadget->dev.parent;

	bam_info = qdss->bam_info;
	/* There is only one qdss pipe, so the pipe number can be set to 0 */
	idx = usb_bam_get_connection_idx(usb_bam_type, QDSS_P_BAM,
		PEER_PERIPHERAL_TO_USB, 0);
	if (idx < 0) {
		pr_err("%s: usb_bam_get_connection_idx failed\n", __func__);
		return idx;
	}


	ret = get_qdss_bam_info(usb_bam_type, idx,
			&bam_info.qdss_bam_phys,
			&bam_info.qdss_bam_size);
	if (ret) {
		pr_err("%s(): failed to get qdss bam info err(%d)\n",
							__func__, ret);
		return ret;
	}

	bam_info.qdss_bam_iova = dma_map_resource(dev->parent,
			bam_info.qdss_bam_phys, bam_info.qdss_bam_size,
			DMA_BIDIRECTIONAL, 0);
	if (!bam_info.qdss_bam_iova) {
		pr_err("dma_map_resource failed\n");
		return -ENOMEM;
	}

	usb_bam_alloc_fifos(usb_bam_type, idx);
	bam_info.data_fifo =
		kzalloc(sizeof(struct sps_mem_buffer), GFP_KERNEL);
	if (!bam_info.data_fifo) {
		usb_bam_free_fifos(usb_bam_type, idx);
		return -ENOMEM;
	}

	pr_debug("%s(): qdss_bam: iova:%lx p_addr:%lx size:%x\n",
			__func__, bam_info.qdss_bam_iova,
			(unsigned long)bam_info.qdss_bam_phys,
			bam_info.qdss_bam_size);

	get_bam2bam_connection_info(usb_bam_type, idx,
			&bam_info.usb_bam_pipe_idx,
			NULL, bam_info.data_fifo, NULL);

	msm_data_fifo_config(qdss->port.data,
		bam_info.data_fifo->iova,
		bam_info.data_fifo->size,
		bam_info.usb_bam_pipe_idx);

	ret = msm_ep_config(qdss->port.data, qdss->endless_req,
				MSM_SPS_MODE | MSM_DISABLE_WB |
				qdss->bam_info.usb_bam_pipe_idx);
	if (ret)
		pr_err("msm_ep_config failed\n");

	ret = usb_bam_connect(usb_bam_type, idx,
				&(bam_info.usb_bam_pipe_idx),
				bam_info.qdss_bam_iova);

	return ret;
}

static int disable_qdss_bam_data_connection(struct f_qdss *qdss)
{
	enum usb_ctrl		usb_bam_type;
	int			idx;
	struct usb_qdss_bam_connect_info bam_info;
	struct device *dev;
	struct usb_gadget *gadget;
	int ret;

	gadget = qdss->gadget;
	dev = gadget->dev.parent;

	usb_bam_type = usb_bam_get_bam_type(gadget->name);
	bam_info = qdss->bam_info;
	/* There is only one qdss pipe, so the pipe number can be set to 0 */
	idx = usb_bam_get_connection_idx(usb_bam_type, QDSS_P_BAM,
		PEER_PERIPHERAL_TO_USB, 0);
	ret = usb_bam_disconnect_pipe(usb_bam_type, idx);
	if (ret)
		pr_err("usb_bam_disconnection error\n");
	dma_unmap_resource(dev->parent, bam_info.qdss_bam_iova,
			bam_info.qdss_bam_size, DMA_BIDIRECTIONAL, 0);
	usb_bam_free_fifos(usb_bam_type, idx);
	kfree(bam_info.data_fifo);

	return ret;
}

int set_qdss_data_connection(struct f_qdss *qdss, int enable)
{
	struct usb_gadget *gadget;
	struct device *dev;
	int ret = 0;

	if (!qdss) {
		pr_err("%s: qdss ptr is NULL\n", __func__);
		return -EINVAL;
	}

	gadget = qdss->gadget;
	dev = gadget->dev.parent;

	pr_debug("%s ch_type:%d\n", __func__, qdss->ch.ch_type);
	if (enable) {
		if (qdss->ch.ch_type == USB_EP_BAM)
			ret = enable_qdss_bam_data_connection(qdss);
		else
			ret = enable_qdss_ebc_data_connection(qdss);
	} else {
		ret = msm_ep_unconfig(qdss->port.data);
		if (ret)
			pr_err("msm_ep_unconfig failed\n");

		if (qdss->ch.ch_type == USB_EP_BAM)
			ret = disable_qdss_bam_data_connection(qdss);
	}

	return ret;
}
