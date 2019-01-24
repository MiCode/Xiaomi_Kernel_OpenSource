/*
 * drivers/vservices/debug.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * Debugging macros and support functions for Virtual Services.
 */
#ifndef _VSERVICES_DEBUG_H
#define _VSERVICES_DEBUG_H

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
#include <linux/printk.h>
#else
#ifndef no_printk
#define no_printk(format, args...) do { } while (0)
#endif
#endif

#include <vservices/session.h>
#include "transport.h"

#define VS_DEBUG_TRANSPORT		(1 << 0)
#define VS_DEBUG_TRANSPORT_MESSAGES	(1 << 1)
#define VS_DEBUG_SESSION		(1 << 2)
#define VS_DEBUG_CLIENT			(1 << 3)
#define VS_DEBUG_CLIENT_CORE		(1 << 4)
#define VS_DEBUG_SERVER			(1 << 5)
#define VS_DEBUG_SERVER_CORE		(1 << 6)
#define VS_DEBUG_PROTOCOL		(1 << 7)
#define VS_DEBUG_ALL			0xff

#ifdef CONFIG_VSERVICES_DEBUG

#define vs_debug(type, session, format, args...)			\
	do {								\
		if ((session)->debug_mask & (type))			\
			dev_dbg(&(session)->dev, format, ##args);	\
	} while (0)

#define vs_dev_debug(type, session, dev, format, args...)		\
	do {								\
		if ((session)->debug_mask & (type))			\
			dev_dbg(dev, format, ##args);			\
	} while (0)

static inline void vs_debug_dump_mbuf(struct vs_session_device *session,
		struct vs_mbuf *mbuf)
{
	if (session->debug_mask & VS_DEBUG_TRANSPORT_MESSAGES)
		print_hex_dump_bytes("msg:", DUMP_PREFIX_OFFSET,
				mbuf->data, mbuf->size);
}

#else

/* Dummy versions: Use no_printk to retain type/format string checking */
#define vs_debug(type, session, format, args...) \
	do { (void)session; no_printk(format, ##args); } while(0)

#define vs_dev_debug(type, session, dev, format, args...) \
	do { (void)session; (void)dev; no_printk(format, ##args); } while(0)

static inline void vs_debug_dump_mbuf(struct vs_session_device *session,
		struct vs_mbuf *mbuf) {}

#endif /* CONFIG_VSERVICES_DEBUG */

#endif /* _VSERVICES_DEBUG_H */
