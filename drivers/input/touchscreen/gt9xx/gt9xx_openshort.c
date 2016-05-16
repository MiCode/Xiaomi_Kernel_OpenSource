/* drivers/input/touchscreen/gt9xx_shorttp.c
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.0
 * Author: meta@goodix.com
 * Accomplished Date:2012/10/20
 * Revision record:
 *
 */

#include "gt9xx_openshort.h"



#define GTP_SHORT_GND
#define GTP_VDD		 33


#define DEFAULT_TEST_ITEMS  (_MAX_TEST | _MIN_TEST | _KEY_MAX_TEST | _KEY_MIN_TEST /*| _UNIFORMITY_TEST*/)


#define MAX_LIMIT_VALUE_GROUP1 2842
#define MIN_LIMIT_VALUE_GROUP1 931
#define MAX_LIMIT_KEY_GROUP1 5000
#define MIN_LIMIT_KEY_GROUP1 500
#define UNIFORMITY_GROUP1 60

#define CTP_TEST_CFG_GROUP1 {\
0x00, 0x38, 0x04, 0x80, 0x07, 0x0A, 0x04, 0x00, 0x23, 0x88, \
0x32, 0x0F, 0x4B, 0x3C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x01, 0x00, 0x08, 0x16, 0x17, 0x17, 0x14, 0x0E, 0x0E, 0x0F, \
0x0B, 0x00, 0xBB, 0x32, 0x00, 0x00, 0x00, 0x01, 0x45, 0x19, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, \
0x00, 0x0A, 0x19, 0x54, 0x45, 0x02, 0x07, 0x00, 0x00, 0x04, \
0x80, 0x0B, 0x00, 0x6C, 0x0D, 0x00, 0x5F, 0x0F, 0x00, 0x4D, \
0x13, 0x00, 0x45, 0x16, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, \
0x85, 0x60, 0x35, 0xFF, 0xFF, 0x19, 0x00, 0x02, 0x01, 0x00, \
0x00, 0x64, 0x00, 0x00, 0xFF, 0x3F, 0x02, 0x00, 0x00, 0xD4, \
0x30, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x1C, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, 0x0E, \
0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00, 0xFF, 0xFF, 0xFF, \
0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, \
0x10, 0x27, 0x20, 0x4E, 0x00, 0x0F, 0x14, 0x03, 0x07, 0x00, \
0x00, 0x28, 0x00, 0x0B, 0x0C, 0x28, 0x00, 0x00, 0x03, 0x00, \
0x06, 0x0A, 0x00, 0x01, 0x00, 0x00, 0x01, 0x24, 0x00, 0x00, \
0x00, 0x6F, 0x80, 0x00, 0x01, 0x00, 0xAF, 0x50, 0x00, 0x1E, \
0xAC, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x01\
}


#define MAX_LIMIT_VALUE_GROUP2 3000
#define MIN_LIMIT_VALUE_GROUP2 1200
#define MAX_LIMIT_KEY_GROUP2 2700
#define MIN_LIMIT_KEY_GROUP2 1500
#define UNIFORMITY_GROUP2 70

#define CTP_TEST_CFG_GROUP2 {\
}

#define MAX_LIMIT_VALUE_GROUP3 2520
#define MIN_LIMIT_VALUE_GROUP3 960
#define MAX_LIMIT_KEY_GROUP3 2640
#define MIN_LIMIT_KEY_GROUP3 1000
#define UNIFORMITY_GROUP3 70

#define CTP_TEST_CFG_GROUP3 {\
	0x00, 0xD0, 0x02, 0x00, 0x05, 0x0A, 0x05, 0x01, 0x01, 0x08, \
	0x28, 0x05, 0x50, 0x32, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x2A, 0x0B, \
	0x11, 0x0F, 0xE2, 0x11, 0x00, 0x00, 0x01, 0x9A, 0x04, 0x2D, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x64, 0x32, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x50, 0x00, \
	0xF0, 0x4A, 0x3A, 0xFF, 0xFF, 0x27, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, \
	0x12, 0x14, 0x16, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0xFF, \
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	0xFF, 0xFF, 0x24, 0x16, 0x22, 0x18, 0x21, 0x1C, 0x20, 0x1D, \
	0x1E, 0x1F, 0x10, 0x06, 0x00, 0x0F, 0x13, 0x0C, 0x12, 0x0A, \
	0x02, 0x08, 0x04, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	0xFF, 0x0E, 0x33, 0x33, 0x33, 0x33, 0xFF, 0xFF, 0x00, 0x00, \
	0xE1, 0xF8, 0xFF, 0xFF, 0xE7, 0x01\
}


#define MAX_LIMIT_VALUE_GROUP4 1800
#define MIN_LIMIT_VALUE_GROUP4 1000
#define MAX_LIMIT_KEY_GROUP4 1500
#define MIN_LIMIT_KEY_GROUP4 800
#define UNIFORMITY_GROUP4 70

#define CTP_TEST_CFG_GROUP4 {\
	}

#define MAX_LIMIT_VALUE_GROUP5 1800
#define MIN_LIMIT_VALUE_GROUP5 1000
#define MAX_LIMIT_KEY_GROUP5 1500
#define MIN_LIMIT_KEY_GROUP5 800
#define UNIFORMITY_GROUP5 70

#define CTP_TEST_CFG_GROUP5 {\
	}

#define MAX_LIMIT_VALUE_GROUP6 1800
#define MIN_LIMIT_VALUE_GROUP6 1000
#define MAX_LIMIT_KEY_GROUP6 1500
#define MIN_LIMIT_KEY_GROUP6 800
#define UNIFORMITY_GROUP6 70

#define CTP_TEST_CFG_GROUP6 {\
	}
#define DSP_SHORT_BURN_CHK		  256
#define _SHORT_INFO_MAX			 50
#define _BEYOND_INFO_MAX			20
#define GTP_OPEN_SAMPLE_NUM		 16
#define GTP_TEST_INFO_MAX		   200



u16 max_limit_value = 1800;
u16 min_limit_value = 1000;
u16 max_limit_key = 1500;
u16 min_limit_key = 800;
u16 uniformity_lmt = 70;

