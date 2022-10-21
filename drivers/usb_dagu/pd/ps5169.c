/*				*/
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include "ps5169.h"

#define PS5169_DRIVER_NAME	"ps5169"

static struct ps5169_info *g_info;

static const struct regmap_config ps5169_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
};

static int ps5169_write_reg(struct ps5169_info *info, u8 reg, u8 data)
{
	int ret = 0;

	if (!info->regmap)
		return ret;

	mutex_lock(&info->i2c_lock);
	ret = regmap_write(info->regmap, reg, data);
	if (ret < 0)
		pr_err("%s: failed-write, reg(0x%02X), ret(%d)\n",
				__func__, reg, ret);
	mutex_unlock(&info->i2c_lock);

	return ret;
}

static int ps5169_read_reg(struct ps5169_info *info, u8 reg, u8 *data)
{
	unsigned int temp;
	int ret;

	if (!info->regmap)
		return ret;

	mutex_lock(&info->i2c_lock);
	ret = regmap_read(info->regmap, reg, &temp);
	if (ret >= 0)
		*data = (u16)temp;
	mutex_unlock(&info->i2c_lock);

	return ret;
}

static int ps5169_update_reg(struct ps5169_info *info, u8 reg, u8 data)
{
	u8 temp;
	int ret = 0;

	ret = ps5169_write_reg(info, reg, data);

	ret = ps5169_read_reg(info, reg, &temp);

	if (data != (u8)temp) {
		pr_err("%s: update reg:%02x err, wdata:%02x, rdata:%02x.\n",
				__func__, reg, data, temp);
		return -1;
	}

	return 0;
}

static bool ps5169_present_check(struct ps5169_info *info)
{
	if (!info->present_flag) {
		pr_err("%s: present_flag false.\n", __func__);
		return false;
	}

	return true;
}

static int ps5169_get_chipid_revision(struct ps5169_info *info)
{
	u8 chip_id_l, chip_id_h;
	u8 revision_l, revision_h;
	int ret = 0;

	ret |= ps5169_read_reg(info, REG_CHIP_ID_L, &chip_id_l);
	ret |= ps5169_read_reg(info, REG_CHIP_ID_H, &chip_id_h);
	pr_info("%s: Chip_ID: 0x%02x, 0x%02x", __func__, chip_id_h, chip_id_l);

	ret |= ps5169_read_reg(info, REG_REVISION_L, &revision_l);
	ret |= ps5169_read_reg(info, REG_REVISION_L, &revision_l);
	pr_info("%s: Revision: 0x%02x, 0x%02x", __func__, revision_h, revision_l);

	if ((chip_id_h == 0x69) && (chip_id_l == 0x87))  {
		info->present_flag = true;
		pr_info("%s: present_flag: true.\n", __func__);
	} else {
		info->present_flag = false;
		pr_err("%s: chip id no match, return: %d.\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void ps5169_get_chipcfg_and_modeselection(struct ps5169_info *info)
{
	u8 cfg_mode, aux_enable, hpd_plug;
	int ret;

	if (!ps5169_present_check(info))
		return;

	ret = ps5169_read_reg(info, REG_CONFIG_MODE, &cfg_mode);
	ret = ps5169_read_reg(info, REG_AUX_ENABLE, &aux_enable);
	ret = ps5169_read_reg(info, REG_HPD_PLUG, &hpd_plug);

	pr_info("%s: Cfg:0x%02x, AUX:0x%02x, HPD:0x%02x.\n",
			__func__, cfg_mode, aux_enable, hpd_plug);
}

static void ps5169_set_config(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	ret |= ps5169_update_reg(info, 0x9d, 0x80);

	msleep(10);

	ret |= ps5169_update_reg(info, 0x9d, 0x00);

	ret |= ps5169_update_reg(info, 0x40, 0x80);		

	ret |= ps5169_update_reg(info, 0x04, 0x44);		

	ret |= ps5169_update_reg(info, 0xA0, 0x02);		


	ret |= ps5169_update_reg(info, 0x51, 0x87);

	ret |= ps5169_update_reg(info, 0x50, 0x20);

	ret |= ps5169_update_reg(info, 0x54, 0x11);

	ret |= ps5169_update_reg(info, 0x5d, 0x66);

	ret |= ps5169_update_reg(info, 0x52, 0x50);		

	ret |= ps5169_update_reg(info, 0x55, 0x00);

	ret |= ps5169_update_reg(info, 0x56, 0x00);

	ret |= ps5169_update_reg(info, 0x57, 0x00);

	ret |= ps5169_update_reg(info, 0x58, 0x00);

	ret |= ps5169_update_reg(info, 0x59, 0x00);

	ret |= ps5169_update_reg(info, 0x5a, 0x00);

	ret |= ps5169_update_reg(info, 0x5b, 0x00);

	ret |= ps5169_update_reg(info, 0x5e, 0x06);

	ret |= ps5169_update_reg(info, 0x5f, 0x00);

	ret |= ps5169_update_reg(info, 0x60, 0x00);

	ret |= ps5169_update_reg(info, 0x61, 0x03);

	ret |= ps5169_update_reg(info, 0x65, 0x40);

	ret |= ps5169_update_reg(info, 0x66, 0x00);

	ret |= ps5169_update_reg(info, 0x67, 0x03);

	ret |= ps5169_update_reg(info, 0x75, 0x0c);

	ret |= ps5169_update_reg(info, 0x77, 0x00);

	ret |= ps5169_update_reg(info, 0x78, 0x7c);

	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
	else
		pr_info("%s: cfg set end.\n", __func__);
}

static void ps5169_enable_work(struct work_struct *work)
{
	struct ps5169_info *info =
		container_of(work, struct ps5169_info, ps_en_work.work);

	int ret;

	if (info->ps_enable == info->pre_ps_enable) {
		pr_err("%s: enable same (%d), return.\n", __func__, info->ps_enable);
		return;
	} else {
		info->ps_enable = info->pre_ps_enable;
		pr_err("%s: enable: %d.\n", __func__, info->ps_enable);
	}

	if (info->ps_enable) {
		ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_active);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl active rc=%d\n", __func__, ret);
			return;
		}
		pr_info("%s: select gpio_active.\n", __func__);
		msleep(50);
		pr_info("%s: true, set cfg.\n", __func__);
		ps5169_set_config(info);
	} else {
		ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_suspend);
		if (ret < 0) {
			pr_err("%s: fail to select pinctrl suspend rc=%d\n", __func__, ret);
			return;
		}
		pr_info("%s: select gpio_suspend.\n", __func__);
	}
}

