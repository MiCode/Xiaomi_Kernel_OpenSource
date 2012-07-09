/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <mach/socinfo.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include "rpm-notifier.h"

struct msm_rpm_driver_data {
	const char *ch_name;
	uint32_t ch_type;
	smd_channel_t *ch_info;
	struct work_struct work;
	spinlock_t smd_lock_write;
	spinlock_t smd_lock_read;
	struct completion smd_open;
};

#define DEFAULT_BUFFER_SIZE 256
#define GFP_FLAG(noirq) (noirq ? GFP_ATOMIC : GFP_KERNEL)
#define INV_HDR "resource does not exist"
#define ERR "err\0"
#define MAX_ERR_BUFFER_SIZE 60

static ATOMIC_NOTIFIER_HEAD(msm_rpm_sleep_notifier);
static bool standalone;

int msm_rpm_register_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&msm_rpm_sleep_notifier, nb);
}

int msm_rpm_unregister_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&msm_rpm_sleep_notifier, nb);
}

static struct workqueue_struct *msm_rpm_smd_wq;

enum {
	MSM_RPM_MSG_REQUEST_TYPE = 0,
	MSM_RPM_MSG_TYPE_NR,
};

static const uint32_t msm_rpm_request_service[MSM_RPM_MSG_TYPE_NR] = {
	0x716572, /* 'req\0' */
};

/*the order of fields matter and reflect the order expected by the RPM*/
struct rpm_request_header {
	uint32_t service_type;
	uint32_t request_len;
};

struct rpm_message_header {
	uint32_t msg_id;
	enum msm_rpm_set set;
	uint32_t resource_type;
	uint32_t resource_id;
	uint32_t data_len;
};

struct msm_rpm_kvp_data {
	uint32_t key;
	uint32_t nbytes; /* number of bytes */
	uint8_t *value;
	bool valid;
};

static atomic_t msm_rpm_msg_id = ATOMIC_INIT(0);

static struct msm_rpm_driver_data msm_rpm_data;

struct msm_rpm_request {
	struct rpm_request_header req_hdr;
	struct rpm_message_header msg_hdr;
	struct msm_rpm_kvp_data *kvp;
	uint32_t num_elements;
	uint32_t write_idx;
	uint8_t *buf;
	uint32_t numbytes;
};

/*
 * Data related to message acknowledgement
 */

LIST_HEAD(msm_rpm_wait_list);

struct msm_rpm_wait_data {
	struct list_head list;
	uint32_t msg_id;
	bool ack_recd;
	int errno;
	struct completion ack;
};
DEFINE_SPINLOCK(msm_rpm_list_lock);

struct msm_rpm_ack_msg {
	uint32_t req;
	uint32_t req_len;
	uint32_t rsc_id;
	uint32_t msg_len;
	uint32_t id_ack;
};

static int irq_process;

LIST_HEAD(msm_rpm_ack_list);

static void msm_rpm_notify_sleep_chain(struct rpm_message_header *hdr,
		struct msm_rpm_kvp_data *kvp)
{
	struct msm_rpm_notifier_data notif;

	notif.rsc_type = hdr->resource_type;
	notif.rsc_id = hdr->resource_id;
	notif.key = kvp->key;
	notif.size = kvp->nbytes;
	notif.value = kvp->value;
	atomic_notifier_call_chain(&msm_rpm_sleep_notifier, 0, &notif);
}

