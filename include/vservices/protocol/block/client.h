
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_CLIENT_BLOCK__)
#define __VSERVICES_CLIENT_BLOCK__

struct vs_service_device;
struct vs_client_block_state;

struct vs_client_block {

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
	struct vs_client_block_state *(*alloc) (struct vs_service_device *
						service);
	void (*release) (struct vs_client_block_state * _state);

	struct vs_service_driver *driver;

/** Opened, reopened and closed functions **/

	void (*opened) (struct vs_client_block_state * _state);

	void (*reopened) (struct vs_client_block_state * _state);

	void (*closed) (struct vs_client_block_state * _state);

/** Send/receive state callbacks **/
	int (*tx_ready) (struct vs_client_block_state * _state);

	struct {
		int (*ack_read) (struct vs_client_block_state * _state,
				 void *_opaque, struct vs_pbuf data,
				 struct vs_mbuf * _mbuf);
		int (*nack_read) (struct vs_client_block_state * _state,
				  void *_opaque,
				  vservice_block_block_io_error_t err);

		int (*ack_write) (struct vs_client_block_state * _state,
				  void *_opaque);
		int (*nack_write) (struct vs_client_block_state * _state,
				   void *_opaque,
				   vservice_block_block_io_error_t err);

	} io;
};

struct vs_client_block_state {
	vservice_block_state_t state;
	bool readonly;
	uint32_t sector_size;
	uint32_t segment_size;
	uint64_t device_sectors;
	bool flushable;
	bool committable;
	struct {
		uint32_t sector_size;
		uint32_t segment_size;
	} io;
	struct vs_service_device *service;
	bool released;
};

extern int vs_client_block_reopen(struct vs_client_block_state *_state);

extern int vs_client_block_close(struct vs_client_block_state *_state);

    /** interface block_io **/
/* command parallel read */
extern int vs_client_block_io_getbufs_ack_read(struct vs_client_block_state
					       *_state, struct vs_pbuf *data,
					       struct vs_mbuf *_mbuf);
extern int vs_client_block_io_free_ack_read(struct vs_client_block_state
					    *_state, struct vs_pbuf *data,
					    struct vs_mbuf *_mbuf);
extern int vs_client_block_io_req_read(struct vs_client_block_state *_state,
				       void *_opaque, uint64_t sector_index,
				       uint32_t num_sects, bool nodelay,
				       bool flush, gfp_t flags);

	/* command parallel write */
extern struct vs_mbuf *vs_client_block_io_alloc_req_write(struct
							  vs_client_block_state
							  *_state,
							  struct vs_pbuf *data,
							  gfp_t flags);
extern int vs_client_block_io_free_req_write(struct vs_client_block_state
					     *_state, struct vs_pbuf *data,
					     struct vs_mbuf *_mbuf);
extern int vs_client_block_io_req_write(struct vs_client_block_state *_state,
					void *_opaque, uint64_t sector_index,
					uint32_t num_sects, bool nodelay,
					bool flush, bool commit,
					struct vs_pbuf data,
					struct vs_mbuf *_mbuf);

/* Status APIs for async parallel commands */
static inline bool vs_client_block_io_req_read_can_send(struct
							vs_client_block_state
							*_state)
{
	return !bitmap_full(_state->state.io.read_bitmask,
			    VSERVICE_BLOCK_IO_READ_MAX_PENDING);
}

static inline bool vs_client_block_io_req_read_is_pending(struct
							  vs_client_block_state
							  *_state)
{
	return !bitmap_empty(_state->state.io.read_bitmask,
			     VSERVICE_BLOCK_IO_READ_MAX_PENDING);
}

static inline bool vs_client_block_io_req_write_can_send(struct
							 vs_client_block_state
							 *_state)
{
	return !bitmap_full(_state->state.io.write_bitmask,
			    VSERVICE_BLOCK_IO_WRITE_MAX_PENDING);
}

static inline bool vs_client_block_io_req_write_is_pending(struct
							   vs_client_block_state
							   *_state)
{
	return !bitmap_empty(_state->state.io.write_bitmask,
			     VSERVICE_BLOCK_IO_WRITE_MAX_PENDING);
}

/** Module registration **/

struct module;

extern int __vservice_block_client_register(struct vs_client_block *client,
					    const char *name,
					    struct module *owner);

static inline int vservice_block_client_register(struct vs_client_block *client,
						 const char *name)
{
#ifdef MODULE
	extern struct module __this_module;
	struct module *this_module = &__this_module;
#else
	struct module *this_module = NULL;
#endif

	return __vservice_block_client_register(client, name, this_module);
}

extern int vservice_block_client_unregister(struct vs_client_block *client);

#endif				/* ! __VSERVICES_CLIENT_BLOCK__ */
