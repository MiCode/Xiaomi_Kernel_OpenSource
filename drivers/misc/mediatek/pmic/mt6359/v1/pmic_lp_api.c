/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"
#include "include/pmic_api_buck.h"

/* ---------------------------------------------------------------  */
/* pmic_<type>_<name>_lp (<user>, <op_en>, <op_cfg>)                */
/* parameter                                                        */
/* <type>   : BUCK / LDO                                            */
/* <name>   : BUCK name / LDO name                                  */
/* <user>   : SRCLKEN0 / SRCLKEN1 / SRCLKEN2 / SRCLKEN3 / SW / SPM  */
/* <op_mode>: user control mode                                     */
/*            0: multi-user mode                                    */
/*            1: low-power mode                                     */
/* <op_en>  : user control enable                                   */
/* <op_cfg> :                                                       */
/*     HW mode :                                                    */
/*            0 : define as ON/OFF control                          */
/*            1 : define as LP/No LP control                        */
/*     SW/SPM :                                                     */
/*            0 : OFF                                               */
/*            1 : ON                                                */
/* ---------------------------------------------------------------  */

#define pmic_set_sw_en(addr, val)             \
			pmic_config_interface_nolock(addr, val, 1, 0)
#define pmic_set_sw_lp(addr, val)             \
			pmic_config_interface_nolock(addr, val, 1, 1)
#define pmic_set_buck_op_mode(user, addr, val)   \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_set_ldo_op_mode(user, addr, val)    \
			pmic_config_interface_nolock(addr, val, 1, (user) + 10)
#define pmic_set_op_en(user, addr, val)       \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_set_op_cfg(user, addr, val)      \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_get_buck_op_mode(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, user)
#define pmic_get_ldo_op_mode(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, (user) + 10)
#define pmic_get_op_en(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, user)
#define pmic_get_op_cfg(user, addr, pval)     \
			pmic_read_interface_nolock(addr, pval, 1, user)

#define en_cfg_shift  0x6

#if defined(LGS) || defined(LGSWS)

const struct PMU_LP_TABLE_ENTRY pmu_lp_table[] = {
	PMIC_LP_BUCK_ENTRY(VCORE),
	PMIC_LP_BUCK_ENTRY(VPU),
	PMIC_LP_BUCK_ENTRY(VPROC1),
	PMIC_LP_BUCK_ENTRY(VPROC2),
	PMIC_LP_BUCK_ENTRY(VGPU11),
	PMIC_LP_BUCK_ENTRY(VGPU12),
	PMIC_LP_BUCK_ENTRY(VMODEM),
	PMIC_LP_BUCK_ENTRY(VS1),
	PMIC_LP_BUCK_ENTRY(VS2),
	PMIC_LP_BUCK_ENTRY(VPA),
	PMIC_LP_LDO_ENTRY(VSRAM_PROC1),
	PMIC_LP_LDO_ENTRY(VSRAM_PROC2),
	PMIC_LP_LDO_ENTRY(VSRAM_OTHERS),
	PMIC_LP_LDO_ENTRY(VSRAM_MD),
	PMIC_LP_LDO_ENTRY(VCAMIO),
	PMIC_LP_LDO_ENTRY(VM18),
	PMIC_LP_LDO_ENTRY(VCN18),
	PMIC_LP_LDO_ENTRY(VCN13),
	PMIC_LP_LDO_ENTRY(VRF18),
	PMIC_LP_LDO_ENTRY(VIO18),
	PMIC_LP_LDO_ENTRY(VEFUSE),
	PMIC_LP_LDO_ENTRY(VRF12),
	PMIC_LP_LDO_ENTRY(VRFCK),
	PMIC_LP_LDO_ENTRY(VA12),
	PMIC_LP_LDO_ENTRY(VA09),
	PMIC_LP_LDO_ENTRY(VBBCK),
	PMIC_LP_LDO_ENTRY(VFE28),
	PMIC_LP_LDO_ENTRY(VBIF28),
	PMIC_LP_LDO_ENTRY(VAUD18),
	PMIC_LP_LDO_ENTRY(VAUX18),
	PMIC_LP_LDO_ENTRY(VXO22),
	PMIC_LP_LDO_VCN33_1_0_ENTRY(VCN33_1_0),
	PMIC_LP_LDO_VCN33_1_1_ENTRY(VCN33_1_1),
	PMIC_LP_LDO_VCN33_2_0_ENTRY(VCN33_2_0),
	PMIC_LP_LDO_VCN33_2_1_ENTRY(VCN33_2_1),
	PMIC_LP_LDO_VUSB_0_ENTRY(VUSB_0),
	PMIC_LP_LDO_VUSB_1_ENTRY(VUSB_1),
	PMIC_LP_LDO_ENTRY(VEMC),
	PMIC_LP_LDO_ENTRY(VIO28),
	PMIC_LP_LDO_ENTRY(VSIM1),
	PMIC_LP_LDO_ENTRY(VSIM2),
	PMIC_LP_LDO_ENTRY(VUFS),
	PMIC_LP_LDO_ENTRY(VIBR),
};

