/* SPDX-License-Identifier: GPL-2.0 */
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef __MTK_AIE_H__
#define __MTK_AIE_H__

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>

#include "mtk-interconnect.h"
#include "mtk_imgsys-dev.h"

#define AIE_IOVA_BITS_SHIFT 4
#define AIE_IOVA(IOVA) (IOVA >> AIE_IOVA_BITS_SHIFT)

#define V4L2_META_FMT_MTFD_RESULT  v4l2_fourcc('M', 'T', 'f', 'd')

#define MTK_AIE_OPP_SET			1
#define MTK_AIE_CLK_LEVEL_CNT		4
#define FLD_MAX_INPUT			15

#define FD_VERSION 1946050
#define ATTR_VERSION 1929401

#define Y2R_CONFIG_SIZE 34
#define RS_CONFIG_SIZE 30
#define FD_CONFIG_SIZE 56

#define Y2R_SRC_DST_FORMAT 0
#define Y2R_IN_W_H 1
#define Y2R_OUT_W_H 2
#define Y2R_RA0_RA1_EN 3
#define Y2R_IN_X_Y_SIZE0 4
#define Y2R_IN_STRIDE0_BUS_SIZE0 5
#define Y2R_IN_X_Y_SIZE1 6
#define Y2R_IN_STRIDE1_BUS_SIZE1 7
#define Y2R_OUT_X_Y_SIZE0 8
#define Y2R_OUT_STRIDE0_BUS_SIZE0 9
#define Y2R_OUT_X_Y_SIZE1 10
#define Y2R_OUT_STRIDE1_BUS_SIZE1 11
#define Y2R_OUT_X_Y_SIZE2 12
#define Y2R_OUT_STRIDE2_BUS_SIZE2 13
#define Y2R_IN_0 14
#define Y2R_IN_1 15
#define Y2R_OUT_0 16
#define Y2R_OUT_1 17
#define Y2R_OUT_2 18
#define Y2R_RS_SEL_SRZ_EN 19
#define Y2R_X_Y_MAG 20
#define Y2R_SRZ_HORI_STEP 22
#define Y2R_SRZ_VERT_STEP 23
#define Y2R_PADDING_EN_UP_DOWN 26
#define Y2R_PADDING_RIGHT_LEFT 27
#define Y2R_CO2_FMT_MODE_EN 28 /* AIE3.0 new */
#define Y2R_CO2_CROP_X 29      /* AIE3.0 new */
#define Y2R_CO2_CROP_Y 30      /* AIE3.0 new */

#define RS_IN_0 22
#define RS_IN_1 23
#define RS_IN_2 24
#define RS_OUT_0 25
#define RS_OUT_1 26
#define RS_OUT_2 27
#define RS_X_Y_MAG 1
#define RS_SRZ_HORI_STEP 3
#define RS_SRZ_VERT_STEP 4
#define RS_INPUT_W_H 7
#define RS_OUTPUT_W_H 8
#define RS_IN_X_Y_SIZE0 10
#define RS_IN_STRIDE0 11
#define RS_IN_X_Y_SIZE1 12
#define RS_IN_STRIDE1 13
#define RS_IN_X_Y_SIZE2 14
#define RS_IN_STRIDE2 15
#define RS_OUT_X_Y_SIZE0 16
#define RS_OUT_STRIDE0 17
#define RS_OUT_X_Y_SIZE1 18
#define RS_OUT_STRIDE1 19
#define RS_OUT_X_Y_SIZE2 20
#define RS_OUT_STRIDE2 21

#define FD_IN_CHANNEL_PACK 0
#define FD_INPUT_ROTATE 1 /* AIE3.0 new */
#define FD_CONV_WIDTH_MOD6 2
#define FD_CONV_IMG_W_H 4

#define FD_IN_IMG_W_H 5
#define FD_OUT_IMG_W_H 6

#define FD_IN_X_Y_SIZE0 9
#define FD_IN_X_Y_SIZE1 11
#define FD_IN_X_Y_SIZE2 13
#define FD_IN_X_Y_SIZE3 15

#define FD_IN_STRIDE0_BUS_SIZE0 10
#define FD_IN_STRIDE1_BUS_SIZE1 12
#define FD_IN_STRIDE2_BUS_SIZE2 14
#define FD_IN_STRIDE3_BUS_SIZE3 16

#define FD_OUT_X_Y_SIZE0 17
#define FD_OUT_X_Y_SIZE1 19
#define FD_OUT_X_Y_SIZE2 21
#define FD_OUT_X_Y_SIZE3 23

#define FD_OUT_STRIDE0_BUS_SIZE0 18
#define FD_OUT_STRIDE1_BUS_SIZE1 20
#define FD_OUT_STRIDE2_BUS_SIZE2 22
#define FD_OUT_STRIDE3_BUS_SIZE3 24

#define FD_IN_0 27
#define FD_IN_1 28
#define FD_IN_2 29
#define FD_IN_3 30

#define FD_OUT_0 31
#define FD_OUT_1 32
#define FD_OUT_2 33
#define FD_OUT_3 34

#define FD_KERNEL_0 35
#define FD_KERNEL_1 36

#define FD_RPN_SET 37
#define FD_IMAGE_COORD 38
#define FD_IMAGE_COORD_XY_OFST 39   /* AIE3.0 new */
#define FD_BIAS_ACCU 47		    /* AIE3.0 new */
#define FD_SRZ_FDRZ_RS 48	   /* AIE3.0 new */
#define FD_SRZ_HORI_STEP 49	 /* AIE3.0 new */
#define FD_SRZ_VERT_STEP 50	 /* AIE3.0 new */
#define FD_SRZ_HORI_SUB_INT_OFST 51 /* AIE3.0 new */
#define FD_SRZ_VERT_SUB_INT_OFST 52 /* AIE3.0 new */
/*MSB bit*/
#define POS_FDCON_IN_BA_MSB 53
#define POS_FDCON_OUT_BA_MSB 54
#define POS_FDCON_KERNEL_BA_MSB 55

#define POS_RSCON_IN_BA_MSB 28
#define POS_RSCON_OUT_BA_MSB 29

#define POS_Y2RCON_IN_BA_MSB 31
#define POS_Y2RCON_OUT_BA_MSB 32

#define FDRZ_BIT ((0x0 << 16) | (0x0 << 12) | (0x0 << 8) | 0x0)
#define SRZ_BIT ((0x1 << 16) | (0x1 << 12) | (0x1 << 8) | 0x1)

/* config size */
#define fd_rs_confi_size 240 /* AIE3.0: 120*2=240 */
#define fd_fd_confi_size 19488 /* AIE3.0: 56*4=224, 216*87=19488*/
#define fd_yuv2rgb_confi_size 136 /* AIE3.0:136 */


#define attr_fd_confi_size 5824     /* AIE3.0:56*4*26=5824 */
#define attr_yuv2rgb_confi_size 136 /* AIE3.0:136 */

#define result_size 49152 /* 384 * 1024 / 8 */ /* AIE2.0 and AIE3.0 */

#define fd_loop_num 87
#define rpn0_loop_num 86
#define rpn1_loop_num 57
#define rpn2_loop_num 28

