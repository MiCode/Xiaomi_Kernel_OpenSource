// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include <linux/pinctrl/consumer.h>

/*
 * use tcpc dev to detect audio plug in
 */
#include "../typec/tcpc/inc/tcpci_core.h"
#include "../typec/tcpc/inc/tcpm.h"

#define USE_POWER_SUPPLY_NOTIFIER    0
#define USE_TCPC_NOTIFIER            1

enum et_function {
	ET_MIC_GND_SWAP,
	ET_USBC_ORIENTATION_CC1,
	ET_USBC_ORIENTATION_CC2,
	ET_USBC_DISPLAYPORT_DISCONNECTED,
	ET_EVENT_MAX,
};

enum SWITCH_STATUS {
	SWITCH_STATUS_INVALID = 0,
	SWITCH_STATUS_NOT_CONNECTED,
	SWITCH_STATUS_USB_MODE,
	SWITCH_STATUS_HEADSET_MODE,
	SWITCH_STATUS_MAX
};

char *switch_status_string[SWITCH_STATUS_MAX] = {
	"switch invalid",
	"switch not connected",
	"switch usb mode",
	"switch headset mode",
};

#define ET7480_I2C_NAME	"et7480-driver"

#define ET7480_ID              0x00
#define ET7480_SWITCH_SETTINGS 0x04
#define ET7480_SWITCH_CONTROL  0x05
#define ET7480_SWITCH_STATUS0  0x06
#define ET7480_SWITCH_STATUS1  0x07
#define ET7480_SLOW_L          0x08
#define ET7480_SLOW_R          0x09
#define ET7480_SLOW_MIC        0x0A
#define ET7480_SLOW_SENSE      0x0B
#define ET7480_SLOW_GND        0x0C
#define ET7480_DELAY_L_R       0x0D
#define ET7480_DELAY_L_MIC     0x0E
#define ET7480_DELAY_L_SENSE   0x0F
#define ET7480_DELAY_L_AGND    0x10
#define ET7480_FUNCTION_ENABLE 0x12
#define ET7480_JACK_STATUS     0x17
#define ET7480_DETECTION_INT   0x18
#define ET7480_RESET           0x1E
#define ET7480_CURRENT_SOURCE  0x1F

#define ET_DBG_TYPE_MODE          0
#define ET_DBG_REG_MODE           1

#define ET7480_DELAY_INIT_TIME     (10 * HZ)

#define ET7480_ID_VALUE   0x88
#define DIO4480_ID_VALUE   0xF1

static struct timer_list et7480_enable_timer;
static struct work_struct et7480_enable_switch_work;
static struct workqueue_struct *et7480_enable_switch_workqueue;

static struct timer_list et7480_delay_init_timer;
static struct work_struct et7480_delay_init_work;
static struct workqueue_struct *et7480_delay_init_workqueue;

struct et7480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct tcpc_device *tcpc_dev;
	struct notifier_block psy_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;//not used
	struct blocking_notifier_head et7480_notifier;
	struct mutex notification_lock;
	struct pinctrl *uart_en_gpio_pinctrl;
	struct pinctrl_state *pinctrl_state_enable;
	struct pinctrl_state *pinctrl_state_disable;
	bool dio4480;
};

static struct et7480_priv *global_et7480_data = NULL;

struct et7480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config et7480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = ET7480_CURRENT_SOURCE,
};

static const struct et7480_reg_val et_reg_i2c_defaults[] = {
	{ET7480_SWITCH_SETTINGS, 0x98},
	{ET7480_SWITCH_CONTROL, 0x18},
	{ET7480_SLOW_L, 0x00},
	{ET7480_SLOW_R, 0x00},
	{ET7480_SLOW_MIC, 0x00},
	{ET7480_SLOW_SENSE, 0x00},
	{ET7480_SLOW_GND, 0x00},
	{ET7480_DELAY_L_R, 0x00},
	{ET7480_DELAY_L_MIC, 0x00},
	{ET7480_DELAY_L_SENSE, 0x00},
	{ET7480_DELAY_L_AGND, 0x00},
	{ET7480_FUNCTION_ENABLE, 0x48},
	{ET7480_CURRENT_SOURCE, 0x07},
};

enum SWITCH_STATUS et7480_get_switch_mode(void);

extern void accdet_eint_callback_wrapper(unsigned int plug_status);
static void dump_register(void);

