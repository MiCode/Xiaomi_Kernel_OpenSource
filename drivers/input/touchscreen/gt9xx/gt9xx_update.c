/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2010 - 2014 Goodix. Ltd.
 * Copyright (C) 2017 XiaoMi, Inc.
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
 * Version: 2.4
 * Release Date: 2014/11/28
 */
#include <linux/kthread.h>
#include "gt9xx.h"

#include <linux/namei.h>
#include <linux/mount.h>
#if ((GTP_AUTO_UPDATE && GTP_HEADER_FW_UPDATE) || GTP_COMPATIBLE_MODE)
	#include "gt9xx_firmware.h"
#endif

#define GUP_REG_HW_INFO             0x4220
#define GUP_REG_FW_MSG              0x41E4
#define GUP_REG_PID_VID             0x8140

#define GUP_SEARCH_FILE_TIMES       50

#define UPDATE_FILE_PATH_1          "/data/_goodix_update_.bin"
#define UPDATE_FILE_PATH_2          "/sdcard/_goodix_update_.bin"

#define CONFIG_FILE_PATH_1          "/data/_goodix_config_.cfg"
#define CONFIG_FILE_PATH_2          "/sdcard/_goodix_config_.cfg"

#define FW_HEAD_LENGTH               14
#define FW_SECTION_LENGTH            0x2000
#define FW_DSP_ISP_LENGTH            0x1000
#define FW_DSP_LENGTH                0x1000
#define FW_BOOT_LENGTH               0x800
#define FW_SS51_LENGTH               (4 * FW_SECTION_LENGTH)
#define FW_BOOT_ISP_LENGTH           0x800
#define FW_GLINK_LENGTH              0x3000
#define FW_GWAKE_LENGTH              (4 * FW_SECTION_LENGTH)

#define PACK_SIZE                    256
#define MAX_FRAME_CHECK_TIME         5

#define _bRW_MISCTL__SRAM_BANK       0x4048
#define _bRW_MISCTL__MEM_CD_EN       0x4049
#define _bRW_MISCTL__CACHE_EN        0x404B
#define _bRW_MISCTL__TMR0_EN         0x40B0
#define _rRW_MISCTL__SWRST_B0_       0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE 0x4184
#define _rRW_MISCTL__BOOTCTL_B0_     0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_    0x4218
#define _rRW_MISCTL__BOOT_CTL_       0x5094

#define AUTO_SEARCH_BIN           0x01
#define AUTO_SEARCH_CFG           0x02
#define BIN_FILE_READY            0x80
#define CFG_FILE_READY            0x08
#define HEADER_FW_READY           0x00

#pragma pack(1)
typedef struct {
	u8  hw_info[4];
	u8  pid[8];
	u16 vid;
} st_fw_head;
#pragma pack()

typedef struct {
	u8 force_update;
	u8 fw_flag;
	struct file *file;
	struct file *cfg_file;
	st_fw_head  ic_fw_msg;
	mm_segment_t old_fs;
	u32 fw_total_len;
	u32 fw_burned_len;
} st_update_msg;

st_update_msg update_msg;
u16 show_len;
u16 total_len;
u8 got_file_flag = 0;
u8 searching_file = 0;

extern u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH];
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern s32  gtp_send_cfg(struct i2c_client *client);
extern s32 gtp_read_version(struct i2c_client *, u16*);
extern struct i2c_client *i2c_connect_client;
extern void gtp_irq_enable(struct goodix_ts_data *ts);
extern void gtp_irq_disable(struct goodix_ts_data *ts);
extern s32 gtp_i2c_read_dbl_check(struct i2c_client *, u16, u8 *, int);
static u8 gup_burn_fw_gwake_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u32 len, u8 bank_cmd);
s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len);

#define _CLOSE_FILE(p_file) if (p_file && !IS_ERR(p_file)) \
					{ \
						filp_close(p_file, NULL); \
					}

#if GTP_ESD_PROTECT
extern void gtp_esd_switch(struct i2c_client *, s32);
#endif

#if GTP_COMPATIBLE_MODE
s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
#endif
/*******************************************************
Function:
	Read data from the i2c slave device.
Input:
	client:     i2c device.
	buf[0~1]:   read start address.
	buf[2~len-1]:   read data buffer.
	len:    GTP_ADDR_LENGTH + read bytes count
Output:
	numbers of i2c_msgs to transfer:
	  2: succeed, otherwise: failed
*********************************************************/
s32 gup_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];


	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}

	return ret;
}

/*******************************************************
Function:
    Write data to the i2c slave device.
Input:
    client:     i2c device.
    buf[0~1]:   write start address.
    buf[2~len-1]:   data buffer
    len:    GTP_ADDR_LENGTH + write bytes count
Output:
    numbers of i2c_msgs to transfer:
	1: succeed, otherwise: failed
*********************************************************/
s32 gup_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}

	return ret;
}

static s32 gup_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = 0;
	s32 i = 0;
	u8 check_sum = 0;
	u8 opr_buf[16];
	u8 sensor_id = 0;
	u16 version = 0;
	u8 drv_cfg_version;
	u8 flash_cfg_version;

#ifndef GTP_CONFIG_OF
	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;
	u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1,
		 cfg_info_group2, cfg_info_group3,
		 cfg_info_group4, cfg_info_group5};
	u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group0),
						  CFG_GROUP_LEN(cfg_info_group1),
						  CFG_GROUP_LEN(cfg_info_group2),
						  CFG_GROUP_LEN(cfg_info_group3),
						  CFG_GROUP_LEN(cfg_info_group4),
						  CFG_GROUP_LEN(cfg_info_group5)};
 #endif

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_SENSOR_ID, &sensor_id, 1);
	if (SUCCESS == ret) {
		if (sensor_id >= 0x06) {
			GTP_ERROR("Invalid sensor_id(0x%02X), No Config Sent!", sensor_id);
			return -EPERM;
		}
	} else {
		GTP_ERROR("Failed to get sensor_id, No config sent!");
		return -EPERM;
	}

		/* parse config data*/
#ifdef GTP_CONFIG_OF
	GTP_DEBUG("Get config data from dts file.");
	ret = gtp_parse_dt_cfg(&ts->client->dev, &config[GTP_ADDR_LENGTH], &ts->gtp_cfg_len, sensor_id);
	if (ret < 0) {
		GTP_ERROR("Failed to parse config data form dts file.");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
#else
	GTP_DEBUG("Get config data from header file.");
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
		(!cfg_info_len[3]) && (!cfg_info_len[4]) &&
		(!cfg_info_len[5])) {
		sensor_id = 0;
	}
	ts->gtp_cfg_len = cfg_info_len[sensor_id];
	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], ts->gtp_cfg_len);
#endif

	GTP_DEBUG("Sensor_ID: %d", sensor_id);

	if (ts->gtp_cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR("Sensor_ID(%d) matches with NULL or INVALID CONFIG GROUP! NO Config Sent! You need to check you header file CFG_GROUP section!", sensor_id);
		return -EPERM;
	}

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA, &opr_buf[0], 1);
	if (ret == SUCCESS) {
		GTP_DEBUG("CFG_GROUP%d Config Version: %d, IC Config Version: %d", sensor_id,
					config[GTP_ADDR_LENGTH], opr_buf[0]);

		flash_cfg_version = opr_buf[0];
		drv_cfg_version = config[GTP_ADDR_LENGTH];

		if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
			config[GTP_ADDR_LENGTH] = 0x00;
		}
	} else {
		GTP_ERROR("Failed to get ic config version!No config sent!");
		return -EPERM;
	}

	GTP_DEBUG("X_MAX = %d, Y_MAX = %d, TRIGGER = 0x%02x",
		ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);

	config[RESOLUTION_LOC]     = (u8)GTP_MAX_WIDTH;
	config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
	config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);

	if (GTP_INT_TRIGGER == 0) {
		config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {
		config[TRIGGER_LOC] |= 0x01;
	}

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
		check_sum += config[i];
	}
	config[ts->gtp_cfg_len] = (~check_sum) + 1;

	GTP_DEBUG_FUNC();
	ret = gtp_send_cfg(ts->client);
	if (ret < 0) {
		GTP_ERROR("Send config error.");
	}

	if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
		check_sum = 0;
		config[GTP_ADDR_LENGTH] = drv_cfg_version;
		for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
			check_sum += config[i];
		}
		config[ts->gtp_cfg_len] = (~check_sum) + 1;
	}
	gtp_read_version(ts->client, &version);
	msleep(10);
	return 0;
}