extern s32 gtp_i2c_read(struct i2c_client *, u8 *, s32);
extern s32 gtp_i2c_write(struct i2c_client *, u8 *, s32);
extern s32 gup_i2c_read(struct i2c_client *, u8 *, s32);
extern s32 gup_i2c_write(struct i2c_client *, u8 *, s32);
extern void gtp_reset_guitar(struct i2c_client*, s32);
extern s32 gup_enter_update_mode(struct i2c_client *);
extern s32 gup_leave_update_mode(void);
extern s32 gtp_send_cfg(struct i2c_client *client);
extern s32 gtp_read_version(struct i2c_client *client, u16 *version);
extern void gtp_irq_disable(struct goodix_ts_data *ts);
extern void gtp_irq_enable(struct goodix_ts_data *ts);
extern s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len);

extern struct i2c_client *i2c_connect_client;

extern void gtp_esd_switch(struct i2c_client *, s32);

u8  gt9xx_drv_num = MAX_DRIVER_NUM;
u8  gt9xx_sen_num = MAX_SENSOR_NUM;
u16 gt9xx_pixel_cnt = MAX_DRIVER_NUM * MAX_SENSOR_NUM;
u16 gt9xx_sc_pxl_cnt = MAX_DRIVER_NUM * MAX_SENSOR_NUM;
struct gt9xx_short_info *short_sum;

u8 chip_type_gt9f = 0;
u8 have_key = 0;
u8 gt9xx_sc_drv_num;
u8 key_is_isolated;
u8 key_iso_pos[5];

struct kobject *goodix_debug_kobj;
static u8  rslt_buf_idx;
static s32 *test_rslt_buf;
static struct gt9xx_open_info *touchpad_sum;

#define _MIN_ERROR_NUM	  (GTP_OPEN_SAMPLE_NUM * 9 / 10)

static char *result_lines[GTP_TEST_INFO_MAX];
static char tmp_info_line[80];
static u16 RsltIndex;

#define GTP_PARENT_PROC_NAME "touchscreen"
#define GTP_OPENHSORT_PROC_NAME "ctp_openshort_test"
#define GTP_RAWDATA_PROC_NAME "ctp_rawdata"

u8 test_cfg_info_group1[] =  CTP_TEST_CFG_GROUP1;
u8 test_cfg_info_group2[] =  CTP_TEST_CFG_GROUP2;
u8 test_cfg_info_group3[] =  CTP_TEST_CFG_GROUP3;
u8 test_cfg_info_group4[] =  CTP_TEST_CFG_GROUP4;
u8 test_cfg_info_group5[] =  CTP_TEST_CFG_GROUP5;
u8 test_cfg_info_group6[] =  CTP_TEST_CFG_GROUP6;

static void append_info_line(void)
{
	if (strlen(tmp_info_line) != 0) {
		result_lines[RsltIndex] = (char *)kzalloc(strlen(tmp_info_line)+1, GFP_KERNEL);
		memcpy(result_lines[RsltIndex], tmp_info_line, strlen(tmp_info_line));
	}
	if (RsltIndex != (GTP_TEST_INFO_MAX-1))
		++RsltIndex;
	else {
		kfree(result_lines[RsltIndex]);
	}
}


