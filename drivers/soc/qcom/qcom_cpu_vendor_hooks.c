// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "VendorHooks: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/sched/debug.h>
#include <linux/io.h>

#include <soc/qcom/watchdog.h>

#include <trace/hooks/debug.h>
#include <trace/hooks/printk.h>
#include <trace/hooks/timer.h>
#include <trace/hooks/traps.h>

static DEFINE_PER_CPU(struct pt_regs, regs_before_stop);
static DEFINE_RAW_SPINLOCK(stop_lock);

static void printk_hotplug(void *unused, int *flag)
{
	*flag = 1;
}

static void trace_ipi_stop(void *unused, struct pt_regs *regs)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;

	per_cpu(regs_before_stop, cpu) = *regs;
	raw_spin_lock_irqsave(&stop_lock, flags);
	pr_crit("CPU%u: stopping\n", cpu);
	show_regs(regs);
	raw_spin_unlock_irqrestore(&stop_lock, flags);
}

static void timer_recalc_index(void *unused,
			unsigned int lvl, unsigned long *expires)
{
	*expires -= 1;
}

/* In line with aarch64_insn_read from arch/arm64/kernel/insn.c */
static int instruction_read(void *addr, u32 *insnp)
{
	int ret;
	__le32 val;

	ret = copy_from_kernel_nofault(&val, addr, AARCH64_INSN_SIZE);
	if (!ret)
		*insnp = le32_to_cpu(val);

	return ret;
}

/* In line with dump_kernel_instr from arch/arm64/kernel/traps.c */
static void dump_instr(const char *rname, u64 instr)
{
	char str[sizeof("00000000 ") * 5 + 2 + 1], *p = str;
	int i;

	for (i = -4; i < 1; i++) {
		unsigned int val, bad;

		bad = instruction_read(&((u32 *)instr)[i], &val);

		if (!bad)
			p += scnprintf(p, sizeof(str) - (p - str),
								i == 0 ? "(%08x) " : "%08x ", val);
		else {
			p += scnprintf(p, sizeof(str) - (p - str), "bad value");
			break;
		}
	}

	printk(KERN_EMERG "%s Code: %s\n", rname, str);
}

/* In line with __show_regs from arch/arm64/kernel/process.c */
void show_regs_min(struct pt_regs *regs)
{
	int i = 29;

	printk(KERN_EMERG "pc : %016llx\n", regs->pc);
	printk(KERN_EMERG "lr : %016llx\n", regs->regs[30]);
	printk(KERN_EMERG "sp : %016llx\n", regs->sp);

	while (i >= 0) {
		printk(KERN_EMERG "x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			pr_cont("x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		pr_cont("\n");
	}
}

static void print_undefinstr(void *unused,
			struct pt_regs *regs, bool user)
{
	if (!user) {
		dump_instr("PC", regs->pc);
		dump_instr("LR", ptrauth_strip_insn_pac(regs->regs[30]));
		show_regs_min(regs);
	}
}

static void print_ptrauth_fault(void *unused, struct pt_regs *regs,
			unsigned int esr, bool user)
{
	if (!user) {
		dump_instr("PC", regs->pc);
		dump_instr("LR", ptrauth_strip_insn_pac(regs->regs[30]));
		printk(KERN_EMERG "ESR value: 0x%08x", esr);
		show_regs_min(regs);
	}
}

#if IS_ENABLED(CONFIG_DEBUG_SPINLOCK) && \
   (IS_ENABLED(CONFIG_DEBUG_SPINLOCK_BITE_ON_BUG) || IS_ENABLED(CONFIG_DEBUG_SPINLOCK_PANIC_ON_BUG))
static int entry_spin_bug(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	raw_spinlock_t *lock = (raw_spinlock_t *)regs->regs[0];
	const char *msg = (const char *)regs->regs[1];
	struct task_struct *owner = READ_ONCE(lock->owner);

	if (!debug_locks_off())
		return 0;

	/* Dup of spin_bug in kernel/locking/spinlock_debug.c */
	if (owner == SPINLOCK_OWNER_INIT)
		owner = NULL;
	printk(KERN_EMERG "BUG: spinlock %s on CPU#%d, %s/%d\n",
		msg, raw_smp_processor_id(),
		current->comm, task_pid_nr(current));
	printk(KERN_EMERG " lock: %pS, .magic: %08x, .owner: %s/%d, "
			".owner_cpu: %d\n",
		lock, READ_ONCE(lock->magic),
		owner ? owner->comm : "<none>",
		owner ? task_pid_nr(owner) : -1,
		READ_ONCE(lock->owner_cpu));

#if IS_ENABLED(CONFIG_DEBUG_SPINLOCK_BITE_ON_BUG)
	qcom_wdt_trigger_bite();
#elif IS_ENABLED(CONFIG_DEBUG_SPINLOCK_PANIC_ON_BUG)
	BUG();
#else
# error "Neither CONFIG_DEBUG_SPINLOCK_BITE_ON_BUG nor CONFIG_DEBUG_SPINLOCK_PANIC_ON_BUG is enabled yet trying to enable spin_bug hook"
#endif
	return 0;
}

struct kretprobe spin_bug_probe = {
	.entry_handler = entry_spin_bug,
	.maxactive = 1,
	.kp.symbol_name = "spin_bug",
};

static void register_spinlock_bug_hook(struct platform_device *pdev)
{
	int ret;

	ret = register_kretprobe(&spin_bug_probe);
	if (ret)
		dev_err(&pdev->dev, "Failed to register spin_bug_probe: %x\n", ret);
}
#else
static inline void register_spinlock_bug_hook(struct platform_device *pdev) { }
#endif

#ifdef CONFIG_RANDOMIZE_BASE
#define KASLR_OFFSET_MASK	0x00000000FFFFFFFF
static void __iomem *map_prop_mem(const char *propname)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, propname);
	void __iomem *addr;

	if (!np) {
		pr_err("Unable to find DT property: %s\n", propname);
		return NULL;
	}

	addr = of_iomap(np, 0);
	if (!addr)
		pr_err("Unable to map memory for DT property: %s\n", propname);
	return addr;
}