#define pym0_start_loop 58
#define pym1_start_loop 29
#define pym2_start_loop 0

#define attr_loop_num 26
#define age_out_rgs 17
#define gender_out_rgs 20
#define indian_out_rgs 22
#define race_out_rgs 25

#define input_WDMA_WRA_num 4
#define output_WDMA_WRA_num 4
#define kernel_RDMA_RA_num 2

#define MAX_ENQUE_FRAME_NUM 10
#define PYM_NUM 3
#define COLOR_NUM 3

#define ATTR_MODE_PYRAMID_WIDTH 128
#define ATTR_OUT_SIZE 32

/* AIE 3.0 register offset */
#define AIE_START_REG 0x000
#define AIE_ENABLE_REG 0x004
#define AIE_LOOP_REG 0x008
#define AIE_YUV2RGB_CON_BASE_ADR_REG 0x00c
#define AIE_RS_CON_BASE_ADR_REG 0x010
#define AIE_FD_CON_BASE_ADR_REG 0x014
#define AIE_INT_EN_REG 0x018
#define AIE_INT_REG 0x01c
#define AIE_RESULT_0_REG 0x08c
#define AIE_RESULT_1_REG 0x090
#define AIE_DMA_CTL_REG 0x094
#define FDVT_YUV2RGB_CON 0x020
#define FDVT_SRC_WD_HT 0x040
#define FDVT_DES_WD_HT 0x044
#define FDVT_DEBUG_INFO_0 0x10c
#define FDVT_DEBUG_INFO_1 0x110
#define FDVT_DEBUG_INFO_2 0x114

#define FDVT_YUV2RGB_CON_BASE_ADR_MSB    0x14C
#define FDVT_RS_CON_BASE_ADR_MSB         0x150
#define FDVT_FD_CON_BASE_ADR_MSB         0x154

#define FDVT_CTRL_REG      0x098
#define FDVT_IN_BASE_ADR_0 0x09c
#define FDVT_IN_BASE_ADR_1 0x0a0
#define FDVT_IN_BASE_ADR_2 0x0a4
#define FDVT_IN_BASE_ADR_3 0x0a8
#define FDVT_OUT_BASE_ADR_0 0x0ac
#define FDVT_OUT_BASE_ADR_1 0x0b0
#define FDVT_OUT_BASE_ADR_2 0x0b4
#define FDVT_OUT_BASE_ADR_3 0x0b8
#define FDVT_KERNEL_BASE_ADR_0 0x0bc
#define FDVT_KERNEL_BASE_ADR_1 0x0c0
#define DMA_DEBUG_SEL_REG 0x3f4

#define FDVT_DMA_RDMA_0_CHECK_SUM 0x240
#define FDVT_DMA_RDMA_1_CHECK_SUM 0x244
#define FDVT_DMA_RDMA_2_CHECK_SUM 0x248
#define FDVT_DMA_RDMA_3_CHECK_SUM 0x24c

#define FDVT_DMA_WDMA_0_CHECK_SUM 0x250
#define FDVT_DMA_WDMA_1_CHECK_SUM 0x254
#define FDVT_DMA_WDMA_2_CHECK_SUM 0x258
#define FDVT_DMA_WDMA_3_CHECK_SUM 0x25c

#define FDVT_DMA_DEBUG_SEL 0x278
#define FDVT_DMA_DEBUG_DATA_RDMA_0_0 0x280
#define FDVT_DMA_DEBUG_DATA_RDMA_0_1 0x284
#define FDVT_DMA_DEBUG_DATA_RDMA_0_2 0x288
#define FDVT_DMA_DEBUG_DATA_RDMA_0_3 0x28c

#define FDVT_DMA_DEBUG_DATA_RDMA_1_0 0x290
#define FDVT_DMA_DEBUG_DATA_RDMA_1_1 0x294
#define FDVT_DMA_DEBUG_DATA_RDMA_1_2 0x298
#define FDVT_DMA_DEBUG_DATA_RDMA_1_3 0x29c

#define FDVT_DMA_DEBUG_DATA_RDMA_2_0 0x2a0
#define FDVT_DMA_DEBUG_DATA_RDMA_2_1 0x2a4
#define FDVT_DMA_DEBUG_DATA_RDMA_2_2 0x2a8
#define FDVT_DMA_DEBUG_DATA_RDMA_2_3 0x2ac

#define FDVT_DMA_DEBUG_DATA_RDMA_3_0 0x2b0
#define FDVT_DMA_DEBUG_DATA_RDMA_3_1 0x2b4
#define FDVT_DMA_DEBUG_DATA_RDMA_3_2 0x2b8
#define FDVT_DMA_DEBUG_DATA_RDMA_3_3 0x2bc

#define FDVT_DMA_DEBUG_DATA_WDMA_0_0 0x2c0
#define FDVT_DMA_DEBUG_DATA_WDMA_0_1 0x2c4
#define FDVT_DMA_DEBUG_DATA_WDMA_0_2 0x2c8
#define FDVT_DMA_DEBUG_DATA_WDMA_0_3 0x2cc

#define FDVT_DMA_DEBUG_DATA_WDMA_1_0 0x2d0
#define FDVT_DMA_DEBUG_DATA_WDMA_1_1 0x2d4
#define FDVT_DMA_DEBUG_DATA_WDMA_1_2 0x2d8
#define FDVT_DMA_DEBUG_DATA_WDMA_1_3 0x2dc

#define FDVT_DMA_DEBUG_DATA_WDMA_2_0 0x2e0
#define FDVT_DMA_DEBUG_DATA_WDMA_2_1 0x2e4
#define FDVT_DMA_DEBUG_DATA_WDMA_2_2 0x2e8
#define FDVT_DMA_DEBUG_DATA_WDMA_2_3 0x2ec

#define FDVT_DMA_DEBUG_DATA_WDMA_3_0 0x2f0
#define FDVT_DMA_DEBUG_DATA_WDMA_3_1 0x2f4
#define FDVT_DMA_DEBUG_DATA_WDMA_3_2 0x2f8
#define FDVT_DMA_DEBUG_DATA_WDMA_3_3 0x2fc
#define FDVT_DMA_ERR_STATUS 0x300

#define FDVT_WRA_0_CON3_REG 0x254
#define FDVT_WRA_1_CON3_REG 0x284

#define FDVT_RDA_0_CON3_REG 0x2b4
#define FDVT_RDA_1_CON3_REG 0x2e4

#define FDVT_WRB_0_CON3_REG 0x314
#define FDVT_WRB_1_CON3_REG 0x344

#define FDVT_RDB_0_CON3_REG 0x374
#define FDVT_RDB_1_CON3_REG 0x3a4

/*CMDQ ADDRESS*/
#define CMDQ_REG_MASK 0xffffffff

