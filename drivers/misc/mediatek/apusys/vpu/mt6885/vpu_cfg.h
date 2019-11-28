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

/**
 * Working buffer's offset
 *
 *  [offset]
 *  0x00000000  +-----------------------+
 *              |  Command Buffer       |
 *              |              [8KB]    |
 *  0x00002000  +-----------------------+
 *              |  Log Buffer           |
 *              |              [8KB]    |
 *              +-----------------------+
 *
 * the first 16 bytes of log buffer:
 *   @tail_addr: the mva of log end, which always points to '\0'
 *
 *   +-----------+----------------------+
 *   |   0 ~ 3   |      4 ~ 15          |
 *   +-----------+----------------------+
 *   |{tail_addr}|    {reserved}        |
 *   +-----------+----------------------+
 */
#define VPU_OFFSET_COMMAND           (0x00000000)
#define VPU_OFFSET_LOG               (0x00002000)
#define VPU_SIZE_LOG_BUF             (0x00010000)
#define VPU_SIZE_LOG_SHIFT           (0x00000300)
#define VPU_SIZE_LOG_HEADER          (0x00000010)
#define VPU_SIZE_WORK_BUF            (0x00012000)
#define VPU_SIZE_LOG_DATA  (VPU_SIZE_LOG_BUF - VPU_SIZE_LOG_HEADER)

 /* Time Constrains */
#define VPU_CMD_TIMEOUT  (3000)
#define VPU_CHECK_COUNT  (2000)
#define VPU_CHECK_MIN_US (500)
#define VPU_CHECK_MAX_US (1000)
#define VPU_PWR_OFF_LATENCY (3000)
#define WAIT_COMMAND_RETRY  (5)

 /* Remote Proc */
#define VPU_REMOTE_PROC (0)

#endif
