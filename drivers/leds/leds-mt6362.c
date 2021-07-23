// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/led-class-flash.h>
#include <media/v4l2-flash-led-class.h>
#ifdef CONFIG_MTK_CHARGER
#include <mt-plat/charger_class.h>
#endif

#ifdef CONFIG_MTK_FLASHLIGHT
#include "flashlight-core.h"

#include "mtk_charger.h"
#endif

enum {
	MT6362_INDICATOR_LED1 = 0,
	MT6362_INDICATOR_LED4,
	MT6362_INDICATOR_LEDMAX,
};

enum {
	MT6362_ISINK_FLASHMODE = 0,
	MT6362_ISINK_BREATHMODE,
	MT6362_ISINK_REGMODE,
};

enum {
	MT6362_FLASH_LED1 = 0,
	MT6362_FLASH_LED2,
	MT6362_FLASH_LEDMAX,
};

/* Real register mappings */
#define MT6362_REG_FLEDSTRBTO	(0x73)
#define MT6362_REG_FLED1ISTRB	(0x74)
#define MT6362_REG_FLED1ITORCH	(0x75)
#define MT6362_REG_FLED2ISTRB	(0x78)
#define MT6362_REG_FLED2ITORCH	(0x79)
#define MT6362_REG_FLEDEN	(0x7E)
#define MT6362_REG_RGBEN	(0x80)
#define MT6362_REG_RGB1ISNK	(0x81)
#define MT6362_REG_RGBMLISNK	(0x84)
#define MT6362_REG_RGB1DIM	(0x85)
#define MT6362_REG_RGB12FREQ	(0x89)

/* macro replacement mappings */
#define MT6362_ILED1_BRIGHTMAX	(16)
#define MT6362_ILED4_BRIGHTMAX	(32)
#define MT6362_ILED1EN_MASK	BIT(7)
#define MT6362_ILED4EN_MASK	BIT(4)
#define MT6362_CHGINDEN_MASK	BIT(3)
#define MT6362_REG_ILED1CURR	MT6362_REG_RGB1ISNK
#define MT6362_ILED1CURR_MASK	(0x0f)
#define MT6362_REG_ILED4CURR	MT6362_REG_RGBMLISNK
#define MT6362_ILED4CURR_MASK	(0x1f)
#define MT6362_REG_ILED1MODE	MT6362_REG_RGB1ISNK
#define MT6362_ILED1MODE_MASK	(0xc0)
#define MT6362_REG_ILED4MODE	(0)
#define MT6362_ILED4MODE_MASK	(0)
#define MT6362_PFREQ_MASK	(0xe0)
#define MT6362_PFREQ_SHIFT	(5)
#define MT6362_PDUTY_MASK	(0xff)
#define MT6362_PDUTY_SHIFT	(0)
#define MT6362_ILED1_MAXUA	(24000)
#define MT6362_ILED1_STEPUA	(2000)
#define MT6362_ILED4_MAXUA	(150000)
#define MT6362_ILED4_STEPUA	(5000)

#define MT6362_FLED1CSEN_MASK	BIT(1)
#define MT6362_FLED2CSEN_MASK	BIT(0)
#define MT6362_FLEDITORCH_MASK	(0x1f)
#define MT6362_FLEDISTRB_MASK	(0x7f)
#define MT6362_FLEDUISTRB_MASK	BIT(7)
#define MT6362_FLEDSTRBTO_MASK	(0x7f)
#define MT6362_FLEDSTRBEN_MASK	BIT(2)
#define MT6362_FLEDTORCHEN_MASK	BIT(3)

#define MT6362_TORCHCURR_MIN	(25000)
#define MT6362_TORCHCURR_STEP	(12500)
#define MT6362_TORCHCURR_MAX	(400000)
#define MT6362_STRBCURR_MIN	(25000)
#define MT6362_STRBCURR_STEP	(6250)
#define MT6362_STRBCURR_MAX	(1500000)
#define MT6362_STRBUISTRB_BOUND	(50000)
/* (STRBUISTRB_BOUND - STRBCURR_MIN) / STRBCURR_STEP */
#define MT6362_STRBUSTRB_BDSTEP	(4)
#define MT6362_STRBTIMEOUT_MIN	(64000)
#define MT6362_STRBTIMEOUT_STEP	(32000)
#define MT6362_STRBTIMEOUT_MAX	(2432000)

struct mt6362_indicator_cdev {
	struct led_classdev cdev;
	struct device_node *np;
	int idx;
	u32 enable_reg;
	u32 enable_mask;
	u32 currsel_reg;
	u32 currsel_mask;
	u32 mode_reg;
	u32 mode_mask;
};

struct mt6362_flash_cdev {
	struct led_classdev_flash flash;
	struct v4l2_flash *v4l2_flash;
	struct device_node *np;
	int idx;
	u32 source_enable_reg;
	u32 source_enable_mask;
	u32 torch_bright_reg;
	u32 torch_bright_mask;
	u32 strobe_bright_reg;
	u32 strobe_bright_mask;
	u32 faults;
#ifdef CONFIG_MTK_FLASHLIGHT
	struct flashlight_device_id dev_id;
#endif
};

