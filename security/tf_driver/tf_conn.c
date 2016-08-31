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

#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/stddef.h>
#include <linux/types.h>

#include "s_version.h"

#include "tf_protocol.h"
#include "tf_defs.h"
#include "tf_util.h"
#include "tf_comm.h"
#include "tf_conn.h"

#ifdef CONFIG_TF_ZEBRA
#include "tf_zebra.h"
#include "tf_crypto.h"
#endif

#ifdef CONFIG_ANDROID
#define TF_PRIVILEGED_UID_GID 1000 /* Android system AID */
#else
#define TF_PRIVILEGED_UID_GID 0
#endif

/*----------------------------------------------------------------------------
 * Management of the shared memory blocks.
 *
 * Shared memory blocks are the blocks registered through
 * the commands REGISTER_SHARED_MEMORY and POWER_MANAGEMENT
 *----------------------------------------------------------------------------*/

/**
 * Unmaps a shared memory
 **/
void tf_unmap_shmem(
		struct tf_connection *connection,
		struct tf_shmem_desc *shmem_desc,
		u32 full_cleanup)
{
	/* check shmem_desc contains a descriptor */
	if (shmem_desc == NULL)
		return;

	dprintk(KERN_DEBUG "tf_unmap_shmem(%p)\n", shmem_desc);

retry:
	mutex_lock(&(connection->shmem_mutex));
	if (atomic_read(&shmem_desc->ref_count) > 1) {
		/*
		 * Shared mem still in use, wait for other operations completion
		 * before actually unmapping it.
		 */
		dprintk(KERN_INFO "Descriptor in use\n");
		mutex_unlock(&(connection->shmem_mutex));
		schedule();
		goto retry;
	}

	tf_cleanup_shared_memory(
			&(connection->cpt_alloc_context),
			shmem_desc,
			full_cleanup);

	list_del(&(shmem_desc->list));

	if ((shmem_desc->type == TF_SHMEM_TYPE_REGISTERED_SHMEM) ||
			(full_cleanup != 0)) {
		internal_kfree(shmem_desc);

		atomic_dec(&(connection->shmem_count));
	} else {
		/*
		 * This is a preallocated shared memory, add to free list
		 * Since the device context is unmapped last, it is
		 * always the first element of the free list if no
		 * device context has been created
		 */
		shmem_desc->block_identifier = 0;
		list_add(&(shmem_desc->list), &(connection->free_shmem_list));
	}

	mutex_unlock(&(connection->shmem_mutex));
}


/**
 * Find the first available slot for a new block of shared memory
 * and map the user buffer.
 * Update the descriptors to L1 descriptors
 * Update the buffer_start_offset and buffer_size fields
 * shmem_desc is updated to the mapped shared memory descriptor
 **/
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
		u32 *descriptor_count)
{
	struct tf_shmem_desc *desc = NULL;
	int error;

	dprintk(KERN_INFO "tf_map_shmem(%p, %p, flags = 0x%08x)\n",
					connection,
					(void *) buffer,
					flags);

	/*
	 * Added temporary to avoid kernel stack buffer
	 */
	if (!in_user_space) {
		if (object_is_on_stack((void *)buffer) != 0) {
			dprintk(KERN_ERR
				"tf_map_shmem: "
				"kernel stack buffers "
				"(addr=0x%08X) "
				"are not supported",
				buffer);
			error = -ENOSYS;
			goto error;
		}
	}

	mutex_lock(&(connection->shmem_mutex));

	/*
	 * Check the list of free shared memory
	 * is not empty
	 */
	if (list_empty(&(connection->free_shmem_list))) {
		if (atomic_read(&(connection->shmem_count)) ==
				TF_SHMEM_MAX_COUNT) {
			printk(KERN_ERR "tf_map_shmem(%p):"
				" maximum shared memories already registered\n",
				connection);
			error = -ENOMEM;
			goto error;
		}

		/* no descriptor available, allocate a new one */

		desc = (struct tf_shmem_desc *) internal_kmalloc(
			sizeof(*desc), GFP_KERNEL);
		if (desc == NULL) {
			printk(KERN_ERR "tf_map_shmem(%p):"
				" failed to allocate descriptor\n",
				connection);
			error = -ENOMEM;
			goto error;
		}

		/* Initialize the structure */
		desc->type = TF_SHMEM_TYPE_REGISTERED_SHMEM;
		atomic_set(&desc->ref_count, 1);
		INIT_LIST_HEAD(&(desc->list));

		atomic_inc(&(connection->shmem_count));
	} else {
		/* take the first free shared memory descriptor */
		desc = list_first_entry(&(connection->free_shmem_list),
			struct tf_shmem_desc, list);
		list_del(&(desc->list));
	}

	/* Add the descriptor to the used list */
	list_add(&(desc->list), &(connection->used_shmem_list));

	error = tf_fill_descriptor_table(
			&(connection->cpt_alloc_context),
			desc,
			buffer,
			connection->vmas,
			descriptors,
			buffer_size,
			buffer_start_offset,
			in_user_space,
			flags,
			descriptor_count);

	if (error != 0) {
		dprintk(KERN_ERR "tf_map_shmem(%p):"
			" tf_fill_descriptor_table failed with error "
			"code %d!\n",
			connection,
			error);
		goto error;
	}
	desc->client_buffer = (u8 *) buffer;

	/*
	 * Successful completion.
	 */
	*shmem_desc = desc;
	mutex_unlock(&(connection->shmem_mutex));
	dprintk(KERN_DEBUG "tf_map_shmem: success\n");
	return 0;


	/*
	 * Error handling.
	 */
error:
	mutex_unlock(&(connection->shmem_mutex));
	dprintk(KERN_ERR "tf_map_shmem: failure with error code %d\n",
		error);

	tf_unmap_shmem(
			connection,
			desc,
			0);

	return error;
}



