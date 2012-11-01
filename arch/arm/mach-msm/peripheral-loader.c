/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/elf.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/list_sort.h>

#include <asm/uaccess.h>
#include <asm/setup.h>

#include "peripheral-loader.h"

#define pil_err(desc, fmt, ...)						\
	dev_err(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)
#define pil_info(desc, fmt, ...)					\
	dev_info(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)

/**
 * proxy_timeout - Override for proxy vote timeouts
 * -1: Use driver-specified timeout
 *  0: Hold proxy votes until shutdown
 * >0: Specify a custom timeout in ms
 */
static int proxy_timeout_ms = -1;
module_param(proxy_timeout_ms, int, S_IRUGO | S_IWUSR);

/**
 * struct pil_mdt - Representation of <name>.mdt file in memory
 * @hdr: ELF32 header
 * @phdr: ELF32 program headers
 */
struct pil_mdt {
	struct elf32_hdr hdr;
	struct elf32_phdr phdr[];
};

/**
 * struct pil_seg - memory map representing one segment
 * @next: points to next seg mentor NULL if last segment
 * @paddr: start address of segment
 * @sz: size of segment
 * @filesz: size of segment on disk
 * @num: segment number
 *
 * Loosely based on an elf program header. Contains all necessary information
 * to load and initialize a segment of the image in memory.
 */
struct pil_seg {
	phys_addr_t paddr;
	unsigned long sz;
	unsigned long filesz;
	int num;
	struct list_head list;
};

/**
 * struct pil_priv - Private state for a pil_desc
 * @proxy: work item used to run the proxy unvoting routine
 * @wlock: wakelock to prevent suspend during pil_boot
 * @wname: name of @wlock
 * @desc: pointer to pil_desc this is private data for
 * @seg: list of segments sorted by physical address
 * @entry_addr: physical address where processor starts booting at
 * @region_start: address where relocatable region starts or lowest address
 * for non-relocatable images
 * @region_end: address where relocatable region ends or highest address for
 * non-relocatable images
 *
 * This struct contains data for a pil_desc that should not be exposed outside
 * of this file. This structure points to the descriptor and the descriptor
 * points to this structure so that PIL drivers can't access the private
 * data of a descriptor but this file can access both.
 */
struct pil_priv {
	struct delayed_work proxy;
	struct wake_lock wlock;
	char wname[32];
	struct pil_desc *desc;
	struct list_head segs;
	phys_addr_t entry_addr;
	phys_addr_t region_start;
	phys_addr_t region_end;
};

/**
 * pil_get_entry_addr() - Retrieve the entry address of a peripheral image
 * @desc: descriptor from pil_desc_init()
 *
 * Returns the physical address where the image boots at or 0 if unknown.
 */
phys_addr_t pil_get_entry_addr(struct pil_desc *desc)
{
	return desc->priv ? desc->priv->entry_addr : 0;
}
EXPORT_SYMBOL(pil_get_entry_addr);

static void pil_proxy_work(struct work_struct *work)
{
	struct delayed_work *delayed = to_delayed_work(work);
	struct pil_priv *priv = container_of(delayed, struct pil_priv, proxy);
	struct pil_desc *desc = priv->desc;

	desc->ops->proxy_unvote(desc);
	wake_unlock(&priv->wlock);
	module_put(desc->owner);
}

static int pil_proxy_vote(struct pil_desc *desc)
{
	int ret = 0;
	struct pil_priv *priv = desc->priv;

	if (desc->ops->proxy_vote) {
		wake_lock(&priv->wlock);
		ret = desc->ops->proxy_vote(desc);
		if (ret)
			wake_unlock(&priv->wlock);
	}
	return ret;
}

static void pil_proxy_unvote(struct pil_desc *desc, unsigned long timeout)
{
	struct pil_priv *priv = desc->priv;

	if (proxy_timeout_ms >= 0)
		timeout = proxy_timeout_ms;

	if (timeout && desc->ops->proxy_unvote) {
		if (WARN_ON(!try_module_get(desc->owner)))
			return;
		schedule_delayed_work(&priv->proxy, msecs_to_jiffies(timeout));
	}
}

static struct pil_seg *pil_init_seg(const struct pil_desc *desc,
				  const struct elf32_phdr *phdr, int num)
{
	struct pil_seg *seg;

	if (memblock_overlaps_memory(phdr->p_paddr, phdr->p_memsz)) {
		pil_err(desc, "kernel memory would be overwritten [%#08lx, %#08lx)\n",
			(unsigned long)phdr->p_paddr,
			(unsigned long)(phdr->p_paddr + phdr->p_memsz));
		return ERR_PTR(-EPERM);
	}

	seg = kmalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return ERR_PTR(-ENOMEM);
	seg->num = num;
	seg->paddr = phdr->p_paddr;
	seg->filesz = phdr->p_filesz;
	seg->sz = phdr->p_memsz;
	INIT_LIST_HEAD(&seg->list);

	return seg;
}

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type == PT_LOAD) && !segment_is_hash(p->p_flags);
}

