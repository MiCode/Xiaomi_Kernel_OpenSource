// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/sysmon_subsystem_stats.h>

#define SYSMON_SMEM_ID			634
#define SLEEPSTATS_SMEM_ID_ADSP			606
#define SLEEPSTATS_SMEM_ID_CDSP			607
#define SLEEPSTATS_SMEM_ID_SLPI			608
#define SLEEPSTATS_LPI_SMEM_ID			613
#define SYS_CLK_TICKS_PER_MS		19200

struct sysmon_smem_stats {
	bool smem_init_adsp;
	bool smem_init_cdsp;
	bool smem_init_slpi;
	struct sysmon_smem_power_stats *sysmon_power_stats_adsp;
	struct sysmon_smem_power_stats *sysmon_power_stats_cdsp;
	struct sysmon_smem_power_stats *sysmon_power_stats_slpi;
	struct sysmon_smem_q6_event_stats *sysmon_event_stats_adsp;
	struct sysmon_smem_q6_event_stats *sysmon_event_stats_cdsp;
	struct sysmon_smem_q6_event_stats *sysmon_event_stats_slpi;
	struct sleep_stats *sleep_stats_adsp;
	struct sleep_stats *sleep_stats_cdsp;
	struct sleep_stats *sleep_stats_slpi;
	struct sleep_stats_island *sleep_lpi_adsp;
	struct sleep_stats_island *sleep_lpi_slpi;
	u32 *q6_avg_load_adsp;
	u32 *q6_avg_load_cdsp;
	u32 *q6_avg_load_slpi;
};

enum feature_id {
	SYSMONSTATS_Q6_LOAD_FEATUREID = 1,
	SYSMONSTATS_Q6_EVENT_FEATUREID,
	SYSMON_POWER_STATS_FEATUREID
};

struct sysmon_smem_q6_load_stats {
	u32 q6load_avg;
	u32 Last_update_time_load_lsb;
	u32 Last_update_time_load_msb;
};

static struct sysmon_smem_stats g_sysmon_stats;

/*Updates SMEM pointers for all the sysmon Master stats*/
static void update_sysmon_smem_pointers(void *smem_pointer, enum dsp_id_t dsp_id, size_t size)
{
	u32 featureId;
	int feature, size_rcvd;
	int size_of_u32 = sizeof(u32);

	featureId = *(unsigned int *)smem_pointer;
	feature = featureId >> 28;
	size_rcvd = (featureId >> 16) & 0xFFF;

	while ((size > 0) && (size >= size_rcvd)) {
		switch (feature) {
		case SYSMON_POWER_STATS_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.sysmon_power_stats_adsp =
					(struct sysmon_smem_power_stats *)
					(smem_pointer + size_of_u32);
				else if (dsp_id == CDSP)
					g_sysmon_stats.sysmon_power_stats_cdsp =
					(struct sysmon_smem_power_stats *)
					(smem_pointer + size_of_u32);
				else if (dsp_id == SLPI)
					g_sysmon_stats.sysmon_power_stats_slpi =
					(struct sysmon_smem_power_stats *)
					(smem_pointer + size_of_u32);
				else
					pr_err("%s: subsystem not found %d\n",
						__func__, SYSMON_POWER_STATS_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMON_POWER_STATS_FEATUREID);
				size = 0;
			}
		break;
		case SYSMONSTATS_Q6_EVENT_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.sysmon_event_stats_adsp =
					(struct sysmon_smem_q6_event_stats *)
					(smem_pointer + size_of_u32);
				else if (dsp_id == CDSP)
					g_sysmon_stats.sysmon_event_stats_cdsp =
					(struct sysmon_smem_q6_event_stats *)
					(smem_pointer + size_of_u32);
				else if (dsp_id == SLPI)
					g_sysmon_stats.sysmon_event_stats_slpi =
					(struct sysmon_smem_q6_event_stats *)
					(smem_pointer + size_of_u32);
				else
					pr_err("%s:subsystem not found %d\n",
					__func__, SYSMONSTATS_Q6_EVENT_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMONSTATS_Q6_EVENT_FEATUREID);
				size = 0;
			}
		break;
		case SYSMONSTATS_Q6_LOAD_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.q6_avg_load_adsp =
					(u32 *)(smem_pointer + size_of_u32);
				else if (dsp_id == CDSP)
					g_sysmon_stats.q6_avg_load_cdsp =
					(u32 *)(smem_pointer + size_of_u32);
				else if (dsp_id == SLPI)
					g_sysmon_stats.q6_avg_load_slpi =
					(u32 *)(smem_pointer + size_of_u32);
				else
					pr_err("%s:subsystem not found %d\n",
						__func__, SYSMONSTATS_Q6_LOAD_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMONSTATS_Q6_LOAD_FEATUREID);
				size = 0;
			}
		break;
		default:
			pr_err("%s: Requested feature not found\n", __func__);
		break;
		}
		if (!IS_ERR_OR_NULL(smem_pointer + size_rcvd)
					&& (size > size_rcvd)) {
			featureId = *(unsigned int *)(smem_pointer + size_rcvd);
			smem_pointer += size_rcvd;
			size = size - size_rcvd;
			feature = featureId >> 28;
			size_rcvd = (featureId >> 16) & 0xFFF;
		} else {
			size = 0;
		}
	}
}

