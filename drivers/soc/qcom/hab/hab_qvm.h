/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
	uint32_t idx;

	int channel;
	int coid;

	unsigned int guest_intr;
	unsigned int guest_iid;
};

/* Shared mem size in each direction for communication pipe */
#define PIPE_SHMEM_SIZE (128 * 1024)

void *qnx_hyp_rx_dispatch(void *data);

#endif /* __HAB_QNX_H */
