/* Himax Android Driver Sample Code for debug nodes
*
* Copyright (C) 2017 Himax Corporation.
* Copyright (C) 2019 XiaoMi, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"


#ifdef HX_ESD_RECOVERY
extern u8 HX_ESD_RESET_ACTIVATE;
extern int hx_EB_event_flag;
extern int hx_EC_event_flag;
extern int hx_ED_event_flag;
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define HIMAX_PROC_TOUCH_FOLDER "android_touch"
#define HIMAX_PROC_DEBUG_LEVEL_FILE	"debug_level"
#define HIMAX_PROC_VENDOR_FILE "vendor"
#define HIMAX_PROC_ATTN_FILE "attn"
#define HIMAX_PROC_INT_EN_FILE "int_en"
#define HIMAX_PROC_LAYOUT_FILE "layout"
#define HIMAX_PROC_CRC_TEST_FILE "CRC_test"

static struct proc_dir_entry *himax_touch_proc_dir;
static struct proc_dir_entry *himax_proc_debug_level_file;
static struct proc_dir_entry *himax_proc_vendor_file;
static struct proc_dir_entry *himax_proc_attn_file;
static struct proc_dir_entry *himax_proc_int_en_file;
static struct proc_dir_entry *himax_proc_layout_file;
static struct proc_dir_entry *himax_proc_CRC_test_file;

uint8_t HX_PROC_SEND_FLAG;

extern int himax_touch_proc_init(void);
extern void himax_touch_proc_deinit(void);
bool getFlashDumpGoing(void);

extern int himax_int_en_set(struct i2c_client *client);

#ifdef HX_TP_PROC_GUEST_INFO
#define HIMAX_PROC_GUEST_INFO_FILE		"guest_info"
static struct proc_dir_entry *himax_proc_guest_info_file;
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
#define HIMAX_PROC_ITO_TEST_FILE		"ITO_test"
static struct proc_dir_entry *himax_proc_ito_test_file;

extern void ito_set_step_status(uint8_t status);
extern uint8_t ito_get_step_status(void);
extern void ito_set_result_status(uint8_t status);
extern uint8_t ito_get_result_status(void);

#endif

#ifdef HX_TP_PROC_REGISTER
#define HIMAX_PROC_REGISTER_FILE	"register"
struct proc_dir_entry *himax_proc_register_file;
uint8_t byte_length = 0;
uint8_t register_command[4];
bool cfg_flag = false;
#endif

#ifdef HX_TP_PROC_DIAG
#define HIMAX_PROC_DIAG_FILE	"tp_data_dump"
struct proc_dir_entry *himax_proc_diag_file;
#define HIMAX_PROC_DIAG_ARR_FILE	"diag_arr"
struct proc_dir_entry *himax_proc_diag_arrange_file;
struct file *diag_sram_fn;
uint8_t write_counter = 0;
uint8_t write_max_count = 30;
#define IIR_DUMP_FILE "/sdcard/HX_IIR_Dump.txt"
#define DC_DUMP_FILE "/sdcard/HX_DC_Dump.txt"
#define BANK_DUMP_FILE "/sdcard/HX_BANK_Dump.txt"

#ifdef HX_TP_PROC_2T2R
static uint8_t x_channel_2;
static uint8_t y_channel_2;
static uint32_t *diag_mutual_2;

int32_t *getMutualBuffer_2(void);
uint8_t getXChannel_2(void);
uint8_t getYChannel_2(void);

void setMutualBuffer_2(void);
void setXChannel_2(uint8_t x);
void setYChannel_2(uint8_t y);
#endif
uint8_t x_channel = 0;
uint8_t y_channel = 0;
int32_t *diag_mutual = NULL;
int32_t *diag_mutual_new = NULL;
int32_t *diag_mutual_old = NULL;
uint8_t diag_max_cnt = 0;
uint8_t hx_state_info[2] = {0};

int g_diag_command = 0;
uint8_t diag_coor[128];/*  = {0xFF}; */
int32_t diag_self[100] = {0};

