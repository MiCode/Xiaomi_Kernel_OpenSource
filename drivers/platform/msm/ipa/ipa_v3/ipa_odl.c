/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#include "ipa_i.h"
#include "ipa_odl.h"
#include <linux/msm_ipa.h>
#include <linux/sched/signal.h>
#include <linux/poll.h>

struct ipa_odl_context *ipa3_odl_ctx;

static DECLARE_WAIT_QUEUE_HEAD(odl_ctl_msg_wq);

static void print_ipa_odl_state_bit_mask(void)
{
	IPADBG("ipa3_odl_ctx->odl_state.odl_init --> %d\n",
		ipa3_odl_ctx->odl_state.odl_init);
	IPADBG("ipa3_odl_ctx->odl_state.odl_open --> %d\n",
		ipa3_odl_ctx->odl_state.odl_open);
	IPADBG("ipa3_odl_ctx->odl_state.adpl_open --> %d\n",
		ipa3_odl_ctx->odl_state.adpl_open);
	IPADBG("ipa3_odl_ctx->odl_state.aggr_byte_limit_sent --> %d\n",
		ipa3_odl_ctx->odl_state.aggr_byte_limit_sent);
	IPADBG("ipa3_odl_ctx->odl_state.odl_ep_setup --> %d\n",
		ipa3_odl_ctx->odl_state.odl_ep_setup);
	IPADBG("ipa3_odl_ctx->odl_state.odl_setup_done_sent --> %d\n",
		ipa3_odl_ctx->odl_state.odl_setup_done_sent);
	IPADBG("ipa3_odl_ctx->odl_state.odl_ep_info_sent --> %d\n",
		ipa3_odl_ctx->odl_state.odl_ep_info_sent);
	IPADBG("ipa3_odl_ctx->odl_state.odl_connected --> %d\n",
		ipa3_odl_ctx->odl_state.odl_connected);
	IPADBG("ipa3_odl_ctx->odl_state.odl_disconnected --> %d\n\n",
		ipa3_odl_ctx->odl_state.odl_disconnected);
}

static int ipa_odl_ctl_fops_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	if (ipa3_odl_ctx->odl_state.odl_init) {
		ipa3_odl_ctx->odl_state.odl_open = true;
	} else {
		IPAERR("Before odl init trying to open odl ctl pipe\n");
		print_ipa_odl_state_bit_mask();
		ret = -ENODEV;
	}

	return ret;
}

static int ipa_odl_ctl_fops_release(struct inode *inode, struct file *filp)
{
	IPADBG("QTI closed ipa_odl_ctl node\n");
	ipa3_odl_ctx->odl_state.odl_open = false;
	return 0;
}

/**
 * ipa_odl_ctl_fops_read() - read message from IPA ODL device
 * @filp:	[in] file pointer
 * @buf:	[out] buffer to read into
 * @count:	[in] size of above buffer
 * @f_pos:	[inout] file position
 *
 * Uer-space should continuously read from /dev/ipa_odl_ctl,
 * read will block when there are no messages to read.
 * Upon return, user-space should read the u32 data from the
 * start of the buffer.
 *
 * 0 --> ODL disconnected.
 * 1 --> ODL connected.
 *
 * Buffer supplied must be big enough to
 * hold the message of size u32.
 *
 * Returns: how many bytes copied to buffer
 *
 * Note: Should not be called from atomic context
 */

