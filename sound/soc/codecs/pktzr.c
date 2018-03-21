/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <soc/qcom/bg_glink.h>
#include "pktzr.h"

#define MSM_BG_THREAD_TIMEOUT (9 * (HZ/10))
#define PKTZR_MAX_CHANNELS 4

struct pktzr_hdr {
	uint8_t version;
	uint8_t reserved;
	uint16_t opcode;
	uint8_t client_id;
	uint8_t domain_id;
	uint16_t token;
	uint32_t payload_size;
};

/* pkt format */
struct pktzr_pkt {
	struct pktzr_hdr hdr;
	uint8_t payload[0];
};

struct pktzr_node {
	struct list_head list;
	uint16_t token;
	uint16_t cmd;
	void *priv;
};

struct pktzr_priv {
	void  *ch_info[PKTZR_MAX_CHANNELS];
	struct completion thread_complete;
	struct mutex pktzr_lock;
	uint16_t token;
	struct list_head ch_list;
	struct platform_device *pdev;
	pktzr_data_cmd_cb_fn data_cmd_cb;
	int num_channels;
	bool pktzr_init_complete;
};

static struct pktzr_priv *ppriv;

static int pktzr_resp_cb(void *payload, int size)
{
	int rc = 0;
	struct pktzr_pkt *pkt;
	struct pktzr_hdr *pkt_hdr;
	struct pktzr_node *pnode, *tmp;

	if (!payload) {
		pr_err("%s: payload is NULL\n", __func__);
		rc = -EINVAL;
		goto done;
	}
	pr_debug("%s: entry\n", __func__);

	pkt = (struct pktzr_pkt *)payload;
	pkt_hdr = &pkt->hdr;

	mutex_lock(&ppriv->pktzr_lock);
	list_for_each_entry_safe(pnode, tmp, &ppriv->ch_list, list) {
		if (pnode->token == pkt_hdr->token) {
			if (pkt_hdr->opcode == PKTZR_BASIC_RESPONSE_RESULT) {
				pr_debug("%s: CMD rsp: success token %d\n",
						__func__, pkt_hdr->token);
				complete(&ppriv->thread_complete);
			} else
				pr_err("%s: CMD rsp: fail token %d\n",
						__func__, pkt_hdr->token);

			mutex_unlock(&ppriv->pktzr_lock);
			return rc;
		}
	}
	pr_err("Invalid token %d or the command timedOut\n", pkt_hdr->token);
	mutex_unlock(&ppriv->pktzr_lock);
done:
	return rc;
}

static int pktzr_send_pkt(void *payload, uint32_t size, void *rsp,
		   uint16_t cmd,  bool sync_cmd)
{
	struct pktzr_pkt *pkt_hdr;
	struct pktzr_node *pnode;
	int pkt_size = 0;
	int rc = 0;

	pr_debug("%s: cmd=%d sync=%d size=%d\n", __func__, cmd, sync_cmd, size);
	if (!ppriv || !ppriv->pktzr_init_complete) {
		pr_err_ratelimited("packetizer not initialized\n");
		return -EINVAL;
	}
	mutex_lock(&ppriv->pktzr_lock);
	if (++ppriv->token == 0)
		ppriv->token = 1;

	pkt_size = sizeof(struct pktzr_pkt) + size;
	pkt_hdr = kzalloc(pkt_size, GFP_KERNEL);
	if (!pkt_hdr) {
		pr_err("%s: Failed to initialise pkt header\n", __func__);
		return -ENOMEM;
	}

	pnode = kzalloc(sizeof(struct pktzr_node), GFP_KERNEL);
	if (!pnode) {
		kfree(pkt_hdr);
		return -ENOMEM;
	}

	pnode->priv = rsp;
	pnode->token = ppriv->token;
	pnode->cmd = cmd;
	pkt_hdr->hdr.version = VERSION_ID;
	pkt_hdr->hdr.opcode = cmd;
	pkt_hdr->hdr.client_id = CLIENT_ID_AUDIO;
	pkt_hdr->hdr.domain_id = DOMAIN_ID_APPS;
	pkt_hdr->hdr.token = ppriv->token;
	pkt_hdr->hdr.payload_size = size;
	memcpy(pkt_hdr->payload, (char *)payload, size);
	INIT_LIST_HEAD(&pnode->list);
	pr_debug("ppriv->token = %d\n", ppriv->token);
	list_add_tail(&pnode->list, &ppriv->ch_list);
	mutex_unlock(&ppriv->pktzr_lock);

	if (cmd == PKTZR_CMD_DATA)
		rc = bg_cdc_glink_write(ppriv->ch_info[1], pkt_hdr, pkt_size);
	else
		rc = bg_cdc_glink_write(ppriv->ch_info[0], pkt_hdr, pkt_size);
	if (rc < 0) {
		pr_err("%s: Failed to send command over glink\n", __func__);
		goto exit;
	}


	if (sync_cmd) {
		pr_debug("%s: command sent waiting!\n", __func__);
		rc = wait_for_completion_timeout(&ppriv->thread_complete,
						 MSM_BG_THREAD_TIMEOUT);
		if (!rc) {
			pr_err("%s: Wait for thread timedout\n", __func__);
			rc = -ETIMEDOUT;
			goto exit;
		}
		mutex_lock(&ppriv->pktzr_lock);
		list_del(&pnode->list);
		kfree(pnode);
		pnode = NULL;
		mutex_unlock(&ppriv->pktzr_lock);
	}
	pr_debug("%s: command processing done\n", __func__);
exit:
	/* Free memory */
	kfree(pkt_hdr);
	mutex_lock(&ppriv->pktzr_lock);
	if (pnode) {
		list_del(&pnode->list);
		kfree(pnode);
	}
	mutex_unlock(&ppriv->pktzr_lock);
	return rc;
}

