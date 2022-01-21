// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/scmi_gplaf.h>
#include "common.h"

#define SCMI_VENDOR_MSG_MAX_TX_SIZE		(100) /* in bytes */
#define SCMI_VENDOR_MSG_START			(3)	  /* MSG 3-15 can be used for spl purpose */
#define SCMI_VENDOR_MSG_GPLAF_START		(16)  /* Each module to use MAX 16 MSG */
#define SCMI_VENDOR_MSG_GPLAF_END		(31)

enum scmi_gplaf_protocol_cmd {
	GPLAF_GAME_START_MSG_ID = SCMI_VENDOR_MSG_GPLAF_START,
	GPLAF_GAME_STOP_MSG_ID,
	GPLAF_SET_LOG_LEVEL,
	GPLAF_RETIRE_FRAME_EVENT,
	GPLAF_GFX_DATA_NOTIFY,
	GPLAF_PASS_DATA,
	GPLAF_UPDATE_HEALTH,
	GPLAF_MAX_MSG_ID = SCMI_VENDOR_MSG_GPLAF_END,
};

static int scmi_gplaf_set_u16_val(const struct scmi_protocol_handle *ph,
			u16 val, u32 msg_id)
{
	int ret = 0;
	struct scmi_xfer *t;
	uint32_t *msg;

	if (!ph) {
		pr_err("scmi_protocol_handle is null\n");
		return ret;
	}
	ret = ph->xops->xfer_get_init(ph, msg_id,
				sizeof(*msg), sizeof(uint32_t), &t);
	if (ret)
		return ret;

	if (!t) {
		pr_err("scmi_xfer struct is null\n");
		return ret;
	}
	msg = t->tx.buf;
	*msg = cpu_to_le32(val);
	ret = ph->xops->do_xfer(ph, t);
	ph->xops->xfer_put(ph, t);
	return ret;
}

static int scmi_gplaf_game_start_cmd(const struct scmi_protocol_handle *ph,
			u16 pid)
{
	return scmi_gplaf_set_u16_val(ph, pid, GPLAF_GAME_START_MSG_ID);
}

static int scmi_gplaf_game_stop_cmd(const struct scmi_protocol_handle *ph)
{
	return scmi_gplaf_set_u16_val(ph, 0, GPLAF_GAME_STOP_MSG_ID);
}

static int scmi_gplaf_set_log_level(const struct scmi_protocol_handle *ph,
			u16 val)
{
	return scmi_gplaf_set_u16_val(ph, val, GPLAF_SET_LOG_LEVEL);
}

static int scmi_gplaf_frame_retire_event(const struct scmi_protocol_handle *ph)
{
	return scmi_gplaf_set_u16_val(ph, 0, GPLAF_RETIRE_FRAME_EVENT);
}

static int scmi_gplaf_gfx_data_notify(const struct scmi_protocol_handle *ph)
{
	return scmi_gplaf_set_u16_val(ph, 0, GPLAF_GFX_DATA_NOTIFY);
}

static int scmi_gplaf_pass_data(const struct scmi_protocol_handle *ph, u16 data)
{
	return scmi_gplaf_set_u16_val(ph, data, GPLAF_PASS_DATA);
}

static int scmi_update_gplaf_health(const struct scmi_protocol_handle *ph, u16 data)
{
	return scmi_gplaf_set_u16_val(ph, data, GPLAF_UPDATE_HEALTH);
}

static struct scmi_gplaf_vendor_ops gplaf_proto_ops = {
	.start_gplaf = scmi_gplaf_game_start_cmd,
	.stop_gplaf = scmi_gplaf_game_stop_cmd,
	.set_gplaf_log_level = scmi_gplaf_set_log_level,
	.send_frame_retire_event = scmi_gplaf_frame_retire_event,
	.send_gfx_data_notify = scmi_gplaf_gfx_data_notify,
	.pass_gplaf_data = scmi_gplaf_pass_data,
	.update_gplaf_health = scmi_update_gplaf_health,
};

static int scmi_gplaf_vendor_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;

	ph->xops->version_get(ph, &version);

	dev_dbg(ph->dev, "GPLAF version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	return 0;
}

static const struct scmi_protocol scmi_gplaf_vendor = {
	.id = SCMI_PROTOCOL_GPLAF,
	.owner = THIS_MODULE,
	.init_instance = &scmi_gplaf_vendor_protocol_init,
	.ops = &gplaf_proto_ops,
};
module_scmi_protocol(scmi_gplaf_vendor);

MODULE_DESCRIPTION("SCMI gplaf vendor Protocol");
MODULE_LICENSE("GPL v2");
