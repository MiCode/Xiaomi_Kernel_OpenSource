/*
 * Copyright (C) 2010 - 2017 Novatek, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Revision: 18364
 * $Date: 2017-11-16 17:42:51 +0800 (週四, 16 十一月 2017) $
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/input/mt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>

#include "tpd.h"
#include "nt36xxx.h"
/*TODO: wakelock not used in k-4.9, fix it*/
#if WAKEUP_GESTURE
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_MTK_I2C_EXTENSION
#if I2C_DMA_SUPPORT
#include <linux/dma-mapping.h>

static uint8_t *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;
#endif
#endif

struct nvt_ts_data *ts;

#if BOOT_UPDATE_FIRMWARE
static struct workqueue_struct *nvt_fwu_wq;
#endif

static const struct nvt_ts_mem_map NT36772_memory_map = {
	.EVENT_BUF_ADDR           = 0x11E00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE0_Q_ADDR         = 0,
	.RAW_PIPE1_ADDR           = 0x12000,
	.RAW_PIPE1_Q_ADDR         = 0,
	.BASELINE_ADDR            = 0x10E70,
	.BASELINE_Q_ADDR          = 0,
	.BASELINE_BTN_ADDR        = 0x12E70,
	.BASELINE_BTN_Q_ADDR      = 0,
	.DIFF_PIPE0_ADDR          = 0x10830,
	.DIFF_PIPE0_Q_ADDR        = 0,
	.DIFF_PIPE1_ADDR          = 0x12830,
	.DIFF_PIPE1_Q_ADDR        = 0,
	.RAW_BTN_PIPE0_ADDR       = 0x10E60,
	.RAW_BTN_PIPE0_Q_ADDR     = 0,
	.RAW_BTN_PIPE1_ADDR       = 0x12E60,
	.RAW_BTN_PIPE1_Q_ADDR     = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0x10E68,
	.DIFF_BTN_PIPE0_Q_ADDR    = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0x12E68,
	.DIFF_BTN_PIPE1_Q_ADDR    = 0,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

static const struct nvt_ts_mem_map NT36525_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE0_Q_ADDR         = 0,
	.RAW_PIPE1_ADDR           = 0x12000,
	.RAW_PIPE1_Q_ADDR         = 0,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_Q_ADDR          = 0,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.BASELINE_BTN_Q_ADDR      = 0,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE0_Q_ADDR        = 0,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.DIFF_PIPE1_Q_ADDR        = 0,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE0_Q_ADDR     = 0,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.RAW_BTN_PIPE1_Q_ADDR     = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE0_Q_ADDR    = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	.DIFF_BTN_PIPE1_Q_ADDR    = 0,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

static const struct nvt_ts_mem_map NT36870_memory_map = {
	.EVENT_BUF_ADDR           = 0x25000,
	.RAW_PIPE0_ADDR           = 0x20000,
	.RAW_PIPE0_Q_ADDR         = 0x204C8,
	.RAW_PIPE1_ADDR           = 0x23000,
	.RAW_PIPE1_Q_ADDR         = 0x234C8,
	.BASELINE_ADDR            = 0x21350,
	.BASELINE_Q_ADDR          = 0x21818,
	.BASELINE_BTN_ADDR        = 0x24350,
	.BASELINE_BTN_Q_ADDR      = 0x24358,
	.DIFF_PIPE0_ADDR          = 0x209B0,
	.DIFF_PIPE0_Q_ADDR        = 0x20E78,
	.DIFF_PIPE1_ADDR          = 0x239B0,
	.DIFF_PIPE1_Q_ADDR        = 0x23E78,
	.RAW_BTN_PIPE0_ADDR       = 0x20990,
	.RAW_BTN_PIPE0_Q_ADDR     = 0x20998,
	.RAW_BTN_PIPE1_ADDR       = 0x23990,
	.RAW_BTN_PIPE1_Q_ADDR     = 0x23998,
	.DIFF_BTN_PIPE0_ADDR      = 0x21340,
	.DIFF_BTN_PIPE0_Q_ADDR    = 0x21348,
	.DIFF_BTN_PIPE1_ADDR      = 0x24340,
	.DIFF_BTN_PIPE1_Q_ADDR    = 0x24348,
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR       = 0x24002,
};

static const struct nvt_ts_mem_map NT36676F_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE0_Q_ADDR         = 0,
	.RAW_PIPE1_ADDR           = 0x12000,
	.RAW_PIPE1_Q_ADDR         = 0,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_Q_ADDR          = 0,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.BASELINE_BTN_Q_ADDR      = 0,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE0_Q_ADDR        = 0,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.DIFF_PIPE1_Q_ADDR        = 0,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE0_Q_ADDR     = 0,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.RAW_BTN_PIPE1_Q_ADDR     = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE0_Q_ADDR    = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	.DIFF_BTN_PIPE1_Q_ADDR    = 0,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

#define NVT_ID_BYTE_MAX 6
struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	const struct nvt_ts_mem_map *mmap;
	uint8_t carrier_system;
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{.id = {0x55, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0x55, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xAA, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xAA, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36525_memory_map, .carrier_system = 0},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x68, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36870_memory_map, .carrier_system = 1},
	{.id = {0xFF, 0xFF, 0xFF, 0x76, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36676F_memory_map, .carrier_system = 0}
};

#if TOUCH_KEY_NUM > 0
const uint16_t touch_key_array[TOUCH_KEY_NUM] = {
	KEY_BACK,
	KEY_HOME,
	KEY_MENU
};
#endif

#if WAKEUP_GESTURE
const uint16_t gesture_key_array[] = {
	KEY_POWER,  //GESTURE_WORD_C
	KEY_POWER,  //GESTURE_WORD_W
	KEY_POWER,  //GESTURE_WORD_V
	KEY_POWER,  //GESTURE_DOUBLE_CLICK
	KEY_POWER,  //GESTURE_WORD_Z
	KEY_POWER,  //GESTURE_WORD_M
	KEY_POWER,  //GESTURE_WORD_O
	KEY_POWER,  //GESTURE_WORD_e
	KEY_POWER,  //GESTURE_WORD_S
	KEY_POWER,  //GESTURE_SLIDE_UP
	KEY_POWER,  //GESTURE_SLIDE_DOWN
	KEY_POWER,  //GESTURE_SLIDE_LEFT
	KEY_POWER,  //GESTURE_SLIDE_RIGHT
};
#endif

static uint8_t bTouchIsAwake;
static int tpd_flag;
static struct task_struct *thread;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

#ifdef CONFIG_MTK_I2C_EXTENSION
#if I2C_DMA_SUPPORT
int32_t i2c_dma_read(struct i2c_client *client, uint16_t addr,
	uint8_t offset, uint8_t *rxbuf, uint16_t len)
{
	uint8_t buf[2] = {offset, 0};
	int32_t ret;
	int32_t retries = 0;

	struct i2c_msg msg[2] = {
		{
			.addr = (addr & I2C_MASK_FLAG),
			.flags = 0,
			.buf = buf,
			.len = 1,
			.timing = client->timing
		},
		{
			.addr = (addr & I2C_MASK_FLAG),
			.ext_flag = (client->ext_flag |
					I2C_ENEXT_FLAG |
					I2C_DMA_FLAG),
			.flags = I2C_M_RD,
			.buf = (uint8_t *)gpDMABuf_pa,
			.len = len,
			.timing = client->timing
		},
	};

	if (rxbuf == NULL) {
		NVT_ERR("rxbuf is NULL!\n");
		return -ENOMEM;
	}

	for (retries = 0; retries < 20; ++retries) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0)
			continue;
		memcpy(rxbuf, gpDMABuf_va, len);
		return ret;
	}

	NVT_ERR("Dma I2C Read Error: 0x%04X, %d byte(s), err-code: %d",
		addr, len, ret);
	return ret;
}

