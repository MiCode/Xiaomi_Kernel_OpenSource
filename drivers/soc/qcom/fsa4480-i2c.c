// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/usb/typec.h>
#include <linux/usb/ucsi_glink.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <linux/qti-regmap-debugfs.h>

#define FSA4480_I2C_NAME	"fsa4480-driver"
/* ADD: AudioSwitch_Bringup */
#define CONFIG_LX_AUDIO_INFO    1
#define CONFIG_ADD_DBG_INFO_NODE   1
#define AUDIO_INFO_MAX_LEN      (64)

#define FSA4480_DEV_ID          0x00
#define HL5281M_ID1_VALUE       0x40
#define HL5281M_ID2_VALUE       0x41
#define HL5281M_ID3_VALUE       0x48
#define DIO4480_ID_VALUE        0xF1
#define DIO4485_ID_VALUE        0xF6
/* END AudioSwitch_Bringup */
#define FSA4480_SWITCH_SETTINGS 0x04
#define FSA4480_SWITCH_CONTROL  0x05
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
#define FSA4480_FUNC_ENABLE     0x12 /* ADD: AudioSwitch_Bringup */
#define FSA4480_RESET           0x1E

#if IS_ENABLED(CONFIG_RUST_DETECTION)
#define FSA4480_RES_DET_PIN       0x31
#define FSA4480_RES_SELECT_MODE   0x13
#define FSA4480_RES_DET_THRESHOLD 0x15
#define FSA4480_RES_DET_VAL       0x14
#define FSA4480_RES_DET_INTERVAL  0x16
#define FSA4480_DETECTION_INT     0x18
#define FSA4480_DET_V_TH          0x32
#define FSA4480_DETM_V_CCIN       0x38
#define FSA4480_DETM_V_SBU1       0x3B
#define FSA4480_DETM_V_SBU2       0x3C
#define FSA4480_V_MULTIPLE        9375 //二供转换电压倍数,(val*9375)/1000
#define FSA4480_V_MULTIPLE_2      1000
#define FSA4480_V_CCIN_TH         1000 //1.85V
#define FSA4480_V_SBU1_TH         1000 //1.85V
#define FSA4480_V_SBU2_TH         1000 //1.85V
#endif

/* ADD: AudioSwitch_Bringup */
#define DIO4485_MIC_DET_STATUS  0x17
#define DIO4485_DELAY_T_SETTING 0x21
#define DIO4485_FREQ1_SETTING   0x4E
#define DIO4485_FREQ2_SETTING   0x50
#define DIO4485_FREQ3_SETTING   0x51

#if defined(CONFIG_ADD_DBG_INFO_NODE)
#define DIO4485_REG_RANGE       0x51
#define FSA4480_DBG_TYPE_MODE      0
#define FSA4480_DBG_REG_MODE       1

enum SWITCH_STATUS {
	SWITCH_STATUS_INVALID = 0,
	SWITCH_STATUS_NOT_CONNECTED = 1,
	SWITCH_STATUS_USB_MODE = 2,
	SWITCH_STATUS_HEADSET_MODE = 3,
	SWITCH_STATUS_MAX
};

char *switch_status_string[SWITCH_STATUS_MAX] = {
	"switch invalid",
	"switch not connected",
	"switch usb mode",
	"switch headset mode",
};
#endif
/* END AudioSwitch_Bringup */

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct notifier_block ucsi_nb;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_notifier;
	struct mutex notification_lock;
        /* ADD: AudioSwitch_Bringup */
        u32 dev_id;
        /* END AudioSwitch_Bringup */
};

/* ADD: AudioSwitch_Bringup */
#if defined(CONFIG_ADD_DBG_INFO_NODE)
static struct fsa4480_priv *global_fsa4480_data = NULL;
#endif
/* END AudioSwitch_Bringup */

struct fsa4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	.max_register = FSA4480_DETM_V_SBU2, //修改max寄存器值，不作修改的话如果注册的值超过这个值就不允许写入
#else
	.max_register = FSA4480_RESET,
#endif
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
	{FSA4480_DELAY_L_MIC, 0x00},
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x09},
	{FSA4480_FUNC_ENABLE, 0x10},
	{FSA4480_SWITCH_SETTINGS, 0x98},
	{FSA4480_FUNC_ENABLE, 0x10 },
};

