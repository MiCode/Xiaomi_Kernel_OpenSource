/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/termios.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/usb_bam.h>

#include "usb_gadget_xport.h"

#define IPA_N_PORTS 4
struct ipa_data_ch_info {
	struct usb_request	*rx_req;
	struct usb_request	*tx_req;
	unsigned long		flags;
	unsigned		id;
	enum transport_type	trans;
	enum gadget_type	gtype;
	bool			is_connected;
	unsigned		port_num;
	spinlock_t		port_lock;

	struct work_struct	connect_w;
	struct work_struct	disconnect_w;
	struct work_struct	suspend_w;
	struct work_struct	resume_w;

	u32			src_pipe_idx;
	u32			dst_pipe_idx;
	u8			src_connection_idx;
	u8			dst_connection_idx;
	enum usb_ctrl		usb_bam_type;
	struct gadget_ipa_port	*port_usb;
	struct usb_bam_connect_ipa_params	ipa_params;
};

static int n_ipa_ports;
static struct workqueue_struct *ipa_data_wq;
struct ipa_data_ch_info *ipa_data_ports[IPA_N_PORTS];
/**
 * ipa_data_endless_complete() - completion callback for endless TX/RX request
 * @ep: USB endpoint for which this completion happen
 * @req: USB endless request
 *
 * This completion is being called when endless (TX/RX) transfer is terminated
 * i.e. disconnect or suspend case.
 */
static void ipa_data_endless_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	pr_debug("%s: endless complete for(%s) with status: %d\n",
				__func__, ep->name, req->status);
}

/**
 * ipa_data_start_endless_xfer() - configure USB endpoint and
 * queue endless TX/RX request
 * @port: USB IPA data channel information
 * @in: USB endpoint direction i.e. true: IN(Device TX), false: OUT(Device RX)
 *
 * It is being used to queue endless TX/RX request with UDC driver.
 * It does set required DBM endpoint configuration before queueing endless
 * TX/RX request.
 */
static void ipa_data_start_endless_xfer(struct ipa_data_ch_info *port, bool in)
{
	int status;

	if (!port->port_usb) {
		pr_err("%s(): port_usb is NULL.\n", __func__);
		return;
	}

	if (in) {
		pr_debug("%s: enqueue endless TX_REQ(IN)\n", __func__);
		status = usb_ep_queue(port->port_usb->in,
					port->tx_req, GFP_ATOMIC);
		if (status)
			pr_err("error enqueuing endless TX_REQ, %d\n", status);
	} else {
		pr_debug("%s: enqueue endless RX_REQ(OUT)\n", __func__);
		status = usb_ep_queue(port->port_usb->out,
					port->rx_req, GFP_ATOMIC);
		if (status)
			pr_err("error enqueuing endless RX_REQ, %d\n", status);
	}
}

/**
 * ipa_data_stop_endless_xfer() - terminate and dequeue endless TX/RX request
 * @port: USB IPA data channel information
 * @in: USB endpoint direction i.e. IN - Device TX, OUT - Device RX
 *
 * It is being used to terminate and dequeue endless TX/RX request with UDC
 * driver.
 */
static void ipa_data_stop_endless_xfer(struct ipa_data_ch_info *port, bool in)
{
	int status;

	if (!port->port_usb) {
		pr_err("%s(): port_usb is NULL.\n", __func__);
		return;
	}

	if (in) {
		pr_debug("%s: dequeue endless TX_REQ(IN)\n", __func__);
		status = usb_ep_dequeue(port->port_usb->in, port->tx_req);
		if (status)
			pr_err("error dequeueing endless TX_REQ, %d\n", status);
	} else {
		pr_debug("%s: dequeue endless RX_REQ(OUT)\n", __func__);
		status = usb_ep_dequeue(port->port_usb->out, port->rx_req);
		if (status)
			pr_err("error dequeueing endless RX_REQ, %d\n", status);
	}
}

/**
 * ipa_data_disconnect_work() - Perform USB IPA BAM disconnect
 * @w: disconnect work
 *
 * It is being schedule from ipa_data_disconnect() API when particular function
 * is being disable due to USB disconnect or USB composition switch is being
 * trigger . This API performs disconnect of USB BAM pipe, IPA BAM pipe and also
 * initiate USB IPA BAM pipe handshake for USB Disconnect sequence. Due to
 * handshake operation and involvement of SPS related APIs, this functioality
 * can't be used from atomic context.
 */