static u8 gup_get_ic_msg(struct i2c_client  *client, u16 addr, u8 *msg, s32 len)
{
	s32 i = 0;

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;

	for (i = 0; i < 5; i++) {
		if (gup_i2c_read(client, msg, GTP_ADDR_LENGTH + len) > 0) {
			break;
		}
	}

	if (i >= 5) {
		GTP_ERROR("Read data from 0x%02x%02x failed!", msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_set_ic_msg(struct i2c_client *client, u16 addr, u8 val)
{
	s32 i = 0;
	u8 msg[3];

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;
	msg[2] = val;

	for (i = 0; i < 5; i++) {
		if (gup_i2c_write(client, msg, GTP_ADDR_LENGTH + 1) > 0) {
			break;
		}
	}

	if (i >= 5) {
		GTP_ERROR("Set data to 0x%02x%02x failed!", msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_get_ic_fw_msg(struct i2c_client *client)
{
	s32 ret = -1;
	u8  retry = 0;
	u8  buf[16];
	u8  i;


	ret = gtp_i2c_read_dbl_check(client, GUP_REG_HW_INFO, &buf[GTP_ADDR_LENGTH], 4);
	if (FAIL == ret) {
		GTP_ERROR("[get_ic_fw_msg]get hw_info failed,exit");
		return FAIL;
	}



	for (i = 0; i < 4; i++) {
		update_msg.ic_fw_msg.hw_info[i] = buf[GTP_ADDR_LENGTH + 3 - i];
	}
	GTP_DEBUG("IC Hardware info:%02x%02x%02x%02x", update_msg.ic_fw_msg.hw_info[0], update_msg.ic_fw_msg.hw_info[1],
						update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);

	for (retry = 0; retry < 2; retry++) {
		ret = gup_get_ic_msg(client, GUP_REG_FW_MSG, buf, 1);
		if (FAIL == ret) {
			GTP_ERROR("Read firmware message fail.");
			return ret;
		}

		update_msg.force_update = buf[GTP_ADDR_LENGTH];
		if ((0xBE != update_msg.force_update) && (!retry)) {
			GTP_INFO("The check sum in ic is error.");
			GTP_INFO("The IC will be updated by force.");
			continue;
		}
		break;
	}
	GTP_DEBUG("IC force update flag:0x%x", update_msg.force_update);


	ret = gtp_i2c_read_dbl_check(client, GUP_REG_PID_VID, &buf[GTP_ADDR_LENGTH], 6);
	if (FAIL == ret) {
		GTP_ERROR("[get_ic_fw_msg]get pid & vid failed,exit");
		return FAIL;
	}

	memset(update_msg.ic_fw_msg.pid, 0, sizeof(update_msg.ic_fw_msg.pid));
	memcpy(update_msg.ic_fw_msg.pid, &buf[GTP_ADDR_LENGTH], 4);
	GTP_DEBUG("IC Product id:%s", update_msg.ic_fw_msg.pid);


	/*|-----FLASH-----RAM-----|
	|------918------918-----|
	|------968------968-----|
	|------913------913-----|
	|------913P-----913P----|
	|------927------927-----|
	|------927P-----927P----|
	|------9110-----9110----|
	|------9110P----9111----|*/
	if (update_msg.ic_fw_msg.pid[0] != 0) {
		if (!memcmp(update_msg.ic_fw_msg.pid, "9111", 4)) {
			GTP_DEBUG("IC Mapping Product id:%s",update_msg.ic_fw_msg.pid);
			memcpy(update_msg.ic_fw_msg.pid, "9110P", 5);
		}
	}

	update_msg.ic_fw_msg.vid = buf[GTP_ADDR_LENGTH+4] +(buf[GTP_ADDR_LENGTH+5]<<8);
	GTP_DEBUG("IC version id:%04x", update_msg.ic_fw_msg.vid);

	return SUCCESS;
}

s32 gup_enter_update_mode(struct i2c_client *client)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 rd_buf[3];


	GTP_GPIO_OUTPUT(gtp_rst_gpio, 0);
	msleep(2);


	GTP_GPIO_OUTPUT(gtp_int_gpio, (client->addr == 0x14));
	msleep(2);


	GTP_GPIO_OUTPUT(gtp_rst_gpio, 1);


	msleep(5);
	while (retry++ < 200) {

		ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}

		ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		if (0x0C == rd_buf[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("Hold ss51 & dsp confirm SUCCESS");
			break;
		}
		GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d", rd_buf[GTP_ADDR_LENGTH]);
	}
	if (retry >= 200) {
		GTP_ERROR("Enter update Hold ss51 failed.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, 0x4010, 0x00);

	return ret;
}

void gup_leave_update_mode(void)
{
	GTP_GPIO_AS_INT(gtp_int_gpio);

	GTP_DEBUG("[leave_update_mode]reset chip.");
	gtp_reset_guitar(i2c_connect_client, 20);
}



/* Update Conditions:
	1. Same hardware info
	2. Same PID
	3. File VID > IC VID
   Force Update Conditions:
	1. Wrong ic firmware checksum
	2. INVALID IC PID or VID
	3. (IC PID == 91XX || File PID == 91XX) && (File VID > IC VID)
*/

static u8 gup_enter_update_judge(st_fw_head *fw_head)
{
	u16 u16_tmp;
	s32 i = 0;
	u32 fw_len = 0;
	s32 pid_cmp_len = 0;
	u16_tmp = fw_head->vid;
	fw_head->vid = (u16)(u16_tmp>>8) + (u16)(u16_tmp<<8);

	GTP_INFO("FILE HARDWARE INFO:%02x%02x%02x%02x", fw_head->hw_info[0], fw_head->hw_info[1], fw_head->hw_info[2], fw_head->hw_info[3]);
	GTP_INFO("FILE PID:%s", fw_head->pid);
	GTP_INFO("FILE VID:%04x", fw_head->vid);
	GTP_INFO("IC HARDWARE INFO:%02x%02x%02x%02x", update_msg.ic_fw_msg.hw_info[0], update_msg.ic_fw_msg.hw_info[1],
			 update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);
	GTP_INFO("IC PID:%s", update_msg.ic_fw_msg.pid);
	GTP_INFO("IC VID:%04x", update_msg.ic_fw_msg.vid);

	if (!memcmp(fw_head->pid, "9158", 4) && !memcmp(update_msg.ic_fw_msg.pid, "915S", 4)) {
		GTP_INFO("Update GT915S to GT9158 directly!");
		return SUCCESS;
	}

	if (!memcmp(fw_head->hw_info, update_msg.ic_fw_msg.hw_info, sizeof(update_msg.ic_fw_msg.hw_info))) {
		fw_len = 42 * 1024;
	} else {
		fw_len = fw_head->hw_info[3];
		fw_len += (((u32)fw_head->hw_info[2]) << 8);
		fw_len += (((u32)fw_head->hw_info[1]) << 16);
		fw_len += (((u32)fw_head->hw_info[0]) << 24);
	}
	if (update_msg.fw_total_len != fw_len) {
		GTP_ERROR("Inconsistent firmware size, Update aborted! Default size: %d(%dK), actual size: %d(%dK)", fw_len, fw_len/1024, update_msg.fw_total_len, update_msg.fw_total_len/1024);
		return FAIL;
	}
	GTP_INFO("Firmware length:%d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);

	if (update_msg.force_update != 0xBE) {
		GTP_INFO("FW chksum error,need enter update.");
		return SUCCESS;
	}


	if (strlen(update_msg.ic_fw_msg.pid) < 3) {
		GTP_INFO("Illegal IC pid, need enter update");
		return SUCCESS;
	} else {
		for (i = 0; i < 3; i++) {
			if ((update_msg.ic_fw_msg.pid[i] < 0x30) || (update_msg.ic_fw_msg.pid[i] > 0x39)) {
				GTP_INFO("Illegal IC pid, out of bound, need enter update");
				return SUCCESS;
			}
		}
	}


	pid_cmp_len = strlen(fw_head->pid);
	if (pid_cmp_len < strlen(update_msg.ic_fw_msg.pid)) {
		pid_cmp_len = strlen(update_msg.ic_fw_msg.pid);
	}

	if ((!memcmp(fw_head->pid, update_msg.ic_fw_msg.pid, pid_cmp_len)) ||
			(!memcmp(update_msg.ic_fw_msg.pid, "91XX", 4)) ||
			(!memcmp(fw_head->pid, "91XX", 4))) {
		if (!memcmp(fw_head->pid, "91XX", 4)) {
			GTP_DEBUG("Force none same pid update mode.");
		} else {
			GTP_DEBUG("Get the same pid.");
		}


		if (fw_head->vid > update_msg.ic_fw_msg.vid) {
			GTP_INFO("Need enter update.");
			return SUCCESS;
		}
		GTP_ERROR("Don't meet the third condition.");
		GTP_ERROR("File VID <= Ic VID, update aborted!");
	} else {
		GTP_ERROR("File PID != Ic PID, update aborted!");
	}

	return FAIL;
}



#if GTP_AUTO_UPDATE_CFG
static u8 ascii2hex(u8 a)
{
	s8 value = 0;

	if (a >= '0' && a <= '9') {
		value = a - '0';
	} else if (a >= 'A' && a <= 'F') {
		value = a - 'A' + 0x0A;
	} else if (a >= 'a' && a <= 'f') {
		value = a - 'a' + 0x0A;
	} else {
		value = 0xff;
	}

	return value;
}

static s8 gup_update_config(struct i2c_client *client)
{
	s32 file_len = 0;
	s32 ret = 0;
	s32 i = 0;
	s32 file_cfg_len = 0;
	s32 chip_cfg_len = 0;
	s32 count = 0;
	u8 *buf;
	u8 *pre_buf;
	u8 *file_config;

	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (NULL == update_msg.cfg_file) {
		GTP_ERROR("[update_cfg]No need to upgrade config!");
		return FAIL;
	}
	file_len = update_msg.cfg_file->f_op->llseek(update_msg.cfg_file, 0, SEEK_END);

	chip_cfg_len = ts->gtp_cfg_len;

	GTP_DEBUG("[update_cfg]config file len:%d", file_len);
	GTP_DEBUG("[update_cfg]need config len:%d", chip_cfg_len);
	if ((file_len+5) < chip_cfg_len*5) {
		GTP_ERROR("Config length error");
		return -EPERM;
	}

	buf = (u8 *)kzalloc(file_len, GFP_KERNEL);
	pre_buf = (u8 *)kzalloc(file_len, GFP_KERNEL);
	file_config = (u8 *)kzalloc(chip_cfg_len + GTP_ADDR_LENGTH, GFP_KERNEL);
	update_msg.cfg_file->f_op->llseek(update_msg.cfg_file, 0, SEEK_SET);

	GTP_DEBUG("[update_cfg]Read config from file.");
	ret = update_msg.cfg_file->f_op->read(update_msg.cfg_file, (char *)pre_buf, file_len, &update_msg.cfg_file->f_pos);
	if (ret < 0) {
		GTP_ERROR("[update_cfg]Read config file failed.");
		goto update_cfg_file_failed;
	}

	GTP_DEBUG("[update_cfg]Delete illgal charactor.");
	for (i = 0, count = 0; i < file_len; i++) {
		if (pre_buf[i] == ' ' || pre_buf[i] == '\r' || pre_buf[i] == '\n') {
			continue;
		}
		buf[count++] = pre_buf[i];
	}

	GTP_DEBUG("[update_cfg]Ascii to hex.");
	file_config[0] = GTP_REG_CONFIG_DATA >> 8;
	file_config[1] = GTP_REG_CONFIG_DATA & 0xff;
	for (i = 0, file_cfg_len = GTP_ADDR_LENGTH; i < count; i += 5) {
		if ((buf[i] == '0') && ((buf[i+1] == 'x') || (buf[i+1] == 'X'))) {
			u8 high, low;
			high = ascii2hex(buf[i+2]);
			low = ascii2hex(buf[i+3]);

			if ((high == 0xFF) || (low == 0xFF)) {
				ret = 0;
				GTP_ERROR("[update_cfg]Illegal config file.");
				goto update_cfg_file_failed;
			}
			file_config[file_cfg_len++] = (high<<4) + low;
		} else {
			ret = 0;
			GTP_ERROR("[update_cfg]Illegal config file.");
			goto update_cfg_file_failed;
		}
	}


	GTP_DEBUG("config:");
	GTP_DEBUG_ARRAY(file_config+2, file_cfg_len);

	i = 0;
	while (i++ < 5) {
		ret = gup_i2c_write(client, file_config, file_cfg_len);
		if (ret > 0) {
			GTP_INFO("[update_cfg]Send config SUCCESS.");
			break;
		}
		GTP_ERROR("[update_cfg]Send config i2c error.");
	}

update_cfg_file_failed:
	kfree(pre_buf);
	kfree(buf);
	kfree(file_config);
	return ret;
}

#endif

#if (GTP_AUTO_UPDATE && (!GTP_HEADER_FW_UPDATE || GTP_AUTO_UPDATE_CFG))
static void gup_search_file(s32 search_type)
{
	s32 i = 0;
	struct file *pfile = NULL;

	got_file_flag = 0x00;

	searching_file = 1;
	for (i = 0; i < GUP_SEARCH_FILE_TIMES; ++i) {
		if (0 == searching_file) {
			GTP_INFO("Force exiting file searching");
			got_file_flag = 0x00;
			return;
		}

		if (search_type & AUTO_SEARCH_BIN) {
			GTP_DEBUG("Search for %s, %s for fw update.(%d/%d)", UPDATE_FILE_PATH_1, UPDATE_FILE_PATH_2, i+1, GUP_SEARCH_FILE_TIMES);
			pfile = filp_open(UPDATE_FILE_PATH_1, O_RDONLY, 0);
			if (IS_ERR(pfile)) {
				pfile = filp_open(UPDATE_FILE_PATH_2, O_RDONLY, 0);
				if (!IS_ERR(pfile)) {
					GTP_INFO("Bin file: %s for fw update.", UPDATE_FILE_PATH_2);
					got_file_flag |= BIN_FILE_READY;
					update_msg.file = pfile;
				}
			} else {
				GTP_INFO("Bin file: %s for fw update.", UPDATE_FILE_PATH_1);
				got_file_flag |= BIN_FILE_READY;
				update_msg.file = pfile;
			}
			if (got_file_flag & BIN_FILE_READY) {
			#if GTP_AUTO_UPDATE_CFG
				if (search_type & AUTO_SEARCH_CFG) {
					i = GUP_SEARCH_FILE_TIMES;
				} else
			#endif
				{
					searching_file = 0;
					return;
				}
			}
		}

	#if GTP_AUTO_UPDATE_CFG
		if ((search_type & AUTO_SEARCH_CFG) && !(got_file_flag & CFG_FILE_READY)) {
			GTP_DEBUG("Search for %s, %s for config update.(%d/%d)", CONFIG_FILE_PATH_1, CONFIG_FILE_PATH_2, i+1, GUP_SEARCH_FILE_TIMES);
			pfile = filp_open(CONFIG_FILE_PATH_1, O_RDONLY, 0);
			if (IS_ERR(pfile)) {
				pfile = filp_open(CONFIG_FILE_PATH_2, O_RDONLY, 0);
				if (!IS_ERR(pfile)) {
					GTP_INFO("Cfg file: %s for config update.", CONFIG_FILE_PATH_2);
					got_file_flag |= CFG_FILE_READY;
					update_msg.cfg_file = pfile;
				}
			} else {
				GTP_INFO("Cfg file: %s for config update.", CONFIG_FILE_PATH_1);
				got_file_flag |= CFG_FILE_READY;
				update_msg.cfg_file = pfile;
			}
			if (got_file_flag & CFG_FILE_READY) {
				searching_file = 0;
				return;
			}
		}
	#endif
		msleep(3000);
	}
	searching_file = 0;
}
#endif

extern int gt9xx_id;

static u8 gup_check_update_file(struct i2c_client  *client, st_fw_head *fw_head, u8 *path)
{
	s32 ret = 0;
	s32 i = 0;
	s32 fw_checksum = 0;
	u8 buf[FW_HEAD_LENGTH];

	got_file_flag = 0x00;
	if (path) {
		GTP_DEBUG("Update File path:%s, %zd", path, strlen(path));
		update_msg.file = filp_open(path, O_RDONLY, 0);

		if (IS_ERR(update_msg.file)) {
			GTP_ERROR("Open update file(%s) error!", path);
			return FAIL;
		}
		got_file_flag = BIN_FILE_READY;
	} else {
#if GTP_AUTO_UPDATE
	#if GTP_HEADER_FW_UPDATE
	if (gt9xx_id == 0) {
		GTP_INFO("Update by gt915 firmware array");
		update_msg.fw_total_len = sizeof(gt915_FW) - FW_HEAD_LENGTH;
		if (sizeof(gt915_FW) < (FW_HEAD_LENGTH+FW_SECTION_LENGTH*4+FW_DSP_ISP_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH)) {
			GTP_ERROR("INVALID gt915_FW, check your gt9xx_firmware.h file!");
			return FAIL;
		}
		GTP_DEBUG("Firmware actual size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);
		memcpy(fw_head, &gt915_FW[0], FW_HEAD_LENGTH);

		fw_checksum = 0;
		for (i = 0; i < update_msg.fw_total_len; i += 2) {
			fw_checksum += (gt915_FW[FW_HEAD_LENGTH + i] << 8) + gt915_FW[FW_HEAD_LENGTH + i + 1];
		}
		GTP_DEBUG("firmware checksum:%x", fw_checksum&0xFFFF);
		if (fw_checksum&0xFFFF) {
			GTP_ERROR("Illegal firmware file.");
			return FAIL;
		}
		got_file_flag = HEADER_FW_READY;
		return SUCCESS;
	} else {
		GTP_INFO("Update by gt917 firmware array");

		update_msg.fw_total_len = sizeof(gt917_FW) - FW_HEAD_LENGTH;
		if (sizeof(gt917_FW) < (FW_HEAD_LENGTH+FW_SECTION_LENGTH*4+FW_DSP_ISP_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH)) {
			GTP_ERROR("INVALID gt917_FW, check your gt9xx_firmware.h file!");
			return FAIL;
		}
		GTP_DEBUG("Firmware actual size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);
		memcpy(fw_head, &gt917_FW[0], FW_HEAD_LENGTH);

		fw_checksum = 0;
		for (i = 0; i < update_msg.fw_total_len; i += 2) {
			fw_checksum += (gt917_FW[FW_HEAD_LENGTH + i] << 8) + gt917_FW[FW_HEAD_LENGTH + i + 1];
		}

		GTP_DEBUG("firmware checksum:%x", fw_checksum&0xFFFF);
		if (fw_checksum&0xFFFF) {
			GTP_ERROR("Illegal firmware file.");
			return FAIL;
		}
		got_file_flag = HEADER_FW_READY;
		return SUCCESS;
	}
	#else

	#if GTP_AUTO_UPDATE_CFG
		gup_search_file(AUTO_SEARCH_BIN | AUTO_SEARCH_CFG);
		if (got_file_flag & CFG_FILE_READY) {
			ret = gup_update_config(i2c_connect_client);
			if (ret <= 0) {
				GTP_ERROR("Update config failed.");
			}
			_CLOSE_FILE(update_msg.cfg_file);
			msleep(500);
		}
	#else
		gup_search_file(AUTO_SEARCH_BIN);
	#endif

		if (!(got_file_flag & BIN_FILE_READY)) {
			GTP_ERROR("No bin file for fw update");
			return FAIL;
		}
	#endif

#else
		{
			GTP_ERROR("NULL file for firmware update");
			return FAIL;
		}
#endif
	}

	update_msg.old_fs = get_fs();
	set_fs(KERNEL_DS);

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	update_msg.fw_total_len = update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_END);
	if (update_msg.fw_total_len < (FW_HEAD_LENGTH + FW_SECTION_LENGTH*4+FW_DSP_ISP_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH)) {
		GTP_ERROR("INVALID bin file(size: %d), update aborted.", update_msg.fw_total_len);
		return FAIL;
	}

	update_msg.fw_total_len -= FW_HEAD_LENGTH;

	GTP_DEBUG("Bin firmware actual size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	ret = update_msg.file->f_op->read(update_msg.file, (char *)buf, FW_HEAD_LENGTH, &update_msg.file->f_pos);
	if (ret < 0) {
		GTP_ERROR("Read firmware head in update file error.");
		return FAIL;
	}

	memcpy(fw_head, buf, FW_HEAD_LENGTH);


	fw_checksum = 0;
	for (i = 0; i < update_msg.fw_total_len; i += 2) {
		u16 temp;
		ret = update_msg.file->f_op->read(update_msg.file, (char *)buf, 2, &update_msg.file->f_pos);
		if (ret < 0) {
			GTP_ERROR("Read firmware file error.");
			return FAIL;
		}

		temp = (buf[0]<<8) + buf[1];
		fw_checksum += temp;
	}

	GTP_DEBUG("firmware checksum:%x", fw_checksum&0xFFFF);
	if (fw_checksum&0xFFFF) {
		GTP_ERROR("Illegal firmware file.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_proc(struct i2c_client *client, u8 *burn_buf, u16 start_addr, u16 total_length)
{
	s32 ret = 0;
	u16 burn_addr = start_addr;
	u16 frame_length = 0;
	u16 burn_length = 0;
	u8  wr_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8  rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8  retry = 0;

	GTP_DEBUG("Begin burn %dk data to addr 0x%x", (total_length/1024), start_addr);
	while (burn_length < total_length) {
		GTP_DEBUG("B/T:%04d/%04d", burn_length, total_length);
		frame_length = ((total_length - burn_length) > PACK_SIZE) ? PACK_SIZE : (total_length - burn_length);
		wr_buf[0] = (u8)(burn_addr>>8);
		rd_buf[0] = wr_buf[0];
		wr_buf[1] = (u8)burn_addr;
		rd_buf[1] = wr_buf[1];
		memcpy(&wr_buf[GTP_ADDR_LENGTH], &burn_buf[burn_length], frame_length);

		for (retry = 0; retry < MAX_FRAME_CHECK_TIME; retry++) {
			ret = gup_i2c_write(client, wr_buf, GTP_ADDR_LENGTH + frame_length);
			if (ret <= 0) {
				GTP_ERROR("Write frame data i2c error.");
				continue;
			}
			ret = gup_i2c_read(client, rd_buf, GTP_ADDR_LENGTH + frame_length);
			if (ret <= 0) {
				GTP_ERROR("Read back frame data i2c error.");
				continue;
			}

			if (memcmp(&wr_buf[GTP_ADDR_LENGTH], &rd_buf[GTP_ADDR_LENGTH], frame_length)) {
				GTP_ERROR("Check frame data fail,not equal.");
				GTP_DEBUG("write array:");
				GTP_DEBUG_ARRAY(&wr_buf[GTP_ADDR_LENGTH], frame_length);
				GTP_DEBUG("read array:");
				GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
				continue;
			} else {

				break;
			}
		}
		if (retry >= MAX_FRAME_CHECK_TIME) {
			GTP_ERROR("Burn frame data time out,exit.");
			return FAIL;
		}
		burn_length += frame_length;
		burn_addr += frame_length;
	}
	return SUCCESS;
}

static u8 gup_load_section_file(u8 *buf, u32 offset, u16 length, u8 set_or_end)
{
#if (GTP_AUTO_UPDATE && GTP_HEADER_FW_UPDATE)
	if (got_file_flag == HEADER_FW_READY) {
		if(SEEK_SET == set_or_end) {
			memcpy(buf, &gt915_FW[FW_HEAD_LENGTH + offset], length);
		} else {
			memcpy(buf, &gt915_FW[update_msg.fw_total_len + FW_HEAD_LENGTH - offset], length);
		}
		return SUCCESS;
	}
#endif
	{
		s32 ret = 0;

		if ((update_msg.file == NULL) || IS_ERR(update_msg.file)) {
			GTP_ERROR("cannot find update file,load section file fail.");
			return FAIL;
		}

		if (SEEK_SET == set_or_end) {
			update_msg.file->f_pos = FW_HEAD_LENGTH + offset;
		} else {
			update_msg.file->f_pos = update_msg.fw_total_len + FW_HEAD_LENGTH - offset;
		}

		ret = update_msg.file->f_op->read(update_msg.file, (char *)buf, length, &update_msg.file->f_pos);

		if (ret < 0) {
			GTP_ERROR("Read update file fail.");
			return FAIL;
		}

		return SUCCESS;
	}
}

static u8 gup_recall_check(struct i2c_client  *client, u8 *chk_src, u16 start_rd_addr, u16 chk_length)
{
	u8  rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	s32 ret = 0;
	u16 recall_addr = start_rd_addr;
	u16 recall_length = 0;
	u16 frame_length = 0;

	while (recall_length < chk_length) {
		frame_length = ((chk_length - recall_length) > PACK_SIZE)? PACK_SIZE : (chk_length - recall_length);
		ret = gup_get_ic_msg(client, recall_addr, rd_buf, frame_length);
		if (ret <= 0) {
			GTP_ERROR("recall i2c error,exit");
			return FAIL;
		}

		if (memcmp(&rd_buf[GTP_ADDR_LENGTH], &chk_src[recall_length], frame_length)) {
			GTP_ERROR("Recall frame data fail,not equal.");
			GTP_DEBUG("chk_src array:");
			GTP_DEBUG_ARRAY(&chk_src[recall_length], frame_length);
			GTP_DEBUG("recall array:");
			GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
			return FAIL;
		}

		recall_length += frame_length;
		recall_addr += frame_length;
	}
	GTP_DEBUG("Recall check %dk firmware success.", (chk_length/1024));

	return SUCCESS;
}

static u8 gup_burn_fw_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u8 bank_cmd)
{
	s32 ret = 0;
	u8  rd_buf[5];


	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]hold ss51 & dsp fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]set scramble fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4)&0x0F);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4)&0x0F);
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}


	ret = gup_burn_proc(client, fw_section, start_addr, FW_SECTION_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_section]burn fw_section fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]hold ss51 & release dsp fail.");
		return FAIL;
	}

	msleep(1);


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd&0x0f);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]send burn cmd fail.");
		return FAIL;
	}
	GTP_DEBUG("[burn_fw_section]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			GTP_ERROR("[burn_fw_section]Get burn state fail");
			return FAIL;
		}
		msleep(10);

	} while (rd_buf[GTP_ADDR_LENGTH]);


	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4)&0x0F);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4)&0x0F);
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}


	ret = gup_recall_check(client, fw_section, start_addr, FW_SECTION_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_section]recall check %dk firmware fail.", FW_SECTION_LENGTH/1024);
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]disable accessing code fail.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_dsp_isp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp_isp = NULL;
	u8  retry = 0;

	GTP_INFO("[burn_dsp_isp]Begin burn dsp isp---->>");


	GTP_DEBUG("[burn_dsp_isp]step1:alloc memory");
	while (retry++ < 5) {
		fw_dsp_isp = (u8 *)kzalloc(FW_DSP_ISP_LENGTH, GFP_KERNEL);
		if (fw_dsp_isp == NULL) {
			continue;
		} else {
			GTP_INFO("[burn_dsp_isp]Alloc %dk byte memory success.", (FW_DSP_ISP_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_dsp_isp]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_DEBUG("[burn_dsp_isp]step2:load dsp isp file data");
	ret = gup_load_section_file(fw_dsp_isp, FW_DSP_ISP_LENGTH, FW_DSP_ISP_LENGTH, SEEK_END);
	if (FAIL == ret) {
		GTP_ERROR("[burn_dsp_isp]load firmware dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step3:disable wdt,clear cache enable");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]disable wdt fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}
	ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]clear cache enable fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step4:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step5:set boot from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]set boot from sram fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step6:software reboot");
	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]software reboot fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step7:select bank2");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]select bank2 fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step8:enable accessing code");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]enable accessing code fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step9:burn 4k dsp_isp");
	ret = gup_burn_proc(client, fw_dsp_isp, 0xC000, FW_DSP_ISP_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_dsp_isp]burn dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}


	GTP_DEBUG("[burn_dsp_isp]step10:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}
	update_msg.fw_burned_len += FW_DSP_ISP_LENGTH;
	GTP_DEBUG("[burn_dsp_isp]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_dsp_isp:
	kfree(fw_dsp_isp);
	return ret;
}

static u8 gup_burn_fw_ss51(struct i2c_client *client)
{
	u8 *fw_ss51 = NULL;
	u8  retry = 0;
	s32 ret = 0;

	GTP_INFO("[burn_fw_ss51]Begin burn ss51 firmware---->>");


	GTP_DEBUG("[burn_fw_ss51]step1:alloc memory");
	while (retry++ < 5) {
		fw_ss51 = (u8 *)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (fw_ss51 == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_ss51]Alloc %dk byte memory success.", (FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_ss51]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_INFO("[burn_fw_ss51]Reset first 8K of ss51 to 0xFF.");
	GTP_DEBUG("[burn_fw_ss51]step2: reset bank0 0xC000~0xD000");
	memset(fw_ss51, 0xFF, FW_SECTION_LENGTH);


	GTP_DEBUG("[burn_fw_ss51]step3:clear control flag");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_ss51]clear control flag fail.");
		ret = FAIL;
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step4:burn ss51 firmware section 1");
	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 1 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step5:load ss51 firmware section 2 file data");
	ret = gup_load_section_file(fw_ss51, FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step6:burn ss51 firmware section 2");
	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x02);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step7:load ss51 firmware section 3 file data");
	ret = gup_load_section_file(fw_ss51, 2 * FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step8:burn ss51 firmware section 3");
	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x13);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step9:load ss51 firmware section 4 file data");
	ret = gup_load_section_file(fw_ss51, 3 * FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}


	GTP_DEBUG("[burn_fw_ss51]step10:burn ss51 firmware section 4");
	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x14);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}

	update_msg.fw_burned_len += (FW_SECTION_LENGTH*4);
	GTP_DEBUG("[burn_fw_ss51]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_ss51:
	kfree(fw_ss51);
	return ret;
}

