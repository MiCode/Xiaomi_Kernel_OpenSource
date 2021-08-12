// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_monitor.h"


#ifndef FS_UT
#ifdef USING_CCU
#include <linux/of_platform.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_ccu.h>
#endif // USING_CCU

#ifdef USING_N3D
#include "vsync_recorder.h"
#endif // USING_N3D
#endif // FS_UT

#ifdef FS_UT
#include <stdint.h> // for uint32_t
#endif


/******************************************************************************/
// Log message => see frame_sync_def.h
/******************************************************************************/
#define REDUCE_FRM_LOG
#define PFX "FrameMonitor"
/******************************************************************************/





/******************************************************************************/
// Frame Monitor Instance Structure (private structure)
/******************************************************************************/
//--------------------------- frame measurement ------------------------------//
struct FrameResult {
	/* TODO: add a auto check process for pred. act. mismatch */
	/* predicted_fl actual_fl is result match */
	// unsigned int is_result_match:1;

	/* predict current framelength */
	unsigned int predicted_fl_us;
	unsigned int predicted_fl_lc;

	/* actual framelength (by timestamp's diff) */
	unsigned int actual_fl_us;
};


struct FrameMeasurement {
	unsigned int is_init:1;

	/* according to the number of vsync passed */
	/* ex: current idx is 2 and the number of vsync passed is 7 */
	/*     => 7 % VSYNCS_MAX = 3, current idx => (2 + 3) % VSYNCS_MAX = 1 */
	/*     => or (2 + 7) % VSYNCS_MAX = 1 */
	/*     all operation based on idx 1 to calculate correct data */
	unsigned int idx;

	unsigned int timestamps[VSYNCS_MAX];

	struct FrameResult results[VSYNCS_MAX];
};
//----------------------------------------------------------------------------//


struct FrameInfo {
	unsigned int sensor_id;
	unsigned int sensor_idx;
	unsigned int tg;

	unsigned int wait_for_setting_predicted_fl:1;
	struct FrameMeasurement fmeas;


#ifdef FS_UT
	unsigned int predicted_curr_fl_us; /* current predicted framelength (us) */
	unsigned int predicted_next_fl_us; /* next predicted framelength (us) */
	unsigned int sensor_curr_fl_us;    /* current framelength set to sensor */
	unsigned int next_vts_bias;        /* next vsync timestamp bias / shift */
#endif
};


struct FrameMonitorInst {
//----------------------------------------------------------------------------//

#ifndef FS_UT
#ifdef USING_CCU
	struct platform_device *ccu_pdev;
	phandle handle;
	int power_on_cnt;
#endif // USING_CCU
#endif // FS_UT

//----------------------------------------------------------------------------//

	struct vsync_time recs[FM_TG_CNT];

	unsigned int query_ts_at_tick[FM_TG_CNT];

	unsigned int cur_tick;
	unsigned int tick_factor;

	struct FrameInfo f_info[SENSOR_MAX_NUM];

//----------------------------------------------------------------------------//

#ifdef FS_UT
	/* flag for using fake information (only for ut test and check) */
	unsigned int debug_flag:1;
#endif

//----------------------------------------------------------------------------//
};
static struct FrameMonitorInst frm_inst;

/******************************************************************************/





/******************************************************************************/
// Dump function
/******************************************************************************/
static inline void dump_vsync_rec(struct vsync_rec (*pData))
{
	unsigned int i = 0;

	LOG_INF("buf->ids:%u, buf->cur_tick:%u, buf->tick_factor:%u\n",
		pData->ids,
		pData->cur_tick,
		pData->tick_factor);

	for (i = 0; i < pData->ids; ++i) {
		LOG_INF(
			"buf->recs[%u]: id:%u (TG), vsyncs:%u, vts:(%u/%u/%u/%u)\n",
			i,
			pData->recs[i].id,
			pData->recs[i].vsyncs,
			pData->recs[i].timestamps[0],
			pData->recs[i].timestamps[1],
			pData->recs[i].timestamps[2],
			pData->recs[i].timestamps[3]);
	}
}


