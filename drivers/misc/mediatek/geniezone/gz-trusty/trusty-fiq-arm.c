// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */
/*
 * This is interrupt driver
 *
 * GZ does not support virtual interrupt, interrupt forwarding driver is
 * need for passing GZ and hypervisor-TEE interrupts
 */

#include <asm/fiq_glue.h>
#include <linux/platform_device.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>

#include "trusty-fiq.h"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

static void __naked trusty_fiq_return(void)
{
	asm volatile(
		".arch_extension sec\n"
		"mov	r12, r0\n"
		"ldr	r0, =" STRINGIFY(SMC_FC_FIQ_EXIT) "\n"
		"smc	#0");
}

int trusty_fiq_arch_probe(struct platform_device *pdev)
{
	return fiq_glue_set_return_handler(trusty_fiq_return);
}

void trusty_fiq_arch_remove(struct platform_device *pdev)
{
	fiq_glue_clear_return_handler(trusty_fiq_return);
}
