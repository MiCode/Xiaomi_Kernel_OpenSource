/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "CAM-SMMU %s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/qcom_iommu.h>
#include <linux/dma-buf.h>
#include <asm/dma-iommu.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/dma-attrs.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/qcom_iommu.h>
#include <linux/msm_iommu_domains.h>
#include "cam_smmu_api.h"

#define BYTE_SIZE 8
#define COOKIE_NUM_BYTE 2
#define COOKIE_SIZE (BYTE_SIZE*COOKIE_NUM_BYTE)
#define COOKIE_MASK ((1<<COOKIE_SIZE)-1)
#define HANDLE_INIT (-1)

#define GET_SMMU_HDL(x, y) (((x) << COOKIE_SIZE) | ((y) & COOKIE_MASK))
#define GET_SMMU_TABLE_IDX(x) (((x) >> COOKIE_SIZE) & COOKIE_MASK)

#ifdef CONFIG_CAM_SMMU_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

enum cam_protection_type {
	CAM_PROT_INVALID,
	CAM_NON_SECURE,
	CAM_SECURE,
	CAM_PROT_MAX,
};

enum cam_iommu_type {
	CAM_SMMU_INVALID,
	CAM_QSMMU,
	CAM_ARM_SMMU,
	CAM_SMMU_MAX,
};

enum cam_smmu_buf_state {
	CAM_SMMU_BUFF_EXIST,
	CAM_SMMU_BUFF_NOT_EXIST
};

struct cam_context_bank_info {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	dma_addr_t va_start;
	size_t va_len;
	const char *name;
	bool is_secure;
	struct list_head list_head;
	struct mutex lock;
	int handle;
	enum cam_smmu_ops_param state;
	int (*fault_handler)(struct iommu_domain *,
		struct device *, unsigned long,
		int, void*);
	void *token;
};

struct cam_iommu_cb_set {
	struct cam_context_bank_info *cb_info;
	u32 cb_num;
	u32 cb_init_count;
};

static struct of_device_id msm_cam_smmu_dt_match[] = {
	{ .compatible = "qcom,msm-cam-smmu", },
	{ .compatible = "qcom,msm-cam-smmu-cb", },
	{ .compatible = "qcom,qsmmu-cam-cb", },
	{}
};

struct cam_dma_buff_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	enum dma_data_direction dir;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int ion_fd;
	size_t len;
};

static struct cam_iommu_cb_set iommu_cb_set;

static enum dma_data_direction cam_smmu_translate_dir(
	enum cam_smmu_map_dir dir);
static int cam_smmu_check_handle_unique(int hdl);
static int cam_smmu_create_iommu_handle(int idx);
static int cam_smmu_create_add_handle_in_table(char *name,
	int *hdl);
static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int ion_fd);
static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
	enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr);
static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info, int idx);
static void cam_smmu_clean_buffer_list(int idx);
static void cam_smmu_init_iommu_table(void);
static void cam_smmu_print_list(int idx);
static void cam_smmu_print_table(void);
static int cam_smmu_probe(struct platform_device *pdev);

static void cam_smmu_print_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	pr_err("index = %d ", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].list_head, list) {
		pr_err("ion_fd = %d, paddr= 0x%p, len = %u\n",
			 mapping->ion_fd, (void *)mapping->paddr,
			 (unsigned int)mapping->len);
	}
}

static void cam_smmu_print_table(void)
{
	int i;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		pr_err("i= %d, handle= %d, name_addr=%p\n", i,
			   (int)iommu_cb_set.cb_info[i].handle,
			   (void *)iommu_cb_set.cb_info[i].name);
		pr_err("dev = %p ", iommu_cb_set.cb_info[i].dev);
	}
}

static void cam_smmu_check_vaddr_in_range(int idx, void *vaddr)
{
	struct cam_dma_buff_info *mapping;
	unsigned long start_addr, end_addr, current_addr;

	current_addr = (unsigned long)vaddr;
	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].list_head, list) {
		start_addr = (unsigned long)mapping->paddr;
		end_addr = (unsigned long)mapping->paddr + mapping->len;

		if (start_addr <= current_addr && current_addr <= end_addr) {
			pr_err("Error: vaddr %p is valid: range:%p-%p, ion_fd = %d\n",
				vaddr, (void *)start_addr, (void *)end_addr,
				mapping->ion_fd);
			return;
		} else {
			pr_err("vaddr %p is not in this range: %p-%p, ion_fd = %d\n",
				vaddr, (void *)start_addr, (void *)end_addr,
				mapping->ion_fd);
		}
	}
	pr_err("Cannot find vaddr:%p in SMMU. %s uses invalid virtual addreess\n",
		vaddr, iommu_cb_set.cb_info[idx].name);
	return;
}

