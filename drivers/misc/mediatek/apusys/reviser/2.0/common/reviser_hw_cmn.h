/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_HW_CMN_H__
#define __APUSYS_REVISER_HW_CMN_H__
#include <linux/types.h>



struct reviser_hw_ops {
	int (*set_boundary)(void *drvinfo, uint8_t boundary);
	int (*set_default)(void *drvinfo);
	int (*set_ctx)(void *drvinfo, int type, int index, uint8_t ctx);
	int (*set_rmp)(void *drvinfo, int index, uint8_t valid, uint8_t ctx,
			uint8_t src_page, uint8_t dst_page);


	void (*dmp_boundary)(void *drvinfo, void *s_file);
	void (*dmp_default)(void *drvinfo, void *s_file);
	void (*dmp_ctx)(void *drvinfo, void *s_file);
	void (*dmp_rmp)(void *drvinfo, void *s_file);
	void (*dmp_exception)(void *drvinfo, void *s_file);


	int (*isr_cb)(void *drvinfo);
	int (*set_int)(void *drvinfo, uint8_t enable);
};

#endif
