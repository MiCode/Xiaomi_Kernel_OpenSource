/*
 * SIMG SiI6031 MHL-USB Switch driver
 * 
 * Copyright 2014 Silicon Image, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mod_devicetable.h>
#include <linux/si_6031_switch.h>

struct gpio stark_gpio_ctrl[3] = {
	{0, GPIOF_OUT_INIT_HIGH, "MHL_USB_0"},
	{0, GPIOF_OUT_INIT_HIGH, "MHL_USB_1"},
	{0, GPIOF_OUT_INIT_HIGH, "MHL_VBUS"}
};

void sii_switch_to_mhl(bool switch_to_mhl)
{
	if(switch_to_mhl)
	{
		printk("%s(): SIMG: switch to MHL gpio [%d, %d] \n", __func__,
			 stark_gpio_ctrl[MHL_USB_0].gpio,
			 stark_gpio_ctrl[MHL_USB_1].gpio);
		gpio_set_value(stark_gpio_ctrl[MHL_USB_0].gpio, 1);
		gpio_set_value(stark_gpio_ctrl[MHL_USB_1].gpio, 1);
	}
	else {
		printk("%s(): SIMG: switch to USB gpio [%d, %d] \n", __func__,
			 stark_gpio_ctrl[MHL_USB_0].gpio,
			 stark_gpio_ctrl[MHL_USB_1].gpio);
		gpio_set_value(stark_gpio_ctrl[MHL_USB_0].gpio, 0);
	 	gpio_set_value(stark_gpio_ctrl[MHL_USB_1].gpio, 0);
	}
	/*gpio_set_value(stark_gpio_ctrl[MHL_USB_VBUS].gpio, 1);*/
	printk("%s(): exit\n", __func__);
}
EXPORT_SYMBOL(sii_switch_to_mhl);

int sii6031_gpio_init(void)
{
	int ret;
	
	printk("%s(): called\n", __func__);
	ret = gpio_request_array(stark_gpio_ctrl, ARRAY_SIZE(stark_gpio_ctrl));
	if (ret < 0) 
		printk("%s(): gpio_request_array failed, error code %d\n",__func__, ret);
	
	return ret;
}

static int sii6031_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int value;

	value = of_get_named_gpio_flags(np, "simg,gpio_sel0", 0, NULL);
	if (value >= 0)
		stark_gpio_ctrl[MHL_USB_0].gpio = value;

	value = of_get_named_gpio_flags(np, "simg,gpio_sel1", 0, NULL);
	if (value >= 0)
		stark_gpio_ctrl[MHL_USB_1].gpio = value;

	value = of_get_named_gpio_flags(np, "simg,gpio_vbus", 0, NULL);
	if (value >= 0)
		stark_gpio_ctrl[MHL_USB_VBUS].gpio = value;


	return 0;

}

static int __devinit sii6031_probe(struct platform_device *pdev)
{
	int ret =0;


	if(pdev->dev.of_node)
		ret = sii6031_parse_dt(&pdev->dev);


	if(ret)
		return -1;
	
	
	ret = sii6031_gpio_init();

	if(ret)
		return -1;
	

	return 0;

}
static int __devexit sii6031_remove(struct platform_device *pdev)
{
	gpio_free_array(stark_gpio_ctrl,ARRAY_SIZE(stark_gpio_ctrl));

	return 0;
}

static const struct of_device_id sii6031_gpio_match[] = {
	{ .compatible = "simg,sii-6031", },
	{ },
};
MODULE_DEVICE_TABLE(of, sii6031_gpio_match);

static struct platform_driver sii6031_driver = {
	.probe          = sii6031_probe,
	.remove         = __devexit_p(sii6031_remove),
	.driver         = {
		.name   = "simg,sii-6031",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(sii6031_gpio_match),
	},
};

static int __init sii6031_init(void)
{
	return platform_driver_register(&sii6031_driver);
}

static void __exit sii6031_exit(void)
{
	platform_driver_unregister(&sii6031_driver);
}
module_init(sii6031_init);
module_exit(sii6031_exit);

MODULE_AUTHOR("Praveen Kumar Vuppala<praveen.vuppala@siliconimage.com>");
MODULE_DESCRIPTION("SiI6031 Switch driver");
MODULE_LICENSE("GPL");
