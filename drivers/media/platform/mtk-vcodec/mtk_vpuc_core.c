/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Andrew-CT Chen <andrew-ct.chen@mediatek.com>
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

#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <asm/dma-iommu.h>

#include "mtk_vpu_core.h"

#define MTK_VPU_DRV_NAME	"mtk_vpu"
#define SHARE_BUF_SIZE		48
#define VPU_PATH		"/dev/vpud"
#define VPU_DEVNAME		"vpu"

/* vpu bank0 extended mapping length */
#define VPU_PMEM0_LEN(vpu)	(vpu->mem.bank0.p_len)
#define VPU_DMEM0_LEN(vpu)	(vpu->mem.bank0.d_len)
/* vpu bank0 extended user virtural address */
#define VPU_PMEM0_VMA(vpu)	(vpu->mem.bank0.p_vma)
#define VPU_DMEM0_VMA(vpu)	(vpu->mem.bank0.d_vma)
/* vpu bank0 extended kernel virtural address */
#define VPU_PMEM0_VIRT(vpu)	(vpu->mem.bank0.p_va)
#define VPU_DMEM0_VIRT(vpu)	(vpu->mem.bank0.d_va)
/* vpu bank0 extended phsyial address */
#define VPU_PMEM0_PHY(vpu)	(vpu->mem.bank0.p_pa)
#define VPU_DMEM0_PHY(vpu)	(vpu->mem.bank0.d_pa)
/* vpu bank0 extended iova address*/
#define VPU_PMEM0_IOVA(vpu)	(vpu->mem.bank0.p_iova)
#define VPU_DMEM0_IOVA(vpu)	(vpu->mem.bank0.d_iova)
/* vpu bank0 extended dma attributes*/
#define VPU_PMEM0_ATTRS(vpu)	(vpu->mem.bank0.p_attrs)
#define VPU_DMEM0_ATTRS(vpu)	(vpu->mem.bank0.d_attrs)

#define MAP_SHMEM_ALLOC_BASE	0x80000000
#define MAP_SHMEM_ALLOC_RANGE	0x08000000
#define MAP_SHMEM_ALLOC_END	(MAP_SHMEM_ALLOC_BASE + MAP_SHMEM_ALLOC_RANGE)
#define MAP_SHMEM_COMMIT_BASE	0x88000000
#define MAP_SHMEM_COMMIT_RANGE	0x08000000
#define MAP_SHMEM_COMMIT_END	(MAP_SHMEM_COMMIT_BASE + MAP_SHMEM_COMMIT_RANGE)

#define MAP_SHMEM_MM_BASE	0x90000000
#define MAP_SHMEM_MM_RANGE	0x40000000
#define MAP_SHMEM_MM_END	(MAP_SHMEM_MM_BASE + MAP_SHMEM_MM_RANGE)

enum vpu_map_hw_reg_id {
	VDEC,
	VENC,
	VENC_LT,
	VPU_MAP_HW_REG_NUM
};

static const unsigned long vpu_map_hw_type[VPU_MAP_HW_REG_NUM] = {
	0x70000000,	/* VDEC */
	0x71000000,	/* VENC */
	0x72000000	/* VENC_LT */
};

static dev_t vpu_devno;
static struct cdev *vpu_cdev;
static struct class *vpu_class;
static struct device *vpu_device;
static struct mtk_vpu_dev *vpu_mtkdev;

/**
 * struct bank - VPU extended memory information
 *
 * @p_vma:	the user virtual memory address of
 *		VPU extended program memory
 * @d_vma:	the user  virtual memory address of VPU extended data memory
 * @p_va:	the kernel virtual memory address of
 *		VPU extended program memory
 * @d_va:	the kernel virtual memory address of VPU extended data memory
 * @p_pa:	the physical memory address of VPU extended program memory
 * @d_pa:	the physical memory address of VPU extended data memory
 * @p_iova:	the iova memory address of VPU extended program memory
 * @d_iova:	the iova memory address of VPU extended data memory
 */
