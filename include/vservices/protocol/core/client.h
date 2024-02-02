
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_CLIENT_CORE__)
#define __VSERVICES_CLIENT_CORE__

struct vs_service_device;
struct vs_client_core_state;

struct vs_client_core {

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
	struct vs_client_core_state *(*alloc) (struct vs_service_device *
					       service);
	void (*release) (struct vs_client_core_state * _state);

	struct vs_service_driver *driver;

	/** Core service base interface **/
	void (*start) (struct vs_client_core_state * _state);
	void (*reset) (struct vs_client_core_state * _state);
    /** Send/receive state callbacks **/
	int (*tx_ready) (struct vs_client_core_state * _state);

	struct {
		int (*state_change) (struct vs_client_core_state * _state,
				     vservice_core_statenum_t old,
				     vservice_core_statenum_t new);

		int (*ack_connect) (struct vs_client_core_state * _state);
		int (*nack_connect) (struct vs_client_core_state * _state);

		int (*ack_disconnect) (struct vs_client_core_state * _state);
		int (*nack_disconnect) (struct vs_client_core_state * _state);

		int (*msg_startup) (struct vs_client_core_state * _state,
				    uint32_t core_in_quota,
				    uint32_t core_out_quota);

		int (*msg_shutdown) (struct vs_client_core_state * _state);

		int (*msg_service_created) (struct vs_client_core_state *
					    _state, uint32_t service_id,
					    struct vs_string service_name,
					    struct vs_string protocol_name,
					    struct vs_mbuf * _mbuf);

		int (*msg_service_removed) (struct vs_client_core_state *
					    _state, uint32_t service_id);

		int (*msg_server_ready) (struct vs_client_core_state * _state,
					 uint32_t service_id, uint32_t in_quota,
					 uint32_t out_quota,
					 uint32_t in_bit_offset,
					 uint32_t in_num_bits,
					 uint32_t out_bit_offset,
					 uint32_t out_num_bits);

		int (*msg_service_reset) (struct vs_client_core_state * _state,
					  uint32_t service_id);

	} core;
};

struct vs_client_core_state {
	vservice_core_protocol_state_t state;
	struct vs_service_device *service;
	bool released;
};

extern int vs_client_core_reopen(struct vs_client_core_state *_state);

extern int vs_client_core_close(struct vs_client_core_state *_state);

    /** interface core **/
/* command sync connect */
extern int vs_client_core_core_req_connect(struct vs_client_core_state *_state,
					   gfp_t flags);

	/* command sync disconnect */
extern int vs_client_core_core_req_disconnect(struct vs_client_core_state
					      *_state, gfp_t flags);

	/* message startup */
/* message shutdown */
/* message service_created */
extern int vs_client_core_core_getbufs_service_created(struct
						       vs_client_core_state
						       *_state,
						       struct vs_string
						       *service_name,
						       struct vs_string
						       *protocol_name,
						       struct vs_mbuf *_mbuf);
extern int vs_client_core_core_free_service_created(struct vs_client_core_state
						    *_state,
						    struct vs_string
						    *service_name,
						    struct vs_string
						    *protocol_name,
						    struct vs_mbuf *_mbuf);
    /* message service_removed */
/* message server_ready */
/* message service_reset */
extern int vs_client_core_core_send_service_reset(struct vs_client_core_state
						  *_state, uint32_t service_id,
						  gfp_t flags);

/** Module registration **/

struct module;

extern int __vservice_core_client_register(struct vs_client_core *client,
					   const char *name,
					   struct module *owner);

static inline int vservice_core_client_register(struct vs_client_core *client,
						const char *name)
{
#ifdef MODULE
	extern struct module __this_module;
	struct module *this_module = &__this_module;
#else
	struct module *this_module = NULL;
#endif

	return __vservice_core_client_register(client, name, this_module);
}

extern int vservice_core_client_unregister(struct vs_client_core *client);

#endif				/* ! __VSERVICES_CLIENT_CORE__ */
