/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __LINUX_REGULATOR_MT6363_H
#define __LINUX_REGULATOR_MT6363_H

enum {
	MT6363_ID_VS2,
	MT6363_ID_VBUCK1,
	MT6363_ID_VBUCK2,
	MT6363_ID_VBUCK3,
	MT6363_ID_VBUCK4,
	MT6363_ID_VBUCK5,
	MT6363_ID_VBUCK6,
	MT6363_ID_VBUCK7,
	MT6363_ID_VS1,
	MT6363_ID_VS3,
	MT6363_ID_VSRAM_DIGRF,
	MT6363_ID_VSRAM_MDFE,
	MT6363_ID_VSRAM_MODEM,
	MT6363_ID_VSRAM_CPUB,
	MT6363_ID_VSRAM_CPUM,
	MT6363_ID_VSRAM_CPUL,
	MT6363_ID_VSRAM_APU,
	MT6363_ID_VEMC,
	MT6363_ID_VCN13,
	MT6363_ID_VTREF18,
	MT6363_ID_VAUX18,
	MT6363_ID_VCN15,
	MT6363_ID_VUFS18,
	MT6363_ID_VIO18,
	MT6363_ID_VM18,
	MT6363_ID_VA15,
	MT6363_ID_VRF18,
	MT6363_ID_VRFIO18,
	MT6363_ID_VIO075,
	MT6363_ID_VUFS12,
	MT6363_ID_VA12_1,
	MT6363_ID_VA12_2,
	MT6363_ID_VRF12,
	MT6363_ID_VRF13,
	MT6363_ID_VRF09,
	MT6363_ID_ISINK_LOAD,
	MT6363_MAX_REGULATOR,
};

#define MTK_REGULATOR_MAX_NR MT6363_MAX_REGULATOR

