// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "common.h"
#include <linux/scmi_plh.h>

#define SCMI_VENDOR_MSG_MAX_TX_SIZE		(100) /* in bytes */
#define SCMI_VENDOR_MSG_START			(3)   /* MSG 3-15 can be used for spl purpose */
#define SCMI_VENDOR_MSG_SPLH_START		(16)  /* Each PLH module to use MAX 16 MSG */
#define SCMI_VENDOR_MSG_SPLH_END		(31)

enum scmi_plh_protocol_cmd {
	PERF_LOCK_SCROLL_INIT_IPC_FREQ_TBL_MSG_ID = SCMI_VENDOR_MSG_SPLH_START,
	PERF_LOCK_SCROLL_START_MSG_ID,
	PERF_LOCK_SCROLL_STOP_MSG_ID,
	PERF_LOCK_SCROLL_SET_SAMPLE_MS,
	PERF_LOCK_SCROLL_SET_LOG_LEVEL,
	PERF_LOCK_SCROLL_MAX_MSG_ID = SCMI_VENDOR_MSG_SPLH_END,
};


static int scmi_plh_scroll_init_ipc_freq_tbl(const struct scmi_protocol_handle *ph,
			u16 *p_init_args, u16 init_len)
{
	int ret, i = 0;
	struct scmi_xfer *t;
	uint32_t *msg, msg_size, msg_val, align_init_len = init_len;

	if (init_len % 2)
		align_init_len += 1; /* align in multiple of u32 */

	msg_size = align_init_len * sizeof(*p_init_args);

	if (msg_size > SCMI_VENDOR_MSG_MAX_TX_SIZE)
		return -EINVAL;

	ret = ph->xops->xfer_get_init(ph, PERF_LOCK_SCROLL_INIT_IPC_FREQ_TBL_MSG_ID,
				(msg_size), sizeof(uint32_t), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;

	for (i = 0; i < init_len/2 ; i++) {
		msg_val = *p_init_args++;
		msg_val |= ((*p_init_args++) << 16);
		*msg++ = cpu_to_le32(msg_val);
	}

	if (init_len % 2)
		*msg = cpu_to_le32(*p_init_args);

	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_plh_scroll_set_u16_val(const struct scmi_protocol_handle *ph,
			u16 val, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	uint32_t *msg;

	ret = ph->xops->xfer_get_init(ph, msg_id,
				sizeof(*msg), sizeof(uint32_t), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	*msg = cpu_to_le32(val);
	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_plh_scroll_start_cmd(const struct scmi_protocol_handle *ph,
			u16 fps)
{
	return scmi_plh_scroll_set_u16_val(ph, fps, PERF_LOCK_SCROLL_START_MSG_ID);
}

static int scmi_plh_scroll_stop_cmd(const struct scmi_protocol_handle *ph)
{
	return scmi_plh_scroll_set_u16_val(ph, 0, PERF_LOCK_SCROLL_STOP_MSG_ID);
}

static int scmi_plh_scroll_set_sample_ms(const struct scmi_protocol_handle *ph,
			u16 sample_ms)
{
	return scmi_plh_scroll_set_u16_val(ph, sample_ms, PERF_LOCK_SCROLL_SET_SAMPLE_MS);
}

static int scmi_plh_scroll_set_log_level(const struct scmi_protocol_handle *ph,
			u16 log_level)
{
	return scmi_plh_scroll_set_u16_val(ph, log_level, PERF_LOCK_SCROLL_SET_LOG_LEVEL);
}

static struct scmi_plh_vendor_ops plh_proto_ops = {
	.init_splh_ipc_freq_tbl = scmi_plh_scroll_init_ipc_freq_tbl,
	.start_splh = scmi_plh_scroll_start_cmd,
	.stop_splh = scmi_plh_scroll_stop_cmd,
	.set_splh_sample_ms = scmi_plh_scroll_set_sample_ms,
	.set_splh_log_level = scmi_plh_scroll_set_log_level,
};

static int scmi_plh_vendor_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;

	ph->xops->version_get(ph, &version);

	dev_dbg(ph->dev, "PLH version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	return 0;
}

static const struct scmi_protocol scmi_plh_vendor = {
	.id = SCMI_PROTOCOL_PLH,
	.owner = THIS_MODULE,
	.init_instance = &scmi_plh_vendor_protocol_init,
	.ops = &plh_proto_ops,
};
module_scmi_protocol(scmi_plh_vendor);

MODULE_DESCRIPTION("SCMI plh vendor Protocol");
MODULE_LICENSE("GPL v2");