void cam_smmu_reg_client_page_fault_handler(int handle,
		int (*client_page_fault_handler)(struct iommu_domain *,
		struct device *, unsigned long,
		int, void*), void *token)
{
	int idx;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d, handle = 0x%x\n",
				idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	iommu_cb_set.cb_info[idx].token = token;
	iommu_cb_set.cb_info[idx].fault_handler =
		client_page_fault_handler;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return;

}

static int cam_smmu_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova,
		int flags, void *token)
{
	char *cb_name;
	int i, rc;

	if (!token) {
		pr_err("Error: token is NULL\n");
		return -ENOSYS;
	}

	cb_name = (char *)token;
	/* check wether it is in the table */
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (!strcmp(iommu_cb_set.cb_info[i].name, cb_name)) {
			mutex_lock(&iommu_cb_set.cb_info[i].lock);
			if (!(iommu_cb_set.cb_info[i].fault_handler)) {
				pr_err("Error: %s: %p has page fault\n",
						(char *)token,
						(void *)iova);
				cam_smmu_check_vaddr_in_range(i,
						(void *)iova);
				rc = -ENOSYS;
			} else {
				rc = iommu_cb_set.cb_info[i].fault_handler(
					domain, dev, iova, flags,
					iommu_cb_set.cb_info[i].token);
			}
			mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			return rc;
		}
	}
	pr_err("Error: cb_name %s is not valid.\n", (char *)token);
	return -ENOSYS;
}

static enum dma_data_direction cam_smmu_translate_dir(
				enum cam_smmu_map_dir dir)
{
	switch (dir) {
	case CAM_SMMU_MAP_READ:
		return DMA_FROM_DEVICE;
	case CAM_SMMU_MAP_WRITE:
		return DMA_TO_DEVICE;
	case CAM_SMMU_MAP_RW:
		return DMA_BIDIRECTIONAL;
	case CAM_SMMU_MAP_INVALID:
	default:
		pr_err("Error: Direction is invalid. dir = %d\n", (int)dir);
		break;
	}
	return DMA_NONE;
}

void cam_smmu_init_iommu_table(void)
{
	unsigned int i;
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_cb_set.cb_info[i].handle = HANDLE_INIT;
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].list_head);
		iommu_cb_set.cb_info[i].state = CAM_SMMU_DETACH;
		iommu_cb_set.cb_info[i].dev = NULL;
		iommu_cb_set.cb_info[i].token = NULL;
		iommu_cb_set.cb_info[i].fault_handler = NULL;
		mutex_init(&iommu_cb_set.cb_info[i].lock);
	}

	return;
}

void cam_smmu_deinit_iommu_table(void)
{
	unsigned int i;
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_cb_set.cb_info[i].handle = HANDLE_INIT;
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].list_head);
		iommu_cb_set.cb_info[i].state = CAM_SMMU_DETACH;
		iommu_cb_set.cb_info[i].dev = NULL;
		iommu_cb_set.cb_info[i].token = NULL;
		iommu_cb_set.cb_info[i].fault_handler = NULL;
		mutex_destroy(&iommu_cb_set.cb_info[i].lock);
	}

	return;
}

static int cam_smmu_check_handle_unique(int hdl)
{
	int i;

	if (hdl == HANDLE_INIT) {
		CDBG("iommu handle is init number. Need to try again\n");
		return 1;
	}

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (iommu_cb_set.cb_info[i].handle == HANDLE_INIT)
			continue;

		if (iommu_cb_set.cb_info[i].handle == hdl) {
			CDBG("iommu handle %d conflicts\n", (int)hdl);
			return 1;
		}
	}
	return 0;
}

/**
 *  use low 2 bytes for handle cookie
 */
static int cam_smmu_create_iommu_handle(int idx)
{
	int rand, hdl = 0;
	get_random_bytes(&rand, COOKIE_NUM_BYTE);
	hdl = GET_SMMU_HDL(idx, rand);
	CDBG("create handle value = %x\n", (int)hdl);
	return hdl;
}

static int cam_smmu_attach_device(int idx)
{
	int rc;
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	/* attach the mapping to device */
	rc = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (rc < 0) {
		pr_err("Error: ARM IOMMU attach failed. ret = %d\n", rc);
		return -ENODEV;
	}
	return rc;
}

