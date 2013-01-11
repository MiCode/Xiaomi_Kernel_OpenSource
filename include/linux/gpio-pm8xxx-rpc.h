/*
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

/*
 * Qualcomm PMIC8XXX gpio rpc driver header file
 *
 */

#ifndef __GPIO_PM8XXX_RPC_H
#define __GPIO_PM8XXX_RPC_H

#define PM8XXX_GPIO_DEV_NAME	"pm8xxx-gpio-rpc"

struct pm8xxx_gpio_rpc_platform_data {
	int	ngpios;
	int	gpio_base;
};

/* GPIO parameters */
/* direction */
#define	PM_GPIO_DIR_OUT			0x01
#define	PM_GPIO_DIR_IN			0x02
#define	PM_GPIO_DIR_BOTH		(PM_GPIO_DIR_OUT | PM_GPIO_DIR_IN)

#endif
