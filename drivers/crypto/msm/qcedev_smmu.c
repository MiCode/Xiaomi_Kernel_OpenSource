/* Qti (or) Qualcomm Technologies Inc CE device driver.
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <asm/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/qcedev.h>
#include "qcedevi.h"
#include "qcedev_smmu.h"
#include "soc/qcom/secure_buffer.h"

static bool compare_ion_buffers(struct qcedev_mem_client *mem_client,
					struct ion_handle *hndl, int fd);

static int qcedev_setup_context_bank(struct context_bank_info *cb,
				struct device *dev)
{
	int rc = 0;
	int secure_vmid = VMID_INVAL;
	struct bus_type *bus;

	if (!dev || !cb) {
		pr_err("%s err: invalid input params\n", __func__);
		return -EINVAL;
	}
	cb->dev = dev;

	bus = cb->dev->bus;
	if (IS_ERR_OR_NULL(bus)) {
		pr_err("%s err: failed to get bus type\n", __func__);
		rc = PTR_ERR(bus) ?: -ENODEV;
		goto remove_cb;
	}

	cb->mapping = arm_iommu_create_mapping(bus, cb->start_addr, cb->size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		pr_err("%s err: failed to create mapping\n", __func__);
		rc = PTR_ERR(cb->mapping) ?: -ENODEV;
		goto remove_cb;
	}

	if (cb->is_secure) {
		/* Hardcoded since we only have this vmid.*/
		secure_vmid = VMID_CP_BITSTREAM;
		rc = iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_SECURE_VMID, &secure_vmid);
		if (rc) {
			pr_err("%s err: programming secure vmid failed %s %d\n",
				__func__, dev_name(dev), rc);
			goto release_mapping;
		}
	}

	rc = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (rc) {
		pr_err("%s err: Failed to attach %s - %d\n",
			__func__, dev_name(dev), rc);
		goto release_mapping;
	}

	pr_info("%s Attached %s and create mapping\n", __func__, dev_name(dev));
	pr_info("%s Context Bank name:%s, is_secure:%d, start_addr:%#x\n",
			__func__, cb->name, cb->is_secure, cb->start_addr);
	pr_info("%s size:%#x, dev:%pK, mapping:%pK\n", __func__, cb->size,
			cb->dev, cb->mapping);
	return rc;

release_mapping:
	arm_iommu_release_mapping(cb->mapping);
remove_cb:
	return rc;
}

