/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SDE_ROTATOR_R3_HWIO_H
#define _SDE_ROTATOR_R3_HWIO_H

#include <linux/bitops.h>

/* MMSS_MDSS:
 * OFFSET=0x000000
 */
#define MMSS_MDSS_HW_INTR_STATUS		0x10
#define MMSS_MDSS_HW_INTR_STATUS_ROT		BIT(2)

/* SDE_ROT_ROTTOP:
 * OFFSET=0x0A8800
 */
#define SDE_ROT_ROTTOP_OFFSET                   0xA8800
#define ROTTOP_HW_VERSION                       (SDE_ROT_ROTTOP_OFFSET+0x00)
#define ROTTOP_CLK_CTRL                         (SDE_ROT_ROTTOP_OFFSET+0x10)
#define ROTTOP_CLK_STATUS                       (SDE_ROT_ROTTOP_OFFSET+0x14)
#define ROTTOP_ROT_NEWROI_PRIOR_TO_START        (SDE_ROT_ROTTOP_OFFSET+0x18)
#define ROTTOP_SW_RESET                         (SDE_ROT_ROTTOP_OFFSET+0x20)
#define ROTTOP_SW_RESET_CTRL                    (SDE_ROT_ROTTOP_OFFSET+0x24)
#define ROTTOP_SW_RESET_OVERRIDE                (SDE_ROT_ROTTOP_OFFSET+0x28)
#define ROTTOP_INTR_EN                          (SDE_ROT_ROTTOP_OFFSET+0x30)
#define ROTTOP_INTR_STATUS                      (SDE_ROT_ROTTOP_OFFSET+0x34)
#define ROTTOP_INTR_CLEAR                       (SDE_ROT_ROTTOP_OFFSET+0x38)
#define ROTTOP_START_CTRL                       (SDE_ROT_ROTTOP_OFFSET+0x40)
#define ROTTOP_STATUS                           (SDE_ROT_ROTTOP_OFFSET+0x44)
#define ROTTOP_OP_MODE                          (SDE_ROT_ROTTOP_OFFSET+0x48)
#define ROTTOP_DNSC                             (SDE_ROT_ROTTOP_OFFSET+0x4C)
#define ROTTOP_DEBUGBUS_CTRL                    (SDE_ROT_ROTTOP_OFFSET+0x50)
#define ROTTOP_DEBUGBUS_STATUS                  (SDE_ROT_ROTTOP_OFFSET+0x54)
#define ROTTOP_ROT_UBWC_DEC_VERSION             (SDE_ROT_ROTTOP_OFFSET+0x58)
#define ROTTOP_ROT_UBWC_ENC_VERSION             (SDE_ROT_ROTTOP_OFFSET+0x5C)
#define ROTTOP_ROT_CNTR_CTRL                    (SDE_ROT_ROTTOP_OFFSET+0x60)
#define ROTTOP_ROT_CNTR_0                       (SDE_ROT_ROTTOP_OFFSET+0x64)
#define ROTTOP_ROT_CNTR_1                       (SDE_ROT_ROTTOP_OFFSET+0x68)
#define ROTTOP_ROT_SCRATCH_0                    (SDE_ROT_ROTTOP_OFFSET+0x70)
#define ROTTOP_ROT_SCRATCH_1                    (SDE_ROT_ROTTOP_OFFSET+0x74)
#define ROTTOP_ROT_SCRATCH_2                    (SDE_ROT_ROTTOP_OFFSET+0x78)
#define ROTTOP_ROT_SCRATCH_3                    (SDE_ROT_ROTTOP_OFFSET+0x7C)

#define ROTTOP_START_CTRL_TRIG_SEL_SW           0
#define ROTTOP_START_CTRL_TRIG_SEL_DONE         1
#define ROTTOP_START_CTRL_TRIG_SEL_REGDMA       2
#define ROTTOP_START_CTRL_TRIG_SEL_MDP          3

#define ROTTOP_OP_MODE_ROT_OUT_MASK             (0x3 << 4)

/* SDE_ROT_SSPP:
 * OFFSET=0x0A8900
 */
