/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * $Revision: 116181 $
 * $Date: 2023-03-31 10:17:24 +0800 (週五, 31 三月 2023) $
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
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
#include <linux/poll.h>
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
#include <misc/xiaomi_touch_notifier.h>
#endif
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */

/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 start */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/soc/qcom/smem.h>
/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 end */

#include "nt36xxx.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_PEN_DIFF "nvt_pen_diff"
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 start */
#define NVT_IRQ "nvt_irq"
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 end */
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
#define NVT_TP_SELFTEST                         "tp_selftest"
#define FTS_PROC_CTP_OPENSHORT_TEST_FILE        "ctp_openshort_test"
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
#define PROC_TOUCHSCREEN_FOLDER                 "touchscreen"
#define NVT_PROXIMITY_SWITCH                    "proximity_switch"
#define NVT_STORE_DTAT_PROXIMITY                "store_data_proximity"
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 start */
#define NVT_EDGE_REJECT_SWITCH                  "nvt_edge_reject_switch"
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 end */
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 start */
#define TP_DATA_DUMP                            "tp_data_dump"
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 end */

/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
#define SELF_TEST_INVAL 0
#define SELF_TEST_NG 1
#define SELF_TEST_OK 2
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
#define CMD_BUF_MAX 8
#define SWITCH_READ_BUF_MAX 64
#define PROXIMITY_SLEEP_TIME 35
#define PROXIMITY_INVALID_COUNT 2
#define PROXIMITY_OFF_CMD 0x86
#define PROXIMITY_ON_CMD 0x85
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */
#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 start */
#define NVT_TP_INFO "tp_info"
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 end */
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 start */
#define PROC_LOCKDOWN_INFO_FILE "tp_lockdown_info"
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 end */
#define XDATA_SECTOR_SIZE   256
/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 start */
#define SMEM_SW_DISPLAY_LOCKDOWN_TABLE 498
/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 end */

static uint8_t xdata_tmp[8192] = {0};
static int32_t xdata[4096] = {0};
static int32_t xdata_pen_tip_x[256] = {0};
static int32_t xdata_pen_tip_y[256] = {0};
static int32_t xdata_pen_ring_x[256] = {0};
static int32_t xdata_pen_ring_y[256] = {0};

/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
static int aftersale_selftest = 0;
static struct proc_dir_entry *nvt_proc_tp_selftest_entry = NULL;
static struct proc_dir_entry *proc_ctp_openshort_test_file = NULL;
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_pen_diff_entry;
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 start */
static struct proc_dir_entry *NVT_proc_irq_entry;
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 end */
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 start */
static struct proc_dir_entry *NVT_proc_tp_info_entry;
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 end */
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 start */
static struct proc_dir_entry *proc_tp_lockdown_info_entry = NULL;
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 end */
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
static struct proc_dir_entry *proc_touchscreen_dir = NULL;
static struct proc_dir_entry *NVT_proc_proximity_switch_entry = NULL;
static struct proc_dir_entry *NVT_proc_store_data_proximity_entry = NULL;
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 start */
static struct proc_dir_entry *nvt_proc_edge_reject_switch_entry = NULL;
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 end */
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 start */
static struct proc_dir_entry *tp_data_dump_entry =NULL;
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 end */
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
static struct xiaomi_touch_notify_data touch_proximity_data;
#endif

/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
extern int nvt_factory_test(void);
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */

DECLARE_WAIT_QUEUE_HEAD(proximity_switch_queue);
extern int g_priximity_data;
extern int32_t g_new_proximity_event_flag;
//extern void set_lcd_reset_gpio_keep_high(bool en);
int32_t g_priximity_enable;
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */
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
		usleep_range(20000, 20000);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_SPI_WRITE(ts->client, buf, 2);
		usleep_range(20000, 20000);
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
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

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

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
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
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 2] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

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

		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + transfer_len*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		nvt_set_page(xdata_addr + data_len - residual_len);

		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			buf[0] = transfer_len * j;
			CTP_SPI_READ(ts->client, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + transfer_len*j + k));
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
			seq_printf(m, "%6d,", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}

