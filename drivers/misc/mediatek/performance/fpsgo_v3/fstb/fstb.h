/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef FSTB_H
#define FSTB_H

int mtk_fstb_exit(void);
int mtk_fstb_init(void);
void fpsgo_comp2fstb_queue_time_update(
	int pid, unsigned long long bufID, int frame_type,
	unsigned long long ts,
	int api, int hwui_flag, int video_flag);
void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid, unsigned long long bufID,
	unsigned long long cur_dequeue_start_ts, unsigned long long cur_queue_end_ts);
int fpsgo_ctrl2fstb_gblock(int tid, int start);
void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps);
int fpsgo_ctrl2fstb_wait_fstb_active(void);
void fpsgo_comp2fstb_camera_active(int pid);

#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
int is_fstb_enable(void);
int is_fstb_active(long long time_diff);
int fpsgo_ctrl2fstb_switch_fstb(int value);
int switch_sample_window(long long time_usec);
int switch_fps_range(int nr_level, struct fps_level *level);
int switch_process_fps_range(char *proc_name,
	int nr_level, struct fps_level *level);
int switch_thread_fps_range(pid_t pid, int nr_level, struct fps_level *level);
int switch_dfps_ceiling(int fps);
int switch_fps_error_threhosld(int threshold);
int switch_percentile_frametime(int ratio);
int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid,
	unsigned long long bufID,
	int tgid,
	int frame_type,
	unsigned long long Q2Q_time,
	long long Runnging_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned long long mid,
	unsigned long long enqueue_length,
	unsigned long long dequeue_length);
void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_cpu_time, int *fps_margin,
		int tgid, unsigned long long mid, int *quantile_cpu_time,
		int *quantile_gpu_time, int *target_fpks, int *cooler_on);
void fpsgo_ctrl2fstb_dfrc_fps(int dfrc_fps);
int fpsgo_fbt2fstb_get_cam_active(void);
void fstb_set_cam_active_fpsgo_off(int active);

/* EARA */
void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rftp, int *hwui, char name[][16]);
void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
				int tfps);

/* Video RB tree */
struct video_info *fstb_search_and_add_video_info(int pid, int add_node);
void fstb_delete_video_info(int pid);
void fstb_set_video_pid(int pid);
void fstb_clear_video_pid(int pid);
void fpsgo_video_pid_tree_lock(const char *tag);
void fpsgo_video_pid_tree_unlock(const char *tag);

#else
static inline int is_fstb_enable(void) { return 0; }
static inline int fpsgo_ctrl2fstb_switch_fstb(int en) { return 0; }
static inline int switch_sample_window(long long time_usec) { return 0; }
static inline int switch_fps_range(int nr_level,
	struct fps_level *level) { return 0; }
static inline int switch_process_fps_range(char *proc_name,
	int nr_level, struct fps_level *level) { return 0; }
static inline int switch_thread_fps_range(pid_t pid,
	int nr_level, struct fps_level *level) { return 0; }
static inline int switch_dfps_ceiling(int fps) { return 0; }
static inline int switch_fps_error_threhosld(int threshold) { return 0; }
static inline int switch_percentile_frametime(int ratio) { return 0; }
static inline int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid,
	unsigned long long bufID,
	int tgid,
	int frame_type,
	unsigned long long Q2Q_time,
	long long Runnging_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned long long mid) { return 0; }
static inline void fpsgo_fbt2fstb_query_fps(int pid,
		int *target_fps, int *target_cpu_time, int *fps_margin,
		int tgid, unsigned long long mid, int *quantile_cpu_time,
		int *quantile_gpu_time, int *target_fpks, int *cooler_on) { }
static void fpsgo_ctrl2fstb_dfrc_fps(int dfrc_fps) { }
static int fpsgo_fbt2fstb_get_cam_active(void) { }
void fstb_set_cam_active_fpsgo_off(int active) { }

/* EARA */
static inline void eara2fstb_get_tfps(int max_cnt, int *pid,
		unsigned long long *buf_id, int *tfps, int *hwui,
		char name[][16]) { }
static inline void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id,
		int diff, int tfps) { }

/* Video rb-tree */
struct video_info *fstb_search_and_add_video_info(int pid, int add_node) { return NULL; }
void fstb_delete_video_info(int pid) { }
void fstb_set_video_pid(int pid) { }
void fstb_clear_video_pid(int pid) { }
void fpsgo_video_pid_tree_lock(const char *tag) {}
void fpsgo_video_pid_tree_unlock(const char *tag) {}

#endif

#endif

