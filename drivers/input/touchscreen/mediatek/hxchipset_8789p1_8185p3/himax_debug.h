/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for debug nodes
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

#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"


#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
extern int hx_EB_event_flag;
extern int hx_EC_event_flag;
extern int hx_EE_event_flag;
#endif

#define HIMAX_PROC_PEN_POS_FILE "pen_pos"

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
extern int himax_int_en_set(void);

extern uint8_t byte_length;
extern uint8_t register_command[4];
extern uint8_t cfg_flag;

#define HIMAX_PROC_DIAG_FOLDER "diag"
struct proc_dir_entry *himax_proc_diag_dir;
#define HIMAX_PROC_STACK_FILE "stack"
extern struct proc_dir_entry *himax_proc_stack_file;
#define HIMAX_PROC_DELTA_FILE "delta_s"
extern struct proc_dir_entry *himax_proc_delta_file;
#define HIMAX_PROC_DC_FILE "dc_s"
extern struct proc_dir_entry *himax_proc_dc_file;
#define HIMAX_PROC_BASELINE_FILE "baseline_s"
extern struct proc_dir_entry *himax_proc_baseline_file;

#if defined(HX_TP_PROC_2T2R)
extern uint32_t *diag_mutual_2;

int32_t	*getMutualBuffer_2(void);
void setMutualBuffer_2(uint8_t x_num, uint8_t y_num);
#endif
extern int32_t *diag_mutual;
extern int32_t *diag_mutual_new;
extern int32_t *diag_mutual_old;
extern uint8_t diag_max_cnt;
extern uint8_t hx_state_info[2];
extern uint8_t diag_coor[128];
extern int32_t *diag_self;
extern int32_t *diag_self_new;
extern int32_t *diag_self_old;
int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
int32_t *getSelfNewBuffer(void);
int32_t *getSelfOldBuffer(void);
void setMutualBuffer(uint8_t x_num, uint8_t y_num);
void setMutualNewBuffer(uint8_t x_num, uint8_t y_num);
void setMutualOldBuffer(uint8_t x_num, uint8_t y_num);
uint8_t process_type;
uint8_t mode_flag;
uint8_t overflow;

#define HIMAX_PROC_DEBUG_FILE	"debug"
extern struct proc_dir_entry *himax_proc_debug_file;
extern bool	fw_update_complete;
extern int handshaking_result;
extern unsigned char debug_level_cmd;
extern uint8_t cmd_set[8];
extern uint8_t mutual_set_flag;

#define HIMAX_PROC_FLASH_DUMP_FILE	"flash_dump"
extern struct proc_dir_entry *himax_proc_flash_dump_file;
extern int Flash_Size;
extern uint8_t *flash_buffer;
extern uint8_t g_flash_cmd;
extern uint8_t g_flash_progress;
extern bool g_flash_dump_rst; /*Fail = 0, Pass = 1*/
void setFlashBuffer(void);

enum flash_dump_prog {
	START,
	ONGOING,
	FINISHED,
};

extern uint32_t **raw_data_array;
extern uint8_t X_NUM4;
extern uint8_t Y_NUM;
extern uint8_t sel_type;

/* Moved from debug.c */
extern struct himax_debug *debug_data;
extern unsigned char IC_CHECKSUM;
extern int i2c_error_count;
extern struct proc_dir_entry *himax_touch_proc_dir;

#if defined(HX_TP_PROC_GUEST_INFO)
extern struct hx_guest_info *g_guest_info_data;
extern char *g_guest_info_item[];
#endif

extern int himax_input_register(struct himax_ts_data *ts);
#if defined(HX_TP_PROC_2T2R)
extern bool Is_2T2R;
#endif

#if defined(HX_RST_PIN_FUNC)
extern void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);
#endif

extern uint8_t HX_PROC_SEND_FLAG;
extern struct himax_target_report_data *g_target_report_data;
extern struct himax_report_data *hx_touch_data;
extern int g_ts_dbg;

/* Moved from debug.c end */
#define BUF_SIZE 1024
#define CMD_NUM 15
char *dbg_cmd_str[] = {
	"crc_test",
	"fw_debug",
	"attn",
	"layout",
	"dd_debug",
	"excp_cnt",
	"senseonoff",
	"debug_level",
	"guest_info",
	"int_en",
	"register",
	"reset",
	"diag_arr",
	"diag",
	NULL
};

int dbg_cmd_flag;
char *dbg_cmd_par;
ssize_t (*dbg_func_ptr_r[CMD_NUM])(char *buf, size_t len);
ssize_t (*dbg_func_ptr_w[CMD_NUM])(char *buf, size_t len);
#endif
