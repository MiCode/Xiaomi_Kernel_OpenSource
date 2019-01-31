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

#ifndef __MTK_CHARGER_INTF_H
#define __MTK_CHARGER_INTF_H

#include "mt_charging.h"
#include <linux/types.h>

/* MTK charger interface */
extern int (*mtk_charger_intf[CHARGING_CMD_NUMBER])(void *data);
extern int chr_control_interface(int cmd, void *data);

/*
 * The following interface are not related to charger
 * They are implemented in mtk_charger_intf.c
 */

extern int mtk_charger_set_hv_threshold(void *data);
extern int mtk_charger_get_hv_status(void *data);
extern int mtk_charger_get_battery_status(void *data);
extern int mtk_charger_get_charger_det_status(void *data);
extern int mtk_charger_get_charger_type(void *data);
extern int mtk_charger_get_is_pcm_timer_trigger(void *data);
extern int mtk_charger_set_platform_reset(void *data);
extern int mtk_charger_get_platform_boot_mode(void *data);
extern int hw_charger_type_detection(void);
extern int slp_get_wake_reason(void);

#endif /* __MTK_CHARGER_INTF_H */
