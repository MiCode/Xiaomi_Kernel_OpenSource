/*
 * Copyright (C) 2010-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DW9718_H__
#define __DW9718_H__

#include <linux/miscdevice.h>
#include <media/nvc_focus.h>
#include <media/nvc.h>

struct dw9718_power_rail {
	struct regulator *vdd;
	struct regulator *vdd_i2c;
};

struct dw9718_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	int gpio_count;
	struct nvc_gpio_pdata *gpio;
	int (*power_on)(struct dw9718_power_rail *pw);
	int (*power_off)(struct dw9718_power_rail *pw);
	int (*detect)(void *buf, size_t size);
};

/* Register Definitions */
#define DW9718_POWER_DN		0x00
#define DW9718_CONTROL			0x01
#define DW9718_VCM_CODE_MSB		0x02
#define DW9718_VCM_CODE_LSB		0x03
#define DW9718_SWITCH_MODE		0x04
#define DW9718_SACT			0x05
#define DW9718_STATUS			0x06

#endif  /* __DW9718_H__ */
