/* Copyright (c) 2010-2017, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/secure_buffer.h>

#include <asm/uaccess.h>
#include <asm/setup.h>
#include <asm-generic/io-64-nonatomic-lo-hi.h>

#include "peripheral-loader.h"

#define pil_err(desc, fmt, ...)						\
	dev_err(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)
#define pil_info(desc, fmt, ...)					\
	dev_info(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)

#if defined(CONFIG_ARM)
#define pil_memset_io(d, c, count) memset(d, c, count)
#else
#define pil_memset_io(d, c, count) memset_io(d, c, count)
#endif

#define PIL_NUM_DESC		10
static void __iomem *pil_info_base;

/**
 * proxy_timeout - Override for proxy vote timeouts
 * -1: Use driver-specified timeout
 *  0: Hold proxy votes until shutdown
 * >0: Specify a custom timeout in ms
 */
static int proxy_timeout_ms = -1;
module_param(proxy_timeout_ms, int, S_IRUGO | S_IWUSR);

static bool disable_timeouts;
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
 * @paddr: physical start address of segment
 * @sz: size of segment
 * @filesz: size of segment on disk
 * @num: segment number
 * @relocated: true if segment is relocated, false otherwise
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
	bool relocated;
};

/**
 * struct pil_priv - Private state for a pil_desc
 * @proxy: work item used to run the proxy unvoting routine
 * @ws: wakeup source to prevent suspend during pil_boot
 * @wname: name of @ws
 * @desc: pointer to pil_desc this is private data for
 * @seg: list of segments sorted by physical address
 * @entry_addr: physical address where processor starts booting at
 * @base_addr: smallest start address among all segments that are relocatable
 * @region_start: address where relocatable region starts or lowest address
 * for non-relocatable images
 * @region_end: address where relocatable region ends or highest address for
 * non-relocatable images
 * @region: region allocated for relocatable images
 * @unvoted_flag: flag to keep track if we have unvoted or not.
 *
 * This struct contains data for a pil_desc that should not be exposed outside
 * of this file. This structure points to the descriptor and the descriptor
 * points to this structure so that PIL drivers can't access the private
 * data of a descriptor but this file can access both.
 */
struct pil_priv {
	struct delayed_work proxy;
	struct wakeup_source ws;
	char wname[32];
	struct pil_desc *desc;
	struct list_head segs;
	phys_addr_t entry_addr;
	phys_addr_t base_addr;
	phys_addr_t region_start;
	phys_addr_t region_end;
	void *region;
	struct pil_image_info __iomem *info;
	int id;
	int unvoted_flag;
	size_t region_size;
};

/**
 * pil_do_ramdump() - Ramdump an image
 * @desc: descriptor from pil_desc_init()
 * @ramdump_dev: ramdump device returned from create_ramdump_device()
 *
 * Calls the ramdump API with a list of segments generated from the addresses
 * that the descriptor corresponds to.
 */
int pil_do_ramdump(struct pil_desc *desc, void *ramdump_dev)
{
	struct pil_priv *priv = desc->priv;
	struct pil_seg *seg;
	int count = 0, ret;
	struct ramdump_segment *ramdump_segs, *s;

	list_for_each_entry(seg, &priv->segs, list)
		count++;

	ramdump_segs = kcalloc(count, sizeof(*ramdump_segs), GFP_KERNEL);
	if (!ramdump_segs)
		return -ENOMEM;

	if (desc->subsys_vmid > 0)
		ret = pil_assign_mem_to_linux(desc, priv->region_start,
				(priv->region_end - priv->region_start));

	s = ramdump_segs;
	list_for_each_entry(seg, &priv->segs, list) {
		s->address = seg->paddr;
		s->size = seg->sz;
		s++;
	}

	ret = do_elf_ramdump(ramdump_dev, ramdump_segs, count);
	kfree(ramdump_segs);

	if (!ret && desc->subsys_vmid > 0)
		ret = pil_assign_mem_to_subsys(desc, priv->region_start,
				(priv->region_end - priv->region_start));

	return ret;
}
EXPORT_SYMBOL(pil_do_ramdump);

