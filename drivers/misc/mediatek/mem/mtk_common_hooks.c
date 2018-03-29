/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/printk.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>

int mem_fault_debug_hook(struct pt_regs *regs)
{
	void __user *pc = (void __user *)instruction_pointer(regs);
	char buf[16];
	int ret = -1;
	u32 *p;

	if (!copy_from_user(buf, pc, 16)) {
		pr_alert("pc: %p\n", pc);
		print_hex_dump(KERN_ALERT, "ddd", DUMP_PREFIX_ADDRESS, 16, 1, buf,
				16, 1);
		p = (u32 *)buf;
		if ((*(p + 2) == 0x75666475) && (*(p + 3) == 0x00006477)) {
			regs->ARM_pc += 16;
			ret = 0;
		} else if ((*(p + 1) == 0x75666475) && (*(p + 2) == 0x00006477)) {
			regs->ARM_pc += 12;
			ret = 0;
		}
	}

	return ret;
}
