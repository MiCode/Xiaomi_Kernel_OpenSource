/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-smem: " fmt

#include <linux/cdev.h>
#include <linux/genalloc.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sipc.h>
#include <linux/module.h>

#include "sipc_priv.h"

/*
 * workround: Due to orca ipa hardware limitations
 * the sipc share memory must map from
 * 0x280000000(orca side) to 0x80000000(roc1
 * side), and the size must be 256M
 */
#ifdef CONFIG_UNISOC_IPA_PCIE_WORKROUND
#define IPA_SRC_BASE	0x280000000
#define IPA_DST_BASE	0x80000000
#define IPA_SIZE	0x10000000
#endif

#if defined(CONFIG_DEBUG_FS)
struct smem_device {
	int			major;
	int			minor;
	struct cdev		cdev;
	struct device		*sys_dev;	/* Device object in sysfs */
};

static struct class *smem_class;
static struct smem_device *smem_dev;
#endif

struct smem_phead {
	struct list_head	smem_phead;
	spinlock_t		lock;
	u32			poolnum;
};

struct smem_pool {
	struct list_head	smem_head;
	struct list_head	smem_plist;
	spinlock_t		lock;

	void	*pcie_base;
	u32	addr;
	u32	size;
	u32	smem;
	u32	mem_type;

	atomic_t	used;
	struct gen_pool	*gen;
};

struct smem_record {
	struct list_head smem_list;
	struct task_struct *task;
	u32 size;
	u32 addr;
};

struct smem_map_list {
	struct list_head map_head;
	spinlock_t	lock;
	u32		inited;
};

struct smem_map {
	struct list_head	map_list;
	struct task_struct	*task;
	const void		*mem;
	unsigned int		count;
};

static struct smem_phead	sipc_smem_phead[SIPC_ID_NR];
static struct smem_map_list	mem_mp;

static struct smem_pool *shmem_find_pool(u32 dst, u32 smem)
{
	struct smem_phead *phead;
	struct smem_pool *spool = NULL;
	struct smem_pool *pos;
	unsigned long flags;

	if (dst >= SIPC_ID_NR)
		return NULL;

	phead = &sipc_smem_phead[dst];

	/* The num of one pool is 0, means the poll is not ready */
	if (!phead->poolnum)
		return NULL;

	spin_lock_irqsave(&phead->lock, flags);
	list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
		if (pos->smem == smem) {
			spool = pos;
			break;
		}
	}
	spin_unlock_irqrestore(&phead->lock, flags);
	return spool;
}

static void *soc_modem_ram_vmap(phys_addr_t start, size_t size, E_MMAP_TYPE mtype)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;
	phys_addr_t addr;
	unsigned long flags;
	struct smem_map *map;
	struct smem_map_list *smem = &mem_mp;

	map = kzalloc(sizeof(struct smem_map), GFP_KERNEL);
	if (!map)
		return NULL;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	if (mtype == MMAP_WRITECOMBINE)
		prot = pgprot_writecombine(PAGE_KERNEL);
	else if (mtype == MMAP_NONCACHE)
		prot = pgprot_noncached(PAGE_KERNEL);
	else
		prot = PAGE_KERNEL;

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		kfree(map);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_IOREMAP, prot) + offset_in_page(start);
	kfree(pages);

	map->count = page_count;
	map->mem = vaddr;
	map->task = current;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_add_tail(&map->map_list, &smem->map_head);
		spin_unlock_irqrestore(&smem->lock, flags);
	}
	return vaddr;
}

static void *pcie_modem_ram_vmap(phys_addr_t start, size_t size, E_MMAP_TYPE mtype)
{
	if (mtype == MMAP_CACHE) {
		pr_err("cache not support!\n");
		return NULL;
	}

#ifdef CONFIG_UNISOC_PCIE_EP_DEVICE
	return sprd_ep_map_memory(PCIE_EP_MODEM, start, size);
#endif

#ifdef CONFIG_PCIE_EPF_SPRD
	return sprd_pci_epf_map_memory(SPRD_FUNCTION_0, start, size);
#endif

	return NULL;
}

static void pcie_modem_ram_unmap(const void *mem)
{
#ifdef CONFIG_UNISOC_PCIE_EP_DEVICE
	return sprd_ep_unmap_memory(PCIE_EP_MODEM, mem);
#endif

#ifdef CONFIG_PCIE_EPF_SPRD
	return sprd_pci_epf_unmap_memory(SPRD_FUNCTION_0, mem);
#endif
}

