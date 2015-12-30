/* Copyright (c) 2015, Linux Foundation. All rights reserved.
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
#include <linux/usb/usb_ctrl_qti.h>
#include <linux/etherdevice.h>
#include <linux/debugfs.h>
#include <linux/usb/f_gsi.h>
#include "rndis.h"

static unsigned int dl_aggr_size = GSI_IN_BUFF_SIZE;
module_param(dl_aggr_size, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dl_aggr_size,
		"Max size of bus transfer to host");

static unsigned int ul_aggr_size = GSI_OUT_BUFF_SIZE;
module_param(ul_aggr_size, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ul_aggr_size,
		"Max size of bus transfer to device");

static unsigned int num_in_bufs = GSI_NUM_IN_BUFFERS;
module_param(num_in_bufs, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(num_in_bufs,
		"Number of IN buffers");

static unsigned int num_out_bufs = GSI_NUM_OUT_BUFFERS;
module_param(num_out_bufs, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(num_out_bufs,
		"Number of OUT buffers");

static struct workqueue_struct *ipa_usb_wq;
static bool gadget_restarted;

struct usb_gsi_debugfs {
	struct dentry *debugfs_root;
};

static struct usb_gsi_debugfs debugfs;

static void ipa_disconnect_handler(struct gsi_data_port *d_port);
static int gsi_ctrl_send_notification(struct f_gsi *gsi,
		enum gsi_ctrl_notify_state);

void post_event(struct gsi_data_port *port, u8 event)
{
	unsigned long flags;

	spin_lock_irqsave(&port->evt_q.q_lock, flags);

	port->evt_q.tail++;
	/* Check for wraparound and make room */
	port->evt_q.tail = port->evt_q.tail % MAXQUEUELEN;

	/* Check for overflow */
	if (port->evt_q.tail == port->evt_q.head) {
		pr_err("%s(): event queue overflow error\n", __func__);
		spin_unlock_irqrestore(&port->evt_q.q_lock, flags);
		return;
	}
	/* Add event to queue */
	port->evt_q.event[port->evt_q.tail] = event;
	spin_unlock_irqrestore(&port->evt_q.q_lock, flags);
}

void post_event_to_evt_queue(struct gsi_data_port *port, u8 event)
{
	post_event(port, event);
	queue_work(port->ipa_usb_wq, &port->usb_ipa_w);
}

u8 read_event(struct gsi_data_port *port)
{
	u8 event;
	unsigned long flags;

	spin_lock_irqsave(&port->evt_q.q_lock, flags);
	if (port->evt_q.head == port->evt_q.tail) {
		pr_debug("%s(): event queue empty\n", __func__);
		spin_unlock_irqrestore(&port->evt_q.q_lock, flags);
		return EVT_NONE;
	}

	port->evt_q.head++;
	/* Check for wraparound and make room */
	port->evt_q.head = port->evt_q.head % MAXQUEUELEN;

	 event = port->evt_q.event[port->evt_q.head];
	 spin_unlock_irqrestore(&port->evt_q.q_lock, flags);

	 return event;
}

u8 peek_event(struct gsi_data_port *port)
{
	u8 event;
	unsigned long flags;
	u8 peek_index = 0;

	spin_lock_irqsave(&port->evt_q.q_lock, flags);
	if (port->evt_q.head == port->evt_q.tail) {
		pr_debug("%s(): event queue empty\n", __func__);
		spin_unlock_irqrestore(&port->evt_q.q_lock, flags);
		return EVT_NONE;
	}

	peek_index = (port->evt_q.head + 1) % MAXQUEUELEN;
	event = port->evt_q.event[peek_index];
	spin_unlock_irqrestore(&port->evt_q.q_lock, flags);

	return event;
}

void reset_event_queue(struct gsi_data_port *port)
{
	unsigned long flags;

	spin_lock_irqsave(&port->evt_q.q_lock, flags);
	port->evt_q.head = port->evt_q.tail = MAXQUEUELEN - 1;
	memset(&port->evt_q.event[0], EVT_NONE, MAXQUEUELEN);
	spin_unlock_irqrestore(&port->evt_q.q_lock, flags);
}

int gsi_wakeup_host(struct f_gsi *gsi)
{

	int ret;
	struct usb_gadget *gadget;
	struct usb_function *func;

	func = &gsi->function;
	gadget = gsi->function.config->cdev->gadget;

	if (!gadget) {
		pr_err("FAILED: d_port->cdev->gadget == NULL");
		return -ENODEV;
	}

	/*
	 * In Super-Speed mode, remote wakeup is not allowed for suspended
	 * functions which have been disallowed by the host to issue Function
	 * Remote Wakeup.
	 * Note - We deviate here from the USB 3.0 spec and allow
	 * non-suspended functions to issue remote-wakeup even if they were not
	 * allowed to do so by the host. This is done in order to support non
	 * fully USB 3.0 compatible hosts.
	 */
	if ((gadget->speed == USB_SPEED_SUPER) && (func->func_is_suspended))
		ret = usb_func_wakeup(func);
	else
		ret = usb_gadget_wakeup(gadget);

	if ((ret == -EBUSY) || (ret == -EAGAIN))
		pr_debug("Remote wakeup is delayed due to LPM exit.\n");
	else if (ret)
		pr_err("Failed to wake up the USB core. ret=%d.\n", ret);

	return ret;
}

static ssize_t usb_gsi_debugfs_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buf;
	unsigned int len = 0, buf_len = 4096;
	struct f_gsi *gsi;
	struct ipa_usb_xdci_chan_params *ipa_chnl_params;
	struct ipa_usb_xdci_connect_params *con_pms;
	int i = 0;
	int j = 0;
	ssize_t ret_cnt;

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len, "%25s\n",
		"USB GSI Info");
	for (i = 0; i < IPA_USB_MAX_TETH_PROT_SIZE; i++) {
		gsi = gsi_prot_ctx[i];
		if (gsi && atomic_read(&gsi->connected)) {
			ipa_chnl_params = &gsi->d_port.ipa_in_channel_params;
			con_pms = &gsi->d_port.ipa_conn_pms;
			len += scnprintf(buf + len, buf_len - len, "%55s\n",
			"==================================================");
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10s\n", "Ctrl Name: ", gsi->c_port.name);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Notify State: ",
					gsi->c_port.notify_state);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Notify Count: ",
					gsi->c_port.notify_count.counter);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Online: ",
					gsi->c_port.ctrl_online.counter);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Open: ",
					gsi->c_port.is_open);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Host to Modem: ",
					gsi->c_port.host_to_modem);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Modem to Host: ",
					gsi->c_port.modem_to_host);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Cpd to Modem: ",
					gsi->c_port.copied_to_modem);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Cpd From Modem: ",
					gsi->c_port.copied_from_modem);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Ctrl Pkt Drops: ",
					gsi->c_port.cpkt_drop_cnt);
			len += scnprintf(buf + len, buf_len - len, "%25s\n",
			"==============");
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Protocol ID: ", gsi->prot_id);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "SM State: ", gsi->d_port.sm_state);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "IN XferRscIndex: ",
					gsi->d_port.in_xfer_rsc_index);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10d\n", "IN Chnl Hdl: ",
					gsi->d_port.in_channel_handle);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "IN Chnl Dbl Addr: ",
					gsi->d_port.in_db_reg_phs_addr_lsb);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "IN TRB Ring Len: ",
					ipa_chnl_params->xfer_ring_len);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "IN TRB Base Addr: ", (unsigned int)
				ipa_chnl_params->xfer_ring_base_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "GEVENTCNTLO IN Addr: ",
				ipa_chnl_params->gevntcount_low_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "DEPCMDLO IN Addr: ",
			ipa_chnl_params->xfer_scratch.depcmd_low_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "IN LastTRB Addr Off: ",
				ipa_chnl_params->xfer_scratch.last_trb_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "IN Buffer Size: ",
			ipa_chnl_params->xfer_scratch.const_buffer_size);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "IN/DL Aggr Size: ",
			con_pms->teth_prot_params.max_xfer_size_bytes_to_host);

			ipa_chnl_params = &gsi->d_port.ipa_out_channel_params;
			len += scnprintf(buf + len, buf_len - len, "%25s\n",
			"==============");
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "OUT XferRscIndex: ",
				gsi->d_port.out_xfer_rsc_index);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10d\n", "OUT Channel Hdl: ",
				gsi->d_port.out_channel_handle);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "OUT Channel Dbl Addr: ",
				gsi->d_port.out_db_reg_phs_addr_lsb);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "OUT TRB Ring Len: ",
				ipa_chnl_params->xfer_ring_len);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "OUT TRB Base Addr: ", (unsigned int)
				ipa_chnl_params->xfer_ring_base_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "GEVENTCNTLO OUT Addr: ",
				ipa_chnl_params->gevntcount_low_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "DEPCMDLO OUT Addr: ",
				ipa_chnl_params->xfer_scratch.depcmd_low_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10x\n", "OUT LastTRB Addr Off: ",
				ipa_chnl_params->xfer_scratch.last_trb_addr);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "OUT Buffer Size: ",
			ipa_chnl_params->xfer_scratch.const_buffer_size);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "OUT/UL Aggr Size: ",
			con_pms->teth_prot_params.max_xfer_size_bytes_to_dev);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "OUT/UL Packets to dev: ",
			con_pms->teth_prot_params.max_packet_number_to_dev);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Net_ready_trigger:",
			gsi->d_port.net_ready_trigger);
			len += scnprintf(buf + len, buf_len - len, "%25s\n",
			"USB Bus Events");
			for (j = 0; j < MAXQUEUELEN; j++)
				len += scnprintf(buf + len, buf_len - len,
					"%d\t", gsi->d_port.evt_q.event[j]);
			len += scnprintf(buf + len, buf_len - len, "\n");
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Eventq head: ",
					gsi->d_port.evt_q.head);
			len += scnprintf(buf + len, buf_len - len,
			"%25s %10u\n", "Eventq tail: ",
					gsi->d_port.evt_q.tail);
		}
	}

	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret_cnt;
}

static const struct file_operations fops_usb_gsi = {
		.read = usb_gsi_debugfs_read,
		.open = simple_open,
		.owner = THIS_MODULE,
		.llseek = default_llseek,
};

static int usb_gsi_debugfs_init(void)
{
	debugfs.debugfs_root = debugfs_create_dir("usb_gsi", 0);
	if (!debugfs.debugfs_root)
		return -ENOMEM;

	debugfs_create_file("info", S_IRUSR, debugfs.debugfs_root,
					gsi_prot_ctx, &fops_usb_gsi);
	return 0;
}

void usb_gsi_debugfs_exit(void)
{
	debugfs_remove_recursive(debugfs.debugfs_root);
}

/*
 * Callback for when when network interface is up
 * and userspace is ready to answer DHCP requests,  or remote wakeup
 */
int ipa_usb_notify_cb(enum ipa_usb_notify_event event,
	void *driver_data)
{
	struct f_gsi *gsi = driver_data;
	unsigned long flags;

	if (!gsi) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&gsi->d_port.lock, flags);

	switch (event) {
	case IPA_USB_DEVICE_READY:

		if (gsi->d_port.net_ready_trigger) {
			pr_err("%s: Already triggered", __func__);
			spin_unlock_irqrestore(&gsi->d_port.lock, flags);
			return 1;
		}

		pr_err("%s: Set net_ready_trigger", __func__);
		gsi->d_port.net_ready_trigger = true;

		if (gsi->prot_id == IPA_USB_ECM)
			gsi_ctrl_send_notification(gsi,
					GSI_CTRL_NOTIFY_CONNECT);

		/* Do not post EVT_CONNECTED for RNDIS.
		   Data path for RNDIS is enabled on EVT_HOST_READY.
		*/
		if (gsi->prot_id != IPA_USB_RNDIS) {
			post_event(&gsi->d_port, EVT_CONNECTED);
			queue_work(gsi->d_port.ipa_usb_wq,
					&gsi->d_port.usb_ipa_w);
		}
		break;

	case IPA_USB_REMOTE_WAKEUP:
		gsi_wakeup_host(gsi);
		break;

	case IPA_USB_SUSPEND_COMPLETED:
		post_event(&gsi->d_port, EVT_IPA_SUSPEND);
		queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);
		break;
	}

	spin_unlock_irqrestore(&gsi->d_port.lock, flags);
	return 1;
}

