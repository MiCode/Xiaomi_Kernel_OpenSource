/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __VPU_CFG_H__
#define __VPU_CFG_H__

#include <linux/types.h>

struct vpu_device;

/* 20180703, 00:00: vpu log mechanism */
#define HOST_VERSION	(0x18070300)

/* Firmware Binary  */
#define VPU_MAX_NUM_CODE_SEGMENTS       (50)
#define VPU_MAX_NUM_ALGOS               (50)
#define VPU_NUMS_IMAGE_HEADER           (3)

/* Command */
#define VPU_CMD_SIZE  (0x2000)
#define VPU_CMD_BUF_NUM (1)

/* Image Layout */
#define VPU_IMG_LEGACY  (0)
#define VPU_IMG_PRELOAD (1)

/* VPU XOS */
#define VPU_NON_XOS (0)
#define VPU_XOS (1)

#define XOS_TIMEOUT_US (1000000)

#define VPU_MAX_PRIORITY (3)

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

extern struct vpu_config vpu_cfg_mt68xx;
extern struct vpu_config vpu_cfg_mt67xx;
extern struct vpu_register vpu_reg_mt68xx;
extern struct vpu_register vpu_reg_mt67xx;
extern struct vpu_misc_ops vpu_cops_mt6885;
extern struct vpu_misc_ops vpu_cops_mt68xx;
extern struct vpu_misc_ops vpu_cops_mt67xx;

#endif