static void ipa_data_disconnect_work(struct work_struct *w)
{
	struct ipa_data_ch_info *port = container_of(w, struct ipa_data_ch_info,
								disconnect_w);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->is_connected) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_debug("Already disconnected.\n");
		return;
	}
	port->is_connected = false;
	pr_debug("%s(): prod_clnt_hdl:%d cons_clnt_hdl:%d\n", __func__,
			port->ipa_params.prod_clnt_hdl,
			port->ipa_params.cons_clnt_hdl);

	spin_unlock_irqrestore(&port->port_lock, flags);
	ret = usb_bam_disconnect_ipa(port->usb_bam_type, &port->ipa_params);
	if (ret)
		pr_err("usb_bam_disconnect_ipa failed: err:%d\n", ret);

	if (port->ipa_params.prod_clnt_hdl)
		usb_bam_free_fifos(port->usb_bam_type,
						port->dst_connection_idx);
	if (port->ipa_params.cons_clnt_hdl)
		usb_bam_free_fifos(port->usb_bam_type,
						port->src_connection_idx);

	pr_debug("%s(): disconnect work completed.\n", __func__);
}

/**
 * ipa_data_disconnect() - Restore USB ep operation and disable USB endpoint
 * @gp: USB gadget IPA Port
 * @port_num: Port num used by function driver which need to be disable
 *
 * It is being called from atomic context from gadget driver when particular
 * function is being disable due to USB cable disconnect or USB composition
 * switch is being trigger. This API performs restoring USB endpoint operation
 * and disable USB endpoint used for accelerated path.
 */
void ipa_data_disconnect(struct gadget_ipa_port *gp, u8 port_num)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	struct usb_gadget *gadget = NULL;

	pr_debug("dev:%pK port number:%d\n", gp, port_num);
	if (port_num >= n_ipa_ports) {
		pr_err("invalid ipa portno#%d\n", port_num);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}

	port = ipa_data_ports[port_num];
	if (!port) {
		pr_err("port %u is NULL", port_num);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb) {
		gadget = port->port_usb->cdev->gadget;
		port->port_usb->ipa_consumer_ep = -1;
		port->port_usb->ipa_producer_ep = -1;

		if (port->port_usb->in) {
			/*
			 * Disable endpoints.
			 * Unlocking is needed since disabling the eps might
			 * stop active transfers and therefore the request
			 * complete function will be called, where we try
			 * to obtain the spinlock as well.
			 */
			if (gadget_is_dwc3(gadget))
				msm_ep_unconfig(port->port_usb->in);
			spin_unlock_irqrestore(&port->port_lock, flags);
			usb_ep_disable(port->port_usb->in);
			spin_lock_irqsave(&port->port_lock, flags);
			port->port_usb->in->endless = false;
		}

		if (port->port_usb->out) {
			if (gadget_is_dwc3(gadget))
				msm_ep_unconfig(port->port_usb->out);
			spin_unlock_irqrestore(&port->port_lock, flags);
			usb_ep_disable(port->port_usb->out);
			spin_lock_irqsave(&port->port_lock, flags);
			port->port_usb->out->endless = false;
		}

		port->port_usb = NULL;
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
	queue_work(ipa_data_wq, &port->disconnect_w);
}

/**
 * configure_fifo() - Configure USB BAM Pipe's data FIFO
 * @idx: USB BAM Pipe index
 * @ep: USB endpoint
 *
 * This function configures USB BAM data fifo using fetched pipe configuraion
 * using provided index value. This function needs to used before starting
 * endless transfer.
 */
static void configure_fifo(enum usb_ctrl bam_type, u8 idx, struct usb_ep *ep)
{
	struct u_bam_data_connect_info bam_info;
	struct sps_mem_buffer data_fifo = {0};

	get_bam2bam_connection_info(bam_type, idx,
				&bam_info.usb_bam_pipe_idx,
				NULL, &data_fifo, NULL);
	msm_data_fifo_config(ep, data_fifo.phys_base, data_fifo.size,
			bam_info.usb_bam_pipe_idx);
}