static int ipa_connect_channels(struct gsi_data_port *d_port)
{
	int ret;
	struct f_gsi *gsi = d_port_to_gsi(d_port);
	struct gsi_data_port *dport;
	struct usb_composite_dev *cdev = gsi->function.config->cdev;
	struct gsi_channel_info gsi_channel_info;
	struct ipa_req_chan_out_params ipa_in_channel_out_params;
	struct ipa_req_chan_out_params ipa_out_channel_out_params;

	usb_gsi_ep_op(gsi->d_port.in_ep, &gsi->d_port.in_request,
		GSI_EP_OP_PREPARE_TRBS);
	usb_gsi_ep_op(gsi->d_port.in_ep, &gsi->d_port.in_request,
			GSI_EP_OP_STARTXFER);
	gsi->d_port.in_xfer_rsc_index = usb_gsi_ep_op(gsi->d_port.in_ep, NULL,
			GSI_EP_OP_GET_XFER_IDX);

	memset(&gsi->d_port.ipa_in_channel_params, 0x0,
			sizeof(gsi->d_port.ipa_in_channel_params));
	gsi_channel_info.ch_req = &gsi->d_port.in_request;
	usb_gsi_ep_op(gsi->d_port.in_ep, (void *)&gsi_channel_info,
			GSI_EP_OP_GET_CH_INFO);
	gsi->d_port.ipa_in_channel_params.client =
	(gsi->prot_id != IPA_USB_DIAG) ? IPA_CLIENT_USB_CONS :
					IPA_CLIENT_USB_DPL_CONS;
	gsi->d_port.ipa_in_channel_params.ipa_ep_cfg.mode.mode = IPA_BASIC;
	gsi->d_port.ipa_in_channel_params.teth_prot = gsi->prot_id;
	gsi->d_port.ipa_in_channel_params.gevntcount_low_addr =
		gsi_channel_info.gevntcount_low_addr;
	gsi->d_port.ipa_in_channel_params.gevntcount_hi_addr =
		gsi_channel_info.gevntcount_hi_addr;
	gsi->d_port.ipa_in_channel_params.dir = GSI_CHAN_DIR_FROM_GSI;
	gsi->d_port.ipa_in_channel_params.xfer_ring_len =
		gsi_channel_info.xfer_ring_len;
	gsi->d_port.ipa_in_channel_params.xfer_ring_base_addr =
		gsi_channel_info.xfer_ring_base_addr;
	gsi->d_port.ipa_in_channel_params.xfer_scratch.last_trb_addr =
		gsi->d_port.in_last_trb_addr = gsi_channel_info.last_trb_addr;
	gsi->d_port.ipa_in_channel_params.xfer_scratch.const_buffer_size =
		gsi_channel_info.const_buffer_size;
	gsi->d_port.ipa_in_channel_params.xfer_scratch.depcmd_low_addr =
		gsi_channel_info.depcmd_low_addr;
	gsi->d_port.ipa_in_channel_params.xfer_scratch.depcmd_hi_addr =
		gsi_channel_info.depcmd_hi_addr;

	if (gsi->d_port.out_ep) {
		dport = &gsi->d_port;
		usb_gsi_ep_op(gsi->d_port.out_ep, &gsi->d_port.out_request,
			GSI_EP_OP_PREPARE_TRBS);
		usb_gsi_ep_op(gsi->d_port.out_ep, &gsi->d_port.out_request,
				GSI_EP_OP_STARTXFER);
		gsi->d_port.out_xfer_rsc_index =
			usb_gsi_ep_op(gsi->d_port.out_ep,
				NULL, GSI_EP_OP_GET_XFER_IDX);
		memset(&gsi->d_port.ipa_out_channel_params, 0x0,
				sizeof(gsi->d_port.ipa_out_channel_params));
		gsi_channel_info.ch_req = &gsi->d_port.out_request;
		usb_gsi_ep_op(gsi->d_port.out_ep, (void *)&gsi_channel_info,
				GSI_EP_OP_GET_CH_INFO);

		dport->ipa_out_channel_params.client = IPA_CLIENT_USB_PROD;
		dport->ipa_out_channel_params.ipa_ep_cfg.mode.mode =
								IPA_BASIC;
		dport->ipa_out_channel_params.teth_prot = gsi->prot_id;
		dport->ipa_out_channel_params.gevntcount_low_addr =
			gsi_channel_info.gevntcount_low_addr;
		dport->ipa_out_channel_params.gevntcount_hi_addr =
			gsi_channel_info.gevntcount_hi_addr;
		dport->ipa_out_channel_params.dir = GSI_CHAN_DIR_TO_GSI;
		dport->ipa_out_channel_params.xfer_ring_len =
			gsi_channel_info.xfer_ring_len;
		dport->ipa_out_channel_params.xfer_ring_base_addr =
			gsi_channel_info.xfer_ring_base_addr;
		dport->ipa_out_channel_params.xfer_scratch.last_trb_addr =
			gsi_channel_info.last_trb_addr;
		dport->ipa_out_channel_params.xfer_scratch.const_buffer_size =
			gsi_channel_info.const_buffer_size;
		dport->ipa_out_channel_params.xfer_scratch.depcmd_low_addr =
			gsi_channel_info.depcmd_low_addr;
		dport->ipa_out_channel_params.xfer_scratch.depcmd_hi_addr =
			gsi_channel_info.depcmd_hi_addr;
	}

	/* Populate connection params */
	gsi->d_port.ipa_conn_pms.max_pkt_size =
		(cdev->gadget->speed == USB_SPEED_SUPER) ?
		IPA_USB_SUPER_SPEED_1024B : IPA_USB_HIGH_SPEED_512B;
	gsi->d_port.ipa_conn_pms.ipa_to_usb_xferrscidx =
			gsi->d_port.in_xfer_rsc_index;
	gsi->d_port.ipa_conn_pms.usb_to_ipa_xferrscidx =
			gsi->d_port.out_xfer_rsc_index;
	gsi->d_port.ipa_conn_pms.usb_to_ipa_xferrscidx_valid =
	(gsi->prot_id != IPA_USB_DIAG) ? true : false;
	gsi->d_port.ipa_conn_pms.ipa_to_usb_xferrscidx_valid = true;
	gsi->d_port.ipa_conn_pms.teth_prot = gsi->prot_id;
	gsi->d_port.ipa_conn_pms.teth_prot_params.max_xfer_size_bytes_to_dev =
		23700;
	gsi->d_port.ipa_conn_pms.teth_prot_params.max_xfer_size_bytes_to_host =
		GSI_IN_BUFF_SIZE;
	gsi->d_port.ipa_conn_pms.teth_prot_params.max_packet_number_to_dev =
		DEFAULT_MAX_PKT_PER_XFER;
	gsi->d_port.ipa_conn_pms.max_supported_bandwidth_mbps =
		(cdev->gadget->speed == USB_SPEED_SUPER) ? 3600 : 400;

	memset(&ipa_in_channel_out_params, 0x0,
				sizeof(ipa_in_channel_out_params));
	memset(&ipa_out_channel_out_params, 0x0,
				sizeof(ipa_out_channel_out_params));

	ret = ipa_usb_xdci_connect(&gsi->d_port.ipa_out_channel_params,
					&gsi->d_port.ipa_in_channel_params,
					&ipa_out_channel_out_params,
					&ipa_in_channel_out_params,
					&gsi->d_port.ipa_conn_pms);
	if (ret) {
		pr_err("%s: IPA Connect failed (%d)", __func__, ret);
		return ret;
	}

	pr_debug("%s: Returned params for IN channel (%x)\n", __func__,
			ipa_in_channel_out_params.clnt_hdl);
	pr_debug("%s: Returned params for IN channel DBL ADD (%x)\n", __func__,
			ipa_in_channel_out_params.db_reg_phs_addr_lsb);

	pr_debug("%s: Returned params for OUT channel HDL (%x)\n",
			__func__, ipa_out_channel_out_params.clnt_hdl);
	pr_debug("%s: Returned params for OUT channel DBL ADD (%x)\n", __func__,
			ipa_out_channel_out_params.db_reg_phs_addr_lsb);

	gsi->d_port.in_channel_handle = ipa_in_channel_out_params.clnt_hdl;
	gsi->d_port.in_db_reg_phs_addr_lsb =
		ipa_in_channel_out_params.db_reg_phs_addr_lsb;
	gsi->d_port.in_db_reg_phs_addr_msb =
		ipa_in_channel_out_params.db_reg_phs_addr_msb;

	if (gsi->prot_id != IPA_USB_DIAG) {
		gsi->d_port.out_channel_handle =
			ipa_out_channel_out_params.clnt_hdl;
		gsi->d_port.out_db_reg_phs_addr_lsb =
			ipa_out_channel_out_params.db_reg_phs_addr_lsb;
		gsi->d_port.out_db_reg_phs_addr_msb =
			ipa_out_channel_out_params.db_reg_phs_addr_msb;
	}
	return ret;
}

static void ipa_data_path_enable(struct gsi_data_port *d_port)
{
	struct f_gsi *gsi = d_port_to_gsi(d_port);
	struct usb_gsi_request req;
	u64 dbl_register_addr;

	pr_debug("in_db_reg_phs_addr_lsb = (%x)",
			gsi->d_port.in_db_reg_phs_addr_lsb);
	usb_gsi_ep_op(gsi->d_port.in_ep,
			(void *)&gsi->d_port.in_db_reg_phs_addr_lsb,
			GSI_EP_OP_STORE_DBL_INFO);

	if (gsi->d_port.out_ep) {
		pr_debug("out_db_reg_phs_addr_lsb = (%x)",
				gsi->d_port.out_db_reg_phs_addr_lsb);
		usb_gsi_ep_op(gsi->d_port.out_ep,
				(void *)&gsi->d_port.out_db_reg_phs_addr_lsb,
				GSI_EP_OP_STORE_DBL_INFO);

		usb_gsi_ep_op(gsi->d_port.out_ep, &gsi->d_port.out_request,
				GSI_EP_OP_ENABLE_GSI);
	}

	dbl_register_addr = gsi->d_port.in_db_reg_phs_addr_msb;
	dbl_register_addr = dbl_register_addr << 32;
	dbl_register_addr =
		dbl_register_addr | gsi->d_port.in_db_reg_phs_addr_lsb;

	/* use temp gsi request to pass 64 bit dbl reg addr and num_bufs */
	req.buf_base_addr = &dbl_register_addr;

	req.num_bufs = gsi->d_port.in_request.num_bufs;
	usb_gsi_ep_op(gsi->d_port.in_ep, &req, GSI_EP_OP_RING_IN_DB);

	if (gsi->d_port.out_ep) {
		usb_gsi_ep_op(gsi->d_port.out_ep, &gsi->d_port.out_request,
			GSI_EP_OP_UPDATEXFER);
	}
}

static void ipa_disconnect_handler(struct gsi_data_port *d_port)
{
	struct f_gsi *gsi = d_port_to_gsi(d_port);

	pr_debug("%s: EP Disable for data", __func__);

	usb_ep_disable(gsi->d_port.in_ep);

	if (gsi->d_port.out_ep)
		usb_ep_disable(gsi->d_port.out_ep);
	gsi->d_port.net_ready_trigger = false;
}

static void ipa_disconnect_work_handler(struct gsi_data_port *d_port)
{
	int ret;
	struct f_gsi *gsi = d_port_to_gsi(d_port);

	ret = ipa_usb_xdci_disconnect(gsi->d_port.out_channel_handle,
			gsi->d_port.in_channel_handle, gsi->prot_id);
	if (ret)
		pr_err("%s: IPA disconnect failed (%d)\n",
				__func__, ret);

	/* invalidate channel handles*/
	gsi->d_port.in_channel_handle = -EINVAL;
	gsi->d_port.out_channel_handle = -EINVAL;

	usb_gsi_ep_op(gsi->d_port.in_ep, NULL, GSI_EP_OP_FREE_TRBS);

	if (gsi->d_port.out_ep)
		usb_gsi_ep_op(gsi->d_port.out_ep, NULL, GSI_EP_OP_FREE_TRBS);

	/*
	 * Unconfig the gsi eps after freeing the trbs. If done in
	 * gsi_disable() then since gsi_disable() is called in interrupt context
	 * and the usb_gsi_ep_op() for GSI_EP_OP_FREE_TRBS which is called from
	 * ipa_disconnect_work_handler() a worker thread, can get delayed. So
	 * when gsi_disable() unconfigures the eps, usb_gsi_ep_op() will not be
	 * executed which leads to a memory leak.
	 * Also if this is done in gsi_unbind() then again this is executed in
	 * interrupt context and ipa_disconnect_work_handler() is a worker
	 * thread which can get delayed.
	 */
	if (gadget_is_dwc3(d_port->gadget)) {
		if (gsi->d_port.in_ep)
			msm_ep_unconfig(gsi->d_port.in_ep);
		if (gsi->d_port.out_ep)
			msm_ep_unconfig(gsi->d_port.out_ep);
	}

}