int32_t i2c_dma_write(struct i2c_client *client, uint16_t addr,
	uint8_t offset, uint8_t *txbuf, uint16_t len)
{
	uint8_t *wr_buf = gpDMABuf_va;
	int32_t ret = -1;
	int32_t retries = 0;

	struct i2c_msg msg = {
		.addr = (addr & I2C_MASK_FLAG),
		.ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		.flags = 0,
		.buf = (uint8_t *)gpDMABuf_pa,
		.len = 1 + len,
		.timing = client->timing
	};

	wr_buf[0] = offset;

	if (txbuf == NULL) {
		NVT_ERR("txbuf is NULL!\n");
		return -ENOMEM;
	}

	memcpy(wr_buf+1, txbuf, len);
	for (retries = 0; retries < 20; ++retries) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret < 0)
			continue;
		return ret;
	}

	NVT_ERR("Dma I2C Write Error: 0x%04X, %d byte(s), err-code: %d",
			offset, len, ret);

	return ret;
}

int32_t i2c_read_bytes_dma(struct i2c_client *client, u16 addr,
	uint8_t offset, uint8_t *rxbuf, uint16_t len)
{
	uint8_t *rd_buf = rxbuf;
	uint16_t left = len;
	uint16_t read_len = 0;
	int32_t ret = -1;

	while (left > 0) {
		if (left > DMA_MAX_TRANSACTION_LENGTH)
			read_len = DMA_MAX_TRANSACTION_LENGTH;
		else
			read_len = left;
		ret = i2c_dma_read(client, addr, offset, rd_buf, read_len);
		if (ret < 0) {
			NVT_ERR("dma i2c read failed!\n");
			return -EIO;
		}

		left -= read_len;
		offset += read_len;
		rd_buf += read_len;
	}

	return ret;
}

int32_t i2c_write_bytes_dma(struct i2c_client *client, u16 addr,
	uint8_t offset, uint8_t *txbuf, uint16_t len)
{
	uint8_t *wr_buf = txbuf;
	int32_t ret = 0;
	int32_t write_len = 0;
	int32_t left = len;

	while (left > 0) {
		if (left > DMA_MAX_I2C_TRANSFER_SIZE)
			write_len = DMA_MAX_I2C_TRANSFER_SIZE;
		else
			write_len = left;
		ret = i2c_dma_write(client, addr, offset, wr_buf, write_len);

		if (ret < 0) {
			NVT_ERR("dma i2c write failed!\n");
			return -EIO;
		}

		left -= write_len;
		offset += write_len;
		wr_buf += write_len;
	}
	return ret;
}

#else	//I2C_DMA_SUPPORT

