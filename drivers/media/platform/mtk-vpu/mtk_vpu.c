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

#include "mtk_vpu.h"

/**
 * VPU (video processor unit) is a tiny processor controlling video hardware
 * related to video codec, scaling and color format converting.
 * VPU interfaces with other blocks by share memory and interrupt.
 **/
#define MTK_VPU_DRV_NAME	"mtk_vpu"

#define VPU_PATH		"/dev/vpud"
#define VPU_DEVNAME		"vpu"

#define IPI_TIMEOUT_MS		2000U
#define VPU_FW_VER_LEN		16

/* vpu extended mapping length */
#define VPU_PMEM0_LEN(vpu)	(vpu->mem.p_len)
#define VPU_DMEM0_LEN(vpu)	(vpu->mem.d_len)
/* vpu extended user virtural address */
#define VPU_PMEM0_VMA(vpu)	(vpu->mem.p_vma)
#define VPU_DMEM0_VMA(vpu)	(vpu->mem.d_vma)
/* vpu extended kernel virtural address */
#define VPU_PMEM0_VIRT(vpu)	(vpu->mem.p_va)
#define VPU_DMEM0_VIRT(vpu)	(vpu->mem.d_va)
/* vpu extended phsyial address */
#define VPU_PMEM0_PHY(vpu)	(vpu->mem.p_pa)
#define VPU_DMEM0_PHY(vpu)	(vpu->mem.d_pa)
/* vpu extended iova address*/
#define VPU_PMEM0_IOVA(vpu)	(vpu->mem.p_iova)
#define VPU_DMEM0_IOVA(vpu)	(vpu->mem.d_iova)
/* vpu extended dma attributes*/
#define VPU_PMEM0_ATTRS(vpu)	(vpu->mem.p_attrs)
#define VPU_DMEM0_ATTRS(vpu)	(vpu->mem.d_attrs)

#define SHARE_BUF_SIZE		48

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
static struct mtk_vpu *vpu_mtkdev;

/**
 * struct vpu_mem - VPU memory information
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
struct vpu_mem {
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
 * struct vpu_run - VPU initialization status
 *
 * @signaled:	the signal of vpu initialization completed
 * @fw_ver:	VPU firmware version
 * @wq:		wait queue for VPU initialization status
 */
struct vpu_run {
	u32 signaled;
	char fw_ver[VPU_FW_VER_LEN];
	unsigned int	dec_capability;
	unsigned int	enc_capability;
	wait_queue_head_t wq;
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
 *		      AP and VPU
 *
 * @id:		IPI id
 * @len:	share buffer length
 * @share_buf:	share buffer data
 */
struct share_obj {
	int32_t id;
	uint32_t len;
	unsigned char share_buf[SHARE_BUF_SIZE];
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

/**
 * struct mtk_vpu - vpu driver data
 *
 * @ipi_desc:		VPU IPI descriptor
 * @plat_dev:		VPU platform device
 * @vpu_mutex:		VPU ipi mutex
 * @file:			VPU daemon file pointer
 *
 */
struct mtk_vpu {
	struct vpu_mem mem;
	struct vpu_run run;
	struct vpu_ipi_desc ipi_desc[IPI_MAX];
	struct platform_device	*plat_dev;
	struct mutex vpu_mutex;
	struct file *file;
	struct task_struct *handle_task;
	struct dma_iommu_mapping *dma_mapping;
	struct map_hw_reg map_base[VPU_MAP_HW_REG_NUM];
	bool   is_open;
	bool   is_alloc;
	wait_queue_head_t ack_wq;
	bool   ipi_id_ack[IPI_MAX];
};

#define VPUD_SET_OBJECT		_IOW('v', 0, struct share_obj)

static inline bool vpu_running(struct mtk_vpu *vpu)
{
	return vpu->run.signaled;
}

int vpu_ipi_register(struct platform_device *pdev,
			 enum ipi_id id, ipi_handler_t handler,
			 const char *name, void *priv)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);
	struct vpu_ipi_desc *ipi_desc = vpu->ipi_desc;

	if (!vpu) {
		pr_err("vpu device in not ready\n");
		return -EPROBE_DEFER;
	}

	if (id < IPI_MAX && handler != NULL) {
		ipi_desc[id].name = name;
		ipi_desc[id].handler = handler;
		ipi_desc[id].priv = priv;
		return 0;
	}