#define SDE_ROT_SSPP_OFFSET                     0xA8900
#define ROT_SSPP_SRC_SIZE                       (SDE_ROT_SSPP_OFFSET+0x00)
#define ROT_SSPP_SRC_IMG_SIZE                   (SDE_ROT_SSPP_OFFSET+0x04)
#define ROT_SSPP_SRC_XY                         (SDE_ROT_SSPP_OFFSET+0x08)
#define ROT_SSPP_OUT_SIZE                       (SDE_ROT_SSPP_OFFSET+0x0C)
#define ROT_SSPP_OUT_XY                         (SDE_ROT_SSPP_OFFSET+0x10)
#define ROT_SSPP_SRC0_ADDR                      (SDE_ROT_SSPP_OFFSET+0x14)
#define ROT_SSPP_SRC1_ADDR                      (SDE_ROT_SSPP_OFFSET+0x18)
#define ROT_SSPP_SRC2_ADDR                      (SDE_ROT_SSPP_OFFSET+0x1C)
#define ROT_SSPP_SRC3_ADDR                      (SDE_ROT_SSPP_OFFSET+0x20)
#define ROT_SSPP_SRC_YSTRIDE0                   (SDE_ROT_SSPP_OFFSET+0x24)
#define ROT_SSPP_SRC_YSTRIDE1                   (SDE_ROT_SSPP_OFFSET+0x28)
#define ROT_SSPP_TILE_FRAME_SIZE                (SDE_ROT_SSPP_OFFSET+0x2C)
#define ROT_SSPP_SRC_FORMAT                     (SDE_ROT_SSPP_OFFSET+0x30)
#define ROT_SSPP_SRC_UNPACK_PATTERN             (SDE_ROT_SSPP_OFFSET+0x34)
#define ROT_SSPP_SRC_OP_MODE                    (SDE_ROT_SSPP_OFFSET+0x38)
#define ROT_SSPP_SRC_CONSTANT_COLOR             (SDE_ROT_SSPP_OFFSET+0x3C)
#define ROT_SSPP_UBWC_STATIC_CTRL               (SDE_ROT_SSPP_OFFSET+0x44)
#define ROT_SSPP_FETCH_CONFIG                   (SDE_ROT_SSPP_OFFSET+0x48)
#define ROT_SSPP_VC1_RANGE                      (SDE_ROT_SSPP_OFFSET+0x4C)
#define ROT_SSPP_REQPRIORITY_FIFO_WATERMARK_0   (SDE_ROT_SSPP_OFFSET+0x50)
#define ROT_SSPP_REQPRIORITY_FIFO_WATERMARK_1   (SDE_ROT_SSPP_OFFSET+0x54)
#define ROT_SSPP_REQPRIORITY_FIFO_WATERMARK_2   (SDE_ROT_SSPP_OFFSET+0x58)
#define ROT_SSPP_DANGER_LUT                     (SDE_ROT_SSPP_OFFSET+0x60)
#define ROT_SSPP_SAFE_LUT                       (SDE_ROT_SSPP_OFFSET+0x64)
#define ROT_SSPP_CREQ_LUT                       (SDE_ROT_SSPP_OFFSET+0x68)
#define ROT_SSPP_QOS_CTRL                       (SDE_ROT_SSPP_OFFSET+0x6C)
#define ROT_SSPP_SRC_ADDR_SW_STATUS             (SDE_ROT_SSPP_OFFSET+0x70)
#define ROT_SSPP_CREQ_LUT_0                     (SDE_ROT_SSPP_OFFSET+0x74)
#define ROT_SSPP_CREQ_LUT_1                     (SDE_ROT_SSPP_OFFSET+0x78)
#define ROT_SSPP_CURRENT_SRC0_ADDR              (SDE_ROT_SSPP_OFFSET+0xA4)
#define ROT_SSPP_CURRENT_SRC1_ADDR              (SDE_ROT_SSPP_OFFSET+0xA8)
#define ROT_SSPP_CURRENT_SRC2_ADDR              (SDE_ROT_SSPP_OFFSET+0xAC)
#define ROT_SSPP_CURRENT_SRC3_ADDR              (SDE_ROT_SSPP_OFFSET+0xB0)
#define ROT_SSPP_DECIMATION_CONFIG              (SDE_ROT_SSPP_OFFSET+0xB4)
#define ROT_SSPP_FETCH_SMP_WR_PLANE0            (SDE_ROT_SSPP_OFFSET+0xD0)
#define ROT_SSPP_FETCH_SMP_WR_PLANE1            (SDE_ROT_SSPP_OFFSET+0xD4)
#define ROT_SSPP_FETCH_SMP_WR_PLANE2            (SDE_ROT_SSPP_OFFSET+0xD8)
#define ROT_SSPP_SMP_UNPACK_RD_PLANE0           (SDE_ROT_SSPP_OFFSET+0xE0)
#define ROT_SSPP_SMP_UNPACK_RD_PLANE1           (SDE_ROT_SSPP_OFFSET+0xE4)
#define ROT_SSPP_SMP_UNPACK_RD_PLANE2           (SDE_ROT_SSPP_OFFSET+0xE8)
#define ROT_SSPP_FILL_LEVELS                    (SDE_ROT_SSPP_OFFSET+0xF0)
#define ROT_SSPP_STATUS                         (SDE_ROT_SSPP_OFFSET+0xF4)
#define ROT_SSPP_UNPACK_LINE_COUNT              (SDE_ROT_SSPP_OFFSET+0xF8)
#define ROT_SSPP_UNPACK_BLK_COUNT               (SDE_ROT_SSPP_OFFSET+0xFC)
#define ROT_SSPP_SW_PIX_EXT_C0_LR               (SDE_ROT_SSPP_OFFSET+0x100)
#define ROT_SSPP_SW_PIX_EXT_C0_TB               (SDE_ROT_SSPP_OFFSET+0x104)
#define ROT_SSPP_SW_PIX_EXT_C0_REQ_PIXELS       (SDE_ROT_SSPP_OFFSET+0x108)
#define ROT_SSPP_SW_PIX_EXT_C1C2_LR             (SDE_ROT_SSPP_OFFSET+0x110)
#define ROT_SSPP_SW_PIX_EXT_C1C2_TB             (SDE_ROT_SSPP_OFFSET+0x114)
#define ROT_SSPP_SW_PIX_EXT_C1C2_REQ_PIXELS     (SDE_ROT_SSPP_OFFSET+0x118)
#define ROT_SSPP_SW_PIX_EXT_C3_LR               (SDE_ROT_SSPP_OFFSET+0x120)
#define ROT_SSPP_SW_PIX_EXT_C3_TB               (SDE_ROT_SSPP_OFFSET+0x124)
#define ROT_SSPP_SW_PIX_EXT_C3_REQ_PIXELS       (SDE_ROT_SSPP_OFFSET+0x128)
#define ROT_SSPP_TRAFFIC_SHAPER                 (SDE_ROT_SSPP_OFFSET+0x130)
#define ROT_SSPP_CDP_CNTL                       (SDE_ROT_SSPP_OFFSET+0x134)
#define ROT_SSPP_UBWC_ERROR_STATUS              (SDE_ROT_SSPP_OFFSET+0x138)
#define ROT_SSPP_SW_CROP_W_C0C3                 (SDE_ROT_SSPP_OFFSET+0x140)
#define ROT_SSPP_SW_CROP_W_C1C2                 (SDE_ROT_SSPP_OFFSET+0x144)
#define ROT_SSPP_SW_CROP_H_C0C3                 (SDE_ROT_SSPP_OFFSET+0x148)
#define ROT_SSPP_SW_CROP_H_C1C2                 (SDE_ROT_SSPP_OFFSET+0x14C)
#define ROT_SSPP_TRAFFIC_SHAPER_PREFILL         (SDE_ROT_SSPP_OFFSET+0x150)
#define ROT_SSPP_TRAFFIC_SHAPER_REC1_PREFILL    (SDE_ROT_SSPP_OFFSET+0x154)
#define ROT_SSPP_OUT_SIZE_REC1                  (SDE_ROT_SSPP_OFFSET+0x160)
#define ROT_SSPP_OUT_XY_REC1                    (SDE_ROT_SSPP_OFFSET+0x164)
#define ROT_SSPP_SRC_XY_REC1                    (SDE_ROT_SSPP_OFFSET+0x168)
#define ROT_SSPP_SRC_SIZE_REC1                  (SDE_ROT_SSPP_OFFSET+0x16C)
#define ROT_SSPP_MULTI_REC_OP_MODE              (SDE_ROT_SSPP_OFFSET+0x170)
#define ROT_SSPP_SRC_FORMAT_REC1                (SDE_ROT_SSPP_OFFSET+0x174)
#define ROT_SSPP_SRC_UNPACK_PATTERN_REC1        (SDE_ROT_SSPP_OFFSET+0x178)
#define ROT_SSPP_SRC_OP_MODE_REC1               (SDE_ROT_SSPP_OFFSET+0x17C)
#define ROT_SSPP_SRC_CONSTANT_COLOR_REC1        (SDE_ROT_SSPP_OFFSET+0x180)
#define ROT_SSPP_TPG_CONTROL                    (SDE_ROT_SSPP_OFFSET+0x190)
#define ROT_SSPP_TPG_CONFIG                     (SDE_ROT_SSPP_OFFSET+0x194)
#define ROT_SSPP_TPG_COMPONENT_LIMITS           (SDE_ROT_SSPP_OFFSET+0x198)
#define ROT_SSPP_TPG_RECTANGLE                  (SDE_ROT_SSPP_OFFSET+0x19C)
#define ROT_SSPP_TPG_BLACK_WHITE_PATTERN_FRAMES (SDE_ROT_SSPP_OFFSET+0x1A0)
#define ROT_SSPP_TPG_RGB_MAPPING                (SDE_ROT_SSPP_OFFSET+0x1A4)
#define ROT_SSPP_TPG_PATTERN_GEN_INIT_VAL       (SDE_ROT_SSPP_OFFSET+0x1A8)

