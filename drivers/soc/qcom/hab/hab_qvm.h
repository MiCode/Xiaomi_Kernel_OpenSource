/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#ifndef __HAB_QNX_H
#define __HAB_QNX_H
#include "hab.h"
#include "hab_pipe.h"

#include "hab_qvm_os.h"

struct qvm_channel {
	int be;

	struct hab_pipe *pipe;
	struct hab_pipe_endpoint *pipe_ep;
	spinlock_t io_lock;

	/* common but only for guest */
	struct guest_shm_factory *guest_factory;
	struct guest_shm_control *guest_ctrl;

	/* cached guest ctrl idx value to prevent trap when accessed */
	uint32_t idx;

	/* Guest VM */
	unsigned int guest_intr;
	unsigned int guest_iid;
	unsigned int factory_addr;
	unsigned int irq;

	/* os-specific part */
	struct qvm_channel_os *os_data;

	/* debug only */
	struct workqueue_struct *wq;
	struct work_data {
		struct work_struct work;
		int data; /* free to modify */
	} wdata;
	char *side_buf; /* to store the contents from hab-pipe */
};

/* This is common but only for guest in HQX */
struct shmem_irq_config {
	unsigned long factory_addr; /* from gvm settings when provided */
	int irq; /* from gvm settings when provided */
};

struct qvm_plugin_info {
	struct shmem_irq_config *pchan_settings;
	int setting_size;
	int curr;
	int probe_cnt;
};

extern struct qvm_plugin_info qvm_priv_info;

/* Shared mem size in each direction for communication pipe */
#define PIPE_SHMEM_SIZE (128 * 1024)

void hab_pipe_reset(struct physical_channel *pchan);
void habhyp_notify(void *commdev);
unsigned long hab_shmem_factory_va(unsigned long factory_addr);
char *hab_shmem_attach(struct qvm_channel *dev, const char *name,
	uint32_t pages);
uint64_t get_guest_ctrl_paddr(struct qvm_channel *dev,
	unsigned long factory_addr, int irq, const char *name, uint32_t pages);
#endif /* __HAB_QNX_H */
