// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "common.h"

#define SCMI_VENDOR_MSG_MAX_TX_SIZE		(100) /* in bytes */
#define SCMI_VENDOR_MSG_START			(3)
#define SCMI_VENDOR_MSG_PLH_START		(16)

enum scmi_plh_protocol_cmd {
	PERF_LOCK_SET_LOG_LEVEL = SCMI_VENDOR_MSG_START,
	PERF_LOCK_SCROLL_INIT_IPC_FREQ_TBL_MSG_ID = SCMI_VENDOR_MSG_PLH_START,
	PERF_LOCK_SCROLL_START_MSG_ID,
	PERF_LOCK_SCROLL_STOP_MSG_ID,
	PERF_LOCK_MAX_MSG_ID,
};


static int scmi_plh_scroll_init_ipc_freq_tbl(const struct scmi_handle *handle,
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

	ret = scmi_xfer_get_init(handle, PERF_LOCK_SCROLL_INIT_IPC_FREQ_TBL_MSG_ID,
				SCMI_PROTOCOL_PLH,
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

	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);
	return ret;
}


static int scmi_send_start_stop(const struct scmi_handle *handle,
			u16 fps, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	uint32_t *msg;

	ret = scmi_xfer_get_init(handle, msg_id,
				SCMI_PROTOCOL_PLH,
				sizeof(*msg), sizeof(uint32_t), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	*msg = cpu_to_le32(fps);
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);
	return ret;
}

static int scmi_plh_scroll_start_cmd(const struct scmi_handle *handle,
			u16 fps)
{
	return scmi_send_start_stop(handle, fps, PERF_LOCK_SCROLL_START_MSG_ID);
}

static int scmi_plh_scroll_stop_cmd(const struct scmi_handle *handle)
{
	return scmi_send_start_stop(handle, 0, PERF_LOCK_SCROLL_STOP_MSG_ID);
}

static int scmi_plh_set_log_level(const struct scmi_handle *handle,
			u16 val)
{
	int ret = 0;
	struct scmi_xfer *t;
	uint32_t *msg;

	ret = scmi_xfer_get_init(handle, PERF_LOCK_SET_LOG_LEVEL,
				SCMI_PROTOCOL_PLH, sizeof(*msg),
				sizeof(uint32_t), &t);
	if (ret)
		return ret;

	msg = t->tx.buf;
	*msg = cpu_to_le32(val);
	ret = scmi_do_xfer(handle, t);
	scmi_xfer_put(handle, t);
	return ret;
}

static struct scmi_plh_vendor_ops plh_ops = {
	.init_splh_ipc_freq_tbl = scmi_plh_scroll_init_ipc_freq_tbl,
	.start_splh = scmi_plh_scroll_start_cmd,
	.stop_splh = scmi_plh_scroll_stop_cmd,
	.set_plh_log_level = scmi_plh_set_log_level,
};

static int scmi_plh_vendor_protocol_init(struct scmi_handle *handle)
{
	u32 version;

	scmi_version_get(handle, SCMI_PROTOCOL_PLH, &version);

	dev_dbg(handle->dev, "PLH version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	handle->plh_ops = &plh_ops;

	return 0;
}

static int __init scmi_plh_init(void)
{
	return scmi_protocol_register(SCMI_PROTOCOL_PLH,
				      &scmi_plh_vendor_protocol_init);
}
subsys_initcall(scmi_plh_init);
