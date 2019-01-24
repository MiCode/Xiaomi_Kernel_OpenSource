/*
 * drivers/vservices/session.c
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is the generic session-management code for the vServices framework.
 * It creates service and session devices on request from session and
 * transport drivers, respectively; it also queues incoming messages from the
 * transport and distributes them to the session's services.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/err.h>

#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/service.h>

#include "session.h"
#include "transport.h"
#include "compat.h"

/* Minimum required time between resets to avoid throttling */
#define RESET_THROTTLE_TIME msecs_to_jiffies(1000)

/*
 * Minimum/maximum reset throttling time. The reset throttle will start at
 * the minimum and increase to the maximum exponetially.
 */
#define RESET_THROTTLE_MIN RESET_THROTTLE_TIME
#define RESET_THROTTLE_MAX msecs_to_jiffies(8 * 1000)

/*
 * If the reset is being throttled and a sane reset (doesn't need throttling)
 * is requested, then if the service's reset delay mutliplied by this value
 * has elapsed throttling is disabled.
 */
#define RESET_THROTTLE_COOL_OFF_MULT 2

/* IDR of session ids to sessions */
static DEFINE_IDR(session_idr);
DEFINE_MUTEX(vs_session_lock);
EXPORT_SYMBOL_GPL(vs_session_lock);

/* Notifier list for vService session events */
static BLOCKING_NOTIFIER_HEAD(vs_session_notifier_list);

static unsigned long default_debug_mask;
module_param(default_debug_mask, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(default_debug_mask, "Default vServices debug mask");

/* vServices root in sysfs at /sys/vservices */
struct kobject *vservices_root;
EXPORT_SYMBOL_GPL(vservices_root);

/* vServices server root in sysfs at /sys/vservices/server-sessions */
struct kobject *vservices_server_root;
EXPORT_SYMBOL_GPL(vservices_server_root);

/* vServices client root in sysfs at /sys/vservices/client-sessions */
struct kobject *vservices_client_root;
EXPORT_SYMBOL_GPL(vservices_client_root);

#ifdef CONFIG_VSERVICES_CHAR_DEV
struct vs_service_device *vs_service_lookup_by_devt(dev_t dev)
{
	struct vs_session_device *session;
	struct vs_service_device *service;

	mutex_lock(&vs_session_lock);
	session = idr_find(&session_idr, MINOR(dev) / VS_MAX_SERVICES);
	get_device(&session->dev);
	mutex_unlock(&vs_session_lock);

	service = vs_session_get_service(session,
			MINOR(dev) % VS_MAX_SERVICES);
	put_device(&session->dev);

	return service;
}
#endif

struct vs_session_for_each_data {
	int (*fn)(struct vs_session_device *session, void *data);
	void *data;
};

int vs_session_for_each_from_idr(int id, void *session, void *_data)
{
	struct vs_session_for_each_data *data =
		(struct vs_session_for_each_data *)_data;
	return data->fn(session, data->data);
}

/**
 * vs_session_for_each_locked - call a callback function for each session
 * @fn: function to call
 * @data: opaque pointer that is passed through to the function
 */
extern int vs_session_for_each_locked(
		int (*fn)(struct vs_session_device *session, void *data),
		void *data)
{
	struct vs_session_for_each_data priv = { .fn = fn, .data = data };

	lockdep_assert_held(&vs_session_lock);

	return idr_for_each(&session_idr, vs_session_for_each_from_idr,
			&priv);
}
EXPORT_SYMBOL(vs_session_for_each_locked);

/**
 * vs_register_notify - register a notifier callback for vServices events
 * @nb: pointer to the notifier block for the callback events.
 */
void vs_session_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&vs_session_notifier_list, nb);
}
EXPORT_SYMBOL(vs_session_register_notify);

/**
 * vs_unregister_notify - unregister a notifier callback for vServices events
 * @nb: pointer to the notifier block for the callback events.
 */
void vs_session_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&vs_session_notifier_list, nb);
}
EXPORT_SYMBOL(vs_session_unregister_notify);

/*
 * Helper function for returning how long ago something happened
 * Marked as __maybe_unused since this is only needed when
 * CONFIG_VSERVICES_DEBUG is enabled, but cannot be removed because it
 * will cause compile time errors.
 */
static __maybe_unused unsigned msecs_ago(unsigned long jiffy_value)
{
	return jiffies_to_msecs(jiffies - jiffy_value);
}

static void session_fatal_error_work(struct work_struct *work)
{
	struct vs_session_device *session = container_of(work,
			struct vs_session_device, fatal_error_work);

	session->transport->vt->reset(session->transport);
}

static void session_fatal_error(struct vs_session_device *session, gfp_t gfp)
{
	schedule_work(&session->fatal_error_work);
}

/*
 * Service readiness state machine
 *
 * The states are:
 *
 * INIT: Initial state. Service may not be completely configured yet
 * (typically because the protocol hasn't been set); call vs_service_start
 * once configuration is complete. The disable count must be nonzero, and
 * must never reach zero in this state.
 * DISABLED: Service is not permitted to communicate. Non-core services are
 * in this state whenever the core protocol and/or transport state does not
 * allow them to be active; core services are only in this state transiently.
 * The disable count must be nonzero; when it reaches zero, the service
 * transitions to RESET state.
 * RESET: Service drivers are inactive at both ends, but the core service
 * state allows the service to become active. The session will schedule a
 * future transition to READY state when entering this state, but the
 * transition may be delayed to throttle the rate at which resets occur.
 * READY: All core-service and session-layer policy allows the service to
 * communicate; it will become active as soon as it has a protocol driver.
 * ACTIVE: The driver is present and communicating.
 * LOCAL_RESET: We have initiated a reset at this end, but the remote end has
 * not yet acknowledged it. We will enter the RESET state on receiving
 * acknowledgement, unless the disable count is nonzero in which case we
 * will enter DISABLED state.
 * LOCAL_DELETE: As for LOCAL_RESET, but we will enter the DELETED state
 * instead of RESET or DISABLED.
 * DELETED: The service is no longer present on the session; the service
 * device structure may still exist because something is holding a reference
 * to it.
 *
 * The permitted transitions are:
 *
 * From          To            Trigger
 * INIT          DISABLED      vs_service_start
 * DISABLED      RESET         vs_service_enable (disable_count -> 0)
 * RESET         READY         End of throttle delay (may be 0)
 * READY         ACTIVE        Latter of probe() and entering READY
 * {READY, ACTIVE}
 *               LOCAL_RESET   vs_service_reset
 * {READY, ACTIVE, LOCAL_RESET}
 *               RESET         vs_service_handle_reset (server)
 * RESET         DISABLED      vs_service_disable (server)
 * {READY, ACTIVE, LOCAL_RESET}
 *               DISABLED      vs_service_handle_reset (client)
 * {INIT, RESET, READY, ACTIVE, LOCAL_RESET}
 *               DISABLED      vs_service_disable_noncore
 * {ACTIVE, LOCAL_RESET}
 *               LOCAL_DELETE  vs_service_delete
 * {INIT, DISABLED, RESET, READY}
 *               DELETED       vs_service_delete
 * LOCAL_DELETE  DELETED       vs_service_handle_reset
 *                             vs_service_disable_noncore
 *
 * See the documentation for the triggers for details.
 */

enum vs_service_readiness {
	VS_SERVICE_INIT,
	VS_SERVICE_DISABLED,
	VS_SERVICE_RESET,
	VS_SERVICE_READY,
	VS_SERVICE_ACTIVE,
	VS_SERVICE_LOCAL_RESET,
	VS_SERVICE_LOCAL_DELETE,
	VS_SERVICE_DELETED,
};

/* Session activation states. */
enum {
	VS_SESSION_RESET,
	VS_SESSION_ACTIVATE,
	VS_SESSION_ACTIVE,
};

/**
 * vs_service_start - Start a service by moving it from the init state to the
 * disabled state.
 *
 * @service: The service to start.
 *
 * Returns true if the service was started, or false if it was not.
 */
bool vs_service_start(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);

	WARN_ON(!service->protocol);

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	if (service->readiness != VS_SERVICE_INIT) {
		if (service->readiness != VS_SERVICE_DELETED)
			dev_err(&service->dev,
					"start called from invalid state %d\n",
					service->readiness);
		mutex_unlock(&service->ready_lock);
		return false;
	}

	if (service->id != 0 && session_drv->service_added) {
		int err = session_drv->service_added(session, service);
		if (err < 0) {
			dev_err(&session->dev, "Failed to add service %d: %d\n",
					service->id, err);
			mutex_unlock(&service->ready_lock);
			return false;
		}
	}

	service->readiness = VS_SERVICE_DISABLED;
	service->disable_count = 1;
	service->last_reset_request = jiffies;

	mutex_unlock(&service->ready_lock);

	/* Tell userspace about the service. */
	dev_set_uevent_suppress(&service->dev, false);
	kobject_uevent(&service->dev.kobj, KOBJ_ADD);

	return true;
}
EXPORT_SYMBOL_GPL(vs_service_start);

static void cancel_pending_rx(struct vs_service_device *service);
static void queue_ready_work(struct vs_service_device *service);

static void __try_start_service(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);
	struct vs_transport *transport;
	int err;
	struct vs_service_driver *driver;

	lockdep_assert_held(&service->ready_lock);

	/* We can't start if the service is not ready yet. */
	if (service->readiness != VS_SERVICE_READY)
		return;

	/*
	 * There should never be anything in the RX queue at this point.
	 * If there is, it can seriously confuse the service drivers for
	 * no obvious reason, so we check.
	 */
	if (WARN_ON(!list_empty(&service->rx_queue)))
		cancel_pending_rx(service);

	if (!service->driver_probed) {
		vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev,
				"ready with no driver\n");
		return;
	}

	/* Prepare the transport to support the service. */
	transport = session->transport;
	err = transport->vt->service_start(transport, service);

	if (err < 0) {
		/* fatal error attempting to start; reset and try again */
		service->readiness = VS_SERVICE_RESET;
		service->last_reset_request = jiffies;
		service->last_reset = jiffies;
		queue_ready_work(service);

		return;
	}

	service->readiness = VS_SERVICE_ACTIVE;

	driver = to_vs_service_driver(service->dev.driver);
	if (driver->start)
		driver->start(service);

	if (service->id && session_drv->service_start) {
		err = session_drv->service_start(session, service);
		if (err < 0) {
			dev_err(&session->dev, "Failed to start service %s (%d): %d\n",
					dev_name(&service->dev),
					service->id, err);
			session_fatal_error(session, GFP_KERNEL);
		}
	}
}

static void try_start_service(struct vs_service_device *service)
{
	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	__try_start_service(service);

	mutex_unlock(&service->ready_lock);
}

