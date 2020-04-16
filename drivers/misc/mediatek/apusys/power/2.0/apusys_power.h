/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _APUSYS_POWER_DRIVER_H_
#define _APUSYS_POWER_DRIVER_H_

#include "apusys_power_cust.h"


#ifndef BUILD_POLICY_TEST
extern int apu_power_device_register(enum DVFS_USER, struct platform_device*);
#endif
extern void apu_power_device_unregister(enum DVFS_USER);
extern int apu_device_power_on(enum DVFS_USER);
extern int apu_device_power_off(enum DVFS_USER);
extern void apu_device_set_opp(enum DVFS_USER user, uint8_t opp);
extern void apu_get_power_info(void);
extern void apu_power_on_callback(void);
extern int apu_device_power_off(enum DVFS_USER user);
extern int apu_power_callback_device_register(enum POWER_CALLBACK_USER user,
					void (*power_on_callback)(void *para),
					void (*power_off_callback)(void *para));
extern void apu_power_callback_device_unregister(enum POWER_CALLBACK_USER user);

extern int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER user, uint8_t opp);
extern int32_t apusys_thermal_dis_throttle_cb(enum DVFS_USER user);

#endif
