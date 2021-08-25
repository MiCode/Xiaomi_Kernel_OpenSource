/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/dma-buf.h>
#include <asm/dma-iommu.h>
#include <linux/dma-direction.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/workqueue.h>
#include <linux/genalloc.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>
#include <uapi/media/cam_req_mgr.h>
#include <linux/debugfs.h>
#include "cam_smmu_api.h"
#include "cam_debug_util.h"

#define SHARED_MEM_POOL_GRANULARITY 16

#define IOMMU_INVALID_DIR -1
#define BYTE_SIZE 8
#define COOKIE_NUM_BYTE 2
#define COOKIE_SIZE (BYTE_SIZE*COOKIE_NUM_BYTE)
#define COOKIE_MASK ((1<<COOKIE_SIZE)-1)
#define HANDLE_INIT (-1)
#define CAM_SMMU_CB_MAX 5

#define GET_SMMU_HDL(x, y) (((x) << COOKIE_SIZE) | ((y) & COOKIE_MASK))
#define GET_SMMU_TABLE_IDX(x) (((x) >> COOKIE_SIZE) & COOKIE_MASK)

static int g_num_pf_handled = 4;
module_param(g_num_pf_handled, int, 0644);

struct firmware_alloc_info {
	struct device *fw_dev;
	void *fw_kva;
	dma_addr_t fw_dma_hdl;
};

struct firmware_alloc_info icp_fw;

struct cam_smmu_work_payload {
	int idx;
	struct iommu_domain *domain;
	struct device *dev;
	unsigned long iova;
	int flags;
	void *token;
	struct list_head list;
};

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
	CAM_SMMU_BUFF_NOT_EXIST,
};

enum cam_smmu_init_dir {
	CAM_SMMU_TABLE_INIT,
	CAM_SMMU_TABLE_DEINIT,
};

struct scratch_mapping {
	void *bitmap;
	size_t bits;
	unsigned int order;
	dma_addr_t base;
};

struct secheap_buf_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
};

struct cam_context_bank_info {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	dma_addr_t va_start;
	size_t va_len;
	const char *name;
	bool is_secure;
	uint8_t scratch_buf_support;
	uint8_t firmware_support;
	uint8_t shared_support;
	uint8_t io_support;
	uint8_t secheap_support;
	uint8_t qdss_support;
	dma_addr_t qdss_phy_addr;
	bool is_fw_allocated;
	bool is_secheap_allocated;
	bool is_qdss_allocated;

	struct scratch_mapping scratch_map;
	struct gen_pool *shared_mem_pool;

	struct cam_smmu_region_info scratch_info;
	struct cam_smmu_region_info firmware_info;
	struct cam_smmu_region_info shared_info;
	struct cam_smmu_region_info io_info;
	struct cam_smmu_region_info secheap_info;
	struct cam_smmu_region_info qdss_info;
	struct secheap_buf_info secheap_buf;

	struct list_head smmu_buf_list;
	struct list_head smmu_buf_kernel_list;
	struct mutex lock;
	int handle;
	enum cam_smmu_ops_param state;

	cam_smmu_client_page_fault_handler handler[CAM_SMMU_CB_MAX];
	void *token[CAM_SMMU_CB_MAX];
	int cb_count;
	int secure_count;
	int pf_count;
};

struct cam_iommu_cb_set {
	struct cam_context_bank_info *cb_info;
	u32 cb_num;
	u32 cb_init_count;
	struct work_struct smmu_work;
	struct mutex payload_list_lock;
	struct list_head payload_list;
	u32 non_fatal_fault;
};

static const struct of_device_id msm_cam_smmu_dt_match[] = {
	{ .compatible = "qcom,msm-cam-smmu", },
	{ .compatible = "qcom,msm-cam-smmu-cb", },
	{ .compatible = "qcom,msm-cam-smmu-fw-dev", },
	{}
};

struct cam_dma_buff_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	enum dma_data_direction dir;
	enum cam_smmu_region_id region_id;
	int iommu_dir;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int ion_fd;
	size_t len;
	size_t phys_len;
};

struct cam_sec_buff_info {
	struct dma_buf *buf;
	enum dma_data_direction dir;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int ion_fd;
	size_t len;
};

static const char *qdss_region_name = "qdss";

static struct cam_iommu_cb_set iommu_cb_set;

static struct dentry *smmu_dentry;

static bool smmu_fatal_flag;

static enum dma_data_direction cam_smmu_translate_dir(
	enum cam_smmu_map_dir dir);

static int cam_smmu_check_handle_unique(int hdl);

static int cam_smmu_create_iommu_handle(int idx);

static int cam_smmu_create_add_handle_in_table(char *name,
	int *hdl);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int ion_fd);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_dma_buf(int idx,
	struct dma_buf *buf);

static struct cam_sec_buff_info *cam_smmu_find_mapping_by_sec_buf_idx(int idx,
	int ion_fd);

static int cam_smmu_init_scratch_map(struct scratch_mapping *scratch_map,
	dma_addr_t base, size_t size,
	int order);

static int cam_smmu_alloc_scratch_va(struct scratch_mapping *mapping,
	size_t size,
	dma_addr_t *iova);

static int cam_smmu_free_scratch_va(struct scratch_mapping *mapping,
	dma_addr_t addr, size_t size);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_virt_address(int idx,
	dma_addr_t virt_addr);

static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
	enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id);

static int cam_smmu_map_kernel_buffer_and_add_to_list(int idx,
	struct dma_buf *buf, enum dma_data_direction dma_dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id);

static int cam_smmu_alloc_scratch_buffer_add_to_list(int idx,
	size_t virt_len,
	size_t phys_len,
	unsigned int iommu_dir,
	dma_addr_t *virt_addr);

static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info, int idx);

static int cam_smmu_free_scratch_buffer_remove_from_list(
	struct cam_dma_buff_info *mapping_info,
	int idx);

static void cam_smmu_clean_user_buffer_list(int idx);

static void cam_smmu_clean_kernel_buffer_list(int idx);

static void cam_smmu_print_user_list(int idx);

static void cam_smmu_print_kernel_list(int idx);

static void cam_smmu_print_table(void);

static int cam_smmu_probe(struct platform_device *pdev);

static uint32_t cam_smmu_find_closest_mapping(int idx, void *vaddr);

static void cam_smmu_page_fault_work(struct work_struct *work)
{
	int j;
	int idx;
	struct cam_smmu_work_payload *payload;
	uint32_t buf_info;

	mutex_lock(&iommu_cb_set.payload_list_lock);
	if (list_empty(&iommu_cb_set.payload_list)) {
		CAM_ERR(CAM_SMMU, "Payload list empty");
		mutex_unlock(&iommu_cb_set.payload_list_lock);
		return;
	}

	payload = list_first_entry(&iommu_cb_set.payload_list,
		struct cam_smmu_work_payload,
		list);
	list_del(&payload->list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	/* Dereference the payload to call the handler */
	idx = payload->idx;
	buf_info = cam_smmu_find_closest_mapping(idx, (void *)payload->iova);
	if (buf_info != 0) {
		CAM_INFO(CAM_SMMU, "closest buf 0x%x idx %d", buf_info, idx);
	}

	for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
		if ((iommu_cb_set.cb_info[idx].handler[j])) {
			iommu_cb_set.cb_info[idx].handler[j](
				payload->domain,
				payload->dev,
				payload->iova,
				payload->flags,
				iommu_cb_set.cb_info[idx].token[j],
				buf_info);
		}
	}
	kfree(payload);
}

static int cam_smmu_create_debugfs_entry(void)
{
	int rc = 0;

	smmu_dentry = debugfs_create_dir("camera_smmu", NULL);
	if (!smmu_dentry)
		return -ENOMEM;

	if (!debugfs_create_bool("cam_smmu_fatal",
		0644,
		smmu_dentry,
		&smmu_fatal_flag)) {
		CAM_ERR(CAM_SMMU, "failed to create cam_smmu_fatal entry");
		rc = -ENOMEM;
		goto err;
	}

	return rc;
err:
	debugfs_remove_recursive(smmu_dentry);
	smmu_dentry = NULL;
	return rc;
}

static void cam_smmu_print_user_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	CAM_ERR(CAM_SMMU, "index = %d", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		CAM_ERR(CAM_SMMU,
			"ion_fd = %d, paddr= 0x%pK, len = %u, region = %d",
			 mapping->ion_fd, (void *)mapping->paddr,
			 (unsigned int)mapping->len,
			 mapping->region_id);
	}
}

static void cam_smmu_print_kernel_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	CAM_ERR(CAM_SMMU, "index = %d", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		CAM_ERR(CAM_SMMU,
			"dma_buf = %pK, paddr= 0x%pK, len = %u, region = %d",
			 mapping->buf, (void *)mapping->paddr,
			 (unsigned int)mapping->len,
			 mapping->region_id);
	}
}

static void cam_smmu_print_table(void)
{
	int i;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		CAM_ERR(CAM_SMMU, "i= %d, handle= %d, name_addr=%pK", i,
			   (int)iommu_cb_set.cb_info[i].handle,
			   (void *)iommu_cb_set.cb_info[i].name);
		CAM_ERR(CAM_SMMU, "dev = %pK", iommu_cb_set.cb_info[i].dev);
	}
}

static uint32_t cam_smmu_find_closest_mapping(int idx, void *vaddr)
{
	struct cam_dma_buff_info *mapping, *closest_mapping =  NULL;
	unsigned long start_addr, end_addr, current_addr;
	uint32_t buf_handle = 0;

	long delta = 0, lowest_delta = 0;

	current_addr = (unsigned long)vaddr;
	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		start_addr = (unsigned long)mapping->paddr;
		end_addr = (unsigned long)mapping->paddr + mapping->len - 1;

		if (start_addr <= current_addr && current_addr <= end_addr) {
			closest_mapping = mapping;
			CAM_INFO(CAM_SMMU,
				"Found va 0x%lx in:0x%lx-0x%lx, fd %d cb:%s",
				current_addr, start_addr,
				end_addr, mapping->ion_fd,
				iommu_cb_set.cb_info[idx].name);
			goto end;
		} else {
			if (start_addr > current_addr)
				delta =  start_addr - current_addr;
			else
				delta = current_addr - end_addr - 1;

			if (delta < lowest_delta || lowest_delta == 0) {
				lowest_delta = delta;
				closest_mapping = mapping;
			}
			CAM_DBG(CAM_SMMU,
				"approx va %lx not in range: %lx-%lx fd = %0x",
				current_addr, start_addr,
				end_addr, mapping->ion_fd);
		}
	}

