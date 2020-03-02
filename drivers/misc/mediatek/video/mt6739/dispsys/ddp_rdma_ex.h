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

#ifndef _DDP_RDMA_EX_H_
#define _DDP_RDMA_EX_H_
#include "ddp_info.h"

#define RDMA_INSTANCES  2
#define RDMA_MAX_WIDTH	4095
#define RDMA_MAX_HEIGHT	4095

enum RDMA_OUTPUT_FORMAT {
	RDMA_OUTPUT_FORMAT_ARGB = 0,
	RDMA_OUTPUT_FORMAT_YUV444 = 1,
};

enum RDMA_MODE {
	RDMA_MODE_DIRECT_LINK = 0,
	RDMA_MODE_MEMORY = 1,
};

struct rdma_color_matrix {
	UINT32 C00;
	UINT32 C01;
	UINT32 C02;
	UINT32 C10;
	UINT32 C11;
	UINT32 C12;
	UINT32 C20;
	UINT32 C21;
	UINT32 C22;
};

struct rdma_color_pre {
	UINT32 ADD0;
	UINT32 ADD1;
	UINT32 ADD2;
};

struct rdma_color_post {
	UINT32 ADD0;
	UINT32 ADD1;
	UINT32 ADD2;
};

int rdma_clock_on(enum DISP_MODULE_ENUM module, void *handle);
int rdma_clock_off(enum DISP_MODULE_ENUM module, void *handle);

void rdma_dump_golden_setting_context(enum DISP_MODULE_ENUM module);

void rdma_enable_color_transform(enum DISP_MODULE_ENUM module);
void rdma_disable_color_transform(enum DISP_MODULE_ENUM module);
void rdma_set_color_matrix(enum DISP_MODULE_ENUM module,
			   struct rdma_color_matrix *matrix,
			   struct rdma_color_pre *pre, struct rdma_color_post *post);
int rdma_reset_by_cmdq(enum DISP_MODULE_ENUM module, void *handle);

#endif
