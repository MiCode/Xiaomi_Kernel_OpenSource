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

#ifndef _DISP_ARR_H_
#define _DISP_ARR_H_

/* used by ARR2.0 */
int primary_display_get_cur_refresh_rate(void);
int primary_display_get_max_refresh_rate(void);
int primary_display_get_min_refresh_rate(void);
int primary_display_set_refresh_rate(unsigned int refresh_rate);

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
typedef void (*FPS_CHG_CALLBACK)(unsigned int new_fps);
void disp_fps_chg_cb_init(void);
void disp_invoke_fps_chg_callbacks(unsigned int new_fps);
int disp_register_fps_chg_callback(
	FPS_CHG_CALLBACK fps_chg_cb);
int disp_unregister_fps_chg_callback(
	FPS_CHG_CALLBACK fps_chg_cb);
#endif

#endif /* _DISP_ARR_H_ */
