// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Protocol block server drivers
 *
 * This driver receives data from the application layer and sends it to the
 * channel layer and vice versa. Receives from channel and sends to app.
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/neuron.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/neuron_block.h>
#include "prot_block.h"

#define DRIVER_NAME_SERVER "neuron-protocol-server-block"

#define C_IN 0
#define C_OUT 1
#define CHANNEL_COUNT 2

#define EV_BD_PARAMS NEURON_BLOCK_SERVER_EVENT_BD_PARAMS
#define EV_RESPONSE NEURON_BLOCK_SERVER_EVENT_RESPONSE
#define EVENT_COUNT NEURON_BLOCK_SERVER_EVENT__COUNT

#define CHANNEL_BIT(n) BIT(n)
#define EVENT_BIT(n) BIT(CHANNEL_COUNT + (n))

struct server_data {
	struct task_struct *thread;
	wait_queue_head_t wait_q;
	unsigned long wakeups;
};

static const struct of_device_id protocol_block_server_match[] = {
	{
		.compatible = "qcom,neuron-protocol-block",
	},
	{},
};
MODULE_DEVICE_TABLE(of, protocol_block_server_match);

static int protocol_block_channel_wakeup(struct neuron_protocol *prot,
					 unsigned int id)
{
	struct server_data *kdata;

	if (WARN_ON_ONCE(id >= CHANNEL_COUNT))
		return -EINVAL;

	kdata = dev_get_drvdata(&prot->dev);
	set_bit(id, &kdata->wakeups);
	dev_dbg(&prot->dev, "wake ch %d (now: %#lx)\n", id, kdata->wakeups);
	wake_up(&kdata->wait_q);

	return 0;
}

static int protocol_block_app_wakeup(struct neuron_protocol *prot,
				     unsigned int ev)
{
	struct server_data *kdata;

	if (WARN_ON_ONCE(ev >= EVENT_COUNT))
		return -EINVAL;

	kdata = dev_get_drvdata(&prot->dev);
	set_bit(CHANNEL_COUNT + ev, &kdata->wakeups);
	dev_dbg(&prot->dev, "wake ev %d (now: %#lx)\n", ev, kdata->wakeups);
	wake_up(&kdata->wait_q);

	return 0;
}

static int handle_request(struct neuron_block_app_server_driver *block_drv,
			  struct neuron_application *app_dev,
			  struct neuron_block_req *req,
			  struct sk_buff *skb_data)
{
	switch (req->req_type) {
	case NEURON_BLOCK_REQUEST_READ:
		WARN_ON(skb_data);
		return block_drv->do_read(app_dev, req->req_id,
					  req->start_sector, req->sectors,
					  req->flags);
	case NEURON_BLOCK_REQUEST_WRITE:
		return block_drv->do_write(app_dev, req->req_id,
					   req->start_sector, req->sectors,
					   req->flags, skb_data);
	case NEURON_BLOCK_REQUEST_DISCARD:
		WARN_ON(skb_data);
		return block_drv->do_discard(app_dev, req->req_id,
					     req->start_sector, req->sectors,
					     req->flags);
	case NEURON_BLOCK_REQUEST_SECURE_ERASE:
		WARN_ON(skb_data);
		return block_drv->do_secure_erase(app_dev, req->req_id,
						  req->start_sector,
						  req->sectors, req->flags);
	case NEURON_BLOCK_REQUEST_WRITE_SAME:
		return block_drv->do_write_same(app_dev, req->req_id,
						req->start_sector,
						req->sectors, req->flags,
						skb_data);
	/* fall through return */
	case NEURON_BLOCK_REQUEST_WRITE_ZEROES:
		WARN_ON(skb_data);
		return block_drv->do_write_zeroes(app_dev, req->req_id,
						  req->start_sector,
						  req->sectors, req->flags);
	default:
		pr_err("Wrong req type: %d\n", req->req_type);
		if (skb_data)
			kfree_skb(skb_data);
		return -EINVAL;
	}
}