/**
 * ipa_data_connect_work() - Perform USB IPA BAM connect
 * @w: connect work
 *
 * It is being schedule from ipa_data_connect() API when particular function
 * which is using USB IPA accelerated path. This API performs allocating request
 * for USB endpoint (tx/rx) for endless purpose, configure USB endpoint to be
 * used in accelerated path, connect of USB BAM pipe, IPA BAM pipe and also
 * initiate USB IPA BAM pipe handshake for connect sequence.
 */
static void ipa_data_connect_work(struct work_struct *w)
{
	struct ipa_data_ch_info *port = container_of(w, struct ipa_data_ch_info,
								connect_w);
	struct gadget_ipa_port	*gport;
	struct usb_gadget	*gadget = NULL;
	u32			sps_params;
	int			ret;
	unsigned long		flags;
	bool			is_ipa_disconnected = true;

	pr_debug("%s: Connect workqueue started", __func__);

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s(): port_usb is NULL.\n", __func__);
		return;
	}

	gport = port->port_usb;
	if (gport && gport->cdev)
		gadget = gport->cdev->gadget;

	if (!gadget) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s: gport is NULL.\n", __func__);
		return;
	}

	gport->ipa_consumer_ep = -1;
	gport->ipa_producer_ep = -1;
	if (gport->out) {
		port->rx_req = usb_ep_alloc_request(gport->out, GFP_ATOMIC);
		if (!port->rx_req) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s: failed to allocate rx_req\n", __func__);
			return;
		}
		port->rx_req->context = port;
		port->rx_req->complete = ipa_data_endless_complete;
		port->rx_req->length = 0;
		port->rx_req->no_interrupt = 1;
	}

	if (gport->in) {
		port->tx_req = usb_ep_alloc_request(gport->in, GFP_ATOMIC);
		if (!port->tx_req) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s: failed to allocate tx_req\n", __func__);
			goto free_rx_req;
		}
		port->tx_req->context = port;
		port->tx_req->complete = ipa_data_endless_complete;
		port->tx_req->length = 0;
		port->tx_req->no_interrupt = 1;
	}

	port->is_connected = true;
	spin_unlock_irqrestore(&port->port_lock, flags);

	/* update IPA Parameteres here. */
	port->ipa_params.usb_connection_speed = gadget->speed;
	if (gadget_is_dwc3(gadget))
		port->ipa_params.reset_pipe_after_lpm =
				msm_dwc3_reset_ep_after_lpm(gadget);
	port->ipa_params.skip_ep_cfg = true;
	port->ipa_params.keep_ipa_awake = true;
	port->ipa_params.cons_clnt_hdl = -1;
	port->ipa_params.prod_clnt_hdl = -1;


	if (gport->out) {
		usb_bam_alloc_fifos(port->usb_bam_type,
						port->src_connection_idx);

		if (gadget_is_dwc3(gadget)) {
			sps_params = MSM_SPS_MODE | MSM_DISABLE_WB
					| MSM_PRODUCER | port->src_pipe_idx;
			port->rx_req->length = 32*1024;
			port->rx_req->udc_priv = sps_params;
			configure_fifo(port->usb_bam_type,
					port->src_connection_idx,
					port->port_usb->out);
			ret = msm_ep_config(gport->out, port->rx_req);
			if (ret) {
				pr_err("msm_ep_config() failed for OUT EP\n");
				usb_bam_free_fifos(port->usb_bam_type,
						port->src_connection_idx);
				goto free_rx_tx_req;
			}
		} else {
			get_bam2bam_connection_info(port->usb_bam_type,
					port->src_connection_idx,
					&port->src_pipe_idx,
					NULL, NULL, NULL);
			sps_params = (MSM_SPS_MODE | port->src_pipe_idx |
				       MSM_VENDOR_ID) & ~MSM_IS_FINITE_TRANSFER;
			port->rx_req->udc_priv = sps_params;
		}
	}

	if (gport->in) {
		usb_bam_alloc_fifos(port->usb_bam_type,
						port->dst_connection_idx);
		if (gadget_is_dwc3(gadget)) {
			sps_params = MSM_SPS_MODE | MSM_DISABLE_WB |
							port->dst_pipe_idx;
			port->tx_req->length = 32*1024;
			port->tx_req->udc_priv = sps_params;
			configure_fifo(port->usb_bam_type,
					port->dst_connection_idx, gport->in);
			ret = msm_ep_config(gport->in, port->tx_req);
			if (ret) {
				pr_err("msm_ep_config() failed for IN EP\n");
				goto unconfig_msm_ep_out;
			}
		} else {
			get_bam2bam_connection_info(port->usb_bam_type,
					port->dst_connection_idx,
					&port->dst_pipe_idx,
					NULL, NULL, NULL);
			sps_params = (MSM_SPS_MODE | port->dst_pipe_idx |
				       MSM_VENDOR_ID) & ~MSM_IS_FINITE_TRANSFER;
			port->tx_req->udc_priv = sps_params;
		}
	}

	/*
	 * Perform below operations for Tx from Device (OUT transfer)
	 * 1. Connect with pipe of USB BAM with IPA BAM pipe
	 * 2. Update USB Endpoint related information using SPS Param.
	 * 3. Configure USB Endpoint/DBM for the same.
	 * 4. Override USB ep queue functionality for endless transfer.
	 */
	if (gport->out) {
		pr_debug("configure bam ipa connect for USB OUT\n");
		port->ipa_params.dir = USB_TO_PEER_PERIPHERAL;
		ret = usb_bam_connect_ipa(port->usb_bam_type,
						&port->ipa_params);
		if (ret) {
			pr_err("usb_bam_connect_ipa out failed err:%d\n", ret);
			goto unconfig_msm_ep_in;
		}

		gport->ipa_consumer_ep = port->ipa_params.ipa_cons_ep_idx;
		is_ipa_disconnected = false;
	}

	if (gport->in) {
		pr_debug("configure bam ipa connect for USB IN\n");
		port->ipa_params.dir = PEER_PERIPHERAL_TO_USB;
		port->ipa_params.dst_client = IPA_CLIENT_USB_DPL_CONS;
		ret = usb_bam_connect_ipa(port->usb_bam_type,
						&port->ipa_params);
		if (ret) {
			pr_err("usb_bam_connect_ipa IN failed err:%d\n", ret);
			goto disconnect_usb_bam_ipa_out;
		}

		gport->ipa_producer_ep = port->ipa_params.ipa_prod_ep_idx;
		is_ipa_disconnected = false;
	}

	pr_debug("ipa_producer_ep:%d ipa_consumer_ep:%d\n",
				gport->ipa_producer_ep,
				gport->ipa_consumer_ep);

	gqti_ctrl_update_ipa_pipes(NULL, DPL_QTI_CTRL_PORT_NO,
				gport->ipa_producer_ep,
				gport->ipa_consumer_ep);

	pr_debug("src_bam_idx:%d dst_bam_idx:%d\n",
			port->src_connection_idx, port->dst_connection_idx);

	if (gport->out)
		ipa_data_start_endless_xfer(port, false);
	if (gport->in)
		ipa_data_start_endless_xfer(port, true);

	pr_debug("Connect workqueue done (port %pK)", port);
	return;

