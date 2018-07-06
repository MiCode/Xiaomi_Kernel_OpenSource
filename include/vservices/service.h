/*
 * include/vservices/service.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file defines the driver and device types for vServices client and
 * server drivers. These are generally defined by generated protocol-layer
 * code. However, they can also be defined directly by applications that
 * don't require protocol generation.
 */

#ifndef _VSERVICE_SERVICE_H_
#define _VSERVICE_SERVICE_H_

#include <linux/version.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/err.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)
#include <asm/atomic.h>
#else
#include <linux/atomic.h>
#endif

#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/types.h>

struct vs_mbuf;

/**
 * struct vs_service_driver - Virtual service driver structure
 * @protocol: Protocol name for this driver
 * @is_server: True if this is a server driver, false if it is a client driver
 * @rx_atomic: If set to false then the receive message handlers are run from
 *	     workqueue context and are allowed to sleep. If set to true
 *	     the message handlers are run from tasklet context and may not
 *	     sleep. For this purpose, tx_ready is considered a receive
 *	     message handler.
 * @tx_atomic: If this is set to true along with rx_atomic, the driver is
 *	allowed to send messages from softirq contexts other than the receive
 *	message handlers, after calling vs_service_state_lock_bh. Otherwise,
 *	messages may only be sent from the receive message handlers, or from
 *	task context after calling vs_service_state_lock.
 * @probe: Probe function for this service
 * @remove: Remove function for this service
 * --- Callbacks ---
 * @receive: Message handler function for this service
 * @notify: Incoming notification handler function for this service
 * @start: Callback which is run when this service is started
 * @reset: Callback which is run when this service is reset
 * @tx_ready: Callback which is run when the service has dropped below its
 *	    send quota
 * --- Resource requirements (valid for server only) ---
 * @in_quota_min: minimum number of input messages for protocol functionality
 * @in_quota_best: suggested number of input messages
 * @out_quota_min: minimum number of output messages for protocol functionality
 * @out_quota_best: suggested number of output messages
 * @in_notify_count: number of input notification bits used
 * @out_notify_count: number of output notification bits used
 * --- Internal ---
 * @driver: Linux device model driver structure
 *
 * The callback functions for a virtual service driver are all called from
 * the virtual service device's work queue.
 */
struct vs_service_driver {
	const char *protocol;
	bool is_server;
	bool rx_atomic, tx_atomic;

	int (*probe)(struct vs_service_device *service);
	int (*remove)(struct vs_service_device *service);

	int (*receive)(struct vs_service_device *service,
		struct vs_mbuf *mbuf);
	void (*notify)(struct vs_service_device *service, u32 flags);

	void (*start)(struct vs_service_device *service);
	void (*reset)(struct vs_service_device *service);

	int (*tx_ready)(struct vs_service_device *service);

	unsigned in_quota_min;
	unsigned in_quota_best;
	unsigned out_quota_min;
	unsigned out_quota_best;
	unsigned in_notify_count;
	unsigned out_notify_count;

	struct device_driver driver;
};

#define to_vs_service_driver(d) \
	container_of(d, struct vs_service_driver, driver)

/* The vServices server/client bus types */
extern struct bus_type vs_client_bus_type;
extern struct bus_type vs_server_bus_type;

/**
 * struct vs_service_stats - Virtual service statistics
 * @over_quota_time: Internal counter for tracking over quota time.
 * @sent_mbufs: Total number of message buffers sent.
 * @sent_bytes: Total bytes sent.
 * @send_failures: Total number of send failures.
 * @recv_mbufs: Total number of message buffers received.
 * @recv_bytes: Total number of bytes recevied.
 * @recv_failures: Total number of receive failures.
 * @nr_over_quota: Number of times an mbuf allocation has failed because the
 *                 service is over quota.
 * @nr_tx_ready: Number of times the service has run its tx_ready handler
 * @over_quota_time_total: The total amount of time in milli-seconds that the
 *                         service has spent over quota. Measured as the time
 *                         between exceeding quota in mbuf allocation and
 *                         running the tx_ready handler.
 * @over_quota_time_avg: The average amount of time in milli-seconds that the
 *                       service is spending in the over quota state.
 */
struct vs_service_stats {
	unsigned long	over_quota_time;

	atomic_t        sent_mbufs;
	atomic_t        sent_bytes;
	atomic_t	send_failures;
	atomic_t        recv_mbufs;
	atomic_t        recv_bytes;
	atomic_t	recv_failures;
	atomic_t        nr_over_quota;
	atomic_t        nr_tx_ready;
	atomic_t        over_quota_time_total;
	atomic_t        over_quota_time_avg;
};

