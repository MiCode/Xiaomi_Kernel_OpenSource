/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _XGF_H_
#define _XGF_H_

#include <linux/rbtree.h>

#define HW_EVENT_NUM 2
#define HW_MONITER_WINDOW 10
#define HW_MONITER_LEVEL 8
#define EMA_DIVISOR 10
#define EMA_DIVIDEND 7
#define EMA_REST_DIVIDEND (EMA_DIVISOR - EMA_DIVIDEND)
#define SP_ALLOW_NAME "UnityMain"
#define SP_ALLOW_NAME2 "Thread-"
#define XGF_DEP_FRAMES_MIN 2
#define XGF_DEP_FRAMES_MAX 20
#define XGF_DO_SP_SUB 0
#define XGF_MAX_UFRMAES 200
#define XGF_UBOOST 1
#define XGF_UBOOST_STDDEV_M 1
#define TIME_50MS  50000000
#define UB_SKIP_FRAME 20
#define UB_BEGIN_FRAME 50
#define XGF_MAX_SPID_LIST_LENGTH 20
#define N 8

enum XGF_ERROR {
	XGF_NOTIFY_OK,
	XGF_SLPTIME_OK,
	XGF_DISABLE,
	XGF_THREAD_NOT_FOUND,
	XGF_PARAM_ERR,
	XGF_BUFFER_ERR,
	XGF_BUFFER_OW,
	XGF_BUFFER_ZERO,
	XGF_DEP_LIST_ERR
};

enum {
	XGF_QUEUE_START = 0,
	XGF_QUEUE_END,
	XGF_DEQUEUE_START,
	XGF_DEQUEUE_END
};

enum HW_EVENT {
	HW_AI = 0,
	HW_CV
};

enum XGF_DEPS_CAT {
	INNER_DEPS = 0,
	OUTER_DEPS,
	PREVI_DEPS
};

struct xgf_sub_sect {
	struct hlist_node hlist;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

struct xgf_ema2_predictor {
	/* data */
	long long learn_rate;
	long long beta;
	long long epsilon;
	long long alpha;
	long long mu;
	long long one;

	long long L[N];
	long long W[N];

	long long RMSProp[N];
	long long nabla[N];
	long long rho[N];

	long long x_record[N];
	long long acc_x;
	long long acc_xx[N + 1];
	long long x_front[N];
	long long xt_last;
	long long xt_last_valid;

	int record_idx;
	long long ar_error_diff;
	long long ar_coeff_sum;
	bool ar_coeff_enable;
	int ar_coeff_frames;
	int acc_idx;
	int coeff_shift;
	int order;
	long long t;
	bool ar_coeff_valid;

	int invalid_th;
	unsigned int invalid_input_cnt;
	unsigned int invalid_negative_output_cnt;
	unsigned int skip_grad_update_cnt;
	bool rmsprop_initialized;
	bool x_record_initialized;
	int err_code;
};

struct xgf_render {
	struct hlist_node hlist;
	pid_t parent;
	pid_t render;
	unsigned long long bufID;

	struct hlist_head sector_head;
	int sector_nr;

	struct hlist_head hw_head;

	int prev_index;
	unsigned long long prev_ts;
	int curr_index;
	unsigned long long curr_ts;
	int event_count;

	int frame_count;
	int u_wake_r;
	int u_wake_r_count;

	struct rb_root deps_list;
	struct rb_root out_deps_list;
	struct rb_root prev_deps_list;

	struct xgf_sub_sect deque;
	struct xgf_sub_sect queue;

	unsigned long long ema_runtime;
	unsigned long long pre_u_runtime;
	unsigned long long u_runtime[XGF_MAX_UFRMAES];
	unsigned long long u_avg_runtime;
	unsigned long long u_runtime_sd;
	int u_runtime_idx;

	int spid;
	int dep_frames;

	int hwui_flag;
};

struct xgf_dep {
	struct rb_node rb_node;

	pid_t tid;
	int render_dep;
	int frame_idx;
};

struct xgf_runtime_sect {
	struct hlist_node hlist;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

struct xgf_render_sector {
	struct hlist_node hlist;
	int sector_id;
	int counted;
	struct hlist_head path_head;

	unsigned long long head_ts;
	unsigned long long tail_ts;
	int head_index;
	int tail_index;

	int got_next_index;
	int stop_at_irq;

	int raw_head_index;
	int raw_tail_index;

	int special_rs;
};

struct xgf_pid_rec {
	struct hlist_node hlist;
	pid_t pid;
};

struct xgf_hw_rec {
	struct hlist_node hlist;
	unsigned long long mid;
	int event_type;
	int hw_type;
};

struct xgf_irq_stack {
	int index;
	int event;

	struct xgf_irq_stack *next;
};

struct xgf_hw_event {
	struct hlist_node hlist;

	int event_type;
	int event_index;
	pid_t tid;
	pid_t render;
	unsigned long long mid;
	int valid;

