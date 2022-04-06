/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __N3D_H__
#define __N3D_H__

#include <linux/mutex.h>

#include "n3d_clk.h"
#include "n3d_hw.h"
#include "kd_imgsensor_define.h"
#include "frame-sync/frame_sync.h"

#define N3D_DEV_NAME "seninf_n3d"

struct SENINF_N3D {
	dev_t dev_no;
	struct cdev *pchar_dev;
	struct class *pclass;
	struct device *dev;

	struct SENINF_N3D_CLK clk;
	struct base_reg regs;

	struct mutex n3d_mutex;
	int is_powered;

	struct sensor_info *sync_sensors[MAX_NUM_OF_SUPPORT_SENSOR];
	unsigned int fl_result[MAX_NUM_OF_SUPPORT_SENSOR];
	int sensor_streaming[MAX_NUM_OF_SUPPORT_SENSOR];
	int sync_state;
	struct FrameSync *fsync_mgr;
	int irq_id;
};

#endif