static u8 gup_burn_fw_dsp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp = NULL;
	u8  retry = 0;
	u8  rd_buf[5];

	GTP_INFO("[burn_fw_dsp]Begin burn dsp firmware---->>");

	GTP_DEBUG("[burn_fw_dsp]step1:alloc memory");
	while (retry++ < 5) {
		fw_dsp = (u8 *)kzalloc(FW_DSP_LENGTH, GFP_KERNEL);
		if (fw_dsp == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_dsp]Alloc %dk byte memory success.", (FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_dsp]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_DEBUG("[burn_fw_dsp]step2:load firmware dsp");
	ret = gup_load_section_file(fw_dsp, 4 * FW_SECTION_LENGTH, FW_DSP_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]load firmware dsp fail.");
		goto exit_burn_fw_dsp;
	}


	GTP_DEBUG("[burn_fw_dsp]step3:select bank3");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}


	GTP_DEBUG("[burn_fw_dsp]step4:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}


	GTP_DEBUG("[burn_fw_dsp]step5:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}


	GTP_DEBUG("[burn_fw_dsp]step6:release ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	msleep(1);


	GTP_DEBUG("[burn_fw_dsp]step7:burn 4k dsp firmware");
	ret = gup_burn_proc(client, fw_dsp, 0x9000, FW_DSP_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]burn fw_section fail.");
		goto exit_burn_fw_dsp;
	}


	GTP_DEBUG("[burn_fw_dsp]step8:send burn cmd to move data to flash from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x05);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]send burn cmd fail.");
		goto exit_burn_fw_dsp;
	}
	GTP_DEBUG("[burn_fw_dsp]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			GTP_ERROR("[burn_fw_dsp]Get burn state fail");
			goto exit_burn_fw_dsp;
		}
		msleep(10);

	} while(rd_buf[GTP_ADDR_LENGTH]);


	GTP_DEBUG("[burn_fw_dsp]step9:recall check 4k dsp firmware");
	ret = gup_recall_check(client, fw_dsp, 0x9000, FW_DSP_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]recall check 4k dsp firmware fail.");
		goto exit_burn_fw_dsp;
	}

	update_msg.fw_burned_len += FW_DSP_LENGTH;
	GTP_DEBUG("[burn_fw_dsp]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_dsp:
	kfree(fw_dsp);
	return ret;
}