int pil_assign_mem_to_subsys(struct pil_desc *desc, phys_addr_t addr,
							size_t size)
{
	int ret;
	int srcVM[1] = {VMID_HLOS};
	int destVM[1] = {desc->subsys_vmid};
	int destVMperm[1] = {PERM_READ | PERM_WRITE};

	ret = hyp_assign_phys(addr, size, srcVM, 1, destVM, destVMperm, 1);
	if (ret)
		pil_err(desc, "%s: failed for %pa address of size %zx - subsys VMid %d\n",
				__func__, &addr, size, desc->subsys_vmid);
	return ret;
}
EXPORT_SYMBOL(pil_assign_mem_to_subsys);

int pil_assign_mem_to_linux(struct pil_desc *desc, phys_addr_t addr,
							size_t size)
{
	int ret;
	int srcVM[1] = {desc->subsys_vmid};
	int destVM[1] = {VMID_HLOS};
	int destVMperm[1] = {PERM_READ | PERM_WRITE | PERM_EXEC};

	ret = hyp_assign_phys(addr, size, srcVM, 1, destVM, destVMperm, 1);
	if (ret)
		panic("%s: failed for %pa address of size %zx - subsys VMid %d. Fatal error.\n",
				__func__, &addr, size, desc->subsys_vmid);

	return ret;
}
EXPORT_SYMBOL(pil_assign_mem_to_linux);

int pil_assign_mem_to_subsys_and_linux(struct pil_desc *desc,
						phys_addr_t addr, size_t size)
{
	int ret;
	int srcVM[1] = {VMID_HLOS};
	int destVM[2] = {VMID_HLOS, desc->subsys_vmid};
	int destVMperm[2] = {PERM_READ | PERM_WRITE, PERM_READ | PERM_WRITE};

	ret = hyp_assign_phys(addr, size, srcVM, 1, destVM, destVMperm, 2);
	if (ret)
		pil_err(desc, "%s: failed for %pa address of size %zx - subsys VMid %d\n",
				__func__, &addr, size, desc->subsys_vmid);

	return ret;
}
EXPORT_SYMBOL(pil_assign_mem_to_subsys_and_linux);

int pil_reclaim_mem(struct pil_desc *desc, phys_addr_t addr, size_t size,
						int VMid)
{
	int ret;
	int srcVM[2] = {VMID_HLOS, desc->subsys_vmid};
	int destVM[1] = {VMid};
	int destVMperm[1] = {PERM_READ | PERM_WRITE};

	if (VMid == VMID_HLOS)
		destVMperm[0] = PERM_READ | PERM_WRITE | PERM_EXEC;

	ret = hyp_assign_phys(addr, size, srcVM, 2, destVM, destVMperm, 1);
	if (ret)
		panic("%s: failed for %pa address of size %zx - subsys VMid %d. Fatal error.\n",
				__func__, &addr, size, desc->subsys_vmid);

	return ret;
}
EXPORT_SYMBOL(pil_reclaim_mem);

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

static void __pil_proxy_unvote(struct pil_priv *priv)
{
	struct pil_desc *desc = priv->desc;

	desc->ops->proxy_unvote(desc);
	notify_proxy_unvote(desc->dev);
	__pm_relax(&priv->ws);
	module_put(desc->owner);

}

static void pil_proxy_unvote_work(struct work_struct *work)
{
	struct delayed_work *delayed = to_delayed_work(work);
	struct pil_priv *priv = container_of(delayed, struct pil_priv, proxy);
	__pil_proxy_unvote(priv);
}

static int pil_proxy_vote(struct pil_desc *desc)
{
	int ret = 0;
	struct pil_priv *priv = desc->priv;

	if (desc->ops->proxy_vote) {
		__pm_stay_awake(&priv->ws);
		ret = desc->ops->proxy_vote(desc);
		if (ret)
			__pm_relax(&priv->ws);
	}

	if (desc->proxy_unvote_irq)
		enable_irq(desc->proxy_unvote_irq);
	notify_proxy_vote(desc->dev);

	return ret;
}

static void pil_proxy_unvote(struct pil_desc *desc, int immediate)
{
	struct pil_priv *priv = desc->priv;
	unsigned long timeout;

	if (proxy_timeout_ms == 0 && !immediate)
		return;
	else if (proxy_timeout_ms > 0)
		timeout = proxy_timeout_ms;
	else
		timeout = desc->proxy_timeout;

	if (desc->ops->proxy_unvote) {
		if (WARN_ON(!try_module_get(desc->owner)))
			return;

		if (immediate)
			timeout = 0;

		if (!desc->proxy_unvote_irq || immediate)
			schedule_delayed_work(&priv->proxy,
					      msecs_to_jiffies(timeout));
	}
}

