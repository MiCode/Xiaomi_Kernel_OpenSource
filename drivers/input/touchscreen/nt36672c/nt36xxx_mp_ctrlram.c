/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 32206 $
 * $Date: 2018-08-10 19:23:04 +0800 (週五, 10 八月 2018) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/firmware.h>

#include "nt36xxx.h"
#include "nt36xxx_mp_ctrlram.h"

#if NVT_TOUCH_MP

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define MP_MODE_CC 0x41
#define FREQ_HOP_DISABLE 0x66
#define FREQ_HOP_ENABLE 0x65

#define SHORT_TEST_CSV_FILE "/data/misc/tp_selftest_data/ShortTest.csv"
#define OPEN_TEST_CSV_FILE "/data/misc/tp_selftest_data/OpenTest.csv"
#define FW_RAWDATA_CSV_FILE "/data/misc/tp_selftest_data/FWMutualTest.csv"
#define FW_CC_CSV_FILE "/data/misc/tp_selftest_data/FWCCTest.csv"
#define NOISE_TEST_CSV_FILE "/data/misc/tp_selftest_data/NoiseTest.csv"

#define nvt_mp_seq_printf(m, fmt, args...) do {	\
	seq_printf(m, fmt, ##args);	\
	if (!nvt_mp_test_result_printed)	\
		printk(fmt, ##args);	\
} while (0)

#define NVT_RESULT_INVALID 0
#define NVT_RESULT_PASS 2
#define NVT_RESULT_FAIL 1


static uint8_t *RecordResult_Short = NULL;
static uint8_t *RecordResult_Open = NULL;
static uint8_t *RecordResult_FWMutual = NULL;
static uint8_t *RecordResult_FW_CC = NULL;
static uint8_t *RecordResult_FW_DiffMax = NULL;
static uint8_t *RecordResult_FW_DiffMin = NULL;

static int32_t TestResult_Short = 0;
static int32_t TestResult_Open = 0;
static int32_t TestResult_FW_Rawdata = 0;
static int32_t TestResult_FWMutual = 0;
static int32_t TestResult_FW_CC = 0;
static int32_t TestResult_Noise = 0;
static int32_t TestResult_FW_DiffMax = 0;
static int32_t TestResult_FW_DiffMin = 0;

static int32_t *RawData_Short = NULL;
static int32_t *RawData_Open = NULL;
static int32_t *RawData_Diff = NULL;
static int32_t *RawData_Diff_Min = NULL;
static int32_t *RawData_Diff_Max = NULL;
static int32_t *RawData_FWMutual = NULL;
static int32_t *RawData_FW_CC = NULL;

static struct proc_dir_entry *NVT_proc_selftest_entry = NULL;
static struct proc_dir_entry *NVT_proc_aftersales_test_entry;


#ifndef NVT_SAVE_TESTDATA_IN_FILE
static struct proc_dir_entry *NVT_proc_test_data_entry = NULL;
#endif

static int8_t nvt_mp_test_result_printed = 0;
static uint8_t mp_fw_ver;

extern void nvt_change_mode(uint8_t mode);
extern uint8_t nvt_get_fw_pipe(void);
extern void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr);
extern void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num);
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible);

