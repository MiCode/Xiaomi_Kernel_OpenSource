/*
 * include/vservices/wait.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Generic wait event helpers for Virtual Service drivers.
 */

#ifndef _VSERVICE_SERVICE_WAIT_H
#define _VSERVICE_SERVICE_WAIT_H

#include <linux/sched.h>
#include <linux/wait.h>

#include <vservices/service.h>

/* Older kernels don't have lockdep_assert_held_once(). */
#ifndef lockdep_assert_held_once
#ifdef CONFIG_LOCKDEP
#define lockdep_assert_held_once(l) do {				\
		WARN_ON_ONCE(debug_locks && !lockdep_is_held(l));	\
	} while (0)
#else
#define lockdep_assert_held_once(l) do { } while (0)
#endif
#endif

/* Legacy wait macro; needs rewriting to use vs_state_lock_safe(). */
/* FIXME: Redmine ticket #229 - philip. */
/**
 * __vs_service_wait_event - Wait for a condition to become true for a
 * Virtual Service.
 *
 * @_service: The service to wait for the condition to be true for.
 * @_wq: Waitqueue to wait on.
 * @_condition: Condition to wait for.
 *
 * Returns: This function returns 0 if the condition is true, or a -ERESTARTSYS
 *          if the wait loop wait interrupted. If _state is TASK_UNINTERRUPTIBLE
 *          then this function will always return 0.
 *
 * This function must be called with the service's state lock held. The wait
 * is performed without the state lock held, but the condition is re-checked
 * after reacquiring the state lock. This property allows this function to
 * check the state of the service's protocol in a thread safe manner.
 *
 * The caller is responsible for ensuring that it has not been detached from
 * the given service.
 *
 * It is nearly always wrong to call this on the service workqueue, since
 * the workqueue is single-threaded and the state can only change when a
 * handler function is called on it.
 */
