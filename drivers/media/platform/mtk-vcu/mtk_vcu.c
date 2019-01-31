/*
 * Copyright (c) 2016 MediaTek Inc.
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

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#ifdef CONFIG_MTK_IOMMU
#include <linux/iommu.h>
#endif

#include "mtk_vcu.h"

/**
 * VCU (Video Communication/Controller Unit) is a tiny processor
 * controlling video hardware related to video codec, scaling and color
 * format converting.
 * VCU interfaces with other blocks by share memory and interrupt.
 **/
#define VCU_PATH		"/dev/vpud"
#define MDP_PATH		"/dev/mdpd"
#define VCU_DEVNAME		"vpu"

#define IPI_TIMEOUT_MS		2000U
#define VCU_FW_VER_LEN		16
/*mtk vcu support mpd max value*/
#define MTK_VCU_NR_MAX       2

/* vcu extended mapping length */
#define VCU_PMEM0_LEN(vcu)	(vcu->extmem.p_len)
#define VCU_DMEM0_LEN(vcu)	(vcu->extmem.d_len)
/* vcu extended user virtural address */
#define VCU_PMEM0_VMA(vcu)	(vcu->extmem.p_vma)
#define VCU_DMEM0_VMA(vcu)	(vcu->extmem.d_vma)
/* vcu extended kernel virtural address */
#define VCU_PMEM0_VIRT(vcu)	(vcu->extmem.p_va)
#define VCU_DMEM0_VIRT(vcu)	(vcu->extmem.d_va)
/* vcu extended phsyial address */
#define VCU_PMEM0_PHY(vcu)	(vcu->extmem.p_pa)
#define VCU_DMEM0_PHY(vcu)	(vcu->extmem.d_pa)
/* vcu extended iova address*/
#define VCU_PMEM0_IOVA(vcu)	(vcu->extmem.p_iova)
#define VCU_DMEM0_IOVA(vcu)	(vcu->extmem.d_iova)

#define SHARE_BUF_SIZE		48

#define MAP_SHMEM_ALLOC_BASE	0x80000000
#define MAP_SHMEM_ALLOC_RANGE	0x08000000
#define MAP_SHMEM_ALLOC_END	(MAP_SHMEM_ALLOC_BASE + MAP_SHMEM_ALLOC_RANGE)
#define MAP_SHMEM_COMMIT_BASE	0x88000000
#define MAP_SHMEM_COMMIT_RANGE	0x08000000
#define MAP_SHMEM_COMMIT_END	(MAP_SHMEM_COMMIT_BASE + MAP_SHMEM_COMMIT_RANGE)

#define MAP_SHMEM_MM_BASE	0x90000000
#define MAP_SHMEM_MM_RANGE	0xFFFFFFFF
#define MAP_SHMEM_MM_END	(MAP_SHMEM_MM_BASE + MAP_SHMEM_MM_RANGE)

enum vcu_map_hw_reg_id {
	VDEC,
	VENC,
	VENC_LT,
	VCU_MAP_HW_REG_NUM
};

static const unsigned long vcu_map_hw_type[VCU_MAP_HW_REG_NUM] = {
	0x70000000,	/* VDEC */
	0x71000000,	/* VENC */
	0x72000000	/* VENC_LT */
};

/* Default vcu_mtkdev[0] handle vdec, vcu_mtkdev[1] handle mdp */
static struct mtk_vcu *vcu_mtkdev[MTK_VCU_NR_MAX];

static struct task_struct *vcud_task;
static struct files_struct *files;

/**
 * struct vcu_mem - VCU memory information
 *
 * @p_vma:	the user virtual memory address of
 *		VCU extended program memory
 * @d_vma:	the user  virtual memory address of VCU extended data memory
 * @p_va:	the kernel virtual memory address of
 *		VCU extended program memory
 * @d_va:	the kernel virtual memory address of VCU extended data memory
 * @p_pa:	the physical memory address of VCU extended program memory
 * @d_pa:	the physical memory address of VCU extended data memory
 * @p_iova:	the iova memory address of VCU extended program memory
 * @d_iova:	the iova memory address of VCU extended data memory
 */
struct vcu_mem {
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
};

/**
 * struct vcu_run - VCU initialization status
 *
 * @signaled:		the signal of vcu initialization completed
 * @fw_ver:		VCU firmware version
 * @dec_capability:	decoder capability which is not used for now and
 *			the value is reserved for future use
 * @enc_capability:	encoder capability which is not used for now and
 *			the value is reserved for future use
 * @wq:			wait queue for VCU initialization status
 */
