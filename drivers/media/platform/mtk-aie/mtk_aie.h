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
#define FDVT_WRA_0_XSIZE_HW                (FDVT_BASE_HW + 0x240)
#define FDVT_WRA_0_YSIZE_HW                (FDVT_BASE_HW + 0x244)
#define FDVT_WRA_0_STRIDE_HW               (FDVT_BASE_HW + 0x248)
#define FDVT_WRA_0_CON_HW                  (FDVT_BASE_HW + 0x24c)
#define FDVT_WRA_0_CON2_HW                 (FDVT_BASE_HW + 0x250)
#define FDVT_WRA_0_CON3_HW                 (FDVT_BASE_HW + 0x254)
#define FDVT_WRA_0_CROP_HW                 (FDVT_BASE_HW + 0x258)
#define FDVT_WRA_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x260)
#define FDVT_WRA_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x268)
#define FDVT_WRA_1_XSIZE_HW                (FDVT_BASE_HW + 0x270)
#define FDVT_WRA_1_YSIZE_HW                (FDVT_BASE_HW + 0x274)
#define FDVT_WRA_1_STRIDE_HW               (FDVT_BASE_HW + 0x278)
#define FDVT_WRA_1_CON_HW                  (FDVT_BASE_HW + 0x27c)
#define FDVT_WRA_1_CON2_HW                 (FDVT_BASE_HW + 0x280)
#define FDVT_WRA_1_CON3_HW                 (FDVT_BASE_HW + 0x284)
#define FDVT_WRA_1_CROP_HW                 (FDVT_BASE_HW + 0x288)
#define FDVT_RDA_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x290)
#define FDVT_RDA_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x298)
#define FDVT_RDA_0_XSIZE_HW                (FDVT_BASE_HW + 0x2a0)
#define FDVT_RDA_0_YSIZE_HW                (FDVT_BASE_HW + 0x2a4)
#define FDVT_RDA_0_STRIDE_HW               (FDVT_BASE_HW + 0x2a8)
#define FDVT_RDA_0_CON_HW                  (FDVT_BASE_HW + 0x2ac)
#define FDVT_RDA_0_CON2_HW                 (FDVT_BASE_HW + 0x2b0)
#define FDVT_RDA_0_CON3_HW                 (FDVT_BASE_HW + 0x2b4)
#define FDVT_RDA_1_BASE_ADDR_HW            (FDVT_BASE_HW + 0x2c0)
#define FDVT_RDA_1_OFST_ADDR_HW            (FDVT_BASE_HW + 0x2c8)
#define FDVT_RDA_1_XSIZE_HW                (FDVT_BASE_HW + 0x2d0)
#define FDVT_RDA_1_YSIZE_HW                (FDVT_BASE_HW + 0x2d4)
#define FDVT_RDA_1_STRIDE_HW               (FDVT_BASE_HW + 0x2d8)
#define FDVT_RDA_1_CON_HW                  (FDVT_BASE_HW + 0x2dc)
#define FDVT_RDA_1_CON2_HW                 (FDVT_BASE_HW + 0x2e0)
#define FDVT_RDA_1_CON3_HW                 (FDVT_BASE_HW + 0x2e4)
#define FDVT_WRB_0_BASE_ADDR_HW            (FDVT_BASE_HW + 0x2f0)
#define FDVT_WRB_0_OFST_ADDR_HW            (FDVT_BASE_HW + 0x2f8)
#define FDVT_WRB_0_XSIZE_HW                (FDVT_BASE_HW + 0x300)
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
#define FLD_EN                       0x400
#define FLD_BASE_ADDR_FACE_0         0x404
#define FLD_BASE_ADDR_FACE_1         0x408
#define FLD_BASE_ADDR_FACE_2         0x40C
#define FLD_BASE_ADDR_FACE_3         0x410
#define FLD_BASE_ADDR_FACE_4         0x414
#define FLD_BASE_ADDR_FACE_5         0x418
#define FLD_BASE_ADDR_FACE_6         0x41C
#define FLD_BASE_ADDR_FACE_7         0x420
#define FLD_BASE_ADDR_FACE_8         0x424
#define FLD_BASE_ADDR_FACE_9         0x428
#define FLD_BASE_ADDR_FACE_10        0x42C
#define FLD_BASE_ADDR_FACE_11        0x430
#define FLD_BASE_ADDR_FACE_12        0x434
#define FLD_BASE_ADDR_FACE_13        0x438
#define FLD_BASE_ADDR_FACE_14        0x43C

