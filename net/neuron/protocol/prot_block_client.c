// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved. */

/* Protocol block client drivers
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
#include <linux/idr.h>
#include <linux/neuron_block.h>
#include "prot_block.h"

#define DRIVER_NAME_CLIENT "neuron-protocol-client-block"

#define C_IN 1
#define C_OUT 0
#define CHANNEL_COUNT 2

#define EV_REQUEST NEURON_BLOCK_CLIENT_EVENT_REQUEST
#define EVENT_COUNT NEURON_BLOCK_CLIENT_EVENT__COUNT

#define CHANNEL_BIT(n) BIT(n)
#define EVENT_BIT(n) BIT(CHANNEL_COUNT + (n))

#define MIN_NEURON_BLOCK_AD_LEN offsetof(struct neuron_block_advertise, label)

struct req_context {
	enum neuron_block_resp_status status;
	enum neuron_block_req_type req_type;
	int req_id;
	u16 flags;
	u32 sectors;
	void *opaque_id;
	off_t offset;
	u64 start_sector;
	struct buffer_list buf;
};

struct client_data {
	struct task_struct *thread;
	wait_queue_head_t wait_q;
	unsigned long wakeups;
	u32 sector_size;
	struct idr req_idr;
};

static const struct of_device_id protocol_block_client_match[] = {
	{
		.compatible = "qcom,neuron-protocol-block",
	},
	{},
};
MODULE_DEVICE_TABLE(of, protocol_block_client_match);

/* Acquire a req id and store its associated context.
 * @return A req id if success, others for failure.
 */
static int acquire_req_id(struct client_data *kdata, struct req_context *ctx)
{
	return idr_alloc(&kdata->req_idr, ctx, 1, INT_MAX, GFP_KERNEL);
}

/* Retrieve req id associated context. */
static int get_req_id_associated(struct client_data *kdata, int req_id,
				 struct req_context **ctx)
{
	struct req_context *context;

	context  = (struct req_context *)idr_find(&kdata->req_idr, req_id);
	if (!context)
		return -ENODEV;
	*ctx = context;
	return 0;
}

static void release_req_id(struct client_data *kdata, int req_id)
{
	idr_remove(&kdata->req_idr, req_id);
}

static int protocol_block_channel_wakeup(struct neuron_protocol *protocol,
					 unsigned int id)
{
	struct client_data *kdata;

	if (WARN_ON_ONCE(id >= CHANNEL_COUNT))
		return -EINVAL;

	kdata = dev_get_drvdata(&protocol->dev);
	set_bit(id, &kdata->wakeups);
	wake_up(&kdata->wait_q);

	return 0;
}

static int protocol_block_app_wakeup(struct neuron_protocol *prot_dev,
				     unsigned int ev)
{
	struct client_data *kdata;

	if (WARN_ON_ONCE(ev >= EVENT_COUNT))
		return -EINVAL;

	kdata = dev_get_drvdata(&prot_dev->dev);
	set_bit(CHANNEL_COUNT + ev, &kdata->wakeups);
	wake_up(&kdata->wait_q);

	return 0;
}

