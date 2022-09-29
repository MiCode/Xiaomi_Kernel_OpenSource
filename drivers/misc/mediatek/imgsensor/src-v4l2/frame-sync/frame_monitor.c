// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_monitor.h"


#ifndef FS_UT
#include <linux/of_platform.h>

#ifdef USING_CCU
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_ccu.h>
#endif // USING_CCU

#ifdef USING_N3D
#include "vsync_recorder.h"
#endif // USING_N3D
#endif // !FS_UT

#ifdef FS_UT
// #include <stdint.h> // for uint32_t
typedef unsigned long long uint64_t;
#endif


/******************************************************************************/
// Log message
/******************************************************************************/
#include "frame_sync_log.h"

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
	struct FrameResult results[VSYNCS_MAX];

	unsigned int timestamps[VSYNCS_MAX];
};
//----------------------------------------------------------------------------//


struct FrameInfo {
	unsigned int sensor_id;
	unsigned int sensor_idx;
	unsigned int tg;

	unsigned int wait_for_setting_predicted_fl:1;
	struct FrameMeasurement fmeas;

	/* vsync data information obtained by query */
	/* - vsync for sensor FL */
	struct vsync_time rec;
	unsigned int query_ts_at_tick;
	unsigned int query_ts_at_us;
	unsigned int time_after_vsync;


#ifdef FS_UT
	unsigned int predicted_curr_fl_us; /* current predicted framelength (us) */
	unsigned int predicted_next_fl_us; /* next predicted framelength (us) */
	unsigned int sensor_curr_fl_us;    /* current framelength set to sensor */
	unsigned int next_vts_bias;        /* next vsync timestamp bias / shift */
#endif // FS_UT
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

	unsigned int camsv0_tg;

//----------------------------------------------------------------------------//

	unsigned int cur_tick;
	unsigned int tick_factor;

	struct FrameInfo f_info[SENSOR_MAX_NUM];

//----------------------------------------------------------------------------//

#ifdef FS_UT
	/* flag for using fake information (only for ut test and check) */
	unsigned int debug_flag:1;
#endif // FS_UT

//----------------------------------------------------------------------------//
};
static struct FrameMonitorInst frm_inst;


#if defined(FS_UT)
#define FS_UT_TG_MAPPING_SIZE 23
static const int ut_tg_mapping[FS_UT_TG_MAPPING_SIZE] = {
	-1, -1, -1, 0, 1,
	2, 3, 4, 5, 10,
	11, 12, 13, 14, 15,
	-1, -1, -1, -1, 6,
	7, 8, 9
};
#endif
/******************************************************************************/





/******************************************************************************/
// Dump function
/******************************************************************************/
void dump_frame_info(const unsigned int idx, const char *caller)
{
	struct FrameInfo *p_f_info = &frm_inst.f_info[idx];

	LOG_MUST(
		"[%s]: [%u] ID:%#x(sidx:%u), tg:%u, rec:(id:%u, vsyncs:%u, ts:(%u/%u/%u/%u)), query at:(%u/+%u(%u))\n",
		caller, idx,
		p_f_info->sensor_id,
		p_f_info->sensor_idx,
		p_f_info->tg,
		p_f_info->rec.id,
		p_f_info->rec.vsyncs,
		p_f_info->rec.timestamps[0],
		p_f_info->rec.timestamps[1],
		p_f_info->rec.timestamps[2],
		p_f_info->rec.timestamps[3],
		p_f_info->query_ts_at_us,
		p_f_info->time_after_vsync,
		p_f_info->query_ts_at_tick);
}


void dump_vsync_recs(const struct vsync_rec (*pData), const char *caller)
{
	unsigned int i = 0;

	LOG_MUST(
		"[%s]: ids:%u, cur_tick:%u, tick_factor:%u\n",
		caller,
		pData->ids,
		pData->cur_tick,
		pData->tick_factor);

	for (i = 0; i < pData->ids; ++i) {
		LOG_MUST(
			"[%s]: recs[%u]: id:%u (TG), vsyncs:%u, ts:(%u/%u/%u/%u)\n",
			caller,
			i,
			pData->recs[i].id,
			pData->recs[i].vsyncs,
			pData->recs[i].timestamps[0],
			pData->recs[i].timestamps[1],
			pData->recs[i].timestamps[2],
			pData->recs[i].timestamps[3]);
	}
}
/******************************************************************************/





