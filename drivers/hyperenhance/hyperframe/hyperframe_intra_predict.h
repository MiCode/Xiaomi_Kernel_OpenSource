// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_INTRA_PREDICT_H
#define _HYPERFRAME_INTRA_PREDICT_H

#include "hyperframe_frame_info.h"

int hyperframe_intra_predict(int pid);
size_t intra_predict_cp_loading(uint64 loadings_arr[], struct frame_info* frame, size_t size);
uint64 hyperframe_target_loading(uint64 loadings_arr[], size_t size);
int hyperframe_target_capcity(int target_loading, int target_fps);
int hyperframe_intra_init(void);
int hyperframe_intra_exit(void);

#endif
// END Performance_FramePredictBoost