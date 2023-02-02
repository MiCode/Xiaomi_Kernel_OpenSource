// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Mediatek Inc.
 *
 * Author: Alice Chen <alice_chen@richtek.com>
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/led-class-flash.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include <media/v4l2-flash-led-class.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"
#include <linux/power_supply.h>
#endif

enum {
	MT6370_LED_FLASH1,
	MT6370_LED_FLASH2,
	MT6370_MAX_LEDS
};

/* Virtual definition for multicolor */

#define MT6370_REG_FLEDEN		0x17E
#define MT6370_REG_STRBTO		0x173
#define MT6370_REG_CHGSTAT2		0x1D1
#define MT6370_REG_FLEDSTAT1		0x1D9
#define MT6370_REG_FLEDISTRB(_id)	(0x174 + 4 * (_id))
#define MT6370_REG_FLEDITOR(_id)	(0x175 + 4 * (_id))
#define MT6370_ITORCH_MASK		GENMASK(4, 0)
#define MT6370_ISTROBE_MASK		GENMASK(6, 0)
#define MT6370_STRBTO_MASK		GENMASK(6, 0)
#define MT6370_TORCHEN_MASK		BIT(3)
#define MT6370_STROBEN_MASK		BIT(2)
#define MT6370_FLCSEN_MASK(_id)		BIT(MT6370_LED_FLASH2 - (_id))
#define MT6370_FLCSEN_MASK_ALL		GENMASK(1, 0)
#define MT6370_FLEDCHGVINOVP_MASK	BIT(3)
#define MT6370_FLED1STRBTO_MASK		BIT(11)
#define MT6370_FLED2STRBTO_MASK		BIT(10)
#define MT6370_FLED1STRB_MASK		BIT(9)
#define MT6370_FLED2STRB_MASK		BIT(8)
#define MT6370_FLED1SHORT_MASK		BIT(7)
#define MT6370_FLED2SHORT_MASK		BIT(6)
#define MT6370_FLEDLVF_MASK		BIT(3)

#define MT6370_LED_JOINT		2
#define MT6370_RANGE_FLED_REG		4
#define MT6370_ITORCH_MIN_uA		25000
#define MT6370_ITORCH_STEP_uA		12500
#define MT6370_ITORCH_MAX_uA		400000
#define MT6370_ITORCH_DOUBLE_MAX_uA	800000
#define MT6370_ISTRB_MIN_uA		50000
#define MT6370_ISTRB_STEP_uA		12500
#define MT6370_ISTRB_MAX_uA		1500000
#define MT6370_ISTRB_DOUBLE_MAX_uA	3000000
#define MT6370_STRBTO_MIN_US		64000
#define MT6370_STRBTO_STEP_US		32000
#define MT6370_STRBTO_MAX_US		2432000

#define STATE_OFF			0
#define STATE_KEEP			1
#define STATE_ON			2

#define to_mt6370_led(ptr, member) container_of(ptr, struct mt6370_led, member)

struct mt6370_led {
	struct led_classdev_flash flash;
	struct v4l2_flash *v4l2_flash;
	struct mt6370_priv *priv;
	u32 led_no;
	u32 default_state;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id dev_id;
#endif
};

struct mt6370_priv {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	unsigned int fled_strobe_used;
	unsigned int fled_torch_used;
	unsigned int leds_active;
	unsigned int leds_count;
	struct mt6370_led leds[];
};

static int mt6370_torch_brightness_set(struct led_classdev *lcdev,
				       enum led_brightness level)
{
	struct mt6370_led *led = to_mt6370_led(lcdev, flash.led_cdev);
	struct mt6370_priv *priv = led->priv;
	u32 led_enable_mask = (led->led_no == MT6370_LED_JOINT) ?
			      MT6370_FLCSEN_MASK_ALL :
			      MT6370_FLCSEN_MASK(led->led_no);
	u32 enable_mask = MT6370_TORCHEN_MASK | led_enable_mask;
	u32 val = level ? led_enable_mask : 0;
	u32 curr;
	int ret, i;

