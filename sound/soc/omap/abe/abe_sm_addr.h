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

#define OMAP_ABE_INIT_SM_ADDR                         0x0
#define OMAP_ABE_INIT_SM_SIZE                         0x9B8

#define OMAP_ABE_S_DATA0_ADDR                         0x9B8
#define OMAP_ABE_S_DATA0_SIZE                         0x8

#define OMAP_ABE_S_TEMP_ADDR                          0x9C0
#define OMAP_ABE_S_TEMP_SIZE                          0x8

#define OMAP_ABE_S_PHOENIXOFFSET_ADDR                 0x9C8
#define OMAP_ABE_S_PHOENIXOFFSET_SIZE                 0x8

#define OMAP_ABE_S_GTARGET1_ADDR                      0x9D0
#define OMAP_ABE_S_GTARGET1_SIZE                      0x38

#define OMAP_ABE_S_GTARGET_DL1_ADDR                   0xA08
#define OMAP_ABE_S_GTARGET_DL1_SIZE                   0x10

#define OMAP_ABE_S_GTARGET_DL2_ADDR                   0xA18
#define OMAP_ABE_S_GTARGET_DL2_SIZE                   0x10

#define OMAP_ABE_S_GTARGET_ECHO_ADDR                  0xA28
#define OMAP_ABE_S_GTARGET_ECHO_SIZE                  0x8

#define OMAP_ABE_S_GTARGET_SDT_ADDR                   0xA30
#define OMAP_ABE_S_GTARGET_SDT_SIZE                   0x8

#define OMAP_ABE_S_GTARGET_VXREC_ADDR                 0xA38
#define OMAP_ABE_S_GTARGET_VXREC_SIZE                 0x10

#define OMAP_ABE_S_GTARGET_UL_ADDR                    0xA48
#define OMAP_ABE_S_GTARGET_UL_SIZE                    0x10

#define OMAP_ABE_S_GTARGET_BTUL_ADDR                  0xA58
#define OMAP_ABE_S_GTARGET_BTUL_SIZE                  0x8

#define OMAP_ABE_S_GCURRENT_ADDR                      0xA60
#define OMAP_ABE_S_GCURRENT_SIZE                      0x90

#define OMAP_ABE_S_GAIN_ONE_ADDR                      0xAF0
#define OMAP_ABE_S_GAIN_ONE_SIZE                      0x8

#define OMAP_ABE_S_TONES_ADDR                         0xAF8
#define OMAP_ABE_S_TONES_SIZE                         0x60

#define OMAP_ABE_S_VX_DL_ADDR                         0xB58
#define OMAP_ABE_S_VX_DL_SIZE                         0x60

#define OMAP_ABE_S_MM_UL2_ADDR                        0xBB8
#define OMAP_ABE_S_MM_UL2_SIZE                        0x60

#define OMAP_ABE_S_MM_DL_ADDR                         0xC18
#define OMAP_ABE_S_MM_DL_SIZE                         0x60

#define OMAP_ABE_S_DL1_M_OUT_ADDR                     0xC78
#define OMAP_ABE_S_DL1_M_OUT_SIZE                     0x60

#define OMAP_ABE_S_DL2_M_OUT_ADDR                     0xCD8
#define OMAP_ABE_S_DL2_M_OUT_SIZE                     0x60

#define OMAP_ABE_S_ECHO_M_OUT_ADDR                    0xD38
#define OMAP_ABE_S_ECHO_M_OUT_SIZE                    0x60

#define OMAP_ABE_S_SDT_M_OUT_ADDR                     0xD98
#define OMAP_ABE_S_SDT_M_OUT_SIZE                     0x60

#define OMAP_ABE_S_VX_UL_ADDR                         0xDF8
#define OMAP_ABE_S_VX_UL_SIZE                         0x60

#define OMAP_ABE_S_VX_UL_M_ADDR                       0xE58
#define OMAP_ABE_S_VX_UL_M_SIZE                       0x60

#define OMAP_ABE_S_BT_DL_ADDR                         0xEB8
#define OMAP_ABE_S_BT_DL_SIZE                         0x60

