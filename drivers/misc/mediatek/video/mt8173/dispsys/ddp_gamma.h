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

/*#include <asm/uaccess.h>*/


typedef enum {
	DISP_GAMMA0 = 0,
	DISP_GAMMA1,
	DISP_GAMMA_TOTAL
} disp_gamma_id_t;


typedef unsigned int gamma_entry;
#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

#define DISP_GAMMA_LUT_SIZE 512

typedef struct {
	disp_gamma_id_t hw_id;
	gamma_entry lut[DISP_GAMMA_LUT_SIZE];
} DISP_GAMMA_LUT_T;


typedef enum {
	DISP_CCORR0 = 0,
	DISP_CCORR1,
	DISP_CCORR_TOTAL
} disp_ccorr_id_t;

typedef struct {
	disp_ccorr_id_t hw_id;
	unsigned int coef[3][3];
} DISP_CCORR_COEF_T;


void disp_gamma_init(disp_gamma_id_t id, unsigned int width, unsigned int height, void *cmdq);

#endif
