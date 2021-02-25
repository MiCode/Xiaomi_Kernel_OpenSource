#ifndef BINDER_OEM_H
#define BINDER_OEM_H
#include <linux/sched.h>

#ifndef ENUM_BINDER_STAT
#define ENUM_BINDER_STAT
enum BINDER_STAT {
	BINDER_IN_IDLE,
	BINDER_IN_BUSY,
	BINDER_THREAD_IN_BUSY,
	BINDER_PROC_IN_BUSY,
	BINDER_IN_TRANSACTION,
	BINDER_ST_NUM,
};
#endif

typedef void (*type_binder_send_hook)(struct task_struct *dst, struct task_struct *src,
		int caller_tid, bool oneway, int code);
typedef void (*type_binder_query_hook)(int uid, struct task_struct *tsk, int tid, int pid,
		enum BINDER_STAT reason);

struct oem_binder_hook {
	unsigned long oem_wahead_thresh;
	unsigned long oem_wahead_space;

	type_binder_send_hook oem_buf_overflow_hook;
	type_binder_send_hook oem_reply_hook;
	type_binder_send_hook oem_trans_hook;
	type_binder_send_hook oem_wait4_hook;
	type_binder_query_hook oem_query_st_hook;
};
#endif
