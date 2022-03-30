/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_HW_H__
#define __APUSYS_REVISER_HW_H__
#include <linux/types.h>



int reviser_isr(void *drvinfo);

void reviser_print_exception(void *drvinfo, void *s_file);

void reviser_print_boundary(void *drvinfo, void *s_file);
void reviser_print_context_ID(void *drvinfo, void *s_file);
void reviser_print_remap_table(void *drvinfo, void *s_file);
void reviser_print_default_iova(void *drvinfo, void *s_file);
int reviser_set_boundary(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary);
int reviser_set_context_ID(void *drvinfo, int type, int index, uint8_t ctx);
int reviser_set_remap_table(void *drvinfo,
		int index, uint8_t valid, uint8_t ID, uint8_t src_page,
		uint8_t dst_page);

int reviser_set_default_iova(void *drvinfo);
int reviser_get_interrupt_offset(void *drvinfo);
int reviser_type_convert(int type, enum REVISER_DEVICE_E *reviser_type);

int reviser_boundary_init(void *drvinfo, uint8_t boundary);
int reviser_enable_interrupt(void *drvinfo, uint8_t enable);

int reviser_check_int_valid(void *drvinfo);
#endif