disconnect_usb_bam_ipa_out:
	if (!is_ipa_disconnected) {
		usb_bam_disconnect_ipa(port->usb_bam_type, &port->ipa_params);
		is_ipa_disconnected = true;
	}
unconfig_msm_ep_in:
	if (gport->in)
		msm_ep_unconfig(port->port_usb->in);
unconfig_msm_ep_out:
	if (gport->in)
		usb_bam_free_fifos(port->usb_bam_type,
						port->dst_connection_idx);
	if (gport->out) {
		msm_ep_unconfig(port->port_usb->out);
		usb_bam_free_fifos(port->usb_bam_type,
						port->src_connection_idx);
	}
free_rx_tx_req:
	spin_lock_irqsave(&port->port_lock, flags);
	port->is_connected = false;
	spin_unlock_irqrestore(&port->port_lock, flags);
	if (gport->in && port->tx_req)
		usb_ep_free_request(gport->in, port->tx_req);
free_rx_req:
	if (gport->out && port->rx_req)
		usb_ep_free_request(gport->out, port->rx_req);
}

/**
 * ipa_data_connect() - Prepare IPA params and enable USB endpoints
 * @gp: USB IPA gadget port
 * @port_num: port number used by accelerated function
 * @src_connection_idx: USB BAM pipe index used as producer
 * @dst_connection_idx: USB BAM pipe index used as consumer
 *
 * It is being called from accelerated function driver (from set_alt()) to
 * initiate USB BAM IPA connection. This API is enabling accelerated endpoints
 * and schedule connect_work() which establishes USB IPA BAM communication.
 */