static u8 gup_burn_fw_boot(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_boot = NULL;
	u8  retry = 0;
	u8  rd_buf[5];

	GTP_INFO("[burn_fw_boot]Begin burn bootloader firmware---->>");


	GTP_DEBUG("[burn_fw_boot]step1:Alloc memory");
	while (retry++ < 5) {
		fw_boot = (u8 *)kzalloc(FW_BOOT_LENGTH, GFP_KERNEL);
		if (fw_boot == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_boot]Alloc %dk byte memory success.", (FW_BOOT_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_boot]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_DEBUG("[burn_fw_boot]step2:load firmware bootloader");
	ret = gup_load_section_file(fw_boot, (4 * FW_SECTION_LENGTH + FW_DSP_LENGTH), FW_BOOT_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]load firmware bootcode fail.");
		goto exit_burn_fw_boot;
	}


	GTP_DEBUG("[burn_fw_boot]step3:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}


	GTP_DEBUG("[burn_fw_boot]step4:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}


	GTP_DEBUG("[burn_fw_boot]step5:hold ss51 & release dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	msleep(1);


	GTP_DEBUG("[burn_fw_boot]step6:select bank3");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}


	GTP_DEBUG("[burn_fw_boot]step6:burn 2k bootloader firmware");
	ret = gup_burn_proc(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]burn fw_boot fail.");
		goto exit_burn_fw_boot;
	}


	GTP_DEBUG("[burn_fw_boot]step7:send burn cmd to move data to flash from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x06);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]send burn cmd fail.");
		goto exit_burn_fw_boot;
	}
	GTP_DEBUG("[burn_fw_boot]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			GTP_ERROR("[burn_fw_boot]Get burn state fail");
			goto exit_burn_fw_boot;
		}
		msleep(10);

	} while(rd_buf[GTP_ADDR_LENGTH]);


	GTP_DEBUG("[burn_fw_boot]step8:recall check 2k bootloader firmware");
	ret = gup_recall_check(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]recall check 2k bootcode firmware fail.");
		goto exit_burn_fw_boot;
	}

	update_msg.fw_burned_len += FW_BOOT_LENGTH;
	GTP_DEBUG("[burn_fw_boot]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_boot:
	kfree(fw_boot);
	return ret;
}
static u8 gup_burn_fw_boot_isp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_boot_isp = NULL;
	u8  retry = 0;
	u8  rd_buf[5];

	if (update_msg.fw_burned_len >= update_msg.fw_total_len) {
		GTP_DEBUG("No need to upgrade the boot_isp code!");
		return SUCCESS;
	}
	GTP_INFO("[burn_fw_boot_isp]Begin burn boot_isp firmware---->>");


	GTP_DEBUG("[burn_fw_boot_isp]step1:Alloc memory");
	while (retry++ < 5) {
		fw_boot_isp = (u8 *)kzalloc(FW_BOOT_ISP_LENGTH, GFP_KERNEL);
		if (fw_boot_isp == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_boot_isp]Alloc %dk byte memory success.", (FW_BOOT_ISP_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_boot_isp]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_DEBUG("[burn_fw_boot_isp]step2:load firmware bootloader isp");

	ret = gup_load_section_file(fw_boot_isp, (update_msg.fw_burned_len - FW_DSP_ISP_LENGTH), FW_BOOT_ISP_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot_isp]load firmware boot_isp fail.");
		goto exit_burn_fw_boot_isp;
	}


	GTP_DEBUG("[burn_fw_boot_isp]step3:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot_isp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}


	GTP_DEBUG("[burn_fw_boot_isp]step4:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot_isp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}



	GTP_DEBUG("[burn_fw_boot_isp]step5:hold ss51 & release dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot_isp]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}

	msleep(1);


	GTP_DEBUG("[burn_fw_boot_isp]step6:select bank3");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot_isp]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}


	GTP_DEBUG("[burn_fw_boot_isp]step7:burn 2k bootloader firmware");
	ret = gup_burn_proc(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot_isp]burn fw_section fail.");
		goto exit_burn_fw_boot_isp;
	}


	GTP_DEBUG("[burn_fw_boot_isp]step8:send burn cmd to move data to flash from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x07);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot_isp]send burn cmd fail.");
		goto exit_burn_fw_boot_isp;
	}
	GTP_DEBUG("[burn_fw_boot_isp]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			GTP_ERROR("[burn_fw_boot_isp]Get burn state fail");
			goto exit_burn_fw_boot_isp;
		}
		msleep(10);

	} while(rd_buf[GTP_ADDR_LENGTH]);


	GTP_DEBUG("[burn_fw_boot_isp]step9:recall check 2k bootloader firmware");
	ret = gup_recall_check(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot_isp]recall check 2k bootcode_isp firmware fail.");
		goto exit_burn_fw_boot_isp;
	}

	update_msg.fw_burned_len += FW_BOOT_ISP_LENGTH;
	GTP_DEBUG("[burn_fw_boot_isp]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_boot_isp:
	kfree(fw_boot_isp);
	return ret;
}

