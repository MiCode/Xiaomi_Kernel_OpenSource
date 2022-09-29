/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _XGF_H_
#define _XGF_H_

#include <linux/rbtree.h>
#include <linux/tracepoint.h>
#include <linux/slab.h>

#define EMA_DIVISOR 10
#define EMA_DIVIDEND 5
#define EMA_REST_DIVIDEND (EMA_DIVISOR - EMA_DIVIDEND)
#define SP_ALLOW_NAME "UnityMain"
#define SP_ALLOW_NAME2 "Thread-"
#define XGF_DEP_FRAMES_MIN 2
#define XGF_DEP_FRAMES_MAX 20
#define XGF_DO_SP_SUB 0
#define TIME_50MS  50000000
#define XGF_MAX_SPID_LIST_LENGTH 20
#define DEFAULT_DFRC 60
#define TARGET_FPS_LEVEL 10
#define MAX_DEP_PATH_NUM 80
#define MAX_DEP_TASK_NUM 100
#define N 8

enum XGF_ERROR {
	XGF_NOTIFY_OK,
	XGF_SLPTIME_OK,
	XGF_FUNC_OK,
	XGF_DISABLE,
	XGF_THREAD_NOT_FOUND,
	XGF_PARAM_ERR,
	XGF_BUFFER_ERR,
	XGF_BUFFER_OW,
	XGF_BUFFER_ZERO,
	XGF_DEP_LIST_ERR,
	XGF_GLOBAL_IDX_ERR
};

enum {
	XGF_QUEUE_START,
	XGF_QUEUE_END,
	XGF_DEQUEUE_START,
	XGF_DEQUEUE_END
};

enum XGF_EVENT {
	IRQ_ENTRY,
	IRQ_EXIT,
	SCHED_WAKING,
	SCHED_SWITCH,
	HRTIMER_ENTRY,
	HRTIMER_EXIT
};

enum XGF_DEPS_CAT {
	INNER_DEPS,
	OUTER_DEPS,
	PREVI_DEPS
};

enum XGF_ALLOC {
	NATIVE_ALLOC,
	XGF_RENDER,
	XGF_DEP,
	XGF_RUNTIME_SECT,
	XGF_SPID,
	XGFF_FRAME
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

struct xgf_runtime_sect {
	struct hlist_node hlist;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

struct xgf_dep_task {
	pid_t tid;
	pid_t caller_tid;
	unsigned long long ts;
};

struct xgf_render_dep {
	int finish_flag;

	struct xgf_dep_task xdt_arr[MAX_DEP_TASK_NUM];
	int xdt_arr_idx;

	int track_tid;
	unsigned long long track_ts;
	int raw_head_index;
	int raw_tail_index;
	unsigned long long raw_head_ts;
	unsigned long long raw_tail_ts;

	int specific_flag;
};

struct xgf_render {
	struct hlist_node hlist;
	pid_t parent;
	pid_t render;
	unsigned long long bufID;

	struct xgf_render_dep xrd_arr[MAX_DEP_PATH_NUM];
	int xrd_arr_idx;
	int l_num;
	int r_num;

	int prev_index;
	unsigned long long prev_ts;
	int curr_index;
	unsigned long long curr_ts;
	int event_count;
	unsigned long long prev_queue_end_ts;

	int frame_count;

	struct rb_root deps_list;
	struct rb_root out_deps_list;
	struct rb_root prev_deps_list;

	struct xgf_runtime_sect deque;
	struct xgf_runtime_sect queue;

	unsigned long long raw_runtime;
	unsigned long long ema_runtime;

	int spid;
	int dep_frames;

	int ema2_enable;
	struct xgf_ema2_predictor *ema2_pt;
};

struct xgf_thread_loading {
	int pid;
	unsigned long long buffer_id;
	unsigned long long loading;
	int last_cb_ts;
};

struct xgff_runtime {
	int pid;
	unsigned long long loading;
};

struct xgff_frame {
	struct hlist_node hlist;
	pid_t parent;
	pid_t tid;
	unsigned long long bufid;
	unsigned long frameid;
	unsigned long long ts;
	struct xgf_thread_loading ploading;
	struct xgf_render xgfrender;
	struct xgff_runtime dep_runtime[XGF_DEP_FRAMES_MAX];
	int count_dep_runtime;
	int is_start_dep;
};

struct xgf_dep {
	struct rb_node rb_node;

	pid_t tid;
	int render_dep;
	int frame_idx;
	int action;
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

struct xgf_policy_cmd {
	struct rb_node rb_node;

