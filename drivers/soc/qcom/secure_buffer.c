// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
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
#include <linux/qcom_scm.h>
#include <soc/qcom/secure_buffer.h>

#define CREATE_TRACE_POINTS
#include "trace_secure_buffer.h"

#define BATCH_MAX_SIZE SZ_2M
#define BATCH_MAX_SECTIONS 32

static struct device *qcom_secure_buffer_dev;
static bool vmid_cp_camera_preview_ro;

static struct qcom_scm_current_perm_info *
populate_dest_info(int *dest_vmids, int nelements, int *dest_perms,
		   size_t *size_in_bytes)
{
	struct qcom_scm_current_perm_info *dest_info;
	int i;
	size_t size;

	/* Ensure allocated size is less than PAGE_ALLOC_COSTLY_ORDER */
	size = nelements * sizeof(*dest_info);
	if (size > PAGE_SIZE)
		return NULL;

	dest_info = kzalloc(size, GFP_KERNEL);
	if (!dest_info)
		return NULL;

	for (i = 0; i < nelements; i++)
		qcom_scm_populate_vmperm_info(&dest_info[i], dest_vmids[i],
					      dest_perms[i]);

	*size_in_bytes = size;
	return dest_info;
}

static unsigned int get_batches_from_sgl(struct qcom_scm_mem_map_info *sgt_copy,
					 struct scatterlist *sgl,
					 struct scatterlist **next_sgl)
{
	u64 batch_size = 0;
	unsigned int i = 0;
	struct scatterlist *curr_sgl = sgl;

	/* Ensure no zero size batches */
	do {
		qcom_scm_populate_mem_map_info(&sgt_copy[i],
					       page_to_phys(sg_page(curr_sgl)),
					       curr_sgl->length);
		batch_size += curr_sgl->length;
		curr_sgl = sg_next(curr_sgl);
		i++;
	} while (curr_sgl && i < BATCH_MAX_SECTIONS &&
		 curr_sgl->length + batch_size < BATCH_MAX_SIZE);

	*next_sgl = curr_sgl;
	return i;
}

static int batched_hyp_assign(struct sg_table *table, u32 *source_vmids,
			      size_t source_size,
			      struct qcom_scm_current_perm_info *destvms,
			      size_t destvms_size)
{
	unsigned int batch_start = 0;
	unsigned int batches_processed;
	unsigned int i = 0;
	u64 total_delta;
	struct scatterlist *curr_sgl = table->sgl;
	struct scatterlist *next_sgl;
	int ret = 0;
	ktime_t batch_assign_start_ts;
	ktime_t first_assign_ts;
	struct qcom_scm_mem_map_info *mem_regions_buf =
		kcalloc(BATCH_MAX_SECTIONS, sizeof(*mem_regions_buf),
			GFP_KERNEL);
	dma_addr_t entries_dma_addr;
	size_t mem_regions_buf_size;

	if (!mem_regions_buf)
		return -ENOMEM;

	first_assign_ts = ktime_get();
	while (batch_start < table->nents) {
		batches_processed = get_batches_from_sgl(mem_regions_buf,
							 curr_sgl, &next_sgl);
		curr_sgl = next_sgl;
		mem_regions_buf_size = batches_processed *
				       sizeof(*mem_regions_buf);
		entries_dma_addr = dma_map_single(qcom_secure_buffer_dev,
						  mem_regions_buf,
						  mem_regions_buf_size,
						  DMA_TO_DEVICE);
		if (dma_mapping_error(qcom_secure_buffer_dev,
				      entries_dma_addr)) {
			ret = -EADDRNOTAVAIL;
			break;
		}

		trace_hyp_assign_batch_start(mem_regions_buf,
					     batches_processed);
		batch_assign_start_ts = ktime_get();
		ret = qcom_scm_assign_mem_regions(mem_regions_buf,
						  mem_regions_buf_size,
						  source_vmids, source_size,
						  destvms, destvms_size);

		trace_hyp_assign_batch_end(ret, ktime_us_delta(ktime_get(),
					   batch_assign_start_ts));
		dma_unmap_single(qcom_secure_buffer_dev, entries_dma_addr,
				 mem_regions_buf_size, DMA_TO_DEVICE);
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
	kfree(mem_regions_buf);
	return ret;
}

static inline void set_each_page_of_sg(struct sg_table *table, u64 flag)
{
	struct scatterlist *sg;
	int npages;
	int i = 0;

	for_each_sg(table->sgl, sg, table->nents, i) {
		npages = sg->length / PAGE_SIZE;
		if (sg->length % PAGE_SIZE)
			npages++;
		while (npages--)
			set_page_private(nth_page(sg_page(sg), npages), flag);
	}
}

#define SECURE_PAGE_MAGIC 0xEEEEEEEE
int page_accessible(unsigned long pfn)
{
	struct page *page = pfn_to_page(pfn);

	if (page->private == SECURE_PAGE_MAGIC)
		return 0;
	else
		return 1;
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
	u32 *source_vm_copy;
	size_t source_vm_copy_size;
	struct qcom_scm_current_perm_info *dest_vm_copy;
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

	trace_hyp_assign_info(source_vm_list, source_nelems, dest_vmids,
			      dest_perms, dest_nelems);


	ret = batched_hyp_assign(table, source_vm_copy, source_vm_copy_size,
				 dest_vm_copy, dest_vm_copy_size);

	if (!ret) {
		while (dest_nelems--) {
			if (dest_vmids[dest_nelems] == VMID_HLOS)
				break;
		}

		if (dest_nelems == -1)
			set_each_page_of_sg(table, SECURE_PAGE_MAGIC);
		else
			set_each_page_of_sg(table, 0);
	}


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
	case VMID_LPASS:
		return "VMID_LPASS";
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
	case VMID_ADSP_HEAP:
		return "VMID_ADSP_HEAP";
	case VMID_INVAL:
		return "VMID_INVAL";
	case VMID_NAV:
		return "VMID_NAV";
	default:
		return "Unknown VMID";
	}
}
EXPORT_SYMBOL(msm_secure_vmid_to_string);

u32 msm_secure_get_vmid_perms(u32 vmid)
{
	if (vmid == VMID_CP_SEC_DISPLAY || (vmid == VMID_CP_CAMERA_PREVIEW &&
					    vmid_cp_camera_preview_ro))
		return PERM_READ;
	else if (vmid == VMID_CP_CDSP)
		return PERM_READ | PERM_WRITE | PERM_EXEC;
	else
		return PERM_READ | PERM_WRITE;
}
EXPORT_SYMBOL(msm_secure_get_vmid_perms);

static int qcom_secure_buffer_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	if (IS_ENABLED(CONFIG_ARM64)) {
		ret = dma_set_mask(dev, DMA_BIT_MASK(64));
		if (ret)
			return ret;
	}

	qcom_secure_buffer_dev = dev;
	vmid_cp_camera_preview_ro = of_property_read_bool(dev->of_node,
					"qcom,vmid-cp-camera-preview-ro");
	return 0;
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

static int __init qcom_secure_buffer_init(void)
{
	return platform_driver_register(&qcom_secure_buffer_driver);
}
subsys_initcall(qcom_secure_buffer_init);

static void __exit qcom_secure_buffer_exit(void)
{
	return platform_driver_unregister(&qcom_secure_buffer_driver);
}
module_exit(qcom_secure_buffer_exit);

MODULE_LICENSE("GPL v2");
