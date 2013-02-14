/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef MSM_MERCURY_HW_REG_H
#define MSM_MERCURY_HW_REG_H


#define JPEGD_BASE  0x00000000

/* Register ADDR, RMSK, and SHFT*/
/* RW */
#define JPEG_CTRL_COMMON                        JPEG_CTRL_COMMON
#define HWIO_JPEG_CTRL_COMMON_ADDR            (JPEGD_BASE+0x00000000)
#define HWIO_JPEG_CTRL_COMMON__POR                    0x00000000
#define HWIO_JPEG_CTRL_COMMON__RMSK                   0x0000001F
#define HWIO_JPEG_CTRL_COMMON__SHFT                            0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_CTRL_COMMON__JPEG_CTRL_COMMON_ZZ_OVERRIDE_EN__BMSK 0x00000010
#define HWIO_JPEG_CTRL_COMMON__JPEG_CTRL_COMMON_ZZ_OVERRIDE_EN__SHFT          4
#define HWIO_JPEG_CTRL_COMMON__JPEG_CTRL_COMMON_MODE__BMSK           0x0000000F
#define HWIO_JPEG_CTRL_COMMON__JPEG_CTRL_COMMON_MODE__SHFT                    0

/* Register Field FMSK and SHFT*/
/* RW */
#define JPEG_CTRL_ENCODE                     JPEG_CTRL_ENCODE
#define HWIO_JPEG_CTRL_ENCODE_ADDR        (JPEGD_BASE+0x00000008)
#define HWIO_JPEG_CTRL_ENCODE__POR                 0x00000000
#define HWIO_JPEG_CTRL_ENCODE__RMSK                0x00000010
#define HWIO_JPEG_CTRL_ENCODE__SHFT                         4
/* Register Element MIN and MAX*/
#define HWIO_JPEG_CTRL_ENCODE___S                           4

/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_CTRL_ENCODE__JPEG_CTRL_ENCODE_EOI_MARKER_EN__BMSK  0x00000010
#define HWIO_JPEG_CTRL_ENCODE__JPEG_CTRL_ENCODE_EOI_MARKER_EN__SHFT           4

/* Register Field FMSK and SHFT*/
#define JPEG_STATUS                        JPEG_STATUS
#define HWIO_JPEG_STATUS_ADDR        (JPEGD_BASE+0x00000010)
#define HWIO_JPEG_STATUS__POR               0x00000000
#define HWIO_JPEG_STATUS__RMSK              0x00003FF0
#define HWIO_JPEG_STATUS__SHFT                       4
/* Register Element MIN and MAX*/
#define HWIO_JPEG_STATUS___S                         4

/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_STATUS__JPEG_STATUS_REGISTER_TIMEOUT__BMSK       0x00002000
#define HWIO_JPEG_STATUS__JPEG_STATUS_REGISTER_TIMEOUT__SHFT               13
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_EOI__BMSK               0x00001000
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_EOI__SHFT                       12
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_UNESCAPED_FF__BMSK  0x00000800
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_UNESCAPED_FF__SHFT          11
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_INV_HUFFCODE__BMSK  0x00000400
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_INV_HUFFCODE__SHFT          10
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_INV_MARKER__BMSK    0x00000200
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_INV_MARKER__SHFT             9
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_SEQ__BMSK     0x00000100
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_SEQ__SHFT              8
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_OVRFLW__BMSK  0x00000080
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_OVRFLW__SHFT           7
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_UNDFLW__BMSK  0x00000040
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_RSTRT_UNDFLW__SHFT           6
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_SCAN_OVRFLW__BMSK   0x00000020
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_SCAN_OVRFLW__SHFT            5
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_SCAN_UNDFLW__BMSK   0x00000010
#define HWIO_JPEG_STATUS__JPEG_STATUS_DHDQ_ERR_SCAN_UNDFLW__SHFT            4

