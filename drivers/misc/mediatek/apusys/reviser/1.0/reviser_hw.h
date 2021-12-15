// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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


void reviser_print_rw(void *drvinfo, void *s_file);
void reviser_print_private(void *drvinfo);
void reviser_print_dram(void *drvinfo, void *s_file);
void reviser_print_tcm(void *drvinfo, void *s_file);
void reviser_print_exception(void *drvinfo, void *s_file);
void reviser_print_error(void *drvinfo, void *s_file);
void reviser_print_boundary(void *drvinfo, void *s_file);
void reviser_print_context_ID(void *drvinfo, void *s_file);
void reviser_print_remap_table(void *drvinfo, void *s_file);
void reviser_print_default_iova(void *drvinfo, void *s_file);
int reviser_set_boundary(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t boundary);
int reviser_set_context_ID(void *drvinfo,
		enum REVISER_DEVICE_E type, int index, uint8_t ID);
int reviser_set_remap_table(void *drvinfo,
		int index, uint8_t valid, uint8_t ID, uint8_t src_page,
		uint8_t dst_page);
int reviser_dram_remap_init(void *drvinfo);
int reviser_dram_remap_destroy(void *drvinfo);
int reviser_set_default_iova(void *drvinfo);
int reviser_get_interrupt_offset(void *drvinfo);
int reviser_type_convert(int type, enum REVISER_DEVICE_E *reviser_type);
bool reviser_is_power(void *drvinfo);
int reviser_boundary_init(void *drvinfo, uint8_t boundary);
void reviser_enable_interrupt(void *drvinfo, uint8_t enable);
int reviser_alloc_tcm(void *drvinfo, void *usr);
int reviser_free_tcm(void *drvinfo, void *usr);
int reviser_power_on(void *drvinfo);
int reviser_power_off(void *drvinfo);
int reviser_check_int_valid(void *drvinfo);
int reviser_init_ip(void);
#endif
