/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>

#include "nt36xxx.h"
#include "nt36xxx_mp_ctrlram.h"

#if NVT_TOUCH_MP

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define MP_MODE_CC 0x41
#define FREQ_HOP_DISABLE 0x66
#define FREQ_HOP_ENABLE 0x65

#define SHORT_TEST_CSV_FILE "/data/local/tmp/ShortTest.csv"
#define OPEN_TEST_CSV_FILE "/data/local/tmp/OpenTest.csv"
#define FW_RAWDATA_CSV_FILE "/data/local/tmp/FWMutualTest.csv"
#define FW_CC_CSV_FILE "/data/local/tmp/FWCCTest.csv"
#define NOISE_TEST_CSV_FILE "/data/local/tmp/NoiseTest.csv"

#define nvt_mp_seq_printf(m, fmt, args...) do {	\
	seq_printf(m, fmt, ##args);	\
	if (!nvt_mp_test_result_printed)	\
		pr_info(fmt, ##args);	\
} while (0)

static uint8_t *RecordResult_Short;
static uint8_t *RecordResult_Short_Diff;
static uint8_t *RecordResult_Short_Base;
static uint8_t *RecordResult_Open;
static uint8_t *RecordResult_FWMutual;
static uint8_t *RecordResult_FW_CC;
static uint8_t *RecordResult_FW_CC_I;
static uint8_t *RecordResult_FW_CC_Q;
static uint8_t *RecordResult_FW_DiffMax;
static uint8_t *RecordResult_FW_DiffMin;

static int32_t TestResult_Short;
static int32_t TestResult_Short_Diff;
static int32_t TestResult_Short_Base;
static int32_t TestResult_Open;
static int32_t TestResult_FW_Rawdata;
static int32_t TestResult_FWMutual;
static int32_t TestResult_FW_CC;
static int32_t TestResult_FW_CC_I;
static int32_t TestResult_FW_CC_Q;
static int32_t TestResult_Noise;
static int32_t TestResult_FW_DiffMax;
static int32_t TestResult_FW_DiffMin;

static int32_t *RawData_Short;
static int32_t *RawData_Short_Diff;
static int32_t *RawData_Short_Base;
static int32_t *RawData_Open;
static int32_t *RawData_Diff;
static int32_t *RawData_Diff_Min;
static int32_t *RawData_Diff_Max;
static int32_t *RawData_FWMutual;
static int32_t *RawData_FW_CC;
static int32_t *RawData_FW_CC_I;
static int32_t *RawData_FW_CC_Q;

static struct proc_dir_entry *NVT_proc_selftest_entry;
static int8_t nvt_mp_test_result_printed;



/*******************************************************
 * Description:
 *	Novatek touchscreen allocate buffer for mp selftest.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---Out of memory
 *******************************************************/
static int nvt_mp_buffer_init(void)
{
	size_t RecordResult_BufSize = IC_X_CFG_SIZE * IC_Y_CFG_SIZE +
		IC_KEY_CFG_SIZE;
	size_t RawData_BufSize = (IC_X_CFG_SIZE * IC_Y_CFG_SIZE +
		IC_KEY_CFG_SIZE) * sizeof(int32_t);

	RecordResult_Short = kzalloc(RecordResult_BufSize,
		GFP_KERNEL);
	if (!RecordResult_Short) {
		NVT_ERR("kzalloc for RecordResult_Short failed!\n");
		return -ENOMEM;
	}

	RecordResult_Short_Diff = RecordResult_Short;

	RecordResult_Short_Base =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Short_Base) {
		NVT_ERR("kzalloc for RecordResult_Short_Base failed!\n");
		return -ENOMEM;
	}

	RecordResult_Open =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_Open) {
		NVT_ERR("kzalloc for RecordResult_Open failed!\n");
		return -ENOMEM;
	}

	RecordResult_FWMutual =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FWMutual) {
		NVT_ERR("kzalloc for RecordResult_FWMutual failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_CC =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_CC) {
		NVT_ERR("kzalloc for RecordResult_FW_CC failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_CC_I = RecordResult_FW_CC;

	RecordResult_FW_CC_Q =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_CC_Q) {
		NVT_ERR("kzalloc for RecordResult_FW_CC_Q failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMax =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMax) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMax failed!\n");
		return -ENOMEM;
	}

	RecordResult_FW_DiffMin =
		kzalloc(RecordResult_BufSize, GFP_KERNEL);
	if (!RecordResult_FW_DiffMin) {
		NVT_ERR("kzalloc for RecordResult_FW_DiffMin failed!\n");
		return -ENOMEM;
	}

	RawData_Short = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short) {
		NVT_ERR("kzalloc for RawData_Short failed!\n");
		return -ENOMEM;
	}

	RawData_Short_Diff = RawData_Short;

	RawData_Short_Base = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Short_Base) {
		NVT_ERR("kzalloc for RawData_Short_Base failed!\n");
		return -ENOMEM;
	}

	RawData_Open = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Open) {
		NVT_ERR("kzalloc for RawData_Open failed!\n");
		return -ENOMEM;
	}

	RawData_Diff = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff) {
		NVT_ERR("kzalloc for RawData_Diff failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Min = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Min) {
		NVT_ERR("kzalloc for RawData_Diff_Min failed!\n");
		return -ENOMEM;
	}

	RawData_Diff_Max = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_Diff_Max) {
		NVT_ERR("kzalloc for RawData_Diff_Max failed!\n");
		return -ENOMEM;
	}

	RawData_FWMutual = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FWMutual) {
		NVT_ERR("kzalloc for RawData_FWMutual failed!\n");
		return -ENOMEM;
	}

	RawData_FW_CC = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FW_CC) {
		NVT_ERR("kzalloc for RawData_FW_CC failed!\n");
		return -ENOMEM;
	}

	RawData_FW_CC_I = RawData_FW_CC;

	RawData_FW_CC_Q = kzalloc(RawData_BufSize, GFP_KERNEL);
	if (!RawData_FW_CC_Q) {
		NVT_ERR("kzalloc for RawData_FW_CC_Q failed!\n");
		return -ENOMEM;
	}

	return 0;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen self-test criteria print function.
 *
 * return:
 *	n.a.
 *******************************************************/
