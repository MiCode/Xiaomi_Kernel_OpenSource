/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include "ocmem_priv.h"
#include <linux/hardirq.h>

static unsigned notifier_threshold;

/* Protect the notifier structure below */
DEFINE_MUTEX(nc_lock);

struct ocmem_notifier {
	int owner;
	struct atomic_notifier_head nc;
	unsigned listeners;
} notifiers[OCMEM_CLIENT_MAX];

int check_notifier(int id)
{
	int ret = 0;

	if (!check_id(id))
		return 0;

	mutex_lock(&nc_lock);
	ret = notifiers[id].listeners;
	mutex_unlock(&nc_lock);
	return ret;
}

int ocmem_notifier_init(void)
{
	int id;
	/* Maximum notifiers for each subsystem */
	notifier_threshold = 1;
	mutex_lock(&nc_lock);
	for (id = 0; id < OCMEM_CLIENT_MAX; id++) {
		notifiers[id].owner = id;
		ATOMIC_INIT_NOTIFIER_HEAD(&notifiers[id].nc);
		notifiers[id].listeners = 0;
	}
	mutex_unlock(&nc_lock);
	return 0;
}

/* Broadcast a notification to listeners */
int dispatch_notification(int id, enum ocmem_notif_type notif,
				struct ocmem_buf *buf)
{
	int ret = 0;
	struct ocmem_notifier *nc_hndl = NULL;
	mutex_lock(&nc_lock);
	nc_hndl = &notifiers[id];
	if (nc_hndl->listeners == 0) {
		/* Send an error so that the scheduler can clean up */
		mutex_unlock(&nc_lock);
		return -EINVAL;
	}
	ret = atomic_notifier_call_chain(&notifiers[id].nc, notif, buf);
	mutex_unlock(&nc_lock);
	return ret;
}

struct ocmem_notifier *ocmem_notifier_register(int client_id,
						struct notifier_block *nb)
{

	int ret = 0;
	struct ocmem_notifier *nc_hndl = NULL;

	if (!is_probe_done())
		return ERR_PTR(-EPROBE_DEFER);

	if (!check_id(client_id)) {
		pr_err("ocmem: Invalid Client id\n");
		return NULL;
	}

	if (!zone_active(client_id)) {
		pr_err("ocmem: Client %s (id: %d) not allowed to use OCMEM\n",
					get_name(client_id), client_id);
		return NULL;
	}

	if (!nb) {
		pr_err("ocmem: Invalid Notifier Block\n");
		return NULL;
	}

	mutex_lock(&nc_lock);

	nc_hndl = &notifiers[client_id];

	if (nc_hndl->listeners >= notifier_threshold) {
		pr_err("ocmem: Max notifiers already registered\n");
		mutex_unlock(&nc_lock);
		return NULL;
	}

	ret = atomic_notifier_chain_register(&nc_hndl->nc, nb);

	if (ret < 0) {
		mutex_unlock(&nc_lock);
		return NULL;
	}

	nc_hndl->listeners++;
	pr_info("ocmem: Notifier registered for %d\n", client_id);
	mutex_unlock(&nc_lock);
	return nc_hndl;
}
EXPORT_SYMBOL(ocmem_notifier_register);

int ocmem_notifier_unregister(struct ocmem_notifier *nc_hndl,
				struct notifier_block *nb)
{

	int ret = 0;

	if (!nc_hndl) {
		pr_err("ocmem: Invalid notification handle\n");
		return -EINVAL;
	}

	mutex_lock(&nc_lock);
	ret = atomic_notifier_chain_unregister(&nc_hndl->nc, nb);
	nc_hndl->listeners--;
	mutex_unlock(&nc_lock);

	return ret;
}
EXPORT_SYMBOL(ocmem_notifier_unregister);
