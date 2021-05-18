// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_sync.h"
#include "frame_sync_camsys.h"
#include "frame_sync_algo.h"
#include "frame_monitor.h"


/******************************************************************************/
// Log message => see frame_sync_def.h
/******************************************************************************/
#define REDUCE_FS_DRV_LOG
#define PFX "FrameSync"
/******************************************************************************/


/******************************************************************************/
// Mutex Lock
/******************************************************************************/
#ifdef FS_UT
#include <pthread.h>
static pthread_mutex_t gRegisterLocker = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gBitLocker = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gStatusLocker = PTHREAD_MUTEX_INITIALIZER;
#else
#include <linux/mutex.h>
static DEFINE_MUTEX(gRegisterLocker);
static DEFINE_MUTEX(gBitLocker);
static DEFINE_MUTEX(gStatusLocker);
#endif // FS_UT
/******************************************************************************/





/******************************************************************************/
// Frame Sync Mgr Structure (private structure)
/******************************************************************************/

//---------------------------- frame recorder --------------------------------//
struct FrameRecord {
	unsigned int shutter_lc;
	unsigned int framelength_lc;
};

struct FrameRecorder {
	/* init: 0 => recorder need input default framelength. */
	unsigned int init;
	unsigned int depthIdx;
	struct FrameRecord frame_recs[RECORDER_DEPTH];
};
//----------------------------------------------------------------------------//


//------------------------ fs mgr register sensor-----------------------------//
struct SensorInfo {
	unsigned int sensor_id;         // imx586 -> 0x0586; s5k3m5sx -> 0x30D5
	unsigned int sensor_idx;        // main1 -> 0; sub1 -> 1;
					// main2 -> 2; sub2 -> 3; main3 -> 4;
};

struct SensorTable {
	unsigned int reg_cnt;
	struct SensorInfo sensors[SENSOR_MAX_NUM];
};
//----------------------------------------------------------------------------//


//------------------------ call back function data ---------------------------//
struct callback_st {
	void *p_ctx;
	unsigned int cmd_id;
	callback_set_framelength func_ptr;
};
//----------------------------------------------------------------------------//


//-------------------------------- fs mgr ------------------------------------//
struct FrameSyncMgr {
	/* Sensor Register Table */
	struct SensorTable reg_table;

	/* Frame Sync Status information */
	enum FS_STATUS fs_status;
	unsigned int user_counter;

	unsigned int streaming_bits;            // can do set / unset
	unsigned int enSync_bits;               // can do set / unset

	unsigned int validSync_bits;            // for checking PF status
	unsigned int pf_ctrl_bits;              // for checking PF status
	unsigned int last_pf_ctrl_bits;         // for checking PF status


#ifdef USING_CCU
	unsigned int power_on_ccu_bits;         // for checking CCU pw ON/OFF
#endif


	/* ctrl needed by FS have been setup */
	unsigned int setup_complete_bits;
	unsigned int last_setup_complete_bits;


	/* Frame Settings Recorder */
	struct FrameRecorder frm_recorder[SENSOR_MAX_NUM];


	/* call back */
	struct callback_st cb_data[SENSOR_MAX_NUM];


	/* fs act cnt */
	unsigned int act_cnt;
};
static struct FrameSyncMgr fs_mgr;
//----------------------------------------------------------------------------//


#ifdef FS_SENSOR_CCU_IT
/* (IT using) Set FL regularly to observe the sensor's response */
#define FL_TABLE_SIZE 8
static unsigned int fl_table[FL_TABLE_SIZE] = {
	40000, 45000, 50000, 55000, 60000, 65000, 70000, 75000};

static unsigned int fl_table_idxs[SENSOR_MAX_NUM] = {0, 1, 2, 3, 4};
#endif // FS_SENSOR_CCU_IT


/******************************************************************************/





/******************************************************************************/
// Operate & Basic & utility function
/******************************************************************************/
static inline void change_fs_status(enum FS_STATUS status)
{
#ifdef FS_UT
	pthread_mutex_lock(&gStatusLocker);
#else
	mutex_lock(&gStatusLocker);
#endif // FS_UT


	fs_mgr.fs_status = status;


#ifdef FS_UT
	pthread_mutex_unlock(&gStatusLocker);
#else
	mutex_unlock(&gStatusLocker);
#endif // FS_UT
}


static inline enum FS_STATUS get_fs_status(void)
{
	enum FS_STATUS status;

#ifdef FS_UT
	pthread_mutex_lock(&gStatusLocker);
#else
	mutex_lock(&gStatusLocker);
#endif // FS_UT


	status = fs_mgr.fs_status;


#ifdef FS_UT
	pthread_mutex_unlock(&gStatusLocker);
#else
	mutex_unlock(&gStatusLocker);
#endif // FS_UT

	return status;
}


static inline unsigned int check_sensorIdx(unsigned int sensor_idx)
{
	if (sensor_idx >= 0 && sensor_idx < SENSOR_MAX_NUM)
		return 1;

	return 0;
}


static inline unsigned int check_bit(unsigned int idx, unsigned int val)
{
	unsigned int ret = check_sensorIdx(idx);

#ifdef FS_UT
	pthread_mutex_lock(&gBitLocker);
#else
	mutex_lock(&gBitLocker);
#endif // FS_UT


	if (ret == 1)
		val = ((val >> idx) & 0x01);


#ifdef FS_UT
	pthread_mutex_unlock(&gBitLocker);
#else
	mutex_unlock(&gBitLocker);
#endif // FS_UT


	if (ret == 1)
		return val;
	else
		return 0xFFFFFFFF;
}


static unsigned int
write_bit(unsigned int idx, unsigned int en, unsigned int (*val))
{
	unsigned int ret = check_sensorIdx(idx);

#ifdef FS_UT
	pthread_mutex_lock(&gBitLocker);
#else
	mutex_lock(&gBitLocker);
#endif // FS_UT


	if (ret == 1) {
		if (en > 0)
			(*val |= (0x01 << idx));
		else
			(*val &= ~(0x01 << idx));
	}


#ifdef FS_UT
	pthread_mutex_unlock(&gBitLocker);
#else
	mutex_unlock(&gBitLocker);
#endif // FS_UT

	// TODO : if sensor idx is wrong, do error handle?
	return ret;
}


static void clear_all_bit(unsigned int (*val))
{
#ifdef FS_UT
	pthread_mutex_lock(&gBitLocker);
#else
	mutex_lock(&gBitLocker);
#endif // FS_UT


	*val = 0;


#ifdef FS_UT
	pthread_mutex_unlock(&gBitLocker);
#else
	mutex_unlock(&gBitLocker);
#endif // FS_UT
}
/******************************************************************************/