#define FLD_INFO_0_FACE_0            0x440
#define FLD_INFO_1_FACE_0            0x444
#define FLD_INFO_2_FACE_0            0x448
#define FLD_INFO_0_FACE_1            0x44C
#define FLD_INFO_1_FACE_1            0x450
#define FLD_INFO_2_FACE_1            0x454
#define FLD_INFO_0_FACE_2            0x458
#define FLD_INFO_1_FACE_2            0x45C
#define FLD_INFO_2_FACE_2            0x460
#define FLD_INFO_0_FACE_3            0x464
#define FLD_INFO_1_FACE_3            0x468
#define FLD_INFO_2_FACE_3            0x46C
#define FLD_INFO_0_FACE_4            0x470
#define FLD_INFO_1_FACE_4            0x474
#define FLD_INFO_2_FACE_4            0x478
#define FLD_INFO_0_FACE_5            0x47C
#define FLD_INFO_1_FACE_5            0x480
#define FLD_INFO_2_FACE_5            0x484
#define FLD_INFO_0_FACE_6            0x488
#define FLD_INFO_1_FACE_6            0x48C
#define FLD_INFO_2_FACE_6            0x490
#define FLD_INFO_0_FACE_7            0x494
#define FLD_INFO_1_FACE_7            0x498

#define FLD_INFO_2_FACE_7            0x4A0
#define FLD_INFO_0_FACE_8            0x4A4
#define FLD_INFO_1_FACE_8            0x4A8
#define FLD_INFO_2_FACE_8            0x4AC
#define FLD_INFO_0_FACE_9            0x4B0
#define FLD_INFO_1_FACE_9            0x4B4
#define FLD_INFO_2_FACE_9            0x4B8
#define FLD_INFO_0_FACE_10           0x4BC
#define FLD_INFO_1_FACE_10           0x4C0
#define FLD_INFO_2_FACE_10           0x4C4
#define FLD_INFO_0_FACE_11           0x4C8
#define FLD_INFO_1_FACE_11           0x4CC
#define FLD_INFO_2_FACE_11           0x4D0
#define FLD_INFO_0_FACE_12           0x4D4
#define FLD_INFO_1_FACE_12           0x4D8
#define FLD_INFO_2_FACE_12           0x4DC
#define FLD_INFO_0_FACE_13           0x4E0
#define FLD_INFO_1_FACE_13           0x4E4
#define FLD_INFO_2_FACE_13           0x4E8
#define FLD_INFO_0_FACE_14           0x4EC
#define FLD_INFO_1_FACE_14           0x4F0
#define FLD_INFO_2_FACE_14           0x4F4

#define FLD_MODEL_PARA0              0x4F8
#define FLD_MODEL_PARA1              0x4FC
#define FLD_MODEL_PARA2              0x500
#define FLD_MODEL_PARA3              0x504
#define FLD_MODEL_PARA4              0x508
#define FLD_MODEL_PARA5              0x50C
#define FLD_MODEL_PARA6              0x510
#define FLD_MODEL_PARA7              0x514
#define FLD_MODEL_PARA8              0x518
#define FLD_MODEL_PARA9              0x51C
#define FLD_MODEL_PARA10             0x520
#define FLD_MODEL_PARA11             0x524
#define FLD_MODEL_PARA12             0x528
#define FLD_MODEL_PARA13             0x52C
#define FLD_MODEL_PARA14             0x530
#define FLD_MODEL_PARA15             0x534
#define FLD_MODEL_PARA16             0x538
#define FLD_DEBUG_INFO0              0x53C
#define FLD_DEBUG_INFO1              0x540

#define FLD_BUSY                     0x544
#define FLD_DONE                     0x548
#define FLD_SRC_WD_HT                0x54C

