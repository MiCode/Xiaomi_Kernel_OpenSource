/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/ipc_logging.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/uio.h>
#include <soc/qcom/glink.h>
#include <soc/qcom/tracer_pkt.h>
#include "glink_loopback_commands.h"
#include "glink_private.h"

enum {
	INFO_MASK = 1U << 0,
	DEBUG_MASK = 1U << 1,
	PERF_MASK = 1U << 2,
};

static int glink_lbsrv_debug_mask;
module_param_named(debug_mask, glink_lbsrv_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define LBSRV_PERF(fmt, args...) do {	\
	if (glink_lbsrv_debug_mask & PERF_MASK) \
		GLINK_IPC_LOG_STR("<LBSRV> " fmt, args); \
} while (0)

#define LBSRV_INFO(x...) do { \
	if (glink_lbsrv_debug_mask & INFO_MASK) \
		GLINK_INFO("<LBSRV> " x); \
} while (0)

#define LBSRV_INFO_PERF(fmt, args...) do {	\
	if (glink_lbsrv_debug_mask & (INFO_MASK | PERF_MASK)) \
		GLINK_IPC_LOG_STR("<LBSRV> " fmt, args); \
} while (0)

#define LBSRV_DBG(x...) do { \
	if (glink_lbsrv_debug_mask & DEBUG_MASK) \
		GLINK_IPC_LOG_STR("<LBSRV> " x); \
} while (0)

#define LBSRV_ERR(x...) do {                              \
	if (!(glink_lbsrv_debug_mask & PERF_MASK)) \
		pr_err("<LBSRV> " x); \
	GLINK_IPC_LOG_STR("<LBSRV> " x);  \
} while (0)

enum ch_type {
	CTL,
	DATA,
};

enum buf_type {
	LINEAR,
	VECTOR,
};

struct tx_config_info {
	uint32_t random_delay;
	uint32_t delay_ms;
	uint32_t echo_count;
	uint32_t transform_type;
};

struct rx_done_config_info {
	uint32_t random_delay;
	uint32_t delay_ms;
};

struct rmt_rx_intent_req_work_info {
	size_t req_intent_size;
	struct delayed_work work;
	struct ch_info *work_ch_info;
};

struct queue_rx_intent_work_info {
	uint32_t req_id;
	bool deferred;
	struct ch_info *req_ch_info;
	uint32_t num_intents;
	uint32_t intent_size;
	uint32_t random_delay;
	uint32_t delay_ms;
	struct delayed_work work;
	struct ch_info *work_ch_info;
};

struct lbsrv_vec {
	uint32_t num_bufs;
	struct kvec vec[0];
};

struct tx_work_info {
	struct tx_config_info tx_config;
	struct delayed_work work;
	struct ch_info *tx_ch_info;
	void *data;
	bool tracer_pkt;
	uint32_t buf_type;
	size_t size;
	void * (*vbuf_provider)(void *iovec, size_t offset, size_t *size);
	void * (*pbuf_provider)(void *iovec, size_t offset, size_t *size);
};

struct rx_done_work_info {
	struct delayed_work work;
	struct ch_info *rx_done_ch_info;
	void *ptr;
};

struct rx_work_info {
	struct ch_info *rx_ch_info;
	void *pkt_priv;
	void *ptr;
	bool tracer_pkt;
	uint32_t buf_type;
	size_t size;
	void * (*vbuf_provider)(void *iovec, size_t offset, size_t *size);
	void * (*pbuf_provider)(void *iovec, size_t offset, size_t *size);
	struct delayed_work work;
};

struct ch_info {
	struct list_head list;
	struct mutex ch_info_lock;
	char name[MAX_NAME_LEN];
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	void *handle;
	bool fully_opened;
	uint32_t type;
	struct delayed_work open_work;
	struct delayed_work close_work;
	struct tx_config_info tx_config;
	struct rx_done_config_info rx_done_config;
	struct queue_rx_intent_work_info *queue_rx_intent_work_info;
};

struct ctl_ch_info {
	char name[MAX_NAME_LEN];
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
};

static struct ctl_ch_info ctl_ch_tbl[] = {
	{"LOCAL_LOOPBACK_SRV", "local", "lloop"},
	{"LOOPBACK_CTL_APSS", "mpss", "smem"},
	{"LOOPBACK_CTL_APSS", "lpass", "smem"},
	{"LOOPBACK_CTL_APSS", "dsps", "smem"},
};

static DEFINE_MUTEX(ctl_ch_list_lock);
static LIST_HEAD(ctl_ch_list);
static DEFINE_MUTEX(data_ch_list_lock);
static LIST_HEAD(data_ch_list);

struct workqueue_struct *glink_lbsrv_wq;

/**
 * link_state_work_info - Information about work handling link state updates
 * edge:	Remote subsystem name in the link.
 * transport:	Name of the transport/link.
 * link_state:	State of the transport/link.
 * work:	Reference to the work item.
 */
struct link_state_work_info {
	char edge[GLINK_NAME_SIZE];
	char transport[GLINK_NAME_SIZE];
	enum glink_link_state link_state;
	struct delayed_work work;
};

static void glink_lbsrv_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv);
static struct glink_link_info glink_lbsrv_link_info = {
			NULL, NULL, glink_lbsrv_link_state_cb};
static void *glink_lbsrv_link_state_notif_handle;

static void glink_lbsrv_open_worker(struct work_struct *work);
static void glink_lbsrv_close_worker(struct work_struct *work);
static void glink_lbsrv_rmt_rx_intent_req_worker(struct work_struct *work);
static void glink_lbsrv_queue_rx_intent_worker(struct work_struct *work);
static void glink_lbsrv_rx_worker(struct work_struct *work);
static void glink_lbsrv_rx_done_worker(struct work_struct *work);
static void glink_lbsrv_tx_worker(struct work_struct *work);

int glink_lbsrv_send_response(void *handle, uint32_t req_id, uint32_t req_type,
		uint32_t response)
{
	struct resp *resp_pkt = kzalloc(sizeof(struct resp), GFP_KERNEL);

	if (!resp_pkt) {
		LBSRV_ERR("%s: Error allocating response packet\n", __func__);
		return -ENOMEM;
	}

	resp_pkt->req_id = req_id;
	resp_pkt->req_type = req_type;
	resp_pkt->response = response;

	return glink_tx(handle, (void *)LINEAR, (void *)resp_pkt,
			sizeof(struct resp), 0);
}