int ipa_data_connect(struct gadget_ipa_port *gp, u8 port_num,
		u8 src_connection_idx, u8 dst_connection_idx)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	int ret;

	pr_debug("dev:%pK port#%d src_connection_idx:%d dst_connection_idx:%d\n",
			gp, port_num, src_connection_idx, dst_connection_idx);

	if (port_num >= n_ipa_ports) {
		pr_err("invalid portno#%d\n", port_num);
		ret = -ENODEV;
		goto err;
	}

	if (!gp) {
		pr_err("gadget port is null\n");
		ret = -ENODEV;
		goto err;
	}

	port = ipa_data_ports[port_num];

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gp;
	port->src_connection_idx = src_connection_idx;
	port->dst_connection_idx = dst_connection_idx;
	port->usb_bam_type = usb_bam_get_bam_type(gp->cdev->gadget->name);

	port->ipa_params.src_pipe = &(port->src_pipe_idx);
	port->ipa_params.dst_pipe = &(port->dst_pipe_idx);
	port->ipa_params.src_idx = src_connection_idx;
	port->ipa_params.dst_idx = dst_connection_idx;

	/*
	 * Disable Xfer complete and Xfer not ready interrupts by
	 * marking endless flag which is used in UDC driver to enable
	 * these interrupts. with this set, these interrupts for selected
	 * endpoints won't be enabled.
	 */
	if (port->port_usb->in) {
		port->port_usb->in->endless = true;
		ret = usb_ep_enable(port->port_usb->in);
		if (ret) {
			pr_err("usb_ep_enable failed eptype:IN ep:%pK",
						port->port_usb->in);
			port->port_usb->in->endless = false;
			goto err_usb_in;
		}
	}

	if (port->port_usb->out) {
		port->port_usb->out->endless = true;
		ret = usb_ep_enable(port->port_usb->out);
		if (ret) {
			pr_err("usb_ep_enable failed eptype:OUT ep:%pK",
						port->port_usb->out);
			port->port_usb->out->endless = false;
			goto err_usb_out;
		}
	}

	if (!port->port_usb->out && !port->port_usb->in) {
		pr_err("%s(): No USB endpoint enabled.\n", __func__);
		ret = -EINVAL;
		goto err_usb_in;
	}

	queue_work(ipa_data_wq, &port->connect_w);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return ret;

err_usb_out:
	if (port->port_usb->in)
		port->port_usb->in->endless = false;
err_usb_in:
	spin_unlock_irqrestore(&port->port_lock, flags);
err:
	pr_debug("%s(): failed with error:%d\n", __func__, ret);
	return ret;
}

/**
 * ipa_data_start() - Restart USB endless transfer
 * @param: IPA data channel information
 * @dir: USB BAM pipe direction
 *
 * It is being used to restart USB endless transfer for USB bus resume.
 * For USB consumer case, it restarts USB endless RX transfer, whereas
 * for USB producer case, it resets DBM endpoint and restart USB endless
 * TX transfer.
 */
static void ipa_data_start(void *param, enum usb_bam_pipe_dir dir)
{
	struct ipa_data_ch_info *port = param;
	struct usb_gadget *gadget = NULL;

	if (!port || !port->port_usb || !port->port_usb->cdev->gadget) {
		pr_err("%s:port,cdev or gadget is  NULL\n", __func__);
		return;
	}

	gadget = port->port_usb->cdev->gadget;
	if (dir == USB_TO_PEER_PERIPHERAL) {
		pr_debug("%s(): start endless RX\n", __func__);
		ipa_data_start_endless_xfer(port, false);
	} else {
		pr_debug("%s(): start endless TX\n", __func__);
		if (msm_dwc3_reset_ep_after_lpm(gadget)) {
			configure_fifo(port->usb_bam_type,
				 port->dst_connection_idx, port->port_usb->in);
		}
		ipa_data_start_endless_xfer(port, true);
	}
}