static void et7480_usbc_update_settings(struct et7480_priv *et_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!et_priv) {
		pr_err("%s: invalid et_priv %p\n", __func__, et_priv);
		return;
	}
	if (!et_priv->regmap) {
		dev_err(et_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, 0x80);
	regmap_write(et_priv->regmap, ET7480_SWITCH_CONTROL, switch_control);
	/* ET7480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, switch_enable);
}

#if 0
#if USE_POWER_SUPPLY_NOTIFIER
static int et7480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{//not used.....
	int ret;
	union power_supply_propval mode;
	struct et7480_priv *et_priv =
			container_of(nb, struct et7480_priv, psy_nb);
	struct device *dev;

	pr_info("%s \n", __func__);

	if (!et_priv)
		return -EINVAL;

	dev = et_priv->dev;
	if (!dev)
		return -EINVAL;

	if ((struct power_supply *)ptr != et_priv->usb_psy ||
				evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(et_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (ret) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, ret);
		return ret;
	}

	dev_dbg(dev, "%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, et_priv->usbc_mode.counter,
		POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);
	pr_info("%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, et_priv->usbc_mode.counter,
		POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);

	switch (mode.intval) {
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
	case POWER_SUPPLY_TYPEC_NONE:
		if (atomic_read(&(et_priv->usbc_mode)) == mode.intval)
			break; /* filter notifications received before */
		atomic_set(&(et_priv->usbc_mode), mode.intval);

		dev_dbg(dev, "%s: queueing usbc_analog_work\n",
			__func__);
		pr_info("%s: queueing usbc_analog_work\n",
			__func__);
		pm_stay_awake(et_priv->dev);
		queue_work(system_freezable_wq, &et_priv->usbc_analog_work);
		break;
	default:
		break;
	}
	return ret;
}
#endif
#endif

static int et7480_tcpc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	struct tcp_notify *noti = ptr;
	struct et7480_priv *et_priv = container_of(nb, struct et7480_priv, psy_nb);

	if (NULL == noti) {
		pr_err("%s: data is NULL. \n", __func__);
		return NOTIFY_DONE;
	}

	switch (evt) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_AUDIO) {
			/* Audio Plug in */
			pr_info("%s: Audio Plug In \n", __func__);
			pinctrl_select_state(et_priv->uart_en_gpio_pinctrl,
						et_priv->pinctrl_state_disable);
			//et7480_usbc_update_settings(et_priv, 0x00, 0x9F); // switch to headset
			if (et_priv->dio4480) {
				regmap_write(et_priv->regmap, ET7480_RESET, 0x01);
				mdelay(1);
				regmap_write(et_priv->regmap, ET7480_FUNCTION_ENABLE, 0x49);
			} else {
				regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, 0x9F);
				regmap_write(et_priv->regmap, ET7480_SWITCH_CONTROL, 0x00);
				regmap_write(et_priv->regmap, ET7480_FUNCTION_ENABLE, 0x49);
			}
			if (et_priv->dio4480) {
				mod_timer(&et7480_enable_timer, jiffies + (int)(1 * HZ));
				pr_info("%s: dio4480 delay 1000ms \n", __func__);
			} else {
				mod_timer(&et7480_enable_timer, jiffies + (int)(0.05 * HZ));
				pr_info("%s: et7480 delay 50ms \n", __func__);
				//accdet_eint_callback_wrapper(1);
			}
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* Audio Plug out */
			pr_info("%s: Audio Plug Out \n", __func__);
			if (et_priv->dio4480) {
				regmap_write(et_priv->regmap, ET7480_RESET, 0x01);
				mdelay(1);
				regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, 0x9f);
				regmap_write(et_priv->regmap, ET7480_SWITCH_CONTROL, 0x18);
				//regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, 0x98);
			} else {
				regmap_write(et_priv->regmap, ET7480_RESET, 0x01);
				mdelay(1);
				regmap_write(et_priv->regmap, ET7480_SWITCH_SETTINGS, 0x9f);
				regmap_write(et_priv->regmap, ET7480_SWITCH_CONTROL, 0x18);
			}
			pinctrl_select_state(et_priv->uart_en_gpio_pinctrl,
						et_priv->pinctrl_state_enable);
			accdet_eint_callback_wrapper(0);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state != TYPEC_ATTACHED_AUDIO) {
			if (et7480_get_switch_mode() != SWITCH_STATUS_USB_MODE) {
				et7480_usbc_update_settings(et_priv, 0x18, 0x98);
				pinctrl_select_state(et_priv->uart_en_gpio_pinctrl,
						et_priv->pinctrl_state_enable);
			}
		}
		break;
	}

	return NOTIFY_OK;
}

