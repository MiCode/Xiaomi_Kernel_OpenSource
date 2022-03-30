/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __LINUX_REGULATOR_MT6369_H
#define __LINUX_REGULATOR_MT6369_H

enum {
	MT6369_ID_VBUCK1,
	MT6369_ID_VPA,
	MT6369_ID_VSRAM_CORE,
	MT6369_ID_VDIGRF,
	MT6369_ID_VAUX18,
	MT6369_ID_VUSB,
	MT6369_ID_VIBR,
	MT6369_ID_VIO28,
	MT6369_ID_VFP,
	MT6369_ID_VTP,
	MT6369_ID_VMCH,
	MT6369_ID_VMC,
	MT6369_ID_VCN33_1,
	MT6369_ID_VCN33_2,
	MT6369_ID_VAUD28,
	MT6369_ID_VANT18,
	MT6369_ID_VEFUSE,
	MT6369_ID_VMCH_EINT_HIGH,
	MT6369_ID_VMCH_EINT_LOW,
	MT6369_MAX_REGULATOR,
};

/* Register */
#define MT6369_TOP_CFG_ELR5			    0x14b
#define MT6369_TOP_TMA_KEY_L                        0x39e
#define MT6369_BUCK_TOP_KEY_PROT_LO                 0x1421
#define MT6369_RG_BUCK_VBUCK1_EN_ADDR               0x240
#define MT6369_RG_BUCK_VBUCK1_EN_SHIFT              1
#define MT6369_RG_BUCK_VPA_EN_ADDR                  0x243
#define MT6369_RG_BUCK_VPA_EN_SHIFT                 3
#define MT6369_RG_BUCK_VBUCK1_LP_ADDR               0x246
#define MT6369_RG_BUCK_VBUCK1_LP_SHIFT              1
#define MT6369_RG_BUCK_VPA_LP_ADDR                  0x249
#define MT6369_RG_BUCK_VPA_LP_SHIFT                 3
#define MT6369_RG_BUCK_VBUCK1_VOSEL_ADDR            0x24d
#define MT6369_RG_BUCK_VBUCK1_VOSEL_MASK            0xff
#define MT6369_RG_BUCK_VPA_VOSEL_ADDR               0x257
#define MT6369_RG_BUCK_VPA_VOSEL_MASK               0x7f
#define MT6369_RG_VBUCK1_FCCM_ADDR                  0x1594
#define MT6369_RG_VBUCK1_FCCM_SHIFT                 0
#define MT6369_RG_VPA_MODESET_ADDR                  0x1608
#define MT6369_RG_VPA_MODESET_SHIFT	            1
#define MT6369_RG_VMDDR_VOSEL_0_ADDR                0x1b37
#define MT6369_RG_VMDDR_VOSEL_0_MASK                0xf
#define MT6369_RG_VMDDR_VOCAL_0_ADDR                0x1b37
#define MT6369_RG_VMDDR_VOCAL_0_MASK                0xf0
#define MT6369_RG_VMDDQ_VOSEL_0_ADDR                0x1b3a
#define MT6369_RG_VMDDQ_VOSEL_0_MASK                0xf
#define MT6369_RG_VMDDQ_VOCAL_0_ADDR                0x1b3a
#define MT6369_RG_VMDDQ_VOCAL_0_MASK                0xf0
#define MT6369_RG_LDO_VSRAM_CORE_VOSEL_ADDR         0x1b3e
#define MT6369_RG_LDO_VSRAM_CORE_VOSEL_MASK         0x7f
#define MT6369_RG_LDO_VDIGRF_VOSEL_ADDR             0x1b3f
#define MT6369_RG_LDO_VDIGRF_VOSEL_MASK             0x7f
#define MT6369_RG_LDO_VMDDR_EN_ADDR                 0x1b87
#define MT6369_RG_LDO_VMDDR_EN_SHIFT                0
#define MT6369_RG_LDO_VMDDR_LP_ADDR                 0x1b87
#define MT6369_RG_LDO_VMDDR_LP_SHIFT                1
#define MT6369_RG_LDO_VMDDQ_EN_ADDR                 0x1b95
#define MT6369_RG_LDO_VMDDQ_EN_SHIFT                0
#define MT6369_RG_LDO_VMDDQ_LP_ADDR                 0x1b95
#define MT6369_RG_LDO_VMDDQ_LP_SHIFT                1
#define MT6369_RG_LDO_VSIM1_EN_ADDR                 0x1ba3
#define MT6369_RG_LDO_VSIM1_EN_SHIFT                0
#define MT6369_RG_LDO_VSIM1_LP_ADDR                 0x1ba3
#define MT6369_RG_LDO_VSIM1_LP_SHIFT                1
#define MT6369_RG_LDO_VSIM2_EN_ADDR                 0x1bb2
#define MT6369_RG_LDO_VSIM2_EN_SHIFT                0
#define MT6369_RG_LDO_VSIM2_LP_ADDR                 0x1bb2
#define MT6369_RG_LDO_VSIM2_LP_SHIFT                1
#define MT6369_RG_LDO_VIBR_EN_ADDR                  0x1bc1
#define MT6369_RG_LDO_VIBR_EN_SHIFT                 0
#define MT6369_RG_LDO_VIBR_LP_ADDR                  0x1bc1
#define MT6369_RG_LDO_VIBR_LP_SHIFT                 1
#define MT6369_RG_LDO_VIO28_EN_ADDR                 0x1bcf
#define MT6369_RG_LDO_VIO28_EN_SHIFT                0
#define MT6369_RG_LDO_VIO28_LP_ADDR                 0x1bcf
#define MT6369_RG_LDO_VIO28_LP_SHIFT                1
#define MT6369_RG_LDO_VFP_EN_ADDR                   0x1c07
#define MT6369_RG_LDO_VFP_EN_SHIFT                  0
#define MT6369_RG_LDO_VFP_LP_ADDR                   0x1c07
#define MT6369_RG_LDO_VFP_LP_SHIFT                  1
#define MT6369_RG_LDO_VTP_EN_ADDR                   0x1c15
#define MT6369_RG_LDO_VTP_EN_SHIFT                  0
#define MT6369_RG_LDO_VTP_LP_ADDR                   0x1c15
#define MT6369_RG_LDO_VTP_LP_SHIFT                  1
#define MT6369_RG_LDO_VUSB_EN_ADDR                  0x1c23
#define MT6369_RG_LDO_VUSB_EN_SHIFT                 0
#define MT6369_RG_LDO_VUSB_LP_ADDR                  0x1c23
#define MT6369_RG_LDO_VUSB_LP_SHIFT                 1
#define MT6369_RG_LDO_VAUD28_EN_ADDR                0x1c31
#define MT6369_RG_LDO_VAUD28_EN_SHIFT               0
#define MT6369_RG_LDO_VAUD28_LP_ADDR                0x1c31
#define MT6369_RG_LDO_VAUD28_LP_SHIFT               1
#define MT6369_RG_LDO_VCN33_1_EN_ADDR               0x1c3f
#define MT6369_RG_LDO_VCN33_1_EN_SHIFT              0
#define MT6369_RG_LDO_VCN33_1_LP_ADDR               0x1c3f
#define MT6369_RG_LDO_VCN33_1_LP_SHIFT              1
#define MT6369_RG_LDO_VCN33_2_EN_ADDR               0x1c4d
#define MT6369_RG_LDO_VCN33_2_EN_SHIFT              0
#define MT6369_RG_LDO_VCN33_2_LP_ADDR               0x1c4d
#define MT6369_RG_LDO_VCN33_2_LP_SHIFT              1
#define MT6369_RG_LDO_VEFUSE_EN_ADDR                0x1c87
#define MT6369_RG_LDO_VEFUSE_EN_SHIFT               0
#define MT6369_RG_LDO_VEFUSE_LP_ADDR                0x1c87
#define MT6369_RG_LDO_VEFUSE_LP_SHIFT               1
#define MT6369_RG_LDO_VMCH_EN_ADDR                  0x1c95
#define MT6369_RG_LDO_VMCH_EN_SHIFT                 0
#define MT6369_RG_LDO_VMCH_LP_ADDR                  0x1c95
#define MT6369_RG_LDO_VMCH_LP_SHIFT                 1
#define MT6369_RG_LDO_VMC_EN_ADDR                   0x1ca4
#define MT6369_RG_LDO_VMC_EN_SHIFT                  0
#define MT6369_RG_LDO_VMC_LP_ADDR                   0x1ca4
#define MT6369_RG_LDO_VMC_LP_SHIFT                  1
#define MT6369_RG_LDO_VANT18_EN_ADDR                0x1cb2
#define MT6369_RG_LDO_VANT18_EN_SHIFT               0
#define MT6369_RG_LDO_VANT18_LP_ADDR                0x1cb2
#define MT6369_RG_LDO_VANT18_LP_SHIFT               1
#define MT6369_RG_LDO_VAUX18_EN_ADDR                0x1cc0
#define MT6369_RG_LDO_VAUX18_EN_SHIFT               0
#define MT6369_RG_LDO_VAUX18_LP_ADDR                0x1cc0
#define MT6369_RG_LDO_VAUX18_LP_SHIFT               1
#define MT6369_RG_LDO_VSRAM_CORE_EN_ADDR            0x1d07
#define MT6369_RG_LDO_VSRAM_CORE_EN_SHIFT           0
#define MT6369_RG_LDO_VSRAM_CORE_LP_ADDR            0x1d07
#define MT6369_RG_LDO_VSRAM_CORE_LP_SHIFT           1
#define MT6369_RG_LDO_VDIGRF_EN_ADDR                0x1d21
#define MT6369_RG_LDO_VDIGRF_EN_SHIFT               0
#define MT6369_RG_LDO_VDIGRF_LP_ADDR                0x1d21
#define MT6369_RG_LDO_VDIGRF_LP_SHIFT               1
#define MT6369_RG_VAUX18_VOCAL_ADDR                 0x1d88
#define MT6369_RG_VAUX18_VOCAL_MASK                 0xf
#define MT6369_RG_VAUX18_VOSEL_ADDR                 0x1d89
#define MT6369_RG_VAUX18_VOSEL_MASK                 0xf
#define MT6369_RG_VSIM1_VOCAL_ADDR                  0x1d8c
#define MT6369_RG_VSIM1_VOCAL_MASK                  0xf
#define MT6369_RG_VSIM1_VOSEL_ADDR                  0x1d8d
#define MT6369_RG_VSIM1_VOSEL_MASK                  0xf
#define MT6369_RG_VSIM2_VOCAL_ADDR                  0x1d90
#define MT6369_RG_VSIM2_VOCAL_MASK                  0xf
#define MT6369_RG_VSIM2_VOSEL_ADDR                  0x1d91
#define MT6369_RG_VSIM2_VOSEL_MASK                  0xf
#define MT6369_RG_VUSB_VOCAL_ADDR                   0x1d94
#define MT6369_RG_VUSB_VOCAL_MASK                   0xf
#define MT6369_RG_VUSB_VOSEL_ADDR                   0x1d95
#define MT6369_RG_VUSB_VOSEL_MASK                   0xf
#define MT6369_RG_VIBR_VOCAL_ADDR                   0x1d98
#define MT6369_RG_VIBR_VOCAL_MASK                   0xf
#define MT6369_RG_VIBR_VOSEL_ADDR                   0x1d99
#define MT6369_RG_VIBR_VOSEL_MASK                   0xf
#define MT6369_RG_VIO28_VOCAL_ADDR                  0x1d9c
#define MT6369_RG_VIO28_VOCAL_MASK                  0xf
#define MT6369_RG_VIO28_VOSEL_ADDR                  0x1d9d
#define MT6369_RG_VIO28_VOSEL_MASK                  0xf
#define MT6369_RG_VFP_VOCAL_ADDR                    0x1da0
#define MT6369_RG_VFP_VOCAL_MASK                    0xf
#define MT6369_RG_VFP_VOSEL_ADDR                    0x1da1
#define MT6369_RG_VFP_VOSEL_MASK                    0xf
#define MT6369_RG_VTP_VOCAL_ADDR                    0x1da4
#define MT6369_RG_VTP_VOCAL_MASK                    0xf
#define MT6369_RG_VTP_VOSEL_ADDR                    0x1da5
#define MT6369_RG_VTP_VOSEL_MASK                    0xf
#define MT6369_RG_VMCH_VOCAL_ADDR                   0x1da8
#define MT6369_RG_VMCH_VOCAL_MASK                   0xf
#define MT6369_RG_VMCH_VOSEL_ADDR                   0x1da9
#define MT6369_RG_VMCH_VOSEL_MASK                   0xf
#define MT6369_RG_VMC_VOCAL_ADDR                    0x1dac
#define MT6369_RG_VMC_VOCAL_MASK                    0xf
#define MT6369_RG_VMC_VOSEL_ADDR                    0x1dad
#define MT6369_RG_VMC_VOSEL_MASK                    0xf
#define MT6369_RG_VCN33_1_VOCAL_ADDR                0x1db0
#define MT6369_RG_VCN33_1_VOCAL_MASK                0xf
#define MT6369_RG_VCN33_1_VOSEL_ADDR                0x1db1
#define MT6369_RG_VCN33_1_VOSEL_MASK                0xf
#define MT6369_RG_VCN33_2_VOCAL_ADDR                0x1db4
#define MT6369_RG_VCN33_2_VOCAL_MASK                0xf
#define MT6369_RG_VCN33_2_VOSEL_ADDR                0x1db5
#define MT6369_RG_VCN33_2_VOSEL_MASK                0xf
#define MT6369_RG_VAUD28_VOCAL_ADDR                 0x1db8
#define MT6369_RG_VAUD28_VOCAL_MASK                 0xf
#define MT6369_RG_VAUD28_VOSEL_ADDR                 0x1db9
#define MT6369_RG_VAUD28_VOSEL_MASK                 0xf
#define MT6369_RG_VANT18_VOCAL_ADDR                 0x1e08
#define MT6369_RG_VANT18_VOCAL_MASK                 0xf
#define MT6369_RG_VANT18_VOSEL_ADDR                 0x1e09
#define MT6369_RG_VANT18_VOSEL_MASK                 0xf
#define MT6369_RG_VEFUSE_VOCAL_ADDR                 0x1e0c
#define MT6369_RG_VEFUSE_VOCAL_MASK                 0xf
#define MT6369_RG_VEFUSE_VOSEL_ADDR                 0x1e0d
#define MT6369_RG_VEFUSE_VOSEL_MASK                 0xf
#define MT6369_LDO_VMCH_EINT                        0x1ca3
#define MT6369_RG_LDO_VMCH_EINT_EN_MASK             0x1
#define MT6369_RG_LDO_VMCH_EINT_POL_MASK            0x4
#define MT6369_RG_LDO_VMCH_EINT_DB_MASK             0x10

#endif /* __LINUX_REGULATOR_MT6369_H */
