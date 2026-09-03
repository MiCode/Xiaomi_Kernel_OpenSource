/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _DISP_LIB_H_
#define _DISP_LIB_H_

#include <linux/list.h>
#include <drm/drm_print.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(__fmt) "[drm][%20s] "__fmt, __func__
#endif

int str_to_u32_array(const char *p, u32 base, u32 array[], u8 size);
int str_to_u8_array(const char *p, u32 base, u8 array[], u8 size);

#ifdef CONFIG_DRM_SPRD_WB_DEBUG
int dump_bmp32(const char *p, u32 width, u32 height,
		bool bgra, const char *filename);
#endif

struct device *sprd_disp_pipe_get_by_port(struct device *dev, int port);
struct device *sprd_disp_pipe_get_input(struct device *dev);
struct device *sprd_disp_pipe_get_output(struct device *dev);

#endif /* _DISP_LIB_H_ */
