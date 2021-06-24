/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_POWER_H__
#define __APUSYS_REVISER_POWER_H__
#include <linux/types.h>

bool reviser_is_power(void *drvinfo);
int reviser_power_on(void *drvinfo);
int reviser_power_off(void *drvinfo);

#endif