static uint32_t calc_delay_ms(uint32_t random_delay, uint32_t delay_ms)
{
	uint32_t tmp_delay_ms;

	if (random_delay && delay_ms)
		tmp_delay_ms = prandom_u32() % delay_ms;
	else if (random_delay)
		tmp_delay_ms = prandom_u32();
	else
		tmp_delay_ms = delay_ms;

	return tmp_delay_ms;
}

static int create_ch_info(char *name, char *edge, char *transport,
			  uint32_t type, struct ch_info **ret_ch_info)
{
	struct ch_info *tmp_ch_info;

	tmp_ch_info = kzalloc(sizeof(struct ch_info), GFP_KERNEL);
	if (!tmp_ch_info) {
		LBSRV_ERR("%s: Error allocation ch_info\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&tmp_ch_info->list);
	mutex_init(&tmp_ch_info->ch_info_lock);
	strlcpy(tmp_ch_info->name, name, MAX_NAME_LEN);
	strlcpy(tmp_ch_info->edge, edge, GLINK_NAME_SIZE);
	strlcpy(tmp_ch_info->transport, transport, GLINK_NAME_SIZE);
	tmp_ch_info->type = type;
	INIT_DELAYED_WORK(&tmp_ch_info->open_work,
			  glink_lbsrv_open_worker);
	INIT_DELAYED_WORK(&tmp_ch_info->close_work,
			  glink_lbsrv_close_worker);
	tmp_ch_info->tx_config.echo_count = 1;

	if (type == CTL) {
		mutex_lock(&ctl_ch_list_lock);
		list_add_tail(&tmp_ch_info->list, &ctl_ch_list);
		mutex_unlock(&ctl_ch_list_lock);
	} else if (type == DATA) {
		mutex_lock(&data_ch_list_lock);
		list_add_tail(&tmp_ch_info->list, &data_ch_list);
		mutex_unlock(&data_ch_list_lock);
	} else {
		LBSRV_ERR("%s:%s:%s %s: Invalid ch type %d\n", transport,
				edge, name, __func__, type);
		kfree(tmp_ch_info);
		return -EINVAL;
	}
	*ret_ch_info = tmp_ch_info;
	return 0;
}

struct ch_info *lookup_ch_list(char *name, char *edge, char *transport,
			       uint32_t type)
{
	struct list_head *ch_list;
	struct mutex *lock;
	struct ch_info *tmp_ch_info;

	if (type == DATA) {
		ch_list = &data_ch_list;
		lock = &data_ch_list_lock;
	} else if (type == CTL) {
		ch_list = &ctl_ch_list;
		lock = &ctl_ch_list_lock;
	} else {
		LBSRV_ERR("%s:%s:%s %s: Invalid ch type %d\n", transport,
			    edge, name, __func__, type);
		return NULL;
	}

	mutex_lock(lock);
	list_for_each_entry(tmp_ch_info, ch_list, list) {
		if (!strcmp(name, tmp_ch_info->name) &&
		    !strcmp(edge, tmp_ch_info->edge) &&
		    !strcmp(transport, tmp_ch_info->transport)) {
			mutex_unlock(lock);
			return tmp_ch_info;
		}
	}
	mutex_unlock(lock);
	return NULL;
}

int glink_lbsrv_handle_open_req(struct ch_info *rx_ch_info,
				struct open_req req)
{
	struct ch_info *tmp_ch_info;
	int ret;
	char name[MAX_NAME_LEN];
	char *temp;

	strlcpy(name, req.ch_name, MAX_NAME_LEN);
	if (!strcmp(rx_ch_info->transport, "lloop")) {
		temp = strnstr(name, "_CLNT", MAX_NAME_LEN);
		if (temp)
			*temp = '\0';
		strlcat(name, "_SRV", MAX_NAME_LEN);
	}
	LBSRV_INFO("%s:%s:%s %s: delay_ms[%d]\n",
		   rx_ch_info->transport, rx_ch_info->edge,
		   name, __func__, req.delay_ms);
	tmp_ch_info = lookup_ch_list(name, rx_ch_info->edge,
				     rx_ch_info->transport, DATA);
	if (tmp_ch_info)
		goto queue_open_work;

	ret = create_ch_info(name, rx_ch_info->edge, rx_ch_info->transport,
			     DATA, &tmp_ch_info);
	if (ret)
		return ret;
queue_open_work:
	queue_delayed_work(glink_lbsrv_wq, &tmp_ch_info->open_work,
			   msecs_to_jiffies(req.delay_ms));
	return 0;
}

int glink_lbsrv_handle_close_req(struct ch_info *rx_ch_info,
				 struct close_req req)
{
	struct ch_info *tmp_ch_info;
	char name[MAX_NAME_LEN];
	char *temp;

	strlcpy(name, req.ch_name, MAX_NAME_LEN);
	if (!strcmp(rx_ch_info->transport, "lloop")) {
		temp = strnstr(name, "_CLNT", MAX_NAME_LEN);
		if (temp)
			*temp = '\0';
		strlcat(name, "_SRV", MAX_NAME_LEN);
	}
	LBSRV_INFO("%s:%s:%s %s: delay_ms[%d]\n",
		    rx_ch_info->transport, rx_ch_info->edge,
		    name, __func__, req.delay_ms);
	tmp_ch_info = lookup_ch_list(name, rx_ch_info->edge,
				     rx_ch_info->transport, DATA);
	if (tmp_ch_info)
		queue_delayed_work(glink_lbsrv_wq, &tmp_ch_info->close_work,
				   msecs_to_jiffies(req.delay_ms));
	return 0;
}

int glink_lbsrv_handle_queue_rx_intent_config_req(struct ch_info *rx_ch_info,
			struct queue_rx_intent_config_req req, uint32_t req_id)
{
	struct ch_info *tmp_ch_info;
	struct queue_rx_intent_work_info *tmp_work_info;
	char name[MAX_NAME_LEN];
	char *temp;
	uint32_t delay_ms;

	strlcpy(name, req.ch_name, MAX_NAME_LEN);
	if (!strcmp(rx_ch_info->transport, "lloop")) {
		temp = strnstr(name, "_CLNT", MAX_NAME_LEN);
		if (temp)
			*temp = '\0';
		strlcat(name, "_SRV", MAX_NAME_LEN);
	}
	LBSRV_INFO("%s:%s:%s %s: num_intents[%d] size[%d]\n",
		   rx_ch_info->transport, rx_ch_info->edge, name, __func__,
		   req.num_intents, req.intent_size);
	tmp_ch_info = lookup_ch_list(name, rx_ch_info->edge,
				     rx_ch_info->transport, DATA);
	if (!tmp_ch_info) {
		LBSRV_ERR("%s:%s:%s %s: Channel info not found\n",
				rx_ch_info->transport, rx_ch_info->edge,
				name, __func__);
		return -EINVAL;
	}

	tmp_work_info = kzalloc(sizeof(struct queue_rx_intent_work_info),
				GFP_KERNEL);
	if (!tmp_work_info) {
		LBSRV_ERR("%s: Error allocating work_info\n", __func__);
		return -ENOMEM;
	}

	tmp_work_info->req_id = req_id;
	tmp_work_info->req_ch_info = rx_ch_info;
	tmp_work_info->num_intents = req.num_intents;
	tmp_work_info->intent_size = req.intent_size;
	tmp_work_info->random_delay =  req.random_delay;
	tmp_work_info->delay_ms = req.delay_ms;
	INIT_DELAYED_WORK(&tmp_work_info->work,
			  glink_lbsrv_queue_rx_intent_worker);
	tmp_work_info->work_ch_info = tmp_ch_info;

	mutex_lock(&tmp_ch_info->ch_info_lock);
	if (tmp_ch_info->fully_opened) {
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		delay_ms = calc_delay_ms(tmp_work_info->random_delay,
					 tmp_work_info->delay_ms);
		queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work,
				   msecs_to_jiffies(delay_ms));

		if (tmp_work_info->random_delay || tmp_work_info->delay_ms)
			glink_lbsrv_send_response(rx_ch_info->handle, req_id,
					QUEUE_RX_INTENT_CONFIG, 0);
	} else {
		tmp_work_info->deferred = true;
		tmp_ch_info->queue_rx_intent_work_info = tmp_work_info;
		mutex_unlock(&tmp_ch_info->ch_info_lock);

		glink_lbsrv_send_response(rx_ch_info->handle, req_id,
				QUEUE_RX_INTENT_CONFIG, 0);
	}

	return 0;
}

int glink_lbsrv_handle_tx_config_req(struct ch_info *rx_ch_info,
				     struct tx_config_req req)
{
	struct ch_info *tmp_ch_info;
	char name[MAX_NAME_LEN];
	char *temp;

	strlcpy(name, req.ch_name, MAX_NAME_LEN);
	if (!strcmp(rx_ch_info->transport, "lloop")) {
		temp = strnstr(name, "_CLNT", MAX_NAME_LEN);
		if (temp)
			*temp = '\0';
		strlcat(name, "_SRV", MAX_NAME_LEN);
	}
	LBSRV_INFO("%s:%s:%s %s: echo_count[%d] transform[%d]\n",
		   rx_ch_info->transport, rx_ch_info->edge, name, __func__,
		   req.echo_count, req.transform_type);
	tmp_ch_info = lookup_ch_list(name, rx_ch_info->edge,
				     rx_ch_info->transport, DATA);
	if (!tmp_ch_info) {
		LBSRV_ERR("%s:%s:%s %s: Channel info not found\n",
				rx_ch_info->transport, rx_ch_info->edge,
				name, __func__);
		return -EINVAL;
	}

	mutex_lock(&tmp_ch_info->ch_info_lock);
	tmp_ch_info->tx_config.random_delay = req.random_delay;
	tmp_ch_info->tx_config.delay_ms = req.delay_ms;
	tmp_ch_info->tx_config.echo_count = req.echo_count;
	tmp_ch_info->tx_config.transform_type = req.transform_type;
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	return 0;
}

int glink_lbsrv_handle_rx_done_config_req(struct ch_info *rx_ch_info,
					  struct rx_done_config_req req)
{
	struct ch_info *tmp_ch_info;
	char name[MAX_NAME_LEN];
	char *temp;

	strlcpy(name, req.ch_name, MAX_NAME_LEN);
	if (!strcmp(rx_ch_info->transport, "lloop")) {
		temp = strnstr(name, "_CLNT", MAX_NAME_LEN);
		if (temp)
			*temp = '\0';
		strlcat(name, "_SRV", MAX_NAME_LEN);
	}
	LBSRV_INFO("%s:%s:%s %s: delay_ms[%d] random_delay[%d]\n",
		   rx_ch_info->transport, rx_ch_info->edge, name,
		   __func__, req.delay_ms, req.random_delay);
	tmp_ch_info = lookup_ch_list(name, rx_ch_info->edge,
				     rx_ch_info->transport, DATA);
	if (!tmp_ch_info) {
		LBSRV_ERR("%s:%s:%s %s: Channel info not found\n",
				rx_ch_info->transport, rx_ch_info->edge,
				name, __func__);
		return -EINVAL;
	}

	mutex_lock(&tmp_ch_info->ch_info_lock);
	tmp_ch_info->rx_done_config.random_delay = req.random_delay;
	tmp_ch_info->rx_done_config.delay_ms = req.delay_ms;
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	return 0;
}

/**
 * glink_lbsrv_handle_req() - Handle the request commands received by clients
 *
 * rx_ch_info:	Channel info on which the request is received
 * pkt:	Request structure received from client
 *
 * This function handles the all supported request types received from client
 * and send the response back to client
 */
void glink_lbsrv_handle_req(struct ch_info *rx_ch_info, struct req pkt)
{
	int ret;

	LBSRV_INFO("%s:%s:%s %s: Request packet type[%d]:id[%d]\n",
			rx_ch_info->transport, rx_ch_info->edge,
			rx_ch_info->name, __func__, pkt.hdr.req_type,
			pkt.hdr.req_id);
	switch (pkt.hdr.req_type) {
	case OPEN:
		ret = glink_lbsrv_handle_open_req(rx_ch_info,
						  pkt.payload.open);
		break;
	case CLOSE:
		ret = glink_lbsrv_handle_close_req(rx_ch_info,
						   pkt.payload.close);
		break;
	case QUEUE_RX_INTENT_CONFIG:
		ret = glink_lbsrv_handle_queue_rx_intent_config_req(
			rx_ch_info, pkt.payload.q_rx_int_conf, pkt.hdr.req_id);
		break;
	case TX_CONFIG:
		ret = glink_lbsrv_handle_tx_config_req(rx_ch_info,
						       pkt.payload.tx_conf);
		break;
	case RX_DONE_CONFIG:
		ret = glink_lbsrv_handle_rx_done_config_req(rx_ch_info,
						pkt.payload.rx_done_conf);
		break;
	default:
		LBSRV_ERR("%s:%s:%s %s: Invalid Request type [%d]\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__, pkt.hdr.req_type);
		ret = -1;
		break;
	}

	if (pkt.hdr.req_type != QUEUE_RX_INTENT_CONFIG)
		glink_lbsrv_send_response(rx_ch_info->handle, pkt.hdr.req_id,
				pkt.hdr.req_type, ret);
}

static void *glink_lbsrv_vbuf_provider(void *iovec, size_t offset,
				       size_t *buf_size)
{
	struct lbsrv_vec *tmp_vec_info = (struct lbsrv_vec *)iovec;
	uint32_t i;
	size_t temp_size = 0;

	for (i = 0; i < tmp_vec_info->num_bufs; i++) {
		temp_size += tmp_vec_info->vec[i].iov_len;
		if (offset >= temp_size)
			continue;
		*buf_size = temp_size - offset;
		return (void *)tmp_vec_info->vec[i].iov_base +
			tmp_vec_info->vec[i].iov_len - *buf_size;
	}
	*buf_size = 0;
	return NULL;
}

static void glink_lbsrv_free_data(void *data, uint32_t buf_type)
{
	struct lbsrv_vec *tmp_vec_info;
	uint32_t i;

	if (buf_type == LINEAR) {
		kfree(data);
	} else {
		tmp_vec_info = (struct lbsrv_vec *)data;
		for (i = 0; i < tmp_vec_info->num_bufs; i++) {
			kfree(tmp_vec_info->vec[i].iov_base);
			tmp_vec_info->vec[i].iov_base = NULL;
		}
		kfree(tmp_vec_info);
	}
}

static void *copy_linear_data(struct rx_work_info *tmp_rx_work_info)
{
	char *data;
	struct ch_info *rx_ch_info = tmp_rx_work_info->rx_ch_info;

	data = kmalloc(tmp_rx_work_info->size, GFP_KERNEL);
	if (data)
		memcpy(data, tmp_rx_work_info->ptr, tmp_rx_work_info->size);
	else
		LBSRV_ERR("%s:%s:%s %s: Error allocating the data\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__);
	return data;
}

static void *copy_vector_data(struct rx_work_info *tmp_rx_work_info)
{
	uint32_t num_bufs = 0;
	struct ch_info *rx_ch_info = tmp_rx_work_info->rx_ch_info;
	struct lbsrv_vec *tmp_vec_info;
	void *buf, *pbuf, *dest_buf;
	size_t offset = 0;
	size_t buf_size;
	uint32_t i;

	do {
		if (tmp_rx_work_info->vbuf_provider)
			buf = tmp_rx_work_info->vbuf_provider(
				tmp_rx_work_info->ptr, offset, &buf_size);
		else
			buf = tmp_rx_work_info->pbuf_provider(
				tmp_rx_work_info->ptr, offset, &buf_size);
		if (!buf)
			break;
		offset += buf_size;
		num_bufs++;
	} while (buf);

	tmp_vec_info = kzalloc(sizeof(*tmp_vec_info) +
			       num_bufs * sizeof(struct kvec), GFP_KERNEL);
	if (!tmp_vec_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating vector info\n",
			  rx_ch_info->transport, rx_ch_info->edge,
			  rx_ch_info->name, __func__);
		return NULL;
	}
	tmp_vec_info->num_bufs = num_bufs;

	offset = 0;
	for (i = 0; i < num_bufs; i++) {
		if (tmp_rx_work_info->vbuf_provider) {
			buf = tmp_rx_work_info->vbuf_provider(
				tmp_rx_work_info->ptr, offset, &buf_size);
		} else {
			pbuf = tmp_rx_work_info->pbuf_provider(
				tmp_rx_work_info->ptr, offset, &buf_size);
			buf = phys_to_virt((unsigned long)pbuf);
		}
		dest_buf = kmalloc(buf_size, GFP_KERNEL);
		if (!dest_buf) {
			LBSRV_ERR("%s:%s:%s %s: Error allocating data\n",
				  rx_ch_info->transport, rx_ch_info->edge,
				  rx_ch_info->name, __func__);
			goto out_copy_vector_data;
		}
		memcpy(dest_buf, buf, buf_size);
		tmp_vec_info->vec[i].iov_base = dest_buf;
		tmp_vec_info->vec[i].iov_len = buf_size;
		offset += buf_size;
	}
	return tmp_vec_info;
out_copy_vector_data:
	glink_lbsrv_free_data((void *)tmp_vec_info, VECTOR);
	return NULL;
}

static void *glink_lbsrv_copy_data(struct rx_work_info *tmp_rx_work_info)
{
	if (tmp_rx_work_info->buf_type == LINEAR)
		return copy_linear_data(tmp_rx_work_info);
	else
		return copy_vector_data(tmp_rx_work_info);
}

static int glink_lbsrv_handle_data(struct rx_work_info *tmp_rx_work_info)
{
	void *data;
	int ret;
	struct ch_info *rx_ch_info = tmp_rx_work_info->rx_ch_info;
	struct tx_work_info *tmp_tx_work_info;
	struct rx_done_work_info *tmp_rx_done_work_info;
	uint32_t delay_ms;

	data = glink_lbsrv_copy_data(tmp_rx_work_info);
	if (!data) {
		ret = -ENOMEM;
		goto out_handle_data;
	}

	tmp_rx_done_work_info = kmalloc(sizeof(struct rx_done_work_info),
					GFP_KERNEL);
	if (!tmp_rx_done_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating rx_done_work_info\n",
			  rx_ch_info->transport, rx_ch_info->edge,
			  rx_ch_info->name, __func__);
		glink_lbsrv_free_data(data, tmp_rx_work_info->buf_type);
		ret = -ENOMEM;
		goto out_handle_data;
	}
	INIT_DELAYED_WORK(&tmp_rx_done_work_info->work,
			  glink_lbsrv_rx_done_worker);
	tmp_rx_done_work_info->rx_done_ch_info = rx_ch_info;
	tmp_rx_done_work_info->ptr = tmp_rx_work_info->ptr;
	delay_ms = calc_delay_ms(rx_ch_info->rx_done_config.random_delay,
				 rx_ch_info->rx_done_config.delay_ms);
	queue_delayed_work(glink_lbsrv_wq, &tmp_rx_done_work_info->work,
			   msecs_to_jiffies(delay_ms));

	tmp_tx_work_info = kmalloc(sizeof(struct tx_work_info), GFP_KERNEL);
	if (!tmp_tx_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating tx_work_info\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__);
		glink_lbsrv_free_data(data, tmp_rx_work_info->buf_type);
		return -ENOMEM;
	}
	mutex_lock(&rx_ch_info->ch_info_lock);
	tmp_tx_work_info->tx_config.random_delay =
					rx_ch_info->tx_config.random_delay;
	tmp_tx_work_info->tx_config.delay_ms = rx_ch_info->tx_config.delay_ms;
	tmp_tx_work_info->tx_config.echo_count =
					rx_ch_info->tx_config.echo_count;
	tmp_tx_work_info->tx_config.transform_type =
					rx_ch_info->tx_config.transform_type;
	mutex_unlock(&rx_ch_info->ch_info_lock);
	INIT_DELAYED_WORK(&tmp_tx_work_info->work, glink_lbsrv_tx_worker);
	tmp_tx_work_info->tx_ch_info = rx_ch_info;
	tmp_tx_work_info->data = data;
	tmp_tx_work_info->tracer_pkt = tmp_rx_work_info->tracer_pkt;
	tmp_tx_work_info->buf_type = tmp_rx_work_info->buf_type;
	tmp_tx_work_info->size = tmp_rx_work_info->size;
	if (tmp_tx_work_info->buf_type == VECTOR)
		tmp_tx_work_info->vbuf_provider = glink_lbsrv_vbuf_provider;
	else
		tmp_tx_work_info->vbuf_provider = NULL;
	tmp_tx_work_info->pbuf_provider = NULL;
	delay_ms = calc_delay_ms(tmp_tx_work_info->tx_config.random_delay,
				 tmp_tx_work_info->tx_config.delay_ms);
	queue_delayed_work(glink_lbsrv_wq, &tmp_tx_work_info->work,
			   msecs_to_jiffies(delay_ms));
	return 0;
out_handle_data:
	glink_rx_done(rx_ch_info->handle, tmp_rx_work_info->ptr, false);
	return ret;
}

