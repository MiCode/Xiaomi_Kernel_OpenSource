/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_MAIN_H_
#define _MC_MAIN_H_

/* Platform specific settings */
#include "platform.h"

/* We need 2 devices for admin and user interface*/
#define MC_DEV_MAX 2
#define MC_VERSION(major, minor) \
		(((major & 0x0000ffff) << 16) | (minor & 0x0000ffff))

/* MobiCore is idle. No scheduling required. */
#define SCHEDULE_IDLE		0
/* MobiCore is non idle, scheduling is required. */
#define SCHEDULE_NON_IDLE	1

/* MobiCore Driver Kernel Module context data. */
struct mc_sched_ctx;
struct mc_log_ctx;

struct mc_device_ctx {
	struct device		*mcd;

	/* MobiCore MCI information */
	struct rw_semaphore	mcp_lock;
	phys_addr_t		mci_base_pa;
	uint			mci_order;
	union {
		void		*mci_base;
		struct {
			struct notification_queue *tx;
			struct notification_queue *rx;
			struct rw_semaphore rx_lock; /* Receive lock */
		} nq;
	};
	struct mcp_context	*mcp;

	struct mutex		clients_lock; /* Clients list + temp notifs */
	/* List of device clients */
	struct list_head	clients;
	/* List of temporary notifications */
	/* CPI todo: mutex to protect temp_nq ? */
	struct list_head	temp_nq;
	/* List of GP sessions waiting final closing notif */
	struct list_head	closing_sess;
	struct mutex		closing_lock; /* Closing sessions list */

	/* Client for the driver itself, used by MCP */
	struct tbase_client	*mcore_client;

	/* Log's routines context, visible and managed by them */
	struct mc_log_ctx	*log;

#ifdef MC_PM_RUNTIME
	struct work_struct	suspend_work;
	struct notifier_block	mc_notif_block;
# ifdef MC_BL_NOTIFIER
	struct notifier_block	switcher_nb;
# endif
#endif
#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	struct clk		*mc_ce_iface_clk;
	struct clk		*mc_ce_core_clk;
	struct clk		*mc_ce_bus_clk;
#ifdef MC_USE_DEVICE_TREE
	struct clk		*mc_ce_core_src_clk;
#endif /* MC_USE_DEVICE_TREE */
#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */

	/* Features */
	bool			f_timeout;
	bool			f_mem_ext;
	bool			f_ta_auth;
};

extern struct mc_device_ctx g_ctx;

struct mc_sleep_mode {
	uint16_t	sleep_req;
	uint16_t	ready_to_sleep;
};

void mc_wakeup_session(uint32_t session_id, int32_t payload);

bool mc_read_notif(uint32_t session_id, int32_t *p_error);

bool mc_ref_client(struct tbase_client *client);

void mc_unref_client(struct tbase_client *client);

void mc_add_client(struct tbase_client *client);

int mc_remove_client(struct tbase_client *client);

#endif /* _MC_MAIN_H_ */
