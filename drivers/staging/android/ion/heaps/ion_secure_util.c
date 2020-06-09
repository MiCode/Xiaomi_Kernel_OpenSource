// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/haven/hh_rm_drv.h>
#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include "ion_secure_util.h"
#include "msm_ion_priv.h"

bool is_secure_vmid_valid(int vmid)
{
	int ret;
	hh_vmid_t trusted_vm_vmid;

	ret = hh_rm_get_vmid(HH_TRUSTED_VM, &trusted_vm_vmid);

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
		(!ret && vmid == trusted_vm_vmid));
}

bool is_secure_allocation(unsigned long flags)
{
	return !!(flags & (ION_FLAGS_CP_MASK | ION_FLAG_SECURE));
}

int get_secure_vmid(unsigned long flags)
{
	int ret;
	hh_vmid_t vmid;

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
	if (flags & ION_FLAG_CP_TRUSTED_VM) {
		ret = hh_rm_get_vmid(HH_TRUSTED_VM, &vmid);
		if (!ret)
			return vmid;
		return ret;
	}
	return -EINVAL;
}

int get_ion_flags(u32 vmid)
{
	int ret;
	hh_vmid_t trusted_vm_vmid;

	if (vmid == VMID_CP_TOUCH)
		return ION_FLAG_CP_TOUCH;
	if (vmid == VMID_CP_BITSTREAM)
		return ION_FLAG_CP_BITSTREAM;
	if (vmid == VMID_CP_PIXEL)
		return ION_FLAG_CP_PIXEL;
	if (vmid == VMID_CP_NON_PIXEL)
		return ION_FLAG_CP_NON_PIXEL;
	if (vmid == VMID_CP_CAMERA)
		return ION_FLAG_CP_CAMERA;
	if (vmid == VMID_HLOS)
		return ION_FLAG_CP_HLOS;
	if (vmid == VMID_CP_SEC_DISPLAY)
		return ION_FLAG_CP_SEC_DISPLAY;
	if (vmid == VMID_CP_APP)
		return ION_FLAG_CP_APP;
	if (vmid == VMID_CP_CAMERA_PREVIEW)
		return ION_FLAG_CP_CAMERA_PREVIEW;
	if (vmid == VMID_CP_SPSS_SP)
		return ION_FLAG_CP_SPSS_SP;
	if (vmid == VMID_CP_SPSS_SP_SHARED)
		return ION_FLAG_CP_SPSS_SP_SHARED;
	if (vmid == VMID_CP_SPSS_HLOS_SHARED)
		return ION_FLAG_CP_SPSS_HLOS_SHARED;
	if (vmid == VMID_CP_CDSP)
		return ION_FLAG_CP_CDSP;

	ret = hh_rm_get_vmid(HH_TRUSTED_VM, &trusted_vm_vmid);
	if (!ret && vmid == trusted_vm_vmid)
		return ION_FLAG_CP_TRUSTED_VM;
	return -EINVAL;
}
EXPORT_SYMBOL(get_ion_flags);

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

int ion_populate_vm_list(unsigned long flags, unsigned int *vm_list,
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
EXPORT_SYMBOL(ion_populate_vm_list);

int ion_hyp_unassign_sg(struct sg_table *sgt, int *source_vm_list,
			int source_nelems, bool clear_page_private)
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

	ret = hyp_assign_table(sgt, source_vm_list, source_nelems, &dest_vmid,
			       &dest_perms, 1);
	if (ret)
		goto out;

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

	for (i = 0; i < dest_nelems; i++)
		dest_perms[i] = msm_secure_get_vmid_perms(dest_vm_list[i]);

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
	ret = ion_populate_vm_list(flags, source_vm_list, source_nelems);
	if (ret) {
		pr_err("%s: Failed to get secure vmids\n", __func__);
		goto out_free_source;
	}

	ret = ion_hyp_unassign_sg(sgt, source_vm_list, source_nelems,
				  set_page_private);

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

	ret = ion_populate_vm_list(flags, dest_vm_list, dest_nelems);
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
	struct msm_ion_buf_lock_state *lock_state = buffer->priv_virt;

	if ((buffer->flags & ION_FLAG_SECURE) &&
	    !(buffer->flags & ION_FLAG_CP_HLOS) &&
	    !(buffer->flags & ION_FLAG_CP_SPSS_HLOS_SHARED))
		return false;
	else if ((get_secure_vmid(buffer->flags) > 0) &&
		 !(buffer->flags & ION_FLAG_CP_HLOS) &&
		 !(buffer->flags & ION_FLAG_CP_SPSS_HLOS_SHARED))
		return false;
	else if (lock_state && lock_state->locked)
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
	    ion_populate_vm_list(flags, vmids, nr)) {
		pr_err("%s: Failed to parse secure flags 0x%lx\n", __func__,
		       flags);
		goto out;
	}

	for (i = 0; i < nr; i++)
		modes[i] = msm_secure_get_vmid_perms(vmids[i]);

	ret = hyp_assign_phys(base, size, &src_vm, 1, vmids, modes, nr);
	if (ret)
		pr_err("%s: Assign call failed, flags 0x%lx\n", __func__,
		       flags);

out:
	kfree(modes);
	kfree(vmids);
	return ret;
}