void glink_lpbsrv_notify_rx(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr, size_t size)
{
	struct rx_work_info *tmp_work_info;
	struct ch_info *rx_ch_info = (struct ch_info *)priv;

	LBSRV_INFO_PERF(
		"%s:%s:%s %s: end (Success) RX priv[%p] data[%p] size[%zu]\n",
		rx_ch_info->transport, rx_ch_info->edge, rx_ch_info->name,
		__func__, pkt_priv, (char *)ptr, size);
	tmp_work_info = kzalloc(sizeof(struct rx_work_info), GFP_KERNEL);
	if (!tmp_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating rx_work\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__);
		return;
	}

	tmp_work_info->rx_ch_info = rx_ch_info;
	tmp_work_info->pkt_priv = (void *)pkt_priv;
	tmp_work_info->ptr = (void *)ptr;
	tmp_work_info->buf_type = LINEAR;
	tmp_work_info->size = size;
	INIT_DELAYED_WORK(&tmp_work_info->work, glink_lbsrv_rx_worker);
	queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work, 0);
}

void glink_lpbsrv_notify_rxv(void *handle, const void *priv,
	const void *pkt_priv, void *ptr, size_t size,
	void * (*vbuf_provider)(void *iovec, size_t offset, size_t *size),
	void * (*pbuf_provider)(void *iovec, size_t offset, size_t *size))
{
	struct rx_work_info *tmp_work_info;
	struct ch_info *rx_ch_info = (struct ch_info *)priv;

	LBSRV_INFO("%s:%s:%s %s: priv[%p] data[%p] size[%zu]\n",
		   rx_ch_info->transport, rx_ch_info->edge, rx_ch_info->name,
		   __func__, pkt_priv, (char *)ptr, size);
	tmp_work_info = kzalloc(sizeof(struct rx_work_info), GFP_KERNEL);
	if (!tmp_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating rx_work\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__);
		return;
	}

	tmp_work_info->rx_ch_info = rx_ch_info;
	tmp_work_info->pkt_priv = (void *)pkt_priv;
	tmp_work_info->ptr = (void *)ptr;
	tmp_work_info->buf_type = VECTOR;
	tmp_work_info->size = size;
	tmp_work_info->vbuf_provider = vbuf_provider;
	tmp_work_info->pbuf_provider = pbuf_provider;
	INIT_DELAYED_WORK(&tmp_work_info->work, glink_lbsrv_rx_worker);
	queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work, 0);
}

