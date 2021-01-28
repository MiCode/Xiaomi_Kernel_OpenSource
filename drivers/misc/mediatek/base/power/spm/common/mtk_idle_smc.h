/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_IDLE_SMC_H__
#define __MTK_IDLE_SMC_H__

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#define mtk_idle_smc_impl(p1, p2, p3, p4, p5, res) \
			arm_smccc_smc(p1, p2, p3, p4,\
			p5, 0, 0, 0, &res)


#ifndef SMC_CALL
/* SMC call's marco */
#define SMC_CALL(_name, _arg0, _arg1, _arg2) ({\
	struct arm_smccc_res res;\
	mtk_idle_smc_impl(MTK_SIP_KERNEL_SPM_##_name,\
			_arg0, _arg1, _arg2, 0, res);\
	res.a0; })

#endif

#ifndef mt_secure_call
#define mt_secure_call(x1, x2, x3, x4, x5) ({\
	struct arm_smccc_res res;\
	mtk_idle_smc_impl(x1, x2, x3, x4, x5, res);\
	res.a0; })
#endif

#endif /* __MTK_IDLE_SMC_H__ */