/* This function is a copy of the find_vma() function
in linux kernel 2.6.15 version with some fixes :
	- memory block may end on vm_end
	- check the full memory block is in the memory area
	- guarantee NULL is returned if no memory area is found */
struct vm_area_struct *tf_find_vma(struct mm_struct *mm,
	unsigned long addr, unsigned long size)
{
	struct vm_area_struct *vma = NULL;

	dprintk(KERN_INFO
		"tf_find_vma addr=0x%lX size=0x%lX\n", addr, size);

	if (mm) {
		/* Check the cache first. */
		/* (Cache hit rate is typically around 35%.) */
		vma = mm->mmap_cache;
		if (!(vma && vma->vm_end >= (addr+size) &&
				vma->vm_start <= addr))	{
			struct rb_node *rb_node;

			rb_node = mm->mm_rb.rb_node;
			vma = NULL;

			while (rb_node) {
				struct vm_area_struct *vma_tmp;

				vma_tmp = rb_entry(rb_node,
					struct vm_area_struct, vm_rb);

				dprintk(KERN_INFO
					"vma_tmp->vm_start=0x%lX"
					"vma_tmp->vm_end=0x%lX\n",
					vma_tmp->vm_start,
					vma_tmp->vm_end);

				if (vma_tmp->vm_end >= (addr+size)) {
					vma = vma_tmp;
					if (vma_tmp->vm_start <= addr)
						break;

					rb_node = rb_node->rb_left;
				} else {
					rb_node = rb_node->rb_right;
				}
			}

			if (vma)
				mm->mmap_cache = vma;
			if (rb_node == NULL)
				vma = NULL;
		}
	}
	return vma;
}

int tf_validate_shmem_and_flags(
	u32 shmem,
	u32 shmem_size,
	u32 flags)
{
	struct vm_area_struct *vma;
	u32 chunk;

	if (shmem_size == 0)
		/* This is always valid */
		return 0;

	if ((shmem + shmem_size) < shmem)
		/* Overflow */
		return -EINVAL;

	down_read(&current->mm->mmap_sem);

	/*
	 *  When looking for a memory address, split buffer into chunks of
	 *  size=PAGE_SIZE.
	 */
	chunk = PAGE_SIZE - (shmem & (PAGE_SIZE-1));
	if (chunk > shmem_size)
		chunk = shmem_size;

	do {
		vma = tf_find_vma(current->mm, shmem, chunk);

		if (vma == NULL) {
			dprintk(KERN_ERR "%s: area not found\n", __func__);
			goto error;
		}

		if (flags & TF_SHMEM_TYPE_READ)
			if (!(vma->vm_flags & VM_READ)) {
				dprintk(KERN_ERR "%s: no read permission\n",
					__func__);
				goto error;
			}
		if (flags & TF_SHMEM_TYPE_WRITE)
			if (!(vma->vm_flags & VM_WRITE)) {
				dprintk(KERN_ERR "%s: no write permission\n",
					__func__);
				goto error;
			}

		shmem_size -= chunk;
		shmem += chunk;
		chunk = (shmem_size <= PAGE_SIZE ?
				shmem_size : PAGE_SIZE);
	} while (shmem_size != 0);

	up_read(&current->mm->mmap_sem);
	return 0;

error:
	up_read(&current->mm->mmap_sem);
	return -EFAULT;
}


static int tf_map_temp_shmem(struct tf_connection *connection,
	 struct tf_command_param_temp_memref *temp_memref,
	 u32 param_type,
	 struct tf_shmem_desc **shmem_desc)
{
	u32 flags;
	u32 error = S_SUCCESS;
	bool in_user_space = connection->owner != TF_CONNECTION_OWNER_KERNEL;

	dprintk(KERN_INFO "tf_map_temp_shmem(%p, "
		"0x%08x[size=0x%08x], offset=0x%08x)\n",
		connection,
		temp_memref->descriptor,
		temp_memref->size,
		temp_memref->offset);

	switch (param_type) {
	case TF_PARAM_TYPE_MEMREF_TEMP_INPUT:
		flags = TF_SHMEM_TYPE_READ;
		break;
	case TF_PARAM_TYPE_MEMREF_TEMP_OUTPUT:
		flags = TF_SHMEM_TYPE_WRITE;
		break;
	case TF_PARAM_TYPE_MEMREF_TEMP_INOUT:
		flags = TF_SHMEM_TYPE_WRITE | TF_SHMEM_TYPE_READ;
		break;
	default:
		error = -EINVAL;
		goto error;
	}

	if (temp_memref->descriptor == 0) {
		/* NULL tmpref */
		temp_memref->offset = 0;
		*shmem_desc = NULL;
	} else if ((temp_memref->descriptor != 0) &&
			(temp_memref->size == 0)) {
		/* Empty tmpref */
		temp_memref->offset = temp_memref->descriptor;
		temp_memref->descriptor = 0;
		temp_memref->size = 0;
		*shmem_desc = NULL;
	} else {
		/* Map the temp shmem block */

		u32 shared_mem_descriptors[TF_MAX_COARSE_PAGES];
		u32 descriptor_count;

		if (in_user_space) {
			error = tf_validate_shmem_and_flags(
				temp_memref->descriptor,
				temp_memref->size,
				flags);
			if (error != 0)
				goto error;
		}

		error = tf_map_shmem(
				connection,
				temp_memref->descriptor,
				flags,
				in_user_space,
				shared_mem_descriptors,
				&(temp_memref->offset),
				temp_memref->size,
				shmem_desc,
				&descriptor_count);
		temp_memref->descriptor = shared_mem_descriptors[0];
	 }

error:
	 return error;
}

/*
 * Clean up a list of shared memory descriptors.
 */
static void tf_shared_memory_cleanup_list(
		struct tf_connection *connection,
		struct list_head *shmem_desc_list)
{
	while (!list_empty(shmem_desc_list)) {
		struct tf_shmem_desc *shmem_desc;

		shmem_desc = list_first_entry(shmem_desc_list,
			struct tf_shmem_desc, list);

		tf_unmap_shmem(connection, shmem_desc, 1);
	}
}