static int ipa_suspend_work_handler(struct gsi_data_port *d_port)
{
	int ret = 0;
	bool block_db;
	struct f_gsi *gsi = d_port_to_gsi(d_port);

	if (!usb_gsi_ep_op(gsi->d_port.in_ep, NULL,
				GSI_EP_OP_CHECK_FOR_SUSPEND)) {
		ret = -EFAULT;
		goto done;
	}

	ret = ipa_usb_xdci_suspend(gsi->d_port.out_channel_handle,
				gsi->d_port.in_channel_handle, gsi->prot_id);

	if (!ret) {
		d_port->sm_state = STATE_SUSPENDED;
		pr_debug("%s: STATE SUSPENDED", __func__);
		goto done;
	}

	if (ret == -EFAULT) {
		block_db = false;
		usb_gsi_ep_op(d_port->in_ep, (void *)&block_db,
					GSI_EP_OP_SET_CLR_BLOCK_DBL);
		gsi_wakeup_host(gsi);
	} else if (ret == -EINPROGRESS) {
		d_port->sm_state = STATE_SUSPEND_IN_PROGRESS;
	} else {
		pr_err("%s: Error %d for %d", __func__, ret, gsi->prot_id);
	}

	pr_debug("%s:ipa_usb_xdci_suspend returns %d\n", __func__, ret);

done:
	return ret;
}

static void ipa_resume_work_handler(struct gsi_data_port *d_port)
{
	bool block_db;
	struct f_gsi *gsi = d_port_to_gsi(d_port);
	int ret;

	ret = ipa_usb_xdci_resume(gsi->d_port.out_channel_handle,
					gsi->d_port.in_channel_handle,
					gsi->prot_id);
	if (ret)
		pr_debug("%s:ipa_usb_xdci_resume returns %d\n", __func__, ret);

	block_db = false;
	usb_gsi_ep_op(d_port->in_ep, (void *)&block_db,
			GSI_EP_OP_SET_CLR_BLOCK_DBL);
}

static void ipa_work_handler(struct work_struct *w)
{
	struct gsi_data_port *d_port = container_of(w, struct gsi_data_port,
						  usb_ipa_w);
	u8 event;
	int ret = 0;

	event = read_event(d_port);

	pr_debug("%s: event = %x sm_state %x\n", __func__,
			event, d_port->sm_state);

	switch (d_port->sm_state) {
	case STATE_UNINITIALIZED:
		if (event == EVT_INITIALIZED) {
			d_port->sm_state = STATE_INITIALIZED;
			pr_debug("%s: STATE INITIALIZED", __func__);
		}
		break;
	case STATE_INITIALIZED:
		if (event == EVT_CONNECT_IN_PROGRESS) {
			ipa_connect_channels(d_port);
			d_port->sm_state = STATE_CONNECT_IN_PROGRESS;
			pr_debug("%s: STATE CONNECT IN PROGRESS", __func__);
		}
		break;
	case STATE_CONNECT_IN_PROGRESS:
		if (event == EVT_HOST_READY) {
			ipa_data_path_enable(d_port);
			d_port->sm_state = STATE_CONNECTED;
			pr_debug("%s: STATE CTRL CONNECTED", __func__);
		} else if (event == EVT_CONNECTED) {
			ipa_data_path_enable(d_port);
			d_port->sm_state = STATE_CONNECTED;
			pr_debug("%s: STATE CTRL CONNECTED", __func__);
		} else if (event == EVT_SUSPEND) {
			if (peek_event(d_port) == EVT_DISCONNECTED) {
				read_event(d_port);
				ipa_disconnect_work_handler(d_port);
				d_port->sm_state = STATE_INITIALIZED;
				usb_gadget_autopm_put_async(d_port->gadget);
				pr_debug("%s: STATE DISCONNECTED", __func__);
				break;
			}
			ret = ipa_suspend_work_handler(d_port);
			if (!ret)
				usb_gadget_autopm_put_async(d_port->gadget);
		} else if (event == EVT_DISCONNECTED) {
			ipa_disconnect_work_handler(d_port);
			d_port->sm_state = STATE_INITIALIZED;
			usb_gadget_autopm_put_async(d_port->gadget);
			pr_debug("%s: STATE DISCONNECTED", __func__);
		}
		break;
	case STATE_CONNECTED:
		if (event == EVT_DISCONNECTED) {
			ipa_disconnect_work_handler(d_port);
			d_port->sm_state = STATE_INITIALIZED;
			usb_gadget_autopm_put_async(d_port->gadget);

			pr_debug("%s: STATE DISCONNECTED", __func__);
		} else if (event == EVT_SUSPEND) {
			if (peek_event(d_port) == EVT_DISCONNECTED) {
				ipa_disconnect_work_handler(d_port);
				d_port->sm_state = STATE_INITIALIZED;
				usb_gadget_autopm_put_async(d_port->gadget);

				pr_debug("%s: STATE DISCONNECTED", __func__);
				break;
			}
			ipa_suspend_work_handler(d_port);
			if (!ret)
				usb_gadget_autopm_put_async(d_port->gadget);
		} else if (event == EVT_CONNECTED) {
			d_port->sm_state = STATE_CONNECTED;
			pr_debug("%s: DATA PATH CONNECTED", __func__);
		}
		break;
	case STATE_DISCONNECTED:
		if (event == EVT_CONNECT_IN_PROGRESS) {
			ipa_connect_channels(d_port);
			d_port->sm_state = STATE_CONNECT_IN_PROGRESS;
			pr_debug("%s: STATE CONNECT IN PROGRESS", __func__);
		} else if (event == EVT_UNINITIALIZED) {
			d_port->sm_state = STATE_UNINITIALIZED;
			pr_debug("%s: STATE UNINITIALIZED", __func__);
		}
		break;
	case STATE_SUSPEND_IN_PROGRESS:
		if (event == EVT_IPA_SUSPEND) {
			d_port->sm_state = STATE_SUSPENDED;
			usb_gadget_autopm_put_async(d_port->gadget);
		} else	if (event == EVT_RESUMED) {
			ipa_resume_work_handler(d_port);
			d_port->sm_state = STATE_CONNECTED;
			/*
			 * Increment usage count here to disallow gadget
			 * parent suspend. This counter will decrement
			 * after IPA disconnect is done in disconnect work
			 * (due to cable disconnect) or in suspended state.
			 */
			usb_gadget_autopm_get_noresume(d_port->gadget);
			pr_debug("%s: STATE CONNECTED", __func__);
		} else if (event == EVT_DISCONNECTED) {
			ipa_disconnect_work_handler(d_port);
			d_port->sm_state = STATE_INITIALIZED;
			usb_gadget_autopm_put_async(d_port->gadget);
			pr_debug("%s: STATE DISCONNECTED", __func__);
		}
		break;

	case STATE_SUSPENDED:
		if (event == EVT_RESUMED) {
			ipa_resume_work_handler(d_port);
			d_port->sm_state = STATE_CONNECTED;
			/*
			 * Increment usage count here to disallow gadget
			 * parent suspend. This counter will decrement
			 * after IPA handshake is done in disconnect work
			 * (due to cable disconnect) or in suspended state.
			 */
			usb_gadget_autopm_get_noresume(d_port->gadget);

			pr_debug("%s: STATE CONNECTED", __func__);
		} else if (event == EVT_DISCONNECTED) {
			ipa_disconnect_work_handler(d_port);
			d_port->sm_state = STATE_INITIALIZED;
			pr_debug("%s: STATE DISCONNECTED", __func__);
		}
		break;
	default:
		pr_debug("%s: Invalid state to SM", __func__);
	}

	if (peek_event(d_port) != EVT_NONE) {
		pr_debug("%s: New events to process", __func__);
		queue_work(d_port->ipa_usb_wq, &d_port->usb_ipa_w);
	}
}

static struct gsi_ctrl_pkt *gsi_ctrl_pkt_alloc(unsigned len, gfp_t flags)
{
	struct gsi_ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct gsi_ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void gsi_ctrl_pkt_free(struct gsi_ctrl_pkt *pkt)
{
	if (pkt) {
		kfree(pkt->buf);
		kfree(pkt);
	}
}

static void gsi_ctrl_clear_cpkt_queues(struct f_gsi *gsi, bool skip_req_q)
{
	struct gsi_ctrl_pkt *cpkt = NULL;
	struct list_head *act, *tmp;

	spin_lock(&gsi->c_port.lock);
	if (skip_req_q)
		goto clean_resp_q;

	list_for_each_safe(act, tmp, &gsi->c_port.cpkt_req_q) {
		cpkt = list_entry(act, struct gsi_ctrl_pkt, list);
		list_del(&cpkt->list);
		gsi_ctrl_pkt_free(cpkt);
	}
clean_resp_q:
	list_for_each_safe(act, tmp, &gsi->c_port.cpkt_resp_q) {
		cpkt = list_entry(act, struct gsi_ctrl_pkt, list);
		list_del(&cpkt->list);
		gsi_ctrl_pkt_free(cpkt);
	}
	spin_unlock(&gsi->c_port.lock);
}

static int gsi_ctrl_send_cpkt_tomodem(struct f_gsi *gsi, void *buf, size_t len)
{
	unsigned long flags;
	struct gsi_ctrl_port *c_port = &gsi->c_port;
	struct gsi_ctrl_pkt *cpkt;

	spin_lock_irqsave(&c_port->lock, flags);
	/* drop cpkt if port is not open */
	if (!gsi->c_port.is_open) {
		pr_debug("%s: ctrl device %s is not open",
			   __func__, gsi->c_port.name);
		c_port->cpkt_drop_cnt++;
		spin_unlock_irqrestore(&c_port->lock, flags);
		return -ENODEV;
	}

	cpkt = gsi_ctrl_pkt_alloc(len, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("%s: Unable to allocate reset function pkt\n", __func__);
		spin_unlock_irqrestore(&c_port->lock, flags);
		return -ENOMEM;
	}

	memcpy(cpkt->buf, buf, len);
	cpkt->len = len;

	list_add_tail(&cpkt->list, &c_port->cpkt_req_q);
	c_port->host_to_modem++;
	spin_unlock_irqrestore(&c_port->lock, flags);

	pr_debug("%s: Wake up read queue", __func__);
	wake_up(&c_port->read_wq);

	return 0;
}

static int gsi_ctrl_dev_open(struct inode *ip, struct file *fp)
{
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);

	if (!c_port) {
		pr_err("%s: gsi ctrl port %p\n", __func__, c_port);
		return -ENODEV;
	}

	pr_debug("Open gsi ctrl device file name=%s", c_port->name);

	if (c_port->is_open) {
		pr_err("Already opened\n");
		return -EBUSY;
	}

	c_port->is_open = true;

	return 0;
}

static int gsi_ctrl_dev_release(struct inode *ip, struct file *fp)
{
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);

	if (!c_port) {
		pr_err("%s: gsi ctrl port %p\n", __func__, c_port);
		return -ENODEV;
	}

	pr_debug("close gsi ctrl device file name=%s", c_port->name);

	c_port->is_open = false;

	return 0;
}

static ssize_t
gsi_ctrl_dev_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);

	struct gsi_ctrl_pkt *cpkt = NULL;
	unsigned long flags;
	int ret = 0;

	pr_debug("%s: Enter(%zu)\n", __func__, count);

	if (!c_port) {
		pr_err("%s: gsi ctrl port %p\n", __func__, c_port);
		return -ENODEV;
	}

	if (count > GSI_MAX_CTRL_PKT_SIZE) {
		pr_err("Buffer size is too big %zu, should be at most %d\n",
			count, GSI_MAX_CTRL_PKT_SIZE);
		return -EINVAL;
	}

	/* block until a new packet is available */
	spin_lock_irqsave(&c_port->lock, flags);
	while (list_empty(&c_port->cpkt_req_q)) {
		pr_debug("Requests list is empty. Wait.\n");
		spin_unlock_irqrestore(&c_port->lock, flags);
		ret = wait_event_interruptible(c_port->read_wq,
			!list_empty(&c_port->cpkt_req_q));
		if (ret < 0) {
			pr_err("Waiting failed\n");
			return -ERESTARTSYS;
		}
		pr_debug("Received request packet\n");
		spin_lock_irqsave(&c_port->lock, flags);
	}

	cpkt = list_first_entry(&c_port->cpkt_req_q, struct gsi_ctrl_pkt,
							list);
	list_del(&cpkt->list);
	spin_unlock_irqrestore(&c_port->lock, flags);

	if (cpkt->len > count) {
		pr_err("cpkt size too big:%d > buf size:%zu\n",
				cpkt->len, count);
		gsi_ctrl_pkt_free(cpkt);
		return -ENOMEM;
	}

	pr_debug("%s: cpkt size:%d\n", __func__, cpkt->len);

	ret = copy_to_user(buf, cpkt->buf, cpkt->len);
	if (ret) {
		pr_err("copy_to_user failed: err %d\n", ret);
		ret = -EFAULT;
	} else {
		pr_debug("%s: copied %d bytes to user\n", __func__, cpkt->len);
		ret = cpkt->len;
		c_port->copied_to_modem++;
	}

	gsi_ctrl_pkt_free(cpkt);

	pr_debug("%s: Exit(%zu)\n", __func__, count);

	return ret;
}