int qcedev_parse_context_bank(struct platform_device *pdev)
{
	struct qcedev_control *podev;
	struct context_bank_info *cb = NULL;
	struct device_node *np = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s err: invalid platform devices\n", __func__);
		return -EINVAL;
	}
	if (!pdev->dev.parent) {
		pr_err("%s err: failed to find a parent for %s\n",
			__func__, dev_name(&pdev->dev));
		return -EINVAL;
	}

	podev = dev_get_drvdata(pdev->dev.parent);
	np = pdev->dev.of_node;
	cb = devm_kzalloc(&pdev->dev, sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		pr_err("%s ERROR = Failed to allocate cb\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&cb->list);
	list_add_tail(&cb->list, &podev->context_banks);

	rc = of_property_read_string(np, "label", &cb->name);
	if (rc)
		pr_debug("%s ERROR = Unable to read label\n", __func__);

	rc = of_property_read_u32(np, "virtual-addr", &cb->start_addr);
	if (rc) {
		pr_err("%s err: cannot read virtual region addr %d\n",
			__func__, rc);
		goto err_setup_cb;
	}

	rc = of_property_read_u32(np, "virtual-size", &cb->size);
	if (rc) {
		pr_err("%s err: cannot read virtual region size %d\n",
			__func__, rc);
		goto err_setup_cb;
	}

	cb->is_secure = of_property_read_bool(np, "qcom,secure-context-bank");

	rc = qcedev_setup_context_bank(cb, &pdev->dev);
	if (rc) {
		pr_err("%s err: cannot setup context bank %d\n", __func__, rc);
		goto err_setup_cb;
	}

	return 0;

err_setup_cb:
	devm_kfree(&pdev->dev, cb);
	list_del(&cb->list);
	return rc;
}

struct qcedev_mem_client *qcedev_mem_new_client(enum qcedev_mem_type mtype)
{
	struct qcedev_mem_client *mem_client = NULL;
	struct ion_client *clnt = NULL;

	switch (mtype) {
	case MEM_ION:
		clnt = msm_ion_client_create("qcedev_client");
		if (!clnt)
			pr_err("%s: err: failed to allocate ion client\n",
				__func__);
		break;

	default:
		pr_err("%s: err: Mem type not supported\n", __func__);
	}

	if (clnt) {
		mem_client = kzalloc(sizeof(*mem_client), GFP_KERNEL);
		if (!mem_client)
			goto err;
		mem_client->mtype = mtype;
		mem_client->client = clnt;
	}

	return mem_client;

err:
	if (clnt)
		ion_client_destroy(clnt);
	return NULL;
}

void qcedev_mem_delete_client(struct qcedev_mem_client *mem_client)
{
	if (mem_client && mem_client->client)
		ion_client_destroy(mem_client->client);

	kfree(mem_client);
}

static bool is_iommu_present(struct qcedev_handle *qce_hndl)
{
	return !list_empty(&qce_hndl->cntl->context_banks);
}

static struct context_bank_info *get_context_bank(
		struct qcedev_handle *qce_hndl, bool is_secure)
{
	struct qcedev_control *podev = qce_hndl->cntl;
	struct context_bank_info *cb = NULL, *match = NULL;

	list_for_each_entry(cb, &podev->context_banks, list) {
		if (cb->is_secure == is_secure) {
			match = cb;
			break;
		}
	}
	return match;
}

static int ion_map_buffer(struct qcedev_handle *qce_hndl,
		struct qcedev_mem_client *mem_client, int fd,
		unsigned int fd_size, struct qcedev_reg_buf_info *binfo)
{
	struct ion_client *clnt = mem_client->client;
	struct ion_handle *hndl = NULL;
	unsigned long ion_flags = 0;
	int rc = 0;
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct context_bank_info *cb = NULL;

	buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(buf))
		return -EINVAL;

	hndl = ion_import_dma_buf(clnt, buf);
	if (IS_ERR_OR_NULL(hndl)) {
		pr_err("%s: err: invalid ion_handle\n", __func__);
		rc = -ENOMEM;
		goto import_buf_err;
	}

	rc = ion_handle_get_flags(clnt, hndl, &ion_flags);
	if (rc) {
		pr_err("%s: err: failed to get ion flags: %d\n", __func__, rc);
		goto map_err;
	}

	if (is_iommu_present(qce_hndl)) {
		cb = get_context_bank(qce_hndl, ion_flags & ION_FLAG_SECURE);
		if (!cb) {
			pr_err("%s: err: failed to get context bank info\n",
				__func__);
			rc = -EIO;
			goto map_err;
		}

		/* Prepare a dma buf for dma on the given device */
		attach = dma_buf_attach(buf, cb->dev);
		if (IS_ERR_OR_NULL(attach)) {
			rc = PTR_ERR(attach) ?: -ENOMEM;
			pr_err("%s: err: failed to attach dmabuf\n", __func__);
			goto map_err;
		}

		/* Get the scatterlist for the given attachment */
		table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(table)) {
			rc = PTR_ERR(table) ?: -ENOMEM;
			pr_err("%s: err: failed to map table\n", __func__);
			goto map_table_err;
		}

		/* Map a scatterlist into an SMMU */
		rc = msm_dma_map_sg_lazy(cb->dev, table->sgl, table->nents,
				DMA_BIDIRECTIONAL, buf);
		if (rc != table->nents) {
			pr_err(
				"%s: err: mapping failed with rc(%d), expected rc(%d)\n",
				__func__, rc, table->nents);
			rc = -ENOMEM;
			goto map_sg_err;
		}

		if (table->sgl) {
			binfo->ion_buf.iova = table->sgl->dma_address;
			binfo->ion_buf.mapped_buf_size = sg_dma_len(table->sgl);
			if (binfo->ion_buf.mapped_buf_size < fd_size) {
				pr_err("%s: err: mapping failed, size mismatch",
						__func__);
				rc = -ENOMEM;
				goto map_sg_err;
			}


		} else {
			pr_err("%s: err: sg list is NULL\n", __func__);
			rc = -ENOMEM;
			goto map_sg_err;
		}

		binfo->ion_buf.mapping_info.dev = cb->dev;
		binfo->ion_buf.mapping_info.mapping = cb->mapping;
		binfo->ion_buf.mapping_info.table = table;
		binfo->ion_buf.mapping_info.attach = attach;
		binfo->ion_buf.mapping_info.buf = buf;
		binfo->ion_buf.hndl = hndl;
	} else {
		pr_err("%s: err: smmu not enabled\n", __func__);
		rc = -EIO;
		goto map_err;
	}

	return 0;