/*
 * Clean up the shared memory information in the connection.
 * Releases all allocated pages.
 */
static void tf_cleanup_shared_memories(struct tf_connection *connection)
{
	/* clean up the list of used and free descriptors.
	 * done outside the mutex, because tf_unmap_shmem already
	 * mutex()ed
	 */
	tf_shared_memory_cleanup_list(connection,
		&connection->used_shmem_list);
	tf_shared_memory_cleanup_list(connection,
		&connection->free_shmem_list);

	mutex_lock(&(connection->shmem_mutex));

	/* Free the Vmas page */
	if (connection->vmas) {
		internal_free_page((unsigned long) connection->vmas);
		connection->vmas = NULL;
	}

	tf_release_coarse_page_table_allocator(
		&(connection->cpt_alloc_context));

	mutex_unlock(&(connection->shmem_mutex));
}


/*
 * Initialize the shared memory in a connection.
 * Allocates the minimum memory to be provided
 * for shared memory management
 */
int tf_init_shared_memory(struct tf_connection *connection)
{
	int error;
	int i;
	int coarse_page_index;

	/*
	 * We only need to initialize special elements and attempt to allocate
	 * the minimum shared memory descriptors we want to support
	 */

	mutex_init(&(connection->shmem_mutex));
	INIT_LIST_HEAD(&(connection->free_shmem_list));
	INIT_LIST_HEAD(&(connection->used_shmem_list));
	atomic_set(&(connection->shmem_count), 0);

	tf_init_coarse_page_table_allocator(
		&(connection->cpt_alloc_context));


	/*
	 * Preallocate 3 pages to increase the chances that a connection
	 * succeeds in allocating shared mem
	 */
	for (i = 0;
	     i < 3;
	     i++) {
		struct tf_shmem_desc *shmem_desc =
			(struct tf_shmem_desc *) internal_kmalloc(
				sizeof(*shmem_desc), GFP_KERNEL);

		if (shmem_desc == NULL) {
			printk(KERN_ERR "tf_init_shared_memory(%p):"
				" failed to pre allocate descriptor %d\n",
				connection,
				i);
			error = -ENOMEM;
			goto error;
		}

		for (coarse_page_index = 0;
		     coarse_page_index < TF_MAX_COARSE_PAGES;
		     coarse_page_index++) {
			struct tf_coarse_page_table *coarse_pg_table;

			coarse_pg_table = tf_alloc_coarse_page_table(
				&(connection->cpt_alloc_context),
				TF_PAGE_DESCRIPTOR_TYPE_PREALLOCATED);

			if (coarse_pg_table == NULL) {
				printk(KERN_ERR "tf_init_shared_memory(%p)"
					": descriptor %d coarse page %d - "
					"tf_alloc_coarse_page_table() "
					"failed\n",
					connection,
					i,
					coarse_page_index);
				error = -ENOMEM;
				goto error;
			}

			shmem_desc->coarse_pg_table[coarse_page_index] =
				coarse_pg_table;
		}
		shmem_desc->coarse_pg_table_count = 0;

		shmem_desc->type = TF_SHMEM_TYPE_PREALLOC_REGISTERED_SHMEM;
		atomic_set(&shmem_desc->ref_count, 1);

		/*
		 * add this preallocated descriptor to the list of free
		 * descriptors Keep the device context specific one at the
		 * beginning of the list
		 */
		INIT_LIST_HEAD(&(shmem_desc->list));
		list_add_tail(&(shmem_desc->list),
			&(connection->free_shmem_list));
	}

	/* allocate memory for the vmas structure */
	connection->vmas =
		(struct vm_area_struct **) internal_get_zeroed_page(GFP_KERNEL);
	if (connection->vmas == NULL) {
		printk(KERN_ERR "tf_init_shared_memory(%p):"
			" vmas - failed to get_zeroed_page\n",
			connection);
		error = -ENOMEM;
		goto error;
	}

	return 0;

error:
	tf_cleanup_shared_memories(connection);
	return error;
}

/*----------------------------------------------------------------------------
 * Connection operations to the Secure World
 *----------------------------------------------------------------------------*/

int tf_create_device_context(
	struct tf_connection *connection)
{
	union tf_command command;
	union tf_answer  answer;
	int error = 0;

	dprintk(KERN_INFO "tf_create_device_context(%p)\n",
			connection);

	command.create_device_context.message_type =
		TF_MESSAGE_TYPE_CREATE_DEVICE_CONTEXT;
	command.create_device_context.message_size =
		(sizeof(struct tf_command_create_device_context)
			- sizeof(struct tf_command_header))/sizeof(u32);
	command.create_device_context.operation_id = (u32) &answer;
	command.create_device_context.device_context_id = (u32) connection;

	error = tf_send_receive(
		&connection->dev->sm,
		&command,
		&answer,
		connection,
		true);

	if ((error != 0) ||
		(answer.create_device_context.error_code != S_SUCCESS))
		goto error;

	/*
	 * CREATE_DEVICE_CONTEXT succeeded,
	 * store device context handler and update connection status
	 */
	connection->device_context =
		answer.create_device_context.device_context;
	spin_lock(&(connection->state_lock));
	connection->state = TF_CONN_STATE_VALID_DEVICE_CONTEXT;
	spin_unlock(&(connection->state_lock));

