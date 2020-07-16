/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Chungying Lu <chungying.lu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/slab.h>

#include "vpu_load_image.h"

#define ALGO_IMAGES_NUM			(2)
#define PARTITION_HEADER_SZ		(0x200)
#define VPU_ALIGN_MASK			(0xF)

#define VPU_IMAGE_FIRMWARE_INDEX	(0)
#define VPU_IMAGE_MTK_ALGO_INDEX	(1)
#define VPU_IMAGE_CUST_ALGO_INDEX	(2)
#define VPU_IMAGE_ALGO_BASE_INDEX	(VPU_IMAGE_MTK_ALGO_INDEX)

struct iram_desc {
	uint32_t bin_offset;
	uint32_t addr;
	uint32_t mem_size;
};

struct iram_desc_header {
	uint32_t iram_num;
	struct iram_desc iram_desc[VPU_MAX_NUM_CODE_SEGMENTS];
};

static char *g_vpu_image_name[VPU_NUMS_IMAGE_HEADER] = {
	"cam_vpu1.img",
	"cam_vpu2.img",
	"cam_vpu3.img"
};

static const uint32_t g_vpu_mva_reset_vector[] = {
	VPU_MVA_RESET_VECTOR,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_RESET_VECTOR,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_RESET_VECTOR,
#endif
};

static const uint32_t g_vpu_mva_main_program[] = {
	VPU_MVA_MAIN_PROGRAM,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_MAIN_PROGRAM,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_MAIN_PROGRAM,
#endif
};

static const uint32_t g_vpu_mva_kernel_lib[] = {
	VPU_MVA_KERNEL_LIB,
#if (MTK_VPU_CORE >= 2)
	VPU2_MVA_KERNEL_LIB,
#endif
#if (MTK_VPU_CORE >= 3)
	VPU3_MVA_KERNEL_LIB,
#endif
};

static void vpu_unload_firmware(struct vpu_device *vpu_device);
static void vpu_unload_algo(struct vpu_device *vpu_device);
static void _vpu_unload_image(struct vpu_device *vpu_device);