static inline void dump_frm_ts_recs(void)
{
	unsigned int i = 0;

	LOG_INF("frm cur_tick:%u, frm tick_factor:%u\n",
		frm_inst.cur_tick,
		frm_inst.tick_factor);

	for (i = 0; i < FM_TG_CNT; ++i) {
		if (frm_inst.recs[i].id == 0)
			continue;

		LOG_INF(
			"frm [%u]: id:%u (TG), vsyncs:%u, vts:(%u/%u/%u/%u), query at tick:%u\n",
			i,
			frm_inst.recs[i].id,
			frm_inst.recs[i].vsyncs,
			frm_inst.recs[i].timestamps[0],
			frm_inst.recs[i].timestamps[1],
			frm_inst.recs[i].timestamps[2],
			frm_inst.recs[i].timestamps[3],
			frm_inst.query_ts_at_tick[i]);
	}
}
/******************************************************************************/





/******************************************************************************/
// Frame Monitor static function (private function)
/******************************************************************************/
#ifdef FS_UT
static void debug_simu_query_vsync_data(struct vsync_rec (*pData))
{
	unsigned int i = 0, j = 0;

	pData->cur_tick = frm_inst.cur_tick;
	pData->tick_factor = frm_inst.tick_factor;
	for (i = 0; i < FM_TG_CNT; ++i) {
		if (frm_inst.recs[i].id == 0)
			continue;

		for (j = 0; j < pData->ids; ++j) {
			if (pData->recs[j].id == frm_inst.recs[i].id) {
				pData->recs[j] = frm_inst.recs[i];
				pData->cur_tick = frm_inst.query_ts_at_tick[i];

				break;
			}
		}
	}

#if !defined(REDUCE_FRM_LOG)
	LOG_INF("[UT] call dump_vsync_rec\n");
	dump_vsync_rec(pData);
#endif
}
#endif // FS_UT


/*
 * only do boundary check
 *
 * return: (0/1) for (false/true)
 */
static inline unsigned int check_tg_valid(unsigned int tg)
{
	return ((tg < CCU_CAM_TG_MIN) || (tg >= FM_TG_CNT)) ? 0 : 1;
}


static void copy_vsync_data_of_query(struct vsync_rec (*pData))
{
	unsigned int i = 0;

	frm_inst.cur_tick = pData->cur_tick;
	frm_inst.tick_factor = pData->tick_factor;


#if defined(FS_UT)
	LOG_INF("query %u tg timestamp data\n",
		pData->ids);
#endif


	for (i = 0; i < pData->ids; ++i) {
		unsigned int idx = 0;

		if (!check_tg_valid(pData->recs[i].id)) {
			LOG_INF(
				"ERROR: invalid tg:%u num, skip\n",
				pData->recs[i].id);

			continue;
		}

		/* because tg is start from 1 in frame monitor */
		idx = pData->recs[i].id - 1;

		/* copy vsync_time struct data */
		frm_inst.recs[idx] = pData->recs[i];

		/* (for SA) copy tick value at querying timestamp data */
		frm_inst.query_ts_at_tick[idx] = pData->cur_tick;
	}


#if defined(FS_UT)
	dump_frm_ts_recs();
#endif
}


#ifdef USING_CCU
#ifndef FS_UT
static unsigned int get_ccu_device(void)
{
	int ret = 0;

#ifndef FS_UT
	phandle handle;
	struct device_node *node = NULL, *rproc_np = NULL;


	/* clear data */
	frm_inst.ccu_pdev = NULL;
	frm_inst.handle = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,camera_fsync_ccu");
	if (!node) {
		LOG_PR_ERR("ERROR: find mediatek,camera_fsync_ccu failed!!!\n");
		return 1;
	}

	ret = of_property_read_u32(node, "mediatek,ccu_rproc", &handle);
	if (ret < 0) {
		LOG_PR_ERR("ERROR: ccu_rproc of_property_read_u32:%d\n", ret);
		return ret;
	}

	rproc_np = of_find_node_by_phandle(handle);
	if (rproc_np) {
		frm_inst.ccu_pdev = of_find_device_by_node(rproc_np);

		if (!frm_inst.ccu_pdev) {
			LOG_PR_ERR("ERROR: failed to find ccu rproc pdev\n");
			frm_inst.ccu_pdev = NULL;
			return ret;
		}

		/* keep for rproc_get_by_phandle() using */
		frm_inst.handle = handle;

		LOG_INF("get ccu proc pdev successfully\n");
	}
#endif

	return ret;
}
#endif // FS_UT


