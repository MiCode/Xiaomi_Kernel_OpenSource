// MIUI ADD: Performance_TurboSched
#define pr_fmt(fmt) "game-link: " fmt
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pkg_stat.h>
#include <linux/sched/signal.h>
#include <linux/mi_sched.h>
#include <../../kernel/locking/mutex.h>
#include <../../kernel/locking/rtmutex_common.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/binder.h>
#include "../../../kernel/sched/walt/walt.h"
#include <linux/sched/walt.h>
#include "../include/mi_module.h"

#define MUTEX_FLAGS		0x07
#define RWSEM_READER_OWNED	(1UL << 0)
#define RWSEM_NONSPINNABLE	(1UL << 1)
#define RWSEM_OWNER_FLAGS_MASK	(RWSEM_READER_OWNED | RWSEM_NONSPINNABLE)
#define RWSEM_WRITER_MASK	(1UL << 0)

static unsigned int game_link_debug;
module_param(game_link_debug, uint, 0644);

extern struct gt_task *get_gt_task(struct task_struct *tsk);
extern void register_game_wake_f(game_wake_f f);
extern void unregister_game_wake_f(void);
extern void register_mi_dequeue_task_fair_hook(int mod, mi_dequeue_task_fair f);extern void unregister_mi_dequeue_task_fair_hook(int mod);
extern void game_dequeue_task(struct rq *rq, struct task_struct *p);
extern void game_wake_task(struct task_struct *p);
extern void set_flw_flag(struct task_struct *tsk, unsigned int rmask,
				unsigned int clear_mask);
extern bool is_rcal_task(struct task_struct *tsk, int no_aging);
extern bool is_render_thread(struct task_struct *tsk);
extern int game_super_task(struct task_struct *p);
extern void game_update_lock_time(struct task_struct *p);
extern bool frame_load_enable(void);
extern bool mi_task_exiting(struct task_struct *p);
extern void register_rvh_mi_try_to_wake_up(int mod, mi_try_to_wake_up f);
extern void unregister_rvh_mi_try_to_wake_up(int);

static inline struct task_struct *__mutex_owner(struct mutex *lock)
{
	return (struct task_struct *)(atomic_long_read(&lock->owner) & ~MUTEX_FLAGS);
}

static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem)
{
	return (struct task_struct *)
		(atomic_long_read(&sem->owner) & ~RWSEM_OWNER_FLAGS_MASK);
}

static bool is_lock_task(struct task_struct *p)
{
	struct gt_task *gt_tsk;
	gt_tsk = get_gt_task(p);
	if (!gt_tsk)
		return false;
	return gt_tsk->flag & MASK_LOCK_TASK;
}

bool is_wait_lock(struct task_struct *p)
{
	struct gt_task *gt_tsk;
	gt_tsk = get_gt_task(p);
	if (!gt_tsk)
		return false;
	return gt_tsk->flag & MASK_WAIT_LOCK;
}
static void render_clain_flw(struct task_struct *task, int wake_flags)
{
	int sync;

	if (!frame_load_enable())
		return;

	if (likely(!is_rcal_task(current, 0)))
		return;

	sync = (wake_flags & WF_SYNC) && (!(current->flags & PF_EXITING));

	if (!sync)
		return;

	game_wake_task(task);
}

static void game_dequeue_hooks(struct rq *rq, struct task_struct *p,
  				     int flags)
{

	if (likely(!frame_load_enable()))
		return;

	if (!(flags & DEQUEUE_SLEEP))
		return;

	if (is_lock_task(p) && !is_wait_lock(p))
		set_flw_flag(p, 0, MASK_LOCK_TASK);

	game_dequeue_task(rq, p);
}

static void do_set_lock_vip(struct task_struct *tsk)
{
	struct gt_task *gt_tsk;

	if (unlikely(refcount_read(&tsk->usage) == 0))
		return;

	if (!tsk || !is_rcal_task(current, 0)) {
		set_flw_flag(current, MASK_WAIT_LOCK, 0);
		return;
	}

	set_flw_flag(current, MASK_AGING_CAL | MASK_WAIT_LOCK, 0);
	set_flw_flag(tsk, MASK_CAL | MASK_LOCK_TASK, MASK_AGING_CAL);

	gt_tsk = get_gt_task(tsk);
	if (!gt_tsk)
		return;

	gt_tsk->lock_start_time = walt_sched_clock();
}