static int protocol_block_client_thread(void *data)
{
	struct neuron_protocol *prot_dev = (struct neuron_protocol *)data;

	struct client_data *kdata = dev_get_drvdata(&prot_dev->dev);

	struct device *dev = &prot_dev->channels[C_OUT]->dev;
	struct neuron_channel *channel_dev = to_neuron_channel(dev);
	struct neuron_channel_driver *channel_drv =
					to_neuron_channel_driver(dev->driver);
	struct device *dev_r = &prot_dev->channels[C_IN]->dev;
	struct neuron_channel *channel_dev_r = to_neuron_channel(dev_r);
	struct neuron_channel_driver *channel_drv_r =
					to_neuron_channel_driver(dev_r->driver);

	struct device *dev_app = &prot_dev->application->dev;
	struct neuron_application *app_dev = to_neuron_application(dev_app);

	struct neuron_app_driver *app_drv =
			to_neuron_app_driver(dev_app->driver);
	struct neuron_block_app_client_driver *block_drv =
		container_of(app_drv,
			     struct neuron_block_app_client_driver, base);

	struct sk_buff *skb_r, *skb, *skb_resp, *skb_req;
	struct req_context *req_ctx = NULL, *resp_ctx = NULL;
	struct neuron_block_resp *resp;
	struct neuron_block_req *req;

	void *opaque_id;
	u16 flags;
	u64 start_sector;
	u32 sectors;
	enum neuron_block_req_type req_type;
	int req_id;

	u32 left;
	u32 to_send;

	struct neuron_block_advertise *ad;
	struct neuron_block_param *param;

	size_t label_size;

	unsigned long wakeup_mask;

	bool req_valid = false;
	bool req_header_sent = false;
	bool resp_valid = false;
	bool resp_ready = false;
	int ret = 0;

	skb_resp = NULL;
	skb_req = NULL;

	/* Wait for the channels to start */
	wakeup_mask = 0;
	while (!kthread_should_stop()) {
		if (!channel_dev->max_size || !channel_dev->queue_length)
			wakeup_mask |= CHANNEL_BIT(C_IN);

		if (!channel_dev_r->max_size || !channel_dev_r->queue_length)
			wakeup_mask |= CHANNEL_BIT(C_OUT);

		if (!wakeup_mask)
			break;

		wait_event_killable(kdata->wait_q,
				    (kdata->wakeups &&
				    channel_dev->max_size &&
				    channel_dev->queue_length &&
				    channel_dev_r->max_size &&
				    channel_dev_r->queue_length) ||
				    kthread_should_stop());
		wakeup_mask = 0;
	}
	if (kthread_should_stop())
		return 0;

	WARN_ON(channel_dev_r->max_size <=
		sizeof(struct neuron_block_advertise));

	skb_r = alloc_skb(channel_dev_r->max_size, GFP_KERNEL);
	if (!skb_r)
		return -ENOMEM;

	skb_put(skb_r, channel_dev_r->max_size);
	/* Wait and get block params */
	do {
		clear_bit(C_IN, &kdata->wakeups);
		/* flush shared variable to memory */
		smp_mb__after_atomic();

		ret = channel_drv_r->receive_msg(channel_dev_r, skb_r);
		if (ret == -EAGAIN) {
			wait_event_killable(kdata->wait_q,
					    kthread_should_stop() ||
					    (kdata->wakeups &
					     CHANNEL_BIT(C_IN)));
		}

	} while (ret == -EAGAIN);

	if (ret <= 0) {
		pr_err("Receiving Block param failed with error %d\n", ret);
		kfree_skb(skb_r);
		return ret;
	}

	if (ret < MIN_NEURON_BLOCK_AD_LEN) {
		pr_err("Invalid block param length\n");
		kfree_skb(skb_r);
		return -EINVAL;
	}

	label_size = ret - MIN_NEURON_BLOCK_AD_LEN;

	ad = (struct neuron_block_advertise *)skb_r->data;

	/* Allocate 1 more byte to hold '\0' in case label is not
	 * NULL terminated.
	 */
	param = kzalloc(sizeof(*param) + label_size + 1, GFP_KERNEL);
	if (!param) {
		consume_skb(skb_r);
		return -ENOMEM;
	}

	/* Save logical block size */
	kdata->sector_size = ad->logical_block_size;

	param->logical_block_size = ad->logical_block_size;
	WARN_ON(param->logical_block_size > channel_dev->max_size);
	param->physical_block_size = ad->physical_block_size;
	param->num_device_sectors = ad->num_device_sectors;
	param->discard_max_hw_sectors = ad->discard_max_hw_sectors;
	param->discard_max_sectors = ad->discard_max_sectors;
	param->discard_granularity = ad->discard_granularity;
	param->alignment_offset = ad->alignment_offset;
	param->read_only = ad->flags & NEURON_BLOCK_READONLY;
	param->discard_zeroes_data = ad->flags & NEURON_BLOCK_DISCARD_ZEROES;
	param->wc_flag = ad->wc_flag;
	param->fua_flag = ad->fua_flag;
	memcpy(param->uuid.b, ad->uuid, sizeof(param->uuid.b));
	strlcpy(param->label, ad->label, label_size);
	/* Ensure it is NULL terminated always. */
	param->label[label_size] = '\0';

	consume_skb(skb_r);

	/* Set block device params. */
	block_drv->do_set_bd_params(app_dev, param);

	/* Pre-allocate a request SKB. This will be reallocated after each
	 * successful send; we can't reuse it in case the channel driver has
	 * cloned it.
	 */
	skb_req = alloc_skb(sizeof(*req), GFP_KERNEL);
	if (!skb_req)
		return -ENOMEM;
	req = (struct neuron_block_req *)skb_put(skb_req, sizeof(*req));

	/* Pre-allocate the response SKB. This can be safely reused
	 * because it is never sent to any asynchronous function outside
	 * this driver.
	 */
	skb_resp = __alloc_skb(sizeof(*resp), GFP_KERNEL, SKB_ALLOC_RX,
			       NUMA_NO_NODE);
	if (!skb_resp) {
		kfree_skb(skb_req);
		return -ENOMEM;
	}
	resp = (struct neuron_block_resp *)skb_put(skb_resp, sizeof(*resp));

	/* Main request handling loop */
	wakeup_mask = 0;
	while (!kthread_should_stop()) {
		unsigned long old_wakeups, consumed_wakeups;

		/*Try to get request from client*/
		if (!req_valid && !(wakeup_mask & EVENT_BIT(EV_REQUEST))) {
			ret = block_drv->get_request(app_dev, &opaque_id,
						     &req_type, &flags,
						     &start_sector,
						     &sectors, &skb);
			if (ret == -EAGAIN) {
				wakeup_mask |= EVENT_BIT(EV_REQUEST);
				continue;
			} else if (ret < 0) {
				dev_err(&prot_dev->dev,
					"get_request returned %d\n", ret);
				goto fail;
			}
			/* Create request context */
			req_ctx = kzalloc(sizeof(*req_ctx), GFP_KERNEL);
			if (!req_ctx) {
				ret = -ENOMEM;
				goto fail;
			}
			req_ctx->opaque_id = opaque_id;
			req_ctx->req_type = req_type;
			req_ctx->flags = flags;
			req_ctx->start_sector = start_sector;
			req_ctx->sectors = sectors;
			req_ctx->buf.head = skb;
			req_ctx->buf.offset = 0;

			/* Acquire req id */
			req_id = acquire_req_id(kdata, req_ctx);
			if (req_id < 0) {
				dev_err(&prot_dev->dev,
					"Can't acquire req id: %d\n", req_id);
				ret = req_id;
				kfree(req_ctx);
				goto fail;
			}
			req_ctx->req_id	= req_id;

			dev_dbg(&prot_dev->dev, "Getting request: req_id =%d, req_type =%d, sectors = %d\n",
				req_id, req_type, sectors);

			req_valid = true;
			req_header_sent = false;
			continue;
		}

		/* If we have a request header, try to send it. */
		if (req_valid && !req_header_sent &&
		    !(wakeup_mask & CHANNEL_BIT(C_OUT))) {
			WARN_ON(!req_ctx);
			req->req_id = req_ctx->req_id;
			req->req_type = req_ctx->req_type;
			req->flags = req_ctx->flags;
			req->start_sector = req_ctx->start_sector;
			req->sectors = req_ctx->sectors;

			ret = channel_drv->send_msg(channel_dev, skb_req);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_OUT);
				continue;
			} else if (ret < 0) {
				dev_err(&prot_dev->dev,
					"Can't send request: %d\n",
					ret);
				goto fail;
			}

			dev_dbg(&prot_dev->dev, "Sending req header for req_id = %d, req_type = %d\n",
				req->req_id, req->req_type);
			/* Consume and replace the request skb. */
			consume_skb(skb_req);
			skb_req = alloc_skb(sizeof(*req), GFP_KERNEL);
			if (!skb_req) {
				kfree_skb(skb_resp);
				return -ENOMEM;
			}
			req = (struct neuron_block_req *)skb_put(skb_req,
								 sizeof(*req));
			memset(req, 0, sizeof(*req));

			/* Is there any data to send? */
			if (req_ctx->sectors &&
			    (req_ctx->req_type == NEURON_BLOCK_REQUEST_WRITE ||
			    req_ctx->req_type ==
			    NEURON_BLOCK_REQUEST_WRITE_SAME))
				req_header_sent = true;
			else
				req_valid = false;

			continue;
		}

		/* If we have a request data, try to send it. */
		if (req_valid && req_header_sent &&
		    !(wakeup_mask & CHANNEL_BIT(C_OUT))) {
			left = req_ctx->buf.head->len - req_ctx->buf.offset;
			WARN_ON(left % kdata->sector_size);
			to_send = min_t(u32, left, channel_dev->max_size);
			to_send = round_down(to_send, kdata->sector_size);
			req_ctx->buf.size = to_send;
			ret = channel_drv->send_msgv(channel_dev, req_ctx->buf);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_OUT);
				continue;
			} else if (ret < 0) {
				dev_err(&prot_dev->dev,
					"Sending request data failed %d\n",
					ret);
				goto fail;
			}
			req_ctx->buf.offset += to_send;
			if (req_ctx->buf.offset == req_ctx->buf.head->len) {
				req_valid = false;
				req_header_sent = false;
			}

			dev_dbg(&prot_dev->dev, "Sending req data for req_id = %d with %d bytes\n",
				req_ctx->req_id, to_send);

			continue;
		}

		/* Try to obtain a response from the server. */
		if (!resp_valid && !(wakeup_mask & CHANNEL_BIT(C_IN))) {
			ret = channel_drv_r->receive_msg(channel_dev_r,
							 skb_resp);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_IN);
				continue;
			} else if (ret < 0) {
				dev_err(&prot_dev->dev, "receive_msg for resp header returned %d\n",
					ret);
				goto fail;
			} else if (skb_resp->len < sizeof(*resp)) {
				dev_err(&prot_dev->dev, "Truncated response\n");
				ret = -EBADMSG;
				goto fail;
			}

			if (get_req_id_associated(kdata, resp->resp_id,
						  &resp_ctx)) {
				dev_err(&prot_dev->dev, "Incorrect response id %d\n",
					resp->resp_id);
				ret = -EBADMSG;
				goto fail;
			}
			resp_ctx->status = resp->resp_status;
			release_req_id(kdata, resp->resp_id);

			dev_dbg(&prot_dev->dev, "Receiving response for req_id %d\n",
				resp->resp_id);

			resp_valid = true;
			if (!resp->resp_status &&
			    resp_ctx->req_type == NEURON_BLOCK_REQUEST_READ &&
			    resp_ctx->sectors) {
				WARN_ON(!resp_ctx->buf.head);
				resp_ctx->buf.offset = 0;
				resp_ctx->buf.size = resp_ctx->sectors *
						     kdata->sector_size;
				resp_ready = false;
			} else {
				resp_ready = true;
			}
			continue;
		}

		/* Try to receive response data if we are waiting for any. */
		if (resp_valid && !resp_ready &&
		    !(wakeup_mask & CHANNEL_BIT(C_IN))) {
			if (WARN_ON(!resp_ctx))
				goto fail;
			ret = channel_drv_r->receive_msgv(channel_dev_r,
							 resp_ctx->buf);
			if (ret == -EAGAIN) {
				wakeup_mask |= CHANNEL_BIT(C_IN);
				continue;
			} else if (ret < 0) {
				dev_err(&prot_dev->dev,
					"receive_msg for resp header returned %d\n",
					ret);
				goto fail;
			}

			dev_dbg(&prot_dev->dev,
				"Receiving response data for req_id %d with %d bytes\n",
				resp_ctx->req_id, ret);

			resp_ctx->buf.offset += ret;
			resp_ctx->buf.size -= ret;
			if (resp_ctx->buf.size == 0)
				resp_ready = true;
			continue;
		}

		/* Deal with response if the response is ready. */
		if (resp_ready) {
			if (WARN_ON(!resp_ctx))
				goto fail;
			dev_dbg(&prot_dev->dev, "Response is done for req %d\n",
				resp_ctx->req_id);
			resp_ready = false;
			resp_valid = false;
			consume_skb(resp_ctx->buf.head);
			block_drv->do_response(app_dev, resp_ctx->opaque_id,
					       resp_ctx->status);
			kfree(resp_ctx);
			continue;
		}

		if (WARN_ON_ONCE(!wakeup_mask))
			continue;

		dev_dbg(&prot_dev->dev, "wait for events %#lx (now: %#lx)\n",
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

fail:
	kfree_skb(skb_req);
	kfree_skb(skb_resp);

	return ret;
}

