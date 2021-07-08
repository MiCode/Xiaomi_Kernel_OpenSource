/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_HW_MGT_H__
#define __APUSYS_REVISER_HW_MGT_H__
#include <linux/types.h>

void *reviser_hw_mgt_get_cb(void);

void reviser_mgt_dmp_boundary(void *drvinfo, void *s_file);
void reviser_mgt_dmp_ctx(void *drvinfo, void *s_file);
void reviser_mgt_dmp_rmp(void *drvinfo, void *s_file);
void reviser_mgt_dmp_default(void *drvinfo, void *s_file);
void reviser_mgt_dmp_exception(void *drvinfo, void *s_file);

int reviser_mgt_set_boundary(void *drvinfo, uint8_t boundary);
int reviser_mgt_set_default(void *drvinfo);
int reviser_mgt_set_ctx(void *drvinfo, int type, int index, uint8_t ctx);
int reviser_mgt_set_rmp(void *drvinfo, int index, uint8_t valid, uint8_t ctx,
		uint8_t src_page, uint8_t dst_page);

int reviser_mgt_isr_cb(void *drvinfo);
int reviser_mgt_set_int(void *drvinfo, uint8_t enable);
#endif