static void nvt_print_lmt_array(int32_t *array, int32_t x_ch, int32_t y_ch)
{
	int32_t i = 0;
	int32_t j = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (j = 0; j < y_ch; j++) {
		for (i = 0; i < x_ch; i++)
			pr_info("%5d, ", array[j * x_ch + i]);
		pr_info("\n");
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++)
		pr_info("%5d, ", array[y_ch * x_ch + k]);
	pr_info("\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
}

static void nvt_print_criteria(void)
{
	NVT_LOG("++\n");

	if (ts->carrier_system) {
		//---PS_Config_Lmt_Short_Diff---
		pr_info("PS_Config_Lmt_Short_Diff_P: %5d\n",
			PS_Config_Lmt_Short_Diff_P);
		pr_info("PS_Config_Lmt_Short_Diff_N: %5d\n",
			PS_Config_Lmt_Short_Diff_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_Short_Diff_P: %5d\n",
			PS_Config_Lmt_Key_Short_Diff_P);
		pr_info("PS_Config_Lmt_Key_Short_Diff_N: %5d\n",
			PS_Config_Lmt_Key_Short_Diff_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
		//---PS_Config_Lmt_Short_Base---
		pr_info("PS_Config_Lmt_Short_Base_P: %5d\n",
			PS_Config_Lmt_Short_Base_P);
		pr_info("PS_Config_Lmt_Short_Base_N: %5d\n",
			PS_Config_Lmt_Short_Base_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_Short_Base_P: %5d\n",
			PS_Config_Lmt_Key_Short_Base_P);
		pr_info("PS_Config_Lmt_Key_Short_Base_N: %5d\n",
			PS_Config_Lmt_Key_Short_Base_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
	} else {
		//---PS_Config_Lmt_Short_Rawdata---
		pr_info("PS_Config_Lmt_Short_Rawdata_P: %5d\n",
			PS_Config_Lmt_Short_Rawdata_P);
		pr_info("PS_Config_Lmt_Short_Rawdata_N: %5d\n",
			PS_Config_Lmt_Short_Rawdata_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_Short_Rawdata_P: %5d\n",
			PS_Config_Lmt_Key_Short_Rawdata_P);
		pr_info("PS_Config_Lmt_Key_Short_Rawdata_N: %5d\n",
			PS_Config_Lmt_Key_Short_Rawdata_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
	}

	//---PS_Config_Lmt_Open_Rawdata---
	pr_info("PS_Config_Lmt_Open_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_P,
		X_Channel, Y_Channel);
	pr_info("PS_Config_Lmt_Open_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_Open_Rawdata_N,
		X_Channel, Y_Channel);

	//---PS_Config_Lmt_FW_Rawdata---
	pr_info("PS_Config_Lmt_FW_Rawdata_P:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_P,
		X_Channel, Y_Channel);
	pr_info("PS_Config_Lmt_FW_Rawdata_N:\n");
	nvt_print_lmt_array(PS_Config_Lmt_FW_Rawdata_N,
		X_Channel, Y_Channel);

	if (ts->carrier_system) {
		//---PS_Config_Lmt_FW_CC_I---
		pr_info("PS_Config_Lmt_FW_CC_I_P: %5d\n",
			PS_Config_Lmt_FW_CC_I_P);
		pr_info("PS_Config_Lmt_FW_CC_I_N: %5d\n",
			PS_Config_Lmt_FW_CC_I_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_FW_CC_I_P: %5d\n",
			PS_Config_Lmt_Key_FW_CC_I_P);
		pr_info("PS_Config_Lmt_Key_FW_CC_I_N: %5d\n",
			PS_Config_Lmt_Key_FW_CC_I_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
		//---PS_Config_Lmt_FW_CC_Q---
		pr_info("PS_Config_Lmt_FW_CC_Q_P: %5d\n",
			PS_Config_Lmt_FW_CC_Q_P);
		pr_info("PS_Config_Lmt_FW_CC_Q_N: %5d\n",
			PS_Config_Lmt_FW_CC_Q_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_FW_CC_Q_P: %5d\n",
			PS_Config_Lmt_Key_FW_CC_Q_P);
		pr_info("PS_Config_Lmt_Key_FW_CC_Q_N: %5d\n",
			PS_Config_Lmt_Key_FW_CC_Q_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
	} else {
		//---PS_Config_Lmt_FW_CC---
		pr_info("PS_Config_Lmt_FW_CC_P: %5d\n",
			PS_Config_Lmt_FW_CC_P);
		pr_info("PS_Config_Lmt_FW_CC_N: %5d\n",
			PS_Config_Lmt_FW_CC_N);
#if TOUCH_KEY_NUM > 0
		pr_info("PS_Config_Lmt_Key_FW_CC_P: %5d\n",
			PS_Config_Lmt_Key_FW_CC_P);
		pr_info("PS_Config_Lmt_Key_FW_CC_N: %5d\n",
			PS_Config_Lmt_Key_FW_CC_N);
#endif /* #if TOUCH_KEY_NUM > 0 */
	}

	//---PS_Config_Lmt_FW_Diff---
	pr_info("PS_Config_Lmt_FW_Diff_P: %5d\n", PS_Config_Lmt_FW_Diff_P);
	pr_info("PS_Config_Lmt_FW_Diff_N: %5d\n", PS_Config_Lmt_FW_Diff_N);
#if TOUCH_KEY_NUM > 0
	pr_info("PS_Config_Lmt_Key_FW_Diff_P: %5d\n",
		PS_Config_Lmt_Key_FW_Diff_P);
	pr_info("PS_Config_Lmt_Key_FW_Diff_N: %5d\n",
		PS_Config_Lmt_Key_FW_Diff_N);
#endif /* #if TOUCH_KEY_NUM > 0 */

	NVT_LOG("--\n");
}

static int32_t nvt_save_rawdata_to_csv(int32_t *rawdata, uint8_t x_ch,
	uint8_t y_ch, const char *file_path, uint32_t offset)
{
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

	pr_info("%s:++\n", __func__);
	fbufp = kzalloc(8192, GFP_KERNEL);
	if (!fbufp) {
		NVT_ERR("kzalloc for fbufp failed!\n");
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			pr_info("%5d, ", rawdata[iArrayIndex]);
			sprintf(fbufp + iArrayIndex * 7 + y * 2,
				"%5d, ", rawdata[iArrayIndex]);
		}
		pr_info("\n");
		sprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2, "\r\n");
	}
#if TOUCH_KEY_NUM > 0
	keydata_output_offset = y_ch * x_ch * 7 + y_ch * 2;
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;
		pr_info("%5d, ", rawdata[iArrayIndex]);
		sprintf(fbufp + keydata_output_offset + k * 7,
			"%5d, ", rawdata[iArrayIndex]);
	}
	pr_info("\n");
	sprintf(fbufp + y_ch * x_ch * 7 + y_ch * 2 + Key_Channel * 7, "\r\n");
#endif /* #if TOUCH_KEY_NUM > 0 */

	org_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(file_path, O_RDWR | O_CREAT, 0644);
	if (fp == NULL || IS_ERR(fp)) {
		NVT_ERR("open %s failed\n", file_path);
		set_fs(org_fs);
		kfree(fbufp);
		fbufp = NULL;
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
		kfree(fbufp);
		fbufp = NULL;
		return -1;
	}

	set_fs(org_fs);
	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}
	kfree(fbufp);
	fbufp = NULL;

	pr_info("%s:--\n", __func__);

	return 0;
}

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if ((buf[1] == 0xA0) || (buf[1] == 0xA1))
			break;

		msleep(20);
	}

	if (i >= retry) {
		NVT_ERR("polling hand shake status failed, buf[1]=0x%02X\n",
			buf[1]);

		// Read back 5 bytes from offset EVENT_MAP_HOST_CMD
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);
		NVT_ERR("Read back 5 bytes from offset EVENT_MAP_HOST_CMD\n");
		NVT_ERR("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5]);

		return -1;
		}
	return 0;
}

