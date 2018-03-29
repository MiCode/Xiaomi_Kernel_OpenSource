/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_API_LDO_H_
#define _MT_PMIC_API_LDO_H_

void pmic_ldo_vio28_sw_en(int en_value);
void pmic_ldo_vio18_sw_en(int en_value);
void pmic_ldo_vufs18_sw_en(int en_value);
void pmic_ldo_va10_sw_en(int en_value);
void pmic_ldo_va12_sw_en(int en_value);
void pmic_ldo_va18_sw_en(int en_value);
void pmic_ldo_vusb33_sw_en(int en_value);
void pmic_ldo_vemc_sw_en(int en_value);
void pmic_ldo_vxo22_sw_en(int en_value);
void pmic_ldo_vefuse_sw_en(int en_value);
void pmic_ldo_vsim1_sw_en(int en_value);
void pmic_ldo_vsim2_sw_en(int en_value);
void pmic_ldo_vcamaf_sw_en(int en_value);
void pmic_ldo_vtouch_sw_en(int en_value);
void pmic_ldo_vcamd1_sw_en(int en_value);
void pmic_ldo_vcamd2_sw_en(int en_value);
void pmic_ldo_vcamio_sw_en(int en_value);
void pmic_ldo_vmipi_sw_en(int en_value);
void pmic_ldo_vgp3_sw_en(int en_value);
void pmic_ldo_vcn33_bt_sw_en(int en_value);
void pmic_ldo_vcn33_wifi_sw_en(int en_value);
void pmic_ldo_vcn18_bt_sw_en(int en_value);
void pmic_ldo_vcn18_wifi_sw_en(int en_value);
void pmic_ldo_vcn28_sw_en(int en_value);
void pmic_ldo_vibr_sw_en(int en_value);
void pmic_ldo_vbif28_sw_en(int en_value);
void pmic_ldo_vfe28_sw_en(int en_value);
void pmic_ldo_vmch_sw_en(int en_value);
void pmic_ldo_vmc_sw_en(int en_value);
void pmic_ldo_vrf18_1_sw_en(int en_value);
void pmic_ldo_vrf18_2_sw_en(int en_value);
void pmic_ldo_vrf12_sw_en(int en_value);
void pmic_ldo_vcama1_sw_en(int en_value);
void pmic_ldo_vcama2_sw_en(int en_value);
void pmic_ldo_vrtc_sw_en(int en_value);
void pmic_ldo_vsram_dvfs1_sw_en(int en_value);
void pmic_ldo_vsram_dvfs2_sw_en(int en_value);
void pmic_ldo_vsram_vgpu_sw_en(int en_value);
void pmic_ldo_vsram_vcore_sw_en(int en_value);
void pmic_ldo_vsram_vmd_sw_en(int en_value);

#endif				/* _MT_PMIC_API_LDO_H_ */
