/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */

#define pr_fmt(fmt)                                                            \
	KBUILD_MODNAME ": " __FILE__ ", function %s, line %d: " fmt, __func__, \
		__LINE__

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

static char *g_match_name;
module_param_named(dtn, g_match_name, charp, 0644);

int g_test_idx = 0;
module_param_named(t, g_test_idx, int, 0644);

static int test_trans_align(struct spi_device *spi)
{
	struct spi_message mssg = { 0 };
	struct spi_transfer tran = { 0 };
	__attribute__((aligned(PAGE_SIZE)))
	u8 buf[] = { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };

	pr_alert("align ptr %pK\n", buf);

	spi_message_init(&mssg);
	tran.tx_buf = buf;
	tran.len = 4;
	spi_message_add_tail(&tran, &mssg);

	return spi_sync(spi, &mssg);
}

static int _test_trans_one(struct spi_device *spi, int num)
{
	struct spi_message mssg = { 0 };
	struct spi_transfer tran = { 0 };
	u8 buf[] = { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };

	spi_message_init(&mssg);
	tran.tx_buf = buf;
	tran.len = num;
	spi_message_add_tail(&tran, &mssg);

	return spi_sync(spi, &mssg);
}

static int test_trans_one_short(struct spi_device *spi)
{
	return _test_trans_one(spi, 2);
}

static int test_trans_one_long(struct spi_device *spi)
{
	return _test_trans_one(spi, 16);
}

static int test_trans_multi(struct spi_device *spi)
{
	struct spi_message mssg = { 0 };
	struct spi_transfer tran[2] = { 0 };
	u8 buf[] = { 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8 };
	u8 bufr[32] = { 0 };

	spi_message_init(&mssg);
	tran[0].tx_buf = buf;
	tran[0].len = 16;
	tran[1].rx_buf = bufr;
	tran[1].len = 32;
	spi_message_add_tail(&tran[0], &mssg);
	spi_message_add_tail(&tran[1], &mssg);

	return spi_sync(spi, &mssg);
}

static int test_trans_malloc(struct spi_device *spi)
{
	struct spi_message mssg = { 0 };
	struct spi_transfer tran[2] = { 0 };
	u8 *buf = devm_kmalloc(&spi->dev, 32, GFP_KERNEL);
	u8 *bufr = devm_kmalloc(&spi->dev, 32, GFP_KERNEL);

	if (buf == NULL || bufr == NULL)
		return -ENOMEM;

	spi_message_init(&mssg);
	tran[0].tx_buf = buf;
	tran[0].len = 16;
	tran[1].rx_buf = bufr;
	tran[1].len = 32;
	spi_message_add_tail(&tran[0], &mssg);
	// spi_message_add_tail(&tran[1], &mssg);

	return spi_sync(spi, &mssg);
}

static int test(struct spi_device *spi)
{
	int ret = 0;

	pr_alert("test %d\n", g_test_idx);
	switch (g_test_idx) {
	case 0:
		ret = 0;
		break;
	case 1:
		ret = test_trans_one_short(spi);
		break;
	case 2:
		ret = test_trans_one_long(spi);
		break;
	case 3:
		ret = test_trans_multi(spi);
		break;
	case 4:
		ret = test_trans_malloc(spi);
		break;
	case 5:
		ret = test_trans_align(spi);
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int ispv4_spi_probe(struct spi_device *spi)
{
	pr_alert("Entry\n");

	spi->max_speed_hz = 1000000;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	pr_alert("Test ret %d\n", test(spi));
	return 0;
}

static int ispv4_spi_remove(struct spi_device *spi)
{
	pr_alert("Entry\n");
	return 0;
}

static struct of_device_id of_match[] = {
	{
		.compatible = "nil",
	},
	{},
};

static struct spi_driver ispv4_spi_drv = {
	.driver = {
		.name = "ispv4-base-spi-test",
		.owner = THIS_MODULE,
		.of_match_table = of_match,
	},
	.probe = ispv4_spi_probe,
	.remove = ispv4_spi_remove,
};

int __init ispv4_base_test_boot_init(void)
{
	if (!g_match_name) {
		pr_alert("no g_match_name\n");
		return -ENOPARAM;
	}

	strncpy(of_match[0].compatible, g_match_name, 16);
	pr_alert("compatible %s\n", of_match[0].compatible);

	return spi_register_driver(&ispv4_spi_drv);
}

void __exit ispv4_base_test_boot_exit(void)
{
	spi_unregister_driver(&ispv4_spi_drv);
}

module_init(ispv4_base_test_boot_init);
module_exit(ispv4_base_test_boot_exit);

MODULE_AUTHOR("Chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 driver");
MODULE_LICENSE("GPL v2");
