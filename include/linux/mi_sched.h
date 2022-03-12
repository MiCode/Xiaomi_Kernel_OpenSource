#ifndef _LINUX_MI_SCHED_H
#define _LINUX_MI_SCHED_H

#define MI_TASK_MAGIC_NUM	0xabcdabcdacbdadbc
#define MI_USER_MAGIC_NUM	0xdcba1234abcd4321
#define MAGIC_INUSE_NUM 	0xacbdacbd

#include <linux/pkg_stat.h>

enum MI_TASK_TYPE {
	MI_NORMAL_TASK,
	MI_MINOR_TASK,
	MI_REBIND_TASK,
	MI_32BIT_TASK,
	MI_TASK_TYPES
};

#define MASK_MI_NORMAL		(1 << MI_NORMAL_TASK)
#define MINOR_TASK		(1 << MI_MINOR_TASK)
#define MASK_REBIND_TASK	(1 << MI_REBIND_TASK)
#define MASK_32BIT_TASK         (1 << MI_32BIT_TASK)

enum MI_USER_FLAG {
	MI_NORMAL_USER,
	MI_32BIT_APP,
	MI_APP_TYPES,
};

#define MASK_MI_NORUSER          (1 << MI_NORMAL_USER)
#define MASK_32BIT_APP		 (1 << MI_32BIT_APP)

struct mi_user_struct;
struct mi_task_struct {
	unsigned long magic_num;
	bool in_user_list;
	bool in_work_wq;
	int owner_uid;
	enum MI_TASK_TYPE flag;
	cpumask_t cpus_allowed;
	cpumask_t rebind_cpumask;
	struct task_struct *owner;
	struct mi_user_struct *mi_user;
	struct list_head list;
	struct package_runtime_info pkg;
	struct gt_task migt;
};

struct mi_user_struct {
	enum MI_USER_FLAG flag;
	unsigned long magic_num;
	rwlock_t lock;
	struct list_head list;
	struct package_runtime_info pkg;
	struct user_struct *user;
};

struct mi_task_struct *get_mi_task_struct(struct task_struct *tsk);
struct mi_user_struct *get_mi_user_struct(struct user_struct *tsk);
#define MI_TASK_UID(mi_task)    (mi_task->owner_uid)

#endif /*_LINUX_MI_SCHED_H*/


