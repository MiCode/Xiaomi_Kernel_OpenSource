/* Himax Android Driver Sample Code for HX83112 chipset
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

#include "himax_platform.h"
#include "himax_common.h"
#include <linux/slab.h>

#define HIMAX_REG_RETRY_TIMES 5

#ifdef HX_ESD_RECOVERY
extern u8 HX_ESD_RESET_ACTIVATE;
#endif

#ifndef HX_ORG_SELFTEST
#define BS_RAWDATANOISE 10
#define BS_OPENSHORT 0
#define HX_OPPO
#ifdef HX_OPPO
#define BS_LPWUG 1
#define BS_DOZE 1
#define BS_LPWUG_dile 1
#endif

/*  skip notch & dummy */
#define SKIPRXNUM 31

#define SKIP_NOTCH_START 5
#define SKIP_NOTCH_END 10
#define SKIP_DUMMY_START 23 /* TX + SKIP_NOTCH_START */
#define SKIP_DUMMY_END 28 /*  TX + SKIP_NOTCH_END */

/* Himax MP Limit */
#define RAWMIN 1000
#define RAWMAX 9000
#define SHORTMIN 0
#define SHORTMAX 150
#define OPENMIN 50
#define OPENMAX 500
#define M_OPENMIN 0
#define M_OPENMAX 150
#define NOISEFRAME 11/*BS_RAWDATANOISE + 1*/
#define UNIFMAX 500
#ifdef HX_OPPO
#define LPWUG_NOISE_MAX 9999
#define LPWUG_NOISE_MIN 0
#define LPWUG_RAWDATA_MAX 9999
#define LPWUG_RAWDATA_MIN 0
#define DOZE_NOISE_MAX 9999
#define DOZE_NOISE_MIN 0
#define DOZE_RAWDATA_MAX 9999
#define DOZE_RAWDATA_MIN		0
#define LPWUG_IDLE_NOISE_MAX 9999
#define LPWUG_IDLE_NOISE_MIN 0
#define LPWUG_IDLE_RAWDATA_MAX 9999
#define LPWUG_IDLE_RAWDATA_MIN 0
#endif

/* Himax MP Password */
#define PWD_OPEN_START 0x77
#define PWD_OPEN_END 0x88
#define PWD_SHORT_START 0x11
#define PWD_SHORT_END 0x33
#define PWD_RAWDATA_START 0x00
#define PWD_RAWDATA_END 0x99
#define PWD_NOISE_START 0x00
#define PWD_NOISE_END 0x99
#define PWD_SORTING_START 0xAA
#define PWD_SORTING_END 0xCC
#ifdef HX_OPPO
#define PWD_LPWUG_START 0x55
#define PWD_LPWUG_END 0x66
#define PWD_DOZE_START 0x22
#define PWD_DOZE_END 0x44
#define PWD_LPWUG_IDLE_START 0x50
#define PWD_LPWUG_IDLE_END 0x60
#endif

/* Himax DataType */
#define DATA_OPEN 0x0B
#define DATA_MICRO_OPEN 0x0C
#define DATA_SHORT 0x0A
#define DATA_RAWDATA 0x0A
#define DATA_NOISE 0x0F
#define DATA_BACK_NORMAL 0x00
#define DATA_LPWUG_RAWDATA 0x0C
#define DATA_LPWUG_NOISE 0x0F
#define DATA_DOZE_RAWDATA 0x0A
#define DATA_DOZE_NOISE 0x0F
#define DATA_LPWUG_IDLE_RAWDATA 0x0A
#define DATA_LPWUG_IDLE_NOISE 0x0F

/* Himax Data Ready Password */
#define Data_PWD0 0xA5
#define Data_PWD1 0x5A

typedef enum {
	HIMAX_INSPECTION_OPEN,
	HIMAX_INSPECTION_MICRO_OPEN,
	HIMAX_INSPECTION_SHORT,
	HIMAX_INSPECTION_RAWDATA,
	HIMAX_INSPECTION_NOISE,
	HIMAX_INSPECTION_SORTING,
	HIMAX_INSPECTION_BACK_NORMAL,
#ifdef HX_OPPO
	HIMAX_INSPECTION_DOZE_RAWDATA,
	HIMAX_INSPECTION_DOZE_NOISE,
	HIMAX_INSPECTION_LPWUG_RAWDATA,
	HIMAX_INSPECTION_LPWUG_NOISE,
	HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA,
	HIMAX_INSPECTION_LPWUG_IDLE_NOISE,
#endif
} THP_INSPECTION_ENUM;