	/* successful completion */
	dprintk(KERN_INFO "tf_create_device_context(%p):"
		" device_context=0x%08x\n",
		connection,
		answer.create_device_context.device_context);
	return 0;

error:
	if (error != 0) {
		dprintk(KERN_ERR "tf_create_device_context failed with "
			"error %d\n", error);
	} else {
		/*
		 * We sent a DeviceCreateContext. The state is now
		 * TF_CONN_STATE_CREATE_DEVICE_CONTEXT_SENT It has to be
		 * reset if we ever want to send a DeviceCreateContext again
		 */
		spin_lock(&(connection->state_lock));
		connection->state = TF_CONN_STATE_NO_DEVICE_CONTEXT;
		spin_unlock(&(connection->state_lock));
		dprintk(KERN_ERR "tf_create_device_context failed with "
			"error_code 0x%08X\n",
			answer.create_device_context.error_code);
		if (answer.create_device_context.error_code ==
			S_ERROR_OUT_OF_MEMORY)
			error = -ENOMEM;
		else
			error = -EFAULT;
	}

	return error;
}

/* Check that the current application belongs to the
 * requested GID */
static bool tf_check_gid(gid_t requested_gid)
{
	if (requested_gid == current_egid()) {
		return true;
	} else {
		u32    size;
		u32    i;
		/* Look in the supplementary GIDs */
		get_group_info(GROUP_INFO);
		size = GROUP_INFO->ngroups;
		for (i = 0; i < size; i++)
			if (requested_gid == GROUP_AT(GROUP_INFO , i))
				return true;
	}
	return false;
}

/*
 * Opens a client session to the Secure World
 */
