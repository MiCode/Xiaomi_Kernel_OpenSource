// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_BASE_H_
#define _HYPERFRAME_BASE_H_

#define WINDOW_LENGTH 100

#define HYPERFRAME_SYSFS_MAX_BUFF_SIZE 1024

typedef unsigned long long t_time;
typedef unsigned long long uint64;

struct timepair {
		t_time t_start;
		t_time t_end;
};

// App type
enum {
	APP_TYPE_UNKNOWN = 0,
	APP_TYPE_NON_FOCUS,
	APP_TYPE_FOCUS,
	APP_TYPE_SF,
};

// time type
enum {
	TIME_UNKNOWN = 0,
	TIME_DOFRAME,
	TIME_RECOREVIEW,
	TIME_DRAWFRAME,
	TIME_DEQUEUE,
	TIME_QUEUE,
	TIME_GPU,
};

struct frame_info {
	struct hlist_node hlist;
	int pid;
	int render_tid;
	int gpu_tid;

	int latest_idx;
	int start_idx;
	int vsync_id_arr[WINDOW_LENGTH];
	int record_flag[WINDOW_LENGTH];
	struct timepair t_doframe[WINDOW_LENGTH];
	struct timepair t_recordview[WINDOW_LENGTH];
	struct timepair t_drawframes[WINDOW_LENGTH];
	struct timepair t_dequeue[WINDOW_LENGTH];
	struct timepair t_queue[WINDOW_LENGTH];
	struct timepair t_gpu[WINDOW_LENGTH];
	t_time ts_latest_frame;

	uint64 ui_loading[WINDOW_LENGTH];
	uint64 render_loading[WINDOW_LENGTH];
	uint64 ui_running_time[WINDOW_LENGTH];
	uint64 render_running_time[WINDOW_LENGTH];

	bool is_input_focused;
	bool latest_focused_idx;
};

struct frame_thread_id {
	unsigned int pid;
	unsigned int tid_render;
};

void* hyperframe_alloc_atomic(int i32Size);
void hyperframe_free(void *pvBuf, int i32Size);

#endif
// END Performance_FramePredictBoost