static void cam_smmu_detach_device(int idx)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	arm_iommu_detach_device(cb->dev);
	return;
}

static int cam_smmu_create_add_handle_in_table(char *name,
					int *hdl)
{
	int i;
	int handle;

	/* create handle and add in the iommu hardware table */
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (!strcmp(iommu_cb_set.cb_info[i].name, name)) {
			mutex_lock(&iommu_cb_set.cb_info[i].lock);
			if (iommu_cb_set.cb_info[i].handle != HANDLE_INIT) {
				pr_err("Error: %s already get handle 0x%x\n",
						name,
						iommu_cb_set.cb_info[i].handle);
				mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			}

			/* make sure handle is unique */
			do {
				handle = cam_smmu_create_iommu_handle(i);
			} while (cam_smmu_check_handle_unique(handle));

			/* put handle in the table */
			iommu_cb_set.cb_info[i].handle = handle;
			*hdl = handle;
			CDBG("%s creates handle 0x%x\n", name, handle);
			mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			return 0;
		}
	}

	/* if i == iommu_cb_set.cb_num */
	pr_err("Error: Cannot find name %s or all handle exist!\n",
			name);
	cam_smmu_print_table();
	return -EINVAL;
}

int cam_smmu_find_index_by_handle(int hdl)
{
	int idx;
	CDBG("find handle %x\n", (int)hdl);
	idx = GET_SMMU_TABLE_IDX(hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d\n", idx);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].handle != hdl) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, hdl);
		return -EINVAL;
	}

	return idx;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
		int ion_fd)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].list_head,
			list) {
		if (mapping->ion_fd == ion_fd) {
			CDBG(" find ion_fd %d\n", ion_fd);
			return mapping;
		}
	}

	pr_err("Error: Cannot find fd %d by index %d\n",
		ion_fd, idx);
	return NULL;
}

static void cam_smmu_clean_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
				&iommu_cb_set.cb_info[idx].list_head, list) {
		CDBG("Free mapping address %p, i = %d, fd = %d\n",
			 (void *)mapping_info->paddr, idx,
			mapping_info->ion_fd);
		ret = cam_smmu_unmap_buf_and_remove_from_list(mapping_info,
				idx);
		if (ret < 0) {
			pr_err("Error: Deleting one buffer failed\n");
			/*
			 * Ignore this error and continue to delete other
			 * buffers in the list
			 */
			continue;
		}
	}
}

static int cam_smmu_attach(int idx)
{
	int ret;

	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		pr_err("Error: index %d got attached before\n",
			idx);
		ret = -EINVAL;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		ret = cam_smmu_attach_device(idx);
		if (ret < 0) {
			pr_err("Error: ATTACH fail\n");
			return -ENODEV;
		}
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_ATTACH;
		ret = 0;
	} else {
		pr_err("Error: Not detach/attach\n");
		ret = -EINVAL;
	}
	return ret;
}

static int cam_smmu_detach(int idx)
{
	int ret;

	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		pr_err("Error: Index %d got detached before\n", idx);
		ret = -EINVAL;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_DETACH;
		cam_smmu_clean_buffer_list(idx);
		cam_smmu_detach_device(idx);
		return 0;
	} else {
		pr_err("Error: Not detach/attach\n");
		ret = -EINVAL;
	}
	return ret;
}

