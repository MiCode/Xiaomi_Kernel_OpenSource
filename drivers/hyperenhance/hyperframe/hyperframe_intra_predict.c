// MIUI ADD: Performance_FramePredictBoost
#include "hyperframe.h"
#include "hyperframe_energy_sched.h"
#include "hyperframe_intra_predict.h"

#define PREDICT_WINDOW_SIZE 20

uint64* loadings;

int hyperframe_intra_predict(int pid)
{
	int ret = -1;
	struct frame_info* frame = NULL;
	uint64 target_loading = 0;
	int target_capacity = 0;

	t_time target_frame_time = 0;

	frame = get_frame_info(pid);

	intra_predict_cp_loading(loadings, frame, PREDICT_WINDOW_SIZE);

	target_loading = hyperframe_target_loading(loadings, PREDICT_WINDOW_SIZE);

	htrace_b_predict_debug(current->tgid, "HYPERFRAME#INTRA#get_vsync_time");
	target_frame_time = nsec_to_100usec(get_vsync_time());
	htrace_e_predict_debug();

	if (target_frame_time == 0)
		target_frame_time = 166;

	target_capacity = target_loading / target_frame_time;

	htrace_b_predict_debug(current->tgid, "HYPERFRAME#INTRA#hyperframe_set_capacity_qos");
	ret = hyperframe_set_capacity_qos(pid, target_capacity);
	htrace_e_predict_debug();

	htrace_b_predict(current->tgid,
			"[HYPERFRAME#INTRA] target_time: %d target_capacity - %d",
			target_frame_time, target_capacity);
	htrace_e_predict();

	htrace_c_sched(pid, ret, "sched-result");

	return ret;
}

size_t intra_predict_cp_loading(uint64 loadings_arr[], struct frame_info* frame, size_t size)
{
	int i;
	int idx;
	int ret = 0;

	if (frame == NULL) {
		ret = 1;
		goto intra_predict_cp_loading_out;
	}

	for (i = 0, idx = frame->latest_idx; i < size; i++, idx--) {
		if (idx < 0)
			idx = WINDOW_LENGTH - 1;

		loadings_arr[i] = frame->ui_loading[idx] + frame->render_loading[idx];
	}

intra_predict_cp_loading_out:
	return ret;
}

uint64 calculate_EMA_for_loading(uint64 loadings_arr[], size_t size)
{
	int i;
	uint64 ema = loadings_arr[0];

	for (i = 1; i < size; i++)
		ema = ((loadings_arr[i] + (loadings_arr[i] << 1)) >> 3) + ((ema + (ema << 2)) >> 3);

	return ema;
}

uint64 hyperframe_target_loading(uint64 loadings_arr[], size_t size)
{
	return calculate_EMA_for_loading(loadings_arr, size);
}

int hyperframe_target_capcity(int target_loading, int target_fps)
{
	int next_capcity = 0;

	next_capcity = target_loading / target_fps;

	return next_capcity;
}

int hyperframe_intra_init(void)
{
	loadings = kcalloc(PREDICT_WINDOW_SIZE, sizeof(uint64), GFP_KERNEL);
	return 0;
}

int hyperframe_intra_exit(void)
{
	kfree(loadings);
	return 0;
}
// END Performance_FramePredictBoost