	unsigned long long head_ts;
	unsigned long long tail_ts;
};

struct xgf_spid {
	struct hlist_node hlist;
	char process_name[16];
	char thread_name[16];
	int pid;
	int rpid;
	int tid;
	unsigned long long bufID;
	int action;
};

extern int (*xgf_est_runtime_fp)(pid_t r_pid,
		struct xgf_render *render,
		unsigned long long *runtime,
		unsigned long long ts);
extern int (*fpsgo_xgf2ko_calculate_target_fps_fp)(int pid,
	unsigned long long bufID,
	int *target_fps_margin,
	unsigned long long cur_dequeue_start_ts,
	unsigned long long cur_queue_end_ts);
extern void (*fpsgo_xgf2ko_do_recycle_fp)(int pid,
	unsigned long long bufID);
extern long long (*xgf_ema2_predict_fp)(struct xgf_ema2_predictor *pt, long long X);
extern void (*xgf_ema2_init_fp)(struct xgf_ema2_predictor *pt);

void xgf_lockprove(const char *tag);
void xgf_trace(const char *fmt, ...);
void xgf_reset_renders(void);
int xgf_est_runtime(pid_t r_pid, struct xgf_render *render,
			unsigned long long *runtime, unsigned long long ts);

void *xgf_alloc(int size);
void xgf_free(void *block);
void *xgf_atomic_val_assign(int select);
int *xgf_extra_sub_assign(void);
int *xgf_spid_sub_assign(void);
int xgf_atomic_read(atomic_t *val);
int xgf_atomic_inc_return(atomic_t *val);
void xgf_atomic_set(atomic_t *val, int i);
unsigned int xgf_cpumask_next(int cpu,  const struct cpumask *srcp);
int xgf_num_possible_cpus(void);
int xgf_get_task_wake_cpu(struct task_struct *t);
int xgf_get_task_pid(struct task_struct *t);
long xgf_get_task_state(struct task_struct *t);
void notify_xgf_ko_ready(void);
unsigned long long xgf_get_time(void);
int xgf_dep_frames_mod(struct xgf_render *render, int pos);
struct xgf_dep *xgf_get_dep(pid_t tid, struct xgf_render *render,
	int pos, int force);
void xgf_clean_deps_list(struct xgf_render *render, int pos);
int xgf_hw_events_update(int rpid, struct xgf_render *render);

void fpsgo_ctrl2xgf_switch_xgf(int val);
void fpsgo_ctrl2xgf_nn_job_begin(unsigned int tid, unsigned long long mid);
int fpsgo_ctrl2xgf_nn_job_end(unsigned int tid, unsigned long long mid);

int fpsgo_comp2xgf_qudeq_notify(int rpid, unsigned long long bufID, int cmd,
	unsigned long long *run_time, unsigned long long *mid,
	unsigned long long ts, int hwui_flag);
void fpsgo_fstb2xgf_do_recycle(int fstb_active);
void fpsgo_create_render_dep(void);
int has_xgf_dep(pid_t tid);
int uboost2xgf_get_info(int pid, unsigned long long bufID,
	unsigned long long *timer_period, int *frame_idx);

int fpsgo_xgf2ko_calculate_target_fps(int pid, unsigned long long bufID,
	int *target_fps_margin, unsigned long long cur_dequeue_start_ts,
	unsigned long long cur_queue_end_ts);
void fpsgo_xgf2ko_do_recycle(int pid, unsigned long long bufID);
int xgf_get_display_rate(void);
int xgf_get_process_id(int pid);
int xgf_check_main_sf_pid(int pid, int process_id);
int xgf_check_specific_pid(int pid);
void xgf_set_logical_render_runtime(int pid, unsigned long long bufID,
	unsigned long long l_runtime, unsigned long long r_runtime);
void xgf_set_logical_render_info(int pid, unsigned long long bufID,
	int *l_arr, int l_num, int *r_arr, int r_num,
	unsigned long long l_start_ts,
	unsigned long long f_start_ts);
void xgf_set_timer_info(int pid, unsigned long long bufID,
	int hrtimer_pid, int hrtimer_flag,
	unsigned long long hrtimer_ts, unsigned long long prev_queue_end_ts);

long long xgf_ema2_predict(struct xgf_ema2_predictor *pt, long long X);
void xgf_ema2_init(struct xgf_ema2_predictor *pt);

enum XGF_EVENT {
	SCHED_SWITCH,
	SCHED_WAKEUP,
	IPI_RAISE,
	IPI_ENTRY,
	IPI_EXIT,
	IRQ_ENTRY,
	IRQ_EXIT,
	SOFTIRQ_ENTRY,
	SOFTIRQ_EXIT,
	SCHED_WAKING,
	HRTIMER_ENTRY,
	HRTIMER_EXIT
};

struct xgf_trace_event {
	unsigned long long ts;
	int event;
	int cpu;
	union {
		int prev_pid;
		int pid;
		int target_cpu;
		int irqnr;
		int none;
	};
	union {
		int note;
	};
};

struct fstb_trace_event {
	unsigned long long ts;
	int event;
	int cpu;
	int note;
	int state;
	int pid;
};

extern struct xgf_trace_event *xgf_event_data;
extern void *xgf_event_index;
extern void *xgf_ko_enabled;
extern int xgf_max_events;
extern struct fstb_trace_event *fstb_event_data;
extern atomic_t fstb_event_data_idx;
extern int fstb_event_buffer_size;
extern int fstb_frame_num;
extern int fstb_no_stable_thr;
extern int fstb_no_stable_multiple;
extern int fstb_no_stable_multiple_eara;
extern int fstb_is_eara_active;
extern int fstb_can_update_thr;
extern int fstb_target_fps_margin_low_fps;
extern int fstb_target_fps_margin_high_fps;
extern int fstb_separate_runtime_enable;
extern int fstb_fps_num;
extern int fstb_fps_choice[];

int __init init_xgf(void);

extern int (*xgff_est_runtime_fp)(pid_t r_pid,
		struct xgf_render *render,
		unsigned long long *runtime,
		unsigned long long ts);
int xgff_est_runtime(pid_t r_pid, struct xgf_render *render,
			unsigned long long *runtime, unsigned long long ts);
extern int (*xgff_update_start_prev_index_fp)(struct xgf_render *render);
int xgff_update_start_prev_index(struct xgf_render *render);

void xgff_clean_deps_list(struct xgf_render *render, int pos);
int xgff_dep_frames_mod(struct xgf_render *render, int pos);
struct xgf_dep *xgff_get_dep(
	pid_t tid, struct xgf_render *render, int pos, int force);

#endif
