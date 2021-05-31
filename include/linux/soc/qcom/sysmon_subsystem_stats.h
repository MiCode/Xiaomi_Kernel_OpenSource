/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

/*
 * This header is for sysmon subsystem stats query API's in drivers.
 */

#ifndef __QCOM_SYSMON_SUBSYSTEM_STATS_H__
#define __QCOM_SYSMON_SUBSYSTEM_STATS_H__

/*
 * @struct sysmon_smem_power_stats_t
 * @brief Structure type to hold DSP power statistics.
 */
struct sysmon_smem_power_stats {
	u32 clk_arr[8];
	/**< Core clock frequency(KHz) array */

	u32 active_time[8];
	/**< Active time(seconds) array corresponding to core clock array */

	u32 pc_time;
	/**< DSP LPM(Low Power Mode) time(seconds) */

	u32 lpi_time;
	/**< DSP LPI(Low Power Island Mode) time(seconds) */
};

/*
 * @enum dsp_id_t
 * @brief Enum to hold SMEM HOST ID for DSP subsystems.
 */
enum dsp_id_t {
	ADSP = 2,
	SLPI,
	CDSP = 5
};

/**
 * API to query requested DSP subsystem power residency.
 * On success, returns power residency statistics in the given
 * sysmon_smem_power_stats structure.
 * @arg: DSP_ID
.*			2 - ADSP
 *			3 - SLPI
 *			5 - CDSP
 *sysmon_smem_power_stats
 *			u32 clk_arr[8];
 *			u32 active_time[8];
 *			u32 pc_time;
 *			u32 lpi_time;
 *@return SUCCESS (0) if Query is succssful
 *        FAILURE (Non-zero) if Query could not be processed.
 *
 */

int sysmon_stats_query_power_residency(enum dsp_id_t dsp_id,
		struct sysmon_smem_power_stats *sysmon_power_stats);

#endif
