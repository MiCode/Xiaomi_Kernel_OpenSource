// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ___GC5035_MTK_EEPROM_H__
#define ___GC5035_MTK_EEPROM_H__

#define  GROUP_NUM   3

struct STRUCT_GC5035_CAL_MODULE_INFO {
      unsigned char module_id;
      unsigned char part_number[3];
      unsigned char sensor_id;
      unsigned char lens_id;
      unsigned char vcm_id;
      unsigned char year;
      unsigned char month;
      unsigned char phase;
      unsigned char mirror_flip_status;
      unsigned char ir_filter_id;
};

struct STRUCT_GC5035_CAL_AWB_INFO {
      unsigned char golden_awb_r_h;
      unsigned char golden_awb_r_l;
      unsigned char golden_awb_gr_h;
      unsigned char golden_awb_gr_l;
      unsigned char golden_awb_gb_h;
      unsigned char golden_awb_gb_l;
      unsigned char golden_awb_b_h;
      unsigned char golden_awb_b_l;

      unsigned char golden_awb_r_over_g_h;
      unsigned char golden_awb_r_over_g_l;
      unsigned char golden_awb_b_over_g_h;
      unsigned char golden_awb_b_over_g_l;
      unsigned char golden_awb_gr_over_gb_h;
      unsigned char golden_awb_gr_over_gb_l;

      unsigned char awb_r_h;
      unsigned char awb_r_l;
      unsigned char awb_gr_h;
      unsigned char awb_gr_l;
      unsigned char awb_gb_h;
      unsigned char awb_gb_l;
      unsigned char awb_b_h;
      unsigned char awb_b_l;

      unsigned char awb_r_over_g_h;
      unsigned char awb_r_over_g_l;
      unsigned char awb_b_over_g_h;
      unsigned char awb_b_over_g_l;
      unsigned char awb_gr_over_gb_h;
      unsigned char awb_gr_over_gb_l;
};

struct STRUCT_GC5035_CAL_DATA_INFO {
    unsigned char flag;
    struct STRUCT_GC5035_CAL_MODULE_INFO module_info;
    struct STRUCT_GC5035_CAL_AWB_INFO awb_info;
    unsigned char checksum;
};

#endif