static void service_ready_work(struct work_struct *work)
{
	struct vs_service_device *service = container_of(work,
			struct vs_service_device, ready_work.work);
	struct vs_session_device *session = vs_service_get_session(service);

	vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev,
			"ready work - last reset request was %u ms ago\n",
			msecs_ago(service->last_reset_request));

	/*
	 * Make sure there's no reset work pending from an earlier driver
	 * failure. We should already be inactive at this point, so it's safe
	 * to just cancel it.
	 */
	cancel_work_sync(&service->reset_work);

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	if (service->readiness != VS_SERVICE_RESET) {
		vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev,
				"ready work found readiness of %d, doing nothing\n",
				service->readiness);
		mutex_unlock(&service->ready_lock);
		return;
	}

	service->readiness = VS_SERVICE_READY;
	/* Record the time at which this happened, for throttling. */
	service->last_ready = jiffies;

	/* Tell userspace that the service is ready. */
	kobject_uevent(&service->dev.kobj, KOBJ_ONLINE);

	/* Start the service, if it has a driver attached. */
	__try_start_service(service);

	mutex_unlock(&service->ready_lock);
}

static int __enable_service(struct vs_service_device *service);

/**
 * __reset_service - make a service inactive, and tell its driver, the
 * transport, and possibly the remote partner
 * @service:       The service to reset
 * @notify_remote: If true, the partner is notified of the reset
 *
 * This routine is called to make an active service inactive. If the given
 * service is currently active, it drops any queued messages for the service,
 * and then informs the service driver and the transport layer that the
 * service has reset. It sets the service readiness to VS_SERVICE_LOCAL_RESET
 * to indicate that the driver is no longer active.
 *
 * This routine has no effect on services that are not active.
 *
 * The caller must hold the target service's ready lock.
 */
static void __reset_service(struct vs_service_device *service,
		bool notify_remote)
{
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);
	struct vs_service_driver *driver = NULL;
	struct vs_transport *transport;
	int err;

	lockdep_assert_held(&service->ready_lock);

	/* If we're already inactive, there's nothing to do. */
	if (service->readiness != VS_SERVICE_ACTIVE)
		return;

	service->last_reset = jiffies;
	service->readiness = VS_SERVICE_LOCAL_RESET;

	cancel_pending_rx(service);

	if (!WARN_ON(!service->driver_probed))
		driver = to_vs_service_driver(service->dev.driver);

	if (driver && driver->reset)
		driver->reset(service);

	wake_up_all(&service->quota_wq);

	transport = vs_service_get_session(service)->transport;

	/*
	 * Ask the transport to reset the service. If this returns a positive
	 * value, we need to leave the service disabled, and the transport
	 * will re-enable it. To avoid allowing the disable count to go
	 * negative if that re-enable races with this callback returning, we
	 * disable the service beforehand and re-enable it if the callback
	 * returns zero.
	 */
	service->disable_count++;
	err = transport->vt->service_reset(transport, service);
	if (err < 0) {
		dev_err(&session->dev, "Failed to reset service %d: %d (transport)\n",
				service->id, err);
		session_fatal_error(session, GFP_KERNEL);
	} else if (!err) {
		err = __enable_service(service);
	}

	if (notify_remote) {
		if (service->id) {
			err = session_drv->service_local_reset(session,
					service);
			if (err == VS_SERVICE_ALREADY_RESET) {
				service->readiness = VS_SERVICE_RESET;
                                service->last_reset = jiffies;
                                queue_ready_work(service);

			} else if (err < 0) {
				dev_err(&session->dev, "Failed to reset service %d: %d (session)\n",
						service->id, err);
				session_fatal_error(session, GFP_KERNEL);
			}
		} else {
			session->transport->vt->reset(session->transport);
		}
	}

	/* Tell userspace that the service is no longer active. */
	kobject_uevent(&service->dev.kobj, KOBJ_OFFLINE);
}

/**
 * reset_service - reset a service and inform the remote partner
 * @service: The service to reset
 *
 * This routine is called when a reset is locally initiated (other than
 * implicitly by a session / core service reset). It bumps the reset request
 * timestamp, acquires the necessary locks, and calls __reset_service.
 *
 * This routine returns with the service ready lock held, to allow the caller
 * to make any other state changes that must be atomic with the service
 * reset.
 */
static void reset_service(struct vs_service_device *service)
	__acquires(service->ready_lock)
{
	service->last_reset_request = jiffies;

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	__reset_service(service, true);
}

/**
 * vs_service_reset - initiate a service reset
 * @service: the service that is to be reset
 * @caller: the service that is initiating the reset
 *
 * This routine informs the partner that the given service is being reset,
 * then disables and flushes the service's receive queues and resets its
 * driver. The service will be automatically re-enabled once the partner has
 * acknowledged the reset (see vs_session_handle_service_reset, above).
 *
 * If the given service is the core service, this will perform a transport
 * reset, which implicitly resets (on the server side) or destroys (on
 * the client side) every other service on the session.
 *
 * If the given service is already being reset, this has no effect, other
 * than to delay completion of the reset if it is being throttled.
 *
 * For lock safety reasons, a service can only be directly reset by itself,
 * the core service, or the service that created it (which is typically also
 * the core service).
 *
 * A service that wishes to reset itself must not do so while holding its state
 * lock or while running on its own workqueue. In these circumstances, call
 * vs_service_reset_nosync() instead. Note that returning an error code
 * (any negative number) from a driver callback forces a call to
 * vs_service_reset_nosync() and prints an error message.
 */
int vs_service_reset(struct vs_service_device *service,
		struct vs_service_device *caller)
{
	struct vs_session_device *session = vs_service_get_session(service);

	if (caller != service && caller != service->owner) {
		struct vs_service_device *core_service = session->core_service;

		WARN_ON(!core_service);
		if (caller != core_service)
			return -EPERM;
	}

	reset_service(service);
	/* reset_service returns with ready_lock held, but we don't need it */
	mutex_unlock(&service->ready_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_service_reset);

/**
 * vs_service_reset_nosync - asynchronously reset a service.
 * @service: the service that is to be reset
 *
 * This routine triggers a reset for the nominated service. It may be called
 * from any context, including interrupt context. It does not wait for the
 * reset to occur, and provides no synchronisation guarantees when called from
 * outside the target service.
 *
 * This is intended only for service drivers that need to reset themselves
 * from a context that would not normally allow it. In other cases, use
 * vs_service_reset.
 */
void vs_service_reset_nosync(struct vs_service_device *service)
{
	service->pending_reset = true;
	schedule_work(&service->reset_work);
}
EXPORT_SYMBOL_GPL(vs_service_reset_nosync);

static void
vs_service_remove_sysfs_entries(struct vs_session_device *session,
		struct vs_service_device *service)
{
	sysfs_remove_link(session->sysfs_entry, service->sysfs_name);
	sysfs_remove_link(&service->dev.kobj, VS_SESSION_SYMLINK_NAME);
}

static void vs_session_release_service_id(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);

	mutex_lock(&session->service_idr_lock);
	idr_remove(&session->service_idr, service->id);
	mutex_unlock(&session->service_idr_lock);
	vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev,
			"service id deallocated\n");
}

static void destroy_service(struct vs_service_device *service,
		bool notify_remote)
{
	struct vs_session_device *session = vs_service_get_session(service);
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);
	struct vs_service_device *core_service __maybe_unused =
			session->core_service;
	int err;

	lockdep_assert_held(&service->ready_lock);
	WARN_ON(service->readiness != VS_SERVICE_DELETED);

	/* Notify the core service and transport that the service is gone */
	session->transport->vt->service_remove(session->transport, service);
	if (notify_remote && service->id && session_drv->service_removed) {
		err = session_drv->service_removed(session, service);
		if (err < 0) {
			dev_err(&session->dev,
					"Failed to remove service %d: %d\n",
					service->id, err);
			session_fatal_error(session, GFP_KERNEL);
		}
	}

	/*
	 * At this point the service is guaranteed to be gone on the client
	 * side, so we can safely release the service ID.
	 */
	if (session->is_server)
		vs_session_release_service_id(service);

	/*
	 * This guarantees that any concurrent vs_session_get_service() that
	 * found the service before we removed it from the IDR will take a
	 * reference before we release ours.
	 *
	 * This similarly protects for_each_[usable_]service().
	 */
	synchronize_rcu();

	/* Matches device_initialize() in vs_service_register() */
	put_device(&service->dev);
}

/**
 * disable_service - prevent a service becoming ready
 * @service: the service that is to be disabled
 * @force: true if the service is known to be in reset
 *
 * This routine may be called for any inactive service. Once disabled, the
 * service cannot be made ready by the session, and thus cannot become active,
 * until vs_service_enable() is called for it. If multiple calls are made to
 * this function, they must be balanced by vs_service_enable() calls.
 *
 * If the force option is true, then any pending unacknowledged reset will be
 * presumed to have been acknowledged. This is used when the core service is
 * entering reset.
 *
 * This is used by the core service client to prevent the service restarting
 * until the server is ready (i.e., a server_ready message is received); by
 * the session layer to stop all communication while the core service itself
 * is in reset; and by the transport layer when the transport was unable to
 * complete reset of a service in its reset callback (typically because
 * a service had passed message buffers to another Linux subsystem and could
 * not free them immediately).
 *
 * In any case, there is no need for the operation to be signalled in any
 * way, because the service is already in reset. It simply delays future
 * signalling of service readiness.
 */
static void disable_service(struct vs_service_device *service, bool force)
{
	lockdep_assert_held(&service->ready_lock);

	switch(service->readiness) {
	case VS_SERVICE_INIT:
	case VS_SERVICE_DELETED:
	case VS_SERVICE_LOCAL_DELETE:
		dev_err(&service->dev, "disabled while uninitialised\n");
		break;
	case VS_SERVICE_ACTIVE:
		dev_err(&service->dev, "disabled while active\n");
		break;
	case VS_SERVICE_LOCAL_RESET:
		/*
		 * Will go to DISABLED state when reset completes, unless
		 * it's being forced (i.e. we're moving to a core protocol
		 * state that implies everything else is reset).
		 */
		if (force)
			service->readiness = VS_SERVICE_DISABLED;
		service->disable_count++;
		break;
	default:
		service->readiness = VS_SERVICE_DISABLED;
		service->disable_count++;
		break;
	}

	cancel_delayed_work(&service->ready_work);
}