/*******************************************************
Description:
	Novatek touchscreen allocate buffer for mp selftest.

return:
	Executive outcomes. 0---succeed. -12---Out of memory
*******************************************************/
static int nvt_mp_buffer_init(void)
{
	size_t RecordResult_BufSize = IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE;
	size_t RawData_BufSize = (IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * sizeof(int32_t);

	RecordResult_Short = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Short) {
		NVT_ERR("kzalloc for RecordResult_Short failed!\n");
		return -ENOMEM;
	}

	RecordResult_Open = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Open) {
		NVT_ERR("kzalloc for RecordResult_Open failed!\n");
		return -ENOMEM;
	}

	RecordResult_FWMutual = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FWMutual) {
		NVT_ERR("kzalloc for RecordResult_FWMutual failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_CC = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_CC) {
		NVT_ERR("kzalloc for RecordResult_FW_CC failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMax = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMax) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMax failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMin = (uint8_t *)kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMin) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMin failed!\n");
		return -ENOMEM;
	}

	RawData_Short = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short) {
		NVT_ERR("kzalloc for RawData_Short failed!\n");
		return -ENOMEM;
	}

	RawData_Open = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Open) {
		NVT_ERR("kzalloc for RawData_Open failed!\n");
		return -ENOMEM;
	}

	RawData_Diff = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff) {
		NVT_ERR("kzalloc for RawData_Diff failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Min = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Min) {
		NVT_ERR("kzalloc for RawData_Diff_Min failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Max = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Max) {
		NVT_ERR("kzalloc for RawData_Diff_Max failed!\n");
		return -ENOMEM;
	}

	RawData_FWMutual = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FWMutual) {
		NVT_ERR("kzalloc for RawData_FWMutual failed!\n");
		return -ENOMEM;
	}

	RawData_FW_CC = (int32_t *)kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FW_CC) {
		NVT_ERR("kzalloc for RawData_FW_CC failed!\n");
		return -ENOMEM;
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen free buffer for mp selftest.

return:
	n.a.
*******************************************************/
static void nvt_mp_buffer_deinit(void)
{
	if (RecordResult_Short) {
		kfree(RecordResult_Short);
		RecordResult_Short = NULL;
	}

	if (RecordResult_Open) {
		kfree(RecordResult_Open);
		RecordResult_Open = NULL;
	}

	if (RecordResult_FWMutual) {
		kfree(RecordResult_FWMutual);
		RecordResult_FWMutual = NULL;
	}

	if (RecordResult_FW_CC) {
		kfree(RecordResult_FW_CC);
		RecordResult_FW_CC = NULL;
	}

	if (RecordResult_FW_DiffMax) {
		kfree(RecordResult_FW_DiffMax);
		RecordResult_FW_DiffMax = NULL;
	}

	if (RecordResult_FW_DiffMin) {
		kfree(RecordResult_FW_DiffMin);
		RecordResult_FW_DiffMin = NULL;
	}

	if (RawData_Short) {
		kfree(RawData_Short);
		RawData_Short = NULL;
	}

	if (RawData_Open) {
		kfree(RawData_Open);
		RawData_Open = NULL;
	}

	if (RawData_Diff) {
		kfree(RawData_Diff);
		RawData_Diff = NULL;
	}

	if (RawData_Diff_Min) {
		kfree(RawData_Diff_Min);
		RawData_Diff_Min = NULL;
	}

	if (RawData_Diff_Max) {
		kfree(RawData_Diff_Max);
		RawData_Diff_Max = NULL;
	}

	if (RawData_FWMutual) {
		kfree(RawData_FWMutual);
		RawData_FWMutual = NULL;
	}

	if (RawData_FW_CC) {
		kfree(RawData_FW_CC);
		RawData_FW_CC = NULL;
	}
}

static void nvt_print_data_log_in_one_line(int32_t *data, int32_t data_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(data_num * 7 + 1, GFP_KERNEL);
	if (!tmp_log) {
		NVT_ERR("kzalloc for tmp_log failed!\n ");
		return;
	}

	for (i = 0; i < data_num; i++) {
		sprintf(tmp_log + i * 7, "%5d, ", data[i]);
	}
	tmp_log[data_num * 7] = '\0';
	NVT_LOG("%s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

static void nvt_print_result_log_in_one_line(uint8_t *result, int32_t result_num)
{
	char *tmp_log = NULL;
	int32_t i = 0;

	tmp_log = (char *)kzalloc(result_num * 6 + 1, GFP_KERNEL);
	if (!tmp_log) {
		NVT_ERR("kzalloc for tmp_log failed!\n ");
		return;
	}

	for (i = 0; i < result_num; i++) {
		sprintf(tmp_log + i * 6, "0x%02X, ", result[i]);
	}
	tmp_log[result_num * 6] = '\0';
	printk("%s", tmp_log);
	if (tmp_log) {
		kfree(tmp_log);
		tmp_log = NULL;
	}

	return;
}

/*******************************************************
Description:
	Novatek touchscreen self-test criteria print function.

return:
	n.a.
*******************************************************/
static void nvt_print_lmt_array(int32_t *array, int32_t x_ch, int32_t y_ch)
{
	int32_t j = 0;

	for (j = 0; j < y_ch; j++) {
		nvt_print_data_log_in_one_line(array + j * x_ch, x_ch);
		printk("\n");
	}
#if TOUCH_KEY_NUM > 0
	nvt_print_data_log_in_one_line(array + y_ch * x_ch, Key_Channel);
	printk("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
}

static void nvt_print_criteria(void)
{
	NVT_LOG("++\n");

	//---PS_Config_Lmt_Short_Rawdata---
	printk("PS_Config_Lmt_Short_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Short_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_Short_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Short_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_Open_Rawdata---
	printk("PS_Config_Lmt_Open_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_Open_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_Rawdata---
	printk("PS_Config_Lmt_FW_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_CC---
	printk("PS_Config_Lmt_FW_CC_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_CC_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_CC_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_CC_N, X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_Diff---
	printk("PS_Config_Lmt_FW_Diff_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Diff_P, X_Channel, Y_Channel);
	printk("PS_Config_Lmt_FW_Diff_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Diff_N, X_Channel, Y_Channel);

	NVT_LOG("--\n");
}

#ifndef NVT_SAVE_TESTDATA_IN_FILE
void dump_buff(int32_t *rawdata, uint8_t x_ch, uint8_t y_ch)
{
	int32_t y = 0;

	for (y = 0; y < y_ch; y++)
	{
		nvt_print_data_log_in_one_line(rawdata + y * x_ch, x_ch);
		printk("\n");
	}
}
#endif
static int32_t nvt_save_rawdata_to_csv(int32_t *rawdata, uint8_t x_ch, uint8_t y_ch,
														const char *file_path, uint32_t offset, enum test_type type)
{
#ifdef NVT_SAVE_TESTDATA_IN_FILE
	int32_t x = 0;
	int32_t y = 0;
	int32_t iArrayIndex = 0;
	struct file *fp = NULL;
	char *fbufp = NULL;
	mm_segment_t org_fs;
	int32_t write_ret = 0;
	uint32_t output_len = 0;
	loff_t pos = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
	int32_t keydata_output_offset = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
#else  /*#ifdef NVT_SAVE_TESTDATA_IN_FILE*/
	struct test_buf *tbuf = ts->testdata;
#endif /*#ifdef NVT_SAVE_TESTDATA_IN_FILE*/

	printk("%s:++\n", __func__);

#ifdef NVT_SAVE_TESTDATA_IN_FILE
	fbufp = (char *)kzalloc(8192, GFP_KERNEL);
	if (!fbufp) {
		NVT_ERR("kzalloc for fbufp failed!\n");
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			sprintf(fbufp + iArrayIndex * 7 + y * 2, "%5d, ", rawdata[iArrayIndex]);
		}
		nvt_print_data_log_in_one_line(rawdata + y * x_ch, x_ch);
		printk("\n");
		sprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2,"\r\n");
	}
#if TOUCH_KEY_NUM > 0
	keydata_output_offset = y_ch * x_ch * 7 + y_ch * 2;
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		sprintf(fbufp + keydata_output_offset + k * 7, "%5d, ", rawdata[iArrayIndex]);
	}
	nvt_print_data_log_in_one_line(rawdata + y_ch * x_ch, Key_Channel);
	printk("\n");
	sprintf(fbufp + y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7, "\r\n");
#endif /* #if TOUCH_KEY_NUM > 0 */

	org_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(file_path, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		NVT_ERR("open %s failed\n", file_path);
		set_fs(org_fs);
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

#if TOUCH_KEY_NUM > 0
	output_len = y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7 + 2;
#else
	output_len = y_ch * x_ch * 7 + y_ch * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	pos = offset;
	write_ret = vfs_write(fp, (char __user *)fbufp, output_len, &pos);
	if (write_ret <= 0) {
		NVT_ERR("write %s failed\n", file_path);
		set_fs(org_fs);
		if (fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		if (fbufp) {
			kfree(fbufp);
			fbufp = NULL;
		}
		return -1;
	}

	set_fs(org_fs);
	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}
	if (fbufp) {
		kfree(fbufp);
		fbufp = NULL;
	}
#else /*#ifdef NVT_SAVE_TESTDATA_IN_FILE*/
	switch (type) {
	case SHORT_TEST:
		NVT_LOG("SHORT_TEST start copy TEST_BUF_LEN = %d", TEST_BUF_LEN);
		memset(tbuf->shorttest.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->shorttest.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->shorttest.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST(%p):=============\n", tbuf->shorttest.buf);
		dump_buff((int32_t *)tbuf->shorttest.buf, x_ch, y_ch);
		NVT_LOG("SHORT_TEST copy complete");
	break;
	case OPEN_TEST:
		NVT_LOG("OPEN_TEST start copy");
		memset(tbuf->opentest.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->opentest.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->opentest.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST(%p):=============\n", tbuf->opentest.buf);
		dump_buff((int32_t *)tbuf->opentest.buf, x_ch, y_ch);
		NVT_LOG("OPEN_TEST copy complete");
	break;
	case FWMUTUAL_TEST:
		NVT_LOG("FWMUTUAL_TEST start copy");
		memset(tbuf->fwmutualtest.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->fwmutualtest.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->fwmutualtest.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST:=============\n");
		dump_buff((int32_t *)tbuf->fwmutualtest.buf, x_ch, y_ch);
		NVT_LOG("FWMUTUAL_TEST copy complete");
	break;
	case FWCC_TEST:
		NVT_LOG("FWCC_TEST start copy");
		memset(tbuf->fwcctest.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->fwcctest.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->fwcctest.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST:=============\n");
		dump_buff((int32_t *)tbuf->fwcctest.buf, x_ch, y_ch);
		NVT_LOG("FWCC_TEST copy complete");
	break;
	case NOISE_MAX_TEST:
		NVT_LOG("NOISE_MAX_TEST start copy");
		memset(tbuf->noisetest_max.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->noisetest_max.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->noisetest_max.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST:=============\n");
		dump_buff((int32_t *)tbuf->noisetest_max.buf, x_ch, y_ch);
		NVT_LOG("NOISE_MAX_TEST copy complete");
	break;
	case NOISE_MIN_TEST:
		NVT_LOG("NOISE_MIN_TEST start copy");
		memset(tbuf->noisetest_min.buf, 0, TEST_BUF_LEN);
		memcpy(tbuf->noisetest_min.buf, (uint8_t *)rawdata, x_ch * y_ch * sizeof(int32_t));
		tbuf->noisetest_min.len = x_ch * y_ch * sizeof(int32_t);
		NVT_LOG("===========SRC:=============\n");
		dump_buff(rawdata, x_ch, y_ch);
		NVT_LOG("===========DST:=============\n");
		dump_buff((int32_t *)tbuf->noisetest_min.buf, x_ch, y_ch);
		NVT_LOG("NOISE_MIN_TEST copy complete");
	break;
	default:
		NVT_ERR("test type is illegal");
	break;
	}
#endif /*#ifdef NVT_SAVE_TESTDATA_IN_FILE*/
	printk("%s:--\n", __func__);

	return 0;
}

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("polling hand shake status failed, buf[1]=0x%02X\n", buf[1]);

		// Read back 5 bytes from offset EVENT_MAP_HOST_CMD for debug check
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);
		NVT_ERR("Read back 5 bytes from offset EVENT_MAP_HOST_CMD: 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n", buf[1], buf[2], buf[3], buf[4], buf[5]);

		return -1;
	} else {
		return 0;
	}
}

static int8_t nvt_switch_FreqHopEnDis(uint8_t FreqHopEnDis)
{
	uint8_t buf[8] = {0};
	uint8_t retry = 0;
	int8_t ret = 0;

	NVT_LOG("++\n");

	for (retry = 0; retry < 20; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

		//---switch FreqHopEnDis---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = FreqHopEnDis;
		CTP_SPI_WRITE(ts->client, buf, 2);

		msleep(35);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(retry == 20)) {
		NVT_ERR("switch FreqHopEnDis 0x%02X failed, buf[1]=0x%02X\n", FreqHopEnDis, buf[1]);
		ret = -1;
	}

	NVT_LOG("--\n");

	return ret;
}

static int32_t nvt_read_baseline(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_RAWDATA_CSV_FILE, 0, FWMUTUAL_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

static int32_t nvt_read_CC(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	printk("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, FW_CC_CSV_FILE, 0, FWCC_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

static void nvt_enable_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_noise(int32_t *xdata)
{
	uint8_t x_num = 0;
	uint8_t y_num = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t iArrayIndex = 0;
	int32_t frame_num = 0;
	uint32_t rawdata_diff_min_offset = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	printk("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		RawData_Diff_Max[iArrayIndex] = (int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
		RawData_Diff_Min[iArrayIndex] = (int8_t)(xdata[iArrayIndex] & 0xFF);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, 0, NOISE_MAX_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2 + Key_Channel * 7 + 2;
#else
	rawdata_diff_min_offset = Y_Channel * X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
	printk("%s:RawData_Diff_Min:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Min, X_Channel, Y_Channel, NOISE_TEST_CSV_FILE, rawdata_diff_min_offset, NOISE_MIN_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

static void nvt_enable_open_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable open test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 5);
}

static int32_t nvt_read_fw_open(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
	rawdata_buf = (uint8_t *)kzalloc((IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * 2, GFP_KERNEL);
#else
	rawdata_buf = (uint8_t *)kzalloc(IC_X_CFG_SIZE * IC_Y_CFG_SIZE * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		NVT_ERR("kzalloc for rawdata_buf failed!\n");
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + y * IC_X_CFG_SIZE * 2);

		buf[0] = (uint8_t)((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) & 0xFF);
		CTP_SPI_READ(ts->client, buf, IC_X_CFG_SIZE * 2 + 1);
		memcpy(rawdata_buf + y * IC_X_CFG_SIZE * 2, buf + 1, IC_X_CFG_SIZE * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	//---change xdata index---
	nvt_set_page(raw_btn_pipe_addr);

	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, IC_KEY_CFG_SIZE * 2 + 1);
	memcpy(rawdata_buf + IC_Y_CFG_SIZE * IC_X_CFG_SIZE * 2, buf + 1, IC_KEY_CFG_SIZE * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		for (x = 0; x < IC_X_CFG_SIZE; x++) {
			if ((AIN_Y[y] != 0xFF) && (AIN_X[x] != 0xFF)) {
				xdata[AIN_Y[y] * X_Channel + AIN_X[x]] = (int16_t)((rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2] + 256 * rawdata_buf[(y * IC_X_CFG_SIZE + x) * 2 + 1]));
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF)
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] = (int16_t)(rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2] + 256 * rawdata_buf[(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);


	printk("%s:RawData_Open\n", __func__);
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, OPEN_TEST_CSV_FILE, 0, OPEN_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

static int32_t nvt_read_fw_short(int32_t *xdata)
{
	uint32_t raw_pipe_addr = 0;
	uint8_t *rawdata_buf = NULL;
	uint32_t x = 0;
	uint32_t y = 0;
	uint8_t buf[128] = {0};
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	uint32_t raw_btn_pipe_addr = 0;
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		return -EAGAIN;
	}

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status()) {
		return -EAGAIN;
	}

#if TOUCH_KEY_NUM > 0
    rawdata_buf = (uint8_t *)kzalloc((X_Channel * Y_Channel + Key_Channel) * 2, GFP_KERNEL);
#else
    rawdata_buf = (uint8_t *)kzalloc(X_Channel * Y_Channel * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		NVT_ERR("kzalloc for rawdata_buf failed!\n");
		return -ENOMEM;
	}

	if (nvt_get_fw_pipe() == 0)
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	else
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

	for (y = 0; y < Y_Channel; y++) {
		//---change xdata index---
		nvt_set_page(raw_pipe_addr + y * X_Channel * 2);

		buf[0] = (uint8_t)((raw_pipe_addr + y * X_Channel * 2) & 0xFF);
		CTP_SPI_READ(ts->client, buf, X_Channel * 2 + 1);
		memcpy(rawdata_buf + y * X_Channel * 2, buf + 1, X_Channel * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

    //---change xdata index---
	nvt_set_page(raw_btn_pipe_addr);

	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, Key_Channel * 2 + 1);
	memcpy(rawdata_buf + Y_Channel * X_Channel * 2, buf + 1, Key_Channel * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < X_Channel; x++) {
			iArrayIndex = y * X_Channel + x;
			xdata[iArrayIndex] = (int16_t)(rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] = (int16_t)(rawdata_buf[iArrayIndex * 2] + 256 * rawdata_buf[iArrayIndex * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (rawdata_buf) {
		kfree(rawdata_buf);
		rawdata_buf = NULL;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	printk("%s:RawData_Short\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel, SHORT_TEST_CSV_FILE, 0, SHORT_TEST) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	NVT_LOG("--\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen raw data test for each single point function.

return:
	Executive outcomes. 0---passed. negative---failed.
*******************************************************/
static int32_t RawDataTest_SinglePoint_Sub(int32_t rawdata[], uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive[], int32_t Rawdata_Limit_Negative[])
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
    int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
	int32_t iArrayIndex = 0;
	bool isPass = true;

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			iArrayIndex = j * x_ch + i;

			RecordResult[iArrayIndex] = 0x00; // default value for PASS

			if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x01;

			if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if(rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x01;

		if(rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x02;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Check RecordResult---
	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++) {
			if (RecordResult[j * x_ch + i] != 0) {
				isPass = false;
				break;
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		if (RecordResult[iArrayIndex] != 0) {
			isPass = false;
			break;
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	if (isPass == false) {
		return -1; // FAIL
	} else {
		return 0; // PASS
	}
}

/*******************************************************
Description:
	Novatek touchscreen print self-test result function.

return:
	n.a.
*******************************************************/
void print_selftest_result(struct seq_file *m, int32_t TestResult, uint8_t RecordResult[], int32_t rawdata[], uint8_t x_len, uint8_t y_len)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t iArrayIndex = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	switch (TestResult) {
		case 0:
			nvt_mp_seq_printf(m, " PASS!\n");
			break;

		case 1:
			nvt_mp_seq_printf(m, " ERROR! Read Data FAIL!\n");
			break;

		case -1:
			nvt_mp_seq_printf(m, " FAIL!\n");
			nvt_mp_seq_printf(m, "RecordResult:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_result_log_in_one_line(RecordResult + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
#if TOUCH_KEY_NUM > 0
			for (k = 0; k < Key_Channel; k++) {
				iArrayIndex = y_len * x_len + k;
				seq_printf(m, "0x%02X, ", RecordResult[iArrayIndex]);
			}
			if (!nvt_mp_test_result_printed)
				nvt_print_result_log_in_one_line(RecordResult + y_len * x_len, Key_Channel);
			nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
			nvt_mp_seq_printf(m, "ReadData:\n");
			for (i = 0; i < y_len; i++) {
				for (j = 0; j < x_len; j++) {
					iArrayIndex = i * x_len + j;
					seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
				}
				if (!nvt_mp_test_result_printed)
					nvt_print_data_log_in_one_line(rawdata + i * x_len, x_len);
				nvt_mp_seq_printf(m, "\n");
			}
#if TOUCH_KEY_NUM > 0
			for (k = 0; k < Key_Channel; k++) {
				iArrayIndex = y_len * x_len + k;
				seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
			}
			if (!nvt_mp_test_result_printed)
				nvt_print_data_log_in_one_line(rawdata + y_len * x_len, Key_Channel);
			nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
			break;
	}
	nvt_mp_seq_printf(m, "\n");
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show_selftest(struct seq_file *m, void *v)
{
	NVT_LOG("++\n");

	nvt_mp_seq_printf(m, "FW Version: %d\n\n", mp_fw_ver);

	nvt_mp_seq_printf(m, "Short Test");
	if ((TestResult_Short == 0) || (TestResult_Short == 1)) {
		print_selftest_result(m, TestResult_Short, RecordResult_Short, RawData_Short, X_Channel, Y_Channel);
	} else { // TestResult_Short is -1
		print_selftest_result(m, TestResult_Short, RecordResult_Short, RawData_Short, X_Channel, Y_Channel);
	}

	nvt_mp_seq_printf(m, "Open Test");
	print_selftest_result(m, TestResult_Open, RecordResult_Open, RawData_Open, X_Channel, Y_Channel);

	nvt_mp_seq_printf(m, "FW Rawdata Test");
	if ((TestResult_FW_Rawdata == 0) || (TestResult_FW_Rawdata == 1)) {
		 print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
	} else { // TestResult_FW_Rawdata is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_FWMutual == -1) {
			nvt_mp_seq_printf(m, "FW Mutual");
			print_selftest_result(m, TestResult_FWMutual, RecordResult_FWMutual, RawData_FWMutual, X_Channel, Y_Channel);
		}
		if (TestResult_FW_CC == -1) {
			nvt_mp_seq_printf(m, "FW CC");
			print_selftest_result(m, TestResult_FW_CC, RecordResult_FW_CC, RawData_FW_CC, X_Channel, Y_Channel);
		}
	}

	nvt_mp_seq_printf(m, "Noise Test");
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");

		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_seq_printf(m, "FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax, RecordResult_FW_DiffMax, RawData_Diff_Max, X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_seq_printf(m, "FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin, RecordResult_FW_DiffMin, RawData_Diff_Min, X_Channel, Y_Channel);
		}
	}

	nvt_mp_test_result_printed = 1;

	NVT_LOG("--\n");

    return 0;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen self-test sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest
};

#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
static void goto_next_line(char **ptr)
{
	do {
		*ptr = *ptr + 1;
	} while (**ptr != '\n');
	*ptr = *ptr + 1;
}

static void copy_this_line(char *dest, char *src)
{
	char *copy_from;
	char *copy_to;

	copy_from = src;
	copy_to = dest;
	do {
		*copy_to = *copy_from;
		copy_from++;
		copy_to++;
	} while((*copy_from != '\n') && (*copy_from != '\r'));
	*copy_to = '\0';
}

int32_t parse_mp_setting_criteria_item(char **ptr, const char *item_string, int32_t *item_value)
{
	char *tmp = NULL;

	NVT_LOG("++\n");
	tmp = strstr(*ptr, item_string);
	if (tmp == NULL) {
		NVT_ERR("%s not found\n", item_string);
		return -1;
	}
	*ptr = tmp;
	goto_next_line(ptr);
	sscanf(*ptr, "%d,", item_value);
	NVT_LOG("%s %d\n", item_string, *item_value);

	NVT_LOG("--\n");
	return 0;
}

int32_t parse_mp_setting_ain_array(char **ptr, const char *item_string, uint8_t *ain_array, uint32_t IC_CFG_SIZE)
{
	char *tmp = NULL;
	int32_t i = 0;
	char *token = NULL;
	char *tok_ptr = NULL;
	size_t offset = 0;
	char tmp_buf[512] = {0};

	NVT_LOG("++\n");
	tmp = strstr(*ptr, item_string);
	if (tmp == NULL) {
		NVT_ERR("%s not found!\n", item_string);
		return -1;
	}
	*ptr = tmp;

	NVT_LOG("%s\n", item_string);
	// walk thru this line
	goto_next_line(ptr);

	// copy this line to tmp_buf
	memset(tmp_buf, 0, sizeof(tmp_buf));
	copy_this_line(tmp_buf, *ptr);
	offset = strlen(tmp_buf);
	tok_ptr = tmp_buf;
	i = 0;
	while ((token = strsep(&tok_ptr,", \t\r\0"))) {
		if (strlen(token) == 0)
			continue;
		if (!strcmp(token, "0xFF") || !strcmp(token, "0xff"))
			ain_array[i] = 255;
		else
			ain_array[i] = (uint8_t) simple_strtol(token, NULL, 10);
		printk("%d, ", ain_array[i]);

		i++;
	}
	printk("\n");
	// check if number equals to IC_CFG_SIZE
	if (i != IC_CFG_SIZE) {
		NVT_ERR("load AIN config failed!, i=%d, IC_CFG_SIZE=%d\n", i, IC_CFG_SIZE);
		return -1;
	}
	// go forward
	*ptr = *ptr + offset;

	NVT_LOG("--\n");
	return 0;
}

int32_t parse_mp_criteria_item_array(char **ptr, const char *item_string, int32_t *item_array)
{
	char *tmp = NULL;
	int32_t i = 0;
	int32_t j = 0;
	char *token = NULL;
	char *tok_ptr = NULL;
	size_t offset = 0;
	char tmp_buf[512] = {0};

	NVT_LOG("++\n");
	tmp = strstr(*ptr, item_string);
	if (tmp == NULL) {
		NVT_ERR("%s not found\n", item_string);
		return -1;
	}
	*ptr = tmp;

	NVT_LOG("%s\n", item_string);
	for (i = 0; i < Y_Channel; i++) {
		// walk thru this line
		goto_next_line(ptr);
		memset(tmp_buf, 0, sizeof(tmp_buf));
		copy_this_line(tmp_buf, *ptr);
		offset = strlen(tmp_buf);
		tok_ptr = tmp_buf;
		j = 0;
		while ((token = strsep(&tok_ptr,", \t\r\0"))) {
			if (strlen(token) == 0)
				continue;
			item_array[i * X_Channel + j] = (int32_t) simple_strtol(token, NULL, 10);
			printk("%5d, ", item_array[i * X_Channel + j]);
			j++;
		}
		printk("\n");
		// check if j equals to X_Channel
		if (j != X_Channel) {
			NVT_ERR("j not equal X_Channel!, j=%d, X_Channel=%d\n", j, X_Channel);
			return -1;
		}
		// go forward
		*ptr = *ptr + offset;
	}
#if TOUCH_KEY_NUM > 0
	// walk thru this line
	goto_next_line(ptr);
	memset(tmp_buf, 0, sizeof(tmp_buf));
	copy_this_line(tmp_buf, *ptr);
	offset = strlen(tmp_buf);
	tok_ptr = tmp_buf;
	i = 0;
	while ((token = strsep(&tok_ptr,", \t\r\0"))) {
		if (strlen(token) == 0)
			continue;
		item_array[Y_Channel * X_Channel + i] = (int32_t) simple_strtol(token, NULL, 10);
		printk("%5d, ", item_array[Y_Channel * X_Channel + i]);
		i++;
	}
	printk("\n");
	// check if i equals to Key_Channel
	if (i != Key_Channel) {
		NVT_ERR("i not equal Key_Channel!, i=%d, Key_Channel=%d\n", i, Key_Channel);
		return -1;
	}
	// go forware
	*ptr = *ptr + offset;
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("--\n");
	return 0;
}

static int32_t nvt_load_mp_setting_criteria_from_csv(const char *filename)
{
	int32_t retval = 0;
	const struct firmware *fw_entry = NULL;
	char *fbufp = NULL;
	char *ptr = NULL;
	//int32_t i = 0;

	NVT_LOG("++\n");

	if (NULL == filename) {
		NVT_ERR("filename is null\n");
		retval = -1;
		goto exit_free;
	}

	retval = request_firmware(&fw_entry, filename, &ts->client->dev);
	if (retval) {
		NVT_ERR("%s load failed, retval=%d\n", filename, retval);
		retval = -1;
		goto exit_free;
	}

	fbufp = (char *)kzalloc(fw_entry->size + 2, GFP_KERNEL);
	if (!fbufp) {
		NVT_ERR("kzalloc %zu bytes failed!\n", fw_entry->size);
		retval = -1;
		goto exit_free;
	}

	memcpy(fbufp, fw_entry->data, fw_entry->size);

	//NVT_LOG("File Size: %zu\n", fw_entry->size);
	//NVT_LOG("---------------------------------------------------\n");
	//printk("fbufp:\n");
	//for(i = 0; i < fw_entry->size; i++) {
	//	printk("%c", fbufp[i]);
	//}
	//NVT_LOG("---------------------------------------------------\n");

	fbufp[fw_entry->size] = '\0';
	fbufp[fw_entry->size + 1] = '\n';

	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "IC_X_CFG_SIZE:", &IC_X_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "IC_Y_CFG_SIZE:", &IC_Y_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}

#if TOUCH_KEY_NUM > 0
	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "IC_KEY_CFG_SIZE:", &IC_KEY_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "X_Channel:", &X_Channel) < 0) {
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "Y_Channel:", &Y_Channel) < 0) {
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_setting_ain_array(&ptr, "AIN_X:", AIN_X, IC_X_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_setting_ain_array(&ptr, "AIN_Y:", AIN_Y, IC_Y_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}

#if TOUCH_KEY_NUM > 0
	ptr = fbufp;
	if (parse_mp_setting_ain_array(&ptr, "AIN_KEY:", AIN_KEY, IC_KEY_CFG_SIZE) < 0) {
		retval = -1;
		goto exit_free;
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_Short_Rawdata_P:", PS_Config_Lmt_Short_Rawdata_P) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_Short_Rawdata_P array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_Short_Rawdata_N:", PS_Config_Lmt_Short_Rawdata_N) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_Short_Rawdata_N array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_Open_Rawdata_P:", PS_Config_Lmt_Open_Rawdata_P) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_Open_Rawdata_P array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_Open_Rawdata_N:", PS_Config_Lmt_Open_Rawdata_N) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_Open_Rawdata_N array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_Rawdata_P:", PS_Config_Lmt_FW_Rawdata_P) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_Rawdata_P array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_Rawdata_N:", PS_Config_Lmt_FW_Rawdata_N) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_Rawdata_N array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_CC_P:", PS_Config_Lmt_FW_CC_P) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_CC_P array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_CC_N:", PS_Config_Lmt_FW_CC_N) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_CC_N array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_Diff_P:", PS_Config_Lmt_FW_Diff_P) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_Diff_P array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_criteria_item_array(&ptr, "PS_Config_Lmt_FW_Diff_N:", PS_Config_Lmt_FW_Diff_N) < 0) {
		NVT_ERR("Cannot get PS_Config_Lmt_FW_Diff_N array value!\n");
		retval = -1;
		goto exit_free;
	}

	ptr = fbufp;
	if (parse_mp_setting_criteria_item(&ptr, "PS_Config_Diff_Test_Frame:", &PS_Config_Diff_Test_Frame) < 0) {
		retval = -1;
		goto exit_free;
	}

	NVT_LOG("Load MP setting and criteria from CSV file finished.\n");
	retval = 0;

exit_free:
	if (fw_entry) {
		release_firmware(fw_entry);
		fw_entry = NULL;
	}
	if (fbufp) {
		kfree(fbufp);
		fbufp = NULL;
	}

	NVT_LOG("--, retval=%d\n", retval);
	return retval;
}
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_selftest open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_selftest_open(struct inode *inode, struct file *file)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	char mp_setting_criteria_csv_filename[64] = {0};
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	TestResult_Short = 0;
	TestResult_Open = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_FWMutual = 0;
	TestResult_FW_CC = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;

	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_MP_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->mp_name);
		}
	} else {
		nvt_update_firmware(ts->mp_name);
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("get fw info failed!\n");
		return -EAGAIN;
	}

	mp_fw_ver = ts->fw_ver;

#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	//---Check if MP Setting Criteria CSV file exist and load---
	snprintf(mp_setting_criteria_csv_filename, PAGE_SIZE, "NT36xxx_MP_Setting_Criteria_%04X.csv", ts->nvt_pid);
	NVT_LOG("MP setting criteria csv filename: %s\n", mp_setting_criteria_csv_filename);
	if (nvt_load_mp_setting_criteria_from_csv(mp_setting_criteria_csv_filename) < 0) {
		NVT_ERR("SelfTest MP setting criteria CSV file not exist or load failed\n");
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */
		/* Parsing criteria from dts */
		if(of_property_read_bool(np, "novatek,mp-support-dt")) {
			/*
			 * Parsing Criteria by Novatek PID
			 * The string rule is "novatek-mp-criteria-<nvt_pid>"
			 * nvt_pid is 2 bytes (show hex).
			 *
			 * Ex. nvt_pid = 500A
			 *     mpcriteria = "novatek-mp-criteria-500A"
			 */
			snprintf(mpcriteria, PAGE_SIZE, "novatek-mp-criteria-%04X", ts->nvt_pid);

			if (nvt_mp_parse_dt(np, mpcriteria)) {
				//---Download Normal FW---
				nvt_update_firmware(ts->fw_name);
				mutex_unlock(&ts->lock);
				NVT_ERR("mp parse device tree failed!\n");
				return -EINVAL;
			}
		} else {
			NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
			//---Print Test Criteria---
			nvt_print_criteria();
		}
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	} else {
		NVT_LOG("SelfTest MP setting criteria loaded from CSV file\n");
	}
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("switch frequency hopping disable failed!\n");
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw reset state failed!\n");
		return -EAGAIN;
	}

	msleep(100);

	//---Enter Test Mode---
	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("clear fw status failed!\n");
		return -EAGAIN;
	}

	nvt_change_mode(MP_MODE_CC);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw status failed!\n");
		return -EAGAIN;
	}

	//---FW Rawdata Test---
	if (nvt_read_baseline(RawData_FWMutual) != 0) {
		TestResult_FWMutual = 1;
	} else {
		TestResult_FWMutual = RawDataTest_SinglePoint_Sub(RawData_FWMutual, RecordResult_FWMutual, X_Channel, Y_Channel,
												PS_Config_Lmt_FW_Rawdata_P, PS_Config_Lmt_FW_Rawdata_N);
	}
	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
	} else {
		TestResult_FW_CC = RawDataTest_SinglePoint_Sub(RawData_FW_CC, RecordResult_FW_CC, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_CC_P, PS_Config_Lmt_FW_CC_N);
	}

	if ((TestResult_FWMutual == 1) || (TestResult_FW_CC == 1)) {
		TestResult_FW_Rawdata = 1;
	} else {
		if ((TestResult_FWMutual == -1) || (TestResult_FW_CC == -1))
			TestResult_FW_Rawdata = -1;
		else
			TestResult_FW_Rawdata = 0;
	}

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	//---Noise Test---
	if (nvt_read_fw_noise(RawData_Diff) != 0) {
		TestResult_Noise = 1;	// 1: ERROR
		TestResult_FW_DiffMax = 1;
		TestResult_FW_DiffMin = 1;
	} else {
		TestResult_FW_DiffMax = RawDataTest_SinglePoint_Sub(RawData_Diff_Max, RecordResult_FW_DiffMax, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		TestResult_FW_DiffMin = RawDataTest_SinglePoint_Sub(RawData_Diff_Min, RecordResult_FW_DiffMin, X_Channel, Y_Channel,
											PS_Config_Lmt_FW_Diff_P, PS_Config_Lmt_FW_Diff_N);

		if ((TestResult_FW_DiffMax == -1) || (TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;
	}

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short) != 0) {
		TestResult_Short = 1; // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short = RawDataTest_SinglePoint_Sub(RawData_Short, RecordResult_Short, X_Channel, Y_Channel,
										PS_Config_Lmt_Short_Rawdata_P, PS_Config_Lmt_Short_Rawdata_N);
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open) != 0) {
		TestResult_Open = 1;    // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open = RawDataTest_SinglePoint_Sub(RawData_Open, RecordResult_Open, X_Channel, Y_Channel,
											PS_Config_Lmt_Open_Rawdata_P, PS_Config_Lmt_Open_Rawdata_N);
	}

	//---Download Normal FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_FW_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->fw_name);
		}
	} else {
		nvt_update_firmware(ts->fw_name);
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	nvt_mp_test_result_printed = 0;

	return seq_open(file, &nvt_selftest_seq_ops);
}

static const struct file_operations nvt_selftest_fops = {
	.owner = THIS_MODULE,
	.open = nvt_selftest_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

#ifdef CONFIG_OF
/*******************************************************
Description:
	Novatek touchscreen parse AIN setting for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_ain(struct device_node *np, const char *name, uint8_t *array, int32_t size)
{
	struct property *data;
	int32_t len, ret;
	int32_t tmp[40];
	int32_t i;

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len != size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
		return -1;
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, tmp, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return -1;
		}

		for (i = 0; i < len; i++)
			array[i] = tmp[i];

#if NVT_DEBUG
		printk("[NVT-ts] %s = ", name);
		nvt_print_result_log_in_one_line(array, len);
		printk("\n");
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for u32 type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_u32(struct device_node *np, const char *name, int32_t *para)
{
	int32_t ret;

	ret = of_property_read_u32(np, name, para);
	if (ret) {
		NVT_ERR("error reading %s. ret=%d\n", name, ret);
		return -1;
	} else {
#if NVT_DEBUG
		NVT_LOG("%s=%d\n", name, *para);
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse criterion for array type.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_array(struct device_node *np, const char *name, int32_t *array,
		int32_t size)
{
	struct property *data;
	int32_t len, ret;
#if NVT_DEBUG
	int32_t j = 0;
#endif

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len < size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
		return -1;
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, array, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return -1;
		}

#if NVT_DEBUG
		NVT_LOG("%s =\n", name);
		for (j = 0; j < Y_Channel; j++) {
			nvt_print_data_log_in_one_line(array + j * X_Channel, X_Channel);
			printk("\n");
		}
#if TOUCH_KEY_NUM > 0
		nvt_print_data_log_in_one_line(array + Y_Channel * X_Channel, Key_Channel);
		printk("\n");
#endif
#endif
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen parse device tree mp function.

return:
	n.a.
*******************************************************/
int32_t nvt_mp_parse_dt(struct device_node *root, const char *node_compatible)
{
	struct device_node *np = root;
	struct device_node *child = NULL;

	NVT_LOG("Parse mp criteria for node %s\n", node_compatible);

	/* find each MP sub-nodes */
	for_each_child_of_node(root, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			NVT_LOG("found child node %s\n", node_compatible);
			np = child;
			break;
		}
	}
	if (child == NULL) {
		NVT_ERR("Not found compatible node %s!\n", node_compatible);
		return -1;
	}

	/* MP Config*/
	if (nvt_mp_parse_u32(np, "IC_X_CFG_SIZE", &IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_u32(np, "IC_Y_CFG_SIZE", &IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_u32(np, "IC_KEY_CFG_SIZE", &IC_KEY_CFG_SIZE))
		return -1;
#endif

	if (nvt_mp_parse_u32(np, "X_Channel", &X_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "Y_Channel", &Y_Channel))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_X", AIN_X, IC_X_CFG_SIZE))
		return -1;

	if (nvt_mp_parse_ain(np, "AIN_Y", AIN_Y, IC_Y_CFG_SIZE))
		return -1;

#if TOUCH_KEY_NUM > 0
	if (nvt_mp_parse_ain(np, "AIN_KEY", AIN_KEY, IC_KEY_CFG_SIZE))
		return -1;
#endif

	/* MP Criteria */
	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_Rawdata_P", PS_Config_Lmt_Short_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Short_Rawdata_N", PS_Config_Lmt_Short_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_P", PS_Config_Lmt_Open_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_N", PS_Config_Lmt_Open_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_P", PS_Config_Lmt_FW_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_N", PS_Config_Lmt_FW_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_P", PS_Config_Lmt_FW_CC_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_CC_N", PS_Config_Lmt_FW_CC_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_P", PS_Config_Lmt_FW_Diff_P,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Diff_N", PS_Config_Lmt_FW_Diff_N,
			X_Channel * Y_Channel + Key_Channel))
		return -1;

	if (nvt_mp_parse_u32(np, "PS_Config_Diff_Test_Frame", &PS_Config_Diff_Test_Frame))
		return -1;

	NVT_LOG("Parse mp criteria done!\n");

	return 0;
}
#endif /* #ifdef CONFIG_OF */

static int nvt_short_test(void)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	char mp_setting_criteria_csv_filename[64] = {0};
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	TestResult_Short = 0;

	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_MP_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->mp_name);
		}
	} else {
		nvt_update_firmware(ts->mp_name);
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("get fw info failed!\n");
		return -EAGAIN;
	}

#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	//---Check if MP Setting Criteria CSV file exist and load---
	snprintf(mp_setting_criteria_csv_filename, PAGE_SIZE, "NT36xxx_MP_Setting_Criteria_%04X.csv", ts->nvt_pid);
	NVT_LOG("MP setting criteria csv filename: %s\n", mp_setting_criteria_csv_filename);
	if (nvt_load_mp_setting_criteria_from_csv(mp_setting_criteria_csv_filename) < 0) {
		NVT_ERR("SelfTest MP setting criteria CSV file not exist or load failed\n");
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */
		/* Parsing criteria from dts */
		if (of_property_read_bool(np, "novatek,mp-support-dt")) {
			/*
			 * Parsing Criteria by Novatek PID
			 * The string rule is "novatek-mp-criteria-<nvt_pid>"
			 * nvt_pid is 2 bytes (show hex).
			 *
			 * Ex. nvt_pid = 500A
			 *     mpcriteria = "novatek-mp-criteria-500A"
			 */
			snprintf(mpcriteria, PAGE_SIZE, "novatek-mp-criteria-%04X", ts->nvt_pid);

			if (nvt_mp_parse_dt(np, mpcriteria)) {
				//---Download Normal FW---
				if (nvt_get_dbgfw_status()) {
					if (nvt_update_firmware(DEFAULT_DEBUG_FW_NAME) < 0) {
						NVT_ERR("use built-in fw");
						nvt_update_firmware(ts->fw_name);
					}
				} else {
					nvt_update_firmware(ts->fw_name);
				}
				mutex_unlock(&ts->lock);
				NVT_ERR("mp parse device tree failed!\n");
				return -EINVAL;
			}
		} else {
			NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
			//---Print Test Criteria---
			nvt_print_criteria();
		}
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	} else {
		NVT_LOG("SelfTest MP setting criteria loaded from CSV file\n");
	}
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("switch frequency hopping disable failed!\n");
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw reset state failed!\n");
		return -EAGAIN;
	}

	msleep(100);

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short) != 0) {
		TestResult_Short = 1; // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Short = RawDataTest_SinglePoint_Sub(RawData_Short, RecordResult_Short, X_Channel, Y_Channel,
										PS_Config_Lmt_Short_Rawdata_P, PS_Config_Lmt_Short_Rawdata_N);
	}

	//---Download Normal FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_FW_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->fw_name);
		}
	} else {
		nvt_update_firmware(ts->fw_name);
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	nvt_mp_test_result_printed = 0;
	return TestResult_Short;
}

