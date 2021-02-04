/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 33334 $
 * $Date: 2018-09-04 19:17:10 +0800 (週二, 04 九月 2018) $
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

#include "nt36xxx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"

#define I2C_TANSFER_LENGTH  64

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

extern int32_t Init_BootLoader(void);
extern int32_t Resume_PD(void);

static uint8_t xdata_tmp[2048] = {0};
static int32_t xdata[2048] = {0};
static int32_t xdata_i[2048] = {0};
static int32_t xdata_q[2048] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
/* add touchpad information by zhangchaofan start, 2018-09-06*/
extern char g_lcd_id[128];
static uint8_t tp_maker_cg_lamination = 0;
static uint8_t display_maker = 0;
static uint8_t cg_ink_color = 0;
static uint8_t hw_version = 0;
static uint8_t project_id = 0;
static uint8_t cg_maker = 0;
static uint8_t reservation_byte = 0;
/* add touchpad information by zhangchaofan end, 2018-09-06*/

/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
		msleep(20);
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

	//NVT_LOG("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr, uint32_t xdata_btn_addr)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[I2C_TANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = ((head_addr + XDATA_SECTOR_SIZE * i) >> 16) & 0xFF;
		buf[2] = ((head_addr + XDATA_SECTOR_SIZE * i) >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read xdata by I2C_TANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / I2C_TANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = I2C_TANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, I2C_TANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < I2C_TANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + I2C_TANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + I2C_TANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = ((xdata_addr + data_len - residual_len) >> 16) & 0xFF;
		buf[2] = ((xdata_addr + data_len - residual_len) >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read xdata by I2C_TANSFER_LENGTH
		for (j = 0; j < (residual_len / I2C_TANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = I2C_TANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, I2C_TANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < I2C_TANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + I2C_TANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + I2C_TANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

#if TOUCH_KEY_NUM > 0
	//read button xdata : step3
	//---change xdata index---
	buf[0] = 0xFF;
	buf[1] = (xdata_btn_addr >> 16) & 0xFF;
	buf[2] = ((xdata_btn_addr >> 8) & 0xFF);
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, (TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		xdata[ts->x_num * ts->y_num + i] = (int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
	}
#endif

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data from IQ to rss function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata_rss(uint32_t xdata_i_addr, uint32_t xdata_q_addr, uint32_t xdata_btn_i_addr, uint32_t xdata_btn_q_addr)
{
	int i = 0;

	nvt_read_mdata(xdata_i_addr, xdata_btn_i_addr);
	memcpy(xdata_i, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));

	nvt_read_mdata(xdata_q_addr, xdata_btn_q_addr);
	memcpy(xdata_q, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));

	for (i = 0; i < (ts->x_num * ts->y_num + TOUCH_KEY_NUM); i++) {
		xdata[i] = (int32_t)int_sqrt((unsigned long)(xdata_i[i] * xdata_i[i]) + (unsigned long)(xdata_q[i] * xdata_q[i]));
	}
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
    *m_x_num = ts->x_num;
    *m_y_num = ts->y_num;
    memcpy(buf, xdata, ((ts->x_num * ts->y_num + TOUCH_KEY_NUM) * sizeof(int32_t)));
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num, ts->max_button_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

#if TOUCH_KEY_NUM > 0
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		seq_printf(m, "%5d, ", xdata[ts->x_num * ts->y_num + i]);
	}
	seq_puts(m, "\n");
#endif

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
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
	Novatek touchscreen xdata sequence print next
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
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (ts->carrier_system) {
		nvt_read_mdata_rss(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_Q_ADDR,
				ts->mmap->BASELINE_BTN_ADDR, ts->mmap->BASELINE_BTN_Q_ADDR);
	} else {
		nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);
	}

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (ts->carrier_system) {
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata_rss(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_PIPE0_Q_ADDR,
				ts->mmap->RAW_BTN_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_Q_ADDR);
		else
			nvt_read_mdata_rss(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_PIPE1_Q_ADDR,
				ts->mmap->RAW_BTN_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_Q_ADDR);
	} else {
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
		else
			nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);
	}

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (ts->carrier_system) {
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata_rss(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_PIPE0_Q_ADDR,
				ts->mmap->DIFF_BTN_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_Q_ADDR);
		else
			nvt_read_mdata_rss(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_PIPE1_Q_ADDR,
				ts->mmap->DIFF_BTN_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_Q_ADDR);
	} else {
		if (nvt_get_fw_pipe() == 0)
			nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
		else
			nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	}

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
static int32_t nvt_get_oem_data(uint8_t *data, uint32_t flash_address, int32_t size)
{
	uint8_t buf[64] = {0};
	uint8_t tmp_data[512] = {0};
	int32_t count_256 = 0;
	uint32_t cur_flash_addr = 0;
	uint32_t cur_sram_addr = 0;
	uint16_t checksum_get = 0;
	uint16_t checksum_cal = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t ret = 0;
	int32_t retry = 0;

	NVT_LOG("++\n");

	// maximum 256 bytes each read
	if (size % 256)
		count_256 = size / 256 + 1;
	else
		count_256 = size / 256;

get_oem_data_retry:
	nvt_sw_reset_idle();

	// Step 1: Initial BootLoader
	ret = Init_BootLoader();
	if (ret < 0) {
		goto get_oem_data_out;
	}

	// Step 2: Resume PD
	ret = Resume_PD();
	if (ret < 0) {
		goto get_oem_data_out;
	}

	// Step 3: Unlock
	buf[0] = 0x00;
	buf[1] = 0x35;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
	msleep(10);

	for (i = 0; i < count_256; i++) {
		cur_flash_addr = flash_address + i * 256;
		// Step 4: Flash Read Command
		buf[0] = 0x00;
		buf[1] = 0x03;
		buf[2] = ((cur_flash_addr >> 16) & 0xFF);
		buf[3] = ((cur_flash_addr >> 8) & 0xFF);
		buf[4] = (cur_flash_addr & 0xFF);
		buf[5] = 0x00;
		buf[6] = 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 7);
		msleep(10);
		// Check 0xAA (Read Command)
		buf[0] = 0x00;
		buf[2] = 0x00;
		CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (buf[1] != 0xAA) {
			NVT_ERR("Check 0xAA (Read Command) error!! status=0x%02X\n", buf[1]);
			ret = -1;
			goto get_oem_data_out;
		}
		msleep(10);

		// Step 5: Read Data and Checksum
		for (j = 0; j < ((256 / 32) + 1); j++) {
			cur_sram_addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR + j * 32;
			buf[0] = 0xFF;
			buf[1] = (cur_sram_addr >> 16) & 0xFF;
			buf[2] = (cur_sram_addr  >> 8) & 0xFF;
			CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

			buf[0] = cur_sram_addr & 0xFF;
			CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 33);

			memcpy(tmp_data + j * 32, buf + 1, 32);
		}
		// get checksum of the 256 bytes data read
		checksum_get = (uint16_t)((tmp_data[1] << 8) | tmp_data[0]);
		// calculate checksum of of the 256 bytes data read
		checksum_cal = (uint16_t)((cur_flash_addr >> 16) & 0xFF) + (uint16_t)((cur_flash_addr >> 8) & 0xFF) + (cur_flash_addr & 0xFF) + 0x00 + 0xFF;
		for (j = 0; j < 256; j++) {
			checksum_cal += tmp_data[j + 2];
		}
		checksum_cal = 65535 - checksum_cal + 1;
		//NVT_LOG("checksum_get = 0x%04X, checksum_cal = 0x%04X\n", checksum_get, checksum_cal);
		// compare the checksum got and calculated
		if (checksum_get != checksum_cal) {
			if (retry < 3) {
				retry++;
				goto get_oem_data_retry;
			} else {
				NVT_ERR("Checksum not match error! checksum_get=0x%04X, checksum_cal=0x%04X, i=%d\n", checksum_get, checksum_cal, i);
				ret = -2;
				goto get_oem_data_out;
			}
		}

		// Step 6: Remapping (Remove 2 Bytes Checksum)
		if ((i + 1) * 256 > size) {
			memcpy(data + i * 256, tmp_data + 2, size - i * 256);
		} else {
			memcpy(data + i * 256, tmp_data + 2, 256);
		}
	}

