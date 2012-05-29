/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_OCMEM_H
#define _ARCH_ARM_MACH_MSM_OCMEM_H

#include <asm/page.h>
#include <linux/module.h>
#include <linux/notifier.h>

#define OCMEM_MIN_ALLOC SZ_64K
#define OCMEM_MIN_ALIGN SZ_64K

/* Maximum number of slots in DM */
#define OCMEM_MAX_CHUNKS 32
#define MIN_CHUNK_SIZE 128

struct ocmem_notifier;

struct ocmem_buf {
	unsigned long addr;
	unsigned long len;
};

struct ocmem_buf_attr {
	unsigned long paddr;
	unsigned long len;
};

struct ocmem_chunk {
	bool ro;
	unsigned long ddr_paddr;
	unsigned long size;
};

struct ocmem_map_list {
	unsigned num_chunks;
	struct ocmem_chunk chunks[OCMEM_MAX_CHUNKS];
};

enum ocmem_power_state {
	OCMEM_OFF = 0x0,
	OCMEM_RETENTION,
	OCMEM_ON,
	OCMEM_MAX = OCMEM_ON,
};

struct ocmem_resource {
	unsigned resource_id;
	unsigned num_keys;
	unsigned int *keys;
};

struct ocmem_vectors {
	unsigned num_resources;
	struct ocmem_resource *r;
};

/* List of clients that allocate/interact with OCMEM */
/* Must be in sync with client_names */
enum ocmem_client {
	/* GMEM clients */
	OCMEM_GRAPHICS = 0x0,
	/* TCMEM clients */
	OCMEM_VIDEO,
	OCMEM_CAMERA,
	/* Dummy Clients */
	OCMEM_HP_AUDIO,
	OCMEM_VOICE,
	/* IMEM Clients */
	OCMEM_LP_AUDIO,
	OCMEM_SENSORS,
	OCMEM_OTHER_OS,
	OCMEM_CLIENT_MAX,
};

/**
 * List of OCMEM notification events which will be broadcasted
 * to clients that optionally register for these notifications
 * on a per allocation basis.
 **/
enum ocmem_notif_type {
	OCMEM_MAP_DONE = 1,
	OCMEM_MAP_FAIL,
	OCMEM_UNMAP_DONE,
	OCMEM_UNMAP_FAIL,
	OCMEM_ALLOC_GROW,
	OCMEM_ALLOC_SHRINK,
	OCMEM_NOTIF_TYPE_COUNT,
};

/* APIS */
/* Notification APIs */
struct ocmem_notifier *ocmem_notifier_register(int client_id,
						struct notifier_block *nb);

int ocmem_notifier_unregister(struct ocmem_notifier *notif_hndl,
				struct notifier_block *nb);

/* Obtain the maximum quota for the client */
unsigned long get_max_quota(int client_id);

/* Allocation APIs */
struct ocmem_buf *ocmem_allocate(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_nowait(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_nb(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_range(int client_id, unsigned long min,
			unsigned long goal, unsigned long step);

/* Free APIs */
int ocmem_free(int client_id, struct ocmem_buf *buf);

/* Dynamic Resize APIs */
int ocmem_shrink(int client_id, struct ocmem_buf *buf,
			unsigned long new_size);

/* Transfer APIs */
int ocmem_map(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list);


int ocmem_unmap(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list);

/* Priority Enforcement APIs */
int ocmem_evict(int client_id);

int ocmem_restore(int client_id);

/* Power Control APIs */
int ocmem_set_power_state(int client_id, struct ocmem_buf *buf,
				enum ocmem_power_state new_state);

enum ocmem_power_state ocmem_get_power_state(int client_id,
				struct ocmem_buf *buf);

struct ocmem_vectors *ocmem_get_vectors(int client_id,
						struct ocmem_buf *buf);
#endif