/**
 * ipa_data_stop() - Stop endless Tx/Rx transfers
 * @param: IPA data channel information
 * @dir: USB BAM pipe direction
 *
 * It is being used to stop endless Tx/Rx transfers. It is being used
 * for USB bus suspend functionality.
 */
static void ipa_data_stop(void *param, enum usb_bam_pipe_dir dir)
{
	struct ipa_data_ch_info *port = param;
	struct usb_gadget *gadget = NULL;

	if (!port || !port->port_usb || !port->port_usb->cdev->gadget) {
		pr_err("%s:port,cdev or gadget is  NULL\n", __func__);
		return;
	}

	gadget = port->port_usb->cdev->gadget;
	if (dir == USB_TO_PEER_PERIPHERAL) {
		pr_debug("%s(): stop endless RX transfer\n", __func__);
		ipa_data_stop_endless_xfer(port, false);
	} else {
		pr_debug("%s(): stop endless TX transfer\n", __func__);
		ipa_data_stop_endless_xfer(port, true);
	}
}

/**
 * ipa_data_suspend() - Initiate USB BAM IPA suspend functionality
 * @gp: Gadget IPA port
 * @port_num: port number used by function
 *
 * It is being used to initiate USB BAM IPA suspend functionality
 * for USB bus suspend functionality.
 */
void ipa_data_suspend(struct gadget_ipa_port *gp, u8 port_num)
{
	struct ipa_data_ch_info *port;
	int ret;

	pr_debug("dev:%pK port number:%d\n", gp, port_num);

	if (port_num >= n_ipa_ports) {
		pr_err("invalid ipa portno#%d\n", port_num);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}

	port = ipa_data_ports[port_num];
	if (!port) {
		pr_err("port %u is NULL", port_num);
		return;
	}

	pr_debug("%s: suspend started\n", __func__);
	ret = usb_bam_register_wake_cb(port->usb_bam_type,
			port->dst_connection_idx, NULL, port);
	if (ret) {
		pr_err("%s(): Failed to register BAM wake callback.\n",
				__func__);
		return;
	}

	usb_bam_register_start_stop_cbs(port->usb_bam_type,
			port->dst_connection_idx, ipa_data_start,
			ipa_data_stop, port);
	usb_bam_suspend(port->usb_bam_type, &port->ipa_params);
}

/**
 * ipa_data_resume() - Initiate USB resume functionality
 * @gp: Gadget IPA port
 * @port_num: port number used by function
 *
 * It is being used to initiate USB resume functionality
 * for USB bus resume case.
 */
void ipa_data_resume(struct gadget_ipa_port *gp, u8 port_num)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	struct usb_gadget *gadget = NULL;
	int ret;

	pr_debug("dev:%pK port number:%d\n", gp, port_num);

	if (port_num >= n_ipa_ports) {
		pr_err("invalid ipa portno#%d\n", port_num);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}

	port = ipa_data_ports[port_num];
	if (!port) {
		pr_err("port %u is NULL", port_num);
		return;
	}

	pr_debug("%s: resume started\n", __func__);
	spin_lock_irqsave(&port->port_lock, flags);
	gadget = port->port_usb->cdev->gadget;
	if (!gadget) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s(): Gadget is NULL.\n", __func__);
		return;
	}

	ret = usb_bam_register_wake_cb(port->usb_bam_type,
				port->dst_connection_idx, NULL, NULL);
	if (ret) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s(): Failed to register BAM wake callback.\n",
								__func__);
		return;
	}

	if (msm_dwc3_reset_ep_after_lpm(gadget)) {
		configure_fifo(port->usb_bam_type, port->src_connection_idx,
				port->port_usb->out);
		configure_fifo(port->usb_bam_type, port->dst_connection_idx,
				port->port_usb->in);
		spin_unlock_irqrestore(&port->port_lock, flags);
		msm_dwc3_reset_dbm_ep(port->port_usb->in);
		spin_lock_irqsave(&port->port_lock, flags);
		usb_bam_resume(port->usb_bam_type, &port->ipa_params);
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
}