#define OMAP_ABE_S_BT_UL_ADDR                         0xF18
#define OMAP_ABE_S_BT_UL_SIZE                         0x60

#define OMAP_ABE_S_BT_DL_8K_ADDR                      0xF78
#define OMAP_ABE_S_BT_DL_8K_SIZE                      0x18

#define OMAP_ABE_S_BT_DL_16K_ADDR                     0xF90
#define OMAP_ABE_S_BT_DL_16K_SIZE                     0x28

#define OMAP_ABE_S_BT_UL_8K_ADDR                      0xFB8
#define OMAP_ABE_S_BT_UL_8K_SIZE                      0x10

#define OMAP_ABE_S_BT_UL_16K_ADDR                     0xFC8
#define OMAP_ABE_S_BT_UL_16K_SIZE                     0x20

#define OMAP_ABE_S_SDT_F_ADDR                         0xFE8
#define OMAP_ABE_S_SDT_F_SIZE                         0x60

#define OMAP_ABE_S_SDT_F_DATA_ADDR                    0x1048
#define OMAP_ABE_S_SDT_F_DATA_SIZE                    0x48

#define OMAP_ABE_S_MM_DL_OSR_ADDR                     0x1090
#define OMAP_ABE_S_MM_DL_OSR_SIZE                     0xC0

#define OMAP_ABE_S_24_ZEROS_ADDR                      0x1150
#define OMAP_ABE_S_24_ZEROS_SIZE                      0xC0

#define OMAP_ABE_S_DMIC1_ADDR                         0x1210
#define OMAP_ABE_S_DMIC1_SIZE                         0x60

#define OMAP_ABE_S_DMIC2_ADDR                         0x1270
#define OMAP_ABE_S_DMIC2_SIZE                         0x60

#define OMAP_ABE_S_DMIC3_ADDR                         0x12D0
#define OMAP_ABE_S_DMIC3_SIZE                         0x60

#define OMAP_ABE_S_AMIC_ADDR                          0x1330
#define OMAP_ABE_S_AMIC_SIZE                          0x60

#define OMAP_ABE_S_DMIC1_L_ADDR                       0x1390
#define OMAP_ABE_S_DMIC1_L_SIZE                       0x60

#define OMAP_ABE_S_DMIC1_R_ADDR                       0x13F0
#define OMAP_ABE_S_DMIC1_R_SIZE                       0x60

#define OMAP_ABE_S_DMIC2_L_ADDR                       0x1450
#define OMAP_ABE_S_DMIC2_L_SIZE                       0x60

#define OMAP_ABE_S_DMIC2_R_ADDR                       0x14B0
#define OMAP_ABE_S_DMIC2_R_SIZE                       0x60

#define OMAP_ABE_S_DMIC3_L_ADDR                       0x1510
#define OMAP_ABE_S_DMIC3_L_SIZE                       0x60

#define OMAP_ABE_S_DMIC3_R_ADDR                       0x1570
#define OMAP_ABE_S_DMIC3_R_SIZE                       0x60

#define OMAP_ABE_S_BT_UL_L_ADDR                       0x15D0
#define OMAP_ABE_S_BT_UL_L_SIZE                       0x60

#define OMAP_ABE_S_BT_UL_R_ADDR                       0x1630
#define OMAP_ABE_S_BT_UL_R_SIZE                       0x60

#define OMAP_ABE_S_AMIC_L_ADDR                        0x1690
#define OMAP_ABE_S_AMIC_L_SIZE                        0x60

#define OMAP_ABE_S_AMIC_R_ADDR                        0x16F0
#define OMAP_ABE_S_AMIC_R_SIZE                        0x60

#define OMAP_ABE_S_ECHOREF_L_ADDR                     0x1750
#define OMAP_ABE_S_ECHOREF_L_SIZE                     0x60

#define OMAP_ABE_S_ECHOREF_R_ADDR                     0x17B0
#define OMAP_ABE_S_ECHOREF_R_SIZE                     0x60

