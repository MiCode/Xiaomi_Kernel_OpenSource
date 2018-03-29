/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
*This program is free software; you can redistribute it and/or
*modify it under the terms of the GNU General Public License as
*published by the Free Software Foundation version 2.
*This program is distributed AS-IS WITHOUT ANY WARRANTY of any
*kind, whether express or implied; INCLUDING without the implied warranty
*of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
*See the GNU General Public License for more details at
*http://www.gnu.org/licenses/gpl-2.0.html.
*/
#include "Wrap.h"
#include "../Common/si_time.h"
#include "../Common/si_usbpd_core.h"
#include "../Common/si_usbpd_main.h"
#include "../Common/si_usbpd_regs.h"

#include <linux/of_irq.h>
#include <typec.h>
#include <linux/platform_device.h>

#define DRP 0
#define DFP 1
#define UFP 2
#define INT_INDEX 0

dev_t dev_num;
struct class *usbpd_class;
static struct i2c_client *i2c_dev_client;

struct gpio sii70xx_gpio[NUM_GPIO] = {
	{GPIO_USBPD_INT, GPIOF_IN, "USBPD_intr"},
	{GPIO_VBUS_SRC, GPIOF_OUT_INIT_LOW, "SRC_En"},
	{GPIO_VBUS_SNK, GPIOF_OUT_INIT_LOW, "SNK_VBUS_En"},
	{GPIO_RESET_CTRL, GPIOF_OUT_INIT_HIGH, "RESET_CTRL_En"}
};

#ifndef CONFIG_USB_C_SWITCH_SII70XX_MHL_MODE
int drp_mode = DRP;
#else
int drp_mode = DFP;
#endif
module_param(drp_mode, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(drp_mode,
		 "An integer parameter to switch	PD mode between DRP(0) DFP(1) and UFP(2)");


void initialisethreadIds(struct sii_usbp_policy_engine *pdev)
{
}

#if 0
static int si_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int value;

	value = of_get_named_gpio_flags(np, "simg-70xx,irq-gpio", 0, NULL);
	if (value >= 0)
		sii70xx_gpio[INT_INDEX].gpio = value;
	else
		return -ENODEV;

	value = of_get_named_gpio_flags(np, "simg-70xx,vbus-src", 0, NULL);
	if (value >= 0)
		sii70xx_gpio[VBUS_SRC].gpio = value;
	else
		return -ENODEV;

	value = of_get_named_gpio_flags(np, "simg-70xx,vbus-snk", 0, NULL);
	if (value >= 0)
		sii70xx_gpio[VBUS_SNK].gpio = value;
	else
		return -ENODEV;
	value = of_get_named_gpio_flags(np, "simg-70xx,reset-enable", 0, NULL);
	if (value >= 0)
		sii70xx_gpio[RESET_CTRL].gpio = value;
	else
		return -ENODEV;

	pr_info("Interrupt GPIO = %d\n", sii70xx_gpio[INT_INDEX].gpio);
	pr_info("VBUS SRC GPIO = %d\n", sii70xx_gpio[VBUS_SRC].gpio);
	pr_info("VBUS SNK GPIO = %d\n", sii70xx_gpio[VBUS_SNK].gpio);
	pr_info("Reset GPIO = %d\n", sii70xx_gpio[RESET_CTRL].gpio);
	return 0;
}
#endif

static int sii_i2c_remove(struct i2c_client *client)
{
	pr_info("GPIO free Array\n");
#if 0
	gpio_free_array(sii70xx_gpio, ARRAY_SIZE(sii70xx_gpio));
#endif
	return 0;
}

