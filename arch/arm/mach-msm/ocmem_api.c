/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

static DEFINE_MUTEX(ocmem_eviction_lock);
static DECLARE_BITMAP(evicted, OCMEM_CLIENT_MAX);

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

	if (ret) {
		pr_err("ocmem: Free failed for client %s\n", get_name(id));
		return ret;
	}
	free_handle(handle);
	return 0;
}

static int __ocmem_shrink(int id, struct ocmem_buf *buf, unsigned long len)
{
	int ret = 0;
	struct ocmem_handle *handle = buffer_to_handle(buf);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_mutex);
	ret = process_shrink(id, handle, len);
	mutex_unlock(&handle->handle_mutex);

	if (ret)
		return -EINVAL;

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
	struct ocmem_buf *buffer;
	struct timeval start_time;
	struct timeval end_time;
	unsigned int delay;
	struct ocmem_zone *zone;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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

	zone = get_zone(client_id);
	if (!zone) {
		pr_err("ocmem: Zone not found for client %d\n", client_id);
		return NULL;
	}

	do_gettimeofday(&start_time);

	buffer = __ocmem_allocate_range(client_id, size, size,
					size, can_block, can_wait);

	do_gettimeofday(&end_time);

	if (!buffer)
		return NULL;

	delay = (end_time.tv_sec * USEC_PER_SEC + end_time.tv_usec)
		 - (start_time.tv_sec * USEC_PER_SEC + start_time.tv_usec);

	if (delay > zone->max_alloc_time)
		zone->max_alloc_time = delay;
	if (delay < zone->min_alloc_time)
		zone->min_alloc_time = delay;
	zone->total_alloc_time += delay;
	inc_ocmem_stat(zone, NR_SYNC_ALLOCATIONS);

	return buffer;
}
EXPORT_SYMBOL(ocmem_allocate);

struct ocmem_buf *ocmem_allocate_nowait(int client_id, unsigned long size)
{
	bool can_block = false;
	bool can_wait = false;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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
EXPORT_SYMBOL(ocmem_allocate_nowait);

struct ocmem_buf *ocmem_allocate_range(int client_id, unsigned long min,
		unsigned long goal, unsigned long step)
{
	bool can_block = true;
	bool can_wait = false;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return NULL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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
EXPORT_SYMBOL(ocmem_allocate_range);

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

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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
EXPORT_SYMBOL(ocmem_allocate_nb);

int ocmem_free(int client_id, struct ocmem_buf *buffer)
{
	int rc;
	struct timeval start_time;
	struct timeval end_time;
	unsigned int delay;
	struct ocmem_zone *zone;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
		return -EINVAL;
	}

	zone = get_zone(client_id);
	if (!zone) {
		pr_err("ocmem: Zone not found for client %d\n", client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	do_gettimeofday(&start_time);

	rc = __ocmem_free(client_id, buffer);

	do_gettimeofday(&end_time);

	if (rc < 0)
		return rc;

	delay = (end_time.tv_sec * USEC_PER_SEC + end_time.tv_usec)
		 - (start_time.tv_sec * USEC_PER_SEC + start_time.tv_usec);

	if (delay > zone->max_free_time)
		zone->max_free_time = delay;
	if (delay < zone->min_free_time)
		zone->min_free_time = delay;
	zone->total_free_time += delay;
	inc_ocmem_stat(zone, NR_FREES);

	return rc;

}
EXPORT_SYMBOL(ocmem_free);

int ocmem_shrink(int client_id, struct ocmem_buf *buffer, unsigned long len)
{
	if (!buffer)
		return -EINVAL;
	if (len >= buffer->len)
		return -EINVAL;

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client id: %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
		return -EINVAL;
	}

	return __ocmem_shrink(client_id, buffer, len);
}
EXPORT_SYMBOL(ocmem_shrink);

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
			!IS_ALIGNED(chunks[i].ddr_paddr, MIN_CHUNK_SIZE) ||
			chunks[i].size < MIN_CHUNK_SIZE ||
			!IS_ALIGNED(chunks[i].size, MIN_CHUNK_SIZE)) {
			pr_err("Invalid ocmem chunk at index %d (p: %lx, size %lx)\n",
					i, chunks[i].ddr_paddr, chunks[i].size);
			return -EINVAL;
		}
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

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client id: %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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

