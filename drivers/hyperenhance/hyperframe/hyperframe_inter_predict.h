// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_INTER_FRAMEPREDICT_H
#define _HYPERFRAME_INTER_FRAMEPREDICT_H

#include "hyperframe_base.h"

extern struct workqueue_struct *hyperframe_notify_wq;

void hyperframe_inter_doframe_predict(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
void hyperframe_inter_recordview_predict(unsigned int pid, long long vsync_id, unsigned int start, unsigned long long time);
enum hrtimer_restart  hyperframe_inter_timer_cb(struct hrtimer *timer);
int hyperframe_inter_init(void);
int hyperframe_inter_exit(void);

#endif
// END Performance_FramePredictBoost