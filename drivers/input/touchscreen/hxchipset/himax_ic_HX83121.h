/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83121 chipset
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

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif

#define hx83121a_fw_addr_raw_out_sel 0x100072EC
#define hx83121a_data_df_rx          60
#define hx83121a_data_df_tx          40
#define hx83121a_data_adc_num        1280
#define hx83121a_notouch_frame       0
#define hx83121a_ic_cmd_incr4        0x12

#define HX83121A_FLASH_SIZE 261120

