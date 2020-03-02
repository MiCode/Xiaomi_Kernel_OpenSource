/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#ifdef CONFIG_MTK_ION
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>
#include <m4u.h>
#include <m4u_port.h>
#define VPU_PORT_OF_IOMMU M4U_PORT_VPU

/* vpu_mem_alloc */
#define IOMMU_VA_START      (0x7DA00000)
#define IOMMU_VA_END        (0x82600000)
#endif

#include "vpu_mem.h"
#include "vpu_drv.h"
#include "vpu_debug.h"

#ifdef CONFIG_MTK_ION
int vpu_mem_flush(struct vpu_mem *s)
{
	struct ion_sys_data sys_data;
	int ret;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle =
		(struct ion_handle *)s->handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	sys_data.cache_sync_param.va = (void *)s->va;
	sys_data.cache_sync_param.size = s->length;

	ret = ion_kernel_ioctl(vpu_drv->ion, ION_CMD_SYSTEM,
		(unsigned long)&sys_data);

	vpu_mem_debug("%s: ion_kernel_ioctl kernel_handle=%llx\n",
		__func__, (unsigned long long)s->handle);

	if (ret) {
		vpu_mem_debug("%s: ion_kernel_ioctl(hndl=%llx): %d failed\n",
			__func__, (unsigned long long)s->handle, ret);
	}

	return 0;
}

int vpu_mem_alloc(struct vpu_mem **shmem,
	struct vpu_mem_param *param)
{
	int ret = 0;
	struct ion_mm_data mm_data;
	struct ion_sys_data sys_data;
	struct ion_handle *handle = NULL;

	*shmem = kzalloc(sizeof(struct vpu_mem), GFP_KERNEL);
	ret = (*shmem == NULL);
	if (ret) {
		pr_info("%s: fail to kzalloc 'struct memory'!\n",
			__func__);
		goto out;
	}

	handle = ion_alloc(vpu_drv->ion, param->size, 0,
					ION_HEAP_MULTIMEDIA_MASK, 0);
	ret = (handle == NULL) ? -ENOMEM : 0;
	if (ret) {
		pr_info("%s: fail to alloc ion buffer, ret=%d\n",
			__func__, ret);
		goto out;
	}

	(*shmem)->handle = (void *) handle;

	vpu_mem_debug("%s: allocated handle: %p, size: %d\n",
		__func__, (*shmem)->handle, param->size);

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER_EXT;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = VPU_PORT_OF_IOMMU;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 1;
	if (param->fixed_addr) {
		mm_data.config_buffer_param.reserve_iova_start =
							param->fixed_addr;

		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;
	} else {
		/* need revise starting address for working buffer*/
		mm_data.config_buffer_param.reserve_iova_start = 0x60000000;
		mm_data.config_buffer_param.reserve_iova_end = IOMMU_VA_END;
	}
	ret = ion_kernel_ioctl(vpu_drv->ion, ION_CMD_MULTIMEDIA,
					(unsigned long)&mm_data);
	if (ret) {
		pr_info("%s: fail to config ion buffer, ret=%d\n",
			__func__, ret);
		goto out;
	}

	/* map pa */
	vpu_mem_debug("%s: vpu param->require_pa(%d)\n",
		__func__, param->require_pa);
	if (param->require_pa) {
		sys_data.sys_cmd = ION_SYS_GET_PHYS;
		sys_data.get_phys_param.kernel_handle = handle;
		sys_data.get_phys_param.phy_addr =
			(unsigned long)(VPU_PORT_OF_IOMMU) << 24 |
			ION_FLAG_GET_FIXED_PHYS;
		sys_data.get_phys_param.len = ION_FLAG_GET_FIXED_PHYS;
		ret = ion_kernel_ioctl(vpu_drv->ion, ION_CMD_SYSTEM,
						(unsigned long)&sys_data);
		if (ret) {
			pr_info("%s: fail to get ion phys, ret=%d\n",
				__func__, ret);
			goto out;
		}

		(*shmem)->pa = sys_data.get_phys_param.phy_addr;
		(*shmem)->length = sys_data.get_phys_param.len;
	}

