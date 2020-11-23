/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_HW_MCLK_h__
#define __IMGSENSOR_HW_MCLK_h__

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "kd_camera_typedef.h"
#include "imgsensor_hw.h"
#include "imgsensor_common.h"
#include "imgsensor.h"

extern struct IMGSENSOR gimgsensor;

enum MCLK_STATE {
	MCLK_STATE_DISABLE = 0,
	MCLK_STATE_ENABLE_2MA,
	MCLK_STATE_ENABLE_4MA,
	MCLK_STATE_ENABLE_6MA,
	MCLK_STATE_ENABLE_8MA,
	MCLK_STATE_MAX_NUM,
};

struct MCLK_PINCTRL_NAMES {
	char *ppinctrl_names;
};

struct mclk {
	struct pinctrl       *ppinctrl;
	struct pinctrl_state *ppinctrl_state[
		IMGSENSOR_SENSOR_IDX_MAX_NUM][MCLK_STATE_MAX_NUM];
	struct mutex         *pmclk_mutex;
	int drive_current[IMGSENSOR_SENSOR_IDX_MAX_NUM];
};

enum IMGSENSOR_RETURN
	imgsensor_hw_mclk_open(struct IMGSENSOR_HW_DEVICE **pdevice);


#endif

