/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

struct tee_mmu;
struct mcp_buffer_map;

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf);

/*
 * Delete a used MMU table.
 */
void tee_mmu_delete(struct tee_mmu *mmu);

/*
 * Compare physical addresses from two MMU tables.
 */
bool client_mmu_matches(const struct tee_mmu *left,
			const struct tee_mmu *right);

/*
 * Fill in buffer info for MMU table.
 */
void tee_mmu_buffer(const struct tee_mmu *mmu, struct mcp_buffer_map *map);

/*
 * Add info to debug buffer.
 */
int tee_mmu_debug_structs(struct kasnprintf_buf *buf,
			  const struct tee_mmu *mmu);

#endif /* _TBASE_MEM_H_ */
