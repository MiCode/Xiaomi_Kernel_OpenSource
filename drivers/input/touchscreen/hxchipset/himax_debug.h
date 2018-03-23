/* Himax Android Driver Sample Code for Himax chipset
*
* Copyright (C) 2015 Himax Corporation.
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

#include "himax_platform.h"
#include "himax_common.h"

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	#define HIMAX_PROC_TOUCH_FOLDER 	"android_touch"
	#define HIMAX_PROC_DEBUG_LEVEL_FILE	"debug_level"
	#define HIMAX_PROC_VENDOR_FILE		"vendor"
	#define HIMAX_PROC_ATTN_FILE		"attn"
	#define HIMAX_PROC_INT_EN_FILE		"int_en"
	#define HIMAX_PROC_LAYOUT_FILE		"layout"

	static struct proc_dir_entry *himax_touch_proc_dir;
	static struct proc_dir_entry *himax_proc_debug_level_file;
	static struct proc_dir_entry *himax_proc_vendor_file;
	static struct proc_dir_entry *himax_proc_attn_file;
	static struct proc_dir_entry *himax_proc_int_en_file;
	static struct proc_dir_entry *himax_proc_layout_file;

	uint8_t HX_PROC_SEND_FLAG;

extern int himax_touch_proc_init(void);
extern void himax_touch_proc_deinit(void);
bool getFlashDumpGoing(void);

#ifdef HX_TP_PROC_REGISTER
	#define HIMAX_PROC_REGISTER_FILE	"register"
	struct proc_dir_entry *himax_proc_register_file;
	uint8_t register_command[4];
#endif

#ifdef HX_TP_PROC_DIAG
	#define HIMAX_PROC_DIAG_FILE	"diag"
	struct proc_dir_entry *himax_proc_diag_file;
	#define HIMAX_PROC_DIAG_ARR_FILE	"diag_arr"
	struct proc_dir_entry *himax_proc_diag_arrange_file;

#ifdef HX_TP_PROC_2T2R
	static bool Is_2T2R;
	static uint8_t x_channel_2;
	static uint8_t y_channel_2;
	static uint8_t *diag_mutual_2;
	
	int16_t *getMutualBuffer_2(void);
	uint8_t 	getXChannel_2(void);
	uint8_t 	getYChannel_2(void);
	
	void 	setMutualBuffer_2(void);
	void 	setXChannel_2(uint8_t x);
	void 	setYChannel_2(uint8_t y);
#endif
	uint8_t x_channel;
	uint8_t y_channel;
	int16_t *diag_mutual;
	int16_t *diag_mutual_new;
	int16_t *diag_mutual_old;
	uint8_t diag_max_cnt;

	int diag_command;
	uint8_t diag_coor[128];// = {0xFF};
	int16_t diag_self[100] = {0};

	int16_t *getMutualBuffer(void);
	int16_t *getMutualNewBuffer(void);
	int16_t *getMutualOldBuffer(void);
	int16_t *getSelfBuffer(void);
	uint8_t 	getDiagCommand(void);
	uint8_t 	getXChannel(void);
	uint8_t 	getYChannel(void);
	
	void 	setMutualBuffer(void);
	void 	setMutualNewBuffer(void);
	void 	setMutualOldBuffer(void);
	void 	setXChannel(uint8_t x);
	void 	setYChannel(uint8_t y);
	uint8_t	coordinate_dump_enable = 0;
	struct file	*coordinate_fn;
#endif

#ifdef HX_TP_PROC_DEBUG
	#define HIMAX_PROC_DEBUG_FILE	"debug"
	struct proc_dir_entry *himax_proc_debug_file = NULL;

	bool	fw_update_complete = false;
	int handshaking_result = 0;
	unsigned char debug_level_cmd = 0;
	unsigned char upgrade_fw[128*1024];
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	#define HIMAX_PROC_FLASH_DUMP_FILE	"flash_dump"
	struct proc_dir_entry *himax_proc_flash_dump_file = NULL;

	static int Flash_Size = 131072;
	static uint8_t *flash_buffer 				= NULL;
	static uint8_t flash_command 				= 0;
	static uint8_t flash_read_step 			= 0;
	static uint8_t flash_progress 			= 0;
	static uint8_t flash_dump_complete	= 0;
	static uint8_t flash_dump_fail 			= 0;
	static uint8_t sys_operation				= 0;
	static uint8_t flash_dump_sector	 	= 0;
	static uint8_t flash_dump_page 			= 0;
	static bool    flash_dump_going			= false;

	static uint8_t getFlashCommand(void);
	static uint8_t getFlashDumpComplete(void);
	static uint8_t getFlashDumpFail(void);
	static uint8_t getFlashDumpProgress(void);
	static uint8_t getFlashReadStep(void);
	//static uint8_t getFlashDumpSector(void);
	//static uint8_t getFlashDumpPage(void);

	void setFlashBuffer(void);
	uint8_t getSysOperation(void);

	static void setFlashCommand(uint8_t command);
	static void setFlashReadStep(uint8_t step);
	static void setFlashDumpComplete(uint8_t complete);
	static void setFlashDumpFail(uint8_t fail);
	static void setFlashDumpProgress(uint8_t progress);
	void setSysOperation(uint8_t operation);
	static void setFlashDumpSector(uint8_t sector);
	static void setFlashDumpPage(uint8_t page);
	static void setFlashDumpGoing(bool going);

#endif

#ifdef HX_TP_PROC_SELF_TEST
	#define HIMAX_PROC_SELF_TEST_FILE	"self_test"
	struct proc_dir_entry *himax_proc_self_test_file = NULL;
	uint32_t **raw_data_array;
	uint8_t X_NUM = 0, Y_NUM = 0;
	uint8_t sel_type = 0x0D;
#endif

#ifdef HX_TP_PROC_RESET
#define HIMAX_PROC_RESET_FILE		"reset"
extern void himax_HW_reset(uint8_t loadconfig,uint8_t int_off);
struct proc_dir_entry *himax_proc_reset_file 		= NULL;
#endif

#ifdef HX_HIGH_SENSE
	#define HIMAX_PROC_HSEN_FILE "HSEN"
	struct proc_dir_entry *himax_proc_HSEN_file = NULL;
#endif

#ifdef HX_TP_PROC_SENSE_ON_OFF
	#define HIMAX_PROC_SENSE_ON_OFF_FILE "SenseOnOff"
	struct proc_dir_entry *himax_proc_SENSE_ON_OFF_file = NULL;
#endif

#ifdef HX_RST_PIN_FUNC
	void himax_HW_reset(uint8_t loadconfig,uint8_t int_off);
#endif

#ifdef HX_SMART_WAKEUP
#define HIMAX_PROC_SMWP_FILE "SMWP"
struct proc_dir_entry *himax_proc_SMWP_file = NULL;
#define HIMAX_PROC_GESTURE_FILE "GESTURE"
struct proc_dir_entry *himax_proc_GESTURE_file = NULL;
uint8_t HX_SMWP_EN = 0;
//extern bool FAKE_POWER_KEY_SEND;
#endif

#endif