int pktzr_cmd_open(void *payload, uint32_t size, struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_OPEN, true);
}

int pktzr_cmd_close(void *payload, uint32_t size, struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_CLOSE, true);
}

int pktzr_cmd_start(void *payload, uint32_t size, struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_START, true);
}

int pktzr_cmd_stop(void *payload, uint32_t size, struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_STOP, true);
}

int pktzr_cmd_set_params(void *payload, uint32_t size,
				struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_SET_CONFIG, true);
}

int pktzr_cmd_data(void *payload, uint32_t size, void *priv_data)
{
	return pktzr_send_pkt(payload, size, priv_data, PKTZR_CMD_DATA, false);
}

int pktzr_cmd_init_params(void *payload, uint32_t size,
				struct pktzr_cmd_rsp *rsp)
{
	return pktzr_send_pkt(payload, size, rsp, PKTZR_CMD_INIT_PARAM, true);
}

int pktzr_init(void *pdev, struct bg_glink_ch_cfg *ch_info, int num_channels,
	       pktzr_data_cmd_cb_fn func)
{
	int i;

	if (num_channels > 4) {
		pr_err("%s: Invalid num channels:%d\n", __func__, num_channels);
		return -EINVAL;
	}

	if (!ppriv) {
		ppriv = kzalloc((sizeof(struct pktzr_priv)), GFP_KERNEL);
		if (!ppriv) {
			pr_err("%s:Failed to allocate memory for ppriv\n",
					__func__);
			return -ENOMEM;
		}
	} else {
		pr_debug("%s: Already initialized\n", __func__);
		goto done;
	}

	for (i = 0; i < num_channels; i++) {
		ppriv->ch_info[i] = bg_cdc_channel_open(pdev,
							&ch_info[i],
							pktzr_resp_cb);
		if (!ppriv->ch_info[i]) {
			pr_err("%s: Failed to open channel\n", __func__);
			goto err;
		}
	}
	ppriv->num_channels = num_channels;
	ppriv->data_cmd_cb = func;
	ppriv->pdev = pdev;
	init_completion(&ppriv->thread_complete);
	mutex_init(&ppriv->pktzr_lock);
	INIT_LIST_HEAD(&ppriv->ch_list);
	ppriv->pktzr_init_complete = true;

done:
	return 0;
err:
	for (i = 0; i < ppriv->token; i++)
		bg_cdc_channel_close(pdev, ppriv->ch_info[i]);

	if (ppriv)
		kzfree(ppriv);
	ppriv = NULL;
	return -EINVAL;
}

void pktzr_deinit(void)
{
	int rc;
	int i;

	if (!ppriv)
		return;
	mutex_lock(&ppriv->pktzr_lock);
	for (i = 0; i < ppriv->num_channels; i++) {
		rc = bg_cdc_channel_close(ppriv->pdev, ppriv->ch_info[i]);
		if (rc)
			pr_err("%s:Failed to close channel\n", __func__);
	}
	mutex_unlock(&ppriv->pktzr_lock);
	reinit_completion(&ppriv->thread_complete);
	mutex_destroy(&ppriv->pktzr_lock);
	kzfree(ppriv);
	ppriv = NULL;
}