/******************************************************************************/
// Dump & Debug function
/******************************************************************************/
static inline void fs_dump_pf_info(struct fs_perframe_st *pf_info)
{
	LOG_INF(
		"ID:%#x(sidx:%u), mim_fl_lc:%u, shutter_lc:%u, margin_lc:%u, flicker(%u), lineTime(ns):%u(%u/%u)\n",
		pf_info->sensor_id,
		pf_info->sensor_idx,
		pf_info->min_fl_lc,
		pf_info->shutter_lc,
		pf_info->margin_lc,
		pf_info->flicker_en,
		pf_info->lineTimeInNs,
		pf_info->linelength,
		pf_info->pclk);
}
/******************************************************************************/





/******************************************************************************/
// Register Sensor function  /  SensorInfo & SensorTable
//
// PS: The method register "BY_SENSOR_IDX" is't been tested in this SW flow.
/******************************************************************************/
static inline unsigned int
compare_sensor_id(unsigned int id1, unsigned int id2)
{
	return (id1 == id2 && id1 != 0) ? 1 : 0;
}


static inline unsigned int
compare_sensor_idx(unsigned int idx1, unsigned int idx2)
{
	return (idx1 == idx2) ? 1 : 0;
}


static inline unsigned int check_sensor_info(
	struct SensorInfo (*info1),
	struct SensorInfo (*info2),
	enum CHECK_SENSOR_INFO_METHOD method)
{
	switch (method) {
	case BY_SENSOR_ID:
		return compare_sensor_id(info1->sensor_id, info2->sensor_id);

	case BY_SENSOR_IDX:
		return compare_sensor_idx(info1->sensor_idx, info2->sensor_idx);

	default:
		return compare_sensor_idx(info1->sensor_idx, info2->sensor_idx);
	}
}


/* TODO: re-write this function to get a point of wanted idx data */
/*       not doing data copy */
static inline struct SensorInfo fs_get_reg_sensor_info(unsigned int idx)
{
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;
	struct SensorInfo info = {
		.sensor_id = pSensorTable->sensors[idx].sensor_id,
		.sensor_idx = pSensorTable->sensors[idx].sensor_idx
	};
	return info;
}


/*******************************************************************************
 * input:
 *    struct SensorInfo*, enum CHECK_SENSOR_INFO_METHOD
 *
 * return:
 *    0xffffffff -> not found this sensor ID in "SensorTable".
 *    others -> array position for the registered sensor save in.
 ******************************************************************************/
static unsigned int fs_search_reg_sensors(
	struct SensorInfo (*sensor_info),
	enum CHECK_SENSOR_INFO_METHOD method)
{
	unsigned int i = 0;

	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;
	unsigned int (*pRegCnt) = &pSensorTable->reg_cnt;

	for (i = 0; i < (*pRegCnt); ++i) {
		struct SensorInfo (*pInfo) = &fs_mgr.reg_table.sensors[i];


		if (check_sensor_info(pInfo, sensor_info, method))
			return i;
	}
	return 0xffffffff;
}


/*******************************************************************************
 * input:
 *    struct SensorInfo*
 *
 * return:
 *    0xffffffff -> reach maximum register capacity, couldn't register it.
 *    others -> array position for the registered sensor save in.
 *
 * !!! You have check the return idx before using it. !!!
 ******************************************************************************/
static unsigned int fs_push_sensor(struct SensorInfo (*sensor_info))
{
	unsigned int *pRegCnt = NULL;
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;

#ifdef FS_UT
	pthread_mutex_lock(&gRegisterLocker);
#else
	mutex_lock(&gRegisterLocker);
#endif // FS_UT


	pRegCnt = &pSensorTable->reg_cnt;

	/* check if reach maximum capacity */
	if (*pRegCnt < SENSOR_MAX_NUM)
		pSensorTable->sensors[(*pRegCnt)++] = *sensor_info;
	else
		goto error_sensor_max_count;


#ifdef FS_UT
	pthread_mutex_unlock(&gRegisterLocker);
#else
	mutex_unlock(&gRegisterLocker);
#endif // FS_UT


	return (*pRegCnt - 1);


error_sensor_max_count:

#ifdef FS_UT
	pthread_mutex_unlock(&gRegisterLocker);
#else
	mutex_unlock(&gRegisterLocker);
#endif // FS_UT

	return 0xffffffff;
}


/*******************************************************************************
 * input:
 *    struct SensorInfo*, enum CHECK_SENSOR_INFO_METHOD
 *
 * return:
 *    0xffffffff -> couldn't register sensor.
 *			-> reach maximum capacity.
 *			-> sensor ID is "0".
 *
 *    others -> array position for the registered sensor save in.
 *
 * !!! You have better to check the return idx before using it. !!!
 ******************************************************************************/
static unsigned int fs_register_sensor(
	struct SensorInfo (*sensor_info),
	enum CHECK_SENSOR_INFO_METHOD method)
{
	unsigned int idx = 0;
	struct SensorInfo info = {0}; // for log using

	/* 1. check error sensor id */
	if (method == BY_SENSOR_ID && sensor_info->sensor_id == 0) {
		LOG_PR_WARN("ERROR: sensor ID is %#x\n", sensor_info->sensor_id);
		return 0xffffffff;
	}


	/* 2. search registered sensors */
	idx = fs_search_reg_sensors(sensor_info, method);
	if (idx == 0xffffffff) {
		/* 2-1. add / push this sensor, regitser it */
		/*      two results: */
		/*          1. register successfully; */
		/*          2. can't register */
		idx = fs_push_sensor(sensor_info);
		if (check_sensorIdx(idx)) {
			LOG_INF(
				"Not found sensor. ID:%#x(sidx:%u), register it (idx:%u), method:%s\n",
				sensor_info->sensor_id,
				sensor_info->sensor_idx,
				idx,
				REG_INFO);
		} else
			LOG_PR_WARN("ERROR: Reach max sensor capacity\n");

	} else {
		/* 2-2. this sensor has been registered, do nothing */
		/* log print info */
		info = fs_get_reg_sensor_info(idx);
		LOG_INF("Found sensor. ID:%#x(sidx:%u), idx:%u, method:%s\n",
			info.sensor_id,
			info.sensor_idx,
			idx,
			REG_INFO);


		if (method == BY_SENSOR_IDX) {
			if (fs_mgr.reg_table.sensors[idx].sensor_id !=
				sensor_info->sensor_id) {

				LOG_INF("Overwrite to... [%u] ID:%#x(sidx:%u), method:%s\n",
					idx,
					sensor_info->sensor_id,
					sensor_info->sensor_idx,
					REG_INFO);

				fs_mgr.reg_table.sensors[idx].sensor_id =
					sensor_info->sensor_id;
			}
		}

	}