/**
 * struct vs_service_device - Virtual service device
 * @id: Unique ID (to the session) for this service
 * @name: Service name
 * @sysfs_name: The sysfs name for the service
 * @protocol: Service protocol name
 * @is_server: True if this device is server, false if it is a client
 * @owner: service responsible for managing this service. This must be
 *     on the same session, and is NULL iff this is the core service.
 *     It must not be a service whose driver has tx_atomic set.
 * @lock_subclass: the number of generations of owners between this service
 *     and the core service; 0 for the core service, 1 for anything directly
 *     created by it, and so on. This is only used for verifying lock
 *     ordering (when lockdep is enabled), hence the name.
 * @ready_lock: mutex protecting readiness, disable_count and driver_probed.
 *     This depends on the state_mutex of the service's owner, if any. Acquire
 *     it using mutex_lock_nested(ready_lock, lock_subclass).
 * @readiness: Service's readiness state, owned by session layer.
 * @disable_count: Number of times the service has been disabled without
 *     a matching enable.
 * @driver_probed: True if a driver has been probed (and not removed)
 * @work_queue: Work queue for this service's task-context work.
 * @rx_tasklet: Tasklet for handling incoming messages. This is only used
 *     if the service driver has rx_atomic set to true. Otherwise
 *     incoming messages are handled on the workqueue by rx_work.
 * @rx_work: Work structure for handling incoming messages. This is only
 *     used if the service driver has rx_atomic set to false.
 * @rx_lock: Spinlock which protects access to rx_queue and tx_ready
 * @rx_queue: Queue of incoming messages
 * @tx_ready: Flag indicating that a tx_ready event is pending
 * @tx_batching: Flag indicating that outgoing messages are being batched
 * @state_spinlock: spinlock used to protect the service state if the
 *     service driver has tx_atomic (and rx_atomic) set to true. This
 *     depends on the service's ready_lock. Acquire it only by
 *     calling vs_service_state_lock_bh().
 * @state_mutex: mutex used to protect the service state if the service
 *     driver has tx_atomic set to false. This depends on the service's
 *     ready_lock, and if rx_atomic is true, the rx_tasklet must be
 *     disabled while it is held. Acquire it only by calling
 *     vs_service_state_lock().
 * @state_spinlock_used: Flag to check if the state spinlock has been acquired.
 * @state_mutex_used: Flag to check if the state mutex has been acquired.
 * @reset_work: Work to reset the service after a driver fails
 * @pending_reset: Set if reset_work has been queued and not completed.
 * @ready_work: Work to make service ready after a throttling delay
 * @cooloff_work: Work for cooling off reset throttling after the reset
 * throttling limit was hit
 * @cleanup_work: Work for cleaning up and freeing the service structure
 * @last_reset: Time in jiffies at which this service last reset
 * @last_reset_request: Time in jiffies the last reset request for this
 *     service occurred at
 * @last_ready: Time in jiffies at which this service last became ready
 * @reset_delay: Time in jiffies that the next throttled reset will be
 *     delayed for. A value of zero means that reset throttling is not in
 *     effect.
 * @is_over_quota: Internal flag for whether the service is over quota. This
 *                 flag is only used for stats accounting.
 * @quota_wq: waitqueue that is woken whenever the available send quota
 *            increases.
 * @notify_send_bits: The number of bits allocated for outgoing notifications.
 * @notify_send_offset: The first bit allocated for outgoing notifications.
 * @notify_recv_bits: The number of bits allocated for incoming notifications.
 * @notify_recv_offset: The first bit allocated for incoming notifications.
 * @send_quota: The maximum number of outgoing messages.
 * @recv_quota: The maximum number of incoming messages.
 * @in_quota_set: For servers, the number of client->server messages
 *     requested during system configuration (sysfs or environment).
 * @out_quota_set: For servers, the number of server->client messages
 *     requested during system configuration (sysfs or environment).
 * @dev: Linux device model device structure
 * @stats: Service statistics
 */
struct vs_service_device {
	vs_service_id_t id;
	char *name;
	char *sysfs_name;
	char *protocol;
	bool is_server;

	struct vs_service_device *owner;
	unsigned lock_subclass;

	struct mutex ready_lock;
	unsigned readiness;
	int disable_count;
	bool driver_probed;

