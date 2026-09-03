/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/trusty/trusty_ipc.h>
#include "../include/wcn_dbg.h"
#include "wcn_ca_trusty.h"

#define WCN_TEE_PORT_NAME "com.android.trusty.kernelbootcp"
#define GNSS_TEE_PORT_NAME "com.android.trusty.kernelbootcp"

static struct wcn_ca_tipc_ctx wcn_tee_tipc = {TIPC_CHANNEL_DISCONNECTED, };
static struct wcn_ca_tipc_ctx gnss_tee_tipc = {TIPC_CHANNEL_DISCONNECTED, };

static struct tipc_msg_buf *wcn_ca_handle_msg(void *data,
					struct tipc_msg_buf *rxbuf, u16 flags)
{
	struct wcn_ca_tipc_ctx *dn = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(dn->chan);
		if (newbuf) {
			WCN_INFO("received new data, rxbuf %p, newbuf %p\n",
						  rxbuf, newbuf);
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &dn->rx_msg_queue);
			wake_up_interruptible(&dn->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			WCN_INFO("discard incoming message\n");
			newbuf = rxbuf;
		}
	}
	mutex_unlock(&dn->lock);

	return newbuf;
}

static void wcn_ca_handle_event(void *data, int event)
{
	struct wcn_ca_tipc_ctx *dn = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		WCN_INFO("wcn_ca channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		WCN_INFO("wcn_ca channel disconnected\n");
		break;

	case TIPC_CHANNEL_CONNECTED:
		WCN_INFO("wcn_ca channel connected\n");
		dn->state = TIPC_CHANNEL_CONNECTED;
		wake_up_interruptible(&dn->readq);
		break;

	default:
		WCN_ERR("%s: wcn_ca unhandled event %d\n", __func__, event);
		break;
	}
}

static struct tipc_chan_ops wcn_ca_chan_ops = {
	.handle_msg = wcn_ca_handle_msg,
	.handle_event = wcn_ca_handle_event,
};

static bool wcn_ca_tipc_connect(struct wcn_ca_tipc_ctx *tipc, const char *port)
{
	struct tipc_chan *chan;
	bool ret = false;
	int chan_conn_ret = 0;
	long timeout;

	if (tipc->state == TIPC_CHANNEL_CONNECTED) {
		WCN_ERR("channel already connected\n");
		return true;
	}

	chan = tipc_create_channel(NULL, &wcn_ca_chan_ops, tipc);
	if (IS_ERR(chan)) {
		WCN_ERR("wcn_ca create channel failed\n");
		return PTR_ERR(chan);
	}

	tipc->chan = chan;
	mutex_init(&tipc->lock);
	init_waitqueue_head(&tipc->readq);
	INIT_LIST_HEAD(&tipc->rx_msg_queue);

	chan_conn_ret = tipc_chan_connect(tipc->chan, port);
	if (chan_conn_ret) {
		WCN_ERR("fail to connect channel\n");
		ret = false;
		return ret;
	}
	ret = true;

	timeout = wait_event_interruptible_timeout(tipc->readq,
		(tipc->state == TIPC_CHANNEL_CONNECTED),
		msecs_to_jiffies(2000));
	if (timeout <= 0) {
		WCN_ERR("fail to %s, waiting exit %ld!\n", __func__, timeout);
		ret = false;
		return ret;
	}

	WCN_INFO("wcn_ca connect channel done\n");

	return ret;
}

static void wcn_ca_tipc_free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb = NULL;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	while (mb) {
		list_del(&mb->node);

		free_pages_exact(mb->buf_va, mb->buf_sz);
		kfree(mb);

		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

static void wcn_ca_tipc_disconnect(struct wcn_ca_tipc_ctx *tipc)
{
	wake_up_interruptible_all(&tipc->readq);

	wcn_ca_tipc_free_msg_buf_list(&tipc->rx_msg_queue);

	/* shutdown channel  */
	tipc_chan_shutdown(tipc->chan);

	tipc->state = TIPC_CHANNEL_DISCONNECTED;

	/* and destroy it */
	tipc_chan_destroy(tipc->chan);

	WCN_INFO("wcn_ca tipc disconnect\n");
}

static ssize_t wcn_ca_tipc_read(struct wcn_ca_tipc_ctx *tipc,
					void *data_ptr, size_t max_len)
{
	struct tipc_msg_buf *mb;
	ssize_t len;
	long timeout;

	timeout = wait_event_interruptible_timeout(tipc->readq,
		!list_empty(&tipc->rx_msg_queue),
		msecs_to_jiffies(2000));
	if (timeout <= 0) {
		WCN_ERR("fail to %s, waiting exit %ld!\n", __func__, timeout);
		return -ETIMEDOUT;
	}

	mb = list_first_entry(&tipc->rx_msg_queue, struct tipc_msg_buf, node);

	len = mb_avail_data(mb);
	if (len > max_len)
		len = max_len;

	memcpy(data_ptr, mb_get_data(mb, len), len);

	list_del(&mb->node);
	tipc_chan_put_rxbuf(tipc->chan, mb);

	return len;
}

static ssize_t wcn_ca_tipc_write(struct wcn_ca_tipc_ctx *tipc,
					void *data_ptr, size_t len)
{
	int ret = -1;
	int avail;
	struct tipc_msg_buf *txbuf = NULL;
	long timeout = 2000;

	if (!tipc) {
		WCN_ERR("wcn_ca tipc context null!\n");
		return ret;
	}

	txbuf = tipc_chan_get_txbuf_timeout(tipc->chan, timeout);
	if (IS_ERR(txbuf))
		return  PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		WCN_ERR("write no buffer space, len = %d, avail = %d\n",
			(int)len, (int)avail);
		ret = -EMSGSIZE;
		goto err;
	}

	memcpy(mb_put_data(txbuf, len), data_ptr, len);

	ret = tipc_chan_queue_msg(tipc->chan, txbuf);
	if (ret) {
		WCN_ERR("tipc_chan_queue_msg error :%d\n", ret);
		goto err;
	}

	return ret;

err:
	tipc_chan_put_txbuf(tipc->chan, txbuf);
	return ret;
}

