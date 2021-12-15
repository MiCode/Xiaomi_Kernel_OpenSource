/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef H_HIMAX_IC
#define H_HIMAX_IC

#include "himax_common.h"
#include "himax_platform.h"

#include <linux/slab.h>

#define HIMAX_REG_RETRY_TIMES 5

#define HX_CMD_NOP 0x00
#define HX_CMD_SETMICROOFF 0x35
#define HX_CMD_SETROMRDY 0x36
#define HX_CMD_TSSLPIN 0x80
#define HX_CMD_TSSLPOUT 0x81
#define HX_CMD_TSSOFF 0x82
#define HX_CMD_TSSON 0x83
#define HX_CMD_ROE 0x85
#define HX_CMD_RAE 0x86
#define HX_CMD_RLE 0x87
#define HX_CMD_CLRES 0x88
#define HX_CMD_TSSWRESET 0x9E
#define HX_CMD_SETDEEPSTB 0xD7
#define HX_CMD_SET_CACHE_FUN 0xDD
#define HX_CMD_SETIDLE 0xF2
#define HX_CMD_SETIDLEDELAY 0xF3
#define HX_CMD_SELFTEST_BUFFER 0x8D
#define HX_CMD_MANUALMODE 0x42
#define HX_CMD_FLASH_ENABLE 0x43
#define HX_CMD_FLASH_SET_ADDRESS 0x44
#define HX_CMD_FLASH_WRITE_REGISTER 0x45
#define HX_CMD_FLASH_SET_COMMAND 0x47
#define HX_CMD_FLASH_WRITE_BUFFER 0x48
#define HX_CMD_FLASH_PAGE_ERASE 0x4D
#define HX_CMD_FLASH_SECTOR_ERASE 0x4E
#define HX_CMD_CB 0xCB
#define HX_CMD_EA 0xEA
#define HX_CMD_4A 0x4A
#define HX_CMD_4F 0x4F
#define HX_CMD_B9 0xB9
#define HX_CMD_76 0x76

#define HX_VER_FW_MAJ 0x33
#define HX_VER_FW_MIN 0x32
#define HX_VER_FW_CFG 0x39

#ifdef HX_ESD_RECOVERY
extern u8 HX_ESD_RESET_ACTIVATE;
#endif

extern unsigned char IC_TYPE;
extern unsigned char IC_CHECKSUM;

enum fw_image_type {
	fw_image_32k = 0x01,
	fw_image_60k,
	fw_image_64k,
	fw_image_124k,
	fw_image_128k,
};

int himax_hand_shaking(struct i2c_client *client);
void himax_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable,
			   bool suspended);
void himax_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable,
			   bool suspended);
void himax_usb_detect_set(struct i2c_client *client, uint8_t *cable_config);
int himax_determin_diag_rawdata(int diag_command);
int himax_determin_diag_storage(int diag_command);
void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command);
void himax_flash_dump_func(struct i2c_client *client,
			   uint8_t local_flash_command, int Flash_Size,
			   uint8_t *flash_buffer);
int himax_chip_self_test(struct i2c_client *client);
void himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte);
void himax_register_read(struct i2c_client *client, uint8_t *read_addr,
			 int read_length, uint8_t *read_data, bool cfg_flag);
void himax_flash_read(struct i2c_client *client, uint8_t *reg_byte,
		      uint8_t *read_data);
void himax_flash_write_burst(struct i2c_client *client, uint8_t *reg_byte,
			     uint8_t *write_data);
void himax_flash_write_burst_length(struct i2c_client *client,
				    uint8_t *reg_byte, uint8_t *write_data,
				    int length);
void himax_register_write(struct i2c_client *client, uint8_t *write_addr,
			  int write_length, uint8_t *write_data, bool cfg_flag);
void himax_sense_off(struct i2c_client *client);
void himax_interface_on(struct i2c_client *client);
bool wait_wip(struct i2c_client *client, int Timing);
void himax_sense_on(struct i2c_client *client, uint8_t FlashMode);
void himax_chip_erase(struct i2c_client *client);
bool himax_block_erase(struct i2c_client *client);
bool himax_sector_erase(struct i2c_client *client, int start_addr);
void himax_sram_write(struct i2c_client *client, uint8_t *FW_content);
bool himax_sram_verify(struct i2c_client *client, uint8_t *FW_File,
		       int FW_Size);
void himax_flash_programming(struct i2c_client *client, uint8_t *FW_content,
			     int FW_Size);
int fts_ctpm_fw_upgrade_with_sys_fs_32k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client,
					 unsigned char *fw, int len,
					 bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client,
					 unsigned char *fw, int len,
					 bool change_iref);
void himax_touch_information(struct i2c_client *client);
int himax_read_i2c_status(struct i2c_client *client);
int himax_read_ic_trigger_type(struct i2c_client *client);
void himax_read_FW_ver(struct i2c_client *client);
bool himax_ic_package_check(struct i2c_client *client);
void himax_power_on_init(struct i2c_client *client);
int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max);
bool himax_read_event_stack(struct i2c_client *client, uint8_t *buf_ts,
			    int length);
void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data);
bool diag_check_sum(
	struct himax_report_data *hx_touch_data); /* return checksum value */
void himax_get_raw_data(uint8_t diag_command, uint16_t mutual_num,
			uint16_t self_num);
void diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num,
			 int self_num, uint8_t diag_cmd, int32_t *mutual_data,
			 int32_t *self_data);
bool himax_calculateChecksum(struct i2c_client *client, bool change_iref);
bool himax_flash_lastdata_check(struct i2c_client *client);
uint8_t himax_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data);
int himax_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr);
void himax_resume_ic_action(struct i2c_client *client);
void himax_suspend_ic_action(struct i2c_client *client);
void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);

#ifdef HX_CHIP_STATUS_MONITOR
extern struct chip_monitor_data *g_chip_monitor_data;
#endif

extern uint8_t getFlashCommand(void);

extern void setFlashDumpComplete(uint8_t complete);
extern void setFlashDumpFail(uint8_t fail);
extern void setFlashDumpProgress(uint8_t progress);
extern void setSysOperation(uint8_t operation);
extern void setFlashDumpGoing(bool going);

#ifdef HX_USB_DETECT_GLOBAL
/* extern kal_bool upmu_is_chr_det(void); */
extern void himax_cable_detect_func(bool force_renew);
#endif

extern int himax_loadSensorConfig(struct i2c_client *client,
				  struct himax_i2c_platform_data *pdata);

#ifdef HX_RST_PIN_FUNC
extern int himax_report_data_init(void);
void calculate_point_number(void);
extern void himax_rst_gpio_set(int pinnum, uint8_t value);
extern u8 HX_HW_RESET_ACTIVATE;

#endif

extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long FW_CFG_VER_FLASH_ADDR;

extern unsigned long FW_VER_MAJ_FLASH_LENG;
extern unsigned long FW_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
extern int g_i_FW_VER;
extern int g_i_CFG_VER;
extern int g_i_CID_MAJ;
extern int g_i_CID_MIN;
extern unsigned char i_CTPM_FW[];
#endif

extern struct himax_ic_data *ic_data;

#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
extern int g_diag_command;
#endif

extern int i2c_error_count;

#endif