static u8 gup_burn_fw_link(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_link = NULL;
	u8  retry = 0;
	u32 offset;

	if (update_msg.fw_burned_len >= update_msg.fw_total_len) {
		GTP_DEBUG("No need to upgrade the link code!");
		return SUCCESS;
	}
	GTP_INFO("[burn_fw_link]Begin burn link firmware---->>");


	GTP_DEBUG("[burn_fw_link]step1:Alloc memory");
	while (retry++ < 5) {
		fw_link = (u8 *)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (fw_link == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_link]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_link]Alloc memory fail,exit.");
		return FAIL;
	}


	GTP_DEBUG("[burn_fw_link]step2:load firmware link section 1");
	offset = update_msg.fw_burned_len - FW_DSP_ISP_LENGTH;
	ret = gup_load_section_file(fw_link, offset, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_link]load firmware link section 1 fail.");
		goto exit_burn_fw_link;
	}


	GTP_DEBUG("[burn_fw_link]step3:burn link firmware section 1");
	ret = gup_burn_fw_gwake_section(client, fw_link, 0x9000, FW_SECTION_LENGTH, 0x38);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_link]burn link firmware section 1 fail.");
		goto exit_burn_fw_link;
	}


	GTP_DEBUG("[burn_fw_link]step4:load link firmware section 2 file data");
	offset += FW_SECTION_LENGTH;
	ret = gup_load_section_file(fw_link, offset, FW_GLINK_LENGTH - FW_SECTION_LENGTH, SEEK_SET);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_link]load link firmware section 2 fail.");
		goto exit_burn_fw_link;
	}


	GTP_DEBUG("[burn_fw_link]step4:burn link firmware section 2");
	ret = gup_burn_fw_gwake_section(client, fw_link, 0x9000, FW_GLINK_LENGTH - FW_SECTION_LENGTH, 0x39);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_link]burn link firmware section 2 fail.");
		goto exit_burn_fw_link;
	}

	update_msg.fw_burned_len += FW_GLINK_LENGTH;
	GTP_DEBUG("[burn_fw_link]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_link:
	kfree(fw_link);
	return ret;
}

static u8 gup_burn_fw_gwake_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u32 len, u8 bank_cmd)
{
	s32 ret = 0;
	u8  rd_buf[5];


	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_app_section]hold ss51 & dsp fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_app_section]set scramble fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_app_section]hold ss51 & release dsp fail.");
		return FAIL;
	}

	msleep(1);


	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4)&0x0F);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4)&0x0F);
		return FAIL;
	}


	ret = gup_burn_proc(client, fw_section, start_addr, len);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_app_section]burn fw_section fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd&0x0F);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_app_section]send burn cmd fail.");
		return FAIL;
	}
	GTP_DEBUG("[burn_fw_section]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			GTP_ERROR("[burn_fw_app_section]Get burn state fail");
			return FAIL;
		}
		msleep(10);

	} while(rd_buf[GTP_ADDR_LENGTH]);


	ret = gup_recall_check(client, fw_section, start_addr, len);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_app_section]recall check %dk firmware fail.", len/1024);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_fw_gwake(struct i2c_client *client)
{
	u8 *fw_gwake = NULL;
	u8  retry = 0;
	s32 ret = 0;
	u16 start_index = 4*FW_SECTION_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH + FW_BOOT_ISP_LENGTH + FW_GLINK_LENGTH;


	if (update_msg.fw_burned_len >= update_msg.fw_total_len) {
		GTP_DEBUG("No need to upgrade the gwake code!");
		return SUCCESS;
	}

	GTP_INFO("[burn_fw_gwake]Begin burn gwake firmware---->>");


	GTP_DEBUG("[burn_fw_gwake]step1:alloc memory");
	while (retry++ < 5) {
		fw_gwake = (u8 *)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (fw_gwake == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_gwake]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_gwake]Alloc memory fail,exit.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_finish]clear control flag fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step2:load app_code firmware section 1 file data");
	ret = gup_load_section_file(fw_gwake, start_index, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]load app_code firmware section 1 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step3:burn app_code firmware section 1");
	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000, FW_SECTION_LENGTH, 0x3A);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]burn app_code firmware section 1 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step5:load app_code firmware section 2 file data");
	ret = gup_load_section_file(fw_gwake, start_index+FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]load app_code firmware section 2 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step6:burn app_code firmware section 2");
	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000, FW_SECTION_LENGTH, 0x3B);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]burn app_code firmware section 2 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step7:load app_code firmware section 3 file data");
	ret = gup_load_section_file(fw_gwake, start_index+2*FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]load app_code firmware section 3 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step8:burn app_code firmware section 3");
	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000, FW_SECTION_LENGTH, 0x3C);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]burn app_code firmware section 3 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step9:load app_code firmware section 4 file data");
	ret = gup_load_section_file(fw_gwake, start_index + 3*FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]load app_code firmware section 4 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]step10:burn app_code firmware section 4");
	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000, FW_SECTION_LENGTH, 0x3D);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_gwake]burn app_code firmware section 4 fail.");
		goto exit_burn_fw_gwake;
	}


	GTP_DEBUG("[burn_fw_gwake]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_gwake:
	kfree(fw_gwake);
	return ret;
}

static u8 gup_burn_fw_finish(struct i2c_client *client)
{
	u8 *fw_ss51 = NULL;
	u8  retry = 0;
	s32 ret = 0;

	GTP_INFO("[burn_fw_finish]burn first 8K of ss51 and finish update.");

	GTP_DEBUG("[burn_fw_finish]step1:alloc memory");
	while (retry++ < 5) {
		fw_ss51 = (u8 *)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (fw_ss51 == NULL) {
			continue;
		} else {
			GTP_DEBUG("[burn_fw_finish]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
			break;
		}
	}
	if (retry >= 5) {
		GTP_ERROR("[burn_fw_finish]Alloc memory fail,exit.");
		return FAIL;
	}

	GTP_DEBUG("[burn_fw_finish]step2: burn ss51 first 8K.");
	ret = gup_load_section_file(fw_ss51, 0, FW_SECTION_LENGTH, SEEK_SET);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_finish]load ss51 firmware section 1 fail.");
		goto exit_burn_fw_finish;
	}

	GTP_DEBUG("[burn_fw_finish]step3:clear control flag");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_finish]clear control flag fail.");
		goto exit_burn_fw_finish;
	}

	GTP_DEBUG("[burn_fw_finish]step4:burn ss51 firmware section 1");
	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);
	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_finish]burn ss51 firmware section 1 fail.");
		goto exit_burn_fw_finish;
	}


	GTP_DEBUG("[burn_fw_finish]step5:enable download DSP code ");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x99);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_finish]enable download DSP code fail.");
		goto exit_burn_fw_finish;
	}


	GTP_DEBUG("[burn_fw_finish]step6:release ss51 & hold dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x08);
	if (ret <= 0) {
		GTP_ERROR("[burn_fw_finish]release ss51 & hold dsp fail.");
		goto exit_burn_fw_finish;
	}

	if (fw_ss51) {
		kfree(fw_ss51);
	}
	return SUCCESS;

exit_burn_fw_finish:
	if (fw_ss51) {
		kfree(fw_ss51);
	}
	return FAIL;
}
s32 gup_update_proc(void *dir)
{
	s32 ret = 0;
	s32 update_ret = FAIL;
	u8  retry = 0;
	st_fw_head fw_head;
	struct goodix_ts_data *ts = NULL;

	GTP_DEBUG("[update_proc]Begin update ......");

	ts = i2c_get_clientdata(i2c_connect_client);

#if GTP_AUTO_UPDATE
	if (searching_file) {
		u8 timeout = 0;
		searching_file = 0;
		GTP_INFO("Exiting searching .bin update file...");
		while ((show_len != 200) && (show_len != 100) && (timeout++ < 100)) {
			msleep(100);
		}
	}
#endif

	show_len = 1;
	total_len = 100;

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		return gup_fw_download_proc(dir, GTP_FL_FW_BURN);
	}
#endif

	update_msg.file = NULL;
	ret = gup_check_update_file(i2c_connect_client, &fw_head, (u8 *)dir);
	if (FAIL == ret) {
		GTP_ERROR("[update_proc]check update file fail.");
		goto file_fail;
	}

	ret = gup_get_ic_fw_msg(i2c_connect_client);
	if (FAIL == ret) {
		GTP_ERROR("[update_proc]get ic message fail.");
		goto file_fail;
	}

	ret = gup_enter_update_judge(&fw_head);
	if (FAIL == ret) {
		GTP_ERROR("[update_proc]Check *.bin file fail.");
		goto file_fail;
	}

	ts->enter_update = 1;
	gtp_irq_disable(ts);
