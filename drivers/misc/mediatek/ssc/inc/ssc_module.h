/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __SSC_MODULE_H__
#define __SSC_MODULE_H__

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/kobject.h>

extern struct kobject *ssc_kobj;

#define ssc_smc_impl(p1, p2, p3, p4, p5, res) \
			arm_smccc_smc(p1, p2, p3, p4\
			, p5, 0, 0, 0, &res)

#define ssc_smc(_action_id, _act, _val1, _val2) ({\
	struct arm_smccc_res res;\
	ssc_smc_impl(MTK_SIP_MTK_SSC_CONTROL,\
		_action_id, _act, _val1, _val2, res);\
	res.a0; })

enum MT_SSC_TIMEOUT_ID {
	SSC_ISP_TIMEOUT = 0,
	SSC_VCORE_TIMEOUT,
	SSC_APU_TIMEOUT,
	SSC_GPU_TIMEOUT,
	SSC_TIMEOUT_NUM,
};

/* SCMI return status */
enum MT_SSC_STATUS {
	SSC_STATUS_OK = 0,
	SSC_STATUS_ERR,
};

enum MT_SSC_SMC_TYPE {
	SSC_INIT = 0,
	SSC_MAX_TIMEOUT,
	SSC_SW_REQ,
	SSC_REGISTER_ACCESS,
};

enum MT_SSC_SMC_ACTION {
	SSC_ACT_READ = 0,
	SSC_ACT_WRITE,
};

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

#endif