static int do_wcn_firmware_sec_verify(struct firmware_verify_ctrl *verify_ctrl)
{
	struct kernelbootcp_message *verify_request = NULL;
	struct kernelbootcp_message *verify_result = NULL;
	struct KBC_IMAGE_S *kbc_image_sptr = NULL;
	int rc = -1;
	size_t len = sizeof(struct kernelbootcp_message), event_data_len;
	u32 cmd = 0;

	if (verify_ctrl->wcn_or_gnss_bin == 1) {
		event_data_len = sizeof(struct KBC_LOAD_TABLE_W);
		cmd = KERNEL_BOOTCP_VERIFY_WCN;
	} else if (verify_ctrl->wcn_or_gnss_bin == 2) {
		event_data_len = sizeof(struct KBC_LOAD_TABLE_G);
		cmd = KERNEL_BOOTCP_VERIFY_GPS;
	} else {
		goto ret;
	}

	if (wcn_ca_tipc_connect(verify_ctrl->ca_tipc_ctx,
			verify_ctrl->tipc_chan_name) != true) {
		WCN_ERR("wcn_ca tipc init fail, Or we are in Nsec Mode.\n");
		rc = 0;
		goto ret;
	}

	len += event_data_len;
	verify_request = kmalloc(len, GFP_KERNEL);
	if (!verify_request)
		goto ret;
	verify_result = kmalloc(len, GFP_KERNEL);
	if (!verify_result)
		goto free_request_buff;

	memset(verify_request, 0, len);
	memset(verify_result, 0, len);

	verify_request->cmd = cmd;
	kbc_image_sptr = (struct KBC_IMAGE_S *)verify_request->payload;
	kbc_image_sptr->img_addr = verify_ctrl->bin_base_addr;
	kbc_image_sptr->img_len = verify_ctrl->bin_length;
	kbc_image_sptr->map_len = verify_ctrl->bin_length;

	WCN_INFO("wcn_ca verify request cmd = %d, payload = %d, len = %ld\n",
		verify_request->cmd, verify_request->payload[0], len);
	rc = wcn_ca_tipc_write(verify_ctrl->ca_tipc_ctx,
			verify_request, len);
	if (rc != 0) {
		WCN_ERR("wcn_ca, tipc write rc = %d\n", rc);
		rc = -2;
		goto tpic_disconnect;
	}

	/* align with TOS QieGang, the system will assert after Tos
	 * validate bin failed, so we may not need the response.
	 */
	WCN_INFO("wcn_ca tipc read\n");
	rc = wcn_ca_tipc_read(verify_ctrl->ca_tipc_ctx,
			verify_result, len);
	if (rc <= 0) {
		WCN_ERR("wcn_ca, tipc read rc = %d\n", rc);
		rc = -3;
		goto tpic_disconnect;
	}

	WCN_INFO("wcn_ca tipc read, cmd = %d, payload = %d\n",
		(int)verify_result->cmd, (int)verify_result->payload[0]);
	if (verify_result->cmd !=
		(verify_request->cmd | KERNELBOOTCP_BIT)) {
		WCN_ERR("verify_result failed.\n");
		rc = -4;
	}

tpic_disconnect:
	wcn_ca_tipc_disconnect(verify_ctrl->ca_tipc_ctx);
	kfree(verify_result);
free_request_buff:
	kfree(verify_request);
ret:
	return rc;
}

int wcn_firmware_sec_verify(u32 wcn_or_gnss_bin,
		phys_addr_t bin_base_addr, u32 bin_length)
{
	struct firmware_verify_ctrl verify_ctrl;
	int ret = -1;

	verify_ctrl.wcn_or_gnss_bin = wcn_or_gnss_bin;
	verify_ctrl.bin_base_addr = bin_base_addr;
	verify_ctrl.bin_length = bin_length;

	if (verify_ctrl.wcn_or_gnss_bin == 1) {
		verify_ctrl.ca_tipc_ctx = &wcn_tee_tipc;
		verify_ctrl.tipc_chan_name = WCN_TEE_PORT_NAME;
	} else if (verify_ctrl.wcn_or_gnss_bin == 2) {
		verify_ctrl.ca_tipc_ctx = &gnss_tee_tipc;
		verify_ctrl.tipc_chan_name = GNSS_TEE_PORT_NAME;
	} else {
		return ret;
	}

	return do_wcn_firmware_sec_verify(&verify_ctrl);
}