#define FLD_PL_IN_BASE_ADDR_0_0      0x550
#define FLD_PL_IN_BASE_ADDR_0_1      0x554
#define FLD_PL_IN_BASE_ADDR_0_2      0x558
#define FLD_PL_IN_BASE_ADDR_0_3      0x55C
#define FLD_PL_IN_BASE_ADDR_0_4      0x560
#define FLD_PL_IN_BASE_ADDR_0_5      0x564
#define FLD_PL_IN_BASE_ADDR_0_6      0x568
#define FLD_PL_IN_BASE_ADDR_0_7      0x56C
#define FLD_PL_IN_BASE_ADDR_0_8      0x570
#define FLD_PL_IN_BASE_ADDR_0_9      0x574
#define FLD_PL_IN_BASE_ADDR_0_10     0x578
#define FLD_PL_IN_BASE_ADDR_0_11     0x57C
#define FLD_PL_IN_BASE_ADDR_0_12     0x580
#define FLD_PL_IN_BASE_ADDR_0_13     0x584
#define FLD_PL_IN_BASE_ADDR_0_14     0x588
#define FLD_PL_IN_BASE_ADDR_0_15     0x58C
#define FLD_PL_IN_BASE_ADDR_0_16     0x590
#define FLD_PL_IN_BASE_ADDR_0_17     0x594
#define FLD_PL_IN_BASE_ADDR_0_18     0x598
#define FLD_PL_IN_BASE_ADDR_0_19     0x59C
#define FLD_PL_IN_BASE_ADDR_0_20     0x5A0
#define FLD_PL_IN_BASE_ADDR_0_21     0x5A4
#define FLD_PL_IN_BASE_ADDR_0_22     0x5A8
#define FLD_PL_IN_BASE_ADDR_0_23     0x5AC
#define FLD_PL_IN_BASE_ADDR_0_24     0x5B0
#define FLD_PL_IN_BASE_ADDR_0_25     0x5B4
#define FLD_PL_IN_BASE_ADDR_0_26     0x5B8
#define FLD_PL_IN_BASE_ADDR_0_27     0x5BC
#define FLD_PL_IN_BASE_ADDR_0_28     0x5C0
#define FLD_PL_IN_BASE_ADDR_0_29     0x5C4

#define FLD_PL_IN_BASE_ADDR_1_0      0x5C8
#define FLD_PL_IN_BASE_ADDR_1_1      0x5CC
#define FLD_PL_IN_BASE_ADDR_1_2      0x5D0
#define FLD_PL_IN_BASE_ADDR_1_3      0x5D4
#define FLD_PL_IN_BASE_ADDR_1_4      0x5D8
#define FLD_PL_IN_BASE_ADDR_1_5      0x5DC
#define FLD_PL_IN_BASE_ADDR_1_6      0x5E0
#define FLD_PL_IN_BASE_ADDR_1_7      0x5E4
#define FLD_PL_IN_BASE_ADDR_1_8      0x5E8
#define FLD_PL_IN_BASE_ADDR_1_9      0x5EC
#define FLD_PL_IN_BASE_ADDR_1_10     0x5F0
#define FLD_PL_IN_BASE_ADDR_1_11     0x5F4
#define FLD_PL_IN_BASE_ADDR_1_12     0x5F8
#define FLD_PL_IN_BASE_ADDR_1_13     0x5FC
#define FLD_PL_IN_BASE_ADDR_1_14     0x600
#define FLD_PL_IN_BASE_ADDR_1_15     0x604
#define FLD_PL_IN_BASE_ADDR_1_16     0x608
#define FLD_PL_IN_BASE_ADDR_1_17     0x60C
#define FLD_PL_IN_BASE_ADDR_1_18     0x610
#define FLD_PL_IN_BASE_ADDR_1_19     0x614
#define FLD_PL_IN_BASE_ADDR_1_20     0x618
#define FLD_PL_IN_BASE_ADDR_1_21     0x61C
#define FLD_PL_IN_BASE_ADDR_1_22     0x620
#define FLD_PL_IN_BASE_ADDR_1_23     0x624
#define FLD_PL_IN_BASE_ADDR_1_24     0x628
#define FLD_PL_IN_BASE_ADDR_1_25     0x62C
#define FLD_PL_IN_BASE_ADDR_1_26     0x630
#define FLD_PL_IN_BASE_ADDR_1_27     0x634
#define FLD_PL_IN_BASE_ADDR_1_28     0x638
#define FLD_PL_IN_BASE_ADDR_1_29     0x63C