#define SET_INFO_LINE_INFO(fmt, args...)	do { \
							memset(tmp_info_line, '\0', 80);\
							sprintf(tmp_info_line, "<Sysfs-INFO>"fmt"\n", ##args);\
							GTP_INFO(fmt, ##args);\
							append_info_line();\
						} while (0)

#define SET_INFO_LINE_ERR(fmt, args...)		do { \
							memset(tmp_info_line, '\0', 80);\
							sprintf(tmp_info_line, "<Sysfs-ERROR>"fmt"\n", ##args);\
							GTP_ERROR(fmt, ##args);\
							append_info_line(); \
						} while (0)


static s32 gtp_i2c_end_cmd(struct i2c_client *client)
{
	u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	s32 ret = 0;

	ret = gtp_i2c_write(client, end_cmd, 3);
	if (ret < 0)
		SET_INFO_LINE_INFO("I2C write end_cmd  error!");
	return ret;
}

static void gtp_open_test_init(struct i2c_client *client)
{
	u8 version = 0;
	u8 sensor_id = 0;


	u16 max_limit_value_info[] = {
		MAX_LIMIT_VALUE_GROUP1, MAX_LIMIT_VALUE_GROUP2, MAX_LIMIT_VALUE_GROUP3,
					MAX_LIMIT_VALUE_GROUP4, MAX_LIMIT_VALUE_GROUP5, MAX_LIMIT_VALUE_GROUP6};

	u16 min_limit_value_info[] = {
		MIN_LIMIT_VALUE_GROUP1, MIN_LIMIT_VALUE_GROUP2, MIN_LIMIT_VALUE_GROUP3,
					MIN_LIMIT_VALUE_GROUP4, MIN_LIMIT_VALUE_GROUP5, MIN_LIMIT_VALUE_GROUP6};

	u16 max_limit_key_info[] = {
		MAX_LIMIT_KEY_GROUP1, MAX_LIMIT_KEY_GROUP2, MAX_LIMIT_KEY_GROUP3,
					MAX_LIMIT_KEY_GROUP4, MAX_LIMIT_KEY_GROUP5, MAX_LIMIT_KEY_GROUP6};

	u16 min_limit_key_info[] = {
		MIN_LIMIT_KEY_GROUP1, MIN_LIMIT_KEY_GROUP2, MIN_LIMIT_KEY_GROUP3,
					MIN_LIMIT_KEY_GROUP4, MIN_LIMIT_KEY_GROUP5, MIN_LIMIT_KEY_GROUP6};


	u16 uniformity_lmt_info[] = {
		UNIFORMITY_GROUP1, UNIFORMITY_GROUP2, UNIFORMITY_GROUP3,
					UNIFORMITY_GROUP4, UNIFORMITY_GROUP5, UNIFORMITY_GROUP6};



	u8 *send_test_cfg_buf[] = {
		test_cfg_info_group1, test_cfg_info_group2, test_cfg_info_group3,
					test_cfg_info_group4, test_cfg_info_group5, test_cfg_info_group6};
	 u8 tset_cfg_info_len[] = {
		CFG_GROUP_LEN(test_cfg_info_group1),
					CFG_GROUP_LEN(test_cfg_info_group2),
					CFG_GROUP_LEN(test_cfg_info_group3),
					CFG_GROUP_LEN(test_cfg_info_group4),
					CFG_GROUP_LEN(test_cfg_info_group5),
					CFG_GROUP_LEN(test_cfg_info_group6)};

	u8 test_config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH] = {
		(u8)(GTP_REG_CONFIG_DATA >> 8), (u8)GTP_REG_CONFIG_DATA, 0};


	sensor_id = 0;

	if (tset_cfg_info_len[sensor_id] != 0) {
		max_limit_value = max_limit_value_info[sensor_id];
		min_limit_value = min_limit_value_info[sensor_id];
		max_limit_key = max_limit_key_info[sensor_id];
		min_limit_key = min_limit_key_info[sensor_id];
		uniformity_lmt = uniformity_lmt_info[sensor_id];

		memset(&test_config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
		memcpy(&test_config[GTP_ADDR_LENGTH], send_test_cfg_buf[sensor_id], tset_cfg_info_len[sensor_id]);

		SET_INFO_LINE_INFO("max_limit_value: %d\n", max_limit_value);
		SET_INFO_LINE_INFO("min_limit_value: %d\n", min_limit_value);
		SET_INFO_LINE_INFO("max_limit_key: %d\n", max_limit_key);
		SET_INFO_LINE_INFO("min_limit_key: %d\n", min_limit_key);
		SET_INFO_LINE_INFO("uniformity_lmt %d\n", uniformity_lmt);

		gtp_i2c_write(client, test_config, tset_cfg_info_len[sensor_id] + GTP_ADDR_LENGTH);

		gtp_i2c_read_dbl_check(client, GTP_REG_CONFIG_DATA, &version, 1);
		SET_INFO_LINE_INFO(" The version is  %x\n", version);
	}
}

s32 gtp_parse_config(struct i2c_client *client)
{
	u8 i = 0;
	u8 key_pos = 0;
	u8 key_val = 0;
	u8 config[256] = {(u8)(GTP_REG_CONFIG_DATA >> 8), (u8)GTP_REG_CONFIG_DATA, 0};
	u8 type_buf[12] = {0x80, 0x00};

	if (gtp_i2c_read(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH) <= 0) {
		SET_INFO_LINE_ERR("Failed to read config!");
		return FAIL;
	}

	gt9xx_drv_num = (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT-GT9_REG_CFG_BEG] & 0x1F)
					+ (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT+1-GT9_REG_CFG_BEG] & 0x1F);
	gt9xx_sen_num = (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT+2-GT9_REG_CFG_BEG] & 0x0F)
					+ ((config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT+2-GT9_REG_CFG_BEG]>>4) & 0x0F);

	GTP_INFO("Driver num: %d, Sensor Num: %d", gt9xx_drv_num, gt9xx_sen_num);
	if (gt9xx_drv_num < MIN_DRIVER_NUM || gt9xx_drv_num > MAX_DRIVER_NUM) {
		SET_INFO_LINE_ERR("driver number error!");
		return FAIL;
	}
	if (gt9xx_sen_num < MIN_SENSOR_NUM || gt9xx_sen_num > MAX_SENSOR_NUM) {
		SET_INFO_LINE_ERR("sensor number error!");
		return FAIL;
	}
	gt9xx_sc_pxl_cnt = gt9xx_pixel_cnt = gt9xx_drv_num * gt9xx_sen_num;

	gtp_i2c_read(client, type_buf, 12);
	if (!memcmp(&type_buf[2], "GOODIX_GT9", 10)) {
		chip_type_gt9f = 0;
		GTP_INFO("Chip type: GT9XX");
	} else {
		chip_type_gt9f = 1;
		GTP_INFO("Chip type: GT9XXF");
	}

	have_key = config[0x804E - GT9_REG_CFG_BEG + GTP_ADDR_LENGTH] & 0x01;

	if (!have_key) {
		GTP_INFO("No key");
		return SUCCESS;
	}
	GTP_INFO("Have Key");
	gt9xx_sc_drv_num = gt9xx_drv_num - 1;

	for (i = 0; i < 5; ++i) {
		key_iso_pos[i] = 0;
	}

	key_is_isolated = 0;
	for (i = 0; i < 4; ++i) {

		key_val = config[GTP_ADDR_LENGTH + GT9_REG_KEY_VAL - GT9_REG_CFG_BEG + i];
		key_pos = key_val%0x08;
		GTP_DEBUG("key_val[%d] = 0x%02x", i+1, key_val);
		if ((key_pos != 0)) {
			key_is_isolated = 0;
			GTP_DEBUG("Key is not isolated!");
			break;
		} else if (key_val == 0x00)
			continue;
		else {
			key_iso_pos[0]++;
			key_iso_pos[i+1] = key_val/0x08 - 1;
			key_is_isolated = 1;
		}
	}

	gt9xx_sc_pxl_cnt = gt9xx_pixel_cnt - (gt9xx_drv_num-gt9xx_sc_drv_num) * gt9xx_sen_num;
	GTP_DEBUG("drv num: %d, sen num: %d, sc drv num: %d", gt9xx_drv_num, gt9xx_sen_num, gt9xx_sc_drv_num);
	if (key_is_isolated) {
		GTP_DEBUG("Isolated [%d key(s)]: %d, %d, %d, %d", key_iso_pos[0], key_iso_pos[1], key_iso_pos[2], key_iso_pos[3], key_iso_pos[4]);
	}

	return SUCCESS;
}

/*
 * Function:
 *	  write one byte to specified register
 * Input:
 *	  reg: the register address
 *	  val: the value to write into
 * Return:
 *	  i2c_write function return
 */