#define SDE_ROT_SSPP_FETCH_CONFIG_RESET_VALUE   0x00087
#define SDE_ROT_SSPP_FETCH_BLOCKSIZE_128        (0 << 16)
#define SDE_ROT_SSPP_FETCH_BLOCKSIZE_96         (2 << 16)
#define SDE_ROT_SSPP_FETCH_BLOCKSIZE_192_EXT    ((0 << 16) | (1 << 15))
#define SDE_ROT_SSPP_FETCH_BLOCKSIZE_144_EXT    ((2 << 16) | (1 << 15))


/* SDE_ROT_WB:
 * OFFSET=0x0A8B00
 */
#define SDE_ROT_WB_OFFSET                       0xA8B00
#define ROT_WB_DST_FORMAT                       (SDE_ROT_WB_OFFSET+0x000)
#define ROT_WB_DST_OP_MODE                      (SDE_ROT_WB_OFFSET+0x004)
#define ROT_WB_DST_PACK_PATTERN                 (SDE_ROT_WB_OFFSET+0x008)
#define ROT_WB_DST0_ADDR                        (SDE_ROT_WB_OFFSET+0x00C)
#define ROT_WB_DST1_ADDR                        (SDE_ROT_WB_OFFSET+0x010)
#define ROT_WB_DST2_ADDR                        (SDE_ROT_WB_OFFSET+0x014)
#define ROT_WB_DST3_ADDR                        (SDE_ROT_WB_OFFSET+0x018)
#define ROT_WB_DST_YSTRIDE0                     (SDE_ROT_WB_OFFSET+0x01C)
#define ROT_WB_DST_YSTRIDE1                     (SDE_ROT_WB_OFFSET+0x020)
#define ROT_WB_DST_DITHER_BITDEPTH              (SDE_ROT_WB_OFFSET+0x024)
#define ROT_WB_DITHER_MATRIX_ROW0               (SDE_ROT_WB_OFFSET+0x030)
#define ROT_WB_DITHER_MATRIX_ROW1               (SDE_ROT_WB_OFFSET+0x034)
#define ROT_WB_DITHER_MATRIX_ROW2               (SDE_ROT_WB_OFFSET+0x038)
#define ROT_WB_DITHER_MATRIX_ROW3               (SDE_ROT_WB_OFFSET+0x03C)
#define ROT_WB_TRAFFIC_SHAPER_WR_CLIENT         (SDE_ROT_WB_OFFSET+0x040)
#define ROT_WB_DST_WRITE_CONFIG                 (SDE_ROT_WB_OFFSET+0x048)
#define ROT_WB_ROTATOR_PIPE_DOWNSCALER          (SDE_ROT_WB_OFFSET+0x054)
#define ROT_WB_OUT_SIZE                         (SDE_ROT_WB_OFFSET+0x074)
#define ROT_WB_DST_ALPHA_X_VALUE                (SDE_ROT_WB_OFFSET+0x078)
#define ROT_WB_HW_VERSION                       (SDE_ROT_WB_OFFSET+0x080)
#define ROT_WB_DANGER_LUT                       (SDE_ROT_WB_OFFSET+0x084)
#define ROT_WB_SAFE_LUT                         (SDE_ROT_WB_OFFSET+0x088)
#define ROT_WB_CREQ_LUT                         (SDE_ROT_WB_OFFSET+0x08C)
#define ROT_WB_QOS_CTRL                         (SDE_ROT_WB_OFFSET+0x090)
#define ROT_WB_SYS_CACHE_MODE                   (SDE_ROT_WB_OFFSET+0x094)
#define ROT_WB_CREQ_LUT_0                       (SDE_ROT_WB_OFFSET+0x098)
#define ROT_WB_CREQ_LUT_1                       (SDE_ROT_WB_OFFSET+0x09C)
#define ROT_WB_UBWC_STATIC_CTRL                 (SDE_ROT_WB_OFFSET+0x144)
#define ROT_WB_SBUF_STATUS_PLANE0               (SDE_ROT_WB_OFFSET+0x148)
#define ROT_WB_SBUF_STATUS_PLANE1               (SDE_ROT_WB_OFFSET+0x14C)
#define ROT_WB_CSC_MATRIX_COEFF_0               (SDE_ROT_WB_OFFSET+0x260)
#define ROT_WB_CSC_MATRIX_COEFF_1               (SDE_ROT_WB_OFFSET+0x264)
#define ROT_WB_CSC_MATRIX_COEFF_2               (SDE_ROT_WB_OFFSET+0x268)
#define ROT_WB_CSC_MATRIX_COEFF_3               (SDE_ROT_WB_OFFSET+0x26C)
#define ROT_WB_CSC_MATRIX_COEFF_4               (SDE_ROT_WB_OFFSET+0x270)
#define ROT_WB_CSC_COMP0_PRECLAMP               (SDE_ROT_WB_OFFSET+0x274)
#define ROT_WB_CSC_COMP1_PRECLAMP               (SDE_ROT_WB_OFFSET+0x278)
#define ROT_WB_CSC_COMP2_PRECLAMP               (SDE_ROT_WB_OFFSET+0x27C)
#define ROT_WB_CSC_COMP0_POSTCLAMP              (SDE_ROT_WB_OFFSET+0x280)
#define ROT_WB_CSC_COMP1_POSTCLAMP              (SDE_ROT_WB_OFFSET+0x284)
#define ROT_WB_CSC_COMP2_POSTCLAMP              (SDE_ROT_WB_OFFSET+0x288)
#define ROT_WB_CSC_COMP0_PREBIAS                (SDE_ROT_WB_OFFSET+0x28C)
#define ROT_WB_CSC_COMP1_PREBIAS                (SDE_ROT_WB_OFFSET+0x290)
#define ROT_WB_CSC_COMP2_PREBIAS                (SDE_ROT_WB_OFFSET+0x294)
#define ROT_WB_CSC_COMP0_POSTBIAS               (SDE_ROT_WB_OFFSET+0x298)
#define ROT_WB_CSC_COMP1_POSTBIAS               (SDE_ROT_WB_OFFSET+0x29C)
#define ROT_WB_CSC_COMP2_POSTBIAS               (SDE_ROT_WB_OFFSET+0x2A0)
#define ROT_WB_DST_ADDR_SW_STATUS               (SDE_ROT_WB_OFFSET+0x2B0)
#define ROT_WB_CDP_CNTL                         (SDE_ROT_WB_OFFSET+0x2B4)
#define ROT_WB_STATUS                           (SDE_ROT_WB_OFFSET+0x2B8)
#define ROT_WB_UBWC_ERROR_STATUS                (SDE_ROT_WB_OFFSET+0x2BC)
#define ROT_WB_OUT_IMG_SIZE                     (SDE_ROT_WB_OFFSET+0x2C0)
#define ROT_WB_OUT_XY                           (SDE_ROT_WB_OFFSET+0x2C4)


