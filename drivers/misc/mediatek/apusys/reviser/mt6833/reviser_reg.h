// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _REVISER_REG_H_
#define _REVISER_REG_H_
#include <linux/types.h>


#define VLM_BASE                 (0x1D800000)
#define TCM_BASE                 (0x1D000000)
#define TCM_SIZE                 (0x000000)
#define VLM_SIZE                 (0x200000)
#define VLM_BANK_SIZE            (0x40000)
#define REMAP_DRAM_SIZE          (0x4000000)
#define REMAP_DRAM_BASE          (0x4000000)
#define VLM_DRAM_BANK_MAX        (8)
#define VLM_TCM_BANK_MAX         (0)
#define VLM_CTXT_DRAM_OFFSET     (0x200000)

#define REVISER_BASE             (0x19021000)
#define REVISER_INT_BASE         (0x19001000)
//Need to check boundary region with iommu team every project
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT == 34)
#define BOUNDARY_APUSYS          (3)
#define BOUNDARY_MASK            (0x300000000ULL)
#else
#define BOUNDARY_APUSYS          (0)
#define BOUNDARY_MASK            (0x000000000ULL)
#endif

#define REVISER_FAIL             (0xFFFFFFFF)
#define REVISER_DEFAULT          (0xFFFFFFFF)

#define REVISER_INT_EN           (0x80)
#define APUSYS_EXCEPT_INT        (0x34)
#define REVISER_INT_EN_MASK      (0xFE000)

#define VP6_CORE0_BASE_0         (0x0100)
#define VP6_CORE0_BASE_1         (0x0108)

#define VLM_REMAP_TABLE_BASE     (0x0200)
#define VLM_DEFAULT_MVA          (VLM_REMAP_TABLE_BASE + 0x00)
#define VLM_REMAP_TABLE_0        (VLM_REMAP_TABLE_BASE + 0x04)
#define VLM_REMAP_TABLE_1        (VLM_REMAP_TABLE_BASE + 0x08)
#define VLM_REMAP_TABLE_2        (VLM_REMAP_TABLE_BASE + 0x0C)
#define VLM_REMAP_TABLE_3        (VLM_REMAP_TABLE_BASE + 0x10)
#define VLM_REMAP_TABLE_4        (VLM_REMAP_TABLE_BASE + 0x14)
#define VLM_REMAP_TABLE_5        (VLM_REMAP_TABLE_BASE + 0x18)
#define VLM_REMAP_TABLE_6        (VLM_REMAP_TABLE_BASE + 0x1C)
#define VLM_REMAP_TABLE_7        (VLM_REMAP_TABLE_BASE + 0x20)
#define VLM_REMAP_TABLE_8        (VLM_REMAP_TABLE_BASE + 0x24)
#define VLM_REMAP_TABLE_9        (VLM_REMAP_TABLE_BASE + 0x28)
#define VLM_REMAP_TABLE_A        (VLM_REMAP_TABLE_BASE + 0x2C)
#define VLM_REMAP_TABLE_B        (VLM_REMAP_TABLE_BASE + 0x30)
#define VLM_REMAP_TABLE_C        (VLM_REMAP_TABLE_BASE + 0x34)


#define VLM_CTXT_BASE            (0x0300)

#define VLM_CTXT_MDLA_MAX        (0)
#define VLM_CTXT_MDLA_0          (VLM_CTXT_BASE + 0x08)
#define VLM_CTXT_MDLA_1          (VLM_CTXT_BASE + 0x0C)

#define VLM_CTXT_VPU_MAX         (2)
#define VLM_CTXT_VPU_0           (VLM_CTXT_BASE + 0x10)
#define VLM_CTXT_VPU_1           (VLM_CTXT_BASE + 0x14)
#define VLM_CTXT_VPU_2           (VLM_CTXT_BASE + 0x18)

#define VLM_CTXT_EDMA_MAX        (0)
#define VLM_CTXT_EDMA_0          (VLM_CTXT_BASE + 0x1C)
#define VLM_CTXT_EDMA_1          (VLM_CTXT_BASE + 0x20)

#define VLM_CTXT_UP_MAX          (1)

#define AXI_EXCEPTION_MD32       (0x0400)
#define AXI_EXCEPTION_MDLA_0     (0x0408)
#define AXI_EXCEPTION_MDLA_1     (0x040C)
#define AXI_EXCEPTION_VPU_0      (0x0410)
#define AXI_EXCEPTION_VPU_1      (0x0414)
#define AXI_EXCEPTION_VPU_2      (0x0418)
#define AXI_EXCEPTION_EDMA_0     (0x041C)
#define AXI_EXCEPTION_EDMA_1     (0x0420)

#define VLM_REMAP_VALID    (0x80000000)
#define VLM_REMAP_VALID_OFFSET   (31)
#define VLM_REMAP_CTX_ID   (0x03E00000)
#define VLM_REMAP_CTX_ID_OFFSET    (21)
#define VLM_REMAP_CTX_SRC   (0x001C0000)
#define VLM_REMAP_CTX_SRC_OFFSET    (18)
#define VLM_REMAP_CTX_DST   (0x0003C000)
#define VLM_REMAP_CTX_DST_OFFSET    (14)

#define VLM_CTXT_BDY_SELECT      (0x00000003)
#define VLM_CTXT_BDY_SELECT_MAX  (3)

#define VLM_CTXT_CTX_ID          (0x03E00000)
#define VLM_CTXT_CTX_ID_OFFSET   (21)
#define VLM_CTXT_CTX_ID_MAX      (32)
#define VLM_CTXT_CTX_ID_COUNT (VLM_CTXT_MDLA_MAX + \
							VLM_CTXT_VPU_MAX + \
							VLM_CTXT_EDMA_MAX + \
							VLM_CTXT_UP_MAX)

#define VLM_REMAP_TABLE_SRC_MAX  (7)
#define VLM_REMAP_TABLE_DST_MAX  (0xC)
#define VLM_REMAP_TABLE_MAX  (0xD)

uint32_t  reviser_get_remap_offset(uint32_t index);
uint32_t  reviser_get_contex_offset_MDLA(uint32_t index);
uint32_t  reviser_get_contex_offset_VPU(uint32_t index);
uint32_t  reviser_get_contex_offset_EDMA(uint32_t index);


#endif
