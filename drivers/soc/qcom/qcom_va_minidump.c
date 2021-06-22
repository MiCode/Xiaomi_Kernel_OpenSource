// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "va-minidump: %s: " fmt, __func__

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/elf.h>
#include <linux/slab.h>
#include <soc/qcom/minidump.h>
#include "elf.h"

struct va_md_tree_node {
	struct va_md_entry entry;
	int lindex;
	int rindex;
};

struct va_md_elf_info {
	unsigned long ehdr;
	unsigned long shdr_cnt;
	unsigned long phdr_cnt;
	unsigned long pload_size;
	unsigned long str_tbl_size;
};

#define VA_MD_VADDR_MARKER	-1
#define VA_MD_CB_MARKER		-2
#define MAX_ELF_SECTION		0xFFFFU

struct va_minidump_data {
	phys_addr_t mem_phys_addr;
	unsigned int total_mem_size;
	unsigned long elf_mem;
	unsigned int num_sections;
	unsigned long str_tbl_idx;
	struct va_md_elf_info elf;
	struct md_region md_entry;
	bool in_oops_handler;
	bool va_md_minidump_reg;
};

struct va_minidump_data va_md_data;

ATOMIC_NOTIFIER_HEAD(qcom_va_md_notifier_list);
EXPORT_SYMBOL(qcom_va_md_notifier_list);

static void va_md_add_entry(struct va_md_entry *entry)
{
	struct va_md_tree_node *dst = ((struct va_md_tree_node *)va_md_data.elf_mem) +
					va_md_data.num_sections;
	unsigned int len = strlen(entry->owner);

	dst->entry = *entry;
	WARN_ONCE(len > MAX_OWNER_STRING - 1,
		"Client entry name %s (len = %u) is greater than expected %u\n",
		entry->owner, len, MAX_OWNER_STRING - 1);
	dst->entry.owner[MAX_OWNER_STRING - 1] = '\0';

	if (entry->vaddr) {
		dst->lindex = VA_MD_VADDR_MARKER;
		dst->rindex = VA_MD_VADDR_MARKER;
	} else {
		dst->lindex = VA_MD_CB_MARKER;
		dst->rindex = VA_MD_CB_MARKER;
	}

	va_md_data.num_sections++;
}

static bool va_md_check_overlap(struct va_md_entry *entry, unsigned int index)
{
	unsigned long ent_start, ent_end;
	unsigned long node_start, node_end;
	struct va_md_tree_node *node = (struct va_md_tree_node *)va_md_data.elf_mem;

	node_start = node[index].entry.vaddr;
	node_end = node[index].entry.vaddr + node[index].entry.size - 1;
	ent_start = entry->vaddr;
	ent_end = entry->vaddr + entry->size - 1;

	if (((node_start <= ent_start) && (ent_start <= node_end)) ||
		((node_start <= ent_end) && (ent_end <= node_end)) ||
		((ent_start <= node_start) && (node_end <= ent_end)))
		return true;

	return false;
}

static bool va_md_move_left(struct va_md_entry *entry, unsigned int index)
{
	unsigned long ent_start, ent_end;
	unsigned long node_start, node_end;
	struct va_md_tree_node *node = (struct va_md_tree_node *)va_md_data.elf_mem;

	node_start = node[index].entry.vaddr;
	node_end = node[index].entry.vaddr + node[index].entry.size - 1;
	ent_start = entry->vaddr;
	ent_end = entry->vaddr + entry->size - 1;

	if ((ent_start < node_start) && (ent_end < node_start))
		return true;

	return false;
}

static bool va_md_move_right(struct va_md_entry *entry, unsigned int index)
{
	unsigned long ent_start, ent_end;
	unsigned long node_start, node_end;
	struct va_md_tree_node *node = (struct va_md_tree_node *)va_md_data.elf_mem;

	node_start = node[index].entry.vaddr;
	node_end = node[index].entry.vaddr + node[index].entry.size - 1;
	ent_start = entry->vaddr;
	ent_end = entry->vaddr + entry->size - 1;

	if ((ent_start > node_end) && (ent_end > node_end))
		return true;

	return false;
}