/* SDE_ROT_REGDMA_RAM:
 * OFFSET=0x0A8E00
 */
#define SDE_ROT_REGDMA_RAM_OFFSET              0xA8E00
#define REGDMA_RAM_REGDMA_CMD_RAM              (SDE_ROT_REGDMA_RAM_OFFSET+0x00)


/* SDE_ROT_REGDMA_CSR:
 * OFFSET=0x0AAE00
 */
#define SDE_ROT_REGDMA_OFFSET                    0xAAE00
#define REGDMA_CSR_REGDMA_VERSION                (SDE_ROT_REGDMA_OFFSET+0x00)
#define REGDMA_CSR_REGDMA_OP_MODE                (SDE_ROT_REGDMA_OFFSET+0x04)
#define REGDMA_CSR_REGDMA_QUEUE_0_SUBMIT         (SDE_ROT_REGDMA_OFFSET+0x10)
#define REGDMA_CSR_REGDMA_QUEUE_0_STATUS         (SDE_ROT_REGDMA_OFFSET+0x14)
#define REGDMA_CSR_REGDMA_QUEUE_1_SUBMIT         (SDE_ROT_REGDMA_OFFSET+0x18)
#define REGDMA_CSR_REGDMA_QUEUE_1_STATUS         (SDE_ROT_REGDMA_OFFSET+0x1C)
#define REGDMA_CSR_REGDMA_BLOCK_LO_0             (SDE_ROT_REGDMA_OFFSET+0x20)
#define REGDMA_CSR_REGDMA_BLOCK_HI_0             (SDE_ROT_REGDMA_OFFSET+0x24)
#define REGDMA_CSR_REGDMA_BLOCK_LO_1             (SDE_ROT_REGDMA_OFFSET+0x28)
#define REGDMA_CSR_REGDMA_BLOCK_HI_1             (SDE_ROT_REGDMA_OFFSET+0x2C)
#define REGDMA_CSR_REGDMA_BLOCK_LO_2             (SDE_ROT_REGDMA_OFFSET+0x30)
#define REGDMA_CSR_REGDMA_BLOCK_HI_2             (SDE_ROT_REGDMA_OFFSET+0x34)
#define REGDMA_CSR_REGDMA_BLOCK_LO_3             (SDE_ROT_REGDMA_OFFSET+0x38)
#define REGDMA_CSR_REGDMA_BLOCK_HI_3             (SDE_ROT_REGDMA_OFFSET+0x3C)
#define REGDMA_CSR_REGDMA_WD_TIMER_CTL           (SDE_ROT_REGDMA_OFFSET+0x40)
#define REGDMA_CSR_REGDMA_WD_TIMER_CTL2          (SDE_ROT_REGDMA_OFFSET+0x44)
#define REGDMA_CSR_REGDMA_WD_TIMER_LOAD_VALUE    (SDE_ROT_REGDMA_OFFSET+0x48)
#define REGDMA_CSR_REGDMA_WD_TIMER_STATUS_VALUE  (SDE_ROT_REGDMA_OFFSET+0x4C)
#define REGDMA_CSR_REGDMA_INT_STATUS             (SDE_ROT_REGDMA_OFFSET+0x50)
#define REGDMA_CSR_REGDMA_INT_EN                 (SDE_ROT_REGDMA_OFFSET+0x54)
#define REGDMA_CSR_REGDMA_INT_CLEAR              (SDE_ROT_REGDMA_OFFSET+0x58)
#define REGDMA_CSR_REGDMA_BLOCK_STATUS           (SDE_ROT_REGDMA_OFFSET+0x5C)
#define REGDMA_CSR_REGDMA_INVALID_CMD_RAM_OFFSET (SDE_ROT_REGDMA_OFFSET+0x60)
#define REGDMA_CSR_REGDMA_FSM_STATE              (SDE_ROT_REGDMA_OFFSET+0x64)
#define REGDMA_CSR_REGDMA_DEBUG_SEL              (SDE_ROT_REGDMA_OFFSET+0x68)


