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
#include <linux/habmm.h>
#include "../../../staging/android/ion/ion_priv.h"

#define MSM_AUDIO_ION_PROBED (1 << 0)

#define MSM_AUDIO_SMMU_VM_CMD_MAP 0x00000001
#define MSM_AUDIO_SMMU_VM_CMD_UNMAP 0x00000002
#define MSM_AUDIO_SMMU_VM_HAB_MINOR_ID 1

struct msm_audio_ion_private {
	bool smmu_enabled;
	bool audioheap_enabled;
	u8 device_status;
	struct list_head smmu_map_list;
	struct mutex smmu_map_mutex;
};

struct msm_audio_smmu_map_data {
	struct ion_client *client;
	struct ion_handle *handle;
	u32 export_id;
	struct list_head list;
};

struct msm_audio_smmu_vm_map_cmd {
	int cmd_id;
	u32 export_id;
	u32 buf_size;
};

struct msm_audio_smmu_vm_map_cmd_rsp {
	int status;
	u64 addr;
};

struct msm_audio_smmu_vm_unmap_cmd {
	int cmd_id;
	u32 export_id;
};

struct msm_audio_smmu_vm_unmap_cmd_rsp {
	int status;
};

static struct msm_audio_ion_private msm_audio_ion_data = {0,};
static u32 msm_audio_ion_hab_handle;

static int msm_audio_ion_get_phys(struct ion_client *client,
				struct ion_handle *handle,
				ion_phys_addr_t *addr, size_t *len,
				void *vaddr);

static int msm_audio_ion_smmu_map(struct ion_client *client,
		struct ion_handle *handle,
		ion_phys_addr_t *addr, size_t *len, void *vaddr)
{
	int rc;
	u32 export_id;
	u32 cmd_rsp_size;
	bool exported = false;
	struct msm_audio_smmu_vm_map_cmd_rsp cmd_rsp;
	struct msm_audio_smmu_map_data *map_data = NULL;
	struct msm_audio_smmu_vm_map_cmd smmu_map_cmd;
	unsigned long delay = jiffies + (HZ / 2);

	rc = ion_handle_get_size(client, handle, len);
	if (rc) {
		pr_err("%s: ion_handle_get_size failed, client = %pK, handle = %pK, rc = %d\n",
			__func__, client, handle, rc);
		goto err;
	}

	/* Data required to track per buffer mapping */
	map_data = kzalloc(sizeof(*map_data), GFP_KERNEL);
	if (!map_data) {
		rc = -ENOMEM;
		goto err;
	}

	/* Export the buffer to physical VM */
	rc = habmm_export(msm_audio_ion_hab_handle, vaddr, *len,
		&export_id, 0);
	if (rc) {
		pr_err("%s: habmm_export failed vaddr = %pK, len = %zd, rc = %d\n",
			__func__, vaddr, *len, rc);
		goto err;
	}

	exported = true;
	smmu_map_cmd.cmd_id = MSM_AUDIO_SMMU_VM_CMD_MAP;
	smmu_map_cmd.export_id = export_id;
	smmu_map_cmd.buf_size = *len;

	mutex_lock(&(msm_audio_ion_data.smmu_map_mutex));
	rc = habmm_socket_send(msm_audio_ion_hab_handle,
		(void *)&smmu_map_cmd, sizeof(smmu_map_cmd), 0);
	if (rc) {
		pr_err("%s: habmm_socket_send failed %d\n",
			__func__, rc);
		mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));
		goto err;
	}

	do {
		cmd_rsp_size = sizeof(cmd_rsp);
		rc = habmm_socket_recv(msm_audio_ion_hab_handle,
			(void *)&cmd_rsp,
			&cmd_rsp_size,
			0xFFFFFFFF,
			0);
	} while (time_before(jiffies, delay) && (rc == -EINTR) &&
			(cmd_rsp_size == 0));
	if (rc) {
		pr_err("%s: habmm_socket_recv failed %d\n",
			__func__, rc);
		mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));
		goto err;
	}
	mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));

	if (cmd_rsp_size != sizeof(cmd_rsp)) {
		pr_err("%s: invalid size for cmd rsp %u, expected %zu\n",
			__func__, cmd_rsp_size, sizeof(cmd_rsp));
		rc = -EIO;
		goto err;
	}

	if (cmd_rsp.status) {
		pr_err("%s: SMMU map command failed %d\n",
			__func__, cmd_rsp.status);
		rc = cmd_rsp.status;
		goto err;
	}

	*addr = (ion_phys_addr_t)cmd_rsp.addr;

	map_data->client = client;
	map_data->handle = handle;
	map_data->export_id = export_id;

	mutex_lock(&(msm_audio_ion_data.smmu_map_mutex));
	list_add_tail(&(map_data->list),
		&(msm_audio_ion_data.smmu_map_list));
	mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));

	return 0;