end:
	if (closest_mapping) {
		buf_handle = GET_MEM_HANDLE(idx, closest_mapping->ion_fd);
		CAM_INFO(CAM_SMMU,
			"Closest map fd %d 0x%lx 0x%lx-0x%lx buf=%pK mem %0x",
			closest_mapping->ion_fd, current_addr,
			(unsigned long)closest_mapping->paddr,
			(unsigned long)closest_mapping->paddr + mapping->len - 1,
			closest_mapping->buf,
			buf_handle);
	} else
		CAM_INFO(CAM_SMMU,
			"Cannot find vaddr:%lx in SMMU %s virt address",
			current_addr, iommu_cb_set.cb_info[idx].name);

	return buf_handle;
}

void cam_smmu_set_client_page_fault_handler(int handle,
	cam_smmu_client_page_fault_handler handler_cb, void *token)
{
	int idx, i = 0;

	if (!token || (handle == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL or invalid handle");
		return;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	if (handler_cb) {
		if (iommu_cb_set.cb_info[idx].cb_count == CAM_SMMU_CB_MAX) {
			CAM_ERR(CAM_SMMU,
				"%s Should not regiester more handlers",
				iommu_cb_set.cb_info[idx].name);
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return;
		}

		iommu_cb_set.cb_info[idx].cb_count++;

		for (i = 0; i < iommu_cb_set.cb_info[idx].cb_count; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == NULL) {
				iommu_cb_set.cb_info[idx].token[i] = token;
				iommu_cb_set.cb_info[idx].handler[i] =
					handler_cb;
				break;
			}
		}
	} else {
		for (i = 0; i < CAM_SMMU_CB_MAX; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == token) {
				iommu_cb_set.cb_info[idx].token[i] = NULL;
				iommu_cb_set.cb_info[idx].handler[i] =
					NULL;
				iommu_cb_set.cb_info[idx].cb_count--;
				break;
			}
		}
		if (i == CAM_SMMU_CB_MAX)
			CAM_ERR(CAM_SMMU,
				"Error: hdl %x no matching tokens: %s",
				handle, iommu_cb_set.cb_info[idx].name);
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
}

void cam_smmu_unset_client_page_fault_handler(int handle, void *token)
{
	int idx, i = 0;

	if (!token || (handle == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL or invalid handle");
		return;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	for (i = 0; i < CAM_SMMU_CB_MAX; i++) {
		if (iommu_cb_set.cb_info[idx].token[i] == token) {
			iommu_cb_set.cb_info[idx].token[i] = NULL;
			iommu_cb_set.cb_info[idx].handler[i] =
				NULL;
			iommu_cb_set.cb_info[idx].cb_count--;
			break;
		}
	}
	if (i == CAM_SMMU_CB_MAX)
		CAM_ERR(CAM_SMMU, "Error: hdl %x no matching tokens: %s",
			handle, iommu_cb_set.cb_info[idx].name);
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
}

static int cam_smmu_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova,
	int flags, void *token)
{
	char *cb_name;
	int idx;
	struct cam_smmu_work_payload *payload;

	if (!token) {
		CAM_ERR(CAM_SMMU, "Error: token is NULL");
		CAM_ERR(CAM_SMMU, "Error: domain = %pK, device = %pK",
			domain, dev);
		CAM_ERR(CAM_SMMU, "iova = %lX, flags = %d", iova, flags);
		return -EINVAL;
	}

	cb_name = (char *)token;
	/* Check whether it is in the table */
	for (idx = 0; idx < iommu_cb_set.cb_num; idx++) {
		if (!strcmp(iommu_cb_set.cb_info[idx].name, cb_name))
			break;
	}

	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: index is not valid, index = %d, token = %s",
			idx, cb_name);
		return -EINVAL;
	}

	if (++iommu_cb_set.cb_info[idx].pf_count > g_num_pf_handled) {
		CAM_INFO_RATE_LIMIT(CAM_SMMU, "PF already handled %d %d %d",
			g_num_pf_handled, idx,
			iommu_cb_set.cb_info[idx].pf_count);
		return -EINVAL;
	}

	payload = kzalloc(sizeof(struct cam_smmu_work_payload), GFP_ATOMIC);
	if (!payload)
		return -EINVAL;

	payload->domain = domain;
	payload->dev = dev;
	payload->iova = iova;
	payload->flags = flags;
	payload->token = token;
	payload->idx = idx;

	mutex_lock(&iommu_cb_set.payload_list_lock);
	list_add_tail(&payload->list, &iommu_cb_set.payload_list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	cam_smmu_page_fault_work(&iommu_cb_set.smmu_work);

	return -EINVAL;
}

static int cam_smmu_translate_dir_to_iommu_dir(
	enum cam_smmu_map_dir dir)
{
	switch (dir) {
	case CAM_SMMU_MAP_READ:
		return IOMMU_READ;
	case CAM_SMMU_MAP_WRITE:
		return IOMMU_WRITE;
	case CAM_SMMU_MAP_RW:
		return IOMMU_READ|IOMMU_WRITE;
	case CAM_SMMU_MAP_INVALID:
	default:
		CAM_ERR(CAM_SMMU, "Error: Direction is invalid. dir = %d", dir);
		break;
	};
	return IOMMU_INVALID_DIR;
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
		CAM_ERR(CAM_SMMU, "Error: Direction is invalid. dir = %d",
			(int)dir);
		break;
	}
	return DMA_NONE;
}

void cam_smmu_reset_iommu_table(enum cam_smmu_init_dir ops)
{
	unsigned int i;
	int j = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_cb_set.cb_info[i].handle = HANDLE_INIT;
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].smmu_buf_list);
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].smmu_buf_kernel_list);
		iommu_cb_set.cb_info[i].state = CAM_SMMU_DETACH;
		iommu_cb_set.cb_info[i].dev = NULL;
		iommu_cb_set.cb_info[i].cb_count = 0;
		iommu_cb_set.cb_info[i].pf_count = 0;
		for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
			iommu_cb_set.cb_info[i].token[j] = NULL;
			iommu_cb_set.cb_info[i].handler[j] = NULL;
		}
		if (ops == CAM_SMMU_TABLE_INIT)
			mutex_init(&iommu_cb_set.cb_info[i].lock);
		else
			mutex_destroy(&iommu_cb_set.cb_info[i].lock);
	}
}

static int cam_smmu_check_handle_unique(int hdl)
{
	int i;

	if (hdl == HANDLE_INIT) {
		CAM_DBG(CAM_SMMU,
			"iommu handle is init number. Need to try again");
		return 1;
	}

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (iommu_cb_set.cb_info[i].handle == HANDLE_INIT)
			continue;

		if (iommu_cb_set.cb_info[i].handle == hdl) {
			CAM_DBG(CAM_SMMU, "iommu handle %d conflicts",
				(int)hdl);
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
	CAM_DBG(CAM_SMMU, "create handle value = %x", (int)hdl);
	return hdl;
}

static int cam_smmu_attach_device(int idx)
{
	int rc;
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	/* attach the mapping to device */
	rc = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: ARM IOMMU attach failed. ret = %d",
			rc);
		rc = -ENODEV;
	}

	return rc;
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
				CAM_ERR(CAM_SMMU,
					"Error: %s already got handle 0x%x",
					name,
					iommu_cb_set.cb_info[i].handle);

				if (iommu_cb_set.cb_info[i].is_secure)
					iommu_cb_set.cb_info[i].secure_count++;

				mutex_unlock(&iommu_cb_set.cb_info[i].lock);
				if (iommu_cb_set.cb_info[i].is_secure) {
					*hdl = iommu_cb_set.cb_info[i].handle;
					return 0;
				}
				return -EINVAL;
			}

			/* make sure handle is unique */
			do {
				handle = cam_smmu_create_iommu_handle(i);
			} while (cam_smmu_check_handle_unique(handle));

			/* put handle in the table */
			iommu_cb_set.cb_info[i].handle = handle;
			iommu_cb_set.cb_info[i].cb_count = 0;
			if (iommu_cb_set.cb_info[i].is_secure)
				iommu_cb_set.cb_info[i].secure_count++;
			*hdl = handle;
			CAM_DBG(CAM_SMMU, "%s creates handle 0x%x",
				name, handle);
			mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			return 0;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find name %s or all handle exist",
		name);
	cam_smmu_print_table();
	return -EINVAL;
}

static int cam_smmu_init_scratch_map(struct scratch_mapping *scratch_map,
					dma_addr_t base, size_t size,
					int order)
{
	unsigned int count = size >> (PAGE_SHIFT + order);
	unsigned int bitmap_size = BITS_TO_LONGS(count) * sizeof(long);
	int err = 0;

	if (!count) {
		err = -EINVAL;
		CAM_ERR(CAM_SMMU, "Page count is zero, size passed = %zu",
			size);
		goto bail;
	}

	scratch_map->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!scratch_map->bitmap) {
		err = -ENOMEM;
		goto bail;
	}

	scratch_map->base = base;
	scratch_map->bits = BITS_PER_BYTE * bitmap_size;
	scratch_map->order = order;

bail:
	return err;
}

static int cam_smmu_alloc_scratch_va(struct scratch_mapping *mapping,
	size_t size,
	dma_addr_t *iova)
{
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count, start;

	count = ((PAGE_ALIGN(size) >> PAGE_SHIFT) +
		 (1 << mapping->order) - 1) >> mapping->order;

	/*
	 * Transparently, add a guard page to the total count of pages
	 * to be allocated
	 */
	count++;

	if (order > mapping->order)
		align = (1 << (order - mapping->order)) - 1;

	start = bitmap_find_next_zero_area(mapping->bitmap, mapping->bits, 0,
					   count, align);

	if (start > mapping->bits)
		return -ENOMEM;

	bitmap_set(mapping->bitmap, start, count);
	*iova = mapping->base + (start << (mapping->order + PAGE_SHIFT));

	return 0;
}

