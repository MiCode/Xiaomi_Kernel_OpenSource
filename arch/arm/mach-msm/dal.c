/* Copyright (c) 2008-2011, The Linux Foundation. All rights reserved.
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
/*
 * Device access library (DAL) implementation.
 */

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#include <mach/dal.h>
#include <mach/msm_smd.h>

#define DALRPC_PROTOCOL_VERSION 0x11
#define DALRPC_SUCCESS 0
#define DALRPC_MAX_PORTNAME_LEN 64
#define DALRPC_MAX_ATTACH_PARAM_LEN 64
#define DALRPC_MAX_SERVICE_NAME_LEN 32
#define DALRPC_MAX_PARAMS 128
#define DALRPC_MAX_PARAMS_SIZE (DALRPC_MAX_PARAMS * 4)
#define DALRPC_MAX_MSG_SIZE (sizeof(struct dalrpc_msg_hdr) + \
			     DALRPC_MAX_PARAMS_SIZE)
#define DALRPC_MSGID_DDI 0x0
#define DALRPC_MSGID_DDI_REPLY 0x80
#define DALRPC_MSGID_ATTACH_REPLY 0x81
#define DALRPC_MSGID_DETACH_REPLY 0x82
#define DALRPC_MSGID_ASYNCH 0xC0
#define ROUND_BUFLEN(x) (((x + 3) & ~0x3))

struct dalrpc_msg_hdr {
	uint32_t len:16;
	uint32_t proto_ver:8;
	uint32_t prio:7;
	uint32_t async:1;
	uint32_t ddi_idx:16;
	uint32_t proto_id:8;
	uint32_t msgid:8;
	void *from;
	void *to;
};

struct dalrpc_msg {
	struct dalrpc_msg_hdr hdr;
	uint32_t param[DALRPC_MAX_PARAMS];
};

struct dalrpc_event_handle {
	struct list_head list;

	int flag;
	spinlock_t lock;
};

struct dalrpc_cb_handle {
	struct list_head list;

	void (*fn)(void *, uint32_t, void *, uint32_t);
	void *context;
};

struct daldevice_handle {;
	struct list_head list;

	void *remote_handle;
	struct completion read_completion;
	struct dalrpc_port *port;
	struct dalrpc_msg msg;
	struct mutex client_lock;
};

struct dalrpc_port {
	struct list_head list;

	char port[DALRPC_MAX_PORTNAME_LEN+1];
	int refcount;

	struct workqueue_struct *wq;
	struct work_struct port_work;
	struct mutex write_lock;

	smd_channel_t *ch;

	struct dalrpc_msg msg_in;
	struct daldevice_handle *msg_owner;
	unsigned msg_bytes_read;

	struct list_head event_list;
	struct mutex event_list_lock;

	struct list_head cb_list;
	struct mutex cb_list_lock;
};

static LIST_HEAD(port_list);
static LIST_HEAD(client_list);
static DEFINE_MUTEX(pc_lists_lock);

static DECLARE_WAIT_QUEUE_HEAD(event_wq);

static int client_exists(void *handle)
{
	struct daldevice_handle *h;

	if (!handle)
		return 0;

	mutex_lock(&pc_lists_lock);

	list_for_each_entry(h, &client_list, list)
		if (h == handle) {
			mutex_unlock(&pc_lists_lock);
			return 1;
		}

	mutex_unlock(&pc_lists_lock);

	return 0;
}

static int client_exists_locked(void *handle)
{
	struct daldevice_handle *h;

	/* this function must be called with pc_lists_lock acquired */

	if (!handle)
		return 0;

	list_for_each_entry(h, &client_list, list)
		if (h == handle)
			return 1;

	return 0;
}

static int port_exists(struct dalrpc_port *p)
{
	struct dalrpc_port *p_iter;

	/* this function must be called with pc_lists_lock acquired */

	if (!p)
		return 0;

	list_for_each_entry(p_iter, &port_list, list)
		if (p_iter == p)
			return 1;

	return 0;
}

static struct dalrpc_port *port_name_exists(char *port)
{
	struct dalrpc_port *p;

	/* this function must be called with pc_lists_lock acquired */

