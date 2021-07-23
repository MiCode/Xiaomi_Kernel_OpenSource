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

/*
 * use tcpc dev to detect audio plug in
 */
#include "mediatek/typec/tcpc/inc/tcpci_core.h"
#include "mediatek/typec/tcpc/inc/tcpm.h"

#define USE_POWER_SUPPLY_NOTIFIER    0
#define USE_TCPC_NOTIFIER            1

enum fsa_function {
	FSA_MIC_GND_SWAP,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	FSA_USBC_DISPLAYPORT_DISCONNECTED,
	FSA_EVENT_MAX,
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

#define FSA4480_I2C_NAME	"fsa4480-driver"

#define FSA4480_SWITCH_SETTINGS 0x04
#define FSA4480_SWITCH_CONTROL  0x05
#define FSA4480_SWITCH_STATUS0  0x06
#define FSA4480_SWITCH_STATUS1  0x07
#define FSA4480_SLOW_L          0x08
#define FSA4480_SLOW_R          0x09
#define FSA4480_SLOW_MIC        0x0A
#define FSA4480_SLOW_SENSE      0x0B
#define FSA4480_SLOW_GND        0x0C
#define FSA4480_DELAY_L_R       0x0D
#define FSA4480_DELAY_L_MIC     0x0E
#define FSA4480_DELAY_L_SENSE   0x0F
#define FSA4480_DELAY_L_AGND    0x10
#define FSA4480_FUNCTION_ENABLE 0x12
#define FSA4480_JACK_STATUS     0x17
#define FSA4480_DETECTION_INT   0x18
#define FSA4480_RESET           0x1E
#define FSA4480_CURRENT_SOURCE  0x1F

#define FSA_DBG_TYPE_MODE          0
#define FSA_DBG_REG_MODE           1

#define FSA4480_DELAY_INIT_TIME     (10 * HZ)

static struct timer_list fsa4480_enable_timer;
static struct work_struct fsa4480_enable_switch_work;
static struct workqueue_struct *fsa4480_enable_switch_workqueue;

static struct timer_list fsa4480_delay_init_timer;
static struct work_struct fsa4480_delay_init_work;
static struct workqueue_struct *fsa4480_delay_init_workqueue;

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct tcpc_device *tcpc_dev;
	struct notifier_block psy_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_notifier;
	struct mutex notification_lock;
	struct pinctrl *uart_en_gpio_pinctrl;
	struct pinctrl_state *pinctrl_state_enable;
	struct pinctrl_state *pinctrl_state_disable;
};

static struct fsa4480_priv *global_fsa4480_data = NULL;

struct fsa4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FSA4480_CURRENT_SOURCE,
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
	{FSA4480_SWITCH_SETTINGS, 0x98},
	{FSA4480_SWITCH_CONTROL, 0x18},
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
	{FSA4480_DELAY_L_MIC, 0x00},
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x00},
	{FSA4480_FUNCTION_ENABLE, 0x48},
};

enum SWITCH_STATUS fsa4480_get_switch_mode(void);

extern void accdet_eint_callback_wrapper(unsigned int plug_status);

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!fsa_priv) {
		pr_err("%s: invalid fsa_priv %p\n", __func__, fsa_priv);
		return;
	}
	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x80);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
	/* FSA4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
}

static int fsa4480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	int ret;
	union power_supply_propval mode;
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, psy_nb);
	struct device *dev;

	pr_info("%s \n", __func__);

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if ((struct power_supply *)ptr != fsa_priv->usb_psy ||
				evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(fsa_priv->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &mode);
	if (ret) {
		dev_err(dev, "%s: Unable to read USB TYPEC_MODE: %d\n",
			__func__, ret);
		return ret;
	}

	dev_dbg(dev, "%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, fsa_priv->usbc_mode.counter,
		POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);
	pr_info("%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
		__func__, mode.intval, fsa_priv->usbc_mode.counter,
		POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER);

	switch (mode.intval) {
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
	case POWER_SUPPLY_TYPEC_NONE:
		if (atomic_read(&(fsa_priv->usbc_mode)) == mode.intval)
			break; /* filter notifications received before */
		atomic_set(&(fsa_priv->usbc_mode), mode.intval);

		dev_dbg(dev, "%s: queueing usbc_analog_work\n",
			__func__);
		pr_info("%s: queueing usbc_analog_work\n",
			__func__);
		pm_stay_awake(fsa_priv->dev);
		queue_work(system_freezable_wq, &fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}
	return ret;
}

