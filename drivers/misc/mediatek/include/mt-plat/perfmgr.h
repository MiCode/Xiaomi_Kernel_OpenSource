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

#ifndef __PERFMGR_H__
#define __PERFMGR_H__

extern int init_perfmgr_touch(void);
extern int perfmgr_touch_suspend(void);

extern int  perfmgr_get_target_core(void);
extern int  perfmgr_get_target_freq(void);
extern void perfmgr_boost(int enable, int core, int freq);

#endif				/* !__PERFMGR_H__ */
