/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef EXTD_UTILS_H
#define EXTD_UTILS_H

#include <asm/ioctl.h>
#include "mtk_extd_mgr.h"

int extd_mutex_init(struct mutex *m);
int extd_sw_mutex_lock(struct mutex *m);
int extd_mutex_trylock(struct mutex *m);
int extd_sw_mutex_unlock(struct mutex *m);
int extd_msleep(unsigned int ms);
int extd_get_time_us(void);
char *_extd_ioctl_spy(unsigned int cmd);

#endif
