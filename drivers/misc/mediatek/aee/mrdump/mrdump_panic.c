// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/kexec.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <mrdump.h>
#include <mt-plat/mboot_params.h>
#include "mrdump_mini.h"
#include "mrdump_private.h"

/* for arm_smccc_smc */
#include <linux/arm-smccc.h>
#include <uapi/linux/psci.h>

static struct pt_regs saved_regs;

/* no export symbol to aee_exception_reboot, only used in exception flow */
/* PSCI v1.1 extended power state encoding for SYSTEM_RESET2 function */
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


#if defined(CONFIG_RANDOMIZE_BASE) && defined(CONFIG_ARM64)
static inline void show_kaslr(void)
{
	u64 const kaslr_off = kaslr_offset();

	pr_notice("Kernel Offset: 0x%llx from 0x%lx\n",
			kaslr_off, KIMAGE_VADDR);
	pr_notice("PHYS_OFFSET: 0x%llx\n", PHYS_OFFSET);
	aee_rr_rec_kaslr_offset(kaslr_off);
}
#else
static inline void show_kaslr(void)
{
	pr_notice("Kernel Offset: disabled\n");
	aee_rr_rec_kaslr_offset(0xd15ab1e);
}
#endif

static char nested_panic_buf[1024];
int aee_nested_printf(const char *fmt, ...)
{
	va_list args;
	static int total_len;

	va_start(args, fmt);
	total_len += vsnprintf(nested_panic_buf, sizeof(nested_panic_buf),
			fmt, args);
	va_end(args);

	aee_sram_fiq_log(nested_panic_buf);

	return total_len;
}

static void check_last_ko(void)
{
	struct list_head *p_modules = aee_get_modules();
	struct module *mod;

	if (!p_modules)
		return;

	list_for_each_entry_rcu(mod, p_modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			break;
		load_ko_addr_list(mod);
		break;
	}
}

static void mrdump_cblock_update(enum AEE_REBOOT_MODE reboot_mode,
				 struct pt_regs *regs, const char *msg, ...)
{
	struct mrdump_crash_record *crash_record;
	void *creg;
	int cpu;
	size_t msg_count;

	local_irq_disable();

	switch (reboot_mode) {
	case AEE_REBOOT_MODE_KERNEL_OOPS:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
		break;
	case AEE_REBOOT_MODE_KERNEL_PANIC:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
		break;
	case AEE_REBOOT_MODE_HANG_DETECT:
		aee_rr_rec_exp_type(AEE_EXP_TYPE_HANG_DETECT);
		break;
	default:
		/* Don't print anything */
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
		break;
	}
	if (mrdump_cblock) {
		crash_record = &mrdump_cblock->crash_record;

		cpu = raw_smp_processor_id();
		if (cpu >= 0 && cpu < nr_cpu_ids) {
			/* null regs, no register dump */
			if (regs) {
				elf_core_copy_kernel_regs(
					(elf_gregset_t *)
					&crash_record->cpu_regs[cpu],
					regs);
			}

			creg = (void *)&crash_record->cpu_creg[cpu];
			mrdump_save_control_register(creg);
		}
		msg_count = strlen(msg);
		if (msg_count >= sizeof(crash_record->msg))
			msg_count = sizeof(crash_record->msg) - 1;
		memcpy_toio(crash_record->msg, msg, msg_count);
		__raw_writeb(0, &crash_record->msg[msg_count]);

		crash_record->fault_cpu = cpu;

		/* FIXME: Check reboot_mode is valid */
		crash_record->reboot_mode = reboot_mode;
	}
}

static int num_die;
int mrdump_common_die(int reboot_reason, const char *msg,
		      struct pt_regs *regs)
{
	int last_step;
	int next_step;

	num_die++;

	last_step = aee_rr_curr_fiq_step();
	if (num_die > 1) {
		/* NESTED KE */
		aee_reinit_die_lock();
	}
	aee_nested_printf("num_die-%d, last_step-%d\n",
			  num_die, last_step);
	/* if we were in nested ke now, then the if condition would be false */
	if (last_step < AEE_FIQ_STEP_COMMON_DIE_START)
		last_step = AEE_FIQ_STEP_COMMON_DIE_START - 1;

	/* skip the works of last_step */
	next_step = last_step + 1;

