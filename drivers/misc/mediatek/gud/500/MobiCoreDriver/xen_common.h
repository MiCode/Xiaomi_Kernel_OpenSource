/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 TRUSTONIC LIMITED
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

#ifndef _MC_XEN_COMMON_H_
#define _MC_XEN_COMMON_H_

#include <linux/list.h>
#include <linux/workqueue.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xen.h>
#include <xen/xenbus.h>

#include "public/mc_user.h"	/* many types */
#include "mci/mciiwp.h"
#include "mci/mcimcp.h"
#include "mmu.h"		/* PMD/PTE max entries */
#include "client.h"		/* For BE to treat other VMs as clients */

#define TEE_XEN_VERSION	3

#define TEE_BUFFERS	4

enum tee_xen_domu_cmd {
	TEE_XEN_DOMU_NONE,
	TEE_XEN_GET_VERSION,
	/* TEE_XEN_MC_OPEN_DEVICE = 11,		SWd does not support this */
	/* TEE_XEN_MC_CLOSE_DEVICE,		SWd does not support this */
	TEE_XEN_MC_HAS_SESSIONS = 13,
	TEE_XEN_MC_OPEN_SESSION,
	TEE_XEN_MC_OPEN_TRUSTLET,
	TEE_XEN_MC_CLOSE_SESSION,
	TEE_XEN_MC_NOTIFY,
	TEE_XEN_MC_WAIT,
	TEE_XEN_MC_MAP,
	TEE_XEN_MC_UNMAP,
	TEE_XEN_MC_GET_ERR,
	/* TEE_XEN_GP_INITIALIZE_CONTEXT = 21,	SWd does not support this */
	/* TEE_XEN_GP_FINALIZE_CONTEXT,		SWd does not support this */
	TEE_XEN_GP_REGISTER_SHARED_MEM = 23,
	TEE_XEN_GP_RELEASE_SHARED_MEM,
	TEE_XEN_GP_OPEN_SESSION,
	TEE_XEN_GP_CLOSE_SESSION,
	TEE_XEN_GP_INVOKE_COMMAND,
	TEE_XEN_GP_REQUEST_CANCELLATION,
};

enum tee_xen_dom0_cmd {
	TEE_XEN_DOM0_NONE,
	TEE_XEN_MC_WAIT_DONE = TEE_XEN_MC_WAIT,
	TEE_XEN_GP_OPEN_SESSION_DONE = TEE_XEN_GP_OPEN_SESSION,
	TEE_XEN_GP_CLOSE_SESSION_DONE = TEE_XEN_GP_CLOSE_SESSION,
	TEE_XEN_GP_INVOKE_COMMAND_DONE = TEE_XEN_GP_INVOKE_COMMAND,
};

union tee_xen_mmu_table {
	/* Array of references to pages (PTE_ENTRIES_MAX or PMD_ENTRIES_MAX) */
	grant_ref_t		*refs;
	/* Address of table */
	void			*addr;
	/* Page for table */
	unsigned long		page;
};

struct tee_xen_buffer_info {
	/* Page Middle Directory, refs to tee_xen_pte_table's (full pages) */
	grant_ref_t		pmd_ref;
	/* Total number of refs for buffer */
	u32			nr_refs;
	u64			addr;		/* Unique VM address */
	u32			offset;
	u32			length;
	u32			flags;
	u32			sva;
};

/* Convenience structure to get buffer info and contents in one place */
struct tee_xen_buffer {
	struct tee_xen_buffer_info	*info;
	union tee_xen_mmu_table		data;
};

struct tee_xen_ring {
	/* DomU side, synchronous and asynchronous commands */
	struct {
		enum tee_xen_domu_cmd		cmd;		/* in */
		u32				id;		/* in (debug) */
		/* Return code of this command from Dom0 */
		int				otherend_ret;	/* out */
		struct mc_uuid_t		uuid;		/* in */
		u32				session_id;	/* in/out */
		/* Buffers to share (4 for GP, 2 for mcOpenTrustlet) */
		struct tee_xen_buffer_info	buffers[TEE_BUFFERS]; /* in */
		/* MC */
		struct mc_version_info		version_info;	/* out */
		s32				timeout;	/* in */
		s32				err;		/* out */
		/* GP */
		u64				operation_id;	/* in */
		struct gp_return		gp_ret;		/* out */
		struct interworld_session	iws;		/* in */
	}			domu;

	/* Dom0 side, response to asynchronous command, never read by Dom0 */
	struct {
		enum tee_xen_dom0_cmd		cmd;		/* in */
		u32				id;		/* in (debug) */
		/* Return code from command */
		int				cmd_ret;	/* in */
		/* The operation id is used to match GP request and response */
		u64				operation_id;	/* in */
		struct gp_return		gp_ret;		/* in */
		struct interworld_session	iws;		/* in */
		/* The session id is used to match MC request and response */
		u32				session_id;	/* in */
	}			dom0;
};

struct tee_xfe {
	struct xenbus_device	*xdev;
	struct kref		kref;
	grant_ref_t		ring_ref;
	int			pte_entries_max;
	int			evtchn_domu;
	int			evtchn_dom0;
	int			irq_domu;
	int			irq_dom0;
	struct list_head	list;
	struct tee_client	*client;
	struct work_struct	work;
	/* Ring page */
	union {
		unsigned long		ring_ul;
		void			*ring_p;
		struct tee_xen_ring	*ring;
	};
	/* Buffer pages */
	struct tee_xen_buffer	buffers[TEE_BUFFERS];
	struct mutex		ring_mutex;	/* Protect our side of ring */
	struct completion	ring_completion;
	int			ring_busy;
	/* Unique ID for commands */
	u32			domu_cmd_id;
};

struct tee_xfe *tee_xfe_create(struct xenbus_device *xdev);
static inline void tee_xfe_get(struct tee_xfe *xfe)
{
	kref_get(&xfe->kref);
}

void tee_xfe_put(struct tee_xfe *xfe);

static inline void ring_get(struct tee_xfe *xfe)
{
	mutex_lock(&xfe->ring_mutex);
	xfe->ring_busy = true;
}

static inline void ring_put(struct tee_xfe *xfe)
{
	xfe->ring_busy = false;
	mutex_unlock(&xfe->ring_mutex);
}

#endif /* _MC_XEN_COMMON_H_ */