void glink_lpbsrv_notify_rx_tp(void *handle, const void *priv,
			    const void *pkt_priv, const void *ptr, size_t size)
{
	struct rx_work_info *tmp_work_info;
	struct ch_info *rx_ch_info = (struct ch_info *)priv;

	LBSRV_INFO_PERF(
		"%s:%s:%s %s: end (Success) RX priv[%p] data[%p] size[%zu]\n",
		rx_ch_info->transport, rx_ch_info->edge, rx_ch_info->name,
		__func__, pkt_priv, (char *)ptr, size);
	tracer_pkt_log_event((void *)ptr, LOOPBACK_SRV_RX);
	tmp_work_info = kmalloc(sizeof(struct rx_work_info), GFP_KERNEL);
	if (!tmp_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating rx_work\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__);
		return;
	}

	tmp_work_info->rx_ch_info = rx_ch_info;
	tmp_work_info->pkt_priv = (void *)pkt_priv;
	tmp_work_info->ptr = (void *)ptr;
	tmp_work_info->tracer_pkt = true;
	tmp_work_info->buf_type = LINEAR;
	tmp_work_info->size = size;
	INIT_DELAYED_WORK(&tmp_work_info->work, glink_lbsrv_rx_worker);
	queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work, 0);
}

void glink_lpbsrv_notify_tx_done(void *handle, const void *priv,
				 const void *pkt_priv, const void *ptr)
{
	struct ch_info *tx_done_ch_info = (struct ch_info *)priv;
	LBSRV_INFO_PERF("%s:%s:%s %s: end (Success) TX_DONE ptr[%p]\n",
			tx_done_ch_info->transport, tx_done_ch_info->edge,
			tx_done_ch_info->name, __func__, ptr);

	if (pkt_priv != (const void *)0xFFFFFFFF)
		glink_lbsrv_free_data((void *)ptr,
				(uint32_t)(uintptr_t)pkt_priv);
}