	list_for_each_entry(p, &port_list, list)
		if (!strcmp(p->port, port))
			return p;

	return NULL;
}

static void port_close(struct dalrpc_port *p)
{
	mutex_lock(&pc_lists_lock);

	p->refcount--;
	if (p->refcount == 0)
		list_del(&p->list);

	mutex_unlock(&pc_lists_lock);

	if (p->refcount == 0) {
		destroy_workqueue(p->wq);
		smd_close(p->ch);
		kfree(p);
	}
}

static int event_exists(struct dalrpc_port *p,
			struct dalrpc_event_handle *ev)
{
	struct dalrpc_event_handle *ev_iter;

	/* this function must be called with event_list_lock acquired */

	list_for_each_entry(ev_iter, &p->event_list, list)
		if (ev_iter == ev)
			return 1;

	return 0;
}

static int cb_exists(struct dalrpc_port *p,
		     struct dalrpc_cb_handle *cb)
{
	struct dalrpc_cb_handle *cb_iter;

	/* this function must be called with the cb_list_lock acquired */

	list_for_each_entry(cb_iter, &p->cb_list, list)
		if (cb_iter == cb)
			return 1;

	return 0;
}

static int check_version(struct dalrpc_msg_hdr *msg_hdr)
{
	static int version_msg = 1;

	/* disabled because asynch events currently have no version */
	return 0;

	if (msg_hdr->proto_ver != DALRPC_PROTOCOL_VERSION) {
		if (version_msg) {
			printk(KERN_ERR "dalrpc: incompatible verison\n");
			version_msg = 0;
		}
		return -1;
	}
	return 0;
}

static void process_asynch(struct dalrpc_port *p)
{
	struct dalrpc_event_handle *ev;
	struct dalrpc_cb_handle *cb;

	ev = (struct dalrpc_event_handle *)p->msg_in.param[0];
	cb = (struct dalrpc_cb_handle *)p->msg_in.param[0];

	mutex_lock(&p->event_list_lock);
	if (event_exists(p, ev)) {
		spin_lock(&ev->lock);
		ev->flag = 1;
		spin_unlock(&ev->lock);
		smp_mb();
		wake_up_all(&event_wq);
		mutex_unlock(&p->event_list_lock);
		return;
	}
	mutex_unlock(&p->event_list_lock);

	mutex_lock(&p->cb_list_lock);
	if (cb_exists(p, cb)) {
		cb->fn(cb->context, p->msg_in.param[1],
		       &p->msg_in.param[3], p->msg_in.param[2]);
		mutex_unlock(&p->cb_list_lock);
		return;
	}
	mutex_unlock(&p->cb_list_lock);
}

static void process_msg(struct dalrpc_port *p)
{
	switch (p->msg_in.hdr.msgid) {

	case DALRPC_MSGID_DDI_REPLY:
	case DALRPC_MSGID_ATTACH_REPLY:
	case DALRPC_MSGID_DETACH_REPLY:
		complete(&p->msg_owner->read_completion);
		break;

	case DALRPC_MSGID_ASYNCH:
		process_asynch(p);
		break;

	default:
		printk(KERN_ERR "process_msg: bad msgid %#x\n",
		       p->msg_in.hdr.msgid);
	}
}

static void flush_msg(struct dalrpc_port *p)
{
	int bytes_read, len;

	len = p->msg_in.hdr.len - sizeof(struct dalrpc_msg_hdr);
	while (len > 0) {
		bytes_read = smd_read(p->ch, NULL, len);
		if (bytes_read <= 0)
			break;
		len -= bytes_read;
	}
	p->msg_bytes_read = 0;
}

static int check_header(struct dalrpc_port *p)
{
	if (check_version(&p->msg_in.hdr) ||
	    p->msg_in.hdr.len > DALRPC_MAX_MSG_SIZE ||
	    (p->msg_in.hdr.msgid != DALRPC_MSGID_ASYNCH &&
	     !client_exists_locked(p->msg_in.hdr.to))) {
		printk(KERN_ERR "dalrpc_read_msg: bad msg\n");
		flush_msg(p);
		return 1;
	}
	p->msg_owner = (struct daldevice_handle *)p->msg_in.hdr.to;

	if (p->msg_in.hdr.msgid != DALRPC_MSGID_ASYNCH)
		memcpy(&p->msg_owner->msg.hdr, &p->msg_in.hdr,
		       sizeof(p->msg_in.hdr));

	return 0;
}