static int cam_smmu_free_scratch_va(struct scratch_mapping *mapping,
	dma_addr_t addr, size_t size)
{
	unsigned int start = (addr - mapping->base) >>
			     (mapping->order + PAGE_SHIFT);
	unsigned int count = ((size >> PAGE_SHIFT) +
			      (1 << mapping->order) - 1) >> mapping->order;

	if (!addr) {
		CAM_ERR(CAM_SMMU, "Error: Invalid address");
		return -EINVAL;
	}

	if (start + count > mapping->bits) {
		CAM_ERR(CAM_SMMU, "Error: Invalid page bits in scratch map");
		return -EINVAL;
	}

	/*
	 * Transparently, add a guard page to the total count of pages
	 * to be freed
	 */
	count++;
	bitmap_clear(mapping->bitmap, start, count);

	return 0;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_virt_address(int idx,
	dma_addr_t virt_addr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->paddr == virt_addr) {
			CAM_DBG(CAM_SMMU, "Found virtual address %lx",
				 (unsigned long)virt_addr);
			return mapping;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find virtual address %lx by index %d",
		(unsigned long)virt_addr, idx);
	return NULL;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int ion_fd)
{
	struct cam_dma_buff_info *mapping;

	if (ion_fd < 0) {
		CAM_ERR(CAM_SMMU, "Invalid fd %d", ion_fd);
		return NULL;
	}

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
			CAM_DBG(CAM_SMMU, "find ion_fd %d", ion_fd);
			return mapping;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find entry by index %d", idx);

	return NULL;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_dma_buf(int idx,
	struct dma_buf *buf)
{
	struct cam_dma_buff_info *mapping;

	if (!buf) {
		CAM_ERR(CAM_SMMU, "Invalid dma_buf");
		return NULL;
	}

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list,
			list) {
		if (mapping->buf == buf) {
			CAM_DBG(CAM_SMMU, "find dma_buf %pK", buf);
			return mapping;
		}
	}

	CAM_ERR(CAM_SMMU, "Error: Cannot find entry by index %d", idx);

	return NULL;
}

static struct cam_sec_buff_info *cam_smmu_find_mapping_by_sec_buf_idx(int idx,
	int ion_fd)
{
	struct cam_sec_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].smmu_buf_list,
		list) {
		if (mapping->ion_fd == ion_fd) {
			CAM_DBG(CAM_SMMU, "find ion_fd %d", ion_fd);
			return mapping;
		}
	}
	CAM_ERR(CAM_SMMU, "Error: Cannot find fd %d by index %d",
		ion_fd, idx);
	return NULL;
}

static void cam_smmu_clean_user_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		CAM_DBG(CAM_SMMU, "Free mapping address %pK, i = %d, fd = %d",
			(void *)mapping_info->paddr, idx,
			mapping_info->ion_fd);

		if (mapping_info->ion_fd == 0xDEADBEEF)
			/* Clean up scratch buffers */
			ret = cam_smmu_free_scratch_buffer_remove_from_list(
							mapping_info, idx);
		else
			/* Clean up regular mapped buffers */
			ret = cam_smmu_unmap_buf_and_remove_from_list(
					mapping_info,
					idx);

		if (ret < 0) {
			CAM_ERR(CAM_SMMU, "Buffer delete failed: idx = %d",
				idx);
			CAM_ERR(CAM_SMMU,
				"Buffer delete failed: addr = %lx, fd = %d",
				(unsigned long)mapping_info->paddr,
				mapping_info->ion_fd);
			/*
			 * Ignore this error and continue to delete other
			 * buffers in the list
			 */
			continue;
		}
	}
}

static void cam_smmu_clean_kernel_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
			&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		CAM_DBG(CAM_SMMU,
			"Free mapping address %pK, i = %d, dma_buf = %pK",
			(void *)mapping_info->paddr, idx,
			mapping_info->buf);

		/* Clean up regular mapped buffers */
		ret = cam_smmu_unmap_buf_and_remove_from_list(
				mapping_info,
				idx);

		if (ret < 0) {
			CAM_ERR(CAM_SMMU,
				"Buffer delete in kernel list failed: idx = %d",
				idx);
			CAM_ERR(CAM_SMMU,
				"Buffer delete failed: addr = %lx, dma_buf = %pK",
				(unsigned long)mapping_info->paddr,
				mapping_info->buf);
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
		ret = -EALREADY;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		ret = cam_smmu_attach_device(idx);
		if (ret < 0) {
			CAM_ERR(CAM_SMMU, "Error: ATTACH fail");
			return -ENODEV;
		}
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_ATTACH;
		ret = 0;
	} else {
		CAM_ERR(CAM_SMMU, "Error: Not detach/attach: %d",
			iommu_cb_set.cb_info[idx].state);
		ret = -EINVAL;
	}

	return ret;
}

static int cam_smmu_detach_device(int idx)
{
	int rc = 0;
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	/* detach the mapping to device if not already detached */
	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		rc = -EALREADY;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		arm_iommu_detach_device(cb->dev);
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_DETACH;
	}

	return rc;
}

static int cam_smmu_alloc_iova(size_t size,
	int32_t smmu_hdl, uint32_t *iova)
{
	int rc = 0;
	int idx;
	uint32_t vaddr = 0;

	if (!iova || !size || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	CAM_DBG(CAM_SMMU, "Allocating iova size = %zu for smmu hdl=%X",
		size, smmu_hdl);

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].handle != smmu_hdl) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, smmu_hdl);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (!iommu_cb_set.cb_info[idx].shared_support) {
		CAM_ERR(CAM_SMMU,
			"Error: Shared memory not supported for hdl = %X",
			smmu_hdl);
		rc = -EINVAL;
		goto get_addr_end;
	}

	vaddr = gen_pool_alloc(iommu_cb_set.cb_info[idx].shared_mem_pool, size);
	if (!vaddr)
		return -ENOMEM;

	*iova = vaddr;

get_addr_end:
	return rc;
}

static int cam_smmu_free_iova(uint32_t addr, size_t size,
	int32_t smmu_hdl)
{
	int rc = 0;
	int idx;

	if (!size || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].handle != smmu_hdl) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, smmu_hdl);
		rc = -EINVAL;
		goto get_addr_end;
	}

	gen_pool_free(iommu_cb_set.cb_info[idx].shared_mem_pool, addr, size);

get_addr_end:
	return rc;
}

int cam_smmu_alloc_firmware(int32_t smmu_hdl,
	dma_addr_t *iova,
	uintptr_t *cpuva,
	size_t *len)
{
	int rc;
	int32_t idx;
	size_t firmware_len = 0;
	size_t firmware_start = 0;
	struct iommu_domain *domain;

	if (!iova || !len || !cpuva || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].firmware_support) {
		CAM_ERR(CAM_SMMU,
			"Firmware memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_fw_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to allocate twice");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	firmware_len = iommu_cb_set.cb_info[idx].firmware_info.iova_len;
	firmware_start = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	CAM_DBG(CAM_SMMU, "Firmware area len from DT = %zu", firmware_len);

	icp_fw.fw_kva = dma_alloc_coherent(icp_fw.fw_dev,
		firmware_len,
		&icp_fw.fw_dma_hdl,
		GFP_KERNEL);
	if (!icp_fw.fw_kva) {
		CAM_ERR(CAM_SMMU, "FW memory alloc failed");
		rc = -ENOMEM;
		goto unlock_and_end;
	} else {
		CAM_DBG(CAM_SMMU, "DMA alloc returned fw = %pK, hdl = %pK",
			icp_fw.fw_kva, (void *)icp_fw.fw_dma_hdl);
	}

	domain = iommu_cb_set.cb_info[idx].mapping->domain;
	rc = iommu_map(domain,
		firmware_start,
		icp_fw.fw_dma_hdl,
		firmware_len,
		IOMMU_READ|IOMMU_WRITE|IOMMU_PRIV);

	if (rc) {
		CAM_ERR(CAM_SMMU, "Failed to map FW into IOMMU");
		rc = -ENOMEM;
		goto alloc_fail;
	}
	iommu_cb_set.cb_info[idx].is_fw_allocated = true;

	*iova = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	*cpuva = (uintptr_t)icp_fw.fw_kva;
	*len = firmware_len;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return rc;

alloc_fail:
	dma_free_coherent(icp_fw.fw_dev,
		firmware_len,
		icp_fw.fw_kva,
		icp_fw.fw_dma_hdl);
unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_alloc_firmware);

