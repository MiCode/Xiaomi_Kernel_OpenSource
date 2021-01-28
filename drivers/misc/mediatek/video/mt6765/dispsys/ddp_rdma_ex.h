/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DDP_RDMA_EX_H_
#define _DDP_RDMA_EX_H_
#include "ddp_info.h"

#define RDMA_INSTANCES  2
#define RDMA_MAX_WIDTH  4095
#define RDMA_MAX_HEIGHT 4095

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

//90hz

/* golden setting */
enum GS_RDMA_FLD {
	GS_RDMA_PRE_ULTRA_TH_LOW = 0,
	GS_RDMA_PRE_ULTRA_TH_HIGH,
	GS_RDMA_VALID_TH_FORCE_PRE_ULTRA,
	GS_RDMA_VDE_FORCE_PRE_ULTRA,
	GS_RDMA_ULTRA_TH_LOW,
	GS_RDMA_ULTRA_TH_HIGH,
	GS_RDMA_VALID_TH_BLOCK_ULTRA,
	GS_RDMA_VDE_BLOCK_ULTRA,
	GS_RDMA_ISSUE_REQ_TH,
	GS_RDMA_OUTPUT_VALID_FIFO_TH,
	GS_RDMA_FIFO_SIZE,
	GS_RDMA_FIFO_UNDERFLOW_EN,
	GS_RDMA_TH_LOW_FOR_SODI,
	GS_RDMA_TH_HIGH_FOR_SODI,
	GS_RDMA_TH_LOW_FOR_DVFS,
	GS_RDMA_TH_HIGH_FOR_DVFS,
	GS_RDMA_SRAM_SEL,
	GS_RDMA_DVFS_PRE_ULTRA_TH_LOW,
	GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH,
	GS_RDMA_DVFS_ULTRA_TH_LOW,
	GS_RDMA_DVFS_ULTRA_TH_HIGH,
	GS_RDMA_IS_DRS_STATUS_TH_LOW,
	GS_RDMA_IS_DRS_STATUS_TH_HIGH,
	GS_RDMA_NOT_DRS_STATUS_TH_LOW,
	GS_RDMA_NOT_DRS_STATUS_TH_HIGH,
	GS_RDMA_URGENT_TH_LOW,
	GS_RDMA_URGENT_TH_HIGH,
	GS_RDMA_SELF_FIFO_SIZE,
	GS_RDMA_RSZ_FIFO_SIZE,
	GS_RDMA_FLD_NUM,
};
int rdma_clock_on(enum DISP_MODULE_ENUM module, void *handle);
int rdma_clock_off(enum DISP_MODULE_ENUM module, void *handle);

void rdma_dump_golden_setting_context(enum DISP_MODULE_ENUM module);

void rdma_enable_color_transform(enum DISP_MODULE_ENUM module);
void rdma_disable_color_transform(enum DISP_MODULE_ENUM module);
void rdma_set_color_matrix(enum DISP_MODULE_ENUM module,
	struct rdma_color_matrix *matrix, struct rdma_color_pre *pre,
	struct rdma_color_post *post);
int rdma_reset_by_cmdq(enum DISP_MODULE_ENUM module, void *handle);

#endif