static int service_handle_reset(struct vs_session_device *session,
		struct vs_service_device *target, bool disable)
{
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);
	int err = 0;

	mutex_lock_nested(&target->ready_lock, target->lock_subclass);

	switch (target->readiness) {
	case VS_SERVICE_LOCAL_DELETE:
		target->readiness = VS_SERVICE_DELETED;
		destroy_service(target, true);
		break;
	case VS_SERVICE_ACTIVE:
		/*
		 * Reset the service and send a reset notification.
		 *
		 * We only send notifications for non-core services. This is
		 * because core notifies by sending a transport reset, which
		 * is what brought us here in the first place. Note that we
		 * must already hold the core service state lock iff the
		 * target is non-core.
		 */
		target->last_reset_request = jiffies;
		__reset_service(target, target->id != 0);
		/* fall through */
	case VS_SERVICE_LOCAL_RESET:
		target->readiness = target->disable_count ?
			VS_SERVICE_DISABLED : VS_SERVICE_RESET;
		if (disable)
			disable_service(target, false);
		if (target->readiness != VS_SERVICE_DISABLED)
			queue_ready_work(target);
		break;
	case VS_SERVICE_READY:
		/* Tell userspace that the service is no longer ready. */
		kobject_uevent(&target->dev.kobj, KOBJ_OFFLINE);
		/* fall through */
	case VS_SERVICE_RESET:
		/*
		 * This can happen for a non-core service if we get a reset
		 * request from the server on the client side, after the
		 * client has enabled the service but before it is active.
		 * Note that the service is already active on the server side
		 * at this point. The client's delay may be due to either
		 * reset throttling or the absence of a driver.
		 *
		 * We bump the reset request timestamp, disable the service
		 * again, and send back an acknowledgement.
		 */
		if (disable && target->id) {
			target->last_reset_request = jiffies;

			err = session_drv->service_local_reset(
					session, target);
			if (err < 0) {
				dev_err(&session->dev,
						"Failed to reset service %d; %d\n",
						target->id, err);
				session_fatal_error(session,
						GFP_KERNEL);
			}

			disable_service(target, false);
			break;
		}
		/* fall through */
	case VS_SERVICE_DISABLED:
		/*
		 * This can happen for the core service if we get a reset
		 * before the transport has activated, or before the core
		 * service has become ready.
		 *
		 * We bump the reset request timestamp, and disable the
		 * service again if the transport had already activated and
		 * enabled it.
		 */
		if (disable && !target->id) {
			target->last_reset_request = jiffies;

			if (target->readiness != VS_SERVICE_DISABLED)
				disable_service(target, false);

			break;
		}
		/* fall through */
	default:
		dev_warn(&target->dev, "remote reset while inactive (%d)\n",
				target->readiness);
		err = -EPROTO;
		break;
	}

	mutex_unlock(&target->ready_lock);
	return err;
}

/**
 * vs_service_handle_reset - handle an incoming notification of a reset
 * @session: the session that owns the service
 * @service_id: the ID of the service that is to be reset
 * @disable: if true, the service will not be automatically re-enabled
 *
 * This routine is called by the core service when the remote end notifies us
 * of a non-core service reset. The service must be in ACTIVE, LOCAL_RESET or
 * LOCAL_DELETED state. It must be called with the core service's state lock
 * held.
 *
 * If the service was in ACTIVE state, the core service is called back to send
 * a notification to the other end. If it was in LOCAL_DELETED state, it is
 * unregistered.
 */
int vs_service_handle_reset(struct vs_session_device *session,
		vs_service_id_t service_id, bool disable)
{
	struct vs_service_device *target;
	int ret;

	if (!service_id)
		return -EINVAL;

	target = vs_session_get_service(session, service_id);
	if (!target)
		return -ENODEV;

	ret = service_handle_reset(session, target, disable);
	vs_put_service(target);
	return ret;
}
EXPORT_SYMBOL_GPL(vs_service_handle_reset);

static int __enable_service(struct vs_service_device *service)
{
	if (WARN_ON(!service->disable_count))
		return -EINVAL;

	if (--service->disable_count > 0)
		return 0;

	/*
	 * If the service is still resetting, it can't become ready until the
	 * reset completes. If it has been deleted, it will never become
	 * ready. In either case, there's nothing more to do.
	 */
	if ((service->readiness == VS_SERVICE_LOCAL_RESET) ||
			(service->readiness == VS_SERVICE_LOCAL_DELETE) ||
			(service->readiness == VS_SERVICE_DELETED))
		return 0;

	if (WARN_ON(service->readiness != VS_SERVICE_DISABLED))
		return -EINVAL;

	service->readiness = VS_SERVICE_RESET;
	service->last_reset = jiffies;
	queue_ready_work(service);

	return 0;
}

/**
 * vs_service_enable - allow a service to become ready
 * @service: the service that is to be enabled
 *
 * Calling this routine for a service permits the session layer to make the
 * service ready. It will do so as soon as any outstanding reset throttling
 * is complete, and will then start the service once it has a driver attached.
 *
 * Services are disabled, requiring a call to this routine to re-enable them:
 * - when first initialised (after vs_service_start),
 * - when reset on the client side by vs_service_handle_reset,
 * - when the transport has delayed completion of a reset, and
 * - when the server-side core protocol is disconnected or reset by
 *   vs_session_disable_noncore.
 */
