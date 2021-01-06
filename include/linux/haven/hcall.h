/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef __HH_HCALL_H
#define __HH_HCALL_H

#include <linux/err.h>
#include <linux/types.h>

#include <linux/haven/hcall_common.h>
#include <linux/haven/hh_common.h>
#include <asm/haven/hcall.h>

struct hh_hcall_hyp_identify_resp {
	u64 api_info;
	u64 flags[3];
};

static inline int hh_hcall_hyp_identify(struct hh_hcall_hyp_identify_resp *resp)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6000,
		(struct hh_hcall_args){ 0 },
		&_resp);

	if (resp) {
		resp->api_info = _resp.resp0;
		resp->flags[0] = _resp.resp1;
		resp->flags[1] = _resp.resp2;
		resp->flags[2] = _resp.resp3;
	}

	return 0;
}

static inline int hh_hcall_dbl_bind(hh_capid_t dbl_capid, hh_capid_t vic_capid,
				    hh_virq_handle_t virq_info)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6010,
		(struct hh_hcall_args){ dbl_capid, vic_capid, virq_info },
		&_resp);

	return ret;
}

static inline int hh_hcall_dbl_unbind(hh_capid_t dbl_capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6011,
		(struct hh_hcall_args){ dbl_capid },
		&_resp);

	return ret;
}

struct hh_hcall_dbl_send_resp {
	u64 old_flags;
};

static inline int hh_hcall_dbl_send(hh_capid_t dbl_capid,
				    hh_dbl_flags_t new_flags,
				    struct hh_hcall_dbl_send_resp *resp)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6012,
		(struct hh_hcall_args){ dbl_capid, new_flags },
		&_resp);

	if (!ret && resp)
		resp->old_flags = _resp.resp1;

	return ret;
}

struct hh_hcall_dbl_recv_resp {
	u64 old_flags;
};

static inline int hh_hcall_dbl_recv(hh_capid_t dbl_capid,
				    hh_dbl_flags_t clear_flags,
				    struct hh_hcall_dbl_recv_resp *resp)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6013,
		(struct hh_hcall_args){ dbl_capid, clear_flags },
		&_resp);

	if (!ret && resp)
		resp->old_flags = _resp.resp1;

	return ret;
}

static inline int hh_hcall_dbl_reset(hh_capid_t dbl_capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6014,
		(struct hh_hcall_args){ dbl_capid },
		&_resp);

	return ret;
}

static inline int hh_hcall_dbl_mask(hh_capid_t dbl_capid,
				    hh_dbl_flags_t enable_mask,
				    hh_dbl_flags_t ack_mask)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6015,
		(struct hh_hcall_args){ dbl_capid, enable_mask, ack_mask },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_bind_send(hh_capid_t msgq_capid,
					  hh_capid_t vic_capid,
					  hh_virq_handle_t virq_info)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6017,
		(struct hh_hcall_args){ msgq_capid, vic_capid, virq_info },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_bind_recv(hh_capid_t msgq_capid,
					  hh_capid_t vic_capid,
					  hh_virq_handle_t virq_info)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6018,
		(struct hh_hcall_args){ msgq_capid, vic_capid, virq_info },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_unbind_send(hh_capid_t msgq_capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6019,
		(struct hh_hcall_args){ msgq_capid },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_unbind_recv(hh_capid_t msgq_capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x601A,
		(struct hh_hcall_args){ msgq_capid },
		&_resp);

	return ret;
}

struct hh_hcall_msgq_send_resp {
	bool not_full;
};

static inline int hh_hcall_msgq_send(hh_capid_t msgq_capid, size_t size,
				     void *data, u64 send_flags,
				     struct hh_hcall_msgq_send_resp *resp)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x601B,
		(struct hh_hcall_args){ msgq_capid, size, (unsigned long)data,
					send_flags },
		&_resp);

	if (resp)
		resp->not_full = _resp.resp1;

	return ret;
}

struct hh_hcall_msgq_recv_resp {
	size_t recv_size;
	bool not_empty;
};