static int8_t nvt_switch_FreqHopEnDis(uint8_t FreqHopEnDis)
{
	uint8_t buf[8] = {0};
	uint8_t retry = 0;
	int8_t ret = 0;

	NVT_LOG("++\n");

	for (retry = 0; retry < 20; retry++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---switch FreqHopEnDis---
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = FreqHopEnDis;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		msleep(35);

		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;
	}

	if (unlikely(retry == 20)) {
		NVT_ERR("switch FreqHopEnDis 0x%02X failed, buf[1]=0x%02X\n",
			FreqHopEnDis, buf[1]);
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
			if (ts->carrier_system) {
				xdata[iArrayIndex] =
					(uint16_t)xdata[iArrayIndex];
			} else {
				xdata[iArrayIndex] =
					(int16_t)xdata[iArrayIndex];
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		if (ts->carrier_system)
			xdata[iArrayIndex] = (uint16_t)xdata[iArrayIndex];
		else
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	pr_info("%s:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel,
		FW_RAWDATA_CSV_FILE, 0) < 0) {
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
	int32_t xdata_tmp = 0;
#if TOUCH_KEY_NUM > 0
	int32_t k = 0;
#endif /* #if TOUCH_KEY_NUM > 0 */
	uint32_t rawdata_cc_q_offset = 0;

	NVT_LOG("++\n");

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR,
		ts->mmap->DIFF_BTN_PIPE1_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR,
		ts->mmap->DIFF_BTN_PIPE0_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			if (ts->carrier_system) {
				xdata_tmp = xdata[iArrayIndex];
				RawData_FW_CC_I[iArrayIndex] =
					(uint8_t)(xdata_tmp & 0xFF);
				RawData_FW_CC_Q[iArrayIndex] =
					(uint8_t)((xdata_tmp >> 8) & 0xFF);
			} else {
				xdata[iArrayIndex] =
					(int16_t)xdata[iArrayIndex];
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		if (ts->carrier_system) {
			xdata_tmp = xdata[iArrayIndex];
			RawData_FW_CC_I[iArrayIndex] =
				(uint8_t)(xdata_tmp & 0xFF);
			RawData_FW_CC_Q[iArrayIndex] =
				(uint8_t)((xdata_tmp >> 8) & 0xFF);
		} else {
			xdata[iArrayIndex] = (int16_t)xdata[iArrayIndex];
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	pr_info("%s:\n", __func__);
	if (ts->carrier_system) {
		pr_info("%s:RawData_CC_I:\n", __func__);
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_FW_CC_I, X_Channel,
			Y_Channel, FW_CC_CSV_FILE, 0) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
#if TOUCH_KEY_NUM > 0
		rawdata_cc_q_offset = Y_Channel * X_Channel * 7 +
			Y_Channel * 2 + Key_Channel * 7 + 2;
#else
		rawdata_cc_q_offset = Y_Channel * X_Channel * 7 +
			Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
		pr_info("%s:RawData_CC_Q:\n", __func__);
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_FW_CC_Q, X_Channel,
			Y_Channel, FW_CC_CSV_FILE, rawdata_cc_q_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
	} else {
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel,
			FW_CC_CSV_FILE, 0) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
	}

	NVT_LOG("--\n");

	return 0;
}

static void nvt_enable_noise_collect(int32_t frame_num)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable noise collect---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x47;
	buf[2] = 0xAA;
	buf[3] = frame_num;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
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
	if (nvt_clear_fw_status())
		return -EAGAIN;

	frame_num = PS_Config_Diff_Test_Frame / 10;
	if (frame_num <= 0)
		frame_num = 1;
	pr_info("%s: frame_num=%d\n", __func__, frame_num);
	nvt_enable_noise_collect(frame_num);
	// need wait PS_Config_Diff_Test_Frame * 8.3ms
	msleep(frame_num * 83);

	if (nvt_polling_hand_shake_status())
		return -EAGAIN;

	if (nvt_get_fw_info())
		return -EAGAIN;

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR,
		ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR,
		ts->mmap->DIFF_BTN_PIPE1_ADDR);

	nvt_get_mdata(xdata, &x_num, &y_num);

	for (y = 0; y < y_num; y++) {
		for (x = 0; x < x_num; x++) {
			iArrayIndex = y * x_num + x;
			if (ts->carrier_system) {
				RawData_Diff_Max[iArrayIndex] =
					(uint16_t)xdata[iArrayIndex];
				RawData_Diff_Min[iArrayIndex] = 0;
			} else {
				RawData_Diff_Max[iArrayIndex] =
					(int8_t)((xdata[iArrayIndex] >> 8)
						& 0xFF);
				RawData_Diff_Min[iArrayIndex] =
					(int8_t)(xdata[iArrayIndex] & 0xFF);
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		if (ts->carrier_system) {
			RawData_Diff_Max[iArrayIndex] =
				(uint16_t)xdata[iArrayIndex];
			RawData_Diff_Min[iArrayIndex] = 0;
		} else {
			RawData_Diff_Max[iArrayIndex] =
				(int8_t)((xdata[iArrayIndex] >> 8) & 0xFF);
			RawData_Diff_Min[iArrayIndex] =
				(int8_t)(xdata[iArrayIndex] & 0xFF);
		}
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	pr_info("%s:RawData_Diff_Max:\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(RawData_Diff_Max, X_Channel,
		Y_Channel, NOISE_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}

	if (!ts->carrier_system) {
#if TOUCH_KEY_NUM > 0
		rawdata_diff_min_offset =
			Y_Channel * X_Channel * 7 +
			Y_Channel * 2 + Key_Channel * 7 + 2;
#else
		rawdata_diff_min_offset =
			Y_Channel * X_Channel * 7 + Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
		pr_info("%s:RawData_Diff_Min:\n", __func__);
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_Diff_Min, X_Channel,
			Y_Channel, NOISE_TEST_CSV_FILE,
			rawdata_diff_min_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
	}

	NVT_LOG("--\n");

	return 0;
}

static void nvt_enable_open_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable open test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x45;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---enable short test---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x43;
	buf[2] = 0xAA;
	buf[3] = 0x02;
	buf[4] = 0x00;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);
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
	if (nvt_clear_fw_status())
		return -EAGAIN;

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status())
		return -EAGAIN;

#if TOUCH_KEY_NUM > 0
	rawdata_buf =
		kzalloc((IC_X_CFG_SIZE * IC_Y_CFG_SIZE + IC_KEY_CFG_SIZE) * 2,
			GFP_KERNEL);
#else
	rawdata_buf =
		kzalloc(IC_X_CFG_SIZE * IC_Y_CFG_SIZE * 2, GFP_KERNEL);
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
		buf[0] = 0xFF;
		buf[1] = (uint8_t)(
			((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) >> 16)
			& 0xFF);
		buf[2] = (uint8_t)(
			((raw_pipe_addr + y * IC_X_CFG_SIZE * 2) >> 8)
			& 0xFF);
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
		buf[0] = (uint8_t)(
			(raw_pipe_addr + y * IC_X_CFG_SIZE * 2) & 0xFF);
		CTP_I2C_READ(ts->client, I2C_FW_Address,
			buf, IC_X_CFG_SIZE * 2 + 1);
		memcpy(rawdata_buf + y * IC_X_CFG_SIZE * 2,
			buf + 1, IC_X_CFG_SIZE * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (nvt_get_fw_pipe() == 0)
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	else
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	//---change xdata index---
	buf[0] = 0xFF;
	buf[1] = (uint8_t)((raw_btn_pipe_addr >> 16) & 0xFF);
	buf[2] = (uint8_t)((raw_btn_pipe_addr >> 8) & 0xFF);
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, IC_KEY_CFG_SIZE * 2 + 1);
	memcpy(rawdata_buf + IC_Y_CFG_SIZE * IC_X_CFG_SIZE * 2,
		buf + 1, IC_KEY_CFG_SIZE * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < IC_Y_CFG_SIZE; y++) {
		for (x = 0; x < IC_X_CFG_SIZE; x++) {
			if ((AIN_Y[y] != 0xFF) && (AIN_X[x] != 0xFF)) {
				xdata[AIN_Y[y] * X_Channel + AIN_X[x]] =
					(int16_t)(
					(rawdata_buf[
						(y * IC_X_CFG_SIZE + x) * 2] +
					256 * rawdata_buf[
						(y * IC_X_CFG_SIZE + x)
							* 2 + 1]));
			}
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < IC_KEY_CFG_SIZE; k++) {
		if (AIN_KEY[k] != 0xFF)
			xdata[Y_Channel * X_Channel + AIN_KEY[k]] =
			(int16_t)(rawdata_buf[
				(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2]
			+ 256 * rawdata_buf[
				(IC_Y_CFG_SIZE * IC_X_CFG_SIZE + k) * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	kfree(rawdata_buf);
	rawdata_buf = NULL;

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);


	pr_info("%s:RawData_Open\n", __func__);
	// Save RawData to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel,
		OPEN_TEST_CSV_FILE, 0) < 0) {
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
	uint32_t rawdata_short_base_offset = 0;

	NVT_LOG("++\n");

	//---Enter Test Mode---
	if (nvt_clear_fw_status())
		return -EAGAIN;

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status())
		return -EAGAIN;

#if TOUCH_KEY_NUM > 0
	rawdata_buf = kzalloc(
		(X_Channel * Y_Channel + Key_Channel) * 2,
		GFP_KERNEL);
#else
	rawdata_buf = kzalloc(X_Channel * Y_Channel * 2, GFP_KERNEL);
#endif /* #if TOUCH_KEY_NUM > 0 */
	if (!rawdata_buf) {
		NVT_ERR("kzalloc for rawdata_buf failed!\n");
		return -ENOMEM;
	}

	if (ts->carrier_system) {
		// to get short diff rawdata at pipe0
		raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
	} else {
		if (nvt_get_fw_pipe() == 0)
			raw_pipe_addr = ts->mmap->RAW_PIPE0_ADDR;
		else
			raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;
	}

	for (y = 0; y < Y_Channel; y++) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = (uint8_t)(((raw_pipe_addr + y * X_Channel * 2) >> 16)
			& 0xFF);
		buf[2] = (uint8_t)(((raw_pipe_addr + y * X_Channel * 2) >> 8)
			& 0xFF);
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
		buf[0] = (uint8_t)((raw_pipe_addr + y * X_Channel * 2) & 0xFF);
		CTP_I2C_READ(ts->client, I2C_FW_Address,
			buf, X_Channel * 2 + 1);
		memcpy(rawdata_buf + y * X_Channel * 2, buf + 1, X_Channel * 2);
	}
#if TOUCH_KEY_NUM > 0
	if (ts->carrier_system) {
		// to get button short diff rawdata at pipe0
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
	} else {
		if (nvt_get_fw_pipe() == 0)
			raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE0_ADDR;
		else
			raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;
	}

    //---change xdata index---
	buf[0] = 0xFF;
	buf[1] = (uint8_t)((raw_btn_pipe_addr >> 16) & 0xFF);
	buf[2] = (uint8_t)((raw_btn_pipe_addr >> 8) & 0xFF);
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, Key_Channel * 2 + 1);
	memcpy(rawdata_buf + Y_Channel * X_Channel * 2,
		buf + 1, Key_Channel * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

	for (y = 0; y < Y_Channel; y++) {
		for (x = 0; x < X_Channel; x++) {
			iArrayIndex = y * X_Channel + x;
			xdata[iArrayIndex] =
				(int16_t)(rawdata_buf[iArrayIndex * 2] +
				256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = Y_Channel * X_Channel + k;
		xdata[iArrayIndex] =
			(int16_t)(rawdata_buf[iArrayIndex * 2] +
			256 * rawdata_buf[iArrayIndex * 2 + 1]);
	}
#endif /* #if TOUCH_KEY_NUM > 0 */

	// for carrier sensing system to get short baseline rawdata
	if (ts->carrier_system) {
		// to get short baseline rawdata at pipe1
		raw_pipe_addr = ts->mmap->RAW_PIPE1_ADDR;

		for (y = 0; y < Y_Channel; y++) {
			//---change xdata index---
			buf[0] = 0xFF;
			buf[1] = (uint8_t)(
				((raw_pipe_addr + y * X_Channel * 2) >> 16)
					& 0xFF);
			buf[2] = (uint8_t)(
				((raw_pipe_addr + y * X_Channel * 2) >> 8)
					& 0xFF);
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
			buf[0] = (uint8_t)((raw_pipe_addr +
				y * X_Channel * 2) & 0xFF);
			CTP_I2C_READ(ts->client, I2C_FW_Address,
				buf, X_Channel * 2 + 1);
			memcpy(rawdata_buf + y * X_Channel * 2,
				buf + 1, X_Channel * 2);
		}
#if TOUCH_KEY_NUM > 0
		// to get button short baseline rawdata at pipe1
		raw_btn_pipe_addr = ts->mmap->RAW_BTN_PIPE1_ADDR;

	    //---change xdata index---
		buf[0] = 0xFF;
		buf[1] = (uint8_t)((raw_btn_pipe_addr >> 16) & 0xFF);
		buf[2] = (uint8_t)((raw_btn_pipe_addr >> 8) & 0xFF);
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
		buf[0] = (uint8_t)(raw_btn_pipe_addr & 0xFF);
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf,
			Key_Channel * 2 + 1);
		memcpy(rawdata_buf + Y_Channel * X_Channel * 2,
			buf + 1, Key_Channel * 2);
#endif /* #if TOUCH_KEY_NUM > 0 */

		for (y = 0; y < Y_Channel; y++) {
			for (x = 0; x < X_Channel; x++) {
				iArrayIndex = y * X_Channel + x;
				RawData_Short_Base[iArrayIndex] =
					(int16_t)(rawdata_buf[iArrayIndex * 2]
					+ 256 *
					rawdata_buf[iArrayIndex * 2 + 1]);
			}
		}
#if TOUCH_KEY_NUM > 0
		for (k = 0; k < Key_Channel; k++) {
			iArrayIndex = Y_Channel * X_Channel + k;
			RawData_Short_Base[iArrayIndex] =
				(int16_t)(rawdata_buf[iArrayIndex * 2] +
				256 * rawdata_buf[iArrayIndex * 2 + 1]);
		}
#endif /* #if TOUCH_KEY_NUM > 0 */
	}

	kfree(rawdata_buf);
	rawdata_buf = NULL;

	//---Leave Test Mode---
	nvt_change_mode(NORMAL_MODE);

	if (ts->carrier_system)
		pr_info("%s:RawData_Short_Diff:\n", __func__);
	else
		pr_info("%s:RawData_Short\n", __func__);
	// Save Rawdata to CSV file
	if (nvt_save_rawdata_to_csv(xdata, X_Channel, Y_Channel,
		SHORT_TEST_CSV_FILE, 0) < 0) {
		NVT_ERR("save rawdata to CSV file failed\n");
		return -EAGAIN;
	}
	if (ts->carrier_system) {
#if TOUCH_KEY_NUM > 0
		rawdata_short_base_offset = Y_Channel * X_Channel * 7 +
					Y_Channel * 2 + Key_Channel * 7 + 2;
#else
		rawdata_short_base_offset = Y_Channel * X_Channel * 7 +
					Y_Channel * 2;
#endif /* #if TOUCH_KEY_NUM > 0 */
		pr_info("%s:RawData_Short_Base:\n", __func__);
		// Save Rawdata to CSV file
		if (nvt_save_rawdata_to_csv(RawData_Short_Base,
			X_Channel, Y_Channel, SHORT_TEST_CSV_FILE,
			rawdata_short_base_offset) < 0) {
			NVT_ERR("save rawdata to CSV file failed\n");
			return -EAGAIN;
		}
	}

	NVT_LOG("--\n");

	return 0;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen raw data test function.
 *
 * return:
 *	Executive outcomes. 0---passed. negative---failed.
 *******************************************************/
static int32_t RawDataTest_Sub(int32_t rawdata[], uint8_t RecordResult[],
	uint8_t x_ch, uint8_t y_ch, int32_t Rawdata_Limit_Postive,
	int32_t Rawdata_Limit_Negative,
	int32_t Rawdata_Limit_Key_Postive, int32_t Rawdata_Limit_Key_Negative)
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

			RecordResult[iArrayIndex] = 0x00;
			// default value for PASS

			if (rawdata[iArrayIndex] > Rawdata_Limit_Postive)
				RecordResult[iArrayIndex] |= 0x01;

			if (rawdata[iArrayIndex] < Rawdata_Limit_Negative)
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if (rawdata[iArrayIndex] > Rawdata_Limit_Key_Postive)
			RecordResult[iArrayIndex] |= 0x01;

		if (rawdata[iArrayIndex] < Rawdata_Limit_Key_Negative)
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

	if (isPass == false)
		return -1; // FAIL
	return 0; // PASS
}

/*******************************************************
 * Description:
 *	Novatek touchscreen raw data test for each single point function.
 *
 * return:
 *	Executive outcomes. 0---passed. negative---failed.
 *******************************************************/
static int32_t RawDataTest_SinglePoint_Sub(int32_t rawdata[],
	uint8_t RecordResult[], uint8_t x_ch, uint8_t y_ch,
	int32_t Rawdata_Limit_Postive[], int32_t Rawdata_Limit_Negative[])
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

			RecordResult[iArrayIndex] = 0x00;
			// default value for PASS

			if (rawdata[iArrayIndex] >
				Rawdata_Limit_Postive[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x01;

			if (rawdata[iArrayIndex] <
				Rawdata_Limit_Negative[iArrayIndex])
				RecordResult[iArrayIndex] |= 0x02;
		}
	}
#if TOUCH_KEY_NUM > 0
	for (k = 0; k < Key_Channel; k++) {
		iArrayIndex = y_ch * x_ch + k;

		RecordResult[iArrayIndex] = 0x00; // default value for PASS

		if (rawdata[iArrayIndex] > Rawdata_Limit_Postive[iArrayIndex])
			RecordResult[iArrayIndex] |= 0x01;

		if (rawdata[iArrayIndex] < Rawdata_Limit_Negative[iArrayIndex])
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

	if (isPass == false)
		return -1; // FAIL
	return 0; // PASS
}

/*******************************************************
 * Description:
 *	Novatek touchscreen print self-test result function.
 *
 * return:
 *	n.a.
 *******************************************************/
void print_selftest_result(struct seq_file *m, int32_t TestResult,
uint8_t RecordResult[], int32_t rawdata[], uint8_t x_len, uint8_t y_len)
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
				nvt_mp_seq_printf(m, "0x%02X, ",
					RecordResult[iArrayIndex]);
			}
			nvt_mp_seq_printf(m, "\n");
		}
