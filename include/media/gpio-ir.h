/* Copyright (C) 2012 by Xiang Xiao <xiaoxiang@xiaomi.com>
 * Copyright (C) 2016 XiaoMi, Inc.
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

#ifndef __GPIO_IR_H__
#define __GPIO_IR_H__

#define GPIO_IR_NAME	"gpio-ir"

struct gpio_ir_data {
	const char  *tx_reg_id;
	unsigned int tx_gpio_nr;
	bool         tx_high_active;
	bool         tx_soft_carrier;
	bool         tx_disable_rx;
	bool         tx_with_timer;
	const char  *rx_reg_id;
	unsigned int rx_gpio_nr;
	bool         rx_high_active;
	bool         rx_soft_carrier;
	u64          rx_init_protos;
	bool         rx_can_wakeup;
	const char  *rx_map_name;
};

#endif /* __GPIO_IR_H__ */