int vs_service_enable(struct vs_service_device *service)
{
	int ret;

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	ret = __enable_service(service);

	mutex_unlock(&service->ready_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(vs_service_enable);

/*
 * Service work functions
 */
static void queue_rx_work(struct vs_service_device *service)
{
	bool rx_atomic;

	rx_atomic = vs_service_has_atomic_rx(service);
	vs_dev_debug(VS_DEBUG_SESSION, vs_service_get_session(service),
			&service->dev, "Queuing rx %s\n",
			rx_atomic ? "tasklet (atomic)" : "work (cansleep)");

	if (rx_atomic)
		tasklet_schedule(&service->rx_tasklet);
	else
		queue_work(service->work_queue, &service->rx_work);
}

static void cancel_pending_rx(struct vs_service_device *service)
{
	struct vs_mbuf *mbuf;

	lockdep_assert_held(&service->ready_lock);

	cancel_work_sync(&service->rx_work);
	tasklet_kill(&service->rx_tasklet);

	spin_lock_irq(&service->rx_lock);
	while (!list_empty(&service->rx_queue)) {
		mbuf = list_first_entry(&service->rx_queue,
				struct vs_mbuf, queue);
		list_del_init(&mbuf->queue);
		spin_unlock_irq(&service->rx_lock);
		vs_service_free_mbuf(service, mbuf);
		spin_lock_irq(&service->rx_lock);
	}
	service->tx_ready = false;
	spin_unlock_irq(&service->rx_lock);
}

static bool reset_throttle_cooled_off(struct vs_service_device *service);
static unsigned long reset_cool_off(struct vs_service_device *service);

static void service_cooloff_work(struct work_struct *work)
{
	struct vs_service_device *service = container_of(work,
			struct vs_service_device, cooloff_work.work);
	struct vs_session_device *session = vs_service_get_session(service);
	unsigned long current_time = jiffies, wake_time;

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	if (reset_throttle_cooled_off(service)) {
		vs_debug(VS_DEBUG_SESSION, session,
				"Reset thrashing cooled off (delay = %u ms, cool off = %u ms, last reset %u ms ago, last reset request was %u ms ago)\n",
				jiffies_to_msecs(service->reset_delay),
				jiffies_to_msecs(reset_cool_off(service)),
				msecs_ago(service->last_reset),
				msecs_ago(service->last_reset_request));

		service->reset_delay = 0;

		/*
		 * If the service is already in reset, then queue_ready_work
		 * has already run and has deferred queuing of the ready_work
		 * until cooloff. Schedule the ready work to run immediately.
		 */
		if (service->readiness == VS_SERVICE_RESET)
			schedule_delayed_work(&service->ready_work, 0);
	} else {
		/*
		 * This can happen if last_reset_request has been bumped
		 * since the cooloff work was first queued. We need to
		 * work out how long it is until the service cools off,
		 * then reschedule ourselves.
		 */
		wake_time = reset_cool_off(service) +
				service->last_reset_request;

		WARN_ON(time_after(current_time, wake_time));

		schedule_delayed_work(&service->cooloff_work,
				wake_time - current_time);
	}

	mutex_unlock(&service->ready_lock);
}

static void
service_reset_work(struct work_struct *work)
{
	struct vs_service_device *service = container_of(work,
			struct vs_service_device, reset_work);

	service->pending_reset = false;

	vs_service_reset(service, service);
}

/* Returns true if there are more messages to handle */
static bool
dequeue_and_handle_received_message(struct vs_service_device *service)
{
	struct vs_service_driver *driver =
			to_vs_service_driver(service->dev.driver);
	struct vs_session_device *session = vs_service_get_session(service);
	const struct vs_transport_vtable *vt = session->transport->vt;
	struct vs_service_stats *stats = &service->stats;
	struct vs_mbuf *mbuf;
	size_t size;
	int ret;

	/* Don't do rx work unless the service is active */
	if (service->readiness != VS_SERVICE_ACTIVE)
		return false;

	/* Atomically take an item from the queue */
	spin_lock_irq(&service->rx_lock);
	if (!list_empty(&service->rx_queue)) {
		mbuf = list_first_entry(&service->rx_queue, struct vs_mbuf,
				queue);
		list_del_init(&mbuf->queue);
		spin_unlock_irq(&service->rx_lock);
		size = vt->mbuf_size(mbuf);

		/*
		 * Call the message handler for the service. The service's
		 * message handler is responsible for freeing the mbuf when it
		 * is done with it.
		 */
		ret = driver->receive(service, mbuf);
		if (ret < 0) {
			atomic_inc(&service->stats.recv_failures);
			dev_err(&service->dev,
					"receive returned %d; resetting service\n",
					ret);
			vs_service_reset_nosync(service);
			return false;
		} else {
			atomic_add(size, &service->stats.recv_bytes);
			atomic_inc(&service->stats.recv_mbufs);
		}

	} else if (service->tx_ready) {
		service->tx_ready = false;
		spin_unlock_irq(&service->rx_lock);

		/*
		 * Update the tx_ready stats accounting and then call the
		 * service's tx_ready handler.
		 */
		atomic_inc(&stats->nr_tx_ready);
		if (atomic_read(&stats->nr_over_quota) > 0) {
			int total;

			total = atomic_add_return(jiffies_to_msecs(jiffies -
							stats->over_quota_time),
					&stats->over_quota_time_total);
			atomic_set(&stats->over_quota_time_avg, total /
					atomic_read(&stats->nr_over_quota));
		}
		atomic_set(&service->is_over_quota, 0);

		/*
		 * Note that a service's quota may reduce at any point, even
		 * during the tx_ready handler. This is important if a service
		 * has an ordered list of pending messages to send. If a
		 * message fails to send from the tx_ready handler due to
		 * over-quota then subsequent messages in the same handler may
		 * send successfully. To avoid sending messages in the
		 * incorrect order the service's tx_ready handler should
		 * return immediately if a message fails to send.
		 */
		ret = driver->tx_ready(service);
		if (ret < 0) {
			dev_err(&service->dev,
					"tx_ready returned %d; resetting service\n",
					ret);
			vs_service_reset_nosync(service);
			return false;
		}
	} else {
		spin_unlock_irq(&service->rx_lock);
	}

	/*
	 * There's no need to lock for this list_empty: if we race
	 * with a msg enqueue, we'll be rescheduled by the other side,
	 * and if we race with a dequeue, we'll just do nothing when
	 * we run (or will be cancelled before we run).
	 */
	return !list_empty(&service->rx_queue) || service->tx_ready;
}

static void service_rx_tasklet(unsigned long data)
{
	struct vs_service_device *service = (struct vs_service_device *)data;
	bool resched;

	/*
	 * There is no need to acquire the state spinlock or mutex here,
	 * because this tasklet is disabled when the lock is held. These
	 * are annotations for sparse and lockdep, respectively.
	 *
	 * We can't annotate the implicit mutex acquire because lockdep gets
	 * upset about inconsistent softirq states.
	 */
	__acquire(service);
	spin_acquire(&service->state_spinlock.dep_map, 0, 0, _THIS_IP_);

	resched = dequeue_and_handle_received_message(service);

	if (resched)
		tasklet_schedule(&service->rx_tasklet);

	spin_release(&service->state_spinlock.dep_map, 0, _THIS_IP_);
	__release(service);
}

static void service_rx_work(struct work_struct *work)
{
	struct vs_service_device *service = container_of(work,
			struct vs_service_device, rx_work);
	bool requeue;

	/*
	 * We must acquire the state mutex here to protect services that
	 * are using vs_service_state_lock().
	 *
	 * There is no need to acquire the spinlock, which is never used in
	 * drivers with task context receive handlers.
	 */
	vs_service_state_lock(service);

	requeue = dequeue_and_handle_received_message(service);

	vs_service_state_unlock(service);

	if (requeue)
		queue_work(service->work_queue, work);
}

/*
 * Service sysfs statistics counters. These files are all atomic_t, and
 * read only, so we use a generator macro to avoid code duplication.
 */
#define service_stat_attr(__name)					\
	static ssize_t service_stat_##__name##_show(struct device *dev, \
			struct device_attribute *attr, char *buf)       \
	{                                                               \
		struct vs_service_device *service =                     \
				to_vs_service_device(dev);              \
									\
		return scnprintf(buf, PAGE_SIZE, "%u\n",		\
				atomic_read(&service->stats.__name));	\
	}                                                               \
	static DEVICE_ATTR(__name, S_IRUGO,                             \
			service_stat_##__name##_show, NULL);

service_stat_attr(sent_mbufs);
service_stat_attr(sent_bytes);
service_stat_attr(recv_mbufs);
service_stat_attr(recv_bytes);
service_stat_attr(nr_over_quota);
service_stat_attr(nr_tx_ready);
service_stat_attr(over_quota_time_total);
service_stat_attr(over_quota_time_avg);

static struct attribute *service_stat_dev_attrs[] = {
	&dev_attr_sent_mbufs.attr,
	&dev_attr_sent_bytes.attr,
	&dev_attr_recv_mbufs.attr,
	&dev_attr_recv_bytes.attr,
	&dev_attr_nr_over_quota.attr,
	&dev_attr_nr_tx_ready.attr,
	&dev_attr_over_quota_time_total.attr,
	&dev_attr_over_quota_time_avg.attr,
	NULL,
};

static const struct attribute_group service_stat_attributes = {
	.name   = "stats",
	.attrs  = service_stat_dev_attrs,
};

static void delete_service(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);
	bool notify_on_destroy = true;

	/* FIXME: Jira ticket SDK-3495 - philipd. */
	/* This should be the caller's responsibility */
	vs_get_service(service);

	mutex_lock_nested(&service->ready_lock, service->lock_subclass);

	/*
	 * If we're on the client side, the service should already have been
	 * disabled at this point.
	 */
	WARN_ON(service->id != 0 && !session->is_server &&
			service->readiness != VS_SERVICE_DISABLED &&
			service->readiness != VS_SERVICE_DELETED);

	/*
	 * Make sure the service is not active, and notify the remote end if
	 * it needs to be reset. Note that we already hold the core service
	 * state lock iff this is a non-core service.
	 */
	__reset_service(service, true);

	/*
	 * If the remote end is aware that the service is inactive, we can
	 * delete right away; otherwise we need to wait for a notification
	 * that the service has reset.
	 */
	switch (service->readiness) {
	case VS_SERVICE_LOCAL_DELETE:
	case VS_SERVICE_DELETED:
		/* Nothing to do here */
		mutex_unlock(&service->ready_lock);
		vs_put_service(service);
		return;
	case VS_SERVICE_ACTIVE:
		BUG();
		break;
	case VS_SERVICE_LOCAL_RESET:
		service->readiness = VS_SERVICE_LOCAL_DELETE;
		break;
	case VS_SERVICE_INIT:
		notify_on_destroy = false;
		/* Fall through */
	default:
		service->readiness = VS_SERVICE_DELETED;
		destroy_service(service, notify_on_destroy);
		break;
	}

	mutex_unlock(&service->ready_lock);

	/*
	 * Remove service syslink from
	 * sys/vservices/(<server>/<client>)-sessions/ directory
	 */
	vs_service_remove_sysfs_entries(session, service);

	sysfs_remove_group(&service->dev.kobj, &service_stat_attributes);

	/*
	 * On the client-side we need to release the service id as soon as
	 * the service is deleted. Otherwise the server may attempt to create
	 * a new service with this id.
	 */
	if (!session->is_server)
		vs_session_release_service_id(service);

	device_del(&service->dev);
	vs_put_service(service);
}

/**
 * vs_service_delete - deactivate and start removing a service device
 * @service: the service to delete
 * @caller: the service initiating deletion
 *
 * Services may only be deleted by their owner (on the server side), or by the
 * core service. This function must not be called for the core service.
 */
int vs_service_delete(struct vs_service_device *service,
		struct vs_service_device *caller)
{
	struct vs_session_device *session =
			vs_service_get_session(service);
	struct vs_service_device *core_service = session->core_service;

	if (WARN_ON(!core_service))
		return -ENODEV;

	if (!service->id)
		return -EINVAL;

	if (caller != service->owner && caller != core_service)
		return -EPERM;

	delete_service(service);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_service_delete);

/**
 * vs_service_handle_delete - deactivate and start removing a service device
 * @service: the service to delete
 *
 * This is a variant of vs_service_delete which must only be called by the
 * core service. It is used by the core service client when a service_removed
 * message is received.
 */
int vs_service_handle_delete(struct vs_service_device *service)
{
	struct vs_session_device *session __maybe_unused =
			vs_service_get_session(service);
	struct vs_service_device *core_service __maybe_unused =
			session->core_service;

	lockdep_assert_held(&core_service->state_mutex);

	delete_service(service);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_service_handle_delete);

static void service_cleanup_work(struct work_struct *work)
{
	struct vs_service_device *service = container_of(work,
			struct vs_service_device, cleanup_work);
	struct vs_session_device *session = vs_service_get_session(service);

	vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev, "cleanup\n");

	if (service->owner)
		vs_put_service(service->owner);

	/* Put our reference to the session */
	if (service->dev.parent)
		put_device(service->dev.parent);

	tasklet_kill(&service->rx_tasklet);
	cancel_work_sync(&service->rx_work);
	cancel_delayed_work_sync(&service->cooloff_work);
	cancel_delayed_work_sync(&service->ready_work);
	cancel_work_sync(&service->reset_work);

	if (service->work_queue)
		destroy_workqueue(service->work_queue);

	kfree(service->sysfs_name);
	kfree(service->name);
	kfree(service->protocol);
	kfree(service);
}

static void vs_service_release(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);

	vs_dev_debug(VS_DEBUG_SESSION, vs_service_get_session(service),
			&service->dev, "release\n");

	/*
	 * We need to defer cleanup to avoid a circular dependency between the
	 * core service's state lock (which can be held at this point, on the
	 * client side) and any non-core service's reset work (which we must
	 * cancel here, and which acquires the core service state lock).
	 */
	schedule_work(&service->cleanup_work);
}

static int service_add_idr(struct vs_session_device *session,
		struct vs_service_device *service, vs_service_id_t service_id)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
	int err, base_id, id;

	if (service_id == VS_SERVICE_AUTO_ALLOCATE_ID)
		base_id = 1;
	else
		base_id = service_id;

retry:
	if (!idr_pre_get(&session->service_idr, GFP_KERNEL))
		return -ENOMEM;

	mutex_lock(&session->service_idr_lock);
	err = idr_get_new_above(&session->service_idr, service, base_id, &id);
	if (err == 0) {
		if (service_id != VS_SERVICE_AUTO_ALLOCATE_ID &&
				id != service_id) {
			/* Failed to allocated the requested service id */
			idr_remove(&session->service_idr, id);
			mutex_unlock(&session->service_idr_lock);
			return -EBUSY;
		}
		if (id > VS_MAX_SERVICE_ID) {
			/* We are out of service ids */
			idr_remove(&session->service_idr, id);
			mutex_unlock(&session->service_idr_lock);
			return -ENOSPC;
		}
	}
	mutex_unlock(&session->service_idr_lock);
	if (err == -EAGAIN)
		goto retry;
	if (err < 0)
		return err;
#else
	int start, end, id;

	if (service_id == VS_SERVICE_AUTO_ALLOCATE_ID) {
		start = 1;
		end = VS_MAX_SERVICES;
	} else {
		start = service_id;
		end = service_id + 1;
	}

	mutex_lock(&session->service_idr_lock);
	id = idr_alloc(&session->service_idr, service, start, end,
			GFP_KERNEL);
	mutex_unlock(&session->service_idr_lock);

	if (id == -ENOSPC)
		return -EBUSY;
	else if (id < 0)
		return id;
#endif

	service->id = id;
	return 0;
}

static int
vs_service_create_sysfs_entries(struct vs_session_device *session,
		struct vs_service_device *service, vs_service_id_t id)
{
	int ret;
	char *sysfs_name, *c;

	/* Add a symlink to session device inside service device sysfs */
	ret = sysfs_create_link(&service->dev.kobj, &session->dev.kobj,
			VS_SESSION_SYMLINK_NAME);
	if (ret) {
		dev_err(&service->dev, "Error %d creating session symlink\n",
				ret);
		goto fail;
	}

