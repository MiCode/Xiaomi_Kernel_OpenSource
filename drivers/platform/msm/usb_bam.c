/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/msm_hsusb.h>
#include <mach/usb_bam.h>
#include <mach/sps.h>
#include <mach/ipa.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <mach/msm_smsm.h>

#define USB_SUMMING_THRESHOLD 512
#define CONNECTIONS_NUM	8

static struct sps_bam_props usb_props;
static struct sps_pipe *sps_pipes[CONNECTIONS_NUM][2];
static struct sps_connect sps_connections[CONNECTIONS_NUM][2];
static struct sps_mem_buffer data_mem_buf[CONNECTIONS_NUM][2];
static struct sps_mem_buffer desc_mem_buf[CONNECTIONS_NUM][2];
static struct platform_device *usb_bam_pdev;
static struct workqueue_struct *usb_bam_wq;
static u32 h_bam;
static spinlock_t usb_bam_lock;

struct usb_bam_event_info {
	struct sps_register_event event;
	int (*callback)(void *);
	void *param;
	struct work_struct event_w;
};

struct usb_bam_connect_info {
	u8 idx;
	u32 *src_pipe;
	u32 *dst_pipe;
	struct usb_bam_event_info wake_event;
	bool src_enabled;
	bool dst_enabled;
};

enum usb_bam_sm {
	USB_BAM_SM_INIT = 0,
	USB_BAM_SM_PLUG_NOTIFIED,
	USB_BAM_SM_PLUG_ACKED,
	USB_BAM_SM_UNPLUG_NOTIFIED,
};

struct usb_bam_peer_handhskae_info {
	enum usb_bam_sm state;
	bool client_ready;
	bool ack_received;
	int pending_work;
	struct usb_bam_event_info reset_event;
};

static struct usb_bam_connect_info usb_bam_connections[CONNECTIONS_NUM];
static struct usb_bam_pipe_connect ***msm_usb_bam_connections_info;
static struct usb_bam_pipe_connect *bam_connection_arr;
void __iomem *qscratch_ram1_reg;
struct clk *mem_clk;
struct clk *mem_iface_clk;
struct usb_bam_peer_handhskae_info peer_handhskae_info;

static int connect_pipe(u8 conn_idx, enum usb_bam_pipe_dir pipe_dir,
						u32 *usb_pipe_idx)
{
	int ret, ram1_value;
	struct sps_pipe **pipe = &sps_pipes[conn_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[conn_idx][pipe_dir];
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	struct usb_bam_pipe_connect *pipe_connection =
				&msm_usb_bam_connections_info
				[pdata->usb_active_bam][conn_idx][pipe_dir];

	*pipe = sps_alloc_endpoint();
	if (*pipe == NULL) {
		pr_err("%s: sps_alloc_endpoint failed\n", __func__);
		return -ENOMEM;
	}

	ret = sps_get_config(*pipe, connection);
	if (ret) {
		pr_err("%s: tx get config failed %d\n", __func__, ret);
		goto free_sps_endpoint;
	}

	ret = sps_phy2h(pipe_connection->src_phy_addr, &(connection->source));
	if (ret) {
		pr_err("%s: sps_phy2h failed (src BAM) %d\n", __func__, ret);
		goto free_sps_endpoint;
	}

	connection->src_pipe_index = pipe_connection->src_pipe_index;
	ret = sps_phy2h(pipe_connection->dst_phy_addr,
					&(connection->destination));
	if (ret) {
		pr_err("%s: sps_phy2h failed (dst BAM) %d\n", __func__, ret);
		goto free_sps_endpoint;
	}
	connection->dest_pipe_index = pipe_connection->dst_pipe_index;

	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		connection->mode = SPS_MODE_SRC;
		*usb_pipe_idx = connection->src_pipe_index;
	} else {
		connection->mode = SPS_MODE_DEST;
		*usb_pipe_idx = connection->dest_pipe_index;
	}

	/* If BAM is using dedicated SPS pipe memory, get it */
	if (pipe_connection->mem_type == SPS_PIPE_MEM) {
		pr_debug("%s: USB BAM using SPS pipe memory\n", __func__);
		ret = sps_setup_bam2bam_fifo(
				&data_mem_buf[conn_idx][pipe_dir],
				pipe_connection->data_fifo_base_offset,
				pipe_connection->data_fifo_size, 1);
		if (ret) {
			pr_err("%s: data fifo setup failure %d\n", __func__,
				ret);
			goto free_sps_endpoint;
		}

		ret = sps_setup_bam2bam_fifo(
				&desc_mem_buf[conn_idx][pipe_dir],
				pipe_connection->desc_fifo_base_offset,
				pipe_connection->desc_fifo_size, 1);
		if (ret) {
			pr_err("%s: desc. fifo setup failure %d\n", __func__,
				ret);
			goto free_sps_endpoint;
		}
	} else if (pipe_connection->mem_type == USB_PRIVATE_MEM) {
		pr_debug("%s: USB BAM using private memory\n", __func__);

		if (IS_ERR(mem_clk) || IS_ERR(mem_iface_clk)) {
			pr_err("%s: Failed to enable USB mem_clk\n", __func__);
			ret = IS_ERR(mem_clk);
			goto free_sps_endpoint;
		}

		clk_prepare_enable(mem_clk);
		clk_prepare_enable(mem_iface_clk);

		/*
		 * Enable USB PRIVATE RAM to be used for BAM FIFOs
		 * HSUSB: Only RAM13 is used for BAM FIFOs
		 * SSUSB: RAM11, 12, 13 are used for BAM FIFOs
		 */
		if (pdata->usb_active_bam == HSUSB_BAM)
			ram1_value = 0x4;
		else
			ram1_value = 0x7;

		pr_debug("Writing 0x%x to QSCRATCH_RAM1\n", ram1_value);
		writel_relaxed(ram1_value, qscratch_ram1_reg);

		data_mem_buf[conn_idx][pipe_dir].phys_base =
			pipe_connection->data_fifo_base_offset +
				pdata->usb_base_address;
		data_mem_buf[conn_idx][pipe_dir].size =
			pipe_connection->data_fifo_size;
		data_mem_buf[conn_idx][pipe_dir].base =
			ioremap(data_mem_buf[conn_idx][pipe_dir].phys_base,
				data_mem_buf[conn_idx][pipe_dir].size);
		memset(data_mem_buf[conn_idx][pipe_dir].base, 0,
			data_mem_buf[conn_idx][pipe_dir].size);

		desc_mem_buf[conn_idx][pipe_dir].phys_base =
			pipe_connection->desc_fifo_base_offset +
				pdata->usb_base_address;
		desc_mem_buf[conn_idx][pipe_dir].size =
			pipe_connection->desc_fifo_size;
		desc_mem_buf[conn_idx][pipe_dir].base =
			ioremap(desc_mem_buf[conn_idx][pipe_dir].phys_base,
				desc_mem_buf[conn_idx][pipe_dir].size);
		memset(desc_mem_buf[conn_idx][pipe_dir].base, 0,
			desc_mem_buf[conn_idx][pipe_dir].size);
	} else {
		pr_debug("%s: USB BAM using system memory\n", __func__);
		/* BAM would use system memory, allocate FIFOs */
		data_mem_buf[conn_idx][pipe_dir].size =
					pipe_connection->data_fifo_size;
		data_mem_buf[conn_idx][pipe_dir].base =
			dma_alloc_coherent(&usb_bam_pdev->dev,
				    pipe_connection->data_fifo_size,
				    &data_mem_buf[conn_idx][pipe_dir].phys_base,
				    0);
		memset(data_mem_buf[conn_idx][pipe_dir].base, 0,
					pipe_connection->data_fifo_size);

		desc_mem_buf[conn_idx][pipe_dir].size =
					pipe_connection->desc_fifo_size;
		desc_mem_buf[conn_idx][pipe_dir].base =
			dma_alloc_coherent(&usb_bam_pdev->dev,
				    pipe_connection->desc_fifo_size,
				    &desc_mem_buf[conn_idx][pipe_dir].phys_base,
				    0);
		memset(desc_mem_buf[conn_idx][pipe_dir].base, 0,
					pipe_connection->desc_fifo_size);
	}

	connection->data = data_mem_buf[conn_idx][pipe_dir];
	connection->desc = desc_mem_buf[conn_idx][pipe_dir];
	connection->event_thresh = 16;
	connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(*pipe, connection);
	if (ret < 0) {
		pr_err("%s: sps_connect failed %d\n", __func__, ret);
		goto error;
	}
	return 0;

error:
	sps_disconnect(*pipe);
free_sps_endpoint:
	sps_free_endpoint(*pipe);
	return ret;
}