int cam_smmu_dealloc_firmware(int32_t smmu_hdl)
{
	int rc = 0;
	int32_t idx;
	size_t firmware_len = 0;
	size_t firmware_start = 0;
	struct iommu_domain *domain;
	size_t unmapped = 0;

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].firmware_support) {
		CAM_ERR(CAM_SMMU,
			"Firmware memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (!iommu_cb_set.cb_info[idx].is_fw_allocated) {
		CAM_ERR(CAM_SMMU,
			"Trying to deallocate firmware that is not allocated");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	firmware_len = iommu_cb_set.cb_info[idx].firmware_info.iova_len;
	firmware_start = iommu_cb_set.cb_info[idx].firmware_info.iova_start;
	domain = iommu_cb_set.cb_info[idx].mapping->domain;
	unmapped = iommu_unmap(domain,
		firmware_start,
		firmware_len);

	if (unmapped != firmware_len) {
		CAM_ERR(CAM_SMMU, "Only %zu unmapped out of total %zu",
			unmapped,
			firmware_len);
		rc = -EINVAL;
	}

	dma_free_coherent(icp_fw.fw_dev,
		firmware_len,
		icp_fw.fw_kva,
		icp_fw.fw_dma_hdl);

	icp_fw.fw_kva = 0;
	icp_fw.fw_dma_hdl = 0;

	iommu_cb_set.cb_info[idx].is_fw_allocated = false;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_dealloc_firmware);

int cam_smmu_alloc_qdss(int32_t smmu_hdl,
	dma_addr_t *iova,
	size_t *len)
{
	int rc;
	int32_t idx;
	size_t qdss_len = 0;
	size_t qdss_start = 0;
	dma_addr_t qdss_phy_addr;
	struct iommu_domain *domain;

	if (!iova || !len || (smmu_hdl == HANDLE_INIT)) {
		CAM_ERR(CAM_SMMU, "Error: Input args are invalid");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].qdss_support) {
		CAM_ERR(CAM_SMMU,
			"QDSS memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_qdss_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to allocate twice");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	qdss_len = iommu_cb_set.cb_info[idx].qdss_info.iova_len;
	qdss_start = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	qdss_phy_addr = iommu_cb_set.cb_info[idx].qdss_phy_addr;
	CAM_DBG(CAM_SMMU, "QDSS area len from DT = %zu", qdss_len);

	domain = iommu_cb_set.cb_info[idx].mapping->domain;
	rc = iommu_map(domain,
		qdss_start,
		qdss_phy_addr,
		qdss_len,
		IOMMU_READ|IOMMU_WRITE);

	if (rc) {
		CAM_ERR(CAM_SMMU, "Failed to map QDSS into IOMMU");
		goto unlock_and_end;
	}

	iommu_cb_set.cb_info[idx].is_qdss_allocated = true;

	*iova = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	*len = qdss_len;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return rc;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_alloc_qdss);

int cam_smmu_dealloc_qdss(int32_t smmu_hdl)
{
	int rc = 0;
	int32_t idx;
	size_t qdss_len = 0;
	size_t qdss_start = 0;
	struct iommu_domain *domain;
	size_t unmapped = 0;

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		rc = -EINVAL;
		goto end;
	}

	if (!iommu_cb_set.cb_info[idx].qdss_support) {
		CAM_ERR(CAM_SMMU,
			"QDSS memory not supported for this SMMU handle");
		rc = -EINVAL;
		goto end;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (!iommu_cb_set.cb_info[idx].is_qdss_allocated) {
		CAM_ERR(CAM_SMMU,
			"Trying to deallocate qdss that is not allocated");
		rc = -ENOMEM;
		goto unlock_and_end;
	}

	qdss_len = iommu_cb_set.cb_info[idx].qdss_info.iova_len;
	qdss_start = iommu_cb_set.cb_info[idx].qdss_info.iova_start;
	domain = iommu_cb_set.cb_info[idx].mapping->domain;
	unmapped = iommu_unmap(domain, qdss_start, qdss_len);

	if (unmapped != qdss_len) {
		CAM_ERR(CAM_SMMU, "Only %zu unmapped out of total %zu",
			unmapped,
			qdss_len);
		rc = -EINVAL;
	}

	iommu_cb_set.cb_info[idx].is_qdss_allocated = false;

unlock_and_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
end:
	return rc;
}
EXPORT_SYMBOL(cam_smmu_dealloc_qdss);

int cam_smmu_get_region_info(int32_t smmu_hdl,
	enum cam_smmu_region_id region_id,
	struct cam_smmu_region_info *region_info)
{
	int32_t idx;
	struct cam_context_bank_info *cb = NULL;

	if (!region_info) {
		CAM_ERR(CAM_SMMU, "Invalid region_info pointer");
		return -EINVAL;
	}

	if (smmu_hdl == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	cb = &iommu_cb_set.cb_info[idx];
	if (!cb) {
		CAM_ERR(CAM_SMMU, "SMMU context bank pointer invalid");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	switch (region_id) {
	case CAM_SMMU_REGION_FIRMWARE:
		if (!cb->firmware_support) {
			CAM_ERR(CAM_SMMU, "Firmware not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->firmware_info.iova_start;
		region_info->iova_len = cb->firmware_info.iova_len;
		break;
	case CAM_SMMU_REGION_SHARED:
		if (!cb->shared_support) {
			CAM_ERR(CAM_SMMU, "Shared mem not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->shared_info.iova_start;
		region_info->iova_len = cb->shared_info.iova_len;
		break;
	case CAM_SMMU_REGION_SCRATCH:
		if (!cb->scratch_buf_support) {
			CAM_ERR(CAM_SMMU, "Scratch memory not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->scratch_info.iova_start;
		region_info->iova_len = cb->scratch_info.iova_len;
		break;
	case CAM_SMMU_REGION_IO:
		if (!cb->io_support) {
			CAM_ERR(CAM_SMMU, "IO memory not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->io_info.iova_start;
		region_info->iova_len = cb->io_info.iova_len;
		break;
	case CAM_SMMU_REGION_SECHEAP:
		if (!cb->secheap_support) {
			CAM_ERR(CAM_SMMU, "Secondary heap not supported");
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -ENODEV;
		}
		region_info->iova_start = cb->secheap_info.iova_start;
		region_info->iova_len = cb->secheap_info.iova_len;
		break;
	default:
		CAM_ERR(CAM_SMMU, "Invalid region id: %d for smmu hdl: %X",
			smmu_hdl, region_id);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_get_region_info);

int cam_smmu_reserve_sec_heap(int32_t smmu_hdl,
	struct dma_buf *buf,
	dma_addr_t *iova,
	size_t *request_len)
{
	struct secheap_buf_info *secheap_buf = NULL;
	size_t size = 0;
	uint32_t sec_heap_iova = 0;
	size_t sec_heap_iova_len = 0;
	int idx;
	int rc = 0;

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].secheap_support) {
		CAM_ERR(CAM_SMMU, "Secondary heap not supported");
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);

	if (iommu_cb_set.cb_info[idx].is_secheap_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to allocate secheap twice");
		rc = -ENOMEM;
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return rc;
	}

	if (IS_ERR_OR_NULL(buf)) {
		rc = PTR_ERR(buf);
		CAM_ERR(CAM_SMMU,
			"Error: dma get buf failed. rc = %d", rc);
		goto err_out;
	}

	secheap_buf = &iommu_cb_set.cb_info[idx].secheap_buf;
	secheap_buf->buf = buf;
	secheap_buf->attach = dma_buf_attach(secheap_buf->buf,
		iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(secheap_buf->attach)) {
		rc = PTR_ERR(secheap_buf->attach);
		CAM_ERR(CAM_SMMU, "Error: dma buf attach failed");
		goto err_put;
	}

	secheap_buf->table = dma_buf_map_attachment(secheap_buf->attach,
		DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(secheap_buf->table)) {
		rc = PTR_ERR(secheap_buf->table);
		CAM_ERR(CAM_SMMU, "Error: dma buf map attachment failed");
		goto err_detach;
	}

	sec_heap_iova = iommu_cb_set.cb_info[idx].secheap_info.iova_start;
	sec_heap_iova_len = iommu_cb_set.cb_info[idx].secheap_info.iova_len;
	size = iommu_map_sg(iommu_cb_set.cb_info[idx].mapping->domain,
		sec_heap_iova,
		secheap_buf->table->sgl,
		secheap_buf->table->nents,
		IOMMU_READ | IOMMU_WRITE);
	if (size != sec_heap_iova_len) {
		CAM_ERR(CAM_SMMU, "IOMMU mapping failed");
		goto err_unmap_sg;
	}

	iommu_cb_set.cb_info[idx].is_secheap_allocated = true;
	*iova = (uint32_t)sec_heap_iova;
	*request_len = sec_heap_iova_len;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return rc;

err_unmap_sg:
	dma_buf_unmap_attachment(secheap_buf->attach,
		secheap_buf->table,
		DMA_BIDIRECTIONAL);
err_detach:
	dma_buf_detach(secheap_buf->buf,
		secheap_buf->attach);
err_put:
	dma_buf_put(secheap_buf->buf);
err_out:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_reserve_sec_heap);

int cam_smmu_release_sec_heap(int32_t smmu_hdl)
{
	int idx;
	size_t size = 0;
	uint32_t sec_heap_iova = 0;
	size_t sec_heap_iova_len = 0;
	struct secheap_buf_info *secheap_buf = NULL;

	idx = GET_SMMU_TABLE_IDX(smmu_hdl);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, smmu_hdl);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].secheap_support) {
		CAM_ERR(CAM_SMMU, "Secondary heap not supported");
		return -EINVAL;
	}
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);

	if (!iommu_cb_set.cb_info[idx].is_secheap_allocated) {
		CAM_ERR(CAM_SMMU, "Trying to release secheap twice");
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -ENOMEM;
	}

	secheap_buf = &iommu_cb_set.cb_info[idx].secheap_buf;
	sec_heap_iova = iommu_cb_set.cb_info[idx].secheap_info.iova_start;
	sec_heap_iova_len = iommu_cb_set.cb_info[idx].secheap_info.iova_len;

	size = iommu_unmap(iommu_cb_set.cb_info[idx].mapping->domain,
		sec_heap_iova,
		sec_heap_iova_len);
	if (size != sec_heap_iova_len) {
		CAM_ERR(CAM_SMMU, "Failed: Unmapped = %zu, requested = %zu",
			size,
			sec_heap_iova_len);
	}

	dma_buf_unmap_attachment(secheap_buf->attach,
		secheap_buf->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(secheap_buf->buf, secheap_buf->attach);
	dma_buf_put(secheap_buf->buf);
	iommu_cb_set.cb_info[idx].is_secheap_allocated = false;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	return 0;
}
EXPORT_SYMBOL(cam_smmu_release_sec_heap);

static int cam_smmu_map_buffer_validate(struct dma_buf *buf,
	int idx, enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id,
	struct cam_dma_buff_info **mapping_info)
{
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct iommu_domain *domain;
	size_t size = 0;
	uint32_t iova = 0;
	int rc = 0;

	if (IS_ERR_OR_NULL(buf)) {
		rc = PTR_ERR(buf);
		CAM_ERR(CAM_SMMU,
			"Error: dma get buf failed. rc = %d", rc);
		goto err_out;
	}

	if (!mapping_info) {
		rc = -EINVAL;
		CAM_ERR(CAM_SMMU, "Error: mapping_info is invalid");
		goto err_out;
	}

	attach = dma_buf_attach(buf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach);
		CAM_ERR(CAM_SMMU, "Error: dma buf attach failed");
		goto err_put;
	}

	if (region_id == CAM_SMMU_REGION_SHARED) {
		table = dma_buf_map_attachment(attach, dma_dir);
		if (IS_ERR_OR_NULL(table)) {
			rc = PTR_ERR(table);
			CAM_ERR(CAM_SMMU, "Error: dma map attachment failed");
			goto err_detach;
		}

		domain = iommu_cb_set.cb_info[idx].mapping->domain;
		if (!domain) {
			CAM_ERR(CAM_SMMU, "CB has no domain set");
			goto err_unmap_sg;
		}

		rc = cam_smmu_alloc_iova(*len_ptr,
			iommu_cb_set.cb_info[idx].handle,
			&iova);

		if (rc < 0) {
			CAM_ERR(CAM_SMMU,
				"IOVA alloc failed for shared memory, size=%zu, idx=%d, handle=%d",
				*len_ptr, idx,
				iommu_cb_set.cb_info[idx].handle);
			goto err_unmap_sg;
		}

		size = iommu_map_sg(domain, iova, table->sgl, table->nents,
				IOMMU_READ | IOMMU_WRITE);

		if (size < 0) {
			CAM_ERR(CAM_SMMU, "IOMMU mapping failed");
			rc = cam_smmu_free_iova(iova,
				size, iommu_cb_set.cb_info[idx].handle);
			if (rc)
				CAM_ERR(CAM_SMMU, "IOVA free failed");
			rc = -ENOMEM;
			goto err_unmap_sg;
		} else {
			CAM_DBG(CAM_SMMU,
				"iommu_map_sg returned iova=%pK, size=%zu",
				iova, size);
			*paddr_ptr = iova;
			*len_ptr = size;
		}
	} else if (region_id == CAM_SMMU_REGION_IO) {
		attach->dma_map_attrs |= DMA_ATTR_DELAYED_UNMAP;

		table = dma_buf_map_attachment(attach, dma_dir);
		if (IS_ERR_OR_NULL(table)) {
			rc = PTR_ERR(table);
			CAM_ERR(CAM_SMMU, "Error: dma map attachment failed");
			goto err_detach;
		}

		*paddr_ptr = sg_dma_address(table->sgl);
		*len_ptr = (size_t)buf->size;
	} else {
		CAM_ERR(CAM_SMMU, "Error: Wrong region id passed");
		rc = -EINVAL;
		goto err_unmap_sg;
	}

	CAM_DBG(CAM_SMMU, "iova=%pK, region_id=%d, paddr=%pK, len=%d",
		iova, region_id, *paddr_ptr, *len_ptr);

	if (table->sgl) {
		CAM_DBG(CAM_SMMU,
			"DMA buf: %pK, device: %pK, attach: %pK, table: %pK",
			(void *)buf,
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)attach, (void *)table);
		CAM_DBG(CAM_SMMU, "table sgl: %pK, rc: %d, dma_address: 0x%x",
			(void *)table->sgl, rc,
			(unsigned int)table->sgl->dma_address);
	} else {
		rc = -EINVAL;
		CAM_ERR(CAM_SMMU, "Error: table sgl is null");
		goto err_unmap_sg;
	}

	/* fill up mapping_info */
	*mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (!(*mapping_info)) {
		rc = -ENOSPC;
		goto err_alloc;
	}

	(*mapping_info)->buf = buf;
	(*mapping_info)->attach = attach;
	(*mapping_info)->table = table;
	(*mapping_info)->paddr = *paddr_ptr;
	(*mapping_info)->len = *len_ptr;
	(*mapping_info)->dir = dma_dir;
	(*mapping_info)->ref_count = 1;
	(*mapping_info)->region_id = region_id;

	if (!*paddr_ptr || !*len_ptr) {
		CAM_ERR(CAM_SMMU, "Error: Space Allocation failed");
		kfree(*mapping_info);
		*mapping_info = NULL;
		rc = -ENOSPC;
		goto err_alloc;
	}
	CAM_DBG(CAM_SMMU, "idx=%d, dma_buf=%pK, dev=%pK, paddr=%pK, len=%u",
		idx, buf, (void *)iommu_cb_set.cb_info[idx].dev,
		(void *)*paddr_ptr, (unsigned int)*len_ptr);

	return 0;

err_alloc:
	if (region_id == CAM_SMMU_REGION_SHARED) {
		cam_smmu_free_iova(iova,
			size,
			iommu_cb_set.cb_info[idx].handle);

		iommu_unmap(iommu_cb_set.cb_info[idx].mapping->domain,
			*paddr_ptr,
			*len_ptr);
	}
err_unmap_sg:
	dma_buf_unmap_attachment(attach, table, dma_dir);
err_detach:
	dma_buf_detach(buf, attach);
err_put:
	dma_buf_put(buf);
err_out:
	return rc;
}


static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
	 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	 size_t *len_ptr, enum cam_smmu_region_id region_id)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info = NULL;
	struct dma_buf *buf = NULL;

	/* returns the dma_buf structure related to an fd */
	buf = dma_buf_get(ion_fd);

	rc = cam_smmu_map_buffer_validate(buf, idx, dma_dir, paddr_ptr, len_ptr,
		region_id, &mapping_info);

	if (rc) {
		CAM_ERR(CAM_SMMU, "buffer validation failure");
		return rc;
	}

	mapping_info->ion_fd = ion_fd;
	/* add to the list */
	list_add(&mapping_info->list,
		&iommu_cb_set.cb_info[idx].smmu_buf_list);

	return 0;
}

static int cam_smmu_map_kernel_buffer_and_add_to_list(int idx,
	struct dma_buf *buf, enum dma_data_direction dma_dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info = NULL;

	rc = cam_smmu_map_buffer_validate(buf, idx, dma_dir, paddr_ptr, len_ptr,
		region_id, &mapping_info);

	if (rc) {
		CAM_ERR(CAM_SMMU, "buffer validation failure");
		return rc;
	}

	mapping_info->ion_fd = -1;

	/* add to the list */
	list_add(&mapping_info->list,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list);

	return 0;
}


static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info,
	int idx)
{
	int rc;
	size_t size;
	struct iommu_domain *domain;

	if ((!mapping_info->buf) || (!mapping_info->table) ||
		(!mapping_info->attach)) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params dev = %pK, table = %pK",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		CAM_ERR(CAM_SMMU, "Error:dma_buf = %pK, attach = %pK",
			(void *)mapping_info->buf,
			(void *)mapping_info->attach);
		return -EINVAL;
	}

	if (mapping_info->region_id == CAM_SMMU_REGION_SHARED) {
		CAM_DBG(CAM_SMMU,
			"Removing SHARED buffer paddr = %pK, len = %zu",
			(void *)mapping_info->paddr, mapping_info->len);

		domain = iommu_cb_set.cb_info[idx].mapping->domain;

		size = iommu_unmap(domain,
			mapping_info->paddr,
			mapping_info->len);

		if (size != mapping_info->len) {
			CAM_ERR(CAM_SMMU, "IOMMU unmap failed");
			CAM_ERR(CAM_SMMU, "Unmapped = %zu, requested = %zu",
				size,
				mapping_info->len);
		}

		rc = cam_smmu_free_iova(mapping_info->paddr,
			mapping_info->len,
			iommu_cb_set.cb_info[idx].handle);

		if (rc)
			CAM_ERR(CAM_SMMU, "IOVA free failed");

	} else if (mapping_info->region_id == CAM_SMMU_REGION_IO) {
		mapping_info->attach->dma_map_attrs |= DMA_ATTR_DELAYED_UNMAP;
	}

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
	int ion_fd, dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		if (mapping->ion_fd == ion_fd) {
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}

	return CAM_SMMU_BUFF_NOT_EXIST;
}