void ps5169_cfg_usb(void)
{
	int ret = 0;

	if (!ps5169_present_check(g_info))
		return;

	msleep(50);

	pr_info("%s: start.\n", __func__);
	if (g_info->flip == 1)
		ret |= ps5169_update_reg(g_info, 0x40, 0xc0);  
	else if (g_info->flip == 2)
		ret |= ps5169_update_reg(g_info, 0x40, 0xd0);    
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection(g_info);
}

static void ps5169_config_flip(struct ps5169_info *info, int flip)
{
	if (!ps5169_present_check(info))
		return;

	info->flip = flip;
	pr_info("%s: flip:%d.\n", __func__, info->flip);
}

static void ps5169_config_dp_only_mode(struct ps5169_info *info, int flip)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	pr_info("%s: flip:%d.\n", __func__, flip);
	if (flip == 1)
		ret |= ps5169_update_reg(info, 0x40, 0xa0);		
	else if (flip == 2)
		ret |= ps5169_update_reg(info, 0x40, 0xb0);		

	ret |= ps5169_update_reg(info, 0xa0, 0x00);		
	ret |= ps5169_update_reg(info, 0xa1, 0x04);		
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection(info);
}

static void ps5169_config_usb_dp_mode(struct ps5169_info *info, int flip)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	pr_info("%s: flip:%d.\n", __func__, flip);
	if (flip == 1)
		ret |= ps5169_update_reg(info, 0x40, 0xe0);		
	else if (flip == 2)
		ret |= ps5169_update_reg(info, 0x40, 0xf0);		

	ret |= ps5169_update_reg(info, 0xa0, 0x00);		
	ret |= ps5169_update_reg(info, 0xa1, 0x04);		
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection(info);
}