/*
 * description:
 *     this function uses ccu_rproc_ipc_send function send data to CCU
 *     and get vsyncs data
 */
static unsigned int query_ccu_vsync_data(struct vsync_rec (*pData))
{
	int ret = 0;


#ifdef FS_UT
	/* run in test mode */
	if (frm_inst.debug_flag)
		return ret;
#endif


#ifndef FS_UT
	if (!frm_inst.ccu_pdev)
		get_ccu_device();

	/* using ccu_rproc_ipc_send to get vsync timestamp data */
	ret = mtk_ccu_rproc_ipc_send(
		frm_inst.ccu_pdev,
		MTK_CCU_FEATURE_FMCTRL,
		MSG_TO_CCU_GET_VSYNC_TIMESTAMP,
		(void *)pData, sizeof(struct vsync_rec));
#endif

	if (ret != 0)
		LOG_PR_ERR("ERROR: query CCU vsync data, ret:%d\n", ret);


#ifndef REDUCE_FRM_LOG
	else
		LOG_INF("query CCU vsync data done, ret:%d\n", ret);
#endif


	return ret;
}
#endif // USING_CCU


static inline void frm_save_vsync_timestamp(struct vsync_rec (*pData))
{
#ifdef FS_UT
	/* run in test mode */
	if (frm_inst.debug_flag) {
		// frm_inst.debug_flag = 0;
		debug_simu_query_vsync_data(pData);

		return;
	}
#endif

	copy_vsync_data_of_query(pData);
}
/******************************************************************************/





/******************************************************************************/
// frame measurement function
/******************************************************************************/
/*
 * input:
 *     idx: search all for finding tg value match
 *
 * output:
 *     vdiff: array for timestamp diff (actual frame length)
 *     p_query_vts_at: query timestamp at tick
 *     p_time_after_sof: query timestamp after SOF
 */
static void calc_ts_diff_and_info(
	unsigned int idx,
	unsigned int vdiff[],
	unsigned int *p_query_vts_at, unsigned int *p_time_after_sof)
{
	unsigned int i = 0, j = 0;
	unsigned int query_at_tick = 0, query_vts_at = 0, time_after_sof = 0;

	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;


	for (i = 0; i < FM_TG_CNT; ++i) {
		if (frm_inst.recs[i].id == 0)
			continue;

		/* find TG value match */
		if (frm_inst.recs[i].id == frm_inst.f_info[idx].tg) {
			for (j = 0; j < VSYNCS_MAX; ++j) {
				p_fmeas->timestamps[j] =
						frm_inst.recs[i].timestamps[j];
			}

			query_at_tick = frm_inst.query_ts_at_tick[i];

			query_vts_at =
				(frm_inst.tick_factor > 0)
				? (query_at_tick / frm_inst.tick_factor) : 0;

			time_after_sof = (query_vts_at > 0)
				? (query_vts_at - p_fmeas->timestamps[0]) : 0;


			if (p_query_vts_at != NULL)
				*p_query_vts_at = query_vts_at;
			if (p_time_after_sof != NULL)
				*p_time_after_sof = time_after_sof;
		}
	}


	for (i = 0; i < VSYNCS_MAX-1; ++i)
		vdiff[i] = p_fmeas->timestamps[i] - p_fmeas->timestamps[i+1];
}