	return idx;
}


/*******************************************************************************
 * input:
 *    ident : sensor ID / sensor idx
 *
 * return:
 *    0xffffffff -> not found this sensor idx in "SensorTable".
 *    others -> array position for the registered sensor save in.
 ******************************************************************************/
static inline unsigned int fs_get_reg_sensor_pos(unsigned int ident)
{
	unsigned int idx = 0;
	struct SensorInfo info = {0};


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		info.sensor_id = ident;
		break;

	case BY_SENSOR_IDX:
		info.sensor_idx = ident;
		break;

	default:
		info.sensor_idx = ident;
		break;
	}


	idx = fs_search_reg_sensors(&info, REGISTER_METHOD);

	return idx;
}
/******************************************************************************/





/******************************************************************************/
// call back struct data operation
/******************************************************************************/
static inline void fs_reset_cb_data(unsigned int idx)
{
	fs_mgr.cb_data[idx].func_ptr = NULL;
	fs_mgr.cb_data[idx].p_ctx = NULL;
	fs_mgr.cb_data[idx].cmd_id = 0;
}


static inline void fs_init_cb_data(unsigned int idx, void *p_ctx,
	callback_set_framelength func_ptr)
{
	fs_mgr.cb_data[idx].func_ptr = func_ptr;
	fs_mgr.cb_data[idx].p_ctx = p_ctx;
	fs_mgr.cb_data[idx].cmd_id = 0;
}


static inline void fs_set_cb_cmd_id(unsigned int idx, unsigned int cmd_id)
{
	fs_mgr.cb_data[idx].cmd_id = cmd_id;
}


/******************************************************************************/





/******************************************************************************/
// Frame Recorder function
/******************************************************************************/
static inline void frec_dump_recorder(unsigned int idx)
{
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	/* log print info */
	struct SensorInfo info = fs_get_reg_sensor_info(idx);

	LOG_INF(
		"[%u] ID:%#x(sidx:%u) recs:(at %u) (0:%u/%u), (1:%u/%u), (2:%u/%u) (fl_lc/shut_lc)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,

		/* ring back */
		(pFrameRecord->depthIdx + (RECORDER_DEPTH-1)) % (RECORDER_DEPTH),

		pFrameRecord->frame_recs[0].framelength_lc,
		pFrameRecord->frame_recs[0].shutter_lc,
		pFrameRecord->frame_recs[1].framelength_lc,
		pFrameRecord->frame_recs[1].shutter_lc,
		pFrameRecord->frame_recs[2].framelength_lc,
		pFrameRecord->frame_recs[2].shutter_lc);
}


/*******************************************************************************
 * Notify fs algo and frame monitor the data in the frame recorder have been
 * updated
 *
 * This function should be call after having any frame recorder operation
 *
 *
 * description:
 *     fs algo will use these information to predict current and
 *     next framelength when calculating vsync diff.
 ******************************************************************************/
static void frec_notify_setting_frame_record_st_data(unsigned int idx)
{
	unsigned int i = 0;

	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];
	unsigned int depthIdx = pFrameRecord->depthIdx;

	struct frame_record_st recs[RECORDER_DEPTH];


	/* 1. prepare frame settings in recorder */
	/*    to this order: 0:newest, 1:second, 2:third */
	for (i = 0; i < RECORDER_DEPTH; ++i) {
		/* 1-1. ring back depthIdx */
		depthIdx = (depthIdx + (RECORDER_DEPTH-1)) % (RECORDER_DEPTH);


		/* 1-2. map the settings to frame record structure */
		recs[i].framelength_lc =
			&pFrameRecord->frame_recs[depthIdx].framelength_lc;

		recs[i].shutter_lc =
			&pFrameRecord->frame_recs[depthIdx].shutter_lc;
	}


	/* 2. call fs alg set frame record data */
	fs_alg_set_frame_record_st_data(idx, recs);
}


#ifdef FS_SENSOR_CCU_IT
/*******************************************************************************
 * input:
 *     idx: sensor register position in sensortable.
 *     fl_lc: framelength_lc result from fs_alg_solve_framelength.
 *
 * description:
 *     update / record fs_alg_solve_framelength results for next calculation
 ******************************************************************************/
static void
frec_set_framelength_lc(unsigned int idx, unsigned int fl_lc)
{
#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif


	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	unsigned int curr_depth = pFrameRecord->depthIdx;


	/* ring back to point to current data records */
	curr_depth = ((curr_depth + (RECORDER_DEPTH-1)) % (RECORDER_DEPTH));


	/* set framelength lc */
	pFrameRecord->frame_recs[curr_depth].framelength_lc = fl_lc;


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF(
		"[%u] ID:%#x(sidx:%u) recs[*%u] = *%u/%u (fl_lc/shut_lc)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		curr_depth,
		pFrameRecord->frame_recs[curr_depth].framelength_lc,
		pFrameRecord->frame_recs[curr_depth].shutter_lc);
#endif // REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	/* move to fs_set_framelength_lc() calling bellow function */
	// frec_notify_setting_frame_record_st_data(idx);
}
#endif // FS_SENSOR_CCU_IT


static inline void frec_push_shutter_fl_lc(
	unsigned int idx,
	unsigned int shutter_lc, unsigned int fl_lc)
{
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];
	unsigned int (*pDepthIdx) = &pFrameRecord->depthIdx;


#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
	unsigned int bufDepthIdx = (*pDepthIdx);	// for log using
#endif // REDUCE_FS_DRV_LOG


	/* push shutter_lc and framelength_lc */
	pFrameRecord->frame_recs[*pDepthIdx].shutter_lc = shutter_lc;
	pFrameRecord->frame_recs[*pDepthIdx].framelength_lc = fl_lc;
	(*pDepthIdx) = (((*pDepthIdx) + 1) % RECORDER_DEPTH);


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[%u] ID:%#x(sidx:%u) recs[%u] = %u/%u (fl_lc/shut_lc)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		bufDepthIdx,
		fl_lc,
		shutter_lc);
#endif // REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


/* abort */
static inline void
frec_push_def_framelength_lc(unsigned int idx, unsigned int val)
{
	unsigned int i = 0;

	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	for (i = 0; i < RECORDER_DEPTH; ++i)
		pFrameRecord->frame_recs[i].framelength_lc = val;
}


