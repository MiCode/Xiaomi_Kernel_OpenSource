/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2018, 2020, The Linux Foundation. All rights reserved. */

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
 * @cont_splash_enabled: Flag to check if cont splash was enabled on bridge
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
	bool cont_splash_enabled;
};

int mdss_dba_utils_video_on(void *data, struct mdss_panel_info *pinfo);
int mdss_dba_utils_video_off(void *data);
void mdss_dba_utils_hdcp_enable(void *data, bool enable);

void *mdss_dba_utils_init(struct mdss_dba_utils_init_data *init_data);
void mdss_dba_utils_deinit(void *data);
#endif /* __MDSS_DBA_UTILS__ */
