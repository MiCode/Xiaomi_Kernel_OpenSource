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

#ifndef __TRUSTZONE_TZ_DDP__
#define __TRUSTZONE_TZ_DDP__

#define TZ_TA_DDP_NAME "DDP TA"
#define TZ_TA_DDP_UUID "dd7b73bc-0244-4072-b541-c9a53d8fbf5b"

/* Data Structure for DDP TA
 * You should define data structure used both in REE/TEE here
 */
/* N/A for DDP TA */

/* Command for DDP TA */
#define TZCMD_DDP_OVL_START          0
#define TZCMD_DDP_OVL_STOP           1
#define TZCMD_DDP_OVL_RESET          2
#define TZCMD_DDP_OVL_ROI            3
#define TZCMD_DDP_OVL_LAYER_SWITCH   4
#define TZCMD_DDP_OVL_LAYER_CONFIG   5
#define TZCMD_DDP_OVL_3D_CONFIG      6
#define TZCMD_DDP_OVL_LAYER_TDSHP_EN 7
#define TZCMD_DDP_OVL_TEST           8
#define TZCMD_DDP_OVL_CONFIG_LAYER_ADDR      9
#define TZCMD_DDP_OVL_IS_EN          10

#define TZCMD_DDP_OVL_ALLOC_MVA      11
#define TZCMD_DDP_OVL_DEALLOC_MVA    12

#define TZCMD_DDP_SECURE_MVA_MAP     13
#define TZCMD_DDP_SECURE_MVA_UNMAP   14

#define TZCMD_DDP_INTR_CALLBACK      15
#define TZCMD_DDP_REGISTER_INTR      16

#define TZCMD_DDP_OVL_BACKUP_REG     17
#define TZCMD_DDP_OVL_RESTORE_REG    18

#define TZCMD_DDP_WDMA_BACKUP_REG    19
#define TZCMD_DDP_WDMA_RESTORE_REG   20

#define TZCMD_DDP_DUMP_REG          30
#define TZCMD_DDP_SET_SECURE_MODE   31

#define TZCMD_DDP_SET_DEBUG_LOG      40

#define TZCMD_DDP_SET_DAPC_MODE     50

#define TZCMD_DDP_WDMA_CONFIG       60

#define TZCMD_DDP_RDMA_ADDR_CONFIG  70

#define TZCMD_DDP_RDMA1_ADDR_CONFIG  71

#endif	/* __TRUSTZONE_TZ_DDP__ */
