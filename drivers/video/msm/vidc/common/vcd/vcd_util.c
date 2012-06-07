/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include "vidc_type.h"
#include "vcd_util.h"

u32 vcd_critical_section_create(u32 **p_cs)
{
	struct mutex *lock;
	if (!p_cs) {
		VCD_MSG_ERROR("Bad critical section ptr");
		return VCD_ERR_BAD_POINTER;
	} else {
		lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
		if (!lock) {
			VCD_MSG_ERROR("Failed: vcd_critical_section_create");
			return VCD_ERR_ALLOC_FAIL;
		}
		mutex_init(lock);
		*p_cs = (u32 *) lock;
		return VCD_S_SUCCESS;
	}
}

u32 vcd_critical_section_release(u32 *cs)
{
	struct mutex *lock = (struct mutex *)cs;
	if (!lock) {
		VCD_MSG_ERROR("Bad critical section object");
		return VCD_ERR_BAD_POINTER;
	}

	mutex_destroy(lock);
	kfree(cs);
	return VCD_S_SUCCESS;
}

u32 vcd_critical_section_enter(u32 *cs)
{
	struct mutex *lock = (struct mutex *)cs;
	if (!lock) {
		VCD_MSG_ERROR("Bad critical section object");
		return VCD_ERR_BAD_POINTER;
	} else
		mutex_lock(lock);

	return VCD_S_SUCCESS;
}

u32 vcd_critical_section_leave(u32 *cs)
{
	struct mutex *lock = (struct mutex *)cs;

	if (!lock) {
		VCD_MSG_ERROR("Bad critical section object");

		return VCD_ERR_BAD_POINTER;
	} else
		mutex_unlock(lock);

	return VCD_S_SUCCESS;
}

int vcd_pmem_alloc(u32 size, u8 **kernel_vaddr, u8 **phy_addr)
{
	*phy_addr =
	    (u8 *) pmem_kalloc(size, PMEM_MEMTYPE | PMEM_ALIGNMENT_4K);

	if (!IS_ERR((void *)*phy_addr)) {

		*kernel_vaddr = ioremap((unsigned long)*phy_addr, size);

		if (!*kernel_vaddr) {
			pr_err("%s: could not ioremap in kernel pmem buffers\n",
			       __func__);
			pmem_kfree((s32) *phy_addr);
			return -ENOMEM;
		}
		pr_debug("write buf: phy addr 0x%08x kernel addr 0x%08x\n",
			 (u32) *phy_addr, (u32) *kernel_vaddr);
		return 0;
	} else {
		pr_err("%s: could not allocte in kernel pmem buffers\n",
		       __func__);
		return -ENOMEM;
	}

}

int vcd_pmem_free(u8 *kernel_vaddr, u8 *phy_addr)
{
	iounmap((void *)kernel_vaddr);
	pmem_kfree((s32) phy_addr);

	return 0;
}