/**
 * ipa_data_port_alloc() - Allocate IPA USB Port structure
 * @portno: port number to be used by particular USB function
 *
 * It is being used by USB function driver to allocate IPA data port
 * for USB IPA data accelerated path.
 *
 * Retrun: 0 in case of success, otherwise errno.
 */
static int ipa_data_port_alloc(int portno)
{
	struct ipa_data_ch_info *port = NULL;

	if (ipa_data_ports[portno] != NULL) {
		pr_debug("port %d already allocated.\n", portno);
		return 0;
	}

	port = kzalloc(sizeof(struct ipa_data_ch_info), GFP_KERNEL);
	if (!port) {
		pr_err("no memory to allocate port %d\n", portno);
		return -ENOMEM;
	}

	ipa_data_ports[portno] = port;

	pr_debug("port:%pK with portno:%d allocated\n", port, portno);
	return 0;
}

/**
 * ipa_data_port_select() - Select particular port for BAM2BAM IPA mode
 * @portno: port number to be used by particular USB function
 * @gtype: USB gadget function type
 *
 * It is being used by USB function driver to select which BAM2BAM IPA
 * port particular USB function wants to use.
 *
 */
void ipa_data_port_select(int portno, enum gadget_type gtype)
{
	struct ipa_data_ch_info *port = NULL;

	pr_debug("portno:%d\n", portno);

	port = ipa_data_ports[portno];
	port->port_num  = portno;
	port->is_connected = false;

	spin_lock_init(&port->port_lock);

	if (!work_pending(&port->connect_w))
		INIT_WORK(&port->connect_w, ipa_data_connect_work);

	if (!work_pending(&port->disconnect_w))
		INIT_WORK(&port->disconnect_w, ipa_data_disconnect_work);

	port->ipa_params.src_client = IPA_CLIENT_USB_PROD;
	port->ipa_params.dst_client = IPA_CLIENT_USB_CONS;
	port->gtype = gtype;
};

void ipa_data_flush_workqueue(void)
{
	pr_debug("%s(): Flushing workqueue\n", __func__);
	flush_workqueue(ipa_data_wq);
}

/**
 * ipa_data_setup() - setup BAM2BAM IPA port
 * @no_ipa_port: total number of BAM2BAM IPA port to support
 *
 * Each USB function who wants to use BAM2BAM IPA port would
 * be counting number of IPA port to use and initialize those
 * ports at time of bind_config() in android gadget driver.
 *
 * Retrun: 0 in case of success, otherwise errno.
 */
int ipa_data_setup(unsigned int no_ipa_port)
{
	int i, ret;

	pr_debug("requested %d IPA BAM ports", no_ipa_port);

	if (!no_ipa_port || no_ipa_port > IPA_N_PORTS) {
		pr_err("Invalid num of ports count:%d\n", no_ipa_port);
		return -EINVAL;
	}

	for (i = 0; i < no_ipa_port; i++) {
		n_ipa_ports++;
		ret = ipa_data_port_alloc(i);
		if (ret) {
			n_ipa_ports--;
			pr_err("Failed to alloc port:%d\n", i);
			goto free_ipa_ports;
		}
	}

	pr_debug("n_ipa_ports:%d\n", n_ipa_ports);

	if (ipa_data_wq) {
		pr_debug("ipa_data_wq is already setup.");
		return 0;
	}

	ipa_data_wq = alloc_workqueue("k_usb_ipa_data",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!ipa_data_wq) {
		pr_err("Failed to create workqueue\n");
		ret = -ENOMEM;
		goto free_ipa_ports;
	}

	return 0;

free_ipa_ports:
	for (i = 0; i < n_ipa_ports; i++) {
		kfree(ipa_data_ports[i]);
		ipa_data_ports[i] = NULL;
		if (ipa_data_wq) {
			destroy_workqueue(ipa_data_wq);
			ipa_data_wq = NULL;
		}
	}

	return ret;
}
