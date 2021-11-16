// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/ratelimit.h>
#include <linux/clk.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <soc/snd_event.h>
#include <dsp/digital-cdc-rsc-mgr.h>
#include <linux/pm_runtime.h>
#include <dsp/audio_notifier.h>

#include "core.h"
#include "pinctrl-utils.h"

#define LPI_AUTO_SUSPEND_DELAY           100 /* delay in msec */
#define LPI_AUTO_SUSPEND_DELAY_ERROR     1   /* delay in msec */

#define LPI_ADDRESS_SIZE                 0x20000
#define LPI_SLEW_ADDRESS_SIZE            0x1000

#define LPI_GPIO_REG_VAL_CTL             0x00
#define LPI_GPIO_REG_DIR_CTL             0x04

#define LPI_SLEW_REG_VAL_CTL             0x00
#define LPI_SLEW_RATE_MAX                0x03
#define LPI_SLEW_BITS_SIZE               0x02
#define LPI_SLEW_OFFSET_INVALID          0xFFFFFFFF

#define LPI_GPIO_REG_PULL_SHIFT          0x0
#define LPI_GPIO_REG_PULL_MASK           0x3

#define LPI_GPIO_REG_FUNCTION_SHIFT      0x2
#define LPI_GPIO_REG_FUNCTION_MASK       0x3C

#define LPI_GPIO_REG_OUT_STRENGTH_SHIFT  0x6
#define LPI_GPIO_REG_OUT_STRENGTH_MASK   0x1C0

#define LPI_GPIO_REG_OE_SHIFT            0x9
#define LPI_GPIO_REG_OE_MASK             0x200

#define LPI_GPIO_REG_DIR_SHIFT           0x1
#define LPI_GPIO_REG_DIR_MASK            0x2

#define LPI_GPIO_BIAS_DISABLE            0x0
#define LPI_GPIO_PULL_DOWN               0x1
#define LPI_GPIO_KEEPER                  0x2
#define LPI_GPIO_PULL_UP                 0x3

#define LPI_GPIO_FUNC_GPIO               "gpio"
#define LPI_GPIO_FUNC_FUNC1              "func1"
#define LPI_GPIO_FUNC_FUNC2              "func2"
#define LPI_GPIO_FUNC_FUNC3              "func3"
#define LPI_GPIO_FUNC_FUNC4              "func4"
#define LPI_GPIO_FUNC_FUNC5              "func5"

static bool lpi_dev_up;
static struct device *lpi_dev;

/* The index of each function in lpi_gpio_functions[] array */
enum lpi_gpio_func_index {
	LPI_GPIO_FUNC_INDEX_GPIO	= 0x00,
	LPI_GPIO_FUNC_INDEX_FUNC1	= 0x01,
	LPI_GPIO_FUNC_INDEX_FUNC2	= 0x02,
	LPI_GPIO_FUNC_INDEX_FUNC3	= 0x03,
	LPI_GPIO_FUNC_INDEX_FUNC4	= 0x04,
	LPI_GPIO_FUNC_INDEX_FUNC5	= 0x05,
};

/**
 * struct lpi_gpio_pad - keep current GPIO settings
 * @offset: stores one of gpio_offset or slew_offset at a given time.
 * @gpio_offset: Nth GPIO in supported GPIOs.
 * @slew_offset: Nth GPIO's position in slew register in supported GPIOs.
 * @output_enabled: Set to true if GPIO output logic is enabled.
 * @value: value of a pin
 * @base: stores one of gpio_base or slew_base at a given time.
 * @gpio_base: Address base of LPI GPIO PAD.
 * @slew_base: Address base of LPI SLEW PAD.
 * @lpi_slew_reg: Address for lpi slew reg.
 * @pullup: Constant current which flow through GPIO output buffer.
 * @strength: No, Low, Medium, High
 * @function: See lpi_gpio_functions[]
 */
struct lpi_gpio_pad {
	u32             offset;
	u32             gpio_offset;
	u32             slew_offset;
	bool            output_enabled;
	bool            value;
	char __iomem    *base;
	char __iomem    *gpio_base;
	char __iomem    *slew_base;
	char __iomem    *lpi_slew_reg;
	unsigned int    pullup;
	unsigned int    strength;
	unsigned int    function;
};

struct lpi_gpio_state {
	struct device       *dev;
	struct pinctrl_dev  *ctrl;
	struct gpio_chip     chip;
	char __iomem        *base;
	struct clk          *lpass_core_hw_vote;
	struct clk          *lpass_audio_hw_vote;
	struct mutex         slew_access_lock;
	bool core_hw_vote_status;
	struct mutex        core_hw_vote_lock;
};

static const char *const lpi_gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
	"gpio29", "gpio30", "gpio31",
};

#define LPI_TLMM_MAX_PINS 100
static u32 lpi_offset[LPI_TLMM_MAX_PINS];
static u32 lpi_slew_offset[LPI_TLMM_MAX_PINS];
static u32 lpi_slew_base[LPI_TLMM_MAX_PINS];

static const char *const lpi_gpio_functions[] = {
	[LPI_GPIO_FUNC_INDEX_GPIO]	= LPI_GPIO_FUNC_GPIO,
	[LPI_GPIO_FUNC_INDEX_FUNC1]	= LPI_GPIO_FUNC_FUNC1,
	[LPI_GPIO_FUNC_INDEX_FUNC2]	= LPI_GPIO_FUNC_FUNC2,
	[LPI_GPIO_FUNC_INDEX_FUNC3]	= LPI_GPIO_FUNC_FUNC3,
	[LPI_GPIO_FUNC_INDEX_FUNC4]	= LPI_GPIO_FUNC_FUNC4,
	[LPI_GPIO_FUNC_INDEX_FUNC5]	= LPI_GPIO_FUNC_FUNC5,
};

int lpi_pinctrl_runtime_suspend(struct device *dev);

static int lpi_gpio_read(struct lpi_gpio_pad *pad, unsigned int addr)
{
	int ret = 0;
	struct lpi_gpio_state *state = dev_get_drvdata(lpi_dev);
	static DEFINE_RATELIMIT_STATE(rtl, 1 * HZ, 1);

	if (!lpi_dev_up) {
		if (__ratelimit(&rtl))
			pr_err("%s: ADSP is down due to SSR, return\n",
				   __func__);
		return 0;
	}
	pm_runtime_get_sync(lpi_dev);
	mutex_lock(&state->core_hw_vote_lock);
	if (!state->core_hw_vote_status) {
		if (__ratelimit(&rtl))
			pr_err("%s: core hw vote clk is not enabled\n",
				__func__);
		ret = -EINVAL;
		goto err;
	}

	ret = ioread32(pad->base + pad->offset + addr);
	if (ret < 0)
		pr_err("%s: read 0x%x failed\n", __func__, addr);

err:
	mutex_unlock(&state->core_hw_vote_lock);
	pm_runtime_mark_last_busy(lpi_dev);
	pm_runtime_put_autosuspend(lpi_dev);
	return ret;
}

static int lpi_gpio_write(struct lpi_gpio_pad *pad, unsigned int addr,
			  unsigned int val)
{
	struct lpi_gpio_state *state = dev_get_drvdata(lpi_dev);
	int ret = 0;
	static DEFINE_RATELIMIT_STATE(rtl, 1 * HZ, 1);

	if (!lpi_dev_up) {
		return 0;
	}
	pm_runtime_get_sync(lpi_dev);
	mutex_lock(&state->core_hw_vote_lock);
	if (!state->core_hw_vote_status) {
		if (__ratelimit(&rtl))
			pr_err("%s: core hw vote clk is not enabled\n",
				__func__);
		ret = -EINVAL;
		goto err;
	}

	iowrite32(val, pad->base + pad->offset + addr);
err:
	mutex_unlock(&state->core_hw_vote_lock);
	pm_runtime_mark_last_busy(lpi_dev);
	pm_runtime_put_autosuspend(lpi_dev);
	return ret;
}

static int lpi_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	/* Every PIN is a group */
	return pctldev->desc->npins;
}