/* ADD: AudioSwitch_Bringup */
#ifdef CONFIG_LX_AUDIO_INFO
static char g_audio_switch[AUDIO_INFO_MAX_LEN];
int fsa4480_get_name(char* audio_switch)
{
	strncpy(audio_switch, g_audio_switch, strlen(g_audio_switch));
	return 0;
}
EXPORT_SYMBOL(fsa4480_get_name);
#endif
/* END AudioSwitch_Bringup */

static int g_fsa4480_usbhs_state;
int fsa4480_flip_event(int* fsa4480_usbhs_state)
{
	*fsa4480_usbhs_state = g_fsa4480_usbhs_state;
	return 0;
}
EXPORT_SYMBOL(fsa4480_flip_event);

struct fsa4480_reg_val fsa_initial_settings[] = {
	{ FSA4480_RESET, 0x01 },
};
struct fsa4480_reg_val fsa_freq_settings[] = {
	{ DIO4485_FREQ1_SETTING, 0x8F },
	{ DIO4485_FREQ1_SETTING, 0x5A },
	{ DIO4485_FREQ3_SETTING, 0x90 },
	{ DIO4485_FREQ2_SETTING, 0x44 },
};

struct fsa4480_reg_val fsa_delay_settings[] = {
	{ FSA4480_DELAY_L_SENSE, 0x00 },
	{ FSA4480_DELAY_L_AGND, 0x00 },
	{ FSA4480_DELAY_L_MIC, 0x1F },
	{ FSA4480_DELAY_L_R, 0x2F },
	{ DIO4485_DELAY_T_SETTING, 0x2F },
};

struct fsa4480_reg_val fsa_enable_settings[] = {
	{ FSA4480_FUNC_ENABLE, 0x18 },
};

static int apply_reg_settings(struct regmap *regmap, struct fsa4480_reg_val *settings, size_t count)
{
	int rc = 0;
	for (size_t i = 0; i < count; i++) {
		rc = regmap_write(regmap, settings[i].reg, settings[i].val);
		if (rc) {
			pr_err("[%s]  write regster addr: [%x] fail.\n",__func__, settings[i].reg);
			break;
		}
	}
	return 0;
}

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	u32 prev_control, prev_enable, cur_control, micdet_status; /* ADD: AudioSwitch_Bringup */
	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, &prev_control);
	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &prev_enable);
	if (prev_control == switch_control && prev_enable == switch_enable) {
		dev_dbg(fsa_priv->dev, "%s: settings unchanged\n", __func__);
		return;
	}

        /* ADD: AudioSwitch_Bringup */
	if (switch_enable == 0x98) {
		regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0x18);
		/* FSA4480 chip hardware requirement */
		usleep_range(50, 55);
		regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x98);
		usleep_range(50, 55);
		g_fsa4480_usbhs_state = 0;
	} else if (switch_enable == 0x9F) {
		if (prev_control == 0x18) {
			if (DIO4485_ID_VALUE == fsa_priv->dev_id) {
				//slow turn on start
				regmap_write(fsa_priv->regmap, 0x08, 0xaf);
				regmap_write(fsa_priv->regmap, 0x09, 0xaf);
				regmap_write(fsa_priv->regmap, 0x0A, 0xaf);
				regmap_write(fsa_priv->regmap, 0x0B, 0xaf);
				regmap_write(fsa_priv->regmap, 0x0C, 0xaf);
				//delay config start
				regmap_write(fsa_priv->regmap, 0x0F, 0x00);
				regmap_write(fsa_priv->regmap, 0x10, 0x00);
				regmap_write(fsa_priv->regmap, 0x0E, 0x1f);
				regmap_write(fsa_priv->regmap, 0x0D, 0x2f);
				regmap_write(fsa_priv->regmap, 0x21, 0x2f);
				regmap_write(fsa_priv->regmap, 0x1C, 0x05);
				//function enable
				regmap_write(fsa_priv->regmap, 0x04, 0x9F);
				regmap_write(fsa_priv->regmap, 0x05, 0x00);
				regmap_write(fsa_priv->regmap, 0x12, 0x59);
				usleep_range(500000, 501000);
			} else if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id)) {
				regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
				regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
				regmap_write(fsa_priv->regmap, FSA4480_FUNC_ENABLE, 0x59);
				usleep_range(50000, 51000);
			}
		} else if (prev_control == 0x7) {
			dev_info(fsa_priv->dev, "%s: Reverse insertion settings unchanged\n", __func__);
		}

            regmap_read(fsa_priv->regmap, DIO4485_MIC_DET_STATUS, &micdet_status);
		if (DIO4485_ID_VALUE == fsa_priv->dev_id) {
			if (micdet_status == 0x1) {
				dev_err(fsa_priv->dev,"%s: The headset is not detected. Please detect it again.\n",__func__);
				if (switch_control == 0x0) {
					apply_reg_settings(fsa_priv->regmap, fsa_freq_settings, ARRAY_SIZE(fsa_freq_settings));
					usleep_range(5000, 5500);
					apply_reg_settings(fsa_priv->regmap, fsa_delay_settings, ARRAY_SIZE(fsa_delay_settings));
					apply_reg_settings(fsa_priv->regmap, fsa_enable_settings, ARRAY_SIZE(fsa_enable_settings));
				}
				regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
				regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
				usleep_range(5000, 5500);
			}
		} else if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id)) {
				if (micdet_status == 0x2) {
					regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
				}
		}
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, &cur_control);
#ifdef CONFIG_LX_AUDIO_INFO
		if (cur_control == 0x0) {
			g_fsa4480_usbhs_state = 1;
		} else if (cur_control == 0x7) {
			g_fsa4480_usbhs_state = 2;
		}
