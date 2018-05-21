/*
 * work.h .
 */

#ifndef _LINUX_WORK_H
#define _LINUX_WORK_H

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
	atomic_long_t data;
	struct list_head entry;
	work_func_t func;
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};

#endif