static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
		 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
		 size_t *len_ptr)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info;
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;

	/* allocate memory for each buffer information */
	buf = dma_buf_get(ion_fd);
	if (IS_ERR_OR_NULL(buf)) {
		rc = PTR_ERR(buf);
		pr_err("Error: dma get buf failed\n");
		goto err_out;
	}

	attach = dma_buf_attach(buf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach);
		pr_err("Error: dma buf attach failed\n");
		goto err_put;
	}

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table);
		pr_err("Error: dma buf map attachment failed\n");
		goto err_detach;
	}

	rc = dma_map_sg(iommu_cb_set.cb_info[idx].dev, table->sgl,
			table->nents, dma_dir);
	if (!rc) {
		pr_err("Error: dma_map_sg failed\n");
		goto err_unmap_sg;
	}

	if (table->sgl) {
		CDBG("DMA buf: %p, device: %p, attach: %p, table: %p\n",
				(void *)buf,
				(void *)iommu_cb_set.cb_info[idx].dev,
				(void *)attach, (void *)table);
		CDBG("table sgl: %p, rc: %d, dma_address: 0x%x\n",
				(void *)table->sgl, rc,
				(unsigned int)table->sgl->dma_address);
	} else {
		rc = -EINVAL;
		pr_err("Error: table sgl is null\n");
		goto err_unmap_sg;
	}

	/* fill up mapping_info */
	mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (!mapping_info) {
		pr_err("Error: No enough space!\n");
		rc = -ENOSPC;
		goto err_unmap_sg;
	}
	mapping_info->ion_fd = ion_fd;
	mapping_info->buf = buf;
	mapping_info->attach = attach;
	mapping_info->table = table;
	mapping_info->paddr = sg_dma_address(table->sgl);
	mapping_info->len = (size_t)sg_dma_len(table->sgl);
	mapping_info->dir = dma_dir;
	mapping_info->ref_count = 1;

	/* return paddr and len to client */
	*paddr_ptr = sg_dma_address(table->sgl);
	*len_ptr = (size_t)sg_dma_len(table->sgl);

	if (!paddr_ptr) {
		pr_err("Error: Space Allocation failed!\n");
		rc = -ENOSPC;
		goto err_unmap_sg;
	}
	CDBG("ion_fd = %d, dev = %p, paddr= %p, len = %u\n", ion_fd,
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)*paddr_ptr, (unsigned int)*len_ptr);

	/* add to the list */
	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].list_head);
	return 0;

err_unmap_sg:
	dma_buf_unmap_attachment(attach, table, dma_dir);
err_detach:
	dma_buf_detach(buf, attach);
err_put:
	dma_buf_put(buf);
err_out:
	return rc;
}

static int cam_smmu_unmap_buf_and_remove_from_list(
		struct cam_dma_buff_info *mapping_info,
		int idx)
{
	if ((!mapping_info->buf) || (!mapping_info->table) ||
		(!mapping_info->attach)) {
		pr_err("Error: Invalid params dev = %p, table = %p",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		pr_err("Error:dma_buf = %p, attach = %p\n",
			(void *)mapping_info->buf,
			(void *)mapping_info->attach);
		return -EINVAL;
	}

	/* iommu buffer clean up */
	dma_unmap_sg(iommu_cb_set.cb_info[idx].dev, mapping_info->table->sgl,
		mapping_info->table->nents, mapping_info->dir);
	dma_buf_unmap_attachment(mapping_info->attach,
		mapping_info->table, mapping_info->dir);
	dma_buf_detach(mapping_info->buf, mapping_info->attach);
	dma_buf_put(mapping_info->buf);
	mapping_info->buf = NULL;

	list_del_init(&mapping_info->list);

	/* free one buffer */
	kfree(mapping_info);
	return 0;
}

static enum cam_smmu_buf_state cam_smmu_check_fd_in_list(int idx,
					int ion_fd, dma_addr_t *paddr_ptr,
					size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;
	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].list_head,
			list) {
		if (mapping->ion_fd == ion_fd) {
			mapping->ref_count++;
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}
	return CAM_SMMU_BUFF_NOT_EXIST;
}

int cam_smmu_get_handle(char *identifier, int *handle_ptr)
{
	int ret = 0;

	if (!identifier) {
		pr_err("Error: iommu harware name is NULL\n");
		return -EFAULT;
	}

	if (!handle_ptr) {
		pr_err("Error: handle pointer is NULL\n");
		return -EFAULT;
	}

	/* create and put handle in the table */
	ret = cam_smmu_create_add_handle_in_table(identifier, handle_ptr);
	if (ret < 0) {
		pr_err("Error: %s gets handle fail\n", identifier);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL(cam_smmu_get_handle);

int cam_smmu_ops(int handle, enum cam_smmu_ops_param ops)
{
	int ret = 0, idx;

	CDBG("E: ops = %d\n", ops);
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d\n", idx);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	switch (ops) {
	case CAM_SMMU_ATTACH: {
		ret = cam_smmu_attach(idx);
		break;
	}
	case CAM_SMMU_DETACH: {
		ret = cam_smmu_detach(idx);
		break;
	}
	case CAM_SMMU_VOTE:
	case CAM_SMMU_DEVOTE:
	default:
		pr_err("Error: idx = %d, ops = %d\n", idx, ops);
		ret = -EINVAL;
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return ret;
}
EXPORT_SYMBOL(cam_smmu_ops);

int cam_smmu_get_phy_addr(int handle, int ion_fd,
		enum cam_smmu_map_dir dir, dma_addr_t *paddr_ptr,
		size_t *len_ptr)
{
	int idx, rc;
	enum dma_data_direction dma_dir;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		pr_err("Error: Input pointers are invalid\n");
		return -EINVAL;
	}
	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		pr_err("Error: translate direction failed. dir = %d\n", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d\n", idx);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, ion_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CDBG("ion_fd:%d already in the list, give same addr back",
				 ion_fd);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return 0;
	}
	rc = cam_smmu_map_buffer_and_add_to_list(idx, ion_fd, dma_dir,
			paddr_ptr, len_ptr);
	if (rc < 0) {
		pr_err("Error: mapping or add list fail\n");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return rc;
	}

	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_get_phy_addr);

