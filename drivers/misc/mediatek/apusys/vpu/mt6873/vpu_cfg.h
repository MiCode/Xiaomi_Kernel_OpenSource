/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __VPU_CFG_H__
#define __VPU_CFG_H__

#include <linux/types.h>

struct vpu_device;

static inline
void vpu_emi_mpu_set(unsigned long start, unsigned int size)
{
}

bool vpu_is_disabled(struct vpu_device *vd);

/* 20180703, 00:00: vpu log mechanism */
#define HOST_VERSION	(0x18070300)

/* iommu */
#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
#define VPU_IOVA_BANK (0x300000000ULL)
#else
#define VPU_IOVA_BANK (0x0ULL)
#endif

#define VPU_IOVA_START (0x7DA00000)
#define VPU_IOVA_END   (0x82600000)

/* Firmware Binary  */
#define VPU_MAX_NUM_CODE_SEGMENTS       (50)
#define VPU_MAX_NUM_ALGOS               (50)
#define VPU_SIZE_BINARY_CODE            (0x02A10000)
#define VPU_NUMS_IMAGE_HEADER           (3)
#define VPU_OFFSET_ALGO_AREA            (0x00C00000)
#define VPU_OFFSET_MAIN_PROGRAM_IMEM    (VPU_SIZE_BINARY_CODE - 0xC0000)
#define VPU_OFFSET_IMAGE_HEADERS        (VPU_SIZE_BINARY_CODE - 0x30000)

/* Command */
#define VPU_CMD_SIZE  (0x2000)

/* Time Constrains */
#define VPU_CMD_TIMEOUT  (9000)
#define VPU_PWR_OFF_LATENCY (3000)
#define WAIT_CMD_LATENCY_US (2000)
#define WAIT_CMD_RETRY  (5)
#define WAIT_XOS_LATENCY_US (500)
#define WAIT_XOS_RETRY  (10)

/* Remote Proc */
#define VPU_REMOTE_PROC (0)

/* Image Layout */
#define VPU_IMG_LEGACY  (0)
#define VPU_IMG_PRELOAD (1)

/* efuse register related define */
#define EFUSE_VPU_OFFSET	5
#define EFUSE_VPU_MASK		0x7
#define EFUSE_VPU_SHIFT		16
#define EFUSE_SEG_OFFSET	30

/* mpu protect region definition */
#define MPU_PROCT_REGION	21
#define MPU_PROCT_D0_AP		0
#define MPU_PROCT_D5_APUSYS	5

/* VPU XOS */
#define VPU_XOS (1)  /* XOS: 1, non-XOS: 0 */
#define XOS_TIMEOUT_US (1000000)

#if VPU_XOS
#define VPU_MAX_PRIORITY (3)
#else
#define VPU_MAX_PRIORITY (1)
#endif

/* Work Buffer */
#define VPU_LOG_OFFSET         (0)
#define VPU_LOG_HEADER_SIZE    (0x10)

/* Dump: Sizes */
#define VPU_DMP_RESET_SZ   0x400U
#define VPU_DMP_MAIN_SZ    0x40000U  // 256 KB
#define VPU_DMP_KERNEL_SZ  0x20000U  // 128 KB
#define VPU_DMP_PRELOAD_SZ 0x20000U  // 128 KB
#define VPU_DMP_IRAM_SZ    0x10000U  // 64 KB
#define VPU_DMP_WORK_SZ    0x2000U   // 8 KB
#define VPU_DMP_REG_SZ     0x1000U   // 4 KB
#define VPU_DMP_IMEM_SZ    0x30000U  // 192 KB
#define VPU_DMP_DMEM_SZ    0x40000U  // 256 KB

/* Dump: Registers */
#define VPU_DMP_REG_CNT_INFO 32
#define VPU_DMP_REG_CNT_DBG 8
#define VPU_DMP_REG_CNT_MBOX 32

/* Dump: Information String Size */
#define VPU_DMP_INFO_SZ 128

/* Tags: Count of Tags */
#define VPU_TAGS_CNT (3000)

