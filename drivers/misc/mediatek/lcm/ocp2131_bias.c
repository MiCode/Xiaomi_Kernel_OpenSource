#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#define OCP2131_Positive_Output	0X00
#define OCP2131_Negative_Output	0X01
#define OCP2131_Set_voltage	0x0F
#define OCP2131_Mtp_Reg		0xFF
#define OCP2131_Mtp_save	0x80

struct ocp2131 {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
};

/*
 * static int __ocp2131_read_reg(struct ocp2131 *ocp, u8 reg, u8 *data)
 * {
 * 	s32 ret;
 *
 * 	ret = i2c_smbus_read_byte_data(ocp->client, reg);
 * 	if (ret < 0) {
 * 		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
 * 		pm_relax(ocp->dev);
 * 		return ret;
 * 	}
 *
 * 	*data = (u8)ret;
 *
 * 	return 0;
 * 	}*/

static int __ocp2131_write_reg(struct ocp2131 *ocp, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(ocp->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		pm_relax(ocp->dev);
		return ret;
	}


	return 0;
}

static int ocp2131_write_reg(struct ocp2131 *ocp, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&ocp->i2c_rw_lock);
	ret = __ocp2131_write_reg(ocp, reg, data);
	mutex_unlock(&ocp->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

/*
 * static int ocp2131_read_reg(struct ocp2131 *ocp, u8 reg, u8 *data)
 * {
 * 	int ret;
 *
 * 	mutex_lock(&ocp->i2c_rw_lock);
 * 	ret = __ocp2131_read_reg(ocp, reg, data);
 * 	mutex_unlock(&ocp->i2c_rw_lock);
 *
 * 	return ret;
 *
 * }*/

static int ocp2131_bias_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ocp2131 *ocp;
	printk("Enter ocp2131_bias_probe\n");
	ocp = devm_kzalloc(&client->dev, sizeof(struct ocp2131), GFP_KERNEL);
	if (!ocp) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}
	ocp->dev = &client->dev;
	ocp->client = client;
	i2c_set_clientdata(client, ocp);

	mutex_init(&ocp->i2c_rw_lock);

	ocp2131_write_reg(ocp, OCP2131_Positive_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Negative_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Mtp_Reg, OCP2131_Mtp_save);
	return 0;
}

static int ocp2131_bias_remove(struct i2c_client *client)
{
	struct ocp2131 *ocp = i2c_get_clientdata(client);
	mutex_destroy(&ocp->i2c_rw_lock);

	printk("Enter ocp2131_bias_remove\n");

	return 0;
}
/*****************************************************************************
 * * i2c driver configuration
 * *****************************************************************************/
static const struct i2c_device_id ocp2131_bias_id[] = {
	{"ocp2131_bias", 0},
	{}
};
static const struct of_device_id ocp2131_bias_match_table[] = {
	{.compatible = "mediatek,lcd_bias"},
	{},
};

MODULE_DEVICE_TABLE(of, ocp2131_bias_match_table);
MODULE_DEVICE_TABLE(i2c, ocp2131_bias_id);

static struct i2c_driver ocp2131_bias_driver = {
	.driver = {
		.name = "ocp2131_bias",
		.owner 	= THIS_MODULE,
		.of_match_table = ocp2131_bias_match_table,
	},
	.id_table = ocp2131_bias_id,
	.probe = ocp2131_bias_probe,
	.remove = ocp2131_bias_remove,
};

module_i2c_driver(ocp2131_bias_driver);
