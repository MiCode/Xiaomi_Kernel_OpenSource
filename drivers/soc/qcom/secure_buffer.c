// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>

#define CREATE_TRACE_POINTS
#include "trace_secure_buffer.h"

#define MEM_PROT_ASSIGN_ID		0x16
#define BATCH_MAX_SIZE SZ_2M
#define BATCH_MAX_SECTIONS 32

static struct device *qcom_secure_buffer_dev;

static struct dest_vm_and_perm_info *
populate_dest_info(int *dest_vmids, int nelements, int *dest_perms,
		   size_t *size_in_bytes)
{
	struct dest_vm_and_perm_info *dest_info;
	int i;
	size_t size;

	/* Ensure allocated size is less than PAGE_ALLOC_COSTLY_ORDER */
	size = nelements * sizeof(*dest_info);
	if (size > PAGE_SIZE)
		return NULL;

	dest_info = kzalloc(size, GFP_KERNEL);
	if (!dest_info)
		return NULL;

	for (i = 0; i < nelements; i++) {
		dest_info[i].vm = dest_vmids[i];
		dest_info[i].perm = dest_perms[i];
		dest_info[i].ctx = 0x0;
		dest_info[i].ctx_size = 0;
	}

	*size_in_bytes = size;
	return dest_info;
}

static unsigned int get_batches_from_sgl(struct mem_prot_info *sg_table_copy,
					 struct scatterlist *sgl,
					 struct scatterlist **next_sgl)
{
	u64 batch_size = 0;
	unsigned int i = 0;
	struct scatterlist *curr_sgl = sgl;

	/* Ensure no zero size batches */
	do {
		sg_table_copy[i].addr = page_to_phys(sg_page(curr_sgl));
		sg_table_copy[i].size = curr_sgl->length;
		batch_size += sg_table_copy[i].size;
		curr_sgl = sg_next(curr_sgl);
		i++;
	} while (curr_sgl && i < BATCH_MAX_SECTIONS &&
		 curr_sgl->length + batch_size < BATCH_MAX_SIZE);

	*next_sgl = curr_sgl;
	return i;
}

static int batched_hyp_assign(struct sg_table *table, struct scm_desc *desc)
{
	unsigned int entries_size;
	unsigned int batch_start = 0;
	unsigned int batches_processed;
	unsigned int i = 0;
	u64 total_delta;
	struct scatterlist *curr_sgl = table->sgl;
	struct scatterlist *next_sgl;
	int ret = 0;
	ktime_t batch_assign_start_ts;
	ktime_t first_assign_ts;
	struct mem_prot_info *sg_table_copy = kcalloc(BATCH_MAX_SECTIONS,
						      sizeof(*sg_table_copy),
						      GFP_KERNEL);
	dma_addr_t entries_dma_addr;

	if (!sg_table_copy)
		return -ENOMEM;

	first_assign_ts = ktime_get();
	while (batch_start < table->nents) {
		batches_processed = get_batches_from_sgl(sg_table_copy,
							 curr_sgl, &next_sgl);
		curr_sgl = next_sgl;
		entries_size = batches_processed * sizeof(*sg_table_copy);
		entries_dma_addr = dma_map_single(qcom_secure_buffer_dev,
						  sg_table_copy, entries_size,
						  DMA_TO_DEVICE);
		if (dma_mapping_error(qcom_secure_buffer_dev,
				      entries_dma_addr)) {
			ret = -EADDRNOTAVAIL;
			break;
		}
		desc->args[0] = entries_dma_addr;
		desc->args[1] = entries_size;

		trace_hyp_assign_batch_start(sg_table_copy, batches_processed);
		batch_assign_start_ts = ktime_get();
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				MEM_PROT_ASSIGN_ID), desc);

		trace_hyp_assign_batch_end(ret, ktime_us_delta(ktime_get(),
					   batch_assign_start_ts));
		i++;

		if (ret) {
			pr_info("%s: Failed to assign memory protection, ret = %d\n",
				__func__, ret);
			/*
			 * Make it clear to clients that the memory may no
			 * longer be in a usable state.
			 */
			ret = -EADDRNOTAVAIL;
			break;
		}

		batch_start += batches_processed;
	}
	total_delta = ktime_us_delta(ktime_get(), first_assign_ts);
	trace_hyp_assign_end(total_delta, div64_u64(total_delta, i));
	kfree(sg_table_copy);
	return ret;
}

/*
 *  When -EADDRNOTAVAIL is returned the memory may no longer be in
 *  a usable state and should no longer be accessed by the HLOS.
 */
