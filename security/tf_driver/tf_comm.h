/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TF_COMM_H__
#define __TF_COMM_H__

#include "tf_defs.h"
#include "tf_protocol.h"

/*----------------------------------------------------------------------------
 * Misc
 *----------------------------------------------------------------------------*/

void tf_set_current_time(struct tf_comm *comm);

/*
 * Atomic accesses to 32-bit variables in the L1 Shared buffer
 */
static inline u32 tf_read_reg32(const u32 *comm_buffer)
{
	u32 result;

	__asm__ __volatile__("@ tf_read_reg32\n"
		"ldrex %0, [%1]\n"
		: "=&r" (result)
		: "r" (comm_buffer)
	);

	return result;
}

static inline void tf_write_reg32(void *comm_buffer, u32 value)
{
	u32 tmp;

	__asm__ __volatile__("@ tf_write_reg32\n"
		"1:	ldrex %0, [%2]\n"
		"	strex %0, %1, [%2]\n"
		"	teq   %0, #0\n"
		"	bne   1b"
		: "=&r" (tmp)
		: "r" (value), "r" (comm_buffer)
		: "cc"
	);
}

/*
 * Atomic accesses to 64-bit variables in the L1 Shared buffer
 */
static inline u64 tf_read_reg64(void *comm_buffer)
{
	u64 result;

	__asm__ __volatile__("@ tf_read_reg64\n"
		"ldrexd %0, [%1]\n"
		: "=&r" (result)
		: "r" (comm_buffer)
	);

	return result;
}

static inline void tf_write_reg64(void *comm_buffer, u64 value)
{
	u64 tmp;

	__asm__ __volatile__("@ tf_write_reg64\n"
		"1:	ldrexd %0, [%2]\n"
		"	strexd %0, %1, [%2]\n"
		"	teq    %0, #0\n"
		"	bne    1b"
		: "=&r" (tmp)
		: "r" (value), "r" (comm_buffer)
		: "cc"
	);
}

/*----------------------------------------------------------------------------
 * SMC operations
 *----------------------------------------------------------------------------*/

/* RPC return values */
#define RPC_NO		0x00	/* No RPC to execute */
#define RPC_YIELD	0x01	/* Yield RPC */
#define RPC_NON_YIELD	0x02	/* non-Yield RPC */

int tf_rpc_execute(struct tf_comm *comm);

/*----------------------------------------------------------------------------
 * Shared memory related operations
 *----------------------------------------------------------------------------*/

#define L1_DESCRIPTOR_FAULT            (0x00000000)
#define L2_DESCRIPTOR_FAULT            (0x00000000)

#define L2_DESCRIPTOR_ADDR_MASK         (0xFFFFF000)

#define DESCRIPTOR_V13_12_MASK      (0x3 << PAGE_SHIFT)
#define DESCRIPTOR_V13_12_GET(a)    ((a & DESCRIPTOR_V13_12_MASK) >> PAGE_SHIFT)

struct tf_coarse_page_table *tf_alloc_coarse_page_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	u32 type);

void tf_free_coarse_page_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_coarse_page_table *coarse_pg_table,
	int force);

void tf_init_coarse_page_table_allocator(
	struct tf_coarse_page_table_allocation_context *alloc_context);

void tf_release_coarse_page_table_allocator(
	struct tf_coarse_page_table_allocation_context *alloc_context);

struct page *tf_l2_page_descriptor_to_page(u32 l2_page_descriptor);

u32 tf_get_l2_descriptor_common(u32 vaddr, struct mm_struct *mm);

void tf_cleanup_shared_memory(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_shmem_desc *shmem_desc,
	u32 full_cleanup);

int tf_fill_descriptor_table(
	struct tf_coarse_page_table_allocation_context *alloc_context,
	struct tf_shmem_desc *shmem_desc,
	u32 buffer,
	struct vm_area_struct **vmas,
	u32 descriptors[TF_MAX_COARSE_PAGES],
	u32 buffer_size,
	u32 *buffer_start_offset,
	bool in_user_space,
	u32 flags,
	u32 *descriptor_count);

/*----------------------------------------------------------------------------
 * Standard communication operations
 *----------------------------------------------------------------------------*/

int tf_schedule_secure_world(struct tf_comm *comm);

int tf_send_receive(
	struct tf_comm *comm,
	union tf_command *command,
	union tf_answer *answer,
	struct tf_connection *connection,
	bool bKillable);


/**
 * get a pointer to the secure world description.
 * This points directly into the L1 shared buffer
 * and is valid only once the communication has
 * been initialized
 **/
u8 *tf_get_description(struct tf_comm *comm);

/*----------------------------------------------------------------------------
 * Power management
 *----------------------------------------------------------------------------*/

enum TF_POWER_OPERATION {
	TF_POWER_OPERATION_HIBERNATE = 1,
	TF_POWER_OPERATION_SHUTDOWN = 2,
	TF_POWER_OPERATION_RESUME = 3,
};

int tf_pm_hibernate(struct tf_comm *comm);
int tf_pm_resume(struct tf_comm *comm);
int tf_pm_shutdown(struct tf_comm *comm);

int tf_power_management(struct tf_comm *comm,
	enum TF_POWER_OPERATION operation);


/*----------------------------------------------------------------------------
 * Communication initialization and termination
 *----------------------------------------------------------------------------*/

int tf_init(struct tf_comm *comm);

void tf_terminate(struct tf_comm *comm);


#endif  /* __TF_COMM_H__ */