static int nvt_open_test(void)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};	//novatek-mp-criteria-default
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	char mp_setting_criteria_csv_filename[64] = {0};
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	TestResult_Open = 0;

	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	//---Download MP FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_MP_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->mp_name);
		}
	} else {
		nvt_update_firmware(ts->mp_name);
	}


	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("get fw info failed!\n");
		return -EAGAIN;
	}

#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	//---Check if MP Setting Criteria CSV file exist and load---
	snprintf(mp_setting_criteria_csv_filename, PAGE_SIZE, "NT36xxx_MP_Setting_Criteria_%04X.csv", ts->nvt_pid);
	NVT_LOG("MP setting criteria csv filename: %s\n", mp_setting_criteria_csv_filename);
	if (nvt_load_mp_setting_criteria_from_csv(mp_setting_criteria_csv_filename) < 0) {
		NVT_ERR("SelfTest MP setting criteria CSV file not exist or load failed\n");
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */
		/* Parsing criteria from dts */
		if (of_property_read_bool(np, "novatek,mp-support-dt")) {
			/*
			 * Parsing Criteria by Novatek PID
			 * The string rule is "novatek-mp-criteria-<nvt_pid>"
			 * nvt_pid is 2 bytes (show hex).
			 *
			 * Ex. nvt_pid = 500A
			 *     mpcriteria = "novatek-mp-criteria-500A"
			 */
			snprintf(mpcriteria, PAGE_SIZE, "novatek-mp-criteria-%04X", ts->nvt_pid);

			if (nvt_mp_parse_dt(np, mpcriteria)) {
				//---Download Normal FW---
				if (nvt_get_dbgfw_status()) {
					if (nvt_update_firmware(DEFAULT_DEBUG_FW_NAME) < 0) {
						NVT_ERR("use built-in fw");
						nvt_update_firmware(ts->fw_name);
					}
				} else {
					nvt_update_firmware(ts->fw_name);
				}
				mutex_unlock(&ts->lock);
				NVT_ERR("mp parse device tree failed!\n");
				return -EINVAL;
			}
		} else {
			NVT_LOG("Not found novatek,mp-support-dt, use default setting\n");
			//---Print Test Criteria---
			nvt_print_criteria();
		}
#if NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV
	} else {
		NVT_LOG("SelfTest MP setting criteria loaded from CSV file\n");
	}
