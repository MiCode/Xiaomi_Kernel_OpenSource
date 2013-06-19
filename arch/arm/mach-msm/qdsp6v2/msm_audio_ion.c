/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/subsystem_restart.h>
#include <mach/qdsp6v2/apr.h>
#include <linux/of_device.h>
#include <linux/msm_audio_ion.h>

#include <linux/iommu.h>
#include <mach/iommu_domains.h>

struct msm_audio_ion_private {
	bool smmu_enabled;
	bool audioheap_enabled;
	struct iommu_group *group;
	u32 domain_id;
	struct iommu_domain *domain;
};

static struct msm_audio_ion_private msm_audio_ion_data = {0,};


static int msm_audio_ion_get_phys(struct ion_client *client,
				  struct ion_handle *handle,
				  ion_phys_addr_t *addr, size_t *len);



int msm_audio_ion_alloc(const char *name, struct ion_client **client,
			struct ion_handle **handle, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	*client = msm_audio_ion_client_create(UINT_MAX, name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto err;
	}

	*handle = ion_alloc(*client, bufsz, SZ_4K,
			ION_HEAP(ION_AUDIO_HEAP_ID), 0);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_debug("system heap is used");
		msm_audio_ion_data.audioheap_enabled = 0;
		*handle = ion_alloc(*client, bufsz, SZ_4K,
				ION_HEAP(ION_SYSTEM_HEAP_ID), 0);

	} else {
		pr_debug("audio heap is used");
		msm_audio_ion_data.audioheap_enabled = 1;
	}

	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ION memory allocation for AUDIO failed rc=%d, smmu_enabled=%d\n",
			__func__, rc, msm_audio_ion_data.smmu_enabled);
		goto err_ion_client;
	}

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_ion_handle;
	}

	*vaddr = ion_map_kernel(*client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}
	pr_debug("%s: mapped address = %p, size=%d\n", __func__, *vaddr, bufsz);

	if (bufsz != 0) {
		pr_debug("%s: memset to 0 %p %d\n", __func__, *vaddr, bufsz);
		memset((void *)*vaddr, 0, bufsz);
	}

	return 0;

err_ion_handle:
	ion_free(*client, *handle);
err_ion_client:
	msm_audio_ion_client_destroy(*client);
err:
	return -EINVAL;
}

int msm_audio_ion_import(const char *name, struct ion_client **client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	*client = msm_audio_ion_client_create(UINT_MAX, name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto err;
	}

	/* name should be audio_acdb_client or Audio_Dec_Client,
	bufsz should be 0 and fd shouldn't be 0 as of now
	*/
	*handle = ion_import_dma_buf(*client, fd);
	pr_err("%s: DMA Buf name=%s, fd=%d handle=%p\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
				__func__);
		goto err_ion_handle;
		}

	if (ionflag != NULL) {
		rc = ion_handle_get_flags(*client, *handle, ionflag);
		if (rc) {
			pr_err("%s: could not get flags for the handle\n",
				__func__);
			goto err_ion_handle;
		}
	}

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
				__func__, rc);
		goto err_ion_handle;
	}

	*vaddr = ion_map_kernel(*client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}
	pr_debug("%s: mapped address = %p, size=%d\n", __func__, *vaddr, bufsz);

	return 0;

err_ion_handle:
	ion_free(*client, *handle);
	msm_audio_ion_client_destroy(*client);
err:
	return -EINVAL;
}

int msm_audio_ion_free(struct ion_client *client, struct ion_handle *handle)
{
	if (msm_audio_ion_data.smmu_enabled) {
		/* Need to populate book kept infomation */
		pr_debug("client=%p, domain=%p, domain_id=%d, group=%p",
			client, msm_audio_ion_data.domain,
			msm_audio_ion_data.domain_id, msm_audio_ion_data.group);

		ion_unmap_iommu(client, handle,
				msm_audio_ion_data.domain_id, 0);
	}

	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	msm_audio_ion_client_destroy(client);
	return 0;
}

int msm_audio_ion_mmap(struct audio_buffer *ab,
		       struct vm_area_struct *vma)
{
	struct sg_table *table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	unsigned int i;
	struct page *page;
	int ret;

	pr_debug("%s\n", __func__);

	table = ion_sg_table(ab->client, ab->handle);

	if (IS_ERR(table)) {
		pr_err("%s: Unable to get sg_table from ion: %ld\n",
			__func__, PTR_ERR(table));
		return PTR_ERR(table);
	} else if (!table) {
		pr_err("%s: sg_list is NULL\n", __func__);
		return -EINVAL;
	}