int i2c_read_bytes_non_dma(struct i2c_client *client, u16 addr,
				uint8_t offset, uint8_t *rxbuf, uint16_t len)
{
	uint8_t buf[2] = {0};
	uint16_t left = len;
	uint16_t index = 0;
	int32_t ret = 0;
	int32_t retries = 0;

	struct i2c_msg msg[2] = {
		{
			.addr = ((addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
			.flags = 0,
			.buf = buf,
			.len = 1,
			.timing = client->timing
		},
		{
			.addr = ((addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
			.flags = I2C_M_RD,
			.timing = client->timing
		},
	};

	if (rxbuf == NULL) {
		NVT_ERR("rxbuf is NULL!\n");
		return -ENOMEM;
	}

	while (left > 0) {
		buf[0] = offset + index;
		msg[1].buf = &rxbuf[index];

		if (left > MAX_TRANSACTION_LENGTH) {
			msg[1].len = MAX_TRANSACTION_LENGTH;
			left -= MAX_TRANSACTION_LENGTH;
			index += MAX_TRANSACTION_LENGTH;
		} else {
			msg[1].len = left;
			left = 0;
		}

		retries = 0;

		while (retries < 20) {
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret == 2)
				break;
			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("I2C read 0x%X length=%d failed! ret=%d\n",
				offset + index, len, ret);
			ret = -EIO;
		}
	}

	return ret;
}

int i2c_write_bytes_non_dma(struct i2c_client *client, u16 addr,
	uint8_t offset, uint8_t *txbuf, uint16_t len)
{
	uint8_t buf[MAX_TRANSACTION_LENGTH];
	uint16_t left = len;
	uint16_t index = 0;
	int32_t ret = 0;
	int32_t retries = 0;

	struct i2c_msg msg = {
		.addr = ((addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
		.flags = 0,
		.buf = buf,
		.timing = client->timing,
	};

	if (txbuf == NULL) {
		NVT_ERR("txbuf is NULL!\n");
		return -ENOMEM;
	}

	while (left > 0) {
		retries = 0;

		buf[0] = (offset + index) & 0xFF;

		if (left > MAX_I2C_TRANSFER_SIZE) {
			memcpy(&buf[1], &txbuf[index], MAX_I2C_TRANSFER_SIZE);
			msg.len = MAX_TRANSACTION_LENGTH;
			left -= MAX_I2C_TRANSFER_SIZE;
			index += MAX_I2C_TRANSFER_SIZE;
		} else {
			memcpy(&buf[1], &txbuf[index], left);
			msg.len = left + 1;
			left = 0;
		}

		while (retries < 20) {
			ret = i2c_transfer(client->adapter, &msgs, 1);
			if (ret == 1)
				break;
			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("I2C write 0x%X length=%d failed! ret=%d\n",
				offset, len, ret);
			ret = -EIO;
		}
	}

	return ret;
}
#endif	//I2C_DMA_SUPPORT
#endif	//CONFIG_MTK_I2C_EXTENSION

/*******************************************************
 * Description:
 *	Novatek touchscreen i2c read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address,
		uint8_t *buf, uint16_t len)
{
#ifndef CONFIG_MTK_I2C_EXTENSION
	struct i2c_msg msgs[2];
	int32_t ret = -1;
	int32_t retries = 0;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = address;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = address;
	msgs[1].len   = len - 1;
	msgs[1].buf   = &buf[1];

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	return ret;
#else	//CONFIG_MTK_I2C_EXTENSION
	#if I2C_DMA_SUPPORT
	return i2c_read_bytes_dma(client, address, buf[0], &buf[1], len-1);
	#else
	return i2c_read_bytes_non_dma(client, address, buf[0], &buf[1], len-1);
	#endif
#endif	//CONFIG_MTK_I2C_EXTENSION
}

/*******************************************************
 * Description:
 *	Novatek touchscreen i2c write function.
 *
 * return:
 *	Executive outcomes. 1---succeed. -5---I/O error
 *******************************************************/
int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address,
		uint8_t *buf, uint16_t len)
{
#ifndef CONFIG_MTK_I2C_EXTENSION
	struct i2c_msg msg;
	int32_t ret = -1;
	int32_t retries = 0;

	msg.flags = !I2C_M_RD;
	msg.addr  = address;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	return ret;
#else	//CONFIG_MTK_I2C_EXTENSION
	#if I2C_DMA_SUPPORT
	return i2c_write_bytes_dma(client, address, buf[0], &buf[1], len-1);
	#else
	return i2c_write_bytes_non_dma(client, address, buf[0], &buf[1], len-1);
	#endif
#endif	//CONFIG_MTK_I2C_EXTENSION
}

/*******************************************************
 * Description:
 *	Novatek touchscreen reset MCU then into idle mode
 *	function.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_sw_reset_idle(void)
{
	uint8_t buf[4] = {0};

	//---write i2c cmds to reset idle---
	buf[0] = 0x00;
	buf[1] = 0xA5;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	msleep(20);
}

/*******************************************************
 * Description:
 *	Novatek touchscreen reset MCU (boot) function.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_bootloader_reset(void)
{
	uint8_t buf[8] = {0};

	//---write i2c cmds to reset---
	buf[0] = 0x00;
	buf[1] = 0x69;
	CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);

	// need 35ms delay after bootloader reset
	msleep(35);
}

/*******************************************************
 * Description:
 *          Novatek touchscreen clear FW status function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---fail.
 *******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		buf[0] = 0xFF;
		buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
		buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		msleep(20);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
 *Description:
 *	Novatek touchscreen check FW status function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_check_fw_status(void)
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

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		msleep(20);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
 * Description:
 *	Novatek touchscreen check FW reset state function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
int32_t nvt_check_fw_reset_state(enum RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	while (1) {
		msleep(20);

		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) &&
			(buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if (unlikely(retry > 100)) {
			NVT_ERR("error, retry=%d\n", retry);
			NVT_ERR("0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
	}

	return ret;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen get novatek project id information
 *	function.
 *
 * return:
 *	Executive outcomes. 0---success. -1---fail.
 *******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[3] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];

	NVT_LOG("PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen get firmware related information
 *	function.
 *
 * return:
 *	Executive outcomes. 0---success. -1---fail.
 *******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	buf[0] = 0xFF;
	buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 17);
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n",
			buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->x_num = 18;
		ts->y_num = 32;
		ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if (retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			NVT_ERR("Set default fw_ver=%d, x_num=%d, y_num=%d\n",
					ts->fw_ver, ts->x_num, ts->y_num);
			NVT_ERR("abs_x_max=%d, abs_y_max=%d\n",
					ts->abs_x_max, ts->abs_y_max);
			NVT_ERR("max_button_num=%d\n", ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	//---Get Novatek PID---
	nvt_read_pid();

	return ret;
}

/*******************************************************
 *Create Device Node (Proc Entry)
 *******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"

/*******************************************************
 *Description:
 *	Novatek touchscreen /proc/NVTflash read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5,-14---failed.
 *******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff,
			size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;

	if (count > sizeof(str)) {
		NVT_ERR("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		return -EFAULT;
	}

	i2c_wr = str[0] >> 7;

	if(str[1] > (sizeof(str)/sizeof(uint8_t) -2))
        {
                 NVT_ERR("Out of range, the max lenght is %d\n",
                                 (sizeof(str)/sizeof(uint8_t) -2));
                 return -EFAULT;
        }

	if (i2c_wr == 0) {	//I2C write
		while (retries < 20) {
			ret = CTP_I2C_WRITE(ts->client,
				(str[0] & 0x7F), &str[2], str[1]);
			if (ret == 1)
				break;
			NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else if (i2c_wr == 1) {	//I2C read
		while (retries < 20) {
			ret = CTP_I2C_READ(ts->client,
				(str[0] & 0x7F), &str[2], str[1]);
			if (ret == 2)
				break;
			NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		// copy buff to user if i2c transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count))
				return -EFAULT;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
		}
	NVT_ERR("Call error, str[0]=%d\n", str[0]);
	return -EFAULT;
}

/*******************************************************
 *Description:
 *	Novatek touchscreen /proc/NVTflash open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTflash close function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	kfree(dev);

	return 0;
}

static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};

/*******************************************************
 * Description:
 *	Novatek touchscreen /proc/NVTflash initial function.
 *
 *return:
 *	Executive outcomes. 0---succeed. -12---failed.
 *******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL, &nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	}
	NVT_LOG("Succeeded!\n");

	NVT_LOG("=========================================================\n");
	NVT_LOG("Create /proc/NVTflash\n");
	NVT_LOG("=========================================================\n");

	return 0;
}
#endif

#if WAKEUP_GESTURE
#define GESTURE_WORD_C          12
#define GESTURE_WORD_W          13
#define GESTURE_WORD_V          14
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_WORD_Z          16
#define GESTURE_WORD_M          17
#define GESTURE_WORD_O          18
#define GESTURE_WORD_e          19
#define GESTURE_WORD_S          20
#define GESTURE_SLIDE_UP        21
#define GESTURE_SLIDE_DOWN      22
#define GESTURE_SLIDE_LEFT      23
#define GESTURE_SLIDE_RIGHT     24
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

static struct wake_lock gestrue_wakelock;

/*******************************************************
 * Description:
 *	Novatek touchscreen wake up gesture key report function.
 *
 * return:
 *	n.a.
 *******************************************************/
void nvt_ts_wakeup_gesture_report(uint8_t gesture_id, uint8_t *data)
{
	uint32_t keycode = 0;
	uint8_t func_type = data[2];
	uint8_t func_id = data[3];

	/* support fw specifal data protocol */
	if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
		gesture_id = func_id;
	} else if (gesture_id > DATA_PROTOCOL) {
		NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n",
			gesture_id, func_type, func_id);
		return;
	}

	NVT_LOG("gesture_id = %d\n", gesture_id);

	switch (gesture_id) {
	case GESTURE_WORD_C:
		NVT_LOG("Gesture : Word-C.\n");
		keycode = gesture_key_array[0];
		break;
	case GESTURE_WORD_W:
		NVT_LOG("Gesture : Word-W.\n");
		keycode = gesture_key_array[1];
		break;
	case GESTURE_WORD_V:
		NVT_LOG("Gesture : Word-V.\n");
		keycode = gesture_key_array[2];
		break;
	case GESTURE_DOUBLE_CLICK:
		NVT_LOG("Gesture : Double Click.\n");
		keycode = gesture_key_array[3];
		break;
	case GESTURE_WORD_Z:
		NVT_LOG("Gesture : Word-Z.\n");
		keycode = gesture_key_array[4];
		break;
	case GESTURE_WORD_M:
		NVT_LOG("Gesture : Word-M.\n");
		keycode = gesture_key_array[5];
		break;
	case GESTURE_WORD_O:
		NVT_LOG("Gesture : Word-O.\n");
		keycode = gesture_key_array[6];
		break;
	case GESTURE_WORD_e:
		NVT_LOG("Gesture : Word-e.\n");
		keycode = gesture_key_array[7];
		break;
	case GESTURE_WORD_S:
		NVT_LOG("Gesture : Word-S.\n");
		keycode = gesture_key_array[8];
		break;
	case GESTURE_SLIDE_UP:
		NVT_LOG("Gesture : Slide UP.\n");
		keycode = gesture_key_array[9];
		break;
	case GESTURE_SLIDE_DOWN:
		NVT_LOG("Gesture : Slide DOWN.\n");
		keycode = gesture_key_array[10];
		break;
	case GESTURE_SLIDE_LEFT:
		NVT_LOG("Gesture : Slide LEFT.\n");
		keycode = gesture_key_array[11];
		break;
	case GESTURE_SLIDE_RIGHT:
		NVT_LOG("Gesture : Slide RIGHT.\n");
		keycode = gesture_key_array[12];
		break;
	default:
		break;
	}

	if (keycode > 0) {
		input_report_key(ts->input_dev, keycode, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, keycode, 0);
		input_sync(ts->input_dev);
	}
}
#endif	//WAKEUP_GESTURE

#define POINT_DATA_LEN 65
/*******************************************************
 * Description:
 *	Novatek touchscreen work function.
 *
 * return:
 *	n.a.
 *******************************************************/
static int touch_event_handler(void *unused)
{
	struct sched_param param = { .sched_priority = 4 };

	int32_t ret = -1;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};
	uint32_t position = 0;
	uint32_t input_x = 0;
	uint32_t input_y = 0;
	uint32_t input_w = 0;
	uint32_t input_p = 0;
	uint8_t input_id = 0;
#if MT_PROTOCOL_B
	uint8_t press_id[TOUCH_MAX_FINGER_NUM] = {0};
#endif /* MT_PROTOCOL_B */

	int32_t i = 0;
	int32_t finger_cnt = 0;

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);

		mutex_lock(&ts->lock);
		memset(point_data, 0, POINT_DATA_LEN + 1);

		ret = CTP_I2C_READ(ts->client, I2C_FW_Address,
			point_data, POINT_DATA_LEN + 1);
		if (ret < 0) {
			NVT_ERR("CTP_I2C_READ failed.(%d)\n", ret);
			goto XFER_ERROR;
		}

/*
 *		//--- dump I2C buf ---
 *		for (i = 0; i < 10; i++) {
 *		NVT_LOG("%02X %02X %02X %02X %02X %02X  ",
 *		point_data[1+i*6], point_data[2+i*6], point_data[3+i*6],
 *		point_data[4+i*6], point_data[5+i*6], point_data[6+i*6]);
 *		}
 *		NVT_LOG("\n");
 */

		if (bTouchIsAwake == 0) {
#if WAKEUP_GESTURE
			input_id = (uint8_t)(point_data[1] >> 3);
			nvt_ts_wakeup_gesture_report(input_id, point_data);
#endif
			enable_irq(ts->client->irq);
			mutex_unlock(&ts->lock);
			NVT_LOG("return for interrupt after suspend...\n");
			continue;
		}

		finger_cnt = 0;
#if MT_PROTOCOL_B
		memset(press_id, 0, ts->max_touch_num);
#endif /* MT_PROTOCOL_B */

		for (i = 0; i < ts->max_touch_num; i++) {
			position = 1 + 6 * i;
			input_id = (uint8_t)(point_data[position + 0] >> 3);
			if ((input_id == 0) || (input_id > ts->max_touch_num))
				continue;

			if (((point_data[position] & 0x07) == 0x01) ||
				((point_data[position] & 0x07) == 0x02)) {
				//finger down (enter & moving)
				input_x =
					(uint32_t)
					(point_data[position + 1] << 4) +
					(uint32_t)
					(point_data[position + 3] >> 4);
				input_y =
					(uint32_t)
					(point_data[position + 2] << 4) +
					(uint32_t)
					(point_data[position + 3] & 0x0F);
				if ((input_x < 0) || (input_y < 0))
					continue;
				if ((input_x > ts->abs_x_max) ||
					(input_y > ts->abs_y_max))
					continue;
				input_w = (uint32_t)(point_data[position + 4]);
				if (input_w == 0)
					input_w = 1;
				if (i < 2) {
					input_p =
						(uint32_t)
						(point_data[position + 5]) +
						(uint32_t)
						(point_data[i + 63] << 8);
					if (input_p > TOUCH_FORCE_NUM)
						input_p = TOUCH_FORCE_NUM;
				} else {
					input_p =
						(uint32_t)
						(point_data[position + 5]);
				}
				if (input_p == 0)
					input_p = 1;

#if MT_PROTOCOL_B
				press_id[input_id - 1] = 1;
				input_mt_slot(ts->input_dev, input_id - 1);
				input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER, true);
#else /* MT_PROTOCOL_B */
				input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, input_id - 1);
				input_report_key(ts->input_dev, BTN_TOUCH, 1);
#endif /* MT_PROTOCOL_B */

				input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, input_x);
				input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, input_y);
				input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, input_w);
				input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, input_p);