#define FDVT_BASE_HW                        0x15310000
#define FDVT_START_HW                      (FDVT_BASE_HW + 0x000)
#define FDVT_ENABLE_HW                     (FDVT_BASE_HW + 0x004)
#define FDVT_LOOP_HW                       (FDVT_BASE_HW + 0x008)
#define FDVT_YUV2RGB_CON_BASE_ADR_HW       (FDVT_BASE_HW + 0x00c)
#define FDVT_RS_CON_BASE_ADR_HW            (FDVT_BASE_HW + 0x010)
#define FDVT_FD_CON_BASE_ADR_HW            (FDVT_BASE_HW + 0x014)
#define FDVT_INT_EN_HW                     (FDVT_BASE_HW + 0x018)
#define FDVT_INT_HW                        (FDVT_BASE_HW + 0x01c)
#define FDVT_YUV2RGB_CON_HW                (FDVT_BASE_HW + 0x020)
#define FDVT_RS_CON_HW                     (FDVT_BASE_HW + 0x024)
#define FDVT_RS_FDRZ_CON0_HW               (FDVT_BASE_HW + 0x028)
#define FDVT_RS_FDRZ_CON1_HW               (FDVT_BASE_HW + 0x02c)
#define FDVT_RS_SRZ_CON0_HW                (FDVT_BASE_HW + 0x030)
#define FDVT_RS_SRZ_CON1_HW                (FDVT_BASE_HW + 0x034)
#define FDVT_RS_SRZ_CON2_HW                (FDVT_BASE_HW + 0x038)
#define FDVT_RS_SRZ_CON3_HW                (FDVT_BASE_HW + 0x03c)
#define FDVT_SRC_WD_HT_HW                  (FDVT_BASE_HW + 0x040)
#define FDVT_DES_WD_HT_HW                  (FDVT_BASE_HW + 0x044)
#define FDVT_CONV_WD_HT_HW                 (FDVT_BASE_HW + 0x048)
#define FDVT_KERNEL_HW                     (FDVT_BASE_HW + 0x04c)
#define FDVT_FD_PACK_MODE_HW               (FDVT_BASE_HW + 0x050)
#define FDVT_CONV0_HW                      (FDVT_BASE_HW + 0x054)
#define FDVT_CONV1_HW                      (FDVT_BASE_HW + 0x058)
#define FDVT_CONV2_HW                      (FDVT_BASE_HW + 0x05c)
#define FDVT_RPN_HW                        (FDVT_BASE_HW + 0x060)
#define FDVT_RPN_IMAGE_COORD_HW            (FDVT_BASE_HW + 0x064)
#define FDVT_FD_ANCHOR_0_HW                (FDVT_BASE_HW + 0x068)
#define FDVT_FD_ANCHOR_1_HW                (FDVT_BASE_HW + 0x06c)
#define FDVT_FD_ANCHOR_2_HW                (FDVT_BASE_HW + 0x070)
#define FDVT_FD_ANCHOR_3_HW                (FDVT_BASE_HW + 0x074)
#define FDVT_FD_ANCHOR_4_HW                (FDVT_BASE_HW + 0x078)
#define FDVT_ANCHOR_SHIFT_MODE_0_HW        (FDVT_BASE_HW + 0x07c)
#define FDVT_ANCHOR_SHIFT_MODE_1_HW        (FDVT_BASE_HW + 0x080)
#define FDVT_LANDMARK_SHIFT_MODE_0_HW      (FDVT_BASE_HW + 0x084)
#define FDVT_LANDMARK_SHIFT_MODE_1_HW      (FDVT_BASE_HW + 0x088)
#define FDVT_RESULT_0_HW                   (FDVT_BASE_HW + 0x08c)
#define FDVT_RESULT_1_HW                   (FDVT_BASE_HW + 0x090)
#define FDVT_DMA_CTL_HW                    (FDVT_BASE_HW + 0x094)
#define FDVT_CTRL_HW                       (FDVT_BASE_HW + 0x098)
#define FDVT_IN_BASE_ADR_0_HW              (FDVT_BASE_HW + 0x09c)
#define FDVT_IN_BASE_ADR_1_HW              (FDVT_BASE_HW + 0x0a0)
#define FDVT_IN_BASE_ADR_2_HW              (FDVT_BASE_HW + 0x0a4)
#define FDVT_IN_BASE_ADR_3_HW              (FDVT_BASE_HW + 0x0a8)
#define FDVT_OUT_BASE_ADR_0_HW             (FDVT_BASE_HW + 0x0ac)
#define FDVT_OUT_BASE_ADR_1_HW             (FDVT_BASE_HW + 0x0b0)
#define FDVT_OUT_BASE_ADR_2_HW             (FDVT_BASE_HW + 0x0b4)
#define FDVT_OUT_BASE_ADR_3_HW             (FDVT_BASE_HW + 0x0b8)
#define FDVT_KERNEL_BASE_ADR_0_HW          (FDVT_BASE_HW + 0x0bc)
#define FDVT_KERNEL_BASE_ADR_1_HW          (FDVT_BASE_HW + 0x0c0)
#define FDVT_IN_SIZE_0_HW                  (FDVT_BASE_HW + 0x0c4)
#define FDVT_IN_STRIDE_0_HW                (FDVT_BASE_HW + 0x0c8)
#define FDVT_IN_SIZE_1_HW                  (FDVT_BASE_HW + 0x0cc)
#define FDVT_IN_STRIDE_1_HW                (FDVT_BASE_HW + 0x0d0)
#define FDVT_IN_SIZE_2_HW                  (FDVT_BASE_HW + 0x0d4)
#define FDVT_IN_STRIDE_2_HW                (FDVT_BASE_HW + 0x0d8)
#define FDVT_IN_SIZE_3_HW                  (FDVT_BASE_HW + 0x0dc)
#define FDVT_IN_STRIDE_3_HW                (FDVT_BASE_HW + 0x0e0)
#define FDVT_OUT_SIZE_0_HW                 (FDVT_BASE_HW + 0x0e4)
#define FDVT_OUT_STRIDE_0_HW               (FDVT_BASE_HW + 0x0e8)
#define FDVT_OUT_SIZE_1_HW                 (FDVT_BASE_HW + 0x0ec)
#define FDVT_OUT_STRIDE_1_HW               (FDVT_BASE_HW + 0x0f0)
#define FDVT_OUT_SIZE_2_HW                 (FDVT_BASE_HW + 0x0f4)
#define FDVT_OUT_STRIDE_2_HW               (FDVT_BASE_HW + 0x0f8)
#define FDVT_OUT_SIZE_3_HW                 (FDVT_BASE_HW + 0x0fc)
#define FDVT_OUT_STRIDE_3_HW               (FDVT_BASE_HW + 0x100)
#define FDVT_KERNEL_SIZE_HW                (FDVT_BASE_HW + 0x104)
#define FDVT_KERNEL_STRIDE_HW              (FDVT_BASE_HW + 0x108)
#define FDVT_DEBUG_INFO_0_HW               (FDVT_BASE_HW + 0x10c)
#define FDVT_DEBUG_INFO_1_HW               (FDVT_BASE_HW + 0x110)
#define FDVT_DEBUG_INFO_2_HW               (FDVT_BASE_HW + 0x114)
#define FDVT_SPARE_CELL_HW                 (FDVT_BASE_HW + 0x118)
#define FDVT_VERSION_HW                    (FDVT_BASE_HW + 0x11c)
#define FDVT_PADDING_CON0_HW               (FDVT_BASE_HW + 0x120)
#define FDVT_PADDING_CON1_HW               (FDVT_BASE_HW + 0x124)
#define FDVT_SECURE_REGISTER               (FDVT_BASE_HW + 0x13C)
#define DMA_SOFT_RSTSTAT_HW                (FDVT_BASE_HW + 0x200)
#define TDRI_BASE_ADDR_HW                  (FDVT_BASE_HW + 0x204)
#define TDRI_OFST_ADDR_HW                  (FDVT_BASE_HW + 0x208)
#define TDRI_XSIZE_HW                      (FDVT_BASE_HW + 0x20c)
#define VERTICAL_FLIP_EN_HW                (FDVT_BASE_HW + 0x210)
#define DMA_SOFT_RESET_HW                  (FDVT_BASE_HW + 0x214)
#define LAST_ULTRA_EN_HW                   (FDVT_BASE_HW + 0x218)
#define SPECIAL_FUN_EN_HW                  (FDVT_BASE_HW + 0x21c)
#define FDVT_WRA_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x230)
#define FDVT_WRA_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x238)

