/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_MEM_SHARE_H
#define _LINUX_MEM_SHARE_H

#define MEM_SHARE_SERVICE_SVC_ID 0x00000034
#define MEM_SHARE_SERVICE_INS_ID 1
#define MEM_SHARE_SERVICE_VERS 1

#define MEMORY_CMA	1
#define MEMORY_NON_CMA	0
#define MAX_CLIENTS 10
#define GPS	0
#define CHECK	0
#define FREE	1

struct mem_blocks {
	/* Client Id information */
	uint32_t client_id;
	/* Peripheral associated with client */
	uint32_t peripheral;
	/* Sequence Id */
	uint32_t sequence_id;
	/* CMA or Non-CMA region */
	uint32_t memory_type;
	/* Guaranteed Memory */
	uint32_t guarantee;
	/* Memory alloted or not */
	uint32_t alloted;
	/* Size required for client */
	uint32_t size;
	/* start address of the memory block reserved by server memory
	 * subsystem to client
	*/
	phys_addr_t phy_addr;
	/* Virtual address for the physical address allocated
	*/
	void *virtual_addr;
};

int memshare_alloc(struct device *dev,
					unsigned int block_size,
					struct mem_blocks *pblk);
void memshare_free(unsigned int block_size,
					struct mem_blocks *pblk);
#endif /* _LINUX_MEM_SHARE_H */
