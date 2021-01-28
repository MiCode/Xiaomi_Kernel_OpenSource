/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6768-reg.h  --  Mediatek 6768 audio driver reg definition
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MT6768_REG_H_
#define _MT6768_REG_H_

/* reg bit enum */
enum {
	MT6768_MEMIF_PBUF_SIZE_32_BYTES,
	MT6768_MEMIF_PBUF_SIZE_64_BYTES,
	MT6768_MEMIF_PBUF_SIZE_128_BYTES,
	MT6768_MEMIF_PBUF_SIZE_256_BYTES,
	MT6768_MEMIF_PBUF_SIZE_NUM,
};

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
/* AFE_DAC_CON0 */
#define AWB2_ON_SFT                                   29
#define AWB2_ON_MASK                                  0x1
#define AWB2_ON_MASK_SFT                              (0x1 << 29)
#define VUL2_ON_SFT                                   27
#define VUL2_ON_MASK                                  0x1
#define VUL2_ON_MASK_SFT                              (0x1 << 27)
#define MOD_DAI_DUP_WR_SFT                            26
#define MOD_DAI_DUP_WR_MASK                           0x1
#define MOD_DAI_DUP_WR_MASK_SFT                       (0x1 << 26)
#define VUL_DATA2_MODE_SFT                            20
#define VUL_DATA2_MODE_MASK                           0xf
#define VUL_DATA2_MODE_MASK_SFT                       (0xf << 20)
#define DL1_DATA2_MODE_SFT                            16
#define DL1_DATA2_MODE_MASK                           0xf
#define DL1_DATA2_MODE_MASK_SFT                       (0xf << 16)
#define VUL_DATA2_R_MONO_SFT                          11
#define VUL_DATA2_R_MONO_MASK                         0x1
#define VUL_DATA2_R_MONO_MASK_SFT                     (0x1 << 11)
#define VUL_DATA2_DATA_SFT                            10
#define VUL_DATA2_DATA_MASK                           0x1
#define VUL_DATA2_DATA_MASK_SFT                       (0x1 << 10)
#define VUL_DATA2_ON_SFT                              9
#define VUL_DATA2_ON_MASK                             0x1
#define VUL_DATA2_ON_MASK_SFT                         (0x1 << 9)
#define DL12_ON_SFT                                   8
#define DL12_ON_MASK                                  0x1
#define DL12_ON_MASK_SFT                              (0x1 << 8)
#define MOD_DAI_ON_SFT                                7
#define MOD_DAI_ON_MASK                               0x1
#define MOD_DAI_ON_MASK_SFT                           (0x1 << 7)
#define AWB_ON_SFT                                    6
#define AWB_ON_MASK                                   0x1
#define AWB_ON_MASK_SFT                               (0x1 << 6)
#define DL3_ON_SFT                                    5
#define DL3_ON_MASK                                   0x1
#define DL3_ON_MASK_SFT                               (0x1 << 5)
#define VUL_ON_SFT                                    3
#define VUL_ON_MASK                                   0x1
#define VUL_ON_MASK_SFT                               (0x1 << 3)
#define DL2_ON_SFT                                    2
#define DL2_ON_MASK                                   0x1
#define DL2_ON_MASK_SFT                               (0x1 << 2)
#define DL1_ON_SFT                                    1
#define DL1_ON_MASK                                   0x1
#define DL1_ON_MASK_SFT                               (0x1 << 1)
#define AFE_ON_SFT                                    0
#define AFE_ON_MASK                                   0x1
#define AFE_ON_MASK_SFT                               (0x1 << 0)

/* AFE_DAC_CON1 */
#define MOD_DAI_MODE_SFT                              30
#define MOD_DAI_MODE_MASK                             0x3
#define MOD_DAI_MODE_MASK_SFT                         (0x3 << 30)
#define VUL_R_MONO_SFT                                28
#define VUL_R_MONO_MASK                               0x1
#define VUL_R_MONO_MASK_SFT                           (0x1 << 28)
#define VUL_DATA_SFT                                  27
#define VUL_DATA_MASK                                 0x1
#define VUL_DATA_MASK_SFT                             (0x1 << 27)
#define AXI_2X1_CG_DISABLE_SFT                        26
#define AXI_2X1_CG_DISABLE_MASK                       0x1
#define AXI_2X1_CG_DISABLE_MASK_SFT                   (0x1 << 26)
#define AWB_R_MONO_SFT                                25
#define AWB_R_MONO_MASK                               0x1
#define AWB_R_MONO_MASK_SFT                           (0x1 << 25)
#define AWB_DATA_SFT                                  24
#define AWB_DATA_MASK                                 0x1
#define AWB_DATA_MASK_SFT                             (0x1 << 24)
#define DL3_DATA_SFT                                  23
#define DL3_DATA_MASK                                 0x1
#define DL3_DATA_MASK_SFT                             (0x1 << 23)
#define DL2_DATA_SFT                                  22
#define DL2_DATA_MASK                                 0x1
#define DL2_DATA_MASK_SFT                             (0x1 << 22)
#define DL1_DATA_SFT                                  21
#define DL1_DATA_MASK                                 0x1
#define DL1_DATA_MASK_SFT                             (0x1 << 21)
#define DL1_DATA2_DATA_SFT                            20
#define DL1_DATA2_DATA_MASK                           0x1
#define DL1_DATA2_DATA_MASK_SFT                       (0x1 << 20)
#define VUL_MODE_SFT                                  16
#define VUL_MODE_MASK                                 0xf
#define VUL_MODE_MASK_SFT                             (0xf << 16)
#define AWB_MODE_SFT                                  12
#define AWB_MODE_MASK                                 0xf
#define AWB_MODE_MASK_SFT                             (0xf << 12)
#define I2S_MODE_SFT                                  8
#define I2S_MODE_MASK                                 0xf
#define I2S_MODE_MASK_SFT                             (0xf << 8)
#define DL2_MODE_SFT                                  4
#define DL2_MODE_MASK                                 0xf
#define DL2_MODE_MASK_SFT                             (0xf << 4)
#define DL1_MODE_SFT                                  0
#define DL1_MODE_MASK                                 0xf
#define DL1_MODE_MASK_SFT                             (0xf << 0)

/* AFE_DAC_CON2 */
#define AWB2_R_MONO_SFT                               21
#define AWB2_R_MONO_MASK                              0x1
#define AWB2_R_MONO_MASK_SFT                          (0x1 << 21)
#define AWB2_DATA_SFT                                 20
#define AWB2_DATA_MASK                                0x1
#define AWB2_DATA_MASK_SFT                            (0x1 << 20)
#define AWB2_MODE_SFT                                 16
#define AWB2_MODE_MASK                                0xf
#define AWB2_MODE_MASK_SFT                            (0xf << 16)
#define DL3_MODE_SFT                                  8
#define DL3_MODE_MASK                                 0xf
#define DL3_MODE_MASK_SFT                             (0xf << 8)
#define VUL2_MODE_SFT                                 4
#define VUL2_MODE_MASK                                0xf
#define VUL2_MODE_MASK_SFT                            (0xf << 4)
#define VUL2_R_MONO_SFT                               1
#define VUL2_R_MONO_MASK                              0x1
#define VUL2_R_MONO_MASK_SFT                          (0x1 << 1)
#define VUL2_DATA_SFT                                 0
#define VUL2_DATA_MASK                                0x1
#define VUL2_DATA_MASK_SFT                            (0x1 << 0)

/* AFE_DAC_MON */
#define AFE_ON_RETM_SFT                               0
#define AFE_ON_RETM_MASK                              0x1
#define AFE_ON_RETM_MASK_SFT                          (0x1 << 0)

/* AFE_I2S_CON */
#define BCK_NEG_EG_LATCH_SFT                          30
#define BCK_NEG_EG_LATCH_MASK                         0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                     (0x1 << 30)
#define BCK_INV_SFT                                   29
#define BCK_INV_MASK                                  0x1
#define BCK_INV_MASK_SFT                              (0x1 << 29)
#define I2SIN_PAD_SEL_SFT                             28
#define I2SIN_PAD_SEL_MASK                            0x1
#define I2SIN_PAD_SEL_MASK_SFT                        (0x1 << 28)
#define I2S_LOOPBACK_SFT                              20
#define I2S_LOOPBACK_MASK                             0x1
#define I2S_LOOPBACK_MASK_SFT                         (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT             17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK            0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT        (0x1 << 17)
#define I2S1_HD_EN_SFT                                12
#define I2S1_HD_EN_MASK                               0x1
#define I2S1_HD_EN_MASK_SFT                           (0x1 << 12)
#define INV_PAD_CTRL_SFT                              7
#define INV_PAD_CTRL_MASK                             0x1
#define INV_PAD_CTRL_MASK_SFT                         (0x1 << 7)
#define I2S_BYPSRC_SFT                                6
#define I2S_BYPSRC_MASK                               0x1
#define I2S_BYPSRC_MASK_SFT                           (0x1 << 6)
#define INV_LRCK_SFT                                  5
#define INV_LRCK_MASK                                 0x1
#define INV_LRCK_MASK_SFT                             (0x1 << 5)
#define I2S_FMT_SFT                                   3
#define I2S_FMT_MASK                                  0x1
#define I2S_FMT_MASK_SFT                              (0x1 << 3)
#define I2S_SRC_SFT                                   2
#define I2S_SRC_MASK                                  0x1
#define I2S_SRC_MASK_SFT                              (0x1 << 2)
#define I2S_WLEN_SFT                                  1
#define I2S_WLEN_MASK                                 0x1
#define I2S_WLEN_MASK_SFT                             (0x1 << 1)
#define I2S_EN_SFT                                    0
#define I2S_EN_MASK                                   0x1
#define I2S_EN_MASK_SFT                               (0x1 << 0)

