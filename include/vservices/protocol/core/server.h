
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(VSERVICES_SERVER_CORE)
#define VSERVICES_SERVER_CORE

struct vs_service_device;
struct vs_server_core_state;

struct vs_server_core {

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
	struct vs_server_core_state *(*alloc) (struct vs_service_device *
					       service);
	void (*release) (struct vs_server_core_state * _state);

	struct vs_service_driver *driver;

	/** Core service base interface **/
	void (*start) (struct vs_server_core_state * _state);
	void (*reset) (struct vs_server_core_state * _state);
    /** Send/receive state callbacks **/
	int (*tx_ready) (struct vs_server_core_state * _state);

	struct {
		int (*state_change) (struct vs_server_core_state * _state,
				     vservice_core_statenum_t old,
				     vservice_core_statenum_t new);

		int (*req_connect) (struct vs_server_core_state * _state);

		int (*req_disconnect) (struct vs_server_core_state * _state);

		int (*msg_service_reset) (struct vs_server_core_state * _state,
					  uint32_t service_id);

	} core;
};

struct vs_server_core_state {
	vservice_core_protocol_state_t state;
	struct vs_service_device *service;
	bool released;
};

/** Complete calls for server core functions **/

    /** interface core **/
/* command sync connect */
extern int vs_server_core_core_send_ack_connect(struct vs_server_core_state
						*_state, gfp_t flags);
extern int vs_server_core_core_send_nack_connect(struct vs_server_core_state
						 *_state, gfp_t flags);
    /* command sync disconnect */
extern int vs_server_core_core_send_ack_disconnect(struct vs_server_core_state
						   *_state, gfp_t flags);
extern int vs_server_core_core_send_nack_disconnect(struct vs_server_core_state
						    *_state, gfp_t flags);
    /* message startup */
extern int vs_server_core_core_send_startup(struct vs_server_core_state *_state,
					    uint32_t core_in_quota,
					    uint32_t core_out_quota,
					    gfp_t flags);

	    /* message shutdown */
extern int vs_server_core_core_send_shutdown(struct vs_server_core_state
					     *_state, gfp_t flags);

	    /* message service_created */
extern struct vs_mbuf *vs_server_core_core_alloc_service_created(struct
								 vs_server_core_state
								 *_state,
								 struct
								 vs_string
								 *service_name,
								 struct
								 vs_string
								 *protocol_name,
								 gfp_t flags);
extern int vs_server_core_core_free_service_created(struct vs_server_core_state
						    *_state,
						    struct vs_string
						    *service_name,
						    struct vs_string
						    *protocol_name,
						    struct vs_mbuf *_mbuf);
extern int vs_server_core_core_send_service_created(struct vs_server_core_state
						    *_state,
						    uint32_t service_id,
						    struct vs_string
						    service_name,
						    struct vs_string
						    protocol_name,
						    struct vs_mbuf *_mbuf);

	    /* message service_removed */
extern int vs_server_core_core_send_service_removed(struct vs_server_core_state
						    *_state,
						    uint32_t service_id,
						    gfp_t flags);

	    /* message server_ready */
extern int vs_server_core_core_send_server_ready(struct vs_server_core_state
						 *_state, uint32_t service_id,
						 uint32_t in_quota,
						 uint32_t out_quota,
						 uint32_t in_bit_offset,
						 uint32_t in_num_bits,
						 uint32_t out_bit_offset,
						 uint32_t out_num_bits,
						 gfp_t flags);

	    /* message service_reset */
extern int vs_server_core_core_send_service_reset(struct vs_server_core_state
						  *_state, uint32_t service_id,
						  gfp_t flags);

/** Module registration **/

struct module;

extern int __vservice_core_server_register(struct vs_server_core *server,
					   const char *name,
					   struct module *owner);

static inline int vservice_core_server_register(struct vs_server_core *server,
						const char *name)
{
#ifdef MODULE
	extern struct module __this_module;
	struct module *this_module = &__this_module;
#else
	struct module *this_module = NULL;
#endif

	return __vservice_core_server_register(server, name, this_module);
}

extern int vservice_core_server_unregister(struct vs_server_core *server);
#endif				/* ! VSERVICES_SERVER_CORE */
