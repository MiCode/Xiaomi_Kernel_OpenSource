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
#include "msm-audio-pinctrl.h"

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

static struct cdc_pinctrl_info pinctrl_info[MAX_PINCTRL_CLIENT];
static struct cdc_gpioset_info gpioset_info[MAX_PINCTRL_CLIENT];

/* Finds the index for the gpio set in the dtsi file */
int msm_get_gpioset_index(enum pinctrl_client client, char *keyword)
{
	int i;

	for (i = 0; i < gpioset_info[client].gpiosets_max; i++) {
		if (!(strcmp(gpioset_info[client].gpiosets[i], keyword)))
			break;
	}
	/* Checking if the keyword is present in dtsi or not */
	if (i != gpioset_info[client].gpiosets_max)
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
int msm_gpioset_initialize(enum pinctrl_client client,
				struct device *dev)
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
	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n",
				__func__);
		return -EINVAL;
	}
	pinctrl_info[client].pinctrl = pinctrl;

	/* Reading of gpio sets */
	num_strings = of_property_count_strings(dev->of_node,
						gpioset_names);
	if (num_strings < 0) {
		dev_err(dev,
			"%s: missing %s in dt node or length is incorrect\n",
				__func__, gpioset_names);
		goto err;
	}
	gpioset_info[client].gpiosets_max = num_strings;
	gpioset_info[client].gpiosets = devm_kzalloc(dev,
				gpioset_info[client].gpiosets_max *
					sizeof(char *), GFP_KERNEL);
	if (!gpioset_info[client].gpiosets) {
		dev_err(dev, "Can't allocate memory for gpio set names\n");
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < num_strings; i++) {
		ret = of_property_read_string_index(dev->of_node,
					gpioset_names, i, &gpioset_names_str);

		gpioset_info[client].gpiosets[i] = devm_kzalloc(dev,
				(strlen(gpioset_names_str) + 1), GFP_KERNEL);

		if (!gpioset_info[client].gpiosets[i]) {
			dev_err(dev, "%s: Can't allocate gpiosets[%d] data\n",
						__func__, i);
			ret = -ENOMEM;
			goto err;
		}
		strlcpy(gpioset_info[client].gpiosets[i],
				gpioset_names_str, strlen(gpioset_names_str)+1);
		gpioset_names_str = NULL;
	}
	num_strings = 0;

	/* Allocating memory for gpio set counter */
	gpioset_info[client].gpioset_state = devm_kzalloc(dev,
				gpioset_info[client].gpiosets_max *
				sizeof(uint8_t), GFP_KERNEL);
	if (!gpioset_info[client].gpioset_state) {
		dev_err(dev, "Can't allocate memory for gpio set counter\n");
		ret = -ENOMEM;
		goto err;
	}

	/* Reading of all combinations of gpio sets */
	num_strings = of_property_count_strings(dev->of_node,
						gpioset_combinations);
	if (num_strings < 0) {
		dev_err(dev,
			"%s: missing %s in dt node or length is incorrect\n",
			__func__, gpioset_combinations);
		goto err;
	}
	gpioset_info[client].gpiosets_comb_max = num_strings;
	gpioset_info[client].gpiosets_comb_names = devm_kzalloc(dev,
				num_strings * sizeof(char *), GFP_KERNEL);
	if (!gpioset_info[client].gpiosets_comb_names) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < gpioset_info[client].gpiosets_comb_max; i++) {
		ret = of_property_read_string_index(dev->of_node,
				gpioset_combinations, i, &gpioset_comb_str);

		gpioset_info[client].gpiosets_comb_names[i] = devm_kzalloc(dev,
				(strlen(gpioset_comb_str) + 1), GFP_KERNEL);
		if (!gpioset_info[client].gpiosets_comb_names[i]) {
			ret = -ENOMEM;
			goto err;
		}

		strlcpy(gpioset_info[client].gpiosets_comb_names[i],
					gpioset_comb_str,
					strlen(gpioset_comb_str)+1);
		pr_debug("%s: GPIO configuration %s\n",
				__func__,
				gpioset_info[client].gpiosets_comb_names[i]);
		gpioset_comb_str = NULL;
	}

	/* Allocating memory for handles to pinctrl states */
	pinctrl_info[client].cdc_lines = devm_kzalloc(dev,
		num_strings * sizeof(char *), GFP_KERNEL);
	if (!pinctrl_info[client].cdc_lines) {
		ret = -ENOMEM;
		goto err;
	}

	/* Get pinctrl handles for gpio sets in dtsi file */
	for (i = 0; i < num_strings; i++) {
		pinctrl_info[client].cdc_lines[i] = pinctrl_lookup_state(
								pinctrl,
					(const char *)gpioset_info[client].
							gpiosets_comb_names[i]);
		if (IS_ERR(pinctrl_info[client].cdc_lines[i]))
			pr_err("%s: Unable to get pinctrl handle for %s\n",
				__func__, gpioset_info[client].
						gpiosets_comb_names[i]);
	}
	goto success;