static int dalrpc_read_msg(struct dalrpc_port *p)
{
	uint8_t *read_ptr;
	int bytes_read;

	/* read msg header */
	while (p->msg_bytes_read < sizeof(p->msg_in.hdr)) {
		read_ptr = (uint8_t *)&p->msg_in.hdr + p->msg_bytes_read;

		bytes_read = smd_read(p->ch, read_ptr,
				      sizeof(p->msg_in.hdr) -
				      p->msg_bytes_read);
		if (bytes_read <= 0)
			return 0;
		p->msg_bytes_read += bytes_read;

		if (p->msg_bytes_read == sizeof(p->msg_in.hdr) &&
		    check_header(p))
			return 1;
	}

	/* read remainder of msg */
	if (p->msg_in.hdr.msgid != DALRPC_MSGID_ASYNCH)
		read_ptr = (uint8_t *)&p->msg_owner->msg;
	else
		read_ptr = (uint8_t *)&p->msg_in;
	read_ptr += p->msg_bytes_read;

	while (p->msg_bytes_read < p->msg_in.hdr.len) {
		bytes_read = smd_read(p->ch, read_ptr,
				      p->msg_in.hdr.len - p->msg_bytes_read);
		if (bytes_read <= 0)
			return 0;
		p->msg_bytes_read += bytes_read;
		read_ptr += bytes_read;
	}

	process_msg(p);
	p->msg_bytes_read = 0;
	p->msg_owner = NULL;

	return 1;
}

static void dalrpc_work(struct work_struct *work)
{
	struct dalrpc_port *p = container_of(work,
					     struct dalrpc_port,
					     port_work);

	/* must lock port/client lists to ensure port doesn't disappear
	   under an asynch event */
	mutex_lock(&pc_lists_lock);
	if (port_exists(p))
		while (dalrpc_read_msg(p))
			;
	mutex_unlock(&pc_lists_lock);
}

static void dalrpc_smd_cb(void *priv, unsigned smd_flags)
{
	struct dalrpc_port *p = priv;

	if (smd_flags != SMD_EVENT_DATA)
		return;

	queue_work(p->wq, &p->port_work);
}

static struct dalrpc_port *dalrpc_port_open(char *port, int cpu)
{
	struct dalrpc_port *p;
	char wq_name[32];

	p = port_name_exists(port);
	if (p) {
		p->refcount++;
		return p;
	}

	p = kzalloc(sizeof(struct dalrpc_port), GFP_KERNEL);
	if (!p)
		return NULL;

	strlcpy(p->port, port, sizeof(p->port));
	p->refcount = 1;

	snprintf(wq_name, sizeof(wq_name), "dalrpc_rcv_%s", port);
	p->wq = create_singlethread_workqueue(wq_name);
	if (!p->wq) {
		printk(KERN_ERR "dalrpc_init: unable to create workqueue\n");
		goto no_wq;
	}
	INIT_WORK(&p->port_work, dalrpc_work);

	mutex_init(&p->write_lock);
	mutex_init(&p->event_list_lock);
	mutex_init(&p->cb_list_lock);

	INIT_LIST_HEAD(&p->event_list);
	INIT_LIST_HEAD(&p->cb_list);

	p->msg_owner = NULL;
	p->msg_bytes_read = 0;

	if (smd_named_open_on_edge(port, cpu, &p->ch, p,
				   dalrpc_smd_cb)) {
		printk(KERN_ERR "dalrpc_port_init() failed to open port\n");
		goto no_smd;
	}

	list_add(&p->list, &port_list);

	return p;

no_smd:
	destroy_workqueue(p->wq);
no_wq:
	kfree(p);
	return NULL;
}

