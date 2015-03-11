 /* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

/*
 * pinctrl -- handle to query pinctrl apis
 * cdc lines -- stores pinctrl handles for pinctrl states
 * active_set -- maintain the overall pinctrl state
 */
struct cdc_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state **cdc_lines;
	int active_set;
};

/*
 * gpiosets -- stores all gpiosets mentioned in dtsi file
 * gpiosets_comb_names -- stores all possible gpioset combinations
 * gpioset_state -- maintains counter for each gpioset
 * gpiosets_max -- maintain the total supported gpiosets
 * gpiosets_comb_max -- maintain the total gpiosets combinations
 */
struct cdc_gpioset_info {
	char **gpiosets;
	char **gpiosets_comb_names;
	uint8_t *gpioset_state;
	int gpiosets_max;
	int gpiosets_comb_max;
};

static struct cdc_pinctrl_info pinctrl_info;
static struct cdc_gpioset_info gpioset_info;

/* Finds the index for the gpio set in the dtsi file */
int msm_get_gpioset_index(char *keyword)
{
	int i;

	for (i = 0; i < gpioset_info.gpiosets_max; i++) {
		if (!(strcmp(gpioset_info.gpiosets[i], keyword)))
			break;
	}
	/* Checking if the keyword is present in dtsi or not */
	if (i != gpioset_info.gpiosets_max)
		return i;
	else
		return -EINVAL;
}

/*
 * This function reads the following from dtsi file
 * 1. All gpio sets
 * 2. All combinations of gpio sets
 * 3. Pinctrl handles to gpio sets
 *
 * Returns error if there is
 * 1. Problem reading from dtsi file
 * 2. Memory allocation failure
 */
int msm_gpioset_initialize(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;
	const char *gpioset_names = "qcom,msm-gpios";
	const char *gpioset_combinations = "qcom,pinctrl-names";
	const char *gpioset_names_str = NULL;
	const char *gpioset_comb_str = NULL;
	int num_strings = 0;
	int ret = 0;
	int i = 0;

	pr_debug("%s\n", __func__);
	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n",
				__func__);
		return -EINVAL;
	}
	pinctrl_info.pinctrl = pinctrl;

	/* Reading of gpio sets */
	num_strings = of_property_count_strings(pdev->dev.of_node,
						gpioset_names);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				__func__, gpioset_names);
		goto err;
	}
	gpioset_info.gpiosets_max = num_strings;
	gpioset_info.gpiosets = devm_kzalloc(&pdev->dev,
				gpioset_info.gpiosets_max * sizeof(char *),
				GFP_KERNEL);
	if (!gpioset_info.gpiosets) {
		dev_err(&pdev->dev, "Can't allocate memory for gpio set names\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < num_strings; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
					gpioset_names, i, &gpioset_names_str);

		gpioset_info.gpiosets[i] = devm_kzalloc(&pdev->dev,
				(strlen(gpioset_names_str) + 1), GFP_KERNEL);

		if (!gpioset_info.gpiosets[i]) {
			dev_err(&pdev->dev, "%s: Can't allocate gpiosets[%d] data\n",
						__func__, i);
			ret = -ENOMEM;
			goto err;
		}
		strlcpy(gpioset_info.gpiosets[i],
				gpioset_names_str, strlen(gpioset_names_str)+1);
		gpioset_names_str = NULL;
	}
	num_strings = 0;

	/* Allocating memory for gpio set counter */
	gpioset_info.gpioset_state = devm_kzalloc(&pdev->dev,
				gpioset_info.gpiosets_max * sizeof(uint8_t),
				GFP_KERNEL);
	if (!gpioset_info.gpioset_state) {
		dev_err(&pdev->dev, "Can't allocate memory for gpio set counter\n");
		ret = -ENOMEM;
		goto err;
	}

	/* Reading of all combinations of gpio sets */
	num_strings = of_property_count_strings(pdev->dev.of_node,
						gpioset_combinations);
	if (num_strings < 0) {
		dev_err(&pdev->dev,
				"%s: missing %s in dt node or length is incorrect\n",
				__func__, gpioset_combinations);
		goto err;
	}
	gpioset_info.gpiosets_comb_max = num_strings;
	gpioset_info.gpiosets_comb_names = devm_kzalloc(&pdev->dev,
				num_strings * sizeof(char *), GFP_KERNEL);
	if (!gpioset_info.gpiosets_comb_names) {
		dev_err(&pdev->dev, "Can't allocate gpio set combination names data\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < gpioset_info.gpiosets_comb_max; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
				gpioset_combinations, i, &gpioset_comb_str);

		gpioset_info.gpiosets_comb_names[i] = devm_kzalloc(&pdev->dev,
				(strlen(gpioset_comb_str) + 1), GFP_KERNEL);
		if (!gpioset_info.gpiosets_comb_names[i]) {
			dev_err(&pdev->dev, "%s: Can't allocate combinations[%d] data\n",
					__func__, i);
			ret = -ENOMEM;
			goto err;
		}

		strlcpy(gpioset_info.gpiosets_comb_names[i], gpioset_comb_str,
					strlen(gpioset_comb_str)+1);
		pr_debug("%s: GPIO configuration %s\n",
				__func__, gpioset_info.gpiosets_comb_names[i]);
		gpioset_comb_str = NULL;
	}

	/* Allocating memory for handles to pinctrl states */
	pinctrl_info.cdc_lines = devm_kzalloc(&pdev->dev,
		num_strings * sizeof(char *), GFP_KERNEL);
	if (!pinctrl_info.cdc_lines) {
		dev_err(&pdev->dev, "Can't allocate pinctrl_info.cdc_lines data\n");
		ret = -ENOMEM;
		goto err;
	}

	/* Get pinctrl handles for gpio sets in dtsi file */
	for (i = 0; i < num_strings; i++) {
		pinctrl_info.cdc_lines[i] = pinctrl_lookup_state(pinctrl,
			(const char *)gpioset_info.gpiosets_comb_names[i]);
		if (IS_ERR(pinctrl_info.cdc_lines[i]))
			pr_err("%s: Unable to get pinctrl handle for %s\n",
				__func__, gpioset_info.gpiosets_comb_names[i]);
	}
	goto success;