static ssize_t ipa_odl_ctl_fops_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos)
{
	char __user *start;
	u8 data;
	int ret = 0;
	static bool old_state;
	bool new_state = false;

	start = buf;
	ipa3_odl_ctx->odl_ctl_msg_wq_flag = false;

	if (!ipa3_odl_ctx->odl_state.adpl_open &&
			!ipa3_odl_ctx->odl_state.odl_disconnected) {
		IPADBG("Failed to send data odl pipe already disconnected\n");
		ret = -EFAULT;
		goto send_failed;
	}

	if (ipa3_odl_ctx->odl_state.odl_ep_setup)
		new_state = true;
	else if (ipa3_odl_ctx->odl_state.odl_disconnected)
		new_state = false;
	else {
		IPADBG("Failed to send data odl already running\n");
		ret = -EFAULT;
		goto send_failed;
	}

	if (old_state != new_state) {
		old_state = new_state;

		if (new_state == true)
			data = 1;
		else if (new_state == false)
			data = 0;

		if (copy_to_user(buf, &data,
					sizeof(data))) {
			IPADBG("Cpoying data to user failed\n");
			ret = -EFAULT;
			goto send_failed;
		}

		buf += sizeof(data);

		if (data == 1)
			ipa3_odl_ctx->odl_state.odl_setup_done_sent =
				true;
	}


	if (start != buf && ret != -EFAULT)
		ret = buf - start;
send_failed:
	return ret;
}

static unsigned int ipa_odl_ctl_fops_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &odl_ctl_msg_wq, wait);

	if (ipa3_odl_ctx->odl_ctl_msg_wq_flag == true) {
		IPADBG("Sending read mask to odl control pipe\n");
		mask |= POLLIN | POLLRDNORM;
	}
	return mask;
}

static long ipa_odl_ctl_fops_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct ipa_odl_ep_info ep_info = {0};
	struct ipa_odl_modem_config status;
	int retval = 0;

	IPADBG("Calling odl ioctl cmd = %d\n", cmd);
	if (!ipa3_odl_ctx->odl_state.odl_setup_done_sent) {
		IPAERR("Before complete the odl setup trying calling ioctl\n");
		print_ipa_odl_state_bit_mask();
		retval = -ENODEV;
		goto fail;
	}

	switch (cmd) {
	case IPA_IOC_ODL_QUERY_ADAPL_EP_INFO:
		/* Send ep_info to user APP */
		ep_info.ep_type = ODL_EP_TYPE_HSUSB;
		ep_info.peripheral_iface_id = ODL_EP_PERIPHERAL_IFACE_ID;
		ep_info.cons_pipe_num = -1;
		ep_info.prod_pipe_num =
			ipa3_odl_ctx->odl_client_hdl;
		if (copy_to_user((void __user *)arg, &ep_info,
					sizeof(ep_info))) {
			retval = -EFAULT;
			goto fail;
		}
		ipa3_odl_ctx->odl_state.odl_ep_info_sent = true;
		break;
	case IPA_IOC_ODL_QUERY_MODEM_CONFIG:
		IPADBG("Received the IPA_IOC_ODL_QUERY_MODEM_CONFIG :\n");
		if (copy_from_user(&status, (const void __user *)arg,
			sizeof(status))) {
			retval = -EFAULT;
			break;
		}
		if (status.config_status == CONFIG_SUCCESS)
			ipa3_odl_ctx->odl_state.odl_connected = true;
		IPADBG("status.config_status = %d odl_connected = %d\n",
		status.config_status, ipa3_odl_ctx->odl_state.odl_connected);
		break;
	default:
		retval = -ENOIOCTLCMD;
		break;
	}

fail:
	return retval;
}

static void delete_first_node(void)
{
	struct ipa3_push_msg_odl *msg;

	if (!list_empty(&ipa3_odl_ctx->adpl_msg_list)) {
		msg = list_first_entry(&ipa3_odl_ctx->adpl_msg_list,
				struct ipa3_push_msg_odl, link);
		if (msg) {
			list_del(&msg->link);
			kfree(msg->buff);
			kfree(msg);
			ipa3_odl_ctx->stats.odl_drop_pkt++;
			if (atomic_read(&ipa3_odl_ctx->stats.numer_in_queue))
				atomic_dec(&ipa3_odl_ctx->stats.numer_in_queue);
		}
	} else {
		IPADBG("List Empty\n");
	}
}