#define OMAP_ABE_S_MM_DL_L_ADDR                       0x1810
#define OMAP_ABE_S_MM_DL_L_SIZE                       0x60

#define OMAP_ABE_S_MM_DL_R_ADDR                       0x1870
#define OMAP_ABE_S_MM_DL_R_SIZE                       0x60

#define OMAP_ABE_S_MM_UL_ADDR                         0x18D0
#define OMAP_ABE_S_MM_UL_SIZE                         0x3C0

#define OMAP_ABE_S_AMIC_96K_ADDR                      0x1C90
#define OMAP_ABE_S_AMIC_96K_SIZE                      0xC0

#define OMAP_ABE_S_DMIC0_96K_ADDR                     0x1D50
#define OMAP_ABE_S_DMIC0_96K_SIZE                     0xC0

#define OMAP_ABE_S_DMIC1_96K_ADDR                     0x1E10
#define OMAP_ABE_S_DMIC1_96K_SIZE                     0xC0

#define OMAP_ABE_S_DMIC2_96K_ADDR                     0x1ED0
#define OMAP_ABE_S_DMIC2_96K_SIZE                     0xC0

#define OMAP_ABE_S_UL_VX_UL_48_8K_ADDR                0x1F90
#define OMAP_ABE_S_UL_VX_UL_48_8K_SIZE                0x60

#define OMAP_ABE_S_UL_VX_UL_48_16K_ADDR               0x1FF0
#define OMAP_ABE_S_UL_VX_UL_48_16K_SIZE               0x60

#define OMAP_ABE_S_UL_MIC_48K_ADDR                    0x2050
#define OMAP_ABE_S_UL_MIC_48K_SIZE                    0x60

#define OMAP_ABE_S_VOICE_8K_UL_ADDR                   0x20B0
#define OMAP_ABE_S_VOICE_8K_UL_SIZE                   0x18

#define OMAP_ABE_S_VOICE_8K_DL_ADDR                   0x20C8
#define OMAP_ABE_S_VOICE_8K_DL_SIZE                   0x10

#define OMAP_ABE_S_MCPDM_OUT1_ADDR                    0x20D8
#define OMAP_ABE_S_MCPDM_OUT1_SIZE                    0xC0

#define OMAP_ABE_S_MCPDM_OUT2_ADDR                    0x2198
#define OMAP_ABE_S_MCPDM_OUT2_SIZE                    0xC0

#define OMAP_ABE_S_MCPDM_OUT3_ADDR                    0x2258
#define OMAP_ABE_S_MCPDM_OUT3_SIZE                    0xC0

#define OMAP_ABE_S_VOICE_16K_UL_ADDR                  0x2318
#define OMAP_ABE_S_VOICE_16K_UL_SIZE                  0x28

#define OMAP_ABE_S_VOICE_16K_DL_ADDR                  0x2340
#define OMAP_ABE_S_VOICE_16K_DL_SIZE                  0x20

#define OMAP_ABE_S_XINASRC_DL_VX_ADDR                 0x2360
#define OMAP_ABE_S_XINASRC_DL_VX_SIZE                 0x140

#define OMAP_ABE_S_XINASRC_UL_VX_ADDR                 0x24A0
#define OMAP_ABE_S_XINASRC_UL_VX_SIZE                 0x140

#define OMAP_ABE_S_XINASRC_MM_EXT_IN_ADDR             0x25E0
#define OMAP_ABE_S_XINASRC_MM_EXT_IN_SIZE             0x140

#define OMAP_ABE_S_VX_REC_ADDR                        0x2720
#define OMAP_ABE_S_VX_REC_SIZE                        0x60

#define OMAP_ABE_S_VX_REC_L_ADDR                      0x2780
#define OMAP_ABE_S_VX_REC_L_SIZE                      0x60

#define OMAP_ABE_S_VX_REC_R_ADDR                      0x27E0
#define OMAP_ABE_S_VX_REC_R_SIZE                      0x60

#define OMAP_ABE_S_DL2_M_L_ADDR                       0x2840
#define OMAP_ABE_S_DL2_M_L_SIZE                       0x60