map_sg_err:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_table_err:
	dma_buf_detach(buf, attach);
map_err:
	if (hndl)
		ion_free(clnt, hndl);
import_buf_err:
	dma_buf_put(buf);
	return rc;
}

static int ion_unmap_buffer(struct qcedev_handle *qce_hndl,
		struct qcedev_reg_buf_info *binfo)
{
	struct dma_mapping_info *mapping_info = &binfo->ion_buf.mapping_info;
	struct qcedev_mem_client *mem_client = qce_hndl->cntl->mem_client;

	if (is_iommu_present(qce_hndl)) {
		msm_dma_unmap_sg(mapping_info->dev, mapping_info->table->sgl,
			 mapping_info->table->nents, DMA_BIDIRECTIONAL,
			 mapping_info->buf);
		dma_buf_unmap_attachment(mapping_info->attach,
			mapping_info->table, DMA_BIDIRECTIONAL);
		dma_buf_detach(mapping_info->buf, mapping_info->attach);
		dma_buf_put(mapping_info->buf);

		if (binfo->ion_buf.hndl)
			ion_free(mem_client->client, binfo->ion_buf.hndl);

	}
	return 0;
}

static int qcedev_map_buffer(struct qcedev_handle *qce_hndl,
		struct qcedev_mem_client *mem_client, int fd,
		unsigned int fd_size, struct qcedev_reg_buf_info *binfo)
{
	int rc = 0;

	switch (mem_client->mtype) {
	case MEM_ION:
		rc = ion_map_buffer(qce_hndl, mem_client, fd, fd_size, binfo);
		break;
	default:
		pr_err("%s: err: Mem type not supported\n", __func__);
		break;
	}

	if (rc)
		pr_err("%s: err: failed to map buffer\n", __func__);

	return rc;
}

static int qcedev_unmap_buffer(struct qcedev_handle *qce_hndl,
		struct qcedev_mem_client *mem_client,
		struct qcedev_reg_buf_info *binfo)
{
	int rc = 0;

	switch (mem_client->mtype) {
	case MEM_ION:
		rc = ion_unmap_buffer(qce_hndl, binfo);
		break;
	default:
		pr_err("%s: err: Mem type not supported\n", __func__);
		break;
	}

	if (rc)
		pr_err("%s: err: failed to unmap buffer\n", __func__);

	return rc;
}

static bool compare_ion_buffers(struct qcedev_mem_client *mem_client,
		struct ion_handle *hndl, int fd)
{
	bool match = false;
	struct ion_handle *fd_hndl = NULL;
	struct dma_buf *dma_buf;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf))
		return false;

	fd_hndl = ion_import_dma_buf(mem_client->client, dma_buf);
	if (IS_ERR_OR_NULL(fd_hndl)) {
		match = false;
		goto err_exit;
	}

	match = fd_hndl == hndl ? true : false;

	if (fd_hndl)
		ion_free(mem_client->client, fd_hndl);
err_exit:
	dma_buf_put(dma_buf);
	return match;
}