#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_OFF);
#endif
	ret = gup_enter_update_mode(i2c_connect_client);
	if (FAIL == ret) {
		 GTP_ERROR("[update_proc]enter update mode fail.");
		 goto update_fail;
	}

	while (retry++ < 5) {
		show_len = 10;
		total_len = 100;
		update_msg.fw_burned_len = 0;
		ret = gup_burn_dsp_isp(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn dsp isp fail.");
			continue;
		}

		show_len = 20;
		ret = gup_burn_fw_gwake(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn app_code firmware fail.");
			continue;
		}

		show_len = 30;
		ret = gup_burn_fw_ss51(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn ss51 firmware fail.");
			continue;
		}

		show_len = 40;
		ret = gup_burn_fw_dsp(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn dsp firmware fail.");
			continue;
		}

		show_len = 50;
		ret = gup_burn_fw_boot(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn bootloader firmware fail.");
			continue;
		}
		show_len = 60;

		ret = gup_burn_fw_boot_isp(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn boot_isp firmware fail.");
			continue;
		}

		show_len = 70;
		ret = gup_burn_fw_link(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn link firmware fail.");
			continue;
		}

		show_len = 80;
		ret = gup_burn_fw_finish(i2c_connect_client);
		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn finish fail.");
			continue;
		}
		show_len = 90;
		GTP_INFO("[update_proc]UPDATE SUCCESS.");
		retry = 0;
		break;
	}
	if (retry >= 5) {
		GTP_ERROR("[update_proc]retry timeout,UPDATE FAIL.");
		update_ret = FAIL;
	} else {
		update_ret = SUCCESS;
	}

update_fail:
	GTP_DEBUG("[update_proc]leave update mode.");
	gup_leave_update_mode();

	msleep(100);

	if (SUCCESS == update_ret) {
		if (ts->fw_error) {
			GTP_INFO("firmware error auto update, resent config!");
			gup_init_panel(ts);
		} else {
			GTP_DEBUG("[update_proc]send config.");
			ret = gtp_send_cfg(i2c_connect_client);
			if (ret < 0) {
				GTP_ERROR("[update_proc]send config fail.");
			} else {
				msleep(100);
			}
		}
	}
	ts->enter_update = 0;
	gtp_irq_enable(ts);

#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_ON);
#endif

file_fail:
	if (update_msg.file && !IS_ERR(update_msg.file)) {
		if (update_msg.old_fs) {
			set_fs(update_msg.old_fs);
		}
		filp_close(update_msg.file, NULL);
	}
#if (GTP_AUTO_UPDATE && GTP_AUTO_UPDATE_CFG && GTP_HEADER_FW_UPDATE)
	if (NULL == dir) {
		gup_search_file(AUTO_SEARCH_CFG);
		if (got_file_flag & CFG_FILE_READY) {
			ret = gup_update_config(i2c_connect_client);
			if (ret <= 0) {
				GTP_ERROR("Update config failed.");
			}
			_CLOSE_FILE(update_msg.cfg_file);
			msleep(500);
		}
	}
#endif

	total_len = 100;
	if (SUCCESS == update_ret) {
		show_len = 100;
		return SUCCESS;
	} else {
		show_len = 200;
		return FAIL;
	}
}

#if GTP_AUTO_UPDATE
u8 gup_init_update_proc(struct goodix_ts_data *ts)
{
	struct task_struct *thread = NULL;

	GTP_INFO("Ready to run update thread.");

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		thread = kthread_run(gup_update_proc, "update", "fl update");
	} else
#endif
	{
		thread = kthread_run(gup_update_proc, (void *)NULL, "guitar_update");
	}
	if (IS_ERR(thread)) {
		GTP_ERROR("Failed to create update thread.\n");
		return -EPERM;
	}

	return 0;
}
#endif



#define FW_DOWNLOAD_LENGTH           0x4000
#define FW_SS51_SECTION_LEN          0x2000
#define FL_PACK_SIZE                 1024
#define GUP_FW_CHK_SIZE              FL_PACK_SIZE

#define FL_UPDATE_PATH              "/data/_fl_update_.bin"
#define FL_UPDATE_PATH_SD           "/sdcard/_fl_update_.bin"

#define PULSE_LENGTH      (200)
#define INIT_CLK_DAC      (50)
#define MAX_CLK_DAC       (120)
#define CLK_AVG_TIME      (1)
#define MILLION           1000000

#define _wRW_MISCTL__RG_DMY                       0x4282
#define _bRW_MISCTL__RG_OSC_CALIB                 0x4268
#define _fRW_MISCTL__GIO0                         0x41e9
#define _fRW_MISCTL__GIO1                         0x41ed
#define _fRW_MISCTL__GIO2                         0x41f1
#define _fRW_MISCTL__GIO3                         0x41f5
#define _fRW_MISCTL__GIO4                         0x41f9
#define _fRW_MISCTL__GIO5                         0x41fd
#define _fRW_MISCTL__GIO6                         0x4201
#define _fRW_MISCTL__GIO7                         0x4205
#define _fRW_MISCTL__GIO8                         0x4209
#define _fRW_MISCTL__GIO9                         0x420d
#define _fRW_MISCTL__MEA                          0x41a0
#define _bRW_MISCTL__MEA_MODE                     0x41a1
#define _wRW_MISCTL__MEA_MAX_NUM                  0x41a4
#define _dRO_MISCTL__MEA_VAL                      0x41b0
#define _bRW_MISCTL__MEA_SRCSEL                   0x41a3
#define _bRO_MISCTL__MEA_RDY                      0x41a8
#define _rRW_MISCTL__ANA_RXADC_B0_                0x4250
#define _bRW_MISCTL__RG_LDO_A18_PWD               0x426f
#define _bRW_MISCTL__RG_BG_PWD                    0x426a
#define _bRW_MISCTL__RG_CLKGEN_PWD                0x4269
#define _fRW_MISCTL__RG_RXADC_PWD                 0x426a
#define _bRW_MISCTL__OSC_CK_SEL                   0x4030
#define _rRW_MISCTL_RG_DMY83                      0x4283
#define _rRW_MISCTL__GIO1CTL_B2_                  0x41ee
#define _rRW_MISCTL__GIO1CTL_B1_                  0x41ed


#if GTP_COMPATIBLE_MODE

u8 i2c_opr_buf[GTP_ADDR_LENGTH + FL_PACK_SIZE] = {0};
u8 chk_cmp_buf[FL_PACK_SIZE] = {0};

extern s32 gtp_fw_startup(struct i2c_client *client);
static u8 gup_download_fw_dsp(struct i2c_client *client, u8 dwn_mode);
static s32 gup_burn_fw_proc(struct i2c_client *client, u16 start_addr, s32 start_index, s32 burn_len);
static s32 gup_check_and_repair(struct i2c_client *client, u16 start_addr, s32 start_index, s32 chk_len);




u8 gup_check_fs_mounted(char *path_name)
{
	struct path root_path;
	struct path path;
	int err = -1;
	err = kern_path("/", LOOKUP_FOLLOW, &root_path);

	if (err) {
		GTP_DEBUG("\"/\" NOT Mounted: %d", err);
		return FAIL;
	}
	err = kern_path(path_name, LOOKUP_FOLLOW, &path);

	if (err) {
		GTP_DEBUG("%s NOT Mounted: %d", path_name, err);
		err = FAIL;
		goto exit_chk;
	}

	if (path.mnt->mnt_sb == root_path.mnt->mnt_sb) {
		err = FAIL;
	} else {
		err = SUCCESS;
	}

	path_put(&path);
exit_chk:
	path_put(&root_path);
	return err;
}

s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len)
{
	s32 ret = 0;
	s32 write_bytes = 0;
	s32 retry = 0;
	u8 *tx_buf = buf;

	while (len > 0) {
		i2c_opr_buf[0] = (u8)(addr >> 8);
		i2c_opr_buf[1] = (u8)(addr & 0xFF);
		if (len > FL_PACK_SIZE) {
			write_bytes = FL_PACK_SIZE;
		} else {
			write_bytes = len;
		}
		memcpy(i2c_opr_buf + 2, tx_buf, write_bytes);
		for (retry = 0; retry < 5; ++retry) {
			ret = gup_i2c_write(client, i2c_opr_buf, write_bytes + GTP_ADDR_LENGTH);
			if (ret == 1) {
				break;
			}
		}
		if (retry >= 5) {
			GTP_ERROR("retry timeout, I2C write 0x%04X %d bytes failed!", addr, write_bytes);
			return -EPERM;
		}
		addr += write_bytes;
		len -= write_bytes;
		tx_buf += write_bytes;
	}

	return 1;
}

s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len)
{
	s32 ret = 0;
	s32 read_bytes = 0;
	s32 retry = 0;
	u8 *tx_buf = buf;

	while (len > 0) {
		i2c_opr_buf[0] = (u8)(addr >> 8);
		i2c_opr_buf[1] = (u8)(addr & 0xFF);
		if (len > FL_PACK_SIZE) {
			read_bytes = FL_PACK_SIZE;
		} else {
			read_bytes = len;
		}
		for (retry = 0; retry < 5; ++retry) {
			ret = gup_i2c_read(client, i2c_opr_buf, read_bytes + GTP_ADDR_LENGTH);
			if (ret == 2) {
				break;
			}
		}
		if (retry >= 5) {
			GTP_ERROR("retry timeout, I2C read 0x%04X %d bytes failed!", addr, read_bytes);
			return -EPERM;
		}
		memcpy(tx_buf, i2c_opr_buf + 2, read_bytes);
		addr += read_bytes;
		len -= read_bytes;
		tx_buf += read_bytes;
	}
	return 2;
}





static void gup_bit_write(s32 addr, s32 bit, s32 val)
{
	u8 buf;
	i2c_read_bytes(i2c_connect_client, addr, &buf, 1);

	buf = (buf & (~((u8)1 << bit))) | ((u8)val << bit);

	i2c_write_bytes(i2c_connect_client, addr, &buf, 1);
}

static void gup_clk_count_init(s32 bCh, s32 bCNT)
{
	u8 buf;


	gup_bit_write(_fRW_MISCTL__MEA, 0, 0);

	gup_bit_write(_fRW_MISCTL__MEA, 1, 1);

	buf = 0;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__MEA_MODE, &buf, 1);

	buf = 8 + bCh;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__MEA_SRCSEL, &buf, 1);

	buf = bCNT;
	i2c_write_bytes(i2c_connect_client, _wRW_MISCTL__MEA_MAX_NUM, &buf, 1);

	gup_bit_write(_fRW_MISCTL__MEA, 1, 0);

	gup_bit_write(_fRW_MISCTL__MEA, 0, 1);
}