	struct workqueue_struct *work_queue;

	struct tasklet_struct rx_tasklet;
	struct work_struct rx_work;

	spinlock_t rx_lock;
	struct list_head rx_queue;
	bool tx_ready, tx_batching;

	spinlock_t state_spinlock;
	struct mutex state_mutex;

	struct work_struct reset_work;
	bool pending_reset;
	struct delayed_work ready_work;
	struct delayed_work cooloff_work;
	struct work_struct cleanup_work;

	unsigned long last_reset;
	unsigned long last_reset_request;
	unsigned long last_ready;
	unsigned long reset_delay;

	atomic_t is_over_quota;
	wait_queue_head_t quota_wq;

	unsigned notify_send_bits;
	unsigned notify_send_offset;
	unsigned notify_recv_bits;
	unsigned notify_recv_offset;
	unsigned send_quota;
	unsigned recv_quota;

	unsigned in_quota_set;
	unsigned out_quota_set;

	void *transport_priv;

	struct device dev;
	struct vs_service_stats stats;

#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	bool state_spinlock_used;
	bool state_mutex_used;
#endif
};

#define to_vs_service_device(d) container_of(d, struct vs_service_device, dev)

/**
 * vs_service_get_session - Return the session for a service
 * @service: Service to get the session for
 */
static inline struct vs_session_device *
vs_service_get_session(struct vs_service_device *service)
{
	return to_vs_session_device(service->dev.parent);
}

/**
 * vs_service_send - Send a message from a service
 * @service: Service to send the message from
 * @mbuf: Message buffer to send
 */
static inline int
vs_service_send(struct vs_service_device *service, struct vs_mbuf *mbuf)
{
	struct vs_session_device *session = vs_service_get_session(service);
	const struct vs_transport_vtable *vt = session->transport->vt;
	const unsigned long flags =
		service->tx_batching ?  VS_TRANSPORT_SEND_FLAGS_MORE : 0;
	size_t msg_size = vt->mbuf_size(mbuf);
	int err;

	err = vt->send(session->transport, service, mbuf, flags);
	if (!err) {
		atomic_inc(&service->stats.sent_mbufs);
		atomic_add(msg_size, &service->stats.sent_bytes);
	} else {
		atomic_inc(&service->stats.send_failures);
	}

	return err;
}

/**
 * vs_service_alloc_mbuf - Allocate a message buffer for a service
 * @service: Service to allocate the buffer for
 * @size: Size of the data buffer to allocate
 * @flags: Flags to pass to the buffer allocation
 */
static inline struct vs_mbuf *
vs_service_alloc_mbuf(struct vs_service_device *service, size_t size,
		gfp_t flags)
{
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_mbuf *mbuf;

	mbuf = session->transport->vt->alloc_mbuf(session->transport,
			service, size, flags);
	if (IS_ERR(mbuf) && PTR_ERR(mbuf) == -ENOBUFS) {
		/* Over quota accounting */
		if (atomic_cmpxchg(&service->is_over_quota, 0, 1) == 0) {
			service->stats.over_quota_time = jiffies;
			atomic_inc(&service->stats.nr_over_quota);
		}
	}

	/*
	 * The transport drivers should return either a valid message buffer
	 * pointer or an ERR_PTR value. Warn here if a transport driver is
	 * returning NULL on message buffer allocation failure.
	 */
	if (WARN_ON_ONCE(!mbuf))
		return ERR_PTR(-ENOMEM);

	return mbuf;
}

/**
 * vs_service_free_mbuf - Deallocate a message buffer for a service
 * @service: Service the message buffer was allocated for
 * @mbuf: Message buffer to deallocate
 */
static inline void
vs_service_free_mbuf(struct vs_service_device *service, struct vs_mbuf *mbuf)
{
	struct vs_session_device *session = vs_service_get_session(service);

	session->transport->vt->free_mbuf(session->transport, service, mbuf);
}

/**
 * vs_service_notify - Send a notification from a service
 * @service: Service to send the notification from
 * @flags: Notification bits to send
 */
static inline int
vs_service_notify(struct vs_service_device *service, u32 flags)
{
	struct vs_session_device *session = vs_service_get_session(service);

	return session->transport->vt->notify(session->transport,
			service, flags);
}

/**
 * vs_service_has_atomic_rx - Return whether or not a service's receive
 * message handler runs in atomic context. This function should only be
 * called for services which are bound to a driver.
 *
 * @service: Service to check
 */
