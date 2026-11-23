

#ifndef UAPI_KERNELMONITOR_H
#define UAPI_KERNELMONITOR_H

#include <linux/ptrace.h>
#include <linux/ioctl.h>

struct pt_regs;

struct xm_timespec {
	unsigned long tv_sec;
	unsigned long tv_usec;
};

#ifndef __KERNEL__
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#define __user

extern unsigned long run_in_host;
extern unsigned long debug_mode;
#endif


#ifndef __KERNEL__
#define __user
#endif
struct xm_ioctl_dump_param {
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
};


#define BACKTRACE_DEPTH 30
#define XM_USER_STACK_SIZE (16 * 1024)
#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 16
#endif
#define CGROUP_NAME_LEN 32
#define PROCESS_CHAINS_COUNT 10
#define PROCESS_ARGV_LEN 128

#define XM_PATH_LEN 100

#define XM_NR_SOFTIRQS 10

#define XM_EVENT_TYPE_INTERVAL 50

enum xm_record_id {
	et_xm_xm = 761203000,

	et_pupil_base = et_xm_xm + XM_EVENT_TYPE_INTERVAL,
	et_pupil_task,
	et_pupil_dump_stack,
	et_pupil_exist_pid,
	et_pupil_exist_comm,

	et_alloc_load_base = et_pupil_base + XM_EVENT_TYPE_INTERVAL,
	et_alloc_load_summary,
	et_alloc_load_detail,
	et_alloc_load_stop,

	et_alloc_top_base = et_alloc_load_base + XM_EVENT_TYPE_INTERVAL,
	et_alloc_top_detail,

	et_drop_packet_base = et_alloc_top_base + XM_EVENT_TYPE_INTERVAL,
	et_drop_packet_summary,
	et_drop_packet_detail,

	et_fs_orphan_base = et_drop_packet_base + XM_EVENT_TYPE_INTERVAL,
	et_fs_orphan_summary,
	et_fs_orphan_detail,

	et_exec_monitor_base = et_fs_orphan_base + XM_EVENT_TYPE_INTERVAL,
	et_exec_monitor_detail,
	et_exec_monitor_perf,

	et_exit_monitor_base = et_exec_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_exit_monitor_detail,
	et_exit_monitor_map,

	et_fs_shm_base = et_exit_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_fs_shm_detail,

	et_irq_delay_base = et_fs_shm_base + XM_EVENT_TYPE_INTERVAL,
	et_irq_delay_detail,

	et_irq_stats_base = et_irq_delay_base + XM_EVENT_TYPE_INTERVAL,
	et_irq_stats_header,
	et_irq_stats_irq_summary,
	et_irq_stats_irq_detail,
	et_irq_stats_softirq_summary,
	et_irq_stats_timer_summary,

	et_irq_trace_base = et_irq_stats_base + XM_EVENT_TYPE_INTERVAL,
	et_irq_trace_detail,
	et_irq_trace_sum,

	et_kprobe_base = et_irq_trace_base + XM_EVENT_TYPE_INTERVAL,
	et_kprobe_detail,
	et_kprobe_raw_detail,

	et_load_monitor_base = et_kprobe_base + XM_EVENT_TYPE_INTERVAL,
	et_load_monitor_detail,
	et_load_monitor_task,
	et_load_monitor_cpu_run,

	et_mm_leak_base = et_load_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_mm_leak_detail,

	et_mutex_monitor_base = et_mm_leak_base + XM_EVENT_TYPE_INTERVAL,
	et_mutex_monitor_detail,

	et_perf_base = et_mutex_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_perf_detail,
	et_perf_raw_detail,

	et_proc_monitor_base = et_perf_base + XM_EVENT_TYPE_INTERVAL,
	et_proc_monitor_summary,
	et_proc_monitor_detail,

	et_run_trace_base = et_proc_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_run_trace,
	et_start,
	et_sched_in,
	et_sched_out,
	et_sched_wakeup,
	et_sys_enter,
	et_sys_enter_raw,
	et_sys_exit,
	et_irq_handler_entry,
	et_irq_handler_exit,
	et_softirq_entry,
	et_softirq_exit,
	et_timer_expire_entry,
	et_timer_expire_exit,
	et_run_trace_perf,
	et_run_trace_raw,
	et_stop,
	et_stop_raw_stack,

	et_runq_info_base = et_run_trace_base + XM_EVENT_TYPE_INTERVAL,
	et_runq_info_summary,
	et_runq_info_detail,

	et_rw_top_base = et_runq_info_base + XM_EVENT_TYPE_INTERVAL,
	et_rw_top_detail,
	et_rw_top_perf,
	et_rw_top_raw_perf,

