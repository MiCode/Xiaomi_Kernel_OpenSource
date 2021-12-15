/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_PMIC_API_BUCK_H_
#define _MT_PMIC_API_BUCK_H_

#include <mach/upmu_hw.h>

void vmd1_pmic_setting_on(void);
void vmd1_pmic_setting_off(void);

#ifdef LP_GOLDEN_SETTING
#define LGS
#endif

#ifdef LP_GOLDEN_SETTING_W_SPM
#define LGSWS
#endif

#if defined(LGS) || defined(LGSWS)
#define PMIC_LP_BUCK_ENTRY(reg) {reg, MT6358_BUCK_##reg##_CON0}
#define PMIC_LP_LDO_ENTRY(reg) {reg, MT6358_LDO_##reg##_CON0}
#define PMIC_LP_LDO_VCN33_0_ENTRY(reg) {reg, MT6358_LDO_VCN33_CON0_0}
#define PMIC_LP_LDO_VCN33_1_ENTRY(reg) {reg, MT6358_LDO_VCN33_CON0_1}
#define PMIC_LP_LDO_VLDO28_0_ENTRY(reg) {reg, MT6358_LDO_VLDO28_CON0_0}
#define PMIC_LP_LDO_VLDO28_1_ENTRY(reg) {reg, MT6358_LDO_VLDO28_CON0_1}
#define PMIC_LP_LDO_VUSB_0_ENTRY(reg) {reg, MT6358_LDO_VUSB_CON0_0}
#define PMIC_LP_LDO_VUSB_1_ENTRY(reg) {reg, MT6358_LDO_VUSB_CON0_1}
#endif

enum BUCK_LDO_EN_USER {
	SW,
	SPM = SW,
	SRCLKEN0,
	SRCLKEN1,
	SRCLKEN2,
	SRCLKEN3,
};

#define HW_OFF	0
#define HW_LP	1
#define SW_OFF	0
#define SW_ON	1
#define SW_LP	3
#define SPM_OFF	0
#define SPM_ON	1
#define SPM_LP	3

enum PMU_LP_TABLE_ENUM {
	VPROC11,
	VCORE,
	VGPU,
	VMODEM,
	VS1,
	VS2,
	VPA,
	VDRAM1,
	VPROC12,
	VSRAM_GPU,
	VSRAM_OTHERS,
	VSRAM_PROC11,
	VXO22,
	VRF18,
	VRF12,
	VEFUSE,
	VCN33_0,
	VCN33_1,
	VCN28,
	VCN18,
	VCAMA1,
	VCAMD,
	VCAMA2,
	VSRAM_PROC12,
	VCAMIO,
	VLDO28_0,
	VLDO28_1,
	VA12,
	VAUX18,
	VAUD28,
	VIO28,
	VIO18,
	VFE28,
	VDRAM2,
	VMC,
	VMCH,
	VEMC,
	VSIM1,
	VSIM2,
	VIBR,
	VUSB_0,
	VUSB_1,
	VBIF28,
#if defined(USE_PMIC_MT6366) && USE_PMIC_MT6366
	VM18,
	VMDDR,
	VSRAM_CORE,
#endif
	TABLE_COUNT_END
};

struct PMU_LP_TABLE_ENTRY {
	enum PMU_LP_TABLE_ENUM flagname;
#if defined(LGS) || defined(LGSWS)
	unsigned short en_adr;
#endif
};

extern int pmic_buck_vproc11_lp(enum BUCK_LDO_EN_USER user,
				unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vcore_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vgpu_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vmodem_lp(enum BUCK_LDO_EN_USER user,
			       unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vs1_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vs2_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vpa_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vdram1_lp(enum BUCK_LDO_EN_USER user,
			       unsigned char op_en, unsigned char op_cfg);
extern int pmic_buck_vproc12_lp(enum BUCK_LDO_EN_USER user,
				unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsram_gpu_lp(enum BUCK_LDO_EN_USER user,
				 unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsram_others_lp(enum BUCK_LDO_EN_USER user,
				    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsram_proc11_lp(enum BUCK_LDO_EN_USER user,
				    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vxo22_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vrf18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vrf12_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vefuse_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcn33_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcn28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcn18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcama1_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcamd_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcama2_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsram_proc12_lp(enum BUCK_LDO_EN_USER user,
				    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vcamio_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vldo28_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_va12_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vaux18_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vaud28_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vio28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vio18_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vfe28_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vdram2_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vmc_lp(enum BUCK_LDO_EN_USER user,
			   unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vmch_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vemc_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsim1_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsim2_lp(enum BUCK_LDO_EN_USER user,
			     unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vibr_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vusb_lp(enum BUCK_LDO_EN_USER user,
			    unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vbif28_lp(enum BUCK_LDO_EN_USER user,
			      unsigned char op_en, unsigned char op_cfg);
#if defined(USE_PMIC_MT6366) && USE_PMIC_MT6366
extern int pmic_ldo_vm18_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vmddr_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en, unsigned char op_cfg);
extern int pmic_ldo_vsram_core_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en,
				  unsigned char op_cfg);
#endif
#endif