#if 0
static int et7480_usbc_analog_setup_switches(struct et7480_priv *et_priv)
{//not used.........
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;

	if (!et_priv)
		return -EINVAL;
	dev = et_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&et_priv->notification_lock);

	/* get latest mode again within locked context */
	rc = power_supply_get_property(et_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (rc) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, rc);
		goto done;
	}
	dev_info(dev, "%s: setting GPIOs active = %d, mode.intval = %d\n",
		__func__, mode.intval != POWER_SUPPLY_TYPEC_NONE, mode.intval);

	pr_info("%s \n", __func__);

	switch (mode.intval) {
	/* add all modes ET should notify for in here */
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		/* activate switches */
		et7480_usbc_update_settings(et_priv, 0x00, 0x9F);//

		/* notify call chain on event */
		blocking_notifier_call_chain(&et_priv->et7480_notifier,
		mode.intval, NULL);
		break;
	case POWER_SUPPLY_TYPEC_NONE:
		/* notify call chain on event */
		blocking_notifier_call_chain(&et_priv->et7480_notifier,
				POWER_SUPPLY_TYPEC_NONE, NULL);//

		/* deactivate switches */
		et7480_usbc_update_settings(et_priv, 0x18, 0x98);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}

done:
	mutex_unlock(&et_priv->notification_lock);
	return rc;
}
#endif

/*
 * et7480_reg_notifier - register notifier block with et driver
 *
 * @nb - notifier block of et7480
 * @node - phandle node to et7480 device
 *
 * Returns 0 on success, or error code
 */
