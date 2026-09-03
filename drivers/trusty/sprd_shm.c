/*
 * drivers/misc/sprd_shm.c
 *
 * Copyright (C) 2017 Unisoc, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "tshm: " fmt

#include <linux/dma-buf.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "sprd_shm.h"

#include <linux/trusty/smcall.h>
#include <trusty.h>


#define TSHM_KERN_POOL_ORDER (3)
#define TSHM_USER_POOL_ORDER  (PAGE_SHIFT)
/**
 * The basic operation functions of tshm pool:
 * alloc, free, create, destroy and config, etc.
 */
static int tshm_pool_alloc(struct gen_pool *pool, struct tshm *shm,
			size_t size)
{
	void *va;
	size_t sz = roundup(size, 1 << pool->min_alloc_order);

	va = (void *)gen_pool_alloc(pool, sz);
	if (!va)
		return -ENOMEM;

	memset(va, 0, sz);
	shm->kaddr = va;
	shm->paddr = gen_pool_virt_to_phys(pool, (unsigned long)va);
	shm->size = sz;

	return 0;
}

static void tshm_pool_free(struct gen_pool *pool, struct tshm *shm)
{
	gen_pool_free(pool, (unsigned long)shm->kaddr, shm->size);
	shm->kaddr = NULL;
}

static struct gen_pool *tshm_pool_create(void *vaddr,
					phys_addr_t paddr,
					size_t size,
					int min_alloc_order)
{
	struct gen_pool *pool;
	int ret;

	/* Ensure start address and size is page aligned */
	if (!PAGE_ALIGNED(vaddr) || !PAGE_ALIGNED(paddr) || !PAGE_ALIGNED(size))
		return ERR_PTR(-EINVAL);

	pool = gen_pool_create(min_alloc_order, -1);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	gen_pool_set_algo(pool, gen_pool_best_fit, NULL);
	ret = gen_pool_add_virt(pool, (unsigned long)vaddr, paddr, size, -1);
	if (ret) {
		gen_pool_destroy(pool);
		return ERR_PTR(ret);
	}

	return pool;
}

static void tshm_pool_destroy(struct gen_pool *pool)
{
	gen_pool_destroy(pool);
}

static int tshm_config_pool(struct tshm_device *shmdev)
{
	struct gen_pool *pool;
	void *vaddr;
	phys_addr_t paddr;
	size_t size;
	size_t kern_pool_size;

	paddr = shmdev->pool_params.start;
	size  = shmdev->pool_params.size;
	kern_pool_size = shmdev->pool_params.kern_pool_size;

	/*
	 * the page count used for kernel pool should less than a half of
	 * the total pages
	 */
	if (kern_pool_size > ((size << PAGE_SHIFT) >> 1))
		return -EINVAL;

	vaddr = memremap(paddr, size, MEMREMAP_WB);
	if (!vaddr)
		return -ENXIO;
	shmdev->pool_base_va = vaddr;

	if (kern_pool_size != 0) {
		pool = tshm_pool_create(vaddr, paddr, kern_pool_size,
				TSHM_KERN_POOL_ORDER);
		if (IS_ERR(pool)) {
			memunmap(vaddr);
			return PTR_ERR(pool);
		}
		shmdev->kern_pool = pool;
		pr_info("kernel pool created: phys_addr %#zx, size %#zx\n",
			(size_t)paddr, kern_pool_size);
	}

	pool = tshm_pool_create(vaddr + kern_pool_size,	paddr + kern_pool_size,
				size - kern_pool_size, TSHM_USER_POOL_ORDER);
	if (IS_ERR(pool)) {
		if (kern_pool_size != 0)
			tshm_pool_destroy(shmdev->kern_pool);
		memunmap(vaddr);
		return PTR_ERR(pool);
	}
	shmdev->user_pool = pool;
	pr_info("user pool created: phys_addr %#zx, size %#zx\n",
		(size_t)paddr + kern_pool_size, size - kern_pool_size);

	return 0;
}

static struct dma_buf *tshm_dma_buf_share(struct tshm *shm);

