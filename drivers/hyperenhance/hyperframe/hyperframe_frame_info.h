// MIUI ADD: Performance_FramePredictBoost
#ifndef HYPERFRAME_FRAME_INFO_H
#define HYPERFRAME_FRAME_INFO_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "hyperframe_base.h"
#include "hyperframe_utils.h"

size_t hyperframe_update_vsync_period(t_time time);
ssize_t hyperframe_update_time(int pid, int vsync_id, t_time time, int type, bool started);
struct frame_info* get_frame_info(int pid);
int get_frame_idx_by_vsyncid(struct frame_info* frame, int vsync_id);
int get_frame_idx_by_vsyncid_inter(struct frame_info* frame, int vsync_id);
int hyperframe_get_target_fps(void);
size_t hyperframe_remove_info_inter(int pid);

size_t update_recent_queue(int pid, int tid_render);
int find_recent_idx(int pid);

size_t hyperframe_frameinfo_init(void);
size_t check_is_latest_thread_id(int thread_id);

#endif
// END Performance_FramePredictBoost