#ifdef CONFIG_MTK_FLASHLIGHT
static struct led_classdev_flash *mt6362_flash_class[MT6362_FLASH_LEDMAX];

/* define charger consumer */
static struct charger_consumer *flashlight_charger_consumer;
#define CHARGER_SUPPLY_NAME "charger_port1"

/* is decrease voltage */
static int is_decrease_voltage;
static DEFINE_MUTEX(mt6362_mutex);

/* define usage count */
static int fd_use_count;
#endif

struct mt6362_leds_data {
	struct device *dev;
	struct regmap *regmap;
	struct mt6362_indicator_cdev indicators[MT6362_INDICATOR_LEDMAX];
	struct mt6362_flash_cdev flashleds[MT6362_FLASH_LEDMAX];
	unsigned long fl_torch_flags;
	unsigned long fl_strb_flags;
	struct charger_device *chg_dev;
};

struct mt6362_led_irqt {
	const char *name;
	irq_handler_t irqh;
};

static int mt6362_iled_brightness_set(struct led_classdev *cdev,
				       enum led_brightness brightness)
{
	struct mt6362_leds_data *data = dev_get_drvdata(cdev->dev->parent);
	struct mt6362_indicator_cdev *mtcdev = (void *)cdev;
	s32 val, shift = ffs(mtcdev->currsel_mask) - 1;
	int rv;

	if (mtcdev->idx == MT6362_INDICATOR_LED1) {
		/* Enable CHGIND software mode */
		rv = regmap_update_bits(data->regmap, MT6362_REG_RGBEN,
				MT6362_CHGINDEN_MASK, MT6362_CHGINDEN_MASK);
		if (rv)
			return rv;
	}

	if (brightness == LED_OFF) {
		return regmap_update_bits(data->regmap,
				mtcdev->enable_reg, mtcdev->enable_mask, 0);
	}

	val = brightness - 1;

	rv = regmap_update_bits(data->regmap, mtcdev->currsel_reg,
				mtcdev->currsel_mask, val << shift);
	if (rv)
		return rv;

	return regmap_update_bits(data->regmap, mtcdev->enable_reg,
				  mtcdev->enable_mask, mtcdev->enable_mask);
}

static enum led_brightness mt6362_iled_brightness_get(struct led_classdev *cdev)
{
	struct mt6362_leds_data *data = dev_get_drvdata(cdev->dev->parent);
	struct mt6362_indicator_cdev *mtcdev = (void *)cdev;
	unsigned int val = 0, shift = ffs(mtcdev->currsel_mask) - 1;
	int rv;

	rv = regmap_read(data->regmap, mtcdev->enable_reg, &val);
	if (rv)
		return rv;

	if (!(val & mtcdev->enable_mask))
		return LED_OFF;

	rv = regmap_read(data->regmap, mtcdev->currsel_reg, &val);
	if (rv)
		return rv;

	val &= mtcdev->currsel_mask;
	val >>= shift;

	/* 0 is off, plus 1 to return back */
	return (val + 1);
}

static int mt6362_iled_blink_set(struct led_classdev *cdev,
				 unsigned long *don, unsigned long *doff)
{
	struct mt6362_leds_data *data = dev_get_drvdata(cdev->dev->parent);
	struct mt6362_indicator_cdev *mtcdev = (void *)cdev;
	const unsigned int dim_freqs[] = {
		4, 8, 250, 500, 1000, 2000, 4000, 8000
	};
	unsigned long sum = *don + *doff;
	int freq, duty, shift, rv;

	if (!mtcdev->mode_reg)
		return -ENOTSUPP;

	if (!*don && !*doff)
		*don = *doff = 500;

	for (freq = 0; freq < ARRAY_SIZE(dim_freqs); freq++) {
		if (sum <= dim_freqs[freq])
			break;
	}

	if (freq == ARRAY_SIZE(dim_freqs)) {
		dev_warn(cdev->dev, "no suited pwm freq, config to 0.125Hz\n");
		freq = ARRAY_SIZE(dim_freqs) - 1;
	}

	freq = ARRAY_SIZE(dim_freqs) - freq - 1;
	rv = regmap_update_bits(data->regmap, MT6362_REG_RGB12FREQ,
				MT6362_PFREQ_MASK, freq << MT6362_PFREQ_SHIFT);
	if (rv)
		return rv;

	duty = 255 * (*don) * 1000 / sum;
	duty /= 1000;
	rv = regmap_update_bits(data->regmap, MT6362_REG_RGB1DIM,
				MT6362_PDUTY_MASK, duty << MT6362_PDUTY_SHIFT);
	if (rv)
		return rv;

	shift = ffs(mtcdev->mode_mask) - 1;
	return regmap_update_bits(data->regmap,
				  mtcdev->mode_reg, mtcdev->mode_mask,
				  MT6362_ISINK_FLASHMODE << shift);
}