#define FDVT_WRB_0_YSIZE_HW                (FDVT_BASE_HW + 0x304)
#define FDVT_WRB_0_STRIDE_HW               (FDVT_BASE_HW + 0x308)
#define FDVT_WRB_0_CON_HW                  (FDVT_BASE_HW + 0x30c)
#define FDVT_WRB_0_CON2_HW                 (FDVT_BASE_HW + 0x310)
#define FDVT_WRB_0_CON3_HW                 (FDVT_BASE_HW + 0x314)
#define FDVT_WRB_0_CROP_HW                 (FDVT_BASE_HW + 0x318)
#define FDVT_WRB_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x320)
#define FDVT_WRB_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x328)
#define FDVT_WRB_1_XSIZE_HW                (FDVT_BASE_HW + 0x330)
#define FDVT_WRB_1_YSIZE_HW                (FDVT_BASE_HW + 0x334)
#define FDVT_WRB_1_STRIDE_HW               (FDVT_BASE_HW + 0x338)
#define FDVT_WRB_1_CON_HW                  (FDVT_BASE_HW + 0x33c)
#define FDVT_WRB_1_CON2_HW                 (FDVT_BASE_HW + 0x340)
#define FDVT_WRB_1_CON3_HW                 (FDVT_BASE_HW + 0x344)
#define FDVT_WRB_1_CROP_HW                 (FDVT_BASE_HW + 0x348)
#define FDVT_RDB_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x350)
#define FDVT_RDB_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x358)
#define FDVT_RDB_0_XSIZE_HW                (FDVT_BASE_HW + 0x360)
#define FDVT_RDB_0_YSIZE_HW                (FDVT_BASE_HW + 0x364)
#define FDVT_RDB_0_STRIDE_HW               (FDVT_BASE_HW + 0x368)
#define FDVT_RDB_0_CON_HW                  (FDVT_BASE_HW + 0x36c)
#define FDVT_RDB_0_CON2_HW                 (FDVT_BASE_HW + 0x370)
#define FDVT_RDB_0_CON3_HW                 (FDVT_BASE_HW + 0x374)
#define FDVT_RDB_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x380)
#define FDVT_RDB_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x388)
#define FDVT_RDB_1_XSIZE_HW                (FDVT_BASE_HW + 0x390)
#define FDVT_RDB_1_YSIZE_HW                (FDVT_BASE_HW + 0x394)
#define FDVT_RDB_1_STRIDE_HW               (FDVT_BASE_HW + 0x398)
#define FDVT_RDB_1_CON_HW                  (FDVT_BASE_HW + 0x39c)
#define FDVT_RDB_1_CON2_HW                 (FDVT_BASE_HW + 0x3a0)
#define FDVT_RDB_1_CON3_HW                 (FDVT_BASE_HW + 0x3a4)
#define DMA_ERR_CTRL_HW                    (FDVT_BASE_HW + 0x3b0)
#define FDVT_WRA_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3b4)
#define FDVT_WRA_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3b8)
#define FDVT_WRB_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3bc)
#define FDVT_WRB_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c0)
#define FDVT_RDA_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c4)
#define FDVT_RDA_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3c8)
#define FDVT_RDB_0_ERR_STAT_HW             (FDVT_BASE_HW + 0x3cc)
#define FDVT_RDB_1_ERR_STAT_HW             (FDVT_BASE_HW + 0x3d0)
#define DMA_DEBUG_ADDR_HW                  (FDVT_BASE_HW + 0x3e0)
#define DMA_RSV1_HW                        (FDVT_BASE_HW + 0x3e4)
#define DMA_RSV2_HW                        (FDVT_BASE_HW + 0x3e8)
#define DMA_RSV3_HW                        (FDVT_BASE_HW + 0x3ec)
#define DMA_RSV4_HW                        (FDVT_BASE_HW + 0x3f0)
#define DMA_DEBUG_SEL_HW                   (FDVT_BASE_HW + 0x3f4)
#define DMA_BW_SELF_TEST_HW                (FDVT_BASE_HW + 0x3f8)

/* AIE 3.0 FLD register offset */
#define FLD_IMG_BASE_ADDR		0x400
#define FLD_MS_BASE_ADDR		0x404
#define FLD_FP_BASE_ADDR		0x408
#define FLD_TR_BASE_ADDR		0x40C
#define FLD_SH_BASE_ADDR		0x410
#define FLD_CV_BASE_ADDR		0x414
#define FLD_BS_BASE_ADDR		0x418
#define FLD_PP_BASE_ADDR		0x41C
#define FLD_FP_FORT_OFST		0x420
#define FLD_TR_FORT_OFST		0x424
#define FLD_SH_FORT_OFST		0x428
#define FLD_CV_FORT_OFST		0x42C

#define FLD_FACE_0_INFO_0		0x430
#define FLD_FACE_0_INFO_1		0x434
#define FLD_FACE_1_INFO_0		0x438
#define FLD_FACE_1_INFO_1		0x43C
#define FLD_FACE_2_INFO_0		0x440
#define FLD_FACE_2_INFO_1		0x444
#define FLD_FACE_3_INFO_0		0x448
#define FLD_FACE_3_INFO_1		0x44C
#define FLD_FACE_4_INFO_0		0x450
#define FLD_FACE_4_INFO_1		0x454
#define FLD_FACE_5_INFO_0		0x458
#define FLD_FACE_5_INFO_1		0x45C
#define FLD_FACE_6_INFO_0		0x460
#define FLD_FACE_6_INFO_1		0x464
#define FLD_FACE_7_INFO_0		0x468
#define FLD_FACE_7_INFO_1		0x46C
#define FLD_FACE_8_INFO_0		0x470
#define FLD_FACE_8_INFO_1		0x474
#define FLD_FACE_9_INFO_0		0x478
#define FLD_FACE_9_INFO_1		0x47C
#define FLD_FACE_10_INFO_0		0x480
#define FLD_FACE_10_INFO_1		0x484
#define FLD_FACE_11_INFO_0		0x488
#define FLD_FACE_11_INFO_1		0x48C
#define FLD_FACE_12_INFO_0		0x490
#define FLD_FACE_12_INFO_1		0x494
#define FLD_FACE_13_INFO_0		0x498
#define FLD_FACE_13_INFO_1		0x49C
#define FLD_FACE_14_INFO_0		0x4A0
#define FLD_FACE_14_INFO_1		0x4A4