err:
	/* Free up memory allocated for gpio set combinations */
	for (i = 0; i < gpioset_info.gpiosets_max; i++) {
		if (NULL != gpioset_info.gpiosets[i])
			devm_kfree(&pdev->dev, gpioset_info.gpiosets[i]);
	}
	if (NULL != gpioset_info.gpiosets)
		devm_kfree(&pdev->dev, gpioset_info.gpiosets);

	/* Free up memory allocated for gpio set combinations */
	for (i = 0; i < gpioset_info.gpiosets_comb_max; i++) {
		if (NULL != gpioset_info.gpiosets_comb_names[i])
			devm_kfree(&pdev->dev,
			gpioset_info.gpiosets_comb_names[i]);
	}
	if (NULL != gpioset_info.gpiosets_comb_names)
		devm_kfree(&pdev->dev, gpioset_info.gpiosets_comb_names);

	/* Free up memory allocated for handles to pinctrl states */
	if (NULL != pinctrl_info.cdc_lines)
		devm_kfree(&pdev->dev, pinctrl_info.cdc_lines);

	/* Free up memory allocated for counter of gpio sets */
	if (NULL != gpioset_info.gpioset_state)
		devm_kfree(&pdev->dev, gpioset_info.gpioset_state);

success:
	return ret;
}

int msm_gpioset_activate(char *keyword)
{
	int ret = 0;
	int gp_set = 0;

	gp_set = msm_get_gpioset_index(keyword);
	if (gp_set < 0) {
		pr_err("%s: gpio set name does not exist\n",
				__func__);
		return gp_set;
	}

	if (!gpioset_info.gpioset_state[gp_set]) {
		/*
		 * If pinctrl pointer is not valid,
		 * no need to proceed further
		 */
		if (IS_ERR(pinctrl_info.cdc_lines[pinctrl_info.active_set]))
			return 0;

		pinctrl_info.active_set |= (1 << gp_set);
		pr_debug("%s: pinctrl.active_set: %d\n", __func__,
				pinctrl_info.active_set);

		/* Select the appropriate pinctrl state */
		ret =  pinctrl_select_state(pinctrl_info.pinctrl,
			pinctrl_info.cdc_lines[pinctrl_info.active_set]);
	}
	gpioset_info.gpioset_state[gp_set]++;

	return ret;
}

int msm_gpioset_suspend(char *keyword)
{
	int ret = 0;
	int gp_set = 0;

	gp_set = msm_get_gpioset_index(keyword);
	if (gp_set < 0) {
		pr_err("%s: gpio set name does not exist\n",
				__func__);
		return gp_set;
	}

	if (1 == gpioset_info.gpioset_state[gp_set]) {
		pinctrl_info.active_set &= ~(1 << gp_set);
		/*
		 * If pinctrl pointer is not valid,
		 * no need to proceed further
		 */
		if (IS_ERR(pinctrl_info.cdc_lines[pinctrl_info.active_set]))
			return -EINVAL;

		pr_debug("%s: pinctrl.active_set: %d\n", __func__,
				pinctrl_info.active_set);
		/* Select the appropriate pinctrl state */
		ret =  pinctrl_select_state(pinctrl_info.pinctrl,
			pinctrl_info.cdc_lines[pinctrl_info.active_set]);
	}
	if (!(gpioset_info.gpioset_state[gp_set])) {
		pr_err("%s: Invalid call to de activate gpios: %d\n", __func__,
				gpioset_info.gpioset_state[gp_set]);
		return -EINVAL;
	}

	gpioset_info.gpioset_state[gp_set]--;

	return ret;
}
