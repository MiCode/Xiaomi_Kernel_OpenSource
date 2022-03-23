/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef _CCU_PLATFORM_DEF_
#define _CCU_PLATFORM_DEF_

/*For user/kernel space sync.*/
#define CCU_HW_OFFSET  (0x0)
#define CCU_PMEM_BASE  (0x1A0A0000)
#define CCU_PMEM_SIZE  (0x10000)
#define CCU_DMEM_BASE  (0x1A080000)
#define CCU_DMEM_SIZE  (0x8000)
#define CCU_DMEM_OFFSET  (0x1C00)
#define CCU_LOG_BASE  (0x500)
#define CCU_LOG_SIZE  (0x400)
#define CCU_HW_DUMP_SIZE  (0x550)
#define CCU_CAMSYS_BASE  (0x1A000000)
#define CCU_CAMSYS_SIZE  (0x1000)
#define CCU_N3D_A_BASE  (0x1A040000)
#define CCU_N3D_A_SIZE  (0x1000)
#define CCU_SENSOR_PM_SIZE  (0x1000)
#define CCU_SENSOR_DM_SIZE  (0x600)

#define CCU_ISR_LOG_BASE	(0x1A00)
#define CCU_ISR_LOG_SIZE	(0x200)

#define CCU_HEADER_NUM (20)
#endif