#define OMAP_ABE_S_DL2_M_R_ADDR                       0x28A0
#define OMAP_ABE_S_DL2_M_R_SIZE                       0x60

#define OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR              0x2900
#define OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE              0xC8

#define OMAP_ABE_S_DL1_M_EQ_DATA_ADDR                 0x29C8
#define OMAP_ABE_S_DL1_M_EQ_DATA_SIZE                 0xC8

#define OMAP_ABE_S_EARP_48_96_LP_DATA_ADDR            0x2A90
#define OMAP_ABE_S_EARP_48_96_LP_DATA_SIZE            0x78

#define OMAP_ABE_S_IHF_48_96_LP_DATA_ADDR             0x2B08
#define OMAP_ABE_S_IHF_48_96_LP_DATA_SIZE             0x78

#define OMAP_ABE_S_VX_UL_8_TEMP_ADDR                  0x2B80
#define OMAP_ABE_S_VX_UL_8_TEMP_SIZE                  0x10

#define OMAP_ABE_S_VX_UL_16_TEMP_ADDR                 0x2B90
#define OMAP_ABE_S_VX_UL_16_TEMP_SIZE                 0x20

#define OMAP_ABE_S_VX_DL_8_48_LP_DATA_ADDR            0x2BB0
#define OMAP_ABE_S_VX_DL_8_48_LP_DATA_SIZE            0x58

#define OMAP_ABE_S_VX_DL_8_48_HP_DATA_ADDR            0x2C08
#define OMAP_ABE_S_VX_DL_8_48_HP_DATA_SIZE            0x38

#define OMAP_ABE_S_VX_DL_16_48_LP_DATA_ADDR           0x2C40
#define OMAP_ABE_S_VX_DL_16_48_LP_DATA_SIZE           0x58

#define OMAP_ABE_S_VX_DL_16_48_HP_DATA_ADDR           0x2C98
#define OMAP_ABE_S_VX_DL_16_48_HP_DATA_SIZE           0x28

#define OMAP_ABE_S_VX_UL_48_8_LP_DATA_ADDR            0x2CC0
#define OMAP_ABE_S_VX_UL_48_8_LP_DATA_SIZE            0x58

#define OMAP_ABE_S_VX_UL_48_8_HP_DATA_ADDR            0x2D18
#define OMAP_ABE_S_VX_UL_48_8_HP_DATA_SIZE            0x38

#define OMAP_ABE_S_VX_UL_48_16_LP_DATA_ADDR           0x2D50
#define OMAP_ABE_S_VX_UL_48_16_LP_DATA_SIZE           0x58

#define OMAP_ABE_S_VX_UL_48_16_HP_DATA_ADDR           0x2DA8
#define OMAP_ABE_S_VX_UL_48_16_HP_DATA_SIZE           0x38

#define OMAP_ABE_S_BT_UL_8_48_LP_DATA_ADDR            0x2DE0
#define OMAP_ABE_S_BT_UL_8_48_LP_DATA_SIZE            0x58

#define OMAP_ABE_S_BT_UL_8_48_HP_DATA_ADDR            0x2E38
#define OMAP_ABE_S_BT_UL_8_48_HP_DATA_SIZE            0x38

#define OMAP_ABE_S_BT_UL_16_48_LP_DATA_ADDR           0x2E70
#define OMAP_ABE_S_BT_UL_16_48_LP_DATA_SIZE           0x58

#define OMAP_ABE_S_BT_UL_16_48_HP_DATA_ADDR           0x2EC8
#define OMAP_ABE_S_BT_UL_16_48_HP_DATA_SIZE           0x28

#define OMAP_ABE_S_BT_DL_48_8_LP_DATA_ADDR            0x2EF0
#define OMAP_ABE_S_BT_DL_48_8_LP_DATA_SIZE            0x58

#define OMAP_ABE_S_BT_DL_48_8_HP_DATA_ADDR            0x2F48
#define OMAP_ABE_S_BT_DL_48_8_HP_DATA_SIZE            0x38