static int vpu_load_firmware(struct vpu_device *vpu_device)
{
	struct vpu_core *vpu_core = NULL;
	struct vpu_shared_memory_param mem_param;
	struct vpu_shared_memory *share_memory = NULL;
	struct vpu_image_header *header = NULL;
	struct vpu_code_segment *code_segment = NULL;
	struct iram_desc_header *iram_desc_header[MTK_VPU_CORE] = {NULL};
	struct iram_desc *iram_desc = NULL;
	const struct firmware *fw = NULL;
	uint64_t dst = 0;
	uint32_t iram_bin_offset[MTK_VPU_CORE] = {0};
	uint32_t iram_num = 0;
	uint32_t dst_addr_mask = 0;
	int i, core, len;
	int ret = 0;

	for (i = 0; i < vpu_device->core_num; i++) {
		vpu_core = vpu_device->vpu_core[i];

		/* Reset vector */
		memset(&mem_param, 0x0, sizeof(mem_param));
		mem_param.require_pa = true;
		mem_param.size = VPU_SIZE_RESET_VECTOR;
		mem_param.fixed_addr = g_vpu_mva_reset_vector[i];
		ret = vpu_alloc_shared_memory(vpu_device,
						&vpu_core->reset_vector,
						&mem_param);
		if (ret) {
			LOG_ERR("fail to allocate reset vector!\n");
			goto exit;
		}

		/* Main program */
		memset(&mem_param, 0x0, sizeof(mem_param));
		mem_param.require_pa = true;
		mem_param.size = VPU_SIZE_MAIN_PROGRAM;
		mem_param.fixed_addr = g_vpu_mva_main_program[i];
		ret = vpu_alloc_shared_memory(vpu_device,
						&vpu_core->main_program,
						&mem_param);
		if (ret) {
			LOG_ERR("fail to allocate main program!\n");
			goto exit;
		}

		/* Iram data */
		memset(&mem_param, 0x0, sizeof(mem_param));
		mem_param.require_pa = true;
		mem_param.size = VPU_SIZE_MAIN_PROGRAM_IMEM;
		ret = vpu_alloc_shared_memory(vpu_device, &vpu_core->iram_data,
						&mem_param);
		if (ret) {
			LOG_ERR("fail to allocate iram data!\n");
			goto exit;
		}

		iram_desc_header[i] =
		(struct iram_desc_header *)(uintptr_t)vpu_core->iram_data->va;
		iram_desc_header[i]->iram_num = 0;
		iram_bin_offset[i] =
			((sizeof(struct iram_desc_header) + VPU_ALIGN_MASK) &
				~VPU_ALIGN_MASK);

		vpu_core->iram_data_mva = (uint64_t)(vpu_core->iram_data->pa);
	}

	fw = vpu_device->fw[0];

	header = &vpu_device->image_header[0];
	memcpy((void *)header, fw->data + PARTITION_HEADER_SZ,
		sizeof(struct vpu_image_header));

	for (i = 0; i < header->code_segment_count; i++) {
		code_segment = &header->code_segments[i];

		for (core = 0; core < vpu_device->core_num; core++) {
			if ((code_segment->vpu_core & (1 << core)) == 0)
				continue;

			vpu_core = vpu_device->vpu_core[core];
			dst_addr_mask = code_segment->dst_addr & VPU_ADDR_MASK;
			if (dst_addr_mask == g_vpu_mva_reset_vector[core]) {
				share_memory = vpu_core->reset_vector;
				dst = share_memory->va +
					(code_segment->dst_addr -
						g_vpu_mva_reset_vector[core]);
			} else if (dst_addr_mask ==
					g_vpu_mva_main_program[core]) {
				share_memory = vpu_core->main_program;
				dst = share_memory->va +
					(code_segment->dst_addr -
						g_vpu_mva_main_program[core]);
			} else if (dst_addr_mask == 0x7FF00000) {
				share_memory = vpu_core->iram_data;
				iram_num = iram_desc_header[core]->iram_num;
				iram_desc =
				&iram_desc_header[core]->iram_desc[iram_num];

				dst = share_memory->va + iram_bin_offset[core];
				iram_desc->addr = code_segment->dst_addr;
				iram_desc->bin_offset = iram_bin_offset[core];
				iram_desc->mem_size = code_segment->length;
				iram_desc_header[core]->iram_num++;

				iram_bin_offset[core] +=
					((iram_desc->mem_size + VPU_ALIGN_MASK)
						& ~VPU_ALIGN_MASK);
			} else {
				break;
			}

			memcpy((void *)(uintptr_t)dst,
			(fw->data + PARTITION_HEADER_SZ + code_segment->offset),
			code_segment->file_size);

			len = code_segment->length - code_segment->file_size;
			if (len > 0) {
				LOG_INF(
					"[%s][%d] seg[%d]: memset size 0x%x from dst 0x%llx\n",
					__func__, __LINE__, i, len, dst);
				memset(
			(void *)(uintptr_t)(dst + code_segment->file_size),
				0, len);
			}
		}
	}

exit:
	if (fw)
		release_firmware(fw);

	vpu_device->fw[0] = NULL;

	if (ret)
		vpu_unload_firmware(vpu_device);

	return ret;
}

static void vpu_unload_firmware(struct vpu_device *vpu_device)
{
	int i;

	for (i = 0; i < vpu_device->core_num; i++)
		vpu_unmap_mva_of_bin(vpu_device->vpu_core[i]);

	memset((void *)&vpu_device->image_header[0], 0x0,
		sizeof(struct vpu_image_header));
}

