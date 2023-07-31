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
#include <linux/delay.h>
#include <linux/ocp2131_bias.h>

#define OCP2131_Positive_Output	0X00
#define OCP2131_Negative_Output	0X01
#define OCP2131_Set_voltage	0x0F
#define OCP2131_Set_vsp_vsn_enable	0x03
#define OCP2131_Set_value	0x03
#define OCP2131_Mtp_Reg		0xFF
#define OCP2131_Mtp_save	0x80
struct i2c_client *gclient;

struct ocp2131 {
	struct device *dev;
	struct i2c_client *client;
	struct mutex i2c_rw_lock;
	int enp_gpio;
	int enn_gpio;
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


static void ocp2131_parse_dt(struct device *dev, struct ocp2131 *ocp)
{
	struct device_node *np = dev->of_node;
	int ret;

	ocp->enp_gpio = of_get_named_gpio(np, "lcm-enp-gpio", 0);
	pr_info("enp_gpio: %d\n", ocp->enp_gpio);

	ocp->enn_gpio = of_get_named_gpio(np, "lcm-enn-gpio", 0);
	pr_info("enn_gpio: %d\n", ocp->enn_gpio);

	if (gpio_is_valid(ocp->enp_gpio)) {
		ret = gpio_request(ocp->enp_gpio, "lcm-enp-gpio");
		if (ret < 0) {
			pr_err("failed to request lcm-enp-gpio\n");
		}
		pr_info("enp_gpio is valid!\n");
	}
	if (gpio_is_valid(ocp->enn_gpio)) {
		ret = gpio_request(ocp->enn_gpio, "lcm-enn-gpio");
		if (ret < 0) {
			pr_err("failed to request lcm-enn-gpio\n");
		}
		pr_info("enn_gpio is valid!\n");
	}
}

void ocp2131_enable(void)
{
	struct ocp2131 *ocp = i2c_get_clientdata(gclient);

	pr_info("%s: entry\n", __func__);

	gpio_set_value(ocp->enp_gpio, 1);
	udelay(5000);
	gpio_set_value(ocp->enn_gpio, 1);

	ocp2131_write_reg(ocp, OCP2131_Positive_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Negative_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Set_vsp_vsn_enable, OCP2131_Set_value);
}
EXPORT_SYMBOL_GPL(ocp2131_enable);

void ocp2131_disable(void)
{
	struct ocp2131 *ocp = i2c_get_clientdata(gclient);

	pr_info("%s: entry\n", __func__);

	gpio_set_value(ocp->enn_gpio, 0);
	udelay(1500);
	gpio_set_value(ocp->enp_gpio, 0);
}
EXPORT_SYMBOL_GPL(ocp2131_disable);

static int ocp2131_bias_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ocp2131 *ocp;
	pr_info("Enter ocp2131_bias_probe\n");
	ocp = devm_kzalloc(&client->dev, sizeof(struct ocp2131), GFP_KERNEL);
	if (!ocp) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}
	ocp->dev = &client->dev;
	ocp->client = client;
	gclient = client;
	i2c_set_clientdata(client, ocp);
	ocp2131_parse_dt(&client->dev, ocp);
	mutex_init(&ocp->i2c_rw_lock);
	ocp2131_write_reg(ocp, OCP2131_Positive_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Negative_Output, OCP2131_Set_voltage);
	ocp2131_write_reg(ocp, OCP2131_Set_vsp_vsn_enable, OCP2131_Set_value);
	//ocp2131_write_reg(ocp, OCP2131_Mtp_Reg, OCP2131_Mtp_save);
	return 0;
}
static int ocp2131_bias_remove(struct i2c_client *client)
{
	struct ocp2131 *ocp = i2c_get_clientdata(client);
	mutex_destroy(&ocp->i2c_rw_lock);
	pr_info("Enter ocp2131_bias_remove\n");
	return 0;
}
/*****************************************************************************
 * * i2c driver configuration
 * *****************************************************************************/
static const struct i2c_device_id ocp2131_bias_id[] = {
	{"ocp2131_bias", 0},
	{},
};
static const struct of_device_id ocp2131_bias_match_table[] = {
	{.compatible = "qcom,ocp2131_bias"},
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

static int __init ocp2131_init(void)
{
	int ret = 0;

	pr_info("%s Entry\n", __func__);

	ret = i2c_add_driver(&ocp2131_bias_driver);
	if (ret != 0)
		pr_err("OCP2131 driver init failed!");

	pr_info("%s Complete\n", __func__);

	return ret;
}

static void __exit ocp2131_exit(void) 
{
	i2c_del_driver(&ocp2131_bias_driver);
}

module_init(ocp2131_init);
module_exit(ocp2131_exit);

MODULE_AUTHOR("OCP");
MODULE_DESCRIPTION("OCP2131 Bias IC Driver");
MODULE_LICENSE("GPL v2");
