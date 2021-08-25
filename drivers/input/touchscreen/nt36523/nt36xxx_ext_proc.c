/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * $Revision: 57674 $
 * $Date: 2020-03-02 11:16:20 +0800 (週一, 02 三月 2020) $
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

#include "nt36xxx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_PEN_DIFF "nvt_pen_diff"
#define NVT_PF_SWITCH "nvt_pf_switch"
#define NVT_SENSITIVITY_SWITCH "nvt_sensitivity_switch"
#define NVT_ER_RANGE_SWITCH "nvt_er_range_switch"
#define NVT_EDGE_REJECT_SWITCH "nvt_edge_reject_switch"
#define NVT_HAND_ONLY_SWITCH "nvt_hand_only_switch"
#define NVT_DRAG_LATENCY_SWITCH "nvt_drag_latency_switch"
#define NVT_SMALL_JITTER_SWITCH "nvt_small_jitter_switch"
#define NVT_GAME_MODE_SWITCH "nvt_game_mode_switch"
#define NVT_XIAOMI_LOCKDOWN_INFO "tp_lockdown_info"

#define BUS_TRANSFER_LENGTH  256

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[5000] = {0};
static int32_t xdata[2500] = {0};
static int32_t xdata_pen_tip_x[256] = {0};
static int32_t xdata_pen_tip_y[256] = {0};
static int32_t xdata_pen_ring_x[256] = {0};
static int32_t xdata_pen_ring_y[256] = {0};

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_pen_diff_entry;
static struct proc_dir_entry *NVT_proc_pf_switch_entry;
static struct proc_dir_entry *NVT_proc_sensitivity_switch_entry;
static struct proc_dir_entry *NVT_proc_er_range_switch_entry;
static struct proc_dir_entry *NVT_proc_edge_reject_switch_entry;
static struct proc_dir_entry *NVT_proc_hand_only_switch_entry;
static struct proc_dir_entry *NVT_proc_drag_latency_switch_entry;
static struct proc_dir_entry *NVT_proc_small_jitter_switch_entry;
static struct proc_dir_entry *NVT_proc_xiaomi_lockdown_info_entry;
static struct proc_dir_entry *NVT_proc_game_mode_switch_entry;

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
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_SPI_WRITE(ts->client, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		msleep(20);
	}
}

int32_t nvt_set_pen_inband_mode_1(uint8_t freq_idx, uint8_t x_term)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x02;
	buf[3] = freq_idx;
	buf[4] = x_term;
	CTP_SPI_WRITE(ts->client, buf, 5);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