#if MT_PROTOCOL_B
#else /* MT_PROTOCOL_B */
				input_mt_sync(ts->input_dev);
#endif /* MT_PROTOCOL_B */

				finger_cnt++;
			}
		}

#if MT_PROTOCOL_B
		for (i = 0; i < ts->max_touch_num; i++) {
			if (press_id[i] != 1) {
				input_mt_slot(ts->input_dev, i);
				input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 0);
				input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER, false);
			}
		}

		input_report_key(ts->input_dev, BTN_TOUCH, (finger_cnt > 0));
#else /* MT_PROTOCOL_B */
		if (finger_cnt == 0) {
			input_report_key(ts->input_dev, BTN_TOUCH, 0);
			input_mt_sync(ts->input_dev);
		}
#endif /* MT_PROTOCOL_B */

#if TOUCH_KEY_NUM > 0
		if (point_data[61] == 0xF8) {
			for (i = 0; i < ts->max_button_num; i++) {
				input_report_key(ts->input_dev,
					touch_key_array[i],
					((point_data[62] >> i) & 0x01));
			}
		} else {
			for (i = 0; i < ts->max_button_num; i++) {
				input_report_key(ts->input_dev,
					touch_key_array[i], 0);
			}
		}
#endif

		input_sync(ts->input_dev);

