// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/trusty/trusty_ipc.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(__fmt) "Trusty CA: %s: "__fmt, current->comm
#endif

#define DISP_TA_PORT_NAME "com.android.trusty.disp"

struct disp_ca {
	int state;
	struct mutex lock;
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct list_head rx_msg_queue;
};

static struct disp_ca disp_ca;

struct tipc_msg_buf *disp_ca_handle_msg(void *data,
			struct tipc_msg_buf *rxbuf, u16 flags)
{
	struct disp_ca *ca = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&ca->lock);
	if (ca->state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(ca->chan);
		if (newbuf) {
			pr_info("received new data, rxbuf %p, newbuf %p\n",
						  rxbuf, newbuf);
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &ca->rx_msg_queue);
			wake_up_interruptible(&ca->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			pr_info("discard incoming message\n");

			newbuf = rxbuf;
		}
	}
	mutex_unlock(&ca->lock);

	return newbuf;
}

static void disp_ca_handle_event(void *data, int event)
{
	struct disp_ca *ca = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		pr_info("channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		pr_info("channel disconnected\n");
		break;

	case TIPC_CHANNEL_CONNECTED:
		pr_info("channel connected\n");
		ca->state = TIPC_CHANNEL_CONNECTED;
		break;

	default:
		pr_err("unknown event type %d\n", event);
		break;
	}
}

static struct tipc_chan_ops disp_ca_ops = {
	.handle_msg = disp_ca_handle_msg,
	.handle_event = disp_ca_handle_event,
};

int disp_ca_connect(void)
{
	struct disp_ca *ca = &disp_ca;
	static bool initialized;
	int ret;

	if (ca->state == TIPC_CHANNEL_CONNECTED) {
		pr_warn("disp ca has already been connected\n");
		return 0;
	}

	if (!initialized) {
		struct tipc_chan *chan;

		chan = tipc_create_channel(NULL, &disp_ca_ops, ca);
		if (IS_ERR(chan)) {
			pr_err("tipc create channel failed\n");
			return PTR_ERR(chan);
		}
		ca->chan = chan;
		mutex_init(&ca->lock);
		init_waitqueue_head(&ca->readq);
		INIT_LIST_HEAD(&ca->rx_msg_queue);
		initialized = true;
	}

	ret = tipc_chan_connect(ca->chan, DISP_TA_PORT_NAME);
	if (ret)
		pr_err("connect channel failed\n");
	else
		pr_info("connect channel done\n");

	return 0;
}

static void disp_ca_free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	while (mb) {
		list_del(&mb->node);

		free_pages_exact(mb->buf_va, mb->buf_sz);
		kfree(mb);

		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

void disp_ca_disconnect(void)
{
	struct disp_ca *ca = &disp_ca;

	wake_up_interruptible_all(&ca->readq);

	disp_ca_free_msg_buf_list(&ca->rx_msg_queue);

	/* tipc_chan_shutdown(ca->chan); */

	ca->state = TIPC_CHANNEL_DISCONNECTED;

	/* tipc_chan_destroy(ca->chan); */

	pr_info("disp ca disconnect\n");
}

ssize_t disp_ca_read(void *buf, size_t len)
{
	struct tipc_msg_buf *mb;
	ssize_t avail;
	struct disp_ca *ca = &disp_ca;

	if (!ca->chan) {
		pr_err("ca tipc chan null!\n");
		return -EINVAL;
	}

	if (!wait_event_interruptible_timeout(ca->readq,
					      !list_empty(&ca->rx_msg_queue),
					      msecs_to_jiffies(500))) {
		pr_err("wait read response time out!\n");
		return -ETIMEDOUT;
	}

	mb = list_first_entry(&ca->rx_msg_queue, struct tipc_msg_buf, node);

	avail = mb_avail_data(mb);
	if (avail > len)
		avail = len;

	memcpy(buf, mb_get_data(mb, avail), avail);

	list_del(&mb->node);
	tipc_chan_put_rxbuf(ca->chan, mb);

	return avail;
}

ssize_t disp_ca_write(void *buf, size_t len)
{
	int ret;
	ssize_t avail;
	struct tipc_msg_buf *txbuf;
	struct disp_ca *ca = &disp_ca;
	long timeout = 1000; /*1sec */

	if (!ca) {
		pr_err("kcademo tipc context null!\n");
		return -EINVAL;
	}

	if (!ca->chan) {
		pr_err("ca tipc chan null!\n");
		return -EINVAL;
	}

	txbuf = tipc_chan_get_txbuf_timeout(ca->chan, timeout);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		pr_err("write no buffer space, len = %zu, avail = %zu\n",
			len, avail);
		ret = -EMSGSIZE;
		goto err;
	}

	memcpy(mb_put_data(txbuf, len), buf, len);

	ret = tipc_chan_queue_msg(ca->chan, txbuf);
	if (ret) {
		pr_err("tipc_chan_queue_msg error :%d\n", ret);
		goto err;
	}

	return len;

err:
	tipc_chan_put_txbuf(ca->chan, txbuf);

	return ret;
}

int disp_ca_wait_response(void)
{
	ssize_t size;
	u8 ack = 0;

	size = disp_ca_read(&ack, sizeof(ack));
	if (size < 0)
		return size;

	if (!ack) {
		pr_err("remote operation failed\n");
		return -ECOMM;
	}

	return 0;
}