static void frm_dump_measurement_data(
	unsigned int idx, unsigned int passed_vsyncs,
	unsigned int query_vts_at, unsigned int time_after_sof)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;


	if (p_fmeas->idx == 0) {
		LOG_INF(
			"[%u] ID:%#x (sidx:%u), tg:%d, vsync:%u, pred/act fl:(curr:%u,*0:%u(%u)/%u, 1:%u(%u)/%u, 2:%u(%u)/%u, 3:%u(%u)/%u), ts_tg_%u:(%u/%u/%u/%u), query_vts_at:%u (SOF + %u)\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			frm_inst.f_info[idx].tg,
			passed_vsyncs,
			p_fmeas->idx,
			p_fmeas->results[0].predicted_fl_us,
			p_fmeas->results[0].predicted_fl_lc,
			p_fmeas->results[0].actual_fl_us,
			p_fmeas->results[1].predicted_fl_us,
			p_fmeas->results[1].predicted_fl_lc,
			p_fmeas->results[1].actual_fl_us,
			p_fmeas->results[2].predicted_fl_us,
			p_fmeas->results[2].predicted_fl_lc,
			p_fmeas->results[2].actual_fl_us,
			p_fmeas->results[3].predicted_fl_us,
			p_fmeas->results[3].predicted_fl_lc,
			p_fmeas->results[3].actual_fl_us,
			frm_inst.f_info[idx].tg,
			p_fmeas->timestamps[0],
			p_fmeas->timestamps[1],
			p_fmeas->timestamps[2],
			p_fmeas->timestamps[3],
			query_vts_at,
			time_after_sof);
	} else if (p_fmeas->idx == 1) {
		LOG_INF(
			"[%u] ID:%#x (sidx:%u), tg:%d, vsync:%u, pred/act fl:(curr:%u, 0:%u(%u)/%u,*1:%u(%u)/%u, 2:%u(%u)/%u, 3:%u(%u)/%u), ts_tg_%u:(%u/%u/%u/%u), query_vts_at:%u (SOF + %u)\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			frm_inst.f_info[idx].tg,
			passed_vsyncs,
			p_fmeas->idx,
			p_fmeas->results[0].predicted_fl_us,
			p_fmeas->results[0].predicted_fl_lc,
			p_fmeas->results[0].actual_fl_us,
			p_fmeas->results[1].predicted_fl_us,
			p_fmeas->results[1].predicted_fl_lc,
			p_fmeas->results[1].actual_fl_us,
			p_fmeas->results[2].predicted_fl_us,
			p_fmeas->results[2].predicted_fl_lc,
			p_fmeas->results[2].actual_fl_us,
			p_fmeas->results[3].predicted_fl_us,
			p_fmeas->results[3].predicted_fl_lc,
			p_fmeas->results[3].actual_fl_us,
			frm_inst.f_info[idx].tg,
			p_fmeas->timestamps[0],
			p_fmeas->timestamps[1],
			p_fmeas->timestamps[2],
			p_fmeas->timestamps[3],
			query_vts_at,
			time_after_sof);
	} else if (p_fmeas->idx == 2) {
		LOG_INF(
			"[%u] ID:%#x (sidx:%u), tg:%d, vsync:%u, pred/act fl:(curr:%u, 0:%u(%u)/%u, 1:%u(%u)/%u,*2:%u(%u)/%u, 3:%u(%u)/%u), ts_tg_%u:(%u/%u/%u/%u), query_vts_at:%u (SOF + %u)\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			frm_inst.f_info[idx].tg,
			passed_vsyncs,
			p_fmeas->idx,
			p_fmeas->results[0].predicted_fl_us,
			p_fmeas->results[0].predicted_fl_lc,
			p_fmeas->results[0].actual_fl_us,
			p_fmeas->results[1].predicted_fl_us,
			p_fmeas->results[1].predicted_fl_lc,
			p_fmeas->results[1].actual_fl_us,
			p_fmeas->results[2].predicted_fl_us,
			p_fmeas->results[2].predicted_fl_lc,
			p_fmeas->results[2].actual_fl_us,
			p_fmeas->results[3].predicted_fl_us,
			p_fmeas->results[3].predicted_fl_lc,
			p_fmeas->results[3].actual_fl_us,
			frm_inst.f_info[idx].tg,
			p_fmeas->timestamps[0],
			p_fmeas->timestamps[1],
			p_fmeas->timestamps[2],
			p_fmeas->timestamps[3],
			query_vts_at,
			time_after_sof);
	} else if (p_fmeas->idx == 3) {
		LOG_INF(
			"[%u] ID:%#x (sidx:%u), tg:%d, vsync:%u, pred/act fl:(curr:%u, 0:%u(%u)/%u, 1:%u(%u)/%u, 2:%u(%u)/%u,*3:%u(%u)/%u), ts_tg_%u:(%u/%u/%u/%u), query_vts_at:%u (SOF + %u)\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			frm_inst.f_info[idx].tg,
			passed_vsyncs,
			p_fmeas->idx,
			p_fmeas->results[0].predicted_fl_us,
			p_fmeas->results[0].predicted_fl_lc,
			p_fmeas->results[0].actual_fl_us,
			p_fmeas->results[1].predicted_fl_us,
			p_fmeas->results[1].predicted_fl_lc,
			p_fmeas->results[1].actual_fl_us,
			p_fmeas->results[2].predicted_fl_us,
			p_fmeas->results[2].predicted_fl_lc,
			p_fmeas->results[2].actual_fl_us,
			p_fmeas->results[3].predicted_fl_us,
			p_fmeas->results[3].predicted_fl_lc,
			p_fmeas->results[3].actual_fl_us,
			frm_inst.f_info[idx].tg,
			p_fmeas->timestamps[0],
			p_fmeas->timestamps[1],
			p_fmeas->timestamps[2],
			p_fmeas->timestamps[3],
			query_vts_at,
			time_after_sof);
	}
}


