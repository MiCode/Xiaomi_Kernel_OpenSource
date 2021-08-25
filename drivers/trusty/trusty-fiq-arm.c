/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/fiq_glue.h>
#include <linux/platform_device.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

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
