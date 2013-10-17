/* Copyright (c) 2010-2011,2013, The Linux Foundation. All rights reserved.
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
#ifndef __ARCH_ARM_MACH_MSM_GPIOMUX_H
#define __ARCH_ARM_MACH_MSM_GPIOMUX_H

#include <linux/bitops.h>
#include <linux/errno.h>

enum msm_gpiomux_setting {
	GPIOMUX_ACTIVE = 0,
	GPIOMUX_SUSPENDED,
	GPIOMUX_NSETTINGS
};

enum gpiomux_drv {
	GPIOMUX_DRV_2MA = 0,
	GPIOMUX_DRV_4MA,
	GPIOMUX_DRV_6MA,
	GPIOMUX_DRV_8MA,
	GPIOMUX_DRV_10MA,
	GPIOMUX_DRV_12MA,
	GPIOMUX_DRV_14MA,
	GPIOMUX_DRV_16MA,
};

enum gpiomux_func {
	GPIOMUX_FUNC_GPIO = 0,
	GPIOMUX_FUNC_1,
	GPIOMUX_FUNC_2,
	GPIOMUX_FUNC_3,
	GPIOMUX_FUNC_4,
	GPIOMUX_FUNC_5,
	GPIOMUX_FUNC_6,
	GPIOMUX_FUNC_7,
	GPIOMUX_FUNC_8,
	GPIOMUX_FUNC_9,
	GPIOMUX_FUNC_A,
	GPIOMUX_FUNC_B,
	GPIOMUX_FUNC_C,
	GPIOMUX_FUNC_D,
	GPIOMUX_FUNC_E,
	GPIOMUX_FUNC_F,
};

enum gpiomux_pull {
	GPIOMUX_PULL_NONE = 0,
	GPIOMUX_PULL_DOWN,
	GPIOMUX_PULL_KEEPER,
	GPIOMUX_PULL_UP,
};

/* Direction settings are only meaningful when GPIOMUX_FUNC_GPIO is selected.
 * This element is ignored for all other FUNC selections, as the output-
 * enable pin is not under software control in those cases.  See the SWI
 * for your target for more details.
 */
enum gpiomux_dir {
	GPIOMUX_IN = 0,
	GPIOMUX_OUT_HIGH,
	GPIOMUX_OUT_LOW,
};

struct gpiomux_setting {
	enum gpiomux_func func;
	enum gpiomux_drv  drv;
	enum gpiomux_pull pull;
	enum gpiomux_dir  dir;
};

/**
 * struct msm_gpiomux_config: gpiomux settings for one gpio line.
 *
 * A complete gpiomux config is the combination of a drive-strength,
 * function, pull, and (sometimes) direction.  For functions other than GPIO,
 * the input/output setting is hard-wired according to the function.
 *
 * @gpio: The index number of the gpio being described.
 * @settings: The settings to be installed, specifically:
 *           GPIOMUX_ACTIVE: The setting to be installed when the
 *           line is active, or its reference count is > 0.
 *           GPIOMUX_SUSPENDED: The setting to be installed when
 *           the line is suspended, or its reference count is 0.
 */
struct msm_gpiomux_config {
	unsigned gpio;
	struct gpiomux_setting *settings[GPIOMUX_NSETTINGS];
};

/**
 * struct msm_gpiomux_configs: a collection of gpiomux configs.
 *
 * It is so common to manage blocks of gpiomux configs that the data structure
 * for doing so has been standardized here as a convenience.
 *
 * @cfg:  A pointer to the first config in an array of configs.
 * @ncfg: The number of configs in the array.
 */
struct msm_gpiomux_configs {
	struct msm_gpiomux_config *cfg;
	size_t                     ncfg;
};

/* Provide an enum and an API to write to misc TLMM registers */
enum msm_tlmm_misc_reg {
	TLMM_ETM_MODE_REG = 0x2014,
	TLMM_SDC2_HDRV_PULL_CTL = 0x2048,
	TLMM_SPARE_REG = 0x2024,
	TLMM_CDC_HDRV_CTL = 0x2054,
	TLMM_CDC_HDRV_PULL_CTL = 0x2058,
};

void msm_tlmm_misc_reg_write(enum msm_tlmm_misc_reg misc_reg, int val);

#ifdef CONFIG_MSM_GPIOMUX

/* Before using gpiomux, initialize the subsystem by telling it how many
 * gpios are going to be managed.  Calling any other gpiomux functions before
 * msm_gpiomux_init is unsupported.
 */
int msm_gpiomux_init(size_t ngpio);

/* DT Variant of msm_gpiomux_init. This will look up the number of gpios from
 * device tree rather than relying on NR_GPIO_IRQS
 */
int msm_gpiomux_init_dt(void);

/* Install a block of gpiomux configurations in gpiomux.  This is functionally
 * identical to calling msm_gpiomux_write many times.
 */
void msm_gpiomux_install(struct msm_gpiomux_config *configs, unsigned nconfigs);

/* Install a block of gpiomux configurations in gpiomux. Do not however write
 * to hardware. Just store the settings to be retrieved at a later time
 */
void msm_gpiomux_install_nowrite(struct msm_gpiomux_config *configs,
				unsigned nconfigs);

/* Increment a gpio's reference count, possibly activating the line. */
int __must_check msm_gpiomux_get(unsigned gpio);

/* Decrement a gpio's reference count, possibly suspending the line. */
int msm_gpiomux_put(unsigned gpio);

/* Install a new setting in a gpio.  To erase a slot, use NULL.
 * The old setting that was overwritten can be passed back to the caller
 * old_setting can be NULL if the caller is not interested in the previous
 * setting
 * If a previous setting was not available to return (NULL configuration)
 * - the function returns 1
 * else function returns 0
 */
int msm_gpiomux_write(unsigned gpio, enum msm_gpiomux_setting which,
	struct gpiomux_setting *setting, struct gpiomux_setting *old_setting);

/* Architecture-internal function for use by the framework only.
 * This function can assume the following:
 * - the gpio value has passed a bounds-check
 * - the gpiomux spinlock has been obtained
 *
 * This function is not for public consumption.  External users
 * should use msm_gpiomux_write.
 */
void __msm_gpiomux_write(unsigned gpio, struct gpiomux_setting val);
#else
static inline int msm_gpiomux_init(size_t ngpio)
{
	return -ENOSYS;
}

static inline void
msm_gpiomux_install(struct msm_gpiomux_config *configs, unsigned nconfigs) {}

static inline int __must_check msm_gpiomux_get(unsigned gpio)
{
	return -ENOSYS;
}

static inline int msm_gpiomux_put(unsigned gpio)
{
	return -ENOSYS;
}

static inline int msm_gpiomux_write(unsigned gpio,
	enum msm_gpiomux_setting which, struct gpiomux_setting *setting,
	struct gpiomux_setting *old_setting)
{
	return -ENOSYS;
}
#endif
#endif