/* SDE_ROT_QDSS:
 * OFFSET=0x0AAF00
 */
#define ROT_QDSS_CONFIG                          0x00
#define ROT_QDSS_ATB_DATA_ENABLE0                0x04
#define ROT_QDSS_ATB_DATA_ENABLE1                0x08
#define ROT_QDSS_ATB_DATA_ENABLE2                0x0C
#define ROT_QDSS_ATB_DATA_ENABLE3                0x10
#define ROT_QDSS_CLK_CTRL                        0x14
#define ROT_QDSS_CLK_STATUS                      0x18
#define ROT_QDSS_PULSE_TRIGGER                   0x20

/*
 * SDE_ROT_VBIF_NRT:
 */
#define SDE_ROT_VBIF_NRT_OFFSET                  0

/* REGDMA OP Code */
#define REGDMA_OP_NOP                   (0 << 28)
#define REGDMA_OP_REGWRITE              (1 << 28)
#define REGDMA_OP_REGMODIFY             (2 << 28)
#define REGDMA_OP_BLKWRITE_SINGLE       (3 << 28)
#define REGDMA_OP_BLKWRITE_INC          (4 << 28)
#define REGDMA_OP_MASK                  0xF0000000

/* REGDMA ADDR offset Mask */
#define REGDMA_ADDR_OFFSET_MASK         0xFFFFF