XFER_ERROR:
		enable_irq(ts->client->irq);

		mutex_unlock(&ts->lock);

	} while (!kthread_should_stop());

	return 0;
}

/*******************************************************
 * Description:
 *	External interrupt service routine.
 *
 * return:
 *	irq execute status.
 *******************************************************/
static irqreturn_t nvt_ts_irq_handler(int32_t irq, void *dev_id)
{
	tpd_flag = 1;
	disable_irq_nosync(ts->client->irq);

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0)
		wake_lock_timeout(&gestrue_wakelock, msecs_to_jiffies(5000));
#endif

	wake_up_interruptible(&waiter);

	return IRQ_HANDLED;
}

/*******************************************************
 * Description:
 *	Register interrupt handler
 *
 * return:
 *	irq execute status.
 *******************************************************/
static int nvt_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = { 0, 0 };

	NVT_LOG("Device Tree Tpd_irq_registration!\n");

	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		of_property_read_u32_array(node, "debounce",
			ints, ARRAY_SIZE(ints));
		//gpio_set_debounce(ints[0], ints[1]);

		ts->client->irq = irq_of_parse_and_map(node, 0);
		NVT_LOG("int_trigger_type=%d\n", ts->int_trigger_type);
		ret = request_irq(ts->client->irq, nvt_ts_irq_handler,
			ts->int_trigger_type, ts->client->name, ts);
		if (ret > 0) {
			ret = -1;
			NVT_ERR("tpd request_irq IRQ LINE NOT AVAILABLE!.\n");
		}
	} else {
		NVT_ERR("request_irq can not find touch eint device node!.\n");
		ret = -1;
	}
	NVT_LOG("irq:%d, debounce:%d-%d:\n", ts->client->irq, ints[0], ints[1]);

	return ret;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen check chip version trim function.
 *
 * return:
 *	Executive outcomes. 0---NVT IC. -1---not NVT IC.
 *******************************************************/