static irqreturn_t proxy_unvote_intr_handler(int irq, void *dev_id)
{
	struct pil_desc *desc = dev_id;
	struct pil_priv *priv = desc->priv;

	pil_info(desc, "Power/Clock ready interrupt received\n");
	if (!desc->priv->unvoted_flag) {
		desc->priv->unvoted_flag = 1;
		__pil_proxy_unvote(priv);
	}

	return IRQ_HANDLED;
}

static bool segment_is_relocatable(const struct elf32_phdr *p)
{
	return !!(p->p_flags & BIT(27));
}

static phys_addr_t pil_reloc(const struct pil_priv *priv, phys_addr_t addr)
{
	return addr - priv->base_addr + priv->region_start;
}

static struct pil_seg *pil_init_seg(const struct pil_desc *desc,
				  const struct elf32_phdr *phdr, int num)
{
	bool reloc = segment_is_relocatable(phdr);
	const struct pil_priv *priv = desc->priv;
	struct pil_seg *seg;

	if (!reloc && memblock_overlaps_memory(phdr->p_paddr, phdr->p_memsz)) {
		pil_err(desc, "kernel memory would be overwritten [%#08lx, %#08lx)\n",
			(unsigned long)phdr->p_paddr,
			(unsigned long)(phdr->p_paddr + phdr->p_memsz));
		return ERR_PTR(-EPERM);
	}

	if (phdr->p_filesz > phdr->p_memsz) {
		pil_err(desc, "Segment %d: file size (%u) is greater than mem size (%u).\n",
			num, phdr->p_filesz, phdr->p_memsz);
		return ERR_PTR(-EINVAL);
	}

	seg = kmalloc(sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return ERR_PTR(-ENOMEM);
	seg->num = num;
	seg->paddr = reloc ? pil_reloc(priv, phdr->p_paddr) : phdr->p_paddr;
	seg->filesz = phdr->p_filesz;
	seg->sz = phdr->p_memsz;
	seg->relocated = reloc;
	INIT_LIST_HEAD(&seg->list);

	return seg;
}

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type == PT_LOAD) && !segment_is_hash(p->p_flags) &&
		p->p_memsz;
}

static void pil_dump_segs(const struct pil_priv *priv)
{
	struct pil_seg *seg;
	phys_addr_t seg_h_paddr;

	list_for_each_entry(seg, &priv->segs, list) {
		seg_h_paddr = seg->paddr + seg->sz;
		pil_info(priv->desc, "%d: %pa %pa\n", seg->num,
				&seg->paddr, &seg_h_paddr);
	}
}

/*
 * Ensure the entry address lies within the image limits and if the image is
 * relocatable ensure it lies within a relocatable segment.
 */
static int pil_init_entry_addr(struct pil_priv *priv, const struct pil_mdt *mdt)
{
	struct pil_seg *seg;
	phys_addr_t entry = mdt->hdr.e_entry;
	bool image_relocated = priv->region;

	if (image_relocated)
		entry = pil_reloc(priv, entry);
	priv->entry_addr = entry;

	if (priv->desc->flags & PIL_SKIP_ENTRY_CHECK)
		return 0;

	list_for_each_entry(seg, &priv->segs, list) {
		if (entry >= seg->paddr && entry < seg->paddr + seg->sz) {
			if (!image_relocated)
				return 0;
			else if (seg->relocated)
				return 0;
		}
	}
	pil_err(priv->desc, "entry address %pa not within range\n", &entry);
	pil_dump_segs(priv);
	return -EADDRNOTAVAIL;
}