/* AFE_I2S_CON1 */
#define I2S2_LR_SWAP_SFT                              31
#define I2S2_LR_SWAP_MASK                             0x1
#define I2S2_LR_SWAP_MASK_SFT                         (0x1 << 31)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT             17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK            0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT        (0x1 << 17)
#define I2S2_SEL_O03_O04_SFT                          16
#define I2S2_SEL_O03_O04_MASK                         0x1
#define I2S2_SEL_O03_O04_MASK_SFT                     (0x1 << 16)
#define I2S2_HD_EN_SFT                                12
#define I2S2_HD_EN_MASK                               0x1
#define I2S2_HD_EN_MASK_SFT                           (0x1 << 12)
#define I2S2_OUT_MODE_SFT                             8
#define I2S2_OUT_MODE_MASK                            0xf
#define I2S2_OUT_MODE_MASK_SFT                        (0xf << 8)
#define INV_LRCK_SFT                                  5
#define INV_LRCK_MASK                                 0x1
#define INV_LRCK_MASK_SFT                             (0x1 << 5)
#define I2S2_FMT_SFT                                  3
#define I2S2_FMT_MASK                                 0x1
#define I2S2_FMT_MASK_SFT                             (0x1 << 3)
#define I2S2_WLEN_SFT                                 1
#define I2S2_WLEN_MASK                                0x1
#define I2S2_WLEN_MASK_SFT                            (0x1 << 1)
#define I2S2_EN_SFT                                   0
#define I2S2_EN_MASK                                  0x1
#define I2S2_EN_MASK_SFT                              (0x1 << 0)

/* AFE_I2S_CON2 */
#define I2S3_LR_SWAP_SFT                              31
#define I2S3_LR_SWAP_MASK                             0x1
#define I2S3_LR_SWAP_MASK_SFT                         (0x1 << 31)
#define I2S3_UPDATE_WORD_SFT                          24
#define I2S3_UPDATE_WORD_MASK                         0x1f
#define I2S3_UPDATE_WORD_MASK_SFT                     (0x1f << 24)
#define I2S3_BCK_INV_SFT                              23
#define I2S3_BCK_INV_MASK                             0x1
#define I2S3_BCK_INV_MASK_SFT                         (0x1 << 23)
#define I2S3_FPGA_BIT_TEST_SFT                        22
#define I2S3_FPGA_BIT_TEST_MASK                       0x1
#define I2S3_FPGA_BIT_TEST_MASK_SFT                   (0x1 << 22)
#define I2S3_FPGA_BIT_SFT                             21
#define I2S3_FPGA_BIT_MASK                            0x1
#define I2S3_FPGA_BIT_MASK_SFT                        (0x1 << 21)
#define I2S3_LOOPBACK_SFT                             20
#define I2S3_LOOPBACK_MASK                            0x1
#define I2S3_LOOPBACK_MASK_SFT                        (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT             17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK            0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT        (0x1 << 17)
#define I2S3_HD_EN_SFT                                12
#define I2S3_HD_EN_MASK                               0x1
#define I2S3_HD_EN_MASK_SFT                           (0x1 << 12)
#define I2S3_OUT_MODE_SFT                             8
#define I2S3_OUT_MODE_MASK                            0xf
#define I2S3_OUT_MODE_MASK_SFT                        (0xf << 8)
#define INV_LRCK_SFT                                  5
#define INV_LRCK_MASK                                 0x1
#define INV_LRCK_MASK_SFT                             (0x1 << 5)
#define I2S3_FMT_SFT                                  3
#define I2S3_FMT_MASK                                 0x1
#define I2S3_FMT_MASK_SFT                             (0x1 << 3)
#define I2S3_WLEN_SFT                                 1
#define I2S3_WLEN_MASK                                0x1
#define I2S3_WLEN_MASK_SFT                            (0x1 << 1)
#define I2S3_EN_SFT                                   0
#define I2S3_EN_MASK                                  0x1
#define I2S3_EN_MASK_SFT                              (0x1 << 0)

/* AFE_I2S_CON3 */
#define I2S4_LR_SWAP_SFT                              31
#define I2S4_LR_SWAP_MASK                             0x1
#define I2S4_LR_SWAP_MASK_SFT                         (0x1 << 31)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT             17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK            0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT        (0x1 << 17)
#define I2S4_HD_EN_SFT                                12
#define I2S4_HD_EN_MASK                               0x1
#define I2S4_HD_EN_MASK_SFT                           (0x1 << 12)
#define I2S4_OUT_MODE_SFT                             8
#define I2S4_OUT_MODE_MASK                            0xf
#define I2S4_OUT_MODE_MASK_SFT                        (0xf << 8)
#define INV_LRCK_SFT                                  5
#define INV_LRCK_MASK                                 0x1
#define INV_LRCK_MASK_SFT                             (0x1 << 5)
#define I2S4_FMT_SFT                                  3
#define I2S4_FMT_MASK                                 0x1
#define I2S4_FMT_MASK_SFT                             (0x1 << 3)
#define I2S4_WLEN_SFT                                 1
#define I2S4_WLEN_MASK                                0x1
#define I2S4_WLEN_MASK_SFT                            (0x1 << 1)
#define I2S4_EN_SFT                                   0
#define I2S4_EN_MASK                                  0x1
#define I2S4_EN_MASK_SFT                              (0x1 << 0)

/* AFE_CONNSYS_I2S_CON */
#define BCK_NEG_EG_LATCH_SFT                          30
#define BCK_NEG_EG_LATCH_MASK                         0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                     (0x1 << 30)
#define BCK_INV_SFT                                   29
#define BCK_INV_MASK                                  0x1
#define BCK_INV_MASK_SFT                              (0x1 << 29)
#define I2SIN_PAD_SEL_SFT                             28
#define I2SIN_PAD_SEL_MASK                            0x1
#define I2SIN_PAD_SEL_MASK_SFT                        (0x1 << 28)
#define I2S_LOOPBACK_SFT                              20
#define I2S_LOOPBACK_MASK                             0x1
#define I2S_LOOPBACK_MASK_SFT                         (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT             17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK            0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT        (0x1 << 17)
#define I2S_MODE_SFT                                  8
#define I2S_MODE_MASK                                 0xf
#define I2S_MODE_MASK_SFT                             (0xf << 8)
#define INV_PAD_CTRL_SFT                              7
#define INV_PAD_CTRL_MASK                             0x1
#define INV_PAD_CTRL_MASK_SFT                         (0x1 << 7)
#define I2S_BYPSRC_SFT                                6
#define I2S_BYPSRC_MASK                               0x1
#define I2S_BYPSRC_MASK_SFT                           (0x1 << 6)
#define INV_LRCK_SFT                                  5
#define INV_LRCK_MASK                                 0x1
#define INV_LRCK_MASK_SFT                             (0x1 << 5)
#define I2S_FMT_SFT                                   3
#define I2S_FMT_MASK                                  0x1
#define I2S_FMT_MASK_SFT                              (0x1 << 3)
#define I2S_SRC_SFT                                   2
#define I2S_SRC_MASK                                  0x1
#define I2S_SRC_MASK_SFT                              (0x1 << 2)
#define I2S_WLEN_SFT                                  1
#define I2S_WLEN_MASK                                 0x1
#define I2S_WLEN_MASK_SFT                             (0x1 << 1)
#define I2S_EN_SFT                                    0
#define I2S_EN_MASK                                   0x1
#define I2S_EN_MASK_SFT                               (0x1 << 0)

/* AFE_ASRC_2CH_CON2 */
#define CHSET_O16BIT_SFT                              19
#define CHSET_O16BIT_MASK                             0x1
#define CHSET_O16BIT_MASK_SFT                         (0x1 << 19)
#define CHSET_CLR_IIR_HISTORY_SFT                     17
#define CHSET_CLR_IIR_HISTORY_MASK                    0x1
#define CHSET_CLR_IIR_HISTORY_MASK_SFT                (0x1 << 17)
#define CHSET_IS_MONO_SFT                             16
#define CHSET_IS_MONO_MASK                            0x1
#define CHSET_IS_MONO_MASK_SFT                        (0x1 << 16)
#define CHSET_IIR_EN_SFT                              11
#define CHSET_IIR_EN_MASK                             0x1
#define CHSET_IIR_EN_MASK_SFT                         (0x1 << 11)
#define CHSET_IIR_STAGE_SFT                           8
#define CHSET_IIR_STAGE_MASK                          0x7
#define CHSET_IIR_STAGE_MASK_SFT                      (0x7 << 8)
#define CHSET_STR_CLR_SFT                             5
#define CHSET_STR_CLR_MASK                            0x1
#define CHSET_STR_CLR_MASK_SFT                        (0x1 << 5)
#define CHSET_ON_SFT                                  2
#define CHSET_ON_MASK                                 0x1
#define CHSET_ON_MASK_SFT                             (0x1 << 2)
#define COEFF_SRAM_CTRL_SFT                           1
#define COEFF_SRAM_CTRL_MASK                          0x1
#define COEFF_SRAM_CTRL_MASK_SFT                      (0x1 << 1)
#define ASM_ON_SFT                                    0
#define ASM_ON_MASK                                   0x1
#define ASM_ON_MASK_SFT                               (0x1 << 0)

/* AFE_GAIN1_CON0 */
#define GAIN1_SAMPLE_PER_STEP_SFT                     8
#define GAIN1_SAMPLE_PER_STEP_MASK                    0xff
#define GAIN1_SAMPLE_PER_STEP_MASK_SFT                (0xff << 8)
#define GAIN1_MODE_SFT                                4
#define GAIN1_MODE_MASK                               0xf
#define GAIN1_MODE_MASK_SFT                           (0xf << 4)
#define GAIN1_ON_SFT                                  0
#define GAIN1_ON_MASK                                 0x1
#define GAIN1_ON_MASK_SFT                             (0x1 << 0)

/* AFE_GAIN1_CON1 */
#define GAIN1_TARGET_SFT                              0
#define GAIN1_TARGET_MASK                             0xfffff
#define GAIN1_TARGET_MASK_SFT                         (0xfffff << 0)

/* AFE_GAIN2_CON0 */
#define GAIN2_SAMPLE_PER_STEP_SFT                     8
#define GAIN2_SAMPLE_PER_STEP_MASK                    0xff
#define GAIN2_SAMPLE_PER_STEP_MASK_SFT                (0xff << 8)
#define GAIN2_MODE_SFT                                4
#define GAIN2_MODE_MASK                               0xf
#define GAIN2_MODE_MASK_SFT                           (0xf << 4)
#define GAIN2_ON_SFT                                  0
#define GAIN2_ON_MASK                                 0x1
#define GAIN2_ON_MASK_SFT                             (0x1 << 0)

/* AFE_GAIN2_CON1 */
#define GAIN2_TARGET_SFT                              0
#define GAIN2_TARGET_MASK                             0xfffff
#define GAIN2_TARGET_MASK_SFT                         (0xfffff << 0)

/* AFE_GAIN1_CUR */
#define AFE_GAIN1_CUR_SFT                             0
#define AFE_GAIN1_CUR_MASK                            0xfffff
#define AFE_GAIN1_CUR_MASK_SFT                        (0xfffff << 0)

