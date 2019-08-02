/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef _TEE_KERNEL_LOWLEVEL_API_H
#define _TEE_KERNEL_LOWLEVEL_API_H

#include <linux/arm-smccc.h>

#ifdef CONFIG_ARM
struct smc_param {
	uint32_t a0;
	uint32_t a1;
	uint32_t a2;
	uint32_t a3;
	uint32_t a4;
	uint32_t a5;
	uint32_t a6;
	uint32_t a7;
};
#endif

#ifdef CONFIG_ARM64
struct smc_param {
	uint64_t a0;
	uint64_t a1;
	uint64_t a2;
	uint64_t a3;
	uint64_t a4;
	uint64_t a5;
	uint64_t a6;
	uint64_t a7;
};
#endif

#define tee_smc_call(p) do {\
		struct arm_smccc_res res; \
		arm_smccc_smc(p->a0, p->a1, p->a2, p->a3, \
			p->a4, p->a5, p->a6, p->a7, &res); \
		p->a0 = res.a0; \
		p->a1 = res.a1; \
		p->a2 = res.a2; \
		p->a3 = res.a3; \
	} while (0)

#endif
