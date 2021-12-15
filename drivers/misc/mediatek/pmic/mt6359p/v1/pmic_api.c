/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/printk.h>
#include <mach/upmu_hw.h>
#include <mt-plat/upmu_common.h>

unsigned int mt6359_upmu_set_rg_buck_vpu_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPU_EN_ADDR,
		val,
		PMIC_RG_BUCK_VPU_EN_MASK,
		PMIC_RG_BUCK_VPU_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vpu_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPU_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VPU_EN_MASK,
		PMIC_RG_BUCK_VPU_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vpu_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VPU_VOSEL_ADDR,
		&val,
		PMIC_DA_VPU_VOSEL_MASK,
		PMIC_DA_VPU_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vpu_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPU_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VPU_VOSEL_MASK,
		PMIC_RG_BUCK_VPU_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vpu_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPU_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VPU_VOSEL_MASK,
		PMIC_RG_BUCK_VPU_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vcore_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VCORE_EN_ADDR,
		val,
		PMIC_RG_BUCK_VCORE_EN_MASK,
		PMIC_RG_BUCK_VCORE_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vcore_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VCORE_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VCORE_EN_MASK,
		PMIC_RG_BUCK_VCORE_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vcore_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VCORE_VOSEL_ADDR,
		&val,
		PMIC_DA_VCORE_VOSEL_MASK,
		PMIC_DA_VCORE_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vcore_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VCORE_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VCORE_VOSEL_MASK,
		PMIC_RG_BUCK_VCORE_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vcore_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VCORE_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VCORE_VOSEL_MASK,
		PMIC_RG_BUCK_VCORE_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vgpu11_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VGPU11_EN_ADDR,
		val,
		PMIC_RG_BUCK_VGPU11_EN_MASK,
		PMIC_RG_BUCK_VGPU11_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vgpu11_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VGPU11_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VGPU11_EN_MASK,
		PMIC_RG_BUCK_VGPU11_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vgpu11_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VGPU11_VOSEL_ADDR,
		&val,
		PMIC_DA_VGPU11_VOSEL_MASK,
		PMIC_DA_VGPU11_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vgpu11_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VGPU11_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VGPU11_VOSEL_MASK,
		PMIC_RG_BUCK_VGPU11_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vgpu11_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VGPU11_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VGPU11_VOSEL_MASK,
		PMIC_RG_BUCK_VGPU11_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vgpu12_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VGPU12_EN_ADDR,
		val,
		PMIC_RG_BUCK_VGPU12_EN_MASK,
		PMIC_RG_BUCK_VGPU12_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vgpu12_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VGPU12_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VGPU12_EN_MASK,
		PMIC_RG_BUCK_VGPU12_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vgpu12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VGPU12_VOSEL_ADDR,
		&val,
		PMIC_DA_VGPU12_VOSEL_MASK,
		PMIC_DA_VGPU12_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vgpu12_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VGPU12_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VGPU12_VOSEL_MASK,
		PMIC_RG_BUCK_VGPU12_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vgpu12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VGPU12_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VGPU12_VOSEL_MASK,
		PMIC_RG_BUCK_VGPU12_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vmodem_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VMODEM_EN_ADDR,
		val,
		PMIC_RG_BUCK_VMODEM_EN_MASK,
		PMIC_RG_BUCK_VMODEM_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vmodem_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VMODEM_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VMODEM_EN_MASK,
		PMIC_RG_BUCK_VMODEM_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vmodem_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VMODEM_VOSEL_ADDR,
		&val,
		PMIC_DA_VMODEM_VOSEL_MASK,
		PMIC_DA_VMODEM_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vmodem_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VMODEM_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VMODEM_VOSEL_MASK,
		PMIC_RG_BUCK_VMODEM_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vmodem_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VMODEM_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VMODEM_VOSEL_MASK,
		PMIC_RG_BUCK_VMODEM_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vproc1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPROC1_EN_ADDR,
		val,
		PMIC_RG_BUCK_VPROC1_EN_MASK,
		PMIC_RG_BUCK_VPROC1_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vproc1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPROC1_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VPROC1_EN_MASK,
		PMIC_RG_BUCK_VPROC1_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vproc1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VPROC1_VOSEL_ADDR,
		&val,
		PMIC_DA_VPROC1_VOSEL_MASK,
		PMIC_DA_VPROC1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vproc1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPROC1_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VPROC1_VOSEL_MASK,
		PMIC_RG_BUCK_VPROC1_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vproc1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPROC1_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VPROC1_VOSEL_MASK,
		PMIC_RG_BUCK_VPROC1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vproc2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPROC2_EN_ADDR,
		val,
		PMIC_RG_BUCK_VPROC2_EN_MASK,
		PMIC_RG_BUCK_VPROC2_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vproc2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPROC2_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VPROC2_EN_MASK,
		PMIC_RG_BUCK_VPROC2_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vproc2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VPROC2_VOSEL_ADDR,
		&val,
		PMIC_DA_VPROC2_VOSEL_MASK,
		PMIC_DA_VPROC2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vproc2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPROC2_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VPROC2_VOSEL_MASK,
		PMIC_RG_BUCK_VPROC2_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vproc2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPROC2_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VPROC2_VOSEL_MASK,
		PMIC_RG_BUCK_VPROC2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vs1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VS1_EN_ADDR,
		val,
		PMIC_RG_BUCK_VS1_EN_MASK,
		PMIC_RG_BUCK_VS1_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vs1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VS1_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VS1_EN_MASK,
		PMIC_RG_BUCK_VS1_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vs1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VS1_VOSEL_ADDR,
		&val,
		PMIC_DA_VS1_VOSEL_MASK,
		PMIC_DA_VS1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vs1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VS1_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VS1_VOSEL_MASK,
		PMIC_RG_BUCK_VS1_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vs1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VS1_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VS1_VOSEL_MASK,
		PMIC_RG_BUCK_VS1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vs2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VS2_EN_ADDR,
		val,
		PMIC_RG_BUCK_VS2_EN_MASK,
		PMIC_RG_BUCK_VS2_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vs2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VS2_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VS2_EN_MASK,
		PMIC_RG_BUCK_VS2_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vs2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VS2_VOSEL_ADDR,
		&val,
		PMIC_DA_VS2_VOSEL_MASK,
		PMIC_DA_VS2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vs2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VS2_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VS2_VOSEL_MASK,
		PMIC_RG_BUCK_VS2_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vs2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VS2_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VS2_VOSEL_MASK,
		PMIC_RG_BUCK_VS2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vpa_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPA_EN_ADDR,
		val,
		PMIC_RG_BUCK_VPA_EN_MASK,
		PMIC_RG_BUCK_VPA_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vpa_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPA_EN_ADDR,
		&val,
		PMIC_RG_BUCK_VPA_EN_MASK,
		PMIC_RG_BUCK_VPA_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_buck_vpa_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_BUCK_VPA_VOSEL_ADDR,
		val,
		PMIC_RG_BUCK_VPA_VOSEL_MASK,
		PMIC_RG_BUCK_VPA_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_buck_vpa_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_BUCK_VPA_VOSEL_ADDR,
		&val,
		PMIC_RG_BUCK_VPA_VOSEL_MASK,
		PMIC_RG_BUCK_VPA_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vpa_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VPA_VOSEL_ADDR,
		&val,
		PMIC_DA_VPA_VOSEL_MASK,
		PMIC_DA_VPA_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vrtc28_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VRTC28_EN_ADDR,
		val,
		PMIC_RG_VRTC28_EN_MASK,
		PMIC_RG_VRTC28_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vrtc28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VRTC28_EN_ADDR,
		&val,
		PMIC_RG_VRTC28_EN_MASK,
		PMIC_RG_VRTC28_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_proc1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_proc1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_PROC1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_proc2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_proc2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_PROC2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_others_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_others_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_md_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_MD_VOSEL_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_md_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_MD_VOSEL_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_MASK,
		PMIC_RG_LDO_VSRAM_MD_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vemc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VEMC_VOSEL_0_ADDR,
		val,
		PMIC_RG_VEMC_VOSEL_0_MASK,
		PMIC_RG_VEMC_VOSEL_0_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vemc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VEMC_VOSEL_0_ADDR,
		&val,
		PMIC_RG_VEMC_VOSEL_0_MASK,
		PMIC_RG_VEMC_VOSEL_0_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vfe28_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VFE28_EN_ADDR,
		val,
		PMIC_RG_LDO_VFE28_EN_MASK,
		PMIC_RG_LDO_VFE28_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vfe28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VFE28_EN_ADDR,
		&val,
		PMIC_RG_LDO_VFE28_EN_MASK,
		PMIC_RG_LDO_VFE28_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vxo22_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VXO22_EN_ADDR,
		val,
		PMIC_RG_LDO_VXO22_EN_MASK,
		PMIC_RG_LDO_VXO22_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vxo22_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VXO22_EN_ADDR,
		&val,
		PMIC_RG_LDO_VXO22_EN_MASK,
		PMIC_RG_LDO_VXO22_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vrf18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VRF18_EN_ADDR,
		val,
		PMIC_RG_LDO_VRF18_EN_MASK,
		PMIC_RG_LDO_VRF18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vrf18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VRF18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VRF18_EN_MASK,
		PMIC_RG_LDO_VRF18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vrf12_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VRF12_EN_ADDR,
		val,
		PMIC_RG_LDO_VRF12_EN_MASK,
		PMIC_RG_LDO_VRF12_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vrf12_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VRF12_EN_ADDR,
		&val,
		PMIC_RG_LDO_VRF12_EN_MASK,
		PMIC_RG_LDO_VRF12_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vefuse_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VEFUSE_EN_ADDR,
		val,
		PMIC_RG_LDO_VEFUSE_EN_MASK,
		PMIC_RG_LDO_VEFUSE_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vefuse_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VEFUSE_EN_ADDR,
		&val,
		PMIC_RG_LDO_VEFUSE_EN_MASK,
		PMIC_RG_LDO_VEFUSE_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn33_1_bt_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN33_1_EN_0_ADDR,
		val,
		PMIC_RG_LDO_VCN33_1_EN_0_MASK,
		PMIC_RG_LDO_VCN33_1_EN_0_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn33_1_bt_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN33_1_EN_0_ADDR,
		&val,
		PMIC_RG_LDO_VCN33_1_EN_0_MASK,
		PMIC_RG_LDO_VCN33_1_EN_0_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn33_1_wifi_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN33_1_EN_1_ADDR,
		val,
		PMIC_RG_LDO_VCN33_1_EN_1_MASK,
		PMIC_RG_LDO_VCN33_1_EN_1_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn33_1_wifi_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN33_1_EN_1_ADDR,
		&val,
		PMIC_RG_LDO_VCN33_1_EN_1_MASK,
		PMIC_RG_LDO_VCN33_1_EN_1_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn33_2_bt_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN33_2_EN_0_ADDR,
		val,
		PMIC_RG_LDO_VCN33_2_EN_0_MASK,
		PMIC_RG_LDO_VCN33_2_EN_0_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn33_2_bt_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN33_2_EN_0_ADDR,
		&val,
		PMIC_RG_LDO_VCN33_2_EN_0_MASK,
		PMIC_RG_LDO_VCN33_2_EN_0_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn33_2_wifi_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN33_2_EN_1_ADDR,
		val,
		PMIC_RG_LDO_VCN33_2_EN_1_MASK,
		PMIC_RG_LDO_VCN33_2_EN_1_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn33_2_wifi_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN33_2_EN_1_ADDR,
		&val,
		PMIC_RG_LDO_VCN33_2_EN_1_MASK,
		PMIC_RG_LDO_VCN33_2_EN_1_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn13_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN13_EN_ADDR,
		val,
		PMIC_RG_LDO_VCN13_EN_MASK,
		PMIC_RG_LDO_VCN13_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn13_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN13_EN_ADDR,
		&val,
		PMIC_RG_LDO_VCN13_EN_MASK,
		PMIC_RG_LDO_VCN13_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcn18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCN18_EN_ADDR,
		val,
		PMIC_RG_LDO_VCN18_EN_MASK,
		PMIC_RG_LDO_VCN18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcn18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCN18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VCN18_EN_MASK,
		PMIC_RG_LDO_VCN18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_va09_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VA09_EN_ADDR,
		val,
		PMIC_RG_LDO_VA09_EN_MASK,
		PMIC_RG_LDO_VA09_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_va09_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VA09_EN_ADDR,
		&val,
		PMIC_RG_LDO_VA09_EN_MASK,
		PMIC_RG_LDO_VA09_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vcamio_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VCAMIO_EN_ADDR,
		val,
		PMIC_RG_LDO_VCAMIO_EN_MASK,
		PMIC_RG_LDO_VCAMIO_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vcamio_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VCAMIO_EN_ADDR,
		&val,
		PMIC_RG_LDO_VCAMIO_EN_MASK,
		PMIC_RG_LDO_VCAMIO_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_va12_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VA12_EN_ADDR,
		val,
		PMIC_RG_LDO_VA12_EN_MASK,
		PMIC_RG_LDO_VA12_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_va12_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VA12_EN_ADDR,
		&val,
		PMIC_RG_LDO_VA12_EN_MASK,
		PMIC_RG_LDO_VA12_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vaux18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VAUX18_EN_ADDR,
		val,
		PMIC_RG_LDO_VAUX18_EN_MASK,
		PMIC_RG_LDO_VAUX18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vaux18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VAUX18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VAUX18_EN_MASK,
		PMIC_RG_LDO_VAUX18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vaud18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VAUD18_EN_ADDR,
		val,
		PMIC_RG_LDO_VAUD18_EN_MASK,
		PMIC_RG_LDO_VAUD18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vaud18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VAUD18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VAUD18_EN_MASK,
		PMIC_RG_LDO_VAUD18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vio18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VIO18_EN_ADDR,
		val,
		PMIC_RG_LDO_VIO18_EN_MASK,
		PMIC_RG_LDO_VIO18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vio18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VIO18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VIO18_EN_MASK,
		PMIC_RG_LDO_VIO18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vemc_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VEMC_EN_ADDR,
		val,
		PMIC_RG_LDO_VEMC_EN_MASK,
		PMIC_RG_LDO_VEMC_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vemc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VEMC_EN_ADDR,
		&val,
		PMIC_RG_LDO_VEMC_EN_MASK,
		PMIC_RG_LDO_VEMC_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsim1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSIM1_EN_ADDR,
		val,
		PMIC_RG_LDO_VSIM1_EN_MASK,
		PMIC_RG_LDO_VSIM1_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsim1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSIM1_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSIM1_EN_MASK,
		PMIC_RG_LDO_VSIM1_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsim2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSIM2_EN_ADDR,
		val,
		PMIC_RG_LDO_VSIM2_EN_MASK,
		PMIC_RG_LDO_VSIM2_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsim2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSIM2_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSIM2_EN_MASK,
		PMIC_RG_LDO_VSIM2_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vusb_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VUSB_EN_0_ADDR,
		val,
		PMIC_RG_LDO_VUSB_EN_0_MASK,
		PMIC_RG_LDO_VUSB_EN_0_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vusb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VUSB_EN_0_ADDR,
		&val,
		PMIC_RG_LDO_VUSB_EN_0_MASK,
		PMIC_RG_LDO_VUSB_EN_0_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vrfck_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VRFCK_EN_ADDR,
		val,
		PMIC_RG_LDO_VRFCK_EN_MASK,
		PMIC_RG_LDO_VRFCK_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vrfck_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VRFCK_EN_ADDR,
		&val,
		PMIC_RG_LDO_VRFCK_EN_MASK,
		PMIC_RG_LDO_VRFCK_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vbbck_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VBBCK_EN_ADDR,
		val,
		PMIC_RG_LDO_VBBCK_EN_MASK,
		PMIC_RG_LDO_VBBCK_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vbbck_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VBBCK_EN_ADDR,
		&val,
		PMIC_RG_LDO_VBBCK_EN_MASK,
		PMIC_RG_LDO_VBBCK_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vbif28_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VBIF28_EN_ADDR,
		val,
		PMIC_RG_LDO_VBIF28_EN_MASK,
		PMIC_RG_LDO_VBIF28_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vbif28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VBIF28_EN_ADDR,
		&val,
		PMIC_RG_LDO_VBIF28_EN_MASK,
		PMIC_RG_LDO_VBIF28_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vibr_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VIBR_EN_ADDR,
		val,
		PMIC_RG_LDO_VIBR_EN_MASK,
		PMIC_RG_LDO_VIBR_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vibr_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VIBR_EN_ADDR,
		&val,
		PMIC_RG_LDO_VIBR_EN_MASK,
		PMIC_RG_LDO_VIBR_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vio28_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VIO28_EN_ADDR,
		val,
		PMIC_RG_LDO_VIO28_EN_MASK,
		PMIC_RG_LDO_VIO28_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vio28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VIO28_EN_ADDR,
		&val,
		PMIC_RG_LDO_VIO28_EN_MASK,
		PMIC_RG_LDO_VIO28_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vm18_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VM18_EN_ADDR,
		val,
		PMIC_RG_LDO_VM18_EN_MASK,
		PMIC_RG_LDO_VM18_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vm18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VM18_EN_ADDR,
		&val,
		PMIC_RG_LDO_VM18_EN_MASK,
		PMIC_RG_LDO_VM18_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vufs_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VUFS_EN_ADDR,
		val,
		PMIC_RG_LDO_VUFS_EN_MASK,
		PMIC_RG_LDO_VUFS_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vufs_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VUFS_EN_ADDR,
		&val,
		PMIC_RG_LDO_VUFS_EN_MASK,
		PMIC_RG_LDO_VUFS_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_proc1_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_PROC1_EN_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_PROC1_EN_MASK,
		PMIC_RG_LDO_VSRAM_PROC1_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_proc1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_PROC1_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_PROC1_EN_MASK,
		PMIC_RG_LDO_VSRAM_PROC1_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vsram_proc1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VSRAM_PROC1_VOSEL_ADDR,
		&val,
		PMIC_DA_VSRAM_PROC1_VOSEL_MASK,
		PMIC_DA_VSRAM_PROC1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_proc2_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_PROC2_EN_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_PROC2_EN_MASK,
		PMIC_RG_LDO_VSRAM_PROC2_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_proc2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_PROC2_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_PROC2_EN_MASK,
		PMIC_RG_LDO_VSRAM_PROC2_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vsram_proc2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VSRAM_PROC2_VOSEL_ADDR,
		&val,
		PMIC_DA_VSRAM_PROC2_VOSEL_MASK,
		PMIC_DA_VSRAM_PROC2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_others_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_OTHERS_EN_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_OTHERS_EN_MASK,
		PMIC_RG_LDO_VSRAM_OTHERS_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_others_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_OTHERS_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_OTHERS_EN_MASK,
		PMIC_RG_LDO_VSRAM_OTHERS_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vsram_others_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VSRAM_OTHERS_VOSEL_ADDR,
		&val,
		PMIC_DA_VSRAM_OTHERS_VOSEL_MASK,
		PMIC_DA_VSRAM_OTHERS_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_ldo_vsram_md_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_LDO_VSRAM_MD_EN_ADDR,
		val,
		PMIC_RG_LDO_VSRAM_MD_EN_MASK,
		PMIC_RG_LDO_VSRAM_MD_EN_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_ldo_vsram_md_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_LDO_VSRAM_MD_EN_ADDR,
		&val,
		PMIC_RG_LDO_VSRAM_MD_EN_MASK,
		PMIC_RG_LDO_VSRAM_MD_EN_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_get_da_vsram_md_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_DA_VSRAM_MD_VOSEL_ADDR,
		&val,
		PMIC_DA_VSRAM_MD_VOSEL_MASK,
		PMIC_DA_VSRAM_MD_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vfe28_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VFE28_VOSEL_ADDR,
		val,
		PMIC_RG_VFE28_VOSEL_MASK,
		PMIC_RG_VFE28_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vfe28_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VFE28_VOSEL_ADDR,
		&val,
		PMIC_RG_VFE28_VOSEL_MASK,
		PMIC_RG_VFE28_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vaux18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VAUX18_VOSEL_ADDR,
		val,
		PMIC_RG_VAUX18_VOSEL_MASK,
		PMIC_RG_VAUX18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vaux18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VAUX18_VOSEL_ADDR,
		&val,
		PMIC_RG_VAUX18_VOSEL_MASK,
		PMIC_RG_VAUX18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vusb_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VUSB_VOSEL_ADDR,
		val,
		PMIC_RG_VUSB_VOSEL_MASK,
		PMIC_RG_VUSB_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vusb_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VUSB_VOSEL_ADDR,
		&val,
		PMIC_RG_VUSB_VOSEL_MASK,
		PMIC_RG_VUSB_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vbif28_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VBIF28_VOSEL_ADDR,
		val,
		PMIC_RG_VBIF28_VOSEL_MASK,
		PMIC_RG_VBIF28_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vbif28_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VBIF28_VOSEL_ADDR,
		&val,
		PMIC_RG_VBIF28_VOSEL_MASK,
		PMIC_RG_VBIF28_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcn33_1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VCN33_1_VOSEL_ADDR,
		val,
		PMIC_RG_VCN33_1_VOSEL_MASK,
		PMIC_RG_VCN33_1_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vcn33_1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VCN33_1_VOSEL_ADDR,
		&val,
		PMIC_RG_VCN33_1_VOSEL_MASK,
		PMIC_RG_VCN33_1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcn33_2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VCN33_2_VOSEL_ADDR,
		val,
		PMIC_RG_VCN33_2_VOSEL_MASK,
		PMIC_RG_VCN33_2_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vcn33_2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VCN33_2_VOSEL_ADDR,
		&val,
		PMIC_RG_VCN33_2_VOSEL_MASK,
		PMIC_RG_VCN33_2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vsim1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VSIM1_VOSEL_ADDR,
		val,
		PMIC_RG_VSIM1_VOSEL_MASK,
		PMIC_RG_VSIM1_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vsim1_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VSIM1_VOSEL_ADDR,
		&val,
		PMIC_RG_VSIM1_VOSEL_MASK,
		PMIC_RG_VSIM1_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vsim2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VSIM2_VOSEL_ADDR,
		val,
		PMIC_RG_VSIM2_VOSEL_MASK,
		PMIC_RG_VSIM2_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vsim2_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VSIM2_VOSEL_ADDR,
		&val,
		PMIC_RG_VSIM2_VOSEL_MASK,
		PMIC_RG_VSIM2_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vio28_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VIO28_VOSEL_ADDR,
		val,
		PMIC_RG_VIO28_VOSEL_MASK,
		PMIC_RG_VIO28_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vio28_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VIO28_VOSEL_ADDR,
		&val,
		PMIC_RG_VIO28_VOSEL_MASK,
		PMIC_RG_VIO28_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vibr_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VIBR_VOSEL_ADDR,
		val,
		PMIC_RG_VIBR_VOSEL_MASK,
		PMIC_RG_VIBR_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vibr_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VIBR_VOSEL_ADDR,
		&val,
		PMIC_RG_VIBR_VOSEL_MASK,
		PMIC_RG_VIBR_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_va12_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VA12_VOSEL_ADDR,
		val,
		PMIC_RG_VA12_VOSEL_MASK,
		PMIC_RG_VA12_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_va12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VA12_VOSEL_ADDR,
		&val,
		PMIC_RG_VA12_VOSEL_MASK,
		PMIC_RG_VA12_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vrf18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VRF18_VOSEL_ADDR,
		val,
		PMIC_RG_VRF18_VOSEL_MASK,
		PMIC_RG_VRF18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vrf18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VRF18_VOSEL_ADDR,
		&val,
		PMIC_RG_VRF18_VOSEL_MASK,
		PMIC_RG_VRF18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vefuse_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VEFUSE_VOSEL_ADDR,
		val,
		PMIC_RG_VEFUSE_VOSEL_MASK,
		PMIC_RG_VEFUSE_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vefuse_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VEFUSE_VOSEL_ADDR,
		&val,
		PMIC_RG_VEFUSE_VOSEL_MASK,
		PMIC_RG_VEFUSE_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcn18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VCN18_VOSEL_ADDR,
		val,
		PMIC_RG_VCN18_VOSEL_MASK,
		PMIC_RG_VCN18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vcn18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VCN18_VOSEL_ADDR,
		&val,
		PMIC_RG_VCN18_VOSEL_MASK,
		PMIC_RG_VCN18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcamio_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VCAMIO_VOSEL_ADDR,
		val,
		PMIC_RG_VCAMIO_VOSEL_MASK,
		PMIC_RG_VCAMIO_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vcamio_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VCAMIO_VOSEL_ADDR,
		&val,
		PMIC_RG_VCAMIO_VOSEL_MASK,
		PMIC_RG_VCAMIO_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vaud18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VAUD18_VOSEL_ADDR,
		val,
		PMIC_RG_VAUD18_VOSEL_MASK,
		PMIC_RG_VAUD18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vaud18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VAUD18_VOSEL_ADDR,
		&val,
		PMIC_RG_VAUD18_VOSEL_MASK,
		PMIC_RG_VAUD18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vio18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VIO18_VOSEL_ADDR,
		val,
		PMIC_RG_VIO18_VOSEL_MASK,
		PMIC_RG_VIO18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vio18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VIO18_VOSEL_ADDR,
		&val,
		PMIC_RG_VIO18_VOSEL_MASK,
		PMIC_RG_VIO18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vm18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VM18_VOSEL_ADDR,
		val,
		PMIC_RG_VM18_VOSEL_MASK,
		PMIC_RG_VM18_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vm18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VM18_VOSEL_ADDR,
		&val,
		PMIC_RG_VM18_VOSEL_MASK,
		PMIC_RG_VM18_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vufs_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VUFS_VOSEL_ADDR,
		val,
		PMIC_RG_VUFS_VOSEL_MASK,
		PMIC_RG_VUFS_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vufs_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VUFS_VOSEL_ADDR,
		&val,
		PMIC_RG_VUFS_VOSEL_MASK,
		PMIC_RG_VUFS_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vrf12_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VRF12_VOSEL_ADDR,
		val,
		PMIC_RG_VRF12_VOSEL_MASK,
		PMIC_RG_VRF12_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vrf12_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VRF12_VOSEL_ADDR,
		&val,
		PMIC_RG_VRF12_VOSEL_MASK,
		PMIC_RG_VRF12_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcn13_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VCN13_VOSEL_ADDR,
		val,
		PMIC_RG_VCN13_VOSEL_MASK,
		PMIC_RG_VCN13_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vcn13_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VCN13_VOSEL_ADDR,
		&val,
		PMIC_RG_VCN13_VOSEL_MASK,
		PMIC_RG_VCN13_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_va09_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VA09_VOSEL_ADDR,
		val,
		PMIC_RG_VA09_VOSEL_MASK,
		PMIC_RG_VA09_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_va09_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VA09_VOSEL_ADDR,
		&val,
		PMIC_RG_VA09_VOSEL_MASK,
		PMIC_RG_VA09_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vxo22_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VXO22_VOSEL_ADDR,
		val,
		PMIC_RG_VXO22_VOSEL_MASK,
		PMIC_RG_VXO22_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vxo22_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VXO22_VOSEL_ADDR,
		&val,
		PMIC_RG_VXO22_VOSEL_MASK,
		PMIC_RG_VXO22_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vrfck_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VRFCK_VOSEL_ADDR,
		val,
		PMIC_RG_VRFCK_VOSEL_MASK,
		PMIC_RG_VRFCK_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vrfck_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VRFCK_VOSEL_ADDR,
		&val,
		PMIC_RG_VRFCK_VOSEL_MASK,
		PMIC_RG_VRFCK_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vbbck_vosel(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface(
		PMIC_RG_VBBCK_VOSEL_ADDR,
		val,
		PMIC_RG_VBBCK_VOSEL_MASK,
		PMIC_RG_VBBCK_VOSEL_SHIFT);

	return ret;
}

