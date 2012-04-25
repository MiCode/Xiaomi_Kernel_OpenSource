/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <mach/ocmem_priv.h>

static inline int check_id(int id)
{
	return (id < OCMEM_CLIENT_MAX && id >= OCMEM_GRAPHICS);
}

static struct ocmem_handle *generate_handle(void)
{
	struct ocmem_handle *handle = NULL;

	handle = kzalloc(sizeof(struct ocmem_handle), GFP_KERNEL);
	if (!handle) {
		pr_err("ocmem: Unable to generate buffer handle\n");
		return NULL;
	}
	mutex_init(&handle->handle_mutex);
	return handle;
}

static int free_handle(struct ocmem_handle *handle)
{
	if (!handle)
		return -EINVAL;

	mutex_destroy(&handle->handle_mutex);
	kfree(handle);
	handle = NULL;
	return 0;
}

static int __ocmem_free(int id, struct ocmem_buf *buf)
{
	int ret = 0;
	struct ocmem_handle *handle = buffer_to_handle(buf);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_mutex);
	ret = process_free(id, handle);
	mutex_unlock(&handle->handle_mutex);

	if (ret)
		return -EINVAL;

	free_handle(handle);
	return 0;
}

static struct ocmem_buf *__ocmem_allocate_range(int id, unsigned long min,
		unsigned long max, unsigned long step, bool block, bool wait)
{
	struct ocmem_handle *handle = NULL;
	int ret = 0;

	handle = generate_handle();
	if (!handle) {
		pr_err("ocmem: Unable to generate handle\n");
		return NULL;
	}

	mutex_lock(&handle->handle_mutex);
	ret = process_allocate(id, handle, min, max, step, block, wait);
	mutex_unlock(&handle->handle_mutex);
	if (ret) {
		pr_err("ocmem allocation failed\n");
		free_handle(handle);
		return NULL;
	} else
		return handle_to_buffer(handle);
}

struct ocmem_buf *ocmem_allocate(int client_id, unsigned long size)
{
	bool can_block = false;
	bool can_wait = true;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	if (size < OCMEM_MIN_ALLOC) {
		pr_err("ocmem: requested size %lx must be at least %x\n",
				size, OCMEM_MIN_ALLOC);
		return NULL;
	}

	if (!IS_ALIGNED(size, OCMEM_MIN_ALIGN)) {
		pr_err("ocmem: Invalid alignment, size must be %x aligned\n",
				OCMEM_MIN_ALIGN);
		return NULL;
	}

	return __ocmem_allocate_range(client_id, size, size,
					size, can_block, can_wait);
}

struct ocmem_buf *ocmem_allocate_nowait(int client_id, unsigned long size)
{
	bool can_block = false;
	bool can_wait = false;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	if (size < OCMEM_MIN_ALLOC) {
		pr_err("ocmem: requested size %lx must be at least %x\n",
				size, OCMEM_MIN_ALLOC);
		return NULL;
	}

	if (!IS_ALIGNED(size, OCMEM_MIN_ALIGN)) {
		pr_err("ocmem: Invalid alignment, size must be %x aligned\n",
				OCMEM_MIN_ALIGN);
		return NULL;
	}
	return __ocmem_allocate_range(client_id, size, size,
					size, can_block, can_wait);
}

struct ocmem_buf *ocmem_allocate_range(int client_id, unsigned long min,
		unsigned long goal, unsigned long step)
{
	bool can_block = true;
	bool can_wait = false;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	/* Asynchronous API requires notifier registration */
	if (!check_notifier(client_id)) {
		pr_err("ocmem: No notifier registered for client %d\n",
				client_id);
		return NULL;
	}

	if (min < OCMEM_MIN_ALLOC) {
		pr_err("ocmem: requested min size %lx must be at least %x\n",
				min, OCMEM_MIN_ALLOC);
		return NULL;
	}

	if (!IS_ALIGNED(min | goal | step, OCMEM_MIN_ALIGN)) {
		pr_err("ocmem: Invalid alignment, args must be %x aligned\n",
				OCMEM_MIN_ALIGN);
		return NULL;
	}

	return __ocmem_allocate_range(client_id, min, goal,
				step, can_block, can_wait);
}

struct ocmem_buf *ocmem_allocate_nb(int client_id, unsigned long size)
{
	bool can_block = true;
	bool can_wait = false;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	/* Asynchronous API requires notifier registration */
	if (!check_notifier(client_id)) {
		pr_err("ocmem: No notifier registered for client %d\n",
				client_id);
		return NULL;
	}

	if (size < OCMEM_MIN_ALLOC) {
		pr_err("ocmem: requested size %lx must be at least %x\n",
				size, OCMEM_MIN_ALLOC);
		return NULL;
	}

	if (!IS_ALIGNED(size, OCMEM_MIN_ALIGN)) {
		pr_err("ocmem: Invalid alignment, args must be %x aligned\n",
				OCMEM_MIN_ALIGN);
		return NULL;
	}

	return __ocmem_allocate_range(client_id, 0, size, size,
						can_block, can_wait);

}

int ocmem_free(int client_id, struct ocmem_buf *buffer)
{
	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	return __ocmem_free(client_id, buffer);
}

int pre_validate_chunk_list(struct ocmem_map_list *list)
{
	int i = 0;
	struct ocmem_chunk *chunks;

	if (!list)
		return -EINVAL;

	if (list->num_chunks > OCMEM_MAX_CHUNKS || list->num_chunks == 0)
		return -EINVAL;

	chunks = list->chunks;

	if (!chunks)
		return -EINVAL;

	for (i = 0; i < list->num_chunks; i++) {
		if (!chunks[i].ddr_paddr ||
			chunks[i].size < MIN_CHUNK_SIZE)
			return -EINVAL;
	}
	return 0;
}

int ocmem_map(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list)
{
	int ret = 0;
	struct ocmem_handle *handle = NULL;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	/* Asynchronous API requires notifier registration */
	if (!check_notifier(client_id)) {
		pr_err("ocmem: No notifier registered for client %d\n",
				client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	if (!pre_validate_chunk_list(list))
		return -EINVAL;

	handle = buffer_to_handle(buffer);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_mutex);
	ret = process_xfer(client_id, handle, list, TO_OCMEM);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}

int ocmem_unmap(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list)
{

	int ret = 0;
	struct ocmem_handle *handle = NULL;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	/* Asynchronous API requires notifier registration */
	if (!check_notifier(client_id)) {
		pr_err("ocmem: No notifier registered for client %d\n",
				client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	if (!pre_validate_chunk_list(list))
		return -EINVAL;

	handle = buffer_to_handle(buffer);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_mutex);
	ret = process_xfer(client_id, handle, list, TO_DDR);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}

unsigned long get_max_quota(int client_id)
{
	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return 0x0;
	}
	return process_quota(client_id);
}
