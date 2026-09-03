// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 Spreadtrum Communications Inc.
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define SC27XX_KPLED_V_SHIFT		12
#define SC27XX_KPLED_V_MSK		GENMASK(15, 12)
#define SC27XX_KPLED_PD			BIT(11)
#define SC27XX_KPLED_PULLDOWN_EN	BIT(10)
#define SC27XX_KPLED_CTRL1_LDO_PD_MASK	BIT(15)
#define SC27XX_KPLED_CTRL0_LDO_PD_MASK	BIT(8)

#define SC27XX_KPLED_CTRL1_LDO_V_SHIFT	7
#define SC27XX_KPLED_CTRL1_LDO_V_MASK	GENMASK(14, 7)
#define SC27XX_KPLED_CTRL0_LDO_V_SHIFT	0
#define SC27XX_KPLED_CTRL0_LDO_V_MASK	GENMASK(7, 0)

#define SC27XX_KPLED_BRIGHTNESS_MAX	127
#define SC27XX_KPLED_BRIGHTNESS_MIN	0

enum sc27xx_pmic_kpled_mode {
	SC27XX_KPLED_CURRENT_MODE,
	SC27XX_KPLED_LDO_MODE,
};

#define SC27XX_KPLED_RUN_MODE	SC27XX_KPLED_LDO_MODE

/* sc27xx keypad backlight */
struct sc27xx_kpled {
	struct mutex mutex;
	struct led_classdev cdev;
	struct regmap *regmap;
	int run_mode; /* current or ldo mode */
	u32 ctrl0;/* current and ldo mode in sc2731 */
	u32 ctrl1; /* ldo mode in sc2721 */
};

#define to_sc27xx_led(led_cdev) \
	container_of(led_cdev, struct sc27xx_kpled, cdev)

static int sc27xx_kpled_current_switch(struct sc27xx_kpled *led, bool enable)
{
	if (enable)
		return regmap_update_bits(led->regmap, led->ctrl0,
					  SC27XX_KPLED_PD, 0);

	return regmap_update_bits(led->regmap, led->ctrl0, SC27XX_KPLED_PD,
				  SC27XX_KPLED_PD);
}

static int sc27xx_kpled_ldo_switch(struct sc27xx_kpled *led, bool enable)
{
	u32 ldo_reg, ldo_pd_mask;
	int err;

	if (led->ctrl1) {
		ldo_reg = led->ctrl1;
		ldo_pd_mask = SC27XX_KPLED_CTRL1_LDO_PD_MASK;
	} else {
		ldo_reg = led->ctrl0,
		ldo_pd_mask = SC27XX_KPLED_CTRL0_LDO_PD_MASK;
	}

	if (enable)
		return regmap_update_bits(led->regmap, ldo_reg, ldo_pd_mask, 0);

	err = regmap_update_bits(led->regmap, led->ctrl0,
				 SC27XX_KPLED_PULLDOWN_EN,
				 SC27XX_KPLED_PULLDOWN_EN);
	if (err)
		return err;

	return regmap_update_bits(led->regmap, ldo_reg, ldo_pd_mask,
				  ldo_pd_mask);
}

static int sc27xx_kpled_set_brightness(struct sc27xx_kpled *led,
				       enum led_brightness value)
{
	u32 ldo_reg, ldo_v_mask, brightness_level = value;
	u8 ldo_v_shift;

	if (brightness_level > SC27XX_KPLED_BRIGHTNESS_MAX)
		brightness_level = SC27XX_KPLED_BRIGHTNESS_MAX;
	else if (brightness_level < SC27XX_KPLED_BRIGHTNESS_MIN)
		brightness_level = SC27XX_KPLED_BRIGHTNESS_MIN;

	led->cdev.brightness = brightness_level;

	if (led->run_mode == SC27XX_KPLED_CURRENT_MODE) {
		brightness_level = brightness_level / 16;
		return regmap_update_bits(led->regmap, led->ctrl0, SC27XX_KPLED_V_MSK,
					  (brightness_level << SC27XX_KPLED_V_SHIFT));
	}

	if (led->ctrl1) {
		ldo_reg = led->ctrl1,
		ldo_v_shift = SC27XX_KPLED_CTRL1_LDO_V_SHIFT;
		ldo_v_mask = SC27XX_KPLED_CTRL1_LDO_V_MASK;
	} else {
		ldo_reg = led->ctrl0,
		ldo_v_shift = SC27XX_KPLED_CTRL0_LDO_V_SHIFT;
		ldo_v_mask = SC27XX_KPLED_CTRL0_LDO_V_MASK;
	}