struct bank {
	unsigned long p_vma;
	unsigned long d_vma;
	void *p_va;
	void *d_va;
	dma_addr_t p_pa;
	dma_addr_t d_pa;
	dma_addr_t p_iova;
	dma_addr_t d_iova;
	unsigned long p_len;
	unsigned long d_len;
	struct dma_attrs p_attrs;
	struct dma_attrs d_attrs;
};

/**
 * struct vpu_mem - VPU extended bank memory
 *
 * @bank0:	extended bank0 memory
 * @bank1:	extended bank1 memory
 */
struct vpu_mem {
	struct bank bank0;
	struct bank bank1;
};

/**
 * struct vpu_ipi_desc - VPU IPI descriptor
 *
 * @handler:	IPI handler
 * @name:	the name of IPI handler
 * @priv:	the private data of IPI handler
 */
struct vpu_ipi_desc {
	ipi_handler_t handler;
	const char *name;
	void *priv;
};

/**
 * struct share_obj - The DTCM(Data Tightly-Coupled Memory) buffer shared with
				AP and VPU
 *
 * @id:		IPI id
 * @len:	share buffer length
 * @share_buf:	share buffer data
 */
struct share_obj {
	int32_t id;
	uint32_t len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE];
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

/**
 * struct mtk_vpu_dev - vpu driver data
 *
 * @ipi_desc:		VPU IPI descriptor
 * @plat_dev:		VPU platform device
 * @vpu_mutex:		VPU ipi mutex
 * @file:			VPU daemon file pointer
 *
 */
struct mtk_vpu_dev {
	struct vpu_mem mem;
	struct vpu_ipi_desc ipi_desc[IPI_MAX];
	struct platform_device	*plat_dev;
	struct mutex vpu_mutex;
	struct file *file;
	struct task_struct *handle_task;
	struct dma_iommu_mapping *dma_mapping;
	struct map_hw_reg map_base[VPU_MAP_HW_REG_NUM];
	bool   is_open;
	bool   is_alloc;
};

#define VPUD_SET_OBJECT		_IOW('v', 0, struct share_obj)

int vpu_wdt_reg_handler(struct platform_device *pdev,
			void WDT_RESET(void *),
			void *private_data, char *module_name)
{
	return 0;
}

int vpu_wdt_unreg_handler(struct platform_device *pdev, char *module_name)
{
	return 0;
}

void vpu_disable_clock(struct platform_device *pdev)
{
}

int vpu_enable_clock(struct platform_device *pdev)
{
	return 0;
}

int vpu_ipi_registration(struct platform_device *pdev,
			 enum ipi_id id, ipi_handler_t handler,
			 const char *name, void *priv)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);
	struct vpu_ipi_desc *ipi_desc = vpu_dev->ipi_desc;

	pr_debug("[VPU] %s\n", __func__);

	if (id < IPI_MAX && handler != NULL) {
		ipi_desc[id].name = name;
		ipi_desc[id].handler = handler;
		ipi_desc[id].priv = priv;
		return 0;
	} else {
		return -EINVAL;
	}
}

static unsigned int checksum(unsigned char *data, unsigned int size)
{
	unsigned int i = 0, sum = 0;

	for (i = 0; i < size; i++)
		sum += data[i];
	return sum;
}

static inline void show_obj(const char *str, struct share_obj *obj)
{
	pr_debug("[VPU][%s] obj id:%d len:%u checksum:%u\n", str,
							     obj->id,
							     obj->len,
							     checksum(obj->share_buf, obj->len));
}

