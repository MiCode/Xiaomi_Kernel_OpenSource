/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6983-reg.h  --  Mediatek 6983 audio driver reg definition
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Ting-Fang Hou <Ting-Fang.Hou@mediatek.com>
 */

#ifndef _MT6983_REG_H_
#define _MT6983_REG_H_

/* reg bit enum */
enum {
	MT6983_MEMIF_PBUF_SIZE_32_BYTES,
	MT6983_MEMIF_PBUF_SIZE_64_BYTES,
	MT6983_MEMIF_PBUF_SIZE_128_BYTES,
	MT6983_MEMIF_PBUF_SIZE_256_BYTES,
	MT6983_MEMIF_PBUF_SIZE_NUM,
};

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
/* AFE_DAC_CON0 */
#define RESERVED_SFT                                          4
#define RESERVED_MASK                                         0xfffffff
#define RESERVED_MASK_SFT                                     (0xfffffff << 4)
#define AFE_ON_BYPASS_SFT                                     3
#define AFE_ON_BYPASS_MASK                                    0x1
#define AFE_ON_BYPASS_MASK_SFT                                (0x1 << 3)
#define AFE_ON_SW_RESET_SFT                                   2
#define AFE_ON_SW_RESET_MASK                                  0x1
#define AFE_ON_SW_RESET_MASK_SFT                              (0x1 << 2)
#define AFE_ON_SFT                                            0
#define AFE_ON_MASK                                           0x1
#define AFE_ON_MASK_SFT                                       (0x1 << 0)

/* AFE_AGENT_ON */
#define VUL12_ON_SFT                                          31
#define VUL12_ON_MASK                                         0x1
#define VUL12_ON_MASK_SFT                                     (0x1 << 31)
#define MOD_DAI_ON_SFT                                        30
#define MOD_DAI_ON_MASK                                       0x1
#define MOD_DAI_ON_MASK_SFT                                   (0x1 << 30)
#define DAI_ON_SFT                                            29
#define DAI_ON_MASK                                           0x1
#define DAI_ON_MASK_SFT                                       (0x1 << 29)
#define DAI2_ON_SFT                                           28
#define DAI2_ON_MASK                                          0x1
#define DAI2_ON_MASK_SFT                                      (0x1 << 28)
#define VUL9_ON_SFT                                           27
#define VUL9_ON_MASK                                          0x1
#define VUL9_ON_MASK_SFT                                      (0x1 << 27)
#define VUL8_ON_SFT                                           26
#define VUL8_ON_MASK                                          0x1
#define VUL8_ON_MASK_SFT                                      (0x1 << 26)
#define VUL7_2_ON_SFT                                         25
#define VUL7_2_ON_MASK                                        0x1
#define VUL7_2_ON_MASK_SFT                                    (0x1 << 25)
#define VUL7_ON_SFT                                           24
#define VUL7_ON_MASK                                          0x1
#define VUL7_ON_MASK_SFT                                      (0x1 << 24)
#define VUL6_ON_SFT                                           23
#define VUL6_ON_MASK                                          0x1
#define VUL6_ON_MASK_SFT                                      (0x1 << 23)
#define VUL5_ON_SFT                                           22
#define VUL5_ON_MASK                                          0x1
#define VUL5_ON_MASK_SFT                                      (0x1 << 22)
#define VUL4_ON_SFT                                           21
#define VUL4_ON_MASK                                          0x1
#define VUL4_ON_MASK_SFT                                      (0x1 << 21)
#define VUL3_ON_SFT                                           20
#define VUL3_ON_MASK                                          0x1
#define VUL3_ON_MASK_SFT                                      (0x1 << 20)
#define VUL2_ON_SFT                                           19
#define VUL2_ON_MASK                                          0x1
#define VUL2_ON_MASK_SFT                                      (0x1 << 19)
#define VUL_ON_SFT                                            18
#define VUL_ON_MASK                                           0x1
#define VUL_ON_MASK_SFT                                       (0x1 << 18)
#define AWB2_ON_SFT                                           17
#define AWB2_ON_MASK                                          0x1
#define AWB2_ON_MASK_SFT                                      (0x1 << 17)
#define AWB_ON_SFT                                            16
#define AWB_ON_MASK                                           0x1
#define AWB_ON_MASK_SFT                                       (0x1 << 16)
#define DL12_ON_SFT                                           15
#define DL12_ON_MASK                                          0x1
#define DL12_ON_MASK_SFT                                      (0x1 << 15)
#define DL11_ON_SFT                                           14
#define DL11_ON_MASK                                          0x1
#define DL11_ON_MASK_SFT                                      (0x1 << 14)
#define DL10_ON_SFT                                           13
#define DL10_ON_MASK                                          0x1
#define DL10_ON_MASK_SFT                                      (0x1 << 13)
#define DL9_ON_SFT                                            12
#define DL9_ON_MASK                                           0x1
#define DL9_ON_MASK_SFT                                       (0x1 << 12)
#define DL8_ON_SFT                                            11
#define DL8_ON_MASK                                           0x1
#define DL8_ON_MASK_SFT                                       (0x1 << 11)
#define DL7_ON_SFT                                            10
#define DL7_ON_MASK                                           0x1
#define DL7_ON_MASK_SFT                                       (0x1 << 10)
#define DL6_ON_SFT                                            9
#define DL6_ON_MASK                                           0x1
#define DL6_ON_MASK_SFT                                       (0x1 << 9)
#define DL5_ON_SFT                                            8
#define DL5_ON_MASK                                           0x1
#define DL5_ON_MASK_SFT                                       (0x1 << 8)
#define DL4_ON_SFT                                            7
#define DL4_ON_MASK                                           0x1
#define DL4_ON_MASK_SFT                                       (0x1 << 7)
#define DL3_ON_SFT                                            6
#define DL3_ON_MASK                                           0x1
#define DL3_ON_MASK_SFT                                       (0x1 << 6)
#define DL2_ON_SFT                                            5
#define DL2_ON_MASK                                           0x1
#define DL2_ON_MASK_SFT                                       (0x1 << 5)
#define DL1_ON_SFT                                            4
#define DL1_ON_MASK                                           0x1
#define DL1_ON_MASK_SFT                                       (0x1 << 4)
#define HDMI_OUT_ON_SFT                                       1
#define HDMI_OUT_ON_MASK                                      0x1
#define HDMI_OUT_ON_MASK_SFT                                  (0x1 << 1)

/* AFE_DAC_MON */
#define AFE_ON_RETM_SFT                                0
#define AFE_ON_RETM_MASK                               0x1
#define AFE_ON_RETM_MASK_SFT                           (0x1 << 0)

/* AFE_I2S_CON */
#define BCK_NEG_EG_LATCH_SFT                                  30
#define BCK_NEG_EG_LATCH_MASK                                 0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                             (0x1 << 30)
#define BCK_INV_SFT                                           29
#define BCK_INV_MASK                                          0x1
#define BCK_INV_MASK_SFT                                      (0x1 << 29)
#define I2SIN_PAD_SEL_SFT                                     28
#define I2SIN_PAD_SEL_MASK                                    0x1
#define I2SIN_PAD_SEL_MASK_SFT                                (0x1 << 28)
#define I2S_LOOPBACK_SFT                                      20
#define I2S_LOOPBACK_MASK                                     0x1
#define I2S_LOOPBACK_MASK_SFT                                 (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S1_HD_EN_SFT                                        12
#define I2S1_HD_EN_MASK                                       0x1
#define I2S1_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S_OUT_MODE_SFT                                      8
#define I2S_OUT_MODE_MASK                                     0xf
#define I2S_OUT_MODE_MASK_SFT                                 (0xf << 8)
#define INV_PAD_CTRL_SFT                                      7
#define INV_PAD_CTRL_MASK                                     0x1
#define INV_PAD_CTRL_MASK_SFT                                 (0x1 << 7)
#define I2S_BYPSRC_SFT                                        6
#define I2S_BYPSRC_MASK                                       0x1
#define I2S_BYPSRC_MASK_SFT                                   (0x1 << 6)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S_FMT_SFT                                           3
#define I2S_FMT_MASK                                          0x1
#define I2S_FMT_MASK_SFT                                      (0x1 << 3)
#define I2S_SRC_SFT                                           2
#define I2S_SRC_MASK                                          0x1
#define I2S_SRC_MASK_SFT                                      (0x1 << 2)
#define I2S_WLEN_SFT                                          1
#define I2S_WLEN_MASK                                         0x1
#define I2S_WLEN_MASK_SFT                                     (0x1 << 1)
#define I2S_EN_SFT                                            0
#define I2S_EN_MASK                                           0x1
#define I2S_EN_MASK_SFT                                       (0x1 << 0)


/* AFE_I2S_CON1 */
#define I2S2_LR_SWAP_SFT                                      31
#define I2S2_LR_SWAP_MASK                                     0x1
#define I2S2_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S2_SEL_O19_O20_SFT                                  18
#define I2S2_SEL_O19_O20_MASK                                 0x1
#define I2S2_SEL_O19_O20_MASK_SFT                             (0x1 << 18)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S2_SEL_O03_O04_SFT                                  16
#define I2S2_SEL_O03_O04_MASK                                 0x1
#define I2S2_SEL_O03_O04_MASK_SFT                             (0x1 << 16)
#define I2S2_32BIT_EN_SFT                                     13
#define I2S2_32BIT_EN_MASK                                    0x1
#define I2S2_32BIT_EN_MASK_SFT                                (0x1 << 13)
#define I2S2_HD_EN_SFT                                        12
#define I2S2_HD_EN_MASK                                       0x1
#define I2S2_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S2_OUT_MODE_SFT                                     8
#define I2S2_OUT_MODE_MASK                                    0xf
#define I2S2_OUT_MODE_MASK_SFT                                (0xf << 8)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S2_FMT_SFT                                          3
#define I2S2_FMT_MASK                                         0x1
#define I2S2_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S2_WLEN_SFT                                         1
#define I2S2_WLEN_MASK                                        0x1
#define I2S2_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S2_EN_SFT                                           0
#define I2S2_EN_MASK                                          0x1
#define I2S2_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON2 */
#define I2S3_LR_SWAP_SFT                                      31
#define I2S3_LR_SWAP_MASK                                     0x1
#define I2S3_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S3_UPDATE_WORD_SFT                                  24
#define I2S3_UPDATE_WORD_MASK                                 0x1f
#define I2S3_UPDATE_WORD_MASK_SFT                             (0x1f << 24)
#define I2S3_BCK_INV_SFT                                      23
#define I2S3_BCK_INV_MASK                                     0x1
#define I2S3_BCK_INV_MASK_SFT                                 (0x1 << 23)
#define I2S3_FPGA_BIT_TEST_SFT                                22
#define I2S3_FPGA_BIT_TEST_MASK                               0x1
#define I2S3_FPGA_BIT_TEST_MASK_SFT                           (0x1 << 22)
#define I2S3_FPGA_BIT_SFT                                     21
#define I2S3_FPGA_BIT_MASK                                    0x1
#define I2S3_FPGA_BIT_MASK_SFT                                (0x1 << 21)
#define I2S3_LOOPBACK_SFT                                     20
#define I2S3_LOOPBACK_MASK                                    0x1
#define I2S3_LOOPBACK_MASK_SFT                                (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S3_HD_EN_SFT                                        12
#define I2S3_HD_EN_MASK                                       0x1
#define I2S3_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S3_OUT_MODE_SFT                                     8
#define I2S3_OUT_MODE_MASK                                    0xf
#define I2S3_OUT_MODE_MASK_SFT                                (0xf << 8)
#define I2S3_FMT_SFT                                          3
#define I2S3_FMT_MASK                                         0x1
#define I2S3_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S3_WLEN_SFT                                         1
#define I2S3_WLEN_MASK                                        0x1
#define I2S3_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S3_EN_SFT                                           0
#define I2S3_EN_MASK                                          0x1
#define I2S3_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON3 */
#define I2S4_LR_SWAP_SFT                                      31
#define I2S4_LR_SWAP_MASK                                     0x1
#define I2S4_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S4_32BIT_EN_SFT                                     13
#define I2S4_32BIT_EN_MASK                                    0x1
#define I2S4_32BIT_EN_MASK_SFT                                (0x1 << 13)
#define I2S4_HD_EN_SFT                                        12
#define I2S4_HD_EN_MASK                                       0x1
#define I2S4_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S4_OUT_MODE_SFT                                     8
#define I2S4_OUT_MODE_MASK                                    0xf
#define I2S4_OUT_MODE_MASK_SFT                                (0xf << 8)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S4_FMT_SFT                                          3
#define I2S4_FMT_MASK                                         0x1
#define I2S4_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S4_WLEN_SFT                                         1
#define I2S4_WLEN_MASK                                        0x1
#define I2S4_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S4_EN_SFT                                           0
#define I2S4_EN_MASK                                          0x1
#define I2S4_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON4 */
#define I2S5_LR_SWAP_SFT                                      31
#define I2S5_LR_SWAP_MASK                                     0x1
#define I2S5_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S_LOOPBACK_SFT                                      20
#define I2S_LOOPBACK_MASK                                     0x1
#define I2S_LOOPBACK_MASK_SFT                                 (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S5_32BIT_EN_SFT                                     13
#define I2S5_32BIT_EN_MASK                                    0x1
#define I2S5_32BIT_EN_MASK_SFT                                (0x1 << 13)
#define I2S5_HD_EN_SFT                                        12
#define I2S5_HD_EN_MASK                                       0x1
#define I2S5_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S5_OUT_MODE_SFT                                     8
#define I2S5_OUT_MODE_MASK                                    0xf
#define I2S5_OUT_MODE_MASK_SFT                                (0xf << 8)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S5_FMT_SFT                                          3
#define I2S5_FMT_MASK                                         0x1
#define I2S5_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S5_WLEN_SFT                                         1
#define I2S5_WLEN_MASK                                        0x1
#define I2S5_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S5_EN_SFT                                           0
#define I2S5_EN_MASK                                          0x1
#define I2S5_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_CONNSYS_I2S_CON */
#define BCK_NEG_EG_LATCH_SFT                                  30
#define BCK_NEG_EG_LATCH_MASK                                 0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                             (0x1 << 30)
#define BCK_INV_SFT                                           29
#define BCK_INV_MASK                                          0x1
#define BCK_INV_MASK_SFT                                      (0x1 << 29)
#define I2SIN_PAD_SEL_SFT                                     28
#define I2SIN_PAD_SEL_MASK                                    0x1
#define I2SIN_PAD_SEL_MASK_SFT                                (0x1 << 28)
#define I2S_LOOPBACK_SFT                                      20
#define I2S_LOOPBACK_MASK                                     0x1
#define I2S_LOOPBACK_MASK_SFT                                 (0x1 << 20)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S_MODE_SFT                                          8
#define I2S_MODE_MASK                                         0xf
#define I2S_MODE_MASK_SFT                                     (0xf << 8)
#define INV_PAD_CTRL_SFT                                      7
#define INV_PAD_CTRL_MASK                                     0x1
#define INV_PAD_CTRL_MASK_SFT                                 (0x1 << 7)
#define I2S_BYPSRC_SFT                                        6
#define I2S_BYPSRC_MASK                                       0x1
#define I2S_BYPSRC_MASK_SFT                                   (0x1 << 6)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S_FMT_SFT                                           3
#define I2S_FMT_MASK                                          0x1
#define I2S_FMT_MASK_SFT                                      (0x1 << 3)
#define I2S_SRC_SFT                                           2
#define I2S_SRC_MASK                                          0x1
#define I2S_SRC_MASK_SFT                                      (0x1 << 2)
#define I2S_WLEN_SFT                                          1
#define I2S_WLEN_MASK                                         0x1
#define I2S_WLEN_MASK_SFT                                     (0x1 << 1)
#define I2S_EN_SFT                                            0
#define I2S_EN_MASK                                           0x1
#define I2S_EN_MASK_SFT                                       (0x1 << 0)

/* AFE_I2S_CON6 */
#define BCK_NEG_EG_LATCH_SFT                                  30
#define BCK_NEG_EG_LATCH_MASK                                 0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                             (0x1 << 30)
#define BCK_INV_SFT                                           29
#define BCK_INV_MASK                                          0x1
#define BCK_INV_MASK_SFT                                      (0x1 << 29)
#define I2S6_LOOPBACK_SFT                                     20
#define I2S6_LOOPBACK_MASK                                    0x1
#define I2S6_LOOPBACK_MASK_SFT                                (0x1 << 20)
#define I2S6_ONOFF_NOT_RESET_CK_ENABLE_SFT                    17
#define I2S6_ONOFF_NOT_RESET_CK_ENABLE_MASK                   0x1
#define I2S6_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT               (0x1 << 17)
#define I2S6_HD_EN_SFT                                        12
#define I2S6_HD_EN_MASK                                       0x1
#define I2S6_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S6_OUT_MODE_SFT                                     8
#define I2S6_OUT_MODE_MASK                                    0xf
#define I2S6_OUT_MODE_MASK_SFT                                (0xf << 8)
#define I2S6_BYPSRC_SFT                                       6
#define I2S6_BYPSRC_MASK                                      0x1
#define I2S6_BYPSRC_MASK_SFT                                  (0x1 << 6)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S6_FMT_SFT                                          3
#define I2S6_FMT_MASK                                         0x1
#define I2S6_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S6_SRC_SFT                                          2
#define I2S6_SRC_MASK                                         0x1
#define I2S6_SRC_MASK_SFT                                     (0x1 << 2)
#define I2S6_WLEN_SFT                                         1
#define I2S6_WLEN_MASK                                        0x1
#define I2S6_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S6_EN_SFT                                           0
#define I2S6_EN_MASK                                          0x1
#define I2S6_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON7 */
#define I2S7_LR_SWAP_SFT                                      31
#define I2S7_LR_SWAP_MASK                                     0x1
#define I2S7_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S7_32BIT_EN_SFT                                     13
#define I2S7_32BIT_EN_MASK                                    0x1
#define I2S7_32BIT_EN_MASK_SFT                                (0x1 << 13)
#define I2S7_HD_EN_SFT                                        12
#define I2S7_HD_EN_MASK                                       0x1
#define I2S7_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S7_OUT_MODE_SFT                                     8
#define I2S7_OUT_MODE_MASK                                    0xf
#define I2S7_OUT_MODE_MASK_SFT                                (0xf << 8)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S7_FMT_SFT                                          3
#define I2S7_FMT_MASK                                         0x1
#define I2S7_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S7_WLEN_SFT                                         1
#define I2S7_WLEN_MASK                                        0x1
#define I2S7_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S7_EN_SFT                                           0
#define I2S7_EN_MASK                                          0x1
#define I2S7_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON8 */
#define BCK_NEG_EG_LATCH_SFT                                  30
#define BCK_NEG_EG_LATCH_MASK                                 0x1
#define BCK_NEG_EG_LATCH_MASK_SFT                             (0x1 << 30)
#define BCK_INV_SFT                                           29
#define BCK_INV_MASK                                          0x1
#define BCK_INV_MASK_SFT                                      (0x1 << 29)
#define I2S8_LOOPBACK_SFT                                     20
#define I2S8_LOOPBACK_MASK                                    0x1
#define I2S8_LOOPBACK_MASK_SFT                                (0x1 << 20)
#define I2S8_ONOFF_NOT_RESET_CK_ENABLE_SFT                    17
#define I2S8_ONOFF_NOT_RESET_CK_ENABLE_MASK                   0x1
#define I2S8_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT               (0x1 << 17)
#define I2S8_HD_EN_SFT                                        12
#define I2S8_HD_EN_MASK                                       0x1
#define I2S8_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S8_OUT_MODE_SFT                                     8
#define I2S8_OUT_MODE_MASK                                    0xf
#define I2S8_OUT_MODE_MASK_SFT                                (0xf << 8)
#define I2S8_BYPSRC_SFT                                       6
#define I2S8_BYPSRC_MASK                                      0x1
#define I2S8_BYPSRC_MASK_SFT                                  (0x1 << 6)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S8_FMT_SFT                                          3
#define I2S8_FMT_MASK                                         0x1
#define I2S8_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S8_SRC_SFT                                          2
#define I2S8_SRC_MASK                                         0x1
#define I2S8_SRC_MASK_SFT                                     (0x1 << 2)
#define I2S8_WLEN_SFT                                         1
#define I2S8_WLEN_MASK                                        0x1
#define I2S8_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S8_EN_SFT                                           0
#define I2S8_EN_MASK                                          0x1
#define I2S8_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_I2S_CON9 */
#define I2S9_LR_SWAP_SFT                                      31
#define I2S9_LR_SWAP_MASK                                     0x1
#define I2S9_LR_SWAP_MASK_SFT                                 (0x1 << 31)
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_SFT                     17
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK                    0x1
#define I2S_ONOFF_NOT_RESET_CK_ENABLE_MASK_SFT                (0x1 << 17)
#define I2S9_32BIT_EN_SFT                                     13
#define I2S9_32BIT_EN_MASK                                    0x1
#define I2S9_32BIT_EN_MASK_SFT                                (0x1 << 13)
#define I2S9_HD_EN_SFT                                        12
#define I2S9_HD_EN_MASK                                       0x1
#define I2S9_HD_EN_MASK_SFT                                   (0x1 << 12)
#define I2S9_OUT_MODE_SFT                                     8
#define I2S9_OUT_MODE_MASK                                    0xf
#define I2S9_OUT_MODE_MASK_SFT                                (0xf << 8)
#define INV_LRCK_SFT                                          5
#define INV_LRCK_MASK                                         0x1
#define INV_LRCK_MASK_SFT                                     (0x1 << 5)
#define I2S9_FMT_SFT                                          3
#define I2S9_FMT_MASK                                         0x1
#define I2S9_FMT_MASK_SFT                                     (0x1 << 3)
#define I2S9_WLEN_SFT                                         1
#define I2S9_WLEN_MASK                                        0x1
#define I2S9_WLEN_MASK_SFT                                    (0x1 << 1)
#define I2S9_EN_SFT                                           0
#define I2S9_EN_MASK                                          0x1
#define I2S9_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_ASRC_2CH_CON2 */
#define CHSET_O16BIT_SFT                                      19
#define CHSET_O16BIT_MASK                                     0x1
#define CHSET_O16BIT_MASK_SFT                                 (0x1 << 19)
#define CHSET_CLR_IIR_HISTORY_SFT                             17
#define CHSET_CLR_IIR_HISTORY_MASK                            0x1
#define CHSET_CLR_IIR_HISTORY_MASK_SFT                        (0x1 << 17)
#define CHSET_IS_MONO_SFT                                     16
#define CHSET_IS_MONO_MASK                                    0x1
#define CHSET_IS_MONO_MASK_SFT                                (0x1 << 16)
#define CHSET_IIR_EN_SFT                                      11
#define CHSET_IIR_EN_MASK                                     0x1
#define CHSET_IIR_EN_MASK_SFT                                 (0x1 << 11)
#define CHSET_IIR_STAGE_SFT                                   8
#define CHSET_IIR_STAGE_MASK                                  0x7
#define CHSET_IIR_STAGE_MASK_SFT                              (0x7 << 8)
#define CHSET_STR_CLR_SFT                                     5
#define CHSET_STR_CLR_MASK                                    0x1
#define CHSET_STR_CLR_MASK_SFT                                (0x1 << 5)
#define CHSET_ON_SFT                                          2
#define CHSET_ON_MASK                                         0x1
#define CHSET_ON_MASK_SFT                                     (0x1 << 2)
#define COEFF_SRAM_CTRL_SFT                                   1
#define COEFF_SRAM_CTRL_MASK                                  0x1
#define COEFF_SRAM_CTRL_MASK_SFT                              (0x1 << 1)
#define ASM_ON_SFT                                            0
#define ASM_ON_MASK                                           0x1
#define ASM_ON_MASK_SFT                                       (0x1 << 0)

/* AFE_GAIN1_CON0 */
#define GAIN1_SAMPLE_PER_STEP_SFT                             8
#define GAIN1_SAMPLE_PER_STEP_MASK                            0xff
#define GAIN1_SAMPLE_PER_STEP_MASK_SFT                        (0xff << 8)
#define GAIN1_MODE_SFT                                        4
#define GAIN1_MODE_MASK                                       0xf
#define GAIN1_MODE_MASK_SFT                                   (0xf << 4)
#define GAIN1_ON_SFT                                          0
#define GAIN1_ON_MASK                                         0x1
#define GAIN1_ON_MASK_SFT                                     (0x1 << 0)

/* AFE_GAIN1_CON1 */
#define GAIN1_TARGET_SFT                                      0
#define GAIN1_TARGET_MASK                                     0xfffffff
#define GAIN1_TARGET_MASK_SFT                                 (0xfffffff << 0)

/* AFE_GAIN2_CON0 */
#define GAIN2_SAMPLE_PER_STEP_SFT                             8
#define GAIN2_SAMPLE_PER_STEP_MASK                            0xff
#define GAIN2_SAMPLE_PER_STEP_MASK_SFT                        (0xff << 8)
#define GAIN2_MODE_SFT                                        4
#define GAIN2_MODE_MASK                                       0xf
#define GAIN2_MODE_MASK_SFT                                   (0xf << 4)
#define GAIN2_ON_SFT                                          0
#define GAIN2_ON_MASK                                         0x1
#define GAIN2_ON_MASK_SFT                                     (0x1 << 0)

/* AFE_GAIN2_CON1 */
#define GAIN2_TARGET_SFT                                      0
#define GAIN2_TARGET_MASK                                     0xfffffff
#define GAIN2_TARGET_MASK_SFT                                 (0xfffffff << 0)

/* AFE_GAIN3_CON0 */
#define GAIN3_SAMPLE_PER_STEP_SFT                             8
#define GAIN3_SAMPLE_PER_STEP_MASK                            0xff
#define GAIN3_SAMPLE_PER_STEP_MASK_SFT                        (0xff << 8)
#define GAIN3_MODE_SFT                                        4
#define GAIN3_MODE_MASK                                       0xf
#define GAIN3_MODE_MASK_SFT                                   (0xf << 4)
#define GAIN3_ON_SFT                                          0
#define GAIN3_ON_MASK                                         0x1
#define GAIN3_ON_MASK_SFT                                     (0x1 << 0)

/* AFE_GAIN3_CON1 */
#define GAIN3_TARGET_SFT                                      0
#define GAIN3_TARGET_MASK                                     0xfffffff
#define GAIN3_TARGET_MASK_SFT                                 (0xfffffff << 0)

/* AFE_GAIN4_CON0 */
#define GAIN4_SAMPLE_PER_STEP_SFT                             8
#define GAIN4_SAMPLE_PER_STEP_MASK                            0xff
#define GAIN4_SAMPLE_PER_STEP_MASK_SFT                        (0xff << 8)
#define GAIN4_MODE_SFT                                        4
#define GAIN4_MODE_MASK                                       0xf
#define GAIN4_MODE_MASK_SFT                                   (0xf << 4)
#define GAIN4_ON_SFT                                          0
#define GAIN4_ON_MASK                                         0x1
#define GAIN4_ON_MASK_SFT                                     (0x1 << 0)

/* AFE_GAIN4_CON1 */
#define GAIN4_TARGET_SFT                                      0
#define GAIN4_TARGET_MASK                                     0xfffffff
#define GAIN4_TARGET_MASK_SFT                                 (0xfffffff << 0)

/* AFE_GAIN1_CUR */
#define AFE_GAIN1_CUR_SFT                                     0
#define AFE_GAIN1_CUR_MASK                                    0xfffffff
#define AFE_GAIN1_CUR_MASK_SFT                                (0xfffffff << 0)

/* AFE_GAIN2_CUR */
#define AFE_GAIN2_CUR_SFT                                     0
#define AFE_GAIN2_CUR_MASK                                    0xfffffff
#define AFE_GAIN2_CUR_MASK_SFT                                (0xfffffff << 0)

/* AFE_GAIN3_CUR */
#define AFE_GAIN3_CUR_SFT                                     0
#define AFE_GAIN3_CUR_MASK                                    0xfffffff
#define AFE_GAIN3_CUR_MASK_SFT                                (0xfffffff << 0)

/* AFE_GAIN4_CUR */
#define AFE_GAIN4_CUR_SFT                                     0
#define AFE_GAIN4_CUR_MASK                                    0xfffffff
#define AFE_GAIN4_CUR_MASK_SFT                                (0xfffffff << 0)

/* PCM_INTF_CON1 */
#define PCM_FIX_VALUE_SEL_SFT                                 31
#define PCM_FIX_VALUE_SEL_MASK                                0x1
#define PCM_FIX_VALUE_SEL_MASK_SFT                            (0x1 << 31)
#define PCM_BUFFER_LOOPBACK_SFT                               30
#define PCM_BUFFER_LOOPBACK_MASK                              0x1
#define PCM_BUFFER_LOOPBACK_MASK_SFT                          (0x1 << 30)
#define PCM_PARALLEL_LOOPBACK_SFT                             29
#define PCM_PARALLEL_LOOPBACK_MASK                            0x1
#define PCM_PARALLEL_LOOPBACK_MASK_SFT                        (0x1 << 29)
#define PCM_SERIAL_LOOPBACK_SFT                               28
#define PCM_SERIAL_LOOPBACK_MASK                              0x1
#define PCM_SERIAL_LOOPBACK_MASK_SFT                          (0x1 << 28)
#define PCM_DAI_PCM_LOOPBACK_SFT                              27
#define PCM_DAI_PCM_LOOPBACK_MASK                             0x1
#define PCM_DAI_PCM_LOOPBACK_MASK_SFT                         (0x1 << 27)
#define PCM_I2S_PCM_LOOPBACK_SFT                              26
#define PCM_I2S_PCM_LOOPBACK_MASK                             0x1
#define PCM_I2S_PCM_LOOPBACK_MASK_SFT                         (0x1 << 26)
#define PCM_SYNC_DELSEL_SFT                                   25
#define PCM_SYNC_DELSEL_MASK                                  0x1
#define PCM_SYNC_DELSEL_MASK_SFT                              (0x1 << 25)
#define PCM_TX_LR_SWAP_SFT                                    24
#define PCM_TX_LR_SWAP_MASK                                   0x1
#define PCM_TX_LR_SWAP_MASK_SFT                               (0x1 << 24)
#define PCM_SYNC_OUT_INV_SFT                                  23
#define PCM_SYNC_OUT_INV_MASK                                 0x1
#define PCM_SYNC_OUT_INV_MASK_SFT                             (0x1 << 23)
#define PCM_BCLK_OUT_INV_SFT                                  22
#define PCM_BCLK_OUT_INV_MASK                                 0x1
#define PCM_BCLK_OUT_INV_MASK_SFT                             (0x1 << 22)
#define PCM_SYNC_IN_INV_SFT                                   21
#define PCM_SYNC_IN_INV_MASK                                  0x1
#define PCM_SYNC_IN_INV_MASK_SFT                              (0x1 << 21)
#define PCM_BCLK_IN_INV_SFT                                   20
#define PCM_BCLK_IN_INV_MASK                                  0x1
#define PCM_BCLK_IN_INV_MASK_SFT                              (0x1 << 20)
#define PCM_TX_LCH_RPT_SFT                                    19
#define PCM_TX_LCH_RPT_MASK                                   0x1
#define PCM_TX_LCH_RPT_MASK_SFT                               (0x1 << 19)
#define PCM_VBT_16K_MODE_SFT                                  18
#define PCM_VBT_16K_MODE_MASK                                 0x1
#define PCM_VBT_16K_MODE_MASK_SFT                             (0x1 << 18)
#define PCM_EXT_MODEM_SFT                                     17
#define PCM_EXT_MODEM_MASK                                    0x1
#define PCM_EXT_MODEM_MASK_SFT                                (0x1 << 17)
#define PCM_24BIT_SFT                                         16
#define PCM_24BIT_MASK                                        0x1
#define PCM_24BIT_MASK_SFT                                    (0x1 << 16)
#define PCM_WLEN_SFT                                          14
#define PCM_WLEN_MASK                                         0x3
#define PCM_WLEN_MASK_SFT                                     (0x3 << 14)
#define PCM_SYNC_LENGTH_SFT                                   9
#define PCM_SYNC_LENGTH_MASK                                  0x1f
#define PCM_SYNC_LENGTH_MASK_SFT                              (0x1f << 9)
#define PCM_SYNC_TYPE_SFT                                     8
#define PCM_SYNC_TYPE_MASK                                    0x1
#define PCM_SYNC_TYPE_MASK_SFT                                (0x1 << 8)
#define PCM_BT_MODE_SFT                                       7
#define PCM_BT_MODE_MASK                                      0x1
#define PCM_BT_MODE_MASK_SFT                                  (0x1 << 7)
#define PCM_BYP_ASRC_SFT                                      6
#define PCM_BYP_ASRC_MASK                                     0x1
#define PCM_BYP_ASRC_MASK_SFT                                 (0x1 << 6)
#define PCM_SLAVE_SFT                                         5
#define PCM_SLAVE_MASK                                        0x1
#define PCM_SLAVE_MASK_SFT                                    (0x1 << 5)
#define PCM_MODE_SFT                                          3
#define PCM_MODE_MASK                                         0x3
#define PCM_MODE_MASK_SFT                                     (0x3 << 3)
#define PCM_FMT_SFT                                           1
#define PCM_FMT_MASK                                          0x3
#define PCM_FMT_MASK_SFT                                      (0x3 << 1)
#define PCM_EN_SFT                                            0
#define PCM_EN_MASK                                           0x1
#define PCM_EN_MASK_SFT                                       (0x1 << 0)

