// SPDX-License-Identifier: GPL-2.0
#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#ifdef CONFIG_ARM
/*
 * dump a block of kernel memory from around the given address
 */
static void
show_data(unsigned long addr, int nbytes, const char *name, unsigned int cpu)
{
	int	i, j;
	int	nlines;
	u32	*p;
	char reg_line[79];

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	if (is_vmalloc_addr((void *) addr))
		return;

	pr_info("\n");
	pr_info("C%u %s: %#lx:\n", cpu, name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		sprintf(reg_line, "%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;

			if (copy_from_kernel_nofault(&data, p, sizeof(data)))
				sprintf(reg_line + 5 + j * 9, " ********");
			else
				sprintf(reg_line + 5 + j * 9, " %08x", data);
			++p;
		}
		reg_line[77] = '\n';
		reg_line[78] = '\0';
		pr_info("C%u %s", cpu, reg_line);
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	unsigned int cpu = smp_processor_id();

	show_data(regs->ARM_pc - nbytes, nbytes * 2, "PC", cpu);
	show_data(regs->ARM_lr - nbytes, nbytes * 2, "LR", cpu);
	show_data(regs->ARM_sp - nbytes, nbytes * 2, "SP", cpu);
	show_data(regs->ARM_ip - nbytes, nbytes * 2, "IP", cpu);
	show_data(regs->ARM_fp - nbytes, nbytes * 2, "FP", cpu);
	show_data(regs->ARM_r0 - nbytes, nbytes * 2, "R0", cpu);
	show_data(regs->ARM_r1 - nbytes, nbytes * 2, "R1", cpu);
	show_data(regs->ARM_r2 - nbytes, nbytes * 2, "R2", cpu);
	show_data(regs->ARM_r3 - nbytes, nbytes * 2, "R3", cpu);
	show_data(regs->ARM_r4 - nbytes, nbytes * 2, "R4", cpu);
	show_data(regs->ARM_r5 - nbytes, nbytes * 2, "R5", cpu);
	show_data(regs->ARM_r6 - nbytes, nbytes * 2, "R6", cpu);
	show_data(regs->ARM_r7 - nbytes, nbytes * 2, "R7", cpu);
	show_data(regs->ARM_r8 - nbytes, nbytes * 2, "R8", cpu);
	show_data(regs->ARM_r9 - nbytes, nbytes * 2, "R9", cpu);
	show_data(regs->ARM_r10 - nbytes, nbytes * 2, "R10", cpu);
}
#endif

#ifdef CONFIG_ARM64
/*
 * dump a block of kernel memory from around the given address
 */
static void
show_data(unsigned long addr, int nbytes, const char *name, unsigned int cpu)
{
	int	i, j;
	int	nlines;
	u32	*p;
	char reg_line[78];

	/*
	 * don't attempt to dump non-kernel addresses or
	 * values that are probably just small negative numbers
	 */
	if (addr < PAGE_OFFSET || addr > -256UL)
		return;

	if (is_vmalloc_addr((void *) addr))
		return;

	pr_info("\n");
	pr_info("C%u %s: %#lx:\n", cpu, name, addr);

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		sprintf(reg_line, "%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;

			if (copy_from_kernel_nofault(&data, p, sizeof(data)))
				sprintf(reg_line + 5 + j * 9, " ********");
			else
				sprintf(reg_line + 5 + j * 9, " %08x", data);
			++p;
		}
		reg_line[77] = '\0';
		pr_info("C%u %s\n", cpu, reg_line);
	}
}

static void show_extra_register_data(struct pt_regs *regs, int nbytes)
{
	unsigned int i;
	unsigned int cpu = smp_processor_id();

	show_data(regs->pc - nbytes, nbytes * 2, "PC", cpu);
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR", cpu);
	show_data(regs->sp - nbytes, nbytes * 2, "SP", cpu);
	for (i = 0; i < 30; i++) {
		char name[4];

		snprintf(name, sizeof(name), "X%u", i);
		show_data(regs->regs[i] - nbytes, nbytes * 2, name, cpu);
	}
}
#endif

static int __sprd_cache_print_notify(struct die_args *args, unsigned long cmd)
{
	struct pt_regs *regs = args->regs;
	int nbytes = 128;

	pr_info("---Die flow print cache!---\n");
	show_extra_register_data(regs, nbytes);

	return NOTIFY_DONE;
}

static int
sprd_cache_print_notify(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __sprd_cache_print_notify(ptr, cmd);
	local_irq_restore(flags);

	return ret;
}

static struct notifier_block sprd_cache_print_nb = {
	.notifier_call = sprd_cache_print_notify,
	.priority = 0x7ffffff0
};

static __init int sprd_cache_print_init(void)
{
	int ret;

	ret = register_die_notifier(&sprd_cache_print_nb);

	pr_info("Cache print init success!\n");
	return ret;
}

static __exit void sprd_cache_print_exit(void)
{
	unregister_die_notifier(&sprd_cache_print_nb);

	pr_info("Cache print exit\n");
}

module_init(sprd_cache_print_init);
module_exit(sprd_cache_print_exit);

MODULE_AUTHOR("ziwei.dai <ziwei.dai@unisoc.com>");
MODULE_DESCRIPTION("unisoc cache print of crash context");
MODULE_LICENSE("GPL");
