/*
 * ACPI system memory implementation
 *
 * Copyright (C) 2014, Intel Corporation
 *   Author: Lv Zheng <lv.zheng@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/acpi.h>

#include "internal.h"

/*
 * This list of permanent mappings is for memory that may be accessed from
 * interrupt context, where we can't do the ioremap().
 */
struct acpi_ioremap {
	struct list_head list;
	void __iomem *virt;
	acpi_physical_address phys;
	acpi_size size;
	unsigned long refcount;
};

static LIST_HEAD(acpi_ioremaps);
static DEFINE_MUTEX(acpi_ioremap_lock);

/*
 * The following functions must be called with 'acpi_ioremap_lock' or RCU
 * read lock held.
 */
static inline void acpi_map_get(struct acpi_ioremap *map)
{
	map->refcount++;
}

static inline void acpi_map_put(struct acpi_ioremap *map)
{
	if (!--map->refcount)
		list_del_rcu(&map->list);
}

static struct acpi_ioremap *
acpi_map_lookup_phys(acpi_physical_address phys, acpi_size size)
{
	struct acpi_ioremap *map;

	list_for_each_entry_rcu(map, &acpi_ioremaps, list)
		if (map->phys <= phys &&
		    phys + size <= map->phys + map->size)
			return map;

	return NULL;
}

static struct acpi_ioremap *
acpi_map_lookup_virt(void __iomem *virt, acpi_size size)
{
	struct acpi_ioremap *map;

	list_for_each_entry_rcu(map, &acpi_ioremaps, list)
		if (map->virt <= virt &&
		    virt + size <= map->virt + map->size)
			return map;

	return NULL;
}

static void __iomem *
acpi_map_vaddr_lookup(acpi_physical_address phys, unsigned int size)
{
	struct acpi_ioremap *map;

	map = acpi_map_lookup_phys(phys, size);
	if (map)
		return map->virt + (phys - map->phys);

	return NULL;
}

#ifndef CONFIG_IA64
#define should_use_kmap(pfn)	page_is_ram(pfn)
#else
/* ioremap will take care of cache attributes */
#define should_use_kmap(pfn)	0
#endif

static void __iomem *acpi_map(acpi_physical_address pg_off, unsigned long pg_sz)
{
	unsigned long pfn;

	pfn = pg_off >> PAGE_SHIFT;
	if (should_use_kmap(pfn)) {
		if (pg_sz > PAGE_SIZE)
			return NULL;
		return (void __iomem __force *)kmap(pfn_to_page(pfn));
	} else
		return acpi_os_ioremap(pg_off, pg_sz);
}

static void acpi_unmap(acpi_physical_address pg_off, void __iomem *vaddr)
{
	unsigned long pfn;

	pfn = pg_off >> PAGE_SHIFT;
	if (should_use_kmap(pfn))
		kunmap(pfn_to_page(pfn));
	else
		iounmap(vaddr);
}

static void acpi_map_cleanup(struct acpi_ioremap *map)
{
	if (!map->refcount) {
		synchronize_rcu();
		acpi_unmap(map->phys, map->virt);
		kfree(map);
	}
}

void __iomem *acpi_os_get_iomem(acpi_physical_address phys, unsigned int size)
{
	struct acpi_ioremap *map;
	void __iomem *virt = NULL;

	mutex_lock(&acpi_ioremap_lock);
	map = acpi_map_lookup_phys(phys, size);
	if (map) {
		virt = map->virt + (phys - map->phys);
		acpi_map_get(map);
	}
	mutex_unlock(&acpi_ioremap_lock);

	return virt;
}
EXPORT_SYMBOL_GPL(acpi_os_get_iomem);

void __iomem *__init_refok
acpi_os_map_memory(acpi_physical_address phys, acpi_size size)
{
	struct acpi_ioremap *map;
	void __iomem *virt;
	acpi_physical_address pg_off;
	acpi_size pg_sz;

	if (phys > ULONG_MAX) {
		printk(KERN_ERR PREFIX "Cannot map memory that high\n");
		return NULL;
	}

	if (!acpi_gbl_permanent_mmap)
		return __acpi_map_table((unsigned long)phys, size);

	mutex_lock(&acpi_ioremap_lock);
	/* Check if there's a suitable mapping already. */
	map = acpi_map_lookup_phys(phys, size);
	if (map) {
		acpi_map_get(map);
		goto out;
	}

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		return NULL;
	}

	pg_off = round_down(phys, PAGE_SIZE);
	pg_sz = round_up(phys + size, PAGE_SIZE) - pg_off;
	virt = acpi_map(pg_off, pg_sz);
	if (!virt) {
		mutex_unlock(&acpi_ioremap_lock);
		kfree(map);
		return NULL;
	}

	INIT_LIST_HEAD(&map->list);
	map->virt = virt;
	map->phys = pg_off;
	map->size = pg_sz;
	map->refcount = 1;
	list_add_tail_rcu(&map->list, &acpi_ioremaps);
