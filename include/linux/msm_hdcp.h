/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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