static inline bool
vs_service_has_atomic_rx(struct vs_service_device *service)
{
	if (WARN_ON(!service->dev.driver))
		return false;

	return to_vs_service_driver(service->dev.driver)->rx_atomic;
}

/**
 * vs_session_max_mbuf_size - Return the maximum allocation size of a message
 * buffer.
 * @service: The service to check
 */
static inline size_t
vs_service_max_mbuf_size(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);

	return session->transport->vt->max_mbuf_size(session->transport);
}

/**
 * vs_service_send_mbufs_available - Return the number of mbufs which can be
 * allocated for sending before going over quota.
 * @service: The service to check
 */
static inline ssize_t
vs_service_send_mbufs_available(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);

	return session->transport->vt->service_send_avail(session->transport,
			service);
}

/**
 * vs_service_has_atomic_tx - Return whether or not a service is allowed to
 * transmit from atomic context (other than its receive message handler).
 * This function should only be called for services which are bound to a
 * driver.
 *
 * @service: Service to check
 */
static inline bool
vs_service_has_atomic_tx(struct vs_service_device *service)
{
	if (WARN_ON(!service->dev.driver))
		return false;

	return to_vs_service_driver(service->dev.driver)->tx_atomic;
}

/**
 * vs_service_state_lock - Acquire a lock allowing service state operations
 * from external task contexts.
 *
 * @service: Service to lock.
 *
 * This must be used to protect any service state accesses that occur in task
 * contexts outside of a callback from the vservices protocol layer. It must
 * not be called from a protocol layer callback, nor from atomic context.
 *
 * If this service's state is also accessed from softirq contexts other than
 * vservices protocol layer callbacks, use vs_service_state_lock_bh instead,
 * and set the driver's tx_atomic flag.
 *
 * If this is called from outside the service's workqueue, the calling driver
 * must provide its own guarantee that it has not been detached from the
 * service. If that is not possible, use vs_state_lock_safe().
 */
static inline void
vs_service_state_lock(struct vs_service_device *service)
__acquires(service)
{
#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	WARN_ON_ONCE(vs_service_has_atomic_tx(service));
#endif

	mutex_lock_nested(&service->state_mutex, service->lock_subclass);

#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	if (WARN_ON_ONCE(service->state_spinlock_used))
		dev_err(&service->dev, "Service is using both the state spinlock and mutex - Fix your driver\n");
	service->state_mutex_used = true;
#endif

	if (vs_service_has_atomic_rx(service))
		tasklet_disable(&service->rx_tasklet);

	__acquire(service);
}

/**
 * vs_service_state_unlock - Release the lock acquired by vs_service_state_lock.
 *
 * @service: Service to unlock.
 */
static inline void
vs_service_state_unlock(struct vs_service_device *service)
__releases(service)
{
	__release(service);

	mutex_unlock(&service->state_mutex);

	if (vs_service_has_atomic_rx(service)) {
		tasklet_enable(&service->rx_tasklet);

		/* Kick the tasklet if there is RX work to do */
		if (!list_empty(&service->rx_queue))
			tasklet_schedule(&service->rx_tasklet);
	}
}

/**
 * vs_service_state_lock_bh - Acquire a lock allowing service state operations
 * from external task or softirq contexts.
 *
 * @service: Service to lock.
 *
 * This is an alternative to vs_service_state_lock for drivers that receive
 * messages in atomic context (i.e. have their rx_atomic flag set), *and* must
 * transmit messages from softirq contexts other than their own message
 * receive and tx_ready callbacks. Such drivers must set their tx_atomic
 * flag, so generated protocol drivers perform correct locking.
 *
 * This should replace all calls to vs_service_state_lock for services that
 * need it. Do not use both locking functions in one service driver.
 *
 * The calling driver must provide its own guarantee that it has not been
 * detached from the service. If that is not possible, use
 * vs_state_lock_safe_bh().
 */
static inline void
vs_service_state_lock_bh(struct vs_service_device *service)
__acquires(service)
__acquires(&service->state_spinlock)
{
#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	WARN_ON_ONCE(!vs_service_has_atomic_rx(service));
	WARN_ON_ONCE(!vs_service_has_atomic_tx(service));
#endif

#ifdef CONFIG_SMP
	/* Not necessary on UP because it's implied by spin_lock_bh(). */
	tasklet_disable(&service->rx_tasklet);
#endif

	spin_lock_bh(&service->state_spinlock);

#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	if (WARN_ON_ONCE(service->state_mutex_used))
		dev_err(&service->dev, "Service is using both the state spinlock and mutex - Fix your driver\n");
	service->state_spinlock_used = true;
#endif

	__acquire(service);
}