static int mt6362_fled_brightness_set(struct led_classdev *cdev,
				      enum led_brightness brightness)
{
	struct mt6362_leds_data *data = dev_get_drvdata(cdev->dev->parent);
	struct mt6362_flash_cdev *mtcdev = (void *)lcdev_to_flcdev(cdev);
	int shift = ffs(mtcdev->torch_bright_mask) - 1, rv;

	dev_info(cdev->dev, "%s brightness:%d\n", __func__, brightness);
	if (data->fl_strb_flags) {
		dev_err(cdev->dev,
			"Disable all leds strobe [%lu]\n", data->fl_strb_flags);
		return -EINVAL;
	}

	if (brightness == LED_OFF) {
		rv = regmap_update_bits(data->regmap, mtcdev->source_enable_reg,
					mtcdev->source_enable_mask, 0);
		if (rv)
			return rv;

		clear_bit(mtcdev->idx, &data->fl_torch_flags);

		if (!data->fl_torch_flags) {
			/* if no user, turn torch mode to off */
			rv = regmap_update_bits(data->regmap, MT6362_REG_FLEDEN,
						MT6362_FLEDTORCHEN_MASK, 0);
			if (rv)
				return rv;
		}

		return 0;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(1);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(cdev->dev, "pt is low\n");
		return 0;
	}
#endif

	brightness -= 1;

	rv = regmap_update_bits(data->regmap, mtcdev->torch_bright_reg,
				mtcdev->torch_bright_mask, brightness << shift);
	if (rv)
		return rv;

	rv = regmap_update_bits(data->regmap, mtcdev->source_enable_reg,
			mtcdev->source_enable_mask, mtcdev->source_enable_mask);
	if (rv)
		return rv;

	/* config torch mode */
	rv = regmap_update_bits(data->regmap, MT6362_REG_FLEDEN,
			MT6362_FLEDTORCHEN_MASK, MT6362_FLEDTORCHEN_MASK);
	if (rv)
		return rv;

	mtcdev->faults = 0;
	set_bit(mtcdev->idx, &data->fl_torch_flags);

	return 0;
}

static enum led_brightness mt6362_fled_brightness_get(struct led_classdev *cdev)
{
	struct mt6362_leds_data *data = dev_get_drvdata(cdev->dev->parent);
	struct mt6362_flash_cdev *mtcdev = (void *)lcdev_to_flcdev(cdev);
	unsigned int val = 0;
	int rv;

	if (!test_bit(mtcdev->idx, &data->fl_torch_flags))
		return LED_OFF;

	rv = regmap_read(data->regmap, mtcdev->torch_bright_reg, &val);
	if (rv)
		return rv;

	val &= mtcdev->torch_bright_mask;
	val >>= (ffs(mtcdev->torch_bright_mask) - 1);

	return (val + 1);
}

static int _mt6362_fled_flash_brightness_set(struct led_classdev_flash *flcdev,
					    u32 brightness)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct mt6362_leds_data *data = dev_get_drvdata(lcdev->dev->parent);
	struct mt6362_flash_cdev *mtcdev = (void *)flcdev;
	const struct led_flash_setting *fs = &flcdev->brightness;
	int val;

	dev_info(lcdev->dev, "%s brightness:%d\n", __func__, brightness);
	if (brightness > fs->max)
		brightness = fs->max;

	val = (brightness - fs->min) / fs->step;

	if (val < MT6362_STRBUSTRB_BDSTEP)
		val |= MT6362_FLEDUISTRB_MASK;
	else
		val = (val - MT6362_STRBUSTRB_BDSTEP + 1) / 2;

	return regmap_write(data->regmap, mtcdev->strobe_bright_reg, val);
}

static int mt6362_fled_flash_brightness_set(struct led_classdev_flash *flcdev,
					    u32 brightness)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;

	dev_info(lcdev->dev, "%s brightness:%d\n", __func__, brightness);
	return 0;
}

static int mt6362_fled_strobe_set(struct led_classdev_flash *flcdev, bool state)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct mt6362_leds_data *data = dev_get_drvdata(lcdev->dev->parent);
	const struct led_flash_setting *fs = &flcdev->brightness;
	struct mt6362_flash_cdev *mtcdev = (void *)flcdev;
	int rv;

	dev_info(lcdev->dev, "%s state:%d\n", __func__, state);
	if (!(state ^ test_bit(mtcdev->idx, &data->fl_strb_flags))) {
		dev_dbg(lcdev->dev,
			"strobe no change [%lu]\n", data->fl_strb_flags);
		return 0;
	}

	if (data->fl_torch_flags) {
		dev_err(lcdev->dev,
			"Disable all leds torch [%lu]\n", data->fl_torch_flags);
		return -EINVAL;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(state);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(lcdev->dev, "pt is low\n");
		return 0;
	}