static int pil_alloc_region(struct pil_priv *priv, phys_addr_t min_addr,
				phys_addr_t max_addr, size_t align)
{
	void *region;
	size_t size = max_addr - min_addr;
	size_t aligned_size;

	/* Don't reallocate due to fragmentation concerns, just sanity check */
	if (priv->region) {
		if (WARN(priv->region_end - priv->region_start < size,
			"Can't reuse PIL memory, too small\n"))
			return -ENOMEM;
		return 0;
	}

	if (align > SZ_4M)
		aligned_size = ALIGN(size, SZ_4M);
	else
		aligned_size = ALIGN(size, SZ_1M);

	init_dma_attrs(&priv->desc->attrs);
	dma_set_attr(DMA_ATTR_SKIP_ZEROING, &priv->desc->attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &priv->desc->attrs);

	region = dma_alloc_attrs(priv->desc->dev, aligned_size,
				&priv->region_start, GFP_KERNEL,
				&priv->desc->attrs);

	if (region == NULL) {
		pil_err(priv->desc, "Failed to allocate relocatable region of size %zx\n",
					size);
		priv->region_start = 0;
		priv->region_end = 0;
		return -ENOMEM;
	}

	priv->region = region;
	priv->region_end = priv->region_start + size;
	priv->base_addr = min_addr;
	priv->region_size = aligned_size;

	return 0;
}

static int pil_setup_region(struct pil_priv *priv, const struct pil_mdt *mdt)
{
	const struct elf32_phdr *phdr;
	phys_addr_t min_addr_r, min_addr_n, max_addr_r, max_addr_n, start, end;
	size_t align = 0;
	int i, ret = 0;
	bool relocatable = false;

	min_addr_n = min_addr_r = (phys_addr_t)ULLONG_MAX;
	max_addr_n = max_addr_r = 0;

	/* Find the image limits */
	for (i = 0; i < mdt->hdr.e_phnum; i++) {
		phdr = &mdt->phdr[i];
		if (!segment_is_loadable(phdr))
			continue;

		start = phdr->p_paddr;
		end = start + phdr->p_memsz;

		if (segment_is_relocatable(phdr)) {
			min_addr_r = min(min_addr_r, start);
			max_addr_r = max(max_addr_r, end);
			/*
			 * Lowest relocatable segment dictates alignment of
			 * relocatable region
			 */
			if (min_addr_r == start)
				align = phdr->p_align;
			relocatable = true;
		} else {
			min_addr_n = min(min_addr_n, start);
			max_addr_n = max(max_addr_n, end);
		}

	}

	/*
	 * Align the max address to the next 4K boundary to satisfy iommus and
	 * XPUs that operate on 4K chunks.
	 */
	max_addr_n = ALIGN(max_addr_n, SZ_4K);
	max_addr_r = ALIGN(max_addr_r, SZ_4K);

	if (relocatable) {
		ret = pil_alloc_region(priv, min_addr_r, max_addr_r, align);
	} else {
		priv->region_start = min_addr_n;
		priv->region_end = max_addr_n;
		priv->base_addr = min_addr_n;
	}

	if (priv->info) {
		__iowrite32_copy(&priv->info->start, &priv->region_start,
					sizeof(priv->region_start) / 4);
		writel_relaxed(priv->region_end - priv->region_start,
				&priv->info->size);
	}

	return ret;
}