static void pil_dump_segs(const struct pil_priv *priv)
{
	struct pil_seg *seg;

	list_for_each_entry(seg, &priv->segs, list) {
		pil_info(priv->desc, "%d: %#08zx %#08lx\n", seg->num,
				seg->paddr, seg->paddr + seg->sz);
	}
}

/*
 * Ensure the entry address lies within the image limits.
 */
static int pil_init_entry_addr(struct pil_priv *priv, const struct pil_mdt *mdt)
{
	struct pil_seg *seg;

	priv->entry_addr = mdt->hdr.e_entry;

	if (priv->desc->flags & PIL_SKIP_ENTRY_CHECK)
		return 0;

	list_for_each_entry(seg, &priv->segs, list) {
		if (priv->entry_addr >= seg->paddr &&
		    priv->entry_addr < seg->paddr + seg->sz)
			return 0;
	}
	pil_err(priv->desc, "boot address %08zx not within range\n",
		priv->entry_addr);
	pil_dump_segs(priv);
	return -EADDRNOTAVAIL;
}

static int pil_setup_region(struct pil_priv *priv, const struct pil_mdt *mdt)
{
	const struct elf32_phdr *phdr;
	phys_addr_t min_addr, max_addr;
	int i;

	min_addr = (phys_addr_t)ULLONG_MAX;
	max_addr = 0;

	/* Find the image limits */
	for (i = 0; i < mdt->hdr.e_phnum; i++) {
		phdr = &mdt->phdr[i];
		if (!segment_is_loadable(phdr))
			continue;

		min_addr = min(min_addr, phdr->p_paddr);
		max_addr = max(max_addr, phdr->p_paddr + phdr->p_memsz);
	}

	priv->region_start = min_addr;
	priv->region_end = max_addr;

	return 0;
}

static int pil_cmp_seg(void *priv, struct list_head *a, struct list_head *b)
{
	struct pil_seg *seg_a = list_entry(a, struct pil_seg, list);
	struct pil_seg *seg_b = list_entry(b, struct pil_seg, list);

	return seg_a->paddr - seg_b->paddr;
}

static int pil_init_mmap(struct pil_desc *desc, const struct pil_mdt *mdt)
{
	struct pil_priv *priv = desc->priv;
	const struct elf32_phdr *phdr;
	struct pil_seg *seg;
	int i, ret;

	ret = pil_setup_region(priv, mdt);
	if (ret)
		return ret;

	for (i = 0; i < mdt->hdr.e_phnum; i++) {
		phdr = &mdt->phdr[i];
		if (!segment_is_loadable(phdr))
			continue;

		seg = pil_init_seg(desc, phdr, i);
		if (IS_ERR(seg))
			return PTR_ERR(seg);

		list_add_tail(&seg->list, &priv->segs);
	}
	list_sort(NULL, &priv->segs, pil_cmp_seg);

	return pil_init_entry_addr(priv, mdt);
}

static void pil_release_mmap(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;
	struct pil_seg *p, *tmp;

	list_for_each_entry_safe(p, tmp, &priv->segs, list) {
		list_del(&p->list);
		kfree(p);
	}
}

#define IOMAP_SIZE SZ_4M

static int pil_load_seg(struct pil_desc *desc, struct pil_seg *seg)
{
	int ret = 0, count, paddr;
	char fw_name[30];
	const struct firmware *fw = NULL;
	const u8 *data;
	int num = seg->num;

	if (seg->filesz) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d",
				desc->name, num);
		ret = request_firmware(&fw, fw_name, desc->dev);
		if (ret) {
			pil_err(desc, "Failed to locate blob %s\n", fw_name);
			return ret;
		}

		if (fw->size != seg->filesz) {
			pil_err(desc, "Blob size %u doesn't match %lu\n",
					fw->size, seg->filesz);
			ret = -EPERM;
			goto release_fw;
		}
	}

	/* Load the segment into memory */
	count = seg->filesz;
	paddr = seg->paddr;
	data = fw ? fw->data : NULL;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			pil_err(desc, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memcpy(buf, data, size);
		iounmap(buf);

		count -= size;
		paddr += size;
		data += size;
	}

	/* Zero out trailing memory */
	count = seg->sz - seg->filesz;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			pil_err(desc, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memset(buf, 0, size);
		iounmap(buf);

		count -= size;
		paddr += size;
	}

	if (desc->ops->verify_blob) {
		ret = desc->ops->verify_blob(desc, seg->paddr, seg->sz);
		if (ret)
			pil_err(desc, "Blob%u failed verification\n", num);
	}

release_fw:
	release_firmware(fw);
	return ret;
}

/* Synchronize request_firmware() with suspend */
static DECLARE_RWSEM(pil_pm_rwsem);

/**
 * pil_boot() - Load a peripheral image into memory and boot it
 * @desc: descriptor from pil_desc_init()
 *
 * Returns 0 on success or -ERROR on failure.
 */