void glink_lpbsrv_notify_state(void *handle, const void *priv, unsigned event)
{
	int ret;
	uint32_t delay_ms;
	struct ch_info *tmp_ch_info = (struct ch_info *)priv;
	struct queue_rx_intent_work_info *tmp_work_info = NULL;

	LBSRV_INFO("%s:%s:%s %s: event[%d]\n",
			tmp_ch_info->transport, tmp_ch_info->edge,
			tmp_ch_info->name, __func__, event);
	if (tmp_ch_info->type == CTL) {
		if (event == GLINK_CONNECTED) {
			ret = glink_queue_rx_intent(handle,
					priv, sizeof(struct req));
			LBSRV_INFO(
				"%s:%s:%s %s: QUEUE RX INTENT size[%zu] ret[%d]\n",
				tmp_ch_info->transport,
				tmp_ch_info->edge,
				tmp_ch_info->name,
				__func__, sizeof(struct req), ret);
		} else if (event == GLINK_LOCAL_DISCONNECTED) {
			queue_delayed_work(glink_lbsrv_wq,
					&tmp_ch_info->open_work,
					msecs_to_jiffies(0));
		} else if (event == GLINK_REMOTE_DISCONNECTED)
			if (!IS_ERR_OR_NULL(tmp_ch_info->handle))
				queue_delayed_work(glink_lbsrv_wq,
					&tmp_ch_info->close_work, 0);
	} else if (tmp_ch_info->type == DATA) {

		if (event == GLINK_CONNECTED) {
			mutex_lock(&tmp_ch_info->ch_info_lock);
			tmp_ch_info->fully_opened = true;
			tmp_work_info = tmp_ch_info->queue_rx_intent_work_info;
			tmp_ch_info->queue_rx_intent_work_info = NULL;
			mutex_unlock(&tmp_ch_info->ch_info_lock);

			if (tmp_work_info) {
				delay_ms = calc_delay_ms(
						tmp_work_info->random_delay,
						tmp_work_info->delay_ms);
				queue_delayed_work(glink_lbsrv_wq,
						&tmp_work_info->work,
						msecs_to_jiffies(delay_ms));
			}
		} else if (event == GLINK_LOCAL_DISCONNECTED ||
			event == GLINK_REMOTE_DISCONNECTED) {

			mutex_lock(&tmp_ch_info->ch_info_lock);
			tmp_ch_info->fully_opened = false;
			/*
			* If the state has changed to LOCAL_DISCONNECTED,
			* the channel has been fully closed and can now be
			* re-opened. If the handle value is -EBUSY, an earlier
			* open request failed because the channel was in the
			* process of closing. Requeue the work from the open
			* request.
			*/
			if (event == GLINK_LOCAL_DISCONNECTED &&
				tmp_ch_info->handle == ERR_PTR(-EBUSY)) {
				queue_delayed_work(glink_lbsrv_wq,
				&tmp_ch_info->open_work,
				msecs_to_jiffies(0));
			}
			if (event == GLINK_REMOTE_DISCONNECTED)
				if (!IS_ERR_OR_NULL(tmp_ch_info->handle))
					queue_delayed_work(
					glink_lbsrv_wq,
					&tmp_ch_info->close_work, 0);
			mutex_unlock(&tmp_ch_info->ch_info_lock);
		}
	}
}