#endif
	rv = regmap_update_bits(data->regmap, mtcdev->source_enable_reg,
				mtcdev->source_enable_mask,
				state ? mtcdev->source_enable_mask : 0);
	if (rv)
		return rv;

	if (state) {
		rv = _mt6362_fled_flash_brightness_set(flcdev, fs->val);
		if (rv)
			return rv;

		if (!data->fl_strb_flags) {
#ifdef CONFIG_MTK_CHARGER
			rv = charger_dev_enable_bleed_discharge(data->chg_dev,
								true);
			if (rv)
				return rv;
#endif
			rv = regmap_update_bits(data->regmap, MT6362_REG_FLEDEN,
						MT6362_FLEDSTRBEN_MASK,
						MT6362_FLEDSTRBEN_MASK);
			if (rv)
				return rv;
			usleep_range(5000, 6000);
		}

		mtcdev->faults = 0;
		set_bit(mtcdev->idx, &data->fl_strb_flags);
	} else {
		clear_bit(mtcdev->idx, &data->fl_strb_flags);

		if (!data->fl_strb_flags) {
			rv = regmap_update_bits(data->regmap, MT6362_REG_FLEDEN,
						MT6362_FLEDSTRBEN_MASK, 0);
			if (rv)
				return rv;
			usleep_range(400, 500);
#ifdef CONFIG_MTK_CHARGER
			rv = charger_dev_enable_bleed_discharge(data->chg_dev,
								false);
			if (rv)
				return rv;
#endif
		}

		rv = _mt6362_fled_flash_brightness_set(flcdev, fs->min);
		if (rv)
			return rv;

	}

	return 0;
}

static int mt6362_fled_strobe_get(struct led_classdev_flash *flcdev,
				  bool *state)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct mt6362_leds_data *data = dev_get_drvdata(lcdev->dev->parent);
	struct mt6362_flash_cdev *mtcdev = (void *)flcdev;

	*state = test_bit(mtcdev->idx, &data->fl_strb_flags);

	return 0;
}

static int mt6362_fled_timeout_set(struct led_classdev_flash *flcdev,
				   u32 timeout)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct mt6362_leds_data *data = dev_get_drvdata(lcdev->dev->parent);
	const struct led_flash_setting *fs = &flcdev->timeout;
	int shift = ffs(MT6362_FLEDSTRBTO_MASK) - 1, val;

	dev_info(lcdev->dev, "%s timeout:%u\n", __func__, timeout);
	if (timeout > fs->max)
		timeout = fs->max;

	val = (timeout - fs->min) / fs->step;

	return regmap_update_bits(data->regmap, MT6362_REG_FLEDSTRBTO,
				  MT6362_FLEDSTRBTO_MASK, val << shift);
}

static int mt6362_fled_fault_get(struct led_classdev_flash *flcdev, u32 *fault)
{
	struct mt6362_flash_cdev *mtcdev = (void *)flcdev;

	*fault = mtcdev->faults;

	return 0;
}

static const struct led_flash_ops mt6362_fled_ctrl_ops = {
	.flash_brightness_set	= mt6362_fled_flash_brightness_set,
	.strobe_set		= mt6362_fled_strobe_set,
	.strobe_get		= mt6362_fled_strobe_get,
	.timeout_set		= mt6362_fled_timeout_set,
	.fault_get		= mt6362_fled_fault_get,
};

static int mt6362_fled_external_strobe_set(struct v4l2_flash *v4l2_flash,
					   bool enable)
{
	struct led_classdev_flash *flcdev = v4l2_flash->fled_cdev;
	struct mt6362_flash_cdev *mtcdev = (void *)flcdev;
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct mt6362_leds_data *data = dev_get_drvdata(lcdev->dev->parent);
	unsigned int val;
	int rv;

	if (!(enable ^ test_bit(mtcdev->idx, &data->fl_strb_flags))) {
		dev_dbg(lcdev->dev,
			"strobe no change [%lu]\n", data->fl_strb_flags);
		return 0;
	}

	if (data->fl_torch_flags) {
		dev_err(lcdev->dev,
			"Disable all leds torch [%lu]\n", data->fl_torch_flags);
		return -EINVAL;
	}

	val = enable ? mtcdev->source_enable_mask : 0;
	rv = regmap_update_bits(data->regmap, mtcdev->source_enable_reg,
				mtcdev->source_enable_mask, val);
	if (rv)
		return rv;

	if (enable) {
		set_bit(mtcdev->idx, &data->fl_strb_flags);
		mtcdev->faults = 0;
	} else
		clear_bit(mtcdev->idx, &data->fl_strb_flags);

	return 0;
}

static const struct v4l2_flash_ops v4l2_flash_ops = {
	.external_strobe_set = mt6362_fled_external_strobe_set,
};

