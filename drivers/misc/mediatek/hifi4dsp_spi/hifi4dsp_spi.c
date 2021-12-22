/*
 * Copyright (C) 2018 MediaTek Inc.
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
#define HIFI4DSP_SPI_DRV_NAME	"hifi4dsp-spi"
#define pr_fmt(fmt) HIFI4DSP_SPI_DRV_NAME ": " fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/platform_data/spi-mt65xx.h>
#include "hifi4dsp_spi.h"
/*
 * SPI command description.
 */
#define CMD_PWOFF			0x02 /* Power Off */
#define CMD_PWON			0x04 /* Power On */
#define CMD_RS				0x06 /* Read Status */
#define CMD_WS				0x08 /* Write Status */
#define CMD_CR				0x0a /* Config Read */
#define CMD_CW				0x0c /* Config Write */
#define CMD_RD				0x81 /* Read Data */
#define CMD_WD				0x0e /* Write Data */
#define CMD_CT				0x10 /* Config Type */
/*
 * SPI slave status register (to master).
 */
#define SLV_ON				BIT(0)
#define SR_CFG_SUCCESS			BIT(1)
#define SR_TXRX_FIFO_RDY		BIT(2)
#define SR_RD_ERR			BIT(3)
#define SR_WR_ERR			BIT(4)
#define SR_RDWR_FINISH			BIT(5)
#define SR_TIMEOUT_ERR			BIT(6)
#define SR_CMD_ERR			BIT(7)
#define CONFIG_READY  ((SR_CFG_SUCCESS | SR_TXRX_FIFO_RDY))
/*
 * hardware limit for once transfter.
 */
#define MAX_SPI_XFER_SIZE_ONCE		(64 * 1024 - 1)
#define MAX_SPI_TRY_CNT			(10)
/*
 * default never pass more than 32 bytes
 */
#define MTK_SPI_BUFSIZ	min(32, SMP_CACHE_BYTES)
#define SPI_READ		     true
#define SPI_WRITE		     false
#define SPI_READ_STA_ERR_RET	(1)
#define DSP_SPIS1_CLKSEL_ADDR	(0x1d00e0cc)
#define SPI_FREQ_52M		(52*1000*1000)
#define SPI_FREQ_26M		(26*1000*1000)
#define SPI_FREQ_13M		(13*1000*1000)
/* HIFI4DSP specific SPI data */
struct mtk_hifi4dsp_spi_data {
	int spi_bus_idx;
	int reserved;
	void *spi_bus_data[2];
};

