/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include "primary_display.h"
#include "disp_drv_log.h"
#include "ddp_dsi.h"

void set_lcm(struct LCM_setting_table_V3 *para_tbl,
			unsigned int size, bool hs)
{
	DISPFUNC();

	_primary_path_switch_dst_lock();
	primary_display_manual_lock();

	primary_display_idlemgr_kick(__func__, 0);

	if (_is_power_on_status(DISP_MODULE_DSI0))
		DSI_dcs_set_lcm_reg_v4(DISP_MODULE_DSI0, hs, para_tbl, size, 1);
	else
		DISPERR("%s invalid: dsi is power off\n", __func__);

	primary_display_manual_unlock();
	_primary_path_switch_dst_unlock();
}

int read_lcm(unsigned char cmd, unsigned char *buf,
			unsigned char buf_size, bool sendhs)
{
	int ret = 0;

	DISPFUNC();
	_primary_path_switch_dst_lock();
	primary_display_manual_lock();

	primary_display_idlemgr_kick(__func__, 0);

	if (_is_power_on_status(DISP_MODULE_DSI0))
		ret = DSI_dcs_read_lcm_reg_v4(DISP_MODULE_DSI0,
					cmd, buf, buf_size, sendhs);
	else
		DISPERR("%s invalid: dsi is power off\n", __func__);

	primary_display_manual_unlock();
	_primary_path_switch_dst_unlock();

	return ret;
}
