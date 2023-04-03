// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/scmi_shared_rail.h>
#include "common.h"

#define SCMI_VENDOR_MSG_SRB_START		(3)

enum scmi_srb_protocol_cmd {
	L3_BOOST_MSG_ID = SCMI_VENDOR_MSG_SRB_START,
	SILVER_CORE_BOOST_MSG_ID,
	SHARED_RAIL_BOOST_MAX_MSG_ID,
};

static int scmi_srb_send_command_val(const struct scmi_protocol_handle *ph,
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

static int scmi_set_shared_rail_boost(const struct scmi_protocol_handle *ph,
						u16 data, enum srb_features feature)
{
	int ret = 0;

	if (feature == L3_BOOST)
		ret = scmi_srb_send_command_val(ph, data, L3_BOOST_MSG_ID);
	else if (feature == SILVER_CORE_BOOST)
		ret = scmi_srb_send_command_val(ph, data, SILVER_CORE_BOOST_MSG_ID);
	else
		ret = -EINVAL;

	return ret;
}

static struct scmi_shared_rail_vendor_ops shared_rail_proto_ops = {
	.set_shared_rail_boost = scmi_set_shared_rail_boost,
};

static int scmi_shared_rail_vendor_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;
	int ret;

	ret = ph->xops->version_get(ph, &version);
	if (ret)
		return ret;

	dev_info(ph->dev, "SHARED RAIL version %d.%d\n",
		PROTOCOL_REV_MAJOR(version), PROTOCOL_REV_MINOR(version));

	return 0;
}

static const struct scmi_protocol scmi_shared_rail_vendor = {
	.id = SCMI_PROTOCOL_SHARED_RAIL,
	.owner = THIS_MODULE,
	.init_instance = &scmi_shared_rail_vendor_protocol_init,
	.ops = &shared_rail_proto_ops,
};
module_scmi_protocol(scmi_shared_rail_vendor);

MODULE_DESCRIPTION("SCMI Shared Rail Vendor Protocol");
MODULE_LICENSE("GPL v2");