static int pmic_lp_golden_set(unsigned int en_adr,
		unsigned char op_en, unsigned char op_cfg)
{
	unsigned int en_cfg = 0, lp_cfg = 0;

	/*--op_cfg 0:SW_OFF, 1:SW_EN, 3: SW_LP (SPM)--*/
	if (op_en > 1 || op_cfg > 3) {
		pr_notice("p\n");
		return -1;
	}

	en_cfg = op_cfg & 0x1;
	lp_cfg = (op_cfg >> 1) & 0x1;
	pmic_set_sw_en(en_adr, en_cfg);
	pmic_set_sw_lp(en_adr, lp_cfg);
}
#endif

static int pmic_lp_type_set(
		unsigned short en_cfg_adr,
		enum PMU_LP_TABLE_ENUM name,
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	unsigned int rb_en = 0, rb_cfg = 0, max_cfg = 1;
	unsigned short op_en_adr = 0, op_cfg_adr = 0;
	int ret = 0, ret_en = 0, ret_cfg = 0;

	if (en_cfg_adr) {
		op_en_adr = en_cfg_adr;
		op_cfg_adr = (unsigned short)(en_cfg_adr + en_cfg_shift);
	}
	/*--else keep default adr = 0--*/
	if (user == SW || user == SPM) {
		max_cfg = 3;
		rb_cfg = 0;
		rb_en = 0;
	}

	if (op_en > 1 || op_cfg > max_cfg) {
		pr_notice("p\n");
		return -1;
	}

	PMICLOG("0x%x,user %d\n", en_cfg_adr, user);

#if defined(LGS) || defined(LGSWS)
	const struct PMU_LP_TABLE_ENTRY *pFlag = &pmu_lp_table[name];

	if (user == SW || user == SPM)
		pmic_lp_golden_set((unsigned int)pFlag->en_adr, op_en, op_cfg);
#endif

	if (op_cfg_adr && op_en_adr) {
		pmic_set_op_en(user, op_en_adr, op_en);
		pmic_get_op_en(user, op_en_adr, &rb_en);
		PMICLOG("user = %d, op en = %d\n", user, rb_en);
		(rb_en == op_en) ? (ret_en = 0) : (ret_en = -1);
		if (user != SW && user != SPM) {
			pmic_set_op_cfg(user, op_cfg_adr, op_cfg);
			pmic_get_op_cfg(user, op_cfg_adr, &rb_cfg);
			(rb_cfg == op_cfg) ? (ret_cfg = 0) : (ret_cfg - 1);
			PMICLOG("user = %d, op cfg = %d\n", user, rb_cfg);
		}
	}

	((!ret_en) && (!ret_cfg)) ? (ret = 0) : (ret = -1);
	if (ret)
		pr_notice("%d, %d, %d\n", user, ret_en, ret_cfg);
	return ret;
}

int pmic_buck_vcore_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VCORE_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VCORE_OP_EN,
				VCORE, user, op_en, op_cfg);
}

int pmic_buck_vpu_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VPU_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VPU_OP_EN,
				VPU, user, op_en, op_cfg);
}

int pmic_buck_vproc1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VPROC1_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VPROC1_OP_EN,
				VPROC1, user, op_en, op_cfg);
}

int pmic_buck_vproc2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VPROC2_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VPROC2_OP_EN,
				VPROC2, user, op_en, op_cfg);
}

int pmic_buck_vgpu11_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VGPU11_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VGPU11_OP_EN,
				VGPU11, user, op_en, op_cfg);
}

int pmic_buck_vgpu12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VGPU12_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VGPU12_OP_EN,
				VGPU12, user, op_en, op_cfg);
}

int pmic_buck_vmodem_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VMODEM_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VMODEM_OP_EN,
				VMODEM, user, op_en, op_cfg);
}

int pmic_buck_vs1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VS1_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VS1_OP_EN,
				VS1, user, op_en, op_cfg);
}

int pmic_buck_vs2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user != SW && user != SPM) {
		pmic_set_buck_op_mode(user, MT6359_BUCK_VS2_OP_MODE,
				op_mode);
	}
	return pmic_lp_type_set(MT6359_BUCK_VS2_OP_EN,
				VS2, user, op_en, op_cfg);
}

int pmic_buck_vpa_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(0, VPA, user, op_en, op_cfg);
}

int pmic_ldo_vsram_proc1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSRAM_PROC1_CON0,
				     op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSRAM_PROC1_OP_EN,
				VSRAM_PROC1, user, op_en, op_cfg);
}

