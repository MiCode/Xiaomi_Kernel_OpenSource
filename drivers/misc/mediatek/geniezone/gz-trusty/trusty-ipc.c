/*
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* #define DEBUG */
#include <linux/aio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/idr.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>	/* Linux kernel 4.14 */
#include <linux/compat.h>
#include <linux/uio.h>

#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>

#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>
#include <gz-trusty/trusty_ipc.h>

#include <linux/hashtable.h>
#include <linux/stringhash.h>
#include "tee_routing_config.h"

#if IS_ENABLED(CONFIG_MTK_ENG_BUILD)
#define trusty_dbg(dev, fmt, ...) dev_dbg(dev, fmt, ##__VA_ARGS__)
#else
#define trusty_dbg(dev, fmt, ...)
#endif

#define MAX_DEVICES			4

#define REPLY_TIMEOUT			5000
#define TXBUF_TIMEOUT			15000

#define MAX_SRV_NAME_LEN		256

#define DEFAULT_MSG_BUF_SIZE		PAGE_SIZE
#define DEFAULT_MSG_BUF_ALIGN		PAGE_SIZE

#define TIPC_CTRL_ADDR			53
#define TIPC_ANY_ADDR			0xFFFFFFFF

#define TIPC_MIN_LOCAL_ADDR		1024

#define TIPC_IOC_MAGIC			'r'
#define TIPC_IOC_CONNECT		_IOW(TIPC_IOC_MAGIC, 0x80, char *)

#define MAX_TOKEN_LEN (8)
#define HASH_SALT ((const void *)0xCAFE)

/* define a hash table with 1<<5 buckets */
DEFINE_HASHTABLE(tee_routing_htable, 5);

struct tipc_virtio_dev;

struct tipc_msg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

enum tipc_ctrl_msg_types {
	TIPC_CTRL_MSGTYPE_GO_ONLINE = 1,
	TIPC_CTRL_MSGTYPE_GO_OFFLINE,
	TIPC_CTRL_MSGTYPE_CONN_REQ,
	TIPC_CTRL_MSGTYPE_CONN_RSP,
	TIPC_CTRL_MSGTYPE_DISC_REQ,
};

struct tipc_ctrl_msg {
	u32 type;
	u32 body_len;
	u8 body[0];
} __packed;

struct tipc_conn_req_body {
	char name[MAX_SRV_NAME_LEN];
} __packed;

struct tipc_conn_rsp_body {
	u32 target;
	u32 status;
	u32 remote;
	u32 max_msg_size;
	u32 max_msg_cnt;
} __packed;

struct tipc_disc_req_body {
	u32 target;
} __packed;

struct tipc_cdev_node {
	struct cdev cdev;
	struct device *dev;
	unsigned int minor;
};

enum tipc_device_state {
	VDS_OFFLINE = 0,
	VDS_ONLINE,
	VDS_DEAD,
};

struct tipc_virtio_dev {
	struct kref refcount;
	struct mutex lock;	/* protects access to this device */
	struct virtio_device *vdev;
	struct virtqueue *rxvq;
	struct virtqueue *txvq;
	char rxvq_name[MAX_DEV_NAME_LEN];
	char txvq_name[MAX_DEV_NAME_LEN];
	uint msg_buf_cnt;
	uint msg_buf_max_cnt;
	size_t msg_buf_max_sz;
	uint free_msg_buf_cnt;
	struct list_head free_buf_list;
	wait_queue_head_t sendq;
	struct idr addr_idr;
	enum tipc_device_state state;
	struct tipc_cdev_node cdev_node;
	char cdev_name[MAX_DEV_NAME_LEN];
	enum tee_id_t tee_id;
};

enum tipc_chan_state {
	TIPC_DISCONNECTED = 0,
	TIPC_CONNECTING,
	TIPC_CONNECTED,
	TIPC_STALE,
};

struct tipc_chan {
	struct mutex lock;	/* protects channel state  */
	struct kref refcount;
	enum tipc_chan_state state;
	struct tipc_virtio_dev *vds;
	const struct tipc_chan_ops *ops;
	void *ops_arg;
	u32 remote;
	u32 local;
	u32 max_msg_size;
	u32 max_msg_cnt;
	char srv_name[MAX_SRV_NAME_LEN];
};

static struct class *tipc_class;
static unsigned int tipc_major;

struct virtio_device *vdev_array[TEE_ID_END];

static DEFINE_IDR(tipc_devices);
static DEFINE_MUTEX(tipc_devices_lock);

static int _match_any(int id, void *p, void *data)
{
	return id;
}

static int _match_data(int id, void *p, void *data)
{
	return (p == data);
}

static void *_alloc_shareable_mem(size_t sz, phys_addr_t *ppa, gfp_t gfp)
{
	return alloc_pages_exact(sz, gfp);
}

static void _free_shareable_mem(size_t sz, void *va, phys_addr_t pa)
{
	free_pages_exact(va, sz);
}

static struct tipc_msg_buf *_alloc_msg_buf(size_t sz)
{
	struct tipc_msg_buf *mb;
	/* allocate tracking structure */
	mb = kzalloc(sizeof(struct tipc_msg_buf), GFP_KERNEL);
	if (!mb)
		return NULL;

	/* allocate buffer that can be shared with secure world */
	mb->buf_va = _alloc_shareable_mem(sz, &mb->buf_pa, GFP_KERNEL);
	if (!mb->buf_va)
		goto err_alloc;

	mb->buf_sz = sz;
	return mb;

err_alloc:
	kfree(mb);
	return NULL;
}

static void _free_msg_buf(struct tipc_msg_buf *mb)
{
	_free_shareable_mem(mb->buf_sz, mb->buf_va, mb->buf_pa);
	kfree(mb);
}

