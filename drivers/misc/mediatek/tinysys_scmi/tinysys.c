// SPDX-License-Identifier: GPL-2.0
/*
 * System Control and Management Interface (SCMI) MTK Tinysys Protocol
 *
 * Copyright (C) 2021 Mediatek Inc.
 */

#define pr_fmt(fmt) "SCMI Notifications TINYSYS - " fmt

#include <linux/module.h>
#include <linux/scmi_protocol.h>
#include <common.h>
#include <notify.h>

#include "protocol.h"


enum scmi_tinysys_protocol_cmd {
	TINYSYS_COMMON_SET = 0x3,
	TINYSYS_COMMON_GET = 0x4,
	TINYSYS_POWER_STATE_NOTIFY = 0x5,
};

struct scmi_tinysys_common_set_state {
	__le32 reserv;
	__le32 p0;
	__le32 p1;
	__le32 p2;
	__le32 p3;
	__le32 p4;
	__le32 p5;
};
struct scmi_tinysys_common_get_state {
	__le32 reserv;
	__le32 p0;
	__le32 p1;
};

struct scmi_tinysys_notify {
	__le32 reserv;
	__le32 f_id;
	__le32 notify_enable;
};

struct scmi_tinysys_notifier_payld {
	__le32 f_id;
	__le32 p1;
	__le32 p2;
	__le32 p3;
	__le32 p4;
};

struct scmi_tinysys_info {
	u32 version;
	int num_domains;
};

static int scmi_tinysys_common_set(const struct scmi_protocol_handle *ph, u32 p0,
			     u32 p1, u32 p2, u32 p3, u32 p4, u32 p5)
{
	int ret;
	struct scmi_xfer *t;
	struct scmi_tinysys_common_set_state *st;

	ret = ph->xops->xfer_get_init(ph, TINYSYS_COMMON_SET, sizeof(*st), 0, &t);
	if (ret)
		return ret;

	st = t->tx.buf;
	st->p0 = cpu_to_le32(p0);
	st->p1 = cpu_to_le32(p1);
	st->p2 = cpu_to_le32(p2);
	st->p3 = cpu_to_le32(p3);
	st->p4 = cpu_to_le32(p4);
	st->p5 = cpu_to_le32(p5);

	ret = ph->xops->do_xfer(ph, t);

	ph->xops->xfer_put(ph, t);
	return ret;
}


