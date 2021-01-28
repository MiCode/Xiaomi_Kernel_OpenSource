/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MTK_DRTX_REG_H__
#define __MTK_DRTX_REG_H__

#define TOP_OFFSET		(0x2000)
#define ENC0_OFFSET		(0x3000)
#define ENC1_OFFSET		(0x3200)
#define TRANS_OFFSET		(0x3400)
#define AUX_OFFSET		(0x3600)
#define SEC_OFFSET		(0x4000)

#define REG_3000_DP_ENCODER0_P0              (0x3000)
#define LANE_NUM_DP_ENCODER0_P0_FLDMASK                                 0x3
#define LANE_NUM_DP_ENCODER0_P0_FLDMASK_POS                             0
#define LANE_NUM_DP_ENCODER0_P0_FLDMASK_LEN                             2

#define VIDEO_MUTE_SW_DP_ENCODER0_P0_FLDMASK                            0x4
#define VIDEO_MUTE_SW_DP_ENCODER0_P0_FLDMASK_POS                        2
#define VIDEO_MUTE_SW_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define VIDEO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK                           0x8
#define VIDEO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_POS                       3
#define VIDEO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK                        0x10
#define ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK_POS                    4
#define ENHANCED_FRAME_EN_DP_ENCODER0_P0_FLDMASK_LEN                    1

#define HDCP_FRAME_EN_DP_ENCODER0_P0_FLDMASK                            0x20
#define HDCP_FRAME_EN_DP_ENCODER0_P0_FLDMASK_POS                        5
#define HDCP_FRAME_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define IDP_EN_DP_ENCODER0_P0_FLDMASK                                   0x40
#define IDP_EN_DP_ENCODER0_P0_FLDMASK_POS                               6
#define IDP_EN_DP_ENCODER0_P0_FLDMASK_LEN                               1

#define BS_SYMBOL_CNT_RESET_DP_ENCODER0_P0_FLDMASK                      0x80
#define BS_SYMBOL_CNT_RESET_DP_ENCODER0_P0_FLDMASK_POS                  7
#define BS_SYMBOL_CNT_RESET_DP_ENCODER0_P0_FLDMASK_LEN                  1

#define MIXER_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK                         0xff00
#define MIXER_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK_POS                     8
#define MIXER_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK_LEN                     8

#define REG_3004_DP_ENCODER0_P0              (0x3004)
#define MIXER_STUFF_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK                   0xff
#define MIXER_STUFF_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK_POS               0
#define MIXER_STUFF_DUMMY_DATA_DP_ENCODER0_P0_FLDMASK_LEN               8

#define VIDEO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK                         0x100
#define VIDEO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK_POS                     8
#define VIDEO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK                0x200
#define DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS            9
#define DP_TX_ENCODER_4P_RESET_SW_DP_ENCODER0_P0_FLDMASK_LEN            1

#define MIXER_RESET_SW_DP_ENCODER0_P0_FLDMASK                           0x400
#define MIXER_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS                       10
#define MIXER_RESET_SW_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define VIDEO_RESET_SW_DP_ENCODER0_P0_FLDMASK                           0x800
#define VIDEO_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS                       11
#define VIDEO_RESET_SW_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define VIDEO_PATTERN_GEN_RESET_SW_DP_ENCODER0_P0_FLDMASK               0x1000
#define VIDEO_PATTERN_GEN_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS           12
#define VIDEO_PATTERN_GEN_RESET_SW_DP_ENCODER0_P0_FLDMASK_LEN           1

#define SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK                             0x2000
#define SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_POS                         13
#define SDP_RESET_SW_DP_ENCODER0_P0_FLDMASK_LEN                         1

#define DP_TX_MUX_DP_ENCODER0_P0_FLDMASK                                0x4000
#define DP_TX_MUX_DP_ENCODER0_P0_FLDMASK_POS                            14
#define DP_TX_MUX_DP_ENCODER0_P0_FLDMASK_LEN                            1

#define MIXER_FSM_RESET_DP_ENCODER0_P0_FLDMASK                          0x8000
#define MIXER_FSM_RESET_DP_ENCODER0_P0_FLDMASK_POS                      15
#define MIXER_FSM_RESET_DP_ENCODER0_P0_FLDMASK_LEN                      1

#define REG_3008_DP_ENCODER0_P0              (0x3008)
#define VIDEO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK                        0xffff
#define VIDEO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK_POS                    0
#define VIDEO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK_LEN                    16

#define REG_300C_DP_ENCODER0_P0              (0x300C)
#define VIDEO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK                        0xff
#define VIDEO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK_POS                    0
#define VIDEO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK_LEN                    8

#define VIDEO_M_CODE_PULSE_DP_ENCODER0_P0_FLDMASK                       0x100
#define VIDEO_M_CODE_PULSE_DP_ENCODER0_P0_FLDMASK_POS                   8
#define VIDEO_M_CODE_PULSE_DP_ENCODER0_P0_FLDMASK_LEN                   1

#define COMPRESSEDSTREAM_FLAG_DP_ENCODER0_P0_FLDMASK                    0x200
#define COMPRESSEDSTREAM_FLAG_DP_ENCODER0_P0_FLDMASK_POS                9
#define COMPRESSEDSTREAM_FLAG_DP_ENCODER0_P0_FLDMASK_LEN                1

#define SDP_SPLIT_EN_DP_ENCODER0_P0_FLDMASK                             0x400
#define SDP_SPLIT_EN_DP_ENCODER0_P0_FLDMASK_POS                         10
#define SDP_SPLIT_EN_DP_ENCODER0_P0_FLDMASK_LEN                         1

#define SDP_SPLIT_FIFO_RST_DP_ENCODER0_P0_FLDMASK                       0x800
#define SDP_SPLIT_FIFO_RST_DP_ENCODER0_P0_FLDMASK_POS                   11
#define SDP_SPLIT_FIFO_RST_DP_ENCODER0_P0_FLDMASK_LEN                   1

#define VIDEO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK                0x7000
#define VIDEO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_POS            12
#define VIDEO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_LEN            3

#define SDP_AUDIO_ONE_SAMPLE_MODE_DP_ENCODER0_P0_FLDMASK                0x8000
#define SDP_AUDIO_ONE_SAMPLE_MODE_DP_ENCODER0_P0_FLDMASK_POS            15
#define SDP_AUDIO_ONE_SAMPLE_MODE_DP_ENCODER0_P0_FLDMASK_LEN            1

#define REG_3010_DP_ENCODER0_P0              (0x3010)
#define HTOTAL_SW_DP_ENCODER0_P0_FLDMASK                                0xffff
#define HTOTAL_SW_DP_ENCODER0_P0_FLDMASK_POS                            0
#define HTOTAL_SW_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3014_DP_ENCODER0_P0              (0x3014)
#define VTOTAL_SW_DP_ENCODER0_P0_FLDMASK                                0xffff
#define VTOTAL_SW_DP_ENCODER0_P0_FLDMASK_POS                            0
#define VTOTAL_SW_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3018_DP_ENCODER0_P0              (0x3018)
#define HSTART_SW_DP_ENCODER0_P0_FLDMASK                                0xffff
#define HSTART_SW_DP_ENCODER0_P0_FLDMASK_POS                            0
#define HSTART_SW_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_301C_DP_ENCODER0_P0              (0x301C)
#define VSTART_SW_DP_ENCODER0_P0_FLDMASK                                0xffff
#define VSTART_SW_DP_ENCODER0_P0_FLDMASK_POS                            0
#define VSTART_SW_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3020_DP_ENCODER0_P0              (0x3020)
#define HWIDTH_SW_DP_ENCODER0_P0_FLDMASK                                0xffff
#define HWIDTH_SW_DP_ENCODER0_P0_FLDMASK_POS                            0
#define HWIDTH_SW_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3024_DP_ENCODER0_P0              (0x3024)
#define VHEIGHT_SW_DP_ENCODER0_P0_FLDMASK                               0xffff
#define VHEIGHT_SW_DP_ENCODER0_P0_FLDMASK_POS                           0
#define VHEIGHT_SW_DP_ENCODER0_P0_FLDMASK_LEN                           16

#define REG_3028_DP_ENCODER0_P0              (0x3028)
#define HSW_SW_DP_ENCODER0_P0_FLDMASK                                   0x7fff
#define HSW_SW_DP_ENCODER0_P0_FLDMASK_POS                               0
#define HSW_SW_DP_ENCODER0_P0_FLDMASK_LEN                               15

#define HSP_SW_DP_ENCODER0_P0_FLDMASK                                   0x8000
#define HSP_SW_DP_ENCODER0_P0_FLDMASK_POS                               15
#define HSP_SW_DP_ENCODER0_P0_FLDMASK_LEN                               1

#define REG_302C_DP_ENCODER0_P0              (0x302C)
#define VSW_SW_DP_ENCODER0_P0_FLDMASK                                   0x7fff
#define VSW_SW_DP_ENCODER0_P0_FLDMASK_POS                               0
#define VSW_SW_DP_ENCODER0_P0_FLDMASK_LEN                               15

#define VSP_SW_DP_ENCODER0_P0_FLDMASK                                   0x8000
#define VSP_SW_DP_ENCODER0_P0_FLDMASK_POS                               15
#define VSP_SW_DP_ENCODER0_P0_FLDMASK_LEN                               1

#define REG_3030_DP_ENCODER0_P0              (0x3030)
#define HTOTAL_SEL_DP_ENCODER0_P0_FLDMASK                               0x1
#define HTOTAL_SEL_DP_ENCODER0_P0_FLDMASK_POS                           0
#define HTOTAL_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define VTOTAL_SEL_DP_ENCODER0_P0_FLDMASK                               0x2
#define VTOTAL_SEL_DP_ENCODER0_P0_FLDMASK_POS                           1
#define VTOTAL_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define HSTART_SEL_DP_ENCODER0_P0_FLDMASK                               0x4
#define HSTART_SEL_DP_ENCODER0_P0_FLDMASK_POS                           2
#define HSTART_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define VSTART_SEL_DP_ENCODER0_P0_FLDMASK                               0x8
#define VSTART_SEL_DP_ENCODER0_P0_FLDMASK_POS                           3
#define VSTART_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define HWIDTH_SEL_DP_ENCODER0_P0_FLDMASK                               0x10
#define HWIDTH_SEL_DP_ENCODER0_P0_FLDMASK_POS                           4
#define HWIDTH_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define VHEIGHT_SEL_DP_ENCODER0_P0_FLDMASK                              0x20
#define VHEIGHT_SEL_DP_ENCODER0_P0_FLDMASK_POS                          5
#define VHEIGHT_SEL_DP_ENCODER0_P0_FLDMASK_LEN                          1

#define HSP_SEL_DP_ENCODER0_P0_FLDMASK                                  0x40
#define HSP_SEL_DP_ENCODER0_P0_FLDMASK_POS                              6
#define HSP_SEL_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define HSW_SEL_DP_ENCODER0_P0_FLDMASK                                  0x80
#define HSW_SEL_DP_ENCODER0_P0_FLDMASK_POS                              7
#define HSW_SEL_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define VSP_SEL_DP_ENCODER0_P0_FLDMASK                                  0x100
#define VSP_SEL_DP_ENCODER0_P0_FLDMASK_POS                              8
#define VSP_SEL_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define VSW_SEL_DP_ENCODER0_P0_FLDMASK                                  0x200
#define VSW_SEL_DP_ENCODER0_P0_FLDMASK_POS                              9
#define VSW_SEL_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define TX_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK                            0x400
#define TX_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK_POS                        10
#define TX_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK                  0x800
#define VBID_AUDIO_MUTE_SW_DP_ENCODER0_P0_FLDMASK_POS                   11
#define VBID_AUDIO_MUTE_FLAG_SW_DP_ENCODER0_P0_FLDMASK_LEN              1

#define VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK                 0x1000
#define VBID_AUDIO_MUTE_SEL_DP_ENCODER0_P0_FLDMASK_POS                  12
#define VBID_AUDIO_MUTE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK_LEN             1

#define VBID_INTERLACE_FLAG_SW_DP_ENCODER0_P0_FLDMASK                   0x2000
#define VBID_INTERLACE_FLAG_SW_DP_ENCODER0_P0_FLDMASK_POS               13
#define VBID_INTERLACE_FLAG_SW_DP_ENCODER0_P0_FLDMASK_LEN               1

#define VBID_INTERLACE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK                  0x4000
#define VBID_INTERLACE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK_POS              14
#define VBID_INTERLACE_FLAG_SEL_DP_ENCODER0_P0_FLDMASK_LEN              1

#define MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK                             0x8000
#define MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK_POS                         15
#define MIXER_SDP_EN_DP_ENCODER0_P0_FLDMASK_LEN                         1

#define REG_3034_DP_ENCODER0_P0              (0x3034)
#define MISC0_DATA_DP_ENCODER0_P0_FLDMASK                               0xff
#define MISC0_DATA_DP_ENCODER0_P0_FLDMASK_POS                           0
#define MISC0_DATA_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define MISC1_DATA_DP_ENCODER0_P0_FLDMASK                               0xff00
#define MISC1_DATA_DP_ENCODER0_P0_FLDMASK_POS                           8
#define MISC1_DATA_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define REG_3038_DP_ENCODER0_P0              (0x3038)
#define TX_VBID_SW_DP_ENCODER0_P0_FLDMASK                               0xff
#define TX_VBID_SW_DP_ENCODER0_P0_FLDMASK_POS                           0
#define TX_VBID_SW_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define VIDEO_DATA_SWAP_DP_ENCODER0_P0_FLDMASK                          0x700
#define VIDEO_DATA_SWAP_DP_ENCODER0_P0_FLDMASK_POS                      8
#define VIDEO_DATA_SWAP_DP_ENCODER0_P0_FLDMASK_LEN                      3

#define VIDEO_SOURCE_SEL_DP_ENCODER0_P0_FLDMASK                         0x800
#define VIDEO_SOURCE_SEL_DP_ENCODER0_P0_FLDMASK_POS                     11
#define VIDEO_SOURCE_SEL_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define FIELD_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK                         0x1000
#define FIELD_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK_POS                     12
#define FIELD_VBID_SW_EN_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define FIELD_SW_DP_ENCODER0_P0_FLDMASK                                 0x2000
#define FIELD_SW_DP_ENCODER0_P0_FLDMASK_POS                             13
#define FIELD_SW_DP_ENCODER0_P0_FLDMASK_LEN                             1

#define V3D_EN_SW_DP_ENCODER0_P0_FLDMASK                                0x4000
#define V3D_EN_SW_DP_ENCODER0_P0_FLDMASK_POS                            14
#define V3D_EN_SW_DP_ENCODER0_P0_FLDMASK_LEN                            1

#define V3D_LR_HW_SWAP_DP_ENCODER0_P0_FLDMASK                           0x8000
#define V3D_LR_HW_SWAP_DP_ENCODER0_P0_FLDMASK_POS                       15
#define V3D_LR_HW_SWAP_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define REG_303C_DP_ENCODER0_P0              (0x303C)
#define SRAM_START_READ_THRD_DP_ENCODER0_P0_FLDMASK                     0x3f
#define SRAM_START_READ_THRD_DP_ENCODER0_P0_FLDMASK_POS                 0
#define SRAM_START_READ_THRD_DP_ENCODER0_P0_FLDMASK_LEN                 6

#define VIDEO_COLOR_DEPTH_DP_ENCODER0_P0_FLDMASK                        0x700
#define VIDEO_COLOR_DEPTH_DP_ENCODER0_P0_FLDMASK_POS                    8
#define VIDEO_COLOR_DEPTH_DP_ENCODER0_P0_FLDMASK_LEN                    3

#define PIXEL_ENCODE_FORMAT_DP_ENCODER0_P0_FLDMASK                      0x7000
#define PIXEL_ENCODE_FORMAT_DP_ENCODER0_P0_FLDMASK_POS                  12
#define PIXEL_ENCODE_FORMAT_DP_ENCODER0_P0_FLDMASK_LEN                  3

#define VIDEO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK                          0x8000
#define VIDEO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK_POS                      15
#define VIDEO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK_LEN                      1

#define REG_3040_DP_ENCODER0_P0              (0x3040)
#define SDP_DOWN_CNT_INIT_DP_ENCODER0_P0_FLDMASK                        0xfff
#define SDP_DOWN_CNT_INIT_DP_ENCODER0_P0_FLDMASK_POS                    0
#define SDP_DOWN_CNT_INIT_DP_ENCODER0_P0_FLDMASK_LEN                    12

#define AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK                            0x1000
#define AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK_POS                        12
#define AUDIO_32CH_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK                           0x2000
#define AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK_POS                       13
#define AUDIO_32CH_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK                            0x4000
#define AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK_POS                        14
#define AUDIO_16CH_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK                           0x8000
#define AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK_POS                       15
#define AUDIO_16CH_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define REG_3044_DP_ENCODER0_P0              (0x3044)
#define VIDEO_N_CODE_0_DP_ENCODER0_P0_FLDMASK                           0xffff
#define VIDEO_N_CODE_0_DP_ENCODER0_P0_FLDMASK_POS                       0
#define VIDEO_N_CODE_0_DP_ENCODER0_P0_FLDMASK_LEN                       16

#define REG_3048_DP_ENCODER0_P0              (0x3048)
#define VIDEO_N_CODE_1_DP_ENCODER0_P0_FLDMASK                           0xff
#define VIDEO_N_CODE_1_DP_ENCODER0_P0_FLDMASK_POS                       0
#define VIDEO_N_CODE_1_DP_ENCODER0_P0_FLDMASK_LEN                       8

#define REG_304C_DP_ENCODER0_P0              (0x304C)
#define VIDEO_SRAM_MODE_DP_ENCODER0_P0_FLDMASK                          0x3
#define VIDEO_SRAM_MODE_DP_ENCODER0_P0_FLDMASK_POS                      0
#define VIDEO_SRAM_MODE_DP_ENCODER0_P0_FLDMASK_LEN                      2

#define VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK                          0x4
#define VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK_POS                      2
#define VBID_VIDEO_MUTE_DP_ENCODER0_P0_FLDMASK_LEN                      1

#define VBID_VIDEO_MUTE_IDLE_PATTERN_SYNC_EN_DP_ENCODER0_P0_FLDMASK     0x8
#define VBID_VIDEO_MUTE_IDLE_PATTERN_SYNC_EN_DP_ENCODER0_P0_FLDMASK_POS 3
#define VBID_VIDEO_MUTE_IDLE_PATTERN_SYNC_EN_DP_ENCODER0_P0_FLDMASK_LEN 1

#define HDCP_SYNC_SEL_DP_ENCODER0_P0_FLDMASK                              0x10
#define HDCP_SYNC_SEL_DP_ENCODER0_P0_FLDMASK_POS                          4
#define HDCP_SYNC_SEL_DP_ENCODER0_P0_FLDMASK_LEN                          1

#define HDCP_SYNC_SW_DP_ENCODER0_P0_FLDMASK                               0x20
#define HDCP_SYNC_SW_DP_ENCODER0_P0_FLDMASK_POS                           5
#define HDCP_SYNC_SW_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define SDP_VSYNC_RISING_MASK_DP_ENCODER0_P0_FLDMASK                      0x100
#define SDP_VSYNC_RISING_MASK_DP_ENCODER0_P0_FLDMASK_POS                  8
#define SDP_VSYNC_RISING_MASK_DP_ENCODER0_P0_FLDMASK_LEN                  1

#define REG_3050_DP_ENCODER0_P0              (0x3050)
#define VIDEO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK                      0xffff
#define VIDEO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK_POS                  0
#define VIDEO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK_LEN                  16

#define REG_3054_DP_ENCODER0_P0              (0x3054)
#define VIDEO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK                      0xff
#define VIDEO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK_POS                  0
#define VIDEO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK_LEN                  8

#define REG_3058_DP_ENCODER0_P0              (0x3058)
#define AUDIO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK                      0xffff
#define AUDIO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK_POS                  0
#define AUDIO_N_CODE_MN_GEN_0_DP_ENCODER0_P0_FLDMASK_LEN                  16

#define REG_305C_DP_ENCODER0_P0              (0x305C)
#define AUDIO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK                      0xff
#define AUDIO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK_POS                  0
#define AUDIO_N_CODE_MN_GEN_1_DP_ENCODER0_P0_FLDMASK_LEN                  8

#define REG_3060_DP_ENCODER0_P0              (0x3060)
#define NUM_INTERLACE_FRAME_DP_ENCODER0_P0_FLDMASK                        0x7
#define NUM_INTERLACE_FRAME_DP_ENCODER0_P0_FLDMASK_POS                    0
#define NUM_INTERLACE_FRAME_DP_ENCODER0_P0_FLDMASK_LEN                    3

#define INTERLACE_DET_EVEN_EN_DP_ENCODER0_P0_FLDMASK                      0x8
#define INTERLACE_DET_EVEN_EN_DP_ENCODER0_P0_FLDMASK_POS                  3
#define INTERLACE_DET_EVEN_EN_DP_ENCODER0_P0_FLDMASK_LEN                  1

#define FIELD_DETECT_EN_DP_ENCODER0_P0_FLDMASK                            0x10
#define FIELD_DETECT_EN_DP_ENCODER0_P0_FLDMASK_POS                        4
#define FIELD_DETECT_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define FIELD_DETECT_UPDATE_THRD_DP_ENCODER0_P0_FLDMASK                   0xff00
#define FIELD_DETECT_UPDATE_THRD_DP_ENCODER0_P0_FLDMASK_POS               8
#define FIELD_DETECT_UPDATE_THRD_DP_ENCODER0_P0_FLDMASK_LEN               8

#define REG_3064_DP_ENCODER0_P0              (0x3064)
#define HDE_NUM_LAST_DP_ENCODER0_P0_FLDMASK                               0xffff
#define HDE_NUM_LAST_DP_ENCODER0_P0_FLDMASK_POS                           0
#define HDE_NUM_LAST_DP_ENCODER0_P0_FLDMASK_LEN                           16

#define REG_3088_DP_ENCODER0_P0              (0x3088)
#define AUDIO_DETECT_EN_DP_ENCODER0_P0_FLDMASK                            0x20
#define AUDIO_DETECT_EN_DP_ENCODER0_P0_FLDMASK_POS                        5
#define AUDIO_DETECT_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define AU_EN_DP_ENCODER0_P0_FLDMASK                                      0x40
#define AU_EN_DP_ENCODER0_P0_FLDMASK_POS                                  6
#define AU_EN_DP_ENCODER0_P0_FLDMASK_LEN                                  1

#define AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK                               0x80
#define AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK_POS                           7
#define AUDIO_8CH_EN_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK                              0x100
#define AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK_POS                          8
#define AUDIO_8CH_SEL_DP_ENCODER0_P0_FLDMASK_LEN                          1

#define AU_GEN_EN_DP_ENCODER0_P0_FLDMASK                                  0x200
#define AU_GEN_EN_DP_ENCODER0_P0_FLDMASK_POS                              9
#define AU_GEN_EN_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK                            0x1000
#define AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK_POS                        12
#define AUDIO_MN_GEN_EN_DP_ENCODER0_P0_FLDMASK_LEN                        1

#define DIS_ASP_DP_ENCODER0_P0_FLDMASK                                    0x2000
#define DIS_ASP_DP_ENCODER0_P0_FLDMASK_POS                                13
#define DIS_ASP_DP_ENCODER0_P0_FLDMASK_LEN                                1

#define AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK                               0x4000
#define AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_POS                           14
#define AUDIO_2CH_EN_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK                              0x8000
#define AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK_POS                          15
#define AUDIO_2CH_SEL_DP_ENCODER0_P0_FLDMASK_LEN                          1

#define REG_308C_DP_ENCODER0_P0              (0x308C)
#define CH_STATUS_0_DP_ENCODER0_P0_FLDMASK                                0xffff
#define CH_STATUS_0_DP_ENCODER0_P0_FLDMASK_POS                            0
#define CH_STATUS_0_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3090_DP_ENCODER0_P0              (0x3090)
#define CH_STATUS_1_DP_ENCODER0_P0_FLDMASK                                0xffff
#define CH_STATUS_1_DP_ENCODER0_P0_FLDMASK_POS                            0
#define CH_STATUS_1_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_3094_DP_ENCODER0_P0              (0x3094)
#define CH_STATUS_2_DP_ENCODER0_P0_FLDMASK                                0xff
#define CH_STATUS_2_DP_ENCODER0_P0_FLDMASK_POS                            0
#define CH_STATUS_2_DP_ENCODER0_P0_FLDMASK_LEN                            8

#define REG_3098_DP_ENCODER0_P0              (0x3098)
#define USER_DATA_0_DP_ENCODER0_P0_FLDMASK                                0xffff
#define USER_DATA_0_DP_ENCODER0_P0_FLDMASK_POS                            0
#define USER_DATA_0_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_309C_DP_ENCODER0_P0              (0x309C)
#define USER_DATA_1_DP_ENCODER0_P0_FLDMASK                                0xffff
#define USER_DATA_1_DP_ENCODER0_P0_FLDMASK_POS                            0
#define USER_DATA_1_DP_ENCODER0_P0_FLDMASK_LEN                            16

#define REG_30A0_DP_ENCODER0_P0              (0x30A0)
#define USER_DATA_2_DP_ENCODER0_P0_FLDMASK                                0xff
#define USER_DATA_2_DP_ENCODER0_P0_FLDMASK_POS                            0
#define USER_DATA_2_DP_ENCODER0_P0_FLDMASK_LEN                            8

#define VSC_EXT_VESA_CFG_DP_ENCODER0_P0_FLDMASK                           0xf00
#define VSC_EXT_VESA_CFG_DP_ENCODER0_P0_FLDMASK_POS                       8
#define VSC_EXT_VESA_CFG_DP_ENCODER0_P0_FLDMASK_LEN                       4

#define VSC_EXT_CEA_CFG_DP_ENCODER0_P0_FLDMASK                            0xf000
#define VSC_EXT_CEA_CFG_DP_ENCODER0_P0_FLDMASK_POS                        12
#define VSC_EXT_CEA_CFG_DP_ENCODER0_P0_FLDMASK_LEN                        4

#define REG_30A4_DP_ENCODER0_P0              (0x30A4)
#define AU_TS_CFG_DP_ENCODER0_P0_FLDMASK                                  0xff
#define AU_TS_CFG_DP_ENCODER0_P0_FLDMASK_POS                              0
#define AU_TS_CFG_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define AVI_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define AVI_CFG_DP_ENCODER0_P0_FLDMASK_POS                                8
#define AVI_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30A8_DP_ENCODER0_P0              (0x30A8)
#define AUI_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff
#define AUI_CFG_DP_ENCODER0_P0_FLDMASK_POS                                0
#define AUI_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define SPD_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define SPD_CFG_DP_ENCODER0_P0_FLDMASK_POS                                8
#define SPD_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30AC_DP_ENCODER0_P0              (0x30AC)
#define MPEG_CFG_DP_ENCODER0_P0_FLDMASK                                   0xff
#define MPEG_CFG_DP_ENCODER0_P0_FLDMASK_POS                               0
#define MPEG_CFG_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define NTSC_CFG_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define NTSC_CFG_DP_ENCODER0_P0_FLDMASK_POS                               8
#define NTSC_CFG_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_30B0_DP_ENCODER0_P0              (0x30B0)
#define VSP_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSP_CFG_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSP_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define EXT_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define EXT_CFG_DP_ENCODER0_P0_FLDMASK_POS                                8
#define EXT_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30B4_DP_ENCODER0_P0              (0x30B4)
#define ACM_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ACM_CFG_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ACM_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ISRC_CFG_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define ISRC_CFG_DP_ENCODER0_P0_FLDMASK_POS                               8
#define ISRC_CFG_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_30B8_DP_ENCODER0_P0              (0x30B8)
#define VSC_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSC_CFG_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSC_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define MSA_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define MSA_CFG_DP_ENCODER0_P0_FLDMASK_POS                                8
#define MSA_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30BC_DP_ENCODER0_P0              (0x30BC)
#define ISRC_CONT_DP_ENCODER0_P0_FLDMASK                                  0x1
#define ISRC_CONT_DP_ENCODER0_P0_FLDMASK_POS                              0
#define ISRC_CONT_DP_ENCODER0_P0_FLDMASK_LEN                              1

#define MSA_BY_SDP_DP_ENCODER0_P0_FLDMASK                                 0x2
#define MSA_BY_SDP_DP_ENCODER0_P0_FLDMASK_POS                             1
#define MSA_BY_SDP_DP_ENCODER0_P0_FLDMASK_LEN                             1

#define SDP_EN_DP_ENCODER0_P0_FLDMASK                                     0x4
#define SDP_EN_DP_ENCODER0_P0_FLDMASK_POS                                 2
#define SDP_EN_DP_ENCODER0_P0_FLDMASK_LEN                                 1

#define NIBBLE_INTERLEAVER_EN_DP_ENCODER0_P0_FLDMASK                      0x8
#define NIBBLE_INTERLEAVER_EN_DP_ENCODER0_P0_FLDMASK_POS                  3
#define NIBBLE_INTERLEAVER_EN_DP_ENCODER0_P0_FLDMASK_LEN                  1

#define ECC_EN_DP_ENCODER0_P0_FLDMASK                                     0x10
#define ECC_EN_DP_ENCODER0_P0_FLDMASK_POS                                 4
#define ECC_EN_DP_ENCODER0_P0_FLDMASK_LEN                                 1

#define ASP_MIN_PL_SIZE_DP_ENCODER0_P0_FLDMASK                            0x60
#define ASP_MIN_PL_SIZE_DP_ENCODER0_P0_FLDMASK_POS                        5
#define ASP_MIN_PL_SIZE_DP_ENCODER0_P0_FLDMASK_LEN                        2

#define AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK                  0x700
#define AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_POS              8
#define AUDIO_M_CODE_MULT_DIV_SEL_DP_ENCODER0_P0_FLDMASK_LEN              3

#define AUDIO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK                           0x4000
#define AUDIO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK_POS                       14
#define AUDIO_M_CODE_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK                               0x8000
#define ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK_POS                           15
#define ASP_HB23_SEL_DP_ENCODER0_P0_FLDMASK_LEN                           1

#define REG_30C0_DP_ENCODER0_P0              (0x30C0)
#define AU_TS_HB0_DP_ENCODER0_P0_FLDMASK                                  0xff
#define AU_TS_HB0_DP_ENCODER0_P0_FLDMASK_POS                              0
#define AU_TS_HB0_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define AU_TS_HB1_DP_ENCODER0_P0_FLDMASK                                  0xff00
#define AU_TS_HB1_DP_ENCODER0_P0_FLDMASK_POS                              8
#define AU_TS_HB1_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define REG_30C4_DP_ENCODER0_P0              (0x30C4)
#define AU_TS_HB2_DP_ENCODER0_P0_FLDMASK                                  0xff
#define AU_TS_HB2_DP_ENCODER0_P0_FLDMASK_POS                              0
#define AU_TS_HB2_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define AU_TS_HB3_DP_ENCODER0_P0_FLDMASK                                  0xff00
#define AU_TS_HB3_DP_ENCODER0_P0_FLDMASK_POS                              8
#define AU_TS_HB3_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define REG_30C8_DP_ENCODER0_P0              (0x30C8)
#define AUDIO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK                          0xffff
#define AUDIO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK_POS                      0
#define AUDIO_M_CODE_SW_0_DP_ENCODER0_P0_FLDMASK_LEN                      16

#define REG_30CC_DP_ENCODER0_P0              (0x30CC)
#define AUDIO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK                          0xff
#define AUDIO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK_POS                      0
#define AUDIO_M_CODE_SW_1_DP_ENCODER0_P0_FLDMASK_LEN                      8

#define REG_30D0_DP_ENCODER0_P0              (0x30D0)
#define AUDIO_N_CODE_0_DP_ENCODER0_P0_FLDMASK                             0xffff
#define AUDIO_N_CODE_0_DP_ENCODER0_P0_FLDMASK_POS                         0
#define AUDIO_N_CODE_0_DP_ENCODER0_P0_FLDMASK_LEN                         16

#define REG_30D4_DP_ENCODER0_P0              (0x30D4)
#define AUDIO_N_CODE_1_DP_ENCODER0_P0_FLDMASK                             0xff
#define AUDIO_N_CODE_1_DP_ENCODER0_P0_FLDMASK_POS                         0
#define AUDIO_N_CODE_1_DP_ENCODER0_P0_FLDMASK_LEN                         8