/* Register ADDR, RMSK, and SHFT*/
/* R */
#define JPEG_SOF_REG_0                               JPEG_SOF_REG_0
#define HWIO_JPEG_SOF_REG_0_ADDR  /* RW */               (JPEGD_BASE+0x00000014)
#define HWIO_JPEG_SOF_REG_0__POR                         0x00000000
#define HWIO_JPEG_SOF_REG_0__RMSK                        0x000000FF
#define HWIO_JPEG_SOF_REG_0__SHFT                                 0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SOF_REG_0__JPEG_SOF_REG_0_NF__BMSK     0x000000FF
#define HWIO_JPEG_SOF_REG_0__JPEG_SOF_REG_0_NF__SHFT              0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_SOF_REG_1                               JPEG_SOF_REG_1
#define HWIO_JPEG_SOF_REG_1_ADDR  /* RW */               (JPEGD_BASE+0x00000018)
#define HWIO_JPEG_SOF_REG_1__POR                         0x00000000
#define HWIO_JPEG_SOF_REG_1__RMSK                        0x00FFFFFF
#define HWIO_JPEG_SOF_REG_1__SHFT                                 0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_C__BMSK      0x00FF0000
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_C__SHFT              16
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_H__BMSK      0x0000F000
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_H__SHFT              12
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_V__BMSK      0x00000F00
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_V__SHFT               8
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_TQ__BMSK     0x000000FF
#define HWIO_JPEG_SOF_REG_1__JPEG_SOF_REG_1_TQ__SHFT              0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_SOF_REG_2                               JPEG_SOF_REG_2
#define HWIO_JPEG_SOF_REG_2_ADDR  /* RW */               (JPEGD_BASE+0x0000001C)
#define HWIO_JPEG_SOF_REG_2__POR                         0x00000000
#define HWIO_JPEG_SOF_REG_2__RMSK                        0xFFFFFFFF
#define HWIO_JPEG_SOF_REG_2__SHFT                                 0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SOF_REG_2__JPEG_SOF_REG_2_Y__BMSK      0xFFFF0000
#define HWIO_JPEG_SOF_REG_2__JPEG_SOF_REG_2_Y__SHFT              16
#define HWIO_JPEG_SOF_REG_2__JPEG_SOF_REG_2_X__BMSK      0x0000FFFF
#define HWIO_JPEG_SOF_REG_2__JPEG_SOF_REG_2_X__SHFT               0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_SOS_REG_0                               JPEG_SOS_REG_0
#define HWIO_JPEG_SOS_REG_0_ADDR  /* RW */               (JPEGD_BASE+0x00000020)
#define HWIO_JPEG_SOS_REG_0__POR                         0x00000000
#define HWIO_JPEG_SOS_REG_0__RMSK                        0xFF000000
#define HWIO_JPEG_SOS_REG_0__SHFT                                24
/*Register Element MIN and MAX*/
#define HWIO_JPEG_SOS_REG_0___S                                  24
#define HWIO_JPEG_SOS_REG_0___S                                  24
#define HWIO_JPEG_SOS_REG_0___S                                  24
#define HWIO_JPEG_SOS_REG_0___S                                  24
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SOS_REG_0__JPEG_SOS_REG_0_NS__BMSK       0xFF000000
#define HWIO_JPEG_SOS_REG_0__JPEG_SOS_REG_0_NS__SHFT               24

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_SOS_REG_1                                   JPEG_SOS_REG_1
#define HWIO_JPEG_SOS_REG_1_ADDR  /* RW */              (JPEGD_BASE+0x00000024)
#define HWIO_JPEG_SOS_REG_1__POR                        0x00000000
#define HWIO_JPEG_SOS_REG_1__RMSK                       0x0000FFFF
#define HWIO_JPEG_SOS_REG_1__SHFT                                0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_CS__BMSK    0x0000FF00
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_CS__SHFT             8
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_TD__BMSK    0x000000F0
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_TD__SHFT             4
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_TA__BMSK    0x0000000F
#define HWIO_JPEG_SOS_REG_1__JPEG_SOS_REG_1_TA__SHFT             0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_QT_IDX                                       JPEG_QT_IDX
#define HWIO_JPEG_QT_IDX_ADDR       (JPEGD_BASE+0x00000030)
#define HWIO_JPEG_QT_IDX__POR                              0x00000000
#define HWIO_JPEG_QT_IDX__RMSK                             0x0000FFFF
#define HWIO_JPEG_QT_IDX__SHFT                                      0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_QT_IDX__JPEG_QT_IDX_TABLE_1__BMSK        0x0000FF00
#define HWIO_JPEG_QT_IDX__JPEG_QT_IDX_TABLE_1__SHFT                  8
#define HWIO_JPEG_QT_IDX__JPEG_QT_IDX_TABLE_0__BMSK         0x000000FF
#define HWIO_JPEG_QT_IDX__JPEG_QT_IDX_TABLE_0__SHFT                  0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DQT                                        JPEG_DQT
#define HWIO_JPEG_DQT_ADDR  /* RW */                    (JPEGD_BASE+0x00000034)
#define HWIO_JPEG_DQT__POR                              0x00000000
#define HWIO_JPEG_DQT__RMSK                             0x0F00FFFF
#define HWIO_JPEG_DQT__SHFT                             0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DQT__JPEG_DQT_TQ__BMSK                0x0F000000
#define HWIO_JPEG_DQT__JPEG_DQT_TQ__SHFT                24
#define HWIO_JPEG_DQT__JPEG_DQT_QK__BMSK                0x0000FFFF
#define HWIO_JPEG_DQT__JPEG_DQT_QK__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DRI                                JPEG_DRI
#define HWIO_JPEG_DRI_ADDR  /* RW */            (JPEGD_BASE+0x00000040)
#define HWIO_JPEG_DRI__POR                      0x00000000
#define HWIO_JPEG_DRI__RMSK                     0x0000FFFF
#define HWIO_JPEG_DRI__SHFT                              0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DRI__JPEG_DRI_RI__BMSK        0x0000FFFF
#define HWIO_JPEG_DRI__JPEG_DRI_RI__SHFT                 0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DHT_REG_0                               JPEG_DHT_REG_0
#define HWIO_JPEG_DHT_REG_0_ADDR  /* RW */               (JPEGD_BASE+0x00000050)
#define HWIO_JPEG_DHT_REG_0__POR                         0x00000000
#define HWIO_JPEG_DHT_REG_0__RMSK                        0x000000FF
#define HWIO_JPEG_DHT_REG_0__SHFT                                 0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DHT_REG_0__JPEG_DHT_REG_0_TH__BMSK     0x000000F0
#define HWIO_JPEG_DHT_REG_0__JPEG_DHT_REG_0_TH__SHFT              4
#define HWIO_JPEG_DHT_REG_0__JPEG_DHT_REG_0_TC__BMSK     0x0000000F
#define HWIO_JPEG_DHT_REG_0__JPEG_DHT_REG_0_TC__SHFT              0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DHT_IDX                                        JPEG_DHT_IDX
#define HWIO_JPEG_DHT_IDX_ADDR  /* RW */      (JPEGD_BASE+0x00000054)
#define HWIO_JPEG_DHT_IDX__POR                                0x00000000
#define HWIO_JPEG_DHT_IDX__RMSK                               0x00000FFF
#define HWIO_JPEG_DHT_IDX__SHFT                                        0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DHT_IDX__JPEG_DHT_IDX_CCC_MAX__BMSK         0x00000F00
#define HWIO_JPEG_DHT_IDX__JPEG_DHT_IDX_CCC_MAX__SHFT                  8
#define HWIO_JPEG_DHT_IDX__JPEG_DHT_IDX_VIJ__BMSK             0x000000FF
#define HWIO_JPEG_DHT_IDX__JPEG_DHT_IDX_VIJ__SHFT                      0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DHT_REG_1                          JPEG_DHT_REG_1
#define HWIO_JPEG_DHT_REG_1_ADDR  /* RW */          (JPEGD_BASE+0x00000058)
#define HWIO_JPEG_DHT_REG_1__POR                    0x00000000
#define HWIO_JPEG_DHT_REG_1__RMSK                   0xFFFFFFFF
#define HWIO_JPEG_DHT_REG_1__SHFT                            0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_0__BMSK       0xFF000000
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_0__SHFT               24
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_1__BMSK       0x00FF0000
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_1__SHFT               16
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_2__BMSK       0x0000FF00
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_2__SHFT                8
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_3__BMSK       0x000000FF
#define HWIO_JPEG_DHT_REG_1__JPEG_DHT_REG_1_VIJ_3__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DHT_CCC_MAX                          JPEG_DHT_CCC_MAX
#define HWIO_JPEG_DHT_CCC_MAX_ADDR  /* RW */            (JPEGD_BASE+0x0000005C)
#define HWIO_JPEG_DHT_CCC_MAX__POR                      0x00000000
#define HWIO_JPEG_DHT_CCC_MAX__RMSK                     0xFFFFFFFF
#define HWIO_JPEG_DHT_CCC_MAX__SHFT                              0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_MAX__BMSK    0xFFFF0000
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_MAX__SHFT            16
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_CCC__BMSK    0x0000FFFF
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_CCC__SHFT             0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_MAX__BMSK    0xFFFF0000
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_MAX__SHFT            16
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_CCC__BMSK    0x0000FFFF
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_CCC_MAX_CCC__SHFT             0
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_LI__BMSK       0x000000FF
#define HWIO_JPEG_DHT_CCC_MAX__JPEG_DHT_LI__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DEC_SCALE                       JPEG_DEC_SCALE
#define HWIO_JPEG_DEC_SCALE_ADDR  /* RW */       (JPEGD_BASE+0x00000060)
#define HWIO_JPEG_DEC_SCALE__POR                 0x00000000
#define HWIO_JPEG_DEC_SCALE__RMSK                0x00000003
#define HWIO_JPEG_DEC_SCALE__SHFT                         0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DEC_SCALE__JPEG_DEC_SCALE_RATIO__BMSK       0x00000003
#define HWIO_JPEG_DEC_SCALE__JPEG_DEC_SCALE_RATIO__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_CONVERT                         JPEG_CONVERT
#define HWIO_JPEG_CONVERT_ADDR  /* RW */       (JPEGD_BASE+0x00000064)
#define HWIO_JPEG_CONVERT__POR                 0x00000000
#define HWIO_JPEG_CONVERT__RMSK                0xFFFF13FF
#define HWIO_JPEG_CONVERT__SHFT                         0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONO_CB_VALUE__BMSK      0xFF000000
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONO_CB_VALUE__SHFT              24
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONO_CR_VALUE__BMSK      0x00FF0000
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONO_CR_VALUE__SHFT              16
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_CLAMP_EN__BMSK           0x00001000
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_CLAMP_EN__SHFT                   12
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_CBCR_SWITCH__BMSK        0x00000200
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_CBCR_SWITCH__SHFT                 9
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONOCHROME_EN__BMSK      0x00000100
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MONOCHROME_EN__SHFT               8
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MEM_ORG__BMSK            0x000000C0
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_MEM_ORG__SHFT                     6
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_422_MCU_TYPE__BMSK       0x00000030
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_422_MCU_TYPE__SHFT                4
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_OUTPUT_FORMAT__BMSK      0x0000000C
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_OUTPUT_FORMAT__SHFT               2
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_INPUT_FORMAT__BMSK       0x00000003
#define HWIO_JPEG_CONVERT__JPEG_CONVERT_INPUT_FORMAT__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_ENC_BYTE_CNT                       JPEG_ENC_BYTE_CNT
#define HWIO_JPEG_ENC_BYTE_CNT_ADDR  /* RW */          (JPEGD_BASE+0x00000070)
#define HWIO_JPEG_ENC_BYTE_CNT__POR                    0x00000000
#define HWIO_JPEG_ENC_BYTE_CNT__RMSK                   0xFFFFFFFF
#define HWIO_JPEG_ENC_BYTE_CNT__SHFT                            0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_ENC_BYTE_CNT__JPEG_ENC_BYTE_CNT_TOT__BMSK     0xFFFFFFFF
#define HWIO_JPEG_ENC_BYTE_CNT__JPEG_ENC_BYTE_CNT_TOT__SHFT              0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_DEBUG                                  JPEG_DEBUG
#define HWIO_JPEG_DEBUG_ADDR  /* RW */              (JPEGD_BASE+0x00000080)
#define HWIO_JPEG_DEBUG__POR                        0x4A504547
#define HWIO_JPEG_DEBUG__RMSK                       0xFFFFFFFF
#define HWIO_JPEG_DEBUG__SHFT                                0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_DEBUG__JPEG_DEBUG__BMSK            0xFFFFFFFF
#define HWIO_JPEG_DEBUG__JPEG_DEBUG__SHFT                     0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_SPARE                                JPEG_SPARE
#define HWIO_JPEG_SPARE_ADDR  /* RW */            (JPEGD_BASE+0x00000084)
#define HWIO_JPEG_SPARE__POR                      0x00000000
#define HWIO_JPEG_SPARE__RMSK                     0xFFFFFFFF
#define HWIO_JPEG_SPARE__SHFT                              0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_SPARE__JPEG_SPARE_00__BMSK            0xFFFFFFFF
#define HWIO_JPEG_SPARE__JPEG_SPARE_00__SHFT                     0