unsigned int mt6359_upmu_get_rg_vbbck_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	ret = pmic_read_interface(
		PMIC_RG_VBBCK_VOSEL_ADDR,
		&val,
		PMIC_RG_VBBCK_VOSEL_MASK,
		PMIC_RG_VBBCK_VOSEL_SHIFT);
	if (ret)
		pr_info("%s error\n", __func__);

	return val;
}

unsigned int mt6359_upmu_set_rg_vcn33_1_bt_vosel(unsigned int val)
{
	return mt6359_upmu_set_rg_vcn33_1_vosel(val);
}

unsigned int mt6359_upmu_get_rg_vcn33_1_bt_vosel(void)
{
	return mt6359_upmu_get_rg_vcn33_1_vosel();
}

unsigned int mt6359_upmu_set_rg_vcn33_1_wifi_vosel(unsigned int val)
{
	return mt6359_upmu_set_rg_vcn33_1_vosel(val);
}

unsigned int mt6359_upmu_get_rg_vcn33_1_wifi_vosel(void)
{
	return mt6359_upmu_get_rg_vcn33_1_vosel();
}

unsigned int mt6359_upmu_set_rg_vcn33_2_bt_vosel(unsigned int val)
{
	return mt6359_upmu_set_rg_vcn33_2_vosel(val);
}

unsigned int mt6359_upmu_get_rg_vcn33_2_bt_vosel(void)
{
	return mt6359_upmu_get_rg_vcn33_2_vosel();
}

unsigned int mt6359_upmu_set_rg_vcn33_2_wifi_vosel(unsigned int val)
{
	return mt6359_upmu_set_rg_vcn33_2_vosel(val);
}

unsigned int mt6359_upmu_get_rg_vcn33_2_wifi_vosel(void)
{
	return mt6359_upmu_get_rg_vcn33_2_vosel();
}

