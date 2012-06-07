/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#define OMAP_ABE_INIT_SM_ADDR                              0x0
#define OMAP_ABE_INIT_SM_SIZE                              0xC80
#define OMAP_ABE_S_DATA0_ADDR                              0xC80
#define OMAP_ABE_S_DATA0_SIZE                              0x8
#define OMAP_ABE_S_TEMP_ADDR                               0xC88
#define OMAP_ABE_S_TEMP_SIZE                               0x8
#define OMAP_ABE_S_PHOENIXOFFSET_ADDR                      0xC90
#define OMAP_ABE_S_PHOENIXOFFSET_SIZE                      0x8
#define OMAP_ABE_S_GTARGET1_ADDR                           0xC98
#define OMAP_ABE_S_GTARGET1_SIZE                           0x38
#define OMAP_ABE_S_GTARGET_DL1_ADDR                        0xCD0
#define OMAP_ABE_S_GTARGET_DL1_SIZE                        0x10
#define OMAP_ABE_S_GTARGET_DL2_ADDR                        0xCE0
#define OMAP_ABE_S_GTARGET_DL2_SIZE                        0x10
#define OMAP_ABE_S_GTARGET_ECHO_ADDR                       0xCF0
#define OMAP_ABE_S_GTARGET_ECHO_SIZE                       0x8
#define OMAP_ABE_S_GTARGET_SDT_ADDR                        0xCF8
#define OMAP_ABE_S_GTARGET_SDT_SIZE                        0x8
#define OMAP_ABE_S_GTARGET_VXREC_ADDR                      0xD00
#define OMAP_ABE_S_GTARGET_VXREC_SIZE                      0x10
#define OMAP_ABE_S_GTARGET_UL_ADDR                         0xD10
#define OMAP_ABE_S_GTARGET_UL_SIZE                         0x10
#define OMAP_ABE_S_GTARGET_BTUL_ADDR                       0xD20
#define OMAP_ABE_S_GTARGET_BTUL_SIZE                       0x8
#define OMAP_ABE_S_GCURRENT_ADDR                           0xD28
#define OMAP_ABE_S_GCURRENT_SIZE                           0x90
#define OMAP_ABE_S_GAIN_ONE_ADDR                           0xDB8
#define OMAP_ABE_S_GAIN_ONE_SIZE                           0x8
#define OMAP_ABE_S_TONES_ADDR                              0xDC0
#define OMAP_ABE_S_TONES_SIZE                              0x60
#define OMAP_ABE_S_VX_DL_ADDR                              0xE20
#define OMAP_ABE_S_VX_DL_SIZE                              0x60
#define OMAP_ABE_S_MM_UL2_ADDR                             0xE80
#define OMAP_ABE_S_MM_UL2_SIZE                             0x60
#define OMAP_ABE_S_MM_DL_ADDR                              0xEE0
#define OMAP_ABE_S_MM_DL_SIZE                              0x60
#define OMAP_ABE_S_DL1_M_OUT_ADDR                          0xF40
#define OMAP_ABE_S_DL1_M_OUT_SIZE                          0x60
#define OMAP_ABE_S_DL2_M_OUT_ADDR                          0xFA0
#define OMAP_ABE_S_DL2_M_OUT_SIZE                          0x60
#define OMAP_ABE_S_ECHO_M_OUT_ADDR                         0x1000
#define OMAP_ABE_S_ECHO_M_OUT_SIZE                         0x60
#define OMAP_ABE_S_SDT_M_OUT_ADDR                          0x1060
#define OMAP_ABE_S_SDT_M_OUT_SIZE                          0x60
#define OMAP_ABE_S_VX_UL_ADDR                              0x10C0
#define OMAP_ABE_S_VX_UL_SIZE                              0x60
#define OMAP_ABE_S_VX_UL_M_ADDR                            0x1120
#define OMAP_ABE_S_VX_UL_M_SIZE                            0x60
#define OMAP_ABE_S_BT_DL_ADDR                              0x1180
#define OMAP_ABE_S_BT_DL_SIZE                              0x60
#define OMAP_ABE_S_BT_UL_ADDR                              0x11E0
#define OMAP_ABE_S_BT_UL_SIZE                              0x60
#define OMAP_ABE_S_BT_DL_8K_ADDR                           0x1240
#define OMAP_ABE_S_BT_DL_8K_SIZE                           0x18
#define OMAP_ABE_S_BT_DL_16K_ADDR                          0x1258
#define OMAP_ABE_S_BT_DL_16K_SIZE                          0x28
#define OMAP_ABE_S_BT_UL_8K_ADDR                           0x1280
#define OMAP_ABE_S_BT_UL_8K_SIZE                           0x10
#define OMAP_ABE_S_BT_UL_16K_ADDR                          0x1290
#define OMAP_ABE_S_BT_UL_16K_SIZE                          0x20
#define OMAP_ABE_S_SDT_F_ADDR                              0x12B0
#define OMAP_ABE_S_SDT_F_SIZE                              0x60
#define OMAP_ABE_S_SDT_F_DATA_ADDR                         0x1310
#define OMAP_ABE_S_SDT_F_DATA_SIZE                         0x48
#define OMAP_ABE_S_MM_DL_OSR_ADDR                          0x1358
#define OMAP_ABE_S_MM_DL_OSR_SIZE                          0xC0
#define OMAP_ABE_S_24_ZEROS_ADDR                           0x1418
#define OMAP_ABE_S_24_ZEROS_SIZE                           0xC0
#define OMAP_ABE_S_DMIC1_ADDR                              0x14D8
#define OMAP_ABE_S_DMIC1_SIZE                              0x60
#define OMAP_ABE_S_DMIC2_ADDR                              0x1538
#define OMAP_ABE_S_DMIC2_SIZE                              0x60
#define OMAP_ABE_S_DMIC3_ADDR                              0x1598
#define OMAP_ABE_S_DMIC3_SIZE                              0x60
#define OMAP_ABE_S_AMIC_ADDR                               0x15F8
#define OMAP_ABE_S_AMIC_SIZE                               0x60
#define OMAP_ABE_S_DMIC1_L_ADDR                            0x1658
#define OMAP_ABE_S_DMIC1_L_SIZE                            0x60
#define OMAP_ABE_S_DMIC1_R_ADDR                            0x16B8
#define OMAP_ABE_S_DMIC1_R_SIZE                            0x60
#define OMAP_ABE_S_DMIC2_L_ADDR                            0x1718
#define OMAP_ABE_S_DMIC2_L_SIZE                            0x60
#define OMAP_ABE_S_DMIC2_R_ADDR                            0x1778
#define OMAP_ABE_S_DMIC2_R_SIZE                            0x60
#define OMAP_ABE_S_DMIC3_L_ADDR                            0x17D8
#define OMAP_ABE_S_DMIC3_L_SIZE                            0x60
#define OMAP_ABE_S_DMIC3_R_ADDR                            0x1838
#define OMAP_ABE_S_DMIC3_R_SIZE                            0x60
#define OMAP_ABE_S_BT_UL_L_ADDR                            0x1898
#define OMAP_ABE_S_BT_UL_L_SIZE                            0x60
#define OMAP_ABE_S_BT_UL_R_ADDR                            0x18F8
#define OMAP_ABE_S_BT_UL_R_SIZE                            0x60
#define OMAP_ABE_S_AMIC_L_ADDR                             0x1958
#define OMAP_ABE_S_AMIC_L_SIZE                             0x60
#define OMAP_ABE_S_AMIC_R_ADDR                             0x19B8
#define OMAP_ABE_S_AMIC_R_SIZE                             0x60
#define OMAP_ABE_S_ECHOREF_L_ADDR                          0x1A18
#define OMAP_ABE_S_ECHOREF_L_SIZE                          0x60
#define OMAP_ABE_S_ECHOREF_R_ADDR                          0x1A78
#define OMAP_ABE_S_ECHOREF_R_SIZE                          0x60
#define OMAP_ABE_S_MM_DL_L_ADDR                            0x1AD8
#define OMAP_ABE_S_MM_DL_L_SIZE                            0x60
#define OMAP_ABE_S_MM_DL_R_ADDR                            0x1B38
#define OMAP_ABE_S_MM_DL_R_SIZE                            0x60
#define OMAP_ABE_S_MM_UL_ADDR                              0x1B98
#define OMAP_ABE_S_MM_UL_SIZE                              0x3C0
#define OMAP_ABE_S_AMIC_96K_ADDR                           0x1F58
#define OMAP_ABE_S_AMIC_96K_SIZE                           0xC0
#define OMAP_ABE_S_DMIC0_96K_ADDR                          0x2018
#define OMAP_ABE_S_DMIC0_96K_SIZE                          0xC0
#define OMAP_ABE_S_DMIC1_96K_ADDR                          0x20D8
#define OMAP_ABE_S_DMIC1_96K_SIZE                          0xC0
#define OMAP_ABE_S_DMIC2_96K_ADDR                          0x2198
#define OMAP_ABE_S_DMIC2_96K_SIZE                          0xC0
#define OMAP_ABE_S_UL_VX_UL_48_8K_ADDR                     0x2258
#define OMAP_ABE_S_UL_VX_UL_48_8K_SIZE                     0x60
#define OMAP_ABE_S_UL_VX_UL_48_16K_ADDR                    0x22B8
#define OMAP_ABE_S_UL_VX_UL_48_16K_SIZE                    0x60
#define OMAP_ABE_S_UL_MIC_48K_ADDR                         0x2318
#define OMAP_ABE_S_UL_MIC_48K_SIZE                         0x60
#define OMAP_ABE_S_VOICE_8K_UL_ADDR                        0x2378
#define OMAP_ABE_S_VOICE_8K_UL_SIZE                        0x18
#define OMAP_ABE_S_VOICE_8K_DL_ADDR                        0x2390
#define OMAP_ABE_S_VOICE_8K_DL_SIZE                        0x10
#define OMAP_ABE_S_MCPDM_OUT1_ADDR                         0x23A0
#define OMAP_ABE_S_MCPDM_OUT1_SIZE                         0xC0
#define OMAP_ABE_S_MCPDM_OUT2_ADDR                         0x2460
#define OMAP_ABE_S_MCPDM_OUT2_SIZE                         0xC0
#define OMAP_ABE_S_MCPDM_OUT3_ADDR                         0x2520
#define OMAP_ABE_S_MCPDM_OUT3_SIZE                         0xC0
#define OMAP_ABE_S_VOICE_16K_UL_ADDR                       0x25E0
#define OMAP_ABE_S_VOICE_16K_UL_SIZE                       0x28
#define OMAP_ABE_S_VOICE_16K_DL_ADDR                       0x2608
#define OMAP_ABE_S_VOICE_16K_DL_SIZE                       0x20
#define OMAP_ABE_S_XINASRC_DL_VX_ADDR                      0x2628
#define OMAP_ABE_S_XINASRC_DL_VX_SIZE                      0x140
#define OMAP_ABE_S_XINASRC_UL_VX_ADDR                      0x2768
#define OMAP_ABE_S_XINASRC_UL_VX_SIZE                      0x140
#define OMAP_ABE_S_XINASRC_MM_EXT_IN_ADDR                  0x28A8
#define OMAP_ABE_S_XINASRC_MM_EXT_IN_SIZE                  0x140
#define OMAP_ABE_S_VX_REC_ADDR                             0x29E8
#define OMAP_ABE_S_VX_REC_SIZE                             0x60
#define OMAP_ABE_S_VX_REC_L_ADDR                           0x2A48
#define OMAP_ABE_S_VX_REC_L_SIZE                           0x60
#define OMAP_ABE_S_VX_REC_R_ADDR                           0x2AA8
#define OMAP_ABE_S_VX_REC_R_SIZE                           0x60
#define OMAP_ABE_S_DL2_M_L_ADDR                            0x2B08
#define OMAP_ABE_S_DL2_M_L_SIZE                            0x60
#define OMAP_ABE_S_DL2_M_R_ADDR                            0x2B68
#define OMAP_ABE_S_DL2_M_R_SIZE                            0x60
#define OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR                   0x2BC8
#define OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE                   0xC8
#define OMAP_ABE_S_DL1_M_EQ_DATA_ADDR                      0x2C90
#define OMAP_ABE_S_DL1_M_EQ_DATA_SIZE                      0xC8
#define OMAP_ABE_S_EARP_48_96_LP_DATA_ADDR                 0x2D58
#define OMAP_ABE_S_EARP_48_96_LP_DATA_SIZE                 0x78
#define OMAP_ABE_S_IHF_48_96_LP_DATA_ADDR                  0x2DD0
#define OMAP_ABE_S_IHF_48_96_LP_DATA_SIZE                  0x78
#define OMAP_ABE_S_VX_UL_8_TEMP_ADDR                       0x2E48
#define OMAP_ABE_S_VX_UL_8_TEMP_SIZE                       0x10
#define OMAP_ABE_S_VX_UL_16_TEMP_ADDR                      0x2E58
#define OMAP_ABE_S_VX_UL_16_TEMP_SIZE                      0x20
#define OMAP_ABE_S_VX_DL_8_48_LP_DATA_ADDR                 0x2E78
#define OMAP_ABE_S_VX_DL_8_48_LP_DATA_SIZE                 0x68
#define OMAP_ABE_S_VX_DL_8_48_HP_DATA_ADDR                 0x2EE0
#define OMAP_ABE_S_VX_DL_8_48_HP_DATA_SIZE                 0x38
#define OMAP_ABE_S_VX_DL_16_48_LP_DATA_ADDR                0x2F18
#define OMAP_ABE_S_VX_DL_16_48_LP_DATA_SIZE                0x68
#define OMAP_ABE_S_VX_DL_16_48_HP_DATA_ADDR                0x2F80
#define OMAP_ABE_S_VX_DL_16_48_HP_DATA_SIZE                0x28
#define OMAP_ABE_S_VX_UL_48_8_LP_DATA_ADDR                 0x2FA8
#define OMAP_ABE_S_VX_UL_48_8_LP_DATA_SIZE                 0x68
#define OMAP_ABE_S_VX_UL_48_8_HP_DATA_ADDR                 0x3010
#define OMAP_ABE_S_VX_UL_48_8_HP_DATA_SIZE                 0x38
#define OMAP_ABE_S_VX_UL_48_16_LP_DATA_ADDR                0x3048
#define OMAP_ABE_S_VX_UL_48_16_LP_DATA_SIZE                0x68
#define OMAP_ABE_S_VX_UL_48_16_HP_DATA_ADDR                0x30B0
#define OMAP_ABE_S_VX_UL_48_16_HP_DATA_SIZE                0x28
#define OMAP_ABE_S_BT_UL_8_48_LP_DATA_ADDR                 0x30D8
#define OMAP_ABE_S_BT_UL_8_48_LP_DATA_SIZE                 0x68
#define OMAP_ABE_S_BT_UL_8_48_HP_DATA_ADDR                 0x3140
#define OMAP_ABE_S_BT_UL_8_48_HP_DATA_SIZE                 0x38
#define OMAP_ABE_S_BT_UL_16_48_LP_DATA_ADDR                0x3178
#define OMAP_ABE_S_BT_UL_16_48_LP_DATA_SIZE                0x68
#define OMAP_ABE_S_BT_UL_16_48_HP_DATA_ADDR                0x31E0
#define OMAP_ABE_S_BT_UL_16_48_HP_DATA_SIZE                0x28
#define OMAP_ABE_S_BT_DL_48_8_LP_DATA_ADDR                 0x3208
#define OMAP_ABE_S_BT_DL_48_8_LP_DATA_SIZE                 0x68
#define OMAP_ABE_S_BT_DL_48_8_HP_DATA_ADDR                 0x3270
#define OMAP_ABE_S_BT_DL_48_8_HP_DATA_SIZE                 0x38
#define OMAP_ABE_S_BT_DL_48_16_LP_DATA_ADDR                0x32A8
#define OMAP_ABE_S_BT_DL_48_16_LP_DATA_SIZE                0x68
#define OMAP_ABE_S_BT_DL_48_16_HP_DATA_ADDR                0x3310
#define OMAP_ABE_S_BT_DL_48_16_HP_DATA_SIZE                0x28
#define OMAP_ABE_S_ECHO_REF_48_8_LP_DATA_ADDR              0x3338
#define OMAP_ABE_S_ECHO_REF_48_8_LP_DATA_SIZE              0x68
#define OMAP_ABE_S_ECHO_REF_48_8_HP_DATA_ADDR              0x33A0
#define OMAP_ABE_S_ECHO_REF_48_8_HP_DATA_SIZE              0x38
#define OMAP_ABE_S_ECHO_REF_48_16_LP_DATA_ADDR             0x33D8
#define OMAP_ABE_S_ECHO_REF_48_16_LP_DATA_SIZE             0x68
#define OMAP_ABE_S_ECHO_REF_48_16_HP_DATA_ADDR             0x3440
#define OMAP_ABE_S_ECHO_REF_48_16_HP_DATA_SIZE             0x28
#define OMAP_ABE_S_XINASRC_ECHO_REF_ADDR                   0x3468
#define OMAP_ABE_S_XINASRC_ECHO_REF_SIZE                   0x140
#define OMAP_ABE_S_ECHO_REF_16K_ADDR                       0x35A8
#define OMAP_ABE_S_ECHO_REF_16K_SIZE                       0x28
#define OMAP_ABE_S_ECHO_REF_8K_ADDR                        0x35D0
#define OMAP_ABE_S_ECHO_REF_8K_SIZE                        0x18
#define OMAP_ABE_S_DL1_EQ_ADDR                             0x35E8
#define OMAP_ABE_S_DL1_EQ_SIZE                             0x60
#define OMAP_ABE_S_DL2_EQ_ADDR                             0x3648
#define OMAP_ABE_S_DL2_EQ_SIZE                             0x60
#define OMAP_ABE_S_DL1_GAIN_OUT_ADDR                       0x36A8
#define OMAP_ABE_S_DL1_GAIN_OUT_SIZE                       0x60
#define OMAP_ABE_S_DL2_GAIN_OUT_ADDR                       0x3708
#define OMAP_ABE_S_DL2_GAIN_OUT_SIZE                       0x60
#define OMAP_ABE_S_DC_HS_ADDR                              0x3768
#define OMAP_ABE_S_DC_HS_SIZE                              0x8
#define OMAP_ABE_S_DC_HF_ADDR                              0x3770
#define OMAP_ABE_S_DC_HF_SIZE                              0x8
#define OMAP_ABE_S_VIBRA_ADDR                              0x3778
#define OMAP_ABE_S_VIBRA_SIZE                              0x30
#define OMAP_ABE_S_VIBRA2_IN_ADDR                          0x37A8
#define OMAP_ABE_S_VIBRA2_IN_SIZE                          0x30
#define OMAP_ABE_S_VIBRA2_ADDR_ADDR                        0x37D8
#define OMAP_ABE_S_VIBRA2_ADDR_SIZE                        0x8
#define OMAP_ABE_S_VIBRACTRL_FORRIGHTSM_ADDR               0x37E0
#define OMAP_ABE_S_VIBRACTRL_FORRIGHTSM_SIZE               0xC0
#define OMAP_ABE_S_RNOISE_MEM_ADDR                         0x38A0
#define OMAP_ABE_S_RNOISE_MEM_SIZE                         0x8
#define OMAP_ABE_S_CTRL_ADDR                               0x38A8
#define OMAP_ABE_S_CTRL_SIZE                               0x90
#define OMAP_ABE_S_VIBRA1_IN_ADDR                          0x3938
#define OMAP_ABE_S_VIBRA1_IN_SIZE                          0x30
#define OMAP_ABE_S_VIBRA1_TEMP_ADDR                        0x3968
#define OMAP_ABE_S_VIBRA1_TEMP_SIZE                        0xC0
#define OMAP_ABE_S_VIBRACTRL_FORLEFTSM_ADDR                0x3A28
#define OMAP_ABE_S_VIBRACTRL_FORLEFTSM_SIZE                0xC0
#define OMAP_ABE_S_VIBRA1_MEM_ADDR                         0x3AE8
#define OMAP_ABE_S_VIBRA1_MEM_SIZE                         0x58
#define OMAP_ABE_S_VIBRACTRL_STEREO_ADDR                   0x3B40
#define OMAP_ABE_S_VIBRACTRL_STEREO_SIZE                   0xC0
#define OMAP_ABE_S_AMIC_96_48_DATA_ADDR                    0x3C00
#define OMAP_ABE_S_AMIC_96_48_DATA_SIZE                    0x98
#define OMAP_ABE_S_DMIC0_96_48_DATA_ADDR                   0x3C98
#define OMAP_ABE_S_DMIC0_96_48_DATA_SIZE                   0x98
#define OMAP_ABE_S_DMIC1_96_48_DATA_ADDR                   0x3D30
#define OMAP_ABE_S_DMIC1_96_48_DATA_SIZE                   0x98
#define OMAP_ABE_S_DMIC2_96_48_DATA_ADDR                   0x3DC8
#define OMAP_ABE_S_DMIC2_96_48_DATA_SIZE                   0x98
#define OMAP_ABE_S_DBG_8K_PATTERN_ADDR                     0x3E60
#define OMAP_ABE_S_DBG_8K_PATTERN_SIZE                     0x10
#define OMAP_ABE_S_DBG_16K_PATTERN_ADDR                    0x3E70
#define OMAP_ABE_S_DBG_16K_PATTERN_SIZE                    0x20
#define OMAP_ABE_S_DBG_24K_PATTERN_ADDR                    0x3E90
#define OMAP_ABE_S_DBG_24K_PATTERN_SIZE                    0x30
#define OMAP_ABE_S_DBG_48K_PATTERN_ADDR                    0x3EC0
#define OMAP_ABE_S_DBG_48K_PATTERN_SIZE                    0x60
#define OMAP_ABE_S_DBG_96K_PATTERN_ADDR                    0x3F20
#define OMAP_ABE_S_DBG_96K_PATTERN_SIZE                    0xC0
#define OMAP_ABE_S_MM_EXT_IN_ADDR                          0x3FE0
#define OMAP_ABE_S_MM_EXT_IN_SIZE                          0x60
#define OMAP_ABE_S_MM_EXT_IN_L_ADDR                        0x4040
#define OMAP_ABE_S_MM_EXT_IN_L_SIZE                        0x60
#define OMAP_ABE_S_MM_EXT_IN_R_ADDR                        0x40A0
#define OMAP_ABE_S_MM_EXT_IN_R_SIZE                        0x60
#define OMAP_ABE_S_MIC4_ADDR                               0x4100
#define OMAP_ABE_S_MIC4_SIZE                               0x60
#define OMAP_ABE_S_MIC4_L_ADDR                             0x4160
#define OMAP_ABE_S_MIC4_L_SIZE                             0x60
#define OMAP_ABE_S_SATURATION_7FFF_ADDR                    0x41C0
#define OMAP_ABE_S_SATURATION_7FFF_SIZE                    0x8
#define OMAP_ABE_S_SATURATION_ADDR                         0x41C8
#define OMAP_ABE_S_SATURATION_SIZE                         0x8
#define OMAP_ABE_S_XINASRC_BT_UL_ADDR                      0x41D0
#define OMAP_ABE_S_XINASRC_BT_UL_SIZE                      0x140
#define OMAP_ABE_S_XINASRC_BT_DL_ADDR                      0x4310
#define OMAP_ABE_S_XINASRC_BT_DL_SIZE                      0x140
#define OMAP_ABE_S_BT_DL_8K_TEMP_ADDR                      0x4450
#define OMAP_ABE_S_BT_DL_8K_TEMP_SIZE                      0x10
#define OMAP_ABE_S_BT_DL_16K_TEMP_ADDR                     0x4460
#define OMAP_ABE_S_BT_DL_16K_TEMP_SIZE                     0x20
#define OMAP_ABE_S_VX_DL_8_48_OSR_LP_DATA_ADDR             0x4480
#define OMAP_ABE_S_VX_DL_8_48_OSR_LP_DATA_SIZE             0xE0
#define OMAP_ABE_S_BT_UL_8_48_OSR_LP_DATA_ADDR             0x4560
#define OMAP_ABE_S_BT_UL_8_48_OSR_LP_DATA_SIZE             0xE0
#define OMAP_ABE_S_MM_DL_44P1_ADDR                         0x4640
#define OMAP_ABE_S_MM_DL_44P1_SIZE                         0x300
#define OMAP_ABE_S_TONES_44P1_ADDR                         0x4940
#define OMAP_ABE_S_TONES_44P1_SIZE                         0x300
#define OMAP_ABE_S_MM_DL_44P1_XK_ADDR                      0x4C40
#define OMAP_ABE_S_MM_DL_44P1_XK_SIZE                      0x10
#define OMAP_ABE_S_TONES_44P1_XK_ADDR                      0x4C50
#define OMAP_ABE_S_TONES_44P1_XK_SIZE                      0x10
#define OMAP_ABE_S_SRC_44P1_MULFAC1_ADDR                   0x4C60
#define OMAP_ABE_S_SRC_44P1_MULFAC1_SIZE                   0x8