/* Register ADDR, RMSK, and SHFT*/
#define JPEG_REGISTER_TIMEOUT                       JPEG_REGISTER_TIMEOUT
#define HWIO_JPEG_REGISTER_TIMEOUT_ADDR    (JPEGD_BASE+0x00000088)
#define HWIO_JPEG_REGISTER_TIMEOUT__POR                        0x0000FFFF
#define HWIO_JPEG_REGISTER_TIMEOUT__RMSK                       0x0000FFFF
#define HWIO_JPEG_REGISTER_TIMEOUT__SHFT                                0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEG_REGISTER_TIMEOUT__JPEG_TIMEOUT_VALUE__BMSK        0x0000FFFF
#define HWIO_JPEG_REGISTER_TIMEOUT__JPEG_TIMEOUT_VALUE__SHFT                 0

/* Register ADDR, RMSK, and SHFT*/
#define JPEGD_STATUS_BUS_DATA                     JPEGD_STATUS_BUS_DATA
#define HWIO_JPEGD_STATUS_BUS_DATA_ADDR  /* RW */       (JPEGD_BASE+0x00000258)
#define HWIO_JPEGD_STATUS_BUS_DATA__POR                      0x00000000
#define HWIO_JPEGD_STATUS_BUS_DATA__RMSK                     0xFFFFFFFF
#define HWIO_JPEGD_STATUS_BUS_DATA__SHFT                              0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEGD_STATUS_BUS_DATA__STATUS_BUS_DATA__BMSK      0xFFFFFFFF
#define HWIO_JPEGD_STATUS_BUS_DATA__STATUS_BUS_DATA__SHFT               0