s32 gtp_write_register(struct i2c_client *client, u16 reg, u8 val)
{
	u8 buf[3];
	buf[0] = (u8) (reg >> 8);
	buf[1] = (u8) reg;
	buf[2] = val;
	return gup_i2c_write(client, buf, 3);
}
/*
 * Function:
 *	  read one byte from specified register into buf
 * Input:
 *	  reg: the register
 *	  buf: the buffer for one byte
 * Return:
 *	  i2c_read function return
 */
s32 gtp_read_register(struct i2c_client *client, u16 reg, u8 *buf)
{
	buf[0] = (u8)(reg >> 8);
	buf[1] = (u8)reg;
	return gup_i2c_read(client, buf, 3);
}

u32 endian_mode(void)
{
	union {
		s32 i;
		s8 c;
	} endian;

	endian.i = 1;

	if (1 == endian.c)
		return MYBIG_ENDIAN;
	else
		return MYLITLE_ENDIAN;
}
/*
*********************************************************************************************************
* Function:
*   send read rawdata cmd
* Input:
*   i2c_client* client: i2c device
* Return:
*   SUCCESS: send process succeed, FAIL: failed
*********************************************************************************************************
*/
s32 gt9_read_raw_cmd(struct i2c_client *client)
{
	u8 raw_cmd[3] = {(u8)(GTP_REG_READ_RAW >> 8), (u8)GTP_REG_READ_RAW, 0x01};
	s32 ret = -1;
	GTP_DEBUG("Send read raw data command");
	ret = gtp_i2c_write(client, raw_cmd, 3);
	if (ret <= 0) {
		SET_INFO_LINE_ERR("i2c write failed.");
		return FAIL;
	}
	msleep(10);
	return SUCCESS;
}

s32 gt9_read_coor_cmd(struct i2c_client *client)
{
	u8 raw_cmd[3] = {(u8)(GTP_REG_READ_RAW >> 8), (u8)GTP_REG_READ_RAW, 0x0};
	s32 ret = -1;

	ret = gtp_i2c_write(client, raw_cmd, 3);
	if (ret < 0) {
		SET_INFO_LINE_ERR("i2c write coor cmd failed!");
		return FAIL;
	}
	msleep(10);
	return SUCCESS;
}
/*
*********************************************************************************************************
* Function:
*   read rawdata from ic registers
* Input:
*   u16* data: rawdata buffer
*   i2c_client* client: i2c device
* Return:
*   SUCCESS: read process succeed, FAIL:  failed
*********************************************************************************************************
*/
s32 gtp_read_rawdata(struct i2c_client *client, u16 *data)
{
	s32 ret = -1;
	u16 retry = 0;
	u8 read_state[3] = {(u8)(GTP_REG_RAW_READY>>8), (u8)GTP_REG_RAW_READY, 0};
	u16 i = 0, j = 0;
	u8 *read_rawbuf;
	u8 tail, head;

	read_rawbuf = (u8 *)kmalloc(sizeof(u8) * (gt9xx_drv_num*gt9xx_sen_num * 2 + GTP_ADDR_LENGTH), GFP_KERNEL);

	if (NULL == read_rawbuf) {
		SET_INFO_LINE_ERR("failed to allocate for read_rawbuf");
		return FAIL;
	}

	if (data == NULL) {
		SET_INFO_LINE_ERR("Invalid raw buffer.");
		goto have_error;
	}

	msleep(10);
	while (retry++ < GTP_WAIT_RAW_MAX_TIMES) {
		ret = gtp_i2c_read(client, read_state, 3);
		if (ret <= 0) {
			SET_INFO_LINE_ERR("i2c read failed.return: %d", ret);
			continue;
		}
		if ((read_state[GTP_ADDR_LENGTH] & 0x80) == 0x80) {
			GTP_DEBUG("Raw data is ready.");
			break;
		}
		if ((retry%20) == 0) {
			GTP_DEBUG("(%d)read_state[2] = 0x%02X", retry, read_state[GTP_ADDR_LENGTH]);
			if (retry == 100)
				gt9_read_raw_cmd(client);
		}
		msleep(5);
	}
	if (retry >= GTP_WAIT_RAW_MAX_TIMES) {
		SET_INFO_LINE_ERR("Wait raw data ready timeout.");
		goto have_error;
	}

	if (chip_type_gt9f) {
		read_rawbuf[0] = (u8)(GTP_REG_RAW_DATA_GT9F >> 8);
		read_rawbuf[1] = (u8)(GTP_REG_RAW_DATA_GT9F);
	} else {
		read_rawbuf[0] = (u8)(GTP_REG_RAW_DATA >> 8);
		read_rawbuf[1] = (u8)(GTP_REG_RAW_DATA);
	}

	ret = gtp_i2c_read(client, read_rawbuf, GTP_ADDR_LENGTH + ((gt9xx_drv_num*gt9xx_sen_num)*2));
	if (ret <= 0) {
		SET_INFO_LINE_ERR("i2c read rawdata failed.");
		goto have_error;
	}
	gtp_i2c_end_cmd(client);

	if (endian_mode() == MYBIG_ENDIAN) {
		head = 0;
		tail = 1;
		GTP_DEBUG("Big Endian.");
	} else {
		head = 1;
		tail = 0;
		GTP_DEBUG("Little Endian.");
	}

	for (i = 0, j = 0; i < ((gt9xx_drv_num*gt9xx_sen_num)*2); i += 2) {
		data[i/2] = (u16)(read_rawbuf[i+head+GTP_ADDR_LENGTH]<<8) + (u16)read_rawbuf[GTP_ADDR_LENGTH+i+tail];
	#if GTP_DEBUG_ARRAY_ON
		printk("%4d ", data[i/2]);
		++j;
		if ((j%10) == 0)
			printk("\n");
	#endif
	}

	kfree(read_rawbuf);
	return SUCCESS;
have_error:
	kfree(read_rawbuf);
	return FAIL;
}
/*
*********************************************************************************************************
* Function:
*   rawdata test initilization function
* Input:
*   u32 check_types: test items
*********************************************************************************************************
*/
static s32 gtp_raw_test_init(void)
{
	u16 i = 0;

	test_rslt_buf = (s32 *) kmalloc(sizeof(s32)*GTP_OPEN_SAMPLE_NUM, GFP_ATOMIC);
	touchpad_sum = (struct gt9xx_open_info *) kmalloc(sizeof(struct gt9xx_open_info) * (4 * _BEYOND_INFO_MAX + 1), GFP_ATOMIC);
	if (NULL == test_rslt_buf || NULL == touchpad_sum)
		return FAIL;
	memset(touchpad_sum, 0, sizeof(struct gt9xx_open_info) * (4 * _BEYOND_INFO_MAX + 1));

	for (i = 0; i < (4 * _BEYOND_INFO_MAX); ++i) {
		touchpad_sum[i].driver = 0xFF;
	}

	for (i = 0; i < GTP_OPEN_SAMPLE_NUM; i++) {
		test_rslt_buf[i] = _CHANNEL_PASS;
	}
	return SUCCESS;
}