/* AFE_GAIN2_CUR */
#define AFE_GAIN2_CUR_SFT                             0
#define AFE_GAIN2_CUR_MASK                            0xfffff
#define AFE_GAIN2_CUR_MASK_SFT                        (0xfffff << 0)

/* AFE_MEMIF_HD_MODE */
#define AWB2_HD_SFT                                   28
#define AWB2_HD_MASK                                  0x3
#define AWB2_HD_MASK_SFT                              (0x3 << 28)
#define MOD_DAI_HD_SFT                                18
#define MOD_DAI_HD_MASK                               0x3
#define MOD_DAI_HD_MASK_SFT                           (0x3 << 18)
#define VUL2_HD_SFT                                   14
#define VUL2_HD_MASK                                  0x3
#define VUL2_HD_MASK_SFT                              (0x3 << 14)
#define VUL12_HD_SFT                                  12
#define VUL12_HD_MASK                                 0x3
#define VUL12_HD_MASK_SFT                             (0x3 << 12)
#define VUL_HD_SFT                                    10
#define VUL_HD_MASK                                   0x3
#define VUL_HD_MASK_SFT                               (0x3 << 10)
#define AWB_HD_SFT                                    8
#define AWB_HD_MASK                                   0x3
#define AWB_HD_MASK_SFT                               (0x3 << 8)
#define DL3_HD_SFT                                    6
#define DL3_HD_MASK                                   0x3
#define DL3_HD_MASK_SFT                               (0x3 << 6)
#define DL2_HD_SFT                                    4
#define DL2_HD_MASK                                   0x3
#define DL2_HD_MASK_SFT                               (0x3 << 4)
#define DL12_HD_SFT                                   2
#define DL12_HD_MASK                                  0x3
#define DL12_HD_MASK_SFT                              (0x3 << 2)
#define DL1_HD_SFT                                    0
#define DL1_HD_MASK                                   0x3
#define DL1_HD_MASK_SFT                               (0x3 << 0)

/* AFE_MEMIF_HDALIGN */
#define AWB2_NORMAL_MODE_SFT                          30
#define AWB2_NORMAL_MODE_MASK                         0x1
#define AWB2_NORMAL_MODE_MASK_SFT                     (0x1 << 30)
#define MOD_DAI_NORMAL_MODE_SFT                       25
#define MOD_DAI_NORMAL_MODE_MASK                      0x1
#define MOD_DAI_NORMAL_MODE_MASK_SFT                  (0x1 << 25)
#define DAI_NORMAL_MODE_SFT                           24
#define DAI_NORMAL_MODE_MASK                          0x1
#define DAI_NORMAL_MODE_MASK_SFT                      (0x1 << 24)
#define VUL2_NORMAL_MODE_SFT                          23
#define VUL2_NORMAL_MODE_MASK                         0x1
#define VUL2_NORMAL_MODE_MASK_SFT                     (0x1 << 23)
#define VUL12_NORMAL_MODE_SFT                         22
#define VUL12_NORMAL_MODE_MASK                        0x1
#define VUL12_NORMAL_MODE_MASK_SFT                    (0x1 << 22)
#define VUL_NORMAL_MODE_SFT                           21
#define VUL_NORMAL_MODE_MASK                          0x1
#define VUL_NORMAL_MODE_MASK_SFT                      (0x1 << 21)
#define AWB_NORMAL_MODE_SFT                           20
#define AWB_NORMAL_MODE_MASK                          0x1
#define AWB_NORMAL_MODE_MASK_SFT                      (0x1 << 20)
#define DL3_NORMAL_MODE_SFT                           19
#define DL3_NORMAL_MODE_MASK                          0x1
#define DL3_NORMAL_MODE_MASK_SFT                      (0x1 << 19)
#define DL2_NORMAL_MODE_SFT                           18
#define DL2_NORMAL_MODE_MASK                          0x1
#define DL2_NORMAL_MODE_MASK_SFT                      (0x1 << 18)
#define DL12_NORMAL_MODE_SFT                          17
#define DL12_NORMAL_MODE_MASK                         0x1
#define DL12_NORMAL_MODE_MASK_SFT                     (0x1 << 17)
#define DL1_NORMAL_MODE_SFT                           16
#define DL1_NORMAL_MODE_MASK                          0x1
#define DL1_NORMAL_MODE_MASK_SFT                      (0x1 << 16)
#define AWB2_ALIGN_SFT                                14
#define AWB2_ALIGN_MASK                               0x1
#define AWB2_ALIGN_MASK_SFT                           (0x1 << 14)
#define MOD_DAI_HD_ALIGN_SFT                          9
#define MOD_DAI_HD_ALIGN_MASK                         0x1
#define MOD_DAI_HD_ALIGN_MASK_SFT                     (0x1 << 9)
#define VUL2_HD_ALIGN_SFT                             7
#define VUL2_HD_ALIGN_MASK                            0x1
#define VUL2_HD_ALIGN_MASK_SFT                        (0x1 << 7)
#define VUL_HD_ALIGN_SFT                              5
#define VUL_HD_ALIGN_MASK                             0x1
#define VUL_HD_ALIGN_MASK_SFT                         (0x1 << 5)
#define AWB_HD_ALIGN_SFT                              4
#define AWB_HD_ALIGN_MASK                             0x1
#define AWB_HD_ALIGN_MASK_SFT                         (0x1 << 4)
#define DL3_HD_ALIGN_SFT                              3
#define DL3_HD_ALIGN_MASK                             0x1
#define DL3_HD_ALIGN_MASK_SFT                         (0x1 << 3)
#define DL2_HD_ALIGN_SFT                              2
#define DL2_HD_ALIGN_MASK                             0x1
#define DL2_HD_ALIGN_MASK_SFT                         (0x1 << 2)
#define DL12_HD_ALIGN_SFT                             1
#define DL12_HD_ALIGN_MASK                            0x1
#define DL12_HD_ALIGN_MASK_SFT                        (0x1 << 1)
#define DL1_HD_ALIGN_SFT                              0
#define DL1_HD_ALIGN_MASK                             0x1
#define DL1_HD_ALIGN_MASK_SFT                         (0x1 << 0)

/* PCM2_INTF_CON */
#define TX_FIX_VALUE_SFT                              24
#define TX_FIX_VALUE_MASK                             0xff
#define TX_FIX_VALUE_MASK_SFT                         (0xff << 24)
#define FIX_VALUE_SEL_SFT                             23
#define FIX_VALUE_SEL_MASK                            0x1
#define FIX_VALUE_SEL_MASK_SFT                        (0x1 << 23)
#define BUFFER_LOOPBACK_SFT                           22
#define BUFFER_LOOPBACK_MASK                          0x1
#define BUFFER_LOOPBACK_MASK_SFT                      (0x1 << 22)
#define PARALLEL_LOOPBACK_SFT                         21
#define PARALLEL_LOOPBACK_MASK                        0x1
#define PARALLEL_LOOPBACK_MASK_SFT                    (0x1 << 21)
#define SERIAL_LOOPBACK_SFT                           20
#define SERIAL_LOOPBACK_MASK                          0x1
#define SERIAL_LOOPBACK_MASK_SFT                      (0x1 << 20)
#define DAI_PCM_LOOPBACK_SFT                          19
#define DAI_PCM_LOOPBACK_MASK                         0x1
#define DAI_PCM_LOOPBACK_MASK_SFT                     (0x1 << 19)
#define I2S_PCM_LOOPBACK_SFT                          18
#define I2S_PCM_LOOPBACK_MASK                         0x1
#define I2S_PCM_LOOPBACK_MASK_SFT                     (0x1 << 18)
#define SYNC_DELSEL_SFT                               17
#define SYNC_DELSEL_MASK                              0x1
#define SYNC_DELSEL_MASK_SFT                          (0x1 << 17)
#define TX_LR_SWAP_SFT                                16
#define TX_LR_SWAP_MASK                               0x1
#define TX_LR_SWAP_MASK_SFT                           (0x1 << 16)
#define SYNC_IN_INV_SFT                               15
#define SYNC_IN_INV_MASK                              0x1
#define SYNC_IN_INV_MASK_SFT                          (0x1 << 15)
#define BCLK_IN_INV_SFT                               14
#define BCLK_IN_INV_MASK                              0x1
#define BCLK_IN_INV_MASK_SFT                          (0x1 << 14)
#define TX_LCH_RPT_SFT                                13
#define TX_LCH_RPT_MASK                               0x1
#define TX_LCH_RPT_MASK_SFT                           (0x1 << 13)
#define VBT_16K_MODE_SFT                              12
#define VBT_16K_MODE_MASK                             0x1
#define VBT_16K_MODE_MASK_SFT                         (0x1 << 12)
#define LOOPBACK_CH_SEL_SFT                           10
#define LOOPBACK_CH_SEL_MASK                          0x3
#define LOOPBACK_CH_SEL_MASK_SFT                      (0x3 << 10)
#define TX3_RCH_DBG_MODE_SFT                          9
#define TX3_RCH_DBG_MODE_MASK                         0x1
#define TX3_RCH_DBG_MODE_MASK_SFT                     (0x1 << 9)
#define TX2_BT_MODE_SFT                               8
#define TX2_BT_MODE_MASK                              0x1
#define TX2_BT_MODE_MASK_SFT                          (0x1 << 8)
#define BT_MODE_SFT                                   7
#define BT_MODE_MASK                                  0x1
#define BT_MODE_MASK_SFT                              (0x1 << 7)
#define PCM_AFIFO_SFT                                 6
#define PCM_AFIFO_MASK                                0x1
#define PCM_AFIFO_MASK_SFT                            (0x1 << 6)
#define PCM_WLEN_SFT                                  5
#define PCM_WLEN_MASK                                 0x1
#define PCM_WLEN_MASK_SFT                             (0x1 << 5)
#define PCM_MODE_SFT                                  3
#define PCM_MODE_MASK                                 0x3
#define PCM_MODE_MASK_SFT                             (0x3 << 3)
#define PCM_FMT_SFT                                   1
#define PCM_FMT_MASK                                  0x3
#define PCM_FMT_MASK_SFT                              (0x3 << 1)
#define PCM_EN_SFT                                    0
#define PCM_EN_MASK                                   0x1
#define PCM_EN_MASK_SFT                               (0x1 << 0)