	pr_err("register vpu ipi with invalid arguments\n");
	return -EINVAL;
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
		 unsigned int len)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);
	struct share_obj send_obj;
	mm_segment_t old_fs = get_fs();
	unsigned long timeout;
	int ret;

	if (id <= IPI_VPU_INIT || id >= IPI_MAX ||
		len > sizeof(send_obj.share_buf) || buf == NULL) {
		pr_err("[VPU] failed to send ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	if (!vpu_running(vpu)) {
		pr_err("[VPU] vpu_ipi_send: VPU is not running\n");
		return -EPERM;
	}

	memcpy((void *)send_obj.share_buf, buf, len);
	send_obj.len = len;
	send_obj.id = id;

	mutex_lock(&vpu->vpu_mutex);
	if (!vpu->is_open) {
		vpu->file = filp_open(VPU_PATH, O_RDONLY, 0);
		if (IS_ERR(vpu->file)) {
			pr_err("[VPU] Open vpud fail (ret=%ld)\n", PTR_ERR(vpu->file));
			mutex_unlock(&vpu->vpu_mutex);
			return -EINVAL;
		}
		vpu->is_open = true;
	}

	vpu->ipi_id_ack[id] = false;

	set_fs(KERNEL_DS);
	ret = vpu->file->f_op->unlocked_ioctl(vpu->file, VPUD_SET_OBJECT, (unsigned long)&send_obj);
	set_fs(old_fs);
	mutex_unlock(&vpu->vpu_mutex);

	if (ret) {
		pr_err("[VPU] failed to send ipi message (ret=%d)\n", ret);
		goto end;
	}

	/* wait for VPU's ACK */
	timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	ret = wait_event_interruptible_timeout(vpu->ack_wq,
					       vpu->ipi_id_ack[id], timeout);
	vpu->ipi_id_ack[id] = false;
	if (0 == ret) {
		pr_err("vpu ipi %d ack time out !", id);
		ret = -EIO;
		goto end;
	} else if (-ERESTARTSYS == ret) {
		pr_err("vpu ipi %d ack wait interrupted by a signal",
			id);
		ret = -ERESTARTSYS;
		goto end;
	}

	ret = 0;
end:
	return ret;
}

unsigned int vpu_get_vdec_hw_capa(struct platform_device *pdev)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);

	return vpu->run.dec_capability;
}

unsigned int vpu_get_venc_hw_capa(struct platform_device *pdev)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);

	return vpu->run.enc_capability;
}

void *vpu_mapping_dm_addr(struct platform_device *pdev,
				u32 dtcm_dmem_addr)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dtcm_dmem_addr);
	uintptr_t d_va_start = (uintptr_t)VPU_DMEM0_VIRT(vpu);
	uintptr_t d_off = d_vma - VPU_DMEM0_VMA(vpu);
	uintptr_t d_va;

	if (!dtcm_dmem_addr || d_off > VPU_DMEM0_LEN(vpu)) {
		pr_err("[VPU] %s: Invalid vma 0x%x\n", __func__, dtcm_dmem_addr);
		return NULL;
	}

	d_va = d_va_start + d_off;
	pr_debug("[VPU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_va);

	return (void *)d_va;
}

dma_addr_t vpu_mapping_iommu_dm_addr(struct platform_device *pdev,
				u32 dmem_addr)
{
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dmem_addr);
	uintptr_t d_off = d_vma - VPU_DMEM0_VMA(vpu);
	uintptr_t d_iova;

	if (!dmem_addr || d_off > VPU_DMEM0_LEN(vpu)) {
		pr_err("[VPU] %s: Invalid vma 0x%x\n", __func__,  dmem_addr);
		return -EINVAL;
	}

	d_iova = VPU_DMEM0_IOVA(vpu) + d_off;
	pr_debug("[VPU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_iova);

	return (dma_addr_t)d_iova;
}

struct platform_device *vpu_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *vpu_node;
	struct platform_device *vpu_pdev;

	pr_debug("[VPU] %s\n", __func__);

	vpu_node = of_parse_phandle(dev->of_node, "vpu", 0);
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

	return vpu_pdev;
}

int vpu_load_firmware(struct platform_device *pdev, bool force)
{
	if (!pdev) {
		pr_err("[VPU] VPU platform device is invalid\n");
		return -EINVAL;
	}
	return 0;
}

static int vpu_ipi_handler(struct mtk_vpu *vpu, struct share_obj *rcv_obj)
{
	struct vpu_ipi_desc *ipi_desc = vpu->ipi_desc;
	int ret = -1;

	if (rcv_obj->id < IPI_MAX && ipi_desc[rcv_obj->id].handler) {
		ipi_desc[rcv_obj->id].handler(rcv_obj->share_buf,
					      rcv_obj->len,
					      ipi_desc[rcv_obj->id].priv);
		if (rcv_obj->id > IPI_VPU_INIT) {
			vpu->ipi_id_ack[rcv_obj->id] = true;
			wake_up_interruptible(&vpu->ack_wq);
		}
		ret = 0;
	} else {
		pr_err("[VPU] No such ipi id = %d\n", rcv_obj->id);
	}

	return ret;
}

static int vpu_ipi_init(struct mtk_vpu *vpu)
{
	vpu->is_open = false;
	vpu->is_alloc = false;
	mutex_init(&vpu->vpu_mutex);

	return 0;
}

static void vpu_init_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_vpu *vpu = (struct mtk_vpu *)priv;
	struct vpu_run *run = (struct vpu_run *)data;

	strncpy(vpu->run.fw_ver, run->fw_ver, VPU_FW_VER_LEN);
	vpu->run.dec_capability = run->dec_capability;
	vpu->run.enc_capability = run->enc_capability;
	vpu->run.signaled = run->signaled;

	pr_debug("[VPU] fw ver: %s\n", vpu->run.fw_ver);
	pr_debug("[VPU] dec cap: %x\n", vpu->run.dec_capability);
	pr_debug("[VPU] enc cap: %x\n", vpu->run.enc_capability);
}