static void sysmon_smem_init_adsp(void)
{
	size_t size;
	void *smem_pointer_adsp = NULL;

	g_sysmon_stats.smem_init_adsp = true;

	g_sysmon_stats.sleep_stats_adsp = qcom_smem_get(ADSP,
						SLEEPSTATS_SMEM_ID_ADSP,
						NULL);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_adsp)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for ADSP: %d\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}

	g_sysmon_stats.sleep_lpi_adsp = qcom_smem_get(ADSP,
						SLEEPSTATS_LPI_SMEM_ID,
						NULL);
	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_adsp)) {
		pr_err("%s:Failed to get fetch LPI sleep data from SMEM for ADSP: %d\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_lpi_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}

	smem_pointer_adsp = qcom_smem_get(ADSP,
						SYSMON_SMEM_ID,
						&size);

	if (IS_ERR_OR_NULL(smem_pointer_adsp) || !size) {
		pr_err("%s:Failed to get fetch sysmon data from SMEM for ADSP: %d\n",
				__func__, PTR_ERR(smem_pointer_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}

	update_sysmon_smem_pointers(smem_pointer_adsp, ADSP, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"event stats:%x\n",
				__func__, g_sysmon_stats.sysmon_event_stats_adsp);
		g_sysmon_stats.smem_init_adsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"power stats: %x\n",
				__func__, g_sysmon_stats.sysmon_power_stats_adsp);
		g_sysmon_stats.smem_init_adsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"q6_avg_load: %x\n",
				__func__, g_sysmon_stats.q6_avg_load_adsp);
		g_sysmon_stats.smem_init_adsp = false;
	}
}