#endif /* NVT_TOUCH_MP_SETTING_CRITERIA_FROM_CSV */

	if (nvt_switch_FreqHopEnDis(FREQ_HOP_DISABLE)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("switch frequency hopping disable failed!\n");
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		NVT_ERR("check fw reset state failed!\n");
		return -EAGAIN;
	}

	msleep(100);

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open) != 0) {
		TestResult_Open = 1;    // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open = RawDataTest_SinglePoint_Sub(RawData_Open, RecordResult_Open, X_Channel, Y_Channel,
											PS_Config_Lmt_Open_Rawdata_P, PS_Config_Lmt_Open_Rawdata_N);
	}

	//---Download Normal FW---
	if (nvt_get_dbgfw_status()) {
		if (nvt_update_firmware(DEFAULT_DEBUG_FW_NAME) < 0) {
			NVT_ERR("use built-in fw");
			nvt_update_firmware(ts->fw_name);
		}
	} else {
		nvt_update_firmware(ts->fw_name);
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	nvt_mp_test_result_printed = 0;

	return TestResult_Open;
}



static ssize_t nvt_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char tmp[5] = {0};
	int cnt;

	if (*pos != 0)
		return 0;
	cnt = snprintf(tmp, sizeof(ts->result_type), "%d\n", ts->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}

	*pos += cnt;
	return cnt;
}

