/*
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/of_device.h>
#include <linux/msm_audio_ion.h>
#include <linux/export.h>
#include <linux/qcom_iommu.h>
#include <asm/dma-iommu.h>

#define MSM_AUDIO_ION_PROBED (1 << 0)

#define MSM_AUDIO_ION_PHYS_ADDR(alloc_data) \
	alloc_data->table->sgl->dma_address

#define MSM_AUDIO_ION_VA_START 0x10000000
#define MSM_AUDIO_ION_VA_LEN 0x0FFFFFFF

#define MSM_AUDIO_SMMU_SID_OFFSET 32

struct addr_range {
	dma_addr_t start;
	size_t size;
};

struct context_bank_info {
	const char *name;
	struct addr_range addr_range;
};

struct msm_audio_ion_private {
	bool smmu_enabled;
	bool audioheap_enabled;
	struct device *cb_dev;
	struct dma_iommu_mapping *mapping;
	u8 device_status;
	struct list_head alloc_list;
	struct mutex list_mutex;
	u64 smmu_sid_bits;
	u32 smmu_version;
};

struct msm_audio_alloc_data {
	struct ion_client *client;
	struct ion_handle *handle;
	size_t len;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct list_head list;
};

static struct msm_audio_ion_private msm_audio_ion_data = {0,};

static int msm_audio_ion_get_phys(struct ion_client *client,
				  struct ion_handle *handle,
				  ion_phys_addr_t *addr, size_t *len);

static int msm_audio_dma_buf_map(struct ion_client *client,
				  struct ion_handle *handle,
				  ion_phys_addr_t *addr, size_t *len);

static int msm_audio_dma_buf_unmap(struct ion_client *client,
				   struct ion_handle *handle);

static void msm_audio_ion_add_allocation(
	struct msm_audio_ion_private *msm_audio_ion_data,
	struct msm_audio_alloc_data *alloc_data)
{
	/*
	 * Since these APIs can be invoked by multiple
	 * clients, there is need to make sure the list
	 * of allocations is always protected
	 */
	mutex_lock(&(msm_audio_ion_data->list_mutex));
	list_add_tail(&(alloc_data->list),
		      &(msm_audio_ion_data->alloc_list));
	mutex_unlock(&(msm_audio_ion_data->list_mutex));
}

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
	bufsz should be 0 and fd shouldn't be 0 as of now
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
	if (msm_audio_ion_data.smmu_enabled)
		msm_audio_dma_buf_unmap(client, handle);

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
			pr_err("%s: Unable to get phys address from ION buffer: %d\n"
				, __func__ , ret);
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
	pclient = msm_ion_client_create(name);
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
	bufsz should be 0 and fd shouldn't be 0 as of now
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
	if (msm_audio_ion_data.smmu_enabled)
		msm_audio_dma_buf_unmap(client, handle);

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
		pr_err("%s: Invalid params: %pK\n", __func__, abuff);
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


static int msm_audio_dma_buf_map(struct ion_client *client,
		struct ion_handle *handle,
		ion_phys_addr_t *addr, size_t *len)
{

	struct msm_audio_alloc_data *alloc_data;
	struct device *cb_dev;
	int rc = 0;

	cb_dev = msm_audio_ion_data.cb_dev;

	/* Data required per buffer mapping */
	alloc_data = kzalloc(sizeof(*alloc_data), GFP_KERNEL);
	if (!alloc_data) {
		pr_err("%s: No memory for alloc_data\n", __func__);
		return -ENOMEM;
	}

	/* Get the ION handle size */
	ion_handle_get_size(client, handle, len);

	alloc_data->client = client;
	alloc_data->handle = handle;
	alloc_data->len = *len;

	/* Get the dma_buf handle from ion_handle */
	alloc_data->dma_buf = ion_share_dma_buf(client, handle);
	if (IS_ERR(alloc_data->dma_buf)) {
		rc = PTR_ERR(alloc_data->dma_buf);
		dev_err(cb_dev,
			"%s: Fail to get dma_buf handle, rc = %d\n",
			__func__, rc);
		goto err_dma_buf;
	}

	/* Attach the dma_buf to context bank device */
	alloc_data->attach = dma_buf_attach(alloc_data->dma_buf,
					    cb_dev);
	if (IS_ERR(alloc_data->attach)) {
		rc = PTR_ERR(alloc_data->attach);
		dev_err(cb_dev,
			"%s: Fail to attach dma_buf to CB, rc = %d\n",
			__func__, rc);
		goto err_attach;
	}

	/*
	 * Get the scatter-gather list.
	 * There is no info as this is a write buffer or
	 * read buffer, hence the request is bi-directional
	 * to accomodate both read and write mappings.
	 */
	alloc_data->table = dma_buf_map_attachment(alloc_data->attach,
				DMA_BIDIRECTIONAL);
	if (IS_ERR(alloc_data->table)) {
		rc = PTR_ERR(alloc_data->table);
		dev_err(cb_dev,
			"%s: Fail to map attachment, rc = %d\n",
			__func__, rc);
		goto err_map_attach;
	}

	rc = dma_map_sg(cb_dev, alloc_data->table->sgl,
			alloc_data->table->nents,
			DMA_BIDIRECTIONAL);
	if (rc != alloc_data->table->nents) {
		dev_err(cb_dev,
			"%s: Fail to map SG, rc = %d, nents = %d\n",
			__func__, rc, alloc_data->table->nents);
		goto err_map_sg;
	}
	/* Make sure not to return rc from dma_map_sg, as it can be nonzero */
	rc = 0;

	/* physical address from mapping */
	*addr = MSM_AUDIO_ION_PHYS_ADDR(alloc_data);

	msm_audio_ion_add_allocation(&msm_audio_ion_data,
				     alloc_data);
	return rc;

err_map_sg:
	dma_buf_unmap_attachment(alloc_data->attach,
				 alloc_data->table,
				 DMA_BIDIRECTIONAL);
err_map_attach:
	dma_buf_detach(alloc_data->dma_buf,
		       alloc_data->attach);
err_attach:
	dma_buf_put(alloc_data->dma_buf);

err_dma_buf:
	kfree(alloc_data);

	return rc;
}