static void sysmon_smem_init_cdsp(void)
{
	size_t size;
	void *smem_pointer_cdsp = NULL;

	g_sysmon_stats.smem_init_cdsp = true;

	g_sysmon_stats.sleep_stats_cdsp = qcom_smem_get(CDSP,
						SLEEPSTATS_SMEM_ID_CDSP,
						NULL);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_cdsp)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for CDSP: %d\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_cdsp));
		g_sysmon_stats.smem_init_cdsp = false;
	}

	smem_pointer_cdsp = qcom_smem_get(CDSP,
							SYSMON_SMEM_ID,
							&size);
	if (IS_ERR_OR_NULL(smem_pointer_cdsp) || !size) {
		pr_err("%s:Failed to get fetch data from SMEM for CDSP: %d\n",
				__func__, PTR_ERR(smem_pointer_cdsp));
		g_sysmon_stats.smem_init_cdsp = false;
	}

	update_sysmon_smem_pointers(smem_pointer_cdsp, CDSP, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"event stats:%x\n",
				__func__, g_sysmon_stats.sysmon_event_stats_cdsp);
		g_sysmon_stats.smem_init_cdsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				" power stats: %x\n",
				__func__, g_sysmon_stats.sysmon_power_stats_cdsp);
		g_sysmon_stats.smem_init_cdsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"q6_avg_load: %x\n",
				__func__, g_sysmon_stats.q6_avg_load_cdsp);
		g_sysmon_stats.smem_init_cdsp = false;
	}
}
static void sysmon_smem_init_slpi(void)
{
	size_t size;
	void *smem_pointer_slpi = NULL;

	g_sysmon_stats.smem_init_slpi = true;

	g_sysmon_stats.sleep_stats_slpi = qcom_smem_get(SLPI,
						SLEEPSTATS_SMEM_ID_SLPI,
						NULL);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_slpi)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for SLPI: %d\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_slpi));
		g_sysmon_stats.smem_init_slpi = false;
	}

	g_sysmon_stats.sleep_lpi_slpi = qcom_smem_get(SLPI,
						SLEEPSTATS_LPI_SMEM_ID,
						NULL);
	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_slpi)) {
		pr_err("%s:Failed to get fetch LPI sleep data from SMEM for SLPI: %d\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_lpi_slpi));
		g_sysmon_stats.smem_init_slpi = false;
	}

	smem_pointer_slpi = qcom_smem_get(SLPI,
							SYSMON_SMEM_ID,
							&size);

	if (IS_ERR_OR_NULL(smem_pointer_slpi) || !size) {
		pr_err("%s:Failed to get fetch data from SMEM for SLPI: %d\n",
				__func__, PTR_ERR(smem_pointer_slpi));
	}

	update_sysmon_smem_pointers(smem_pointer_slpi, SLPI, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"event stats:%x\n",
				__func__, g_sysmon_stats.sysmon_event_stats_slpi);
		g_sysmon_stats.smem_init_slpi = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"power stats: %x%x\n",
				__func__, g_sysmon_stats.sysmon_power_stats_slpi);
		g_sysmon_stats.smem_init_slpi = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"q6_avg_load: %x\n",
				__func__, g_sysmon_stats.q6_avg_load_slpi);
		g_sysmon_stats.smem_init_slpi = false;
	}
}
/**
 * sysmon_stats_query_power_residency() - * API to query requested
 * DSP subsystem power residency.On success, returns power residency
 * statistics in the given sysmon_smem_power_stats structure.
 */
int sysmon_stats_query_power_residency(enum dsp_id_t dsp_id,
					struct sysmon_smem_power_stats *sysmon_power_stats)
{
	int ret = 0;

	if (!sysmon_power_stats) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_adsp))
			memcpy(sysmon_power_stats, g_sysmon_stats.sysmon_power_stats_adsp,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_cdsp))
			memcpy(sysmon_power_stats, g_sysmon_stats.sysmon_power_stats_cdsp,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_slpi))
			memcpy(sysmon_power_stats, g_sysmon_stats.sysmon_power_stats_slpi,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;
}
EXPORT_SYMBOL(sysmon_stats_query_power_residency);
/**
 * API to query requested DSP subsystem's Q6 clock and bandwidth.
 * On success, returns Q6 clock and bandwidth statistics in the given
 * sysmon_smem_q6_event_stats structure.
 */
int sysmon_stats_query_q6_votes(enum dsp_id_t dsp_id,
		struct sysmon_smem_q6_event_stats *sysmon_q6_event_stats)
{
	int ret = 0;

	if (!sysmon_q6_event_stats) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_adsp))
			memcpy(sysmon_q6_event_stats, g_sysmon_stats.sysmon_event_stats_adsp,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_cdsp))
			memcpy(sysmon_q6_event_stats, g_sysmon_stats.sysmon_event_stats_cdsp,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_slpi))
			memcpy(sysmon_q6_event_stats, g_sysmon_stats.sysmon_event_stats_slpi,
				sizeof(struct sysmon_smem_power_stats));
		else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;
}
EXPORT_SYMBOL(sysmon_stats_query_q6_votes);