static int msm_rpm_add_kvp_data_common(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int size, bool noirq)
{
	int i;
	int data_size, msg_size;

	if (!handle)
		return -EINVAL;

	data_size = ALIGN(size, SZ_4);
	msg_size = data_size + sizeof(struct rpm_request_header);

	for (i = 0; i < handle->write_idx; i++) {
		if (handle->kvp[i].key != key)
			continue;
		if (handle->kvp[i].nbytes != data_size) {
			kfree(handle->kvp[i].value);
			handle->kvp[i].value = NULL;
		} else {
			if (!memcmp(handle->kvp[i].value, data, data_size))
				return 0;
		}
		break;
	}

	if (i >= handle->num_elements)
		return -ENOMEM;

	if (i == handle->write_idx)
		handle->write_idx++;

	if (!handle->kvp[i].value) {
		handle->kvp[i].value = kzalloc(data_size, GFP_FLAG(noirq));

		if (!handle->kvp[i].value)
			return -ENOMEM;
	} else {
		/* We enter the else case, if a key already exists but the
		 * data doesn't match. In which case, we should zero the data
		 * out.
		 */
		memset(handle->kvp[i].value, 0, data_size);
	}

	if (!handle->kvp[i].valid)
		handle->msg_hdr.data_len += msg_size;
	else
		handle->msg_hdr.data_len += (data_size - handle->kvp[i].nbytes);

	handle->kvp[i].nbytes = data_size;
	handle->kvp[i].key = key;
	memcpy(handle->kvp[i].value, data, size);
	handle->kvp[i].valid = true;

	if (handle->msg_hdr.set == MSM_RPM_CTX_SLEEP_SET)
		msm_rpm_notify_sleep_chain(&handle->msg_hdr, &handle->kvp[i]);

	return 0;

}

static struct msm_rpm_request *msm_rpm_create_request_common(
		enum msm_rpm_set set, uint32_t rsc_type, uint32_t rsc_id,
		int num_elements, bool noirq)
{
	struct msm_rpm_request *cdata;

	cdata = kzalloc(sizeof(struct msm_rpm_request),
			GFP_FLAG(noirq));

	if (!cdata) {
		printk(KERN_INFO"%s():Cannot allocate memory for client data\n",
				__func__);
		goto cdata_alloc_fail;
	}

	cdata->msg_hdr.set = set;
	cdata->msg_hdr.resource_type = rsc_type;
	cdata->msg_hdr.resource_id = rsc_id;
	cdata->msg_hdr.data_len = 0;

	cdata->num_elements = num_elements;
	cdata->write_idx = 0;

	cdata->kvp = kzalloc(sizeof(struct msm_rpm_kvp_data) * num_elements,
			GFP_FLAG(noirq));

	if (!cdata->kvp) {
		pr_warn("%s(): Cannot allocate memory for key value data\n",
				__func__);
		goto kvp_alloc_fail;
	}

	cdata->buf = kzalloc(DEFAULT_BUFFER_SIZE, GFP_FLAG(noirq));

	if (!cdata->buf)
		goto buf_alloc_fail;

	cdata->numbytes = DEFAULT_BUFFER_SIZE;
	return cdata;

buf_alloc_fail:
	kfree(cdata->kvp);
kvp_alloc_fail:
	kfree(cdata);
cdata_alloc_fail:
	return NULL;

}

void msm_rpm_free_request(struct msm_rpm_request *handle)
{
	int i;

	if (!handle)
		return;
	for (i = 0; i < handle->write_idx; i++)
		kfree(handle->kvp[i].value);
	kfree(handle->kvp);
	kfree(handle);
}
EXPORT_SYMBOL(msm_rpm_free_request);

struct msm_rpm_request *msm_rpm_create_request(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements)
{
	return msm_rpm_create_request_common(set, rsc_type, rsc_id,
			num_elements, false);
}
EXPORT_SYMBOL(msm_rpm_create_request);

struct msm_rpm_request *msm_rpm_create_request_noirq(
		enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, int num_elements)
{
	return msm_rpm_create_request_common(set, rsc_type, rsc_id,
			num_elements, true);
}
EXPORT_SYMBOL(msm_rpm_create_request_noirq);

int msm_rpm_add_kvp_data(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int size)
{
	return msm_rpm_add_kvp_data_common(handle, key, data, size, false);

}
EXPORT_SYMBOL(msm_rpm_add_kvp_data);

int msm_rpm_add_kvp_data_noirq(struct msm_rpm_request *handle,
		uint32_t key, const uint8_t *data, int size)
{
	return msm_rpm_add_kvp_data_common(handle, key, data, size, true);
}
EXPORT_SYMBOL(msm_rpm_add_kvp_data_noirq);

