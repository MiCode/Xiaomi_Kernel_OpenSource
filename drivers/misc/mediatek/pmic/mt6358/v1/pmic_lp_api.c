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
#define pmic_set_op_en(user, addr, val)       \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_set_op_cfg(user, addr, val)      \
			pmic_config_interface_nolock(addr, val, 1, user)
#define pmic_get_op_en(user, addr, pval)      \
			pmic_read_interface_nolock(addr, pval, 1, user)
#define pmic_get_op_cfg(user, addr, pval)     \
			pmic_read_interface_nolock(addr, pval, 1, user)

#define en_cfg_shift  0x6

#if defined(LGS) || defined(LGSWS)

const struct PMU_LP_TABLE_ENTRY pmu_lp_table[] = {
	PMIC_LP_BUCK_ENTRY(VPROC11),
	PMIC_LP_BUCK_ENTRY(VCORE),
	PMIC_LP_BUCK_ENTRY(VGPU),
	PMIC_LP_BUCK_ENTRY(VMODEM),
	PMIC_LP_BUCK_ENTRY(VS1),
	PMIC_LP_BUCK_ENTRY(VS2),
	PMIC_LP_BUCK_ENTRY(VPA),
	PMIC_LP_BUCK_ENTRY(VDRAM1),
	PMIC_LP_BUCK_ENTRY(VPROC12),
	PMIC_LP_LDO_ENTRY(VSRAM_GPU),
	PMIC_LP_LDO_ENTRY(VSRAM_OTHERS),
	PMIC_LP_LDO_ENTRY(VSRAM_PROC11),
	PMIC_LP_LDO_ENTRY(VXO22),
	PMIC_LP_LDO_ENTRY(VRF18),
	PMIC_LP_LDO_ENTRY(VRF12),
	PMIC_LP_LDO_ENTRY(VEFUSE),
	PMIC_LP_LDO_VCN33_0_ENTRY(VCN33_0),
	PMIC_LP_LDO_VCN33_1_ENTRY(VCN33_1),
	PMIC_LP_LDO_ENTRY(VCN28),
	PMIC_LP_LDO_ENTRY(VCN18),
	PMIC_LP_LDO_ENTRY(VCAMA1),
	PMIC_LP_LDO_ENTRY(VCAMD),
	PMIC_LP_LDO_ENTRY(VCAMA2),
	PMIC_LP_LDO_ENTRY(VSRAM_PROC12),
	PMIC_LP_LDO_ENTRY(VCAMIO),
	PMIC_LP_LDO_VLDO28_0_ENTRY(VLDO28_0),
	PMIC_LP_LDO_VLDO28_1_ENTRY(VLDO28_1),
	PMIC_LP_LDO_ENTRY(VA12),
	PMIC_LP_LDO_ENTRY(VAUX18),
	PMIC_LP_LDO_ENTRY(VAUD28),
	PMIC_LP_LDO_ENTRY(VIO28),
	PMIC_LP_LDO_ENTRY(VIO18),
	PMIC_LP_LDO_ENTRY(VFE28),
	PMIC_LP_LDO_ENTRY(VDRAM2),
	PMIC_LP_LDO_ENTRY(VMC),
	PMIC_LP_LDO_ENTRY(VMCH),
	PMIC_LP_LDO_ENTRY(VEMC),
	PMIC_LP_LDO_ENTRY(VSIM1),
	PMIC_LP_LDO_ENTRY(VSIM2),
	PMIC_LP_LDO_ENTRY(VIBR),
	PMIC_LP_LDO_VUSB_0_ENTRY(VUSB_0),
	PMIC_LP_LDO_VUSB_1_ENTRY(VUSB_1),
	PMIC_LP_LDO_ENTRY(VBIF28),
#if defined(USE_PMIC_MT6366) && USE_PMIC_MT6366
	PMIC_LP_LDO_ENTRY(VM18),
	PMIC_LP_LDO_ENTRY(VMDDR),
	PMIC_LP_LDO_ENTRY(VSRAM_CORE),
#endif
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

static int pmic_lp_type_set(unsigned short en_cfg_adr,
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

	PMICLOG("0x%x,type %d\n", en_cfg_adr, user);

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

int pmic_buck_vproc11_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VPROC11_OP_EN,
				VPROC11, user, op_en, op_cfg);
}

int pmic_buck_vcore_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VCORE_OP_EN,
				VCORE, user, op_en, op_cfg);
}

int pmic_buck_vgpu_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VGPU_OP_EN,
				VGPU, user, op_en, op_cfg);
}

int pmic_buck_vmodem_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VMODEM_OP_EN,
				VMODEM, user, op_en, op_cfg);
}

int pmic_buck_vs1_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VS1_OP_EN,
				VS1, user, op_en, op_cfg);
}

int pmic_buck_vs2_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VS2_OP_EN,
				VS2, user, op_en, op_cfg);
}

int pmic_buck_vpa_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(0,
				VPA, user, op_en, op_cfg);
}

int pmic_buck_vdram1_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VDRAM1_OP_EN,
				VDRAM1, user, op_en, op_cfg);
}