/* /////////////////////////////////////////////////////////////////////////////// */
struct usbtypc *g_exttypec;
struct sii70xx_drv_context *g_drv_context;
int register_typec_switch_callback(struct typec_switch_data *new_driver)
{
	pr_err("Register driver %s %d\n", new_driver->name, new_driver->type);

	if (new_driver->type == DEVICE_TYPE) {
		g_exttypec->device_driver = new_driver;
		g_exttypec->device_driver->on = 0;
		return 0;
	}

	if (new_driver->type == HOST_TYPE) {
		g_exttypec->host_driver = new_driver;
		g_exttypec->host_driver->on = 0;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL_GPL(register_typec_switch_callback);

int unregister_typec_switch_callback(struct typec_switch_data *new_driver)
{
	if ((new_driver->type == DEVICE_TYPE) && (g_exttypec->device_driver == new_driver))
		g_exttypec->device_driver = NULL;

	if ((new_driver->type == HOST_TYPE) && (g_exttypec->host_driver == new_driver))
		g_exttypec->host_driver = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_typec_switch_callback);

void sii70xx_gpio_init(void)
{
	struct device_node *node;
	u32 ints[2] = { 0, 0 };
	unsigned int gpiopin, debounce;

	pinctrl_select_state(g_exttypec->pinctrl, g_exttypec->pin_cfg->sii7033_rst_init);

	node = of_find_compatible_node(NULL, NULL, "mediatek,fusb300-eint");
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		debounce = ints[1];
		gpiopin = ints[0];

		gpio_set_debounce(gpiopin, debounce);

	}

	g_exttypec->irqnum = irq_of_parse_and_map(node, 0);
	g_exttypec->en_irq = 1;
	pr_err("sii70xx_gpio_init : %d\n", g_exttypec->irqnum);
}

int usb_redriver_init(struct usbtypc *typec)
{
	int retval = 0;
	int u3_eq_c1 = 197;
	int u3_eq_c2 = 196;

	typec->u_rd = kzalloc(sizeof(struct usb_redriver), GFP_KERNEL);
	typec->u_rd->c1_gpio = u3_eq_c1;
	typec->u_rd->eq_c1 = U3_EQ_HIGH;

	typec->u_rd->c2_gpio = u3_eq_c2;
	typec->u_rd->eq_c2 = U3_EQ_HIGH;

	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->re_c1_init);
	pinctrl_select_state(typec->pinctrl, typec->pin_cfg->re_c2_init);

	pr_info("c1_gpio=0x%X, out=%d\n", typec->u_rd->c1_gpio,
		gpio_get_value(typec->u_rd->c1_gpio));

	pr_info("c2_gpio=0x%X, out=%d\n", typec->u_rd->c2_gpio,
		gpio_get_value(typec->u_rd->c2_gpio));

	return retval;
}

/*
 * ctrl_pin=1=C1 control pin
 * ctrl_pin=2=C2 control pin
 * (stat=0) = State=L
 * (stat=1) = State=High-Z
 * (stat=2) = State=H
 */
int usb_redriver_config(struct usbtypc *typec, int ctrl_pin, int stat)
{
	int retval = 0;
	int pin_num = 0;

	pr_info("%s pin=%d, stat=%d\n", __func__, ctrl_pin, stat);

	if (ctrl_pin == U3_EQ_C1) {
		pin_num = typec->u_rd->c1_gpio;
	} else if (ctrl_pin == U3_EQ_C2) {
		pin_num = typec->u_rd->c2_gpio;
	} else {
		retval = -EINVAL;
		goto end;
	}

	/* switch(stat) { */
	/* case U3_EQ_LOW: */
	/* retval |= mt_set_gpio_dir( pin_num, GPIO_DIR_OUT); */
	/* retval |= mt_set_gpio_out( pin_num, GPIO_OUT_ZERO); */
	/* retval |= mt_set_gpio_pull_enable( pin_num, GPIO_PULL_ENABLE); */
	/* break; */
	/* case U3_EQ_HZ: */
	/* retval |= mt_set_gpio_dir( pin_num, GPIO_DIR_IN); */
	/* retval |= mt_set_gpio_pull_enable( pin_num, GPIO_PULL_DISABLE); */
	/* break; */
	/* case U3_EQ_HIGH: */
	/* retval |= mt_set_gpio_dir( pin_num, GPIO_DIR_OUT); */
	/* retval |= mt_set_gpio_out( pin_num, GPIO_OUT_ONE); */
	/* retval |= mt_set_gpio_pull_enable( pin_num, GPIO_PULL_ENABLE); */
	/* break; */
	/* default: */
	/* retval = -EINVAL; */
	/* break; */
	/* } */

	pr_info("%s gpio=%d, out=%d\n", __func__, pin_num, gpio_get_value(pin_num));
end:

	return retval;
}