static ssize_t nvt_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int retval = 0;
	char tmp[6];

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	if (!strncmp("short", tmp, 5)) {
		retval = nvt_short_test();
	} else if (!strncmp("open", tmp, 4)) {
		retval = nvt_open_test();
	} else if (!strncmp("i2c", tmp, 3)) {
		retval = nvt_get_fw_info();
	} else {
		NVT_ERR("[%s] cmd not support", tmp);
		retval = -EINVAL;
	}

	switch (retval) {
	case 0:
	ts->result_type = NVT_RESULT_PASS;
	break;
	case -EINVAL:
	ts->result_type = NVT_RESULT_INVALID;
	break;
	default:
	ts->result_type = NVT_RESULT_FAIL;
	break;
	}

out:
	if (retval >= 0)
		retval = count;

	return retval;
}


static const struct file_operations nvt_aftersales_test_ops = {
	.read		= nvt_selftest_read,
	.write		= nvt_selftest_write,
};

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_mp_proc_init(void)
{
	NVT_proc_selftest_entry = proc_create("nvt_selftest", 0444, NULL, &nvt_selftest_fops);
	if (NVT_proc_selftest_entry == NULL) {
		NVT_ERR("create /proc/nvt_selftest Failed!\n");
		return -1;
	} else {
		if(nvt_mp_buffer_init()) {
			NVT_ERR("Allocate mp memory failed\n");
			return -1;
		}
		else {
			NVT_LOG("create /proc/nvt_selftest Succeeded!\n");
		}
	}

	NVT_proc_aftersales_test_entry = proc_create("tp_selftest", 0644, NULL, &nvt_aftersales_test_ops);
	if (NVT_proc_aftersales_test_entry == NULL) {
		NVT_ERR("create /proc/tp_selftest Failed!\n");
		return -1;
	} else {
			NVT_LOG("create /proc/tp_selftest Succeeded!\n");
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen MP function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_mp_proc_deinit(void)
{
	nvt_mp_buffer_deinit();

	if (NVT_proc_selftest_entry != NULL) {
		remove_proc_entry("nvt_selftest", NULL);
		NVT_proc_selftest_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", "nvt_selftest");
	}
}

#ifndef NVT_SAVE_TESTDATA_IN_FILE
static void test_buff_free(struct test_buf *buf)
{
	if(buf) {
		if (buf->shorttest.buf)
			kfree(buf->shorttest.buf);
		if (buf->opentest.buf)
			kfree(buf->opentest.buf);
		if (buf->fwmutualtest.buf)
			kfree(buf->fwmutualtest.buf);
		if (buf->fwcctest.buf)
			kfree(buf->fwcctest.buf);
		if (buf->noisetest_max.buf)
			kfree(buf->noisetest_max.buf);
		if (buf->noisetest_min.buf)
			kfree(buf->noisetest_min.buf);
		kfree(buf);
	}
	return;
}

static int32_t test_buff_init(struct test_buf **tbuf)
{
	struct test_buf *buf;
	int32_t ret;

	buf = (struct test_buf *)kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf);
		goto alloc_fail;
	}

	buf->shorttest.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->shorttest.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->shorttest.buf);
		goto alloc_fail;
	}

	buf->opentest.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->opentest.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->opentest.buf);
		goto alloc_fail;
	}

	buf->fwmutualtest.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->fwmutualtest.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->fwmutualtest.buf);
		goto alloc_fail;
	}

	buf->fwcctest.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->fwcctest.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->fwcctest.buf);
		goto alloc_fail;
	}

	buf->noisetest_max.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->noisetest_max.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->noisetest_max.buf);
		goto alloc_fail;
	}

	buf->noisetest_min.buf = kzalloc(TEST_BUF_LEN, GFP_KERNEL);
	if (!buf->noisetest_min.buf) {
		NVT_ERR("alloc memory failed");
		ret = PTR_ERR(buf->noisetest_min.buf);
		goto alloc_fail;
	}

	buf->shorttest.type = SHORT_TEST;
	buf->opentest.type = OPEN_TEST;
	buf->fwmutualtest.type = FWMUTUAL_TEST;
	buf->fwcctest.type = FWCC_TEST;
	buf->noisetest_max.type = NOISE_MAX_TEST;
	buf->noisetest_min.type = NOISE_MIN_TEST;


	*tbuf = buf;
	return ret;

