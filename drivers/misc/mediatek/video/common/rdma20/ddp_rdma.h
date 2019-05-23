/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef _DDP_RDMA_H_
#define _DDP_RDMA_H_

/* #include <mt-plat/sync_write.h> */
#include <linux/types.h>
/* #include <mach/mt_reg_base.h> */
#include "ddp_info.h"
#include "ddp_hal.h"

extern unsigned long long rdma_start_time[];
extern unsigned long long rdma_end_time[];
extern unsigned int rdma_start_irq_cnt[];
extern unsigned int rdma_done_irq_cnt[];
extern unsigned int rdma_underflow_irq_cnt[];
extern unsigned int rdma_targetline_irq_cnt[];

/* init module */
int rdma_init(enum DISP_MODULE_ENUM module, void *handle);

/* deinit module */
int rdma_deinit(enum DISP_MODULE_ENUM module, void *handle);

/* start module */
int rdma_start(enum DISP_MODULE_ENUM module, void *handle);

/* stop module */
int rdma_stop(enum DISP_MODULE_ENUM module, void *handle);

/* reset module */
int rdma_reset(enum DISP_MODULE_ENUM module, void *handle);

/* common interface */
unsigned long rdma_base_addr(enum DISP_MODULE_ENUM module);
unsigned int rdma_index(enum DISP_MODULE_ENUM module);
void rdma_set_target_line(enum DISP_MODULE_ENUM module,
					unsigned int line, void *handle);
void rdma_get_address(enum DISP_MODULE_ENUM module, unsigned long *data);
void rdma_dump_reg(enum DISP_MODULE_ENUM module);
void rdma_dump_analysis(enum DISP_MODULE_ENUM module);
void rdma_get_info(int idx, struct RDMA_BASIC_STRUCT *info);

#endif
