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

#include "vpu_drv.h"
#include "vpu_cmn.h"
#include <linux/slab.h>

enum {
	VPU_PROP_RESERVED,
	VPU_NUM_PROPS
};

extern const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES];

int vpu_init_algo(void);

/**
 * vpu_create_algo - load algo binary to retrieve info, and allocate struct algo
 * @core:       core index of vpu device
 * @name:       the name of algo
 * @ralgo:      return the created algo
 * @needload: need load algo tp dsp or not
 */

/* vpu_algo.c */
struct __vpu_algo *vpu_alg_alloc(struct vpu_device *vd);
static inline void vpu_alg_free(struct __vpu_algo *alg) { kfree(alg); }
int vpu_alg_load(struct vpu_device *vd, const char *name,
	struct __vpu_algo *alg);
void vpu_alg_unload(struct vpu_device *vd);
struct __vpu_algo *vpu_alg_get(struct vpu_device *vd, const char *name,
	struct __vpu_algo *alg);
void vpu_alg_put(struct __vpu_algo *alg);

/* vpu_hw.c */
int vpu_init_dev_algo(struct platform_device *pdev, struct vpu_device *vd);
void vpu_exit_dev_algo(struct platform_device *pdev, struct vpu_device *vd);
int vpu_hw_alg_init(struct vpu_device *vd, struct __vpu_algo *alg);
int vpu_hw_alg_info(struct vpu_device *vd, struct __vpu_algo *alg);

/* vpu_algo.c
 * handleing dynamic load/unload algo
 */
int vpu_alg_add(struct vpu_device *vd, struct apusys_firmware_hnd *fw);
int vpu_alg_del(struct vpu_device *vd, struct apusys_firmware_hnd *fw);
int vpu_firmware(struct vpu_device *vd, struct apusys_firmware_hnd *fw);

#endif
