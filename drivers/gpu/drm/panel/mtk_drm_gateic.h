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
extern struct platform_driver mtk_gateic_rt4831a_driver;
extern struct i2c_driver mtk_panel_i2c_driver;

struct mtk_gateic_data {
	struct device *dev;
	atomic_t init;
	atomic_t ref;
	atomic_t backlight_status;
	unsigned int backlight_level;
	unsigned int backlight_mode;
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
	int (*set_voltage)(unsigned int level);

	/* reset panel */
	int (*reset)(int on);

	/* enable backlight */
	int (*enable_backlight)(void);

	/* set backlight */
	int (*set_backlight)(unsigned int level);
};

/* function: register gateic ops of panel driver
 * input: gateic ops: panel driver pointer, func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 * mention that gatic driver should be probed before panel driver,
 * else this function will be failed.
 */
int mtk_drm_gateic_set(struct mtk_gateic_funcs *gateic_ops, char func);

/* function: get gateic ops of panel driver
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
struct mtk_gateic_funcs *mtk_drm_gateic_get(char func);

/* function: panel power on
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_power_on(char func);

/* function: panel power off
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_power_off(char func);

/* function: set panel power voltage
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_set_voltage(unsigned int level, char func);

/* function: reset panel
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_reset(int on, char func);

/* function: set backlight
 * input: func: DBI, DPI, DSI, brightness level
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_set_backlight(unsigned int level, char func);

/* function: enable backlight
 * input: func: DBI, DPI, DSI,
 *		enable: 1 for enable, 0 for disable
 *		pwm_enable: set backlight by pwm or not
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_enable_backlight(char func);

/* function: write i2c data
 * input: addr: i2c address, value: i2c value
 * output: 0 for success; !0 for failed
 */
int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
int mtk_panel_i2c_read_bytes(unsigned char addr, unsigned char *value);

/* function: write i2c data buffer
 * input: addr: i2c address,
 *		value: data buffer
 *		size: data size
 * output: 0 for success; !0 for failed
 */
int mtk_panel_i2c_write_multiple_bytes(unsigned char addr, unsigned char *value,
		unsigned int size);

/* function: write gateic data
 * input: addr: i2c address,
 *		value: i2c value
 *		func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_write_bytes(unsigned char addr,
		 unsigned char value, char func);
int mtk_drm_gateic_read_bytes(unsigned char addr,
		 unsigned char *value, char func);

/* function: write gateic data buffer
 * input: addr: i2c address,
 *		value: data buffer
 *		size: data size
 *		func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
int mtk_drm_gateic_write_multiple_bytes(unsigned char addr,
		 unsigned char *value, unsigned int size, char func);

/*********** led interfaces define **********/
#define _gate_ic_i2c_write_bytes(addr, value) \
	mtk_drm_gateic_write_bytes(addr, value, MTK_LCM_FUNC_DSI)

#define _gate_ic_i2c_read_bytes(addr, value) \
	mtk_drm_gateic_read_bytes(addr, value, MTK_LCM_FUNC_DSI)

#define _gate_ic_i2c_write_regs(addr, value, size) \
	mtk_drm_gateic_write_multiple_bytes(addr, value, size, \
				MTK_LCM_FUNC_DSI)

/*do nothing just combine the legacy interfaces
 * all the actions has been moved to power_on/off callback
 */
#define _gate_ic_i2c_panel_bias_enable(power_status) { }

#define _gate_ic_Power_on() \
	mtk_drm_gateic_power_on(MTK_LCM_FUNC_DSI)

#define _gate_ic_Power_off() \
	mtk_drm_gateic_power_off(MTK_LCM_FUNC_DSI)

#define _gate_ic_i2c_backight_level_set(level) \
	mtk_drm_gateic_set_backlight(level, MTK_LCM_FUNC_DSI)
/************************************/

#endif