/* Register ADDR, RMSK, and SHFT*/
#define JPEGD_STATUS_BUS_CONFIG                     JPEGD_STATUS_BUS_CONFIG
#define HWIO_JPEGD_STATUS_BUS_CONFIG_ADDR  /* RW */     (JPEGD_BASE+0x0000025C)
#define HWIO_JPEGD_STATUS_BUS_CONFIG__POR                        0x00000000
#define HWIO_JPEGD_STATUS_BUS_CONFIG__RMSK                       0x0000001F
#define HWIO_JPEGD_STATUS_BUS_CONFIG__SHFT                                0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEGD_STATUS_BUS_CONFIG__STATUS_BUS_SEL__BMSK         0x0000001F
#define HWIO_JPEGD_STATUS_BUS_CONFIG__STATUS_BUS_SEL__SHFT                  0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_AXI_CONFIG                       RTDMA_JPEG_AXI_CONFIG
#define HWIO_RTDMA_JPEG_AXI_CONFIG_ADDR  /* RW */        (JPEGD_BASE+0x00000260)
#define HWIO_RTDMA_JPEG_AXI_CONFIG__POR                        0x00000024
#define HWIO_RTDMA_JPEG_AXI_CONFIG__RMSK                       0x00000FFF
#define HWIO_RTDMA_JPEG_AXI_CONFIG__SHFT                                0
/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_AXI_CONFIG__OUT_OF_ORDER_WR__BMSK          0x00000800
#define HWIO_RTDMA_JPEG_AXI_CONFIG__OUT_OF_ORDER_WR__SHFT                  11
#define HWIO_RTDMA_JPEG_AXI_CONFIG__OUT_OF_ORDER_RD__BMSK          0x00000400
#define HWIO_RTDMA_JPEG_AXI_CONFIG__OUT_OF_ORDER_RD__SHFT                  10
#define HWIO_RTDMA_JPEG_AXI_CONFIG__BOUND_LIMIT__BMSK              0x00000300
#define HWIO_RTDMA_JPEG_AXI_CONFIG__BOUND_LIMIT__SHFT                       8
#define HWIO_RTDMA_JPEG_AXI_CONFIG__PACK_TIMEOUT__BMSK             0x000000F0
#define HWIO_RTDMA_JPEG_AXI_CONFIG__PACK_TIMEOUT__SHFT                      4
#define HWIO_RTDMA_JPEG_AXI_CONFIG__PACK_MAX_BLEN__BMSK            0x0000000F
#define HWIO_RTDMA_JPEG_AXI_CONFIG__PACK_MAX_BLEN__SHFT                     0