/* PCM_INTF_CON2 */
#define PCM1_TX_FIFO_OV_SFT                                   31
#define PCM1_TX_FIFO_OV_MASK                                  0x1
#define PCM1_TX_FIFO_OV_MASK_SFT                              (0x1 << 31)
#define PCM1_RX_FIFO_OV_SFT                                   30
#define PCM1_RX_FIFO_OV_MASK                                  0x1
#define PCM1_RX_FIFO_OV_MASK_SFT                              (0x1 << 30)
#define PCM2_TX_FIFO_OV_SFT                                   29
#define PCM2_TX_FIFO_OV_MASK                                  0x1
#define PCM2_TX_FIFO_OV_MASK_SFT                              (0x1 << 29)
#define PCM2_RX_FIFO_OV_SFT                                   28
#define PCM2_RX_FIFO_OV_MASK                                  0x1
#define PCM2_RX_FIFO_OV_MASK_SFT                              (0x1 << 28)
#define PCM1_SYNC_GLITCH_SFT                                  27
#define PCM1_SYNC_GLITCH_MASK                                 0x1
#define PCM1_SYNC_GLITCH_MASK_SFT                             (0x1 << 27)
#define PCM2_SYNC_GLITCH_SFT                                  26
#define PCM2_SYNC_GLITCH_MASK                                 0x1
#define PCM2_SYNC_GLITCH_MASK_SFT                             (0x1 << 26)
#define TX3_RCH_DBG_MODE_SFT                                  17
#define TX3_RCH_DBG_MODE_MASK                                 0x1
#define TX3_RCH_DBG_MODE_MASK_SFT                             (0x1 << 17)
#define PCM1_PCM2_LOOPBACK_SFT                                16
#define PCM1_PCM2_LOOPBACK_MASK                               0x1
#define PCM1_PCM2_LOOPBACK_MASK_SFT                           (0x1 << 16)
#define I2S_PCM_LOOPBACK_CH_SFT                               12
#define I2S_PCM_LOOPBACK_CH_MASK                              0x3
#define I2S_PCM_LOOPBACK_CH_MASK_SFT                          (0x3 << 12)
#define TX_FIX_VALUE_SFT                                      0
#define TX_FIX_VALUE_MASK                                     0xff
#define TX_FIX_VALUE_MASK_SFT                                 (0xff << 0)

/* PCM2_INTF_CON */
#define PCM2_TX_FIX_VALUE_SFT                                 24
#define PCM2_TX_FIX_VALUE_MASK                                0xff
#define PCM2_TX_FIX_VALUE_MASK_SFT                            (0xff << 24)
#define PCM2_FIX_VALUE_SEL_SFT                                23
#define PCM2_FIX_VALUE_SEL_MASK                               0x1
#define PCM2_FIX_VALUE_SEL_MASK_SFT                           (0x1 << 23)
#define PCM2_BUFFER_LOOPBACK_SFT                              22
#define PCM2_BUFFER_LOOPBACK_MASK                             0x1
#define PCM2_BUFFER_LOOPBACK_MASK_SFT                         (0x1 << 22)
#define PCM2_PARALLEL_LOOPBACK_SFT                            21
#define PCM2_PARALLEL_LOOPBACK_MASK                           0x1
#define PCM2_PARALLEL_LOOPBACK_MASK_SFT                       (0x1 << 21)
#define PCM2_SERIAL_LOOPBACK_SFT                              20
#define PCM2_SERIAL_LOOPBACK_MASK                             0x1
#define PCM2_SERIAL_LOOPBACK_MASK_SFT                         (0x1 << 20)
#define PCM2_DAI_PCM_LOOPBACK_SFT                             19
#define PCM2_DAI_PCM_LOOPBACK_MASK                            0x1
#define PCM2_DAI_PCM_LOOPBACK_MASK_SFT                        (0x1 << 19)
#define PCM2_I2S_PCM_LOOPBACK_SFT                             18
#define PCM2_I2S_PCM_LOOPBACK_MASK                            0x1
#define PCM2_I2S_PCM_LOOPBACK_MASK_SFT                        (0x1 << 18)
#define PCM2_SYNC_DELSEL_SFT                                  17
#define PCM2_SYNC_DELSEL_MASK                                 0x1
#define PCM2_SYNC_DELSEL_MASK_SFT                             (0x1 << 17)
#define PCM2_TX_LR_SWAP_SFT                                   16
#define PCM2_TX_LR_SWAP_MASK                                  0x1
#define PCM2_TX_LR_SWAP_MASK_SFT                              (0x1 << 16)
#define PCM2_SYNC_IN_INV_SFT                                  15
#define PCM2_SYNC_IN_INV_MASK                                 0x1
#define PCM2_SYNC_IN_INV_MASK_SFT                             (0x1 << 15)
#define PCM2_BCLK_IN_INV_SFT                                  14
#define PCM2_BCLK_IN_INV_MASK                                 0x1
#define PCM2_BCLK_IN_INV_MASK_SFT                             (0x1 << 14)
#define PCM2_TX_LCH_RPT_SFT                                   13
#define PCM2_TX_LCH_RPT_MASK                                  0x1
#define PCM2_TX_LCH_RPT_MASK_SFT                              (0x1 << 13)
#define PCM2_VBT_16K_MODE_SFT                                 12
#define PCM2_VBT_16K_MODE_MASK                                0x1
#define PCM2_VBT_16K_MODE_MASK_SFT                            (0x1 << 12)
#define PCM2_LOOPBACK_CH_SEL_SFT                              10
#define PCM2_LOOPBACK_CH_SEL_MASK                             0x3
#define PCM2_LOOPBACK_CH_SEL_MASK_SFT                         (0x3 << 10)
#define PCM2_TX2_BT_MODE_SFT                                  8
#define PCM2_TX2_BT_MODE_MASK                                 0x1
#define PCM2_TX2_BT_MODE_MASK_SFT                             (0x1 << 8)
#define PCM2_BT_MODE_SFT                                      7
#define PCM2_BT_MODE_MASK                                     0x1
#define PCM2_BT_MODE_MASK_SFT                                 (0x1 << 7)
#define PCM2_AFIFO_SFT                                        6
#define PCM2_AFIFO_MASK                                       0x1
#define PCM2_AFIFO_MASK_SFT                                   (0x1 << 6)
#define PCM2_WLEN_SFT                                         5
#define PCM2_WLEN_MASK                                        0x1
#define PCM2_WLEN_MASK_SFT                                    (0x1 << 5)
#define PCM2_MODE_SFT                                         3
#define PCM2_MODE_MASK                                        0x3
#define PCM2_MODE_MASK_SFT                                    (0x3 << 3)
#define PCM2_FMT_SFT                                          1
#define PCM2_FMT_MASK                                         0x3
#define PCM2_FMT_MASK_SFT                                     (0x3 << 1)
#define PCM2_EN_SFT                                           0
#define PCM2_EN_MASK                                          0x1
#define PCM2_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_ADDA_MTKAIF_CFG0 */
#define MTKAIF_RXIF_CLKINV_ADC_SFT                            31
#define MTKAIF_RXIF_CLKINV_ADC_MASK                           0x1
#define MTKAIF_RXIF_CLKINV_ADC_MASK_SFT                       (0x1 << 31)
#define MTKAIF_RXIF_BYPASS_SRC_SFT                            17
#define MTKAIF_RXIF_BYPASS_SRC_MASK                           0x1
#define MTKAIF_RXIF_BYPASS_SRC_MASK_SFT                       (0x1 << 17)
#define MTKAIF_RXIF_PROTOCOL2_SFT                             16
#define MTKAIF_RXIF_PROTOCOL2_MASK                            0x1
#define MTKAIF_RXIF_PROTOCOL2_MASK_SFT                        (0x1 << 16)
#define MTKAIF_TXIF_NLE_DEBUG_SFT                             8
#define MTKAIF_TXIF_NLE_DEBUG_MASK                            0x1
#define MTKAIF_TXIF_NLE_DEBUG_MASK_SFT                        (0x1 << 8)
#define MTKAIF_TXIF_BYPASS_SRC_SFT                            5
#define MTKAIF_TXIF_BYPASS_SRC_MASK                           0x1
#define MTKAIF_TXIF_BYPASS_SRC_MASK_SFT                       (0x1 << 5)
#define MTKAIF_TXIF_PROTOCOL2_SFT                             4
#define MTKAIF_TXIF_PROTOCOL2_MASK                            0x1
#define MTKAIF_TXIF_PROTOCOL2_MASK_SFT                        (0x1 << 4)
#define MTKAIF_TXIF_8TO5_SFT                                  2
#define MTKAIF_TXIF_8TO5_MASK                                 0x1
#define MTKAIF_TXIF_8TO5_MASK_SFT                             (0x1 << 2)
#define MTKAIF_RXIF_8TO5_SFT                                  1
#define MTKAIF_RXIF_8TO5_MASK                                 0x1
#define MTKAIF_RXIF_8TO5_MASK_SFT                             (0x1 << 1)
#define MTKAIF_IF_LOOPBACK1_SFT                               0
#define MTKAIF_IF_LOOPBACK1_MASK                              0x1
#define MTKAIF_IF_LOOPBACK1_MASK_SFT                          (0x1 << 0)

/* AFE_ADDA_MTKAIF_RX_CFG2 */
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_SFT                   16
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK                  0x1
#define MTKAIF_RXIF_DETECT_ON_PROTOCOL2_MASK_SFT              (0x1 << 16)
#define MTKAIF_RXIF_DELAY_CYCLE_SFT                           12
#define MTKAIF_RXIF_DELAY_CYCLE_MASK                          0xf
#define MTKAIF_RXIF_DELAY_CYCLE_MASK_SFT                      (0xf << 12)
#define MTKAIF_RXIF_DELAY_DATA_SFT                            8
#define MTKAIF_RXIF_DELAY_DATA_MASK                           0x1
#define MTKAIF_RXIF_DELAY_DATA_MASK_SFT                       (0x1 << 8)
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_SFT                    4
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK                   0x7
#define MTKAIF_RXIF_FIFO_RSP_PROTOCOL2_MASK_SFT               (0x7 << 4)

/* AFE_ADDA_DL_SRC2_CON0 */
#define DL_2_INPUT_MODE_CTL_SFT                               28
#define DL_2_INPUT_MODE_CTL_MASK                              0xf
#define DL_2_INPUT_MODE_CTL_MASK_SFT                          (0xf << 28)
#define DL_2_CH1_SATURATION_EN_CTL_SFT                        27
#define DL_2_CH1_SATURATION_EN_CTL_MASK                       0x1
#define DL_2_CH1_SATURATION_EN_CTL_MASK_SFT                   (0x1 << 27)
#define DL_2_CH2_SATURATION_EN_CTL_SFT                        26
#define DL_2_CH2_SATURATION_EN_CTL_MASK                       0x1
#define DL_2_CH2_SATURATION_EN_CTL_MASK_SFT                   (0x1 << 26)
#define DL_2_OUTPUT_SEL_CTL_SFT                               24
#define DL_2_OUTPUT_SEL_CTL_MASK                              0x3
#define DL_2_OUTPUT_SEL_CTL_MASK_SFT                          (0x3 << 24)
#define DL_2_FADEIN_0START_EN_SFT                             16
#define DL_2_FADEIN_0START_EN_MASK                            0x3
#define DL_2_FADEIN_0START_EN_MASK_SFT                        (0x3 << 16)
#define DL_DISABLE_HW_CG_CTL_SFT                              15
#define DL_DISABLE_HW_CG_CTL_MASK                             0x1
#define DL_DISABLE_HW_CG_CTL_MASK_SFT                         (0x1 << 15)
#define C_DATA_EN_SEL_CTL_PRE_SFT                             14
#define C_DATA_EN_SEL_CTL_PRE_MASK                            0x1
#define C_DATA_EN_SEL_CTL_PRE_MASK_SFT                        (0x1 << 14)
#define DL_2_SIDE_TONE_ON_CTL_PRE_SFT                         13
#define DL_2_SIDE_TONE_ON_CTL_PRE_MASK                        0x1
#define DL_2_SIDE_TONE_ON_CTL_PRE_MASK_SFT                    (0x1 << 13)
#define DL_2_MUTE_CH1_OFF_CTL_PRE_SFT                         12
#define DL_2_MUTE_CH1_OFF_CTL_PRE_MASK                        0x1
#define DL_2_MUTE_CH1_OFF_CTL_PRE_MASK_SFT                    (0x1 << 12)
#define DL_2_MUTE_CH2_OFF_CTL_PRE_SFT                         11
#define DL_2_MUTE_CH2_OFF_CTL_PRE_MASK                        0x1
#define DL_2_MUTE_CH2_OFF_CTL_PRE_MASK_SFT                    (0x1 << 11)
#define DL2_ARAMPSP_CTL_PRE_SFT                               9
#define DL2_ARAMPSP_CTL_PRE_MASK                              0x3
#define DL2_ARAMPSP_CTL_PRE_MASK_SFT                          (0x3 << 9)
#define DL_2_IIRMODE_CTL_PRE_SFT                              6
#define DL_2_IIRMODE_CTL_PRE_MASK                             0x7
#define DL_2_IIRMODE_CTL_PRE_MASK_SFT                         (0x7 << 6)
#define DL_2_VOICE_MODE_CTL_PRE_SFT                           5
#define DL_2_VOICE_MODE_CTL_PRE_MASK                          0x1
#define DL_2_VOICE_MODE_CTL_PRE_MASK_SFT                      (0x1 << 5)
#define D2_2_MUTE_CH1_ON_CTL_PRE_SFT                          4
#define D2_2_MUTE_CH1_ON_CTL_PRE_MASK                         0x1
#define D2_2_MUTE_CH1_ON_CTL_PRE_MASK_SFT                     (0x1 << 4)
#define D2_2_MUTE_CH2_ON_CTL_PRE_SFT                          3
#define D2_2_MUTE_CH2_ON_CTL_PRE_MASK                         0x1
#define D2_2_MUTE_CH2_ON_CTL_PRE_MASK_SFT                     (0x1 << 3)
#define DL_2_IIR_ON_CTL_PRE_SFT                               2
#define DL_2_IIR_ON_CTL_PRE_MASK                              0x1
#define DL_2_IIR_ON_CTL_PRE_MASK_SFT                          (0x1 << 2)
#define DL_2_GAIN_ON_CTL_PRE_SFT                              1
#define DL_2_GAIN_ON_CTL_PRE_MASK                             0x1
#define DL_2_GAIN_ON_CTL_PRE_MASK_SFT                         (0x1 << 1)
#define DL_2_SRC_ON_TMP_CTL_PRE_SFT                           0
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK                          0x1
#define DL_2_SRC_ON_TMP_CTL_PRE_MASK_SFT                      (0x1 << 0)

/* AFE_ADDA_DL_SRC2_CON1 */
#define DL_2_GAIN_CTL_PRE_SFT                                 16
#define DL_2_GAIN_CTL_PRE_MASK                                0xffff
#define DL_2_GAIN_CTL_PRE_MASK_SFT                            (0xffff << 16)
#define DL_2_GAIN_MODE_CTL_SFT                                0
#define DL_2_GAIN_MODE_CTL_MASK                               0x1
#define DL_2_GAIN_MODE_CTL_MASK_SFT                           (0x1 << 0)

/* AFE_ADDA_UL_SRC_CON0 */
#define ULCF_CFG_EN_CTL_SFT                                   31
#define ULCF_CFG_EN_CTL_MASK                                  0x1
#define ULCF_CFG_EN_CTL_MASK_SFT                              (0x1 << 31)
#define UL_DMIC_PHASE_SEL_CH1_SFT                             27
#define UL_DMIC_PHASE_SEL_CH1_MASK                            0x7
#define UL_DMIC_PHASE_SEL_CH1_MASK_SFT                        (0x7 << 27)
#define UL_DMIC_PHASE_SEL_CH2_SFT                             24
#define UL_DMIC_PHASE_SEL_CH2_MASK                            0x7
#define UL_DMIC_PHASE_SEL_CH2_MASK_SFT                        (0x7 << 24)
#define UL_MODE_3P25M_CH2_CTL_SFT                             22
#define UL_MODE_3P25M_CH2_CTL_MASK                            0x1
#define UL_MODE_3P25M_CH2_CTL_MASK_SFT                        (0x1 << 22)
#define UL_MODE_3P25M_CH1_CTL_SFT                             21
#define UL_MODE_3P25M_CH1_CTL_MASK                            0x1
#define UL_MODE_3P25M_CH1_CTL_MASK_SFT                        (0x1 << 21)
#define UL_VOICE_MODE_CH1_CH2_CTL_SFT                         17
#define UL_VOICE_MODE_CH1_CH2_CTL_MASK                        0x7
#define UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT                    (0x7 << 17)
#define UL_AP_DMIC_ON_SFT                                     16
#define UL_AP_DMIC_ON_MASK                                    0x1
#define UL_AP_DMIC_ON_MASK_SFT                                (0x1 << 16)
#define DMIC_LOW_POWER_MODE_CTL_SFT                           14
#define DMIC_LOW_POWER_MODE_CTL_MASK                          0x3
#define DMIC_LOW_POWER_MODE_CTL_MASK_SFT                      (0x3 << 14)
#define UL_DISABLE_HW_CG_CTL_SFT                              12
#define UL_DISABLE_HW_CG_CTL_MASK                             0x1
#define UL_DISABLE_HW_CG_CTL_MASK_SFT                         (0x1 << 12)
#define UL_IIR_ON_TMP_CTL_SFT                                 10
#define UL_IIR_ON_TMP_CTL_MASK                                0x1
#define UL_IIR_ON_TMP_CTL_MASK_SFT                            (0x1 << 10)
#define UL_IIRMODE_CTL_SFT                                    7
#define UL_IIRMODE_CTL_MASK                                   0x7
#define UL_IIRMODE_CTL_MASK_SFT                               (0x7 << 7)
#define DIGMIC_4P33M_SEL_SFT                                  6
#define DIGMIC_4P33M_SEL_MASK                                 0x1
#define DIGMIC_4P33M_SEL_MASK_SFT                             (0x1 << 6)
#define DIGMIC_3P25M_1P625M_SEL_CTL_SFT                       5
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK                      0x1
#define DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT                  (0x1 << 5)
#define UL_LOOP_BACK_MODE_CTL_SFT                             2
#define UL_LOOP_BACK_MODE_CTL_MASK                            0x1
#define UL_LOOP_BACK_MODE_CTL_MASK_SFT                        (0x1 << 2)
#define UL_SDM_3_LEVEL_CTL_SFT                                1
#define UL_SDM_3_LEVEL_CTL_MASK                               0x1
#define UL_SDM_3_LEVEL_CTL_MASK_SFT                           (0x1 << 1)
#define UL_SRC_ON_TMP_CTL_SFT                                 0
#define UL_SRC_ON_TMP_CTL_MASK                                0x1
#define UL_SRC_ON_TMP_CTL_MASK_SFT                            (0x1 << 0)

/* AFE_ADDA_UL_SRC_CON1 */
#define C_DAC_EN_CTL_SFT                                      27
#define C_DAC_EN_CTL_MASK                                     0x1
#define C_DAC_EN_CTL_MASK_SFT                                 (0x1 << 27)
#define C_MUTE_SW_CTL_SFT                                     26
#define C_MUTE_SW_CTL_MASK                                    0x1
#define C_MUTE_SW_CTL_MASK_SFT                                (0x1 << 26)
#define ASDM_SRC_SEL_CTL_SFT                                  25
#define ASDM_SRC_SEL_CTL_MASK                                 0x1
#define ASDM_SRC_SEL_CTL_MASK_SFT                             (0x1 << 25)
#define C_AMP_DIV_CH2_CTL_SFT                                 21
#define C_AMP_DIV_CH2_CTL_MASK                                0x7
#define C_AMP_DIV_CH2_CTL_MASK_SFT                            (0x7 << 21)
#define C_FREQ_DIV_CH2_CTL_SFT                                16
#define C_FREQ_DIV_CH2_CTL_MASK                               0x1f
#define C_FREQ_DIV_CH2_CTL_MASK_SFT                           (0x1f << 16)
#define C_SINE_MODE_CH2_CTL_SFT                               12
#define C_SINE_MODE_CH2_CTL_MASK                              0xf
#define C_SINE_MODE_CH2_CTL_MASK_SFT                          (0xf << 12)
#define C_AMP_DIV_CH1_CTL_SFT                                 9
#define C_AMP_DIV_CH1_CTL_MASK                                0x7
#define C_AMP_DIV_CH1_CTL_MASK_SFT                            (0x7 << 9)
#define C_FREQ_DIV_CH1_CTL_SFT                                4
#define C_FREQ_DIV_CH1_CTL_MASK                               0x1f
#define C_FREQ_DIV_CH1_CTL_MASK_SFT                           (0x1f << 4)
#define C_SINE_MODE_CH1_CTL_SFT                               0
#define C_SINE_MODE_CH1_CTL_MASK                              0xf
#define C_SINE_MODE_CH1_CTL_MASK_SFT                          (0xf << 0)

/* AFE_ADDA_TOP_CON0 */
#define C_LOOP_BACK_MODE_CTL_SFT                              12
#define C_LOOP_BACK_MODE_CTL_MASK                             0xf
#define C_LOOP_BACK_MODE_CTL_MASK_SFT                         (0xf << 12)
#define ADDA_UL_GAIN_MODE_SFT                                 8
#define ADDA_UL_GAIN_MODE_MASK                                0x3
#define ADDA_UL_GAIN_MODE_MASK_SFT                            (0x3 << 8)
#define C_EXT_ADC_CTL_SFT                                     0
#define C_EXT_ADC_CTL_MASK                                    0x1
#define C_EXT_ADC_CTL_MASK_SFT                                (0x1 << 0)

/* AFE_ADDA_UL_DL_CON0 */
#define AFE_ADDA_UL_LR_SWAP_SFT                               31
#define AFE_ADDA_UL_LR_SWAP_MASK                              0x1
#define AFE_ADDA_UL_LR_SWAP_MASK_SFT                          (0x1 << 31)
#define AFE_ADDA_CKDIV_RST_SFT                                30
#define AFE_ADDA_CKDIV_RST_MASK                               0x1
#define AFE_ADDA_CKDIV_RST_MASK_SFT                           (0x1 << 30)
#define AFE_ADDA_FIFO_AUTO_RST_EN_SFT                         29
#define AFE_ADDA_FIFO_AUTO_RST_EN_MASK                        0x1
#define AFE_ADDA_FIFO_AUTO_RST_EN_MASK_SFT                    (0x1 << 29)
#define AFE_ADDA_FIFO_AUTO_RST_SFT                            28
#define AFE_ADDA_FIFO_AUTO_RST_MASK                           0x1
#define AFE_ADDA_FIFO_AUTO_RST_MASK_SFT                       (0x1 << 28)
#define AFE_ADDA_UL_FIFO_DIGMIC_TESTIN_SFT                    25
#define AFE_ADDA_UL_FIFO_DIGMIC_TESTIN_MASK                   0x3
#define AFE_ADDA_UL_FIFO_DIGMIC_TESTIN_MASK_SFT               (0x3 << 25)
#define AFE_ADDA_UL_FIFO_DIGMIC_WDATA_TESTEN_SFT              24
#define AFE_ADDA_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK             0x1
#define AFE_ADDA_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK_SFT         (0x1 << 24)
#define AFE_ADDA7_UL_LR_SWAP_SFT                              23
#define AFE_ADDA7_UL_LR_SWAP_MASK                             0x1
#define AFE_ADDA7_UL_LR_SWAP_MASK_SFT                         (0x1 << 23)
#define AFE_ADDA7_CKDIV_RST_SFT                               22
#define AFE_ADDA7_CKDIV_RST_MASK                              0x1
#define AFE_ADDA7_CKDIV_RST_MASK_SFT                          (0x1 << 22)
#define AFE_ADDA7_FIFO_AUTO_RST_EN_SFT                        21
#define AFE_ADDA7_FIFO_AUTO_RST_EN_MASK                       0x1
#define AFE_ADDA7_FIFO_AUTO_RST_EN_MASK_SFT                   (0x1 << 21)
#define AFE_ADDA7_FIFO_AUTO_RST_SFT                           20
#define AFE_ADDA7_FIFO_AUTO_RST_MASK                          0x1
#define AFE_ADDA7_FIFO_AUTO_RST_MASK_SFT                      (0x1 << 20)
#define AFE_ADDA7_UL_FIFO_DIGMIC_TESTIN_SFT                   17
#define AFE_ADDA7_UL_FIFO_DIGMIC_TESTIN_MASK                  0x3
#define AFE_ADDA7_UL_FIFO_DIGMIC_TESTIN_MASK_SFT              (0x3 << 17)
#define AFE_ADDA7_UL_FIFO_DIGMIC_WDATA_TESTEN_SFT             16
#define AFE_ADDA7_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK            0x1
#define AFE_ADDA7_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK_SFT        (0x1 << 16)
#define AFE_ADDA6_UL_LR_SWAP_SFT                              15
#define AFE_ADDA6_UL_LR_SWAP_MASK                             0x1
#define AFE_ADDA6_UL_LR_SWAP_MASK_SFT                         (0x1 << 15)
#define AFE_ADDA6_CKDIV_RST_SFT                               14
#define AFE_ADDA6_CKDIV_RST_MASK                              0x1
#define AFE_ADDA6_CKDIV_RST_MASK_SFT                          (0x1 << 14)
#define AFE_ADDA6_FIFO_AUTO_RST_EN_SFT                        13
#define AFE_ADDA6_FIFO_AUTO_RST_EN_MASK                       0x1
#define AFE_ADDA6_FIFO_AUTO_RST_EN_MASK_SFT                   (0x1 << 13)
#define AFE_ADDA6_FIFO_AUTO_RST_SFT                           12
#define AFE_ADDA6_FIFO_AUTO_RST_MASK                          0x1
#define AFE_ADDA6_FIFO_AUTO_RST_MASK_SFT                      (0x1 << 12)
#define AFE_ADDA6_UL_FIFO_DIGMIC_TESTIN_SFT                   9
#define AFE_ADDA6_UL_FIFO_DIGMIC_TESTIN_MASK                  0x3
#define AFE_ADDA6_UL_FIFO_DIGMIC_TESTIN_MASK_SFT              (0x3 << 9)
#define AFE_ADDA6_UL_FIFO_DIGMIC_WDATA_TESTEN_SFT             8
#define AFE_ADDA6_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK            0x1
#define AFE_ADDA6_UL_FIFO_DIGMIC_WDATA_TESTEN_MASK_SFT        (0x1 << 8)
#define AFE_ADDA_DIGMIC_CLK_ON_SFT                            4
#define AFE_ADDA_DIGMIC_CLK_ON_MASK                           0x1
#define AFE_ADDA_DIGMIC_CLK_ON_MASK_SFT                       (0x1 << 4)
#define ADDA_AFE_ON_SFT                                       0
#define ADDA_AFE_ON_MASK                                      0x1
#define ADDA_AFE_ON_MASK_SFT                                  (0x1 << 0)

/* AFE_SIDETONE_DEBUG */
#define STF_EN_SEL_SFT                                 16
#define STF_EN_SEL_MASK                                0xf
#define STF_EN_SEL_MASK_SFT                            (0xf << 16)

/* AFE_SIDETONE_CON0 */
#define R_RDY_SFT                                             30
#define R_RDY_MASK                                            0x1
#define R_RDY_MASK_SFT                                        (0x1 << 30)
#define W_RDY_SFT                                             29
#define W_RDY_MASK                                            0x1
#define W_RDY_MASK_SFT                                        (0x1 << 29)
#define R_W_EN_SFT                                            25
#define R_W_EN_MASK                                           0x1
#define R_W_EN_MASK_SFT                                       (0x1 << 25)
#define R_W_SEL_SFT                                           24
#define R_W_SEL_MASK                                          0x1
#define R_W_SEL_MASK_SFT                                      (0x1 << 24)
#define SEL_CH2_SFT                                           23
#define SEL_CH2_MASK                                          0x1
#define SEL_CH2_MASK_SFT                                      (0x1 << 23)
#define SIDE_TONE_COEFFICIENT_ADDR_SFT                        16
#define SIDE_TONE_COEFFICIENT_ADDR_MASK                       0x1f
#define SIDE_TONE_COEFFICIENT_ADDR_MASK_SFT                   (0x1f << 16)
#define SIDE_TONE_COEFFICIENT_SFT                             0
#define SIDE_TONE_COEFFICIENT_MASK                            0xffff
#define SIDE_TONE_COEFFICIENT_MASK_SFT                        (0xffff << 0)

/* AFE_SIDETONE_COEFF */
#define SIDE_TONE_COEFF_SFT                                   0
#define SIDE_TONE_COEFF_MASK                                  0xffff
#define SIDE_TONE_COEFF_MASK_SFT                              (0xffff << 0)

/* AFE_SIDETONE_CON1 */
#define STF_BYPASS_MODE_SFT                                   31
#define STF_BYPASS_MODE_MASK                                  0x1
#define STF_BYPASS_MODE_MASK_SFT                              (0x1 << 31)
#define STF_BYPASS_MODE_O28_O29_SFT                           30
#define STF_BYPASS_MODE_O28_O29_MASK                          0x1
#define STF_BYPASS_MODE_O28_O29_MASK_SFT                      (0x1 << 30)
#define STF_BYPASS_MODE_I2S4_SFT                              29
#define STF_BYPASS_MODE_I2S4_MASK                             0x1
#define STF_BYPASS_MODE_I2S4_MASK_SFT                         (0x1 << 29)
#define STF_BYPASS_MODE_I2S5_SFT                              28
#define STF_BYPASS_MODE_I2S5_MASK                             0x1
#define STF_BYPASS_MODE_I2S5_MASK_SFT                         (0x1 << 28)
#define STF_BYPASS_MODE_DL3_SFT                               27
#define STF_BYPASS_MODE_DL3_MASK                              0x1
#define STF_BYPASS_MODE_DL3_MASK_SFT                          (0x1 << 27)
#define STF_BYPASS_MODE_I2S7_SFT                              26
#define STF_BYPASS_MODE_I2S7_MASK                             0x1
#define STF_BYPASS_MODE_I2S7_MASK_SFT                         (0x1 << 26)
#define STF_BYPASS_MODE_I2S9_SFT                              25
#define STF_BYPASS_MODE_I2S9_MASK                             0x1
#define STF_BYPASS_MODE_I2S9_MASK_SFT                         (0x1 << 25)
#define SIDE_TONE_ON_SFT                                      8
#define SIDE_TONE_ON_MASK                                     0x1
#define SIDE_TONE_ON_MASK_SFT                                 (0x1 << 8)
#define SIDE_TONE_HALF_TAP_NUM_SFT                            0
#define SIDE_TONE_HALF_TAP_NUM_MASK                           0x3f
#define SIDE_TONE_HALF_TAP_NUM_MASK_SFT                       (0x3f << 0)

/* AFE_SIDETONE_GAIN */
#define POSITIVE_GAIN_SFT                                     16
#define POSITIVE_GAIN_MASK                                    0x7
#define POSITIVE_GAIN_MASK_SFT                                (0x7 << 16)
#define SIDE_TONE_GAIN_SFT                                    0
#define SIDE_TONE_GAIN_MASK                                   0xffff
#define SIDE_TONE_GAIN_MASK_SFT                               (0xffff << 0)

/* AFE_ADDA_DL_SDM_DCCOMP_CON */
#define USE_3RD_SDM_SFT                                       28
#define USE_3RD_SDM_MASK                                      0x1
#define USE_3RD_SDM_MASK_SFT                                  (0x1 << 28)
#define DL_FIFO_START_POINT_SFT                               24
#define DL_FIFO_START_POINT_MASK                              0x7
#define DL_FIFO_START_POINT_MASK_SFT                          (0x7 << 24)
#define DL_FIFO_SWAP_SFT                                      20
#define DL_FIFO_SWAP_MASK                                     0x1
#define DL_FIFO_SWAP_MASK_SFT                                 (0x1 << 20)
#define C_AUDSDM1ORDSELECT_CTL_SFT                            19
#define C_AUDSDM1ORDSELECT_CTL_MASK                           0x1
#define C_AUDSDM1ORDSELECT_CTL_MASK_SFT                       (0x1 << 19)
#define C_SDM7BITSEL_CTL_SFT                                  18
#define C_SDM7BITSEL_CTL_MASK                                 0x1
#define C_SDM7BITSEL_CTL_MASK_SFT                             (0x1 << 18)
#define GAIN_AT_SDM_RST_PRE_CTL_SFT                           15
#define GAIN_AT_SDM_RST_PRE_CTL_MASK                          0x1
#define GAIN_AT_SDM_RST_PRE_CTL_MASK_SFT                      (0x1 << 15)
#define DL_DCM_AUTO_IDLE_EN_SFT                               14
#define DL_DCM_AUTO_IDLE_EN_MASK                              0x1
#define DL_DCM_AUTO_IDLE_EN_MASK_SFT                          (0x1 << 14)
#define AFE_DL_SRC_DCM_EN_SFT                                 13
#define AFE_DL_SRC_DCM_EN_MASK                                0x1
#define AFE_DL_SRC_DCM_EN_MASK_SFT                            (0x1 << 13)
#define AFE_DL_POST_SRC_DCM_EN_SFT                            12
#define AFE_DL_POST_SRC_DCM_EN_MASK                           0x1
#define AFE_DL_POST_SRC_DCM_EN_MASK_SFT                       (0x1 << 12)
#define AUD_SDM_MONO_SFT                                      9
#define AUD_SDM_MONO_MASK                                     0x1
#define AUD_SDM_MONO_MASK_SFT                                 (0x1 << 9)
#define AUD_DC_COMP_EN_SFT                                    8
#define AUD_DC_COMP_EN_MASK                                   0x1
#define AUD_DC_COMP_EN_MASK_SFT                               (0x1 << 8)
#define ATTGAIN_CTL_SFT                                       0
#define ATTGAIN_CTL_MASK                                      0x3f
#define ATTGAIN_CTL_MASK_SFT                                  (0x3f << 0)