/*
*********************************************************************************************************
* Function:
*   touchscreen rawdata min limit test
* Input:
*   u16* raw_buf: rawdata buffer
*********************************************************************************************************
*/
static void gtp_raw_min_test(u16 *raw_buf)
{
	u16 i, j = 0;
	u8 driver, sensor;
	u8 sum_base = 1 * _BEYOND_INFO_MAX;
	u8 new_flag = 0;

	for (i = 0; i < gt9xx_sc_pxl_cnt; i++) {
		if (raw_buf[i] < min_limit_value) {
			test_rslt_buf[rslt_buf_idx] |= _BEYOND_MIN_LIMIT;
			driver = (i/gt9xx_sen_num);
			sensor = (i%gt9xx_sen_num);
			new_flag = 0;
			for (j = sum_base; j < (sum_base+_BEYOND_INFO_MAX); ++j) {
				if (touchpad_sum[j].driver == 0xFF) {
					new_flag = 1;
					break;
				}
				if ((driver == touchpad_sum[j].driver) && (sensor == touchpad_sum[j].sensor)) {
					touchpad_sum[j].times++;
					new_flag = 0;
					break;
				}
			}
			if (new_flag) {
				touchpad_sum[j].driver = driver;
				touchpad_sum[j].sensor = sensor;
				touchpad_sum[j].beyond_type |= _BEYOND_MIN_LIMIT;
				touchpad_sum[j].raw_val = raw_buf[i];
				touchpad_sum[j].times = 1;
				GTP_DEBUG("[%d, %d]rawdata: %d, raw min limit: %d", driver, sensor, raw_buf[i], min_limit_value);
			} else
				continue;
		}
	}
}

/*
*********************************************************************************************************
* Function:
*   touchscreen rawdata max limit test
* Input:
*   u16* raw_buf: rawdata buffer
*********************************************************************************************************
*/
static void gtp_raw_max_test(u16 *raw_buf)
{
	u16 i, j;
	u8 driver, sensor;
	u8 sum_base = 0 * _BEYOND_INFO_MAX;
	u8 new_flag = 0;

	for (i = 0; i < gt9xx_sc_pxl_cnt; i++) {
		if (raw_buf[i] > max_limit_value) {
			test_rslt_buf[rslt_buf_idx] |= _BEYOND_MAX_LIMIT;
			driver = (i/gt9xx_sen_num);
			sensor = (i%gt9xx_sen_num);
			new_flag = 0;
			for (j = sum_base; j < (sum_base+_BEYOND_INFO_MAX); ++j) {
				if (touchpad_sum[j].driver == 0xFF) {
					new_flag = 1;
					break;
				}
				if ((driver == touchpad_sum[j].driver) && (sensor == touchpad_sum[j].sensor)) {
					touchpad_sum[j].times++;
					new_flag = 0;
					break;
				}
			}
			if (new_flag) {
				touchpad_sum[j].driver = driver;
				touchpad_sum[j].sensor = sensor;
				touchpad_sum[j].beyond_type |= _BEYOND_MAX_LIMIT;
				touchpad_sum[j].raw_val = raw_buf[i];
				touchpad_sum[j].times = 1;
				GTP_DEBUG("[%d, %d]rawdata: %d, raw max limit: %d", driver, sensor, raw_buf[i], max_limit_value);
			} else
				continue;
		}
	}
}