static u32 gup_clk_count_get(void)
{
	s32 ready = 0;
	s32 temp;
	s8  buf[4];

	while ((ready == 0)) {
		i2c_read_bytes(i2c_connect_client, _bRO_MISCTL__MEA_RDY, buf, 1);
		ready = buf[0];
	}

	msleep(50);


	gup_bit_write(_fRW_MISCTL__MEA, 0, 0);
	i2c_read_bytes(i2c_connect_client, _dRO_MISCTL__MEA_VAL, buf, 4);
	GTP_DEBUG("Clk_count 0: %2X", buf[0]);
	GTP_DEBUG("Clk_count 1: %2X", buf[1]);
	GTP_DEBUG("Clk_count 2: %2X", buf[2]);
	GTP_DEBUG("Clk_count 3: %2X", buf[3]);

	temp = (s32)buf[0] + ((s32)buf[1] << 8) + ((s32)buf[2] << 16) + ((s32)buf[3] << 24);
	GTP_INFO("Clk_count : %d", temp);
	return temp;
}
u8 gup_clk_dac_setting(int dac)
{
	s8 buf1, buf2;

	i2c_read_bytes(i2c_connect_client, _wRW_MISCTL__RG_DMY, &buf1, 1);
	i2c_read_bytes(i2c_connect_client, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);

	buf1 = (buf1 & 0xFFCF) | ((dac & 0x03) << 4);
	buf2 = (dac >> 2) & 0x3f;

	i2c_write_bytes(i2c_connect_client, _wRW_MISCTL__RG_DMY, &buf1, 1);
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);

	return 0;
}

static u8 gup_clk_calibration_pin_select(s32 bCh)
{
	s32 i2c_addr;

	switch (bCh) {
	case 0:
		i2c_addr = _fRW_MISCTL__GIO0;
		break;

	case 1:
		i2c_addr = _fRW_MISCTL__GIO1;
		break;

	case 2:
		i2c_addr = _fRW_MISCTL__GIO2;
		break;

	case 3:
		i2c_addr = _fRW_MISCTL__GIO3;
		break;

	case 4:
		i2c_addr = _fRW_MISCTL__GIO4;
		break;

	case 5:
		i2c_addr = _fRW_MISCTL__GIO5;
		break;

	case 6:
		i2c_addr = _fRW_MISCTL__GIO6;
		break;

	case 7:
		i2c_addr = _fRW_MISCTL__GIO7;
		break;

	case 8:
		i2c_addr = _fRW_MISCTL__GIO8;
		break;

	case 9:
		i2c_addr = _fRW_MISCTL__GIO9;
		break;
	}

	gup_bit_write(i2c_addr, 1, 0);

	return 0;
}

void gup_output_pulse(int t)
{
	unsigned long flags;


	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
	msleep(10);

	local_irq_save(flags);

	GTP_GPIO_OUTPUT(gtp_int_gpio, 1);
	msleep(50);
	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
	msleep(t - 50);
	GTP_GPIO_OUTPUT(gtp_int_gpio, 1);

	local_irq_restore(flags);

	msleep(20);
	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
}

static void gup_sys_clk_init(void)
{
	u8 buf;


	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 5, 0);

	buf = 0;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__RG_LDO_A18_PWD, &buf, 1);

	buf = 0;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__RG_BG_PWD, &buf, 1);

	buf = 0;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__RG_CLKGEN_PWD, &buf, 1);

	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 0, 0);

	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 1, 0);


	buf = 1;
	i2c_write_bytes(i2c_connect_client, _bRW_MISCTL__OSC_CK_SEL, &buf, 1);
}

s32 gup_clk_calibration(void)
{
	u8 buf;

	s32 i;
	struct timeval start, end;
	s32 count;
	s32 count_ref;
	s32 sec;
	s32 usec;

	struct goodix_ts_data *ts;

	buf = 0x0C;
	i2c_write_bytes(i2c_connect_client, _rRW_MISCTL__SWRST_B0_, &buf, 1);
	gup_bit_write(_rRW_MISCTL_RG_DMY83, 7, 0);
	gup_bit_write(_rRW_MISCTL__GIO1CTL_B2_, 0, 0);
	gup_bit_write(_rRW_MISCTL__GIO1CTL_B1_, 1, 0);

	GTP_INFO("CLK calibration GO");
	gup_sys_clk_init();
	gup_clk_calibration_pin_select(1);

	GTP_GPIO_OUTPUT(gtp_int_gpio, 0);

	ts = i2c_get_clientdata(i2c_connect_client);

	for (i = INIT_CLK_DAC; i < MAX_CLK_DAC; i++) {
		GTP_INFO("CLK calibration DAC %d", i);

		if (ts->gtp_is_suspend) {
			i = 72;
			break;
		}

		gup_clk_dac_setting(i);
		gup_clk_count_init(1, CLK_AVG_TIME);

		GTP_GPIO_OUTPUT(gtp_int_gpio, 0);


		do_gettimeofday(&start);
		GTP_GPIO_OUTPUT(gtp_int_gpio, 1);


		msleep(1);
		GTP_GPIO_OUTPUT(gtp_int_gpio, 0);
		msleep(1);


		do_gettimeofday(&end);
		GTP_GPIO_OUTPUT(gtp_int_gpio, 1);


		count = gup_clk_count_get();
		msleep(20);
		GTP_GPIO_OUTPUT(gtp_int_gpio, 0);

		usec = end.tv_usec - start.tv_usec;
		sec = end.tv_sec - start.tv_sec;
		count_ref = 60 * (usec + sec * MILLION);

		GTP_DEBUG("== time %d, %d, %d", sec, usec, count_ref);

		if (count > count_ref) {
			GTP_DEBUG("== count_diff %d", count - count_ref);
			break;
		}
	}

	gtp_reset_guitar(i2c_connect_client, 20);

	GTP_GPIO_AS_INT(gtp_int_gpio);
	return i;
}

s32 gup_hold_ss51_dsp(struct i2c_client *client)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 rd_buf[3];

	while (retry++ < 200) {

		ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}


		ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		if (0x0C == rd_buf[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("[enter_update_mode]Hold ss51 & dsp confirm SUCCESS");
			break;
		}
		GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d", rd_buf[GTP_ADDR_LENGTH]);
	}
	if (retry >= 200) {
		GTP_ERROR("Enter update Hold ss51 failed.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, 0x4010, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]DSP_CK and DSP_ALU_CK PowerOn fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]disable wdt fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]clear cache enable fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]set boot from sram fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);
	if (ret <= 0) {
	    GTP_ERROR("[enter_update_mode]software reboot fail.");
	    return FAIL;
	}

	return SUCCESS;
}

s32 gup_enter_update_mode_fl(struct i2c_client *client)
{
	s32 ret = -1;

	GTP_GPIO_OUTPUT(gtp_rst_gpio, 0);
	msleep(2);

	GTP_GPIO_OUTPUT(gtp_int_gpio, (client->addr == 0x14));
	msleep(2);

	GTP_GPIO_OUTPUT(gtp_rst_gpio, 1);

	msleep(5);

	ret = gup_hold_ss51_dsp(client);
	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]hold ss51 & dsp failed.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]clear control flag fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]set scramble fail.");
		return FAIL;
	}


	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]enable accessing code fail.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_download_fw_dsp(struct i2c_client *client, u8 dwn_mode)
{
	s32 ret = 0;


	GTP_DEBUG("[download_fw_dsp]step1:select bank2");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);
	if (ret == FAIL) {
		GTP_ERROR("select bank 2 fail");
		return FAIL;
	}

	if (GTP_FL_FW_BURN == dwn_mode) {
		GTP_INFO("[download_fw_dsp]Begin download dsp fw---->>");

		if (ret <= 0) {
			GTP_ERROR("[download_fw_dsp]select bank2 fail.");
			return FAIL;
		}
		GTP_DEBUG("burn fw dsp");
		ret = gup_burn_fw_proc(client, 0xC000, 2 * FW_DOWNLOAD_LENGTH, FW_DSP_LENGTH);
		if (FAIL == ret) {
			GTP_ERROR("[download_fw_dsp]download FW dsp fail.");
			return FAIL;
		}
		GTP_INFO("check firmware dsp");
		ret = gup_check_and_repair(client, 0xC000, 2 * FW_DOWNLOAD_LENGTH, FW_DSP_LENGTH);
		if (FAIL == ret) {
			GTP_ERROR("check fw dsp failed!");
			return FAIL;
		}
	} else if (GTP_FL_ESD_RECOVERY == dwn_mode) {
		GTP_INFO("[download_fw_dsp]Begin esd check dsp fw---->>");




		{

			GTP_INFO("esd recovery redownload firmware dsp code");
			ret = gup_burn_fw_proc(client, 0xC000, 2 * FW_DOWNLOAD_LENGTH, FW_DSP_LENGTH);
			if (FAIL == ret) {
				GTP_ERROR("redownload fw dsp failed!");
				return FAIL;
			}
		}
	} else {
		GTP_INFO("check firmware dsp");
		ret = gup_check_and_repair(client, 0xC000, 2 * FW_DOWNLOAD_LENGTH, FW_DSP_LENGTH);
		if (FAIL == ret) {
			GTP_ERROR("check fw dsp failed!");
			return FAIL;
		}
	}
	return SUCCESS;
}

static s32 gup_burn_fw_proc(struct i2c_client *client, u16 start_addr, s32 start_index, s32 burn_len)
{
	s32 ret = 0;
	GTP_DEBUG("burn firmware: 0x%04X, %d bytes, start_index: 0x%04X", start_addr, burn_len, start_index);

	ret = i2c_write_bytes(client, start_addr, (u8*)&gt915_FW_fl[FW_HEAD_LENGTH + start_index], burn_len);
	if (ret < 0) {
		GTP_ERROR("burn 0x%04X, %d bytes failed!", start_addr, burn_len);
		return FAIL;
	}
	return SUCCESS;
}

static s32 gup_check_and_repair(struct i2c_client *client, u16 start_addr, s32 start_index, s32 chk_len)
{
	s32 ret = 0;
	s32 cmp_len = 0;
	u16 cmp_addr = start_addr;
	s32 i = 0;
	s32 chked_times = 0;
	u8 chk_fail = 0;

	GTP_DEBUG("check firmware: start 0x%04X, %d bytes", start_addr, chk_len);
	while ((chk_len > 0) && (chked_times < GTP_CHK_FW_MAX)) {
		if (chk_len >= GUP_FW_CHK_SIZE) {
			cmp_len = GUP_FW_CHK_SIZE;
		} else {
			cmp_len = chk_len;
		}
		ret = i2c_read_bytes(client, cmp_addr, chk_cmp_buf, cmp_len);
		if (ret < 0) {
			chk_fail = 1;
			break;
		}
		for (i = 0; i < cmp_len; ++i) {
		if (chk_cmp_buf[i] != gt915_FW_fl[FW_HEAD_LENGTH + start_index + i]) {
				chk_fail = 1;
			i2c_write_bytes(client, cmp_addr+i, &gt915_FW_fl[FW_HEAD_LENGTH + start_index + i], cmp_len-i);
				GTP_ERROR("Check failed index: %d(%d != %d), redownload chuck", i, chk_cmp_buf[i],
					gt915_FW_fl[FW_HEAD_LENGTH + start_index +i]);
				break;
			}
		}
		if (chk_fail == 1) {
			chk_fail = 0;
			chked_times++;
		} else {
			cmp_addr += cmp_len;
			start_index += cmp_len;
			chk_len -= cmp_len;
		}
	}
	if (chk_len > 0) {
		GTP_ERROR("cmp_addr: 0x%04X, start_index: 0x%02X, chk_len: 0x%04X", cmp_addr,
				start_index, chk_len);
		return FAIL;
	}
	return SUCCESS;
}

