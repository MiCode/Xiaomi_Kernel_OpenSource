/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_REVISER_HW_H__
#define __APUSYS_REVISER_HW_H__
#include <linux/types.h>


enum REVISER_DEVICE_E {
	REVISER_DEVICE_NONE,
	REVISER_DEVICE_SECURE_MD32,
	REVISER_DEVICE_NORMAL_MD32,
	REVISER_DEVICE_MDLA,
	REVISER_DEVICE_VPU,
	REVISER_DEVICE_EDMA,
	REVISER_DEVICE_MAX,
};



void reviser_print_private(void *private);
void reviser_print_boundary(void *private);
void reviser_print_context_ID(void *private);
void reviser_print_remap_table(void *private);
void reviser_set_context_boundary(void *private,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary);
void reviser_set_context_ID(void *private,
		enum REVISER_DEVICE_E type, int index, uint8_t ID);
void reviser_set_remap_talbe(void *private,
		int index, uint8_t valid, uint8_t ID, uint8_t src_page,
		uint8_t dst_page);
#endif