	/* Get the length of the string for sysfs dir */
	sysfs_name = kasprintf(GFP_KERNEL, "%s:%d", service->name, id);
	if (!sysfs_name) {
		ret = -ENOMEM;
		goto fail_session_link;
	}

	/*
	 * We dont want to create symlinks with /'s which could get interpreted
	 * as another directory so replace all /'s with !'s
	 */
	while ((c = strchr(sysfs_name, '/')))
		*c = '!';
	ret = sysfs_create_link(session->sysfs_entry, &service->dev.kobj,
			sysfs_name);
	if (ret)
		goto fail_free_sysfs_name;

	service->sysfs_name = sysfs_name;

	return 0;

fail_free_sysfs_name:
	kfree(sysfs_name);
fail_session_link:
	sysfs_remove_link(&service->dev.kobj, VS_SESSION_SYMLINK_NAME);
fail:
	return ret;
}

/**
 * vs_service_register - create and register a new vs_service_device
 * @session: the session device that is the parent of the service
 * @owner: the service responsible for managing the new service
 * @service_id: the ID of the new service
 * @name: the name of the new service
 * @protocol: the protocol for the new service
 * @plat_data: value to be assigned to (struct device *)->platform_data
 *
 * This function should only be called by a session driver that is bound to
 * the given session.
 *
 * The given service_id must not have been passed to a prior successful
 * vs_service_register call, unless the service ID has since been freed by a
 * call to the session driver's service_removed callback.
 *
 * The core service state lock must not be held while calling this function.
 */
struct vs_service_device *vs_service_register(struct vs_session_device *session,
		struct vs_service_device *owner, vs_service_id_t service_id,
		const char *protocol, const char *name, const void *plat_data)
{
	struct vs_service_device *service;
	struct vs_session_driver *session_drv;
	int ret = -EIO;
	char *c;

	if (service_id && !owner) {
		dev_err(&session->dev, "Non-core service must have an owner\n");
		ret = -EINVAL;
		goto fail;
	} else if (!service_id && owner) {
		dev_err(&session->dev, "Core service must not have an owner\n");
		ret = -EINVAL;
		goto fail;
	}

	if (!session->dev.driver)
		goto fail;

	session_drv = to_vs_session_driver(session->dev.driver);

	service = kzalloc(sizeof(*service), GFP_KERNEL);
	if (!service) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&service->rx_queue);
	INIT_WORK(&service->rx_work, service_rx_work);
	INIT_WORK(&service->reset_work, service_reset_work);
	INIT_DELAYED_WORK(&service->ready_work, service_ready_work);
	INIT_DELAYED_WORK(&service->cooloff_work, service_cooloff_work);
	INIT_WORK(&service->cleanup_work, service_cleanup_work);
	spin_lock_init(&service->rx_lock);
	init_waitqueue_head(&service->quota_wq);

	service->owner = vs_get_service(owner);

	service->readiness = VS_SERVICE_INIT;
	mutex_init(&service->ready_lock);
	service->driver_probed = false;

	/*
	 * Service state locks - A service is only allowed to use one of these
	 */
	spin_lock_init(&service->state_spinlock);
	mutex_init(&service->state_mutex);
#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	service->state_spinlock_used = false;
	service->state_mutex_used = false;
#endif

	/* Lock ordering
	 *
	 * The dependency order for the various service locks is as follows:
	 *
	 * cooloff_work
	 * reset_work
	 * ready_work
	 * ready_lock/0
	 * rx_work/0
	 * state_mutex/0
	 * ready_lock/1
	 * ...
	 * state_mutex/n
	 * state_spinlock
	 *
	 * The subclass is the service's rank in the hierarchy of
	 * service ownership. This results in core having subclass 0 on
	 * server-side and 1 on client-side. Services directly created
	 * by the core will have a lock subclass value of 2 for
	 * servers, 3 for clients. Services created by non-core
	 * services will have a lock subclass value of x + 1, where x
	 * is the lock subclass of the creator service. (e.g servers
	 * will have even numbered lock subclasses, clients will have
	 * odd numbered lock subclasses).
	 *
	 * If a service driver has any additional locks for protecting
	 * internal state, they will generally fit between state_mutex/n and
	 * ready_lock/n+1 on this list. For the core service, this applies to
	 * the session lock.
	 */

	if (owner)
		service->lock_subclass = owner->lock_subclass + 2;
	else
		service->lock_subclass = session->is_server ? 0 : 1;

#ifdef CONFIG_LOCKDEP
	if (service->lock_subclass >= MAX_LOCKDEP_SUBCLASSES) {
		dev_warn(&session->dev, "Owner hierarchy is too deep, lockdep will fail\n");
	} else {
		/*
		 * We need to set the default subclass for the rx work,
		 * because the workqueue API doesn't (and can't) provide
		 * anything like lock_nested() for it.
		 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
		/*
		 * Lockdep allows a specific lock's subclass to be set with
		 * the subclass argument to lockdep_init_map(). However, prior
		 * to Linux 3.3, that only works the first time it is called
		 * for a given class and subclass. So we have to fake it,
		 * putting every subclass in a different class, so the only
		 * thing that breaks is printing the subclass in lockdep
		 * warnings.
		 */
		static struct lock_class_key
				rx_work_keys[MAX_LOCKDEP_SUBCLASSES];
		struct lock_class_key *key =
				&rx_work_keys[service->lock_subclass];
#else
		struct lock_class_key *key = service->rx_work.lockdep_map.key;
#endif

		/*
		 * We can't use the lockdep_set_class() macro because the
		 * work's lockdep map is called .lockdep_map instead of
		 * .dep_map.
		 */
		lockdep_init_map(&service->rx_work.lockdep_map,
				"&service->rx_work", key,
				service->lock_subclass);
	}
#endif

	/*
	 * Copy the protocol and name. Remove any leading or trailing
	 * whitespace characters (including newlines) since the strings
	 * may have been passed via sysfs files.
	 */
	if (protocol) {
		service->protocol = kstrdup(protocol, GFP_KERNEL);
		if (!service->protocol) {
			ret = -ENOMEM;
			goto fail_copy_protocol;
		}
		c = strim(service->protocol);
		if (c != service->protocol)
			memmove(service->protocol, c,
					strlen(service->protocol) + 1);
	}

	service->name = kstrdup(name, GFP_KERNEL);
	if (!service->name) {
		ret = -ENOMEM;
		goto fail_copy_name;
	}
	c = strim(service->name);
	if (c != service->name)
		memmove(service->name, c, strlen(service->name) + 1);

	service->is_server = session_drv->is_server;

	/* Grab a reference to the session we are on */
	service->dev.parent = get_device(&session->dev);
	service->dev.bus = session_drv->service_bus;
	service->dev.release = vs_service_release;

	service->last_reset = 0;
	service->last_reset_request = 0;
	service->last_ready = 0;
	service->reset_delay = 0;

	device_initialize(&service->dev);
	service->dev.platform_data = (void *)plat_data;

	ret = service_add_idr(session, service, service_id);
	if (ret)
		goto fail_add_idr;

#ifdef CONFIG_VSERVICES_NAMED_DEVICE
	/* Integrate session and service names in vservice devnodes */
	dev_set_name(&service->dev, "vservice-%s:%s:%s:%d:%d",
			session->is_server ? "server" : "client",
			session->name, service->name,
			session->session_num, service->id);
#else
	dev_set_name(&service->dev, "%s:%d", dev_name(&session->dev),
			service->id);
#endif

#ifdef CONFIG_VSERVICES_CHAR_DEV
	if (service->id > 0)
		service->dev.devt = MKDEV(vservices_cdev_major,
			(session->session_num * VS_MAX_SERVICES) +
			service->id);
#endif

	service->work_queue = vs_create_workqueue(dev_name(&service->dev));
	if (!service->work_queue) {
		ret = -ENOMEM;
		goto fail_create_workqueue;
	}

	tasklet_init(&service->rx_tasklet, service_rx_tasklet,
			(unsigned long)service);

	/*
	 * If this is the core service, set the core service pointer in the
	 * session.
	 */
	if (service->id == 0) {
		mutex_lock(&session->service_idr_lock);
		if (session->core_service) {
			ret = -EEXIST;
			mutex_unlock(&session->service_idr_lock);
			goto fail_become_core;
		}

		/* Put in vs_session_bus_remove() */
		session->core_service = vs_get_service(service);
		mutex_unlock(&session->service_idr_lock);
	}

	/* Notify the transport */
	ret = session->transport->vt->service_add(session->transport, service);
	if (ret) {
		dev_err(&session->dev,
				"Failed to add service %d (%s:%s) to transport: %d\n",
				service->id, service->name,
				service->protocol, ret);
		goto fail_transport_add;
	}

	/* Delay uevent until vs_service_start(). */
	dev_set_uevent_suppress(&service->dev, true);

	ret = device_add(&service->dev);
	if (ret)
		goto fail_device_add;

	/* Create the service statistics sysfs group */
	ret = sysfs_create_group(&service->dev.kobj, &service_stat_attributes);
	if (ret)
		goto fail_sysfs_create_group;

	/* Create additional sysfs files */
	ret = vs_service_create_sysfs_entries(session, service, service->id);
	if (ret)
		goto fail_sysfs_add_entries;

	return service;

fail_sysfs_add_entries:
	sysfs_remove_group(&service->dev.kobj, &service_stat_attributes);
fail_sysfs_create_group:
	device_del(&service->dev);
fail_device_add:
	session->transport->vt->service_remove(session->transport, service);
fail_transport_add:
	if (service->id == 0) {
		session->core_service = NULL;
		vs_put_service(service);
	}
fail_become_core:
fail_create_workqueue:
	vs_session_release_service_id(service);
fail_add_idr:
	/*
	 * device_initialize() has been called, so we must call put_device()
	 * and let vs_service_release() handle the rest of the cleanup.
	 */
	put_device(&service->dev);
	return ERR_PTR(ret);

fail_copy_name:
	if (service->protocol)
		kfree(service->protocol);
fail_copy_protocol:
	kfree(service);
fail:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(vs_service_register);

/**
 * vs_session_get_service - Look up a service by ID on a session and get
 * a reference to it. The caller must call vs_put_service when it is finished
 * with the service.
 *
 * @session: The session to search for the service on
 * @service_id: ID of the service to find
 */
struct vs_service_device *
vs_session_get_service(struct vs_session_device *session,
		vs_service_id_t service_id)
{
	struct vs_service_device *service;

	if (!session)
		return NULL;

	rcu_read_lock();
	service = idr_find(&session->service_idr, service_id);
	if (!service) {
		rcu_read_unlock();
		return NULL;
	}
	vs_get_service(service);
	rcu_read_unlock();

	return service;
}
EXPORT_SYMBOL_GPL(vs_session_get_service);