/* FLD CMDQ FACE INFO */
#define FLD_CMDQ_FACE_0_INFO_0		(FDVT_BASE_HW + 0x430)
#define FLD_CMDQ_FACE_0_INFO_1		(FDVT_BASE_HW + 0x434)
#define FLD_CMDQ_FACE_1_INFO_0		(FDVT_BASE_HW + 0x438)
#define FLD_CMDQ_FACE_1_INFO_1		(FDVT_BASE_HW + 0x43C)
#define FLD_CMDQ_FACE_2_INFO_0		(FDVT_BASE_HW + 0x440)
#define FLD_CMDQ_FACE_2_INFO_1		(FDVT_BASE_HW + 0x444)
#define FLD_CMDQ_FACE_3_INFO_0		(FDVT_BASE_HW + 0x448)
#define FLD_CMDQ_FACE_3_INFO_1		(FDVT_BASE_HW + 0x44C)
#define FLD_CMDQ_FACE_4_INFO_0		(FDVT_BASE_HW + 0x450)
#define FLD_CMDQ_FACE_4_INFO_1		(FDVT_BASE_HW + 0x454)
#define FLD_CMDQ_FACE_5_INFO_0		(FDVT_BASE_HW + 0x458)
#define FLD_CMDQ_FACE_5_INFO_1		(FDVT_BASE_HW + 0x45C)
#define FLD_CMDQ_FACE_6_INFO_0		(FDVT_BASE_HW + 0x460)
#define FLD_CMDQ_FACE_6_INFO_1		(FDVT_BASE_HW + 0x464)
#define FLD_CMDQ_FACE_7_INFO_0		(FDVT_BASE_HW + 0x468)
#define FLD_CMDQ_FACE_7_INFO_1		(FDVT_BASE_HW + 0x46C)
#define FLD_CMDQ_FACE_8_INFO_0		(FDVT_BASE_HW + 0x470)
#define FLD_CMDQ_FACE_8_INFO_1		(FDVT_BASE_HW + 0x474)
#define FLD_CMDQ_FACE_9_INFO_0		(FDVT_BASE_HW + 0x478)
#define FLD_CMDQ_FACE_9_INFO_1		(FDVT_BASE_HW + 0x47C)
#define FLD_CMDQ_FACE_10_INFO_0		(FDVT_BASE_HW + 0x480)
#define FLD_CMDQ_FACE_10_INFO_1		(FDVT_BASE_HW + 0x484)
#define FLD_CMDQ_FACE_11_INFO_0		(FDVT_BASE_HW + 0x488)
#define FLD_CMDQ_FACE_11_INFO_1		(FDVT_BASE_HW + 0x48C)
#define FLD_CMDQ_FACE_12_INFO_0		(FDVT_BASE_HW + 0x490)
#define FLD_CMDQ_FACE_12_INFO_1		(FDVT_BASE_HW + 0x494)
#define FLD_CMDQ_FACE_13_INFO_0		(FDVT_BASE_HW + 0x498)
#define FLD_CMDQ_FACE_13_INFO_1		(FDVT_BASE_HW + 0x49C)
#define FLD_CMDQ_FACE_14_INFO_0		(FDVT_BASE_HW + 0x4A0)
#define FLD_CMDQ_FACE_14_INFO_1		(FDVT_BASE_HW + 0x4A4)

#define FLD_NUM_CONFIG_0		0x4A8
#define FLD_FACE_NUM			0x4AC
#define FLD_CMDQ_FACE_NUM		(FDVT_BASE_HW + 0x4AC)

#define FLD_PCA_MEAN_SCALE_0	0x4B0
#define FLD_PCA_MEAN_SCALE_1	0x4B4
#define FLD_PCA_MEAN_SCALE_2	0x4B8
#define FLD_PCA_MEAN_SCALE_3	0x4BC
#define FLD_PCA_MEAN_SCALE_4	0x4C0
#define FLD_PCA_MEAN_SCALE_5	0x4C4
#define FLD_PCA_MEAN_SCALE_6	0x4C8
#define FLD_PCA_VEC_0			0x4CC
#define FLD_PCA_VEC_1			0x4D0
#define FLD_PCA_VEC_2			0x4D4
#define FLD_PCA_VEC_3			0x4D8
#define FLD_PCA_VEC_4			0x4DC
#define FLD_PCA_VEC_5			0x4E0
#define FLD_PCA_VEC_6			0x4E4
#define FLD_CV_BIAS_FR_0		0x4E8
#define FLD_CV_BIAS_PF_0		0x4EC
#define FLD_CV_RANGE_FR_0		0x4F0
#define FLD_CV_RANGE_FR_1		0x4F4
#define FLD_CV_RANGE_PF_0		0x4F8
#define FLD_CV_RANGE_PF_1		0x4FC
#define FLD_PP_COEF				0x500
#define FLD_SRC_SIZE			0x504
#define FLD_CMDQ_SRC_SIZE		(FDVT_BASE_HW + 0x504)
#define FLD_SRC_PITCH			0x508
#define FLD_CMDQ_SRC_PITCH		(FDVT_BASE_HW + 0x508)
#define FLD_BS_CONFIG0			0x50C
#define FLD_BS_CONFIG1			0x510
#define FLD_BS_CONFIG2			0x514
#define FLD_FLD_BUSY			0x518
#define FLD_FLD_DONE			0x51C
#define FLD_FLD_SECURE			0x520
#define FLD_FLD_SECURE_W		0x524
#define FLD_FLD_DBG_SEL			0x528
#define FLD_FLD_STM_DBG_DATA0	0x52C
#define FLD_FLD_STM_DBG_DATA1	0x530
#define FLD_FLD_STM_DBG_DATA2	0x534
#define FLD_FLD_STM_DBG_DATA3	0x538
#define FLD_FLD_SH_DBG_DATA0	0x53C
#define FLD_FLD_SH_DBG_DATA1	0x540
#define FLD_FLD_SH_DBG_DATA2	0x544
#define FLD_FLD_SH_DBG_DATA3	0x548
#define FLD_FLD_SH_DBG_DATA4	0x54C
#define FLD_FLD_SH_DBG_DATA5	0x550
#define FLD_FLD_SH_DBG_DATA6	0x554
#define FLD_FLD_SH_DBG_DATA7	0x558

#define MTK_FD_OUTPUT_MIN_WIDTH 0U
#define MTK_FD_OUTPUT_MIN_HEIGHT 0U
#define MTK_FD_OUTPUT_MAX_WIDTH 4096U
#define MTK_FD_OUTPUT_MAX_HEIGHT 4096U

#define MTK_FD_HW_TIMEOUT 1000 /* 1000 msec */
#define MAX_FACE_NUM 1024
#define RLT_NUM 48
#define GENDER_OUT 32

