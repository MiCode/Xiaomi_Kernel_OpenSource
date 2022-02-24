// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VPU_CFG_H__
#define __VPU_CFG_H__

#include <linux/types.h>

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
#define VPU_DMP_SZ         0x1000U  // default: 4 KB

/* Dump: Registers */
#define VPU_DMP_REG_CNT_INFO 32
#define VPU_DMP_REG_CNT_DBG 8

/* Dump: Information String Size */
#define VPU_DMP_INFO_SZ 128

/* Register Defines */
#define CG_CON		0x000
#define CG_CLR		0x008
#define SW_RST		0x00C
#define DONE_ST		0x10C
#define CTRL		0x110
#define XTENSA_INT	0x114
#define CTL_XTENSA_INT	0x118
#define DEFAULT0	0x13C
#define DEFAULT1	0x140
#define DEFAULT2	0x144
#define XTENSA_INFO00	0x150
#define XTENSA_INFO01	0x154
#define XTENSA_INFO02	0x158
#define XTENSA_INFO03	0x15C
#define XTENSA_INFO04	0x160
#define XTENSA_INFO05	0x164
#define XTENSA_INFO06	0x168
#define XTENSA_INFO07	0x16C
#define XTENSA_INFO08	0x170
#define XTENSA_INFO09	0x174
#define XTENSA_INFO10	0x178
#define XTENSA_INFO11	0x17C
#define XTENSA_INFO12	0x180
#define XTENSA_INFO13	0x184
#define XTENSA_INFO14	0x188
#define XTENSA_INFO15	0x18C
#define XTENSA_INFO16	0x190
#define XTENSA_INFO17	0x194
#define XTENSA_INFO18	0x198
#define XTENSA_INFO19	0x19C
#define XTENSA_INFO20	0x1A0
#define XTENSA_INFO21	0x1A4
#define XTENSA_INFO22	0x1A8
#define XTENSA_INFO23	0x1AC
#define XTENSA_INFO24	0x1B0
#define XTENSA_INFO25	0x1B4
#define XTENSA_INFO26	0x1B8
#define XTENSA_INFO27	0x1BC
#define XTENSA_INFO28	0x1C0
#define XTENSA_INFO29	0x1C4
#define XTENSA_INFO30	0x1C8
#define XTENSA_INFO31	0x1CC
#define DEBUG_INFO00	0x1D0
#define DEBUG_INFO01	0x1D4
#define DEBUG_INFO02	0x1D8
#define DEBUG_INFO03	0x1DC
#define DEBUG_INFO04	0x1E0
#define DEBUG_INFO05	0x1E4
#define DEBUG_INFO06	0x1E8
#define DEBUG_INFO07	0x1EC
#define XTENSA_ALTRESETVEC	0x1F8

#endif