/* Runs in interrupt context */
static void msm_rpm_notify(void *data, unsigned event)
{
	struct msm_rpm_driver_data *pdata = (struct msm_rpm_driver_data *)data;
	BUG_ON(!pdata);

	if (!(pdata->ch_info))
		return;

	switch (event) {
	case SMD_EVENT_DATA:
		queue_work(msm_rpm_smd_wq, &pdata->work);
		break;
	case SMD_EVENT_OPEN:
		complete(&pdata->smd_open);
		break;
	case SMD_EVENT_CLOSE:
	case SMD_EVENT_STATUS:
	case SMD_EVENT_REOPEN_READY:
		break;
	default:
		pr_info("Unknown SMD event\n");

	}
}

static struct msm_rpm_wait_data *msm_rpm_get_entry_from_msg_id(uint32_t msg_id)
{
	struct list_head *ptr;
	struct msm_rpm_wait_data *elem;
	unsigned long flags;

	spin_lock_irqsave(&msm_rpm_list_lock, flags);

	list_for_each(ptr, &msm_rpm_wait_list) {
		elem = list_entry(ptr, struct msm_rpm_wait_data, list);
		if (elem && (elem->msg_id == msg_id))
			break;
		elem = NULL;
	}
	spin_unlock_irqrestore(&msm_rpm_list_lock, flags);
	return elem;
}

static int msm_rpm_get_next_msg_id(void)
{
	int id;

	do {
		id = atomic_inc_return(&msm_rpm_msg_id);
	} while ((id == 0) || msm_rpm_get_entry_from_msg_id(id));

	return id;
}

static int msm_rpm_add_wait_list(uint32_t msg_id)
{
	unsigned long flags;
	struct msm_rpm_wait_data *data =
		kzalloc(sizeof(struct msm_rpm_wait_data), GFP_ATOMIC);

	if (!data)
		return -ENOMEM;

	init_completion(&data->ack);
	data->ack_recd = false;
	data->msg_id = msg_id;
	spin_lock_irqsave(&msm_rpm_list_lock, flags);
	list_add(&data->list, &msm_rpm_wait_list);
	spin_unlock_irqrestore(&msm_rpm_list_lock, flags);

	return 0;
}

static void msm_rpm_free_list_entry(struct msm_rpm_wait_data *elem)
{
	unsigned long flags;

	spin_lock_irqsave(&msm_rpm_list_lock, flags);
	list_del(&elem->list);
	spin_unlock_irqrestore(&msm_rpm_list_lock, flags);
	kfree(elem);
}

static void msm_rpm_process_ack(uint32_t msg_id, int errno)
{
	struct list_head *ptr;
	struct msm_rpm_wait_data *elem;
	unsigned long flags;

	spin_lock_irqsave(&msm_rpm_list_lock, flags);

	list_for_each(ptr, &msm_rpm_wait_list) {
		elem = list_entry(ptr, struct msm_rpm_wait_data, list);
		if (elem && (elem->msg_id == msg_id)) {
			elem->errno = errno;
			elem->ack_recd = true;
			complete(&elem->ack);
			break;
		}
		elem = NULL;
	}
	WARN_ON(!elem);

	spin_unlock_irqrestore(&msm_rpm_list_lock, flags);
}

struct msm_rpm_kvp_packet {
	uint32_t id;
	uint32_t len;
	uint32_t val;
};

static inline uint32_t msm_rpm_get_msg_id_from_ack(uint8_t *buf)
{
	return ((struct msm_rpm_ack_msg *)buf)->id_ack;
}

static inline int msm_rpm_get_error_from_ack(uint8_t *buf)
{
	uint8_t *tmp;
	uint32_t req_len = ((struct msm_rpm_ack_msg *)buf)->req_len;

	int rc = -ENODEV;

	req_len -= sizeof(struct msm_rpm_ack_msg);
	req_len += 2 * sizeof(uint32_t);
	if (!req_len)
		return 0;

	tmp = buf + sizeof(struct msm_rpm_ack_msg);

	BUG_ON(memcmp(tmp, ERR, sizeof(uint32_t)));

	tmp += 2 * sizeof(uint32_t);

	if (!(memcmp(tmp, INV_HDR, min(req_len, sizeof(INV_HDR))-1)))
		rc = -EINVAL;

	return rc;
}