	return regmap_update_bits(led->regmap, ldo_reg, ldo_v_mask,
				  ((brightness_level << ldo_v_shift) &
				   ldo_v_mask));
}

static int sc27xx_kpled_enable(struct sc27xx_kpled *led,
			       enum led_brightness value)
{
	int err;

	if (led->run_mode == SC27XX_KPLED_CURRENT_MODE)
		err = sc27xx_kpled_current_switch(led, true);
	else
		err = sc27xx_kpled_ldo_switch(led, true);

	if (err)
		return err;

	return sc27xx_kpled_set_brightness(led, value);
}

static int sc27xx_kpled_disable(struct sc27xx_kpled *led)
{
	int err;

	err = sc27xx_kpled_set_brightness(led, LED_OFF);

	if (err)
		return err;

	if (led->run_mode == SC27XX_KPLED_CURRENT_MODE)
		err = sc27xx_kpled_current_switch(led, false);
	else
		err = sc27xx_kpled_ldo_switch(led, false);

	return err;
}

static int sc27xx_kpled_set(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	struct sc27xx_kpled *led = to_sc27xx_led(led_cdev);
	int err;

	mutex_lock(&led->mutex);

	if (value == LED_OFF)
		err = sc27xx_kpled_disable(led);
	else
		err = sc27xx_kpled_enable(led, value);

	mutex_unlock(&led->mutex);

	return err;
}

static int sc27xx_kpled_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct sc27xx_kpled *led;
	u32 ctrl0, ctrl1;

	err = of_property_read_u32_index(np, "reg", 0, &ctrl0);
	if (err) {
		dev_err(dev, "failed to get control register 0\n");
		return err;
	}

	err = of_property_read_u32_index(np, "reg", 1, &ctrl1);
	if (err) {
		dev_warn(dev, "no control register 1 setting\n");
		ctrl1 = 0;
	}

	led = devm_kzalloc(dev, sizeof(struct sc27xx_kpled), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	platform_set_drvdata(pdev, led);
	mutex_init(&led->mutex);
	led->regmap = dev_get_regmap(dev->parent, NULL);
	if (!led->regmap) {
		dev_err(dev, "failed to get regmap\n");
		mutex_destroy(&led->mutex);
		return -ENODEV;
	}

	led->cdev.brightness_set_blocking = sc27xx_kpled_set;
	led->cdev.default_trigger = "none";
	led->cdev.name = "keyboard-backlight";
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led->cdev.max_brightness = SC27XX_KPLED_BRIGHTNESS_MAX;
	led->run_mode = SC27XX_KPLED_RUN_MODE;
	led->ctrl0 = ctrl0;
	led->ctrl1 = ctrl1;

	err = devm_led_classdev_register(dev, &led->cdev);
	if (err < 0) {
		dev_err(dev, "failed to register led device\n");
		mutex_destroy(&led->mutex);
	}

	return err;
}

static int sc27xx_kpled_remove(struct platform_device *pdev)
{
	struct sc27xx_kpled *led = platform_get_drvdata(pdev);

	sc27xx_kpled_disable(led);
	mutex_destroy(&led->mutex);

	return 0;
}

static const struct of_device_id sc27xx_kpled_of_match[] = {
	{ .compatible = "sprd,sc2731-keypad-led", },
	{ .compatible = "sprd,sc2723-keypad-led", },
	{ .compatible = "sprd,sc2721-keypad-led", },
	{ .compatible = "sprd,sc2730-keypad-led", },
	{ .compatible = "sprd,ump9620-keypad-led", },
	{ }
};

MODULE_DEVICE_TABLE(of, sc27xx_kpled_of_match);

static struct platform_driver sc27xx_kpled_driver = {
	.driver = {
		.name = "sc27xx-keypad-led",
		.of_match_table = sc27xx_kpled_of_match,
	 },
	.probe = sc27xx_kpled_probe,
	.remove = sc27xx_kpled_remove,
};

module_platform_driver(sc27xx_kpled_driver);

MODULE_DESCRIPTION("sc27xx Keyboard backlight driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sc27xx_kpled");