struct vcu_run {
	u32 signaled;
	char fw_ver[VCU_FW_VER_LEN];
	unsigned int	dec_capability;
	unsigned int	enc_capability;
	wait_queue_head_t wq;
};

/**
 * struct vcu_ipi_desc - VCU IPI descriptor
 *
 * @handler:	IPI handler
 * @name:	the name of IPI handler
 * @priv:	the private data of IPI handler
 */
struct vcu_ipi_desc {
	ipi_handler_t handler;
	const char *name;
	void *priv;
};

/**
 * struct share_obj - DTCM (Data Tightly-Coupled Memory) buffer shared with
 *		      AP and VCU
 *
 * @id:		IPI id
 * @len:	share buffer length
 * @share_buf:	share buffer data
 */
struct share_obj {
	s32 id;
	u32 len;
	unsigned char share_buf[SHARE_BUF_SIZE];
};

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @iova:	iova of buffer
 * @len:	buffer length
 * @va: kernel virtual address
 */
struct mem_obj {
	unsigned long iova;
	unsigned long len;
	u64 va;
};

struct compat_mem_obj {
	compat_ulong_t iova;
	compat_ulong_t len;
	compat_u64 va;
};

struct map_hw_reg {
	unsigned long base;
	unsigned long len;
};

/**
 * struct mtk_vcu - vcu driver data
 * @extmem:		VCU extended memory information
 * @run:		VCU initialization status
 * @ipi_desc:		VCU IPI descriptor
 * @dev:		VCU struct device
 * @vcu_mutex:		protect mtk_vcu (except recv_buf) and ensure only
 *			one client to use VCU service at a time. For example,
 *			suppose a client is using VCU to decode VP8.
 *			If the other client wants to encode VP8,
 *			it has to wait until VP8 decode completes.
 * @file:		VCU daemon file pointer
 * @is_open:		The flag to indicate if VCUD device is open.
 * @is_alloc:		The flag to indicate if VCU extended memory is allocated.
 * @ack_wq:		The wait queue for each codec and mdp. When sleeping
 *			processes wake up, they will check the condition
 *			"ipi_id_ack" to run the corresponding action or
 *			go back to sleep.
 * @ipi_id_ack:		The ACKs for registered IPI function sending
 *			interrupt to VCU
 * @vcu_devno:		The vcu_devno for vcu init vcu character device
 * @vcu_cdev:		The point of vcu character device.
 * @vcu_class:		The class_create for create vcu device
 * @vcu_device:		VCU struct device
 * @vcuname:		VCU struct device name in dtsi
 * @path:		The path to keep mdpd path or vcud path.
 * @vpuid:		VCU device id
 *
 */
struct mtk_vcu {
	struct vcu_mem extmem;
	struct vcu_run run;
	struct vcu_ipi_desc ipi_desc[IPI_MAX];
	struct device *dev;
	struct mutex vcu_mutex; /* for protecting vcu data structure */
	struct file *file;
	struct iommu_domain *io_domain;
	struct map_hw_reg map_base[VCU_MAP_HW_REG_NUM];
	bool   is_open;
	bool   is_alloc;
	wait_queue_head_t ack_wq;
	bool ipi_id_ack[IPI_MAX];
	dev_t vcu_devno;
	struct cdev *vcu_cdev;
	struct class *vcu_class;
	struct device *vcu_device;
	const char *vcuname;
	const char *path;
	int vcuid;
};

#define VCUD_SET_OBJECT	_IOW('v', 0, struct share_obj)
#define VCUD_MVA_ALLOCATION	_IOWR('v', 1, struct mem_obj)
#define VCUD_MVA_FREE		_IOWR('v', 2, struct mem_obj)
#define VCUD_CACHE_FLUSH_ALL	_IOR('v', 3, struct mem_obj)

#define COMPAT_VCUD_SET_OBJECT	_IOW('v', 0, struct share_obj)
#define COMPAT_VCUD_MVA_ALLOCATION	_IOWR('v', 1, struct compat_mem_obj)
#define COMPAT_VCUD_MVA_FREE		_IOWR('v', 2, struct compat_mem_obj)
#define COMPAT_VCUD_CACHE_FLUSH_ALL	_IOR('v', 3, struct compat_mem_obj)

