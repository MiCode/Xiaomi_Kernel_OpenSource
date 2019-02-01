/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_HDCP_H
#define __MSM_HDCP_H
#include <linux/types.h>
#include "video/msm_hdmi_hdcp_mgr.h"

void msm_hdcp_notify_topology(struct device *dev);
void msm_hdcp_cache_repeater_topology(struct device *dev,
			struct HDCP_V2V1_MSG_TOPOLOGY *tp);
void msm_hdcp_register_cb(struct device *dev, void *ctx,
	void (*cb)(void *ctx, u8 data));

#endif /* __MSM_HDCP_H */