static inline void frec_push_def_shutter_fl_lc(
	unsigned int idx,
	unsigned int shutter_lc, unsigned int fl_lc)
{
#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif


	unsigned int i = 0;

	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	for (i = 0; i < RECORDER_DEPTH; ++i) {
		pFrameRecord->frame_recs[i].shutter_lc = shutter_lc;
		pFrameRecord->frame_recs[i].framelength_lc = fl_lc;
	}

	pFrameRecord->init = 1;


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[%u] ID:%#x(sidx:%u) frame recorder initialized:%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		pFrameRecord->init);
#endif // REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


/* abort, init proc in frec_push_def_shutter_fl_lc function */
static inline void frec_init_recorder(unsigned int idx)
{
	struct SensorInfo info = {0}; // for log using
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	pFrameRecord->init = 1;

	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[%u] ID:%#x(sidx:%u) init:%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		pFrameRecord->init);
}


/* abort */
static inline void
frec_init_recorder_val(unsigned int idx, unsigned int lineTimeInNs)
{
	unsigned int i = 0;

	struct SensorInfo info = {0}; // for log using
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	/* convert fl_lc to framelength (us) and set init to true */
	unsigned int val = convert2TotalTime(
				lineTimeInNs,
				pFrameRecord->frame_recs[0].framelength_lc);

	for (i = 0; i < RECORDER_DEPTH; ++i)
		pFrameRecord->frame_recs[i].framelength_lc = val;

	pFrameRecord->init = 1;


	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[%u] ID:%#x(sidx:%u) init:%u, def_framelength:%u (us)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		pFrameRecord->init,
		val);
}


static inline void frec_push_record(
	unsigned int idx,
	unsigned int shutter_lc, unsigned int framelength_lc)
{
	if (fs_mgr.frm_recorder[idx].init == 0) {
		// frec_init_recorder_val(idx, frameCtrl->lineTimeInNs);
		// frec_init_recorder(idx);

		// TODO : add error handle ?

		/* log print info */
		struct SensorInfo info = fs_get_reg_sensor_info(idx);

		LOG_INF(
			"[%u] ID:%#x(sidx:%u) push shutter, fl before initialized recorder\n",
			idx,
			info.sensor_id,
			info.sensor_idx);
	}


	frec_push_shutter_fl_lc(idx, shutter_lc, framelength_lc);


#ifndef REDUCE_FS_DRV_LOG
	/* log */
	frec_dump_recorder(idx);
#endif // REDUCE_FS_DRV_LOG
}


static inline void
frec_reset_recorder(unsigned int idx)
{
	unsigned int i = 0;

	struct FrameRecord clear_st = {0};
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif


	/* all FrameRecorder member variables set to 0 */
	pFrameRecord->init = 0;
	pFrameRecord->depthIdx = 0;

	for (i = 0; i < RECORDER_DEPTH; ++i)
		pFrameRecord->frame_recs[i] = clear_st;


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[%u] ID:%#x(sidx:%u) init:%u, depthIdx:%u, recs[]:%u/%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		pFrameRecord->init,
		pFrameRecord->depthIdx,
		pFrameRecord->frame_recs[0].framelength_lc,
		pFrameRecord->frame_recs[0].shutter_lc);
#endif // REDUCE_FS_DRV_LOG
}
/******************************************************************************/





#ifdef FS_SENSOR_CCU_IT
/******************************************************************************/
// Frame Sync IT (FS_Drv + FRM + Adaptor + Sensor Driver + CCU) function
/******************************************************************************/
static inline void
fs_set_status_bits(unsigned int idx, unsigned int flag, unsigned int (*bits));

static void fs_set_sensor_driver_framelength_lc(
	unsigned int idx, unsigned int fl_lc);

static void fs_set_framelength_lc(unsigned int idx, unsigned int fl_lc);

static void fs_single_cam_IT(unsigned int idx, unsigned int line_time_ns)
{
	unsigned int idxs[TG_MAX_NUM] = {0};
	unsigned int fl_lc = 0;

	struct SensorInfo info = {0}; // for log using


	/* 1. query timestamp from CCU */
	idxs[0] = idx;

	/* get Vsync data by calling fs_algo func to call Frame Monitor */
	if (fs_alg_get_vsync_data(idxs, 1))
		LOG_PR_WARN("Get Vsync data ERROR\n");


	/* 2. set FL regularly for testing */
	fl_lc = fl_table[fl_table_idxs[idx]];
	fl_lc = convert2LineCount(line_time_ns, fl_lc);

	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("[FS IT] [%u] ID:%#x(sidx:%u) set FL:%u(%u), lineTimeInNs:%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		fl_table[fl_table_idxs[idx]],
		fl_lc,
		line_time_ns);

	fl_table_idxs[idx] = (fl_table_idxs[idx] + 1) % FL_TABLE_SIZE;

	/* all framelength operation must use this API */
	fs_set_framelength_lc(idx, fl_lc);


	/* 3. clear pf_ctrl_bits data of this idx */
	fs_set_status_bits(idx, 0, &fs_mgr.pf_ctrl_bits);
}
#endif // FS_SENSOR_CCU_IT
/******************************************************************************/





/******************************************************************************/
// Frame Sync Mgr function
/******************************************************************************/
static void fs_init(void)
{
	enum FS_STATUS status = get_fs_status();

	fs_mgr.user_counter++;

	if (status == FS_NONE) {
		change_fs_status(FS_INITIALIZED);

		LOG_INF("FrameSync init. (User:%u)\n", fs_mgr.user_counter);
	} else if (status == FS_INITIALIZED)
		LOG_INF("Initialized. (User:%u)\n", fs_mgr.user_counter);

	// else if () => for re-init.
}


static unsigned int fs_update_status(unsigned int idx, unsigned int flag)
{
	unsigned int cnt = 0;
	enum FS_STATUS status = 0;


#ifdef FS_UT
	pthread_mutex_lock(&gBitLocker);
#else
	mutex_lock(&gBitLocker);
#endif // FS_UT


	/* update validSync_bits value */
	fs_mgr.validSync_bits =
		fs_mgr.streaming_bits &
		fs_mgr.enSync_bits;


	/* change status or not by counting the number of valid sync sensors */
#ifdef FS_UT
	cnt = __builtin_popcount(fs_mgr.validSync_bits);
#else
	cnt = hweight32(fs_mgr.validSync_bits);
#endif // FS_UT


	if (cnt > 1 && cnt <= SENSOR_MAX_NUM)
		change_fs_status(FS_WAIT_FOR_SYNCFRAME_START);
	else
		change_fs_status(FS_INITIALIZED);

	status = get_fs_status();


#ifdef FS_UT
	pthread_mutex_unlock(&gBitLocker);
#else
	mutex_unlock(&gBitLocker);
#endif // FS_UT



#ifndef USING_CCU
	LOG_INF(
		"stat:%u, ready:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u, setup_complete:%u [idx:%u, en:%u]\n",
		status,
		cnt,
		fs_mgr.streaming_bits,
		fs_mgr.enSync_bits,
		fs_mgr.validSync_bits,
		fs_mgr.pf_ctrl_bits,
		fs_mgr.setup_complete_bits,
		idx,
		flag);
#else
	LOG_INF(
		"stat:%u, ready:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u, setup_complete:%u, pw_ccu:%u [idx:%u, en:%u]\n",
		status,
		cnt,
		fs_mgr.streaming_bits,
		fs_mgr.enSync_bits,
		fs_mgr.validSync_bits,
		fs_mgr.pf_ctrl_bits,
		fs_mgr.setup_complete_bits,
		fs_mgr.power_on_ccu_bits,
		idx,
		flag);
#endif


	return cnt;
}


