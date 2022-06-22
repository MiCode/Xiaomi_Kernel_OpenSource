// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/xarray.h>
#include <linux/list.h>

#include "debug.h"

static LIST_HEAD(rtx_pkt_list);
static DEFINE_SPINLOCK(rtx_pkt_list_lock);

static DEFINE_XARRAY(rtx_records);
static DEFINE_MUTEX(rtx_records_lock);

static struct work_struct rtx_work;

#define QRTR_FLAGS_CONFIRM_RX   BIT(0)

#define QRTR_PROTO_VER_1 1
#define QRTR_PROTO_VER_2 3

struct qrtr_hdr_v1 {
	__le32 version;
	__le32 type;
	__le32 src_node_id;
	__le32 src_port_id;
	__le32 confirm_rx;
	__le32 size;
	__le32 dst_node_id;
	__le32 dst_port_id;
} __packed;

struct qrtr_hdr_v2 {
	u8 version;
	u8 type;
	u8 flags;
	u8 optlen;
	__le32 size;
	__le16 src_node_id;
	__le16 src_port_id;
	__le16 dst_node_id;
	__le16 dst_port_id;
};

struct qrtr_rtx_record {
	u8 state;
	unsigned long key;
	struct timespec64 time;
};

struct qrtr_rtx_pkt {
	u8 state;
	unsigned long key;
	struct list_head item;
};

void qrtr_log_resume_tx_node_erase(unsigned int node_id)
{
	unsigned long index;
	struct qrtr_rtx_record *record;

	mutex_lock(&rtx_records_lock);
	xa_for_each(&rtx_records, index, record) {
		if ((record->key >> 32) == node_id &&
		    record->state != RTX_UNREG_NODE) {
			xa_erase(&rtx_records, record->key);
			kfree(record);
		}
	}
	mutex_unlock(&rtx_records_lock);

	qrtr_log_resume_tx(node_id, 0, RTX_UNREG_NODE);
}
EXPORT_SYMBOL(qrtr_log_resume_tx_node_erase);

static void qrtr_update_record(unsigned long key, u8 state)
{
	struct qrtr_rtx_record *record;

	mutex_lock(&rtx_records_lock);
	record = xa_load(&rtx_records, key);
	if (!record) {
		record = kzalloc(sizeof(*record), GFP_KERNEL);
		if (!record) {
			mutex_unlock(&rtx_records_lock);
			return;
		}

		record->key = key;
		record->state = state;
		ktime_get_ts64(&record->time);
		xa_store(&rtx_records, record->key, record, GFP_KERNEL);
		mutex_unlock(&rtx_records_lock);
		return;
	}

	if (record->state == RTX_REMOVE_RECORD) {
		xa_erase(&rtx_records, record->key);
		mutex_unlock(&rtx_records_lock);
		kfree(record);
		return;
	}

	record->state = state;
	ktime_get_ts64(&record->time);
	mutex_unlock(&rtx_records_lock);
}

static void qrtr_rtx_work(struct work_struct *work)
{
	struct qrtr_rtx_pkt *pkt, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&rtx_pkt_list_lock, flags);
	list_for_each_entry_safe(pkt, tmp, &rtx_pkt_list, item) {
		list_del(&pkt->item);
		spin_unlock_irqrestore(&rtx_pkt_list_lock, flags);
		qrtr_update_record(pkt->key, pkt->state);
		kfree(pkt);
		spin_lock_irqsave(&rtx_pkt_list_lock, flags);
	}
	spin_unlock_irqrestore(&rtx_pkt_list_lock, flags);
}

int qrtr_log_resume_tx(unsigned int node_id,
		       unsigned int port_id, u8 state)
{
	struct qrtr_rtx_pkt *pkt;
	unsigned long flags;

	pkt = kzalloc(sizeof(*pkt), GFP_ATOMIC);
	if (!pkt)
		return -ENOMEM;

	pkt->state = state;
	pkt->key = ((u64)node_id << 32 | port_id);

	spin_lock_irqsave(&rtx_pkt_list_lock, flags);
	list_add(&pkt->item, &rtx_pkt_list);
	spin_unlock_irqrestore(&rtx_pkt_list_lock, flags);

	schedule_work(&rtx_work);
	return 0;
}
EXPORT_SYMBOL(qrtr_log_resume_tx);

void qrtr_log_skb_failure(const void *data, size_t len)
{
	const struct qrtr_hdr_v1 *v1;
	const struct qrtr_hdr_v2 *v2;
	bool confirm_rx = false;
	unsigned int node_id;
	unsigned int port_id;
	unsigned int ver;

	ver = *(u8 *)data;
	if (ver == QRTR_PROTO_VER_1 && len > sizeof(*v1)) {
		v1 = data;
		if (v1->confirm_rx) {
			node_id = v1->src_port_id;
			port_id = v1->src_port_id;
			confirm_rx = true;
		}
	} else if (ver == QRTR_PROTO_VER_2 && len > sizeof(*v2)) {
		v2 = data;
		if (v2->flags & QRTR_FLAGS_CONFIRM_RX) {
			node_id = v2->src_node_id;
			port_id = v2->src_port_id;
			confirm_rx = true;
		}
	} else {
		pr_err("%s: Invalid version %d\n", __func__, ver);
	}

	if (confirm_rx)
		qrtr_log_resume_tx(node_id, port_id,
				   RTX_SKB_ALLOC_FAIL);
}
EXPORT_SYMBOL(qrtr_log_skb_failure);

void qrtr_debug_init(void)
{
	INIT_WORK(&rtx_work, qrtr_rtx_work);
}
EXPORT_SYMBOL(qrtr_debug_init);

void qrtr_debug_remove(void)
{
	cancel_work_sync(&rtx_work);
}
EXPORT_SYMBOL(qrtr_debug_remove);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. QRTR debug");
MODULE_LICENSE("GPL v2");