static int connect_pipe_ipa(
			struct usb_bam_connect_ipa_params *connection_params)
{
	int ret;
	u8 conn_idx = connection_params->idx;
	enum usb_bam_pipe_dir pipe_dir = connection_params->dir;
	struct sps_pipe **pipe = &sps_pipes[conn_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[conn_idx][pipe_dir];
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	struct usb_bam_pipe_connect *pipe_connection =
				&msm_usb_bam_connections_info
				[pdata->usb_active_bam][conn_idx][pipe_dir];

	struct ipa_connect_params ipa_in_params;
	struct ipa_sps_params sps_out_params;
	u32 usb_handle, usb_phy_addr;
	u32 clnt_hdl = 0;

	memset(&ipa_in_params, 0, sizeof(ipa_in_params));
	memset(&sps_out_params, 0, sizeof(sps_out_params));

	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		usb_phy_addr = pipe_connection->src_phy_addr;
		ipa_in_params.client_ep_idx = pipe_connection->src_pipe_index;
	} else {
		usb_phy_addr = pipe_connection->dst_phy_addr;
		ipa_in_params.client_ep_idx = pipe_connection->dst_pipe_index;
	}
	/* Get HSUSB / HSIC bam handle */
	ret = sps_phy2h(usb_phy_addr, &usb_handle);
	if (ret) {
		pr_err("%s: sps_phy2h failed (HSUSB/HSIC BAM) %d\n",
			__func__, ret);
		return ret;
	}

	/* IPA input parameters */
	ipa_in_params.client_bam_hdl = usb_handle;
	ipa_in_params.desc_fifo_sz = pipe_connection->desc_fifo_size;
	ipa_in_params.data_fifo_sz = pipe_connection->data_fifo_size;
	ipa_in_params.notify = connection_params->notify;
	ipa_in_params.priv = connection_params->priv;
	ipa_in_params.client = connection_params->client;

	/* If BAM is using dedicated SPS pipe memory, get it */

