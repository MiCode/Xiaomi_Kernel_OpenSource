/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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
void tbase_mmu_delete(struct tbase_mmu *mmu_table);

/*
 * Get type of MMU table.
 */
uint32_t tbase_mmu_type(const struct tbase_mmu *mmu_table);

/*
 * Get physical address for a MMU table.
 */
unsigned long tbase_mmu_phys(const struct tbase_mmu *mmu_table);

#endif /* _TBASE_MEM_H_ */