	int tgid;
	int ema2_enable;
	unsigned long long ts;
};

struct fpsgo_trace_event {
	unsigned long long ts;
	int event;
	int cpu;
	int note;
	int state;
	int pid;
};

extern int (*xgf_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
extern int (*xgff_est_runtime_fp)(
	pid_t r_pid,
	struct xgf_render *render,
	unsigned long long *runtime,
	unsigned long long ts
	);
extern int (*xgff_update_start_prev_index_fp)(struct xgf_render *render);
extern int (*fpsgo_xgf2ko_calculate_target_fps_fp)(
	int pid,
	unsigned long long bufID,
	int *target_fps_margin,
	unsigned long long cur_queue_end_ts,
	int eara_is_active);
extern void (*fpsgo_xgf2ko_do_recycle_fp)(int pid,
	unsigned long long bufID);
extern long long (*xgf_ema2_predict_fp)(struct xgf_ema2_predictor *pt, long long X);
extern void (*xgf_ema2_init_fp)(struct xgf_ema2_predictor *pt);

void xgf_trace(const char *fmt, ...);
int xgf_est_runtime(pid_t r_pid, struct xgf_render *render,
	unsigned long long *runtime, unsigned long long ts);
int xgff_est_runtime(pid_t r_pid, struct xgf_render *render,
	unsigned long long *runtime, unsigned long long ts);
int xgff_update_start_prev_index(struct xgf_render *render);
void *xgf_alloc(int size, int cmd);
void xgf_free(void *pvBuf, int cmd);
int *xgf_extra_sub_assign(void);
int *xgf_spid_sub_assign(void);
int xgf_num_possible_cpus(void);
int xgf_get_task_wake_cpu(struct task_struct *t);
int xgf_get_task_pid(struct task_struct *t);
long xgf_get_task_state(struct task_struct *t);
void notify_xgf_ko_ready(void);
void xgf_get_runtime(pid_t tid, u64 *runtime);
int xgf_get_logical_tid(int rpid, int tgid, int *l_tid,
	unsigned long long prev_ts, unsigned long long last_ts);
unsigned long long xgf_get_time(void);
unsigned long long xgf_calculate_sqrt(unsigned long long x);
int xgf_dep_frames_mod(struct xgf_render *render, int pos);
struct xgf_dep *xgf_get_dep(pid_t tid, struct xgf_render *render,
	int pos, int force);
void xgf_update_deps_list(struct xgf_render *render, int pos);
void xgf_duplicate_deps_list(struct xgf_render *render);
void xgf_clean_deps_list(struct xgf_render *render, int pos);

void fpsgo_ctrl2xgf_switch_xgf(int val);

int fpsgo_comp2xgf_qudeq_notify(int rpid, unsigned long long bufID, int cmd,
	unsigned long long *run_time, unsigned long long ts, int skip);
void fpsgo_fstb2xgf_do_recycle(int fstb_active);
int has_xgf_dep(pid_t tid);

int fpsgo_fstb2xgf_get_target_fps(int pid, unsigned long long bufID,
	int *target_fps_margin, unsigned long long cur_queue_end_ts,
	int eara_is_active);
int fpsgo_xgf2ko_calculate_target_fps(int pid, unsigned long long bufID,
	int *target_fps_margin, unsigned long long cur_queue_end_ts,
	int eara_is_active);
int fpsgo_fstb2xgf_notify_recycle(int pid, unsigned long long bufID);
void fpsgo_xgf2ko_do_recycle(int pid, unsigned long long bufID);
void fpsgo_ctrl2xgf_set_display_rate(int dfrc_fps);
int xgf_get_display_rate(void);
int xgf_get_process_id(int pid);
int xgf_check_main_sf_pid(int pid, int process_id);
int xgf_check_specific_pid(int pid);
void xgf_set_timer_info(int pid, unsigned long long bufID,
	int hrtimer_pid, int hrtimer_flag,
	unsigned long long hrtimer_ts, unsigned long long prev_queue_end_ts);

long long xgf_ema2_predict(struct xgf_ema2_predictor *pt, long long X);
void xgf_ema2_init(struct xgf_ema2_predictor *pt);

extern int xgf_trace_enable;
extern atomic_t xgf_ko_enable;
extern struct fpsgo_trace_event *xgf_event_buffer;
extern atomic_t xgf_event_buffer_idx;
extern int xgf_event_buffer_size;
extern struct fpsgo_trace_event *fstb_event_buffer;
extern atomic_t fstb_event_buffer_idx;
extern int fstb_event_buffer_size;
extern int fstb_frame_num;
extern int fstb_no_stable_thr;
extern int fstb_can_update_thr;
extern int fstb_target_fps_margin_low_fps;
extern int fstb_target_fps_margin_high_fps;
extern int fstb_fps_num;
extern int fstb_fps_choice[];
extern int fstb_no_r_timer_enable;

int __init init_xgf(void);
int __exit exit_xgf(void);

struct xgf_thread_loading fbt_xgff_list_loading_add(int pid,
	unsigned long long buffer_id, unsigned long long ts);
long fbt_xgff_get_loading_by_cluster(struct xgf_thread_loading *ploading,
					unsigned long long ts,
					unsigned int prefer_cluster, int skip, long *area);

#endif