static void dalrpc_sendwait(struct daldevice_handle *h)
{
	u8 *buf = (u8 *)&h->msg;
	int len = h->msg.hdr.len;
	int written;

	mutex_lock(&h->port->write_lock);
	do {
		written = smd_write(h->port->ch, buf + (h->msg.hdr.len - len),
				 len);
		if (written < 0)
			break;
		len -= written;
	} while (len);
	mutex_unlock(&h->port->write_lock);

	if (!h->msg.hdr.async)
		wait_for_completion(&h->read_completion);
}

int daldevice_attach(uint32_t device_id, char *port, int cpu,
		     void **handle_ptr)
{
	struct daldevice_handle *h;
	char dyn_port[DALRPC_MAX_PORTNAME_LEN + 1] = "DAL00";
	int ret;
	int tries = 0;

	if (!port)
		port = dyn_port;

	if (strlen(port) > DALRPC_MAX_PORTNAME_LEN)
		return -EINVAL;

	h = kzalloc(sizeof(struct daldevice_handle), GFP_KERNEL);
	if (!h) {
		*handle_ptr = NULL;
		return -ENOMEM;
	}

	init_completion(&h->read_completion);
	mutex_init(&h->client_lock);

	mutex_lock(&pc_lists_lock);
	list_add(&h->list, &client_list);
	mutex_unlock(&pc_lists_lock);

	/* 3 attempts, enough for one each on the user specified port, the
	 * dynamic discovery port, and the port recommended by the dynamic
	 * discovery port */
	while (tries < 3) {
		tries++;

		mutex_lock(&pc_lists_lock);
		h->port = dalrpc_port_open(port, cpu);
		if (!h->port) {
			list_del(&h->list);
			mutex_unlock(&pc_lists_lock);
			printk(KERN_ERR "daldevice_attach: could not "
			       "open port\n");
			kfree(h);
			*handle_ptr = NULL;
			return -EIO;
		}
		mutex_unlock(&pc_lists_lock);

		h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4 +
			DALRPC_MAX_ATTACH_PARAM_LEN +
			DALRPC_MAX_SERVICE_NAME_LEN;
		h->msg.hdr.proto_ver = DALRPC_PROTOCOL_VERSION;
		h->msg.hdr.ddi_idx = 0;
		h->msg.hdr.msgid = 0x1;
		h->msg.hdr.prio = 0;
		h->msg.hdr.async = 0;
		h->msg.hdr.from = h;
		h->msg.hdr.to = 0;
		h->msg.param[0] = device_id;

		memset(&h->msg.param[1], 0,
		       DALRPC_MAX_ATTACH_PARAM_LEN +
		       DALRPC_MAX_SERVICE_NAME_LEN);

		dalrpc_sendwait(h);
		ret = h->msg.param[0];

		if (ret == DALRPC_SUCCESS) {
			h->remote_handle = h->msg.hdr.from;
			*handle_ptr = h;
			break;
		} else if (strnlen((char *)&h->msg.param[1],
				   DALRPC_MAX_PORTNAME_LEN)) {
			/* another port was recommended in the response. */
			strlcpy(dyn_port, (char *)&h->msg.param[1],
				sizeof(dyn_port));
			dyn_port[DALRPC_MAX_PORTNAME_LEN] = 0;
			port = dyn_port;
		} else if (port == dyn_port) {
			/* the dynamic discovery port (or port that
			 * was recommended by it) did not recognize
			 * the device id, give up */
			daldevice_detach(h);
			break;
		} else
			/* the user specified port did not work, try
			 * the dynamic discovery port */
			port = dyn_port;

		port_close(h->port);
	}

	return ret;
}
EXPORT_SYMBOL(daldevice_attach);

static void dalrpc_ddi_prologue(uint32_t ddi_idx, struct daldevice_handle *h,
							uint32_t idx_async)
{
	h->msg.hdr.proto_ver = DALRPC_PROTOCOL_VERSION;
	h->msg.hdr.prio = 0;
	h->msg.hdr.async = idx_async;
	h->msg.hdr.msgid = DALRPC_MSGID_DDI;
	h->msg.hdr.from = h;
	h->msg.hdr.to = h->remote_handle;
	h->msg.hdr.ddi_idx = ddi_idx;
}

