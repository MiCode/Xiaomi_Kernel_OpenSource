/*
 * Himax Android Driver Sample Code for debug nodes
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"

#define HIMAX_PROC_DEBUG_LEVEL_FILE "debug_level"
#define HIMAX_PROC_VENDOR_FILE "vendor"
#define HIMAX_PROC_ATTN_FILE "attn"
#define HIMAX_PROC_INT_EN_FILE "int_en"
#define HIMAX_PROC_LAYOUT_FILE "layout"
#define HIMAX_PROC_CRC_TEST_FILE "CRC_test"

#ifdef HX_ESD_RECOVERY
extern u8 HX_ESD_RESET_ACTIVATE;
extern int hx_EB_event_flag;
extern int hx_EC_event_flag;
extern int hx_ED_event_flag;
#endif

#ifdef HX_TP_PROC_2T2R
	extern bool Is_2T2R;
#endif

extern bool DSRAM_Flag;

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
extern int himax_int_en_set(void);

extern int himax_debug_init(void);
extern int himax_debug_remove(void);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
#define HIMAX_PROC_ITO_TEST_FILE "ITO_test"
static struct proc_dir_entry *himax_proc_ito_test_file;

extern void ito_set_step_status(uint8_t status);
extern uint8_t ito_get_step_status(void);
extern void ito_set_result_status(uint8_t status);
extern uint8_t ito_get_result_status(void);
#endif

#endif
