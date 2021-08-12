// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/led-class-flash.h>
#include <media/v4l2-flash-led-class.h>

#include <linux/mfd/mt6360-private.h>

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
#include "flashlight-core.h"

#include <linux/power_supply.h>
#endif

enum {
	MT6360_LED_ISINK1 = 0,
	MT6360_LED_ISINK2,
	MT6360_LED_ISINK3,
	MT6360_LED_ISINK4,
	MT6360_LED_MAX,
};

enum {
	MT6360_LEDMODE_PWM = 0,
	MT6360_LEDMODE_BREATH,
	MT6360_LEDMODE_CC,
	MT6360_LEDMODE_MAX,
};

enum {
	MT6360_FLED_CH1 = 0,
	MT6360_FLED_CH2,
	MT6360_FLED_MAX,
};

/* ILED setting/reg */
#define MT6360_SINKCUR_MAX1	(0x0d)
#define MT6360_SINKCUR_MAX2	(0x0d)
#define MT6360_SINKCUR_MAX3	(0x0d)
#define MT6360_SINKCUR_MAX4	(0x1f)
#define MT6360_CURRSEL_REG1	(MT6360_PMU_RGB1_ISNK)
#define MT6360_CURRSEL_REG2	(MT6360_PMU_RGB2_ISNK)
#define MT6360_CURRSEL_REG3	(MT6360_PMU_RGB3_ISNK)
#define MT6360_CURRSEL_REG4	(MT6360_PMU_RGB_ML_ISNK)
#define MT6360_CURRSEL_MASK1	(0x0f)
#define MT6360_CURRSEL_MASK2	(0x0f)
#define MT6360_CURRSEL_MASK3	(0x0f)
#define MT6360_CURRSEL_MASK4	(0x1f)
#define MT6360_LEDMODE_REG1	(MT6360_PMU_RGB1_ISNK)
#define MT6360_LEDMODE_REG2	(MT6360_PMU_RGB2_ISNK)
#define MT6360_LEDMODE_REG3	(MT6360_PMU_RGB3_ISNK)
#define MT6360_LEDMODE_REG4	(0)
#define MT6360_LEDMODE_MASK1	(0xc0)
#define MT6360_LEDMODE_MASK2	(0xc0)
#define MT6360_LEDMODE_MASK3	(0xc0)
#define MT6360_LEDMODE_MASK4	(0)
#define MT6360_PWMDUTY_REG1	(MT6360_PMU_RGB1_DIM)
#define MT6360_PWMDUTY_REG2	(MT6360_PMU_RGB2_DIM)
#define MT6360_PWMDUTY_REG3	(MT6360_PMU_RGB3_DIM)
#define MT6360_PWMDUTY_REG4	(0)
#define MT6360_PWMDUTY_MASK1	(0xff)
#define MT6360_PWMDUTY_MASK2	(0xff)
#define MT6360_PWMDUTY_MASK3	(0xff)
#define MT6360_PWMDUTY_MASK4	(0)
#define MT6360_PWMFREQ_REG1	(MT6360_PMU_RGB12_Freq)
#define MT6360_PWMFREQ_REG2	(MT6360_PMU_RGB12_Freq)
#define MT6360_PWMFREQ_REG3	(MT6360_PMU_RGB34_Freq)
#define MT6360_PWMFREQ_REG4	(0)
#define MT6360_PWMFREQ_MASK1	(0xe0)
#define MT6360_PWMFREQ_MASK2	(0x1c)
#define MT6360_PWMFREQ_MASK3	(0xe0)
#define MT6360_PWMFREQ_MASK4	(0)
#define MT6360_BREATH_REGBASE1	(MT6360_PMU_RGB1_Tr)
#define MT6360_BREATH_REGBASE2	(MT6360_PMU_RGB2_Tr)
#define MT6360_BREATH_REGBASE3	(MT6360_PMU_RGB3_Tr)
#define MT6360_BREATH_REGBASE4	(0)
#define MT6360_LEDEN_MASK1	(0x80)
#define MT6360_LEDEN_MASK2	(0x40)
#define MT6360_LEDEN_MASK3	(0x20)
#define MT6360_LEDEN_MASK4	(0x10)
#define MT6360_LEDEN_REG	(MT6360_PMU_RGB_EN)
#define MT6360_LEDALLEN_MASK	(0xf0)

#define MT6360_CHRIND_MASK	(0x08)

/* pattern order -> toff, tr1, tr2, ton, tf1, tf2 */
#define MT6360_BRPATTERN_NUM	(6)
#define MT6360_BREATHREG_NUM	(3)

/* FLED setting */
#define MT6360_CSENABLE_REG1	(MT6360_PMU_FLED_EN)
#define MT6360_CSENABLE_MASK1	(0x02)
#define MT6360_CSENABLE_REG2	(MT6360_PMU_FLED_EN)
#define MT6360_CSENABLE_MASK2	(0x01)
#define MT6360_TORBRIGHT_MAX1	(0x1f)
#define MT6360_TORBRIGHT_MAX2	(0x1f)
#define MT6360_TORBRIGHT_REG1	(MT6360_PMU_FLED1_TOR_CTRL)
#define MT6360_TORBRIGHT_MASK1	(0x1f)
#define MT6360_STRBRIGHT_REG1	(MT6360_PMU_FLED1_STRB_CTRL2)
#define MT6360_STRBRIGHT_MASK1	(0x7f)
#define MT6360_TORBRIGHT_REG2	(MT6360_PMU_FLED2_TOR_CTRL)
#define MT6360_TORBRIGHT_MASK2	(0x1f)
#define MT6360_STRBRIGHT_REG2	(MT6360_PMU_FLED2_STRB_CTRL2)
#define MT6360_STRBRIGHT_MASK2	(0x7f)
#define MT6360_TORENABLE_REG1	(MT6360_PMU_FLED_EN)
#define MT6360_TORENABLE_MASK1	(0x08)
#define MT6360_TORENABLE_REG2	(MT6360_PMU_FLED_EN)
#define MT6360_TORENABLE_MASK2	(0x08)
#define MT6360_STRBENABLE_REG1	(MT6360_PMU_FLED_EN)
#define MT6360_STRBENABLE_MASK1 (0x06)
#define MT6360_STRBENABLE_REG2	(MT6360_PMU_FLED_EN)
#define MT6360_STRBENABLE_MASK2 (0x04)
#define MT6360_STRBTIMEOUT_REG	(MT6360_PMU_FLED_STRB_CTRL)
#define MT6360_STRBTIMEOUT_MASK	(0x7f)
#define MT6360_TORCHCUR_MIN	(25000)
#define MT6360_TORCHCUR_STEP	(12500)
#define MT6360_TORCHCUR_MAX	(400000)
#define MT6360_STROBECUR_MIN	(50000)
#define MT6360_STROBECUR_STEP	(12500)
#define MT6360_STROBECUR_MAX	(1500000)
#define MT6360_STRBTIMEOUT_MIN	(64000)
#define MT6360_STRBTIMEOUT_STEP	(32000)
#define MT6360_STRBTIMEOUT_MAX	(2432000)