#if TOUCH_KEY_NUM > 0
		for (k = 0; k < Key_Channel; k++) {
			iArrayIndex = y_len * x_len + k;
			nvt_mp_seq_printf(m, "0x%02X, ",
				RecordResult[iArrayIndex]);
		}
		nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
		nvt_mp_seq_printf(m, "ReadData:\n");
		for (i = 0; i < y_len; i++) {
			for (j = 0; j < x_len; j++) {
				iArrayIndex = i * x_len + j;
				nvt_mp_seq_printf(m, "%5d, ",
					rawdata[iArrayIndex]);
			}
			nvt_mp_seq_printf(m, "\n");
		}
#if TOUCH_KEY_NUM > 0
		for (k = 0; k < Key_Channel; k++) {
			iArrayIndex = y_len * x_len + k;
			nvt_mp_seq_printf(m, "%5d, ", rawdata[iArrayIndex]);
		}
		nvt_mp_seq_printf(m, "\n");
#endif /* #if TOUCH_KEY_NUM > 0 */
		break;
	}
	nvt_mp_seq_printf(m, "\n");
}

/*******************************************************
 * Description:
 *	Novatek touchscreen self-test sequence print show
 *	function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t c_show_selftest(struct seq_file *m, void *v)
{
	NVT_LOG("++\n");

	nvt_mp_seq_printf(m, "FW Version: %d\n\n", ts->fw_ver);

	nvt_mp_seq_printf(m, "Short Test");
	if ((TestResult_Short == 0) || (TestResult_Short == 1)) {
		print_selftest_result(m,
			TestResult_Short,
			RecordResult_Short,
			RawData_Short, X_Channel, Y_Channel);
	} else { // TestResult_Short is -1
		if (ts->carrier_system) {
			nvt_mp_seq_printf(m, " FAIL!\n");
			if (TestResult_Short_Diff == -1) {
				nvt_mp_seq_printf(m, "Short Diff");
				print_selftest_result(m,
					TestResult_Short_Diff,
					RecordResult_Short_Diff,
					RawData_Short_Diff,
					X_Channel, Y_Channel);
			}
			if (TestResult_Short_Base == -1) {
				nvt_mp_seq_printf(m, "Short Base");
				print_selftest_result(m,
					TestResult_Short_Base,
					RecordResult_Short_Base,
					RawData_Short_Base,
					X_Channel, Y_Channel);
			}
		} else {
			print_selftest_result(m, TestResult_Short,
				RecordResult_Short, RawData_Short,
				X_Channel, Y_Channel);
		}
	}

	nvt_mp_seq_printf(m, "Open Test");
	print_selftest_result(m, TestResult_Open,
		RecordResult_Open, RawData_Open,
		X_Channel, Y_Channel);

	nvt_mp_seq_printf(m, "FW Rawdata Test");
	if ((TestResult_FW_Rawdata == 0) || (TestResult_FW_Rawdata == 1)) {
		print_selftest_result(m,
			TestResult_FWMutual,
			RecordResult_FWMutual,
			RawData_FWMutual,
			X_Channel, Y_Channel);
	} else { // TestResult_FW_Rawdata is -1
		nvt_mp_seq_printf(m, " FAIL!\n");
		if (TestResult_FWMutual == -1) {
			nvt_mp_seq_printf(m, "FW Mutual");
			print_selftest_result(m,
				TestResult_FWMutual,
				RecordResult_FWMutual,
				RawData_FWMutual,
				X_Channel, Y_Channel);
		}
		if (TestResult_FW_CC == -1) {
			if (ts->carrier_system) {
				if (TestResult_FW_CC_I == -1) {
					nvt_mp_seq_printf(m, "FW CC_I");
					print_selftest_result(m,
						TestResult_FW_CC_I,
						RecordResult_FW_CC_I,
						RawData_FW_CC_I,
						X_Channel, Y_Channel);
				}
				if (TestResult_FW_CC_Q == -1) {
					nvt_mp_seq_printf(m, "FW CC_Q");
					print_selftest_result(m,
						TestResult_FW_CC_Q,
						RecordResult_FW_CC_Q,
						RawData_FW_CC_Q,
						X_Channel, Y_Channel);
				}
			} else {
				nvt_mp_seq_printf(m, "FW CC");
				print_selftest_result(m, TestResult_FW_CC,
					RecordResult_FW_CC, RawData_FW_CC,
					X_Channel, Y_Channel);
			}
		}
	}

	nvt_mp_seq_printf(m, "Noise Test");
	if ((TestResult_Noise == 0) || (TestResult_Noise == 1)) {
		print_selftest_result(m, TestResult_FW_DiffMax,
			RecordResult_FW_DiffMax, RawData_Diff_Max,
			X_Channel, Y_Channel);
	} else { // TestResult_Noise is -1
		nvt_mp_seq_printf(m, " FAIL!\n");

		if (TestResult_FW_DiffMax == -1) {
			nvt_mp_seq_printf(m, "FW Diff Max");
			print_selftest_result(m, TestResult_FW_DiffMax,
				RecordResult_FW_DiffMax, RawData_Diff_Max,
				X_Channel, Y_Channel);
		}
		if (TestResult_FW_DiffMin == -1) {
			nvt_mp_seq_printf(m, "FW Diff Min");
			print_selftest_result(m, TestResult_FW_DiffMin,
				RecordResult_FW_DiffMin, RawData_Diff_Min,
				X_Channel, Y_Channel);
		}
	}

	nvt_mp_test_result_printed = 1;

	NVT_LOG("--\n");

	return 0;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen self-test sequence print start
 *	function.
 *
 * return:
 *	Executive outcomes. 1---call next function.
 *	NULL---not call next function and sequence loop
 *	stop.
 *******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen self-test sequence print next
 *	function.
 *
 * return:
 *	Executive outcomes. NULL---no next and call sequence
 *	stop function.
 *******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen self-test sequence print stop
 *	function.
 *
 * return:
 *	n.a.
 *******************************************************/