int usb_redriver_enter_dps(struct usbtypc *typec)
{
	int retval = 0;

	retval |= usb_redriver_config(typec, U3_EQ_C1, U3_EQ_LOW);
	retval |= usb_redriver_config(typec, U3_EQ_C2, U3_EQ_LOW);
	return retval;
}

int usb_redriver_exit_dps(struct usbtypc *typec)
{
	int retval = 0;

	if ((typec->u_rd->eq_c1 == U3_EQ_HIGH) || (typec->u_rd->eq_c2 == U3_EQ_HIGH)) {
		retval |= usb_redriver_config(typec, U3_EQ_C1, typec->u_rd->eq_c1);
		retval |= usb_redriver_config(typec, U3_EQ_C2, typec->u_rd->eq_c2);
	} else {
		retval |= usb_redriver_config(typec, U3_EQ_C1, U3_EQ_HIGH);
		retval |= usb_redriver_config(typec, U3_EQ_C2, U3_EQ_HIGH);

		udelay(1);

		retval |= usb_redriver_config(typec, U3_EQ_C1, typec->u_rd->eq_c1);
		retval |= usb_redriver_config(typec, U3_EQ_C2, typec->u_rd->eq_c2);
	}
	return retval;
}

static DEFINE_MUTEX(typec_lock);
void sii7033_eint_work(struct work_struct *data)
{
	mutex_lock(&typec_lock);

	usbpd_irq_handler(g_exttypec->irqnum, g_drv_context);

	if (!g_exttypec->en_irq) {
		g_exttypec->en_irq = 1;
		enable_irq(g_exttypec->irqnum);
	}

	mutex_unlock(&typec_lock);
}

irqreturn_t sii7033_eint_isr(int irqnum, void *data)
{
	if (g_exttypec->en_irq) {
		disable_irq_nosync(irqnum);
		g_exttypec->en_irq = 0;
	}

	schedule_delayed_work_on(WORK_CPU_UNBOUND, &g_exttypec->eint_work, 0);
	return IRQ_HANDLED;
}

/* /////////////////////////////////////////////////////////////////////////////// */
static int usbc_pinctrl_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct usbtypc *typec;

	if (!g_exttypec)
		g_exttypec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);

	typec = g_exttypec;
	typec->pinctrl = devm_pinctrl_get(&pdev->dev);

	if (IS_ERR(typec->pinctrl)) {
		pr_info("Cannot find usb pinctrl!\n");
	} else {
		typec->pin_cfg = kzalloc(sizeof(struct usbc_pin_ctrl), GFP_KERNEL);

		pr_info("pinctrl=%p\n", typec->pinctrl);

		/********************************************************/
		typec->pin_cfg->re_c1_init =
		    pinctrl_lookup_state(typec->pinctrl, "redriver_c1_init");
		if (IS_ERR(typec->pin_cfg->re_c1_init))
			pr_info("Can *NOT* find redriver_c1_init\n");
		else
			pr_info("Find redriver_c1_init\n");

		/********************************************************/
		typec->pin_cfg->re_c2_init =
		    pinctrl_lookup_state(typec->pinctrl, "redriver_c2_init");
		if (IS_ERR(typec->pin_cfg->re_c2_init))
			pr_info("Can *NOT* find redriver_c2_init\n");
		else
			pr_info("Find redriver_c2_init\n");
		/********************************************************/
		typec->pin_cfg->sii7033_rst_init =
			pinctrl_lookup_state(typec->pinctrl, "sii7033_rst_init");
		if (IS_ERR(typec->pin_cfg->sii7033_rst_init))
			pr_info("Can *NOT* find sii7033_rst_init\n");
		else
			pr_info("Find sii7033_rst_init\n");

		/********************************************************/
		typec->pin_cfg->sii7033_rst_low =
		    pinctrl_lookup_state(typec->pinctrl, "sii7033_rst_low");
		if (IS_ERR(typec->pin_cfg->sii7033_rst_low))
			pr_info("Can *NOT* find sii7033_rst_low\n");
		else
			pr_info("Find sii7033_rst_low\n");

		/********************************************************/
		typec->pin_cfg->sii7033_rst_high =
		    pinctrl_lookup_state(typec->pinctrl, "sii7033_rst_high");
		if (IS_ERR(typec->pin_cfg->sii7033_rst_high))
			pr_info("Can *NOT* find sii7033_rst_high\n");
		else
			pr_info("Find sii7033_rst_high\n");

		/********************************************************/
		pr_info("Finish parsing pinctrl\n");
	}
	return retval;
}

