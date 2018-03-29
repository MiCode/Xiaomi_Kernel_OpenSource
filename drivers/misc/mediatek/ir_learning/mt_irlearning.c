/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mt_spi.h>
#include "mt_irlearning.h"

static struct mt_irlearning mt_irlearning_dev;
static struct mt_chip_conf irlearning_spi_conf;
static atomic_t ir_usage_cnt;

__weak int get_ir_device(void)
{
	if (atomic_cmpxchg(&ir_usage_cnt, 0, 1) != 0)
		return -EBUSY;
	return 0;
}

__weak int put_ir_device(void)
{
	if (atomic_cmpxchg(&ir_usage_cnt, 1, 0) != 1)
		return -EFAULT;
	return 0;
}

static int dev_char_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = get_ir_device();
	if (ret) {
		pr_err("[IRLEARNING] device busy\n");
		goto exit;
	}

	pr_debug("[IRLEARNING] open by %s\n", current->comm);
	nonseekable_open(inode, file);
exit:
	return ret;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
	int ret = 0;

	ret = put_ir_device();
	if (ret) {
		pr_err("[IRTX] device close without open\n");
		goto exit;
}

	pr_debug("[IRLEARNING] close by %s\n", current->comm);
exit:
	return ret;
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{

	return count;
}

static long dev_char_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int i;
	unsigned char *data_ptr;
	struct spi_message spi_msg;
	struct spi_transfer spi_trf = {0x00};

	if (!mt_irlearning_dev.spi_dev || !mt_irlearning_dev.spi_buffer)
		return -ENODEV;

	switch (cmd) {
	case SPI_IOC_READ_WAVE:
		pr_debug("[IRLEARNING] ioctl read message\n");
		spi_message_init(&spi_msg);
		spi_message_add_tail(&spi_trf, &spi_msg);
		spi_trf.rx_buf = mt_irlearning_dev.spi_buffer;
		spi_trf.len = SPI_BUF_LEN;
		spi_trf.tx_buf = mt_irlearning_dev.spi_buffer;
		memset(spi_trf.rx_buf, 0, spi_trf.len);
		ret = spi_sync(mt_irlearning_dev.spi_dev, &spi_msg);
		pr_debug("[IRLEARNING] spi_sync ret=%d\n", ret);

		/* invert bit */
		if (mt_irlearning_dev.spi_data_invert) {
			pr_debug("[IRLEARNING] invert data\n");
		for (i = 0; i < SPI_BUF_LEN; i++) {
			data_ptr = (unsigned char *)mt_irlearning_dev.spi_buffer + i;
			*data_ptr = ~(*data_ptr);
		}
		}

		if (copy_to_user((void __user *)arg, spi_trf.rx_buf, spi_trf.len)) {
			pr_err("[IRLEARNING] copy_to_user failed\n");
			ret = -EFAULT;
		}
		ret = spi_trf.len;
		break;
	case SPI_IOC_GET_SAMPLE_RATE:
		pr_debug("[IRLEARNING] ioctl get sample rate %d->%d\n",
				mt_irlearning_dev.spi_clock, mt_irlearning_dev.spi_hz);
		ret = put_user(mt_irlearning_dev.spi_hz, (unsigned int __user *)arg);
		break;
	default:
		pr_err("[IRLEARNING] unknown ioctl cmd 0x%x\n", cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static int irlearning_spi_remove(struct spi_device *spi)
{

	pr_debug("[IRLEARNING] remove\n");
	return 0;
}

static int __init irlearning_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	pr_debug("[IRLEARNING] spi probe\n");
	/* update sample rate */
	irlearning_spi_conf.high_time = mt_irlearning_dev.spi_clock / 1000000 / 2;
	irlearning_spi_conf.low_time = mt_irlearning_dev.spi_clock / 1000000 / 2;
	mt_irlearning_dev.spi_hz = mt_irlearning_dev.spi_clock /
			(irlearning_spi_conf.high_time + irlearning_spi_conf.low_time);
	/* keep the rest as default */
	irlearning_spi_conf.setuptime = 3;
	irlearning_spi_conf.holdtime = 3;
	irlearning_spi_conf.cs_idletime = 2;
	irlearning_spi_conf.ulthgh_thrsh = 0;
	if (mt_irlearning_dev.spi_cs_invert)
		irlearning_spi_conf.cs_pol = ACTIVE_HIGH;
	else
		irlearning_spi_conf.cs_pol = ACTIVE_LOW;
	irlearning_spi_conf.cpol = 0;
	irlearning_spi_conf.cpha = 1;
	irlearning_spi_conf.rx_mlsb = 1;
	irlearning_spi_conf.tx_mlsb = 1;
	irlearning_spi_conf.tx_endian = 0;
	irlearning_spi_conf.rx_endian = 0;
	irlearning_spi_conf.com_mod = DMA_TRANSFER;
	irlearning_spi_conf.pause = 0;
	irlearning_spi_conf.finish_intr = 1;
	irlearning_spi_conf.deassert = 0;
	irlearning_spi_conf.ulthigh = 0;
	irlearning_spi_conf.tckdly = 0;

	spi->controller_data = (void *)&irlearning_spi_conf;
	spi->mode = SPI_MODE_3; /* FIXME */
	spi->bits_per_word = 32;
	spi->max_speed_hz = mt_irlearning_dev.spi_hz;

	ret = spi_setup(spi);
	if (ret < 0) {
		pr_err("[IRLEARNING] spi_setup fail ret=%d\n", ret);
		goto exit;
	}
	mt_irlearning_dev.spi_dev = spi;

exit:
	return ret;
}


static struct spi_device_id spi_id_table = {"spi-irlearning", 0};

static struct spi_driver irlearning_spi_driver = {
	.driver = {
		.name = "irlearning_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = irlearning_spi_probe,
	.remove = irlearning_spi_remove,
	.id_table = &spi_id_table,
};

static struct spi_board_info irlearning_spi_device[] __initdata = {
	[0] = {
		.modalias = "spi-irlearning",
		.bus_num = 0,
		.chip_select = 1,
		.mode = SPI_MODE_3,
	},
};

static struct file_operations const char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.release = &dev_char_close,
	.unlocked_ioctl = &dev_char_ioctl,
};

static int irlearning_probe(struct platform_device *plat_dev)
{
	struct cdev *c_dev;
	dev_t dev_t_irlearning;
	struct device *dev = NULL;
	static void *dev_class;
	int ret = 0;

#ifdef CONFIG_OF
	if (plat_dev->dev.of_node == NULL) {
		pr_err("[IRLEARNING] OF node is NULL\n");
		return -ENODEV;
	}
	of_property_read_u32(plat_dev->dev.of_node, "spi_clock", &mt_irlearning_dev.spi_clock);
	of_property_read_u32(plat_dev->dev.of_node, "spi_data_invert", &mt_irlearning_dev.spi_data_invert);
	of_property_read_u32(plat_dev->dev.of_node, "spi_cs_invert", &mt_irlearning_dev.spi_cs_invert);
	pr_warn("[IRLEARNING] device tree info: spi_clock=%d, data_invert=%d, cs_invert=%d\n",
		mt_irlearning_dev.spi_clock, mt_irlearning_dev.spi_data_invert, mt_irlearning_dev.spi_cs_invert);
#endif

	/* create char device */
	ret = alloc_chrdev_region(&dev_t_irlearning, 0, 1, DEV_NAME);
	if (ret) {
		pr_err("[IRLEARNING] alloc_chrdev_region fail ret=%d\n", ret);
		goto exit;
	}
	c_dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!c_dev) {
		ret = -ENOMEM;
		goto exit;
	}
	cdev_init(c_dev, &char_dev_fops);
	c_dev->owner = THIS_MODULE;
	ret = cdev_add(c_dev, dev_t_irlearning, 1);
	if (ret) {
		pr_err("[IRLEARNING] cdev_add fail ret=%d\n", ret);
		goto exit;
	}
	dev_class = class_create(THIS_MODULE, DEV_NAME);
	dev = device_create(dev_class, NULL, dev_t_irlearning, NULL, DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("[IRLEARNING] device_create fail ret=%d\n", ret);
		goto exit;
	}

	/* create SPI device */
	ret = spi_register_board_info(irlearning_spi_device, ARRAY_SIZE(irlearning_spi_device));
	if (ret) {
		pr_err("[IRLEARNING] spi_register_board_info fail ret=%d\n", ret);
		goto exit;
	}
	ret = spi_register_driver(&irlearning_spi_driver);
	if (ret) {
		pr_err("[IRLEARNING] spi_register_driver fail ret=%d\n", ret);
		goto exit;
	}

	/* alloc buffer */
	mt_irlearning_dev.spi_buffer = kzalloc(SPI_BUF_LEN, GFP_KERNEL);
	if (!mt_irlearning_dev.spi_buffer) {
		ret = -ENOMEM;
		goto exit;
	}

 exit:
	return ret;
}

static struct platform_driver irlearning_driver = {
	.driver = {
			.name = DEV_NAME,
		},
	.probe = irlearning_probe,
};

#ifdef CONFIG_OF
static const struct of_device_id irlearning_of_ids[] = {
	{.compatible = "mediatek,irlearning-spi",},
	{}
};
#else
static struct platform_device irlearning_device = {
	.name = DEV_NAME,
};
#endif

static int __init irlearning_init(void)
{
	int ret = 0;

	pr_debug("[IRLEARNING] init\n");

#ifdef CONFIG_OF
	irlearning_driver.driver.of_match_table = irlearning_of_ids;
#else
	ret = platform_device_register(&irlearning_device);
	if (ret) {
		pr_err("[IRLEARNING] platform device register fail %d\n", ret);
		goto exit;
	}
#endif

	ret = platform_driver_register(&irlearning_driver);
	if (ret) {
		pr_err("[IRLEARNING] platform driver register fail %d\n", ret);
		goto exit;
	}

exit:
	return ret;
}

module_init(irlearning_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
