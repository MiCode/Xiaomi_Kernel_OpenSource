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

struct tbase_mmu;
struct mcp_buffer_map;

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tbase_mmu *tbase_mmu_create(struct task_struct *task,
				   const void *wsm_buffer,
				   unsigned int wsm_len);

/*
 * Delete a used MMU table.
 */
void tbase_mmu_delete(struct tbase_mmu *mmu);

/*
 * Fill in buffer info for MMU table.
 */
void tbase_mmu_buffer(const struct tbase_mmu *mmu, struct mcp_buffer_map *map);

/*
 * Add info to debug buffer.
 */
int tbase_mmu_info(const struct tbase_mmu *mmu, struct kasnprintf_buf *buf);

#endif /* _TBASE_MEM_H_ */
