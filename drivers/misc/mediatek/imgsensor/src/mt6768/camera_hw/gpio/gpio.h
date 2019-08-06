/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __IMGSENSOR_HW_GPIO_H__
#define __IMGSENSOR_HW_GPIO_H__
#include "imgsensor_common.h"
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include "imgsensor_hw.h"

extern struct mutex pinctrl_mutex;

enum GPIO_CTRL_STATE {
	/* Main */
	GPIO_CTRL_STATE_CAM0_PDN_H,
	GPIO_CTRL_STATE_CAM0_PDN_L,
	GPIO_CTRL_STATE_CAM0_RST_H,
	GPIO_CTRL_STATE_CAM0_RST_L,
	GPIO_CTRL_STATE_LDO_VCAMA_H,
	GPIO_CTRL_STATE_LDO_VCAMA_L,
	GPIO_CTRL_STATE_LDO_VCAMD_H,
	GPIO_CTRL_STATE_LDO_VCAMD_L,
	GPIO_CTRL_STATE_LDO_VCAMIO_H,
	GPIO_CTRL_STATE_LDO_VCAMIO_L,
	GPIO_CTRL_STATE_LDO_VCAMAF_H,
	GPIO_CTRL_STATE_LDO_VCAMAF_L,
	/* Sub */
	GPIO_CTRL_STATE_CAM1_PDN_H,
	GPIO_CTRL_STATE_CAM1_PDN_L,
	GPIO_CTRL_STATE_CAM1_RST_H,
	GPIO_CTRL_STATE_CAM1_RST_L,
	GPIO_CTRL_STATE_LDO_SUB_VCAMA_H,
	GPIO_CTRL_STATE_LDO_SUB_VCAMA_L,
	GPIO_CTRL_STATE_LDO_SUB_VCAMD_H,
	GPIO_CTRL_STATE_LDO_SUB_VCAMD_L,
	/* Main2 */
	GPIO_CTRL_STATE_CAM2_PDN_H,
	GPIO_CTRL_STATE_CAM2_PDN_L,
	GPIO_CTRL_STATE_CAM2_RST_H,
	GPIO_CTRL_STATE_CAM2_RST_L,
	GPIO_CTRL_STATE_LDO_MAIN2_VCAMA_H,
	GPIO_CTRL_STATE_LDO_MAIN2_VCAMA_L,
	GPIO_CTRL_STATE_LDO_MAIN2_VCAMD_H,
	GPIO_CTRL_STATE_LDO_MAIN2_VCAMD_L,
	/* Sub2 */
	GPIO_CTRL_STATE_CAM3_PDN_H,
	GPIO_CTRL_STATE_CAM3_PDN_L,
	GPIO_CTRL_STATE_CAM3_RST_H,
	GPIO_CTRL_STATE_CAM3_RST_L,
	GPIO_CTRL_STATE_LDO_SUB2_VCAMA_H,
	GPIO_CTRL_STATE_LDO_SUB2_VCAMA_L,
	GPIO_CTRL_STATE_LDO_SUB2_VCAMD_H,
	GPIO_CTRL_STATE_LDO_SUB2_VCAMD_L,
	/* Main3 */
	GPIO_CTRL_STATE_CAM4_PDN_H,
	GPIO_CTRL_STATE_CAM4_PDN_L,
	GPIO_CTRL_STATE_CAM4_RST_H,
	GPIO_CTRL_STATE_CAM4_RST_L,
	GPIO_CTRL_STATE_LDO_MAIN3_VCAMA_H,
	GPIO_CTRL_STATE_LDO_MAIN3_VCAMA_L,
	GPIO_CTRL_STATE_LDO_MAIN3_VCAMD_H,
	GPIO_CTRL_STATE_LDO_MAIN3_VCAMD_L,

#ifdef MIPI_SWITCH
	GPIO_CTRL_STATE_MIPI_SWITCH_EN_H,
	GPIO_CTRL_STATE_MIPI_SWITCH_EN_L,
	GPIO_CTRL_STATE_MIPI_SWITCH_SEL_H,
	GPIO_CTRL_STATE_MIPI_SWITCH_SEL_L,
#endif

	GPIO_CTRL_STATE_MAX_NUM
};

enum GPIO_STATE {
	GPIO_STATE_H,
	GPIO_STATE_L,
};

struct GPIO_PINCTRL {
	char *ppinctrl_lookup_names;
};

struct GPIO {
	struct pinctrl       *ppinctrl;
	struct pinctrl_state *ppinctrl_state[GPIO_CTRL_STATE_MAX_NUM];
};

enum IMGSENSOR_RETURN
imgsensor_hw_gpio_open(struct IMGSENSOR_HW_DEVICE **pdevice);

extern struct platform_device *gpimgsensor_hw_platform_device;

#endif