	if (pipe_connection->mem_type == SPS_PIPE_MEM) {
		pr_debug("%s: USB BAM using SPS pipe memory\n", __func__);
		ret = sps_setup_bam2bam_fifo(
			&data_mem_buf[conn_idx][pipe_dir],
			pipe_connection->data_fifo_base_offset,
			pipe_connection->data_fifo_size, 1);
		if (ret) {
			pr_err("%s: data fifo setup failure %d\n", __func__,
				ret);
			return ret;
		}

		ret = sps_setup_bam2bam_fifo(
			&desc_mem_buf[conn_idx][pipe_dir],
			pipe_connection->desc_fifo_base_offset,
			pipe_connection->desc_fifo_size, 1);
		if (ret) {
			pr_err("%s: desc. fifo setup failure %d\n", __func__,
				ret);
			return ret;
		}

	} else {
		pr_err("%s: unsupported memory type(%d)\n",
			__func__, pipe_connection->mem_type);
		return -EINVAL;
	}

	ipa_in_params.desc = desc_mem_buf[conn_idx][pipe_dir];
	ipa_in_params.data = data_mem_buf[conn_idx][pipe_dir];

	memcpy(&ipa_in_params.ipa_ep_cfg, &connection_params->ipa_ep_cfg,
		   sizeof(struct ipa_ep_cfg));

	ret = ipa_connect(&ipa_in_params, &sps_out_params, &clnt_hdl);
	if (ret) {
		pr_err("%s: ipa_connect failed\n", __func__);
		return ret;
	}

	*pipe = sps_alloc_endpoint();
	if (*pipe == NULL) {
		pr_err("%s: sps_alloc_endpoint failed\n", __func__);
		ret = -ENOMEM;
		goto disconnect_ipa;
	}

	ret = sps_get_config(*pipe, connection);
	if (ret) {
		pr_err("%s: tx get config failed %d\n", __func__, ret);
		goto free_sps_endpoints;
	}

	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		/* USB src IPA dest */
		connection->mode = SPS_MODE_SRC;
		connection_params->cons_clnt_hdl = clnt_hdl;
		connection->source = usb_handle;
		connection->destination = sps_out_params.ipa_bam_hdl;
		connection->src_pipe_index = pipe_connection->src_pipe_index;
		connection->dest_pipe_index = sps_out_params.ipa_ep_idx;
		*(connection_params->src_pipe) = connection->src_pipe_index;
	} else {
		/* IPA src, USB dest */
		connection->mode = SPS_MODE_DEST;
		connection_params->prod_clnt_hdl = clnt_hdl;
		connection->source = sps_out_params.ipa_bam_hdl;
		connection->destination = usb_handle;
		connection->src_pipe_index = sps_out_params.ipa_ep_idx;
		connection->dest_pipe_index = pipe_connection->dst_pipe_index;
		*(connection_params->dst_pipe) = connection->dest_pipe_index;
	}

	connection->data = sps_out_params.data;
	connection->desc = sps_out_params.desc;
	connection->event_thresh = 16;
	connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(*pipe, connection);
	if (ret < 0) {
		pr_err("%s: sps_connect failed %d\n", __func__, ret);
		goto error;
	}

	return 0;

error:
	sps_disconnect(*pipe);
free_sps_endpoints:
	sps_free_endpoint(*pipe);
disconnect_ipa:
	ipa_disconnect(clnt_hdl);
	return ret;
}

static int disconnect_pipe(u8 connection_idx, enum usb_bam_pipe_dir pipe_dir)
{
	struct msm_usb_bam_platform_data *pdata =
				usb_bam_pdev->dev.platform_data;
	struct usb_bam_pipe_connect *pipe_connection =
			&msm_usb_bam_connections_info
			[pdata->usb_active_bam][connection_idx][pipe_dir];
	struct sps_pipe *pipe = sps_pipes[connection_idx][pipe_dir];
	struct sps_connect *connection =
		&sps_connections[connection_idx][pipe_dir];

	sps_disconnect(pipe);
	sps_free_endpoint(pipe);

	if (pipe_connection->mem_type == SYSTEM_MEM) {
		pr_debug("%s: Freeing system memory used by PIPE\n", __func__);
		dma_free_coherent(&usb_bam_pdev->dev, connection->data.size,
			  connection->data.base, connection->data.phys_base);
		dma_free_coherent(&usb_bam_pdev->dev, connection->desc.size,
			  connection->desc.base, connection->desc.phys_base);
	} else if (pipe_connection->mem_type == USB_PRIVATE_MEM) {
		pr_debug("Freeing USB private memory used by BAM PIPE\n");
		writel_relaxed(0x0, qscratch_ram1_reg);
		iounmap(connection->data.base);
		iounmap(connection->desc.base);
		clk_disable_unprepare(mem_clk);
		clk_disable_unprepare(mem_iface_clk);
	}

	connection->options &= ~SPS_O_AUTO_ENABLE;
	return 0;
}

int usb_bam_connect(u8 idx, u32 *src_pipe_idx, u32 *dst_pipe_idx)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	struct msm_usb_bam_platform_data *pdata =
				usb_bam_pdev->dev.platform_data;
	int usb_active_bam = pdata->usb_active_bam;
	int ret;

	if (!usb_bam_pdev) {
		pr_err("%s: usb_bam device not found\n", __func__);
		return -ENODEV;
	}

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if (connection->src_enabled && connection->dst_enabled) {
		pr_debug("%s: connection %d was already established\n",
			__func__, idx);
		return 0;
	}
	connection->src_pipe = src_pipe_idx;
	connection->dst_pipe = dst_pipe_idx;
	connection->idx = idx;

	/* Check if BAM requires RESET before connect */
	if (pdata->reset_on_connect[usb_active_bam] == true)
		sps_device_reset(h_bam);

	if (src_pipe_idx) {
		/* open USB -> Peripheral pipe */
		ret = connect_pipe(connection->idx, USB_TO_PEER_PERIPHERAL,
						   connection->src_pipe);
		if (ret) {
			pr_err("%s: src pipe connection failure\n", __func__);
			return ret;
		}
		connection->src_enabled = 1;
	}

	if (dst_pipe_idx) {
		/* open Peripheral -> USB pipe */
		ret = connect_pipe(connection->idx, PEER_PERIPHERAL_TO_USB,
						   connection->dst_pipe);
		if (ret) {
			pr_err("%s: dst pipe connection failure\n", __func__);
			return ret;
		}
		connection->dst_enabled = 1;
	}

	return 0;
}