static enum cam_smmu_buf_state cam_smmu_check_dma_buf_in_list(int idx,
	struct dma_buf *buf, dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list, list) {
		if (mapping->buf == buf) {
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}

	return CAM_SMMU_BUFF_NOT_EXIST;
}

static enum cam_smmu_buf_state cam_smmu_check_secure_fd_in_list(int idx,
					int ion_fd, dma_addr_t *paddr_ptr,
					size_t *len_ptr)
{
	struct cam_sec_buff_info *mapping;

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			mapping->ref_count++;
			return CAM_SMMU_BUFF_EXIST;
		}
	}

	return CAM_SMMU_BUFF_NOT_EXIST;
}

static enum cam_smmu_buf_state cam_smmu_validate_secure_fd_in_list(int idx,
	int ion_fd, dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	struct cam_sec_buff_info *mapping;

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
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
		CAM_ERR(CAM_SMMU, "Error: iommu hardware name is NULL");
		return -EINVAL;
	}

	if (!handle_ptr) {
		CAM_ERR(CAM_SMMU, "Error: handle pointer is NULL");
		return -EINVAL;
	}

	/* create and put handle in the table */
	ret = cam_smmu_create_add_handle_in_table(identifier, handle_ptr);
	if (ret < 0)
		CAM_ERR(CAM_SMMU, "Error: %s get handle fail", identifier);

	return ret;
}
EXPORT_SYMBOL(cam_smmu_get_handle);

int cam_smmu_ops(int handle, enum cam_smmu_ops_param ops)
{
	int ret = 0, idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Error: Index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
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
		ret = cam_smmu_detach_device(idx);
		break;
	}
	case CAM_SMMU_VOTE:
	case CAM_SMMU_DEVOTE:
	default:
		CAM_ERR(CAM_SMMU, "Error: idx = %d, ops = %d", idx, ops);
		ret = -EINVAL;
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return ret;
}
EXPORT_SYMBOL(cam_smmu_ops);

static int cam_smmu_alloc_scratch_buffer_add_to_list(int idx,
	size_t virt_len,
	size_t phys_len,
	unsigned int iommu_dir,
	dma_addr_t *virt_addr)
{
	unsigned long nents = virt_len / phys_len;
	struct cam_dma_buff_info *mapping_info = NULL;
	size_t unmapped;
	dma_addr_t iova = 0;
	struct scatterlist *sg;
	int i = 0;
	int rc;
	struct iommu_domain *domain = NULL;
	struct page *page;
	struct sg_table *table = NULL;

	CAM_DBG(CAM_SMMU, "nents = %lu, idx = %d, virt_len  = %zx",
		nents, idx, virt_len);
	CAM_DBG(CAM_SMMU, "phys_len = %zx, iommu_dir = %d, virt_addr = %pK",
		phys_len, iommu_dir, virt_addr);

	/*
	 * This table will go inside the 'mapping' structure
	 * where it will be held until put_scratch_buffer is called
	 */
	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		rc = -ENOMEM;
		goto err_table_alloc;
	}

	rc = sg_alloc_table(table, nents, GFP_KERNEL);
	if (rc < 0) {
		rc = -EINVAL;
		goto err_sg_alloc;
	}

	page = alloc_pages(GFP_KERNEL, get_order(phys_len));
	if (!page) {
		rc = -ENOMEM;
		goto err_page_alloc;
	}

	/* Now we create the sg list */
	for_each_sg(table->sgl, sg, table->nents, i)
		sg_set_page(sg, page, phys_len, 0);


	/* Get the domain from within our cb_set struct and map it*/
	domain = iommu_cb_set.cb_info[idx].mapping->domain;

	rc = cam_smmu_alloc_scratch_va(&iommu_cb_set.cb_info[idx].scratch_map,
		virt_len, &iova);

	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Could not find valid iova for scratch buffer");
		goto err_iommu_map;
	}

	if (iommu_map_sg(domain,
		iova,
		table->sgl,
		table->nents,
		iommu_dir) != virt_len) {
		CAM_ERR(CAM_SMMU, "iommu_map_sg() failed");
		goto err_iommu_map;
	}

	/* Now update our mapping information within the cb_set struct */
	mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (!mapping_info) {
		rc = -ENOMEM;
		goto err_mapping_info;
	}

	mapping_info->ion_fd = 0xDEADBEEF;
	mapping_info->buf = NULL;
	mapping_info->attach = NULL;
	mapping_info->table = table;
	mapping_info->paddr = iova;
	mapping_info->len = virt_len;
	mapping_info->iommu_dir = iommu_dir;
	mapping_info->ref_count = 1;
	mapping_info->phys_len = phys_len;
	mapping_info->region_id = CAM_SMMU_REGION_SCRATCH;

	CAM_DBG(CAM_SMMU, "paddr = %pK, len = %zx, phys_len = %zx",
		(void *)mapping_info->paddr,
		mapping_info->len, mapping_info->phys_len);

	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].smmu_buf_list);

	*virt_addr = (dma_addr_t)iova;

	CAM_DBG(CAM_SMMU, "mapped virtual address = %lx",
		(unsigned long)*virt_addr);
	return 0;