#if 0
int et7480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{//not used.....
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct et7480_priv *et_priv;

	if (!client)
		return -EINVAL;

	et_priv = (struct et7480_priv *)i2c_get_clientdata(client);
	if (!et_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&et_priv->et7480_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(et_priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);
	rc = et7480_usbc_analog_setup_switches(et_priv);

	return rc;
}
EXPORT_SYMBOL(et7480_reg_notifier);
#endif

/*
 * et7480_unreg_notifier - unregister notifier block with et driver
 *
 * @nb - notifier block of et7480
 * @node - phandle node to et7480 device
 *
 * Returns 0 on pass, or error code
 */
int et7480_unreg_notifier(struct notifier_block *nb,
			     struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct et7480_priv *et_priv;

	if (!client)
		return -EINVAL;

	et_priv = (struct et7480_priv *)i2c_get_clientdata(client);
	if (!et_priv)
		return -EINVAL;

	et7480_usbc_update_settings(et_priv, 0x18, 0x98);
	return blocking_notifier_chain_unregister
					(&et_priv->et7480_notifier, nb);
}
EXPORT_SYMBOL(et7480_unreg_notifier);

static int et7480_validate_display_port_settings(struct et7480_priv *et_priv)
{
	u32 switch_status = 0;

	regmap_read(et_priv->regmap, ET7480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * et7480_switch_event - configure ET switch position based on event
 *
 * @node - phandle node to et7480 device
 * @event - et_function enum
 *
 * Returns int on whether the switch happened or not
 */
int et7480_switch_event(struct device_node *node,
			 enum et_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct et7480_priv *et_priv;

	if (!client)
		return -EINVAL;

	et_priv = (struct et7480_priv *)i2c_get_clientdata(client);
	if (!et_priv)
		return -EINVAL;
	if (!et_priv->regmap)
		return -EINVAL;

	switch (event) {
	case ET_MIC_GND_SWAP:
		regmap_read(et_priv->regmap, ET7480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		et7480_usbc_update_settings(et_priv, switch_control, 0x9F);
		return 1;
	case ET_USBC_ORIENTATION_CC1:
		et7480_usbc_update_settings(et_priv, 0x18, 0xF8);
		return et7480_validate_display_port_settings(et_priv);
	case ET_USBC_ORIENTATION_CC2:
		et7480_usbc_update_settings(et_priv, 0x78, 0xF8);
		return et7480_validate_display_port_settings(et_priv);
	case ET_USBC_DISPLAYPORT_DISCONNECTED:
		et7480_usbc_update_settings(et_priv, 0x18, 0x98);
		break;
	default:
		break;
	}
	dump_register();
	return 0;
}
EXPORT_SYMBOL(et7480_switch_event);

#if 0
static void et7480_usbc_analog_work_fn(struct work_struct *work)
{//not used
	struct et7480_priv *et_priv =
		container_of(work, struct et7480_priv, usbc_analog_work);

	if (!et_priv) {
		pr_err("%s: et container invalid\n", __func__);
		return;
	}
	et7480_usbc_analog_setup_switches(et_priv);
	pm_relax(et_priv->dev);
}
#endif

static void et7480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(et_reg_i2c_defaults); i++)
		regmap_write(regmap, et_reg_i2c_defaults[i].reg,
					et_reg_i2c_defaults[i].val);
}

/* add et7480 info node */
enum SWITCH_STATUS et7480_get_switch_mode(void)
{
	uint val = 0;
	enum SWITCH_STATUS state = SWITCH_STATUS_INVALID;
	regmap_read(global_et7480_data->regmap, ET7480_SWITCH_STATUS0, &val);

	switch (val & 0xf) {
	case 0x0:
		state = SWITCH_STATUS_NOT_CONNECTED;
		break;
	case 0x5:
		state = SWITCH_STATUS_USB_MODE;
		break;
	case 0xA:
		state = SWITCH_STATUS_HEADSET_MODE;
		break;
	default:
		state = SWITCH_STATUS_INVALID;
		break;
	}

	return state;
}

int et7480_switch_mode(enum SWITCH_STATUS val)
{
	if (val == SWITCH_STATUS_HEADSET_MODE) {
		et7480_usbc_update_settings(global_et7480_data, 0x00, 0x9F); // switch to headset
	} else if (val == SWITCH_STATUS_USB_MODE) {
		et7480_usbc_update_settings(global_et7480_data, 0x18, 0x98); // switch to USB
	}

	return 0;
}

static ssize_t sysfs_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf, u32 type)
{
	int value = 0;
	char *mode = "Unknown mode";
	int i = 0;
	ssize_t ret_size = 0;

	switch (type) {
	case ET_DBG_TYPE_MODE:
		value = et7480_get_switch_mode();
		mode = switch_status_string[value];
		ret_size += sprintf(buf, "%s: %d \n", mode, value);
		break;

	case ET_DBG_REG_MODE:
		for (i = 0; i <= ET7480_CURRENT_SOURCE; i++) {
			regmap_read(global_et7480_data->regmap, i, &value);
			ret_size += sprintf(buf + ret_size, "Reg: 0x%x, Value: 0x%x \n", i, value);
		}
	    break;
	default:
		pr_warn("%s: invalid type %d\n", __func__, type);
		break;
	}
	return ret_size;
}

static ssize_t sysfs_set(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count, u32 type)
{
	int err;
	unsigned long value;

	err = kstrtoul(buf, 10, &value);
	if (err) {
		pr_warn("%s: get data of type %d failed\n", __func__, type);
		return err;
	}

	pr_info("%s: set type %d, data %ld\n", __func__, type, value);
	switch (type) {
		case ET_DBG_TYPE_MODE:
			et7480_switch_mode((enum SWITCH_STATUS)value);
			break;
		default:
			pr_warn("%s: invalid type %d\n", __func__, type);
			break;
	}
	return count;
}

#define et7480_DEVICE_SHOW(_name, _type) static ssize_t \
show_##_name(struct device *dev, \
			  struct device_attribute *attr, char *buf) \
{ \
	return sysfs_show(dev, attr, buf, _type); \
}

#define et7480_DEVICE_SET(_name, _type) static ssize_t \
set_##_name(struct device *dev, \
			 struct device_attribute *attr, \
			 const char *buf, size_t count) \
{ \
	return sysfs_set(dev, attr, buf, count, _type); \
}

