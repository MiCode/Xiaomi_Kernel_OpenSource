/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "ocp96011-i2c.h"
#include "../tcpc/inc/tcpm.h"

#define OCP96011_I2C_NAME	"ocp96011"

#define OCP96011_SWITCH_SETTINGS 0x04
#define OCP96011_SWITCH_CONTROL  0x05
#define OCP96011_SWITCH_STATUS1  0x07
#define OCP96011_SLOW_L          0x08
#define OCP96011_SLOW_R          0x09
#define OCP96011_SLOW_MIC        0x0A
#define OCP96011_SLOW_SENSE      0x0B
#define OCP96011_SLOW_GND        0x0C
#define OCP96011_DELAY_L_R       0x0D
#define OCP96011_DELAY_L_MIC     0x0E
#define OCP96011_DELAY_L_SENSE   0x0F
#define OCP96011_DELAY_L_AGND    0x10
#define OCP96011_RESET           0x1E

struct ocp96011_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head ocp96011_notifier;
	struct mutex notification_lock;
};

struct ocp96011_priv *fsa_priv;

struct ocp96011_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config ocp96011_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = OCP96011_RESET,
};

static const struct ocp96011_reg_val fsa_reg_i2c_defaults[] = {
	{OCP96011_SLOW_L, 0x00},
	{OCP96011_SLOW_R, 0x00},
	{OCP96011_SLOW_MIC, 0x00},
	{OCP96011_SLOW_SENSE, 0x00},
	{OCP96011_SLOW_GND, 0x00},
	{OCP96011_DELAY_L_R, 0x00},
	{OCP96011_DELAY_L_MIC, 0x00},
	{OCP96011_DELAY_L_SENSE, 0x00},
	{OCP96011_DELAY_L_AGND, 0x09},
	{OCP96011_SWITCH_SETTINGS, 0x98},
};

u32 ocp96011_get_headset_status(void)
{
	u32 reg17 = 0;
	usleep_range(100*1000, 150*1000);
	regmap_read(fsa_priv->regmap, 0x17, &reg17);
	dev_info(fsa_priv->dev, "%s: reg17 = %d\n", __func__, reg17);
	return reg17;
}

static void ocp96011_usbc_update_settings(struct ocp96011_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
printk(" %s enter switch_control=0x%x switch_enable=0x%x\n", __func__, switch_control, switch_enable);
	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_SETTINGS, 0x80);
	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_CONTROL, switch_control);
	/* OCP96011 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_SETTINGS, switch_enable);
printk(" %s end\n", __func__);

}
#if 1
static int ocp96011_usbc_event_changed(struct notifier_block *nb,unsigned long evt, void *ptr)
{
	int ret;
	union power_supply_propval mode;
	struct device *dev;
printk(" %s 0**evt =%d******************\n",__func__ ,evt);
	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if ((struct power_supply *)ptr != fsa_priv->usb_psy ||
				evt != PSY_EVENT_PROP_CHANGED)
	{
		printk(" %s 2.5 \n", __func__);
		return 0;
	}

	ret = power_supply_get_property(fsa_priv->usb_psy,
			TCP_NOTIFY_TYPEC_STATE, &mode);
	if (ret) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, ret);
		return ret;
	}

	printk( "%s:4 USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, fsa_priv->usbc_mode.counter,
		TYPEC_ATTACHED_AUDIO);

	switch (mode.intval) {
	//case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
	case TYPEC_UNATTACHED:
		if (atomic_read(&(fsa_priv->usbc_mode)) == mode.intval)
			break; /* filter notifications received before */
		atomic_set(&(fsa_priv->usbc_mode), mode.intval);

		dev_dbg(dev, "%s: queueing usbc_analog_work\n",
			__func__);
		pm_stay_awake(fsa_priv->dev);
		schedule_work(&fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}

	return ret;
}
#endif
#if 0
static int ocp96011_usbc_analog_setup_switches(struct ocp96011_priv *fsa_priv)
{
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;
printk(" %s enter \n",__func__);
	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
printk(" %s 1 \n",__func__);
	/* get latest mode again within locked context */
	rc = power_supply_get_property(fsa_priv->usb_psy,
			TCP_NOTIFY_TYPEC_STATE, &mode);
	if (rc) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}
	dev_dbg(dev, "%s: setting GPIOs active = %d\n",
		__func__, mode.intval != TYPEC_UNATTACHED);