/* Register ADDR, RMSK, and SHFT*/
#define JPEGD_CLK_CONTROL                             JPEGD_CLK_CONTROL
#define HWIO_JPEGD_CLK_CONTROL_ADDR  /* RW */   (JPEGD_BASE+0x00000264)
#define HWIO_JPEGD_CLK_CONTROL__POR             0x00000005
#define HWIO_JPEGD_CLK_CONTROL__RMSK                         0x0000000F
#define HWIO_JPEGD_CLK_CONTROL__SHFT                                  0
/* Register Field FMSK and SHFT*/
#define HWIO_JPEGD_CLK_CONTROL__JPEG_CLKIDLE__BMSK           0x00000008
#define HWIO_JPEGD_CLK_CONTROL__JPEG_CLKIDLE__SHFT                    3
#define HWIO_JPEGD_CLK_CONTROL__JPEG_CLKON__BMSK             0x00000004
#define HWIO_JPEGD_CLK_CONTROL__JPEG_CLKON__SHFT                      2
#define HWIO_JPEGD_CLK_CONTROL__AXI_CLKIDLE__BMSK            0x00000002
#define HWIO_JPEGD_CLK_CONTROL__AXI_CLKIDLE__SHFT                     1
#define HWIO_JPEGD_CLK_CONTROL__AXI_CLKON__BMSK              0x00000001
#define HWIO_JPEGD_CLK_CONTROL__AXI_CLKON__SHFT                       0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUF_CONFIG                     RTDMA_JPEG_WR_BUF_CONFIG
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG_ADDR  /* RW */   (JPEGD_BASE+0x00000200)
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__POR             0x00000000
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__RMSK            0x0000001F
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__SHFT                     0
/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__BUF_FORMAT__BMSK         0x0000001C
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__BUF_FORMAT__SHFT                  2
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__NUM_OF_PLANES__BMSK      0x00000003
#define HWIO_RTDMA_JPEG_WR_BUF_CONFIG__NUM_OF_PLANES__SHFT               0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_OP                               RTDMA_JPEG_WR_OP
#define HWIO_RTDMA_JPEG_WR_OP_ADDR  /* RW */        (JPEGD_BASE+0x00000204)
#define HWIO_RTDMA_JPEG_WR_OP__POR                  0x00000000
#define HWIO_RTDMA_JPEG_WR_OP__RMSK                 0x00000013
#define HWIO_RTDMA_JPEG_WR_OP__SHFT                          0
/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_OP__ALIGN__BMSK          0x00000010
#define HWIO_RTDMA_JPEG_WR_OP__ALIGN__SHFT                   4
#define HWIO_RTDMA_JPEG_WR_OP__FLIP__BMSK           0x00000002
#define HWIO_RTDMA_JPEG_WR_OP__FLIP__SHFT                    1
#define HWIO_RTDMA_JPEG_WR_OP__MIRROR__BMSK         0x00000001
#define HWIO_RTDMA_JPEG_WR_OP__MIRROR__SHFT                  0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUF_Y_PNTR                      RTDMA_JPEG_WR_BUF_Y_PNTR
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR_ADDR    (JPEGD_BASE+0x00000208)
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR__POR                0x00000000
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR__RMSK               0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR__SHFT                        3
/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR___S                          3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR__PNTR__BMSK         0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_Y_PNTR__PNTR__SHFT                  3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUF_U_PNTR                      RTDMA_JPEG_WR_BUF_U_PNTR
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR_ADDR  /* RW */     (JPEGD_BASE+0x0000020C)
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR__POR               0x00000000
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR__RMSK              0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR__SHFT                       3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR___S                         3
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR___S                         3
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR___S                         3
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR___S                         3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR__PNTR__BMSK        0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_U_PNTR__PNTR__SHFT                 3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUF_V_PNTR                       RTDMA_JPEG_WR_BUF_V_PNTR
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR_ADDR   (JPEGD_BASE+0x00000210)
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR__POR                0x00000000
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR__RMSK               0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR__SHFT                        3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR___S                          3
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR___S                          3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR__PNTR__BMSK         0xFFFFFFF8
#define HWIO_RTDMA_JPEG_WR_BUF_V_PNTR__PNTR__SHFT                  3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUF_PITCH                         RTDMA_JPEG_WR_BUF_PITCH
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH_ADDR  /* RW */    (JPEGD_BASE+0x00000214)
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH__POR              0x00000000
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH__RMSK             0x00003FF8
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH__SHFT                      3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH___S                        3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH__PITCH__BMSK          0x00003FF8
#define HWIO_RTDMA_JPEG_WR_BUF_PITCH__PITCH__SHFT                   3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_PLANE_SIZE                      RTDMA_JPEG_WR_PLANE_SIZE
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE_ADDR  /* RW */  (JPEGD_BASE+0x00000218)
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__POR            0x00000000
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__RMSK           0x1FFF1FFF
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__SHFT                    0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__PLANE_VSIZE__BMSK       0x1FFF0000
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__PLANE_VSIZE__SHFT               16
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__PLANE_HSIZE__BMSK       0x00001FFF
#define HWIO_RTDMA_JPEG_WR_PLANE_SIZE__PLANE_HSIZE__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BLOCK_SIZE                      RTDMA_JPEG_WR_BLOCK_SIZE
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE_ADDR  /* RW */    (JPEGD_BASE+0x0000021C)
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__POR              0x00000000
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__RMSK             0x00000FFF
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__SHFT                      0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__BLOCK_VSIZE__BMSK           0x00000FC0
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__BLOCK_VSIZE__SHFT                    6
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__BLOCK_HSIZE__BMSK           0x0000003F
#define HWIO_RTDMA_JPEG_WR_BLOCK_SIZE__BLOCK_HSIZE__SHFT                    0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_BUFFER_SIZE                      RTDMA_JPEG_WR_BUFFER_SIZE
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE_ADDR  /* RW */   (JPEGD_BASE+0x00000220)
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE__POR             0x00000000
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE__RMSK            0x00001FFF
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE__SHFT                     0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE__BUFFER_VSIZE__BMSK         0x00001FFF
#define HWIO_RTDMA_JPEG_WR_BUFFER_SIZE__BUFFER_VSIZE__SHFT                  0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_STA_ACK                      RTDMA_JPEG_WR_STA_ACK
#define HWIO_RTDMA_JPEG_WR_STA_ACK_ADDR  /* RW */   (JPEGD_BASE+0x00000224)
#define HWIO_RTDMA_JPEG_WR_STA_ACK__POR             0x00000000
#define HWIO_RTDMA_JPEG_WR_STA_ACK__RMSK            0x0000000F
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SHFT                     3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_WR_STA_ACK___S                       3
#define HWIO_RTDMA_JPEG_WR_STA_ACK___S                       3
#define HWIO_RTDMA_JPEG_WR_STA_ACK___S                       3
#define HWIO_RTDMA_JPEG_WR_STA_ACK___S                       3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SW_RESET_ABORT_RDY_STA__BMSK   0x00000008
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SW_RESET_ABORT_RDY_STA__SHFT            3
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SW_RESET_ABORT_RDY_ACK__BMSK   0x00000008
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SW_RESET_ABORT_RDY_ACK__SHFT            3
#define HWIO_RTDMA_JPEG_WR_STA_ACK__ERR_STA__BMSK                  0x00000004
#define HWIO_RTDMA_JPEG_WR_STA_ACK__ERR_STA__SHFT                           2
#define HWIO_RTDMA_JPEG_WR_STA_ACK__ERR_ACK__BMSK                  0x00000004
#define HWIO_RTDMA_JPEG_WR_STA_ACK__ERR_ACK__SHFT                           2
#define HWIO_RTDMA_JPEG_WR_STA_ACK__EOF_STA__BMSK                  0x00000002
#define HWIO_RTDMA_JPEG_WR_STA_ACK__EOF_STA__SHFT                           1
#define HWIO_RTDMA_JPEG_WR_STA_ACK__EOF_ACK__BMSK                  0x00000002
#define HWIO_RTDMA_JPEG_WR_STA_ACK__EOF_ACK__SHFT                           1
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SOF_STA__BMSK                  0x00000001
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SOF_STA__SHFT                           0
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SOF_ACK__BMSK           0x00000001
#define HWIO_RTDMA_JPEG_WR_STA_ACK__SOF_ACK__SHFT                    0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_WR_INT_EN                      RTDMA_JPEG_WR_INT_EN
#define HWIO_RTDMA_JPEG_WR_INT_EN_ADDR  /* W */        (JPEGD_BASE+0x00000228)
#define HWIO_RTDMA_JPEG_WR_INT_EN__POR                 0x00000000
#define HWIO_RTDMA_JPEG_WR_INT_EN__RMSK                0x0000000F
#define HWIO_RTDMA_JPEG_WR_INT_EN__SHFT                         0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_WR_INT_EN__SW_RESET_ABORT_RDY_EN__BMSK 0x00000008
#define HWIO_RTDMA_JPEG_WR_INT_EN__SW_RESET_ABORT_RDY_EN__SHFT          3
#define HWIO_RTDMA_JPEG_WR_INT_EN__ERR_EN__BMSK                0x00000004
#define HWIO_RTDMA_JPEG_WR_INT_EN__ERR_EN__SHFT                         2
#define HWIO_RTDMA_JPEG_WR_INT_EN__EOF_EN__BMSK                0x00000002
#define HWIO_RTDMA_JPEG_WR_INT_EN__EOF_EN__SHFT                         1
#define HWIO_RTDMA_JPEG_WR_INT_EN__SOF_EN__BMSK                0x00000001
#define HWIO_RTDMA_JPEG_WR_INT_EN__SOF_EN__SHFT                         0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUF_CONFIG                     RTDMA_JPEG_RD_BUF_CONFIG
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG_ADDR  /* RW */     (JPEGD_BASE+0x00000100)
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__POR               0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__RMSK              0x0000001F
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__SHFT                       0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__BUF_FORMAT__BMSK          0x0000001C
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__BUF_FORMAT__SHFT                   2
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__NUM_OF_PLANES__BMSK       0x00000003
#define HWIO_RTDMA_JPEG_RD_BUF_CONFIG__NUM_OF_PLANES__SHFT                0