static inline void
fs_set_status_bits(unsigned int idx, unsigned int flag, unsigned int (*bits))
{
	if (flag > 0)
		write_bit(idx, 1, bits);
	else
		write_bit(idx, 0, bits);

	//unsigned int validSyncCnt = fs_update_status(sensor_idx, flag);
	fs_update_status(idx, flag);
}


static inline void fs_set_stream(unsigned int idx, unsigned int flag)
{
	fs_set_status_bits(idx, flag, &fs_mgr.streaming_bits);
}


static inline void fs_set_sync_status(unsigned int idx, unsigned int flag)
{
	struct SensorInfo info = {0}; // for log using
	info = fs_get_reg_sensor_info(idx);


	/* unset sync => reset pf_ctrl_bits data of this idx */
	/* TODO: add a API for doing this */
	if (flag == 0) {
		write_bit(idx, flag, &fs_mgr.pf_ctrl_bits);
		write_bit(idx, flag, &fs_mgr.setup_complete_bits);


#ifdef USING_CCU
#ifdef DELAY_CCU_OP
		if (check_bit(idx, fs_mgr.power_on_ccu_bits) == 1) {
			write_bit(idx, 0, &fs_mgr.power_on_ccu_bits);

			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%u (OFF)\n",
				idx,
				info.sensor_id,
				info.sensor_idx,
				fs_mgr.power_on_ccu_bits);

			/* power off CCU */
			frm_power_on_ccu(0);
		}
#endif // DELAY_CCU_OP
#endif // USING_CCU
	}


#ifdef USING_CCU
#ifdef DELAY_CCU_OP
	if (flag > 0 && check_bit(idx, fs_mgr.power_on_ccu_bits) == 0) {
		write_bit(idx, 1, &fs_mgr.power_on_ccu_bits);

		LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%u (ON)\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			fs_mgr.power_on_ccu_bits);

		/* power on CCU and get device handle */
		frm_power_on_ccu(1);

		frm_reset_ccu_vsync_timestamp(idx);
	}
#endif // DELAY_CCU_OP
#endif // USING_CCU


	fs_set_status_bits(idx, flag, &fs_mgr.enSync_bits);


	/* log print info */
	LOG_INF("en:%u [%u] ID:%#x(sidx:%u)   [enSync_bits:%u]\n",
		flag,
		idx,
		info.sensor_id,
		info.sensor_idx,
		fs_mgr.enSync_bits);
}


void fs_set_sync(unsigned int ident, unsigned int flag)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}

	fs_set_sync_status(idx, flag);
}


unsigned int fs_is_set_sync(unsigned int ident)
{
	unsigned int idx = 0, result = 0;
	struct SensorInfo info = {0}; // for log using

	idx = fs_get_reg_sensor_pos(ident);

	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return 0;
	}


	result = check_bit(idx, fs_mgr.enSync_bits);


	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("%u [%u] ID:%#x(sidx:%u)   [enSync_bits:%u]\n",
		result,
		idx,
		info.sensor_id,
		info.sensor_idx,
		fs_mgr.enSync_bits);


	return result;
}


static inline void fs_notify_sensor_ctrl_setup_complete(
	unsigned int idx,
	unsigned int sensor_id)
{
	/* check this sensor is valid for sync or not */
	if (check_bit(idx, fs_mgr.validSync_bits) == 1)
		write_bit(idx, 1, &fs_mgr.setup_complete_bits);


#ifndef REDUCE_FS_DRV_LOG
	LOG_INF(
		"Sensor ctrl setup completed. [%u] ID:%#x  [setup_complete_bits:%u]\n",
		idx,
		sensor_id,
		fs_mgr.setup_complete_bits);
#endif // REDUCE_FS_DRV_LOG
}


static inline void fs_reset_idx_ctx(unsigned int idx)
{
	/* 1. unset sync */
	fs_set_sync_status(idx, 0);

	/* 2. reset frm frame info data */
	frm_reset_frame_info(idx);

	/* 3. reset fs instance data (algo -> fs_inst[idx]) */
	fs_alg_reset_fs_inst(idx);

	/* 4. reset frame recorder data */
	frec_reset_recorder(idx);

	/* 5. unset stream (stream off) */
	fs_set_stream(idx, 0);

	/* 6. clear/reset callback function pointer */
	fs_reset_cb_data(idx);
}


/*******************************************************************************
 * update fs_streaming_st data
 *     (for cam_mux switch & sensor stream on before cam mux setup)
 ******************************************************************************/
void fs_update_tg(unsigned int ident, unsigned int tg)
{
	unsigned int is_streaming = 0;
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	struct SensorInfo info = fs_get_reg_sensor_info(idx);


	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}


	is_streaming = check_bit(idx, fs_mgr.streaming_bits);

	if (!is_streaming) {
		LOG_PR_WARN(
			"WARNING: [%u] ID:%#x(sidx:%u) is not streaming ON. set TG:%u. (TG maybe overwrite when fs_streaming(1))\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			tg);
	}


	/* 1. update the fs_streaming_st data */
	fs_alg_update_tg(idx, tg);
	frm_update_tg(idx, tg);
}


/*******************************************************************************
 * input:
 *     flag:"non 0" -> stream on;
 *              "0" -> stream off;
 *     struct fs_streaming_st *
 *
 * return:
 *     "0" -> done (no error)
 *     "non 0" -> error. (1 -> register failed)
 ******************************************************************************/