static void c_stop(struct seq_file *m, void *v)
{

}

const struct seq_operations nvt_selftest_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show_selftest
};

/*******************************************************
 * Description:
 *	Novatek touchscreen /proc/nvt_selftest open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/
static int32_t nvt_selftest_open(struct inode *inode, struct file *file)
{
	TestResult_Short = 0;
	TestResult_Short_Diff = 0;
	TestResult_Short_Base = 0;
	TestResult_Open = 0;
	TestResult_FW_Rawdata = 0;
	TestResult_FWMutual = 0;
	TestResult_FW_CC = 0;
	TestResult_FW_CC_I = 0;
	TestResult_FW_CC_Q = 0;
	TestResult_Noise = 0;
	TestResult_FW_DiffMax = 0;
	TestResult_FW_DiffMin = 0;

	NVT_LOG("++\n");

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	//---Print Test Criteria---
	nvt_print_criteria();

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

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		NVT_ERR("get fw info failed!\n");
		return -EAGAIN;
	}

	//---FW Rawdata Test---
	if (nvt_read_baseline(RawData_FWMutual) != 0) {
		TestResult_FWMutual = 1;
	} else {
		TestResult_FWMutual =
			RawDataTest_SinglePoint_Sub(RawData_FWMutual,
				RecordResult_FWMutual,
				X_Channel, Y_Channel,
				PS_Config_Lmt_FW_Rawdata_P,
				PS_Config_Lmt_FW_Rawdata_N);
	}
	if (nvt_read_CC(RawData_FW_CC) != 0) {
		TestResult_FW_CC = 1;
		if (ts->carrier_system) {
			TestResult_FW_CC_I = 1;
			TestResult_FW_CC_Q = 1;
		}
	} else {
		if (ts->carrier_system) {
			TestResult_FW_CC_I =
				RawDataTest_Sub(RawData_FW_CC_I,
					RecordResult_FW_CC_I,
					X_Channel, Y_Channel,
					PS_Config_Lmt_FW_CC_I_P,
					PS_Config_Lmt_FW_CC_I_N,
					PS_Config_Lmt_Key_FW_CC_I_P,
					PS_Config_Lmt_Key_FW_CC_I_N);
			TestResult_FW_CC_Q =
				RawDataTest_Sub(RawData_FW_CC_Q,
					RecordResult_FW_CC_Q,
					X_Channel, Y_Channel,
					PS_Config_Lmt_FW_CC_Q_P,
					PS_Config_Lmt_FW_CC_Q_N,
					PS_Config_Lmt_Key_FW_CC_Q_P,
					PS_Config_Lmt_Key_FW_CC_Q_N);
			if ((TestResult_FW_CC_I == -1) ||
				(TestResult_FW_CC_Q == -1))
				TestResult_FW_CC = -1;
			else
				TestResult_FW_CC = 0;
		} else {
			TestResult_FW_CC =
				RawDataTest_Sub(RawData_FW_CC,
					RecordResult_FW_CC,
					X_Channel, Y_Channel,
					PS_Config_Lmt_FW_CC_P,
					PS_Config_Lmt_FW_CC_N,
					PS_Config_Lmt_Key_FW_CC_P,
					PS_Config_Lmt_Key_FW_CC_N);
		}
	}

	if ((TestResult_FWMutual == 1) || (TestResult_FW_CC == 1)) {
		TestResult_FW_Rawdata = 1;
	} else {
		if ((TestResult_FWMutual == -1) ||
			(TestResult_FW_CC == -1))
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
		TestResult_FW_DiffMax =
			RawDataTest_Sub(RawData_Diff_Max,
				RecordResult_FW_DiffMax,
				X_Channel, Y_Channel,
				PS_Config_Lmt_FW_Diff_P,
				PS_Config_Lmt_FW_Diff_N,
				PS_Config_Lmt_Key_FW_Diff_P,
				PS_Config_Lmt_Key_FW_Diff_N);

		// for carrier sensing system, only positive noise data
		if (ts->carrier_system) {
			TestResult_FW_DiffMin = 0;
		} else {
			TestResult_FW_DiffMin =
				RawDataTest_Sub(RawData_Diff_Min,
					RecordResult_FW_DiffMin,
					X_Channel, Y_Channel,
					PS_Config_Lmt_FW_Diff_P,
					PS_Config_Lmt_FW_Diff_N,
					PS_Config_Lmt_Key_FW_Diff_P,
					PS_Config_Lmt_Key_FW_Diff_N);
		}

		if ((TestResult_FW_DiffMax == -1) ||
			(TestResult_FW_DiffMin == -1))
			TestResult_Noise = -1;
		else
			TestResult_Noise = 0;
	}

	//--Short Test---
	if (nvt_read_fw_short(RawData_Short) != 0) {
		TestResult_Short = 1; // 1:ERROR
		if (ts->carrier_system) {
			TestResult_Short_Diff = 1;
			TestResult_Short_Base = 1;
		}
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		if (ts->carrier_system) {
			TestResult_Short_Diff =
				RawDataTest_Sub(RawData_Short_Diff,
					RecordResult_Short_Diff,
					X_Channel, Y_Channel,
					PS_Config_Lmt_Short_Diff_P,
					PS_Config_Lmt_Short_Diff_N,
					PS_Config_Lmt_Key_Short_Diff_P,
					PS_Config_Lmt_Key_Short_Diff_N);
			TestResult_Short_Base =
				RawDataTest_Sub(RawData_Short_Base,
					RecordResult_Short_Base,
					X_Channel, Y_Channel,
					PS_Config_Lmt_Short_Base_P,
					PS_Config_Lmt_Short_Base_N,
					PS_Config_Lmt_Key_Short_Base_P,
					PS_Config_Lmt_Key_Short_Base_N);

			if ((TestResult_Short_Diff == -1) ||
				(TestResult_Short_Base == -1))
				TestResult_Short = -1;
			else
				TestResult_Short = 0;
		} else {
			TestResult_Short =
				RawDataTest_Sub(RawData_Short,
					RecordResult_Short,
					X_Channel, Y_Channel,
					PS_Config_Lmt_Short_Rawdata_P,
					PS_Config_Lmt_Short_Rawdata_N,
					PS_Config_Lmt_Key_Short_Rawdata_P,
					PS_Config_Lmt_Key_Short_Rawdata_N);
		}
	}

	//---Open Test---
	if (nvt_read_fw_open(RawData_Open) != 0) {
		TestResult_Open = 1;    // 1:ERROR
	} else {
		//---Self Test Check --- // 0:PASS, -1:FAIL
		TestResult_Open =
			RawDataTest_SinglePoint_Sub(RawData_Open,
				RecordResult_Open, X_Channel, Y_Channel,
				PS_Config_Lmt_Open_Rawdata_P,
				PS_Config_Lmt_Open_Rawdata_N);
	}

	//---Reset IC---
	nvt_bootloader_reset();

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
 * Description:
 *	Novatek touchscreen parse AIN setting for array type.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_mp_parse_ain(struct device_node *np, const char *name,
	uint8_t *array, int32_t size)
{
	struct property *data;
	int32_t len, ret;
	int32_t tmp[40];
	int32_t i;

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len != size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, tmp, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return;
		}

		for (i = 0; i < len; i++)
			array[i] = tmp[i];

#if NVT_DEBUG
		pr_info("[NVT-ts] %s = ", name);
		for (i = 0; i < len; i++)
			pr_info("%02d ", array[i]);
		pr_info("\n");
#endif
	}
}

/*******************************************************
 * Description:
 *	Novatek touchscreen parse criterion for u32 type.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_mp_parse_u32(struct device_node *np, const char *name, int32_t *para)
{
	int32_t ret;

	ret = of_property_read_u32(np, name, para);
	if (ret)
		NVT_ERR("error reading %s. ret=%d\n", name, ret);
	else {
#if NVT_DEBUG
		NVT_LOG("%s=%d\n", name, *para);
#endif
	}
}

/*******************************************************
 * Description:
 *	Novatek touchscreen parse criterion for array type.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_mp_parse_array(struct device_node *np,
	const char *name, int32_t *array,
		int32_t size)
{
	struct property *data;
	int32_t len, ret;
#if NVT_DEBUG
	int32_t i, j, iArrayIndex = 0;
#endif

	data = of_find_property(np, name, &len);
	len /= sizeof(u32);
	if ((!data) || (!len) || (len < size)) {
		NVT_ERR("error find %s. len=%d\n", name, len);
	} else {
		NVT_LOG("%s. len=%d\n", name, len);
		ret = of_property_read_u32_array(np, name, array, len);
		if (ret) {
			NVT_ERR("error reading %s. ret=%d\n", name, ret);
			return;
		}

#if NVT_DEBUG
		NVT_LOG("%s =\n", name);
		for (j = 0; j < Y_Channel; j++) {
			pr_info("[NVT-ts] ");
			for (i = 0; i < X_Channel; i++) {
				iArrayIndex = j * X_Channel + i;
				pr_info("%5d, ", array[iArrayIndex]);
			}
			pr_info("\n");
		}
#if TOUCH_KEY_NUM > 0
		pr_info("[NVT-ts] ");
		for (i = 0; i < Key_Channel; i++) {
			iArrayIndex++;
			pr_info("%5d, ", array[iArrayIndex]);
		}
		pr_info("\n");
#endif
#endif
	}
}

/*******************************************************
 * Description:
 *	Novatek touchscreen parse device tree mp function.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_mp_parse_dt(struct device_node *root, const char *node_compatible)
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

	/* MP Config*/
	nvt_mp_parse_u32(np, "IC_X_CFG_SIZE", &IC_X_CFG_SIZE);

	nvt_mp_parse_u32(np, "IC_Y_CFG_SIZE", &IC_Y_CFG_SIZE);

