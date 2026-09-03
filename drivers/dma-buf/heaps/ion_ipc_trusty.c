// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/trusty/trusty_ipc.h>
#include "ion_debug.h"
#include "ion_ipc_trusty.h"

#define ION_TEE_PORT "com.android.trusty.ion"

struct ion_tipc_ctx {
	int state;
	struct mutex lock; /* handle message */
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct list_head rx_msg_queue;
};

struct ion_tipc_ctx *ion_tipc;

struct tipc_msg_buf *ion_handle_msg(void *data,	struct tipc_msg_buf *rxbuf, u16 flags)
{
	struct ion_tipc_ctx *dn = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(dn->chan);
		if (newbuf) {
			ION_DEBUG("received new data, rxbuf %p, newbuf %p\n", rxbuf, newbuf);
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &dn->rx_msg_queue);
			wake_up_interruptible(&dn->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			ION_INFO("discard incoming message\n");

			newbuf = rxbuf;
		}
	}
	mutex_unlock(&dn->lock);

	return newbuf;
}

static void ion_handle_event(void *data, int event)
{
	struct ion_tipc_ctx *dn = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		ION_INFO("ion channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		ION_INFO("ion channel disconnected\n");
		break;

	case TIPC_CHANNEL_CONNECTED:
		ION_INFO("sp channel connected\n");
		dn->state = TIPC_CHANNEL_CONNECTED;
		break;

	default:
		ION_ERR("%s: ion unhandled event %d\n", __func__, event);
		break;
	}
}

static struct tipc_chan_ops ion_chan_ops = {
	.handle_msg = ion_handle_msg,
	.handle_event = ion_handle_event,
};

static struct ion_tipc_ctx *ion_tipc_connect(char *srv_name)
{
	struct ion_tipc_ctx *tipc;
	struct tipc_chan *chan;
	int ret = 0;

	tipc = kzalloc(sizeof(*tipc), GFP_KERNEL);
	if (!tipc)
		goto err_alloc_dn;

	mutex_init(&tipc->lock);
	init_waitqueue_head(&tipc->readq);
	INIT_LIST_HEAD(&tipc->rx_msg_queue);

	tipc->state = TIPC_CHANNEL_DISCONNECTED;
	chan = tipc_create_channel(NULL, &ion_chan_ops, tipc);
	if (IS_ERR(chan)) {
		ret = PTR_ERR(chan);
		ION_ERR("ion create channel failed\n");
		goto err_create_chan;
	}

	tipc->chan = chan;

	tipc_chan_connect(chan, srv_name);

	ION_INFO("ion connect channel done\n");

	return tipc;

err_create_chan:
	kfree(tipc);
err_alloc_dn:
	return NULL;
}

static void ion_tipc_free_msg_buf_list(struct list_head *list)
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

static void ion_tipc_disconnect(struct ion_tipc_ctx *tipc)
{
	wake_up_interruptible_all(&tipc->readq);

	tipc->state = TIPC_CHANNEL_DISCONNECTED;

	ion_tipc_free_msg_buf_list(&tipc->rx_msg_queue);

	/* shutdown channel  */
	tipc_chan_shutdown(tipc->chan);

	/* and destroy it */
	tipc_chan_destroy(tipc->chan);

	kfree(tipc);

	ION_INFO("ion tipc disconnect\n");
}

ssize_t ion_tipc_read(void *data_ptr, size_t max_len)
{
	struct tipc_msg_buf *mb;
	ssize_t len;
	struct ion_tipc_ctx *tipc = ion_tipc;

	if (!tipc) {
		ION_ERR("ion tipc context null!\n");
		return 0;
	}

	ION_DEBUG("ion polling data..\n");

	if (wait_event_interruptible(tipc->readq, !list_empty(&tipc->rx_msg_queue)))
		return -ERESTARTSYS;

	mb = list_first_entry(&tipc->rx_msg_queue, struct tipc_msg_buf, node);

	len = mb_avail_data(mb);
	if (len > max_len)
		len = max_len;

	memcpy(data_ptr, mb_get_data(mb, len), len);

	list_del(&mb->node);
	tipc_chan_put_rxbuf(tipc->chan, mb);

	return len;
}
EXPORT_SYMBOL_GPL(ion_tipc_read);

ssize_t ion_tipc_write(void *data_ptr, size_t len)
{
	int ret;
	int avail;
	struct tipc_msg_buf *txbuf = NULL;
	long timeout = 1000; /*1sec */
	struct ion_tipc_ctx *tipc = ion_tipc;
	struct list_head *msg_buf_list;
	struct list_head *pos, *pos_next;

	if (!tipc) {
		ION_ERR("ion tipc context null!\n");
		return -1;
	}

	msg_buf_list = kzalloc(sizeof(*msg_buf_list), GFP_KERNEL);
	if (!msg_buf_list) {
		ret = -ENOMEM;
		goto err;
	}
	INIT_LIST_HEAD(msg_buf_list);

	txbuf = tipc_chan_get_txbuf_timeout(tipc->chan, timeout);
	if (IS_ERR(txbuf))
		return  PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		ION_ERR("write no buffer space, len = %d, avail = %d\n",
			(int)len, (int)avail);
		ret = -EMSGSIZE;
		goto err;
	}

	memcpy(mb_put_data(txbuf, len), data_ptr, len);
	list_add_tail(&txbuf->node, msg_buf_list);

	ret = tipc_chan_queue_msg_list(tipc->chan, msg_buf_list);
	if (ret) {
		ION_ERR("tipc_chan_queue_msg_list error :%d\n", ret);
		goto err;
	}

	return ret;

err:
	if (msg_buf_list) {
		list_for_each_safe(pos, pos_next, msg_buf_list) {
			txbuf = list_entry(pos, struct tipc_msg_buf, node);
			if (txbuf) {
				list_del(&txbuf->node);
				tipc_chan_put_txbuf(tipc->chan, txbuf);
			}
		}
		kfree(msg_buf_list);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ion_tipc_write);

int ion_tipc_init(void)
{
	ION_INFO("ion tipc init start\n");

	ion_tipc = ion_tipc_connect(ION_TEE_PORT);
	if (ion_tipc)
		return 0;
	else
		return -1;
}
EXPORT_SYMBOL_GPL(ion_tipc_init);
void ion_tipc_exit(void)
{
	ion_tipc_disconnect(ion_tipc);
}
EXPORT_SYMBOL_GPL(ion_tipc_exit);
MODULE_LICENSE("GPL v2");