static void ps5169_remove_usb(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	info->flip = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		

	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static void ps5169_remove_dp(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	info->flip = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		
	ret |= ps5169_update_reg(info, 0xa0, 0x02);		
	ret |= ps5169_update_reg(info, 0xa1, 0x00);	
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static void ps5169_remove_usb_dp(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check(info))
		return;

	info->flip = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		
	ret |= ps5169_update_reg(info, 0xa0, 0x02);		
	ret |= ps5169_update_reg(info, 0xa1, 0x00);		
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}


/* For tuning usb3.0 eye */
static void ps5169_set_eq0_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data;

	if (!ps5169_present_check(info))
		return;

	if ((level > 7) || (level < 0)) {
		pr_err("%s: level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x8F;
	level_tmp = data | (level << 4);
	pr_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x50, level_tmp);
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq0_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level;

	if (!ps5169_present_check(info))
		return 0;

	ret |= ps5169_read_reg(info, 0x50, &data);

	level = (data & 0x70) >> 4;

	return level;
}

static void ps5169_set_eq1_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data;

	if (!ps5169_present_check(info))
		return;

	if ((level > 7) || (level < 0)) {
		pr_err("%s:level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x8F;
	level_tmp = data | (level << 4);
	pr_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x5D, level_tmp);
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq1_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level;

	if (!ps5169_present_check(info))
		return 0;

	ret |= ps5169_read_reg(info, 0x5D, &data);

	level = (data & 0x70) >> 4;

	return level;
}

static void ps5169_set_eq2_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data;

	if (!ps5169_present_check(info))
		return;

	if ((level > 15) || (level < 0)) {
		pr_err("%s: level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x0F;
	level_tmp = data | (level << 4);
	pr_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x54, level_tmp);
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq2_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level;

	if (!ps5169_present_check(info))
		return 0;

	ret |= ps5169_read_reg(info, 0x54, &data);

	level = (data & 0xf0) >> 4;

	return level;
}

static void ps5169_set_tx_gain(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data;

	if (!ps5169_present_check(info))
		return;

	if ((level > 1) || (level < 0)) {
		pr_err("%s:level err:%d.\n", __func__, level);
		return;
	}

	data &= 0xFE;
	level_tmp = data | level;
	pr_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x5C, level_tmp);
	if (ret < 0)
		pr_err("%s: crc err.\n", __func__);
}

static int ps5169_get_tx_gain(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level;

	if (!ps5169_present_check(info))
		return 0;

	ret |= ps5169_read_reg(info, 0x5C, &data);

	level = data & 0x01;

	return level;
}

static enum power_supply_property ps5169_props[] = {
	POWER_SUPPLY_PROP_PS_EN,
	POWER_SUPPLY_PROP_PS_CHIPID,
	POWER_SUPPLY_PROP_PS_CFGMOD,
	POWER_SUPPLY_PROP_PS_CFG_FLIP,
	POWER_SUPPLY_PROP_PS_CFG_USB,
	POWER_SUPPLY_PROP_PS_CFG_DP,
	POWER_SUPPLY_PROP_PS_CFG_USB_DP,
	POWER_SUPPLY_PROP_PS_RMOV_USB,
	POWER_SUPPLY_PROP_PS_RMOV_DP,
	POWER_SUPPLY_PROP_PS_RMOV_USB_DP,
	POWER_SUPPLY_PROP_EQ0_TX,
	POWER_SUPPLY_PROP_EQ1_TX,
	POWER_SUPPLY_PROP_EQ2_TX,
	POWER_SUPPLY_PROP_TX_GAIN,
};

static int ps5169_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct ps5169_info *info = power_supply_get_drvdata(psy);
	int rc = 0;
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PS_EN:
		val->intval = info->ps_enable;
		break;
	case POWER_SUPPLY_PROP_PS_CFG_FLIP:
		val->intval = info->flip;
		break;
	case POWER_SUPPLY_PROP_PS_CHIPID:
	case POWER_SUPPLY_PROP_PS_CFGMOD:
	case POWER_SUPPLY_PROP_PS_CFG_USB:
	case POWER_SUPPLY_PROP_PS_CFG_DP:
	case POWER_SUPPLY_PROP_PS_CFG_USB_DP:
	case POWER_SUPPLY_PROP_PS_RMOV_USB:
	case POWER_SUPPLY_PROP_PS_RMOV_DP:
	case POWER_SUPPLY_PROP_PS_RMOV_USB_DP:
		break;
	case POWER_SUPPLY_PROP_EQ0_TX:
		val->intval = ps5169_get_eq0_tx(info);
		break;
	case POWER_SUPPLY_PROP_EQ1_TX:
		val->intval = ps5169_get_eq1_tx(info);
		break;
	case POWER_SUPPLY_PROP_EQ2_TX:
		val->intval = ps5169_get_eq2_tx(info);
		break;
	case POWER_SUPPLY_PROP_TX_GAIN:
		val->intval = ps5169_get_tx_gain(info);
		break;
	default:
		pr_debug("get prop %d is not supported in ps5169\n", psp);
		rc = -EINVAL;
		break;
	}

	return 0;
}