/* Register */
#define MT6363_TOP_TRAP                             0x36
#define MT6363_TOP_TMA_KEY_L                        0x39e
#define MT6363_BUCK_TOP_KEY_PROT_LO                 0x142a
#define MT6363_RG_BUCK_VS2_EN_ADDR                  0x240
#define MT6363_RG_BUCK_VS2_EN_SHIFT                 0
#define MT6363_RG_BUCK_VBUCK1_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK1_EN_SHIFT              1
#define MT6363_RG_BUCK_VBUCK2_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK2_EN_SHIFT              2
#define MT6363_RG_BUCK_VBUCK3_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK3_EN_SHIFT              3
#define MT6363_RG_BUCK_VBUCK4_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK4_EN_SHIFT              4
#define MT6363_RG_BUCK_VBUCK5_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK5_EN_SHIFT              5
#define MT6363_RG_BUCK_VBUCK6_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK6_EN_SHIFT              6
#define MT6363_RG_BUCK_VBUCK7_EN_ADDR               0x240
#define MT6363_RG_BUCK_VBUCK7_EN_SHIFT              7
#define MT6363_RG_BUCK_VS1_EN_ADDR                  0x243
#define MT6363_RG_BUCK_VS1_EN_SHIFT                 0
#define MT6363_RG_BUCK_VS3_EN_ADDR                  0x243
#define MT6363_RG_BUCK_VS3_EN_SHIFT                 1
#define MT6363_RG_LDO_VSRAM_DIGRF_EN_ADDR           0x243
#define MT6363_RG_LDO_VSRAM_DIGRF_EN_SHIFT          4
#define MT6363_RG_LDO_VSRAM_MDFE_EN_ADDR            0x243
#define MT6363_RG_LDO_VSRAM_MDFE_EN_SHIFT           5
#define MT6363_RG_LDO_VSRAM_MODEM_EN_ADDR           0x243
#define MT6363_RG_LDO_VSRAM_MODEM_EN_SHIFT          6
#define MT6363_RG_BUCK_VS2_LP_ADDR                  0x246
#define MT6363_RG_BUCK_VS2_LP_SHIFT                 0
#define MT6363_RG_BUCK_VBUCK1_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK1_LP_SHIFT              1
#define MT6363_RG_BUCK_VBUCK2_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK2_LP_SHIFT              2
#define MT6363_RG_BUCK_VBUCK3_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK3_LP_SHIFT              3
#define MT6363_RG_BUCK_VBUCK4_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK4_LP_SHIFT              4
#define MT6363_RG_BUCK_VBUCK5_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK5_LP_SHIFT              5
#define MT6363_RG_BUCK_VBUCK6_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK6_LP_SHIFT              6
#define MT6363_RG_BUCK_VBUCK7_LP_ADDR               0x246
#define MT6363_RG_BUCK_VBUCK7_LP_SHIFT              7
#define MT6363_RG_BUCK_VS1_LP_ADDR                  0x249
#define MT6363_RG_BUCK_VS1_LP_SHIFT                 0
#define MT6363_RG_BUCK_VS3_LP_ADDR                  0x249
#define MT6363_RG_BUCK_VS3_LP_SHIFT                 1
#define MT6363_RG_LDO_VSRAM_DIGRF_LP_ADDR           0x249
#define MT6363_RG_LDO_VSRAM_DIGRF_LP_SHIFT          4
#define MT6363_RG_LDO_VSRAM_MDFE_LP_ADDR            0x249
#define MT6363_RG_LDO_VSRAM_MDFE_LP_SHIFT           5
#define MT6363_RG_LDO_VSRAM_MODEM_LP_ADDR           0x249
#define MT6363_RG_LDO_VSRAM_MODEM_LP_SHIFT          6
#define MT6363_RG_BUCK_VS2_VOSEL_ADDR               0x24c
#define MT6363_RG_BUCK_VS2_VOSEL_MASK               0xFF
#define MT6363_RG_BUCK_VBUCK1_VOSEL_ADDR            0x24d
#define MT6363_RG_BUCK_VBUCK1_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK2_VOSEL_ADDR            0x24e
#define MT6363_RG_BUCK_VBUCK2_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK3_VOSEL_ADDR            0x24f
#define MT6363_RG_BUCK_VBUCK3_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK4_VOSEL_ADDR            0x250
#define MT6363_RG_BUCK_VBUCK4_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK5_VOSEL_ADDR            0x251
#define MT6363_RG_BUCK_VBUCK5_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK6_VOSEL_ADDR            0x252
#define MT6363_RG_BUCK_VBUCK6_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VBUCK7_VOSEL_ADDR            0x253
#define MT6363_RG_BUCK_VBUCK7_VOSEL_MASK            0xFF
#define MT6363_RG_BUCK_VS1_VOSEL_ADDR               0x254
#define MT6363_RG_BUCK_VS1_VOSEL_MASK               0xFF
#define MT6363_RG_BUCK_VS3_VOSEL_ADDR               0x255
#define MT6363_RG_BUCK_VS3_VOSEL_MASK               0xFF
#define MT6363_RG_LDO_VSRAM_DIGRF_VOSEL_ADDR        0x258
#define MT6363_RG_LDO_VSRAM_DIGRF_VOSEL_MASK        0x7F
#define MT6363_RG_LDO_VSRAM_MDFE_VOSEL_ADDR         0x259
#define MT6363_RG_LDO_VSRAM_MDFE_VOSEL_MASK         0x7F
#define MT6363_RG_LDO_VSRAM_MODEM_VOSEL_ADDR        0x25a
#define MT6363_RG_LDO_VSRAM_MODEM_VOSEL_MASK        0x7F
#define MT6363_RG_VS1_FCCM_ADDR                     0x1994
#define MT6363_RG_VS1_FCCM_SHIFT                    0
#define MT6363_RG_VS3_FCCM_ADDR                     0x19a3
#define MT6363_RG_VS3_FCCM_SHIFT                    0
#define MT6363_RG_VBUCK1_FCCM_ADDR                  0x1a32
#define MT6363_RG_VBUCK1_FCCM_SHIFT                 0
#define MT6363_RG_VBUCK2_FCCM_ADDR                  0x1a32
#define MT6363_RG_VBUCK2_FCCM_SHIFT                 1
#define MT6363_RG_VBUCK3_FCCM_ADDR                  0x1a32
#define MT6363_RG_VBUCK3_FCCM_SHIFT                 2
#define MT6363_RG_VS2_FCCM_ADDR                     0x1a32
#define MT6363_RG_VS2_FCCM_SHIFT                    3
#define MT6363_RG_VBUCK4_FCCM_ADDR                  0x1ab2
#define MT6363_RG_VBUCK4_FCCM_SHIFT                 0
#define MT6363_RG_VBUCK5_FCCM_ADDR                  0x1ab2
#define MT6363_RG_VBUCK5_FCCM_SHIFT                 1
#define MT6363_RG_VBUCK6_FCCM_ADDR                  0x1ab2
#define MT6363_RG_VBUCK6_FCCM_SHIFT                 2
#define MT6363_RG_VBUCK7_FCCM_ADDR                  0x1ab2
#define MT6363_RG_VBUCK7_FCCM_SHIFT                 3
#define MT6363_RG_VCN13_VOSEL_ADDR                  0x1b3f
#define MT6363_RG_VCN13_VOSEL_MASK                  0xF
#define MT6363_RG_VEMC_VOSEL_0_ADDR                 0x1b40
#define MT6363_RG_VEMC_VOSEL_0_MASK                 0xF
#define MT6363_RG_LDO_VSRAM_CPUB_VOSEL_ADDR         0x1b44
#define MT6363_RG_LDO_VSRAM_CPUB_VOSEL_MASK         0x7F
#define MT6363_RG_LDO_VSRAM_CPUM_VOSEL_ADDR         0x1b45
#define MT6363_RG_LDO_VSRAM_CPUM_VOSEL_MASK         0x7F
#define MT6363_RG_LDO_VSRAM_CPUL_VOSEL_ADDR         0x1b46
#define MT6363_RG_LDO_VSRAM_CPUL_VOSEL_MASK         0x7F
#define MT6363_RG_LDO_VSRAM_APU_VOSEL_ADDR          0x1b47
#define MT6363_RG_LDO_VSRAM_APU_VOSEL_MASK          0x7F
#define MT6363_RG_VEMC_VOCAL_0_ADDR                 0x1b4b
#define MT6363_RG_VEMC_VOCAL_0_MASK                 0xF
#define MT6363_RG_LDO_VCN15_EN_ADDR                 0x1b87
#define MT6363_RG_LDO_VCN15_EN_SHIFT                0
#define MT6363_RG_LDO_VCN15_LP_ADDR                 0x1b87
#define MT6363_RG_LDO_VCN15_LP_SHIFT                1
#define MT6363_RG_LDO_VRF09_EN_ADDR                 0x1b95
#define MT6363_RG_LDO_VRF09_EN_SHIFT                0
#define MT6363_RG_LDO_VRF09_LP_ADDR                 0x1b95
#define MT6363_RG_LDO_VRF09_LP_SHIFT                1
#define MT6363_RG_LDO_VRF12_EN_ADDR                 0x1ba3
#define MT6363_RG_LDO_VRF12_EN_SHIFT                0
#define MT6363_RG_LDO_VRF12_LP_ADDR                 0x1ba3
#define MT6363_RG_LDO_VRF12_LP_SHIFT                1
#define MT6363_RG_LDO_VRF13_EN_ADDR                 0x1bb1
#define MT6363_RG_LDO_VRF13_EN_SHIFT                0
#define MT6363_RG_LDO_VRF13_LP_ADDR                 0x1bb1
#define MT6363_RG_LDO_VRF13_LP_SHIFT                1
#define MT6363_RG_LDO_VRF18_EN_ADDR                 0x1bbf
#define MT6363_RG_LDO_VRF18_EN_SHIFT                0
#define MT6363_RG_LDO_VRF18_LP_ADDR                 0x1bbf
#define MT6363_RG_LDO_VRF18_LP_SHIFT                1
#define MT6363_RG_LDO_VRFIO18_EN_ADDR               0x1bcd
#define MT6363_RG_LDO_VRFIO18_EN_SHIFT              0
#define MT6363_RG_LDO_VRFIO18_LP_ADDR               0x1bcd
#define MT6363_RG_LDO_VRFIO18_LP_SHIFT              1
#define MT6363_RG_LDO_VTREF18_EN_ADDR               0x1c07
#define MT6363_RG_LDO_VTREF18_EN_SHIFT              0
#define MT6363_RG_LDO_VTREF18_LP_ADDR               0x1c07
#define MT6363_RG_LDO_VTREF18_LP_SHIFT              1
#define MT6363_RG_LDO_VAUX18_EN_ADDR                0x1c15
#define MT6363_RG_LDO_VAUX18_EN_SHIFT               0
#define MT6363_RG_LDO_VAUX18_LP_ADDR                0x1c15
#define MT6363_RG_LDO_VAUX18_LP_SHIFT               1
#define MT6363_RG_LDO_VEMC_EN_ADDR                  0x1c23
#define MT6363_RG_LDO_VEMC_EN_SHIFT                 0
#define MT6363_RG_LDO_VEMC_LP_ADDR                  0x1c23
#define MT6363_RG_LDO_VEMC_LP_SHIFT                 1
#define MT6363_RG_LDO_VUFS12_EN_ADDR                0x1c31
#define MT6363_RG_LDO_VUFS12_EN_SHIFT               0
#define MT6363_RG_LDO_VUFS12_LP_ADDR                0x1c31
#define MT6363_RG_LDO_VUFS12_LP_SHIFT               1
#define MT6363_RG_LDO_VUFS18_EN_ADDR                0x1c3f
#define MT6363_RG_LDO_VUFS18_EN_SHIFT               0
#define MT6363_RG_LDO_VUFS18_LP_ADDR                0x1c3f
#define MT6363_RG_LDO_VUFS18_LP_SHIFT               1
#define MT6363_RG_LDO_VIO18_EN_ADDR                 0x1c4d
#define MT6363_RG_LDO_VIO18_EN_SHIFT                0
#define MT6363_RG_LDO_VIO18_LP_ADDR                 0x1c4d
#define MT6363_RG_LDO_VIO18_LP_SHIFT                1
#define MT6363_RG_LDO_VIO075_EN_ADDR                0x1c87
#define MT6363_RG_LDO_VIO075_EN_SHIFT               0
#define MT6363_RG_LDO_VIO075_LP_ADDR                0x1c87
#define MT6363_RG_LDO_VIO075_LP_SHIFT               1
#define MT6363_RG_LDO_VA12_1_EN_ADDR                0x1c95
#define MT6363_RG_LDO_VA12_1_EN_SHIFT               0
#define MT6363_RG_LDO_VA12_1_LP_ADDR                0x1c95
#define MT6363_RG_LDO_VA12_1_LP_SHIFT               1
#define MT6363_RG_LDO_VA12_2_EN_ADDR                0x1ca3
#define MT6363_RG_LDO_VA12_2_EN_SHIFT               0
#define MT6363_RG_LDO_VA12_2_LP_ADDR                0x1ca3
#define MT6363_RG_LDO_VA12_2_LP_SHIFT               1
#define MT6363_RG_LDO_VA15_EN_ADDR                  0x1cb1
#define MT6363_RG_LDO_VA15_EN_SHIFT                 0
#define MT6363_RG_LDO_VA15_LP_ADDR                  0x1cb1
#define MT6363_RG_LDO_VA15_LP_SHIFT                 1
#define MT6363_RG_LDO_VM18_EN_ADDR                  0x1cbf
#define MT6363_RG_LDO_VM18_EN_SHIFT                 0
#define MT6363_RG_LDO_VM18_LP_ADDR                  0x1cbf
#define MT6363_RG_LDO_VM18_LP_SHIFT                 1
#define MT6363_RG_LDO_VCN13_EN_ADDR                 0x1d07
#define MT6363_RG_LDO_VCN13_EN_SHIFT                0
#define MT6363_RG_LDO_VCN13_LP_ADDR                 0x1d07
#define MT6363_RG_LDO_VCN13_LP_SHIFT                1
#define MT6363_RG_LDO_VSRAM_CPUB_EN_ADDR            0x1e07
#define MT6363_RG_LDO_VSRAM_CPUB_EN_SHIFT           0
#define MT6363_RG_LDO_VSRAM_CPUB_LP_ADDR            0x1e07
#define MT6363_RG_LDO_VSRAM_CPUB_LP_SHIFT           1
#define MT6363_RG_LDO_VSRAM_CPUM_EN_ADDR            0x1e1d
#define MT6363_RG_LDO_VSRAM_CPUM_EN_SHIFT           0
#define MT6363_RG_LDO_VSRAM_CPUM_LP_ADDR            0x1e1d
#define MT6363_RG_LDO_VSRAM_CPUM_LP_SHIFT           1
#define MT6363_RG_LDO_VSRAM_CPUL_EN_ADDR            0x1e87
#define MT6363_RG_LDO_VSRAM_CPUL_EN_SHIFT           0
#define MT6363_RG_LDO_VSRAM_CPUL_LP_ADDR            0x1e87
#define MT6363_RG_LDO_VSRAM_CPUL_LP_SHIFT           1
#define MT6363_RG_LDO_VSRAM_APU_EN_ADDR             0x1e9d
#define MT6363_RG_LDO_VSRAM_APU_EN_SHIFT            0
#define MT6363_RG_LDO_VSRAM_APU_LP_ADDR             0x1e9d
#define MT6363_RG_LDO_VSRAM_APU_LP_SHIFT            1
#define MT6363_RG_VTREF18_VOCAL_ADDR                0x1f08
#define MT6363_RG_VTREF18_VOCAL_MASK                0xF
#define MT6363_RG_VTREF18_VOSEL_ADDR                0x1f09
#define MT6363_RG_VTREF18_VOSEL_MASK                0xF
#define MT6363_RG_VAUX18_VOCAL_ADDR                 0x1f0c
#define MT6363_RG_VAUX18_VOCAL_MASK                 0xF
#define MT6363_RG_VAUX18_VOSEL_ADDR                 0x1f0d
#define MT6363_RG_VAUX18_VOSEL_MASK                 0xF
#define MT6363_RG_VCN15_VOCAL_ADDR                  0x1f13
#define MT6363_RG_VCN15_VOCAL_MASK                  0xF
#define MT6363_RG_VCN15_VOSEL_ADDR                  0x1f14
#define MT6363_RG_VCN15_VOSEL_MASK                  0xF
#define MT6363_RG_VUFS18_VOCAL_ADDR                 0x1f17
#define MT6363_RG_VUFS18_VOCAL_MASK                 0xF
#define MT6363_RG_VUFS18_VOSEL_ADDR                 0x1f18
#define MT6363_RG_VUFS18_VOSEL_MASK                 0xF
#define MT6363_RG_VIO18_VOCAL_ADDR                  0x1f1b
#define MT6363_RG_VIO18_VOCAL_MASK                  0xF
#define MT6363_RG_VIO18_VOSEL_ADDR                  0x1f1c
#define MT6363_RG_VIO18_VOSEL_MASK                  0xF
#define MT6363_RG_VM18_VOCAL_ADDR                   0x1f1f
#define MT6363_RG_VM18_VOCAL_MASK                   0xF
#define MT6363_RG_VM18_VOSEL_ADDR                   0x1f20
#define MT6363_RG_VM18_VOSEL_MASK                   0xF
#define MT6363_RG_VA15_VOCAL_ADDR                   0x1f23
#define MT6363_RG_VA15_VOCAL_MASK                   0xF
#define MT6363_RG_VA15_VOSEL_ADDR                   0x1f24
#define MT6363_RG_VA15_VOSEL_MASK                   0xF
#define MT6363_RG_VRF18_VOCAL_ADDR                  0x1f27
#define MT6363_RG_VRF18_VOCAL_MASK                  0xF
#define MT6363_RG_VRF18_VOSEL_ADDR                  0x1f28
#define MT6363_RG_VRF18_VOSEL_MASK                  0xF
#define MT6363_RG_VRFIO18_VOCAL_ADDR                0x1f2b
#define MT6363_RG_VRFIO18_VOCAL_MASK                0xF
#define MT6363_RG_VRFIO18_VOSEL_ADDR                0x1f2c
#define MT6363_RG_VRFIO18_VOSEL_MASK                0xF
#define MT6363_RG_VIO075_VOCAL_ADDR                 0x1f31
#define MT6363_RG_VIO075_VOCAL_MASK                 0xF
#define MT6363_RG_VIO075_VOSEL_ADDR                 0x1f31
#define MT6363_RG_VIO075_VOSEL_MASK                 0x7
#define MT6363_RG_VCN13_VOCAL_ADDR                  0x1f88
#define MT6363_RG_VCN13_VOCAL_MASK                  0xF
#define MT6363_RG_VUFS12_VOCAL_ADDR                 0x1f91
#define MT6363_RG_VUFS12_VOCAL_MASK                 0xF
#define MT6363_RG_VUFS12_VOSEL_ADDR                 0x1f92
#define MT6363_RG_VUFS12_VOSEL_MASK                 0xF
#define MT6363_RG_VA12_1_VOCAL_ADDR                 0x1f95
#define MT6363_RG_VA12_1_VOCAL_MASK                 0xF
#define MT6363_RG_VA12_1_VOSEL_ADDR                 0x1f96
#define MT6363_RG_VA12_1_VOSEL_MASK                 0xF
#define MT6363_RG_VA12_2_VOCAL_ADDR                 0x1f99
#define MT6363_RG_VA12_2_VOCAL_MASK                 0xF
#define MT6363_RG_VA12_2_VOSEL_ADDR                 0x1f9a
#define MT6363_RG_VA12_2_VOSEL_MASK                 0xF
#define MT6363_RG_VRF12_VOCAL_ADDR                  0x1f9d
#define MT6363_RG_VRF12_VOCAL_MASK                  0xF
#define MT6363_RG_VRF12_VOSEL_ADDR                  0x1f9e
#define MT6363_RG_VRF12_VOSEL_MASK                  0xF
#define MT6363_RG_VRF13_VOCAL_ADDR                  0x1fa1
#define MT6363_RG_VRF13_VOCAL_MASK                  0xF
#define MT6363_RG_VRF13_VOSEL_ADDR                  0x1fa2
#define MT6363_RG_VRF13_VOSEL_MASK                  0xF
#define MT6363_RG_VRF09_VOCAL_ADDR                  0x1fa8
#define MT6363_RG_VRF09_VOCAL_MASK                  0xF
#define MT6363_RG_VRF09_VOSEL_ADDR                  0x1fa9
#define MT6363_RG_VRF09_VOSEL_MASK                  0xF
#define MT6363_ISINK_EN_CTRL0                       0x220b
#define MT6363_ISINK_EN_CTRL1                       0x220c


#endif /* __LINUX_REGULATOR_MT6363_H */