bool glink_lpbsrv_rmt_rx_intent_req_cb(void *handle, const void *priv,
				       size_t sz)
{
	struct rmt_rx_intent_req_work_info *tmp_work_info;
	struct ch_info *tmp_ch_info = (struct ch_info *)priv;
	LBSRV_INFO("%s:%s:%s %s: QUEUE RX INTENT to receive size[%zu]\n",
		   tmp_ch_info->transport, tmp_ch_info->edge, tmp_ch_info->name,
		   __func__, sz);

	tmp_work_info = kmalloc(sizeof(struct rmt_rx_intent_req_work_info),
				GFP_ATOMIC);
	if (!tmp_work_info) {
		LBSRV_ERR("%s:%s:%s %s: Error allocating rx_work\n",
				tmp_ch_info->transport, tmp_ch_info->edge,
				tmp_ch_info->name, __func__);
		return false;
	}
	tmp_work_info->req_intent_size = sz;
	tmp_work_info->work_ch_info = tmp_ch_info;

	INIT_DELAYED_WORK(&tmp_work_info->work,
			  glink_lbsrv_rmt_rx_intent_req_worker);
	queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work, 0);
	return true;
}

void glink_lpbsrv_notify_rx_sigs(void *handle, const void *priv,
			uint32_t old_sigs, uint32_t new_sigs)
{
	LBSRV_INFO(" %s old_sigs[0x%x] New_sigs[0x%x]\n",
				__func__, old_sigs, new_sigs);
	glink_sigs_set(handle, new_sigs);
}

static void glink_lbsrv_rx_worker(struct work_struct *work)
{
	struct delayed_work *rx_work = to_delayed_work(work);
	struct rx_work_info *tmp_rx_work_info =
		container_of(rx_work, struct rx_work_info, work);
	struct ch_info *rx_ch_info = tmp_rx_work_info->rx_ch_info;
	struct req request_pkt;
	int ret;

	if (rx_ch_info->type == CTL) {
		request_pkt = *((struct req *)tmp_rx_work_info->ptr);
		glink_rx_done(rx_ch_info->handle, tmp_rx_work_info->ptr, false);
		ret = glink_queue_rx_intent(rx_ch_info->handle, rx_ch_info,
					    sizeof(struct req));
		LBSRV_INFO("%s:%s:%s %s: QUEUE RX INTENT size[%zu] ret[%d]\n",
				rx_ch_info->transport, rx_ch_info->edge,
				rx_ch_info->name, __func__,
				sizeof(struct req), ret);
		glink_lbsrv_handle_req(rx_ch_info, request_pkt);
	} else {
		ret = glink_lbsrv_handle_data(tmp_rx_work_info);
	}
	kfree(tmp_rx_work_info);
}

static void glink_lbsrv_open_worker(struct work_struct *work)
{
	struct delayed_work *open_work = to_delayed_work(work);
	struct ch_info *tmp_ch_info =
		container_of(open_work, struct ch_info, open_work);
	struct glink_open_config open_cfg;

	LBSRV_INFO("%s: glink_loopback_server_init\n", __func__);
	mutex_lock(&tmp_ch_info->ch_info_lock);
	if (!IS_ERR_OR_NULL(tmp_ch_info->handle)) {
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		return;
	}

	memset(&open_cfg, 0, sizeof(struct glink_open_config));
	open_cfg.transport = tmp_ch_info->transport;
	open_cfg.edge = tmp_ch_info->edge;
	open_cfg.name = tmp_ch_info->name;

	open_cfg.notify_rx = glink_lpbsrv_notify_rx;
	if (tmp_ch_info->type == DATA)
		open_cfg.notify_rxv = glink_lpbsrv_notify_rxv;
	open_cfg.notify_tx_done = glink_lpbsrv_notify_tx_done;
	open_cfg.notify_state = glink_lpbsrv_notify_state;
	open_cfg.notify_rx_intent_req = glink_lpbsrv_rmt_rx_intent_req_cb;
	open_cfg.notify_rx_sigs = glink_lpbsrv_notify_rx_sigs;
	open_cfg.notify_rx_abort = NULL;
	open_cfg.notify_tx_abort = NULL;
	open_cfg.notify_rx_tracer_pkt = glink_lpbsrv_notify_rx_tp;
	open_cfg.priv = tmp_ch_info;

	tmp_ch_info->handle = glink_open(&open_cfg);
	if (IS_ERR_OR_NULL(tmp_ch_info->handle)) {
		LBSRV_ERR("%s:%s:%s %s: unable to open channel\n",
			  open_cfg.transport, open_cfg.edge, open_cfg.name,
			  __func__);
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		return;
	}
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	LBSRV_INFO("%s:%s:%s %s: Open complete\n", open_cfg.transport,
			open_cfg.edge, open_cfg.name, __func__);
}

