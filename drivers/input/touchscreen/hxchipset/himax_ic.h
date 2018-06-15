/* Himax Android Driver Sample Code for HMX83100 chipset
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

#include <linux/slab.h>

#define HX_85XX_A_SERIES_PWON		1
#define HX_85XX_B_SERIES_PWON		2
#define HX_85XX_C_SERIES_PWON		3
#define HX_85XX_D_SERIES_PWON		4
#define HX_85XX_E_SERIES_PWON		5
#define HX_85XX_ES_SERIES_PWON		6
#define HX_85XX_F_SERIES_PWON		7
#define HX_83100_SERIES_PWON		8

#define HX_TP_BIN_CHECKSUM_SW		1
#define HX_TP_BIN_CHECKSUM_HW		2
#define HX_TP_BIN_CHECKSUM_CRC	3

enum fw_image_type {
	fw_image_60k	= 0x01,
	fw_image_64k,
	fw_image_124k,
	fw_image_128k,
};

int himax_hand_shaking(struct i2c_client *client);
void himax_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable);
void himax_get_SMWP_enable(struct i2c_client *client, uint8_t *tmp_data);
void himax_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable);
void himax_get_HSEN_enable(struct i2c_client *client, uint8_t *tmp_data);
void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command);

void himax_flash_dump_func(struct i2c_client *client,
uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer);

int himax_chip_self_test(struct i2c_client *client);

/*himax_83100_BURST_INC0_EN*/
int himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte);

/*RegisterRead83100*/
void himax_register_read(struct i2c_client *client,
	uint8_t *read_addr, int read_length, uint8_t *read_data);

/*himax_83100_Flash_Read*/
void himax_flash_read(struct i2c_client *client,
	uint8_t *reg_byte, uint8_t *read_data);

/*himax_83100_Flash_Write_Burst*/
void himax_flash_write_burst(struct i2c_client *client,
	uint8_t *reg_byte, uint8_t *write_data);

/*himax_83100_Flash_Write_Burst_length*/
int himax_flash_write_burst_length(struct i2c_client *client,
	uint8_t *reg_byte, uint8_t *write_data, int length);

/*RegisterWrite83100*/
int himax_register_write(struct i2c_client *client,
	uint8_t *write_addr, int write_length, uint8_t *write_data);

/*himax_83100_SenseOff*/
void himax_sense_off(struct i2c_client *client);
/*himax_83100_Interface_on*/
void himax_interface_on(struct i2c_client *client);
bool wait_wip(struct i2c_client *client, int Timing);

/*himax_83100_SenseOn*/
void himax_sense_on(struct i2c_client *client,
	uint8_t FlashMode);

/*himax_83100_Chip_Erase*/
void himax_chip_erase(struct i2c_client *client);
/*himax_83100_Block_Erase*/
bool himax_block_erase(struct i2c_client *client);

/*himax_83100_Sector_Erase*/
bool himax_sector_erase(struct i2c_client *client, int start_addr);

/*himax_83100_Sram_Write*/
void himax_sram_write(struct i2c_client *client, uint8_t *FW_content);

/*himax_83100_Sram_Verify*/
bool himax_sram_verify(struct i2c_client *client,
	uint8_t *FW_File, int FW_Size);

/*himax_83100_Flash_Programming*/
void himax_flash_programming(struct i2c_client *client,
	uint8_t *FW_content, int FW_Size);

/*himax_83100_CheckChipVersion*/
bool himax_check_chip_version(struct i2c_client *client);

/*himax_83100_Check_CRC*/
int himax_check_CRC(struct i2c_client *client, int mode);

bool Calculate_CRC_with_AP(unsigned char *FW_content,
	int CRC_from_FW, int mode);

int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client,
	unsigned char *fw, int len, bool change_iref);

int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client,
	unsigned char *fw, int len, bool change_iref);

int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client,
	unsigned char *fw, int len, bool change_iref);

int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client,
	unsigned char *fw, int len, bool change_iref);

void himax_touch_information(struct i2c_client *client);
void himax_read_FW_ver(struct i2c_client *client);
bool himax_ic_package_check(struct i2c_client *client);

void himax_read_event_stack(struct i2c_client *client,
	uint8_t *buf, uint8_t length);

int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max);
bool read_event_stack(struct i2c_client *client, uint8_t *buf_ts, int length);
bool post_read_event_stack(struct i2c_client *client);

/*return checksum value*/
bool diag_check_sum(uint8_t hx_touch_info_size, uint8_t *buf_ts);

void diag_parse_raw_data(int hx_touch_info_size, int RawDataLen,
	int mul_num, int self_num, uint8_t *buf_ts,
	uint8_t diag_cmd, int16_t *mutual_data, int16_t *self_data);

void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data);
extern struct himax_ts_data *private_ts;
extern struct himax_ic_data *ic_data;

int himax_load_CRC_bin_file(struct i2c_client *client);