static inline void frm_reset_measurement(unsigned int idx)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;
	struct FrameMeasurement clear_st = {0};

	*p_fmeas = clear_st;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), tg:%d, all set to zero\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].tg);
#endif
}


static inline void frm_init_measurement(
	unsigned int idx,
	unsigned int def_fl_us, unsigned int def_fl_lc)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;

	frm_reset_measurement(idx);

	p_fmeas->results[p_fmeas->idx].predicted_fl_us = def_fl_us;
	p_fmeas->results[p_fmeas->idx].predicted_fl_lc = def_fl_lc;

	p_fmeas->results[p_fmeas->idx + 1].predicted_fl_us = def_fl_us;
	p_fmeas->results[p_fmeas->idx + 1].predicted_fl_lc = def_fl_lc;

	p_fmeas->is_init = 1;
}


#ifdef FS_UT
static void frm_update_predicted_fl_us(
	unsigned int idx,
	unsigned int curr_fl_us, unsigned int next_fl_us);
#endif


void frm_set_frame_measurement(
	unsigned int idx, unsigned int passed_vsyncs,
	unsigned int curr_fl_us, unsigned int curr_fl_lc,
	unsigned int next_fl_us, unsigned int next_fl_lc)
{
	unsigned int i = 0;

	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;
	// unsigned int last_updated_idx = (p_fmeas->idx + 1) % VSYNCS_MAX;
	unsigned int meas_idx = 0, vts_idx = 0;
	unsigned int query_vts_at = 0, time_after_sof = 0;
	unsigned int vdiff[VSYNCS_MAX] = {0};


	if (!frm_inst.f_info[idx].wait_for_setting_predicted_fl) {

#ifndef REDUCE_FRM_LOG
		LOG_PR_WARN("Error: not wait for setting predicted fl, return\n");
#endif

		return;
	}

	if (!p_fmeas->is_init)
		frm_init_measurement(idx, curr_fl_us, curr_fl_lc);


#ifdef FS_UT
	/* 0. for UT test */
	frm_update_predicted_fl_us(idx, curr_fl_us, next_fl_us);
#endif


	/* 1. update frame measurement reference idx */
	p_fmeas->idx = (p_fmeas->idx + passed_vsyncs) % VSYNCS_MAX;


	/* 2. set frame measurement predicted next frame length */
	meas_idx = p_fmeas->idx;


	/* 3. calculate actual frame length using vsync timestamp */
	calc_ts_diff_and_info(idx, vdiff, &query_vts_at, &time_after_sof);

	/* ring back */
	vts_idx = (p_fmeas->idx + (VSYNCS_MAX-1)) % VSYNCS_MAX;
	for (i = 0; i < VSYNCS_MAX-1; ++i) {
		p_fmeas->results[vts_idx].actual_fl_us = vdiff[i];

		vts_idx = (vts_idx + (VSYNCS_MAX-1)) % VSYNCS_MAX;
	}

	/* for passing more vsyncs case */
	/*     i from 1 for preventing passed_vsyncs is 0 */
	/*     and (0-1) compare to (unsigned int) will cause overflow */
	/*     and preventing passed_vsyncs is a big value cause timeout */
	for (i = 1; (i < passed_vsyncs) && (i < VSYNCS_MAX+1); ++i) {
		p_fmeas->results[meas_idx].predicted_fl_us = curr_fl_us;
		p_fmeas->results[meas_idx].predicted_fl_lc = curr_fl_lc;

		meas_idx = (meas_idx + (VSYNCS_MAX-1)) % VSYNCS_MAX;
	}


	/* 4. dump frame measurement */
	frm_dump_measurement_data(
		idx, passed_vsyncs, query_vts_at, time_after_sof);


	/* 5. update newest predict fl (lc / us) */
	/* ring forward the measurement idx */
	/* (because current predicted fl => +0; next predicted fl => +1) */
	meas_idx = (p_fmeas->idx + 1) % VSYNCS_MAX;

	p_fmeas->results[meas_idx].predicted_fl_us = next_fl_us;
	p_fmeas->results[meas_idx].predicted_fl_lc = next_fl_lc;


	/* x. clear check flag */
	frm_inst.f_info[idx].wait_for_setting_predicted_fl = 0;
}
/******************************************************************************/