#define FLD_PL_IN_BASE_ADDR_2_0      0x640
#define FLD_PL_IN_BASE_ADDR_2_1      0x644
#define FLD_PL_IN_BASE_ADDR_2_2      0x648
#define FLD_PL_IN_BASE_ADDR_2_3      0x64C
#define FLD_PL_IN_BASE_ADDR_2_4      0x650
#define FLD_PL_IN_BASE_ADDR_2_5      0x654
#define FLD_PL_IN_BASE_ADDR_2_6      0x658
#define FLD_PL_IN_BASE_ADDR_2_7      0x65C
#define FLD_PL_IN_BASE_ADDR_2_8      0x660
#define FLD_PL_IN_BASE_ADDR_2_9      0x664
#define FLD_PL_IN_BASE_ADDR_2_10     0x668
#define FLD_PL_IN_BASE_ADDR_2_11     0x66C
#define FLD_PL_IN_BASE_ADDR_2_12     0x670
#define FLD_PL_IN_BASE_ADDR_2_13     0x674
#define FLD_PL_IN_BASE_ADDR_2_14     0x678
#define FLD_PL_IN_BASE_ADDR_2_15     0x67C
#define FLD_PL_IN_BASE_ADDR_2_16     0x680
#define FLD_PL_IN_BASE_ADDR_2_17     0x684
#define FLD_PL_IN_BASE_ADDR_2_18     0x688
#define FLD_PL_IN_BASE_ADDR_2_19     0x68C
#define FLD_PL_IN_BASE_ADDR_2_20     0x690
#define FLD_PL_IN_BASE_ADDR_2_21     0x694
#define FLD_PL_IN_BASE_ADDR_2_22     0x698
#define FLD_PL_IN_BASE_ADDR_2_23     0x69C
#define FLD_PL_IN_BASE_ADDR_2_24     0x6A0
#define FLD_PL_IN_BASE_ADDR_2_25     0x6A4
#define FLD_PL_IN_BASE_ADDR_2_26     0x6A8
#define FLD_PL_IN_BASE_ADDR_2_27     0x6AC
#define FLD_PL_IN_BASE_ADDR_2_28     0x6B0
#define FLD_PL_IN_BASE_ADDR_2_29     0x6B4

#define FLD_PL_IN_BASE_ADDR_3_0      0x6B8
#define FLD_PL_IN_BASE_ADDR_3_1      0x6BC
#define FLD_PL_IN_BASE_ADDR_3_2      0x6C0
#define FLD_PL_IN_BASE_ADDR_3_3      0x6C4
#define FLD_PL_IN_BASE_ADDR_3_4      0x6C8
#define FLD_PL_IN_BASE_ADDR_3_5      0x6CC
#define FLD_PL_IN_BASE_ADDR_3_6      0x6D0
#define FLD_PL_IN_BASE_ADDR_3_7      0x6D4
#define FLD_PL_IN_BASE_ADDR_3_8      0x6D8
#define FLD_PL_IN_BASE_ADDR_3_9      0x6DC
#define FLD_PL_IN_BASE_ADDR_3_10     0x6E0
#define FLD_PL_IN_BASE_ADDR_3_11     0x6E4
#define FLD_PL_IN_BASE_ADDR_3_12     0x6E8
#define FLD_PL_IN_BASE_ADDR_3_13     0x6EC
#define FLD_PL_IN_BASE_ADDR_3_14     0x6F0
#define FLD_PL_IN_BASE_ADDR_3_15     0x6F4
#define FLD_PL_IN_BASE_ADDR_3_16     0x6F8
#define FLD_PL_IN_BASE_ADDR_3_17     0x6FC
#define FLD_PL_IN_BASE_ADDR_3_18     0x700
#define FLD_PL_IN_BASE_ADDR_3_19     0x704
#define FLD_PL_IN_BASE_ADDR_3_20     0x708
#define FLD_PL_IN_BASE_ADDR_3_21     0x70C
#define FLD_PL_IN_BASE_ADDR_3_22     0x710
#define FLD_PL_IN_BASE_ADDR_3_23     0x714
#define FLD_PL_IN_BASE_ADDR_3_24     0x718
#define FLD_PL_IN_BASE_ADDR_3_25     0x71C
#define FLD_PL_IN_BASE_ADDR_3_26     0x720
#define FLD_PL_IN_BASE_ADDR_3_27     0x724
#define FLD_PL_IN_BASE_ADDR_3_28     0x728
#define FLD_PL_IN_BASE_ADDR_3_29     0x72C