#define OMAP_ABE_S_BT_DL_48_16_LP_DATA_ADDR           0x2F80
#define OMAP_ABE_S_BT_DL_48_16_LP_DATA_SIZE           0x58

#define OMAP_ABE_S_BT_DL_48_16_HP_DATA_ADDR           0x2FD8
#define OMAP_ABE_S_BT_DL_48_16_HP_DATA_SIZE           0x28

#define OMAP_ABE_S_ECHO_REF_48_8_LP_DATA_ADDR         0x3000
#define OMAP_ABE_S_ECHO_REF_48_8_LP_DATA_SIZE         0x58

#define OMAP_ABE_S_ECHO_REF_48_8_HP_DATA_ADDR         0x3058
#define OMAP_ABE_S_ECHO_REF_48_8_HP_DATA_SIZE         0x38

#define OMAP_ABE_S_ECHO_REF_48_16_LP_DATA_ADDR        0x3090
#define OMAP_ABE_S_ECHO_REF_48_16_LP_DATA_SIZE        0x58

#define OMAP_ABE_S_ECHO_REF_48_16_HP_DATA_ADDR        0x30E8
#define OMAP_ABE_S_ECHO_REF_48_16_HP_DATA_SIZE        0x28

#define OMAP_ABE_S_APS_IIRMEM1_ADDR                   0x3110
#define OMAP_ABE_S_APS_IIRMEM1_SIZE                   0x48

#define OMAP_ABE_S_APS_M_IIRMEM2_ADDR                 0x3158
#define OMAP_ABE_S_APS_M_IIRMEM2_SIZE                 0x18

#define OMAP_ABE_S_APS_C_IIRMEM2_ADDR                 0x3170
#define OMAP_ABE_S_APS_C_IIRMEM2_SIZE                 0x18

#define OMAP_ABE_S_APS_DL1_OUTSAMPLES_ADDR            0x3188
#define OMAP_ABE_S_APS_DL1_OUTSAMPLES_SIZE            0x10

#define OMAP_ABE_S_APS_DL1_COIL_OUTSAMPLES_ADDR       0x3198
#define OMAP_ABE_S_APS_DL1_COIL_OUTSAMPLES_SIZE       0x10

#define OMAP_ABE_S_APS_DL2_L_OUTSAMPLES_ADDR          0x31A8
#define OMAP_ABE_S_APS_DL2_L_OUTSAMPLES_SIZE          0x10

#define OMAP_ABE_S_APS_DL2_L_COIL_OUTSAMPLES_ADDR     0x31B8
#define OMAP_ABE_S_APS_DL2_L_COIL_OUTSAMPLES_SIZE     0x10

#define OMAP_ABE_S_APS_DL2_R_OUTSAMPLES_ADDR          0x31C8
#define OMAP_ABE_S_APS_DL2_R_OUTSAMPLES_SIZE          0x10

#define OMAP_ABE_S_APS_DL2_R_COIL_OUTSAMPLES_ADDR     0x31D8
#define OMAP_ABE_S_APS_DL2_R_COIL_OUTSAMPLES_SIZE     0x10

#define OMAP_ABE_S_XINASRC_ECHO_REF_ADDR              0x31E8
#define OMAP_ABE_S_XINASRC_ECHO_REF_SIZE              0x140

#define OMAP_ABE_S_ECHO_REF_16K_ADDR                  0x3328
#define OMAP_ABE_S_ECHO_REF_16K_SIZE                  0x28

#define OMAP_ABE_S_ECHO_REF_8K_ADDR                   0x3350
#define OMAP_ABE_S_ECHO_REF_8K_SIZE                   0x18

#define OMAP_ABE_S_DL1_EQ_ADDR                        0x3368
#define OMAP_ABE_S_DL1_EQ_SIZE                        0x60

#define OMAP_ABE_S_DL2_EQ_ADDR                        0x33C8
#define OMAP_ABE_S_DL2_EQ_SIZE                        0x60

