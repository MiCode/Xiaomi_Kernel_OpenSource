/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MTK_SRCLKEN_BRIDGE_H__
#define __MTK_SRCLKEN_BRIDGE_H__


enum srclken_config {
	SRCLKEN_OK = 0,
	SRCLKEN_NOT_SUPPORT,
	SRCLKEN_BT_ONLY,
	SRCLKEN_FULL_SET,
	SRCLKEN_BRINGUP,
	SRCLKEN_ERR,
};

/*******************************************************************************
 * Bridging from platform -> srclken.ko
 ******************************************************************************/
typedef void (*srclken_bridge_dump_cb)(void);
typedef enum srclken_config (*srclken_bridge_get_cb)(void);

struct srclken_bridge {
	srclken_bridge_get_cb get_stage_cb;
	srclken_bridge_dump_cb dump_sta_cb;
	srclken_bridge_dump_cb dump_cfg_cb;
	srclken_bridge_dump_cb dump_last_sta_cb;
};

void srclken_export_platform_bridge_register(struct srclken_bridge *cb);
void srclken_export_platform_bridge_unregister(void);

enum srclken_config srclken_get_stage(void);
enum srclken_config  srclken_dump_sta_log(void);
enum srclken_config  srclken_dump_cfg_log(void);
enum srclken_config  srclken_dump_last_sta_log(void);

#endif