/* AFE_SINEGEN_CON0 */
#define DAC_EN_SFT                                            26
#define DAC_EN_MASK                                           0x1
#define DAC_EN_MASK_SFT                                       (0x1 << 26)
#define MUTE_SW_CH2_SFT                                       25
#define MUTE_SW_CH2_MASK                                      0x1
#define MUTE_SW_CH2_MASK_SFT                                  (0x1 << 25)
#define MUTE_SW_CH1_SFT                                       24
#define MUTE_SW_CH1_MASK                                      0x1
#define MUTE_SW_CH1_MASK_SFT                                  (0x1 << 24)
#define SINE_MODE_CH2_SFT                                     20
#define SINE_MODE_CH2_MASK                                    0xf
#define SINE_MODE_CH2_MASK_SFT                                (0xf << 20)
#define AMP_DIV_CH2_SFT                                       17
#define AMP_DIV_CH2_MASK                                      0x7
#define AMP_DIV_CH2_MASK_SFT                                  (0x7 << 17)
#define FREQ_DIV_CH2_SFT                                      12
#define FREQ_DIV_CH2_MASK                                     0x1f
#define FREQ_DIV_CH2_MASK_SFT                                 (0x1f << 12)
#define SINE_MODE_CH1_SFT                                     8
#define SINE_MODE_CH1_MASK                                    0xf
#define SINE_MODE_CH1_MASK_SFT                                (0xf << 8)
#define AMP_DIV_CH1_SFT                                       5
#define AMP_DIV_CH1_MASK                                      0x7
#define AMP_DIV_CH1_MASK_SFT                                  (0x7 << 5)
#define FREQ_DIV_CH1_SFT                                      0
#define FREQ_DIV_CH1_MASK                                     0x1f
#define FREQ_DIV_CH1_MASK_SFT                                 (0x1f << 0)

/* AFE_SINEGEN_CON2 */
#define INNER_LOOP_BACK_MODE_SFT                              0
#define INNER_LOOP_BACK_MODE_MASK                             0x7f
#define INNER_LOOP_BACK_MODE_MASK_SFT                         (0x7f << 0)

/* AFE_HD_ENGEN_ENABLE */
#define AFE_24M_ON_SFT                                        1
#define AFE_24M_ON_MASK                                       0x1
#define AFE_24M_ON_MASK_SFT                                   (0x1 << 1)
#define AFE_22M_ON_SFT                                        0
#define AFE_22M_ON_MASK                                       0x1
#define AFE_22M_ON_MASK_SFT                                   (0x1 << 0)

/* Remove:: AFE_ADDA_DL_NLE_FIFO_MON */
#define DL_NLE_FIFO_WBIN_SFT                           8
#define DL_NLE_FIFO_WBIN_MASK                          0xf
#define DL_NLE_FIFO_WBIN_MASK_SFT                      (0xf << 8)
#define DL_NLE_FIFO_RBIN_SFT                           4
#define DL_NLE_FIFO_RBIN_MASK                          0xf
#define DL_NLE_FIFO_RBIN_MASK_SFT                      (0xf << 4)
#define DL_NLE_FIFO_RDACTIVE_SFT                       3
#define DL_NLE_FIFO_RDACTIVE_MASK                      0x1
#define DL_NLE_FIFO_RDACTIVE_MASK_SFT                  (0x1 << 3)
#define DL_NLE_FIFO_STARTRD_SFT                        2
#define DL_NLE_FIFO_STARTRD_MASK                       0x1
#define DL_NLE_FIFO_STARTRD_MASK_SFT                   (0x1 << 2)
#define DL_NLE_FIFO_RD_EMPTY_SFT                       1
#define DL_NLE_FIFO_RD_EMPTY_MASK                      0x1
#define DL_NLE_FIFO_RD_EMPTY_MASK_SFT                  (0x1 << 1)
#define DL_NLE_FIFO_WR_FULL_SFT                        0
#define DL_NLE_FIFO_WR_FULL_MASK                       0x1
#define DL_NLE_FIFO_WR_FULL_MASK_SFT                   (0x1 << 0)

/* AFE_DL1_CON0 */
#define DL1_MODE_SFT                                          24
#define DL1_MODE_MASK                                         0xf
#define DL1_MODE_MASK_SFT                                     (0xf << 24)
#define DL1_MINLEN_SFT                                        20
#define DL1_MINLEN_MASK                                       0xf
#define DL1_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL1_MAXLEN_SFT                                        16
#define DL1_MAXLEN_MASK                                       0xf
#define DL1_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL1_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL1_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL1_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL1_PBUF_SIZE_SFT                                     12
#define DL1_PBUF_SIZE_MASK                                    0x3
#define DL1_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL1_MONO_SFT                                          8
#define DL1_MONO_MASK                                         0x1
#define DL1_MONO_MASK_SFT                                     (0x1 << 8)
#define DL1_NORMAL_MODE_SFT                                   5
#define DL1_NORMAL_MODE_MASK                                  0x1
#define DL1_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL1_HALIGN_SFT                                        4
#define DL1_HALIGN_MASK                                       0x1
#define DL1_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL1_HD_MODE_SFT                                       0
#define DL1_HD_MODE_MASK                                      0x3
#define DL1_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL2_CON0 */
#define DL2_MODE_SFT                                          24
#define DL2_MODE_MASK                                         0xf
#define DL2_MODE_MASK_SFT                                     (0xf << 24)
#define DL2_MINLEN_SFT                                        20
#define DL2_MINLEN_MASK                                       0xf
#define DL2_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL2_MAXLEN_SFT                                        16
#define DL2_MAXLEN_MASK                                       0xf
#define DL2_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL2_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL2_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL2_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL2_PBUF_SIZE_SFT                                     12
#define DL2_PBUF_SIZE_MASK                                    0x3
#define DL2_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL2_MONO_SFT                                          8
#define DL2_MONO_MASK                                         0x1
#define DL2_MONO_MASK_SFT                                     (0x1 << 8)
#define DL2_NORMAL_MODE_SFT                                   5
#define DL2_NORMAL_MODE_MASK                                  0x1
#define DL2_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL2_HALIGN_SFT                                        4
#define DL2_HALIGN_MASK                                       0x1
#define DL2_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL2_HD_MODE_SFT                                       0
#define DL2_HD_MODE_MASK                                      0x3
#define DL2_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL3_CON0 */
#define DL3_MODE_SFT                                          24
#define DL3_MODE_MASK                                         0xf
#define DL3_MODE_MASK_SFT                                     (0xf << 24)
#define DL3_MINLEN_SFT                                        20
#define DL3_MINLEN_MASK                                       0xf
#define DL3_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL3_MAXLEN_SFT                                        16
#define DL3_MAXLEN_MASK                                       0xf
#define DL3_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL3_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL3_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL3_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL3_PBUF_SIZE_SFT                                     12
#define DL3_PBUF_SIZE_MASK                                    0x3
#define DL3_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL3_MONO_SFT                                          8
#define DL3_MONO_MASK                                         0x1
#define DL3_MONO_MASK_SFT                                     (0x1 << 8)
#define DL3_NORMAL_MODE_SFT                                   5
#define DL3_NORMAL_MODE_MASK                                  0x1
#define DL3_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL3_HALIGN_SFT                                        4
#define DL3_HALIGN_MASK                                       0x1
#define DL3_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL3_HD_MODE_SFT                                       0
#define DL3_HD_MODE_MASK                                      0x3
#define DL3_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL4_CON0 */
#define DL4_MODE_SFT                                          24
#define DL4_MODE_MASK                                         0xf
#define DL4_MODE_MASK_SFT                                     (0xf << 24)
#define DL4_MINLEN_SFT                                        20
#define DL4_MINLEN_MASK                                       0xf
#define DL4_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL4_MAXLEN_SFT                                        16
#define DL4_MAXLEN_MASK                                       0xf
#define DL4_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL4_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL4_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL4_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL4_PBUF_SIZE_SFT                                     12
#define DL4_PBUF_SIZE_MASK                                    0x3
#define DL4_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL4_MONO_SFT                                          8
#define DL4_MONO_MASK                                         0x1
#define DL4_MONO_MASK_SFT                                     (0x1 << 8)
#define DL4_NORMAL_MODE_SFT                                   5
#define DL4_NORMAL_MODE_MASK                                  0x1
#define DL4_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL4_HALIGN_SFT                                        4
#define DL4_HALIGN_MASK                                       0x1
#define DL4_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL4_HD_MODE_SFT                                       0
#define DL4_HD_MODE_MASK                                      0x3
#define DL4_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL5_CON0 */
#define DL5_MODE_SFT                                          24
#define DL5_MODE_MASK                                         0xf
#define DL5_MODE_MASK_SFT                                     (0xf << 24)
#define DL5_MINLEN_SFT                                        20
#define DL5_MINLEN_MASK                                       0xf
#define DL5_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL5_MAXLEN_SFT                                        16
#define DL5_MAXLEN_MASK                                       0xf
#define DL5_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL5_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL5_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL5_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL5_PBUF_SIZE_SFT                                     12
#define DL5_PBUF_SIZE_MASK                                    0x3
#define DL5_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL5_MONO_SFT                                          8
#define DL5_MONO_MASK                                         0x1
#define DL5_MONO_MASK_SFT                                     (0x1 << 8)
#define DL5_NORMAL_MODE_SFT                                   5
#define DL5_NORMAL_MODE_MASK                                  0x1
#define DL5_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL5_HALIGN_SFT                                        4
#define DL5_HALIGN_MASK                                       0x1
#define DL5_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL5_HD_MODE_SFT                                       0
#define DL5_HD_MODE_MASK                                      0x3
#define DL5_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL6_CON0 */
#define DL6_MODE_SFT                                          24
#define DL6_MODE_MASK                                         0xf
#define DL6_MODE_MASK_SFT                                     (0xf << 24)
#define DL6_MINLEN_SFT                                        20
#define DL6_MINLEN_MASK                                       0xf
#define DL6_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL6_MAXLEN_SFT                                        16
#define DL6_MAXLEN_MASK                                       0xf
#define DL6_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL6_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL6_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL6_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL6_PBUF_SIZE_SFT                                     12
#define DL6_PBUF_SIZE_MASK                                    0x3
#define DL6_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL6_MONO_SFT                                          8
#define DL6_MONO_MASK                                         0x1
#define DL6_MONO_MASK_SFT                                     (0x1 << 8)
#define DL6_NORMAL_MODE_SFT                                   5
#define DL6_NORMAL_MODE_MASK                                  0x1
#define DL6_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL6_HALIGN_SFT                                        4
#define DL6_HALIGN_MASK                                       0x1
#define DL6_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL6_HD_MODE_SFT                                       0
#define DL6_HD_MODE_MASK                                      0x3
#define DL6_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL7_CON0 */
#define DL7_MODE_SFT                                          24
#define DL7_MODE_MASK                                         0xf
#define DL7_MODE_MASK_SFT                                     (0xf << 24)
#define DL7_MINLEN_SFT                                        20
#define DL7_MINLEN_MASK                                       0xf
#define DL7_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL7_MAXLEN_SFT                                        16
#define DL7_MAXLEN_MASK                                       0xf
#define DL7_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL7_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL7_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL7_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL7_PBUF_SIZE_SFT                                     12
#define DL7_PBUF_SIZE_MASK                                    0x3
#define DL7_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL7_MONO_SFT                                          8
#define DL7_MONO_MASK                                         0x1
#define DL7_MONO_MASK_SFT                                     (0x1 << 8)
#define DL7_NORMAL_MODE_SFT                                   5
#define DL7_NORMAL_MODE_MASK                                  0x1
#define DL7_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL7_HALIGN_SFT                                        4
#define DL7_HALIGN_MASK                                       0x1
#define DL7_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL7_HD_MODE_SFT                                       0
#define DL7_HD_MODE_MASK                                      0x3
#define DL7_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL8_CON0 */
#define DL8_MODE_SFT                                          24
#define DL8_MODE_MASK                                         0xf
#define DL8_MODE_MASK_SFT                                     (0xf << 24)
#define DL8_MINLEN_SFT                                        20
#define DL8_MINLEN_MASK                                       0xf
#define DL8_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL8_MAXLEN_SFT                                        16
#define DL8_MAXLEN_MASK                                       0xf
#define DL8_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL8_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL8_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL8_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL8_PBUF_SIZE_SFT                                     12
#define DL8_PBUF_SIZE_MASK                                    0x3
#define DL8_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL8_MONO_SFT                                          8
#define DL8_MONO_MASK                                         0x1
#define DL8_MONO_MASK_SFT                                     (0x1 << 8)
#define DL8_NORMAL_MODE_SFT                                   5
#define DL8_NORMAL_MODE_MASK                                  0x1
#define DL8_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL8_HALIGN_SFT                                        4
#define DL8_HALIGN_MASK                                       0x1
#define DL8_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL8_HD_MODE_SFT                                       0
#define DL8_HD_MODE_MASK                                      0x3
#define DL8_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL9_CON0 */
#define DL9_MODE_SFT                                          24
#define DL9_MODE_MASK                                         0xf
#define DL9_MODE_MASK_SFT                                     (0xf << 24)
#define DL9_MINLEN_SFT                                        20
#define DL9_MINLEN_MASK                                       0xf
#define DL9_MINLEN_MASK_SFT                                   (0xf << 20)
#define DL9_MAXLEN_SFT                                        16
#define DL9_MAXLEN_MASK                                       0xf
#define DL9_MAXLEN_MASK_SFT                                   (0xf << 16)
#define DL9_SW_CLEAR_BUF_EMPTY_SFT                            15
#define DL9_SW_CLEAR_BUF_EMPTY_MASK                           0x1
#define DL9_SW_CLEAR_BUF_EMPTY_MASK_SFT                       (0x1 << 15)
#define DL9_PBUF_SIZE_SFT                                     12
#define DL9_PBUF_SIZE_MASK                                    0x3
#define DL9_PBUF_SIZE_MASK_SFT                                (0x3 << 12)
#define DL9_MONO_SFT                                          8
#define DL9_MONO_MASK                                         0x1
#define DL9_MONO_MASK_SFT                                     (0x1 << 8)
#define DL9_NORMAL_MODE_SFT                                   5
#define DL9_NORMAL_MODE_MASK                                  0x1
#define DL9_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DL9_HALIGN_SFT                                        4
#define DL9_HALIGN_MASK                                       0x1
#define DL9_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DL9_HD_MODE_SFT                                       0
#define DL9_HD_MODE_MASK                                      0x3
#define DL9_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_DL12_CON0 */
#define DL12_MODE_SFT                                         24
#define DL12_MODE_MASK                                        0xf
#define DL12_MODE_MASK_SFT                                    (0xf << 24)
#define DL12_MINLEN_SFT                                       20
#define DL12_MINLEN_MASK                                      0xf
#define DL12_MINLEN_MASK_SFT                                  (0xf << 20)
#define DL12_MAXLEN_SFT                                       16
#define DL12_MAXLEN_MASK                                      0xf
#define DL12_MAXLEN_MASK_SFT                                  (0xf << 16)
#define DL12_SW_CLEAR_BUF_EMPTY_SFT                           15
#define DL12_SW_CLEAR_BUF_EMPTY_MASK                          0x1
#define DL12_SW_CLEAR_BUF_EMPTY_MASK_SFT                      (0x1 << 15)
#define DL12_PBUF_SIZE_SFT                                    12
#define DL12_PBUF_SIZE_MASK                                   0x3
#define DL12_PBUF_SIZE_MASK_SFT                               (0x3 << 12)
#define DL12_4CH_EN_SFT                                       11
#define DL12_4CH_EN_MASK                                      0x1
#define DL12_4CH_EN_MASK_SFT                                  (0x1 << 11)
#define DL12_MONO_SFT                                         8
#define DL12_MONO_MASK                                        0x1
#define DL12_MONO_MASK_SFT                                    (0x1 << 8)
#define DL12_NORMAL_MODE_SFT                                  5
#define DL12_NORMAL_MODE_MASK                                 0x1
#define DL12_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define DL12_HALIGN_SFT                                       4
#define DL12_HALIGN_MASK                                      0x1
#define DL12_HALIGN_MASK_SFT                                  (0x1 << 4)
#define DL12_HD_MODE_SFT                                      0
#define DL12_HD_MODE_MASK                                     0x3
#define DL12_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_DL10_CON0 */
#define DL10_MODE_SFT                                         24
#define DL10_MODE_MASK                                        0xf
#define DL10_MODE_MASK_SFT                                    (0xf << 24)
#define DL10_MINLEN_SFT                                       20
#define DL10_MINLEN_MASK                                      0xf
#define DL10_MINLEN_MASK_SFT                                  (0xf << 20)
#define DL10_MAXLEN_SFT                                       16
#define DL10_MAXLEN_MASK                                      0xf
#define DL10_MAXLEN_MASK_SFT                                  (0xf << 16)
#define DL10_SW_CLEAR_BUF_EMPTY_SFT                           15
#define DL10_SW_CLEAR_BUF_EMPTY_MASK                          0x1
#define DL10_SW_CLEAR_BUF_EMPTY_MASK_SFT                      (0x1 << 15)
#define DL10_PBUF_SIZE_SFT                                    12
#define DL10_PBUF_SIZE_MASK                                   0x3
#define DL10_PBUF_SIZE_MASK_SFT                               (0x3 << 12)
#define DL10_MONO_SFT                                         8
#define DL10_MONO_MASK                                        0x1
#define DL10_MONO_MASK_SFT                                    (0x1 << 8)
#define DL10_NORMAL_MODE_SFT                                  5
#define DL10_NORMAL_MODE_MASK                                 0x1
#define DL10_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define DL10_HALIGN_SFT                                       4
#define DL10_HALIGN_MASK                                      0x1
#define DL10_HALIGN_MASK_SFT                                  (0x1 << 4)
#define DL10_HD_MODE_SFT                                      0
#define DL10_HD_MODE_MASK                                     0x3
#define DL10_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_DL11_CON0 */
#define DL11_CH_NUM_SFT                                       28
#define DL11_CH_NUM_MASK                                      0xf
#define DL11_CH_NUM_MASK_SFT                                  (0xf << 28)
#define DL11_MODE_SFT                                         24
#define DL11_MODE_MASK                                        0xf
#define DL11_MODE_MASK_SFT                                    (0xf << 24)
#define DL11_MINLEN_SFT                                       20
#define DL11_MINLEN_MASK                                      0xf
#define DL11_MINLEN_MASK_SFT                                  (0xf << 20)
#define DL11_MAXLEN_SFT                                       16
#define DL11_MAXLEN_MASK                                      0xf
#define DL11_MAXLEN_MASK_SFT                                  (0xf << 16)
#define DL11_SW_CLEAR_BUF_EMPTY_SFT                           15
#define DL11_SW_CLEAR_BUF_EMPTY_MASK                          0x1
#define DL11_SW_CLEAR_BUF_EMPTY_MASK_SFT                      (0x1 << 15)
#define DL11_PBUF_SIZE_SFT                                    12
#define DL11_PBUF_SIZE_MASK                                   0x3
#define DL11_PBUF_SIZE_MASK_SFT                               (0x3 << 12)
#define DL11_MONO_SFT                                         8
#define DL11_MONO_MASK                                        0x1
#define DL11_MONO_MASK_SFT                                    (0x1 << 8)
#define DL11_NORMAL_MODE_SFT                                  5
#define DL11_NORMAL_MODE_MASK                                 0x1
#define DL11_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define DL11_HALIGN_SFT                                       4
#define DL11_HALIGN_MASK                                      0x1
#define DL11_HALIGN_MASK_SFT                                  (0x1 << 4)
#define DL11_HD_MODE_SFT                                      0
#define DL11_HD_MODE_MASK                                     0x3
#define DL11_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_AWB_CON0 */
#define AWB_MODE_SFT                                          24
#define AWB_MODE_MASK                                         0xf
#define AWB_MODE_MASK_SFT                                     (0xf << 24)
#define AWB_SW_CLEAR_BUF_FULL_SFT                             15
#define AWB_SW_CLEAR_BUF_FULL_MASK                            0x1
#define AWB_SW_CLEAR_BUF_FULL_MASK_SFT                        (0x1 << 15)
#define AWB_R_MONO_SFT                                        9
#define AWB_R_MONO_MASK                                       0x1
#define AWB_R_MONO_MASK_SFT                                   (0x1 << 9)
#define AWB_MONO_SFT                                          8
#define AWB_MONO_MASK                                         0x1
#define AWB_MONO_MASK_SFT                                     (0x1 << 8)
#define AWB_WR_SIGN_SFT                                       6
#define AWB_WR_SIGN_MASK                                      0x1
#define AWB_WR_SIGN_MASK_SFT                                  (0x1 << 6)
#define AWB_NORMAL_MODE_SFT                                   5
#define AWB_NORMAL_MODE_MASK                                  0x1
#define AWB_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define AWB_HALIGN_SFT                                        4
#define AWB_HALIGN_MASK                                       0x1
#define AWB_HALIGN_MASK_SFT                                   (0x1 << 4)
#define AWB_HD_MODE_SFT                                       0
#define AWB_HD_MODE_MASK                                      0x3
#define AWB_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_AWB2_CON0 */
#define AWB2_MODE_SFT                                         24
#define AWB2_MODE_MASK                                        0xf
#define AWB2_MODE_MASK_SFT                                    (0xf << 24)
#define AWB2_SW_CLEAR_BUF_FULL_SFT                            15
#define AWB2_SW_CLEAR_BUF_FULL_MASK                           0x1
#define AWB2_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define AWB2_R_MONO_SFT                                       9
#define AWB2_R_MONO_MASK                                      0x1
#define AWB2_R_MONO_MASK_SFT                                  (0x1 << 9)
#define AWB2_MONO_SFT                                         8
#define AWB2_MONO_MASK                                        0x1
#define AWB2_MONO_MASK_SFT                                    (0x1 << 8)
#define AWB2_WR_SIGN_SFT                                      6
#define AWB2_WR_SIGN_MASK                                     0x1
#define AWB2_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define AWB2_NORMAL_MODE_SFT                                  5
#define AWB2_NORMAL_MODE_MASK                                 0x1
#define AWB2_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define AWB2_HALIGN_SFT                                       4
#define AWB2_HALIGN_MASK                                      0x1
#define AWB2_HALIGN_MASK_SFT                                  (0x1 << 4)
#define AWB2_HD_MODE_SFT                                      0
#define AWB2_HD_MODE_MASK                                     0x3
#define AWB2_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL_CON0 */
#define VUL_MODE_SFT                                          24
#define VUL_MODE_MASK                                         0xf
#define VUL_MODE_MASK_SFT                                     (0xf << 24)
#define VUL_SW_CLEAR_BUF_FULL_SFT                             15
#define VUL_SW_CLEAR_BUF_FULL_MASK                            0x1
#define VUL_SW_CLEAR_BUF_FULL_MASK_SFT                        (0x1 << 15)
#define VUL_R_MONO_SFT                                        9
#define VUL_R_MONO_MASK                                       0x1
#define VUL_R_MONO_MASK_SFT                                   (0x1 << 9)
#define VUL_MONO_SFT                                          8
#define VUL_MONO_MASK                                         0x1
#define VUL_MONO_MASK_SFT                                     (0x1 << 8)
#define VUL_WR_SIGN_SFT                                       6
#define VUL_WR_SIGN_MASK                                      0x1
#define VUL_WR_SIGN_MASK_SFT                                  (0x1 << 6)
#define VUL_NORMAL_MODE_SFT                                   5
#define VUL_NORMAL_MODE_MASK                                  0x1
#define VUL_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define VUL_HALIGN_SFT                                        4
#define VUL_HALIGN_MASK                                       0x1
#define VUL_HALIGN_MASK_SFT                                   (0x1 << 4)
#define VUL_HD_MODE_SFT                                       0
#define VUL_HD_MODE_MASK                                      0x3
#define VUL_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_VUL12_CON0 */
#define VUL12_MODE_SFT                                        24
#define VUL12_MODE_MASK                                       0xf
#define VUL12_MODE_MASK_SFT                                   (0xf << 24)
#define VUL12_SW_CLEAR_BUF_FULL_SFT                           15
#define VUL12_SW_CLEAR_BUF_FULL_MASK                          0x1
#define VUL12_SW_CLEAR_BUF_FULL_MASK_SFT                      (0x1 << 15)
#define VUL12_4CH_EN_SFT                                      11
#define VUL12_4CH_EN_MASK                                     0x1
#define VUL12_4CH_EN_MASK_SFT                                 (0x1 << 11)
#define VUL12_R_MONO_SFT                                      9
#define VUL12_R_MONO_MASK                                     0x1
#define VUL12_R_MONO_MASK_SFT                                 (0x1 << 9)
#define VUL12_MONO_SFT                                        8
#define VUL12_MONO_MASK                                       0x1
#define VUL12_MONO_MASK_SFT                                   (0x1 << 8)
#define VUL12_WR_SIGN_SFT                                     6
#define VUL12_WR_SIGN_MASK                                    0x1
#define VUL12_WR_SIGN_MASK_SFT                                (0x1 << 6)
#define VUL12_NORMAL_MODE_SFT                                 5
#define VUL12_NORMAL_MODE_MASK                                0x1
#define VUL12_NORMAL_MODE_MASK_SFT                            (0x1 << 5)
#define VUL12_HALIGN_SFT                                      4
#define VUL12_HALIGN_MASK                                     0x1
#define VUL12_HALIGN_MASK_SFT                                 (0x1 << 4)
#define VUL12_HD_MODE_SFT                                     0
#define VUL12_HD_MODE_MASK                                    0x3
#define VUL12_HD_MODE_MASK_SFT                                (0x3 << 0)

/* AFE_VUL2_CON0 */
#define VUL2_MODE_SFT                                         24
#define VUL2_MODE_MASK                                        0xf
#define VUL2_MODE_MASK_SFT                                    (0xf << 24)
#define VUL2_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL2_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL2_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL2_R_MONO_SFT                                       9
#define VUL2_R_MONO_MASK                                      0x1
#define VUL2_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL2_MONO_SFT                                         8
#define VUL2_MONO_MASK                                        0x1
#define VUL2_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL2_WR_SIGN_SFT                                      6
#define VUL2_WR_SIGN_MASK                                     0x1
#define VUL2_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL2_NORMAL_MODE_SFT                                  5
#define VUL2_NORMAL_MODE_MASK                                 0x1
#define VUL2_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL2_HALIGN_SFT                                       4
#define VUL2_HALIGN_MASK                                      0x1
#define VUL2_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL2_HD_MODE_SFT                                      0
#define VUL2_HD_MODE_MASK                                     0x3
#define VUL2_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL3_CON0 */
#define VUL3_MODE_SFT                                         24
#define VUL3_MODE_MASK                                        0xf
#define VUL3_MODE_MASK_SFT                                    (0xf << 24)
#define VUL3_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL3_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL3_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL3_R_MONO_SFT                                       9
#define VUL3_R_MONO_MASK                                      0x1
#define VUL3_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL3_MONO_SFT                                         8
#define VUL3_MONO_MASK                                        0x1
#define VUL3_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL3_WR_SIGN_SFT                                      6
#define VUL3_WR_SIGN_MASK                                     0x1
#define VUL3_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL3_NORMAL_MODE_SFT                                  5
#define VUL3_NORMAL_MODE_MASK                                 0x1
#define VUL3_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL3_HALIGN_SFT                                       4
#define VUL3_HALIGN_MASK                                      0x1
#define VUL3_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL3_HD_MODE_SFT                                      0
#define VUL3_HD_MODE_MASK                                     0x3
#define VUL3_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL4_CON0 */
#define VUL4_MODE_SFT                                         24
#define VUL4_MODE_MASK                                        0xf
#define VUL4_MODE_MASK_SFT                                    (0xf << 24)
#define VUL4_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL4_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL4_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL4_R_MONO_SFT                                       9
#define VUL4_R_MONO_MASK                                      0x1
#define VUL4_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL4_MONO_SFT                                         8
#define VUL4_MONO_MASK                                        0x1
#define VUL4_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL4_WR_SIGN_SFT                                      6
#define VUL4_WR_SIGN_MASK                                     0x1
#define VUL4_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL4_NORMAL_MODE_SFT                                  5
#define VUL4_NORMAL_MODE_MASK                                 0x1
#define VUL4_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL4_HALIGN_SFT                                       4
#define VUL4_HALIGN_MASK                                      0x1
#define VUL4_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL4_HD_MODE_SFT                                      0
#define VUL4_HD_MODE_MASK                                     0x3
#define VUL4_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL5_CON0 */
#define VUL5_MODE_SFT                                         24
#define VUL5_MODE_MASK                                        0xf
#define VUL5_MODE_MASK_SFT                                    (0xf << 24)
#define VUL5_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL5_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL5_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL5_R_MONO_SFT                                       9
#define VUL5_R_MONO_MASK                                      0x1
#define VUL5_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL5_MONO_SFT                                         8
#define VUL5_MONO_MASK                                        0x1
#define VUL5_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL5_WR_SIGN_SFT                                      6
#define VUL5_WR_SIGN_MASK                                     0x1
#define VUL5_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL5_NORMAL_MODE_SFT                                  5
#define VUL5_NORMAL_MODE_MASK                                 0x1
#define VUL5_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL5_HALIGN_SFT                                       4
#define VUL5_HALIGN_MASK                                      0x1
#define VUL5_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL5_HD_MODE_SFT                                      0
#define VUL5_HD_MODE_MASK                                     0x3
#define VUL5_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL6_CON0 */
#define VUL6_MODE_SFT                                         24
#define VUL6_MODE_MASK                                        0xf
#define VUL6_MODE_MASK_SFT                                    (0xf << 24)
#define VUL6_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL6_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL6_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL6_R_MONO_SFT                                       9
#define VUL6_R_MONO_MASK                                      0x1
#define VUL6_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL6_MONO_SFT                                         8
#define VUL6_MONO_MASK                                        0x1
#define VUL6_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL6_WR_SIGN_SFT                                      6
#define VUL6_WR_SIGN_MASK                                     0x1
#define VUL6_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL6_NORMAL_MODE_SFT                                  5
#define VUL6_NORMAL_MODE_MASK                                 0x1
#define VUL6_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL6_HALIGN_SFT                                       4
#define VUL6_HALIGN_MASK                                      0x1
#define VUL6_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL6_HD_MODE_SFT                                      0
#define VUL6_HD_MODE_MASK                                     0x3
#define VUL6_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL7_2_CON0 */
#define VUL7_2_MODE_SFT                                       24
#define VUL7_2_MODE_MASK                                      0xf
#define VUL7_2_MODE_MASK_SFT                                  (0xf << 24)
#define VUL7_2_SW_CLEAR_BUF_FULL_SFT                          15
#define VUL7_2_SW_CLEAR_BUF_FULL_MASK                         0x1
#define VUL7_2_SW_CLEAR_BUF_FULL_MASK_SFT                     (0x1 << 15)
#define VUL7_2_4CH_EN_SFT                                     11
#define VUL7_2_4CH_EN_MASK                                    0x1
#define VUL7_2_4CH_EN_MASK_SFT                                (0x1 << 11)
#define VUL7_2_R_MONO_SFT                                     9
#define VUL7_2_R_MONO_MASK                                    0x1
#define VUL7_2_R_MONO_MASK_SFT                                (0x1 << 9)
#define VUL7_2_MONO_SFT                                       8
#define VUL7_2_MONO_MASK                                      0x1
#define VUL7_2_MONO_MASK_SFT                                  (0x1 << 8)
#define VUL7_2_WR_SIGN_SFT                                    6
#define VUL7_2_WR_SIGN_MASK                                   0x1
#define VUL7_2_WR_SIGN_MASK_SFT                               (0x1 << 6)
#define VUL7_2_NORMAL_MODE_SFT                                5
#define VUL7_2_NORMAL_MODE_MASK                               0x1
#define VUL7_2_NORMAL_MODE_MASK_SFT                           (0x1 << 5)
#define VUL7_2_HALIGN_SFT                                     4
#define VUL7_2_HALIGN_MASK                                    0x1
#define VUL7_2_HALIGN_MASK_SFT                                (0x1 << 4)
#define VUL7_2_HD_MODE_SFT                                    0
#define VUL7_2_HD_MODE_MASK                                   0x3
#define VUL7_2_HD_MODE_MASK_SFT                               (0x3 << 0)