#define REG_30D8_DP_ENCODER0_P0              (0x30D8)
#define ACM_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ACM_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ACM_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ACM_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ACM_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ACM_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30DC_DP_ENCODER0_P0              (0x30DC)
#define ACM_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ACM_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ACM_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ACM_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ACM_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ACM_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30E0_DP_ENCODER0_P0              (0x30E0)
#define ISRC_HB0_DP_ENCODER0_P0_FLDMASK                                   0xff
#define ISRC_HB0_DP_ENCODER0_P0_FLDMASK_POS                               0
#define ISRC_HB0_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define ISRC_HB1_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define ISRC_HB1_DP_ENCODER0_P0_FLDMASK_POS                               8
#define ISRC_HB1_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_30E4_DP_ENCODER0_P0              (0x30E4)
#define ISRC_HB2_DP_ENCODER0_P0_FLDMASK                                   0xff
#define ISRC_HB2_DP_ENCODER0_P0_FLDMASK_POS                               0
#define ISRC_HB2_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define ISRC0_HB3_DP_ENCODER0_P0_FLDMASK                                  0xff00
#define ISRC0_HB3_DP_ENCODER0_P0_FLDMASK_POS                              8
#define ISRC0_HB3_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define REG_30E8_DP_ENCODER0_P0              (0x30E8)
#define AVI_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define AVI_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define AVI_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define AVI_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define AVI_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define AVI_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30EC_DP_ENCODER0_P0              (0x30EC)
#define AVI_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define AVI_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define AVI_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define AVI_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define AVI_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define AVI_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30F0_DP_ENCODER0_P0              (0x30F0)
#define AUI_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define AUI_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define AUI_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define AUI_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define AUI_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define AUI_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30F4_DP_ENCODER0_P0              (0x30F4)
#define AUI_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define AUI_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define AUI_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define AUI_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define AUI_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define AUI_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30F8_DP_ENCODER0_P0              (0x30F8)
#define SPD_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define SPD_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define SPD_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define SPD_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define SPD_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define SPD_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_30FC_DP_ENCODER0_P0              (0x30FC)
#define SPD_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define SPD_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define SPD_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define SPD_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define SPD_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define SPD_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3100_DP_ENCODER0_P0              (0x3100)
#define MPEG_HB0_DP_ENCODER0_P0_FLDMASK                                   0xff
#define MPEG_HB0_DP_ENCODER0_P0_FLDMASK_POS                               0
#define MPEG_HB0_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define MPEG_HB1_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define MPEG_HB1_DP_ENCODER0_P0_FLDMASK_POS                               8
#define MPEG_HB1_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_3104_DP_ENCODER0_P0              (0x3104)
#define MPEG_HB2_DP_ENCODER0_P0_FLDMASK                                   0xff
#define MPEG_HB2_DP_ENCODER0_P0_FLDMASK_POS                               0
#define MPEG_HB2_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define MPEG_HB3_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define MPEG_HB3_DP_ENCODER0_P0_FLDMASK_POS                               8
#define MPEG_HB3_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_3108_DP_ENCODER0_P0              (0x3108)
#define NTSC_HB0_DP_ENCODER0_P0_FLDMASK                                   0xff
#define NTSC_HB0_DP_ENCODER0_P0_FLDMASK_POS                               0
#define NTSC_HB0_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define NTSC_HB1_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define NTSC_HB1_DP_ENCODER0_P0_FLDMASK_POS                               8
#define NTSC_HB1_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_310C_DP_ENCODER0_P0              (0x310C)
#define NTSC_HB2_DP_ENCODER0_P0_FLDMASK                                   0xff
#define NTSC_HB2_DP_ENCODER0_P0_FLDMASK_POS                               0
#define NTSC_HB2_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define NTSC_HB3_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define NTSC_HB3_DP_ENCODER0_P0_FLDMASK_POS                               8
#define NTSC_HB3_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_3110_DP_ENCODER0_P0              (0x3110)
#define VSP_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSP_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSP_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define VSP_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define VSP_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define VSP_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3114_DP_ENCODER0_P0              (0x3114)
#define VSP_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSP_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSP_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define VSP_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define VSP_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define VSP_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3118_DP_ENCODER0_P0              (0x3118)
#define VSC_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSC_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSC_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define VSC_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define VSC_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define VSC_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_311C_DP_ENCODER0_P0              (0x311C)
#define VSC_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define VSC_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define VSC_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define VSC_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define VSC_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define VSC_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3120_DP_ENCODER0_P0              (0x3120)
#define EXT_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define EXT_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define EXT_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define EXT_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define EXT_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define EXT_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3124_DP_ENCODER0_P0              (0x3124)
#define EXT_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define EXT_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define EXT_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define EXT_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define EXT_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define EXT_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3128_DP_ENCODER0_P0              (0x3128)
#define ASP_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ASP_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ASP_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ASP_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ASP_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ASP_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_312C_DP_ENCODER0_P0              (0x312C)
#define ASP_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ASP_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ASP_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ASP_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ASP_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ASP_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3130_DP_ENCODER0_P0              (0x3130)
#define PPS_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define PPS_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define PPS_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define PPS_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define PPS_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define PPS_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3134_DP_ENCODER0_P0              (0x3134)
#define PPS_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define PPS_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define PPS_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define PPS_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define PPS_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define PPS_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_3138_DP_ENCODER0_P0              (0x3138)
#define HDR0_HB0_DP_ENCODER0_P0_FLDMASK                                   0xff
#define HDR0_HB0_DP_ENCODER0_P0_FLDMASK_POS                               0
#define HDR0_HB0_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define HDR0_HB1_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define HDR0_HB1_DP_ENCODER0_P0_FLDMASK_POS                               8
#define HDR0_HB1_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_313C_DP_ENCODER0_P0              (0x313C)
#define HDR0_HB2_DP_ENCODER0_P0_FLDMASK                                   0xff
#define HDR0_HB2_DP_ENCODER0_P0_FLDMASK_POS                               0
#define HDR0_HB2_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define HDR0_HB3_DP_ENCODER0_P0_FLDMASK                                   0xff00
#define HDR0_HB3_DP_ENCODER0_P0_FLDMASK_POS                               8
#define HDR0_HB3_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define REG_3140_DP_ENCODER0_P0              (0x3140)
#define PGEN_CURSOR_V_DP_ENCODER0_P0_FLDMASK                              0x1fff
#define PGEN_CURSOR_V_DP_ENCODER0_P0_FLDMASK_POS                          0
#define PGEN_CURSOR_V_DP_ENCODER0_P0_FLDMASK_LEN                          13

#define PGEN_TG_SEL_DP_ENCODER0_P0_FLDMASK                                0x2000
#define PGEN_TG_SEL_DP_ENCODER0_P0_FLDMASK_POS                            13
#define PGEN_TG_SEL_DP_ENCODER0_P0_FLDMASK_LEN                            1

#define PGEN_PG_SEL_DP_ENCODER0_P0_FLDMASK                                0x4000
#define PGEN_PG_SEL_DP_ENCODER0_P0_FLDMASK_POS                            14
#define PGEN_PG_SEL_DP_ENCODER0_P0_FLDMASK_LEN                            1

#define PGEN_CURSOR_EN_DP_ENCODER0_P0_FLDMASK                             0x8000
#define PGEN_CURSOR_EN_DP_ENCODER0_P0_FLDMASK_POS                         15
#define PGEN_CURSOR_EN_DP_ENCODER0_P0_FLDMASK_LEN                         1

#define REG_3144_DP_ENCODER0_P0              (0x3144)
#define PGEN_CURSOR_H_DP_ENCODER0_P0_FLDMASK                              0x3fff
#define PGEN_CURSOR_H_DP_ENCODER0_P0_FLDMASK_POS                          0
#define PGEN_CURSOR_H_DP_ENCODER0_P0_FLDMASK_LEN                          14

#define REG_3148_DP_ENCODER0_P0              (0x3148)
#define PGEN_CURSOR_RGB_COLOR_CODE_0_DP_ENCODER0_P0_FLDMASK               0xffff
#define PGEN_CURSOR_RGB_COLOR_CODE_0_DP_ENCODER0_P0_FLDMASK_POS           0
#define PGEN_CURSOR_RGB_COLOR_CODE_0_DP_ENCODER0_P0_FLDMASK_LEN           16

#define REG_314C_DP_ENCODER0_P0              (0x314C)
#define PGEN_CURSOR_RGB_COLOR_CODE_1_DP_ENCODER0_P0_FLDMASK               0xffff
#define PGEN_CURSOR_RGB_COLOR_CODE_1_DP_ENCODER0_P0_FLDMASK_POS           0
#define PGEN_CURSOR_RGB_COLOR_CODE_1_DP_ENCODER0_P0_FLDMASK_LEN           16

#define REG_3150_DP_ENCODER0_P0              (0x3150)
#define PGEN_CURSOR_RGB_COLOR_CODE_2_DP_ENCODER0_P0_FLDMASK               0xf
#define PGEN_CURSOR_RGB_COLOR_CODE_2_DP_ENCODER0_P0_FLDMASK_POS           0
#define PGEN_CURSOR_RGB_COLOR_CODE_2_DP_ENCODER0_P0_FLDMASK_LEN           4

#define REG_3154_DP_ENCODER0_P0              (0x3154)
#define PGEN_HTOTAL_DP_ENCODER0_P0_FLDMASK                                0x3fff
#define PGEN_HTOTAL_DP_ENCODER0_P0_FLDMASK_POS                            0
#define PGEN_HTOTAL_DP_ENCODER0_P0_FLDMASK_LEN                            14

#define REG_3158_DP_ENCODER0_P0              (0x3158)
#define PGEN_HSYNC_RISING_DP_ENCODER0_P0_FLDMASK                          0x3fff
#define PGEN_HSYNC_RISING_DP_ENCODER0_P0_FLDMASK_POS                      0
#define PGEN_HSYNC_RISING_DP_ENCODER0_P0_FLDMASK_LEN                      14

#define REG_315C_DP_ENCODER0_P0              (0x315C)
#define PGEN_HSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK                     0x3fff
#define PGEN_HSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_HSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK_LEN                 14

#define REG_3160_DP_ENCODER0_P0              (0x3160)
#define PGEN_HFDE_START_DP_ENCODER0_P0_FLDMASK                            0x3fff
#define PGEN_HFDE_START_DP_ENCODER0_P0_FLDMASK_POS                        0
#define PGEN_HFDE_START_DP_ENCODER0_P0_FLDMASK_LEN                        14

#define REG_3164_DP_ENCODER0_P0              (0x3164)
#define PGEN_HFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK                     0x3fff
#define PGEN_HFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_HFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK_LEN                 14

#define REG_3168_DP_ENCODER0_P0              (0x3168)
#define PGEN_VTOTAL_DP_ENCODER0_P0_FLDMASK                                0x1fff
#define PGEN_VTOTAL_DP_ENCODER0_P0_FLDMASK_POS                            0
#define PGEN_VTOTAL_DP_ENCODER0_P0_FLDMASK_LEN                            13

#define REG_316C_DP_ENCODER0_P0              (0x316C)
#define PGEN_VSYNC_RISING_DP_ENCODER0_P0_FLDMASK                          0x1fff
#define PGEN_VSYNC_RISING_DP_ENCODER0_P0_FLDMASK_POS                      0
#define PGEN_VSYNC_RISING_DP_ENCODER0_P0_FLDMASK_LEN                      13

#define REG_3170_DP_ENCODER0_P0              (0x3170)
#define PGEN_VSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK                     0x1fff
#define PGEN_VSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_VSYNC_PULSE_WIDTH_DP_ENCODER0_P0_FLDMASK_LEN                 13

#define REG_3174_DP_ENCODER0_P0              (0x3174)
#define PGEN_VFDE_START_DP_ENCODER0_P0_FLDMASK                            0x1fff
#define PGEN_VFDE_START_DP_ENCODER0_P0_FLDMASK_POS                        0
#define PGEN_VFDE_START_DP_ENCODER0_P0_FLDMASK_LEN                        13

#define REG_3178_DP_ENCODER0_P0              (0x3178)
#define PGEN_VFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK                     0x1fff
#define PGEN_VFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_VFDE_ACTIVE_WIDTH_DP_ENCODER0_P0_FLDMASK_LEN                 13

#define REG_317C_DP_ENCODER0_P0              (0x317C)
#define PGEN_PAT_BASE_PIXEL_0_DP_ENCODER0_P0_FLDMASK                      0xfff
#define PGEN_PAT_BASE_PIXEL_0_DP_ENCODER0_P0_FLDMASK_POS                  0
#define PGEN_PAT_BASE_PIXEL_0_DP_ENCODER0_P0_FLDMASK_LEN                  12

#define REG_3180_DP_ENCODER0_P0              (0x3180)
#define PGEN_PAT_BASE_PIXEL_1_DP_ENCODER0_P0_FLDMASK                      0xfff
#define PGEN_PAT_BASE_PIXEL_1_DP_ENCODER0_P0_FLDMASK_POS                  0
#define PGEN_PAT_BASE_PIXEL_1_DP_ENCODER0_P0_FLDMASK_LEN                  12

#define REG_3184_DP_ENCODER0_P0              (0x3184)
#define PGEN_PAT_BASE_PIXEL_2_DP_ENCODER0_P0_FLDMASK                      0xfff
#define PGEN_PAT_BASE_PIXEL_2_DP_ENCODER0_P0_FLDMASK_POS                  0
#define PGEN_PAT_BASE_PIXEL_2_DP_ENCODER0_P0_FLDMASK_LEN                  12

#define REG_3188_DP_ENCODER0_P0              (0x3188)
#define PGEN_INITIAL_H_CNT_DP_ENCODER0_P0_FLDMASK                         0x3fff
#define PGEN_INITIAL_H_CNT_DP_ENCODER0_P0_FLDMASK_POS                     0
#define PGEN_INITIAL_H_CNT_DP_ENCODER0_P0_FLDMASK_LEN                     14

#define REG_318C_DP_ENCODER0_P0              (0x318C)
#define PGEN_INITIAL_V_CNT_DP_ENCODER0_P0_FLDMASK                         0x1fff
#define PGEN_INITIAL_V_CNT_DP_ENCODER0_P0_FLDMASK_POS                     0
#define PGEN_INITIAL_V_CNT_DP_ENCODER0_P0_FLDMASK_LEN                     13

#define REG_3190_DP_ENCODER0_P0              (0x3190)
#define PGEN_INITIAL_CB_SEL_DP_ENCODER0_P0_FLDMASK                        0x7
#define PGEN_INITIAL_CB_SEL_DP_ENCODER0_P0_FLDMASK_POS                    0
#define PGEN_INITIAL_CB_SEL_DP_ENCODER0_P0_FLDMASK_LEN                    3

#define PGEN_FRAME_8K4K_MODE_EN_DP_ENCODER0_P0_FLDMASK                    0x10
#define PGEN_FRAME_8K4K_MODE_EN_DP_ENCODER0_P0_FLDMASK_POS                4
#define PGEN_FRAME_8K4K_MODE_EN_DP_ENCODER0_P0_FLDMASK_LEN                1

#define PGEN_FRAME_8K4K_MODE_SET_DP_ENCODER0_P0_FLDMASK                   0x20
#define PGEN_FRAME_8K4K_MODE_SET_DP_ENCODER0_P0_FLDMASK_POS               5
#define PGEN_FRAME_8K4K_MODE_SET_DP_ENCODER0_P0_FLDMASK_LEN               1

#define PGEN_INITIAL_H_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK                   0x40
#define PGEN_INITIAL_H_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK_POS               6
#define PGEN_INITIAL_H_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK_LEN               1

#define PGEN_INITIAL_V_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK                   0x80
#define PGEN_INITIAL_V_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK_POS               7
#define PGEN_INITIAL_V_GRAD_FLAG_DP_ENCODER0_P0_FLDMASK_LEN               1

#define PGEN_FRAME_END_H_EN_DP_ENCODER0_P0_FLDMASK                        0x100
#define PGEN_FRAME_END_H_EN_DP_ENCODER0_P0_FLDMASK_POS                    8
#define PGEN_FRAME_END_H_EN_DP_ENCODER0_P0_FLDMASK_LEN                    1

#define PGEN_FRAME_END_V_EN_DP_ENCODER0_P0_FLDMASK                        0x200
#define PGEN_FRAME_END_V_EN_DP_ENCODER0_P0_FLDMASK_POS                    9
#define PGEN_FRAME_END_V_EN_DP_ENCODER0_P0_FLDMASK_LEN                    1

#define REG_3194_DP_ENCODER0_P0              (0x3194)
#define PGEN_PAT_EXTRA_PIXEL_0_DP_ENCODER0_P0_FLDMASK                     0xfff
#define PGEN_PAT_EXTRA_PIXEL_0_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_PAT_EXTRA_PIXEL_0_DP_ENCODER0_P0_FLDMASK_LEN                 12

#define REG_3198_DP_ENCODER0_P0              (0x3198)
#define PGEN_PAT_EXTRA_PIXEL_1_DP_ENCODER0_P0_FLDMASK                     0xfff
#define PGEN_PAT_EXTRA_PIXEL_1_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_PAT_EXTRA_PIXEL_1_DP_ENCODER0_P0_FLDMASK_LEN                 12

#define REG_319C_DP_ENCODER0_P0              (0x319C)
#define PGEN_PAT_EXTRA_PIXEL_2_DP_ENCODER0_P0_FLDMASK                     0xfff
#define PGEN_PAT_EXTRA_PIXEL_2_DP_ENCODER0_P0_FLDMASK_POS                 0
#define PGEN_PAT_EXTRA_PIXEL_2_DP_ENCODER0_P0_FLDMASK_LEN                 12

#define REG_31A0_DP_ENCODER0_P0              (0x31A0)
#define PGEN_PAT_INCREMENT_0_DP_ENCODER0_P0_FLDMASK                       0xffff
#define PGEN_PAT_INCREMENT_0_DP_ENCODER0_P0_FLDMASK_POS                   0
#define PGEN_PAT_INCREMENT_0_DP_ENCODER0_P0_FLDMASK_LEN                   16

#define REG_31A4_DP_ENCODER0_P0              (0x31A4)
#define PGEN_PAT_INCREMENT_1_DP_ENCODER0_P0_FLDMASK                       0x1
#define PGEN_PAT_INCREMENT_1_DP_ENCODER0_P0_FLDMASK_POS                   0
#define PGEN_PAT_INCREMENT_1_DP_ENCODER0_P0_FLDMASK_LEN                   1

#define REG_31A8_DP_ENCODER0_P0              (0x31A8)
#define PGEN_PAT_HWIDTH_DP_ENCODER0_P0_FLDMASK                            0x3fff
#define PGEN_PAT_HWIDTH_DP_ENCODER0_P0_FLDMASK_POS                        0
#define PGEN_PAT_HWIDTH_DP_ENCODER0_P0_FLDMASK_LEN                        14

#define REG_31AC_DP_ENCODER0_P0              (0x31AC)
#define PGEN_PAT_VWIDTH_DP_ENCODER0_P0_FLDMASK                            0x1fff
#define PGEN_PAT_VWIDTH_DP_ENCODER0_P0_FLDMASK_POS                        0
#define PGEN_PAT_VWIDTH_DP_ENCODER0_P0_FLDMASK_LEN                        13

#define REG_31B0_DP_ENCODER0_P0              (0x31B0)
#define PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK                        0x7
#define PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_POS                    0
#define PGEN_PAT_RGB_ENABLE_DP_ENCODER0_P0_FLDMASK_LEN                    3

#define PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK                           0x70
#define PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_POS                       4
#define PGEN_PATTERN_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       3

#define PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK                         0x80
#define PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_POS                     7
#define PGEN_PAT_DIRECTION_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define PGEN_PAT_GRADIENT_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK              0x100
#define PGEN_PAT_GRADIENT_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK_POS          8
#define PGEN_PAT_GRADIENT_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK_LEN          1

#define PGEN_PAT_COLOR_BAR_GRADIENT_EN_DP_ENCODER0_P0_FLDMASK             0x200
#define PGEN_PAT_COLOR_BAR_GRADIENT_EN_DP_ENCODER0_P0_FLDMASK_POS         9
#define PGEN_PAT_COLOR_BAR_GRADIENT_EN_DP_ENCODER0_P0_FLDMASK_LEN         1

#define PGEN_PAT_CHESSBOARD_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK            0x400
#define PGEN_PAT_CHESSBOARD_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK_POS        10
#define PGEN_PAT_CHESSBOARD_NORMAL_MODE_DP_ENCODER0_P0_FLDMASK_LEN        1

#define PGEN_PAT_EXCHANGE_DP_ENCODER0_P0_FLDMASK                          0x800
#define PGEN_PAT_EXCHANGE_DP_ENCODER0_P0_FLDMASK_POS                      11
#define PGEN_PAT_EXCHANGE_DP_ENCODER0_P0_FLDMASK_LEN                      1

#define PGEN_PAT_RGB_SUB_PIXEL_MASK_DP_ENCODER0_P0_FLDMASK                0x1000
#define PGEN_PAT_RGB_SUB_PIXEL_MASK_DP_ENCODER0_P0_FLDMASK_POS            12
#define PGEN_PAT_RGB_SUB_PIXEL_MASK_DP_ENCODER0_P0_FLDMASK_LEN            1

#define REG_31B4_DP_ENCODER0_P0              (0x31B4)
#define PGEN_PAT_THICKNESS_DP_ENCODER0_P0_FLDMASK                         0xf
#define PGEN_PAT_THICKNESS_DP_ENCODER0_P0_FLDMASK_POS                     0
#define PGEN_PAT_THICKNESS_DP_ENCODER0_P0_FLDMASK_LEN                     4

#define REG_31C0_DP_ENCODER0_P0              (0x31C0)
#define VIDEO_MUTE_CNT_THRD_DP_ENCODER0_P0_FLDMASK                        0xfff
#define VIDEO_MUTE_CNT_THRD_DP_ENCODER0_P0_FLDMASK_POS                    0
#define VIDEO_MUTE_CNT_THRD_DP_ENCODER0_P0_FLDMASK_LEN                    12

#define REG_31C4_DP_ENCODER0_P0              (0x31C4)
#define PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK                         0x800
#define PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_POS                     11
#define PPS_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define MST_EN_DP_ENCODER0_P0_FLDMASK                                     0x1000
#define MST_EN_DP_ENCODER0_P0_FLDMASK_POS                                 12
#define MST_EN_DP_ENCODER0_P0_FLDMASK_LEN                                 1

#define DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK                              0x2000
#define DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK_POS                          13
#define DSC_BYPASS_EN_DP_ENCODER0_P0_FLDMASK_LEN                          1

#define VSC_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK                         0x4000
#define VSC_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_POS                     14
#define VSC_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define HDR0_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK                        0x8000
#define HDR0_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_POS                    15
#define HDR0_HW_BYPASS_MASK_DP_ENCODER0_P0_FLDMASK_LEN                    1

#define REG_31C8_DP_ENCODER0_P0              (0x31C8)
#define VSC_EXT_VESA_HB0_DP_ENCODER0_P0_FLDMASK                           0xff
#define VSC_EXT_VESA_HB0_DP_ENCODER0_P0_FLDMASK_POS                       0
#define VSC_EXT_VESA_HB0_DP_ENCODER0_P0_FLDMASK_LEN                       8

#define VSC_EXT_VESA_HB1_DP_ENCODER0_P0_FLDMASK                           0xff00
#define VSC_EXT_VESA_HB1_DP_ENCODER0_P0_FLDMASK_POS                       8
#define VSC_EXT_VESA_HB1_DP_ENCODER0_P0_FLDMASK_LEN                       8

#define REG_31CC_DP_ENCODER0_P0              (0x31CC)
#define VSC_EXT_VESA_HB2_DP_ENCODER0_P0_FLDMASK                           0xff
#define VSC_EXT_VESA_HB2_DP_ENCODER0_P0_FLDMASK_POS                       0
#define VSC_EXT_VESA_HB2_DP_ENCODER0_P0_FLDMASK_LEN                       8

#define VSC_EXT_VESA_HB3_DP_ENCODER0_P0_FLDMASK                           0xff00
#define VSC_EXT_VESA_HB3_DP_ENCODER0_P0_FLDMASK_POS                       8
#define VSC_EXT_VESA_HB3_DP_ENCODER0_P0_FLDMASK_LEN                       8

#define REG_31D0_DP_ENCODER0_P0              (0x31D0)
#define VSC_EXT_CEA_HB0_DP_ENCODER0_P0_FLDMASK                            0xff
#define VSC_EXT_CEA_HB0_DP_ENCODER0_P0_FLDMASK_POS                        0
#define VSC_EXT_CEA_HB0_DP_ENCODER0_P0_FLDMASK_LEN                        8

#define VSC_EXT_CEA_HB1_DP_ENCODER0_P0_FLDMASK                            0xff00
#define VSC_EXT_CEA_HB1_DP_ENCODER0_P0_FLDMASK_POS                        8
#define VSC_EXT_CEA_HB1_DP_ENCODER0_P0_FLDMASK_LEN                        8

#define REG_31D4_DP_ENCODER0_P0              (0x31D4)
#define VSC_EXT_CEA_HB2_DP_ENCODER0_P0_FLDMASK                            0xff
#define VSC_EXT_CEA_HB2_DP_ENCODER0_P0_FLDMASK_POS                        0
#define VSC_EXT_CEA_HB2_DP_ENCODER0_P0_FLDMASK_LEN                        8

#define VSC_EXT_CEA_HB3_DP_ENCODER0_P0_FLDMASK                            0xff00
#define VSC_EXT_CEA_HB3_DP_ENCODER0_P0_FLDMASK_POS                        8
#define VSC_EXT_CEA_HB3_DP_ENCODER0_P0_FLDMASK_LEN                        8

#define REG_31D8_DP_ENCODER0_P0              (0x31D8)
#define VSC_EXT_VESA_NUM_DP_ENCODER0_P0_FLDMASK                           0x3f
#define VSC_EXT_VESA_NUM_DP_ENCODER0_P0_FLDMASK_POS                       0
#define VSC_EXT_VESA_NUM_DP_ENCODER0_P0_FLDMASK_LEN                       6

#define VSC_EXT_CEA_NUM_DP_ENCODER0_P0_FLDMASK                            0x3f00
#define VSC_EXT_CEA_NUM_DP_ENCODER0_P0_FLDMASK_POS                        8
#define VSC_EXT_CEA_NUM_DP_ENCODER0_P0_FLDMASK_LEN                        6

#define REG_31DC_DP_ENCODER0_P0              (0x31DC)
#define HDR0_CFG_DP_ENCODER0_P0_FLDMASK                                   0xff
#define HDR0_CFG_DP_ENCODER0_P0_FLDMASK_POS                               0
#define HDR0_CFG_DP_ENCODER0_P0_FLDMASK_LEN                               8

#define RESERVED_CFG_DP_ENCODER0_P0_FLDMASK                               0xff00
#define RESERVED_CFG_DP_ENCODER0_P0_FLDMASK_POS                           8
#define RESERVED_CFG_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define REG_31E0_DP_ENCODER0_P0              (0x31E0)
#define RESERVED_HB0_DP_ENCODER0_P0_FLDMASK                               0xff
#define RESERVED_HB0_DP_ENCODER0_P0_FLDMASK_POS                           0
#define RESERVED_HB0_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define RESERVED_HB1_DP_ENCODER0_P0_FLDMASK                               0xff00
#define RESERVED_HB1_DP_ENCODER0_P0_FLDMASK_POS                           8
#define RESERVED_HB1_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define REG_31E4_DP_ENCODER0_P0              (0x31E4)
#define RESERVED_HB2_DP_ENCODER0_P0_FLDMASK                               0xff
#define RESERVED_HB2_DP_ENCODER0_P0_FLDMASK_POS                           0
#define RESERVED_HB2_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define RESERVED_HB3_DP_ENCODER0_P0_FLDMASK                               0xff00
#define RESERVED_HB3_DP_ENCODER0_P0_FLDMASK_POS                           8
#define RESERVED_HB3_DP_ENCODER0_P0_FLDMASK_LEN                           8

#define REG_31E8_DP_ENCODER0_P0              (0x31E8)
#define PPS_CFG_DP_ENCODER0_P0_FLDMASK                                    0xff
#define PPS_CFG_DP_ENCODER0_P0_FLDMASK_POS                                0
#define PPS_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define PPS_CFG_ONE_TIME_DP_ENCODER0_P0_FLDMASK                           0x100
#define PPS_CFG_ONE_TIME_DP_ENCODER0_P0_FLDMASK_POS                       8
#define PPS_CFG_ONE_TIME_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define SDP_SPLIT_FIFO_READ_START_POINT_DP_ENCODER0_P0_FLDMASK            0xf000
#define SDP_SPLIT_FIFO_READ_START_POINT_DP_ENCODER0_P0_FLDMASK_POS        12
#define SDP_SPLIT_FIFO_READ_START_POINT_DP_ENCODER0_P0_FLDMASK_LEN        4

#define REG_31EC_DP_ENCODER0_P0              (0x31EC)
#define VIDEO_M_CODE_FROM_DPRX_DP_ENCODER0_P0_FLDMASK                     0x1
#define VIDEO_M_CODE_FROM_DPRX_DP_ENCODER0_P0_FLDMASK_POS                 0
#define VIDEO_M_CODE_FROM_DPRX_DP_ENCODER0_P0_FLDMASK_LEN                 1

#define MSA_MISC_FROM_DPRX_DP_ENCODER0_P0_FLDMASK                         0x2
#define MSA_MISC_FROM_DPRX_DP_ENCODER0_P0_FLDMASK_POS                     1
#define MSA_MISC_FROM_DPRX_DP_ENCODER0_P0_FLDMASK_LEN                     1

#define ADS_CFG_DP_ENCODER0_P0_FLDMASK                                    0x4
#define ADS_CFG_DP_ENCODER0_P0_FLDMASK_POS                                2
#define ADS_CFG_DP_ENCODER0_P0_FLDMASK_LEN                                1

#define ADS_MODE_DP_ENCODER0_P0_FLDMASK                                   0x8
#define ADS_MODE_DP_ENCODER0_P0_FLDMASK_POS                               3
#define ADS_MODE_DP_ENCODER0_P0_FLDMASK_LEN                               1

#define AUDIO_CH_SRC_SEL_DP_ENCODER0_P0_FLDMASK                           0x10
#define AUDIO_CH_SRC_SEL_DP_ENCODER0_P0_FLDMASK_POS                       4
#define AUDIO_CH_SRC_SEL_DP_ENCODER0_P0_FLDMASK_LEN                       1

#define ISRC1_HB3_DP_ENCODER0_P0_FLDMASK                                  0xff00
#define ISRC1_HB3_DP_ENCODER0_P0_FLDMASK_POS                              8
#define ISRC1_HB3_DP_ENCODER0_P0_FLDMASK_LEN                              8

#define REG_31F0_DP_ENCODER0_P0              (0x31F0)
#define ADS_HB0_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ADS_HB0_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ADS_HB0_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ADS_HB1_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ADS_HB1_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ADS_HB1_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_31F8_DP_ENCODER0_P0              (0x31F8)
#define ADS_HB2_DP_ENCODER0_P0_FLDMASK                                    0xff
#define ADS_HB2_DP_ENCODER0_P0_FLDMASK_POS                                0
#define ADS_HB2_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define ADS_HB3_DP_ENCODER0_P0_FLDMASK                                    0xff00
#define ADS_HB3_DP_ENCODER0_P0_FLDMASK_POS                                8
#define ADS_HB3_DP_ENCODER0_P0_FLDMASK_LEN                                8

#define REG_31FC_DP_ENCODER0_P0              (0x31FC)
#define VIDEO_ARBITER_DE_LAST_NUM0_SW_DP_ENCODER0_P0_FLDMASK              0x3
#define VIDEO_ARBITER_DE_LAST_NUM0_SW_DP_ENCODER0_P0_FLDMASK_POS          0
#define VIDEO_ARBITER_DE_LAST_NUM0_SW_DP_ENCODER0_P0_FLDMASK_LEN          2

#define VIDEO_ARBITER_DE_LAST_NUM1_SW_DP_ENCODER0_P0_FLDMASK              0xc
#define VIDEO_ARBITER_DE_LAST_NUM1_SW_DP_ENCODER0_P0_FLDMASK_POS          2
#define VIDEO_ARBITER_DE_LAST_NUM1_SW_DP_ENCODER0_P0_FLDMASK_LEN          2

#define VIDEO_ARBITER_DE_LAST_NUM2_SW_DP_ENCODER0_P0_FLDMASK              0x30
#define VIDEO_ARBITER_DE_LAST_NUM2_SW_DP_ENCODER0_P0_FLDMASK_POS          4
#define VIDEO_ARBITER_DE_LAST_NUM2_SW_DP_ENCODER0_P0_FLDMASK_LEN          2

#define VIDEO_ARBITER_DE_LAST_NUM3_SW_DP_ENCODER0_P0_FLDMASK              0xc0
#define VIDEO_ARBITER_DE_LAST_NUM3_SW_DP_ENCODER0_P0_FLDMASK_POS          6
#define VIDEO_ARBITER_DE_LAST_NUM3_SW_DP_ENCODER0_P0_FLDMASK_LEN          2

#define HDE_NUM_EVEN_EN_SW_LANE0_DP_ENCODER0_P0_FLDMASK                   0x100
#define HDE_NUM_EVEN_EN_SW_LANE0_DP_ENCODER0_P0_FLDMASK_POS               8
#define HDE_NUM_EVEN_EN_SW_LANE0_DP_ENCODER0_P0_FLDMASK_LEN               1

#define HDE_NUM_EVEN_EN_SW_LANE1_DP_ENCODER0_P0_FLDMASK                   0x200
#define HDE_NUM_EVEN_EN_SW_LANE1_DP_ENCODER0_P0_FLDMASK_POS               9
#define HDE_NUM_EVEN_EN_SW_LANE1_DP_ENCODER0_P0_FLDMASK_LEN               1

#define HDE_NUM_EVEN_EN_SW_LANE2_DP_ENCODER0_P0_FLDMASK                   0x400
#define HDE_NUM_EVEN_EN_SW_LANE2_DP_ENCODER0_P0_FLDMASK_POS               10
#define HDE_NUM_EVEN_EN_SW_LANE2_DP_ENCODER0_P0_FLDMASK_LEN               1

#define HDE_NUM_EVEN_EN_SW_LANE3_DP_ENCODER0_P0_FLDMASK                   0x800
#define HDE_NUM_EVEN_EN_SW_LANE3_DP_ENCODER0_P0_FLDMASK_POS               11
#define HDE_NUM_EVEN_EN_SW_LANE3_DP_ENCODER0_P0_FLDMASK_LEN               1

#define DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK                             0x1000
#define DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK_POS                         12
#define DE_LAST_NUM_SW_DP_ENCODER0_P0_FLDMASK_LEN                         1

