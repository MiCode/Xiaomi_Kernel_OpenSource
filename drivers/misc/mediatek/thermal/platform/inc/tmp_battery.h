/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __TMP_BATTERY_H__
#define __TMP_BATTERY_H__

#include <charging.h>
/* Extern two API functions from battery driver
 * to limit max charging current.
 */
/**
 *  return value means charging current in mA
 *  -1 means error
 *  Implementation in mt_battery.c and mt_battery_fan5405.c
 */
extern int get_bat_charging_current_level(void);

/**
 *  current_limit means limit of charging current in mA
 *  -1 means no limit
 *  Implementation in mt_battery.c and mt_battery_fan5405.c
 */
extern int set_bat_charging_current_limit(int current_limit);
extern enum charger_type mt_get_charger_type(void);

extern int read_tbat_value(void);

#endif	/* __TMP_BATTERY_H__ */