static void soc_modem_ram_unmap(const void *mem)
{
	struct smem_map *map, *next;
	unsigned long flags;
	struct smem_map_list *smem = &mem_mp;
	bool found = false;

	if (smem->inited) {
		spin_lock_irqsave(&smem->lock, flags);
		list_for_each_entry_safe(map, next, &smem->map_head, map_list) {
			if (map->mem == mem) {
				list_del(&map->map_list);
				found = true;
				break;
			}
		}
		spin_unlock_irqrestore(&smem->lock, flags);

		if (found) {
			vunmap(mem - offset_in_page(mem));
			kfree(map);
		}
	}
}

static void *shmem_ram_vmap(u8 dst, u16 smem, phys_addr_t start,
			    size_t size,
			    E_MMAP_TYPE mtype)
{
	struct smem_pool *spool;

	spool = shmem_find_pool(dst, smem);
	if (spool == NULL) {
		pr_err("pool dst %d is not existed!\n", dst);
		return NULL;
	}

	if (spool->mem_type == SMEM_PCIE) {
		if (start < spool->addr
		    || start + size > spool->addr + spool->size) {
			pr_info("error, start = 0x%llx, size = 0x%lx.\n",
				start, size);
			return NULL;
		}

		pr_info("succ, start = 0x%llx, size = 0x%lx.\n",
			start, size);
		return (spool->pcie_base + start - spool->addr);
	}

	return soc_modem_ram_vmap(start, size, mtype);

}

int smem_init(u32 addr, u32 size, u32 dst, u32 smem, u32 mem_type)
{
	struct smem_phead *phead;
	struct smem_map_list *smem_list = &mem_mp;
	struct smem_pool *spool;
	unsigned long flags;

	if (dst >= SIPC_ID_NR)
		return -EINVAL;

	phead = &sipc_smem_phead[dst];
	/* fisrt init, create the pool head */
	if (!phead->poolnum) {
		spin_lock_init(&phead->lock);
		INIT_LIST_HEAD(&phead->smem_phead);
	}

	spool = kzalloc(sizeof(struct smem_pool), GFP_KERNEL);
	if (!spool)
		return -ENOMEM;

	spin_lock_irqsave(&phead->lock, flags);
	list_add_tail(&spool->smem_plist, &phead->smem_phead);
	phead->poolnum++;
	spin_unlock_irqrestore(&phead->lock, flags);

	spool->addr = addr;
	spool->smem = smem;
	spool->mem_type = mem_type;

	if (size >= SMEM_ALIGN_POOLSZ)
		size = PAGE_ALIGN(size);
	else
		size = ALIGN(size, SMEM_ALIGN_BYTES);

	spool->size = size;
	atomic_set(&spool->used, 0);
	spin_lock_init(&spool->lock);
	INIT_LIST_HEAD(&spool->smem_head);

	spin_lock_init(&smem_list->lock);
	INIT_LIST_HEAD(&smem_list->map_head);
	smem_list->inited = 1;

	/* allocator block size is times of pages */
	if (spool->size >= SMEM_ALIGN_POOLSZ)
		spool->gen = gen_pool_create(PAGE_SHIFT, -1);
	else
		spool->gen = gen_pool_create(SMEM_MIN_ORDER, -1);

	if (!spool->gen) {
		kfree(spool);
		pr_err("Failed to create smem gen pool!\n");
		return -ENOMEM;
	}

	if (gen_pool_add(spool->gen, spool->addr, spool->size, -1) != 0) {
		gen_pool_destroy(spool->gen);
		kfree(spool);
		pr_err("Failed to add smem gen pool!\n");
		return -ENOMEM;
	}
	pr_info("pool addr = 0x%x, size = 0x%x added.\n",
		spool->addr, spool->size);

	return 0;
}

/* ****************************************************************** */
u32 smem_alloc_ex(u8 dst, u16 smem, u32 size)

