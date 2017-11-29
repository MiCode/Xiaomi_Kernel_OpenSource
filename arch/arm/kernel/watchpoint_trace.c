/*
 * watchpoint_trace.c - HW Breakpoint file to watch kernel data address
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * usage:
 *	Find the symbol address from System.map, which you want to monitor.
 *	(Eg: c116b8d8 D watchdog_thresh from obj/KERNEL_OBJ/System.map )
 *	And then issue below command:
 *	mount -t debugfs debug /sys/kernel/debug
 *	echo 0xc116b8d8 > /sys/kernel/debug/watchpoint
 *
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * Author: Yang Dongdong <yangdongdong@xiaomi.com>
 */
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/kallsyms.h>
#include <linux/sched.h>	/* Needed for show_regs */
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <asm/hw_breakpoint.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>	/* Needed for probe_kernel_address */
#include <linux/workqueue.h>
#include <linux/param.h>	/* Needed for HZ */

struct perf_event * __percpu *wp_event;
static unsigned long wp_addr;
module_param_named(addr, wp_addr, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(addr, "Kernel address to monitor; this module will report any"
		" write operations on the kernel memory address,"
		" such as symbol address");
static void watchpoint_register(struct work_struct *work);
static void watchpoint_unregister(struct work_struct *work);
static DECLARE_WORK(unreg_work, watchpoint_unregister);
static DECLARE_DELAYED_WORK(reg_work, watchpoint_register);

/*
 * dump a block of kernel memory from around the given address
 */
static void show_data(unsigned long addr, int nbytes, const char *name)
{
	int	i, j;
	int	nlines;
	u32	*p;

	/*
	 * don't attempt to dump non-kernel addresses, values that are probably
	 * just small negative numbers, or vmalloc addresses that may point to
	 * memory-mapped peripherals
	 */
	if (addr < PAGE_OFFSET || addr > -256UL ||
		is_vmalloc_addr((void *)addr))
	return;

	printk("\n%s: %#lx:\n", name, addr);

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
		printk("%04lx ", (unsigned long)p & 0xffff);
		for (j = 0; j < 8; j++) {
			u32	data;
			/*
			 * vmalloc addresses may point to
			 * memory-mapped peripherals
			 */
			if (is_vmalloc_addr(p) ||
			    probe_kernel_address(p, data)) {
				printk(" ********");
			} else {
				printk(" %08x", data);
			}
			++p;
		}
		printk("\n");
	}
}

static void watchpoint_handler(struct perf_event *bp,
		 struct perf_sample_data *data,
		struct pt_regs *regs)
{
	char name[16];

	on_each_cpu(reset_bp_ctrl_regs, NULL, 1);
	schedule_work(&unreg_work);
	if (unlikely(!regs))
		return;

	printk(KERN_INFO "watchpoint 0x%llx value is changed\n", bp->attr.bp_addr);
	memset(name, 0x0, sizeof(name));
	snprintf(name, sizeof(name), "0x%llx", bp->attr.bp_addr);
	show_data((bp->attr.bp_addr - 128), 256, name);
	dump_stack();

	printk(KERN_INFO "Dump stack from watchpoint_handler\n");
	schedule_delayed_work(&reg_work, 1*HZ);

	return;
}

static void watchpoint_register(struct work_struct *work)
{
	struct perf_event_attr attr;
	int ret = 0;

	hw_breakpoint_init(&attr);
	attr.bp_addr = wp_addr;
	attr.bp_len = HW_BREAKPOINT_LEN_4;
	attr.bp_type = HW_BREAKPOINT_W;
	attr.exclude_kernel = 0;

	wp_event = register_wide_hw_breakpoint(&attr, watchpoint_handler, NULL);
	if (IS_ERR((void __force *)wp_event)) {
		ret = PTR_ERR((void __force *)wp_event);
		goto fail;
	}
	printk(KERN_INFO "HW Breakpoint for 0x%llx write installed\n", attr.bp_addr);

	return;

fail:
	printk(KERN_INFO "Breakpoint registration failed ret=%d\n", ret);

	return;
}

static void watchpoint_unregister(struct work_struct *work)
{
	if (!wp_event)
		return;

	if (IS_ERR((void __force *)wp_event)) {
		printk(KERN_INFO "Jump out, watchpoint unregister failure, %ld\n",
		PTR_ERR((void __force *)wp_event));
		return;
	}
	unregister_wide_hw_breakpoint(wp_event);
	wp_event = NULL;
	return;
}

static int watchpoint_get(void  *data, u64 *val)
{
	*val = wp_addr;
	printk(KERN_INFO "HW Watchpoint at 0x%lx\n", wp_addr);

	return 0;
}

static int watchpoint_set(void  *data, u64 val)
{
	unsigned long addr = (unsigned long)val;
	u32 __data;

	/*
	 * vmalloc addresses may point to
	 * memory-mapped peripherals
	 */
	if (is_vmalloc_addr((u32 *)addr) ||
	    probe_kernel_address((u32 *)addr, __data)) {
		printk(KERN_INFO "Can not safely set watchpoint at 0x%lx\n", addr);
	} else {
		printk(KERN_INFO "0x%lx = 0x%08x\n", addr, __data);
	}
	watchpoint_unregister(NULL);
	wp_addr = addr;
	watchpoint_register(NULL);
	printk(KERN_INFO "Trigger watchpoint at 0x%lx\n", wp_addr);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(watchpoint_fops, watchpoint_get,
	watchpoint_set, "%llu\n");

static int __init hw_break_module_init(void)
{
	unsigned long addr = (unsigned long)wp_addr;
	u32 __data;

	/*
	 * vmalloc addresses may point to
	 * memory-mapped peripherals
	 */
	if (is_vmalloc_addr((u32 *)addr) ||
	    probe_kernel_address((u32 *)addr, __data)) {
		printk(KERN_INFO "Can not safely set watchpoint at 0x%lx\n", addr);
	} else {
		printk(KERN_INFO "0x%lx = 0x%08x\n", addr, __data);
	}
	schedule_delayed_work(&reg_work, 10*HZ);
	debugfs_create_file("watchpoint",
			0644, NULL, NULL, &watchpoint_fops);

	return 0;
}

static void __exit hw_break_module_exit(void)
{
	watchpoint_unregister(NULL);
	printk(KERN_INFO "HW Breakpoint for 0x%lx write uninstalled\n", wp_addr);
}

module_init(hw_break_module_init);
module_exit(hw_break_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YangDongdong");
MODULE_DESCRIPTION("watchpoint trace");

