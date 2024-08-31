// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_CPU_LOADING_TRACK_H
#define _HYPERFRAME_CPU_LOADING_TRACK_H

#include "hyperframe_energy_sched.h"
#include "hyperframe_base.h"
#include "hyperframe_frame_info.h"

static int hf_cluster_num;
extern int hf_cluster_num;

struct hyperframe_cpu_freq
{
	int cluster;
	int freq;
	t_time ts;
};

struct hyperframe_sched_switch
{
	int cid;
	int pid;
	t_time ts;
	bool running;
};

void hyperframe_cpu_loading_track_init(void);
void hyperframe_cpu_loading_track_exit(void);

void set_ui_and_renderthread(int ui_thread, int render_thread, int is_focus);
void start_cpu_tracer(void);
void end_cpu_tracer(void);

void add_freq_data(int cluster, int freq);
void add_sched_switch(unsigned int cid, struct task_struct *prev, struct task_struct *next);

void notify_frame_loading_update(int pid, int vsync_id);
int calculate_frame_loading(int pid, struct frame_info* frame, int vsync_id);
int hyperframe_find_freq_add_loading(int cluster, t_time start_time, t_time end_time, uint64* loading);

#endif
// END Performance_FramePredictBoost