unsigned int
fs_streaming(unsigned int flag, struct fs_streaming_st (*sensor_info))
{
	unsigned int idx = 0;


	/* 1. register this sensor */
	struct SensorInfo info = {
		.sensor_id = sensor_info->sensor_id,
		.sensor_idx = sensor_info->sensor_idx,
	};

	/* register this sensor and check return idx/position value */
	idx = fs_register_sensor(&info, REGISTER_METHOD);
	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [idx:%u] ID:%#x(sidx:%u)\n",
			idx, info.sensor_id, info.sensor_idx);

		/* TODO: return a special error number ? */
		return 1;
	}


	/* 2. reset this idx item and reset CCU vsync timestamp */
#ifndef REDUCE_FS_DRV_LOG
	LOG_INF("Reset FS.(en:%u) [%u] ID:%#x(sidx:%u)\n",
		flag,
		idx,
		sensor_info->sensor_id,
		sensor_info->sensor_idx);
#endif // REDUCE_FS_DRV_LOG

	fs_reset_idx_ctx(idx);


	/* 3. if fs_streaming on, set information of this idx correctlly */
	if (flag > 0) {
		LOG_INF("Stream on [%u] ID:%#x(sidx:%u)\n",
			idx,
			sensor_info->sensor_id,
			sensor_info->sensor_idx);


#ifdef USING_CCU
#ifndef DELAY_CCU_OP
		if (check_bit(idx, fs_mgr.power_on_ccu_bits) == 0) {
			write_bit(idx, 1, &fs_mgr.power_on_ccu_bits);

			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%u (ON)\n",
				idx,
				sensor_info->sensor_id,
				sensor_info->sensor_idx,
				fs_mgr.power_on_ccu_bits);

			/* power on CCU and get device handle */
			frm_power_on_ccu(1);
		}
#endif // DELAY_CCU_OP
#endif // USING_CCU


		/* set data to frm, fs algo, and frame recorder */
		frm_init_frame_info_st_data(idx,
			sensor_info->sensor_id, sensor_info->sensor_idx,
			sensor_info->tg);

		fs_alg_set_streaming_st_data(idx, sensor_info);
		//frec_push_def_framelength_lc(idx, sensor_info->def_fl_lc);

		frec_push_def_shutter_fl_lc(
				idx,
				sensor_info->def_shutter_lc,
				sensor_info->def_fl_lc);

		fs_set_stream(idx, 1);

		/* set/init callback data */
		fs_init_cb_data(idx, sensor_info->p_ctx, sensor_info->func_ptr);
	} else {
		LOG_INF("Stream off [%u] ID:%#x(sidx:%u)\n",
			idx,
			sensor_info->sensor_id,
			sensor_info->sensor_idx);


		/* reset fs act cnt */
		fs_mgr.act_cnt = 0;


#ifdef USING_CCU
#ifndef DELAY_CCU_OP
		if (check_bit(idx, fs_mgr.power_on_ccu_bits) == 1) {
			write_bit(idx, flag, &fs_mgr.power_on_ccu_bits);

			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%u (OFF)\n",
				idx,
				sensor_info->sensor_id,
				sensor_info->sensor_idx,
				fs_mgr.power_on_ccu_bits);

			/* power off CCU */
			frm_power_on_ccu(0);
		}
#endif // DELAY_CCU_OP
#endif // USING_CCU
	}


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	info = fs_get_reg_sensor_info(idx);
	LOG_INF("en:%u, [%u] ID:%#x(sidx:%u)   [streaming_bits:%u]\n",
		flag,
		idx,
		info.sensor_id,
		info.sensor_idx,
		fs_mgr.streaming_bits);
#endif // REDUCE_FS_DRV_LOG


#ifdef FS_SENSOR_CCU_IT
	if (flag > 0)
		fs_set_sync(idx, 1);
#endif // FS_SENSOR_CCU_IT

	return 0;
}


static unsigned int fs_try_trigger_frame_sync(void);
/*******************************************************************************
 * input:
 *     flag:"non 0" -> Start sync frame;
 *              "0" -> End sync frame;
 *
 * return:
 *     "0" -> error
 *     "non 0" -> (Start -> cnt of validSync_bits;
 *                   End -> cnt of pf_ctrl_bits)
 *
 * header file:
 *     frame_sync_camsys.h
 ******************************************************************************/
unsigned int fs_sync_frame(unsigned int flag)
{
	enum FS_STATUS status = get_fs_status();
	unsigned int triggered = 0;


	/* check sync frame start/end */
	/*     flag > 0  : start sync frame */
	/*     flag == 0 : end sync frame */
	if (flag > 0) {
		/* check status is ready for starting sync frame or not */
		if (status < FS_WAIT_FOR_SYNCFRAME_START) {
			LOG_PR_WARN(
				"[Start:%u] ERROR: stat:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u(last:%u), setup_complete:%u(last:%u)\n",
				flag,
				status,
				fs_mgr.streaming_bits,
				fs_mgr.enSync_bits,
				fs_mgr.validSync_bits,
				fs_mgr.pf_ctrl_bits,
				fs_mgr.last_pf_ctrl_bits,
				fs_mgr.setup_complete_bits,
				fs_mgr.last_setup_complete_bits);

			return 0;
		}

		/* ready for sync -> update status */
		change_fs_status(FS_START_TO_GET_PERFRAME_CTRL);


		/* log --- show new status and all bits value */
		status = get_fs_status();
		LOG_INF(
			"[Start:%u] stat:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u(last:%u), setup_complete:%u(last:%u)\n",
			flag,
			status,
			fs_mgr.streaming_bits,
			fs_mgr.enSync_bits,
			fs_mgr.validSync_bits,
			fs_mgr.pf_ctrl_bits,
			fs_mgr.last_pf_ctrl_bits,
			fs_mgr.setup_complete_bits,
			fs_mgr.last_setup_complete_bits);


		/* return the number of valid sync sensors */
#ifdef FS_UT
		return __builtin_popcount(fs_mgr.validSync_bits);
#else
		return hweight32(fs_mgr.validSync_bits);
#endif // FS_UT

	} else {
		/* 1. check fs status */
		if (status != FS_START_TO_GET_PERFRAME_CTRL) {
			LOG_PR_WARN(
				"[Start:%u] ERROR: stat:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u(%u), setup_complete:%u(%u)\n",
				flag,
				status,
				fs_mgr.streaming_bits,
				fs_mgr.enSync_bits,
				fs_mgr.validSync_bits,
				fs_mgr.pf_ctrl_bits,
				fs_mgr.last_pf_ctrl_bits,
				fs_mgr.setup_complete_bits,
				fs_mgr.last_setup_complete_bits);

			return 0;
		}


		/* 2. check for running frame sync processing or not */
		triggered = fs_try_trigger_frame_sync();

		if (triggered) {
			/* 2-1. update status */
			change_fs_status(FS_WAIT_FOR_SYNCFRAME_START);


			/* 2-2. framesync trigger complete */
			/*      , clear pf_ctrl_bits, setup_complete_bits */
			fs_mgr.last_pf_ctrl_bits = fs_mgr.pf_ctrl_bits;
			fs_mgr.last_setup_complete_bits =
						fs_mgr.setup_complete_bits;
			clear_all_bit(&fs_mgr.pf_ctrl_bits);
			clear_all_bit(&fs_mgr.setup_complete_bits);


			/* 2-3. (LOG) show new status and pf_ctrl_bits value */
			status = get_fs_status();
			LOG_INF(
				"[Start:%u] stat:%u, pf_ctrl:%u->%u, ctrl_setup_complete:%u->%u\n",
				flag,
				status,
				fs_mgr.last_pf_ctrl_bits,
				fs_mgr.pf_ctrl_bits,
				fs_mgr.last_setup_complete_bits,
				fs_mgr.setup_complete_bits);
		}


		/* return the number of perframe ctrl sensors */
#ifdef FS_UT
		return __builtin_popcount(fs_mgr.last_pf_ctrl_bits);
#else
		return hweight32(fs_mgr.last_pf_ctrl_bits);
#endif // FS_UT
	}
}


