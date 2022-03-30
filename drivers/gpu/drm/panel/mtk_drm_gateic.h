/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_GATEIC_H_
#define _MTK_DRM_GATEIC_H_

#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include "mtk_drm_panel_common.h"
#include "../mediatek/mediatek_v2/mtk_log.h"

#define MTK_LCM_NAME_LENGTH (128)

#define DO_LCM_KZALLOC(buf, size, flag, debug) \
do { \
	buf = kzalloc(roundup(size, 4), flag); \
	if (buf != NULL) \
		mtk_lcm_total_size += size; \
	if (debug == 1 && buf != NULL) \
		pr_notice("%s, %d, buf:0x%lx, size:%u, align:%u, flag:0x%x\n", \
			__func__, __LINE__, (unsigned long)buf, \
			(unsigned int)size, (unsigned int)roundup(size, 4), \
			(unsigned int)flag); \
} while (0)

#define LCM_KZALLOC(buf, size, flag) DO_LCM_KZALLOC(buf, size, flag, 0)

#define DO_LCM_KFREE(buf, size, debug) \
do { \
	if (debug == 1) \
		pr_notice("%s, %d, size:%u\n", \
			__func__, __LINE__, (unsigned int)roundup(size, 4)); \
	if (mtk_lcm_total_size >= roundup(size, 4)) \
		mtk_lcm_total_size -= roundup(size, 4); \
	else \
		mtk_lcm_total_size = 0; \
	kfree(buf); \
	buf = NULL; \
} while (0)

#define LCM_KFREE(buf, size) DO_LCM_KFREE(buf, size, 0)

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
	unsigned int lcm_count;
	const char **lcm_list;
};

struct mtk_gateic_funcs {
	struct list_head list;

	/* check if lcm is supported by this gateic*/
	int (*match_lcm_list)(const char *lcm_name);

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
int mtk_drm_gateic_register(struct mtk_gateic_funcs *ops, char func);

/* function: select a gateic ops fort current panel
 * input: lcm_name: panel name, func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 * mention that each gate ic should have customized lcm list in dts node
 */
int mtk_drm_gateic_select(const char *lcm_name, char func);

/* function: get gateic ops of panel driver
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
struct mtk_gateic_funcs *mtk_drm_gateic_get(char func);

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

/* function: check if lcm is supported by current gateic
 * input: lcm_name, gateic supported panel list and panel count
 * output: false for no, true for yes
 */
bool mtk_gateic_match_lcm_list(const char *lcm_name,
	const char **list, unsigned int count, const char *gateic_name);

#endif
