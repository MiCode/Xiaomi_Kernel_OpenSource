// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/random.h>
#include <linux/clk.h>

static bool spis_auto_test_flag;

static void spi_slave_txbuf_malloc(struct spi_transfer *trans)
{
	int i;

	trans->tx_buf = kzalloc(trans->len, GFP_KERNEL);
	for (i = 0; i < trans->len; i++)
		*((char *)trans->tx_buf + i) = i + 1;
}

static void spi_slave_rxbuf_malloc(struct spi_transfer *trans)
{
	trans->rx_buf = kzalloc(trans->len, GFP_KERNEL);
	memset(trans->rx_buf, 0, trans->len);
}

static void spi_slave_txbuf_free(struct spi_transfer *trans)
{
	kfree(trans->tx_buf);
}

static void spi_slave_rxbuf_free(struct spi_transfer *trans)
{
	kfree(trans->rx_buf);
}

static void spi_slave_dump_packet(char *name, u8 *ptr, int len)
{
	int i;

	pr_info("%s: ", name);
	for (i = 0; i < len; i++)
		pr_info(" %02x", ptr[i]);

	pr_info("\n");
}

int spis_loopback_check(struct spi_transfer *trans)
{
	int i, err = 0;

	for (i = 0; i < trans->len; i++) {
		if (*((u8 *) trans->tx_buf + i) != *((u8 *) trans->rx_buf + i))
			err++;
	}

	if (err) {
		pr_info("spis_len:%d, err %d\n", trans->len, err);
		spi_slave_dump_packet("spis tx",
			(char *)trans->tx_buf, trans->len);
		spi_slave_dump_packet("spis rx", trans->rx_buf, trans->len);
		pr_info("spis test fail.\n");
		spis_auto_test_flag = false;
		return -1;
	}

	pr_info("spis_len:%d, err %d\n", trans->len, err);
	pr_info("spis test pass.\n");
	spis_auto_test_flag = true;

	return 0;
}

static int spi_slave_txrx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_txbuf_malloc(&trans);
	spi_slave_rxbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spis_loopback_check(&trans);

	spi_slave_txbuf_free(&trans);
	spi_slave_rxbuf_free(&trans);

	return ret;
}

static int spi_slave_tx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_txbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spi_slave_dump_packet("spis tx", (char *)trans.tx_buf, len);

	spi_slave_txbuf_free(&trans);

	return ret;
}

static int spi_slave_rx_transfer(struct spi_device *spi, int len)
{
	int ret;
	struct spi_transfer trans;
	struct spi_message msg;

	memset(&trans, 0, sizeof(trans));
	trans.len = len;

	spi_slave_rxbuf_malloc(&trans);

	spi_message_init(&msg);
	spi_message_add_tail(&trans, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		pr_info("Message transfer err,line(%d):%d\n", __LINE__, ret);

	spi_slave_dump_packet("spis rx", trans.rx_buf, len);

	spi_slave_rxbuf_free(&trans);

	return ret;
}

static ssize_t spi_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	char *bp = buf;

	if (spis_auto_test_flag)
		bp += sprintf(bp, "spis talk with spim pass\n");
	else
		bp += sprintf(bp, "spis talk with spim fail\n");

	*bp++ = '\n';

	return bp - buf;
}

static ssize_t spi_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int len;
	struct spi_device *spi = container_of(dev, struct spi_device, dev);

	if (!strncmp(buf, "txrx", 4)) {
		buf += 5;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_txrx_transfer(spi, len);
	} else if (!strncmp(buf, "onlytx", 6)) {
		buf += 7;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_tx_transfer(spi, len);
	} else if (!strncmp(buf, "onlyrx", 6)) {
		buf += 7;
		if (!strncmp(buf, "len=", 4))
			if (sscanf(buf+4, "%d", &len) > 0)
				spi_slave_rx_transfer(spi, len);
	}

	return count;
}

static DEVICE_ATTR_RW(spi);

static struct device_attribute *spis_attribute[] = {
	&dev_attr_spi,
};
static void spis_create_attribute(struct device *dev)
{
	int size, idx;

	size = ARRAY_SIZE(spis_attribute);
	for (idx = 0; idx < size; idx++)
		device_create_file(dev, spis_attribute[idx]);
}

static int spi_slave_mt27xx_test_probe(struct spi_device *spi)
{
	spis_create_attribute(&spi->dev);
	return 0;
}

static int spi_slave_mt27xx_test_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver spi_slave_mt27xx_test_driver = {
	.driver = {
		.name	= "spi-slave-mt27xx-test",
	},
	.probe		= spi_slave_mt27xx_test_probe,
	.remove		= spi_slave_mt27xx_test_remove,
};
module_spi_driver(spi_slave_mt27xx_test_driver);

MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL v2");