/******************************************************************************/
// Frame Monitor static function (private function)
/******************************************************************************/
#ifdef USING_CCU
#ifndef FS_UT
/*
 * return:
 *     0: NO error.
 *     1: find compatiable node failed.
 *     2: get ccu_pdev failed.
 *     < 0: read node property failed.
 */
static unsigned int get_ccu_device(const char *caller)
{
	int ret = 0;

#ifndef FS_UT
	phandle handle;
	struct device_node *node = NULL, *rproc_np = NULL;


	/* clear data */
	frm_inst.ccu_pdev = NULL;
	frm_inst.handle = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,camera-fsync-ccu");
	if (!node) {
		ret = 1;
		LOG_MUST(
			"[%s]: ERROR: find DTS compatiable node:(mediatek,camera-fsync-ccu) failed, ret:%d\n",
			caller, ret);
		return ret;
	}

	ret = of_property_read_u32(node, "mediatek,ccu-rproc", &handle);
	if (ret < 0) {
		LOG_MUST(
			"[%s]: ERROR: read DTS node:(mediatek,camera-fsync-ccu) property:(mediatek,ccu-rproc) failed, ret:%d\n",
			caller, ret);
		return ret;
	}

	rproc_np = of_find_node_by_phandle(handle);
	if (rproc_np) {
		frm_inst.ccu_pdev = of_find_device_by_node(rproc_np);

		if (!frm_inst.ccu_pdev) {
			LOG_MUST(
				"[%s]: ERROR: find DTS device by node failed (ccu rproc pdev), ret:%d->2\n",
				caller, ret);
			frm_inst.ccu_pdev = NULL;
			ret = 2;
			return ret;
		}

		/* keep for rproc_get_by_phandle() using */
		frm_inst.handle = handle;

		LOG_MUST(
			"[%s]: get ccu proc pdev successfully, ret:%d\n",
			caller, ret);
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
	if (unlikely(!frm_inst.ccu_pdev)) {
		ret = get_ccu_device(__func__);
		if (unlikely(ret != 0))
			return ret;
	}

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
		LOG_MUST("query CCU vsync data done, ret:%d\n", ret);
#endif


	return ret;
}
#endif // USING_CCU
/******************************************************************************/





/******************************************************************************/
// frame measurement function
/******************************************************************************/
static void frm_dump_measurement_data(
	unsigned int idx, unsigned int passed_vsyncs)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;


	if (p_fmeas->idx == 0) {
		LOG_PF_INF(
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
			frm_inst.f_info[idx].query_ts_at_us,
			frm_inst.f_info[idx].time_after_vsync);
	} else if (p_fmeas->idx == 1) {
		LOG_PF_INF(
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
			frm_inst.f_info[idx].query_ts_at_us,
			frm_inst.f_info[idx].time_after_vsync);
	} else if (p_fmeas->idx == 2) {
		LOG_PF_INF(
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
			frm_inst.f_info[idx].query_ts_at_us,
			frm_inst.f_info[idx].time_after_vsync);
	} else if (p_fmeas->idx == 3) {
		LOG_PF_INF(
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
			frm_inst.f_info[idx].query_ts_at_us,
			frm_inst.f_info[idx].time_after_vsync);
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


static void frm_prepare_frame_measurement_info(
	const unsigned int idx, unsigned int vdiff[])
{
	struct FrameInfo *p_f_info = &frm_inst.f_info[idx];
	unsigned int i = 0;

	/* update timestamp data in fmeas */
	for (i = 0; i < VSYNCS_MAX; ++i)
		p_f_info->fmeas.timestamps[i] = p_f_info->rec.timestamps[i];

	for (i = 0; i < VSYNCS_MAX-1; ++i) {
		vdiff[i] =
			p_f_info->fmeas.timestamps[i] -
			p_f_info->fmeas.timestamps[i+1];
	}
}


void frm_set_frame_measurement(
	unsigned int idx, unsigned int passed_vsyncs,
	unsigned int curr_fl_us, unsigned int curr_fl_lc,
	unsigned int next_fl_us, unsigned int next_fl_lc)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;
	unsigned int i = 0;
	unsigned int meas_idx = 0, vts_idx = 0;
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
	frm_prepare_frame_measurement_info(idx, vdiff);

	/* ring back */
	vts_idx = (p_fmeas->idx + (VSYNCS_MAX-1)) % VSYNCS_MAX;
	for (i = 0; i < VSYNCS_MAX-1; ++i) {
		p_fmeas->results[vts_idx].actual_fl_us = vdiff[i];

		vts_idx = (vts_idx + (VSYNCS_MAX-1)) % VSYNCS_MAX;
	}

	p_fmeas->results[meas_idx].predicted_fl_us = curr_fl_us;
	p_fmeas->results[meas_idx].predicted_fl_lc = curr_fl_lc;
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
	frm_dump_measurement_data(idx, passed_vsyncs);


	/* 5. update newest predict fl (lc / us) */
	/* ring forward the measurement idx */
	/* (because current predicted fl => +0; next predicted fl => +1) */
	meas_idx = (p_fmeas->idx + 1) % VSYNCS_MAX;

	p_fmeas->results[meas_idx].predicted_fl_us = next_fl_us;
	p_fmeas->results[meas_idx].predicted_fl_lc = next_fl_lc;


	/* x. clear check flag */
	frm_inst.f_info[idx].wait_for_setting_predicted_fl = 0;
}


void frm_get_curr_frame_mesurement_and_ts_data(
	const unsigned int idx, unsigned int *p_fmeas_idx,
	unsigned int *p_pr_fl_us, unsigned int *p_pr_fl_lc,
	unsigned int *p_act_fl_us, unsigned int *p_ts_arr)
{
	struct FrameMeasurement *p_fmeas = &frm_inst.f_info[idx].fmeas;
	unsigned int fmeas_idx = 0;
	unsigned int i = 0;

	if (p_fmeas == NULL) {
		LOG_MUST(
			"[%u] ID:%#x(sidx:%u), tg:%u, p_fmeas is NULL\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			frm_inst.f_info[idx].tg);
		return;
	}


	/* current result => ring back for get latest result */
	fmeas_idx = ((p_fmeas->idx) + (VSYNCS_MAX - 1)) % VSYNCS_MAX;
	if (p_fmeas_idx != NULL)
		*p_fmeas_idx = fmeas_idx;

	if (p_pr_fl_us != NULL)
		*p_pr_fl_us = p_fmeas->results[fmeas_idx].predicted_fl_us;
	if (p_pr_fl_lc != NULL)
		*p_pr_fl_lc = p_fmeas->results[fmeas_idx].predicted_fl_lc;
	if (p_act_fl_us != NULL)
		*p_act_fl_us = p_fmeas->results[fmeas_idx].actual_fl_us;
	if (p_ts_arr != NULL) {
		for (i = 0; i < VSYNCS_MAX; ++i)
			p_ts_arr[i] = p_fmeas->timestamps[i];
	}
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


	if (unlikely(!frm_inst.ccu_pdev)) {
		ret = get_ccu_device(__func__);
		if (unlikely(ret != 0))
			return;
	}

	ccu_rproc = rproc_get_by_phandle(frm_inst.handle);

	if (ccu_rproc == NULL) {
		LOG_PR_ERR("ERROR: ccu rproc_get_by_phandle failed, NULL ptr\n");
		return;
	}


	if (flag > 0) {
		/* boot up ccu */
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
		ret = rproc_bootx(ccu_rproc, RPROC_UID_FS);
#else
		ret = rproc_boot(ccu_rproc);
#endif

		if (ret != 0) {
			LOG_MUST(
				"ERROR: call ccu rproc_boot failed, ret:%d\n",
				ret);
			return;
		}

		frm_inst.power_on_cnt++;

#if !defined(REDUCE_FRM_LOG)
		LOG_MUST("framesync power on ccu, cnt:%d\n",
			frm_inst.power_on_cnt);
#endif // REDUCE_FRM_LOG

	} else {
		/* shutdown ccu */
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
		rproc_shutdownx(ccu_rproc, RPROC_UID_FS);
#else
		rproc_shutdown(ccu_rproc);
#endif

		frm_inst.power_on_cnt--;

#if !defined(REDUCE_FRM_LOG)
		LOG_MUST("framesync power off ccu, cnt:%d\n",
			frm_inst.power_on_cnt);
#endif // REDUCE_FRM_LOG
	}

#endif // FS_UT
}


/*
 * this function will be called at streaming on / off
 * uses ccu_rproc_ipc_send function send command data to ccu
 */
void frm_reset_ccu_vsync_timestamp(unsigned int idx, unsigned int en)
{
	unsigned int tg = 0;
	uint64_t selbits = 0;
	int ret = 0;

	tg = frm_inst.f_info[idx].tg;

	/* case handling */
	if (unlikely(tg == CAMMUX_ID_INVALID)) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), tg:%u(invalid), do not call CCU reset(1)/clear(0):%u rproc.\n",
			idx,
			frm_inst.f_info[idx].sensor_id,
			frm_inst.f_info[idx].sensor_idx,
			tg, en);

		return;
	}

	/* bit 0 no use, so "bit 1" --> means 2 */
	/* TG_1 -> bit 1, TG_2 -> bit 2, TG_3 -> bit 3 */
	selbits = ((uint64_t)1 << tg);