static int vpu_load_algo(struct vpu_device *vpu_device)
{
	struct vpu_image_header *header[ALGO_IMAGES_NUM] = {NULL};
	struct vpu_image_header *algo_image_header = NULL;
	struct vpu_shared_memory_param mem_param;
	struct vpu_shared_memory *share_memory = NULL;
	struct vpu_algo_info *algo_info = NULL;
	const struct firmware *fw[ALGO_IMAGES_NUM] = {NULL};
	int i, j;
	int num_algos[ALGO_IMAGES_NUM] = {0}, size_algos = 0;
	int algos = 0, offs = 0;
	int core_mask = 0;
	int algo_images_num = 0;
	int ret = 0;
#if 0
	const char *line_bar =
		"+------+-----+--------------------------------+--------+-----------+----------+"
		;
#endif

	/* load algo header by core */
	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		fw[i] = vpu_device->fw[i + 1];

		header[i] = kzalloc(sizeof(struct vpu_image_header),
					GFP_KERNEL);
		if (header[i] == NULL) {
			LOG_ERR("[%s][%d] alloc image header[%d] failed!\n",
				__func__, __LINE__, i);
			goto exit;
		}

		if (fw[i]) {
			/* read header */
			memcpy((void *)header[i],
			       fw[i]->data + PARTITION_HEADER_SZ,
			       sizeof(struct vpu_image_header));
			algo_images_num++;
		}
	}


	/* No algo image exist, but user can create algo by their own */
	/* Hence return 0 */
	if (algo_images_num == 0) {
		ret = 0;
		goto exit;
	}

	for (i = 0; i < vpu_device->core_num; i++)
		core_mask |= (1 << i);

	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		for (j = 0; j < header[i]->algo_info_count; j++) {
			algo_info = &header[i]->algo_infos[j];
			if (algo_info->vpu_core & core_mask) {
				size_algos +=
					(algo_info->length + VPU_ALIGN_MASK)
						& (~VPU_ALIGN_MASK);
				num_algos[i]++;
				algos++;
			}
		}
	}

	LOG_INF("total algo len = %d, num algo = %d\n", size_algos, algos);

	memset(&mem_param, 0x0, sizeof(mem_param));
	mem_param.require_pa = true;
	mem_param.size = size_algos;

	ret = vpu_alloc_shared_memory(vpu_device,
				      &vpu_device->algo_binary_data,
				      &mem_param);
	if (ret) {
		LOG_ERR("fail to allocate algo buffer!\n");
		goto exit;
	}

	share_memory = vpu_device->algo_binary_data;
	for (i = 0; i < vpu_device->core_num; i++)
		vpu_device->vpu_core[i]->algo_data_mva = share_memory->pa;

	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		algo_image_header =
		&vpu_device->image_header[VPU_IMAGE_ALGO_BASE_INDEX + i];
		memcpy(algo_image_header, header[i],
			sizeof(struct vpu_image_header));
		algo_image_header->algo_info_count = num_algos[i];

		algos = 0;
		if (fw[i]) {
			for (j = 0; j < header[i]->algo_info_count; j++) {
				algo_info = &header[i]->algo_infos[j];
				if ((algo_info->vpu_core & core_mask) == 0x0)
					continue;

				/* read algo */
				memcpy((void *)(uintptr_t)
						(share_memory->va + offs),
					fw[i]->data + PARTITION_HEADER_SZ
						+ algo_info->offset,
					algo_info->length);

				/* copy algo info */
				memcpy(&algo_image_header->algo_infos[algos],
					algo_info,
					sizeof(struct vpu_algo_info));

				algo_image_header->algo_infos[algos].offset =
					VPU_OFFSET_ALGO_AREA + offs;

				offs += (algo_info->length + VPU_ALIGN_MASK)
						& (~VPU_ALIGN_MASK);
				algos++;
			}
		}
	}

#if 0
	LOG_DBG("%s\n", line_bar);
	LOG_DBG("|%-6s|%-5s|%-32s|%-8s|%-11s|%-10s|\n",
		"Num", "Image", "Name", "Core", "Offset", "Length");
	LOG_DBG("%s\n", line_bar);
	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		algo_image_header = &vpu_device->image_header[i + 1];
		for (j = 0; j < algo_image_header->algo_info_count; j++)  {
			algo_info = &algo_image_header->algo_infos[j];
			LOG_DBG("|%-6d|%-5d|%-32s|0x%-6lx|0x%-9x|0x%-8x|\n",
				j, i, algo_info->name,
				(unsigned long)(algo_info->vpu_core),
				algo_info->offset,
				algo_info->length);
		}
	}
	LOG_DBG("%s\n", line_bar);
#endif

	for (i = 0; i < vpu_device->core_num; i++)
		vpu_total_algo_num(vpu_device->vpu_core[i]);