#define MT6362_INDICATOR_DESC(_id) \
{\
	.cdev = {\
		.name = "mt6362_isink" #_id,\
		.max_brightness = MT6362_ILED##_id##_BRIGHTMAX,\
		.brightness_set_blocking = mt6362_iled_brightness_set,\
		.brightness_get = mt6362_iled_brightness_get,\
		.blink_set = mt6362_iled_blink_set,\
	},\
	.idx		= MT6362_INDICATOR_LED##_id,\
	.enable_reg	= MT6362_REG_RGBEN,\
	.enable_mask	= MT6362_ILED##_id##EN_MASK,\
	.currsel_reg	= MT6362_REG_ILED##_id##CURR,\
	.currsel_mask	= MT6362_ILED##_id##CURR_MASK,\
	.mode_reg	= MT6362_REG_ILED##_id##MODE,\
	.mode_mask	= MT6362_ILED##_id##MODE_MASK,\
}

/* MT6362 only support ISINK1 -> CHGIND, ISINK4 -> MoonLight */
static const struct mt6362_indicator_cdev default_ileds[] = {
	MT6362_INDICATOR_DESC(1),
	MT6362_INDICATOR_DESC(4),
};

#define MT6362_FLASH_DESC(_id) \
{\
	.flash = {\
		.led_cdev = {\
			.name = "mt6362_flash_ch" #_id,\
			.max_brightness = 7,\
			.brightness_set_blocking = mt6362_fled_brightness_set,\
			.brightness_get = mt6362_fled_brightness_get,\
			.flags = LED_DEV_CAP_FLASH,\
		},\
		.brightness = {\
			.min	= MT6362_STRBCURR_MIN,\
			.step	= MT6362_STRBCURR_STEP,\
			.max	= MT6362_STRBCURR_MAX,\
		},\
		.timeout = {\
			.min	= MT6362_STRBTIMEOUT_MIN,\
			.step	= MT6362_STRBTIMEOUT_STEP,\
			.max	= MT6362_STRBTIMEOUT_MAX,\
		},\
		.ops = &mt6362_fled_ctrl_ops,\
	},\
	.idx			= MT6362_FLASH_LED##_id,\
	.source_enable_reg	= MT6362_REG_FLEDEN,\
	.source_enable_mask	= MT6362_FLED##_id##CSEN_MASK,\
	.torch_bright_reg	= MT6362_REG_FLED##_id##ITORCH,\
	.torch_bright_mask	= MT6362_FLEDITORCH_MASK,\
	.strobe_bright_reg	= MT6362_REG_FLED##_id##ISTRB,\
	.strobe_bright_mask	= MT6362_FLEDISTRB_MASK,\
}

static const struct mt6362_flash_cdev default_fleds[] = {
	MT6362_FLASH_DESC(1),
	MT6362_FLASH_DESC(2),
};

#define MT6362_FLED_IRQH(_name, _ledbits, _event) \
static irqreturn_t  mt6362_##_name##_irq_handler(int irq, void *irqdata)\
{\
	struct mt6362_leds_data *data = irqdata;\
	unsigned long ledbits = _ledbits, offset;\
	for_each_set_bit(offset, &ledbits, MT6362_FLASH_LEDMAX) {\
		struct mt6362_flash_cdev *mtcdev = data->flashleds + offset;\
		mtcdev->faults |= _event;\
	} \
	return IRQ_HANDLED;\
}

MT6362_FLED_IRQH(fled_lvf_evt, BIT(MT6362_FLASH_LED1) | BIT(MT6362_FLASH_LED2),
		 LED_FAULT_OVER_CURRENT);
MT6362_FLED_IRQH(fled_lbp_evt, BIT(MT6362_FLASH_LED1) | BIT(MT6362_FLASH_LED2),
		 LED_FAULT_INPUT_VOLTAGE);
MT6362_FLED_IRQH(fled_chgvinovp_evt,
		 BIT(MT6362_FLASH_LED1) | BIT(MT6362_FLASH_LED2),
		 LED_FAULT_INPUT_VOLTAGE);
MT6362_FLED_IRQH(fled1_short_evt, BIT(MT6362_FLASH_LED1),
		 LED_FAULT_SHORT_CIRCUIT);
MT6362_FLED_IRQH(fled2_short_evt, BIT(MT6362_FLASH_LED2),
		 LED_FAULT_SHORT_CIRCUIT);
MT6362_FLED_IRQH(fled1_strbto_evt, BIT(MT6362_FLASH_LED1), LED_FAULT_TIMEOUT);
MT6362_FLED_IRQH(fled2_strbto_evt, BIT(MT6362_FLASH_LED2), LED_FAULT_TIMEOUT);

#define MT6362_IRQ_DECLARE(_name) \
{\
	.name = #_name,\
	.irqh = mt6362_##_name##_irq_handler,\
}

