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
#define OMAP_ABE_INIT_CM_ADDR                              0x0
#define OMAP_ABE_INIT_CM_SIZE                              0x640
#define OMAP_ABE_C_DATA_LSB_2_ADDR                         0x640
#define OMAP_ABE_C_DATA_LSB_2_SIZE                         0x4
#define OMAP_ABE_C_1_ALPHA_ADDR                            0x644
#define OMAP_ABE_C_1_ALPHA_SIZE                            0x48
#define OMAP_ABE_C_ALPHA_ADDR                              0x68C
#define OMAP_ABE_C_ALPHA_SIZE                              0x48
#define OMAP_ABE_C_GAINSWRAMP_ADDR                         0x6D4
#define OMAP_ABE_C_GAINSWRAMP_SIZE                         0x38
#define OMAP_ABE_C_GAINS_DL1M_ADDR                         0x70C
#define OMAP_ABE_C_GAINS_DL1M_SIZE                         0x10
#define OMAP_ABE_C_GAINS_DL2M_ADDR                         0x71C
#define OMAP_ABE_C_GAINS_DL2M_SIZE                         0x10
#define OMAP_ABE_C_GAINS_ECHOM_ADDR                        0x72C
#define OMAP_ABE_C_GAINS_ECHOM_SIZE                        0x8
#define OMAP_ABE_C_GAINS_SDTM_ADDR                         0x734
#define OMAP_ABE_C_GAINS_SDTM_SIZE                         0x8
#define OMAP_ABE_C_GAINS_VXRECM_ADDR                       0x73C
#define OMAP_ABE_C_GAINS_VXRECM_SIZE                       0x10
#define OMAP_ABE_C_GAINS_ULM_ADDR                          0x74C
#define OMAP_ABE_C_GAINS_ULM_SIZE                          0x10
#define OMAP_ABE_C_GAINS_BTUL_ADDR                         0x75C
#define OMAP_ABE_C_GAINS_BTUL_SIZE                         0x8
#define OMAP_ABE_C_SDT_COEFS_ADDR                          0x764
#define OMAP_ABE_C_SDT_COEFS_SIZE                          0x24
#define OMAP_ABE_C_COEFASRC1_VX_ADDR                       0x788
#define OMAP_ABE_C_COEFASRC1_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC2_VX_ADDR                       0x7D4
#define OMAP_ABE_C_COEFASRC2_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC3_VX_ADDR                       0x820
#define OMAP_ABE_C_COEFASRC3_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC4_VX_ADDR                       0x86C
#define OMAP_ABE_C_COEFASRC4_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC5_VX_ADDR                       0x8B8
#define OMAP_ABE_C_COEFASRC5_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC6_VX_ADDR                       0x904
#define OMAP_ABE_C_COEFASRC6_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC7_VX_ADDR                       0x950
#define OMAP_ABE_C_COEFASRC7_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC8_VX_ADDR                       0x99C
#define OMAP_ABE_C_COEFASRC8_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC9_VX_ADDR                       0x9E8
#define OMAP_ABE_C_COEFASRC9_VX_SIZE                       0x4C
#define OMAP_ABE_C_COEFASRC10_VX_ADDR                      0xA34
#define OMAP_ABE_C_COEFASRC10_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC11_VX_ADDR                      0xA80
#define OMAP_ABE_C_COEFASRC11_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC12_VX_ADDR                      0xACC
#define OMAP_ABE_C_COEFASRC12_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC13_VX_ADDR                      0xB18
#define OMAP_ABE_C_COEFASRC13_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC14_VX_ADDR                      0xB64
#define OMAP_ABE_C_COEFASRC14_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC15_VX_ADDR                      0xBB0
#define OMAP_ABE_C_COEFASRC15_VX_SIZE                      0x4C
#define OMAP_ABE_C_COEFASRC16_VX_ADDR                      0xBFC
#define OMAP_ABE_C_COEFASRC16_VX_SIZE                      0x4C
#define OMAP_ABE_C_ALPHACURRENT_UL_VX_ADDR                 0xC48
#define OMAP_ABE_C_ALPHACURRENT_UL_VX_SIZE                 0x4
#define OMAP_ABE_C_BETACURRENT_UL_VX_ADDR                  0xC4C
#define OMAP_ABE_C_BETACURRENT_UL_VX_SIZE                  0x4
#define OMAP_ABE_C_ALPHACURRENT_DL_VX_ADDR                 0xC50
#define OMAP_ABE_C_ALPHACURRENT_DL_VX_SIZE                 0x4
#define OMAP_ABE_C_BETACURRENT_DL_VX_ADDR                  0xC54
#define OMAP_ABE_C_BETACURRENT_DL_VX_SIZE                  0x4
#define OMAP_ABE_C_COEFASRC1_MM_ADDR                       0xC58
#define OMAP_ABE_C_COEFASRC1_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC2_MM_ADDR                       0xCA0
#define OMAP_ABE_C_COEFASRC2_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC3_MM_ADDR                       0xCE8
#define OMAP_ABE_C_COEFASRC3_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC4_MM_ADDR                       0xD30
#define OMAP_ABE_C_COEFASRC4_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC5_MM_ADDR                       0xD78
#define OMAP_ABE_C_COEFASRC5_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC6_MM_ADDR                       0xDC0
#define OMAP_ABE_C_COEFASRC6_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC7_MM_ADDR                       0xE08
#define OMAP_ABE_C_COEFASRC7_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC8_MM_ADDR                       0xE50
#define OMAP_ABE_C_COEFASRC8_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC9_MM_ADDR                       0xE98
#define OMAP_ABE_C_COEFASRC9_MM_SIZE                       0x48
#define OMAP_ABE_C_COEFASRC10_MM_ADDR                      0xEE0
#define OMAP_ABE_C_COEFASRC10_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC11_MM_ADDR                      0xF28
#define OMAP_ABE_C_COEFASRC11_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC12_MM_ADDR                      0xF70
#define OMAP_ABE_C_COEFASRC12_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC13_MM_ADDR                      0xFB8
#define OMAP_ABE_C_COEFASRC13_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC14_MM_ADDR                      0x1000
#define OMAP_ABE_C_COEFASRC14_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC15_MM_ADDR                      0x1048
#define OMAP_ABE_C_COEFASRC15_MM_SIZE                      0x48
#define OMAP_ABE_C_COEFASRC16_MM_ADDR                      0x1090
#define OMAP_ABE_C_COEFASRC16_MM_SIZE                      0x48
#define OMAP_ABE_C_ALPHACURRENT_MM_EXT_IN_ADDR             0x10D8
#define OMAP_ABE_C_ALPHACURRENT_MM_EXT_IN_SIZE             0x4
#define OMAP_ABE_C_BETACURRENT_MM_EXT_IN_ADDR              0x10DC
#define OMAP_ABE_C_BETACURRENT_MM_EXT_IN_SIZE              0x4
#define OMAP_ABE_C_DL2_L_COEFS_ADDR                        0x10E0
#define OMAP_ABE_C_DL2_L_COEFS_SIZE                        0x64
#define OMAP_ABE_C_DL2_R_COEFS_ADDR                        0x1144
#define OMAP_ABE_C_DL2_R_COEFS_SIZE                        0x64
#define OMAP_ABE_C_DL1_COEFS_ADDR                          0x11A8
#define OMAP_ABE_C_DL1_COEFS_SIZE                          0x64
#define OMAP_ABE_C_SRC_3_LP_COEFS_ADDR                     0x120C
#define OMAP_ABE_C_SRC_3_LP_COEFS_SIZE                     0x34
#define OMAP_ABE_C_SRC_3_LP_GAIN_COEFS_ADDR                0x1240
#define OMAP_ABE_C_SRC_3_LP_GAIN_COEFS_SIZE                0x34
#define OMAP_ABE_C_SRC_3_HP_COEFS_ADDR                     0x1274
#define OMAP_ABE_C_SRC_3_HP_COEFS_SIZE                     0x14
#define OMAP_ABE_C_SRC_6_LP_COEFS_ADDR                     0x1288
#define OMAP_ABE_C_SRC_6_LP_COEFS_SIZE                     0x34
#define OMAP_ABE_C_SRC_6_LP_GAIN_COEFS_ADDR                0x12BC
#define OMAP_ABE_C_SRC_6_LP_GAIN_COEFS_SIZE                0x34
#define OMAP_ABE_C_SRC_6_HP_COEFS_ADDR                     0x12F0
#define OMAP_ABE_C_SRC_6_HP_COEFS_SIZE                     0x1C
#define OMAP_ABE_C_ALPHACURRENT_ECHO_REF_ADDR              0x130C
#define OMAP_ABE_C_ALPHACURRENT_ECHO_REF_SIZE              0x4
#define OMAP_ABE_C_BETACURRENT_ECHO_REF_ADDR               0x1310
#define OMAP_ABE_C_BETACURRENT_ECHO_REF_SIZE               0x4
#define OMAP_ABE_C_VIBRA2_CONSTS_ADDR                      0x1314
#define OMAP_ABE_C_VIBRA2_CONSTS_SIZE                      0x10
#define OMAP_ABE_C_VIBRA1_COEFFS_ADDR                      0x1324
#define OMAP_ABE_C_VIBRA1_COEFFS_SIZE                      0x2C
#define OMAP_ABE_C_48_96_LP_COEFS_ADDR                     0x1350
#define OMAP_ABE_C_48_96_LP_COEFS_SIZE                     0x3C
#define OMAP_ABE_C_96_48_AMIC_COEFS_ADDR                   0x138C
#define OMAP_ABE_C_96_48_AMIC_COEFS_SIZE                   0x4C
#define OMAP_ABE_C_96_48_DMIC_COEFS_ADDR                   0x13D8
#define OMAP_ABE_C_96_48_DMIC_COEFS_SIZE                   0x4C
#define OMAP_ABE_C_INPUT_SCALE_ADDR                        0x1424
#define OMAP_ABE_C_INPUT_SCALE_SIZE                        0x4
#define OMAP_ABE_C_OUTPUT_SCALE_ADDR                       0x1428
#define OMAP_ABE_C_OUTPUT_SCALE_SIZE                       0x4
#define OMAP_ABE_C_MUTE_SCALING_ADDR                       0x142C
#define OMAP_ABE_C_MUTE_SCALING_SIZE                       0x4
#define OMAP_ABE_C_GAINS_0DB_ADDR                          0x1430
#define OMAP_ABE_C_GAINS_0DB_SIZE                          0x8
#define OMAP_ABE_C_ALPHACURRENT_BT_DL_ADDR                 0x1438
#define OMAP_ABE_C_ALPHACURRENT_BT_DL_SIZE                 0x4
#define OMAP_ABE_C_BETACURRENT_BT_DL_ADDR                  0x143C
#define OMAP_ABE_C_BETACURRENT_BT_DL_SIZE                  0x4
#define OMAP_ABE_C_ALPHACURRENT_BT_UL_ADDR                 0x1440
#define OMAP_ABE_C_ALPHACURRENT_BT_UL_SIZE                 0x4
#define OMAP_ABE_C_BETACURRENT_BT_UL_ADDR                  0x1444
#define OMAP_ABE_C_BETACURRENT_BT_UL_SIZE                  0x4
#define OMAP_ABE_C_SRC_FIR6_LP_GAIN_COEFS_ADDR             0x1448
#define OMAP_ABE_C_SRC_FIR6_LP_GAIN_COEFS_SIZE             0x2A0
#define OMAP_ABE_C_SRC_44P1_COEFS_ADDR                     0x16E8
#define OMAP_ABE_C_SRC_44P1_COEFS_SIZE                     0x480
#define OMAP_ABE_C_SRC_MM_DL_44P1_STEP_ADDR                0x1B68
#define OMAP_ABE_C_SRC_MM_DL_44P1_STEP_SIZE                0x8
#define OMAP_ABE_C_SRC_TONES_44P1_STEP_ADDR                0x1B70
#define OMAP_ABE_C_SRC_TONES_44P1_STEP_SIZE                0x8
#define OMAP_ABE_C_SRC_44P1_MULFAC2_ADDR                   0x1B78
#define OMAP_ABE_C_SRC_44P1_MULFAC2_SIZE                   0x8
