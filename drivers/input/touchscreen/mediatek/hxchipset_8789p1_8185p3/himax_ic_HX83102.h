/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83102 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>

#define hx83102ab_fw_addr_sorting_mode_en  0x100007FC
#define hx83102ab_fw_addr_selftest_addr_en 0x100007F8
#define hx83102ab_data_adc_cfg_1           0x10007B00
#define hx83102a_data_df_rx                48
#define hx83102a_data_df_tx                24
#define hx83102a_data_adc_num              100
#define hx83102b_data_adc_num              64
#define hx83102ab_notouch_frame            0

#define hx83102d_fw_addr_raw_out_sel      0x800204f4
#define hx83102d_zf_data_adc_cfg_1        0x10007B00
#define hx83102d_zf_data_adc_cfg_2        0x10006A00
#define hx83102d_zf_data_adc_cfg_3        0x10007500
#define hx83102d_zf_data_bor_prevent_info 0x10007268
#define hx83102d_zf_data_notch_info       0x10007300
#define hx83102d_zf_func_info_en          0x10007FD0
#define hx83102d_zf_po_sub_func           0x10005A00
#define hx83102d_zf_data_sram_start_addr  0x20000000
#define hx83102d_adr_osc_en               0x9000009C
#define hx83102d_adr_osc_pw               0x90000280
#define hx83102d_data_adc_num             48
#define hx83102d_notouch_frame            0

#define hx83102e_fw_addr_raw_out_sel 0x100072EC
#define hx83102e_ic_adr_tcon_rst     0x80020004
#define hx83102e_data_df_rx          48
#define hx83102e_data_df_tx          30
#define hx83102e_data_adc_num        100
#define hx83102e_notouch_frame            0

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif
