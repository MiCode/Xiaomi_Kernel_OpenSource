// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/qcom-pinctrl.h>
#include <asoc/msm-cdc-pinctrl.h>

#define MAX_GPIOS 16

struct msm_cdc_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_active;
	struct pinctrl_state *pinctrl_sleep;
	struct pinctrl_state *pinctrl_alt_active;
	int gpio;
	bool state;
	u32 tlmm_gpio[MAX_GPIOS];
	char __iomem *chip_wakeup_register[MAX_GPIOS];
	u32 chip_wakeup_maskbit[MAX_GPIOS];
	u32 count;
	u32 wakeup_reg_count;
	bool wakeup_capable;
	bool chip_wakeup_reg;
};

static struct msm_cdc_pinctrl_info *msm_cdc_pinctrl_get_gpiodata(
						struct device_node *np)
{
	struct platform_device *pdev;
	struct msm_cdc_pinctrl_info *gpio_data;

	if (!np) {
		pr_err("%s: device node is null\n", __func__);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: platform device not found!\n", __func__);
		return NULL;
	}

	gpio_data = dev_get_drvdata(&pdev->dev);
	if (!gpio_data)
		dev_err(&pdev->dev, "%s: cannot find cdc gpio info\n",
			__func__);

	return gpio_data;
}

/*
 * msm_cdc_get_gpio_state: select pinctrl sleep state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure and GPIO value on success
 */
int msm_cdc_get_gpio_state(struct device_node *np)
{
	struct msm_cdc_pinctrl_info *gpio_data;
	int value = -EINVAL;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return value;

	if (gpio_is_valid(gpio_data->gpio))
		value = gpio_get_value_cansleep(gpio_data->gpio);

	return value;
}
EXPORT_SYMBOL(msm_cdc_get_gpio_state);

/*
 * msm_cdc_pinctrl_select_sleep_state: select pinctrl sleep state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure
 */
int msm_cdc_pinctrl_select_sleep_state(struct device_node *np)
{
	struct msm_cdc_pinctrl_info *gpio_data;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (!gpio_data->pinctrl_sleep) {
		pr_err("%s: pinctrl sleep state is null\n", __func__);
		return -EINVAL;
	}
	gpio_data->state = false;

	return pinctrl_select_state(gpio_data->pinctrl,
				    gpio_data->pinctrl_sleep);
}
EXPORT_SYMBOL(msm_cdc_pinctrl_select_sleep_state);

/*
 * msm_cdc_pinctrl_select_alt_active_state: select pinctrl alt_active state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure
 */
int msm_cdc_pinctrl_select_alt_active_state(struct device_node *np)
{
	struct msm_cdc_pinctrl_info *gpio_data;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (!gpio_data->pinctrl_alt_active) {
		pr_err("%s: pinctrl alt_active state is null\n", __func__);
		return -EINVAL;
	}
	gpio_data->state = true;

	return pinctrl_select_state(gpio_data->pinctrl,
				    gpio_data->pinctrl_alt_active);
}
EXPORT_SYMBOL(msm_cdc_pinctrl_select_alt_active_state);

/*
 * msm_cdc_pinctrl_select_active_state: select pinctrl active state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure
 */
int msm_cdc_pinctrl_select_active_state(struct device_node *np)
{
	struct msm_cdc_pinctrl_info *gpio_data;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (!gpio_data->pinctrl_active) {
		pr_err("%s: pinctrl active state is null\n", __func__);
		return -EINVAL;
	}
	gpio_data->state = true;

	return pinctrl_select_state(gpio_data->pinctrl,
				    gpio_data->pinctrl_active);
}
EXPORT_SYMBOL(msm_cdc_pinctrl_select_active_state);

/*
 * msm_cdc_pinctrl_get_state: get curren pinctrl state
 * @np: pointer to struct device_node
 *
 * Returns 0 for sleep state, 1 for active state,
 * error code for failure
 */
int msm_cdc_pinctrl_get_state(struct device_node *np)
{
	struct msm_cdc_pinctrl_info *gpio_data;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	return gpio_data->state;
}
EXPORT_SYMBOL(msm_cdc_pinctrl_get_state);

/*
 * msm_cdc_pinctrl_set_wakeup_capable: Set a pinctrl to wakeup capable
 * @np: pointer to struct device_node
 * @enable: wakeup capable when set to true
 *
 * Returns 0 for success and error code for failure
 */
int msm_cdc_pinctrl_set_wakeup_capable(struct device_node *np, bool enable)
{
	struct msm_cdc_pinctrl_info *gpio_data;
	int ret = 0;
	u32 i = 0, temp = 0;

	gpio_data = msm_cdc_pinctrl_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (gpio_data->wakeup_capable) {
		for (i = 0; i < gpio_data->count; i++) {
			ret = msm_gpio_mpm_wake_set(gpio_data->tlmm_gpio[i],
						    enable);
			if (ret < 0)
				goto exit;
		}
	}
	if (gpio_data->chip_wakeup_reg) {
		for (i = 0; i < gpio_data->wakeup_reg_count; i++) {
			temp = ioread32(gpio_data->chip_wakeup_register[i]);
			if (enable)
				temp |= (1 <<
					 gpio_data->chip_wakeup_maskbit[i]);
			else
				temp &= ~(1 <<
					  gpio_data->chip_wakeup_maskbit[i]);
			iowrite32(temp, gpio_data->chip_wakeup_register[i]);
		}
	}
exit:
	return ret;
}
EXPORT_SYMBOL(msm_cdc_pinctrl_set_wakeup_capable);

static int msm_cdc_pinctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_cdc_pinctrl_info *gpio_data;
	u32 tlmm_gpio[MAX_GPIOS] = {0};
	u32 chip_wakeup_reg[MAX_GPIOS] = {0};
	u32 chip_wakeup_default_val[MAX_GPIOS] = {0};
	u32 i = 0, temp = 0;
	int count = 0;

	gpio_data = devm_kzalloc(&pdev->dev,
				 sizeof(struct msm_cdc_pinctrl_info),
				 GFP_KERNEL);
	if (!gpio_data)
		return -ENOMEM;

	gpio_data->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(gpio_data->pinctrl)) {
		dev_err(&pdev->dev, "%s: Cannot get cdc gpio pinctrl:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl));
		ret = PTR_ERR(gpio_data->pinctrl);
		goto err_pctrl_get;
	}

	gpio_data->pinctrl_active = pinctrl_lookup_state(
					gpio_data->pinctrl, "aud_active");
	if (IS_ERR_OR_NULL(gpio_data->pinctrl_active)) {
		dev_err(&pdev->dev, "%s: Cannot get aud_active pinctrl state:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl_active));
		ret = PTR_ERR(gpio_data->pinctrl_active);
		goto err_lookup_state;
	}

	gpio_data->pinctrl_sleep = pinctrl_lookup_state(
					gpio_data->pinctrl, "aud_sleep");
	if (IS_ERR_OR_NULL(gpio_data->pinctrl_sleep)) {
		dev_err(&pdev->dev, "%s: Cannot get aud_sleep pinctrl state:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl_sleep));
		ret = PTR_ERR(gpio_data->pinctrl_sleep);
		goto err_lookup_state;
	}

	gpio_data->pinctrl_alt_active = pinctrl_lookup_state(
					gpio_data->pinctrl, "aud_alt_active");
	if (IS_ERR_OR_NULL(gpio_data->pinctrl_alt_active)) {
		dev_dbg(&pdev->dev, "%s: Cannot get aud_alt_active pinctrl state:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl_alt_active));
	}

	/* skip setting to sleep state for LPI_TLMM GPIOs */
	if (!of_property_read_bool(pdev->dev.of_node, "qcom,lpi-gpios")) {
		/* Set pinctrl state to aud_sleep by default */
		ret = pinctrl_select_state(gpio_data->pinctrl,
					   gpio_data->pinctrl_sleep);
		if (ret)
			dev_err(&pdev->dev, "%s: set cdc gpio sleep state fail: %d\n",
				__func__, ret);
	}


	count = of_property_count_u32_elems(pdev->dev.of_node, "qcom,chip-wakeup-reg");
	if (count <= 0)
		goto cdc_tlmm_gpio;
	if (!of_property_read_u32_array(pdev->dev.of_node, "qcom,chip-wakeup-reg",
				chip_wakeup_reg, count)) {
		if (of_property_read_u32_array(pdev->dev.of_node,
					   "qcom,chip-wakeup-maskbit",
					   gpio_data->chip_wakeup_maskbit, count)) {
			dev_err(&pdev->dev,
				"chip-wakeup-maskbit needed if chip-wakeup-reg is defined!\n");
			goto cdc_tlmm_gpio;
		}
		gpio_data->chip_wakeup_reg = true;
		for (i = 0; i < count; i++) {
			gpio_data->chip_wakeup_register[i] =
				devm_ioremap(&pdev->dev, chip_wakeup_reg[i], 0x4);
		}
		if (!of_property_read_u32_array(pdev->dev.of_node,
					"qcom,chip-wakeup-default-val",
					chip_wakeup_default_val, count)) {
			for (i = 0; i < count; i++) {
				temp = ioread32(gpio_data->chip_wakeup_register[i]);
				if (chip_wakeup_default_val[i])
					temp |= (1 <<
						 gpio_data->chip_wakeup_maskbit[i]);
				else
					temp &= ~(1 <<
						  gpio_data->chip_wakeup_maskbit[i]);
				iowrite32(temp, gpio_data->chip_wakeup_register[i]);
			}
		}
		gpio_data->wakeup_reg_count = count;
	}

cdc_tlmm_gpio:
	count = of_property_count_u32_elems(pdev->dev.of_node, "qcom,tlmm-pins");
	if (count <= 0)
		goto cdc_rst;
	if (!of_property_read_u32_array(pdev->dev.of_node, "qcom,tlmm-pins",
				tlmm_gpio, count)) {
		gpio_data->wakeup_capable = true;
		for (i = 0; i < count; i++)
			gpio_data->tlmm_gpio[i] = tlmm_gpio[i];
		gpio_data->count = count;
	}

cdc_rst:
	gpio_data->gpio = of_get_named_gpio(pdev->dev.of_node,
					    "qcom,cdc-rst-n-gpio", 0);
	if (gpio_is_valid(gpio_data->gpio)) {
		ret = gpio_request(gpio_data->gpio, "MSM_CDC_RESET");
		if (ret) {
			dev_err(&pdev->dev, "%s: Failed to request gpio %d\n",
				__func__, gpio_data->gpio);
			goto err_lookup_state;
		}
	}

	dev_set_drvdata(&pdev->dev, gpio_data);
	return 0;

err_lookup_state:
	devm_pinctrl_put(gpio_data->pinctrl);
err_pctrl_get:
	devm_kfree(&pdev->dev, gpio_data);
	return ret;
}

static int msm_cdc_pinctrl_remove(struct platform_device *pdev)
{
	struct msm_cdc_pinctrl_info *gpio_data;

	gpio_data = dev_get_drvdata(&pdev->dev);

	/* to free the requested gpio before exiting */
	if (gpio_data) {
		if (gpio_is_valid(gpio_data->gpio))
			gpio_free(gpio_data->gpio);

		if (gpio_data->pinctrl)
			devm_pinctrl_put(gpio_data->pinctrl);
	}

	devm_kfree(&pdev->dev, gpio_data);

	return 0;
}

static const struct of_device_id msm_cdc_pinctrl_match[] = {
	{.compatible = "qcom,msm-cdc-pinctrl"},
	{}
};

static struct platform_driver msm_cdc_pinctrl_driver = {
	.driver = {
		.name = "msm-cdc-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = msm_cdc_pinctrl_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_cdc_pinctrl_probe,
	.remove = msm_cdc_pinctrl_remove,
};

int msm_cdc_pinctrl_drv_init(void)
{
	return platform_driver_register(&msm_cdc_pinctrl_driver);
}

void msm_cdc_pinctrl_drv_exit(void)
{
	platform_driver_unregister(&msm_cdc_pinctrl_driver);
}
MODULE_DESCRIPTION("MSM CODEC pin control platform driver");
MODULE_LICENSE("GPL v2");