static ssize_t gsi_ctrl_dev_write(struct file *fp, const char __user *buf,
		size_t count, loff_t *pos)
{
	int ret = 0;
	unsigned long flags;
	struct gsi_ctrl_pkt *cpkt;
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);
	struct f_gsi *gsi = c_port_to_gsi(c_port);
	struct usb_request *req = c_port->notify_req;

	pr_debug("Enter(%zu)\n", count);

	if (!c_port || !req || !req->buf) {
		pr_err("%s: c_port %p req %p req->buf %p\n",
			__func__, c_port, req, req ? req->buf : req);
		return -ENODEV;
	}

	if (!count || count > GSI_MAX_CTRL_PKT_SIZE) {
		pr_err("error: ctrl pkt length %zu\n", count);
		return -EINVAL;
	}

	if (!atomic_read(&gsi->connected)) {
		pr_err("USB cable not connected\n");
		return -ECONNRESET;
	}

	if (gsi->function.func_is_suspended &&
			!gsi->function.func_wakeup_allowed) {
		c_port->cpkt_drop_cnt++;
		pr_err("drop ctrl pkt of len %zu\n", count);
		return -ENOTSUPP;
	}

	cpkt = gsi_ctrl_pkt_alloc(count, GFP_KERNEL);
	if (!cpkt) {
		pr_err("failed to allocate ctrl pkt\n");
		return -ENOMEM;
	}

	ret = copy_from_user(cpkt->buf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed err:%d\n", ret);
		gsi_ctrl_pkt_free(cpkt);
		return ret;
	}
	c_port->copied_from_modem++;

	spin_lock_irqsave(&c_port->lock, flags);
	list_add_tail(&cpkt->list, &c_port->cpkt_resp_q);
	spin_unlock_irqrestore(&c_port->lock, flags);

	ret = gsi_ctrl_send_notification(gsi,
			GSI_CTRL_NOTIFY_RESPONSE_AVAILABLE);

	c_port->modem_to_host++;
	pr_debug("Exit(%zu)\n", count);

	return ret ? ret : count;
}

static long gsi_ctrl_dev_ioctl(struct file *fp, unsigned cmd,
		unsigned long arg)
{
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);
	struct f_gsi *gsi = c_port_to_gsi(c_port);
	struct ep_info info;
	int val, ret = 0;

	if (!c_port) {
		pr_err("%s: gsi ctrl port %p\n", __func__, c_port);
		return -ENODEV;
	}

	switch (cmd) {
	case QTI_CTRL_MODEM_OFFLINE:
		if (gsi->prot_id == IPA_USB_DIAG) {
			pr_debug("%s():Modem Offline not handled\n", __func__);
			goto exit_ioctl;
		}
		atomic_set(&c_port->ctrl_online, 0);
		gsi_ctrl_send_notification(gsi, GSI_CTRL_NOTIFY_OFFLINE);
		gsi_ctrl_clear_cpkt_queues(gsi, true);
		break;
	case QTI_CTRL_MODEM_ONLINE:
		if (gsi->prot_id == IPA_USB_DIAG) {
			pr_debug("%s():Modem Online not handled\n", __func__);
			goto exit_ioctl;
		}

		atomic_set(&c_port->ctrl_online, 1);
		break;
	case QTI_CTRL_GET_LINE_STATE:
		val = atomic_read(&gsi->connected);
		ret = copy_to_user((void __user *)arg, &val, sizeof(val));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("%s: Sent line_state: %d for prot id:%d\n", __func__,
				atomic_read(&gsi->connected), gsi->prot_id);
		break;
	case QTI_CTRL_EP_LOOKUP:
	case GSI_MBIM_EP_LOOKUP:
		pr_debug("%s(): EP_LOOKUP for prot id:%d\n", __func__,
							gsi->prot_id);
		if (!atomic_read(&gsi->connected)) {
			pr_debug("EP_LOOKUP failed: not connected\n");
			ret = -EAGAIN;
			break;
		}

		if (gsi->prot_id == IPA_USB_DIAG &&
				(gsi->d_port.in_channel_handle == -EINVAL)) {
			ret = -EAGAIN;
			break;
		}

		if (gsi->d_port.in_channel_handle == -EINVAL &&
			gsi->d_port.out_channel_handle == -EINVAL) {
			ret = -EAGAIN;
			break;
		}

		info.ph_ep_info.ep_type = GSI_MBIM_DATA_EP_TYPE_HSUSB;
		info.ph_ep_info.peripheral_iface_id = gsi->data_id;
		info.ipa_ep_pair.cons_pipe_num =
		(gsi->prot_id == IPA_USB_DIAG) ? -1 :
				gsi->d_port.out_channel_handle;
		info.ipa_ep_pair.prod_pipe_num = gsi->d_port.in_channel_handle;

		pr_debug("%s(): prot id :%d ep_type:%d intf:%d\n",
				__func__, gsi->prot_id, info.ph_ep_info.ep_type,
				info.ph_ep_info.peripheral_iface_id);

		pr_debug("%s(): ipa_cons_idx:%d ipa_prod_idx:%d\n",
				__func__, info.ipa_ep_pair.cons_pipe_num,
				info.ipa_ep_pair.prod_pipe_num);

		ret = copy_to_user((void __user *)arg, &info,
			sizeof(info));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		break;
	case GSI_MBIM_GET_NTB_SIZE:
		ret = copy_to_user((void __user *)arg,
			&gsi->d_port.ntb_info.ntb_input_size,
			sizeof(gsi->d_port.ntb_info.ntb_input_size));
		if (ret) {
			pr_err("copying to user space failed\n");
			ret = -EFAULT;
		}
		pr_debug("Sent NTB size %d\n",
				gsi->d_port.ntb_info.ntb_input_size);
		break;
	case GSI_MBIM_GET_DATAGRAM_COUNT:
		ret = copy_to_user((void __user *)arg,
			&gsi->d_port.ntb_info.ntb_max_datagrams,
			sizeof(gsi->d_port.ntb_info.ntb_max_datagrams));
		if (ret) {
			pr_err("copying to user space failed\n");
			ret = -EFAULT;
		}
		pr_debug("Sent NTB datagrams count %d\n",
			gsi->d_port.ntb_info.ntb_max_datagrams);
		break;
	default:
		pr_err("wrong parameter");
		ret = -EINVAL;
	}

exit_ioctl:
	return ret;
}

static unsigned int gsi_ctrl_dev_poll(struct file *fp, poll_table *wait)
{
	struct gsi_ctrl_port *c_port = container_of(fp->private_data,
						struct gsi_ctrl_port,
						ctrl_device);
	unsigned long flags;
	unsigned int mask = 0;

	if (!c_port) {
		pr_err("%s: gsi ctrl port %p\n", __func__, c_port);
		return -ENODEV;
	}

	poll_wait(fp, &c_port->read_wq, wait);

	spin_lock_irqsave(&c_port->lock, flags);
	if (!list_empty(&c_port->cpkt_req_q)) {
		mask |= POLLIN | POLLRDNORM;
		pr_debug("%s sets POLLIN for %s\n", __func__, c_port->name);
	}
	spin_unlock_irqrestore(&c_port->lock, flags);

	return mask;
}

/* file operations for rmnet/mbim/dpl devices */
static const struct file_operations gsi_ctrl_dev_fops = {
	.owner = THIS_MODULE,
	.open = gsi_ctrl_dev_open,
	.release = gsi_ctrl_dev_release,
	.read = gsi_ctrl_dev_read,
	.write = gsi_ctrl_dev_write,
	.unlocked_ioctl = gsi_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gsi_ctrl_dev_ioctl,
#endif
	.poll = gsi_ctrl_dev_poll,
};

/* peak (theoretical) bulk transfer rate in bits-per-second */
static unsigned int gsi_xfer_bitrate(struct usb_gadget *g)
{
	if (gadget_is_superspeed(g) && g->speed == USB_SPEED_SUPER)
		return 13 * 1024 * 8 * 1000 * 8;
	else if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return 13 * 512 * 8 * 1000 * 8;
	else
		return 19 * 64 * 1 * 1000 * 8;
}

int gsi_function_ctrl_port_init(enum ipa_usb_teth_prot prot_id)
{
	int ret;
	int sz = GSI_CTRL_NAME_LEN;
	bool ctrl_dev_create = true;
	struct f_gsi *gsi = gsi_prot_ctx[prot_id];

	if (!gsi) {
		pr_err("%s: gsi prot ctx is %p\n", __func__, gsi);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&gsi->c_port.cpkt_req_q);
	INIT_LIST_HEAD(&gsi->c_port.cpkt_resp_q);

	spin_lock_init(&gsi->c_port.lock);

	init_waitqueue_head(&gsi->c_port.read_wq);

	if (prot_id == IPA_USB_RMNET)
		strlcat(gsi->c_port.name, GSI_RMNET_CTRL_NAME, sz);
	else if (prot_id == IPA_USB_MBIM)
		strlcat(gsi->c_port.name, GSI_MBIM_CTRL_NAME, sz);
	else if (prot_id == IPA_USB_DIAG)
		strlcat(gsi->c_port.name, GSI_DPL_CTRL_NAME, sz);
	else
		ctrl_dev_create = false;

	if (!ctrl_dev_create)
		return 0;

	gsi->c_port.ctrl_device.name = gsi->c_port.name;
	gsi->c_port.ctrl_device.fops = &gsi_ctrl_dev_fops;
	gsi->c_port.ctrl_device.minor = MISC_DYNAMIC_MINOR;

	ret = misc_register(&gsi->c_port.ctrl_device);
	if (ret) {
		pr_err("%s: misc register failed for prot id %d\n",
				__func__, prot_id);
		return ret;
	}

	return 0;
}

struct net_device *gsi_rndis_get_netdev(const char *netname)
{
	struct net_device *net_dev;

	net_dev = dev_get_by_name(&init_net, netname);
	if (!net_dev)
		return ERR_PTR(-EINVAL);

	/*
	 * Decrement net_dev refcount as it was incremented in
	 * dev_get_by_name().
	 */
	dev_put(net_dev);
	return net_dev;
}

static void gsi_rndis_open(struct f_gsi *rndis)
{
	struct usb_composite_dev *cdev = rndis->function.config->cdev;

	pr_debug("%s\n", __func__);

	rndis_set_param_medium(rndis->config, RNDIS_MEDIUM_802_3,
				gsi_xfer_bitrate(cdev->gadget) / 100);
	rndis_signal_connect(rndis->config);
}

void gsi_rndis_ipa_reset_trigger(void)
{
	struct f_gsi *rndis = gsi_prot_ctx[IPA_USB_RNDIS];
	unsigned long flags;

	if (!rndis) {
		pr_err("%s: gsi prot ctx is %p\n", __func__, rndis);
		return;
	}

	spin_lock_irqsave(&rndis->d_port.lock, flags);
	if (!rndis) {
		pr_err("%s: No RNDIS instance", __func__);
		spin_unlock_irqrestore(&rndis->d_port.lock, flags);
		return;
	}

	rndis->d_port.net_ready_trigger = false;
	spin_unlock_irqrestore(&rndis->d_port.lock, flags);
}

void gsi_rndis_flow_ctrl_enable(bool enable)
{
	struct f_gsi *rndis = gsi_prot_ctx[IPA_USB_RNDIS];
	struct gsi_data_port *d_port;

	if (!rndis) {
		pr_err("%s: gsi prot ctx is %p\n", __func__, rndis);
		return;
	}

	d_port = &rndis->d_port;

	if (enable)	{
		gsi_rndis_ipa_reset_trigger();
		usb_gsi_ep_op(d_port->in_ep, NULL, GSI_EP_OP_ENDXFER);
		usb_gsi_ep_op(d_port->out_ep, NULL, GSI_EP_OP_ENDXFER);
		post_event(d_port, EVT_DISCONNECTED);
	} else {
		post_event(d_port, EVT_HOST_READY);
	}

	queue_work(rndis->d_port.ipa_usb_wq, &rndis->d_port.usb_ipa_w);
}

/*
 * This function handles the Microsoft-specific OS descriptor control
 * requests that are issued by Windows host drivers to determine the
 * configuration containing the MBIM function.
 *
 * This function handles two specific device requests,
 * and only when a configuration has not yet been selected.
 */