#define OMAP_ABE_S_DL1_GAIN_OUT_ADDR                  0x3428
#define OMAP_ABE_S_DL1_GAIN_OUT_SIZE                  0x60

#define OMAP_ABE_S_DL2_GAIN_OUT_ADDR                  0x3488
#define OMAP_ABE_S_DL2_GAIN_OUT_SIZE                  0x60

#define OMAP_ABE_S_APS_DL2_L_IIRMEM1_ADDR             0x34E8
#define OMAP_ABE_S_APS_DL2_L_IIRMEM1_SIZE             0x48

#define OMAP_ABE_S_APS_DL2_R_IIRMEM1_ADDR             0x3530
#define OMAP_ABE_S_APS_DL2_R_IIRMEM1_SIZE             0x48

#define OMAP_ABE_S_APS_DL2_L_M_IIRMEM2_ADDR           0x3578
#define OMAP_ABE_S_APS_DL2_L_M_IIRMEM2_SIZE           0x18

#define OMAP_ABE_S_APS_DL2_R_M_IIRMEM2_ADDR           0x3590
#define OMAP_ABE_S_APS_DL2_R_M_IIRMEM2_SIZE           0x18

#define OMAP_ABE_S_APS_DL2_L_C_IIRMEM2_ADDR           0x35A8
#define OMAP_ABE_S_APS_DL2_L_C_IIRMEM2_SIZE           0x18

#define OMAP_ABE_S_APS_DL2_R_C_IIRMEM2_ADDR           0x35C0
#define OMAP_ABE_S_APS_DL2_R_C_IIRMEM2_SIZE           0x18

#define OMAP_ABE_S_DL1_APS_ADDR                       0x35D8
#define OMAP_ABE_S_DL1_APS_SIZE                       0x60

#define OMAP_ABE_S_DL2_L_APS_ADDR                     0x3638
#define OMAP_ABE_S_DL2_L_APS_SIZE                     0x60

#define OMAP_ABE_S_DL2_R_APS_ADDR                     0x3698
#define OMAP_ABE_S_DL2_R_APS_SIZE                     0x60

#define OMAP_ABE_S_APS_DL1_EQ_DATA_ADDR               0x36F8
#define OMAP_ABE_S_APS_DL1_EQ_DATA_SIZE               0x48

#define OMAP_ABE_S_APS_DL2_EQ_DATA_ADDR               0x3740
#define OMAP_ABE_S_APS_DL2_EQ_DATA_SIZE               0x48

#define OMAP_ABE_S_DC_DCVALUE_ADDR                    0x3788
#define OMAP_ABE_S_DC_DCVALUE_SIZE                    0x8

#define OMAP_ABE_S_VIBRA_ADDR                         0x3790
#define OMAP_ABE_S_VIBRA_SIZE                         0x30

#define OMAP_ABE_S_VIBRA2_IN_ADDR                     0x37C0
#define OMAP_ABE_S_VIBRA2_IN_SIZE                     0x30

#define OMAP_ABE_S_VIBRA2_ADDR_ADDR                   0x37F0
#define OMAP_ABE_S_VIBRA2_ADDR_SIZE                   0x8

#define OMAP_ABE_S_VIBRACTRL_FORRIGHTSM_ADDR          0x37F8
#define OMAP_ABE_S_VIBRACTRL_FORRIGHTSM_SIZE          0xC0

#define OMAP_ABE_S_RNOISE_MEM_ADDR                    0x38B8
#define OMAP_ABE_S_RNOISE_MEM_SIZE                    0x8

#define OMAP_ABE_S_CTRL_ADDR                          0x38C0
#define OMAP_ABE_S_CTRL_SIZE                          0x90

#define OMAP_ABE_S_VIBRA1_IN_ADDR                     0x3950
#define OMAP_ABE_S_VIBRA1_IN_SIZE                     0x30

#define OMAP_ABE_S_VIBRA1_TEMP_ADDR                   0x3980
#define OMAP_ABE_S_VIBRA1_TEMP_SIZE                   0xC0

