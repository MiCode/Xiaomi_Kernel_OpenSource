// MIUI ADD: Performance_TurboSched
#ifndef MIST_GFLT_H
#define MIST_GFLT_H

struct gflt_load_info {
	u64 total_time;
	u64 busy_time;
	u64 ram_time;
	u64 ram_wait;
};

int mi_get_gflt_enable(void);

#endif
// END Performance_TurboSched
