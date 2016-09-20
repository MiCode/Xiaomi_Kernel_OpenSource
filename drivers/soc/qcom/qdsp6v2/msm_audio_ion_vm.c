/*
 * Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/msm_audio_ion.h>
#include "../../../staging/android/ion/ion_priv.h"
#include "../../../staging/android/ion/ion_hvenv_driver.h"

#define MSM_AUDIO_ION_PROBED (1 << 0)

struct msm_audio_ion_private {
	bool smmu_enabled;
	bool audioheap_enabled;
	u8 device_status;
};

static struct msm_audio_ion_private msm_audio_ion_data = {0,};

static int msm_audio_ion_get_phys(struct ion_client *client,
				struct ion_handle *handle,
				ion_phys_addr_t *addr, size_t *len);

int msm_audio_ion_alloc(const char *name, struct ion_client **client,
			struct ion_handle **handle, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = -EINVAL;
	unsigned long err_ion_ptr = 0;

	if ((msm_audio_ion_data.smmu_enabled == true) &&
	    !(msm_audio_ion_data.device_status & MSM_AUDIO_ION_PROBED)) {
		pr_debug("%s:probe is not done, deferred\n", __func__);
		return -EPROBE_DEFER;
	}
	if (!name || !client || !handle || !paddr || !vaddr
		|| !bufsz || !pa_len) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	*client = msm_audio_ion_client_create(name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		goto err;
	}

	*handle = ion_alloc(*client, bufsz, SZ_4K,
			ION_HEAP(ION_AUDIO_HEAP_ID), 0);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		if (msm_audio_ion_data.smmu_enabled == true) {
			pr_debug("system heap is used");
			msm_audio_ion_data.audioheap_enabled = 0;
			*handle = ion_alloc(*client, bufsz, SZ_4K,
					ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
		}
		if (IS_ERR_OR_NULL((void *) (*handle))) {
			if (IS_ERR((void *)(*handle)))
				err_ion_ptr = PTR_ERR((int *)(*handle));
			pr_err("%s:ION alloc fail err ptr=%ld, smmu_enabled=%d\n",
			__func__, err_ion_ptr, msm_audio_ion_data.smmu_enabled);
			rc = -ENOMEM;
			goto err_ion_client;
		}
	} else {
		pr_debug("audio heap is used");
		msm_audio_ion_data.audioheap_enabled = 1;
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
	pr_debug("%s: mapped address = %pK, size=%zd\n", __func__,
		*vaddr, bufsz);

	if (bufsz != 0) {
		pr_debug("%s: memset to 0 %pK %zd\n", __func__, *vaddr, bufsz);
		memset((void *)*vaddr, 0, bufsz);
	}

	return rc;

err_ion_handle:
	ion_free(*client, *handle);
err_ion_client:
	msm_audio_ion_client_destroy(*client);
	*handle = NULL;
	*client = NULL;
err:
	return rc;
}
EXPORT_SYMBOL(msm_audio_ion_alloc);

int msm_audio_ion_phys_free(struct ion_client *client,
			    struct ion_handle *handle,
			    ion_phys_addr_t *paddr,
			    size_t *pa_len, u8 assign_type)
{
	if (!(msm_audio_ion_data.device_status & MSM_AUDIO_ION_PROBED)) {
		pr_debug("%s:probe is not done, deferred\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!client || !handle || !paddr || !pa_len) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	/* hyp assign is not supported in VM */

	ion_free(client, handle);
	ion_client_destroy(client);

	return 0;
}

int msm_audio_ion_phys_assign(const char *name, struct ion_client **client,
			      struct ion_handle **handle, int fd,
			      ion_phys_addr_t *paddr,
			      size_t *pa_len, u8 assign_type)
{
	int ret;

	if (!(msm_audio_ion_data.device_status & MSM_AUDIO_ION_PROBED)) {
		pr_debug("%s:probe is not done, deferred\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!name || !client || !handle || !paddr || !pa_len) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	*client = msm_audio_ion_client_create(name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client failed\n", __func__);
		return -EINVAL;
	}

	*handle = ion_import_dma_buf(*client, fd);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
			__func__);
		ret = -EINVAL;
		goto err_destroy_client;
	}

	ret = ion_phys(*client, *handle, paddr, pa_len);
	if (ret) {
		pr_err("%s: could not get physical address for handle, ret = %d\n",
			__func__, ret);
		goto err_ion_handle;
	}

	/* hyp assign is not supported in VM */

	return ret;