exit:
	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		if (fw[i])
			release_firmware(fw[i]);

		vpu_device->fw[i + 1] = NULL;

		if (header[i] != NULL)
			kfree(header[i]);
	}

	if (ret)
		vpu_unload_algo(vpu_device);

	return ret;
}

static void vpu_unload_algo(struct vpu_device *vpu_device)
{
	struct vpu_image_header *algo_image_header = NULL;
	int i;

	if (vpu_device->algo_binary_data) {
		vpu_free_shared_memory(vpu_device,
				       vpu_device->algo_binary_data);
		vpu_device->algo_binary_data = NULL;
	}

	for (i = 0; i < vpu_device->core_num; i++)
		vpu_device->vpu_core[i]->algo_data_mva = 0;

	for (i = 0; i < ALGO_IMAGES_NUM; i++) {
		algo_image_header =
			&vpu_device->image_header[VPU_IMAGE_ALGO_BASE_INDEX + i]
			;
		memset((void *)algo_image_header, 0x0,
			sizeof(struct vpu_image_header));
	}
}

static void vpu_request_firmware_cbk(const struct firmware *fw, void *context)
{
	struct vpu_device *vpu_device = NULL;
	int i, index = 0;
	int ret = 0;

	vpu_device = ((struct vpu_image_ctx *)context)->vpu_device;
	index = ((struct vpu_image_ctx *)context)->vpu_image_index;

	mutex_lock(&vpu_device->vpu_load_image_lock);

	vpu_device->fw[index] = fw;
	vpu_device->vpu_req_firmware_cbk[index] = true;

	if (!fw) {
		if (index == VPU_IMAGE_FIRMWARE_INDEX) {
			LOG_ERR("[%s][%d] load firmware failed!\n",
				__func__, __LINE__);
			vpu_device->vpu_load_image_state =
					VPU_LOAD_IMAGE_FAILED;
			goto exit;
		}

		/* Even one of algo image is null,   */
		/* still have to enter vpu_load_algo */
		LOG_ERR("%s: firmware %d load failed\n", __func__, index);
	}

	switch (index) {
	case VPU_IMAGE_FIRMWARE_INDEX:
		ret = vpu_load_firmware(vpu_device);
		if (ret) {
			LOG_ERR("[%s][%d] vpu_load_firmware failed!\n",
					__func__, __LINE__);
			vpu_device->vpu_load_image_state =
					VPU_LOAD_IMAGE_FAILED;
		}
		break;

	case VPU_IMAGE_MTK_ALGO_INDEX:
	case VPU_IMAGE_CUST_ALGO_INDEX:
		if (vpu_device->vpu_req_firmware_cbk[VPU_IMAGE_MTK_ALGO_INDEX]
	&& vpu_device->vpu_req_firmware_cbk[VPU_IMAGE_CUST_ALGO_INDEX]) {
			ret = vpu_load_algo(vpu_device);
			if (ret)
				LOG_ERR("[%s][%d] vpu_load_algo failed!\n",
					__func__, __LINE__);
		}
		break;
	}

exit:
	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; ++i)
		if (vpu_device->vpu_req_firmware_cbk[i] == false)
			break;

	if (i == VPU_NUMS_IMAGE_HEADER) {
		if (vpu_device->vpu_load_image_state
				== VPU_LOAD_IMAGE_LOADING) {
			vpu_device->vpu_load_image_state =
				VPU_LOAD_IMAGE_LOADED;
			LOG_INF("[%s][%d] vpu load image done!\n",
				__func__, __LINE__);
		} else {
			_vpu_unload_image(vpu_device);
			LOG_INF("[%s][%d] vpu load image failed! state: %d\n",
				__func__, __LINE__,
				vpu_device->vpu_load_image_state);
		}

		kfree(vpu_device->vpu_image_ctx);
		vpu_device->vpu_image_ctx = NULL;
	}

	mutex_unlock(&vpu_device->vpu_load_image_lock);
}