static int usbc_pinctrl_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id usb_pinctrl_ids[] = {
	{.compatible = "mediatek,usb_c_pinctrl",},
	{},
};

static struct platform_driver usbc_pinctrl_driver = {
	.probe = usbc_pinctrl_probe,
	.remove = usbc_pinctrl_remove,
	.driver = {
		   .name = "usbc_pinctrl",
#ifdef CONFIG_OF
		   .of_match_table = usb_pinctrl_ids,
#endif
		   },
};

static int sii_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	dev_dbg(&client->dev, "Enter\n");

	pr_info("SII70XX Driver v8.7\n");

#if 0
	if (client->dev.of_node) {
		ret = si_parse_dt(&client->dev);
		if (ret)
			return -ENODEV;
	} else
		return -ENODEV;

	ret = gpio_request_array(sii70xx_gpio, ARRAY_SIZE(sii70xx_gpio));

	if (ret < 0) {
		dev_err(&client->dev, "gpio_request_array failed");
		return -EINVAL;
	}
#endif

	i2c_dev_client = client;
	usbpd_pf_i2c_init(client->adapter);

	if (!g_exttypec)
		g_exttypec = kzalloc(sizeof(struct usbtypc), GFP_KERNEL);
	sii70xx_gpio_init();
	usb_redriver_init(g_exttypec);
	g_exttypec->en_irq = 1;
	mutex_init(&typec_lock);
	INIT_DELAYED_WORK(&g_exttypec->eint_work, sii7033_eint_work);

	pr_info("drp_mode 0x%x\n", drp_mode);
	/*pdev->pd_mode = drp_mode; */
	sii70xx_device_init(&client->dev, (struct gpio *)sii70xx_gpio);

	return ret;
}

#if 0
static struct i2c_device_id sii_i2c_id[] = {
	{SII_DRIVER_NAME, 0x68},
	{}
};

static const struct of_device_id sii_match_table[] = {
	{.compatible = COMPATIBLE_NAME},
	{}
};
#endif

static const struct i2c_device_id sii_i2c_id[] = {
	{SII_DRIVER_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static const struct of_device_id sii70_of_match[] = {
	{.compatible = "mediatek,usb_type_c"},
	{},
};
#endif

static struct i2c_driver sii_i2c_driver = {
	.driver = {
		   .name = SII_DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = sii70_of_match,
#endif
		   },
	.probe = sii_i2c_probe,
	.remove = sii_i2c_remove,
	.id_table = sii_i2c_id,
};

static int __init i2c_init(void)
{
	int ret = -ENODEV;

	ret = i2c_add_driver(&sii_i2c_driver);
	if (ret < 0) {
		if (ret == 0)
			i2c_del_driver(&sii_i2c_driver);
		pr_debug("failed !\n\nCHECK POWER AND CONNECTION ");
		pr_debug("TO Sii70xx Starter Kit.\n\n");
		goto err_exit;
	}

	if (!platform_driver_register(&usbc_pinctrl_driver))
		pr_debug("register usbc pinctrl succeed!!\n");
	else {
		pr_debug("register usbc pinctrl fail!!\n");
		goto err_exit;
	}

	goto done;

err_exit:
done:
	pr_debug("returning %d\n", ret);
	return ret;
}

static int __init si_drv_70xx_init(void)
{
	int ret;

	ret = i2c_init();

	return ret;
}

static void __exit si_drv_70xx_exit(void)
{
	pr_info("si_70xx_exit called\n");

	usbpd_device_exit(&i2c_dev_client->dev);
	pr_info("client removed\n");
	i2c_del_driver(&sii_i2c_driver);
	pr_info("driver unloaded\n");
}
fs_initcall(si_drv_70xx_init);

#if 0
module_init(si_drv_70xx_init);
module_exit(si_drv_70xx_exit);

MODULE_DESCRIPTION("Silicon Image SiI70xx switch driver");
MODULE_AUTHOR("Silicon Image <http://www.siliconimage.com>");
MODULE_LICENSE("GPL");
#endif