static int va_md_tree_insert(struct va_md_entry *entry)
{
	unsigned int baseindex = 0;
	int ret = 0;
	static int num_nodes;
	struct va_md_tree_node *tree = (struct va_md_tree_node *)va_md_data.elf_mem;

	if (!entry->vaddr || !va_md_data.num_sections) {
		va_md_add_entry(entry);
		goto out;
	}

	while (baseindex < va_md_data.num_sections) {
		if ((tree[baseindex].lindex == VA_MD_CB_MARKER) &&
			(tree[baseindex].rindex == VA_MD_CB_MARKER)) {
			baseindex++;
			continue;
		}

		if (va_md_check_overlap(entry, baseindex)) {
			entry->owner[MAX_OWNER_STRING - 1] = '\0';
			pr_err("Overlapping region owner:%s\n", entry->owner);
			ret = -EINVAL;
			goto out;
		}

		if (va_md_move_left(entry, baseindex)) {
			if (tree[baseindex].lindex == VA_MD_VADDR_MARKER) {
				tree[baseindex].lindex = va_md_data.num_sections;
				va_md_add_entry(entry);
				num_nodes++;
				goto exit_loop;
			} else {
				baseindex = tree[baseindex].lindex;
				continue;
			}

		} else if (va_md_move_right(entry, baseindex)) {
			if (tree[baseindex].rindex == VA_MD_VADDR_MARKER) {
				tree[baseindex].rindex = va_md_data.num_sections;
				va_md_add_entry(entry);
				num_nodes++;
				goto exit_loop;
			} else {
				baseindex = tree[baseindex].rindex;
				continue;
			}
		} else {
			pr_err("Warning: Corrupted Binary Search Tree\n");
		}
	}

exit_loop:
	if (!num_nodes) {
		va_md_add_entry(entry);
		num_nodes++;
	}

out:
	return ret;
}

static bool va_md_overflow_check(void)
{
	unsigned long end_addr;
	unsigned long start_addr = va_md_data.elf_mem;

	start_addr += sizeof(struct va_md_tree_node) * va_md_data.num_sections;
	end_addr = start_addr + sizeof(struct va_md_tree_node) - 1;

	if (end_addr > va_md_data.elf_mem + va_md_data.total_mem_size - 1)
		return true;
	else
		return false;
}

int qcom_va_md_add_region(struct va_md_entry *entry)
{
	if (!va_md_data.in_oops_handler)
		return -EINVAL;

	if ((!entry->vaddr == !entry->cb) || (entry->size <= 0)) {
		entry->owner[MAX_OWNER_STRING - 1] = '\0';
		pr_err("Invalid entry from owner:%s\n", entry->owner);
		return -EINVAL;
	}

	if (va_md_data.num_sections > MAX_ELF_SECTION) {
		pr_err("MAX_ELF_SECTION reached\n");
		return -ENOSPC;
	}

	if (va_md_overflow_check()) {
		pr_err("Total CMA consumed for Qcom VA minidump\n");
		return -ENOMEM;
	}

	return va_md_tree_insert(entry);
}
EXPORT_SYMBOL(qcom_va_md_add_region);

static void qcom_va_md_minidump_registration(void)
{
	strlcpy(va_md_data.md_entry.name, "KVA_DUMP", sizeof(va_md_data.md_entry.name));

	va_md_data.md_entry.virt_addr = va_md_data.elf.ehdr;
	va_md_data.md_entry.phys_addr =	va_md_data.mem_phys_addr +
		(sizeof(struct va_md_tree_node) * va_md_data.num_sections);
	va_md_data.md_entry.size = sizeof(struct elfhdr) +
		(sizeof(struct elf_shdr) * va_md_data.elf.shdr_cnt) +
		(sizeof(struct elf_phdr) * va_md_data.elf.phdr_cnt) +
		va_md_data.elf.pload_size + va_md_data.elf.str_tbl_size;
	va_md_data.md_entry.size = ALIGN(va_md_data.md_entry.size, 4);

	if (msm_minidump_add_region(&va_md_data.md_entry) < 0) {
		pr_err("Failed to register VA driver CMA region with minidump\n");
		va_md_data.va_md_minidump_reg = false;
		return;
	}

	va_md_data.va_md_minidump_reg = true;
}

static inline unsigned long set_sec_name(struct elfhdr *ehdr, const char *name)
{
	char *strtab = elf_str_table(ehdr);
	unsigned long idx = va_md_data.str_tbl_idx;
	unsigned long ret = 0;

	if ((strtab == NULL) || (name == NULL))
		return 0;

	ret = idx;
	idx += strlcpy((strtab + idx), name, MAX_OWNER_STRING);
	va_md_data.str_tbl_idx = idx + 1;
	return ret;
}

static void qcom_va_add_elf_hdr(void)
{
	struct elfhdr *ehdr = (struct elfhdr *)va_md_data.elf.ehdr;
	unsigned long phdr_off;

	phdr_off = sizeof(*ehdr) + (sizeof(struct elf_shdr) * va_md_data.elf.shdr_cnt);

	memcpy(ehdr->e_ident, ELFMAG, SELFMAG);
	ehdr->e_ident[EI_CLASS] = ELF_CLASS;
	ehdr->e_ident[EI_DATA] = ELF_DATA;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = ELF_OSABI;
	ehdr->e_type = ET_CORE;
	ehdr->e_machine  = ELF_ARCH;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_ehsize = sizeof(*ehdr);
	ehdr->e_shoff = sizeof(*ehdr);
	ehdr->e_shentsize = sizeof(struct elf_shdr);
	ehdr->e_shstrndx = 1;
	ehdr->e_phentsize = sizeof(struct elf_phdr);
	ehdr->e_phoff = phdr_off;
}