int qcedev_check_and_map_buffer(void *handle,
		int fd, unsigned int offset, unsigned int fd_size,
		unsigned long long *vaddr)
{
	bool found = false;
	struct qcedev_reg_buf_info *binfo = NULL, *temp = NULL;
	struct qcedev_mem_client *mem_client = NULL;
	struct qcedev_handle *qce_hndl = handle;
	int rc = 0;
	unsigned long mapped_size = 0;

	if (!handle || !vaddr || fd < 0 || offset >= fd_size) {
		pr_err("%s: err: invalid input arguments\n", __func__);
		return -EINVAL;
	}

	if (!qce_hndl->cntl || !qce_hndl->cntl->mem_client) {
		pr_err("%s: err: invalid qcedev handle\n", __func__);
		return -EINVAL;
	}
	mem_client = qce_hndl->cntl->mem_client;

	if (mem_client->mtype != MEM_ION)
		return -EPERM;

	/* Check if the buffer fd is already mapped */
	mutex_lock(&qce_hndl->registeredbufs.lock);
	list_for_each_entry(temp, &qce_hndl->registeredbufs.list, list) {
		found = compare_ion_buffers(mem_client, temp->ion_buf.hndl, fd);
		if (found) {
			*vaddr = temp->ion_buf.iova;
			mapped_size = temp->ion_buf.mapped_buf_size;
			atomic_inc(&temp->ref_count);
			break;
		}
	}
	mutex_unlock(&qce_hndl->registeredbufs.lock);

	/* If buffer fd is not mapped then create a fresh mapping */
	if (!found) {
		pr_debug("%s: info: ion fd not registered with driver\n",
			__func__);
		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			pr_err("%s: err: failed to allocate binfo\n",
				__func__);
			rc = -ENOMEM;
			goto error;
		}
		rc = qcedev_map_buffer(qce_hndl, mem_client, fd,
							fd_size, binfo);
		if (rc) {
			pr_err("%s: err: failed to map fd (%d) error = %d\n",
				__func__, fd, rc);
			goto error;
		}

		*vaddr = binfo->ion_buf.iova;
		mapped_size = binfo->ion_buf.mapped_buf_size;
		atomic_inc(&binfo->ref_count);

		/* Add buffer mapping information to regd buffer list */
		mutex_lock(&qce_hndl->registeredbufs.lock);
		list_add_tail(&binfo->list, &qce_hndl->registeredbufs.list);
		mutex_unlock(&qce_hndl->registeredbufs.lock);
	}

	/* Make sure the offset is within the mapped range */
	if (offset >= mapped_size) {
		pr_err(
			"%s: err: Offset (%u) exceeds mapped size(%lu) for fd: %d\n",
			__func__, offset, mapped_size, fd);
		rc = -ERANGE;
		goto unmap;
	}

	/* return the mapped virtual address adjusted by offset */
	*vaddr += offset;

	return 0;

unmap:
	if (!found)
		qcedev_unmap_buffer(handle, mem_client, binfo);

error:
	kfree(binfo);
	return rc;
}

int qcedev_check_and_unmap_buffer(void *handle, int fd)
{
	struct qcedev_reg_buf_info *binfo = NULL, *dummy = NULL;
	struct qcedev_mem_client *mem_client = NULL;
	struct qcedev_handle *qce_hndl = handle;
	bool found = false;

	if (!handle || fd < 0) {
		pr_err("%s: err: invalid input arguments\n", __func__);
		return -EINVAL;
	}

	if (!qce_hndl->cntl || !qce_hndl->cntl->mem_client) {
		pr_err("%s: err: invalid qcedev handle\n", __func__);
		return -EINVAL;
	}
	mem_client = qce_hndl->cntl->mem_client;

	if (mem_client->mtype != MEM_ION)
		return -EPERM;

	/* Check if the buffer fd is mapped and present in the regd list. */
	mutex_lock(&qce_hndl->registeredbufs.lock);
	list_for_each_entry_safe(binfo, dummy,
		&qce_hndl->registeredbufs.list, list) {

		found = compare_ion_buffers(mem_client,
				binfo->ion_buf.hndl, fd);
		if (found) {
			atomic_dec(&binfo->ref_count);

			/* Unmap only if there are no more references */
			if (atomic_read(&binfo->ref_count) == 0) {
				qcedev_unmap_buffer(qce_hndl,
					mem_client, binfo);
				list_del(&binfo->list);
				kfree(binfo);
			}
			break;
		}
	}
	mutex_unlock(&qce_hndl->registeredbufs.lock);

	if (!found) {
		pr_err("%s: err: calling unmap on unknown fd %d\n",
			__func__, fd);
		return -EINVAL;
	}

	return 0;
}
