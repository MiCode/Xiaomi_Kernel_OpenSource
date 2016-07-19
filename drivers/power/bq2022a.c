#define DEBUG
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/regulator/consumer.h>

#include <linux/platform_device.h>

int battery_is_matching = 0;
static int gpio_id = 0;

static spinlock_t lock;

const unsigned char cos_battery_data[64] = {
	0xed, 0x21, 0x4c, 0xe5, 0xed, 0xa9, 0x4b, 0x2e,
	0x62, 0xd4, 0xc6, 0xbe, 0x5e, 0xc8, 0xb6, 0xf5,
	0x51, 0x53, 0x0f, 0xd9, 0xdb, 0xde, 0x60, 0xcc,
	0xd2, 0xfd, 0x93, 0xfb, 0x55, 0x2f, 0x74, 0x51,
	0x48, 0x91, 0x8a, 0x67, 0x48, 0xb0, 0x7d, 0xcc,
	0x70, 0xf4, 0x90, 0xa1, 0x3f, 0x9b, 0xe4, 0x3c,
	0xb5, 0xcd, 0x7f, 0xb8, 0x08, 0x6f, 0xbd, 0x31,
	0x2f, 0xa6, 0x51, 0x15, 0xa5, 0x7a, 0x0c, 0xdf};
const unsigned char acc_battery_data[64] = {
	0xed, 0x21, 0x4c, 0xe5, 0xed, 0xa9, 0x4b, 0x2e,
	0x62, 0xd4, 0xc6, 0xbe, 0x5e, 0xc8, 0xb6, 0xf5,
	0x83, 0x24, 0x62, 0x8e, 0xb1, 0x0b, 0xe1, 0x1a,
	0xb9, 0xcd, 0x76, 0xef, 0x28, 0x0e, 0xc0, 0x01,
	0x77, 0x3d, 0x20, 0x9f, 0xd9, 0x26, 0x5f, 0xf6,
	0xdc, 0x67, 0x64, 0xf2, 0x84, 0x87, 0x63, 0x4b,
	0x24, 0x7b, 0xa7, 0x97, 0x66, 0x73, 0x44, 0xb1,
	0x71, 0x42, 0x3b, 0xdf, 0xc8, 0xad, 0x8b, 0x86};

const unsigned char des_battery_data[64] = {
	0xed, 0x21, 0x4c, 0xe5, 0xed, 0xa9, 0x4b, 0x2e,
	0x62, 0xd4, 0xc6, 0xbe, 0x5e, 0xc8, 0xb6, 0xf5,
	0xcc, 0xd6, 0xfb, 0x4c, 0xf2, 0xb1, 0xbc, 0x2e,
	0xdb, 0x05, 0xe7, 0xf4, 0x22, 0x22, 0xcf, 0xf7,
	0x90, 0xd5, 0x14, 0x9d, 0xdb, 0x3f, 0x97, 0x76,
	0xc3, 0xf9, 0x17, 0x8c, 0x25, 0xbb, 0x08, 0x59,
	0x92, 0xe3, 0xf9, 0xf9, 0xd6, 0x79, 0xb2, 0x98,
	0x3c, 0x3c, 0xb9, 0x47, 0x7b, 0xe5, 0x12, 0x84};

static unsigned char battery_read_data[64];


static int bq2022a_init(void)
{
	int id = gpio_id;
	int state;
	int i;
	unsigned long flag;

	spin_lock_irqsave(&lock, flag);
	gpio_direction_output(id, 1);
	udelay(500);
	gpio_direction_output(id, 0);
	udelay(600);
	gpio_direction_output(id, 1);
	udelay(30);
	gpio_direction_input(id);
	udelay(50);
	for (i = 0; i < 4; i++) {
		state = gpio_get_value(id);
		if (state == 0) {
			pr_err("bq2022a_init fail!\n");
			udelay(60);
		} else {
			pr_err("bq2022a_init_ok!\n");
			break;
		}
	}
	spin_unlock_irqrestore(&lock, flag);
	return 0;
}

static void bq2022a_write_bit(int on)
{
	udelay(80);
	gpio_direction_output(gpio_id, 0);
	udelay(8);
	if (on) {
		gpio_direction_output(gpio_id, 1);
		udelay(60);
	} else
		udelay(60);

	gpio_direction_input(gpio_id);

}

