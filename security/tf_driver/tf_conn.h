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

#ifndef __TF_CONN_H__
#define __TF_CONN_H__

#include "tf_defs.h"

/*
 * Returns a pointer to the connection referenced by the
 * specified file.
 */
static inline struct tf_connection *tf_conn_from_file(
	struct file *file)
{
	return file->private_data;
}

int tf_validate_shmem_and_flags(u32 shmem, u32 shmem_size, u32 flags);

int tf_map_shmem(
		struct tf_connection *connection,
		u32 buffer,
		/* flags for read-write access rights on the memory */
		u32 flags,
		bool in_user_space,
		u32 descriptors[TF_MAX_COARSE_PAGES],
		u32 *buffer_start_offset,
		u32 buffer_size,
		struct tf_shmem_desc **shmem_desc,
		u32 *descriptor_count);

void tf_unmap_shmem(
		struct tf_connection *connection,
		struct tf_shmem_desc *shmem_desc,
		u32 full_cleanup);

/*----------------------------------------------------------------------------
 * Connection operations to the Secure World
 *----------------------------------------------------------------------------*/

int tf_create_device_context(
	 struct tf_connection *connection);

int tf_destroy_device_context(
	struct tf_connection *connection);

int tf_open_client_session(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

int tf_close_client_session(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

int tf_register_shared_memory(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

int tf_release_shared_memory(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

int tf_invoke_client_command(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

int tf_cancel_client_command(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer);

/*----------------------------------------------------------------------------
 * Connection initialization and cleanup operations
 *----------------------------------------------------------------------------*/

int tf_open(struct tf_device *dev,
	struct file *file,
	struct tf_connection **connection);

void tf_close(
	struct tf_connection *connection);


#endif  /* !defined(__TF_CONN_H__) */