static void _free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb = NULL;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	while (mb) {
		list_del(&mb->node);
		_free_msg_buf(mb);
		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

static inline void mb_reset(struct tipc_msg_buf *mb)
{
	mb->wpos = 0;
	mb->rpos = 0;
}

static void _free_vds(struct kref *kref)
{
	struct tipc_virtio_dev *vds =
	    container_of(kref, struct tipc_virtio_dev, refcount);

	kfree(vds);
}

static void _free_chan(struct kref *kref)
{
	struct tipc_chan *ch = container_of(kref, struct tipc_chan, refcount);

	if (ch->ops && ch->ops->handle_release)
		ch->ops->handle_release(ch->ops_arg);

	kref_put(&ch->vds->refcount, _free_vds);
	kfree(ch);
}

static struct tipc_msg_buf *vds_alloc_msg_buf(struct tipc_virtio_dev *vds)
{
	return _alloc_msg_buf(vds->msg_buf_max_sz);
}

static void vds_free_msg_buf(struct tipc_virtio_dev *vds,
			     struct tipc_msg_buf *mb)
{
	_free_msg_buf(mb);
}

static bool _put_txbuf_locked(struct tipc_virtio_dev *vds,
			      struct tipc_msg_buf *mb)
{
	list_add_tail(&mb->node, &vds->free_buf_list);
	return vds->free_msg_buf_cnt++ == 0;
}

static struct tipc_msg_buf *_get_txbuf_locked(struct tipc_virtio_dev *vds)
{
	struct tipc_msg_buf *mb;

	if (vds->state != VDS_ONLINE)
		return ERR_PTR(-ENODEV);

	if (vds->free_msg_buf_cnt) {
		/* take it out of free list */
		mb = list_first_entry(&vds->free_buf_list,
				      struct tipc_msg_buf, node);
		list_del(&mb->node);
		vds->free_msg_buf_cnt--;
	} else {
		if (vds->msg_buf_cnt >= vds->msg_buf_max_cnt)
			return ERR_PTR(-EAGAIN);

		/* try to allocate it */
		mb = _alloc_msg_buf(vds->msg_buf_max_sz);
		if (!mb)
			return ERR_PTR(-ENOMEM);

		vds->msg_buf_cnt++;
	}
	return mb;
}

static struct tipc_msg_buf *_vds_get_txbuf(struct tipc_virtio_dev *vds)
{
	struct tipc_msg_buf *mb;

	mutex_lock(&vds->lock);
	mb = _get_txbuf_locked(vds);
	mutex_unlock(&vds->lock);

	return mb;
}

static void vds_put_txbuf(struct tipc_virtio_dev *vds, struct tipc_msg_buf *mb)
{

	mutex_lock(&vds->lock);
	_put_txbuf_locked(vds, mb);
	wake_up_interruptible(&vds->sendq);
	mutex_unlock(&vds->lock);
}

static int is_valid_vds(struct tipc_virtio_dev *vds)
{
	int i = 0;
	int ret = 0;

	trusty_dbg(&vds->vdev->dev, "%s: vds 0x%p\n", __func__, vds);
	if (unlikely(!virt_addr_valid(vds)))
		return -EFAULT;

	for (i = 0 ; i < TEE_ID_END ; i++)
		if (vdev_array[i])
			ret |= (vds == vdev_array[i]->priv);

	return (ret > 0) ? ret : -ENODEV;
}

static struct tipc_msg_buf *vds_get_txbuf(struct tipc_virtio_dev *vds,
					  long timeout)
{
	struct tipc_msg_buf *mb;
	int ret;

	/* sanity check */
	ret = is_valid_vds(vds);
	if (unlikely(ret < 0)) {
		pr_info("%s: error vds 0x%p ret:%d\n", __func__, vds, ret);
		return ERR_PTR(ret);
	}

	mb = _vds_get_txbuf(vds);

	if ((PTR_ERR(mb) == -EAGAIN) && timeout) {
		DEFINE_WAIT_FUNC(wait, woken_wake_function);

		timeout = msecs_to_jiffies(timeout);
		add_wait_queue(&vds->sendq, &wait);
		for (;;) {
			timeout = wait_woken(&wait, TASK_INTERRUPTIBLE,
					     timeout);
			if (!timeout) {
				mb = ERR_PTR(-ETIMEDOUT);
				break;
			}

			if (signal_pending(current)) {
				mb = ERR_PTR(-ERESTARTSYS);
				break;
			}

			mb = _vds_get_txbuf(vds);
			if (PTR_ERR(mb) != -EAGAIN)
				break;
		}
		remove_wait_queue(&vds->sendq, &wait);
	}

	if (IS_ERR(mb))
		return mb;

	WARN_ON(!mb);

	/* reset and reserve space for message header */
	mb_reset(mb);
	mb_put_data(mb, sizeof(struct tipc_msg_hdr));

	return mb;
}

static int vds_queue_txbuf(struct tipc_virtio_dev *vds, struct tipc_msg_buf *mb)
{
	int err;
	struct scatterlist sg;
	bool need_notify = false;


	mutex_lock(&vds->lock);
	if (vds->state == VDS_ONLINE) {
		sg_init_one(&sg, mb->buf_va, mb->wpos);
		err = virtqueue_add_outbuf(vds->txvq, &sg, 1, mb, GFP_KERNEL);
		need_notify = virtqueue_kick_prepare(vds->txvq);
	} else {
		err = -ENODEV;
	}
	mutex_unlock(&vds->lock);

	if (need_notify)
		virtqueue_notify(vds->txvq);

	return err;
}

static int vds_add_channel(struct tipc_virtio_dev *vds, struct tipc_chan *chan)
{
	int ret;

	mutex_lock(&vds->lock);
	if (vds->state == VDS_ONLINE) {
		ret = idr_alloc(&vds->addr_idr, chan,
				TIPC_MIN_LOCAL_ADDR, TIPC_ANY_ADDR - 1,
				GFP_KERNEL);
		if (ret > 0) {
			chan->local = ret;
			kref_get(&chan->refcount);
			ret = 0;
		}
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&vds->lock);

	return ret;
}

static void vds_del_channel(struct tipc_virtio_dev *vds, struct tipc_chan *chan)
{
	mutex_lock(&vds->lock);
	if (chan->local) {
		idr_remove(&vds->addr_idr, chan->local);
		chan->local = 0;
		chan->remote = 0;
		kref_put(&chan->refcount, _free_chan);
	}
	mutex_unlock(&vds->lock);
}

static struct tipc_chan *vds_lookup_channel(struct tipc_virtio_dev *vds,
					    u32 addr)
{
	int id;
	struct tipc_chan *chan = NULL;

	mutex_lock(&vds->lock);
	if (addr == TIPC_ANY_ADDR) {
		id = idr_for_each(&vds->addr_idr, _match_any, NULL);
		if (id > 0)
			chan = idr_find(&vds->addr_idr, id);
	} else {
		chan = idr_find(&vds->addr_idr, addr);
	}
	if (chan)
		kref_get(&chan->refcount);
	mutex_unlock(&vds->lock);

	return chan;
}

static struct tipc_chan *vds_create_channel(struct tipc_virtio_dev *vds,
					    const struct tipc_chan_ops *ops,
					    void *ops_arg)
{
	int ret;
	struct tipc_chan *chan = NULL;

	if (!vds)
		return ERR_PTR(-ENOENT);

	if (!ops)
		return ERR_PTR(-EINVAL);

	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return ERR_PTR(-ENOMEM);

	kref_get(&vds->refcount);
	chan->vds = vds;
	chan->ops = ops;
	chan->ops_arg = ops_arg;
	mutex_init(&chan->lock);
	kref_init(&chan->refcount);
	chan->state = TIPC_DISCONNECTED;

	ret = vds_add_channel(vds, chan);
	if (ret) {
		kfree(chan);
		kref_put(&vds->refcount, _free_vds);
		return ERR_PTR(ret);
	}

	return chan;
}

static void fill_msg_hdr(struct tipc_msg_buf *mb, u32 src, u32 dst)
{
	struct tipc_msg_hdr *hdr = mb_get_data(mb, sizeof(*hdr));

	hdr->src = src;
	hdr->dst = dst;
	hdr->len = mb_avail_data(mb);
	hdr->flags = 0;
	hdr->reserved = 0;
}

struct tipc_chan *tipc_create_channel(struct device *dev,
				      const struct tipc_chan_ops *ops,
				      void *ops_arg)
{
	struct virtio_device *vd = NULL;
	struct tipc_chan *chan;
	struct tipc_virtio_dev *vds;
	struct tipc_dn_chan *dn = ops_arg;

	mutex_lock(&tipc_devices_lock);
	if (dev) {
		vd = container_of(dev, struct virtio_device, dev);
	} else {
		if (is_tee_id(dn->tee_id))
			vd = vdev_array[dn->tee_id];
		if (!vd) {
			mutex_unlock(&tipc_devices_lock);
			return ERR_PTR(-ENOENT);
		}
	}
	vds = vd->priv;
	kref_get(&vds->refcount);
	mutex_unlock(&tipc_devices_lock);

	chan = vds_create_channel(vds, ops, ops_arg);
	kref_put(&vds->refcount, _free_vds);
	return chan;
}
EXPORT_SYMBOL(tipc_create_channel);

struct tipc_msg_buf *tipc_chan_get_rxbuf(struct tipc_chan *chan)
{
	/* sanity check */
	if (unlikely(!virt_addr_valid(chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, chan);
		return ERR_PTR(-EFAULT);
	}
	return vds_alloc_msg_buf(chan->vds);
}
EXPORT_SYMBOL(tipc_chan_get_rxbuf);

void tipc_chan_put_rxbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	/* sanity check */
	if (unlikely(!virt_addr_valid(chan)))
		pr_info("%s: error channel 0x%p\n", __func__, chan);
	else
		vds_free_msg_buf(chan->vds, mb);
}
EXPORT_SYMBOL(tipc_chan_put_rxbuf);

struct tipc_msg_buf *tipc_chan_get_txbuf_timeout(struct tipc_chan *chan,
						 long timeout)
{
	/* sanity check */
	if (unlikely(!virt_addr_valid(chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, chan);
		return ERR_PTR(-EFAULT);
	}
	return vds_get_txbuf(chan->vds, timeout);
}
EXPORT_SYMBOL(tipc_chan_get_txbuf_timeout);

void tipc_chan_put_txbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	/* sanity check */
	if (unlikely(!virt_addr_valid(chan)))
		pr_info("%s: error channel 0x%p\n", __func__, chan);
	else
		vds_put_txbuf(chan->vds, mb);
}
EXPORT_SYMBOL(tipc_chan_put_txbuf);

int tipc_chan_queue_msg(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	int err;

	/* sanity check */
	if (unlikely(!virt_addr_valid(chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, chan);
		return -EFAULT;
	}

	mutex_lock(&chan->lock);
	switch (chan->state) {
	case TIPC_CONNECTED:
		fill_msg_hdr(mb, chan->local, chan->remote);
		err = vds_queue_txbuf(chan->vds, mb);
		if (err) {
			/* this should never happen */
			pr_info("%s: failed to queue tx buffer (%d)\n",
				__func__, err);
		}
		break;
	case TIPC_DISCONNECTED:
	case TIPC_CONNECTING:
		err = -ENOTCONN;
		break;
	case TIPC_STALE:
		err = -ESHUTDOWN;
		break;
	default:
		err = -EBADFD;
		pr_info("%s: unexpected channel state %d\n",
			__func__, chan->state);
	}
	mutex_unlock(&chan->lock);

	return err;
}
EXPORT_SYMBOL(tipc_chan_queue_msg);


int tipc_chan_connect(struct tipc_chan *chan, const char *name)
{
	int err;
	struct tipc_ctrl_msg *msg;
	struct tipc_conn_req_body *body;
	struct tipc_msg_buf *txbuf;

	/* sanity check */
	if (unlikely(!virt_addr_valid(chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, chan);
		return -EFAULT;
	}

	txbuf = vds_get_txbuf(chan->vds, TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* reserve space for connection request control message */
	msg = mb_put_data(txbuf, sizeof(*msg) + sizeof(*body));
	body = (struct tipc_conn_req_body *)msg->body;

	/* fill message */
	msg->type = TIPC_CTRL_MSGTYPE_CONN_REQ;
	msg->body_len = sizeof(*body);

	strncpy(body->name, name, sizeof(body->name));
	body->name[sizeof(body->name) - 1] = '\0';

	mutex_lock(&chan->lock);
	switch (chan->state) {
	case TIPC_DISCONNECTED:
		/* save service name we are connecting to */
		strncpy(chan->srv_name, body->name, sizeof(body->name) - 1);

		fill_msg_hdr(txbuf, chan->local, TIPC_CTRL_ADDR);
		err = vds_queue_txbuf(chan->vds, txbuf);
		if (err) {
			/* this should never happen */
			pr_info("%s: failed to queue tx buffer (%d)\n",
				__func__, err);
		} else {
			chan->state = TIPC_CONNECTING;
			txbuf = NULL;	/* prevents discarding buffer */
		}
		break;
	case TIPC_CONNECTED:
	case TIPC_CONNECTING:
		/* check if we are trying to connect to the same service */
		if (strcmp(chan->srv_name, body->name) == 0)
			err = 0;
		else if (chan->state == TIPC_CONNECTING)
			err = -EALREADY;	/* in progress */
		else
			err = -EISCONN;	/* already connected */
		break;

	case TIPC_STALE:
		err = -ESHUTDOWN;
		break;
	default:
		err = -EBADFD;
		pr_info("%s: unexpected channel state %d\n",
			__func__, chan->state);
		break;
	}
	mutex_unlock(&chan->lock);

	if (txbuf)
		tipc_chan_put_txbuf(chan, txbuf);	/* discard it */

	return err;
}
EXPORT_SYMBOL(tipc_chan_connect);

int tipc_chan_shutdown(struct tipc_chan *chan)
{
	int err;
	struct tipc_ctrl_msg *msg;
	struct tipc_disc_req_body *body;
	struct tipc_msg_buf *txbuf = NULL;

	/* sanity check */
	if (unlikely(!virt_addr_valid(chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, chan);
		return -EFAULT;
	}

	if (unlikely(!virt_addr_valid(chan->vds))) {
		pr_info("%s: error vds 0x%p\n", __func__, chan->vds);
		return -EFAULT;
	}

	/* get tx buffer */
	txbuf = vds_get_txbuf(chan->vds, TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	mutex_lock(&chan->lock);
	if (chan->state == TIPC_CONNECTED || chan->state == TIPC_CONNECTING) {
		/* reserve space for disconnect request control message */
		msg = mb_put_data(txbuf, sizeof(*msg) + sizeof(*body));
		body = (struct tipc_disc_req_body *)msg->body;

		msg->type = TIPC_CTRL_MSGTYPE_DISC_REQ;
		msg->body_len = sizeof(*body);
		body->target = chan->remote;

		fill_msg_hdr(txbuf, chan->local, TIPC_CTRL_ADDR);
		err = vds_queue_txbuf(chan->vds, txbuf);
		if (err) {
			/* this should never happen */
			pr_info("%s: failed to queue tx buffer (%d)\n",
				__func__, err);
		}
	} else {
		err = -ENOTCONN;
	}
	chan->state = TIPC_STALE;
	mutex_unlock(&chan->lock);

	if (err) {
		/* release buffer */
		tipc_chan_put_txbuf(chan, txbuf);
	}

	return err;
}
EXPORT_SYMBOL(tipc_chan_shutdown);

void tipc_chan_destroy(struct tipc_chan *chan)
{
	vds_del_channel(chan->vds, chan);
	kref_put(&chan->refcount, _free_chan);
}
EXPORT_SYMBOL(tipc_chan_destroy);

static int dn_wait_for_reply(struct tipc_dn_chan *dn, int timeout)
{
	int ret;

	ret = wait_for_completion_interruptible_timeout(&dn->reply_comp,
							msecs_to_jiffies
							(timeout));
	if (ret < 0)
		return ret;

	mutex_lock(&dn->lock);
	if (!ret) {
		/* no reply from remote */
		dn->state = TIPC_STALE;
		ret = -ETIMEDOUT;
	} else {
		/* got reply */
		if (dn->state == TIPC_CONNECTED)
			ret = 0;
		else if (dn->state == TIPC_DISCONNECTED)
			if (!list_empty(&dn->rx_msg_queue))
				ret = 0;
			else
				ret = -ENOTCONN;
		else
			ret = -EIO;
	}
	mutex_unlock(&dn->lock);

	return ret;
}

struct tipc_msg_buf *dn_handle_msg(void *data, struct tipc_msg_buf *rxbuf)
{
	struct tipc_dn_chan *dn = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(dn->chan);
		if (newbuf) {
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &dn->rx_msg_queue);
			wake_up_interruptible(&dn->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			pr_info("%s: discard incoming message\n", __func__);
			newbuf = rxbuf;
		}
	}
	mutex_unlock(&dn->lock);

	return newbuf;
}

static void dn_connected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_CONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	mutex_unlock(&dn->lock);
}

static void dn_disconnected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_DISCONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_shutdown(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);

	/* set state to STALE */
	dn->state = TIPC_STALE;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_handle_event(void *data, int event)
{
	struct tipc_dn_chan *dn = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		dn_shutdown(dn);
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		dn_disconnected(dn);
		break;

	case TIPC_CHANNEL_CONNECTED:
		dn_connected(dn);
		break;

	default:
		pr_info("%s: unhandled event %d\n", __func__, event);
		break;
	}
}

static void dn_handle_release(void *data)
{
	kfree(data);
}

static struct tipc_chan_ops _dn_ops = {
	.handle_msg = dn_handle_msg,
	.handle_event = dn_handle_event,
	.handle_release = dn_handle_release,
};

#define cdev_to_cdn(c) container_of((c), struct tipc_cdev_node, cdev)
#define cdn_to_vds(cdn) container_of((cdn), struct tipc_virtio_dev, cdev_node)

static struct tipc_virtio_dev *dn_lookup_vds(struct tipc_cdev_node *cdn)
{
	int ret;
	struct tipc_virtio_dev *vds = (struct tipc_virtio_dev *)NULL;

	mutex_lock(&tipc_devices_lock);
	ret = idr_for_each(&tipc_devices, _match_data, cdn);
	if (ret) {
		vds = cdn_to_vds(cdn);
		kref_get(&vds->refcount);
	}
	mutex_unlock(&tipc_devices_lock);

	return vds;
}

static int tipc_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct tipc_virtio_dev *vds;
	struct tipc_dn_chan *dn;
	struct tipc_cdev_node *cdn = cdev_to_cdn(inode->i_cdev);

	vds = dn_lookup_vds(cdn);
	if (!vds) {
		ret = -ENOENT;
		goto err_vds_lookup;
	}

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn) {
		ret = -ENOMEM;
		goto err_alloc_chan;
	}

	mutex_init(&dn->lock);
	init_waitqueue_head(&dn->readq);
	init_completion(&dn->reply_comp);
	INIT_LIST_HEAD(&dn->rx_msg_queue);

	dn->tee_id = vds->tee_id;
	dn->state = TIPC_DISCONNECTED;

	dn->chan = vds_create_channel(vds, &_dn_ops, dn);
	if (IS_ERR(dn->chan)) {
		ret = PTR_ERR(dn->chan);
		goto err_create_chan;
	}

	filp->private_data = dn;
	kref_put(&vds->refcount, _free_vds);
	return 0;

err_create_chan:
	kfree(dn);
err_alloc_chan:
	kref_put(&vds->refcount, _free_vds);
err_vds_lookup:
	return ret;
}


static int dn_connect_ioctl(struct tipc_dn_chan *dn, char __user *usr_name)
{
	int err;
	char name[MAX_SRV_NAME_LEN];

	/* copy in service name from user space */
	err = strncpy_from_user(name, usr_name, sizeof(name));
	if (err < 0) {
		pr_info("%s: copy_from_user (%p) failed (%d)\n",
			__func__, usr_name, err);
		return err;
	}
	name[sizeof(name) - 1] = '\0';

	/* send connect request */
	err = tipc_chan_connect(dn->chan, name);
	if (err)
		return err;

	/* and wait for reply */
	return dn_wait_for_reply(dn, REPLY_TIMEOUT);
}

static long tipc_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg, bool is_compat)
{
	int ret;
	char __user *user_req;
	struct tipc_dn_chan *dn = filp->private_data;

	if (_IOC_TYPE(cmd) != TIPC_IOC_MAGIC)
		return -EINVAL;

#if IS_ENABLED(CONFIG_COMPAT)
	if (is_compat)
		user_req = (char __user *)compat_ptr(arg);
	else
#endif
		user_req = (char __user *)arg;

	switch (cmd) {
	case TIPC_IOC_CONNECT:
		ret = dn_connect_ioctl(dn, user_req);
		if (ret) {
			pr_info("%s: TIPC_IOC_CONNECT error (%d)!\n",
				__func__, ret);
		}
		break;
	default:
		pr_info("%s: Unhandled ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	}
	return ret;
}

static long tipc_ioctl_entry(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return tipc_ioctl(filp, cmd, arg, false);
}

#if IS_ENABLED(CONFIG_COMPAT)
static long tipc_compat_ioctl_entry(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	return tipc_ioctl(filp, cmd, arg, true);
}
#endif

static inline bool _got_rx(struct tipc_dn_chan *dn)
{
	if (dn->state != TIPC_CONNECTED)
		return true;

	if (!list_empty(&dn->rx_msg_queue))
		return true;

	return false;
}

static ssize_t tipc_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t ret;
	size_t len;
	struct tipc_msg_buf *mb;
	struct file *filp = iocb->ki_filp;
	struct tipc_dn_chan *dn = filp->private_data;

	mutex_lock(&dn->lock);

	while (list_empty(&dn->rx_msg_queue)) {
		if (dn->state != TIPC_CONNECTED) {
			if (dn->state == TIPC_CONNECTING)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_DISCONNECTED)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_STALE)
				ret = -ESHUTDOWN;
			else
				ret = -EBADFD;
			goto out;
		}

		mutex_unlock(&dn->lock);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(dn->readq, _got_rx(dn)))
			return -ERESTARTSYS;

		mutex_lock(&dn->lock);
	}

	mb = list_first_entry(&dn->rx_msg_queue, struct tipc_msg_buf, node);

	len = mb_avail_data(mb);
	if (len > iov_iter_count(iter)) {
		ret = -EMSGSIZE;
		goto out;
	}

	if (copy_to_iter(mb_get_data(mb, len), len, iter) != len) {
		ret = -EFAULT;
		goto out;
	}

	ret = len;
	list_del(&mb->node);
	tipc_chan_put_rxbuf(dn->chan, mb);