#if TOUCH_KEY_NUM > 0
	nvt_mp_parse_u32(np, "IC_KEY_CFG_SIZE", &IC_KEY_CFG_SIZE);
#endif

	nvt_mp_parse_u32(np, "X_Channel", &X_Channel);

	nvt_mp_parse_u32(np, "Y_Channel", &Y_Channel);

	nvt_mp_parse_ain(np, "AIN_X", AIN_X, IC_X_CFG_SIZE);

	nvt_mp_parse_ain(np, "AIN_Y", AIN_Y, IC_Y_CFG_SIZE);

#if TOUCH_KEY_NUM > 0
	nvt_mp_parse_ain(np, "AIN_KEY", AIN_KEY, IC_KEY_CFG_SIZE);
#endif

	/* MP Criteria */
	if (!ts->carrier_system) {
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Rawdata_P",
			&PS_Config_Lmt_Short_Rawdata_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Rawdata_N",
			&PS_Config_Lmt_Short_Rawdata_N);

#if TOUCH_KEY_NUM > 0
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Rawdata_P",
		&PS_Config_Lmt_Key_Short_Rawdata_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Rawdata_N",
			&PS_Config_Lmt_Key_Short_Rawdata_N);
#endif
	}

	if (ts->carrier_system) {
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Diff_P",
			&PS_Config_Lmt_Short_Diff_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Diff_N",
			&PS_Config_Lmt_Short_Diff_N);