static int8_t nvt_ts_check_chip_ver_trim(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {
		nvt_bootloader_reset();
		nvt_sw_reset_idle();

		buf[0] = 0x00;
		buf[1] = 0x35;
		CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		msleep(20);

		buf[0] = 0xFF;
		buf[1] = 0x01;
		buf[2] = 0xF6;
		CTP_I2C_WRITE(ts->client, I2C_BLDR_Address, buf, 3);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_I2C_READ(ts->client, I2C_BLDR_Address, buf, 7);
		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X\n",
			buf[1], buf[2], buf[3]);
		NVT_LOG("buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0;
			list < (sizeof(trim_id_table) /
				sizeof(struct nvt_ts_trim_id_table));
			list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] !=
						trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX)
				found_nvt_chip = 1;

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system =
					trim_id_table[list].carrier_system;
				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(20);
	}

out:
	return ret;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen driver probe function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed
 *******************************************************/
static int32_t nvt_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int32_t ret = 0;
#if ((TOUCH_KEY_NUM > 0) || WAKEUP_GESTURE)
	int32_t retry = 0;
#endif

	NVT_LOG("start\n");

	ts = kmalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		NVT_ERR("failed to allocated memory for nvt ts data\n");
		return -ENOMEM;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	//---request INT-pin---
	NVT_GPIO_AS_INT(GTP_INT_PORT);

	//---check i2c func.---
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		NVT_ERR("i2c_check_functionality failed. (no I2C_FUNC_I2C)\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	// need 10ms delay after POR(power on reset)
	msleep(20);

	//---check input device---
	if (tpd->dev == NULL) {
		NVT_LOG("input device tpd->dev is NULL\n");
		//---allocate input device---
		ts->input_dev = input_allocate_device();
		if (ts->input_dev == NULL) {
			NVT_ERR("allocate input device failed\n");
			ret = -ENOMEM;
			goto err_input_dev_alloc_failed;
		}

		//---register input device---
		ts->input_dev->name = NVT_TS_NAME;
		ret = input_register_device(ts->input_dev);
		if (ret) {
			NVT_ERR("register input device (%s) failed. ret=%d\n",
				ts->input_dev->name, ret);
			goto err_input_register_device_failed;
		}
	} else {
		ts->input_dev = tpd->dev;
	}

#ifdef CONFIG_MTK_I2C_EXTENSION
#if I2C_DMA_SUPPORT
	ts->input_dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	gpDMABuf_va = (uint8_t *)dma_alloc_coherent(&ts->input_dev->dev,
			DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		NVT_ERR("Allocate DMA I2C Buffer failed!\n");
		goto err_dma_alloc_coherent_failed;
	}
	memset(gpDMABuf_va, 0, DMA_MAX_TRANSACTION_LENGTH);
#endif
#endif

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim();
	if (ret) {
		NVT_ERR("chip is not identified\n");
		ret = -EINVAL;
		goto err_chipvertrim_failed;
	}

	mutex_init(&ts->lock);

	mutex_lock(&ts->lock);
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);
	nvt_get_fw_info();
	mutex_unlock(&ts->lock);

	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		NVT_ERR("failed to create kernel thread: %d\n", ret);
		goto err_create_kthread_failed;
	}

	ts->max_touch_num = TOUCH_MAX_FINGER_NUM;