static void msm_rpm_read_smd_data(char *buf)
{
	int pkt_sz;
	int bytes_read = 0;

	pkt_sz = smd_cur_packet_size(msm_rpm_data.ch_info);

	BUG_ON(pkt_sz > MAX_ERR_BUFFER_SIZE);

	if (pkt_sz != smd_read_avail(msm_rpm_data.ch_info))
		return;

	BUG_ON(pkt_sz == 0);

	do {
		int len;

		len = smd_read(msm_rpm_data.ch_info, buf + bytes_read, pkt_sz);
		pkt_sz -= len;
		bytes_read += len;

	} while (pkt_sz > 0);

	BUG_ON(pkt_sz < 0);
}

static void msm_rpm_smd_work(struct work_struct *work)
{
	uint32_t msg_id;
	int errno;
	char buf[MAX_ERR_BUFFER_SIZE] = {0};
	unsigned long flags;

	while (smd_is_pkt_avail(msm_rpm_data.ch_info) && !irq_process) {
		spin_lock_irqsave(&msm_rpm_data.smd_lock_read, flags);
		msm_rpm_read_smd_data(buf);
		spin_unlock_irqrestore(&msm_rpm_data.smd_lock_read, flags);
		msg_id = msm_rpm_get_msg_id_from_ack(buf);
		errno = msm_rpm_get_error_from_ack(buf);
		msm_rpm_process_ack(msg_id, errno);
	}
}

static int msm_rpm_send_data(struct msm_rpm_request *cdata,
		int msg_type, bool noirq)
{
	uint8_t *tmpbuff;
	int i, ret, msg_size;
	unsigned long flags;

	int req_hdr_sz, msg_hdr_sz;

	if (!cdata->msg_hdr.data_len)
		return 0;
	req_hdr_sz = sizeof(cdata->req_hdr);
	msg_hdr_sz = sizeof(cdata->msg_hdr);

	cdata->req_hdr.service_type = msm_rpm_request_service[msg_type];

	cdata->msg_hdr.msg_id = msm_rpm_get_next_msg_id();

	cdata->req_hdr.request_len = cdata->msg_hdr.data_len + msg_hdr_sz;
	msg_size = cdata->req_hdr.request_len + req_hdr_sz;

	/* populate data_len */
	if (msg_size > cdata->numbytes) {
		kfree(cdata->buf);
		cdata->numbytes = msg_size;
		cdata->buf = kzalloc(msg_size, GFP_FLAG(noirq));
	}

	if (!cdata->buf)
		return 0;

	tmpbuff = cdata->buf;

	memcpy(tmpbuff, &cdata->req_hdr, req_hdr_sz + msg_hdr_sz);

	tmpbuff += req_hdr_sz + msg_hdr_sz;

	for (i = 0; (i < cdata->write_idx); i++) {
		/* Sanity check */
		BUG_ON((tmpbuff - cdata->buf) > cdata->numbytes);

		if (!cdata->kvp[i].valid)
			continue;

		memcpy(tmpbuff, &cdata->kvp[i].key, sizeof(uint32_t));
		tmpbuff += sizeof(uint32_t);

		memcpy(tmpbuff, &cdata->kvp[i].nbytes, sizeof(uint32_t));
		tmpbuff += sizeof(uint32_t);

		memcpy(tmpbuff, cdata->kvp[i].value, cdata->kvp[i].nbytes);
		tmpbuff += cdata->kvp[i].nbytes;
	}

	if (standalone) {
		for (i = 0; (i < cdata->write_idx); i++)
			cdata->kvp[i].valid = false;

		cdata->msg_hdr.data_len = 0;
		ret = cdata->msg_hdr.msg_id;
		return ret;
	}

	msm_rpm_add_wait_list(cdata->msg_hdr.msg_id);

	spin_lock_irqsave(&msm_rpm_data.smd_lock_write, flags);

	ret = smd_write_avail(msm_rpm_data.ch_info);

	if (ret < 0) {
		pr_warn("%s(): SMD not initialized\n", __func__);
		spin_unlock_irqrestore(&msm_rpm_data.smd_lock_write, flags);
		return 0;
	}

	while ((ret < msg_size)) {
		if (!noirq) {
			spin_unlock_irqrestore(&msm_rpm_data.smd_lock_write,
					flags);
			cpu_relax();
			spin_lock_irqsave(&msm_rpm_data.smd_lock_write, flags);
		} else
			udelay(5);
		ret = smd_write_avail(msm_rpm_data.ch_info);
	}

	ret = smd_write(msm_rpm_data.ch_info, &cdata->buf[0], msg_size);
	spin_unlock_irqrestore(&msm_rpm_data.smd_lock_write, flags);

	if (ret == msg_size) {
		for (i = 0; (i < cdata->write_idx); i++)
			cdata->kvp[i].valid = false;
		cdata->msg_hdr.data_len = 0;
		ret = cdata->msg_hdr.msg_id;
	} else if (ret < msg_size) {
		struct msm_rpm_wait_data *rc;
		ret = 0;
		pr_info("Failed to write data msg_size:%d ret:%d\n",
				msg_size, ret);
		rc = msm_rpm_get_entry_from_msg_id(cdata->msg_hdr.msg_id);
		if (rc)
			msm_rpm_free_list_entry(rc);
	}
	return ret;
}

