#ifndef _LINUX_MMAP_LOCK_H
#define _LINUX_MMAP_LOCK_H

#include <linux/lockdep.h>
#include <linux/mm_types.h>
#include <linux/mmdebug.h>
#include <linux/rwsem.h>
#include <linux/tracepoint-defs.h>
#include <linux/types.h>

#ifdef CONFIG_SPECULATIVE_PAGE_FAULT
#define MMAP_LOCK_SEQ_INITIALIZER(name) \
	.mmap_seq = 0,
#else
#define MMAP_LOCK_SEQ_INITIALIZER(name)
#endif

#define MMAP_LOCK_INITIALIZER(name)				\
	.mmap_lock = __RWSEM_INITIALIZER((name).mmap_lock),	\
	MMAP_LOCK_SEQ_INITIALIZER(name)

DECLARE_TRACEPOINT(mmap_lock_start_locking);
DECLARE_TRACEPOINT(mmap_lock_acquire_returned);
DECLARE_TRACEPOINT(mmap_lock_released);

#ifdef CONFIG_TRACING

void __mmap_lock_do_trace_start_locking(struct mm_struct *mm, bool write);
void __mmap_lock_do_trace_acquire_returned(struct mm_struct *mm, bool write,
					   bool success);
void __mmap_lock_do_trace_released(struct mm_struct *mm, bool write);

static inline void __mmap_lock_trace_start_locking(struct mm_struct *mm,
						   bool write)
{
	if (tracepoint_enabled(mmap_lock_start_locking))
		__mmap_lock_do_trace_start_locking(mm, write);
}

static inline void __mmap_lock_trace_acquire_returned(struct mm_struct *mm,
						      bool write, bool success)
{
	if (tracepoint_enabled(mmap_lock_acquire_returned))
		__mmap_lock_do_trace_acquire_returned(mm, write, success);
}

static inline void __mmap_lock_trace_released(struct mm_struct *mm, bool write)
{
	if (tracepoint_enabled(mmap_lock_released))
		__mmap_lock_do_trace_released(mm, write);
}

#else /* !CONFIG_TRACING */

static inline void __mmap_lock_trace_start_locking(struct mm_struct *mm,
						   bool write)
{
}

static inline void __mmap_lock_trace_acquire_returned(struct mm_struct *mm,
						      bool write, bool success)
{
}

static inline void __mmap_lock_trace_released(struct mm_struct *mm, bool write)
{
}

#endif /* CONFIG_TRACING */

static inline void mmap_init_lock(struct mm_struct *mm)
{
	init_rwsem(&mm->mmap_lock);
#ifdef CONFIG_SPECULATIVE_PAGE_FAULT
	mm->mmap_seq = 0;
#endif
}

static inline void __mmap_seq_write_lock(struct mm_struct *mm)
{
#ifdef CONFIG_SPECULATIVE_PAGE_FAULT
	VM_BUG_ON_MM(mm->mmap_seq & 1, mm);
	mm->mmap_seq++;
	smp_wmb();
#endif
}

static inline void __mmap_seq_write_unlock(struct mm_struct *mm)
{
#ifdef CONFIG_SPECULATIVE_PAGE_FAULT
	smp_wmb();
	mm->mmap_seq++;
	VM_BUG_ON_MM(mm->mmap_seq & 1, mm);
#endif
}

#ifdef CONFIG_SPECULATIVE_PAGE_FAULT
static inline unsigned long mmap_seq_read_start(struct mm_struct *mm)
{
	unsigned long seq;

	seq = READ_ONCE(mm->mmap_seq);
	smp_rmb();
	return seq;
}

static inline bool mmap_seq_read_check(struct mm_struct *mm, unsigned long seq)
{
	smp_rmb();
	return seq == READ_ONCE(mm->mmap_seq);
}
#endif

static inline void mmap_write_lock(struct mm_struct *mm)
{
	__mmap_lock_trace_start_locking(mm, true);
	down_write(&mm->mmap_lock);
	__mmap_lock_trace_acquire_returned(mm, true, true);
	__mmap_seq_write_lock(mm);
}

static inline void mmap_write_lock_nested(struct mm_struct *mm, int subclass)
{
	__mmap_lock_trace_start_locking(mm, true);
	down_write_nested(&mm->mmap_lock, subclass);
	__mmap_lock_trace_acquire_returned(mm, true, true);
	__mmap_seq_write_lock(mm);
}

static inline int mmap_write_lock_killable(struct mm_struct *mm)
{
	int error;

	__mmap_lock_trace_start_locking(mm, true);
	error = down_write_killable(&mm->mmap_lock);
	__mmap_lock_trace_acquire_returned(mm, true, !error);
	if (likely(!error))
		__mmap_seq_write_lock(mm);
	return error;
}

static inline bool mmap_write_trylock(struct mm_struct *mm)
{
	bool ok;

	__mmap_lock_trace_start_locking(mm, true);
	ok = down_write_trylock(&mm->mmap_lock) != 0;
	__mmap_lock_trace_acquire_returned(mm, true, ok);
	if (likely(ok))
		__mmap_seq_write_lock(mm);
	return ok;
}

static inline void mmap_write_unlock(struct mm_struct *mm)
{
	__mmap_lock_trace_released(mm, true);
	__mmap_seq_write_unlock(mm);
	up_write(&mm->mmap_lock);
}

static inline void mmap_write_downgrade(struct mm_struct *mm)
{
	__mmap_lock_trace_acquire_returned(mm, false, true);
	__mmap_seq_write_unlock(mm);
	downgrade_write(&mm->mmap_lock);
}

static inline void mmap_read_lock(struct mm_struct *mm)
{
	__mmap_lock_trace_start_locking(mm, false);
	down_read(&mm->mmap_lock);
	__mmap_lock_trace_acquire_returned(mm, false, true);
}

static inline int mmap_read_lock_killable(struct mm_struct *mm)
{
	int error;

	__mmap_lock_trace_start_locking(mm, false);
	error = down_read_killable(&mm->mmap_lock);
	__mmap_lock_trace_acquire_returned(mm, false, !error);
	return error;
}

static inline bool mmap_read_trylock(struct mm_struct *mm)
{
	bool ok;

	__mmap_lock_trace_start_locking(mm, false);
	ok = down_read_trylock(&mm->mmap_lock) != 0;
	__mmap_lock_trace_acquire_returned(mm, false, ok);
	return ok;
}

static inline void mmap_read_unlock(struct mm_struct *mm)
{
	__mmap_lock_trace_released(mm, false);
	up_read(&mm->mmap_lock);
}

static inline void mmap_read_unlock_non_owner(struct mm_struct *mm)
{
	__mmap_lock_trace_released(mm, false);
	up_read_non_owner(&mm->mmap_lock);
}

static inline void mmap_assert_locked(struct mm_struct *mm)
{
	lockdep_assert_held(&mm->mmap_lock);
	VM_BUG_ON_MM(!rwsem_is_locked(&mm->mmap_lock), mm);
}

static inline void mmap_assert_write_locked(struct mm_struct *mm)
{
	lockdep_assert_held_write(&mm->mmap_lock);
	VM_BUG_ON_MM(!rwsem_is_locked(&mm->mmap_lock), mm);
}

static inline bool mmap_lock_is_contended(struct mm_struct *mm)
{
	return rwsem_is_contended(&mm->mmap_lock) != 0;
}

#endif /* _LINUX_MMAP_LOCK_H */