#ifndef FS_UT
	if (unlikely(!frm_inst.ccu_pdev)) {
		ret = get_ccu_device(__func__);
		if (unlikely(ret != 0))
			return;
	}

	/* call CCU to reset vsync timestamp */
	ret = mtk_ccu_rproc_ipc_send(
		frm_inst.ccu_pdev,
		MTK_CCU_FEATURE_FMCTRL,
		(en)
			? MSG_TO_CCU_RESET_VSYNC_TIMESTAMP
			: MSG_TO_CCU_CLEAR_VSYNC_TIMESTAMP,
		(void *)&selbits, sizeof(selbits));
#endif

	if (ret != 0)
		LOG_PR_ERR(
			"ERROR: call CCU reset(1)/clear(0):%u, tg:%u (selbits:%#llx) vsync data, ret:%d\n",
			en, tg, selbits, ret);
	else
		LOG_MUST(
			"called CCU reset(1)/clear(0):%u, tg:%u (selbits:%#llx) vsync data, ret:%d\n",
			en, tg, selbits, ret);
}


int frm_get_ccu_pwn_cnt(void)
{
#if !defined(FS_UT)
	return frm_inst.power_on_cnt;
#else
	return 0;
#endif // FS_UT
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
	frm_reset_ccu_vsync_timestamp(idx, 1);
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


static void frm_set_frame_info_vsync_rec_data(const struct vsync_rec (*pData))
{
	unsigned int i = 0, j = 0;

#if defined(FS_UT)
	dump_vsync_recs(pData, __func__);
#endif // FS_UT

	/* always keeps newest tick info (and tick factor) */
	frm_inst.cur_tick = pData->cur_tick;
	frm_inst.tick_factor = pData->tick_factor;

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		struct FrameInfo *p_f_info = &frm_inst.f_info[i];

		/* for SA Frame-Sync, "ids" equal to 1 */
		for (j = 0; j < pData->ids; ++j) {
			unsigned int rec_id = pData->recs[j].id;

			/* check each sensor info (if tg match) */
			if (p_f_info->tg == rec_id) {
				/* copy data */
				p_f_info->rec = pData->recs[j];
				p_f_info->query_ts_at_tick = pData->cur_tick;

				/* update frame info data */
				p_f_info->query_ts_at_us =
					(frm_inst.tick_factor > 0)
					? (p_f_info->query_ts_at_tick
						/ frm_inst.tick_factor)
					: 0;
				p_f_info->time_after_vsync =
					(p_f_info->query_ts_at_us > 0)
					? (p_f_info->query_ts_at_us
						- p_f_info->rec.timestamps[0])
					: 0;

#if defined(FS_UT)
				dump_frame_info(i, __func__);
#endif // FS_UT
			}
		}
	}
}