static inline int hh_hcall_msgq_recv(hh_capid_t msgq_capid, void *buffer,
				     size_t max_size,
				     struct hh_hcall_msgq_recv_resp *resp)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x601C,
		(struct hh_hcall_args){ msgq_capid, (unsigned long)buffer,
					max_size },
		&_resp);

	if (!ret && resp) {
		resp->recv_size = _resp.resp1;
		resp->not_empty = _resp.resp2;
	}

	return ret;
}

static inline int hh_hcall_msgq_flush(hh_capid_t msgq_capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x601D,
		(struct hh_hcall_args){ msgq_capid },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_configure_send(hh_capid_t msgq_capid,
					       long not_full_threshold,
					       long not_full_delay)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x601F,
		(struct hh_hcall_args){ msgq_capid, not_full_threshold,
					not_full_delay, -1 },
		&_resp);

	return ret;
}

static inline int hh_hcall_msgq_configure_recv(hh_capid_t msgq_capid,
					       long not_empty_threshold,
					       long not_empty_delay)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6020,
		(struct hh_hcall_args){ msgq_capid, not_empty_threshold,
					not_empty_delay, -1 },
		&_resp);

	return ret;
}

static inline int hh_hcall_vcpu_affinity_set(hh_capid_t vcpu_capid,
						uint32_t cpu_index)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x603d,
			(struct hh_hcall_args){ vcpu_capid, cpu_index, -1 },
			&_resp);

	return ret;
}

static inline int hh_hcall_vpm_group_get_state(u64 vpmg_capid,
		uint64_t *vpmg_state)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6045,
			(struct hh_hcall_args){ vpmg_capid, 0 },
			&_resp);
	*vpmg_state = _resp.resp1;

	return ret;
}

static inline int hh_hcall_trace_update_class_flags(
		uint64_t set_flags, uint64_t clear_flags,
		uint64_t *new_flags)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x603f,
			(struct hh_hcall_args){ set_flags, clear_flags, 0 },
			&_resp);

	if (!ret && new_flags)
		*new_flags = _resp.resp1;

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_assert_virq(hh_capid_t capid, u64 int_status)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x604e,
			(struct hh_hcall_args){ capid, int_status, 0 },
			&_resp);

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_set_dev_features(hh_capid_t capid,
				u64 features_sel, u64 features)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x604f,
			(struct hh_hcall_args){ capid, features_sel,
				features, 0 }, &_resp);

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_set_queue_num_max(hh_capid_t capid,
				u64 queue_sel, u64 queue_num_max)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6050,
			(struct hh_hcall_args){ capid, queue_sel,
					queue_num_max, 0 }, &_resp);

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_get_drv_features(hh_capid_t capid,
			u64 features_sel, u64 *features)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6051,
			(struct hh_hcall_args){ capid, features_sel, 0},
			&_resp);

	if (!ret && features)
		*features = _resp.resp1;

	return ret;
}

struct hh_hcall_virtio_queue_info {
	u64 queue_num;
	u64 queue_ready;
	u64 queue_desc;
	u64 queue_driver;
	u64 queue_device;
};

static inline int
hh_hcall_virtio_mmio_backend_get_queue_info(hh_capid_t capid,
		u64 queue_sel, struct hh_hcall_virtio_queue_info *queue_info)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6052,
			(struct hh_hcall_args){ capid, queue_sel, 0},
			&_resp);

	if (!ret && queue_info) {
		queue_info->queue_num		= _resp.resp1;
		queue_info->queue_ready		= _resp.resp2;
		queue_info->queue_desc		= _resp.resp3;
		queue_info->queue_driver	= _resp.resp4;
		queue_info->queue_device	= _resp.resp5;
	}

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_get_event(hh_capid_t capid,
			u64 *event_data, u64 *event)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6053,
			(struct hh_hcall_args){ capid, 0},
			&_resp);

	if (!ret && event_data)
		*event_data = _resp.resp1;
	if (!ret && event)
		*event = _resp.resp2;

	return ret;
}

static inline int
hh_hcall_virtio_mmio_backend_ack_reset(hh_capid_t capid)
{
	int ret;
	struct hh_hcall_resp _resp = {0};

	ret = _hh_hcall(0x6054,
			(struct hh_hcall_args){ capid, 0},
			&_resp);

	return ret;
}

#endif