int hyp_assign_table(struct sg_table *table,
			u32 *source_vm_list, int source_nelems,
			int *dest_vmids, int *dest_perms,
			int dest_nelems)
{
	int ret = 0;
	struct scm_desc desc = {0};
	u32 *source_vm_copy;
	size_t source_vm_copy_size;
	struct dest_vm_and_perm_info *dest_vm_copy;
	size_t dest_vm_copy_size;
	dma_addr_t source_dma_addr, dest_dma_addr;

	if (!qcom_secure_buffer_dev)
		return -EPROBE_DEFER;

	if (!table || !table->sgl || !source_vm_list || !source_nelems ||
	    !dest_vmids || !dest_perms || !dest_nelems || !table->nents)
		return -EINVAL;

	/*
	 * We can only pass cache-aligned sizes to hypervisor, so we need
	 * to kmalloc and memcpy the source_vm_list here.
	 */
	source_vm_copy_size = sizeof(*source_vm_copy) * source_nelems;
	source_vm_copy = kmemdup(source_vm_list, source_vm_copy_size,
				 GFP_KERNEL);
	if (!source_vm_copy)
		return -ENOMEM;

	source_dma_addr = dma_map_single(qcom_secure_buffer_dev, source_vm_copy,
					 source_vm_copy_size, DMA_TO_DEVICE);
	if (dma_mapping_error(qcom_secure_buffer_dev, source_dma_addr)) {
		ret = -ENOMEM;
		goto out_free_source;
	}

	dest_vm_copy = populate_dest_info(dest_vmids, dest_nelems, dest_perms,
					  &dest_vm_copy_size);
	if (!dest_vm_copy) {
		ret = -ENOMEM;
		goto out_unmap_source;
	}

	dest_dma_addr = dma_map_single(qcom_secure_buffer_dev, dest_vm_copy,
				       dest_vm_copy_size, DMA_TO_DEVICE);
	if (dma_mapping_error(qcom_secure_buffer_dev, dest_dma_addr)) {
		ret = -ENOMEM;
		goto out_free_dest;
	}


	desc.args[2] = source_dma_addr;
	desc.args[3] = source_vm_copy_size;
	desc.args[4] = dest_dma_addr;
	desc.args[5] = dest_vm_copy_size;
	desc.args[6] = 0;

	desc.arginfo = SCM_ARGS(7, SCM_RO, SCM_VAL, SCM_RO, SCM_VAL, SCM_RO,
				SCM_VAL, SCM_VAL);

	trace_hyp_assign_info(source_vm_list, source_nelems, dest_vmids,
			      dest_perms, dest_nelems);

	ret = batched_hyp_assign(table, &desc);

	dma_unmap_single(qcom_secure_buffer_dev, dest_dma_addr,
			 dest_vm_copy_size, DMA_TO_DEVICE);
out_free_dest:
	kfree(dest_vm_copy);
out_unmap_source:
	dma_unmap_single(qcom_secure_buffer_dev, source_dma_addr,
			 source_vm_copy_size, DMA_TO_DEVICE);
out_free_source:
	kfree(source_vm_copy);
	return ret;
}
EXPORT_SYMBOL(hyp_assign_table);

int hyp_assign_phys(phys_addr_t addr, u64 size, u32 *source_vm_list,
			int source_nelems, int *dest_vmids,
			int *dest_perms, int dest_nelems)
{
	struct sg_table table;
	int ret;

	if (!qcom_secure_buffer_dev)
		return -EPROBE_DEFER;

	ret = sg_alloc_table(&table, 1, GFP_KERNEL);
	if (ret)
		return ret;

	sg_set_page(table.sgl, phys_to_page(addr), size, 0);

	ret = hyp_assign_table(&table, source_vm_list, source_nelems,
			       dest_vmids, dest_perms, dest_nelems);

	sg_free_table(&table);
	return ret;
}
EXPORT_SYMBOL(hyp_assign_phys);

const char *msm_secure_vmid_to_string(int secure_vmid)
{
	switch (secure_vmid) {
	case VMID_HLOS:
		return "VMID_HLOS";
	case VMID_CP_TOUCH:
		return "VMID_CP_TOUCH";
	case VMID_CP_BITSTREAM:
		return "VMID_CP_BITSTREAM";
	case VMID_CP_PIXEL:
		return "VMID_CP_PIXEL";
	case VMID_CP_NON_PIXEL:
		return "VMID_CP_NON_PIXEL";
	case VMID_CP_CAMERA:
		return "VMID_CP_CAMERA";
	case VMID_HLOS_FREE:
		return "VMID_HLOS_FREE";
	case VMID_MSS_MSA:
		return "VMID_MSS_MSA";
	case VMID_MSS_NONMSA:
		return "VMID_MSS_NONMSA";
	case VMID_CP_SEC_DISPLAY:
		return "VMID_CP_SEC_DISPLAY";
	case VMID_CP_APP:
		return "VMID_CP_APP";
	case VMID_WLAN:
		return "VMID_WLAN";
	case VMID_WLAN_CE:
		return "VMID_WLAN_CE";
	case VMID_CP_CAMERA_PREVIEW:
		return "VMID_CP_CAMERA_PREVIEW";
	case VMID_CP_SPSS_SP:
		return "VMID_CP_SPSS_SP";
	case VMID_CP_SPSS_SP_SHARED:
		return "VMID_CP_SPSS_SP_SHARED";
	case VMID_CP_SPSS_HLOS_SHARED:
		return "VMID_CP_SPSS_HLOS_SHARED";
	case VMID_INVAL:
		return "VMID_INVAL";
	default:
		return "Unknown VMID";
	}
}
EXPORT_SYMBOL(msm_secure_vmid_to_string);

u32 msm_secure_get_vmid_perms(u32 vmid)
{
	if (vmid == VMID_CP_SEC_DISPLAY)
		return PERM_READ;
	else if (vmid == VMID_CP_CDSP)
		return PERM_READ | PERM_WRITE | PERM_EXEC;
	else
		return PERM_READ | PERM_WRITE;
}
EXPORT_SYMBOL(msm_secure_get_vmid_perms);

static int qcom_secure_buffer_probe(struct platform_device *pdev)
{
	int ret;

#ifdef CONFIG_ARM64
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
#else
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
#endif
	if (!ret)
		qcom_secure_buffer_dev = &pdev->dev;

	return ret;
}

static const struct of_device_id qcom_secure_buffer_of_match[] = {
	{.compatible = "qcom,secure-buffer"},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_secure_buffer_of_match);

static struct platform_driver qcom_secure_buffer_driver = {
	.probe = qcom_secure_buffer_probe,
	.driver = {
		.name = "qcom_secure_buffer",
		.of_match_table = qcom_secure_buffer_of_match,
	},
};
module_platform_driver(qcom_secure_buffer_driver);

MODULE_LICENSE("GPL v2");
