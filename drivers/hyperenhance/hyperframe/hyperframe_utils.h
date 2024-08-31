// MIUI ADD: Performance_FramePredictBoost
#ifndef HYPERFRAME_UTILS_H
#define HYPERFRAME_UTILS_H

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/trace.h>

#include "hyperframe_base.h"

extern uint32_t hyperframe_systrace_mask;

#define KOBJ_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)
#define KOBJ_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)
#define KOBJ_ATTR_RWO(_name)     \
	struct kobj_attribute kobj_attr_##_name =       \
		__ATTR(_name, 0664,     \
		_name##_show, _name##_store)
#define KOBJ_ATTR_ROO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0444,	\
		_name##_show, NULL)

#define HYPERFRAME_SYSTRACE_LIST(macro) \
	macro(BASE, 0), \
	macro(FRAMEINFO, 1), \
	macro(CPU, 2), \
	macro(PREDICT, 3), \
	macro(SCHED, 4), \
	macro(MAX, 5), \

#define GENERATE_ENUM(name, shft) HYPERFRAME_DEBUG_##name = 1U << shft
enum {
	HYPERFRAME_SYSTRACE_LIST(GENERATE_ENUM)
};

void __hyperframe_systrace_c(pid_t pid, unsigned long long val, const char *name, ...);
void __hyperframe_systrace_b(pid_t tgid, const char *name, ...);
void __hyperframe_systrace_e(void);

// 'v' means 'value'
#define hyperframe_systrace_c(mask, pid, val, fmt...) \
	do { \
		if (hyperframe_systrace_mask & mask) \
			__hyperframe_systrace_c(pid, val, fmt); \
	} while (0)

#define hyperframe_systrace_b(mask, tgid, fmt, args...) \
	do { \
		if (hyperframe_systrace_mask & mask) \
			__hyperframe_systrace_b(tgid, fmt, ##args); \
	} while (0)

#define hyperframe_systrace_e(mask) \
	do { \
		if (hyperframe_systrace_mask & mask) \
			__hyperframe_systrace_e(); \
	} while (0)

// 'fi' means 'frameinfo'
#define htrace_c_fi(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_BASE, pid, val, fmt)
#define htrace_c_fi_debug(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_FRAMEINFO, pid, val, fmt)

#define htrace_c_cpu(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_BASE, pid, val, fmt)
#define htrace_c_cpu_debug(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_CPU, pid, val, fmt)

#define htrace_c_predict(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_BASE, pid, val, fmt)
#define htrace_c_predict_debug(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_PREDICT, pid, val, fmt)

#define htrace_c_sched(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_BASE, pid, val, fmt)
#define htrace_c_sched_debug(pid, val, fmt...) \
	hyperframe_systrace_c(HYPERFRAME_DEBUG_SCHED, pid, val, fmt)

// 'Trace Begin' start
#define htrace_b_fi(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_BASE, tgid, fmt, ##args)
#define htrace_b_fi_debug(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_FRAMEINFO, tgid, fmt, ##args)

#define htrace_b_cpu(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_BASE, tgid, fmt, ##args)
#define htrace_b_cpu_debug(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_CPU, tgid, fmt, ##args)

#define htrace_b_predict(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_BASE, tgid, fmt, ##args)
#define htrace_b_predict_debug(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_PREDICT, tgid, fmt, ##args)

#define htrace_b_sched(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_BASE, tgid, fmt, ##args)
#define htrace_b_sched_debug(tgid, fmt, args...) \
	hyperframe_systrace_b(HYPERFRAME_DEBUG_SCHED, tgid, fmt, ##args)
// 'Trace Begin' end

// 'Trace End' start
#define htrace_e_fi() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_BASE)
#define htrace_e_fi_debug() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_FRAMEINFO)

#define htrace_e_cpu() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_BASE)
#define htrace_e_cpu_debug() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_CPU)

#define htrace_e_predict() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_BASE)
#define htrace_e_predict_debug() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_PREDICT)

#define htrace_e_sched() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_BASE)
#define htrace_e_sched_debug() \
	hyperframe_systrace_e(HYPERFRAME_DEBUG_SCHED)
// 'Trace End' end

// file system.
int hyperframe_create_dir(struct kobject *parent, const char *name, struct kobject **ppsKobj);
void hyperframe_remove_dir(struct kobject **ppsKobj);
void hyperframe_create_file(struct kobject *parent, struct kobj_attribute *kobj_attr);
void hyperframe_remove_file(struct kobject *parent, struct kobj_attribute *kobj_attr);
void hyperframe_filesystem_init(void);
void hyperframe_filesystem_exit(void);

int init_hyperframe_debug(void);

void idx_add(int* val, int base, int increase, int boundary);
int idx_add_safe(int base, int increase, int boundary);
void idx_sub(int* val, int base, int decrease, int boundary);
int idx_sub_safe(int base, int decrease, int boundary);
void idx_add_self(int* val, int increase, int boundary);
void idx_sub_self(int* val, int decrease, int boundary);

unsigned long long hyperframe_get_time(void);
int nsec_to_100usec(unsigned long long nsec);

#endif
// END Performance_FramePredictBoost