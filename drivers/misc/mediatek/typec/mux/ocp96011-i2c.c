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
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "ocp96011-i2c.h"
#include "tcpm.h"
#include "../tcpc/inc/tcpci_typec.h"
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

struct ocp96011_priv *fsa_priv;
struct ocp96011_priv *fsa_priv_sub;
struct ocp96011_priv *fsa_priv_sub_temp;
struct ocp96011_reg_val {
	u16 reg;
	u8 val;
};


static int32_t ocp96011_headset_count = 0;
static int32_t headset_value = -1;
static int32_t send_accdet_status_flag = 0;
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

int ocp960_get_headset_count(void)
{
	return ocp96011_headset_count;
}
EXPORT_SYMBOL(ocp960_get_headset_count);

u32 ocp96011_get_headset_status(void)
{
	u32 reg17 = 0;

	usleep_range(100*1000, 150*1000);
	if (headset_value == 1) {
		regmap_read(fsa_priv->regmap, 0x17, &reg17);
		dev_info(fsa_priv->dev, "%s: reg17 = %d\n", __func__, reg17);
	} else if (headset_value == 0){
		regmap_read(fsa_priv_sub->regmap, 0x17, &reg17);
		dev_info(fsa_priv_sub->dev, "%s: reg17 = %d\n", __func__, reg17);
	}
	return reg17;
}
EXPORT_SYMBOL(ocp96011_get_headset_status);

