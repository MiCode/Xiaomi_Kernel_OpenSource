/*
 * Copyright (C) 2021 MediaTek Inc.
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