static struct tshm *tshm_alloc_shm(struct tshm_device *shmdev,
				   u64 size, u32 flags)
{
	void *ret;
	int rc;
	struct tshm *shm;
	struct gen_pool *pool;

	shm = kzalloc(sizeof(*shm), GFP_KERNEL);
	if (!shm) {
		ret = ERR_PTR(-ENOMEM);
		goto kzalloc_failed;
	}

	shm->dev = shmdev;
	shm->flags = flags;

	if (flags & TSHM_ALLOC_USER)
		pool = shmdev->user_pool;
	else if (flags & TSHM_ALLOC_KERN)
		pool = shmdev->kern_pool;
	else {
		ret = ERR_PTR(-EINVAL);
		goto flags_invalid;
	}

	rc = tshm_pool_alloc(pool, shm, size);
	if (rc) {
		ret = ERR_PTR(rc);
		goto pool_alloc_failed;
	}

	mutex_lock(&shmdev->lock);
	shm->id = idr_alloc(&shmdev->idr, shm, 1, 0, GFP_KERNEL);
	mutex_unlock(&shmdev->lock);
	if (shm->id < 0) {
		ret = ERR_PTR(shm->id);
		goto idr_alloc_failed;
	}

	if (flags & TSHM_ALLOC_USER) {
		shm->dmabuf = tshm_dma_buf_share(shm);
		if (IS_ERR(shm->dmabuf)) {
			ret = ERR_CAST(shm->dmabuf);
			goto dma_buf_export_failed;
		}
	}

	return shm;

dma_buf_export_failed:
	mutex_lock(&shmdev->lock);
	idr_remove(&shmdev->idr, shm->id);
	mutex_unlock(&shmdev->lock);
idr_alloc_failed:
	tshm_pool_free(pool, shm);
pool_alloc_failed:
flags_invalid:
	kfree(shm);
kzalloc_failed:
	return ret;
}

static void tshm_free_shm(struct tshm *shm)
{
	struct tshm_device *shmdev = shm->dev;
	struct gen_pool *pool;
	uint32_t paddr_h, paddr_l;
	int ret = 0;

#ifdef CONFIG_ARM64
	paddr_h = (uint32_t)(shm->paddr >> 32);
	paddr_l = (uint32_t)(shm->paddr & 0xffffffff);
#else
	paddr_h = 0;
	paddr_l = (uint32_t)(shm->paddr & 0xffffffff);
#endif

	if (shm->flags & TSHM_REGISTERED) {
		ret = trusty_std_call32(shmdev->trusty_dev, SMC_SC_SHM_UNREGISTER,
				shm->id, paddr_h, paddr_l);
	}

	if (ret == 0) {
		mutex_lock(&shmdev->lock);
		idr_remove(&shmdev->idr, shm->id);
		mutex_unlock(&shmdev->lock);
	} else {
		pr_err("stdcall free shm failed 0x%x\n", ret);
	}

	if (shm->flags & TSHM_ALLOC_USER)
		pool = shmdev->user_pool;
	else if (shm->flags & TSHM_ALLOC_KERN)
		pool = shmdev->kern_pool;
	else {
		pr_err("free shm: invalid flags %u\n", shm->flags);
		return;
	}

	tshm_pool_free(pool, shm);
	kfree(shm);
}



/**
 * dmabuf operation functions
 *
 * Some dummy functions(map/unmap/kmap/...) must be filled in dma_buf_ops
 * since dma_buf_export will check them, although it is weird.
 */
static struct sg_table *tshm_dma_buf_map_dma_buf(struct dma_buf_attachment *attach,
					 enum dma_data_direction dir)
{
	return NULL;
}

static void tshm_dma_buf_unmap(struct dma_buf_attachment *attach,
			       struct sg_table *table,
			       enum dma_data_direction dir)
{
}

static void tshm_dma_buf_release(struct dma_buf *dmabuf)
{
	struct tshm *shm = dmabuf->priv;

	tshm_free_shm(shm);
}

static int tshm_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct tshm *shm = dmabuf->priv;
	size_t size = vma->vm_end - vma->vm_start;

	return remap_pfn_range(vma, vma->vm_start, shm->paddr >> PAGE_SHIFT,
			       size, vma->vm_page_prot);
}

static const struct dma_buf_ops tshm_dma_buf_ops = {
	.map_dma_buf = tshm_dma_buf_map_dma_buf,
	.unmap_dma_buf = tshm_dma_buf_unmap,
	.mmap = tshm_dma_buf_mmap,
	.release = tshm_dma_buf_release,
};

static int tshm_dma_buf_share_fd(struct tshm *shm)
{
	int fd;

	fd = dma_buf_fd(shm->dmabuf, O_CLOEXEC);

	return fd;
}

static struct dma_buf *tshm_dma_buf_share(struct tshm *shm)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &tshm_dma_buf_ops;
	exp_info.size = shm->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = shm;

	return dma_buf_export(&exp_info);
}

/**
 * The file operation functions of tshm device
 */

static int tshm_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tshm_device *tshmdev =
		container_of(miscdev, struct tshm_device, dev);

	/* Replace miscdev with tshmdev in the file's private_data */
	file->private_data = tshmdev;

	return 0;
}

static int tshm_release(struct inode *inode, struct file *file)
{
	return 0;
}

union tshm_ioctl_data {
	struct tshm_alloc_data alloc_data;
	struct tshm_register_data reg_data;
};

static long tshm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct tshm *shm;
	struct tshm_device *shmdev = filp->private_data;
	union tshm_ioctl_data data;
	uint32_t paddr_h, paddr_l, id;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case TSHM_IOC_ALLOC:
		shm = tshm_alloc_shm(shmdev, data.alloc_data.size,
				     TSHM_ALLOC_USER);
		if (IS_ERR(shm))
			return PTR_ERR(shm);