alloc_fail:
	*tbuf = NULL;
	test_buff_free(buf);
	return ret;
}

struct item_buf *item_arr[MAX_TEST_TYPE];

static void *t_start(struct seq_file *m, loff_t *pos)
{
	struct test_buf *tbuf = ts->testdata;

	item_arr[SHORT_TEST] = &tbuf->shorttest;
	item_arr[OPEN_TEST] = &tbuf->opentest;
	item_arr[FWMUTUAL_TEST] = &tbuf->fwmutualtest;
	item_arr[FWCC_TEST] = &tbuf->fwcctest;
	item_arr[NOISE_MAX_TEST] = &tbuf->noisetest_max;
	item_arr[NOISE_MIN_TEST] = &tbuf->noisetest_min;

	if (*pos >= MAX_TEST_TYPE) {
		*pos = 0;
		return NULL;
	}

	NVT_LOG("In start\n");
	NVT_LOG("=short= item ptr (%p), type (%d), buf ptr(%p)\n", &tbuf->shorttest, tbuf->shorttest.type, tbuf->shorttest.buf);
	NVT_LOG("=open= item ptr (%p), type (%d), buf ptr(%p)\n", &tbuf->opentest, tbuf->opentest.type, tbuf->opentest.buf);
	NVT_LOG("pos (%lld), first item ptr(%p)", *pos, item_arr[*pos]);
	return item_arr[*pos];
}

