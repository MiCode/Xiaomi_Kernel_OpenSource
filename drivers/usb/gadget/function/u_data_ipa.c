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

#include "u_data_ipa.h"

struct ipa_data_ch_info {
	struct usb_request			*rx_req;
	struct usb_request			*tx_req;
	unsigned long				flags;
	unsigned int				id;
	enum ipa_func_type			func_type;
	bool					is_connected;
	unsigned int				port_num;
	spinlock_t				port_lock;

	struct work_struct			connect_w;
	struct work_struct			disconnect_w;
	struct work_struct			suspend_w;
	struct work_struct			resume_w;

	u32					src_pipe_idx;
	u32					dst_pipe_idx;
	u8					src_connection_idx;
	u8					dst_connection_idx;
	enum usb_ctrl				usb_bam_type;
	struct data_port			*port_usb;
	struct usb_gadget			*gadget;
	atomic_t				pipe_connect_notified;
	struct usb_bam_connect_ipa_params	ipa_params;
};

struct rndis_data_ch_info {
	/* this provides downlink (device->host i.e host) side configuration*/
	u32	dl_max_transfer_size;
	/* this provides uplink (host->device i.e device) side configuration */
	u32	ul_max_transfer_size;
	u32	ul_max_packets_number;
	bool	ul_aggregation_enable;
	u32	prod_clnt_hdl;
	u32	cons_clnt_hdl;
	void	*priv;
};

static struct workqueue_struct *ipa_data_wq;
struct ipa_data_ch_info *ipa_data_ports[IPA_N_PORTS];
static struct rndis_data_ch_info *rndis_data;
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
	unsigned long flags;
	int status;
	struct usb_ep *ep;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb || (in && !port->tx_req)
				|| (!in && !port->rx_req)) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s(): port_usb/req is NULL.\n", __func__);
		return;
	}

	if (in)
		ep = port->port_usb->in;
	else
		ep = port->port_usb->out;

	spin_unlock_irqrestore(&port->port_lock, flags);

	if (in) {
		pr_debug("%s: enqueue endless TX_REQ(IN)\n", __func__);
		status = usb_ep_queue(ep, port->tx_req, GFP_ATOMIC);
		if (status)
			pr_err("error enqueuing endless TX_REQ, %d\n", status);
	} else {
		pr_debug("%s: enqueue endless RX_REQ(OUT)\n", __func__);
		status = usb_ep_queue(ep, port->rx_req, GFP_ATOMIC);
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
	unsigned long flags;
	int status;
	struct usb_ep *ep;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb || (in && !port->tx_req)
				|| (!in && !port->rx_req)) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		pr_err("%s(): port_usb/req is NULL.\n", __func__);
		return;
	}

	if (in)
		ep = port->port_usb->in;
	else
		ep = port->port_usb->out;

	spin_unlock_irqrestore(&port->port_lock, flags);

	if (in) {
		pr_debug("%s: dequeue endless TX_REQ(IN)\n", __func__);
		status = usb_ep_dequeue(ep, port->tx_req);
		if (status)
			pr_err("error dequeueing endless TX_REQ, %d\n", status);
	} else {
		pr_debug("%s: dequeue endless RX_REQ(OUT)\n", __func__);
		status = usb_ep_dequeue(ep, port->rx_req);
		if (status)
			pr_err("error dequeueing endless RX_REQ, %d\n", status);
	}
}

/*
 * Called when IPA triggers us that the network interface is up.
 *  Starts the transfers on bulk endpoints.
 * (optimization reasons, the pipes and bam with IPA are already connected)
 */
