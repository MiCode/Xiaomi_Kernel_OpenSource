/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __DDP_GAMMA_H__
#define __DDP_GAMMA_H__

#include <linux/uaccess.h>

enum disp_gamma_id_t {
	DISP_GAMMA0 = 0,
	DISP_GAMMA1,
	DISP_GAMMA_TOTAL
};

#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

#define DISP_GAMMA_LUT_SIZE 512

struct DISP_GAMMA_LUT_T {
	enum disp_gamma_id_t hw_id;
	unsigned int lut[DISP_GAMMA_LUT_SIZE];
};

enum disp_ccorr_id_t {
	DISP_CCORR0 = 0,
	DISP_CCORR1,
	DISP_CCORR_TOTAL
};

struct DISP_CCORR_COEF_T {
	enum disp_ccorr_id_t hw_id;
	unsigned int coef[3][3];
};

extern int corr_dbg_en;

void ccorr_test(const char *cmd, char *debug_output);
int ccorr_interface_for_color(unsigned int ccorr_idx,
	unsigned int ccorr_coef[3][3], void *handle);
void disp_ccorr_on_end_of_frame(void);
void disp_pq_notify_backlight_changed(int bl_1024);
int disp_ccorr_set_color_matrix(void *cmdq,
	int32_t matrix[16], int32_t hint);

#endif

