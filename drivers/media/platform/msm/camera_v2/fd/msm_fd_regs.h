/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2015, 2018, 2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_FD_REGS_H__
#define __MSM_FD_REGS_H__

/* FD core registers */
#define MSM_FD_CONTROL (0x00)
#define MSM_FD_CONTROL_SRST   (1 << 0)
#define MSM_FD_CONTROL_RUN    (1 << 1)
#define MSM_FD_CONTROL_FINISH (1 << 2)

#define MSM_FD_RESULT_CNT (0x04)
#define MSM_FD_RESULT_CNT_MASK (0x3F)

#define MSM_FD_CONDT (0x08)
#define MSM_FD_CONDT_MIN_MASK  (0x03)
#define MSM_FD_CONDT_MIN_SHIFT (0x00)
#define MSM_FD_CONDT_DIR_MAX   (0x08)
#define MSM_FD_CONDT_DIR_MASK  (0x3C)
#define MSM_FD_CONDT_DIR_SHIFT (0x02)

#define MSM_FD_START_X (0x0C)
#define MSM_FD_START_X_MASK (0x3FF)

#define MSM_FD_START_Y (0x10)
#define MSM_FD_START_Y_MASK (0x1FF)

#define MSM_FD_SIZE_X (0x14)
#define MSM_FD_SIZE_X_MASK (0x3FF)

#define MSM_FD_SIZE_Y (0x18)
#define MSM_FD_SIZE_Y_MASK (0x1FF)

#define MSM_FD_DHINT (0x1C)
#define MSM_FD_DHINT_MASK (0xF)

#define MSM_FD_IMAGE_ADDR (0x24)
#define MSM_FD_IMAGE_ADDR_ALIGN (0x8)

#define MSM_FD_WORK_ADDR (0x28)
#define MSM_FD_WORK_ADDR_ALIGN (0x8)

#define MSM_FD_IMAGE_SIZE (0x2C)
#define MSM_FD_IMAGE_SIZE_QVGA  (0x0)
#define MSM_FD_IMAGE_SIZE_VGA   (0x1)
#define MSM_FD_IMAGE_SIZE_WQVGA (0x2)
#define MSM_FD_IMAGE_SIZE_WVGA  (0x3)

#define MSM_FD_LINE_BYTES (0x30)
#define MSM_FD_LINE_BYTES_MASK  (0x1FFF)
#define MSM_FD_LINE_BYTES_ALIGN (0x8)

#define MSM_FD_RESULT_CENTER_X(x) (0x400 + (0x10 * (x)))

#define MSM_FD_RESULT_CENTER_Y(x) (0x404 + (0x10 * (x)))

#define MSM_FD_RESULT_CONF_SIZE(x) (0x408 + (0x10 * (x)))
#define MSM_FD_RESULT_SIZE_MASK  (0x1FF)
#define MSM_FD_RESULT_SIZE_SHIFT (0x000)
#define MSM_FD_RESULT_CONF_MASK  (0xF)
#define MSM_FD_RESULT_CONF_SHIFT (0x9)

#define MSM_FD_RESULT_ANGLE_POSE(x) (0x40C + (0x10 * (x)))
#define MSM_FD_RESULT_ANGLE_MASK  (0x1FF)
#define MSM_FD_RESULT_ANGLE_SHIFT (0x000)
#define MSM_FD_RESULT_POSE_MASK   (0x7)
#define MSM_FD_RESULT_POSE_SHIFT  (0x9)
#define MSM_FD_RESULT_POSE_FRONT           (0x1)
#define MSM_FD_RESULT_POSE_RIGHT_DIAGONAL  (0x2)
#define MSM_FD_RESULT_POSE_RIGHT           (0x3)
#define MSM_FD_RESULT_POSE_LEFT_DIAGONAL   (0x4)
#define MSM_FD_RESULT_POSE_LEFT            (0x5)

/* FD misc registers */
#define MSM_FD_MISC_HW_VERSION (0x00)
#define MSM_FD_MISC_CGC_DISABLE (0x04)
#define MSM_FD_HW_STOP          (0x08)

#define MSM_FD_MISC_SW_RESET (0x10)
#define MSM_FD_MISC_SW_RESET_SET (1 << 0)

#define MSM_FD_MISC_FIFO_STATUS (0x14)
#define MSM_FD_MISC_FIFO_STATUS_RFIFO_DCNT_MAST (0x1F)
#define MSM_FD_MISC_FIFO_STATUS_RFIFO_DCNT_SHIFT (0)
#define MSM_FD_MISC_FIFO_STATUS_RFIFO_FULL  (1 << 13)
#define MSM_FD_MISC_FIFO_STATUS_RFIFO_EMPTY (1 << 14)
#define MSM_FD_MISC_FIFO_STATUS_WFIFO_DCNT_MAST (0x1F)
#define MSM_FD_MISC_FIFO_STATUS_WFIFO_DCNT_SHIFT (16)
#define MSM_FD_MISC_FIFO_STATUS_WFIFO_EMPTY (1 << 29)
#define MSM_FD_MISC_FIFO_STATUS_WFIFO_FULL  (1 << 30)

