/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#define OMAP_ABE_INIT_CM_ADDR                         0x0
#define OMAP_ABE_INIT_CM_SIZE                         0x4DC

#define OMAP_ABE_C_DATA_LSB_2_ADDR                    0x4DC
#define OMAP_ABE_C_DATA_LSB_2_SIZE                    0x4

#define OMAP_ABE_C_1_ALPHA_ADDR                       0x4E0
#define OMAP_ABE_C_1_ALPHA_SIZE                       0x48

#define OMAP_ABE_C_ALPHA_ADDR                         0x528
#define OMAP_ABE_C_ALPHA_SIZE                         0x48

#define OMAP_ABE_C_GAINSWRAMP_ADDR                    0x570
#define OMAP_ABE_C_GAINSWRAMP_SIZE                    0x38

#define OMAP_ABE_C_GAINS_DL1M_ADDR                    0x5A8
#define OMAP_ABE_C_GAINS_DL1M_SIZE                    0x10

#define OMAP_ABE_C_GAINS_DL2M_ADDR                    0x5B8
#define OMAP_ABE_C_GAINS_DL2M_SIZE                    0x10

#define OMAP_ABE_C_GAINS_ECHOM_ADDR                   0x5C8
#define OMAP_ABE_C_GAINS_ECHOM_SIZE                   0x8

#define OMAP_ABE_C_GAINS_SDTM_ADDR                    0x5D0
#define OMAP_ABE_C_GAINS_SDTM_SIZE                    0x8

#define OMAP_ABE_C_GAINS_VXRECM_ADDR                  0x5D8
#define OMAP_ABE_C_GAINS_VXRECM_SIZE                  0x10

#define OMAP_ABE_C_GAINS_ULM_ADDR                     0x5E8
#define OMAP_ABE_C_GAINS_ULM_SIZE                     0x10

#define OMAP_ABE_C_GAINS_BTUL_ADDR                    0x5F8
#define OMAP_ABE_C_GAINS_BTUL_SIZE                    0x8

#define OMAP_ABE_C_SDT_COEFS_ADDR                     0x600
#define OMAP_ABE_C_SDT_COEFS_SIZE                     0x24

#define OMAP_ABE_C_COEFASRC1_VX_ADDR                  0x624
#define OMAP_ABE_C_COEFASRC1_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC2_VX_ADDR                  0x670
#define OMAP_ABE_C_COEFASRC2_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC3_VX_ADDR                  0x6BC
#define OMAP_ABE_C_COEFASRC3_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC4_VX_ADDR                  0x708
#define OMAP_ABE_C_COEFASRC4_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC5_VX_ADDR                  0x754
#define OMAP_ABE_C_COEFASRC5_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC6_VX_ADDR                  0x7A0
#define OMAP_ABE_C_COEFASRC6_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC7_VX_ADDR                  0x7EC
#define OMAP_ABE_C_COEFASRC7_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC8_VX_ADDR                  0x838
#define OMAP_ABE_C_COEFASRC8_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC9_VX_ADDR                  0x884
#define OMAP_ABE_C_COEFASRC9_VX_SIZE                  0x4C

#define OMAP_ABE_C_COEFASRC10_VX_ADDR                 0x8D0
#define OMAP_ABE_C_COEFASRC10_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC11_VX_ADDR                 0x91C
#define OMAP_ABE_C_COEFASRC11_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC12_VX_ADDR                 0x968
#define OMAP_ABE_C_COEFASRC12_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC13_VX_ADDR                 0x9B4
#define OMAP_ABE_C_COEFASRC13_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC14_VX_ADDR                 0xA00
#define OMAP_ABE_C_COEFASRC14_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC15_VX_ADDR                 0xA4C
#define OMAP_ABE_C_COEFASRC15_VX_SIZE                 0x4C

#define OMAP_ABE_C_COEFASRC16_VX_ADDR                 0xA98
#define OMAP_ABE_C_COEFASRC16_VX_SIZE                 0x4C

#define OMAP_ABE_C_ALPHACURRENT_UL_VX_ADDR            0xAE4
#define OMAP_ABE_C_ALPHACURRENT_UL_VX_SIZE            0x4

#define OMAP_ABE_C_BETACURRENT_UL_VX_ADDR             0xAE8
#define OMAP_ABE_C_BETACURRENT_UL_VX_SIZE             0x4

#define OMAP_ABE_C_ALPHACURRENT_DL_VX_ADDR            0xAEC
#define OMAP_ABE_C_ALPHACURRENT_DL_VX_SIZE            0x4

