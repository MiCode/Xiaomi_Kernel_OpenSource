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

#ifndef _DPI_DVT_PLATFORM_H_
#define _DPI_DVT_PLATFORM_H_

#include "dpi_dvt_test.h"

#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)

#include "ddp_info.h"
#include "ddp_hal.h"
#include "ddp_reg.h"

#define WHITNEY_DPI_DVT
#define HW_MUTEX 1
#define HW_MUTEX_FOR_UPLINK 0
#define M4U_RDMA_PORT_SHORT M4U_PORT_DISP_RDMA1

/* #define RDMA_MODE_DIRECT_LINK 0 */
/* #define RDMA_MODE_MEMORY	  1 */

/* RDMA1 -> DPI0 */
/* OVL1 -> RDMA1 -> DPI0 */
/* OVL0 -> COLOR0 -> CCORR0 -> ALL0 -> GAMMA0 -> DITHER0 -> RDMA0 -> DPI0 */
#define OVL1_MOD_FOR_MUTEX 14
#define OVL1_2L_MOD_FOR_MUTEX 11
#define RDMA1_MOD_FOR_MUTEX 1
#define RDMA2_MOD_FOR_MUTEX 2
#define OVL0_MOD_FOR_MUTEX 13
#define OVL0_2L_MOD_FOR_MUTEX 15
#define COLOR_MOD_FOR_MUTEX 19
#define COLOR1_MOD_FOR_MUTEX 20
#define CCORR_MOD_FOR_MUTEX 21
#define CCORR1_MOD_FOR_MUTEX 22
#define AAL_MOD_FOR_MUTEX 23
#define AAL1_MOD_FOR_MUTEX 24
#define GAMMA_MOD_FOR_MUTEX 25
#define GAMMA1_MOD_FOR_MUTEX 26
#define DITHER_MOD_FOR_MUTEX 28
#define DITHER1_MOD_FOR_MUTEX 29
#define RDMA0_MOD_FOR_MUTEX 0
#define WDMA1_MOD_FOR_MUTEX 18
#define DISP_DSC_MOD_FOR_MUTEX 31
#define RDMA_MOD_FOR_MUTEX_SHORT RDMA1_MOD_FOR_MUTEX
#define DSI1_MOD_FOR_MUTEX 4

/* RDMA Related */
#define DISP_MODULE_RDMA_SHORT DISP_MODULE_RDMA1
#define DISP_MODULE_RDMA_LONG0  DISP_MODULE_RDMA0
#define DISP_MODULE_RDMA_LONG1  DISP_MODULE_RDMA1
#define LONG_PATH0_MODULE_NUM 10
#define LONG_PATH1_MODULE_NUM 10
#define SHORT_PATH_MODULE_NUM 2
#define WDMA_PATH_MODULE_NUM 3
#define RDMA2_PATH_MODULE_NUM 2
extern unsigned int long_path0_module[LONG_PATH0_MODULE_NUM];
extern unsigned int long_path1_module[LONG_PATH1_MODULE_NUM];
extern unsigned int short_path_module[SHORT_PATH_MODULE_NUM];
extern unsigned int wdma_path_module[WDMA_PATH_MODULE_NUM];
extern unsigned int rdma2_path_module[RDMA2_PATH_MODULE_NUM];
extern unsigned int long_dsi1_path1_module[LONG_PATH1_MODULE_NUM];

int init_cmdq_slots(cmdqBackupSlotHandle *pSlot, int count, int init_val);
int rdma2_dvt_connect_path(void *handle);
int rdma2_dvt_mutex_set(unsigned int mutex_id, void *handle);

int dvt_connect_path(void *handle);
int dvt_disconnect_path(void *handle);
int dvt_acquire_mutex(void);
int dvt_ddp_path_top_clock_on(void);
int dvt_ddp_path_top_clock_off(void);

int dvt_mutex_set(unsigned int mutex_id, void *handle);
int dvt_mutex_enable(void *handle, unsigned int hwmutex_id);
int dvt_mutex_disenable(void *handle, unsigned int hwmutex_id);
int dvt_mutex_reset(void *handle, unsigned int hwmutex_id);
void  dvt_mutex_dump_reg(void);

void  dvt_mutex_dump_reg(void);

int dvt_connect_ovl0_dpi(void *handle);
int dvt_disconnect_ovl0_dpi(void *handle);
int dvt_mutex_set_ovl0_dpi(unsigned int mutex_id, void *handle);

int dvt_connect_ovl1_dpi(void *handle);
int dvt_disconnect_ovl1_dpi(void *handle);
int dvt_mutex_set_ovl1_dpi(unsigned int mutex_id, void *handle);
int dvt_mutex_set_ovl1_wdma(unsigned int mutex_id, void *handle);
int dvt_connect_ovl1_wdma(void *handle);
int dvt_disconnect_ovl1_wdma(void *handle);
int dsi_dvt_connect_path(void *handle);
int dsi_dvt_mutex_set(unsigned int mutex_id, void *handle);
int dsi_dsc_dvt_connect_path(void *handle);
int dsi_dsc_dvt_mutex_set(unsigned int mutex_id, void *handle);
int ovl_dsi_dsc_dvt_connect_path(void *handle);
int ovl_dsi_dsc_dvt_mutex_set(unsigned int mutex_id, void *handle);


#endif
#endif
