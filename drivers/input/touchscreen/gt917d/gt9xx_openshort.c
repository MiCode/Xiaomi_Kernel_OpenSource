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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "gt9xx_openshort.h"

#define _BEYOND_INFO_MAX			20  /*open test max show 20 infos for each test item*/
#define GTP_OPEN_SAMPLE_NUM		 1  /*open test raw data sampled count*/
#define GTP_TEST_INFO_MAX		   200 /*test info lines max count*/

/****************** Customer Config End ***********************/

extern s32 gtp_i2c_read(struct i2c_client*, u8*, s32);
extern s32 gtp_i2c_write(struct i2c_client*, u8*, s32);
extern void gtp_reset_guitar(struct i2c_client*, s32);

extern struct i2c_client *i2c_connect_client;

u8  gt9xx_drv_num = MAX_DRIVER_NUM; /*default driver and sensor number*/
u8  gt9xx_sen_num = MAX_SENSOR_NUM;
u16 gt9xx_pixel_cnt = MAX_DRIVER_NUM * MAX_SENSOR_NUM;
u16 gt9xx_sc_pxl_cnt = MAX_DRIVER_NUM * MAX_SENSOR_NUM;
struct gt9xx_short_info *short_sum;

u8 chip_type_gt9f = 0;
u8 have_key = 0;
u8 gt9xx_sc_drv_num;
u8 key_is_isolated; /* 0: no, 1: yes*/
u8 key_iso_pos[5];
static u8 raw_data[MAX_DRIVER_NUM*MAX_SENSOR_NUM*2+20];
static u8 diff_data[MAX_DRIVER_NUM*MAX_SENSOR_NUM*2+20];
static u8 refraw_data[MAX_DRIVER_NUM*MAX_SENSOR_NUM*2+20];

struct kobject *goodix_debug_kobj;
/* static u8  rslt_buf_idx = 0;*/
static s32 *test_rslt_buf;
static struct gt9xx_open_info *touchpad_sum;

#define _MIN_ERROR_NUM	  (GTP_OPEN_SAMPLE_NUM * 9 / 10)


static s32 gtp_i2c_end_cmd(struct i2c_client *client)
{
	u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	s32 ret = 0;

	ret = gtp_i2c_write(client, end_cmd, 3);
	if (ret < 0) {
		GTP_DEBUG("I2C write end_cmd  error!");
	}
	return ret;
}