/* AFE_VUL8_CON0 */
#define VUL8_MODE_SFT                                         24
#define VUL8_MODE_MASK                                        0xf
#define VUL8_MODE_MASK_SFT                                    (0xf << 24)
#define VUL8_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL8_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL8_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL8_R_MONO_SFT                                       9
#define VUL8_R_MONO_MASK                                      0x1
#define VUL8_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL8_MONO_SFT                                         8
#define VUL8_MONO_MASK                                        0x1
#define VUL8_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL8_WR_SIGN_SFT                                      6
#define VUL8_WR_SIGN_MASK                                     0x1
#define VUL8_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL8_NORMAL_MODE_SFT                                  5
#define VUL8_NORMAL_MODE_MASK                                 0x1
#define VUL8_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL8_HALIGN_SFT                                       4
#define VUL8_HALIGN_MASK                                      0x1
#define VUL8_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL8_HD_MODE_SFT                                      0
#define VUL8_HD_MODE_MASK                                     0x3
#define VUL8_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_VUL9_CON0 */
#define VUL9_MODE_SFT                                         24
#define VUL9_MODE_MASK                                        0xf
#define VUL9_MODE_MASK_SFT                                    (0xf << 24)
#define VUL9_SW_CLEAR_BUF_FULL_SFT                            15
#define VUL9_SW_CLEAR_BUF_FULL_MASK                           0x1
#define VUL9_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define VUL9_R_MONO_SFT                                       9
#define VUL9_R_MONO_MASK                                      0x1
#define VUL9_R_MONO_MASK_SFT                                  (0x1 << 9)
#define VUL9_MONO_SFT                                         8
#define VUL9_MONO_MASK                                        0x1
#define VUL9_MONO_MASK_SFT                                    (0x1 << 8)
#define VUL9_WR_SIGN_SFT                                      6
#define VUL9_WR_SIGN_MASK                                     0x1
#define VUL9_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define VUL9_NORMAL_MODE_SFT                                  5
#define VUL9_NORMAL_MODE_MASK                                 0x1
#define VUL9_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define VUL9_HALIGN_SFT                                       4
#define VUL9_HALIGN_MASK                                      0x1
#define VUL9_HALIGN_MASK_SFT                                  (0x1 << 4)
#define VUL9_HD_MODE_SFT                                      0
#define VUL9_HD_MODE_MASK                                     0x3
#define VUL9_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_DAI_CON0 */
#define DAI_MODE_SFT                                          24
#define DAI_MODE_MASK                                         0x3
#define DAI_MODE_MASK_SFT                                     (0x3 << 24)
#define DAI_SW_CLEAR_BUF_FULL_SFT                             15
#define DAI_SW_CLEAR_BUF_FULL_MASK                            0x1
#define DAI_SW_CLEAR_BUF_FULL_MASK_SFT                        (0x1 << 15)
#define DAI_DUPLICATE_WR_SFT                                  10
#define DAI_DUPLICATE_WR_MASK                                 0x1
#define DAI_DUPLICATE_WR_MASK_SFT                             (0x1 << 10)
#define DAI_MONO_SFT                                          8
#define DAI_MONO_MASK                                         0x1
#define DAI_MONO_MASK_SFT                                     (0x1 << 8)
#define DAI_WR_SIGN_SFT                                       6
#define DAI_WR_SIGN_MASK                                      0x1
#define DAI_WR_SIGN_MASK_SFT                                  (0x1 << 6)
#define DAI_NORMAL_MODE_SFT                                   5
#define DAI_NORMAL_MODE_MASK                                  0x1
#define DAI_NORMAL_MODE_MASK_SFT                              (0x1 << 5)
#define DAI_HALIGN_SFT                                        4
#define DAI_HALIGN_MASK                                       0x1
#define DAI_HALIGN_MASK_SFT                                   (0x1 << 4)
#define DAI_HD_MODE_SFT                                       0
#define DAI_HD_MODE_MASK                                      0x3
#define DAI_HD_MODE_MASK_SFT                                  (0x3 << 0)

/* AFE_MOD_DAI_CON0 */
#define MOD_DAI_MODE_SFT                                      24
#define MOD_DAI_MODE_MASK                                     0x3
#define MOD_DAI_MODE_MASK_SFT                                 (0x3 << 24)
#define MOD_DAI_SW_CLEAR_BUF_FULL_SFT                         15
#define MOD_DAI_SW_CLEAR_BUF_FULL_MASK                        0x1
#define MOD_DAI_SW_CLEAR_BUF_FULL_MASK_SFT                    (0x1 << 15)
#define MOD_DAI_DUPLICATE_WR_SFT                              10
#define MOD_DAI_DUPLICATE_WR_MASK                             0x1
#define MOD_DAI_DUPLICATE_WR_MASK_SFT                         (0x1 << 10)
#define MOD_DAI_MONO_SFT                                      8
#define MOD_DAI_MONO_MASK                                     0x1
#define MOD_DAI_MONO_MASK_SFT                                 (0x1 << 8)
#define MOD_DAI_WR_SIGN_SFT                                   6
#define MOD_DAI_WR_SIGN_MASK                                  0x1
#define MOD_DAI_WR_SIGN_MASK_SFT                              (0x1 << 6)
#define MOD_DAI_NORMAL_MODE_SFT                               5
#define MOD_DAI_NORMAL_MODE_MASK                              0x1
#define MOD_DAI_NORMAL_MODE_MASK_SFT                          (0x1 << 5)
#define MOD_DAI_HALIGN_SFT                                    4
#define MOD_DAI_HALIGN_MASK                                   0x1
#define MOD_DAI_HALIGN_MASK_SFT                               (0x1 << 4)
#define MOD_DAI_HD_MODE_SFT                                   0
#define MOD_DAI_HD_MODE_MASK                                  0x3
#define MOD_DAI_HD_MODE_MASK_SFT                              (0x3 << 0)

/* AFE_DAI2_CON0 */
#define DAI2_MODE_SFT                                         24
#define DAI2_MODE_MASK                                        0xf
#define DAI2_MODE_MASK_SFT                                    (0xf << 24)
#define DAI2_SW_CLEAR_BUF_FULL_SFT                            15
#define DAI2_SW_CLEAR_BUF_FULL_MASK                           0x1
#define DAI2_SW_CLEAR_BUF_FULL_MASK_SFT                       (0x1 << 15)
#define DAI2_DUPLICATE_WR_SFT                                 10
#define DAI2_DUPLICATE_WR_MASK                                0x1
#define DAI2_DUPLICATE_WR_MASK_SFT                            (0x1 << 10)
#define DAI2_MONO_SFT                                         8
#define DAI2_MONO_MASK                                        0x1
#define DAI2_MONO_MASK_SFT                                    (0x1 << 8)
#define DAI2_WR_SIGN_SFT                                      6
#define DAI2_WR_SIGN_MASK                                     0x1
#define DAI2_WR_SIGN_MASK_SFT                                 (0x1 << 6)
#define DAI2_NORMAL_MODE_SFT                                  5
#define DAI2_NORMAL_MODE_MASK                                 0x1
#define DAI2_NORMAL_MODE_MASK_SFT                             (0x1 << 5)
#define DAI2_HALIGN_SFT                                       4
#define DAI2_HALIGN_MASK                                      0x1
#define DAI2_HALIGN_MASK_SFT                                  (0x1 << 4)
#define DAI2_HD_MODE_SFT                                      0
#define DAI2_HD_MODE_MASK                                     0x3
#define DAI2_HD_MODE_MASK_SFT                                 (0x3 << 0)

/* AFE_MEMIF_CON0 */
#define CPU_COMPACT_MODE_SFT                                  2
#define CPU_COMPACT_MODE_MASK                                 0x1
#define CPU_COMPACT_MODE_MASK_SFT                             (0x1 << 2)
#define CPU_HD_ALIGN_SFT                                      1
#define CPU_HD_ALIGN_MASK                                     0x1
#define CPU_HD_ALIGN_MASK_SFT                                 (0x1 << 1)
#define SYSRAM_SIGN_SFT                                       0
#define SYSRAM_SIGN_MASK                                      0x1
#define SYSRAM_SIGN_MASK_SFT                                  (0x1 << 0)

/* AFE_HDMI_OUT_CON0 */
#define HDMI_CH_NUM_SFT                                       24
#define HDMI_CH_NUM_MASK                                      0xf
#define HDMI_CH_NUM_MASK_SFT                                  (0xf << 24)
#define HDMI_OUT_MINLEN_SFT                                   20
#define HDMI_OUT_MINLEN_MASK                                  0xf
#define HDMI_OUT_MINLEN_MASK_SFT                              (0xf << 20)
#define HDMI_OUT_MAXLEN_SFT                                   16
#define HDMI_OUT_MAXLEN_MASK                                  0xf
#define HDMI_OUT_MAXLEN_MASK_SFT                              (0xf << 16)
#define HDMI_OUT_SW_CLEAR_BUF_EMPTY_SFT                       15
#define HDMI_OUT_SW_CLEAR_BUF_EMPTY_MASK                      0x1
#define HDMI_OUT_SW_CLEAR_BUF_EMPTY_MASK_SFT                  (0x1 << 15)
#define HDMI_OUT_PBUF_SIZE_SFT                                12
#define HDMI_OUT_PBUF_SIZE_MASK                               0x3
#define HDMI_OUT_PBUF_SIZE_MASK_SFT                           (0x3 << 12)
#define HDMI_OUT_NORMAL_MODE_SFT                              5
#define HDMI_OUT_NORMAL_MODE_MASK                             0x1
#define HDMI_OUT_NORMAL_MODE_MASK_SFT                         (0x1 << 5)
#define HDMI_OUT_HALIGN_SFT                                   4
#define HDMI_OUT_HALIGN_MASK                                  0x1
#define HDMI_OUT_HALIGN_MASK_SFT                              (0x1 << 4)
#define HDMI_OUT_HD_MODE_SFT                                  0
#define HDMI_OUT_HD_MODE_MASK                                 0x3
#define HDMI_OUT_HD_MODE_MASK_SFT                             (0x3 << 0)

/* AFE_IRQ_MCU_CON0 */
#define IRQ31_MCU_ON_SFT                                      31
#define IRQ31_MCU_ON_MASK                                     0x1
#define IRQ31_MCU_ON_MASK_SFT                                 (0x1 << 31)
#define IRQ26_MCU_ON_SFT                                      26
#define IRQ26_MCU_ON_MASK                                     0x1
#define IRQ26_MCU_ON_MASK_SFT                                 (0x1 << 26)
#define IRQ25_MCU_ON_SFT                                      25
#define IRQ25_MCU_ON_MASK                                     0x1
#define IRQ25_MCU_ON_MASK_SFT                                 (0x1 << 25)
#define IRQ24_MCU_ON_SFT                                      24
#define IRQ24_MCU_ON_MASK                                     0x1
#define IRQ24_MCU_ON_MASK_SFT                                 (0x1 << 24)
#define IRQ23_MCU_ON_SFT                                      23
#define IRQ23_MCU_ON_MASK                                     0x1
#define IRQ23_MCU_ON_MASK_SFT                                 (0x1 << 23)
#define IRQ22_MCU_ON_SFT                                      22
#define IRQ22_MCU_ON_MASK                                     0x1
#define IRQ22_MCU_ON_MASK_SFT                                 (0x1 << 22)
#define IRQ21_MCU_ON_SFT                                      21
#define IRQ21_MCU_ON_MASK                                     0x1
#define IRQ21_MCU_ON_MASK_SFT                                 (0x1 << 21)
#define IRQ20_MCU_ON_SFT                                      20
#define IRQ20_MCU_ON_MASK                                     0x1
#define IRQ20_MCU_ON_MASK_SFT                                 (0x1 << 20)
#define IRQ19_MCU_ON_SFT                                      19
#define IRQ19_MCU_ON_MASK                                     0x1
#define IRQ19_MCU_ON_MASK_SFT                                 (0x1 << 19)
#define IRQ18_MCU_ON_SFT                                      18
#define IRQ18_MCU_ON_MASK                                     0x1
#define IRQ18_MCU_ON_MASK_SFT                                 (0x1 << 18)
#define IRQ17_MCU_ON_SFT                                      17
#define IRQ17_MCU_ON_MASK                                     0x1
#define IRQ17_MCU_ON_MASK_SFT                                 (0x1 << 17)
#define IRQ16_MCU_ON_SFT                                      16
#define IRQ16_MCU_ON_MASK                                     0x1
#define IRQ16_MCU_ON_MASK_SFT                                 (0x1 << 16)
#define IRQ15_MCU_ON_SFT                                      15
#define IRQ15_MCU_ON_MASK                                     0x1
#define IRQ15_MCU_ON_MASK_SFT                                 (0x1 << 15)
#define IRQ14_MCU_ON_SFT                                      14
#define IRQ14_MCU_ON_MASK                                     0x1
#define IRQ14_MCU_ON_MASK_SFT                                 (0x1 << 14)
#define IRQ13_MCU_ON_SFT                                      13
#define IRQ13_MCU_ON_MASK                                     0x1
#define IRQ13_MCU_ON_MASK_SFT                                 (0x1 << 13)
#define IRQ12_MCU_ON_SFT                                      12
#define IRQ12_MCU_ON_MASK                                     0x1
#define IRQ12_MCU_ON_MASK_SFT                                 (0x1 << 12)
#define IRQ11_MCU_ON_SFT                                      11
#define IRQ11_MCU_ON_MASK                                     0x1
#define IRQ11_MCU_ON_MASK_SFT                                 (0x1 << 11)
#define IRQ10_MCU_ON_SFT                                      10
#define IRQ10_MCU_ON_MASK                                     0x1
#define IRQ10_MCU_ON_MASK_SFT                                 (0x1 << 10)
#define IRQ9_MCU_ON_SFT                                       9
#define IRQ9_MCU_ON_MASK                                      0x1
#define IRQ9_MCU_ON_MASK_SFT                                  (0x1 << 9)
#define IRQ8_MCU_ON_SFT                                       8
#define IRQ8_MCU_ON_MASK                                      0x1
#define IRQ8_MCU_ON_MASK_SFT                                  (0x1 << 8)
#define IRQ7_MCU_ON_SFT                                       7
#define IRQ7_MCU_ON_MASK                                      0x1
#define IRQ7_MCU_ON_MASK_SFT                                  (0x1 << 7)
#define IRQ6_MCU_ON_SFT                                       6
#define IRQ6_MCU_ON_MASK                                      0x1
#define IRQ6_MCU_ON_MASK_SFT                                  (0x1 << 6)
#define IRQ5_MCU_ON_SFT                                       5
#define IRQ5_MCU_ON_MASK                                      0x1
#define IRQ5_MCU_ON_MASK_SFT                                  (0x1 << 5)
#define IRQ4_MCU_ON_SFT                                       4
#define IRQ4_MCU_ON_MASK                                      0x1
#define IRQ4_MCU_ON_MASK_SFT                                  (0x1 << 4)
#define IRQ3_MCU_ON_SFT                                       3
#define IRQ3_MCU_ON_MASK                                      0x1
#define IRQ3_MCU_ON_MASK_SFT                                  (0x1 << 3)
#define IRQ2_MCU_ON_SFT                                       2
#define IRQ2_MCU_ON_MASK                                      0x1
#define IRQ2_MCU_ON_MASK_SFT                                  (0x1 << 2)
#define IRQ1_MCU_ON_SFT                                       1
#define IRQ1_MCU_ON_MASK                                      0x1
#define IRQ1_MCU_ON_MASK_SFT                                  (0x1 << 1)
#define IRQ0_MCU_ON_SFT                                       0
#define IRQ0_MCU_ON_MASK                                      0x1
#define IRQ0_MCU_ON_MASK_SFT                                  (0x1 << 0)

/* AFE_IRQ_MCU_CON1 */
#define IRQ7_MCU_MODE_SFT                                     28
#define IRQ7_MCU_MODE_MASK                                    0xf
#define IRQ7_MCU_MODE_MASK_SFT                                (0xf << 28)
#define IRQ6_MCU_MODE_SFT                                     24
#define IRQ6_MCU_MODE_MASK                                    0xf
#define IRQ6_MCU_MODE_MASK_SFT                                (0xf << 24)
#define IRQ5_MCU_MODE_SFT                                     20
#define IRQ5_MCU_MODE_MASK                                    0xf
#define IRQ5_MCU_MODE_MASK_SFT                                (0xf << 20)
#define IRQ4_MCU_MODE_SFT                                     16
#define IRQ4_MCU_MODE_MASK                                    0xf
#define IRQ4_MCU_MODE_MASK_SFT                                (0xf << 16)
#define IRQ3_MCU_MODE_SFT                                     12
#define IRQ3_MCU_MODE_MASK                                    0xf
#define IRQ3_MCU_MODE_MASK_SFT                                (0xf << 12)
#define IRQ2_MCU_MODE_SFT                                     8
#define IRQ2_MCU_MODE_MASK                                    0xf
#define IRQ2_MCU_MODE_MASK_SFT                                (0xf << 8)
#define IRQ1_MCU_MODE_SFT                                     4
#define IRQ1_MCU_MODE_MASK                                    0xf
#define IRQ1_MCU_MODE_MASK_SFT                                (0xf << 4)
#define IRQ0_MCU_MODE_SFT                                     0
#define IRQ0_MCU_MODE_MASK                                    0xf
#define IRQ0_MCU_MODE_MASK_SFT                                (0xf << 0)

/* AFE_IRQ_MCU_CON2 */
#define IRQ15_MCU_MODE_SFT                                    28
#define IRQ15_MCU_MODE_MASK                                   0xf
#define IRQ15_MCU_MODE_MASK_SFT                               (0xf << 28)
#define IRQ14_MCU_MODE_SFT                                    24
#define IRQ14_MCU_MODE_MASK                                   0xf
#define IRQ14_MCU_MODE_MASK_SFT                               (0xf << 24)
#define IRQ13_MCU_MODE_SFT                                    20
#define IRQ13_MCU_MODE_MASK                                   0xf
#define IRQ13_MCU_MODE_MASK_SFT                               (0xf << 20)
#define IRQ12_MCU_MODE_SFT                                    16
#define IRQ12_MCU_MODE_MASK                                   0xf
#define IRQ12_MCU_MODE_MASK_SFT                               (0xf << 16)
#define IRQ11_MCU_MODE_SFT                                    12
#define IRQ11_MCU_MODE_MASK                                   0xf
#define IRQ11_MCU_MODE_MASK_SFT                               (0xf << 12)
#define IRQ10_MCU_MODE_SFT                                    8
#define IRQ10_MCU_MODE_MASK                                   0xf
#define IRQ10_MCU_MODE_MASK_SFT                               (0xf << 8)
#define IRQ9_MCU_MODE_SFT                                     4
#define IRQ9_MCU_MODE_MASK                                    0xf
#define IRQ9_MCU_MODE_MASK_SFT                                (0xf << 4)
#define IRQ8_MCU_MODE_SFT                                     0
#define IRQ8_MCU_MODE_MASK                                    0xf
#define IRQ8_MCU_MODE_MASK_SFT                                (0xf << 0)

/* AFE_IRQ_MCU_CON3 */
#define IRQ23_MCU_MODE_SFT                                    28
#define IRQ23_MCU_MODE_MASK                                   0xf
#define IRQ23_MCU_MODE_MASK_SFT                               (0xf << 28)
#define IRQ22_MCU_MODE_SFT                                    24
#define IRQ22_MCU_MODE_MASK                                   0xf
#define IRQ22_MCU_MODE_MASK_SFT                               (0xf << 24)
#define IRQ21_MCU_MODE_SFT                                    20
#define IRQ21_MCU_MODE_MASK                                   0xf
#define IRQ21_MCU_MODE_MASK_SFT                               (0xf << 20)
#define IRQ20_MCU_MODE_SFT                                    16
#define IRQ20_MCU_MODE_MASK                                   0xf
#define IRQ20_MCU_MODE_MASK_SFT                               (0xf << 16)
#define IRQ19_MCU_MODE_SFT                                    12
#define IRQ19_MCU_MODE_MASK                                   0xf
#define IRQ19_MCU_MODE_MASK_SFT                               (0xf << 12)
#define IRQ18_MCU_MODE_SFT                                    8
#define IRQ18_MCU_MODE_MASK                                   0xf
#define IRQ18_MCU_MODE_MASK_SFT                               (0xf << 8)
#define IRQ17_MCU_MODE_SFT                                    4
#define IRQ17_MCU_MODE_MASK                                   0xf
#define IRQ17_MCU_MODE_MASK_SFT                               (0xf << 4)
#define IRQ16_MCU_MODE_SFT                                    0
#define IRQ16_MCU_MODE_MASK                                   0xf
#define IRQ16_MCU_MODE_MASK_SFT                               (0xf << 0)

/* AFE_IRQ_MCU_CON4 */
#define IRQ26_MCU_MODE_SFT                                    8
#define IRQ26_MCU_MODE_MASK                                   0xf
#define IRQ26_MCU_MODE_MASK_SFT                               (0xf << 8)
#define IRQ25_MCU_MODE_SFT                                    4
#define IRQ25_MCU_MODE_MASK                                   0xf
#define IRQ25_MCU_MODE_MASK_SFT                               (0xf << 4)
#define IRQ24_MCU_MODE_SFT                                    0
#define IRQ24_MCU_MODE_MASK                                   0xf
#define IRQ24_MCU_MODE_MASK_SFT                               (0xf << 0)

/* AFE_IRQ_MCU_CLR */
#define IRQ31_MCU_CLR_SFT                                     31
#define IRQ31_MCU_CLR_MASK                                    0x1
#define IRQ31_MCU_CLR_MASK_SFT                                (0x1 << 31)
#define IRQ26_MCU_CLR_SFT                                     26
#define IRQ26_MCU_CLR_MASK                                    0x1
#define IRQ26_MCU_CLR_MASK_SFT                                (0x1 << 26)
#define IRQ25_MCU_CLR_SFT                                     25
#define IRQ25_MCU_CLR_MASK                                    0x1
#define IRQ25_MCU_CLR_MASK_SFT                                (0x1 << 25)
#define IRQ24_MCU_CLR_SFT                                     24
#define IRQ24_MCU_CLR_MASK                                    0x1
#define IRQ24_MCU_CLR_MASK_SFT                                (0x1 << 24)
#define IRQ23_MCU_CLR_SFT                                     23
#define IRQ23_MCU_CLR_MASK                                    0x1
#define IRQ23_MCU_CLR_MASK_SFT                                (0x1 << 23)
#define IRQ22_MCU_CLR_SFT                                     22
#define IRQ22_MCU_CLR_MASK                                    0x1
#define IRQ22_MCU_CLR_MASK_SFT                                (0x1 << 22)
#define IRQ21_MCU_CLR_SFT                                     21
#define IRQ21_MCU_CLR_MASK                                    0x1
#define IRQ21_MCU_CLR_MASK_SFT                                (0x1 << 21)
#define IRQ20_MCU_CLR_SFT                                     20
#define IRQ20_MCU_CLR_MASK                                    0x1
#define IRQ20_MCU_CLR_MASK_SFT                                (0x1 << 20)
#define IRQ19_MCU_CLR_SFT                                     19
#define IRQ19_MCU_CLR_MASK                                    0x1
#define IRQ19_MCU_CLR_MASK_SFT                                (0x1 << 19)
#define IRQ18_MCU_CLR_SFT                                     18
#define IRQ18_MCU_CLR_MASK                                    0x1
#define IRQ18_MCU_CLR_MASK_SFT                                (0x1 << 18)
#define IRQ17_MCU_CLR_SFT                                     17
#define IRQ17_MCU_CLR_MASK                                    0x1
#define IRQ17_MCU_CLR_MASK_SFT                                (0x1 << 17)
#define IRQ16_MCU_CLR_SFT                                     16
#define IRQ16_MCU_CLR_MASK                                    0x1
#define IRQ16_MCU_CLR_MASK_SFT                                (0x1 << 16)
#define IRQ15_MCU_CLR_SFT                                     15
#define IRQ15_MCU_CLR_MASK                                    0x1
#define IRQ15_MCU_CLR_MASK_SFT                                (0x1 << 15)
#define IRQ14_MCU_CLR_SFT                                     14
#define IRQ14_MCU_CLR_MASK                                    0x1
#define IRQ14_MCU_CLR_MASK_SFT                                (0x1 << 14)
#define IRQ13_MCU_CLR_SFT                                     13
#define IRQ13_MCU_CLR_MASK                                    0x1
#define IRQ13_MCU_CLR_MASK_SFT                                (0x1 << 13)
#define IRQ12_MCU_CLR_SFT                                     12
#define IRQ12_MCU_CLR_MASK                                    0x1
#define IRQ12_MCU_CLR_MASK_SFT                                (0x1 << 12)
#define IRQ11_MCU_CLR_SFT                                     11
#define IRQ11_MCU_CLR_MASK                                    0x1
#define IRQ11_MCU_CLR_MASK_SFT                                (0x1 << 11)
#define IRQ10_MCU_CLR_SFT                                     10
#define IRQ10_MCU_CLR_MASK                                    0x1
#define IRQ10_MCU_CLR_MASK_SFT                                (0x1 << 10)
#define IRQ9_MCU_CLR_SFT                                      9
#define IRQ9_MCU_CLR_MASK                                     0x1
#define IRQ9_MCU_CLR_MASK_SFT                                 (0x1 << 9)
#define IRQ8_MCU_CLR_SFT                                      8
#define IRQ8_MCU_CLR_MASK                                     0x1
#define IRQ8_MCU_CLR_MASK_SFT                                 (0x1 << 8)
#define IRQ7_MCU_CLR_SFT                                      7
#define IRQ7_MCU_CLR_MASK                                     0x1
#define IRQ7_MCU_CLR_MASK_SFT                                 (0x1 << 7)
#define IRQ6_MCU_CLR_SFT                                      6
#define IRQ6_MCU_CLR_MASK                                     0x1
#define IRQ6_MCU_CLR_MASK_SFT                                 (0x1 << 6)
#define IRQ5_MCU_CLR_SFT                                      5
#define IRQ5_MCU_CLR_MASK                                     0x1
#define IRQ5_MCU_CLR_MASK_SFT                                 (0x1 << 5)
#define IRQ4_MCU_CLR_SFT                                      4
#define IRQ4_MCU_CLR_MASK                                     0x1
#define IRQ4_MCU_CLR_MASK_SFT                                 (0x1 << 4)
#define IRQ3_MCU_CLR_SFT                                      3
#define IRQ3_MCU_CLR_MASK                                     0x1
#define IRQ3_MCU_CLR_MASK_SFT                                 (0x1 << 3)
#define IRQ2_MCU_CLR_SFT                                      2
#define IRQ2_MCU_CLR_MASK                                     0x1
#define IRQ2_MCU_CLR_MASK_SFT                                 (0x1 << 2)
#define IRQ1_MCU_CLR_SFT                                      1
#define IRQ1_MCU_CLR_MASK                                     0x1
#define IRQ1_MCU_CLR_MASK_SFT                                 (0x1 << 1)
#define IRQ0_MCU_CLR_SFT                                      0
#define IRQ0_MCU_CLR_MASK                                     0x1
#define IRQ0_MCU_CLR_MASK_SFT                                 (0x1 << 0)

/* AFE_IRQ_MCU_EN */
#define IRQ31_MCU_EN_SFT                                     31
#define IRQ30_MCU_EN_SFT                                     30
#define IRQ29_MCU_EN_SFT                                     29
#define IRQ28_MCU_EN_SFT                                     28
#define IRQ27_MCU_EN_SFT                                           27
#define IRQ26_MCU_EN_SFT                                     26
#define IRQ25_MCU_EN_SFT                                     25
#define IRQ24_MCU_EN_SFT                                     24
#define IRQ23_MCU_EN_SFT                                     23
#define IRQ22_MCU_EN_SFT                                     22
#define IRQ21_MCU_EN_SFT                                     21
#define IRQ20_MCU_EN_SFT                                     20
#define IRQ19_MCU_EN_SFT                                     19
#define IRQ18_MCU_EN_SFT                                     18
#define IRQ17_MCU_EN_SFT                                     17
#define IRQ16_MCU_EN_SFT                                     16
#define IRQ15_MCU_EN_SFT                                     15
#define IRQ14_MCU_EN_SFT                                     14
#define IRQ13_MCU_EN_SFT                                     13
#define IRQ12_MCU_EN_SFT                                     12
#define IRQ11_MCU_EN_SFT                                     11
#define IRQ10_MCU_EN_SFT                                     10
#define IRQ9_MCU_EN_SFT                                      9
#define IRQ8_MCU_EN_SFT                                      8
#define IRQ7_MCU_EN_SFT                                      7
#define IRQ6_MCU_EN_SFT                                      6
#define IRQ5_MCU_EN_SFT                                      5
#define IRQ4_MCU_EN_SFT                                      4
#define IRQ3_MCU_EN_SFT                                      3
#define IRQ2_MCU_EN_SFT                                      2
#define IRQ1_MCU_EN_SFT                                      1
#define IRQ0_MCU_EN_SFT                                      0

/* AFE_IRQ_MCU_SCP_EN */
#define IRQ31_MCU_SCP_EN_SFT                                 31
#define IRQ30_MCU_SCP_EN_SFT                                 30
#define IRQ29_MCU_SCP_EN_SFT                                 29
#define IRQ28_MCU_SCP_EN_SFT                                 28
#define IRQ27_MCU_SCP_EN_SFT                                 27
#define IRQ26_MCU_SCP_EN_SFT                                 26
#define IRQ25_MCU_SCP_EN_SFT                                 25
#define IRQ24_MCU_SCP_EN_SFT                                 24
#define IRQ23_MCU_SCP_EN_SFT                                 23
#define IRQ22_MCU_SCP_EN_SFT                                 22
#define IRQ21_MCU_SCP_EN_SFT                                 21
#define IRQ20_MCU_SCP_EN_SFT                                 20
#define IRQ19_MCU_SCP_EN_SFT                                 19
#define IRQ18_MCU_SCP_EN_SFT                                 18
#define IRQ17_MCU_SCP_EN_SFT                                 17
#define IRQ16_MCU_SCP_EN_SFT                                 16
#define IRQ15_MCU_SCP_EN_SFT                                 15
#define IRQ14_MCU_SCP_EN_SFT                                 14
#define IRQ13_MCU_SCP_EN_SFT                                 13
#define IRQ12_MCU_SCP_EN_SFT                                 12
#define IRQ11_MCU_SCP_EN_SFT                                 11
#define IRQ10_MCU_SCP_EN_SFT                                 10
#define IRQ9_MCU_SCP_EN_SFT                                  9
#define IRQ8_MCU_SCP_EN_SFT                                  8
#define IRQ7_MCU_SCP_EN_SFT                                  7
#define IRQ6_MCU_SCP_EN_SFT                                  6
#define IRQ5_MCU_SCP_EN_SFT                                  5
#define IRQ4_MCU_SCP_EN_SFT                                  4
#define IRQ3_MCU_SCP_EN_SFT                                  3
#define IRQ2_MCU_SCP_EN_SFT                                  2
#define IRQ1_MCU_SCP_EN_SFT                                  1
#define IRQ0_MCU_SCP_EN_SFT                                  0

/* AFE_TDM_CON1 */
#define TDM_EN_SFT                                            0
#define TDM_EN_MASK                                           0x1
#define TDM_EN_MASK_SFT                                       (0x1 << 0)
#define BCK_INVERSE_SFT                                       1
#define BCK_INVERSE_MASK                                      0x1
#define BCK_INVERSE_MASK_SFT                                  (0x1 << 1)
#define LRCK_INVERSE_SFT                                      2
#define LRCK_INVERSE_MASK                                     0x1
#define LRCK_INVERSE_MASK_SFT                                 (0x1 << 2)
#define DELAY_DATA_SFT                                        3
#define DELAY_DATA_MASK                                       0x1
#define DELAY_DATA_MASK_SFT                                   (0x1 << 3)
#define LEFT_ALIGN_SFT                                        4
#define LEFT_ALIGN_MASK                                       0x1
#define LEFT_ALIGN_MASK_SFT                                   (0x1 << 4)
#define WLEN_SFT                                              8
#define WLEN_MASK                                             0x3
#define WLEN_MASK_SFT                                         (0x3 << 8)
#define CHANNEL_NUM_SFT                                       10
#define CHANNEL_NUM_MASK                                      0x3
#define CHANNEL_NUM_MASK_SFT                                  (0x3 << 10)
#define CHANNEL_BCK_CYCLES_SFT                                12
#define CHANNEL_BCK_CYCLES_MASK                               0x3
#define CHANNEL_BCK_CYCLES_MASK_SFT                           (0x3 << 12)
#define DAC_BIT_NUM_SFT                                       16
#define DAC_BIT_NUM_MASK                                      0x1f
#define DAC_BIT_NUM_MASK_SFT                                  (0x1f << 16)
#define LRCK_TDM_WIDTH_SFT                                    24
#define LRCK_TDM_WIDTH_MASK                                   0xff
#define LRCK_TDM_WIDTH_MASK_SFT                               (0xff << 24)

/* AFE_TDM_CON2 */
#define ST_CH_PAIR_SOUT0_SFT                                  0
#define ST_CH_PAIR_SOUT0_MASK                                 0x7
#define ST_CH_PAIR_SOUT0_MASK_SFT                             (0x7 << 0)
#define ST_CH_PAIR_SOUT1_SFT                                  4
#define ST_CH_PAIR_SOUT1_MASK                                 0x7
#define ST_CH_PAIR_SOUT1_MASK_SFT                             (0x7 << 4)
#define ST_CH_PAIR_SOUT2_SFT                                  8
#define ST_CH_PAIR_SOUT2_MASK                                 0x7
#define ST_CH_PAIR_SOUT2_MASK_SFT                             (0x7 << 8)
#define ST_CH_PAIR_SOUT3_SFT                                  12
#define ST_CH_PAIR_SOUT3_MASK                                 0x7
#define ST_CH_PAIR_SOUT3_MASK_SFT                             (0x7 << 12)
#define TDM_FIX_VALUE_SEL_SFT                                 16
#define TDM_FIX_VALUE_SEL_MASK                                0x1
#define TDM_FIX_VALUE_SEL_MASK_SFT                            (0x1 << 16)
#define TDM_I2S_LOOPBACK_SFT                                  20
#define TDM_I2S_LOOPBACK_MASK                                 0x1
#define TDM_I2S_LOOPBACK_MASK_SFT                             (0x1 << 20)
#define TDM_I2S_LOOPBACK_CH_SFT                               21
#define TDM_I2S_LOOPBACK_CH_MASK                              0x3
#define TDM_I2S_LOOPBACK_CH_MASK_SFT                          (0x3 << 21)
#define TDM_FIX_VALUE_SFT                                     24
#define TDM_FIX_VALUE_MASK                                    0xff
#define TDM_FIX_VALUE_MASK_SFT                                (0xff << 24)

