/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "Minidump: " fmt

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/minidump.h>


#define MAX_NUM_ENTRIES		(CONFIG_MINIDUMP_MAX_ENTRIES + 1)
#define SMEM_ENTRY_SIZE		32
#define MAX_MEM_LENGTH		(SMEM_ENTRY_SIZE * MAX_NUM_ENTRIES)
#define MAX_STRTBL_SIZE		(MAX_NUM_ENTRIES * MAX_NAME_LENGTH)
#define SMEM_MINIDUMP_TABLE_ID	602

/* Bootloader Minidump table */
struct md_smem_table {
	u32	version;
	u32	smem_length;
	u64	next_avail_offset;
	char	reserved[MAX_NAME_LENGTH];
	u64	*region_start;
};

/* Bootloader Minidump region */
struct md_smem_region {
	char	name[MAX_NAME_LENGTH];
	u64	address;
	u64	size;
};

/* md_table: APPS minidump table
 * @num_regions: Number of entries registered
 * @region_base_offset: APPS region start offset smem table
 * @md_smem_table: Pointer smem table
 * @region: Pointer to APPS region in smem table
 * @entry: All registered client entries.
 */

struct md_table {
	u32			num_regions;
	u32			region_base_offset;
	struct md_smem_table	*md_smem_table;
	struct md_smem_region	*region;
	struct md_region	entry[MAX_NUM_ENTRIES];
};

/*
 * md_elfhdr: Minidump table elf header
 * @md_ehdr: elf main header
 * @shdr: Section header
 * @phdr: Program header
 * @elf_offset: section offset in elf
 * @strtable_idx: string table current index position
 */
struct md_elfhdr {
	struct elfhdr	*md_ehdr;
	struct elf_shdr	*shdr;
	struct elf_phdr	*phdr;
	u64		elf_offset;
	u64		strtable_idx;
};

/* Protect elfheader and smem table from deferred calls contention */
static DEFINE_SPINLOCK(mdt_lock);
static struct md_table	minidump_table;
static struct md_elfhdr	minidump_elfheader;

bool minidump_enabled;
static unsigned int pendings;
static unsigned int region_idx = 1; /* First entry is ELF header*/

static inline struct elf_shdr *elf_sheader(struct elfhdr *hdr)
{
	return (struct elf_shdr *)((size_t)hdr + (size_t)hdr->e_shoff);
}

static inline struct elf_shdr *elf_section(struct elfhdr *hdr, int idx)
{
	return &elf_sheader(hdr)[idx];
}

static inline struct elf_phdr *elf_pheader(struct elfhdr *hdr)
{
	return (struct elf_phdr *)((size_t)hdr + (size_t)hdr->e_phoff);
}

static inline struct elf_phdr *elf_program(struct elfhdr *hdr, int idx)
{
	return &elf_pheader(hdr)[idx];
}

static inline char *elf_str_table(struct elfhdr *hdr)
{
	if (hdr->e_shstrndx == SHN_UNDEF)
		return NULL;
	return (char *)hdr + elf_section(hdr, hdr->e_shstrndx)->sh_offset;
}

static inline char *elf_lookup_string(struct elfhdr *hdr, int offset)
{
	char *strtab = elf_str_table(hdr);

	if ((strtab == NULL) || (minidump_elfheader.strtable_idx < offset))
		return NULL;
	return strtab + offset;
}

static inline unsigned int set_section_name(const char *name)
{
	char *strtab = elf_str_table(minidump_elfheader.md_ehdr);
	int idx = minidump_elfheader.strtable_idx;
	int ret = 0;

	if ((strtab == NULL) || (name == NULL))
		return 0;

	ret = idx;
	idx += strlcpy((strtab + idx), name, MAX_NAME_LENGTH);
	minidump_elfheader.strtable_idx = idx + 1;

	return ret;
}

/* return 1 if name already exists */
static inline bool md_check_name(const char *name)
{
	struct md_region *mde = minidump_table.entry;
	int i, regno = minidump_table.num_regions;

	for (i = 0; i < regno; i++, mde++)
		if (!strcmp(mde->name, name))
			return true;
	return false;
}