static inline bool vcu_running(struct mtk_vcu *vcu)
{
	return vcu->run.signaled;
}

int vcu_ipi_register(struct platform_device *pdev,
		     enum ipi_id id, ipi_handler_t handler,
		     const char *name, void *priv)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct vcu_ipi_desc *ipi_desc;

	if (!vcu) {
		dev_err(&pdev->dev, "vcu device in not ready\n");
		return -EPROBE_DEFER;
	}

	if (id >= 0 && id < IPI_MAX && handler != NULL) {
		ipi_desc = vcu->ipi_desc;
		ipi_desc[id].name = name;
		ipi_desc[id].handler = handler;
		ipi_desc[id].priv = priv;
		return 0;
	}

	dev_err(&pdev->dev, "register vcu ipi id %d with invalid arguments\n",
	       id);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(vcu_ipi_register);

int vcu_ipi_send(struct platform_device *pdev,
		 enum ipi_id id, void *buf,
		 unsigned int len)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	struct share_obj send_obj;
	mm_segment_t old_fs = get_fs();
	unsigned long timeout;
	int ret;

	if (id <= IPI_VCU_INIT || id >= IPI_MAX ||
	    len > sizeof(send_obj.share_buf) || buf == NULL) {
		dev_err(&pdev->dev, "[VCU] failed to send ipi message (Invalid arg.)\n");
		return -EINVAL;
	}

	if (!vcu_running(vcu)) {
		dev_err(&pdev->dev, "[VCU] vcu_ipi_send: VCU is not running\n");
		return -EPERM;
	}

	memcpy((void *)send_obj.share_buf, buf, len);
	send_obj.len = len;
	send_obj.id = id;

	mutex_lock(&vcu->vcu_mutex);
	if (!vcu->is_open) {
		vcu->file = filp_open(vcu->path, O_RDONLY, 0);
		if (IS_ERR(vcu->file)) {
			dev_err(&pdev->dev, "[VCU] Open vcud fail (ret=%ld)\n", PTR_ERR(vcu->file));
			mutex_unlock(&vcu->vcu_mutex);
			return -EINVAL;
		}
		vcu->is_open = true;
	}

	vcu->ipi_id_ack[id] = false;
	/* send the command to VCU */
	set_fs(KERNEL_DS);
#if IS_ENABLED(CONFIG_COMPAT)
	ret = vcu->file->f_op->compat_ioctl(vcu->file, VCUD_SET_OBJECT, (unsigned long)&send_obj);
#else
	ret = vcu->file->f_op->unlocked_ioctl(vcu->file, VCUD_SET_OBJECT, (unsigned long)&send_obj);
#endif
	set_fs(old_fs);
	mutex_unlock(&vcu->vcu_mutex);

	if (ret) {
		dev_err(&pdev->dev, "[VCU] failed to send ipi message (ret=%d)\n", ret);
		goto end;
	}

	/* wait for VCU's ACK */
	timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	ret = wait_event_timeout(vcu->ack_wq, vcu->ipi_id_ack[id], timeout);
	vcu->ipi_id_ack[id] = false;
	if (ret == 0) {
		dev_err(&pdev->dev, "vcu ipi %d ack time out !", id);
		ret = -EIO;
		goto end;
	} else if (-ERESTARTSYS == ret) {
		dev_err(&pdev->dev, "vcu ipi %d ack wait interrupted by a signal",
		       id);
		ret = -ERESTARTSYS;
		goto end;
	}

	ret = 0;
end:
	return ret;
}
EXPORT_SYMBOL_GPL(vcu_ipi_send);

unsigned int vcu_get_vdec_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.dec_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_vdec_hw_capa);

unsigned int vcu_get_venc_hw_capa(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	return vcu->run.enc_capability;
}
EXPORT_SYMBOL_GPL(vcu_get_venc_hw_capa);

void *vcu_mapping_dm_addr(struct platform_device *pdev,
			  uintptr_t dtcm_dmem_addr)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	uintptr_t d_vma = (uintptr_t)(dtcm_dmem_addr);
	uintptr_t d_va_start = (uintptr_t)VCU_DMEM0_VIRT(vcu);
	uintptr_t d_off = d_vma - VCU_DMEM0_VMA(vcu);
	uintptr_t d_va;

	if (!dtcm_dmem_addr || d_off > VCU_DMEM0_LEN(vcu)) {
		dev_err(&pdev->dev, "[VCU] %s: Invalid vma 0x%lx\n", __func__, dtcm_dmem_addr);
		return NULL;
	}

	d_va = d_va_start + d_off;
	dev_dbg(&pdev->dev, "[VCU] %s: 0x%lx -> 0x%lx\n", __func__, d_vma, d_va);

	return (void *)d_va;
}
EXPORT_SYMBOL_GPL(vcu_mapping_dm_addr);