int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
uint8_t getDiagCommand(void);
uint8_t getXChannel(void);
uint8_t getYChannel(void);

void setMutualBuffer(void);
void setMutualNewBuffer(void);
void setMutualOldBuffer(void);
void setXChannel(uint8_t x);
void setYChannel(uint8_t y);
#endif

#ifdef HX_TP_PROC_DEBUG
#define HIMAX_PROC_DEBUG_FILE	"debug"
struct proc_dir_entry *himax_proc_debug_file = NULL;
#define HIMAX_PROC_FW_DEBUG_FILE	"FW_debug"
struct proc_dir_entry *himax_proc_fw_debug_file = NULL;
#define HIMAX_PROC_DD_DEBUG_FILE	"DD_debug"
struct proc_dir_entry *himax_proc_dd_debug_file = NULL;

bool fw_update_complete = false;
int handshaking_result = 0;
unsigned char debug_level_cmd = 0;
unsigned char upgrade_fw[128*1024];
uint8_t cmd_set[8];
uint8_t mutual_set_flag = 0;
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
#define HIMAX_PROC_FLASH_DUMP_FILE	"flash_dump"
struct proc_dir_entry *himax_proc_flash_dump_file = NULL;

static int Flash_Size = 131072;
static uint8_t *flash_buffer;
static uint8_t flash_command;
static uint8_t flash_read_step;
static uint8_t flash_progress;
static uint8_t flash_dump_complete;
static uint8_t flash_dump_fail;
static uint8_t sys_operation;
static bool	flash_dump_going;

static uint8_t getFlashDumpComplete(void);
static uint8_t getFlashDumpFail(void);
static uint8_t getFlashDumpProgress(void);
static uint8_t getFlashReadStep(void);
uint8_t getFlashCommand(void);
uint8_t getSysOperation(void);

static void setFlashCommand(uint8_t command);
static void setFlashReadStep(uint8_t step);

void setFlashBuffer(void);
void setFlashDumpComplete(uint8_t complete);
void setFlashDumpFail(uint8_t fail);
void setFlashDumpProgress(uint8_t progress);
void setSysOperation(uint8_t operation);
void setFlashDumpGoing(bool going);

#endif

#ifdef HX_TP_PROC_SELF_TEST
#define HIMAX_PROC_SELF_TEST_FILE	"tp_selftest"
struct proc_dir_entry *himax_proc_self_test_file = NULL;
uint32_t **raw_data_array;
uint8_t X_NUM = 0, Y_NUM = 0;
uint8_t sel_type = 0x0D;
#endif

#ifdef HX_TP_PROC_LOCK_DOWN
#define HIMAX_PROC_LOCK_DOWN_FILE	"tp_lockdown"
struct proc_dir_entry *himax_proc_lock_down_file = NULL;
#endif

#ifdef HX_TP_PROC_RESET
#define HIMAX_PROC_RESET_FILE		"reset"
struct proc_dir_entry *himax_proc_reset_file = NULL;
#endif

#ifdef HX_HIGH_SENSE
#define HIMAX_PROC_HSEN_FILE "HSEN"
struct proc_dir_entry *himax_proc_HSEN_file = NULL;
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
#define HIMAX_PROC_SENSE_ON_OFF_FILE "SenseOnOff"
struct proc_dir_entry *himax_proc_SENSE_ON_OFF_file = NULL;
#endif

#ifdef HX_SMART_WAKEUP
#define HIMAX_PROC_SMWP_FILE "SMWP"
struct proc_dir_entry *himax_proc_SMWP_file = NULL;
#define HIMAX_PROC_GESTURE_FILE "GESTURE"
struct proc_dir_entry *himax_proc_GESTURE_file = NULL;
uint8_t HX_SMWP_EN = 0;
/* extern bool FAKE_POWER_KEY_SEND; */
#endif

#ifdef HX_ESD_RECOVERY
#define HIMAX_PROC_ESD_CNT_FILE "ESD_cnt"
struct proc_dir_entry *himax_proc_ESD_cnt_file = NULL;
#endif

#endif

#endif
