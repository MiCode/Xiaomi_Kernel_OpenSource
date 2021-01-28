// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include "mtk_smi.h"
#include "mmdvfs_mgr.h"
#include "mmdvfs_internal.h"

static mmdvfs_state_change_cb quick_change_cbs[MMDVFS_SCEN_COUNT];
static mmdvfs_prepare_action_cb quick_action_cbs[MMDVFS_SCEN_COUNT];

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
		MMDVFSMSG("unable to get resolution\n");
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

int register_mmdvfs_prepare_cb(int mmdvfs_client_id,
	mmdvfs_prepare_action_cb func)
{
	if (mmdvfs_client_id >= 0 && mmdvfs_client_id < MMDVFS_SCEN_COUNT) {
		MMDVFSMSG("prepare_cb registered: %d\n", mmdvfs_client_id);
		quick_action_cbs[mmdvfs_client_id] = func;
	} else {
		MMDVFSMSG("clk_switch_cb failed: id=%d\n", mmdvfs_client_id);
		return 1;
	}
	return 0;
}

void mmdvfs_notify_prepare_action(struct mmdvfs_prepare_action_event *event)
{
		int i = 0;

		mmdvfs_internal_notify_vcore_calibration(event);

		MMDVFSMSG("prepare_action: %d\n", event->event_type);
		for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
			mmdvfs_prepare_action_cb func = quick_action_cbs[i];

			if (func != NULL) {
				MMDVFSMSG("prepare_action id:%d act:%d\n",
				i, event->event_type);
				func(event);
			}
		}
}


int register_mmdvfs_state_change_cb(int mmdvfs_client_id,
	mmdvfs_state_change_cb func)
{
		if (mmdvfs_client_id >= 0
		&& mmdvfs_client_id < MMDVFS_SCEN_COUNT) {
			quick_change_cbs[mmdvfs_client_id] = func;
		} else {
			MMDVFSMSG("clk_switch_cb failed id=%d\n",
				mmdvfs_client_id);
			return 1;
		}
	return 0;
}


void mmdvfs_internal_handle_state_change(
	struct mmdvfs_state_change_event *event)
{
		int i = 0;

		for (i = 0; i < MMDVFS_SCEN_COUNT; i++) {
			mmdvfs_state_change_cb func = quick_change_cbs[i];

			if (func != NULL)
				func(event);
		}
}
