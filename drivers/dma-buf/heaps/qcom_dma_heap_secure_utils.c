// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/qcom_dma_heap.h>

static int get_secure_vmid(unsigned long flags)
{
	if (flags & QCOM_DMA_HEAP_FLAG_CP_TOUCH)
		return VMID_CP_TOUCH;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_BITSTREAM)
		return VMID_CP_BITSTREAM;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_PIXEL)
		return VMID_CP_PIXEL;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_NON_PIXEL)
		return VMID_CP_NON_PIXEL;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_CAMERA)
		return VMID_CP_CAMERA;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_SEC_DISPLAY)
		return VMID_CP_SEC_DISPLAY;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_APP)
		return VMID_CP_APP;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_CAMERA_PREVIEW)
		return VMID_CP_CAMERA_PREVIEW;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_SPSS_SP)
		return VMID_CP_SPSS_SP;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_SPSS_SP_SHARED)
		return VMID_CP_SPSS_SP_SHARED;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_SPSS_HLOS_SHARED)
		return VMID_CP_SPSS_HLOS_SHARED;
	if (flags & QCOM_DMA_HEAP_FLAG_CP_CDSP)
		return VMID_CP_CDSP;

	return -EINVAL;
}

static unsigned int count_set_bits(unsigned long val)
{
	return ((unsigned int)bitmap_weight(&val, BITS_PER_LONG));
}

bool qcom_is_buffer_hlos_accessible(unsigned long flags)
{
	if (!(flags & QCOM_DMA_HEAP_FLAG_CP_HLOS) &&
	    !(flags & QCOM_DMA_HEAP_FLAG_CP_SPSS_HLOS_SHARED))
		return false;

	return true;
}

static int get_vmid(unsigned long flags)
{
	int vmid;

	vmid = get_secure_vmid(flags);
	if (vmid < 0) {
		if (flags & QCOM_DMA_HEAP_FLAG_CP_HLOS)
			vmid = VMID_HLOS;
	}
	return vmid;
}

static int populate_vm_list(unsigned long flags, unsigned int *vm_list,
			 int nelems)
{
	unsigned int itr = 0;
	int vmid;

	flags = flags & QCOM_DMA_HEAP_FLAGS_CP_MASK;
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

int hyp_assign_from_flags(u64 base, u64 size, unsigned long flags)
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

	if ((flags & ~QCOM_DMA_HEAP_FLAGS_CP_MASK) ||
	    populate_vm_list(flags, vmids, nr)) {
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