#if TOUCH_KEY_NUM > 0
	ts->max_button_num = TOUCH_KEY_NUM;
#endif

	ts->int_trigger_type = INT_TRIGGER_TYPE;


	//---set input device info.---
	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) |
				BIT_MASK(EV_KEY) |
				BIT_MASK(EV_ABS);
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	ts->input_dev->propbit[0] = BIT(INPUT_PROP_DIRECT);

#if MT_PROTOCOL_B
	input_mt_init_slots(ts->input_dev, ts->max_touch_num, 0);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
		0, TOUCH_FORCE_NUM, 0, 0);    //pressure = TOUCH_FORCE_NUM

#if TOUCH_MAX_FINGER_NUM > 1
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		0, 255, 0, 0);    //area = 255

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		0, ts->abs_y_max, 0, 0);
#if MT_PROTOCOL_B
	/* no need to set ABS_MT_TRACKING_ID,
	 *input_mt_init_slots() already set it
	 */
#else
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0,
					ts->max_touch_num, 0, 0);
#endif //MT_PROTOCOL_B
#endif //TOUCH_MAX_FINGER_NUM > 1

#if TOUCH_KEY_NUM > 0
	for (retry = 0; retry < ts->max_button_num; retry++) {
		input_set_capability(ts->input_dev, EV_KEY,
			touch_key_array[retry]);
	}
#endif

#if WAKEUP_GESTURE
	for (retry = 0;
		retry < (sizeof(gesture_key_array) /
			sizeof(gesture_key_array[0]));
		retry++) {
		input_set_capability(ts->input_dev,
			EV_KEY, gesture_key_array[retry]);
	}
	wake_lock_init(&gestrue_wakelock, WAKE_LOCK_SUSPEND, "poll-wake-lock");
#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;

	ret = nvt_irq_registration();
	if (ret != 0) {
		NVT_ERR("request irq failed. ret=%d\n", ret);
		goto err_int_request_failed;
	} else {
		disable_irq(client->irq);
		NVT_LOG("request irq %d succeed\n", client->irq);
	}

#if BOOT_UPDATE_FIRMWARE
	nvt_fwu_wq = create_singlethread_workqueue("nvt_fwu_wq");
	if (!nvt_fwu_wq) {
		NVT_ERR("nvt_fwu_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_fwu_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->nvt_fwu_work, Boot_Update_Firmware);
	// please make sure boot update start after display reset(RESX) sequence
	queue_delayed_work(nvt_fwu_wq, &ts->nvt_fwu_work,
				msecs_to_jiffies(14000));
#endif

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_init_NVT_ts;
	}
#endif

	bTouchIsAwake = 1;
	tpd_load_status = 1;
	NVT_LOG("end\n");

	enable_irq(client->irq);

	return 0;

#if (NVT_TOUCH_PROC || NVT_TOUCH_EXT_PROC || NVT_TOUCH_MP)
err_init_NVT_ts:
#endif
	free_irq(client->irq, ts);
#if BOOT_UPDATE_FIRMWARE
err_create_nvt_fwu_wq_failed:
#endif
err_int_request_failed:
err_create_kthread_failed:
	mutex_destroy(&ts->lock);
err_chipvertrim_failed:
#ifdef CONFIG_MTK_I2C_EXTENSION
#if I2C_DMA_SUPPORT
err_dma_alloc_coherent_failed:
	if (gpDMABuf_va)
		dma_free_coherent(NULL, DMA_MAX_TRANSACTION_LENGTH,
					gpDMABuf_va, gpDMABuf_pa);
#endif
#endif
err_input_register_device_failed:
	if (tpd->dev == NULL)
		input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
err_check_functionality_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ts);
	return ret;
}

