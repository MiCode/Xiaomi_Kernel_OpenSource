/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _AOL_GEOFENCE_H_
#define _AOL_GEOFENCE_H_

#include <linux/types.h>
#include <linux/compiler.h>

int aol_geofence_init(void);
void aol_geofence_deinit(void);

int aol_geofence_bind_to_conap(void);

#endif // _AOL_GEOFENCE_H_
