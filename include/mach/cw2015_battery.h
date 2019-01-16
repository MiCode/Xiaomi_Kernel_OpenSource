/*
 * RockChip ADC Battery Driver 
 * Copyright (C) 2012, RockChip
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Authors: xuhuicong <xhc@rock-chips.com>
 *
 * Based on rk30_adc_battery.c

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef CW2015_BATTERY_H
#define CW2015_BATTERY_H

#define SIZE_BATINFO    64 

struct cw_bat_platform_data {

        int is_dc_charge;
        int dc_det_pin;
        int dc_det_level;

        int is_usb_charge;
        int chg_mode_sel_pin;
        int chg_mode_sel_level;

        int bat_low_pin;
        int bat_low_level;
        int chg_ok_pin;
        int chg_ok_level;
        u8* cw_bat_config_info;
};
extern int g_cw2015_capacity;
extern int g_mtk_init_vol;
extern int g_cw2015_vol;
#endif