err:
	if (exported)
		(void)habmm_unexport(msm_audio_ion_hab_handle, export_id, 0);

	kfree(map_data);

	return rc;
}

static int msm_audio_ion_smmu_unmap(struct ion_client *client,
				struct ion_handle *handle)
{
	int rc;
	bool found = false;
	u32 cmd_rsp_size;
	struct msm_audio_smmu_vm_unmap_cmd_rsp cmd_rsp;
	struct msm_audio_smmu_map_data *map_data, *next;
	struct msm_audio_smmu_vm_unmap_cmd smmu_unmap_cmd;
	unsigned long delay = jiffies + (HZ / 2);

	/*
	 * Though list_for_each_entry_safe is delete safe, lock
	 * should be explicitly acquired to avoid race condition
	 * on adding elements to the list.
	 */
	mutex_lock(&(msm_audio_ion_data.smmu_map_mutex));
	list_for_each_entry_safe(map_data, next,
		&(msm_audio_ion_data.smmu_map_list), list) {

		if (map_data->handle == handle && map_data->client == client) {
			found = true;
			smmu_unmap_cmd.cmd_id = MSM_AUDIO_SMMU_VM_CMD_UNMAP;
			smmu_unmap_cmd.export_id = map_data->export_id;

			rc = habmm_socket_send(msm_audio_ion_hab_handle,
				(void *)&smmu_unmap_cmd,
				sizeof(smmu_unmap_cmd), 0);
			if (rc) {
				pr_err("%s: habmm_socket_send failed %d\n",
					__func__, rc);
				goto err;
			}

			do {
				cmd_rsp_size = sizeof(cmd_rsp);
				rc = habmm_socket_recv(msm_audio_ion_hab_handle,
					(void *)&cmd_rsp,
					&cmd_rsp_size,
					0xFFFFFFFF,
					0);
			} while (time_before(jiffies, delay) &&
					(rc == -EINTR) && (cmd_rsp_size == 0));
			if (rc) {
				pr_err("%s: habmm_socket_recv failed %d\n",
					__func__, rc);
				goto err;
			}

			if (cmd_rsp_size != sizeof(cmd_rsp)) {
				pr_err("%s: invalid size for cmd rsp %u\n",
					__func__, cmd_rsp_size);
				rc = -EIO;
				goto err;
			}

			if (cmd_rsp.status) {
				pr_err("%s: SMMU unmap command failed %d\n",
					__func__, cmd_rsp.status);
				rc = cmd_rsp.status;
				goto err;
			}

			rc = habmm_unexport(msm_audio_ion_hab_handle,
				map_data->export_id, 0xFFFFFFFF);
			if (rc) {
				pr_err("%s: habmm_unexport failed export_id = %d, rc = %d\n",
					__func__, map_data->export_id, rc);
			}

			list_del(&(map_data->list));
			kfree(map_data);
			break;
		}
	}
	mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));

	if (!found) {
		pr_err("%s: cannot find map_data ion_handle %pK, ion_client %pK\n",
			__func__, handle, client);
		rc = -EINVAL;
	}

	return rc;