int vpu_ipi_send(struct platform_device *pdev,
		 enum ipi_id id, void *buf,
		 unsigned int len, unsigned int wait)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);
	struct share_obj send_obj;
	mm_segment_t old_fs = get_fs();
	int ret;

	if (id >= IPI_MAX || len > sizeof(send_obj.share_buf) || buf == NULL) {
		pr_err("[VPU] failed to send ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	memcpy((void *)send_obj.share_buf, buf, len);
	send_obj.len = len;
	send_obj.id = id;

	mutex_lock(&vpu_dev->vpu_mutex);
	if (!vpu_dev->is_open) {
		vpu_dev->file = filp_open(VPU_PATH, O_RDONLY, 0);
		if (IS_ERR(vpu_dev->file)) {
			pr_err("[VPU] Open vpud fail (ret=%ld)\n", PTR_ERR(vpu_dev->file));
			mutex_unlock(&vpu_dev->vpu_mutex);
			return -EINVAL;
		}
		vpu_dev->is_open = true;
	}

	set_fs(KERNEL_DS);
	ret = vpu_dev->file->f_op->unlocked_ioctl(vpu_dev->file, VPUD_SET_OBJECT, (unsigned long)&send_obj);
	set_fs(old_fs);
	mutex_unlock(&vpu_dev->vpu_mutex);

	if (ret)
		pr_err("[VPU] failed to send ipi message (ret=%d)\n", ret);

	return ret;
}

int get_vpu_semaphore(struct platform_device *pdev, int flag)
{
	return 0;
}

int release_vpu_semaphore(struct platform_device *pdev, int flag)
{
	return 0;
}

void *vpu_mapping_dm_addr(struct platform_device *pdev,
				void *dtcm_dmem_addr)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dtcm_dmem_addr);
	uintptr_t d_va_start = (uintptr_t)VPU_DMEM0_VIRT(vpu_dev);
	uintptr_t d_off = d_vma - VPU_DMEM0_VMA(vpu_dev);
	uintptr_t d_va;

	if (dtcm_dmem_addr == NULL || d_off > VPU_DMEM0_LEN(vpu_dev)) {
		pr_err("[VPU] %s: Invalid vma 0x%p\n", __func__, dtcm_dmem_addr);
		return NULL;
	}

	d_va = d_va_start + d_off;
	pr_debug("[VPU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_va);

	return (void *)d_va;
}

void *vpu_mapping_ext_mem_addr(struct platform_device *pdev,
				void *virt_ext_mem_addr)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);
	uintptr_t d_va = (uintptr_t)virt_ext_mem_addr;
	uintptr_t d_va_start = (uintptr_t)VPU_DMEM0_VIRT(vpu_dev);
	uintptr_t d_off = d_va - d_va_start;
	uintptr_t d_vma;

	/* Assumption: only use data section */
	if (virt_ext_mem_addr == NULL || d_off > VPU_DMEM0_LEN(vpu_dev)) {
		pr_err("[VPU] %s: Invalid va 0x%p\n", __func__, virt_ext_mem_addr);
		return NULL;
	}

	d_vma = VPU_DMEM0_VMA(vpu_dev) + d_off;
	pr_debug("[VPU] %s: 0x%lx -> 0x%lx\n", __func__, d_va, d_vma);

	return (void *)d_vma;
}

dma_addr_t *vpu_mapping_iommu_dm_addr(struct platform_device *pdev,
				void *dmem_addr)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dmem_addr);
	uintptr_t d_off = d_vma - VPU_DMEM0_VMA(vpu_dev);
	uintptr_t d_iova;

	if (dmem_addr == NULL || d_off > VPU_DMEM0_LEN(vpu_dev)) {
		pr_err("[VPU] %s: Invalid vma 0x%p\n", __func__,  dmem_addr);
		return NULL;
	}

	d_iova = VPU_DMEM0_IOVA(vpu_dev) + d_off;
	pr_debug("[VPU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_iova);

	return (void *)d_iova;
}