/*******************************************************************************
 * update fs_perframe_st data
 *     (for v4l2_ctrl_request_setup order)
 *     (set_framelength is called after set_anti_flicker)
 ******************************************************************************/
void fs_update_auto_flicker_mode(unsigned int ident, unsigned int en)
{
	unsigned int is_streaming = 0;
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	struct SensorInfo info = fs_get_reg_sensor_info(idx);


	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}


	is_streaming = check_bit(idx, fs_mgr.streaming_bits);

	if (!is_streaming) {
		LOG_PR_WARN(
			"WARNING: [%u] ID:%#x(sidx:%u) is not streaming ON. (set flicker_en:%u)\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			en);
	}


	/* 1. update the fs_perframe_st data in fs algo */
	fs_alg_set_anti_flicker(idx, en);


	/* if this is the last ctrl needed by FrameSync, notify setup complete */
	/* NOTE: don't bind here, flicker is NOT per-frame ctrl */
	// fs_notify_sensor_ctrl_setup_complete(idx, info.sensor_id);
}


/*******************************************************************************
 * update fs_perframe_st data
 *     (for v4l2_ctrl_request_setup order)
 *     (set_max_framerate_by_scenario is called after set_shutter)
 ******************************************************************************/
void fs_update_min_framelength_lc(unsigned int ident, unsigned int min_fl_lc)
{
	unsigned int is_streaming = 0;
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	struct SensorInfo info = fs_get_reg_sensor_info(idx);


	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}


	is_streaming = check_bit(idx, fs_mgr.streaming_bits);

	if (!is_streaming) {
		LOG_PR_WARN(
			"WARNING: [%u] ID:%#x(sidx:%u) is not streaming ON. (set min_fl_lc:%u)\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			min_fl_lc);
	}


	/* 1. update the fs_perframe_st data in fs algo */
	fs_alg_update_min_fl_lc(idx, min_fl_lc);


#ifndef USING_V4L2_CTRL_REQUEST_SETUP
	/* if this is the last ctrl needed by FrameSync, notify setup complete */
	fs_notify_sensor_ctrl_setup_complete(idx, info.sensor_id);
#endif // USING_V4L2_CTRL_REQUEST_SETUP
}


static void fs_set_sensor_driver_framelength_lc(
	unsigned int idx, unsigned int fl_lc)
{
	int ret = 0;
	struct callback_st *p_cb = &fs_mgr.cb_data[idx];
	callback_set_framelength cb_func = p_cb->func_ptr;

	struct SensorInfo info = {0}; // for log using

	info = fs_get_reg_sensor_info(idx);


	/* callback sensor driver to set framelength lc */
	if (p_cb->func_ptr != NULL) {
		if (fl_lc != 0)
			ret = cb_func(p_cb->p_ctx, p_cb->cmd_id, fl_lc);

		if (ret != 0) {
			LOG_PR_WARN(
				"ERROR: [%u] ID:%#x(sidx:%u), set fl_lc:%u failed, p_ctx:%p\n",
				idx,
				info.sensor_id,
				info.sensor_idx,
				fl_lc,
				fs_mgr.cb_data[idx].p_ctx);
		}
	} else
		LOG_PR_WARN("ERROR: [%u] ID:%#x(sidx:%u), func_ptr is NULL\n",
			idx,
			info.sensor_id,
			info.sensor_idx);
}


/*******************************************************************************
 * all framelength operation must use this API
 ******************************************************************************/