#if TOUCH_KEY_NUM > 0
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Diff_P",
		&PS_Config_Lmt_Key_Short_Diff_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Diff_N",
			&PS_Config_Lmt_Key_Short_Diff_N);
#endif

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Base_P",
		&PS_Config_Lmt_Short_Base_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Short_Base_N",
			&PS_Config_Lmt_Short_Base_N);

#if TOUCH_KEY_NUM > 0
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Base_P",
		&PS_Config_Lmt_Key_Short_Base_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_Short_Base_N",
			&PS_Config_Lmt_Key_Short_Base_N);
#endif
	}

	nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_P",
		PS_Config_Lmt_Open_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel);

	nvt_mp_parse_array(np, "PS_Config_Lmt_Open_Rawdata_N",
		PS_Config_Lmt_Open_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel);

	nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_P",
		PS_Config_Lmt_FW_Rawdata_P,
			X_Channel * Y_Channel + Key_Channel);

	nvt_mp_parse_array(np, "PS_Config_Lmt_FW_Rawdata_N",
		PS_Config_Lmt_FW_Rawdata_N,
			X_Channel * Y_Channel + Key_Channel);

	nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_P",
		&PS_Config_Lmt_FW_CC_P);

	nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_N",
		&PS_Config_Lmt_FW_CC_N);