err_mapping_info:
	unmapped = iommu_unmap(domain, iova,  virt_len);
	if (unmapped != virt_len)
		CAM_ERR(CAM_SMMU, "Unmapped only %zx instead of %zx",
			unmapped, virt_len);
err_iommu_map:
	__free_pages(page, get_order(phys_len));
err_page_alloc:
	sg_free_table(table);
err_sg_alloc:
	kfree(table);
err_table_alloc:
	return rc;
}

static int cam_smmu_free_scratch_buffer_remove_from_list(
	struct cam_dma_buff_info *mapping_info,
	int idx)
{
	int rc = 0;
	size_t unmapped;
	struct iommu_domain *domain =
		iommu_cb_set.cb_info[idx].mapping->domain;
	struct scratch_mapping *scratch_map =
		&iommu_cb_set.cb_info[idx].scratch_map;

	if (!mapping_info->table) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params: dev = %pK, table = %pK",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		return -EINVAL;
	}

	/* Clean up the mapping_info struct from the list */
	unmapped = iommu_unmap(domain, mapping_info->paddr, mapping_info->len);
	if (unmapped != mapping_info->len)
		CAM_ERR(CAM_SMMU, "Unmapped only %zx instead of %zx",
			unmapped, mapping_info->len);

	rc = cam_smmu_free_scratch_va(scratch_map,
		mapping_info->paddr,
		mapping_info->len);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid iova while freeing scratch buffer");
		rc = -EINVAL;
	}

	__free_pages(sg_page(mapping_info->table->sgl),
			get_order(mapping_info->phys_len));
	sg_free_table(mapping_info->table);
	kfree(mapping_info->table);
	list_del_init(&mapping_info->list);

	kfree(mapping_info);
	mapping_info = NULL;

	return rc;
}

int cam_smmu_get_scratch_iova(int handle,
	enum cam_smmu_map_dir dir,
	dma_addr_t *paddr_ptr,
	size_t virt_len,
	size_t phys_len)
{
	int idx, rc;
	unsigned int iommu_dir;

	if (!paddr_ptr || !virt_len || !phys_len) {
		CAM_ERR(CAM_SMMU, "Error: Input pointer or lengths invalid");
		return -EINVAL;
	}

	if (virt_len < phys_len) {
		CAM_ERR(CAM_SMMU, "Error: virt_len > phys_len");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	iommu_dir = cam_smmu_translate_dir_to_iommu_dir(dir);
	if (iommu_dir == IOMMU_INVALID_DIR) {
		CAM_ERR(CAM_SMMU,
			"Error: translate direction failed. dir = %d", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto error;
	}

	if (!iommu_cb_set.cb_info[idx].scratch_buf_support) {
		CAM_ERR(CAM_SMMU,
			"Error: Context bank does not support scratch bufs");
		rc = -EINVAL;
		goto error;
	}

	CAM_DBG(CAM_SMMU, "smmu handle = %x, idx = %d, dir = %d",
		handle, idx, dir);
	CAM_DBG(CAM_SMMU, "virt_len = %zx, phys_len  = %zx",
		phys_len, virt_len);

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		CAM_ERR(CAM_SMMU,
			"Err:Dev %s should call SMMU attach before map buffer",
			iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto error;
	}

	if (!IS_ALIGNED(virt_len, PAGE_SIZE)) {
		CAM_ERR(CAM_SMMU,
			"Requested scratch buffer length not page aligned");
		rc = -EINVAL;
		goto error;
	}

	if (!IS_ALIGNED(virt_len, phys_len)) {
		CAM_ERR(CAM_SMMU,
			"Requested virt length not aligned with phys length");
		rc = -EINVAL;
		goto error;
	}

	rc = cam_smmu_alloc_scratch_buffer_add_to_list(idx,
		virt_len,
		phys_len,
		iommu_dir,
		paddr_ptr);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "Error: mapping or add list fail");

error:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}

int cam_smmu_put_scratch_iova(int handle,
	dma_addr_t paddr)
{
	int idx;
	int rc = -1;
	struct cam_dma_buff_info *mapping_info;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto handle_err;
	}

	if (!iommu_cb_set.cb_info[idx].scratch_buf_support) {
		CAM_ERR(CAM_SMMU,
			"Error: Context bank does not support scratch buffers");
		rc = -EINVAL;
		goto handle_err;
	}

	/* Based on virtual address and index, we can find mapping info
	 * of the scratch buffer
	 */
	mapping_info = cam_smmu_find_mapping_by_virt_address(idx, paddr);
	if (!mapping_info) {
		CAM_ERR(CAM_SMMU, "Error: Invalid params");
		rc = -ENODEV;
		goto handle_err;
	}

	/* unmapping one buffer from device */
	rc = cam_smmu_free_scratch_buffer_remove_from_list(mapping_info, idx);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");
		goto handle_err;
	}

handle_err:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}

static int cam_smmu_map_stage2_buffer_and_add_to_list(int idx, int ion_fd,
		 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
		 size_t *len_ptr)
{
	int rc = 0;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct cam_sec_buff_info *mapping_info;

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	dmabuf = dma_buf_get(ion_fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		CAM_ERR(CAM_SMMU,
			"Error: dma buf get failed, idx=%d, ion_fd=%d",
			idx, ion_fd);
		rc = PTR_ERR(dmabuf);
		goto err_out;
	}

	/*
	 * ion_phys() is deprecated. call dma_buf_attach() and
	 * dma_buf_map_attachment() to get the buffer's physical
	 * address.
	 */
	attach = dma_buf_attach(dmabuf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		CAM_ERR(CAM_SMMU,
			"Error: dma buf attach failed, idx=%d, ion_fd=%d",
			idx, ion_fd);
		rc = PTR_ERR(attach);
		goto err_put;
	}

	attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		CAM_ERR(CAM_SMMU, "Error: dma buf map attachment failed");
		rc = PTR_ERR(table);
		goto err_detach;
	}

	/* return addr and len to client */
	*paddr_ptr = sg_phys(table->sgl);
	*len_ptr = (size_t)sg_dma_len(table->sgl);

	/* fill up mapping_info */
	mapping_info = kzalloc(sizeof(struct cam_sec_buff_info), GFP_KERNEL);
	if (!mapping_info) {
		rc = -ENOMEM;
		goto err_unmap_sg;
	}

	mapping_info->ion_fd = ion_fd;
	mapping_info->paddr = *paddr_ptr;
	mapping_info->len = *len_ptr;
	mapping_info->dir = dma_dir;
	mapping_info->ref_count = 1;
	mapping_info->buf = dmabuf;

	CAM_DBG(CAM_SMMU, "idx=%d, ion_fd=%d, dev=%pK, paddr=%pK, len=%u",
			idx, ion_fd,
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)*paddr_ptr, (unsigned int)*len_ptr);

	/* add to the list */
	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].smmu_buf_list);

	return 0;

err_unmap_sg:
	dma_buf_unmap_attachment(attach, table, dma_dir);
err_detach:
	dma_buf_detach(dmabuf, attach);
err_put:
	dma_buf_put(dmabuf);
err_out:
	return rc;
}

int cam_smmu_map_stage2_iova(int handle,
		int ion_fd, enum cam_smmu_map_dir dir,
		dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	int idx, rc;
	enum dma_data_direction dma_dir;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid inputs, paddr_ptr:%pK, len_ptr: %pK",
			paddr_ptr, len_ptr);
		return -EINVAL;
	}
	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		CAM_ERR(CAM_SMMU,
			"Error: translate direction failed. dir = %d", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if ((handle == HANDLE_INIT) ||
		(idx < 0) ||
		(idx >= iommu_cb_set.cb_num)) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't map secure mem to non secure cb, idx=%d",
			idx);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, idx=%d, table_hdl=%x, hdl=%x",
			idx, iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_secure_fd_in_list(idx, ion_fd, paddr_ptr,
			len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CAM_DBG(CAM_SMMU,
			"fd:%d already in list idx:%d, handle=%d give same addr back",
			ion_fd, idx, handle);
		rc = 0;
		goto get_addr_end;
	}
	rc = cam_smmu_map_stage2_buffer_and_add_to_list(idx, ion_fd, dma_dir,
			paddr_ptr, len_ptr);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Error: mapping or add list fail, idx=%d, handle=%d, fd=%d, rc=%d",
			idx, handle, ion_fd, rc);
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_map_stage2_iova);

static int cam_smmu_secure_unmap_buf_and_remove_from_list(
		struct cam_sec_buff_info *mapping_info,
		int idx)
{
	if (!mapping_info) {
		CAM_ERR(CAM_SMMU, "Error: List doesn't exist");
		return -EINVAL;
	}
	dma_buf_put(mapping_info->buf);
	list_del_init(&mapping_info->list);

	CAM_DBG(CAM_SMMU, "unmap fd: %d, idx : %d", mapping_info->ion_fd, idx);

	/* free one buffer */
	kfree(mapping_info);
	return 0;
}

int cam_smmu_unmap_stage2_iova(int handle, int ion_fd)
{
	int idx, rc;
	struct cam_sec_buff_info *mapping_info;

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if ((handle == HANDLE_INIT) ||
		(idx < 0) ||
		(idx >= iommu_cb_set.cb_num)) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't unmap secure mem from non secure cb");
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto put_addr_end;
	}

	/* based on ion fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_sec_buf_idx(idx, ion_fd);
	if (!mapping_info) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params! idx = %d, fd = %d",
			idx, ion_fd);
		rc = -EINVAL;
		goto put_addr_end;
	}

	mapping_info->ref_count--;
	if (mapping_info->ref_count > 0) {
		CAM_DBG(CAM_SMMU,
			"idx: %d fd = %d ref_count: %d",
			idx, ion_fd, mapping_info->ref_count);
		rc = 0;
		goto put_addr_end;
	}
	mapping_info->ref_count = 0;

	/* unmapping one buffer from device */
	rc = cam_smmu_secure_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc) {
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");
		goto put_addr_end;
	}

