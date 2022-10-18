#include "cam_actuator_parklens_thread.h"

parklens_thread_t *parklens_thread_run(int32_t (*thread_handler)(void *data),
	void *data,
	const char thread_name[])
{
	struct task_struct *task;

	task = kthread_create(thread_handler, data, thread_name);

	if (IS_ERR(task))
		return NULL;

	wake_up_process(task);

	return task;
}

int32_t parklens_wake_up_process(parklens_thread_t *thread)
{
	return wake_up_process(thread);
}

bool parklens_exit_thread(bool status)
{
	CAM_DBG(CAM_ACTUATOR, "exit - parklens thread");

	if (status == true)
		do_exit(0);
	else
		do_exit(SIGKILL);

	return true;
}

int32_t parklens_event_create(struct parklens_event *event)
{
	if (!event)
		return -1;

	/* check for 'already initialized' event */
	if (event->cookie == LINUX_EVENT_COOKIE)
		return -2;

	/* initialize new event */
	init_completion(&event->complete);
	event->cookie = LINUX_EVENT_COOKIE;

	return 0;
}

int32_t parklens_event_set(struct parklens_event *event)
{
	if (!event)
		return -1;

	/* ensure event is initialized */
	if (event->cookie != LINUX_EVENT_COOKIE)
		return -2;

	event->done = true;
	complete(&event->complete);

	return 0;
}

int32_t parklens_event_reset(struct parklens_event *event)
{
	if (!event)
		return -1;

	/* ensure event is initialized */
	if (event->cookie != LINUX_EVENT_COOKIE)
		return -2;

	/* (re)initialize event */
	event->done = false;
	event->force_set = false;
	INIT_COMPLETION(event->complete);

	return 0;
}

int32_t parklens_wait_single_event(struct parklens_event *event,
        uint32_t timeout)
{
	if (in_interrupt())
		return -1;

	if (!event)
		return -2;

	/* ensure event is initialized */
	if (event->cookie != LINUX_EVENT_COOKIE)
		return -3;

	if (timeout) {
		long ret;

		ret = wait_for_completion_timeout(
			&event->complete,
			msecs_to_jiffies(timeout));

		if (ret <= 0)
			return -4;
	} else {
		wait_for_completion(&event->complete);
	}

	return 0;
}

int32_t parklens_event_destroy(struct parklens_event *event)
{
	if (!event)
		return -1;

	/* ensure event is initialized */
	if (event->cookie != LINUX_EVENT_COOKIE)
		return -2;

	/* make sure nobody is waiting on the event */
	complete_all(&event->complete);

	/* destroy the event */
	memset(event, 0, sizeof(struct parklens_event));

	return 0;
}

int32_t parklens_wake_lock_create(struct parklens_wake_lock *lock,
        const char *name)
{
	memset(lock, 0, sizeof(*lock));
	lock->priv = wakeup_source_register(lock->lock.dev, name);
	if (!(lock->priv)) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to create wake lock");
		return -1;
	}

	lock->lock = *(lock->priv);
	return 0;
}

int32_t parklens_wake_lock_acquire(struct parklens_wake_lock *lock)
{
	__pm_stay_awake(lock->priv);

	return 0;
}

int32_t parklens_wake_lock_release(struct parklens_wake_lock *lock)
{
	__pm_relax(lock->priv);

	return 0;
}

int32_t parklens_wake_lock_destroy(struct parklens_wake_lock *lock)
{
        wakeup_source_unregister(lock->priv);
        return 0;
}
