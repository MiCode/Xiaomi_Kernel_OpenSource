/*
 * Copyright (C) ST-Ericsson SA 2011
 * Author: Maxime Coquelin <maxime.coquelin@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sort.h>
#include <linux/pasr.h>
#include <linux/debugfs.h>

#include "helper.h"

#define NR_DIES 8
#define NR_INT 8

struct ddr_die {
	u64 size;
	phys_addr_t addr;
};

struct interleaved_area {
	phys_addr_t addr1;
	phys_addr_t addr2;
	u64 size;
};

struct pasr_info {
	int nr_dies;
	struct ddr_die die[NR_DIES];

	int nr_int;
	struct interleaved_area int_area[NR_INT];
};

static struct pasr_info __initdata pasr_info;
static struct pasr_map pasr_map;
u64 section_size;
unsigned int section_bit;

static void add_ddr_die(phys_addr_t addr, u64 size);
static void add_interleaved_area(phys_addr_t a1,
		phys_addr_t a2, u64 size);

static int __init section_param(char *p)
{
	section_size = memparse(p, &p);
	section_bit = ffs(section_size) - 1;

	return 0;
}
early_param("section", section_param);

static int __init ddr_die_param(char *p)
{
	phys_addr_t start;
	u64 size;

	size = memparse(p, &p);

	if (*p != '@')
		goto err;

	start = memparse(p + 1, &p);

	add_ddr_die(start, size);

	return 0;
err:
	return -EINVAL;
}
early_param("ddr_die", ddr_die_param);

static int __init interleaved_param(char *p)
{
	phys_addr_t start1, start2;
	u64 size;

	size = memparse(p, &p);

	if (*p != '@')
		goto err;

	start1 = memparse(p + 1, &p);

	if (*p != ':')
		goto err;

	start2 = memparse(p + 1, &p);

	add_interleaved_area(start1, start2, size);

	return 0;
err:
	return -EINVAL;
}
early_param("interleaved", interleaved_param);

void __init add_ddr_die(phys_addr_t addr, u64 size)
{
	BUG_ON(pasr_info.nr_dies >= NR_DIES);

	pasr_info.die[pasr_info.nr_dies].addr = addr;
	pasr_info.die[pasr_info.nr_dies++].size = size;
}

void __init add_interleaved_area(phys_addr_t a1, phys_addr_t a2,
		u64 size)
{
	BUG_ON(pasr_info.nr_int >= NR_INT);

	pasr_info.int_area[pasr_info.nr_int].addr1 = a1;
	pasr_info.int_area[pasr_info.nr_int].addr2 = a2;
	pasr_info.int_area[pasr_info.nr_int++].size = size;
}

#ifdef DEBUG
static void __init pasr_print_info(struct pasr_info *info)
{
	int i;

	pr_info("PASR information coherent\n");


	pr_info("DDR Dies layout:\n");
	pr_info("\tid - start address - end address\n");
	for (i = 0; i < info->nr_dies; i++)
		pr_info("\t- %d : %#09llx - %#09llx\n",
			i, (u64)info->die[i].addr,
			(u64)(info->die[i].addr
				+ info->die[i].size - 1));

	if (info->nr_int == 0) {
		pr_info("No interleaved areas declared\n");
		return;
	}

	pr_info("Interleaving layout:\n");
	pr_info("\tid - start @1 - end @2 : start @2 - end @2\n");
	for (i = 0; i < info->nr_int; i++)
		pr_info("\t-%d - %#09llx - %#09llx : %#09llx - %#09llx\n"
			, i
			, (u64)info->int_area[i].addr1
			, (u64)(info->int_area[i].addr1
				+ info->int_area[i].size - 1)
			, (u64)info->int_area[i].addr2
			, (u64)(info->int_area[i].addr2
				+ info->int_area[i].size - 1));
}
#else
#define pasr_print_info(info) do {} while (0)
#endif /* DEBUG */

static int __init is_in_physmem(phys_addr_t addr, struct ddr_die *d)
{
	return ((addr >= d->addr) && (addr <= d->addr + d->size - 1));
}

static int __init pasr_check_interleave_in_physmem(struct pasr_info *info,
						struct interleaved_area *i)
{
	struct ddr_die *d;
	int j;
	int err = 4;