int tf_open_client_session(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;
	struct tf_shmem_desc *shmem_desc[4] = {NULL};
	u32 i;

	dprintk(KERN_INFO "tf_open_client_session(%p)\n", connection);

	/*
	 * Initialize the message size with no login data. This will be later
	 * adjusted the the cases below
	 */
	command->open_client_session.message_size =
		(sizeof(struct tf_command_open_client_session) - 20
			- sizeof(struct tf_command_header))/4;

	switch (command->open_client_session.login_type) {
	case TF_LOGIN_PUBLIC:
		 /* Nothing to do */
		 break;

	case TF_LOGIN_USER:
		/*
		 * Send the EUID of the calling application in the login data.
		 * Update message size.
		 */
		*(u32 *) &command->open_client_session.login_data =
			current_euid();
#ifndef CONFIG_ANDROID
		command->open_client_session.login_type =
			(u32) TF_LOGIN_USER_LINUX_EUID;
#else
		command->open_client_session.login_type =
			(u32) TF_LOGIN_USER_ANDROID_EUID;
#endif

		/* Added one word */
		command->open_client_session.message_size += 1;
		break;

	case TF_LOGIN_GROUP: {
		/* Check requested GID */
		gid_t  requested_gid =
			*(u32 *) command->open_client_session.login_data;

		if (!tf_check_gid(requested_gid)) {
			dprintk(KERN_ERR "tf_open_client_session(%p) "
				"TF_LOGIN_GROUP: requested GID (0x%x) does "
				"not match real eGID (0x%x)"
				"or any of the supplementary GIDs\n",
				connection, requested_gid, current_egid());
			error = -EACCES;
			goto error;
		}
#ifndef CONFIG_ANDROID
		command->open_client_session.login_type =
			TF_LOGIN_GROUP_LINUX_GID;
#else
		command->open_client_session.login_type =
			TF_LOGIN_GROUP_ANDROID_GID;
#endif

		command->open_client_session.message_size += 1; /* GID */
		break;
	}

#ifndef CONFIG_ANDROID
	case TF_LOGIN_APPLICATION: {
		/*
		 * Compute SHA-1 hash of the application fully-qualified path
		 * name.  Truncate the hash to 16 bytes and send it as login
		 * data.  Update message size.
		 */
		u8 pSHA1Hash[SHA1_DIGEST_SIZE];

		error = tf_hash_application_path_and_data(pSHA1Hash,
			NULL, 0);
		if (error != 0) {
			dprintk(KERN_ERR "tf_open_client_session: "
				"error in tf_hash_application_path_and_data\n");
			goto error;
		}
		memcpy(&command->open_client_session.login_data,
			pSHA1Hash, 16);
		command->open_client_session.login_type =
			TF_LOGIN_APPLICATION_LINUX_PATH_SHA1_HASH;
		/* 16 bytes */
		command->open_client_session.message_size += 4;
		break;
	}
#else
	case TF_LOGIN_APPLICATION:
		/*
		 * Send the real UID of the calling application in the login
		 * data. Update message size.
		 */
		*(u32 *) &command->open_client_session.login_data =
			current_uid();

		command->open_client_session.login_type =
			(u32) TF_LOGIN_APPLICATION_ANDROID_UID;

		/* Added one word */
		command->open_client_session.message_size += 1;
		break;
#endif

#ifndef CONFIG_ANDROID
	case TF_LOGIN_APPLICATION_USER: {
		/*
		 * Compute SHA-1 hash of the concatenation of the application
		 * fully-qualified path name and the EUID of the calling
		 * application.  Truncate the hash to 16 bytes and send it as
		 * login data.  Update message size.
		 */
		u8 pSHA1Hash[SHA1_DIGEST_SIZE];

		error = tf_hash_application_path_and_data(pSHA1Hash,
			(u8 *) &(current_euid()), sizeof(current_euid()));
		if (error != 0) {
			dprintk(KERN_ERR "tf_open_client_session: "
				"error in tf_hash_application_path_and_data\n");
			goto error;
		}
		memcpy(&command->open_client_session.login_data,
			pSHA1Hash, 16);
		command->open_client_session.login_type =
			TF_LOGIN_APPLICATION_USER_LINUX_PATH_EUID_SHA1_HASH;

		/* 16 bytes */
		command->open_client_session.message_size += 4;

		break;
	}
#else
	case TF_LOGIN_APPLICATION_USER:
		/*
		 * Send the real UID and the EUID of the calling application in
		 * the login data. Update message size.
		 */
		*(u32 *) &command->open_client_session.login_data =
			current_uid();
		*(u32 *) &command->open_client_session.login_data[4] =
			current_euid();

		command->open_client_session.login_type =
			TF_LOGIN_APPLICATION_USER_ANDROID_UID_EUID;

		/* Added two words */
		command->open_client_session.message_size += 2;
		break;
#endif

#ifndef CONFIG_ANDROID
	case TF_LOGIN_APPLICATION_GROUP: {
		/*
		 * Check requested GID.  Compute SHA-1 hash of the concatenation
		 * of the application fully-qualified path name and the
		 * requested GID.  Update message size
		 */
		gid_t  requested_gid;
		u8     pSHA1Hash[SHA1_DIGEST_SIZE];

		requested_gid =	*(u32 *) &command->open_client_session.
			login_data;

		if (!tf_check_gid(requested_gid)) {
			dprintk(KERN_ERR "tf_open_client_session(%p) "
			"TF_LOGIN_APPLICATION_GROUP: requested GID (0x%x) "
			"does not match real eGID (0x%x)"
			"or any of the supplementary GIDs\n",
			connection, requested_gid, current_egid());
			error = -EACCES;
			goto error;
		}

		error = tf_hash_application_path_and_data(pSHA1Hash,
			&requested_gid, sizeof(u32));
		if (error != 0) {
			dprintk(KERN_ERR "tf_open_client_session: "
				"error in tf_hash_application_path_and_data\n");
			goto error;
		}

		memcpy(&command->open_client_session.login_data,
			pSHA1Hash, 16);
		command->open_client_session.login_type =
			TF_LOGIN_APPLICATION_GROUP_LINUX_PATH_GID_SHA1_HASH;

		/* 16 bytes */
		command->open_client_session.message_size += 4;
		break;
	}
#else
	case TF_LOGIN_APPLICATION_GROUP: {
		/*
		 * Check requested GID. Send the real UID and the requested GID
		 * in the login data. Update message size.
		 */
		gid_t requested_gid;

		requested_gid =	*(u32 *) &command->open_client_session.
			login_data;

		if (!tf_check_gid(requested_gid)) {
			dprintk(KERN_ERR "tf_open_client_session(%p) "
			"TF_LOGIN_APPLICATION_GROUP: requested GID (0x%x) "
			"does not match real eGID (0x%x)"
			"or any of the supplementary GIDs\n",
			connection, requested_gid, current_egid());
			error = -EACCES;
			goto error;
		}

		*(u32 *) &command->open_client_session.login_data =
			current_uid();
		*(u32 *) &command->open_client_session.login_data[4] =
			requested_gid;

		command->open_client_session.login_type =
			TF_LOGIN_APPLICATION_GROUP_ANDROID_UID_GID;

		/* Added two words */
		command->open_client_session.message_size += 2;

		break;
	}
#endif

	case TF_LOGIN_PRIVILEGED:
		/* A privileged login may be performed only on behalf of the
		   kernel itself or on behalf of a process with euid=0 or
		   egid=0 or euid=system or egid=system. */
		if (connection->owner == TF_CONNECTION_OWNER_KERNEL) {
			dprintk(KERN_DEBUG "tf_open_client_session: "
				"TF_LOGIN_PRIVILEGED for kernel API\n");
		} else if ((current_euid() != TF_PRIVILEGED_UID_GID) &&
			   (current_egid() != TF_PRIVILEGED_UID_GID) &&
			   (current_euid() != 0) && (current_egid() != 0)) {
			dprintk(KERN_ERR "tf_open_client_session: "
				" user %d, group %d not allowed to open "
				"session with TF_LOGIN_PRIVILEGED\n",
				current_euid(), current_egid());
			error = -EACCES;
			goto error;
		} else {
			dprintk(KERN_DEBUG "tf_open_client_session: "
				"TF_LOGIN_PRIVILEGED for %u:%u\n",
				current_euid(), current_egid());
		}
		command->open_client_session.login_type =
			TF_LOGIN_PRIVILEGED;
		break;

	case TF_LOGIN_AUTHENTICATION: {
		/*
		 * Compute SHA-1 hash of the application binary
		 * Send this hash as the login data (20 bytes)
		 */

		u8 *hash;
		hash = &(command->open_client_session.login_data[0]);

		error = tf_get_current_process_hash(hash);
		if (error != 0) {
			dprintk(KERN_ERR "tf_open_client_session: "
				"error in tf_get_current_process_hash\n");
			goto error;
		}
		command->open_client_session.login_type =
			TF_LOGIN_AUTHENTICATION_BINARY_SHA1_HASH;

		/* 20 bytes */
		command->open_client_session.message_size += 5;
		break;
	}

	case TF_LOGIN_PRIVILEGED_KERNEL:
		/* A kernel login may be performed only on behalf of the
		   kernel itself. */
		if (connection->owner == TF_CONNECTION_OWNER_KERNEL) {
			dprintk(KERN_DEBUG "tf_open_client_session: "
				"TF_LOGIN_PRIVILEGED_KERNEL for kernel API\n");
			command->open_client_session.login_type =
				TF_LOGIN_PRIVILEGED_KERNEL;
		} else {
			dprintk(KERN_ERR "tf_open_client_session: "
				" user %d, group %d not allowed to open "
				"session with TF_LOGIN_PRIVILEGED_KERNEL\n",
				current_euid(), current_egid());
			error = -EACCES;
			goto error;
		}
		command->open_client_session.login_type =
			TF_LOGIN_PRIVILEGED_KERNEL;
		break;

	default:
		 dprintk(KERN_ERR "tf_open_client_session: "
			"unknown login_type(%08X)\n",
			command->open_client_session.login_type);
		 error = -EOPNOTSUPP;
		 goto error;
	}

	/* Map the temporary memory references */
	for (i = 0; i < 4; i++) {
		int param_type;
		param_type = TF_GET_PARAM_TYPE(
			command->open_client_session.param_types, i);
		if ((param_type & (TF_PARAM_TYPE_MEMREF_FLAG |
				   TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG))
				== TF_PARAM_TYPE_MEMREF_FLAG) {
			/* Map temp mem ref */
			error = tf_map_temp_shmem(connection,
				&command->open_client_session.
					params[i].temp_memref,
				param_type,
				&shmem_desc[i]);
			if (error != 0) {
				dprintk(KERN_ERR "tf_open_client_session: "
					"unable to map temporary memory block "
					"(%08X)\n", error);
				goto error;
			}
		}
	}

	/* Fill the handle of the Device Context */
	command->open_client_session.device_context =
		connection->device_context;

	error = tf_send_receive(
		&connection->dev->sm,
		command,
		answer,
		connection,
		true);

error:
	/* Unmap the temporary memory references */
	for (i = 0; i < 4; i++)
		if (shmem_desc[i] != NULL)
			tf_unmap_shmem(connection, shmem_desc[i], 0);

	if (error != 0)
		dprintk(KERN_ERR "tf_open_client_session returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_open_client_session returns "
			"error_code 0x%08X\n",
			answer->open_client_session.error_code);

	return error;
}