/* AFE_DPTX_CON */
#define DPTX_CHANNEL_ENABLE_SFT                               8
#define DPTX_CHANNEL_ENABLE_MASK                              0xff
#define DPTX_CHANNEL_ENABLE_MASK_SFT                          (0xff << 8)
#define DPTX_REGISTER_MONITOR_SELECT_SFT                      3
#define DPTX_REGISTER_MONITOR_SELECT_MASK                     0x3
#define DPTX_REGISTER_MONITOR_SELECT_MASK_SFT                 (0x3 << 3)
#define DPTX_16_BIT_SFT                                       2
#define DPTX_16_BIT_MASK                                      0x1
#define DPTX_16_BIT_MASK_SFT                                  (0x1 << 2)
#define DPTX_CHANNEL_NUM_SFT                                  1
#define DPTX_CHANNEL_NUM_MASK                                 0x1
#define DPTX_CHANNEL_NUM_MASK_SFT                             (0x1 << 1)
#define DPTX_ON_SFT                                           0
#define DPTX_ON_MASK                                          0x1
#define DPTX_ON_MASK_SFT                                      (0x1 << 0)

/* AFE_HDMI_CONN0 */
#define HDMI_O_7_SFT                                          21
#define HDMI_O_7_MASK                                         0x7
#define HDMI_O_7_MASK_SFT                                     (0x7 << 21)
#define HDMI_O_6_SFT                                          18
#define HDMI_O_6_MASK                                         0x7
#define HDMI_O_6_MASK_SFT                                     (0x7 << 18)
#define HDMI_O_5_SFT                                          15
#define HDMI_O_5_MASK                                         0x7
#define HDMI_O_5_MASK_SFT                                     (0x7 << 15)
#define HDMI_O_4_SFT                                          12
#define HDMI_O_4_MASK                                         0x7
#define HDMI_O_4_MASK_SFT                                     (0x7 << 12)
#define HDMI_O_3_SFT                                          9
#define HDMI_O_3_MASK                                         0x7
#define HDMI_O_3_MASK_SFT                                     (0x7 << 9)
#define HDMI_O_2_SFT                                          6
#define HDMI_O_2_MASK                                         0x7
#define HDMI_O_2_MASK_SFT                                     (0x7 << 6)
#define HDMI_O_1_SFT                                          3
#define HDMI_O_1_MASK                                         0x7
#define HDMI_O_1_MASK_SFT                                     (0x7 << 3)
#define HDMI_O_0_SFT                                          0
#define HDMI_O_0_MASK                                         0x7
#define HDMI_O_0_MASK_SFT                                     (0x7 << 0)

/* AFE_AUD_PAD_TOP */
#define AUD_PAD_TOP_MON_SFT                                   15
#define AUD_PAD_TOP_MON_MASK                                  0x1ffff
#define AUD_PAD_TOP_MON_MASK_SFT                              (0x1ffff << 15)
#define AUD_PAD_TOP_CLOCK_EN_SFT                              7
#define AUD_PAD_TOP_CLOCK_EN_MASK                             0x1
#define AUD_PAD_TOP_CLOCK_EN_MASK_SFT                         (0x1 << 7)
#define AUD_PAD_TOP_FIFO_RSP_SFT                              4
#define AUD_PAD_TOP_FIFO_RSP_MASK                             0x7
#define AUD_PAD_TOP_FIFO_RSP_MASK_SFT                         (0x7 << 4)
#define RG_RX_PROTOCOL2_SFT                                   3
#define RG_RX_PROTOCOL2_MASK                                  0x1
#define RG_RX_PROTOCOL2_MASK_SFT                              (0x1 << 3)
#define RESERVDED_01_SFT                                      1
#define RESERVDED_01_MASK                                     0x3
#define RESERVDED_01_MASK_SFT                                 (0x3 << 1)
#define RG_RX_FIFO_ON_SFT                                     0
#define RG_RX_FIFO_ON_MASK                                    0x1
#define RG_RX_FIFO_ON_MASK_SFT                                (0x1 << 0)

/* AFE_ADDA_MTKAIF_SYNCWORD_CFG */
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT             23
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK            0x1
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK_SFT        (0x1 << 23)
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_SFT                     20
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_MASK                    0x7
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD2_MASK_SFT                (0x7 << 20)
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_DISABLE_SFT             19
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK            0x1
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK_SFT        (0x1 << 19)
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_SFT                     16
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_MASK                    0x7
#define RG_ADDA6_MTKAIF_RX_SYNC_WORD1_MASK_SFT                (0x7 << 16)
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT              15
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK             0x1
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_DISABLE_MASK_SFT         (0x1 << 15)
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_SFT                      12
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_MASK                     0x7
#define RG_ADDA_MTKAIF_RX_SYNC_WORD2_MASK_SFT                 (0x7 << 12)
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_SFT              11
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK             0x1
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_DISABLE_MASK_SFT         (0x1 << 11)
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_SFT                      8
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_MASK                     0x7
#define RG_ADDA_MTKAIF_RX_SYNC_WORD1_MASK_SFT                 (0x7 << 8)
#define RG_MTKAIF_TX_SYNC_WORD2_SFT                           4
#define RG_MTKAIF_TX_SYNC_WORD2_MASK                          0x7
#define RG_MTKAIF_TX_SYNC_WORD2_MASK_SFT                      (0x7 << 4)
#define RG_MTKAIF_TX_SYNC_WORD1_SFT                           0
#define RG_MTKAIF_TX_SYNC_WORD1_MASK                          0x7
#define RG_MTKAIF_TX_SYNC_WORD1_MASK_SFT                      (0x7 << 0)


/* AFE_ADDA_MTKAIF_RX_CFG0 */
#define MTKAIF_RXIF_VOICE_MODE_SFT                            20
#define MTKAIF_RXIF_VOICE_MODE_MASK                           0xf
#define MTKAIF_RXIF_VOICE_MODE_MASK_SFT                       (0xf << 20)
#define MTKAIF_RXIF_DETECT_ON_SFT                             16
#define MTKAIF_RXIF_DETECT_ON_MASK                            0x1
#define MTKAIF_RXIF_DETECT_ON_MASK_SFT                        (0x1 << 16)
#define MTKAIF_RXIF_DATA_BIT_SFT                              8
#define MTKAIF_RXIF_DATA_BIT_MASK                             0x7
#define MTKAIF_RXIF_DATA_BIT_MASK_SFT                         (0x7 << 8)
#define MTKAIF_RXIF_FIFO_RSP_SFT                              4
#define MTKAIF_RXIF_FIFO_RSP_MASK                             0x7
#define MTKAIF_RXIF_FIFO_RSP_MASK_SFT                         (0x7 << 4)
#define MTKAIF_RXIF_DATA_MODE_SFT                             0
#define MTKAIF_RXIF_DATA_MODE_MASK                            0x1
#define MTKAIF_RXIF_DATA_MODE_MASK_SFT                        (0x1 << 0)

/* GENERAL_ASRC_MODE */
#define GENERAL3_ASRCOUT_MODE_SFT                             20
#define GENERAL3_ASRCOUT_MODE_MASK                            0xf
#define GENERAL3_ASRCOUT_MODE_MASK_SFT                        (0xf << 20)
#define GENERAL3_ASRCIN_MODE_SFT                              16
#define GENERAL3_ASRCIN_MODE_MASK                             0xf
#define GENERAL3_ASRCIN_MODE_MASK_SFT                         (0xf << 16)
#define GENERAL2_ASRCOUT_MODE_SFT                             12
#define GENERAL2_ASRCOUT_MODE_MASK                            0xf
#define GENERAL2_ASRCOUT_MODE_MASK_SFT                        (0xf << 12)
#define GENERAL2_ASRCIN_MODE_SFT                              8
#define GENERAL2_ASRCIN_MODE_MASK                             0xf
#define GENERAL2_ASRCIN_MODE_MASK_SFT                         (0xf << 8)
#define GENERAL1_ASRCOUT_MODE_SFT                             4
#define GENERAL1_ASRCOUT_MODE_MASK                            0xf
#define GENERAL1_ASRCOUT_MODE_MASK_SFT                        (0xf << 4)
#define GENERAL1_ASRCIN_MODE_SFT                              0
#define GENERAL1_ASRCIN_MODE_MASK                             0xf
#define GENERAL1_ASRCIN_MODE_MASK_SFT                         (0xf << 0)

/* GENERAL_ASRC_EN_ON */
#define GENERAL3_ASRC_EN_ON_SFT                               2
#define GENERAL3_ASRC_EN_ON_MASK                              0x1
#define GENERAL3_ASRC_EN_ON_MASK_SFT                          (0x1 << 2)
#define GENERAL2_ASRC_EN_ON_SFT                               1
#define GENERAL2_ASRC_EN_ON_MASK                              0x1
#define GENERAL2_ASRC_EN_ON_MASK_SFT                          (0x1 << 1)
#define GENERAL1_ASRC_EN_ON_SFT                               0
#define GENERAL1_ASRC_EN_ON_MASK                              0x1
#define GENERAL1_ASRC_EN_ON_MASK_SFT                          (0x1 << 0)

/* AFE_GENERAL1_ASRC_2CH_CON0 */
#define G_SRC_CHSET_STR_CLR_SFT                               4
#define G_SRC_CHSET_STR_CLR_MASK                              0x1
#define G_SRC_CHSET_STR_CLR_MASK_SFT                          (0x1 << 4)
#define G_SRC_CHSET_ON_SFT                                    2
#define G_SRC_CHSET_ON_MASK                                   0x1
#define G_SRC_CHSET_ON_MASK_SFT                               (0x1 << 2)
#define G_SRC_COEFF_SRAM_CTRL_SFT                             1
#define G_SRC_COEFF_SRAM_CTRL_MASK                            0x1
#define G_SRC_COEFF_SRAM_CTRL_MASK_SFT                        (0x1 << 1)
#define G_SRC_ASM_ON_SFT                                      0
#define G_SRC_ASM_ON_MASK                                     0x1
#define G_SRC_ASM_ON_MASK_SFT                                 (0x1 << 0)

/* AFE_GENERAL1_ASRC_2CH_CON2 */
#define G_SRC_CHSET_O16BIT_SFT                                19
#define G_SRC_CHSET_O16BIT_MASK                               0x1
#define G_SRC_CHSET_O16BIT_MASK_SFT                           (0x1 << 19)
#define G_SRC_CHSET_CLR_IIR_HISTORY_SFT                       17
#define G_SRC_CHSET_CLR_IIR_HISTORY_MASK                      0x1
#define G_SRC_CHSET_CLR_IIR_HISTORY_MASK_SFT                  (0x1 << 17)
#define G_SRC_CHSET_IS_MONO_SFT                               16
#define G_SRC_CHSET_IS_MONO_MASK                              0x1
#define G_SRC_CHSET_IS_MONO_MASK_SFT                          (0x1 << 16)
#define G_SRC_CHSET_IIR_EN_SFT                                11
#define G_SRC_CHSET_IIR_EN_MASK                               0x1
#define G_SRC_CHSET_IIR_EN_MASK_SFT                           (0x1 << 11)
#define G_SRC_CHSET_IIR_STAGE_SFT                             8
#define G_SRC_CHSET_IIR_STAGE_MASK                            0x7
#define G_SRC_CHSET_IIR_STAGE_MASK_SFT                        (0x7 << 8)

/* AFE_GENERAL1_ASRC_2CH_CON3 */
#define G_SRC_ASM_FREQ_4_SFT                                  0
#define G_SRC_ASM_FREQ_4_MASK                                 0xffffff
#define G_SRC_ASM_FREQ_4_MASK_SFT                             (0xffffff << 0)

/* AFE_GENERAL1_ASRC_2CH_CON4 */
#define G_SRC_ASM_FREQ_5_SFT                                  0
#define G_SRC_ASM_FREQ_5_MASK                                 0xffffff
#define G_SRC_ASM_FREQ_5_MASK_SFT                             (0xffffff << 0)

/* AFE_GENERAL1_ASRC_2CH_CON13 */
#define G_SRC_COEFF_SRAM_ADR_SFT                              0
#define G_SRC_COEFF_SRAM_ADR_MASK                             0x3f
#define G_SRC_COEFF_SRAM_ADR_MASK_SFT                         (0x3f << 0)

/* AFE_ADDA_DL_SDM_AUTO_RESET_CON */
#define ADDA_SDM_AUTO_RESET_ONOFF_SFT                         31
#define ADDA_SDM_AUTO_RESET_ONOFF_MASK                        0x1
#define ADDA_SDM_AUTO_RESET_ONOFF_MASK_SFT                    (0x1 << 31)
#define ADDA_USE_NEW_2ND_SDM_SFT                              28
#define ADDA_USE_NEW_2ND_SDM_MASK                             0x1
#define ADDA_USE_NEW_2ND_SDM_MASK_SFT                         (0x1 << 28)
#define ADDA_SDM_AUTO_RESET_COUNT_TH_SFT                      0
#define ADDA_SDM_AUTO_RESET_COUNT_TH_MASK                     0xffffff
#define ADDA_SDM_AUTO_RESET_COUNT_TH_MASK_SFT                 (0xffffff << 0)

/* AFE_ADDA_3RD_DAC_DL_SDM_AUTO_RESET_CON */
#define ADDA_3RD_DAC_SDM_AUTO_RESET_ONOFF_SFT                 31
#define ADDA_3RD_DAC_SDM_AUTO_RESET_ONOFF_MASK                0x1
#define ADDA_3RD_DAC_SDM_AUTO_RESET_ONOFF_MASK_SFT            (0x1 << 31)
#define ADDA_3RD_DAC_USE_NEW_2ND_SDM_SFT                      28
#define ADDA_3RD_DAC_USE_NEW_2ND_SDM_MASK                     0x1
#define ADDA_3RD_DAC_USE_NEW_2ND_SDM_MASK_SFT                 (0x1 << 28)
#define ADDA_3RD_DAC_SDM_AUTO_RESET_COUNT_TH_SFT              0
#define ADDA_3RD_DAC_SDM_AUTO_RESET_COUNT_TH_MASK             0xffffff
#define ADDA_3RD_DAC_SDM_AUTO_RESET_COUNT_TH_MASK_SFT         (0xffffff << 0)

/* AFE_TINY_CONN0 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_3_CFG_SFT                                           24
#define O_3_CFG_MASK                                          0x3f
#define O_3_CFG_MASK_SFT                                      (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_2_CFG_SFT                                           16
#define O_2_CFG_MASK                                          0x3f
#define O_2_CFG_MASK_SFT                                      (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_1_CFG_SFT                                           8
#define O_1_CFG_MASK                                          0x3f
#define O_1_CFG_MASK_SFT                                      (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_0_CFG_SFT                                           0
#define O_0_CFG_MASK                                          0x3f
#define O_0_CFG_MASK_SFT                                      (0x3f << 0)

/* AFE_TINY_CONN1 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_7_CFG_SFT                                           24
#define O_7_CFG_MASK                                          0x3f
#define O_7_CFG_MASK_SFT                                      (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_6_CFG_SFT                                           16
#define O_6_CFG_MASK                                          0x3f
#define O_6_CFG_MASK_SFT                                      (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_5_CFG_SFT                                           8
#define O_5_CFG_MASK                                          0x3f
#define O_5_CFG_MASK_SFT                                      (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_4_CFG_SFT                                           0
#define O_4_CFG_MASK                                          0x3f
#define O_4_CFG_MASK_SFT                                      (0x3f << 0)

/* AFE_TINY_CONN5 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_23_CFG_SFT                                          24
#define O_23_CFG_MASK                                         0x3f
#define O_23_CFG_MASK_SFT                                     (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_22_CFG_SFT                                          16
#define O_22_CFG_MASK                                         0x3f
#define O_22_CFG_MASK_SFT                                     (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_21_CFG_SFT                                          8
#define O_21_CFG_MASK                                         0x3f
#define O_21_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_20_CFG_SFT                                          0
#define O_20_CFG_MASK                                         0x3f
#define O_20_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_TINY_CONN8 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_35_CFG_SFT                                          24
#define O_35_CFG_MASK                                         0x3f
#define O_35_CFG_MASK_SFT                                     (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_34_CFG_SFT                                          16
#define O_34_CFG_MASK                                         0x3f
#define O_34_CFG_MASK_SFT                                     (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_33_CFG_SFT                                          8
#define O_33_CFG_MASK                                         0x3f
#define O_33_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_32_CFG_SFT                                          0
#define O_32_CFG_MASK                                         0x3f
#define O_32_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_TINY_CONN9 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_39_CFG_SFT                                          24
#define O_39_CFG_MASK                                         0x3f
#define O_39_CFG_MASK_SFT                                     (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_38_CFG_SFT                                          16
#define O_38_CFG_MASK                                         0x3f
#define O_38_CFG_MASK_SFT                                     (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_37_CFG_SFT                                          8
#define O_37_CFG_MASK                                         0x3f
#define O_37_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_36_CFG_SFT                                          0
#define O_36_CFG_MASK                                         0x3f
#define O_36_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_TINY_CONN10 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_43_CFG_SFT                                          24
#define O_43_CFG_MASK                                         0x3f
#define O_43_CFG_MASK_SFT                                     (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_42_CFG_SFT                                          16
#define O_42_CFG_MASK                                         0x3f
#define O_42_CFG_MASK_SFT                                     (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_41_CFG_SFT                                          8
#define O_41_CFG_MASK                                         0x3f
#define O_41_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_40_CFG_SFT                                          0
#define O_40_CFG_MASK                                         0x3f
#define O_40_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_TINY_CONN11 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define O_47_CFG_SFT                                          24
#define O_47_CFG_MASK                                         0x3f
#define O_47_CFG_MASK_SFT                                     (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define O_46_CFG_SFT                                          16
#define O_46_CFG_MASK                                         0x3f
#define O_46_CFG_MASK_SFT                                     (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_45_CFG_SFT                                          8
#define O_45_CFG_MASK                                         0x3f
#define O_45_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_44_CFG_SFT                                          0
#define O_44_CFG_MASK                                         0x3f
#define O_44_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_TINY_CONN12 */
#define RESERVED_03_SFT                                       30
#define RESERVED_03_MASK                                      0x3
#define RESERVED_03_MASK_SFT                                  (0x3 << 30)
#define RESERVED_04_SFT                                       24
#define RESERVED_04_MASK                                      0x3f
#define RESERVED_04_MASK_SFT                                  (0x3f << 24)
#define RESERVED_02_SFT                                       22
#define RESERVED_02_MASK                                      0x3
#define RESERVED_02_MASK_SFT                                  (0x3 << 22)
#define RESERVED_05_SFT                                       16
#define RESERVED_05_MASK                                      0x3f
#define RESERVED_05_MASK_SFT                                  (0x3f << 16)
#define RESERVED_01_SFT                                       14
#define RESERVED_01_MASK                                      0x3
#define RESERVED_01_MASK_SFT                                  (0x3 << 14)
#define O_49_CFG_SFT                                          8
#define O_49_CFG_MASK                                         0x3f
#define O_49_CFG_MASK_SFT                                     (0x3f << 8)
#define RESERVED_00_SFT                                       6
#define RESERVED_00_MASK                                      0x3
#define RESERVED_00_MASK_SFT                                  (0x3 << 6)
#define O_48_CFG_SFT                                          0
#define O_48_CFG_MASK                                         0x3f
#define O_48_CFG_MASK_SFT                                     (0x3f << 0)

/* AFE_MEMIF_CONN */
#define VUL9_USE_TINY_SFT                                     12
#define VUL9_USE_TINY_MASK                                    0x1
#define VUL9_USE_TINY_MASK_SFT                                (0x1 << 12)
#define VUL8_USE_TINY_SFT                                     11
#define VUL8_USE_TINY_MASK                                    0x1
#define VUL8_USE_TINY_MASK_SFT                                (0x1 << 11)
#define VUL7_2_USE_TINY_SFT                                   10
#define VUL7_2_USE_TINY_MASK                                  0x1
#define VUL7_2_USE_TINY_MASK_SFT                              (0x1 << 10)
#define VUL7_USE_TINY_SFT                                     9
#define VUL7_USE_TINY_MASK                                    0x1
#define VUL7_USE_TINY_MASK_SFT                                (0x1 << 9)
#define VUL6_USE_TINY_SFT                                     8
#define VUL6_USE_TINY_MASK                                    0x1
#define VUL6_USE_TINY_MASK_SFT                                (0x1 << 8)
#define VUL5_USE_TINY_SFT                                     7
#define VUL5_USE_TINY_MASK                                    0x1
#define VUL5_USE_TINY_MASK_SFT                                (0x1 << 7)
#define VUL4_USE_TINY_SFT                                     6
#define VUL4_USE_TINY_MASK                                    0x1
#define VUL4_USE_TINY_MASK_SFT                                (0x1 << 6)
#define VUL3_USE_TINY_SFT                                     5
#define VUL3_USE_TINY_MASK                                    0x1
#define VUL3_USE_TINY_MASK_SFT                                (0x1 << 5)
#define AWB2_USE_TINY_SFT                                     4
#define AWB2_USE_TINY_MASK                                    0x1
#define AWB2_USE_TINY_MASK_SFT                                (0x1 << 4)
#define AWB_USE_TINY_SFT                                      3
#define AWB_USE_TINY_MASK                                     0x1
#define AWB_USE_TINY_MASK_SFT                                 (0x1 << 3)
#define VUL12_USE_TINY_SFT                                    2
#define VUL12_USE_TINY_MASK                                   0x1
#define VUL12_USE_TINY_MASK_SFT                               (0x1 << 2)
#define VUL2_USE_TINY_SFT                                     1
#define VUL2_USE_TINY_MASK                                    0x1
#define VUL2_USE_TINY_MASK_SFT                                (0x1 << 1)
#define VUL1_USE_TINY_SFT                                     0
#define VUL1_USE_TINY_MASK                                    0x1
#define VUL1_USE_TINY_MASK_SFT                                (0x1 << 0)

/* AFE_ASRC_2CH_CON0 */
#define CON0_CHSET_STR_CLR_SFT                                4
#define CON0_CHSET_STR_CLR_MASK                               0x1
#define CON0_CHSET_STR_CLR_MASK_SFT                           (0x1 << 4)
#define CON0_CHSET_ON_SFT                                     2
#define CON0_CHSET_ON_MASK                                    0x1
#define CON0_CHSET_ON_MASK_SFT                                (0x1 << 2)
#define CON0_COEFF_SRAM_CTRL_SFT                              1
#define CON0_COEFF_SRAM_CTRL_MASK                             0x1
#define CON0_COEFF_SRAM_CTRL_MASK_SFT                         (0x1 << 1)
#define CON0_ASM_ON_SFT                                       0
#define CON0_ASM_ON_MASK                                      0x1
#define CON0_ASM_ON_MASK_SFT                                  (0x1 << 0)

/* AFE_ASRC_2CH_CON5 */
#define FREQ_CALI_CYCLE_SFT                                   16
#define FREQ_CALI_CYCLE_MASK                                  0xffff
#define FREQ_CALI_CYCLE_MASK_SFT                              (0xffff << 16)
#define FREQ_CALI_AUTORST_EN_SFT                              14
#define FREQ_CALI_AUTORST_EN_MASK                             0x1
#define FREQ_CALI_AUTORST_EN_MASK_SFT                         (0x1 << 14)
#define FREQ_CALC_RUNNING_SFT                                 13
#define FREQ_CALC_RUNNING_MASK                                0x1
#define FREQ_CALC_RUNNING_MASK_SFT                            (0x1 << 13)
#define AUTO_TUNE_FREQ5_SFT                                   12
#define AUTO_TUNE_FREQ5_MASK                                  0x1
#define AUTO_TUNE_FREQ5_MASK_SFT                              (0x1 << 12)
#define COMP_FREQ_RES_EN_SFT                                  11
#define COMP_FREQ_RES_EN_MASK                                 0x1
#define COMP_FREQ_RES_EN_MASK_SFT                             (0x1 << 11)
#define FREQ_CALI_SEL_SFT                                     8
#define FREQ_CALI_SEL_MASK                                    0x3
#define FREQ_CALI_SEL_MASK_SFT                                (0x3 << 8)
#define FREQ_CALI_BP_DGL_SFT                                  7
#define FREQ_CALI_BP_DGL_MASK                                 0x1
#define FREQ_CALI_BP_DGL_MASK_SFT                             (0x1 << 7)
#define FREQ_CALI_MAX_GWIDTH_SFT                              4
#define FREQ_CALI_MAX_GWIDTH_MASK                             0x7
#define FREQ_CALI_MAX_GWIDTH_MASK_SFT                         (0x7 << 4)
#define AUTO_TUNE_FREQ4_SFT                                   3
#define AUTO_TUNE_FREQ4_MASK                                  0x1
#define AUTO_TUNE_FREQ4_MASK_SFT                              (0x1 << 3)
#define FREQ_CALI_AUTO_RESTART_SFT                            2
#define FREQ_CALI_AUTO_RESTART_MASK                           0x1
#define FREQ_CALI_AUTO_RESTART_MASK_SFT                       (0x1 << 2)
#define CALI_USE_FREQ_OUT_SFT                                 1
#define CALI_USE_FREQ_OUT_MASK                                0x1
#define CALI_USE_FREQ_OUT_MASK_SFT                            (0x1 << 1)
#define CALI_EN_SFT                                           0
#define CALI_EN_MASK                                          0x1
#define CALI_EN_MASK_SFT                                      (0x1 << 0)

/* AFE_ADDA_MTKAIFV4_TX_CFG0 */
#define MTKAIFV4_TXIF_EN_SEL_SFT                              12
#define MTKAIFV4_TXIF_EN_SEL_MASK                             0x1
#define MTKAIFV4_TXIF_EN_SEL_MASK_SFT                         (0x1 << 12)
#define MTKAIFV4_TXIF_PROTOCOL3_SFT                           11
#define MTKAIFV4_TXIF_PROTOCOL3_MASK                          0x1
#define MTKAIFV4_TXIF_PROTOCOL3_MASK_SFT                      (0x1 << 11)
#define MTKAIFV4_ADDA6_OUT_EN_SEL_SFT                         10
#define MTKAIFV4_ADDA6_OUT_EN_SEL_MASK                        0x1
#define MTKAIFV4_ADDA6_OUT_EN_SEL_MASK_SFT                    (0x1 << 10)
#define MTKAIFV4_ADDA_OUT_EN_SEL_SFT                          9
#define MTKAIFV4_ADDA_OUT_EN_SEL_MASK                         0x1
#define MTKAIFV4_ADDA_OUT_EN_SEL_MASK_SFT                     (0x1 << 9)
#define MTKAIFV4_TXIF_INPUT_MODE_SFT                          4
#define MTKAIFV4_TXIF_INPUT_MODE_MASK                         0x1f
#define MTKAIFV4_TXIF_INPUT_MODE_MASK_SFT                     (0x1f << 4)
#define MTKAIFV4_TXIF_FOUR_CHANNEL_SFT                        1
#define MTKAIFV4_TXIF_FOUR_CHANNEL_MASK                       0x1
#define MTKAIFV4_TXIF_FOUR_CHANNEL_MASK_SFT                   (0x1 << 1)
#define MTKAIFV4_TXIF_AFE_ON_SFT                              0
#define MTKAIFV4_TXIF_AFE_ON_MASK                             0x1
#define MTKAIFV4_TXIF_AFE_ON_MASK_SFT                         (0x1 << 0)

/* AFE_ADDA6_MTKAIFV4_TX_CFG0 */
#define ADDA6_MTKAIFV4_TXIF_EN_SEL_SFT                        12
#define ADDA6_MTKAIFV4_TXIF_EN_SEL_MASK                       0x1
#define ADDA6_MTKAIFV4_TXIF_EN_SEL_MASK_SFT                   (0x1 << 12)
#define ADDA6_MTKAIFV4_TXIF_INPUT_MODE_SFT                    4
#define ADDA6_MTKAIFV4_TXIF_INPUT_MODE_MASK                   0x1f
#define ADDA6_MTKAIFV4_TXIF_INPUT_MODE_MASK_SFT               (0x1f << 4)
#define ADDA6_MTKAIFV4_TXIF_FOUR_CHANNEL_SFT                  1
#define ADDA6_MTKAIFV4_TXIF_FOUR_CHANNEL_MASK                 0x1
#define ADDA6_MTKAIFV4_TXIF_FOUR_CHANNEL_MASK_SFT             (0x1 << 1)
#define ADDA6_MTKAIFV4_TXIF_AFE_ON_SFT                        0
#define ADDA6_MTKAIFV4_TXIF_AFE_ON_MASK                       0x1
#define ADDA6_MTKAIFV4_TXIF_AFE_ON_MASK_SFT                   (0x1 << 0)

/* AFE_ADDA_MTKAIFV4_RX_CFG0 */
#define MTKAIFV4_RXIF_CLKINV_SFT                              31
#define MTKAIFV4_RXIF_CLKINV_MASK                             0x1
#define MTKAIFV4_RXIF_CLKINV_MASK_SFT                         (0x1 << 31)
#define MTKAIFV4_RXIF_LOOPBACK_MODE_SFT                       28
#define MTKAIFV4_RXIF_LOOPBACK_MODE_MASK                      0x1
#define MTKAIFV4_RXIF_LOOPBACK_MODE_MASK_SFT                  (0x1 << 28)
#define MTKAIFV4_RXIF_EN_SEL_SFT                              12
#define MTKAIFV4_RXIF_EN_SEL_MASK                             0x1
#define MTKAIFV4_RXIF_EN_SEL_MASK_SFT                         (0x1 << 12)
#define MTKAIFV4_TXIF_PROTOCOL3_SFT                           11
#define MTKAIFV4_TXIF_PROTOCOL3_MASK                          0x1
#define MTKAIFV4_TXIF_PROTOCOL3_MASK_SFT                      (0x1 << 11)
#define MTKAIFV4_ADDA6_IN_EN_SEL_SFT                          10
#define MTKAIFV4_ADDA6_IN_EN_SEL_MASK                         0x1
#define MTKAIFV4_ADDA6_IN_EN_SEL_MASK_SFT                     (0x1 << 10)
#define MTKAIFV4_ADDA_IN_EN_SEL_SFT                           9
#define MTKAIFV4_ADDA_IN_EN_SEL_MASK                          0x1
#define MTKAIFV4_ADDA_IN_EN_SEL_MASK_SFT                      (0x1 << 9)
#define MTKAIFV4_RXIF_INPUT_MODE_SFT                          4
#define MTKAIFV4_RXIF_INPUT_MODE_MASK                         0x1f
#define MTKAIFV4_RXIF_INPUT_MODE_MASK_SFT                     (0x1f << 4)
#define MTKAIFV4_RXIF_FOUR_CHANNEL_SFT                        1
#define MTKAIFV4_RXIF_FOUR_CHANNEL_MASK                       0x1
#define MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT                   (0x1 << 1)
#define MTKAIFV4_RXIF_AFE_ON_SFT                              0
#define MTKAIFV4_RXIF_AFE_ON_MASK                             0x1
#define MTKAIFV4_RXIF_AFE_ON_MASK_SFT                         (0x1 << 0)

/* AFE_ADDA_MTKAIFV4_RX_CFG1 */
#define MTKAIFV4_RXIF_SYNC_CNT_TABLE_SFT                      17
#define MTKAIFV4_RXIF_SYNC_CNT_TABLE_MASK                     0xfff
#define MTKAIFV4_RXIF_SYNC_CNT_TABLE_MASK_SFT                 (0xfff << 17)
#define MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_SFT                   12
#define MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_MASK                  0x1f
#define MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_MASK_SFT              (0x1f << 12)
#define MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_SFT            8
#define MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_MASK           0xf
#define MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_MASK_SFT       (0xf << 8)
#define MTKAIFV4_RXIF_SYNC_CHECK_ROUND_SFT                    4
#define MTKAIFV4_RXIF_SYNC_CHECK_ROUND_MASK                   0xf
#define MTKAIFV4_RXIF_SYNC_CHECK_ROUND_MASK_SFT               (0xf << 4)
#define MTKAIFV4_RXIF_FIFO_RSP_SFT                            1
#define MTKAIFV4_RXIF_FIFO_RSP_MASK                           0x7
#define MTKAIFV4_RXIF_FIFO_RSP_MASK_SFT                       (0x7 << 1)
#define MTKAIFV4_RXIF_SELF_DEFINE_TABLE_SFT                   0
#define MTKAIFV4_RXIF_SELF_DEFINE_TABLE_MASK                  0x1
#define MTKAIFV4_RXIF_SELF_DEFINE_TABLE_MASK_SFT              (0x1 << 0)

/* AFE_ADDA6_MTKAIFV4_RX_CFG0 */
#define ADDA6_MTKAIFV4_RXIF_CLKINV_SFT                        31
#define ADDA6_MTKAIFV4_RXIF_CLKINV_MASK                       0x1
#define ADDA6_MTKAIFV4_RXIF_CLKINV_MASK_SFT                   (0x1 << 31)
#define ADDA6_MTKAIFV4_RXIF_LOOPBACK_MODE_SFT                 28
#define ADDA6_MTKAIFV4_RXIF_LOOPBACK_MODE_MASK                0x1
#define ADDA6_MTKAIFV4_RXIF_LOOPBACK_MODE_MASK_SFT            (0x1 << 28)
#define ADDA6_MTKAIFV4_RXIF_EN_SEL_SFT                        12
#define ADDA6_MTKAIFV4_RXIF_EN_SEL_MASK                       0x1
#define ADDA6_MTKAIFV4_RXIF_EN_SEL_MASK_SFT                   (0x1 << 12)
#define ADDA6_MTKAIFV4_RXIF_INPUT_MODE_SFT                    4
#define ADDA6_MTKAIFV4_RXIF_INPUT_MODE_MASK                   0x1f
#define ADDA6_MTKAIFV4_RXIF_INPUT_MODE_MASK_SFT               (0x1f << 4)
#define ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_SFT                  1
#define ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_MASK                 0x1
#define ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT             (0x1 << 1)
#define ADDA6_MTKAIFV4_RXIF_AFE_ON_SFT                        0
#define ADDA6_MTKAIFV4_RXIF_AFE_ON_MASK                       0x1
#define ADDA6_MTKAIFV4_RXIF_AFE_ON_MASK_SFT                   (0x1 << 0)