/*
*********************************************************************************************************
* Function:
*   key rawdata max limit test
* Input:
*   u16* raw_buf: rawdata buffer
*********************************************************************************************************
*/
static void gtp_key_max_test(u16 *raw_buf)
{
	u16 i = 0, j = 1, k = 0;
	u8 key_cnt = key_iso_pos[0];
	u8 driver, sensor;
	u8 sum_base = 2 * _BEYOND_INFO_MAX;
	u8 new_flag = 0;

	driver = gt9xx_drv_num-1;
	for (i = gt9xx_sc_pxl_cnt; i < gt9xx_pixel_cnt; ++i) {
		sensor = ((i)%gt9xx_sen_num);
		if (key_is_isolated) {
			if ((key_iso_pos[j] != sensor) || (key_cnt == 0)) {
				continue;
			} else {
				--key_cnt;
				++j;
			}
		}
		if (raw_buf[i] > max_limit_key) {
			test_rslt_buf[rslt_buf_idx] |= _BEYOND_KEY_MAX_LMT;
			new_flag = 0;
			for (k = sum_base; k < (sum_base+_BEYOND_INFO_MAX); ++k) {
				if (touchpad_sum[k].driver == 0xFF) {
					new_flag = 1;
					break;
				}
				if (touchpad_sum[k].sensor == sensor) {
					touchpad_sum[k].times++;
					new_flag = 0;
					break;
				}
			}
			if (new_flag) {
				touchpad_sum[k].driver = driver;
				touchpad_sum[k].sensor = sensor;
				touchpad_sum[k].beyond_type |= _BEYOND_KEY_MAX_LMT;
				touchpad_sum[k].raw_val = raw_buf[i];
				touchpad_sum[k].times = 1;
				if (key_is_isolated)
					touchpad_sum[k].key = j-1;
				GTP_DEBUG("[%d, %d]key rawdata: %d, key max limit: %d", driver, sensor, raw_buf[i], max_limit_key);
			} else
				continue;
		}
	}
}
/*
*********************************************************************************************************
* Function:
*   key rawdata min limit test
* Input:
*   u16* raw_buf: rawdata buffer
*********************************************************************************************************
*/
static void gtp_key_min_test(u16 *raw_buf)
{
	u16 i = 0, j = 1, k = 0;
	u8 key_cnt = key_iso_pos[0];
	u8 driver, sensor;
	u8 sum_base = 3 * _BEYOND_INFO_MAX;
	u8 new_flag = 0;

	driver = gt9xx_drv_num-1;
	for (i = gt9xx_sc_pxl_cnt; i < gt9xx_pixel_cnt; ++i) {
		sensor = (i%gt9xx_sen_num);
		if (key_is_isolated) {

			if ((key_iso_pos[j] != sensor) || (key_cnt == 0)) {
				continue;
			} else {
				--key_cnt;
				++j;
			}
		}

		if (raw_buf[i] < min_limit_key) {
			test_rslt_buf[rslt_buf_idx] |= _BEYOND_KEY_MIN_LMT;
			new_flag = 0;
			for (k = sum_base; k < (sum_base + _BEYOND_INFO_MAX); ++k) {
				if (touchpad_sum[k].driver == 0xFF) {
					new_flag = 1;
					break;
				}
				if (sensor == touchpad_sum[k].sensor) {
					touchpad_sum[k].times++;
					break;
				}
			}
			if (new_flag) {
				touchpad_sum[k].driver = driver;
				touchpad_sum[k].sensor = sensor;
				touchpad_sum[k].beyond_type |= _BEYOND_KEY_MIN_LMT;
				touchpad_sum[k].raw_val = raw_buf[i];
				touchpad_sum[k].times = 1;
				if (key_is_isolated)
					touchpad_sum[k].key = j-1;
				GTP_DEBUG("[%d, %d]key rawdata: %d, key min limit: %d", driver, sensor, raw_buf[i], min_limit_key);
			} else
				continue;
		}
	}
}

static void gtp_uniformity_test(u16 *raw_buf)
{
	u16 i = 0;
	u8 sum_base = 4 * _BEYOND_INFO_MAX;
	u16 min_val = 0, max_val = 0;
	u16 uniformity = 0;

	min_val = raw_buf[0];
	max_val = raw_buf[0];
	for (i = 1; i < gt9xx_sc_pxl_cnt; i++) {
		if (raw_buf[i] > max_val)
			max_val = raw_buf[i];
		if (raw_buf[i] < min_val)
			min_val = raw_buf[i];
	}

	if (0 == max_val)
		uniformity = 0;
	else
		uniformity = (min_val * 100) / max_val;
	 GTP_DEBUG("min_val: %d, max_val: %d, tp uniformity: %d%%", min_val, max_val, uniformity);
	 if (uniformity < uniformity_lmt) {
		test_rslt_buf[rslt_buf_idx] |= _BEYOND_UNIFORMITY_LMT;
		touchpad_sum[sum_base].beyond_type |= _BEYOND_UNIFORMITY_LMT;
		touchpad_sum[sum_base].times++;
		touchpad_sum[sum_base].raw_val += uniformity;
		GTP_INFO("min_val: %d, max_val: %d, uniformity: %d%%, times: %d", min_val, max_val, uniformity, touchpad_sum[sum_base].times);
	}
}


/*
*********************************************************************************************************
* Function:
*   analyse rawdata retrived from ic registers
* Input:
*   u16 *raw_buf, buffer for rawdata,
*   u32 check_types, test items
* Return:
*   SUCCESS: test process succeed, FAIL: failed
*********************************************************************************************************
*/
static u32 gtp_raw_test(u16 *raw_buf, u32 check_types)
{
	if (raw_buf == NULL) {
		GTP_DEBUG("Invalid raw buffer pointer!");
		return FAIL;
	}

	if (check_types & _MAX_TEST)
		gtp_raw_max_test(raw_buf);

	if (check_types & _MIN_TEST)
		gtp_raw_min_test(raw_buf);
	if (have_key) {
		if (check_types & _KEY_MAX_TEST)
			gtp_key_max_test(raw_buf);
		if (check_types & _KEY_MIN_TEST)
			gtp_key_min_test(raw_buf);
	}

	if (check_types & _UNIFORMITY_TEST)
		gtp_uniformity_test(raw_buf);
	return SUCCESS;
}


/*
====================================================================================================
* Function:
*   output the test result
* Return:
*   return the result. if result == 0, the TP is ok, otherwise list the beyonds
====================================================================================================
*/