/* Register ADDR, RMSK, and SHFT, W */
#define RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO   RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO_ADDR  (JPEGD_BASE+0x00000104)
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__POR            0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__RMSK           0x0000001C
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__SHFT                    2

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO___S                      2
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO___S                      2
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO___S                      2
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO___S                      2

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_APPLY__BMSK   0x00000010
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_APPLY__SHFT            4
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_EOF__BMSK     0x00000008
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_EOF__SHFT              3
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_SOF__BMSK     0x00000004
#define HWIO_RTDMA_JPEG_RD_BUF_MNGR_BUF_ID_FIFO__BUF_SOF__SHFT              2

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUF_Y_PNTR                        RTDMA_JPEG_RD_BUF_Y_PNTR
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR_ADDR  /* RW */   (JPEGD_BASE+0x0000010C)
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR__POR             0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR__RMSK            0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR__SHFT                     3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR___S                       3
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR___S                       3
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR___S                       3
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR___S                       3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR__PNTR__BMSK      0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_Y_PNTR__PNTR__SHFT               3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUF_U_PNTR                     RTDMA_JPEG_RD_BUF_U_PNTR
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR_ADDR  /* RW */ (JPEGD_BASE+0x00000110)
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR__POR           0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR__RMSK          0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR__SHFT                   3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR___S                     3
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR___S                     3
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR___S                     3
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR___S                     3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR__PNTR__BMSK        0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_U_PNTR__PNTR__SHFT               3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUF_V_PNTR                     RTDMA_JPEG_RD_BUF_V_PNTR
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR_ADDR  /* RW */    (JPEGD_BASE+0x00000114)
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR__POR              0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR__RMSK             0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR__SHFT                      3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR___S                        3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR__PNTR__BMSK       0xFFFFFFF8
#define HWIO_RTDMA_JPEG_RD_BUF_V_PNTR__PNTR__SHFT                3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUF_PITCH                       RTDMA_JPEG_RD_BUF_PITCH
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH_ADDR  /* RW */    (JPEGD_BASE+0x00000118)
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH__POR              0x00000000
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH__RMSK             0x00003FF8
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH__SHFT                      3