#define MT6360_FLEDSUPPORT_FAULTS	(LED_FAULT_UNDER_VOLTAGE |\
					 LED_FAULT_SHORT_CIRCUIT |\
					 LED_FAULT_INPUT_VOLTAGE |\
					 LED_FAULT_TIMEOUT)

/* debug info */
#define MT6360_PMU_CHG_CTRL1	(0x11)
#define MT6360_PMU_CHG_CTRL2	(0x12)
#define MT6360_MASK_HZ_EN	(0x04)
#define MT6360_MASK_CFO_EN	(0x02)
#define MT6360_FLED_CHG_VINOVP	(MT6360_PMU_CHG_MASK2)

struct mt6360_led_platform_data {
	u32 rgbon_sync;
	u32 fled1_ultraistrb;
	u32 fled2_ultraistrb;
};

struct breath_element_cfg {
	/* base, step in ms */
	unsigned int base;
	unsigned int step;
	unsigned int maxval;
	unsigned int reg_offset;
	unsigned int reg_mask;
};

struct mt6360_led_classdev {
	struct led_classdev cdev;
	int index;
	struct device_node *np;
	unsigned int currsel_reg;
	unsigned int currsel_mask;
	unsigned int enable_mask;
	unsigned int mode_reg;
	unsigned int mode_mask;
	unsigned int pwmduty_reg;
	unsigned int pwmduty_mask;
	unsigned int pwmfreq_reg;
	unsigned int pwmfreq_mask;
	unsigned int breath_regbase;
};

struct mt6360_fled_classdev {
	struct led_classdev_flash fl_cdev;
	int index;
	struct v4l2_flash *v4l2_flash;
	struct device_node *np;
	unsigned int cs_enable_reg;
	unsigned int cs_enable_mask;
	unsigned int torch_bright_reg;
	unsigned int torch_bright_mask;
	unsigned int torch_enable_reg;
	unsigned int torch_enable_mask;
	unsigned int strobe_bright_reg;
	unsigned int strobe_bright_mask;
	unsigned int strobe_enable_reg;
	unsigned int strobe_enable_mask;
	unsigned int strobe_external_reg;
	unsigned int strobe_external_mask;
	u32 faults;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	struct flashlight_device_id dev_id;
#endif
};

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
static struct led_classdev_flash *mt6360_flash_class[MT6360_FLED_MAX];

/* is decrease voltage */
static int is_decrease_voltage;
static DEFINE_MUTEX(mt6360_mutex);

/* define usage count */
static int fd_use_count;
#endif

struct mt6360_led_info {
	struct device *dev;
	struct mt6360_led_platform_data *pdata;
	struct regmap *regmap;
	struct mt6360_led_classdev mtled_cdev[MT6360_LED_MAX];
	struct mt6360_fled_classdev mtfled_cdev[MT6360_FLED_MAX];
	unsigned long fl_torch_flags;
	unsigned long fl_strobe_flags;
};

static const struct mt6360_led_platform_data def_platform_data = {
	.rgbon_sync = 0,
	.fled1_ultraistrb = 1,
	.fled2_ultraistrb = 1,
};

static int mt6360_led_brightness_set(struct led_classdev *cdev,
				      enum led_brightness brightness)
{
	struct mt6360_led_classdev *mtled_cdev =
					     (struct mt6360_led_classdev *)cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(cdev->dev->parent);
	int shift, sync_regval = 0, ret;

	dev_dbg(cdev->dev, "%s, bright %d\n", __func__, brightness);
	/* if isink1 user control, set chrind function to sw mode */
	if (mtled_cdev->index == MT6360_LED_ISINK1) {
		ret = regmap_update_bits(mli->regmap,
				   MT6360_PMU_RGB_EN, MT6360_CHRIND_MASK, 0xff);
		if (ret < 0)
			dev_err(cdev->dev, "disable chrind func fail\n");
	}
	if (brightness == LED_OFF) {
		ret = regmap_update_bits(mli->regmap,
				  MT6360_LEDEN_REG, mtled_cdev->enable_mask, 0);
		if (ret < 0)
			return ret;
		if (mtled_cdev->mode_reg == 0)
			goto out_bright_set;
		/* if off, force config to cc_mode */
		shift = ffs(mtled_cdev->mode_mask) - 1;
		ret = regmap_update_bits(mli->regmap, mtled_cdev->mode_reg,
			     mtled_cdev->mode_mask, MT6360_LEDMODE_CC << shift);
		if (ret < 0)
			dev_err(cdev->dev, "config cc mode fail\n");
		goto out_bright_set;
	}
	shift = ffs(mtled_cdev->currsel_mask) - 1;
	brightness -= 1;
	ret = regmap_update_bits(mli->regmap, mtled_cdev->currsel_reg,
				 mtled_cdev->currsel_mask, brightness << shift);
	if (ret < 0)
		return ret;
	if (mli->pdata->rgbon_sync) {
		ret = regmap_read(mli->regmap, MT6360_LEDEN_REG,  &sync_regval);
		if (ret < 0)
			goto out_bright_set;
		ret = regmap_update_bits(mli->regmap,
				     MT6360_LEDEN_REG, MT6360_LEDALLEN_MASK, 0);
		if (ret < 0)
			goto out_bright_set;
		sync_regval |= mtled_cdev->enable_mask;
		ret = regmap_update_bits(mli->regmap, MT6360_LEDEN_REG,
					 MT6360_LEDALLEN_MASK, sync_regval);
	} else {
		ret = regmap_update_bits(mli->regmap, MT6360_LEDEN_REG,
					 mtled_cdev->enable_mask, 0xff);
	}
out_bright_set:
	return ret;
}

