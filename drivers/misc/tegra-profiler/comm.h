/*
 * drivers/misc/tegra-profiler/comm.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_COMM_H__
#define __QUADD_COMM_H__

#include <linux/types.h>

struct quadd_record_data;
struct quadd_comm_cap;
struct quadd_module_state;
struct miscdevice;

struct quadd_ring_buffer {
	char *buf;
	spinlock_t lock;

	size_t size;
	size_t pos_read;
	size_t pos_write;
	size_t fill_count;
};

struct quadd_parameters;

struct quadd_comm_control_interface {
	int (*start)(void);
	void (*stop)(void);
	int (*set_parameters)(struct quadd_parameters *param,
			      uid_t *debug_app_uid);
	void (*get_capabilities)(struct quadd_comm_cap *cap);
	void (*get_state)(struct quadd_module_state *state);
};

struct quadd_comm_data_interface {
	void (*put_sample)(struct quadd_record_data *data, char *extra_data,
			   unsigned int extra_length);
	void (*reset)(void);
};

struct quadd_comm_ctx {
	struct quadd_comm_control_interface *control;
	struct quadd_ring_buffer rb;

	atomic_t active;

	struct mutex io_mutex;
	int nr_users;

	int params_ok;
	pid_t process_pid;
	uid_t debug_app_uid;

	struct miscdevice *misc_dev;
};

struct quadd_comm_data_interface *
quadd_comm_events_init(struct quadd_comm_control_interface *control);
void quadd_comm_events_exit(void);

#endif	/* __QUADD_COMM_H__ */