/*
 * Closes a client session from the Secure World
 */
int tf_close_client_session(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;

	dprintk(KERN_DEBUG "tf_close_client_session(%p)\n", connection);

	command->close_client_session.message_size =
		(sizeof(struct tf_command_close_client_session) -
			sizeof(struct tf_command_header)) / 4;
	command->close_client_session.device_context =
		connection->device_context;

	error = tf_send_receive(
		&connection->dev->sm,
		command,
		answer,
		connection,
		true);

	if (error != 0)
		dprintk(KERN_ERR "tf_close_client_session returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_close_client_session returns "
			"error 0x%08X\n",
			answer->close_client_session.error_code);

	return error;
}


/*
 * Registers a shared memory to the Secure World
 */
int tf_register_shared_memory(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;
	struct tf_shmem_desc *shmem_desc = NULL;
	bool in_user_space = connection->owner != TF_CONNECTION_OWNER_KERNEL;
	struct tf_command_register_shared_memory *msg =
		&command->register_shared_memory;

	dprintk(KERN_INFO "tf_register_shared_memory(%p) "
		"%p[0x%08X][0x%08x]\n",
		connection,
		(void *)msg->shared_mem_descriptors[0],
		msg->shared_mem_size,
		(u32)msg->memory_flags);

	if (in_user_space) {
		error = tf_validate_shmem_and_flags(
			msg->shared_mem_descriptors[0],
			msg->shared_mem_size,
			(u32)msg->memory_flags);
		if (error != 0)
			goto error;
	}

	/* Initialize message_size with no descriptors */
	msg->message_size
		= (offsetof(struct tf_command_register_shared_memory,
						shared_mem_descriptors) -
			sizeof(struct tf_command_header)) / 4;

	/* Map the shmem block and update the message */
	if (msg->shared_mem_size == 0) {
		/* Empty shared mem */
		msg->shared_mem_start_offset = msg->shared_mem_descriptors[0];
	} else {
		u32 descriptor_count;
		error = tf_map_shmem(
			connection,
			msg->shared_mem_descriptors[0],
			msg->memory_flags,
			in_user_space,
			msg->shared_mem_descriptors,
			&(msg->shared_mem_start_offset),
			msg->shared_mem_size,
			&shmem_desc,
			&descriptor_count);
		if (error != 0) {
			dprintk(KERN_ERR "tf_register_shared_memory: "
				"unable to map shared memory block\n");
			goto error;
		}
		msg->message_size += descriptor_count;
	}

	/*
	 * write the correct device context handle and the address of the shared
	 * memory descriptor in the message
	 */
	msg->device_context = connection->device_context;
	msg->block_id = (u32)shmem_desc;

	/* Send the updated message */
	error = tf_send_receive(
		&connection->dev->sm,
		command,
		answer,
		connection,
		true);

	if ((error != 0) ||
		(answer->register_shared_memory.error_code
			!= S_SUCCESS)) {
		dprintk(KERN_ERR "tf_register_shared_memory: "
			"operation failed. Unmap block\n");
		goto error;
	}

	/* Saves the block handle returned by the secure world */
	if (shmem_desc != NULL)
		shmem_desc->block_identifier =
			answer->register_shared_memory.block;

	/* successful completion */
	dprintk(KERN_INFO "tf_register_shared_memory(%p):"
		" block_id=0x%08x block=0x%08x\n",
		connection, msg->block_id,
		answer->register_shared_memory.block);
	return 0;

	/* error completion */
error:
	tf_unmap_shmem(
		connection,
		shmem_desc,
		0);

	if (error != 0)
		dprintk(KERN_ERR "tf_register_shared_memory returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_register_shared_memory returns "
			"error_code 0x%08X\n",
			answer->register_shared_memory.error_code);

	return error;
}


/*
 * Releases a shared memory from the Secure World
 */
int tf_release_shared_memory(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;

	dprintk(KERN_DEBUG "tf_release_shared_memory(%p)\n", connection);

	command->release_shared_memory.message_size =
		(sizeof(struct tf_command_release_shared_memory) -
			sizeof(struct tf_command_header)) / 4;
	command->release_shared_memory.device_context =
		connection->device_context;

	error = tf_send_receive(
		&connection->dev->sm,
		command,
		answer,
		connection,
		true);

	if ((error != 0) ||
		(answer->release_shared_memory.error_code != S_SUCCESS))
		goto error;

	/* Use block_id to get back the pointer to shmem_desc */
	tf_unmap_shmem(
		connection,
		(struct tf_shmem_desc *)
			answer->release_shared_memory.block_id,
		0);

	/* successful completion */
	dprintk(KERN_INFO "tf_release_shared_memory(%p):"
		" block_id=0x%08x block=0x%08x\n",
		connection, answer->release_shared_memory.block_id,
		command->release_shared_memory.block);
	return 0;


error:
	if (error != 0)
		dprintk(KERN_ERR "tf_release_shared_memory returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_release_shared_memory returns "
			"nChannelStatus 0x%08X\n",
			answer->release_shared_memory.error_code);

	return error;

}