	switch (next_step) {
	case AEE_FIQ_STEP_COMMON_DIE_START:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_START);
		mrdump_cblock_update(reboot_reason, regs, msg);
		mrdump_mini_ke_cpu_regs(regs);
		check_last_ko();
	case AEE_FIQ_STEP_COMMON_DIE_LOCK:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_LOCK);
		/* release locks after stopping other cpus */
		aee_reinit_die_lock();
		aee_zap_locks();
	case AEE_FIQ_STEP_COMMON_DIE_KASLR:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_KASLR);
		show_kaslr();
	case AEE_FIQ_STEP_COMMON_DIE_SCP:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_SCP);
		aee_rr_rec_scp();
	case AEE_FIQ_STEP_COMMON_DIE_TRACE:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_TRACE);
		switch (reboot_reason) {
		case AEE_REBOOT_MODE_KERNEL_OOPS:
			aee_show_regs(regs);
			dump_stack();
			break;
		case AEE_REBOOT_MODE_KERNEL_PANIC:
#ifndef CONFIG_DEBUG_BUGVERBOSE
			dump_stack();
#endif
			break;
		default:
			/* Don't print anything */
			break;
		}
	case AEE_FIQ_STEP_COMMON_DIE_EMISC:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_EMISC);
		mrdump_mini_add_extra_misc();
	case AEE_FIQ_STEP_COMMON_DIE_CS:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_CS);
		console_unlock();
	case AEE_FIQ_STEP_COMMON_DIE_DONE:
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_COMMON_DIE_DONE);
	default:
		aee_nested_printf("num_die-%d, last_step-%d, next_step-%d\n",
				  num_die, last_step, next_step);
		aee_exception_reboot();
		break;
	}

	return NOTIFY_DONE;
}
EXPORT_SYMBOL(mrdump_common_die);

int ipanic(struct notifier_block *this, unsigned long event, void *ptr)
{
	crash_setup_regs(&saved_regs, NULL);
	return mrdump_common_die(AEE_REBOOT_MODE_KERNEL_PANIC,
				 "Kernel Panic", &saved_regs);
}

static int ipanic_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *dargs = (struct die_args *)ptr;
	return mrdump_common_die(AEE_REBOOT_MODE_KERNEL_OOPS,
				 "Kernel Oops", dargs->regs);
}

static struct notifier_block panic_blk = {
	.notifier_call = ipanic,
};

static struct notifier_block die_blk = {
	.notifier_call = ipanic_die,
};

static __init int mrdump_parse_chosen(struct mrdump_params *mparams)
{
	struct device_node *node;
	u32 reg[2];
	const char *lkver, *ddr_rsv;

	memset(mparams, 0, sizeof(struct mrdump_params));

	node = of_find_node_by_path("/chosen");
	if (node) {
		if (of_property_read_u32_array(node, "mrdump,cblock",
					       reg, ARRAY_SIZE(reg)) == 0) {
			mparams->cb_addr = reg[0];
			mparams->cb_size = reg[1];
			pr_notice("%s: mrdump_cbaddr=%x, mrdump_cbsize=%x\n",
				  __func__, mparams->cb_addr, mparams->cb_size);
		}

		if (of_property_read_string(node, "mrdump,lk", &lkver) == 0) {
			strlcpy(mparams->lk_version, lkver,
				sizeof(mparams->lk_version));
			pr_notice("%s: lk version %s\n", __func__, lkver);
		}

		if (of_property_read_string(node, "mrdump,ddr_rsv",
					    &ddr_rsv) == 0) {
			if (strcmp(ddr_rsv, "yes") == 0)
				mparams->drm_ready = true;
			pr_notice("%s: ddr reserve mode %s\n", __func__,
				  ddr_rsv);
		}

		return 0;
	}
	of_node_put(node);
	pr_notice("%s: Can't find chosen node\n", __func__);
	return -1;
}

#ifdef CONFIG_MODULES
/* Module notifier call back, update module info list */
static int mrdump_module_callback(struct notifier_block *nb,
				  unsigned long val, void *data)
{
	struct module *mod = data;

	if (val == MODULE_STATE_LIVE)
		load_ko_addr_list(mod);
	else if (val == MODULE_STATE_GOING)
		unload_ko_addr_list(mod);

	return NOTIFY_DONE;
}

static struct notifier_block mrdump_module_nb = {
	.notifier_call = mrdump_module_callback,
};
#endif

static int __init mrdump_panic_init(void)
{
	struct mrdump_params mparams = {};

#ifdef MODULE
	if (mrdump_ka_init()) {
		pr_notice("%s: mrdump helper not available\n", __func__);
		return -EINVAL;
	}
#endif
	mrdump_parse_chosen(&mparams);
#ifdef MODULE
	mrdump_module_init_mboot_params();
#endif
	mrdump_cblock_init(mparams.cb_addr, mparams.cb_size);
	if (mrdump_cblock == NULL) {
		pr_notice("%s: MT-RAMDUMP no control block\n", __func__);
		return -EINVAL;
	}
	mrdump_mini_init(&mparams);

	if (strcmp(mparams.lk_version, MRDUMP_GO_DUMP) == 0) {
		mrdump_full_init();
	} else {
		pr_notice("%s: Full ramdump disabled, version %s not matched.\n",
			  __func__, mparams.lk_version);
	}

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);
#ifdef CONFIG_MODULES
	register_module_notifier(&mrdump_module_nb);
#endif
	pr_debug("ipanic: startup\n");
	return 0;
}

arch_initcall(mrdump_panic_init);

#ifdef MODULE
static void __exit mrdump_panic_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);
	unregister_die_notifier(&die_blk);
#ifdef CONFIG_MODULES
	unregister_module_notifier(&mrdump_module_nb);
#endif
	pr_debug("ipanic: exit\n");
}
module_exit(mrdump_panic_exit);
#endif