/* REGDMA command trigger select */
#define REGDMA_CMD_TRIG_SEL_SW_START    (0 << 27)
#define REGDMA_CMD_TRIG_SEL_MDP_FLUSH   (1 << 27)

/* General defines */
#define ROT_DONE_MASK                   0x1
#define ROT_DONE_CLEAR                  0x1
#define ROT_BUSY_BIT                    BIT(0)
#define ROT_ERROR_BIT                   BIT(8)
#define ROT_STATUS_MASK                 (ROT_BUSY_BIT | ROT_ERROR_BIT)
#define REGDMA_BUSY                     BIT(0)
#define REGDMA_EN                       0x1
#define REGDMA_SECURE_EN                BIT(8)
#define REGDMA_HALT                     BIT(16)

#define REGDMA_WATCHDOG_INT             BIT(19)
#define REGDMA_INVALID_DESCRIPTOR       BIT(18)
#define REGDMA_INCOMPLETE_CMD           BIT(17)
#define REGDMA_INVALID_CMD              BIT(16)
#define REGDMA_QUEUE1_INT2              BIT(10)
#define REGDMA_QUEUE1_INT1              BIT(9)
#define REGDMA_QUEUE1_INT0              BIT(8)
#define REGDMA_QUEUE0_INT2              BIT(2)
#define REGDMA_QUEUE0_INT1              BIT(1)
#define REGDMA_QUEUE0_INT0              BIT(0)
#define REGDMA_INT_MASK                 0x000F0707
#define REGDMA_INT_HIGH_MASK            0x00000007
#define REGDMA_INT_LOW_MASK             0x00000700
#define REGDMA_INT_ERR_MASK             0x000F0000
#define REGDMA_TIMESTAMP_REG            ROT_SSPP_TPG_PATTERN_GEN_INIT_VAL
#define REGDMA_RESET_STATUS_REG         ROT_SSPP_TPG_RGB_MAPPING

#define REGDMA_INT_0_MASK               0x101
#define REGDMA_INT_1_MASK               0x202
#define REGDMA_INT_2_MASK               0x404

#endif /*_SDE_ROTATOR_R3_HWIO_H */