static int protocol_block_server_thread(void *data)
{
	struct neuron_protocol *prot = (struct neuron_protocol *)data;
	struct neuron_channel *c_in = prot->channels[C_IN];
	struct neuron_channel_driver *c_in_drv =
		to_neuron_channel_driver(c_in->dev.driver);
	struct neuron_channel *c_out = prot->channels[C_OUT];
	struct neuron_channel_driver *c_out_drv =
		to_neuron_channel_driver(c_out->dev.driver);
	struct neuron_application *app = prot->application;
	struct neuron_app_driver *app_drv =
		to_neuron_app_driver(app->dev.driver);
	struct neuron_block_app_server_driver *block_drv =
		container_of(app_drv,
			     struct neuron_block_app_server_driver,
			     base);

	struct server_data *kdata = dev_get_drvdata(&prot->dev);

	int ret;
	size_t size;

	struct sk_buff *skb_ad = NULL;
	struct neuron_block_advertise *ad;
	const struct neuron_block_param *param;

	size_t sector_size;

	unsigned long wakeup_mask;

	bool req_valid = false;
	bool req_ready = false;
	struct neuron_block_req *req;
	struct sk_buff *skb_req = NULL;
	struct sk_buff *skb_req_data = NULL;
	size_t req_data_pos = 0;

	bool resp_valid = false;
	bool resp_sent_header = false;
	struct neuron_block_resp *resp;
	struct sk_buff *skb_resp = NULL;
	struct sk_buff *skb_resp_data = NULL;
	size_t resp_data_pos = 0;

	kdata->wakeups = 0;

	/* Wait for the channels to start */
	wakeup_mask = 0;
	while (!kthread_should_stop()) {
		if (!c_in->max_size || !c_in->queue_length)
			wakeup_mask |= CHANNEL_BIT(C_IN);

		if (!c_out->max_size || !c_out->queue_length)
			wakeup_mask |= CHANNEL_BIT(C_OUT);

		if (!wakeup_mask)
			break;

		wait_event_killable(kdata->wait_q,
				    kthread_should_stop() ||
				    (kdata->wakeups & wakeup_mask));

		wakeup_mask &= ~xchg(&kdata->wakeups, 0);
		/* flush shared variable to memory */
		smp_mb__after_atomic();
	}
	if (kthread_should_stop())
		return 0;

	/* Wait for the block params to be ready */
	while (!kthread_should_stop()) {
		clear_bit(EVENT_BIT(EV_BD_PARAMS), &kdata->wakeups);
		/* flush shared variable to memory */
		smp_mb__after_atomic();

		ret = block_drv->get_bd_params(app, &param);
		if (ret == 0) {
			break;
		} else if (ret != -EAGAIN) {
			dev_err(&prot->dev, "Failed to get params: %d\n", ret);
			return ret;
		}

		wait_event_killable(kdata->wait_q,
				    kthread_should_stop() ||
				    (kdata->wakeups & EVENT_BIT(EV_BD_PARAMS)));
	}
	if (kthread_should_stop())
		return 0;

	/* Save the logical sector size; the protocol will need it later */
	sector_size = param->logical_block_size;

	size = sizeof(*ad) + strlen(param->label);

	skb_ad = alloc_skb(ret, GFP_KERNEL);
	ad = (struct neuron_block_advertise *)skb_put(skb_ad, sizeof(*ad));
	memset(ad, 0, sizeof(*ad));
	ad->logical_block_size = param->logical_block_size;
	ad->physical_block_size = param->physical_block_size;
	ad->num_device_sectors = param->num_device_sectors;
	ad->discard_max_hw_sectors = param->discard_max_hw_sectors;
	ad->discard_max_sectors = param->discard_max_sectors;
	ad->discard_granularity = param->discard_granularity;
	ad->alignment_offset = param->alignment_offset;
	ad->wc_flag = param->wc_flag;
	ad->fua_flag = param->fua_flag;
	memcpy(ad->uuid, param->uuid.b, sizeof(param->uuid.b));
	strlcpy(ad->label, param->label, strlen(param->label));
	if (param->read_only)
		ad->flags |= NEURON_BLOCK_READONLY;
	if (param->discard_zeroes_data)
		ad->flags |= NEURON_BLOCK_DISCARD_ZEROES;
	if (param->logical_block_size > c_in->max_size)
		return -EPROTO;
	if (param->logical_block_size > c_out->max_size)
		return -EPROTO;
	kfree(param);

	while (!kthread_should_stop()) {
		clear_bit(CHANNEL_BIT(C_OUT), &kdata->wakeups);
		/* flush shared variable to memory */
		smp_mb__after_atomic();

		ret = c_out_drv->send_msg(c_out, skb_ad);
		if (ret == 0) {
			break;
		} else if (ret != -EAGAIN) {
			dev_err(&prot->dev, "Failed to send params: %d\n", ret);
			kfree_skb(skb_ad);
			return ret;
		}
		wait_event_killable(kdata->wait_q,
				    kthread_should_stop() ||
				    (kdata->wakeups & CHANNEL_BIT(C_OUT)));
	}
	consume_skb(skb_ad);
	if (kthread_should_stop())
		return 0;

	/* Pre-allocate the request SKB. This can be safely reused
	 * because it is never sent to any asynchronous function outside
	 * this driver.
	 */
	skb_req = __alloc_skb(sizeof(*req), GFP_KERNEL, SKB_ALLOC_RX,
			      NUMA_NO_NODE);
	if (!skb_req) {
		ret = -ENOMEM;
		goto fail;
	}
	req = (struct neuron_block_req *)skb_put(skb_req, sizeof(*req));

	/* Pre-allocate a response SKB. This will be reallocated after each
	 * successful send; we can't reuse it in case the channel driver has
	 * cloned it.
	 */
	skb_resp = alloc_skb(sizeof(*resp), GFP_KERNEL);
	if (!skb_resp) {
		ret = -ENOMEM;
		goto fail;
	}
	resp = (struct neuron_block_resp *)skb_put(skb_resp, sizeof(*resp));
	memset(resp, 0, sizeof(*resp));

	/* Main request handling loop */
	wakeup_mask = 0;
	while (!kthread_should_stop()) {
		unsigned long old_wakeups, consumed_wakeups;

		/* Try to obtain a request if we don't have one */
		if (!req_valid && !(wakeup_mask & CHANNEL_BIT(C_IN))) {
			size_t size;

			/* New request */
			WARN_ON(skb_req_data);

			ret = c_in_drv->receive_msg(c_in, skb_req);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_IN);
				continue;
			} else if (ret < 0) {
				dev_err(&prot->dev,
					"%d Recv error %d\n", __LINE__, ret);
				goto fail;
			} else if (ret < sizeof(*req)) {
				dev_err(&prot->dev, "Truncated request\n");
				ret = -EBADMSG;
				goto fail;
			}
			req_valid = true;

			if (req->req_type == NEURON_BLOCK_REQUEST_WRITE)
				size = req->sectors * sector_size;
			else if (req->req_type ==
					NEURON_BLOCK_REQUEST_WRITE_SAME)
				size = sector_size;
			else
				size = 0;

			if (size > 0) {
				WARN_ON(skb_req_data);
				skb_req_data = neuron_alloc_pskb(size,
								 GFP_KERNEL);
				if (IS_ERR(skb_req_data)) {
					ret = PTR_ERR(skb_req_data);
					goto fail;
				}
				req_data_pos = 0;
				req_ready = false;
			} else {
				req_ready = true;
			}
			continue;
		}

		/* Try to receive request data if we are waiting for any */
		if (req_valid && !req_ready &&
		    !(wakeup_mask & CHANNEL_BIT(C_IN))) {
			struct buffer_list buf;

			WARN_ON(!skb_req_data);

			buf.head = skb_req_data;
			buf.offset = req_data_pos;
			buf.size = min_t(size_t, c_in->max_size,
					 skb_req_data->len - req_data_pos);
			buf.size = round_down(buf.size, sector_size);

			WARN_ON(!buf.size);

			ret = c_in_drv->receive_msgv(c_in, buf);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_IN);
				continue;
			} else if (ret < 0) {
				dev_err(&prot->dev,
					"%d Recv error %d\n", __LINE__, ret);
				goto fail;
			} else if (!ret) {
				dev_err(&prot->dev, "Empty payload\n");
				ret = -EBADMSG;
				goto fail;
			} else if (ret % sector_size) {
				dev_err(&prot->dev, "Bad payload length %d\n",
					ret);
				ret = -EBADMSG;
				goto fail;
			}
			req_data_pos += ret;

			if (req_data_pos == skb_req_data->len)
				req_ready = true;
			continue;
		}

		/* Try to handle a request if there is one ready */
		if (req_valid && req_ready) {
			ret = handle_request(block_drv, app, req, skb_req_data);
			skb_req_data = NULL;

			if (ret < 0) {
				dev_err(&prot->dev,
					"Request failed: %d\n", ret);
				ret = -EBADMSG;
				goto fail;
			}

			req_data_pos = 0;
			req_ready = false;
			req_valid = false;
			continue;
		}

		/* Try to obtain a response from the application layer. */
		if (!resp_valid && !(wakeup_mask & EVENT_BIT(EV_RESPONSE))) {
			enum neuron_block_resp_status resp_status;

			WARN_ON(skb_resp_data);

			ret = block_drv->get_response(app, &resp->resp_id,
					&resp_status, &skb_resp_data);

			if (ret == -EAGAIN) {
				wakeup_mask |= EVENT_BIT(EV_RESPONSE);
				continue;
			} else if (ret < 0) {
				dev_err(&prot->dev, "Can't get response: %d\n",
					ret);
				goto fail;
			}

			resp->resp_status = (u16)resp_status;
			resp_valid = true;
			resp_sent_header = false;
			resp_data_pos = 0;
			continue;
		}

		/* If we have a response header, try to send it. */
		if (resp_valid && !resp_sent_header &&
		    !(wakeup_mask & CHANNEL_BIT(C_OUT))) {
			ret = c_out_drv->send_msg(c_out, skb_resp);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_OUT);
				continue;
			} else if (ret < 0) {
				dev_err(&prot->dev, "Can't send response: %d\n",
					ret);
				goto fail;
			}

			/* Consume and replace the response skb */
			consume_skb(skb_resp);
			skb_resp = alloc_skb(sizeof(*resp), GFP_KERNEL);
			if (!skb_resp) {
				ret = -ENOMEM;
				goto fail;
			}
			resp = (struct neuron_block_resp *)skb_put(skb_resp,
					sizeof(*resp));
			memset(resp, 0, sizeof(*resp));

			/* Is there any data to send? */
			if (skb_resp_data)
				resp_sent_header = true;
			else
				resp_valid = false;
			continue;
		}

		/* If we have response data, try to send it. */
		if (resp_valid && resp_sent_header &&
		    !(wakeup_mask & CHANNEL_BIT(C_OUT))) {
			struct buffer_list buf;

			WARN_ON(!skb_resp_data);

			buf.head = skb_resp_data;
			buf.offset = resp_data_pos;
			buf.size = min_t(size_t, c_out->max_size,
					 skb_resp_data->len - resp_data_pos);
			buf.size = round_down(buf.size, sector_size);

			WARN_ON(!buf.size);

			ret = c_out_drv->send_msgv(c_out, buf);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_OUT);
				continue;
			} else if (ret < 0) {
				dev_err(&prot->dev, "Send error %d\n", ret);
				goto fail;
			}
			resp_data_pos += buf.size;

			WARN_ON(resp_data_pos > skb_resp_data->len);

			if (resp_data_pos == skb_resp_data->len) {
				resp_valid = false;
				consume_skb(skb_resp_data);
				skb_resp_data = NULL;
			}
			continue;
		}

		if (WARN_ON_ONCE(!wakeup_mask))
			continue;

		dev_dbg(&prot->dev, "wait for events %#lx (now: %#lx)\n",
			wakeup_mask, kdata->wakeups);
		wait_event_killable(kdata->wait_q,
				    kthread_should_stop() ||
				    (kdata->wakeups & wakeup_mask));

		do {
			old_wakeups = READ_ONCE(kdata->wakeups);
			consumed_wakeups = old_wakeups & wakeup_mask;
		} while (cmpxchg(&kdata->wakeups, old_wakeups,
					old_wakeups & ~consumed_wakeups) !=
				old_wakeups);
		/* flush shared variable to memory */
		smp_mb__after_atomic();
		wakeup_mask &= ~consumed_wakeups;
	}

	consume_skb(skb_req);
	consume_skb(skb_resp);

	return 0;