#define OMAP_ABE_C_BETACURRENT_DL_VX_ADDR             0xAF0
#define OMAP_ABE_C_BETACURRENT_DL_VX_SIZE             0x4

#define OMAP_ABE_C_COEFASRC1_MM_ADDR                  0xAF4
#define OMAP_ABE_C_COEFASRC1_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC2_MM_ADDR                  0xB3C
#define OMAP_ABE_C_COEFASRC2_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC3_MM_ADDR                  0xB84
#define OMAP_ABE_C_COEFASRC3_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC4_MM_ADDR                  0xBCC
#define OMAP_ABE_C_COEFASRC4_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC5_MM_ADDR                  0xC14
#define OMAP_ABE_C_COEFASRC5_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC6_MM_ADDR                  0xC5C
#define OMAP_ABE_C_COEFASRC6_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC7_MM_ADDR                  0xCA4
#define OMAP_ABE_C_COEFASRC7_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC8_MM_ADDR                  0xCEC
#define OMAP_ABE_C_COEFASRC8_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC9_MM_ADDR                  0xD34
#define OMAP_ABE_C_COEFASRC9_MM_SIZE                  0x48

#define OMAP_ABE_C_COEFASRC10_MM_ADDR                 0xD7C
#define OMAP_ABE_C_COEFASRC10_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC11_MM_ADDR                 0xDC4
#define OMAP_ABE_C_COEFASRC11_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC12_MM_ADDR                 0xE0C
#define OMAP_ABE_C_COEFASRC12_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC13_MM_ADDR                 0xE54
#define OMAP_ABE_C_COEFASRC13_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC14_MM_ADDR                 0xE9C
#define OMAP_ABE_C_COEFASRC14_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC15_MM_ADDR                 0xEE4
#define OMAP_ABE_C_COEFASRC15_MM_SIZE                 0x48

#define OMAP_ABE_C_COEFASRC16_MM_ADDR                 0xF2C
#define OMAP_ABE_C_COEFASRC16_MM_SIZE                 0x48

#define OMAP_ABE_C_ALPHACURRENT_MM_EXT_IN_ADDR        0xF74
#define OMAP_ABE_C_ALPHACURRENT_MM_EXT_IN_SIZE        0x4

#define OMAP_ABE_C_BETACURRENT_MM_EXT_IN_ADDR         0xF78
#define OMAP_ABE_C_BETACURRENT_MM_EXT_IN_SIZE         0x4

#define OMAP_ABE_C_DL2_L_COEFS_ADDR                   0xF7C
#define OMAP_ABE_C_DL2_L_COEFS_SIZE                   0x64

#define OMAP_ABE_C_DL2_R_COEFS_ADDR                   0xFE0
#define OMAP_ABE_C_DL2_R_COEFS_SIZE                   0x64

#define OMAP_ABE_C_DL1_COEFS_ADDR                     0x1044
#define OMAP_ABE_C_DL1_COEFS_SIZE                     0x64

#define OMAP_ABE_C_SRC_3_LP_COEFS_ADDR                0x10A8
#define OMAP_ABE_C_SRC_3_LP_COEFS_SIZE                0x2C

#define OMAP_ABE_C_SRC_3_LP_GAIN_COEFS_ADDR           0x10D4
#define OMAP_ABE_C_SRC_3_LP_GAIN_COEFS_SIZE           0x2C

#define OMAP_ABE_C_SRC_3_HP_COEFS_ADDR                0x1100
#define OMAP_ABE_C_SRC_3_HP_COEFS_SIZE                0x14

#define OMAP_ABE_C_SRC_6_LP_COEFS_ADDR                0x1114
#define OMAP_ABE_C_SRC_6_LP_COEFS_SIZE                0x2C

#define OMAP_ABE_C_SRC_6_LP_GAIN_COEFS_ADDR           0x1140
#define OMAP_ABE_C_SRC_6_LP_GAIN_COEFS_SIZE           0x2C

#define OMAP_ABE_C_SRC_6_HP_COEFS_ADDR                0x116C
#define OMAP_ABE_C_SRC_6_HP_COEFS_SIZE                0x1C

#define OMAP_ABE_C_APS_DL1_COEFFS1_ADDR               0x1188
#define OMAP_ABE_C_APS_DL1_COEFFS1_SIZE               0x24

#define OMAP_ABE_C_APS_DL1_M_COEFFS2_ADDR             0x11AC
#define OMAP_ABE_C_APS_DL1_M_COEFFS2_SIZE             0xC

#define OMAP_ABE_C_APS_DL1_C_COEFFS2_ADDR             0x11B8
#define OMAP_ABE_C_APS_DL1_C_COEFFS2_SIZE             0xC