static int protocol_block_client_probe(struct neuron_protocol *prot_dev)
{
	struct client_data *kdata;

	kdata = kzalloc(sizeof(*kdata), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	init_waitqueue_head(&kdata->wait_q);

	idr_init(&kdata->req_idr);

	dev_set_drvdata(&prot_dev->dev, kdata);

	kdata->thread = kthread_run(protocol_block_client_thread, prot_dev,
				    "%s", prot_dev->dev.driver->name);
	return 0;
}

static void protocol_block_client_remove(struct neuron_protocol *prot_dev)
{
	struct client_data *kdata;

	kdata = dev_get_drvdata(&prot_dev->dev);
	kthread_stop(kdata->thread);
	dev_set_drvdata(&prot_dev->dev, NULL);
	kfree(kdata);
}

static const struct neuron_channel_match_table channels_client_block[] = {
	{NEURON_CHANNEL_MESSAGE_QUEUE, NEURON_CHANNEL_SEND},
	{NEURON_CHANNEL_MESSAGE_QUEUE, NEURON_CHANNEL_RECEIVE}
};

static const char * const processes_client_block[] = {"client"};

struct neuron_protocol_driver protocol_client_block_driver = {
	.channel_count = 2,
	.channels = channels_client_block,
	.process_count = 1,
	.processes = processes_client_block,
	.driver = {
		.name = DRIVER_NAME_CLIENT,
		.owner = THIS_MODULE,
		.of_match_table = protocol_block_client_match,
	},
	.probe  = protocol_block_client_probe,
	.remove = protocol_block_client_remove,
	.channel_wakeup = protocol_block_channel_wakeup,
	.app_wakeup = protocol_block_app_wakeup,
};
EXPORT_SYMBOL(protocol_client_block_driver);

static int __init protocol_client_block_init(void)
{
	int ret;

	ret = neuron_register_protocol_driver(&protocol_client_block_driver);
	if (ret < 0) {
		pr_err("Failed to register driver\n");
		return ret;
	}
	return 0;
}

static void __exit protocol_client_block_exit(void)
{
	neuron_unregister_protocol_driver(&protocol_client_block_driver);
}

module_init(protocol_client_block_init);
module_exit(protocol_client_block_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Protocol block client drivers");
