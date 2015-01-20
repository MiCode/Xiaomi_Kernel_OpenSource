/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __IA_CSS_DPC2_TYPES_H
#define __IA_CSS_DPC2_TYPES_H

#include "type_support.h"
#define METRIC1_ONE_FP	(1<<12)
#define METRIC2_ONE_FP	(1<<5)
#define METRIC3_ONE_FP	(1<<12)
#define WBGAIN_ONE_FP	(1<<9)


struct ia_css_dpc2_config {
	int32_t metric1;
	int32_t metric2;
	int32_t metric3;
	int32_t wb_gain_gr;
	int32_t wb_gain_r;
	int32_t wb_gain_b;
	int32_t wb_gain_gb;
};

#endif /* __IA_CSS_DPC2_TYPES_H */