out:
	mutex_unlock(&dn->lock);
	return ret;
}

static ssize_t tipc_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t ret;
	size_t len;
	long timeout = TXBUF_TIMEOUT;
	struct tipc_msg_buf *txbuf = NULL;
	struct file *filp = iocb->ki_filp;
	struct tipc_dn_chan *dn = filp->private_data;

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	txbuf = tipc_chan_get_txbuf_timeout(dn->chan, timeout);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* message length */
	len = iov_iter_count(iter);

	/* check available space */
	if (len > mb_avail_space(txbuf)) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	/* copy in message data */
	if (copy_from_iter(mb_put_data(txbuf, len), len, iter) != len) {
		ret = -EFAULT;
		goto err_out;
	}

	/* queue message */
	ret = tipc_chan_queue_msg(dn->chan, txbuf);
	if (ret)
		goto err_out;

	return len;

err_out:
	tipc_chan_put_txbuf(dn->chan, txbuf);
	return ret;
}

static unsigned int tipc_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct tipc_dn_chan *dn = filp->private_data;

	mutex_lock(&dn->lock);

	poll_wait(filp, &dn->readq, wait);

	/* Writes always succeed for now */
	mask |= POLLOUT | POLLWRNORM;

	if (!list_empty(&dn->rx_msg_queue))
		mask |= POLLIN | POLLRDNORM;

	if (dn->state != TIPC_CONNECTED)
		mask |= POLLERR;

	mutex_unlock(&dn->lock);
	return mask;
}


