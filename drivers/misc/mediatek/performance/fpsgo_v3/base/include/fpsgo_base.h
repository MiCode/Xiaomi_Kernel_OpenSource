/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_BASE_H__
#define __FPSGO_BASE_H__

#include <linux/compiler.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/sched/task.h>
#include <linux/sched.h>

#define MAX_DEP_NUM 50
#define WINDOW 20
#define RESCUE_TIMER_NUM 5
#define QUOTA_MAX_SIZE 300
#define GCC_MAX_SIZE 300
#define LOADING_CNT 256
#define FBT_FILTER_MAX_WINDOW 100
#define FPSGO_MW 1
#define BY_PID_DEFAULT_VAL -1
#define BY_PID_DELETE_VAL -2

enum {
	FPSGO_SET_UNKNOWN = -1,
	FPSGO_SET_BOOST_TA = 0,
	FPSGO_SET_IDLE_PREFER = 1,
	FPSGO_SET_SCHED_RATE = 2,
};

/* composite key for render_info rbtree */
struct fbt_render_key {
	int key1;
	unsigned long long key2;
};

struct fbt_jerk {
	int id;
	int jerking;
	int postpone;
	unsigned long long frame_qu_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct fbt_proc {
	int active_jerk_id;
	unsigned long long active_frame_qu_ts;
	struct fbt_jerk jerks[RESCUE_TIMER_NUM];
};

struct fbt_frame_info {
	int target_fps;
	int mips_diff;
	long mips;
	unsigned long long running_time;
	int count;
};

struct fbt_loading_info {
	int target_fps;
	long loading;
	int blc_wt;
	int index;
};

struct fpsgo_loading {
	int pid;
	int loading;
	int prefer_type;
	int policy;
	long nice_bk;
	int action;
	int rmidx;
	int reset_taskmask;
};

struct fbt_thread_blc {
	int pid;
	unsigned long long buffer_id;
	unsigned int blc;
	unsigned int blc_b;
	unsigned int blc_m;
	int dep_num;
	struct fpsgo_loading dep[MAX_DEP_NUM];
	struct list_head entry;
};

struct fbt_boost_info {
	int target_fps;
	unsigned long long target_time;
	unsigned long long t2wnt;
	unsigned int last_blc;
	unsigned int last_blc_b;
	unsigned int last_blc_m;
	unsigned int last_normal_blc;
	unsigned int last_normal_blc_b;
	unsigned int last_normal_blc_m;
	unsigned int sbe_rescue;

	/* adjust loading */
	int loading_weight;
	int weight_cnt;
	int hit_cnt;
	int deb_cnt;
	int hit_cluster;

	/* SeparateCap */
	long *cl_loading;

	/* rescue*/
	struct fbt_proc proc;
	int cur_stage;

	/* variance control */
	struct fbt_frame_info frame_info[WINDOW];
	unsigned int floor;
	int floor_count;
	int reset_floor_bound;
	int f_iter;

