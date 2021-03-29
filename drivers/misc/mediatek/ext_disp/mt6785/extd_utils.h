/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef EXTD_UTILS_H
#define EXTD_UTILS_H

#include <asm/ioctl.h>
#include "mtk_extd_mgr.h"

int extd_mutex_init(struct mutex *m);
int extd_sw_mutex_lock(struct mutex *m);
int extd_mutex_trylock(struct mutex *m);
int extd_sw_mutex_unlock(struct mutex *m);
int extd_msleep(unsigned int ms);
long int extd_get_time_us(void);
char *_extd_ioctl_spy(unsigned int cmd);

#endif