struct platform_device *vcu_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *vcu_node;
	struct platform_device *vcu_pdev;

	dev_dbg(&pdev->dev, "[VCU] %s\n", __func__);

	vcu_node = of_parse_phandle(dev->of_node, "mediatek,vcu", 0);
	if (!vcu_node) {
		dev_err(dev, "[VCU] can't get vcu node\n");
		return NULL;
	}

	vcu_pdev = of_find_device_by_node(vcu_node);
	if (WARN_ON(!vcu_pdev)) {
		dev_err(dev, "[VCU] vcu pdev failed\n");
		of_node_put(vcu_node);
		return NULL;
	}

	return vcu_pdev;
}
EXPORT_SYMBOL_GPL(vcu_get_plat_device);

int vcu_load_firmware(struct platform_device *pdev)
{
	if (!pdev) {
		dev_err(&pdev->dev, "[VCU] VCU platform device is invalid\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(vcu_load_firmware);

int vcu_compare_version(struct platform_device *pdev,
			const char *expected_version)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);
	int cur_major, cur_minor, cur_build, cur_rel, cur_ver_num;
	int major, minor, build, rel, ver_num;
	char *cur_version = vcu->run.fw_ver;

	cur_ver_num = sscanf(cur_version, "%d.%d.%d-rc%d",
			     &cur_major, &cur_minor, &cur_build, &cur_rel);
	if (cur_ver_num < 3)
		return -1;
	ver_num = sscanf(expected_version, "%d.%d.%d-rc%d",
			 &major, &minor, &build, &rel);
	if (ver_num < 3)
		return -1;

	if (cur_major < major)
		return -1;
	if (cur_major > major)
		return 1;

	if (cur_minor < minor)
		return -1;
	if (cur_minor > minor)
		return 1;

	if (cur_build < build)
		return -1;
	if (cur_build > build)
		return 1;

	if (cur_ver_num < ver_num)
		return -1;
	if (cur_ver_num > ver_num)
		return 1;

	if (ver_num > 3) {
		if (cur_rel < rel)
			return -1;
		if (cur_rel > rel)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vcu_compare_version);

void vcu_get_task(struct task_struct **task, struct files_struct **f)
{
	pr_debug("mtk_vcu_get_task %p\n", vcud_task);
	*task = vcud_task;
	*f = files;
}
EXPORT_SYMBOL_GPL(vcu_get_task);

static int vcu_ipi_handler(struct mtk_vcu *vcu, struct share_obj *rcv_obj)
{
	struct vcu_ipi_desc *ipi_desc = vcu->ipi_desc;
	int non_ack = 0;
	int ret = -1;

	if (rcv_obj->id < IPI_MAX && ipi_desc[rcv_obj->id].handler) {
		non_ack = ipi_desc[rcv_obj->id].handler(rcv_obj->share_buf,
							rcv_obj->len,
							ipi_desc[rcv_obj->id].priv);
		if (rcv_obj->id > IPI_VCU_INIT && non_ack == 0) {
			vcu->ipi_id_ack[rcv_obj->id] = true;
			wake_up(&vcu->ack_wq);
		}
		ret = 0;
	} else {
		dev_err(vcu->dev, "[VCU] No such ipi id = %d\n", rcv_obj->id);
	}

	return ret;
}

static int vcu_ipi_init(struct mtk_vcu *vcu)
{
	vcu->is_open = false;
	vcu->is_alloc = false;
	mutex_init(&vcu->vcu_mutex);

	return 0;
}

static int vcu_init_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_vcu *vcu = (struct mtk_vcu *)priv;
	struct vcu_run *run = (struct vcu_run *)data;

	vcu->run.signaled = run->signaled;
	strncpy(vcu->run.fw_ver, run->fw_ver, VCU_FW_VER_LEN);
	vcu->run.dec_capability = run->dec_capability;
	vcu->run.enc_capability = run->enc_capability;

	dev_dbg(vcu->dev, "[VCU] fw ver: %s\n", vcu->run.fw_ver);
	dev_dbg(vcu->dev, "[VCU] dec cap: %x\n", vcu->run.dec_capability);
	dev_dbg(vcu->dev, "[VCU] enc cap: %x\n", vcu->run.enc_capability);
	return 0;
}

static int mtk_vcu_open(struct inode *inode, struct file *file)
{
	int vcuid;

	if (strcmp(current->comm, "mdpd") != 0) {
		vcud_task = current;
		files = vcud_task->files;
		vcuid = 0;

	} else {
		vcuid = 1;
		pr_debug("[VCU] thread name: %s\n", current->comm);
	}

	vcu_mtkdev[vcuid]->vcuid = vcuid;
	file->private_data = (struct mtk_vcu *)vcu_mtkdev[vcuid];

	return 0;
}

static int mtk_vcu_release(struct inode *inode, struct file *file)
{
	return 0;
}

static void vcu_free_d_ext_mem(struct mtk_vcu *vcu)
{
	mutex_lock(&vcu->vcu_mutex);
	if (vcu->is_open) {
		filp_close(vcu->file, NULL);
		vcu->is_open = false;
	}
	if (vcu->is_alloc) {
		kfree(VCU_DMEM0_VIRT(vcu));
		VCU_DMEM0_VIRT(vcu) = NULL;
		vcu->is_alloc = false;
	}
	mutex_unlock(&vcu->vcu_mutex);
}

static int vcu_alloc_d_ext_mem(struct mtk_vcu *vcu, unsigned long len)
{
	mutex_lock(&vcu->vcu_mutex);
	if (!vcu->is_alloc) {
		VCU_DMEM0_VIRT(vcu) = kmalloc(len, GFP_KERNEL);
		VCU_DMEM0_PHY(vcu) = virt_to_phys(VCU_DMEM0_VIRT(vcu));
		VCU_DMEM0_LEN(vcu) = len;
		vcu->is_alloc = true;
	}
	mutex_unlock(&vcu->vcu_mutex);

	dev_dbg(vcu->dev, "[VCU] Data extend memory (len:%lu) phy=0x%llx virt=0x%p iova=0x%llx\n",
		VCU_DMEM0_LEN(vcu),
		(unsigned long long)VCU_DMEM0_PHY(vcu),
		VCU_DMEM0_VIRT(vcu),
		(unsigned long long)VCU_DMEM0_IOVA(vcu));
	return 0;
}

static int mtk_vcu_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa_end = pa_start + length;
	unsigned long start = vma->vm_start;
	unsigned long pos = 0;
	int i;
	struct mtk_vcu *vcu_mtkdev = (struct mtk_vcu *)file->private_data;

	pr_debug("[VCU] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 vma->vm_start, vma->vm_end, vma->vm_pgoff);

	/*only vcud need this case*/
	if (vcu_mtkdev->vcuid == 0) {
		for (i = 0; i < VCU_MAP_HW_REG_NUM; i++) {
			if (pa_start == vcu_map_hw_type[i] && length <= vcu_mtkdev->map_base[i].len) {
				vma->vm_pgoff = vcu_mtkdev->map_base[i].base >> PAGE_SHIFT;
				goto reg_valid_map;
			}
		}
	}

	if (pa_start >= MAP_SHMEM_ALLOC_BASE && pa_end <= MAP_SHMEM_ALLOC_END) {
		vcu_free_d_ext_mem(vcu_mtkdev);
		if (vcu_alloc_d_ext_mem(vcu_mtkdev, length)) {
			dev_err(vcu_mtkdev->dev, "[VCU] allocate DM failed\n");
			return -ENOMEM;
		}
		vma->vm_pgoff = (unsigned long)(VCU_DMEM0_PHY(vcu_mtkdev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_COMMIT_BASE && pa_end <= MAP_SHMEM_COMMIT_END) {
		VCU_DMEM0_VMA(vcu_mtkdev) = vma->vm_start;
		vma->vm_pgoff = (unsigned long)(VCU_DMEM0_PHY(vcu_mtkdev) >> PAGE_SHIFT);
		goto valid_map;
	}

	if (pa_start >= MAP_SHMEM_MM_BASE) {
		pa_start = pa_start - MAP_SHMEM_MM_BASE;
#ifdef CONFIG_MTK_IOMMU
		while (length > 0) {
			vma->vm_pgoff = iommu_iova_to_phys(vcu_mtkdev->io_domain,
						   pa_start + pos);
			vma->vm_pgoff >>= PAGE_SHIFT;
			vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
			if (remap_pfn_range(vma, start, vma->vm_pgoff, PAGE_SIZE, vma->vm_page_prot))
				return -EAGAIN;

			start += PAGE_SIZE;
			pos += PAGE_SIZE;
			if (length > PAGE_SIZE)
				length -= PAGE_SIZE;
			else
				length = 0;
		}
		return 0;
#endif
	}

	dev_err(vcu_mtkdev->dev, "[VCU] Invalid argument\n");
	return -EINVAL;

reg_valid_map:
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

valid_map:
	dev_dbg(vcu_mtkdev->dev, "[VCU] Mapping pgoff 0x%lx\n", vma->vm_pgoff);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}

	return 0;
}

static long mtk_vcu_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -1;
	unsigned char *user_data_addr = NULL;
	struct mtk_vcu *vcu_mtkdev = (struct mtk_vcu *)file->private_data;

	struct device *dev = vcu_mtkdev->dev;

	switch (cmd) {
	case VCUD_SET_OBJECT:
		{
			struct share_obj intermediate;

			user_data_addr = (unsigned char *)arg;
			ret = copy_from_user(&intermediate, user_data_addr, sizeof(struct share_obj));
			if (ret != 0) {
				pr_err("[VCU] %s(%d) Copy data from user failed!\n", __func__, __LINE__);
				return -ENOMEM;
			}
			ret = vcu_ipi_handler(vcu_mtkdev, (struct share_obj *)user_data_addr);
		}
		break;
	case VCUD_MVA_ALLOCATION:
		{
			struct mem_obj intermediate;

			user_data_addr = (unsigned char *)arg;
			ret = copy_from_user(&intermediate, user_data_addr, sizeof(struct mem_obj));
			if (ret != 0) {
				pr_err("[VCU] %s(%d) Copy data from user failed!\n", __func__, __LINE__);
				return -ENOMEM;
			}

			intermediate.va = (unsigned long)dma_alloc_coherent(dev, intermediate.len,
							(dma_addr_t *)(&intermediate.iova), GFP_KERNEL);
			if (!intermediate.iova) {
				pr_err("[VCU] Dma alloc buf failed!\n");
				return -ENOMEM;
			}

			ret = copy_to_user(user_data_addr, &intermediate, sizeof(struct mem_obj));
			if (ret != 0) {
				pr_err("[VCU] %s(%d) Copy data to user failed!\n", __func__, __LINE__);
				return -ENOMEM;
			}
			ret = 0;
		}
		break;
	case VCUD_MVA_FREE:
		{
			struct mem_obj intermediate;

			user_data_addr = (unsigned char *)arg;
			ret = copy_from_user(&intermediate, user_data_addr, sizeof(struct mem_obj));
			if (ret != 0) {
				pr_err("[VCU] %s(%d) Copy data from user failed!\n", __func__, __LINE__);
				return -ENOMEM;
			}
			if (!intermediate.iova) {
				dev_err(dev, "[VCU] Dma free buf failed!\n");
				return -ENOMEM;
			}

			dma_free_coherent(dev, intermediate.len, (void *)(intermediate.va),
							intermediate.iova);
			intermediate.va = 0;
			intermediate.iova = 0;

			ret = copy_to_user(user_data_addr, &intermediate, sizeof(struct mem_obj));
			if (ret != 0) {
				pr_err("[VCU] %s(%d) Copy data to user failed!\n", __func__, __LINE__);
				return -ENOMEM;
			}
			ret = 0;
		}
		break;
	case VCUD_CACHE_FLUSH_ALL:
		dev_dbg(dev, "[VCU] Flush cache in kernel\n");
#if defined(CONFIG_BIT32)
		outer_flush_all();
#endif
		ret = 0;
		break;
	default:
		dev_err(dev, "[VCU] Unknown cmd\n");
		break;
	}

	return ret;
}

static int compat_get_vpud_allocation_data(
				struct compat_mem_obj __user *data32,
				struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	int err = 0;

	err = get_user(l, &data32->iova);
	err |= put_user(l, &data->iova);
	err |= get_user(l, &data32->len);
	err |= put_user(l, &data->len);
	err |= get_user(u, &data32->va);
	err |= put_user(u, &data->va);

	return err;
}

static int compat_put_vpud_allocation_data(
				struct compat_mem_obj __user *data32,
				struct mem_obj __user *data)
{
	compat_ulong_t l;
	compat_u64 u;
	int err = 0;

	err = get_user(l, &data->iova);
	err |= put_user(l, &data32->iova);
	err |= get_user(l, &data->len);
	err |= put_user(l, &data32->len);
	err |= get_user(u, &data->va);
	err |= put_user(u, &data32->va);

	return err;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mtk_vcu_unlocked_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	long ret = -1;

	switch (cmd) {
	case COMPAT_VCUD_SET_OBJECT:
		{
			struct share_obj __user *data32;

			data32 = compat_ptr(arg);
			ret = file->f_op->unlocked_ioctl(file, VCUD_SET_OBJECT, (unsigned long)data32);
		}
		break;
	case COMPAT_VCUD_MVA_ALLOCATION:
		{
			struct compat_mem_obj __user *data32;
			struct mem_obj __user *data;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(struct mem_obj));
			if (!data)
				return -EFAULT;

			err = compat_get_vpud_allocation_data(data32, data);
			if (err)
				return err;
			ret = file->f_op->unlocked_ioctl(file, VCUD_MVA_ALLOCATION, (unsigned long)data);

			err = compat_put_vpud_allocation_data(data32, data);
			if (err)
				return err;
		}
		break;
	case COMPAT_VCUD_MVA_FREE:
		{
			struct compat_mem_obj __user *data32;
			struct mem_obj __user *data;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(struct mem_obj));
			if (!data)
				return -EFAULT;

			err = compat_get_vpud_allocation_data(data32, data);
			if (err)
				return err;
			ret = file->f_op->unlocked_ioctl(file, VCUD_MVA_FREE, (unsigned long)data);

			err = compat_put_vpud_allocation_data(data32, data);
			if (err)
				return err;
		}
		break;
	case COMPAT_VCUD_CACHE_FLUSH_ALL:
		ret = file->f_op->unlocked_ioctl(file, VCUD_CACHE_FLUSH_ALL, 0);
		break;
	default:
		pr_err("[VCU] Invalid cmd_number 0x%x.\n", cmd);
		break;
	}
	return ret;
}
#endif

