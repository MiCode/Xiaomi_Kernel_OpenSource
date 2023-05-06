#ifndef _MI_LOG_MODULE_H_
#define _MI_LOG_MODULE_H_
#define MI_LOG_MODE_RO  0440
#define TASK_COMM_LEN 16

#define RET_OK 0
#define RET_FAIL -1

#define FILL_STATE_NONE 0
#define FILL_STATE_NONE_THREAD 1
#define FILL_STATE_ONGOING 2
#define FILL_STATE_FINISHED 3

struct delay_struct {
	u64 version;
	u64 blkio_delay;      /* wait for sync block io completion */
	u64 swapin_delay;     /* wait for swapin block io completion */
	u64 freepages_delay;  /* wait for memory reclaim */
	u64 cpu_runtime;
	u64 cpu_run_delay;
	u64 utime;
	u64 stime;
};

struct binder_delay_info {
	u64 version;
	pid_t pid;
	pid_t binder_target_pid;
	char binder_target_comm[TASK_COMM_LEN];
	pid_t binder_target_tid;
	u64 blkio_delay;
	u64 swapin_delay;
	u64 freepages_delay;
	u64 cpu_runtime;
	u64 cpu_run_delay;
	u64 utime;
	u64 stime;
	u64 binder_target_thread_full;
};

struct task_node {
	u64 version;
	pid_t pid;
	u64 blkio_delay;
	u64 swapin_delay;
	u64 freepages_delay;
	u64 cpu_runtime;
	u64 cpu_run_delay;
	u64 utime;
	u64 stime;
	int fill_state;
	struct rb_node rb_node;
	struct binder_delay_info delay_info;
};
#endif
