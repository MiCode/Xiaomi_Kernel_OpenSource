/*
 * Copyright (C) 2015, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author:
 * Ramalingam C <ramalingam.c@intel.com>
 */

#include <core/common/dsi/intel_dsi_drrs.h>

struct drrs_dsi_platform_ops vlv_dsi_drrs_ops = {
	.configure_dsi_pll = vlv_drrs_configure_dsi_pll,
	.mnp_calculate_for_pclk	= vlv_dsi_mnp_calculate_for_pclk,
};

inline struct drrs_dsi_platform_ops *vlv_dsi_drrs_ops_init(void)
{
	return &vlv_dsi_drrs_ops;
}