int ipa3_send_adpl_msg(unsigned long skb_data)
{
	struct ipa3_push_msg_odl *msg;
	struct sk_buff *skb = (struct sk_buff *)skb_data;
	void *data;

	IPADBG_LOW("Processing DPL data\n");
	msg = kzalloc(sizeof(struct ipa3_push_msg_odl), GFP_KERNEL);
	if (msg == NULL) {
		IPADBG("Memory allocation failed\n");
		return -ENOMEM;
	}

	data = kmalloc(skb->len, GFP_KERNEL);
	if (data == NULL) {
		kfree(msg);
		return -ENOMEM;
	}
	memcpy(data, skb->data, skb->len);
	msg->buff = data;
	msg->len = skb->len;
	mutex_lock(&ipa3_odl_ctx->adpl_msg_lock);
	if (atomic_read(&ipa3_odl_ctx->stats.numer_in_queue) >=
						MAX_QUEUE_TO_ODL)
		delete_first_node();
	list_add_tail(&msg->link, &ipa3_odl_ctx->adpl_msg_list);
	atomic_inc(&ipa3_odl_ctx->stats.numer_in_queue);
	mutex_unlock(&ipa3_odl_ctx->adpl_msg_lock);
	IPA_STATS_INC_CNT(ipa3_odl_ctx->stats.odl_rx_pkt);

	return 0;
}

/**
 * odl_ipa_packet_receive_notify() - Rx notify
 *
 * @priv: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * IPA will pass a packet to the Linux network stack with skb->data
 */
static void odl_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	IPADBG_LOW("Rx packet was received\n");
	if (evt == IPA_RECEIVE)
		ipa3_send_adpl_msg(data);
	else
		IPAERR("Invalid evt %d received in wan_ipa_receive\n", evt);
}

int ipa_setup_odl_pipe(void)
{
	struct ipa_sys_connect_params *ipa_odl_ep_cfg;
	int ret;

	ipa_odl_ep_cfg = &ipa3_odl_ctx->odl_sys_param;

	IPADBG("Setting up the odl endpoint\n");
	ipa_odl_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en = IPA_ENABLE_CS_OFFLOAD_DL;

	ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_en = IPA_ENABLE_AGGR;
	ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_hard_byte_limit_en = 1;
	if (ipa3_is_mhip_offload_enabled()) {
		IPADBG("MHIP is enabled, disable aggregation for ODL pipe");
		ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	}
	ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr = IPA_GENERIC;
	ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_byte_limit =
						IPA_ODL_AGGR_BYTE_LIMIT;
	ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_pkt_limit = 0;

	ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 4;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata_valid = 1;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_metadata = 1;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size_valid = 1;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_ofst_pkt_size = 2;

	ipa_odl_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = 0;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = true;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
	ipa_odl_ep_cfg->ipa_ep_cfg.hdr_ext.hdr_little_endian = 0;
	ipa_odl_ep_cfg->ipa_ep_cfg.metadata_mask.metadata_mask = 0xFF000000;

	ipa_odl_ep_cfg->client = IPA_CLIENT_ODL_DPL_CONS;
	ipa_odl_ep_cfg->notify = odl_ipa_packet_receive_notify;

	ipa_odl_ep_cfg->napi_obj = NULL;
	ipa_odl_ep_cfg->desc_fifo_sz = IPA_ODL_RX_RING_SIZE *
						IPA_FIFO_ELEMENT_SIZE;
	ipa3_odl_ctx->odl_client_hdl = -1;

	/* For MHIP, ODL functionality is DMA. So bypass aggregation, checksum
	 * offload, hdr_len.
	 */
	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ &&
		ipa3_is_mhip_offload_enabled()) {
		IPADBG("MHIP enabled: bypass aggr + csum offload for ODL");
		ipa_odl_ep_cfg->ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
		ipa_odl_ep_cfg->ipa_ep_cfg.cfg.cs_offload_en =
			IPA_DISABLE_CS_OFFLOAD;
		ipa_odl_ep_cfg->ipa_ep_cfg.hdr.hdr_len = 0;
	}

	ret = ipa3_setup_sys_pipe(ipa_odl_ep_cfg,
			&ipa3_odl_ctx->odl_client_hdl);
	return ret;

}

/**
 * ipa3_odl_register_pm - Register odl client for PM
 *
 * This function will register 1 client with IPA PM to represent odl
 * in clock scaling calculation:
 *	- "ODL" - this client will be activated when pipe connected
 */
