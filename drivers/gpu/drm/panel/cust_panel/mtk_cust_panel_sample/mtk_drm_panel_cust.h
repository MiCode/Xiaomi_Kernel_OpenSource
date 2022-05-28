/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Device Tree defines for LCM settings
 * Copyright (c) 2022 MediaTek Inc.
 */

#include "../cust_lcm_hal.h"

#ifndef MTK_DRM_PANEL_CUST_H
#define MTK_DRM_PANEL_CUST_H

/* the private lcm operation customized by customer*/
enum cust_lcm_cmd {
	LCM_CUST_CMD_GET_NAME,
	LCM_CUST_CMD_GET_TYPE,
	LCM_CUST_CMD_PRE_PREPARE,
};

/* the private design of lcm params customized by customer*/
struct cust_lcm_params {
	char *name;
	unsigned int type;
};

/* the private design of lcm ops customized by customer*/
struct cust_lcm_data {
	u32 flag;
	u8 id;
	u32 data_len;
	u8 *data;
};

/* the private lcm ops table customized by customer */
struct cust_lcm_ops_table {
	struct mtk_lcm_ops_table pre_prepare;
};
#endif