static int fsa4480_tcpc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	struct tcp_notify *noti = ptr;
	struct fsa4480_priv *fsa_priv = container_of(nb, struct fsa4480_priv, psy_nb);

	uint temp_val = 0;

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
			pinctrl_select_state(fsa_priv->uart_en_gpio_pinctrl,
						fsa_priv->pinctrl_state_disable);
			//fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F); // switch to headset
			regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x9F);
			regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0x00);
			regmap_write(fsa_priv->regmap, FSA4480_FUNCTION_ENABLE, 0x49);
			mod_timer(&fsa4480_enable_timer, jiffies + (int)(0.2 * HZ));
			accdet_eint_callback_wrapper(1);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_AUDIO &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			/* Audio Plug out */
			pr_info("%s: Audio Plug Out \n", __func__);
			regmap_write(fsa_priv->regmap, FSA4480_FUNCTION_ENABLE, 0x48);
			fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);  // switch to usb
			pinctrl_select_state(fsa_priv->uart_en_gpio_pinctrl,
						fsa_priv->pinctrl_state_enable);
			accdet_eint_callback_wrapper(0);
		} else if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state != TYPEC_ATTACHED_AUDIO) {
			if (fsa4480_get_switch_mode() != SWITCH_STATUS_USB_MODE) {
				fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
				pinctrl_select_state(fsa_priv->uart_en_gpio_pinctrl,
						fsa_priv->pinctrl_state_enable);
			}
		}
		break;
	}

	return NOTIFY_OK;
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	int rc = 0;
	union power_supply_propval mode;
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode again within locked context */
	rc = power_supply_get_property(fsa_priv->usb_psy,
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
	/* add all modes FSA should notify for in here */
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		/* activate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
		mode.intval, NULL);
		break;
	case POWER_SUPPLY_TYPEC_NONE:
		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
				POWER_SUPPLY_TYPEC_NONE, NULL);

		/* deactivate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}

done:
	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}

/*
 * fsa4480_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on success, or error code
 */
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&fsa_priv->fsa4480_notifier, nb);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	dev_dbg(fsa_priv->dev, "%s: verify if USB adapter is already inserted\n",
		__func__);
	rc = fsa4480_usbc_analog_setup_switches(fsa_priv);

	return rc;
}
EXPORT_SYMBOL(fsa4480_reg_notifier);

/*
 * fsa4480_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on pass, or error code
 */
int fsa4480_unreg_notifier(struct notifier_block *nb,
			     struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	return blocking_notifier_chain_unregister
					(&fsa_priv->fsa4480_notifier, nb);
}
EXPORT_SYMBOL(fsa4480_unreg_notifier);

static int fsa4480_validate_display_port_settings(struct fsa4480_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * fsa4480_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to fsa4480 device
 * @event - fsa_function enum
 *
 * Returns int on whether the switch happened or not
 */
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

	switch (event) {
	case FSA_MIC_GND_SWAP:
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		return 1;
	case FSA_USBC_ORIENTATION_CC1:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_ORIENTATION_CC2:
		fsa4480_usbc_update_settings(fsa_priv, 0x78, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(fsa4480_switch_event);

static void fsa4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct fsa4480_priv *fsa_priv =
		container_of(work, struct fsa4480_priv, usbc_analog_work);

	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	fsa4480_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
}

static void fsa4480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
					fsa_reg_i2c_defaults[i].val);
}

