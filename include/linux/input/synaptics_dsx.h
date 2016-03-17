/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SYNAPTICS_DSX_H_
#define _SYNAPTICS_DSX_H_

#define PLATFORM_DRIVER_NAME "synaptics_dsx"
#define ACTIVE_PEN_DRIVER_NAME "synaptics_dsx_active_pen"
#define PROXIMITY_DRIVER_NAME "synaptics_dsx_proximity"
#define I2C_DRIVER_NAME "synaptics_dsx_i2c"
#define SPI_DRIVER_NAME "synaptics_dsx_spi"
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_EDGE_SUPPORT
#define PLATFORM_EDGE_DRIVER_NAME "synaptics_dsx_edge"
#endif
/*
 * struct synaptics_dsx_button_map - button map
 * @nbuttons: number of buttons
 * @map: pointer to array of button codes
 */
struct synaptics_dsx_button_map {
	unsigned char nbuttons;
	unsigned int *map;
};

struct synaptics_dsx_config_info {
	unsigned char *tp_ids;
	const char *fw_name;
};

enum synaptics_dsx_lockdown_area {
	LOCKDOWN_AREA_PRODUCT_ID = 0,
	LOCKDOWN_AREA_GUEST_SERIALIZATION = 1,
	LOCKDOWN_AREA_UNKNOWN = 0xFF,
};

/*
 * struct synaptics_dsx_board_data - DSX board data
 * @x_flip: x flip flag
 * @y_flip: y flip flag
 * @swap_axes: swap axes flag
 * @irq_gpio: attention interrupt GPIO
 * @irq_on_state: attention interrupt active state
 * @power_gpio: power switch GPIO
 * @power_on_state: power switch active state
 * @reset_gpio: reset GPIO
 * @reset_on_state: reset active state
 * @max_y_for_2d: maximum y value for 2D area when virtual buttons are present
 * @irq_flags: IRQ flags
 * @i2c_addr: I2C slave address
 * @ub_i2c_addr: microbootloader mode I2C slave address
 * @device_descriptor_addr: HID device descriptor address
 * @panel_x: x-axis resolution of display panel
 * @panel_y: y-axis resolution of display panel
 * @power_delay_ms: delay time to wait after powering up device
 * @reset_delay_ms: delay time to wait after resetting device
 * @reset_active_ms: reset active time
 * @byte_delay_us: delay time between two bytes of SPI data
 * @block_delay_us: delay time between two SPI transfers
 * @pwr_reg_name: pointer to name of regulator for power control
 * @bus_reg_name: pointer to name of regulator for bus pullup control
 * @cap_button_map: pointer to 0D button map
 * @vir_button_map: pointer to virtual button map
 */
struct synaptics_dsx_board_data {
	int id;
	bool x_flip;
	bool y_flip;
	bool swap_axes;
	int irq_gpio;
	int irq_on_state;
	int power_gpio;
	int power_on_state;
	int reset_gpio;
	int reset_on_state;
	int max_y_for_2d;
	int config_array_size;
	int tp_id_num;
	unsigned char *tp_id_bytes;
	unsigned long irq_flags;
	unsigned short i2c_addr;
	unsigned short ub_i2c_addr;
	unsigned short device_descriptor_addr;
	unsigned int panel_x;
	unsigned int panel_y;
	unsigned int max_major;
	unsigned int max_minor;
	unsigned int max_finger_num;
	unsigned int power_delay_ms;
	unsigned int reset_delay_ms;
	unsigned int reset_active_ms;
	unsigned int byte_delay_us;
	unsigned int block_delay_us;
	u32 power_gpio_flags;
	u32 reset_gpio_flags;
	u32 irq_gpio_flags;
	bool cut_off_power;
	bool use_charger_bit;
	int (*gpio_config)(int gpio, bool configure, int dir, int state);
	const char *pwr_reg_name;
	const char *bus_reg_name;
	const char *power_gpio_name;
	const char *reset_gpio_name;
	const char *irq_gpio_name;
	struct synaptics_dsx_button_map *cap_button_map;
	struct synaptics_dsx_button_map *vir_button_map;
	struct synaptics_dsx_config_info *config_array;
	enum synaptics_dsx_lockdown_area lockdown_area;
};

#endif