	/* map va */
	if (param->require_va) {
		(*shmem)->va =
			(uint64_t)ion_map_kernel(vpu_drv->ion, handle);
		ret = ((*shmem)->va) ? 0 : -ENOMEM;
		if (ret) {
			pr_info("%s: fail to map va of buffer!\n", __func__);
			goto out;
		}
	}

	return 0;

out:
	if (handle)
		ion_free(vpu_drv->ion, handle);

	if (*shmem) {
		kfree(*shmem);
		*shmem = NULL;
	}

	return ret;
}

void vpu_mem_free(struct vpu_mem **shmem)
{
	struct ion_handle *handle;

	if (!vpu_drv || !vpu_drv->ion)
		return;

	if (!shmem || !*shmem)
		return;

	handle = (struct ion_handle *) (*shmem)->handle;

	vpu_mem_debug("%s: shmem: %p, handle: %p\n",
		__func__, *shmem, handle);

	if (handle) {
		ion_unmap_kernel(vpu_drv->ion, handle);
		ion_free(vpu_drv->ion, handle);
	}

	kfree(*shmem);
	*shmem = NULL;
}
#else
// TODO: implement memory allocators for simulator
int vpu_mem_flush(struct vpu_mem *s)
{
}

int vpu_mem_alloc(struct vpu_mem **shmem,
	struct vpu_mem_param *param)
{
	return 0;
}

void vpu_mem_free(struct vpu_mem **shmem)
{
}
#endif

#ifdef CONFIG_MTK_M4U
int vpu_mva_alloc(unsigned long va, struct sg_table *sg,
	unsigned int size, unsigned int flags,
	unsigned int *pMva)
{
	return m4u_alloc_mva(vpu_drv->m4u, VPU_PORT_OF_IOMMU,
		va, sg, size, M4U_PROT_READ | M4U_PROT_WRITE, flags, pMva);
}

int vpu_mva_free(const unsigned int mva)
{
	return m4u_dealloc_mva(vpu_drv->m4u, VPU_PORT_OF_IOMMU, mva);
}

enum m4u_callback_ret_t vpu_m4u_fault_callback(int port,
	unsigned int mva, void *data)
{
	vpu_mem_debug("%s: port=%d, mva=0x%x", __func__, port, mva);
	// TODO: add check
	return M4U_CALLBACK_HANDLED;
}
#else
// TODO: implement mva mappers for simulator
int vpu_mva_alloc(unsigned long va, struct sg_table *sg,
	unsigned int size, unsigned int flags,
	unsigned int *pMva)
{
	return 0;
}

int vpu_mva_free(const unsigned int mva)
{
	return 0;
}
#endif

/* called by vpu_init() */
int vpu_init_mem(void)
{
#ifdef CONFIG_MTK_M4U
	vpu_drv_debug("%s: m4u_register_fault_callback\n", __func__);  // debug
	m4u_register_fault_callback(VPU_PORT_OF_IOMMU,
		vpu_m4u_fault_callback, NULL);

	vpu_drv_debug("%s: m4u_create_client\n", __func__);	// debug
	if (!vpu_drv->m4u)
		vpu_drv->m4u = m4u_create_client();
#endif

#ifdef CONFIG_MTK_ION
	vpu_drv_debug("%s: ion_client_create\n", __func__); // debug
	if (!vpu_drv->ion)
		vpu_drv->ion = ion_client_create(g_ion_device, "vpu");
#endif

	return 0;
}

/* called by vpu_exit() */
void vpu_exit_mem(void)
{
	if (!vpu_drv)
		return;

#ifdef CONFIG_MTK_M4U
	vpu_drv_debug("%s: m4u_unregister_fault_callback\n", __func__);
	m4u_unregister_fault_callback(VPU_PORT_OF_IOMMU);

	vpu_drv_debug("%s: m4u_destroy_client\n", __func__);
	if (vpu_drv->m4u) {
		m4u_destroy_client(vpu_drv->m4u);
		vpu_drv->m4u = NULL;
	}
#endif

#ifdef CONFIG_MTK_ION
	vpu_drv_debug("%s: ion_client_destroy\n", __func__);
	if (vpu_drv->ion) {
		ion_client_destroy(vpu_drv->ion);
		vpu_drv->ion = NULL;
	}
#endif
}