struct platform_device *vpu_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *ofnode;
	struct device_node *vpu_node;
	struct platform_device *vpu_pdev;
	struct mtk_vpu_dev *vpu_dev;

	pr_debug("[VPU] %s\n", __func__);

	ofnode = dev->of_node;
	vpu_node = of_parse_phandle(ofnode, "vpu", 0);
	if (!vpu_node) {
		pr_err("[VPU] can't get vpu node\n");
		return NULL;
	}

	vpu_pdev = of_find_device_by_node(vpu_node);
	if (WARN_ON(!vpu_pdev)) {
		pr_err("[VPU] vpu pdev failed\n");
		of_node_put(vpu_node);
		return NULL;
	}

	vpu_dev = platform_get_drvdata(vpu_pdev);
	if (!vpu_dev) {
		pr_err("[VPU] VPU platform device is invalid\n");
		return NULL;
	}

	return vpu_pdev;
}

int vpu_load_firmware(struct platform_device *pdev, int force_load)
{
	if (!pdev) {
		pr_err("[VPU] VPU platform device is invalid\n");
		return -EINVAL;
	}
	return 0;
}

static int vpu_ipi_handler(struct mtk_vpu_dev *vpu_dev, struct share_obj *rcv_obj)
{
	struct vpu_ipi_desc *ipi_desc = vpu_dev->ipi_desc;
	int ret = -1;

	if (ipi_desc[rcv_obj->id].handler) {
		ipi_desc[rcv_obj->id].handler(rcv_obj->share_buf,
					      rcv_obj->len,
					      ipi_desc[rcv_obj->id].priv);
		ret = 0;
	} else {
		pr_err("[VPU] No such ipi id = %d\n", rcv_obj->id);
	}

	return ret;
}

static int vpu_ipi_init(struct mtk_vpu_dev *vpu_dev)
{
	vpu_dev->is_open = false;
	vpu_dev->is_alloc = false;
	mutex_init(&vpu_dev->vpu_mutex);

	return 0;
}

static int mtk_vpu_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mtk_vpu_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void vpu_free_d_ext_mem(struct mtk_vpu_dev *vpu_dev)
{
	mutex_lock(&vpu_dev->vpu_mutex);
	if (vpu_dev->is_open) {
		filp_close(vpu_dev->file, NULL);
		vpu_dev->is_open = false;
	}
	if (vpu_dev->is_alloc) {
		dma_free_attrs(&vpu_dev->plat_dev->dev, VPU_DMEM0_LEN(vpu_dev), VPU_DMEM0_VIRT(vpu_dev),
			       VPU_DMEM0_IOVA(vpu_dev), VPU_DMEM0_ATTRS(&vpu_dev));
		VPU_DMEM0_VIRT(vpu_dev) = NULL;
		vpu_dev->is_alloc = false;
	}
	mutex_unlock(&vpu_dev->vpu_mutex);
}

static int vpu_alloc_d_ext_mem(struct mtk_vpu_dev *vpu_dev, unsigned long len)
{
	struct device *dev = &vpu_dev->plat_dev->dev;

	mutex_lock(&vpu_dev->vpu_mutex);
	if (!vpu_dev->is_alloc) {
		init_dma_attrs(VPU_DMEM0_ATTRS(&vpu_dev));
		dma_set_attr(DMA_ATTR_FORCE_CONTIGUOUS, VPU_DMEM0_ATTRS(&vpu_dev));

		VPU_DMEM0_VIRT(vpu_dev) = dma_alloc_attrs(dev,
							  len,
							  VPU_DMEM0_IOVA(&vpu_dev),
							  GFP_KERNEL,
							  VPU_DMEM0_ATTRS(&vpu_dev));
		if (VPU_DMEM0_VIRT(vpu_dev) == NULL) {
			pr_err("[VPU] Failed to allocate memory for extended data memory\n");
			mutex_unlock(&vpu_dev->vpu_mutex);
			return -1;
		}

		VPU_DMEM0_PHY(vpu_dev) = iommu_iova_to_phys(vpu_dev->dma_mapping->domain,
							    vpu_dev->mem.bank0.d_iova);
		VPU_DMEM0_LEN(vpu_dev) = len;
		vpu_dev->is_alloc = true;
	}
	mutex_unlock(&vpu_dev->vpu_mutex);

	pr_info("[VPU] Data extend memory (len:%lu) phy=0x%llx virt=0x%p iova=0x%llx\n",
		VPU_DMEM0_LEN(vpu_dev),
		(unsigned long long)VPU_DMEM0_PHY(vpu_dev),
		VPU_DMEM0_VIRT(vpu_dev),
		(unsigned long long)VPU_DMEM0_IOVA(vpu_dev));
	return 0;
}