static int msm_audio_dma_buf_unmap(struct ion_client *client,
				   struct ion_handle *handle)
{
	int rc = 0;
	struct msm_audio_alloc_data *alloc_data = NULL;
	struct list_head *ptr, *next;
	struct device *cb_dev = msm_audio_ion_data.cb_dev;
	bool found = false;

	/*
	 * Though list_for_each_safe is delete safe, lock
	 * should be explicitly acquired to avoid race condition
	 * on adding elements to the list.
	 */
	mutex_lock(&(msm_audio_ion_data.list_mutex));
	list_for_each_safe(ptr, next,
			    &(msm_audio_ion_data.alloc_list)) {

		alloc_data = list_entry(ptr, struct msm_audio_alloc_data,
					list);

		if (alloc_data->handle == handle &&
		    alloc_data->client == client) {
			found = true;
			dma_unmap_sg(cb_dev,
				    alloc_data->table->sgl,
				    alloc_data->table->nents,
				    DMA_BIDIRECTIONAL);

			dma_buf_unmap_attachment(alloc_data->attach,
						 alloc_data->table,
						 DMA_BIDIRECTIONAL);

			dma_buf_detach(alloc_data->dma_buf,
				       alloc_data->attach);

			dma_buf_put(alloc_data->dma_buf);

			list_del(&(alloc_data->list));
			kfree(alloc_data);
			break;
		}
	}
	mutex_unlock(&(msm_audio_ion_data.list_mutex));

	if (!found) {
		dev_err(cb_dev,
			"%s: cannot find allocation, ion_handle %pK, ion_client %pK",
			__func__, handle, client);
		rc = -EINVAL;
	}

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
		rc = msm_audio_dma_buf_map(client, handle, addr, len);
		if (rc) {
			pr_err("%s: failed to map DMA buf, err = %d\n",
				__func__, rc);
			goto err;
		}
		/* Append the SMMU SID information to the IOVA address */
		*addr |= msm_audio_ion_data.smmu_sid_bits;
	} else {
		rc = ion_phys(client, handle, addr, len);
	}

	pr_debug("phys=%pK, len=%zd, rc=%d\n", &(*addr), *len, rc);
err:
	return rc;
}

static int msm_audio_smmu_init_legacy(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	struct device_node *ctx_node = NULL;
	struct context_bank_info *cb;
	int ret;
	u32 read_val[2];

	cb = devm_kzalloc(dev, sizeof(struct context_bank_info), GFP_KERNEL);
	if (!cb)
		return -ENOMEM;

	ctx_node = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!ctx_node) {
		dev_err(dev, "%s Could not find any iommus for audio\n",
			__func__);
		return -EINVAL;
	}
	ret = of_property_read_string(ctx_node, "label", &(cb->name));
	if (ret) {
		dev_err(dev, "%s Could not find label\n", __func__);
		return -EINVAL;
	}
	pr_debug("label found : %s\n", cb->name);
	ret = of_property_read_u32_array(ctx_node,
				"qcom,virtual-addr-pool",
				read_val, 2);
	if (ret) {
		dev_err(dev, "%s Could not read addr pool for group : (%d)\n",
			__func__, ret);
		return -EINVAL;
	}
	msm_audio_ion_data.cb_dev = msm_iommu_get_ctx(cb->name);
	cb->addr_range.start = (dma_addr_t) read_val[0];
	cb->addr_range.size = (size_t) read_val[1];
	dev_dbg(dev, "%s Legacy iommu usage\n", __func__);
	mapping = arm_iommu_create_mapping(
				msm_iommu_get_bus(msm_audio_ion_data.cb_dev),
					   cb->addr_range.start,
					   cb->addr_range.size);
	if (IS_ERR(mapping))
		return PTR_ERR(mapping);

	ret = arm_iommu_attach_device(msm_audio_ion_data.cb_dev, mapping);
	if (ret) {
		dev_err(dev, "%s: Attach failed, err = %d\n",
			__func__, ret);
		goto fail_attach;
	}

	msm_audio_ion_data.mapping = mapping;
	INIT_LIST_HEAD(&msm_audio_ion_data.alloc_list);
	mutex_init(&(msm_audio_ion_data.list_mutex));

	return 0;