#define __vs_service_wait_event(_service, _wq, _cond, _state)		\
	({								\
		DEFINE_WAIT(__wait);					\
		int __ret = 0;						\
									\
		lockdep_assert_held_once(&(_service)->state_mutex);	\
		do {							\
			prepare_to_wait(&(_wq), &__wait, (_state));	\
									\
			if (_cond)					\
				break;					\
									\
			if ((_state) == TASK_INTERRUPTIBLE &&		\
					signal_pending(current)) {	\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
									\
			vs_service_state_unlock(_service);		\
			schedule();					\
			vs_service_state_lock(_service);		\
		} while (!(_cond));					\
									\
		finish_wait(&(_wq), &__wait);				\
		__ret;							\
	})

/* Legacy wait macros; need rewriting to use __vs_wait_state(). */
/* FIXME: Redmine ticket #229 - philip. */
#define vs_service_wait_event(_service, _wq, _cond) \
	__vs_service_wait_event(_service, _wq, _cond, TASK_INTERRUPTIBLE)
#define vs_service_wait_event_nointr(_service, _wq, _cond) \
	__vs_service_wait_event(_service, _wq, _cond, TASK_UNINTERRUPTIBLE)

/**
 * __vs_wait_state - block until a condition becomes true on a service state.
 *
 * @_state: The protocol state to wait on.
 * @_cond: Condition to wait for.
 * @_intr: If true, perform an interruptible wait; the wait may then fail
 *         with -ERESTARTSYS.
 * @_timeout: A timeout in jiffies, or negative for no timeout. If the
 *         timeout expires, the wait will fail with -ETIMEDOUT.
 * @_bh: The token _bh if this service uses tx_atomic (sends from a
 *         non-framework tasklet); otherwise nothing.
 *
 * Return: Return a pointer to a message buffer on successful allocation,
 *         or an error code in ERR_PTR form.
 *
 * This macro blocks waiting until a particular condition becomes true on a
 * service state. The service must be running; if not, or if it ceases to be
 * running during the wait, -ECANCELED will be returned.
 *
 * This is not an exclusive wait. If an exclusive wait is desired it is
 * usually better to use the waiting alloc or send functions.
 *
 * This macro must be called with a reference to the service held, and with
 * the service's state lock held. The state lock will be dropped by waiting
 * but reacquired before returning, unless -ENOLINK is returned, in which case
 * the service driver has been unbound and the lock cannot be reacquired.
 */
#define __vs_wait_state(_state, _cond, _intr, _timeout, _bh)	\
	({								\
		DEFINE_WAIT(__wait);					\
		int __ret;						\
		int __jiffies __maybe_unused = (_timeout);		\
		struct vs_service_device *__service = (_state)->service;\
									\
		while (1) {						\
			prepare_to_wait(&__service->quota_wq, &__wait,	\
					_intr ? TASK_INTERRUPTIBLE :    \
					TASK_UNINTERRUPTIBLE);		\
									\
			if (!VSERVICE_BASE_STATE_IS_RUNNING(		\
					(_state)->state.base)) {	\
				__ret = -ECANCELED;			\
				break;					\
			}						\
									\
			if (_cond) {					\
				__ret = 0;				\
				break;					\
			}						\
									\
			if (_intr && signal_pending(current)) {		\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
									\
			vs_state_unlock##_bh(_state);			\
									\
			if (_timeout >= 0) {				\
				__jiffies = schedule_timeout(__jiffies);\
				if (!__jiffies) {			\
					__ret = -ETIMEDOUT;		\
					break;				\
				}					\
			} else {					\
				schedule();				\
			}						\
									\
			if (!vs_state_lock_safe##_bh(_state)) {		\
				__ret = -ENOLINK;			\
				break;					\
			}						\
		}							\
									\
		finish_wait(&__service->quota_wq, &__wait);		\
		__ret;							\
	})

/* Specialisations of __vs_wait_state for common uses. */
#define vs_wait_state(_state, _cond) \
	__vs_wait_state(_state, _cond, true, -1,)
#define vs_wait_state_timeout(_state, _cond, _timeout) \
	__vs_wait_state(_state, _cond, true, _timeout,)
#define vs_wait_state_nointr(_state, _cond) \
	__vs_wait_state(_state, _cond, false, -1,)
#define vs_wait_state_nointr_timeout(_state, _cond, _timeout) \
	__vs_wait_state(_state, _cond, false, _timeout,)
#define vs_wait_state_bh(_state, _cond) \
	__vs_wait_state(_state, _cond, true, -1, _bh)
#define vs_wait_state_timeout_bh(_state, _cond, _timeout) \
	__vs_wait_state(_state, _cond, true, _timeout, _bh)
#define vs_wait_state_nointr_bh(_state, _cond) \
	__vs_wait_state(_state, _cond, false, -1, _bh)
#define vs_wait_state_nointr_timeout_bh(_state, _cond, _timeout) \
	__vs_wait_state(_state, _cond, false, _timeout, _bh)

/**
 * __vs_wait_alloc - block until quota is available, then allocate a buffer.
 *
 * @_state: The protocol state to allocate a message for.
 * @_alloc_func: The message buffer allocation function to run. This is the
 *         full function invocation, not a pointer to the function.
 * @_cond: Additional condition which must remain true, or else the wait
 *         will fail with -ECANCELED. This is typically used to check the
 *         service's protocol state. Note that this condition will only
 *         be checked after sleeping; it is assumed to be true when the
 *         macro is first called.
 * @_unlock: If true, drop the service state lock before sleeping. The wait
 *         may then fail with -ENOLINK if the driver is detached from the
 *         service, in which case the lock is dropped.
 * @_intr: If true, perform an interruptible wait; the wait may then fail
 *         with -ERESTARTSYS.
 * @_timeout: A timeout in jiffies, or negative for no timeout. If the
 *         timeout expires, the wait will fail with -ETIMEDOUT.
 * @_bh: The token _bh if this service uses tx_atomic (sends from a
 *         non-framework tasklet); otherwise nothing.
 *
 * Return: Return a pointer to a message buffer on successful allocation,
 *         or an error code in ERR_PTR form.
 *
 * This macro calls a specified message allocation function, and blocks
 * if it returns -ENOBUFS, waiting until quota is available on the service
 * before retrying. It aborts the wait if the service resets, or if the
 * optionally specified condition becomes false. Note that a reset followed
 * quickly by an activate might not trigger a failure; if that is significant
 * for your driver, use the optional condition to detect it.
 *
 * This macro must be called with a reference to the service held, and with
 * the service's state lock held. The reference and state lock will still be
 * held on return, unless -ENOLINK is returned, in which case the lock has been
 * dropped and cannot be reacquired.
 *
 * This is always an exclusive wait. It is safe to call without separately
 * waking the waitqueue afterwards; if the allocator function fails for any
 * reason other than quota exhaustion then another waiter will be woken.
 *
 * Be wary of potential deadlocks when using this macro on the service
 * workqueue. If both ends block their service workqueues waiting for quota,
 * then no progress can be made. It is usually only correct to block the
 * service workqueue on the server side.
 */
#define __vs_wait_alloc(_state, _alloc_func, _cond, _unlock, _intr, 	\
		_timeout, _bh)						\
	({								\
		DEFINE_WAIT(__wait);					\
		struct vs_mbuf *__mbuf = NULL;				\
		int __jiffies __maybe_unused = (_timeout);		\
		struct vs_service_device *__service = (_state)->service;\
									\
		while (!vs_service_send_mbufs_available(__service)) {	\
			if (_intr && signal_pending(current)) {		\
				__mbuf = ERR_PTR(-ERESTARTSYS);		\
				break;					\
			}						\
									\
			prepare_to_wait_exclusive(			\
					&__service->quota_wq, &__wait,	\
					_intr ? TASK_INTERRUPTIBLE :    \
					TASK_UNINTERRUPTIBLE);		\
									\
			if (_unlock)					\
				vs_state_unlock##_bh(_state);		\
									\
			if (_timeout >= 0) {				\
				__jiffies = schedule_timeout(__jiffies);\
				if (!__jiffies) {			\
					__mbuf = ERR_PTR(-ETIMEDOUT);	\
					break;				\
				}					\
			} else {					\
				schedule();				\
			}						\
									\
			if (_unlock && !vs_state_lock_safe##_bh(	\
						_state)) {		\
				__mbuf = ERR_PTR(-ENOLINK);		\
				break;					\
			}						\
									\
			if (!VSERVICE_BASE_STATE_IS_RUNNING(		\
					(_state)->state.base) ||	\
					!(_cond)) {			\
				__mbuf = ERR_PTR(-ECANCELED);		\
				break;					\
			}						\
		}							\
		finish_wait(&__service->quota_wq, &__wait);		\
									\
		if (__mbuf == NULL)					\
			__mbuf = (_alloc_func);				\
		if (IS_ERR(__mbuf) && (PTR_ERR(__mbuf) != -ENOBUFS))	\
			wake_up(&__service->quota_wq);			\
		__mbuf;							\
	})