#define REG_3200_DP_ENCODER1_P0              (0x3200)
#define SDP_DB0_DP_ENCODER1_P0_FLDMASK                                    0xff
#define SDP_DB0_DP_ENCODER1_P0_FLDMASK_POS                                0
#define SDP_DB0_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define SDP_DB1_DP_ENCODER1_P0_FLDMASK                                    0xff00
#define SDP_DB1_DP_ENCODER1_P0_FLDMASK_POS                                8
#define SDP_DB1_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define REG_3204_DP_ENCODER1_P0              (0x3204)
#define SDP_DB2_DP_ENCODER1_P0_FLDMASK                                    0xff
#define SDP_DB2_DP_ENCODER1_P0_FLDMASK_POS                                0
#define SDP_DB2_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define SDP_DB3_DP_ENCODER1_P0_FLDMASK                                    0xff00
#define SDP_DB3_DP_ENCODER1_P0_FLDMASK_POS                                8
#define SDP_DB3_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define REG_3208_DP_ENCODER1_P0              (0x3208)
#define SDP_DB4_DP_ENCODER1_P0_FLDMASK                                    0xff
#define SDP_DB4_DP_ENCODER1_P0_FLDMASK_POS                                0
#define SDP_DB4_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define SDP_DB5_DP_ENCODER1_P0_FLDMASK                                    0xff00
#define SDP_DB5_DP_ENCODER1_P0_FLDMASK_POS                                8
#define SDP_DB5_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define REG_320C_DP_ENCODER1_P0              (0x320C)
#define SDP_DB6_DP_ENCODER1_P0_FLDMASK                                    0xff
#define SDP_DB6_DP_ENCODER1_P0_FLDMASK_POS                                0
#define SDP_DB6_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define SDP_DB7_DP_ENCODER1_P0_FLDMASK                                    0xff00
#define SDP_DB7_DP_ENCODER1_P0_FLDMASK_POS                                8
#define SDP_DB7_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define REG_3210_DP_ENCODER1_P0              (0x3210)
#define SDP_DB8_DP_ENCODER1_P0_FLDMASK                                    0xff
#define SDP_DB8_DP_ENCODER1_P0_FLDMASK_POS                                0
#define SDP_DB8_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define SDP_DB9_DP_ENCODER1_P0_FLDMASK                                    0xff00
#define SDP_DB9_DP_ENCODER1_P0_FLDMASK_POS                                8
#define SDP_DB9_DP_ENCODER1_P0_FLDMASK_LEN                                8

#define REG_3214_DP_ENCODER1_P0              (0x3214)
#define SDP_DB10_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB10_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB10_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB11_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB11_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB11_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3218_DP_ENCODER1_P0              (0x3218)
#define SDP_DB12_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB12_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB12_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB13_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB13_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB13_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_321C_DP_ENCODER1_P0              (0x321C)
#define SDP_DB14_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB14_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB14_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB15_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB15_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB15_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3220_DP_ENCODER1_P0              (0x3220)
#define SDP_DB16_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB16_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB16_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB17_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB17_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB17_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3224_DP_ENCODER1_P0              (0x3224)
#define SDP_DB18_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB18_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB18_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB19_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB19_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB19_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3228_DP_ENCODER1_P0              (0x3228)
#define SDP_DB20_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB20_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB20_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB21_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB21_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB21_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_322C_DP_ENCODER1_P0              (0x322C)
#define SDP_DB22_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB22_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB22_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB23_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB23_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB23_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3230_DP_ENCODER1_P0              (0x3230)
#define SDP_DB24_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB24_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB24_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB25_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB25_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB25_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3234_DP_ENCODER1_P0              (0x3234)
#define SDP_DB26_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB26_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB26_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB27_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB27_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB27_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3238_DP_ENCODER1_P0              (0x3238)
#define SDP_DB28_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB28_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB28_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB29_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB29_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB29_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_323C_DP_ENCODER1_P0              (0x323C)
#define SDP_DB30_DP_ENCODER1_P0_FLDMASK                                   0xff
#define SDP_DB30_DP_ENCODER1_P0_FLDMASK_POS                               0
#define SDP_DB30_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define SDP_DB31_DP_ENCODER1_P0_FLDMASK                                   0xff00
#define SDP_DB31_DP_ENCODER1_P0_FLDMASK_POS                               8
#define SDP_DB31_DP_ENCODER1_P0_FLDMASK_LEN                               8

#define REG_3240_DP_ENCODER1_P0              (0x3240)
#define SDP_DB0_R_DP_ENCODER1_P0_FLDMASK                                  0xff
#define SDP_DB0_R_DP_ENCODER1_P0_FLDMASK_POS                              0
#define SDP_DB0_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define SDP_DB1_R_DP_ENCODER1_P0_FLDMASK                                  0xff00
#define SDP_DB1_R_DP_ENCODER1_P0_FLDMASK_POS                              8
#define SDP_DB1_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define REG_3244_DP_ENCODER1_P0              (0x3244)
#define SDP_DB2_R_DP_ENCODER1_P0_FLDMASK                                  0xff
#define SDP_DB2_R_DP_ENCODER1_P0_FLDMASK_POS                              0
#define SDP_DB2_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define SDP_DB3_R_DP_ENCODER1_P0_FLDMASK                                  0xff00
#define SDP_DB3_R_DP_ENCODER1_P0_FLDMASK_POS                              8
#define SDP_DB3_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define REG_3248_DP_ENCODER1_P0              (0x3248)
#define SDP_DB4_R_DP_ENCODER1_P0_FLDMASK                                  0xff
#define SDP_DB4_R_DP_ENCODER1_P0_FLDMASK_POS                              0
#define SDP_DB4_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define SDP_DB5_R_DP_ENCODER1_P0_FLDMASK                                  0xff00
#define SDP_DB5_R_DP_ENCODER1_P0_FLDMASK_POS                              8
#define SDP_DB5_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define REG_324C_DP_ENCODER1_P0              (0x324C)
#define SDP_DB6_R_DP_ENCODER1_P0_FLDMASK                                  0xff
#define SDP_DB6_R_DP_ENCODER1_P0_FLDMASK_POS                              0
#define SDP_DB6_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define SDP_DB7_R_DP_ENCODER1_P0_FLDMASK                                  0xff00
#define SDP_DB7_R_DP_ENCODER1_P0_FLDMASK_POS                              8
#define SDP_DB7_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define REG_3250_DP_ENCODER1_P0              (0x3250)
#define SDP_DB8_R_DP_ENCODER1_P0_FLDMASK                                  0xff
#define SDP_DB8_R_DP_ENCODER1_P0_FLDMASK_POS                              0
#define SDP_DB8_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define SDP_DB9_R_DP_ENCODER1_P0_FLDMASK                                  0xff00
#define SDP_DB9_R_DP_ENCODER1_P0_FLDMASK_POS                              8
#define SDP_DB9_R_DP_ENCODER1_P0_FLDMASK_LEN                              8

#define REG_3254_DP_ENCODER1_P0              (0x3254)
#define SDP_DB10_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB10_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB10_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB11_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB11_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB11_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3258_DP_ENCODER1_P0              (0x3258)
#define SDP_DB12_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB12_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB12_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB13_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB13_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB13_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_325C_DP_ENCODER1_P0              (0x325C)
#define SDP_DB14_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB14_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB14_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB15_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB15_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB15_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3260_DP_ENCODER1_P0              (0x3260)
#define SDP_DB16_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB16_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB16_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB17_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB17_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB17_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3264_DP_ENCODER1_P0              (0x3264)
#define SDP_DB18_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB18_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB18_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB19_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB19_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB19_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3268_DP_ENCODER1_P0              (0x3268)
#define SDP_DB20_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB20_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB20_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB21_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB21_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB21_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_326C_DP_ENCODER1_P0              (0x326C)
#define SDP_DB22_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB22_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB22_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB23_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB23_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB23_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3270_DP_ENCODER1_P0              (0x3270)
#define SDP_DB24_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB24_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB24_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB25_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB25_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB25_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3274_DP_ENCODER1_P0              (0x3274)
#define SDP_DB26_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB26_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB26_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB27_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB27_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB27_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3278_DP_ENCODER1_P0              (0x3278)
#define SDP_DB28_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB28_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB28_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB29_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB29_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB29_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_327C_DP_ENCODER1_P0              (0x327C)
#define SDP_DB30_R_DP_ENCODER1_P0_FLDMASK                                 0xff
#define SDP_DB30_R_DP_ENCODER1_P0_FLDMASK_POS                             0
#define SDP_DB30_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define SDP_DB31_R_DP_ENCODER1_P0_FLDMASK                                 0xff00
#define SDP_DB31_R_DP_ENCODER1_P0_FLDMASK_POS                             8
#define SDP_DB31_R_DP_ENCODER1_P0_FLDMASK_LEN                             8

#define REG_3280_DP_ENCODER1_P0              (0x3280)
#define SDP_PACKET_TYPE_DP_ENCODER1_P0_FLDMASK                            0x1f
#define SDP_PACKET_TYPE_DP_ENCODER1_P0_FLDMASK_POS                        0
#define SDP_PACKET_TYPE_DP_ENCODER1_P0_FLDMASK_LEN                        5

#define SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK                               0x20
#define SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK_POS                           5
#define SDP_PACKET_W_DP_ENCODER1_P0_FLDMASK_LEN                           1

#define SDP_PACKET_R_DP_ENCODER1_P0_FLDMASK                               0x40
#define SDP_PACKET_R_DP_ENCODER1_P0_FLDMASK_POS                           6
#define SDP_PACKET_R_DP_ENCODER1_P0_FLDMASK_LEN                           1

#define REG_328C_DP_ENCODER1_P0              (0x328C)
#define VSC_SW_HW_SEL_VESA_DP_ENCODER1_P0_FLDMASK                         0x1
#define VSC_SW_HW_SEL_VESA_DP_ENCODER1_P0_FLDMASK_POS                     0
#define VSC_SW_HW_SEL_VESA_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define VSC_SRAM_HW_RST_VESA_DP_ENCODER1_P0_FLDMASK                       0x2
#define VSC_SRAM_HW_RST_VESA_DP_ENCODER1_P0_FLDMASK_POS                   1
#define VSC_SRAM_HW_RST_VESA_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VSC_SRAM_SW_RST_VESA_DP_ENCODER1_P0_FLDMASK                       0x4
#define VSC_SRAM_SW_RST_VESA_DP_ENCODER1_P0_FLDMASK_POS                   2
#define VSC_SRAM_SW_RST_VESA_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VSC_SRAM_HW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK                     0x8
#define VSC_SRAM_HW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK_POS                 3
#define VSC_SRAM_HW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK_LEN                 1

#define VSC_SRAM_HW_FULL_VESA_DP_ENCODER1_P0_FLDMASK                      0x10
#define VSC_SRAM_HW_FULL_VESA_DP_ENCODER1_P0_FLDMASK_POS                  4
#define VSC_SRAM_HW_FULL_VESA_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define VSC_SRAM_HW_FULL_CLR_VESA_DP_ENCODER1_P0_FLDMASK                  0x20
#define VSC_SRAM_HW_FULL_CLR_VESA_DP_ENCODER1_P0_FLDMASK_POS              5
#define VSC_SRAM_HW_FULL_CLR_VESA_DP_ENCODER1_P0_FLDMASK_LEN              1

#define VSC_DATA_TOGGLE_VESA_DP_ENCODER1_P0_FLDMASK                       0x40
#define VSC_DATA_TOGGLE_VESA_DP_ENCODER1_P0_FLDMASK_POS                   6
#define VSC_DATA_TOGGLE_VESA_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VSC_DATA_RDY_VESA_DP_ENCODER1_P0_FLDMASK                          0x80
#define VSC_DATA_RDY_VESA_DP_ENCODER1_P0_FLDMASK_POS                      7
#define VSC_DATA_RDY_VESA_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VSC_SRAM_SW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK                     0x100
#define VSC_SRAM_SW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK_POS                 8
#define VSC_SRAM_SW_EMPTY_VESA_DP_ENCODER1_P0_FLDMASK_LEN                 1

#define REG_3290_DP_ENCODER1_P0              (0x3290)
#define VSC_DATA_BYTE0_VESA_DP_ENCODER1_P0_FLDMASK                        0xff
#define VSC_DATA_BYTE0_VESA_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VSC_DATA_BYTE0_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define VSC_DATA_BYTE1_VESA_DP_ENCODER1_P0_FLDMASK                        0xff00
#define VSC_DATA_BYTE1_VESA_DP_ENCODER1_P0_FLDMASK_POS                    8
#define VSC_DATA_BYTE1_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_3294_DP_ENCODER1_P0              (0x3294)
#define VSC_DATA_BYTE2_VESA_DP_ENCODER1_P0_FLDMASK                        0xff
#define VSC_DATA_BYTE2_VESA_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VSC_DATA_BYTE2_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define VSC_DATA_BYTE3_VESA_DP_ENCODER1_P0_FLDMASK                        0xff00
#define VSC_DATA_BYTE3_VESA_DP_ENCODER1_P0_FLDMASK_POS                    8
#define VSC_DATA_BYTE3_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_3298_DP_ENCODER1_P0              (0x3298)
#define VSC_DATA_BYTE4_VESA_DP_ENCODER1_P0_FLDMASK                        0xff
#define VSC_DATA_BYTE4_VESA_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VSC_DATA_BYTE4_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define VSC_DATA_BYTE5_VESA_DP_ENCODER1_P0_FLDMASK                        0xff00
#define VSC_DATA_BYTE5_VESA_DP_ENCODER1_P0_FLDMASK_POS                    8
#define VSC_DATA_BYTE5_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_329C_DP_ENCODER1_P0              (0x329C)
#define VSC_DATA_BYTE6_VESA_DP_ENCODER1_P0_FLDMASK                        0xff
#define VSC_DATA_BYTE6_VESA_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VSC_DATA_BYTE6_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define VSC_DATA_BYTE7_VESA_DP_ENCODER1_P0_FLDMASK                        0xff00
#define VSC_DATA_BYTE7_VESA_DP_ENCODER1_P0_FLDMASK_POS                    8
#define VSC_DATA_BYTE7_VESA_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_32A0_DP_ENCODER1_P0              (0x32A0)
#define VSC_SW_HW_SEL_CEA_DP_ENCODER1_P0_FLDMASK                          0x1
#define VSC_SW_HW_SEL_CEA_DP_ENCODER1_P0_FLDMASK_POS                      0
#define VSC_SW_HW_SEL_CEA_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VSC_SRAM_HW_RST_CEA_DP_ENCODER1_P0_FLDMASK                        0x2
#define VSC_SRAM_HW_RST_CEA_DP_ENCODER1_P0_FLDMASK_POS                    1
#define VSC_SRAM_HW_RST_CEA_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define VSC_SRAM_SW_RST_CEA_DP_ENCODER1_P0_FLDMASK                        0x4
#define VSC_SRAM_SW_RST_CEA_DP_ENCODER1_P0_FLDMASK_POS                    2
#define VSC_SRAM_SW_RST_CEA_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define VSC_SRAM_HW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK                      0x8
#define VSC_SRAM_HW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK_POS                  3
#define VSC_SRAM_HW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define VSC_SRAM_HW_FULL_CEA_DP_ENCODER1_P0_FLDMASK                       0x10
#define VSC_SRAM_HW_FULL_CEA_DP_ENCODER1_P0_FLDMASK_POS                   4
#define VSC_SRAM_HW_FULL_CEA_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VSC_SRAM_HW_FULL_CLR_CEA_DP_ENCODER1_P0_FLDMASK                   0x20
#define VSC_SRAM_HW_FULL_CLR_CEA_DP_ENCODER1_P0_FLDMASK_POS               5
#define VSC_SRAM_HW_FULL_CLR_CEA_DP_ENCODER1_P0_FLDMASK_LEN               1

#define VSC_DATA_TOGGLE_CEA_DP_ENCODER1_P0_FLDMASK                        0x40
#define VSC_DATA_TOGGLE_CEA_DP_ENCODER1_P0_FLDMASK_POS                    6
#define VSC_DATA_TOGGLE_CEA_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define VSC_DATA_RDY_CEA_DP_ENCODER1_P0_FLDMASK                           0x80
#define VSC_DATA_RDY_CEA_DP_ENCODER1_P0_FLDMASK_POS                       7
#define VSC_DATA_RDY_CEA_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define VSC_SRAM_SW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK                      0x100
#define VSC_SRAM_SW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK_POS                  8
#define VSC_SRAM_SW_EMPTY_CEA_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define REG_32A4_DP_ENCODER1_P0              (0x32A4)
#define VSC_DATA_BYTE0_CEA_DP_ENCODER1_P0_FLDMASK                         0xff
#define VSC_DATA_BYTE0_CEA_DP_ENCODER1_P0_FLDMASK_POS                     0
#define VSC_DATA_BYTE0_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define VSC_DATA_BYTE1_CEA_DP_ENCODER1_P0_FLDMASK                         0xff00
#define VSC_DATA_BYTE1_CEA_DP_ENCODER1_P0_FLDMASK_POS                     8
#define VSC_DATA_BYTE1_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define REG_32A8_DP_ENCODER1_P0              (0x32A8)
#define VSC_DATA_BYTE2_CEA_DP_ENCODER1_P0_FLDMASK                         0xff
#define VSC_DATA_BYTE2_CEA_DP_ENCODER1_P0_FLDMASK_POS                     0
#define VSC_DATA_BYTE2_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define VSC_DATA_BYTE3_CEA_DP_ENCODER1_P0_FLDMASK                         0xff00
#define VSC_DATA_BYTE3_CEA_DP_ENCODER1_P0_FLDMASK_POS                     8
#define VSC_DATA_BYTE3_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define REG_32AC_DP_ENCODER1_P0              (0x32AC)
#define VSC_DATA_BYTE4_CEA_DP_ENCODER1_P0_FLDMASK                         0xff
#define VSC_DATA_BYTE4_CEA_DP_ENCODER1_P0_FLDMASK_POS                     0
#define VSC_DATA_BYTE4_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define VSC_DATA_BYTE5_CEA_DP_ENCODER1_P0_FLDMASK                         0xff00
#define VSC_DATA_BYTE5_CEA_DP_ENCODER1_P0_FLDMASK_POS                     8
#define VSC_DATA_BYTE5_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define REG_32B0_DP_ENCODER1_P0              (0x32B0)
#define VSC_DATA_BYTE6_CEA_DP_ENCODER1_P0_FLDMASK                         0xff
#define VSC_DATA_BYTE6_CEA_DP_ENCODER1_P0_FLDMASK_POS                     0
#define VSC_DATA_BYTE6_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define VSC_DATA_BYTE7_CEA_DP_ENCODER1_P0_FLDMASK                         0xff00
#define VSC_DATA_BYTE7_CEA_DP_ENCODER1_P0_FLDMASK_POS                     8
#define VSC_DATA_BYTE7_CEA_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define REG_32B4_DP_ENCODER1_P0              (0x32B4)
#define VSC_DATA_SW_CAN_WRITE_VESA_DP_ENCODER1_P0_FLDMASK                 0x1
#define VSC_DATA_SW_CAN_WRITE_VESA_DP_ENCODER1_P0_FLDMASK_POS             0
#define VSC_DATA_SW_CAN_WRITE_VESA_DP_ENCODER1_P0_FLDMASK_LEN             1

#define VSC_DATA_SW_CAN_WRITE_CEA_DP_ENCODER1_P0_FLDMASK                  0x2
#define VSC_DATA_SW_CAN_WRITE_CEA_DP_ENCODER1_P0_FLDMASK_POS              1
#define VSC_DATA_SW_CAN_WRITE_CEA_DP_ENCODER1_P0_FLDMASK_LEN              1

#define VSC_DATA_TRANSMIT_SEL_VESA_DP_ENCODER1_P0_FLDMASK                 0x4
#define VSC_DATA_TRANSMIT_SEL_VESA_DP_ENCODER1_P0_FLDMASK_POS             2
#define VSC_DATA_TRANSMIT_SEL_VESA_DP_ENCODER1_P0_FLDMASK_LEN             1

#define VSC_DATA_TRANSMIT_SEL_CEA_DP_ENCODER1_P0_FLDMASK                  0x8
#define VSC_DATA_TRANSMIT_SEL_CEA_DP_ENCODER1_P0_FLDMASK_POS              3
#define VSC_DATA_TRANSMIT_SEL_CEA_DP_ENCODER1_P0_FLDMASK_LEN              1

#define REG_32C0_DP_ENCODER1_P0              (0x32C0)
#define IRQ_MASK_DP_ENCODER1_P0_FLDMASK                                   0xffff
#define IRQ_MASK_DP_ENCODER1_P0_FLDMASK_POS                               0
#define IRQ_MASK_DP_ENCODER1_P0_FLDMASK_LEN                               16

#define REG_32C4_DP_ENCODER1_P0              (0x32C4)
#define IRQ_CLR_DP_ENCODER1_P0_FLDMASK                                    0xffff
#define IRQ_CLR_DP_ENCODER1_P0_FLDMASK_POS                                0
#define IRQ_CLR_DP_ENCODER1_P0_FLDMASK_LEN                                16

#define REG_32C8_DP_ENCODER1_P0              (0x32C8)
#define IRQ_FORCE_DP_ENCODER1_P0_FLDMASK                                  0xffff
#define IRQ_FORCE_DP_ENCODER1_P0_FLDMASK_POS                              0
#define IRQ_FORCE_DP_ENCODER1_P0_FLDMASK_LEN                              16

#define REG_32CC_DP_ENCODER1_P0              (0x32CC)
#define IRQ_STATUS_DP_ENCODER1_P0_FLDMASK                                 0xffff
#define IRQ_STATUS_DP_ENCODER1_P0_FLDMASK_POS                             0
#define IRQ_STATUS_DP_ENCODER1_P0_FLDMASK_LEN                             16

#define REG_32D0_DP_ENCODER1_P0              (0x32D0)
#define IRQ_FINAL_STATUS_DP_ENCODER1_P0_FLDMASK                           0xffff
#define IRQ_FINAL_STATUS_DP_ENCODER1_P0_FLDMASK_POS                       0
#define IRQ_FINAL_STATUS_DP_ENCODER1_P0_FLDMASK_LEN                       16

#define REG_32D4_DP_ENCODER1_P0              (0x32D4)
#define IRQ_MASK_51_DP_ENCODER1_P0_FLDMASK                                0xffff
#define IRQ_MASK_51_DP_ENCODER1_P0_FLDMASK_POS                            0
#define IRQ_MASK_51_DP_ENCODER1_P0_FLDMASK_LEN                            16

#define REG_32D8_DP_ENCODER1_P0              (0x32D8)
#define IRQ_CLR_51_DP_ENCODER1_P0_FLDMASK                                 0xffff
#define IRQ_CLR_51_DP_ENCODER1_P0_FLDMASK_POS                             0
#define IRQ_CLR_51_DP_ENCODER1_P0_FLDMASK_LEN                             16

#define REG_32DC_DP_ENCODER1_P0              (0x32DC)
#define IRQ_FORCE_51_DP_ENCODER1_P0_FLDMASK                               0xffff
#define IRQ_FORCE_51_DP_ENCODER1_P0_FLDMASK_POS                           0
#define IRQ_FORCE_51_DP_ENCODER1_P0_FLDMASK_LEN                           16

#define REG_32E0_DP_ENCODER1_P0              (0x32E0)
#define IRQ_STATUS_51_DP_ENCODER1_P0_FLDMASK                              0xffff
#define IRQ_STATUS_51_DP_ENCODER1_P0_FLDMASK_POS                          0
#define IRQ_STATUS_51_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_32E4_DP_ENCODER1_P0              (0x32E4)
#define IRQ_FINAL_STATUS_51_DP_ENCODER1_P0_FLDMASK                        0xffff
#define IRQ_FINAL_STATUS_51_DP_ENCODER1_P0_FLDMASK_POS                    0
#define IRQ_FINAL_STATUS_51_DP_ENCODER1_P0_FLDMASK_LEN                    16

#define REG_32E8_DP_ENCODER1_P0              (0x32E8)
#define AUDIO_SRAM_WRITE_ADDR_0_DP_ENCODER1_P0_FLDMASK                    0x7f
#define AUDIO_SRAM_WRITE_ADDR_0_DP_ENCODER1_P0_FLDMASK_POS                0
#define AUDIO_SRAM_WRITE_ADDR_0_DP_ENCODER1_P0_FLDMASK_LEN                7

#define AUDIO_SRAM_WRITE_ADDR_1_DP_ENCODER1_P0_FLDMASK                    0x7f00
#define AUDIO_SRAM_WRITE_ADDR_1_DP_ENCODER1_P0_FLDMASK_POS                8
#define AUDIO_SRAM_WRITE_ADDR_1_DP_ENCODER1_P0_FLDMASK_LEN                7

#define REG_32EC_DP_ENCODER1_P0              (0x32EC)
#define AUDIO_SRAM_WRITE_ADDR_2_DP_ENCODER1_P0_FLDMASK                    0x7f
#define AUDIO_SRAM_WRITE_ADDR_2_DP_ENCODER1_P0_FLDMASK_POS                0
#define AUDIO_SRAM_WRITE_ADDR_2_DP_ENCODER1_P0_FLDMASK_LEN                7

#define AUDIO_SRAM_WRITE_ADDR_3_DP_ENCODER1_P0_FLDMASK                    0x7f00
#define AUDIO_SRAM_WRITE_ADDR_3_DP_ENCODER1_P0_FLDMASK_POS                8
#define AUDIO_SRAM_WRITE_ADDR_3_DP_ENCODER1_P0_FLDMASK_LEN                7

#define REG_32F0_DP_ENCODER1_P0              (0x32F0)
#define M_CODE_FEC_MERGE_0_DP_ENCODER1_P0_FLDMASK                         0xffff
#define M_CODE_FEC_MERGE_0_DP_ENCODER1_P0_FLDMASK_POS                     0
#define M_CODE_FEC_MERGE_0_DP_ENCODER1_P0_FLDMASK_LEN                     16

#define REG_32F4_DP_ENCODER1_P0              (0x32F4)
#define M_CODE_FEC_MERGE_1_DP_ENCODER1_P0_FLDMASK                         0xff
#define M_CODE_FEC_MERGE_1_DP_ENCODER1_P0_FLDMASK_POS                     0
#define M_CODE_FEC_MERGE_1_DP_ENCODER1_P0_FLDMASK_LEN                     8

#define REG_32F8_DP_ENCODER1_P0              (0x32F8)
#define MSA_UPDATE_LINE_CNT_THRD_DP_ENCODER1_P0_FLDMASK                   0xff
#define MSA_UPDATE_LINE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS               0
#define MSA_UPDATE_LINE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_LEN               8

#define SDP_SPLIT_BUG_FIX_DP_ENCODER1_P0_FLDMASK                          0x200
#define SDP_SPLIT_BUG_FIX_DP_ENCODER1_P0_FLDMASK_POS                      9
#define SDP_SPLIT_BUG_FIX_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define MSA_MUTE_MASK_DP_ENCODER1_P0_FLDMASK                              0x400
#define MSA_MUTE_MASK_DP_ENCODER1_P0_FLDMASK_POS                          10
#define MSA_MUTE_MASK_DP_ENCODER1_P0_FLDMASK_LEN                          1

#define MSA_UPDATE_SEL_DP_ENCODER1_P0_FLDMASK                             0x3000
#define MSA_UPDATE_SEL_DP_ENCODER1_P0_FLDMASK_POS                         12
#define MSA_UPDATE_SEL_DP_ENCODER1_P0_FLDMASK_LEN                         2

#define VIDEO_MUTE_TOGGLE_SEL_DP_ENCODER1_P0_FLDMASK                      0xc000
#define VIDEO_MUTE_TOGGLE_SEL_DP_ENCODER1_P0_FLDMASK_POS                  14
#define VIDEO_MUTE_TOGGLE_SEL_DP_ENCODER1_P0_FLDMASK_LEN                  2

#define REG_3300_DP_ENCODER1_P0              (0x3300)
#define AUDIO_AFIFO_CNT_SEL_DP_ENCODER1_P0_FLDMASK                        0x1
#define AUDIO_AFIFO_CNT_SEL_DP_ENCODER1_P0_FLDMASK_POS                    0
#define AUDIO_AFIFO_CNT_SEL_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define AUDIO_SRAM_CNT_SEL_DP_ENCODER1_P0_FLDMASK                         0x2
#define AUDIO_SRAM_CNT_SEL_DP_ENCODER1_P0_FLDMASK_POS                     1
#define AUDIO_SRAM_CNT_SEL_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define AUDIO_AFIFO_CNT_DP_ENCODER1_P0_FLDMASK                            0xf0
#define AUDIO_AFIFO_CNT_DP_ENCODER1_P0_FLDMASK_POS                        4
#define AUDIO_AFIFO_CNT_DP_ENCODER1_P0_FLDMASK_LEN                        4

#define VIDEO_AFIFO_RDY_SEL_DP_ENCODER1_P0_FLDMASK                        0x300
#define VIDEO_AFIFO_RDY_SEL_DP_ENCODER1_P0_FLDMASK_POS                    8
#define VIDEO_AFIFO_RDY_SEL_DP_ENCODER1_P0_FLDMASK_LEN                    2

#define REG_3304_DP_ENCODER1_P0              (0x3304)
#define AUDIO_SRAM_CNT_DP_ENCODER1_P0_FLDMASK                             0x7f
#define AUDIO_SRAM_CNT_DP_ENCODER1_P0_FLDMASK_POS                         0
#define AUDIO_SRAM_CNT_DP_ENCODER1_P0_FLDMASK_LEN                         7

#define AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK                              0x100
#define AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK_POS                          8
#define AU_PRTY_REGEN_DP_ENCODER1_P0_FLDMASK_LEN                          1

#define AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK                            0x200
#define AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK_POS                        9
#define AU_CH_STS_REGEN_DP_ENCODER1_P0_FLDMASK_LEN                        1

#define AUDIO_VALIDITY_REGEN_DP_ENCODER1_P0_FLDMASK                       0x400
#define AUDIO_VALIDITY_REGEN_DP_ENCODER1_P0_FLDMASK_POS                   10
#define AUDIO_VALIDITY_REGEN_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define AUDIO_RESERVED_REGEN_DP_ENCODER1_P0_FLDMASK                       0x800
#define AUDIO_RESERVED_REGEN_DP_ENCODER1_P0_FLDMASK_POS                   11
#define AUDIO_RESERVED_REGEN_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK                  0x1000
#define AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK_POS              12
#define AUDIO_SAMPLE_PRSENT_REGEN_DP_ENCODER1_P0_FLDMASK_LEN              1

#define REG_3320_DP_ENCODER1_P0              (0x3320)
#define AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK            0x1ff
#define AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS        0
#define AUDIO_PATTERN_GEN_DSTB_CNT_THRD_DP_ENCODER1_P0_FLDMASK_LEN        9

#define REG_3324_DP_ENCODER1_P0              (0x3324)
#define AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK                           0x300
#define AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_POS                       8
#define AUDIO_SOURCE_MUX_DP_ENCODER1_P0_FLDMASK_LEN                       2

#define AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK                   0x3000
#define AUDIO_PATGEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_POS                    12
#define AUDIO_PATTERN_GEN_CH_NUM_DP_ENCODER1_P0_FLDMASK_LEN               2

#define AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK                   0xc000
#define AUDIO_PATGEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_POS                    14
#define AUDIO_PATTERN_GEN_FS_SEL_DP_ENCODER1_P0_FLDMASK_LEN               2

#define REG_3328_DP_ENCODER1_P0              (0x3328)
#define VSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK                           0x1
#define VSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK_POS                       0
#define VSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define HSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK                           0x2
#define HSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK_POS                       1
#define HSYNC_DETECT_POL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define HTOTAL_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK                       0x4
#define HTOTAL_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK_POS                   2
#define HTOTAL_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define HDE_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK                          0x8
#define HDE_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK_POS                      3
#define HDE_DETECT_STABLE_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define REG_332C_DP_ENCODER1_P0              (0x332C)
#define VTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK                              0xffff
#define VTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK_POS                          0
#define VTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_3330_DP_ENCODER1_P0              (0x3330)
#define VDE_DETECT_DP_ENCODER1_P0_FLDMASK                                 0xffff
#define VDE_DETECT_DP_ENCODER1_P0_FLDMASK_POS                             0
#define VDE_DETECT_DP_ENCODER1_P0_FLDMASK_LEN                             16

#define REG_3334_DP_ENCODER1_P0              (0x3334)
#define HTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK                              0xffff
#define HTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK_POS                          0
#define HTOTAL_DETECT_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_3338_DP_ENCODER1_P0              (0x3338)
#define HDE_DETECT_DP_ENCODER1_P0_FLDMASK                                 0xffff
#define HDE_DETECT_DP_ENCODER1_P0_FLDMASK_POS                             0
#define HDE_DETECT_DP_ENCODER1_P0_FLDMASK_LEN                             16

#define REG_3340_DP_ENCODER1_P0              (0x3340)
#define BIST_FAIL_VIDEO_L0_DP_ENCODER1_P0_FLDMASK                         0x1
#define BIST_FAIL_VIDEO_L0_DP_ENCODER1_P0_FLDMASK_POS                     0
#define BIST_FAIL_VIDEO_L0_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_VIDEO_L1_DP_ENCODER1_P0_FLDMASK                         0x2
#define BIST_FAIL_VIDEO_L1_DP_ENCODER1_P0_FLDMASK_POS                     1
#define BIST_FAIL_VIDEO_L1_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_VIDEO_L2_DP_ENCODER1_P0_FLDMASK                         0x4
#define BIST_FAIL_VIDEO_L2_DP_ENCODER1_P0_FLDMASK_POS                     2
#define BIST_FAIL_VIDEO_L2_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_VIDEO_L3_DP_ENCODER1_P0_FLDMASK                         0x8
#define BIST_FAIL_VIDEO_L3_DP_ENCODER1_P0_FLDMASK_POS                     3
#define BIST_FAIL_VIDEO_L3_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_AUDIO_L0_DP_ENCODER1_P0_FLDMASK                         0x10
#define BIST_FAIL_AUDIO_L0_DP_ENCODER1_P0_FLDMASK_POS                     4
#define BIST_FAIL_AUDIO_L0_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_AUDIO_L1_DP_ENCODER1_P0_FLDMASK                         0x20
#define BIST_FAIL_AUDIO_L1_DP_ENCODER1_P0_FLDMASK_POS                     5
#define BIST_FAIL_AUDIO_L1_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_AUDIO_L2_DP_ENCODER1_P0_FLDMASK                         0x40
#define BIST_FAIL_AUDIO_L2_DP_ENCODER1_P0_FLDMASK_POS                     6
#define BIST_FAIL_AUDIO_L2_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_AUDIO_L3_DP_ENCODER1_P0_FLDMASK                         0x80
#define BIST_FAIL_AUDIO_L3_DP_ENCODER1_P0_FLDMASK_POS                     7
#define BIST_FAIL_AUDIO_L3_DP_ENCODER1_P0_FLDMASK_LEN                     1

