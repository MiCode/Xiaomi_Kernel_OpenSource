// SPDX-License-Identifier: GPL-2.0-only
/* copyright (C) 2023 Unisoc (Shanghai) Technologies Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <asm/traps.h>

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
		snprintf(reg_line, 6, "%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;

			if (copy_from_kernel_nofault(&data, p, sizeof(data)))
				snprintf(reg_line + 5 + j * 9, 10, " ********");
			else
				snprintf(reg_line + 5 + j * 9, 10, " %08x", data);
			++p;
		}
		reg_line[77] = '\n';
		reg_line[78] = '\0';
		pr_info("C%u %s", cpu, reg_line);
	}
}

static int __sprd_cache_print(struct pt_regs *regs, u32 instr)
{
	int nbytes = 128;
	unsigned int cpu = smp_processor_id();

	pr_info("---Print cache for undef exception---\n");
	pr_info("instr value:%x\n", instr);
	show_data(regs->pc - nbytes, nbytes * 2, "PC", cpu);
	show_data(regs->regs[30] - nbytes, nbytes * 2, "LR", cpu);

	return 1;
}

static int sprd_cache_print(struct pt_regs *regs, u32 instr)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __sprd_cache_print(regs, instr);
	local_irq_restore(flags);

	return ret;
}

static int sprd_undef_handler(struct pt_regs *regs, u32 instr)
{
	if (user_mode(regs))
		return 1;

	return sprd_cache_print(regs, instr);
}

static struct undef_hook sprd_print_cache_hook = {
	.instr_mask	= 0x0,
	.instr_val	= 0x0,
	.pstate_mask	= 0x0,
	.pstate_val = 0x0,
	.fn		= sprd_undef_handler,
};

static __init int sprd_undef_hook_init(void)
{
	register_undef_hook(&sprd_print_cache_hook);

	pr_info("Sprd undef hook register!\n");
	return 0;
}

static __exit void sprd_undef_hook_exit(void)
{
	unregister_undef_hook(&sprd_print_cache_hook);

	pr_info("Sprd undef hook unregister!\n");
}

module_init(sprd_undef_hook_init);
module_exit(sprd_undef_hook_exit);

MODULE_AUTHOR("ziwei.dai <ziwei.dai@unisoc.com>");
MODULE_DESCRIPTION("SPRD print cache when undefined instruction exception occurs");
MODULE_LICENSE("GPL");