static int tipc_release(struct inode *inode, struct file *filp)
{
	struct tipc_dn_chan *dn = filp->private_data;

	dn_shutdown(dn);

	/* free all pending buffers */
	_free_msg_buf_list(&dn->rx_msg_queue);

	/* shutdown channel  */
	tipc_chan_shutdown(dn->chan);

	/* and destroy it */
	tipc_chan_destroy(dn->chan);

	return 0;
}

static const struct file_operations tipc_fops = {
	.open = tipc_open,
	.release = tipc_release,
	.unlocked_ioctl = tipc_ioctl_entry,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = tipc_compat_ioctl_entry,
#endif
	.read_iter = tipc_read_iter,
	.write_iter = tipc_write_iter,
	.poll = tipc_poll,
	.owner = THIS_MODULE,
};

/* strdup do not free the string memory, pls free it after using */
static char *strdup_s(const char *str)
{
	size_t lens = strlen(str);
	char *tmp = kmalloc(lens + 1, GFP_KERNEL);

	if (!tmp)
		return ERR_PTR(-ENOMEM);

	strncpy(tmp, str, lens);
	tmp[lens] = '\0';
	return tmp;
}

int port_lookup_tid(const char *port, enum tee_id_t *o_tid)
{
	char *token, *str, *p;
	const char *delim = ".";
	u32 hash_val;
	struct tee_routing_obj *tr_obj;
	const enum tee_id_t default_tee_id = tee_routing_config[0].tee_id;

	/* Set default value */
	*o_tid = default_tee_id;

	str = strdup_s(port);
	if (IS_ERR(str))
		return -ENOMEM;

	p = str;
	token = strsep(&p, delim);
	if (!token) {
		/* we can not determine which vds to be delivered,
		 * just take the default.
		 */
		WARN(1, "[%s] Service name error %s\n", __func__, port);
		kfree(str);
		return -EINVAL;
	}

	hash_val = hashlen_hash(hashlen_string(HASH_SALT, token));

	hash_for_each_possible(tee_routing_htable, tr_obj, node, hash_val) {
		/* If hash table hit, set from tee_routing_config. */
		if (strcmp(token, tr_obj->srv_name) == 0) {
			*o_tid = tr_obj->tee_id;
			pr_debug("[%s] find token %s, tid %d\n",
				 __func__, token, *o_tid);
			break;
		}
	}

	kfree(str);
	return 0;
}
EXPORT_SYMBOL(port_lookup_tid);