static const char *lpi_gpio_get_group_name(struct pinctrl_dev *pctldev,
					   unsigned int pin)
{
	return pctldev->desc->pins[pin].name;
}

static int lpi_gpio_get_group_pins(struct pinctrl_dev *pctldev,
				   unsigned int pin,
				   const unsigned int **pins,
				   unsigned int *num_pins)
{
	*pins = &pctldev->desc->pins[pin].number;
	*num_pins = 1;
	return 0;
}

static const struct pinctrl_ops lpi_gpio_pinctrl_ops = {
	.get_groups_count	= lpi_gpio_get_groups_count,
	.get_group_name		= lpi_gpio_get_group_name,
	.get_group_pins		= lpi_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_group,
	.dt_free_map		= pinctrl_utils_free_map,
};

static int lpi_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(lpi_gpio_functions);
}

static const char *lpi_gpio_get_function_name(struct pinctrl_dev *pctldev,
					      unsigned int function)
{
	return lpi_gpio_functions[function];
}

static int lpi_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					unsigned int function,
					const char *const **groups,
					unsigned *const num_qgroups)
{
	*groups = lpi_gpio_groups;
	*num_qgroups = pctldev->desc->npins;
	return 0;
}

static int lpi_gpio_set_mux(struct pinctrl_dev *pctldev, unsigned int function,
			    unsigned int pin)
{
	struct lpi_gpio_pad *pad;
	unsigned int val;

	pad = pctldev->desc->pins[pin].drv_data;

	pad->function = function;

	val = lpi_gpio_read(pad, LPI_GPIO_REG_VAL_CTL);
	val &= ~(LPI_GPIO_REG_FUNCTION_MASK);
	val |= pad->function << LPI_GPIO_REG_FUNCTION_SHIFT;
	lpi_gpio_write(pad, LPI_GPIO_REG_VAL_CTL, val);
	return 0;
}

static const struct pinmux_ops lpi_gpio_pinmux_ops = {
	.get_functions_count	= lpi_gpio_get_functions_count,
	.get_function_name	= lpi_gpio_get_function_name,
	.get_function_groups	= lpi_gpio_get_function_groups,
	.set_mux		= lpi_gpio_set_mux,
};

static int lpi_config_get(struct pinctrl_dev *pctldev,
			  unsigned int pin, unsigned long *config)
{
	unsigned int param = pinconf_to_config_param(*config);
	struct lpi_gpio_pad *pad;
	unsigned int arg;

	pad = pctldev->desc->pins[pin].drv_data;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		arg = pad->pullup = LPI_GPIO_BIAS_DISABLE;
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		arg = pad->pullup == LPI_GPIO_PULL_DOWN;
		break;
	case PIN_CONFIG_BIAS_BUS_HOLD:
		arg = pad->pullup = LPI_GPIO_KEEPER;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		arg = pad->pullup == LPI_GPIO_PULL_UP;
		break;
	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT:
		arg = pad->output_enabled;
		break;
	default:
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);
	return 0;
}

static unsigned int lpi_drive_to_regval(u32 arg)
{
	return (arg/2 - 1);
}