static void frm_set_wait_for_setting_fmeas_by_tg(
	unsigned int tgs[], unsigned int len)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		if (frm_inst.f_info[i].tg == 0)
			continue;

		for (j = 0; j < len; ++j) {
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


static unsigned int frm_get_camsv0_tg(void)
{
#if !defined(FS_UT)

	unsigned int ret = 0, camsv_id, cammux_id;
	struct device_node *dev_node = NULL;

	do {
		dev_node = of_find_compatible_node(dev_node, NULL,
			"mediatek,camsv");

		if (dev_node) {
			if (of_property_read_u32(dev_node,
					"mediatek,camsv-id", &camsv_id)
				|| of_property_read_u32(dev_node,
					"mediatek,cammux-id", &cammux_id)) {
				/* property not found */
				continue;
			}

			if (camsv_id == 0) {
				ret = cammux_id + 1;
				break;
			}
		}
	} while (dev_node);

	if (ret == 0) {
		LOG_MUST(
			"ERROR: camsv0 cammux id not found, camsv0_tg:%u\n",
			ret);

		return 0;
	}

	LOG_MUST(
		"dev-node:%s, camsv-id:%u, cammux-id:%u, camsv0_tg:%u\n",
		dev_node->name, camsv_id, cammux_id, ret);

	return ret;

#else
	return 4; /* 3 raw */
#endif // FS_UT
}


unsigned int frm_convert_cammux_tg_to_ccu_tg(unsigned int tg)
{
	int camsv_id = -1;
	unsigned int tg_mapped;

	if (frm_inst.camsv0_tg == 0)
		frm_inst.camsv0_tg = frm_get_camsv0_tg();

	camsv_id =
		(tg >= frm_inst.camsv0_tg)
		? (tg - frm_inst.camsv0_tg) : -1;

	tg_mapped = (camsv_id >= 0) ? (camsv_id + CAMSV_TG_MIN) : tg;

	/* error handle for TG mapping non valid */
	if (tg_mapped > CAMSV_TG_MAX) {
		LOG_MUST(
			"NOTICE: input tg:%u, camsv0_tg:%u, camsv_id:%d, tg_mapped:%u but camsv_tg_max:%u(camsv_id_max:%u), ret:%u\n",
			tg,
			frm_inst.camsv0_tg,
			camsv_id,
			tg_mapped,
			CAMSV_TG_MAX,
			CAMSV_MAX,
			tg);

		return tg;
	}

	LOG_MUST(
		"input tg:%u, camsv0_tg:%u, camsv_id:%d, tg_mapped:%u\n",
		tg, frm_inst.camsv0_tg, camsv_id, tg_mapped);

	return tg_mapped;
}


static int frm_get_camsv_id(unsigned int id)
{
#if !defined(FS_UT)

	struct device_node *dev_node = NULL;
	unsigned int camsv_id, cammux_id;
	int ret = -1;

	do {
		dev_node = of_find_compatible_node(dev_node, NULL,
			"mediatek,camsv");

		if (dev_node) {
			if (of_property_read_u32(dev_node,
					"mediatek,camsv-id", &camsv_id)
				|| of_property_read_u32(dev_node,
					"mediatek,cammux-id", &cammux_id)) {
				/* property not found */
				continue;
			}

			if (cammux_id == (id - 1)) {
				ret = camsv_id;
				break;
			}
		}
	} while (dev_node);

#if !defined(REDUCE_FRM_LOG)
	LOG_MUST(
		"get cammux_id:%u(from 1), camsv_id:%u(from 0), cammux_id:%u, ret:%d\n",
		id, camsv_id, cammux_id, ret);
#endif

	return ret;

#else
	return (id > 0 && id <= FS_UT_TG_MAPPING_SIZE)
		? ut_tg_mapping[id-1] : -1;
#endif // FS_UT
}


unsigned int frm_convert_cammux_id_to_ccu_tg_id(unsigned int cammux_id)
{
	int camsv_id = frm_get_camsv_id(cammux_id);
	unsigned int ccu_tg_id;

	ccu_tg_id = (camsv_id >= 0) ? (camsv_id + CAMSV_TG_MIN) : cammux_id;

	LOG_MUST(
		"get cammux_id:%u(from 1), camsv_id:%d(from 0), ccu_tg_id:%u(CAMSV_TG_MIN:%u, CAMSV_TG_MAX:%u)\n",
		cammux_id, camsv_id, ccu_tg_id, CAMSV_TG_MIN, CAMSV_TG_MAX);

	return ccu_tg_id;
}


void frm_update_tg(unsigned int idx, unsigned int tg)
{
	frm_inst.f_info[idx].tg = tg;


#if !defined(REDUCE_FRM_LOG)
	LOG_INF("[%u] ID:%#x(sidx:%u), updated tg:%u\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].tg);
#endif // REDUCE_FRM_LOG
}


static void frm_save_vsync_timestamp(struct vsync_rec (*pData))
{
#ifdef FS_UT
	/* run in test mode */
	if (frm_inst.debug_flag) {
		// frm_inst.debug_flag = 0;
		frm_debug_copy_frame_info_vsync_rec_data(pData);

		return;
	}
#endif

	frm_set_frame_info_vsync_rec_data(pData);
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
	dump_vsync_recs(pData, __func__);
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

#if !defined(REDUCE_FRM_LOG)
		LOG_INF(
			"f_info[%u](sensor_id:%#x / sensor_idx:%u / tg:%u), input tg:%u\n",
			i,
			frm_inst.f_info[i].sensor_id,
			frm_inst.f_info[i].sensor_idx,
			frm_inst.f_info[i].tg,
			tg);
#endif

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


#if !defined(REDUCE_FRM_LOG)
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), predicted_curr_fl:%u (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].predicted_curr_fl_us);
#endif
}