static void *t_next(struct seq_file *m, void *v, loff_t *pos)
{
	NVT_LOG("In next\n");
	NVT_LOG("ptr v (%p), pos (%lld), item ptr (%p)\n", v, *pos, &item_arr[*pos]);

	++*pos;
	if (*pos >= MAX_TEST_TYPE) {
		*pos = 0;
		return NULL;
	}

	NVT_LOG("ptr v (%p), pos (%lld), item ptr (%p)\n", v, *pos, &item_arr[*pos]);
	return item_arr[*pos];
}

static int32_t t_show(struct seq_file *m, void *v)
{
	int i, j;
	struct item_buf *item = v;
	int32_t *data = (int32_t *)item->buf;

	NVT_LOG("In show\n");
	NVT_LOG("item ptr (%p), type (%d), buf ptr(%p)\n", item, item->type, item->buf);


	switch (item->type)
	{
		case SHORT_TEST:
			seq_printf(m, "========SHORT_TEST========\n");
		break;
		case OPEN_TEST:
			seq_printf(m, "========OPEN_TEST========\n");
		break;
		case FWMUTUAL_TEST:
			seq_printf(m, "========FWMUTUAL_TEST========\n");
		break;
		case FWCC_TEST:
			seq_printf(m, "========FWCC_TEST========\n");
		break;
		case NOISE_MAX_TEST:
			seq_printf(m, "========NOISE_MAX_TEST========\n");
		break;
		case NOISE_MIN_TEST:
			seq_printf(m, "========NOISE_MIN_TEST========\n");
		break;
		default:
			NVT_LOG("test type illegal (%d)\n", item->type);
		return -EINVAL;
	}

	for (i = 0; i < Y_Channel; i++) {
		for (j = 0; j < X_Channel; j++) {
			seq_printf(m, "%5d, ", data[i * X_Channel + j]);
		}
		seq_puts(m, "\n");
	}
	seq_puts(m, "\n\n");

	return 0;
}