out:
	mutex_unlock(&acpi_ioremap_lock);
	return map->virt + (phys - map->phys);
}
EXPORT_SYMBOL_GPL(acpi_os_map_memory);

void __ref acpi_os_unmap_memory(void __iomem *virt, acpi_size size)
{
	struct acpi_ioremap *map;

	if (!acpi_gbl_permanent_mmap) {
		__acpi_unmap_table(virt, size);
		return;
	}

	mutex_lock(&acpi_ioremap_lock);
	map = acpi_map_lookup_virt(virt, size);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		WARN(true, PREFIX "%s: bad address %p\n", __func__, virt);
		return;
	}
	acpi_map_put(map);
	mutex_unlock(&acpi_ioremap_lock);

	acpi_map_cleanup(map);
}
EXPORT_SYMBOL_GPL(acpi_os_unmap_memory);

void __init early_acpi_os_unmap_memory(void __iomem *virt, acpi_size size)
{
	if (!acpi_gbl_permanent_mmap)
		__acpi_unmap_table(virt, size);
}

int acpi_os_map_generic_address(struct acpi_generic_address *gas)
{
	u64 addr;
	void __iomem *virt;

	if (gas->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY)
		return 0;

	/* Handle possible alignment issues */
	memcpy(&addr, &gas->address, sizeof(addr));
	if (!addr || !gas->bit_width)
		return -EINVAL;

	virt = acpi_os_map_memory(addr, gas->bit_width / 8);
	if (!virt)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL(acpi_os_map_generic_address);

void acpi_os_unmap_generic_address(struct acpi_generic_address *gas)
{
	u64 addr;
	struct acpi_ioremap *map;

	if (gas->space_id != ACPI_ADR_SPACE_SYSTEM_MEMORY)
		return;

	/* Handle possible alignment issues */
	memcpy(&addr, &gas->address, sizeof(addr));
	if (!addr || !gas->bit_width)
		return;

	mutex_lock(&acpi_ioremap_lock);
	map = acpi_map_lookup_phys(addr, gas->bit_width / 8);
	if (!map) {
		mutex_unlock(&acpi_ioremap_lock);
		return;
	}
	acpi_map_put(map);
	mutex_unlock(&acpi_ioremap_lock);

	acpi_map_cleanup(map);
}
EXPORT_SYMBOL(acpi_os_unmap_generic_address);

#ifdef readq
static inline u64 read64(const volatile void __iomem *addr)
{
	return readq(addr);
}
#else
static inline u64 read64(const volatile void __iomem *addr)
{
	u64 l, h;

	l = readl(addr);
	h = readl(addr+4);
	return l | (h << 32);
}
#endif

#ifdef writeq
static inline void write64(u64 val, volatile void __iomem *addr)
{
	writeq(val, addr);
}
#else
static inline void write64(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val>>32, addr+4);
}
#endif

acpi_status
acpi_os_read_memory(acpi_physical_address phys_addr, u64 *value, u32 width)
{
	void __iomem *virt_addr;
	unsigned int size = width / 8;
	bool unmap = false;
	u64 dummy;

	rcu_read_lock();
	virt_addr = acpi_map_vaddr_lookup(phys_addr, size);
	if (!virt_addr) {
		rcu_read_unlock();
		virt_addr = acpi_os_ioremap(phys_addr, size);
		if (!virt_addr)
			return AE_BAD_ADDRESS;
		unmap = true;
	}

	if (!value)
		value = &dummy;

	switch (width) {
	case 8:
		*(u8 *) value = readb(virt_addr);
		break;
	case 16:
		*(u16 *) value = readw(virt_addr);
		break;
	case 32:
		*(u32 *) value = readl(virt_addr);
		break;
	case 64:
		*(u64 *) value = read64(virt_addr);
		break;
	default:
		BUG();
	}

	if (unmap)
		iounmap(virt_addr);
	else
		rcu_read_unlock();

	return AE_OK;
}

acpi_status
acpi_os_write_memory(acpi_physical_address phys_addr, u64 value, u32 width)
{
	void __iomem *virt_addr;
	unsigned int size = width / 8;
	bool unmap = false;

	rcu_read_lock();
	virt_addr = acpi_map_vaddr_lookup(phys_addr, size);
	if (!virt_addr) {
		rcu_read_unlock();
		virt_addr = acpi_os_ioremap(phys_addr, size);
		if (!virt_addr)
			return AE_BAD_ADDRESS;
		unmap = true;
	}

	switch (width) {
	case 8:
		writeb(value, virt_addr);
		break;
	case 16:
		writew(value, virt_addr);
		break;
	case 32:
		writel(value, virt_addr);
		break;
	case 64:
		write64(value, virt_addr);
		break;
	default:
		BUG();
	}

	if (unmap)
		iounmap(virt_addr);
	else
		rcu_read_unlock();

	return AE_OK;
}