static void glink_lbsrv_close_worker(struct work_struct *work)
{
	struct delayed_work *close_work = to_delayed_work(work);
	struct ch_info *tmp_ch_info =
		container_of(close_work, struct ch_info, close_work);

	mutex_lock(&tmp_ch_info->ch_info_lock);
	if (!IS_ERR_OR_NULL(tmp_ch_info->handle)) {
		glink_close(tmp_ch_info->handle);
		tmp_ch_info->handle = NULL;
	}
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	LBSRV_INFO("%s:%s:%s %s: Close complete\n", tmp_ch_info->transport,
			tmp_ch_info->edge, tmp_ch_info->name, __func__);
}

static void glink_lbsrv_rmt_rx_intent_req_worker(struct work_struct *work)
{

	struct delayed_work *rmt_rx_intent_req_work = to_delayed_work(work);
	struct rmt_rx_intent_req_work_info *tmp_work_info =
		container_of(rmt_rx_intent_req_work,
			struct rmt_rx_intent_req_work_info, work);
	struct ch_info *tmp_ch_info = tmp_work_info->work_ch_info;
	int ret;

	mutex_lock(&tmp_ch_info->ch_info_lock);
	if (IS_ERR_OR_NULL(tmp_ch_info->handle)) {
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		LBSRV_ERR("%s:%s:%s %s: Invalid CH handle\n",
				  tmp_ch_info->transport,
				  tmp_ch_info->edge,
				  tmp_ch_info->name, __func__);
		kfree(tmp_work_info);
		return;
	}
	ret = glink_queue_rx_intent(tmp_ch_info->handle,
			(void *)tmp_ch_info, tmp_work_info->req_intent_size);
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	LBSRV_INFO("%s:%s:%s %s: QUEUE RX INTENT size[%zu] ret[%d]\n",
		   tmp_ch_info->transport, tmp_ch_info->edge,
		   tmp_ch_info->name, __func__, tmp_work_info->req_intent_size,
		   ret);
	if (ret < 0) {
		LBSRV_ERR("%s:%s:%s %s: Err %d q'ing intent size %zu\n",
			  tmp_ch_info->transport, tmp_ch_info->edge,
			  tmp_ch_info->name, __func__, ret,
			  tmp_work_info->req_intent_size);
	}
	kfree(tmp_work_info);
	return;
}

static void glink_lbsrv_queue_rx_intent_worker(struct work_struct *work)
{
	struct delayed_work *queue_rx_intent_work = to_delayed_work(work);
	struct queue_rx_intent_work_info *tmp_work_info =
		container_of(queue_rx_intent_work,
			struct queue_rx_intent_work_info, work);
	struct ch_info *tmp_ch_info = tmp_work_info->work_ch_info;
	int ret;
	uint32_t delay_ms;

	while (1) {
		mutex_lock(&tmp_ch_info->ch_info_lock);
		if (IS_ERR_OR_NULL(tmp_ch_info->handle)) {
			mutex_unlock(&tmp_ch_info->ch_info_lock);
			return;
		}

		ret = glink_queue_rx_intent(tmp_ch_info->handle,
			(void *)tmp_ch_info, tmp_work_info->intent_size);
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		if (ret < 0) {
			LBSRV_ERR("%s:%s:%s %s: Err %d q'ing intent size %d\n",
				  tmp_ch_info->transport, tmp_ch_info->edge,
				  tmp_ch_info->name, __func__, ret,
				  tmp_work_info->intent_size);
			kfree(tmp_work_info);
			return;
		}
		LBSRV_INFO("%s:%s:%s %s: Queued rx intent of size %d\n",
			   tmp_ch_info->transport, tmp_ch_info->edge,
			   tmp_ch_info->name, __func__,
			   tmp_work_info->intent_size);
		tmp_work_info->num_intents--;
		if (!tmp_work_info->num_intents)
			break;

		delay_ms = calc_delay_ms(tmp_work_info->random_delay,
					 tmp_work_info->delay_ms);
		if (delay_ms) {
			queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work,
					   msecs_to_jiffies(delay_ms));
			return;
		}
	}
	LBSRV_INFO("%s:%s:%s %s: Queued all intents. size:%d\n",
		   tmp_ch_info->transport, tmp_ch_info->edge, tmp_ch_info->name,
		   __func__, tmp_work_info->intent_size);

	if (!tmp_work_info->deferred && !tmp_work_info->random_delay &&
			!tmp_work_info->delay_ms)
		glink_lbsrv_send_response(tmp_work_info->req_ch_info->handle,
				tmp_work_info->req_id, QUEUE_RX_INTENT_CONFIG,
				0);
	kfree(tmp_work_info);
}

static void glink_lbsrv_rx_done_worker(struct work_struct *work)
{
	struct delayed_work *rx_done_work = to_delayed_work(work);
	struct rx_done_work_info *tmp_work_info =
		container_of(rx_done_work, struct rx_done_work_info, work);
	struct ch_info *tmp_ch_info = tmp_work_info->rx_done_ch_info;

	mutex_lock(&tmp_ch_info->ch_info_lock);
	if (!IS_ERR_OR_NULL(tmp_ch_info->handle))
		glink_rx_done(tmp_ch_info->handle, tmp_work_info->ptr, false);
	mutex_unlock(&tmp_ch_info->ch_info_lock);
	kfree(tmp_work_info);
}