void ipa_data_start_rx_tx(enum ipa_func_type func)
{
	struct ipa_data_ch_info	*port;
	unsigned long flags;
	struct usb_ep *epin, *epout;

	pr_debug("%s: Triggered: starting tx, rx", __func__);
	/* queue in & out requests */
	port = ipa_data_ports[func];
	if (!port) {
		pr_err("%s: port is NULL, can't start tx, rx", __func__);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->port_usb || !port->port_usb->in ||
		!port->port_usb->out) {
		pr_err("%s: Can't start tx, rx, ep not enabled", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	if (!port->rx_req || !port->tx_req) {
		pr_err("%s: No request d->rx_req=%pK, d->tx_req=%pK", __func__,
			port->rx_req, port->tx_req);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
	if (!port->is_connected) {
		pr_debug("%s: pipes are disconnected", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	epout = port->port_usb->out;
	epin = port->port_usb->in;
	spin_unlock_irqrestore(&port->port_lock, flags);

	/* queue in & out requests */
	pr_debug("%s: Starting rx", __func__);
	if (epout)
		ipa_data_start_endless_xfer(port, false);

	pr_debug("%s: Starting tx", __func__);
	if (epin)
		ipa_data_start_endless_xfer(port, true);
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

	if (port->func_type == USB_IPA_FUNC_RNDIS) {
		/*
		 * NOTE: it is required to disconnect USB and IPA BAM related
		 * pipes before calling IPA tethered function related disconnect
		 * API. IPA tethered function related disconnect API delete
		 * depedency graph with IPA RM which would results into IPA not
		 * pulling data although there is pending data on USB BAM
		 * producer pipe.
		 */
		if (atomic_xchg(&port->pipe_connect_notified, 0) == 1) {
			void *priv;

			priv = rndis_qc_get_ipa_priv();
			rndis_ipa_pipe_disconnect_notify(priv);
		}
	}

	if (port->ipa_params.prod_clnt_hdl)
		usb_bam_free_fifos(port->usb_bam_type,
						port->dst_connection_idx);
	if (port->ipa_params.cons_clnt_hdl)
		usb_bam_free_fifos(port->usb_bam_type,
						port->src_connection_idx);

	if (port->func_type == USB_IPA_FUNC_RMNET)
		teth_bridge_disconnect(port->ipa_params.src_client);
	/*
	 * Decrement usage count which was incremented
	 * upon cable connect or cable disconnect in suspended state.
	 */
	usb_gadget_autopm_put_async(port->gadget);

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
void ipa_data_disconnect(struct data_port *gp, enum ipa_func_type func)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	struct usb_gadget *gadget = NULL;

	pr_debug("dev:%pK port number:%d\n", gp, func);
	if (func >= USB_IPA_NUM_FUNCS) {
		pr_err("invalid ipa portno#%d\n", func);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}

	port = ipa_data_ports[func];
	if (!port) {
		pr_err("port %u is NULL", func);
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
			msm_ep_unconfig(port->port_usb->in);
			spin_unlock_irqrestore(&port->port_lock, flags);
			usb_ep_disable(port->port_usb->in);
			spin_lock_irqsave(&port->port_lock, flags);
			if (port->tx_req) {
				usb_ep_free_request(port->port_usb->in,
						port->tx_req);
				port->tx_req = NULL;
			}
			port->port_usb->in->endless = false;
		}

		if (port->port_usb->out) {
			msm_ep_unconfig(port->port_usb->out);
			spin_unlock_irqrestore(&port->port_lock, flags);
			usb_ep_disable(port->port_usb->out);
			spin_lock_irqsave(&port->port_lock, flags);
			if (port->rx_req) {
				usb_ep_free_request(port->port_usb->out,
						port->rx_req);
				port->rx_req = NULL;
			}
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
	struct sps_mem_buffer data_fifo = {0};
	u32 usb_bam_pipe_idx;

	get_bam2bam_connection_info(bam_type, idx,
				&usb_bam_pipe_idx,
				NULL, &data_fifo, NULL);
	msm_data_fifo_config(ep, data_fifo.phys_base, data_fifo.size,
			usb_bam_pipe_idx);
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
	struct data_port	*gport;
	struct usb_gadget	*gadget = NULL;
	struct teth_bridge_connect_params connect_params;
	struct teth_bridge_init_params teth_bridge_params;
	u32			sps_params;
	int			ret;
	unsigned long		flags;
	bool			is_ipa_disconnected = true;

	pr_debug("%s: Connect workqueue started\n", __func__);

	spin_lock_irqsave(&port->port_lock, flags);

	if (!port->port_usb) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_gadget_autopm_put_async(port->gadget);
		pr_err("%s(): port_usb is NULL.\n", __func__);
		return;
	}

	gport = port->port_usb;
	if (gport && gport->cdev)
		gadget = gport->cdev->gadget;

	if (!gadget) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_gadget_autopm_put_async(port->gadget);
		pr_err("%s: gport is NULL.\n", __func__);
		return;
	}

	/*
	 * check if connect_w got called two times during RNDIS resume as
	 * explicit flow control is called to start data transfers after
	 * ipa_data_connect()
	 */
	if (port->is_connected) {
		pr_debug("IPA connect is already done & Transfers started\n");
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_gadget_autopm_put_async(port->gadget);
		return;
	}

	gport->ipa_consumer_ep = -1;
	gport->ipa_producer_ep = -1;

	port->is_connected = true;

	/* update IPA Parameteres here. */
	port->ipa_params.usb_connection_speed = gadget->speed;
	port->ipa_params.reset_pipe_after_lpm =
				msm_dwc3_reset_ep_after_lpm(gadget);
	port->ipa_params.skip_ep_cfg = true;
	port->ipa_params.keep_ipa_awake = true;
	port->ipa_params.cons_clnt_hdl = -1;
	port->ipa_params.prod_clnt_hdl = -1;

	if (gport->out) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_bam_alloc_fifos(port->usb_bam_type,
						port->src_connection_idx);
		spin_lock_irqsave(&port->port_lock, flags);
		if (!port->port_usb || port->rx_req == NULL) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s: port_usb is NULL, or rx_req cleaned\n",
				__func__);
			goto out;
		}

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
			spin_unlock_irqrestore(&port->port_lock, flags);
			goto out;
		}
	}

	if (gport->in) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		usb_bam_alloc_fifos(port->usb_bam_type,
						port->dst_connection_idx);
		spin_lock_irqsave(&port->port_lock, flags);
		if (!port->port_usb || port->tx_req == NULL) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s: port_usb is NULL, or tx_req cleaned\n",
				__func__);
			goto unconfig_msm_ep_out;
		}
		sps_params = MSM_SPS_MODE | MSM_DISABLE_WB |
						port->dst_pipe_idx;
		port->tx_req->length = 32*1024;
		port->tx_req->udc_priv = sps_params;
		configure_fifo(port->usb_bam_type,
				port->dst_connection_idx, gport->in);
		ret = msm_ep_config(gport->in, port->tx_req);
		if (ret) {
			pr_err("msm_ep_config() failed for IN EP\n");
			spin_unlock_irqrestore(&port->port_lock, flags);
			goto unconfig_msm_ep_out;
		}
	}

	if (port->func_type == USB_IPA_FUNC_RMNET) {
		teth_bridge_params.client = port->ipa_params.src_client;
		ret = teth_bridge_init(&teth_bridge_params);
		if (ret) {
			pr_err("%s:teth_bridge_init() failed\n", __func__);
			spin_unlock_irqrestore(&port->port_lock, flags);
			goto unconfig_msm_ep_in;
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

		if (port->func_type == USB_IPA_FUNC_RNDIS) {
			port->ipa_params.notify = rndis_qc_get_ipa_rx_cb();
			port->ipa_params.priv = rndis_qc_get_ipa_priv();
			port->ipa_params.skip_ep_cfg =
				rndis_qc_get_skip_ep_config();
		} else if (port->func_type == USB_IPA_FUNC_RMNET) {
			port->ipa_params.notify =
				teth_bridge_params.usb_notify_cb;
			port->ipa_params.priv =
				teth_bridge_params.private_data;
			port->ipa_params.reset_pipe_after_lpm =
				msm_dwc3_reset_ep_after_lpm(gadget);
			port->ipa_params.ipa_ep_cfg.mode.mode = IPA_BASIC;
			port->ipa_params.skip_ep_cfg =
				teth_bridge_params.skip_ep_cfg;
		}

		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_bam_connect_ipa(port->usb_bam_type,
						&port->ipa_params);
		if (ret) {
			pr_err("usb_bam_connect_ipa out failed err:%d\n", ret);
			goto disconnect_usb_bam_ipa_out;
		}
		spin_lock_irqsave(&port->port_lock, flags);
		is_ipa_disconnected = false;
		/* check if USB cable is disconnected or not */
		if (!port->port_usb) {
			pr_debug("%s:%d: cable is disconnected.\n",
						__func__, __LINE__);
			spin_unlock_irqrestore(&port->port_lock, flags);
			goto disconnect_usb_bam_ipa_out;
		}

		gport->ipa_consumer_ep = port->ipa_params.ipa_cons_ep_idx;
	}

	if (gport->in) {
		pr_debug("configure bam ipa connect for USB IN\n");
		port->ipa_params.dir = PEER_PERIPHERAL_TO_USB;

		if (port->func_type == USB_IPA_FUNC_RNDIS) {
			port->ipa_params.notify = rndis_qc_get_ipa_tx_cb();
			port->ipa_params.priv = rndis_qc_get_ipa_priv();
			port->ipa_params.skip_ep_cfg =
				rndis_qc_get_skip_ep_config();
		} else if (port->func_type == USB_IPA_FUNC_RMNET) {
			port->ipa_params.notify =
				teth_bridge_params.usb_notify_cb;
			port->ipa_params.priv =
				teth_bridge_params.private_data;
			port->ipa_params.reset_pipe_after_lpm =
				msm_dwc3_reset_ep_after_lpm(gadget);
			port->ipa_params.ipa_ep_cfg.mode.mode = IPA_BASIC;
			port->ipa_params.skip_ep_cfg =
				teth_bridge_params.skip_ep_cfg;
		}

		if (port->func_type == USB_IPA_FUNC_DPL)
			port->ipa_params.dst_client = IPA_CLIENT_USB_DPL_CONS;
		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = usb_bam_connect_ipa(port->usb_bam_type,
						&port->ipa_params);
		if (ret) {
			pr_err("usb_bam_connect_ipa IN failed err:%d\n", ret);
			goto disconnect_usb_bam_ipa_out;
		}
		spin_lock_irqsave(&port->port_lock, flags);
		is_ipa_disconnected = false;
		/* check if USB cable is disconnected or not */
		if (!port->port_usb) {
			pr_debug("%s:%d: cable is disconnected.\n",
						__func__, __LINE__);
			spin_unlock_irqrestore(&port->port_lock, flags);
			goto disconnect_usb_bam_ipa_out;
		}

		gport->ipa_producer_ep = port->ipa_params.ipa_prod_ep_idx;
	}

	spin_unlock_irqrestore(&port->port_lock, flags);
	if (port->func_type == USB_IPA_FUNC_RNDIS) {
		rndis_data->prod_clnt_hdl =
			port->ipa_params.prod_clnt_hdl;
		rndis_data->cons_clnt_hdl =
			port->ipa_params.cons_clnt_hdl;
		rndis_data->priv = port->ipa_params.priv;

		pr_debug("ul_max_transfer_size:%d\n",
				rndis_data->ul_max_transfer_size);
		pr_debug("ul_max_packets_number:%d\n",
				rndis_data->ul_max_packets_number);
		pr_debug("dl_max_transfer_size:%d\n",
				rndis_data->dl_max_transfer_size);

		ret = rndis_ipa_pipe_connect_notify(
				rndis_data->cons_clnt_hdl,
				rndis_data->prod_clnt_hdl,
				rndis_data->ul_max_transfer_size,
				rndis_data->ul_max_packets_number,
				rndis_data->dl_max_transfer_size,
				rndis_data->priv);
		if (ret) {
			pr_err("%s: failed to connect IPA: err:%d\n",
				__func__, ret);
			return;
		}
		atomic_set(&port->pipe_connect_notified, 1);
	} else if (port->func_type == USB_IPA_FUNC_RMNET ||
			port->func_type == USB_IPA_FUNC_DPL) {
		/* For RmNet and DPL need to update_ipa_pipes to qti */
		enum qti_port_type qti_port_type = port->func_type ==
			USB_IPA_FUNC_RMNET ? QTI_PORT_RMNET : QTI_PORT_DPL;
		gqti_ctrl_update_ipa_pipes(port->port_usb, qti_port_type,
			gport->ipa_producer_ep, gport->ipa_consumer_ep);
	}

	if (port->func_type == USB_IPA_FUNC_RMNET) {
		connect_params.ipa_usb_pipe_hdl =
			port->ipa_params.prod_clnt_hdl;
		connect_params.usb_ipa_pipe_hdl =
			port->ipa_params.cons_clnt_hdl;
		connect_params.tethering_mode =
			TETH_TETHERING_MODE_RMNET;
		connect_params.client_type =
			port->ipa_params.src_client;
		ret = teth_bridge_connect(&connect_params);
		if (ret) {
			pr_err("%s:teth_bridge_connect() failed\n", __func__);
			goto disconnect_usb_bam_ipa_out;
		}
	}

	pr_debug("ipa_producer_ep:%d ipa_consumer_ep:%d\n",
				gport->ipa_producer_ep,
				gport->ipa_consumer_ep);

	pr_debug("src_bam_idx:%d dst_bam_idx:%d\n",
			port->src_connection_idx, port->dst_connection_idx);

	/* Don't queue the transfers yet, only after network stack is up */
	if (port->func_type == USB_IPA_FUNC_RNDIS) {
		pr_debug("%s: Not starting now, waiting for network notify",
			__func__);
		return;
	}

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
	if (port->func_type == USB_IPA_FUNC_RMNET)
		teth_bridge_disconnect(port->ipa_params.src_client);
unconfig_msm_ep_in:
	spin_lock_irqsave(&port->port_lock, flags);
	/* check if USB cable is disconnected or not */
	if (port->port_usb && gport->in)
		msm_ep_unconfig(port->port_usb->in);
	spin_unlock_irqrestore(&port->port_lock, flags);
unconfig_msm_ep_out:
	if (gport->in)
		usb_bam_free_fifos(port->usb_bam_type,
						port->dst_connection_idx);
	spin_lock_irqsave(&port->port_lock, flags);
	/* check if USB cable is disconnected or not */
	if (port->port_usb && gport->out)
		msm_ep_unconfig(port->port_usb->out);
	spin_unlock_irqrestore(&port->port_lock, flags);
out:
	if (gport->out)
		usb_bam_free_fifos(port->usb_bam_type,
						port->src_connection_idx);
	spin_lock_irqsave(&port->port_lock, flags);
	port->is_connected = false;
	spin_unlock_irqrestore(&port->port_lock, flags);
	usb_gadget_autopm_put_async(port->gadget);
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
int ipa_data_connect(struct data_port *gp, enum ipa_func_type func,
		u8 src_connection_idx, u8 dst_connection_idx)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	int ret = 0;

	pr_debug("dev:%pK port#%d src_connection_idx:%d dst_connection_idx:%d\n",
			gp, func, src_connection_idx, dst_connection_idx);

	if (func >= USB_IPA_NUM_FUNCS) {
		pr_err("invalid portno#%d\n", func);
		ret = -ENODEV;
		goto err;
	}

	if (!gp) {
		pr_err("gadget port is null\n");
		ret = -ENODEV;
		goto err;
	}

	port = ipa_data_ports[func];

	spin_lock_irqsave(&port->port_lock, flags);
	port->port_usb = gp;
	port->gadget = gp->cdev->gadget;

	if (gp->out) {
		port->rx_req = usb_ep_alloc_request(gp->out, GFP_ATOMIC);
		if (!port->rx_req) {
			spin_unlock_irqrestore(&port->port_lock, flags);
			pr_err("%s: failed to allocate rx_req\n", __func__);
			goto err;
		}
		port->rx_req->context = port;
		port->rx_req->complete = ipa_data_endless_complete;
		port->rx_req->length = 0;
		port->rx_req->no_interrupt = 1;
	}

	if (gp->in) {
		port->tx_req = usb_ep_alloc_request(gp->in, GFP_ATOMIC);
		if (!port->tx_req) {
			pr_err("%s: failed to allocate tx_req\n", __func__);
			goto free_rx_req;
		}
		port->tx_req->context = port;
		port->tx_req->complete = ipa_data_endless_complete;
		port->tx_req->length = 0;
		port->tx_req->no_interrupt = 1;
	}
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
			usb_ep_free_request(port->port_usb->in, port->tx_req);
			port->tx_req = NULL;
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
			usb_ep_free_request(port->port_usb->out, port->rx_req);
			port->rx_req = NULL;
			port->port_usb->out->endless = false;
			goto err_usb_out;
		}
	}

	/* Wait for host to enable flow_control */
	if (port->func_type == USB_IPA_FUNC_RNDIS) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		ret = 0;
		return ret;
	}

	/*
	 * Increment usage count upon cable connect. Decrement after IPA
	 * handshake is done in disconnect work (due to cable disconnect)
	 * or in suspend work.
	 */
	usb_gadget_autopm_get_noresume(port->gadget);

	queue_work(ipa_data_wq, &port->connect_w);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return ret;