static s32 gtp_get_test_result(void)
{
	u16 i = 0, j = 0;
	u16 beyond_max_num = 0;
	u16 beyond_min_num = 0;
	u16 beyond_key_max = 0;
	u16 beyond_key_min = 0;

	u16 beyond_uniformity = 0;

	s32 result = _CHANNEL_PASS;

#if GTP_DEBUG_ON
	for (i = 0; i < 4 * _BEYOND_INFO_MAX; ++i) {
		printk("(%2d, %2d)[%2d] ", touchpad_sum[i].driver, touchpad_sum[i].sensor, touchpad_sum[i].times);
		if (i && ((i+1) % 5 == 0))
			printk("\n");
	}
	printk("\n");
#endif

	for (i = 0; i < GTP_OPEN_SAMPLE_NUM; ++i) {
		if (test_rslt_buf[i] & _BEYOND_MAX_LIMIT)
			beyond_max_num++;
		if (test_rslt_buf[i] & _BEYOND_MIN_LIMIT)
			beyond_min_num++;

		if (have_key) {
			if (test_rslt_buf[i] & _BEYOND_KEY_MAX_LMT)
				beyond_key_max++;
			if (test_rslt_buf[i] & _BEYOND_KEY_MIN_LMT)
				beyond_key_min++;
		}

		if (test_rslt_buf[i] & _BEYOND_UNIFORMITY_LMT) {
			beyond_uniformity++;
		}
	}
	if (beyond_max_num > _MIN_ERROR_NUM) {
		result |= _BEYOND_MAX_LIMIT;
		j = 0;
		SET_INFO_LINE_INFO("Beyond Max Limit Points Info: ");
		for (i = 0; i < _BEYOND_INFO_MAX; ++i) {
			if (touchpad_sum[i].driver == 0xFF)
				break;
			SET_INFO_LINE_INFO("  Drv: %d, Sen: %d RawVal: %d", touchpad_sum[i].driver, touchpad_sum[i].sensor, touchpad_sum[i].raw_val);
		}
	}
	if (beyond_min_num > _MIN_ERROR_NUM) {
		result |= _BEYOND_MIN_LIMIT;
		SET_INFO_LINE_INFO("Beyond Min Limit Points Info:");
		j = 0;
		for (i = _BEYOND_INFO_MAX; i < (2*_BEYOND_INFO_MAX); ++i) {
			if (touchpad_sum[i].driver == 0xFF)
				break;
			SET_INFO_LINE_INFO("  Drv: %d, Sen: %d RawVal: %d", touchpad_sum[i].driver, touchpad_sum[i].sensor, touchpad_sum[i].raw_val);
		}
	}

	if (have_key) {
		if (beyond_key_max > _MIN_ERROR_NUM) {
			result |= _BEYOND_KEY_MAX_LMT;
			SET_INFO_LINE_INFO("Beyond Key Max Limit Key Info:");
			for (i = 2*_BEYOND_INFO_MAX; i < (3*_BEYOND_INFO_MAX); ++i) {
				if (touchpad_sum[i].driver == 0xFF)
					break;
				SET_INFO_LINE_INFO("  Drv: %d, Sen: %d RawVal: %d", touchpad_sum[i].driver, touchpad_sum[i].sensor, touchpad_sum[i].raw_val);
			}
		}
		if (beyond_key_min > _MIN_ERROR_NUM) {
			result |= _BEYOND_KEY_MIN_LMT;
			SET_INFO_LINE_INFO("Beyond Key Min Limit Key Info:");
			for (i = 3*_BEYOND_INFO_MAX; i < (4*_BEYOND_INFO_MAX); ++i) {
				if (touchpad_sum[i].driver == 0xFF)
					break;
				SET_INFO_LINE_INFO("  Drv: %d, Sen: %d RawVal: %d", touchpad_sum[i].driver, touchpad_sum[i].sensor, touchpad_sum[i].raw_val);
			}
		}
	}

	if (beyond_uniformity > _MIN_ERROR_NUM) {
		result |= _BEYOND_UNIFORMITY_LMT;
		SET_INFO_LINE_INFO("Beyond Uniformity Limit Info: ");
		SET_INFO_LINE_INFO("  Uniformity Limit: %d%%, Tp Uniformity: %d%%", uniformity_lmt, touchpad_sum[4*_BEYOND_INFO_MAX].raw_val / touchpad_sum[4 * _BEYOND_INFO_MAX].times);
	}

	if (result == 0) {
		SET_INFO_LINE_INFO("[TEST SUCCEED]: ");
		SET_INFO_LINE_INFO("\tThe TP is OK!");
		return result;
	}
	SET_INFO_LINE_INFO("[TEST FAILED]:");
	if (result & _BEYOND_MAX_LIMIT)
		SET_INFO_LINE_INFO("  Beyond Raw Max Limit [Max Limit: %d]", max_limit_value);
	if (result & _BEYOND_MIN_LIMIT)
		SET_INFO_LINE_INFO("  Beyond Raw Min Limit [Min Limit: %d]", min_limit_value);

	if (have_key) {
		if (result & _BEYOND_KEY_MAX_LMT)
			SET_INFO_LINE_INFO("  Beyond KeyVal Max Limit [Key Max Limit: %d]", max_limit_key);
		if (result & _BEYOND_KEY_MIN_LMT)
			SET_INFO_LINE_INFO("  Beyond KeyVal Min Limit [Key Min Limit: %d]", min_limit_key);
	}

	if (result & _BEYOND_UNIFORMITY_LMT) {
		SET_INFO_LINE_INFO("  Beyond Uniformity Limit [Uniformity Limit: %d%%]", uniformity_lmt);
	}
	return result;
}

