/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2020 TRUSTONIC LIMITED
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

#ifndef MC_XEN_COMMON_H
#define MC_XEN_COMMON_H

#include <linux/list.h>
#include <linux/workqueue.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xen.h>
#include <xen/xenbus.h>

#include "public/mc_user.h"	/* many types */
#include "protocol_common.h"	/* For BE to treat other VMs as clients */

#define TEE_XEN_VERSION	3

struct tee_xen_ring {
	struct fe2be_data	fe2be_data;
	struct be2fe_data	be2fe_data;
};

struct tee_xfe {
	struct protocol_fe	pfe;
	struct xenbus_device	*xdev;
	struct kref		kref;
	u64			ring_ref;
	int			pte_entries_max;
	int			evtchn_domu;
	int			evtchn_dom0;
	int			irq_domu;
	int			irq_dom0;
	struct list_head	list;
	struct work_struct	work;
	/* Ring page */
	union {
		unsigned long		ring_ul;
		void			*ring_p;
		struct tee_xen_ring	*ring;
	};
	struct completion	ring_completion;
};

struct tee_xfe *tee_xfe_create(struct xenbus_device *xdev);
static inline void tee_xfe_get(struct tee_xfe *xfe)
{
	kref_get(&xfe->kref);
}

void tee_xfe_put(struct tee_xfe *xfe);

static inline void ring_get(struct tee_xfe *xfe)
{
	mutex_lock(&xfe->pfe.protocol_mutex);
	xfe->pfe.protocol_busy = true;
}

static inline void ring_put(struct tee_xfe *xfe)
{
	xfe->pfe.protocol_busy = false;
	mutex_unlock(&xfe->pfe.protocol_mutex);
}

#endif /* MC_XEN_COMMON_H */