int32_t nvt_set_pen_normal_mode(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 5;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0xC1;
	buf[2] = 0x04;
	CTP_SPI_WRITE(ts->client, buf, 3);

	for (i = 0; i < retry; i++) {
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
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
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_SPI_READ(ts->client, buf, 2);

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
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
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
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
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
	nvt_set_page(xdata_btn_addr);

	//---read data---
	buf[0] = (xdata_btn_addr & 0xFF);
	CTP_SPI_READ(ts->client, buf, (TOUCH_KEY_NUM * 2 + 1));

	//---2bytes-to-1data---
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		xdata[ts->x_num * ts->y_num + i] = (int16_t)(buf[1 + i * 2] + 256 * buf[1 + i * 2 + 1]);
	}
#endif

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
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
	Novatek touchscreen read and get number of meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		nvt_set_page(head_addr + XDATA_SECTOR_SIZE * i);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / BUS_TRANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by BUS_TRANSFER_LENGTH
		for (j = 0; j < (residual_len / BUS_TRANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = BUS_TRANSFER_LENGTH * j;
			CTP_SPI_READ(ts->client, buf, BUS_TRANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < BUS_TRANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + BUS_TRANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + BUS_TRANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		buffer[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
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
	Novatek pen 1D diff xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_pen_1d_diff_show(struct seq_file *m, void *v)
{
	int32_t i = 0;

	seq_printf(m, "Tip X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Tip Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_tip_y[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%5d, ", xdata_pen_ring_y[i]);
	}
	seq_puts(m, "\n");

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

const struct seq_operations nvt_pen_diff_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_pen_1d_diff_show
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

	nvt_read_mdata(ts->mmap->BASELINE_ADDR, ts->mmap->BASELINE_BTN_ADDR);

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

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);

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

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);

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

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_pen_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_pen_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

	NVT_LOG("++\n");

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	if (nvt_set_pen_inband_mode_1(0xFF, 0x00)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

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

	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_X_ADDR, xdata_pen_tip_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_TIP_Y_ADDR, xdata_pen_tip_y, ts->y_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_X_ADDR, xdata_pen_ring_x, ts->x_num);
	nvt_read_get_num_mdata(ts->mmap->PEN_1D_DIFF_RING_Y_ADDR, xdata_pen_ring_y, ts->y_num);

	nvt_change_mode(NORMAL_MODE);

	nvt_set_pen_normal_mode();

	nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);

	mutex_unlock(&ts->lock);

	NVT_LOG("--\n");

	return seq_open(file, &nvt_pen_diff_seq_ops);
}

static const struct file_operations nvt_pen_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_pen_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int nvt_xiaomi_lockdown_info_show(struct seq_file *m, void *v)
{
	u8 *lk = ts->lockdown_info;

	seq_printf(m, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
                	lk[0], lk[1], lk[2], lk[3], lk[4], lk[5], lk[6], lk[7]);
    return 0;
}

static int32_t nvt_xiaomi_lockdown_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvt_xiaomi_lockdown_info_show, NULL);
}

static const struct file_operations nvt_xiaomi_lockdown_info_fops = {
	.owner = THIS_MODULE,
	.open = nvt_xiaomi_lockdown_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int32_t nvt_set_pocket_palm_switch(uint8_t pocket_palm_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("set pocket palm switch: %d\n", pocket_palm_switch);

	msleep(35);

	/* ---set xdata index to EVENT BUF ADDR--- */
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_pocket_palm_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	if (pocket_palm_switch == 0) {
		/* pocket palm disable */
		buf[1] = 0x74;
	} else if (pocket_palm_switch == 1) {
		/* pocket palm enable */
		buf[1] = 0x73;
	} else {
		NVT_ERR("Invalid value! pocket_palm_switch = %d\n", pocket_palm_switch);
		ret = -EINVAL;
		goto nvt_set_pocket_palm_switch_out;
	}
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write pocket palm switch command fail!\n");
		goto nvt_set_pocket_palm_switch_out;
	}

nvt_set_pocket_palm_switch_out:
	NVT_LOG("%s --\n", __func__);
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_pf_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_pf_switch(uint8_t pf_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set pf switch: %d\n", pf_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_pf_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x70;
	buf[2] = pf_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write pf switch command fail!\n");
		goto nvt_set_pf_switch_out;
	}

nvt_set_pf_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_pf_switch(uint8_t *pf_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);
	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5D);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_pf_switch_out;
	}

	buf[0] = 0x5D;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read pf switch status fail!\n");
		goto nvt_get_pf_switch_out;
	}

	*pf_switch = (buf[1] & 0x07);
	NVT_LOG("pf_switch = %d\n", *pf_switch);