/* add fsa4480 info node */
enum SWITCH_STATUS fsa4480_get_switch_mode(void)
{
	uint val = 0;
	enum SWITCH_STATUS state = SWITCH_STATUS_INVALID;
	regmap_read(global_fsa4480_data->regmap, FSA4480_SWITCH_STATUS0, &val);

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

int fsa4480_switch_mode(enum SWITCH_STATUS val)
{
	if (val == SWITCH_STATUS_HEADSET_MODE) {
		fsa4480_usbc_update_settings(global_fsa4480_data, 0x00, 0x9F); // switch to headset
	} else if (val == SWITCH_STATUS_USB_MODE) {
		fsa4480_usbc_update_settings(global_fsa4480_data, 0x18, 0x98); // switch to USB
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
	case FSA_DBG_TYPE_MODE:
		value = fsa4480_get_switch_mode();
		mode = switch_status_string[value];
		ret_size += sprintf(buf, "%s: %d \n", mode, value);
		break;

	case FSA_DBG_REG_MODE:
		for (i = 0; i <= FSA4480_CURRENT_SOURCE; i++) {
			regmap_read(global_fsa4480_data->regmap, i, &value);
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
		case FSA_DBG_TYPE_MODE:
			fsa4480_switch_mode((enum SWITCH_STATUS)value);
			break;
		default:
			pr_warn("%s: invalid type %d\n", __func__, type);
			break;
	}
	return count;
}

#define fsa4480_DEVICE_SHOW(_name, _type) static ssize_t \
show_##_name(struct device *dev, \
			  struct device_attribute *attr, char *buf) \
{ \
	return sysfs_show(dev, attr, buf, _type); \
}

#define fsa4480_DEVICE_SET(_name, _type) static ssize_t \
set_##_name(struct device *dev, \
			 struct device_attribute *attr, \
			 const char *buf, size_t count) \
{ \
	return sysfs_set(dev, attr, buf, count, _type); \
}