/* Update Mini dump table in SMEM */
static int md_update_smem_table(const struct md_region *entry)
{
	struct md_smem_region *mdr;
	struct elfhdr *hdr = minidump_elfheader.md_ehdr;
	struct elf_shdr *shdr = elf_section(hdr, hdr->e_shnum++);
	struct elf_phdr *phdr = elf_program(hdr, hdr->e_phnum++);

	mdr = &minidump_table.region[region_idx++];

	strlcpy(mdr->name, entry->name, sizeof(mdr->name));
	mdr->address = entry->phys_addr;
	mdr->size = entry->size;

	/* Update elf header */
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_name = set_section_name(mdr->name);
	shdr->sh_addr = (elf_addr_t)entry->virt_addr;
	shdr->sh_size = mdr->size;
	shdr->sh_flags = SHF_WRITE;
	shdr->sh_offset = minidump_elfheader.elf_offset;
	shdr->sh_entsize = 0;

	phdr->p_type = PT_LOAD;
	phdr->p_offset = minidump_elfheader.elf_offset;
	phdr->p_vaddr = entry->virt_addr;
	phdr->p_paddr = entry->phys_addr;
	phdr->p_filesz = phdr->p_memsz =  mdr->size;
	phdr->p_flags = PF_R | PF_W;

	minidump_elfheader.elf_offset += shdr->sh_size;

	return 0;
}

int msm_minidump_add_region(const struct md_region *entry)
{
	u32 entries;
	struct md_region *mdr;
	int ret = 0;

	if (!entry)
		return -EINVAL;

	if (((strlen(entry->name) > MAX_NAME_LENGTH) ||
		 md_check_name(entry->name)) && !entry->virt_addr) {
		pr_err("Invalid entry details\n");
		return -EINVAL;
	}

	if (!IS_ALIGNED(entry->size, 4)) {
		pr_err("size should be 4 byte aligned\n");
		return -EINVAL;
	}

	spin_lock(&mdt_lock);
	entries = minidump_table.num_regions;
	if (entries >= MAX_NUM_ENTRIES) {
		pr_err("Maximum entries reached.\n");
		spin_unlock(&mdt_lock);
		return -ENOMEM;
	}

	mdr = &minidump_table.entry[entries];
	strlcpy(mdr->name, entry->name, sizeof(mdr->name));
	mdr->virt_addr = entry->virt_addr;
	mdr->phys_addr = entry->phys_addr;
	mdr->size = entry->size;
	mdr->id = entry->id;

	minidump_table.num_regions = entries + 1;

	if (minidump_enabled)
		ret = md_update_smem_table(entry);
	else
		pendings++;

	spin_unlock(&mdt_lock);

	pr_debug("Minidump: added %s to %s list\n",
			 mdr->name, minidump_enabled ? "":"pending");
	return ret;
}
EXPORT_SYMBOL(msm_minidump_add_region);