static void bq2022a_write_byte(unsigned char byte)
{
	int i;
	uint bit;
	unsigned long flag;

	spin_lock_irqsave(&lock, flag);
	for (i = 0; i < 8; i++) {
		bit = 0x00;
		bit = 0x01&(byte >> i);
		bq2022a_write_bit(bit);
	}
	spin_unlock_irqrestore(&lock, flag);
	udelay(400);
}

static int bq2022a_read_bit(void)
{
	uint bit;

	udelay(40);
	gpio_direction_output(gpio_id, 0);
	udelay(4);
	gpio_direction_input(gpio_id);
	udelay(2);
	gpio_direction_input(gpio_id);
	bit = gpio_get_value(gpio_id);
	udelay(6);

	return bit;
}

static unsigned char bq2022a_read_byte(void)
{
	unsigned char count = 0, bit = 0, byte_tmep = 0;
	unsigned long flag;

	spin_lock_irqsave(&lock, flag);
	for (count = 0; count < 8; count++) {
		bit = bq2022a_read_bit();

		byte_tmep = byte_tmep|(bit<<count);
	}
	spin_unlock_irqrestore(&lock, flag);
	udelay(500);
	return (unsigned char)byte_tmep;

}

static int bq2022a_check_vendor_id(void)
{
	int error_num = 0;
	int i;

	for (i = 0; i < 64; i++) {
		if (battery_read_data[i] != cos_battery_data[i])
			error_num++;
		}
	pr_err("get_battery_vendor_cos_error=%d\n", error_num);
	if (error_num < 15) {
		error_num = 1;
		return 1;
	}

	error_num = 0;
	for (i = 0; i < 64; i++) {
		if (battery_read_data[i] != acc_battery_data[i])
			error_num++;
	}
	pr_err("get_battery_vendor_acc=%d\n", error_num);
	if (error_num < 15)
		return 2;

	error_num = 0;
	for (i = 0; i < 64; i++) {
		if (battery_read_data[i] != des_battery_data[i])
		error_num++;
	}
	pr_err("get_battery_vendor_des=%d\n", error_num);
	if (error_num < 15)
		return 3;

	error_num = 0;

	return 0;
}

static void bq2022a_read_vendor(void)
{
	int crc;
	int count;

	bq2022a_init();
	bq2022a_write_byte(0xcc);
	bq2022a_write_byte(0xc3);
	bq2022a_write_byte(0x00);
	bq2022a_write_byte(0x00);
	crc = bq2022a_read_byte();
	for (count = 0; count < 32; count++)
		battery_read_data[count] = bq2022a_read_byte();

	crc = bq2022a_read_byte();
	for (count = 32; count < 64; count++)
		battery_read_data[count] = bq2022a_read_byte();

	crc = bq2022a_read_byte();

}

static int bq2022a_probe(struct platform_device *pdev)
{
	int ret = 0;
	int id;
	int i;

	struct device_node *node = pdev->dev.of_node;


	gpio_id = of_get_named_gpio(node, "bq2022a,id", 0);
	if (gpio_id < 0) {
		pr_err("failed get gpio_id\n");
		return -EINVAL;
	}
	id = gpio_id;

	ret = gpio_request(id, "bq2022a");
	if (ret < 0)
		pr_err("bq2022a: request gpio failed\n");

	spin_lock_init(&lock);

	bq2022a_read_vendor();

	battery_is_matching = bq2022a_check_vendor_id();

	if (!battery_is_matching) {
		for (i = 0; i < 2; i++) {
			bq2022a_read_vendor();
			battery_is_matching = bq2022a_check_vendor_id();
			if (battery_is_matching)
				break;
		}
	}
	pr_info("battery_is_matching = %d\n", battery_is_matching);
	gpio_direction_input(id);
	return ret;

}

static int bq2022a_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id bq2022a_of_match[] = {
	{ .compatible = "ti,bq2022a" },
	{ },
};
MODULE_DEVICE_TABLE(of, bq2022a_of_match);

static struct platform_driver bq2022a_driver = {
	.probe		= bq2022a_probe,
	.remove 	= bq2022a_remove,
	.driver = {
		.of_match_table = of_match_ptr(bq2022a_of_match),
		.name		= "ti,bq2022a",
		.owner		= THIS_MODULE,
	}
};


static int __init sms_power_ctrl_init(void)
{
	return platform_driver_register(&bq2022a_driver);
}

static void __exit sms_power_ctr_exit(void)
{
	platform_driver_unregister(&bq2022a_driver);
}

module_init(sms_power_ctrl_init);
module_exit(sms_power_ctr_exit);

MODULE_AUTHOR("longcheer");
MODULE_DESCRIPTION("bq2022a driver");

