/*
 * OF helpers for regulator framework
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

static void of_get_regulation_constraints(struct device_node *np,
					struct regulator_init_data **init_data)
{
	const __be32 *min_uV, *max_uV, *uV_offset;
	const __be32 *min_uA, *max_uA, *ramp_delay;
	struct regulation_constraints *constraints = &(*init_data)->constraints;

	constraints->name = of_get_property(np, "regulator-name", NULL);

	min_uV = of_get_property(np, "regulator-min-microvolt", NULL);
	if (min_uV)
		constraints->min_uV = be32_to_cpu(*min_uV);
	max_uV = of_get_property(np, "regulator-max-microvolt", NULL);
	if (max_uV)
		constraints->max_uV = be32_to_cpu(*max_uV);

	/* Voltage change possible? */
	if (constraints->min_uV != constraints->max_uV)
		constraints->valid_ops_mask |= REGULATOR_CHANGE_VOLTAGE;
	/* Only one voltage?  Then make sure it's set. */
	if (min_uV && max_uV && constraints->min_uV == constraints->max_uV)
		constraints->apply_uV = true;

	uV_offset = of_get_property(np, "regulator-microvolt-offset", NULL);
	if (uV_offset)
		constraints->uV_offset = be32_to_cpu(*uV_offset);
	min_uA = of_get_property(np, "regulator-min-microamp", NULL);
	if (min_uA)
		constraints->min_uA = be32_to_cpu(*min_uA);
	max_uA = of_get_property(np, "regulator-max-microamp", NULL);
	if (max_uA)
		constraints->max_uA = be32_to_cpu(*max_uA);

	/* Current change possible? */
	if (constraints->min_uA != constraints->max_uA)
		constraints->valid_ops_mask |= REGULATOR_CHANGE_CURRENT;

	if (of_find_property(np, "regulator-boot-on", NULL))
		constraints->boot_on = true;

	if (of_find_property(np, "regulator-always-on", NULL))
		constraints->always_on = true;
	else /* status change should be possible if not always on. */
		constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;

	ramp_delay = of_get_property(np, "regulator-ramp-delay", NULL);
	if (ramp_delay)
		constraints->ramp_delay = be32_to_cpu(*ramp_delay);
}

static const char *consumer_supply_prop_name = "qcom,consumer-supplies";
#define MAX_DEV_NAME_LEN 256
/*
 * Fill in regulator init_data based on qcom legacy requirements.
 */
static int of_get_qcom_regulator_init_data(struct device *dev,
					struct regulator_init_data **init_data)
{
	struct device_node *node = dev->of_node;
	struct regulator_consumer_supply *consumer_supplies;
	int i, rc, num_consumer_supplies, array_len;

	array_len = of_property_count_strings(node, consumer_supply_prop_name);
	if (array_len > 0) {
		/* Array length must be divisible by 2. */
		if (array_len & 1) {
			dev_err(dev, "error: %s device node property value "
				"contains an odd number of elements: %d\n",
				consumer_supply_prop_name, array_len);
			return -EINVAL;
		}
		num_consumer_supplies = array_len / 2;

		consumer_supplies = devm_kzalloc(dev,
			sizeof(struct regulator_consumer_supply)
			* num_consumer_supplies, GFP_KERNEL);
		if (consumer_supplies == NULL) {
			dev_err(dev, "devm_kzalloc failed\n");
			return -ENOMEM;
		}

		for (i = 0; i < num_consumer_supplies; i++) {
			rc = of_property_read_string_index(node,
				consumer_supply_prop_name, i * 2,
				&consumer_supplies[i].supply);
			if (rc) {
				dev_err(dev, "of_property_read_string_index "
					"failed, rc=%d\n", rc);
				devm_kfree(dev, consumer_supplies);
				return rc;
			}

			rc = of_property_read_string_index(node,
				consumer_supply_prop_name, (i * 2) + 1,
				&consumer_supplies[i].dev_name);
			if (rc) {
				dev_err(dev, "of_property_read_string_index "
					"failed, rc=%d\n", rc);
				devm_kfree(dev, consumer_supplies);
				return rc;
			}

			/* Treat dev_name = "" as a wildcard. */
			if (strnlen(consumer_supplies[i].dev_name,
					MAX_DEV_NAME_LEN) == 0)
				consumer_supplies[i].dev_name = NULL;
		}

		(*init_data)->consumer_supplies = consumer_supplies;
		(*init_data)->num_consumer_supplies = num_consumer_supplies;
	}

	return 0;
}

/**
 * of_get_regulator_init_data - extract regulator_init_data structure info
 * @dev: device requesting for regulator_init_data
 *
 * Populates regulator_init_data structure by extracting data from device
 * tree node, returns a pointer to the populated struture or NULL if memory
 * alloc fails.
 */
struct regulator_init_data *of_get_regulator_init_data(struct device *dev,
						struct device_node *node)
{
	struct regulator_init_data *init_data;
	int rc;

	if (!node)
		return NULL;

	init_data = devm_kzalloc(dev, sizeof(*init_data), GFP_KERNEL);
	if (!init_data)
		return NULL; /* Out of memory? */

	of_get_regulation_constraints(node, &init_data);
	rc = of_get_qcom_regulator_init_data(dev, &init_data);
	if (rc) {
		devm_kfree(dev, init_data);
		return NULL;
	}

	return init_data;
}
EXPORT_SYMBOL_GPL(of_get_regulator_init_data);

/**
 * of_regulator_match - extract multiple regulator init data from device tree.
 * @dev: device requesting the data
 * @node: parent device node of the regulators
 * @matches: match table for the regulators
 * @num_matches: number of entries in match table
 *
 * This function uses a match table specified by the regulator driver to
 * parse regulator init data from the device tree. @node is expected to
 * contain a set of child nodes, each providing the init data for one
 * regulator. The data parsed from a child node will be matched to a regulator
 * based on either the deprecated property regulator-compatible if present,
 * or otherwise the child node's name. Note that the match table is modified
 * in place.
 *
 * Returns the number of matches found or a negative error code on failure.
 */
int of_regulator_match(struct device *dev, struct device_node *node,
		       struct of_regulator_match *matches,
		       unsigned int num_matches)
{
	unsigned int count = 0;
	unsigned int i;
	const char *name;
	struct device_node *child;

	if (!dev || !node)
		return -EINVAL;

	for (i = 0; i < num_matches; i++) {
		struct of_regulator_match *match = &matches[i];
		match->init_data = NULL;
		match->of_node = NULL;
	}

	for_each_child_of_node(node, child) {
		name = of_get_property(child,
					"regulator-compatible", NULL);
		if (!name)
			name = child->name;
		for (i = 0; i < num_matches; i++) {
			struct of_regulator_match *match = &matches[i];
			if (match->of_node)
				continue;

			if (strcmp(match->name, name))
				continue;

			match->init_data =
				of_get_regulator_init_data(dev, child);
			if (!match->init_data) {
				dev_err(dev,
					"failed to parse DT for regulator %s\n",
					child->name);
				return -EINVAL;
			}
			match->of_node = child;
			count++;
			break;
		}
	}

	return count;
}
EXPORT_SYMBOL_GPL(of_regulator_match);