#define RACE_RST_X_NUM 4
#define RACE_RST_Y_NUM 64
#define GENDER_RST_X_NUM 2
#define GENDER_RST_Y_NUM 64
#define MRACE_RST_NUM 4
#define MGENDER_RST_NUM 2
#define MAGE_RST_NUM 2
#define MINDIAN_RST_NUM 2

#define POSE_LOOP_NUM 3
#define FLD_MAX_OUT 1680

extern struct mtk_aie_user_para g_user_param;

static const unsigned int fld_face_info_idx_0[FLD_MAX_INPUT] = {
	FLD_FACE_0_INFO_0, FLD_FACE_1_INFO_0, FLD_FACE_2_INFO_0,
	FLD_FACE_3_INFO_0, FLD_FACE_4_INFO_0, FLD_FACE_5_INFO_0,
	FLD_FACE_6_INFO_0, FLD_FACE_7_INFO_0, FLD_FACE_8_INFO_0,
	FLD_FACE_9_INFO_0, FLD_FACE_10_INFO_0, FLD_FACE_11_INFO_0,
	FLD_FACE_12_INFO_0, FLD_FACE_13_INFO_0, FLD_FACE_14_INFO_0
};

static const unsigned int fld_face_info_idx_1[FLD_MAX_INPUT] = {
	FLD_FACE_0_INFO_1, FLD_FACE_1_INFO_1, FLD_FACE_2_INFO_1,
	FLD_FACE_3_INFO_1, FLD_FACE_4_INFO_1, FLD_FACE_5_INFO_1,
	FLD_FACE_6_INFO_1, FLD_FACE_7_INFO_1, FLD_FACE_8_INFO_1,
	FLD_FACE_9_INFO_1, FLD_FACE_10_INFO_1, FLD_FACE_11_INFO_1,
	FLD_FACE_12_INFO_1, FLD_FACE_13_INFO_1, FLD_FACE_14_INFO_1
};

static const unsigned int fld_face_info_cmdq_idx_0[FLD_MAX_INPUT] = {
	FLD_CMDQ_FACE_0_INFO_0, FLD_CMDQ_FACE_1_INFO_0, FLD_CMDQ_FACE_2_INFO_0,
	FLD_CMDQ_FACE_3_INFO_0, FLD_CMDQ_FACE_4_INFO_0, FLD_CMDQ_FACE_5_INFO_0,
	FLD_CMDQ_FACE_6_INFO_0, FLD_CMDQ_FACE_7_INFO_0, FLD_CMDQ_FACE_8_INFO_0,
	FLD_CMDQ_FACE_9_INFO_0, FLD_CMDQ_FACE_10_INFO_0, FLD_CMDQ_FACE_11_INFO_0,
	FLD_CMDQ_FACE_12_INFO_0, FLD_CMDQ_FACE_13_INFO_0, FLD_CMDQ_FACE_14_INFO_0
};

static const unsigned int fld_face_info_cmdq_idx_1[FLD_MAX_INPUT] = {
	FLD_CMDQ_FACE_0_INFO_1, FLD_CMDQ_FACE_1_INFO_1, FLD_CMDQ_FACE_2_INFO_1,
	FLD_CMDQ_FACE_3_INFO_1, FLD_CMDQ_FACE_4_INFO_1, FLD_CMDQ_FACE_5_INFO_1,
	FLD_CMDQ_FACE_6_INFO_1, FLD_CMDQ_FACE_7_INFO_1, FLD_CMDQ_FACE_8_INFO_1,
	FLD_CMDQ_FACE_9_INFO_1, FLD_CMDQ_FACE_10_INFO_1, FLD_CMDQ_FACE_11_INFO_1,
	FLD_CMDQ_FACE_12_INFO_1, FLD_CMDQ_FACE_13_INFO_1, FLD_CMDQ_FACE_14_INFO_1
};

struct aie_static_info {
	unsigned int fd_wdma_size[fd_loop_num][output_WDMA_WRA_num];
	unsigned int out_xsize_plus_1[fd_loop_num];
	unsigned int out_height[fd_loop_num];
	unsigned int out_ysize_plus_1_stride2[fd_loop_num];
	unsigned int out_stride[fd_loop_num];
	unsigned int out_stride_stride2[fd_loop_num];
	unsigned int out_width[fd_loop_num];
	unsigned int img_width[fd_loop_num];
	unsigned int img_height[fd_loop_num];
	unsigned int stride2_out_width[fd_loop_num];
	unsigned int stride2_out_height[fd_loop_num];
	unsigned int out_xsize_plus_1_stride2[fd_loop_num];
	unsigned int input_xsize_plus_1[fd_loop_num];
};

enum aie_state {
	STATE_NA = 0,
	STATE_INIT = 1
};

enum aie_mode { FDMODE = 0, ATTRIBUTEMODE = 1, POSEMODE = 2, FLDMODE = 3 };
enum FLDROP { NORMAL = 0, RIGHT = 1, LEFT = 2 };
enum FLDRIP { FLD_0 = 0, FLD_1 = 1, FLD_2 = 2, FLD_3 = 3, FLD_4 = 4, FLD_5 = 5, FLD_6 = 6,
	FLD_7 = 7, FLD_8 = 8, FLD_9 = 9, FLD_10 = 10, FLD_11 = 11};


enum aie_format {
	FMT_NA = 0,
	FMT_YUV_2P = 1,
	FMT_YVU_2P = 2,
	FMT_YUYV = 3,
	FMT_YVYU = 4,
	FMT_UYVY = 5,
	FMT_VYUY = 6,
	FMT_MONO = 7,
	FMT_YUV420_2P = 8,
	FMT_YUV420_1P = 9
};

enum aie_input_degree {
	DEGREE_0 = 0,
	DEGREE_90 = 1,
	DEGREE_270 = 2,
	DEGREE_180 = 3
};

struct aie_init_info {
	u16 max_img_width;
	u16 max_img_height;
	s16 feature_threshold;
	u16 pyramid_width;
	u16 pyramid_height;
	u32 is_secure;
	u32 sec_mem_type;
};

/* align v4l2 user space interface */
struct fd_result {
	u16 fd_pyramid0_num;
	u16 fd_pyramid1_num;
	u16 fd_pyramid2_num;
	u16 fd_total_num;
	u8 rpn31_rlt[MAX_FACE_NUM][RLT_NUM];
	u8 rpn63_rlt[MAX_FACE_NUM][RLT_NUM];
	u8 rpn95_rlt[MAX_FACE_NUM][RLT_NUM];
};

/* align v4l2 user space interface */
struct attr_result {
	u8 rpn17_rlt[GENDER_OUT];
	u8 rpn20_rlt[GENDER_OUT];
	u8 rpn22_rlt[GENDER_OUT];
	u8 rpn25_rlt[GENDER_OUT];
};

struct aie_roi {
	u32 x1;
	u32 y1;
	u32 x2;
	u32 y2;
};

struct aie_padding {
	u32 left;
	u32 right;
	u32 down;
	u32 up;
};

struct FLD_LANDMARK {
	u16 x;
	u16 y;
};

struct FLD_RESULT {
	struct FLD_LANDMARK fld_landmark[FLD_MAX_INPUT];
	s16 fld_out_rip;
	s16 fld_out_rop;
	u16 confidence;
	s16 blinkscore;
};