int vpu_load_image(struct vpu_device *vpu_device)
{
	struct vpu_image_header *image_header = NULL;
	int i;
	int ret = 0;

	/* vpu_load_image_state: unload -> loading -> loaded */

	mutex_lock(&vpu_device->vpu_load_image_lock);

	if (vpu_device->vpu_load_image_state != VPU_LOAD_IMAGE_UNLOAD)
		goto out;

	vpu_device->vpu_load_image_state = VPU_LOAD_IMAGE_LOADING;


	LOG_INF("[%s][%d] vpu load image start...\n", __func__, __LINE__);

	image_header = kzalloc(
			sizeof(struct vpu_image_header) * VPU_NUMS_IMAGE_HEADER,
			GFP_KERNEL);
	ret = (image_header) ? 0 : -ENOMEM;
	if (ret) {
		LOG_ERR("[%s][%d] alloc image header failed!\n",
			__func__, __LINE__);
		goto exit;
	}

	vpu_device->image_header = image_header;

	vpu_device->vpu_image_ctx = kzalloc(
			sizeof(struct vpu_image_ctx) * VPU_NUMS_IMAGE_HEADER,
			GFP_KERNEL);
	ret = (vpu_device->vpu_image_ctx) ? 0 : -ENOMEM;
	if (ret) {
		LOG_ERR("[%s][%d] alloc vpu image ctx failed!\n",
			__func__, __LINE__);
		goto exit;
	}

	for (i = 0; i < VPU_NUMS_IMAGE_HEADER; i++) {
		vpu_device->vpu_image_ctx[i].vpu_device = vpu_device;
		vpu_device->vpu_image_ctx[i].vpu_image_index = i;

		ret = request_firmware_nowait(THIS_MODULE, 1,
					g_vpu_image_name[i],
					vpu_device->dev, GFP_KERNEL,
					&vpu_device->vpu_image_ctx[i],
					vpu_request_firmware_cbk);
		if (ret)
			LOG_ERR("[%s][%d] request_firmware_nowait %d failed!\n",
				__func__, __LINE__, i);
	}

exit:
	if (ret) {
		if (image_header != NULL) {
			kfree(image_header);
			vpu_device->image_header = NULL;
		}

		vpu_device->vpu_load_image_state = VPU_LOAD_IMAGE_UNLOAD;
	}

out:
	mutex_unlock(&vpu_device->vpu_load_image_lock);
	return ret;
}

static void _vpu_unload_image(struct vpu_device *vpu_device)
{
	/* unload algo */
	vpu_unload_algo(vpu_device);

	/* unload firmware */
	vpu_unload_firmware(vpu_device);

	kfree(vpu_device->image_header);

	vpu_device->image_header = NULL;
}

int vpu_unload_image(struct vpu_device *vpu_device)
{
	mutex_lock(&vpu_device->vpu_load_image_lock);

	if (vpu_device->vpu_load_image_state != VPU_LOAD_IMAGE_LOADED)
		goto exit;

	_vpu_unload_image(vpu_device);
	vpu_device->vpu_load_image_state = VPU_LOAD_IMAGE_UNLOAD;

exit:
	mutex_unlock(&vpu_device->vpu_load_image_lock);

	LOG_INF("[%s][%d] unload image\n", __func__, __LINE__);
	return 0;
}

int vpu_check_load_image_state(struct vpu_device *vpu_device)
{
	int ret = 1;

	/* ret =  0: loaded or no need to load */
	/* ret =  1: unload or loading         */
	/* ret = -1: load image failed         */

	mutex_lock(&vpu_device->vpu_load_image_lock);

	if ((vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_NONE)
		|| (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_LOADED))
		ret = 0;
	else if (vpu_device->vpu_load_image_state == VPU_LOAD_IMAGE_FAILED)
		ret = -1;

	mutex_unlock(&vpu_device->vpu_load_image_lock);
	return ret;
}

int vpu_wait_load_image(struct vpu_device *vpu_device)
{
	int count = 0, ret = 0;

	while ((ret = vpu_check_load_image_state(vpu_device)) != 0) {
		if (ret == -1) {
			break;
		} else if (count++ == VPU_LOAD_IMAGE_USLEEP_TIMEOUT) {
			LOG_INF("[%s][%d] wait vpu load image timeout!\n",
				__func__, __LINE__);
			ret = -1;
			break;
		}

		usleep_range(VPU_LOAD_IMAGE_USLEEP_MIN,
				VPU_LOAD_IMAGE_USLEEP_MAX);
	}

	return ret;
}

