/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
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
 * @core:       core index of vpu device
 * @name:       the name of algo
 * @ralgo:      return the created algo
 * @needload: need load algo tp dsp or not
 */
int vpu_create_algo(int core, char *name, struct vpu_algo **ralgo,
				bool needload);

#endif