/* AFE_ADDA_MTKAIF_CFG0 */
#define MTKAIF_RXIF_CLKINV_ADC_SFT                    31
#define MTKAIF_RXIF_CLKINV_ADC_MASK                   0x1
#define MTKAIF_RXIF_CLKINV_ADC_MASK_SFT               (0x1 << 31)
#define MTKAIF_RXIF_BYPASS_SRC_SFT                    17
#define MTKAIF_RXIF_BYPASS_SRC_MASK                   0x1
#define MTKAIF_RXIF_BYPASS_SRC_MASK_SFT               (0x1 << 17)
#define MTKAIF_RXIF_PROTOCOL2_SFT                     16
#define MTKAIF_RXIF_PROTOCOL2_MASK                    0x1
#define MTKAIF_RXIF_PROTOCOL2_MASK_SFT                (0x1 << 16)
#define MTKAIF_TXIF_BYPASS_SRC_SFT                    5
#define MTKAIF_TXIF_BYPASS_SRC_MASK                   0x1
#define MTKAIF_TXIF_BYPASS_SRC_MASK_SFT               (0x1 << 5)
#define MTKAIF_TXIF_PROTOCOL2_SFT                     4
#define MTKAIF_TXIF_PROTOCOL2_MASK                    0x1
#define MTKAIF_TXIF_PROTOCOL2_MASK_SFT                (0x1 << 4)
#define MTKAIF_TXIF_8TO5_SFT                          2
#define MTKAIF_TXIF_8TO5_MASK                         0x1
#define MTKAIF_TXIF_8TO5_MASK_SFT                     (0x1 << 2)
#define MTKAIF_RXIF_8TO5_SFT                          1
#define MTKAIF_RXIF_8TO5_MASK                         0x1
#define MTKAIF_RXIF_8TO5_MASK_SFT                     (0x1 << 1)
#define MTKAIF_IF_LOOPBACK1_SFT                       0
#define MTKAIF_IF_LOOPBACK1_MASK                      0x1
#define MTKAIF_IF_LOOPBACK1_MASK_SFT                  (0x1 << 0)

/* AFE_ADDA_MTKAIF_RX_CFG2 */
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_SFT           16
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK          0x1
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK_SFT      (0x1 << 16)
#define MTKAIF_RXIF_DELAY_CYCLE_SFT                   12
#define MTKAIF_RXIF_DELAY_CYCLE_MASK                  0xf
#define MTKAIF_RXIF_DELAY_CYCLE_MASK_SFT              (0xf << 12)
#define MTKAIF_RXIF_DELAY_DATA_SFT                    8
#define MTKAIF_RXIF_DELAY_DATA_MASK                   0x1
#define MTKAIF_RXIF_DELAY_DATA_MASK_SFT               (0x1 << 8)
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_SFT            4
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK           0x7
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK_SFT       (0x7 << 4)

/* AFE_ADDA_DL_SRC2_CON0 */
#define DL_2_INPUT_MODE_CTL_SFT                       28
#define DL_2_INPUT_MODE_CTL_MASK                      0xf
#define DL_2_INPUT_MODE_CTL_MASK_SFT                  (0xf << 28)
#define DL_2_CH1_SATURATION_EN_CTL_SFT                27
#define DL_2_CH1_SATURATION_EN_CTL_MASK               0x1
#define DL_2_CH1_SATURATION_EN_CTL_MASK_SFT           (0x1 << 27)
#define DL_2_CH2_SATURATION_EN_CTL_SFT                26
#define DL_2_CH2_SATURATION_EN_CTL_MASK               0x1
#define DL_2_CH2_SATURATION_EN_CTL_MASK_SFT           (0x1 << 26)
#define DL_2_OUTPUT_SEL_CTL_SFT                       24
#define DL_2_OUTPUT_SEL_CTL_MASK                      0x3
#define DL_2_OUTPUT_SEL_CTL_MASK_SFT                  (0x3 << 24)
#define DL_2_FADEIN_0START_EN_SFT                     16
#define DL_2_FADEIN_0START_EN_MASK                    0x3
#define DL_2_FADEIN_0START_EN_MASK_SFT                (0x3 << 16)
#define DL_DISABLE_HW_CG_CTL_SFT                      15
#define DL_DISABLE_HW_CG_CTL_MASK                     0x1
#define DL_DISABLE_HW_CG_CTL_MASK_SFT                 (0x1 << 15)
#define C_DATA_EN_SEL_CTL_PRE_SFT                     14
#define C_DATA_EN_SEL_CTL_PRE_MASK                    0x1
#define C_DATA_EN_SEL_CTL_PRE_MASK_SFT                (0x1 << 14)
#define DL_2_SIDE_TONE_ON_CTL_PRE_SFT                 13
#define DL_2_SIDE_TONE_ON_CTL_PRE_MASK                0x1
#define DL_2_SIDE_TONE_ON_CTL_PRE_MASK_SFT            (0x1 << 13)
#define DL_2_MUTE_CH1_OFF_CTL_PRE_SFT                 12
#define DL_2_MUTE_CH1_OFF_CTL_PRE_MASK                0x1
#define DL_2_MUTE_CH1_OFF_CTL_PRE_MASK_SFT            (0x1 << 12)
#define DL_2_MUTE_CH2_OFF_CTL_PRE_SFT                 11
#define DL_2_MUTE_CH2_OFF_CTL_PRE_MASK                0x1
#define DL_2_MUTE_CH2_OFF_CTL_PRE_MASK_SFT            (0x1 << 11)
#define DL2_ARAMPSP_CTL_PRE_SFT                       9
#define DL2_ARAMPSP_CTL_PRE_MASK                      0x3
#define DL2_ARAMPSP_CTL_PRE_MASK_SFT                  (0x3 << 9)
#define DL_2_IIRMODE_CTL_PRE_SFT                      6
#define DL_2_IIRMODE_CTL_PRE_MASK                     0x7
#define DL_2_IIRMODE_CTL_PRE_MASK_SFT                 (0x7 << 6)
#define DL_2_VOICE_MODE_CTL_PRE_SFT                   5
#define DL_2_VOICE_MODE_CTL_PRE_MASK                  0x1
#define DL_2_VOICE_MODE_CTL_PRE_MASK_SFT              (0x1 << 5)
#define D2_2_MUTE_CH1_ON_CTL_PRE_SFT                  4
#define D2_2_MUTE_CH1_ON_CTL_PRE_MASK                 0x1
#define D2_2_MUTE_CH1_ON_CTL_PRE_MASK_SFT             (0x1 << 4)
#define D2_2_MUTE_CH2_ON_CTL_PRE_SFT                  3
#define D2_2_MUTE_CH2_ON_CTL_PRE_MASK                 0x1
#define D2_2_MUTE_CH2_ON_CTL_PRE_MASK_SFT             (0x1 << 3)
#define DL_2_IIR_ON_CTL_PRE_SFT                       2
#define DL_2_IIR_ON_CTL_PRE_MASK                      0x1
#define DL_2_IIR_ON_CTL_PRE_MASK_SFT                  (0x1 << 2)
#define DL_2_GAIN_ON_CTL_PRE_SFT                      1
#define DL_2_GAIN_ON_CTL_PRE_MASK                     0x1
#define DL_2_GAIN_ON_CTL_PRE_MASK_SFT                 (0x1 << 1)
#define DL_2_SRC_ON_TMP_CTL_PRE_SFT                   0
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK                  0x1
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK_SFT              (0x1 << 0)

/* AFE_ADDA_DL_SRC2_CON1 */
#define DL_2_GAIN_CTL_PRE_SFT                         16
#define DL_2_GAIN_CTL_PRE_MASK                        0xffff
#define DL_2_GAIN_CTL_PRE_MASK_SFT                    (0xffff << 16)
#define DL_2_GAIN_MODE_CTL_SFT                        0
#define DL_2_GAIN_MODE_CTL_MASK                       0x1
#define DL_2_GAIN_MODE_CTL_MASK_SFT                   (0x1 << 0)

/* AFE_ADDA_UL_SRC_CON0 */
#define ULCF_CFG_EN_CTL_SFT                           31
#define ULCF_CFG_EN_CTL_MASK                          0x1
#define ULCF_CFG_EN_CTL_MASK_SFT                      (0x1 << 31)
#define UL_MODE_3P25M_CH2_CTL_SFT                     22
#define UL_MODE_3P25M_CH2_CTL_MASK                    0x1
#define UL_MODE_3P25M_CH2_CTL_MASK_SFT                (0x1 << 22)
#define UL_MODE_3P25M_CH1_CTL_SFT                     21
#define UL_MODE_3P25M_CH1_CTL_MASK                    0x1
#define UL_MODE_3P25M_CH1_CTL_MASK_SFT                (0x1 << 21)
#define UL_VOICE_MODE_CH1_CH2_CTL_SFT                 17
#define UL_VOICE_MODE_CH1_CH2_CTL_MASK                0x7
#define UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT            (0x7 << 17)
#define DMIC_LOW_POWER_MODE_CTL_SFT                   14
#define DMIC_LOW_POWER_MODE_CTL_MASK                  0x3
#define DMIC_LOW_POWER_MODE_CTL_MASK_SFT              (0x3 << 14)
#define UL_DISABLE_HW_CG_CTL_SFT                      12
#define UL_DISABLE_HW_CG_CTL_MASK                     0x1
#define UL_DISABLE_HW_CG_CTL_MASK_SFT                 (0x1 << 12)
#define UL_IIR_ON_TMP_CTL_SFT                         10
#define UL_IIR_ON_TMP_CTL_MASK                        0x1
#define UL_IIR_ON_TMP_CTL_MASK_SFT                    (0x1 << 10)
#define UL_IIRMODE_CTL_SFT                            7
#define UL_IIRMODE_CTL_MASK                           0x7
#define UL_IIRMODE_CTL_MASK_SFT                       (0x7 << 7)
#define DIGMIC_3P25M_1P625M_SEL_CTL_SFT               5
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK              0x1
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT          (0x1 << 5)
#define UL_LOOP_BACK_MODE_CTL_SFT                     2
#define UL_LOOP_BACK_MODE_CTL_MASK                    0x1
#define UL_LOOP_BACK_MODE_CTL_MASK_SFT                (0x1 << 2)
#define UL_SDM_3_LEVEL_CTL_SFT                        1
#define UL_SDM_3_LEVEL_CTL_MASK                       0x1
#define UL_SDM_3_LEVEL_CTL_MASK_SFT                   (0x1 << 1)
#define UL_SRC_ON_TMP_CTL_SFT                         0
#define UL_SRC_ON_TMP_CTL_MASK                        0x1
#define UL_SRC_ON_TMP_CTL_MASK_SFT                    (0x1 << 0)