/*
 * Invokes a client command to the Secure World
 */
int tf_invoke_client_command(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;
	struct tf_shmem_desc *shmem_desc[4] = {NULL};
	int i;
#ifdef CONFIG_TF_ION
	struct ion_handle *new_handle = NULL;
#endif /* CONFIG_TF_ION */

	dprintk(KERN_INFO "tf_invoke_client_command(%p)\n", connection);

	command->release_shared_memory.message_size =
		(sizeof(struct tf_command_invoke_client_command) -
			sizeof(struct tf_command_header)) / 4;

#ifdef CONFIG_TF_ZEBRA
	error = tf_crypto_try_shortcuted_update(connection,
		(struct tf_command_invoke_client_command *) command,
		(struct tf_answer_invoke_client_command *) answer);
	if (error == 0)
		return error;
#endif

	/* Map the tmprefs */
	for (i = 0; i < 4; i++) {
		int param_type = TF_GET_PARAM_TYPE(
			command->invoke_client_command.param_types, i);

		if ((param_type & (TF_PARAM_TYPE_MEMREF_FLAG |
					TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG))
				== TF_PARAM_TYPE_MEMREF_FLAG) {
			/* A temporary memref: map it */
			error = tf_map_temp_shmem(connection,
					&command->invoke_client_command.
						params[i].temp_memref,
					param_type, &shmem_desc[i]);
			if (error != 0) {
				dprintk(KERN_ERR
					"tf_invoke_client_command: "
					"unable to map temporary memory "
					"block\n (%08X)", error);
				goto error;
			}
		}
#ifdef CONFIG_TF_ION
		else if (param_type == TF_PARAM_TYPE_MEMREF_ION_HANDLE) {
			struct tf_command_invoke_client_command *invoke;
			ion_phys_addr_t ion_addr;
			size_t ion_len;
			struct ion_buffer *buffer;

			if (connection->ion_client == NULL) {
				connection->ion_client = ion_client_create(
					zebra_ion_device,
					(1 << ION_HEAP_TYPE_CARVEOUT),
					"tf");
			}
			if (connection->ion_client == NULL) {
				dprintk(KERN_ERR "%s(%p): "
					"unable to create ion client\n",
					__func__, connection);
				error = -EFAULT;
				goto error;
			}

			invoke = &command->invoke_client_command;

			dprintk(KERN_INFO "ion_handle %x",
				invoke->params[i].value.a);
			buffer = ion_share(connection->ion_client,
				(struct ion_handle *)invoke->params[i].value.a);
			if (buffer == NULL) {
				dprintk(KERN_ERR "%s(%p): "
					"unable to share ion handle\n",
					__func__, connection);
				error = -EFAULT;
				goto error;
			}

			dprintk(KERN_INFO "ion_buffer %p", buffer);
			new_handle = ion_import(connection->ion_client, buffer);
			if (new_handle == NULL) {
				dprintk(KERN_ERR "%s(%p): "
					"unable to import ion buffer\n",
					__func__, connection);
				error = -EFAULT;
				goto error;
			}

			dprintk(KERN_INFO "new_handle %x", new_handle);
			error = ion_phys(connection->ion_client,
					new_handle,
					&ion_addr,
					&ion_len);
			if (error) {
				dprintk(KERN_ERR
				"%s: unable to convert ion handle "
				"0x%08X (error code 0x%08X)\n",
				__func__,
				new_handle,
				error);
				error = -EINVAL;
				goto error;
			}
			dprintk(KERN_INFO
			"%s: handle=0x%08x phys_add=0x%08x length=0x%08x\n",
			__func__, invoke->params[i].value.a, ion_addr, ion_len);

			invoke->params[i].value.a = (u32) ion_addr;
			invoke->params[i].value.b = (u32) ion_len;

			invoke->param_types &= ~((0xF) << (4*i));
			invoke->param_types |=
				TF_PARAM_TYPE_VALUE_INPUT << (4*i);
		}
#endif /* CONFIG_TF_ION */
	}

	command->invoke_client_command.device_context =
		connection->device_context;

	error = tf_send_receive(&connection->dev->sm, command,
		answer, connection, true);

error:
#ifdef CONFIG_TF_ION
	if (new_handle != NULL)
		ion_free(connection->ion_client, new_handle);
#endif /* CONFIG_TF_ION */
	/* Unmap de temp mem refs */
	for (i = 0; i < 4; i++) {
		if (shmem_desc[i] != NULL) {
			dprintk(KERN_INFO "tf_invoke_client_command: "
				"UnMatemp_memref %d\n ", i);

			tf_unmap_shmem(connection, shmem_desc[i], 0);
		}
	}

	if (error != 0)
		dprintk(KERN_ERR "tf_invoke_client_command returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_invoke_client_command returns "
			"error_code 0x%08X\n",
			answer->invoke_client_command.error_code);

	return error;
}


/*
 * Cancels a client command from the Secure World
 */
int tf_cancel_client_command(
	struct tf_connection *connection,
	union tf_command *command,
	union tf_answer *answer)
{
	int error = 0;

	dprintk(KERN_DEBUG "tf_cancel_client_command(%p)\n", connection);

	command->cancel_client_operation.device_context =
		connection->device_context;
	command->cancel_client_operation.message_size =
		(sizeof(struct tf_command_cancel_client_operation) -
			sizeof(struct tf_command_header)) / 4;

	error = tf_send_receive(
		&connection->dev->sm,
		command,
		answer,
		connection,
		true);

	if ((error != 0) ||
		(answer->cancel_client_operation.error_code != S_SUCCESS))
		goto error;


	/* successful completion */
	return 0;

error:
	if (error != 0)
		dprintk(KERN_ERR "tf_cancel_client_command returns %d\n",
			error);
	else
		dprintk(KERN_ERR "tf_cancel_client_command returns "
			"nChannelStatus 0x%08X\n",
			answer->cancel_client_operation.error_code);

