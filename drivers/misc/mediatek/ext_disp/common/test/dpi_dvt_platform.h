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

#define HW_MUTEX 1
#define HW_MUTEX_FOR_UPLINK 0
#define M4U_RDMA_PORT_SHORT M4U_PORT_DISP_RDMA1

/* #define RDMA_MODE_DIRECT_LINK 0 */
/* #define RDMA_MODE_MEMORY	  1 */

/* K2 */
/* RDMA1 -> DPI0 */
/* OVL1 -> RDMA1 -> DPI0 */
/* OVL0 -> COLOR0 -> CCORR0 -> ALL0 -> GAMMA0 -> DITHER0 -> RDMA0 -> DPI0 */
#define OVL1_MOD_FOR_MUTEX 11
#define RDMA1_MOD_FOR_MUTEX 14
#define DISP_DSC_MOD_FOR_MUTEX 25
#define RDMA_MOD_FOR_MUTEX_SHORT RDMA1_MOD_FOR_MUTEX

/* DPI Related */
/*
#ifndef DISP_REG_CONFIG_DPI_SEL_IN
#define DISP_REG_CONFIG_DPI_SEL_IN DISP_REG_CONFIG_DPI0_SEL_IN
#endif
*/

/* RDMA Related */
#define DISP_MODULE_RDMA_SHORT DISP_MODULE_RDMA1
#define DISP_MODULE_RDMA_LONG0  DISP_MODULE_RDMA0
#define DISP_MODULE_RDMA_LONG1  DISP_MODULE_RDMA1

int dvt_connect_path(void *handle);
int dvt_disconnect_path(void *handle);
int dvt_acquire_mutex(void);
int dvt_ddp_path_top_clock_on(void);
int dvt_ddp_path_top_clock_off(void);

int dvt_mutex_set(DPI_U32 mutex_id, void *handle);
DPI_I32 dvt_mutex_enable(void *handle, DPI_U32 hwmutex_id);
DPI_I32 dvt_mutex_disenable(void *handle, DPI_U32 hwmutex_id);
DPI_I32 dvt_mutex_reset(void *handle, DPI_U32 hwmutex_id);
void  dvt_mutex_dump_reg(void);

void  dvt_mutex_dump_reg(void);

int dvt_connect_ovl0_dpi(void *handle);
int dvt_disconnect_ovl0_dpi(void *handle);
int dvt_mutex_set_ovl0_dpi(DPI_U32 mutex_id, void *handle);

int dvt_connect_ovl1_dpi(void *handle);
int dvt_disconnect_ovl1_dpi(void *handle);
int dvt_mutex_set_ovl1_dpi(DPI_U32 mutex_id, void *handle);
/*
void _test_cmdq_build_trigger_loop(void);
void _test_cmdq_change_buffer(unsigned int addr);
void _test_cmdq_get_checkSum();
void _test_cmdq_for_interlace(unsigned int lodd_address, unsigned int leven_address);
void _test_cmdq_3D_and_interlace(unsigned int lodd_address, unsigned int leven_address,
	unsigned int rodd_address, unsigned int reven_address);
*/

#endif    /*#if defined(RDMA_DPI_PATH_SUPPORT) || defined(DPI_DVT_TEST_SUPPORT)*/
#endif