static void qcom_va_add_hdrs(void)
{
	struct elf_shdr *shdr;
	struct elf_phdr *phdr;
	unsigned long strtbl_off, offset, i;
	struct elfhdr *ehdr = (struct elfhdr *)va_md_data.elf.ehdr;
	struct va_md_tree_node *arr = (struct va_md_tree_node *)va_md_data.elf_mem;

	strtbl_off = ehdr->e_phoff + (sizeof(*phdr) * va_md_data.elf.phdr_cnt);

	/* First section header is NULL */
	shdr = elf_section(ehdr, ehdr->e_shnum);
	ehdr->e_shnum++;

	/* String table section */
	va_md_data.str_tbl_idx = 1;
	shdr = elf_section(ehdr, ehdr->e_shnum);
	ehdr->e_shnum++;

	shdr->sh_type = SHT_STRTAB;
	shdr->sh_offset = strtbl_off;
	shdr->sh_name = set_sec_name(ehdr, "STR_TBL");
	shdr->sh_size = va_md_data.elf.str_tbl_size;

	offset = strtbl_off + va_md_data.elf.str_tbl_size;
	for (i = 0; i < (va_md_data.elf.shdr_cnt - 2); i++) {
		/* section header */
		shdr = elf_section(ehdr, ehdr->e_shnum);
		shdr->sh_type = SHT_PROGBITS;
		shdr->sh_name = set_sec_name(ehdr, arr[i].entry.owner);
		shdr->sh_size = arr[i].entry.size;
		shdr->sh_flags = SHF_WRITE;
		shdr->sh_offset = offset;

		/* program header */
		phdr = elf_program(ehdr, ehdr->e_phnum);
		phdr->p_type = PT_LOAD;
		phdr->p_offset = offset;
		phdr->p_filesz = phdr->p_memsz = arr[i].entry.size;
		phdr->p_flags = PF_R | PF_W;

		if (arr[i].entry.vaddr) {
			shdr->sh_addr =  phdr->p_vaddr = arr[i].entry.vaddr;
			memcpy((void *)(va_md_data.elf.ehdr + offset),
				(void *)shdr->sh_addr, shdr->sh_size);
		} else {
			shdr->sh_addr =  phdr->p_vaddr = va_md_data.elf.ehdr + offset;
			arr[i].entry.cb((void *)(va_md_data.elf.ehdr + offset),
				shdr->sh_size);
		}

		offset += shdr->sh_size;
		ehdr->e_shnum++;
		ehdr->e_phnum++;
	}
}

static int qcom_va_md_calc_size(unsigned int shdr_cnt)
{
	unsigned int len, size = 0;
	static unsigned long tot_size;
	struct va_md_tree_node *arr = (struct va_md_tree_node *)va_md_data.elf_mem;

	if (!shdr_cnt) {
		tot_size = sizeof(struct va_md_tree_node) * va_md_data.num_sections;
		size = (sizeof(struct elfhdr) + (2 * sizeof(struct elf_shdr)) +
				strlen("STR_TBL") + 2);
	}

	len = strlen(arr[shdr_cnt].entry.owner);
	size += (sizeof(struct elf_shdr) + sizeof(struct elf_phdr) +
		arr[shdr_cnt].entry.size + len + 1);
	tot_size += size;
	if (tot_size > va_md_data.total_mem_size) {
		pr_err("Total CMA consumed, no space left\n");
		return -ENOSPC;
	}

	if (!shdr_cnt) {
		va_md_data.elf.ehdr = va_md_data.elf_mem + (sizeof(struct va_md_tree_node)
					* va_md_data.num_sections);
		va_md_data.elf.shdr_cnt = 2;
		va_md_data.elf.phdr_cnt = 0;
		va_md_data.elf.pload_size = 0;
		va_md_data.elf.str_tbl_size = strlen("STR_TBL") + 2;
	}

	va_md_data.elf.shdr_cnt++;
	va_md_data.elf.phdr_cnt++;
	va_md_data.elf.pload_size += arr[shdr_cnt].entry.size;
	va_md_data.elf.str_tbl_size += (len + 1);

	return 0;
}

static int qcom_va_md_calc_elf_size(void)
{
	unsigned int i;
	int ret = 0;

	if (va_md_overflow_check()) {
		pr_err("Total CMA consumed, no space to create ELF\n");
		return -ENOSPC;
	}

	pr_debug("Num sections:%u\n", va_md_data.num_sections);
	for (i = 0; i < va_md_data.num_sections; i++) {
		ret = qcom_va_md_calc_size(i);
		if (ret < 0)
			break;
	}

	return ret;
}