{
	struct smem_pool *spool;
	struct smem_record *recd;
	unsigned long flags;
	u32 addr = 0;

	spool = shmem_find_pool(dst, smem);
	if (spool == NULL) {
		pr_err("pool dst %d is not existed!\n", dst);
		return 0;
	}

	recd = kzalloc(sizeof(struct smem_record), GFP_KERNEL);
	if (!recd)
		return 0;

	if (spool->size >= SMEM_ALIGN_POOLSZ)
		size = PAGE_ALIGN(size);
	else
		size = ALIGN(size, SMEM_ALIGN_BYTES);

	addr = gen_pool_alloc(spool->gen, size);
	if (!addr) {
		pr_err("pool dst=%d, size=0x%x failed to alloc smem!\n",
		       dst, size);
		kfree(recd);
		return 0;
	}

	/* record smem alloc info */
	atomic_add(size, &spool->used);
	recd->size = size;
	recd->task = current;
	recd->addr = addr;
	spin_lock_irqsave(&spool->lock, flags);
	list_add_tail(&recd->smem_list, &spool->smem_head);
	spin_unlock_irqrestore(&spool->lock, flags);

	return addr;
}
EXPORT_SYMBOL_GPL(smem_alloc_ex);

void smem_free_ex(u8 dst, u16 smem, u32 addr, u32 size)
{
	struct smem_pool *spool;
	struct smem_record *recd, *next;
	unsigned long flags;

	spool = shmem_find_pool(dst, smem);
	if (spool == NULL) {
		pr_err("pool dst %d is not existed!\n", dst);
		return;
	}

	if (size >= SMEM_ALIGN_POOLSZ)
		size = PAGE_ALIGN(size);
	else
		size = ALIGN(size, SMEM_ALIGN_BYTES);

	atomic_sub(size, &spool->used);
	gen_pool_free(spool->gen, addr, size);
	/* delete record node from list */
	spin_lock_irqsave(&spool->lock, flags);
	list_for_each_entry_safe(recd, next, &spool->smem_head, smem_list) {
		if (recd->addr == addr) {
			list_del(&recd->smem_list);
			kfree(recd);
			break;
		}
	}
	spin_unlock_irqrestore(&spool->lock, flags);
}
EXPORT_SYMBOL_GPL(smem_free_ex);

void *shmem_ram_vmap_nocache_ex(u8 dst, u16 smem,
				phys_addr_t start, size_t size)
{
	return shmem_ram_vmap(dst, smem, start, size, MMAP_NONCACHE);
}
EXPORT_SYMBOL_GPL(shmem_ram_vmap_nocache_ex);


void *shmem_ram_vmap_cache_ex(u8 dst, u16 smem, phys_addr_t start, size_t size)
{
	return shmem_ram_vmap(dst, smem, start, size, MMAP_CACHE);
}
EXPORT_SYMBOL_GPL(shmem_ram_vmap_cache_ex);

void shmem_ram_unmap_ex(u8 dst, u16 smem, const void *mem)
{
	struct smem_pool *spool;

	spool = shmem_find_pool(dst, smem);
	if (spool == NULL) {
		pr_err("pool dst %d is not existed!\n", dst);
		return;
	}

	if (spool->mem_type == SMEM_PCIE)
		/* do nothing,  because it also do nothing in shmem_ram_vmap */
		return;
	else
		return soc_modem_ram_unmap(mem);
}
EXPORT_SYMBOL_GPL(shmem_ram_unmap_ex);

void *modem_ram_vmap_ex(u32 modem_type, phys_addr_t start, size_t size, E_MMAP_TYPE mtype)
{
	if (modem_type == PCIE_MODEM)
		return pcie_modem_ram_vmap(start, size, mtype);
	else
		return soc_modem_ram_vmap(start, size, mtype);
}
EXPORT_SYMBOL_GPL(modem_ram_vmap_ex);

void *modem_ram_vmap_nocache(u32 modem_type, phys_addr_t start, size_t size)
{
	if (modem_type == PCIE_MODEM)
		return pcie_modem_ram_vmap(start, size, MMAP_NONCACHE);
	else
		return soc_modem_ram_vmap(start, size, MMAP_NONCACHE);
}
EXPORT_SYMBOL_GPL(modem_ram_vmap_nocache);


void *modem_ram_vmap_cache(u32 modem_type, phys_addr_t start, size_t size)
{
	if (modem_type == PCIE_MODEM)
		return pcie_modem_ram_vmap(start, size, MMAP_CACHE);
	else
		return soc_modem_ram_vmap(start, size, MMAP_CACHE);
}
EXPORT_SYMBOL_GPL(modem_ram_vmap_cache);

void modem_ram_unmap(u32 modem_type, const void *mem)
{
	if (modem_type == PCIE_MODEM)
		return pcie_modem_ram_unmap(mem);
	else
		return soc_modem_ram_unmap(mem);
}
EXPORT_SYMBOL_GPL(modem_ram_unmap);