fail:
	if (skb_req)
		kfree_skb(skb_req);
	if (skb_req_data)
		kfree_skb(skb_req_data);
	if (skb_resp)
		kfree_skb(skb_resp);
	if (skb_resp_data)
		kfree_skb(skb_resp_data);

	return ret;
}

static int protocol_block_server_probe(struct neuron_protocol *protocol_dev)
{
	struct server_data *kdata;

	kdata = kzalloc(sizeof(*kdata), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	init_waitqueue_head(&kdata->wait_q);
	kdata->wakeups = 0;

	dev_set_drvdata(&protocol_dev->dev, kdata);

	kdata->thread = kthread_run(protocol_block_server_thread, protocol_dev,
				    "%s", dev_name(&protocol_dev->dev));

	return 0;
}

static void protocol_block_server_remove(struct neuron_protocol *protocol_dev)
{
	struct server_data *kdata;

	kdata = dev_get_drvdata(&protocol_dev->dev);
	kthread_stop(kdata->thread);
	dev_set_drvdata(&protocol_dev->dev, NULL);
	kfree(kdata);
}

static const struct neuron_channel_match_table channels_server_block[] = {
	[C_IN] = {NEURON_CHANNEL_MESSAGE_QUEUE, NEURON_CHANNEL_RECEIVE},
	[C_OUT] = {NEURON_CHANNEL_MESSAGE_QUEUE, NEURON_CHANNEL_SEND},
};

static const char * const processes_server_block[] = {"server"};

struct neuron_protocol_driver protocol_server_block_driver = {
	.channel_count = CHANNEL_COUNT,
	.channels = channels_server_block,
	.process_count = 1,
	.processes = processes_server_block,
	.driver = {
		.name = DRIVER_NAME_SERVER,
		.owner = THIS_MODULE,
		.of_match_table = protocol_block_server_match,
	},
	.probe  = protocol_block_server_probe,
	.remove = protocol_block_server_remove,
	.channel_wakeup = protocol_block_channel_wakeup,
	.app_wakeup = protocol_block_app_wakeup,
};
EXPORT_SYMBOL(protocol_server_block_driver);

static int __init protocol_server_block_init(void)
{
	int ret;

	ret = neuron_register_protocol_driver(&protocol_server_block_driver);
	if (ret < 0) {
		pr_err("Failed to register driver\n");
		return ret;
	}
	return 0;
}

static void __exit protocol_server_block_exit(void)
{
	neuron_unregister_protocol_driver(&protocol_server_block_driver);
}

module_init(protocol_server_block_init);
module_exit(protocol_server_block_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Neuron block server protocol driver");