nvt_get_pf_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_pf_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t pf_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_pf_switch(&pf_switch);

	mutex_unlock(&ts->lock);

	//cnt = snprintf(buf, PAGE_SIZE - len, "pf_switch: %d\n", pf_switch);
	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "pf_switch: %d\n", pf_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_pf_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t pf_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if (tmp < 0 || tmp > 4) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	pf_switch = (uint8_t)tmp;
	NVT_LOG("pf_switch = %d\n", pf_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_pf_switch(pf_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_pf_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_pf_switch_proc_read,
	.write = nvt_pf_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_sensitivity_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_sensitivity_switch(uint8_t sensitivity_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set sensitivity switch: %d\n", sensitivity_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_sensitivity_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x71;
	buf[2] = sensitivity_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write sensitivity switch command fail!\n");
		goto nvt_set_sensitivity_switch_out;
	}

nvt_set_sensitivity_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_sensitivity_switch(uint8_t *sensitivity_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5D);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_sensitivity_switch_out;
	}

	buf[0] = 0x5D;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read sensitivity switch status fail!\n");
		goto nvt_get_sensitivity_switch_out;
	}

	*sensitivity_switch = ((buf[1] >> 3) & 0x07);
	NVT_LOG("sensitivity_switch = %d\n", *sensitivity_switch);

nvt_get_sensitivity_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_sensitivity_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t sensitivity_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_sensitivity_switch(&sensitivity_switch);

	mutex_unlock(&ts->lock);

	//cnt = snprintf(buf, PAGE_SIZE - len, "sensitivity_switch: %d\n", sensitivity_switch);
	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "sensitivity_switch: %d\n", sensitivity_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_sensitivity_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t sensitivity_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if (tmp < 0 || tmp > 4) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	sensitivity_switch = (uint8_t)tmp;
	NVT_LOG("sensitivity_switch = %d\n", sensitivity_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_sensitivity_switch(sensitivity_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_sensitivity_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_sensitivity_switch_proc_read,
	.write = nvt_sensitivity_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_er_range_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_er_range_switch(uint8_t er_range_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set er range switch: %d\n", er_range_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_er_range_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x72;
	buf[2] = er_range_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write er range switch command fail!\n");
		goto nvt_set_er_range_switch_out;
	}

nvt_set_er_range_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_er_range_switch(uint8_t *er_range_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5D);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_er_range_switch_out;
	}

	buf[0] = 0x5D;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read er range switch status fail!\n");
		goto nvt_get_er_range_switch_out;
	}

	*er_range_switch = ((buf[1] >> 6) & 0x03);
	NVT_LOG("er_range_switch = %d\n", *er_range_switch);

nvt_get_er_range_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_er_range_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t er_range_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_er_range_switch(&er_range_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "er_range_switch: %d\n", er_range_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_er_range_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t er_range_switch;
	char *tmp_buf = 0;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}
	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}

	if (tmp < 0 || tmp > 3) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	er_range_switch = (uint8_t)tmp;
	NVT_LOG("er_range_switch = %d\n", er_range_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_er_range_switch(er_range_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_er_range_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_er_range_switch_proc_read,
	.write = nvt_er_range_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_edge_reject_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_edge_reject_switch(uint8_t edge_reject_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set edge reject switch: %d\n", edge_reject_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_edge_reject_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	if (edge_reject_switch == 1) {
		// vertical
		buf[1] = 0xBA;
	} else if (edge_reject_switch == 2) {
		// left up
		buf[1] = 0xBB;
	} else if (edge_reject_switch == 3) {
		// righ up
		buf[1] = 0xBC;
	} else {
		NVT_ERR("Invalid value! edge_reject_switch = %d\n", edge_reject_switch);
		ret = -EINVAL;
		goto nvt_set_edge_reject_switch_out;
	}
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write edge reject switch command fail!\n");
		goto nvt_set_edge_reject_switch_out;
	}

nvt_set_edge_reject_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_edge_reject_switch(uint8_t *edge_reject_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5C);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_edge_reject_switch_out;
	}

	buf[0] = 0x5C;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read edge reject switch status fail!\n");
		goto nvt_get_edge_reject_switch_out;
	}

	*edge_reject_switch = ((buf[1] >> 5) & 0x03);
	NVT_LOG("edge_reject_switch = %d\n", *edge_reject_switch);