static int pil_cmp_seg(void *priv, struct list_head *a, struct list_head *b)
{
	int ret = 0;
	struct pil_seg *seg_a = list_entry(a, struct pil_seg, list);
	struct pil_seg *seg_b = list_entry(b, struct pil_seg, list);

	if (seg_a->paddr < seg_b->paddr)
		ret = -1;
	else if (seg_a->paddr > seg_b->paddr)
		ret = 1;

	return ret;
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


	pil_info(desc, "loading from %pa to %pa\n", &priv->region_start,
							&priv->region_end);

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

struct pil_map_fw_info {
	void *region;
	struct dma_attrs attrs;
	phys_addr_t base_addr;
	struct device *dev;
};

static void pil_release_mmap(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;
	struct pil_seg *p, *tmp;
	u64 zero = 0ULL;

	if (priv->info) {
		__iowrite32_copy(&priv->info->start, &zero,
					sizeof(zero) / 4);
		writel_relaxed(0, &priv->info->size);
	}

	list_for_each_entry_safe(p, tmp, &priv->segs, list) {
		list_del(&p->list);
		kfree(p);
	}
}

static void pil_clear_segment(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;
	u8 __iomem *buf;

	struct pil_map_fw_info map_fw_info = {
		.attrs = desc->attrs,
		.region = priv->region,
		.base_addr = priv->region_start,
		.dev = desc->dev,
	};

	void *map_data = desc->map_data ? desc->map_data : &map_fw_info;

	/* Clear memory so that unauthorized ELF code is not left behind */
	buf = desc->map_fw_mem(priv->region_start, (priv->region_end -
					priv->region_start), map_data);
	pil_memset_io(buf, 0, (priv->region_end - priv->region_start));
	desc->unmap_fw_mem(buf, (priv->region_end - priv->region_start),
								map_data);

}

#define IOMAP_SIZE SZ_1M

static void *map_fw_mem(phys_addr_t paddr, size_t size, void *data)
{
	struct pil_map_fw_info *info = data;

	return dma_remap(info->dev, info->region, paddr, size,
					&info->attrs);
}

static void unmap_fw_mem(void *vaddr, size_t size, void *data)
{
	struct pil_map_fw_info *info = data;

	dma_unremap(info->dev, vaddr, size);
}

static int pil_load_seg(struct pil_desc *desc, struct pil_seg *seg)
{
	int ret = 0, count;
	phys_addr_t paddr;
	char fw_name[30];
	int num = seg->num;
	struct pil_map_fw_info map_fw_info = {
		.attrs = desc->attrs,
		.region = desc->priv->region,
		.base_addr = desc->priv->region_start,
		.dev = desc->dev,
	};
	void *map_data = desc->map_data ? desc->map_data : &map_fw_info;

	if (seg->filesz) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d",
				desc->fw_name, num);
		ret = request_firmware_into_buf(fw_name, desc->dev, seg->paddr,
					      seg->filesz, desc->map_fw_mem,
					      desc->unmap_fw_mem, map_data);
		if (ret < 0) {
			pil_err(desc, "Failed to locate blob %s or blob is too big.\n",
				fw_name);
			return ret;
		}

		if (ret != seg->filesz) {
			pil_err(desc, "Blob size %u doesn't match %lu\n",
					ret, seg->filesz);
			return -EPERM;
		}
		ret = 0;
	}

	/* Zero out trailing memory */
	paddr = seg->paddr + seg->filesz;
	count = seg->sz - seg->filesz;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = desc->map_fw_mem(paddr, size, map_data);
		if (!buf) {
			pil_err(desc, "Failed to map memory\n");
			return -ENOMEM;
		}
		pil_memset_io(buf, 0, size);

		desc->unmap_fw_mem(buf, size, map_data);

		count -= size;
		paddr += size;
	}

	if (desc->ops->verify_blob) {
		ret = desc->ops->verify_blob(desc, seg->paddr, seg->sz);
		if (ret)
			pil_err(desc, "Blob%u failed verification\n", num);
	}

	return ret;
}