int usb_bam_connect_ipa(struct usb_bam_connect_ipa_params *ipa_params)
{
	u8 idx = ipa_params->idx;
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if ((connection->src_enabled &&
		 ipa_params->dir == USB_TO_PEER_PERIPHERAL) ||
		 (connection->dst_enabled &&
		  ipa_params->dir == PEER_PERIPHERAL_TO_USB)) {
		pr_debug("%s: connection %d was already established\n",
			__func__, idx);
		return 0;
	}

	if (ipa_params->dir == USB_TO_PEER_PERIPHERAL)
		connection->src_pipe = ipa_params->src_pipe;
	else
		connection->dst_pipe = ipa_params->dst_pipe;

	connection->idx = idx;

	ret = connect_pipe_ipa(ipa_params);
	if (ret) {
		pr_err("%s: dst pipe connection failure\n", __func__);
		return ret;
	}

	if (ipa_params->dir == USB_TO_PEER_PERIPHERAL)
		connection->src_enabled = 1;
	else
		connection->dst_enabled = 1;

	return 0;
}

int usb_bam_client_ready(bool ready)
{
	spin_lock(&usb_bam_lock);
	if (peer_handhskae_info.client_ready == ready) {
		pr_debug("%s: client state is already %d\n",
			__func__, ready);
		spin_unlock(&usb_bam_lock);
		return 0;
	}

	peer_handhskae_info.client_ready = ready;

	spin_unlock(&usb_bam_lock);
	if (!queue_work(usb_bam_wq, &peer_handhskae_info.reset_event.event_w)) {
		spin_lock(&usb_bam_lock);
		peer_handhskae_info.pending_work++;
		spin_unlock(&usb_bam_lock);
	}

	return 0;
}

static void usb_bam_work(struct work_struct *w)
{
	struct usb_bam_event_info *event_info =
		container_of(w, struct usb_bam_event_info, event_w);

	event_info->callback(event_info->param);
}

static void usb_bam_wake_cb(struct sps_event_notify *notify)
{
	struct usb_bam_event_info *wake_event_info =
		(struct usb_bam_event_info *)notify->user;

	queue_work(usb_bam_wq, &wake_event_info->event_w);
}

static void usb_bam_sm_work(struct work_struct *w)
{
	pr_debug("%s: current state: %d\n", __func__,
		peer_handhskae_info.state);

	spin_lock(&usb_bam_lock);

	switch (peer_handhskae_info.state) {
	case USB_BAM_SM_INIT:
		if (peer_handhskae_info.client_ready) {
			spin_unlock(&usb_bam_lock);
			smsm_change_state(SMSM_APPS_STATE, 0,
				SMSM_USB_PLUG_UNPLUG);
			spin_lock(&usb_bam_lock);
			peer_handhskae_info.state = USB_BAM_SM_PLUG_NOTIFIED;
		}
		break;
	case USB_BAM_SM_PLUG_NOTIFIED:
		if (peer_handhskae_info.ack_received) {
			peer_handhskae_info.state = USB_BAM_SM_PLUG_ACKED;
			peer_handhskae_info.ack_received = 0;
		}
		break;
	case USB_BAM_SM_PLUG_ACKED:
		if (!peer_handhskae_info.client_ready) {
			spin_unlock(&usb_bam_lock);
			smsm_change_state(SMSM_APPS_STATE,
				SMSM_USB_PLUG_UNPLUG, 0);
			spin_lock(&usb_bam_lock);
			peer_handhskae_info.state = USB_BAM_SM_UNPLUG_NOTIFIED;
		}
		break;
	case USB_BAM_SM_UNPLUG_NOTIFIED:
		if (peer_handhskae_info.ack_received) {
			spin_unlock(&usb_bam_lock);
			peer_handhskae_info.reset_event.
				callback(peer_handhskae_info.reset_event.param);
			spin_lock(&usb_bam_lock);
			peer_handhskae_info.state = USB_BAM_SM_INIT;
			peer_handhskae_info.ack_received = 0;
		}
		break;
	}

	if (peer_handhskae_info.pending_work) {
		peer_handhskae_info.pending_work--;
		spin_unlock(&usb_bam_lock);
		queue_work(usb_bam_wq,
			&peer_handhskae_info.reset_event.event_w);
		spin_lock(&usb_bam_lock);
	}
	spin_unlock(&usb_bam_lock);
}

static void usb_bam_ack_toggle_cb(void *priv, uint32_t old_state,
	uint32_t new_state)
{
	static int last_processed_state;
	int current_state;

	spin_lock(&usb_bam_lock);

	current_state = new_state & SMSM_USB_PLUG_UNPLUG;

	if (current_state == last_processed_state) {
		spin_unlock(&usb_bam_lock);
		return;
	}

	last_processed_state = current_state;
	peer_handhskae_info.ack_received = true;

	spin_unlock(&usb_bam_lock);
	if (!queue_work(usb_bam_wq, &peer_handhskae_info.reset_event.event_w)) {
		spin_lock(&usb_bam_lock);
		peer_handhskae_info.pending_work++;
		spin_unlock(&usb_bam_lock);
	}
}