/* AFE_ADDA6_MTKAIFV4_RX_CFG1 */
#define ADDA6_MTKAIFV4_RXIF_SYNC_CNT_TABLE_SFT                17
#define ADDA6_MTKAIFV4_RXIF_SYNC_CNT_TABLE_MASK               0xfff
#define ADDA6_MTKAIFV4_RXIF_SYNC_CNT_TABLE_MASK_SFT           (0xfff << 17)
#define ADDA6_MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_SFT             12
#define ADDA6_MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_MASK            0x1f
#define ADDA6_MTKAIFV4_RXIF_SYNC_SEARCH_TABLE_MASK_SFT        (0x1f << 12)
#define ADDA6_MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_SFT      8
#define ADDA6_MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_MASK     0xf
#define ADDA6_MTKAIFV4_RXIF_INVALID_SYNC_CHECK_ROUND_MASK_SFT (0xf << 8)
#define ADDA6_MTKAIFV4_RXIF_SYNC_CHECK_ROUND_SFT              4
#define ADDA6_MTKAIFV4_RXIF_SYNC_CHECK_ROUND_MASK             0xf
#define ADDA6_MTKAIFV4_RXIF_SYNC_CHECK_ROUND_MASK_SFT         (0xf << 4)
#define ADDA6_MTKAIFV4_RXIF_FIFO_RSP_SFT                      1
#define ADDA6_MTKAIFV4_RXIF_FIFO_RSP_MASK                     0x7
#define ADDA6_MTKAIFV4_RXIF_FIFO_RSP_MASK_SFT                 (0x7 << 1)
#define ADDA6_MTKAIFV4_RXIF_SELF_DEFINE_TABLE_SFT             0
#define ADDA6_MTKAIFV4_RXIF_SELF_DEFINE_TABLE_MASK            0x1
#define ADDA6_MTKAIFV4_RXIF_SELF_DEFINE_TABLE_MASK_SFT        (0x1 << 0)

/* AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG */
#define ADDA6_MTKAIFV4_TXIF_SYNCWORD_SFT                      16
#define ADDA6_MTKAIFV4_TXIF_SYNCWORD_MASK                     0xffff
#define ADDA6_MTKAIFV4_TXIF_SYNCWORD_MASK_SFT                 (0xffff << 16)
#define ADDA_MTKAIFV4_TXIF_SYNCWORD_SFT                       0
#define ADDA_MTKAIFV4_TXIF_SYNCWORD_MASK                      0xffff
#define ADDA_MTKAIFV4_TXIF_SYNCWORD_MASK_SFT                  (0xffff << 0)

/* AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG */
#define ADDA6_MTKAIFV4_RXIF_SYNCWORD_SFT                      16
#define ADDA6_MTKAIFV4_RXIF_SYNCWORD_MASK                     0xffff
#define ADDA6_MTKAIFV4_RXIF_SYNCWORD_MASK_SFT                 (0xffff << 16)
#define ADDA_MTKAIFV4_RXIF_SYNCWORD_SFT                       0
#define ADDA_MTKAIFV4_RXIF_SYNCWORD_MASK                      0xffff
#define ADDA_MTKAIFV4_RXIF_SYNCWORD_MASK_SFT                  (0xffff << 0)

/* AFE_CM1_CON0_CFG */
#define AFE_CM_DEBUG_MODE                             31
#define AFE_CM_DEBUG_MODE_MASK                        0x1
#define AFE_CM_DEBUG_MODE_MASK_SFT                    (0x1 << 31)
#define AFE_CM1_VUL9_BYPASS_CM                        30
#define AFE_CM1_VUL9_BYPASS_CM_MASK                   0x1
#define AFE_CM1_VUL9_BYPASS_CM_MASK_SFT               (0x1 << 30)
#define AFE_CM1_VUL8_BYPASS_CM                        29
#define AFE_CM1_VUL8_BYPASS_CM_MASK                   0x1
#define AFE_CM1_VUL8_BYPASS_CM_MASK_SFT               (0x1 << 29)
#define AFE_CM_UPDATE_CNT                             16
#define AFE_CM_UPDATE_CNT_MASK                        0x1fff
#define AFE_CM_UPDATE_CNT_MASK_SFT                    (0x1fff << 16)
#define AFE_CM_1X_EN_SEL_FS                           8
#define AFE_CM_1X_EN_SEL_FS_MASK                      0xf
#define AFE_CM_1X_EN_SEL_FS_MASK_SFT                  (0xf << 8)
#define AFE_CM_CH_NUM                                 3
#define AFE_CM_CH_NUM_MASK                            0x1f
#define AFE_CM_CH_NUM_MASK_SFT                        (0x1f << 3)
#define AFE_CM_BYTE_SWAP                              1
#define AFE_CM_BYTE_SWAP_MASK                         0x1
#define AFE_CM_BYTE_SWAP_MASK_SFT                     (0x1 << 1)
#define AFE_CM_ON                                     0
#define AFE_CM_ON_MASK                                0x1
#define AFE_CM_ON_MASK_SFT                            (0x1 << 0)

/* AFE_CM2_CON0_CFG */
#define AFE_CM2_AWB2_TINY_BYPASS_CM                    30
#define AFE_CM2_AWB2_TINY_BYPASS_CM_MASK               0x1
#define AFE_CM2_AWB2_TINY_BYPASS_CM_MASK_SFT           (0x1 << 30)

/* ETDM_IN0_CON0/ETDM_IN1_CON0 */
#define REG_ETDM_IN_EN_SFT                             0
#define REG_ETDM_IN_EN_MASK                            0x1
#define REG_ETDM_IN_EN_MASK_SFT                        (0x1 << 0)
#define REG_SYNC_MODE_SFT                              1
#define REG_SYNC_MODE_MASK                             0x1
#define REG_SYNC_MODE_MASK_SFT                         (0x1 << 1)
#define REG_LSB_FIRST_SFT                              3
#define REG_LSB_FIRST_MASK                             0x1
#define REG_LSB_FIRST_MASK_SFT                         (0x1 << 3)
#define REG_SOFT_RST_SFT                               4
#define REG_SOFT_RST_MASK                              0x1
#define REG_SOFT_RST_MASK_SFT                          (0x1 << 4)
#define REG_SLAVE_MODE_SFT                             5
#define REG_SLAVE_MODE_MASK                            0x1
#define REG_SLAVE_MODE_MASK_SFT                        (0x1 << 5)
#define REG_FMT_SFT                                    6
#define REG_FMT_MASK                                   0x7
#define REG_FMT_MASK_SFT                               (0x7 << 6)
#define REG_LRCK_EDGE_SEL_SFT                          10
#define REG_LRCK_EDGE_SEL_MASK                         0x1
#define REG_LRCK_EDGE_SEL_MASK_SFT                     (0x1 << 10)
#define REG_BIT_LENGTH_SFT                             11
#define REG_BIT_LENGTH_MASK                            0x1f
#define REG_BIT_LENGTH_MASK_SFT                        (0x1f << 11)
#define REG_WORD_LENGTH_SFT                            16
#define REG_WORD_LENGTH_MASK                           0x1f
#define REG_WORD_LENGTH_MASK_SFT                       (0x1f << 16)
#define REG_CH_NUM_SFT                                 23
#define REG_CH_NUM_MASK                                0x1f
#define REG_CH_NUM_MASK_SFT                            (0x1f << 23)
#define REG_RELATCH_1X_EN_SEL_DOMAIN_SFT               28
#define REG_RELATCH_1X_EN_SEL_DOMAIN_MASK              0x7
#define REG_RELATCH_1X_EN_SEL_DOMAIN_MASK_SFT          (0x7 << 28)
#define REG_VALID_TOGETHER_SFT                         31
#define REG_VALID_TOGETHER_MASK                        0x1
#define REG_VALID_TOGETHER_MASK_SFT                    (0x1 << 31)

/* ETDM_IN0_CON1/ETDM_IN1_CON1 */
#define REG_INITIAL_COUNT_SFT                      0
#define REG_INITIAL_COUNT_MASK                     0x1f
#define REG_INITIAL_COUNT_MASK_SFT                 (0x1f << 0)
#define REG_INITIAL_POINT_SFT                      5
#define REG_INITIAL_POINT_MASK                     0x1f
#define REG_INITIAL_POINT_MASK_SFT                 (0x1f << 5)
#define REG_LRCK_AUTO_OFF_SFT                      10
#define REG_LRCK_AUTO_OFF_MASK                     0x1
#define REG_LRCK_AUTO_OFF_MASK_SFT                 (0x1 << 10)
#define REG_BCK_AUTO_OFF_SFT                       11
#define REG_BCK_AUTO_OFF_MASK                      0x1
#define REG_BCK_AUTO_OFF_MASK_SFT                  (0x1 << 11)
#define REG_INITIAL_LRCK_SFT                       13
#define REG_INITIAL_LRCK_MASK                      0x1
#define REG_INITIAL_LRCK_MASK_SFT                  (0x1 << 13)
#define REG_LRCK_RESET_SFT                         15
#define REG_LRCK_RESET_MASK                        0x1
#define REG_LRCK_RESET_MASK_SFT                    (0x1 << 15)
#define PINMUX_MCLK_CTRL_OE_SFT                    16
#define PINMUX_MCLK_CTRL_OE_MASK                   0x1
#define PINMUX_MCLK_CTRL_OE_MASK_SFT               (0x1 << 16)
#define REG_OUTPUT_CR_EN_SFT                       18
#define REG_OUTPUT_CR_EN_MASK                      0x1
#define REG_OUTPUT_CR_EN_MASK_SFT                  (0x1 << 18)
#define REG_LR_ALIGN_SFT                           19
#define REG_LR_ALIGN_MASK                          0x1
#define REG_LR_ALIGN_MASK_SFT                      (0x1 << 19)
#define REG_LRCK_WIDTH_SFT                         20
#define REG_LRCK_WIDTH_MASK                        0x3ff
#define REG_LRCK_WIDTH_MASK_SFT                    (0x3ff << 20)
#define REG_DIRECT_INPUT_MASTER_BCK_SFT            30
#define REG_DIRECT_INPUT_MASTER_BCK_MASK           0x1
#define REG_DIRECT_INPUT_MASTER_BCK_MASK_SFT       (0x1 << 30)
#define REG_LRCK_AUTO_MODE_SFT                     31
#define REG_LRCK_AUTO_MODE_MASK                    0x1
#define REG_LRCK_AUTO_MODE_MASK_SFT                (0x1 << 31)

/* ETDM_IN0_CON2/ETDM_IN1_CON2 */
#define REG_UPDATE_POINT_SFT                       0
#define REG_UPDATE_POINT_MASK                      0x1f
#define REG_UPDATE_POINT_MASK_SFT                  (0x1f << 0)
#define REG_UPDATE_GAP_SFT                         5
#define REG_UPDATE_GAP_MASK                        0x1f
#define REG_UPDATE_GAP_MASK_SFT                    (0x1f << 5)
#define REG_CLOCK_SOURCE_SEL_SFT                   10
#define REG_CLOCK_SOURCE_SEL_MASK                  0x7
#define REG_CLOCK_SOURCE_SEL_MASK_SFT              (0x7 << 10)
#define REG_AGENT_USE_ETDM_BCK_SFT                 13
#define REG_AGENT_USE_ETDM_BCK_MASK                0x1
#define REG_AGENT_USE_ETDM_BCK_MASK_SFT            (0x1 << 13)
#define REG_CK_EN_SEL_AUTO_SFT                     14
#define REG_CK_EN_SEL_AUTO_MASK                    0x1
#define REG_CK_EN_SEL_AUTO_MASK_SFT                (0x1 << 14)
#define REG_MULTI_IP_ONE_DATA_CH_NUM_SFT           15
#define REG_MULTI_IP_ONE_DATA_CH_NUM_MASK          0x1f
#define REG_MULTI_IP_ONE_DATA_CH_NUM_MASK_SFT      (0x1f << 15)
#define REG_MASK_AUTO_SFT                          20
#define REG_MASK_AUTO_MASK                         0x1
#define REG_MASK_AUTO_MASK_SFT                     (0x1 << 20)
#define REG_MASK_NUM_SFT                           21
#define REG_MASK_NUM_MASK                          0x1f
#define REG_MASK_NUM_MASK_SFT                      (0x1f << 21)
#define REG_UPDATE_POINT_AUTO_SFT                  26
#define REG_UPDATE_POINT_AUTO_MASK                 0x1
#define REG_UPDATE_POINT_AUTO_MASK_SFT             (0x1 << 26)
#define REG_MULTI_IP_MODE_SFT                      31
#define REG_MULTI_IP_MODE_MASK                     0x1
#define REG_MULTI_IP_MODE_MASK_SFT                 (0x1 << 31)

/* ETDM_IN0_CON3/ETDM_IN1_CON3 */
#define REG_DISABLE_OUT_0_SFT                      0
#define REG_DISABLE_OUT_0_MASK                     0x1
#define REG_DISABLE_OUT_0_MASK_SFT                 (0x1 << 0)
#define REG_DISABLE_OUT_1_SFT                      1
#define REG_DISABLE_OUT_1_MASK                     0x1
#define REG_DISABLE_OUT_1_MASK_SFT                 (0x1 << 1)
#define REG_DISABLE_OUT_2_SFT                      2
#define REG_DISABLE_OUT_2_MASK                     0x1
#define REG_DISABLE_OUT_2_MASK_SFT                 (0x1 << 2)
#define REG_DISABLE_OUT_3_SFT                      3
#define REG_DISABLE_OUT_3_MASK                     0x1
#define REG_DISABLE_OUT_3_MASK_SFT                 (0x1 << 3)
#define REG_DISABLE_OUT_4_SFT                      4
#define REG_DISABLE_OUT_4_MASK                     0x1
#define REG_DISABLE_OUT_4_MASK_SFT                 (0x1 << 4)
#define REG_DISABLE_OUT_5_SFT                      5
#define REG_DISABLE_OUT_5_MASK                     0x1
#define REG_DISABLE_OUT_5_MASK_SFT                 (0x1 << 5)
#define REG_DISABLE_OUT_6_SFT                      6
#define REG_DISABLE_OUT_6_MASK                     0x1
#define REG_DISABLE_OUT_6_MASK_SFT                 (0x1 << 6)
#define REG_DISABLE_OUT_7_SFT                      7
#define REG_DISABLE_OUT_7_MASK                     0x1
#define REG_DISABLE_OUT_7_MASK_SFT                 (0x1 << 7)
#define REG_DISABLE_OUT_8_SFT                      8
#define REG_DISABLE_OUT_8_MASK                     0x1
#define REG_DISABLE_OUT_8_MASK_SFT                 (0x1 << 8)
#define REG_DISABLE_OUT_9_SFT                      9
#define REG_DISABLE_OUT_9_MASK                     0x1
#define REG_DISABLE_OUT_9_MASK_SFT                 (0x1 << 9)
#define REG_DISABLE_OUT_10_SFT                     10
#define REG_DISABLE_OUT_10_MASK                    0x1
#define REG_DISABLE_OUT_10_MASK_SFT                (0x1 << 10)
#define REG_DISABLE_OUT_11_SFT                     11
#define REG_DISABLE_OUT_11_MASK                    0x1
#define REG_DISABLE_OUT_11_MASK_SFT                (0x1 << 11)
#define REG_DISABLE_OUT_12_SFT                     12
#define REG_DISABLE_OUT_12_MASK                    0x1
#define REG_DISABLE_OUT_12_MASK_SFT                (0x1 << 12)
#define REG_DISABLE_OUT_13_SFT                     13
#define REG_DISABLE_OUT_13_MASK                    0x1
#define REG_DISABLE_OUT_13_MASK_SFT                (0x1 << 13)
#define REG_DISABLE_OUT_14_SFT                     14
#define REG_DISABLE_OUT_14_MASK                    0x1
#define REG_DISABLE_OUT_14_MASK_SFT                (0x1 << 14)
#define REG_DISABLE_OUT_15_SFT                     15
#define REG_DISABLE_OUT_15_MASK                    0x1
#define REG_DISABLE_OUT_15_MASK_SFT                (0x1 << 15)
#define REG_RJ_DATA_RIGHT_ALIGN_SFT                16
#define REG_RJ_DATA_RIGHT_ALIGN_MASK               0x1
#define REG_RJ_DATA_RIGHT_ALIGN_MASK_SFT           (0x1 << 16)
#define REG_MONITOR_SEL_SFT                        17
#define REG_MONITOR_SEL_MASK                       0x3
#define REG_MONITOR_SEL_MASK_SFT                   (0x3 << 17)
#define REG_CNT_UPPER_LIMIT_SFT                    19
#define REG_CNT_UPPER_LIMIT_MASK                   0x3f
#define REG_CNT_UPPER_LIMIT_MASK_SFT               (0x3f << 19)
#define REG_COMPACT_SAMPLE_END_DIS_SFT             25
#define REG_COMPACT_SAMPLE_END_DIS_MASK            0x1
#define REG_COMPACT_SAMPLE_END_DIS_MASK_SFT        (0x1 << 25)
#define REG_FS_TIMING_SEL_SFT                      26
#define REG_FS_TIMING_SEL_MASK                     0x1f
#define REG_FS_TIMING_SEL_MASK_SFT                 (0x1f << 26)
#define REG_SAMPLE_END_MODE_SFT                    31
#define REG_SAMPLE_END_MODE_MASK                   0x1
#define REG_SAMPLE_END_MODE_MASK_SFT               (0x1 << 31)

/* ETDM_IN0_CON4/ETDM_IN1_CON4 */
#define REG_DSD_MODE_SFT                           0
#define REG_DSD_MODE_MASK                          0x3f
#define REG_DSD_MODE_MASK_SFT                      (0x3f << 0)
#define REG_DSD_REPACK_AUTO_MODE_SFT               8
#define REG_DSD_REPACK_AUTO_MODE_MASK              0x1
#define REG_DSD_REPACK_AUTO_MODE_MASK_SFT          (0x1 << 8)
#define REG_REPACK_WORD_LENGTH_SFT                 9
#define REG_REPACK_WORD_LENGTH_MASK                0x3
#define REG_REPACK_WORD_LENGTH_MASK_SFT            (0x3 << 9)
#define REG_ASYNC_RESET_SFT                        11
#define REG_ASYNC_RESET_MASK                       0x1
#define REG_ASYNC_RESET_MASK_SFT                   (0x1 << 11)
#define REG_DSD_CHNUM_SFT                          12
#define REG_DSD_CHNUM_MASK                         0xf
#define REG_DSD_CHNUM_MASK_SFT                     (0xf << 12)
#define REG_SLAVE_BCK_INV_SFT                      16
#define REG_SLAVE_BCK_INV_MASK                     0x1
#define REG_SLAVE_BCK_INV_MASK_SFT                 (0x1 << 16)
#define REG_SLAVE_LRCK_INV_SFT                     17
#define REG_SLAVE_LRCK_INV_MASK                    0x1
#define REG_SLAVE_LRCK_INV_MASK_SFT                (0x1 << 17)
#define REG_MASTER_BCK_INV_SFT                     18
#define REG_MASTER_BCK_INV_MASK                    0x1
#define REG_MASTER_BCK_INV_MASK_SFT                (0x1 << 18)
#define REG_MASTER_LRCK_INV_SFT                    19
#define REG_MASTER_LRCK_INV_MASK                   0x1
#define REG_MASTER_LRCK_INV_MASK_SFT               (0x1 << 19)
#define REG_RELATCH_1X_EN_SEL_SFT                  20
#define REG_RELATCH_1X_EN_SEL_MASK                 0x1f
#define REG_RELATCH_1X_EN_SEL_MASK_SFT             (0x1f << 20)
#define REG_SAMPLE_END_POINT_SFT                   25
#define REG_SAMPLE_END_POINT_MASK                  0x1f
#define REG_SAMPLE_END_POINT_MASK_SFT              (0x1f << 25)
#define REG_WAIT_LAST_SAMPLE_SFT                   30
#define REG_WAIT_LAST_SAMPLE_MASK                  0x1
#define REG_WAIT_LAST_SAMPLE_MASK_SFT              (0x1 << 30)
#define REG_MASTER_BCK_FORCE_ON_SFT                31
#define REG_MASTER_BCK_FORCE_ON_MASK               0x1
#define REG_MASTER_BCK_FORCE_ON_MASK_SFT           (0x1 << 31)

/* ETDM_IN0_CON5/ETDM_IN1_CON5 */
/* ETDM_IN0_CON6/ETDM_IN1_CON6 */
/* ETDM_IN0_CON7/ETDM_IN1_CON7 */
/* ETDM_IN0_CON8/ETDM_IN1_CON8 */
#define REG_AFIFO_THRESHOLD_SFT                     29
#define REG_AFIFO_THRESHOLD_MASK                    0x3
#define REG_AFIFO_THRESHOLD_MASK_SFT                (0x1 << 29)
#define REG_CK_EN_SEL_MANUAL_SFT                    16
#define REG_CK_EN_SEL_MANUAL_MASK                   0x3ff
#define REG_CK_EN_SEL_MANUAL_MASK_SFT               (0x3ff << 16)
#define REG_AFIFO_SW_RESET_SFT                      15
#define REG_AFIFO_SW_RESET_MASK                     0x1
#define REG_AFIFO_SW_RESET_MASK_SFT                 (0x1 << 15)
#define REG_AFIFO_RESET_SEL_SFT                     14
#define REG_AFIFO_RESET_SEL_MASK                    0x1
#define REG_AFIFO_RESET_SEL_MASK_SFT                (0x1 << 14)
#define REG_AFIFO_AUTO_RESET_DIS_SFT                9
#define REG_AFIFO_AUTO_RESET_DIS_MASK               0x1
#define REG_AFIFO_AUTO_RESET_DIS_MASK_SFT           (0x1 << 9)
#define REG_ETDM_USE_AFIFO_SFT                      8
#define REG_ETDM_USE_AFIFO_MASK                     0x1
#define REG_ETDM_USE_AFIFO_MASK_SFT                 (0x1 << 8)
#define REG_AFIFO_CLOCK_DOMAIN_SEL_SFT              5
#define REG_AFIFO_CLOCK_DOMAIN_SEL_MASK             0x7
#define REG_AFIFO_CLOCK_DOMAIN_SEL_MASK_SFT         (0x7 << 5)
#define REG_AFIFO_MODE_SFT                          0
#define REG_AFIFO_MODE_MASK                         0x1f
#define REG_AFIFO_MODE_MASK_SFT                     (0x1f << 0)

/* ETDM_IN0_CON9/ETDM_IN1_CON9 */
#define REG_OUT2_LATCH_TIME_SFT                     10
#define REG_OUT2_LATCH_TIME_MASK                    0x1f
#define REG_OUT2_LATCH_TIME_MASK_SFT                (0x1f << 10)
#define REG_ALMOST_END_BIT_COUNT_SFT                5
#define REG_ALMOST_END_BIT_COUNT_MASK               0x1f
#define REG_ALMOST_END_BIT_COUNT_MASK_SFT           (0x1f << 5)
#define REG_ALMOST_END_CH_COUNT_SFT                 0
#define REG_ALMOST_END_CH_COUNT_MASK                0x1f
#define REG_ALMOST_END_CH_COUNT_MASK_SFT            (0x1f << 0)

/* ETDM_OUT0_CON0/ETDM_OUT1_CON0 */
#define REG_ETDM_OUT_EN_SFT                         0
#define REG_ETDM_OUT_EN_MASK                        0x1
#define REG_ETDM_OUT_EN_MASK_SFT                    (0x1 << 0)
#define REG_SYNC_MODE_SFT                           1
#define REG_SYNC_MODE_MASK                          0x1
#define REG_SYNC_MODE_MASK_SFT                      (0x1 << 1)
#define REG_LSB_FIRST_SFT                           3
#define REG_LSB_FIRST_MASK                          0x1
#define REG_LSB_FIRST_MASK_SFT                      (0x1 << 3)
#define REG_SOFT_RST_SFT                            4
#define REG_SOFT_RST_MASK                           0x1
#define REG_SOFT_RST_MASK_SFT                       (0x1 << 4)
#define REG_SLAVE_MODE_SFT                          5
#define REG_SLAVE_MODE_MASK                         0x1
#define REG_SLAVE_MODE_MASK_SFT                     (0x1 << 5)
#define REG_FMT_SFT                                 6
#define REG_FMT_MASK                                0x7
#define REG_FMT_MASK_SFT                            (0x7 << 6)
#define REG_USE_TINYCONN_32BIT_SFT                  9
#define REG_USE_TINYCONN_32BIT_MASK                 0x1
#define REG_USE_TINYCONN_32BIT_MASK_SFT             (0x1 << 9)
#define REG_LRCK_EDGE_SEL_SFT                       10
#define REG_LRCK_EDGE_SEL_MASK                      0x1
#define REG_LRCK_EDGE_SEL_MASK_SFT                  (0x1 << 10)
#define REG_BIT_LENGTH_SFT                          11
#define REG_BIT_LENGTH_MASK                         0x1f
#define REG_BIT_LENGTH_MASK_SFT                     (0x1f << 11)
#define REG_WORD_LENGTH_SFT                         16
#define REG_WORD_LENGTH_MASK                        0x1f
#define REG_WORD_LENGTH_MASK_SFT                    (0x1f << 16)
#define REG_CH_NUM_SFT                              23
#define REG_CH_NUM_MASK                             0x1f
#define REG_CH_NUM_MASK_SFT                         (0x1f << 23)
#define REG_RELATCH_1X_EN_SEL_DOMAIN_SFT            28
#define REG_RELATCH_1X_EN_SEL_DOMAIN_MASK           0x7
#define REG_RELATCH_1X_EN_SEL_DOMAIN_MASK_SFT       (0x7 << 28)
#define REG_VALID_TOGETHER_SFT                      31
#define REG_VALID_TOGETHER_MASK                     0x1
#define REG_VALID_TOGETHER_MASK_SFT                 (0x1 << 31)

/* ETDM_OUT0_CON1/ETDM_OUT1_CON1 */
#define REG_INITIAL_COUNT_SFT                     0
#define REG_INITIAL_COUNT_MASK                    0x1f
#define REG_INITIAL_COUNT_MASK_SFT                (0x1f << 0)
#define REG_INITIAL_POINT_SFT                     5
#define REG_INITIAL_POINT_MASK                    0x1f
#define REG_INITIAL_POINT_MASK_SFT                (0x1f << 5)
#define REG_LRCK_AUTO_OFF_SFT                     10
#define REG_LRCK_AUTO_OFF_MASK                    0x1
#define REG_LRCK_AUTO_OFF_MASK_SFT                (0x1 << 10)
#define REG_BCK_AUTO_OFF_SFT                      11
#define REG_BCK_AUTO_OFF_MASK                     0x1
#define REG_BCK_AUTO_OFF_MASK_SFT                 (0x1 << 11)
#define REG_INITIAL_LRCK_SFT                      13
#define REG_INITIAL_LRCK_MASK                     0x1
#define REG_INITIAL_LRCK_MASK_SFT                 (0x1 << 13)
#define REG_LRCK_RESET_SFT                        15
#define REG_LRCK_RESET_MASK                       0x1
#define REG_LRCK_RESET_MASK_SFT                   (0x1 << 15)
#define PINMUX_MCLK_CTRL_OE_SFT                   16
#define PINMUX_MCLK_CTRL_OE_MASK                  0x1
#define PINMUX_MCLK_CTRL_OE_MASK_SFT              (0x1 << 16)
#define PINMUX_BCK_LRCK_CTRL_OE_SFT               17
#define PINMUX_BCK_LRCK_CTRL_OE_MASK              0x1
#define PINMUX_BCK_LRCK_CTRL_OE_MASK_SFT          (0x1 << 17)
#define REG_OUTPUT_CR_EN_SFT                      18
#define REG_OUTPUT_CR_EN_MASK                     0x1
#define REG_OUTPUT_CR_EN_MASK_SFT                 (0x1 << 18)
#define OUT_REG_LRCK_WIDTH_SFT                    19
#define OUT_REG_LRCK_WIDTH_MASK                   0x3ff
#define OUT_REG_LRCK_WIDTH_MASK_SFT               (0x3ff << 19)
#define OUT_REG_LRCK_AUTO_MODE_SFT                29
#define OUT_REG_LRCK_AUTO_MODE_MASK               0x1
#define OUT_REG_LRCK_AUTO_MODE_MASK_SFT           (0x1 << 29)
#define REG_DIRECT_INPUT_MASTER_BCK_SFT           30
#define REG_DIRECT_INPUT_MASTER_BCK_MASK          0x1
#define REG_DIRECT_INPUT_MASTER_BCK_MASK_SFT      (0x1 << 30)
#define REG_16B_COMPACT_MODE_SFT                  31
#define REG_16B_COMPACT_MODE_MASK                 0x1
#define REG_16B_COMPACT_MODE_MASK_SFT             (0x1 << 31)

/* ETDM_OUT0_CON2/ETDM_OUT1_CON2 */
/* ETDM_OUT0_CON3/ETDM_OUT1_CON3 */
/* ETDM_OUT0_CON4/ETDM_OUT1_CON4 */
#define OUT_REG_FS_TIMING_SEL_SFT                 0
#define OUT_REG_FS_TIMING_SEL_MASK                0x1f
#define OUT_REG_FS_TIMING_SEL_MASK_SFT            (0x1f << 0)
#define OUT_REG_CLOCK_SOURCE_SEL_SFT              6
#define OUT_REG_CLOCK_SOURCE_SEL_MASK             0x7
#define OUT_REG_CLOCK_SOURCE_SEL_MASK_SFT         (0x7 << 6)
#define OUT_REG_CK_EN_SEL_AUTO_SFT                10
#define OUT_REG_CK_EN_SEL_AUTO_MASK               0x1
#define OUT_REG_CK_EN_SEL_AUTO_MASK_SFT           (0x1 << 10)
#define REG_ASYNC_RESET_SFT                       11
#define REG_ASYNC_RESET_MASK                      0x1
#define REG_ASYNC_RESET_MASK_SFT                  (0x1 << 11)
#define INTERCONN_OUT_EN_SEL_SFT                  24
#define INTERCONN_OUT_EN_SEL_MASK                 0x1f
#define INTERCONN_OUT_EN_SEL_MASK_SFT             (0x1f << 24)
#define REG_WAIT_LAST_SAMPLE_SFT                  30
#define REG_WAIT_LAST_SAMPLE_MASK                 0x1
#define REG_WAIT_LAST_SAMPLE_MASK_SFT             (0x1 << 30)
#define REG_MASTER_BCK_FORCE_ON_SFT               31
#define REG_MASTER_BCK_FORCE_ON_MASK              0x1
#define REG_MASTER_BCK_FORCE_ON_MASK_SFT          (0x1 << 31)

/* ETDM_OUT0_CON5/ETDM_OUT1_CON5 */
/* ETDM_OUT0_CON6/ETDM_OUT1_CON6 */
/* ETDM_OUT0_CON7/ETDM_OUT1_CON7 */
/* ETDM_OUT0_CON8/ETDM_OUT1_CON8 */

/* ETDM_0_3_COWORK_CON0 */
#define ETDM_OUT0_DATA_SEL_SFT                         0
#define ETDM_OUT0_DATA_SEL_MASK                        0xf
#define ETDM_OUT0_DATA_SEL_MASK_SFT                    (0xf << 0)
#define ETDM_OUT0_SYNC_SEL_SFT                         4
#define ETDM_OUT0_SYNC_SEL_MASK                        0xf
#define ETDM_OUT0_SYNC_SEL_MASK_SFT                    (0xf << 4)
#define ETDM_OUT0_SLAVE_SEL_SFT                        8
#define ETDM_OUT0_SLAVE_SEL_MASK                       0xf
#define ETDM_OUT0_SLAVE_SEL_MASK_SFT                   (0xf << 8)
#define ETDM_OUT1_DATA_SEL_SFT                         12
#define ETDM_OUT1_DATA_SEL_MASK                        0xf
#define ETDM_OUT1_DATA_SEL_MASK_SFT                    (0xf << 12)
#define ETDM_OUT1_SYNC_SEL_SFT                         16
#define ETDM_OUT1_SYNC_SEL_MASK                        0xf
#define ETDM_OUT1_SYNC_SEL_MASK_SFT                    (0xf << 16)
#define ETDM_OUT1_SLAVE_SEL_SFT                        20
#define ETDM_OUT1_SLAVE_SEL_MASK                       0xf
#define ETDM_OUT1_SLAVE_SEL_MASK_SFT                   (0xf << 20)
#define ETDM_OUT0_PCM0_MUX_SEL_SFT                     21
#define ETDM_OUT0_PCM0_MUX_SEL_MASK                    0x1
#define ETDM_OUT0_PCM0_MUX_SEL_MASK_SFT                (0x1 << 21)
#define ETDM_IN0_SLAVE_SEL_SFT                         24
#define ETDM_IN0_SLAVE_SEL_MASK                        0xf
#define ETDM_IN0_SLAVE_SEL_MASK_SFT                    (0xf << 24)
#define ETDM_IN0_SYNC_SEL_SFT                          28
#define ETDM_IN0_SYNC_SEL_MASK                         0xf
#define ETDM_IN0_SYNC_SEL_MASK_SFT                     (0xf << 28)

