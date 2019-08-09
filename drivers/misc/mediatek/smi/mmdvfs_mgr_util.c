/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include "mmdvfs_mgr.h"
#include "mmdvfs_internal.h"

static mmdvfs_state_change_cb quick_state_change_cbs[MMDVFS_SCEN_COUNT];
static mmdvfs_prepare_cb quick_prepare_action_cbs[MMDVFS_SCEN_COUNT];

enum mmdvfs_lcd_size_enum mmdvfs_get_lcd_resolution(void)
{
	enum mmdvfs_lcd_size_enum result = MMDVFS_LCD_SIZE_HD;
	long lcd_resolution = 0;
	long lcd_w = 0;
	long lcd_h = 0;
	int convert_err = -EINVAL;

#if defined(CONFIG_LCM_WIDTH) && defined(CONFIG_LCM_HEIGHT)
	convert_err = kstrtoul(CONFIG_LCM_WIDTH, 10, &lcd_w);
	if (!convert_err)
		convert_err = kstrtoul(CONFIG_LCM_HEIGHT, 10, &lcd_h);
#endif	/* CONFIG_LCM_WIDTH, CONFIG_LCM_HEIGHT */

	if (convert_err) {
#if !defined(CONFIG_FPGA_EARLY_PORTING) && defined(CONFIG_MTK_FB)
		lcd_w = DISP_GetScreenWidth();
		lcd_h = DISP_GetScreenHeight();
#else
		MMDVFSMSG(
			"unable to get resolution, query API is unavailable\n");
#endif
	}

	lcd_resolution = lcd_w * lcd_h;

	if (lcd_resolution <= MMDVFS_DISPLAY_SIZE_HD)
		result = MMDVFS_LCD_SIZE_HD;
	else if (lcd_resolution <= MMDVFS_DISPLAY_SIZE_FHD)
		result = MMDVFS_LCD_SIZE_FHD;
	else
		result = MMDVFS_LCD_SIZE_WQHD;

	return result;
}

int register_mmdvfs_prepare_cb(int client_id, mmdvfs_prepare_cb func)
{
	if (client_id >= 0 && client_id < MMDVFS_SCEN_COUNT) {
		MMDVFSMSG("%s: %d\n", __func__, client_id);
		quick_prepare_action_cbs[client_id] = func;
	} else {
		MMDVFSMSG("clk_switch_cb register failed: %d\n", client_id);
		return 1;
	}
	return 0;
}

void mmdvfs_notify_prepare_action(struct mmdvfs_prepare_event *event)
{
	int i = 0;

	mmdvfs_internal_notify_vcore_calibration(event);

	MMDVFSMSG("%s: %d\n", __func__, event->event_type);
	for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
		mmdvfs_prepare_cb func = quick_prepare_action_cbs[i];

		if (func != NULL) {
			MMDVFSMSG("mmdvfs_notify_prepare cb, id:%d, act:%d\n",
			i, event->event_type);
			func(event);
		}
	}
}

void mmdvfs_internal_handle_state_change(
	struct mmdvfs_state_change_event *event)
{
	int i = 0;

	for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
		mmdvfs_state_change_cb func = quick_state_change_cbs[i];

		if (func != NULL)
			func(event);
	}
}