/*
 ===================================================
 * Function:
 *	  test gt9 series ic open test
 * Input:
 *	  client, i2c_client
 * Return:
 *	  SUCCESS: test process success, FAIL, test process failed
 *
 ===================================================
*/
s32 gt9xx_open_test(struct i2c_client *client)
{
	u16 i = 0;

	s32 ret = FAIL;
	struct goodix_ts_data *ts;
	u16 *raw_buf = NULL;

	ts = i2c_get_clientdata(client);
	gtp_irq_disable(ts);

  #if GTP_ESD_PROTECT

	gtp_esd_switch(ts->client, SWITCH_OFF);
  #endif
	ts->gtp_rawdiff_mode = 1;

	SET_INFO_LINE_INFO("---gtp open test---");
	GTP_INFO("Parsing configuration...");


	gtp_open_test_init(client);

	ret = gtp_parse_config(client);
	if (ret == FAIL) {
		SET_INFO_LINE_ERR("failed to parse config...");
		ret = FAIL;
		goto open_test_exit;
	}
	raw_buf = (u16 *)kmalloc(sizeof(u16) * (gt9xx_drv_num*gt9xx_sen_num), GFP_KERNEL);
	if (NULL == raw_buf) {
		SET_INFO_LINE_ERR("failed to allocate mem for raw_buf!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_INFO("Step 1: Send Rawdata Cmd");

	ret = gtp_raw_test_init();
	if (FAIL == ret) {
		SET_INFO_LINE_ERR("Allocate memory for open test failed!");
		ret = FAIL;
		goto open_test_exit;
	}
	ret = gt9_read_raw_cmd(client);
	if (ret == FAIL) {
		SET_INFO_LINE_ERR("Send Read Rawdata Cmd failed!");
		ret = FAIL;
		goto open_test_exit;
	}
	GTP_INFO("Step 2: Sample Rawdata");
	for (i = 0; i < GTP_OPEN_SAMPLE_NUM; ++i) {
		rslt_buf_idx = i;
		ret = gtp_read_rawdata(client, raw_buf);
		if (ret == FAIL) {
			SET_INFO_LINE_ERR("Read Rawdata failed!");
			ret = FAIL;
			goto open_test_exit;
		}
		ret = gtp_raw_test(raw_buf, DEFAULT_TEST_ITEMS);
		if (ret == FAIL) {
			gtp_i2c_end_cmd(client);
			continue;
		}
	}

	GTP_INFO("Step 3: Analyse Result");
	SET_INFO_LINE_INFO("Total %d Sample Data, Max Show %d Info for each Test Item", GTP_OPEN_SAMPLE_NUM, _BEYOND_INFO_MAX);
	if (0 == gtp_get_test_result())
		ret = SUCCESS;
	else
		ret = FAIL;

open_test_exit:
	if (raw_buf)
		kfree(raw_buf);
	if (test_rslt_buf) {
		kfree(test_rslt_buf);
		test_rslt_buf = NULL;
	}
	if (touchpad_sum) {
		kfree(touchpad_sum);
		touchpad_sum = NULL;
	}
	gtp_irq_enable(ts);

	ts->gtp_rawdiff_mode = 0;
	gt9_read_coor_cmd(client);
	SET_INFO_LINE_INFO("---gtp open test end---");

	gtp_send_cfg(client);
	msleep(300);

  #if GTP_ESD_PROTECT

	gtp_esd_switch(ts->client, SWITCH_ON);
  #endif
  return ret;
}

static s32 rawdata_cnt = 1;

static ssize_t gtp_open_test_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	char *ptr = page;
	s32 result = 3;
	s32 index;
	ssize_t len;

	if (*ppos)
		return 0;

	result = gt9xx_open_test(i2c_connect_client);

	for (index = 0, len = 0; index < RsltIndex; ++index) {
		kfree(result_lines[index]);
	}
	RsltIndex = 0;

if (result != 1)
	result = 0;


	len = sprintf(ptr, "result=%d\n", result);
	*ppos += len;
	return len;
}

#define TX_NUM_MAX 30
#define RX_NUM_MAX 30
short gtp_iTxNum = 11;
short gtp_iRxNum = 21;

s32 Gtp_RawData[TX_NUM_MAX][RX_NUM_MAX];

void swap_single_to_double(u16 *buf, int  RawData[TX_NUM_MAX][RX_NUM_MAX])
{
	int i = 0, j = 0;
	for (i = 0; i < gtp_iRxNum; i++) {
		for (j = 0; j < gtp_iTxNum; j++) {
			RawData[j][i] = (int)buf[i*gtp_iTxNum+j];
		}
	}
}
static ssize_t gtp_rawdata_read_proc(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t num_read_chars = 0;
	struct i2c_client *client = i2c_connect_client;
	s32 ret = -1;
	u16 *rawdata_buf;
	s32 i = 0;

	struct goodix_ts_data *ts;
	if (*ppos)
		return 0;
	ts = i2c_get_clientdata(i2c_connect_client);

	ts->gtp_rawdiff_mode = 1;
	gtp_irq_disable(ts);

		gtp_open_test_init(client);
	printk("Parsing configuration!\n");
	ret = gtp_parse_config(client);
	if (ret == FAIL) {
		sprintf(buf, "%s", "Parse config failed!\n");
	}
	rawdata_buf = (u16 *) kmalloc(sizeof(u16) * gt9xx_pixel_cnt, GFP_ATOMIC);
	if (NULL == rawdata_buf) {
		printk("failed to allocate memmory for rawdata buffer!");
		return 0;
	}
	gt9_read_raw_cmd(i2c_connect_client);
	for (i = 0; i < rawdata_cnt; ++i) {
		ret = gtp_read_rawdata(client, rawdata_buf);
	swap_single_to_double(rawdata_buf, Gtp_RawData);

	memcpy(buf, Gtp_RawData, TX_NUM_MAX*RX_NUM_MAX*sizeof(s32));
	}
	GTP_DEBUG("Rawdata buffer len: %zu", num_read_chars);

	ts->gtp_rawdiff_mode = 0;
	gtp_irq_enable(ts);
	gt9_read_coor_cmd(i2c_connect_client);

	msleep(50);
	gtp_reset_guitar(client, 20);
	msleep(100);
	gtp_send_cfg(client);
	*ppos += count;
	return count;

}

static ssize_t gtp_rawdata_write_proc(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}

static ssize_t gtp_open_test_write_proc(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos)
{
	return -EPERM;
}


static const struct file_operations gtp_open_test_proc_ops = {
	.owner = THIS_MODULE,
	.read = gtp_open_test_read_proc,
	.write = gtp_open_test_write_proc,
	.open = simple_open,
};
static const struct file_operations gtp_rawdata_procs_fops = {
	.owner = THIS_MODULE,
	.read = gtp_rawdata_read_proc,
	.write = gtp_rawdata_write_proc,
	.open = simple_open,
};

void gtp_create_ctp_proc(void)
{
	struct proc_dir_entry *gtp_device_proc = NULL;
	struct proc_dir_entry *gtp_openshort_proc = NULL;
	struct proc_dir_entry *gtp_rawdata_proc = NULL;

	gtp_device_proc = proc_mkdir(GTP_PARENT_PROC_NAME, NULL);
	if (gtp_device_proc == NULL) {
		printk("gt9xx: create parent_proc fail\n");
		return;
	}

	gtp_openshort_proc = proc_create(GTP_OPENHSORT_PROC_NAME, 0666, gtp_device_proc, &gtp_open_test_proc_ops);
	if (gtp_openshort_proc == NULL) {
		printk("gt9xx: create openshort_proc fail\n");
	}
	gtp_rawdata_proc = proc_create(GTP_RAWDATA_PROC_NAME, 0777, gtp_device_proc, &gtp_rawdata_procs_fops);
	if (gtp_rawdata_proc == NULL) {
		printk("gt9xx: create ctp_rawdata_proc fail\n");
	}
}