static int mtk_vpu_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa_end = pa_start + length;
	int i;

	pr_debug("[VPU] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);

	for (i = 0; i < VPU_MAP_HW_REG_NUM; i++) {
		if (pa_start == vpu_map_hw_type[i] && length <= vpu_mtkdev->map_base[i].len) {
			vma->vm_pgoff = vpu_mtkdev->map_base[i].base >> PAGE_SHIFT;
			goto valid_map;
		}
	}

	if (pa_start >= MAP_SHMEM_ALLOC_BASE && pa_end <= MAP_SHMEM_ALLOC_END) {
		vpu_free_d_ext_mem(vpu_mtkdev);
		if (vpu_alloc_d_ext_mem(vpu_mtkdev, length)) {
			pr_err("[VPU] allocate DM failed\n");
			return -ENOMEM;
		}
		vma->vm_pgoff = (unsigned long)(VPU_DMEM0_PHY(vpu_mtkdev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_COMMIT_BASE && pa_end <= MAP_SHMEM_COMMIT_END) {
		VPU_DMEM0_VMA(vpu_mtkdev) = vma->vm_start;
		vma->vm_pgoff = (unsigned long)(VPU_DMEM0_PHY(vpu_mtkdev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_MM_BASE && pa_end <= MAP_SHMEM_MM_END) {
		vma->vm_pgoff = iommu_iova_to_phys(vpu_mtkdev->dma_mapping->domain,
						    pa_start - MAP_SHMEM_MM_BASE);
		vma->vm_pgoff >>= PAGE_SHIFT;
		goto valid_map;
	}

	pr_err("[VPU] Invalid argument\n");
	return -EINVAL;

valid_map:
	pr_debug("[VPU] Mapping pgoff 0x%lx\n", vma->vm_pgoff);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}

static long mtk_vpu_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;

	switch (cmd) {
	case VPUD_SET_OBJECT:
		ret = vpu_ipi_handler(vpu_mtkdev, (struct share_obj *)arg);
		break;
	default:
		pr_err("[VPU] Unknown cmd\n");
		break;
	}

	return ret;
}

static const struct file_operations vpu_fops = {
	.owner      = THIS_MODULE,
	.unlocked_ioctl = mtk_vpu_unlocked_ioctl,
	.open       = mtk_vpu_open,
	.release    = mtk_vpu_release,
	.mmap       = mtk_vpu_mmap,

};

static int vpu_iommu_init(struct device *dev)
{
	struct device_node *np;
	struct platform_device *pdev;
	int err;

	np = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!np) {
		pr_debug("[VPU] can't find iommus node\n");
		return -ENOENT;
	}

	pdev = of_find_device_by_node(np);
	if (WARN_ON(!pdev)) {
		of_node_put(np);
		pr_err("[VPU] can't find iommu device by node\n");
		return -EINVAL;
	}

	pr_debug("[VPU] %s()-> %s\n", __func__, dev_name(&pdev->dev));
	vpu_mtkdev->dma_mapping = pdev->dev.archdata.iommu;

	err = arm_iommu_attach_device(dev, vpu_mtkdev->dma_mapping);

	if (err) {
		pr_err("[VPU] arm_iommu_attach_device fail %d\n", err);
		return err;
	}

	return 0;
}

static int mtk_vpu_probe(struct platform_device *pdev)
{
	struct mtk_vpu_dev *vpu_dev;
	struct device *dev;
	struct resource *res;
	int i, ret = 0;

	pr_debug("[VPU] initialization\n");

	dev = &pdev->dev;
	vpu_dev = devm_kzalloc(dev, sizeof(*vpu_dev), GFP_KERNEL);
	if (!vpu_dev) {
		pr_err("[VPU] failed to allocate vpu driver data\n");
		return -ENOMEM;
	}
	vpu_dev->plat_dev = pdev;
	platform_set_drvdata(pdev, vpu_dev);
	vpu_mtkdev = vpu_dev;

	for (i = 0; i < VPU_MAP_HW_REG_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			pr_err("Get memory resource failed.\n");
			ret = -ENXIO;
			goto err_ipi_init;
		}
		vpu_dev->map_base[i].base = res->start;
		vpu_dev->map_base[i].len = resource_size(res);
		pr_debug("[VPU] base[%d]: 0x%x 0x%x", i, res->start, resource_size(res));
	}

	pr_debug("[VPU] vpu ipi init\n");
	ret = vpu_ipi_init(vpu_dev);
	if (ret != 0) {
		pr_err("[VPU] Failed to init ipi\n");
		goto err_ipi_init;
	}

	ret = vpu_iommu_init(dev);
	if (ret != 0) {
		pr_err("[VPU] failed to init vpu iommu\n");
		goto err_ipi_init;
	}

	/* init character device */

	ret = alloc_chrdev_region(&vpu_devno, 0, 1, VPU_DEVNAME);
	if (ret < 0) {
		pr_err("[VPU]  alloc_chrdev_region failed (ret=%d)\n", ret);
		goto err_alloc;
	}

	vpu_cdev = cdev_alloc();
	vpu_cdev->owner = THIS_MODULE;
	vpu_cdev->ops = &vpu_fops;

	ret = cdev_add(vpu_cdev, vpu_devno, 1);
	if (ret < 0) {
		pr_err("[VPU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vpu_class = class_create(THIS_MODULE, VPU_DEVNAME);
	if (IS_ERR(vpu_class)) {
		ret = PTR_ERR(vpu_class);
		pr_err("[VPU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vpu_device = device_create(vpu_class, NULL, vpu_devno, NULL, VPU_DEVNAME);
	if (IS_ERR(vpu_device)) {
		ret = PTR_ERR(vpu_device);
		pr_err("[VPU] class create fail (ret=%d)", ret);
		goto err_device;
	}

	pr_debug("[VPU] initialization completed\n");
	return 0;

err_device:
	class_destroy(vpu_class);
err_add:
	cdev_del(vpu_cdev);
	unregister_chrdev_region(vpu_devno, 1);
err_alloc:
	arm_iommu_detach_device(dev);
err_ipi_init:
	devm_kfree(dev, vpu_dev);

	return ret;
}

static const struct of_device_id mtk_vpu_match[] = {
	{
		.compatible = "mediatek,mt2701-vpu",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vpu_match);

static int mtk_vpu_remove(struct platform_device *pdev)
{
	struct mtk_vpu_dev *vpu_dev = platform_get_drvdata(pdev);

	devm_kfree(&pdev->dev, vpu_dev);

	device_destroy(vpu_class, vpu_devno);
	class_destroy(vpu_class);
	cdev_del(vpu_cdev);
	unregister_chrdev_region(vpu_devno, 1);

	return 0;
}

static int mtk_vpu_suspend(struct device *dev)
{
	return 0;
}

static int mtk_vpu_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops mtk_vpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_vpu_suspend,
				mtk_vpu_resume)
};

static struct platform_driver mtk_vpu_driver = {
	.probe	= mtk_vpu_probe,
	.remove	= mtk_vpu_remove,
	.driver	= {
		.name	= MTK_VPU_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mtk_vpu_match,
		.pm = &mtk_vpu_pm_ops,
	},
};

module_platform_driver(mtk_vpu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek Video Prosessor Unit driver");