	mutex_lock(&priv->lock);

	/*
	 * There is only one set of flash control logic,
	 * and this flag is used to check if 'strobe' is currently being used.
	 */
	if (priv->fled_strobe_used) {
		dev_warn(lcdev->dev, "Please disable strobe first [%d]\n",
			 priv->fled_strobe_used);
		ret = -EBUSY;
		goto unlock;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(1);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low() && level) {
		dev_info(lcdev->dev, "pt is low\n");
		mutex_unlock(&priv->lock);
		return 0;
	}
#endif

	if (level)
		curr = priv->fled_torch_used | BIT(led->led_no);
	else
		curr = priv->fled_torch_used & ~BIT(led->led_no);

	if (curr)
		val |= MT6370_TORCHEN_MASK;

	if (level) {
		level -= 1;
		if (led->led_no == MT6370_LED_JOINT) {
			int flevel[MT6370_MAX_LEDS];

			flevel[0] = level / 2;
			flevel[1] = level - flevel[0];
			for (i = 0; i < MT6370_MAX_LEDS; i++) {
				ret = regmap_update_bits(priv->regmap,
						MT6370_REG_FLEDITOR(i),
						MT6370_ITORCH_MASK, flevel[i]);
				if (ret)
					goto unlock;
			}
		} else {
			ret = regmap_update_bits(priv->regmap,
					MT6370_REG_FLEDITOR(led->led_no),
					MT6370_ITORCH_MASK, level);
			if (ret)
				goto unlock;
		}
	}

	ret = regmap_update_bits(priv->regmap, MT6370_REG_FLEDEN,
				 enable_mask, val);
	if (ret)
		goto unlock;