#if 0 // for debug
	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			printk("\n");
		printk("%02X ", data[i]);
	}
	printk("\n");
#endif // for debug

get_oem_data_out:
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);

	NVT_LOG("--\n");

	return ret;
}

/* add touchpad information by zhangchaofan start, 2018-09-06*/
static int32_t nvt_get_xiaomi_lockdown_info(void)
{
	uint8_t data_buf[8] = {0};
	int ret = 0;

	ret = nvt_get_oem_data(data_buf, 0x1E000, 8);

	if (ret < 0) {
		NVT_ERR("get oem data failed!\n");
	} else {
		tp_maker_cg_lamination = data_buf[0];
		NVT_LOG("The maker of Touch Panel & CG Lamination: 0x%02X\n", tp_maker_cg_lamination);
		display_maker = data_buf[1];
		NVT_LOG("Display maker: 0x%02X\n", display_maker);
		cg_ink_color = data_buf[2];
		NVT_LOG("CG ink color: 0x%02X\n", cg_ink_color);
		hw_version = data_buf[3];
		NVT_LOG("HW version: 0x%02X\n", hw_version);
		project_id = ((data_buf[4] << 8) | data_buf[5]);
		NVT_LOG("Project ID: 0x%04X\n", project_id);
		cg_maker = data_buf[6];
		NVT_LOG("CG maker: 0x%02X\n", cg_maker);
		reservation_byte = data_buf[7];
		NVT_LOG("Reservation byte: 0x%02X\n", reservation_byte);
	}

	return ret;
}