/* AFE_ADDA_UL_SRC_CON1 */
#define C_DAC_EN_CTL_SFT                              27
#define C_DAC_EN_CTL_MASK                             0x1
#define C_DAC_EN_CTL_MASK_SFT                         (0x1 << 27)
#define C_MUTE_SW_CTL_SFT                             26
#define C_MUTE_SW_CTL_MASK                            0x1
#define C_MUTE_SW_CTL_MASK_SFT                        (0x1 << 26)
#define ASDM_SRC_SEL_CTL_SFT                          25
#define ASDM_SRC_SEL_CTL_MASK                         0x1
#define ASDM_SRC_SEL_CTL_MASK_SFT                     (0x1 << 25)
#define C_AMP_DIV_CH2_CTL_SFT                         21
#define C_AMP_DIV_CH2_CTL_MASK                        0x7
#define C_AMP_DIV_CH2_CTL_MASK_SFT                    (0x7 << 21)
#define C_FREQ_DIV_CH2_CTL_SFT                        16
#define C_FREQ_DIV_CH2_CTL_MASK                       0x1f
#define C_FREQ_DIV_CH2_CTL_MASK_SFT                   (0x1f << 16)
#define C_SINE_MODE_CH2_CTL_SFT                       12
#define C_SINE_MODE_CH2_CTL_MASK                      0xf
#define C_SINE_MODE_CH2_CTL_MASK_SFT                  (0xf << 12)
#define C_AMP_DIV_CH1_CTL_SFT                         9
#define C_AMP_DIV_CH1_CTL_MASK                        0x7
#define C_AMP_DIV_CH1_CTL_MASK_SFT                    (0x7 << 9)
#define C_FREQ_DIV_CH1_CTL_SFT                        4
#define C_FREQ_DIV_CH1_CTL_MASK                       0x1f
#define C_FREQ_DIV_CH1_CTL_MASK_SFT                   (0x1f << 4)
#define C_SINE_MODE_CH1_CTL_SFT                       0
#define C_SINE_MODE_CH1_CTL_MASK                      0xf
#define C_SINE_MODE_CH1_CTL_MASK_SFT                  (0xf << 0)

/* AFE_ADDA_TOP_CON0 */
#define C_LOOP_BACK_MODE_CTL_SFT                      12
#define C_LOOP_BACK_MODE_CTL_MASK                     0xf
#define C_LOOP_BACK_MODE_CTL_MASK_SFT                 (0xf << 12)
#define C_EXT_ADC_CTL_SFT                             0
#define C_EXT_ADC_CTL_MASK                            0x1
#define C_EXT_ADC_CTL_MASK_SFT                        (0x1 << 0)

/* AFE_ADDA_UL_DL_CON0 */
#define ADDA_AFE_ON_SFT                               0
#define ADDA_AFE_ON_MASK                              0x1
#define ADDA_AFE_ON_MASK_SFT                          (0x1 << 0)

/* AFE_SIDETONE_CON0 */
#define R_RDY_SFT                                     30
#define R_RDY_MASK                                    0x1
#define R_RDY_MASK_SFT                                (0x1 << 30)
#define W_RDY_SFT                                     29
#define W_RDY_MASK                                    0x1
#define W_RDY_MASK_SFT                                (0x1 << 29)
#define R_W_EN_SFT                                    25
#define R_W_EN_MASK                                   0x1
#define R_W_EN_MASK_SFT                               (0x1 << 25)
#define R_W_SEL_SFT                                   24
#define R_W_SEL_MASK                                  0x1
#define R_W_SEL_MASK_SFT                              (0x1 << 24)
#define SEL_CH2_SFT                                   23
#define SEL_CH2_MASK                                  0x1
#define SEL_CH2_MASK_SFT                              (0x1 << 23)
#define SIDE_TONE_COEFFICIENT_ADDR_SFT                16
#define SIDE_TONE_COEFFICIENT_ADDR_MASK               0x1f
#define SIDE_TONE_COEFFICIENT_ADDR_MASK_SFT           (0x1f << 16)
#define SIDE_TONE_COEFFICIENT_SFT                     0
#define SIDE_TONE_COEFFICIENT_MASK                    0xffff
#define SIDE_TONE_COEFFICIENT_MASK_SFT                (0xffff << 0)

/* AFE_SIDETONE_COEFF */
#define SIDE_TONE_COEFF_SFT                           0
#define SIDE_TONE_COEFF_MASK                          0xffff
#define SIDE_TONE_COEFF_MASK_SFT                      (0xffff << 0)

/* AFE_SIDETONE_CON1 */
#define STF_BYPASS_MODE_SFT                           31
#define STF_BYPASS_MODE_MASK                          0x1
#define STF_BYPASS_MODE_MASK_SFT                      (0x1 << 31)
#define STF_BYPASS_MODE_O28_O29_SFT                   30
#define STF_BYPASS_MODE_O28_O29_MASK                  0x1
#define STF_BYPASS_MODE_O28_O29_MASK_SFT              (0x1 << 30)
#define STF_BYPASS_MODE_I2S4_SFT                      29
#define STF_BYPASS_MODE_I2S4_MASK                     0x1
#define STF_BYPASS_MODE_I2S4_MASK_SFT                 (0x1 << 29)
#define SIDE_TONE_ON_SFT                              8
#define SIDE_TONE_ON_MASK                             0x1
#define SIDE_TONE_ON_MASK_SFT                         (0x1 << 8)
#define SIDE_TONE_HALF_TAP_NUM_SFT                    0
#define SIDE_TONE_HALF_TAP_NUM_MASK                   0x3f
#define SIDE_TONE_HALF_TAP_NUM_MASK_SFT               (0x3f << 0)

/* AFE_SIDETONE_GAIN */
#define POSITIVE_GAIN_SFT                             16
#define POSITIVE_GAIN_MASK                            0x7
#define POSITIVE_GAIN_MASK_SFT                        (0x7 << 16)
#define SIDE_TONE_GAIN_SFT                            0
#define SIDE_TONE_GAIN_MASK                           0xffff
#define SIDE_TONE_GAIN_MASK_SFT                       (0xffff << 0)

/* AFE_ADDA_DL_SDM_DCCOMP_CON */
#define DL_FIFO_START_POINT_SFT                       24
#define DL_FIFO_START_POINT_MASK                      0x7
#define DL_FIFO_START_POINT_MASK_SFT                  (0x7 << 24)
#define DL_FIFO_SWAP_SFT                              20
#define DL_FIFO_SWAP_MASK                             0x1
#define DL_FIFO_SWAP_MASK_SFT                         (0x1 << 20)
#define C_AUDSDM1ORDSELECT_CTL_SFT                    19
#define C_AUDSDM1ORDSELECT_CTL_MASK                   0x1
#define C_AUDSDM1ORDSELECT_CTL_MASK_SFT               (0x1 << 19)
#define C_SDM7BITSEL_CTL_SFT                          18
#define C_SDM7BITSEL_CTL_MASK                         0x1
#define C_SDM7BITSEL_CTL_MASK_SFT                     (0x1 << 18)
#define GAIN_AT_SDM_RST_PRE_CTL_SFT                   15
#define GAIN_AT_SDM_RST_PRE_CTL_MASK                  0x1
#define GAIN_AT_SDM_RST_PRE_CTL_MASK_SFT              (0x1 << 15)
#define DL_DCM_AUTO_IDLE_EN_SFT                       14
#define DL_DCM_AUTO_IDLE_EN_MASK                      0x1
#define DL_DCM_AUTO_IDLE_EN_MASK_SFT                  (0x1 << 14)
#define AFE_DL_SRC_DCM_EN_SFT                         13
#define AFE_DL_SRC_DCM_EN_MASK                        0x1
#define AFE_DL_SRC_DCM_EN_MASK_SFT                    (0x1 << 13)
#define AFE_DL_POST_SRC_DCM_EN_SFT                    12
#define AFE_DL_POST_SRC_DCM_EN_MASK                   0x1
#define AFE_DL_POST_SRC_DCM_EN_MASK_SFT               (0x1 << 12)
#define AUD_SDM_MONO_SFT                              9
#define AUD_SDM_MONO_MASK                             0x1
#define AUD_SDM_MONO_MASK_SFT                         (0x1 << 9)
#define AUD_DC_COMP_EN_SFT                            8
#define AUD_DC_COMP_EN_MASK                           0x1
#define AUD_DC_COMP_EN_MASK_SFT                       (0x1 << 8)
#define ATTGAIN_CTL_SFT                               0
#define ATTGAIN_CTL_MASK                              0x3f
#define ATTGAIN_CTL_MASK_SFT                          (0x3f << 0)

/* AFE_SINEGEN_CON0 */
#define DAC_EN_SFT                                    26
#define DAC_EN_MASK                                   0x1
#define DAC_EN_MASK_SFT                               (0x1 << 26)
#define MUTE_SW_CH2_SFT                               25
#define MUTE_SW_CH2_MASK                              0x1
#define MUTE_SW_CH2_MASK_SFT                          (0x1 << 25)
#define MUTE_SW_CH1_SFT                               24
#define MUTE_SW_CH1_MASK                              0x1
#define MUTE_SW_CH1_MASK_SFT                          (0x1 << 24)
#define SINE_MODE_CH2_SFT                             20
#define SINE_MODE_CH2_MASK                            0xf
#define SINE_MODE_CH2_MASK_SFT                        (0xf << 20)
#define AMP_DIV_CH2_SFT                               17
#define AMP_DIV_CH2_MASK                              0x7
#define AMP_DIV_CH2_MASK_SFT                          (0x7 << 17)
#define FREQ_DIV_CH2_SFT                              12
#define FREQ_DIV_CH2_MASK                             0x1f
#define FREQ_DIV_CH2_MASK_SFT                         (0x1f << 12)
#define SINE_MODE_CH1_SFT                             8
#define SINE_MODE_CH1_MASK                            0xf
#define SINE_MODE_CH1_MASK_SFT                        (0xf << 8)
#define AMP_DIV_CH1_SFT                               5
#define AMP_DIV_CH1_MASK                              0x7
#define AMP_DIV_CH1_MASK_SFT                          (0x7 << 5)
#define FREQ_DIV_CH1_SFT                              0
#define FREQ_DIV_CH1_MASK                             0x1f
#define FREQ_DIV_CH1_MASK_SFT                         (0x1f << 0)

