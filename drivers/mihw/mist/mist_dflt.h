// MIUI ADD: Performance_TurboSched
#ifndef MIST_DFLT_H
#define MIST_DFLT_H

#include <soc/qcom/dcvs.h>
#include <soc/qcom/pmu_lib.h>

enum mist_ev_idx {
	FE_STALL_IDX,
	BE_STALL_IDX,
	CYC_EV_IDX,
	NUM_DFLT_EVS
};

enum dflt_hw_type {
	DFLT_DDR,
	DFLT_LLCC,
	DFLT_L3,
	NUM_DFLT_HW_TYPES
};

enum cluster_freq_map {
	SILVER_FREQ_MAP=0,
	GOLD_FREQ_MAP,
	PRIME_FREQ_MAP,
	NUM_CLUSTER_FREQ_MAPS
};

#endif
// END Performance_TurboSched