/******************************************************************************/
// frame monitor function
/******************************************************************************/
#ifdef USING_CCU
void frm_power_on_ccu(unsigned int flag)
{
#ifndef FS_UT

	int ret = 0;
	struct rproc *ccu_rproc = NULL;


	if (!frm_inst.ccu_pdev)
		get_ccu_device();

	ccu_rproc = rproc_get_by_phandle(frm_inst.handle);

	if (ccu_rproc == NULL) {
		LOG_PR_ERR("ERROR: ccu rproc_get_by_phandle failed, NULL ptr\n");
		return;
	}


	if (flag > 0) {
		/* boot up ccu */
		ret = rproc_boot(ccu_rproc);

		if (ret != 0) {
			LOG_PR_ERR("ERROR: ccu rproc_boot failed!\n");
			return;
		}

		frm_inst.power_on_cnt++;

		LOG_INF("framesync power on ccu, cnt:%d\n",
			frm_inst.power_on_cnt);
	} else {
		/* shutdown ccu */
		rproc_shutdown(ccu_rproc);

		frm_inst.power_on_cnt--;

		LOG_INF("framesync power off ccu, cnt:%d\n",
			frm_inst.power_on_cnt);
	}

#endif
}


/*
 * this function will be called at streaming on / off
 * uses ccu_rproc_ipc_send function send command data to ccu
 */
void frm_reset_ccu_vsync_timestamp(unsigned int idx)
{
	unsigned int tg = 0;
	uint32_t selbits = 0;
	int ret = 0;


	tg = frm_inst.f_info[idx].tg;

	/* bit 0 no use, so "bit 1" --> means 2 */
	/* TG_1 -> bit 1, TG_2 -> bit 2, TG_3 -> bit 3 */
	selbits = (1 << tg);


#ifndef FS_UT
	if (!frm_inst.ccu_pdev)
		get_ccu_device();

	/* call CCU to reset vsync timestamp */
	ret = mtk_ccu_rproc_ipc_send(
		frm_inst.ccu_pdev,
		MTK_CCU_FEATURE_FMCTRL,
		MSG_TO_CCU_RESET_VSYNC_TIMESTAMP,
		(void *)&selbits, sizeof(selbits));
#endif

	if (ret != 0)
		LOG_PR_ERR("ERROR: call CCU reset tg:%u (selbits:%u) vsync data\n",
			tg, selbits);
	else
		LOG_INF("called CCU reset tg:%u (selbits:%u) vsync data\n",
			tg, selbits);
}
#endif // USING_CCU


