#ifndef _CAM_ACTUATOR_PARKLENS_THREAD_H_
#define _CAM_ACTUATOR_PARKLENS_THREAD_H_


#include "cam_sensor_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include <linux/module.h>
#include <cam_sensor_cmn_header.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include "cam_debug_util.h"
#include <linux/pm_wakeup.h>
#include <linux/suspend.h>
#include <linux/completion.h>
#include <linux/atomic.h>

#define LINUX_EVENT_COOKIE 0x12341234

#define PARKLENS_SLEEPTIME 10

typedef struct task_struct parklens_thread_t;
typedef wait_queue_head_t parklens_wait_queue_head_t;
typedef atomic_t parklens_atomic_t;

struct parklens_wake_lock {
       struct wakeup_source lock;
       struct wakeup_source *priv;
};

struct parklens_event {
	struct completion complete;
	uint32_t cookie;
	bool done;
	bool force_set;
};

enum parklens_opcodes {
    ENTER_PARKLENS_WITH_POWERDOWN,
    ENTER_PARKLENS_WITHOUT_POWERDOWN,
    EXIT_PARKLENS_WITH_POWERDOWN,
    EXIT_PARKLENS_WITHOUT_POWERDOWN,
};

enum parklens_exit_results {
    PARKLENS_ENTER,
    /* power down case */
    PARKLENS_EXIT_CREATE_WAKELOCK_FAILED,
    PARKLENS_EXIT_WITHOUT_POWERDOWN,
    /* without power down case */
    PARKLENS_EXIT_WITH_POWEDOWN,
    PARKLENS_EXIT_WITHOUT_PARKLENS,
    PARKLENS_EXIT_CCI_ERROR,
    PARKLENS_EXIT_WITH_POWEDOWN_FAILED,
};

enum parklens_states {
    PARKLENS_INVALID,
    PARKLENS_STOP,
    PARKLENS_RUNNING,
};

struct cam_actuator_parklens_ctrl_t {
    parklens_thread_t *parklens_thread;
    parklens_wait_queue_head_t parklens_wait_queue;
    struct parklens_event start_event;
    struct parklens_event shutdown_event;
    parklens_atomic_t parklens_opcode;
    parklens_atomic_t exit_result;
    parklens_atomic_t parklens_state;
};

#define parklens_init_waitqueue_head(_q) init_waitqueue_head(_q)

#define parklens_wake_up_interruptible(_q) wake_up_interruptible(_q)

#define parklens_wait_queue_timeout(wait_queue, condition, timeout) \
			wait_event_timeout(wait_queue, condition, timeout)

#define INIT_COMPLETION(event) reinit_completion(&event)

/**
 * @threadfn:    The function to run in the thread
 * @data:        Data pointer for @threadfn()
 * @thread_name: Thread name of @threadfn()
 *
 *  This API create a kthread
 *
 * @return Pointer to created kernel thread on success else NULL
 */
parklens_thread_t *parklens_thread_run(int32_t (*threadfn)(void *data),
	void *data,
	const char thread_name[]);

/**
 * @status: exit status
 *
 * This API exit thread execution
 *
 * @return: exit status
 */
bool parklens_exit_thread(bool status);

/**
 * @lock: The wake lock to create
 * @name: Name of wake lock
 *
 * This API creates a wake lock
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int32_t parklens_wake_lock_create(struct parklens_wake_lock *lock, const char *name);

/**
 * @lock: The wake lock to acquire
 *
 * This API acquires a wake lock
 *
 * @return Status of operation. Zero is success.
 */
int32_t parklens_wake_lock_acquire(struct parklens_wake_lock *lock);

/**
 * @lock: The wake lock to release
 *
 * This API release a wake lock
 *
 * @return Status of operation. Zero is success.
 */
int32_t parklens_wake_lock_release(struct parklens_wake_lock *lock);

/**
 * @lock: The wake lock to destroy
 *
 * This API destroy a wake lock
 *
 * @return Status of operation. Zero is success.
 */
int32_t parklens_wake_lock_destroy(struct parklens_wake_lock *lock);

/**
 * @v: A pointer to an opaque atomic variable
 *
 * This API initialize an atomic type variable
 */
static inline void parklens_atomic_init(parklens_atomic_t *v)
{
	atomic_set(v, 0);
}

/**
 * @v: A pointer to an opaque atomic variable
 *
 * The API read the value of an atomic variable
 *
 * @return Current value of the variable
 */
static inline int32_t parklens_atomic_read(parklens_atomic_t *v)
{
	return atomic_read(v);
}

/**
 * @v: A pointer to an opaque atomic variable
 *
 *  The API set a value to the value of an atomic variable
 */
static inline void parklens_atomic_set(parklens_atomic_t *v, int i)
{
	atomic_set(v, i);
}

/**
 * @event: Pointer to the opaque event object to initialize
 *
 * This API initializes the specified event. Upon
 * successful initialization, the state of the event becomes initialized
 * and not signalled.
 *
 * An event must be initialized before it may be used in any other event
 * functions.
 * Attempting to initialize an already initialized event results in
 * a failure.
 *
 * @return: Result of creating event. Negative in case of error. Zero otherwise.
 */
int32_t parklens_event_create(struct parklens_event *event);

/**
 * @event: The event to set to the signalled state
 *
 * This API set event to the signalled state
 *
 * @return: Result of set event. Negative in case of error. Zero otherwise.
 */
int32_t parklens_event_set(struct parklens_event *event);

/**
  resets a parklens event
 * @event: The event to set to the NOT signalled state
 *
 * The state of the specified event is set to 'NOT signalled' by calling
 * The state of the event remains NOT signalled until an explicit call
 * to parklens_event_set().
 *
 * This function sets the event to a NOT signalled state even if the event was
 * signalled multiple times before being signaled.
 *
 * @return: Result of set event. Negative in case of error. Zero otherwise.
 */
int32_t parklens_event_reset(struct parklens_event *event);

/**
 * @event: Pointer to an event to wait on.
 * @timeout: Timeout value (in milliseconds).
 *
 * This API waits for the event to be set.
 *
 * This function returns
 * if this interval elapses, regardless if any of the events have
 * been set.  An input value of 0 for this timeout parameter means
 * to wait infinitely, meaning a timeout will never occur.
 *
 * @return: Result of set event. Negative in case of error. Zero otherwise.
 */
int32_t parklens_wait_single_event(struct parklens_event *event, uint32_t timeout);

/**
 * @event: The event object to be destroyed.
 *
 * This API destroys a parklens event
 *
 * A destroyed event object can be reinitialized using parklens_event_create();
 * the results of otherwise referencing the object after it has been destroyed
 * are undefined.  Calls to parklens event functions to manipulate the lock such
 * as parklens_event_set() will fail if the event is destroyed.  Therefore,
 * don't use the event after it has been destroyed until it has
 * been re-initialized.
 *
 * @return: Result of set event. Negative in case of error. Zero otherwise.
 */
int32_t parklens_event_destroy(struct parklens_event *event);

#endif
