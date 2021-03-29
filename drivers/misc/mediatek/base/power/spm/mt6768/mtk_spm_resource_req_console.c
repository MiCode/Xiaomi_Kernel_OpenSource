/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/mutex.h>
#include <mtk_idle.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_resource_req_console.h>
#include <mtk_idle_module_plat.h>

DEFINE_MUTEX(__spm_mutex);

#define IS_MTK_CONSOLE_SPM_RES_REQ(x) ({\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_IDLE_DRAM, PW_SPM_##x##_REQ, 0) |\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_IDLE_SYSPLL, PW_SPM_##x##_REQ, 0) |\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_IDLE_BUS26M, PW_SPM_##x##_REQ, 0) |\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_SUSPEND, PW_SPM_##x##_REQ, 0);\
})

#define IDLE_MODEL_BUS26M_PWR	(&pwrctrl_bus26m)
#define IDLE_MODEL_SYSPLL_PWR	(&pwrctrl_syspll)
#define IDLE_MODEL_DRAM_PWR		(&pwrctrl_dram)

#define get_spm_pwrctl(idle_type)	idle_type##_PWR
#define get_spm_suspend_pwrctl()	(__spm_suspend.pwrctrl)

int spm_resource_req_console_deliver(struct pwr_ctrl *pwr, int id
		, unsigned int req, unsigned int res_bitmask)
{
	int req_value;

	if (pwr) {
		mutex_lock(&__spm_mutex);
		req_value = (req == SPM_RESOURCE_CONSOLE_REQ) ? 1:0;

		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_DRAM_S0)) {
			SMC_CALL(PWR_CTRL_ARGS, id
					, PW_SPM_DDREN_REQ, req_value);
			pwr->spm_ddren_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_DRAM_S1)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_SPM_APSRC_REQ, req_value);
			pwr->spm_apsrc_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_MAINPLL)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_SPM_VRF18_REQ, req_value);
			pwr->spm_vrf18_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_AXI_BUS)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_SPM_INFRA_REQ, req_value);
			pwr->spm_infra_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_26M)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_SPM_F26M_REQ, req_value);
			pwr->spm_f26m_req = req_value;
		}
		mutex_unlock(&__spm_mutex);
	}

	return 0;
}

int spm_resource_req_console_by_id(int id
			, unsigned int req, unsigned int res_bitmask)
{
	int bRet = 0;
	struct pwr_ctrl *pwr;

	if (id == SPM_PWR_CTRL_IDLE_DRAM)
		pwr = get_spm_pwrctl(IDLE_MODEL_DRAM);
	else if (id == SPM_PWR_CTRL_IDLE_SYSPLL)
		pwr = get_spm_pwrctl(IDLE_MODEL_SYSPLL);
	else if (id == SPM_PWR_CTRL_IDLE_BUS26M)
		pwr = get_spm_pwrctl(IDLE_MODEL_BUS26M);
	else if (id == SPM_PWR_CTRL_SUSPEND)
		pwr = get_spm_suspend_pwrctl();
	else
		pwr = NULL;

	if (pwr)
		bRet = spm_resource_req_console_deliver(
				pwr, id, req, res_bitmask);

	return bRet;
}

/* spm_resource_req_console use to support external service
 * to control spm resource such as DoE
 */
int spm_resource_req_console(unsigned int req, unsigned int res_bitmask)
{
	struct pwr_ctrl *pwr;

	pwr = get_spm_pwrctl(IDLE_MODEL_DRAM);
	spm_resource_req_console_deliver(pwr,
			SPM_PWR_CTRL_IDLE_DRAM, req, res_bitmask);
	pwr = get_spm_pwrctl(IDLE_MODEL_BUS26M);
	spm_resource_req_console_deliver(pwr,
			SPM_PWR_CTRL_IDLE_BUS26M, req, res_bitmask);
	pwr = get_spm_pwrctl(IDLE_MODEL_SYSPLL);
	spm_resource_req_console_deliver(pwr,
			SPM_PWR_CTRL_IDLE_SYSPLL, req, res_bitmask);

	pwr = get_spm_suspend_pwrctl();
	spm_resource_req_console_deliver(pwr,
			SPM_PWR_CTRL_SUSPEND, req, res_bitmask);
	return 0;
}

int spm_get_resource_req_console_status(unsigned int *res_bitmask)
{
	unsigned int res = 0;

	if (!res_bitmask)
		return -1;

	if (IS_MTK_CONSOLE_SPM_RES_REQ(DDREN))
		res |= _RES_MASK(MTK_SPM_RES_EX_DRAM_S0);

	if (IS_MTK_CONSOLE_SPM_RES_REQ(APSRC))
		res |= _RES_MASK(MTK_SPM_RES_EX_DRAM_S1);

	if (IS_MTK_CONSOLE_SPM_RES_REQ(VRF18))
		res |= _RES_MASK(MTK_SPM_RES_EX_MAINPLL);

	if (IS_MTK_CONSOLE_SPM_RES_REQ(INFRA))
		res |= _RES_MASK(MTK_SPM_RES_EX_AXI_BUS);

	if (IS_MTK_CONSOLE_SPM_RES_REQ(F26M))
		res |= _RES_MASK(MTK_SPM_RES_EX_26M);

	*res_bitmask = res;

	return 0;
}

static int __init spm_resource_console_init(void)
{
	unsigned int dts_require = 0;

	struct spm_resource_console_req
		plat_res_console_sup[MTK_SPM_RES_EX_MAX] = {
		{"resource-requested-ddren", MTK_SPM_RES_EX_DRAM_S0},
		{"resource-requested-apsrc", MTK_SPM_RES_EX_DRAM_S1},
		{"resource-requested-vrf18", MTK_SPM_RES_EX_MAINPLL},
		{"resource-requested-infra", MTK_SPM_RES_EX_AXI_BUS},
		{"resource-requested-f26m", MTK_SPM_RES_EX_26M}
	};

	dts_require = spm_resource_console_dts_required(
			plat_res_console_sup, MTK_SPM_RES_EX_MAX);

	spm_resource_req_console(SPM_RESOURCE_CONSOLE_REQ, dts_require);

	return 0;
}
late_initcall(spm_resource_console_init);

