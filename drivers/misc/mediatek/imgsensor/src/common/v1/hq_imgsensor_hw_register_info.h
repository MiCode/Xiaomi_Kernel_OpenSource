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

#ifndef __IMGSENSOR_H__
#define __IMGSENSOR_H__
#define IMGSENSOR_DEVICE_NNUMBER 255

#include "imgsensor_common.h"
#include "imgsensor_i2c.h"
#include "imgsensor_hw.h"
#include "imgsensor_clk.h"
#include "hq_imgsensor_hw_register_info.c"
struct IMGSENSOR_STATUS {
	u32 reserved:31;
	u32 oc:1;
};
struct IMGSENSOR {
	struct IMGSENSOR_STATUS status;
	struct IMGSENSOR_HW     hw;
	struct IMGSENSOR_CLK    clk;
	struct IMGSENSOR_SENSOR sensor[IMGSENSOR_SENSOR_IDX_MAX_NUM];
	atomic_t imgsensor_open_cnt;
	enum IMGSENSOR_RETURN (*imgsensor_oc_irq_enable)
			(enum IMGSENSOR_SENSOR_IDX sensor_idx, bool enable);

	/*for driving current control*/
	enum IMGSENSOR_RETURN (*mclk_set_drive_current)
		(void *pinstance,
		enum IMGSENSOR_SENSOR_IDX sensor_idx,
		enum ISP_DRIVING_CURRENT_ENUM drive_current);
};

MINT8 hq_imgsensor_sensor_hw_register(struct IMGSENSOR_SENSOR *psensor, struct IMGSENSOR_SENSOR_INST *psensor_inst);


#endif