/*Checks for DSP power collapse and resets the q6 average
 *load to zero if dsp is power collapsed.
 */
u32 sysmon_read_q6_load(enum dsp_id_t dsp_id)
{
	struct sysmon_smem_q6_load_stats sysmon_q6_load;
	u64 last_q6load_update_at = 0;
	u64 curr_timestamp = __arch_counter_get_cntvct();

	switch (dsp_id) {
	case ADSP:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_adsp,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb<<32)|
					sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	case CDSP:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_cdsp,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb << 32)|
					sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	case SLPI:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_slpi,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb << 32)|
						sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		return -EINVAL;
	break;
	}

	return sysmon_q6_load.q6load_avg;
}

/**
 * API to query requested DSP subsystem's Q6 load.
 * On success, returns average Q6 load in KCPS for the given
 * q6load_avg parameter.
 */
int sysmon_stats_query_q6_load(enum dsp_id_t dsp_id, u32 *q6load_avg)
{
	int ret = 0;
	u32 q6_average_load = 0;

	if (!q6load_avg) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_adsp)) {
			q6_average_load = sysmon_read_q6_load(ADSP);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_cdsp)) {
			q6_average_load = sysmon_read_q6_load(CDSP);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_slpi)) {
			q6_average_load = sysmon_read_q6_load(SLPI);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;

}
EXPORT_SYMBOL(sysmon_stats_query_q6_load);
/*
 * API to query requested DSP subsystem sleep stats for
 * LPM and LPI.On success, returns sleep
 * statistics in the given sleep_stats structure for LPM and
 * LPI(on supported subsystems).
 */
int sysmon_stats_query_sleep(enum dsp_id_t dsp_id,
					struct sleep_stats *sleep_stats_lpm,
					struct sleep_stats_island *sleep_stats_lpi)
{
	int ret = 0;

	if (!sleep_stats_lpm && !sleep_stats_lpi) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}
	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();
		/*
		 * If a subsystem is in sleep when reading the sleep stats adjust
		 * the accumulated sleep duration to show actual sleep time.
		 */
		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_adsp)) {
				if (g_sysmon_stats.sleep_stats_adsp->last_entered_at >
					g_sysmon_stats.sleep_stats_adsp->last_exited_at)
					g_sysmon_stats.sleep_stats_adsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_adsp->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_adsp,
							sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}

		if (sleep_stats_lpi) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_adsp)) {
				if (g_sysmon_stats.sleep_lpi_adsp->last_entered_at >
					g_sysmon_stats.sleep_lpi_adsp->last_exited_at)
					g_sysmon_stats.sleep_lpi_adsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_adsp->last_entered_at;

				memcpy(sleep_stats_lpi, g_sysmon_stats.sleep_lpi_adsp,
							sizeof(struct sleep_stats_island));
			} else
				ret = -ENOKEY;
		}
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_cdsp)) {
				if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
					g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
					g_sysmon_stats.sleep_stats_cdsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_cdsp->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_cdsp,
						sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_slpi)) {
				if (g_sysmon_stats.sleep_stats_slpi->last_entered_at >
					g_sysmon_stats.sleep_stats_slpi->last_exited_at)
					g_sysmon_stats.sleep_stats_slpi->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_slpi->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_slpi,
							sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}

		if (sleep_stats_lpi) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_slpi)) {
				if (g_sysmon_stats.sleep_lpi_slpi->last_entered_at >
					g_sysmon_stats.sleep_lpi_slpi->last_exited_at)
					g_sysmon_stats.sleep_lpi_slpi->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_slpi->last_entered_at;

				memcpy(sleep_stats_lpi, g_sysmon_stats.sleep_lpi_slpi,
							sizeof(struct sleep_stats_island));
			} else
				ret = -ENOKEY;
		}

	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;
}
EXPORT_SYMBOL(sysmon_stats_query_sleep);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Sysmon subsystem Stats driver");