static enum led_brightness mt6360_led_brightness_get(struct led_classdev *cdev)
{
	struct mt6360_led_classdev *mtled_cdev =
					     (struct mt6360_led_classdev *)cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(cdev->dev->parent);
	unsigned int regval = 0;
	int shift = ffs(mtled_cdev->currsel_mask) - 1, ret;

	ret = regmap_read(mli->regmap, MT6360_LEDEN_REG, &regval);
	if (ret < 0) {
		dev_err(cdev->dev, "%s: get enable fail\n", __func__);
		return LED_OFF;
	}
	if (!(regval & mtled_cdev->enable_mask))
		return LED_OFF;
	ret = regmap_read(mli->regmap, mtled_cdev->currsel_reg, &regval);
	if (ret < 0) {
		dev_err(cdev->dev, "%s: get isink fail\n", __func__);
		return LED_OFF;
	}
	regval &= mtled_cdev->currsel_mask;
	regval >>= shift;
	return (regval + 1);
}

static const unsigned int dim_freqs[] = {
	4, 8, 250, 500, 1000, 2000, 4000, 8000,
};

static int mt6360_led_blink_set(struct led_classdev *cdev,
			     unsigned long *delay_on,  unsigned long *delay_off)
{
	struct mt6360_led_classdev *mtled_cdev =
					     (struct mt6360_led_classdev *)cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(cdev->dev->parent);
	int freq, duty, shift, sum, ret;

	if (mtled_cdev->mode_reg == 0)
		return -ENOTSUPP;
	if (*delay_on == 0 && *delay_off == 0)
		*delay_on = *delay_off = 500;
	sum = *delay_on + *delay_off;
	for (freq = 0; freq < ARRAY_SIZE(dim_freqs); freq++) {
		if (sum <= dim_freqs[freq])
			break;
	}
	if (freq == ARRAY_SIZE(dim_freqs)) {
		dev_err(cdev->dev, "exceed pwm frequency max\n");
		return -EINVAL;
	}
	/* invert */
	freq = ARRAY_SIZE(dim_freqs) - 1 - freq;
	dev_dbg(cdev->dev, "freq sel [%d]\n", freq);
	shift = ffs(mtled_cdev->pwmfreq_mask) - 1;
	ret = regmap_update_bits(mli->regmap, mtled_cdev->pwmfreq_reg,
				 mtled_cdev->pwmfreq_mask, freq << shift);
	if (ret < 0) {
		dev_err(cdev->dev, "Failed to set pwmfreq\n");
		return ret;
	}
	duty = 255 * (*delay_on) / sum;
	shift = ffs(mtled_cdev->pwmduty_mask) - 1;
	ret = regmap_update_bits(mli->regmap, mtled_cdev->pwmduty_reg,
				 mtled_cdev->pwmduty_mask, duty << shift);
	if (ret < 0) {
		dev_err(cdev->dev, "Failed to set pwmduty\n");
		return ret;
	}
	dev_dbg(cdev->dev, "final duty [%d]\n", duty);
	shift = ffs(mtled_cdev->mode_mask) - 1;
	ret = regmap_update_bits(mli->regmap, mtled_cdev->mode_reg,
			    mtled_cdev->mode_mask, MT6360_LEDMODE_PWM << shift);
	return ret;
}

#define MT6360_LED_DESC(_id)  {						\
	.cdev = {							\
		.name = "mt6360_isink" #_id,				\
		.max_brightness = MT6360_SINKCUR_MAX##_id,		\
		.brightness_set_blocking = mt6360_led_brightness_set,	\
		.brightness_get = mt6360_led_brightness_get,		\
		.blink_set = mt6360_led_blink_set,			\
	},								\
	.index = MT6360_LED_ISINK##_id,					\
	.currsel_reg = MT6360_CURRSEL_REG##_id,				\
	.currsel_mask = MT6360_CURRSEL_MASK##_id,			\
	.enable_mask = MT6360_LEDEN_MASK##_id,				\
	.mode_reg = MT6360_LEDMODE_REG##_id,				\
	.mode_mask = MT6360_LEDMODE_MASK##_id,				\
	.pwmduty_reg = MT6360_PWMDUTY_REG##_id,				\
	.pwmduty_mask = MT6360_PWMDUTY_MASK##_id,			\
	.pwmfreq_reg = MT6360_PWMFREQ_REG##_id,				\
	.pwmfreq_mask = MT6360_PWMFREQ_MASK##_id,			\
	.breath_regbase = MT6360_BREATH_REGBASE##_id,			\
}

/* ISINK 1/2/3 for RGBLED, ISINK4 for MoonLight */
static const struct mt6360_led_classdev def_led_classdev[MT6360_LED_MAX] = {
	MT6360_LED_DESC(1),
	MT6360_LED_DESC(2),
	MT6360_LED_DESC(3),
	MT6360_LED_DESC(4),
};

static inline bool mt6360_fled_check_flags_if_any(unsigned long *flags)
{
	return (*flags) ? true : false;
}

static int mt6360_fled_strobe_brightness_set(
			   struct led_classdev_flash *fled_cdev, u32 brightness)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct led_flash_setting *fs = &fled_cdev->brightness;
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index, shift;
	u32 val;

	dev_dbg(led_cdev->dev,
		"%s: id[%d], brightness %u\n", __func__, id, brightness);
	val = brightness;
	val = (val - fs->min) / fs->step;
	shift = ffs(mtfled_cdev->strobe_bright_mask) - 1;
	return regmap_update_bits(mli->regmap, mtfled_cdev->strobe_bright_reg,
				 mtfled_cdev->strobe_bright_mask, val << shift);
}