/* ETDM_0_3_COWORK_CON1 */
#define ETDM_IN0_SDATA0_SEL_SFT                        0
#define ETDM_IN0_SDATA0_SEL_MASK                       0xf
#define ETDM_IN0_SDATA0_SEL_MASK_SFT                   (0xf << 0)
#define ETDM_IN0_SDATA1_15_SEL_SFT                     4
#define ETDM_IN0_SDATA1_15_SEL_MASK                    0xf
#define ETDM_IN0_SDATA1_15_SEL_MASK_SFT                (0xf << 4)
#define ETDM_IN1_SLAVE_SEL_SFT                         8
#define ETDM_IN1_SLAVE_SEL_MASK                        0xf
#define ETDM_IN1_SLAVE_SEL_MASK_SFT                    (0xf << 8)
#define ETDM_IN1_SYNC_SEL_SFT                          12
#define ETDM_IN1_SYNC_SEL_MASK                         0xf
#define ETDM_IN1_SYNC_SEL_MASK_SFT                     (0xf << 12)
#define ETDM_IN1_SDATA0_SEL_SFT                        16
#define ETDM_IN1_SDATA0_SEL_MASK                       0xf
#define ETDM_IN1_SDATA0_SEL_MASK_SFT                   (0xf << 16)
#define ETDM_IN1_SDATA1_15_SEL_SFT                     20
#define ETDM_IN1_SDATA1_15_SEL_MASK                    0xf
#define ETDM_IN1_SDATA1_15_SEL_MASK_SFT                (0xf << 20)
#define ETDM_IN0_INTERCONN_MUX_SEL_SFT                 24
#define ETDM_IN0_INTERCONN_MUX_SEL_MASK                0x1
#define ETDM_IN0_INTERCONN_MUX_SEL_MASK_SFT            (0x1 << 24)
#define ETDM_OUT0_INTERCONN_MUX_SEL_SFT                25
#define ETDM_OUT0_INTERCONN_MUX_SEL_MASK               0x1
#define ETDM_OUT0_INTERCONN_MUX_SEL_MASK_SFT           (0x1 << 25)

/* ETDM_0_3_COWORK_CON3 */
#define ETDM_OUT1_USE_SGEN_SFT                         29
#define ETDM_OUT1_USE_SGEN_MASK                        0x1
#define ETDM_OUT1_USE_SGEN_MASK_SFT                    (0x1 << 29)
#define ETDM_OUT0_USE_SGEN_SFT                         28
#define ETDM_OUT0_USE_SGEN_MASK                        0x1
#define ETDM_OUT0_USE_SGEN_MASK_SFT                    (0x1 << 28)