static int mtk_vpu_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mtk_vpu_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void vpu_free_d_ext_mem(struct mtk_vpu *vpu)
{
	mutex_lock(&vpu->vpu_mutex);
	if (vpu->is_open) {
		filp_close(vpu->file, NULL);
		vpu->is_open = false;
	}
	if (vpu->is_alloc) {
		dma_free_attrs(&vpu->plat_dev->dev, VPU_DMEM0_LEN(vpu), VPU_DMEM0_VIRT(vpu),
			       VPU_DMEM0_IOVA(vpu), VPU_DMEM0_ATTRS(&vpu));
		VPU_DMEM0_VIRT(vpu) = NULL;
		vpu->is_alloc = false;
	}
	mutex_unlock(&vpu->vpu_mutex);
}

static int vpu_alloc_d_ext_mem(struct mtk_vpu *vpu, unsigned long len)
{
	struct device *dev = &vpu->plat_dev->dev;

	mutex_lock(&vpu->vpu_mutex);
	if (!vpu->is_alloc) {
		init_dma_attrs(VPU_DMEM0_ATTRS(&vpu));
		dma_set_attr(DMA_ATTR_FORCE_CONTIGUOUS, VPU_DMEM0_ATTRS(&vpu));

		VPU_DMEM0_VIRT(vpu) = dma_alloc_attrs(dev,
							  len,
							  VPU_DMEM0_IOVA(&vpu),
							  GFP_KERNEL,
							  VPU_DMEM0_ATTRS(&vpu));
		if (VPU_DMEM0_VIRT(vpu) == NULL) {
			pr_err("[VPU] Failed to allocate memory for extended data memory\n");
			mutex_unlock(&vpu->vpu_mutex);
			return -1;
		}

		VPU_DMEM0_PHY(vpu) = iommu_iova_to_phys(vpu->dma_mapping->domain,
							    vpu->mem.d_iova);
		VPU_DMEM0_LEN(vpu) = len;
		vpu->is_alloc = true;
	}
	mutex_unlock(&vpu->vpu_mutex);

	pr_info("[VPU] Data extend memory (len:%lu) phy=0x%llx virt=0x%p iova=0x%llx\n",
		VPU_DMEM0_LEN(vpu),
		(unsigned long long)VPU_DMEM0_PHY(vpu),
		VPU_DMEM0_VIRT(vpu),
		(unsigned long long)VPU_DMEM0_IOVA(vpu));
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
	struct mtk_vpu *vpu;
	struct device *dev;
	struct resource *res;
	int i, ret = 0;

	pr_debug("[VPU] initialization\n");

	dev = &pdev->dev;
	vpu = devm_kzalloc(dev, sizeof(*vpu), GFP_KERNEL);
	if (!vpu) {
		pr_err("[VPU] failed to allocate vpu driver data\n");
		return -ENOMEM;
	}
	vpu->plat_dev = pdev;
	platform_set_drvdata(pdev, vpu);
	vpu_mtkdev = vpu;

	for (i = 0; i < VPU_MAP_HW_REG_NUM; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			pr_err("Get memory resource failed.\n");
			ret = -ENXIO;
			goto err_ipi_init;
		}
		vpu->map_base[i].base = res->start;
		vpu->map_base[i].len = resource_size(res);
		pr_debug("[VPU] base[%d]: 0x%x 0x%x", i, res->start, resource_size(res));
	}

	pr_debug("[VPU] vpu ipi init\n");
	ret = vpu_ipi_init(vpu);
	if (ret != 0) {
		pr_err("[VPU] Failed to init ipi\n");
		goto err_ipi_init;
	}

	/* register vpu initialization IPI */
	ret = vpu_ipi_register(pdev, IPI_VPU_INIT, vpu_init_ipi_handler,
			       "vpu_init", vpu);
	if (ret) {
		dev_err(dev, "Failed to register IPI_VPU_INIT\n");
		goto vpu_mutex_destroy;
	}

	ret = vpu_iommu_init(dev);
	if (ret != 0) {
		pr_err("[VPU] failed to init vpu iommu\n");
		goto vpu_mutex_destroy;
	}

	init_waitqueue_head(&vpu->ack_wq);

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
vpu_mutex_destroy:
	mutex_destroy(&vpu->vpu_mutex);
err_ipi_init:
	devm_kfree(dev, vpu);

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
	struct mtk_vpu *vpu = platform_get_drvdata(pdev);

	devm_kfree(&pdev->dev, vpu);

	device_destroy(vpu_class, vpu_devno);
	class_destroy(vpu_class);
	cdev_del(vpu_cdev);
	unregister_chrdev_region(vpu_devno, 1);

	return 0;
}

static struct platform_driver mtk_vpu_driver = {
	.probe	= mtk_vpu_probe,
	.remove	= mtk_vpu_remove,
	.driver	= {
		.name	= MTK_VPU_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mtk_vpu_match,
	},
};

module_platform_driver(mtk_vpu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek Video Prosessor Unit driver");