put_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_unmap_stage2_iova);

static int cam_smmu_map_iova_validate_params(int handle,
	enum cam_smmu_map_dir dir,
	dma_addr_t *paddr_ptr, size_t *len_ptr,
	enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;
	enum dma_data_direction dma_dir;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU, "Input pointers are invalid");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Invalid handle");
		return -EINVAL;
	}

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	if (region_id != CAM_SMMU_REGION_SHARED)
		*len_ptr = (size_t)0;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		CAM_ERR(CAM_SMMU, "translate direction failed. dir = %d", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	return rc;
}

int cam_smmu_map_user_iova(int handle, int ion_fd,
	enum cam_smmu_map_dir dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;
	enum dma_data_direction dma_dir;

	rc = cam_smmu_map_iova_validate_params(handle, dir, paddr_ptr,
		len_ptr, region_id);
	if (rc) {
		CAM_ERR(CAM_SMMU, "initial checks failed, unable to proceed");
		return rc;
	}

	dma_dir = (enum dma_data_direction)dir;
	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't map non-secure mem to secure cb idx=%d",
			idx);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"hdl is not valid, idx=%d, table_hdl = %x, hdl = %x",
			idx, iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		CAM_ERR(CAM_SMMU,
			"Err:Dev %s should call SMMU attach before map buffer",
			iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, ion_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CAM_ERR(CAM_SMMU,
			"fd:%d already in list idx:%d, handle=%d, give same addr back",
			ion_fd, idx, handle);
		rc = -EALREADY;
		goto get_addr_end;
	}

	rc = cam_smmu_map_buffer_and_add_to_list(idx, ion_fd, dma_dir,
			paddr_ptr, len_ptr, region_id);
	if (rc < 0)
		CAM_ERR(CAM_SMMU,
			"mapping or add list fail, idx=%d, fd=%d, region=%d, rc=%d",
			idx, ion_fd, region_id, rc);

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_map_user_iova);

int cam_smmu_map_kernel_iova(int handle, struct dma_buf *buf,
	enum cam_smmu_map_dir dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr, enum cam_smmu_region_id region_id)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;
	enum dma_data_direction dma_dir;

	rc = cam_smmu_map_iova_validate_params(handle, dir, paddr_ptr,
		len_ptr, region_id);
	if (rc) {
		CAM_ERR(CAM_SMMU, "initial checks failed, unable to proceed");
		return rc;
	}

	dma_dir = cam_smmu_translate_dir(dir);
	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't map non-secure mem to secure cb");
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU, "hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		CAM_ERR(CAM_SMMU,
			"Err:Dev %s should call SMMU attach before map buffer",
			iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_dma_buf_in_list(idx, buf,
	paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CAM_ERR(CAM_SMMU,
			"dma_buf :%pK already in the list", buf);
		rc = -EALREADY;
		goto get_addr_end;
	}

	rc = cam_smmu_map_kernel_buffer_and_add_to_list(idx, buf, dma_dir,
			paddr_ptr, len_ptr, region_id);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "mapping or add list fail");

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_map_kernel_iova);

int cam_smmu_get_iova(int handle, int ion_fd,
	dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU, "Error: Input pointers are invalid");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't get non-secure mem from secure cb");
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, ion_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_NOT_EXIST) {
		CAM_ERR(CAM_SMMU, "ion_fd:%d not in the mapped list", ion_fd);
		rc = -EINVAL;
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_get_iova);

int cam_smmu_get_stage2_iova(int handle, int ion_fd,
	dma_addr_t *paddr_ptr, size_t *len_ptr)
{
	int idx, rc = 0;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		CAM_ERR(CAM_SMMU, "Error: Input pointers are invalid");
		return -EINVAL;
	}

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	if (!iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't get secure mem from non secure cb");
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_validate_secure_fd_in_list(idx,
		ion_fd,
		paddr_ptr,
		len_ptr);

	if (buf_state == CAM_SMMU_BUFF_NOT_EXIST) {
		CAM_ERR(CAM_SMMU, "ion_fd:%d not in the mapped list", ion_fd);
		rc = -EINVAL;
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_get_stage2_iova);

static int cam_smmu_unmap_validate_params(int handle)
{
	int idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	return 0;
}

int cam_smmu_unmap_user_iova(int handle,
	int ion_fd, enum cam_smmu_region_id region_id)
{
	int idx, rc;
	struct cam_dma_buff_info *mapping_info;

	rc = cam_smmu_unmap_validate_params(handle);
	if (rc) {
		CAM_ERR(CAM_SMMU, "unmap util validation failure");
		return rc;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't unmap non-secure mem from secure cb");
		rc = -EINVAL;
		goto unmap_end;
	}

	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Based on ion_fd & index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, ion_fd);

	if (!mapping_info) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params idx = %d, fd = %d",
			idx, ion_fd);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Unmapping one buffer from device */
	CAM_DBG(CAM_SMMU, "SMMU: removing buffer idx = %d", idx);
	rc = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");

unmap_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_unmap_user_iova);

int cam_smmu_unmap_kernel_iova(int handle,
	struct dma_buf *buf, enum cam_smmu_region_id region_id)
{
	int idx, rc;
	struct cam_dma_buff_info *mapping_info;

	rc = cam_smmu_unmap_validate_params(handle);
	if (rc) {
		CAM_ERR(CAM_SMMU, "unmap util validation failure");
		return rc;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].is_secure) {
		CAM_ERR(CAM_SMMU,
			"Error: can't unmap non-secure mem from secure cb");
		rc = -EINVAL;
		goto unmap_end;
	}

	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Based on dma_buf & index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_dma_buf(idx, buf);

	if (!mapping_info) {
		CAM_ERR(CAM_SMMU,
			"Error: Invalid params idx = %d, dma_buf = %pK",
			idx, buf);
		rc = -EINVAL;
		goto unmap_end;
	}

	/* Unmapping one buffer from device */
	CAM_DBG(CAM_SMMU, "SMMU: removing buffer idx = %d", idx);
	rc = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0)
		CAM_ERR(CAM_SMMU, "Error: unmap or remove list fail");

unmap_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_unmap_kernel_iova);


int cam_smmu_put_iova(int handle, int ion_fd)
{
	int idx;
	int rc = 0;
	struct cam_dma_buff_info *mapping_info;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto put_addr_end;
	}

	/* based on ion fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, ion_fd);
	if (!mapping_info) {
		CAM_ERR(CAM_SMMU, "Error: Invalid params idx = %d, fd = %d",
			idx, ion_fd);
		rc = -EINVAL;
		goto put_addr_end;
	}

put_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_put_iova);

int cam_smmu_destroy_handle(int handle)
{
	int idx;

	if (handle == HANDLE_INIT) {
		CAM_ERR(CAM_SMMU, "Error: Invalid handle");
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU,
			"Error: handle or index invalid. idx = %d hdl = %x",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		CAM_ERR(CAM_SMMU,
			"Error: hdl is not valid, table_hdl = %x, hdl = %x",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (!list_empty_careful(&iommu_cb_set.cb_info[idx].smmu_buf_list)) {
		CAM_ERR(CAM_SMMU, "UMD %s buffer list is not clean",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_user_list(idx);
		cam_smmu_clean_user_buffer_list(idx);
	}

	if (!list_empty_careful(
		&iommu_cb_set.cb_info[idx].smmu_buf_kernel_list)) {
		CAM_ERR(CAM_SMMU, "KMD %s buffer list is not clean",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_kernel_list(idx);
		cam_smmu_clean_kernel_buffer_list(idx);
	}

	if (iommu_cb_set.cb_info[idx].is_secure) {
		if (iommu_cb_set.cb_info[idx].secure_count == 0) {
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return -EPERM;
		}

		iommu_cb_set.cb_info[idx].secure_count--;
		if (iommu_cb_set.cb_info[idx].secure_count == 0) {
			iommu_cb_set.cb_info[idx].cb_count = 0;
			iommu_cb_set.cb_info[idx].handle = HANDLE_INIT;
		}

		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return 0;
	}

	iommu_cb_set.cb_info[idx].cb_count = 0;
	iommu_cb_set.cb_info[idx].handle = HANDLE_INIT;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_destroy_handle);

static void cam_smmu_deinit_cb(struct cam_context_bank_info *cb)
{
	arm_iommu_detach_device(cb->dev);

	if (cb->io_support && cb->mapping) {
		arm_iommu_release_mapping(cb->mapping);
		cb->mapping = NULL;
	}

	if (cb->shared_support) {
		gen_pool_destroy(cb->shared_mem_pool);
		cb->shared_mem_pool = NULL;
	}

	if (cb->scratch_buf_support) {
		kfree(cb->scratch_map.bitmap);
		cb->scratch_map.bitmap = NULL;
	}
}

static void cam_smmu_release_cb(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++)
		cam_smmu_deinit_cb(&iommu_cb_set.cb_info[i]);

	devm_kfree(&pdev->dev, iommu_cb_set.cb_info);
	iommu_cb_set.cb_num = 0;
}

static int cam_smmu_setup_cb(struct cam_context_bank_info *cb,
	struct device *dev)
{
	int rc = 0;

	if (!cb || !dev) {
		CAM_ERR(CAM_SMMU, "Error: invalid input params");
		return -EINVAL;
	}

	cb->dev = dev;
	cb->is_fw_allocated = false;
	cb->is_secheap_allocated = false;

	/* Create a pool with 64K granularity for supporting shared memory */
	if (cb->shared_support) {
		cb->shared_mem_pool = gen_pool_create(
			SHARED_MEM_POOL_GRANULARITY, -1);

		if (!cb->shared_mem_pool)
			return -ENOMEM;

		rc = gen_pool_add(cb->shared_mem_pool,
			cb->shared_info.iova_start,
			cb->shared_info.iova_len,
			-1);

		CAM_DBG(CAM_SMMU, "Shared mem start->%lX",
			(unsigned long)cb->shared_info.iova_start);
		CAM_DBG(CAM_SMMU, "Shared mem len->%zu",
			cb->shared_info.iova_len);

		if (rc) {
			CAM_ERR(CAM_SMMU, "Genpool chunk creation failed");
			gen_pool_destroy(cb->shared_mem_pool);
			cb->shared_mem_pool = NULL;
			return rc;
		}
	}

	if (cb->scratch_buf_support) {
		rc = cam_smmu_init_scratch_map(&cb->scratch_map,
			cb->scratch_info.iova_start,
			cb->scratch_info.iova_len,
			0);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU,
				"Error: failed to create scratch map");
			rc = -ENODEV;
			goto end;
		}
	}

	/* create a virtual mapping */
	if (cb->io_support) {
		cb->mapping = arm_iommu_create_mapping(&platform_bus_type,
			cb->io_info.iova_start, cb->io_info.iova_len);
		if (IS_ERR(cb->mapping)) {
			CAM_ERR(CAM_SMMU, "Error: create mapping Failed");
			rc = -ENODEV;
			goto end;
		}

		iommu_cb_set.non_fatal_fault = smmu_fatal_flag;
		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_NON_FATAL_FAULTS,
			&iommu_cb_set.non_fatal_fault) < 0) {
			CAM_ERR(CAM_SMMU,
				"Error: failed to set non fatal fault attribute");
		}

	} else {
		CAM_ERR(CAM_SMMU, "Context bank does not have IO region");
		rc = -ENODEV;
		goto end;
	}

	return rc;