#define MSM_FD_MISC_DATA_ENDIAN (0x18)
#define MSM_FD_MISC_DATA_ENDIAN_BYTE_SWAP_SET (1 << 0)

#define MSM_FD_MISC_VBIF_REQ_PRIO (0x20)
#define MSM_FD_MISC_VBIF_REQ_PRIO_MASK (0x3)

#define MSM_FD_MISC_VBIF_PRIO_LEVEL (0x24)
#define MSM_FD_MISC_VBIF_PRIO_LEVEL_MASK (0x3)

#define MSM_FD_MISC_VBIF_MMU_PDIRECT (0x28)
#define MSM_FD_MISC_VBIF_MMU_PDIRECT_INCREMENT (1 << 0)

#define MSM_FD_MISC_VBIF_IRQ_CLR (0x30)
#define MSM_FD_MISC_VBIF_IRQ_CLR_ALL (1 << 0)

#define MSM_FD_MISC_VBIF_DONE_STATUS (0x34)
#define MSM_FD_MISC_VBIF_DONE_STATUS_WRITE (1 << 0)
#define MSM_FD_MISC_VBIF_DONE_STATUS_READ  (1 << 1)

#define MSM_FD_MISC_IRQ_MASK (0x50)
#define MSM_FD_MISC_IRQ_MASK_HALT_REQ (1 << 1)
#define MSM_FD_MISC_IRQ_MASK_CORE_IRQ (1 << 0)

#define MSM_FD_MISC_IRQ_STATUS (0x54)
#define MSM_FD_MISC_IRQ_STATUS_HALT_REQ (1 << 1)
#define MSM_FD_MISC_IRQ_STATUS_CORE_IRQ (1 << 0)

#define MSM_FD_MISC_IRQ_CLEAR (0x58)
#define MSM_FD_MISC_IRQ_CLEAR_HALT (1 << 1)
#define MSM_FD_MISC_IRQ_CLEAR_CORE (1 << 0)

#define MSM_FD_MISC_TEST_BUS_SEL (0x40)
#define MSM_FD_MISC_TEST_BUS_SEL_TEST_MODE_MASK  (0xF)
#define MSM_FD_MISC_TEST_BUS_SEL_TEST_MODE_SHIFT (0)
#define MSM_FD_MISC_TEST_BUS_SEL_7_0_MASK    (0x3)
#define MSM_FD_MISC_TEST_BUS_SEL_7_0_SHIFT   (16)
#define MSM_FD_MISC_TEST_BUS_SEL_15_8_MASK   (0x3)
#define MSM_FD_MISC_TEST_BUS_SEL_15_8_SHIFT  (18)
#define MSM_FD_MISC_TEST_BUS_SEL_23_16_MASK  (0x3)
#define MSM_FD_MISC_TEST_BUS_SEL_23_16_SHIFT (20)
#define MSM_FD_MISC_TEST_BUS_SEL_31_24_MASK  (0x3)
#define MSM_FD_MISC_TEST_BUS_SEL_31_24_SHIFT (22)

#define MSM_FD_MISC_AHB_TEST_EN (0x44)
#define MSM_FD_MISC_AHB_TEST_EN_MASK (0x3)

#define MSM_FD_MISC_FD2VBIF_INT_TEST_SEL  (0x48)
#define MSM_FD_MISC_FD2VBIF_INT_TEST_MASK (0xF)

#define MSM_FD_MISC_TEST_BUS (0x4C)

/* FD vbif registers */
#define MSM_FD_VBIF_CLKON                   (0x04)
#define MSM_FD_VBIF_QOS_OVERRIDE_EN         (0x10)
#define MSM_FD_VBIF_QOS_OVERRIDE_REQPRI     (0x18)
#define MSM_FD_VBIF_QOS_OVERRIDE_PRILVL     (0x1C)
#define MSM_FD_VBIF_IN_RD_LIM_CONF0         (0xB0)
#define MSM_FD_VBIF_IN_WR_LIM_CONF0         (0xC0)
#define MSM_FD_VBIF_OUT_RD_LIM_CONF0        (0xD0)
#define MSM_FD_VBIF_OUT_WR_LIM_CONF0        (0xD4)
#define MSM_FD_VBIF_DDR_OUT_MAX_BURST       (0xD8)
#define MSM_FD_VBIF_ARB_CTL                 (0xF0)
#define MSM_FD_VBIF_OUT_AXI_AMEMTYPE_CONF0  (0x160)
#define MSM_FD_VBIF_OUT_AXI_AOOO_EN         (0x178)
#define MSM_FD_VBIF_OUT_AXI_AOOO            (0x17c)
#define MSM_FD_VBIF_ROUND_ROBIN_QOS_ARB     (0x124)

#endif /* __MSM_FD_REGS_H__ */