#define FLD_PL_IN_SIZE_0             0x730
#define FLD_PL_IN_STRIDE_0           0x734
#define FLD_PL_IN_SIZE_1             0x738
#define FLD_PL_IN_STRIDE_1           0x73C
#define FLD_PL_IN_SIZE_2_0           0x740
#define FLD_PL_IN_STRIDE_2_0         0x744
#define FLD_PL_IN_SIZE_2_1           0x748
#define FLD_PL_IN_STRIDE_2_1         0x74C
#define FLD_PL_IN_SIZE_2_2           0x750
#define FLD_PL_IN_STRIDE_2_2         0x754
#define FLD_PL_IN_SIZE_3             0x758
#define FLD_PL_IN_STRIDE_3           0x75C

#define FLD_SH_IN_BASE_ADDR_0        0x760
#define FLD_SH_IN_BASE_ADDR_1        0x764
#define FLD_SH_IN_BASE_ADDR_2        0x768
#define FLD_SH_IN_BASE_ADDR_3        0x76C
#define FLD_SH_IN_BASE_ADDR_4        0x770
#define FLD_SH_IN_BASE_ADDR_5        0x774
#define FLD_SH_IN_BASE_ADDR_6        0x778
#define FLD_SH_IN_BASE_ADDR_7        0x77C
#define FLD_SH_IN_BASE_ADDR_8        0x780
#define FLD_SH_IN_BASE_ADDR_9        0x784
#define FLD_SH_IN_BASE_ADDR_10       0x788
#define FLD_SH_IN_BASE_ADDR_11       0x78C
#define FLD_SH_IN_BASE_ADDR_12       0x790
#define FLD_SH_IN_BASE_ADDR_13       0x794
#define FLD_SH_IN_BASE_ADDR_14       0x798
#define FLD_SH_IN_BASE_ADDR_15       0x79C
#define FLD_SH_IN_BASE_ADDR_16       0x7A0
#define FLD_SH_IN_BASE_ADDR_17       0x7A4
#define FLD_SH_IN_BASE_ADDR_18       0x7A8
#define FLD_SH_IN_BASE_ADDR_19       0x7AC
#define FLD_SH_IN_BASE_ADDR_20       0x7B0
#define FLD_SH_IN_BASE_ADDR_21       0x7B4
#define FLD_SH_IN_BASE_ADDR_22       0x7B8
#define FLD_SH_IN_BASE_ADDR_23       0x7BC
#define FLD_SH_IN_BASE_ADDR_24       0x7C0
#define FLD_SH_IN_BASE_ADDR_25       0x7C4
#define FLD_SH_IN_BASE_ADDR_26       0x7C8
#define FLD_SH_IN_BASE_ADDR_27       0x7CC
#define FLD_SH_IN_BASE_ADDR_28       0x7D0
#define FLD_SH_IN_BASE_ADDR_29       0x7D4

#define FLD_SH_IN_SIZE_0             0x7D8
#define FLD_SH_IN_STRIDE_0           0x7DC
#define FLD_TR_OUT_BASE_ADDR_0       0x7E0
#define FLD_TR_OUT_SIZE_0            0x7E4
#define FLD_TR_OUT_STRIDE_0          0x7E8
#define FLD_PP_OUT_BASE_ADDR_0       0x7EC
#define FLD_PP_OUT_SIZE_0            0x7F0
#define FLD_PP_OUT_STRIDE_0          0x7F4
#define FLD_SPARE                    0x7F8

#define FLD_BASE_ADDR_FACE_0_7_MSB 0x7FC
#define FLD_BASE_ADDR_FACE_8_14_MSB 0x800

#define FLD_PL_IN_BASE_ADDR_0_0_7_MSB 0x804
#define FLD_PL_IN_BASE_ADDR_0_8_15_MSB 0x808
#define FLD_PL_IN_BASE_ADDR_0_16_23_MSB 0x80C
#define FLD_PL_IN_BASE_ADDR_0_24_29_MSB 0x810

#define FLD_PL_IN_BASE_ADDR_1_0_7_MSB 0x814
#define FLD_PL_IN_BASE_ADDR_1_8_15_MSB 0x818
#define FLD_PL_IN_BASE_ADDR_1_16_23_MSB 0x81C
#define FLD_PL_IN_BASE_ADDR_1_24_29_MSB 0x820