#if TOUCH_KEY_NUM > 0
	for (i = 0; i < TOUCH_KEY_NUM; i++) {
		seq_printf(m, "%6d,", xdata[ts->x_num * ts->y_num + i]);
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
		seq_printf(m, "%6d,", xdata_pen_tip_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Tip Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%6d,", xdata_pen_tip_y[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring X:\n");
	for (i = 0; i < ts->x_num; i++) {
		seq_printf(m, "%6d,", xdata_pen_ring_x[i]);
	}
	seq_puts(m, "\n");
	seq_printf(m, "Ring Y:\n");
	for (i = 0; i < ts->y_num; i++) {
		seq_printf(m, "%6d,", xdata_pen_ring_y[i]);
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
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 start */
static ssize_t nvt_irq_write(struct file *file, const char __user *buffer,size_t count, loff_t *data)
{
	char temp_buff[128] = { 0 };
	uint32_t pdata =1;
	if(!buffer){
		return -EINVAL;
	}
	if (copy_from_user(temp_buff, buffer, count - 1)) {
		NVT_ERR("copy from user error!\n");
		return -EFAULT;
	}

	if (sscanf(temp_buff, "%d", &pdata)) {
		if (pdata == 1) {
			nvt_irq_enable(true);
		} else {
			nvt_irq_enable(false);
		}
	}
	return count;
}
#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_irq_fops = {
	.proc_write = nvt_irq_write,
};
#else
static const struct file_operations nvt_irq_fops = {
	.owner = THIS_MODULE,
	.write = nvt_irq_write,
};
#endif
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 end */
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

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 start */
/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 start */
static int tp_lockdown_info_proc_show(struct seq_file *m, void *v)
{
	if(ts->lkdown_readed) {
		seq_printf(m, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
			ts->lockdown_info[0], ts->lockdown_info[1], ts->lockdown_info[2], ts->lockdown_info[3],
			ts->lockdown_info[4], ts->lockdown_info[5], ts->lockdown_info[6], ts->lockdown_info[7]);
	} else {
		NVT_ERR("lockdown info is not ready yet !!!");
		seq_printf(m, "s\n","1234567890000000");
	}
	return 0;
}
/* N19 code for HQ-351947 by jiyongfei at 2023/3/4 end */
static int tp_lockdown_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, tp_lockdown_info_proc_show, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops proc_tp_lockdown_info_fops = {
    .proc_open = tp_lockdown_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};
#else
static const struct file_operations proc_tp_lockdown_info_fops = {
    .open  = tp_lockdown_info_open,
    .read  = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};
#endif
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 end */
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 start */
static int32_t nvt_tp_info_show(struct seq_file *m, void *v)
{
    nvt_get_fw_info();
    seq_printf(m, "[Vendor]:Tianma [TP-IC]:NT36672C [FW]:0x%02x\n", tp_fw_version);
    return 0;
}
static int32_t nvt_tp_info_open(struct inode *inode, struct file *file)
{
    return single_open(file, nvt_tp_info_show, NULL);
}
static const struct proc_ops nvt_tp_info_fops = {
    .proc_open = nvt_tp_info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 end */

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

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

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

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

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

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

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

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_pen_diff_fops = {
	.proc_open = nvt_pen_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_pen_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_pen_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
static ssize_t aftersale_selftest_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	char temp_buf[20] = {0};
	if (copy_from_user(temp_buf, buf, count)) {
		return count;
	}
	if(!strncmp("short", temp_buf, 5) || !strncmp("open", temp_buf, 4)){
		aftersale_selftest = 1;
	} else if (!strncmp("spi", temp_buf, 3)) {
		aftersale_selftest = 2;
	}else {
		aftersale_selftest = 0;
		NVT_LOG("tp_selftest echo incorrect\n");
	}
	return count;
}
static int32_t nvt_spi_check(void)
{
	int ret = 0;
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
	return ret;
}

static ssize_t aftersale_selftest_read(struct file *file, char __user *buf,
			size_t count, loff_t *pos)
{
	int retval = 0;
	char temp_buf[256] = {0};
	if (aftersale_selftest == 1) {
		if (nvt_factory_test() < 0) {
			retval = SELF_TEST_NG;
		} else {
			retval = SELF_TEST_OK;
		}
	} else if (aftersale_selftest == 2) {
		retval = nvt_spi_check();
		NVT_LOG("RETVAL is %d\n", retval);
		if (!retval)
			retval = SELF_TEST_OK;
		else
			retval = SELF_TEST_NG;
	}
	snprintf(temp_buf, 256, "%d\n", retval);
	return simple_read_from_buffer(buf, count, pos, temp_buf, strlen(temp_buf));
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops aftersale_test_ops = {
	.proc_read = aftersale_selftest_read,
	.proc_write = aftersale_selftest_write,
};
#else
static const struct file_operations aftersale_test_ops = {
	.read = aftersale_selftest_read,
	.write = aftersale_selftest_write,
};
#endif
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */

/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 start */
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
	//static int finished;
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t edge_reject_switch = 0;
	char tmp_buf[64] = {0};
	NVT_LOG("++\n");
	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/
	/*if (finished) {
		NVT_LOG("read END\n");
		finished = 0;
		return 0;
	}
	finished = 1;*/
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
	int32_t ret = 0;
	int32_t tmp = 0;
	uint8_t edge_reject_switch = 0;
	char *tmp_buf = NULL;
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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_edge_reject_switch_fops = {
	.proc_read = nvt_edge_reject_switch_proc_read,
	.proc_write = nvt_edge_reject_switch_proc_write,
};
#else
static const struct file_operations nvt_edge_reject_switch_fops = {
	.read = nvt_edge_reject_switch_proc_read,
	.write = nvt_edge_reject_switch_proc_write,
};
#endif
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 end */
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 start */
static int32_t tp_data_dump_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;
	/*-------diff data-----*/
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	seq_printf(m, "diffdata\n");
	nvt_change_mode(TEST_MODE_2);
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR, ts->mmap->DIFF_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR, ts->mmap->DIFF_BTN_PIPE1_ADDR);
	nvt_change_mode(NORMAL_MODE);
	mutex_unlock(&ts->lock);
	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}
	memset(xdata, 0, sizeof(xdata));
	/*-------raw data--------*/
	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}
	seq_printf(m, "\nrawdata\n");
	nvt_change_mode(TEST_MODE_2);
	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR, ts->mmap->RAW_BTN_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR, ts->mmap->RAW_BTN_PIPE1_ADDR);
	nvt_change_mode(NORMAL_MODE);
	mutex_unlock(&ts->lock);
	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++) {
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		}
		seq_puts(m, "\n");
	}
	return 0;
}