err_usb_out:
	if (port->port_usb->in) {
		usb_ep_disable(port->port_usb->in);
		port->port_usb->in->endless = false;
	}
err_usb_in:
	if (gp->in && port->tx_req) {
		usb_ep_free_request(gp->in, port->tx_req);
		port->tx_req = NULL;
	}
free_rx_req:
	if (gp->out && port->rx_req) {
		usb_ep_free_request(gp->out, port->rx_req);
		port->rx_req = NULL;
	}
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

void ipa_data_flush_workqueue(void)
{
	pr_debug("%s(): Flushing workqueue\n", __func__);
	flush_workqueue(ipa_data_wq);
}

/**
 * ipa_data_suspend() - Initiate USB BAM IPA suspend functionality
 * @gp: Gadget IPA port
 * @port_num: port number used by function
 *
 * It is being used to initiate USB BAM IPA suspend functionality
 * for USB bus suspend functionality.
 */
void ipa_data_suspend(struct data_port *gp, enum ipa_func_type func,
			bool remote_wakeup_enabled)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;

	if (func >= USB_IPA_NUM_FUNCS) {
		pr_err("invalid ipa portno#%d\n", func);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}
	pr_debug("%s: suspended port %d\n", __func__, func);

	port = ipa_data_ports[func];
	if (!port) {
		pr_err("%s(): Port is NULL.\n", __func__);
		return;
	}

	/* suspend with remote wakeup disabled */
	if (!remote_wakeup_enabled) {
		/*
		 * When remote wakeup is disabled, IPA BAM is disconnected
		 * because it cannot send new data until the USB bus is resumed.
		 * Endpoint descriptors info is saved before it gets reset by
		 * the BAM disconnect API. This lets us restore this info when
		 * the USB bus is resumed.
		 */
		if (gp->in) {
			gp->in_ep_desc_backup = gp->in->desc;
			pr_debug("in_ep_desc_backup = %pK\n",
				gp->in_ep_desc_backup);
		}
		if (gp->out) {
			gp->out_ep_desc_backup = gp->out->desc;
			pr_debug("out_ep_desc_backup = %pK\n",
				gp->out_ep_desc_backup);
		}
		ipa_data_disconnect(gp, func);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);
	queue_work(ipa_data_wq, &port->suspend_w);
	spin_unlock_irqrestore(&port->port_lock, flags);
}
static void bam2bam_data_suspend_work(struct work_struct *w)
{
	struct ipa_data_ch_info *port = container_of(w, struct ipa_data_ch_info,
								connect_w);
	unsigned long flags;
	int ret;

	pr_debug("%s: suspend started\n", __func__);
	spin_lock_irqsave(&port->port_lock, flags);

	/* In case of RNDIS, host enables flow_control invoking connect_w. If it
	 * is delayed then we may end up having suspend_w run before connect_w.
	 * In this scenario, connect_w may or may not at all start if cable gets
	 * disconnected or if host changes configuration e.g. RNDIS --> MBIM
	 * For these cases don't do runtime_put as there was no _get yet, and
	 * detect this condition on disconnect to not do extra pm_runtme_get
	 * for SUSPEND --> DISCONNECT scenario.
	 */
	if (!port->is_connected) {
		pr_err("%s: Not yet connected. SUSPEND pending.\n", __func__);
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}
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
	/*
	 * release lock here because bam_data_start() or
	 * bam_data_stop() called from usb_bam_suspend()
	 * re-acquires port lock.
	 */
	spin_unlock_irqrestore(&port->port_lock, flags);
	usb_bam_suspend(port->usb_bam_type, &port->ipa_params);
	spin_lock_irqsave(&port->port_lock, flags);

	/*
	 * Decrement usage count after IPA handshake is done
	 * to allow gadget parent to go to lpm. This counter was
	 * incremented upon cable connect.
	 */
	usb_gadget_autopm_put_async(port->gadget);

	spin_unlock_irqrestore(&port->port_lock, flags);
}