int cam_smmu_put_phy_addr(int handle, int ion_fd)
{
	int idx;
	int ret = -1;
	struct cam_dma_buff_info *mapping_info;

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d.\n", idx);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	/* based on ion fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, ion_fd);
	if (!mapping_info) {
		pr_err("Error: Invalid params\n");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	mapping_info->ref_count--;
	if (mapping_info->ref_count > 0) {
		CDBG("There are still %u buffer(s) with same fd %d",
			mapping_info->ref_count, mapping_info->ion_fd);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return 0;
	}

	/* unmapping one buffer from device */
	ret = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (ret < 0) {
		pr_err("Error: unmap or remove list fail\n");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return ret;
	}

	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_put_phy_addr);

int cam_smmu_destroy_handle(int handle)
{
	int idx, ret;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d\n", idx);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (!list_empty_careful(&iommu_cb_set.cb_info[idx].list_head)) {
		pr_err("List is not clean!\n");
		cam_smmu_print_list(idx);
		cam_smmu_clean_buffer_list(idx);
	}

	iommu_cb_set.cb_info[idx].handle = HANDLE_INIT;
	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		pr_err("It should get detached before.\n");
		ret = cam_smmu_detach(idx);
		if (ret < 0) {
			pr_err("Error: Detach idx %d fail\n", idx);
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -EINVAL;
		}
	}

	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_destroy_handle);

/*This function can only be called after smmu driver probe*/
int cam_smmu_get_num_of_clients(void)
{
	return iommu_cb_set.cb_num;
}

static void cam_smmu_release_cb(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		arm_iommu_detach_device(iommu_cb_set.cb_info[i].dev);
		arm_iommu_release_mapping(iommu_cb_set.cb_info[i].mapping);
	}

	devm_kfree(&pdev->dev, iommu_cb_set.cb_info);
	iommu_cb_set.cb_num = 0;
}

static int cam_smmu_setup_cb(struct cam_context_bank_info *cb,
	struct device *dev)
{
	int rc = 0;
	int order = 0;
	int disable_htw = 1;

	if (!cb || !dev) {
		pr_err("Error: invalid input params\n");
		return -EINVAL;
	}

	cb->dev = dev;
	cb->va_start = SZ_128K;
	cb->va_len = SZ_2G - SZ_128K;

	/* create a virtual mapping */
	cb->mapping = arm_iommu_create_mapping(&platform_bus_type,
		cb->va_start, cb->va_len, order);
	if (IS_ERR(cb->mapping)) {
		pr_err("Error: create mapping Failed\n");
		rc = -ENODEV;
		goto end;
	}

	/*
	 * Set the domain attributes
	 * disable L2 redirect since it decreases
	 * performance
	 */
	if (iommu_domain_set_attr(cb->mapping->domain,
		DOMAIN_ATTR_COHERENT_HTW_DISABLE,
		&disable_htw)) {
		pr_err("Error: couldn't disable coherent HTW\n");
		rc = -ENODEV;
		goto err_set_attr;
	}
	return 0;
err_set_attr:
	arm_iommu_release_mapping(cb->mapping);
end:
	return rc;
}

static int cam_alloc_smmu_context_banks(struct device *dev)
{
	struct device_node *domains_child_node = NULL;
	if (!dev) {
		pr_err("Error: Invalid device\n");
		return -ENODEV;
	}

	iommu_cb_set.cb_num = 0;

	/* traverse thru all the child nodes and increment the cb count */
	for_each_child_of_node(dev->of_node, domains_child_node) {
		if (of_device_is_compatible(domains_child_node,
			"qcom,msm-cam-smmu-cb"))
			iommu_cb_set.cb_num++;

		if (of_device_is_compatible(domains_child_node,
			"qcom,qsmmu-cam-cb"))
			iommu_cb_set.cb_num++;
	}

	if (iommu_cb_set.cb_num == 0) {
		pr_err("Error: no context banks present\n");
		return -ENOENT;
	}

	/* allocate memory for the context banks */
	iommu_cb_set.cb_info = devm_kzalloc(dev,
		iommu_cb_set.cb_num * sizeof(struct cam_context_bank_info),
		GFP_KERNEL);

	if (!iommu_cb_set.cb_info) {
		pr_err("Error: cannot allocate context banks\n");
		return -ENOMEM;
	}

	cam_smmu_init_iommu_table();
	iommu_cb_set.cb_init_count = 0;

	CDBG("no of context banks :%d\n", iommu_cb_set.cb_num);
	return 0;
}