#define fsa4480_DEVICE_SHOW_SET(name, type) \
fsa4480_DEVICE_SHOW(name, type) \
fsa4480_DEVICE_SET(name, type) \
static DEVICE_ATTR(name, S_IWUSR | S_IRUGO, show_##name, set_##name);

fsa4480_DEVICE_SHOW_SET(fsa4480_switch_mode, FSA_DBG_TYPE_MODE);
fsa4480_DEVICE_SHOW_SET(fsa4480_reg, FSA_DBG_REG_MODE);

static struct attribute *fsa4480_attrs[] = {
	&dev_attr_fsa4480_switch_mode.attr,
	&dev_attr_fsa4480_reg.attr,
	NULL
};

static const struct attribute_group fsa4480_group = {
	.attrs = fsa4480_attrs,
};
/* end of info node */

static int fsa4480_gpio_pinctrl_init(struct fsa4480_priv *fsa_priv)
{
	int retval = 0;

	pr_info("%s \n", __func__);

	fsa_priv->uart_en_gpio_pinctrl = devm_pinctrl_get(fsa_priv->dev);
	if (IS_ERR_OR_NULL(fsa_priv->uart_en_gpio_pinctrl)) {
		retval = PTR_ERR(fsa_priv->uart_en_gpio_pinctrl);
		goto gpio_err_handle;
	}

	fsa_priv->pinctrl_state_disable
		= pinctrl_lookup_state(fsa_priv->uart_en_gpio_pinctrl, "uart_disable");
	if (IS_ERR_OR_NULL(fsa_priv->pinctrl_state_disable)) {
		retval = PTR_ERR(fsa_priv->pinctrl_state_disable);
		goto gpio_err_handle;
	}

	fsa_priv->pinctrl_state_enable
		= pinctrl_lookup_state(fsa_priv->uart_en_gpio_pinctrl, "uart_enable");
	if (IS_ERR_OR_NULL(fsa_priv->pinctrl_state_enable)) {
		retval = PTR_ERR(fsa_priv->pinctrl_state_enable);
		goto gpio_err_handle;
	}

	return 0;

gpio_err_handle:
	pr_info("%s: init failed \n", __func__);
	devm_pinctrl_put(fsa_priv->uart_en_gpio_pinctrl);
	fsa_priv->uart_en_gpio_pinctrl = NULL;

	return retval;
}

static void fsa4480_enable_switch_handler(unsigned long data)
{
	int ret = 0;

	ret = queue_work(fsa4480_enable_switch_workqueue, &fsa4480_enable_switch_work);
	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

static void fsa4480_enable_switch_work_callback(struct work_struct *work)
{
	u32 int_status = 0;
	u32 jack_status = 0;
	u32 switch_control = 0;
	u32 switch_setting = 0;
	pr_info("%s()\n", __func__);
	regmap_read(global_fsa4480_data->regmap, FSA4480_DETECTION_INT, &int_status);
	regmap_read(global_fsa4480_data->regmap, FSA4480_JACK_STATUS, &jack_status);
	if (int_status & (1 << 2)) {
		if (jack_status & (1 << 1)) {
			pr_info("%s: 3-pole detect\n", __func__);
			regmap_read(global_fsa4480_data->regmap, FSA4480_SWITCH_SETTINGS, &switch_setting);
			regmap_read(global_fsa4480_data->regmap, FSA4480_SWITCH_CONTROL, &switch_control);

			/* Set Bit 1 to 0, Enable Mic <---> SBU2 */
			regmap_write(global_fsa4480_data->regmap, FSA4480_SWITCH_CONTROL, switch_control & (~(1 << 1)));
			/* FSA4480 chip hardware requirement */
			usleep_range(50, 55);
			/* Enable Bit 1, Enable Mic <---> SBU2 Switch */
			regmap_write(global_fsa4480_data->regmap, FSA4480_SWITCH_SETTINGS, switch_setting | (1 << 1));
		}
	}
}

static void delay_init_timer_callback(unsigned long data)
{
	int ret = 0;

	ret = queue_work(fsa4480_delay_init_workqueue, &fsa4480_delay_init_work);
	pr_info("%s \n", __func__);

	if (!ret)
		pr_info("%s, queue work return: %d!\n", __func__, ret);
}

static void fsa4480_delay_init_work_callback(struct work_struct *work)
{
#if USE_TCPC_NOTIFIER
	int rc = 0;
	pr_info("%s() \n", __func__);

	if (global_fsa4480_data && global_fsa4480_data->tcpc_dev) {
		/* check tcpc status at startup */
		if (TYPEC_ATTACHED_AUDIO == tcpm_inquire_typec_attach_state(global_fsa4480_data->tcpc_dev)) {
			/* Audio Plug in */
			pr_info("%s: Audio is Plug In status at startup\n", __func__);
			pinctrl_select_state(global_fsa4480_data->uart_en_gpio_pinctrl,
						global_fsa4480_data->pinctrl_state_disable);
			regmap_write(global_fsa4480_data->regmap, FSA4480_SWITCH_SETTINGS, 0x9F);
			regmap_write(global_fsa4480_data->regmap, FSA4480_SWITCH_CONTROL, 0x00);
			regmap_write(global_fsa4480_data->regmap, FSA4480_FUNCTION_ENABLE, 0x49);
			mod_timer(&fsa4480_enable_timer, jiffies + (int)(0.2 * HZ));
			accdet_eint_callback_wrapper(1);
		}

		/* register tcpc_event */
		global_fsa4480_data->psy_nb.notifier_call = fsa4480_tcpc_event_changed;
		global_fsa4480_data->psy_nb.priority = 0;
		rc = register_tcp_dev_notifier(global_fsa4480_data->tcpc_dev, &global_fsa4480_data->psy_nb, TCP_NOTIFY_TYPE_USB);
		if (rc) {
			pr_err("%s: register_tcp_dev_notifier failed\n", __func__);
		}
	}
#endif
}

static int fsa4480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct fsa4480_priv *fsa_priv;
	int rc = 0;

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	global_fsa4480_data = fsa_priv; // add for debug
	fsa_priv->dev = &i2c->dev;

	fsa_priv->usb_psy = power_supply_get_by_name("usb");
	if (!fsa_priv->usb_psy) {
		rc = -EPROBE_DEFER;
		dev_dbg(fsa_priv->dev,
			"%s: could not get USB psy info: %d\n",
			__func__, rc);
		goto err_data;
	}

	fsa_priv->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!fsa_priv->tcpc_dev) {
		rc = -EPROBE_DEFER;
		pr_err("%s get tcpc device type_c_port0 fail \n", __func__);
		goto err_data;
	}

	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
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

	fsa4480_gpio_pinctrl_init(fsa_priv);
	if (fsa_priv->uart_en_gpio_pinctrl) {
		rc = pinctrl_select_state(fsa_priv->uart_en_gpio_pinctrl,
					fsa_priv->pinctrl_state_enable);
		if (rc < 0) {
			pr_err("Failed to select enable pinstate %d\n", rc);
		}
	}

	fsa4480_update_reg_defaults(fsa_priv->regmap);

#if USE_POWER_SUPPLY_NOTIFIER
	fsa_priv->psy_nb.notifier_call = fsa4480_usbc_event_changed;
	fsa_priv->psy_nb.priority = 0;
	rc = power_supply_reg_notifier(&fsa_priv->psy_nb);
	if (rc) {
		dev_err(fsa_priv->dev, "%s: power supply reg failed: %d\n",
			__func__, rc);
		goto err_supply;
	}
#endif

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

	fsa_priv->fsa4480_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((fsa_priv->fsa4480_notifier).rwsem);
	fsa_priv->fsa4480_notifier.head = NULL;

	rc = sysfs_create_group(&i2c->dev.kobj, &fsa4480_group);
	if (rc) {
		pr_err("%s: create attr error %d\n", __func__, rc);
	}

	fsa4480_enable_switch_workqueue = create_singlethread_workqueue("enableSwitchQueue");
	INIT_WORK(&fsa4480_enable_switch_work, fsa4480_enable_switch_work_callback);
	if (!fsa4480_enable_switch_workqueue) {
		rc = -1;
		pr_notice("%s create fsa4480_enable_switch workqueue fail.\n", __func__);
		goto err_data;
	}

	pr_info("%s(), setup enable timer", __func__);
	setup_timer(&fsa4480_enable_timer, fsa4480_enable_switch_handler, (unsigned long)fsa_priv);

	fsa4480_delay_init_workqueue = create_singlethread_workqueue("delayInitQueue");
	INIT_WORK(&fsa4480_delay_init_work, fsa4480_delay_init_work_callback);

	/* delay 2s to register tcpc event change, after accdet init done */
	setup_timer(&fsa4480_delay_init_timer, delay_init_timer_callback, 0);
	mod_timer(&fsa4480_delay_init_timer, jiffies + FSA4480_DELAY_INIT_TIME);

	return 0;

err_supply:
#if USE_POWER_SUPPLY_NOTIFIER
	power_supply_put(fsa_priv->usb_psy);
#endif
err_data:
	devm_kfree(&i2c->dev, fsa_priv);
	return rc;
}

static int fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);

#if USE_POWER_SUPPLY_NOTIFIER
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	/* deregister from PMI */
	power_supply_unreg_notifier(&fsa_priv->psy_nb);
	power_supply_put(fsa_priv->usb_psy);
#endif

	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{
		.compatible = "mediatek,fsa4480-audioswitch",
	},
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
};

static int __init fsa4480_init(void)
{
	int rc;

	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
late_initcall(fsa4480_init);

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL v2");
