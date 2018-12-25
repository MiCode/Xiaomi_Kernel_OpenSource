/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_ALGO_H__
#define __VPU_ALGO_H__

#include "vpu_cmn.h"

enum {
	VPU_PROP_RESERVED,
	VPU_NUM_PROPS
};

extern const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES];

/**
 * vpu_create_algo - load algo binary to retrieve info, and allocate struct algo
 * @name:       the name of algo
 * @ralgo:      return the created algo
 */
int vpu_create_algo(char *name, struct vpu_algo **ralgo);

#endif