static int lpi_config_set(struct pinctrl_dev *pctldev, unsigned int pin,
			  unsigned long *configs, unsigned int nconfs)
{
	struct lpi_gpio_pad *pad;
	unsigned int param, arg;
	int i, ret = 0;
	volatile unsigned long val;
	struct lpi_gpio_state *state = dev_get_drvdata(pctldev->dev);

	pad = pctldev->desc->pins[pin].drv_data;

	for (i = 0; i < nconfs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		dev_dbg(pctldev->dev, "%s: param: %d arg: %d pin: %d\n",
			__func__, param, arg, pin);

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			pad->pullup = LPI_GPIO_BIAS_DISABLE;
			break;
		case PIN_CONFIG_BIAS_PULL_DOWN:
			pad->pullup = LPI_GPIO_PULL_DOWN;
			break;
		case PIN_CONFIG_BIAS_BUS_HOLD:
			pad->pullup = LPI_GPIO_KEEPER;
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
			pad->pullup = LPI_GPIO_PULL_UP;
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			pad->output_enabled = false;
			break;
		case PIN_CONFIG_OUTPUT:
			pad->output_enabled = true;
			pad->value = arg;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			pad->strength = arg;
			break;
		case PIN_CONFIG_SLEW_RATE:
			if (pad->slew_base == NULL ||
				pad->slew_offset == LPI_SLEW_OFFSET_INVALID) {
				dev_dbg(pctldev->dev, "%s: invalid slew settings for pin: %d\n",
					__func__, pin);
				goto set_gpio;
			}
			if (arg > LPI_SLEW_RATE_MAX) {
				dev_err(pctldev->dev, "%s: invalid slew rate %u for pin: %d\n",
					__func__, arg, pin);
				goto set_gpio;
			}
			pad->base = pad->slew_base;
			pad->offset = 0;
			mutex_lock(&state->slew_access_lock);
			if (pad->lpi_slew_reg != NULL) {
				pad->base = pad->lpi_slew_reg;
				lpi_gpio_write(pad, LPI_SLEW_REG_VAL_CTL, arg);
				pad->base = pad->slew_base;
				goto slew_exit;
			}
			val = lpi_gpio_read(pad, LPI_SLEW_REG_VAL_CTL);
			pad->offset = pad->slew_offset;
			for (i = 0; i < LPI_SLEW_BITS_SIZE; i++) {
				if (arg & 0x01)
					set_bit(pad->offset, &val);
				else
					clear_bit(pad->offset, &val);
				pad->offset++;
				arg = arg >> 1;
			}
			pad->offset = 0;
			lpi_gpio_write(pad, LPI_SLEW_REG_VAL_CTL, val);
slew_exit:
			mutex_unlock(&state->slew_access_lock);
			break;
		default:
			ret = -EINVAL;
			goto done;
		}
	}

set_gpio:
	pad->base = pad->gpio_base;
	pad->offset = pad->gpio_offset;
	val = lpi_gpio_read(pad, LPI_GPIO_REG_VAL_CTL);
	val &= ~(LPI_GPIO_REG_PULL_MASK | LPI_GPIO_REG_OUT_STRENGTH_MASK |
		 LPI_GPIO_REG_OE_MASK);
	val |= pad->pullup << LPI_GPIO_REG_PULL_SHIFT;
	val |= lpi_drive_to_regval(pad->strength) <<
		LPI_GPIO_REG_OUT_STRENGTH_SHIFT;
	if (pad->output_enabled)
		val |= pad->value << LPI_GPIO_REG_OE_SHIFT;

	lpi_gpio_write(pad, LPI_GPIO_REG_VAL_CTL, val);
	lpi_gpio_write(pad, LPI_GPIO_REG_DIR_CTL,
		       pad->output_enabled << LPI_GPIO_REG_DIR_SHIFT);
done:
	return ret;
}

static const struct pinconf_ops lpi_gpio_pinconf_ops = {
	.is_generic			= true,
	.pin_config_group_get		= lpi_config_get,
	.pin_config_group_set		= lpi_config_set,
};

static int lpi_gpio_direction_input(struct gpio_chip *chip, unsigned int pin)
{
	struct lpi_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_INPUT_ENABLE, 1);

	return lpi_config_set(state->ctrl, pin, &config, 1);
}

static int lpi_gpio_direction_output(struct gpio_chip *chip,
				     unsigned int pin, int val)
{
	struct lpi_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, val);

	return lpi_config_set(state->ctrl, pin, &config, 1);
}

static int lpi_gpio_get(struct gpio_chip *chip, unsigned int pin)
{
	struct lpi_gpio_state *state = gpiochip_get_data(chip);
	struct lpi_gpio_pad *pad;
	int value;

	pad = state->ctrl->desc->pins[pin].drv_data;

	value = lpi_gpio_read(pad, LPI_GPIO_REG_VAL_CTL);
	return value;
}