	/* filter heavy frames */
	struct fbt_loading_info filter_loading[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_b[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_m[FBT_FILTER_MAX_WINDOW];

	int filter_index;
	int filter_index_b;
	int filter_index_m;
	unsigned int filter_frames_count;
	unsigned int filter_frames_count_b;
	unsigned int filter_frames_count_m;
	int filter_blc;

	/* quota */
	long long quota_raw[QUOTA_MAX_SIZE];
	int quota_cnt;
	int quota_cur_idx;
	int quota_fps;
	int quota;
	int quota_adj; /* remove outlier */
	int quota_mod; /* mod target time */
	int enq_raw[QUOTA_MAX_SIZE];
	int enq_sum;
	int enq_avg;
	int deq_raw[QUOTA_MAX_SIZE];
	int deq_sum;
	int deq_avg;

	/* GCC */
	int gcc_quota;
	int gcc_count;
	int gcc_target_fps;
	int correction;
	int quantile_cpu_time;
	int quantile_gpu_time;

	/* FRS */
	unsigned long long t_duration;
};

struct fpsgo_boost_attr {
	/*    LLF    */
	int loading_th_by_pid;
	int llf_task_policy_by_pid;
	int light_loading_policy_by_pid;

	/*    rescue    */
	int rescue_second_enable_by_pid;
	int rescue_second_time_by_pid;
	int rescue_second_group_by_pid;

	/* filter frame */
	int filter_frame_enable_by_pid;
	int filter_frame_window_size_by_pid;
	int filter_frame_kmin_by_pid;

	/* boost affinity */
	int boost_affinity_by_pid;

	/* boost LR */
	int boost_lr_by_pid;

	/* separate cap */
	int separate_aa_by_pid;
	int limit_uclamp_by_pid;
	int limit_ruclamp_by_pid;
	int limit_uclamp_m_by_pid;
	int limit_ruclamp_m_by_pid;
	int separate_pct_b_by_pid;
	int separate_pct_m_by_pid;
	int separate_release_sec_by_pid;

	/* blc boost*/
	int blc_boost_by_pid;

	/* QUOTA */
	int qr_enable_by_pid;
	int qr_t2wnt_x_by_pid;
	int qr_t2wnt_y_p_by_pid;
	int qr_t2wnt_y_n_by_pid;

	/*  GCC   */
	int gcc_enable_by_pid;
	int gcc_fps_margin_by_pid;
	int gcc_up_sec_pct_by_pid;
	int gcc_down_sec_pct_by_pid;
	int gcc_up_step_by_pid;
	int gcc_down_step_by_pid;
	int gcc_reserved_up_quota_pct_by_pid;
	int gcc_reserved_down_quota_pct_by_pid;
	int gcc_enq_bound_thrs_by_pid;
	int gcc_deq_bound_thrs_by_pid;
	int gcc_enq_bound_quota_by_pid;
	int gcc_deq_bound_quota_by_pid;

	/* Reset taskmask */
	int reset_taskmask;
};

struct render_info {
	struct rb_node render_key_node;
	struct list_head bufferid_list;
	struct rb_node linger_node;

	/*render basic info pid bufferId..etc*/
	int pid;
	struct fbt_render_key render_key; /*pid,identifier*/
	unsigned long long identifier;
	unsigned long long buffer_id;
	int queue_SF;
	int tgid;	/*render's process pid*/
	int api;	/*connected API*/
	int frame_type;
	int hwui;
	int sbe_control_flag;
	int control_pid_flag;
	unsigned long long render_last_cb_ts;

	/*render queue/dequeue/frame time info*/
	unsigned long long t_enqueue_start;
	unsigned long long t_enqueue_end;
	unsigned long long t_dequeue_start;
	unsigned long long t_dequeue_end;
	unsigned long long enqueue_length;
	unsigned long long enqueue_length_real;
	unsigned long long dequeue_length;
	unsigned long long Q2Q_time;
	unsigned long long running_time;

	/*fbt*/
	int linger;
	struct fbt_boost_info boost_info;
	struct fbt_thread_blc *p_blc;
	struct fpsgo_loading *dep_arr;
	int dep_valid_size;
	unsigned long long dep_loading_ts;
	unsigned long long linger_ts;
	int avg_freq;

	struct mutex thr_mlock;

	/* boost policy */
	struct fpsgo_boost_attr attr;
};

#if FPSGO_MW
struct fpsgo_attr_by_pid {
	struct rb_node entry;
	int tgid;
	unsigned long long ts;
	struct fpsgo_boost_attr attr;
};
#endif

struct BQ_id {
	struct fbt_render_key key;
	unsigned long long identifier;
	unsigned long long buffer_id;
	int queue_SF;
	int pid;
	int queue_pid;
	struct rb_node entry;
};

struct hwui_info {
	int pid;
	struct rb_node entry;
};

struct sbe_info {
	int pid;
	struct rb_node entry;
};

struct fps_control_pid_info {
	int pid;
	struct rb_node entry;
};

struct video_info {
	int pid;
	unsigned int count_instance;
	struct rb_node entry;
};

struct gbe_runtime {
	int pid;
	unsigned long long runtime;
	unsigned long long loading;
};

struct cam_dep_thread {
	int pid;
	struct rb_node rb_node;
};

#ifdef FPSGO_DEBUG
#define FPSGO_LOGI(...)	pr_debug("FPSGO:" __VA_ARGS__)
#else
#define FPSGO_LOGI(...)
#endif
#define FPSGO_LOGE(...)	pr_debug("FPSGO:" __VA_ARGS__)
#define FPSGO_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

long long fpsgo_task_sched_runtime(struct task_struct *p);
long fpsgo_sched_setaffinity(pid_t pid, const struct cpumask *in_mask);
void *fpsgo_alloc_atomic(int i32Size);
void fpsgo_free(void *pvBuf, int i32Size);
unsigned long long fpsgo_get_time(void);
int fpsgo_arch_nr_clusters(void);
int fpsgo_arch_nr_get_opp_cpu(int cpu);
int fpsgo_arch_nr_get_cap_cpu(int cpu, int opp);
int fpsgo_arch_nr_max_opp_cpu(void);
int fpsgo_arch_nr_freq_cpu(void);
unsigned int fpsgo_cpufreq_get_freq_by_idx(
	int cpu, unsigned int opp);

int fpsgo_get_tgid(int pid);
void fpsgo_render_tree_lock(const char *tag);
void fpsgo_render_tree_unlock(const char *tag);
void fpsgo_thread_lock(struct mutex *mlock);
void fpsgo_thread_unlock(struct mutex *mlock);
void fpsgo_lockprove(const char *tag);
void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock);
#if FPSGO_MW
void fpsgo_clear_llf_cpu_policy_by_pid(int tgid);
struct fpsgo_attr_by_pid *fpsgo_find_attr_by_pid(int pid, int add_new);
void delete_attr_by_pid(int tgid);
void fpsgo_reset_render_pid_attr(int tgid);
int is_to_delete_fpsgo_attr(struct fpsgo_attr_by_pid *fpsgo_attr);
#endif  // FPSGO_MW
struct render_info *eara2fpsgo_search_render_info(int pid,
		unsigned long long buffer_id);
void fpsgo_delete_render_info(int pid,
	unsigned long long buffer_id, unsigned long long identifier);
struct render_info *fpsgo_search_and_add_render_info(int pid,
		unsigned long long identifier, int force);
struct hwui_info *fpsgo_search_and_add_hwui_info(int pid, int force);
void fpsgo_delete_hwui_info(int pid);
struct sbe_info *fpsgo_search_and_add_sbe_info(int pid, int force);
void fpsgo_delete_sbe_info(int pid);
struct fps_control_pid_info *fpsgo_search_and_add_fps_control_pid(int pid, int force);
void fpsgo_delete_fpsgo_control_pid(int pid);
void fpsgo_check_thread_status(void);
void fpsgo_clear(void);
struct BQ_id *fpsgo_find_BQ_id(int pid, int tgid, long long identifier,
		int action);
int fpsgo_get_BQid_pair(int pid, int tgid, long long identifier,
		unsigned long long *buffer_id, int *queue_SF, int enqueue);
void fpsgo_main_trace(const char *fmt, ...);
void fpsgo_clear_uclamp_boost(void);
void fpsgo_clear_llf_cpu_policy(void);
void fpsgo_del_linger(struct render_info *thr);
int fpsgo_base_is_finished(struct render_info *thr);
int fpsgo_update_swap_buffer(int pid);
void fpsgo_sentcmd(int cmd, int value1, int value2);
void fpsgo_ctrl2base_get_pwr_cmd(int *cmd, int *value1, int *value2);
int fpsgo_sbe_rescue_traverse(int pid, int start, int enhance, unsigned long long frame_id);
void fpsgo_stop_boost_by_pid(int pid);
void fpsgo_stop_boost_by_render(struct render_info *r);

int init_fpsgo_common(void);

enum FPSGO_FRAME_TYPE {
	NON_VSYNC_ALIGNED_TYPE = 0,
	BY_PASS_TYPE = 1,
};

enum FPSGO_CONNECT_API {
	WINDOW_DISCONNECT = 0,
	NATIVE_WINDOW_API_EGL = 1,
	NATIVE_WINDOW_API_CPU = 2,
	NATIVE_WINDOW_API_MEDIA = 3,
	NATIVE_WINDOW_API_CAMERA = 4,
};

enum FPSGO_FORCE {
	FPSGO_FORCE_OFF = 0,
	FPSGO_FORCE_ON = 1,
	FPSGO_FREE = 2,
};

enum FPSGO_BQID_ACT {
	ACTION_FIND = 0,
	ACTION_FIND_ADD,
	ACTION_FIND_DEL,
};

enum FPSGO_RENDER_INFO_HWUI {
	RENDER_INFO_HWUI_UNKNOWN = 0,
	RENDER_INFO_HWUI_TYPE = 1,
	RENDER_INFO_HWUI_NONE = 2,
};

#endif