nvt_get_edge_reject_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_edge_reject_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t edge_reject_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_edge_reject_switch(&edge_reject_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "edge_reject_switch: %d\n", edge_reject_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_edge_reject_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t edge_reject_switch;
	char *tmp_buf = 0;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}

	if (tmp < 1 || tmp > 3) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	edge_reject_switch = (uint8_t)tmp;
	NVT_LOG("edge_reject_switch = %d\n", edge_reject_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_edge_reject_switch(edge_reject_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_edge_reject_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_edge_reject_switch_proc_read,
	.write = nvt_edge_reject_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_hand_only_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_hand_only_switch(uint8_t hand_only_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set hand only switch: %d\n", hand_only_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_hand_only_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x77;
	buf[2] = hand_only_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write hand only switch command fail!\n");
		goto nvt_set_hand_only_switch_out;
	}

nvt_set_hand_only_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_hand_only_switch(uint8_t *hand_only_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5E);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_hand_only_switch_out;
	}

	buf[0] = 0x5E;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read hand only switch status fail!\n");
		goto nvt_get_hand_only_switch_out;
	}

	*hand_only_switch = ((buf[1] >> 2) & 0x01);
	NVT_LOG("hand_only_switch = %d\n", *hand_only_switch);

nvt_get_hand_only_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_hand_only_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t hand_only_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_hand_only_switch(&hand_only_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "hand_only_switch: %d\n", hand_only_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_hand_only_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t hand_only_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}
	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value!, ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if ((tmp < 0) || (tmp > 1)) {
		NVT_ERR("Invalid value!, tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	hand_only_switch = (uint8_t)tmp;
	NVT_LOG("hand_only_switch = %d\n", hand_only_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_hand_only_switch(hand_only_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_hand_only_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_hand_only_switch_proc_read,
	.write = nvt_hand_only_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_drag_latency_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_drag_latency_switch(uint8_t drag_latency_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set drag latency switch: %d\n", drag_latency_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_drag_latency_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x78;
	buf[2] = drag_latency_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write drag_latency switch command fail!\n");
		goto nvt_set_drag_latency_switch_out;
	}

nvt_set_drag_latency_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_drag_latency_switch(uint8_t *drag_latency_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5E);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_drag_latency_switch_out;
	}

	buf[0] = 0x5E;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read drag_latency switch status fail!\n");
		goto nvt_get_drag_latency_switch_out;
	}

	*drag_latency_switch = ((buf[1] >> 3) & 0x07);
	NVT_LOG("drag_latency_switch = %d\n", *drag_latency_switch);

nvt_get_drag_latency_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_drag_latency_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t drag_latency_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_drag_latency_switch(&drag_latency_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "drag_latency_switch: %d\n", drag_latency_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_drag_latency_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t drag_latency_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if (tmp < 0 || tmp > 4) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	drag_latency_switch = (uint8_t)tmp;
	NVT_LOG("drag_latency_switch = %d\n", drag_latency_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_drag_latency_switch(drag_latency_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_drag_latency_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_drag_latency_switch_proc_read,
	.write = nvt_drag_latency_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_small_jitter_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_small_jitter_switch(uint8_t small_jitter_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set small jitter switch: %d\n", small_jitter_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_small_jitter_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x79;
	buf[2] = small_jitter_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write small jitter switch command fail!\n");
		goto nvt_set_small_jitter_switch_out;
	}

nvt_set_small_jitter_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_small_jitter_switch(uint8_t *small_jitter_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5E);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_small_jitter_switch_out;
	}

	buf[0] = 0x5E;
	buf[1] = 0x00;
	buf[2] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Read small jitter switch status fail!\n");
		goto nvt_get_small_jitter_switch_out;
	}

	*small_jitter_switch = (((buf[2] & 0x01) << 2) | ((buf[1] >> 6) & 0x03));
	NVT_LOG("small_jitter_switch = %d\n", *small_jitter_switch);

nvt_get_small_jitter_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_small_jitter_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t small_jitter_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_small_jitter_switch(&small_jitter_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "small_jitter_switch: %d\n", small_jitter_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_small_jitter_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t small_jitter_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value! count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}

	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value! ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if (tmp < 0 || tmp > 4) {
		NVT_ERR("Invalid value! tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	small_jitter_switch = (uint8_t)tmp;
	NVT_LOG("small_jitter_switch = %d\n", small_jitter_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_small_jitter_switch(small_jitter_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_small_jitter_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_small_jitter_switch_proc_read,
	.write = nvt_small_jitter_switch_proc_write,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_game_mode_switch function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
int32_t nvt_set_game_mode_switch(uint8_t game_mode_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set game mode switch: %d\n", game_mode_switch);

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_set_game_mode_switch_out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x7A;
	buf[2] = game_mode_switch;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write game mode switch command fail!\n");
		goto nvt_set_game_mode_switch_out;
	}

nvt_set_game_mode_switch_out:
	NVT_LOG("--\n");
	return ret;
}

int32_t nvt_get_game_mode_switch(uint8_t *game_mode_switch)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");

	msleep(35);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | 0x5F);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto nvt_get_game_mode_switch_out;
	}

	buf[0] = 0x5F;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Read game mode switch status fail!\n");
		goto nvt_get_game_mode_switch_out;
	}

	*game_mode_switch = ((buf[1] >> 1) & 0x01);
	NVT_LOG("game_mode_switch = %d\n", *game_mode_switch);

nvt_get_game_mode_switch_out:
	NVT_LOG("--\n");
	return ret;
}

static ssize_t nvt_game_mode_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t game_mode_switch;
	char tmp_buf[64];

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_game_mode_switch(&game_mode_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "game_mode_switch: %d\n", game_mode_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_game_mode_switch_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret;
	int32_t tmp;
	uint8_t game_mode_switch;
	char *tmp_buf;

	NVT_LOG("++\n");

	if (count == 0 || count > 2) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc((count+1), GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret =  -EFAULT;
		goto out;
	}
	ret = sscanf(tmp_buf, "%d", &tmp);
	if (ret != 1) {
		NVT_ERR("Invalid value!, ret = %d\n", ret);
		ret = -EINVAL;
		goto out;
	}
	if ((tmp < 0) || (tmp > 1)) {
		NVT_ERR("Invalid value!, tmp = %d\n", tmp);
		ret = -EINVAL;
		goto out;
	}
	game_mode_switch = (uint8_t)tmp;
	NVT_LOG("game_mode_switch = %d\n", game_mode_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_game_mode_switch(game_mode_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	if (tmp_buf)
		kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

static const struct file_operations nvt_game_mode_switch_fops = {
	.owner = THIS_MODULE,
	.read = nvt_game_mode_switch_proc_read,
	.write = nvt_game_mode_switch_proc_write,
};

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
		NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
	}

	if (ts->pen_support) {
		NVT_proc_pen_diff_entry = proc_create(NVT_PEN_DIFF, 0444, NULL,&nvt_pen_diff_fops);
		if (NVT_proc_pen_diff_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_PEN_DIFF);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_PEN_DIFF);
		}
	}

	NVT_proc_pf_switch_entry = proc_create(NVT_PF_SWITCH, 0666, NULL, &nvt_pf_switch_fops);
	if (NVT_proc_pf_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_PF_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_PF_SWITCH);
	}

	NVT_proc_sensitivity_switch_entry = proc_create(NVT_SENSITIVITY_SWITCH, 0666, NULL, &nvt_sensitivity_switch_fops);
	if (NVT_proc_sensitivity_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SENSITIVITY_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_SENSITIVITY_SWITCH);
	}

	NVT_proc_er_range_switch_entry = proc_create(NVT_ER_RANGE_SWITCH, 0666, NULL, &nvt_er_range_switch_fops);
	if (NVT_proc_er_range_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_ER_RANGE_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_ER_RANGE_SWITCH);
	}

	NVT_proc_edge_reject_switch_entry = proc_create(NVT_EDGE_REJECT_SWITCH, 0666, NULL, &nvt_edge_reject_switch_fops);
	if (NVT_proc_edge_reject_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_EDGE_REJECT_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_EDGE_REJECT_SWITCH);
	}

	NVT_proc_hand_only_switch_entry = proc_create(NVT_HAND_ONLY_SWITCH, 0666, NULL, &nvt_hand_only_switch_fops);
	if (NVT_proc_hand_only_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_HAND_ONLY_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_HAND_ONLY_SWITCH);
	}

	NVT_proc_drag_latency_switch_entry = proc_create(NVT_DRAG_LATENCY_SWITCH, 0666, NULL, &nvt_drag_latency_switch_fops);
	if (NVT_proc_drag_latency_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_DRAG_LATENCY_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_DRAG_LATENCY_SWITCH);
	}

	NVT_proc_small_jitter_switch_entry = proc_create(NVT_SMALL_JITTER_SWITCH, 0666, NULL, &nvt_small_jitter_switch_fops);
	if (NVT_proc_small_jitter_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_SMALL_JITTER_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_SMALL_JITTER_SWITCH);
	}

	NVT_proc_game_mode_switch_entry = proc_create(NVT_GAME_MODE_SWITCH, 0666, NULL, &nvt_game_mode_switch_fops);
	if (NVT_proc_game_mode_switch_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_GAME_MODE_SWITCH);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_GAME_MODE_SWITCH);
	}

	NVT_proc_xiaomi_lockdown_info_entry = proc_create(NVT_XIAOMI_LOCKDOWN_INFO, 0444, NULL, &nvt_xiaomi_lockdown_info_fops);
	if (NVT_proc_xiaomi_lockdown_info_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_XIAOMI_LOCKDOWN_INFO);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_XIAOMI_LOCKDOWN_INFO);
	}

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}

	if (ts->pen_support) {
		if (NVT_proc_pen_diff_entry != NULL) {
			remove_proc_entry(NVT_PEN_DIFF, NULL);
			NVT_proc_pen_diff_entry = NULL;
			NVT_LOG("Removed /proc/%s\n", NVT_PEN_DIFF);
		}
	}

	if (NVT_proc_pf_switch_entry != NULL) {
		remove_proc_entry(NVT_PF_SWITCH, NULL);
		NVT_proc_pf_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_PF_SWITCH);
	}

	if (NVT_proc_sensitivity_switch_entry != NULL) {
		remove_proc_entry(NVT_SENSITIVITY_SWITCH, NULL);
		NVT_proc_sensitivity_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SENSITIVITY_SWITCH);
	}

	if (NVT_proc_er_range_switch_entry != NULL) {
		remove_proc_entry(NVT_ER_RANGE_SWITCH, NULL);
		NVT_proc_er_range_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_ER_RANGE_SWITCH);
	}

	if (NVT_proc_edge_reject_switch_entry != NULL) {
		remove_proc_entry(NVT_EDGE_REJECT_SWITCH, NULL);
		NVT_proc_edge_reject_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_EDGE_REJECT_SWITCH);
	}

	if (NVT_proc_hand_only_switch_entry != NULL) {
		remove_proc_entry(NVT_HAND_ONLY_SWITCH, NULL);
		NVT_proc_hand_only_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_HAND_ONLY_SWITCH);
	}

	if (NVT_proc_drag_latency_switch_entry != NULL) {
		remove_proc_entry(NVT_DRAG_LATENCY_SWITCH, NULL);
		NVT_proc_drag_latency_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DRAG_LATENCY_SWITCH);
	}

	if (NVT_proc_small_jitter_switch_entry != NULL) {
		remove_proc_entry(NVT_SMALL_JITTER_SWITCH, NULL);
		NVT_proc_small_jitter_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_SMALL_JITTER_SWITCH);
	}

	if (NVT_proc_game_mode_switch_entry != NULL) {
		remove_proc_entry(NVT_GAME_MODE_SWITCH, NULL);
		NVT_proc_game_mode_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_GAME_MODE_SWITCH);
	}
}
#endif