static void lpi_gpio_set(struct gpio_chip *chip, unsigned int pin, int value)
{
	struct lpi_gpio_state *state = gpiochip_get_data(chip);
	unsigned long config;

	config = pinconf_to_config_packed(PIN_CONFIG_OUTPUT, value);

	lpi_config_set(state->ctrl, pin, &config, 1);
}

static int lpi_notifier_service_cb(struct notifier_block *this,
				   unsigned long opcode, void *ptr)
{
	static bool initial_boot = true;
	struct lpi_gpio_state *state = dev_get_drvdata(lpi_dev);

	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		if (initial_boot) {
			initial_boot = false;
			break;
		}
		snd_event_notify(lpi_dev, SND_EVENT_DOWN);
		lpi_dev_up = false;
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (initial_boot)
			initial_boot = false;

		/* Reset HW votes after SSR */
		if (!lpi_dev_up) {
			/* Add 100ms sleep to ensure AVS is up after SSR */
			msleep(100);
			if (state->lpass_core_hw_vote)
				digital_cdc_rsc_mgr_hw_vote_reset(
					state->lpass_core_hw_vote);
			if (state->lpass_audio_hw_vote)
				digital_cdc_rsc_mgr_hw_vote_reset(
					state->lpass_audio_hw_vote);
		}

		lpi_dev_up = true;
		snd_event_notify(lpi_dev, SND_EVENT_UP);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

int lpi_pinctrl_suspend(struct device *dev)
{
	int ret = 0;

	trace_printk("%s: system suspend\n",  __func__);
	dev_dbg(dev, "%s: system suspend\n", __func__);

	if ((!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev))) {
		ret = lpi_pinctrl_runtime_suspend(dev);
		if (!ret) {
			/*
			 * Synchronize runtime-pm and system-pm states:
			 * At this point, we are already suspended. If
			 * runtime-pm still thinks its active, then
			 * make sure its status is in sync with HW
			 * status. The three below calls let the
			 * runtime-pm know that we are suspended
			 * already without re-invoking the suspend
			 * callback
			 */
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
	}

	return ret;
}

int lpi_pinctrl_resume(struct device *dev)
{
	return 0;
}

static struct notifier_block service_nb = {
	.notifier_call  = lpi_notifier_service_cb,
	.priority = -INT_MAX,
};

static void lpi_pinctrl_ssr_disable(struct device *dev, void *data)
{
	trace_printk("%s: enter\n", __func__);
	lpi_dev_up = false;
	lpi_pinctrl_suspend(dev);
}

static const struct snd_event_ops lpi_pinctrl_ssr_ops = {
	.disable = lpi_pinctrl_ssr_disable,
};

#ifdef CONFIG_DEBUG_FS
#include <linux/seq_file.h>

static unsigned int lpi_regval_to_drive(u32 val)
{
	return (val + 1) * 2;
}

static void lpi_gpio_dbg_show_one(struct seq_file *s,
				  struct pinctrl_dev *pctldev,
				  struct gpio_chip *chip,
				  unsigned int offset,
				  unsigned int gpio)
{
	struct lpi_gpio_state *state = gpiochip_get_data(chip);
	struct pinctrl_pin_desc pindesc;
	struct lpi_gpio_pad *pad;
	unsigned int func;
	int is_out;
	int drive;
	int pull;
	u32 ctl_reg;

	static const char * const pulls[] = {
		"no pull",
		"pull down",
		"keeper",
		"pull up"
	};

	pctldev = pctldev ? : state->ctrl;
	pindesc = pctldev->desc->pins[offset];
	pad = pctldev->desc->pins[offset].drv_data;
	ctl_reg = lpi_gpio_read(pad, LPI_GPIO_REG_DIR_CTL);
	is_out = (ctl_reg & LPI_GPIO_REG_DIR_MASK) >> LPI_GPIO_REG_DIR_SHIFT;
	ctl_reg = lpi_gpio_read(pad, LPI_GPIO_REG_VAL_CTL);

	func = (ctl_reg & LPI_GPIO_REG_FUNCTION_MASK) >>
		LPI_GPIO_REG_FUNCTION_SHIFT;
	drive = (ctl_reg & LPI_GPIO_REG_OUT_STRENGTH_MASK) >>
		 LPI_GPIO_REG_OUT_STRENGTH_SHIFT;
	pull = (ctl_reg & LPI_GPIO_REG_PULL_MASK) >> LPI_GPIO_REG_PULL_SHIFT;

	seq_printf(s, " %-8s: %-3s %d",
		   pindesc.name, is_out ? "out" : "in", func);
	seq_printf(s, " %dmA", lpi_regval_to_drive(drive));
	seq_printf(s, " %s", pulls[pull]);
}

static void lpi_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	unsigned int gpio = chip->base;
	unsigned int i;

	for (i = 0; i < chip->ngpio; i++, gpio++) {
		lpi_gpio_dbg_show_one(s, NULL, chip, i, gpio);
		seq_puts(s, "\n");
	}
}