static int cam_populate_smmu_context_banks(struct device *dev,
	enum cam_iommu_type type)
{
	int rc = 0;
	struct cam_context_bank_info *cb;
	struct device *ctx;

	if (!dev) {
		pr_err("Error: Invalid device\n");
		return -ENODEV;
	}

	/* check the bounds */
	if (iommu_cb_set.cb_init_count >= iommu_cb_set.cb_num) {
		pr_err("Error: populate more than allocated cb\n");
		rc = -EBADHANDLE;
		goto cb_init_fail;
	}

	/* read the context bank from cb set */
	cb = &iommu_cb_set.cb_info[iommu_cb_set.cb_init_count];

	/* set the name of the context bank */
	rc = of_property_read_string(dev->of_node, "label", &cb->name);
	if (rc) {
		pr_err("Error: failed to read label from sub device\n");
		goto cb_init_fail;
	}

	/* set the secure/non secure domain type */
	if (of_property_read_bool(dev->of_node, "qcom,secure-context"))
		cb->is_secure = CAM_SECURE;
	else
		cb->is_secure = CAM_NON_SECURE;

	CDBG("cb->name : %s, cb->is_secure :%d\n", cb->name, cb->is_secure);

	/* set up the iommu mapping for the  context bank */

	if (type == CAM_QSMMU) {
		ctx = msm_iommu_get_ctx(cb->name);
		pr_info("getting QSMMU ctx : %s\n", cb->name);
	} else {
		ctx = dev;
		pr_info("getting Arm SMMU ctx : %s\n", cb->name);
	}

	rc = cam_smmu_setup_cb(cb, ctx);
	if (rc < 0)
		pr_err("Error: failed to setup cb : %s\n", cb->name);

	iommu_set_fault_handler(cb->mapping->domain,
			cam_smmu_iommu_fault_handler,
			(void *)cb->name);

	/* increment count to next bank */
	iommu_cb_set.cb_init_count++;

	CDBG("X: cb init count :%d\n", iommu_cb_set.cb_init_count);
	return rc;

cb_init_fail:
	iommu_cb_set.cb_info = NULL;
	return rc;
}

static int cam_smmu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device *dev = &pdev->dev;

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu")) {
		rc = cam_alloc_smmu_context_banks(dev);
		if (rc < 0)	{
			pr_err("Error: allocating context banks\n");
			return -ENOMEM;
		}
	}
	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_ARM_SMMU);
		if (rc < 0) {
			pr_err("Error: populating context banks\n");
			return -ENOMEM;
		}
		return rc;
	}
	if (of_device_is_compatible(dev->of_node, "qcom,qsmmu-cam-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_QSMMU);
		if (rc < 0) {
			pr_err("Error: populating context banks\n");
			return -ENOMEM;
		}
		return rc;
	}

	/* probe thru all the subdevices */
	rc = of_platform_populate(pdev->dev.of_node, msm_cam_smmu_dt_match,
				NULL, &pdev->dev);
	if (rc < 0)
		pr_err("Error: populating devices\n");
	return rc;
}

static int cam_smmu_remove(struct platform_device *pdev)
{
	/* release all the context banks and memory allocated */
	cam_smmu_deinit_iommu_table();
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cam-smmu"))
		cam_smmu_release_cb(pdev);
	return 0;
}
static struct platform_driver cam_smmu_driver = {
	.probe = cam_smmu_probe,
	.remove = cam_smmu_remove,
	.driver = {
		.name = "msm_cam_smmu",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_smmu_dt_match,
	},
};

static int __init cam_smmu_init_module(void)
{
	return platform_driver_register(&cam_smmu_driver);
}

static void __exit cam_smmu_exit_module(void)
{
	platform_driver_unregister(&cam_smmu_driver);
}

module_init(cam_smmu_init_module);
module_exit(cam_smmu_exit_module);
MODULE_DESCRIPTION("MSM Camera SMMU driver");
MODULE_LICENSE("GPL v2");

