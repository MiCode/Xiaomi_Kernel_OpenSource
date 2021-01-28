// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <asm/system_misc.h>
#include <asm/kexec.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <mrdump.h>
#include <linux/reboot.h>
#include "mrdump_private.h"
#include "mrdump_mini.h"
#include <mt-plat/mboot_params.h>

/* for arm_smccc_smc */
#include <linux/arm-smccc.h>
#include <uapi/linux/psci.h>

__weak void console_unlock(void)
{
	pr_notice("%s weak function\n", __func__);
}

static inline unsigned long get_linear_memory_size(void)
{
	return (unsigned long)high_memory - PAGE_OFFSET;
}

/* no export symbol to aee_exception_reboot, only used in exception flow */
/* PSCI v1.1 extended power state encoding for SYSTEM_RESET2 function */
#define PSCI_1_1_FN_SYSTEM_RESET2       0x84000012
#define PSCI_1_1_RESET2_TYPE_VENDOR_SHIFT   31
#define PSCI_1_1_RESET2_TYPE_VENDOR     \
	(1 << PSCI_1_1_RESET2_TYPE_VENDOR_SHIFT)

static void aee_exception_reboot(void)
{
	struct arm_smccc_res res;
	int opt1 = 1, opt2 = 0;

	arm_smccc_smc(PSCI_1_1_FN_SYSTEM_RESET2,
		PSCI_1_1_RESET2_TYPE_VENDOR | opt1,
		opt2, 0, 0, 0, 0, 0, &res);
}

/*save stack as binary into buf,
 *return value

    -1: bottom unaligned
    -2: bottom out of kernel addr space
    -3 top out of kernel addr addr
    -4: buff len not enough
    >0: used length of the buf
 */
int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
		unsigned long top)
{
	/*should check stack address in kernel range */
	if (bottom & 3)
		return -1;
	if (!((bottom >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (bottom <= (PAGE_OFFSET + get_linear_memory_size())))) {
		if (!((bottom >= VMALLOC_START) && (bottom <= VMALLOC_END)))
			return -2;
	}

	if (!((top >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (top <= (PAGE_OFFSET + get_linear_memory_size())))) {
		if (!((top >= VMALLOC_START) && (top <= VMALLOC_END)))
			return -3;
	}

	if (top > ALIGN(bottom, THREAD_SIZE))
		top = ALIGN(bottom, THREAD_SIZE);

	if (buf_len < top - bottom)
		return -4;

	memcpy((void *)buf, (void *)bottom, top - bottom);

	return top - bottom;
}

int mrdump_common_die(int fiq_step, int reboot_reason, const char *msg,
		      struct pt_regs *regs)
{
	bust_spinlocks(1);
	aee_disable_api();

	show_kaslr();
	print_modules();
	aee_rr_rec_fiq_step(fiq_step);
	aee_rr_rec_scp();
	__mrdump_create_oops_dump(reboot_reason, regs, msg);

	switch (reboot_reason) {
	case AEE_REBOOT_MODE_KERNEL_OOPS:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
		__show_regs(regs);
		dump_stack();
		break;
	case AEE_REBOOT_MODE_KERNEL_PANIC:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
#ifndef CONFIG_DEBUG_BUGVERBOSE
		dump_stack();
#endif
		break;
	case AEE_REBOOT_MODE_HANG_DETECT:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_HANG_DETECT);
		break;
	default:
		/* Don't print anything */
		break;
	}
	mrdump_mini_ke_cpu_regs(regs);
	console_unlock();
	dis_D_inner_flush_all();
	aee_exception_reboot();
	return NOTIFY_DONE;
}

int ipanic(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct pt_regs saved_regs;
	int fiq_step;

	fiq_step = AEE_FIQ_STEP_KE_IPANIC_START;
	crash_setup_regs(&saved_regs, NULL);
	return mrdump_common_die(fiq_step,
				 AEE_REBOOT_MODE_KERNEL_PANIC,
				 "Kernel Panic", &saved_regs);
}

static int ipanic_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *dargs = (struct die_args *)ptr;
	int fiq_step;

	fiq_step = AEE_FIQ_STEP_KE_IPANIC_DIE;
	return mrdump_common_die(fiq_step,
				 AEE_REBOOT_MODE_KERNEL_OOPS,
				 "Kernel Oops", dargs->regs);
}

static struct notifier_block panic_blk = {
	.notifier_call = ipanic,
};

static struct notifier_block die_blk = {
	.notifier_call = ipanic_die,
};

static int __init mrdump_panic_init(void)
{
	mrdump_hw_init();
	mrdump_cblock_init();
	mrdump_mini_init();

	mrdump_full_init();
	mrdump_wdt_init();

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);
	pr_debug("ipanic: startup\n");
	return 0;
}

arch_initcall(mrdump_panic_init);
