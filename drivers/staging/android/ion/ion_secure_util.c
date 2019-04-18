/*
 *
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include "ion_secure_util.h"
#include "ion.h"

bool is_secure_vmid_valid(int vmid)
{
	return (vmid == VMID_CP_TOUCH ||
		vmid == VMID_CP_BITSTREAM ||
		vmid == VMID_CP_PIXEL ||
		vmid == VMID_CP_NON_PIXEL ||
		vmid == VMID_CP_CAMERA ||
		vmid == VMID_CP_SEC_DISPLAY ||
		vmid == VMID_CP_APP ||
		vmid == VMID_CP_CAMERA_PREVIEW ||
		vmid == VMID_CP_SPSS_SP ||
		vmid == VMID_CP_SPSS_SP_SHARED ||
		vmid == VMID_CP_SPSS_HLOS_SHARED ||
		vmid == VMID_CP_CDSP ||
		vmid == VMID_CP_DSP_EXT);
}

int get_secure_vmid(unsigned long flags)
{
	if (flags & ION_FLAG_CP_TOUCH)
		return VMID_CP_TOUCH;
	if (flags & ION_FLAG_CP_BITSTREAM)
		return VMID_CP_BITSTREAM;
	if (flags & ION_FLAG_CP_PIXEL)
		return VMID_CP_PIXEL;
	if (flags & ION_FLAG_CP_NON_PIXEL)
		return VMID_CP_NON_PIXEL;
	if (flags & ION_FLAG_CP_CAMERA)
		return VMID_CP_CAMERA;
	if (flags & ION_FLAG_CP_SEC_DISPLAY)
		return VMID_CP_SEC_DISPLAY;
	if (flags & ION_FLAG_CP_APP)
		return VMID_CP_APP;
	if (flags & ION_FLAG_CP_CAMERA_PREVIEW)
		return VMID_CP_CAMERA_PREVIEW;
	if (flags & ION_FLAG_CP_SPSS_SP)
		return VMID_CP_SPSS_SP;
	if (flags & ION_FLAG_CP_SPSS_SP_SHARED)
		return VMID_CP_SPSS_SP_SHARED;
	if (flags & ION_FLAG_CP_SPSS_HLOS_SHARED)
		return VMID_CP_SPSS_HLOS_SHARED;
	if (flags & ION_FLAG_CP_CDSP)
		return VMID_CP_CDSP;
	if (flags & ION_FLAG_CP_DSP_EXT)
		return VMID_CP_DSP_EXT;
	return -EINVAL;
}

static unsigned int count_set_bits(unsigned long val)
{
	return ((unsigned int)bitmap_weight(&val, BITS_PER_LONG));
}

static int get_vmid(unsigned long flags)
{
	int vmid;

	vmid = get_secure_vmid(flags);
	if (vmid < 0) {
		if (flags & ION_FLAG_CP_HLOS)
			vmid = VMID_HLOS;
	}
	return vmid;
}

static int populate_vm_list(unsigned long flags, unsigned int *vm_list,
			    int nelems)
{
	unsigned int itr = 0;
	int vmid;

	flags = flags & ION_FLAGS_CP_MASK;
	if (!flags)
		return -EINVAL;

	for_each_set_bit(itr, &flags, BITS_PER_LONG) {
		vmid = get_vmid(0x1UL << itr);
		if (vmid < 0 || !nelems)
			return -EINVAL;

		vm_list[nelems - 1] = vmid;
		nelems--;
	}
	return 0;
}

int ion_hyp_unassign_sg(struct sg_table *sgt, int *source_vm_list,
			int source_nelems, bool clear_page_private,
			bool try_lock)
{
	u32 dest_vmid = VMID_HLOS;
	u32 dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	struct scatterlist *sg;
	int ret, i;

	if (source_nelems <= 0) {
		pr_err("%s: source_nelems invalid\n",
		       __func__);
		ret = -EINVAL;
		goto out;
	}

	if (try_lock)
		ret = try_hyp_assign_table(sgt, source_vm_list, source_nelems,
					   &dest_vmid, &dest_perms, 1);
	else
		ret = hyp_assign_table(sgt, source_vm_list, source_nelems,
				       &dest_vmid, &dest_perms, 1);
	if (ret) {
		if (!try_lock)
			pr_err("%s: Unassign call failed.\n",
			       __func__);
		goto out;
	}
	if (clear_page_private)
		for_each_sg(sgt->sgl, sg, sgt->nents, i)
			ClearPagePrivate(sg_page(sg));
out:
	return ret;
}

int ion_hyp_assign_sg(struct sg_table *sgt, int *dest_vm_list,
		      int dest_nelems, bool set_page_private)
{
	u32 source_vmid = VMID_HLOS;
	struct scatterlist *sg;
	int *dest_perms;
	int i;
	int ret = 0;

	if (dest_nelems <= 0) {
		pr_err("%s: dest_nelems invalid\n",
		       __func__);
		ret = -EINVAL;
		goto out;
	}

	dest_perms = kcalloc(dest_nelems, sizeof(*dest_perms), GFP_KERNEL);
	if (!dest_perms) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < dest_nelems; i++) {
		if (dest_vm_list[i] == VMID_CP_SEC_DISPLAY ||
		    dest_vm_list[i] == VMID_CP_DSP_EXT)
			dest_perms[i] = PERM_READ;
		else
			dest_perms[i] = PERM_READ | PERM_WRITE;
	}

	ret = hyp_assign_table(sgt, &source_vmid, 1,
			       dest_vm_list, dest_perms, dest_nelems);

	if (ret) {
		pr_err("%s: Assign call failed\n",
		       __func__);
		goto out_free_dest;
	}
	if (set_page_private)
		for_each_sg(sgt->sgl, sg, sgt->nents, i)
			SetPagePrivate(sg_page(sg));

out_free_dest:
	kfree(dest_perms);
out:
	return ret;
}

int ion_hyp_unassign_sg_from_flags(struct sg_table *sgt, unsigned long flags,
				   bool set_page_private)
{
	int ret = 0;
	int *source_vm_list;
	int source_nelems;

	source_nelems = count_set_bits(flags & ION_FLAGS_CP_MASK);
	source_vm_list = kcalloc(source_nelems, sizeof(*source_vm_list),
				 GFP_KERNEL);
	if (!source_vm_list)
		return -ENOMEM;
	ret = populate_vm_list(flags, source_vm_list, source_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmids\n", __func__);
		goto out_free_source;
	}

	ret = ion_hyp_unassign_sg(sgt, source_vm_list, source_nelems,
				  set_page_private, false);

out_free_source:
	kfree(source_vm_list);
	return ret;
}

int ion_hyp_assign_sg_from_flags(struct sg_table *sgt, unsigned long flags,
				 bool set_page_private)
{
	int ret = 0;
	int *dest_vm_list = NULL;
	int dest_nelems;

	dest_nelems = count_set_bits(flags & ION_FLAGS_CP_MASK);
	dest_vm_list = kcalloc(dest_nelems, sizeof(*dest_vm_list), GFP_KERNEL);
	if (!dest_vm_list) {
		ret = -ENOMEM;
		goto out;
	}

	ret = populate_vm_list(flags, dest_vm_list, dest_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmid(s)\n", __func__);
		goto out_free_dest_vm;
	}

	ret = ion_hyp_assign_sg(sgt, dest_vm_list, dest_nelems,
				set_page_private);

out_free_dest_vm:
	kfree(dest_vm_list);
out:
	return ret;
}

bool hlos_accessible_buffer(struct ion_buffer *buffer)
{
	if ((buffer->flags & ION_FLAG_SECURE) &&
	    !(buffer->flags & ION_FLAG_CP_HLOS) &&
	    !(buffer->flags & ION_FLAG_CP_SPSS_HLOS_SHARED))
		return false;
	else if ((get_secure_vmid(buffer->flags) > 0) &&
		 !(buffer->flags & ION_FLAG_CP_HLOS) &&
		 !(buffer->flags & ION_FLAG_CP_SPSS_HLOS_SHARED))
		return false;

	return true;
}

int ion_hyp_assign_from_flags(u64 base, u64 size, unsigned long flags)
{
	u32 *vmids, *modes;
	u32 nr, i;
	int ret = -EINVAL;
	u32 src_vm = VMID_HLOS;

	nr = count_set_bits(flags);
	vmids = kcalloc(nr, sizeof(*vmids), GFP_KERNEL);
	if (!vmids)
		return -ENOMEM;

	modes = kcalloc(nr, sizeof(*modes), GFP_KERNEL);
	if (!modes) {
		kfree(vmids);
		return -ENOMEM;
	}

	if ((flags & ~ION_FLAGS_CP_MASK) ||
	    populate_vm_list(flags, vmids, nr)) {
		pr_err("%s: Failed to parse secure flags 0x%lx\n", __func__,
		       flags);
		goto out;
	}

	for (i = 0; i < nr; i++)
		if (vmids[i] == VMID_CP_SEC_DISPLAY ||
		    vmids[i] == VMID_CP_DSP_EXT)
			modes[i] = PERM_READ;
		else
			modes[i] = PERM_READ | PERM_WRITE;

	ret = hyp_assign_phys(base, size, &src_vm, 1, vmids, modes, nr);
	if (ret) {
		pr_err("%s: Assign call failed, flags 0x%lx\n", __func__,
		       flags);
	}

out:
	kfree(modes);
	kfree(vmids);
	return ret;
}