static int gsi_os_desc_ctrlrequest(struct usb_composite_dev *cdev,
			    const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);

	/* only respond to OS desc when no configuration selected */
	if (cdev->config ||
			!mbim_gsi_ext_config_desc.function.subCompatibleID[0])
		return value;

	pr_debug("%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

	/* Handle MSFT OS string */
	if (ctrl->bRequestType ==
			(USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE)
			&& ctrl->bRequest == USB_REQ_GET_DESCRIPTOR
			&& (w_value >> 8) == USB_DT_STRING
			&& (w_value & 0xFF) == GSI_MBIM_OS_STRING_ID) {

		value = (w_length < sizeof(mbim_gsi_os_string) ?
				w_length : sizeof(mbim_gsi_os_string));
		memcpy(cdev->req->buf, mbim_gsi_os_string, value);

	} else if (ctrl->bRequestType ==
			(USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
			&& ctrl->bRequest == MBIM_VENDOR_CODE && w_index == 4) {

		/* Handle Extended OS descriptor */
		value = (w_length < sizeof(mbim_gsi_ext_config_desc) ?
				w_length : sizeof(mbim_gsi_ext_config_desc));
		memcpy(cdev->req->buf, &mbim_gsi_ext_config_desc, value);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		int rc;

		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			pr_err("response queue error: %d\n", rc);
	}
	return value;
}

static int queue_notification_request(struct f_gsi *gsi)
{
	int ret;
	unsigned long flags;
	struct usb_cdc_notification *event;
	struct gsi_ctrl_pkt *cpkt;

	ret = usb_func_ep_queue(&gsi->function, gsi->c_port.notify,
			   gsi->c_port.notify_req, GFP_ATOMIC);
	if (ret == -ENOTSUPP || (ret < 0 && ret != -EAGAIN)) {
		spin_lock_irqsave(&gsi->c_port.lock, flags);
		/* check if device disconnected while we dropped lock */
		if (atomic_read(&gsi->connected) &&
			!list_empty(&gsi->c_port.cpkt_resp_q)) {
			cpkt = list_first_entry(&gsi->c_port.cpkt_resp_q,
					struct gsi_ctrl_pkt, list);
			list_del(&cpkt->list);
			atomic_dec(&gsi->c_port.notify_count);
			pr_err("drop ctrl pkt of len %d error %d\n", cpkt->len,
				ret);
			gsi_ctrl_pkt_free(cpkt);
		}
		gsi->c_port.cpkt_drop_cnt++;
		spin_unlock_irqrestore(&gsi->c_port.lock, flags);
	} else {
		ret = 0;
		event = gsi->c_port.notify_req->buf;
		pr_debug("%s:Queued Notify type %02x\n", __func__,
				event->bNotificationType);
	}

	return ret;
}
static int gsi_ctrl_send_notification(struct f_gsi *gsi,
		enum gsi_ctrl_notify_state state)
{
	__le32 *data;
	struct usb_cdc_notification *event;
	struct usb_request *req = gsi->c_port.notify_req;
	struct usb_composite_dev *cdev = gsi->function.config->cdev;

	if (!atomic_read(&gsi->connected)) {
		pr_debug("%s: cable disconnect\n", __func__);
		return -ENODEV;
	}

	event = req->buf;

	switch (state) {
	case GSI_CTRL_NOTIFY_NONE:
		if (atomic_read(&gsi->c_port.notify_count) > 0)
			pr_debug("GSI_CTRL_NOTIFY_NONE %d\n",
			atomic_read(&gsi->c_port.notify_count));
		else
			pr_debug("No pending notifications\n");
		return 0;
	case GSI_CTRL_NOTIFY_CONNECT:
		event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
		event->wValue = cpu_to_le16(1);
		event->wLength = cpu_to_le16(0);
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_SPEED;
		break;
	case GSI_CTRL_NOTIFY_SPEED:
		event->bNotificationType = USB_CDC_NOTIFY_SPEED_CHANGE;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(8);

		/* SPEED_CHANGE data is up/down speeds in bits/sec */
		data = req->buf + sizeof(*event);
		data[0] = cpu_to_le32(gsi_xfer_bitrate(cdev->gadget));
		data[1] = data[0];

		pr_debug("notify speed %d\n", gsi_xfer_bitrate(cdev->gadget));
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_NONE;
		break;
	case GSI_CTRL_NOTIFY_OFFLINE:
		event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(0);
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_NONE;
		break;
	case GSI_CTRL_NOTIFY_RESPONSE_AVAILABLE:
		event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(0);
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_RESPONSE_AVAILABLE;

		if (gsi->prot_id == IPA_USB_RNDIS) {
			data = req->buf;
			data[0] = cpu_to_le32(1);
			data[1] = cpu_to_le32(0);
		}
		break;
	default:
		pr_err("%s:unknown notify state\n", __func__);
		return -EINVAL;
	}

	pr_debug("send Notify type %02x\n", event->bNotificationType);

	if (atomic_inc_return(&gsi->c_port.notify_count) != 1) {
		pr_debug("delay ep_queue: notify req is busy[%d]\n",
			atomic_read(&gsi->c_port.notify_count));
		return 0;
	}

	return queue_notification_request(gsi);
}

static void gsi_ctrl_notify_resp_complete(struct usb_ep *ep,
					struct usb_request *req)
{
	struct f_gsi *gsi = req->context;
	struct usb_cdc_notification *event = req->buf;
	int status = req->status;

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_NONE;
		atomic_set(&gsi->c_port.notify_count, 0);
		pr_debug("ESHUTDOWN/ECONNRESET, connection gone\n");
		gsi_ctrl_clear_cpkt_queues(gsi, false);
		gsi_ctrl_send_cpkt_tomodem(gsi, NULL, 0);
		break;
	default:
		pr_err("Unknown event %02x --> %d\n",
			event->bNotificationType, req->status);
		/* FALLTHROUGH */
	case 0:
		/*
		  * handle multiple pending resp available
		  * notifications by queuing same until we're done,
		  * rest of the notification require queuing new
		  * request.
		  */
		if (!atomic_dec_and_test(&gsi->c_port.notify_count)) {
			pr_debug("notify_count = %d\n",
			atomic_read(&gsi->c_port.notify_count));
			 queue_notification_request(gsi);
		} else if (gsi->c_port.notify_state != GSI_CTRL_NOTIFY_NONE &&
				gsi->c_port.notify_state !=
				GSI_CTRL_NOTIFY_RESPONSE_AVAILABLE) {
			gsi_ctrl_send_notification(gsi,
					gsi->c_port.notify_state);
		}
		break;
	}
}

static void gsi_rndis_response_available(void *_rndis)
{
	struct f_gsi *gsi = _rndis;

	gsi_ctrl_send_notification(gsi, GSI_CTRL_NOTIFY_RESPONSE_AVAILABLE);
}

static void gsi_rndis_command_complete(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_gsi *rndis = req->context;
	int status;

	status = rndis_msg_parser(rndis->config, (u8 *) req->buf);
	if (status < 0)
		pr_err("RNDIS command error %d, %d/%d\n",
			status, req->actual, req->length);
}

static void
gsi_ctrl_set_ntb_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* now for SET_NTB_INPUT_SIZE only */
	unsigned in_size = 0;
	struct f_gsi *gsi = req->context;
	struct gsi_ntb_info *ntb = NULL;

	pr_debug("dev:%p\n", gsi);

	req->context = NULL;
	if (req->status || req->actual != req->length) {
		pr_err("Bad control-OUT transfer\n");
		goto invalid;
	}

	if (req->length == 4) {
		in_size = get_unaligned_le32(req->buf);
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
		in_size > le32_to_cpu(mbim_gsi_ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
	} else if (req->length == 8) {
		ntb = (struct gsi_ntb_info *)req->buf;
		in_size = get_unaligned_le32(&(ntb->ntb_input_size));
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
		in_size > le32_to_cpu(mbim_gsi_ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
		gsi->d_port.ntb_info.ntb_max_datagrams =
			get_unaligned_le16(&(ntb->ntb_max_datagrams));
	} else {
		pr_err("Illegal NTB length %d\n", in_size);
		goto invalid;
	}

	pr_debug("Set NTB INPUT SIZE %d\n", in_size);

	gsi->d_port.ntb_info.ntb_input_size = in_size;
	return;

invalid:
	usb_ep_set_halt(ep);
}

static void gsi_ctrl_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gsi *gsi = req->context;

	gsi_ctrl_send_cpkt_tomodem(gsi, req->buf, req->actual);
}

static void gsi_ctrl_reset_cmd_complete(struct usb_ep *ep,
		struct usb_request *req)
{
	struct f_gsi *gsi = req->context;

	gsi_ctrl_send_cpkt_tomodem(gsi, req->buf, 0);
}