int pmic_buck_vproc12_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_BUCK_VPROC12_OP_EN,
				VPROC12, user, op_en, op_cfg);
}

int pmic_ldo_vsram_gpu_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSRAM_GPU_OP_EN,
				VSRAM_GPU, user, op_en, op_cfg);
}

int pmic_ldo_vsram_others_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSRAM_OTHERS_OP_EN,
				VSRAM_OTHERS, user, op_en, op_cfg);
}

int pmic_ldo_vsram_proc11_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSRAM_PROC11_OP_EN,
				VSRAM_PROC11, user, op_en, op_cfg);
}

int pmic_ldo_vxo22_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VXO22_OP_EN,
				VXO22, user, op_en, op_cfg);
}

int pmic_ldo_vrf18_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VRF18_OP_EN,
				VRF18, user, op_en, op_cfg);
}

int pmic_ldo_vrf12_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VRF12_OP_EN,
				VRF12, user, op_en, op_cfg);
}

int pmic_ldo_vefuse_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VEFUSE_OP_EN,
				VEFUSE, user, op_en, op_cfg);
}

int pmic_ldo_vcn33_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	ret = pmic_lp_type_set(MT6358_LDO_VCN33_OP_EN,
			VCN33_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6358_LDO_VCN33_OP_EN,
				VCN33_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_vcn28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCN28_OP_EN,
				VCN28, user, op_en, op_cfg);
}

int pmic_ldo_vcn18_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCN18_OP_EN,
				VCN18, user, op_en, op_cfg);
}

int pmic_ldo_vcama1_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCAMA1_OP_EN,
				VCAMA1, user, op_en, op_cfg);
}

int pmic_ldo_vcamd_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCAMD_OP_EN,
				VCAMD, user, op_en, op_cfg);
}

int pmic_ldo_vcama2_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCAMA2_OP_EN,
				VCAMA2, user, op_en, op_cfg);
}

int pmic_ldo_vsram_proc12_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSRAM_PROC12_OP_EN,
				VSRAM_PROC12, user, op_en, op_cfg);
}

int pmic_ldo_vcamio_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VCAMIO_OP_EN,
				VCAMIO, user, op_en, op_cfg);
}

int pmic_ldo_vldo28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	ret = pmic_lp_type_set(MT6358_LDO_VLDO28_OP_EN,
			VLDO28_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6358_LDO_VLDO28_OP_EN,
				VLDO28_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_va12_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VA12_OP_EN,
				VA12, user, op_en, op_cfg);
}

int pmic_ldo_vaux18_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VAUX18_OP_EN,
				VAUX18, user, op_en, op_cfg);
}

int pmic_ldo_vaud28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VAUD28_OP_EN,
				VAUD28, user, op_en, op_cfg);
}

int pmic_ldo_vio28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VIO28_OP_EN,
				VIO28, user, op_en, op_cfg);
}

int pmic_ldo_vio18_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VIO18_OP_EN,
				VIO18, user, op_en, op_cfg);
}

int pmic_ldo_vfe28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VFE28_OP_EN,
				VFE28, user, op_en, op_cfg);
}

int pmic_ldo_vdram2_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VDRAM2_OP_EN,
				VDRAM2, user, op_en, op_cfg);
}

int pmic_ldo_vmc_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VMC_OP_EN,
				VMC, user, op_en, op_cfg);
}

int pmic_ldo_vmch_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VMCH_OP_EN,
				VMCH, user, op_en, op_cfg);
}

int pmic_ldo_vemc_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VEMC_OP_EN,
				VEMC, user, op_en, op_cfg);
}

int pmic_ldo_vsim1_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSIM1_OP_EN,
				VSIM1, user, op_en, op_cfg);
}

int pmic_ldo_vsim2_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSIM2_OP_EN,
				VSIM2, user, op_en, op_cfg);
}

int pmic_ldo_vibr_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VIBR_OP_EN,
				VIBR, user, op_en, op_cfg);
}

int pmic_ldo_vusb_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	int ret = 0;

	ret = pmic_lp_type_set(MT6358_LDO_VUSB_OP_EN,
			VUSB_0, user, op_en, op_cfg);
	if (ret)
		return ret;

	ret = pmic_lp_type_set(MT6358_LDO_VUSB_OP_EN,
				VUSB_1, user, op_en, op_cfg);
	return ret;
}

int pmic_ldo_vbif28_lp(
		enum BUCK_LDO_EN_USER user,
		unsigned char op_en,
		unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VBIF28_OP_EN,
				VBIF28, user, op_en, op_cfg);
}

#if defined(USE_PMIC_MT6366) && USE_PMIC_MT6366
int pmic_ldo_vm18_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VM18_OP_EN, VM18, user, op_en, op_cfg);
}

int pmic_ldo_vmddr_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VMDDR_OP_EN, VMDDR, user, op_en, op_cfg);
}

int pmic_ldo_vsram_core_lp(enum BUCK_LDO_EN_USER user, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_lp_type_set(MT6358_LDO_VSRAM_CORE_OP_EN, VSRAM_CORE, user, op_en, op_cfg);
}
#endif