err_ion_handle:
	ion_free(*client, *handle);

err_destroy_client:
	ion_client_destroy(*client);
	*client = NULL;
	*handle = NULL;

	return ret;
}

int msm_audio_ion_import(const char *name, struct ion_client **client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	if ((msm_audio_ion_data.smmu_enabled == true) &&
	    !(msm_audio_ion_data.device_status & MSM_AUDIO_ION_PROBED)) {
		pr_debug("%s:probe is not done, deferred\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!name || !client || !handle || !paddr || !vaddr || !pa_len) {
		pr_err("%s: Invalid params\n", __func__);
		rc = -EINVAL;
		goto err;
	}

	*client = msm_audio_ion_client_create(name);
	if (IS_ERR_OR_NULL((void *)(*client))) {
		pr_err("%s: ION create client for AUDIO failed\n", __func__);
		rc = -EINVAL;
		goto err;
	}

	/* name should be audio_acdb_client or Audio_Dec_Client,
	 * bufsz should be 0 and fd shouldn't be 0 as of now
	 */
	*handle = ion_import_dma_buf(*client, fd);
	pr_debug("%s: DMA Buf name=%s, fd=%d handle=%pK\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *) (*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
				__func__);
		rc = -EINVAL;
		goto err_destroy_client;
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
		rc = -ENOMEM;
		goto err_ion_handle;
	}
	pr_debug("%s: mapped address = %pK, size=%zd\n", __func__,
		*vaddr, bufsz);

	return 0;

err_ion_handle:
	ion_free(*client, *handle);
err_destroy_client:
	msm_audio_ion_client_destroy(*client);
	*client = NULL;
	*handle = NULL;
err:
	return rc;
}

int msm_audio_ion_free(struct ion_client *client, struct ion_handle *handle)
{
	if (!client || !handle) {
		pr_err("%s Invalid params\n", __func__);
		return -EINVAL;
	}

	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	msm_audio_ion_client_destroy(client);
	return 0;
}
EXPORT_SYMBOL(msm_audio_ion_free);

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
			unsigned long len = sg->length;

			page = sg_page(sg);

			if (offset >= len) {
				offset -= len;
				continue;
			} else if (offset) {
				page += offset / PAGE_SIZE;
				len -= offset;
				offset = 0;
			}
			len = min(len, remainder);
			pr_debug("vma=%pK, addr=%x len=%ld vm_start=%x vm_end=%x vm_page_prot=%ld\n",
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
		size_t va_len = 0;

		pr_debug("%s: page is NULL\n", __func__);

		ret = ion_phys(ab->client, ab->handle, &phys_addr, &phys_len);
		if (ret) {
			pr_err("%s: Unable to get phys address from ION buffer: %d\n",
				__func__, ret);
			return ret;
		}
		pr_debug("phys=%pK len=%zd\n", &phys_addr, phys_len);
		pr_debug("vma=%pK, vm_start=%x vm_end=%x vm_pgoff=%ld vm_page_prot=%ld\n",
			vma, (unsigned int)vma->vm_start,
			(unsigned int)vma->vm_end, vma->vm_pgoff,
			(unsigned long int)vma->vm_page_prot);
		va_len = vma->vm_end - vma->vm_start;
		if ((offset > phys_len) || (va_len > phys_len-offset)) {
			pr_err("wrong offset size %ld, lens= %zd, va_len=%zd\n",
				offset, phys_len, va_len);
			return -EINVAL;
		}
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
struct ion_client *msm_audio_ion_client_create(const char *name)
{
	struct ion_client *pclient = NULL;

	pclient = hvenv_ion_client_create(name);
	return pclient;
}


void msm_audio_ion_client_destroy(struct ion_client *client)
{
	pr_debug("%s: client = %pK smmu_enabled = %d\n", __func__,
		client, msm_audio_ion_data.smmu_enabled);

	ion_client_destroy(client);
}

int msm_audio_ion_import_legacy(const char *name, struct ion_client *client,
			struct ion_handle **handle, int fd,
			unsigned long *ionflag, size_t bufsz,
			ion_phys_addr_t *paddr, size_t *pa_len, void **vaddr)
{
	int rc = 0;

	if (!name || !client || !handle || !paddr || !vaddr || !pa_len) {
		pr_err("%s: Invalid params\n", __func__);
		rc = -EINVAL;
		goto err;
	}
	/* client is already created for legacy and given*/
	/* name should be audio_acdb_client or Audio_Dec_Client,
	 * bufsz should be 0 and fd shouldn't be 0 as of now
	 */
	*handle = ion_import_dma_buf(client, fd);
	pr_debug("%s: DMA Buf name=%s, fd=%d handle=%pK\n", __func__,
							name, fd, *handle);
	if (IS_ERR_OR_NULL((void *)(*handle))) {
		pr_err("%s: ion import dma buffer failed\n",
			__func__);
		rc = -EINVAL;
		goto err;
	}

	if (ionflag != NULL) {
		rc = ion_handle_get_flags(client, *handle, ionflag);
		if (rc) {
			pr_err("%s: could not get flags for the handle\n",
							__func__);
			rc = -EINVAL;
			goto err_ion_handle;
		}
	}

	rc = msm_audio_ion_get_phys(client, *handle, paddr, pa_len);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		rc = -EINVAL;
		goto err_ion_handle;
	}

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		rc = -EINVAL;
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	return 0;

err_ion_handle:
	ion_free(client, *handle);
err:
	return rc;
}