#if TOUCH_KEY_NUM > 0
	nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_P",
	&PS_Config_Lmt_Key_FW_CC_P);

	nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_N",
		&PS_Config_Lmt_Key_FW_CC_N);
#endif

	if (ts->carrier_system) {
		nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_I_P",
			&PS_Config_Lmt_FW_CC_I_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_I_N",
			&PS_Config_Lmt_FW_CC_I_N);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_Q_P",
			&PS_Config_Lmt_FW_CC_Q_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_CC_Q_N",
			&PS_Config_Lmt_FW_CC_Q_N);

#if TOUCH_KEY_NUM > 0
		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_I_P",
		&PS_Config_Lmt_Key_FW_CC_I_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_I_N",
			&PS_Config_Lmt_Key_FW_CC_I_N);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_Q_P",
			&PS_Config_Lmt_Key_FW_CC_Q_P);

		nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_CC_Q_N",
			&PS_Config_Lmt_Key_FW_CC_Q_N);
#endif
	}

	nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_Diff_P",
		&PS_Config_Lmt_FW_Diff_P);

	nvt_mp_parse_u32(np, "PS_Config_Lmt_FW_Diff_N",
		&PS_Config_Lmt_FW_Diff_N);

#if TOUCH_KEY_NUM > 0
	nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_Diff_P",
	&PS_Config_Lmt_Key_FW_Diff_P);

	nvt_mp_parse_u32(np, "PS_Config_Lmt_Key_FW_Diff_N",
		&PS_Config_Lmt_Key_FW_Diff_N);
#endif

	nvt_mp_parse_u32(np, "PS_Config_Diff_Test_Frame",
	&PS_Config_Diff_Test_Frame);

	NVT_LOG("Parse mp criteria done!\n");
}
#endif /* #ifdef CONFIG_OF */

/*******************************************************
 * Description:
 *	Novatek touchscreen MP function proc. file node
 *	initial function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_mp_proc_init(void)
{
	struct device_node *np = ts->client->dev.of_node;
	unsigned char mpcriteria[32] = {0};
	//novatek-mp-criteria-default

	NVT_proc_selftest_entry = proc_create("nvt_selftest",
		0444, NULL, &nvt_selftest_fops);
	if (NVT_proc_selftest_entry == NULL) {
		NVT_ERR("create /proc/nvt_selftest Failed!\n");
		return -1;
	}
	if (nvt_mp_buffer_init()) {
		NVT_ERR("Allocate mp memory failed\n");
		return -1;
	}
#ifdef CONFIG_OF
	/* Parsing criteria from dts */
	if (of_property_read_bool(np,
		"novatek,mp-support-dt")) {
		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is
		 * "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *     mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, ARRAY_SIZE(mpcriteria),
			"novatek-mp-criteria-%04X", ts->nvt_pid);

		nvt_mp_parse_dt(np, mpcriteria);
	} else {
		NVT_LOG("Not found novatek,mp-support-dt\n");
		NVT_LOG("use default setting\n");
	}
#endif
	NVT_LOG("create /proc/nvt_selftest Succeeded!\n");
	return 0;
}

#endif /* #if NVT_TOUCH_MP */
