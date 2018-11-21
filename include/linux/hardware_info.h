/*
 * Huaqin  Inc. (C) 2011. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HARDWARE_INFO_H__
#define __HARDWARE_INFO_H__

#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#define qcom_emmc "/sys/class/mmc_host/mmc0/mmc0:0001/block/mmcblk0/size"
#define qcom_emmc_len  16

typedef struct
{
	char  *id;
	char  *name;
} EMMC_VENDOR_TABLE;

typedef struct
{
	int  stage_value;
	char  *pcba_stage_name;
} BOARD_STAGE_TABLE;

typedef struct
{
	int  adc_value;
	char  *pcba_type_name;
} BOARD_TYPE_TABLE;

typedef struct
{
	unsigned int  adc_vol;
	char  *revision;
} BOARD_VERSION_TABLE;

typedef struct {
	int stage_value;
	int adc_value;
} SMEM_BOARD_INFO_DATA;

enum hardware_id {
	HWID_NONE = 0x00,
	HWID_DDR = 0x10,
	HWID_EMMC,
	HWID_NAND,
	HWID_FLASH,

	HWID_LCM = 0x20,
	HWID_LCD_BIAS,
	HWID_BACKLIGHT,
	HWID_CTP_DRIVER,
	HWID_CTP_MODULE,
	HWID_CTP_FW_VER,
	HWID_CTP_FW_INFO,
	HWID_CTP_COLOR_INFO,
	HWID_CTP_LOCKDOWN_INFO,

	HWID_MAIN_CAM = 0x30,
	HWID_MAIN_CAM_2,
	HWID_SUB_CAM,
	HWID_SUB_CAM_2,
	HWID_MAIN_LENS,
	HWID_MAIN_LENS_2,
	HWID_SUB_LENS,
	HWID_SUB_LENS_2,
	HWID_MAIN_OTP,
	HWID_MAIN_OTP_2,
	HWID_SUB_OTP,
	HWID_SUB_OTP_2,
	HWID_FLASHLIGHT,
	HWID_FLaSHLIGHT_2,

	HWID_GSENSOR = 0x70,
	HWID_ALSPS,
	HWID_GYROSCOPE,
	HWID_MSENSOR,
	HWID_FINGERPRINT,
	HWID_SAR_SENSOR_1,
	HWID_SAR_SENSOR_2,
	HWID_IRDA,
	HWID_BAROMETER,
	HWID_PEDOMETER,
	HWID_HUMIDITY,
	HWID_NFC,
	HWID_TEE,

	HWID_BATERY_ID = 0xA0,
	HWID_CHARGER,

	HWID_USB_TYPE_C = 0xE0,

	HWID_SUMMARY = 0xF0,
	HWID_VER,
	HWID_END
};


struct global_otp_struct {
	int otp_valid;
	int vendor_id;
	int module_code;
	int module_ver;
	int sw_ver;
	int year;
	int month;
	int day;
	int vcm_vendorid;
	int vcm_moduleid;
};

typedef struct {
	const char *version;
	const char *lcm;
	const char *ctp_driver;
	const char *ctp_module;
	unsigned char ctp_fw_version[20];
	const char *ctp_fw_info;
	const char *ctp_color_info;
	const char *ctp_lockdown_info;
	const char *main_camera;
	const char *sub_camera;
	const char *alsps;
	const char *gsensor;
	const char *gyroscope;
	const char *msensor;
	const char *fingerprint;
	const char *sar_sensor_1;
	const char *sar_sensor_2;
	const char *bat_id;
	const unsigned int *flash;
	const char *nfc;


} HARDWARE_INFO;

void get_hardware_info_data(enum hardware_id id, const void *data);
char *get_type_name(void);

#endif