err:
	if (found) {
		(void)habmm_unexport(msm_audio_ion_hab_handle,
			map_data->export_id, 0xFFFFFFFF);
		list_del(&(map_data->list));
		kfree(map_data);
	}

	mutex_unlock(&(msm_audio_ion_data.smmu_map_mutex));
	return rc;
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

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len, *vaddr);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_get_phys;
	}

	return rc;

err_get_phys:
	ion_unmap_kernel(*client, *handle);
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

	*vaddr = ion_map_kernel(*client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		rc = -ENOMEM;
		goto err_ion_handle;
	}
	pr_debug("%s: mapped address = %pK, size=%zd\n", __func__,
		*vaddr, bufsz);

	rc = msm_audio_ion_get_phys(*client, *handle, paddr, pa_len, *vaddr);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_get_phys;
	}

	return 0;

err_get_phys:
	ion_unmap_kernel(*client, *handle);
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
	int ret = 0;

	if (!client || !handle) {
		pr_err("%s Invalid params\n", __func__);
		return -EINVAL;
	}

	if (msm_audio_ion_data.smmu_enabled) {
		ret = msm_audio_ion_smmu_unmap(client, handle);
		if (ret)
			pr_err("%s: smmu unmap failed with ret %d\n",
				__func__, ret);
	}

	ion_unmap_kernel(client, handle);

	ion_free(client, handle);
	msm_audio_ion_client_destroy(client);
	return ret;
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

	/*Need to add condition SMMU enable or not */
	*vaddr = ion_map_kernel(client, *handle);
	if (IS_ERR_OR_NULL((void *)*vaddr)) {
		pr_err("%s: ION memory mapping for AUDIO failed\n", __func__);
		rc = -EINVAL;
		goto err_ion_handle;
	}

	if (bufsz != 0)
		memset((void *)*vaddr, 0, bufsz);

	rc = msm_audio_ion_get_phys(client, *handle, paddr, pa_len, *vaddr);
	if (rc) {
		pr_err("%s: ION Get Physical for AUDIO failed, rc = %d\n",
			__func__, rc);
		goto err_get_phys;
	}

	return 0;

err_get_phys:
	ion_unmap_kernel(client, *handle);
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
		ion_phys_addr_t *addr, size_t *len, void *vaddr)
{
	int rc = 0;

	pr_debug("%s: smmu_enabled = %d\n", __func__,
		msm_audio_ion_data.smmu_enabled);

	if (msm_audio_ion_data.smmu_enabled) {
		rc = msm_audio_ion_smmu_map(client, handle, addr, len, vaddr);
		if (rc) {
			pr_err("%s: failed to do smmu map, err = %d\n",
				__func__, rc);
			goto err;
		}
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

u32 msm_audio_populate_upper_32_bits(ion_phys_addr_t pa)
{
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

	if (smmu_enabled) {
		rc = habmm_socket_open(&msm_audio_ion_hab_handle,
			HAB_MMID_CREATE(MM_AUD_3,
				MSM_AUDIO_SMMU_VM_HAB_MINOR_ID),
			0xFFFFFFFF,
			HABMM_SOCKET_OPEN_FLAGS_SINGLE_BE_SINGLE_FE);
		if (rc) {
			pr_err("%s: habmm_socket_open failed %d\n",
				__func__, rc);
			return rc;
		}

		pr_info("%s: msm_audio_ion_hab_handle %x\n",
			__func__, msm_audio_ion_hab_handle);

		INIT_LIST_HEAD(&msm_audio_ion_data.smmu_map_list);
		mutex_init(&(msm_audio_ion_data.smmu_map_mutex));
	}

	if (!rc)
		msm_audio_ion_data.device_status |= MSM_AUDIO_ION_PROBED;

	return rc;
}

static int msm_audio_ion_remove(struct platform_device *pdev)
{
	if (msm_audio_ion_data.smmu_enabled) {
		if (msm_audio_ion_hab_handle)
			habmm_socket_close(msm_audio_ion_hab_handle);

		mutex_destroy(&(msm_audio_ion_data.smmu_map_mutex));
	}
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
