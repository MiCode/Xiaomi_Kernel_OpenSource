/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI__MSM_HDMI_HDCP_MGR_H
#define _UAPI__MSM_HDMI_HDCP_MGR_H

#include <linux/types.h>

enum DS_TYPE {  /* type of downstream device */
	DS_UNKNOWN,
	DS_RECEIVER,
	DS_REPEATER,
};

enum {
	MSG_ID_IDX,
	RET_CODE_IDX,
	HEADER_LEN,
};

enum RET_CODE {
	HDCP_NOT_AUTHED,
	HDCP_AUTHED,
	HDCP_DISABLE,
};

enum MSG_ID { /* List of functions expected to be called after it */
	DOWN_CHECK_TOPOLOGY,
	UP_REQUEST_TOPOLOGY,
	UP_SEND_TOPOLOGY,
	DOWN_REQUEST_TOPOLOGY,
	MSG_NUM,
};

enum SOURCE_ID {
	HDCP_V1_TX,
	HDCP_V1_RX,
	HDCP_V2_RX,
	HDCP_V2_TX,
	SRC_NUM,
};

/*
 * how to parse sysfs params buffer
 * from hdcp_tx driver.
 */

struct HDCP_V2V1_MSG_TOPOLOGY {
	/* indicates downstream's type */
	__u32 ds_type;
	__u8 bksv[5];
	__u8 dev_count;
	__u8 depth;
	__u8 ksv_list[5 * 127];
	__u32 max_cascade_exceeded;
	__u32 max_dev_exceeded;
};

#endif /* _UAPI__MSM_HDMI_HDCP_MGR_H */