	if (pre_validate_chunk_list(list) != 0)
		return -EINVAL;

	handle = buffer_to_handle(buffer);

	if (!handle)
		return -EINVAL;

	mutex_lock(&handle->handle_mutex);
	ret = process_xfer(client_id, handle, list, TO_OCMEM);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}
EXPORT_SYMBOL(ocmem_map);

int ocmem_unmap(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list)
{

	int ret = 0;
	struct ocmem_handle *handle = NULL;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client id: %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
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

	if (pre_validate_chunk_list(list) != 0)
		return -EINVAL;

	handle = buffer_to_handle(buffer);
	mutex_lock(&handle->handle_mutex);
	ret = process_xfer(client_id, handle, list, TO_DDR);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}
EXPORT_SYMBOL(ocmem_unmap);

int ocmem_drop(int client_id, struct ocmem_buf *buffer,
			   struct ocmem_map_list *list)
{
	int ret = 0;
	struct ocmem_handle *handle = NULL;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client id: %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	if (pre_validate_chunk_list(list) != 0)
		return -EINVAL;

	handle = buffer_to_handle(buffer);
	mutex_lock(&handle->handle_mutex);
	ret = process_drop(client_id, handle, list);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}
EXPORT_SYMBOL(ocmem_drop);


int ocmem_dump(int client_id, struct ocmem_buf *buffer,
			unsigned long dst_phys_addr)
{
	int ret = 0;
	struct ocmem_handle *handle = NULL;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client id: %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
		return -EINVAL;
	}

	if (!buffer) {
		pr_err("ocmem: Invalid buffer\n");
		return -EINVAL;
	}

	handle = buffer_to_handle(buffer);
	mutex_lock(&handle->handle_mutex);
	ret = process_dump(client_id, handle, dst_phys_addr);
	mutex_unlock(&handle->handle_mutex);
	return ret;
}
EXPORT_SYMBOL(ocmem_dump);

unsigned long get_max_quota(int client_id)
{
	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return 0x0;
	}
	return process_quota(client_id);
}
EXPORT_SYMBOL(get_max_quota);

/* Synchronous eviction/restore calls */
/* Only a single eviction or restoration is allowed */
/* Evictions/Restorations cannot be concurrent with other maps */
int ocmem_evict(int client_id)
{
	int ret = 0;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	mutex_lock(&ocmem_eviction_lock);
	if (test_bit(client_id, evicted)) {
		pr_err("ocmem: Previous eviction was not restored by %d\n",
			client_id);
		mutex_unlock(&ocmem_eviction_lock);
		return -EINVAL;
	}

	ret = process_evict(client_id);
	if (ret == 0)
		set_bit(client_id, evicted);

	mutex_unlock(&ocmem_eviction_lock);
	return ret;
}
EXPORT_SYMBOL(ocmem_evict);

int ocmem_restore(int client_id)
{
	int ret = 0;

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid client id: %d\n", client_id);
		return -EINVAL;
	}

	mutex_lock(&ocmem_eviction_lock);
	if (!test_bit(client_id, evicted)) {
		pr_err("ocmem: No previous eviction by %d\n", client_id);
		mutex_unlock(&ocmem_eviction_lock);
		return -EINVAL;
	}
	ret = process_restore(client_id);
	clear_bit(client_id, evicted);
	mutex_unlock(&ocmem_eviction_lock);
	return ret;
}
EXPORT_SYMBOL(ocmem_restore);

/* Wrappers until power control is transitioned to clients */
enum ocmem_power_state ocmem_get_power_state(int client_id,
						struct ocmem_buf *buffer)
{
	return 0;
}

int ocmem_set_power_state(int client_id, struct ocmem_buf *buffer,
					enum ocmem_power_state new_state)
{
	return 0;
}

struct ocmem_vectors *ocmem_get_vectors(int client_id,
				struct ocmem_buf *buffer)
{
	return NULL;
}