static void mi_mutex_wait_start_hook(void *nouse, struct mutex *lock)
{
	bool is_owner_exit = false;
	struct task_struct *lock_tsk;

	if (!frame_load_enable())
		return;

	rcu_read_lock();
	lock_tsk = __mutex_owner(lock);
	is_owner_exit = mi_task_exiting(lock_tsk);
	if (is_owner_exit) {
		rcu_read_unlock();
		return;
	}

	do_set_lock_vip(lock_tsk);
	rcu_read_unlock();
}

static void mi_rt_mutex_wait_start_hook(void *nouse, struct rt_mutex_base *lock)
{
	bool is_owner_exit = false;
	struct task_struct *lock_tsk;

	if (!frame_load_enable())
		return;

	rcu_read_lock();
	lock_tsk = rt_mutex_owner(lock);
	is_owner_exit = mi_task_exiting(lock_tsk);
	if (is_owner_exit) {
		rcu_read_unlock();
		return;
	}

	do_set_lock_vip(lock_tsk);
	rcu_read_unlock();
}

static void mi_mutex_wait_finish_hook(void *nouse, struct task_struct *task)
{
	if (!frame_load_enable())
		return;

	if (current == NULL)
		return;

	rcu_read_lock();
	if (is_wait_lock(task)) {
		set_flw_flag(task, 0, MASK_WAIT_LOCK);
		if (is_lock_task(current)) {
			set_flw_flag(task, MASK_CAL | MASK_LOCK_TASK,
					MASK_AGING_CAL);
			set_flw_flag(current, MASK_AGING_CAL, MASK_LOCK_TASK);
			game_update_lock_time(current);
		}
	}

	rcu_read_unlock();
	if (unlikely(game_link_debug == 10)) {
		if (game_super_task(task) && is_render_thread(task))
			dump_stack();
	}
}

static void mi_rwsem_write_wait_start_hook(void *nouse, struct rw_semaphore *sem)
{
	bool is_owner_exit = false;
	struct task_struct *sem_tsk;
	long count;

	if (!frame_load_enable())
		return;

	rcu_read_lock();
	count = atomic_long_read(&sem->count);

	sem_tsk = rwsem_owner(sem);
	if (!(count & RWSEM_WRITER_MASK)){
		rcu_read_unlock();
		return;
	}

	is_owner_exit = mi_task_exiting(sem_tsk);
	if (is_owner_exit) {
		rcu_read_unlock();
		return;
	}

	do_set_lock_vip(sem_tsk);
	rcu_read_unlock();
}

static void mi_rwsem_read_wait_start_hook(void *nouse, struct rw_semaphore *sem)
{
	bool is_owner_exit = false;
	struct task_struct *sem_tsk;
	long count;

	if (!frame_load_enable())
		return;

	rcu_read_lock();
	count = atomic_long_read(&sem->count);
	sem_tsk = rwsem_owner(sem);
	if (!(count & RWSEM_WRITER_MASK)){
		rcu_read_unlock();
		return;
	}

	is_owner_exit = mi_task_exiting(sem_tsk);
	if (is_owner_exit) {
		rcu_read_unlock();
		return;
	}

	do_set_lock_vip(sem_tsk);
	rcu_read_unlock();
}

void game_link_init(void)
{
	register_game_wake_f(render_clain_flw);
	register_mi_dequeue_task_fair_hook(MIGT_TASK, game_dequeue_hooks);
	register_trace_android_vh_mutex_wait_start(mi_mutex_wait_start_hook, NULL);
	register_trace_android_vh_rtmutex_wait_start(mi_rt_mutex_wait_start_hook, NULL);
	register_trace_android_vh_rwsem_write_wait_start(mi_rwsem_write_wait_start_hook, NULL);
	register_trace_android_vh_rwsem_read_wait_start(mi_rwsem_read_wait_start_hook, NULL);
	register_rvh_mi_try_to_wake_up(MIGT_TASK, mi_mutex_wait_finish_hook);
}

void game_link_exit(void)
{
	unregister_rvh_mi_try_to_wake_up(MIGT_TASK);
	unregister_trace_android_vh_mutex_wait_start(mi_mutex_wait_start_hook, NULL);
	unregister_trace_android_vh_rtmutex_wait_start(mi_rt_mutex_wait_start_hook, NULL);
	unregister_trace_android_vh_rwsem_write_wait_start(mi_rwsem_write_wait_start_hook, NULL);
	unregister_trace_android_vh_rwsem_read_wait_start(mi_rwsem_read_wait_start_hook, NULL);
	unregister_mi_dequeue_task_fair_hook(MIGT_TASK);
	unregister_game_wake_f();
}
// END Performance_TurboSched