static int scmi_tinysys_common_get(const struct scmi_protocol_handle *ph,
	u32 p0, u32 p1, struct scmi_tinysys_status *rvalue)
{
	int ret;
	struct scmi_xfer *t;
	struct scmi_tinysys_common_get_state *gt;

	ret = ph->xops->xfer_get_init(ph, TINYSYS_COMMON_GET,
				 sizeof(*gt), sizeof(*rvalue), &t);
	if (ret)
		return ret;

	gt = t->tx.buf;
	gt->p0 = cpu_to_le32(p0);
	gt->p1 = cpu_to_le32(p1);

	ret = ph->xops->do_xfer(ph, t);
	if (!ret) {
		memcpy((void *)rvalue, (t->rx.buf), sizeof(*rvalue));
	}

	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_tinysys_request_notify(const struct scmi_protocol_handle *ph,
				     u32 src_id, bool enable)
{
	int ret;
	struct scmi_xfer *t;
	struct scmi_tinysys_notify *notify;

	ret = ph->xops->xfer_get_init(ph, TINYSYS_POWER_STATE_NOTIFY,
			sizeof(*notify), 0, &t);
	if (ret)
		return ret;

	notify = t->tx.buf;
	notify->f_id = src_id;
	notify->notify_enable = enable ? cpu_to_le32(BIT(0)) : 0;

	ret = ph->xops->do_xfer(ph, t);

	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_tinysys_set_notify_enabled(const struct scmi_protocol_handle *ph,
					  u8 evt_id, u32 src_id, bool enable)
{
	int ret;

	ret = scmi_tinysys_request_notify(ph, src_id, enable);
	if (ret)
		pr_debug("FAIL_ENABLE - evt[%X] - ret:%d\n", evt_id, ret);

	return ret;
}

static void *scmi_tinysys_fill_custom_report(const struct scmi_protocol_handle *ph,
					    u8 evt_id, ktime_t timestamp,
					    const void *payld, size_t payld_sz,
					    void *report, u32 *src_id)
{
	const struct scmi_tinysys_notifier_payld *p = payld;
	struct scmi_tinysys_notifier_report *r = report;

	if (evt_id != SCMI_EVENT_TINYSYS_NOTIFIER ||
	    sizeof(*p) != payld_sz)
		return NULL;

	r->timestamp = timestamp;
	r->f_id = le32_to_cpu(p->f_id);
	r->p1_status= le32_to_cpu(p->p1);
	r->p2_status= le32_to_cpu(p->p2);
	r->p3_status= le32_to_cpu(p->p3);
	r->p4_status= le32_to_cpu(p->p4);

	*src_id = p->f_id;

	return r;
}

static int scmi_tinysys_attributes_get(const struct scmi_protocol_handle *ph,
				     struct scmi_tinysys_info *pi)
{
	int ret;
	struct scmi_xfer *t;
	u32 attr;

	ret = ph->xops->xfer_get_init(ph, PROTOCOL_ATTRIBUTES,
				      0, sizeof(attr), &t);
	if (ret)
		return ret;

	ret = ph->xops->do_xfer(ph, t);
	if (!ret) {
		attr = get_unaligned_le32(t->rx.buf);
		pi->num_domains = attr;
	}

	ph->xops->xfer_put(ph, t);
	return ret;
}


static int scmi_tinysys_get_num_sources(const struct scmi_protocol_handle *ph)
{
	struct scmi_tinysys_info *pinfo = ph->get_priv(ph);

	if (!pinfo)
		return -EINVAL;

	return pinfo->num_domains;
}

static const struct scmi_event tinysys_events[] = {
	{
		.id = SCMI_EVENT_TINYSYS_NOTIFIER,
		.max_payld_sz =	sizeof(struct scmi_tinysys_notifier_payld),
		.max_report_sz = sizeof(struct scmi_tinysys_notifier_report),
	},
};

static const struct scmi_tinysys_proto_ops tinysys_proto_ops = {
		.common_set = scmi_tinysys_common_set,
		.common_get = scmi_tinysys_common_get,
};

static const struct scmi_event_ops tinysys_event_ops = {
	.get_num_sources = scmi_tinysys_get_num_sources,
	.set_notify_enabled = scmi_tinysys_set_notify_enabled,
	.fill_custom_report = scmi_tinysys_fill_custom_report,
};

static const struct scmi_protocol_events tinysys_protocol_events = {
	.queue_sz = 4 * SCMI_PROTO_QUEUE_SZ,
	.ops = &tinysys_event_ops,
	.evts = tinysys_events,
	.num_events = ARRAY_SIZE(tinysys_events),
};

static int scmi_tinysys_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;
	struct scmi_tinysys_info *pinfo;

	ph->xops->version_get(ph, &version);

	dev_dbg(ph->dev, "Tinysys Protocol Version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	pinfo = devm_kzalloc(ph->dev, sizeof(*pinfo), GFP_KERNEL);
	if (!pinfo)
		return -ENOMEM;

	scmi_tinysys_attributes_get(ph, pinfo);

	pinfo->version = version;

	return ph->set_priv(ph, pinfo);
}

const struct scmi_protocol scmi_tinysys_protocol = {
	.id = SCMI_PROTOCOL_TINYSYS,
	.owner = THIS_MODULE,
	.init_instance = &scmi_tinysys_protocol_init,
	.ops = &tinysys_proto_ops,
	.events = &tinysys_protocol_events,
};

int scmi_tinysys_register(void)
{
	return scmi_protocol_register(&scmi_tinysys_protocol);
}

MODULE_DESCRIPTION("SCMI tinysys protocol");
MODULE_LICENSE("GPL v2");