static int qcom_va_md_panic_handler(struct notifier_block *this,
				    unsigned long event, void *ptr)
{
	unsigned long size;

	if (va_md_data.in_oops_handler)
		return NOTIFY_DONE;

	va_md_data.in_oops_handler = true;
	atomic_notifier_call_chain(&qcom_va_md_notifier_list, 0, NULL);
	if (!va_md_data.num_sections)
		goto out;

	if (qcom_va_md_calc_elf_size() < 0)
		goto out;

	size = sizeof(struct elfhdr) +
		(sizeof(struct elf_shdr) * va_md_data.elf.shdr_cnt) +
		(sizeof(struct elf_phdr) * va_md_data.elf.phdr_cnt) +
		va_md_data.elf.pload_size + va_md_data.elf.str_tbl_size;
	size = ALIGN(size, 4);
	memset((void *)va_md_data.elf.ehdr, 0, size);

	qcom_va_md_minidump_registration();
out:
	return NOTIFY_DONE;
}

static int qcom_va_md_elf_panic_handler(struct notifier_block *this,
				    unsigned long event, void *ptr)
{
	if (!va_md_data.num_sections || !va_md_data.va_md_minidump_reg)
		goto out;

	qcom_va_add_elf_hdr();
	qcom_va_add_hdrs();

out:
	va_md_data.in_oops_handler = false;
	return NOTIFY_DONE;
}

static struct notifier_block qcom_va_md_panic_blk = {
	.notifier_call = qcom_va_md_panic_handler,
	.priority = INT_MAX - 3,
};

static struct notifier_block qcom_va_md_elf_panic_blk = {
	.notifier_call = qcom_va_md_elf_panic_handler,
	.priority = INT_MAX - 4,
};

static int qcom_va_md_reserve_mem(struct device *dev)
{
	struct device_node *node;
	unsigned int size[2];
	int ret = 0;

	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (node) {
		ret = of_reserved_mem_device_init_by_idx(dev, dev->of_node, 0);
		of_node_put(dev->of_node);
		if (ret) {
			pr_err("Failed to initialize CMA mem, ret %d\n",
				ret);
			goto out;
		}
	}

	ret = of_property_read_u32_array(node, "size", size, 2);
	if (ret) {
		pr_err("Failed to get size of CMA, ret %d\n", ret);
		goto out;
	}

	va_md_data.total_mem_size = size[1];

out:
	return ret;
}

static int qcom_va_md_driver_remove(struct platform_device *pdev)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &qcom_va_md_elf_panic_blk);
	atomic_notifier_chain_unregister(&panic_notifier_list, &qcom_va_md_panic_blk);
	vunmap((void *)va_md_data.elf_mem);
	return 0;
}

static int qcom_va_md_driver_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	void *vaddr;
	int count;
	struct page **pages, *page;
	dma_addr_t dma_handle;

	ret = qcom_va_md_reserve_mem(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "CMA for VA based minidump is not present\n");
		goto out;
	}

	vaddr = dma_alloc_coherent(&pdev->dev, va_md_data.total_mem_size, &dma_handle,
				   GFP_KERNEL);
	if (!vaddr) {
		ret = -ENOMEM;
		goto out;
	}

	dma_free_coherent(&pdev->dev, va_md_data.total_mem_size, vaddr, dma_handle);
	page = phys_to_page(dma_to_phys(&pdev->dev, dma_handle));
	count = PAGE_ALIGN(va_md_data.total_mem_size) >> PAGE_SHIFT;
	pages = kmalloc_array(count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	for (i = 0; i < count; i++)
		pages[i] = nth_page(page, i);

	vaddr = vmap(pages, count, VM_DMA_COHERENT, pgprot_dmacoherent(PAGE_KERNEL));
	kfree(pages);

	va_md_data.mem_phys_addr = dma_to_phys(&pdev->dev, dma_handle);
	va_md_data.elf_mem = (unsigned long)vaddr;

	atomic_notifier_chain_register(&panic_notifier_list, &qcom_va_md_panic_blk);
	atomic_notifier_chain_register(&panic_notifier_list, &qcom_va_md_elf_panic_blk);

out:
	return ret;
}

static const struct of_device_id qcom_va_md_of_match[] = {
	{.compatible = "qcom,va-minidump"},
	{}
};

MODULE_DEVICE_TABLE(of, qcom_va_md_of_match);

static struct platform_driver qcom_va_md_driver = {
	.driver = {
		   .name = "qcom-va-minidump",
		   .of_match_table = qcom_va_md_of_match,
		   },
	.probe = qcom_va_md_driver_probe,
	.remove = qcom_va_md_driver_remove,
};

module_platform_driver(qcom_va_md_driver);

MODULE_DESCRIPTION("Qcom VA Minidump Driver");
MODULE_LICENSE("GPL v2");
