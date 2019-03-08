/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef _DISP_ARR_H_
#define _DISP_ARR_H_

/* used by ARR2.0 */
int primary_display_get_cur_refresh_rate(void);
int primary_display_get_max_refresh_rate(void);
int primary_display_get_min_refresh_rate(void);
int primary_display_set_refresh_rate(unsigned int refresh_rate);

#endif /* _DISP_ARR_H_ */