int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *user), void* param)
{
	struct sps_pipe *pipe = sps_pipes[idx][PEER_PERIPHERAL_TO_USB];
	struct sps_connect *sps_connection =
		&sps_connections[idx][PEER_PERIPHERAL_TO_USB];
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	struct usb_bam_event_info *wake_event_info =
		&connection->wake_event;
	int ret;

	wake_event_info->param = param;
	wake_event_info->callback = callback;
	wake_event_info->event.mode = SPS_TRIGGER_CALLBACK;
	wake_event_info->event.xfer_done = NULL;
	wake_event_info->event.callback = callback ? usb_bam_wake_cb : NULL;
	wake_event_info->event.user = wake_event_info;
	wake_event_info->event.options = SPS_O_WAKEUP;
	ret = sps_register_event(pipe, &wake_event_info->event);
	if (ret) {
		pr_err("%s: sps_register_event() failed %d\n", __func__, ret);
		return ret;
	}

	sps_connection->options = callback ?
		(SPS_O_AUTO_ENABLE | SPS_O_WAKEUP | SPS_O_WAKEUP_IS_ONESHOT) :
			SPS_O_AUTO_ENABLE;
	ret = sps_set_config(pipe, sps_connection);
	if (ret) {
		pr_err("%s: sps_set_config() failed %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

int usb_bam_register_peer_reset_cb(u8 idx,
	 int (*callback)(void *), void *param)
{
	u32 ret = 0;

	if (callback) {
		peer_handhskae_info.reset_event.param = param;
		peer_handhskae_info.reset_event.callback = callback;

		ret = smsm_state_cb_register(SMSM_MODEM_STATE,
			SMSM_USB_PLUG_UNPLUG, usb_bam_ack_toggle_cb, NULL);
		if (ret) {
			pr_err("%s: failed to register SMSM callback\n",
				__func__);
		} else {
			if (smsm_get_state(SMSM_MODEM_STATE) &
				SMSM_USB_PLUG_UNPLUG)
				usb_bam_ack_toggle_cb(NULL, 0,
					SMSM_USB_PLUG_UNPLUG);
		}
	} else {
		peer_handhskae_info.reset_event.param = NULL;
		peer_handhskae_info.reset_event.callback = NULL;
		smsm_state_cb_deregister(SMSM_MODEM_STATE,
			SMSM_USB_PLUG_UNPLUG, usb_bam_ack_toggle_cb, NULL);
	}

	return ret;
}

int usb_bam_disconnect_pipe(u8 idx)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	if (!connection->src_enabled && !connection->dst_enabled) {
		pr_debug("%s: connection %d isn't enabled\n",
			__func__, idx);
		return 0;
	}

	if (connection->src_enabled) {
		/* close USB -> Peripheral pipe */
		ret = disconnect_pipe(connection->idx, USB_TO_PEER_PERIPHERAL);
		if (ret) {
			pr_err("%s: src pipe connection failure\n", __func__);
			return ret;
		}
		connection->src_enabled = 0;
	}
	if (connection->dst_enabled) {
		/* close Peripheral -> USB pipe */
		ret = disconnect_pipe(connection->idx, PEER_PERIPHERAL_TO_USB);
		if (ret) {
			pr_err("%s: dst pipe connection failure\n", __func__);
			return ret;
		}
		connection->dst_enabled = 0;
	}

	connection->src_pipe = 0;
	connection->dst_pipe = 0;

	return 0;
}

int usb_bam_disconnect_ipa(u8 idx,
		struct usb_bam_connect_ipa_params *ipa_params)
{
	struct usb_bam_connect_info *connection = &usb_bam_connections[idx];
	int ret;

	if (!usb_bam_pdev) {
		pr_err("%s: usb_bam device not found\n", __func__);
		return -ENODEV;
	}

	if (idx >= CONNECTIONS_NUM) {
		pr_err("%s: Invalid connection index\n",
			__func__);
		return -EINVAL;
	}

	/* Currently just calls ipa_disconnect, no sps pipes
	   disconenction support */

	/* close IPA -> USB pipe */
	if (connection->dst_pipe) {
		ret = ipa_disconnect(ipa_params->prod_clnt_hdl);
		if (ret) {
			pr_err("%s: dst pipe disconnection failure\n",
				__func__);
			return ret;
		}
	}
	/* close USB -> IPA pipe */
	if (connection->src_pipe) {
		ret = ipa_disconnect(ipa_params->cons_clnt_hdl);
		if (ret) {
			pr_err("%s: src pipe disconnection failure\n",
				__func__);
			return ret;
		}
	}

	return 0;

}

int usb_bam_reset(void)
{
	struct usb_bam_connect_info *connection;
	int i;
	int ret = 0, ret_int;
	bool reconnect[CONNECTIONS_NUM];
	u32 *reconnect_src_pipe[CONNECTIONS_NUM];
	u32 *reconnect_dst_pipe[CONNECTIONS_NUM];

	/* Disconnect all pipes */
	for (i = 0; i < CONNECTIONS_NUM; i++) {
		connection = &usb_bam_connections[i];
		reconnect[i] = connection->src_enabled ||
			connection->dst_enabled;
		reconnect_src_pipe[i] = connection->src_pipe;
		reconnect_dst_pipe[i] = connection->dst_pipe;

		ret_int = usb_bam_disconnect_pipe(i);
		if (ret_int) {
			pr_err("%s: failure to connect pipe %d\n",
				__func__, i);
			ret = ret_int;
			continue;
		}
	}

	/* Reset USB/HSIC BAM */
	if (sps_device_reset(h_bam))
		pr_err("%s: BAM reset failed\n", __func__);

	/* Reconnect all pipes */
	for (i = 0; i < CONNECTIONS_NUM; i++) {
		connection = &usb_bam_connections[i];
		if (reconnect[i]) {
			ret_int = usb_bam_connect(i, reconnect_src_pipe[i],
				reconnect_dst_pipe[i]);
			if (ret_int) {
				pr_err("%s: failure to reconnect pipe %d\n",
					__func__, i);
				ret = ret_int;
				continue;
			}
		}
	}

	return ret;
}

static int update_connections_info(struct device_node *node, int bam,
	int conn_num, int dir, enum usb_pipe_mem_type mem_type)
{
	u32 rc;
	char *key = NULL;
	uint32_t val = 0;

	struct usb_bam_pipe_connect *pipe_connection;

	pipe_connection = &msm_usb_bam_connections_info[bam][conn_num][dir];

	pipe_connection->mem_type = mem_type;

	key = "qcom,src-bam-physical-address";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->src_phy_addr = val;

	key = "qcom,src-bam-pipe-index";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->src_pipe_index = val;

	key = "qcom,dst-bam-physical-address";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->dst_phy_addr = val;

	key = "qcom,dst-bam-pipe-index";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->dst_pipe_index = val;

	key = "qcom,data-fifo-offset";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->data_fifo_base_offset = val;

	key = "qcom,data-fifo-size";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->data_fifo_size = val;

	key = "qcom,descriptor-fifo-offset";
	rc = of_property_read_u32(node, key, &val);
	if (!rc)
		pipe_connection->desc_fifo_base_offset = val;

	key = "qcom,descriptor-fifo-size";
	rc = of_property_read_u32(node, key, &val);
	if (rc)
		goto err;
	pipe_connection->desc_fifo_size = val;

	return 0;

err:
	pr_err("%s: Error in name %s key %s\n", __func__,
		node->full_name, key);
	return -EFAULT;
}

static int usb_bam_update_conn_array_index(struct platform_device *pdev,
		void *buff, int bam_max, int conn_max, int pipe_dirs)
{
	int bam_num, conn_num;
	struct usb_bam_pipe_connect *bam_connection_arr = buff;

	msm_usb_bam_connections_info = devm_kzalloc(&pdev->dev,
		bam_max * sizeof(struct usb_bam_pipe_connect **),
		GFP_KERNEL);

	if (!msm_usb_bam_connections_info)
		return -ENOMEM;

	for (bam_num = 0; bam_num < bam_max; bam_num++) {
		msm_usb_bam_connections_info[bam_num] =
			devm_kzalloc(&pdev->dev, conn_max *
			sizeof(struct usb_bam_pipe_connect *),
			GFP_KERNEL);
		if (!msm_usb_bam_connections_info[bam_num])
			return -ENOMEM;

		for (conn_num = 0; conn_num < conn_max; conn_num++)
			msm_usb_bam_connections_info[bam_num][conn_num] =
				bam_connection_arr +
				(bam_num * conn_max * pipe_dirs) +
				(conn_num * pipe_dirs);
	}

	return 0;
}

static u8 qdss_conn_num;

u8 usb_bam_get_qdss_num(void)
{
	return qdss_conn_num;
}
EXPORT_SYMBOL(usb_bam_get_qdss_num);

static struct msm_usb_bam_platform_data *usb_bam_dt_to_pdata(
	struct platform_device *pdev)
{
	struct msm_usb_bam_platform_data *pdata;
	struct device_node *node = pdev->dev.of_node;
	int conn_num, bam;
	u8 dir;
	u8 ncolumns = 2;
	int bam_amount, rc = 0;
	u32 pipe_entry = 0;
	char *key = NULL;
	enum usb_pipe_mem_type mem_type;
	bool reset_bam;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("unable to allocate platform data\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-active-bam",
		&pdata->usb_active_bam);
	if (rc) {
		pr_err("Invalid usb active bam property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-total-bam-num",
		&pdata->total_bam_num);
	if (rc) {
		pr_err("Invalid usb total bam num property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-bam-num-pipes",
		&pdata->usb_bam_num_pipes);
	if (rc) {
		pr_err("Invalid usb bam num pipes property\n");
		return NULL;
	}

	rc = of_property_read_u32(node, "qcom,usb-base-address",
		&pdata->usb_base_address);
	if (rc)
		pr_debug("%s: Invalid usb base address property\n", __func__);

	pdata->ignore_core_reset_ack = of_property_read_bool(node,
					"qcom,ignore-core-reset-ack");

	pdata->disable_clk_gating = of_property_read_bool(node,
					"qcom,disable-clk-gating");

	for_each_child_of_node(pdev->dev.of_node, node)
		pipe_entry++;

	/*
	 * we need to know the number of connection, so we will know
	 * how much memory to allocate
	 */
	conn_num = pipe_entry / 2;
	bam_amount = pdata->total_bam_num;

	if (conn_num <= 0 || conn_num >= pdata->usb_bam_num_pipes)
		goto err;


	/* alloc msm_usb_bam_connections_info */
	bam_connection_arr = devm_kzalloc(&pdev->dev, bam_amount *
		conn_num * ncolumns *
		sizeof(struct usb_bam_pipe_connect), GFP_KERNEL);

	if (!bam_connection_arr)
		goto err;

	rc = usb_bam_update_conn_array_index(pdev, bam_connection_arr,
					bam_amount, conn_num, ncolumns);
	if (rc)
		goto err;

	/* retrieve device tree parameters */
	for_each_child_of_node(pdev->dev.of_node, node) {
		const char *str;

		key = "qcom,usb-bam-type";
		rc = of_property_read_u32(node, key, &bam);
		if (rc)
			goto err;

		key = "qcom,usb-bam-mem-type";
		rc = of_property_read_u32(node, key, &mem_type);
		if (rc)
			goto err;

		if (mem_type == USB_PRIVATE_MEM &&
			!pdata->usb_base_address)
			goto err;

		rc = of_property_read_string(node, "label", &str);
		if (rc) {
			pr_err("Cannot read string\n");
			goto err;
		}
		reset_bam = of_property_read_bool(node,
					"qcom,reset-bam-on-connect");
		if (reset_bam)
			pdata->reset_on_connect[bam] = true;

		if (strnstr(str, "usb-to", 30))
			dir = USB_TO_PEER_PERIPHERAL;
		else if (strnstr(str, "to-usb", 30))
			dir = PEER_PERIPHERAL_TO_USB;
		else
			goto err;

		/* Check if connection type is supported */
		if (!strcmp(str, "usb-to-peri-qdss-dwc3") ||
			!strcmp(str, "peri-to-usb-qdss-dwc3") ||
			!strcmp(str, "usb-to-ipa") ||
			!strcmp(str, "ipa-to-usb") ||
			!strcmp(str, "usb-to-peri-qdss-hsusb") ||
			!strcmp(str, "peri-to-usb-qdss-hsusb"))
				conn_num = 0;
		else if (!strcmp(str, "usb-to-qdss-hsusb") ||
			!strcmp(str, "qdss-to-usb-hsusb")) {
				conn_num = 1;
				qdss_conn_num = 1;
		}
		else
			goto err;

		rc = update_connections_info(node, bam, conn_num,
						dir, mem_type);
		if (rc)
			goto err;
	}

	pdata->connections = &msm_usb_bam_connections_info[0][0][0];

	return pdata;
err:
	pr_err("%s: failed\n", __func__);
	return NULL;
}

static char *bam_enable_strings[3] = {
	[SSUSB_BAM] = "ssusb",
	[HSUSB_BAM] = "hsusb",
	[HSIC_BAM]  = "hsic",
};

static int usb_bam_init(void)
{
	int ret;
	void *usb_virt_addr;
	struct msm_usb_bam_platform_data *pdata =
		usb_bam_pdev->dev.platform_data;
	struct usb_bam_pipe_connect *pipe_connection =
		&msm_usb_bam_connections_info[pdata->usb_active_bam][0][0];
	struct resource *res, *ram_resource;
	int irq;

	qdss_conn_num = 0;

	res = platform_get_resource_byname(usb_bam_pdev, IORESOURCE_MEM,
				bam_enable_strings[pdata->usb_active_bam]);
	if (!res) {
		dev_err(&usb_bam_pdev->dev, "Unable to get memory resource\n");
		return -ENODEV;
	}

	irq = platform_get_irq_byname(usb_bam_pdev,
				bam_enable_strings[pdata->usb_active_bam]);
	if (irq < 0) {
		dev_err(&usb_bam_pdev->dev, "Unable to get IRQ resource\n");
		return irq;
	}

	usb_virt_addr = devm_ioremap(&usb_bam_pdev->dev, res->start,
							 resource_size(res));
	if (!usb_virt_addr) {
		pr_err("%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}

	/* Check if USB3 pipe memory needs to be enabled */
	if (pipe_connection->mem_type == USB_PRIVATE_MEM) {
		pr_debug("%s: Enabling USB private memory for: %s\n", __func__,
				bam_enable_strings[pdata->usb_active_bam]);

		ram_resource = platform_get_resource_byname(usb_bam_pdev,
					 IORESOURCE_MEM, "qscratch_ram1_reg");
		if (!res) {
			dev_err(&usb_bam_pdev->dev, "Unable to get qscratch\n");
			ret = -ENODEV;
			goto free_bam_regs;
		}

		qscratch_ram1_reg = devm_ioremap(&usb_bam_pdev->dev,
						ram_resource->start,
						resource_size(ram_resource));
		if (!qscratch_ram1_reg) {
			pr_err("%s: ioremap failed for qscratch\n", __func__);
			ret = -ENOMEM;
			goto free_bam_regs;
		}
	}
	usb_props.phys_addr = res->start;
	usb_props.virt_addr = usb_virt_addr;
	usb_props.virt_size = resource_size(res);
	usb_props.irq = irq;
	usb_props.summing_threshold = USB_SUMMING_THRESHOLD;
	usb_props.event_threshold = 512;
	usb_props.num_pipes = pdata->usb_bam_num_pipes;
	/*
	 * HSUSB and HSIC Cores don't support RESET ACK signal to BAMs
	 * Hence, let BAM to ignore acknowledge from USB while resetting PIPE
	 */
	if (pdata->ignore_core_reset_ack && pdata->usb_active_bam != SSUSB_BAM)
		usb_props.options = SPS_BAM_NO_EXT_P_RST;
	if (pdata->disable_clk_gating)
		usb_props.options |= SPS_BAM_NO_LOCAL_CLK_GATING;

	ret = sps_register_bam_device(&usb_props, &h_bam);
	if (ret < 0) {
		pr_err("%s: register bam error %d\n", __func__, ret);
		ret = -EFAULT;
		goto free_qscratch_reg;
	}

	return 0;

free_qscratch_reg:
	iounmap(qscratch_ram1_reg);
free_bam_regs:
	iounmap(usb_virt_addr);

	return ret;
}

static ssize_t
usb_bam_show_enable(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct msm_usb_bam_platform_data *pdata = dev->platform_data;

	if (!pdata)
		return 0;
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 bam_enable_strings[pdata->usb_active_bam]);
}

static ssize_t usb_bam_store_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct msm_usb_bam_platform_data *pdata = dev->platform_data;
	char str[10], *pstr;
	int ret, i;

	if (!pdata) {
		dev_err(dev, "no usb_bam pdata found\n");
		return -ENODEV;
	}

	strlcpy(str, buf, sizeof(str));
	pstr = strim(str);

	for (i = 0; i < ARRAY_SIZE(bam_enable_strings); i++) {
		if (!strncmp(pstr, bam_enable_strings[i], sizeof(str)))
			pdata->usb_active_bam = i;
	}

	dev_dbg(dev, "active_bam=%s\n",
		bam_enable_strings[pdata->usb_active_bam]);

	ret = usb_bam_init();
	if (ret) {
		dev_err(dev, "failed to initialize usb bam\n");
		return ret;
	}

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUSR, usb_bam_show_enable,
		   usb_bam_store_enable);

static int usb_bam_probe(struct platform_device *pdev)
{
	int ret, i;
	struct msm_usb_bam_platform_data *pdata;

	dev_dbg(&pdev->dev, "usb_bam_probe\n");

	for (i = 0; i < CONNECTIONS_NUM; i++) {
		usb_bam_connections[i].src_enabled = 0;
		usb_bam_connections[i].dst_enabled = 0;
		INIT_WORK(&usb_bam_connections[i].wake_event.event_w,
			usb_bam_work);
	}

	spin_lock_init(&usb_bam_lock);
	INIT_WORK(&peer_handhskae_info.reset_event.event_w, usb_bam_sm_work);

	mem_clk = devm_clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(mem_clk))
		dev_dbg(&pdev->dev, "failed to get mem_clock\n");

	mem_iface_clk = devm_clk_get(&pdev->dev, "mem_iface_clk");
	if (IS_ERR(mem_iface_clk))
		dev_dbg(&pdev->dev, "failed to get mem_iface_clock\n");

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "device tree enabled\n");
		pdata = usb_bam_dt_to_pdata(pdev);
		if (!pdata)
			return -ENOMEM;
		pdev->dev.platform_data = pdata;
	} else if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return -ENODEV;
	} else {
		pdata = pdev->dev.platform_data;
		ret = usb_bam_update_conn_array_index(pdev, pdata->connections,
				MAX_BAMS, CONNECTIONS_NUM, 2);
		if (ret) {
			pr_err("usb_bam_update_conn_array_index failed\n");
			return ret;
		}
	}
	usb_bam_pdev = pdev;

	ret = device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret)
		dev_err(&pdev->dev, "failed to create device file\n");

	usb_bam_wq = alloc_workqueue("usb_bam_wq",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!usb_bam_wq) {
		pr_err("unable to create workqueue usb_bam_wq\n");
		return -ENOMEM;
	}

	return ret;
}