static void t_stop(struct seq_file *m, void *v)
{
	NVT_LOG("In stop\n");
	return;
}

const struct seq_operations t_fops = {
	.start  = t_start,
	.next   = t_next,
	.stop   = t_stop,
	.show   = t_show
};


static int32_t nvt_test_data_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &t_fops);
}

static const struct file_operations nvt_test_data_fops = {
	.owner = THIS_MODULE,
	.open = nvt_test_data_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int32_t nvt_test_data_proc_init(struct spi_device *client)
{
	int ret;
	struct test_buf *tbuf;
	struct nvt_ts_data *ts = spi_get_drvdata(client);

	NVT_proc_test_data_entry = proc_create("nvt_test_data", 0, NULL, &nvt_test_data_fops);
	if (NVT_proc_test_data_entry == NULL) {
		NVT_ERR("create proc/nvt_test_data Failed!");
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/nvt_test_data Succeeded!");
	}

	ret = test_buff_init(&tbuf);
	if (!tbuf) {
		NVT_ERR("test buff init failed");
		remove_proc_entry("nvt_test_data", NULL);
		return ret;
	}

	ts->testdata = tbuf;

	return 0;
}

void nvt_test_data_proc_deinit(void)
{
	test_buff_free(ts->testdata);
	ts->testdata = NULL;

	if (NVT_proc_test_data_entry != NULL) {
		remove_proc_entry("nvt_test_data", NULL);
		NVT_proc_test_data_entry = NULL;
		NVT_LOG("Removed /proc/nvt_test_data");
	}
}
#endif /*ifndef NVT_SAVE_TESTDATA_IN_FILE*/
#endif /* #if NVT_TOUCH_MP */