	for (j = 0; j < info->nr_dies; j++) {
		d = &info->die[j];

		if (is_in_physmem(i->addr1, d))
			err--;
		if (is_in_physmem(i->addr1 + i->size - 1, d))
			err--;
		if (is_in_physmem(i->addr2, d))
			err--;
		if (is_in_physmem(i->addr2 + i->size - 1, d))
			err--;
	}

	return err;
}

static int __init ddrdie_cmp(const void *_a, const void *_b)
{
	const struct ddr_die *a = _a, *b = _b;

	return a->addr < b->addr ? -1 : a->addr > b->addr ? 1 : 0;
}

static int __init interleaved_cmp(const void *_a, const void *_b)
{
	const struct interleaved_area *a = _a, *b = _b;

	return a->addr1 < b->addr1 ? -1 : a->addr1 > b->addr1 ? 1 : 0;
}

static int __init pasr_info_sanity_check(struct pasr_info *info)
{
	int i;

	/* Check at least one physical chunk is defined */
	if (info->nr_dies == 0) {
		pr_err("%s: No DDR dies declared in command line\n", __func__);
		return -EINVAL;
	}

	/* Sort DDR dies areas */
	sort(&info->die, info->nr_dies,
			sizeof(info->die[0]), ddrdie_cmp, NULL);

	/* Physical layout checking */
	for (i = 0; i < info->nr_dies; i++) {
		struct ddr_die *d1, *d2;

		d1 = &info->die[i];

		if (d1->size == 0) {
			pr_err("%s: DDR die at %#x has 0 size\n",
					__func__, d1->addr);
			return -EINVAL;
		}

		/*  Check die is aligned on section boundaries */
		if (((d1->addr & ~(section_size - 1)) != d1->addr)
			|| (((d1->size & ~(section_size - 1))) != d1->size)) {
			pr_err("%s: DDR die at %#x (size %#llx) \
				is not aligned on section boundaries %#llx\n",
				__func__, d1->addr, d1->size, section_size);
			return -EINVAL;
		}

		if (i == 0)
			continue;

		/* Check areas are not overlapping */
		d2 = d1;
		d1 = &info->die[i-1];
		if ((d1->addr + d1->size - 1) >= d2->addr) {
			pr_err("%s: DDR dies at %#x and %#x are overlapping\n",
					__func__, d1->addr, d2->addr);
			return -EINVAL;
		}
	}

	/* Interleave layout checking */
	if (info->nr_int == 0)
		goto out;

	/* Sort interleaved areas */
	sort(&info->int_area, info->nr_int,
			sizeof(info->int_area[0]), interleaved_cmp, NULL);

	for (i = 0; i < info->nr_int; i++) {
		struct interleaved_area *i1;

		i1 = &info->int_area[i];
		if (i1->size == 0) {
			pr_err("%s: Interleaved area %#x/%#x  has 0 size\n",
					__func__, i1->addr1, i1->addr2);
			return -EINVAL;
		}

		/* Check area is aligned on section boundaries */
		if (((i1->addr1 & ~(section_size - 1)) != i1->addr1)
			|| ((i1->addr2 & ~(section_size - 1)) != i1->addr2)
			|| ((i1->size & ~(section_size - 1)) != i1->size)) {
			pr_err("%s: Interleaved area at %#x/%#x (size %#lx) \
				is not aligned on section boundaries %#lx\n",
				__func__, i1->addr1, i1->addr2, i1->size,
				section_size);
			return -EINVAL;
		}

		/* Check interleaved areas are not overlapping */
		if ((i1->addr1 + i1->size - 1) >= i1->addr2) {
			pr_err("%s: Interleaved areas %#x and \
					%#x are overlapping\n",
					__func__, i1->addr1, i1->addr2);
			return -EINVAL;
		}

		/* Check the interleaved areas are in the physical areas */
		if (pasr_check_interleave_in_physmem(info, i1)) {
			pr_err("%s: Interleaved area %#x/%#x \
					not in physical memory\n",
					__func__, i1->addr1, i1->addr2);
			return -EINVAL;
		}
	}

out:
	return 0;
}