static int32_t tp_data_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, tp_data_dump_show, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops tp_data_dump_fops = {
	.proc_open  = tp_data_dump_open,
	.proc_read  = seq_read,
};
#else
static const struct file_operations tp_data_dump_fops = {
	.open 	 = tp_data_dump_open,
	.read 	 = seq_read,
};
#endif
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 end */
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
int32_t nvt_set_proximity_switch(uint8_t proximity_switch)
{
	uint8_t buf[CMD_BUF_MAX] = {0};
	int32_t ret = 0;

	NVT_LOG("++\n");
	NVT_LOG("set proximity switch: %d\n", proximity_switch);

	msleep(PROXIMITY_SLEEP_TIME);

	//---set xdata index to EVENT BUF ADDR---
	ret = nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	if (ret < 0) {
		NVT_ERR("Set event buffer index fail!\n");
		goto out;
	}

	buf[0] = EVENT_MAP_HOST_CMD;
	if (proximity_switch == 0) {
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
		touch_proximity_data.ps_enable = XIAOMI_TOUCH_SENSOR_PS_DISABLE;
		xiaomi_touch_notifier_call_chain(XIAOMI_TOUCH_SENSOR_EVENT_PS_SWITCH, &touch_proximity_data);
#endif
		// proximity disable
		//set_lcd_reset_gpio_keep_high(false);
		g_priximity_enable = 0;
		g_priximity_data = -1;
		buf[1] = PROXIMITY_OFF_CMD;
		NVT_LOG("g_priximity_enable is %d,g_priximity_data = %d\n",g_priximity_enable,g_priximity_data);
	} else if (proximity_switch == 1) {
#if IS_ENABLED(CONFIG_XIAOMI_TOUCH_NOTIFIER)
		touch_proximity_data.ps_enable = XIAOMI_TOUCH_SENSOR_PS_ENABLE;
		xiaomi_touch_notifier_call_chain(XIAOMI_TOUCH_SENSOR_EVENT_PS_SWITCH, &touch_proximity_data);
#endif
		// proximity enable
		//set_lcd_reset_gpio_keep_high(true);
		g_priximity_enable = 1;
		buf[1] = PROXIMITY_ON_CMD;
		NVT_LOG("g_priximity_enable is %d\n",g_priximity_enable);
	} else {
		NVT_ERR("Invalid value! proximity_switch = %d\n", proximity_switch);
		ret = -EINVAL;
		goto out;
	}
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write proximity switch command fail!\n");
		goto out;
	}