static int pil_parse_devicetree(struct pil_desc *desc)
{
	struct device_node *ofnode = desc->dev->of_node;
	int clk_ready = 0;

	if (!ofnode)
		return -EINVAL;

	if (of_property_read_u32(ofnode, "qcom,mem-protect-id",
					&desc->subsys_vmid))
		pr_debug("Unable to read the addr-protect-id for %s\n",
					desc->name);

	if (desc->ops->proxy_unvote && of_find_property(ofnode,
					"qcom,gpio-proxy-unvote",
					NULL)) {
		clk_ready = of_get_named_gpio(ofnode,
				"qcom,gpio-proxy-unvote", 0);

		if (clk_ready < 0) {
			dev_dbg(desc->dev,
				"[%s]: Error getting proxy unvoting gpio\n",
				desc->name);
			return clk_ready;
		}

		clk_ready = gpio_to_irq(clk_ready);
		if (clk_ready < 0) {
			dev_err(desc->dev,
				"[%s]: Error getting proxy unvote IRQ\n",
				desc->name);
			return clk_ready;
		}
	}
	desc->proxy_unvote_irq = clk_ready;
	return 0;
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
	struct pil_priv *priv = desc->priv;
	bool mem_protect = false;
	bool hyp_assign = false;

	if (desc->shutdown_fail)
		pil_err(desc, "Subsystem shutdown failed previously!\n");

	/* Reinitialize for new image */
	pil_release_mmap(desc);

	down_read(&pil_pm_rwsem);
	snprintf(fw_name, sizeof(fw_name), "%s.mdt", desc->fw_name);
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

	desc->priv->unvoted_flag = 0;
	ret = pil_proxy_vote(desc);
	if (ret) {
		pil_err(desc, "Failed to proxy vote\n");
		goto release_fw;
	}

	if (desc->ops->init_image)
		ret = desc->ops->init_image(desc, fw->data, fw->size);
	if (ret) {
		pil_err(desc, "Invalid firmware metadata\n");
		goto err_boot;
	}

	if (desc->ops->mem_setup)
		ret = desc->ops->mem_setup(desc, priv->region_start,
				priv->region_end - priv->region_start);
	if (ret) {
		pil_err(desc, "Memory setup error\n");
		goto err_deinit_image;
	}

	if (desc->subsys_vmid > 0) {
		/* In case of modem ssr, we need to assign memory back to linux.
		 * This is not true after cold boot since linux already owns it.
		 * Also for secure boot devices, modem memory has to be released
		 * after MBA is booted. */
		if (desc->modem_ssr) {
			ret = pil_assign_mem_to_linux(desc, priv->region_start,
				(priv->region_end - priv->region_start));
			if (ret)
				pil_err(desc, "Failed to assign to linux, ret- %d\n",
								ret);
		}
		ret = pil_assign_mem_to_subsys_and_linux(desc,
				priv->region_start,
				(priv->region_end - priv->region_start));
		if (ret) {
			pil_err(desc, "Failed to assign memory, ret - %d\n",
								ret);
			goto err_deinit_image;
		}
		hyp_assign = true;
	}

	list_for_each_entry(seg, &desc->priv->segs, list) {
		ret = pil_load_seg(desc, seg);
		if (ret)
			goto err_deinit_image;
	}

	if (desc->subsys_vmid > 0) {
		ret =  pil_reclaim_mem(desc, priv->region_start,
				(priv->region_end - priv->region_start),
				desc->subsys_vmid);
		if (ret) {
			pil_err(desc, "Failed to assign %s memory, ret - %d\n",
							desc->name, ret);
			goto err_deinit_image;
		}
		hyp_assign = false;
	}

	ret = desc->ops->auth_and_reset(desc);
	if (ret) {
		pil_err(desc, "Failed to bring out of reset\n");
		goto err_auth_and_reset;
	}
	pil_info(desc, "Brought out of reset\n");
	desc->modem_ssr = false;
err_auth_and_reset:
	if (ret && desc->subsys_vmid > 0) {
		pil_assign_mem_to_linux(desc, priv->region_start,
				(priv->region_end - priv->region_start));
		mem_protect = true;
	}
err_deinit_image:
	if (ret && desc->ops->deinit_image)
		desc->ops->deinit_image(desc);
err_boot:
	if (ret && desc->proxy_unvote_irq)
		disable_irq(desc->proxy_unvote_irq);
	pil_proxy_unvote(desc, ret);
release_fw:
	release_firmware(fw);
out:
	up_read(&pil_pm_rwsem);
	if (ret) {
		if (priv->region) {
			if (desc->subsys_vmid > 0 && !mem_protect &&
					hyp_assign) {
				pil_reclaim_mem(desc, priv->region_start,
					(priv->region_end -
						priv->region_start),
					VMID_HLOS);
			}
			dma_free_attrs(desc->dev, priv->region_size,
					priv->region, priv->region_start,
					&desc->attrs);
			priv->region = NULL;
		}
		if (desc->clear_fw_region && priv->region_start)
			pil_clear_segment(desc);
		pil_release_mmap(desc);
	}
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

	if (desc->ops->shutdown) {
		if (desc->ops->shutdown(desc))
			desc->shutdown_fail = true;
		else
			desc->shutdown_fail = false;
	}

	if (desc->proxy_unvote_irq) {
		disable_irq(desc->proxy_unvote_irq);
		if (!desc->priv->unvoted_flag)
			pil_proxy_unvote(desc, 1);
	} else if (!proxy_timeout_ms)
		pil_proxy_unvote(desc, 1);
	else
		flush_delayed_work(&priv->proxy);
	desc->modem_ssr = true;
}
EXPORT_SYMBOL(pil_shutdown);

/**
 * pil_free_memory() - Free memory resources associated with a peripheral
 * @desc: descriptor from pil_desc_init()
 */