#define OMAP_ABE_C_APS_DL2_L_COEFFS1_ADDR             0x11C4
#define OMAP_ABE_C_APS_DL2_L_COEFFS1_SIZE             0x24

#define OMAP_ABE_C_APS_DL2_R_COEFFS1_ADDR             0x11E8
#define OMAP_ABE_C_APS_DL2_R_COEFFS1_SIZE             0x24

#define OMAP_ABE_C_APS_DL2_L_M_COEFFS2_ADDR           0x120C
#define OMAP_ABE_C_APS_DL2_L_M_COEFFS2_SIZE           0xC

#define OMAP_ABE_C_APS_DL2_R_M_COEFFS2_ADDR           0x1218
#define OMAP_ABE_C_APS_DL2_R_M_COEFFS2_SIZE           0xC

#define OMAP_ABE_C_APS_DL2_L_C_COEFFS2_ADDR           0x1224
#define OMAP_ABE_C_APS_DL2_L_C_COEFFS2_SIZE           0xC

#define OMAP_ABE_C_APS_DL2_R_C_COEFFS2_ADDR           0x1230
#define OMAP_ABE_C_APS_DL2_R_C_COEFFS2_SIZE           0xC

#define OMAP_ABE_C_ALPHACURRENT_ECHO_REF_ADDR         0x123C
#define OMAP_ABE_C_ALPHACURRENT_ECHO_REF_SIZE         0x4

#define OMAP_ABE_C_BETACURRENT_ECHO_REF_ADDR          0x1240
#define OMAP_ABE_C_BETACURRENT_ECHO_REF_SIZE          0x4

#define OMAP_ABE_C_APS_DL1_EQ_ADDR                    0x1244
#define OMAP_ABE_C_APS_DL1_EQ_SIZE                    0x24

#define OMAP_ABE_C_APS_DL2_L_EQ_ADDR                  0x1268
#define OMAP_ABE_C_APS_DL2_L_EQ_SIZE                  0x24

#define OMAP_ABE_C_APS_DL2_R_EQ_ADDR                  0x128C
#define OMAP_ABE_C_APS_DL2_R_EQ_SIZE                  0x24

#define OMAP_ABE_C_VIBRA2_CONSTS_ADDR                 0x12B0
#define OMAP_ABE_C_VIBRA2_CONSTS_SIZE                 0x10

#define OMAP_ABE_C_VIBRA1_COEFFS_ADDR                 0x12C0
#define OMAP_ABE_C_VIBRA1_COEFFS_SIZE                 0x2C

#define OMAP_ABE_C_48_96_LP_COEFS_ADDR                0x12EC
#define OMAP_ABE_C_48_96_LP_COEFS_SIZE                0x3C

#define OMAP_ABE_C_96_48_AMIC_COEFS_ADDR              0x1328
#define OMAP_ABE_C_96_48_AMIC_COEFS_SIZE              0x4C

#define OMAP_ABE_C_96_48_DMIC_COEFS_ADDR              0x1374
#define OMAP_ABE_C_96_48_DMIC_COEFS_SIZE              0x4C

#define OMAP_ABE_C_INPUT_SCALE_ADDR                   0x13C0
#define OMAP_ABE_C_INPUT_SCALE_SIZE                   0x4

#define OMAP_ABE_C_OUTPUT_SCALE_ADDR                  0x13C4
#define OMAP_ABE_C_OUTPUT_SCALE_SIZE                  0x4

#define OMAP_ABE_C_MUTE_SCALING_ADDR                  0x13C8
#define OMAP_ABE_C_MUTE_SCALING_SIZE                  0x4

#define OMAP_ABE_C_GAINS_0DB_ADDR                     0x13CC
#define OMAP_ABE_C_GAINS_0DB_SIZE                     0x8

#define OMAP_ABE_C_ALPHACURRENT_BT_UL_ADDR            0x13D4
#define OMAP_ABE_C_ALPHACURRENT_BT_UL_SIZE            0x4

#define OMAP_ABE_C_BETACURRENT_BT_UL_ADDR             0x13D8
#define OMAP_ABE_C_BETACURRENT_BT_UL_SIZE             0x4

#define OMAP_ABE_C_ALPHACURRENT_BT_DL_ADDR            0x13DC
#define OMAP_ABE_C_ALPHACURRENT_BT_DL_SIZE            0x4

#define OMAP_ABE_C_BETACURRENT_BT_DL_ADDR             0x13E0
#define OMAP_ABE_C_BETACURRENT_BT_DL_SIZE             0x4