static const struct file_operations vcu_fops = {
	.owner      = THIS_MODULE,
	.unlocked_ioctl = mtk_vcu_unlocked_ioctl,
	.open       = mtk_vcu_open,
	.release    = mtk_vcu_release,
	.mmap       = mtk_vcu_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = mtk_vcu_unlocked_compat_ioctl,
#endif
};

static int mtk_vcu_probe(struct platform_device *pdev)
{
	struct mtk_vcu *vcu;
	struct device *dev;
	struct resource *res;
	int i, vcuid, ret = 0;

	dev_dbg(&pdev->dev, "[VCU] initialization\n");

	dev = &pdev->dev;
	vcu = devm_kzalloc(dev, sizeof(*vcu), GFP_KERNEL);
	if (!vcu)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "mediatek,vcuid", &vcuid);
	if (ret) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuid\n");
		return ret;
	}
	vcu_mtkdev[vcuid] = vcu;

#ifdef CONFIG_MTK_IOMMU
	vcu_mtkdev[vcuid]->io_domain = iommu_get_domain_for_dev(dev);
	if (vcu_mtkdev[vcuid]->io_domain == NULL) {
		dev_err(dev, "[VCU] vcuid: %d get iommu domain fail !!\n", vcuid);
		return -EPROBE_DEFER;
	}
	dev_err(dev, "vcu iommudom: %p,vcuid:%d\n", vcu_mtkdev[vcuid]->io_domain, vcuid);
