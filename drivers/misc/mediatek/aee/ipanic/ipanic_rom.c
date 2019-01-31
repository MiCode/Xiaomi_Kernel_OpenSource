/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/memory.h>
#include <asm/cacheflush.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <mrdump.h>
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#endif
#include <linux/reboot.h>
#include "ipanic.h"
#include <asm/system_misc.h>
#include <mmprofile.h>
#include "../mrdump/mrdump_private.h"

static bool ipanic_enable = 1;
static spinlock_t ipanic_lock;
struct ipanic_ops *ipanic_ops;
typedef int (*fn_next) (void *data, unsigned char *buffer, size_t sz_buf);

int __weak ipanic_atflog_buffer(void *data, unsigned char *buffer,
		size_t sz_buf)
{
	return 0;
}

int __weak panic_dump_android_log(char *buffer, size_t sz_buf, int type)
{
	return 0;
}

int __weak has_mt_dump_support(void)
{
	pr_notice("%s: no mt_dump support!\n", __func__);
	return 0;
}

int __weak panic_dump_disp_log(void *data, unsigned char *buffer, size_t sz_buf)
{
	pr_notice("%s: weak function\n", __func__);
	return 0;
}

void __weak sysrq_sched_debug_show_at_AEE(void)
{
	pr_notice("%s weak function at %s\n", __func__, __FILE__);
}

void __weak inner_dcache_flush_all(void)
{
	pr_notice("%s weak function at %s\n", __func__, __FILE__);
}

void __weak inner_dcache_disable(void)
{
	pr_notice("%s weak function at %s\n", __func__, __FILE__);
}

int ipanic(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct kmsg_dumper dumper;
	struct pt_regs saved_regs;

	memset(&dumper, 0x0, sizeof(struct kmsg_dumper));
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_IPANIC_START);
	aee_rr_rec_exp_type(2);
#endif
	bust_spinlocks(1);
	mrdump_mini_save_regs(&saved_regs);
	__mrdump_create_oops_dump(AEE_REBOOT_MODE_KERNEL_PANIC, &saved_regs,
			"Kernel Panic");
	spin_lock_irq(&ipanic_lock);
	aee_disable_api();
	mrdump_mini_ke_cpu_regs(NULL);
	inner_dcache_flush_all();
	aee_exception_reboot();
	return NOTIFY_DONE;
}

void ipanic_recursive_ke(struct pt_regs *regs, struct pt_regs *excp_regs,
		int cpu)
{
	struct pt_regs saved_regs;

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_exp_type(3);
#endif
	aee_nested_printf("minidump\n");
	bust_spinlocks(1);
	if (excp_regs != NULL) {
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				excp_regs, "Kernel NestedPanic");
	} else if (regs != NULL) {
		aee_nested_printf("previous excp_regs NULL\n");
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				regs, "Kernel NestedPanic");
	} else {
		aee_nested_printf("both NULL\n");
		mrdump_mini_save_regs(&saved_regs);
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				&saved_regs, "Kernel NestedPanic");
	}
	inner_dcache_flush_all();
#ifdef __aarch64__
	inner_dcache_disable();
#else
	cpu_proc_fin();
#endif
	mrdump_mini_ke_cpu_regs(excp_regs);
	mrdump_mini_per_cpu_regs(cpu, regs, current);
	inner_dcache_flush_all();
	aee_exception_reboot();
}
EXPORT_SYMBOL(ipanic_recursive_ke);

__weak void console_unlock(void)
{
	pr_notice("%s weak function\n", __func__);
}

void ipanic_zap_console_sem(void)
{
	if (console_trylock())
		pr_notice("we can get console_sem\n");
	else
		pr_notice("we cannot get console_sem\n");
	console_unlock();
}

#ifdef CONFIG_RANDOMIZE_BASE
static u64 show_kaslr(void)
{
	u64 const kaslr_offset = kimage_vaddr - KIMAGE_VADDR;

	pr_notice("Kernel Offset: 0x%llx from 0x%lx\n", kaslr_offset,
			KIMAGE_VADDR);
	return kaslr_offset;
}
#else
static u64 show_kaslr(void)
{
	pr_notice("Kernel Offset: disabled\n");
	return 0;
}
#endif

static int ipanic_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *dargs = (struct die_args *)ptr;
	u64 kaslr_offset;

	kaslr_offset = show_kaslr();
	print_modules();
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_kaslr_offset(kaslr_offset);
	aee_rr_rec_exp_type(2);
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_IPANIC_DIE);
#endif
	aee_disable_api();
	__mrdump_create_oops_dump(AEE_REBOOT_MODE_KERNEL_OOPS, dargs->regs,
			"Kernel Oops");

	__show_regs(dargs->regs);
	dump_stack();
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_scp();
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
	wq_debug_dump();
#endif

	mrdump_mini_ke_cpu_regs(dargs->regs);
	dis_D_inner_fL1L2();

#if defined(CONFIG_MTK_MLC_NAND_SUPPORT) || defined(CONFIG_MTK_TLC_NAND_SUPPORT)
	LOGE("MLC/TLC project, disable ipanic flow\n");
	ipanic_enable = 0; /*for mlc/tlc nand project, only enable lk flow*/
#endif
	ipanic_zap_console_sem();
	aee_exception_reboot();
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call = ipanic,
};

static struct notifier_block die_blk = {
	.notifier_call = ipanic_die,
};

int __init aee_ipanic_init(void)
{
	spin_lock_init(&ipanic_lock);
	mrdump_init();
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);
	LOGI("ipanic: startup, partition assgined %s\n", AEE_IPANIC_PLABEL);
	return 0;
}

arch_initcall(aee_ipanic_init);

/* 0644: S_IRUGO | S_IWUSR */
module_param(ipanic_enable, bool, 0644);