/* AFE_SINEGEN_CON2 */
#define INNER_LOOP_BACK_MODE_SFT                      0
#define INNER_LOOP_BACK_MODE_MASK                     0x3f
#define INNER_LOOP_BACK_MODE_MASK_SFT                 (0x3f << 0)

/* AFE_MEMIF_MINLEN */
#define DL3_MINLEN_SFT                                12
#define DL3_MINLEN_MASK                               0xf
#define DL3_MINLEN_MASK_SFT                           (0xf << 12)
#define DL2_MINLEN_SFT                                8
#define DL2_MINLEN_MASK                               0xf
#define DL2_MINLEN_MASK_SFT                           (0xf << 8)
#define DL1_DATA2_MINLEN_SFT                          4
#define DL1_DATA2_MINLEN_MASK                         0xf
#define DL1_DATA2_MINLEN_MASK_SFT                     (0xf << 4)
#define DL1_MINLEN_SFT                                0
#define DL1_MINLEN_MASK                               0xf
#define DL1_MINLEN_MASK_SFT                           (0xf << 0)

/* AFE_MEMIF_MAXLEN */
#define DL3_MAXLEN_SFT                                8
#define DL3_MAXLEN_MASK                               0xf
#define DL3_MAXLEN_MASK_SFT                           (0xf << 8)
#define DL2_MAXLEN_SFT                                4
#define DL2_MAXLEN_MASK                               0xf
#define DL2_MAXLEN_MASK_SFT                           (0xf << 4)
#define DL1_DATA2_MAXLEN_SFT                          2
#define DL1_DATA2_MAXLEN_MASK                         0x3
#define DL1_DATA2_MAXLEN_MASK_SFT                     (0x3 << 2)
#define DL1_MAXLEN_SFT                                0
#define DL1_MAXLEN_MASK                               0x3
#define DL1_MAXLEN_MASK_SFT                           (0x3 << 0)

/* AFE_MEMIF_PBUF_SIZE */
#define VUL12_4CH_EN_SFT                              17
#define VUL12_4CH_EN_MASK                             0x1
#define VUL12_4CH_EN_MASK_SFT                         (0x1 << 17)
#define DL12_4CH_EN_SFT                               16
#define DL12_4CH_EN_MASK                              0x1
#define DL12_4CH_EN_MASK_SFT                          (0x1 << 16)
#define DL3_PBUF_SIZE_SFT                             10
#define DL3_PBUF_SIZE_MASK                            0x3
#define DL3_PBUF_SIZE_MASK_SFT                        (0x3 << 10)
#define DL1_DATA2_PBUF_SIZE_SFT                       8
#define DL1_DATA2_PBUF_SIZE_MASK                      0x3
#define DL1_DATA2_PBUF_SIZE_MASK_SFT                  (0x3 << 8)
#define DL2_PBUF_SIZE_SFT                             2
#define DL2_PBUF_SIZE_MASK                            0x3
#define DL2_PBUF_SIZE_MASK_SFT                        (0x3 << 2)
#define DL1_PBUF_SIZE_SFT                             0
#define DL1_PBUF_SIZE_MASK                            0x3
#define DL1_PBUF_SIZE_MASK_SFT                        (0x3 << 0)

/* AFE_HD_ENGEN_ENABLE */
#define AFE_24M_ON_SFT                                1
#define AFE_24M_ON_MASK                               0x1
#define AFE_24M_ON_MASK_SFT                           (0x1 << 1)
#define AFE_22M_ON_SFT                                0
#define AFE_22M_ON_MASK                               0x1
#define AFE_22M_ON_MASK_SFT                           (0x1 << 0)

/* AFE_IRQ_MCU_CON0 */
#define IRQ12_MCU_ON_SFT                              12
#define IRQ12_MCU_ON_MASK                             0x1
#define IRQ12_MCU_ON_MASK_SFT                         (0x1 << 12)
#define IRQ11_MCU_ON_SFT                              11
#define IRQ11_MCU_ON_MASK                             0x1
#define IRQ11_MCU_ON_MASK_SFT                         (0x1 << 11)
#define IRQ10_MCU_ON_SFT                              10
#define IRQ10_MCU_ON_MASK                             0x1
#define IRQ10_MCU_ON_MASK_SFT                         (0x1 << 10)
#define IRQ9_MCU_ON_SFT                               9
#define IRQ9_MCU_ON_MASK                              0x1
#define IRQ9_MCU_ON_MASK_SFT                          (0x1 << 9)
#define IRQ8_MCU_ON_SFT                               8
#define IRQ8_MCU_ON_MASK                              0x1
#define IRQ8_MCU_ON_MASK_SFT                          (0x1 << 8)
#define IRQ7_MCU_ON_SFT                               7
#define IRQ7_MCU_ON_MASK                              0x1
#define IRQ7_MCU_ON_MASK_SFT                          (0x1 << 7)
#define IRQ6_MCU_ON_SFT                               6
#define IRQ6_MCU_ON_MASK                              0x1
#define IRQ6_MCU_ON_MASK_SFT                          (0x1 << 6)
#define IRQ5_MCU_ON_SFT                               5
#define IRQ5_MCU_ON_MASK                              0x1
#define IRQ5_MCU_ON_MASK_SFT                          (0x1 << 5)
#define IRQ4_MCU_ON_SFT                               4
#define IRQ4_MCU_ON_MASK                              0x1
#define IRQ4_MCU_ON_MASK_SFT                          (0x1 << 4)
#define IRQ3_MCU_ON_SFT                               3
#define IRQ3_MCU_ON_MASK                              0x1
#define IRQ3_MCU_ON_MASK_SFT                          (0x1 << 3)
#define IRQ2_MCU_ON_SFT                               2
#define IRQ2_MCU_ON_MASK                              0x1
#define IRQ2_MCU_ON_MASK_SFT                          (0x1 << 2)
#define IRQ1_MCU_ON_SFT                               1
#define IRQ1_MCU_ON_MASK                              0x1
#define IRQ1_MCU_ON_MASK_SFT                          (0x1 << 1)
#define IRQ0_MCU_ON_SFT                               0
#define IRQ0_MCU_ON_MASK                              0x1
#define IRQ0_MCU_ON_MASK_SFT                          (0x1 << 0)

/* AFE_IRQ_MCU_CLR */
#define IRQ12_MCU_MISS_CNT_CLR_SFT                    28
#define IRQ12_MCU_MISS_CNT_CLR_MASK                   0x1
#define IRQ12_MCU_MISS_CNT_CLR_MASK_SFT               (0x1 << 28)
#define IRQ11_MCU_MISS_CNT_CLR_SFT                    27
#define IRQ11_MCU_MISS_CNT_CLR_MASK                   0x1
#define IRQ11_MCU_MISS_CNT_CLR_MASK_SFT               (0x1 << 27)
#define IRQ10_MCU_MISS_CLR_SFT                        26
#define IRQ10_MCU_MISS_CLR_MASK                       0x1
#define IRQ10_MCU_MISS_CLR_MASK_SFT                   (0x1 << 26)
#define IRQ9_MCU_MISS_CLR_SFT                         25
#define IRQ9_MCU_MISS_CLR_MASK                        0x1
#define IRQ9_MCU_MISS_CLR_MASK_SFT                    (0x1 << 25)
#define IRQ8_MCU_MISS_CLR_SFT                         24
#define IRQ8_MCU_MISS_CLR_MASK                        0x1
#define IRQ8_MCU_MISS_CLR_MASK_SFT                    (0x1 << 24)
#define IRQ7_MCU_MISS_CLR_SFT                         23
#define IRQ7_MCU_MISS_CLR_MASK                        0x1
#define IRQ7_MCU_MISS_CLR_MASK_SFT                    (0x1 << 23)
#define IRQ6_MCU_MISS_CLR_SFT                         22
#define IRQ6_MCU_MISS_CLR_MASK                        0x1
#define IRQ6_MCU_MISS_CLR_MASK_SFT                    (0x1 << 22)
#define IRQ5_MCU_MISS_CLR_SFT                         21
#define IRQ5_MCU_MISS_CLR_MASK                        0x1
#define IRQ5_MCU_MISS_CLR_MASK_SFT                    (0x1 << 21)
#define IRQ4_MCU_MISS_CLR_SFT                         20
#define IRQ4_MCU_MISS_CLR_MASK                        0x1
#define IRQ4_MCU_MISS_CLR_MASK_SFT                    (0x1 << 20)
#define IRQ3_MCU_MISS_CLR_SFT                         19
#define IRQ3_MCU_MISS_CLR_MASK                        0x1
#define IRQ3_MCU_MISS_CLR_MASK_SFT                    (0x1 << 19)
#define IRQ2_MCU_MISS_CLR_SFT                         18
#define IRQ2_MCU_MISS_CLR_MASK                        0x1
#define IRQ2_MCU_MISS_CLR_MASK_SFT                    (0x1 << 18)
#define IRQ1_MCU_MISS_CLR_SFT                         17
#define IRQ1_MCU_MISS_CLR_MASK                        0x1
#define IRQ1_MCU_MISS_CLR_MASK_SFT                    (0x1 << 17)
#define IRQ0_MCU_MISS_CLR_SFT                         16
#define IRQ0_MCU_MISS_CLR_MASK                        0x1
#define IRQ0_MCU_MISS_CLR_MASK_SFT                    (0x1 << 16)
#define IRQ12_MCU_CLR_SFT                             12
#define IRQ12_MCU_CLR_MASK                            0x1
#define IRQ12_MCU_CLR_MASK_SFT                        (0x1 << 12)
#define IRQ11_MCU_CLR_SFT                             11
#define IRQ11_MCU_CLR_MASK                            0x1
#define IRQ11_MCU_CLR_MASK_SFT                        (0x1 << 11)
#define IRQ10_MCU_CLR_SFT                             10
#define IRQ10_MCU_CLR_MASK                            0x1
#define IRQ10_MCU_CLR_MASK_SFT                        (0x1 << 10)
#define IRQ9_MCU_CLR_SFT                              9
#define IRQ9_MCU_CLR_MASK                             0x1
#define IRQ9_MCU_CLR_MASK_SFT                         (0x1 << 9)
#define IRQ8_MCU_CLR_SFT                              8
#define IRQ8_MCU_CLR_MASK                             0x1
#define IRQ8_MCU_CLR_MASK_SFT                         (0x1 << 8)
#define IRQ7_MCU_CLR_SFT                              7
#define IRQ7_MCU_CLR_MASK                             0x1
#define IRQ7_MCU_CLR_MASK_SFT                         (0x1 << 7)
#define IRQ6_MCU_CLR_SFT                              6
#define IRQ6_MCU_CLR_MASK                             0x1
#define IRQ6_MCU_CLR_MASK_SFT                         (0x1 << 6)
#define IRQ5_MCU_CLR_SFT                              5
#define IRQ5_MCU_CLR_MASK                             0x1
#define IRQ5_MCU_CLR_MASK_SFT                         (0x1 << 5)
#define IRQ4_MCU_CLR_SFT                              4
#define IRQ4_MCU_CLR_MASK                             0x1
#define IRQ4_MCU_CLR_MASK_SFT                         (0x1 << 4)
#define IRQ3_MCU_CLR_SFT                              3
#define IRQ3_MCU_CLR_MASK                             0x1
#define IRQ3_MCU_CLR_MASK_SFT                         (0x1 << 3)
#define IRQ2_MCU_CLR_SFT                              2
#define IRQ2_MCU_CLR_MASK                             0x1
#define IRQ2_MCU_CLR_MASK_SFT                         (0x1 << 2)
#define IRQ1_MCU_CLR_SFT                              1
#define IRQ1_MCU_CLR_MASK                             0x1
#define IRQ1_MCU_CLR_MASK_SFT                         (0x1 << 1)
#define IRQ0_MCU_CLR_SFT                              0
#define IRQ0_MCU_CLR_MASK                             0x1
#define IRQ0_MCU_CLR_MASK_SFT                         (0x1 << 0)