s32 gtp_parse_config(struct i2c_client *client)
{
	u8 i = 0;
	u8 key_pos = 0;
	u8 key_val = 0;
	u8 config[256] = {(u8)(GTP_REG_CONFIG_DATA >> 8), (u8)GTP_REG_CONFIG_DATA, 0};
	u8 type_buf[12] = {0x80, 0x00};

	if (gtp_i2c_read(client, config, GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH) <= 0) {
		GTP_DEBUG("Failed to read config!");
		return FAIL;
	}

	gt9xx_drv_num = (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT - GT9_REG_CFG_BEG] & 0x1F)
					+ (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT + 1 - GT9_REG_CFG_BEG] & 0x1F);
	gt9xx_sen_num = (config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT + 2 - GT9_REG_CFG_BEG] & 0x0F)
					+ ((config[GTP_ADDR_LENGTH + GT9_REG_SEN_DRV_CNT + 2 - GT9_REG_CFG_BEG] >> 4) & 0x0F);

	GTP_DEBUG("Driver num: %d, Sensor Num: %d", gt9xx_drv_num, gt9xx_sen_num);
	if (gt9xx_drv_num < MIN_DRIVER_NUM || gt9xx_drv_num > MAX_DRIVER_NUM) {
		GTP_DEBUG("driver number error!");
		return FAIL;
	}
	if (gt9xx_sen_num < MIN_SENSOR_NUM || gt9xx_sen_num > MAX_SENSOR_NUM) {
		GTP_DEBUG("sensor number error!");
		return FAIL;
	}
	gt9xx_sc_pxl_cnt = gt9xx_pixel_cnt = gt9xx_drv_num * gt9xx_sen_num;

	gtp_i2c_read(client, type_buf, 12);
	if (!memcmp(&type_buf[2], "GOODIX_GT9", 10)) {
		chip_type_gt9f = 0;
		GTP_DEBUG("Chip type: GT9XX");
	} else {
		chip_type_gt9f = 1;
		GTP_DEBUG("Chip type: GT9XXF");
	}

	have_key = config[0x804E - GT9_REG_CFG_BEG + GTP_ADDR_LENGTH] & 0x01;

	if (!have_key) {
		GTP_DEBUG("No key");
		return SUCCESS;
	}
	GTP_DEBUG("Have Key");
	gt9xx_sc_drv_num = gt9xx_drv_num - 1;

	for (i = 0; i < 5; ++i) {
		key_iso_pos[i] = 0;
	}

	key_is_isolated = 0;
	for (i = 0; i < 4; ++i) {
		/*all keys are multiples of 0x08 -> isolated keys*/
		key_val = config[GTP_ADDR_LENGTH + GT9_REG_KEY_VAL - GT9_REG_CFG_BEG + i];
		key_pos = key_val%0x08;
		GTP_DEBUG("key_val[%d] = 0x%02x", i+1, key_val);
		if ((key_pos != 0)) {
			key_is_isolated = 0;
			GTP_DEBUG("Key is not isolated!");
			break;
		} else if (key_val == 0x00) {
			 /*no more key*/
			continue;
		} else {
			key_iso_pos[0]++;	   /*isolated key count*/
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

u32 endian_mode(void)
{
	union {s32 i; s8 c; }
	endian;

	endian.i = 1;

	if (1 == endian.c) {
		return MYBIG_ENDIAN;
	} else {
		return MYLITLE_ENDIAN;
	}
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
		GTP_DEBUG("i2c write failed.");
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
		GTP_DEBUG("i2c write coor cmd failed!");
		return FAIL;
	}
	msleep(10);
	return SUCCESS;
}


s32 gtp_read_diffdata(struct i2c_client *client, u16 *data)
{
	s32 ret = -1;
	u16 retry = 0;
	u8 read_state[3] = {(u8)(GTP_REG_RAW_READY>>8), (u8)GTP_REG_RAW_READY, 0};
	u16 i = 0, j = 0;
	u8 *read_diffbuf;
	u8 tail, head;
	u8 temp[20];

	read_diffbuf = (u8 *)kmalloc(sizeof(u8) * (gt9xx_drv_num * gt9xx_sen_num * 2 + GTP_ADDR_LENGTH), GFP_KERNEL);

	if (NULL == read_diffbuf) {
		GTP_DEBUG("failed to allocate for read diffbuf");
		return FAIL;
	}

	if (data == NULL) {
		GTP_DEBUG("Invalid raw buffer.");
		goto have_error;
	}

	msleep(10);
	while (retry++ < GTP_WAIT_RAW_MAX_TIMES) {
		ret = gtp_i2c_read(client, read_state, 3);
		if (ret <= 0) {
			GTP_DEBUG("i2c read failed.return: %d", ret);
			continue;
		}
		if ((read_state[GTP_ADDR_LENGTH] & 0x80) == 0x80) {
			GTP_DEBUG("Raw data is ready.");
			break;
		}
		if ((retry%20) == 0) {
			GTP_DEBUG("(%d)read_state[2] = 0x%02X", retry, read_state[GTP_ADDR_LENGTH]);
			if (retry == 100) {
				gt9_read_raw_cmd(client);
			}
		}
		msleep(5);
	}
	if (retry >= GTP_WAIT_RAW_MAX_TIMES) {
		GTP_DEBUG("Wait raw data ready timeout.");
		goto have_error;
	}

	if (chip_type_gt9f) {
		read_diffbuf[0] = (u8)(GTP_REG_RAW_DATA_GT9F >> 8);
		read_diffbuf[1] = (u8)(GTP_REG_RAW_DATA_GT9F);
	} else {
		read_diffbuf[0] = (u8)(GTP_REG_DIFF_DATA >> 8);
		read_diffbuf[1] = (u8)(GTP_REG_DIFF_DATA);
	}

	ret = gtp_i2c_read(client, read_diffbuf, GTP_ADDR_LENGTH + ((gt9xx_drv_num*gt9xx_sen_num)*2));
	if (ret <= 0) {
		GTP_DEBUG("i2c read rawdata failed.");
		goto have_error;
	}
	gtp_i2c_end_cmd(client);	/* clear buffer state*/

	if (endian_mode() == MYBIG_ENDIAN) {
		head = 0;
		tail = 1;
		GTP_DEBUG("Big Endian.");
	} else {
		head = 1;
		tail = 0;
		GTP_DEBUG("Little Endian.");
	}


	GTP_DEBUG("---read diff data-----\n");

	sprintf(temp, "tx: %d\n", gt9xx_drv_num);
	sprintf(diff_data, temp);
	sprintf(temp, "rx: %d\n", gt9xx_sen_num);
	strcat(diff_data, temp);

	for (i = 0, j = 0; i < ((gt9xx_drv_num*gt9xx_sen_num) * 2); i += 2) {
		data[i/2] = (u16)(read_diffbuf[i+head+GTP_ADDR_LENGTH]<<8) + (u16)read_diffbuf[GTP_ADDR_LENGTH+i+tail];
		sprintf(temp, "%-5d ", (short)data[i/2]);
		strcat(diff_data, temp);
		++j;
		if ((j%28) == 0) {
			sprintf(temp, "\n");
			strcat(diff_data, temp);
		}
	}

	kfree(read_diffbuf);
	return SUCCESS;
have_error:
	kfree(read_diffbuf);
	return FAIL;
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
	u8 temp[20];

	read_rawbuf = (u8 *)kmalloc(sizeof(u8) * (gt9xx_drv_num*gt9xx_sen_num * 2 + GTP_ADDR_LENGTH), GFP_KERNEL);

	if (NULL == read_rawbuf) {
		GTP_DEBUG("failed to allocate for read_rawbuf");
		return FAIL;
	}

	if (data == NULL) {
		GTP_DEBUG("Invalid raw buffer.");
		goto have_error;
	}

	msleep(10);
	while (retry++ < GTP_WAIT_RAW_MAX_TIMES) {
		ret = gtp_i2c_read(client, read_state, 3);
		if (ret <= 0) {
			GTP_DEBUG("i2c read failed.return: %d", ret);
			continue;
		}
		if ((read_state[GTP_ADDR_LENGTH] & 0x80) == 0x80) {
			GTP_DEBUG("Raw data is ready.");
			break;
		}
		if ((retry%20) == 0) {
			GTP_DEBUG("(%d)read_state[2] = 0x%02X", retry, read_state[GTP_ADDR_LENGTH]);
			if (retry == 100) {
				gt9_read_raw_cmd(client);
			}
		}
		msleep(5);
	}
	if (retry >= GTP_WAIT_RAW_MAX_TIMES) {
		GTP_DEBUG("Wait raw data ready timeout.");
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
		GTP_DEBUG("i2c read rawdata failed.");
		goto have_error;
	}
	gtp_i2c_end_cmd(client);	/*clear buffer state*/

	if (endian_mode() == MYBIG_ENDIAN) {
		head = 0;
		tail = 1;
		GTP_DEBUG("Big Endian.");
	} else {
		head = 1;
		tail = 0;
		GTP_DEBUG("Little Endian.");
	}

	GTP_DEBUG("---read raw data-----\n");

	sprintf(temp, "tx: %d\n", gt9xx_drv_num);
	sprintf(raw_data, temp);
	sprintf(temp, "rx: %d\n", gt9xx_sen_num);
	strcat(raw_data, temp);

	for (i = 0, j = 0; i < ((gt9xx_drv_num*gt9xx_sen_num) * 2); i += 2) {
		data[i/2] = (u16)(read_rawbuf[i+head+GTP_ADDR_LENGTH]<<8) + (u16)read_rawbuf[GTP_ADDR_LENGTH+i+tail];
	#if 1
		sprintf(temp, "%-5d ", data[i/2]);
		strcat(raw_data, temp);
		++j;
		if ((j%28) == 0) {
			sprintf(temp, "\n");
			strcat(raw_data, temp);
		}
	#endif
	}

	kfree(read_rawbuf);
	return SUCCESS;
have_error:
	kfree(read_rawbuf);
	return FAIL;
}

s32 gtp_read_refrawdata(struct i2c_client *client, u16 *data)
{
	s32 ret = -1;
	u16 retry = 0;
	u8 read_state[3] = {(u8)(GTP_REG_RAW_READY>>8), (u8)GTP_REG_RAW_READY, 0};
	u16 i = 0, j = 0;
	u8 *read_rawbuf;
	u8 tail, head;
	u8 temp[20];

	read_rawbuf = (u8 *)kmalloc(sizeof(u8) * (gt9xx_drv_num*gt9xx_sen_num * 2 + GTP_ADDR_LENGTH), GFP_KERNEL);

	if (NULL == read_rawbuf) {
		GTP_DEBUG("failed to allocate for read_rawbuf");
		return FAIL;
	}

	if (data == NULL) {
		GTP_DEBUG("Invalid raw buffer.");
		goto have_error;
	}

	msleep(10);
	while (retry++ < GTP_WAIT_RAW_MAX_TIMES) {
		ret = gtp_i2c_read(client, read_state, 3);
		if (ret <= 0) {
			GTP_DEBUG("i2c read failed.return: %d", ret);
			continue;
		}
		if ((read_state[GTP_ADDR_LENGTH] & 0x80) == 0x80) {
			GTP_DEBUG("Raw data is ready.");
			break;
		}
		if ((retry%20) == 0) {
			GTP_DEBUG("(%d)read_state[2] = 0x%02X", retry, read_state[GTP_ADDR_LENGTH]);
			if (retry == 100) {
				gt9_read_raw_cmd(client);
			}
		}
		msleep(5);
	}
	if (retry >= GTP_WAIT_RAW_MAX_TIMES) {
		GTP_DEBUG("Wait raw data ready timeout.");
		goto have_error;
	}

	if (chip_type_gt9f) {
		read_rawbuf[0] = (u8)(GTP_REG_RAW_DATA_GT9F >> 8);
		read_rawbuf[1] = (u8)(GTP_REG_RAW_DATA_GT9F);
	} else {
		read_rawbuf[0] = (u8)(GTP_REG_REFRAW_DATA >> 8);
		read_rawbuf[1] = (u8)(GTP_REG_REFRAW_DATA);
	}

	ret = gtp_i2c_read(client, read_rawbuf, GTP_ADDR_LENGTH + ((gt9xx_drv_num*gt9xx_sen_num) * 2));
	GTP_DEBUG("%x-%x:\n", read_rawbuf[i], read_rawbuf[i+1]);
	for (i = 0, j = 0; i < ((gt9xx_drv_num*gt9xx_sen_num) * 2); i += 2) {
		GTP_DEBUG("%d-%d   ", read_rawbuf[i+2], read_rawbuf[i+3]);
		++j;
		if ((j%28) == 0)
			GTP_DEBUG("\n");
	}
	if (ret <= 0) {
		GTP_DEBUG("i2c read rawdata failed.");
		goto have_error;
	}
	gtp_i2c_end_cmd(client);	/* clear buffer state*/

	if (endian_mode() == MYBIG_ENDIAN) {
		head = 0;
		tail = 1;
		GTP_DEBUG("Big Endian.");
	} else {
		head = 1;
		tail = 0;
		GTP_DEBUG("Little Endian.");
	}

	GTP_DEBUG("---read refraw data-----\n");

	sprintf(temp, "tx: %d\n", gt9xx_drv_num);
	sprintf(refraw_data, temp);
	sprintf(temp, "rx: %d\n", gt9xx_sen_num);
	strcat(refraw_data, temp);

	for (i = 0, j = 0; i < ((gt9xx_drv_num * gt9xx_sen_num) * 2); i += 2) {
		data[i/2] = (u16)(read_rawbuf[i + head + GTP_ADDR_LENGTH] << 8) + (u16)read_rawbuf[GTP_ADDR_LENGTH + i + tail];
	#if 1
		sprintf(temp, "%-5d ", data[i/2]);
		strcat(refraw_data, temp);
		++j;
		if ((j%28) == 0) {
			sprintf(temp, "\n");
			strcat(refraw_data, temp);
		}
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

	test_rslt_buf = (s32 *)kmalloc(sizeof(s32) * GTP_OPEN_SAMPLE_NUM, GFP_ATOMIC);
	touchpad_sum = (struct gt9xx_open_info *) kmalloc(sizeof(struct gt9xx_open_info) * (4 * _BEYOND_INFO_MAX + 1), GFP_ATOMIC);
	if (NULL == test_rslt_buf || NULL == touchpad_sum) {
		return FAIL;
	}
	memset(touchpad_sum, 0, sizeof(struct gt9xx_open_info) * (4 * _BEYOND_INFO_MAX + 1));

	for (i = 0; i < (4 * _BEYOND_INFO_MAX); ++i) {
		touchpad_sum[i].driver = 0xFF;
	}

	for (i = 0; i < GTP_OPEN_SAMPLE_NUM; i++) {
		test_rslt_buf[i] = _CHANNEL_PASS;
	}
	return SUCCESS;
}

/**
 * ============================
 * @Author:   HQ-zmc
 * @Version:  1.0
 * @DateTime: 2018-01-31 19:44:25
 * @input:
 * @output:
 * ============================
 */
/*Modifiy by HQ-zmc [Date: 2018-04-10 11:46:05]*/
#define HQ_GTP_TP_DATA_DUMP "hq_tp_data_dump"
static struct proc_dir_entry *hq_gtp_data_dump_proc;

static int hq_tp_data_dump_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", diff_data);
	seq_printf(m, "%s\n", raw_data);
	seq_printf(m, "%s\n", refraw_data);

	return 0;
}

static int hq_tp_data_dump_proc_open(struct inode *inode, struct file *file)
{
	s32 ret = FAIL; /* SUCCESS, FAIL*/
	struct goodix_ts_data *ts;
	u16 *raw_buf = NULL;
	struct i2c_client *client = i2c_connect_client;

	/* seq_printf(m, "--------test------\n");*/

	ts = i2c_get_clientdata(client);

	GTP_DEBUG("---gtp open test---");
	GTP_DEBUG("Parsing configuration...");
	/* gtp_hopping_switch(client, 0);*/
	ret = gtp_parse_config(client);
	if (ret == FAIL) {
		GTP_DEBUG("failed to parse config...");
		ret = FAIL;
		goto open_test_exit;
	}
	raw_buf = (u16 *)kmalloc(sizeof(u16) * (gt9xx_drv_num * gt9xx_sen_num), GFP_KERNEL);
	if (NULL == raw_buf) {
		GTP_DEBUG("failed to allocate mem for raw_buf!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("\nStep 1: Send Rawdata Cmd");

	ret = gtp_raw_test_init();
	if (FAIL == ret) {
		GTP_DEBUG("Allocate memory for open test failed!");
		ret = FAIL;
		goto open_test_exit;
	}
	ret = gt9_read_raw_cmd(client);
	if (ret == FAIL) {
		GTP_DEBUG("Send Read Rawdata Cmd failed!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("Step 2: Sample Diffdata");

	ret = gtp_read_diffdata(client, raw_buf);
	if (ret == FAIL) {
		GTP_DEBUG("Read Diffdata failed!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("Step 3: Sample Rawdata");
	ret = gtp_read_rawdata(client, raw_buf);
	if (ret == FAIL) {
		GTP_DEBUG("Read Rawdata failed!");
		ret = FAIL;
		goto open_test_exit;
	}


	GTP_DEBUG("Step 4: Sample RefRawdata");
	ret = gtp_read_refrawdata(client, raw_buf);
	if (ret == FAIL) {
		GTP_DEBUG("Read RefRawdata failed!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("Step 5: Analyse Result");
	GTP_DEBUG("Total %d Sample Data, Max Show %d Info for each Test Item", GTP_OPEN_SAMPLE_NUM, _BEYOND_INFO_MAX);
open_test_exit:
	if (raw_buf) {
		kfree(raw_buf);
	}
	if (test_rslt_buf) {
		kfree(test_rslt_buf);
		test_rslt_buf = NULL;
	}
	if (touchpad_sum) {
		kfree(touchpad_sum);
		touchpad_sum = NULL;
	}

	gt9_read_coor_cmd(client);  /*back to read coordinates data */
	GTP_DEBUG("---gtp open test end---");

	return single_open(file, hq_tp_data_dump_proc_show, NULL);
}

static const struct file_operations hq_tp_data_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = hq_tp_data_dump_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};



#define GTP_TP_DATA_DUMP "tp_data_dump"
static struct proc_dir_entry *gtp_data_dump_proc;

static int tp_data_dump_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", diff_data);
	seq_printf(m, "%s\n", raw_data);

	return 0;
}

static int tp_data_dump_proc_open(struct inode *inode, struct file *file)
{
	s32 ret = FAIL; /*SUCCESS, FAIL*/
	struct goodix_ts_data *ts;
	short *raw_buf = NULL;
	struct i2c_client *client = i2c_connect_client;

	/*seq_printf(m, "--------test------\n");*/

	ts = i2c_get_clientdata(client);

	GTP_DEBUG("---gtp open test---");
	GTP_DEBUG("Parsing configuration...");
	/* gtp_hopping_switch(client, 0);*/
	ret = gtp_parse_config(client);
	if (ret == FAIL) {
		GTP_DEBUG("failed to parse config...");
		ret = FAIL;
		goto open_test_exit;
	}
	raw_buf = (short *)kmalloc(sizeof(short) * (gt9xx_drv_num * gt9xx_sen_num), GFP_KERNEL);
	if (NULL == raw_buf) {
		GTP_DEBUG("failed to allocate mem for raw_buf!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("\nStep 1: Send Rawdata Cmd");

	ret = gtp_raw_test_init();
	if (FAIL == ret) {
		GTP_DEBUG("Allocate memory for open test failed!");
		ret = FAIL;
		goto open_test_exit;
	}
	ret = gt9_read_raw_cmd(client);
	if (ret == FAIL) {
		GTP_DEBUG("Send Read Rawdata Cmd failed!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("Step 2: Sample Diffdata");

	ret = gtp_read_diffdata(client, raw_buf);
	if (ret == FAIL) {
		GTP_DEBUG("Read Diffdata failed!");
		ret = FAIL;
		goto open_test_exit;
	}

	GTP_DEBUG("Step 3: Sample Rawdata");
	ret = gtp_read_rawdata(client, raw_buf);
	if (ret == FAIL) {
		GTP_DEBUG("Read Rawdata failed!");
		ret = FAIL;
		goto open_test_exit;
	}
	GTP_DEBUG("Step 4: Analyse Result");
	GTP_DEBUG("Total %d Sample Data, Max Show %d Info for each Test Item", GTP_OPEN_SAMPLE_NUM, _BEYOND_INFO_MAX);
open_test_exit:
	if (raw_buf) {
		kfree(raw_buf);
	}
	if (test_rslt_buf) {
		kfree(test_rslt_buf);
		test_rslt_buf = NULL;
	}
	if (touchpad_sum) {
		kfree(touchpad_sum);
		touchpad_sum = NULL;
	}

	gt9_read_coor_cmd(client);  /* back to read coordinates data */
	GTP_DEBUG("---gtp open test end---");

	return single_open(file, tp_data_dump_proc_show, NULL);
}

static const struct file_operations tp_data_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = tp_data_dump_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int create_gtp_data_dump_proc(void)
{
	int ret = 0;

	GTP_DEBUG("%s:ENTER FUNC ---- %d\n", __func__, __LINE__);
	gtp_data_dump_proc = proc_create(GTP_TP_DATA_DUMP, 0444, NULL, &tp_data_dump_proc_fops);
	if (gtp_data_dump_proc == NULL) {
		GTP_DEBUG("fts, create_proc_entry tp_data_dump_proc failed\n");
		ret = -1;
	}

	hq_gtp_data_dump_proc = proc_create(HQ_GTP_TP_DATA_DUMP, 0444, NULL, &hq_tp_data_dump_proc_fops);
	if (hq_gtp_data_dump_proc == NULL) {
		GTP_DEBUG("fts, create_proc_entry hq_gtp_data_dump_proc failed\n");
		ret = -1;
	}
	return ret;
}