#ifdef CONFIG_ARM64
		paddr_h = (uint32_t)(shm->paddr >> 32);
		paddr_h = paddr_h & 0xffff;
		paddr_l = (uint32_t)(shm->paddr & 0xffffffff);
#else
		paddr_h = 0;
		paddr_l = (uint32_t)(shm->paddr & 0xffffffff);
#endif
		id = (uint32_t)shm->id;

		ret = trusty_std_call32(shmdev->trusty_dev,
				SMC_SC_SHM_REGISTER,
				((id & 0xffff) << 16) | paddr_h,
				paddr_l, (uint32_t)shm->size);

		if (ret < 0) {
			pr_debug("trusty register shm failed (%ld)\n", ret);
			tshm_free_shm(shm);
			return ret;
		}

		shm->flags |= TSHM_REGISTERED;

		data.alloc_data.id = shm->id;
		data.alloc_data.flags = shm->flags;
		/*
		 * The actual size may be greater than the requsted size due to
		 * size alignment in the shm allocation function
		 */
		data.alloc_data.size = shm->size;
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			ret = -EFAULT;
		else
			ret = tshm_dma_buf_share_fd(shm);

		break;
	}

	return ret;
}

static const struct file_operations tshm_fops = {
	.owner		= THIS_MODULE,
	.open		= tshm_open,
	.release	= tshm_release,
	.unlocked_ioctl = tshm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= tshm_ioctl,
#endif
};


static int tshm_parse_dt(struct tshm_device *shmdev,
			struct device_node *np)
{
	struct device_node *npmr;
	struct tshm_pool_params *params;
	u64 size;
	int ret;

	if (!np)
		return -ENODEV;

	npmr = of_parse_phandle(np, "memory-region", 0);
	if (!npmr)
		return -ENODEV;

	params = &shmdev->pool_params;

	params->start = of_translate_address(npmr,
		of_get_address(npmr, 0, &size, NULL));
	params->size = size;
	of_node_put(npmr);

	if (!PAGE_ALIGNED(params->start) || !PAGE_ALIGNED(params->size))
		return -EINVAL;

	ret = of_property_read_u64(np, "kern_pool_size", &size);
	if (ret)
		size = 0;

	params->kern_pool_size = (size_t)size;

	return 0;
}

int tshm_device_probe(struct platform_device *pdev)
{
	struct tshm_device *shmdev;
	int ret;

	shmdev = kzalloc(sizeof(*shmdev), GFP_KERNEL);
	if (!shmdev)
		return -ENOMEM;

	ret = tshm_parse_dt(shmdev, pdev->dev.of_node);
	if (ret)
		goto err_parse_dt_failed;

	shmdev->dev.minor = MISC_DYNAMIC_MINOR;
	shmdev->dev.name = "tshm";
	shmdev->dev.fops = &tshm_fops;
	shmdev->dev.parent = NULL;
	shmdev->trusty_dev = pdev->dev.parent;

	ret = misc_register(&shmdev->dev);
	if (ret)
		goto err_miscdev_register_failed;

	ret = tshm_config_pool(shmdev);
	if (ret)
		goto err_config_pool_failed;

	idr_init(&shmdev->idr);
	mutex_init(&shmdev->lock);
	platform_set_drvdata(pdev, shmdev);

	return 0;

err_config_pool_failed:
	misc_deregister(&shmdev->dev);
err_miscdev_register_failed:
err_parse_dt_failed:
	kfree(shmdev);
	return ret;
}

static int tshm_device_remove(struct platform_device *pdev)
{
	struct tshm_device *shmdev = platform_get_drvdata(pdev);

	if (shmdev->kern_pool)
		tshm_pool_destroy(shmdev->kern_pool);
	tshm_pool_destroy(shmdev->user_pool);
	memunmap(shmdev->pool_base_va);

	idr_destroy(&shmdev->idr);
	mutex_destroy(&shmdev->lock);
	misc_deregister(&shmdev->dev);

	kfree(shmdev);

	return 0;
}

static const struct of_device_id tshm_device_ids[] = {
	{ .compatible = "sprd,tshm", },
	{}
};
MODULE_DEVICE_TABLE(of, tshm_device_ids);

static struct platform_driver tshm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "tshm",
		.of_match_table = of_match_ptr(tshm_device_ids),
	},
	.probe  = tshm_device_probe,
	.remove = tshm_device_remove,
};

static int __init tshm_init(void)
{
	return platform_driver_register(&tshm_driver);
}

static void __exit tshm_exit(void)
{
	platform_driver_unregister(&tshm_driver);
}

module_init(tshm_init);
module_exit(tshm_exit);

MODULE_DESCRIPTION("SPRD TSHM driver");
MODULE_LICENSE("GPL");