/**
 * __for_each_service - Iterate over all non-core services on a session.
 *
 * @session: Session to iterate services on
 * @func: Callback function for each iterated service
 *
 * Iterate over all services on a session, excluding the core service, and
 * call a callback function on each.
 */
static void __for_each_service(struct vs_session_device *session,
		void (*func)(struct vs_service_device *))
{
	struct vs_service_device *service;
	int id;

	for (id = 1; ; id++) {
		rcu_read_lock();
		service = idr_get_next(&session->service_idr, &id);
		if (!service) {
			rcu_read_unlock();
			break;
		}
		vs_get_service(service);
		rcu_read_unlock();

		func(service);
		vs_put_service(service);
	}
}

/**
 * vs_session_delete_noncore - immediately delete all non-core services
 * @session: the session whose services are to be deleted
 *
 * This function disables and deletes all non-core services without notifying
 * the core service. It must only be called by the core service, with its state
 * lock held. It is used when the core service client disconnects or
 * resets, and when the core service server has its driver removed.
 */
void vs_session_delete_noncore(struct vs_session_device *session)
{
	struct vs_service_device *core_service __maybe_unused =
			session->core_service;

	lockdep_assert_held(&core_service->state_mutex);

	vs_session_disable_noncore(session);

	__for_each_service(session, delete_service);
}
EXPORT_SYMBOL_GPL(vs_session_delete_noncore);

/**
 * vs_session_for_each_service - Iterate over all initialised and non-deleted
 * non-core services on a session.
 *
 * @session: Session to iterate services on
 * @func: Callback function for each iterated service
 * @data: Extra data to pass to the callback
 *
 * Iterate over all services on a session, excluding the core service and any
 * service that has been deleted or has not yet had vs_service_start() called,
 * and call a callback function on each. The callback function is called with
 * the service's ready lock held.
 */
void vs_session_for_each_service(struct vs_session_device *session,
		void (*func)(struct vs_service_device *, void *), void *data)
{
	struct vs_service_device *service;
	int id;

	for (id = 1; ; id++) {
		rcu_read_lock();
		service = idr_get_next(&session->service_idr, &id);
		if (!service) {
			rcu_read_unlock();
			break;
		}
		vs_get_service(service);
		rcu_read_unlock();

		mutex_lock_nested(&service->ready_lock, service->lock_subclass);

		if (service->readiness != VS_SERVICE_LOCAL_DELETE &&
				service->readiness != VS_SERVICE_DELETED &&
				service->readiness != VS_SERVICE_INIT)
			func(service, data);

		mutex_unlock(&service->ready_lock);
		vs_put_service(service);
	}
}

static void force_disable_service(struct vs_service_device *service,
		void *unused)
{
	lockdep_assert_held(&service->ready_lock);

	if (service->readiness == VS_SERVICE_ACTIVE)
		__reset_service(service, false);

	disable_service(service, true);
}

/**
 * vs_session_disable_noncore - immediately disable all non-core services
 * @session: the session whose services are to be disabled
 *
 * This function must be called by the core service driver to disable all
 * services, whenever it resets or is otherwise disconnected. It is called
 * directly by the server-side core service, and by the client-side core
 * service via vs_session_delete_noncore().
 */
void vs_session_disable_noncore(struct vs_session_device *session)
{
	vs_session_for_each_service(session, force_disable_service, NULL);
}
EXPORT_SYMBOL_GPL(vs_session_disable_noncore);

static void try_enable_service(struct vs_service_device *service, void *unused)
{
	lockdep_assert_held(&service->ready_lock);

	__enable_service(service);
}

/**
 * vs_session_enable_noncore - enable all disabled non-core services
 * @session: the session whose services are to be enabled
 *
 * This function is called by the core server driver to enable all services
 * when the core client connects.
 */
void vs_session_enable_noncore(struct vs_session_device *session)
{
	vs_session_for_each_service(session, try_enable_service, NULL);
}
EXPORT_SYMBOL_GPL(vs_session_enable_noncore);

/**
 * vs_session_handle_message - process an incoming message from a transport
 * @session: the session that is receiving the message
 * @mbuf: a buffer containing the message payload
 * @service_id: the id of the service that the message was addressed to
 *
 * This routine will return 0 if the buffer was accepted, or a negative value
 * otherwise. In the latter case the caller should free the buffer. If the
 * error is fatal, this routine will reset the service.
 *
 * This routine may be called from interrupt context.
 *
 * The caller must always serialise calls to this function relative to
 * vs_session_handle_reset and vs_session_handle_activate. We don't do this
 * internally, to avoid having to disable interrupts when called from task
 * context.
 */
int vs_session_handle_message(struct vs_session_device *session,
		struct vs_mbuf *mbuf, vs_service_id_t service_id)
{
	struct vs_service_device *service;
	struct vs_transport *transport;
	unsigned long flags;

	transport = session->transport;

	service = vs_session_get_service(session, service_id);
	if (!service) {
		dev_err(&session->dev, "message for unknown service %d\n",
				service_id);
		session_fatal_error(session, GFP_ATOMIC);
		return -ENOTCONN;
	}

	/*
	 * Take the rx lock before checking service readiness. This guarantees
	 * that if __reset_service() has just made the service inactive, we
	 * either see it and don't enqueue the message, or else enqueue the
	 * message before cancel_pending_rx() runs (and removes it).
	 */
	spin_lock_irqsave(&service->rx_lock, flags);

	/* If the service is not active, drop the message. */
	if (service->readiness != VS_SERVICE_ACTIVE) {
		spin_unlock_irqrestore(&service->rx_lock, flags);
		vs_put_service(service);
		return -ECONNRESET;
	}

	list_add_tail(&mbuf->queue, &service->rx_queue);
	spin_unlock_irqrestore(&service->rx_lock, flags);

	/* Schedule processing of the message by the service's drivers. */
	queue_rx_work(service);
	vs_put_service(service);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_session_handle_message);

/**
 * vs_session_quota_available - notify a service that it can transmit
 * @session: the session owning the service that is ready
 * @service_id: the id of the service that is ready
 * @count: the number of buffers that just became ready
 * @call_tx_ready: true if quota has just become nonzero due to a buffer being
 *                 freed by the remote communication partner
 *
 * This routine is called by the transport driver when a send-direction
 * message buffer becomes free. It wakes up any task that is waiting for
 * send quota to become available.
 *
 * This routine may be called from interrupt context from the transport
 * driver, and as such, it may not sleep.
 *
 * The caller must always serialise calls to this function relative to
 * vs_session_handle_reset and vs_session_handle_activate. We don't do this
 * internally, to avoid having to disable interrupts when called from task
 * context.
 *
 * If the call_tx_ready argument is true, this function also schedules a
 * call to the driver's tx_ready callback. Note that this never has priority
 * over handling incoming messages; it will only be handled once the receive
 * queue is empty. This is to increase batching of outgoing messages, and also
 * to reduce the chance that an outgoing message will be dropped by the partner
 * because an incoming message has already changed the state.
 *
 * In general, task context drivers should use the waitqueue, and softirq
 * context drivers (with tx_atomic set) should use tx_ready.
 */
void vs_session_quota_available(struct vs_session_device *session,
		vs_service_id_t service_id, unsigned count,
		bool send_tx_ready)
{
	struct vs_service_device *service;
	unsigned long flags;

	service = vs_session_get_service(session, service_id);
	if (!service) {
		dev_err(&session->dev, "tx ready for unknown service %d\n",
				service_id);
		session_fatal_error(session, GFP_ATOMIC);
		return;
	}

	wake_up_nr(&service->quota_wq, count);

	if (send_tx_ready) {
		/*
		 * Take the rx lock before checking service readiness. This
		 * guarantees that if __reset_service() has just made the
		 * service inactive, we either see it and don't set the tx_ready
		 * flag, or else set the flag before cancel_pending_rx() runs
		 * (and clears it).
		 */
		spin_lock_irqsave(&service->rx_lock, flags);

		/* If the service is not active, drop the tx_ready event */
		if (service->readiness != VS_SERVICE_ACTIVE) {
			spin_unlock_irqrestore(&service->rx_lock, flags);
			vs_put_service(service);
			return;
		}

		service->tx_ready = true;
		spin_unlock_irqrestore(&service->rx_lock, flags);

		/* Schedule RX processing by the service driver. */
		queue_rx_work(service);
	}

	vs_put_service(service);
}
EXPORT_SYMBOL_GPL(vs_session_quota_available);

/**
 * vs_session_handle_notify - process an incoming notification from a transport
 * @session: the session that is receiving the notification
 * @flags: notification flags
 * @service_id: the id of the service that the notification was addressed to
 *
 * This function may be called from interrupt context from the transport driver,
 * and as such, it may not sleep.
 */
void vs_session_handle_notify(struct vs_session_device *session,
		unsigned long bits, vs_service_id_t service_id)
{
	struct vs_service_device *service;
	struct vs_service_driver *driver;
	unsigned long flags;

	service = vs_session_get_service(session, service_id);
	if (!service) {
		/* Ignore the notification since the service id doesn't exist */
		dev_err(&session->dev, "notification for unknown service %d\n",
				service_id);
		return;
	}

	/*
	 * Take the rx lock before checking service readiness. This guarantees
	 * that if __reset_service() has just made the service inactive, we
	 * either see it and don't send the notification, or else send it
	 * before cancel_pending_rx() runs (and thus before the driver is
	 * deactivated).
	 */
	spin_lock_irqsave(&service->rx_lock, flags);

	/* If the service is not active, drop the notification. */
	if (service->readiness != VS_SERVICE_ACTIVE) {
		spin_unlock_irqrestore(&service->rx_lock, flags);
		vs_put_service(service);
		return;
	}

	/* There should be a driver bound on the service */
	if (WARN_ON(!service->dev.driver)) {
		spin_unlock_irqrestore(&service->rx_lock, flags);
		vs_put_service(service);
		return;
	}

	driver = to_vs_service_driver(service->dev.driver);
	/* Call the driver's notify function */
	driver->notify(service, bits);

	spin_unlock_irqrestore(&service->rx_lock, flags);
	vs_put_service(service);
}
EXPORT_SYMBOL_GPL(vs_session_handle_notify);

static unsigned long reset_cool_off(struct vs_service_device *service)
{
	return service->reset_delay * RESET_THROTTLE_COOL_OFF_MULT;
}

static bool ready_needs_delay(struct vs_service_device *service)
{
	/*
	 * We throttle resets if too little time elapsed between the service
	 * last becoming ready, and the service last starting a reset.
	 *
	 * We do not use the current time here because it includes the time
	 * taken by the local service driver to actually process the reset.
	 */
	return service->last_reset && service->last_ready && time_before(
			service->last_reset,
			service->last_ready + RESET_THROTTLE_TIME);
}