#define BIST_FAIL_VSC_VESA_HW_DP_ENCODER1_P0_FLDMASK                      0x100
#define BIST_FAIL_VSC_VESA_HW_DP_ENCODER1_P0_FLDMASK_POS                  8
#define BIST_FAIL_VSC_VESA_HW_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define BIST_FAIL_VSC_CEA_HW_DP_ENCODER1_P0_FLDMASK                       0x200
#define BIST_FAIL_VSC_CEA_HW_DP_ENCODER1_P0_FLDMASK_POS                   9
#define BIST_FAIL_VSC_CEA_HW_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define BIST_FAIL_VSC_VESA_SW_DP_ENCODER1_P0_FLDMASK                      0x400
#define BIST_FAIL_VSC_VESA_SW_DP_ENCODER1_P0_FLDMASK_POS                  10
#define BIST_FAIL_VSC_VESA_SW_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define BIST_FAIL_VSC_CEA_SW_DP_ENCODER1_P0_FLDMASK                       0x800
#define BIST_FAIL_VSC_CEA_SW_DP_ENCODER1_P0_FLDMASK_POS                   11
#define BIST_FAIL_VSC_CEA_SW_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define LR_FIELD_SYNC_SEL_DP_ENCODER1_P0_FLDMASK                          0x7000
#define LR_FIELD_SYNC_SEL_DP_ENCODER1_P0_FLDMASK_POS                      12
#define LR_FIELD_SYNC_SEL_DP_ENCODER1_P0_FLDMASK_LEN                      3

#define REG_3344_DP_ENCODER1_P0              (0x3344)
#define DP_CH1_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f
#define DP_CH1_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          0
#define DP_CH1_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define DP_CH2_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f00
#define DP_CH2_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          8
#define DP_CH2_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define REG_3348_DP_ENCODER1_P0              (0x3348)
#define DP_CH3_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f
#define DP_CH3_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          0
#define DP_CH3_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define DP_CH4_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f00
#define DP_CH4_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          8
#define DP_CH4_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define REG_334C_DP_ENCODER1_P0              (0x334C)
#define DP_CH5_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f
#define DP_CH5_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          0
#define DP_CH5_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define DP_CH6_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f00
#define DP_CH6_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          8
#define DP_CH6_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define REG_3350_DP_ENCODER1_P0              (0x3350)
#define DP_CH7_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f
#define DP_CH7_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          0
#define DP_CH7_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define DP_CH8_MATRIX_DP_ENCODER1_P0_FLDMASK                              0x1f00
#define DP_CH8_MATRIX_DP_ENCODER1_P0_FLDMASK_POS                          8
#define DP_CH8_MATRIX_DP_ENCODER1_P0_FLDMASK_LEN                          5

#define REG_3354_DP_ENCODER1_P0              (0x3354)
#define DP_S2P_LAUNCH_CFG_DP_ENCODER1_P0_FLDMASK                          0x7f
#define DP_S2P_LAUNCH_CFG_DP_ENCODER1_P0_FLDMASK_POS                      0
#define DP_S2P_LAUNCH_CFG_DP_ENCODER1_P0_FLDMASK_LEN                      7

#define AUDIO_HAYDN_EN_FORCE_DP_ENCODER1_P0_FLDMASK                       0x1000
#define AUDIO_HAYDN_EN_FORCE_DP_ENCODER1_P0_FLDMASK_POS                   12
#define AUDIO_HAYDN_EN_FORCE_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define AUDIO_HAYDN_FORMAT_DP_ENCODER1_P0_FLDMASK                         0xf00
#define AUDIO_HAYDN_FORMAT_DP_ENCODER1_P0_FLDMASK_POS                     8
#define AUDIO_HAYDN_FORMAT_DP_ENCODER1_P0_FLDMASK_LEN                     4

#define REG_3358_DP_ENCODER1_P0              (0x3358)
#define TU_SIZE_DP_ENCODER1_P0_FLDMASK                                    0x7f
#define TU_SIZE_DP_ENCODER1_P0_FLDMASK_POS                                0
#define TU_SIZE_DP_ENCODER1_P0_FLDMASK_LEN                                7

#define TU_CALC_SW_DP_ENCODER1_P0_FLDMASK                                 0x80
#define TU_CALC_SW_DP_ENCODER1_P0_FLDMASK_POS                             7
#define TU_CALC_SW_DP_ENCODER1_P0_FLDMASK_LEN                             1

#define REG_335C_DP_ENCODER1_P0              (0x335C)
#define SYMBOL_DATA_PER_TU_SW_0_DP_ENCODER1_P0_FLDMASK                    0xffff
#define SYMBOL_DATA_PER_TU_SW_0_DP_ENCODER1_P0_FLDMASK_POS                0
#define SYMBOL_DATA_PER_TU_SW_0_DP_ENCODER1_P0_FLDMASK_LEN                16

#define REG_3360_DP_ENCODER1_P0              (0x3360)
#define SYMBOL_DATA_PER_TU_SW_1_DP_ENCODER1_P0_FLDMASK                    0x7fff
#define SYMBOL_DATA_PER_TU_SW_1_DP_ENCODER1_P0_FLDMASK_POS                0
#define SYMBOL_DATA_PER_TU_SW_1_DP_ENCODER1_P0_FLDMASK_LEN                15

#define REG_3364_DP_ENCODER1_P0              (0x3364)
#define SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK                0xfff
#define SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK_POS            0
#define SDP_DOWN_CNT_INIT_IN_HBLANK_DP_ENCODER1_P0_FLDMASK_LEN            12

#define FIFO_READ_START_POINT_DP_ENCODER1_P0_FLDMASK                      0xf000
#define FIFO_READ_START_POINT_DP_ENCODER1_P0_FLDMASK_POS                  12
#define FIFO_READ_START_POINT_DP_ENCODER1_P0_FLDMASK_LEN                  4

#define REG_3368_DP_ENCODER1_P0              (0x3368)
#define VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK              0x3
#define VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK_POS          0
#define VIDEO_SRAM_FIFO_CNT_RESET_SEL_DP_ENCODER1_P0_FLDMASK_LEN          2

#define VIDEO_STABLE_EN_DP_ENCODER1_P0_FLDMASK                            0x4
#define VIDEO_STABLE_EN_DP_ENCODER1_P0_FLDMASK_POS                        2
#define VIDEO_STABLE_EN_DP_ENCODER1_P0_FLDMASK_LEN                        1

#define VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK                      0xf0
#define VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS                  4
#define VIDEO_STABLE_CNT_THRD_DP_ENCODER1_P0_FLDMASK_LEN                  4

#define SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK                                0x100
#define SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK_POS                            8
#define SDP_DP13_EN_DP_ENCODER1_P0_FLDMASK_LEN                            1

#define VIDEO_PIXEL_SWAP_DP_ENCODER1_P0_FLDMASK                           0x600
#define VIDEO_PIXEL_SWAP_DP_ENCODER1_P0_FLDMASK_POS                       9
#define VIDEO_PIXEL_SWAP_DP_ENCODER1_P0_FLDMASK_LEN                       2

#define BS2BS_MODE_DP_ENCODER1_P0_FLDMASK                                 0x3000
#define BS2BS_MODE_DP_ENCODER1_P0_FLDMASK_POS                             12
#define BS2BS_MODE_DP_ENCODER1_P0_FLDMASK_LEN                             2

#define REG_336C_DP_ENCODER1_P0              (0x336C)
#define DSC_EN_DP_ENCODER1_P0_FLDMASK                                     0x1
#define DSC_EN_DP_ENCODER1_P0_FLDMASK_POS                                 0
#define DSC_EN_DP_ENCODER1_P0_FLDMASK_LEN                                 1

#define DSC_BYTE_SWAP_DP_ENCODER1_P0_FLDMASK                              0x2
#define DSC_BYTE_SWAP_DP_ENCODER1_P0_FLDMASK_POS                          1
#define DSC_BYTE_SWAP_DP_ENCODER1_P0_FLDMASK_LEN                          1

#define DSC_SLICE_NUM_DP_ENCODER1_P0_FLDMASK                              0xf0
#define DSC_SLICE_NUM_DP_ENCODER1_P0_FLDMASK_POS                          4
#define DSC_SLICE_NUM_DP_ENCODER1_P0_FLDMASK_LEN                          4

#define DSC_CHUNK_REMAINDER_DP_ENCODER1_P0_FLDMASK                        0xf00
#define DSC_CHUNK_REMAINDER_DP_ENCODER1_P0_FLDMASK_POS                    8
#define DSC_CHUNK_REMAINDER_DP_ENCODER1_P0_FLDMASK_LEN                    4

#define REG_3370_DP_ENCODER1_P0              (0x3370)
#define DSC_CHUNK_NUM_DP_ENCODER1_P0_FLDMASK                              0xffff
#define DSC_CHUNK_NUM_DP_ENCODER1_P0_FLDMASK_POS                          0
#define DSC_CHUNK_NUM_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_33AC_DP_ENCODER1_P0              (0x33AC)
#define TEST_CRC_R_CR_DP_ENCODER1_P0_FLDMASK                              0xffff
#define TEST_CRC_R_CR_DP_ENCODER1_P0_FLDMASK_POS                          0
#define TEST_CRC_R_CR_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_33B0_DP_ENCODER1_P0              (0x33B0)
#define TEST_CRC_G_Y_DP_ENCODER1_P0_FLDMASK                               0xffff
#define TEST_CRC_G_Y_DP_ENCODER1_P0_FLDMASK_POS                           0
#define TEST_CRC_G_Y_DP_ENCODER1_P0_FLDMASK_LEN                           16

#define REG_33B4_DP_ENCODER1_P0              (0x33B4)
#define TEST_CRC_B_CB_DP_ENCODER1_P0_FLDMASK                              0xffff
#define TEST_CRC_B_CB_DP_ENCODER1_P0_FLDMASK_POS                          0
#define TEST_CRC_B_CB_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_33B8_DP_ENCODER1_P0              (0x33B8)
#define TEST_CRC_WRAP_CNT_DP_ENCODER1_P0_FLDMASK                          0xf
#define TEST_CRC_WRAP_CNT_DP_ENCODER1_P0_FLDMASK_POS                      0
#define TEST_CRC_WRAP_CNT_DP_ENCODER1_P0_FLDMASK_LEN                      4

#define CRC_COLOR_FORMAT_DP_ENCODER1_P0_FLDMASK                           0x1f0
#define CRC_COLOR_FORMAT_DP_ENCODER1_P0_FLDMASK_POS                       4
#define CRC_COLOR_FORMAT_DP_ENCODER1_P0_FLDMASK_LEN                       5

#define CRC_TEST_SINK_START_DP_ENCODER1_P0_FLDMASK                        0x200
#define CRC_TEST_SINK_START_DP_ENCODER1_P0_FLDMASK_POS                    9
#define CRC_TEST_SINK_START_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define REG_33BC_DP_ENCODER1_P0              (0x33BC)
#define CRC_TEST_CONFIG_DP_ENCODER1_P0_FLDMASK                            0x1fff
#define CRC_TEST_CONFIG_DP_ENCODER1_P0_FLDMASK_POS                        0
#define CRC_TEST_CONFIG_DP_ENCODER1_P0_FLDMASK_LEN                        13

#define REG_33C0_DP_ENCODER1_P0              (0x33C0)
#define VIDEO_TU_VALUE_DP_ENCODER1_P0_FLDMASK                             0x7f
#define VIDEO_TU_VALUE_DP_ENCODER1_P0_FLDMASK_POS                         0
#define VIDEO_TU_VALUE_DP_ENCODER1_P0_FLDMASK_LEN                         7

#define DP_TX_MIXER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK                    0xf00
#define DP_TX_MIXER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_POS                8
#define DP_TX_MIXER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_LEN                4

#define DP_TX_SDP_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK                      0xf000
#define DP_TX_SDP_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_POS                  12
#define DP_TX_SDP_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_LEN                  4

#define REG_33C4_DP_ENCODER1_P0              (0x33C4)
#define DP_TX_VIDEO_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK                    0x1f
#define DP_TX_VIDEO_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_POS                0
#define DP_TX_VIDEO_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_LEN                5

#define DP_TX_ENCODER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK                  0x60
#define DP_TX_ENCODER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_POS              5
#define DP_TX_ENCODER_TESTBUS_SEL_DP_ENCODER1_P0_FLDMASK_LEN              2

#define REG_33C8_DP_ENCODER1_P0              (0x33C8)
#define VIDEO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK                        0xffff
#define VIDEO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VIDEO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK_LEN                    16

#define REG_33CC_DP_ENCODER1_P0              (0x33CC)
#define VIDEO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK                        0xff
#define VIDEO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK_POS                    0
#define VIDEO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_33D0_DP_ENCODER1_P0              (0x33D0)
#define AUDIO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK                        0xffff
#define AUDIO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK_POS                    0
#define AUDIO_M_CODE_READ_0_DP_ENCODER1_P0_FLDMASK_LEN                    16

#define REG_33D4_DP_ENCODER1_P0              (0x33D4)
#define AUDIO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK                        0xff
#define AUDIO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK_POS                    0
#define AUDIO_M_CODE_READ_1_DP_ENCODER1_P0_FLDMASK_LEN                    8

#define REG_33D8_DP_ENCODER1_P0              (0x33D8)
#define VSC_EXT_CFG_DP_ENCODER1_P0_FLDMASK                                0xff
#define VSC_EXT_CFG_DP_ENCODER1_P0_FLDMASK_POS                            0
#define VSC_EXT_CFG_DP_ENCODER1_P0_FLDMASK_LEN                            8

#define SDP_SPLIT_FIFO_EMPTY_DP_ENCODER1_P0_FLDMASK                       0x100
#define SDP_SPLIT_FIFO_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                   8
#define SDP_SPLIT_FIFO_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define SDP_SPLIT_FIFO_FULL_DP_ENCODER1_P0_FLDMASK                        0x200
#define SDP_SPLIT_FIFO_FULL_DP_ENCODER1_P0_FLDMASK_POS                    9
#define SDP_SPLIT_FIFO_FULL_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define SDP_SPLIT_FIFO_FULL_CLR_DP_ENCODER1_P0_FLDMASK                    0x400
#define SDP_SPLIT_FIFO_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                10
#define SDP_SPLIT_FIFO_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                1

#define SDP_SPLIT_INSERT_INVALID_CNT_THRD_DP_ENCODER1_P0_FLDMASK          0xf000
#define SDP_SPLIT_INSERT_INVALID_CNT_THRD_DP_ENCODER1_P0_FLDMASK_POS      12
#define SDP_SPLIT_INSERT_INVALID_CNT_THRD_DP_ENCODER1_P0_FLDMASK_LEN      4

#define REG_33DC_DP_ENCODER1_P0              (0x33DC)
#define VIDEO_SRAM0_FULL_DP_ENCODER1_P0_FLDMASK                           0x1
#define VIDEO_SRAM0_FULL_DP_ENCODER1_P0_FLDMASK_POS                       0
#define VIDEO_SRAM0_FULL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define VIDEO_SRAM0_FULL_CLR_DP_ENCODER1_P0_FLDMASK                       0x2
#define VIDEO_SRAM0_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                   1
#define VIDEO_SRAM0_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VIDEO_SRAM1_FULL_DP_ENCODER1_P0_FLDMASK                           0x4
#define VIDEO_SRAM1_FULL_DP_ENCODER1_P0_FLDMASK_POS                       2
#define VIDEO_SRAM1_FULL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define VIDEO_SRAM1_FULL_CLR_DP_ENCODER1_P0_FLDMASK                       0x8
#define VIDEO_SRAM1_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                   3
#define VIDEO_SRAM1_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VIDEO_SRAM2_FULL_DP_ENCODER1_P0_FLDMASK                           0x10
#define VIDEO_SRAM2_FULL_DP_ENCODER1_P0_FLDMASK_POS                       4
#define VIDEO_SRAM2_FULL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define VIDEO_SRAM2_FULL_CLR_DP_ENCODER1_P0_FLDMASK                       0x20
#define VIDEO_SRAM2_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                   5
#define VIDEO_SRAM2_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VIDEO_SRAM3_FULL_DP_ENCODER1_P0_FLDMASK                           0x40
#define VIDEO_SRAM3_FULL_DP_ENCODER1_P0_FLDMASK_POS                       6
#define VIDEO_SRAM3_FULL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define VIDEO_SRAM3_FULL_CLR_DP_ENCODER1_P0_FLDMASK                       0x80
#define VIDEO_SRAM3_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                   7
#define VIDEO_SRAM3_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                   1

#define VIDEO_SRAM0_EMPTY_DP_ENCODER1_P0_FLDMASK                          0x100
#define VIDEO_SRAM0_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                      8
#define VIDEO_SRAM0_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VIDEO_SRAM0_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK                      0x200
#define VIDEO_SRAM0_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_POS                  9
#define VIDEO_SRAM0_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define VIDEO_SRAM1_EMPTY_DP_ENCODER1_P0_FLDMASK                          0x400
#define VIDEO_SRAM1_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                      10
#define VIDEO_SRAM1_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VIDEO_SRAM1_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK                      0x800
#define VIDEO_SRAM1_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_POS                  11
#define VIDEO_SRAM1_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define VIDEO_SRAM2_EMPTY_DP_ENCODER1_P0_FLDMASK                          0x1000
#define VIDEO_SRAM2_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                      12
#define VIDEO_SRAM2_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VIDEO_SRAM2_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK                      0x2000
#define VIDEO_SRAM2_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_POS                  13
#define VIDEO_SRAM2_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define VIDEO_SRAM3_EMPTY_DP_ENCODER1_P0_FLDMASK                          0x4000
#define VIDEO_SRAM3_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                      14
#define VIDEO_SRAM3_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                      1

#define VIDEO_SRAM3_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK                      0x8000
#define VIDEO_SRAM3_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_POS                  15
#define VIDEO_SRAM3_EMPTY_CLR_DP_ENCODER1_P0_FLDMASK_LEN                  1

#define REG_33E0_DP_ENCODER1_P0              (0x33E0)
#define BS2BS_CNT_SW_DP_ENCODER1_P0_FLDMASK                               0xffff
#define BS2BS_CNT_SW_DP_ENCODER1_P0_FLDMASK_POS                           0
#define BS2BS_CNT_SW_DP_ENCODER1_P0_FLDMASK_LEN                           16

#define REG_33E4_DP_ENCODER1_P0              (0x33E4)
#define MIXER_STATE_0_DP_ENCODER1_P0_FLDMASK                              0xffff
#define MIXER_STATE_0_DP_ENCODER1_P0_FLDMASK_POS                          0
#define MIXER_STATE_0_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_33E8_DP_ENCODER1_P0              (0x33E8)
#define MIXER_STATE_1_DP_ENCODER1_P0_FLDMASK                              0xffff
#define MIXER_STATE_1_DP_ENCODER1_P0_FLDMASK_POS                          0
#define MIXER_STATE_1_DP_ENCODER1_P0_FLDMASK_LEN                          16

#define REG_33EC_DP_ENCODER1_P0              (0x33EC)
#define MIXER_STATE_2_DP_ENCODER1_P0_FLDMASK                              0xff
#define MIXER_STATE_2_DP_ENCODER1_P0_FLDMASK_POS                          0
#define MIXER_STATE_2_DP_ENCODER1_P0_FLDMASK_LEN                          8

#define VIDEO_PERIOD_ENABLE_DP_ENCODER1_P0_FLDMASK                        0x200
#define VIDEO_PERIOD_ENABLE_DP_ENCODER1_P0_FLDMASK_POS                    9
#define VIDEO_PERIOD_ENABLE_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define BS2BS_CNT_SW_SEL_DP_ENCODER1_P0_FLDMASK                           0x400
#define BS2BS_CNT_SW_SEL_DP_ENCODER1_P0_FLDMASK_POS                       10
#define BS2BS_CNT_SW_SEL_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define AUDIO_SRAM_FULL_DP_ENCODER1_P0_FLDMASK                            0x800
#define AUDIO_SRAM_FULL_DP_ENCODER1_P0_FLDMASK_POS                        11
#define AUDIO_SRAM_FULL_DP_ENCODER1_P0_FLDMASK_LEN                        1

#define AUDIO_SRAM_FULL_CLR_DP_ENCODER1_P0_FLDMASK                        0x1000
#define AUDIO_SRAM_FULL_CLR_DP_ENCODER1_P0_FLDMASK_POS                    12
#define AUDIO_SRAM_FULL_CLR_DP_ENCODER1_P0_FLDMASK_LEN                    1

#define AUDIO_SRAM_EMPTY_DP_ENCODER1_P0_FLDMASK                           0x2000
#define AUDIO_SRAM_EMPTY_DP_ENCODER1_P0_FLDMASK_POS                       13
#define AUDIO_SRAM_EMPTY_DP_ENCODER1_P0_FLDMASK_LEN                       1

#define REG_33F0_DP_ENCODER1_P0              (0x33F0)
#define DP_ENCODER_DUMMY_RW_0_DP_ENCODER1_P0_FLDMASK                      0xffff
#define DP_ENCODER_DUMMY_RW_0_DP_ENCODER1_P0_FLDMASK_POS                  0
#define DP_ENCODER_DUMMY_RW_0_DP_ENCODER1_P0_FLDMASK_LEN                  16

#define REG_33F4_DP_ENCODER1_P0              (0x33F4)
#define DP_ENCODER_DUMMY_RW_1_DP_ENCODER1_P0_FLDMASK                      0xffff
#define DP_ENCODER_DUMMY_RW_1_DP_ENCODER1_P0_FLDMASK_POS                  0
#define DP_ENCODER_DUMMY_RW_1_DP_ENCODER1_P0_FLDMASK_LEN                  16

#define REG_33F8_DP_ENCODER1_P0              (0x33F8)
#define DP_ENCODER_DUMMY_R_0_DP_ENCODER1_P0_FLDMASK                       0xffff
#define DP_ENCODER_DUMMY_R_0_DP_ENCODER1_P0_FLDMASK_POS                   0
#define DP_ENCODER_DUMMY_R_0_DP_ENCODER1_P0_FLDMASK_LEN                   16

#define REG_33FC_DP_ENCODER1_P0              (0x33FC)
#define DP_ENCODER_DUMMY_R_1_DP_ENCODER1_P0_FLDMASK                       0xffff
#define DP_ENCODER_DUMMY_R_1_DP_ENCODER1_P0_FLDMASK_POS                   0
#define DP_ENCODER_DUMMY_R_1_DP_ENCODER1_P0_FLDMASK_LEN                   16

#define REG_3400_DP_TRANS_P0              (0x3400)
#define PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK                            0x3
#define PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_POS                        0
#define PRE_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_LEN                        2

#define PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK                            0xc
#define PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_POS                        2
#define PRE_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_LEN                        2

#define PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK                            0x30
#define PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_POS                        4
#define PRE_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_LEN                        2

#define PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK                            0xc0
#define PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_POS                        6
#define PRE_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_LEN                        2

#define PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK                             0x700
#define PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK_POS                         8
#define PRE_MISC_PORT_MUX_DP_TRANS_P0_FLDMASK_LEN                         3

#define HDCP_SEL_DP_TRANS_P0_FLDMASK                                      0x800
#define HDCP_SEL_DP_TRANS_P0_FLDMASK_POS                                  11
#define HDCP_SEL_DP_TRANS_P0_FLDMASK_LEN                                  1

#define PATTERN1_EN_DP_TRANS_P0_FLDMASK                                   0x1000
#define PATTERN1_EN_DP_TRANS_P0_FLDMASK_POS                               12
#define PATTERN1_EN_DP_TRANS_P0_FLDMASK_LEN                               1

#define PATTERN2_EN_DP_TRANS_P0_FLDMASK                                   0x2000
#define PATTERN2_EN_DP_TRANS_P0_FLDMASK_POS                               13
#define PATTERN2_EN_DP_TRANS_P0_FLDMASK_LEN                               1

#define PATTERN3_EN_DP_TRANS_P0_FLDMASK                                   0x4000
#define PATTERN3_EN_DP_TRANS_P0_FLDMASK_POS                               14
#define PATTERN3_EN_DP_TRANS_P0_FLDMASK_LEN                               1

#define PATTERN4_EN_DP_TRANS_P0_FLDMASK                                   0x8000
#define PATTERN4_EN_DP_TRANS_P0_FLDMASK_POS                               15
#define PATTERN4_EN_DP_TRANS_P0_FLDMASK_LEN                               1

#define REG_3404_DP_TRANS_P0              (0x3404)
#define DP_SCR_EN_DP_TRANS_P0_FLDMASK                                     0x1
#define DP_SCR_EN_DP_TRANS_P0_FLDMASK_POS                                 0
#define DP_SCR_EN_DP_TRANS_P0_FLDMASK_LEN                                 1

#define ALTER_SCRAMBLER_RESET_EN_DP_TRANS_P0_FLDMASK                      0x2
#define ALTER_SCRAMBLER_RESET_EN_DP_TRANS_P0_FLDMASK_POS                  1
#define ALTER_SCRAMBLER_RESET_EN_DP_TRANS_P0_FLDMASK_LEN                  1

#define SCRAMB_BYPASS_IN_EN_DP_TRANS_P0_FLDMASK                           0x4
#define SCRAMB_BYPASS_IN_EN_DP_TRANS_P0_FLDMASK_POS                       2
#define SCRAMB_BYPASS_IN_EN_DP_TRANS_P0_FLDMASK_LEN                       1

#define SCRAMB_BYPASS_MASK_DP_TRANS_P0_FLDMASK                            0x8
#define SCRAMB_BYPASS_MASK_DP_TRANS_P0_FLDMASK_POS                        3
#define SCRAMB_BYPASS_MASK_DP_TRANS_P0_FLDMASK_LEN                        1

#define INDEX_SCR_MODE_DP_TRANS_P0_FLDMASK                                0x30
#define INDEX_SCR_MODE_DP_TRANS_P0_FLDMASK_POS                            4
#define INDEX_SCR_MODE_DP_TRANS_P0_FLDMASK_LEN                            2

#define PAT_INIT_DISPARITY_DP_TRANS_P0_FLDMASK                            0x40
#define PAT_INIT_DISPARITY_DP_TRANS_P0_FLDMASK_POS                        6
#define PAT_INIT_DISPARITY_DP_TRANS_P0_FLDMASK_LEN                        1

#define TPS_DISPARITY_RESET_DP_TRANS_P0_FLDMASK                           0x80
#define TPS_DISPARITY_RESET_DP_TRANS_P0_FLDMASK_POS                       7
#define TPS_DISPARITY_RESET_DP_TRANS_P0_FLDMASK_LEN                       1

#define REG_3408_DP_TRANS_P0              (0x3408)
#define LANE_SKEW_SEL_LANE0_DP_TRANS_P0_FLDMASK                           0x3
#define LANE_SKEW_SEL_LANE0_DP_TRANS_P0_FLDMASK_POS                       0
#define LANE_SKEW_SEL_LANE0_DP_TRANS_P0_FLDMASK_LEN                       2

#define LANE_SKEW_SEL_LANE1_DP_TRANS_P0_FLDMASK                           0xc
#define LANE_SKEW_SEL_LANE1_DP_TRANS_P0_FLDMASK_POS                       2
#define LANE_SKEW_SEL_LANE1_DP_TRANS_P0_FLDMASK_LEN                       2

#define LANE_SKEW_SEL_LANE2_DP_TRANS_P0_FLDMASK                           0x30
#define LANE_SKEW_SEL_LANE2_DP_TRANS_P0_FLDMASK_POS                       4
#define LANE_SKEW_SEL_LANE2_DP_TRANS_P0_FLDMASK_LEN                       2

#define LANE_SKEW_SEL_LANE3_DP_TRANS_P0_FLDMASK                           0xc0
#define LANE_SKEW_SEL_LANE3_DP_TRANS_P0_FLDMASK_POS                       6
#define LANE_SKEW_SEL_LANE3_DP_TRANS_P0_FLDMASK_LEN                       2

#define POST_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK                           0x300
#define POST_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_POS                       8
#define POST_MISC_LANE0_MUX_DP_TRANS_P0_FLDMASK_LEN                       2

#define POST_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK                           0xc00
#define POST_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_POS                       10
#define POST_MISC_LANE1_MUX_DP_TRANS_P0_FLDMASK_LEN                       2

#define POST_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK                           0x3000
#define POST_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_POS                       12
#define POST_MISC_LANE2_MUX_DP_TRANS_P0_FLDMASK_LEN                       2

#define POST_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK                           0xc000
#define POST_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_POS                       14
#define POST_MISC_LANE3_MUX_DP_TRANS_P0_FLDMASK_LEN                       2

#define REG_340C_DP_TRANS_P0              (0x340C)
#define TOP_RESET_SW_DP_TRANS_P0_FLDMASK                                  0x100
#define TOP_RESET_SW_DP_TRANS_P0_FLDMASK_POS                              8
#define TOP_RESET_SW_DP_TRANS_P0_FLDMASK_LEN                              1

#define LANE0_RESET_SW_DP_TRANS_P0_FLDMASK                                0x200
#define LANE0_RESET_SW_DP_TRANS_P0_FLDMASK_POS                            9
#define LANE0_RESET_SW_DP_TRANS_P0_FLDMASK_LEN                            1

#define LANE1_RESET_SW_DP_TRANS_P0_FLDMASK                                0x400
#define LANE1_RESET_SW_DP_TRANS_P0_FLDMASK_POS                            10
#define LANE1_RESET_SW_DP_TRANS_P0_FLDMASK_LEN                            1

#define LANE2_RESET_SW_DP_TRANS_P0_FLDMASK                                0x800
#define LANE2_RESET_SW_DP_TRANS_P0_FLDMASK_POS                            11
#define LANE2_RESET_SW_DP_TRANS_P0_FLDMASK_LEN                            1

#define LANE3_RESET_SW_DP_TRANS_P0_FLDMASK                                0x1000
#define LANE3_RESET_SW_DP_TRANS_P0_FLDMASK_POS                            12
#define LANE3_RESET_SW_DP_TRANS_P0_FLDMASK_LEN                            1

#define DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_FLDMASK                 0x2000
#define DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_FLDMASK_POS             13
#define DP_TX_TRANSMITTER_4P_RESET_SW_DP_TRANS_P0_FLDMASK_LEN             1

#define HDCP13_RST_SW_DP_TRANS_P0_FLDMASK                                 0x4000
#define HDCP13_RST_SW_DP_TRANS_P0_FLDMASK_POS                             14
#define HDCP13_RST_SW_DP_TRANS_P0_FLDMASK_LEN                             1

#define HDCP22_RST_SW_DP_TRANS_P0_FLDMASK                                 0x8000
#define HDCP22_RST_SW_DP_TRANS_P0_FLDMASK_POS                             15
#define HDCP22_RST_SW_DP_TRANS_P0_FLDMASK_LEN                             1

#define REG_3410_DP_TRANS_P0              (0x3410)
#define HPD_DEB_THD_DP_TRANS_P0_FLDMASK                                   0xf
#define HPD_DEB_THD_DP_TRANS_P0_FLDMASK_POS                               0
#define HPD_DEB_THD_DP_TRANS_P0_FLDMASK_LEN                               4

#define HPD_INT_THD_DP_TRANS_P0_FLDMASK                                   0xf0
#define HPD_INT_THD_DP_TRANS_P0_FLDMASK_POS                               4
#define HPD_INT_THD_DP_TRANS_P0_FLDMASK_LEN                               4

#define HPD_DISC_THD_DP_TRANS_P0_FLDMASK                                  0xf00
#define HPD_DISC_THD_DP_TRANS_P0_FLDMASK_POS                              8
#define HPD_DISC_THD_DP_TRANS_P0_FLDMASK_LEN                              4

#define HPD_CONN_THD_DP_TRANS_P0_FLDMASK                                  0xf000
#define HPD_CONN_THD_DP_TRANS_P0_FLDMASK_POS                              12
#define HPD_CONN_THD_DP_TRANS_P0_FLDMASK_LEN                              4

#define REG_3414_DP_TRANS_P0              (0x3414)
#define HPD_OVR_EN_DP_TRANS_P0_FLDMASK                                    0x1
#define HPD_OVR_EN_DP_TRANS_P0_FLDMASK_POS                                0
#define HPD_OVR_EN_DP_TRANS_P0_FLDMASK_LEN                                1

#define HPD_SET_DP_TRANS_P0_FLDMASK                                       0x2
#define HPD_SET_DP_TRANS_P0_FLDMASK_POS                                   1
#define HPD_SET_DP_TRANS_P0_FLDMASK_LEN                                   1

#define HPD_DB_DP_TRANS_P0_FLDMASK                                        0x4
#define HPD_DB_DP_TRANS_P0_FLDMASK_POS                                    2
#define HPD_DB_DP_TRANS_P0_FLDMASK_LEN                                    1

#define REG_3418_DP_TRANS_P0              (0x3418)
#define IRQ_CLR_DP_TRANS_P0_FLDMASK                                       0xf
#define IRQ_CLR_DP_TRANS_P0_FLDMASK_POS                                   0
#define IRQ_CLR_DP_TRANS_P0_FLDMASK_LEN                                   4

#define IRQ_MASK_DP_TRANS_P0_FLDMASK                                      0xf0
#define IRQ_MASK_DP_TRANS_P0_FLDMASK_POS                                  4
#define IRQ_MASK_DP_TRANS_P0_FLDMASK_LEN                                  4

#define IRQ_FORCE_DP_TRANS_P0_FLDMASK                                     0xf00
#define IRQ_FORCE_DP_TRANS_P0_FLDMASK_POS                                 8
#define IRQ_FORCE_DP_TRANS_P0_FLDMASK_LEN                                 4

#define IRQ_STATUS_DP_TRANS_P0_FLDMASK                                    0xf000
#define IRQ_STATUS_DP_TRANS_P0_FLDMASK_POS                                12
#define IRQ_STATUS_DP_TRANS_P0_FLDMASK_LEN                                4

#define REG_341C_DP_TRANS_P0              (0x341C)
#define IRQ_CLR_51_DP_TRANS_P0_FLDMASK                                    0xf
#define IRQ_CLR_51_DP_TRANS_P0_FLDMASK_POS                                0
#define IRQ_CLR_51_DP_TRANS_P0_FLDMASK_LEN                                4