/* AFE_IRQ_MCU_EN */
#define IRQ12_MCU_EN_SFT                               12
#define IRQ11_MCU_EN_SFT                               11
#define IRQ10_MCU_EN_SFT                               10
#define IRQ9_MCU_EN_SFT                                9
#define IRQ8_MCU_EN_SFT                                8
#define IRQ7_MCU_EN_SFT                                7
#define IRQ6_MCU_EN_SFT                                6
#define IRQ5_MCU_EN_SFT                                5
#define IRQ4_MCU_EN_SFT                                4
#define IRQ3_MCU_EN_SFT                                3
#define IRQ2_MCU_EN_SFT                                2
#define IRQ1_MCU_EN_SFT                                1
#define IRQ0_MCU_EN_SFT                                0

/* AFE_IRQ_MCU_SCP_EN */
#define IRQ12_MCU_SCP_EN_SFT                           12
#define IRQ11_MCU_SCP_EN_SFT                           11
#define IRQ10_MCU_SCP_EN_SFT                           10
#define IRQ9_MCU_SCP_EN_SFT                            9
#define IRQ8_MCU_SCP_EN_SFT                            8
#define IRQ7_MCU_SCP_EN_SFT                            7
#define IRQ6_MCU_SCP_EN_SFT                            6
#define IRQ5_MCU_SCP_EN_SFT                            5
#define IRQ4_MCU_SCP_EN_SFT                            4
#define IRQ3_MCU_SCP_EN_SFT                            3
#define IRQ2_MCU_SCP_EN_SFT                            2
#define IRQ1_MCU_SCP_EN_SFT                            1
#define IRQ0_MCU_SCP_EN_SFT                            0

/* AFE_ASRC_2CH_CON0 */
#define CON0_CHSET_STR_CLR_SFT                         4
#define CON0_CHSET_STR_CLR_MASK                        1
#define CON0_CHSET_STR_CLR_MASK_SFT                    (0x1 << 4)
#define CON0_ASM_ON_SFT                                0
#define CON0_ASM_ON_MASK                               1
#define CON0_ASM_ON_MASK_SFT                           (0x1 << 0)

/* AFE_ASRC_2CH_CON5 */
#define CALI_EN_SFT                                    0
#define CALI_EN_MASK                                   1
#define CALI_EN_MASK_SFT                               (0x1 << 0)

