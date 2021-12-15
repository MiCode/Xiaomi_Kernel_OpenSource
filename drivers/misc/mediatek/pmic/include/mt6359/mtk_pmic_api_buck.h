/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_PMIC_API_BUCK_H_
#define _MT_PMIC_API_BUCK_H_

void vmd1_pmic_setting_on(void);
void vmd1_pmic_setting_off(void);

#ifdef LP_GOLDEN_SETTING
#define LGS
#endif

#ifdef LP_GOLDEN_SETTING_W_SPM
#define LGSWS
#endif

#if defined(LGS) || defined(LGSWS)
#define PMIC_LP_BUCK_ENTRY(reg) {reg, MT6359_BUCK_##reg##_CON0}
#define PMIC_LP_LDO_ENTRY(reg) {reg, MT6359_LDO_##reg##_CON0}
#define PMIC_LP_LDO_VCN33_0_ENTRY(reg) {reg, MT6359_LDO_VCN33_CON0_0}
#define PMIC_LP_LDO_VCN33_1_ENTRY(reg) {reg, MT6359_LDO_VCN33_CON0_1}
#define PMIC_LP_LDO_VLDO28_0_ENTRY(reg) {reg, MT6359_LDO_VLDO28_CON0_0}
#define PMIC_LP_LDO_VLDO28_1_ENTRY(reg) {reg, MT6359_LDO_VLDO28_CON0_1}
#define PMIC_LP_LDO_VUSB_0_ENTRY(reg) {reg, MT6359_LDO_VUSB_CON0_0}
#define PMIC_LP_LDO_VUSB_1_ENTRY(reg) {reg, MT6359_LDO_VUSB_CON0_1}
#endif

enum BUCK_LDO_EN_USER {
	SRCLKEN0,
	SRCLKEN1,
	SRCLKEN2,
	SRCLKEN3,
	SRCLKEN4,
	SRCLKEN5,
	SRCLKEN6,
	SRCLKEN7,
	SRCLKEN8,
	SRCLKEN9,
	SRCLKEN10,
	SRCLKEN11,
	SRCLKEN12,
	SRCLKEN13,
	SRCLKEN14,
	SW,
	SPM = SW,
};

#define HW_OFF		0
#define HW_LP		1
#define HW_ULP		2
#define SW_OFF		0
#define SW_ON		1
#define SW_LP		3
#define SPM_OFF		0
#define SPM_ON		1
#define SPM_LP		3

enum PMU_LP_TABLE_ENUM {
	VCORE,
	VPU,
	VPROC1,
	VPROC2,
	VGPU11,
	VGPU12,
	VMODEM,
	VS1,
	VS2,
	VPA,
	VSRAM_PROC1,
	VSRAM_PROC2,
	VSRAM_OTHERS,
	VSRAM_MD,
	VCAMIO,
	VM18,
	VCN18,
	VCN13,
	VRF18,
	VIO18,
	VEFUSE,
	VRF12,
	VRFCK,
	VA12,
	VA09,
	VBBCK,
	VFE28,
	VBIF28,
	VAUD18,
	VAUX18,
	VXO22,
	VCN33_1_0,
	VCN33_1_1,
	VCN33_2_0,
	VCN33_2_1,
	VUSB_0,
	VUSB_1,
	VEMC,
	VIO28,
	VSIM1,
	VSIM2,
	VUFS,
	VIBR,
	TABLE_COUNT_END
};

struct PMU_LP_TABLE_ENTRY {
	enum PMU_LP_TABLE_ENUM flagname;
#if defined(LGS) || defined(LGSWS)
	unsigned short en_adr;
#endif
};

extern int pmic_buck_vcore_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vpu_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vproc1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vproc2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vgpu11_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vgpu12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vmodem_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vs1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vs2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_buck_vpa_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_proc1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_proc2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_others_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsram_md_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcamio_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vm18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcn18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcn13_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrf18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vio18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vefuse_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrf12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vrfck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_va12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_va09_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vbbck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vfe28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vbif28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vaud18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vaux18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vxo22_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcn33_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vcn33_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vusb_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vemc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vio28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsim1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vsim2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vufs_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
extern int pmic_ldo_vibr_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg);
#endif