static int msm_minidump_add_header(void)
{
	struct md_smem_region *mdreg = &minidump_table.region[0];
	struct elfhdr *md_ehdr;
	struct elf_shdr *shdr;
	struct elf_phdr *phdr;
	unsigned int strtbl_off, elfh_size, phdr_off;
	char *banner;

	/* Header buffer contains:
	 * elf header, MAX_NUM_ENTRIES+1 of section and program elf headers,
	 * string table section and linux banner.
	 */
	elfh_size = sizeof(*md_ehdr) + MAX_STRTBL_SIZE + MAX_MEM_LENGTH +
		((sizeof(*shdr) + sizeof(*phdr)) * (MAX_NUM_ENTRIES + 1));

	minidump_elfheader.md_ehdr = kzalloc(elfh_size, GFP_KERNEL);
	if (!minidump_elfheader.md_ehdr)
		return -ENOMEM;

	strlcpy(mdreg->name, "KELF_HEADER", sizeof(mdreg->name));
	mdreg->address = virt_to_phys(minidump_elfheader.md_ehdr);
	mdreg->size = elfh_size;

	md_ehdr = minidump_elfheader.md_ehdr;
	/* Assign section/program headers offset */
	minidump_elfheader.shdr = shdr = (struct elf_shdr *)(md_ehdr + 1);
	minidump_elfheader.phdr = phdr =
				 (struct elf_phdr *)(shdr + MAX_NUM_ENTRIES);
	phdr_off = sizeof(*md_ehdr) + (sizeof(*shdr) * MAX_NUM_ENTRIES);

	memcpy(md_ehdr->e_ident, ELFMAG, SELFMAG);
	md_ehdr->e_ident[EI_CLASS] = ELF_CLASS;
	md_ehdr->e_ident[EI_DATA] = ELF_DATA;
	md_ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	md_ehdr->e_ident[EI_OSABI] = ELF_OSABI;
	md_ehdr->e_type = ET_CORE;
	md_ehdr->e_machine  = ELF_ARCH;
	md_ehdr->e_version = EV_CURRENT;
	md_ehdr->e_ehsize = sizeof(*md_ehdr);
	md_ehdr->e_phoff = phdr_off;
	md_ehdr->e_phentsize = sizeof(*phdr);
	md_ehdr->e_shoff = sizeof(*md_ehdr);
	md_ehdr->e_shentsize = sizeof(*shdr);
	md_ehdr->e_shstrndx = 1;

	minidump_elfheader.elf_offset = elfh_size;

	/*
	 * First section header should be NULL,
	 * 2nd section is string table.
	 */
	minidump_elfheader.strtable_idx = 1;
	strtbl_off = sizeof(*md_ehdr) +
			((sizeof(*phdr) + sizeof(*shdr)) * MAX_NUM_ENTRIES);
	shdr++;
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_offset = (elf_addr_t)strtbl_off;
	shdr->sh_size = MAX_STRTBL_SIZE;
	shdr->sh_entsize = 0;
	shdr->sh_flags = 0;
	shdr->sh_name = set_section_name("STR_TBL");
	shdr++;

	/* 3rd section is for minidump_table VA, used by parsers */
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_entsize = 0;
	shdr->sh_flags = 0;
	shdr->sh_addr = (elf_addr_t)&minidump_table;
	shdr->sh_name = set_section_name("minidump_table");
	shdr++;

	/* 4th section is linux banner */
	banner = (char *)md_ehdr + strtbl_off + MAX_STRTBL_SIZE;
	strlcpy(banner, linux_banner, MAX_MEM_LENGTH);

	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_offset = (elf_addr_t)(strtbl_off + MAX_STRTBL_SIZE);
	shdr->sh_size = strlen(linux_banner) + 1;
	shdr->sh_addr = (elf_addr_t)linux_banner;
	shdr->sh_entsize = 0;
	shdr->sh_flags = SHF_WRITE;
	shdr->sh_name = set_section_name("linux_banner");

	phdr->p_type = PT_LOAD;
	phdr->p_offset = (elf_addr_t)(strtbl_off + MAX_STRTBL_SIZE);
	phdr->p_vaddr = (elf_addr_t)linux_banner;
	phdr->p_paddr = virt_to_phys(linux_banner);
	phdr->p_filesz = phdr->p_memsz = strlen(linux_banner) + 1;
	phdr->p_flags = PF_R | PF_W;

	/* Update headers count*/
	md_ehdr->e_phnum = 1;
	md_ehdr->e_shnum = 4;

	return 0;
}

static int __init msm_minidump_init(void)
{
	unsigned int i, size;
	struct md_region *mdr;
	struct md_smem_table *smem_table;

	/* Get Minidump table */
	smem_table = smem_get_entry(SMEM_MINIDUMP_TABLE_ID, &size, 0,
					SMEM_ANY_HOST_FLAG);
	if (IS_ERR_OR_NULL(smem_table)) {
		pr_err("SMEM is not initialized.\n");
		return -ENODEV;
	}

	if ((smem_table->next_avail_offset + MAX_MEM_LENGTH) >
		 smem_table->smem_length) {
		pr_err("SMEM memory not available.\n");
		return -ENOMEM;
	}

	/* Get next_avail_offset and update it to reserve memory */
	minidump_table.region_base_offset = smem_table->next_avail_offset;
	minidump_table.region = (struct md_smem_region *)((uintptr_t)smem_table
				+ minidump_table.region_base_offset);

	smem_table->next_avail_offset =
			minidump_table.region_base_offset + MAX_MEM_LENGTH;
	minidump_table.md_smem_table = smem_table;

	msm_minidump_add_header();

	/* Add pending entries to smem table */
	spin_lock(&mdt_lock);
	minidump_enabled = true;

	for (i = 0; i < pendings; i++) {
		mdr = &minidump_table.entry[i];
		if (md_update_smem_table(mdr)) {
			pr_err("Unable to add entry %s to smem table\n",
				mdr->name);
			spin_unlock(&mdt_lock);
			return -ENOENT;
		}
	}

	pendings = 0;
	spin_unlock(&mdt_lock);

	pr_info("Enabled, region base:%d, region 0x%pK\n",
		 minidump_table.region_base_offset, minidump_table.region);

	return 0;
}
subsys_initcall(msm_minidump_init)
