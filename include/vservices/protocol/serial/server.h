
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(VSERVICES_SERVER_SERIAL)
#define VSERVICES_SERVER_SERIAL

struct vs_service_device;
struct vs_server_serial_state;

struct vs_server_serial {

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

	/*
	 * These are the driver's recommended message quotas. They are used
	 * by the core service to select message quotas for services with no
	 * explicitly configured quotas.
	 */
	u32 in_quota_best;
	u32 out_quota_best;
    /** session setup **/
	struct vs_server_serial_state *(*alloc) (struct vs_service_device *
						 service);
	void (*release) (struct vs_server_serial_state * _state);

	struct vs_service_driver *driver;

/** Open, reopen, close and closed functions **/

	 vs_server_response_type_t(*open) (struct vs_server_serial_state *
					   _state);

	 vs_server_response_type_t(*reopen) (struct vs_server_serial_state *
					     _state);

	 vs_server_response_type_t(*close) (struct vs_server_serial_state *
					    _state);

	void (*closed) (struct vs_server_serial_state * _state);

/** Send/receive state callbacks **/
	int (*tx_ready) (struct vs_server_serial_state * _state);

	struct {
		int (*msg_msg) (struct vs_server_serial_state * _state,
				struct vs_pbuf b, struct vs_mbuf * _mbuf);

	} serial;
};

struct vs_server_serial_state {
	vservice_serial_protocol_state_t state;
	uint32_t packet_size;
	struct {
		uint32_t packet_size;
	} serial;
	struct vs_service_device *service;
	bool released;
};

/** Complete calls for server core functions **/
extern int vs_server_serial_open_complete(struct vs_server_serial_state *_state,
					  vs_server_response_type_t resp);

extern int vs_server_serial_close_complete(struct vs_server_serial_state
					   *_state,
					   vs_server_response_type_t resp);

extern int vs_server_serial_reopen_complete(struct vs_server_serial_state
					    *_state,
					    vs_server_response_type_t resp);

    /** interface serial **/
/* message msg */
extern struct vs_mbuf *vs_server_serial_serial_alloc_msg(struct
							 vs_server_serial_state
							 *_state,
							 struct vs_pbuf *b,
							 gfp_t flags);
extern int vs_server_serial_serial_getbufs_msg(struct vs_server_serial_state
					       *_state, struct vs_pbuf *b,
					       struct vs_mbuf *_mbuf);
extern int vs_server_serial_serial_free_msg(struct vs_server_serial_state
					    *_state, struct vs_pbuf *b,
					    struct vs_mbuf *_mbuf);
extern int vs_server_serial_serial_send_msg(struct vs_server_serial_state
					    *_state, struct vs_pbuf b,
					    struct vs_mbuf *_mbuf);

/** Module registration **/

struct module;

extern int __vservice_serial_server_register(struct vs_server_serial *server,
					     const char *name,
					     struct module *owner);

static inline int vservice_serial_server_register(struct vs_server_serial
						  *server, const char *name)
{
#ifdef MODULE
	extern struct module __this_module;
	struct module *this_module = &__this_module;
#else
	struct module *this_module = NULL;
#endif

	return __vservice_serial_server_register(server, name, this_module);
}

extern int vservice_serial_server_unregister(struct vs_server_serial *server);
#endif				/* ! VSERVICES_SERVER_SERIAL */