#define IRQ_MASK_51_DP_TRANS_P0_FLDMASK                                   0xf0
#define IRQ_MASK_51_DP_TRANS_P0_FLDMASK_POS                               4
#define IRQ_MASK_51_DP_TRANS_P0_FLDMASK_LEN                               4

#define IRQ_FORCE_51_DP_TRANS_P0_FLDMASK                                  0xf00
#define IRQ_FORCE_51_DP_TRANS_P0_FLDMASK_POS                              8
#define IRQ_FORCE_51_DP_TRANS_P0_FLDMASK_LEN                              4

#define IRQ_STATUS_51_DP_TRANS_P0_FLDMASK                                 0xf000
#define IRQ_STATUS_51_DP_TRANS_P0_FLDMASK_POS                             12
#define IRQ_STATUS_51_DP_TRANS_P0_FLDMASK_LEN                             4

#define REG_3420_DP_TRANS_P0              (0x3420)
#define HPD_STATUS_DP_TRANS_P0_FLDMASK                                    0x1
#define HPD_STATUS_DP_TRANS_P0_FLDMASK_POS                                0
#define HPD_STATUS_DP_TRANS_P0_FLDMASK_LEN                                1

#define REG_3428_DP_TRANS_P0              (0x3428)
#define POST_MISC_BIT_REVERSE_EN_LANE0_DP_TRANS_P0_FLDMASK                0x1
#define POST_MISC_BIT_REVERSE_EN_LANE0_DP_TRANS_P0_FLDMASK_POS            0
#define POST_MISC_BIT_REVERSE_EN_LANE0_DP_TRANS_P0_FLDMASK_LEN            1

#define POST_MISC_BIT_REVERSE_EN_LANE1_DP_TRANS_P0_FLDMASK                0x2
#define POST_MISC_BIT_REVERSE_EN_LANE1_DP_TRANS_P0_FLDMASK_POS            1
#define POST_MISC_BIT_REVERSE_EN_LANE1_DP_TRANS_P0_FLDMASK_LEN            1

#define POST_MISC_BIT_REVERSE_EN_LANE2_DP_TRANS_P0_FLDMASK                0x4
#define POST_MISC_BIT_REVERSE_EN_LANE2_DP_TRANS_P0_FLDMASK_POS            2
#define POST_MISC_BIT_REVERSE_EN_LANE2_DP_TRANS_P0_FLDMASK_LEN            1

#define POST_MISC_BIT_REVERSE_EN_LANE3_DP_TRANS_P0_FLDMASK                0x8
#define POST_MISC_BIT_REVERSE_EN_LANE3_DP_TRANS_P0_FLDMASK_POS            3
#define POST_MISC_BIT_REVERSE_EN_LANE3_DP_TRANS_P0_FLDMASK_LEN            1

#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK                    0x10
#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_POS                4
#define POST_MISC_PN_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK                    0x20
#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_POS                5
#define POST_MISC_PN_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK                    0x40
#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_POS                6
#define POST_MISC_PN_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK                    0x80
#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_POS                7
#define POST_MISC_PN_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_LEN                1

#define POST_MISC_DATA_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK                  0x100
#define POST_MISC_DATA_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_POS              8
#define POST_MISC_DATA_SWAP_EN_LANE0_DP_TRANS_P0_FLDMASK_LEN              1

#define POST_MISC_DATA_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK                  0x200
#define POST_MISC_DATA_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_POS              9
#define POST_MISC_DATA_SWAP_EN_LANE1_DP_TRANS_P0_FLDMASK_LEN              1

#define POST_MISC_DATA_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK                  0x400
#define POST_MISC_DATA_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_POS              10
#define POST_MISC_DATA_SWAP_EN_LANE2_DP_TRANS_P0_FLDMASK_LEN              1

#define POST_MISC_DATA_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK                  0x800
#define POST_MISC_DATA_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_POS              11
#define POST_MISC_DATA_SWAP_EN_LANE3_DP_TRANS_P0_FLDMASK_LEN              1

#define REG_342C_DP_TRANS_P0              (0x342C)
#define XTAL_FREQ_DP_TRANS_P0_FLDMASK                                     0xff
#define XTAL_FREQ_DP_TRANS_P0_FLDMASK_POS                                 0
#define XTAL_FREQ_DP_TRANS_P0_FLDMASK_LEN                                 8

#define REG_3430_DP_TRANS_P0              (0x3430)
#define HPD_INT_THD_ECO_DP_TRANS_P0_FLDMASK                               0x3
#define HPD_INT_THD_ECO_DP_TRANS_P0_FLDMASK_POS                           0
#define HPD_INT_THD_ECO_DP_TRANS_P0_FLDMASK_LEN                           2

#define REG_3440_DP_TRANS_P0              (0x3440)
#define PGM_PAT_EN_DP_TRANS_P0_FLDMASK                                    0xf
#define PGM_PAT_EN_DP_TRANS_P0_FLDMASK_POS                                0
#define PGM_PAT_EN_DP_TRANS_P0_FLDMASK_LEN                                4

#define PGM_PAT_SEL_L0_DP_TRANS_P0_FLDMASK                                0x70
#define PGM_PAT_SEL_L0_DP_TRANS_P0_FLDMASK_POS                            4
#define PGM_PAT_SEL_L0_DP_TRANS_P0_FLDMASK_LEN                            3

#define PGM_PAT_SEL_L1_DP_TRANS_P0_FLDMASK                                0x700
#define PGM_PAT_SEL_L1_DP_TRANS_P0_FLDMASK_POS                            8
#define PGM_PAT_SEL_L1_DP_TRANS_P0_FLDMASK_LEN                            3

#define PGM_PAT_SEL_L2_DP_TRANS_P0_FLDMASK                                0x7000
#define PGM_PAT_SEL_L2_DP_TRANS_P0_FLDMASK_POS                            12
#define PGM_PAT_SEL_L2_DP_TRANS_P0_FLDMASK_LEN                            3

#define REG_3444_DP_TRANS_P0              (0x3444)
#define PGM_PAT_SEL_L3_DP_TRANS_P0_FLDMASK                                0x7
#define PGM_PAT_SEL_L3_DP_TRANS_P0_FLDMASK_POS                            0
#define PGM_PAT_SEL_L3_DP_TRANS_P0_FLDMASK_LEN                            3

#define PRBS_EN_DP_TRANS_P0_FLDMASK                                       0x8
#define PRBS_EN_DP_TRANS_P0_FLDMASK_POS                                   3
#define PRBS_EN_DP_TRANS_P0_FLDMASK_LEN                                   1

#define REG_3448_DP_TRANS_P0              (0x3448)
#define PGM_PAT_L0_0_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L0_0_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L0_0_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_344C_DP_TRANS_P0              (0x344C)
#define PGM_PAT_L0_1_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L0_1_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L0_1_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3450_DP_TRANS_P0              (0x3450)
#define PGM_PAT_L0_2_DP_TRANS_P0_FLDMASK                                  0xff
#define PGM_PAT_L0_2_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L0_2_DP_TRANS_P0_FLDMASK_LEN                              8

#define REG_3454_DP_TRANS_P0              (0x3454)
#define PGM_PAT_L1_0_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L1_0_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L1_0_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3458_DP_TRANS_P0              (0x3458)
#define PGM_PAT_L1_1_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L1_1_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L1_1_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_345C_DP_TRANS_P0              (0x345C)
#define PGM_PAT_L1_2_DP_TRANS_P0_FLDMASK                                  0xff
#define PGM_PAT_L1_2_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L1_2_DP_TRANS_P0_FLDMASK_LEN                              8

#define REG_3460_DP_TRANS_P0              (0x3460)
#define PGM_PAT_L2_0_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L2_0_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L2_0_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3464_DP_TRANS_P0              (0x3464)
#define PGM_PAT_L2_1_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L2_1_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L2_1_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3468_DP_TRANS_P0              (0x3468)
#define PGM_PAT_L2_2_DP_TRANS_P0_FLDMASK                                  0xff
#define PGM_PAT_L2_2_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L2_2_DP_TRANS_P0_FLDMASK_LEN                              8

#define REG_346C_DP_TRANS_P0              (0x346C)
#define PGM_PAT_L3_0_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L3_0_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L3_0_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3470_DP_TRANS_P0              (0x3470)
#define PGM_PAT_L3_1_DP_TRANS_P0_FLDMASK                                  0xffff
#define PGM_PAT_L3_1_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L3_1_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_3474_DP_TRANS_P0              (0x3474)
#define PGM_PAT_L3_2_DP_TRANS_P0_FLDMASK                                  0xff
#define PGM_PAT_L3_2_DP_TRANS_P0_FLDMASK_POS                              0
#define PGM_PAT_L3_2_DP_TRANS_P0_FLDMASK_LEN                              8

#define REG_3478_DP_TRANS_P0              (0x3478)
#define CP2520_PATTERN1_DP_TRANS_P0_FLDMASK                               0x1
#define CP2520_PATTERN1_DP_TRANS_P0_FLDMASK_POS                           0
#define CP2520_PATTERN1_DP_TRANS_P0_FLDMASK_LEN                           1

#define CP2520_PATTERN2_DP_TRANS_P0_FLDMASK                               0x2
#define CP2520_PATTERN2_DP_TRANS_P0_FLDMASK_POS                           1
#define CP2520_PATTERN2_DP_TRANS_P0_FLDMASK_LEN                           1

#define CP2520_PATTERN1_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x10
#define CP2520_PATTERN1_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         4
#define CP2520_PATTERN1_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x20
#define CP2520_PATTERN1_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         5
#define CP2520_PATTERN1_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x40
#define CP2520_PATTERN1_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         6
#define CP2520_PATTERN1_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x80
#define CP2520_PATTERN1_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         7
#define CP2520_PATTERN1_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x100
#define CP2520_PATTERN1_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         8
#define CP2520_PATTERN1_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x200
#define CP2520_PATTERN1_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         9
#define CP2520_PATTERN1_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x400
#define CP2520_PATTERN1_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         10
#define CP2520_PATTERN1_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN1_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x800
#define CP2520_PATTERN1_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         11
#define CP2520_PATTERN1_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define REG_347C_DP_TRANS_P0              (0x347C)
#define CP2520_PATTERN2_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x1
#define CP2520_PATTERN2_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         0
#define CP2520_PATTERN2_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x2
#define CP2520_PATTERN2_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         1
#define CP2520_PATTERN2_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x4
#define CP2520_PATTERN2_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         2
#define CP2520_PATTERN2_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x8
#define CP2520_PATTERN2_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         3
#define CP2520_PATTERN2_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x10
#define CP2520_PATTERN2_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         4
#define CP2520_PATTERN2_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x20
#define CP2520_PATTERN2_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         5
#define CP2520_PATTERN2_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x40
#define CP2520_PATTERN2_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         6
#define CP2520_PATTERN2_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN2_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x80
#define CP2520_PATTERN2_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         7
#define CP2520_PATTERN2_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x100
#define CP2520_PATTERN3_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         8
#define CP2520_PATTERN3_KCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x200
#define CP2520_PATTERN3_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         9
#define CP2520_PATTERN3_KCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x400
#define CP2520_PATTERN3_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         10
#define CP2520_PATTERN3_KCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x800
#define CP2520_PATTERN3_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         11
#define CP2520_PATTERN3_KCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK             0x1000
#define CP2520_PATTERN3_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS         12
#define CP2520_PATTERN3_DCODE_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK             0x2000
#define CP2520_PATTERN3_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS         13
#define CP2520_PATTERN3_DCODE_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK             0x4000
#define CP2520_PATTERN3_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS         14
#define CP2520_PATTERN3_DCODE_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN         1

#define CP2520_PATTERN3_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK             0x8000
#define CP2520_PATTERN3_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS         15
#define CP2520_PATTERN3_DCODE_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN         1

#define REG_3480_DP_TRANS_P0              (0x3480)
#define DP_EN_DP_TRANS_P0_FLDMASK                                         0x1
#define DP_EN_DP_TRANS_P0_FLDMASK_POS                                     0
#define DP_EN_DP_TRANS_P0_FLDMASK_LEN                                     1

#define HDCP_CAPABLE_DP_TRANS_P0_FLDMASK                                  0x2
#define HDCP_CAPABLE_DP_TRANS_P0_FLDMASK_POS                              1
#define HDCP_CAPABLE_DP_TRANS_P0_FLDMASK_LEN                              1

#define SELECT_INTERNAL_AN_DP_TRANS_P0_FLDMASK                            0x4
#define SELECT_INTERNAL_AN_DP_TRANS_P0_FLDMASK_POS                        2
#define SELECT_INTERNAL_AN_DP_TRANS_P0_FLDMASK_LEN                        1

#define AN_FREERUN_DP_TRANS_P0_FLDMASK                                    0x8
#define AN_FREERUN_DP_TRANS_P0_FLDMASK_POS                                3
#define AN_FREERUN_DP_TRANS_P0_FLDMASK_LEN                                1

#define KM_GENERATED_DP_TRANS_P0_FLDMASK                                  0x10
#define KM_GENERATED_DP_TRANS_P0_FLDMASK_POS                              4
#define KM_GENERATED_DP_TRANS_P0_FLDMASK_LEN                              1

#define REQ_BLOCK_CIPHER_AUTH_DP_TRANS_P0_FLDMASK                         0x1000
#define REQ_BLOCK_CIPHER_AUTH_DP_TRANS_P0_FLDMASK_POS                     12
#define REQ_BLOCK_CIPHER_AUTH_DP_TRANS_P0_FLDMASK_LEN                     1

#define HDCP_1LANE_SEL_DP_TRANS_P0_FLDMASK                                0x2000
#define HDCP_1LANE_SEL_DP_TRANS_P0_FLDMASK_POS                            13
#define HDCP_1LANE_SEL_DP_TRANS_P0_FLDMASK_LEN                            1

#define HDCP_24LANE_SEL_DP_TRANS_P0_FLDMASK                               0x4000
#define HDCP_24LANE_SEL_DP_TRANS_P0_FLDMASK_POS                           14
#define HDCP_24LANE_SEL_DP_TRANS_P0_FLDMASK_LEN                           1

#define MST_EN_DP_TRANS_P0_FLDMASK                                        0x8000
#define MST_EN_DP_TRANS_P0_FLDMASK_POS                                    15
#define MST_EN_DP_TRANS_P0_FLDMASK_LEN                                    1

#define REG_34A4_DP_TRANS_P0              (0x34A4)
#define EN_COPY_2LANE_MSA_DP_TRANS_P0_FLDMASK                             0x1
#define EN_COPY_2LANE_MSA_DP_TRANS_P0_FLDMASK_POS                         0
#define EN_COPY_2LANE_MSA_DP_TRANS_P0_FLDMASK_LEN                         1

#define EN_COPY_4LANE_MSA_DP_TRANS_P0_FLDMASK                             0x2
#define EN_COPY_4LANE_MSA_DP_TRANS_P0_FLDMASK_POS                         1
#define EN_COPY_4LANE_MSA_DP_TRANS_P0_FLDMASK_LEN                         1

#define LANE_NUM_DP_TRANS_P0_FLDMASK                                      0xc
#define LANE_NUM_DP_TRANS_P0_FLDMASK_POS                                  2
#define LANE_NUM_DP_TRANS_P0_FLDMASK_LEN                                  2

#define HDCP22_AUTH_DONE_DP_TRANS_P0_FLDMASK                              0x10
#define HDCP22_AUTH_DONE_DP_TRANS_P0_FLDMASK_POS                          4
#define HDCP22_AUTH_DONE_DP_TRANS_P0_FLDMASK_LEN                          1

#define DISCARD_UNUSED_CIPHER_DP_TRANS_P0_FLDMASK                         0x20
#define DISCARD_UNUSED_CIPHER_DP_TRANS_P0_FLDMASK_POS                     5
#define DISCARD_UNUSED_CIPHER_DP_TRANS_P0_FLDMASK_LEN                     1

#define HDCP22_CIPHER_REVERSE_DP_TRANS_P0_FLDMASK                         0x40
#define HDCP22_CIPHER_REVERSE_DP_TRANS_P0_FLDMASK_POS                     6
#define HDCP22_CIPHER_REVERSE_DP_TRANS_P0_FLDMASK_LEN                     1

#define MST_DELAY_CYCLE_FLAG_SEL_DP_TRANS_P0_FLDMASK                      0x80
#define MST_DELAY_CYCLE_FLAG_SEL_DP_TRANS_P0_FLDMASK_POS                  7
#define MST_DELAY_CYCLE_FLAG_SEL_DP_TRANS_P0_FLDMASK_LEN                  1

#define TEST_CONFIG_HDCP22_DP_TRANS_P0_FLDMASK                            0xf00
#define TEST_CONFIG_HDCP22_DP_TRANS_P0_FLDMASK_POS                        8
#define TEST_CONFIG_HDCP22_DP_TRANS_P0_FLDMASK_LEN                        4

#define R0_AVAILABLE_DP_TRANS_P0_FLDMASK                                  0x1000
#define R0_AVAILABLE_DP_TRANS_P0_FLDMASK_POS                              12
#define R0_AVAILABLE_DP_TRANS_P0_FLDMASK_LEN                              1

#define DPES_TX_HDCP22_DP_TRANS_P0_FLDMASK                                0x2000
#define DPES_TX_HDCP22_DP_TRANS_P0_FLDMASK_POS                            13
#define DPES_TX_HDCP22_DP_TRANS_P0_FLDMASK_LEN                            1

#define DP_AES_OUT_RDY_L_DP_TRANS_P0_FLDMASK                              0x4000
#define DP_AES_OUT_RDY_L_DP_TRANS_P0_FLDMASK_POS                          14
#define DP_AES_OUT_RDY_L_DP_TRANS_P0_FLDMASK_LEN                          1

#define REPEATER_I_DP_TRANS_P0_FLDMASK                                    0x8000
#define REPEATER_I_DP_TRANS_P0_FLDMASK_POS                                15
#define REPEATER_I_DP_TRANS_P0_FLDMASK_LEN                                1

#define REG_34A8_DP_TRANS_P0              (0x34A8)
#define TEST_CONFIG_HDCP13_DP_TRANS_P0_FLDMASK                            0xff00
#define TEST_CONFIG_HDCP13_DP_TRANS_P0_FLDMASK_POS                        8
#define TEST_CONFIG_HDCP13_DP_TRANS_P0_FLDMASK_LEN                        8

#define REG_34D0_DP_TRANS_P0              (0x34D0)
#define TX_HDCP22_TYPE_DP_TRANS_P0_FLDMASK                                0xff
#define TX_HDCP22_TYPE_DP_TRANS_P0_FLDMASK_POS                            0
#define TX_HDCP22_TYPE_DP_TRANS_P0_FLDMASK_LEN                            8

#define PIPE_DELAY_EN_CNT_DP_TRANS_P0_FLDMASK                             0xf00
#define PIPE_DELAY_EN_CNT_DP_TRANS_P0_FLDMASK_POS                         8
#define PIPE_DELAY_EN_CNT_DP_TRANS_P0_FLDMASK_LEN                         4

#define PIPE_DELAY_DP_TRANS_P0_FLDMASK                                    0xf000
#define PIPE_DELAY_DP_TRANS_P0_FLDMASK_POS                                12
#define PIPE_DELAY_DP_TRANS_P0_FLDMASK_LEN                                4

#define REG_34D4_DP_TRANS_P0              (0x34D4)
#define DP_AES_INCTR_L_0_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_L_0_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_L_0_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_34D8_DP_TRANS_P0              (0x34D8)
#define DP_AES_INCTR_L_1_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_L_1_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_L_1_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_34DC_DP_TRANS_P0              (0x34DC)
#define DP_AES_INCTR_L_2_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_L_2_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_L_2_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_34E0_DP_TRANS_P0              (0x34E0)
#define DP_AES_INCTR_L_3_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_L_3_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_L_3_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_34E4_DP_TRANS_P0              (0x34E4)
#define HDCP_TYPE_TX_0_DP_TRANS_P0_FLDMASK                                0xffff
#define HDCP_TYPE_TX_0_DP_TRANS_P0_FLDMASK_POS                            0
#define HDCP_TYPE_TX_0_DP_TRANS_P0_FLDMASK_LEN                            16

#define REG_34E8_DP_TRANS_P0              (0x34E8)
#define HDCP_TYPE_TX_1_DP_TRANS_P0_FLDMASK                                0xffff
#define HDCP_TYPE_TX_1_DP_TRANS_P0_FLDMASK_POS                            0
#define HDCP_TYPE_TX_1_DP_TRANS_P0_FLDMASK_LEN                            16

#define REG_34EC_DP_TRANS_P0              (0x34EC)
#define HDCP_TYPE_TX_2_DP_TRANS_P0_FLDMASK                                0xffff
#define HDCP_TYPE_TX_2_DP_TRANS_P0_FLDMASK_POS                            0
#define HDCP_TYPE_TX_2_DP_TRANS_P0_FLDMASK_LEN                            16

#define REG_34F0_DP_TRANS_P0              (0x34F0)
#define HDCP_TYPE_TX_3_DP_TRANS_P0_FLDMASK                                0xffff
#define HDCP_TYPE_TX_3_DP_TRANS_P0_FLDMASK_POS                            0
#define HDCP_TYPE_TX_3_DP_TRANS_P0_FLDMASK_LEN                            16

#define REG_34F4_DP_TRANS_P0              (0x34F4)
#define SST_HDCP_TYPE_TX_DP_TRANS_P0_FLDMASK                              0xff
#define SST_HDCP_TYPE_TX_DP_TRANS_P0_FLDMASK_POS                          0
#define SST_HDCP_TYPE_TX_DP_TRANS_P0_FLDMASK_LEN                          8

#define PIPE_OV_VALUE_DP_TRANS_P0_FLDMASK                                 0xf00
#define PIPE_OV_VALUE_DP_TRANS_P0_FLDMASK_POS                             8
#define PIPE_OV_VALUE_DP_TRANS_P0_FLDMASK_LEN                             4

#define PIPE_OV_ENABLE_DP_TRANS_P0_FLDMASK                                0x1000
#define PIPE_OV_ENABLE_DP_TRANS_P0_FLDMASK_POS                            12
#define PIPE_OV_ENABLE_DP_TRANS_P0_FLDMASK_LEN                            1

#define REG_34F8_DP_TRANS_P0              (0x34F8)
#define DP_AES_OUT_RDY_H_DP_TRANS_P0_FLDMASK                              0x4000
#define DP_AES_OUT_RDY_H_DP_TRANS_P0_FLDMASK_POS                          14
#define DP_AES_OUT_RDY_H_DP_TRANS_P0_FLDMASK_LEN                          1

#define REG_34FC_DP_TRANS_P0              (0x34FC)
#define HDCP_4P_TO_2P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK                    0xff
#define HDCP_4P_TO_2P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK_POS                0
#define HDCP_4P_TO_2P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK_LEN                8

#define HDCP_2P_TO_4P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK                    0xff00
#define HDCP_2P_TO_4P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK_POS                8
#define HDCP_2P_TO_4P_FIFO_RST_CHK_DP_TRANS_P0_FLDMASK_LEN                8

#define REG_3500_DP_TRANS_P0              (0x3500)
#define DP_AES_INCTR_H_0_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_H_0_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_H_0_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_3504_DP_TRANS_P0              (0x3504)
#define DP_AES_INCTR_H_1_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_H_1_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_H_1_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_3508_DP_TRANS_P0              (0x3508)
#define DP_AES_INCTR_H_2_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_H_2_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_H_2_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_350C_DP_TRANS_P0              (0x350C)
#define DP_AES_INCTR_H_3_DP_TRANS_P0_FLDMASK                              0xffff
#define DP_AES_INCTR_H_3_DP_TRANS_P0_FLDMASK_POS                          0
#define DP_AES_INCTR_H_3_DP_TRANS_P0_FLDMASK_LEN                          16

#define REG_3510_DP_TRANS_P0              (0x3510)
#define HDCP22_TYPE_DP_TRANS_P0_FLDMASK                                   0xff
#define HDCP22_TYPE_DP_TRANS_P0_FLDMASK_POS                               0
#define HDCP22_TYPE_DP_TRANS_P0_FLDMASK_LEN                               8

#define REG_3540_DP_TRANS_P0              (0x3540)
#define FEC_EN_DP_TRANS_P0_FLDMASK                                        0x1
#define FEC_EN_DP_TRANS_P0_FLDMASK_POS                                    0
#define FEC_EN_DP_TRANS_P0_FLDMASK_LEN                                    1

#define FEC_END_MODE_DP_TRANS_P0_FLDMASK                                  0x6
#define FEC_END_MODE_DP_TRANS_P0_FLDMASK_POS                              1
#define FEC_END_MODE_DP_TRANS_P0_FLDMASK_LEN                              2

#define FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK                             0x8
#define FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK_POS                         3
#define FEC_CLOCK_EN_MODE_DP_TRANS_P0_FLDMASK_LEN                         1

#define FEC_FIFO_READ_START_DP_TRANS_P0_FLDMASK                           0xf0
#define FEC_FIFO_READ_START_DP_TRANS_P0_FLDMASK_POS                       4
#define FEC_FIFO_READ_START_DP_TRANS_P0_FLDMASK_LEN                       4

#define FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK                          0xf00
#define FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK_POS                      8
#define FEC_FIFO_UNDER_POINT_DP_TRANS_P0_FLDMASK_LEN                      4

#define FEC_FIFO_OVER_POINT_DP_TRANS_P0_FLDMASK                           0xf000
#define FEC_FIFO_OVER_POINT_DP_TRANS_P0_FLDMASK_POS                       12
#define FEC_FIFO_OVER_POINT_DP_TRANS_P0_FLDMASK_LEN                       4

#define REG_3544_DP_TRANS_P0              (0x3544)
#define FEC_FIFO_RST_DP_TRANS_P0_FLDMASK                                  0x1
#define FEC_FIFO_RST_DP_TRANS_P0_FLDMASK_POS                              0
#define FEC_FIFO_RST_DP_TRANS_P0_FLDMASK_LEN                              1

#define FEC_SUPPORT_DP_TRANS_P0_FLDMASK                                   0x2
#define FEC_SUPPORT_DP_TRANS_P0_FLDMASK_POS                               1
#define FEC_SUPPORT_DP_TRANS_P0_FLDMASK_LEN                               1

#define FEC_PATTERN_NEW_DP_TRANS_P0_FLDMASK                               0x4
#define FEC_PATTERN_NEW_DP_TRANS_P0_FLDMASK_POS                           2
#define FEC_PATTERN_NEW_DP_TRANS_P0_FLDMASK_LEN                           1

#define FEC_INSERT_FIFO_EMPTY_DP_TRANS_P0_FLDMASK                         0x10
#define FEC_INSERT_FIFO_EMPTY_DP_TRANS_P0_FLDMASK_POS                     4
#define FEC_INSERT_FIFO_EMPTY_DP_TRANS_P0_FLDMASK_LEN                     1

#define FEC_INSERT_FIFO_EMPTY_CLR_DP_TRANS_P0_FLDMASK                     0x20
#define FEC_INSERT_FIFO_EMPTY_CLR_DP_TRANS_P0_FLDMASK_POS                 5
#define FEC_INSERT_FIFO_EMPTY_CLR_DP_TRANS_P0_FLDMASK_LEN                 1

#define FEC_INSERT_FIFO_FULL_DP_TRANS_P0_FLDMASK                          0x40
#define FEC_INSERT_FIFO_FULL_DP_TRANS_P0_FLDMASK_POS                      6
#define FEC_INSERT_FIFO_FULL_DP_TRANS_P0_FLDMASK_LEN                      1

#define FEC_INSERT_FIFO_FULL_CLR_DP_TRANS_P0_FLDMASK                      0x80
#define FEC_INSERT_FIFO_FULL_CLR_DP_TRANS_P0_FLDMASK_POS                  7
#define FEC_INSERT_FIFO_FULL_CLR_DP_TRANS_P0_FLDMASK_LEN                  1

#define PARITY_INTERLEAVER_DATA_INVERT_PIPE_SEL_DP_TRANS_P0_FLDMASK       0x700
#define PARITY_INTERLEAVER_DATA_INVERT_PIPE_SEL_DP_TRANS_P0_FLDMASK_POS   8
#define PARITY_INTERLEAVER_DATA_INVERT_PIPE_SEL_DP_TRANS_P0_FLDMASK_LEN   3

#define PAT_INIT_DISPARITY_FEC_DP_TRANS_P0_FLDMASK                        0x800
#define PAT_INIT_DISPARITY_FEC_DP_TRANS_P0_FLDMASK_POS                    11
#define PAT_INIT_DISPARITY_FEC_DP_TRANS_P0_FLDMASK_LEN                    1

#define FEC_PARITY_DATA_LANE_SWAP_DP_TRANS_P0_FLDMASK                     0x1000
#define FEC_PARITY_DATA_LANE_SWAP_DP_TRANS_P0_FLDMASK_POS                 12
#define FEC_PARITY_DATA_LANE_SWAP_DP_TRANS_P0_FLDMASK_LEN                 1

#define REG_3548_DP_TRANS_P0              (0x3548)
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE0_DP_TRANS_P0_FLDMASK             0x7
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE0_DP_TRANS_P0_FLDMASK_POS         0
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE0_DP_TRANS_P0_FLDMASK_LEN         3

#define FEC_INSERT_SYMBOL_ERROR_LANE0_DP_TRANS_P0_FLDMASK                 0x8
#define FEC_INSERT_SYMBOL_ERROR_LANE0_DP_TRANS_P0_FLDMASK_POS             3
#define FEC_INSERT_SYMBOL_ERROR_LANE0_DP_TRANS_P0_FLDMASK_LEN             1

#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE1_DP_TRANS_P0_FLDMASK             0x70
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE1_DP_TRANS_P0_FLDMASK_POS         4
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE1_DP_TRANS_P0_FLDMASK_LEN         3

#define FEC_INSERT_SYMBOL_ERROR_LANE1_DP_TRANS_P0_FLDMASK                 0x80
#define FEC_INSERT_SYMBOL_ERROR_LANE1_DP_TRANS_P0_FLDMASK_POS             7
#define FEC_INSERT_SYMBOL_ERROR_LANE1_DP_TRANS_P0_FLDMASK_LEN             1

#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE2_DP_TRANS_P0_FLDMASK             0x700
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE2_DP_TRANS_P0_FLDMASK_POS         8
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE2_DP_TRANS_P0_FLDMASK_LEN         3

#define FEC_INSERT_SYMBOL_ERROR_LANE2_DP_TRANS_P0_FLDMASK                 0x800
#define FEC_INSERT_SYMBOL_ERROR_LANE2_DP_TRANS_P0_FLDMASK_POS             11
#define FEC_INSERT_SYMBOL_ERROR_LANE2_DP_TRANS_P0_FLDMASK_LEN             1

#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE3_DP_TRANS_P0_FLDMASK             0x7000
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE3_DP_TRANS_P0_FLDMASK_POS         12
#define FEC_INSERT_SYMBOL_ERROR_CNT_LANE3_DP_TRANS_P0_FLDMASK_LEN         3

#define FEC_INSERT_SYMBOL_ERROR_LANE3_DP_TRANS_P0_FLDMASK                 0x8000
#define FEC_INSERT_SYMBOL_ERROR_LANE3_DP_TRANS_P0_FLDMASK_POS             15
#define FEC_INSERT_SYMBOL_ERROR_LANE3_DP_TRANS_P0_FLDMASK_LEN             1

#define REG_354C_DP_TRANS_P0              (0x354C)
#define FEC_CP_HIT_LANE0_DP_TRANS_P0_FLDMASK                              0x1
#define FEC_CP_HIT_LANE0_DP_TRANS_P0_FLDMASK_POS                          0
#define FEC_CP_HIT_LANE0_DP_TRANS_P0_FLDMASK_LEN                          1

#define FEC_CP_HIT_LANE1_DP_TRANS_P0_FLDMASK                              0x2
#define FEC_CP_HIT_LANE1_DP_TRANS_P0_FLDMASK_POS                          1
#define FEC_CP_HIT_LANE1_DP_TRANS_P0_FLDMASK_LEN                          1

#define FEC_CP_HIT_LANE2_DP_TRANS_P0_FLDMASK                              0x4
#define FEC_CP_HIT_LANE2_DP_TRANS_P0_FLDMASK_POS                          2
#define FEC_CP_HIT_LANE2_DP_TRANS_P0_FLDMASK_LEN                          1

#define FEC_CP_HIT_LANE3_DP_TRANS_P0_FLDMASK                              0x8
#define FEC_CP_HIT_LANE3_DP_TRANS_P0_FLDMASK_POS                          3
#define FEC_CP_HIT_LANE3_DP_TRANS_P0_FLDMASK_LEN                          1

#define FEC_CP_HIT_CLR_DP_TRANS_P0_FLDMASK                                0x10
#define FEC_CP_HIT_CLR_DP_TRANS_P0_FLDMASK_POS                            4
#define FEC_CP_HIT_CLR_DP_TRANS_P0_FLDMASK_LEN                            1

#define FEC_ENCODE_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK                    0x300
#define FEC_ENCODE_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_POS                8
#define FEC_ENCODE_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_LEN                2

#define FEC_INSERT_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK                    0xc00
#define FEC_INSERT_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_POS                10
#define FEC_INSERT_TOP_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_LEN                2

#define REG_3550_DP_TRANS_P0              (0x3550)
#define FEC_INSERT_FIFO_WCNT_DP_TRANS_P0_FLDMASK                          0x1f
#define FEC_INSERT_FIFO_WCNT_DP_TRANS_P0_FLDMASK_POS                      0
#define FEC_INSERT_FIFO_WCNT_DP_TRANS_P0_FLDMASK_LEN                      5

#define FEC_INSERT_FIFO_RCNT_DP_TRANS_P0_FLDMASK                          0x1f00
#define FEC_INSERT_FIFO_RCNT_DP_TRANS_P0_FLDMASK_POS                      8
#define FEC_INSERT_FIFO_RCNT_DP_TRANS_P0_FLDMASK_LEN                      5