int msm_rpm_send_request(struct msm_rpm_request *handle)
{
	return msm_rpm_send_data(handle, MSM_RPM_MSG_REQUEST_TYPE, false);
}
EXPORT_SYMBOL(msm_rpm_send_request);

int msm_rpm_send_request_noirq(struct msm_rpm_request *handle)
{
	return msm_rpm_send_data(handle, MSM_RPM_MSG_REQUEST_TYPE, true);
}
EXPORT_SYMBOL(msm_rpm_send_request_noirq);

int msm_rpm_wait_for_ack(uint32_t msg_id)
{
	struct msm_rpm_wait_data *elem;
	int rc = 0;

	if (!msg_id)
		return -EINVAL;

	if (standalone)
		return 0;

	elem = msm_rpm_get_entry_from_msg_id(msg_id);
	if (!elem)
		return 0;

	rc = wait_for_completion_timeout(&elem->ack, msecs_to_jiffies(1));
	if (!rc) {
		pr_warn("%s(): Timed out after 1 ms\n", __func__);
		rc = -ETIMEDOUT;
	} else {
		rc = elem->errno;
		msm_rpm_free_list_entry(elem);
	}
	return rc;
}
EXPORT_SYMBOL(msm_rpm_wait_for_ack);

int msm_rpm_wait_for_ack_noirq(uint32_t msg_id)
{
	struct msm_rpm_wait_data *elem;
	unsigned long flags;
	int rc = 0;
	uint32_t id = 0;
	int count = 0;

	if (!msg_id)
		return -EINVAL;

	if (standalone)
		return 0;

	spin_lock_irqsave(&msm_rpm_data.smd_lock_read, flags);
	irq_process = true;

	elem = msm_rpm_get_entry_from_msg_id(msg_id);

	if (!elem)
		/* Should this be a bug
		 * Is it ok for another thread to read the msg?
		 */
		goto wait_ack_cleanup;

	while ((id != msg_id) && (count++ < 10)) {
		if (smd_is_pkt_avail(msm_rpm_data.ch_info)) {
			int errno;
			char buf[MAX_ERR_BUFFER_SIZE] = {};

			msm_rpm_read_smd_data(buf);
			id = msm_rpm_get_msg_id_from_ack(buf);
			errno = msm_rpm_get_error_from_ack(buf);
			msm_rpm_process_ack(id, errno);
		} else
			udelay(100);
	}

	if (count == 10) {
		rc = -ETIMEDOUT;
		pr_warn("%s(): Timed out after 1ms\n", __func__);
	} else {
		rc = elem->errno;
		msm_rpm_free_list_entry(elem);
	}
wait_ack_cleanup:
	irq_process = false;
	spin_unlock_irqrestore(&msm_rpm_data.smd_lock_read, flags);
	return rc;
}
EXPORT_SYMBOL(msm_rpm_wait_for_ack_noirq);

