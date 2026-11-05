// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Took is_el1_instruction_abort() from arch/arm64/mm/fault.c
 * Copyright (C) 2012 ARM Ltd
 */

#include <linux/module.h>
#include <trace/hooks/fault.h>
#include <asm/esr.h>
#include <asm/ptrace.h>

static bool is_el1_instruction_abort(unsigned long esr)
{
	return ESR_ELx_EC(esr) == ESR_ELx_EC_IABT_CUR;
}

static void can_fixup_sea(void *unused, unsigned long addr, unsigned long esr,
			  struct pt_regs *regs, bool *can_fixup)
{
	if (!user_mode(regs) && !is_el1_instruction_abort(esr))
		*can_fixup = true;
	else
		*can_fixup = false;
}

static int __init init_mem_hooks(void)
{
	int ret;

	ret = register_trace_android_vh_try_fixup_sea(can_fixup_sea, NULL);
	if (ret) {
		pr_err("Failed to register try_fixup_sea\n");
		return ret;
	}

	return 0;
}

module_init(init_mem_hooks);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Memory Trace Hook Call-Back Registration");
MODULE_LICENSE("GPL");