/* Register Element MIN and MAX*/
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH___S                        3
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH___S                        3

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH__PITCH__BMSK            0x00003FF8
#define HWIO_RTDMA_JPEG_RD_BUF_PITCH__PITCH__SHFT                     3

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_PLANE_SIZE                     RTDMA_JPEG_RD_PLANE_SIZE
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE_ADDR  /* RW */  (JPEGD_BASE+0x0000011C)
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__POR            0x00000000
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__RMSK            0x1FFF1FFF
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__SHFT                     0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__PLANE_VSIZE__BMSK         0x1FFF0000
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__PLANE_VSIZE__SHFT                 16
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__PLANE_HSIZE__BMSK         0x00001FFF
#define HWIO_RTDMA_JPEG_RD_PLANE_SIZE__PLANE_HSIZE__SHFT                  0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BLOCK_SIZE                       RTDMA_JPEG_RD_BLOCK_SIZE
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE_ADDR  /* RW */    (JPEGD_BASE+0x00000120)
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__POR              0x000003CF
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__RMSK             0x00000FFF
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__SHFT                      0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__BLOCK_VSIZE__BMSK       0x00000FC0
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__BLOCK_VSIZE__SHFT                6
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__BLOCK_HSIZE__BMSK       0x0000003F
#define HWIO_RTDMA_JPEG_RD_BLOCK_SIZE__BLOCK_HSIZE__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_BUFFER_SIZE               RTDMA_JPEG_RD_BUFFER_SIZE
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE_ADDR  (JPEGD_BASE+0x00000124)
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE__POR               0x00000000
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE__RMSK              0x00001FFF
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE__SHFT                       0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE__BUFFER_VSIZE__BMSK      0x00001FFF
#define HWIO_RTDMA_JPEG_RD_BUFFER_SIZE__BUFFER_VSIZE__SHFT                0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_STA_ACK                     RTDMA_JPEG_RD_STA_ACK
#define HWIO_RTDMA_JPEG_RD_STA_ACK_ADDR         (JPEGD_BASE+0x00000128)
#define HWIO_RTDMA_JPEG_RD_STA_ACK__POR                     0x00000000
#define HWIO_RTDMA_JPEG_RD_STA_ACK__RMSK                    0x00000003
#define HWIO_RTDMA_JPEG_RD_STA_ACK__SHFT                             0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_STA_ACK__EOF_STA__BMSK           0x00000002
#define HWIO_RTDMA_JPEG_RD_STA_ACK__EOF_STA__SHFT                    1
#define HWIO_RTDMA_JPEG_RD_STA_ACK__SOF_STA__BMSK           0x00000001
#define HWIO_RTDMA_JPEG_RD_STA_ACK__SOF_STA__SHFT                    0
#define HWIO_RTDMA_JPEG_RD_STA_ACK__EOF_ACK__BMSK           0x00000002
#define HWIO_RTDMA_JPEG_RD_STA_ACK__EOF_ACK__SHFT                    1
#define HWIO_RTDMA_JPEG_RD_STA_ACK__SOF_ACK__BMSK           0x00000001
#define HWIO_RTDMA_JPEG_RD_STA_ACK__SOF_ACK__SHFT                    0

/* Register ADDR, RMSK, and SHFT*/
#define RTDMA_JPEG_RD_INT_EN                      RTDMA_JPEG_RD_INT_EN
#define HWIO_RTDMA_JPEG_RD_INT_EN_ADDR  /* W */        (JPEGD_BASE+0x0000012C)
#define HWIO_RTDMA_JPEG_RD_INT_EN__POR                      0x00000000
#define HWIO_RTDMA_JPEG_RD_INT_EN__RMSK                     0x00000003
#define HWIO_RTDMA_JPEG_RD_INT_EN__SHFT                              0

/* Register Field FMSK and SHFT*/
#define HWIO_RTDMA_JPEG_RD_INT_EN__EOF_EN__BMSK             0x00000002
#define HWIO_RTDMA_JPEG_RD_INT_EN__EOF_EN__SHFT                      1
#define HWIO_RTDMA_JPEG_RD_INT_EN__SOF_EN__BMSK             0x00000001
#define HWIO_RTDMA_JPEG_RD_INT_EN__SOF_EN__SHFT                      0

#endif /* MSM_MERCURY_HW_REG_H */