/* Register Defines */
#define MBOX_INBOX_0	0x000
#define MBOX_INBOX_1	0x004
#define MBOX_INBOX_2	0x008
#define MBOX_INBOX_3	0x00C
#define MBOX_INBOX_4	0x010
#define MBOX_INBOX_5	0x014
#define MBOX_INBOX_6	0x018
#define MBOX_INBOX_7	0x01C
#define MBOX_INBOX_8	0x020
#define MBOX_INBOX_9	0x024
#define MBOX_INBOX_10	0x028
#define MBOX_INBOX_11	0x02C
#define MBOX_INBOX_12	0x030
#define MBOX_INBOX_13	0x034
#define MBOX_INBOX_14	0x038
#define MBOX_INBOX_15	0x03C
#define MBOX_INBOX_16	0x040
#define MBOX_INBOX_17	0x044
#define MBOX_INBOX_18	0x048
#define MBOX_INBOX_19	0x04C
#define MBOX_INBOX_20	0x050
#define MBOX_INBOX_21	0x054
#define MBOX_INBOX_22	0x058
#define MBOX_INBOX_23	0x05C
#define MBOX_INBOX_24	0x060
#define MBOX_INBOX_25	0x064
#define MBOX_INBOX_26	0x068
#define MBOX_INBOX_27	0x06C
#define MBOX_INBOX_28	0x070
#define MBOX_INBOX_29	0x074
#define MBOX_INBOX_30	0x078
#define MBOX_INBOX_31	0x07C
#define MBOX_DUMMY_0	0x080
#define MBOX_DUMMY_1	0x084
#define MBOX_DUMMY_2	0x088
#define MBOX_DUMMY_3	0x08C
#define MBOX_DUMMY_4	0x090
#define MBOX_DUMMY_5	0x094
#define MBOX_DUMMY_6	0x098
#define MBOX_DUMMY_7	0x09C
#define MBOX_INBOX_IRQ  0x0A0
#define MBOX_INBOX_MASK	0x0A4
#define MBOX_INBOX_PRI_MASK	0x0A8
#define CG_CON		0x100
#define CG_CLR		0x108
#define SW_RST		0x10C
#define DONE_ST		0x90C
#define CTRL		0x910
#define XTENSA_INT	0x200
#define CTL_XTENSA_INT	0x204
#define DEFAULT0	0x93C
#define DEFAULT1	0x940
#define DEFAULT2	0x944
#define XTENSA_INFO00	0x250
#define XTENSA_INFO01	0x254
#define XTENSA_INFO02	0x258
#define XTENSA_INFO03	0x25C
#define XTENSA_INFO04	0x260
#define XTENSA_INFO05	0x264
#define XTENSA_INFO06	0x268
#define XTENSA_INFO07	0x26C
#define XTENSA_INFO08	0x270
#define XTENSA_INFO09	0x274
#define XTENSA_INFO10	0x278
#define XTENSA_INFO11	0x27C
#define XTENSA_INFO12	0x280
#define XTENSA_INFO13	0x284
#define XTENSA_INFO14	0x288
#define XTENSA_INFO15	0x28C
#define XTENSA_INFO16	0x290
#define XTENSA_INFO17	0x294
#define XTENSA_INFO18	0x298
#define XTENSA_INFO19	0x29C
#define XTENSA_INFO20	0x2A0
#define XTENSA_INFO21	0x2A4
#define XTENSA_INFO22	0x2A8
#define XTENSA_INFO23	0x2AC
#define XTENSA_INFO24	0x2B0
#define XTENSA_INFO25	0x2B4
#define XTENSA_INFO26	0x2B8
#define XTENSA_INFO27	0x2BC
#define XTENSA_INFO28	0x2C0
#define XTENSA_INFO29	0x2C4
#define XTENSA_INFO30	0x2C8
#define XTENSA_INFO31	0x2CC
#define DEBUG_INFO00	0x2D0
#define DEBUG_INFO01	0x2D4
#define DEBUG_INFO02	0x2D8
#define DEBUG_INFO03	0x2DC
#define DEBUG_INFO04	0x2E0
#define DEBUG_INFO05	0x2E4
#define DEBUG_INFO06	0x2E8
#define DEBUG_INFO07	0x2EC
#define XTENSA_ALTRESETVEC	0x2F8

/* Register Config: CTRL */
#define P_DEBUG_ENABLE (1 << 31)
#define STATE_VECTOR_SELECT (1 << 19)
#define PBCLK_ENABLE (1 << 26)
#define PRID (0x1FFFE)
#define PIF_GATED (1 << 17)

/* Register Config: SW_RST */
#define APU_D_RST (1 << 8)
#define APU_B_RST (1 << 4)
#define OCDHALTONRESET (1 << 12)

/* Register Config: DEFAULT0 */
#define ARUSER (0x2 << 23)
#define AWUSER (0x2 << 18)
#define QOS_SWAP (1 << 28)

/* Register Config: DEFAULT1 */
#define ARUSER_IDMA (0x2 << 0)
#define AWUSER_IDMA (0x2 << 5)

/* Register Config: DEFAULT2 */
#define DBG_EN (0xf)

/* Register Config: CG_CLR */
#define JTAG_CG_CLR (0x2)

/* Register Mask: DONE_ST */
#define PWAITMODE (1 << 7)

#endif