#endif
	}
	/* END AudioSwitch_Bringup */
}

static int fsa4480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, ucsi_nb);
	struct device *dev;
	enum typec_accessory acc = ((struct ucsi_glink_constat_info *)ptr)->acc;

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	dev_dbg(dev, "%s: USB change event received, supply mode %d, usbc mode %d, expected %d\n",
			__func__, acc, fsa_priv->usbc_mode.counter,
			TYPEC_ACCESSORY_AUDIO);

	switch (acc) {
	case TYPEC_ACCESSORY_AUDIO:
	case TYPEC_ACCESSORY_NONE:
		if (atomic_read(&(fsa_priv->usbc_mode)) == acc)
			break; /* filter notifications received before */
		atomic_set(&(fsa_priv->usbc_mode), acc);

		dev_dbg(dev, "%s: queueing usbc_analog_work\n",
			__func__);
		pm_stay_awake(fsa_priv->dev);
		queue_work(system_freezable_wq, &fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}

	return 0;
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	int rc = 0;
	int mode;
	struct device *dev;

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode again within locked context */
	mode = atomic_read(&(fsa_priv->usbc_mode));

	dev_dbg(dev, "%s: setting GPIOs active = %d\n",
		__func__, mode != TYPEC_ACCESSORY_NONE);

	switch (mode) {
	/* add all modes FSA should notify for in here */
	case TYPEC_ACCESSORY_AUDIO:
		/* ADD: AudioSwitch_Bringup */
		if (DIO4485_ID_VALUE == fsa_priv->dev_id) {
			apply_reg_settings(fsa_priv->regmap, fsa_freq_settings, ARRAY_SIZE(fsa_freq_settings));
			apply_reg_settings(fsa_priv->regmap, fsa_delay_settings, ARRAY_SIZE(fsa_delay_settings));
		}
		/* END AudioSwitch_Bringup */
		/* activate switches */
		fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);

		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
					     mode, NULL);
		break;
	case TYPEC_ACCESSORY_NONE:
		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
				TYPEC_ACCESSORY_NONE, NULL);

		/* deactivate switches */
		/* ADD: AudioSwitch_Bringup */
		if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id)) {
			regmap_write(fsa_priv->regmap, FSA4480_FUNC_ENABLE, 0x58);
		}
		/* END AudioSwitch_Bringup */
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}
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

	dev_dbg(fsa_priv->dev, "%s: registered notifier for %s\n",
		__func__, node->name);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	if (atomic_read(&(fsa_priv->usbc_mode)) == TYPEC_ACCESSORY_AUDIO) {
		dev_dbg(fsa_priv->dev, "%s: analog adapter already inserted\n",
			__func__);
		rc = fsa4480_usbc_analog_setup_switches(fsa_priv);
	}
	return rc;
}
EXPORT_SYMBOL_GPL(fsa4480_reg_notifier);

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
EXPORT_SYMBOL_GPL(fsa4480_unreg_notifier);

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

	/* ADD: AudioSwitch_Bringup */
	dev_dbg(fsa_priv->dev, "%s: event %d \n", __func__, event);
	/* END AudioSwitch_Bringup */

	switch (event) {
	case FSA_MIC_GND_SWAP:
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		return 1; /* ADD: AudioSwitch_Bringup */
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
EXPORT_SYMBOL_GPL(fsa4480_switch_event);

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

/* ADD: AudioSwitch_Bringup */
#if defined(CONFIG_ADD_DBG_INFO_NODE)
enum SWITCH_STATUS fsa4480_get_switch_mode(void)
{
	uint val = 0;
	enum SWITCH_STATUS state = SWITCH_STATUS_INVALID;
	regmap_read(global_fsa4480_data->regmap, FSA4480_SWITCH_CONTROL, &val);
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
	enum typec_accessory acc = TYPEC_ACCESSORY_NONE;

	dev_info(global_fsa4480_data->dev, "%s: %d %d\n", __func__, val, SWITCH_STATUS_HEADSET_MODE);
	if (val == SWITCH_STATUS_HEADSET_MODE) {
		acc = TYPEC_ACCESSORY_AUDIO;
	} else if (val == SWITCH_STATUS_USB_MODE) {
		acc = TYPEC_ACCESSORY_NONE;
	}

	mutex_lock(&global_fsa4480_data->notification_lock);
	if (atomic_read(&(global_fsa4480_data->usbc_mode)) == acc)
		goto done; /* filter notifications received before */

	atomic_set(&(global_fsa4480_data->usbc_mode), acc);

	dev_dbg(global_fsa4480_data->dev, "%s: %d\n", __func__, acc);
	switch (acc) {
	/* add all modes FSA should notify for in here */
	case TYPEC_ACCESSORY_AUDIO:
		if (DIO4485_ID_VALUE == global_fsa4480_data->dev_id) {
			regmap_write(global_fsa4480_data->regmap, FSA4480_RESET, 0x01);
			msleep(5);
			regmap_write(global_fsa4480_data->regmap, DIO4485_FREQ1_SETTING, 0x8F);
			regmap_write(global_fsa4480_data->regmap, DIO4485_FREQ1_SETTING, 0x5A);
			regmap_write(global_fsa4480_data->regmap, DIO4485_FREQ3_SETTING, 0x90);
			regmap_write(global_fsa4480_data->regmap, DIO4485_FREQ2_SETTING, 0x45);
			regmap_write(global_fsa4480_data->regmap, FSA4480_DELAY_L_SENSE, 0x00);
			regmap_write(global_fsa4480_data->regmap, FSA4480_DELAY_L_AGND, 0x00);
			regmap_write(global_fsa4480_data->regmap, FSA4480_DELAY_L_MIC, 0x1F);
			regmap_write(global_fsa4480_data->regmap, FSA4480_DELAY_L_R, 0x2F);
			regmap_write(global_fsa4480_data->regmap, DIO4485_DELAY_T_SETTING, 0x2F);
		}
		/* activate switches */
		fsa4480_usbc_update_settings(global_fsa4480_data, 0x00, 0x9F); // switch to headset
		/* notify call chain on event */
		blocking_notifier_call_chain(&global_fsa4480_data->fsa4480_notifier, acc, NULL);
		break;
	case TYPEC_ACCESSORY_NONE:
		/* notify call chain on event */
		blocking_notifier_call_chain(&global_fsa4480_data->fsa4480_notifier, TYPEC_ACCESSORY_NONE, NULL);
		/* deactivate switches */
		/* ADD: AudioSwitch_Bringup */
		if ((HL5281M_ID1_VALUE == global_fsa4480_data->dev_id) || (HL5281M_ID2_VALUE == global_fsa4480_data->dev_id) || (HL5281M_ID3_VALUE == global_fsa4480_data->dev_id)) {
			regmap_write(global_fsa4480_data->regmap, FSA4480_FUNC_ENABLE, 0x58);
		}
		/* END AudioSwitch_Bringup */
		fsa4480_usbc_update_settings(global_fsa4480_data, 0x18, 0x98); // switch to USB
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}
done:
	mutex_unlock(&global_fsa4480_data->notification_lock);
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
	enum typec_accessory acc = TYPEC_ACCESSORY_NONE;

	switch (type) {
	case FSA4480_DBG_TYPE_MODE:
		value = fsa4480_get_switch_mode();
		mode = switch_status_string[value];
		ret_size += sprintf(buf + ret_size, "%s: %d (2:usb, 3:headset)\n", mode, value);

		acc = atomic_read(&(global_fsa4480_data->usbc_mode));
		ret_size += sprintf(buf + ret_size, "%d (0:none, 1:audio) \n", value);
		break;
	case FSA4480_DBG_REG_MODE:
		for (i = 0; i <= DIO4485_REG_RANGE; i++) {
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
		pr_err("%s: get data of type %d failed\n", __func__, type);
		return err;
	}
	pr_info("%s: set type %d, data %ld\n", __func__, type, value);
	switch (type) {
		case FSA4480_DBG_TYPE_MODE:
			fsa4480_switch_mode((enum SWITCH_STATUS)value);
			break;
		default:
			pr_err("%s: invalid type %d\n", __func__, type);
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
fsa4480_DEVICE_SHOW_SET(switch_mode, FSA4480_DBG_TYPE_MODE);
fsa4480_DEVICE_SHOW_SET(reg, FSA4480_DBG_REG_MODE);
static struct attribute *fsa4480_attrs[] = {
	&dev_attr_switch_mode.attr,
	&dev_attr_reg.attr,
	NULL
};
static const struct attribute_group fsa4480_group = {
	.attrs = fsa4480_attrs,
};
#endif
/* END AudioSwitch_Bringup */


#if IS_ENABLED(CONFIG_RUST_DETECTION)
int rust_detection_workfunc_open(void)
{
	struct fsa4480_priv *fsa_priv = global_fsa4480_data;
	int reg_val_enable = 0, reg_val_pin = 0, reg_val_interval = 0, reg_val_select_mode = 0;
	int reg_val_ccin = 0, reg_val_sbu1 = 0, reg_val_sbu2 = 0, reg_val_vh = 0;
	int val_result = 0, reg_val_det_int = 0, count_retry = 10, ret = 0;
	struct device *dev = NULL;
    pr_err("%s:  lpd check is start\n", __func__);
	if (!fsa_priv) {
		pr_err("%s: [UST DET] Failed because fsa4480_data is NULL\n", __func__);
		return -EINVAL;
	}

	dev = fsa_priv->dev;
	if (!dev) {
		pr_err("%s: [UST DET] dev is NULL\n", __func__);
		return -EINVAL;
	}
        if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id) || (DIO4485_ID_VALUE == fsa_priv->dev_id)) {
		dev_err(dev, "%s: dev_id = 0x%x\n", __func__, fsa_priv->dev_id);
	} else {
		dev_err(dev, "%s: dev_id not matched\n", __func__);
		return -EINVAL;
	}


	ret |= regmap_read(fsa_priv->regmap, FSA4480_RES_DET_INTERVAL, &reg_val_interval);
	ret |= regmap_write(fsa_priv->regmap, FSA4480_RES_DET_INTERVAL, (0x4 | reg_val_interval)); //选择multi-pin mode， bit<2> = 1

	ret |= regmap_read(fsa_priv->regmap, FSA4480_RES_SELECT_MODE, &reg_val_select_mode);
	ret |= regmap_write(fsa_priv->regmap, FSA4480_RES_SELECT_MODE, (0x8 | reg_val_select_mode)); //选择电压模式，bit<3> = 1

	ret |= regmap_read(fsa_priv->regmap, FSA4480_RES_DET_PIN, &reg_val_pin);
	ret |= regmap_write(fsa_priv->regmap, FSA4480_RES_DET_PIN, (0x13 | reg_val_pin)); //选择需要检测的pin，bit<4>,bit<1>,bit<0> = 1

	ret |= regmap_write(fsa_priv->regmap, FSA4480_DET_V_TH, 0xD0); //阈值设为最大，所有pin都会检测

	ret |= regmap_read(fsa_priv->regmap, FSA4480_DETECTION_INT, &reg_val_det_int); //0x18 清除状态

	ret |= regmap_read(fsa_priv->regmap, FSA4480_FUNC_ENABLE, &reg_val_enable);
	ret |= regmap_write(fsa_priv->regmap, FSA4480_FUNC_ENABLE, (0x2 | reg_val_enable)); //打开检测使能，bit<1> = 1

	msleep(50);
	while (count_retry--) {
		ret = regmap_read(fsa_priv->regmap, FSA4480_DETECTION_INT, &reg_val_det_int);
		dev_err(dev, "%s: FSA4480_DETECTION_INT, ret = %d\n", __func__, reg_val_det_int);
		if (ret) {
			dev_err(dev, "%s: detection not complete, reg_val_det_int = 0x%x, count = %d\n", __func__, reg_val_det_int, count_retry);
			if (count_retry == 0) {
				return ret;
			}
		}
        
		if (reg_val_det_int & 0x1) {
			break;
		}

		msleep(5);
	}
	regmap_read(fsa_priv->regmap, FSA4480_DET_V_TH, &reg_val_vh); //0x32的值
	dev_err(dev, "%s: FSA4480_DET_V_TH, ret = %d\n", __func__, reg_val_vh);
	ret |= regmap_read(fsa_priv->regmap, FSA4480_DETM_V_CCIN, &reg_val_ccin); //0x38的值
	ret |= regmap_read(fsa_priv->regmap, FSA4480_DETM_V_SBU1, &reg_val_sbu1); //0x3B的值
	ret |= regmap_read(fsa_priv->regmap, FSA4480_DETM_V_SBU2, &reg_val_sbu2); //0x3C的值
	if (ret) {
		dev_err(dev, "%s: get reg failed, ret = %d\n", __func__, ret);
		return -EINVAL;
	}
	dev_err(dev, "%s: params check, dev_id = 0x%x, ccin=%d, sbu1=%d, sbu2=%d\n", __func__, fsa_priv->dev_id, reg_val_ccin, reg_val_sbu1, reg_val_sbu2);
	if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id)) {
		if (((((reg_val_ccin + 1) * FSA4480_V_MULTIPLE) / FSA4480_V_MULTIPLE_2) > FSA4480_V_CCIN_TH)
				&& (((((reg_val_sbu1 + 1) * FSA4480_V_MULTIPLE) / FSA4480_V_MULTIPLE_2) > FSA4480_V_SBU1_TH)
				|| ((((reg_val_sbu2 + 1) * FSA4480_V_MULTIPLE) / FSA4480_V_MULTIPLE_2) > FSA4480_V_SBU2_TH))) {
			dev_dbg(dev, "%s: HL5281M Moisture occur\n", __func__);
			val_result = 1;
			return val_result;
		}
	} else if (DIO4485_ID_VALUE == fsa_priv->dev_id) {
		if (((reg_val_ccin * 9) > FSA4480_V_CCIN_TH)
				&& (((reg_val_sbu1 * 9) > FSA4480_V_SBU1_TH)
				|| ((reg_val_sbu2 * 9) > FSA4480_V_SBU2_TH))) {
			dev_dbg(dev, "%s: DIO4485 Moisture occur\n", __func__);
			val_result = 1;
			return val_result;
		}
	}

	dev_err(dev, "%s: Moisture doesn't occur\n", __func__);

	return val_result;
}
EXPORT_SYMBOL_GPL(rust_detection_workfunc_open);