void get_bam2bam_connection_info(u8 conn_idx, enum usb_bam_pipe_dir pipe_dir,
	u32 *usb_bam_handle, u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo)
{
	struct sps_connect *connection =
		&sps_connections[conn_idx][pipe_dir];


	if (pipe_dir == USB_TO_PEER_PERIPHERAL) {
		*usb_bam_handle = connection->source;
		*usb_bam_pipe_idx = connection->src_pipe_index;
		*peer_pipe_idx = connection->dest_pipe_index;
	} else {
		*usb_bam_handle = connection->destination;
		*usb_bam_pipe_idx = connection->dest_pipe_index;
		*peer_pipe_idx = connection->src_pipe_index;
	}
	if (data_fifo)
		memcpy(data_fifo, &data_mem_buf[conn_idx][pipe_dir],
			sizeof(struct sps_mem_buffer));
	if (desc_fifo)
		memcpy(desc_fifo, &desc_mem_buf[conn_idx][pipe_dir],
			sizeof(struct sps_mem_buffer));
}
EXPORT_SYMBOL(get_bam2bam_connection_info);

static int usb_bam_remove(struct platform_device *pdev)
{
	destroy_workqueue(usb_bam_wq);

	return 0;
}

static const struct of_device_id usb_bam_dt_match[] = {
	{ .compatible = "qcom,usb-bam-msm",
	},
	{}
};
MODULE_DEVICE_TABLE(of, usb_bam_dt_match);

static struct platform_driver usb_bam_driver = {
	.probe = usb_bam_probe,
	.remove = usb_bam_remove,
	.driver		= {
		.name	= "usb_bam",
		.of_match_table = usb_bam_dt_match,
	},
};

static int __init init(void)
{
	return platform_driver_register(&usb_bam_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	platform_driver_unregister(&usb_bam_driver);
}
module_exit(cleanup);

MODULE_DESCRIPTION("MSM USB BAM DRIVER");
MODULE_LICENSE("GPL v2");