#endif

	if (vcuid == 1)
		vcu_mtkdev[vcuid]->path = MDP_PATH;
	else if (vcuid == 0)
		vcu_mtkdev[vcuid]->path = VCU_PATH;
	else
		return -ENXIO;

	ret = of_property_read_string(dev->of_node, "mediatek,vcuname", &vcu_mtkdev[vcuid]->vcuname);
	if (ret) {
		dev_err(dev, "[VCU] failed to find mediatek,vcuname\n");
		return ret;
	}

	vcu->dev = &pdev->dev;
	platform_set_drvdata(pdev, vcu_mtkdev[vcuid]);

	if (vcuid == 0) {
		for (i = 0; i < VCU_MAP_HW_REG_NUM; i++) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (!res) {
				dev_err(dev, "Get memory resource failed.\n");
				ret = -ENXIO;
				goto err_ipi_init;
			}
			vcu->map_base[i].base = res->start;
			vcu->map_base[i].len = resource_size(res);
			dev_dbg(dev, "[VCU] base[%d]: 0x%lx 0x%lx", i, vcu->map_base[i].base,
				vcu->map_base[i].len);
		}
	}
	dev_dbg(dev, "[VCU] vcu ipi init\n");
	ret = vcu_ipi_init(vcu);
	if (ret != 0) {
		dev_err(dev, "[VCU] Failed to init ipi\n");
		goto err_ipi_init;
	}

	/* register vcu initialization IPI */
	ret = vcu_ipi_register(pdev, IPI_VCU_INIT, vcu_init_ipi_handler,
			       "vcu_init", vcu);
	if (ret) {
		dev_err(dev, "Failed to register IPI_VCU_INIT\n");
		goto vcu_mutex_destroy;
	}

	init_waitqueue_head(&vcu->ack_wq);
	/* init character device */

	ret = alloc_chrdev_region(&vcu_mtkdev[vcuid]->vcu_devno, 0, 1, vcu_mtkdev[vcuid]->vcuname);
	if (ret < 0) {
		dev_err(dev, "[VCU]  alloc_chrdev_region failed (ret=%d)\n", ret);
		goto err_alloc;
	}

	vcu_mtkdev[vcuid]->vcu_cdev = cdev_alloc();
	vcu_mtkdev[vcuid]->vcu_cdev->owner = THIS_MODULE;
	vcu_mtkdev[vcuid]->vcu_cdev->ops = &vcu_fops;

	ret = cdev_add(vcu_mtkdev[vcuid]->vcu_cdev, vcu_mtkdev[vcuid]->vcu_devno, 1);
	if (ret < 0) {
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_class = class_create(THIS_MODULE, vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_class)) {
		ret = PTR_ERR(vcu_mtkdev[vcuid]->vcu_class);
		dev_err(dev, "[VCU] class create fail (ret=%d)", ret);
		goto err_add;
	}

	vcu_mtkdev[vcuid]->vcu_device = device_create(vcu_mtkdev[vcuid]->vcu_class, NULL,
				vcu_mtkdev[vcuid]->vcu_devno, NULL, vcu_mtkdev[vcuid]->vcuname);
	if (IS_ERR(vcu_mtkdev[vcuid]->vcu_device)) {
		ret = PTR_ERR(vcu_mtkdev[vcuid]->vcu_device);
		dev_err(dev, "[VCU] device_create fail (ret=%d)", ret);
		goto err_device;
	}

	dev_dbg(dev, "[VCU] initialization completed\n");
	return 0;