int lct_nvt_tp_info_node_init(void)
{
    char tp_info_buf[64];
    char tp_lockdown_info_buf[64];
    nvt_get_xiaomi_lockdown_info();
    memset(tp_info_buf, 0, sizeof(tp_info_buf));
    if (IS_ERR_OR_NULL(g_lcd_id)){
        NVT_ERR("g_lcd_id is ERROR!\n");
        goto tp_node_init;
    } else {
        LOGV("LCM information : %s\n", g_lcd_id);
        if (strstr(g_lcd_id,"nt36672a video mode dsi shenchao panel") != NULL) {
			if (reservation_byte == 0x00) {
				ts->touch_vendor_id = TP_VENDOR_EBBG;
				sprintf(tp_info_buf, "[Vendor]shenchao,[FW]0x%02x,[IC]nt36672a\n", ts->fw_ver);
			}
			else if (reservation_byte == 0x01) {
				ts->touch_vendor_id = TP_VENDOR_EBBG_2ND;
				sprintf(tp_info_buf, "[Vendor]shenchao_2nd,[FW]0x%02x,[IC]nt36672a\n", ts->fw_ver);
			}
			goto tp_node_init;
        } else if (strstr(g_lcd_id,"nt36672a video mode dsi tianma panel") != NULL) {
			sprintf(tp_info_buf, "[Vendor]tianma,[FW]0x%02x,[IC]nt36672a\n", ts->fw_ver);
			goto tp_node_init;
		} else {
			init_lct_tp_info(NULL, NULL);
			return -ENODEV;
        }
    }
tp_node_init:
    sprintf(tp_lockdown_info_buf, "%02X%02X%02X%02X%04X%02X%02X\n", tp_maker_cg_lamination, display_maker, cg_ink_color, hw_version, project_id, cg_maker, reservation_byte);
    init_lct_tp_info(tp_info_buf, tp_lockdown_info_buf);
    return 0;
}
/* add touchpad information by zhangchaofan end, 2018-09-06*/

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		NVT_ERR("create proc/nvt_fw_version Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/nvt_fw_version Succeeded!\n");
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/nvt_baseline Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/nvt_baseline Succeeded!\n");
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/nvt_raw Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/nvt_raw Succeeded!\n");
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/nvt_diff Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/nvt_diff Succeeded!\n");
	}

	return 0;
}
#endif
