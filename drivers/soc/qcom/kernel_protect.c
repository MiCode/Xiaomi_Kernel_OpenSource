/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/printk.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <soc/qcom/secure_buffer.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>


#ifdef CONFIG_MSM_KERNEL_PROTECT_TEST

/*
 * We're going to crash the system so we need to make sure debug messages
 * in the main msm_protect_kernel initcall make it to the serial console.
 */
#undef pr_debug
#define pr_debug pr_err

/* tests protection by trying to hijack  __alloc_pages_nodemask */
static void msm_protect_kernel_test(void)
{
	/*
	 * There's nothing special about __alloc_pages_nodemask, we just
	 * need something that lives in the regulator (non-init) kernel
	 * text section that we know will never be compiled out.
	 */
	char *addr = (char *)__alloc_pages_nodemask;

	pr_err("Checking whether the kernel text is writable...\n");
	pr_err("A BUG means it is writable (this is bad)\n");
	pr_err("A stage-2 fault means it's not writable (this is good, but we'll still crash)\n");
	/*
	 * We can't simply do a `*addr = 0' since the kernel text might be
	 * read-only in stage-1.  We have to ensure the address is writable
	 * in stage-1 first, otherwise we'll just get a stage-1 fault and
	 * we'll never know if our stage-2 protection is actually working.
	 */
	if (set_memory_rw(round_down((u64)addr, PAGE_SIZE), 1)) {
		pr_err("Couldn't set memory as RW.  Can't perform check!\n");
		return;
	}
	pr_err("Writing now...\n");
	*addr = 0;
	pr_err("If we're still alive right now then kernel protection did NOT work.\n");
	BUG();
}

#else

static void msm_protect_kernel_test(void)
{
}

#endif

static int __init msm_protect_kernel(void)
{
	int ret;
	u32 vmid_hlos = VMID_HLOS;
	int dest_perms = PERM_READ | PERM_EXEC;
	/*
	 * Although the kernel image is mapped with section mappings, the
	 * start and end of the .text segment are on a PAGE_SIZE
	 * boundaries.
	 */
	phys_addr_t kernel_x_start_rounded = round_down(__pa(_stext),
							PAGE_SIZE);
	phys_addr_t kernel_x_end = round_up(__pa(_etext), PAGE_SIZE);
	void *virt_start = phys_to_virt(kernel_x_start_rounded);
	void *virt_end = phys_to_virt(kernel_x_end);

	pr_debug("assigning from phys: %pa to %pa\n",
		 &kernel_x_start_rounded, &kernel_x_end);
	pr_debug("virtual: %p to %p\n", virt_start, virt_end);
	ret = hyp_assign_phys(kernel_x_start_rounded,
			      kernel_x_end - kernel_x_start_rounded,
			      &vmid_hlos, 1, &vmid_hlos, &dest_perms, 1);
	if (ret)
		/*
		 * We want to fail relatively silently since not all
		 * platforms support the hyp_assign_phys call.
		 */
		pr_debug("Couldn't protect the kernel region: %d\n", ret);

	msm_protect_kernel_test();

	return ret;
}

/*
 * The assign call only works if it happens before we go into SMP mode.  It
 * needs to be an early_initcall so that it happens before we bring the
 * other cores out of reset.
 */
early_initcall(msm_protect_kernel);