struct FLD_CROP_RIP_ROP {
	struct aie_roi fld_in_crop;
	enum FLDRIP fld_in_rip;
	enum FLDROP fld_in_rop;
};

/* align v4l2 user space interface: FdDrv_output_struct */
struct aie_enq_info {
	u32 sel_mode;
	u32 src_img_fmt;
	u16 src_img_width;
	u16 src_img_height;
	u16 src_img_stride;
	u32 pyramid_base_width;
	u32 pyramid_base_height;
	u32 number_of_pyramid;
	u32 rotate_degree;
	u32 en_roi;
	struct aie_roi src_roi;
	u32 en_padding;
	struct aie_padding src_padding;
	u32 freq_level;
	u32 src_img_addr;
	u32 src_img_addr_uv;
	u32 fd_version;
	u32 attr_version;
	u32 pose_version;
	struct fd_result fd_out;
	struct attr_result attr_out;
	u32 fld_face_num;
	struct FLD_CROP_RIP_ROP fld_input[FLD_MAX_INPUT];
	unsigned char fld_raw_out[FLD_MAX_OUT]; //fld output buf
	struct FLD_RESULT fld_output[FLD_MAX_INPUT]; //fld output parsing data
};

struct aie_reg_cfg {
	u32 rs_adr;
	u32 yuv2rgb_adr;
	u32 fd_adr;
	u32 fd_pose_adr;
	u32 fd_mode;
	u32 hw_result;
	u32 hw_result1;
};

struct aie_para {
	u32 sel_mode;
	u16 max_img_width;
	u16 max_img_height;
	u16 img_width;
	u16 img_height;
	u16 crop_width;
	u16 crop_height;
	u32 src_img_fmt;
	u32 rotate_degree;
	s16 rpn_anchor_thrd;
	u16 pyramid_width;
	u16 pyramid_height;
	u16 max_pyramid_width;
	u16 max_pyramid_height;
	u16 number_of_pyramid;
	u32 src_img_addr;
	u32 src_img_addr_uv;

	void *fd_fd_cfg_va;
	void *fd_rs_cfg_va;
	void *fd_yuv2rgb_cfg_va;
	void *fd_fd_pose_cfg_va;

	void *attr_fd_cfg_va[MAX_ENQUE_FRAME_NUM];
	void *attr_yuv2rgb_cfg_va[MAX_ENQUE_FRAME_NUM];

	void *rs_pym_rst_va[PYM_NUM][COLOR_NUM];

	dma_addr_t fd_fd_cfg_pa;
	dma_addr_t fd_rs_cfg_pa;
	dma_addr_t fd_yuv2rgb_cfg_pa;
	dma_addr_t fd_fd_pose_cfg_pa;

	dma_addr_t attr_fd_cfg_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t attr_yuv2rgb_cfg_pa[MAX_ENQUE_FRAME_NUM];

	dma_addr_t rs_pym_rst_pa[PYM_NUM][COLOR_NUM];
};


struct aie_fld_para {
	u32 sel_mode;
	u16 img_width;
	u16 img_height;
	u32 face_num;
	u32 src_img_addr;
	struct FLD_CROP_RIP_ROP fld_input[FLD_MAX_INPUT];

	void *fld_output_va;
	dma_addr_t fld_output_pa;
};

struct aie_attr_para {
	u32 w_idx;
	u32 r_idx;
	u32 sel_mode[MAX_ENQUE_FRAME_NUM];
	u16 img_width[MAX_ENQUE_FRAME_NUM];
	u16 img_height[MAX_ENQUE_FRAME_NUM];
	u16 crop_width[MAX_ENQUE_FRAME_NUM];
	u16 crop_height[MAX_ENQUE_FRAME_NUM];
	u32 src_img_fmt[MAX_ENQUE_FRAME_NUM];
	u32 rotate_degree[MAX_ENQUE_FRAME_NUM];
	u32 src_img_addr[MAX_ENQUE_FRAME_NUM];
	u32 src_img_addr_uv[MAX_ENQUE_FRAME_NUM];
};

struct aie_fd_dma_para {
	void *fd_out_hw_va[fd_loop_num][output_WDMA_WRA_num];
	void *fd_kernel_va[fd_loop_num][kernel_RDMA_RA_num];
	void *attr_out_hw_va[attr_loop_num][output_WDMA_WRA_num];
	void *attr_kernel_va[attr_loop_num][kernel_RDMA_RA_num];

	void *age_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *gender_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *isIndian_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *race_out_hw_va[MAX_ENQUE_FRAME_NUM];

	void *fd_pose_out_hw_va[POSE_LOOP_NUM][output_WDMA_WRA_num];

	/* HW FLD Buffer Pointer for arrange*/
	void *fld_cv_va[FLD_MAX_INPUT];
	void *fld_leafnode_va[FLD_MAX_INPUT];
	void *fld_fp_va[FLD_MAX_INPUT];
	void *fld_tree02_va[FLD_MAX_INPUT];
	void *fld_shape_va[FLD_MAX_INPUT];
	void *fld_blink_weight_va;
	void *fld_output_va;

	dma_addr_t fld_blink_weight_pa;
	dma_addr_t fld_cv_pa[FLD_MAX_INPUT];
	dma_addr_t fld_leafnode_pa[FLD_MAX_INPUT];
	dma_addr_t fld_fp_pa[FLD_MAX_INPUT];
	dma_addr_t fld_tree02_pa[FLD_MAX_INPUT];
	dma_addr_t fld_shape_pa[FLD_MAX_INPUT];
	dma_addr_t fld_output_pa;

	dma_addr_t fd_out_hw_pa[fd_loop_num][output_WDMA_WRA_num];
	dma_addr_t fd_kernel_pa[fd_loop_num][kernel_RDMA_RA_num];
	dma_addr_t attr_out_hw_pa[attr_loop_num][output_WDMA_WRA_num];
	dma_addr_t attr_kernel_pa[attr_loop_num][kernel_RDMA_RA_num];

	dma_addr_t age_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t gender_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t isIndian_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t race_out_hw_pa[MAX_ENQUE_FRAME_NUM];

	dma_addr_t fd_pose_out_hw_pa[POSE_LOOP_NUM][output_WDMA_WRA_num];
};

struct imem_buf_info {
	void *va;
	dma_addr_t pa;
	struct dma_buf_map map;
	unsigned int size;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
};

struct fd_buffer {
	__u32 dma_addr; /* used by DMA HW */
} __packed;

struct user_param {
	unsigned int fd_mode;
	unsigned int src_img_fmt;
	unsigned int src_img_width;
	unsigned int src_img_height;
	unsigned int src_img_stride;
	unsigned int pyramid_base_width;
	unsigned int pyramid_base_height;
	unsigned int number_of_pyramid;
	unsigned int rotate_degree;
	int en_roi;
	unsigned int src_roi_x1;
	unsigned int src_roi_y1;
	unsigned int src_roi_x2;
	unsigned int src_roi_y2;
	int en_padding;
	unsigned int src_padding_left;
	unsigned int src_padding_right;
	unsigned int src_padding_down;
	unsigned int src_padding_up;
	unsigned int freq_level;
	unsigned int fld_face_num;
	struct FLD_CROP_RIP_ROP fld_input[15];
} __packed;