static DEFINE_MUTEX(hifi4dsp_bus_lock);
static struct mtk_hifi4dsp_spi_data hifi4dsp_spi_data;
static int hifi4dsp_spi_init_done;
static int default_spi_trans_mode = 2;
static int spi_config_type_wr(struct spi_device *spi, int type, u32 addr,
						  int len, bool wr, u32 speed)
{
	int status, i, try = 0;
	u8 tx_cmd_type_single[] = {CMD_CT, 0x04}; // config type
	u8 tx_cmd_type_dual[]   = {CMD_CT, 0x05};
	u8 tx_cmd_type_quad[]   = {CMD_CT, 0x06};
	u8 cmd_config[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};
	u8 tx_cmd_read_sta[2] = {CMD_RS, 0x00};
	u8 rx_cmd_read_sta[2] = {0, 0};
	u8 read_status;
	void *buffer;
	struct spi_transfer x[3];
	struct spi_message message;
loop:
	spi_message_init(&message);
	memset(x, 0, sizeof(x));
	memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
	if (type == 2) {
		buffer = tx_cmd_type_quad;
	} else if (type == 1) {
		buffer = tx_cmd_type_dual;
	} else if (type == 0) {
		buffer = tx_cmd_type_single;
	} else {
		status = -EINVAL;
		pr_notice("Input wrong type!\n");
		goto tail;
	}
	x[0].tx_buf	= buffer;
	x[0].len		= ARRAY_SIZE(tx_cmd_type_quad);
	x[0].tx_nbits	= SPI_NBITS_SINGLE;
	x[0].rx_nbits	= SPI_NBITS_SINGLE;
	x[0].speed_hz	= speed;
	x[0].cs_change = 1;
	spi_message_add_tail(&x[0], &message);
	if (wr)
		cmd_config[0] = CMD_CR;
	else
		cmd_config[0] = CMD_CW;
	for (i = 0; i < 4; i++) {
		cmd_config[1 + i] = (addr & (0xff << (i * 8))) >> (i * 8);
		cmd_config[5 + i] = ((len - 1) & (0xff << (i * 8))) >> (i * 8);
	}
	x[1].tx_buf	= cmd_config;
	x[1].len		= ARRAY_SIZE(cmd_config);
	x[1].tx_nbits	= SPI_NBITS_SINGLE;
	x[1].rx_nbits	= SPI_NBITS_SINGLE;
	x[1].speed_hz	= speed;
	x[1].cs_change = 1;
	spi_message_add_tail(&x[1], &message);
	x[2].tx_buf	= tx_cmd_read_sta;
	x[2].rx_buf	= rx_cmd_read_sta;
	x[2].len		= ARRAY_SIZE(tx_cmd_read_sta);
	x[2].tx_nbits	= SPI_NBITS_SINGLE;
	x[2].rx_nbits	= SPI_NBITS_SINGLE;
	x[2].speed_hz	= speed;
	spi_message_add_tail(&x[2], &message);
	status = spi_sync(spi, &message);
	if (status)
		goto tail;
	read_status = rx_cmd_read_sta[1];
	if ((read_status & CONFIG_READY) != CONFIG_READY) {
		pr_notice("SPI slave status error: 0x%x, line:%d\n",
				read_status, __LINE__);
		if (try++ <= MAX_SPI_TRY_CNT)
			goto loop;
	}
tail:
	if (status) {
		pr_notice("config type & addr & len err, line(%d), type(%d), ret(%d)\n",
				__LINE__, type, status);
	}
	return status;
}
static int spi_trigger_wr_data(struct spi_device *spi,
			int type, int len, bool wr, void *buf_store, u32 speed)
{
	int status;
	struct spi_message msg;
	struct spi_transfer x[3];
	size_t size;
	void *local_buf = NULL;
	u8 mtk_spi_buffer[MTK_SPI_BUFSIZ];
	u8 tx_cmd_read_sta[2] = {CMD_RS, 0x00};
	u8 rx_cmd_read_sta[2] = {0, 0};
	u8 tx_cmd_write_sta[2] = {CMD_WS, 0x01};
	u8 rx_cmd_write_sta[2] = {0, 0};
	u8 read_status;
	u32 retry_count = 0;

	memset(x, 0, sizeof(x));
	if (!buf_store) {
		status = -EINVAL;
		goto tail;
	}
	size = len + 1;
	if (size > MTK_SPI_BUFSIZ) {
		local_buf = kzalloc(size, GFP_KERNEL);
		if (!local_buf) {
			status = -ENOMEM;
			pr_notice("tx/rx malloc fail!, line:%d\n", __LINE__);
			goto tail;
		}
	} else {
		local_buf = mtk_spi_buffer;
		memset(local_buf, 0, MTK_SPI_BUFSIZ);
	}
	x[0].tx_nbits = SPI_NBITS_SINGLE;
	x[0].rx_nbits = SPI_NBITS_SINGLE;
	if (type == 1) {
		x[0].tx_nbits = SPI_NBITS_DUAL;
		x[0].rx_nbits = SPI_NBITS_DUAL;
	} else if (type == 2) {
		x[0].tx_nbits = SPI_NBITS_QUAD;
		x[0].rx_nbits = SPI_NBITS_QUAD;
	}
	x[0].len = size;
	x[0].speed_hz = speed;
	x[0].cs_change = 1;
	if (wr) {// read data
		*((u8 *)local_buf) = CMD_RD;
		x[0].tx_buf = local_buf;
		x[0].rx_buf = local_buf;
	} else {
		*((u8 *)local_buf) = CMD_WD;// write data
		memcpy((u8 *)local_buf + 1, buf_store, len); // CMD + data
		x[0].tx_buf = local_buf;
	}
	spi_message_init(&msg);
	spi_message_add_tail(&x[0], &msg);
	/*
	 * Check SPI-Slave Read Status,
	 * SR_RDWR_FINISH = 1 & RD_ERR/WR_ERR = 0 ???
	 */
	memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
	x[1].tx_buf	= tx_cmd_read_sta;           // read status
	x[1].rx_buf = rx_cmd_read_sta;
	x[1].len		= ARRAY_SIZE(tx_cmd_read_sta);
	x[1].tx_nbits	= SPI_NBITS_SINGLE;
	x[1].rx_nbits	= SPI_NBITS_SINGLE;
	x[1].speed_hz	= speed;
	spi_message_add_tail(&x[1], &msg);
	status = spi_sync(spi, &msg);
	if (status)
		goto tail;
	read_status = rx_cmd_read_sta[1];
	if (((read_status & SR_RD_ERR) == SR_RD_ERR) ||
		((read_status & SR_WR_ERR) == SR_WR_ERR) ||
		((read_status & SR_TIMEOUT_ERR) == SR_TIMEOUT_ERR)) {
		pr_notice("SPI slave status error: 0x%x, line:%d\n",
				read_status, __LINE__);
		x[2].tx_buf	= tx_cmd_write_sta;		// write status
		x[2].rx_buf = rx_cmd_write_sta;
		x[2].len		= ARRAY_SIZE(tx_cmd_write_sta);
		x[2].tx_nbits	= SPI_NBITS_SINGLE;
		x[2].rx_nbits	= SPI_NBITS_SINGLE;
		x[2].speed_hz	= speed;
		spi_message_init(&msg);
		spi_message_add_tail(&x[2], &msg);
		status = spi_sync(spi, &msg);
		if (status)
			goto tail;
		do {
			memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
			x[1].tx_buf	= tx_cmd_read_sta;           // read status
			x[1].rx_buf = rx_cmd_read_sta;
			x[1].len		= ARRAY_SIZE(tx_cmd_read_sta);
			x[1].tx_nbits	= SPI_NBITS_SINGLE;
			x[1].rx_nbits	= SPI_NBITS_SINGLE;
			x[1].speed_hz	= speed;

			spi_message_init(&msg);
			spi_message_add_tail(&x[1], &msg);
			status = spi_sync(spi, &msg);
			if (status)
				goto tail;
			retry_count++;
			read_status = rx_cmd_read_sta[1];
		} while ((((read_status & SR_RD_ERR) == SR_RD_ERR) ||
			((read_status & SR_WR_ERR) == SR_WR_ERR) ||
			((read_status & SR_TIMEOUT_ERR) == SR_TIMEOUT_ERR)) &&
			(retry_count < 100000));
		status = SPI_READ_STA_ERR_RET;
	} else {
		while (((read_status & SR_RDWR_FINISH) != SR_RDWR_FINISH) &&
			(retry_count < 100000)) {
			pr_notice("SPI slave r/w not finished: 0x%x, line:%d\n",
					read_status, __LINE__);
			memset(rx_cmd_read_sta, 0, ARRAY_SIZE(rx_cmd_read_sta));
			x[1].tx_buf	= tx_cmd_read_sta;           // read status
			x[1].rx_buf = rx_cmd_read_sta;
			x[1].len		= ARRAY_SIZE(tx_cmd_read_sta);
			x[1].tx_nbits	= SPI_NBITS_SINGLE;
			x[1].rx_nbits	= SPI_NBITS_SINGLE;
			x[1].speed_hz	= speed;
			spi_message_init(&msg);
			spi_message_add_tail(&x[1], &msg);
			status = spi_sync(spi, &msg);
			if (status)
				goto tail;
			retry_count++;
			read_status = rx_cmd_read_sta[1];
		}
		if (retry_count >= 100000)
			status = SPI_READ_STA_ERR_RET;
	}
tail:
	/* Only for successful read */
	if (wr && !status)
		memcpy(buf_store, ((u8 *)x[0].rx_buf + 1), len);
	if (local_buf != mtk_spi_buffer)
		kfree(local_buf);
	if (status)
		pr_notice("write/read to slave err, line(%d), len(%d), ret(%d)\n",
				__LINE__, len, status);
	return status;
}
int dsp_spi_write(u32 addr, void *value, int len, u32 speed)
{
	int ret, try = 0, xfer_speed;
	int type = default_spi_trans_mode;
	struct spi_device *spi = hifi4dsp_spi_data.spi_bus_data[0];
	void *tx_store;

	pr_notice("%s addr = 0x%08x, len = %d\n", __func__, addr, len);
	xfer_speed = speed;
	mutex_lock(&hifi4dsp_bus_lock);
spi_config_write:
	ret = spi_config_type_wr(spi, type, addr, len, SPI_WRITE, xfer_speed);
	if (ret < 0) {
		pr_notice("SPI config write fail! line:%d\n", __LINE__);
		goto tail;
	}
	tx_store = value;
	ret = spi_trigger_wr_data(spi, type, len, SPI_WRITE, tx_store,
				  xfer_speed);
	if (ret < 0) {
		pr_notice("SPI write data error! line:%d\n", __LINE__);
		goto tail;
	}
	if (ret > 0) {
		if (try++ < MAX_SPI_TRY_CNT)
			goto spi_config_write;
		else
			pr_notice("SPI write fail, retry count > %d, line:%d\n",
				 MAX_SPI_TRY_CNT, __LINE__);
	}
tail:
	mutex_unlock(&hifi4dsp_bus_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(dsp_spi_write);
int dsp_spi_write_ex(u32 addr, void *value, int len, u32 speed)
{
	int ret = 0;
	int res_len;
	int once_len;
	int loop;
	int cycle;
	u32 new_addr;
	u8 *new_buf;

	once_len = MAX_SPI_XFER_SIZE_ONCE;
	cycle = len / once_len;
	res_len = len % once_len;
	for (loop = 0; loop < cycle; loop++) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_write(new_addr, new_buf, once_len, speed);
		if (ret)
			pr_notice("dsp_spi_write() fail! line:%d\n", __LINE__);
	}
	if (res_len) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_write(new_addr, new_buf, res_len, speed);
		if (ret)
			pr_notice("dsp_spi_write() fail! line:%d\n", __LINE__);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dsp_spi_write_ex);
int dsp_spi_read(u32 addr, void *value, int len, u32 speed)
{
	int ret, try = 0, xfer_speed;
	int type = default_spi_trans_mode;
	struct spi_device *spi = hifi4dsp_spi_data.spi_bus_data[0];

	pr_notice("%s addr = 0x%08x, len = %d\n", __func__, addr, len);
	xfer_speed = speed;
	mutex_lock(&hifi4dsp_bus_lock);
spi_config_read:
	ret = spi_config_type_wr(spi, type, addr, len, SPI_READ, xfer_speed);
	if (ret < 0) {
		pr_notice("SPI config write fail! line:%d\n", __LINE__);
		goto tail;
	}
	ret = spi_trigger_wr_data(spi, type, len, SPI_READ, value, xfer_speed);
	if (ret < 0) {
		pr_notice("SPI read data error! line:%d\n", __LINE__);
		goto tail;
	}
	if (ret > 0) {
		if (try++ < MAX_SPI_TRY_CNT)
			goto spi_config_read;
		else
			pr_debug("SPI read fail, retry count > %d, line:%d\n",
				 MAX_SPI_TRY_CNT, __LINE__);
	}
	pr_notice("[mt6382] spi read regiter %d", *((u32 *)value));
tail:
	mutex_unlock(&hifi4dsp_bus_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(dsp_spi_read);
int dsp_spi_read_ex(u32 addr, void *value, int len, u32 speed)
{
	int ret = 0;
	int res_len;
	int once_len;
	int loop;
	int cycle;
	u32 new_addr;
	u8 *new_buf;

	once_len = MAX_SPI_XFER_SIZE_ONCE;
	cycle = len / once_len;
	res_len = len % once_len;
	for (loop = 0; loop < cycle; loop++) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_read(new_addr, new_buf, once_len, speed);
		if (ret)
			pr_notice("dsp_spi_read() fail! line:%d\n", __LINE__);
	}
	if (res_len) {
		new_addr = addr + once_len * loop;
		new_buf = (u8 *)value + once_len * loop;
		ret = dsp_spi_read(new_addr, new_buf, res_len, speed);
		if (ret)
			pr_notice("dsp_spi_read() fail! line:%d\n", __LINE__);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(dsp_spi_read_ex);
int spi_read_register(u32 addr, u32 *val, u32 speed)
{
	return dsp_spi_read(addr, (u8 *)val, 4, speed);
}
int spi_write_register(u32 addr, u32 val, u32 speed)
{
	return dsp_spi_write(addr, (u8 *)&val, 4, speed);
}
int spi_set_register32(u32 addr, u32 val, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, read_val | val, speed);
	return 0;
}
int spi_clr_register32(u32 addr, u32 val, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, read_val & (~val), speed);
	return 0;
}
int spi_write_register_mask(u32 addr, u32 val, u32 msk, u32 speed)
{
	u32 read_val;

	spi_read_register(addr, &read_val, speed);
	spi_write_register(addr, ((read_val & (~(msk))) | ((val) & (msk))),
						speed);
	return 0;
}
//#define DSP_ADDR 0x1fc00000
static u32 dsp_addr = 0x1fc00000;
int spi_multipin_loopback_transfer(int len, int xfer_speed)
{
	int ret = 0;
	void *tx_buf;
	void *rx_buf;
	int i, err = 0;

	pr_info("%s entry...\n", __func__);
	tx_buf = kzalloc(len, GFP_KERNEL);
	rx_buf = kzalloc(len, GFP_KERNEL);
	for (i = 0; i < len; i++)
		*((char *)tx_buf + i) = i%255;
	memset(rx_buf, 0, len);
	ret = dsp_spi_write_ex(dsp_addr, tx_buf, len, xfer_speed*1000*1000);
	if (ret < 0) {
		pr_debug("Write transfer err,line(%d):%d\n", __LINE__,
				ret);
		goto tail;
	}
	ret = dsp_spi_read_ex(dsp_addr, rx_buf, len, xfer_speed*1000*1000);
	if (ret < 0) {
		pr_debug("Read transfer err,line(%d):%d\n", __LINE__,
				ret);
		goto tail;
	}
#if 0
	if (xfer_speed == 13) {
		ret = dsp_spi_write_ex(dsp_addr, tx_buf, len, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("Write transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
		ret = dsp_spi_read_ex(dsp_addr, rx_buf, len, SPI_SPEED_LOW);
		if (ret < 0) {
			pr_debug("Read transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
	} else if (xfer_speed == 52) {
		dsp_spi_write_ex(dsp_addr, tx_buf, len, SPI_SPEED_HIGH);
		if (ret < 0) {
			pr_debug("Write transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
		dsp_spi_read_ex(dsp_addr, rx_buf, len, SPI_SPEED_HIGH);
		if (ret < 0) {
			pr_debug("Read transfer err,line(%d):%d\n", __LINE__,
				 ret);
			goto tail;
		}
	} else {
		pr_debug("Unavailabel speed!\n");
		goto tail;
	}
#endif
	for (i = 0; i < len; i++) {
		if (*((char *)tx_buf+i) != *((char *)rx_buf+i)) {
			pr_debug("tx[%d]:0x%x, rx[%d]:0x%x\r\n",
				i, *((char *)tx_buf+i), i,
				*((char *)rx_buf + i));
			err++;
		}
	}
	pr_debug("total length %d bytes, err %d bytes.\n", len, err);
	pr_info("%s quit...\n", __func__);
tail:
	kfree(tx_buf);
	kfree(rx_buf);
	if (ret < 0)
		return ret;
	return err;
}
static ssize_t hifi4dsp_spi_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int len, xfer_speed, ret;

	if (!strncmp(buf, "addr=", 5) &&
		(sscanf(buf + 5, "%x", &dsp_addr) == 1)) {
		buf += 16;
		if (!strncmp(buf, "speed=", 6) &&
			(sscanf(buf + 6, "%d", &xfer_speed) == 1)) {
			buf += 9;
			if (!strncmp(buf, "len=", 4) &&
				(sscanf(buf + 4, "%d", &len) == 1)) {
				pr_info("**dump set**\n addr = 0x%x, speed = %d, len = %d\n",
						 dsp_addr, xfer_speed, len);
				ret = spi_multipin_loopback_transfer(len,
						xfer_speed);
			}
		}
	}
	return count;
}
static DEVICE_ATTR(hifi4dsp_spi, 0200, NULL, hifi4dsp_spi_store);
static struct device_attribute *spi_attribute[] = {
	&dev_attr_hifi4dsp_spi,
};
static void spi_create_attribute(struct device *dev)
{
	int size, idx, ret;

	size = ARRAY_SIZE(spi_attribute);
	for (idx = 0; idx < size; idx++) {
		ret = device_create_file(dev, spi_attribute[idx]);
		if (ret != 0)
			pr_info("device_create_file fail!\n");
	}
}
int hifi4dsp_spi_get_status(void)
{
	return hifi4dsp_spi_init_done;
}
static int hifi4dsp_spi_probe(struct spi_device *spi)
{
	int err = 0, ret = 0, tick_delay = 0;
	static struct task_struct *dsp_task;
	struct device_node *nc = spi->dev.of_node;
	struct mtk_chip_config *data;
	struct mtk_hifi4dsp_spi_data *pri_data = &hifi4dsp_spi_data;

	pr_info("%s() enter.\n", __func__);
	data = kzalloc(sizeof(struct mtk_chip_config), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto tail;
	}
	ret = of_property_read_u32(nc, "tick-dly", &tick_delay);
	if (ret) {
		pr_info("tick-dly isn't setting!\n");
		tick_delay = 0;
	} else
		pr_info("tick-dly = %d\n", tick_delay);
	ret = of_property_read_u32(nc, "spi-pin-mode", &default_spi_trans_mode);
	if (ret) {
		pr_info("spi-pin-mode isn't setting!\n");
		default_spi_trans_mode = 2;
	} else
		pr_info("spi-pin-mode = %d\n", default_spi_trans_mode);
	/*
	 * Structure filled with mtk-spi crtical values.
	 */
	spi->bits_per_word = 8;
	data->rx_mlsb = 0;
	data->tx_mlsb = 0;
	//data->command_cnt = 1; //for six pin spi
	//data->dummy_cnt = 0;	 //for six pin spi
	data->tick_delay = tick_delay;
	spi->controller_data = (void *)data;
	/* Fill  structure mtk_hifi4dsp_spi_data */
	pri_data->spi_bus_data[pri_data->spi_bus_idx++] = spi;
	dsp_task = NULL;
	spi_create_attribute(&spi->dev);
	hifi4dsp_spi_init_done = 1;
tail:
	return err;
}
static int hifi4dsp_spi_remove(struct spi_device *spi)
{
	pr_info("%s().\n", __func__);
	if (spi && spi->controller_data)
		kfree(spi->controller_data);
	return 0;
}
static const struct spi_device_id hifi4dsp_spi_ids[] = {
	{ "mt8570" },
	{}
};
MODULE_DEVICE_TABLE(spi, hifi4dsp_spi_ids);
static const struct of_device_id hifi4dsp_spi_of_ids[] = {
	{ .compatible = "mediatek,hifi4dsp-spi" },
	{}
};
MODULE_DEVICE_TABLE(of, hifi4dsp_spi_of_ids);
static struct spi_driver hifi4dsp_spi_drv = {
	.driver = {
		.name	= HIFI4DSP_SPI_DRV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = hifi4dsp_spi_of_ids,
	},
	.id_table	= hifi4dsp_spi_ids,
	.probe	= hifi4dsp_spi_probe,
	.remove	= hifi4dsp_spi_remove,
};
module_spi_driver(hifi4dsp_spi_drv);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dehui Sun <dehui.sun@mediatek.com>");
MODULE_DESCRIPTION("SPI driver for hifi4dsp chip");