int daldevice_detach(void *handle)
{
	struct daldevice_handle *h = handle;

	if (!client_exists(h))
		return -EINVAL;

	dalrpc_ddi_prologue(0, h, 0);

	if (!h->remote_handle)
		goto norpc;

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4;
	h->msg.hdr.msgid = 0x2;
	h->msg.param[0] = 0;

	dalrpc_sendwait(h);

norpc:
	mutex_lock(&pc_lists_lock);
	list_del(&h->list);
	mutex_unlock(&pc_lists_lock);

	port_close(h->port);

	kfree(h);

	return 0;
}
EXPORT_SYMBOL(daldevice_detach);

uint32_t dalrpc_fcn_0(uint32_t ddi_idx, void *handle, uint32_t s1)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4;
	h->msg.hdr.proto_id = 0;
	h->msg.param[0] = s1;

	dalrpc_sendwait(h);

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_0);

uint32_t dalrpc_fcn_1(uint32_t ddi_idx, void *handle, uint32_t s1,
		      uint32_t s2)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8;
	h->msg.hdr.proto_id = 1;
	h->msg.param[0] = s1;
	h->msg.param[1] = s2;

	dalrpc_sendwait(h);

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_1);

uint32_t dalrpc_fcn_2(uint32_t ddi_idx, void *handle, uint32_t s1,
		      uint32_t *p_s2)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4;
	h->msg.hdr.proto_id = 2;
	h->msg.param[0] = s1;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS)
		*p_s2 = h->msg.param[1];

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_2);

uint32_t dalrpc_fcn_3(uint32_t ddi_idx, void *handle, uint32_t s1,
		      uint32_t s2, uint32_t s3)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 12;
	h->msg.hdr.proto_id = 3;
	h->msg.param[0] = s1;
	h->msg.param[1] = s2;
	h->msg.param[2] = s3;

	dalrpc_sendwait(h);

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_3);

uint32_t dalrpc_fcn_4(uint32_t ddi_idx, void *handle, uint32_t s1,
		      uint32_t s2, uint32_t *p_s3)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8;
	h->msg.hdr.proto_id = 4;
	h->msg.param[0] = s1;
	h->msg.param[1] = s2;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS)
		*p_s3 = h->msg.param[1];

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_4);

uint32_t dalrpc_fcn_5(uint32_t ddi_idx, void *handle, const void *ibuf,
		      uint32_t ilen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret, idx_async;

	if ((ilen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	idx_async = (ddi_idx & 0x80000000) >> 31;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, idx_async);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 5;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);

	dalrpc_sendwait(h);

	if (h->msg.hdr.async)
		ret = DALRPC_SUCCESS;
	else
		ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_5);

uint32_t dalrpc_fcn_6(uint32_t ddi_idx, void *handle, uint32_t s1,
		      const void *ibuf, uint32_t ilen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if ((ilen + 8) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 6;
	h->msg.param[0] = s1;
	h->msg.param[1] = ilen;
	memcpy(&h->msg.param[2], ibuf, ilen);

	dalrpc_sendwait(h);

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_6);

uint32_t dalrpc_fcn_7(uint32_t ddi_idx, void *handle, const void *ibuf,
		      uint32_t ilen, void *obuf, uint32_t olen,
		      uint32_t *oalen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + 8) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;


	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 7;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 1;
	h->msg.param[param_idx] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		*oalen = h->msg.param[1];
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_7);

uint32_t dalrpc_fcn_8(uint32_t ddi_idx, void *handle, const void *ibuf,
		      uint32_t ilen, void *obuf, uint32_t olen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + 8) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 8;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 1;
	h->msg.param[param_idx] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_8);