	return error;
}



/*
 * Destroys a device context from the Secure World
 */
int tf_destroy_device_context(
	struct tf_connection *connection)
{
	int error;
	/*
	 * AFY: better use the specialized tf_command_destroy_device_context
	 * structure: this will save stack
	 */
	union tf_command command;
	union tf_answer answer;

	dprintk(KERN_INFO "tf_destroy_device_context(%p)\n", connection);

	BUG_ON(connection == NULL);

	command.header.message_type = TF_MESSAGE_TYPE_DESTROY_DEVICE_CONTEXT;
	command.header.message_size =
		(sizeof(struct tf_command_destroy_device_context) -
			sizeof(struct tf_command_header))/sizeof(u32);

	/*
	 * fill in the device context handler
	 * it is guarantied that the first shared memory descriptor describes
	 * the device context
	 */
	command.destroy_device_context.device_context =
		connection->device_context;

	error = tf_send_receive(
		&connection->dev->sm,
		&command,
		&answer,
		connection,
		false);

	if ((error != 0) ||
		(answer.destroy_device_context.error_code != S_SUCCESS))
		goto error;

	spin_lock(&(connection->state_lock));
	connection->state = TF_CONN_STATE_NO_DEVICE_CONTEXT;
	spin_unlock(&(connection->state_lock));

	/* successful completion */
	dprintk(KERN_INFO "tf_destroy_device_context(%p)\n",
		connection);
	return 0;

error:
	if (error != 0) {
		dprintk(KERN_ERR "tf_destroy_device_context failed with "
			"error %d\n", error);
	} else {
		dprintk(KERN_ERR "tf_destroy_device_context failed with "
			"error_code 0x%08X\n",
			answer.destroy_device_context.error_code);
		if (answer.destroy_device_context.error_code ==
			S_ERROR_OUT_OF_MEMORY)
			error = -ENOMEM;
		else
			error = -EFAULT;
	}

	return error;
}


/*----------------------------------------------------------------------------
 * Connection initialization and cleanup operations
 *----------------------------------------------------------------------------*/

/*
 * Opens a connection to the specified device.
 *
 * The placeholder referenced by connection is set to the address of the
 * new connection; it is set to NULL upon failure.
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
int tf_open(struct tf_device *dev,
	struct file *file,
	struct tf_connection **connection)
{
	int error;
	struct tf_connection *conn = NULL;

	dprintk(KERN_INFO "tf_open(%p, %p)\n", file, connection);

	/*
	 * Allocate and initialize the conn.
	 * kmalloc only allocates sizeof(*conn) virtual memory
	 */
	conn = (struct tf_connection *) internal_kmalloc(sizeof(*conn),
		GFP_KERNEL);
	if (conn == NULL) {
		printk(KERN_ERR "tf_open(): "
			"Out of memory for conn!\n");
		error = -ENOMEM;
		goto error;
	}

	memset(conn, 0, sizeof(*conn));

	conn->state = TF_CONN_STATE_NO_DEVICE_CONTEXT;
	conn->dev = dev;
	spin_lock_init(&(conn->state_lock));
	atomic_set(&(conn->pending_op_count), 0);
	INIT_LIST_HEAD(&(conn->list));

	/*
	 * Initialize the shared memory
	 */
	error = tf_init_shared_memory(conn);
	if (error != 0)
		goto error;

#ifdef CONFIG_TF_ZEBRA
	/*
	 * Initialize CUS specifics
	 */
	tf_crypto_init_cus(conn);
#endif

	/*
	 * Attach the conn to the device.
	 */
	spin_lock(&(dev->connection_list_lock));
	list_add(&(conn->list), &(dev->connection_list));
	spin_unlock(&(dev->connection_list_lock));

	/*
	 * Successful completion.
	 */

	*connection = conn;

	dprintk(KERN_INFO "tf_open(): Success (conn=%p)\n", conn);
	return 0;

	/*
	 * Error handling.
	 */

error:
	dprintk(KERN_ERR "tf_open(): Failure (error %d)\n", error);
	/* Deallocate the descriptor pages if necessary */
	internal_kfree(conn);
	*connection = NULL;
	return error;
}


/*
 * Closes the specified connection.
 *
 * Upon return, the connection has been destroyed and cannot be used anymore.
 *
 * This function does nothing if connection is set to NULL.
 */
void tf_close(struct tf_connection *connection)
{
	int error;
	enum TF_CONN_STATE state;

	dprintk(KERN_DEBUG "tf_close(%p)\n", connection);

	if (connection == NULL)
		return;

	/*
	 * Assumption: Linux guarantees that no other operation is in progress
	 * and that no other operation will be started when close is called
	 */
	BUG_ON(atomic_read(&(connection->pending_op_count)) != 0);

	/*
	 * Exchange a Destroy Device Context message if needed.
	 */
	spin_lock(&(connection->state_lock));
	state = connection->state;
	spin_unlock(&(connection->state_lock));
	if (state == TF_CONN_STATE_VALID_DEVICE_CONTEXT) {
		/*
		 * A DestroyDeviceContext operation was not performed. Do it
		 * now.
		 */
		error = tf_destroy_device_context(connection);
		if (error != 0)
			/* avoid cleanup if destroy device context fails */
			goto error;
	}

	/*
	 * Clean up the shared memory
	 */
	tf_cleanup_shared_memories(connection);

#ifdef CONFIG_TF_ION
	if (connection->ion_client != NULL)
		ion_client_destroy(connection->ion_client);
#endif

	spin_lock(&(connection->dev->connection_list_lock));
	list_del(&(connection->list));
	spin_unlock(&(connection->dev->connection_list_lock));

	internal_kfree(connection);

	return;

error:
	dprintk(KERN_DEBUG "tf_close(%p) failed with error code %d\n",
		connection, error);
}