/* Search tipc_virtio_dev by first word of port name. See tee_routing_config.h
 * for detail rules.
 */
static struct tipc_virtio_dev *port_lookup_vds(const char *port)
{
	struct tipc_virtio_dev *vds;
	enum tee_id_t tee_id;
	int ret;

	ret = port_lookup_tid(port, &tee_id);

	if (ret) {
		pr_info("[%s] get tee_id failed %d ret %d, may cause failure\n",
			__func__, tee_id, ret);
	}

	if (likely(is_tee_id(tee_id))) {
		if (likely(vdev_array[tee_id])) {
			vds = vdev_array[tee_id]->priv;
			kref_get(&vds->refcount);
			return vds;
		}
	}

	return ERR_PTR(-ENODEV);
}

static int tipc_open_channel(struct tipc_dn_chan **o_dn, const char *port)
{
	int ret;
	struct tipc_virtio_dev *vds;
	struct tipc_dn_chan *dn;

	vds = port_lookup_vds(port);

	if (IS_ERR(vds)) {
		pr_info("[%s] ERROR: virtio device not found\n", __func__);
		ret = -ENOENT;
		goto err_vds_lookup;
	}

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn) {
		ret = -ENOMEM;
		goto err_alloc_chan;
	}

	mutex_init(&dn->lock);
	init_waitqueue_head(&dn->readq);
	init_completion(&dn->reply_comp);
	INIT_LIST_HEAD(&dn->rx_msg_queue);

	dn->tee_id = vds->tee_id;
	dn->state = TIPC_DISCONNECTED;

	dn->chan = vds_create_channel(vds, &_dn_ops, dn);
	if (IS_ERR(dn->chan)) {
		ret = PTR_ERR(dn->chan);
		goto err_create_chan;
	}

	kref_put(&vds->refcount, _free_vds);
	*o_dn = dn;
	return 0;

err_create_chan:
	kfree(dn);
err_alloc_chan:
	kref_put(&vds->refcount, _free_vds);
err_vds_lookup:
	return ret;
}

int tipc_k_connect(struct tipc_k_handle *h, const char *port)
{
	int err;
	struct tipc_dn_chan *dn = NULL;

	err = tipc_open_channel(&dn, port);
	if (err)
		return err;

	h->dn = dn;

	/* send connect request */
	err = tipc_chan_connect(dn->chan, port);
	if (err)
		return err;

	/* and wait for reply */
	return dn_wait_for_reply(dn, REPLY_TIMEOUT);
}
EXPORT_SYMBOL(tipc_k_connect);

int tipc_k_disconnect(struct tipc_k_handle *h)
{
	struct tipc_dn_chan *dn = h->dn;

	dn_shutdown(dn);

	/* free all pending buffers */
	_free_msg_buf_list(&dn->rx_msg_queue);

	/* shutdown channel  */
	tipc_chan_shutdown(dn->chan);

	/* and destroy it */
	tipc_chan_destroy(dn->chan);
	/* data is now be free in dn_handle_release(..) */

	/*kfree(dn);*/

	return 0;
}
EXPORT_SYMBOL(tipc_k_disconnect);

ssize_t tipc_k_read(struct tipc_k_handle *h, void *buf, size_t buf_len,
		    unsigned int flags)
{
	ssize_t ret;
	size_t data_len;
	struct tipc_msg_buf *mb;
	struct tipc_dn_chan *dn = (struct tipc_dn_chan *)h->dn;

	mutex_lock(&dn->lock);

	while (list_empty(&dn->rx_msg_queue)) {
		if (dn->state != TIPC_CONNECTED) {
			if (dn->state == TIPC_CONNECTING)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_DISCONNECTED)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_STALE)
				ret = -ESHUTDOWN;
			else
				ret = -EBADFD;
			goto out;
		}

		mutex_unlock(&dn->lock);

		if (flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible(dn->readq, _got_rx(dn)))
			return -ERESTARTSYS;

		mutex_lock(&dn->lock);
	}

	mb = list_first_entry(&dn->rx_msg_queue, struct tipc_msg_buf, node);

	data_len = mb_avail_data(mb);
	if (data_len > buf_len) {
		ret = -EMSGSIZE;
		goto out;
	}

	memcpy(buf, mb_get_data(mb, data_len), data_len);

	ret = data_len;
	list_del(&mb->node);
	tipc_chan_put_rxbuf(dn->chan, mb);