#ifdef DEBUG
static void __init pasr_print_map(struct pasr_map *map)
{
	int i, j;

	if (!map)
		goto out;

	pr_info("PASR map:\n");

	for (i = 0; i < map->nr_dies; i++) {
		struct pasr_die *die = &map->die[i];

		pr_info("Die %d:\n", i);
		for (j = 0; j < die->nr_sections; j++) {
			struct pasr_section *s = &die->section[j];
			pr_info("\tSection %d: @ = %#09llx, Pair = %s @%#09llx\n"
					, j, s->start, s->pair ? "Yes" : "No",
					s->pair ? s->pair->start : 0);
		}
	}
out:
	return;
}
#else
#define pasr_print_map(map) do {} while (0)
#endif /* DEBUG */

static int __init pasr_build_map(struct pasr_info *info, struct pasr_map *map)
{
	int i, j;
	struct pasr_die *die;

	map->nr_dies = info->nr_dies;
	die = map->die;

	for (i = 0; i < info->nr_dies; i++) {
		phys_addr_t addr = info->die[i].addr;
		struct pasr_section *section = die[i].section;

		die[i].start = addr;
		die[i].idx = i;
		die[i].nr_sections = info->die[i].size >> section_bit;

		for (j = 0; j < die[i].nr_sections; j++) {
			section[j].start = addr;
			addr += section_size;
			section[j].die = &die[i];
		}

		die[i].end = addr;
	}

	for (i = 0; i < info->nr_int; i++) {
		struct interleaved_area *ia = &info->int_area[i];
		struct pasr_section *s1, *s2;
		unsigned long offset = 0;

		for (j = 0; j < (ia->size >> section_bit); j++) {
			s1 = pasr_addr2section(map, ia->addr1 + offset);
			s2 = pasr_addr2section(map, ia->addr2 + offset);
			if (!s1 || !s2)
				return -EINVAL;

			offset += section_size;

			s1->pair = s2;
			s2->pair = s1;
		}
	}
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *rootdir;

static int pasr_print_meminfo(struct seq_file *s, void *data)
{
	struct pasr_map *map = &pasr_map;
	unsigned int i, j;

	if (!map)
		return 0;

	for (i = 0; i < map->nr_dies; i++) {
		struct pasr_die *die = &map->die[i];
		seq_printf(s, "die %d\n", i);
		for (j = 0; j < die->nr_sections; j++) {
			struct pasr_section *section = &die->section[j];
			u64 percentage;

			percentage = (u64)section->free_size * 100;
			do_div(percentage, section_size);
			seq_printf(s, "section %d %llu %llu\n", j, section->free_size,
					percentage);
		}
	}
	return 0;
}

static int meminfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, pasr_print_meminfo, inode->i_private);
}

static const struct file_operations meminfo_fops = {
	.open = meminfo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init pasr_init_debug(void)
{
	struct dentry *d;

	rootdir = debugfs_create_dir("pasr", NULL);
	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("meminfo", S_IRUGO, rootdir, (void *)&pasr_map,
				&meminfo_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
late_initcall(pasr_init_debug);
#endif

int __init early_pasr_setup(void)
{
	int ret;

	ret = pasr_info_sanity_check(&pasr_info);
	if (ret) {
		pr_err("PASR info sanity check failed (err %d)\n", ret);
		return ret;
	}

	pasr_print_info(&pasr_info);

	ret = pasr_build_map(&pasr_info, &pasr_map);
	if (ret) {
		pr_err("PASR build map failed (err %d)\n", ret);
		return ret;
	}

	pasr_print_map(&pasr_map);

	ret = pasr_init_core(&pasr_map);

	pr_debug("PASR: First stage init done.\n");

	return ret;
}

/*
 * late_pasr_setup() has to be called after Linux allocator is
 * initialized but before other CPUs are launched.
 */
int __init late_pasr_setup(void)
{
	int i, j;
	struct pasr_section *s;

	for_each_pasr_section(i, j, pasr_map, s) {
		if (!s->lock) {
			s->lock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
			BUG_ON(!s->lock);
			spin_lock_init(s->lock);
			if (s->pair)
				s->pair->lock = s->lock;
		}
	}

	pr_debug("PASR Second stage init done.\n");

	return 0;
}
