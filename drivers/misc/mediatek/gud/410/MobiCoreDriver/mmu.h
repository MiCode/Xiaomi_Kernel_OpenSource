/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TBASE_MEM_H_
#define _TBASE_MEM_H_

/*
 * This represents the maximum number of entries in a Page Table Entries
 * array which maps one 4KiB page. Each entry is 64 bits long physical
 * address with some possible flags. With 512 entries it is possible
 * to map 2MiB memory block.
 */
#define PTE_ENTRIES_MAX	512

/*
 * This represents the maximum number of entries in a Page Middle Directory
 * which maps one 4KiB page. Each entry is a 64 bits physical address that
 * points to a PTE. With 512 entries t is possible to map 1GB memory block.
 */
#define PMD_ENTRIES_MAX	512

struct tee_mmu;
struct mcp_buffer_map;

struct tee_deleter {
	void *object;
	void (*delete)(void *object);
};

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf);

/*
 * Allocate MMU table and map pages into it.
 * This is for Xen Dom0 to re-create a buffer with existing pages.
 */
struct tee_mmu *tee_mmu_wrap(struct tee_deleter *deleter, struct page **pages,
			     const struct mcp_buffer_map *buf);

/*
 * Give the MMU an object to release when released
 */
void tee_mmu_set_deleter(struct tee_mmu *mmu, struct tee_deleter *deleter);

/*
 * Gets a reference on a MMU table.
 */
void tee_mmu_get(struct tee_mmu *mmu);

/*
 * Puts a reference on a MMU table.
 */
void tee_mmu_put(struct tee_mmu *mmu);

/*
 * Fill in buffer info for MMU table.
 */
void tee_mmu_buffer(struct tee_mmu *mmu, struct mcp_buffer_map *map);

/*
 * Add info to debug buffer.
 */
int tee_mmu_debug_structs(struct kasnprintf_buf *buf,
			  const struct tee_mmu *mmu);

#endif /* _TBASE_MEM_H_ */