#define AUDIO_TOP_CON0                0x0000
#define AUDIO_TOP_CON1                0x0004
#define AUDIO_TOP_CON3                0x000c
#define AFE_DAC_CON0                  0x0010
#define AFE_DAC_CON1                  0x0014
#define AFE_I2S_CON                   0x0018
#define AFE_CONN0                     0x0020
#define AFE_CONN1                     0x0024
#define AFE_CONN3                     0x002c
#define AFE_CONN4                     0x0030
#define AFE_I2S_CON1                  0x0034
#define AFE_I2S_CON2                  0x0038
#define AFE_DL1_BASE                  0x0040
#define AFE_DL1_CUR                   0x0044
#define AFE_DL1_END                   0x0048
#define AFE_I2S_CON3                  0x004c
#define AFE_DL2_BASE                  0x0050
#define AFE_DL2_CUR                   0x0054
#define AFE_DL2_END                   0x0058
#define AFE_CONN5                     0x005c
#define AFE_CONN_24BIT                0x006c
#define AFE_AWB_BASE                  0x0070
#define AFE_AWB_END                   0x0078
#define AFE_AWB_CUR                   0x007c
#define AFE_VUL_BASE                  0x0080
#define AFE_VUL_END                   0x0088
#define AFE_VUL_CUR                   0x008c
#define AFE_CONN6                     0x00bc
#define AFE_MEMIF_MSB                 0x00cc
#define AFE_MEMIF_MON0                0x00d0
#define AFE_MEMIF_MON1                0x00d4
#define AFE_MEMIF_MON2                0x00d8
#define AFE_MEMIF_MON3                0x00dc
#define AFE_MEMIF_MON4                0x00e0
#define AFE_MEMIF_MON5                0x00e4
#define AFE_MEMIF_MON6                0x00e8
#define AFE_MEMIF_MON7                0x00ec
#define AFE_MEMIF_MON8                0x00f0
#define AFE_MEMIF_MON9                0x00f4
#define AFE_ADDA_DL_SRC2_CON0         0x0108
#define AFE_ADDA_DL_SRC2_CON1         0x010c
#define AFE_ADDA_UL_SRC_CON0          0x0114
#define AFE_ADDA_UL_SRC_CON1          0x0118
#define AFE_ADDA_TOP_CON0             0x0120
#define AFE_ADDA_UL_DL_CON0           0x0124
#define AFE_ADDA_SRC_DEBUG            0x012c
#define AFE_ADDA_SRC_DEBUG_MON0       0x0130
#define AFE_ADDA_SRC_DEBUG_MON1       0x0134
#define AFE_ADDA_UL_SRC_MON0          0x0148
#define AFE_ADDA_UL_SRC_MON1          0x014c
#define AFE_SIDETONE_DEBUG            0x01d0
#define AFE_SIDETONE_MON              0x01d4
#define AFE_SINEGEN_CON2              0x01dc
#define AFE_SIDETONE_CON0             0x01e0
#define AFE_SIDETONE_COEFF            0x01e4
#define AFE_SIDETONE_CON1             0x01e8
#define AFE_SIDETONE_GAIN             0x01ec
#define AFE_SINEGEN_CON0              0x01f0
#define AFE_TOP_CON0                  0x0200
#define AFE_BUS_CFG                   0x0240
#define AFE_BUS_MON0                  0x0244
#define AFE_ADDA_PREDIS_CON0          0x0260
#define AFE_ADDA_PREDIS_CON1          0x0264
#define AFE_MRGIF_MON0                0x0270
#define AFE_MRGIF_MON1                0x0274
#define AFE_MRGIF_MON2                0x0278
#define AFE_ADDA_IIR_COEF_02_01       0x0290
#define AFE_ADDA_IIR_COEF_04_03       0x0294
#define AFE_ADDA_IIR_COEF_06_05       0x0298
#define AFE_ADDA_IIR_COEF_08_07       0x029c
#define AFE_ADDA_IIR_COEF_10_09       0x02a0
#define AFE_DAC_CON2                  0x02e0
#define AFE_IRQ_MCU_CON1              0x02e4
#define AFE_IRQ_MCU_CON2              0x02e8
#define AFE_DAC_MON                   0x02ec
#define AFE_VUL2_BASE                 0x02f0
#define AFE_VUL2_END                  0x02f8
#define AFE_VUL2_CUR                  0x02fc
#define AFE_IRQ_MCU_CNT0              0x0300
#define AFE_IRQ_MCU_CNT6              0x0304
#define AFE_IRQ_MCU_EN1               0x030c
#define AFE_IRQ0_MCU_CNT_MON          0x0310
#define AFE_IRQ6_MCU_CNT_MON          0x0314
#define AFE_MOD_DAI_BASE              0x0330
#define AFE_MOD_DAI_END               0x0338
#define AFE_MOD_DAI_CUR               0x033c
#define AFE_DL1_D2_BASE               0x0340
#define AFE_DL1_D2_CUR                0x0344
#define AFE_DL1_D2_END                0x0348
#define AFE_VUL_D2_BASE               0x0350
#define AFE_VUL_D2_END                0x0358
#define AFE_VUL_D2_CUR                0x035c
#define AFE_DL3_BASE                  0x0360
#define AFE_DL3_CUR                   0x0364
#define AFE_DL3_END                   0x0368
#define AFE_IRQ3_MCU_CNT_MON          0x0398
#define AFE_IRQ4_MCU_CNT_MON          0x039c
#define AFE_IRQ_MCU_CON0              0x03a0
#define AFE_IRQ_MCU_STATUS            0x03a4
#define AFE_IRQ_MCU_CLR               0x03a8
#define AFE_IRQ_MCU_CNT1              0x03ac
#define AFE_IRQ_MCU_CNT2              0x03b0
#define AFE_IRQ_MCU_EN                0x03b4
#define AFE_IRQ_MCU_MON2              0x03b8
#define AFE_IRQ_MCU_CNT5              0x03bc
#define AFE_IRQ1_MCU_CNT_MON          0x03c0
#define AFE_IRQ2_MCU_CNT_MON          0x03c4
#define AFE_IRQ1_MCU_EN_CNT_MON       0x03c8
#define AFE_IRQ5_MCU_CNT_MON          0x03cc
#define AFE_MEMIF_MINLEN              0x03d0
#define AFE_MEMIF_MAXLEN              0x03d4
#define AFE_MEMIF_PBUF_SIZE           0x03d8
#define AFE_IRQ_MCU_CNT7              0x03dc
#define AFE_IRQ7_MCU_CNT_MON          0x03e0
#define AFE_IRQ_MCU_CNT3              0x03e4
#define AFE_IRQ_MCU_CNT4              0x03e8
#define AFE_IRQ_MCU_CNT11             0x03ec
#define AFE_APLL_TUNER_CFG            0x03f0
#define AFE_MEMIF_HD_MODE             0x03f8
#define AFE_MEMIF_HDALIGN             0x03fc
#define AFE_CONN33                    0x0408
#define AFE_IRQ_MCU_CNT12             0x040c
#define AFE_GAIN1_CON0                0x0410
#define AFE_GAIN1_CON1                0x0414
#define AFE_GAIN1_CON2                0x0418
#define AFE_GAIN1_CON3                0x041c
#define AFE_GAIN1_CUR                 0x0424
#define AFE_GAIN2_CON0                0x0428
#define AFE_GAIN2_CON1                0x042c
#define AFE_GAIN2_CON2                0x0430
#define AFE_GAIN2_CON3                0x0434
#define AFE_GAIN2_CUR                 0x043c
#define AFE_CONN9                     0x0440
#define AFE_CONN10                    0x0444
#define AFE_CONN12                    0x044c
#define AFE_CONN13                    0x0450
#define AFE_CONN14                    0x0454
#define AFE_CONN15                    0x0458
#define AFE_CONN16                    0x045c
#define AFE_CONN17                    0x0460
#define AFE_CONN18                    0x0464
#define AFE_CONN21                    0x0470
#define AFE_CONN22                    0x0474
#define AFE_CONN23                    0x0478
#define AFE_CONN24                    0x047c
#define AFE_CONN_RS                   0x0494
#define AFE_CONN_DI                   0x0498
#define AFE_CONN25                    0x04b0
#define AFE_CONN28                    0x04bc
#define AFE_CONN29                    0x04c0
#define AFE_CONN32                    0x04cc
#define AFE_SRAM_DELSEL_CON0          0x04f0
#define AFE_SRAM_DELSEL_CON2          0x04f8
#define AFE_SRAM_DELSEL_CON3          0x04fc
#define AFE_ASRC_2CH_CON12            0x0528
#define AFE_ASRC_2CH_CON13            0x052c
#define PCM2_INTF_CON                 0x053c
#define FPGA_CFG0                     0x05b0
#define FPGA_CFG1                     0x05b4
#define FPGA_CFG2                     0x05c0
#define FPGA_CFG3                     0x05c4
#define AUDIO_TOP_DBG_CON             0x05c8
#define AUDIO_TOP_DBG_MON0            0x05cc
#define AUDIO_TOP_DBG_MON1            0x05d0
#define AFE_IRQ8_MCU_CNT_MON          0x05e4
#define AFE_IRQ11_MCU_CNT_MON         0x05e8
#define AFE_IRQ12_MCU_CNT_MON         0x05ec
#define AFE_GENERAL_REG0              0x0800
#define AFE_GENERAL_REG1              0x0804
#define AFE_GENERAL_REG2              0x0808
#define AFE_GENERAL_REG3              0x080c
#define AFE_GENERAL_REG4              0x0810
#define AFE_GENERAL_REG5              0x0814
#define AFE_GENERAL_REG6              0x0818
#define AFE_GENERAL_REG7              0x081c
#define AFE_GENERAL_REG8              0x0820
#define AFE_GENERAL_REG9              0x0824
#define AFE_GENERAL_REG10             0x0828
#define AFE_GENERAL_REG11             0x082c
#define AFE_GENERAL_REG12             0x0830
#define AFE_GENERAL_REG13             0x0834
#define AFE_GENERAL_REG14             0x0838
#define AFE_GENERAL_REG15             0x083c
#define AFE_CBIP_CFG0                 0x0840
#define AFE_CBIP_MON0                 0x0844
#define AFE_CBIP_SLV_MUX_MON0         0x0848
#define AFE_CBIP_SLV_DECODER_MON0     0x084c
#define AFE_CONN0_1                   0x0900
#define AFE_CONN1_1                   0x0904
#define AFE_CONN3_1                   0x090c
#define AFE_CONN4_1                   0x0910
#define AFE_CONN5_1                   0x0914
#define AFE_CONN6_1                   0x0918
#define AFE_CONN9_1                   0x0924
#define AFE_CONN10_1                  0x0928
#define AFE_CONN12_1                  0x0930
#define AFE_CONN13_1                  0x0934
#define AFE_CONN14_1                  0x0938
#define AFE_CONN15_1                  0x093c
#define AFE_CONN16_1                  0x0940
#define AFE_CONN17_1                  0x0944
#define AFE_CONN18_1                  0x0948
#define AFE_CONN21_1                  0x0954
#define AFE_CONN22_1                  0x0958
#define AFE_CONN23_1                  0x095c
#define AFE_CONN24_1                  0x0960
#define AFE_CONN25_1                  0x0964
#define AFE_CONN28_1                  0x0970
#define AFE_CONN29_1                  0x0974
#define AFE_CONN32_1                  0x0980
#define AFE_CONN33_1                  0x0984
#define AFE_CONN_RS_1                 0x098c
#define AFE_CONN_DI_1                 0x0990
#define AFE_CONN_24BIT_1              0x0994
#define AFE_CONN_REG                  0x0998
#define AFE_CONN38                    0x09ac
#define AFE_CONN38_1                  0x09bc
#define AFE_CONN39                    0x09c0
#define AFE_CONN39_1                  0x09e0
#define AFE_DL1_BASE_MSB              0x0b00
#define AFE_DL1_CUR_MSB               0x0b04
#define AFE_DL1_END_MSB               0x0b08
#define AFE_DL2_BASE_MSB              0x0b10
#define AFE_DL2_CUR_MSB               0x0b14
#define AFE_DL2_END_MSB               0x0b18
#define AFE_AWB_BASE_MSB              0x0b20
#define AFE_AWB_END_MSB               0x0b28
#define AFE_AWB_CUR_MSB               0x0b2c
#define AFE_VUL_BASE_MSB              0x0b30
#define AFE_VUL_END_MSB               0x0b38
#define AFE_VUL_CUR_MSB               0x0b3c
#define AFE_VUL2_BASE_MSB             0x0b50
#define AFE_VUL2_END_MSB              0x0b58
#define AFE_VUL2_CUR_MSB              0x0b5c
#define AFE_MOD_DAI_BASE_MSB          0x0b60
#define AFE_MOD_DAI_END_MSB           0x0b68
#define AFE_MOD_DAI_CUR_MSB           0x0b6c
#define AFE_DL1_D2_BASE_MSB           0x0b70
#define AFE_DL1_D2_CUR_MSB            0x0b74
#define AFE_DL1_D2_END_MSB            0x0b78
#define AFE_VUL_D2_BASE_MSB           0x0b80
#define AFE_VUL_D2_END_MSB            0x0b88
#define AFE_VUL_D2_CUR_MSB            0x0b8c
#define AFE_DL3_BASE_MSB              0x0b90
#define AFE_DL3_CUR_MSB               0x0b94
#define AFE_DL3_END_MSB               0x0b98
#define AFE_AWB2_BASE                 0x0bd0
#define AFE_AWB2_END                  0x0bd8
#define AFE_AWB2_CUR                  0x0bdc
#define AFE_AWB2_BASE_MSB             0x0be0
#define AFE_AWB2_END_MSB              0x0be8
#define AFE_AWB2_CUR_MSB              0x0bec
#define AFE_ADDA_DL_SDM_DCCOMP_CON    0x0c50
#define AFE_ADDA_DL_SDM_TEST          0x0c54
#define AFE_ADDA_DL_DC_COMP_CFG0      0x0c58
#define AFE_ADDA_DL_DC_COMP_CFG1      0x0c5c
#define AFE_ADDA_DL_SDM_FIFO_MON      0x0c60
#define AFE_ADDA_DL_SRC_LCH_MON       0x0c64
#define AFE_ADDA_DL_SRC_RCH_MON       0x0c68
#define AFE_ADDA_DL_SDM_OUT_MON       0x0c6c
#define AFE_CONNSYS_I2S_CON           0x0c78
#define AFE_CONNSYS_I2S_MON           0x0c7c
#define AFE_ASRC_2CH_CON0             0x0c80
#define AFE_ASRC_2CH_CON1             0x0c84
#define AFE_ASRC_2CH_CON2             0x0c88
#define AFE_ASRC_2CH_CON3             0x0c8c
#define AFE_ASRC_2CH_CON4             0x0c90
#define AFE_ASRC_2CH_CON5             0x0c94
#define AFE_ASRC_2CH_CON6             0x0c98
#define AFE_ASRC_2CH_CON7             0x0c9c
#define AFE_ASRC_2CH_CON8             0x0ca0
#define AFE_ASRC_2CH_CON9             0x0ca4
#define AFE_ASRC_2CH_CON10            0x0ca8
#define AFE_ADDA_PREDIS_CON2          0x0d40
#define AFE_ADDA_PREDIS_CON3          0x0d44
#define AFE_MEMIF_MON12               0x0d70
#define AFE_MEMIF_MON13               0x0d74
#define AFE_MEMIF_MON14               0x0d78
#define AFE_MEMIF_MON15               0x0d7c
#define AFE_MEMIF_MON16               0x0d80
#define AFE_MEMIF_MON17               0x0d84
#define AFE_MEMIF_MON18               0x0d88
#define AFE_MEMIF_MON19               0x0d8c
#define AFE_MEMIF_MON21               0x0d94
#define AFE_MEMIF_MON23               0x0d9c
#define AFE_MEMIF_MON24               0x0da0
#define AFE_HD_ENGEN_ENABLE           0x0dd0
#define AFE_ADDA_MTKAIF_CFG0          0x0e00
#define AFE_ADDA_MTKAIF_TX_CFG1       0x0e14
#define AFE_ADDA_MTKAIF_RX_CFG0       0x0e20
#define AFE_ADDA_MTKAIF_RX_CFG1       0x0e24
#define AFE_ADDA_MTKAIF_RX_CFG2       0x0e28
#define AFE_ADDA_MTKAIF_MON0          0x0e34
#define AFE_ADDA_MTKAIF_MON1          0x0e38
#define AFE_AUD_PAD_TOP               0x0e40

#define AFE_MAX_REGISTER AFE_AUD_PAD_TOP

#define AFE_IRQ_STATUS_BITS 0x1fff
#endif