static void ocp96011_usbc_update_settings(struct ocp96011_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
    dev_info(fsa_priv->dev," %s enter switch_control=0x%x switch_enable=0x%x\n", __func__, switch_control, switch_enable);
	if (!fsa_priv->regmap) {
		dev_info(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_SETTINGS, 0x80);
	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_CONTROL, switch_control);
	/* OCP96011 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, OCP96011_SWITCH_SETTINGS, switch_enable);
    dev_info(fsa_priv->dev," %s end\n", __func__);

}
#if 0
static int ocp96011_usbc_event_changed(struct notifier_block *nb,unsigned long evt, void *ptr)
{

	int ret;
	union power_supply_propval mode;
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if ((struct power_supply *)ptr != fsa_priv->usb_psy ||
				evt != PSY_EVENT_PROP_CHANGED)
	{
		pr_info(" %s 2.5 \n", __func__);
		return 0;
	}

	ret = power_supply_get_property(fsa_priv->usb_psy,
			TCP_NOTIFY_TYPEC_STATE, &mode);
	if (ret) {
		dev_info(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, ret);
		return ret;
	}

	pr_info( "%s:4 USB change event received, supply mode %d, usbc mode %d, expected %d\n",
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
		//schedule_work(&fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}

	return ret;
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
int ocp96011_reg_notifier(struct notifier_block *nb)
{
	int rc = 0;

	if (!fsa_priv)
		return -EINVAL;

	if (!fsa_priv_sub)
		return -EINVAL;

	rc = blocking_notifier_chain_register(&fsa_priv->ocp96011_notifier, nb);
	if (rc)
		return rc;

	rc = blocking_notifier_chain_register(&fsa_priv_sub->ocp96011_notifier, nb);
	if (rc)
		return rc;

	send_accdet_status_flag = 1;

	return rc;
}
EXPORT_SYMBOL(ocp96011_reg_notifier);

/*
 * ocp96011_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of ocp96011
 * @node - phandle node to ocp96011 device
 *
 * Returns 0 on pass, or error code
 */
int ocp96011_unreg_notifier(struct notifier_block *nb)
{
	int rc = 0;
	struct device *dev;
	union power_supply_propval mode;

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
		dev_info(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
	}
	/* Do not reset switch settings for usb digital hs */
	if (mode.intval != TYPEC_ATTACHED_SNK)
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
	rc = blocking_notifier_chain_unregister
					(&fsa_priv->ocp96011_notifier, nb);

	mutex_unlock(&fsa_priv->notification_lock);

	send_accdet_status_flag = 0;

	return rc;
}
EXPORT_SYMBOL(ocp96011_unreg_notifier);

static int ocp96011_validate_display_port_settings(struct ocp96011_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, OCP96011_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_info("AUX SBU1/2 switch status is invalid = %u\n",
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
int ocp96011_switch_event( enum fsa_function event, struct ocp96011_priv *fsa_priv)
{
	int reg12 = 0;
	int reg4b7 = 0;
	int i = 0;

	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

	regmap_read(fsa_priv->regmap, 0x12,&reg12);

	dev_info(fsa_priv->dev,"%s  reg12=0x%x\n", __func__, reg12);
	dev_info(fsa_priv->dev,"%s  event=%d, 0=usb, 1=audio-hs, 2&3=DP plug in, 4= DP plug out, 5= High-Z \n", __func__, event);

	switch (event) {
	case FSA_SWITCH_TO_USB://config as USB switch
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
		if(ocp96011_headset_count > 0)
			ocp96011_headset_count--;
		if (fsa_priv->addr == 0x43)
			headset_value = 0;
		else
			headset_value = 1;
		blocking_notifier_call_chain(&fsa_priv->ocp96011_notifier, event, NULL);
		return ocp96011_validate_display_port_settings(fsa_priv);
	case FSA_SWITCH_TO_AUDIO://config as Audio switch
		regmap_write(fsa_priv->regmap, 0x1e, 0x01);
		usleep_range(1000, 1005);
		regmap_write(fsa_priv->regmap, 0x12, 0x45);
		ocp96011_headset_count++;
		if (fsa_priv->addr == 0x43)
			headset_value = 0;
		else 
			headset_value = 1;
		while ((send_accdet_status_flag == 0) && (i <= 300)) {
			msleep(100);
			i++;
		}
		blocking_notifier_call_chain(&fsa_priv->ocp96011_notifier, event, NULL);
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
	case FSA_SWITCH_TO_HIGH_Z_STATUS://config as high-Z status note:set 04Hb7 = 0
		regmap_read(fsa_priv->regmap, 0x04,&reg4b7);
		reg4b7 = reg4b7 & 0x7f;
		regmap_write(fsa_priv->regmap, 0x04, reg4b7);
		regmap_read(fsa_priv->regmap, 0x04,&reg4b7);
		break;
	default:
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

static void ocp96011_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

static void ocp96011_i2c_reset(struct ocp96011_priv *fsa_priv)
{
	int reg4 = 0;

	regmap_write(fsa_priv->regmap, 0x1e, 0x01);
	regmap_read(fsa_priv->regmap, 0x04,&reg4);
}

#if IS_ENABLED(CONFIG_TCPC_CLASS)
static int ocp96011_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct ocp96011_priv *rpmd =
		container_of(nb, struct ocp96011_priv, tcpc_nb);
	struct typec_mux_state state = {.mode = event, .data = noti};

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		dev_info(rpmd->dev, "old_state=%d, new_state=%d, polarity=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state,
				noti->typec_state.polarity);

		if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			    noti->typec_state.old_state == TYPEC_ATTACHED_DEBUG) &&
			    noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s OTG plug out\n", __func__);
			ocp96011_switch_event(FSA_USBC_DISPLAYPORT_DISCONNECTED, rpmd);
			/* disable host connection */
		}

		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			dev_info(rpmd->dev, "accdet Audio plug in\n");
			ocp96011_switch_event(FSA_SWITCH_TO_AUDIO, rpmd);			
			break;
		}

		if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "accdet Audio plug out\n");
			ocp96011_switch_event(FSA_SWITCH_TO_USB, rpmd);
			break;
		}
		break;
	case TCP_NOTIFY_AMA_DP_HPD_STATE:
		dev_info(rpmd->dev, "%s irq = %u, state = %u, orient = %d\n",
				    __func__, noti->ama_dp_hpd_state.irq,
				    noti->ama_dp_hpd_state.state,noti->typec_state.polarity);

		typec_mux_set(rpmd->mux, &state);
		if(noti->typec_state.polarity){
			dev_info(rpmd->dev, "$s, c1 polarity %d\n", __func__, noti->typec_state.polarity);
			ocp96011_switch_event(FSA_USBC_ORIENTATION_CC2, rpmd);
		}else{
			dev_info(rpmd->dev, "$s, c2 polarity %d\n", __func__, noti->typec_state.polarity);
			ocp96011_switch_event(FSA_USBC_ORIENTATION_CC1, rpmd);
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int ocp96011_tcpc_init(struct ocp96011_priv *fsa_priv)
{
	struct tcpc_device *tcpc_dev;
	struct device_node *np = fsa_priv->dev->of_node;
	const char *tcpc_name;
	int ret;

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;

	tcpc_dev = tcpc_dev_get_by_name(tcpc_name);
	if (!tcpc_dev) {
		dev_info(fsa_priv->dev, "get tcpc device fail\n");
		return -ENODEV;
	}

	fsa_priv->tcpc_nb.notifier_call = ocp96011_tcpc_notifier;
	ret = register_tcp_dev_notifier(tcpc_dev, &fsa_priv->tcpc_nb,
			TCP_NOTIFY_TYPE_USB);
	if (ret < 0) {
		dev_info(fsa_priv->dev, "register notifer fail\n");
		return -EINVAL;
	}

	fsa_priv->tcpc_dev = tcpc_dev;
	if (strncmp(tcpc_name, "type_c_port0", 12) == 0)
		fsa_priv_sub = fsa_priv;

	return 0;
}
#endif

static int ocp96011_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	int rc = 0;
	int ret;
	struct device_node *np = i2c->dev.of_node;
	const char *tcpc_name;

	pr_notice("[KE/ocp96011] name=%s addr=0x%x\n",i2c->name, i2c->addr);

	ret = i2c_check_functionality(i2c->adapter,I2C_FUNC_SMBUS_I2C_BLOCK |I2C_FUNC_SMBUS_BYTE_DATA);
	pr_notice("%s I2C functionality : %s\n", __func__, ret ? "ok" : "fail");

	ret = of_property_read_string(np, "tcpc", &tcpc_name);
	if (ret < 0)
		return -ENODEV;
	if (strncmp(tcpc_name, "type_c_port0", 12) == 0) {
		fsa_priv_sub = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),GFP_KERNEL);
		if (!fsa_priv_sub)
		{
			pr_notice("ocp96011_probe malloc fail %s \n",tcpc_name);
			return -ENOMEM;
		}
		fsa_priv_sub_temp = fsa_priv_sub;
	}	

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;
	fsa_priv->dev = &i2c->dev;
	fsa_priv->addr = i2c->addr;
	fsa_priv->usb_psy = power_supply_get_by_name("usb");
	if (!fsa_priv->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_info(fsa_priv->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
		//goto err_data;
	}
	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &ocp96011_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_info(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_supply;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_supply;
	}
	ocp96011_i2c_reset(fsa_priv);
	ocp96011_update_reg_defaults(fsa_priv->regmap);
#if 0
	fsa_priv->psy_nb.notifier_call = ocp96011_usbc_event_changed;
	fsa_priv->psy_nb.priority = 0;

	rc = ocp96011_reg_notifier(&fsa_priv->psy_nb);
	if (rc) {
		dev_info(fsa_priv->dev, "%s: power supply reg failed: %d\n",
			__func__, rc);
		goto err_supply;
	}
#endif
	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	fsa_priv->ocp96011_notifier.rwsem = (struct rw_semaphore)__RWSEM_INITIALIZER((fsa_priv->ocp96011_notifier).rwsem);
	fsa_priv->ocp96011_notifier.head = NULL;

#if IS_ENABLED(CONFIG_TCPC_CLASS)
	ret = ocp96011_tcpc_init(fsa_priv);
	if (ret < 0) {
		dev_info(fsa_priv->dev, "failed to init tcpc\n");
		goto err_supply;
	}
#endif
	return 0;

err_supply:
	power_supply_put(fsa_priv->usb_psy);
//err_data: z17 add
	devm_kfree(&i2c->dev, fsa_priv);
	if (!fsa_priv_sub_temp)
	{
		devm_kfree(&i2c->dev, fsa_priv_sub_temp);
	}
	return rc;
}

