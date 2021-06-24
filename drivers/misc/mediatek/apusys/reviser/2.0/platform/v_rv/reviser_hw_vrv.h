/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_HW_H__
#define __APUSYS_REVISER_HW_H__
#include <linux/types.h>



int reviser_isr(void *drvinfo);

void reviser_print_rvr_exception(void *drvinfo, void *s_file);

void reviser_print_rvr_boundary(void *drvinfo, void *s_file);
void reviser_print_rvr_context_ID(void *drvinfo, void *s_file);
void reviser_print_rvr_remap_table(void *drvinfo, void *s_file);
void reviser_print_rvr_default_iova(void *drvinfo, void *s_file);
#endif