printk(" %s mode.intval = %d \n",__func__,mode.intval);
	switch (mode.intval) {
	/* add all modes FSA should notify for in here */
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		/* activate switches */
		ocp96011_usbc_update_settings(fsa_priv, 0x00, 0x9F);

		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->ocp96011_notifier,
		mode.intval, NULL);
		break;
	case TYPEC_UNATTACHED:
		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->ocp96011_notifier,
				TYPEC_UNATTACHED, NULL);

		/* deactivate switches */
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}
printk(" %s end \n",__func__);
done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}
#endif
/*
 * ocp96011_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of ocp96011
 * @node - phandle node to ocp96011 device
 *
 * Returns 0 on success, or error code
 */
#if 1
int ocp96011_reg_notifier(struct notifier_block *nb)
#else
int ocp96011_reg_notifier(struct notifier_block *nb, struct device_node *node)
#endif
{
	int rc = 0;
	/*struct i2c_client *client = of_find_i2c_device_by_node(node);

	if (!client)
		return -EINVAL;
*/
	if (!fsa_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register(&fsa_priv->ocp96011_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	printk("%s: verify if USB adapter is already inserted\n",
		__func__);
	//rc = ocp96011_usbc_analog_setup_switches(fsa_priv);

	return rc;
}
//EXPORT_SYMBOL(ocp96011_reg_notifier);

/*
 * ocp96011_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of ocp96011
 * @node - phandle node to ocp96011 device
 *
 * Returns 0 on pass, or error code
 */
#if 1
int ocp96011_unreg_notifier(struct notifier_block *nb)
#else
int ocp96011_unreg_notifier(struct notifier_block *nb, struct device_node *node)
#endif
{
	int rc = 0;
	//struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct device *dev;
	union power_supply_propval mode;
/*
	if (!client)
		return -EINVAL;
*/
	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode within locked context */
	rc = power_supply_get_property(fsa_priv->usb_psy,
			TCP_NOTIFY_TYPEC_STATE, &mode);
	if (rc) {
		dev_dbg(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}
	/* Do not reset switch settings for usb digital hs */
	if (mode.intval != TYPEC_ATTACHED_SNK)
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
	rc = blocking_notifier_chain_unregister
					(&fsa_priv->ocp96011_notifier, nb);
done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}
EXPORT_SYMBOL(ocp96011_unreg_notifier);

static int ocp96011_validate_display_port_settings(struct ocp96011_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, OCP96011_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * ocp96011_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to ocp96011 device
 * @event - fsa_function enum
 *
 * Returns int on whether the switch happened or not
 */
int ocp96011_switch_event( enum fsa_function event)
{
//	int switch_control = 0;
	int reg12 = 0;
	int reg4b7 = 0;

	//struct i2c_client *client = of_find_i2c_device_by_node(node);

	//if (!client)
	//	return -EINVAL;

	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

	regmap_read(fsa_priv->regmap, 0x12,&reg12);
	printk("%s  event=%d reg12=0x%x\n", __func__, event, reg12);

	switch (event) {
	case 0://config as Audio switch
		ocp96011_usbc_update_settings(fsa_priv, 0x00, 0x9F);
		regmap_write(fsa_priv->regmap, 0x12, 0x01);
		break;
	case 1://config as USB switch
		regmap_write(fsa_priv->regmap, 0x12, 0x00);
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
		return ocp96011_validate_display_port_settings(fsa_priv);
	case 2://config as high-Z status note:set 04Hb7 = 0
		regmap_read(fsa_priv->regmap, 0x04,&reg4b7);
		//printk("%s config read0 reg4b7=0x%x\n", __func__, reg4b7);
		reg4b7 = reg4b7 & 0x7f;
		regmap_write(fsa_priv->regmap, 0x04, reg4b7);
		regmap_read(fsa_priv->regmap, 0x04,&reg4b7);
		//printk("%s config 04Hb7 default 0 read1 reg4b7=0x%x\n", __func__, reg4b7);
		break;

#if 0
	case FSA_MIC_GND_SWAP://耳机通道，自动识别mic/GND是否反接
		regmap_read(fsa_priv->regmap, OCP96011_SWITCH_CONTROL,&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		ocp96011_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		break;
	case FSA_USBC_ORIENTATION_CC1:
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0xF8);
		return ocp96011_validate_display_port_settings(fsa_priv);
	case FSA_USBC_ORIENTATION_CC2:
		ocp96011_usbc_update_settings(fsa_priv, 0x78, 0xF8);
		return ocp96011_validate_display_port_settings(fsa_priv);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		break;
#endif

	}

	return 0;
}
EXPORT_SYMBOL(ocp96011_switch_event);


#if 0
static void ocp96011_usbc_analog_work_fn(struct work_struct *work)
{
printk(" %s enter \n", __func__);
	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	ocp96011_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
printk(" %s end \n", __func__);
}
#endif

static void ocp96011_update_reg_defaults(struct regmap *regmap)
{
	u8 i;
	//printk("%s enter \n", __func__);
	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

static void ocp96011_i2c_reset(void)
{
	int reg4 = 0;
	printk("%s enter \n", __func__);
	regmap_write(fsa_priv->regmap, 0x1e, 0x01);
	regmap_read(fsa_priv->regmap, 0x04,&reg4);
//	printk("%s read reg4=0x%x\n", __func__, reg4);
}


static int ocp96011_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	int rc = 0;
	int ret;

	pr_notice("[KE/ocp96011] name=%s addr=0x%x\n",i2c->name, i2c->addr);

	ret = i2c_check_functionality(i2c->adapter,I2C_FUNC_SMBUS_I2C_BLOCK |I2C_FUNC_SMBUS_BYTE_DATA);
	printk("%s I2C functionality : %s\n", __func__, ret ? "ok" : "fail");

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	fsa_priv->dev = &i2c->dev;
	fsa_priv->usb_psy = power_supply_get_by_name("usb");
	if (!fsa_priv->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_dbg(fsa_priv->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
		goto err_data;
	}
	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &ocp96011_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_supply;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_supply;
	}

	ocp96011_i2c_reset();
	ocp96011_update_reg_defaults(fsa_priv->regmap);

	//ocp96011_switch_event(2);

	fsa_priv->psy_nb.notifier_call = ocp96011_usbc_event_changed;
	fsa_priv->psy_nb.priority = 0;

//#if 1
//	rc = ocp96011_reg_notifier(&fsa_priv->psy_nb);
//#else
//	rc = power_supply_reg_notifier(&fsa_priv->psy_nb);
//#endif
//	if (rc) {
//		dev_info(fsa_priv->dev, "%s: power supply reg failed: %d\n",
//			__func__, rc);
//		goto err_supply;
//	}

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);
	//INIT_WORK(&fsa_priv->usbc_analog_work,ocp96011_usbc_analog_work_fn);

	fsa_priv->ocp96011_notifier.rwsem = (struct rw_semaphore)__RWSEM_INITIALIZER((fsa_priv->ocp96011_notifier).rwsem);
	fsa_priv->ocp96011_notifier.head = NULL;
	return 0;

err_supply:
	power_supply_put(fsa_priv->usb_psy);
err_data:
	devm_kfree(&i2c->dev, fsa_priv);

	return rc;
}

static int ocp96011_remove(struct i2c_client *i2c)
{

	if (!fsa_priv)
		return -EINVAL;

	ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	/* deregister from PMI */
#if 1
	ocp96011_unreg_notifier(&fsa_priv->psy_nb);
#else
	power_supply_unreg_notifier(&fsa_priv->psy_nb);
#endif
	power_supply_put(fsa_priv->usb_psy);
	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	i2c_unregister_device(i2c);

	return 0;
}

static void ocp96011_shutdown(struct i2c_client *i2c)
{
	printk("%s enter \n", __func__);
	ocp96011_switch_event(1);
}

///#ifdef CONFIG_OF
static const struct of_device_id ocp96011_i2c_dt_match[] = {
	{.compatible = "ocp96011"},
	{}
};
MODULE_DEVICE_TABLE(of, ocp96011_i2c_dt_match);
//#endif
static const struct i2c_device_id ocp96011_id[] = {
	{"ocp96011", 0},
	{}
};

static struct i2c_driver ocp96011_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ocp96011",
//#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ocp96011_i2c_dt_match),
//#endif
	},
	.probe = ocp96011_probe,
	.remove = ocp96011_remove,
	.shutdown = ocp96011_shutdown,
	.id_table = ocp96011_id,
};

static int __init ocp96011_init(void)
{
	int rc = 0;
	printk(" %s enter\n", __func__);
	rc = i2c_add_driver(&ocp96011_i2c_driver);
	if (rc){
		pr_err("ocp96011: Failed to register I2C driver: %d\n", rc);
	}
	
	return rc;
}
module_init(ocp96011_init);

static void __exit ocp96011_exit(void)
{
	i2c_del_driver(&ocp96011_i2c_driver);
}
module_exit(ocp96011_exit);

MODULE_DESCRIPTION("OCP96011 I2C driver");
MODULE_LICENSE("GPL v2");