int pmic_ldo_vsram_proc2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSRAM_PROC2_CON0,
				     op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSRAM_PROC2_OP_EN,
				VSRAM_PROC2, user, op_en, op_cfg);
}

int pmic_ldo_vsram_others_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSRAM_OTHERS_CON0,
				     op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSRAM_OTHERS_OP_EN,
				VSRAM_OTHERS, user, op_en, op_cfg);
}

int pmic_ldo_vsram_md_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSRAM_MD_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSRAM_MD_OP_EN,
				VSRAM_MD, user, op_en, op_cfg);
}

int pmic_ldo_vcamio_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VCAMIO_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VCAMIO_OP_EN,
				VCAMIO, user, op_en, op_cfg);
}

int pmic_ldo_vm18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VM18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VM18_OP_EN,
				VM18, user, op_en, op_cfg);
}

int pmic_ldo_vcn18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VCN18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VCN18_OP_EN,
				VCN18, user, op_en, op_cfg);
}

int pmic_ldo_vcn13_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VCN13_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VCN13_OP_EN,
				VCN13, user, op_en, op_cfg);
}

int pmic_ldo_vrf18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VRF18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VRF18_OP_EN,
				VRF18, user, op_en, op_cfg);
}

int pmic_ldo_vio18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VIO18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VIO18_OP_EN,
				VIO18, user, op_en, op_cfg);
}

int pmic_ldo_vefuse_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VEFUSE_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VEFUSE_OP_EN,
				VEFUSE, user, op_en, op_cfg);
}

int pmic_ldo_vrf12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VRF12_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VRF12_OP_EN,
				VRF12, user, op_en, op_cfg);
}

int pmic_ldo_vrfck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VRFCK_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VRFCK_OP_EN,
				VRFCK, user, op_en, op_cfg);
}

int pmic_ldo_va12_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VA12_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VA12_OP_EN,
				VA12, user, op_en, op_cfg);
}

int pmic_ldo_va09_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VA09_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VA09_OP_EN,
				VA09, user, op_en, op_cfg);
}

int pmic_ldo_vbbck_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VBBCK_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VBBCK_OP_EN,
				VBBCK, user, op_en, op_cfg);
}

int pmic_ldo_vfe28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VFE28_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VFE28_OP_EN,
				VFE28, user, op_en, op_cfg);
}

int pmic_ldo_vbif28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VBIF28_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VBIF28_OP_EN,
				VBIF28, user, op_en, op_cfg);
}

int pmic_ldo_vaud18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VAUD18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VAUD18_OP_EN,
				VAUD18, user, op_en, op_cfg);
}

int pmic_ldo_vaux18_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VAUX18_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VAUX18_OP_EN,
				VAUX18, user, op_en, op_cfg);
}

int pmic_ldo_vxo22_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VXO22_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VXO22_OP_EN,
				VXO22, user, op_en, op_cfg);
}

int pmic_ldo_vcn33_1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VCN33_1_CON0, op_mode);
	ret = pmic_lp_type_set(MT6359_LDO_VCN33_1_OP_EN,
			VCN33_1_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6359_LDO_VCN33_1_OP_EN,
				VCN33_1_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_vcn33_2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VCN33_2_CON0, op_mode);
	ret = pmic_lp_type_set(MT6359_LDO_VCN33_2_OP_EN,
			VCN33_2_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6359_LDO_VCN33_2_OP_EN,
				VCN33_2_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_vusb_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VUSB_CON0, op_mode);
	ret = pmic_lp_type_set(MT6359_LDO_VUSB_OP_EN,
			VUSB_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6359_LDO_VUSB_OP_EN,
				VUSB_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_vemc_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VEMC_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VEMC_OP_EN,
				VEMC, user, op_en, op_cfg);
}

int pmic_ldo_vio28_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VIO28_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VIO28_OP_EN,
				VIO28, user, op_en, op_cfg);
}

int pmic_ldo_vsim1_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSIM1_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSIM1_OP_EN,
				VSIM1, user, op_en, op_cfg);
}

int pmic_ldo_vsim2_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VSIM2_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VSIM2_OP_EN,
				VSIM2, user, op_en, op_cfg);
}

int pmic_ldo_vufs_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VUFS_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VUFS_OP_EN,
				VUFS, user, op_en, op_cfg);
}

int pmic_ldo_vibr_lp(
		enum BUCK_LDO_EN_USER user,
		int op_mode,
		unsigned char op_en,
		unsigned char op_cfg)
{
	if (user <= SRCLKEN2)
		pmic_set_ldo_op_mode(user, MT6359_LDO_VIBR_CON0, op_mode);
	return pmic_lp_type_set(MT6359_LDO_VIBR_OP_EN,
				VIBR, user, op_en, op_cfg);
}