#define AUDIO_TOP_CON0                         0x0000
#define AUDIO_TOP_CON1                         0x0004
#define AUDIO_TOP_CON2                         0x0008
#define AUDIO_TOP_CON3                         0x000c
#define AFE_DAC_CON0                           0x0010
#define AFE_I2S_CON                            0x0018
#define AFE_DAIBT_CON0                         0x001c
#define AFE_CONN0                              0x0020
#define AFE_CONN1                              0x0024
#define AFE_CONN2                              0x0028
#define AFE_CONN3                              0x002c
#define AFE_CONN4                              0x0030
#define AFE_I2S_CON1                           0x0034
#define AFE_I2S_CON2                           0x0038
#define AFE_MRGIF_CON                          0x003c
#define AFE_I2S_CON3                           0x0040
#define AFE_CONN5                              0x0044
#define AFE_CONN_24BIT                         0x0048
#define AFE_DL1_CON0                           0x004c
#define AFE_DL1_BASE_MSB                       0x0050
#define AFE_DL1_BASE                           0x0054
#define AFE_DL1_CUR_MSB                        0x0058
#define AFE_DL1_CUR                            0x005c
#define AFE_DL1_END_MSB                        0x0060
#define AFE_DL1_END                            0x0064
#define AFE_DL2_CON0                           0x0068
#define AFE_DL2_BASE_MSB                       0x006c
#define AFE_DL2_BASE                           0x0070
#define AFE_DL2_CUR_MSB                        0x0074
#define AFE_DL2_CUR                            0x0078
#define AFE_DL2_END_MSB                        0x007c
#define AFE_DL2_END                            0x0080
#define AFE_DL3_CON0                           0x0084
#define AFE_DL3_BASE_MSB                       0x0088
#define AFE_DL3_BASE                           0x008c
#define AFE_DL3_CUR_MSB                        0x0090
#define AFE_DL3_CUR                            0x0094
#define AFE_DL3_END_MSB                        0x0098
#define AFE_DL3_END                            0x009c
#define AFE_CONN6                              0x00bc
#define AFE_DL4_CON0                           0x00cc
#define AFE_DL4_BASE_MSB                       0x00d0
#define AFE_DL4_BASE                           0x00d4
#define AFE_DL4_CUR_MSB                        0x00d8
#define AFE_DL4_CUR                            0x00dc
#define AFE_DL4_END_MSB                        0x00e0
#define AFE_DL4_END                            0x00e4
#define AFE_DL12_CON0                          0x00e8
#define AFE_DL12_BASE_MSB                      0x00ec
#define AFE_DL12_BASE                          0x00f0
#define AFE_DL12_CUR_MSB                       0x00f4
#define AFE_DL12_CUR                           0x00f8
#define AFE_DL12_END_MSB                       0x00fc
#define AFE_DL12_END                           0x0100
#define AFE_ADDA_DL_SRC2_CON0                  0x0108
#define AFE_ADDA_DL_SRC2_CON1                  0x010c
#define AFE_ADDA_UL_SRC_CON0                   0x0114
#define AFE_ADDA_UL_SRC_CON1                   0x0118
#define AFE_ADDA_TOP_CON0                      0x0120
#define AFE_ADDA_UL_DL_CON0                    0x0124
#define AFE_ADDA_SRC_DEBUG                     0x012c
#define AFE_ADDA_SRC_DEBUG_MON0                0x0130
#define AFE_ADDA_SRC_DEBUG_MON1                0x0134
#define AFE_ADDA_UL_SRC_MON0                   0x0148
#define AFE_ADDA_UL_SRC_MON1                   0x014c
#define AFE_SECURE_CON0                        0x0150
#define AFE_SRAM_BOUND                         0x0154
#define AFE_SECURE_CON1                        0x0158
#define AFE_SECURE_CONN0                       0x015c
#define AFE_VUL_CON0                           0x0170
#define AFE_VUL_BASE_MSB                       0x0174
#define AFE_VUL_BASE                           0x0178
#define AFE_VUL_CUR_MSB                        0x017c
#define AFE_VUL_CUR                            0x0180
#define AFE_VUL_END_MSB                        0x0184
#define AFE_VUL_END                            0x0188
#define AFE_ADDA_3RD_DAC_DL_SDM_AUTO_RESET_CON 0x018c
#define AFE_ADDA_3RD_DAC_DL_SRC2_CON0          0x0190
#define AFE_ADDA_3RD_DAC_DL_SRC2_CON1          0x0194
#define AFE_ADDA_3RD_DAC_PREDIS_CON0           0x01a0
#define AFE_ADDA_3RD_DAC_PREDIS_CON1           0x01a4
#define AFE_ADDA_3RD_DAC_PREDIS_CON2           0x01a8
#define AFE_ADDA_3RD_DAC_PREDIS_CON3           0x01ac
#define AFE_ADDA_3RD_DAC_DL_SDM_DCCOMP_CON     0x01b0
#define AFE_ADDA_3RD_DAC_DL_SDM_TEST           0x01b4
#define AFE_ADDA_3RD_DAC_DL_DC_COMP_CFG0       0x01b8
#define AFE_ADDA_3RD_DAC_DL_DC_COMP_CFG1       0x01bc
#define AFE_ADDA_3RD_DAC_DL_SDM_FIFO_MON       0x01c0
#define AFE_ADDA_3RD_DAC_DL_SRC_LCH_MON        0x01c4
#define AFE_ADDA_3RD_DAC_DL_SRC_RCH_MON        0x01c8
#define AFE_ADDA_3RD_DAC_DL_SDM_OUT_MON        0x01cc
#define AFE_SIDETONE_DEBUG                     0x01d0
#define AFE_SIDETONE_MON                       0x01d4
#define AFE_ADDA_3RD_DAC_DL_SDM_DITHER_CON     0x01d8
#define AFE_SINEGEN_CON2                       0x01dc
#define AFE_SIDETONE_CON0                      0x01e0
#define AFE_SIDETONE_COEFF                     0x01e4
#define AFE_SIDETONE_CON1                      0x01e8
#define AFE_SIDETONE_GAIN                      0x01ec
#define AFE_SINEGEN_CON0                       0x01f0
#define AFE_SINEGEN_CON_TDM                    0x01fc
#define AFE_TOP_CON0                           0x0200
#define AFE_VUL2_CON0                          0x020c
#define AFE_VUL2_BASE_MSB                      0x0210
#define AFE_VUL2_BASE                          0x0214
#define AFE_VUL2_CUR_MSB                       0x0218
#define AFE_VUL2_CUR                           0x021c
#define AFE_VUL2_END_MSB                       0x0220
#define AFE_VUL2_END                           0x0224
#define AFE_VUL3_CON0                          0x0228
#define AFE_VUL3_BASE_MSB                      0x022c
#define AFE_VUL3_BASE                          0x0230
#define AFE_VUL3_CUR_MSB                       0x0234
#define AFE_VUL3_CUR                           0x0238
#define AFE_VUL3_END_MSB                       0x023c
#define AFE_VUL3_END                           0x0240
#define AFE_BUSY                               0x0244
#define AFE_BUS_CFG                            0x0250
#define AFE_ADDA_PREDIS_CON0                   0x0260
#define AFE_ADDA_PREDIS_CON1                   0x0264
#define AFE_I2S_MON                            0x027c
#define AFE_ADDA_IIR_COEF_02_01                0x0290
#define AFE_ADDA_IIR_COEF_04_03                0x0294
#define AFE_ADDA_IIR_COEF_06_05                0x0298
#define AFE_ADDA_IIR_COEF_08_07                0x029c
#define AFE_ADDA_IIR_COEF_10_09                0x02a0
#define AFE_ADDA_ULCF_CFG_02_01                0x02a4
#define AFE_ADDA_ULCF_CFG_04_03                0x02a8
#define AFE_ADDA_ULCF_CFG_06_05                0x02ac
#define AFE_ADDA_ULCF_CFG_08_07                0x02b0
#define AFE_ADDA_ULCF_CFG_10_09                0x02b4
#define AFE_ADDA_ULCF_CFG_12_11                0x02b8
#define AFE_ADDA_ULCF_CFG_14_13                0x02bc
#define AFE_ADDA_ULCF_CFG_16_15                0x02c0
#define AFE_ADDA_ULCF_CFG_18_17                0x02c4
#define AFE_ADDA_ULCF_CFG_20_19                0x02c8
#define AFE_ADDA_ULCF_CFG_22_21                0x02cc
#define AFE_ADDA_ULCF_CFG_24_23                0x02d0
#define AFE_ADDA_ULCF_CFG_26_25                0x02d4
#define AFE_ADDA_ULCF_CFG_28_27                0x02d8
#define AFE_ADDA_ULCF_CFG_30_29                0x02dc
#define AFE_IRQ_MCU_CON1                       0x02e4
#define AFE_IRQ_MCU_CON2                       0x02e8
#define AFE_DAC_MON                            0x02ec
#define AFE_IRQ_MCU_CON3                       0x02f0
#define AFE_IRQ_MCU_CON4                       0x02f4
#define AFE_IRQ_MCU_CNT0                       0x0300
#define AFE_IRQ_MCU_CNT6                       0x0304
#define AFE_IRQ_MCU_CNT8                       0x0308
#define AFE_IRQ_MCU_DSP2_EN                    0x030c
#define AFE_IRQ0_MCU_CNT_MON                   0x0310
#define AFE_IRQ6_MCU_CNT_MON                   0x0314
#define AFE_BT_SECURITY0                       0x0320
#define AFE_BT_SECURITY1                       0x0324
#define AFE_VUL4_CON0                          0x0358
#define AFE_VUL4_BASE_MSB                      0x035c
#define AFE_VUL4_BASE                          0x0360
#define AFE_VUL4_CUR_MSB                       0x0364
#define AFE_VUL4_CUR                           0x0368
#define AFE_VUL4_END_MSB                       0x036c
#define AFE_VUL4_END                           0x0370
#define AFE_VUL12_CON0                         0x0374
#define AFE_VUL12_BASE_MSB                     0x0378
#define AFE_VUL12_BASE                         0x037c
#define AFE_VUL12_CUR_MSB                      0x0380
#define AFE_VUL12_CUR                          0x0384
#define AFE_VUL12_END_MSB                      0x0388
#define AFE_VUL12_END                          0x038c
#define AFE_HDMI_CONN0                         0x0390
#define AFE_IRQ3_MCU_CNT_MON                   0x0398
#define AFE_IRQ4_MCU_CNT_MON                   0x039c
#define AFE_IRQ_MCU_CON0                       0x03a0
#define AFE_IRQ_MCU_STATUS                     0x03a4
#define AFE_IRQ_MCU_CLR                        0x03a8
#define AFE_IRQ_MCU_CNT1                       0x03ac
#define AFE_IRQ_MCU_CNT2                       0x03b0
#define AFE_IRQ_MCU_EN                         0x03b4
#define AFE_IRQ_MCU_MON2                       0x03b8
#define AFE_IRQ_MCU_CNT5                       0x03bc
#define AFE_IRQ1_MCU_CNT_MON                   0x03c0
#define AFE_IRQ2_MCU_CNT_MON                   0x03c4
#define AFE_IRQ5_MCU_CNT_MON                   0x03cc
#define AFE_IRQ_MCU_DSP_EN                     0x03d0
#define AFE_IRQ_MCU_SCP_EN                     0x03d4
#define AFE_IRQ_MCU_CNT7                       0x03dc
#define AFE_IRQ7_MCU_CNT_MON                   0x03e0
#define AFE_IRQ_MCU_CNT3                       0x03e4
#define AFE_IRQ_MCU_CNT4                       0x03e8
#define AFE_IRQ_MCU_CNT11                      0x03ec
#define AFE_APLL1_TUNER_CFG                    0x03f0
#define AFE_APLL2_TUNER_CFG                    0x03f4
#define AFE_IRQ_MCU_MISS_CLR                   0x03f8
#define AFE_CONN33                             0x0408
#define AFE_IRQ_MCU_CNT12                      0x040c
#define AFE_GAIN1_CON0                         0x0410
#define AFE_GAIN1_CON1                         0x0414
#define AFE_GAIN1_CON2                         0x0418
#define AFE_GAIN1_CON3                         0x041c
#define AFE_CONN7                              0x0420
#define AFE_GAIN1_CUR                          0x0424
#define AFE_GAIN2_CON0                         0x0428
#define AFE_GAIN2_CON1                         0x042c
#define AFE_GAIN2_CON2                         0x0430
#define AFE_GAIN2_CON3                         0x0434
#define AFE_CONN8                              0x0438
#define AFE_GAIN2_CUR                          0x043c
#define AFE_CONN9                              0x0440
#define AFE_CONN10                             0x0444
#define AFE_CONN11                             0x0448
#define AFE_CONN12                             0x044c
#define AFE_CONN13                             0x0450
#define AFE_CONN14                             0x0454
#define AFE_CONN15                             0x0458
#define AFE_CONN16                             0x045c
#define AFE_CONN17                             0x0460
#define AFE_CONN18                             0x0464
#define AFE_CONN19                             0x0468
#define AFE_CONN20                             0x046c
#define AFE_CONN21                             0x0470
#define AFE_CONN22                             0x0474
#define AFE_CONN23                             0x0478
#define AFE_CONN24                             0x047c
#define AFE_CONN_RS                            0x0494
#define AFE_CONN_DI                            0x0498
#define AFE_CONN25                             0x04b0
#define AFE_CONN26                             0x04b4
#define AFE_CONN27                             0x04b8
#define AFE_CONN28                             0x04bc
#define AFE_CONN29                             0x04c0
#define AFE_CONN30                             0x04c4
#define AFE_CONN31                             0x04c8
#define AFE_CONN32                             0x04cc
#define AFE_SRAM_DELSEL_CON1                   0x04f4
#define AFE_CONN56                             0x0500
#define AFE_CONN57                             0x0504
#define AFE_CONN58                             0x0508
#define AFE_CONN59                             0x050c
#define AFE_CONN56_1                           0x0510
#define AFE_CONN57_1                           0x0514
#define AFE_CONN58_1                           0x0518
#define AFE_CONN59_1                           0x051c
#define AFE_TINY_CONN2                         0x0520
#define AFE_TINY_CONN3                         0x0524
#define AFE_TINY_CONN4                         0x0528
#define AFE_TINY_CONN5                         0x052c
#define PCM_INTF_CON1                          0x0530
#define PCM_INTF_CON2                          0x0538
#define PCM2_INTF_CON                          0x053c
#define AFE_TDM_CON1                           0x0548
#define AFE_TDM_CON2                           0x054c
#define AFE_DPTX_CON                           0x0558
#define AFE_DPTX_MON                           0x055c
#define AFE_I2S_CON6                           0x0564
#define AFE_I2S_CON7                           0x0568
#define AFE_I2S_CON8                           0x056c
#define AFE_I2S_CON9                           0x0570
#define AFE_CONN34                             0x0580
#define FPGA_CFG0                              0x05b0
#define FPGA_CFG1                              0x05b4
#define FPGA_CFG2                              0x05c0
#define FPGA_CFG3                              0x05c4
#define AUDIO_TOP_DBG_CON                      0x05c8
#define AUDIO_TOP_DBG_MON0                     0x05cc
#define AUDIO_TOP_DBG_MON1                     0x05d0
#define AFE_IRQ8_MCU_CNT_MON                   0x05e4
#define AFE_IRQ11_MCU_CNT_MON                  0x05e8
#define AFE_IRQ12_MCU_CNT_MON                  0x05ec
#define AFE_IRQ_MCU_CNT9                       0x0600
#define AFE_IRQ_MCU_CNT10                      0x0604
#define AFE_IRQ_MCU_CNT13                      0x0608
#define AFE_IRQ_MCU_CNT14                      0x060c
#define AFE_IRQ_MCU_CNT15                      0x0610
#define AFE_IRQ_MCU_CNT16                      0x0614
#define AFE_IRQ_MCU_CNT17                      0x0618
#define AFE_IRQ_MCU_CNT18                      0x061c
#define AFE_IRQ_MCU_CNT19                      0x0620
#define AFE_IRQ_MCU_CNT20                      0x0624
#define AFE_IRQ_MCU_CNT21                      0x0628
#define AFE_IRQ_MCU_CNT22                      0x062c
#define AFE_IRQ_MCU_CNT23                      0x0630
#define AFE_IRQ_MCU_CNT24                      0x0634
#define AFE_IRQ_MCU_CNT25                      0x0638
#define AFE_IRQ_MCU_CNT26                      0x063c
#define AFE_IRQ_MCU_CNT31                      0x0640
#define AFE_SPM_CONTROL_REQ                    0x0648
#define AFE_SPM_CONTROL_ACK                    0x064c
#define AFE_TINY_CONN6                         0x0650
#define AFE_TINY_CONN7                         0x0654
#define AFE_IRQ9_MCU_CNT_MON                   0x0660
#define AFE_IRQ10_MCU_CNT_MON                  0x0664
#define AFE_IRQ13_MCU_CNT_MON                  0x0668
#define AFE_IRQ14_MCU_CNT_MON                  0x066c
#define AFE_IRQ15_MCU_CNT_MON                  0x0670
#define AFE_IRQ16_MCU_CNT_MON                  0x0674
#define AFE_IRQ17_MCU_CNT_MON                  0x0678
#define AFE_IRQ18_MCU_CNT_MON                  0x067c
#define AFE_IRQ19_MCU_CNT_MON                  0x0680
#define AFE_IRQ20_MCU_CNT_MON                  0x0684
#define AFE_IRQ21_MCU_CNT_MON                  0x0688
#define AFE_IRQ22_MCU_CNT_MON                  0x068c
#define AFE_IRQ23_MCU_CNT_MON                  0x0690
#define AFE_IRQ24_MCU_CNT_MON                  0x0694
#define AFE_IRQ25_MCU_CNT_MON                  0x0698
#define AFE_IRQ26_MCU_CNT_MON                  0x069c
#define AFE_IRQ31_MCU_CNT_MON                  0x06a0
#define AFE_ADDA_MTKAIFV4_TX_CFG0              0x07bc
#define AFE_ADDA6_MTKAIFV4_TX_CFG0             0x07c0
#define AFE_ADDA_MTKAIFV4_RX_CFG0              0x07c4
#define AFE_ADDA_MTKAIFV4_RX_CFG1              0x07c8
#define AFE_ADDA6_MTKAIFV4_RX_CFG0             0x07cc
#define AFE_ADDA6_MTKAIFV4_RX_CFG1             0x07d0
#define AFE_ADDA_MTKAIFV4_TX_SYNCWORD_CFG      0x07d4
#define AFE_ADDA_MTKAIFV4_RX_SYNCWORD_CFG      0x07d8
#define AFE_ADDA_MTKAIFV4_MON0                 0x07dc
#define AFE_ADDA_MTKAIFV4_MON1                 0x07e0
#define AFE_ADDA6_MTKAIFV4_MON0                0x07e4
#define AFE_GENERAL_REG0                       0x0800
#define AFE_GENERAL_REG1                       0x0804
#define AFE_GENERAL_REG2                       0x0808
#define AFE_GENERAL_REG3                       0x080c
#define AFE_GENERAL_REG4                       0x0810
#define AFE_GENERAL_REG5                       0x0814
#define AFE_GENERAL_REG6                       0x0818
#define AFE_GENERAL_REG7                       0x081c
#define AFE_GENERAL_REG8                       0x0820
#define AFE_GENERAL_REG9                       0x0824
#define AFE_GENERAL_REG10                      0x0828
#define AFE_GENERAL_REG11                      0x082c
#define AFE_GENERAL_REG12                      0x0830
#define AFE_GENERAL_REG13                      0x0834
#define AFE_GENERAL_REG14                      0x0838
#define AFE_GENERAL_REG15                      0x083c
#define AFE_CBIP_CFG0                          0x0840
#define AFE_CBIP_MON0                          0x0844
#define AFE_CBIP_SLV_MUX_MON0                  0x0848
#define AFE_CBIP_SLV_DECODER_MON0              0x084c
#define AFE_ADDA6_MTKAIF_MON0                  0x0854
#define AFE_ADDA6_MTKAIF_MON1                  0x0858
#define AFE_AWB_CON0                           0x085c
#define AFE_AWB_BASE_MSB                       0x0860
#define AFE_AWB_BASE                           0x0864
#define AFE_AWB_CUR_MSB                        0x0868
#define AFE_AWB_CUR                            0x086c
#define AFE_AWB_END_MSB                        0x0870
#define AFE_AWB_END                            0x0874
#define AFE_AWB2_CON0                          0x0878
#define AFE_AWB2_BASE_MSB                      0x087c
#define AFE_AWB2_BASE                          0x0880
#define AFE_AWB2_CUR_MSB                       0x0884
#define AFE_AWB2_CUR                           0x0888
#define AFE_AWB2_END_MSB                       0x088c
#define AFE_AWB2_END                           0x0890
#define AFE_DAI_CON0                           0x0894
#define AFE_DAI_BASE_MSB                       0x0898
#define AFE_DAI_BASE                           0x089c
#define AFE_DAI_CUR_MSB                        0x08a0
#define AFE_DAI_CUR                            0x08a4
#define AFE_DAI_END_MSB                        0x08a8
#define AFE_DAI_END                            0x08ac
#define AFE_DAI2_CON0                          0x08b0
#define AFE_DAI2_BASE_MSB                      0x08b4
#define AFE_DAI2_BASE                          0x08b8
#define AFE_DAI2_CUR_MSB                       0x08bc
#define AFE_DAI2_CUR                           0x08c0
#define AFE_DAI2_END_MSB                       0x08c4
#define AFE_DAI2_END                           0x08c8
#define AFE_MEMIF_CON0                         0x08cc
#define AFE_CONN0_1                            0x0900
#define AFE_CONN1_1                            0x0904
#define AFE_CONN2_1                            0x0908
#define AFE_CONN3_1                            0x090c
#define AFE_CONN4_1                            0x0910
#define AFE_CONN5_1                            0x0914
#define AFE_CONN6_1                            0x0918
#define AFE_CONN7_1                            0x091c
#define AFE_CONN8_1                            0x0920
#define AFE_CONN9_1                            0x0924
#define AFE_CONN10_1                           0x0928
#define AFE_CONN11_1                           0x092c
#define AFE_CONN12_1                           0x0930
#define AFE_CONN13_1                           0x0934
#define AFE_CONN14_1                           0x0938
#define AFE_CONN15_1                           0x093c
#define AFE_CONN16_1                           0x0940
#define AFE_CONN17_1                           0x0944
#define AFE_CONN18_1                           0x0948
#define AFE_CONN19_1                           0x094c
#define AFE_CONN20_1                           0x0950
#define AFE_CONN21_1                           0x0954
#define AFE_CONN22_1                           0x0958
#define AFE_CONN23_1                           0x095c
#define AFE_CONN24_1                           0x0960
#define AFE_CONN25_1                           0x0964
#define AFE_CONN26_1                           0x0968
#define AFE_CONN27_1                           0x096c
#define AFE_CONN28_1                           0x0970
#define AFE_CONN29_1                           0x0974
#define AFE_CONN30_1                           0x0978
#define AFE_CONN31_1                           0x097c
#define AFE_CONN32_1                           0x0980
#define AFE_CONN33_1                           0x0984
#define AFE_CONN34_1                           0x0988
#define AFE_CONN_RS_1                          0x098c
#define AFE_CONN_DI_1                          0x0990
#define AFE_CONN_24BIT_1                       0x0994
#define AFE_CONN_REG                           0x0998
#define AFE_CONN35                             0x09a0
#define AFE_CONN36                             0x09a4
#define AFE_CONN37                             0x09a8
#define AFE_CONN38                             0x09ac
#define AFE_CONN35_1                           0x09b0
#define AFE_CONN36_1                           0x09b4
#define AFE_CONN37_1                           0x09b8
#define AFE_CONN38_1                           0x09bc
#define AFE_CONN39                             0x09c0
#define AFE_CONN40                             0x09c4
#define AFE_CONN41                             0x09c8
#define AFE_CONN42                             0x09cc
#define AFE_SGEN_CON_SGEN32                    0x09d0
#define AFE_CONN39_1                           0x09e0
#define AFE_CONN40_1                           0x09e4
#define AFE_CONN41_1                           0x09e8
#define AFE_CONN42_1                           0x09ec
#define AFE_I2S_CON4                           0x09f8
#define ETDM_IN0_CON0                          0x0a04
#define ETDM_IN0_CON1                          0x0a08
#define ETDM_IN0_CON2                          0x0a0c
#define ETDM_IN0_CON3                          0x0a10
#define ETDM_IN0_CON4                          0x0a14
#define ETDM_IN0_CON5                          0x0a18
#define ETDM_IN0_CON6                          0x0a1c
#define ETDM_IN0_CON7                          0x0a20
#define ETDM_IN0_CON8                          0x0a24
#define ETDM_OUT0_CON0                         0x0a28
#define ETDM_OUT0_CON1                         0x0a2c
#define ETDM_OUT0_CON2                         0x0a30
#define ETDM_OUT0_CON3                         0x0a34
#define ETDM_OUT0_CON4                         0x0a38
#define ETDM_OUT0_CON5                         0x0a3c
#define ETDM_OUT0_CON6                         0x0a40
#define ETDM_OUT0_CON7                         0x0a44
#define ETDM_OUT0_CON8                         0x0a48
#define ETDM_IN0_MON                           0x0a4c
#define ETDM_OUT0_MON                          0x0a50
#define AFE_ADDA6_TOP_CON0                     0x0a80
#define AFE_ADDA6_UL_SRC_CON0                  0x0a84
#define AFE_ADDA6_UL_SRC_CON1                  0x0a88
#define AFE_ADDA6_SRC_DEBUG                    0x0a8c
#define AFE_ADDA6_SRC_DEBUG_MON0               0x0a90
#define AFE_ADDA6_ULCF_CFG_02_01               0x0aa0
#define AFE_ADDA6_ULCF_CFG_04_03               0x0aa4
#define AFE_ADDA6_ULCF_CFG_06_05               0x0aa8
#define AFE_ADDA6_ULCF_CFG_08_07               0x0aac
#define AFE_ADDA6_ULCF_CFG_10_09               0x0ab0
#define AFE_ADDA6_ULCF_CFG_12_11               0x0ab4
#define AFE_ADDA6_ULCF_CFG_14_13               0x0ab8
#define AFE_ADDA6_ULCF_CFG_16_15               0x0abc
#define AFE_ADDA6_ULCF_CFG_18_17               0x0ac0
#define AFE_ADDA6_ULCF_CFG_20_19               0x0ac4
#define AFE_ADDA6_ULCF_CFG_22_21               0x0ac8
#define AFE_ADDA6_ULCF_CFG_24_23               0x0acc
#define AFE_ADDA6_ULCF_CFG_26_25               0x0ad0
#define AFE_ADDA6_ULCF_CFG_28_27               0x0ad4
#define AFE_ADDA6_ULCF_CFG_30_29               0x0ad8
#define AFE_ADDA6_UL_SRC_MON0                  0x0ae4
#define AFE_ADDA6_UL_SRC_MON1                  0x0ae8
#define AFE_TINY_CONN0                         0x0af0
#define AFE_TINY_CONN1                         0x0af4
#define AFE_CONN43                             0x0af8
#define AFE_CONN43_1                           0x0afc
#define AFE_MOD_DAI_CON0                       0x0b00
#define AFE_MOD_DAI_BASE_MSB                   0x0b04
#define AFE_MOD_DAI_BASE                       0x0b08
#define AFE_MOD_DAI_CUR_MSB                    0x0b0c
#define AFE_MOD_DAI_CUR                        0x0b10
#define AFE_MOD_DAI_END_MSB                    0x0b14
#define AFE_MOD_DAI_END                        0x0b18
#define AFE_HDMI_OUT_CON0                      0x0b1c
#define AFE_HDMI_OUT_BASE_MSB                  0x0b20
#define AFE_HDMI_OUT_BASE                      0x0b24
#define AFE_HDMI_OUT_CUR_MSB                   0x0b28
#define AFE_HDMI_OUT_CUR                       0x0b2c
#define AFE_HDMI_OUT_END_MSB                   0x0b30
#define AFE_HDMI_OUT_END                       0x0b34
#define AFE_AWB_RCH_MON                        0x0b70
#define AFE_AWB_LCH_MON                        0x0b74
#define AFE_VUL_RCH_MON                        0x0b78
#define AFE_VUL_LCH_MON                        0x0b7c
#define AFE_VUL12_RCH_MON                      0x0b80
#define AFE_VUL12_LCH_MON                      0x0b84
#define AFE_VUL2_RCH_MON                       0x0b88
#define AFE_VUL2_LCH_MON                       0x0b8c
#define AFE_DAI_DATA_MON                       0x0b90
#define AFE_MOD_DAI_DATA_MON                   0x0b94
#define AFE_DAI2_DATA_MON                      0x0b98
#define AFE_AWB2_RCH_MON                       0x0b9c
#define AFE_AWB2_LCH_MON                       0x0ba0
#define AFE_VUL3_RCH_MON                       0x0ba4
#define AFE_VUL3_LCH_MON                       0x0ba8
#define AFE_VUL4_RCH_MON                       0x0bac
#define AFE_VUL4_LCH_MON                       0x0bb0
#define AFE_VUL5_RCH_MON                       0x0bb4
#define AFE_VUL5_LCH_MON                       0x0bb8
#define AFE_VUL6_RCH_MON                       0x0bbc
#define AFE_VUL6_LCH_MON                       0x0bc0
#define AFE_DL1_RCH_MON                        0x0bc4
#define AFE_DL1_LCH_MON                        0x0bc8
#define AFE_DL2_RCH_MON                        0x0bcc
#define AFE_DL2_LCH_MON                        0x0bd0
#define AFE_DL12_RCH1_MON                      0x0bd4
#define AFE_DL12_LCH1_MON                      0x0bd8
#define AFE_DL12_RCH2_MON                      0x0bdc
#define AFE_DL12_LCH2_MON                      0x0be0
#define AFE_DL3_RCH_MON                        0x0be4
#define AFE_DL3_LCH_MON                        0x0be8
#define AFE_DL4_RCH_MON                        0x0bec
#define AFE_DL4_LCH_MON                        0x0bf0
#define AFE_DL5_RCH_MON                        0x0bf4
#define AFE_DL5_LCH_MON                        0x0bf8
#define AFE_DL6_RCH_MON                        0x0bfc
#define AFE_DL6_LCH_MON                        0x0c00
#define AFE_DL7_RCH_MON                        0x0c04
#define AFE_DL7_LCH_MON                        0x0c08
#define AFE_DL8_RCH_MON                        0x0c0c
#define AFE_DL8_LCH_MON                        0x0c10
#define AFE_VUL5_CON0                          0x0c14
#define AFE_VUL5_BASE_MSB                      0x0c18
#define AFE_VUL5_BASE                          0x0c1c
#define AFE_VUL5_CUR_MSB                       0x0c20
#define AFE_VUL5_CUR                           0x0c24
#define AFE_VUL5_END_MSB                       0x0c28
#define AFE_VUL5_END                           0x0c2c
#define AFE_VUL6_CON0                          0x0c30
#define AFE_VUL6_BASE_MSB                      0x0c34
#define AFE_VUL6_BASE                          0x0c38
#define AFE_VUL6_CUR_MSB                       0x0c3c
#define AFE_VUL6_CUR                           0x0c40
#define AFE_VUL6_END_MSB                       0x0c44
#define AFE_VUL6_END                           0x0c48
#define AFE_ADDA_DL_SDM_DCCOMP_CON             0x0c50
#define AFE_ADDA_DL_SDM_TEST                   0x0c54
#define AFE_ADDA_DL_DC_COMP_CFG0               0x0c58
#define AFE_ADDA_DL_DC_COMP_CFG1               0x0c5c
#define AFE_ADDA_DL_SDM_FIFO_MON               0x0c60
#define AFE_ADDA_DL_SRC_LCH_MON                0x0c64
#define AFE_ADDA_DL_SRC_RCH_MON                0x0c68
#define AFE_ADDA_DL_SDM_OUT_MON                0x0c6c
#define AFE_ADDA_DL_SDM_DITHER_CON             0x0c70
#define AFE_ADDA_DL_SDM_AUTO_RESET_CON         0x0c74
#define AFE_CONNSYS_I2S_CON                    0x0c78
#define AFE_CONNSYS_I2S_MON                    0x0c7c
#define AFE_ASRC_2CH_CON0                      0x0c80
#define AFE_ASRC_2CH_CON1                      0x0c84
#define AFE_ASRC_2CH_CON2                      0x0c88
#define AFE_ASRC_2CH_CON3                      0x0c8c
#define AFE_ASRC_2CH_CON4                      0x0c90
#define AFE_ASRC_2CH_CON5                      0x0c94
#define AFE_ASRC_2CH_CON6                      0x0c98
#define AFE_ASRC_2CH_CON7                      0x0c9c
#define AFE_ASRC_2CH_CON8                      0x0ca0
#define AFE_ASRC_2CH_CON9                      0x0ca4
#define AFE_ASRC_2CH_CON10                     0x0ca8
#define AFE_ASRC_2CH_CON12                     0x0cb0
#define AFE_ASRC_2CH_CON13                     0x0cb4
#define AFE_ADDA6_IIR_COEF_02_01               0x0ce0
#define AFE_ADDA6_IIR_COEF_04_03               0x0ce4
#define AFE_ADDA6_IIR_COEF_06_05               0x0ce8
#define AFE_ADDA6_IIR_COEF_08_07               0x0cec
#define AFE_ADDA6_IIR_COEF_10_09               0x0cf0
#define AFE_SE_PROT_SIDEBAND                   0x0d38
#define AFE_SE_DOMAIN_SIDEBAND0                0x0d3c
#define AFE_ADDA_PREDIS_CON2                   0x0d40
#define AFE_ADDA_PREDIS_CON3                   0x0d44
#define AFE_MEMIF_CONN                         0x0d50
#define AFE_SE_DOMAIN_SIDEBAND1                0x0d54
#define AFE_SE_DOMAIN_SIDEBAND2                0x0d58
#define AFE_SE_DOMAIN_SIDEBAND3                0x0d5c
#define AFE_CONN44                             0x0d70
#define AFE_CONN45                             0x0d74
#define AFE_CONN46                             0x0d78
#define AFE_CONN47                             0x0d7c
#define AFE_CONN44_1                           0x0d80
#define AFE_CONN45_1                           0x0d84
#define AFE_CONN46_1                           0x0d88
#define AFE_CONN47_1                           0x0d8c
#define AFE_DL9_CUR_MSB                        0x0dc0
#define AFE_DL9_CUR                            0x0dc4
#define AFE_DL9_END_MSB                        0x0dc8
#define AFE_DL9_END                            0x0dcc
#define AFE_HD_ENGEN_ENABLE                    0x0dd0
#define AFE_ADDA_MTKAIF_CFG0                   0x0e00
#define AFE_ADDA_MTKAIF_SYNCWORD_CFG           0x0e14
#define AFE_ADDA_MTKAIF_RX_CFG0                0x0e20
#define AFE_ADDA_MTKAIF_RX_CFG1                0x0e24
#define AFE_ADDA_MTKAIF_RX_CFG2                0x0e28
#define AFE_ADDA_MTKAIF_MON0                   0x0e34
#define AFE_ADDA_MTKAIF_MON1                   0x0e38
#define AFE_AUD_PAD_TOP                        0x0e40
#define AFE_ADDA6_MTKAIF_CFG0                  0x0e70
#define AFE_ADDA6_MTKAIF_RX_CFG0               0x0e74
#define AFE_ADDA6_MTKAIF_RX_CFG1               0x0e78
#define AFE_ADDA6_MTKAIF_RX_CFG2               0x0e7c
#define AFE_GENERAL1_ASRC_2CH_CON0             0x0e80
#define AFE_GENERAL1_ASRC_2CH_CON1             0x0e84
#define AFE_GENERAL1_ASRC_2CH_CON2             0x0e88
#define AFE_GENERAL1_ASRC_2CH_CON3             0x0e8c
#define AFE_GENERAL1_ASRC_2CH_CON4             0x0e90
#define AFE_GENERAL1_ASRC_2CH_CON5             0x0e94
#define AFE_GENERAL1_ASRC_2CH_CON6             0x0e98
#define AFE_GENERAL1_ASRC_2CH_CON7             0x0e9c
#define AFE_GENERAL1_ASRC_2CH_CON8             0x0ea0
#define AFE_GENERAL1_ASRC_2CH_CON9             0x0ea4
#define AFE_GENERAL1_ASRC_2CH_CON10            0x0ea8
#define AFE_GENERAL1_ASRC_2CH_CON12            0x0eb0
#define AFE_GENERAL1_ASRC_2CH_CON13            0x0eb4
#define GENERAL_ASRC_MODE                      0x0eb8
#define GENERAL_ASRC_EN_ON                     0x0ebc
#define AFE_CONN48                             0x0ec0
#define AFE_CONN49                             0x0ec4
#define AFE_CONN50                             0x0ec8
#define AFE_CONN51                             0x0ecc
#define AFE_CONN52                             0x0ed0
#define AFE_CONN53                             0x0ed4
#define AFE_CONN54                             0x0ed8
#define AFE_CONN55                             0x0edc
#define AFE_CONN48_1                           0x0ee0
#define AFE_CONN49_1                           0x0ee4
#define AFE_CONN50_1                           0x0ee8
#define AFE_CONN51_1                           0x0eec
#define AFE_CONN52_1                           0x0ef0
#define AFE_CONN53_1                           0x0ef4
#define AFE_CONN54_1                           0x0ef8
#define AFE_CONN55_1                           0x0efc
#define AFE_GENERAL2_ASRC_2CH_CON0             0x0f00
#define AFE_GENERAL2_ASRC_2CH_CON1             0x0f04
#define AFE_GENERAL2_ASRC_2CH_CON2             0x0f08
#define AFE_GENERAL2_ASRC_2CH_CON3             0x0f0c
#define AFE_GENERAL2_ASRC_2CH_CON4             0x0f10
#define AFE_GENERAL2_ASRC_2CH_CON5             0x0f14
#define AFE_GENERAL2_ASRC_2CH_CON6             0x0f18
#define AFE_GENERAL2_ASRC_2CH_CON7             0x0f1c
#define AFE_GENERAL2_ASRC_2CH_CON8             0x0f20
#define AFE_GENERAL2_ASRC_2CH_CON9             0x0f24
#define AFE_GENERAL2_ASRC_2CH_CON10            0x0f28
#define AFE_GENERAL2_ASRC_2CH_CON12            0x0f30
#define AFE_GENERAL2_ASRC_2CH_CON13            0x0f34
#define AFE_DL9_RCH_MON                        0x0f38
#define AFE_DL9_LCH_MON                        0x0f3c
#define AFE_DL5_CON0                           0x0f4c
#define AFE_DL5_BASE_MSB                       0x0f50
#define AFE_DL5_BASE                           0x0f54
#define AFE_DL5_CUR_MSB                        0x0f58
#define AFE_DL5_CUR                            0x0f5c
#define AFE_DL5_END_MSB                        0x0f60
#define AFE_DL5_END                            0x0f64
#define AFE_DL6_CON0                           0x0f68
#define AFE_DL6_BASE_MSB                       0x0f6c
#define AFE_DL6_BASE                           0x0f70
#define AFE_DL6_CUR_MSB                        0x0f74
#define AFE_DL6_CUR                            0x0f78
#define AFE_DL6_END_MSB                        0x0f7c
#define AFE_DL6_END                            0x0f80
#define AFE_DL7_CON0                           0x0f84
#define AFE_DL7_BASE_MSB                       0x0f88
#define AFE_DL7_BASE                           0x0f8c
#define AFE_DL7_CUR_MSB                        0x0f90
#define AFE_DL7_CUR                            0x0f94
#define AFE_DL7_END_MSB                        0x0f98
#define AFE_DL7_END                            0x0f9c
#define AFE_DL8_CON0                           0x0fa0
#define AFE_DL8_BASE_MSB                       0x0fa4
#define AFE_DL8_BASE                           0x0fa8
#define AFE_DL8_CUR_MSB                        0x0fac
#define AFE_DL8_CUR                            0x0fb0
#define AFE_DL8_END_MSB                        0x0fb4
#define AFE_DL8_END                            0x0fb8
#define AFE_DL9_CON0                           0x0fbc
#define AFE_DL9_BASE_MSB                       0x0fc0
#define AFE_DL9_BASE                           0x0fc4
#define AFE_SE_SECURE_CON                      0x1004
#define AFE_PROT_SIDEBAND_MON                  0x1008
#define AFE_DOMAIN_SIDEBAND0_MON               0x100c
#define AFE_DOMAIN_SIDEBAND1_MON               0x1010
#define AFE_DOMAIN_SIDEBAND2_MON               0x1014
#define AFE_DOMAIN_SIDEBAND3_MON               0x1018
#define AFE_SECURE_MASK_CONN0                  0x1020
#define AFE_SECURE_MASK_CONN1                  0x1024
#define AFE_SECURE_MASK_CONN2                  0x1028
#define AFE_SECURE_MASK_CONN3                  0x102c
#define AFE_SECURE_MASK_CONN4                  0x1030
#define AFE_SECURE_MASK_CONN5                  0x1034
#define AFE_SECURE_MASK_CONN6                  0x1038
#define AFE_SECURE_MASK_CONN7                  0x103c
#define AFE_SECURE_MASK_CONN8                  0x1040
#define AFE_SECURE_MASK_CONN9                  0x1044
#define AFE_SECURE_MASK_CONN10                 0x1048
#define AFE_SECURE_MASK_CONN11                 0x104c
#define AFE_SECURE_MASK_CONN12                 0x1050
#define AFE_SECURE_MASK_CONN13                 0x1054
#define AFE_SECURE_MASK_CONN14                 0x1058
#define AFE_SECURE_MASK_CONN15                 0x105c
#define AFE_SECURE_MASK_CONN16                 0x1060
#define AFE_SECURE_MASK_CONN17                 0x1064
#define AFE_SECURE_MASK_CONN18                 0x1068
#define AFE_SECURE_MASK_CONN19                 0x106c
#define AFE_SECURE_MASK_CONN20                 0x1070
#define AFE_SECURE_MASK_CONN21                 0x1074
#define AFE_SECURE_MASK_CONN22                 0x1078
#define AFE_SECURE_MASK_CONN23                 0x107c
#define AFE_SECURE_MASK_CONN24                 0x1080
#define AFE_SECURE_MASK_CONN25                 0x1084
#define AFE_SECURE_MASK_CONN26                 0x1088
#define AFE_SECURE_MASK_CONN27                 0x108c
#define AFE_SECURE_MASK_CONN28                 0x1090
#define AFE_SECURE_MASK_CONN29                 0x1094
#define AFE_SECURE_MASK_CONN30                 0x1098
#define AFE_SECURE_MASK_CONN31                 0x109c
#define AFE_SECURE_MASK_CONN32                 0x10a0
#define AFE_SECURE_MASK_CONN33                 0x10a4
#define AFE_SECURE_MASK_CONN34                 0x10a8
#define AFE_SECURE_MASK_CONN35                 0x10ac
#define AFE_SECURE_MASK_CONN36                 0x10b0
#define AFE_SECURE_MASK_CONN37                 0x10b4
#define AFE_SECURE_MASK_CONN38                 0x10b8
#define AFE_SECURE_MASK_CONN39                 0x10bc
#define AFE_SECURE_MASK_CONN40                 0x10c0
#define AFE_SECURE_MASK_CONN41                 0x10c4
#define AFE_SECURE_MASK_CONN42                 0x10c8
#define AFE_SECURE_MASK_CONN43                 0x10cc
#define AFE_SECURE_MASK_CONN44                 0x10d0
#define AFE_SECURE_MASK_CONN45                 0x10d4
#define AFE_SECURE_MASK_CONN46                 0x10d8
#define AFE_SECURE_MASK_CONN47                 0x10dc
#define AFE_SECURE_MASK_CONN48                 0x10e0
#define AFE_SECURE_MASK_CONN49                 0x10e4
#define AFE_SECURE_MASK_CONN50                 0x10e8
#define AFE_SECURE_MASK_CONN51                 0x10ec
#define AFE_SECURE_MASK_CONN52                 0x10f0
#define AFE_SECURE_MASK_CONN53                 0x10f4
#define AFE_SECURE_MASK_CONN54                 0x10f8
#define AFE_SECURE_MASK_CONN55                 0x10fc
#define AFE_SECURE_MASK_CONN56                 0x1100
#define AFE_SECURE_MASK_CONN57                 0x1104
#define AFE_SECURE_MASK_CONN0_1                0x1108
#define AFE_SECURE_MASK_CONN1_1                0x110c
#define AFE_SECURE_MASK_CONN2_1                0x1110
#define AFE_SECURE_MASK_CONN3_1                0x1114
#define AFE_SECURE_MASK_CONN4_1                0x1118
#define AFE_SECURE_MASK_CONN5_1                0x111c
#define AFE_SECURE_MASK_CONN6_1                0x1120
#define AFE_SECURE_MASK_CONN7_1                0x1124
#define AFE_SECURE_MASK_CONN8_1                0x1128
#define AFE_SECURE_MASK_CONN9_1                0x112c
#define AFE_SECURE_MASK_CONN10_1               0x1130
#define AFE_SECURE_MASK_CONN11_1               0x1134
#define AFE_SECURE_MASK_CONN12_1               0x1138
#define AFE_SECURE_MASK_CONN13_1               0x113c
#define AFE_SECURE_MASK_CONN14_1               0x1140
#define AFE_SECURE_MASK_CONN15_1               0x1144
#define AFE_SECURE_MASK_CONN16_1               0x1148
#define AFE_SECURE_MASK_CONN17_1               0x114c
#define AFE_SECURE_MASK_CONN18_1               0x1150
#define AFE_SECURE_MASK_CONN19_1               0x1154
#define AFE_SECURE_MASK_CONN20_1               0x1158
#define AFE_SECURE_MASK_CONN21_1               0x115c
#define AFE_SECURE_MASK_CONN22_1               0x1160
#define AFE_SECURE_MASK_CONN23_1               0x1164
#define AFE_SECURE_MASK_CONN24_1               0x1168
#define AFE_SECURE_MASK_CONN25_1               0x116c
#define AFE_SECURE_MASK_CONN26_1               0x1170
#define AFE_SECURE_MASK_CONN27_1               0x1174
#define AFE_SECURE_MASK_CONN28_1               0x1178
#define AFE_SECURE_MASK_CONN29_1               0x117c
#define AFE_SECURE_MASK_CONN30_1               0x1180
#define AFE_SECURE_MASK_CONN31_1               0x1184
#define AFE_SECURE_MASK_CONN32_1               0x1188
#define AFE_SECURE_MASK_CONN33_1               0x118c
#define AFE_SECURE_MASK_CONN34_1               0x1190
#define AFE_SECURE_MASK_CONN35_1               0x1194
#define AFE_SECURE_MASK_CONN36_1               0x1198
#define AFE_SECURE_MASK_CONN37_1               0x119c
#define AFE_SECURE_MASK_CONN38_1               0x11a0
#define AFE_SECURE_MASK_CONN39_1               0x11a4
#define AFE_SECURE_MASK_CONN40_1               0x11a8
#define AFE_SECURE_MASK_CONN41_1               0x11ac
#define AFE_SECURE_MASK_CONN42_1               0x11b0
#define AFE_SECURE_MASK_CONN43_1               0x11b4
#define AFE_SECURE_MASK_CONN44_1               0x11b8
#define AFE_SECURE_MASK_CONN45_1               0x11bc
#define AFE_SECURE_MASK_CONN46_1               0x11c0
#define AFE_SECURE_MASK_CONN47_1               0x11c4
#define AFE_SECURE_MASK_CONN48_1               0x11c8
#define AFE_SECURE_MASK_CONN49_1               0x11cc
#define AFE_SECURE_MASK_CONN50_1               0x11d0
#define AFE_SECURE_MASK_CONN51_1               0x11d4
#define AFE_SECURE_MASK_CONN52_1               0x11d8
#define AFE_SECURE_MASK_CONN53_1               0x11dc
#define AFE_SECURE_MASK_CONN54_1               0x11e0
#define AFE_SECURE_MASK_CONN55_1               0x11e4
#define AFE_SECURE_MASK_CONN56_1               0x11e8
#define AFE_SECURE_MASK_TINY_CONN0             0x1200
#define AFE_SECURE_MASK_TINY_CONN1             0x1204
#define AFE_SECURE_MASK_TINY_CONN2             0x1208
#define AFE_SECURE_MASK_TINY_CONN3             0x120c
#define AFE_SECURE_MASK_TINY_CONN4             0x1210
#define AFE_SECURE_MASK_TINY_CONN5             0x1214
#define AFE_SECURE_MASK_TINY_CONN6             0x1218
#define AFE_SECURE_MASK_TINY_CONN7             0x121c
#define FPGA_CFG4                              0x1230
#define FPGA_CFG5                              0x1234
#define FPGA_CFG6                              0x1238
#define FPGA_CFG7                              0x123c
#define FPGA_CFG8                              0x1240
#define FPGA_CFG9                              0x1244
#define FPGA_CFG10                             0x1248
#define FPGA_CFG11                             0x124c
#define FPGA_CFG12                             0x1250
#define FPGA_CFG13                             0x1254
#define AFE_VUL7_CON0                          0x1260
#define AFE_VUL7_BASE_MSB                      0x1264
#define AFE_VUL7_BASE                          0x1268
#define AFE_VUL7_CUR_MSB                       0x126c
#define AFE_VUL7_CUR                           0x1270
#define AFE_VUL7_END_MSB                       0x1274
#define AFE_VUL7_END                           0x1278
#define AFE_VUL7_RCH_MON                       0x127c
#define AFE_VUL7_LCH_MON                       0x1280
#define AFE_DL10_CON0                          0x1290
#define AFE_DL10_BASE_MSB                      0x1294
#define AFE_DL10_BASE                          0x1298
#define AFE_DL10_CUR_MSB                       0x129c
#define AFE_DL10_CUR                           0x12a0
#define AFE_DL10_END_MSB                       0x12a4
#define AFE_DL10_END                           0x12a8
#define AFE_DL10_RCH_MON                       0x12ac
#define AFE_DL10_LCH_MON                       0x12b0
#define AFE_VUL7_2_CON0                        0x12c0
#define AFE_VUL7_2_BASE_MSB                    0x12c4
#define AFE_VUL7_2_BASE                        0x12c8
#define AFE_VUL7_2_CUR_MSB                     0x12cc
#define AFE_VUL7_2_CUR                         0x12d0
#define AFE_VUL7_2_END_MSB                     0x12d4
#define AFE_VUL7_2_END                         0x12d8
#define AFE_CM1_CON0                           0x12f0
#define AFE_CM1_MON                            0x12f4
#define AFE_CM2_CON0                           0x12f8
#define AFE_CM2_MON                            0x12fc
#define AFE_GENERAL3_ASRC_2CH_CON0             0x1300
#define AFE_GENERAL3_ASRC_2CH_CON1             0x1304
#define AFE_GENERAL3_ASRC_2CH_CON2             0x1308
#define AFE_GENERAL3_ASRC_2CH_CON3             0x130c
#define AFE_GENERAL3_ASRC_2CH_CON4             0x1310
#define AFE_GENERAL3_ASRC_2CH_CON5             0x1314
#define AFE_GENERAL3_ASRC_2CH_CON6             0x1318
#define AFE_GENERAL3_ASRC_2CH_CON7             0x131c
#define AFE_GENERAL3_ASRC_2CH_CON8             0x1320
#define AFE_GENERAL3_ASRC_2CH_CON9             0x1324
#define AFE_GENERAL3_ASRC_2CH_CON10            0x1328
#define AFE_GENERAL3_ASRC_2CH_CON12            0x1330
#define AFE_GENERAL3_ASRC_2CH_CON13            0x1334
#define AFE_ADDA7_TOP_CON0                     0x1340
#define AFE_ADDA7_UL_SRC_CON0                  0x1344
#define AFE_ADDA7_UL_SRC_CON1                  0x1348
#define AFE_ADDA7_SRC_DEBUG                    0x1354
#define AFE_ADDA7_SRC_DEBUG_MON0               0x1358
#define AFE_ADDA7_UL_SRC_MON0                  0x1360
#define AFE_ADDA7_UL_SRC_MON1                  0x1364
#define AFE_PROXIMITY_CON0                     0x1370
#define AFE_ADDA7_ULCF_CFG_02_01               0x13a0
#define AFE_ADDA7_ULCF_CFG_04_03               0x13a4
#define AFE_ADDA7_ULCF_CFG_06_05               0x13a8
#define AFE_ADDA7_ULCF_CFG_08_07               0x13ac
#define AFE_ADDA7_ULCF_CFG_10_09               0x13b0
#define AFE_ADDA7_ULCF_CFG_12_11               0x13b4
#define AFE_ADDA7_ULCF_CFG_14_13               0x13b8
#define AFE_ADDA7_ULCF_CFG_16_15               0x13bc
#define AFE_ADDA7_ULCF_CFG_18_17               0x13c0
#define AFE_ADDA7_ULCF_CFG_20_19               0x13c4
#define AFE_ADDA7_ULCF_CFG_22_21               0x13c8
#define AFE_ADDA7_ULCF_CFG_24_23               0x13cc
#define AFE_ADDA7_ULCF_CFG_26_25               0x13d0
#define AFE_ADDA7_ULCF_CFG_28_27               0x13d4
#define AFE_ADDA7_ULCF_CFG_30_29               0x13d8
#define AFE_ADDA7_IIR_COEF_02_01               0x13dc
#define AFE_ADDA7_IIR_COEF_04_03               0x13e0
#define AFE_ADDA7_IIR_COEF_06_05               0x13e4
#define AFE_ADDA7_IIR_COEF_08_07               0x13e8
#define AFE_ADDA7_IIR_COEF_10_09               0x13ec
#define AFE_CONN80_1                           0x13f0
#define AFE_CONN81_1                           0x13f4
#define AFE_CONN82_1                           0x13f8
#define AFE_CONN83_1                           0x13fc
#define AFE_CONN60                             0x1400
#define AFE_CONN61                             0x1404
#define AFE_CONN60_1                           0x1408
#define AFE_CONN61_1                           0x140c
#define AFE_CONN62                             0x1410
#define AFE_CONN63                             0x1414
#define AFE_CONN64                             0x1418
#define AFE_CONN65                             0x141c
#define AFE_CONN66                             0x1420
#define AFE_CONN67                             0x1424
#define AFE_CONN68                             0x1428
#define AFE_CONN69                             0x142c
#define ETDM_IN1_CON0                          0x1430
#define ETDM_IN1_CON1                          0x1434
#define ETDM_IN1_CON2                          0x1438
#define ETDM_IN1_CON3                          0x143c
#define ETDM_IN1_CON4                          0x1440
#define ETDM_IN1_CON5                          0x1444
#define ETDM_IN1_CON6                          0x1448
#define ETDM_IN1_CON7                          0x144c
#define ETDM_IN1_CON8                          0x1450
#define ETDM_OUT1_CON0                         0x1454
#define ETDM_OUT1_CON1                         0x1458
#define ETDM_OUT1_CON2                         0x145c
#define ETDM_OUT1_CON3                         0x1460
#define ETDM_OUT1_CON4                         0x1464
#define ETDM_OUT1_CON5                         0x1468
#define ETDM_OUT1_CON6                         0x146c
#define ETDM_OUT1_CON7                         0x1470
#define ETDM_OUT1_CON8                         0x1474
#define ETDM_IN1_MON                           0x1478
#define ETDM_OUT1_MON                          0x147c
#define AFE_CONN70                             0x1480
#define AFE_CONN71                             0x1484
#define AFE_CONN72                             0x1488
#define AFE_CONN73                             0x148c
#define AFE_CONN74                             0x1490
#define AFE_CONN75                             0x1494
#define AFE_CONN76                             0x1498
#define AFE_CONN77                             0x149c
#define AFE_CONN78                             0x14a0
#define AFE_CONN79                             0x14a4
#define AFE_CONN80                             0x14a8
#define AFE_CONN81                             0x14ac
#define AFE_CONN82                             0x14b0
#define AFE_CONN83                             0x14b4
#define AFE_CONN62_1                           0x14b8
#define AFE_CONN63_1                           0x14bc
#define AFE_CONN64_1                           0x14c0
#define AFE_CONN65_1                           0x14c4
#define AFE_CONN66_1                           0x14c8
#define AFE_CONN67_1                           0x14cc
#define AFE_CONN68_1                           0x14d0
#define AFE_CONN69_1                           0x14d4
#define AFE_CONN70_1                           0x14d8
#define AFE_CONN71_1                           0x14dc
#define AFE_CONN72_1                           0x14e0
#define AFE_CONN73_1                           0x14e4
#define AFE_CONN74_1                           0x14e8
#define AFE_CONN75_1                           0x14ec
#define AFE_CONN76_1                           0x14f0
#define AFE_CONN77_1                           0x14f4
#define AFE_CONN78_1                           0x14f8
#define AFE_CONN79_1                           0x14fc
#define AFE_CONN0_2                            0x1500
#define AFE_CONN1_2                            0x1504
#define AFE_CONN2_2                            0x1508
#define AFE_CONN3_2                            0x150c
#define AFE_CONN4_2                            0x1510
#define AFE_CONN5_2                            0x1514
#define AFE_CONN6_2                            0x1518
#define AFE_CONN7_2                            0x151c
#define AFE_CONN8_2                            0x1520
#define AFE_CONN9_2                            0x1524
#define AFE_CONN10_2                           0x1528
#define AFE_CONN11_2                           0x152c
#define AFE_CONN12_2                           0x1530
#define AFE_CONN13_2                           0x1534
#define AFE_CONN14_2                           0x1538
#define AFE_CONN15_2                           0x153c
#define AFE_CONN16_2                           0x1540
#define AFE_CONN17_2                           0x1544
#define AFE_CONN18_2                           0x1548
#define AFE_CONN19_2                           0x154c
#define AFE_CONN20_2                           0x1550
#define AFE_CONN21_2                           0x1554
#define AFE_CONN22_2                           0x1558
#define AFE_CONN23_2                           0x155c
#define AFE_CONN24_2                           0x1560
#define AFE_CONN25_2                           0x1564
#define AFE_CONN26_2                           0x1568
#define AFE_CONN27_2                           0x156c
#define AFE_CONN28_2                           0x1570
#define AFE_CONN29_2                           0x1574
#define AFE_CONN30_2                           0x1578
#define AFE_CONN31_2                           0x157c
#define AFE_CONN32_2                           0x1580
#define AFE_CONN33_2                           0x1584
#define AFE_CONN34_2                           0x1588
#define AFE_CONN35_2                           0x158c
#define AFE_CONN36_2                           0x1590
#define AFE_CONN37_2                           0x1594
#define AFE_CONN38_2                           0x1598
#define AFE_CONN39_2                           0x159c
#define AFE_CONN40_2                           0x15a0
#define AFE_CONN41_2                           0x15a4
#define AFE_CONN42_2                           0x15a8
#define AFE_CONN43_2                           0x15ac
#define AFE_CONN44_2                           0x15b0
#define AFE_CONN45_2                           0x15b4
#define AFE_CONN46_2                           0x15b8
#define AFE_CONN47_2                           0x15bc
#define AFE_CONN48_2                           0x15c0
#define AFE_CONN49_2                           0x15c4
#define AFE_CONN50_2                           0x15c8
#define AFE_CONN51_2                           0x15cc
#define AFE_CONN52_2                           0x15d0
#define AFE_CONN53_2                           0x15d4
#define AFE_CONN54_2                           0x15d8
#define AFE_CONN55_2                           0x15dc
#define AFE_CONN56_2                           0x15e0
#define AFE_CONN57_2                           0x15e4
#define AFE_CONN58_2                           0x15e8
#define AFE_CONN59_2                           0x15ec
#define AFE_CONN60_2                           0x15f0
#define AFE_CONN61_2                           0x15f4
#define AFE_CONN62_2                           0x15f8
#define AFE_CONN63_2                           0x15fc
#define AFE_CONN64_2                           0x1600
#define AFE_CONN65_2                           0x1604
#define AFE_CONN66_2                           0x1608
#define AFE_CONN67_2                           0x160c
#define AFE_CONN68_2                           0x1610
#define AFE_CONN69_2                           0x1614
#define AFE_CONN70_2                           0x1618
#define AFE_CONN71_2                           0x161c
#define AFE_CONN72_2                           0x1620
#define AFE_CONN73_2                           0x1624
#define AFE_CONN74_2                           0x1628
#define AFE_CONN75_2                           0x162c
#define AFE_CONN76_2                           0x1630
#define AFE_CONN77_2                           0x1634
#define AFE_CONN78_2                           0x1638
#define AFE_CONN79_2                           0x163c
#define AFE_CONN80_2                           0x1640
#define AFE_CONN81_2                           0x1644
#define AFE_CONN82_2                           0x1648
#define AFE_CONN83_2                           0x164c
#define AFE_CONN_RS_2                          0x1650
#define AFE_CONN_DI_2                          0x1654
#define AFE_CONN_24BIT_2                       0x1658
#define AFE_TINY_CONN8                         0x1660
#define AFE_TINY_CONN9                         0x1664
#define AFE_TINY_CONN10                        0x1668
#define AFE_TINY_CONN11                        0x166c
#define AFE_TINY_CONN12                        0x1670
#define ETDM_IN0_CON9                          0x18a8
#define ETDM_IN1_CON9                          0x18ac
#define ETDM_0_3_COWORK_CON0                   0x18b0
#define ETDM_0_3_COWORK_CON1                   0x18b4
#define ETDM_0_3_COWORK_CON3                   0x18bc
#define ETDM_COWORK_SEC_CON0                   0x18c0
#define ETDM_COWORK_SEC_CON1                   0x18c4
#define ETDM_COWORK_SEC_CON3                   0x18cc
#define AFE_GAIN3_CON0                         0x1900
#define AFE_GAIN3_CON1                         0x1904
#define AFE_GAIN3_CON2                         0x1908
#define AFE_GAIN3_CON3                         0x190c
#define AFE_GAIN3_CUR                          0x1910
#define AFE_GAIN4_CON0                         0x1920
#define AFE_GAIN4_CON1                         0x1924
#define AFE_GAIN4_CON2                         0x1928
#define AFE_GAIN4_CON3                         0x192c
#define AFE_GAIN4_CUR                          0x1930
#define AFE_VUL8_CON0                          0x1940
#define AFE_VUL8_BASE_MSB                      0x1944
#define AFE_VUL8_BASE                          0x1948
#define AFE_VUL8_CUR_MSB                       0x194c
#define AFE_VUL8_CUR                           0x1950
#define AFE_VUL8_END_MSB                       0x1954
#define AFE_VUL8_END                           0x1958
#define AFE_VUL8_RCH_MON                       0x195c
#define AFE_VUL8_LCH_MON                       0x1960
#define AFE_VUL9_CON0                          0x1970
#define AFE_VUL9_BASE_MSB                      0x1974
#define AFE_VUL9_BASE                          0x1978
#define AFE_VUL9_CUR_MSB                       0x197c
#define AFE_VUL9_CUR                           0x1980
#define AFE_VUL9_END_MSB                       0x1984
#define AFE_VUL9_END                           0x1988
#define AFE_VUL9_RCH_MON                       0x198c
#define AFE_VUL9_LCH_MON                       0x1990
#define AFE_DL11_CON0                          0x19a0
#define AFE_DL11_BASE_MSB                      0x19a4
#define AFE_DL11_BASE                          0x19a8
#define AFE_DL11_CUR_MSB                       0x19ac
#define AFE_DL11_CUR                           0x19b0
#define AFE_DL11_END_MSB                       0x19b4
#define AFE_DL11_END                           0x19b8
#define AFE_DAC_CON0_USER1                     0x1a00
#define AFE_DAC_CON0_USER2                     0x1a04
#define AFE_AGENT_ON                           0x1b00
#define AFE_AGENT_ON_SET                       0x1b04
#define AFE_AGENT_ON_CLR                       0x1b08

#define AFE_MAX_REGISTER AFE_AGENT_ON_CLR

#define AFE_IRQ_STATUS_BITS 0x87FFFFFF
#define AFE_IRQ_CNT_SHIFT 0
#define AFE_IRQ_CNT_MASK 0x3ffff
#endif
