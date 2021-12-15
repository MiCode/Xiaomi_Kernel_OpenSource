/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <helio-dvfsrc_v2.h>
#include <helio-dvfsrc-opp.h>
#include <helio-dvfsrc-ipi.h>

static struct opp_profile opp_table[VCORE_DVFS_OPP_NUM];
static int vcore_dvfs_to_vcore_opp[VCORE_DVFS_OPP_NUM];
static int vcore_dvfs_to_ddr_opp[VCORE_DVFS_OPP_NUM];
static int vcore_uv_table[VCORE_OPP_NUM];
static int vcore_opp_to_pwrap_cmd[VCORE_OPP_NUM];
static int ddr_table[DDR_OPP_NUM];


/* ToDo: Copy Opp Table to AEE Dump */
int get_cur_vcore_dvfs_opp(void)
{
#if defined(VCOREFS_LEVEL_POSITIVE)
	return __builtin_ffs(spm_get_dvfs_level());
#else
	return VCORE_DVFS_OPP_NUM - __builtin_ffs(get_dvfs_final_level());
#endif
}

void set_opp_table(unsigned int vcore_dvfs_opp, int vcore_uv, int ddr_khz)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_OPP_TABLE;
	qos_d.u.opp_table.vcore_dvfs_opp = vcore_dvfs_opp;
	qos_d.u.opp_table.vcore_uv = vcore_uv;
	qos_d.u.opp_table.ddr_khz = ddr_khz;
	qos_ipi_to_sspm_command(&qos_d, 4);
#endif

	if (vcore_dvfs_opp >= VCORE_DVFS_OPP_NUM)
		return;

	opp_table[vcore_dvfs_opp].vcore_uv = vcore_uv;
	opp_table[vcore_dvfs_opp].ddr_khz = ddr_khz;
}

void set_vcore_opp(unsigned int vcore_dvfs_opp, int vcore_opp)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_VCORE_OPP;
	qos_d.u.vcore_opp.vcore_dvfs_opp = vcore_dvfs_opp;
	qos_d.u.vcore_opp.vcore_opp = vcore_opp;
	qos_ipi_to_sspm_command(&qos_d, 3);
#endif

	if (vcore_dvfs_opp >= VCORE_DVFS_OPP_NUM)
		return;

	vcore_dvfs_to_vcore_opp[vcore_dvfs_opp] = vcore_opp;
}

int get_vcore_opp(unsigned int opp)
{
	if (opp >= VCORE_DVFS_OPP_NUM)
		return 0;

	return vcore_dvfs_to_vcore_opp[opp];
}

int get_vcore_uv(unsigned int opp)
{
	if (opp >= VCORE_DVFS_OPP_NUM)
		return 0;

	return opp_table[opp].vcore_uv;
}

int get_cur_vcore_opp(void)
{
	unsigned int idx;

	if (!is_qos_enabled())
		return VCORE_OPP_UNREQ;

	idx = get_cur_vcore_dvfs_opp();

	if (idx >= VCORE_DVFS_OPP_NUM)
		return VCORE_OPP_UNREQ;

	return vcore_dvfs_to_vcore_opp[idx];
}

int get_cur_vcore_uv(void)
{
	unsigned int idx;

	if (!is_qos_enabled())
		return 0;

	idx = get_cur_vcore_dvfs_opp();

	if (idx >= VCORE_DVFS_OPP_NUM)
		return 0;

	return opp_table[idx].vcore_uv;
}

void set_ddr_opp(unsigned int vcore_dvfs_opp, int ddr_opp)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	struct qos_ipi_data qos_d;

	qos_d.cmd = QOS_IPI_DDR_OPP;
	qos_d.u.ddr_opp.vcore_dvfs_opp = vcore_dvfs_opp;
	qos_d.u.ddr_opp.ddr_opp = ddr_opp;
	qos_ipi_to_sspm_command(&qos_d, 3);
#endif
	if (vcore_dvfs_opp >= VCORE_DVFS_OPP_NUM)
		return;

	vcore_dvfs_to_ddr_opp[vcore_dvfs_opp] = ddr_opp;
}

int get_ddr_opp(unsigned int opp)
{
	if (opp >= VCORE_DVFS_OPP_NUM)
		return 0;

	return vcore_dvfs_to_ddr_opp[opp];
}

int get_ddr_khz(unsigned int opp)
{
	if (opp >= VCORE_DVFS_OPP_NUM)
		return 0;

	return opp_table[opp].ddr_khz;
}

int get_cur_ddr_opp(void)
{
	unsigned int idx;

	if (!is_qos_enabled())
		return DDR_OPP_UNREQ;

	idx = get_cur_vcore_dvfs_opp();

	if (idx >= VCORE_DVFS_OPP_NUM)
		return DDR_OPP_UNREQ;

	return vcore_dvfs_to_ddr_opp[idx];
}

int get_cur_ddr_khz(void)
{
	unsigned int idx;

	if (!is_qos_enabled())
		return 0;

	idx = get_cur_vcore_dvfs_opp();

	if (idx >= VCORE_DVFS_OPP_NUM)
		return 0;

	return opp_table[idx].ddr_khz;
}

void set_vcore_uv_table(unsigned int vcore_opp, int vcore_uv)
{
	if (vcore_opp >= VCORE_OPP_NUM)
		return;

	spm_dvfs_pwrap_cmd(get_pwrap_cmd(vcore_opp),
			vcore_uv_to_pmic(vcore_uv));

	vcore_uv_table[vcore_opp] = vcore_uv;
}

int get_opp_ddr_freq(unsigned int ddr_opp)
{
	if (ddr_opp >= DDR_OPP_NUM)
		return 0;

	return ddr_table[ddr_opp];
}

void set_opp_ddr_freq(unsigned int ddr_opp, int ddr_freq)
{
	if (ddr_opp >= DDR_OPP_NUM)
		return;

	ddr_table[ddr_opp] = ddr_freq;
}

int get_vcore_uv_table(unsigned int vcore_opp)
{
	if (vcore_opp >= VCORE_OPP_NUM)
		return 0;

	return vcore_uv_table[vcore_opp];
}

void set_pwrap_cmd(unsigned int vcore_opp, int pwrap_cmd)
{
	if (vcore_opp >= VCORE_OPP_NUM)
		return;

	vcore_opp_to_pwrap_cmd[vcore_opp] = pwrap_cmd;
}

int get_pwrap_cmd(unsigned int vcore_opp)
{
	if (vcore_opp >= VCORE_OPP_NUM)
		return 0;

	return vcore_opp_to_pwrap_cmd[vcore_opp];
}