void pil_free_memory(struct pil_desc *desc)
{
	struct pil_priv *priv = desc->priv;

	if (priv->region) {
		if (desc->subsys_vmid > 0)
			pil_assign_mem_to_linux(desc, priv->region_start,
				(priv->region_end - priv->region_start));
		dma_free_attrs(desc->dev, priv->region_size,
				priv->region, priv->region_start, &desc->attrs);
		priv->region = NULL;
	}
}
EXPORT_SYMBOL(pil_free_memory);

static DEFINE_IDA(pil_ida);

bool is_timeout_disabled(void)
{
	return disable_timeouts;
}
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
	int ret;
	void __iomem *addr;
	char buf[sizeof(priv->info->name)];

	if (WARN(desc->ops->proxy_unvote && !desc->ops->proxy_vote,
				"Invalid proxy voting. Ignoring\n"))
		((struct pil_reset_ops *)desc->ops)->proxy_unvote = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	desc->priv = priv;
	priv->desc = desc;

	priv->id = ret = ida_simple_get(&pil_ida, 0, PIL_NUM_DESC, GFP_KERNEL);
	if (priv->id < 0)
		goto err;

	if (pil_info_base) {
		addr = pil_info_base + sizeof(struct pil_image_info) * priv->id;
		priv->info = (struct pil_image_info __iomem *)addr;

		strncpy(buf, desc->name, sizeof(buf));
		__iowrite32_copy(priv->info->name, buf, sizeof(buf) / 4);
	}

	ret = pil_parse_devicetree(desc);
	if (ret)
		goto err_parse_dt;

	/* Ignore users who don't make any sense */
	WARN(desc->ops->proxy_unvote && desc->proxy_unvote_irq == 0
		 && !desc->proxy_timeout,
		 "Invalid proxy unvote callback or a proxy timeout of 0"
		 " was specified or no proxy unvote IRQ was specified.\n");

	if (desc->proxy_unvote_irq) {
		ret = request_threaded_irq(desc->proxy_unvote_irq,
				  NULL,
				  proxy_unvote_intr_handler,
				  IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				  desc->name, desc);
		if (ret < 0) {
			dev_err(desc->dev,
				"Unable to request proxy unvote IRQ: %d\n",
				ret);
			goto err;
		}
		disable_irq(desc->proxy_unvote_irq);
	}

	snprintf(priv->wname, sizeof(priv->wname), "pil-%s", desc->name);
	wakeup_source_init(&priv->ws, priv->wname);
	INIT_DELAYED_WORK(&priv->proxy, pil_proxy_unvote_work);
	INIT_LIST_HEAD(&priv->segs);

	/* Make sure mapping functions are set. */
	if (!desc->map_fw_mem)
		desc->map_fw_mem = map_fw_mem;

	if (!desc->unmap_fw_mem)
		desc->unmap_fw_mem = unmap_fw_mem;

	return 0;
err_parse_dt:
	ida_simple_remove(&pil_ida, priv->id);
err:
	kfree(priv);
	return ret;
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
		ida_simple_remove(&pil_ida, priv->id);
		flush_delayed_work(&priv->proxy);
		wakeup_source_trash(&priv->ws);
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
	struct device_node *np;
	struct resource res;
	int i;

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-pil");
	if (!np) {
		pr_warn("pil: failed to find qcom,msm-imem-pil node\n");
		goto out;
	}
	if (of_address_to_resource(np, 0, &res)) {
		pr_warn("pil: address to resource on imem region failed\n");
		goto out;
	}
	pil_info_base = ioremap(res.start, resource_size(&res));
	if (!pil_info_base) {
		pr_warn("pil: could not map imem region\n");
		goto out;
	}
	if (__raw_readl(pil_info_base) == 0x53444247) {
		pr_info("pil: pil-imem set to disable pil timeouts\n");
		disable_timeouts = true;
	}
	for (i = 0; i < resource_size(&res)/sizeof(u32); i++)
		writel_relaxed(0, pil_info_base + (i * sizeof(u32)));

out:
	return register_pm_notifier(&pil_pm_notifier);
}
device_initcall(msm_pil_init);

static void __exit msm_pil_exit(void)
{
	unregister_pm_notifier(&pil_pm_notifier);
	if (pil_info_base)
		iounmap(pil_info_base);
}
module_exit(msm_pil_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Load peripheral images and bring peripherals out of reset");