#define REG_3554_DP_TRANS_P0              (0x3554)
#define FEC_CLK_GATE_DATA_CNT_0_DP_TRANS_P0_FLDMASK                       0x7f
#define FEC_CLK_GATE_DATA_CNT_0_DP_TRANS_P0_FLDMASK_POS                   0
#define FEC_CLK_GATE_DATA_CNT_0_DP_TRANS_P0_FLDMASK_LEN                   7

#define REG_3558_DP_TRANS_P0              (0x3558)
#define FEC_CLK_GATE_DATA_CNT_1_0_DP_TRANS_P0_FLDMASK                     0xffff
#define FEC_CLK_GATE_DATA_CNT_1_0_DP_TRANS_P0_FLDMASK_POS                 0
#define FEC_CLK_GATE_DATA_CNT_1_0_DP_TRANS_P0_FLDMASK_LEN                 16

#define REG_355C_DP_TRANS_P0              (0x355C)
#define FEC_CLK_GATE_DATA_CNT_1_1_DP_TRANS_P0_FLDMASK                     0x3
#define FEC_CLK_GATE_DATA_CNT_1_1_DP_TRANS_P0_FLDMASK_POS                 0
#define FEC_CLK_GATE_DATA_CNT_1_1_DP_TRANS_P0_FLDMASK_LEN                 2

#define REG_3580_DP_TRANS_P0              (0x3580)
#define DP_TX_TRANS_TESTBUS_SEL_DP_TRANS_P0_FLDMASK                       0x1f
#define DP_TX_TRANS_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_POS                   0
#define DP_TX_TRANS_TESTBUS_SEL_DP_TRANS_P0_FLDMASK_LEN                   5

#define POST_MISC_DATA_LANE0_OV_DP_TRANS_P0_FLDMASK                       0x100
#define POST_MISC_DATA_LANE0_OV_DP_TRANS_P0_FLDMASK_POS                   8
#define POST_MISC_DATA_LANE0_OV_DP_TRANS_P0_FLDMASK_LEN                   1

#define POST_MISC_DATA_LANE1_OV_DP_TRANS_P0_FLDMASK                       0x200
#define POST_MISC_DATA_LANE1_OV_DP_TRANS_P0_FLDMASK_POS                   9
#define POST_MISC_DATA_LANE1_OV_DP_TRANS_P0_FLDMASK_LEN                   1

#define POST_MISC_DATA_LANE2_OV_DP_TRANS_P0_FLDMASK                       0x400
#define POST_MISC_DATA_LANE2_OV_DP_TRANS_P0_FLDMASK_POS                   10
#define POST_MISC_DATA_LANE2_OV_DP_TRANS_P0_FLDMASK_LEN                   1

#define POST_MISC_DATA_LANE3_OV_DP_TRANS_P0_FLDMASK                       0x800
#define POST_MISC_DATA_LANE3_OV_DP_TRANS_P0_FLDMASK_POS                   11
#define POST_MISC_DATA_LANE3_OV_DP_TRANS_P0_FLDMASK_LEN                   1

#define REG_3584_DP_TRANS_P0              (0x3584)
#define POST_MISC_DATA_LANE0_0_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE0_0_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE0_0_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_3588_DP_TRANS_P0              (0x3588)
#define POST_MISC_DATA_LANE0_1_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE0_1_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE0_1_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_358C_DP_TRANS_P0              (0x358C)
#define POST_MISC_DATA_LANE0_2_DP_TRANS_P0_FLDMASK                        0xff
#define POST_MISC_DATA_LANE0_2_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE0_2_DP_TRANS_P0_FLDMASK_LEN                    8

#define REG_3590_DP_TRANS_P0              (0x3590)
#define POST_MISC_DATA_LANE1_0_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE1_0_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE1_0_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_3594_DP_TRANS_P0              (0x3594)
#define POST_MISC_DATA_LANE1_1_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE1_1_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE1_1_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_3598_DP_TRANS_P0              (0x3598)
#define POST_MISC_DATA_LANE1_2_DP_TRANS_P0_FLDMASK                        0xff
#define POST_MISC_DATA_LANE1_2_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE1_2_DP_TRANS_P0_FLDMASK_LEN                    8

#define REG_359C_DP_TRANS_P0              (0x359C)
#define POST_MISC_DATA_LANE2_0_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE2_0_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE2_0_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_35A0_DP_TRANS_P0              (0x35A0)
#define POST_MISC_DATA_LANE2_1_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE2_1_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE2_1_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_35A4_DP_TRANS_P0              (0x35A4)
#define POST_MISC_DATA_LANE2_2_DP_TRANS_P0_FLDMASK                        0xff
#define POST_MISC_DATA_LANE2_2_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE2_2_DP_TRANS_P0_FLDMASK_LEN                    8

#define REG_35A8_DP_TRANS_P0              (0x35A8)
#define POST_MISC_DATA_LANE3_0_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE3_0_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE3_0_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_35AC_DP_TRANS_P0              (0x35AC)
#define POST_MISC_DATA_LANE3_1_DP_TRANS_P0_FLDMASK                        0xffff
#define POST_MISC_DATA_LANE3_1_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE3_1_DP_TRANS_P0_FLDMASK_LEN                    16

#define REG_35B0_DP_TRANS_P0              (0x35B0)
#define POST_MISC_DATA_LANE3_2_DP_TRANS_P0_FLDMASK                        0xff
#define POST_MISC_DATA_LANE3_2_DP_TRANS_P0_FLDMASK_POS                    0
#define POST_MISC_DATA_LANE3_2_DP_TRANS_P0_FLDMASK_LEN                    8

#define REG_35C0_DP_TRANS_P0              (0x35C0)
#define SW_IRQ_SRC_DP_TRANS_P0_FLDMASK                                    0xffff
#define SW_IRQ_SRC_DP_TRANS_P0_FLDMASK_POS                                0
#define SW_IRQ_SRC_DP_TRANS_P0_FLDMASK_LEN                                16

#define REG_35C4_DP_TRANS_P0              (0x35C4)
#define SW_IRQ_MASK_DP_TRANS_P0_FLDMASK                                   0xffff
#define SW_IRQ_MASK_DP_TRANS_P0_FLDMASK_POS                               0
#define SW_IRQ_MASK_DP_TRANS_P0_FLDMASK_LEN                               16

#define REG_35C8_DP_TRANS_P0              (0x35C8)
#define SW_IRQ_CLR_DP_TRANS_P0_FLDMASK                                    0xffff
#define SW_IRQ_CLR_DP_TRANS_P0_FLDMASK_POS                                0
#define SW_IRQ_CLR_DP_TRANS_P0_FLDMASK_LEN                                16

#define REG_35CC_DP_TRANS_P0              (0x35CC)
#define SW_IRQ_STATUS_DP_TRANS_P0_FLDMASK                                 0xffff
#define SW_IRQ_STATUS_DP_TRANS_P0_FLDMASK_POS                             0
#define SW_IRQ_STATUS_DP_TRANS_P0_FLDMASK_LEN                             16

#define REG_35D0_DP_TRANS_P0              (0x35D0)
#define SW_IRQ_FINAL_STATUS_DP_TRANS_P0_FLDMASK                           0xffff
#define SW_IRQ_FINAL_STATUS_DP_TRANS_P0_FLDMASK_POS                       0
#define SW_IRQ_FINAL_STATUS_DP_TRANS_P0_FLDMASK_LEN                       16

#define REG_35D4_DP_TRANS_P0              (0x35D4)
#define SW_IRQ_RAW_STATUS_DP_TRANS_P0_FLDMASK                             0xffff
#define SW_IRQ_RAW_STATUS_DP_TRANS_P0_FLDMASK_POS                         0
#define SW_IRQ_RAW_STATUS_DP_TRANS_P0_FLDMASK_LEN                         16

#define REG_35D8_DP_TRANS_P0              (0x35D8)
#define SW_IRQ_FORCE_DP_TRANS_P0_FLDMASK                                  0xffff
#define SW_IRQ_FORCE_DP_TRANS_P0_FLDMASK_POS                              0
#define SW_IRQ_FORCE_DP_TRANS_P0_FLDMASK_LEN                              16

#define REG_35F0_DP_TRANS_P0              (0x35F0)
#define DP_TRANSMITTER_DUMMY_RW_0_DP_TRANS_P0_FLDMASK                     0xffff
#define DP_TRANSMITTER_DUMMY_RW_0_DP_TRANS_P0_FLDMASK_POS                 0
#define DP_TRANSMITTER_DUMMY_RW_0_DP_TRANS_P0_FLDMASK_LEN                 16

#define REG_35F4_DP_TRANS_P0              (0x35F4)
#define DP_TRANSMITTER_DUMMY_RW_1_DP_TRANS_P0_FLDMASK                     0xffff
#define DP_TRANSMITTER_DUMMY_RW_1_DP_TRANS_P0_FLDMASK_POS                 0
#define DP_TRANSMITTER_DUMMY_RW_1_DP_TRANS_P0_FLDMASK_LEN                 16

#define REG_35F8_DP_TRANS_P0              (0x35F8)
#define DP_TRANSMITTER_DUMMY_R_0_DP_TRANS_P0_FLDMASK                      0xffff
#define DP_TRANSMITTER_DUMMY_R_0_DP_TRANS_P0_FLDMASK_POS                  0
#define DP_TRANSMITTER_DUMMY_R_0_DP_TRANS_P0_FLDMASK_LEN                  16

#define REG_35FC_DP_TRANS_P0              (0x35FC)
#define DP_TRANSMITTER_DUMMY_R_1_DP_TRANS_P0_FLDMASK                      0xffff
#define DP_TRANSMITTER_DUMMY_R_1_DP_TRANS_P0_FLDMASK_POS                  0
#define DP_TRANSMITTER_DUMMY_R_1_DP_TRANS_P0_FLDMASK_LEN                  16

#define REG_3600_AUX_TX_P0              (0x3600)
#define DP_TX_SW_RESET_AUX_TX_P0_FLDMASK                                  0x1
#define DP_TX_SW_RESET_AUX_TX_P0_FLDMASK_POS                              0
#define DP_TX_SW_RESET_AUX_TX_P0_FLDMASK_LEN                              1

#define AUX_TOP_RESET_AUX_TX_P0_FLDMASK                                   0x2
#define AUX_TOP_RESET_AUX_TX_P0_FLDMASK_POS                               1
#define AUX_TOP_RESET_AUX_TX_P0_FLDMASK_LEN                               1

#define SOFTWARE_RESET_RESERVED_AUX_TX_P0_FLDMASK                         0x1c
#define SOFTWARE_RESET_RESERVED_AUX_TX_P0_FLDMASK_POS                     2
#define SOFTWARE_RESET_RESERVED_AUX_TX_P0_FLDMASK_LEN                     3

#define AUX_CLK_EN_AUX_TX_P0_FLDMASK                                      0x100
#define AUX_CLK_EN_AUX_TX_P0_FLDMASK_POS                                  8
#define AUX_CLK_EN_AUX_TX_P0_FLDMASK_LEN                                  1

#define AUX_CLK_INV_AUX_TX_P0_FLDMASK                                     0x200
#define AUX_CLK_INV_AUX_TX_P0_FLDMASK_POS                                 9
#define AUX_CLK_INV_AUX_TX_P0_FLDMASK_LEN                                 1

#define AUX_CLK_SEL_AUX_TX_P0_FLDMASK                                     0xc00
#define AUX_CLK_SEL_AUX_TX_P0_FLDMASK_POS                                 10
#define AUX_CLK_SEL_AUX_TX_P0_FLDMASK_LEN                                 2

#define REG_3604_AUX_TX_P0              (0x3604)
#define AUX_TX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK                       0x8000
#define AUX_TX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_POS                   15
#define AUX_TX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_LEN                   1

#define AUX_TX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK                       0x4000
#define AUX_TX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_POS                   14
#define AUX_TX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_LEN                   1

#define AUX_RX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK                       0x2000
#define AUX_RX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_POS                   13
#define AUX_RX_FSM_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_LEN                   1

#define AUX_RX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK                       0x1000
#define AUX_RX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_POS                   12
#define AUX_RX_PHY_SOFTWARE_RESET_AUX_TX_P0_FLDMASK_LEN                   1

#define DP_TX_TESTBUS_SEL_AUX_TX_P0_FLDMASK                               0xff
#define DP_TX_TESTBUS_SEL_AUX_TX_P0_FLDMASK_POS                           0
#define DP_TX_TESTBUS_SEL_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3608_AUX_TX_P0              (0x3608)
#define DP_TX_INT_STATUS_AUX_TX_P0_FLDMASK                                0xffff
#define DP_TX_INT_STATUS_AUX_TX_P0_FLDMASK_POS                            0
#define DP_TX_INT_STATUS_AUX_TX_P0_FLDMASK_LEN                            16

#define REG_360C_AUX_TX_P0              (0x360C)
#define AUX_SWAP_AUX_TX_P0_FLDMASK                                        0x8000
#define AUX_SWAP_AUX_TX_P0_FLDMASK_POS                                    15
#define AUX_SWAP_AUX_TX_P0_FLDMASK_LEN                                    1

#define AUX_AUX_REPLY_MCU_AUX_TX_P0_FLDMASK                               0x4000
#define AUX_AUX_REPLY_MCU_AUX_TX_P0_FLDMASK_POS                           14
#define AUX_AUX_REPLY_MCU_AUX_TX_P0_FLDMASK_LEN                           1

#define AUX_TIMEOUT_CMP_MASK_AUX_TX_P0_FLDMASK                            0x2000
#define AUX_TIMEOUT_CMP_MASK_AUX_TX_P0_FLDMASK_POS                        13
#define AUX_TIMEOUT_CMP_MASK_AUX_TX_P0_FLDMASK_LEN                        1

#define AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK                                 0x1fff
#define AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK_POS                             0
#define AUX_TIMEOUT_THR_AUX_TX_P0_FLDMASK_LEN                             13

#define REG_3610_AUX_TX_P0              (0x3610)
#define AUX_EDID_REPLY_MCU_AUX_TX_P0_FLDMASK                              0x8000
#define AUX_EDID_REPLY_MCU_AUX_TX_P0_FLDMASK_POS                          15
#define AUX_EDID_REPLY_MCU_AUX_TX_P0_FLDMASK_LEN                          1

#define AUX_EDID_ADDR_AUX_TX_P0_FLDMASK                                   0x7f00
#define AUX_EDID_ADDR_AUX_TX_P0_FLDMASK_POS                               8
#define AUX_EDID_ADDR_AUX_TX_P0_FLDMASK_LEN                               7

#define AUX_MCCS_REPLY_MCU_AUX_TX_P0_FLDMASK                              0x80
#define AUX_MCCS_REPLY_MCU_AUX_TX_P0_FLDMASK_POS                          7
#define AUX_MCCS_REPLY_MCU_AUX_TX_P0_FLDMASK_LEN                          1

#define AUX_MCCS_ADDR_AUX_TX_P0_FLDMASK                                   0x7f
#define AUX_MCCS_ADDR_AUX_TX_P0_FLDMASK_POS                               0
#define AUX_MCCS_ADDR_AUX_TX_P0_FLDMASK_LEN                               7

#define REG_3614_AUX_TX_P0              (0x3614)
#define AUX_TIMEOUT_THR_EXTEN_AUX_TX_P0_FLDMASK                           0x4000
#define AUX_TIMEOUT_THR_EXTEN_AUX_TX_P0_FLDMASK_POS                       14
#define AUX_TIMEOUT_THR_EXTEN_AUX_TX_P0_FLDMASK_LEN                       1

#define AUX_RX_AVERAGE_SEL_AUX_TX_P0_FLDMASK                              0x3000
#define AUX_RX_AVERAGE_SEL_AUX_TX_P0_FLDMASK_POS                          12
#define AUX_RX_AVERAGE_SEL_AUX_TX_P0_FLDMASK_LEN                          2

#define AUX_RX_SYNC_PATTERN_THR_AUX_TX_P0_FLDMASK                         0xf00
#define AUX_RX_SYNC_PATTERN_THR_AUX_TX_P0_FLDMASK_POS                     8
#define AUX_RX_SYNC_PATTERN_THR_AUX_TX_P0_FLDMASK_LEN                     4

#define AUX_RX_DECODE_SEL_AUX_TX_P0_FLDMASK                               0x80
#define AUX_RX_DECODE_SEL_AUX_TX_P0_FLDMASK_POS                           7
#define AUX_RX_DECODE_SEL_AUX_TX_P0_FLDMASK_LEN                           1

#define AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK                               0x7f
#define AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_UI_CNT_THR_AUX_TX_P0_FLDMASK_LEN                           7

#define REG_3618_AUX_TX_P0              (0x3618)
#define AUX_RX_DP_REV_AUX_TX_P0_FLDMASK                                   0x400
#define AUX_RX_DP_REV_AUX_TX_P0_FLDMASK_POS                               10
#define AUX_RX_DP_REV_AUX_TX_P0_FLDMASK_LEN                               1

#define AUX_RX_FIFO_FULL_AUX_TX_P0_FLDMASK                                0x200
#define AUX_RX_FIFO_FULL_AUX_TX_P0_FLDMASK_POS                            9
#define AUX_RX_FIFO_FULL_AUX_TX_P0_FLDMASK_LEN                            1

#define AUX_RX_FIFO_EMPTY_AUX_TX_P0_FLDMASK                               0x100
#define AUX_RX_FIFO_EMPTY_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_EMPTY_AUX_TX_P0_FLDMASK_LEN                           1

#define AUX_RX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK                        0xf0
#define AUX_RX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK_POS                    4
#define AUX_RX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK_LEN                    4

#define AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK                       0xf
#define AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK_POS                   0
#define AUX_RX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK_LEN                   4

#define REG_361C_AUX_TX_P0              (0x361C)
#define AUX_RX_DATA_BYTE_CNT_AUX_TX_P0_FLDMASK                            0xff00
#define AUX_RX_DATA_BYTE_CNT_AUX_TX_P0_FLDMASK_POS                        8
#define AUX_RX_DATA_BYTE_CNT_AUX_TX_P0_FLDMASK_LEN                        8

#define AUX_RESERVED_RO_0_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RESERVED_RO_0_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RESERVED_RO_0_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3620_AUX_TX_P0              (0x3620)
#define AUX_RD_MODE_AUX_TX_P0_FLDMASK                                     0x200
#define AUX_RD_MODE_AUX_TX_P0_FLDMASK_POS                                 9
#define AUX_RD_MODE_AUX_TX_P0_FLDMASK_LEN                                 1

#define AUX_RX_FIFO_READ_PULSE_TX_P0_FLDMASK                          0x100
#define AUX_RX_FIFO_R_PULSE_TX_P0_FLDMASK_POS                      8
#define AUX_RX_FIFO_READ_PULSE_AUX_TX_P0_FLDMASK_LEN                      1

#define AUX_RX_FIFO_READ_DATA_AUX_TX_P0_FLDMASK                           0xff
#define AUX_RX_FIFO_READ_DATA_AUX_TX_P0_FLDMASK_POS                       0
#define AUX_RX_FIFO_READ_DATA_AUX_TX_P0_FLDMASK_LEN                       8

#define REG_3624_AUX_TX_P0              (0x3624)
#define AUX_RX_REPLY_COMMAND_AUX_TX_P0_FLDMASK                            0xf
#define AUX_RX_REPLY_COMMAND_AUX_TX_P0_FLDMASK_POS                        0
#define AUX_RX_REPLY_COMMAND_AUX_TX_P0_FLDMASK_LEN                        4

#define AUX_RX_REPLY_ADDRESS_NONE_AUX_TX_P0_FLDMASK                       0xf00
#define AUX_RX_REPLY_ADDRESS_NONE_AUX_TX_P0_FLDMASK_POS                   8
#define AUX_RX_REPLY_ADDRESS_NONE_AUX_TX_P0_FLDMASK_LEN                   4

#define REG_3628_AUX_TX_P0              (0x3628)
#define AUX_RESERVED_RO_1_AUX_TX_P0_FLDMASK                               0xfc00
#define AUX_RESERVED_RO_1_AUX_TX_P0_FLDMASK_POS                           10
#define AUX_RESERVED_RO_1_AUX_TX_P0_FLDMASK_LEN                           6

#define AUX_RX_PHY_STATE_AUX_TX_P0_FLDMASK                                0x3ff
#define AUX_RX_PHY_STATE_AUX_TX_P0_FLDMASK_POS                            0
#define AUX_RX_PHY_STATE_AUX_TX_P0_FLDMASK_LEN                            10

#define REG_362C_AUX_TX_P0              (0x362C)
#define AUX_NO_LENGTH_AUX_TX_P0_FLDMASK                                   0x1
#define AUX_NO_LENGTH_AUX_TX_P0_FLDMASK_POS                               0
#define AUX_NO_LENGTH_AUX_TX_P0_FLDMASK_LEN                               1

#define AUX_TX_AUXTX_OV_EN_AUX_TX_P0_FLDMASK                              0x2
#define AUX_TX_AUXTX_OV_EN_AUX_TX_P0_FLDMASK_POS                          1
#define AUX_TX_AUXTX_OV_EN_AUX_TX_P0_FLDMASK_LEN                          1

#define AUX_RESERVED_RW_0_AUX_TX_P0_FLDMASK                               0xfffc
#define AUX_RESERVED_RW_0_AUX_TX_P0_FLDMASK_POS                           2
#define AUX_RESERVED_RW_0_AUX_TX_P0_FLDMASK_LEN                           14

#define REG_3630_AUX_TX_P0              (0x3630)
#define AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK                            0x8
#define AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK_POS                        3
#define AUX_TX_REQUEST_READY_AUX_TX_P0_FLDMASK_LEN                        1

#define AUX_TX_PRE_NUM_AUX_TX_P0_FLDMASK                                  0xff00
#define AUX_TX_PRE_NUM_AUX_TX_P0_FLDMASK_POS                              8
#define AUX_TX_PRE_NUM_AUX_TX_P0_FLDMASK_LEN                              8

#define REG_3634_AUX_TX_P0              (0x3634)
#define AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK                         0xff00
#define AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK_POS                     8
#define AUX_TX_OVER_SAMPLE_RATE_AUX_TX_P0_FLDMASK_LEN                     8

#define AUX_TX_FIFO_WRITE_DATA_AUX_TX_P0_FLDMASK                          0xff
#define AUX_TX_FIFO_WRITE_DATA_AUX_TX_P0_FLDMASK_POS                      0
#define AUX_TX_FIFO_WRITE_DATA_AUX_TX_P0_FLDMASK_LEN                      8

#define REG_3638_AUX_TX_P0              (0x3638)
#define AUX_TX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK                        0xf0
#define AUX_TX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK_POS                    4
#define AUX_TX_FIFO_READ_POINTER_AUX_TX_P0_FLDMASK_LEN                    4

#define AUX_TX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK                       0xf
#define AUX_TX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK_POS                   0
#define AUX_TX_FIFO_WRITE_POINTER_AUX_TX_P0_FLDMASK_LEN                   4

#define REG_363C_AUX_TX_P0              (0x363C)
#define AUX_TX_FIFO_FULL_AUX_TX_P0_FLDMASK                                0x1000
#define AUX_TX_FIFO_FULL_AUX_TX_P0_FLDMASK_POS                            12
#define AUX_TX_FIFO_FULL_AUX_TX_P0_FLDMASK_LEN                            1

#define AUX_TX_FIFO_EMPTY_AUX_TX_P0_FLDMASK                               0x800
#define AUX_TX_FIFO_EMPTY_AUX_TX_P0_FLDMASK_POS                           11
#define AUX_TX_FIFO_EMPTY_AUX_TX_P0_FLDMASK_LEN                           1

#define AUX_TX_PHY_STATE_AUX_TX_P0_FLDMASK                                0x7ff
#define AUX_TX_PHY_STATE_AUX_TX_P0_FLDMASK_POS                            0
#define AUX_TX_PHY_STATE_AUX_TX_P0_FLDMASK_LEN                            11

#define REG_3640_AUX_TX_P0              (0x3640)
#define AUX_RX_RECV_COMPLETE_IRQ_TX_P0_FLDMASK                    0x40
#define AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_POS                6
#define AUX_RX_AUX_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_LEN                1

#define AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK                   0x20
#define AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_POS               5
#define AUX_RX_EDID_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_LEN               1

#define AUX_RX_MCCS_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK                   0x10
#define AUX_RX_MCCS_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_POS               4
#define AUX_RX_MCCS_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_LEN               1

#define AUX_RX_CMD_RECV_IRQ_AUX_TX_P0_FLDMASK                             0x8
#define AUX_RX_CMD_RECV_IRQ_AUX_TX_P0_FLDMASK_POS                         3
#define AUX_RX_CMD_RECV_IRQ_AUX_TX_P0_FLDMASK_LEN                         1

#define AUX_RX_ADDR_RECV_IRQ_AUX_TX_P0_FLDMASK                            0x4
#define AUX_RX_ADDR_RECV_IRQ_AUX_TX_P0_FLDMASK_POS                        2
#define AUX_RX_ADDR_RECV_IRQ_AUX_TX_P0_FLDMASK_LEN                        1

#define AUX_RX_DATA_RECV_IRQ_AUX_TX_P0_FLDMASK                            0x2
#define AUX_RX_DATA_RECV_IRQ_AUX_TX_P0_FLDMASK_POS                        1
#define AUX_RX_DATA_RECV_IRQ_AUX_TX_P0_FLDMASK_LEN                        1

#define AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK                           0x1
#define AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK_POS                       0
#define AUX_400US_TIMEOUT_IRQ_AUX_TX_P0_FLDMASK_LEN                       1

#define REG_3644_AUX_TX_P0              (0x3644)
#define MCU_REQUEST_COMMAND_AUX_TX_P0_FLDMASK                             0xf
#define MCU_REQUEST_COMMAND_AUX_TX_P0_FLDMASK_POS                         0
#define MCU_REQUEST_COMMAND_AUX_TX_P0_FLDMASK_LEN                         4

#define AUX_STATE_AUX_TX_P0_FLDMASK                                       0xf00
#define AUX_STATE_AUX_TX_P0_FLDMASK_POS                                   8
#define AUX_STATE_AUX_TX_P0_FLDMASK_LEN                                   4

#define REG_3648_AUX_TX_P0              (0x3648)
#define MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_FLDMASK                         0xffff
#define MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_FLDMASK_POS                     0
#define MCU_REQUEST_ADDRESS_LSB_AUX_TX_P0_FLDMASK_LEN                     16

#define REG_364C_AUX_TX_P0              (0x364C)
#define MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_FLDMASK                         0xf
#define MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_FLDMASK_POS                     0
#define MCU_REQUEST_ADDRESS_MSB_AUX_TX_P0_FLDMASK_LEN                     4

#define REG_3650_AUX_TX_P0              (0x3650)
#define MCU_REQUEST_DATA_NUM_AUX_TX_P0_FLDMASK                            0xf000
#define MCU_REQ_DATA_NUM_AUX_TX_P0_FLDMASK_POS                        12
#define MCU_REQUEST_DATA_NUM_AUX_TX_P0_FLDMASK_LEN                        4

#define PHY_FIFO_RST_AUX_TX_P0_FLDMASK                                    0x200
#define PHY_FIFO_RST_AUX_TX_P0_FLDMASK_POS                                9
#define PHY_FIFO_RST_AUX_TX_P0_FLDMASK_LEN                                1

#define MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK                    0x100
#define MCU_ACK_TRAN_COMPLETE_AUX_TX_P0_FLDMASK_POS                8
#define MCU_ACK_TRANSACTION_COMPLETE_AUX_TX_P0_FLDMASK_LEN                1

#define AUX_TEST_CONFIG_AUX_TX_P0_FLDMASK                                 0xff
#define AUX_TEST_CONFIG_AUX_TX_P0_FLDMASK_POS                             0
#define AUX_TEST_CONFIG_AUX_TX_P0_FLDMASK_LEN                             8

#define REG_3654_AUX_TX_P0              (0x3654)
#define TST_AUXRX_AUX_TX_P0_FLDMASK                                       0xff
#define TST_AUXRX_AUX_TX_P0_FLDMASK_POS                                   0
#define TST_AUXRX_AUX_TX_P0_FLDMASK_LEN                                   8

#define REG_3658_AUX_TX_P0              (0x3658)
#define AUX_TX_OV_EN_AUX_TX_P0_FLDMASK                                    0x1
#define AUX_TX_OV_EN_AUX_TX_P0_FLDMASK_POS                                0
#define AUX_TX_OV_EN_AUX_TX_P0_FLDMASK_LEN                                1

#define AUX_TX_VALUE_SET_AUX_TX_P0_FLDMASK                                0x2
#define AUX_TX_VALUE_SET_AUX_TX_P0_FLDMASK_POS                            1
#define AUX_TX_VALUE_SET_AUX_TX_P0_FLDMASK_LEN                            1

#define AUX_TX_OEN_SET_AUX_TX_P0_FLDMASK                                  0x4
#define AUX_TX_OEN_SET_AUX_TX_P0_FLDMASK_POS                              2
#define AUX_TX_OEN_SET_AUX_TX_P0_FLDMASK_LEN                              1

#define AUX_TX_OV_MODE_AUX_TX_P0_FLDMASK                                  0x8
#define AUX_TX_OV_MODE_AUX_TX_P0_FLDMASK_POS                              3
#define AUX_TX_OV_MODE_AUX_TX_P0_FLDMASK_LEN                              1

#define AUX_TX_OFF_AUX_TX_P0_FLDMASK                                      0x10
#define AUX_TX_OFF_AUX_TX_P0_FLDMASK_POS                                  4
#define AUX_TX_OFF_AUX_TX_P0_FLDMASK_LEN                                  1

#define EXT_AUX_PHY_MODE_AUX_TX_P0_FLDMASK                                0x20
#define EXT_AUX_PHY_MODE_AUX_TX_P0_FLDMASK_POS                            5
#define EXT_AUX_PHY_MODE_AUX_TX_P0_FLDMASK_LEN                            1

#define EXT_TX_OEN_POLARITY_AUX_TX_P0_FLDMASK                             0x40
#define EXT_TX_OEN_POLARITY_AUX_TX_P0_FLDMASK_POS                         6
#define EXT_TX_OEN_POLARITY_AUX_TX_P0_FLDMASK_LEN                         1

#define AUX_RX_OEN_SET_AUX_TX_P0_FLDMASK                                  0x80
#define AUX_RX_OEN_SET_AUX_TX_P0_FLDMASK_POS                              7
#define AUX_RX_OEN_SET_AUX_TX_P0_FLDMASK_LEN                              1

#define REG_365C_AUX_TX_P0              (0x365C)
#define AUX_RCTRL_AUX_TX_P0_FLDMASK                                       0x1f
#define AUX_RCTRL_AUX_TX_P0_FLDMASK_POS                                   0
#define AUX_RCTRL_AUX_TX_P0_FLDMASK_LEN                                   5

#define AUX_RPD_AUX_TX_P0_FLDMASK                                         0x20
#define AUX_RPD_AUX_TX_P0_FLDMASK_POS                                     5
#define AUX_RPD_AUX_TX_P0_FLDMASK_LEN                                     1

#define AUX_RX_SEL_AUX_TX_P0_FLDMASK                                      0x40
#define AUX_RX_SEL_AUX_TX_P0_FLDMASK_POS                                  6
#define AUX_RX_SEL_AUX_TX_P0_FLDMASK_LEN                                  1

#define AUXRX_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK                              0x80
#define AUXRX_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_POS                          7
#define AUXRX_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_LEN                          1

#define AUXRXVALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK                         0x100
#define AUXRXVALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_POS                     8
#define AUXRXVALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_LEN                     1

#define AUX_DEBOUNCE_CLKSEL_AUX_TX_P0_FLDMASK                             0xe00
#define AUX_DEBOUNCE_CLKSEL_AUX_TX_P0_FLDMASK_POS                         9
#define AUX_DEBOUNCE_CLKSEL_AUX_TX_P0_FLDMASK_LEN                         3

#define DATA_VALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK                         0x1000
#define DATA_VALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_POS                     12
#define DATA_VALID_DEBOUNCE_SEL_AUX_TX_P0_FLDMASK_LEN                     1

#define REG_3660_AUX_TX_P0              (0x3660)
#define DP_TX_INT_MASK_AUX_TX_P0_FLDMASK                                  0xffff
#define DP_TX_INT_MASK_AUX_TX_P0_FLDMASK_POS                              0
#define DP_TX_INT_MASK_AUX_TX_P0_FLDMASK_LEN                              16

#define REG_3664_AUX_TX_P0              (0x3664)
#define DP_TX_INT_FORCE_AUX_TX_P0_FLDMASK                                 0xffff
#define DP_TX_INT_FORCE_AUX_TX_P0_FLDMASK_POS                             0
#define DP_TX_INT_FORCE_AUX_TX_P0_FLDMASK_LEN                             16

#define REG_3668_AUX_TX_P0              (0x3668)
#define DP_TX_INT_CLR_AUX_TX_P0_FLDMASK                                   0xffff
#define DP_TX_INT_CLR_AUX_TX_P0_FLDMASK_POS                               0
#define DP_TX_INT_CLR_AUX_TX_P0_FLDMASK_LEN                               16

#define REG_366C_AUX_TX_P0              (0x366C)
#define XTAL_FREQ_AUX_TX_P0_FLDMASK                                       0xff00
#define XTAL_FREQ_AUX_TX_P0_FLDMASK_POS                                   8
#define XTAL_FREQ_AUX_TX_P0_FLDMASK_LEN                                   8

#define REG_3670_AUX_TX_P0              (0x3670)
#define DPTX_GPIO_OEN_AUX_TX_P0_FLDMASK                                   0x7
#define DPTX_GPIO_OEN_AUX_TX_P0_FLDMASK_POS                               0
#define DPTX_GPIO_OEN_AUX_TX_P0_FLDMASK_LEN                               3

#define DPTX_GPIO_OUT_AUX_TX_P0_FLDMASK                                   0x38
#define DPTX_GPIO_OUT_AUX_TX_P0_FLDMASK_POS                               3
#define DPTX_GPIO_OUT_AUX_TX_P0_FLDMASK_LEN                               3

