/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __RADIO_SILABS_TRANSPORT_H
#define __RADIO_SILABS_TRANSPORT_H

#define WRITE_REG_NUM		8
#define READ_REG_NUM		16

#define DEBUG
#define FMDBG(fmt, args...) pr_debug("silabs_radio: " fmt, ##args)

#define FMDERR(fmt, args...) pr_err("silabs_radio: " fmt, ##args)

int silabs_fm_i2c_read(u8 *buf, u8 len);
int silabs_fm_i2c_write(u8 *buf, u8 len);
int get_int_gpio_number(void);
int silabs_fm_power_cfg(int on);

struct fm_power_vreg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage levels to be set */
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	bool set_voltage_sup;
	/* is this regulator enabled? */
	bool is_enabled;
};


struct fm_i2c_device_data {
	struct i2c_client *client;
	struct pwm_device *pwm;
	bool is_len_gpio_valid;
	struct fm_power_vreg_data *dreg;
	struct fm_power_vreg_data *areg;
	int reset_gpio;
	int int_gpio;
	int status_gpio;
	struct pinctrl *fm_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

#endif /* __RADIO_SILABS_TRANSPORT_H */