uint32_t dalrpc_fcn_9(uint32_t ddi_idx, void *handle, void *obuf,
		      uint32_t olen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if ((olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 4;
	h->msg.hdr.proto_id = 9;
	h->msg.param[0] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_9);

uint32_t dalrpc_fcn_10(uint32_t ddi_idx, void *handle, uint32_t s1,
		       const void *ibuf, uint32_t ilen, void *obuf,
		       uint32_t olen, uint32_t *oalen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + 12) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 12 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 10;
	h->msg.param[0] = s1;
	h->msg.param[1] = ilen;
	memcpy(&h->msg.param[2], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 2;
	h->msg.param[param_idx] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		*oalen = h->msg.param[1];
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_10);

uint32_t dalrpc_fcn_11(uint32_t ddi_idx, void *handle, uint32_t s1,
		       void *obuf, uint32_t olen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if ((olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8;
	h->msg.hdr.proto_id = 11;
	h->msg.param[0] = s1;
	h->msg.param[1] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_11);

uint32_t dalrpc_fcn_12(uint32_t ddi_idx, void *handle, uint32_t s1,
		       void *obuf, uint32_t olen, uint32_t *oalen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;

	if ((olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 8;
	h->msg.hdr.proto_id = 12;
	h->msg.param[0] = s1;
	h->msg.param[1] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		*oalen = h->msg.param[1];
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_12);

uint32_t dalrpc_fcn_13(uint32_t ddi_idx, void *handle, const void *ibuf,
		       uint32_t ilen, const void *ibuf2, uint32_t ilen2,
		       void *obuf, uint32_t olen)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + ilen2 + 12) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + 4) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 12 +
		ROUND_BUFLEN(ilen) + ROUND_BUFLEN(ilen2);
	h->msg.hdr.proto_id = 13;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 1;
	h->msg.param[param_idx++] = ilen2;
	memcpy(&h->msg.param[param_idx], ibuf2, ilen2);
	param_idx += (ROUND_BUFLEN(ilen2) / 4);
	h->msg.param[param_idx] = olen;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_13);

uint32_t dalrpc_fcn_14(uint32_t ddi_idx, void *handle, const void *ibuf,
		       uint32_t ilen, void *obuf, uint32_t olen,
		       void *obuf2, uint32_t olen2, uint32_t *oalen2)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + 12) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + olen2 + 8) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 12 +
		ROUND_BUFLEN(ilen);
	h->msg.hdr.proto_id = 14;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 1;
	h->msg.param[param_idx++] = olen;
	h->msg.param[param_idx] = olen2;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		param_idx = (ROUND_BUFLEN(h->msg.param[1]) / 4) + 2;
		if (h->msg.param[param_idx] > olen2) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
		memcpy(obuf2, &h->msg.param[param_idx + 1],
		       h->msg.param[param_idx]);
		*oalen2 = h->msg.param[param_idx];
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_14);

uint32_t dalrpc_fcn_15(uint32_t ddi_idx, void *handle, const void *ibuf,
		       uint32_t ilen, const void *ibuf2, uint32_t ilen2,
		       void *obuf, uint32_t olen, uint32_t *oalen,
		       void *obuf2, uint32_t olen2)
{
	struct daldevice_handle *h = handle;
	uint32_t ret;
	int param_idx;

	if ((ilen + ilen2 + 16) > DALRPC_MAX_PARAMS_SIZE ||
	    (olen + olen2 + 8) > DALRPC_MAX_PARAMS_SIZE)
		return -EINVAL;

	if (!client_exists(h))
		return -EINVAL;

	mutex_lock(&h->client_lock);

	dalrpc_ddi_prologue(ddi_idx, h, 0);

	h->msg.hdr.len = sizeof(struct dalrpc_msg_hdr) + 16 +
		ROUND_BUFLEN(ilen) + ROUND_BUFLEN(ilen2);
	h->msg.hdr.proto_id = 15;
	h->msg.param[0] = ilen;
	memcpy(&h->msg.param[1], ibuf, ilen);
	param_idx = (ROUND_BUFLEN(ilen) / 4) + 1;
	h->msg.param[param_idx++] = ilen2;
	memcpy(&h->msg.param[param_idx], ibuf2, ilen2);
	param_idx += (ROUND_BUFLEN(ilen2) / 4);
	h->msg.param[param_idx++] = olen;
	h->msg.param[param_idx] = olen2;

	dalrpc_sendwait(h);

	if (h->msg.param[0] == DALRPC_SUCCESS) {
		if (h->msg.param[1] > olen) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		param_idx = (ROUND_BUFLEN(h->msg.param[1]) / 4) + 2;
		if (h->msg.param[param_idx] > olen2) {
			mutex_unlock(&h->client_lock);
			return -EIO;
		}
		memcpy(obuf, &h->msg.param[2], h->msg.param[1]);
		memcpy(obuf2, &h->msg.param[param_idx + 1],
		       h->msg.param[param_idx]);
		*oalen = h->msg.param[1];
	}

	ret = h->msg.param[0];
	mutex_unlock(&h->client_lock);
	return ret;
}
EXPORT_SYMBOL(dalrpc_fcn_15);