void frm_reset_frame_info(unsigned int idx)
{
	struct FrameInfo clear_st = {0};

	frm_inst.f_info[idx] = clear_st;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF("clear idx:%u data. (all to zero)\n", idx);
#endif
}


void frm_init_frame_info_st_data(
	unsigned int idx,
	unsigned int sensor_id, unsigned int sensor_idx, unsigned int tg)
{
	frm_reset_frame_info(idx);


	frm_inst.f_info[idx].sensor_id = sensor_id;
	frm_inst.f_info[idx].sensor_idx = sensor_idx;
	frm_inst.f_info[idx].tg = tg;


#ifdef USING_CCU
#ifndef DELAY_CCU_OP
	frm_reset_ccu_vsync_timestamp(idx);
#endif
#endif


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), tg:%d\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].tg);
#endif
}


static void frm_set_wait_for_setting_fmeas_by_tg(
	unsigned int tgs[], unsigned int len)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		if (frm_inst.f_info[i].tg == 0)
			continue;

		for (j = 0; j < len; ++j) {
			if (!check_tg_valid(tgs[j])) {
				LOG_INF(
					"ERROR: invalid tg:%u num, skip\n",
					tgs[j]);

				continue;
			}

			if (frm_inst.f_info[i].tg == tgs[j]) {
				frm_inst.f_info[i]
					.wait_for_setting_predicted_fl = 1;


#ifndef REDUCE_FRM_LOG
				/* log */
				LOG_INF(
					"[%u] ID:%#x (sidx:%u), tg:%d, wait for setting predicted fl(%u)\n",
					i,
					frm_inst.f_info[i].sensor_id,
					frm_inst.f_info[i].sensor_idx,
					frm_inst.f_info[i].tg,
					frm_inst.f_info[i]
						.wait_for_setting_predicted_fl);
#endif
			}
		}
	}
}


void frm_update_tg(unsigned int idx, unsigned int tg)
{
	frm_inst.f_info[idx].tg = tg;


	LOG_INF("[%u] ID:%#x(sidx:%u), updated tg:%u\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].tg);
}


/*
 * input:
 *     tgs: array of tg num for query timestamp
 *     len: array length
 *     *pData: pointer for timestamp dat of query needs to be written to
 *
 * return (0/non 0) for (done/error)
 */
unsigned int frm_query_vsync_data(
	unsigned int tgs[], unsigned int len, struct vsync_rec *pData)
{
	unsigned int i = 0;

#if defined(USING_CCU) || defined(USING_N3D)
	unsigned int ret = 0;
#endif

	struct vsync_rec vsyncs_data = {0};


#ifndef REDUCE_FRM_LOG
	LOG_INF("Query %u sensors timestamps\n", len);
#endif


	/* boundary checking */
	//if (len > SENSOR_MAX_NUM)
	if (len > TG_MAX_NUM) {
		LOG_PR_WARN("ERROR: too many TGs. (bigger than CCU TG_MAX)\n");
		return 1;
	}


	/* 1. setup input Data */
	/*    for query vsync data from CCU, put TG(s) in the structure */
	vsyncs_data.ids = len;
	for (i = 0; i < len; ++i)
		vsyncs_data.recs[i].id = tgs[i];


#ifndef REDUCE_FRM_LOG
	LOG_INF("recs[0]:%u, recs[1]:%u, recs:[2]:%u (TG)\n",
		vsyncs_data.recs[0].id,
		vsyncs_data.recs[1].id,
		vsyncs_data.recs[2].id);
#endif


#ifdef USING_CCU
	/* 2. get vsync data from CCU using rproc ipc send */
	ret = query_ccu_vsync_data(&vsyncs_data);
	if (ret != 0) {
		LOG_PR_WARN("ERROR: at querying vsync data from CCU\n");
		return 1;
	}
#endif // USING_CCU


#ifdef USING_N3D
	/* 2. get vsync data from CCU using rproc ipc send */
	ret = query_n3d_vsync_data(&vsyncs_data);
	if (ret != 0) {
		LOG_PR_WARN("ERROR: at querying vsync data from N3D\n");
		return 1;
	}
#endif // USING_N3D


	/* 3. save data (in buffer) querying before to frame monitor */
	frm_save_vsync_timestamp(&vsyncs_data);

	/* 4. write back for caller */
	*pData = vsyncs_data;


#ifndef REDUCE_FRM_LOG
	LOG_INF("call dump_frm_ts_recs\n");
	dump_frm_ts_recs();

	LOG_INF("call dump_vsync_rec\n");
	dump_vsync_rec(pData);
#endif


	frm_set_wait_for_setting_fmeas_by_tg(tgs, len);


	return 0;
}
/******************************************************************************/





/******************************************************************************/
// Debug function
/******************************************************************************/
#ifdef FS_UT
/* only for FrameSync Driver and ut test used */
int frm_get_instance_idx_by_tg(unsigned int tg)
{
	int ret = -1;
	unsigned int i = 0;

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		if (frm_inst.f_info[i].tg == tg) {
			ret = i;
			break;
		}
	}

	return ret;
}


