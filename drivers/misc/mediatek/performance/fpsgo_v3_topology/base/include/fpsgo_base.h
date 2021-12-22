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

#define WINDOW 20
#define RESCUE_TIMER_NUM 5
#define QUOTA_MAX_SIZE 300
#define GCC_MAX_SIZE 300

/* EARA job type */
enum HW_EVENT4RENDER {
	PER_FRAME = 0,
	CRO_FRAME = 10,
	BACKGROUND = 20
};

struct fbt_jerk {
	int id;
	int jerking;
	int postpone;
	struct hrtimer timer;
	struct work_struct work;
};
struct fbt_proc {
	int active_jerk_id;
	struct fbt_jerk jerks[RESCUE_TIMER_NUM];
};

struct fbt_frame_info {
	int target_fps;
	int mips_diff;
	long mips;
	unsigned long long running_time;
	int count;
};

struct fbt_thread_loading {
	int pid;
	unsigned long long buffer_id;
	atomic_t loading;
	atomic_t *loading_cl;
	atomic_t last_cb_ts;
	struct list_head entry;
};

struct fbt_thread_blc {
	int pid;
	unsigned long long buffer_id;
	unsigned int blc;
	struct list_head entry;
};

struct fbt_boost_info {
	int target_fps;
	unsigned long long target_time;
	unsigned int last_blc;

	/* adjust loading */
	int loading_weight;
	int weight_cnt;
	int hit_cnt;
	int deb_cnt;
	int hit_cluster;

	/* rescue*/
	struct fbt_proc proc;
	int cur_stage;

	/* variance control */
	struct fbt_frame_info frame_info[WINDOW];
	unsigned int floor;
	int floor_count;
	int reset_floor_bound;
	int f_iter;

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

	/* GCC */
	int gcc_quota;
	int gcc_count;
	int gcc_target_fps;
	int correction;
	int gcc_pct_thrs;
	int gcc_avg_pct;
	unsigned long long gcc_pct_reset_ts;
	int quantile_cpu_time;
	int quantile_gpu_time;

};

struct uboost {
	unsigned long long vsync_u_runtime;
	unsigned long long checkp_u_runtime;
	unsigned long long timer_period;
	int uboosting;
	struct hrtimer timer;
	struct work_struct work;
};

struct render_info {
	struct rb_node render_key_node;
	struct list_head bufferid_list;
	struct rb_node linger_node;

	/*render basic info pid bufferId..etc*/
	int pid;
	unsigned long long render_key; /*pid,identifier*/
	unsigned long long identifier;
	unsigned long long buffer_id;
	int queue_SF;
	int tgid;	/*render's process pid*/
	int api;	/*connected API*/
	int frame_type;
	int hwui;
	int ux;

	/*render queue/dequeue/frame time info*/
	unsigned long long t_enqueue_start;
	unsigned long long t_enqueue_end;
	unsigned long long t_dequeue_start;
	unsigned long long t_dequeue_end;
	unsigned long long enqueue_length;
	unsigned long long dequeue_length;
	unsigned long long Q2Q_time;
	unsigned long long running_time;

	/*fbt*/
	int linger;
	struct fbt_boost_info boost_info;
	struct fbt_thread_loading *pLoading;
	struct fbt_thread_blc *p_blc;
	struct fpsgo_loading *dep_arr;
	int dep_valid_size;
	unsigned long long dep_loading_ts;
	unsigned long long linger_ts;
	long long last_sched_runtime;

	/*TODO: EARA mid list*/
	unsigned long long mid;

	/*uboost*/
	struct uboost uboost_info;

	struct mutex thr_mlock;
};

struct BQ_id {
	unsigned long long key;
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

struct fpsgo_loading {
	int pid;
	int loading;
	int prefer_type;
	int policy;
	long nice_bk;
};

struct gbe_runtime {
	int pid;
	unsigned long long runtime;
	unsigned long long loading;
};

#ifdef FPSGO_DEBUG
#define FPSGO_LOGI(...)	pr_debug("FPSGO:" __VA_ARGS__)
#else
#define FPSGO_LOGI(...)
#endif
#define FPSGO_LOGE(...)	pr_debug("FPSGO:" __VA_ARGS__)
#define FPSGO_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

void *fpsgo_alloc_atomic(int i32Size);
void fpsgo_free(void *pvBuf, int i32Size);
unsigned long long fpsgo_get_time(void);

int fpsgo_get_tgid(int pid);
void fpsgo_render_tree_lock(const char *tag);
void fpsgo_render_tree_unlock(const char *tag);
void fpsgo_thread_lock(struct mutex *mlock);
void fpsgo_thread_unlock(struct mutex *mlock);
void fpsgo_lockprove(const char *tag);
void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock);
void fpsgo_delete_render_info(int pid,
	unsigned long long buffer_id, unsigned long long identifier);
struct render_info *fpsgo_search_and_add_render_info(int pid,
		unsigned long long identifier, int force);
struct hwui_info *fpsgo_search_and_add_hwui_info(int pid, int force);
void fpsgo_delete_hwui_info(int pid);
int fpsgo_has_bypass(void);
void fpsgo_check_thread_status(void);
void fpsgo_clear(void);
struct BQ_id *fpsgo_find_BQ_id(int pid, int tgid, long long identifier,
		int action);
int fpsgo_get_BQid_pair(int pid, int tgid, long long identifier,
		unsigned long long *buffer_id, int *queue_SF, int enqueue);
void fpsgo_main_trace(const char *fmt, ...);
void fpsgo_clear_uclamp_boost(void);
void fpsgo_clear_llf_cpu_policy(int orig_llf);
void fpsgo_del_linger(struct render_info *thr);
int fpsgo_uboost_traverse(unsigned long long ts);
int fpsgo_base_is_finished(struct render_info *thr);
int fpsgo_update_swap_buffer(int pid);

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
	ACTION_DEL_PID
};

enum FPSGO_RENDER_INFO_HWUI {
	RENDER_INFO_HWUI_UNKNOWN = 0,
	RENDER_INFO_HWUI_TYPE = 1,
	RENDER_INFO_HWUI_NONE = 2,
};

#endif

