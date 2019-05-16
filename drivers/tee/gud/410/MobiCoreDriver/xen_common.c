// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifdef CONFIG_XEN

#include "main.h"
#include "client.h"
#include "xen_common.h"

struct tee_xfe *tee_xfe_create(struct xenbus_device *xdev)
{
	struct tee_xfe *xfe;

	/* Alloc */
	xfe = kzalloc(sizeof(*xfe), GFP_KERNEL);
	if (!xfe)
		return NULL;

	atomic_inc(&g_ctx.c_xen_fes);
	/* Init */
	dev_set_drvdata(&xdev->dev, xfe);
	xfe->xdev = xdev;
	kref_init(&xfe->kref);
	xfe->evtchn_domu = -1;
	xfe->evtchn_dom0 = -1;
	xfe->irq_domu = -1;
	xfe->irq_dom0 = -1;
	INIT_LIST_HEAD(&xfe->list);
	mutex_init(&xfe->ring_mutex);
	init_completion(&xfe->ring_completion);
	return xfe;
}

static void tee_xfe_release(struct kref *kref)
{
	struct tee_xfe *xfe = container_of(kref, struct tee_xfe, kref);

	if (xfe->client)
		client_close(xfe->client);

	kfree(xfe);
	atomic_dec(&g_ctx.c_xen_fes);
}

void tee_xfe_put(struct tee_xfe *xfe)
{
	kref_put(&xfe->kref, tee_xfe_release);
}

#endif