#define et7480_DEVICE_SHOW_SET(name, type) \
et7480_DEVICE_SHOW(name, type) \
et7480_DEVICE_SET(name, type) \
static DEVICE_ATTR(name, S_IWUSR | S_IRUGO, show_##name, set_##name);

et7480_DEVICE_SHOW_SET(et7480_switch_mode, ET_DBG_TYPE_MODE);
et7480_DEVICE_SHOW_SET(et7480_reg, ET_DBG_REG_MODE);

static struct attribute *et7480_attrs[] = {
	&dev_attr_et7480_switch_mode.attr,
	&dev_attr_et7480_reg.attr,
	NULL
};

static const struct attribute_group et7480_group = {
	.attrs = et7480_attrs,
};
/* end of info node */

static int et7480_gpio_pinctrl_init(struct et7480_priv *et_priv)
{
	int retval = 0;

	pr_info("%s \n", __func__);

	et_priv->uart_en_gpio_pinctrl = devm_pinctrl_get(et_priv->dev);
	if (IS_ERR_OR_NULL(et_priv->uart_en_gpio_pinctrl)) {
		retval = PTR_ERR(et_priv->uart_en_gpio_pinctrl);
		goto gpio_err_handle;
	}

	et_priv->pinctrl_state_disable
		= pinctrl_lookup_state(et_priv->uart_en_gpio_pinctrl, "uart_disable");
	if (IS_ERR_OR_NULL(et_priv->pinctrl_state_disable)) {
		retval = PTR_ERR(et_priv->pinctrl_state_disable);
		goto gpio_err_handle;
	}

	et_priv->pinctrl_state_enable
		= pinctrl_lookup_state(et_priv->uart_en_gpio_pinctrl, "uart_enable");
	if (IS_ERR_OR_NULL(et_priv->pinctrl_state_enable)) {
		retval = PTR_ERR(et_priv->pinctrl_state_enable);
		goto gpio_err_handle;
	}

	return 0;

gpio_err_handle:
	pr_info("%s: init failed \n", __func__);
	devm_pinctrl_put(et_priv->uart_en_gpio_pinctrl);
	et_priv->uart_en_gpio_pinctrl = NULL;

	return retval;
}

static void et7480_enable_switch_handler(struct timer_list *t)
{
	int ret = 0;

	ret = queue_work(et7480_enable_switch_workqueue, &et7480_enable_switch_work);
	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

static void et7480_enable_switch_work_callback(struct work_struct *work)
{
	u32 int_status = 0;
	u32 jack_status = 0;
	u32 switch_control = 0;
	u32 switch_setting = 0;
	pr_info("%s()\n", __func__);
	regmap_read(global_et7480_data->regmap, ET7480_DETECTION_INT, &int_status);
	regmap_read(global_et7480_data->regmap, ET7480_JACK_STATUS, &jack_status);
	if (global_et7480_data->dio4480 && (jack_status == 0x01)) {
		pr_info("%s() write reg\n", __func__);
		regmap_write(global_et7480_data->regmap, ET7480_RESET, 0x01);
		mdelay(1);
		regmap_write(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, 0x00);
		regmap_write(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, 0x9f);
	}else if ((!global_et7480_data->dio4480) && (int_status & (1 << 2) ) && (jack_status & (1 << 1))) {
		pr_info("%s: 3-pole detect\n", __func__);
		regmap_read(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, &switch_setting);
		regmap_read(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, &switch_control);

		/* Set Bit 1 to 0, Enable Mic <---> SBU2 */
		regmap_write(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, switch_control & (~(1 << 1)));
		/* ET7480 chip hardware requirement */
		usleep_range(50, 55);
		/* Enable Bit 1, Enable Mic <---> SBU2 Switch */
		regmap_write(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, switch_setting | (1 << 1));
	}
	if (global_et7480_data->dio4480){
		mdelay(20);
	}
	accdet_eint_callback_wrapper(1);
	dump_register();
}

//static void delay_init_timer_callback(unsigned long data)
static void delay_init_timer_callback(struct timer_list *t)
{//(struct timer_list *t)
	int ret = 0;

	ret = queue_work(et7480_delay_init_workqueue, &et7480_delay_init_work);
	pr_info("%s \n", __func__);

	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

static void et7480_delay_init_work_callback(struct work_struct *work)
{
#if USE_TCPC_NOTIFIER
	int rc = 0;
	pr_info("%s() \n", __func__);

	if (global_et7480_data && global_et7480_data->tcpc_dev) {
		/* check tcpc status at startup */
		if (TYPEC_ATTACHED_AUDIO == tcpm_inquire_typec_attach_state(global_et7480_data->tcpc_dev)) {
			/* Audio Plug in */
			pr_info("%s: Audio is Plug In status at startup\n", __func__);
			if(global_et7480_data->dio4480) {
			pinctrl_select_state(global_et7480_data->uart_en_gpio_pinctrl,
						global_et7480_data->pinctrl_state_disable);
			regmap_write(global_et7480_data->regmap, ET7480_RESET, 0x01);
			mdelay(1);
			regmap_write(global_et7480_data->regmap, ET7480_FUNCTION_ENABLE, 0x49);
			mod_timer(&et7480_enable_timer, jiffies + (int)(1* HZ));
			} else {
			pinctrl_select_state(global_et7480_data->uart_en_gpio_pinctrl,
						global_et7480_data->pinctrl_state_disable);
			regmap_write(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, 0x9F);
			regmap_write(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, 0x00);
			regmap_write(global_et7480_data->regmap, ET7480_FUNCTION_ENABLE, 0x49);
			mod_timer(&et7480_enable_timer, jiffies + (int)(0.2 * HZ));
			accdet_eint_callback_wrapper(1);
			}
		} else {
			if (global_et7480_data->dio4480) {
				regmap_write(global_et7480_data->regmap, ET7480_RESET, 0x01);
				mdelay(1);
				regmap_write(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, 0x9f);
				regmap_write(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, 0x18);
			} else {
				regmap_write(global_et7480_data->regmap, ET7480_RESET, 0x01);
				mdelay(1);
				regmap_write(global_et7480_data->regmap, ET7480_SWITCH_SETTINGS, 0x9f);
				regmap_write(global_et7480_data->regmap, ET7480_SWITCH_CONTROL, 0x18);
			}
		}

		/* register tcpc_event */
		global_et7480_data->psy_nb.notifier_call = et7480_tcpc_event_changed;
		global_et7480_data->psy_nb.priority = 0;
		rc = register_tcp_dev_notifier(global_et7480_data->tcpc_dev, &global_et7480_data->psy_nb, TCP_NOTIFY_TYPE_USB);
		if (rc) {
			pr_err("%s: register_tcp_dev_notifier failed\n", __func__);
		}
	}
#endif
}

static void dump_register(void)
{
	int adr = 0, value = 0;
	pr_info("%s:dump %s reg\n",__func__,global_et7480_data->dio4480 ? "dio4480" : "et7480");
	for (adr = 0; adr <= ET7480_CURRENT_SOURCE; adr++) {
		regmap_read(global_et7480_data->regmap, adr, &value);
		pr_info("(0x%x)=0x%x",adr,value);
	}
}

int get_type_c_hph_direction(void)
{
	u32 jack_status;
	regmap_read(global_et7480_data->regmap, ET7480_JACK_STATUS, &jack_status);
	pr_info("%s:jack status is 0x%x\n",__func__,jack_status);
	return jack_status == 0x04;
}
EXPORT_SYMBOL(get_type_c_hph_direction);

static int et7480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct et7480_priv *et_priv;
	int rc = 0;
	int reg_val = 0;

	et_priv = devm_kzalloc(&i2c->dev, sizeof(*et_priv),
				GFP_KERNEL);
	if (!et_priv)
		return -ENOMEM;

	global_et7480_data = et_priv; // add for debug
	et_priv->dev = &i2c->dev;

#if 0
	et_priv->usb_psy = power_supply_get_by_name("usb");
	if (!et_priv->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_dbg(et_priv->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
		goto err_data;
	}
#endif

	et_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!et_priv->tcpc_dev) {
		rc = -EPROBE_DEFER;
		pr_err("%s get tcpc device type_c_port0 fail \n", __func__);
		goto err_data;
	}

	et_priv->regmap = devm_regmap_init_i2c(i2c, &et7480_regmap_config);
	if (IS_ERR_OR_NULL(et_priv->regmap)) {
		dev_err(et_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!et_priv->regmap) {
			rc = -EINVAL;
			goto err_supply;
		}
		rc = PTR_ERR(et_priv->regmap);
		goto err_supply;
	}

	et7480_gpio_pinctrl_init(et_priv);
	if (et_priv->uart_en_gpio_pinctrl) {
		rc = pinctrl_select_state(et_priv->uart_en_gpio_pinctrl,
					et_priv->pinctrl_state_enable);
		if (rc < 0) {
			pr_err("Failed to select enable pinstate %d\n", rc);
		}
	}

	et7480_update_reg_defaults(et_priv->regmap);
#if 0
#if USE_POWER_SUPPLY_NOTIFIER //not used
	et_priv->psy_nb.notifier_call = et7480_usbc_event_changed;//not used
	et_priv->psy_nb.priority = 0;
	rc = power_supply_reg_notifier(&et_priv->psy_nb);
	if (rc) {
		dev_err(et_priv->dev, "%s: power supply reg failed: %d\n",
			__func__, rc);
		goto err_supply;
	}
#endif
#endif
	mutex_init(&et_priv->notification_lock);
	i2c_set_clientdata(i2c, et_priv);

#if 0
	INIT_WORK(&et_priv->usbc_analog_work,
		  et7480_usbc_analog_work_fn);
#endif

	et_priv->et7480_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((et_priv->et7480_notifier).rwsem);
	et_priv->et7480_notifier.head = NULL;

	rc = sysfs_create_group(&i2c->dev.kobj, &et7480_group);
	if (rc) {
		pr_err("%s: create attr error %d\n", __func__, rc);
	}

	et7480_enable_switch_workqueue = create_singlethread_workqueue("enableSwitchQueue");
	INIT_WORK(&et7480_enable_switch_work, et7480_enable_switch_work_callback);
	if (!et7480_enable_switch_workqueue) {
		rc = -1;
		pr_notice("%s create et7480_enable_switch workqueue fail.\n", __func__);
		goto err_data;
	}

	pr_info("%s(), setup enable timer", __func__);
	timer_setup(&et7480_enable_timer, et7480_enable_switch_handler, (unsigned long)et_priv);

	et7480_delay_init_workqueue = create_singlethread_workqueue("delayInitQueue");
	INIT_WORK(&et7480_delay_init_work, et7480_delay_init_work_callback);

	/* delay 2s to register tcpc event change, after accdet init done */
	timer_setup(&et7480_delay_init_timer, delay_init_timer_callback, 0);
	mod_timer(&et7480_delay_init_timer, jiffies + ET7480_DELAY_INIT_TIME);

	regmap_read(et_priv->regmap, ET7480_ID, &reg_val);
	if (reg_val == DIO4480_ID_VALUE) {
		et_priv->dio4480 = true;
		pr_info("%s audio switch use dio4480 reg_val is %x", __func__, reg_val);
	} else {
		et_priv->dio4480 = false;
		pr_info("%s audio switch use et7480 reg_val is %x", __func__, reg_val);
	}

	return 0;

err_supply:
#if 0
#if USE_POWER_SUPPLY_NOTIFIER
	power_supply_put(et_priv->usb_psy);
#endif
#endif
err_data:
	devm_kfree(&i2c->dev, et_priv);
	return rc;
}

static int et7480_remove(struct i2c_client *i2c)
{
	struct et7480_priv *et_priv =
			(struct et7480_priv *)i2c_get_clientdata(i2c);

	if (!et_priv)
		return -EINVAL;

	et7480_usbc_update_settings(et_priv, 0x18, 0x98);
#if 0
#if USE_POWER_SUPPLY_NOTIFIER
	cancel_work_sync(&et_priv->usbc_analog_work);
	pm_relax(et_priv->dev);
	/* deregister from PMI */
	power_supply_unreg_notifier(&et_priv->psy_nb);
	power_supply_put(et_priv->usb_psy);
#endif
#endif
	mutex_destroy(&et_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static const struct of_device_id et7480_i2c_dt_match[] = {
	{
		.compatible = "mediatek,et7480-audioswitch",
	},
	{}
};

static struct i2c_driver et7480_i2c_driver = {
	.driver = {
		.name = ET7480_I2C_NAME,
		.of_match_table = et7480_i2c_dt_match,
	},
	.probe = et7480_probe,
	.remove = et7480_remove,
};

static int __init et7480_init(void)
{
	int rc;
	rc = i2c_add_driver(&et7480_i2c_driver);
	if (rc)
		pr_err("et7480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
late_initcall(et7480_init);

static void __exit et7480_exit(void)
{
	i2c_del_driver(&et7480_i2c_driver);
}
module_exit(et7480_exit);

MODULE_DESCRIPTION("ET7480 I2C driver");
MODULE_LICENSE("GPL v2");