#ifdef CONFIG_DEBUG_FS
static int smem_debug_show(struct seq_file *m, void *private)
{
	struct smem_phead *phead;
	struct smem_pool *spool, *pos;
	struct smem_record *recd;
	struct smsg_ipc *ipc;
	u32 fsize, i;
	unsigned long flags;
	u32 cnt = 0;

	for (i = 0; i < SIPC_ID_NR; i++) {
		phead = &sipc_smem_phead[i];
		ipc = smsg_ipcs[i];
		if (!phead->poolnum || !ipc)
			continue;

		cnt = 0;
		sipc_debug_putline(m, '*', 90);
		seq_printf(m, "dst:%d, name: %s, smem pool list:\n",
			   i, ipc->name);

		spin_lock_irqsave(&phead->lock, flags);
		list_for_each_entry(pos, &phead->smem_phead, smem_plist) {
			spool = pos;
			fsize = gen_pool_avail(spool->gen);
			cnt++;
			sipc_debug_putline(m, '*', 80);
			seq_printf(m, "%d, dst:%d, name: %s, smem pool info:\n",
				   cnt++, i,
				   (smsg_ipcs[i])->name);
			seq_printf(m, "phys_addr=0x%x, total=0x%x, used=0x%x, free=0x%x\n",
				   spool->addr, spool->size,
				   spool->used.counter, fsize);
			seq_puts(m, "smem record list:\n");

			list_for_each_entry(recd,
					    &spool->smem_head, smem_list) {
				seq_printf(m, "task %s: pid=%u, addr=0x%x, size=0x%x\n",
					   recd->task->comm,
					   recd->task->pid,
					   recd->addr,
					   recd->size);
			}
		}
		spin_unlock_irqrestore(&phead->lock, flags);
	}
	return 0;
}

static int smem_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, smem_debug_show, inode->i_private);
}

static const struct file_operations smem_debug_fops = {
	.open = smem_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int smem_init_debug(void)
{
	int rval;
	dev_t dev_no;

	smem_dev = kzalloc(sizeof(struct smem_device), GFP_KERNEL);
	if (!smem_dev)
		return -ENOMEM;
	rval = alloc_chrdev_region(&dev_no, 0, 1, "smem_debug");
	if (rval) {
		pr_err("Failed to alloc chrdev region\n");
		goto free_smem_dev;
	}
	smem_dev->major = MAJOR(dev_no);
	smem_dev->minor = MINOR(dev_no);
	cdev_init(&smem_dev->cdev, &smem_debug_fops);
	rval = cdev_add(&smem_dev->cdev, dev_no, 1);
	if (rval) {
		pr_err("Failed to add smem cdev\n");
		goto free_devno;
	}
	smem_class = class_create(THIS_MODULE, "smem");
	if (IS_ERR(smem_class)) {
		rval = PTR_ERR(smem_class);
		pr_err("Failed to create smem class\n");
		goto free_cdev;
	}
	smem_dev->sys_dev = device_create(smem_class, NULL,
				       dev_no,
				       smem_dev, "sipc_smem");

	return 0;

free_cdev:
	cdev_del(&smem_dev->cdev);

free_devno:
	unregister_chrdev_region(dev_no, 1);

free_smem_dev:
	kfree(smem_dev);
	smem_dev = NULL;
	return rval;
}
EXPORT_SYMBOL_GPL(smem_init_debug);

void smem_destroy_debug(void)
{
	if (smem_dev) {
		dev_t dev_no = MKDEV(smem_dev->major, smem_dev->minor);

		if (smem_dev->sys_dev) {
			device_destroy(smem_class, dev_no);
			smem_dev->sys_dev = NULL;
		}

		if (!IS_ERR(smem_class))
			class_destroy(smem_class);

		unregister_chrdev_region(dev_no, 1);
		cdev_del(&smem_dev->cdev);
		kfree(smem_dev);
		smem_dev = NULL;
	}
}
EXPORT_SYMBOL_GPL(smem_destroy_debug);
#endif /* endof CONFIG_DEBUG_FS */


MODULE_AUTHOR("Wenping Zhou <wenping.zhou@unisoc.com>");
MODULE_DESCRIPTION("SIPC/SMEM driver");
MODULE_LICENSE("GPL v2");