end:
	if (cb->shared_support) {
		gen_pool_destroy(cb->shared_mem_pool);
		cb->shared_mem_pool = NULL;
	}

	if (cb->scratch_buf_support) {
		kfree(cb->scratch_map.bitmap);
		cb->scratch_map.bitmap = NULL;
	}

	return rc;
}

static int cam_alloc_smmu_context_banks(struct device *dev)
{
	struct device_node *domains_child_node = NULL;

	if (!dev) {
		CAM_ERR(CAM_SMMU, "Error: Invalid device");
		return -ENODEV;
	}

	iommu_cb_set.cb_num = 0;

	/* traverse thru all the child nodes and increment the cb count */
	for_each_available_child_of_node(dev->of_node, domains_child_node) {
		if (of_device_is_compatible(domains_child_node,
			"qcom,msm-cam-smmu-cb"))
			iommu_cb_set.cb_num++;

		if (of_device_is_compatible(domains_child_node,
			"qcom,qsmmu-cam-cb"))
			iommu_cb_set.cb_num++;
	}

	if (iommu_cb_set.cb_num == 0) {
		CAM_ERR(CAM_SMMU, "Error: no context banks present");
		return -ENOENT;
	}

	/* allocate memory for the context banks */
	iommu_cb_set.cb_info = devm_kzalloc(dev,
		iommu_cb_set.cb_num * sizeof(struct cam_context_bank_info),
		GFP_KERNEL);

	if (!iommu_cb_set.cb_info) {
		CAM_ERR(CAM_SMMU, "Error: cannot allocate context banks");
		return -ENOMEM;
	}

	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_INIT);
	iommu_cb_set.cb_init_count = 0;

	CAM_DBG(CAM_SMMU, "no of context banks :%d", iommu_cb_set.cb_num);
	return 0;
}

static int cam_smmu_get_memory_regions_info(struct device_node *of_node,
	struct cam_context_bank_info *cb)
{
	int rc = 0;
	struct device_node *mem_map_node = NULL;
	struct device_node *child_node = NULL;
	const char *region_name;
	int num_regions = 0;

	if (!of_node || !cb) {
		CAM_ERR(CAM_SMMU, "Invalid argument(s)");
		return -EINVAL;
	}

	mem_map_node = of_get_child_by_name(of_node, "iova-mem-map");
	cb->is_secure = of_property_read_bool(of_node, "qcom,secure-cb");

	/*
	 * We always expect a memory map node, except when it is a secure
	 * context bank.
	 */
	if (!mem_map_node) {
		if (cb->is_secure)
			return 0;
		CAM_ERR(CAM_SMMU, "iova-mem-map not present");
		return -EINVAL;
	}

	for_each_available_child_of_node(mem_map_node, child_node) {
		uint32_t region_start;
		uint32_t region_len;
		uint32_t region_id;
		uint32_t qdss_region_phy_addr = 0;

		num_regions++;
		rc = of_property_read_string(child_node,
			"iova-region-name", &region_name);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "IOVA region not found");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-start", &region_start);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-start");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-len", &region_len);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-len");
			return -EINVAL;
		}

		rc = of_property_read_u32(child_node,
			"iova-region-id", &region_id);
		if (rc < 0) {
			of_node_put(mem_map_node);
			CAM_ERR(CAM_SMMU, "Failed to read iova-region-id");
			return -EINVAL;
		}

		if (strcmp(region_name, qdss_region_name) == 0) {
			rc = of_property_read_u32(child_node,
				"qdss-phy-addr", &qdss_region_phy_addr);
			if (rc < 0) {
				of_node_put(mem_map_node);
				CAM_ERR(CAM_SMMU,
					"Failed to read qdss phy addr");
				return -EINVAL;
			}
		}

		switch (region_id) {
		case CAM_SMMU_REGION_FIRMWARE:
			cb->firmware_support = 1;
			cb->firmware_info.iova_start = region_start;
			cb->firmware_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_SHARED:
			cb->shared_support = 1;
			cb->shared_info.iova_start = region_start;
			cb->shared_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_SCRATCH:
			cb->scratch_buf_support = 1;
			cb->scratch_info.iova_start = region_start;
			cb->scratch_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_IO:
			cb->io_support = 1;
			cb->io_info.iova_start = region_start;
			cb->io_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_SECHEAP:
			cb->secheap_support = 1;
			cb->secheap_info.iova_start = region_start;
			cb->secheap_info.iova_len = region_len;
			break;
		case CAM_SMMU_REGION_QDSS:
			cb->qdss_support = 1;
			cb->qdss_info.iova_start = region_start;
			cb->qdss_info.iova_len = region_len;
			cb->qdss_phy_addr = qdss_region_phy_addr;
			break;
		default:
			CAM_ERR(CAM_SMMU,
				"Incorrect region id present in DT file: %d",
				region_id);
		}

		CAM_DBG(CAM_SMMU, "Found label -> %s", cb->name);
		CAM_DBG(CAM_SMMU, "Found region -> %s", region_name);
		CAM_DBG(CAM_SMMU, "region_start -> %X", region_start);
		CAM_DBG(CAM_SMMU, "region_len -> %X", region_len);
		CAM_DBG(CAM_SMMU, "region_id -> %X", region_id);
	}
	of_node_put(mem_map_node);

	if (!num_regions) {
		CAM_ERR(CAM_SMMU,
			"No memory regions found, at least one needed");
		rc = -ENODEV;
	}

	return rc;
}

static int cam_populate_smmu_context_banks(struct device *dev,
	enum cam_iommu_type type)
{
	int rc = 0;
	struct cam_context_bank_info *cb;
	struct device *ctx = NULL;

	if (!dev) {
		CAM_ERR(CAM_SMMU, "Error: Invalid device");
		return -ENODEV;
	}

	/* check the bounds */
	if (iommu_cb_set.cb_init_count >= iommu_cb_set.cb_num) {
		CAM_ERR(CAM_SMMU, "Error: populate more than allocated cb");
		rc = -EBADHANDLE;
		goto cb_init_fail;
	}

	/* read the context bank from cb set */
	cb = &iommu_cb_set.cb_info[iommu_cb_set.cb_init_count];

	/* set the name of the context bank */
	rc = of_property_read_string(dev->of_node, "label", &cb->name);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU,
			"Error: failed to read label from sub device");
		goto cb_init_fail;
	}

	rc = cam_smmu_get_memory_regions_info(dev->of_node,
		cb);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: Getting region info");
		return rc;
	}

	if (cb->is_secure) {
		/* increment count to next bank */
		cb->dev = dev;
		iommu_cb_set.cb_init_count++;
		return 0;
	}

	/* set up the iommu mapping for the  context bank */
	if (type == CAM_QSMMU) {
		CAM_ERR(CAM_SMMU, "Error: QSMMU ctx not supported for : %s",
			cb->name);
		return -ENODEV;
	}

	ctx = dev;
	CAM_DBG(CAM_SMMU, "getting Arm SMMU ctx : %s", cb->name);

	rc = cam_smmu_setup_cb(cb, ctx);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: failed to setup cb : %s", cb->name);
		goto cb_init_fail;
	}
	if (cb->io_support && cb->mapping)
		iommu_set_fault_handler(cb->mapping->domain,
			cam_smmu_iommu_fault_handler,
			(void *)cb->name);
	/* increment count to next bank */
	iommu_cb_set.cb_init_count++;

	CAM_DBG(CAM_SMMU, "X: cb init count :%d", iommu_cb_set.cb_init_count);

cb_init_fail:
	return rc;
}

static int cam_smmu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device *dev = &pdev->dev;

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu")) {
		rc = cam_alloc_smmu_context_banks(dev);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: allocating context banks");
			return -ENOMEM;
		}
	}
	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_ARM_SMMU);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: populating context banks");
			cam_smmu_release_cb(pdev);
			return -ENOMEM;
		}
		return rc;
	}
	if (of_device_is_compatible(dev->of_node, "qcom,qsmmu-cam-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_QSMMU);
		if (rc < 0) {
			CAM_ERR(CAM_SMMU, "Error: populating context banks");
			return -ENOMEM;
		}
		return rc;
	}

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-fw-dev")) {
		icp_fw.fw_dev = &pdev->dev;
		icp_fw.fw_kva = NULL;
		icp_fw.fw_dma_hdl = 0;
		return rc;
	}

	/* probe through all the subdevices */
	rc = of_platform_populate(pdev->dev.of_node, msm_cam_smmu_dt_match,
				NULL, &pdev->dev);
	if (rc < 0) {
		CAM_ERR(CAM_SMMU, "Error: populating devices");
	} else {
		INIT_WORK(&iommu_cb_set.smmu_work, cam_smmu_page_fault_work);
		mutex_init(&iommu_cb_set.payload_list_lock);
		INIT_LIST_HEAD(&iommu_cb_set.payload_list);
	}

	return rc;
}

static int cam_smmu_remove(struct platform_device *pdev)
{
	/* release all the context banks and memory allocated */
	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_DEINIT);
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
		.suppress_bind_attrs = true,
	},
};

static int __init cam_smmu_init_module(void)
{
	cam_smmu_create_debugfs_entry();
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