int rust_detection_workfunc_close(void)
{
	struct fsa4480_priv *fsa_priv = global_fsa4480_data;
	int reg_val_enable = 0, ret = 0;
	struct device *dev = NULL;
    pr_err("%s: [UST DET] \n", __func__);
	if (!fsa_priv) {
		pr_err("%s: [UST DET] Failed because fsa4480_data is NULL\n", __func__);
		return -EINVAL;
	}

	dev = fsa_priv->dev;
	if (!dev) {
		pr_err("%s: [UST DET] dev is NULL\n", __func__);
		return -EINVAL;
	}
	if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id) || (DIO4485_ID_VALUE == fsa_priv->dev_id)) {
		dev_dbg(dev, "%s: dev_id = 0x%x\n", __func__, fsa_priv->dev_id);
	} else {
		dev_err(dev, "%s: dev_id not matched\n", __func__);
		return -EINVAL;
	}


	ret |= regmap_read(fsa_priv->regmap, FSA4480_FUNC_ENABLE, &reg_val_enable);
	ret |= regmap_write(fsa_priv->regmap, FSA4480_FUNC_ENABLE, (0xFD & reg_val_enable)); //关闭检测使能，bit<1> = 0

	if (ret) {
		dev_err(dev, "%s: get reg failed, ret = %d\n", __func__, ret);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: Moisture detection off\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(rust_detection_workfunc_close);
#endif
static int fsa4480_probe(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv;
	int rc = 0;
	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	fsa_priv->dev = &i2c->dev;

	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_data;
	}

	fsa4480_update_reg_defaults(fsa_priv->regmap);
	devm_regmap_qti_debugfs_register(fsa_priv->dev, fsa_priv->regmap);

	fsa_priv->ucsi_nb.notifier_call = fsa4480_usbc_event_changed;
	fsa_priv->ucsi_nb.priority = 0;
	rc = register_ucsi_glink_notifier(&fsa_priv->ucsi_nb);
	if (rc) {
		dev_err(fsa_priv->dev, "%s: ucsi glink notifier registration failed: %d\n",
			__func__, rc);
		goto err_data;
	}

        /* ADD: AudioSwitch_Bringup */
	rc = regmap_read(fsa_priv->regmap, FSA4480_DEV_ID, &fsa_priv->dev_id);
#ifdef CONFIG_LX_AUDIO_INFO
	strcpy(g_audio_switch, dev_name(&i2c->dev));
	if (rc) {
		strcat(g_audio_switch, "-fail");
	} else {
		if ((HL5281M_ID1_VALUE == fsa_priv->dev_id) || (HL5281M_ID2_VALUE == fsa_priv->dev_id) || (HL5281M_ID3_VALUE == fsa_priv->dev_id)) {
			strcat(g_audio_switch, "-HL5281M");
		} else if (DIO4485_ID_VALUE == fsa_priv->dev_id) {
			strcat(g_audio_switch, "-DIO4485");
		} else if (DIO4480_ID_VALUE == fsa_priv->dev_id) {
			strcat(g_audio_switch, "-DIO4480");
		} else {
			strcat(g_audio_switch, "-unknown");
		}
	}
	dev_err(fsa_priv->dev, "%s: %s 0x%x\n", __func__, g_audio_switch, fsa_priv->dev_id);
#endif

#if defined(CONFIG_ADD_DBG_INFO_NODE)
	global_fsa4480_data = fsa_priv;
	rc = sysfs_create_group(&i2c->dev.kobj, &fsa4480_group);
	if (rc) {
		dev_err(fsa_priv->dev, "%s: create attr error %d\n", __func__, rc);
	}
#endif
	/* END AudioSwitch_Bringup */

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

	BLOCKING_INIT_NOTIFIER_HEAD(&fsa_priv->fsa4480_notifier);

	return 0;

err_data:
	return rc;
}

static void fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return;

	unregister_ucsi_glink_notifier(&fsa_priv->ucsi_nb);
	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);
}

/* ADD: AudioSwitch_Bringup */
static void fsa4480_shutdown(struct i2c_client* i2c)
{
	struct fsa4480_priv* fsa_priv = (struct fsa4480_priv*)i2c_get_clientdata(i2c);
	if (!fsa_priv)
		return;
	pr_info("%s poweroff reset switch.", __func__);
	apply_reg_settings(fsa_priv->regmap, fsa_initial_settings, ARRAY_SIZE(fsa_initial_settings));
}
/* END AudioSwitch_Bringup */

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{
		.compatible = "qcom,fsa4480-i2c",
	},
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
	.shutdown = fsa4480_shutdown, /* ADD: AudioSwitch_Bringup */
};

static int __init fsa4480_init(void)
{
	int rc;
	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(fsa4480_init);

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL");