fail_attach:
	arm_iommu_release_mapping(mapping);
	return ret;
}

static int msm_audio_smmu_init(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	int ret;
	int disable_htw = 1;

	mapping = arm_iommu_create_mapping(
					msm_iommu_get_bus(dev),
					   MSM_AUDIO_ION_VA_START,
					   MSM_AUDIO_ION_VA_LEN);
	if (IS_ERR(mapping))
		return PTR_ERR(mapping);

	iommu_domain_set_attr(mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				&disable_htw);

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret) {
		dev_err(dev, "%s: Attach failed, err = %d\n",
			__func__, ret);
		goto fail_attach;
	}

	msm_audio_ion_data.cb_dev = dev;
	msm_audio_ion_data.mapping = mapping;
	INIT_LIST_HEAD(&msm_audio_ion_data.alloc_list);
	mutex_init(&(msm_audio_ion_data.list_mutex));

	return 0;

fail_attach:
	arm_iommu_release_mapping(mapping);
	return ret;
}

static const struct of_device_id msm_audio_ion_dt_match[] = {
	{ .compatible = "qcom,msm-audio-ion" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_audio_ion_dt_match);


u32 msm_audio_ion_get_smmu_sid_mode32(void)
{
	if (msm_audio_ion_data.smmu_enabled)
		return upper_32_bits(msm_audio_ion_data.smmu_sid_bits);
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
	const char *msm_audio_ion_smmu = "qcom,smmu-version";
	bool smmu_enabled;
	enum apr_subsys_state q6_state;
	struct device *dev = &pdev->dev;

	if (dev->of_node == NULL) {
		dev_err(dev,
			"%s: device tree is not found\n",
			__func__);
		msm_audio_ion_data.smmu_enabled = 0;
		return 0;
	}

	smmu_enabled = of_property_read_bool(dev->of_node,
					     msm_audio_ion_dt);
	msm_audio_ion_data.smmu_enabled = smmu_enabled;

	if (smmu_enabled) {
		rc = of_property_read_u32(dev->of_node,
					msm_audio_ion_smmu,
					&msm_audio_ion_data.smmu_version);
		if (rc) {
			dev_err(dev,
				"%s: qcom,smmu_version missing in DT node\n",
				__func__);
			return rc;
		}
		dev_dbg(dev, "%s: SMMU version is (%d)", __func__,
				msm_audio_ion_data.smmu_version);
		q6_state = apr_get_q6_state();
		if (q6_state == APR_SUBSYS_DOWN) {
			dev_dbg(dev,
				"defering %s, adsp_state %d\n",
				__func__, q6_state);
			return -EPROBE_DEFER;
		} else {
			dev_dbg(dev, "%s: adsp is ready\n", __func__);
		}
	}

	dev_dbg(dev, "%s: SMMU is %s\n", __func__,
		(smmu_enabled) ? "Enabled" : "Disabled");

	if (smmu_enabled) {
		u64 smmu_sid = 0;
		struct of_phandle_args iommuspec;

		/* Get SMMU SID information from Devicetree */
		rc = of_parse_phandle_with_args(dev->of_node, "iommus",
						"#iommu-cells", 0, &iommuspec);
		if (rc)
			dev_err(dev, "%s: could not get smmu SID, ret = %d\n",
				__func__, rc);
		else
			smmu_sid = iommuspec.args[0];

		msm_audio_ion_data.smmu_sid_bits =
			smmu_sid << MSM_AUDIO_SMMU_SID_OFFSET;

		if (msm_audio_ion_data.smmu_version == 0x1) {
			rc = msm_audio_smmu_init_legacy(dev);
		} else if (msm_audio_ion_data.smmu_version == 0x2) {
			rc = msm_audio_smmu_init(dev);
		} else {
			dev_err(dev, "%s: smmu version invalid %d\n",
				__func__, msm_audio_ion_data.smmu_version);
			rc = -EINVAL;
		}
		if (rc)
			dev_err(dev, "%s: smmu init failed, err = %d\n",
				__func__, rc);
	}

	if (!rc)
		msm_audio_ion_data.device_status |= MSM_AUDIO_ION_PROBED;

	return rc;
}

static int msm_audio_ion_remove(struct platform_device *pdev)
{
	struct dma_iommu_mapping *mapping;
	struct device *audio_cb_dev;

	mapping = msm_audio_ion_data.mapping;
	audio_cb_dev = msm_audio_ion_data.cb_dev;

	if (audio_cb_dev && mapping) {
		arm_iommu_detach_device(audio_cb_dev);
		arm_iommu_release_mapping(mapping);
	}

	msm_audio_ion_data.smmu_enabled = 0;
	msm_audio_ion_data.device_status = 0;
	return 0;
}

static struct platform_driver msm_audio_ion_driver = {
	.driver = {
		.name = "msm-audio-ion",
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

MODULE_DESCRIPTION("MSM Audio ION module");
MODULE_LICENSE("GPL v2");