static int
gsi_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_gsi *gsi = func_to_gsi(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	int id, value = -EOPNOTSUPP;
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	struct gsi_ctrl_pkt *cpkt;
	u8 *buf;
	u32 n;

	if (!atomic_read(&gsi->connected)) {
		pr_debug("usb cable is not connected\n");
		return -ENOTCONN;
	}

	/* rmnet and dpl does not have ctrl_id */
	if (gsi->ctrl_id == -ENODEV)
		id = gsi->data_id;
	else
		id = gsi->ctrl_id;

	/* composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 */
	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_RESET_FUNCTION:

		pr_debug("USB_CDC_RESET_FUNCTION\n");
		value = 0;
		req->complete = gsi_ctrl_reset_cmd_complete;
		req->context = gsi;
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		pr_debug("USB_CDC_SEND_ENCAPSULATED_COMMAND\n");

		if (w_value || w_index != id)
			goto invalid;
		/* read the request; process it later */
		value = w_length;
		if (gsi->prot_id == IPA_USB_RNDIS)
			req->complete = gsi_rndis_command_complete;
		else
			req->complete = gsi_ctrl_cmd_complete;
		/* later, rndis_response_available() sends a notification */
		break;
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		pr_debug("USB_CDC_GET_ENCAPSULATED_RESPONSE\n");
		if (w_value || w_index != id)
			goto invalid;

		if (gsi->prot_id == IPA_USB_RNDIS) {
			/* return the result */
			buf = rndis_get_next_response(gsi->config, &n);
			if (buf) {
				memcpy(req->buf, buf, n);
				rndis_free_response(gsi->config, buf);
				value = n;
			}
			break;
		}

		spin_lock(&gsi->c_port.lock);
		if (list_empty(&gsi->c_port.cpkt_resp_q)) {
			pr_debug("ctrl resp queue empty\n");
			spin_unlock(&gsi->c_port.lock);
			break;
		}

		cpkt = list_first_entry(&gsi->c_port.cpkt_resp_q,
					struct gsi_ctrl_pkt, list);
		list_del(&cpkt->list);
		spin_unlock(&gsi->c_port.lock);

		value = min_t(unsigned, w_length, cpkt->len);
		memcpy(req->buf, cpkt->buf, value);
		gsi_ctrl_pkt_free(cpkt);

		pr_debug("copied encapsulated_response %d bytes\n",
			value);
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_ETHERNET_PACKET_FILTER:
		/* see 6.2.30: no data, wIndex = interface,
		 * wValue = packet filter bitmap
		 */
		if (w_length != 0 || w_index != id)
			goto invalid;
		pr_debug("packet filter %02x\n", w_value);
		/* REVISIT locking of cdc_filter.  This assumes the UDC
		 * driver won't have a concurrent packet TX irq running on
		 * another CPU; or that if it does, this write is atomic...
		 */
		gsi->d_port.cdc_filter = w_value;
		value = 0;
		break;
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_PARAMETERS:
		pr_debug("USB_CDC_GET_NTB_PARAMETERS\n");

		if (w_length == 0 || w_value != 0 || w_index != id)
			break;

		value = w_length > sizeof(mbim_gsi_ntb_parameters) ?
			sizeof(mbim_gsi_ntb_parameters) : w_length;
		memcpy(req->buf, &mbim_gsi_ntb_parameters, value);
		break;
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_INPUT_SIZE:

		pr_debug("USB_CDC_GET_NTB_INPUT_SIZE\n");

		if (w_length < 4 || w_value != 0 || w_index != id)
			break;

		put_unaligned_le32(gsi->d_port.ntb_info.ntb_input_size,
				req->buf);
		value = 4;
		pr_debug("Reply to host INPUT SIZE %d\n",
			 gsi->d_port.ntb_info.ntb_input_size);
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NTB_INPUT_SIZE:
		pr_debug("USB_CDC_SET_NTB_INPUT_SIZE\n");

		if (w_length != 4 && w_length != 8) {
			pr_err("wrong NTB length %d\n", w_length);
			break;
		}

		if (w_value != 0 || w_index != id)
			break;

		req->complete = gsi_ctrl_set_ntb_cmd_complete;
		req->length = w_length;
		req->context = gsi;

		value = req->length;
		break;
	default:
invalid:
		pr_err("invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		pr_debug("req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->context = gsi;
		req->zero = (value < w_length);
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("response on err %d\n", value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

/*
 * Because the data interface supports multiple altsettings,
 * function *MUST* implement a get_alt() method.
 */
static int gsi_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_gsi *gsi = func_to_gsi(f);

	if (intf == gsi->ctrl_id)
		return 0;
	else if (intf == gsi->data_id)
		return gsi->data_interface_up;

	return -EINVAL;
}

static int gsi_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gsi	 *gsi = func_to_gsi(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct net_device	*net;
	int ret;

	pr_debug("intf=%u, alt=%u\n", intf, alt);

	/* Control interface has only altsetting 0 */
	if (intf == gsi->ctrl_id || gsi->prot_id == IPA_USB_RMNET) {
		if (alt != 0)
			goto fail;

		if (!gsi->c_port.notify)
			goto fail;

		if (gsi->c_port.notify->driver_data) {
			pr_debug("reset gsi control %d\n", intf);
			usb_ep_disable(gsi->c_port.notify);
		}

		ret = config_ep_by_speed(cdev->gadget, f,
					gsi->c_port.notify);
		if (ret) {
			gsi->c_port.notify->desc = NULL;
			pr_err("Failed configuring notify ep %s: err %d\n",
				gsi->c_port.notify->name, ret);
			goto fail;
		}

		ret = usb_ep_enable(gsi->c_port.notify);
		if (ret) {
			pr_err("usb ep#%s enable failed, err#%d\n",
				gsi->c_port.notify->name, ret);
			goto fail;
		}
		gsi->c_port.notify->driver_data = gsi;
	}

	/* Data interface has two altsettings, 0 and 1 */
	if (intf == gsi->data_id) {
		if (!gadget_is_dwc3(cdev->gadget))
			goto notify_ep_disable;

		gsi->d_port.net_ready_trigger = false;
		/* for rndis and rmnet alt is always 0 update alt accordingly */
		if (gsi->prot_id == IPA_USB_RNDIS ||
				gsi->prot_id == IPA_USB_RMNET ||
				gsi->prot_id == IPA_USB_DIAG) {
			if (gsi->d_port.in_ep &&
				!gsi->d_port.in_ep->driver_data)
				alt = 1;
			else
				alt = 0;
		}

		if (alt > 1)
			goto notify_ep_disable;

		if (gsi->data_interface_up == alt)
			return 0;

		if (gsi->d_port.in_ep && gsi->d_port.in_ep->driver_data)
			gsi->d_port.ntb_info.ntb_input_size =
				MBIM_NTB_DEFAULT_IN_SIZE;
		if (alt == 1) {
			if (gsi->d_port.in_ep && !gsi->d_port.in_ep->desc
				&& config_ep_by_speed(cdev->gadget, f,
					gsi->d_port.in_ep)) {
				gsi->d_port.in_ep->desc = NULL;
				goto notify_ep_disable;
			}

			if (gsi->d_port.out_ep && !gsi->d_port.out_ep->desc
				&& config_ep_by_speed(cdev->gadget, f,
					gsi->d_port.out_ep)) {
				gsi->d_port.out_ep->desc = NULL;
				goto notify_ep_disable;
			}

			if (gsi->d_port.in_ep &&
				msm_ep_config(gsi->d_port.in_ep)) {
				pr_err("%s: in ep config failed\n", __func__);
				goto notify_ep_disable;
			}

			if (gsi->d_port.out_ep &&
				msm_ep_config(gsi->d_port.out_ep)) {
				pr_err("%s: out ep config failed\n", __func__);
				goto in_ep_unconfig;
			}

			/* Configure EPs for GSI */
			if (gsi->d_port.in_ep) {
				if (gsi->prot_id == IPA_USB_DIAG)
					gsi->d_port.in_ep->ep_intr_num = 3;
				else
					gsi->d_port.in_ep->ep_intr_num = 2;
				usb_gsi_ep_op(gsi->d_port.in_ep,
					&gsi->d_port.in_request,
						GSI_EP_OP_CONFIG);
			}

			if (gsi->d_port.out_ep) {
				gsi->d_port.out_ep->ep_intr_num = 1;
				usb_gsi_ep_op(gsi->d_port.out_ep,
					&gsi->d_port.out_request,
						GSI_EP_OP_CONFIG);
			}

			gsi->d_port.gadget = cdev->gadget;

			if (gsi->prot_id == IPA_USB_RNDIS) {
				gsi_rndis_open(gsi);
				net = gsi_rndis_get_netdev("rndis0");
				if (IS_ERR(net))
					goto out_ep_unconfig;

				pr_debug("RNDIS RX/TX early activation\n");
				gsi->d_port.cdc_filter = 0;
				rndis_set_param_dev(gsi->config, net,
						&gsi->d_port.cdc_filter);
			}

			if (gsi->prot_id == IPA_USB_ECM)
				gsi->d_port.cdc_filter = DEFAULT_FILTER;

			/*
			 * Increment usage count upon cable connect. Decrement
			 * after IPA disconnect is done in disconnect work
			 * (due to cable disconnect) or in suspend work.
			 */
			usb_gadget_autopm_get_noresume(gsi->d_port.gadget);

			post_event(&gsi->d_port, EVT_CONNECT_IN_PROGRESS);
			queue_work(gsi->d_port.ipa_usb_wq,
					&gsi->d_port.usb_ipa_w);
		}
		if (alt == 0 && ((gsi->d_port.in_ep &&
			!gsi->d_port.in_ep->driver_data) ||
			(gsi->d_port.out_ep &&
			!gsi->d_port.out_ep->driver_data))) {
			ipa_disconnect_handler(&gsi->d_port);
			if (gsi->data_interface_up) {
				if ((gsi->d_port.in_ep &&
					msm_ep_unconfig(gsi->d_port.in_ep)) ||
					(gsi->d_port.out_ep &&
					msm_ep_unconfig(gsi->d_port.out_ep))) {
					pr_err("ep_unconfig failed\n");
					goto notify_ep_disable;
				}
			}
		}
		gsi->data_interface_up = alt;
		pr_debug("DATA_INTERFACE id %d, data interface status %d\n",
				gsi->data_id, gsi->data_interface_up);
	}

	atomic_set(&gsi->connected, 1);

	return 0;

out_ep_unconfig:
	if (gsi->d_port.out_ep)
		msm_ep_unconfig(gsi->d_port.out_ep);
in_ep_unconfig:
	if (gsi->d_port.in_ep)
		msm_ep_unconfig(gsi->d_port.in_ep);
notify_ep_disable:
	if (gsi->c_port.notify && gsi->c_port.notify->driver_data)
		usb_ep_disable(gsi->c_port.notify);
fail:
	return -EINVAL;
}

static void gsi_disable(struct usb_function *f)
{
	struct f_gsi *gsi = func_to_gsi(f);

	atomic_set(&gsi->connected, 0);

	if (gsi->prot_id == IPA_USB_RNDIS)
		rndis_uninit(gsi->config);

	 /* Disable Control Path */
	if (gsi->c_port.notify &&
		gsi->c_port.notify->driver_data) {
		usb_ep_disable(gsi->c_port.notify);
		gsi->c_port.notify->driver_data = NULL;
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_NONE;
	}

	atomic_set(&gsi->c_port.notify_count, 0);

	gsi_ctrl_clear_cpkt_queues(gsi, false);
	/* send 0 len pkt to qti/qbi to notify state change */
	gsi_ctrl_send_cpkt_tomodem(gsi, NULL, 0);

	/* Disable Data Path  - only if it was initialized already (alt=1) */
	if (!gsi->data_interface_up) {
		pr_debug("data interface is not opened. Returning\n");
		return;
	}

	gsi->data_interface_up = false;

	pr_debug("%s deactivated\n", gsi->function.name);
	ipa_disconnect_handler(&gsi->d_port);
	post_event(&gsi->d_port, EVT_DISCONNECTED);
	queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);
}

static void gsi_suspend(struct usb_function *f)
{
	bool block_db;
	struct f_gsi *gsi = func_to_gsi(f);
	bool remote_wakeup_allowed;

	if (f->config->cdev->gadget->speed == USB_SPEED_SUPER)
		remote_wakeup_allowed = f->func_wakeup_allowed;
	else
		remote_wakeup_allowed = f->config->cdev->gadget->remote_wakeup;

	pr_info("%s(): gsi suspend: remote_wakeup_allowed:%d\n:",
					__func__, remote_wakeup_allowed);

	if (!remote_wakeup_allowed) {
		if (gsi->prot_id == IPA_USB_RNDIS)
			rndis_flow_control(gsi->config, true);
		/*
		 * When remote wakeup is disabled, IPA is disconnected
		 * because it cannot send new data until the USB bus is
		 * resumed. Endpoint descriptors info is saved before it
		 * gets reset by the BAM disconnect API. This lets us
		 * restore this info when the USB bus is resumed.
		 */
		if (gsi->d_port.in_ep)
			gsi->in_ep_desc_backup = gsi->d_port.in_ep->desc;
		if (gsi->d_port.out_ep)
			gsi->out_ep_desc_backup = gsi->d_port.out_ep->desc;

		ipa_disconnect_handler(&gsi->d_port);

		post_event(&gsi->d_port, EVT_DISCONNECTED);
		queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);
		pr_debug("%s(): Disconnecting\n", __func__);
	} else {
		block_db = true;
		usb_gsi_ep_op(gsi->d_port.in_ep, (void *)&block_db,
				GSI_EP_OP_SET_CLR_BLOCK_DBL);
		post_event(&gsi->d_port, EVT_SUSPEND);
		queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);
	}

	pr_debug("gsi suspended\n");
}

static void gsi_resume(struct usb_function *f)
{
	struct f_gsi *gsi = func_to_gsi(f);
	bool remote_wakeup_allowed;
	struct usb_composite_dev *cdev = f->config->cdev;

	pr_debug("%s: gsi resumed\n", __func__);

	/*
	 * If the function is in USB3 Function Suspend state, resume is
	 * canceled. In this case resume is done by a Function Resume request.
	 */
	if ((cdev->gadget->speed == USB_SPEED_SUPER) &&
		f->func_is_suspended)
		return;

	if (f->config->cdev->gadget->speed == USB_SPEED_SUPER)
		remote_wakeup_allowed = f->func_wakeup_allowed;
	else
		remote_wakeup_allowed = f->config->cdev->gadget->remote_wakeup;

	if (!remote_wakeup_allowed) {
		gsi->d_port.in_ep->desc = gsi->in_ep_desc_backup;
		gsi->d_port.out_ep->desc = gsi->out_ep_desc_backup;

		/* Configure EPs for GSI */
		gsi->d_port.out_ep->ep_intr_num = 1;
		usb_gsi_ep_op(gsi->d_port.out_ep, &gsi->d_port.out_request,
				GSI_EP_OP_CONFIG);
		gsi->d_port.in_ep->ep_intr_num = 2;
		usb_gsi_ep_op(gsi->d_port.in_ep, &gsi->d_port.in_request,
				GSI_EP_OP_CONFIG);
		post_event(&gsi->d_port, EVT_CONNECT_IN_PROGRESS);

		/*
		 * Linux host does not send RNDIS_MSG_INIT or non-zero
		 * RNDIS_MESSAGE_PACKET_FILTER after performing bus resume.
		 * Trigger state machine explicitly on resume.
		 */
		if (gsi->prot_id == IPA_USB_RNDIS)
			rndis_flow_control(gsi->config, false);
	} else
		post_event(&gsi->d_port, EVT_RESUMED);

	queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);

	if (!gsi->c_port.notify->desc)
		config_ep_by_speed(cdev->gadget, f, gsi->c_port.notify);

	atomic_set(&gsi->c_port.notify_count, 0);
	pr_debug("%s: gsi resume completed\n", __func__);
}

static int gsi_func_suspend(struct usb_function *f, u8 options)
{
	bool func_wakeup_allowed;

	pr_debug("Got Function Suspend(%u) command for %s function\n",
		options, f->name ? f->name : "");

	func_wakeup_allowed =
		((options & FUNC_SUSPEND_OPT_RW_EN_MASK) != 0);

	if (options & FUNC_SUSPEND_OPT_SUSP_MASK) {
		f->func_wakeup_allowed = func_wakeup_allowed;
		if (!f->func_is_suspended) {
			gsi_suspend(f);
			f->func_is_suspended = true;
		}
	} else {
		if (f->func_is_suspended) {
			f->func_is_suspended = false;
			gsi_resume(f);
		}
		f->func_wakeup_allowed = func_wakeup_allowed;
	}

	return 0;
}

static int gsi_update_function_bind_params(struct f_gsi *gsi,
	struct usb_composite_dev *cdev,
	struct gsi_function_bind_info *info)
{
	struct usb_ep *ep;
	struct usb_cdc_notification *event;
	struct usb_function *f = &gsi->function;
	u32 len = 0;
	int status;

	/* maybe allocate device-global string IDs */
	if (info->string_defs[0].id != 0)
		goto skip_string_id_alloc;

	if (info->ctrl_str_idx >= 0 && info->ctrl_desc) {
		/* ctrl interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		info->string_defs[info->ctrl_str_idx].id = status;
		info->ctrl_desc->iInterface = status;
	}

	if (info->data_str_idx >= 0 && info->data_desc) {
		/* data interface label */
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		info->string_defs[info->data_str_idx].id = status;
		info->data_desc->iInterface = status;
	}

	if (info->iad_str_idx >= 0 && info->iad_desc) {
		/* IAD iFunction label */
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		info->string_defs[info->iad_str_idx].id = status;
		info->iad_desc->iFunction = status;
	}

	if (info->mac_str_idx >= 0 && info->cdc_eth_desc) {
		/* IAD iFunction label */
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		info->string_defs[info->mac_str_idx].id = status;
		info->cdc_eth_desc->iMACAddress = status;
	}

skip_string_id_alloc:
	if (info->ctrl_desc)
		info->ctrl_desc->bInterfaceNumber = gsi->ctrl_id;