static inline void fs_set_framelength_lc(unsigned int idx, unsigned int fl_lc)
{
#ifdef FS_SENSOR_CCU_IT
	/* 0. update frame recorder data */
	/*    frec data in normal process will be update by fs algo */
	frec_set_framelength_lc(idx, fl_lc);
#endif // FS_SENSOR_CCU_IT


#ifndef REDUCE_FS_DRV_LOG
	frec_dump_recorder(idx);
#endif


	/* 1. using callback function to set framelength */
	fs_set_sensor_driver_framelength_lc(idx, fl_lc);


	/* 2. set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


/*******************************************************************************
 * Run Frame Sync Method
 *
 * ex:
 *     1. software frame sync.
 ******************************************************************************/
static inline unsigned int fs_run_frame_sync_proc(
	unsigned int solveIdxs[],
	unsigned int framelength[],
	unsigned int len)
{
	unsigned int ret = 0;


	/* run frame sync method */
	/* ex: */
	/*     1. software frame sync */
	/*        call fs algo API to solve framelength */
	ret = fs_alg_solve_frame_length(solveIdxs, framelength, len);

	return ret;
}


unsigned int fs_try_trigger_frame_sync(void)
{
	unsigned int i = 0, ret = 0;
	unsigned int fs_act = 0;
	unsigned int len = 0; /* how many sensors wait for doing frame sync */
	unsigned int solveIdxs[SENSOR_MAX_NUM] = {0};
	unsigned int fl_lc[SENSOR_MAX_NUM] = {0};


	/* if validSync_bits == pf_ctrl_bits => "DO" FrameSync */
	fs_act = (fs_mgr.validSync_bits == fs_mgr.pf_ctrl_bits) ? 1 : 0;

	/* add for checking all ctrl needed by FrameSync have been setup */
	if (fs_act == 1) {
		fs_act = (fs_mgr.pf_ctrl_bits == fs_mgr.setup_complete_bits)
			? 1 : 0;

		if (fs_act == 0) {
			LOG_INF(
				"WARNING: sensor ctrl has not been setup yet, validSync:%2d, pf_ctrl:%2d, setup_complete:%2d\n",
				fs_mgr.validSync_bits,
				fs_mgr.pf_ctrl_bits,
				fs_mgr.setup_complete_bits);
		}
	}


	if (fs_act == 1) {
		LOG_INF("fs_activate:%u, validSync:%2d, pf_ctrl:%2d, cnt:%u\n",
			fs_act,
			fs_mgr.validSync_bits,
			fs_mgr.pf_ctrl_bits,
			++fs_mgr.act_cnt);


		/* 1 pick up validSync sensor information correctlly */
		for (i = 0; i < SENSOR_MAX_NUM; ++i) {
			if (check_bit(i, fs_mgr.validSync_bits) == 1) {
				/* pick up sensor registered location */
				solveIdxs[len++] = i;
			}
		}


		/* 2 run frame sync proc to solve frame length */
		/*   if no error */
		/*       update framelength to frame recorder and then */
		/*       use callback function to set framelength to sensor */
		ret = fs_run_frame_sync_proc(solveIdxs, fl_lc, len);
		if (ret == 0) { // 0 => No error
			for (i = 0; i < len; ++i) {
				unsigned int idx = solveIdxs[i];

				/* set framelength */
				/* all framelength operation must use this API */
				fs_set_framelength_lc(idx, fl_lc[idx]);
			}
		}

	} else {
		LOG_INF(
			"wait for other pf_ctrl setup, validSync:%2d, pf_ctrl:%2d, setup_complete:%2d\n",
			fs_mgr.validSync_bits,
			fs_mgr.pf_ctrl_bits,
			fs_mgr.setup_complete_bits);
	}


	return fs_act;
}


static void
fs_check_frame_sync_ctrl(unsigned int idx, struct fs_perframe_st (*frameCtrl))
{
	/* before doing frame sync, check some situation */
	unsigned int ret = 0;


	/* 1. check this sensor is valid for sync or not */
	ret = check_bit(idx, fs_mgr.validSync_bits);
	if (ret == 0) {

#ifndef REDUCE_FS_DRV_LOG
		LOG_PR_WARN(
			"WARNING: Not valid for doing sync. [%u] ID:%#x\n",
			idx,
			frameCtrl->sensor_id);
#endif // REDUCE_FS_DRV_LOG

		return;
	}


	/* 2. check this perframe ctrl is valid or not */
	ret = check_bit(idx, fs_mgr.pf_ctrl_bits);
	if (ret == 1) {
		LOG_PR_WARN(
			"WARNING: Set same sensor, return. [%u] ID:%#x  [pf_ctrl_bits:%u]\n",
			idx,
			frameCtrl->sensor_id,
			fs_mgr.pf_ctrl_bits);

		return;

	} else {
		write_bit(idx, 1, &fs_mgr.pf_ctrl_bits);
		fs_set_cb_cmd_id(idx, frameCtrl->cmd_id);


#ifdef FS_SENSOR_CCU_IT
		/* for frame-sync single cam IT */
		fs_single_cam_IT(idx, frameCtrl->lineTimeInNs);
#endif // FS_SENSOR_CCU_IT
	}


	/* trigger framesync at ::fs_sync_frame(0) */
	/* 3. check for running frame sync processing or not */
	// fs_try_trigger_frame_sync();
}


/*******************************************************************************
 * input:
 *     fs_perframe_st (*frameCtrl)
 *
 * description:
 *     call by sensor driver,
 *     fs_perframe_st is a settings for a certain sensor.
 ******************************************************************************/
void fs_set_shutter(struct fs_perframe_st (*frameCtrl))
{
	unsigned int ident = 0, idx = 0;


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		ident = frameCtrl->sensor_id;
		break;

	case BY_SENSOR_IDX:
		ident = frameCtrl->sensor_idx;
		break;

	default:
		ident = frameCtrl->sensor_idx;
		break;
	}


	idx = fs_get_reg_sensor_pos(ident);

	if (check_sensorIdx(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}

	/* check streaming status, due to maybe calling by non-sync flow */
	if (check_bit(idx, fs_mgr.streaming_bits) == 0) {
		LOG_PR_WARN("ERROR: [%u] is stream off. ID:%#x(sidx:%u)\n",
			idx,
			frameCtrl->sensor_id,
			frameCtrl->sensor_idx);

		return;
	}


	//fs_dump_pf_info(frameCtrl);


	/* 1. set perframe ctrl data to fs algo */
	fs_alg_set_perframe_st_data(idx, frameCtrl);
	/* TODO: anti flicker flow may not be in here */
	// fs_alg_set_anti_flicker(idx, frameCtrl->flicker_en);


#ifdef FS_UT
	/* only do when FS_UT */
	/* In normal run, use data send from sensor driver directly */
	/* 2. call fs algo api to simulate the result of write_shutter() */
	frameCtrl->out_fl_lc = fs_alg_write_shutter(idx);
#endif // FS_UT


	/* 3. push frame settings into frame recorder */
	frec_push_record(idx, frameCtrl->shutter_lc, frameCtrl->out_fl_lc);


	/* 4. frame sync ctrl */
	fs_check_frame_sync_ctrl(idx, frameCtrl);


#ifdef USING_V4L2_CTRL_REQUEST_SETUP
	/* if this is the last ctrl needed by FrameSync, notify setup complete*/
	/* for N3D and not using v4l2_ctrl_request_setup*/
	fs_notify_sensor_ctrl_setup_complete(idx, frameCtrl->sensor_id);
#endif // USING_V4L2_CTRL_REQUEST_SETUP
}
/******************************************************************************/





/******************************************************************************/
// Frame Sync entry function
/******************************************************************************/
// export some Frame Sync member function.
static struct FrameSync frameSync = {
	fs_streaming,
	fs_set_sync,
	fs_sync_frame,
	fs_set_shutter,
	fs_update_tg,
	fs_update_auto_flicker_mode,
	fs_update_min_framelength_lc,
	fs_is_set_sync
};


/*******************************************************************************
 * Frame Sync init function.
 *
 *    init FrameSync object.
 *    get FrameSync function for operation.
 *
 *    return: (0 / 1) => (no error / error)
 ******************************************************************************/
unsigned int FrameSyncInit(struct FrameSync (**pframeSync))
{
	if (pframeSync != NULL) {
		fs_init();
		*pframeSync = &frameSync;
		//LOG_INF("Init.\n");
		return 0;
	}

	// maybe add a correct error type.
	return 1;
}
/******************************************************************************/