static void glink_lbsrv_tx_worker(struct work_struct *work)
{
	struct delayed_work *tx_work = to_delayed_work(work);
	struct tx_work_info *tmp_work_info =
		container_of(tx_work, struct tx_work_info, work);
	struct ch_info *tmp_ch_info = tmp_work_info->tx_ch_info;
	int ret;
	uint32_t delay_ms;
	uint32_t flags;

	LBSRV_INFO_PERF("%s:%s:%s %s: start TX data[%p] size[%zu]\n",
		   tmp_ch_info->transport, tmp_ch_info->edge, tmp_ch_info->name,
		   __func__, tmp_work_info->data, tmp_work_info->size);
	while (1) {
		mutex_lock(&tmp_ch_info->ch_info_lock);
		if (IS_ERR_OR_NULL(tmp_ch_info->handle)) {
			mutex_unlock(&tmp_ch_info->ch_info_lock);
			return;
		}

		flags = 0;
		if (tmp_work_info->tracer_pkt) {
			flags |= GLINK_TX_TRACER_PKT;
			tracer_pkt_log_event(tmp_work_info->data,
					     LOOPBACK_SRV_TX);
		}
		if (tmp_work_info->buf_type == LINEAR)
			ret = glink_tx(tmp_ch_info->handle,
			       (tmp_work_info->tx_config.echo_count > 1 ?
					(void *)0xFFFFFFFF :
					(void *)(uintptr_t)
						tmp_work_info->buf_type),
			       (void *)tmp_work_info->data,
			       tmp_work_info->size, flags);
		else
			ret = glink_txv(tmp_ch_info->handle,
				(tmp_work_info->tx_config.echo_count > 1 ?
					(void *)0xFFFFFFFF :
					(void *)(uintptr_t)
						tmp_work_info->buf_type),
				(void *)tmp_work_info->data,
				tmp_work_info->size,
				tmp_work_info->vbuf_provider,
				tmp_work_info->pbuf_provider,
				flags);
		mutex_unlock(&tmp_ch_info->ch_info_lock);
		if (ret < 0 && ret != -EAGAIN) {
			LBSRV_ERR("%s:%s:%s %s: TX Error %d\n",
					tmp_ch_info->transport,
					tmp_ch_info->edge,
					tmp_ch_info->name, __func__, ret);
			glink_lbsrv_free_data(tmp_work_info->data,
					      tmp_work_info->buf_type);
			kfree(tmp_work_info);
			return;
		}
		if (ret != -EAGAIN)
			tmp_work_info->tx_config.echo_count--;
		if (!tmp_work_info->tx_config.echo_count)
			break;

		delay_ms = calc_delay_ms(tmp_work_info->tx_config.random_delay,
					 tmp_work_info->tx_config.delay_ms);
		if (delay_ms) {
			queue_delayed_work(glink_lbsrv_wq, &tmp_work_info->work,
					   msecs_to_jiffies(delay_ms));
			return;
		}
	}
	kfree(tmp_work_info);
}

/**
 * glink_lbsrv_link_state_worker() - Function to handle link state updates
 * work:	Pointer to the work item in the link_state_work_info.
 *
 * This worker function is scheduled when there is a link state update. Since
 * the loopback server registers for all transports, it receives all link state
 * updates about all transports that get registered in the system.
 */
static void glink_lbsrv_link_state_worker(struct work_struct *work)
{
	struct delayed_work *ls_work = to_delayed_work(work);
	struct link_state_work_info *ls_info =
		container_of(ls_work, struct link_state_work_info, work);
	struct ch_info *tmp_ch_info;

	if (ls_info->link_state == GLINK_LINK_STATE_UP) {
		LBSRV_INFO("%s: LINK_STATE_UP %s:%s\n",
			  __func__, ls_info->edge, ls_info->transport);
		mutex_lock(&ctl_ch_list_lock);
		list_for_each_entry(tmp_ch_info, &ctl_ch_list, list) {
			if (strcmp(tmp_ch_info->edge, ls_info->edge) ||
			    strcmp(tmp_ch_info->transport, ls_info->transport))
				continue;
			queue_delayed_work(glink_lbsrv_wq,
					   &tmp_ch_info->open_work, 0);
		}
		mutex_unlock(&ctl_ch_list_lock);
	} else if (ls_info->link_state == GLINK_LINK_STATE_DOWN) {
		LBSRV_INFO("%s: LINK_STATE_DOWN %s:%s\n",
			  __func__, ls_info->edge, ls_info->transport);

	}
	kfree(ls_info);
	return;
}

/**
 * glink_lbsrv_link_state_cb() - Callback to receive link state updates
 * cb_info:	Information containing link & its state.
 * priv:	Private data passed during the link state registration.
 *
 * This function is called by the GLINK core to notify the loopback server
 * regarding the link state updates. This function is registered with the
 * GLINK core by the loopback server during glink_register_link_state_cb().
 */
static void glink_lbsrv_link_state_cb(struct glink_link_state_cb_info *cb_info,
				      void *priv)
{
	struct link_state_work_info *ls_info;

	if (!cb_info)
		return;

	LBSRV_INFO("%s: %s:%s\n", __func__, cb_info->edge, cb_info->transport);
	ls_info = kmalloc(sizeof(*ls_info), GFP_KERNEL);
	if (!ls_info) {
		LBSRV_ERR("%s: Error allocating link state info\n", __func__);
		return;
	}

	strlcpy(ls_info->edge, cb_info->edge, GLINK_NAME_SIZE);
	strlcpy(ls_info->transport, cb_info->transport, GLINK_NAME_SIZE);
	ls_info->link_state = cb_info->link_state;
	INIT_DELAYED_WORK(&ls_info->work, glink_lbsrv_link_state_worker);
	queue_delayed_work(glink_lbsrv_wq, &ls_info->work, 0);
}

static int glink_loopback_server_init(void)
{
	int i;
	int ret;
	struct ch_info *tmp_ch_info;

	glink_lbsrv_wq = create_singlethread_workqueue("glink_lbsrv");
	if (!glink_lbsrv_wq) {
		LBSRV_ERR("%s: Error creating glink_lbsrv_wq\n", __func__);
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(ctl_ch_tbl); i++) {
		ret = create_ch_info(ctl_ch_tbl[i].name, ctl_ch_tbl[i].edge,
				     ctl_ch_tbl[i].transport, CTL,
				     &tmp_ch_info);
		if (ret < 0) {
			LBSRV_ERR("%s: Error creating ctl ch index %d\n",
				__func__, i);
			continue;
		}
	}
	glink_lbsrv_link_state_notif_handle = glink_register_link_state_cb(
						&glink_lbsrv_link_info, NULL);
	return 0;
}

module_init(glink_loopback_server_init);

MODULE_DESCRIPTION("MSM Generic Link (G-Link) Loopback Server");
MODULE_LICENSE("GPL v2");