static const struct mt6362_led_irqt irqts[] = {
	MT6362_IRQ_DECLARE(fled_lvf_evt),
	MT6362_IRQ_DECLARE(fled_lbp_evt),
	MT6362_IRQ_DECLARE(fled_chgvinovp_evt),
	MT6362_IRQ_DECLARE(fled1_short_evt),
	MT6362_IRQ_DECLARE(fled2_short_evt),
	MT6362_IRQ_DECLARE(fled1_strbto_evt),
	MT6362_IRQ_DECLARE(fled2_strbto_evt),
};

static int mt6362_leds_irq_register(struct platform_device *pdev,
					  void *irqdata)
{
	int i, irq, rv;

	for (i = 0; i < ARRAY_SIZE(irqts); i++) {
		irq = platform_get_irq_byname(pdev, irqts[i].name);
		if (irq <= 0)
			continue;

		rv = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					       irqts[i].irqh, 0, NULL, irqdata);
		if (rv) {
			dev_err(&pdev->dev,
				"failed to request irq [%s]\n", irqts[i].name);
			return rv;
		}
	}

	return 0;
}

static void mt6362_init_v4l2_flash_config(struct led_classdev_flash *flcdev,
					 struct v4l2_flash_config *v4l2_config)
{
	struct led_classdev *lcdev = &flcdev->led_cdev;
	struct led_flash_setting *s = &v4l2_config->intensity;

	snprintf(v4l2_config->dev_name,
		 sizeof(v4l2_config->dev_name), "%s", lcdev->name);

	s->min = MT6362_TORCHCURR_MIN;
	s->step = MT6362_TORCHCURR_STEP;
	s->val = s->max = MT6362_TORCHCURR_MIN +
			(lcdev->max_brightness - 1) * MT6362_TORCHCURR_STEP;

	v4l2_config->flash_faults = LED_FAULT_OVER_CURRENT |
				LED_FAULT_INPUT_VOLTAGE |
				LED_FAULT_SHORT_CIRCUIT | LED_FAULT_TIMEOUT;

	v4l2_config->has_external_strobe = 1;
}

static void mt6362_init_flash_config(struct led_classdev_flash *flcdev)
{
	struct led_flash_setting *s;

	s = &flcdev->brightness;
	s->val = s->max;

	s = &flcdev->timeout;
	s->val = s->max;
}

static enum led_brightness mt6362_indicator_brightness_level(unsigned int id,
							     int max_uA)
{
	int rv;

	switch (id) {
	default:
	case MT6362_INDICATOR_LED1:
		if (max_uA > MT6362_ILED1_MAXUA)
			max_uA = MT6362_ILED1_MAXUA;
		rv = max_uA / MT6362_ILED1_STEPUA;
		break;
	case MT6362_INDICATOR_LED4:
		if (max_uA > MT6362_ILED4_MAXUA)
			max_uA = MT6362_ILED4_MAXUA;
		rv = max_uA / MT6362_ILED4_STEPUA;
		break;
	}

	/* 0 -> off, plus 1 to return back */
	return (rv + 1);
}

static enum led_brightness mt6362_torch_brightness_level(unsigned int id,
							 int max_uA)
{
	if (max_uA > MT6362_TORCHCURR_MAX)
		max_uA = MT6362_TORCHCURR_MAX;

	/* 0 -> off, plus 1 to return back */
	return (max_uA - MT6362_TORCHCURR_MIN) / MT6362_TORCHCURR_STEP + 1;
}


#ifdef CONFIG_MTK_FLASHLIGHT
static int mt6362_set_scenario(int scenario)
{
	/* notify charger to increase or decrease voltage */
	if (!flashlight_charger_consumer) {
		pr_info("Failed with no charger consumer handler.\n");
		return -1;
	}

	mutex_lock(&mt6362_mutex);
	if (scenario & FLASHLIGHT_SCENARIO_CAMERA_MASK) {
		if (!is_decrease_voltage) {
#ifdef CONFIG_MTK_CHARGER
			pr_info("Decrease voltage level.\n");
			charger_manager_enable_high_voltage_charging(
					flashlight_charger_consumer, false);
#endif
			is_decrease_voltage = 1;
		}
	} else {
		if (is_decrease_voltage) {
#ifdef CONFIG_MTK_CHARGER
			pr_info("Increase voltage level.\n");
			charger_manager_enable_high_voltage_charging(
					flashlight_charger_consumer, true);
#endif
			is_decrease_voltage = 0;
		}
	}
	mutex_unlock(&mt6362_mutex);

	return 0;
}

static int mt6362_open(void)
{
	mutex_lock(&mt6362_mutex);
	fd_use_count++;
	pr_debug("open driver: %d\n", fd_use_count);
	mutex_unlock(&mt6362_mutex);
	return 0;
}