static int mt6360_fled_strobe_brightness_get(
			  struct led_classdev_flash *fled_cdev, u32 *brightness)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct led_flash_setting *fs = &fled_cdev->brightness;
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index, shift, ret;
	u32 regval = 0;

	dev_dbg(led_cdev->dev, "%s: id[%d]\n", __func__, id);
	ret = regmap_read(mli->regmap, mtfled_cdev->strobe_bright_reg, &regval);
	if (ret < 0)
		return ret;
	regval &= mtfled_cdev->strobe_bright_mask;
	shift = ffs(mtfled_cdev->strobe_bright_mask) - 1;
	regval >>= shift;
	/* convert to microamp value */
	*brightness = regval * fs->step + fs->min;
	return 0;
}

static int mt6360_fled_strobe_set(
			       struct led_classdev_flash *fled_cdev, bool state)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index, ret, regval = 0;

	/* Un-mask fled_chg_vinovp */
	ret = regmap_update_bits(mli->regmap,
				 MT6360_FLED_CHG_VINOVP, 0x08, 0);
	if (ret < 0)
		dev_err(led_cdev->dev, "Fail to set fled_chg_vinovp, %d\n", ret);

	dev_notice(led_cdev->dev, "%s: id[%d], state %d\n", __func__, id, state);
	if (!(state ^ test_bit(id, &mli->fl_strobe_flags))) {
		regmap_update_bits(mli->regmap,
				   MT6360_FLED_CHG_VINOVP, 0x08, 0xff);
		dev_dbg(led_cdev->dev,
			"no change for strobe [%lu]\n", mli->fl_strobe_flags);
		return 0;
	}
	if (mt6360_fled_check_flags_if_any(&mli->fl_torch_flags)) {
		dev_err(led_cdev->dev,
			"Disable all leds torch [%lu]\n", mli->fl_torch_flags);
		return -EINVAL;
	}

	if (state == true) {
		ret = regmap_read(mli->regmap, MT6360_PMU_CHG_CTRL1, &regval);
		if (ret < 0)
			return ret;
		if (regval & MT6360_MASK_HZ_EN)
			dev_notice(led_cdev->dev,
				   "%s: strobe with hz mode\n", __func__);

		ret = regmap_read(mli->regmap, MT6360_PMU_CHG_CTRL2, &regval);
		if (ret < 0)
			return ret;
		if (regval & MT6360_MASK_CFO_EN)
			dev_notice(led_cdev->dev,
				   "%s: strobe with cfo_en=0\n", __func__);
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(state);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(led_cdev->dev, "pt is low\n");
		return 0;
	}
#endif

	ret = regmap_update_bits(mli->regmap, mtfled_cdev->cs_enable_reg,
				 mtfled_cdev->cs_enable_mask, state ? 0xff : 0);
	if (ret < 0) {
		dev_err(led_cdev->dev, "Fail to set cs enable [%d]\n", state);
		goto out_strobe_set;
	}
	ret = regmap_update_bits(mli->regmap, mtfled_cdev->strobe_enable_reg,
			     mtfled_cdev->strobe_enable_mask, state ? 0xff : 0);
	if (ret < 0) {
		dev_err(led_cdev->dev, "Fail to set strb enable [%d]\n", state);
		goto out_strobe_set;
	}
	if (state) {
		if (!mt6360_fled_check_flags_if_any(&mli->fl_strobe_flags))
			usleep_range(5000, 6000);
		set_bit(id, &mli->fl_strobe_flags);
		mtfled_cdev->faults = 0;
	} else {
		clear_bit(id, &mli->fl_strobe_flags);
		if (!mt6360_fled_check_flags_if_any(&mli->fl_strobe_flags))
			usleep_range(400, 500);
	}
	regmap_update_bits(mli->regmap,
			   MT6360_FLED_CHG_VINOVP, 0x08, 0xff);
out_strobe_set:
	return ret;
}

static int mt6360_fled_strobe_get(
			      struct led_classdev_flash *fled_cdev, bool *state)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index;

	dev_dbg(led_cdev->dev, "%s: id[%d]\n", __func__, id);
	*state = test_bit(id, &mli->fl_strobe_flags) ? true : false;
	return 0;
}

static int mt6360_fled_strobe_timeout_set(
			      struct led_classdev_flash *fled_cdev, u32 timeout)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct led_flash_setting *ts = &fled_cdev->timeout;
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index, shift, ret;
	u32 regval;

	dev_dbg(led_cdev->dev,
		"%s: id[%d], timeout %u\n", __func__, id, timeout);
	regval = (timeout - ts->min) / ts->step;
	shift = ffs(MT6360_STRBTIMEOUT_MASK) - 1;
	ret = regmap_update_bits(mli->regmap, MT6360_STRBTIMEOUT_REG,
				 MT6360_STRBTIMEOUT_MASK, regval << shift);
	return ret;
}

static int mt6360_fled_strobe_fault_get(
			       struct led_classdev_flash *fled_cdev, u32 *fault)
{
	struct led_classdev *led_cdev = &fled_cdev->led_cdev;
	struct mt6360_fled_classdev *mtfled_cdev = (void *)fled_cdev;
	int id = mtfled_cdev->index;

	dev_dbg(led_cdev->dev, "%s: id[%d]\n", __func__, id);
	*fault = mtfled_cdev->faults;
	return 0;
}

static const struct led_flash_ops mt6360_fled_ops = {
	.flash_brightness_set = mt6360_fled_strobe_brightness_set,
	.flash_brightness_get = mt6360_fled_strobe_brightness_get,
	.strobe_set = mt6360_fled_strobe_set,
	.strobe_get = mt6360_fled_strobe_get,
	.timeout_set = mt6360_fled_strobe_timeout_set,
	.fault_get = mt6360_fled_strobe_fault_get,
};

static int mt6360_fled_brightness_set(struct led_classdev *led_cdev,
				      enum led_brightness brightness)
{
	struct led_classdev_flash *lcf = lcdev_to_flcdev(led_cdev);
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct mt6360_fled_classdev *mtfled_cdev = (void *)lcf;
	int id = mtfled_cdev->index, shift, keep, ret;