/*******************************************************
 *Description:
 *	Novatek touchscreen driver release function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 *******************************************************/
static int32_t nvt_ts_remove(struct i2c_client *client)
{
	mutex_destroy(&ts->lock);

	NVT_LOG("Removing driver...\n");

	free_irq(client->irq, ts);
	if (tpd->dev == NULL)
		input_unregister_device(ts->input_dev);
	i2c_set_clientdata(client, NULL);
	kfree(ts);

	return 0;
}

static int nvt_i2c_detect(struct i2c_client *client,
				struct i2c_board_info *info)
{
	strcpy(info->type, NVT_I2C_NAME);
	return 0;
}

static const struct i2c_device_id nvt_ts_id[] = {
	{ NVT_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id nvt_match_table[] = {
	{.compatible = "mediatek,cap_touch"},
	{},
};
#endif

static struct i2c_driver nvt_i2c_driver = {
	.probe = nvt_ts_probe,
	.remove = nvt_ts_remove,
	.detect = nvt_i2c_detect,
	.driver.name = NVT_I2C_NAME,
	.driver = {
		.name = NVT_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = nvt_match_table,
#endif
	},
	.id_table = nvt_ts_id,
};

static int nvt_local_init(void)
{
	int ret = 0;

	NVT_LOG("start\n");

	ret = i2c_add_driver(&nvt_i2c_driver);
	if (ret) {
		NVT_ERR("unable to add i2c driver.\n");
		return -1;
	}

	if (tpd_load_status == 0) {
		NVT_ERR("add error touch panel driver.\n");
		i2c_del_driver(&nvt_i2c_driver);
		return -1;
	}

	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data*/
		tpd_button_setting(tpd_dts_data.tpd_key_num,
				tpd_dts_data.tpd_key_local,
				tpd_dts_data.tpd_key_dim_local);
	}

	tpd_type_cap = 1;

	NVT_LOG("end\n");

	return 0;
}

/*******************************************************
 * Description:
 *	Novatek touchscreen driver suspend function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 *******************************************************/
static void nvt_ts_suspend(struct device *dev)
{
	uint8_t buf[4] = {0};
#if MT_PROTOCOL_B
	uint32_t i = 0;
#endif

	if (!bTouchIsAwake) {
		NVT_LOG("Touch is already suspend\n");
		return;
	}

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	bTouchIsAwake = 0;

#if WAKEUP_GESTURE
	//---write i2c command to enter "wakeup gesture mode"---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x13;
#if 0 // Do not set 0xFF first, ToDo
	buf[2] = 0xFF;
	buf[3] = 0xFF;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 4);
#else
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
#endif

	enable_irq_wake(ts->client->irq);

	NVT_LOG("Enabled touch wakeup gesture\n");

#else // WAKEUP_GESTURE
	disable_irq(ts->client->irq);

	//---write i2c command to enter "deep sleep mode"---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x11;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
#endif // WAKEUP_GESTURE

	/* release all touches */
#if MT_PROTOCOL_B
	for (i = 0; i < ts->max_touch_num; i++) {
		input_mt_slot(ts->input_dev, i);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
#if !MT_PROTOCOL_B
	input_mt_sync(ts->input_dev);
#endif
	input_sync(ts->input_dev);

	msleep(50);

	mutex_unlock(&ts->lock);

	NVT_LOG("end\n");

}

/*******************************************************
 * Description:
 *	Novatek touchscreen driver resume function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 ******************************************************/
static void nvt_ts_resume(struct device *dev)
{
	if (bTouchIsAwake) {
		NVT_LOG("Touch is already resume\n");
		return;
	}

	mutex_lock(&ts->lock);

	NVT_LOG("start\n");

	/* please make sure display reset(RESX) sequence
	 *and mipi dsi cmds sent before this
	 */
	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_REK);

#if !WAKEUP_GESTURE
	enable_irq(ts->client->irq);
#endif
	bTouchIsAwake = 1;

	mutex_unlock(&ts->lock);

	NVT_LOG("end\n");

}

static struct device_attribute *novatek_attrs[] = {
};

static struct tpd_driver_t nvt_device_driver = {
	.tpd_device_name = NVT_I2C_NAME,
	.tpd_local_init = nvt_local_init,
	.suspend = nvt_ts_suspend,
	.resume = nvt_ts_resume,
	.attrs = {
		.attr = novatek_attrs,
		.num  = ARRAY_SIZE(novatek_attrs),
	},
};

/*******************************************************
 *Description:
 *	Driver Install function.
 *
 *return:
 *	Executive Outcomes. 0---succeed. not 0---failed.
 ********************************************************/
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	NVT_LOG("start\n");
	tpd_get_dts_info();

	ret = tpd_driver_add(&nvt_device_driver);
	if (ret < 0) {
		NVT_ERR("failed to add i2c driver");
		goto err_driver;
	}

	NVT_LOG("end\n");

err_driver:
	return ret;
}

static void __exit nvt_driver_exit(void)
{
	tpd_driver_remove(&nvt_device_driver);

#if BOOT_UPDATE_FIRMWARE
	if (nvt_fwu_wq)
		destroy_workqueue(nvt_fwu_wq);
#endif
}

module_init(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
