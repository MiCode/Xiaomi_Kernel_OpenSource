/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __SSC_MODULE_H__
#define __SSC_MODULE_H__

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#define ssc_smc_impl(p1, p2, p3, p4, p5, res) \
			arm_smccc_smc(p1, p2, p3, p4\
			, p5, 0, 0, 0, &res)

#define ssc_smc(_funcid, _lp_id, _act, _val1, _val2) ({\
	struct arm_smccc_res res;\
	ssc_smc_impl(_funcid, _lp_id, _act, _val1\
					, _val2, res);\
	res.a0; })

enum MT_SSC_TIMEOUT_ID {
	SSC_ISP_TIMEOUT = 0,
	SSC_VCORE_TIMEOUT,
	SSC_APU_TIMEOUT,
	SSC_GPU_TIMEOUT,
	SSC_TIMEOUT_NUM,
};


#endif
