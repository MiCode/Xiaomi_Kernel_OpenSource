/*
 *  Richtek Charger Interface for Mediatek
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  ShuFanLee <shufan_lee@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef __MTK_CHARGER_INTF_H
#define __MTK_CHARGER_INTF_H

#include <linux/types.h>
#include "mt_charging.h"

/* MTK charger interface */
extern int(*mtk_charger_intf[CHARGING_CMD_NUMBER])(void *data);
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