void frm_update_next_vts_bias_us(unsigned int idx, unsigned int vts_bias)
{
	frm_inst.f_info[idx].next_vts_bias = vts_bias;


#if !defined(REDUCE_FRM_LOG)
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


#if !defined(REDUCE_FRM_LOG)
	LOG_INF(
		"[%u] ID:%#x (sidx:%u), sensor_curr_fl:%u (us)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].sensor_curr_fl_us);
#endif
}


void frm_update_predicted_fl_us(
	unsigned int idx,
	unsigned int curr_fl_us, unsigned int next_fl_us)
{
	frm_inst.f_info[idx].predicted_curr_fl_us = curr_fl_us;
	frm_inst.f_info[idx].predicted_next_fl_us = next_fl_us;


#if !defined(REDUCE_FRM_LOG)
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

#if !defined(REDUCE_FRM_LOG)
	LOG_INF(
		"f_info[%u](sensor_id:%#x / sensor_idx:%u / tg:%u / predicted_fl(c:%u, n:%u) / sensor_curr_fl:%u)\n",
		idx,
		frm_inst.f_info[idx].sensor_id,
		frm_inst.f_info[idx].sensor_idx,
		frm_inst.f_info[idx].tg,
		frm_inst.f_info[idx].predicted_curr_fl_us,
		frm_inst.f_info[idx].predicted_next_fl_us,
		frm_inst.f_info[idx].sensor_curr_fl_us);
#endif
}


void frm_get_next_vts_bias_us(unsigned int idx, unsigned int *vts_bias)
{
	*vts_bias = frm_inst.f_info[idx].next_vts_bias;
}


void frm_debug_copy_frame_info_vsync_rec_data(struct vsync_rec (*p_vsync_res))
{
	unsigned int i = 0, j = 0;

	/* always keeps newest tick info (and tick factor) */
	p_vsync_res->cur_tick = frm_inst.cur_tick;
	p_vsync_res->tick_factor = frm_inst.tick_factor;

#if !defined(REDUCE_FRM_LOG)
	LOG_MUST(
		"sync vsync rec data (cur tick:%u, tick factor:%u)\n",
		frm_inst.cur_tick,
		frm_inst.tick_factor);
#endif // REDUCE_FRM_LOG

	for (i = 0; i < p_vsync_res->ids; ++i) {
		unsigned int rec_id = p_vsync_res->recs[i].id;

		for (j = 0; j < SENSOR_MAX_NUM; ++j) {
			/* check info match */
			if (frm_inst.f_info[j].tg == rec_id) {
				/* copy data */
				p_vsync_res->recs[i] = frm_inst.f_info[j].rec;

#if !defined(REDUCE_FRM_LOG)
				LOG_MUST(
					"rec_id:%u/tg:%u, sync data vsyncs:%u, ts:(%u/%u/%u/%u)\n",
					p_vsync_res->recs[i].id,
					frm_inst.f_info[j].tg,
					p_vsync_res->recs[i].vsyncs,
					p_vsync_res->recs[i].timestamps[0],
					p_vsync_res->recs[i].timestamps[1],
					p_vsync_res->recs[i].timestamps[2],
					p_vsync_res->recs[i].timestamps[3]);
#endif // REDUCE_FRM_LOG
			}
		}
	}

	dump_vsync_recs(p_vsync_res, __func__);
}


void frm_debug_set_last_vsync_data(struct vsync_rec (*pData))
{
	frm_inst.debug_flag = 1;

#ifndef REDUCE_FRM_LOG
	LOG_INF("[debug] set data [debug_flag:%d]\n", frm_inst.debug_flag);
#endif

	frm_set_frame_info_vsync_rec_data(pData);
}
#endif
/******************************************************************************/
