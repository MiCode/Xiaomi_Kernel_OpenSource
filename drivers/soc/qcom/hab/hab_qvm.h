/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <guest_shm.h>
#include <linux/stddef.h>

#define PULSE_CODE_NOTIFY 0
#define PULSE_CODE_INPUT 1

struct qvm_channel {
	int be;

	struct hab_pipe *pipe;
	struct hab_pipe_endpoint *pipe_ep;
	spinlock_t io_lock;
	struct tasklet_struct task;
	struct guest_shm_factory *guest_factory;
	struct guest_shm_control *guest_ctrl;
	/* cached guest ctrl idx value to prevent trap when accessed */
	uint32_t idx;

	int channel;
	int coid;

	/* Guest VM */
	unsigned int guest_intr;
	unsigned int guest_iid;
	unsigned int factory_addr;
	unsigned int irq;

};

/* Shared mem size in each direction for communication pipe */
#define PIPE_SHMEM_SIZE (128 * 1024)

void *qnx_hyp_rx_dispatch(void *data);
void hab_pipe_reset(struct physical_channel *pchan);
void habhyp_notify(void *commdev);
#endif /* __HAB_QNX_H */