static int ps5169_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct ps5169_info *info = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PS_EN:
		info->pre_ps_enable = val->intval;
		schedule_delayed_work(&info->ps_en_work, 0);
		break;
	case POWER_SUPPLY_PROP_PS_CFG_FLIP:
		ps5169_config_flip(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_PS_CFG_USB:
		break;
	case POWER_SUPPLY_PROP_PS_CFG_DP:
		ps5169_config_dp_only_mode(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_PS_CFG_USB_DP:
		ps5169_config_usb_dp_mode(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_PS_RMOV_USB:
		if (val->intval)
			ps5169_remove_usb(info);
		break;
	case POWER_SUPPLY_PROP_PS_RMOV_DP:
		if (val->intval)
			ps5169_remove_dp(info);
		break;
	case POWER_SUPPLY_PROP_PS_RMOV_USB_DP:
		if (val->intval)
			ps5169_remove_usb_dp(info);
		break;
	case POWER_SUPPLY_PROP_EQ0_TX:
		ps5169_set_eq0_tx(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_EQ1_TX:
		ps5169_set_eq1_tx(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_EQ2_TX:
		ps5169_set_eq2_tx(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_TX_GAIN:
		ps5169_set_tx_gain(info, val->intval);
		break;
	default:
		pr_debug("set ps5169 prop %d is not supported\n", psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int ps5169_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PS_EN:
	case POWER_SUPPLY_PROP_PS_CFG_FLIP:
	case POWER_SUPPLY_PROP_PS_CFG_USB:
	case POWER_SUPPLY_PROP_PS_CFG_DP:
	case POWER_SUPPLY_PROP_PS_CFG_USB_DP:
	case POWER_SUPPLY_PROP_PS_RMOV_USB:
	case POWER_SUPPLY_PROP_PS_RMOV_DP:
	case POWER_SUPPLY_PROP_PS_RMOV_USB_DP:
	case POWER_SUPPLY_PROP_EQ0_TX:
	case POWER_SUPPLY_PROP_EQ1_TX:
	case POWER_SUPPLY_PROP_EQ2_TX:
	case POWER_SUPPLY_PROP_TX_GAIN:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc ps_psy_desc = {
	.name = "ps5169",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = ps5169_props,
	.num_properties = ARRAY_SIZE(ps5169_props),
	.get_property = ps5169_get_prop,
	.set_property = ps5169_set_prop,
	.property_is_writeable = ps5169_prop_is_writeable,
};

static int ps5169_init_psy(struct ps5169_info *info)
{
	struct power_supply_config ps_cfg = {};

	info->ps_psy_desc = ps_psy_desc;
	ps_cfg.drv_data = info;
	ps_cfg.of_node = info->dev->of_node;
	info->ps_psy = devm_power_supply_register(info->dev,
			&info->ps_psy_desc,
			&ps_cfg);
	if (IS_ERR(info->ps_psy)) {
		pr_err("Couldn't register ps5169 power supply.\n");
		return PTR_ERR(info->ps_psy);
	}

	return 0;
}

static int ps5169_parse_dt(struct ps5169_info *info)
{
	struct device_node *node = info->dev->of_node;
	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	info->enable_gpio = of_get_named_gpio(node, "mi,ps5169-enable", 0);
	if ((!gpio_is_valid(info->enable_gpio)))
		return -EINVAL;

	return 0;
}

static int ps5169_gpio_init(struct ps5169_info *info)
{
	int ret;

	info->ps5169_pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR_OR_NULL(info->ps5169_pinctrl)) {
		pr_err("%s: No pinctrl config specified\n", __func__);
		ret = PTR_ERR(info->dev);
		return ret;
	}

	info->ps5169_gpio_active =
		pinctrl_lookup_state(info->ps5169_pinctrl, "ps5169_active");
	if (IS_ERR_OR_NULL(info->ps5169_gpio_active)) {
		pr_err("%s: No active config specified\n", __func__);
		ret = PTR_ERR(info->ps5169_gpio_active);
		return ret;
	}
	info->ps5169_gpio_suspend =
		pinctrl_lookup_state(info->ps5169_pinctrl, "ps5169_suspend");
	if (IS_ERR_OR_NULL(info->ps5169_gpio_suspend)) {
		pr_err("%s: No suspend config specified\n", __func__);
		ret = PTR_ERR(info->ps5169_gpio_suspend);
		return ret;
	}
	ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_active);
	if (ret < 0) {
		pr_err("%s: fail to select pinctrl active rc=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int ps5169_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	static int retry_count = 0;
	struct ps5169_info *info;

	pr_info("%s: =/START-PROBE/=\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	mutex_init(&info->i2c_lock);
	info->name = PS5169_DRIVER_NAME;
	info->client  = client;
	info->dev = &client->dev;
	info->present_flag = false;
	info->pre_ps_enable = 0;
	info->ps_enable = 0;
	info->flip = 0;
	i2c_set_clientdata(client, info);
	g_info = info;

	info->regmap = devm_regmap_init_i2c(client, &ps5169_regmap_config);
	if (IS_ERR(info->regmap)) {
		pr_err("%s: failed to initialize regmap\n", __func__);
		return PTR_ERR(info->regmap);
	}

	ret = ps5169_parse_dt(info);
	if (ret < 0) {
		pr_err("%s: parse dt error [%d]\n", __func__, ret);
		goto cleanup;
	}

	ret = ps5169_gpio_init(info);
	if (ret < 0) {
		pr_err("%s: gpio init error [%d]\n", __func__, ret);
		goto cleanup;
	}

	ret = ps5169_init_psy(info);
	if (ret < 0) {
		pr_err("%s: psy init error [%d]\n", __func__, ret);
		goto cleanup;
	}	

	ret = ps5169_get_chipid_revision(info);
	if (ret < 0) {
		if (retry_count < 3) {
			pr_err("%s: chipid i2c err, probe retry count:%d.\n",
					__func__, retry_count);
			retry_count++;
			return -EPROBE_DEFER;
		} else {
			pr_err("%s: chipid i2c err, retry count max, no find ps5169.\n", __func__);
			goto cleanup;
		}
	}

	ps5169_set_config(info);
	pr_info("%s: set cfg.\n", __func__);

	INIT_DELAYED_WORK(&info->ps_en_work, ps5169_enable_work);

	pr_info("%s: success probe!\n", __func__);

	return 0;

cleanup:
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int ps5169_remove(struct i2c_client *client)
{
	struct ps5169_info *info = i2c_get_clientdata(client);
	pr_err("%s: driver remove\n", __func__);
	info->present_flag = false;
	cancel_delayed_work_sync(&info->ps_en_work);
	gpio_free(info->enable_gpio);
	i2c_set_clientdata(client, NULL);
	return 0;
}


static const struct of_device_id ps5169_dt_match[] = {
	{ .compatible = "mi,ps5169" },
	{ },
};
MODULE_DEVICE_TABLE(of, ps5169_dt_match);

static const struct i2c_device_id ps5169_id[] = {
	{ "ps5169", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ps5169_id);

static struct i2c_driver ps5169_driver = {
	.driver   = {
			.name = PS5169_DRIVER_NAME,
			.of_match_table = of_match_ptr(ps5169_dt_match),
			},
	.probe    = ps5169_probe,
	.remove   = ps5169_remove,
	.id_table = ps5169_id,
};

static int __init ps5169_init(void)
{
	int ret;
	pr_info("%s.\n", __func__);
	ret = i2c_add_driver(&ps5169_driver);
	if (ret)
		pr_err("ps5169 i2c driver init failed!\n");

	return ret;
}
static void __exit ps5169_exit(void)
{
	i2c_del_driver(&ps5169_driver);
}

module_init(ps5169_init);
module_exit(ps5169_exit);

MODULE_DESCRIPTION("PS5169 driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");