	dev_dbg(led_cdev->dev,
		"%s: id [%d], brightness %d\n", __func__, id, brightness);
	if (mt6360_fled_check_flags_if_any(&mli->fl_strobe_flags)) {
		dev_err(led_cdev->dev,
		       "Disable all leds strobe [%lu]\n", mli->fl_strobe_flags);
		return -EINVAL;
	}
	if (brightness == LED_OFF) {
		clear_bit(id, &mli->fl_torch_flags);
		keep = mt6360_fled_check_flags_if_any(&mli->fl_torch_flags);
		ret = regmap_update_bits(mli->regmap,
					 mtfled_cdev->torch_enable_reg,
					 mtfled_cdev->torch_enable_mask,
					 keep ? 0xff : 0);
		if (ret < 0) {
			dev_err(led_cdev->dev, "Fail to set torch disable\n");
			goto out_bright_set;
		}
		ret = regmap_update_bits(mli->regmap,
					 mtfled_cdev->cs_enable_reg,
					 mtfled_cdev->cs_enable_mask, 0);
		if (ret < 0)
			dev_err(led_cdev->dev, "Fail to set torch disable\n");
		goto out_bright_set;
	}

#ifdef CONFIG_MTK_FLASHLIGHT_DLPT
	flashlight_kicker_pbm(1);
#endif
#ifdef CONFIG_MTK_FLASHLIGHT_PT
	if (flashlight_pt_is_low()) {
		dev_info(led_cdev->dev, "pt is low\n");
		return 0;
	}
#endif

	shift = ffs(mtfled_cdev->torch_bright_mask) - 1;
	brightness -= 1;
	ret = regmap_update_bits(mli->regmap, mtfled_cdev->torch_bright_reg,
			   mtfled_cdev->torch_bright_mask, brightness << shift);
	if (ret < 0) {
		dev_err(led_cdev->dev,
			"Fail to set torch bright [%d]\n", brightness);
		goto out_bright_set;
	}
	ret = regmap_update_bits(mli->regmap, mtfled_cdev->cs_enable_reg,
				 mtfled_cdev->cs_enable_mask, 0xff);
	if (ret < 0) {
		dev_err(led_cdev->dev, "Fail to set cs enable\n");
		goto out_bright_set;
	}
	ret = regmap_update_bits(mli->regmap, mtfled_cdev->torch_enable_reg,
				 mtfled_cdev->torch_enable_mask, 0xff);
	set_bit(id, &mli->fl_torch_flags);
out_bright_set:
	return ret;
}

static enum led_brightness mt6360_fled_brightness_get(
						  struct led_classdev *led_cdev)
{
	struct led_classdev_flash *lcf = lcdev_to_flcdev(led_cdev);
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct mt6360_fled_classdev *mtfled_cdev = (void *)lcf;
	int id = mtfled_cdev->index, shift, ret;
	u32 regval = 0;

	dev_dbg(led_cdev->dev, "%s: id [%d]\n", __func__, id);
	if (!test_bit(id, &mli->fl_torch_flags))
		return LED_OFF;
	ret = regmap_read(mli->regmap, mtfled_cdev->torch_bright_reg, &regval);
	if (ret < 0) {
		dev_err(led_cdev->dev, "%s: Fail to get torb reg\n", __func__);
		return LED_OFF;
	}
	shift = ffs(mtfled_cdev->torch_bright_mask) - 1;
	regval &= mtfled_cdev->torch_bright_mask;
	regval >>= shift;
	return (regval + 1);
}

#define MT6360_FLED_DESC(_id)  {					\
	.fl_cdev = {							\
	 .led_cdev = {							\
		.name = "mt6360_fled_ch" #_id,				\
		.max_brightness = MT6360_TORBRIGHT_MAX##_id,		\
		.brightness_set_blocking =  mt6360_fled_brightness_set,	\
		.brightness_get = mt6360_fled_brightness_get,		\
		.flags = LED_DEV_CAP_FLASH,				\
	 },								\
	 .brightness = {						\
		.min = MT6360_STROBECUR_MIN,				\
		.step = MT6360_STROBECUR_STEP,				\
		.max = MT6360_STROBECUR_MAX,				\
		.val = MT6360_STROBECUR_MIN,				\
	 },								\
	 .timeout = {							\
		.min = MT6360_STRBTIMEOUT_MIN,				\
		.step = MT6360_STRBTIMEOUT_STEP,			\
		.max = MT6360_STRBTIMEOUT_MAX,				\
		.val = MT6360_STRBTIMEOUT_MIN,				\
	 },								\
	 .ops = &mt6360_fled_ops,					\
	},								\
	.index = MT6360_FLED_CH##_id,					\
	.cs_enable_reg = MT6360_CSENABLE_REG##_id,			\
	.cs_enable_mask = MT6360_CSENABLE_MASK##_id,			\
	.torch_bright_reg = MT6360_TORBRIGHT_REG##_id,			\
	.torch_bright_mask = MT6360_TORBRIGHT_MASK##_id,		\
	.torch_enable_reg = MT6360_TORENABLE_REG##_id,			\
	.torch_enable_mask = MT6360_TORENABLE_MASK##_id,		\
	.strobe_bright_reg = MT6360_STRBRIGHT_REG##_id,			\
	.strobe_bright_mask = MT6360_STRBRIGHT_MASK##_id,		\
	.strobe_enable_reg = MT6360_STRBENABLE_REG##_id,		\
	.strobe_enable_mask = MT6360_STRBENABLE_MASK##_id,		\
}

static const struct mt6360_fled_classdev def_fled_classdev[MT6360_FLED_MAX] = {
	MT6360_FLED_DESC(1),
	MT6360_FLED_DESC(2),
};

#if IS_ENABLED(CONFIG_V4L2_FLASH_LED_CLASS)
static int mt6360_fled_external_strobe_set(
				     struct v4l2_flash *v4l2_flash, bool enable)
{
	struct led_classdev_flash *lcf = v4l2_flash->fled_cdev;
	struct led_classdev *led_cdev = &lcf->led_cdev;
	struct mt6360_led_info *mli = dev_get_drvdata(led_cdev->dev->parent);
	struct mt6360_fled_classdev *mtfled_cdev = (void *)lcf;
	int id = mtfled_cdev->index, ret;

