/**
 * @file   rx1619_1.c
 * @author  <colin>
 * @date   2019-04-09
 *
 * @brief
 *
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
//#include <soc/qcom/socinfo.h>

#define rx1619_1_DRIVER_NAME      "rx1619_trim"

static struct rx1619_1_chg *g_chip;

struct rx1619_1_chg {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap       *regmap;
};

/*
static int rx1619_1_read(struct rx1619_1_chg *chip, u8 *val, u16 addr)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc >= 0) {
		*val = (u8)temp;
		//dev_err(chip->dev, "[rx1619_1] [%s] [0x%04x] = [0x%x] \n", __func__, addr, *val);
	}

	return rc;
}*/


static int rx1619_1_write(struct rx1619_1_chg *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);

	return rc;
}



void rx1619_1_trim(void)
{
	printk("--------------rx1619_1_trim++\n");

	/************prepare_for_mtp_write************/
	rx1619_1_write(g_chip, 0x69, 0x2017);
	rx1619_1_write(g_chip, 0x96, 0x2017);
	rx1619_1_write(g_chip, 0x66, 0x2017);
	rx1619_1_write(g_chip, 0x99, 0x2017);
	rx1619_1_write(g_chip, 0xff, 0x2018);
	rx1619_1_write(g_chip, 0xff, 0x2019);
	rx1619_1_write(g_chip, 0x5a, 0x0001);
	rx1619_1_write(g_chip, 0xa5, 0x0003);
	/************prepare_for_mtp_write************/

	printk("--------------rx1619_1_trim--\n");
}

// first step: define regmap_config
static const struct regmap_config rx1619_1_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int rx1619_1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rx1619_1_chg *chip;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	chip->regmap = regmap_init_i2c(client, &rx1619_1_regmap_config);
	if (!chip->regmap) {
		dev_err(&client->dev, "parent regmap is missing\n");
		return -EINVAL;
	}

	chip->client = client;
	chip->dev = &client->dev;

	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);

	//rx1619_1_dump_reg();
	g_chip = chip;

	dev_err(chip->dev, "[rx1619_1] [%s] success! \n", __func__);

	return 0;
}

static int rx1619_1_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id rx1619_1_id[] = {
	{rx1619_1_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rx1619_1_id);

static struct of_device_id  rx1619_1_match_table[] = {
	{ .compatible = "nuvolta,wl_rx1619_trim",},
	{}
};

static struct i2c_driver rx1619_1_driver = {
	.driver = {
		.name = rx1619_1_DRIVER_NAME,
		.of_match_table = rx1619_1_match_table,
	},
	.probe = rx1619_1_probe,
	.remove = rx1619_1_remove,
	.id_table = rx1619_1_id,
};

static int __init rx1619_1_init(void)
{
	int ret;
#ifdef CONFIG_RX1619_REMOVE
	return 0;
#endif
	ret = i2c_add_driver(&rx1619_1_driver);
	if (ret)
		printk(KERN_ERR "rx1619_1 i2c driver init failed!\n");

	return ret;
}

static void __exit rx1619_1_exit(void)
{
	i2c_del_driver(&rx1619_1_driver);
}

module_init(rx1619_1_init);
module_exit(rx1619_1_exit);

MODULE_AUTHOR("colin");
MODULE_DESCRIPTION("NUVOLTA Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL/BSD");