static int ipa3_odl_register_pm(void)
{
	int result = 0;
	struct ipa_pm_register_params pm_reg;

	memset(&pm_reg, 0, sizeof(pm_reg));
	pm_reg.name = "ODL";
	pm_reg.group = IPA_PM_GROUP_DEFAULT;
	pm_reg.skip_clk_vote = true;
	result = ipa_pm_register(&pm_reg, &ipa3_odl_ctx->odl_pm_hdl);
	if (result) {
		IPAERR("failed to create IPA PM client %d\n", result);
		return result;
	}
	return result;
}

int ipa3_odl_pipe_open(void)
{
	int ret = 0;
	struct ipa_ep_cfg_holb holb_cfg;

	if (!ipa3_odl_ctx->odl_state.adpl_open) {
		IPAERR("adpl pipe not configured\n");
		return 0;
	}

	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.tmr_val = 0;
	holb_cfg.en = 1;

	ipa3_cfg_ep_holb_by_client(IPA_CLIENT_USB_DPL_CONS, &holb_cfg);
	ret = ipa_setup_odl_pipe();
	if (ret) {
		IPAERR(" Setup endpoint config failed\n");
		ipa3_odl_ctx->odl_state.adpl_open = false;
		goto fail;
	}
	ipa3_cfg_ep_holb_by_client(IPA_CLIENT_ODL_DPL_CONS, &holb_cfg);
	ipa3_odl_ctx->odl_state.odl_ep_setup = true;
	IPADBG("Setup endpoint config success\n");

	ipa3_odl_ctx->stats.odl_drop_pkt = 0;
	atomic_set(&ipa3_odl_ctx->stats.numer_in_queue, 0);
	ipa3_odl_ctx->stats.odl_rx_pkt = 0;
	ipa3_odl_ctx->stats.odl_tx_diag_pkt = 0;
	/*
	 * Send signal to ipa_odl_ctl_fops_read,
	 * to send ODL ep open notification
	 */
	if (ipa3_is_mhip_offload_enabled()) {
		IPADBG("MHIP is enabled, continue\n");
		ipa3_odl_ctx->odl_state.odl_open = true;
		ipa3_odl_ctx->odl_state.odl_setup_done_sent = true;
		ipa3_odl_ctx->odl_state.odl_ep_info_sent = true;
		ipa3_odl_ctx->odl_state.odl_connected = true;
		ipa3_odl_ctx->odl_state.odl_disconnected = false;

		/* Enable ADPL over ODL for MPM */
		ret = ipa3_mpm_enable_adpl_over_odl(true);
		if (ret) {
			IPAERR("mpm failed to enable ADPL over ODL %d\n", ret);
			return ret;
		}
	} else {
		ipa3_odl_ctx->odl_ctl_msg_wq_flag = true;
		IPAERR("Wake up odl ctl\n");
		wake_up_interruptible(&odl_ctl_msg_wq);
		if (ipa3_odl_ctx->odl_state.odl_disconnected)
			ipa3_odl_ctx->odl_state.odl_disconnected = false;
	}
fail:
	return ret;

}
static int ipa_adpl_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	IPADBG("Called the function :\n");
	mutex_lock(&ipa3_odl_ctx->pipe_lock);
	if (ipa3_odl_ctx->odl_state.odl_init &&
				!ipa3_odl_ctx->odl_state.adpl_open) {
		/* Activate ipa_pm*/
		ret = ipa_pm_activate_sync(ipa3_odl_ctx->odl_pm_hdl);
		if (ret)
			IPAERR("failed to activate pm\n");
		ipa3_odl_ctx->odl_state.adpl_open = true;
		ret = ipa3_odl_pipe_open();
	} else {
		IPAERR("Before odl init trying to open adpl pipe\n");
		print_ipa_odl_state_bit_mask();
		ret = -ENODEV;
	}
	mutex_unlock(&ipa3_odl_ctx->pipe_lock);

	return ret;
}