	dev_dbg(led_cdev->dev, "%s: id[%d], %d\n", __func__, id, enable);
	if (!(enable ^ test_bit(id, &mli->fl_strobe_flags))) {
		dev_dbg(led_cdev->dev,
			"no change for strobe [%lu]\n", mli->fl_strobe_flags);
		return 0;
	}
	if (mt6360_fled_check_flags_if_any(&mli->fl_torch_flags)) {
		dev_err(led_cdev->dev,
			"Disable all leds torch [%lu]\n", mli->fl_torch_flags);
		return -EINVAL;
	}
	ret = regmap_update_bits(mli->regmap, mtfled_cdev->cs_enable_reg,
			  mtfled_cdev->cs_enable_mask, enable ? 0xff : 0);
	if (enable) {
		set_bit(id, &mli->fl_strobe_flags);
		mtfled_cdev->faults = 0;
	} else
		clear_bit(id, &mli->fl_strobe_flags);
	return ret;
}

static const struct v4l2_flash_ops v4l2_flash_ops = {
	.external_strobe_set = mt6360_fled_external_strobe_set,
};

static void mt6360_init_v4l2_flash_config(
				       struct mt6360_fled_classdev *mtfled_cdev,
				       struct v4l2_flash_config *config)
{
	struct led_flash_setting *torch_intensity = &config->intensity;
	struct led_classdev *led_cdev = &(mtfled_cdev->fl_cdev.led_cdev);
	s32 val;
	int ret = 0;

	ret = snprintf(config->dev_name, sizeof(config->dev_name),
		 "%s", mtfled_cdev->fl_cdev.led_cdev.name);
	if ((ret < 0) || (ret >= sizeof(config->dev_name))) {
		dev_notice(led_cdev->dev, "%s:fail,ret = %d\n", __func__, ret);
		return;
	}

	torch_intensity->min = MT6360_TORCHCUR_MIN;
	torch_intensity->step = MT6360_TORCHCUR_STEP;
	val = MT6360_TORCHCUR_MIN;
	val += ((led_cdev->max_brightness - 1) * MT6360_TORCHCUR_STEP);
	torch_intensity->val = torch_intensity->max = val;
	config->flash_faults |= MT6360_FLEDSUPPORT_FAULTS;
	config->has_external_strobe = 1;
}
#else
static const struct v4l2_flash_ops v4l2_flash_ops;

static void mt6360_init_v4l2_flash_config(
				       struct mt6360_fled_classdev *mtfled_cdev,
				       struct v4l2_flash_config *config)
{
}
#endif /* IS_ENABLED(CONFIG_V4L2_FLASH_LED_CLASS) */

static irqreturn_t mt6360_pmu_fled_lvf_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_err(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH1].faults |= LED_FAULT_UNDER_VOLTAGE;
	mli->mtfled_cdev[MT6360_FLED_CH2].faults |= LED_FAULT_UNDER_VOLTAGE;
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_short_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_err(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH2].faults |= LED_FAULT_SHORT_CIRCUIT;
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_short_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_err(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH1].faults |= LED_FAULT_SHORT_CIRCUIT;
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_strb_to_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_dbg(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH2].faults |= LED_FAULT_TIMEOUT;
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_strb_to_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_dbg(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH1].faults |= LED_FAULT_TIMEOUT;
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled_chg_vinovp_evt_handler(int irq, void *data)
{
	struct mt6360_led_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	mli->mtfled_cdev[MT6360_FLED_CH1].faults |= LED_FAULT_INPUT_VOLTAGE;
	mli->mtfled_cdev[MT6360_FLED_CH2].faults |= LED_FAULT_INPUT_VOLTAGE;
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_fled_irq_desc[] = {
	{ "fled_chg_vinovp_evt",  mt6360_pmu_fled_chg_vinovp_evt_handler },
	{ "fled_lvf_evt", mt6360_pmu_fled_lvf_evt_handler },
	{ "fled2_short_evt", mt6360_pmu_fled2_short_evt_handler },
	{ "fled1_short_evt", mt6360_pmu_fled1_short_evt_handler },
	{ "fled2_strb_to_evt", mt6360_pmu_fled2_strb_to_evt_handler },
	{ "fled1_strb_to_evt", mt6360_pmu_fled1_strb_to_evt_handler },
};

static int mt6360_fled_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, irq, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_fled_irq_desc); i++) {
		irq_desc = mt6360_pmu_fled_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		irq = platform_get_irq_byname(pdev, irq_desc->name);
		if (irq < 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
			return ret;
		}
	}
	return 0;
}

#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
/******************************************************************************
 * Charger power supply class
 *****************************************************************************/
static int mt6360_high_voltage_supply(int enable)
{
	union power_supply_propval prop;
	static struct power_supply *chg_psy;
	int ret;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		ret = -1;
	} else {
		prop.intval = enable;
		ret = power_supply_set_property(chg_psy,
			 POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
		pr_notice("%s enable_hv:%d\n", __func__, prop.intval);
		power_supply_changed(chg_psy);
	}

	return ret;
}

static int mt6360_set_scenario(int scenario)
{
	/* notify charger to increase or decrease voltage */
	mutex_lock(&mt6360_mutex);
	if (scenario & FLASHLIGHT_SCENARIO_CAMERA_MASK) {
		if (!is_decrease_voltage) {
			pr_info("Decrease voltage level.\n");
			mt6360_high_voltage_supply(0);
			is_decrease_voltage = 1;
		}
	} else {
		if (is_decrease_voltage) {
			pr_info("Increase voltage level.\n");
			mt6360_high_voltage_supply(1);
			is_decrease_voltage = 0;
		}
	}
	mutex_unlock(&mt6360_mutex);

	return 0;
}

static int mt6360_open(void)
{
	mutex_lock(&mt6360_mutex);
	fd_use_count++;
	pr_debug("open driver: %d\n", fd_use_count);
	mutex_unlock(&mt6360_mutex);
	return 0;
}

static int mt6360_release(void)
{
	mutex_lock(&mt6360_mutex);
	fd_use_count--;
	pr_debug("close driver: %d\n", fd_use_count);
	/* If camera NE, we need to enable pe by ourselves*/
	if (fd_use_count == 0 && is_decrease_voltage) {
		pr_info("Increase voltage level.\n");
		mt6360_high_voltage_supply(1);
		is_decrease_voltage = 0;
	}
	mutex_unlock(&mt6360_mutex);
	return 0;
}

