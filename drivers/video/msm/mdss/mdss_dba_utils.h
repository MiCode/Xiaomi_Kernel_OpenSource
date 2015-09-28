/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_DBA_UTILS__
#define __MDSS_DBA_UTILS__

#include <linux/types.h>

#include "mdss_panel.h"

/**
 * struct mdss_dba_utils_init_data - Init data for registering with DBA utils.
 * @kobj: An instance of Kobject for sysfs creation
 * @instance_id: Instance ID of device registered with DBA
 * @chip_name: Name of the device registered with DBA
 * @client_name: Name of the client registering with DBA
 * @pinfo: Detailed panel information
 *
 * This structure's instance is needed to be passed as parameter
 * to register API to let the DBA utils module configure and
 * allocate an instance of DBA utils for the client.
 */
struct mdss_dba_utils_init_data {
	struct kobject *kobj;
	u32 instance_id;
	u32 fb_node;
	char *chip_name;
	char *client_name;
	struct mdss_panel_info *pinfo;
};

int mdss_dba_utils_video_on(void *data, struct mdss_panel_info *pinfo);
int mdss_dba_utils_video_off(void *data);
void mdss_dba_utils_hdcp_enable(void *data, bool enable);

void *mdss_dba_utils_init(struct mdss_dba_utils_init_data *init_data);
void mdss_dba_utils_deinit(void *data);
#endif /* __MDSS_DBA_UTILS__ */
