/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef MTK_TS_WMT_H
#define MTK_TS_WMT_H
typedef int (*wcn_bridge_thermal_query_cb)(void);

extern int tswmt_get_WiFi_tx_tput(void);
struct wcn_platform_bridge {
	wcn_bridge_thermal_query_cb thermal_query_cb;
};

void wcn_export_platform_bridge_register(struct wcn_platform_bridge *cb);
void wcn_export_platform_bridge_unregister(void);

#endif