#define DPTX_GPIO_IN_AUX_TX_P0_FLDMASK                                    0x1c0
#define DPTX_GPIO_IN_AUX_TX_P0_FLDMASK_POS                                6
#define DPTX_GPIO_IN_AUX_TX_P0_FLDMASK_LEN                                3

#define AUX_IN_AUX_TX_P0_FLDMASK                                          0x200
#define AUX_IN_AUX_TX_P0_FLDMASK_POS                                      9
#define AUX_IN_AUX_TX_P0_FLDMASK_LEN                                      1

#define PD_AUX_RTERM_AUX_TX_P0_FLDMASK                                    0x400
#define PD_AUX_RTERM_AUX_TX_P0_FLDMASK_POS                                10
#define PD_AUX_RTERM_AUX_TX_P0_FLDMASK_LEN                                1

#define DPTX_GPIO_EN_AUX_TX_P0_FLDMASK                                    0x7000
#define DPTX_GPIO_EN_AUX_TX_P0_FLDMASK_POS                                12
#define DPTX_GPIO_EN_AUX_TX_P0_FLDMASK_LEN                                3

#define REG_3674_AUX_TX_P0              (0x3674)
#define AUXTX_ISEL_AUX_TX_P0_FLDMASK                                      0x1f
#define AUXTX_ISEL_AUX_TX_P0_FLDMASK_POS                                  0
#define AUXTX_ISEL_AUX_TX_P0_FLDMASK_LEN                                  5

#define AUXRX_VTH_AUX_TX_P0_FLDMASK                                       0x60
#define AUXRX_VTH_AUX_TX_P0_FLDMASK_POS                                   5
#define AUXRX_VTH_AUX_TX_P0_FLDMASK_LEN                                   2

#define EN_RXCM_BOOST_AUX_TX_P0_FLDMASK                                   0x80
#define EN_RXCM_BOOST_AUX_TX_P0_FLDMASK_POS                               7
#define EN_RXCM_BOOST_AUX_TX_P0_FLDMASK_LEN                               1

#define DPTX_AUX_R_CTRL_AUX_TX_P0_FLDMASK                                 0x1f00
#define DPTX_AUX_R_CTRL_AUX_TX_P0_FLDMASK_POS                             8
#define DPTX_AUX_R_CTRL_AUX_TX_P0_FLDMASK_LEN                             5

#define I2C_EN_AUXN_AUX_TX_P0_FLDMASK                                     0x2000
#define I2C_EN_AUXN_AUX_TX_P0_FLDMASK_POS                                 13
#define I2C_EN_AUXN_AUX_TX_P0_FLDMASK_LEN                                 1

#define I2C_EN_AUXP_AUX_TX_P0_FLDMASK                                     0x4000
#define I2C_EN_AUXP_AUX_TX_P0_FLDMASK_POS                                 14
#define I2C_EN_AUXP_AUX_TX_P0_FLDMASK_LEN                                 1

#define REG_3678_AUX_TX_P0              (0x3678)
#define TEST_AUXTX_AUX_TX_P0_FLDMASK                                      0xff00
#define TEST_AUXTX_AUX_TX_P0_FLDMASK_POS                                  8
#define TEST_AUXTX_AUX_TX_P0_FLDMASK_LEN                                  8

#define REG_367C_AUX_TX_P0              (0x367C)
#define DPTX_AUXRX_AUX_TX_P0_FLDMASK                                      0x4
#define DPTX_AUXRX_AUX_TX_P0_FLDMASK_POS                                  2
#define DPTX_AUXRX_AUX_TX_P0_FLDMASK_LEN                                  1

#define DPTX_AUXRX_VALID_AUX_TX_P0_FLDMASK                                0x8
#define DPTX_AUXRX_VALID_AUX_TX_P0_FLDMASK_POS                            3
#define DPTX_AUXRX_VALID_AUX_TX_P0_FLDMASK_LEN                            1

#define DPTX_AUXRX_WO_TH_AUX_TX_P0_FLDMASK                                0x10
#define DPTX_AUXRX_WO_TH_AUX_TX_P0_FLDMASK_POS                            4
#define DPTX_AUXRX_WO_TH_AUX_TX_P0_FLDMASK_LEN                            1

#define DPTX_AUXRX_L_TEST_AUX_TX_P0_FLDMASK                               0x20
#define DPTX_AUXRX_L_TEST_AUX_TX_P0_FLDMASK_POS                           5
#define DPTX_AUXRX_L_TEST_AUX_TX_P0_FLDMASK_LEN                           1

#define EN_AUXRX_AUX_TX_P0_FLDMASK                                        0x400
#define EN_AUXRX_AUX_TX_P0_FLDMASK_POS                                    10
#define EN_AUXRX_AUX_TX_P0_FLDMASK_LEN                                    1

#define EN_AUXTX_AUX_TX_P0_FLDMASK                                        0x800
#define EN_AUXTX_AUX_TX_P0_FLDMASK_POS                                    11
#define EN_AUXTX_AUX_TX_P0_FLDMASK_LEN                                    1

#define EN_AUX_AUX_TX_P0_FLDMASK                                          0x1000
#define EN_AUX_AUX_TX_P0_FLDMASK_POS                                      12
#define EN_AUX_AUX_TX_P0_FLDMASK_LEN                                      1

#define EN_5V_TOL_AUX_TX_P0_FLDMASK                                       0x2000
#define EN_5V_TOL_AUX_TX_P0_FLDMASK_POS                                   13
#define EN_5V_TOL_AUX_TX_P0_FLDMASK_LEN                                   1

#define AUXP_I_AUX_TX_P0_FLDMASK                                          0x4000
#define AUXP_I_AUX_TX_P0_FLDMASK_POS                                      14
#define AUXP_I_AUX_TX_P0_FLDMASK_LEN                                      1

#define AUXN_I_AUX_TX_P0_FLDMASK                                          0x8000
#define AUXN_I_AUX_TX_P0_FLDMASK_POS                                      15
#define AUXN_I_AUX_TX_P0_FLDMASK_LEN                                      1

#define REG_3680_AUX_TX_P0              (0x3680)
#define AUX_SWAP_TX_AUX_TX_P0_FLDMASK                                     0x1
#define AUX_SWAP_TX_AUX_TX_P0_FLDMASK_POS                                 0
#define AUX_SWAP_TX_AUX_TX_P0_FLDMASK_LEN                                 1

#define REG_3684_AUX_TX_P0              (0x3684)
#define TEST_IO_LOOPBK_AUX_TX_P0_FLDMASK                                  0x1f
#define TEST_IO_LOOPBK_AUX_TX_P0_FLDMASK_POS                              0
#define TEST_IO_LOOPBK_AUX_TX_P0_FLDMASK_LEN                              5

#define RO_IO_LOOPBKT_AUX_TX_P0_FLDMASK                                   0x300
#define RO_IO_LOOPBKT_AUX_TX_P0_FLDMASK_POS                               8
#define RO_IO_LOOPBKT_AUX_TX_P0_FLDMASK_LEN                               2

#define SEL_TCLK_AUX_TX_P0_FLDMASK                                        0x3000
#define SEL_TCLK_AUX_TX_P0_FLDMASK_POS                                    12
#define SEL_TCLK_AUX_TX_P0_FLDMASK_LEN                                    2

#define TESTEN_ASIO_AUX_TX_P0_FLDMASK                                     0x4000
#define TESTEN_ASIO_AUX_TX_P0_FLDMASK_POS                                 14
#define TESTEN_ASIO_AUX_TX_P0_FLDMASK_LEN                                 1

#define REG_3688_AUX_TX_P0              (0x3688)
#define TEST_AUXRX_VTH_AUX_TX_P0_FLDMASK                                  0x7
#define TEST_AUXRX_VTH_AUX_TX_P0_FLDMASK_POS                              0
#define TEST_AUXRX_VTH_AUX_TX_P0_FLDMASK_LEN                              3

#define REG_368C_AUX_TX_P0              (0x368C)
#define RX_FIFO_DONE_AUX_TX_P0_FLDMASK                                    0x1
#define RX_FIFO_DONE_AUX_TX_P0_FLDMASK_POS                                0
#define RX_FIFO_DONE_AUX_TX_P0_FLDMASK_LEN                                1

#define RX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK                                0x2
#define RX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK_POS                            1
#define RX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK_LEN                            1

#define TX_FIFO_DONE_AUX_TX_P0_FLDMASK                                    0x4
#define TX_FIFO_DONE_AUX_TX_P0_FLDMASK_POS                                2
#define TX_FIFO_DONE_AUX_TX_P0_FLDMASK_LEN                                1

#define TX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK                                0x8
#define TX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK_POS                            3
#define TX_FIFO_DONE_CLR_AUX_TX_P0_FLDMASK_LEN                            1

#define REG_3690_AUX_TX_P0              (0x3690)
#define DATA_LOW_CNT_THRD_AUX_TX_P0_FLDMASK                               0x7f
#define DATA_LOW_CNT_THRD_AUX_TX_P0_FLDMASK_POS                           0
#define DATA_LOW_CNT_THRD_AUX_TX_P0_FLDMASK_LEN                           7

#define RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK                          0x100
#define RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK_POS                      8
#define RX_REPLY_COMPLETE_MODE_AUX_TX_P0_FLDMASK_LEN                      1

#define REG_36C0_AUX_TX_P0              (0x36C0)
#define RX_GTC_VALUE_0_AUX_TX_P0_FLDMASK                                  0xffff
#define RX_GTC_VALUE_0_AUX_TX_P0_FLDMASK_POS                              0
#define RX_GTC_VALUE_0_AUX_TX_P0_FLDMASK_LEN                              16

#define REG_36C4_AUX_TX_P0              (0x36C4)
#define RX_GTC_VALUE_1_AUX_TX_P0_FLDMASK                                  0xffff
#define RX_GTC_VALUE_1_AUX_TX_P0_FLDMASK_POS                              0
#define RX_GTC_VALUE_1_AUX_TX_P0_FLDMASK_LEN                              16

#define REG_36C8_AUX_TX_P0              (0x36C8)
#define RX_GTC_MASTER_REQ_AUX_TX_P0_FLDMASK                               0x1
#define RX_GTC_MASTER_REQ_AUX_TX_P0_FLDMASK_POS                           0
#define RX_GTC_MASTER_REQ_AUX_TX_P0_FLDMASK_LEN                           1

#define TX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK                      0x2
#define TX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK_POS                  1
#define TX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK_LEN                  1

#define RX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK                           0x4
#define RX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK_POS                       2
#define RX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK_LEN                       1

#define REG_36CC_AUX_TX_P0              (0x36CC)
#define RX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK                        0xffff
#define RX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK_POS                    0
#define RX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK_LEN                    16

#define REG_36D0_AUX_TX_P0              (0x36D0)
#define TX_GTC_VALUE_0_AUX_TX_P0_FLDMASK                                  0xffff
#define TX_GTC_VALUE_0_AUX_TX_P0_FLDMASK_POS                              0
#define TX_GTC_VALUE_0_AUX_TX_P0_FLDMASK_LEN                              16

#define REG_36D4_AUX_TX_P0              (0x36D4)
#define TX_GTC_VALUE_1_AUX_TX_P0_FLDMASK                                  0xffff
#define TX_GTC_VALUE_1_AUX_TX_P0_FLDMASK_POS                              0
#define TX_GTC_VALUE_1_AUX_TX_P0_FLDMASK_LEN                              16

#define REG_36D8_AUX_TX_P0              (0x36D8)
#define RX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK                      0x1
#define RX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK_POS                  0
#define RX_GTC_VALUE_PHASE_SKEW_EN_AUX_TX_P0_FLDMASK_LEN                  1

#define TX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK                           0x2
#define TX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK_POS                       1
#define TX_GTC_FREQ_LOCK_DONE_AUX_TX_P0_FLDMASK_LEN                       1

#define TX_GTC_VALUE_PHASE_ADJUST_EN_AUX_TX_P0_FLDMASK                    0x4
#define TX_GTC_VALUE_PHASE_ADJUST_EN_AUX_TX_P0_FLDMASK_POS                2
#define TX_GTC_VALUE_PHASE_ADJUST_EN_AUX_TX_P0_FLDMASK_LEN                1

#define REG_36DC_AUX_TX_P0              (0x36DC)
#define TX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK                        0xffff
#define TX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK_POS                    0
#define TX_GTC_PHASE_SKEW_OFFSET_AUX_TX_P0_FLDMASK_LEN                    16

#define REG_36E0_AUX_TX_P0              (0x36E0)
#define GTC_STATE_AUX_TX_P0_FLDMASK                                       0xf
#define GTC_STATE_AUX_TX_P0_FLDMASK_POS                                   0
#define GTC_STATE_AUX_TX_P0_FLDMASK_LEN                                   4

#define RX_MASTER_LOCK_ACCQUI_CHKTIME_AUX_TX_P0_FLDMASK                   0xf0
#define RX_MASTER_LOCK_ACCQUI_CHKTIME_AUX_TX_P0_FLDMASK_POS               4
#define RX_MASTER_LOCK_ACCQUI_CHKTIME_AUX_TX_P0_FLDMASK_LEN               4

#define FREQ_AUX_TX_P0_FLDMASK                                            0xff00
#define FREQ_AUX_TX_P0_FLDMASK_POS                                        8
#define FREQ_AUX_TX_P0_FLDMASK_LEN                                        8

#define REG_36E4_AUX_TX_P0              (0x36E4)
#define GTC_TX_1M_ADD_VAL_AUX_TX_P0_FLDMASK                               0x3ff
#define GTC_TX_1M_ADD_VAL_AUX_TX_P0_FLDMASK_POS                           0
#define GTC_TX_1M_ADD_VAL_AUX_TX_P0_FLDMASK_LEN                           10

#define GTC_TX_10M_ADD_VAL_AUX_TX_P0_FLDMASK                              0xf000
#define GTC_TX_10M_ADD_VAL_AUX_TX_P0_FLDMASK_POS                          12
#define GTC_TX_10M_ADD_VAL_AUX_TX_P0_FLDMASK_LEN                          4

#define REG_36E8_AUX_TX_P0              (0x36E8)
#define CHK_TX_PH_ADJUST_CHK_EN_AUX_TX_P0_FLDMASK                         0x1
#define CHK_TX_PH_ADJUST_CHK_EN_AUX_TX_P0_FLDMASK_POS                     0
#define CHK_TX_PH_ADJUST_CHK_EN_AUX_TX_P0_FLDMASK_LEN                     1

#define TX_SLAVE_WAIT_SKEW_EN_AUX_TX_P0_FLDMASK                           0x2
#define TX_SLAVE_WAIT_SKEW_EN_AUX_TX_P0_FLDMASK_POS                       1
#define TX_SLAVE_WAIT_SKEW_EN_AUX_TX_P0_FLDMASK_LEN                       1

#define GTC_SEND_RCV_EN_AUX_TX_P0_FLDMASK                                 0x4
#define GTC_SEND_RCV_EN_AUX_TX_P0_FLDMASK_POS                             2
#define GTC_SEND_RCV_EN_AUX_TX_P0_FLDMASK_LEN                             1

#define AUXTX_HW_ACCS_EN_AUX_TX_P0_FLDMASK                                0x8
#define AUXTX_HW_ACCS_EN_AUX_TX_P0_FLDMASK_POS                            3
#define AUXTX_HW_ACCS_EN_AUX_TX_P0_FLDMASK_LEN                            1

#define GTC_TX_MASTER_EN_AUX_TX_P0_FLDMASK                                0x10
#define GTC_TX_MASTER_EN_AUX_TX_P0_FLDMASK_POS                            4
#define GTC_TX_MASTER_EN_AUX_TX_P0_FLDMASK_LEN                            1

#define GTC_TX_SLAVE_EN_AUX_TX_P0_FLDMASK                                 0x20
#define GTC_TX_SLAVE_EN_AUX_TX_P0_FLDMASK_POS                             5
#define GTC_TX_SLAVE_EN_AUX_TX_P0_FLDMASK_LEN                             1

#define OFFSET_TRY_NUM_AUX_TX_P0_FLDMASK                                  0xf00
#define OFFSET_TRY_NUM_AUX_TX_P0_FLDMASK_POS                              8
#define OFFSET_TRY_NUM_AUX_TX_P0_FLDMASK_LEN                              4

#define HW_SW_ARBIT_AUX_TX_P0_FLDMASK                                     0xc000
#define HW_SW_ARBIT_AUX_TX_P0_FLDMASK_POS                                 14
#define HW_SW_ARBIT_AUX_TX_P0_FLDMASK_LEN                                 2

#define REG_36EC_AUX_TX_P0              (0x36EC)
#define GTC_DB_OPTION_AUX_TX_P0_FLDMASK                                   0x7
#define GTC_DB_OPTION_AUX_TX_P0_FLDMASK_POS                               0
#define GTC_DB_OPTION_AUX_TX_P0_FLDMASK_LEN                               3

#define TX_SLAVE_CHK_RX_LCK_EN_AUX_TX_P0_FLDMASK                          0x8
#define TX_SLAVE_CHK_RX_LCK_EN_AUX_TX_P0_FLDMASK_POS                      3
#define TX_SLAVE_CHK_RX_LCK_EN_AUX_TX_P0_FLDMASK_LEN                      1

#define GTC_PUL_DELAY_AUX_TX_P0_FLDMASK                                   0xff00
#define GTC_PUL_DELAY_AUX_TX_P0_FLDMASK_POS                               8
#define GTC_PUL_DELAY_AUX_TX_P0_FLDMASK_LEN                               8

#define REG_36F0_AUX_TX_P0              (0x36F0)
#define GTC_TX_LCK_ACQ_SEND_NUM_AUX_TX_P0_FLDMASK                         0x1f
#define GTC_TX_LCK_ACQ_SEND_NUM_AUX_TX_P0_FLDMASK_POS                     0
#define GTC_TX_LCK_ACQ_SEND_NUM_AUX_TX_P0_FLDMASK_LEN                     5

#define REG_3700_AUX_TX_P0              (0x3700)
#define AUX_PHYWAKE_MODE_AUX_TX_P0_FLDMASK                                0x1
#define AUX_PHYWAKE_MODE_AUX_TX_P0_FLDMASK_POS                            0
#define AUX_PHYWAKE_MODE_AUX_TX_P0_FLDMASK_LEN                            1

#define AUX_PHYWAKE_ONLY_AUX_TX_P0_FLDMASK                                0x2
#define AUX_PHYWAKE_ONLY_AUX_TX_P0_FLDMASK_POS                            1
#define AUX_PHYWAKE_ONLY_AUX_TX_P0_FLDMASK_LEN                            1

#define PHYWAKE_PRE_NUM_AUX_TX_P0_FLDMASK                                 0x70
#define PHYWAKE_PRE_NUM_AUX_TX_P0_FLDMASK_POS                             4
#define PHYWAKE_PRE_NUM_AUX_TX_P0_FLDMASK_LEN                             3

#define REG_3704_AUX_TX_P0              (0x3704)
#define AUX_PHYWAKE_ACK_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK               0x1
#define AUX_PHYWAKE_ACK_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_POS           0
#define AUX_PHYWAKE_ACK_RECV_COMPLETE_IRQ_AUX_TX_P0_FLDMASK_LEN           1

#define AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK          0x2
#define AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK_POS      1
#define AUX_TX_FIFO_WRITE_DATA_NEW_MODE_TOGGLE_AUX_TX_P0_FLDMASK_LEN      1

#define AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK                         0x4
#define AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK_POS                     2
#define AUX_TX_FIFO_NEW_MODE_EN_AUX_TX_P0_FLDMASK_LEN                     1

#define REG_3708_AUX_TX_P0              (0x3708)
#define AUX_TX_FIFO_WRITE_DATA_BYTE0_AUX_TX_P0_FLDMASK                    0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE0_AUX_TX_P0_FLDMASK_POS                0
#define AUX_TX_FIFO_WRITE_DATA_BYTE0_AUX_TX_P0_FLDMASK_LEN                8

#define AUX_TX_FIFO_WRITE_DATA_BYTE1_AUX_TX_P0_FLDMASK                    0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE1_AUX_TX_P0_FLDMASK_POS                8
#define AUX_TX_FIFO_WRITE_DATA_BYTE1_AUX_TX_P0_FLDMASK_LEN                8

#define REG_370C_AUX_TX_P0              (0x370C)
#define AUX_TX_FIFO_WRITE_DATA_BYTE2_AUX_TX_P0_FLDMASK                    0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE2_AUX_TX_P0_FLDMASK_POS                0
#define AUX_TX_FIFO_WRITE_DATA_BYTE2_AUX_TX_P0_FLDMASK_LEN                8

#define AUX_TX_FIFO_WRITE_DATA_BYTE3_AUX_TX_P0_FLDMASK                    0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE3_AUX_TX_P0_FLDMASK_POS                8
#define AUX_TX_FIFO_WRITE_DATA_BYTE3_AUX_TX_P0_FLDMASK_LEN                8

#define REG_3710_AUX_TX_P0              (0x3710)
#define AUX_TX_FIFO_WRITE_DATA_BYTE4_AUX_TX_P0_FLDMASK                    0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE4_AUX_TX_P0_FLDMASK_POS                0
#define AUX_TX_FIFO_WRITE_DATA_BYTE4_AUX_TX_P0_FLDMASK_LEN                8

#define AUX_TX_FIFO_WRITE_DATA_BYTE5_AUX_TX_P0_FLDMASK                    0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE5_AUX_TX_P0_FLDMASK_POS                8
#define AUX_TX_FIFO_WRITE_DATA_BYTE5_AUX_TX_P0_FLDMASK_LEN                8

#define REG_3714_AUX_TX_P0              (0x3714)
#define AUX_TX_FIFO_WRITE_DATA_BYTE6_AUX_TX_P0_FLDMASK                    0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE6_AUX_TX_P0_FLDMASK_POS                0
#define AUX_TX_FIFO_WRITE_DATA_BYTE6_AUX_TX_P0_FLDMASK_LEN                8

#define AUX_TX_FIFO_WRITE_DATA_BYTE7_AUX_TX_P0_FLDMASK                    0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE7_AUX_TX_P0_FLDMASK_POS                8
#define AUX_TX_FIFO_WRITE_DATA_BYTE7_AUX_TX_P0_FLDMASK_LEN                8

#define REG_3718_AUX_TX_P0              (0x3718)
#define AUX_TX_FIFO_WRITE_DATA_BYTE8_AUX_TX_P0_FLDMASK                    0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE8_AUX_TX_P0_FLDMASK_POS                0
#define AUX_TX_FIFO_WRITE_DATA_BYTE8_AUX_TX_P0_FLDMASK_LEN                8

#define AUX_TX_FIFO_WRITE_DATA_BYTE9_AUX_TX_P0_FLDMASK                    0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE9_AUX_TX_P0_FLDMASK_POS                8
#define AUX_TX_FIFO_WRITE_DATA_BYTE9_AUX_TX_P0_FLDMASK_LEN                8

#define REG_371C_AUX_TX_P0              (0x371C)
#define AUX_TX_FIFO_WRITE_DATA_BYTE10_AUX_TX_P0_FLDMASK                   0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE10_AUX_TX_P0_FLDMASK_POS               0
#define AUX_TX_FIFO_WRITE_DATA_BYTE10_AUX_TX_P0_FLDMASK_LEN               8

#define AUX_TX_FIFO_WRITE_DATA_BYTE11_AUX_TX_P0_FLDMASK                   0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE11_AUX_TX_P0_FLDMASK_POS               8
#define AUX_TX_FIFO_WRITE_DATA_BYTE11_AUX_TX_P0_FLDMASK_LEN               8

#define REG_3720_AUX_TX_P0              (0x3720)
#define AUX_TX_FIFO_WRITE_DATA_BYTE12_AUX_TX_P0_FLDMASK                   0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE12_AUX_TX_P0_FLDMASK_POS               0
#define AUX_TX_FIFO_WRITE_DATA_BYTE12_AUX_TX_P0_FLDMASK_LEN               8

#define AUX_TX_FIFO_WRITE_DATA_BYTE13_AUX_TX_P0_FLDMASK                   0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE13_AUX_TX_P0_FLDMASK_POS               8
#define AUX_TX_FIFO_WRITE_DATA_BYTE13_AUX_TX_P0_FLDMASK_LEN               8

#define REG_3724_AUX_TX_P0              (0x3724)
#define AUX_TX_FIFO_WRITE_DATA_BYTE14_AUX_TX_P0_FLDMASK                   0xff
#define AUX_TX_FIFO_WRITE_DATA_BYTE14_AUX_TX_P0_FLDMASK_POS               0
#define AUX_TX_FIFO_WRITE_DATA_BYTE14_AUX_TX_P0_FLDMASK_LEN               8

#define AUX_TX_FIFO_WRITE_DATA_BYTE15_AUX_TX_P0_FLDMASK                   0xff00
#define AUX_TX_FIFO_WRITE_DATA_BYTE15_AUX_TX_P0_FLDMASK_POS               8
#define AUX_TX_FIFO_WRITE_DATA_BYTE15_AUX_TX_P0_FLDMASK_LEN               8

#define REG_3740_AUX_TX_P0              (0x3740)
#define HPD_OEN_AUX_TX_P0_FLDMASK                                         0x1
#define HPD_OEN_AUX_TX_P0_FLDMASK_POS                                     0
#define HPD_OEN_AUX_TX_P0_FLDMASK_LEN                                     1

#define HPD_I_AUX_TX_P0_FLDMASK                                           0x2
#define HPD_I_AUX_TX_P0_FLDMASK_POS                                       1
#define HPD_I_AUX_TX_P0_FLDMASK_LEN                                       1

#define REG_3744_AUX_TX_P0              (0x3744)
#define TEST_AUXRX_AUX_TX_P0_FLDMASK                                      0xffff
#define TEST_AUXRX_AUX_TX_P0_FLDMASK_POS                                  0
#define TEST_AUXRX_AUX_TX_P0_FLDMASK_LEN                                  16

#define REG_3748_AUX_TX_P0              (0x3748)
#define CK_XTAL_AUX_TX_P0_FLDMASK                                         0x1
#define CK_XTAL_AUX_TX_P0_FLDMASK_POS                                     0
#define CK_XTAL_AUX_TX_P0_FLDMASK_LEN                                     1

#define EN_FT_MUX_AUX_TX_P0_FLDMASK                                       0x2
#define EN_FT_MUX_AUX_TX_P0_FLDMASK_POS                                   1
#define EN_FT_MUX_AUX_TX_P0_FLDMASK_LEN                                   1

#define EN_GPIO_AUX_TX_P0_FLDMASK                                         0x4
#define EN_GPIO_AUX_TX_P0_FLDMASK_POS                                     2
#define EN_GPIO_AUX_TX_P0_FLDMASK_LEN                                     1

#define EN_HBR3_AUX_TX_P0_FLDMASK                                         0x8
#define EN_HBR3_AUX_TX_P0_FLDMASK_POS                                     3
#define EN_HBR3_AUX_TX_P0_FLDMASK_LEN                                     1

#define PD_NGATE_OV_AUX_TX_P0_FLDMASK                                     0x10
#define PD_NGATE_OV_AUX_TX_P0_FLDMASK_POS                                 4
#define PD_NGATE_OV_AUX_TX_P0_FLDMASK_LEN                                 1

#define PD_NGATE_OVEN_AUX_TX_P0_FLDMASK                                   0x20
#define PD_NGATE_OVEN_AUX_TX_P0_FLDMASK_POS                               5
#define PD_NGATE_OVEN_AUX_TX_P0_FLDMASK_LEN                               1

#define PD_VCM_OP_AUX_TX_P0_FLDMASK                                       0x40
#define PD_VCM_OP_AUX_TX_P0_FLDMASK_POS                                   6
#define PD_VCM_OP_AUX_TX_P0_FLDMASK_LEN                                   1

#define CK_XTAL_SW_AUX_TX_P0_FLDMASK                                      0x80
#define CK_XTAL_SW_AUX_TX_P0_FLDMASK_POS                                  7
#define CK_XTAL_SW_AUX_TX_P0_FLDMASK_LEN                                  1

#define SEL_FTMUX_AUX_TX_P0_FLDMASK                                       0x300
#define SEL_FTMUX_AUX_TX_P0_FLDMASK_POS                                   8
#define SEL_FTMUX_AUX_TX_P0_FLDMASK_LEN                                   2

#define GTC_EN_AUX_TX_P0_FLDMASK                                          0x400
#define GTC_EN_AUX_TX_P0_FLDMASK_POS                                      10
#define GTC_EN_AUX_TX_P0_FLDMASK_LEN                                      1

#define GTC_DATA_IN_MODE_AUX_TX_P0_FLDMASK                                0x800
#define GTC_DATA_IN_MODE_AUX_TX_P0_FLDMASK_POS                            11
#define GTC_DATA_IN_MODE_AUX_TX_P0_FLDMASK_LEN                            1

#define REG_374C_AUX_TX_P0              (0x374C)
#define AUX_VALID_DB_TH_AUX_TX_P0_FLDMASK                                 0xf
#define AUX_VALID_DB_TH_AUX_TX_P0_FLDMASK_POS                             0
#define AUX_VALID_DB_TH_AUX_TX_P0_FLDMASK_LEN                             4

#define CLK_AUX_MUX_VALID_EN_AUX_TX_P0_FLDMASK                            0x100
#define CLK_AUX_MUX_VALID_EN_AUX_TX_P0_FLDMASK_POS                        8
#define CLK_AUX_MUX_VALID_EN_AUX_TX_P0_FLDMASK_LEN                        1

#define CLK_AUX_MUX_VALID_INV_AUX_TX_P0_FLDMASK                           0x200
#define CLK_AUX_MUX_VALID_INV_AUX_TX_P0_FLDMASK_POS                       9
#define CLK_AUX_MUX_VALID_INV_AUX_TX_P0_FLDMASK_LEN                       1

#define CLK_AUX_MUX_VALID_SEL_AUX_TX_P0_FLDMASK                           0xc00
#define CLK_AUX_MUX_VALID_SEL_AUX_TX_P0_FLDMASK_POS                       10
#define CLK_AUX_MUX_VALID_SEL_AUX_TX_P0_FLDMASK_LEN                       2

#define CLK_AUX_MUX_DATA_EN_AUX_TX_P0_FLDMASK                             0x1000
#define CLK_AUX_MUX_DATA_EN_AUX_TX_P0_FLDMASK_POS                         12
#define CLK_AUX_MUX_DATA_EN_AUX_TX_P0_FLDMASK_LEN                         1

#define CLK_AUX_MUX_DATA_INV_AUX_TX_P0_FLDMASK                            0x2000
#define CLK_AUX_MUX_DATA_INV_AUX_TX_P0_FLDMASK_POS                        13
#define CLK_AUX_MUX_DATA_INV_AUX_TX_P0_FLDMASK_LEN                        1

#define CLK_AUX_MUX_DATA_SEL_AUX_TX_P0_FLDMASK                            0xc000
#define CLK_AUX_MUX_DATA_SEL_AUX_TX_P0_FLDMASK_POS                        14
#define CLK_AUX_MUX_DATA_SEL_AUX_TX_P0_FLDMASK_LEN                        2

#define REG_3780_AUX_TX_P0              (0x3780)
#define AUX_RX_FIFO_DATA0_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RX_FIFO_DATA0_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_FIFO_DATA0_AUX_TX_P0_FLDMASK_LEN                           8

#define AUX_RX_FIFO_DATA1_AUX_TX_P0_FLDMASK                               0xff00
#define AUX_RX_FIFO_DATA1_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_DATA1_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3784_AUX_TX_P0              (0x3784)
#define AUX_RX_FIFO_DATA2_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RX_FIFO_DATA2_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_FIFO_DATA2_AUX_TX_P0_FLDMASK_LEN                           8

#define AUX_RX_FIFO_DATA3_AUX_TX_P0_FLDMASK                               0xff00
#define AUX_RX_FIFO_DATA3_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_DATA3_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3788_AUX_TX_P0              (0x3788)
#define AUX_RX_FIFO_DATA4_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RX_FIFO_DATA4_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_FIFO_DATA4_AUX_TX_P0_FLDMASK_LEN                           8

#define AUX_RX_FIFO_DATA5_AUX_TX_P0_FLDMASK                               0xff00
#define AUX_RX_FIFO_DATA5_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_DATA5_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_378C_AUX_TX_P0              (0x378C)
#define AUX_RX_FIFO_DATA6_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RX_FIFO_DATA6_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_FIFO_DATA6_AUX_TX_P0_FLDMASK_LEN                           8

#define AUX_RX_FIFO_DATA7_AUX_TX_P0_FLDMASK                               0xff00
#define AUX_RX_FIFO_DATA7_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_DATA7_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3790_AUX_TX_P0              (0x3790)
#define AUX_RX_FIFO_DATA8_AUX_TX_P0_FLDMASK                               0xff
#define AUX_RX_FIFO_DATA8_AUX_TX_P0_FLDMASK_POS                           0
#define AUX_RX_FIFO_DATA8_AUX_TX_P0_FLDMASK_LEN                           8

#define AUX_RX_FIFO_DATA9_AUX_TX_P0_FLDMASK                               0xff00
#define AUX_RX_FIFO_DATA9_AUX_TX_P0_FLDMASK_POS                           8
#define AUX_RX_FIFO_DATA9_AUX_TX_P0_FLDMASK_LEN                           8

#define REG_3794_AUX_TX_P0              (0x3794)
#define AUX_RX_FIFO_DATA10_AUX_TX_P0_FLDMASK                              0xff
#define AUX_RX_FIFO_DATA10_AUX_TX_P0_FLDMASK_POS                          0
#define AUX_RX_FIFO_DATA10_AUX_TX_P0_FLDMASK_LEN                          8

#define AUX_RX_FIFO_DATA11_AUX_TX_P0_FLDMASK                              0xff00
#define AUX_RX_FIFO_DATA11_AUX_TX_P0_FLDMASK_POS                          8
#define AUX_RX_FIFO_DATA11_AUX_TX_P0_FLDMASK_LEN                          8

