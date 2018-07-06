
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_CLIENT_SERIAL__)
#define __VSERVICES_CLIENT_SERIAL__

struct vs_service_device;
struct vs_client_serial_state;

struct vs_client_serial {

	/*
	 * If set to false then the receive message handlers are run from
	 * workqueue context and are allowed to sleep. If set to true the
	 * message handlers are run from tasklet context and may not sleep.
	 */
	bool rx_atomic;

	/*
	 * If this is set to true along with rx_atomic, the driver is allowed
	 * to send messages from softirq contexts other than the receive
	 * message handlers, after calling vs_service_state_lock_bh. Otherwise,
	 * messages may only be sent from the receive message handlers, or
	 * from task context after calling vs_service_state_lock. This must
	 * not be set to true if rx_atomic is set to false.
	 */
	bool tx_atomic;
    /** session setup **/
	struct vs_client_serial_state *(*alloc) (struct vs_service_device *
						 service);
	void (*release) (struct vs_client_serial_state * _state);

	struct vs_service_driver *driver;

/** Opened, reopened and closed functions **/

	void (*opened) (struct vs_client_serial_state * _state);

	void (*reopened) (struct vs_client_serial_state * _state);

	void (*closed) (struct vs_client_serial_state * _state);

/** Send/receive state callbacks **/
	int (*tx_ready) (struct vs_client_serial_state * _state);

	struct {
		int (*msg_msg) (struct vs_client_serial_state * _state,
				struct vs_pbuf b, struct vs_mbuf * _mbuf);

	} serial;
};

struct vs_client_serial_state {
	vservice_serial_protocol_state_t state;
	uint32_t packet_size;
	struct {
		uint32_t packet_size;
	} serial;
	struct vs_service_device *service;
	bool released;
};

extern int vs_client_serial_reopen(struct vs_client_serial_state *_state);

extern int vs_client_serial_close(struct vs_client_serial_state *_state);

    /** interface serial **/
/* message msg */
extern struct vs_mbuf *vs_client_serial_serial_alloc_msg(struct
							 vs_client_serial_state
							 *_state,
							 struct vs_pbuf *b,
							 gfp_t flags);
extern int vs_client_serial_serial_getbufs_msg(struct vs_client_serial_state
					       *_state, struct vs_pbuf *b,
					       struct vs_mbuf *_mbuf);
extern int vs_client_serial_serial_free_msg(struct vs_client_serial_state
					    *_state, struct vs_pbuf *b,
					    struct vs_mbuf *_mbuf);
extern int vs_client_serial_serial_send_msg(struct vs_client_serial_state
					    *_state, struct vs_pbuf b,
					    struct vs_mbuf *_mbuf);

/** Module registration **/

struct module;

extern int __vservice_serial_client_register(struct vs_client_serial *client,
					     const char *name,
					     struct module *owner);

static inline int vservice_serial_client_register(struct vs_client_serial
						  *client, const char *name)
{
#ifdef MODULE
	extern struct module __this_module;
	struct module *this_module = &__this_module;
#else
	struct module *this_module = NULL;
#endif

	return __vservice_serial_client_register(client, name, this_module);
}

extern int vservice_serial_client_unregister(struct vs_client_serial *client);

#endif				/* ! __VSERVICES_CLIENT_SERIAL__ */