	/* uncached */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* We need to check if a page is associated with this sg list because:
	 * If the allocation came from a carveout we currently don't have
	 * pages associated with carved out memory. This might change in the
	 * future and we can remove this check and the else statement.
	 */
	page = sg_page(table->sgl);
	if (page) {
		pr_debug("%s: page is NOT null\n", __func__);
		for_each_sg(table->sgl, sg, table->nents, i) {
			unsigned long remainder = vma->vm_end - addr;
			unsigned long len = sg_dma_len(sg);

			page = sg_page(sg);

			if (offset >= sg_dma_len(sg)) {
				offset -= sg_dma_len(sg);
				continue;
			} else if (offset) {
				page += offset / PAGE_SIZE;
				len = sg_dma_len(sg) - offset;
				offset = 0;
			}
			len = min(len, remainder);
			pr_debug("vma=%p, addr=%x len=%ld vm_start=%x vm_end=%x vm_page_prot=%ld\n",
				vma, (unsigned int)addr, len,
				(unsigned int)vma->vm_start,
				(unsigned int)vma->vm_end,
				(unsigned long int)vma->vm_page_prot);
			remap_pfn_range(vma, addr, page_to_pfn(page), len,
					vma->vm_page_prot);
			addr += len;
			if (addr >= vma->vm_end)
				return 0;
		}
	} else {
		ion_phys_addr_t phys_addr;
		size_t phys_len;
		pr_debug("%s: page is NULL\n", __func__);

		ret = ion_phys(ab->client, ab->handle, &phys_addr, &phys_len);
		if (ret) {
			pr_err("%s: Unable to get phys address from ION buffer: %d\n"
				, __func__ , ret);
			return ret;
		}
		pr_debug("phys=%x len=%d\n", (unsigned int)phys_addr, phys_len);
		pr_debug("vma=%p, vm_start=%x vm_end=%x vm_pgoff=%ld vm_page_prot=%ld\n",
			vma, (unsigned int)vma->vm_start,
			(unsigned int)vma->vm_end, vma->vm_pgoff,
			(unsigned long int)vma->vm_page_prot);
		ret =  remap_pfn_range(vma, vma->vm_start,
				__phys_to_pfn(phys_addr) + vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot);
	}
	return 0;
}


bool msm_audio_ion_is_smmu_available(void)
{
	return msm_audio_ion_data.smmu_enabled;
}

/* move to static section again */
struct ion_client *msm_audio_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	struct ion_client *pclient = NULL;
	/*IOMMU group and domain are moved to probe()*/
	pclient = msm_ion_client_create(heap_mask, name);
	return pclient;
}


void msm_audio_ion_client_destroy(struct ion_client *client)
{
	pr_debug("%s: client = %p smmu_enabled = %d\n", __func__,
		client, msm_audio_ion_data.smmu_enabled);

	ion_client_destroy(client);
}

int msm_audio_ion_import_legacy(const char *name, struct ion_client *client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;
	/* client is already created for legacy and given*/
	/* name should be audio_acdb_client or Audio_Dec_Client,
	bufsz should be 0 and fd shouldn't be 0 as of now
	*/
	*handle = ion_import_dma_buf(client, fd);
	pr_debug("%s: DMA Buf name=%s, fd=%d handle=%p\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *)(*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
			__func__);
		goto err_ion_handle;
		}

	if (ionflag != NULL) {
		rc = ion_handle_get_flags(client, *handle, ionflag);
		if (rc) {
			pr_err("%s: could not get flags for the handle\n",
							__func__);
			goto err_ion_handle;
		}
	}

	rc = msm_audio_ion_get_phys(client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_ion_handle;
	}

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	return 0;

err_ion_handle:
	ion_free(client, *handle);
	return -EINVAL;

}

int msm_audio_ion_free_legacy(struct ion_client *client,
			      struct ion_handle *handle)
{
	/* To add condition for SMMU enabled */
	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	/* no client_destrody in legacy*/
	return 0;
}

int msm_audio_ion_cache_operations(struct audio_buffer *abuff, int cache_op)
{
	unsigned long ionflag = 0;
	int rc = 0;
	int msm_cache_ops = 0;

	if (!abuff) {
		pr_err("Invalid params: %p, %p\n", __func__, abuff);
		return -EINVAL;
	}
	rc = ion_handle_get_flags(abuff->client, abuff->handle,
		&ionflag);
	if (rc) {
		pr_err("ion_handle_get_flags failed: %d\n", rc);
		goto cache_op_failed;
	}

	/* has to be CACHED */
	if (ION_IS_CACHED(ionflag)) {
		/* ION_IOC_INV_CACHES or ION_IOC_CLEAN_CACHES */
		msm_cache_ops = cache_op;
		rc = msm_ion_do_cache_op(abuff->client,
				abuff->handle,
				(unsigned long *) abuff->data,
				(unsigned long)abuff->size,
				msm_cache_ops);
		if (rc) {
			pr_err("cache operation failed %d\n", rc);
			goto cache_op_failed;
		}
	}
cache_op_failed:
	return rc;
}