	if (info->iad_desc)
		info->iad_desc->bFirstInterface = gsi->ctrl_id;

	if (info->union_desc) {
		info->union_desc->bMasterInterface0 = gsi->ctrl_id;
		info->union_desc->bSlaveInterface0 = gsi->data_id;
	}

	if (info->data_desc)
		info->data_desc->bInterfaceNumber = gsi->data_id;

	if (info->data_nop_desc)
		info->data_nop_desc->bInterfaceNumber = gsi->data_id;

	/* allocate instance-specific endpoints */
	if (info->fs_in_desc) {
		ep = usb_ep_autoconfig(cdev->gadget, info->fs_in_desc);
		if (!ep)
			goto fail;
		gsi->d_port.in_ep = ep;
		ep->driver_data = cdev;	/* claim */
	}

	if (info->fs_out_desc) {
		ep = usb_ep_autoconfig(cdev->gadget, info->fs_out_desc);
		if (!ep)
			goto fail;
		gsi->d_port.out_ep = ep;
		ep->driver_data = cdev;	/* claim */
	}

	if (info->fs_notify_desc) {
		ep = usb_ep_autoconfig(cdev->gadget, info->fs_notify_desc);
		if (!ep)
			goto fail;
		gsi->c_port.notify = ep;
		ep->driver_data = cdev;	/* claim */

		atomic_set(&gsi->c_port.notify_count, 0);

		/* allocate notification request and buffer */
		gsi->c_port.notify_req = usb_ep_alloc_request(ep, GFP_KERNEL);
		if (!gsi->c_port.notify_req)
			goto fail;

		gsi->c_port.notify_req->buf =
			kmalloc(info->notify_buf_len, GFP_KERNEL);
		if (!gsi->c_port.notify_req->buf)
			goto fail;

		gsi->c_port.notify_req->length = info->notify_buf_len;
		gsi->c_port.notify_req->context = gsi;
		gsi->c_port.notify_req->complete =
				gsi_ctrl_notify_resp_complete;
		event = gsi->c_port.notify_req->buf;
		event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
				| USB_RECIP_INTERFACE;

		if (gsi->ctrl_id == -ENODEV)
			event->wIndex = cpu_to_le16(gsi->data_id);
		else
			event->wIndex = cpu_to_le16(gsi->ctrl_id);

		event->wLength = cpu_to_le16(0);
		gsi->c_port.notify_state = GSI_CTRL_NOTIFY_NONE;
	}

	gsi->d_port.in_request.buf_len = info->in_req_buf_len;
	gsi->d_port.in_request.num_bufs = info->in_req_num_buf;
	len = gsi->d_port.in_request.buf_len * gsi->d_port.in_request.num_bufs;
	dev_dbg(&cdev->gadget->dev, "%zu %zu\n", gsi->d_port.in_request.buf_len,
			gsi->d_port.in_request.num_bufs);
	gsi->d_port.in_request.buf_base_addr =
		dma_zalloc_coherent(&cdev->gadget->dev, len,
				&gsi->d_port.in_request.dma, GFP_KERNEL);
	if (!gsi->d_port.in_request.buf_base_addr) {
		dev_err(&cdev->gadget->dev,
				"failed to allocate buf_base_addr for %s\n",
				gsi->function.name);
		goto fail;
	}

	if (gsi->d_port.out_ep) {
		gsi->d_port.out_request.buf_len = info->out_req_buf_len;
		gsi->d_port.out_request.num_bufs = info->out_req_num_buf;
		len =
		gsi->d_port.out_request.buf_len *
			gsi->d_port.out_request.num_bufs;
		dev_dbg(&cdev->gadget->dev, "%zu %zu\n",
				gsi->d_port.out_request.buf_len,
				gsi->d_port.out_request.num_bufs);
		gsi->d_port.out_request.buf_base_addr =
			dma_zalloc_coherent(&cdev->gadget->dev, len,
				&gsi->d_port.out_request.dma, GFP_KERNEL);
		if (!gsi->d_port.out_request.buf_base_addr) {
			dev_err(&cdev->gadget->dev,
				"failed to allocate buf_base_addr for %s\n",
					gsi->function.name);
			goto fail;
		}
	}

	/* Initialize event queue */
	spin_lock_init(&gsi->d_port.evt_q.q_lock);
	gsi->d_port.evt_q.head = gsi->d_port.evt_q.tail = MAXQUEUELEN - 1;

	/* copy descriptors, and track endpoint copies */
	f->fs_descriptors = usb_copy_descriptors(info->fs_desc_hdr);
	if (!gsi->function.fs_descriptors)
		goto fail;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(cdev->gadget)) {
		if (info->fs_in_desc)
			info->hs_in_desc->bEndpointAddress =
					info->fs_in_desc->bEndpointAddress;
		if (info->fs_out_desc)
			info->hs_out_desc->bEndpointAddress =
					info->fs_out_desc->bEndpointAddress;
		if (info->fs_notify_desc)
			info->hs_notify_desc->bEndpointAddress =
					info->fs_notify_desc->bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(info->hs_desc_hdr);
		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(cdev->gadget)) {
		if (info->fs_in_desc)
			info->ss_in_desc->bEndpointAddress =
					info->fs_in_desc->bEndpointAddress;

		if (info->fs_out_desc)
			info->ss_out_desc->bEndpointAddress =
					info->fs_out_desc->bEndpointAddress;
		if (info->fs_notify_desc)
			info->ss_notify_desc->bEndpointAddress =
					info->fs_notify_desc->bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(info->ss_desc_hdr);
		if (!f->ss_descriptors)
			goto fail;
	}

	return 0;

fail:
	if (gadget_is_superspeed(cdev->gadget) && f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(cdev->gadget) && f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->fs_descriptors)
		usb_free_descriptors(f->fs_descriptors);
	if (gsi->c_port.notify_req) {
		kfree(gsi->c_port.notify_req->buf);
		usb_ep_free_request(gsi->c_port.notify, gsi->c_port.notify_req);
	}
	/* we might as well release our claims on endpoints */
	if (gsi->c_port.notify)
		gsi->c_port.notify->driver_data = NULL;
	if (gsi->d_port.out_ep && gsi->d_port.out_ep->desc)
		gsi->d_port.out_ep->driver_data = NULL;
	if (gsi->d_port.in_ep && gsi->d_port.in_ep->desc)
		gsi->d_port.in_ep->driver_data = NULL;
	if (len && gsi->d_port.in_request.buf_base_addr)
		dma_free_coherent(&cdev->gadget->dev, len,
			gsi->d_port.in_request.buf_base_addr,
			gsi->d_port.in_request.dma);
	if (len && gsi->d_port.out_request.buf_base_addr)
		dma_free_coherent(&cdev->gadget->dev, len,
			gsi->d_port.out_request.buf_base_addr,
			gsi->d_port.out_request.dma);
	pr_err("%s: bind failed for %s\n", __func__, f->name);
	return -ENOMEM;
}

static int gsi_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct gsi_function_bind_info info = {0};
	struct f_gsi *gsi = func_to_gsi(f);
	int status;

	if (gsi->prot_id == IPA_USB_RMNET ||
		gsi->prot_id == IPA_USB_DIAG)
		gsi->ctrl_id = -ENODEV;
	else {
		status = gsi->ctrl_id = usb_interface_id(c, f);
		if (status < 0)
			goto fail;
	}

	status = gsi->data_id = usb_interface_id(c, f);
	if (status < 0)
		goto fail;

	switch (gsi->prot_id) {
	case IPA_USB_RNDIS:
		info.string_defs = rndis_gsi_string_defs;
		info.ctrl_desc = &rndis_gsi_control_intf;
		info.ctrl_str_idx = 0;
		info.data_desc = &rndis_gsi_data_intf;
		info.data_str_idx = 1;
		info.iad_desc = &rndis_gsi_iad_descriptor;
		info.iad_str_idx = 2;
		info.union_desc = &rndis_gsi_union_desc;
		info.fs_in_desc = &rndis_gsi_fs_in_desc;
		info.fs_out_desc = &rndis_gsi_fs_out_desc;
		info.fs_notify_desc = &rndis_gsi_fs_notify_desc;
		info.hs_in_desc = &rndis_gsi_hs_in_desc;
		info.hs_out_desc = &rndis_gsi_hs_out_desc;
		info.hs_notify_desc = &rndis_gsi_hs_notify_desc;
		info.ss_in_desc = &rndis_gsi_ss_in_desc;
		info.ss_out_desc = &rndis_gsi_ss_out_desc;
		info.ss_notify_desc = &rndis_gsi_ss_notify_desc;
		info.fs_desc_hdr = gsi_eth_fs_function;
		info.hs_desc_hdr = gsi_eth_hs_function;
		info.ss_desc_hdr = gsi_eth_ss_function;
		info.in_req_buf_len = dl_aggr_size;
		info.in_req_num_buf = num_in_bufs;
		info.out_req_buf_len = ul_aggr_size;
		info.out_req_num_buf = num_out_bufs;
		info.notify_buf_len = sizeof(struct usb_cdc_notification);

		status = rndis_register(gsi_rndis_response_available, gsi,
				gsi_rndis_flow_ctrl_enable);
		if (status < 0)
			goto fail;

		gsi->config = status;

		rndis_set_param_medium(gsi->config, RNDIS_MEDIUM_802_3, 0);

		/* export host's Ethernet address in CDC format */
		random_ether_addr(gsi->d_port.ipa_init_params.device_ethaddr);
		random_ether_addr(gsi->d_port.ipa_init_params.host_ethaddr);
		pr_debug("setting host_ethaddr=%pM, device_ethaddr=%pM\n",
		gsi->d_port.ipa_init_params.host_ethaddr,
		gsi->d_port.ipa_init_params.device_ethaddr);
		memcpy(gsi->ethaddr, &gsi->d_port.ipa_init_params.host_ethaddr,
				ETH_ALEN);
		rndis_set_host_mac(gsi->config, gsi->ethaddr);

		if (gsi->manufacturer && gsi->vendorID &&
			rndis_set_param_vendor(gsi->config, gsi->vendorID,
				gsi->manufacturer))
			goto dereg_rndis;

		pr_debug("%s(): max_pkt_per_xfer:%d\n", __func__,
					DEFAULT_MAX_PKT_PER_XFER);
		rndis_set_max_pkt_xfer(gsi->config, DEFAULT_MAX_PKT_PER_XFER);

		/* In case of aggregated packets QC device will request
		 * aliment to 4 (2^2).
		 */
		pr_debug("%s(): pkt_alignment_factor:%d\n", __func__,
					DEFAULT_PKT_ALIGNMENT_FACTOR);
		rndis_set_pkt_alignment_factor(gsi->config,
					DEFAULT_PKT_ALIGNMENT_FACTOR);
		break;
	case IPA_USB_MBIM:
		info.string_defs = mbim_gsi_string_defs;
		info.ctrl_desc = &mbim_gsi_control_intf;
		info.ctrl_str_idx = 0;
		info.data_desc = &mbim_gsi_data_intf;
		info.data_str_idx = 1;
		info.data_nop_desc = &mbim_gsi_data_nop_intf;
		info.iad_desc = &mbim_gsi_iad_desc;
		info.iad_str_idx = -1;
		info.union_desc = &mbim_gsi_union_desc;
		info.fs_in_desc = &mbim_gsi_fs_in_desc;
		info.fs_out_desc = &mbim_gsi_fs_out_desc;
		info.fs_notify_desc = &mbim_gsi_fs_notify_desc;
		info.hs_in_desc = &mbim_gsi_hs_in_desc;
		info.hs_out_desc = &mbim_gsi_hs_out_desc;
		info.hs_notify_desc = &mbim_gsi_hs_notify_desc;
		info.ss_in_desc = &mbim_gsi_ss_in_desc;
		info.ss_out_desc = &mbim_gsi_ss_out_desc;
		info.ss_notify_desc = &mbim_gsi_ss_notify_desc;
		info.fs_desc_hdr = mbim_gsi_fs_function;
		info.hs_desc_hdr = mbim_gsi_hs_function;
		info.ss_desc_hdr = mbim_gsi_ss_function;
		info.in_req_buf_len = 0x4000;
		info.in_req_num_buf = num_in_bufs;
		info.out_req_buf_len = 0x4000;
		info.out_req_num_buf = num_out_bufs;
		info.notify_buf_len = sizeof(struct usb_cdc_notification);
		mbim_gsi_desc.wMaxSegmentSize = cpu_to_le16(0x800);

		/*
		 * If MBIM is bound in a config other than the first, tell
		 * Windows about it by returning the num as a string in the
		 * OS descriptor's subCompatibleID field. Windows only supports
		 * up to config #4.
		 */
		if (c->bConfigurationValue >= 2 &&
				c->bConfigurationValue <= 4) {
			pr_debug("MBIM in configuration %d\n",
					c->bConfigurationValue);
			mbim_gsi_ext_config_desc.function.subCompatibleID[0] =
				c->bConfigurationValue + '0';
		}
		break;
	case IPA_USB_RMNET:
		info.string_defs = rmnet_gsi_string_defs;
		info.data_desc = &rmnet_gsi_interface_desc;
		info.data_str_idx = 0;
		info.fs_in_desc = &rmnet_gsi_fs_in_desc;
		info.fs_out_desc = &rmnet_gsi_fs_out_desc;
		info.fs_notify_desc = &rmnet_gsi_fs_notify_desc;
		info.hs_in_desc = &rmnet_gsi_hs_in_desc;
		info.hs_out_desc = &rmnet_gsi_hs_out_desc;
		info.hs_notify_desc = &rmnet_gsi_hs_notify_desc;
		info.ss_in_desc = &rmnet_gsi_ss_in_desc;
		info.ss_out_desc = &rmnet_gsi_ss_out_desc;
		info.ss_notify_desc = &rmnet_gsi_ss_notify_desc;
		info.fs_desc_hdr = rmnet_gsi_fs_function;
		info.hs_desc_hdr = rmnet_gsi_hs_function;
		info.ss_desc_hdr = rmnet_gsi_ss_function;
		info.in_req_buf_len = 16384;
		info.in_req_num_buf = num_in_bufs;
		info.out_req_buf_len = 16384;
		info.out_req_num_buf = num_out_bufs;
		info.notify_buf_len = sizeof(struct usb_cdc_notification);
		break;
	case IPA_USB_ECM:
		info.string_defs = ecm_gsi_string_defs;
		info.ctrl_desc = &ecm_gsi_control_intf;
		info.ctrl_str_idx = 0;
		info.data_desc = &ecm_gsi_data_intf;
		info.data_str_idx = 2;
		info.data_nop_desc = &ecm_gsi_data_nop_intf;
		info.cdc_eth_desc = &ecm_gsi_desc;
		info.mac_str_idx = 1;
		info.union_desc = &ecm_gsi_union_desc;
		info.fs_in_desc = &ecm_gsi_fs_in_desc;
		info.fs_out_desc = &ecm_gsi_fs_out_desc;
		info.fs_notify_desc = &ecm_gsi_fs_notify_desc;
		info.hs_in_desc = &ecm_gsi_hs_in_desc;
		info.hs_out_desc = &ecm_gsi_hs_out_desc;
		info.hs_notify_desc = &ecm_gsi_hs_notify_desc;
		info.ss_in_desc = &ecm_gsi_ss_in_desc;
		info.ss_out_desc = &ecm_gsi_ss_out_desc;
		info.ss_notify_desc = &ecm_gsi_ss_notify_desc;
		info.fs_desc_hdr = ecm_gsi_fs_function;
		info.hs_desc_hdr = ecm_gsi_hs_function;
		info.ss_desc_hdr = ecm_gsi_ss_function;
		info.in_req_buf_len = 2048;
		info.in_req_num_buf = num_in_bufs;
		info.out_req_buf_len = 2048;
		info.out_req_num_buf = num_out_bufs;
		info.notify_buf_len = GSI_CTRL_NOTIFY_BUFF_LEN;

		/* export host's Ethernet address in CDC format */
		random_ether_addr(gsi->d_port.ipa_init_params.device_ethaddr);
		random_ether_addr(gsi->d_port.ipa_init_params.host_ethaddr);
		pr_debug("setting host_ethaddr=%pM, device_ethaddr=%pM\n",
		gsi->d_port.ipa_init_params.host_ethaddr,
		gsi->d_port.ipa_init_params.device_ethaddr);

		snprintf(gsi->ethaddr, sizeof(gsi->ethaddr),
		"%02X%02X%02X%02X%02X%02X",
		gsi->d_port.ipa_init_params.host_ethaddr[0],
		gsi->d_port.ipa_init_params.host_ethaddr[1],
		gsi->d_port.ipa_init_params.host_ethaddr[2],
		gsi->d_port.ipa_init_params.host_ethaddr[3],
		gsi->d_port.ipa_init_params.host_ethaddr[4],
		gsi->d_port.ipa_init_params.host_ethaddr[5]);
		info.string_defs[1].s = gsi->ethaddr;
		break;
	case IPA_USB_DIAG:
		info.string_defs = qdss_gsi_string_defs;
		info.data_desc = &qdss_gsi_data_intf_desc;
		info.data_str_idx = 0;
		info.fs_in_desc = &qdss_gsi_hs_data_desc;
		info.hs_in_desc = &qdss_gsi_hs_data_desc;
		info.ss_in_desc = &qdss_gsi_ss_data_desc;
		info.fs_desc_hdr = qdss_gsi_hs_data_only_desc;
		info.hs_desc_hdr = qdss_gsi_hs_data_only_desc;
		info.ss_desc_hdr = qdss_gsi_ss_data_only_desc;
		info.in_req_buf_len = 16384;
		info.in_req_num_buf = num_in_bufs;
		info.notify_buf_len = sizeof(struct usb_cdc_notification);
		break;
	default:
		pr_err("%s: Invalid prot id %d\n", __func__, gsi->prot_id);
		return -EINVAL;
	}

	status = gsi_update_function_bind_params(gsi, cdev, &info);

	post_event(&gsi->d_port, EVT_INITIALIZED);
	queue_work(gsi->d_port.ipa_usb_wq, &gsi->d_port.usb_ipa_w);

	DBG(cdev, "%s: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			f->name,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			gsi->d_port.in_ep->name, gsi->d_port.out_ep->name,
			gsi->c_port.notify->name);
	return 0;