int msm_audio_ion_free_legacy(struct ion_client *client,
			      struct ion_handle *handle)
{
	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	/* no client_destrody in legacy*/
	return 0;
}

static int msm_audio_ion_get_phys(struct ion_client *client,
		struct ion_handle *handle,
		ion_phys_addr_t *addr, size_t *len)
{
	int rc = 0;

	pr_debug("%s: smmu_enabled = %d\n", __func__,
		msm_audio_ion_data.smmu_enabled);

	if (msm_audio_ion_data.smmu_enabled) {
		rc = ion_phys(client, handle, addr, len);
		if (rc) {
			pr_err("%s: ion_phys failed, err = %d\n",
				__func__, rc);
			goto err;
		}
		/* Append the SMMU SID information to the IOVA address */
		*addr |= (1 << 32);
	} else {
		rc = ion_phys(client, handle, addr, len);
	}

	pr_debug("%s: phys=%pK, len=%zd, rc=%d\n",
		__func__, &(*addr), *len, rc);
err:
	return rc;
}

static const struct of_device_id msm_audio_ion_dt_match[] = {
	{ .compatible = "qcom,msm-audio-ion-vm" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_audio_ion_dt_match);

u32 msm_audio_ion_get_smmu_sid_mode32(void)
{
	if (msm_audio_ion_data.smmu_enabled)
		return 1;
	else
		return 0;
}

u32 msm_audio_populate_upper_32_bits(ion_phys_addr_t pa)
{
	if (sizeof(ion_phys_addr_t) == sizeof(u32))
		return msm_audio_ion_get_smmu_sid_mode32();
	else
		return upper_32_bits(pa);
}

static int msm_audio_ion_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *msm_audio_ion_dt = "qcom,smmu-enabled";
	bool smmu_enabled;
	struct device *dev = &pdev->dev;

	if (dev->of_node == NULL) {
		pr_err("%s: device tree is not found\n",
			__func__);
		msm_audio_ion_data.smmu_enabled = 0;
		return 0;
	}

	smmu_enabled = of_property_read_bool(dev->of_node,
					     msm_audio_ion_dt);
	msm_audio_ion_data.smmu_enabled = smmu_enabled;

	pr_info("%s: SMMU is %s\n", __func__,
		(smmu_enabled) ? "Enabled" : "Disabled");

	if (!rc)
		msm_audio_ion_data.device_status |= MSM_AUDIO_ION_PROBED;

	return rc;
}

static int msm_audio_ion_remove(struct platform_device *pdev)
{
	msm_audio_ion_data.smmu_enabled = 0;
	msm_audio_ion_data.device_status = 0;
	return 0;
}

static struct platform_driver msm_audio_ion_driver = {
	.driver = {
		.name = "msm-audio-ion-vm",
		.owner = THIS_MODULE,
		.of_match_table = msm_audio_ion_dt_match,
	},
	.probe = msm_audio_ion_probe,
	.remove = msm_audio_ion_remove,
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

MODULE_DESCRIPTION("MSM Audio ION VM module");
MODULE_LICENSE("GPL v2");