#else
#define lpi_gpio_dbg_show NULL
#endif

static const struct gpio_chip lpi_gpio_template = {
	.direction_input	= lpi_gpio_direction_input,
	.direction_output	= lpi_gpio_direction_output,
	.get			= lpi_gpio_get,
	.set			= lpi_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.dbg_show		= lpi_gpio_dbg_show,
};

static int lpi_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pinctrl_pin_desc *pindesc;
	struct pinctrl_desc *pctrldesc;
	struct lpi_gpio_pad *pad, *pads;
	struct lpi_gpio_state *state;
	int ret, npins, i;
	char __iomem *lpi_base;
	char __iomem *slew_base;
	u32 reg, slew_reg;
	struct clk *lpass_core_hw_vote = NULL;
	struct clk *lpass_audio_hw_vote = NULL;

	ret = of_property_read_u32(dev->of_node, "reg", &reg);
	if (ret < 0) {
		dev_err(dev, "missing base address\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,gpios-count", &npins);
	if (ret < 0)
		return ret;

	WARN_ON(npins > ARRAY_SIZE(lpi_gpio_groups));

	ret = of_property_read_u32_array(dev->of_node, "qcom,lpi-offset-tbl",
					 lpi_offset, npins);
	if (ret < 0) {
		dev_err(dev, "error in reading lpi offset table: %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32_array(dev->of_node,
					 "qcom,lpi-slew-offset-tbl",
					 lpi_slew_offset, npins);
	if (ret < 0) {
		for (i = 0; i < npins; i++)
			lpi_slew_offset[i] = LPI_SLEW_OFFSET_INVALID;
		dev_dbg(dev, "%s: error in reading lpi slew offset table: %d\n",
			__func__, ret);
	}

	ret = of_property_read_u32_array(dev->of_node,
					 "qcom,lpi-slew-base-tbl",
					 lpi_slew_base, npins);
	if (ret < 0) {
		for (i = 0; i < npins; i++)
			lpi_slew_base[i] = LPI_SLEW_OFFSET_INVALID;
		dev_dbg(dev, "%s: error in reading lpi slew table: %d\n",
			__func__, ret);
	}

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);

	state->dev = &pdev->dev;

	slew_reg = 0;
	ret = of_property_read_u32(dev->of_node, "qcom,slew-reg", &slew_reg);
	if (!ret) {
		slew_base = devm_ioremap(dev, slew_reg, LPI_SLEW_ADDRESS_SIZE);
		if (slew_base == NULL) {
			dev_err(dev,
				"%s devm_ioremap failed for slew rate reg\n",
				__func__);
			ret = -ENOMEM;
			goto err_io;
		}
	} else {
		slew_base = NULL;
		dev_dbg(dev, "error in reading lpi slew register: %d\n",
			__func__, ret);
	}

	pindesc = devm_kcalloc(dev, npins, sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc)
		return -ENOMEM;

	pads = devm_kcalloc(dev, npins, sizeof(*pads), GFP_KERNEL);
	if (!pads)
		return -ENOMEM;

	pctrldesc = devm_kzalloc(dev, sizeof(*pctrldesc), GFP_KERNEL);
	if (!pctrldesc)
		return -ENOMEM;

	pctrldesc->pctlops = &lpi_gpio_pinctrl_ops;
	pctrldesc->pmxops = &lpi_gpio_pinmux_ops;
	pctrldesc->confops = &lpi_gpio_pinconf_ops;
	pctrldesc->owner = THIS_MODULE;
	pctrldesc->name = dev_name(dev);
	pctrldesc->pins = pindesc;
	pctrldesc->npins = npins;

	lpi_base = devm_ioremap(dev, reg, LPI_ADDRESS_SIZE);
	if (lpi_base == NULL) {
		dev_err(dev, "%s devm_ioremap failed\n", __func__);
		return -ENOMEM;
	}

	state->base = lpi_base;

	for (i = 0; i < npins; i++, pindesc++) {
		pad = &pads[i];
		pindesc->drv_data = pad;
		pindesc->number = i;
		pindesc->name = lpi_gpio_groups[i];

		pad->gpio_base = lpi_base;
		pad->slew_base = slew_base;
		pad->base = pad->gpio_base;

		pad->gpio_offset = lpi_offset[i];
		pad->slew_offset = lpi_slew_offset[i];
		pad->offset = pad->gpio_offset;
		pad->lpi_slew_reg = NULL;
		if ((lpi_slew_base[i] != LPI_SLEW_OFFSET_INVALID) &&
		     lpi_slew_base[i])
			pad->lpi_slew_reg = devm_ioremap(dev,
                                                lpi_slew_base[i], 0x4);
	}

	state->chip = lpi_gpio_template;
	state->chip.parent = dev;
	state->chip.base = -1;
	state->chip.ngpio = npins;
	state->chip.label = dev_name(dev);
	state->chip.of_gpio_n_cells = 2;
	state->chip.can_sleep = false;

	mutex_init(&state->slew_access_lock);
	mutex_init(&state->core_hw_vote_lock);

	state->ctrl = devm_pinctrl_register(dev, pctrldesc, state);
	if (IS_ERR(state->ctrl))
		return PTR_ERR(state->ctrl);

	ret = gpiochip_add_data(&state->chip, state);
	if (ret) {
		dev_err(state->dev, "can't add gpio chip\n");
		goto err_chip;
	}

	ret = gpiochip_add_pin_range(&state->chip, dev_name(dev), 0, 0, npins);
	if (ret) {
		dev_err(dev, "failed to add pin range\n");
		goto err_range;
	}

	lpi_dev = &pdev->dev;
	lpi_dev_up = true;
	ret = audio_notifier_register("lpi_tlmm", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);
	if (ret < 0) {
		pr_err("%s: Audio notifier register failed ret = %d\n",
			__func__, ret);
		goto err_range;
	}

	ret = snd_event_client_register(dev, &lpi_pinctrl_ssr_ops, NULL);
	if (!ret) {
		snd_event_notify(dev, SND_EVENT_UP);
	} else {
		dev_err(dev, "%s: snd_event registration failed, ret [%d]\n",
			__func__, ret);
		goto err_snd_evt;
	}

	/* Register LPASS core hw vote */
	lpass_core_hw_vote = devm_clk_get(&pdev->dev, "lpass_core_hw_vote");
	if (IS_ERR(lpass_core_hw_vote)) {
		ret = PTR_ERR(lpass_core_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_core_hw_vote", ret);
		lpass_core_hw_vote = NULL;
		ret = 0;
	}
	state->lpass_core_hw_vote = lpass_core_hw_vote;

	/* Register LPASS audio hw vote */
	lpass_audio_hw_vote = devm_clk_get(&pdev->dev, "lpass_audio_hw_vote");
	if (IS_ERR(lpass_audio_hw_vote)) {
		ret = PTR_ERR(lpass_audio_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_audio_hw_vote", ret);
		lpass_audio_hw_vote = NULL;
		ret = 0;
	}
	state->lpass_audio_hw_vote = lpass_audio_hw_vote;

	state->core_hw_vote_status = false;
	pm_runtime_set_autosuspend_delay(&pdev->dev, LPI_AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

err_snd_evt:
	audio_notifier_deregister("lpi_tlmm");
err_range:
	gpiochip_remove(&state->chip);
err_chip:
	mutex_destroy(&state->core_hw_vote_lock);
	mutex_destroy(&state->slew_access_lock);
err_io:
	return ret;
}

static int lpi_pinctrl_remove(struct platform_device *pdev)
{
	struct lpi_gpio_state *state = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	snd_event_client_deregister(&pdev->dev);
	audio_notifier_deregister("lpi_tlmm");
	gpiochip_remove(&state->chip);
	mutex_destroy(&state->core_hw_vote_lock);
	mutex_destroy(&state->slew_access_lock);

	return 0;
}

static const struct of_device_id lpi_pinctrl_of_match[] = {
	{ .compatible = "qcom,lpi-pinctrl" }, /* Generic */
	{ },
};

MODULE_DEVICE_TABLE(of, lpi_pinctrl_of_match);

int lpi_pinctrl_runtime_resume(struct device *dev)
{
	struct lpi_gpio_state *state = dev_get_drvdata(dev);
	int ret = 0;
	struct clk *hw_vote = state->lpass_core_hw_vote;

	trace_printk("%s: enter\n", __func__);
	if (state->lpass_core_hw_vote == NULL) {
		dev_dbg(dev, "%s: Invalid core hw node\n", __func__);
		if (state->lpass_audio_hw_vote == NULL) {
			dev_dbg(dev, "%s: Invalid audio hw node\n", __func__);
			return 0;
		}
		hw_vote = state->lpass_audio_hw_vote;
	}

	mutex_lock(&state->core_hw_vote_lock);
	ret = digital_cdc_rsc_mgr_hw_vote_enable(hw_vote);
	if (ret < 0) {
		pm_runtime_set_autosuspend_delay(dev,
						 LPI_AUTO_SUSPEND_DELAY_ERROR);
		dev_err(dev, "%s:lpass core hw island enable failed\n",
			__func__);
		goto exit;
	} else {
		state->core_hw_vote_status = true;
	}

	pm_runtime_set_autosuspend_delay(dev, LPI_AUTO_SUSPEND_DELAY);

exit:
	mutex_unlock(&state->core_hw_vote_lock);
	trace_printk("%s: exit\n", __func__);
	return 0;
}

int lpi_pinctrl_runtime_suspend(struct device *dev)
{
	struct lpi_gpio_state *state = dev_get_drvdata(dev);
	struct clk *hw_vote = state->lpass_core_hw_vote;

	trace_printk("%s: enter\n", __func__);
	if (state->lpass_core_hw_vote == NULL) {
		dev_dbg(dev, "%s: Invalid core hw node\n", __func__);
		if (state->lpass_audio_hw_vote == NULL) {
			dev_dbg(dev, "%s: Invalid audio hw node\n", __func__);
			return 0;
		}
		hw_vote = state->lpass_audio_hw_vote;
	}

	mutex_lock(&state->core_hw_vote_lock);
	if (state->core_hw_vote_status) {
		digital_cdc_rsc_mgr_hw_vote_disable(hw_vote);
		state->core_hw_vote_status = false;
	}
	mutex_unlock(&state->core_hw_vote_lock);
	trace_printk("%s: exit\n", __func__);
	return 0;
}

static const struct dev_pm_ops lpi_pinctrl_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		lpi_pinctrl_suspend,
		lpi_pinctrl_resume
	)
	SET_RUNTIME_PM_OPS(
		lpi_pinctrl_runtime_suspend,
		lpi_pinctrl_runtime_resume,
		NULL
	)
};

static struct platform_driver lpi_pinctrl_driver = {
	.driver = {
		   .name = "qcom-lpi-pinctrl",
		   .pm = &lpi_pinctrl_dev_pm_ops,
		   .of_match_table = lpi_pinctrl_of_match,
		   .suppress_bind_attrs = true,
	},
	.probe = lpi_pinctrl_probe,
	.remove = lpi_pinctrl_remove,
};

module_platform_driver(lpi_pinctrl_driver);

MODULE_DESCRIPTION("QTI LPI GPIO pin control driver");
MODULE_LICENSE("GPL v2");