#define FLD_PL_IN_BASE_ADDR_2_0_7_MSB 0x824
#define FLD_PL_IN_BASE_ADDR_2_8_15_MSB 0x828
#define FLD_PL_IN_BASE_ADDR_2_16_23_MSB 0x82C
#define FLD_PL_IN_BASE_ADDR_2_24_29_MSB 0x830

#define FLD_PL_IN_BASE_ADDR_3_0_7_MSB 0x834
#define FLD_PL_IN_BASE_ADDR_3_8_15_MSB 0x838
#define FLD_PL_IN_BASE_ADDR_3_16_23_MSB 0x83C
#define FLD_PL_IN_BASE_ADDR_3_24_29_MSB 0x840

#define FLD_SH_IN_BASE_ADDR_0_7_MSB 0x844
#define FLD_SH_IN_BASE_ADDR_8_15_MSB 0x848
#define FLD_SH_IN_BASE_ADDR_16_23_MSB 0x84C
#define FLD_SH_IN_BASE_ADDR_24_29_MSB 0x850

#define FLD_BS_IN_BASE_ADDR_0_7_MSB 0x8d4
#define FLD_BS_IN_BASE_ADDR_8_15_MSB 0x8d8

#define FLD_TR_OUT_BASE_ADDR_0_MSB 0x854
#define FLD_PP_OUT_BASE_ADDR_0_MSB 0x858

#define FLD_BS_IN_BASE_ADDR_00       0x85C
#define FLD_BS_IN_BASE_ADDR_01       0x860
#define FLD_BS_IN_BASE_ADDR_02       0x864
#define FLD_BS_IN_BASE_ADDR_03       0x868
#define FLD_BS_IN_BASE_ADDR_04       0x86C
#define FLD_BS_IN_BASE_ADDR_05       0x870
#define FLD_BS_IN_BASE_ADDR_06       0x874
#define FLD_BS_IN_BASE_ADDR_07       0x878
#define FLD_BS_IN_BASE_ADDR_08       0x87C
#define FLD_BS_IN_BASE_ADDR_09       0x880
#define FLD_BS_IN_BASE_ADDR_10       0x884
#define FLD_BS_IN_BASE_ADDR_11       0x888
#define FLD_BS_IN_BASE_ADDR_12       0x88C
#define FLD_BS_IN_BASE_ADDR_13       0x890
#define FLD_BS_IN_BASE_ADDR_14       0x894
#define FLD_BS_BIAS                  0x8E4
#define FLD_CV_FM_RANGE_0            0x8E8
#define FLD_CV_FM_RANGE_1            0x8EC
#define FLD_CV_PM_RANGE_0            0x8F0
#define FLD_CV_PM_RANGE_1            0x8F4
#define FLD_BS_RANGE_0               0x8F8
#define FLD_BS_RANGE_1               0x8FC

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

static const unsigned int fld_face_info_0[FLD_MAX_INPUT] = {
	FLD_INFO_0_FACE_0, FLD_INFO_0_FACE_1, FLD_INFO_0_FACE_2,
	FLD_INFO_0_FACE_3, FLD_INFO_0_FACE_4, FLD_INFO_0_FACE_5,
	FLD_INFO_0_FACE_6, FLD_INFO_0_FACE_7, FLD_INFO_0_FACE_8,
	FLD_INFO_0_FACE_9, FLD_INFO_0_FACE_10, FLD_INFO_0_FACE_11,
	FLD_INFO_0_FACE_12, FLD_INFO_0_FACE_13, FLD_INFO_0_FACE_14
};

static const unsigned int fld_face_info_1[FLD_MAX_INPUT] = {
	FLD_INFO_1_FACE_0, FLD_INFO_1_FACE_1, FLD_INFO_1_FACE_2,
	FLD_INFO_1_FACE_3, FLD_INFO_1_FACE_4, FLD_INFO_1_FACE_5,
	FLD_INFO_1_FACE_6, FLD_INFO_1_FACE_7, FLD_INFO_1_FACE_8,
	FLD_INFO_1_FACE_9, FLD_INFO_1_FACE_10, FLD_INFO_1_FACE_11,
	FLD_INFO_1_FACE_12, FLD_INFO_1_FACE_13, FLD_INFO_1_FACE_14
};