/**
 * ipa_data_resume() - Initiate USB resume functionality
 * @gp: Gadget IPA port
 * @port_num: port number used by function
 *
 * It is being used to initiate USB resume functionality
 * for USB bus resume case.
 */
void ipa_data_resume(struct data_port *gp, enum ipa_func_type func,
			bool remote_wakeup_enabled)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;
	struct usb_gadget *gadget = NULL;
	u8 src_connection_idx = 0;
	u8 dst_connection_idx = 0;
	enum usb_ctrl usb_bam_type;

	pr_debug("dev:%pK port number:%d\n", gp, func);

	if (func >= USB_IPA_NUM_FUNCS) {
		pr_err("invalid ipa portno#%d\n", func);
		return;
	}

	if (!gp) {
		pr_err("data port is null\n");
		return;
	}

	port = ipa_data_ports[func];
	if (!port) {
		pr_err("port %u is NULL", func);
		return;
	}

	gadget = gp->cdev->gadget;
	/* resume with remote wakeup disabled */
	if (!remote_wakeup_enabled) {
		int bam_pipe_num = (func == USB_IPA_FUNC_DPL) ? 1 : 0;

		usb_bam_type = usb_bam_get_bam_type(gadget->name);
		/* Restore endpoint descriptors info. */
		if (gp->in) {
			gp->in->desc = gp->in_ep_desc_backup;
			pr_debug("in_ep_desc_backup = %pK\n",
				gp->in_ep_desc_backup);
			dst_connection_idx = usb_bam_get_connection_idx(
				usb_bam_type, IPA_P_BAM, PEER_PERIPHERAL_TO_USB,
				USB_BAM_DEVICE, bam_pipe_num);
		}
		if (gp->out) {
			gp->out->desc = gp->out_ep_desc_backup;
			pr_debug("out_ep_desc_backup = %pK\n",
				gp->out_ep_desc_backup);
			src_connection_idx = usb_bam_get_connection_idx(
				usb_bam_type, IPA_P_BAM, USB_TO_PEER_PERIPHERAL,
				USB_BAM_DEVICE, bam_pipe_num);
		}
		ipa_data_connect(gp, func,
				src_connection_idx, dst_connection_idx);
		return;
	}

	spin_lock_irqsave(&port->port_lock, flags);

	/*
	 * Increment usage count here to disallow gadget
	 * parent suspend. This counter will decrement
	 * after IPA handshake is done in disconnect work
	 * (due to cable disconnect) or in bam_data_disconnect
	 * in suspended state.
	 */
	usb_gadget_autopm_get_noresume(port->gadget);
	queue_work(ipa_data_wq, &port->resume_w);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static void bam2bam_data_resume_work(struct work_struct *w)
{
	struct ipa_data_ch_info *port = container_of(w, struct ipa_data_ch_info,
								connect_w);
	struct usb_gadget *gadget;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&port->port_lock, flags);
	if (!port->port_usb || !port->port_usb->cdev) {
		pr_err("port->port_usb or cdev is NULL");
		goto exit;
	}

	if (!port->port_usb->cdev->gadget) {
		pr_err("port->port_usb->cdev->gadget is NULL");
		goto exit;
	}

	pr_debug("%s: resume started\n", __func__);
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

