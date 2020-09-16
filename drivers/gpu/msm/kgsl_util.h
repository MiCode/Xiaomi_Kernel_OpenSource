/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _KGSL_UTIL_H_
#define _KGSL_UTIL_H_

struct regulator;
struct clk_bulk_data;

/**
 * kgsl_regulator_disable_wait - Disable a regulator and wait for it
 * @reg: A &struct regulator handle
 * @timeout: Time to wait (in milliseconds)
 *
 * Disable the regulator and wait @timeout milliseconds for it to enter the
 * disabled state.
 *
 * Return: True if the regulator was disabled or false if it timed out
 */
bool kgsl_regulator_disable_wait(struct regulator *reg, u32 timeout);

/**
 * kgsl_of_clk_by_name - Return a clock device for a given name
 * @clks: Pointer to an array of bulk clk data
 * @count: Number of entries in the array
 * @id: Name of the clock to search for
 *
 * Returns: A pointer to the clock device for the given name or NULL if not
 * found
 */
struct clk *kgsl_of_clk_by_name(struct clk_bulk_data *clks, int count,
		const char *id);
/**
 * kgsl_regulator_set_voltage - Set voltage level for regulator
 * @dev: A &struct device pointer
 * @reg: A &struct regulator handle
 * @voltage: Voltage value to set regulator
 *
 * Return: 0 on success and negative error on failure.
 */
int kgsl_regulator_set_voltage(struct device *dev,
		struct regulator *reg, u32 voltage);
#endif
