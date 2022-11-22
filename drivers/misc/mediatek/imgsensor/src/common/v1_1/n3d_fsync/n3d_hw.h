/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __N3D_HW_H__
#define __N3D_HW_H__

#include <linux/mutex.h>

#include "kd_seninf_n3d.h"

enum {
	SENINF_N3D_A,
	SENINF_N3D_B,
	SENINF_N3D_MAX_NUM
};

#define SENINF_N3D_NAMES \
	"seninf_n3d_a", \
	"seninf_n3d_b", \

struct base_reg {
	void __iomem *pseninf_top_base;
	void __iomem *pseninf_n3d_base[SENINF_N3D_MAX_NUM];

	struct mutex reg_mutex;
};

int set_n3d_source(struct base_reg *regs,
		   struct sensor_info *sen1,
		   struct sensor_info *sen2);
int disable_n3d(struct base_reg *regs);
int read_status(struct base_reg *regs);

#endif