/* Specialisations of __vs_wait_alloc for common uses. */
#define vs_wait_alloc(_state, _cond, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, true, -1,)
#define vs_wait_alloc_timeout(_state, _cond, _alloc_func, _timeout) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, true, _timeout,)
#define vs_wait_alloc_nointr(_state, _cond, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, false, -1,)
#define vs_wait_alloc_nointr_timeout(_state, _cond, _alloc_func, _timeout) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, false, _timeout,)
#define vs_wait_alloc_bh(_state, _cond, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, true, -1, _bh)
#define vs_wait_alloc_timeout_bh(_state, _cond, _alloc_func, _timeout) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, true, _timeout, _bh)
#define vs_wait_alloc_nointr_bh(_state, _cond, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, false, -1, _bh)
#define vs_wait_alloc_nointr_timeout_bh(_state, _cond, _alloc_func, _timeout) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, false, _timeout, _bh)
#define vs_wait_alloc_locked(_state, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, true, false, true, -1,)

/* Legacy wait macros, to be removed and replaced with those above. */
/* FIXME: Redmine ticket #229 - philip. */
#define vs_service_waiting_alloc(_state, _alloc_func) \
	__vs_wait_alloc(_state, _alloc_func, true, false, true, -1,)
#define vs_service_waiting_alloc_cond_locked(_state, _alloc_func, _cond) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, true, -1,)
#define vs_service_waiting_alloc_cond_locked_nointr(_state, _alloc_func, _cond) \
	__vs_wait_alloc(_state, _alloc_func, _cond, true, false, -1,)

/**
 * __vs_wait_send - block until quota is available, then send a message.
 *
 * @_state: The protocol state to send a message for.
 * @_cond: Additional condition which must remain true, or else the wait
 *         will fail with -ECANCELED. This is typically used to check the
 *         service's protocol state. Note that this condition will only
 *         be checked after sleeping; it is assumed to be true when the
 *         macro is first called.
 * @_send_func: The message send function to run. This is the full function
 *         invocation, not a pointer to the function.
 * @_unlock: If true, drop the service state lock before sleeping. The wait
 *         may then fail with -ENOLINK if the driver is detached from the
 *         service, in which case the lock is dropped.
 * @_check_running: If true, the wait will return -ECANCELED if the service's
 *         base state is not active, or ceases to be active.
 * @_intr: If true, perform an interruptible wait; the wait may then fail
 *         with -ERESTARTSYS.
 * @_timeout: A timeout in jiffies, or negative for no timeout. If the
 *         timeout expires, the wait will fail with -ETIMEDOUT.
 * @_bh: The token _bh if this service uses tx_atomic (sends from a
 *         non-framework tasklet); otherwise nothing.
 *
 * Return: If the send succeeds, then 0 is returned; otherwise an error
 *         code may be returned as described above.
 *
 * This macro calls a specified message send function, and blocks if it
 * returns -ENOBUFS, waiting until quota is available on the service before
 * retrying. It aborts the wait if it finds the service in reset, or if the
 * optionally specified condition becomes false. Note that a reset followed
 * quickly by an activate might not trigger a failure; if that is significant
 * for your driver, use the optional condition to detect it.
 *
 * This macro must be called with a reference to the service held, and with
 * the service's state lock held. The reference and state lock will still be
 * held on return, unless -ENOLINK is returned, in which case the lock has been
 * dropped and cannot be reacquired.
 *
 * This is always an exclusive wait. It is safe to call without separately
 * waking the waitqueue afterwards; if the allocator function fails for any
 * reason other than quota exhaustion then another waiter will be woken.
 *
 * Be wary of potential deadlocks when calling this function on the service
 * workqueue. If both ends block their service workqueues waiting for quota,
 * then no progress can be made. It is usually only correct to block the
 * service workqueue on the server side.
 */
