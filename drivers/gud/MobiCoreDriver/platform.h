/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_PLATFORM_H_
#define _MC_PLATFORM_H_

/* MobiCore Interrupt for Qualcomm (DT IRQ has priority if present) */
#define MC_INTR_SSIQ						280

/* Use SMC for fastcalls */
#define MC_SMC_FASTCALL

#include <linux/types.h>

/*--------------- Implementation -------------- */
#if defined(CONFIG_ARCH_APQ8084) || defined(CONFIG_ARCH_MSM8916) || \
	defined(CONFIG_ARCH_MSM8994) || defined(CONFIG_ARCH_MSM8909) || \
	defined(CONFIG_ARCH_MSM8996)

#include <soc/qcom/scm.h>

#if defined(CONFIG_ARM64) || defined(CONFIG_ARCH_MSM8916)

	#include <soc/qcom/qseecomi.h>
	#include <linux/slab.h>
	#include <linux/io.h>
	#include <linux/mm.h>
	#include <asm/cacheflush.h>
	#include <linux/errno.h>

	#define SCM_MOBIOS_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) \
		| 0x33000000)

	#define TZ_EXECUTIVE_EXT_ID_PARAM_ID \
		TZ_SYSCALL_CREATE_PARAM_ID_4( \
			TZ_SYSCALL_PARAM_TYPE_BUF_RW, \
			TZ_SYSCALL_PARAM_TYPE_VAL, \
			TZ_SYSCALL_PARAM_TYPE_BUF_RW, \
			TZ_SYSCALL_PARAM_TYPE_VAL)

#endif

#else
#include <mach/scm.h>
#endif

/* from following file */
#define SCM_SVC_MOBICORE		250
#define SCM_CMD_MOBICORE		1

static inline int smc_fastcall(void *fc_generic, size_t size)
{
#if defined(CONFIG_ARCH_APQ8084) || defined(CONFIG_ARCH_MSM8916) || \
	defined(CONFIG_ARCH_MSM8994) || defined(CONFIG_ARCH_MSM8996)
	if (is_scm_armv8()) {
		struct scm_desc desc = {0};
		int ret;
		void *scm_buf = NULL;

		scm_buf = kzalloc(PAGE_ALIGN(size), GFP_KERNEL);
		if (!scm_buf)
			return -ENOMEM;
		memcpy(scm_buf, fc_generic, size);
		dmac_flush_range(scm_buf, scm_buf + size);

		desc.arginfo = TZ_EXECUTIVE_EXT_ID_PARAM_ID;
		desc.args[0] = virt_to_phys(scm_buf);
		desc.args[1] = (u32)size;
		desc.args[2] = virt_to_phys(scm_buf);
		desc.args[3] = (u32)size;

		ret = scm_call2(
			SCM_MOBIOS_FNID(SCM_SVC_MOBICORE, SCM_CMD_MOBICORE),
				&desc);

		dmac_flush_range(scm_buf, scm_buf + size);

		memcpy(fc_generic, scm_buf, size);
		kfree(scm_buf);
		return ret;
	}
#endif

	return scm_call(SCM_SVC_MOBICORE, SCM_CMD_MOBICORE,
			fc_generic, size,
			fc_generic, size);
}

/* Fastcall value should be the one for armv7, even if on armv8,
 * as long as the __aarch32__ flag is not activated in SW.
 * But for 8996, architecture is armv8 with __aarch32__ in Sw.
 */
#if !defined(CONFIG_ARCH_MSM8996)
#define MC_ARMV7_FC
#endif

#if defined(CONFIG_ARCH_MSM8996)
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif

/*
 * Perform crypto clock enable/disable
 * of clocks
 *     "bus_clk"
 *     "core_clk"
 *     "iface_clk"
 */
#if (!defined(CONFIG_ARCH_MSM8960) && !defined(CONFIG_ARCH_MSM8994)) || \
		defined(CONFIG_ARCH_MSM8996)
#define MC_CRYPTO_CLOCK_MANAGEMENT
#endif

/*
 * Perform clock enable/disable for clock  "core_clk_src"
 */
#if defined(CONFIG_ARCH_MSM8916) || defined(CONFIG_ARCH_MSM8909) || \
	defined(CONFIG_ARCH_MSM8996)
#define MC_DEVICE_PROPNAME "qcom,mcd"
#if defined(MC_CRYPTO_CLOCK_MANAGEMENT)
#define MC_CLOCK_CORESRC_PROPNAME "qcom,ce-opp-freq"
#define MC_CLOCK_CORESRC_DEFAULTRATE 100000000
#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */
#endif


#if !defined(CONFIG_ARCH_MSM8996)
/* uid/gid behave like old kernels but with new types */
/* This flag does not exist on 8996 3.10 kernel version */
#if !defined(CONFIG_UIDGID_STRICT_TYPE_CHECKS)
#define MC_UIDGID_OLDSTYLE
#endif
/* Fastcall value should be the one for armv7, even if on armv8,
 * as long as the __aarch32__ flag is not activated in SW.
 * But for 8996, architecture is armv8 with __aarch32__ in Sw.
 */
#define MC_ARMV7_FC
#endif /* not CONFIG_ARCH_MSM8996 */

#endif /* _MC_PLATFORM_H_ */