struct mtk_aie_user_para {
	signed int feature_threshold;
	unsigned int is_secure;
	unsigned int sec_mem_type;
	unsigned int max_img_width;
	unsigned int max_img_height;
	unsigned int pyramid_width;
	unsigned int pyramid_height;
	struct user_param user_param;
} __packed;

struct fd_enq_param {
	struct fd_buffer src_img[2];
	struct fd_buffer user_result;
	struct user_param user_param;
} __packed;

struct mtk_aie_dvfs {
	struct device *dev;
	struct regulator *reg;
	unsigned int clklv_num[MTK_AIE_OPP_SET];
	unsigned int clklv[MTK_AIE_OPP_SET][MTK_AIE_CLK_LEVEL_CNT];
	unsigned int voltlv[MTK_AIE_OPP_SET][MTK_AIE_CLK_LEVEL_CNT];
	unsigned int clklv_idx[MTK_AIE_OPP_SET];
	unsigned int clklv_target[MTK_AIE_OPP_SET];
	unsigned int cur_volt;
};

struct mtk_aie_qos_path {
	struct icc_path *path;	/* cmdq event enum value */
	char dts_name[256];
	unsigned long long bw;
};

struct mtk_aie_qos {
	struct device *dev;
	struct mtk_aie_qos_path *qos_path;
};

struct mtk_aie_req_work {
	struct work_struct work;
	struct mtk_aie_dev *fd_dev;
};

struct ipesys_aie_clocks {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
};

struct mtk_aie_drv_ops {
	void (*reset)(struct mtk_aie_dev *fd);
	int (*alloc_buf)(struct mtk_aie_dev *fd);
	int (*init)(struct mtk_aie_dev *fd);
	void (*uninit)(struct mtk_aie_dev *fd);
	int (*prepare)(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
	void (*execute)(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
	void (*get_fd_result)(struct mtk_aie_dev *fd,
			struct aie_enq_info *aie_cfg);
	void (*get_attr_result)(struct mtk_aie_dev *fd,
			struct aie_enq_info *aie_cfg);
	void (*get_fld_result)(struct mtk_aie_dev *fd,
			struct aie_enq_info *aie_cfg);
	void (*irq_handle)(struct mtk_aie_dev *fd);
	void (*config_fld_buf_reg)(struct mtk_aie_dev *fd);
	void (*fdvt_dump_reg)(struct mtk_aie_dev *fd);
};

struct mtk_aie_dev {
	struct device *dev;
	struct platform_device *img_pdev;
	struct platform_device *aov_pdev;
	struct ipesys_aie_clocks aie_clk;
	struct cmdq_client *fdvt_clt;
	struct cmdq_client *fdvt_secure_clt;
	s32 fdvt_event_id;
	struct mtk_aie_ctx *ctx;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct media_device mdev;
	struct video_device vfd;
	struct device *larb;

	/* Lock for V4L2 operations */
	struct mutex vfd_lock;

	void __iomem *fd_base;

	u32 fdvt_sec_set;
	u32 fdvt_sec_wait;
	u32 fd_stream_count;
	struct completion fd_job_finished;
	struct delayed_work job_timeout_work;

	struct aie_enq_info *aie_cfg;
	struct aie_reg_cfg reg_cfg;

	/* Input Buffer Pointer */
	struct imem_buf_info rs_cfg_data;
	struct imem_buf_info fd_cfg_data;
	struct imem_buf_info pose_cfg_data;
	struct imem_buf_info yuv2rgb_cfg_data;
	/* HW Output Buffer Pointer */
	struct imem_buf_info rs_output_hw;
	struct imem_buf_info fd_dma_hw;
	struct imem_buf_info fd_dma_result_hw;
	struct imem_buf_info fd_kernel_hw;
	struct imem_buf_info fd_attr_dma_hw;

	/* HW FLD Buffer Pointer for allocate memory*/
	struct imem_buf_info fld_cv_hw;
	struct imem_buf_info fld_fp_hw;
	struct imem_buf_info fld_leafnode_hw;
	struct imem_buf_info fld_tree_02_hw;
	struct imem_buf_info fld_shape_hw;
	struct imem_buf_info fld_blink_weight_hw;
	struct imem_buf_info fld_output_hw;

	/* Image information */
	unsigned long long img_y;
	unsigned long long img_uv;
	unsigned int img_msb_y;
	unsigned int img_msb_uv;

	/* DRAM Buffer Size */
	unsigned int fd_rs_cfg_size;
	unsigned int fd_fd_cfg_data_size;
	unsigned int fd_fd_cfg_aligned_size;
	unsigned int fd_yuv2rgb_cfg_size;
	unsigned int fd_yuv2rgb_cfg_aligned_size;
	unsigned int fd_pose_cfg_size;
	unsigned int attr_fd_cfg_data_size;
	unsigned int attr_fd_cfg_aligned_size;
	unsigned int attr_yuv2rgb_cfg_data_size;
	unsigned int attr_yuv2rgb_cfg_aligned_size;

	/* HW Output Buffer Size */
	unsigned int rs_pym_out_size[PYM_NUM];
	unsigned int fd_dma_max_size;
	unsigned int fd_dma_rst_max_size;
	unsigned int fd_fd_kernel_size;
	unsigned int fd_attr_kernel_size;
	unsigned int fd_attr_dma_max_size;
	unsigned int fd_attr_dma_rst_max_size;

	unsigned int pose_height;

	/*DMA Buffer*/
	struct dma_buf *dmabuf;
	unsigned long long kva;
	struct dma_buf_map map;
	int map_count;

	struct aie_para *base_para;
	struct aie_attr_para *attr_para;
	struct aie_fd_dma_para *dma_para;
	struct aie_fld_para *fld_para;

	struct aie_static_info st_info;
	unsigned int fd_state;
	struct mtk_aie_dvfs dvfs_info;
	struct mtk_aie_qos qos_info;

	wait_queue_head_t flushing_waitq;
	atomic_t num_composing;

	/* AIE driver operation */
	const struct mtk_aie_drv_ops *drv_ops;

	struct workqueue_struct *frame_done_wq;
	struct mtk_aie_req_work req_work;
};

struct mtk_aie_ctx {
	struct mtk_aie_dev *fd_dev;
	struct device *dev;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_pix_format_mplane src_fmt;
	struct v4l2_meta_format dst_fmt;
};

extern const struct mtk_aie_drv_ops aie_ops_isp71;
extern const struct mtk_aie_drv_ops aie_ops_isp7s;

/**************************************************************************/
/*                   C L A S S    D E C L A R A T I O N                   */
/**************************************************************************/

void config_aie_cmdq_secure_init(struct mtk_aie_dev *fd);
void aie_enable_secure_domain(struct mtk_aie_dev *fd);
void aie_disable_secure_domain(struct mtk_aie_dev *fd);
extern int mtk_aov_notify(struct platform_device *pdev, unsigned int notify, unsigned int status);
#endif /*__MTK_AIE_H__*/