dereg_rndis:
	rndis_deregister(gsi->config);
fail:
	return status;
}

static void gsi_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_gsi *gsi = func_to_gsi(f);
	struct usb_composite_dev *cdev = c->cdev;
	u32 len;

	/*
	 * call flush_workqueue to make sure that any pending
	 * disconnect_work() is being flushed before calling
	 * ipa_usb_deinit_teth_prot ipa
	 */
	flush_workqueue(gsi->d_port.ipa_usb_wq);
	ipa_usb_deinit_teth_prot(gsi->prot_id);
	gadget_restarted = false;

	if (gsi->prot_id == IPA_USB_RNDIS) {
		gsi->d_port.sm_state = STATE_UNINITIALIZED;
		rndis_deregister(gsi->config);
	}

	if (gsi->prot_id == IPA_USB_MBIM)
		mbim_gsi_ext_config_desc.function.subCompatibleID[0] = 0;

	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->fs_descriptors);

	if (gsi->c_port.notify) {
		kfree(gsi->c_port.notify_req->buf);
		usb_ep_free_request(gsi->c_port.notify, gsi->c_port.notify_req);

		len =
		gsi->d_port.out_request.buf_len *
			gsi->d_port.out_request.num_bufs;
		dma_free_coherent(&cdev->gadget->dev, len,
			gsi->d_port.out_request.buf_base_addr,
			gsi->d_port.out_request.dma);
	}

	len = gsi->d_port.in_request.buf_len * gsi->d_port.in_request.num_bufs;
	dma_free_coherent(&cdev->gadget->dev, len,
		gsi->d_port.in_request.buf_base_addr,
		gsi->d_port.in_request.dma);
}

static void ipa_ready_callback(void *user_data)
{
	struct f_gsi *gsi = user_data;

	pr_info("%s: ipa is ready\n", __func__);

	gsi->d_port.ipa_ready = true;
	wake_up_interruptible(&gsi->d_port.wait_for_ipa_ready);
}

int gsi_bind_config(struct usb_configuration *c, enum ipa_usb_teth_prot prot_id)
{
	struct f_gsi	*gsi;
	int status = 0;

	pr_debug("%s: prot id %d\n", __func__, prot_id);

	if (prot_id >= IPA_USB_MAX_TETH_PROT_SIZE) {
		pr_err("%s: invalid prot id %d\n", __func__, prot_id);
		return -EINVAL;
	}

	gsi = gsi_prot_ctx[prot_id];

	if (!gsi) {
		pr_err("%s: gsi prot ctx is %p\n", __func__, gsi);
		return -EINVAL;
	}

	if (!gadget_restarted) {
		usb_gadget_restart(c->cdev->gadget);
		gadget_restarted = true;
	}

	switch (prot_id) {
	case IPA_USB_RNDIS:
		gsi->function.name = "rndis";
		gsi->function.strings = rndis_gsi_strings;
		break;
	case IPA_USB_ECM:
		gsi->function.name = "cdc_ethernet";
		gsi->function.strings = ecm_gsi_strings;
		break;
	case IPA_USB_RMNET:
		gsi->function.name = "rmnet";
		gsi->function.strings = rmnet_gsi_strings;
		break;
	case IPA_USB_MBIM:
		gsi->function.name = "mbim";
		gsi->function.strings = mbim_gsi_strings;
		break;
	case IPA_USB_DIAG:
		gsi->function.name = "dpl";
		gsi->function.strings = qdss_gsi_strings;
		break;
	default:
		pr_err("%s: invalid prot id %d\n", __func__, prot_id);
		return -EINVAL;
	}

	/* descriptors are per-instance copies */
	gsi->function.bind = gsi_bind;
	gsi->function.unbind = gsi_unbind;
	gsi->function.set_alt = gsi_set_alt;
	gsi->function.get_alt = gsi_get_alt;
	gsi->function.setup = gsi_setup;
	gsi->function.disable = gsi_disable;
	gsi->function.suspend = gsi_suspend;
	gsi->function.func_suspend = gsi_func_suspend;
	gsi->function.resume = gsi_resume;

	INIT_WORK(&gsi->d_port.usb_ipa_w, ipa_work_handler);

	status = usb_add_function(c, &gsi->function);
	if (status)
		return status;

	status = ipa_register_ipa_ready_cb(ipa_ready_callback, gsi);
	if (!status) {
		pr_info("%s: ipa is not ready\n", __func__);
		status = wait_event_interruptible_timeout(
			gsi->d_port.wait_for_ipa_ready, gsi->d_port.ipa_ready,
			msecs_to_jiffies(GSI_IPA_READY_TIMEOUT));
		if (!status) {
			pr_err("%s: ipa ready timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	gsi->d_port.ipa_usb_notify_cb = ipa_usb_notify_cb;
	status = ipa_usb_init_teth_prot(prot_id,
		&gsi->d_port.ipa_init_params, gsi->d_port.ipa_usb_notify_cb,
		gsi);
	if (status) {
		pr_err("%s: failed to init teth prot %d\n", __func__, prot_id);
		return status;
	}

	return status;
}

static int gsi_function_init(enum ipa_usb_teth_prot prot_id)
{
	struct f_gsi *gsi;
	int ret = 0;

	if (prot_id >= IPA_USB_MAX_TETH_PROT_SIZE) {
		pr_err("%s: invalid prto id %d\n", __func__, prot_id);
		ret = -EINVAL;
		goto error;
	}

	gsi = kzalloc(sizeof(*gsi), GFP_KERNEL);
	if (!gsi) {
		ret = -ENOMEM;
		goto error;
	}

	spin_lock_init(&gsi->d_port.lock);

	init_waitqueue_head(&gsi->d_port.wait_for_ipa_ready);

	gsi->d_port.in_channel_handle = -EINVAL;
	gsi->d_port.out_channel_handle = -EINVAL;

	gsi->prot_id = prot_id;

	gsi_prot_ctx[prot_id] = gsi;

	gsi->d_port.ipa_usb_wq = ipa_usb_wq;

	ret = gsi_function_ctrl_port_init(prot_id);
	if (ret) {
		kfree(gsi);
		gsi_prot_ctx[prot_id] = NULL;
	}

error:
	return ret;
}

static void gsi_function_cleanup(enum ipa_usb_teth_prot prot_id)
{
	struct f_gsi *gsi = gsi_prot_ctx[prot_id];

	if (prot_id >= IPA_USB_MAX_TETH_PROT_SIZE) {
		pr_err("%s: invalid prto id %d\n", __func__, prot_id);
		return;
	}

	if (gsi->c_port.ctrl_device.fops) {
		misc_deregister(&gsi->c_port.ctrl_device);
		gsi->c_port.ctrl_device.fops = NULL;
	}

	kfree(gsi_prot_ctx[prot_id]);
	gsi_prot_ctx[prot_id] = NULL;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GSI function driver");

static int fgsi_init(void)
{
	ipa_usb_wq = alloc_workqueue("k_ipa_usb",
				WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!ipa_usb_wq) {
		pr_err("Failed to create workqueue for IPA\n");
		return -ENOMEM;
	}
	usb_gsi_debugfs_init();
	return 0;
}
module_init(fgsi_init);

static void __exit fgsi_exit(void)
{
	if (ipa_usb_wq)
		destroy_workqueue(ipa_usb_wq);
	usb_gsi_debugfs_exit();
}
module_exit(fgsi_exit);