static int ipa_adpl_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	/* Deactivate ipa_pm */
	mutex_lock(&ipa3_odl_ctx->pipe_lock);
	ret = ipa_pm_deactivate_sync(ipa3_odl_ctx->odl_pm_hdl);
	if (ret)
		IPAERR("failed to activate pm\n");
	ipa3_odl_pipe_cleanup(false);

	/* Disable ADPL over ODL for MPM */
	if (ipa3_is_mhip_offload_enabled()) {
		ret = ipa3_mpm_enable_adpl_over_odl(false);
		if (ret)
			IPAERR("mpm failed to disable ADPL over ODL\n");

	}
	mutex_unlock(&ipa3_odl_ctx->pipe_lock);

	return ret;
}

void ipa3_odl_pipe_cleanup(bool is_ssr)
{
	bool ipa_odl_opened = false;
	struct ipa_ep_cfg_holb holb_cfg;

	if (!ipa3_odl_ctx->odl_state.adpl_open) {
		IPAERR("adpl pipe not configured\n");
		return;
	}
	if (ipa3_odl_ctx->odl_state.odl_open)
		ipa_odl_opened = true;

	memset(&ipa3_odl_ctx->odl_state, 0, sizeof(ipa3_odl_ctx->odl_state));

	/*Since init will not be done again*/
	ipa3_odl_ctx->odl_state.odl_init = true;
	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.tmr_val = 0;
	holb_cfg.en = 0;

	ipa3_cfg_ep_holb_by_client(IPA_CLIENT_USB_DPL_CONS, &holb_cfg);

	ipa3_teardown_sys_pipe(ipa3_odl_ctx->odl_client_hdl);
	ipa3_odl_ctx->odl_client_hdl = -1;
	/*Assume QTI will never close this node once opened*/
	if (ipa_odl_opened)
		ipa3_odl_ctx->odl_state.odl_open = true;

	/*Assume DIAG will not close this node in SSR case*/
	if (is_ssr)
		ipa3_odl_ctx->odl_state.adpl_open = true;
	else
		ipa3_odl_ctx->odl_state.adpl_open = false;

	ipa3_odl_ctx->odl_state.odl_disconnected = true;
	ipa3_odl_ctx->odl_state.odl_ep_setup = false;
	ipa3_odl_ctx->odl_state.aggr_byte_limit_sent = false;
	ipa3_odl_ctx->odl_state.odl_connected = false;
	/*
	 * Send signal to ipa_odl_ctl_fops_read,
	 * to send ODL ep close notification
	 */
	ipa3_odl_ctx->odl_ctl_msg_wq_flag = true;
	ipa3_odl_ctx->stats.odl_drop_pkt = 0;
	atomic_set(&ipa3_odl_ctx->stats.numer_in_queue, 0);
	ipa3_odl_ctx->stats.odl_rx_pkt = 0;
	ipa3_odl_ctx->stats.odl_tx_diag_pkt = 0;
	IPADBG("Wake up odl ctl\n");
	wake_up_interruptible(&odl_ctl_msg_wq);

}

/**
 * ipa_adpl_read() - read message from IPA device
 * @filp:	[in] file pointer
 * @buf:	[out] buffer to read into
 * @count:	[in] size of above buffer
 * @f_pos:	[inout] file position
 *
 * User-space should continually read from /dev/ipa_adpl,
 * read will block when there are no messages to read.
 * Upon return, user-space should read
 * Buffer supplied must be big enough to
 * hold the data.
 *
 * Returns:	how many bytes copied to buffer
 *
 * Note:	Should not be called from atomic context
 */