#define REG_3798_AUX_TX_P0              (0x3798)
#define AUX_RX_FIFO_DATA12_AUX_TX_P0_FLDMASK                              0xff
#define AUX_RX_FIFO_DATA12_AUX_TX_P0_FLDMASK_POS                          0
#define AUX_RX_FIFO_DATA12_AUX_TX_P0_FLDMASK_LEN                          8

#define AUX_RX_FIFO_DATA13_AUX_TX_P0_FLDMASK                              0xff00
#define AUX_RX_FIFO_DATA13_AUX_TX_P0_FLDMASK_POS                          8
#define AUX_RX_FIFO_DATA13_AUX_TX_P0_FLDMASK_LEN                          8

#define REG_379C_AUX_TX_P0              (0x379C)
#define AUX_RX_FIFO_DATA14_AUX_TX_P0_FLDMASK                              0xff
#define AUX_RX_FIFO_DATA14_AUX_TX_P0_FLDMASK_POS                          0
#define AUX_RX_FIFO_DATA14_AUX_TX_P0_FLDMASK_LEN                          8

#define AUX_RX_FIFO_DATA15_AUX_TX_P0_FLDMASK                              0xff00
#define AUX_RX_FIFO_DATA15_AUX_TX_P0_FLDMASK_POS                          8
#define AUX_RX_FIFO_DATA15_AUX_TX_P0_FLDMASK_LEN                          8

#define REG_37C0_AUX_TX_P0              (0x37C0)
#define AUX_DRV_EN_TIME_THRD_AUX_TX_P0_FLDMASK                            0x1f
#define AUX_DRV_EN_TIME_THRD_AUX_TX_P0_FLDMASK_POS                        0
#define AUX_DRV_EN_TIME_THRD_AUX_TX_P0_FLDMASK_LEN                        5

#define AUX_DRV_DIS_TIME_THRD_AUX_TX_P0_FLDMASK                           0x1f00
#define AUX_DRV_DIS_TIME_THRD_AUX_TX_P0_FLDMASK_POS                       8
#define AUX_DRV_DIS_TIME_THRD_AUX_TX_P0_FLDMASK_LEN                       5

#define REG_37C4_AUX_TX_P0              (0x37C4)
#define AUX_WAIT_TRANSFER_TIME_THRD_AUX_TX_P0_FLDMASK                     0xff
#define AUX_WAIT_TRANSFER_TIME_THRD_AUX_TX_P0_FLDMASK_POS                 0
#define AUX_WAIT_TRANSFER_TIME_THRD_AUX_TX_P0_FLDMASK_LEN                 8

#define AUX_WAIT_RECEIVE_TIME_THRD_AUX_TX_P0_FLDMASK                      0xff00
#define AUX_WAIT_RECEIVE_TIME_THRD_AUX_TX_P0_FLDMASK_POS                  8
#define AUX_WAIT_RECEIVE_TIME_THRD_AUX_TX_P0_FLDMASK_LEN                  8

#define REG_37C8_AUX_TX_P0              (0x37C8)
#define MTK_ATOP_EN_AUX_TX_P0_FLDMASK                                     0x1
#define MTK_ATOP_EN_AUX_TX_P0_FLDMASK_POS                                 0
#define MTK_ATOP_EN_AUX_TX_P0_FLDMASK_LEN                                 1

/*-----------------------------------------------------*/
#define DP_TX_TOP_PWR_STATE              (TOP_OFFSET + 0x00)
#define DP_PWR_STATE_FLDMASK                                              0x3
#define DP_PWR_STATE_FLDMASK_POS                                          0
#define DP_PWR_STATE_FLDMASK_LEN                                          2

#define DP_SCRAMB_EN_FLDMASK                                              0x4
#define DP_SCRAMB_EN_FLDMASK_POS                                          2
#define DP_SCRAMB_EN_FLDMASK_LEN                                          1

#define DP_DISP_RST_FLDMASK                                               0x8
#define DP_DISP_RST_FLDMASK_POS                                           3
#define DP_DISP_RST_FLDMASK_LEN                                           1

#define DP_TX_TOP_SWING_EMP              (TOP_OFFSET + 0x04)
#define DP_TX0_VOLT_SWING_FLDMASK                                         0x3
#define DP_TX0_VOLT_SWING_FLDMASK_POS                                     0
#define DP_TX0_VOLT_SWING_FLDMASK_LEN                                     2

#define DP_TX0_PRE_EMPH_FLDMASK                                           0xc
#define DP_TX0_PRE_EMPH_FLDMASK_POS                                       2
#define DP_TX0_PRE_EMPH_FLDMASK_LEN                                       2

#define DP_TX0_DATAK_FLDMASK                                              0xf0
#define DP_TX0_DATAK_FLDMASK_POS                                          4
#define DP_TX0_DATAK_FLDMASK_LEN                                          4

#define DP_TX1_VOLT_SWING_FLDMASK                                         0x300
#define DP_TX1_VOLT_SWING_FLDMASK_POS                                     8
#define DP_TX1_VOLT_SWING_FLDMASK_LEN                                     2

#define DP_TX1_PRE_EMPH_FLDMASK                                           0xc00
#define DP_TX1_PRE_EMPH_FLDMASK_POS                                       10
#define DP_TX1_PRE_EMPH_FLDMASK_LEN                                       2

#define DP_TX1_DATAK_FLDMASK                                              0xf000
#define DP_TX1_DATAK_FLDMASK_POS                                          12
#define DP_TX1_DATAK_FLDMASK_LEN                                          4

#define DP_TX2_VOLT_SWING_FLDMASK                              0x30000
#define DP_TX2_VOLT_SWING_FLDMASK_POS                          16
#define DP_TX2_VOLT_SWING_FLDMASK_LEN                          2

#define DP_TX2_PRE_EMPH_FLDMASK                                0xc0000
#define DP_TX2_PRE_EMPH_FLDMASK_POS                            18
#define DP_TX2_PRE_EMPH_FLDMASK_LEN                            2

#define DP_TX2_DATAK_FLDMASK                                   0xf00000
#define DP_TX2_DATAK_FLDMASK_POS                               20
#define DP_TX2_DATAK_FLDMASK_LEN                               4

#define DP_TX3_VOLT_SWING_FLDMASK                              0x3000000
#define DP_TX3_VOLT_SWING_FLDMASK_POS                          24
#define DP_TX3_VOLT_SWING_FLDMASK_LEN                          2

#define DP_TX3_PRE_EMPH_FLDMASK                                0xc000000
#define DP_TX3_PRE_EMPH_FLDMASK_POS                            26
#define DP_TX3_PRE_EMPH_FLDMASK_LEN                            2

#define DP_TX3_DATAK_FLDMASK                                   0xf0000000L
#define DP_TX3_DATAK_FLDMASK_POS                               28
#define DP_TX3_DATAK_FLDMASK_LEN                               4

#define DP_TX_TOP_APB_WSTRB              (TOP_OFFSET + 0x10)
#define APB_WSTRB_FLDMASK                                      0xf
#define APB_WSTRB_FLDMASK_POS                                  0
#define APB_WSTRB_FLDMASK_LEN                                  4

#define APB_WSTRB_EN_FLDMASK                                   0x10
#define APB_WSTRB_EN_FLDMASK_POS                               4
#define APB_WSTRB_EN_FLDMASK_LEN                               1

#define DP_TX_TOP_RESERVED              (TOP_OFFSET + 0x14)
#define RESERVED_FLDMASK                                       0xffffffffL
#define RESERVED_FLDMASK_POS                                   0
#define RESERVED_FLDMASK_LEN                                   32

#define DP_TX_TOP_RESET_AND_PROBE              (TOP_OFFSET + 0x20)
#define SW_RST_B_FLDMASK                                       0x1f
#define SW_RST_B_FLDMASK_POS                                   0
#define SW_RST_B_FLDMASK_LEN                                   5

#define PROBE_LOW_SEL_FLDMASK                                  0x38000
#define PROBE_LOW_SEL_FLDMASK_POS                              15
#define PROBE_LOW_SEL_FLDMASK_LEN                              3

#define PROBE_HIGH_SEL_FLDMASK                                 0x1c0000
#define PROBE_HIGH_SEL_FLDMASK_POS                             18
#define PROBE_HIGH_SEL_FLDMASK_LEN                             3

#define PROBE_LOW_HIGH_SWAP_FLDMASK                            0x200000
#define PROBE_LOW_HIGH_SWAP_FLDMASK_POS                        21
#define PROBE_LOW_HIGH_SWAP_FLDMASK_LEN                        1

#define DP_TX_TOP_SOFT_PROBE              (TOP_OFFSET + 0x24)
#define SW_PROBE_VALUE_FLDMASK                                 0xffffffffL
#define SW_PROBE_VALUE_FLDMASK_POS                             0
#define SW_PROBE_VALUE_FLDMASK_LEN                             32

#define DP_TX_TOP_IRQ_STATUS              (TOP_OFFSET + 0x28)
#define RGS_IRQ_STATUS_FLDMASK                                            0x7
#define RGS_IRQ_STATUS_FLDMASK_POS                                        0
#define RGS_IRQ_STATUS_FLDMASK_LEN                                        3

#define DP_TX_TOP_IRQ_MASK              (TOP_OFFSET + 0x2C)
#define IRQ_MASK_FLDMASK                                                  0x7
#define IRQ_MASK_FLDMASK_POS                                              0
#define IRQ_MASK_FLDMASK_LEN                                              3

#define IRQ_OUT_HIGH_ACTIVE_FLDMASK                                       0x100
#define IRQ_OUT_HIGH_ACTIVE_FLDMASK_POS                                   8
#define IRQ_OUT_HIGH_ACTIVE_FLDMASK_LEN                                   1

#define DP_TX_TOP_BLACK_SCREEN              (TOP_OFFSET + 0x30)
#define BLACK_SCREEN_ENABLE_FLDMASK                                       0x1
#define BLACK_SCREEN_ENABLE_FLDMASK_POS                                   0
#define BLACK_SCREEN_ENABLE_FLDMASK_LEN                                   1

#define DP_TX_TOP_MEM_PD              (TOP_OFFSET + 0x38)
#define MEM_ISO_EN_FLDMASK                                                0x1
#define MEM_ISO_EN_FLDMASK_POS                                            0
#define MEM_ISO_EN_FLDMASK_LEN                                            1

#define MEM_PD_FLDMASK                                                    0x2
#define MEM_PD_FLDMASK_POS                                                1
#define MEM_PD_FLDMASK_LEN                                                1

#define FUSE_SEL_FLDMASK                                                  0x4
#define FUSE_SEL_FLDMASK_POS                                              2
#define FUSE_SEL_FLDMASK_LEN                                              1

#define LOAD_PREFUSE_FLDMASK                                              0x8
#define LOAD_PREFUSE_FLDMASK_POS                                          3
#define LOAD_PREFUSE_FLDMASK_LEN                                          1

#define DP_TX_TOP_MBIST_PREFUSE              (TOP_OFFSET + 0x3C)
#define RGS_PREFUSE_FLDMASK                                     0xffff
#define RGS_PREFUSE_FLDMASK_POS                                 0
#define RGS_PREFUSE_FLDMASK_LEN                                 16

#define DP_TX_TOP_MEM_DELSEL_0              (TOP_OFFSET + 0x40)
#define DELSEL_0_FLDMASK                                        0xfffff
#define DELSEL_0_FLDMASK_POS                                    0
#define DELSEL_0_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_0_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_0_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_0_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_1              (TOP_OFFSET + 0x44)
#define DELSEL_1_FLDMASK                                        0xfffff
#define DELSEL_1_FLDMASK_POS                                    0
#define DELSEL_1_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_1_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_1_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_1_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_2              (TOP_OFFSET + 0x48)
#define DELSEL_2_FLDMASK                                        0xfffff
#define DELSEL_2_FLDMASK_POS                                    0
#define DELSEL_2_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_2_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_2_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_2_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_3              (TOP_OFFSET + 0x4C)
#define DELSEL_3_FLDMASK                                        0xfffff
#define DELSEL_3_FLDMASK_POS                                    0
#define DELSEL_3_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_3_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_3_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_3_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_4              (TOP_OFFSET + 0x50)
#define DELSEL_4_FLDMASK                                        0xfffff
#define DELSEL_4_FLDMASK_POS                                    0
#define DELSEL_4_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_4_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_4_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_4_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_5              (TOP_OFFSET + 0x54)
#define DELSEL_5_FLDMASK                                        0xfffff
#define DELSEL_5_FLDMASK_POS                                    0
#define DELSEL_5_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_5_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_5_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_5_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_6              (TOP_OFFSET + 0x58)
#define DELSEL_6_FLDMASK                                        0xfffff
#define DELSEL_6_FLDMASK_POS                                    0
#define DELSEL_6_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_6_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_6_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_6_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_7              (TOP_OFFSET + 0x5C)
#define DELSEL_7_FLDMASK                                        0xfffff
#define DELSEL_7_FLDMASK_POS                                    0
#define DELSEL_7_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_7_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_7_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_7_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_8              (TOP_OFFSET + 0x60)
#define DELSEL_8_FLDMASK                                        0xfffff
#define DELSEL_8_FLDMASK_POS                                    0
#define DELSEL_8_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_8_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_8_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_8_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_9              (TOP_OFFSET + 0x64)
#define DELSEL_9_FLDMASK                                        0xfffff
#define DELSEL_9_FLDMASK_POS                                    0
#define DELSEL_9_FLDMASK_LEN                                    20

#define USE_DEFAULT_DELSEL_9_FLDMASK                            0x100000
#define USE_DEFAULT_DELSEL_9_FLDMASK_POS                        20
#define USE_DEFAULT_DELSEL_9_FLDMASK_LEN                        1

#define DP_TX_TOP_MEM_DELSEL_10              (TOP_OFFSET + 0x68)
#define DELSEL_10_FLDMASK                                       0xfffff
#define DELSEL_10_FLDMASK_POS                                   0
#define DELSEL_10_FLDMASK_LEN                                   20

#define USE_DEFAULT_DELSEL_10_FLDMASK                           0x100000
#define USE_DEFAULT_DELSEL_10_FLDMASK_POS                       20
#define USE_DEFAULT_DELSEL_10_FLDMASK_LEN                       1

#define DP_TX_TOP_MEM_DELSEL_11              (TOP_OFFSET + 0x6C)
#define DELSEL_11_FLDMASK                                       0xfffff
#define DELSEL_11_FLDMASK_POS                                   0
#define DELSEL_11_FLDMASK_LEN                                   20

#define USE_DEFAULT_DELSEL_11_FLDMASK                           0x100000
#define USE_DEFAULT_DELSEL_11_FLDMASK_POS                       20
#define USE_DEFAULT_DELSEL_11_FLDMASK_LEN                       1

#define DP_TX_TOP_MEM_DELSEL_12              (TOP_OFFSET + 0x70)
#define DELSEL_12_FLDMASK                                       0xfffff
#define DELSEL_12_FLDMASK_POS                                   0
#define DELSEL_12_FLDMASK_LEN                                   20

#define USE_DEFAULT_DELSEL_12_FLDMASK                           0x100000
#define USE_DEFAULT_DELSEL_12_FLDMASK_POS                       20
#define USE_DEFAULT_DELSEL_12_FLDMASK_LEN                       1

#define DP_TX_TOP_PWR_ACK              (TOP_OFFSET + 0x80)
#define RGS_DP_TX_PWR_ACK_FLDMASK                               0x1
#define RGS_DP_TX_PWR_ACK_FLDMASK_POS                           0
#define RGS_DP_TX_PWR_ACK_FLDMASK_LEN                           1

#define RGS_DP_TX_PWR_ACK_2ND_FLDMASK                           0x2
#define RGS_DP_TX_PWR_ACK_2ND_FLDMASK_POS                       1
#define RGS_DP_TX_PWR_ACK_2ND_FLDMASK_LEN                       1

/*-----------------------------------------------------*/
#define DP_TX_SECURE_REG0              (SEC_OFFSET + 0x00)
#define HDCP22_KS_XOR_LC128_KEY_0_FLDMASK                       0xffffffffL
#define HDCP22_KS_XOR_LC128_KEY_0_FLDMASK_POS                   0
#define HDCP22_KS_XOR_LC128_KEY_0_FLDMASK_LEN                   32

#define DP_TX_SECURE_REG1              (SEC_OFFSET + 0x04)
#define HDCP22_KS_XOR_LC128_KEY_1_FLDMASK                       0xffffffffL
#define HDCP22_KS_XOR_LC128_KEY_1_FLDMASK_POS                   0
#define HDCP22_KS_XOR_LC128_KEY_1_FLDMASK_LEN                   32

#define DP_TX_SECURE_REG2              (SEC_OFFSET + 0x08)
#define HDCP22_KS_XOR_LC128_KEY_2_FLDMASK                       0xffffffffL
#define HDCP22_KS_XOR_LC128_KEY_2_FLDMASK_POS                   0
#define HDCP22_KS_XOR_LC128_KEY_2_FLDMASK_LEN                   32

#define DP_TX_SECURE_REG3              (SEC_OFFSET + 0x0c)
#define HDCP22_KS_XOR_LC128_KEY_3_FLDMASK                       0xffffffffL
#define HDCP22_KS_XOR_LC128_KEY_3_FLDMASK_POS                   0
#define HDCP22_KS_XOR_LC128_KEY_3_FLDMASK_LEN                   32

#define DP_TX_SECURE_REG4              (SEC_OFFSET + 0x10)
#define HDCP22_RIV_0_FLDMASK                                    0xffffffffL
#define HDCP22_RIV_0_FLDMASK_POS                                0
#define HDCP22_RIV_0_FLDMASK_LEN                                32

#define DP_TX_SECURE_REG5              (SEC_OFFSET + 0x14)
#define HDCP22_RIV_1_FLDMASK                                    0xffffffffL
#define HDCP22_RIV_1_FLDMASK_POS                                0
#define HDCP22_RIV_1_FLDMASK_LEN                                32

#define DP_TX_SECURE_REG6              (SEC_OFFSET + 0x18)
#define HDCP13_LN_SEED_FLDMASK                                  0xff
#define HDCP13_LN_SEED_FLDMASK_POS                              0
#define HDCP13_LN_SEED_FLDMASK_LEN                              8

#define DP_TX_SECURE_REG7              (SEC_OFFSET + 0x1C)
#define HDCP13_LN_CODE_0_FLDMASK                                0xffffffffL
#define HDCP13_LN_CODE_0_FLDMASK_POS                            0
#define HDCP13_LN_CODE_0_FLDMASK_LEN                            32

#define DP_TX_SECURE_REG8              (SEC_OFFSET + 0x20)
#define HDCP13_LN_CODE_1_FLDMASK                                0xffffff
#define HDCP13_LN_CODE_1_FLDMASK_POS                            0
#define HDCP13_LN_CODE_1_FLDMASK_LEN                            24

#define DP_TX_SECURE_REG9              (SEC_OFFSET + 0x24)
#define HDCP13_AN_CODE_0_FLDMASK                                0xffffffffL
#define HDCP13_AN_CODE_0_FLDMASK_POS                            0
#define HDCP13_AN_CODE_0_FLDMASK_LEN                            32

#define DP_TX_SECURE_REG10              (SEC_OFFSET + 0x28)
#define HDCP13_AN_CODE_1_FLDMASK                                0xffffffffL
#define HDCP13_AN_CODE_1_FLDMASK_POS                            0
#define HDCP13_AN_CODE_1_FLDMASK_LEN                            32

#define DP_TX_SECURE_REG11              (SEC_OFFSET + 0x2C)
#define DP_TX_TRANSMITTER_4P_RESET_SW_SECURE_FLDMASK            0x1
#define DP_TX_TRANSMITTER_4P_RESET_SW_SECURE_FLDMASK_POS        0
#define DP_TX_TRANSMITTER_4P_RESET_SW_SECURE_FLDMASK_LEN        1

#define HDCP22_RST_SW_SECURE_FLDMASK                            0x2
#define HDCP22_RST_SW_SECURE_FLDMASK_POS                        1
#define HDCP22_RST_SW_SECURE_FLDMASK_LEN                        1

#define HDCP13_RST_SW_SECURE_FLDMASK                            0x4
#define HDCP13_RST_SW_SECURE_FLDMASK_POS                        2
#define HDCP13_RST_SW_SECURE_FLDMASK_LEN                        1

#define VIDEO_MUTE_SW_SECURE_FLDMASK                            0x8
#define VIDEO_MUTE_SW_SECURE_FLDMASK_POS                        3
#define VIDEO_MUTE_SW_SECURE_FLDMASK_LEN                        1

#define VIDEO_MUTE_SEL_SECURE_FLDMASK                           0x10
#define VIDEO_MUTE_SEL_SECURE_FLDMASK_POS                       4
#define VIDEO_MUTE_SEL_SECURE_FLDMASK_LEN                       1

#define HDCP_FRAME_EN_SECURE_FLDMASK                            0x20
#define HDCP_FRAME_EN_SECURE_FLDMASK_POS                        5
#define HDCP_FRAME_EN_SECURE_FLDMASK_LEN                        1

#define HDCP_FRAME_EN_SEL_SECURE_FLDMASK                        0x40
#define HDCP_FRAME_EN_SEL_SECURE_FLDMASK_POS                    6
#define HDCP_FRAME_EN_SEL_SECURE_FLDMASK_LEN                    1

#define VSC_SEL_SECURE_FLDMASK                                  0x80
#define VSC_SEL_SECURE_FLDMASK_POS                              7
#define VSC_SEL_SECURE_FLDMASK_LEN                              1

#define VSC_DATA_TOGGLE_VESA_SECURE_FLDMASK                     0x100
#define VSC_DATA_TOGGLE_VESA_SECURE_FLDMASK_POS                 8
#define VSC_DATA_TOGGLE_VESA_SECURE_FLDMASK_LEN                 1

#define VSC_DATA_RDY_VESA_SECURE_FLDMASK                        0x200
#define VSC_DATA_RDY_VESA_SECURE_FLDMASK_POS                    9
#define VSC_DATA_RDY_VESA_SECURE_FLDMASK_LEN                    1

#define VSC_DATA_TOGGLE_CEA_SECURE_FLDMASK                      0x400
#define VSC_DATA_TOGGLE_CEA_SECURE_FLDMASK_POS                  10
#define VSC_DATA_TOGGLE_CEA_SECURE_FLDMASK_LEN                  1

#define VSC_DATA_RDY_CEA_SECURE_FLDMASK                         0x800
#define VSC_DATA_RDY_CEA_SECURE_FLDMASK_POS                     11
#define VSC_DATA_RDY_CEA_SECURE_FLDMASK_LEN                     1

#define DP_TX_SECURE_REG12              (SEC_OFFSET + 0x30)
#define VSC_DATA_BYTE7_CEA_SECURE_FLDMASK                       0xff000000L
#define VSC_DATA_BYTE7_CEA_SECURE_FLDMASK_POS                   24
#define VSC_DATA_BYTE7_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE6_CEA_SECURE_FLDMASK                       0xff0000
#define VSC_DATA_BYTE6_CEA_SECURE_FLDMASK_POS                   16
#define VSC_DATA_BYTE6_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE5_CEA_SECURE_FLDMASK                       0xff00
#define VSC_DATA_BYTE5_CEA_SECURE_FLDMASK_POS                   8
#define VSC_DATA_BYTE5_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE4_CEA_SECURE_FLDMASK                       0xff
#define VSC_DATA_BYTE4_CEA_SECURE_FLDMASK_POS                   0
#define VSC_DATA_BYTE4_CEA_SECURE_FLDMASK_LEN                   8

#define DP_TX_SECURE_REG13              (SEC_OFFSET + 0x34)
#define VSC_DATA_BYTE3_CEA_SECURE_FLDMASK                       0xff000000L
#define VSC_DATA_BYTE3_CEA_SECURE_FLDMASK_POS                   24
#define VSC_DATA_BYTE3_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE2_CEA_SECURE_FLDMASK                       0xff0000
#define VSC_DATA_BYTE2_CEA_SECURE_FLDMASK_POS                   16
#define VSC_DATA_BYTE2_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE1_CEA_SECURE_FLDMASK                       0xff00
#define VSC_DATA_BYTE1_CEA_SECURE_FLDMASK_POS                   8
#define VSC_DATA_BYTE1_CEA_SECURE_FLDMASK_LEN                   8

#define VSC_DATA_BYTE0_CEA_SECURE_FLDMASK                       0xff
#define VSC_DATA_BYTE0_CEA_SECURE_FLDMASK_POS                   0
#define VSC_DATA_BYTE0_CEA_SECURE_FLDMASK_LEN                   8

#define DP_TX_SECURE_REG14              (SEC_OFFSET + 0x38)
#define VSC_DATA_BYTE7_VESA_SECURE_FLDMASK                      0xff000000L
#define VSC_DATA_BYTE7_VESA_SECURE_FLDMASK_POS                  24
#define VSC_DATA_BYTE7_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE6_VESA_SECURE_FLDMASK                      0xff0000
#define VSC_DATA_BYTE6_VESA_SECURE_FLDMASK_POS                  16
#define VSC_DATA_BYTE6_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE5_VESA_SECURE_FLDMASK                      0xff00
#define VSC_DATA_BYTE5_VESA_SECURE_FLDMASK_POS                  8
#define VSC_DATA_BYTE5_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE4_VESA_SECURE_FLDMASK                      0xff
#define VSC_DATA_BYTE4_VESA_SECURE_FLDMASK_POS                  0
#define VSC_DATA_BYTE4_VESA_SECURE_FLDMASK_LEN                  8

#define DP_TX_SECURE_REG15              (SEC_OFFSET + 0x3C)
#define VSC_DATA_BYTE3_VESA_SECURE_FLDMASK                      0xff000000L
#define VSC_DATA_BYTE3_VESA_SECURE_FLDMASK_POS                  24
#define VSC_DATA_BYTE3_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE2_VESA_SECURE_FLDMASK                      0xff0000
#define VSC_DATA_BYTE2_VESA_SECURE_FLDMASK_POS                  16
#define VSC_DATA_BYTE2_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE1_VESA_SECURE_FLDMASK                      0xff00
#define VSC_DATA_BYTE1_VESA_SECURE_FLDMASK_POS                  8
#define VSC_DATA_BYTE1_VESA_SECURE_FLDMASK_LEN                  8

#define VSC_DATA_BYTE0_VESA_SECURE_FLDMASK                      0xff
#define VSC_DATA_BYTE0_VESA_SECURE_FLDMASK_POS                  0
#define VSC_DATA_BYTE0_VESA_SECURE_FLDMASK_LEN                  8

#define DP_TX_SECURE_STATUS_0              (SEC_OFFSET + 0x80)
#define RGS_DP_TX_HDCP13_HDCP_AN_0_FLDMASK                      0xffffffffL
#define RGS_DP_TX_HDCP13_HDCP_AN_0_FLDMASK_POS                  0
#define RGS_DP_TX_HDCP13_HDCP_AN_0_FLDMASK_LEN                  32

#define DP_TX_SECURE_STATUS_1              (SEC_OFFSET + 0x84)
#define RGS_DP_TX_HDCP13_HDCP_AN_1_FLDMASK                      0xffffffffL
#define RGS_DP_TX_HDCP13_HDCP_AN_1_FLDMASK_POS                  0
#define RGS_DP_TX_HDCP13_HDCP_AN_1_FLDMASK_LEN                  32

#define DP_TX_SECURE_STATUS_2              (SEC_OFFSET + 0x88)
#define RGS_DP_TX_HDCP13_HDCP_R0_FLDMASK                        0xffff
#define RGS_DP_TX_HDCP13_HDCP_R0_FLDMASK_POS                    0
#define RGS_DP_TX_HDCP13_HDCP_R0_FLDMASK_LEN                    16

#define DP_TX_SECURE_STATUS_3              (SEC_OFFSET + 0x8C)
#define RGS_DP_TX_HDCP13_HDCP_M0_0_FLDMASK                      0xffffffffL
#define RGS_DP_TX_HDCP13_HDCP_M0_0_FLDMASK_POS                  0
#define RGS_DP_TX_HDCP13_HDCP_M0_0_FLDMASK_LEN                  32

#define DP_TX_SECURE_STATUS_4              (SEC_OFFSET + 0x90)
#define RGS_DP_TX_HDCP13_HDCP_M0_1_FLDMASK                      0xffffffffL
#define RGS_DP_TX_HDCP13_HDCP_M0_1_FLDMASK_POS                  0
#define RGS_DP_TX_HDCP13_HDCP_M0_1_FLDMASK_LEN                  32

#define DP_TX_SECURE_ACC_FAIL              (SEC_OFFSET + 0xf0)
#define NO_AUTH_READ_VALUE_FLDMASK                              0xffffffffL
#define NO_AUTH_READ_VALUE_FLDMASK_POS                          0
#define NO_AUTH_READ_VALUE_FLDMASK_LEN                          32


//DPCD address
#define DPCD_00000		0x00000
#define DPCD_00001		0x00001
#define DPCD_00002		0x00002
#define DPCD_00003		0x00003
#define DPCD_00004		0x00004
#define DPCD_00005		0x00005
#define DPCD_0000A		0x0000A
#define DPCD_0000E		0x0000E
#define DPCD_00021		0x00021
#define DPCD_00030		0x00030
#define DPCD_00060		0x00060
#define DPCD_00080		0x00080
#define DPCD_00090		0x00090
#define DPCD_00100		0x00100
#define DPCD_00101		0x00101
#define DPCD_00102		0x00102
#define DPCD_00103		0x00103
#define DPCD_00104		0x00104
#define DPCD_00105		0x00105
#define DPCD_00106		0x00106
#define DPCD_00107		0x00107
#define DPCD_00111		0x00111
#define DPCD_00120		0x00120
#define DPCD_00160		0x00160
#define DPCD_001A1		0x001A1
#define DPCD_001C0		0x001C0
#define DPCD_00200		0x00200
#define DPCD_00201		0x00201
#define DPCD_00202		0x00202
#define DPCD_00203		0x00203
#define DPCD_00204		0x00204
#define DPCD_00205		0x00205
#define DPCD_00206		0x00206
#define DPCD_00210		0x00210
#define DPCD_00218		0x00218
#define DPCD_00219		0x00219
#define DPCD_00220		0x00220
#define DPCD_00230		0x00230
#define DPCD_00250		0x00250
#define DPCD_00260		0x00260
#define DPCD_00261		0x00261
#define DPCD_00271		0x00271
#define DPCD_00280		0x00280
#define DPCD_00281		0x00281
#define DPCD_00282		0x00282
#define DPCD_002C0		0x002C0
#define DPCD_00600		0x00600
#define DPCD_01000		0x01000
#define DPCD_01200		0x01200
#define DPCD_01400		0x01400
#define DPCD_01600		0x01600
#define DPCD_02002		0x02002
#define DPCD_02003		0x02003
#define DPCD_0200C		0x0200C
#define DPCD_0200D		0x0200D
#define DPCD_0200E		0x0200E
#define DPCD_0200F		0x0200F
#define DPCD_02200		0x02200
#define DPCD_02201		0x02201
#define DPCD_02202		0x02202
#define DPCD_02203		0x02203
#define DPCD_02204		0x02204
#define DPCD_02205		0x02205
#define DPCD_02206		0x02206
#define DPCD_02207		0x02207
#define DPCD_02208		0x02208
#define DPCD_02209		0x02209
#define DPCD_0220A		0x0220A
#define DPCD_0220B		0x0220B
#define DPCD_0220C		0x0220C
#define DPCD_0220D		0x0220D
#define DPCD_0220E		0x0220E
#define DPCD_0220F		0x0220F
#define DPCD_02210		0x02210
#define DPCD_02211		0x02211
#define DPCD_68000		0x68000
#define DPCD_68005		0x68005
#define DPCD_68007		0x68007
#define DPCD_6800C		0x6800C
#define DPCD_68014		0x68014
#define DPCD_68018		0x68018
#define DPCD_6801C		0x6801C
#define DPCD_68020		0x68020
#define DPCD_68024		0x68024
#define DPCD_68028		0x68028
#define DPCD_68029		0x68029
#define DPCD_6802A		0x6802A
#define DPCD_6802C		0x6802C
#define DPCD_6803B		0x6803B
#define DPCD_6921D		0x6921D
#define DPCD_69000		0x69000
#define DPCD_69008		0x69008
#define DPCD_6900B		0x6900B
#define DPCD_69215		0x69215
#define DPCD_6921D		0x6921D
#define DPCD_69220		0x69220
#define DPCD_692A0		0x692A0
#define DPCD_692B0		0x692B0
#define DPCD_692C0		0x692C0
#define DPCD_692E0		0x692E0
#define DPCD_692F0		0x692F0
#define DPCD_692F8		0x692F8
#define DPCD_69318		0x69318
#define DPCD_69328		0x69328
#define DPCD_69330		0x69330
#define DPCD_69332		0x69332
#define DPCD_69335		0x69335
#define DPCD_69345		0x69345
#define DPCD_693E0		0x693E0
#define DPCD_693F0		0x693F0
#define DPCD_693F3		0x693F3
#define DPCD_693F5		0x693F5
#define DPCD_69473		0x69473
#define DPCD_69493		0x69493
#define DPCD_69494		0x69494
#define DPCD_69518		0x69518

#define BIT0  0x00000001
#define BIT1  0x00000002
#define BIT2  0x00000004
#define BIT3  0x00000008
#define BIT4  0x00000010
#define BIT5  0x00000020
#define BIT6  0x00000040
#define BIT7  0x00000080
#define BIT8  0x00000100
#define BIT9  0x00000200
#define BIT10 0x00000400
#define BIT11 0x00000800
#define BIT12 0x00001000
#define BIT13 0x00002000
#define BIT14 0x00004000
#define BIT15 0x00008000
#define BIT16 0x00010000
#define BIT17 0x00020000
#define BIT18 0x00040000
#define BIT19 0x00080000
#define BIT20 0x00100000
#define BIT21 0x00200000
#define BIT22 0x00400000
#define BIT23 0x00800000
#define BIT24 0x01000000
#define BIT25 0x02000000
#define BIT26 0x04000000
#define BIT27 0x08000000
#define BIT28 0x10000000
#define BIT29 0x20000000
#define BIT30 0x40000000
#define BIT31 0x80000000

#endif /*__MTK_DRRX_REG_H__*/

