/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef QPNP_PON_H
#define QPNP_PON_H

#include <linux/errno.h>

#ifdef CONFIG_QPNP_POWER_ON
int qpnp_pon_system_pwr_off(bool reset);
#else
static int qpnp_pon_system_pwr_off(bool reset) { return -ENODEV; }
#endif

#endif