static bool reset_throttle_cooled_off(struct vs_service_device *service)
{
	/*
	 * Reset throttling cools off if enough time has elapsed since the
	 * last reset request.
	 *
	 * We check against the last requested reset, not the last serviced
	 * reset or ready. If we are throttling, a reset may not have been
	 * serviced for some time even though we are still receiving requests.
	 */
	return service->reset_delay && service->last_reset_request &&
			time_after(jiffies, service->last_reset_request +
					reset_cool_off(service));
}

/*
 * Queue up the ready work for a service. If a service is resetting too fast
 * then it will be throttled using an exponentially increasing delay before
 * marking it ready. If the reset speed backs off then the ready throttling
 * will be cleared. If a service reaches the maximum throttling delay then all
 * resets will be ignored until the cool off period has elapsed.
 *
 * The basic logic of the reset throttling is:
 *
 *  - If a reset request is processed and the last ready was less than
 *    RESET_THROTTLE_TIME ago, then the ready needs to be delayed to
 *    throttle resets.
 *
 *  - The ready delay increases exponentially on each throttled reset
 *    between RESET_THROTTLE_MIN and RESET_THROTTLE_MAX.
 *
 *  - If RESET_THROTTLE_MAX is reached then no ready will be sent until the
 *    reset requests have cooled off.
 *
 *  - Reset requests have cooled off when no reset requests have been
 *    received for RESET_THROTTLE_COOL_OFF_MULT * the service's current
 *    ready delay. The service's reset throttling is disabled.
 *
 * Note: Be careful when adding print statements, including debugging, to
 * this function. The ready throttling is intended to prevent DOSing of the
 * vServices due to repeated resets (e.g. because of a persistent failure).
 * Adding a printk on each reset for example would reset in syslog spamming
 * which is a DOS attack in itself.
 *
 * The ready lock must be held by the caller.
 */
static void queue_ready_work(struct vs_service_device *service)
{
	struct vs_session_device *session = vs_service_get_session(service);
	unsigned long delay;
	bool wait_for_cooloff = false;

	lockdep_assert_held(&service->ready_lock);

	/* This should only be called when the service enters reset. */
	WARN_ON(service->readiness != VS_SERVICE_RESET);

	if (ready_needs_delay(service)) {
		/* Reset delay increments exponentially */
		if (!service->reset_delay) {
			service->reset_delay = RESET_THROTTLE_MIN;
		} else if (service->reset_delay < RESET_THROTTLE_MAX) {
			service->reset_delay *= 2;
		} else {
			wait_for_cooloff = true;
		}

		delay = service->reset_delay;
	} else {
		/* The reset request appears to have been be sane. */
		delay = 0;

	}

	if (service->reset_delay > 0) {
		/*
		 * Schedule cooloff work, to set the reset_delay to 0 if
		 * the reset requests stop for long enough.
		 */
		schedule_delayed_work(&service->cooloff_work,
				reset_cool_off(service));
	}

	if (wait_for_cooloff) {
		/*
		 * We need to finish cooling off before we service resets
		 * again. Schedule cooloff_work to run after the current
		 * cooloff period ends; it may reschedule itself even later
		 * if any more requests arrive.
		 */
		dev_err(&session->dev,
				"Service %s is resetting too fast - must cool off for %u ms\n",
				dev_name(&service->dev),
				jiffies_to_msecs(reset_cool_off(service)));
		return;
	}

	if (delay)
		dev_err(&session->dev,
				"Service %s is resetting too fast - delaying ready by %u ms\n",
				dev_name(&service->dev),
				jiffies_to_msecs(delay));

	vs_debug(VS_DEBUG_SESSION, session,
			"Service %s will become ready in %u ms\n",
			dev_name(&service->dev),
			jiffies_to_msecs(delay));

	if (service->last_ready)
		vs_debug(VS_DEBUG_SESSION, session,
				"Last became ready %u ms ago\n",
				msecs_ago(service->last_ready));
	if (service->reset_delay >= RESET_THROTTLE_MAX)
		dev_err(&session->dev, "Service %s hit max reset throttle\n",
				dev_name(&service->dev));

	schedule_delayed_work(&service->ready_work, delay);
}

static void session_activation_work(struct work_struct *work)
{
	struct vs_session_device *session = container_of(work,
			struct vs_session_device, activation_work);
	struct vs_service_device *core_service = session->core_service;
	struct vs_session_driver *session_drv =
			to_vs_session_driver(session->dev.driver);
	int activation_state;
	int ret;

	if (WARN_ON(!core_service))
		return;

	if (WARN_ON(!session_drv))
		return;

	/*
	 * We use an atomic to prevent duplicate activations if we race with
	 * an activate after a reset. This is very unlikely, but possible if
	 * this work item is preempted.
	 */
	activation_state = atomic_cmpxchg(&session->activation_state,
			VS_SESSION_ACTIVATE, VS_SESSION_ACTIVE);

	switch (activation_state) {
	case VS_SESSION_ACTIVATE:
		vs_debug(VS_DEBUG_SESSION, session,
				"core service will be activated\n");
		vs_service_enable(core_service);
		break;

	case VS_SESSION_RESET:
		vs_debug(VS_DEBUG_SESSION, session,
				"core service will be deactivated\n");

		/* Handle the core service reset */
		ret = service_handle_reset(session, core_service, true);

		/* Tell the transport if the reset succeeded */
		if (ret >= 0)
			session->transport->vt->ready(session->transport);
		else
			dev_err(&session->dev, "core service reset unhandled: %d\n",
					ret);

		break;

	default:
		vs_debug(VS_DEBUG_SESSION, session,
				"core service already active\n");
		break;
	}
}

/**
 * vs_session_handle_reset - Handle a reset at the session layer.
 * @session: Session to reset
 *
 * This function is called by the transport when it receives a transport-level
 * reset notification.
 *
 * After a session is reset by calling this function, it will reset all of its
 * attached services, and then call the transport's ready callback. The
 * services will remain in reset until the session is re-activated by a call
 * to vs_session_handle_activate().
 *
 * Calling this function on a session that is already reset is permitted, as
 * long as the transport accepts the consequent duplicate ready callbacks.
 *
 * A newly created session is initially in the reset state, and will not call
 * the transport's ready callback. The transport may choose to either act as
 * if the ready callback had been called, or call this function again to
 * trigger a new ready callback.
 */
void vs_session_handle_reset(struct vs_session_device *session)
{
	atomic_set(&session->activation_state, VS_SESSION_RESET);

	schedule_work(&session->activation_work);
}
EXPORT_SYMBOL_GPL(vs_session_handle_reset);

/**
 * vs_session_handle_activate - Allow a session to leave the reset state.
 * @session: Session to mark active.
 *
 * This function is called by the transport when a transport-level reset is
 * completed; that is, after the session layer has reset its services and
 * called the ready callback, at *both* ends of the connection.
 */
void vs_session_handle_activate(struct vs_session_device *session)
{
	atomic_set(&session->activation_state, VS_SESSION_ACTIVATE);

	schedule_work(&session->activation_work);
}
EXPORT_SYMBOL_GPL(vs_session_handle_activate);

static ssize_t id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", session->session_num);
}

/*
 * The vServices session device type
 */
static ssize_t is_server_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", session->is_server);
}

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", session->name);
}

#ifdef CONFIG_VSERVICES_DEBUG
static ssize_t debug_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%.8lx\n", session->debug_mask);
}

static ssize_t debug_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vs_session_device *session = to_vs_session_device(dev);
	int err;

	err = kstrtoul(buf, 0, &session->debug_mask);
	if (err)
		return err;

	/* Clear any bits we don't know about */
	session->debug_mask &= VS_DEBUG_ALL;

	return count;
}
#endif /* CONFIG_VSERVICES_DEBUG */

static struct device_attribute vservices_session_dev_attrs[] = {
	__ATTR_RO(id),
	__ATTR_RO(is_server),
	__ATTR_RO(name),
#ifdef CONFIG_VSERVICES_DEBUG
	__ATTR(debug_mask, S_IRUGO | S_IWUSR,
			debug_mask_show, debug_mask_store),
#endif
	__ATTR_NULL,
};

static int vs_session_free_idr(struct vs_session_device *session)
{
	mutex_lock(&vs_session_lock);
	idr_remove(&session_idr, session->session_num);
	mutex_unlock(&vs_session_lock);
	return 0;
}

static void vs_session_device_release(struct device *dev)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	vs_session_free_idr(session);

	kfree(session->name);
	kfree(session);
}

/*
 * The vServices session bus
 */
static int vs_session_bus_match(struct device *dev,
		struct device_driver *driver)
{
	struct vs_session_device *session = to_vs_session_device(dev);
	struct vs_session_driver *session_drv = to_vs_session_driver(driver);

	return (session->is_server == session_drv->is_server);
}

static int vs_session_bus_remove(struct device *dev)
{
	struct vs_session_device *session = to_vs_session_device(dev);
	struct vs_service_device *core_service = session->core_service;

	if (!core_service)
		return 0;

	/*
	 * Abort any pending session activation. We rely on the transport to
	 * not call vs_session_handle_activate after this point.
	 */
	cancel_work_sync(&session->activation_work);

	/* Abort any pending fatal error handling, which is redundant now. */
	cancel_work_sync(&session->fatal_error_work);

	/*
	 * Delete the core service. This will implicitly delete everything
	 * else (in reset on the client side, and in release on the server
	 * side). The session holds a reference, so this won't release the
	 * service struct.
	 */
	delete_service(core_service);

	/* Now clean up the core service. */
	session->core_service = NULL;

	/* Matches the get in vs_service_register() */
	vs_put_service(core_service);

	return 0;
}

static int vservices_session_uevent(struct device *dev,
		struct kobj_uevent_env *env)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	dev_dbg(dev, "uevent\n");

	if (add_uevent_var(env, "IS_SERVER=%d", session->is_server))
		return -ENOMEM;

	if (add_uevent_var(env, "SESSION_ID=%d", session->session_num))
		return -ENOMEM;

	return 0;
}

static void vservices_session_shutdown(struct device *dev)
{
	struct vs_session_device *session = to_vs_session_device(dev);

	dev_dbg(dev, "shutdown\n");

	/* Do a transport reset */
	session->transport->vt->reset(session->transport);
}

struct bus_type vs_session_bus_type = {
	.name		= "vservices-session",
	.match		= vs_session_bus_match,
	.remove		= vs_session_bus_remove,
	.dev_attrs	= vservices_session_dev_attrs,
	.uevent		= vservices_session_uevent,
	.shutdown	= vservices_session_shutdown,
};
EXPORT_SYMBOL_GPL(vs_session_bus_type);