static ssize_t ipa_adpl_read(struct file *filp, char __user *buf, size_t count,
		  loff_t *f_pos)
{
	int ret =  0;
	char __user *start = buf;
	struct ipa3_push_msg_odl *msg;

	while (1) {
		IPADBG_LOW("Writing message to adpl pipe\n");
		if (!ipa3_odl_ctx->odl_state.odl_open)
			break;

		mutex_lock(&ipa3_odl_ctx->adpl_msg_lock);
		msg = NULL;
		if (!list_empty(&ipa3_odl_ctx->adpl_msg_list)) {
			msg = list_first_entry(&ipa3_odl_ctx->adpl_msg_list,
					struct ipa3_push_msg_odl, link);
			list_del(&msg->link);
			if (atomic_read(&ipa3_odl_ctx->stats.numer_in_queue))
				atomic_dec(&ipa3_odl_ctx->stats.numer_in_queue);
		}

		mutex_unlock(&ipa3_odl_ctx->adpl_msg_lock);

		if (msg != NULL) {
			if (msg->len > count) {
				IPAERR("Message length greater than count\n");
				kfree(msg->buff);
				kfree(msg);
				msg = NULL;
				ret = -EAGAIN;
				break;
			}

			if (msg->buff) {
				if (copy_to_user(buf, msg->buff,
							msg->len)) {
					ret = -EFAULT;
					kfree(msg->buff);
					kfree(msg);
					msg = NULL;
					ret = -EAGAIN;
					break;
				}
				buf += msg->len;
				count -= msg->len;
				kfree(msg->buff);
			}
			IPA_STATS_INC_CNT(ipa3_odl_ctx->stats.odl_tx_diag_pkt);
			kfree(msg);
			msg = NULL;
		} else {
			ret = -EAGAIN;
			break;
		}

		ret = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;

		ret = -EINTR;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

	}

	if (start != buf && ret != -EFAULT)
		ret = buf - start;

	return ret;
}

static long ipa_adpl_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct odl_agg_pipe_info odl_pipe_info;
	int retval = 0;

	if (!ipa3_odl_ctx->odl_state.odl_connected) {
		IPAERR("ODL config in progress not allowed ioctl\n");
		print_ipa_odl_state_bit_mask();
		retval = -ENODEV;
		goto fail;
	}
	IPADBG("Calling adpl ioctl\n");

	switch (cmd) {
	case IPA_IOC_ODL_GET_AGG_BYTE_LIMIT:
		odl_pipe_info.agg_byte_limit =
		/*Modem expecting value in bytes. so passing 15 = 15*1024*/
		(ipa3_odl_ctx->odl_sys_param.ipa_ep_cfg.aggr.aggr_byte_limit *
			1024);
		if (copy_to_user((void __user *)arg, &odl_pipe_info,
					sizeof(odl_pipe_info))) {
			retval = -EFAULT;
			goto fail;
		}
		ipa3_odl_ctx->odl_state.aggr_byte_limit_sent = true;
		break;
	default:
		retval = -ENOIOCTLCMD;
		print_ipa_odl_state_bit_mask();
		break;
	}

fail:
	return retval;
}

static const struct file_operations ipa_odl_ctl_fops = {
	.owner = THIS_MODULE,
	.open = ipa_odl_ctl_fops_open,
	.release = ipa_odl_ctl_fops_release,
	.read = ipa_odl_ctl_fops_read,
	.unlocked_ioctl = ipa_odl_ctl_fops_ioctl,
	.poll = ipa_odl_ctl_fops_poll,
};

static const struct file_operations ipa_adpl_fops = {
	.owner = THIS_MODULE,
	.open = ipa_adpl_open,
	.release = ipa_adpl_release,
	.read = ipa_adpl_read,
	.unlocked_ioctl = ipa_adpl_ioctl,
};