#define __vs_wait_send(_state, _cond, _send_func, _unlock, 		\
		_check_running, _intr, _timeout, _bh)			\
	({								\
		DEFINE_WAIT(__wait);					\
		int __ret = 0;						\
		int __jiffies __maybe_unused = (_timeout);		\
		struct vs_service_device *__service = (_state)->service;\
									\
		while (!vs_service_send_mbufs_available(__service)) {	\
			if (_intr && signal_pending(current)) {		\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
									\
			prepare_to_wait_exclusive(			\
					&__service->quota_wq, &__wait,	\
					_intr ? TASK_INTERRUPTIBLE :    \
					TASK_UNINTERRUPTIBLE);		\
									\
			if (_unlock)					\
				vs_state_unlock##_bh(_state);		\
									\
			if (_timeout >= 0) {				\
				__jiffies = schedule_timeout(__jiffies);\
				if (!__jiffies) {			\
					__ret = -ETIMEDOUT;		\
					break;				\
				}					\
			} else {					\
				schedule();				\
			}						\
									\
			if (_unlock && !vs_state_lock_safe##_bh(	\
						_state)) {		\
				__ret = -ENOLINK;			\
				break;					\
			}						\
									\
			if ((_check_running &&				\
					!VSERVICE_BASE_STATE_IS_RUNNING(\
					(_state)->state.base)) ||	\
					!(_cond)) {			\
				__ret = -ECANCELED;			\
				break;					\
			}						\
		}							\
		finish_wait(&__service->quota_wq, &__wait);		\
									\
		if (!__ret)						\
			__ret = (_send_func);				\
		if ((__ret < 0) && (__ret != -ENOBUFS))			\
			wake_up(&__service->quota_wq);			\
		__ret;							\
	})

/* Specialisations of __vs_wait_send for common uses. */
#define vs_wait_send(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, true, -1,)
#define vs_wait_send_timeout(_state, _cond, _send_func, _timeout) \
	__vs_wait_send(_state, _cond, _send_func, true, true, true, _timeout,)
#define vs_wait_send_nointr(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, false, -1,)
#define vs_wait_send_nointr_timeout(_state, _cond, _send_func, _timeout) \
	__vs_wait_send(_state, _cond, _send_func, true, true, false, _timeout,)
#define vs_wait_send_bh(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, true, -1, _bh)
#define vs_wait_send_timeout_bh(_state, _cond, _send_func, _timeout) \
	__vs_wait_send(_state, _cond, _send_func, true, true, true, \
			_timeout, _bh)
#define vs_wait_send_nointr_bh(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, false, -1, _bh)
#define vs_wait_send_nointr_timeout_bh(_state, _cond, _send_func, _timeout) \
	__vs_wait_send(_state, _cond, _send_func, true, true, false, \
			_timeout, _bh)
#define vs_wait_send_locked(_state, _send_func) \
	__vs_wait_send(_state, true, _send_func, false, true, true, -1,)
#define vs_wait_send_locked_nocheck(_state, _send_func) \
	__vs_wait_send(_state, true, _send_func, false, false, true, -1,)

/* Legacy wait macros, to be removed and replaced with those above. */
/* FIXME: Redmine ticket #229 - philip. */
#define vs_service_waiting_send(_state, _send_func) \
	__vs_wait_send(_state, true, _send_func, true, true, true, -1,)
#define vs_service_waiting_send_nointr(_state, _send_func) \
	__vs_wait_send(_state, true, _send_func, true, true, false, -1,)
#define vs_service_waiting_send_cond(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, true, -1,)
#define vs_service_waiting_send_cond_nointr(_state, _cond, _send_func) \
	__vs_wait_send(_state, _cond, _send_func, true, true, false, -1,)
#define vs_service_waiting_send_nocheck(_state, _send_func) \
	__vs_wait_send(_state, true, _send_func, true, false, true, -1,)

#endif /* _VSERVICE_SERVICE_WAIT_H */