void *dalrpc_alloc_event(void *handle)
{
	struct daldevice_handle *h;
	struct dalrpc_event_handle *ev;

	h = (struct daldevice_handle *)handle;

	if (!client_exists(h))
		return NULL;

	ev = kmalloc(sizeof(struct dalrpc_event_handle), GFP_KERNEL);
	if (!ev)
		return NULL;

	ev->flag = 0;
	spin_lock_init(&ev->lock);

	mutex_lock(&h->port->event_list_lock);
	list_add(&ev->list, &h->port->event_list);
	mutex_unlock(&h->port->event_list_lock);

	return ev;
}
EXPORT_SYMBOL(dalrpc_alloc_event);

void *dalrpc_alloc_cb(void *handle,
		      void (*fn)(void *, uint32_t, void *, uint32_t),
		      void *context)
{
	struct daldevice_handle *h;
	struct dalrpc_cb_handle *cb;

	h = (struct daldevice_handle *)handle;

	if (!client_exists(h))
		return NULL;

	cb = kmalloc(sizeof(struct dalrpc_cb_handle), GFP_KERNEL);
	if (!cb)
		return NULL;

	cb->fn = fn;
	cb->context = context;

	mutex_lock(&h->port->cb_list_lock);
	list_add(&cb->list, &h->port->cb_list);
	mutex_unlock(&h->port->cb_list_lock);

	return cb;
}
EXPORT_SYMBOL(dalrpc_alloc_cb);

void dalrpc_dealloc_event(void *handle,
			  void *ev_h)
{
	struct daldevice_handle *h;
	struct dalrpc_event_handle *ev;

	h = (struct daldevice_handle *)handle;
	ev = (struct dalrpc_event_handle *)ev_h;

	mutex_lock(&h->port->event_list_lock);
	list_del(&ev->list);
	mutex_unlock(&h->port->event_list_lock);
	kfree(ev);
}
EXPORT_SYMBOL(dalrpc_dealloc_event);

void dalrpc_dealloc_cb(void *handle,
		       void *cb_h)
{
	struct daldevice_handle *h;
	struct dalrpc_cb_handle *cb;

	h = (struct daldevice_handle *)handle;
	cb = (struct dalrpc_cb_handle *)cb_h;

	mutex_lock(&h->port->cb_list_lock);
	list_del(&cb->list);
	mutex_unlock(&h->port->cb_list_lock);
	kfree(cb);
}
EXPORT_SYMBOL(dalrpc_dealloc_cb);

static int event_occurred(int num_events, struct dalrpc_event_handle **events,
			  int *occurred)
{
	int i;

	for (i = 0; i < num_events; i++) {
		spin_lock(&events[i]->lock);
		if (events[i]->flag) {
			events[i]->flag = 0;
			spin_unlock(&events[i]->lock);
			*occurred = i;
			return 1;
		}
		spin_unlock(&events[i]->lock);
	}

	return 0;
}

int dalrpc_event_wait_multiple(int num, void **ev_h, int timeout)
{
	struct dalrpc_event_handle **events;
	int ret, occurred;

	events = (struct dalrpc_event_handle **)ev_h;

	if (timeout == DALRPC_TIMEOUT_INFINITE) {
		wait_event(event_wq,
			   event_occurred(num, events, &occurred));
		return occurred;
	}

	ret = wait_event_timeout(event_wq,
				 event_occurred(num, events, &occurred),
				 timeout);
	if (ret > 0)
		return occurred;
	else
		return -ETIMEDOUT;
}
EXPORT_SYMBOL(dalrpc_event_wait_multiple);
