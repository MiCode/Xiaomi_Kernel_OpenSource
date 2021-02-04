/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/platform_data/spi-mt65xx.h>

#include "spis-mt27xx.h"

#define SPIS_RX_FIFO_STATUS_CMD		0xA1
#define SPIS_TX_FIFO_STATUS_CMD		0xA5
#define SPIS_DATA_TRANSFER_CMD		0x55

static int spis_debug;
static bool spi_auto_test_flag;

int spis_cmd = SPIS_DATA_TRANSFER_CMD;
void mtk_debug_packet(char *name, u8 *ptr, int len)
{
	int i;

	if (spis_debug)
		pr_info("%s: ", name);
	for (i = 0; i < len; i++)
		if (spis_debug)
			pr_info(" %02x", ptr[i]);

	if (spis_debug)
		pr_info("\n");
}

void spi_txrxbuf_malloc(struct spi_transfer *trans)
{
	int i;

	trans->tx_buf = kzalloc(trans->len, GFP_KERNEL);
	trans->rx_buf = kzalloc(trans->len, GFP_KERNEL);

	for (i = 0; i < trans->len; i++)
		*((char *)trans->tx_buf+i) = i;
}

void spi_txrxbuf_free(struct spi_transfer *trans)
{
	kfree(trans->tx_buf);
	kfree(trans->rx_buf);
}

void spis_kfree(struct spi_transfer *spis_trans)
{
	kfree(spis_trans->tx_buf);
	kfree(spis_trans->rx_buf);
}

void spis_malloc(struct spi_transfer *spis_trans)
{
	int i;

	spis_trans->tx_buf = kzalloc(spis_trans->len, GFP_KERNEL);
	spis_trans->rx_buf = kzalloc(spis_trans->len, GFP_KERNEL);

	for (i = 0; i < spis_trans->len; i++)
		*((char *)spis_trans->tx_buf + i) = i+0x1;
}

int spim_spis_check(struct spi_transfer *trans,
		    struct spi_transfer *spis_trans)
{
	int i, err1 = 0, err2 = 0;

	for (i = 0; i < spis_trans->len; i++) {
		if (*((char *) trans->tx_buf + i + 1) !=
		    *((char *) spis_trans->rx_buf + i)) {
			err1++;
			if (spis_debug)
				pr_info("spis_len:%d, err1 %d\n",
				       spis_trans->len, err1);
			if (spis_debug)
				pr_info("spi tx_buf[%d]:0x%x\n",
				       i+1, *((char *) trans->tx_buf + i+1));
			if (spis_debug)
				pr_info("spi rx_buf[%d]:0x%x\n",
				       i, *((char *) trans->rx_buf + i));
		}

		if (*((char *) spis_trans->tx_buf + i)
		    != *((char *) trans->rx_buf + i + 1)) {
			err2++;
			if (spis_debug)
				pr_info("spis_len:%d, err2 %d\n",
				       spis_trans->len, err2);
			if (spis_debug)
				pr_info("spi tx_buf[%d]:0x%x\n",
				       i, *((char *) trans->tx_buf + i));
			if (spis_debug)
				pr_info("spi rx_buf[%d]:0x%x\n",
				       i + 1, *((char *) trans->rx_buf + i + 1));
		}
	}

	if (err1 || err2) {
		pr_info("spis_len:%d, err1 %d, err2 %d\n",
		       spis_trans->len, err1, err2);
		spis_debug = 1;
		if (spis_debug)
			pr_info("after transfer:\n");
		mtk_debug_packet("spim_tx_buf",
				 (void *)trans->tx_buf, trans->len);
		mtk_debug_packet("spim_rx_buf", trans->rx_buf, trans->len);
		mtk_debug_packet("spis_tx_buf",
				 (u8 *)spis_trans->tx_buf, spis_trans->len);
		mtk_debug_packet("spis_rx_buf",
				 spis_trans->rx_buf, spis_trans->len);
		spis_debug = 0;
		spi_auto_test_flag = false;
	} else {
		if (spis_debug)
			pr_info("After transfer:\n");
		mtk_debug_packet("spim_tx_buf",
				 (void *)trans->tx_buf, trans->len);
		mtk_debug_packet("spim_rx_buf",
				 trans->rx_buf, trans->len);
		mtk_debug_packet("spis_tx_buf",
				 (u8 *)spis_trans->tx_buf, spis_trans->len);
		mtk_debug_packet("spis_rx_buf",
				 spis_trans->rx_buf, spis_trans->len);

		pr_info("spis_len:%d, err1 %d, err2 %d\n",
		       spis_trans->len, err1, err2);
		spi_auto_test_flag = true;
	}

	if (spi_auto_test_flag) {
		pr_info("spis2spim test pass.\n");
		return 0;
	} else {
		pr_info("spis2spim test fail.\n");
		return -1;
	}
}

int spis2spim_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;
	struct spi_transfer spis_trans;

	/* init spim transfer */
	memset(&trans, 0, sizeof(trans));
	trans.cs_change = 0;
	trans.len = len;

	spi_txrxbuf_malloc(&trans);

	*((char *)trans.tx_buf) = spis_cmd;

	spis_trans.len = len-1;
	spis_malloc(&spis_trans);

	mtk_spis_transfer_one(spi, &spis_trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);
	if (spi_sync(spi, &msg))
		pr_info("%s line%d:spi_sync error\n", __func__, __LINE__);

	mtk_spis_wait_for_transfer_done(spi);

	if (spim_spis_check(&trans, &spis_trans))
		return -1;

	spi_txrxbuf_free(&trans);
	spis_kfree(&spis_trans);

	return ret;
}

static ssize_t spis_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	char *bp = buf;

	if (spi_auto_test_flag)
		bp += sprintf(bp, "spim talk with spis pass\n");
	else
		bp += sprintf(bp, "spim talk with spis fail\n");

	*bp++ = '\n';

	return bp - buf;
}

static ssize_t spis_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	if (!strncmp(buf, "spidebug", 8)) {
		buf += 9;
		if (!kstrtou32(buf, 0, &spis_debug))
			pr_info("spis_debug[%d]\n", spis_debug);
	} else if (!strncmp(buf, "spiscmd", 7)) {
		buf += 8;
		if (!kstrtou32(buf, 0, &spis_cmd))
			pr_info("spis_cmd[0x%x]\n", spis_cmd);
	} else if (!strncmp(buf, "spis2spim", 9)) {
		int len;

		buf += 10;
		if (!strncmp(buf, "len=", 4)) {
			if (sscanf(buf + 4, "%d", &len) != 1)
				pr_info("len fail\n");
		}
		spis2spim_transfer(spi, len);
	}

	return count;
}

static DEVICE_ATTR(spi, 0644, spis_show, spis_store);

static struct device_attribute *spis_attribute[] = {
	&dev_attr_spi,
};

int mtk_spis_create_attribute(struct device *dev)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(spis_attribute); idx++)
		device_create_file(dev, spis_attribute[idx]);

	return 0;
}