/* Error code of AFE Inspection */
typedef enum {
	HX_INSPECT_OK = 0, /* OK */
	HX_INSPECT_ESPI = (1 << 0), /* SPI communication error */
	HX_INSPECT_ERAW = (1 << 1), /* Raw data error */
	HX_INSPECT_ENOISE = (1 << 2), /* Noise error */
	HX_INSPECT_EOPEN = (1 << 3), /* Sensor open error */
	HX_INSPECT_EMOPEN = (1 << 4), /* Sensor micro open error */
	HX_INSPECT_ESHORT = (1 << 5), /* Sensor short error */
	HX_INSPECT_ERC = (1 << 6), /* Sensor RC error */
	HX_INSPECT_EPIN = (1 << 7), /* Errors of TSVD! FTSHD! FTRCST! FTRCRQ and other PINs when Report Rate Switching between 60 Hz and 120 Hz */
	HX_INSPECT_EOTHER = (1 << 8) /* All other errors */
} HX_INSPECT_ERR_ENUM;

/*  skip notch | Dummy */
typedef enum {
	SKIPTXNUM_START = 6,
	SKIPTXNUM_6 = SKIPTXNUM_START,
	SKIPTXNUM_7,
	SKIPTXNUM_8,
	SKIPTXNUM_9,
	SKIPTXNUM_END = SKIPTXNUM_9,
} SKIPTXNUMINDEX;

#endif
enum fw_image_type {
	fw_image_32k = 0x01,
	fw_image_48k,
	fw_image_60k,
	fw_image_64k,
	fw_image_124k,
	fw_image_128k,
};

int himax_hand_shaking(struct i2c_client *client);
void himax_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable, bool suspended);
void himax_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable, bool suspended);
void himax_usb_detect_set(struct i2c_client *client, uint8_t *cable_config);
int himax_determin_diag_rawdata(int diag_command);
int himax_determin_diag_storage(int diag_command);
void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command);
void himax_flash_dump_func(struct i2c_client *client, uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer);
int himax_chip_self_test(struct i2c_client *client);
int himax_chip_self_test_open(struct i2c_client *client);
int himax_chip_self_test_short(struct i2c_client *client);
void himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte);
int himax_register_read(struct i2c_client *client, uint8_t *read_addr, int read_length, uint8_t *read_data, bool cfg_flag);
void himax_flash_read(struct i2c_client *client, uint8_t *reg_byte, uint8_t *read_data);
int himax_flash_write_burst(struct i2c_client *client, uint8_t *reg_byte, uint8_t *write_data);
void himax_flash_write_burst_lenth(struct i2c_client *client, uint8_t *reg_byte, uint8_t *write_data, int length);
void himax_register_write(struct i2c_client *client, uint8_t *write_addr, int write_length, uint8_t *write_data, bool cfg_flag);
bool himax_sense_off(struct i2c_client *client);
void himax_interface_on(struct i2c_client *client);
bool wait_wip(struct i2c_client *client, int Timing);
void himax_sense_on(struct i2c_client *client, uint8_t FlashMode);
void himax_chip_erase(struct i2c_client *client);
bool himax_block_erase(struct i2c_client *client, int start_addr, int length);
void himax_flash_programming(struct i2c_client *client, uint8_t *FW_content, int FW_Size);
int himax_check_CRC(struct i2c_client *client, int mode);
int fts_ctpm_fw_upgrade_with_sys_fs_32k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
void himax_touch_information(struct i2c_client *client);
int  himax_read_i2c_status(struct i2c_client *client);
int  himax_read_ic_trigger_type(struct i2c_client *client);
void himax_read_FW_ver(struct i2c_client *client);
bool himax_ic_package_check(struct i2c_client *client);
void himax_power_on_init(struct i2c_client *client);
bool himax_read_event_stack(struct i2c_client *client, uint8_t *buf, uint8_t length);
void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data);
bool himax_calculateChecksum(struct i2c_client *client, bool change_iref);
bool himax_flash_lastdata_check(struct i2c_client *client);
bool himax_program_reload(struct i2c_client *client);
uint8_t himax_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data);
int himax_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr);
void himax_resume_ic_action(struct i2c_client *client);
void himax_suspend_ic_action(struct i2c_client *client);
void himax_lock_down_info(uint8_t *data, int length);

/* ts_work */
int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max);
bool diag_check_sum(struct himax_report_data *hx_touch_data); /* return checksum value */
void diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num, int self_num, uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data);