out:
	mutex_unlock(&dn->lock);
	return ret;
}
EXPORT_SYMBOL(tipc_k_read);

ssize_t tipc_k_write(struct tipc_k_handle *h, void *buf, size_t len,
		     unsigned int flags)
{
	ssize_t ret;
	long timeout = TXBUF_TIMEOUT;
	struct tipc_msg_buf *txbuf = NULL;
	struct tipc_dn_chan *dn = (struct tipc_dn_chan *)h->dn;

	if (flags & O_NONBLOCK)
		timeout = 0;

	/* sanity check */
	if (unlikely(!virt_addr_valid(dn))) {
		pr_info("%s: error handle 0x%p\n", __func__, dn);
		return -EFAULT;
	}

	if (unlikely(!virt_addr_valid(dn->chan))) {
		pr_info("%s: error channel 0x%p\n", __func__, dn->chan);
		return -EFAULT;
	}

	txbuf = tipc_chan_get_txbuf_timeout(dn->chan, timeout);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* check available space */
	if (len > mb_avail_space(txbuf)) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	/* copy in message data */
	memcpy(mb_put_data(txbuf, len), buf, len);

	/* queue message */
	ret = tipc_chan_queue_msg(dn->chan, txbuf);
	if (ret)
		goto err_out;

	return len;

err_out:
	tipc_chan_put_txbuf(dn->chan, txbuf);
	return ret;
}
EXPORT_SYMBOL(tipc_k_write);

static void chan_trigger_event(struct tipc_chan *chan, int event)
{
	if (!event)
		return;

	chan->ops->handle_event(chan->ops_arg, event);
}

static void _cleanup_vq(struct virtqueue *vq)
{
	struct tipc_msg_buf *mb;

	while ((mb = virtqueue_detach_unused_buf(vq)) != NULL)
		_free_msg_buf(mb);
}

static int _create_cdev_node(struct device *parent,
			     struct tipc_cdev_node *cdn, const char *name)
{
	int ret;
	dev_t devt;

	if (!name) {
		dev_dbg(parent, "%s: cdev name has to be provided\n", __func__);
		return -EINVAL;
	}

	/* allocate minor */
	ret = idr_alloc(&tipc_devices, cdn, 0, MAX_DEVICES - 1, GFP_KERNEL);
	if (ret < 0) {
		dev_dbg(parent, "%s: failed (%d) to get id\n", __func__, ret);
		return ret;
	}

	cdn->minor = ret;
	cdev_init(&cdn->cdev, &tipc_fops);
	cdn->cdev.owner = THIS_MODULE;

	/* Add character device */
	devt = MKDEV(tipc_major, cdn->minor);
	ret = cdev_add(&cdn->cdev, devt, 1);
	if (ret) {
		dev_dbg(parent, "%s: cdev_add failed (%d)\n", __func__, ret);
		goto err_add_cdev;
	}

	/* Create a device node */
	cdn->dev = device_create(tipc_class, parent, devt, NULL, name);

	if (IS_ERR(cdn->dev)) {
		ret = PTR_ERR(cdn->dev);
		dev_dbg(parent, "%s: device_create failed: %d\n",
			__func__, ret);
		goto err_device_create;
	}

	return 0;

err_device_create:
	cdn->dev = NULL;
	cdev_del(&cdn->cdev);
err_add_cdev:
	idr_remove(&tipc_devices, cdn->minor);
	return ret;
}

static void create_cdev_node(struct tipc_virtio_dev *vds,
			     struct tipc_cdev_node *cdn)
{
	int err;

	mutex_lock(&tipc_devices_lock);

	if (is_tee_id(vds->tee_id)) {
		if (!vdev_array[vds->tee_id]) {
			kref_get(&vds->refcount);
			vdev_array[vds->tee_id] = vds->vdev;
		}
	}

	if (vds->cdev_name[0] && !cdn->dev) {
		kref_get(&vds->refcount);
		err = _create_cdev_node(&vds->vdev->dev, cdn, vds->cdev_name);
		if (err) {
			dev_info(&vds->vdev->dev,
				 "failed (%d) to create cdev node\n", err);
			kref_put(&vds->refcount, _free_vds);
		}
	}
	mutex_unlock(&tipc_devices_lock);
}

static void destroy_cdev_node(struct tipc_virtio_dev *vds,
			      struct tipc_cdev_node *cdn)
{
	mutex_lock(&tipc_devices_lock);

	if (cdn->dev) {
		device_destroy(tipc_class, MKDEV(tipc_major, cdn->minor));
		cdev_del(&cdn->cdev);
		idr_remove(&tipc_devices, cdn->minor);
		cdn->dev = NULL;
		kref_put(&vds->refcount, _free_vds);
	}

	if (is_tee_id(vds->tee_id)) {
		if (vdev_array[vds->tee_id] == vds->vdev) {
			vdev_array[vds->tee_id] = NULL;
			kref_put(&vds->refcount, _free_vds);
		}
	}

	mutex_unlock(&tipc_devices_lock);
}

static void _go_online(struct tipc_virtio_dev *vds)
{
	mutex_lock(&vds->lock);

	if (vds->state == VDS_OFFLINE)
		vds->state = VDS_ONLINE;

	mutex_unlock(&vds->lock);

	create_cdev_node(vds, &vds->cdev_node);

	dev_info(&vds->vdev->dev, "is online\n");
}

static void _go_offline(struct tipc_virtio_dev *vds)
{
	struct tipc_chan *chan;

	/* change state to OFFLINE */
	mutex_lock(&vds->lock);
	if (vds->state != VDS_ONLINE) {
		mutex_unlock(&vds->lock);
		return;
	}
	vds->state = VDS_OFFLINE;
	mutex_unlock(&vds->lock);

	/* wakeup all waiters */
	wake_up_interruptible_all(&vds->sendq);

	/* shutdown all channels */
	while ((chan = vds_lookup_channel(vds, TIPC_ANY_ADDR))) {
		mutex_lock(&chan->lock);
		chan->state = TIPC_STALE;
		chan->remote = 0;
		chan_trigger_event(chan, TIPC_CHANNEL_SHUTDOWN);
		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	}

	/* shutdown device node */
	destroy_cdev_node(vds, &vds->cdev_node);
	dev_info(&vds->vdev->dev, "is offline\n");
}

static void _handle_conn_rsp(struct tipc_virtio_dev *vds,
			     struct tipc_conn_rsp_body *rsp, size_t len)
{
	struct tipc_chan *chan;

	if (sizeof(*rsp) != len) {
		dev_info(&vds->vdev->dev, "%s: Invalid response length %zd\n",
			 __func__, len);
		return;
	}