static u8 gup_download_fw_ss51(struct i2c_client *client, u8 dwn_mode)
{
	s32 section = 0;
	s32 ret = 0;
	s32 start_index = 0;
	u8  bank = 0;
	u16 burn_addr = 0xC000;

	if (GTP_FL_FW_BURN == dwn_mode) {
		GTP_INFO("download firmware ss51");
	} else {
		GTP_INFO("check firmware ss51");
	}
	for (section = 1; section <= 4; section += 2) {
		switch (section) {
		case 1:
			bank = 0x00;
			burn_addr = (section - 1) * FW_SS51_SECTION_LEN + 0xC000;
			break;
		case 3:
			bank = 0x01;
			burn_addr = (section - 3) * FW_SS51_SECTION_LEN + 0xC000;
			break;
		}
		start_index = (section - 1) * FW_SS51_SECTION_LEN;

		GTP_DEBUG("download firmware ss51: select bank%d", bank);
		ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, bank);
		if (GTP_FL_FW_BURN == dwn_mode) {
			GTP_DEBUG("download firmware ss51 section%d & %d", section, section+1);
			ret = gup_burn_fw_proc(client, burn_addr, start_index, 2 * FW_SS51_SECTION_LEN);
			if (ret == FAIL) {
				GTP_ERROR("download fw ss51 section%d & %d failed!", section, section+1);
				return FAIL;
			}
			GTP_DEBUG("check firmware ss51 section%d & %d", section, section+1);
			ret = gup_check_and_repair(client, burn_addr, start_index, 2 * FW_SS51_SECTION_LEN);
			if (ret == FAIL) {
				GTP_ERROR("check ss51 section%d & %d failed!", section, section+1);
				return FAIL;
			}
		} else if (GTP_FL_ESD_RECOVERY == dwn_mode) {



			{

				GTP_DEBUG("esd recovery redownload ss51 section%d & %d", section, section+1);
				ret = gup_burn_fw_proc(client, burn_addr, start_index, 2 * FW_SS51_SECTION_LEN);
				if (ret == FAIL) {
					GTP_ERROR("download fw ss51 section%d failed!", section);
					return FAIL;
				}
			}
		} else {
			GTP_DEBUG("check firmware ss51 section%d & %d", section, section+1);
			ret = gup_check_and_repair(client, burn_addr, start_index, 2 * FW_SS51_SECTION_LEN);
			if (ret == FAIL) {
				GTP_ERROR("check ss51 section%d & %d failed!", section, section+1);
				return FAIL;
			}
		}
	}

	return SUCCESS;
}


static s32 gup_prepare_fl_fw(char *path, st_fw_head *fw_head)
{
	s32 ret = 0;
	s32 i = 0;
	s32 timeout = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	if (!memcmp(path, "update", 6)) {
		GTP_INFO("Search for GT9XXF firmware file to update");

		searching_file = 1;
		for (i = 0; i < GUP_SEARCH_FILE_TIMES; ++i) {
			if (0 == searching_file) {
				GTP_INFO("Force terminate auto update for GT9XXF...");
				return FAIL;
			}
			GTP_DEBUG("Search for %s, %s for fw update.(%d/%d)", FL_UPDATE_PATH, FL_UPDATE_PATH_SD, i+1, GUP_SEARCH_FILE_TIMES);
			update_msg.file = filp_open(FL_UPDATE_PATH, O_RDONLY, 0);
			if (IS_ERR(update_msg.file)) {
				update_msg.file = filp_open(FL_UPDATE_PATH_SD, O_RDONLY, 0);
				if (IS_ERR(update_msg.file)) {
					msleep(3000);
					continue;
				} else {
					path = FL_UPDATE_PATH_SD;
					break;
				}
			} else {
				path = FL_UPDATE_PATH;
				break;
			}
		}
		searching_file = 0;
		if (i == 50) {
			GTP_INFO("Search timeout, update aborted");
			return FAIL;
		} else {
			GTP_INFO("GT9XXF firmware file %s found!", path);
			_CLOSE_FILE(update_msg.file);
		}
		while (ts->rqst_processing && (timeout++ < 5)) {
			GTP_DEBUG("request processing, waiting for accomplishment");
			msleep(1000);
		}
	}
	GTP_INFO("Firmware update file path: %s", path);

	update_msg.file = filp_open(path, O_RDONLY, 0);

	if (IS_ERR(update_msg.file)) {
		GTP_ERROR("Open update file(%s) error!", path);
		return FAIL;
	}

	update_msg.old_fs = get_fs();
	set_fs(KERNEL_DS);

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	update_msg.fw_total_len = update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_END);

	update_msg.force_update = 0xBE;
	if (update_msg.fw_total_len != sizeof(gt915_FW_fl)) {
		GTP_ERROR("Inconsistent fw size. default size: %d(%dK), file size: %d(%dK)", sizeof(gt915_FW_fl), sizeof(gt915_FW_fl)/1024, update_msg.fw_total_len, update_msg.fw_total_len/1024);
		set_fs(update_msg.old_fs);
		_CLOSE_FILE(update_msg.file);
		return FAIL;
	}

	update_msg.fw_total_len -= FW_HEAD_LENGTH;
	GTP_DEBUG("Fimrware size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len / 1024);

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	ret = update_msg.file->f_op->read(update_msg.file, (char*)gt915_FW_fl,
					update_msg.fw_total_len + FW_HEAD_LENGTH,
					&update_msg.file->f_pos);
	update_msg.fw_total_len  += FW_HEAD_LENGTH;
	set_fs(update_msg.old_fs);
	_CLOSE_FILE(update_msg.file);

	if (ret < 0) {
		GTP_ERROR("read %s failed, err-code: %d", path, ret);
		return FAIL;
	}
	return SUCCESS;
}
static u8 gup_check_update_file_fl(struct i2c_client  *client, st_fw_head *fw_head, char *path)
{
	s32 ret = 0;
	s32 i = 0;
	s32 fw_checksum = 0;

	if (NULL != path) {
		ret = gup_prepare_fl_fw(path, fw_head);
		if (FAIL == ret) {
			return FAIL;
		}
	} else {
		update_msg.fw_total_len = sizeof(gt915_FW_fl);
	}

	memcpy(fw_head, gt915_FW_fl, FW_HEAD_LENGTH);
	GTP_INFO("FILE HARDWARE INFO: %02x%02x%02x%02x", fw_head->hw_info[0], fw_head->hw_info[1], fw_head->hw_info[2], fw_head->hw_info[3]);
	GTP_INFO("FILE PID: %s", fw_head->pid);
	fw_head->vid = ((fw_head->vid & 0xFF00) >> 8) + ((fw_head->vid & 0x00FF) << 8);
	GTP_INFO("FILE VID: %04x", fw_head->vid);

	fw_checksum = 0;
	for(i = FW_HEAD_LENGTH; i < update_msg.fw_total_len; i += 2) {
		fw_checksum += (gt915_FW_fl[i] << 8) + gt915_FW_fl[i+1];
	}
	ret = SUCCESS;

	GTP_DEBUG("firmware checksum: %x", fw_checksum&0xFFFF);
	if (fw_checksum & 0xFFFF) {
		GTP_ERROR("Illegal firmware file.");
		ret = FAIL;
	}

	return ret;
}

s32 gup_fw_download_proc(void *dir, u8 dwn_mode)
{
	s32 ret = 0;
	u8  retry = 0;
	st_fw_head fw_head;
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(i2c_connect_client);
	if (NULL == dir) {
		if (GTP_FL_FW_BURN == dwn_mode) {
			GTP_INFO("[fw_download_proc]Begin fw download ......");
		} else if (GTP_FL_ESD_RECOVERY == dwn_mode) {
			GTP_INFO("[fw_download_proc]Begin fw esd recovery check ......");
		} else {
			GTP_INFO("[fw_download_proc]Being fw repair check......");
		}
	} else {
		GTP_INFO("[fw_download_proc]Begin firmware update by bin file");
	}

	total_len = 100;
	show_len = 0;

	ret = gup_check_update_file_fl(i2c_connect_client, &fw_head, (char *)dir);
	show_len = 10;

	if (FAIL == ret) {
		GTP_ERROR("[fw_download_proc]check update file fail.");
		goto file_fail;
	}

	if (!memcmp(fw_head.pid, "950", 3)) {
		ts->is_950 = 1;
		GTP_DEBUG("GT9XXF Ic Type: gt950");
	} else {
		ts->is_950 = 0;
	}

	if (NULL != dir) {
		gtp_irq_disable(ts);
#if GTP_ESD_PROTECT
		gtp_esd_switch(ts->client, SWITCH_OFF);
#endif
	}

	ret = gup_enter_update_mode_fl(i2c_connect_client);
	show_len = 20;
	if (FAIL == ret) {
		GTP_ERROR("[fw_download_proc]enter update mode fail.");
		goto download_fail;
	}

	while (retry++ < 5) {
		ret = gup_download_fw_ss51(i2c_connect_client, dwn_mode);
		show_len = 60;
		if (FAIL == ret) {
			GTP_ERROR("[fw_download_proc]burn ss51 firmware fail.");
			continue;
		}

		ret = gup_download_fw_dsp(i2c_connect_client, dwn_mode);
		show_len = 80;
		if (FAIL == ret) {
			GTP_ERROR("[fw_download_proc]burn dsp firmware fail.");
			continue;
		}

		GTP_INFO("[fw_download_proc]UPDATE SUCCESS.");
		break;
	}

	if (retry >= 5) {
		GTP_ERROR("[fw_download_proc]retry timeout,UPDATE FAIL.");
		goto download_fail;
	}

	if (NULL != dir) {
		gtp_irq_enable(ts);
		gtp_fw_startup(ts->client);
	#if GTP_ESD_PROTECT
		gtp_esd_switch(ts->client, SWITCH_ON);
	#endif
	}
	show_len = 100;
	return SUCCESS;

download_fail:
	if (NULL != dir) {
		gtp_irq_enable(ts);
		gtp_fw_startup(ts->client);
	#if GTP_ESD_PROTECT
		gtp_esd_switch(ts->client, SWITCH_ON);
	#endif
	}
file_fail:
	show_len = 200;

	return FAIL;
}

#endif