	priv->fled_torch_used = curr;

unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static int mt6370_flash_brightness_set(struct led_classdev_flash *fl_cdev,
				       u32 brightness)
{
	/*
	 * Because of the current spikes when turning on the flash,
	 * the brightness should be kept by the LED framework.
	 * This empty function is used to prevent checking failure when
	 * led_classdev_flash registers ops.
	 */
	return 0;
}

static int _mt6370_flash_brightness_set(struct led_classdev_flash *fl_cdev,
					u32 brightness)
{
	struct mt6370_led *led = to_mt6370_led(fl_cdev, flash);
	struct mt6370_priv *priv = led->priv;
	struct led_flash_setting *s = &fl_cdev->brightness;
	u32 val = (brightness - s->min) / s->step;
	int ret, i;

	if (led->led_no == MT6370_LED_JOINT) {
		int flevel[MT6370_MAX_LEDS];

		flevel[0] = val / 2;
		flevel[1] = val - flevel[0];
		for (i = 0; i < MT6370_MAX_LEDS; i++) {
			ret = regmap_update_bits(priv->regmap,
						 MT6370_REG_FLEDISTRB(i),
						 MT6370_ISTROBE_MASK, flevel[i]);
			if (ret)
				return ret;
		}
	} else {
		return regmap_update_bits(priv->regmap,
					  MT6370_REG_FLEDISTRB(led->led_no),
					  MT6370_ISTROBE_MASK, val);
	}

	return 0;
}

static int mt6370_strobe_set(struct led_classdev_flash *fl_cdev, bool state)
{
	struct mt6370_led *led = to_mt6370_led(fl_cdev, flash);
	struct mt6370_priv *priv = led->priv;
	struct led_classdev *lcdev = &fl_cdev->led_cdev;
	struct led_flash_setting *s = &fl_cdev->brightness;
	u32 led_enable_mask = (led->led_no == MT6370_LED_JOINT) ?
			      MT6370_FLCSEN_MASK_ALL :
			      MT6370_FLCSEN_MASK(led->led_no);
	u32 enable_mask = MT6370_STROBEN_MASK | led_enable_mask;
	u32 val = state ? led_enable_mask : 0;
	u32 curr;
	int ret;

	mutex_lock(&priv->lock);

	/*
	 * There is only one set of flash control logic,
	 * and this flag is used to check if 'torch' is currently being used.
	 */
	if (priv->fled_torch_used) {
		dev_warn(lcdev->dev, "Please disable torch first [0x%x]\n",
				      priv->fled_torch_used);
		ret = -EBUSY;
		goto unlock;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(state);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(lcdev->dev, "pt is low\n");
		mutex_unlock(&priv->lock);
		return 0;
	}
#endif

	if (state)
		curr = priv->fled_strobe_used | BIT(led->led_no);
	else
		curr = priv->fled_strobe_used & ~BIT(led->led_no);

	if (curr)
		val |= MT6370_STROBEN_MASK;

	ret = regmap_update_bits(priv->regmap, MT6370_REG_FLEDEN, enable_mask,
				 val);
	if (ret) {
		dev_err(lcdev->dev, "[%d] control current source %d fail\n",
				     led->led_no, state);
		goto unlock;
	}

	/*
	 * If the flash needs to turn on, configure the flash current to
	 * ramp up to the setting value.
	 * Otherwise, always revert to the minimum one.
	 */
	ret = _mt6370_flash_brightness_set(fl_cdev, state ? s->val : s->min);
	if (ret)
		goto unlock;

	/*
	 * For the flash to turn on/off, we must wait for
	 * HW ramping up/down time 5ms/500us to prevent the unexpected problem.
	 */
	if (!priv->fled_strobe_used && curr)
		usleep_range(5000, 6000);
	else if (priv->fled_strobe_used && !curr)
		usleep_range(500, 600);

	priv->fled_strobe_used = curr;

unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static int mt6370_strobe_get(struct led_classdev_flash *fl_cdev, bool *state)
{
	struct mt6370_led *led = to_mt6370_led(fl_cdev, flash);
	struct mt6370_priv *priv = led->priv;

	mutex_lock(&priv->lock);
	*state = !!(priv->fled_strobe_used & BIT(led->led_no));
	mutex_unlock(&priv->lock);

	return 0;
}

static int mt6370_timeout_set(struct led_classdev_flash *fl_cdev, u32 timeout)
{
	struct mt6370_led *led = to_mt6370_led(fl_cdev, flash);
	struct mt6370_priv *priv = led->priv;
	struct led_flash_setting *s = &fl_cdev->timeout;
	u32 val = (timeout - s->min) / s->step;
	int ret;

	mutex_lock(&priv->lock);
	ret = regmap_update_bits(priv->regmap, MT6370_REG_STRBTO,
				 MT6370_STRBTO_MASK, val);
	mutex_unlock(&priv->lock);

	return ret;
}

static int mt6370_fault_get(struct led_classdev_flash *fl_cdev, u32 *fault)
{
	struct mt6370_led *led = to_mt6370_led(fl_cdev, flash);
	struct mt6370_priv *priv = led->priv;
	u16 fled_stat;
	unsigned int chg_stat, strobe_timeout_mask, fled_short_mask;
	u32 rfault = 0;
	int ret;

	mutex_lock(&priv->lock);
	ret = regmap_read(priv->regmap, MT6370_REG_CHGSTAT2, &chg_stat);
	if (ret)
		goto unlock;

	ret = regmap_raw_read(priv->regmap, MT6370_REG_FLEDSTAT1, &fled_stat,
			      sizeof(fled_stat));
	if (ret)
		goto unlock;

	switch (led->led_no) {
	case MT6370_LED_FLASH1:
		strobe_timeout_mask = MT6370_FLED1STRBTO_MASK;
		fled_short_mask = MT6370_FLED1SHORT_MASK;
		break;

	case MT6370_LED_FLASH2:
		strobe_timeout_mask = MT6370_FLED2STRBTO_MASK;
		fled_short_mask = MT6370_FLED2SHORT_MASK;
		break;

	case MT6370_LED_JOINT:
		strobe_timeout_mask = MT6370_FLED1STRBTO_MASK |
				      MT6370_FLED2STRBTO_MASK;
		fled_short_mask = MT6370_FLED1SHORT_MASK |
				  MT6370_FLED2SHORT_MASK;
	}

	if (chg_stat & MT6370_FLEDCHGVINOVP_MASK)
		rfault |= LED_FAULT_INPUT_VOLTAGE;

	if (fled_stat & strobe_timeout_mask)
		rfault |= LED_FAULT_TIMEOUT;

	if (fled_stat & fled_short_mask)
		rfault |= LED_FAULT_SHORT_CIRCUIT;

	if (fled_stat & MT6370_FLEDLVF_MASK)
		rfault |= LED_FAULT_UNDER_VOLTAGE;

	*fault = rfault;
unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static const struct led_flash_ops mt6370_flash_ops = {
	.flash_brightness_set = mt6370_flash_brightness_set,
	.strobe_set = mt6370_strobe_set,
	.strobe_get = mt6370_strobe_get,
	.timeout_set = mt6370_timeout_set,
	.fault_get = mt6370_fault_get,
};

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
static struct led_classdev_flash *mt6370_flash_class[MT6370_MAX_LEDS];
/* is decrease voltage */
static int is_decrease_voltage;
static DEFINE_MUTEX(mt6370_mutex);
/* define usage count */
static int fd_use_count;
/******************************************************************************
 * Charger power supply class
 *****************************************************************************/
static int mt6370_high_voltage_supply(int enable)
{
	union power_supply_propval prop;
	static struct power_supply *chg_psy;
	int ret;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		ret = -1;
	} else {
		prop.intval = enable;
		ret = power_supply_set_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
		pr_info("%s enable_hv:%d\n", __func__, prop.intval);
		power_supply_changed(chg_psy);
	}

	return ret;
}

static int mt6370_set_scenario(int scenario)
{
	/* notify charger to increase or decrease voltage */
	mutex_lock(&mt6370_mutex);
	if (scenario & FLASHLIGHT_SCENARIO_CAMERA_MASK) {
		if (!is_decrease_voltage) {
			mt6370_high_voltage_supply(0);
			is_decrease_voltage = 1;
		}
	} else {
		if (is_decrease_voltage) {
			mt6370_high_voltage_supply(1);
			is_decrease_voltage = 0;
		}
	}
	mutex_unlock(&mt6370_mutex);

	return 0;
}

static int mt6370_open(void)
{
	mutex_lock(&mt6370_mutex);
	fd_use_count++;
	mutex_unlock(&mt6370_mutex);

	return 0;
}

static int mt6370_release(void)
{
	mutex_lock(&mt6370_mutex);
	fd_use_count--;
	/* If camera NE, we need to enable pe by ourselves*/
	if (fd_use_count == 0 && is_decrease_voltage) {
		pr_info("Increase voltage level.\n");
		mt6370_high_voltage_supply(1);
		is_decrease_voltage = 0;
	}
	mutex_unlock(&mt6370_mutex);

	return 0;
}

static int mt6370_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	if (channel >= MT6370_MAX_LEDS || channel < 0)
		return -EINVAL;

	flcdev = mt6370_flash_class[channel];
	if (flcdev == NULL)
		return -EINVAL;

	lcdev = &flcdev->led_cdev;
	if (lcdev == NULL)
		return -EINVAL;

	switch (cmd) {
	case FLASH_IOC_SET_ONOFF:
		mt6370_torch_brightness_set(lcdev, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		mt6370_set_scenario(fl_arg->arg);
		break;

	default:
		dev_info(lcdev->dev, "No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static ssize_t mt6370_strobe_store(struct flashlight_arg arg)
{
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	if (arg.channel < 0 || arg.channel >= MT6370_MAX_LEDS)
		return -EINVAL;

	flcdev = mt6370_flash_class[arg.channel];
	lcdev = &flcdev->led_cdev;
	mt6370_torch_brightness_set(lcdev, LED_ON);
	msleep(arg.dur);
	mt6370_torch_brightness_set(lcdev, LED_OFF);
	return 0;
}

static int mt6370_set_driver(int set)
{
	return 0;
}

static struct flashlight_operations mt6370_ops = {
	mt6370_open,
	mt6370_release,
	mt6370_ioctl,
	mt6370_strobe_store,
	mt6370_set_driver
};

static int mt6370_init_proprietary_properties(struct mt6370_led *led,
					struct led_init_data *init_data)
{
	struct led_classdev_flash *flash = &led->flash;
	struct led_classdev *lcdev = &flash->led_cdev;
	int ret;

	ret = fwnode_property_read_u32(init_data->fwnode,
			"reg", &led->dev_id.channel);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(init_data->fwnode,
			"type", &led->dev_id.type);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(init_data->fwnode,
			"ct", &led->dev_id.ct);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(init_data->fwnode,
			"part", &led->dev_id.part);
	if (ret)
		return ret;

	strscpy(led->dev_id.name, lcdev->dev->kobj.name,
		sizeof(led->dev_id.name));

	led->dev_id.decouple = 0;
	mt6370_flash_class[led->dev_id.channel] = &led->flash;

	if (flashlight_dev_register_by_device_id(&led->dev_id, &mt6370_ops))
		return -EFAULT;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_V4L2_FLASH_LED_CLASS)
static int mt6370_flash_external_strobe_set(struct v4l2_flash *v4l2_flash,
					    bool enable)
{
	struct led_classdev_flash *flash = v4l2_flash->fled_cdev;
	struct mt6370_led *led = to_mt6370_led(flash, flash);
	struct mt6370_priv *priv = led->priv;
	u32 mask = (led->led_no == MT6370_LED_JOINT) ? MT6370_FLCSEN_MASK_ALL :
		   MT6370_FLCSEN_MASK(led->led_no);
	u32 val = enable ? mask : 0;
	int ret;

	mutex_lock(&priv->lock);
	ret = regmap_update_bits(priv->regmap, MT6370_REG_FLEDEN, mask, val);
	if (ret)
		goto unlock;

	if (enable)
		priv->fled_strobe_used |= BIT(led->led_no);
	else
		priv->fled_strobe_used &= ~BIT(led->led_no);

unlock:
	mutex_unlock(&priv->lock);
	return ret;
}

static const struct v4l2_flash_ops v4l2_flash_ops = {
	.external_strobe_set = mt6370_flash_external_strobe_set,
};

static void mt6370_init_v4l2_flash_config(struct mt6370_led *led,
					  struct v4l2_flash_config *config)
{
	struct led_classdev *lcdev;
	struct led_flash_setting *s = &config->intensity;

	lcdev = &led->flash.led_cdev;

	s->min = MT6370_ITORCH_MIN_uA;
	s->step = MT6370_ITORCH_STEP_uA;
	s->val = s->max = s->min + (lcdev->max_brightness - 1) * s->step;

	config->has_external_strobe = 1;
	strscpy(config->dev_name, lcdev->dev->kobj.name,
		sizeof(config->dev_name));

	config->flash_faults = LED_FAULT_SHORT_CIRCUIT | LED_FAULT_TIMEOUT |
			       LED_FAULT_INPUT_VOLTAGE |
			       LED_FAULT_UNDER_VOLTAGE;
}
#else
static const struct v4l2_flash_ops v4l2_flash_ops;
static void mt6370_init_v4l2_flash_config(struct mt6370_led *led,
					  struct v4l2_flash_config *config)
{
}
#endif

static void mt6370_v4l2_flash_release(void *v4l2_flash)
{
	v4l2_flash_release(v4l2_flash);
}

/*
 * To Avoid ABI Function
 */

static void mt6370_devm_led_classdev_flash_release(struct device *dev, void *res)
{
	led_classdev_flash_unregister(*(struct led_classdev_flash **)res);
}

static int mt6370_devm_led_classdev_flash_register_ext(struct device *parent,
						       struct led_classdev_flash *fled_cdev,
						       struct led_init_data *init_data)
{
	struct led_classdev_flash **dr;
	int ret;

	dr = devres_alloc(mt6370_devm_led_classdev_flash_release, sizeof(*dr),
			  GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	ret = led_classdev_flash_register_ext(parent, fled_cdev, init_data);
	if (ret) {
		devres_free(dr);
		return ret;
	}

	*dr = fled_cdev;
	devres_add(parent, dr);

	return 0;
}

static int mt6370_led_register(struct device *parent, struct mt6370_led *led,
				struct led_init_data *init_data)
{
	struct v4l2_flash_config v4l2_config = {};
	int ret;

	ret = mt6370_devm_led_classdev_flash_register_ext(parent, &led->flash,
							  init_data);
	if (ret)
		return dev_err_probe(parent, ret,
				     "Couldn't register flash %d\n", led->led_no);

	mt6370_init_v4l2_flash_config(led, &v4l2_config);
	led->v4l2_flash = v4l2_flash_init(parent, init_data->fwnode,
					  &led->flash, &v4l2_flash_ops,
					  &v4l2_config);
	if (IS_ERR(led->v4l2_flash))
		return dev_err_probe(parent, PTR_ERR(led->v4l2_flash),
				     "Failed to register %d v4l2 sd\n", led->led_no);

	return devm_add_action_or_reset(parent, mt6370_v4l2_flash_release,
					led->v4l2_flash);
}

static u32 mt6370_clamp(u32 val, u32 min, u32 max, u32 step)
{
	u32 retval;

	retval = clamp_val(val, min, max);
	if (step > 1)
		retval = rounddown(retval - min, step) + min;

	return retval;
}

static int mt6370_init_flash_properties(struct mt6370_led *led,
					struct led_init_data *init_data)
{
	struct led_classdev_flash *flash = &led->flash;
	struct led_classdev *lcdev = &flash->led_cdev;
	struct mt6370_priv *priv = led->priv;
	struct led_flash_setting *s;
	u32 sources[MT6370_MAX_LEDS];
	u32 max_uA, val;
	int i, ret, num;

	num = fwnode_property_count_u32(init_data->fwnode, "led-sources");
	if (num < 1 || num > MT6370_MAX_LEDS)
		return dev_err_probe(priv->dev, -EINVAL,
				     "Not specified or wrong number of led-sources\n");

	ret = fwnode_property_read_u32_array(init_data->fwnode, "led-sources", sources, num);
	if (ret)
		return ret;

	for (i = 0; i < num; i++) {
		if (sources[i] >= MT6370_MAX_LEDS)
			return -EINVAL;
		if (priv->leds_active & BIT(sources[i]))
			return -EINVAL;
		priv->leds_active |= BIT(sources[i]);

	}
	led->led_no = (num == MT6370_MAX_LEDS) ? MT6370_LED_JOINT :
		       sources[0];

	max_uA = (num == 2) ? MT6370_ITORCH_DOUBLE_MAX_uA : MT6370_ITORCH_MAX_uA;
	ret = fwnode_property_read_u32(init_data->fwnode, "led-max-microamp", &val);
	if (ret) {
		dev_info(priv->dev,
			 "Not specified led-max-microamp, config to the minimum\n");
		val = MT6370_ITORCH_MIN_uA;
	} else {
		val = mt6370_clamp(val, MT6370_ITORCH_MIN_uA, max_uA,
				  MT6370_ITORCH_STEP_uA);
	}

	lcdev->max_brightness = (val - MT6370_ITORCH_MIN_uA) /
				MT6370_ITORCH_STEP_uA + 1;
	lcdev->brightness_set_blocking = mt6370_torch_brightness_set;
	lcdev->flags |= LED_DEV_CAP_FLASH;

	max_uA = (num == 2) ? MT6370_ISTRB_DOUBLE_MAX_uA : MT6370_ISTRB_MAX_uA;
	ret = fwnode_property_read_u32(init_data->fwnode, "flash-max-microamp", &val);
	if (ret) {
		dev_info(priv->dev,
		   "Not specified flash-max-microamp, config to the minimum\n");
		val = MT6370_ISTRB_MIN_uA;
	} else {
		val = mt6370_clamp(val, MT6370_ISTRB_MIN_uA, max_uA,
				  MT6370_ISTRB_STEP_uA);
	}

	s = &flash->brightness;
	s->min = MT6370_ISTRB_MIN_uA;
	s->step = MT6370_ISTRB_STEP_uA;
	s->val = s->max = val;

	/*
	 * Always configure to the minimum level when off
	 * to prevent flash current spikes.
	 */
	ret = _mt6370_flash_brightness_set(flash, s->min);
	if (ret)
		return ret;

	ret = fwnode_property_read_u32(init_data->fwnode,
				       "flash-max-timeout-us", &val);
	if (ret) {
		dev_info(priv->dev,
			 "Not specified flash-max-timeout-us, config to the minimum\n");
		val = MT6370_STRBTO_MIN_US;
	} else {
		val = mt6370_clamp(val, MT6370_STRBTO_MIN_US,
				   MT6370_STRBTO_MAX_US, MT6370_STRBTO_STEP_US);
	}

	s = &flash->timeout;
	s->min = MT6370_STRBTO_MIN_US;
	s->step = MT6370_STRBTO_STEP_US;
	s->val = s->max = val;

	flash->ops = &mt6370_flash_ops;

	return 0;
}

static int mt6370_init_common_properties(struct mt6370_led *led,
					 struct led_init_data *init_data)
{
	const char * const states[] = { "off", "keep", "on" };
	const char *str = states[STATE_OFF];
	int ret;

	fwnode_property_read_string(init_data->fwnode, "default-state", &str);
	ret = match_string(states, ARRAY_SIZE(states), str);
	if (ret >= 0)
		led->default_state = ret;

	return 0;
}

static int mt6370_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt6370_priv *priv;
	struct fwnode_handle *child;
	size_t count;
	int i = 0, ret;

	count = device_get_child_node_count(dev);
	if (!count || count > MT6370_MAX_LEDS)
		return dev_err_probe(dev, -EINVAL,
		       "No child node or node count over max led number %zu\n", count);

	priv = devm_kzalloc(dev, struct_size(priv, leds, count), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->leds_count = count;
	priv->dev = dev;
	mutex_init(&priv->lock);

	priv->regmap = dev_get_regmap(dev->parent, NULL);
	if (!priv->regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to get parent regmap\n");

	device_for_each_child_node(dev, child) {
		struct mt6370_led *led = priv->leds + i;
		struct led_init_data init_data = { .fwnode = child, };

		led->priv = priv;
		ret = mt6370_init_common_properties(led, &init_data);
		if (ret)
			return ret;

		ret = mt6370_init_flash_properties(led, &init_data);
		if (ret)
			return ret;

		ret = mt6370_led_register(dev, led, &init_data);
		if (ret)
			return ret;

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
		ret = mt6370_init_proprietary_properties(led, &init_data);
		if (ret)
			return ret;
#endif
		i++;
	}

	return 0;
}

static const struct of_device_id mt6370_led_of_id[] = {
	{ .compatible = "mediatek,mt6370-flashlight" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6370_led_of_id);

static struct platform_driver mt6370_led_driver = {
	.driver = {
		.name = "mt6370-flashlight",
		.of_match_table = mt6370_led_of_id,
	},
	.probe = mt6370_led_probe,
};
module_platform_driver(mt6370_led_driver);

MODULE_AUTHOR("Alice Chen <alice_chen@richtek.com>");
MODULE_DESCRIPTION("MT6370 FLASH LED Driver");
MODULE_LICENSE("GPL v2");