	trusty_dbg(&vds->vdev->dev,
		"%s: connection response: for addr 0x%x: status %d remote addr 0x%x\n",
		__func__, rsp->target, rsp->status, rsp->remote);

	/* Lookup channel */
	chan = vds_lookup_channel(vds, rsp->target);
	if (chan) {
		mutex_lock(&chan->lock);
		if (chan->state == TIPC_CONNECTING) {
			if (!rsp->status) {
				chan->state = TIPC_CONNECTED;
				chan->remote = rsp->remote;
				chan->max_msg_cnt = rsp->max_msg_cnt;
				chan->max_msg_size = rsp->max_msg_size;
				chan_trigger_event(chan,
						   TIPC_CHANNEL_CONNECTED);
			} else {
				chan->state = TIPC_DISCONNECTED;
				chan->remote = 0;
				chan_trigger_event(chan,
						   TIPC_CHANNEL_DISCONNECTED);
			}
		}
		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	}
}

static void _handle_disc_req(struct tipc_virtio_dev *vds,
			     struct tipc_disc_req_body *req, size_t len)
{
	struct tipc_chan *chan;

	if (sizeof(*req) != len) {
		dev_info(&vds->vdev->dev, "%s: Invalid request length %zd\n",
			 __func__, len);
		return;
	}

	trusty_dbg(&vds->vdev->dev, "%s: disconnect request: for addr 0x%x\n",
		__func__, req->target);

	chan = vds_lookup_channel(vds, req->target);
	if (chan) {
		mutex_lock(&chan->lock);
		if (chan->state == TIPC_CONNECTED ||
		    chan->state == TIPC_CONNECTING) {
			chan->state = TIPC_DISCONNECTED;
			chan->remote = 0;
			chan_trigger_event(chan, TIPC_CHANNEL_DISCONNECTED);
		}
		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	}
}

static void _handle_ctrl_msg(struct tipc_virtio_dev *vds,
			     void *data, int len, u32 src)
{
	struct tipc_ctrl_msg *msg = data;

	if ((len < sizeof(*msg)) || (sizeof(*msg) + msg->body_len != len)) {
		dev_info(&vds->vdev->dev,
			 "%s: Invalid message length ( %d vs. %d)\n",
			 __func__, (int)(sizeof(*msg) + msg->body_len), len);
		return;
	}

	trusty_dbg(&vds->vdev->dev,
		"%s: Incoming ctrl message: src 0x%x type %d len %d\n",
		__func__, src, msg->type, msg->body_len);

	switch (msg->type) {
	case TIPC_CTRL_MSGTYPE_GO_ONLINE:
		_go_online(vds);
		break;

	case TIPC_CTRL_MSGTYPE_GO_OFFLINE:
		_go_offline(vds);
		break;

	case TIPC_CTRL_MSGTYPE_CONN_RSP:
		_handle_conn_rsp(vds, (struct tipc_conn_rsp_body *)msg->body,
				 msg->body_len);
		break;

	case TIPC_CTRL_MSGTYPE_DISC_REQ:
		_handle_disc_req(vds, (struct tipc_disc_req_body *)msg->body,
				 msg->body_len);
		break;

	default:
		dev_info(&vds->vdev->dev,
			 "%s: Unexpected message type: %d\n",
			 __func__, msg->type);
	}
}

static int _handle_rxbuf(struct tipc_virtio_dev *vds,
			 struct tipc_msg_buf *rxbuf, size_t rxlen)
{
	int err;
	struct scatterlist sg;
	struct tipc_msg_hdr *msg;
	struct device *dev = &vds->vdev->dev;

	/* message sanity check */
	if (rxlen > rxbuf->buf_sz) {
		dev_info(dev, "inbound msg is too big: %zd\n", rxlen);
		goto drop_it;
	}

	if (rxlen < sizeof(*msg)) {
		dev_info(dev, "inbound msg is too short: %zd\n", rxlen);
		goto drop_it;
	}

	/* reset buffer and put data  */
	mb_reset(rxbuf);
	mb_put_data(rxbuf, rxlen);

	/* get message header */
	msg = mb_get_data(rxbuf, sizeof(*msg));
	if (mb_avail_data(rxbuf) != msg->len) {
		dev_info(dev, "inbound msg length mismatch: (%d vs. %d)\n",
			 (uint) mb_avail_data(rxbuf), (uint) msg->len);
		goto drop_it;
	}

	trusty_dbg(dev, "From: %d, To: %d, Len: %d, Flags: 0x%x, Reserved: %d\n",
		msg->src, msg->dst, msg->len, msg->flags, msg->reserved);

	/* message directed to control endpoint is a special case */
	if (msg->dst == TIPC_CTRL_ADDR) {
		_handle_ctrl_msg(vds, msg->data, msg->len, msg->src);
	} else {
		struct tipc_chan *chan = NULL;
		/* Lookup channel */
		chan = vds_lookup_channel(vds, msg->dst);
		if (chan) {
			/* handle it */
			rxbuf = chan->ops->handle_msg(chan->ops_arg, rxbuf);
			WARN_ON(!rxbuf);
			kref_put(&chan->refcount, _free_chan);
		}
	}

drop_it:

	if (!rxbuf) {
		dev_info(dev, "rxbuf is null. failed\n");
		return -ENOMEM;
	}
	/* add the buffer back to the virtqueue */
	sg_init_one(&sg, rxbuf->buf_va, rxbuf->buf_sz);
	err = virtqueue_add_inbuf(vds->rxvq, &sg, 1, rxbuf, GFP_KERNEL);

	if (err < 0) {
		dev_info(dev, "failed to add a virtqueue buffer: %d\n", err);
		return err;
	}

	return 0;
}

static void _rxvq_cb(struct virtqueue *rxvq)
{
	unsigned int len;
	struct tipc_msg_buf *mb;
	unsigned int msg_cnt = 0;
	struct tipc_virtio_dev *vds = rxvq->vdev->priv;

	while ((mb = virtqueue_get_buf(rxvq, &len)) != NULL) {
		if (_handle_rxbuf(vds, mb, len))
			break;
		msg_cnt++;
	}

	/* tell the other size that we added rx buffers */
	if (msg_cnt)
		virtqueue_kick(rxvq);
}

static void _txvq_cb(struct virtqueue *txvq)
{
	unsigned int len;
	struct tipc_msg_buf *mb;
	bool need_wakeup = false;
	struct tipc_virtio_dev *vds = txvq->vdev->priv;

	/* detach all buffers */
	mutex_lock(&vds->lock);
	while ((mb = virtqueue_get_buf(txvq, &len)) != NULL)
		need_wakeup |= _put_txbuf_locked(vds, mb);
	mutex_unlock(&vds->lock);

	if (need_wakeup) {
		/* wake up potential senders waiting for a tx buffer */
		wake_up_interruptible_all(&vds->sendq);
	}
}

static void tee_routing_init(void)
{
	int i;

	hash_init(tee_routing_htable);

	for (i = 0; i < MAX_TEE_ROUTING_NUM &&
	     tee_routing_config[i].tee_id != TEE_ID_END; i++) {
		char *srv_name = tee_routing_config[i].srv_name;
		u32 hash_val =
		    hashlen_hash(hashlen_string(HASH_SALT, srv_name));
		pr_debug("[%s] name %s, hash_val 0x%x added\n", __func__,
			 srv_name, hash_val);
		hash_add(tee_routing_htable, &tee_routing_config[i].node,
			 hash_val);
	}
}