static const unsigned int fld_face_info_2[FLD_MAX_INPUT] = {
	FLD_INFO_2_FACE_0, FLD_INFO_2_FACE_1, FLD_INFO_2_FACE_2,
	FLD_INFO_2_FACE_3, FLD_INFO_2_FACE_4, FLD_INFO_2_FACE_5,
	FLD_INFO_2_FACE_6, FLD_INFO_2_FACE_7, FLD_INFO_2_FACE_8,
	FLD_INFO_2_FACE_9, FLD_INFO_2_FACE_10, FLD_INFO_2_FACE_11,
	FLD_INFO_2_FACE_12, FLD_INFO_2_FACE_13, FLD_INFO_2_FACE_14
};

static const unsigned int fld_pl_in_addr_0[FLD_MAX_INPUT] = {
	FLD_PL_IN_BASE_ADDR_0_0, FLD_PL_IN_BASE_ADDR_0_1, FLD_PL_IN_BASE_ADDR_0_2,
	FLD_PL_IN_BASE_ADDR_0_3, FLD_PL_IN_BASE_ADDR_0_4, FLD_PL_IN_BASE_ADDR_0_5,
	FLD_PL_IN_BASE_ADDR_0_6, FLD_PL_IN_BASE_ADDR_0_7, FLD_PL_IN_BASE_ADDR_0_8,
	FLD_PL_IN_BASE_ADDR_0_9, FLD_PL_IN_BASE_ADDR_0_10, FLD_PL_IN_BASE_ADDR_0_11,
	FLD_PL_IN_BASE_ADDR_0_12, FLD_PL_IN_BASE_ADDR_0_13, FLD_PL_IN_BASE_ADDR_0_14
};

static const unsigned int fld_pl_in_addr_1[FLD_MAX_INPUT] = {
	FLD_PL_IN_BASE_ADDR_1_0, FLD_PL_IN_BASE_ADDR_1_1, FLD_PL_IN_BASE_ADDR_1_2,
	FLD_PL_IN_BASE_ADDR_1_3, FLD_PL_IN_BASE_ADDR_1_4, FLD_PL_IN_BASE_ADDR_1_5,
	FLD_PL_IN_BASE_ADDR_1_6, FLD_PL_IN_BASE_ADDR_1_7, FLD_PL_IN_BASE_ADDR_1_8,
	FLD_PL_IN_BASE_ADDR_1_9, FLD_PL_IN_BASE_ADDR_1_10, FLD_PL_IN_BASE_ADDR_1_11,
	FLD_PL_IN_BASE_ADDR_1_12, FLD_PL_IN_BASE_ADDR_1_13, FLD_PL_IN_BASE_ADDR_1_14
};

static const unsigned int fld_pl_in_addr_2[FLD_MAX_INPUT] = {
	FLD_PL_IN_BASE_ADDR_2_0, FLD_PL_IN_BASE_ADDR_2_1, FLD_PL_IN_BASE_ADDR_2_2,
	FLD_PL_IN_BASE_ADDR_2_3, FLD_PL_IN_BASE_ADDR_2_4, FLD_PL_IN_BASE_ADDR_2_5,
	FLD_PL_IN_BASE_ADDR_2_6, FLD_PL_IN_BASE_ADDR_2_7, FLD_PL_IN_BASE_ADDR_2_8,
	FLD_PL_IN_BASE_ADDR_2_9, FLD_PL_IN_BASE_ADDR_2_10, FLD_PL_IN_BASE_ADDR_2_11,
	FLD_PL_IN_BASE_ADDR_2_12, FLD_PL_IN_BASE_ADDR_2_13, FLD_PL_IN_BASE_ADDR_2_14
};

static const unsigned int fld_pl_in_addr_3[FLD_MAX_INPUT] = {
	FLD_PL_IN_BASE_ADDR_3_0, FLD_PL_IN_BASE_ADDR_3_1, FLD_PL_IN_BASE_ADDR_3_2,
	FLD_PL_IN_BASE_ADDR_3_3, FLD_PL_IN_BASE_ADDR_3_4, FLD_PL_IN_BASE_ADDR_3_5,
	FLD_PL_IN_BASE_ADDR_3_6, FLD_PL_IN_BASE_ADDR_3_7, FLD_PL_IN_BASE_ADDR_3_8,
	FLD_PL_IN_BASE_ADDR_3_9, FLD_PL_IN_BASE_ADDR_3_10, FLD_PL_IN_BASE_ADDR_3_11,
	FLD_PL_IN_BASE_ADDR_3_12, FLD_PL_IN_BASE_ADDR_3_13, FLD_PL_IN_BASE_ADDR_3_14
};

