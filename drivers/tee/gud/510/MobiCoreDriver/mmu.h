/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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

#ifndef MC_MMU_H
#define MC_MMU_H

struct tee_mmu;
struct mcp_buffer_map;
struct mc_ioctl_buffer;

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf);

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

#endif /* MC_MMU_H */
