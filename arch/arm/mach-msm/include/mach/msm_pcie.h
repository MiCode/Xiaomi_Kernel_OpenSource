/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_PCIE_H
#define __ASM_ARCH_MSM_PCIE_H

#include <linux/types.h>

/* gpios */
enum msm_pcie_gpio {
	MSM_PCIE_GPIO_RST_N,
	MSM_PCIE_GPIO_PWR_EN,
	MSM_PCIE_MAX_GPIO
};

/* gpio info structure */
struct msm_pcie_gpio_info_t {
	char      *name;
	uint32_t   num;
	uint32_t   on;
};

/* msm pcie platfrom data */
struct msm_pcie_platform {
	struct msm_pcie_gpio_info_t  *gpio;

	uint32_t                      axi_addr;
	uint32_t                      axi_size;
	uint32_t                      wake_n;
};

#endif