err_device:
	class_destroy(vcu_mtkdev[vcuid]->vcu_class);
err_add:
	cdev_del(vcu_mtkdev[vcuid]->vcu_cdev);
err_alloc:
	unregister_chrdev_region(vcu_mtkdev[vcuid]->vcu_devno, 1);
vcu_mutex_destroy:
	mutex_destroy(&vcu->vcu_mutex);
err_ipi_init:
	devm_kfree(dev, vcu);

	return ret;
}

static const struct of_device_id mtk_vcu_match[] = {
	{.compatible = "mediatek,mt8173-vcu",},
	{.compatible = "mediatek,mt2701-vpu",},
	{.compatible = "mediatek,mt2712-vcu",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vcu_match);

static int mtk_vcu_remove(struct platform_device *pdev)
{
	struct mtk_vcu *vcu = platform_get_drvdata(pdev);

	if (vcu->is_open) {
		filp_close(vcu->file, NULL);
		vcu->is_open = false;
	}
	devm_kfree(&pdev->dev, vcu);

	device_destroy(vcu->vcu_class, vcu->vcu_devno);
	class_destroy(vcu->vcu_class);
	cdev_del(vcu->vcu_cdev);
	unregister_chrdev_region(vcu->vcu_devno, 1);

	return 0;
}

static struct platform_driver mtk_vcu_driver = {
	.probe	= mtk_vcu_probe,
	.remove	= mtk_vcu_remove,
	.driver	= {
		.name	= "mtk_vcu",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_vcu_match,
	},
};

module_platform_driver(mtk_vcu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek Video Communication And Controller Unit driver");
