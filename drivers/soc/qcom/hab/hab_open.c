/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include "hab.h"

void hab_open_request_init(struct hab_open_request *request,
		int type,
		struct physical_channel *pchan,
		int vchan_id,
		int sub_id,
		int open_id)
{
	request->type = type;
	request->pchan = pchan;
	request->vchan_id = vchan_id;
	request->sub_id = sub_id;
	request->open_id = open_id;
}

int hab_open_request_send(struct hab_open_request *request)
{
	struct hab_header header = HAB_HEADER_INITIALIZER;
	struct hab_open_send_data data;

	HAB_HEADER_SET_SIZE(header, sizeof(struct hab_open_send_data));
	HAB_HEADER_SET_TYPE(header, request->type);

	data.vchan_id = request->vchan_id;
	data.open_id = request->open_id;
	data.sub_id = request->sub_id;

	return physical_channel_send(request->pchan, &header, &data);
}

int hab_open_request_add(struct physical_channel *pchan,
		struct hab_header *header)
{
	struct hab_open_node *node;
	struct hab_device *dev = pchan->habdev;
	struct hab_open_send_data data;
	struct hab_open_request *request;

	node = kzalloc(sizeof(*node), GFP_ATOMIC);
	if (!node)
		return -ENOMEM;

	if (physical_channel_read(pchan, &data, HAB_HEADER_GET_SIZE(*header)) !=
		HAB_HEADER_GET_SIZE(*header))
		return -EIO;

	request = &node->request;
	request->type     = HAB_HEADER_GET_TYPE(*header);
	request->pchan    = pchan;
	request->vchan_id = data.vchan_id;
	request->sub_id   = data.sub_id;
	request->open_id  = data.open_id;
	node->age = 0;
	hab_pchan_get(pchan);

	spin_lock_bh(&dev->openlock);
	list_add_tail(&node->node, &dev->openq_list);
	spin_unlock_bh(&dev->openlock);

	return 0;
}

static int hab_open_request_find(struct uhab_context *ctx,
		struct hab_device *dev,
		struct hab_open_request *listen,
		struct hab_open_request **recv_request)
{
	struct hab_open_node *node, *tmp;
	struct hab_open_request *request;
	int ret = 0;

	if (ctx->closing ||
		(listen->pchan && listen->pchan->closed)) {
		*recv_request = NULL;
		return 1;
	}

	spin_lock_bh(&dev->openlock);
	if (list_empty(&dev->openq_list))
		goto done;

	list_for_each_entry_safe(node, tmp, &dev->openq_list, node) {
		request = (struct hab_open_request *)node;
		if  (request->type == listen->type &&
			(request->sub_id == listen->sub_id) &&
			(!listen->open_id ||
			request->open_id == listen->open_id) &&
			(!listen->pchan   ||
			request->pchan == listen->pchan)) {
			list_del(&node->node);
			*recv_request = request;
			ret = 1;
			break;
		}
		node->age++;
		if (node->age > Q_AGE_THRESHOLD) {
			list_del(&node->node);
			hab_open_request_free(request);
		}
	}

done:
	spin_unlock_bh(&dev->openlock);
	return ret;
}

void hab_open_request_free(struct hab_open_request *request)
{
	if (request) {
		hab_pchan_put(request->pchan);
		kfree(request);
	}
}

int hab_open_listen(struct uhab_context *ctx,
		struct hab_device *dev,
		struct hab_open_request *listen,
		struct hab_open_request **recv_request,
		int ms_timeout)
{
	int ret = 0;

	if (!ctx || !listen || !recv_request)
		return -EINVAL;

	*recv_request = NULL;
	if (ms_timeout > 0) {
		ret = wait_event_interruptible_timeout(dev->openq,
			hab_open_request_find(ctx, dev, listen, recv_request),
			ms_timeout);
		if (!ret || (-ERESTARTSYS == ret))
			ret = -EAGAIN;
		else if (ret > 0)
			ret = 0;
	} else {
		ret = wait_event_interruptible(dev->openq,
			hab_open_request_find(ctx, dev, listen, recv_request));
	}

	return ret;
}