exit:
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
static int ipa_data_port_alloc(enum ipa_func_type func)
{
	struct ipa_data_ch_info *port = NULL;

	if (ipa_data_ports[func] != NULL) {
		pr_debug("port %d already allocated.\n", func);
		return 0;
	}

	port = kzalloc(sizeof(struct ipa_data_ch_info), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	ipa_data_ports[func] = port;

	pr_debug("port:%pK with portno:%d allocated\n", port, func);
	return 0;
}

/**
 * ipa_data_port_select() - Select particular port for BAM2BAM IPA mode
 * @portno: port number to be used by particular USB function
 * @func_type: USB gadget function type
 *
 * It is being used by USB function driver to select which BAM2BAM IPA
 * port particular USB function wants to use.
 *
 */
void ipa_data_port_select(enum ipa_func_type func)
{
	struct ipa_data_ch_info *port = NULL;

	pr_debug("portno:%d\n", func);

	port = ipa_data_ports[func];
	port->port_num  = func;
	port->is_connected = false;

	spin_lock_init(&port->port_lock);

	if (!work_pending(&port->connect_w))
		INIT_WORK(&port->connect_w, ipa_data_connect_work);

	if (!work_pending(&port->disconnect_w))
		INIT_WORK(&port->disconnect_w, ipa_data_disconnect_work);

	INIT_WORK(&port->suspend_w, bam2bam_data_suspend_work);
	INIT_WORK(&port->resume_w, bam2bam_data_resume_work);

	port->ipa_params.src_client = IPA_CLIENT_USB_PROD;
	port->ipa_params.dst_client = IPA_CLIENT_USB_CONS;
	port->func_type = func;
};

void ipa_data_free(enum ipa_func_type func)
{
	pr_debug("freeing %d IPA BAM port", func);

	kfree(ipa_data_ports[func]);
	ipa_data_ports[func] = NULL;
	if (func == USB_IPA_FUNC_RNDIS)
		kfree(rndis_data);
	if (ipa_data_wq) {
		destroy_workqueue(ipa_data_wq);
		ipa_data_wq = NULL;
	}
}

/**
 * ipa_data_setup() - setup BAM2BAM IPA port
 *
 * Each USB function who wants to use BAM2BAM IPA port would
 * be counting number of IPA port to use and initialize those
 * ports at time of bind_config() in android gadget driver.
 *
 * Retrun: 0 in case of success, otherwise errno.
 */
int ipa_data_setup(enum ipa_func_type func)
{
	int ret;

	pr_debug("requested %d IPA BAM port", func);

	if (func >= USB_IPA_NUM_FUNCS) {
		pr_err("Invalid num of ports count:%d\n", func);
		return -EINVAL;
	}

	ret = ipa_data_port_alloc(func);
	if (ret) {
		pr_err("Failed to alloc port:%d\n", func);
		return ret;
	}

	if (func == USB_IPA_FUNC_RNDIS) {
		rndis_data = kzalloc(sizeof(*rndis_data), GFP_KERNEL);
		if (!rndis_data) {
			pr_err("%s: fail allocate and initialize new instance\n",
				__func__);
			goto free_ipa_ports;
		}
	}
	if (ipa_data_wq) {
		pr_debug("ipa_data_wq is already setup.");
		return 0;
	}

	ipa_data_wq = alloc_workqueue("k_usb_ipa_data",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!ipa_data_wq) {
		pr_err("Failed to create workqueue\n");
		ret = -ENOMEM;
		goto free_rndis_data;
	}

	return 0;

free_rndis_data:
	if (func == USB_IPA_FUNC_RNDIS)
		kfree(rndis_data);
free_ipa_ports:
	kfree(ipa_data_ports[func]);
	ipa_data_ports[func] = NULL;

	return ret;
}

void ipa_data_set_ul_max_xfer_size(u32 max_transfer_size)
{
	if (!max_transfer_size) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}
	rndis_data->ul_max_transfer_size = max_transfer_size;
	pr_debug("%s(): ul_max_xfer_size:%d\n", __func__, max_transfer_size);
}