out:
	NVT_LOG("--\n");
	return ret;
}
EXPORT_SYMBOL(nvt_set_proximity_switch);

int32_t nvt_get_proximity_switch(uint8_t *proximity_switch)
{
	NVT_LOG("++\n");

	msleep(PROXIMITY_SLEEP_TIME);

	*proximity_switch = g_priximity_enable;
	NVT_LOG("proximity_switch = %d\n", *proximity_switch);

	NVT_LOG("--\n");
	return 0;
}

static ssize_t nvt_proximity_switch_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t cnt = 0;
	int32_t len = 0;
	uint8_t proximity_switch = 0;
	char tmp_buf[SWITCH_READ_BUF_MAX] = {0};

	NVT_LOG("++\n");

	/*
	* We return 0 to indicate end of file, that we have
	* no more information. Otherwise, processes will
	* continue to read from us in an endless loop.
	*/

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_get_proximity_switch(&proximity_switch);

	mutex_unlock(&ts->lock);

	cnt = snprintf(tmp_buf, sizeof(tmp_buf), "proximity_switch: %d\n", proximity_switch);
	if (copy_to_user(buf, tmp_buf, sizeof(tmp_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	buf += cnt;
	len += cnt;

	NVT_LOG("--\n");
	return len;
}

static ssize_t nvt_proximity_switch_proc_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos)
{
	int32_t ret = 0;
	int32_t tmp = 0;
	uint8_t proximity_switch = 0;
	char *tmp_buf = NULL;
	if(!buf){
		return -EINVAL;
	}
	NVT_LOG("++\n");

	if (count == 0 || count > PROXIMITY_INVALID_COUNT) {
		NVT_ERR("Invalid value!, count = %zu\n", count);
		ret = -EINVAL;
		goto out;
	}

	tmp_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		goto out;
	}
	if (copy_from_user(tmp_buf, buf, count)) {
		NVT_ERR("copy_from_user() error!\n");
		ret = -EFAULT;
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
	proximity_switch = (uint8_t)tmp;
	NVT_LOG("proximity_switch = %d\n", proximity_switch);

	if (mutex_lock_interruptible(&ts->lock)) {
		return -ERESTARTSYS;
	}

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	nvt_set_proximity_switch(proximity_switch);

	mutex_unlock(&ts->lock);

	ret = count;
out:
	kfree(tmp_buf);
	NVT_LOG("--\n");
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_proximity_switch_fops = {
	.proc_read = nvt_proximity_switch_proc_read,
	.proc_write = nvt_proximity_switch_proc_write,
};
#else
static const struct file_operations nvt_proximity_switch_fops = {
	.read  = nvt_proximity_switch_proc_read,
	.write = nvt_proximity_switch_proc_write,
};
#endif

static ssize_t nvt_store_data_proximity_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int cnt = 0;
	char *data_buf = NULL;
	int ret = 0;
	data_buf = kzalloc(count + 1, GFP_KERNEL);
	if (!data_buf) {
		NVT_ERR("Allocate tmp_buf fail!\n");
		ret = -ENOMEM;
		kfree(data_buf);
	}

	NVT_LOG("-----nvt_store_data_proximity_read----\n");
	NVT_LOG("g_new_proximity_event_flag is %d,g_priximity_data is %d\n",g_new_proximity_event_flag,g_priximity_data);
	wait_event_interruptible(proximity_switch_queue,g_new_proximity_event_flag);

	cnt = snprintf(data_buf,sizeof(data_buf),"%d\n",g_priximity_data);
	if (copy_to_user(buf, data_buf, sizeof(data_buf))) {
		NVT_ERR("copy_to_user() error!\n");
		return -EFAULT;
	}
	g_new_proximity_event_flag = 0;
	buf += cnt;
	ret += cnt;
	return ret;
}

static __poll_t nvt_store_data_proximity_poll(struct file *file, poll_table *wait)
{
	int ret = 0;
	if(!file || !wait){
		return -EINVAL;
	}
	poll_wait(file,&proximity_switch_queue,wait);
	if(g_new_proximity_event_flag) {
		ret = POLLIN | POLLRDNORM;
	}
	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops nvt_store_data_proximity_fops = {
	.proc_read = nvt_store_data_proximity_read,
	.proc_poll = nvt_store_data_proximity_poll,

};
#else
static const struct file_operations nvt_store_data_proximity_fops = {
	.read = nvt_store_data_proximity_read,
	.poll = nvt_store_data_proximity_poll,
};
#endif

/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
static int openshort_test_proc_show(struct seq_file *m, void *v)
{
    if(nvt_factory_test ()<  0)
        seq_printf(m, "%s\n", "result=0");
    else
        seq_printf(m, "%s\n", "result=1");
    return 0;
}
static int openshort_test_open(struct inode *inode, struct file *file)
{
    return single_open(file, openshort_test_proc_show, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops proc_ctp_openshort_test_fops = {
	.proc_open = openshort_test_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
static const struct file_operations proc_ctp_openshort_test_fops = {
    .open 	 = openshort_test_open,
    .read 	 = seq_read,
    .llseek	 = seq_lseek,
    .release = single_release,
};
#endif
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */

/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 start */
	tp_data_dump_entry = proc_create(TP_DATA_DUMP, (S_IWUSR | S_IRUGO), NULL, &tp_data_dump_fops);
	if (tp_data_dump_entry == NULL) {
		printk(KERN_ERR "create proc/%s Failed!\n", TP_DATA_DUMP);
	} else {
		printk("create proc/%s Succeeded!\n", TP_DATA_DUMP);
	}
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 end */
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 start */
	nvt_proc_edge_reject_switch_entry = proc_create(NVT_EDGE_REJECT_SWITCH, (S_IWUSR | S_IRUGO), NULL, &nvt_edge_reject_switch_fops);
	if (nvt_proc_edge_reject_switch_entry == NULL) {
		printk(KERN_ERR "%s: nvt_proc_edge_reject_switch_entry file create failed!\n", __func__);
	} else {
		printk("%s: proc_create nvt_proc_edge_reject_switch_entry success",__func__);
	}
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 end */

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
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 start */
	NVT_proc_irq_entry = proc_create(NVT_IRQ, 0222, NULL,&nvt_irq_fops);
	if (NVT_proc_irq_entry == NULL) {
		NVT_ERR("create proc/%s Failed!\n", NVT_IRQ);
		return -ENOMEM;
	} else {
		NVT_LOG("create proc/%s Succeeded!\n", NVT_IRQ);
	}
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 end */
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 start */
    NVT_proc_tp_info_entry = proc_create(NVT_TP_INFO, 0444, NULL, &nvt_tp_info_fops);
    if (NVT_proc_tp_info_entry == NULL) {
        NVT_ERR("create proc/%s Failed!\n", NVT_TP_INFO);
        return -ENOMEM;
    } else {
        NVT_LOG("create proc/%s Succeeded!\n", NVT_TP_INFO);
    }
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 end */
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 start */
    proc_tp_lockdown_info_entry = proc_create(PROC_LOCKDOWN_INFO_FILE, 0444, NULL, &proc_tp_lockdown_info_fops);
    if (proc_tp_lockdown_info_entry == NULL) {
        NVT_ERR("create proc/%s Failed!\n", PROC_LOCKDOWN_INFO_FILE);
        return -ENOMEM;
    } else {
        NVT_LOG("create proc/%s Succeeded!\n", PROC_LOCKDOWN_INFO_FILE);
    }
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 end */
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
	nvt_proc_tp_selftest_entry = proc_create(NVT_TP_SELFTEST, (S_IWUSR | S_IRUGO), NULL, &aftersale_test_ops);
	if (nvt_proc_tp_selftest_entry == NULL) {
		printk(KERN_ERR "%s: nvt_proc_tp_selftest_entry file create failed!\n", __func__);
	} else {
		printk("%s: proc_create nvt_proc_tp_selftest_entry success",__func__);
	}
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
	proc_touchscreen_dir = proc_mkdir(PROC_TOUCHSCREEN_FOLDER , NULL);
	if (proc_touchscreen_dir == NULL)  {
		printk (KERN_ERR "%s: proc_touchpanel_dir file create failed!\n", __func__);
	} else {
	NVT_proc_proximity_switch_entry = proc_create(NVT_PROXIMITY_SWITCH,
		(S_IWUSR | S_IRUGO), proc_touchscreen_dir, &nvt_proximity_switch_fops);
		if (NVT_proc_proximity_switch_entry == NULL) {
			printk (KERN_ERR "%s: NVT_proc_proximity_switch create failed!\n", __func__);
			remove_proc_entry(NVT_PROXIMITY_SWITCH, NULL);
		}
		NVT_proc_store_data_proximity_entry = proc_create(NVT_STORE_DTAT_PROXIMITY,
		(S_IWUSR | S_IRUGO), proc_touchscreen_dir, &nvt_store_data_proximity_fops);
		if (NVT_proc_store_data_proximity_entry == NULL) {
			printk (KERN_ERR "%s: NVT_proc_store_data_proximity create failed!\n", __func__);
			remove_proc_entry(NVT_STORE_DTAT_PROXIMITY, NULL);
		}
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
		proc_ctp_openshort_test_file = proc_create(FTS_PROC_CTP_OPENSHORT_TEST_FILE,
		(S_IWUSR | S_IRUGO), proc_touchscreen_dir, &proc_ctp_openshort_test_fops);
		if (proc_ctp_openshort_test_file == NULL) {
			printk (KERN_ERR "%s: proc_ctp_openshort_test_file create failed!\n", __func__);
			remove_proc_entry(PROC_TOUCHSCREEN_FOLDER, NULL);
		} else {
			printk ("%s:  proc_create PROC_CTP_OPENSHORT_TEST_FILE success",__func__);
		}
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
	}
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */
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
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 start */
	if (tp_data_dump_entry != NULL) {
		remove_proc_entry(TP_DATA_DUMP, NULL);
		tp_data_dump_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", TP_DATA_DUMP);
	}
/* N19 code for HQ-354561 by p-luozhibin1 at 2023.1.24 end */
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 start */
	if (nvt_proc_edge_reject_switch_entry != NULL) {
		remove_proc_entry(NVT_EDGE_REJECT_SWITCH, NULL);
		nvt_proc_edge_reject_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_EDGE_REJECT_SWITCH);
	}
/* N19 code for HQ-351885 by p-luozhibin1 at 2023.1.17 end */

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
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 start */
	if (NVT_proc_irq_entry != NULL) {
		remove_proc_entry(NVT_IRQ, NULL);
		NVT_proc_irq_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_IRQ);
	}
/* N19 code for HQ-359787 by liaoxianguo at 2023/12/19 end */
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 start */
    if (NVT_proc_tp_info_entry != NULL) {
        remove_proc_entry(NVT_TP_INFO, NULL);
        NVT_proc_tp_info_entry = NULL;
        NVT_LOG("Removed /proc/%s\n", NVT_TP_INFO);
    }
/* N19 code for HQ-355103 by liuyupei at 2023/12/27 end */
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 start */
    if (proc_tp_lockdown_info_entry != NULL) {
        remove_proc_entry(PROC_LOCKDOWN_INFO_FILE, NULL);
        proc_tp_lockdown_info_entry = NULL;
        NVT_LOG("Removed /proc/%s\n", PROC_LOCKDOWN_INFO_FILE);
    }
/* N19 code for HQ-351947 by liuyupei at 2024/1/12 end */
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 start */
	if (NVT_proc_proximity_switch_entry != NULL) {
		remove_proc_entry(NVT_PROXIMITY_SWITCH, NULL);
		NVT_proc_proximity_switch_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_PROXIMITY_SWITCH);
	}
	if (NVT_proc_store_data_proximity_entry != NULL) {
		remove_proc_entry(NVT_STORE_DTAT_PROXIMITY, NULL);
		NVT_proc_store_data_proximity_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_STORE_DTAT_PROXIMITY);
	}
/* N19 code for HQ-357608 by liaoxianguo at 2023/12/29 end */
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 start */
	if (proc_ctp_openshort_test_file != NULL) {
		remove_proc_entry(NVT_TP_SELFTEST, NULL);
		proc_ctp_openshort_test_file = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_TP_SELFTEST);
	}
	if (nvt_proc_tp_selftest_entry != NULL) {
		remove_proc_entry(NVT_TP_SELFTEST, NULL);
		nvt_proc_tp_selftest_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_TP_SELFTEST);
	}
/* N19 code for HQ-351846 by jiyongfei at 2023/3/4 end */
}
#endif