/**
 * vs_service_state_unlock_bh - Release the lock acquired by
 * vs_service_state_lock_bh.
 *
 * @service: Service to unlock.
 */
static inline void
vs_service_state_unlock_bh(struct vs_service_device *service)
__releases(service)
__releases(&service->state_spinlock)
{
	__release(service);

	spin_unlock_bh(&service->state_spinlock);

#ifdef CONFIG_SMP
	tasklet_enable(&service->rx_tasklet);
#endif
}

/* Convenience macros for locking a state structure rather than a service. */
#define vs_state_lock(state) vs_service_state_lock((state)->service)
#define vs_state_unlock(state) vs_service_state_unlock((state)->service)
#define vs_state_lock_bh(state) vs_service_state_lock_bh((state)->service)
#define vs_state_unlock_bh(state) vs_service_state_unlock_bh((state)->service)

/**
 * vs_state_lock_safe[_bh] - Aqcuire a lock for a state structure's service,
 * when the service may have been detached from the state.
 *
 * This is useful for blocking operations that can't easily be terminated
 * before returning from the service reset handler, such as file I/O. To use
 * this, the state structure should be reference-counted rather than freed in
 * the release callback, and the driver should retain its own reference to the
 * service until the state structure is freed.
 *
 * This macro acquires the lock and returns true if the state has not been
 * detached from the service. Otherwise, it returns false.
 *
 * Note that the _bh variant cannot be used from atomic context, because it
 * acquires a mutex.
 */
#define __vs_state_lock_safe(_state, _lock, _unlock) ({ \
	bool __ok = true;						\
	typeof(_state) __state = (_state);				\
	struct vs_service_device *__service = __state->service;		\
	mutex_lock_nested(&__service->ready_lock,			\
			__service->lock_subclass);			\
	__ok = !ACCESS_ONCE(__state->released);				\
	if (__ok) {							\
		_lock(__state);						\
		__ok = !ACCESS_ONCE(__state->released);			\
		if (!__ok)						\
			_unlock(__state);				\
	}								\
	mutex_unlock(&__service->ready_lock);				\
	__ok;								\
})
#define vs_state_lock_safe(_state) \
	__vs_state_lock_safe((_state), vs_state_lock, vs_state_unlock)
#define vs_state_lock_safe_bh(_state) \
	__vs_state_lock_safe((_state), vs_state_lock_bh, vs_state_unlock_bh)

/**
 * vs_get_service - Get a reference to a service.
 * @service: Service to get a reference to.
 */
static inline struct vs_service_device *
vs_get_service(struct vs_service_device *service)
{
	if (service)
		get_device(&service->dev);
	return service;
}

/**
 * vs_put_service - Put a reference to a service.
 * @service: The service to put the reference to.
 */
static inline void
vs_put_service(struct vs_service_device *service)
{
	put_device(&service->dev);
}

extern int vs_service_reset(struct vs_service_device *service,
		struct vs_service_device *caller);
extern void vs_service_reset_nosync(struct vs_service_device *service);

/**
 * vs_service_send_batch_start - Start a batch of outgoing messages
 * @service: The service that is starting a batch
 * @flush: Finish any previously started batch (if false, then duplicate
 * calls to this function have no effect)
 */
static inline void
vs_service_send_batch_start(struct vs_service_device *service, bool flush)
{
	if (flush && service->tx_batching) {
		struct vs_session_device *session =
			vs_service_get_session(service);
		const struct vs_transport_vtable *vt = session->transport->vt;
		if (vt->flush)
			vt->flush(session->transport, service);
	} else {
		service->tx_batching = true;
	}
}

/**
 * vs_service_send_batch_end - End a batch of outgoing messages
 * @service: The service that is ending a batch
 * @flush: Start sending the batch immediately (if false, the batch will
 * be flushed when the next message is sent)
 */
static inline void
vs_service_send_batch_end(struct vs_service_device *service, bool flush)
{
	service->tx_batching = false;
	if (flush) {
		struct vs_session_device *session =
			vs_service_get_session(service);
		const struct vs_transport_vtable *vt = session->transport->vt;
		if (vt->flush)
			vt->flush(session->transport, service);
	}
}


#endif /* _VSERVICE_SERVICE_H_ */