int msm_rpm_send_message(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems)
{
	int i, rc;
	struct msm_rpm_request *req =
		msm_rpm_create_request(set, rsc_type, rsc_id, nelems);
	if (!req)
		return -ENOMEM;

	for (i = 0; i < nelems; i++) {
		rc = msm_rpm_add_kvp_data(req, kvp[i].key,
				kvp[i].data, kvp[i].length);
		if (rc)
			goto bail;
	}

	rc = msm_rpm_wait_for_ack(msm_rpm_send_request(req));
bail:
	msm_rpm_free_request(req);
	return rc;
}
EXPORT_SYMBOL(msm_rpm_send_message);

int msm_rpm_send_message_noirq(enum msm_rpm_set set, uint32_t rsc_type,
		uint32_t rsc_id, struct msm_rpm_kvp *kvp, int nelems)
{
	int i, rc;
	struct msm_rpm_request *req =
		msm_rpm_create_request_noirq(set, rsc_type, rsc_id, nelems);
	if (!req)
		return -ENOMEM;

	for (i = 0; i < nelems; i++) {
		rc = msm_rpm_add_kvp_data_noirq(req, kvp[i].key,
					kvp[i].data, kvp[i].length);
		if (rc)
			goto bail;
	}

	rc = msm_rpm_wait_for_ack_noirq(msm_rpm_send_request_noirq(req));
bail:
	msm_rpm_free_request(req);
	return rc;
}
EXPORT_SYMBOL(msm_rpm_send_message_noirq);
static bool msm_rpm_set_standalone(void)
{
	if (machine_is_msm8974()) {
		pr_warn("%s(): Running in standalone mode, requests "
				"will not be sent to RPM\n", __func__);
		standalone = true;
	}
	return standalone;
}

static int __devinit msm_rpm_dev_probe(struct platform_device *pdev)
{
	char *key = NULL;
	int ret;

	key = "rpm-channel-name";
	ret = of_property_read_string(pdev->dev.of_node, key,
					&msm_rpm_data.ch_name);
	if (ret)
		goto fail;

	key = "rpm-channel-type";
	ret = of_property_read_u32(pdev->dev.of_node, key,
					&msm_rpm_data.ch_type);
	if (ret)
		goto fail;

	init_completion(&msm_rpm_data.smd_open);
	spin_lock_init(&msm_rpm_data.smd_lock_write);
	spin_lock_init(&msm_rpm_data.smd_lock_read);
	INIT_WORK(&msm_rpm_data.work, msm_rpm_smd_work);

	if (smd_named_open_on_edge(msm_rpm_data.ch_name, msm_rpm_data.ch_type,
				&msm_rpm_data.ch_info, &msm_rpm_data,
				msm_rpm_notify)) {
		pr_info("Cannot open RPM channel %s %d\n", msm_rpm_data.ch_name,
				msm_rpm_data.ch_type);

		msm_rpm_set_standalone();
		BUG_ON(!standalone);
		complete(&msm_rpm_data.smd_open);
	}

	ret = wait_for_completion_timeout(&msm_rpm_data.smd_open,
			msecs_to_jiffies(5));

	BUG_ON(!ret);

	smd_disable_read_intr(msm_rpm_data.ch_info);

	if (!standalone) {
		msm_rpm_smd_wq = create_singlethread_workqueue("rpm-smd");
		if (!msm_rpm_smd_wq)
			return -EINVAL;
	}

	of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	return 0;
fail:
	pr_err("%s(): Failed to read node: %s, key=%s\n", __func__,
			pdev->dev.of_node->full_name, key);
	return -EINVAL;
}

static struct of_device_id msm_rpm_match_table[] =  {
	{.compatible = "qcom,rpm-smd"},
	{},
};

static struct platform_driver msm_rpm_device_driver = {
	.probe = msm_rpm_dev_probe,
	.driver = {
		.name = "rpm-smd",
		.owner = THIS_MODULE,
		.of_match_table = msm_rpm_match_table,
	},
};

int __init msm_rpm_driver_init(void)
{
	static bool registered;

	if (registered)
		return 0;
	registered = true;

	return platform_driver_register(&msm_rpm_device_driver);
}
EXPORT_SYMBOL(msm_rpm_driver_init);
late_initcall(msm_rpm_driver_init);
