/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef VDEC_VP9_VPU_H_
#define VDEC_VP9_VPU_H_

#include <linux/wait.h>

int vp9_dec_vpu_init(void *vdec_inst, unsigned int *data,
		     unsigned int items);
int vp9_dec_vpu_start(void *vdec_inst);
int vp9_dec_vpu_end(void *vdec_inst);
int vp9_dec_vpu_reset(void *vdec_inst);
int vp9_dec_vpu_deinit(void *vdec_inst);

#endif /* #ifndef VDEC_DRV_VP9_VPU_H_ */

