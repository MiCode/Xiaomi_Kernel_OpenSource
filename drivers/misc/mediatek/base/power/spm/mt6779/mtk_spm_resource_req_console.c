// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/mutex.h>
#include <mtk_idle.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_lp_dts.h>

DEFINE_MUTEX(__spm_mutex);

#define IS_MTK_CONSOLE_SPM_RES_REQ(x) ({\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_SODI, PW_REG_SPM_##x##_REQ, 0);\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_SODI3, PW_REG_SPM_##x##_REQ, 0);\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_DPIDLE, PW_REG_SPM_##x##_REQ, 0);\
	SMC_CALL(GET_PWR_CTRL_ARGS,\
		SPM_PWR_CTRL_SUSPEND, PW_REG_SPM_##x##_REQ, 0);\
})

#define IDLE_TYPE_SO_PWR	(&pwrctrl_so)
#define IDLE_TYPE_SO3_PWR	(&pwrctrl_so3)
#define IDLE_TYPE_DP_PWR	(&pwrctrl_dp)
#define get_spm_pwrctl(idle_type)	idle_type##_PWR

#define get_spm_suspend_pwrctl()	(__spm_suspend.pwrctrl)

int spm_resource_req_console_by_id(int id
			, unsigned int req, unsigned int res_bitmask)
{
	int req_value;
	struct pwr_ctrl *pPwrctrl;

	mutex_lock(&__spm_mutex);

	if (id == SPM_PWR_CTRL_SODI)
		pPwrctrl = get_spm_pwrctl(IDLE_TYPE_SO);
	else if (id == SPM_PWR_CTRL_SODI3)
		pPwrctrl = get_spm_pwrctl(IDLE_TYPE_SO3);
	else if (id == SPM_PWR_CTRL_DPIDLE)
		pPwrctrl = get_spm_pwrctl(IDLE_TYPE_DP);
	else if (id == SPM_PWR_CTRL_SUSPEND)
		pPwrctrl = get_spm_suspend_pwrctl();
	else
		pPwrctrl = NULL;

	if (pPwrctrl) {
		req_value = (req == SPM_RESOURCE_CONSOLE_REQ) ? 1:0;

		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_DRAM_S0)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_REG_SPM_DDR_EN_REQ, req_value);
			pPwrctrl->reg_spm_ddr_en_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_DRAM_S1)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_REG_SPM_APSRC_REQ, req_value);
			pPwrctrl->reg_spm_apsrc_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_MAINPLL)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_REG_SPM_VRF18_REQ, req_value);
			pPwrctrl->reg_spm_vrf18_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_AXI_BUS)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_REG_SPM_INFRA_REQ, req_value);
			pPwrctrl->reg_spm_infra_req = req_value;
		}
		if (res_bitmask & _RES_MASK(MTK_SPM_RES_EX_26M)) {
			SMC_CALL(PWR_CTRL_ARGS, id
				, PW_REG_SPM_F26M_REQ, req_value);
			pPwrctrl->reg_spm_f26m_req = req_value;
		}
	}

	mutex_unlock(&__spm_mutex);
	return 0;
}

static const char *res_array[MTK_SPM_RES_EX_MAX] = {
	"resource-requested-ddren",
	"resource-requested-apsrc",
	"resource-requested-vrf18",
	"resource-requested-infra",
	"resource-requested-f26m"
};

int spm_resource_parse_req_console(struct device_node *spm_node)
{
	int i = 0, k = 0;
	u32 spm_request = 0;
	u32 spm_res_bitmask = 0;

	for (k = 0; k < MTK_SPM_RES_EX_MAX; k++) {
		i = of_property_read_u32(spm_node,
			res_array[k], &spm_request);

		if (i == 0) {
			if (spm_request)
				spm_res_bitmask |= _RES_MASK(k);
		}
	}

	of_node_put(spm_node);

	if (spm_res_bitmask)
		spm_resource_req_console(SPM_RESOURCE_CONSOLE_REQ,
			spm_res_bitmask);
	return 0;
}

int spm_resource_req_console(unsigned int req, unsigned int res_bitmask)
{
	spm_resource_req_console_by_id(SPM_PWR_CTRL_SODI, req, res_bitmask);
	spm_resource_req_console_by_id(SPM_PWR_CTRL_SODI3, req, res_bitmask);
	spm_resource_req_console_by_id(SPM_PWR_CTRL_DPIDLE, req, res_bitmask);
	spm_resource_req_console_by_id(SPM_PWR_CTRL_SUSPEND, req, res_bitmask);
	return 0;
}

int spm_get_resource_req_console_status(unsigned int *res_bitmask)
{
	unsigned int res = 0;

	if (!res_bitmask)
		return -1;

	if (IS_MTK_CONSOLE_SPM_RES_REQ(DDR_EN))
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