static int tipc_virtio_probe(struct virtio_device *vdev)
{
	int err, i;
	struct tipc_virtio_dev *vds;
	struct tipc_dev_config config;
	struct virtqueue *vqs[2];
	vq_callback_t *vq_cbs[] = { _rxvq_cb, _txvq_cb };
	char *vq_names[2];
	int tee_id = vdev->dev.id;

	dev_info(&vdev->dev, "--- init trusty-ipc for MTEE %d ---\n",
		 tee_id);

	vds = kzalloc(sizeof(*vds), GFP_KERNEL);
	if (!vds)
		return -ENOMEM;

	vds->vdev = vdev;
	vds->tee_id = tee_id;
	mutex_init(&vds->lock);
	kref_init(&vds->refcount);
	init_waitqueue_head(&vds->sendq);
	INIT_LIST_HEAD(&vds->free_buf_list);
	idr_init(&vds->addr_idr);

	/* set default max message size and alignment */
	memset(&config, 0, sizeof(config));
	config.msg_buf_max_size = DEFAULT_MSG_BUF_SIZE;
	config.msg_buf_alignment = DEFAULT_MSG_BUF_ALIGN;

	/* get configuration if present */
	vdev->config->get(vdev, 0, &config, sizeof(config));

	/* set char device name*/
	snprintf(vds->cdev_name, MAX_DEV_NAME_LEN, "%s-ipc-%s",
		 config.dev_name.tee_name, config.dev_name.cdev_name);

	/* set vqueue name */
	snprintf(vds->rxvq_name, MAX_DEV_NAME_LEN, "%s-rxvq",
		 config.dev_name.tee_name);

	snprintf(vds->txvq_name, MAX_DEV_NAME_LEN, "%s-txvq",
		 config.dev_name.tee_name);

	vq_names[0] = vds->rxvq_name;
	vq_names[1] = vds->txvq_name;
	/* find tx virtqueues (rx and tx and in this order) */
	err = vdev->config->find_vqs(vdev, 2, vqs, vq_cbs,
				     (const char **)vq_names, NULL, NULL);

	if (err)
		goto err_find_vqs;

	vds->rxvq = vqs[0];
	vds->txvq = vqs[1];

	/* save max buffer size and count */
	vds->msg_buf_max_sz = config.msg_buf_max_size;
	vds->msg_buf_max_cnt = virtqueue_get_vring_size(vds->txvq);

	/* set up the receive buffers 32 */
	for (i = 0; i < virtqueue_get_vring_size(vds->rxvq); i++) {
		struct scatterlist sg;
		struct tipc_msg_buf *rxbuf;

		rxbuf = _alloc_msg_buf(vds->msg_buf_max_sz);
		if (!rxbuf) {
			dev_info(&vdev->dev, "failed to allocate rx buffer\n");
			err = -ENOMEM;
			goto err_free_rx_buffers;
		}

		sg_init_one(&sg, rxbuf->buf_va, rxbuf->buf_sz);
		err = virtqueue_add_inbuf(vds->rxvq, &sg, 1, rxbuf, GFP_KERNEL);
		WARN_ON(err);	/* sanity check; this can't really happen */
	}

	vdev->priv = vds;
	vds->state = VDS_OFFLINE;
	trusty_dbg(&vdev->dev, "%s: done\n", __func__);
	return 0;

err_free_rx_buffers:
	_cleanup_vq(vds->rxvq);
err_find_vqs:
	kref_put(&vds->refcount, _free_vds);
	return err;
}

static void tipc_virtio_remove(struct virtio_device *vdev)
{
	struct tipc_virtio_dev *vds = vdev->priv;

	_go_offline(vds);

	mutex_lock(&vds->lock);
	vds->state = VDS_DEAD;
	vds->vdev = NULL;
	mutex_unlock(&vds->lock);

	vdev->config->reset(vdev);

	idr_destroy(&vds->addr_idr);

	_cleanup_vq(vds->rxvq);
	_cleanup_vq(vds->txvq);
	_free_msg_buf_list(&vds->free_buf_list);

	vdev->config->del_vqs(vds->vdev);

	kref_put(&vds->refcount, _free_vds);
}

static struct virtio_device_id tipc_virtio_id_table[] = {
	{VIRTIO_ID_TRUSTY_IPC, VIRTIO_DEV_ANY_ID},
	{0},
};

static unsigned int trusty_features[] = {
	0,
};

static struct virtio_driver virtio_tipc_driver = {
	.feature_table = trusty_features,
	.feature_table_size = ARRAY_SIZE(trusty_features),
	.driver.name = "trusty-virtio-tipc",
	.driver.owner = THIS_MODULE,
	.id_table = tipc_virtio_id_table,
	.probe = tipc_virtio_probe,
	.remove = tipc_virtio_remove,
};

/* The virtio device for NEBULA */
static struct virtio_device_id nebula_virtio_id_table[] = {
	{VIRTIO_ID_NEBULA_IPC, VIRTIO_DEV_ANY_ID},
	{0},
};

static unsigned int nebula_features[] = {
	0,
};

static struct virtio_driver virtio_nebula_driver = {
	.feature_table = nebula_features,
	.feature_table_size = ARRAY_SIZE(nebula_features),
	.driver.name = "nebula-virtio-tipc",
	.driver.owner = THIS_MODULE,
	.id_table = nebula_virtio_id_table,
	.probe = tipc_virtio_probe,
	.remove = tipc_virtio_remove,
};

static int __init tipc_init(void)
{
	int ret;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, 0, MAX_DEVICES, KBUILD_MODNAME);
	if (ret) {
		pr_info("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		return ret;
	}

	tipc_major = MAJOR(dev);
	tipc_class = class_create(THIS_MODULE, KBUILD_MODNAME);
	if (IS_ERR(tipc_class)) {
		ret = PTR_ERR(tipc_class);
		pr_info("%s: class_create failed: %d\n", __func__, ret);
		goto err_class_create;
	}

	/* For multiple TEEs */
	tee_routing_init();

	ret = register_virtio_driver(&virtio_tipc_driver);
	if (ret) {
		pr_info("Register virtio driver failed: %d\n", ret);
		goto err_register_virtio_drv;
	}

	ret = register_virtio_driver(&virtio_nebula_driver);
	if (ret) {
		pr_info("Register nebula virtio driver failed: %d\n", ret);
		goto err_register_nebula_drv;
	}

	return ret;

err_register_virtio_drv:
	class_destroy(tipc_class);
err_class_create:
	unregister_chrdev_region(dev, MAX_DEVICES);
err_register_nebula_drv:
	return ret;
}

static void __exit tipc_exit(void)
{
	unregister_virtio_driver(&virtio_tipc_driver);
	unregister_virtio_driver(&virtio_nebula_driver);
	class_destroy(tipc_class);
	unregister_chrdev_region(MKDEV(tipc_major, 0), MAX_DEVICES);
}

/* We need to init this early */
subsys_initcall(tipc_init);
module_exit(tipc_exit);

MODULE_DEVICE_TABLE(tipc, tipc_virtio_id_table);
MODULE_DEVICE_TABLE(tipc, nebula_virtio_id_table);

MODULE_DESCRIPTION("Trusty IPC driver");
MODULE_LICENSE("GPL v2");