static const unsigned int fld_sh_in_addr[FLD_MAX_INPUT] = {
	FLD_SH_IN_BASE_ADDR_0, FLD_SH_IN_BASE_ADDR_1, FLD_SH_IN_BASE_ADDR_2,
	FLD_SH_IN_BASE_ADDR_3, FLD_SH_IN_BASE_ADDR_4, FLD_SH_IN_BASE_ADDR_5,
	FLD_SH_IN_BASE_ADDR_6, FLD_SH_IN_BASE_ADDR_7, FLD_SH_IN_BASE_ADDR_8,
	FLD_SH_IN_BASE_ADDR_9, FLD_SH_IN_BASE_ADDR_10, FLD_SH_IN_BASE_ADDR_11,
	FLD_SH_IN_BASE_ADDR_12, FLD_SH_IN_BASE_ADDR_13, FLD_SH_IN_BASE_ADDR_14
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

enum aie_mode { FDMODE = 0, ATTRIBUTEMODE = 1, POSEMODE = 2 };
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
	void *fld_tree13_va[FLD_MAX_INPUT];
	void *fld_blink_weight_va;
	void *fld_output_va;

	dma_addr_t fld_blink_weight_pa;
	dma_addr_t fld_cv_pa[FLD_MAX_INPUT];
	dma_addr_t fld_leafnode_pa[FLD_MAX_INPUT];
	dma_addr_t fld_fp_pa[FLD_MAX_INPUT];
	dma_addr_t fld_tree02_pa[FLD_MAX_INPUT];
	dma_addr_t fld_tree13_pa[FLD_MAX_INPUT];
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

struct mtk_aie_dev {
	struct device *dev;
	struct platform_device *img_pdev;
	struct ipesys_aie_clocks aie_clk;
	struct cmdq_client *fdvt_clt;
	struct cmdq_client *fdvt_secure_clt;
	s32 fdvt_event_id;
	struct mtk_aie_ctx *ctx;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct media_device mdev;
	struct video_device vfd;
	struct clk *vcore_gals;
	struct clk *main_gals;
	struct clk *img_ipe;
	struct clk *ipe_fdvt;
	struct clk *ipe_fdvt1;
	struct clk *ipe_smi_larb12;
	struct clk *ipe_top;
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
	struct imem_buf_info fld_tree_13_hw;
	struct imem_buf_info fld_blink_weight_hw;
	struct imem_buf_info fld_output_hw;

	/*MSB*/
	unsigned int img_msb_y;
	unsigned int img_msb_uv;

	/* DRAM Buffer Size */
	unsigned int fd_rs_cfg_size;
	unsigned int fd_fd_cfg_size;
	unsigned int fd_yuv2rgb_cfg_size;
	unsigned int fd_pose_cfg_size;
	unsigned int attr_fd_cfg_size;
	unsigned int attr_yuv2rgb_cfg_size;

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

/**************************************************************************/
/*                   C L A S S    D E C L A R A T I O N                   */
/**************************************************************************/

void aie_reset(struct mtk_aie_dev *fd);
int aie_alloc_aie_buf(struct mtk_aie_dev *fd);
int aie_init(struct mtk_aie_dev *fd);
void aie_uninit(struct mtk_aie_dev *fd);
int aie_prepare(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_execute(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_execute_pose(struct mtk_aie_dev *fd);
void aie_irqhandle(struct mtk_aie_dev *fd);
void config_aie_cmdq_hw(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void config_aie_cmdq_secure_init(struct mtk_aie_dev *fd);
void aie_enable_secure_domain(struct mtk_aie_dev *fd);
void aie_disable_secure_domain(struct mtk_aie_dev *fd);
void aie_get_fd_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_get_attr_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_get_fld_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
struct dma_buf *aie_imem_sec_alloc(struct mtk_aie_dev *fd, u32 size, bool IsSecure);
unsigned long long aie_get_sec_iova(struct mtk_aie_dev *fd, struct dma_buf *my_dma_buf,
					struct imem_buf_info *bufinfo);
void *aie_get_va(struct mtk_aie_dev *fd, struct dma_buf *my_dma_buf);

#endif /*__MTK_AIE_H__*/