void frm_update_predicted_curr_fl_us(unsigned int idx, unsigned int fl_us)
{
	frm_inst.f_info[idx].predicted_curr_fl_us = fl_us;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), predicted_curr_fl:%u (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].predicted_fl_us);
#endif
}


void frm_update_next_vts_bias_us(unsigned int idx, unsigned int vts_bias)
{
	frm_inst.f_info[idx].next_vts_bias = vts_bias;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), next_vts_bias:%u (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].next_vts_bias);
#endif
}


void frm_set_sensor_curr_fl_us(unsigned int idx, unsigned int fl_us)
{
	frm_inst.f_info[idx].sensor_curr_fl_us = fl_us;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), sensor_curr_fl:%u (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].sensor_curr_fl_us);
#endif
}


static void frm_update_predicted_fl_us(
	unsigned int idx,
	unsigned int curr_fl_us, unsigned int next_fl_us)
{
	frm_inst.f_info[idx].predicted_curr_fl_us = curr_fl_us;
	frm_inst.f_info[idx].predicted_next_fl_us = next_fl_us;


#ifndef REDUCE_FRM_LOG
	/* log */
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), predicted_fl( curr:%u, next:%u ) (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].predicted_curr_fl_us,
		frm_inst.f_info[idx].predicted_next_fl_us);
#endif
}

unsigned int frm_get_predicted_curr_fl_us(unsigned int idx)
{
	return frm_inst.f_info[idx].predicted_curr_fl_us;
}


void frm_get_predicted_fl_us(
	unsigned int idx,
	unsigned int fl_us[], unsigned int *sensor_curr_fl_us)
{
	fl_us[0] = frm_inst.f_info[idx].predicted_curr_fl_us;
	fl_us[1] = frm_inst.f_info[idx].predicted_next_fl_us;
	*sensor_curr_fl_us = frm_inst.f_info[idx].sensor_curr_fl_us;
}


void frm_get_next_vts_bias_us(unsigned int idx, unsigned int *vts_bias)
{
	*vts_bias = frm_inst.f_info[idx].next_vts_bias;
}


void frm_debug_set_last_vsync_data(struct vsync_rec (*pData))
{
	frm_inst.debug_flag = 1;


#ifndef REDUCE_FRM_LOG
	LOG_INF("[debug] set data [debug_flag:%d]\n", frm_inst.debug_flag);
#endif


	copy_vsync_data_of_query(pData);
}
#endif
/******************************************************************************/