static int mt6360_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg;
	int channel;
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	fl_arg = (struct flashlight_dev_arg *)arg;
	channel = fl_arg->channel;

	if (channel >= MT6360_FLED_MAX || channel < 0) {
		pr_info("Failed with error channel\n");
		return -EINVAL;
	}

	flcdev = mt6360_flash_class[channel];
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
		mt6360_fled_brightness_set(lcdev, (int)fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		pr_debug("FLASH_IOC_SET_SCENARIO(%d): %d\n",
				channel, (int)fl_arg->arg);
		mt6360_set_scenario(fl_arg->arg);
		break;

	default:
		dev_info(lcdev->dev, "No such command and arg(%d): (%d, %d)\n",
				channel, _IOC_NR(cmd), (int)fl_arg->arg);
		return -ENOTTY;

	}
	return 0;
}

static ssize_t mt6360_strobe_store(struct flashlight_arg arg)
{
	struct led_classdev_flash *flcdev;
	struct led_classdev *lcdev;

	if (arg.channel < 0 || arg.channel >= MT6360_FLED_MAX) {
		pr_info("%s: fail, error channel %d\n", __func__, arg.channel);
		return -EINVAL;
	}

	flcdev = mt6360_flash_class[arg.channel];
	lcdev = &flcdev->led_cdev;
	mt6360_fled_brightness_set(lcdev, 1);
	msleep(arg.dur);
	mt6360_fled_brightness_set(lcdev, 0);
	return 0;
}

static int mt6360_set_driver(int set)
{
	return 0;
}

static struct flashlight_operations mt6360_ops = {
	mt6360_open,
	mt6360_release,
	mt6360_ioctl,
	mt6360_strobe_store,
	mt6360_set_driver
};
#endif

static int mt6360_iled_parse_dt(struct device *dev,
				struct mt6360_led_info *mli)
{
	struct device_node *iled_np, *child;
	struct mt6360_led_classdev *mtled_cdev;
	u32 val;
	int ret;

	if (!dev->of_node)
		return 0;
	iled_np = of_find_node_by_name(dev->of_node, "iled");
	if (!iled_np)
		return 0;
	for_each_available_child_of_node(iled_np, child) {
		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			dev_err(dev, "Fail to read reg property\n");
			continue;
		}
		if (val >= MT6360_LED_MAX) {
			dev_err(dev, "Invalid iled reg [%u]\n", val);
			ret = -EINVAL;
			goto out_iled_dt;
		}
		mtled_cdev = mli->mtled_cdev + val;

		of_property_read_string(child,
					"label", &(mtled_cdev->cdev.name));
		of_property_read_string(child, "linux,default-trigger",
					&(mtled_cdev->cdev.default_trigger));
		mtled_cdev->np = child;
	}
	return 0;
out_iled_dt:
	of_node_put(child);
	return ret;
}

static int mt6360_fled_parse_dt(struct device *dev,
				struct mt6360_led_info *mli)
{
	struct device_node *fled_np, *child;
	struct mt6360_fled_classdev *mtfled_cdev;
	struct led_classdev *led_cdev;
	struct led_flash_setting *fs;
	u32 val;
	int ret;

	if (!dev->of_node)
		return 0;
	fled_np = of_find_node_by_name(dev->of_node, "fled");
	if (!fled_np)
		return 0;
	for_each_available_child_of_node(fled_np, child) {
		u32 reg = 0;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			dev_err(dev, "Fail to read reg property\n");
			continue;
		}
		if (val >= MT6360_FLED_MAX) {
			dev_err(dev, "Invalid fled reg [%u]\n", val);
			ret = -EINVAL;
			goto out_fled_dt;
		}
		mtfled_cdev = mli->mtfled_cdev + val;
		reg = val;

		of_property_read_string(child, "label",
					&(mtfled_cdev->fl_cdev.led_cdev.name));
		ret = of_property_read_u32(child, "led-max-microamp", &val);
		if (ret) {
			dev_warn(dev, "led-max-microamp property missing\n");
			val = MT6360_TORCHCUR_MIN;
		}
		if (val < MT6360_TORCHCUR_MIN)
			val = MT6360_TORCHCUR_MIN;
		val = (val - MT6360_TORCHCUR_MIN) / MT6360_TORCHCUR_STEP + 1;
		led_cdev = &(mtfled_cdev->fl_cdev.led_cdev);
		led_cdev->max_brightness = min(led_cdev->max_brightness, val);
		ret = of_property_read_u32(child, "flash-max-microamp", &val);
		if (ret) {
			dev_warn(dev, "flash-max-microamp property missing\n");
			val = MT6360_STROBECUR_MIN;
		}
		if (val < MT6360_STROBECUR_MIN)
			val = MT6360_STROBECUR_MIN;
		fs = &(mtfled_cdev->fl_cdev.brightness);
		fs->val = fs->max = min(fs->max, val);
		ret = of_property_read_u32(child, "flash-max-timeout", &val);
		if (ret) {
			dev_warn(dev, "flash-max-timeout property missing\n");
			val = MT6360_STRBTIMEOUT_MIN;
		}
		if (val < MT6360_STRBTIMEOUT_MIN)
			val = MT6360_STRBTIMEOUT_MIN;
		fs = &(mtfled_cdev->fl_cdev.timeout);
		fs->val = fs->max = min(fs->max, val);
		mtfled_cdev->np = child;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
		of_property_read_u32(child, "type", &mtfled_cdev->dev_id.type);
		of_property_read_u32(child, "ct", &mtfled_cdev->dev_id.ct);
		of_property_read_u32(child, "part", &mtfled_cdev->dev_id.part);
		snprintf(mtfled_cdev->dev_id.name, FLASHLIGHT_NAME_SIZE,
				mtfled_cdev->fl_cdev.led_cdev.name);
		mtfled_cdev->dev_id.channel = reg;
		mt6360_flash_class[reg] = &mtfled_cdev->fl_cdev;
		mtfled_cdev->dev_id.decouple = 0;
		dev_info(dev, "Parse dt (type,ct,part,name,channel,decouple)=(%d,%d,%d,%s,%d,%d).\n",
				mtfled_cdev->dev_id.type, mtfled_cdev->dev_id.ct,
				mtfled_cdev->dev_id.part, mtfled_cdev->dev_id.name,
				mtfled_cdev->dev_id.channel,
				mtfled_cdev->dev_id.decouple);
		if (flashlight_dev_register_by_device_id(&mtfled_cdev->dev_id,
			&mt6360_ops))
			return -EFAULT;
#endif
	}
	return 0;
