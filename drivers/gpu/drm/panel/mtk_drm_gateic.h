/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_GATEIC_H_
#define _MTK_DRM_GATEIC_H_

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include "dt-bindings/lcm/mtk_lcm_settings.h"
#include "../mediatek/mediatek_v2/mtk_log.h"

extern struct platform_driver mtk_gateic_rt4801h_driver;
extern struct i2c_driver mtk_panel_i2c_driver;

enum vol_level {
	VOL_4_0V = 40, /* VOL_4_0V means 4.0V */
	VOL_4_1V,
	VOL_4_2V,
	VOL_4_3V,
	VOL_4_4V,
	VOL_4_5V,
	VOL_4_6V,
	VOL_4_7V,
	VOL_4_8V,
	VOL_4_9V,
	VOL_5_0V,      /* VOL_5_0V means 5.0V */
	VOL_5_1V,
	VOL_5_2V,
	VOL_5_3V,
	VOL_5_4V,
	VOL_5_5V,
	VOL_5_6V,
	VOL_5_7V,
	VOL_5_8V,
	VOL_5_9V,
	VOL_6_0V,      /* VOL_6_0V means 6.0V */
};

struct mtk_gateic_data {
	struct device *dev;
	unsigned int init;
	unsigned int ref;
	/* for gate IC of gpio control */
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos_gpio;
	struct gpio_desc *bias_neg_gpio;
	/* for gate IC of pmic control */
	struct regulator *bias_pos_reg;
	struct regulator *bias_neg_reg;
};

struct mtk_gateic_funcs {
	/* power on with default voltage */
	int (*power_on)(void);

	/* power off panel */
	int (*power_off)(void);

	/* function: update voltage settings as panel spec requirement
	 * input: the voltage level required by panel spec,
	 * mention that gate IC will automatically transfer
	 * the input of voltage level to HW settings.
	 */
	int (*set_voltage)(enum vol_level level);

	/* reset panel */
	int (*reset)(int on);
};

/* function: register gateic ops of panel driver
 * input: panel driver pointer
 * output: 0 for success; !0 for failed
 * mention that gatic driver should be probed before panel driver,
 * else this function will be failed.
 */
int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value);

/* function: register gateic ops of panel driver
 * input: gateic ops: panel driver pointer, type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 * mention that gatic driver should be probed before panel driver,
 * else this function will be failed.
 */
int mtk_drm_gateic_set(struct mtk_gateic_funcs *gateic_ops, char func);

/* function: get gateic ops of panel driver
 * input: type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
struct mtk_gateic_funcs *mtk_drm_gateic_get(char func);

/* function: panel power on
 * input: type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_power_on(char func);

/* function: panel power off
 * input: type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_power_off(char func);

/* function: set panel power voltage
 * input: type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_set_voltage(enum vol_level level, char func);

/* function: reset panel
 * input: type: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_reset(int on, char func);

#endif