	et_sys_delay_base = et_rw_top_base + XM_EVENT_TYPE_INTERVAL,
	et_sys_delay_detail,

	et_tcp_retrans_base = et_sys_delay_base + XM_EVENT_TYPE_INTERVAL,
	et_tcp_retrans_summary,
	et_tcp_retrans_detail,
	et_tcp_retrans_trace,

	et_utilization_base = et_tcp_retrans_base + XM_EVENT_TYPE_INTERVAL,
	et_utilization_detail,

	et_sched_delay_base = et_utilization_base + XM_EVENT_TYPE_INTERVAL,
	et_sched_delay_dither,
	et_sched_delay_rq,

	et_reboot_base = et_sched_delay_base + XM_EVENT_TYPE_INTERVAL,
	et_reboot_detail,

	et_df_du_base = et_reboot_base + XM_EVENT_TYPE_INTERVAL,
	et_df_du_detail,

	et_ping_delay_base = et_df_du_base + XM_EVENT_TYPE_INTERVAL,
	et_ping_delay_summary,
	et_ping_delay_detail,
	et_ping_delay_event,

	et_uprobe_base = et_ping_delay_base + XM_EVENT_TYPE_INTERVAL,
	et_uprobe_detail,
	et_uprobe_raw_detail,

	et_sys_cost_base = et_uprobe_base + XM_EVENT_TYPE_INTERVAL,
	et_sys_cost_detail,

	et_fs_cache = et_sys_cost_base + XM_EVENT_TYPE_INTERVAL,
	et_fs_cache_detail,

	et_high_order = et_fs_cache + XM_EVENT_TYPE_INTERVAL,
	et_high_order_detail,

	et_d = et_high_order + XM_EVENT_TYPE_INTERVAL,
	et_d_detail,

	et_net_bandwidth_base = et_d + XM_EVENT_TYPE_INTERVAL,
	et_net_bandwidth_summary,
	et_net_bandwidth_detail,

	et_sig_info_base = et_net_bandwidth_base + XM_EVENT_TYPE_INTERVAL,
	et_sig_info_detail,

	et_task_monitor_base = et_sig_info_base + XM_EVENT_TYPE_INTERVAL,
	et_task_monitor_summary,
	et_task_monitor_detail,

	et_rw_sem_base = et_task_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_rw_sem_detail,

	et_rss_monitor_base = et_rw_sem_base + XM_EVENT_TYPE_INTERVAL,
	et_rss_monitor_detail,
	et_rss_monitor_raw_detail,

	et_memcg_stats_base = et_rss_monitor_base + XM_EVENT_TYPE_INTERVAL,
	et_memcg_stats_summary,
	et_memcg_stats_detail,

	et_throttle_delay_base = et_memcg_stats_base + XM_EVENT_TYPE_INTERVAL,
	et_throttle_delay_dither,
	et_throttle_delay_rq,

	et_tcp_connect_base = et_throttle_delay_base + XM_EVENT_TYPE_INTERVAL,
	et_tcp_connect_detail,

	et_pmu_base = et_tcp_connect_base + XM_EVENT_TYPE_INTERVAL,
	et_pmu_detail,

	et_count
};

struct xm_proc_chains_detail {
	unsigned int full_argv[PROCESS_CHAINS_COUNT];
	char chains[PROCESS_CHAINS_COUNT][PROCESS_ARGV_LEN];
	unsigned int tgid[PROCESS_CHAINS_COUNT];
};

struct xm_task_detail {
	char cgroup_buf[CGROUP_NAME_LEN];
	char cgroup_cpuset[CGROUP_NAME_LEN];
	int pid;
	int tgid;
	int container_pid;
	int container_tgid;
	long state;
	int task_type;
	unsigned long syscallno;
	/**
	 * 0->user 1->sys 2->idle
	 */
	unsigned long sys_task;
	/**
	 * 1->user mode 0->sys mode -1->unknown
	 */
	unsigned long user_mode;
	char comm[TASK_COMM_LEN];
};

struct xm_kern_stack_detail {
	unsigned long stack[BACKTRACE_DEPTH];
};

struct xm_user_stack_detail {
	struct user_pt_regs regs;
	unsigned long ip;
	unsigned long bp;
	unsigned long sp;
	unsigned long stack[BACKTRACE_DEPTH];
};

struct xm_raw_stack_detail {
	struct user_pt_regs regs;
	unsigned long ip;
	unsigned long bp;
	unsigned long sp;
	unsigned long stack_size;
	unsigned long stack[XM_USER_STACK_SIZE / sizeof(unsigned long)];
};

#endif /* UAPI_KERNELMONITOR_H */
