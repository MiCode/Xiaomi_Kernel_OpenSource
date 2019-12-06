/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _KGSL_UTIL_H_
#define _KGSL_UTIL_H_

struct regulator;

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

#endif