static void store_kaslr_offset(void)
{
	void __iomem *mem = map_prop_mem("qcom,msm-imem-kaslr_offset");

	if (!mem)
		return;

	__raw_writel(0xdead4ead, mem);
	__raw_writel((kimage_vaddr - KIMAGE_VADDR) & KASLR_OFFSET_MASK,
		     mem + 4);
	__raw_writel(((kimage_vaddr - KIMAGE_VADDR) >> 32) & KASLR_OFFSET_MASK,
		     mem + 8);

	iounmap(mem);
}
#else
static void store_kaslr_offset(void) {}
#endif /* CONFIG_RANDOMIZE_BASE */

static int cpu_vendor_hooks_driver_probe(struct platform_device *pdev)
{
	int ret;

	store_kaslr_offset();

	ret = register_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register android_vh_ipi_stop hook\n");
		return ret;
	}

	ret = register_trace_android_vh_printk_hotplug(printk_hotplug, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to android_vh_printk_hotplug hook\n");
		unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
		return ret;
	}

	ret = register_trace_android_vh_timer_calc_index(timer_recalc_index, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to android_vh_timer_calc_index hook\n");
		unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
		unregister_trace_android_vh_printk_hotplug(printk_hotplug, NULL);
		return ret;
	}

	ret = register_trace_android_rvh_do_undefinstr(print_undefinstr, NULL);
	if (ret)
		dev_err(&pdev->dev, "Failed to android_rvh_do_undefinstr hook\n");

	ret = register_trace_android_rvh_do_ptrauth_fault(print_ptrauth_fault, NULL);
	if (ret)
		dev_err(&pdev->dev, "Failed to android_rvh_do_ptrauth_fault hook\n");

	register_spinlock_bug_hook(pdev);

	return ret;
}

static int cpu_vendor_hooks_driver_remove(struct platform_device *pdev)
{
	/* Reset all initialized global variables and unregister callbacks. */
	unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	unregister_trace_android_vh_printk_hotplug(printk_hotplug, NULL);
	unregister_trace_android_vh_timer_calc_index(timer_recalc_index, NULL);
	return 0;
}

static const struct of_device_id cpu_vendor_hooks_of_match[] = {
	{ .compatible = "qcom,cpu-vendor-hooks" },
	{ }
};
MODULE_DEVICE_TABLE(of, cpu_vendor_hooks_of_match);

static struct platform_driver cpu_vendor_hooks_driver = {
	.driver = {
		.name = "qcom-cpu-vendor-hooks",
		.of_match_table = cpu_vendor_hooks_of_match,
	},
	.probe = cpu_vendor_hooks_driver_probe,
	.remove = cpu_vendor_hooks_driver_remove,
};

static int __init qcom_vendor_hook_driver_init(void)
{
	return platform_driver_register(&cpu_vendor_hooks_driver);
}
#if IS_MODULE(CONFIG_QCOM_CPU_VENDOR_HOOKS)
module_init(qcom_vendor_hook_driver_init);
#else
pure_initcall(qcom_vendor_hook_driver_init);
#endif

static void __exit qcom_vendor_hook_driver_exit(void)
{
	return platform_driver_unregister(&cpu_vendor_hooks_driver);
}
module_exit(qcom_vendor_hook_driver_exit);

MODULE_DESCRIPTION("QCOM CPU Vendor Hooks Driver");
MODULE_LICENSE("GPL v2");
