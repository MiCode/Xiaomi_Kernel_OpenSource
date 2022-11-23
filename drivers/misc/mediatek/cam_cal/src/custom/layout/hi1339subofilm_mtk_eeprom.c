// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "cam_cal_config.h"


static struct STRUCT_CALIBRATION_LAYOUT_STRUCT cal_layout_table = {
    0x00000001, 0x010b00ff, CAM_CAL_SINGLE_EEPROM_DATA,
    {
        {0x00000001, 0x00000000, 0x00000027, custom_do_module_info},
        {0x00000000, 0x00000000, 0x00000000, do_part_number},
        {0x00000001, 0x00000046, 0x0000074F, custom_do_single_lsc},
        {0x00000001, 0x00000027, 0x0000001F, custom_do_awb_gain},
        {0x00000000, 0x000007A0, 0x000005E1, custom_do_pdaf},
        {0x00000000, 0x00000000, 0x00000D81, do_dump_all},
        {0x00000000, 0x00000F80, 0x0000000A, do_lens_id},
    }
};

struct STRUCT_CAM_CAL_CONFIG_STRUCT hi1339subofilm_mtk_eeprom = {
    .name = "hi1339subofilm_mtk_eeprom",
    .check_layout_function = custom_layout_check,
    .read_function = Common_read_region,
    .layout = &cal_layout_table,
    .sensor_id = HI1339SUBOFILM_SENSOR_ID,
    .i2c_write_id = 0xA2,
    .max_size = 0x1000,
    .enable_preload = 1,
    .preload_size = 0x795,
    .has_stored_data = 1,
};