static int ocp96011_remove(struct i2c_client *i2c)
{

	if (i2c->addr == 0x42) {
		if (!fsa_priv)
			return -EINVAL;
		ocp96011_usbc_update_settings(fsa_priv, 0x18, 0x98);
		pm_relax(fsa_priv->dev);
		ocp96011_unreg_notifier(&fsa_priv->psy_nb);
		power_supply_put(fsa_priv->usb_psy);
		mutex_destroy(&fsa_priv->notification_lock);
		devm_kfree(&i2c->dev, fsa_priv);
	}
	else {
		if (!fsa_priv_sub)
			return -EINVAL;
		ocp96011_usbc_update_settings(fsa_priv_sub, 0x18, 0x98);
		pm_relax(fsa_priv_sub->dev);
		ocp96011_unreg_notifier(&fsa_priv_sub->psy_nb);
		power_supply_put(fsa_priv_sub->usb_psy);
		mutex_destroy(&fsa_priv_sub->notification_lock);
		if (!fsa_priv_sub_temp)
		{
			devm_kfree(&i2c->dev, fsa_priv_sub_temp);
		}
	}
	dev_set_drvdata(&i2c->dev, NULL);

	i2c_unregister_device(i2c);

	return 0;
}

static void ocp96011_shutdown(struct i2c_client *i2c)
{

	if (i2c->addr == 0x42)
		ocp96011_switch_event(FSA_SWITCH_TO_USB, fsa_priv);
	else
		ocp96011_switch_event(FSA_SWITCH_TO_USB, fsa_priv_sub);
}


static const struct of_device_id ocp96011_i2c_dt_match[] = {
	{.compatible = "ocp96011"},
	{.compatible = "ocp96011-sub"},
	{}
};
MODULE_DEVICE_TABLE(of, ocp96011_i2c_dt_match);

static const struct i2c_device_id ocp96011_id[] = {
	{"ocp96011", 0},
	{"ocp96011-sub", 0},
	{}
};

static struct i2c_driver ocp96011_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ocp96011",
		.of_match_table = of_match_ptr(ocp96011_i2c_dt_match),
	},
	.probe = ocp96011_probe,
	.remove = ocp96011_remove,
	.shutdown = ocp96011_shutdown,
	.id_table = ocp96011_id,
};

static int __init ocp96011_init(void)
{
	int rc = 0;

	rc = i2c_add_driver(&ocp96011_i2c_driver);
	if (rc){
		pr_info("ocp96011: Failed to register I2C driver: %d\n", rc);
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