int pil_boot(struct pil_desc *desc)
{
	int ret;
	char fw_name[30];
	const struct pil_mdt *mdt;
	const struct elf32_hdr *ehdr;
	struct pil_seg *seg;
	const struct firmware *fw;
	unsigned long proxy_timeout = desc->proxy_timeout;
	struct pil_priv *priv = desc->priv;

	/* Reinitialize for new image */
	pil_release_mmap(desc);

	down_read(&pil_pm_rwsem);
	snprintf(fw_name, sizeof(fw_name), "%s.mdt", desc->name);
	ret = request_firmware(&fw, fw_name, desc->dev);
	if (ret) {
		pil_err(desc, "Failed to locate %s\n", fw_name);
		goto out;
	}

	if (fw->size < sizeof(*ehdr)) {
		pil_err(desc, "Not big enough to be an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	mdt = (const struct pil_mdt *)fw->data;
	ehdr = &mdt->hdr;

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		pil_err(desc, "Not an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	if (ehdr->e_phnum == 0) {
		pil_err(desc, "No loadable segments\n");
		ret = -EIO;
		goto release_fw;
	}
	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw->size) {
		pil_err(desc, "Program headers not within mdt\n");
		ret = -EIO;
		goto release_fw;
	}

	ret = pil_init_mmap(desc, mdt);
	if (ret)
		goto release_fw;

	if (desc->ops->init_image)
		ret = desc->ops->init_image(desc, fw->data, fw->size);
	if (ret) {
		pil_err(desc, "Invalid firmware metadata\n");
		goto release_fw;
	}

	if (desc->ops->mem_setup)
		ret = desc->ops->mem_setup(desc, priv->region_start,
				priv->region_end - priv->region_start);
	if (ret) {
		pil_err(desc, "Memory setup error\n");
		goto release_fw;
	}

	list_for_each_entry(seg, &desc->priv->segs, list) {
		ret = pil_load_seg(desc, seg);
		if (ret)
			goto release_fw;
	}

	ret = pil_proxy_vote(desc);
	if (ret) {
		pil_err(desc, "Failed to proxy vote\n");
		goto release_fw;
	}

	ret = desc->ops->auth_and_reset(desc);
	if (ret) {
		pil_err(desc, "Failed to bring out of reset\n");
		proxy_timeout = 0; /* Remove proxy vote immediately on error */
		goto err_boot;
	}
	pil_info(desc, "Brought out of reset\n");
err_boot:
	pil_proxy_unvote(desc, proxy_timeout);
release_fw:
	release_firmware(fw);
out:
	up_read(&pil_pm_rwsem);
	if (ret)
		pil_release_mmap(desc);
	return ret;
}
EXPORT_SYMBOL(pil_boot);

/**
 * pil_shutdown() - Shutdown a peripheral
 * @desc: descriptor from pil_desc_init()
 */
void pil_shutdown(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;
	desc->ops->shutdown(desc);
	if (proxy_timeout_ms == 0 && desc->ops->proxy_unvote)
		desc->ops->proxy_unvote(desc);
	else
		flush_delayed_work(&priv->proxy);
}
EXPORT_SYMBOL(pil_shutdown);

/**
 * pil_desc_init() - Initialize a pil descriptor
 * @desc: descriptor to intialize
 *
 * Initialize a pil descriptor for use by other pil functions. This function
 * must be called before calling pil_boot() or pil_shutdown().
 *
 * Returns 0 for success and -ERROR on failure.
 */
int pil_desc_init(struct pil_desc *desc)
{
	struct pil_priv *priv;

	/* Ignore users who don't make any sense */
	WARN(desc->ops->proxy_unvote && !desc->proxy_timeout,
			"A proxy timeout of 0 was specified.\n");
	if (WARN(desc->ops->proxy_unvote && !desc->ops->proxy_vote,
				"Invalid proxy voting. Ignoring\n"))
		((struct pil_reset_ops *)desc->ops)->proxy_unvote = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	desc->priv = priv;
	priv->desc = desc;

	snprintf(priv->wname, sizeof(priv->wname), "pil-%s", desc->name);
	wake_lock_init(&priv->wlock, WAKE_LOCK_SUSPEND, priv->wname);
	INIT_DELAYED_WORK(&priv->proxy, pil_proxy_work);
	INIT_LIST_HEAD(&priv->segs);

	return 0;
}
EXPORT_SYMBOL(pil_desc_init);

/**
 * pil_desc_release() - Release a pil descriptor
 * @desc: descriptor to free
 */
void pil_desc_release(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;

	if (priv) {
		flush_delayed_work(&priv->proxy);
		wake_lock_destroy(&priv->wlock);
	}
	desc->priv = NULL;
	kfree(priv);
}
EXPORT_SYMBOL(pil_desc_release);

static int pil_pm_notify(struct notifier_block *b, unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&pil_pm_rwsem);
		break;
	case PM_POST_SUSPEND:
		up_write(&pil_pm_rwsem);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block pil_pm_notifier = {
	.notifier_call = pil_pm_notify,
};

static int __init msm_pil_init(void)
{
	return register_pm_notifier(&pil_pm_notifier);
}
subsys_initcall(msm_pil_init);

static void __exit msm_pil_exit(void)
{
	unregister_pm_notifier(&pil_pm_notifier);
}
module_exit(msm_pil_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Load peripheral images and bring peripherals out of reset");