int ipa_odl_init(void)
{
	int result = 0;
	struct cdev *cdev;
	int loop = 0;
	struct ipa3_odl_char_device_context *odl_cdev;

	ipa3_odl_ctx = kzalloc(sizeof(*ipa3_odl_ctx), GFP_KERNEL);
	if (!ipa3_odl_ctx) {
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	odl_cdev = ipa3_odl_ctx->odl_cdev;
	INIT_LIST_HEAD(&ipa3_odl_ctx->adpl_msg_list);
	mutex_init(&ipa3_odl_ctx->adpl_msg_lock);
	mutex_init(&ipa3_odl_ctx->pipe_lock);

	odl_cdev[loop].class = class_create(THIS_MODULE, "ipa_adpl");

	if (IS_ERR(odl_cdev[loop].class)) {
		IPAERR("Error: odl_cdev->class NULL\n");
		result = -ENODEV;
		goto create_char_dev0_fail;
	}

	result = alloc_chrdev_region(&odl_cdev[loop].dev_num, 0, 1, "ipa_adpl");
	if (result) {
		IPAERR("alloc_chrdev_region error for ipa adpl pipe\n");
		result = -ENODEV;
		goto alloc_chrdev0_region_fail;
	}

	odl_cdev[loop].dev = device_create(odl_cdev[loop].class, NULL,
		 odl_cdev[loop].dev_num, ipa3_ctx, "ipa_adpl");
	if (IS_ERR(odl_cdev[loop].dev)) {
		IPAERR("device_create err:%ld\n", PTR_ERR(odl_cdev[loop].dev));
		result = PTR_ERR(odl_cdev[loop].dev);
		goto device0_create_fail;
	}

	cdev = &odl_cdev[loop].cdev;
	cdev_init(cdev, &ipa_adpl_fops);
	cdev->owner = THIS_MODULE;
	cdev->ops = &ipa_adpl_fops;

	result = cdev_add(cdev, odl_cdev[loop].dev_num, 1);
	if (result) {
		IPAERR("cdev_add err=%d\n", -result);
		goto cdev0_add_fail;
	}

	loop++;

	odl_cdev[loop].class = class_create(THIS_MODULE, "ipa_odl_ctl");

	if (IS_ERR(odl_cdev[loop].class)) {
		IPAERR("Error: odl_cdev->class NULL\n");
		result =  -ENODEV;
		goto create_char_dev1_fail;
	}

	result = alloc_chrdev_region(&odl_cdev[loop].dev_num, 0, 1,
							"ipa_odl_ctl");
	if (result) {
		IPAERR("alloc_chrdev_region error for ipa odl ctl pipe\n");
		goto alloc_chrdev1_region_fail;
	}

	odl_cdev[loop].dev = device_create(odl_cdev[loop].class, NULL,
		 odl_cdev[loop].dev_num, ipa3_ctx, "ipa_odl_ctl");
	if (IS_ERR(odl_cdev[loop].dev)) {
		IPAERR("device_create err:%ld\n", PTR_ERR(odl_cdev[loop].dev));
		result = PTR_ERR(odl_cdev[loop].dev);
		goto device1_create_fail;
	}

	cdev = &odl_cdev[loop].cdev;
	cdev_init(cdev, &ipa_odl_ctl_fops);
	cdev->owner = THIS_MODULE;
	cdev->ops = &ipa_odl_ctl_fops;

	result = cdev_add(cdev, odl_cdev[loop].dev_num, 1);
	if (result) {
		IPAERR(":cdev_add err=%d\n", -result);
		goto cdev1_add_fail;
	}

	ipa3_odl_ctx->odl_state.odl_init = true;

	/* register ipa_pm */
	result = ipa3_odl_register_pm();
	if (result) {
		IPAWANERR("ipa3_odl_register_pm failed, ret: %d\n",
				result);
		goto cdev1_add_fail;
	}
	return 0;
cdev1_add_fail:
	device_destroy(odl_cdev[1].class, odl_cdev[1].dev_num);
device1_create_fail:
	unregister_chrdev_region(odl_cdev[1].dev_num, 1);
alloc_chrdev1_region_fail:
	class_destroy(odl_cdev[1].class);
create_char_dev1_fail:
cdev0_add_fail:
	device_destroy(odl_cdev[0].class, odl_cdev[0].dev_num);
device0_create_fail:
	unregister_chrdev_region(odl_cdev[0].dev_num, 1);
alloc_chrdev0_region_fail:
	class_destroy(odl_cdev[0].class);
create_char_dev0_fail:
	kfree(ipa3_odl_ctx);
fail_mem_ctx:
	return result;
}

bool ipa3_is_odl_connected(void)
{
	return ipa3_odl_ctx->odl_state.odl_connected;
}
