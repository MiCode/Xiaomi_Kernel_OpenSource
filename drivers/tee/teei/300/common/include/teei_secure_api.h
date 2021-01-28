/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef _TEEI_KERN_API_
#define _TEEI_KERN_API_

#include <linux/arm-smccc.h>

#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#define ARCH_EXTENSION_SEC
#endif

#define TEEI_FC_CPU_ON			(0xb4000080)
#define TEEI_FC_CPU_OFF			(0xb4000081)
#define TEEI_FC_CPU_DORMANT		(0xb4000082)
#define TEEI_FC_CPU_DORMANT_CANCEL	(0xb4000083)
#define TEEI_FC_CPU_ERRATA_802022	(0xb4000084)

static inline long teei_secure_call(u64 function_id,
		u64 arg0, u64 arg1, u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2,
			0, 0, 0, 0, &res);

	return res.a0;
}
#endif /* _TEEI_KERN_API_ */