err:
	/* Free up memory allocated for gpio set combinations */
	for (i = 0; i < gpioset_info[client].gpiosets_max; i++) {
		if (NULL != gpioset_info[client].gpiosets[i])
			devm_kfree(dev, gpioset_info[client].gpiosets[i]);
	}
	if (NULL != gpioset_info[client].gpiosets)
		devm_kfree(dev, gpioset_info[client].gpiosets);

	/* Free up memory allocated for gpio set combinations */
	for (i = 0; i < gpioset_info[client].gpiosets_comb_max; i++) {
		if (NULL != gpioset_info[client].gpiosets_comb_names[i])
			devm_kfree(dev,
			gpioset_info[client].gpiosets_comb_names[i]);
	}
	if (NULL != gpioset_info[client].gpiosets_comb_names)
		devm_kfree(dev, gpioset_info[client].gpiosets_comb_names);

	/* Free up memory allocated for handles to pinctrl states */
	if (NULL != pinctrl_info[client].cdc_lines)
		devm_kfree(dev, pinctrl_info[client].cdc_lines);

	/* Free up memory allocated for counter of gpio sets */
	if (NULL != gpioset_info[client].gpioset_state)
		devm_kfree(dev, gpioset_info[client].gpioset_state);

success:
	return ret;
}

int msm_gpioset_activate(enum pinctrl_client client, char *keyword)
{
	int ret = 0;
	int gp_set = 0;
	int active_set = 0;

	gp_set = msm_get_gpioset_index(client, keyword);
	if (gp_set < 0) {
		pr_err("%s: gpio set name does not exist\n",
				__func__);
		return gp_set;
	}

	if (!gpioset_info[client].gpioset_state[gp_set]) {
		/*
		 * If pinctrl pointer is not valid,
		 * no need to proceed further
		 */
		active_set = pinctrl_info[client].active_set;
		if (IS_ERR(pinctrl_info[client].cdc_lines[active_set]))
			return 0;

		pinctrl_info[client].active_set |= (1 << gp_set);
		active_set = pinctrl_info[client].active_set;
		pr_debug("%s: pinctrl.active_set: %d\n", __func__, active_set);

		/* Select the appropriate pinctrl state */
		ret =  pinctrl_select_state(pinctrl_info[client].pinctrl,
			pinctrl_info[client].cdc_lines[active_set]);
	}
	gpioset_info[client].gpioset_state[gp_set]++;

	return ret;
}

int msm_gpioset_suspend(enum pinctrl_client client, char *keyword)
{
	int ret = 0;
	int gp_set = 0;
	int active_set = 0;

	gp_set = msm_get_gpioset_index(client, keyword);
	if (gp_set < 0) {
		pr_err("%s: gpio set name does not exist\n",
				__func__);
		return gp_set;
	}

	if (1 == gpioset_info[client].gpioset_state[gp_set]) {
		pinctrl_info[client].active_set &= ~(1 << gp_set);
		/*
		 * If pinctrl pointer is not valid,
		 * no need to proceed further
		 */
		active_set = pinctrl_info[client].active_set;
		if (IS_ERR(pinctrl_info[client].cdc_lines[active_set]))
			return -EINVAL;

		pr_debug("%s: pinctrl.active_set: %d\n", __func__,
				pinctrl_info[client].active_set);
		/* Select the appropriate pinctrl state */
		ret =  pinctrl_select_state(pinctrl_info[client].pinctrl,
			pinctrl_info[client].cdc_lines[pinctrl_info[client].
								active_set]);
	}
	if (!(gpioset_info[client].gpioset_state[gp_set])) {
		pr_err("%s: Invalid call to de activate gpios: %d\n", __func__,
				gpioset_info[client].gpioset_state[gp_set]);
		return -EINVAL;
	}

	gpioset_info[client].gpioset_state[gp_set]--;

	return ret;
}