#define OMAP_ABE_S_VIBRACTRL_FORLEFTSM_ADDR           0x3A40
#define OMAP_ABE_S_VIBRACTRL_FORLEFTSM_SIZE           0xC0

#define OMAP_ABE_S_VIBRA1_MEM_ADDR                    0x3B00
#define OMAP_ABE_S_VIBRA1_MEM_SIZE                    0x58

#define OMAP_ABE_S_VIBRACTRL_STEREO_ADDR              0x3B58
#define OMAP_ABE_S_VIBRACTRL_STEREO_SIZE              0xC0

#define OMAP_ABE_S_AMIC_96_48_DATA_ADDR               0x3C18
#define OMAP_ABE_S_AMIC_96_48_DATA_SIZE               0x98

#define OMAP_ABE_S_DMIC0_96_48_DATA_ADDR              0x3CB0
#define OMAP_ABE_S_DMIC0_96_48_DATA_SIZE              0x98

#define OMAP_ABE_S_DMIC1_96_48_DATA_ADDR              0x3D48
#define OMAP_ABE_S_DMIC1_96_48_DATA_SIZE              0x98

#define OMAP_ABE_S_DMIC2_96_48_DATA_ADDR              0x3DE0
#define OMAP_ABE_S_DMIC2_96_48_DATA_SIZE              0x98

#define OMAP_ABE_S_DBG_8K_PATTERN_ADDR                0x3E78
#define OMAP_ABE_S_DBG_8K_PATTERN_SIZE                0x10

#define OMAP_ABE_S_DBG_16K_PATTERN_ADDR               0x3E88
#define OMAP_ABE_S_DBG_16K_PATTERN_SIZE               0x20

#define OMAP_ABE_S_DBG_24K_PATTERN_ADDR               0x3EA8
#define OMAP_ABE_S_DBG_24K_PATTERN_SIZE               0x30

#define OMAP_ABE_S_DBG_48K_PATTERN_ADDR               0x3ED8
#define OMAP_ABE_S_DBG_48K_PATTERN_SIZE               0x60

#define OMAP_ABE_S_DBG_96K_PATTERN_ADDR               0x3F38
#define OMAP_ABE_S_DBG_96K_PATTERN_SIZE               0xC0

#define OMAP_ABE_S_MM_EXT_IN_ADDR                     0x3FF8
#define OMAP_ABE_S_MM_EXT_IN_SIZE                     0x60

#define OMAP_ABE_S_MM_EXT_IN_L_ADDR                   0x4058
#define OMAP_ABE_S_MM_EXT_IN_L_SIZE                   0x60

#define OMAP_ABE_S_MM_EXT_IN_R_ADDR                   0x40B8
#define OMAP_ABE_S_MM_EXT_IN_R_SIZE                   0x60

#define OMAP_ABE_S_MIC4_ADDR                          0x4118
#define OMAP_ABE_S_MIC4_SIZE                          0x60

#define OMAP_ABE_S_MIC4_L_ADDR                        0x4178
#define OMAP_ABE_S_MIC4_L_SIZE                        0x60

#define OMAP_ABE_S_MIC4_R_ADDR                        0x41D8
#define OMAP_ABE_S_MIC4_R_SIZE                        0x60

#define OMAP_ABE_S_HW_TEST_ADDR                       0x4238
#define OMAP_ABE_S_HW_TEST_SIZE                       0x8

#define OMAP_ABE_S_XINASRC_BT_UL_ADDR                 0x4240
#define OMAP_ABE_S_XINASRC_BT_UL_SIZE                 0x140

#define OMAP_ABE_S_XINASRC_BT_DL_ADDR                 0x4380
#define OMAP_ABE_S_XINASRC_BT_DL_SIZE                 0x140

#define OMAP_ABE_S_BT_DL_8K_TEMP_ADDR                 0x44C0
#define OMAP_ABE_S_BT_DL_8K_TEMP_SIZE                 0x10

#define OMAP_ABE_S_BT_DL_16K_TEMP_ADDR                0x44D0
#define OMAP_ABE_S_BT_DL_16K_TEMP_SIZE                0x20