static int msm_audio_ion_get_phys(struct ion_client *client,
		struct ion_handle *handle,
		ion_phys_addr_t *addr, size_t *len)
{
	int rc = 0;
	pr_debug("%s: smmu_enabled = %d\n", __func__,
		msm_audio_ion_data.smmu_enabled);

	if (msm_audio_ion_data.smmu_enabled) {
		rc = ion_map_iommu(client, handle, msm_audio_ion_data.domain_id,
			0 /*partition_num*/, SZ_4K /*align*/, 0/*iova_length*/,
			addr, (unsigned long *)len,
			0, 0);
		if (rc) {
			pr_err("%s: ION map iommu failed %d\n", __func__, rc);
			return rc;
		}
		pr_debug("client=%p, domain=%p, domain_id=%d, group=%p",
			client, msm_audio_ion_data.domain,
			msm_audio_ion_data.domain_id, msm_audio_ion_data.group);
	} else {
		/* SMMU is disabled*/
		rc = ion_phys(client, handle, addr, len);
	}
	pr_debug("phys=%x, len=%d, rc=%d\n", (unsigned int)*addr, *len, rc);
	return rc;
}

static int msm_audio_ion_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *msm_audio_ion_dt = "qcom,smmu-enabled";
	bool smmu_enabled;

	if (pdev->dev.of_node == NULL) {
		pr_err("%s: device tree is not found\n", __func__);
		msm_audio_ion_data.smmu_enabled = 0;
		return 0;
	}

	smmu_enabled = of_property_read_bool(pdev->dev.of_node,
					     msm_audio_ion_dt);
	msm_audio_ion_data.smmu_enabled = smmu_enabled;

	if (smmu_enabled) {
		msm_audio_ion_data.group = iommu_group_find("lpass_audio");
		if (!msm_audio_ion_data.group) {
			pr_debug("Failed to find group lpass_audio deferred\n");
			goto fail_group;
		}
		msm_audio_ion_data.domain =
			iommu_group_get_iommudata(msm_audio_ion_data.group);
		if (IS_ERR_OR_NULL(msm_audio_ion_data.domain)) {
			pr_err("Failed to get domain data for group %p",
					msm_audio_ion_data.group);
			goto fail_group;
		}
		msm_audio_ion_data.domain_id =
				msm_find_domain_no(msm_audio_ion_data.domain);
		if (msm_audio_ion_data.domain_id < 0) {
			pr_err("Failed to get domain index for domain %p",
					msm_audio_ion_data.domain);
			goto fail_group;
		}
		pr_debug("domain=%p, domain_id=%d, group=%p",
			msm_audio_ion_data.domain,
			msm_audio_ion_data.domain_id, msm_audio_ion_data.group);

		/* iommu_attach_group() will make AXI clock ON. For future PL
		this will require to be called in once per session */
		rc = iommu_attach_group(msm_audio_ion_data.domain,
					msm_audio_ion_data.group);
		if (rc) {
			pr_err("%s:ION attach group failed %d\n", __func__, rc);
			return rc;
		}

	}

	pr_debug("%s: SMMU-Enabled = %d\n", __func__, smmu_enabled);
	return rc;

fail_group:
	return -EPROBE_DEFER;
}

static int msm_audio_ion_remove(struct platform_device *pdev)
{
	pr_debug("%s: msm audio ion is unloaded, domain=%p, group=%p\n",
		__func__, msm_audio_ion_data.domain, msm_audio_ion_data.group);
	iommu_detach_group(msm_audio_ion_data.domain, msm_audio_ion_data.group);

	return 0;
}

static const struct of_device_id msm_audio_ion_dt_match[] = {
	{ .compatible = "qcom,msm-audio-ion" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_audio_ion_dt_match);

static struct platform_driver msm_audio_ion_driver = {
	.driver = {
		.name = "msm-audio-ion",
		.owner = THIS_MODULE,
		.of_match_table = msm_audio_ion_dt_match,
	},
	.probe = msm_audio_ion_probe,
	.remove = __devexit_p(msm_audio_ion_remove),
};

static int __init msm_audio_ion_init(void)
{
	return platform_driver_register(&msm_audio_ion_driver);
}
module_init(msm_audio_ion_init);

static void __exit msm_audio_ion_exit(void)
{
	platform_driver_unregister(&msm_audio_ion_driver);
}
module_exit(msm_audio_ion_exit);

MODULE_DESCRIPTION("MSM Audio ION module");
MODULE_LICENSE("GPL v2");