out_fled_dt:
	of_node_put(child);
	return ret;
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(fled1_ultraistrb, struct mt6360_led_platform_data,
			     MT6360_PMU_FLED1_STRB_CTRL2, 7, 0x80, NULL, 0),
	MT6360_PDATA_VALPROP(fled2_ultraistrb, struct mt6360_led_platform_data,
			     MT6360_PMU_FLED2_STRB_CTRL2, 7, 0x80, NULL, 0),
};

static int mt6360_led_apply_pdata(struct mt6360_led_info *mli,
				   struct mt6360_led_platform_data *pdata)
{
	int ret;

	dev_dbg(mli->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mli->regmap, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mli->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(rgbon_sync, struct mt6360_led_platform_data),
};

static int mt6360_led_parse_dt_data(struct device *dev,
				     struct mt6360_led_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static int mt6360_led_probe(struct platform_device *pdev)
{
	struct mt6360_led_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_led_info *mli;
	struct mt6360_led_classdev *mtled_cdev;
	struct mt6360_fled_classdev *mtfled_cdev;
	struct v4l2_flash_config v4l2_config;
	int i, ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	mli = devm_kzalloc(&pdev->dev, sizeof(*mli), GFP_KERNEL);
	if (!mli)
		return -ENOMEM;
	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_led_parse_dt_data(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "parse dt fail\n");
			return ret;
		}
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mli->dev = &pdev->dev;
	mli->pdata = pdata;
	platform_set_drvdata(pdev, mli);

	/* get parent regmap */
	mli->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!mli->regmap) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		return -ENODEV;
	}
	/* apply platform data */
	ret = mt6360_led_apply_pdata(mli, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "apply pdata fail\n");
		return ret;
	}
	/* iled register */
	memcpy(mli->mtled_cdev, def_led_classdev, sizeof(def_led_classdev));
	ret = mt6360_iled_parse_dt(&pdev->dev, mli);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to parse iled dt\n");
		return ret;
	}
	for (i = 0; i < MT6360_LED_MAX; i++) {
		mtled_cdev = mli->mtled_cdev + i;
		ret = devm_led_classdev_register(&pdev->dev,
						 &(mtled_cdev->cdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register led[%d]\n", i);
			return ret;
		}
		mtled_cdev->cdev.dev->of_node = mtled_cdev->np;
	}
	/* fled register */
	memcpy(mli->mtfled_cdev, def_fled_classdev, sizeof(def_fled_classdev));
	ret = mt6360_fled_parse_dt(&pdev->dev, mli);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to parse fled dt\n");
		return ret;
	}
	for (i = 0; i < MT6360_FLED_MAX; i++) {
		mtfled_cdev = mli->mtfled_cdev + i;
		ret = led_classdev_flash_register(&pdev->dev,
						  &mtfled_cdev->fl_cdev);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register fled[%d]\n", i);
			goto out_fled_cdev;
		}
	}
	for (i = 0; i < MT6360_FLED_MAX; i++) {
		mtfled_cdev = mli->mtfled_cdev + i;
		memset(&v4l2_config, 0, sizeof(v4l2_config));
		mt6360_init_v4l2_flash_config(mtfled_cdev, &v4l2_config);
		mtfled_cdev->v4l2_flash = v4l2_flash_init(&pdev->dev,
					      of_fwnode_handle(mtfled_cdev->np),
					      &mtfled_cdev->fl_cdev,
					      &v4l2_flash_ops, &v4l2_config);
		if (IS_ERR(mtfled_cdev->v4l2_flash)) {
			dev_err(&pdev->dev, "Failed to register v4l2_sd\n");
			ret = PTR_ERR(mtfled_cdev->v4l2_flash);
			goto out_v4l2_sd;
		}
	}

	/* Default mask fled_chg_vinovp*/
	ret = regmap_update_bits(mli->regmap,
			   MT6360_FLED_CHG_VINOVP, 0x08, 0xff);
	if (ret < 0)
		dev_err(&pdev->dev, "Fail to set fled_chg_vinovp, %d\n", ret);
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
	/* clear attributes */
	fd_use_count = 0;
	is_decrease_voltage = 0;
#endif

	ret = mt6360_fled_irq_register(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register irqs\n");
		goto out_v4l2_sd;
	}
	dev_info(&pdev->dev, "Successfully probed\n");
	return 0;
out_v4l2_sd:
	while (--i >= 0) {
		mtfled_cdev = mli->mtfled_cdev + i;
		v4l2_flash_release(mtfled_cdev->v4l2_flash);
	}
	i = MT6360_FLED_MAX;
out_fled_cdev:
	while (--i >= 0) {
		mtfled_cdev = mli->mtfled_cdev + i;
		led_classdev_flash_unregister(&mtfled_cdev->fl_cdev);
	}
	return ret;
}

static int mt6360_led_remove(struct platform_device *pdev)
{
	struct mt6360_led_info *mli = platform_get_drvdata(pdev);
	struct mt6360_fled_classdev *mtfled_cdev;
	int i;

	for (i = 0; i < MT6360_FLED_MAX; i++) {
		mtfled_cdev = mli->mtfled_cdev + i;
#if IS_ENABLED(CONFIG_MTK_FLASHLIGHT)
		flashlight_dev_unregister_by_device_id(&mtfled_cdev->dev_id);
#endif
		v4l2_flash_release(mtfled_cdev->v4l2_flash);
		led_classdev_flash_unregister(&mtfled_cdev->fl_cdev);
	}
	return 0;
}

static const struct of_device_id __maybe_unused mt6360_led_of_id[] = {
	{ .compatible = "mediatek,mt6360_led", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_led_of_id);

static const struct platform_device_id mt6360_led_id[] = {
	{ "mt6360_led", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_led_id);

static struct platform_driver mt6360_led_driver = {
	.driver = {
		.name = "mt6360_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6360_led_of_id),
	},
	.probe = mt6360_led_probe,
	.remove = mt6360_led_remove,
	.id_table = mt6360_led_id,
};
module_platform_driver(mt6360_led_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 Led Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