/*
 * Common code for the vServices client and server buses
 */
int vs_service_bus_probe(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_service_driver *vsdrv = to_vs_service_driver(dev->driver);
	struct vs_session_device *session = vs_service_get_session(service);
	int ret;

	vs_dev_debug(VS_DEBUG_SESSION, session, &service->dev, "probe\n");

	/*
	 * Increase the reference count on the service driver. We don't allow
	 * service driver modules to be removed if there are any device
	 * instances present. The devices must be explicitly removed first.
	 */
	if (!try_module_get(vsdrv->driver.owner))
		return -ENODEV;

	ret = vsdrv->probe(service);
	if (ret) {
		module_put(vsdrv->driver.owner);
		return ret;
	}

	service->driver_probed = true;

	try_start_service(service);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_service_bus_probe);

int vs_service_bus_remove(struct device *dev)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_service_driver *vsdrv = to_vs_service_driver(dev->driver);
	int err = 0;

	reset_service(service);

	/* Prevent reactivation of the driver */
	service->driver_probed = false;

	/* The driver has now had its reset() callback called; remove it */
	vsdrv->remove(service);

	/*
	 * Take the service's state mutex and spinlock. This ensures that any
	 * thread that is calling vs_state_lock_safe[_bh] will either complete
	 * now, or see the driver removal and fail, irrespective of which type
	 * of lock it is using.
	 */
	mutex_lock_nested(&service->state_mutex, service->lock_subclass);
	spin_lock_bh(&service->state_spinlock);

	/* Release all the locks. */
	spin_unlock_bh(&service->state_spinlock);
	mutex_unlock(&service->state_mutex);
	mutex_unlock(&service->ready_lock);

#ifdef CONFIG_VSERVICES_LOCK_DEBUG
	service->state_spinlock_used = false;
	service->state_mutex_used = false;
#endif

	module_put(vsdrv->driver.owner);

	return err;
}
EXPORT_SYMBOL_GPL(vs_service_bus_remove);

int vs_service_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct vs_service_device *service = to_vs_service_device(dev);
	struct vs_session_device *session = vs_service_get_session(service);

	dev_dbg(dev, "uevent\n");

	if (add_uevent_var(env, "IS_SERVER=%d", service->is_server))
		return -ENOMEM;

	if (add_uevent_var(env, "SERVICE_ID=%d", service->id))
		return -ENOMEM;

	if (add_uevent_var(env, "SESSION_ID=%d", session->session_num))
		return -ENOMEM;

	if (add_uevent_var(env, "SERVICE_NAME=%s", service->name))
		return -ENOMEM;

	if (add_uevent_var(env, "PROTOCOL=%s", service->protocol ?: ""))
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(vs_service_bus_uevent);

static int vs_session_create_sysfs_entry(struct vs_transport *transport,
		struct vs_session_device *session, bool server,
		const char *transport_name)
{
	char *sysfs_name;
	struct kobject *sysfs_parent = vservices_client_root;

	if (!transport_name)
		return -EINVAL;

	sysfs_name = kasprintf(GFP_KERNEL, "%s:%s", transport->type,
			transport_name);
	if (!sysfs_name)
		return -ENOMEM;

	if (server)
		sysfs_parent = vservices_server_root;

	session->sysfs_entry = kobject_create_and_add(sysfs_name, sysfs_parent);

	kfree(sysfs_name);
	if (!session->sysfs_entry)
		return -ENOMEM;
	return 0;
}

static int vs_session_alloc_idr(struct vs_session_device *session)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
	int err, id;

retry:
	if (!idr_pre_get(&session_idr, GFP_KERNEL))
		return -ENOMEM;

	mutex_lock(&vs_session_lock);
	err = idr_get_new_above(&session_idr, session, 0, &id);
	if (err == 0) {
		if (id >= VS_MAX_SESSIONS) {
			/* We are out of session ids */
			idr_remove(&session_idr, id);
			mutex_unlock(&vs_session_lock);
			return -EBUSY;
		}
	}
	mutex_unlock(&vs_session_lock);
	if (err == -EAGAIN)
		goto retry;
	if (err < 0)
		return err;
#else
	int id;

	mutex_lock(&vs_session_lock);
	id = idr_alloc(&session_idr, session, 0, VS_MAX_SESSIONS, GFP_KERNEL);
	mutex_unlock(&vs_session_lock);

	if (id == -ENOSPC)
		return -EBUSY;
	else if (id < 0)
		return id;
#endif

	session->session_num = id;
	return 0;
}

/**
 * vs_session_register - register a vservices session on a transport
 * @transport: vservices transport that the session will attach to
 * @parent: device that implements the transport (for sysfs)
 * @server: true if the session is server-side
 * @transport_name: name of the transport
 *
 * This function is intended to be called from the probe() function of a
 * transport driver. It sets up a new session device, which then either
 * performs automatic service discovery (for clients) or creates sysfs nodes
 * that allow the user to create services (for servers).
 *
 * Note that the parent is only used by the driver framework; it is not
 * directly accessed by the session drivers. Thus, a single transport device
 * can support multiple sessions, as long as they each have a unique struct
 * vs_transport.
 *
 * Note: This function may sleep, and therefore must not be called from
 * interrupt context.
 *
 * Returns a pointer to the new device, or an error pointer.
 */
struct vs_session_device *vs_session_register(struct vs_transport *transport,
		struct device *parent, bool server, const char *transport_name)
{
	struct device *dev;
	struct vs_session_device *session;
	int ret = -ENOMEM;

	WARN_ON(!transport);

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		goto fail_session_alloc;

	session->transport = transport;
	session->is_server = server;
	session->name = kstrdup(transport_name, GFP_KERNEL);
	if (!session->name)
		goto fail_free_session;

	INIT_WORK(&session->activation_work, session_activation_work);
	INIT_WORK(&session->fatal_error_work, session_fatal_error_work);

#ifdef CONFIG_VSERVICES_DEBUG
	session->debug_mask = default_debug_mask & VS_DEBUG_ALL;
#endif

	idr_init(&session->service_idr);
	mutex_init(&session->service_idr_lock);

	/*
	 * We must create session sysfs entry before device_create
	 * so, that sysfs entry is available while registering
	 * core service.
	 */
	ret = vs_session_create_sysfs_entry(transport, session, server,
			transport_name);
	if (ret)
		goto fail_free_session;

	ret = vs_session_alloc_idr(session);
	if (ret)
		goto fail_sysfs_entry;

	dev = &session->dev;
	dev->parent = parent;
	dev->bus = &vs_session_bus_type;
	dev->release = vs_session_device_release;
	dev_set_name(dev, "vservice:%d", session->session_num);

	ret = device_register(dev);
	if (ret) {
		goto fail_session_map;
	}

	/* Add a symlink to transport device inside session device sysfs dir */
	if (parent) {
		ret = sysfs_create_link(&session->dev.kobj,
				&parent->kobj, VS_TRANSPORT_SYMLINK_NAME);
		if (ret) {
			dev_err(&session->dev,
					"Error %d creating transport symlink\n",
					ret);
			goto fail_session_device_unregister;
		}
	}

	return session;

fail_session_device_unregister:
	device_unregister(&session->dev);
	kobject_put(session->sysfs_entry);
	/* Remaining cleanup will be done in vs_session_release */
	return ERR_PTR(ret);
fail_session_map:
	vs_session_free_idr(session);
fail_sysfs_entry:
	kobject_put(session->sysfs_entry);
fail_free_session:
	kfree(session->name);
	kfree(session);
fail_session_alloc:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(vs_session_register);

void vs_session_start(struct vs_session_device *session)
{
	struct vs_service_device *core_service = session->core_service;

	if (WARN_ON(!core_service))
		return;

	blocking_notifier_call_chain(&vs_session_notifier_list,
			VS_SESSION_NOTIFY_ADD, session);

	vs_service_start(core_service);
}
EXPORT_SYMBOL_GPL(vs_session_start);

/**
 * vs_session_unregister - unregister a session device
 * @session: the session device to unregister
 */
void vs_session_unregister(struct vs_session_device *session)
{
	if (session->dev.parent)
		sysfs_remove_link(&session->dev.kobj, VS_TRANSPORT_SYMLINK_NAME);
	blocking_notifier_call_chain(&vs_session_notifier_list,
			VS_SESSION_NOTIFY_REMOVE, session);

	device_unregister(&session->dev);

	kobject_put(session->sysfs_entry);
}
EXPORT_SYMBOL_GPL(vs_session_unregister);

struct service_unbind_work_struct {
	struct vs_service_device *service;
	struct work_struct work;
};

static void service_unbind_work(struct work_struct *work)
{
	struct service_unbind_work_struct *unbind_work = container_of(work,
			struct service_unbind_work_struct, work);

	device_release_driver(&unbind_work->service->dev);

	/* Matches vs_get_service() in vs_session_unbind_driver() */
	vs_put_service(unbind_work->service);
	kfree(unbind_work);
}

int vs_session_unbind_driver(struct vs_service_device *service)
{
	struct service_unbind_work_struct *unbind_work =
			kmalloc(sizeof(*unbind_work), GFP_KERNEL);

	if (!unbind_work)
		return -ENOMEM;

	INIT_WORK(&unbind_work->work, service_unbind_work);

	/* Put in service_unbind_work() */
	unbind_work->service = vs_get_service(service);
	schedule_work(&unbind_work->work);

	return 0;
}
EXPORT_SYMBOL_GPL(vs_session_unbind_driver);

static int __init vservices_init(void)
{
	int r;

	printk(KERN_INFO "vServices Framework 1.0\n");

	vservices_root = kobject_create_and_add("vservices", NULL);
	if (!vservices_root) {
		r = -ENOMEM;
		goto fail_create_root;
	}

	r = bus_register(&vs_session_bus_type);
	if (r < 0)
		goto fail_bus_register;

	r = vs_devio_init();
	if (r < 0)
		goto fail_devio_init;

	return 0;

fail_devio_init:
	bus_unregister(&vs_session_bus_type);
fail_bus_register:
	kobject_put(vservices_root);
fail_create_root:
	return r;
}

static void __exit vservices_exit(void)
{
	printk(KERN_INFO "vServices Framework exit\n");

	vs_devio_exit();
	bus_unregister(&vs_session_bus_type);
	kobject_put(vservices_root);
}

subsys_initcall(vservices_init);
module_exit(vservices_exit);

MODULE_DESCRIPTION("OKL4 Virtual Services Session");
MODULE_AUTHOR("Open Kernel Labs, Inc");