static int mt6362_release(void)
{
	mutex_lock(&mt6362_mutex);
	fd_use_count--;
	pr_debug("close driver: %d\n", fd_use_count);
	/* If camera NE, we need to enable pe by ourselves*/
	if (fd_use_count == 0 && is_decrease_voltage) {
#ifdef CONFIG_MTK_CHARGER
		pr_info("Increase voltage level.\n");
		charger_manager_enable_high_voltage_charging(
				flashlight_charger_consumer, true);
#endif
		is_decrease_voltage = 0;
	}
	mutex_unlock(&mt6362_mutex);
	return 0;
}

static int mt6362_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	if (channel >= MT6362_FLASH_LEDMAX || channel < 0) {
		pr_info("Failed with error channel\n");
		return -EINVAL;
	}

	flcdev = mt6362_flash_class[channel];
	if (flcdev == NULL) {
		pr_info("Get flcdev failed\n");
		return -EINVAL;
	}

	lcdev = &flcdev->led_cdev;
	if (lcdev == NULL) {
		pr_info("Get lcdev failed\n");
		return -EINVAL;
	}

	switch (cmd) {
	case FLASH_IOC_SET_ONOFF:
		pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6362_fled_brightness_set(lcdev, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6362_set_scenario(fl_arg->arg);
		break;

	default:
		dev_info(lcdev->dev, "No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;

	}
	return 0;
}

static ssize_t mt6362_strobe_store(struct flashlight_arg arg)
{
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	flcdev = mt6362_flash_class[arg.channel];
	lcdev = &flcdev->led_cdev;
	mt6362_fled_brightness_set(lcdev, 1);
	msleep(arg.dur);
	mt6362_fled_brightness_set(lcdev, 0);
	return 0;
}

static int mt6362_set_driver(int set)
{
	return 0;
}

static struct flashlight_operations mt6362_ops = {
	mt6362_open,
	mt6362_release,
	mt6362_ioctl,
	mt6362_strobe_store,
	mt6362_set_driver
};
#endif

static int mt6362_leds_parse_dt(struct platform_device *pdev,
				struct mt6362_leds_data *data)
{
	struct device_node *np = pdev->dev.of_node, *child;
	const char *iled_name = "indicator", *fled_name = "flash";
	int rv;

	dev_info(&pdev->dev, "mt6362 led parse dt\n");
	if (!np)
		return 0;

	/* indicator led dt parsing */
	for (child = of_get_child_by_name(np, iled_name); child;
			child = of_find_node_by_name(child, iled_name)) {
		struct mt6362_indicator_cdev *mtcdev;
		u32 reg = 0, max_uA = 0;

		rv = of_property_read_u32(child, "reg", &reg);
		if (rv)
			continue;
		if (reg >= MT6362_INDICATOR_LEDMAX) {
			dev_err(&pdev->dev, "not valid reg property\n");
			return -EINVAL;
		}
		mtcdev = data->indicators + reg;
		mtcdev->np = child;
		of_property_read_string(child, "label", &mtcdev->cdev.name);
		of_property_read_string(child, "linux,default-trigger",
					&mtcdev->cdev.default_trigger);
		rv = of_property_read_u32(child, "led-max-microamp", &max_uA);
		if (rv == 0) {
			mtcdev->cdev.max_brightness =
				mt6362_indicator_brightness_level(reg, max_uA);
		}
	}

	for (child = of_get_child_by_name(np, fled_name); child;
			child = of_find_node_by_name(child, fled_name)) {
		struct mt6362_flash_cdev *mtcdev;
		struct led_classdev_flash *flcdev;
		u32 reg = 0, max_uA = 0;

		rv = of_property_read_u32(child, "reg", &reg);
		if (rv)
			continue;
		if (reg >= MT6362_FLASH_LEDMAX) {
			dev_err(&pdev->dev, "not valid reg property\n");
			return -EINVAL;
		}
		mtcdev = data->flashleds + reg;
		flcdev = &mtcdev->flash;
		mtcdev->np = child;
		of_property_read_string(child, "label", &flcdev->led_cdev.name);
		of_property_read_string(child, "linux,default-trigger",
					&flcdev->led_cdev.default_trigger);
		rv = of_property_read_u32(child, "led-max-microamp", &max_uA);
		if (rv == 0) {
			flcdev->led_cdev.max_brightness =
				mt6362_torch_brightness_level(reg, max_uA);
		}
		of_property_read_u32(child, "flash-max-microamp",
				     &flcdev->brightness.max);
		of_property_read_u32(child, "flash-max-timeout-us",
				     &flcdev->timeout.max);
#ifdef CONFIG_MTK_FLASHLIGHT
		of_property_read_u32(child, "type", &mtcdev->dev_id.type);
		of_property_read_u32(child, "ct", &mtcdev->dev_id.ct);
		of_property_read_u32(child, "part", &mtcdev->dev_id.part);
		snprintf(mtcdev->dev_id.name, FLASHLIGHT_NAME_SIZE,
				flcdev->led_cdev.name);
		mtcdev->dev_id.channel = reg;
		mt6362_flash_class[reg] = flcdev;
		mtcdev->dev_id.decouple = 0;
		dev_info(&pdev->dev,
				"Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				mtcdev->dev_id.type, mtcdev->dev_id.ct,
				mtcdev->dev_id.part, mtcdev->dev_id.name,
				mtcdev->dev_id.channel,
				mtcdev->dev_id.decouple);
		if (flashlight_dev_register_by_device_id(&mtcdev->dev_id,
			&mt6362_ops))
			return -EFAULT;
#endif
	}

	dev_info(&pdev->dev, "mt6362 led parse dt done\n");
	return 0;
}

static int mt6362_leds_probe(struct platform_device *pdev)
{
	struct mt6362_leds_data *data;
	int i, rv;

	dev_info(&pdev->dev, "mt6362 led probe\n");
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &pdev->dev;
	memcpy(data->indicators, default_ileds, sizeof(default_ileds));
	memcpy(data->flashleds, default_fleds, sizeof(default_fleds));

	platform_set_drvdata(pdev, data);

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	rv = mt6362_leds_parse_dt(pdev, data);
	if (rv) {
		dev_err(&pdev->dev, "faled to parse dt\n");
		return rv;
	}

	/* indicator led register */
	for (i = 0; i < MT6362_INDICATOR_LEDMAX; i++) {
		struct mt6362_indicator_cdev *mtcdev = data->indicators + i;

		rv = devm_of_led_classdev_register(&pdev->dev,
						   mtcdev->np, &mtcdev->cdev);
		if (rv) {
			dev_err(&pdev->dev, "failed to register %d ileds\n", i);
			return rv;
		}
	}

	/* flash led register */
	for (i = 0; i < MT6362_FLASH_LEDMAX; i++) {
		struct mt6362_flash_cdev *mtcdev = data->flashleds + i;
		struct led_classdev_flash *flcdev = &mtcdev->flash;
		struct v4l2_flash_config v4l2_config;

		/* config strobe default bright to mininum 25mA */
		rv = regmap_write(data->regmap, mtcdev->strobe_bright_reg,
				  MT6362_FLEDUISTRB_MASK);
		if (rv)
			return rv;

		mt6362_init_flash_config(flcdev);

		rv = led_classdev_flash_register(&pdev->dev, flcdev);
		if (rv) {
			dev_err(&pdev->dev, "failed to register %d fleds\n", i);
			return rv;
		}

		mt6362_init_v4l2_flash_config(flcdev, &v4l2_config);

		mtcdev->v4l2_flash = v4l2_flash_init(&pdev->dev,
						of_fwnode_handle(mtcdev->np),
						&mtcdev->flash, &v4l2_flash_ops,
						&v4l2_config);
		if (IS_ERR(mtcdev->v4l2_flash)) {
			dev_err(&pdev->dev, "failed to register %d v4l2\n", i);
			rv = PTR_ERR(mtcdev->v4l2_flash);
			return rv;
		}
	}

#ifdef CONFIG_MTK_FLASHLIGHT
	/* clear attributes */
	fd_use_count = 0;
	is_decrease_voltage = 0;

	/* get charger consumer manager */
	flashlight_charger_consumer = charger_manager_get_by_name(
			&pdev->dev, CHARGER_SUPPLY_NAME);
	if (!flashlight_charger_consumer) {
		pr_info("Failed to get charger manager.\n");
		return -EFAULT;
	}
#endif

	rv = mt6362_leds_irq_register(pdev, data);
	if (rv) {
		dev_err(&pdev->dev, "failed to register led irqs\n");
		return rv;
	}

#ifdef CONFIG_MTK_CHARGER
	data->chg_dev = get_charger_by_name("primary_chg");
	if (!data->chg_dev) {
		dev_err(&pdev->dev,
			"%s: can't find primary charger\n", __func__);
		return -EINVAL;
	}
#endif

	dev_info(&pdev->dev, "mt6362 probe done\n");
	return 0;
}

static int mt6362_leds_remove(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_FLASHLIGHT
	struct mt6362_leds_data *data = platform_get_drvdata(pdev);
	struct mt6362_flash_cdev *mtcdev;

	mtcdev = data->flashleds + MT6362_FLASH_LED1;
	flashlight_dev_unregister_by_device_id(&mtcdev->dev_id);

	mtcdev = data->flashleds + MT6362_FLASH_LED2;
	flashlight_dev_unregister_by_device_id(&mtcdev->dev_id);
#endif
	return 0;
}

static const struct of_device_id __maybe_unused mt6362_leds_ofid_tbls[] = {
	{ .compatible = "mediatek,mt6362-leds", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt6362_leds_ofid_tbls);

static struct platform_driver mt6362_leds_driver = {
	.driver = {
		.name = "mt6362-leds",
		.of_match_table = of_match_ptr(mt6362_leds_ofid_tbls),
	},
	.probe = mt6362_leds_probe,
	.remove = mt6362_leds_remove,
};
module_platform_driver(mt6362_leds_driver);

MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6362 SPMI LEDS Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