void ipa_data_set_dl_max_xfer_size(u32 max_transfer_size)
{

	if (!max_transfer_size) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}
	rndis_data->dl_max_transfer_size = max_transfer_size;
	pr_debug("%s(): dl_max_xfer_size:%d\n", __func__, max_transfer_size);
}

void ipa_data_set_ul_max_pkt_num(u8 max_packets_number)
{
	if (!max_packets_number) {
		pr_err("%s: invalid parameters\n", __func__);
		return;
	}

	rndis_data->ul_max_packets_number = max_packets_number;

	if (max_packets_number > 1)
		rndis_data->ul_aggregation_enable = true;
	else
		rndis_data->ul_aggregation_enable = false;

	pr_debug("%s(): ul_aggregation enable:%d ul_max_packets_number:%d\n",
				__func__, rndis_data->ul_aggregation_enable,
				max_packets_number);
}

void ipa_data_start_rndis_ipa(enum ipa_func_type func)
{
	struct ipa_data_ch_info *port;

	pr_debug("%s\n", __func__);

	port = ipa_data_ports[func];
	if (!port) {
		pr_err("%s: port is NULL", __func__);
		return;
	}

	if (atomic_read(&port->pipe_connect_notified)) {
		pr_debug("%s: Transfers already started?\n", __func__);
		return;
	}
	/*
	 * Increment usage count upon cable connect. Decrement after IPA
	 * handshake is done in disconnect work due to cable disconnect
	 * or in suspend work.
	 */
	usb_gadget_autopm_get_noresume(port->gadget);
	queue_work(ipa_data_wq, &port->connect_w);
}

void ipa_data_stop_rndis_ipa(enum ipa_func_type func)
{
	struct ipa_data_ch_info *port;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	port = ipa_data_ports[func];
	if (!port) {
		pr_err("%s: port is NULL", __func__);
		return;
	}

	if (!atomic_read(&port->pipe_connect_notified))
		return;

	rndis_ipa_reset_trigger();
	ipa_data_stop_endless_xfer(port, true);
	ipa_data_stop_endless_xfer(port, false);
	spin_lock_irqsave(&port->port_lock, flags);
	/* check if USB cable is disconnected or not */
	if (port->port_usb) {
		msm_ep_unconfig(port->port_usb->in);
		msm_ep_unconfig(port->port_usb->out);
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
	queue_work(ipa_data_wq, &port->disconnect_w);
}
