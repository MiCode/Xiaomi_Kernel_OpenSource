/*
 * include/vservices/transport.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file defines the private interface that vServices transport drivers
 * must provide to the vservices session and protocol layers. The transport,
 * transport vtable, and message buffer structures are defined in the public
 * <vservices/transport.h> header.
 */

#ifndef _VSERVICES_TRANSPORT_PRIV_H_
#define _VSERVICES_TRANSPORT_PRIV_H_

#include <linux/types.h>
#include <linux/list.h>

#include <vservices/transport.h>
#include <vservices/types.h>
#include <vservices/buffer.h>

/**
 * struct vs_notify_info - Notification information stored in the transport
 * @service_id: Service id for this notification info
 * @offset: Offset into the notification mapping
 */
struct vs_notify_info {
	vs_service_id_t service_id;
	unsigned offset;
};

#define VS_MAX_SERVICES		128
#define VS_MAX_SERVICE_ID	(VS_MAX_SERVICES - 1)

#endif /* _VSERVICES_TRANSPORT_PRIV_H_ */
