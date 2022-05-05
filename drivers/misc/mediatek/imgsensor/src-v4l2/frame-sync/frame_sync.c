// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#if !defined(FS_UT)
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#endif // FS_UT

#include "frame_sync.h"
#include "frame_sync_camsys.h"
#include "frame_sync_algo.h"
#include "frame_monitor.h"

#if !defined(FS_UT)
#include "frame_sync_console.h"
#include "hw_sensor_sync_algo.h"
#endif // FS_UT


/******************************************************************************/
// Log message
/******************************************************************************/
#include "frame_sync_log.h"

#define REDUCE_FS_DRV_LOG
#define PFX "FrameSync"
/******************************************************************************/


/******************************************************************************/
// Mutex Lock
/******************************************************************************/
#ifndef ALL_USING_ATOMIC
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
#endif // ALL_USING_ATOMIC
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
#ifndef ALL_USING_ATOMIC
	unsigned int reg_cnt;
#else
	FS_Atomic_T reg_cnt;
#endif // ALL_USING_ATOMIC

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
#ifndef ALL_USING_ATOMIC
	enum FS_STATUS fs_status;
#else
	FS_Atomic_T fs_status;
#endif // ALL_USING_ATOMIC

	unsigned int user_counter;              // also fs_init() cnt

#ifndef ALL_USING_ATOMIC
	unsigned int streaming_bits;            // can do set / unset
	unsigned int enSync_bits;               // can do set / unset

	unsigned int validSync_bits;            // for checking PF status
	unsigned int pf_ctrl_bits;              // for checking PF status
	unsigned int last_pf_ctrl_bits;         // for checking PF status


#ifdef USING_CCU
	unsigned int power_on_ccu_bits;         // for checking CCU pw ON/OFF
#endif // USING_CCU


	/* ctrl needed by FS have been setup */
	unsigned int setup_complete_bits;
	unsigned int last_setup_complete_bits;
#else // ALL_USING_ATOMIC

	FS_Atomic_T streaming_bits;
	FS_Atomic_T enSync_bits;

	FS_Atomic_T validSync_bits;
	FS_Atomic_T pf_ctrl_bits;
	FS_Atomic_T last_pf_ctrl_bits;


#ifdef USING_CCU
	FS_Atomic_T power_on_ccu_bits;
#endif // USING_CCU


	FS_Atomic_T setup_complete_bits;
	FS_Atomic_T last_setup_complete_bits;
#endif // ALL_USING_ATOMIC


	/* for different fps sync sensor sync together */
	/* e.g. fps: (60 vs 30) */
	/* => frame_cell_size: 2 */
	/* => frame_tag: 0, 1, 0, 1, 0, ... */
	enum FS_FEATURE_MODE ft_mode[SENSOR_MAX_NUM];
	unsigned int frame_cell_size[SENSOR_MAX_NUM];
	unsigned int frame_tag[SENSOR_MAX_NUM];


	/* keep for supporting trigger by ext vsync */
	struct fs_perframe_st pf_ctrl[SENSOR_MAX_NUM];


	/* Frame Settings Recorder */
	struct FrameRecorder frm_recorder[SENSOR_MAX_NUM];


	/* call back */
	struct callback_st cb_data[SENSOR_MAX_NUM];


	/* fs act cnt (for ctrl pair trigger) */
	unsigned int act_cnt;


#ifdef SUPPORT_FS_NEW_METHOD
	unsigned int user_set_sa:1;             // user config using SA
	FS_Atomic_T using_sa_ver;               // flag - using standalone ver
	FS_Atomic_T sa_bits;                    // for needing standalone ver
	FS_Atomic_T sa_method;                  // 0:adaptive switch
	FS_Atomic_T master_idx;                 // SA master idx
#endif // SUPPORT_FS_NEW_METHOD
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
#ifndef ALL_USING_ATOMIC
static inline void change_fs_status(enum FS_STATUS status)
{
	FS_MUTEX_LOCK(&gStatusLocker);

	fs_mgr.fs_status = status;

	FS_MUTEX_UNLOCK(&gStatusLocker);
}


static inline enum FS_STATUS get_fs_status(void)
{
	enum FS_STATUS status;


	FS_MUTEX_LOCK(&gStatusLocker);

	status = fs_mgr.fs_status;

	FS_MUTEX_UNLOCK(&gStatusLocker);


	return status;
}


#else // ifndef ALL_USING_ATOMIC
static inline void change_fs_status(enum FS_STATUS status)
{
	FS_ATOMIC_SET((int)status, &fs_mgr.fs_status);
}


/*
 * return: enum FS_STATUS value
 *     FS_STATUS_UNKNOWN:
 *         when reading atomic value with
 *         the value is out of valid range.
 */
static inline enum FS_STATUS get_fs_status(void)
{
	enum FS_STATUS status;
	int val = FS_ATOMIC_READ(&fs_mgr.fs_status);


	/* check value valid for enum */
	if (val >= FS_NONE && val < FS_STATUS_UNKNOWN)
		status = (enum FS_STATUS)val;
	else {
		LOG_INF("ERROR: get status val:%d, treat as FS_STATUS_UNKNOWN\n",
			val);

		status = FS_STATUS_UNKNOWN;
	}

	return status;
}
#endif // ALL_USING_ATOMIC


/*
 * return: (0/1) for (non-valid/valid)
 */
static inline unsigned int check_idx_valid(unsigned int sensor_idx)
{
	return (sensor_idx < SENSOR_MAX_NUM) ? 1 : 0;
}


#ifndef ALL_USING_ATOMIC
static inline unsigned int check_bit(unsigned int idx, unsigned int val)
{
	unsigned int ret = check_idx_valid(idx);


	FS_MUTEX_LOCK(&gBitLocker);

	if (ret == 1)
		val = ((val >> idx) & 0x01);

	FS_MUTEX_UNLOCK(&gBitLocker);


	return (ret == 1) ? val : 0xFFFFFFFF;
}


static unsigned int write_bit(
	unsigned int idx, unsigned int en, unsigned int (*val))
{
	unsigned int ret = check_idx_valid(idx);


	FS_MUTEX_LOCK(&gBitLocker);

	if (ret == 1) {
		if (en > 0)
			(*val |= (0x01 << idx));
		else
			(*val &= ~(0x01 << idx));
	}

	FS_MUTEX_UNLOCK(&gBitLocker);


	// TODO : if sensor idx is wrong, do error handle?
	return ret;
}


static inline void clear_all_bit(unsigned int (*val))
{
	FS_MUTEX_LOCK(&gBitLocker);

	*val = 0;

	FS_MUTEX_UNLOCK(&gBitLocker);
}
#endif // ALL_USING_ATOMIC


#if defined(SUPPORT_FS_NEW_METHOD) || defined(ALL_USING_ATOMIC)
/*
 * return: (0/1) or 0xFFFFFFFF
 *     0xFFFFFFFF: when check_idx_valid() return error
 */
static inline unsigned int check_bit_atomic(
	unsigned int idx, FS_Atomic_T *p_fs_atomic_val)
{
	unsigned int ret = check_idx_valid(idx);
	unsigned int result = 0;


	if (ret == 1) {
		result = FS_ATOMIC_READ(p_fs_atomic_val);
		result = ((result >> idx) & 1UL);
	}

	return (ret == 1) ? (result) : 0xFFFFFFFF;
}


/*
 * return: 1 or 0xFFFFFFFF
 *     0xFFFFFFFF: when check_idx_valid() return error
 */
static inline unsigned int write_bit_atomic(
	unsigned int idx, unsigned int en, FS_Atomic_T *p_fs_atomic_val)
{
	unsigned int ret = check_idx_valid(idx);


	if (ret == 1) {
		/* en > 0 => set ; en == 0 => clear */
		if (en > 0)
			FS_ATOMIC_FETCH_OR((1UL << idx), p_fs_atomic_val);
		else
			FS_ATOMIC_FETCH_AND((~(1UL << idx)), p_fs_atomic_val);
	}

	return (ret == 1) ? (ret) : 0xFFFFFFFF;
}


static inline void clear_all_bit_atomic(FS_Atomic_T *p_fs_atomic_val)
{
	FS_ATOMIC_SET(0, p_fs_atomic_val);
}
#endif // SUPPORT_FS_NEW_METHOD
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
static inline unsigned int compare_sensor_id(
	unsigned int id1, unsigned int id2)
{
	return (id1 == id2 && id1 != 0) ? 1 : 0;
}


static inline unsigned int compare_sensor_idx(
	unsigned int idx1, unsigned int idx2)
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


static inline void fs_get_reg_sensor_info(
	unsigned int idx, struct SensorInfo *pInfo)
{
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;

	if (pInfo && check_idx_valid(idx)) {
		pInfo->sensor_id = pSensorTable->sensors[idx].sensor_id;
		pInfo->sensor_idx = pSensorTable->sensors[idx].sensor_idx;
	} else {
		LOG_INF(
			"ERROR: pInfo %p or/and idx:%u is/are not valid\n",
			pInfo, idx);
	}
}


/*
 * return: uint_t or 0xffffffff
 *     uint_t: array position for the registered sensor save in.
 *     0xffffffff: not found this sensor ID in "SensorTable".
 */
static unsigned int fs_search_reg_sensors(
	struct SensorInfo (*sensor_info),
	enum CHECK_SENSOR_INFO_METHOD method)
{
	unsigned int i = 0;

#ifndef ALL_USING_ATOMIC
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;
	unsigned int (*pRegCnt) = &pSensorTable->reg_cnt;

	for (i = 0; i < (*pRegCnt); ++i) {
#else
	int reg_cnt = FS_ATOMIC_READ(&fs_mgr.reg_table.reg_cnt);

	for (i = 0; i < reg_cnt; ++i) {
#endif // ALL_USING_ATOMIC
		struct SensorInfo (*pInfo) = &fs_mgr.reg_table.sensors[i];


		if (check_sensor_info(pInfo, sensor_info, method))
			return i;
	}

	return 0xffffffff;
}


/*
 * return: uint_t or 0xffffffff
 *     uint_t: array position for the registered sensor save in.
 *     0xffffffff: reach maximum register capacity, couldn't register it.
 *
 * !!! You have check the return idx before using it. !!!
 */
#ifndef ALL_USING_ATOMIC
static unsigned int fs_push_sensor(struct SensorInfo (*sensor_info))
{
	unsigned int *pRegCnt = NULL;
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;


	FS_MUTEX_LOCK(&gRegisterLocker);


	pRegCnt = &pSensorTable->reg_cnt;

	/* check if reach maximum capacity */
	if (*pRegCnt < SENSOR_MAX_NUM)
		pSensorTable->sensors[(*pRegCnt)++] = *sensor_info;
	else
		goto error_sensor_max_count;


	FS_MUTEX_UNLOCK(&gRegisterLocker);


	return (*pRegCnt - 1);


error_sensor_max_count:

	FS_MUTEX_UNLOCK(&gRegisterLocker);

	return 0xffffffff;
}
#else // ifndef ALL_USING_ATOMIC
static inline unsigned int fs_push_sensor(struct SensorInfo (*sensor_info))
{
	struct SensorTable (*pSensorTable) = &fs_mgr.reg_table;
	FS_Atomic_T *p_fs_atomic_val = &fs_mgr.reg_table.reg_cnt;
	int cnt_now = 0, cnt_new = 0;


	do {
		cnt_now = FS_ATOMIC_READ(p_fs_atomic_val);
		cnt_new = cnt_now + 1;

		/* boundary check and check if reach maximum capacity */
		if (!((cnt_now >= 0) && (cnt_now < SENSOR_MAX_NUM)))
			return 0xFFFFFFFF;
#ifdef FS_UT
	} while (!atomic_compare_exchange_weak(
			p_fs_atomic_val, &cnt_now, cnt_new));
#else
	} while (atomic_cmpxchg(p_fs_atomic_val, cnt_now, cnt_new) != cnt_now);
#endif // FS_UT


	/* => cnt_now is correct and avalible */
	pSensorTable->sensors[(unsigned int)cnt_now] = *sensor_info;

	return cnt_now;
}
#endif // ALL_USING_ATOMIC


/*
 * return: uint_t or 0xffffffff
 *     uint_t: array position for the registered sensor save in.
 *     0xffffffff: couldn't register sensor.
 *                 (reach maximum capacity or sensor ID is 0)
 *
 * !!! You have better to check the return idx before using it. !!!
 */
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
		if (check_idx_valid(idx) == 0) {
			LOG_PR_WARN("ERROR: Reach max sensor capacity\n");
			return idx;
		}

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_INF(
			"ID:%#x(sidx:%u), register it (idx:%u), method:%s\n",
			sensor_info->sensor_id,
			sensor_info->sensor_idx,
			idx,
			REG_INFO);
#endif // REDUCE_FS_DRV_LOG

	} else {
		/* 2-2. this sensor has been registered, do nothing */
		/* log print info */
		fs_get_reg_sensor_info(idx, &info);

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_INF("ID:%#x(sidx:%u), idx:%u, method:%s, already registered\n",
			info.sensor_id,
			info.sensor_idx,
			idx,
			REG_INFO);
#endif // REDUCE_FS_DRV_LOG


		if (method == BY_SENSOR_IDX) {
			if (fs_mgr.reg_table.sensors[idx].sensor_id !=
				sensor_info->sensor_id) {

				LOG_INF(
					"Overwrite to... [%u] ID:%#x(sidx:%u), method:%s\n",
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


/*
 * return: uint_t or 0xffffffff
 *     uint_t: array position for the registered sensor save in.
 *     0xffffffff: not found this sensor idx in "SensorTable".
 *
 * input:
 *     ident: sensor ID / sensor idx
 */
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


static inline void fs_init_cb_data(
	unsigned int idx, void *p_ctx, callback_set_framelength func_ptr)
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
	struct SensorInfo info = {0};

	/* log print info */
	fs_get_reg_sensor_info(idx, &info);

	LOG_INF(
		"[%u] ID:%#x(sidx:%u) recs:(at %u) (0:%u/%u), (1:%u/%u), (2:%u/%u), (3:%u/%u) (fl_lc/shut_lc)\n",
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
		pFrameRecord->frame_recs[2].shutter_lc,
		pFrameRecord->frame_recs[3].framelength_lc,
		pFrameRecord->frame_recs[3].shutter_lc);
}


static inline void frec_reset_recorder(unsigned int idx)
{
	unsigned int i = 0;

	struct FrameRecord clear_st = {0};
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif // REDUCE_FS_DRV_LOG


	/* all FrameRecorder member variables set to 0 */
	pFrameRecord->init = 0;
	pFrameRecord->depthIdx = 0;

	for (i = 0; i < RECORDER_DEPTH; ++i)
		pFrameRecord->frame_recs[i] = clear_st;


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	fs_get_reg_sensor_info(idx, &info);
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


/*
 * Notify fs algo and frame monitor the data in the frame recorder have been
 * updated
 *
 * This function should be call after having any frame recorder operation
 *
 *
 * description:
 *     fs algo will use these information to predict current and
 *     next framelength when calculating vsync diff.
 */
static void frec_notify_setting_frame_record_st_data(unsigned int idx)
{
	unsigned int i = 0;

	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];
	unsigned int depthIdx = pFrameRecord->depthIdx;

	struct frame_record_st recs[RECORDER_DEPTH];


	/* 1. prepare frame settings in the recorder */
	/*    => 0:newest, 1:second, 2:third */
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


/*
 * description:
 *     update / frec data for next calculation/calibration
 *
 * input:
 *     idx: sensor register position in sensor table.
 *     shutter_lc: shutter lc you want to update to frec (> 0 will update)
 *     fl_lc: frame length lc you want to update to frec (> 0 will update)
 */
static void frec_update_shutter_fl_lc(unsigned int idx,
	unsigned int shutter_lc, unsigned int fl_lc)
{
	struct SensorInfo info = {0}; // for log using
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];

	unsigned int curr_depth = pFrameRecord->depthIdx;


	/* log print info */
	fs_get_reg_sensor_info(idx, &info);


	/* ring back to point to current data records */
	curr_depth = ((curr_depth + (RECORDER_DEPTH-1)) % (RECORDER_DEPTH));


	/* set / update shutter/framelength lc */
	if (shutter_lc > 0)
		pFrameRecord->frame_recs[curr_depth].shutter_lc = shutter_lc;

	if (fl_lc > 0)
		pFrameRecord->frame_recs[curr_depth].framelength_lc = fl_lc;

	if ((shutter_lc == 0) && (fl_lc == 0)) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u) get: %u/%u => recs[*%u] = *%u/%u (fl_lc/shut_lc), don't update frec data\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			fl_lc,
			shutter_lc,
			curr_depth,
			pFrameRecord->frame_recs[curr_depth].framelength_lc,
			pFrameRecord->frame_recs[curr_depth].shutter_lc);
	}


#ifndef REDUCE_FS_DRV_LOG
	LOG_INF(
		"[%u] ID:%#x(sidx:%u) get: %u/%u => recs[*%u] = *%u/%u (fl_lc/shut_lc)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		fl_lc,
		shutter_lc,
		curr_depth,
		pFrameRecord->frame_recs[curr_depth].framelength_lc,
		pFrameRecord->frame_recs[curr_depth].shutter_lc);
#endif // REDUCE_FS_DRV_LOG


#ifndef REDUCE_FS_DRV_LOG
	frec_dump_recorder(idx);
#endif //REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


static inline void frec_push_def_shutter_fl_lc(
	unsigned int idx, unsigned int shutter_lc, unsigned int fl_lc)
{
#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif // REDUCE_FS_DRV_LOG


	unsigned int i = 0;

	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];


	/* case handling */
	if (pFrameRecord->init) {
		LOG_MUST(
			"NOTICE: [%u] frec was initialized:%u, auto return [get %u/%u (fl_lc/shut_lc)]\n",
			idx,
			pFrameRecord->init,
			fl_lc, shutter_lc);

		frec_dump_recorder(idx);
		return;
	}
	pFrameRecord->init = 1;


	/* init all frec value to default shutter and framelength */
	for (i = 0; i < RECORDER_DEPTH; ++i) {
		pFrameRecord->frame_recs[i].shutter_lc = shutter_lc;
		pFrameRecord->frame_recs[i].framelength_lc = fl_lc;
	}


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	fs_get_reg_sensor_info(idx, &info);
	LOG_INF("[%u] ID:%#x(sidx:%u) frame recorder initialized:%u, with def(exp:%u/fl:%u)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		pFrameRecord->init,
		shutter_lc,
		fl_lc);

	frec_dump_recorder(idx);
#endif // REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


static inline void frec_push_shutter_fl_lc(
	unsigned int idx, unsigned int shutter_lc, unsigned int fl_lc)
{
	struct FrameRecorder (*pFrameRecord) = &fs_mgr.frm_recorder[idx];
	unsigned int (*pDepthIdx) = &pFrameRecord->depthIdx;


#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
	unsigned int bufDepthIdx = (*pDepthIdx);	// for log using
#endif // REDUCE_FS_DRV_LOG


	/* push shutter_lc and framelength_lc if are not equal to 0 */
	if (shutter_lc > 0)
		pFrameRecord->frame_recs[*pDepthIdx].shutter_lc = shutter_lc;
	if (fl_lc > 0)
		pFrameRecord->frame_recs[*pDepthIdx].framelength_lc = fl_lc;

	/* depth idx ring forward */
	(*pDepthIdx) = (((*pDepthIdx) + 1) % RECORDER_DEPTH);


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	fs_get_reg_sensor_info(idx, &info);
	LOG_INF(
		"[%u] ID:%#x(sidx:%u) get fl_lc:%u/shut_lc:%u => recs[%u] = %u/%u (fl_lc/shut_lc), depthIdx update to %u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		fl_lc, shutter_lc,
		bufDepthIdx,
		pFrameRecord->frame_recs[bufDepthIdx].framelength_lc,
		pFrameRecord->frame_recs[bufDepthIdx].shutter_lc,
		(*pDepthIdx));
#endif // REDUCE_FS_DRV_LOG


	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);
}


static inline void frec_push_record(
	unsigned int idx, unsigned int shutter_lc, unsigned int framelength_lc)
{
	if (fs_mgr.frm_recorder[idx].init == 0) {
		struct SensorInfo info = {0};

		// TODO : add error handle ?

		/* log print info */
		fs_get_reg_sensor_info(idx, &info);

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


#if defined(QUERY_CCU_TS_AT_SOF)
/*
 * be careful:
 *    only call this API after all frame settings/info are be done!
 *    owning to this API will push a frame settings into recorder.
 *
 *    e.g. call this API after doing bellow processing
 *       setup frame monitor frame measurement,
 *       dump fs algo info, ..., etc
 */
static void frec_notify_vsync(unsigned int idx)
{
	/* push previous frame settings into frame recorder */
	frec_push_record(idx,
		fs_mgr.pf_ctrl[idx].shutter_lc,
		fs_mgr.pf_ctrl[idx].out_fl_lc);


#if !defined(REDUCE_FS_DRV_LOG)
	/* log */
	frec_dump_recorder(idx);
#endif // REDUCE_FS_DRV_LOG
}
#endif // QUERY_CCU_TS_AT_SOF
/******************************************************************************/





/******************************************************************************/
// Frame Sync Mgr function
/******************************************************************************/
#ifdef SUPPORT_FS_NEW_METHOD
static inline void fs_init_members(void)
{
#ifdef ALL_USING_ATOMIC
	FS_ATOMIC_INIT(0, &fs_mgr.reg_table.reg_cnt);
	FS_ATOMIC_INIT(0, &fs_mgr.fs_status);
	FS_ATOMIC_INIT(0, &fs_mgr.streaming_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.enSync_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.validSync_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.pf_ctrl_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.last_pf_ctrl_bits);
#if defined(USING_CCU)
	FS_ATOMIC_INIT(0, &fs_mgr.power_on_ccu_bits);
#endif // USING_CCU
	FS_ATOMIC_INIT(0, &fs_mgr.setup_complete_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.last_setup_complete_bits);
#endif // ALL_USING_ATOMIC
	FS_ATOMIC_INIT(0, &fs_mgr.using_sa_ver);
	FS_ATOMIC_INIT(0, &fs_mgr.sa_bits);
	FS_ATOMIC_INIT(0, &fs_mgr.sa_method);
	FS_ATOMIC_INIT(MASTER_IDX_NONE, &fs_mgr.master_idx);
}
#endif // SUPPORT_FS_NEW_METHOD


static inline void fs_init(void)
{
	enum FS_STATUS status = get_fs_status();

	fs_mgr.user_counter++;

	if (status == FS_NONE) {
#ifdef SUPPORT_FS_NEW_METHOD
		fs_init_members();
#endif // SUPPORT_FS_NEW_METHOD

		change_fs_status(FS_INITIALIZED);

		LOG_MUST("FrameSync init. (User:%u)\n", fs_mgr.user_counter);
	} else if (status == FS_INITIALIZED)
		LOG_MUST("Initialized. (User:%u)\n", fs_mgr.user_counter);

	// else if () => for re-init.
}


#ifdef SUPPORT_FS_NEW_METHOD
void fs_sa_set_sa_method(enum FS_SA_METHOD method)
{
	FS_ATOMIC_SET((int)method, &fs_mgr.sa_method);
}


/*
 * Support user/custom setting for using FrameSync StandAlone(SA) mode
 */
void fs_set_using_sa_mode(unsigned int en)
{
	fs_mgr.user_set_sa = (en > 0) ? 1 : 0;
}


static inline unsigned int fs_user_sa_config(void)
{
#ifdef FORCE_USING_SA_MODE
	return 1;
#else
	return fs_mgr.user_set_sa;
#endif // FORCE_USING_SA_MODE
}
#endif // SUPPORT_FS_NEW_METHOD


#ifndef ALL_USING_ATOMIC
static unsigned int fs_update_status(unsigned int idx, unsigned int flag)
{
	unsigned int cnt = 0;
	enum FS_STATUS status = 0;


	FS_MUTEX_LOCK(&gBitLocker);


	/* update validSync_bits value */
	fs_mgr.validSync_bits =
		fs_mgr.streaming_bits &
		fs_mgr.enSync_bits;


	/* change status or not by counting the number of valid sync sensors */
	cnt = FS_POPCOUNT(fs_mgr.validSync_bits);


	if (cnt > 1 && cnt <= SENSOR_MAX_NUM)
		change_fs_status(FS_WAIT_FOR_SYNCFRAME_START);
	else
		change_fs_status(FS_INITIALIZED);

	status = get_fs_status();


	FS_MUTEX_UNLOCK(&gBitLocker);


#ifndef USING_CCU
	LOG_INF(
		"stat:%u, ready:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u, setup_complete:%u, ft_mode:%u [idx:%u, en:%u]\n",
		status,
		cnt,
		fs_mgr.streaming_bits,
		fs_mgr.enSync_bits,
		fs_mgr.validSync_bits,
		fs_mgr.pf_ctrl_bits,
		fs_mgr.setup_complete_bits,
		fs_mgr.ft_mode[idx],
		idx,
		flag);
#else
	LOG_INF(
		"stat:%u, ready:%u, streaming:%u, enSync:%u, validSync:%u, pf_ctrl:%u, setup_complete:%u, ft_mode:%u, pw_ccu:%u(cnt:%u) [idx:%u, en:%u]\n",
		status,
		cnt,
		fs_mgr.streaming_bits,
		fs_mgr.enSync_bits,
		fs_mgr.validSync_bits,
		fs_mgr.pf_ctrl_bits,
		fs_mgr.setup_complete_bits,
		fs_mgr.ft_mode[idx],
		fs_mgr.power_on_ccu_bits,
		frm_get_ccu_pwn_cnt(),
		idx,
		flag);
#endif // USING_CCU


	return cnt;
}


static inline void fs_set_status_bits(
	unsigned int idx, unsigned int flag, unsigned int (*bits))
{
	if (flag > 0)
		write_bit(idx, 1, bits);
	else
		write_bit(idx, 0, bits);

	//unsigned int validSyncCnt = fs_update_status(sensor_idx, flag);
	fs_update_status(idx, flag);
}


#else // ifndef ALL_USING_ATOMIC
static inline int fs_update_valid_sync_bit(void)
{
	int streaming = 0, en_sync = 0;

	streaming = FS_ATOMIC_READ(&fs_mgr.streaming_bits);
	en_sync = FS_ATOMIC_READ(&fs_mgr.enSync_bits);

	FS_ATOMIC_SET((streaming & en_sync), &fs_mgr.validSync_bits);

	return (streaming & en_sync);
}


static void fs_update_status(unsigned int idx, unsigned int flag)
{
	unsigned int cnt = 0;
	int valid_sync = 0;
	enum FS_STATUS status = 0;


	/* update validSync_bits value */
	valid_sync = fs_update_valid_sync_bit();


	/* change status or not by counting the number of valid sync sensors */
	cnt = FS_POPCOUNT(valid_sync);

	if (cnt > 1 && cnt <= SENSOR_MAX_NUM)
		change_fs_status(FS_WAIT_FOR_SYNCFRAME_START);
	else
		change_fs_status(FS_INITIALIZED);

	status = get_fs_status();


	/* only 'on' stage print the log */
	// if (!flag)
	//	return;

#ifndef USING_CCU
	LOG_INF(
		"stat:%u, ready:%u, streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d, setup_complete:%d, ft_mode:%u, SA(%u/%u/%u) [idx:%u, en:%u]\n",
		status,
		cnt,
		FS_ATOMIC_READ(&fs_mgr.streaming_bits),
		FS_ATOMIC_READ(&fs_mgr.enSync_bits),
		FS_ATOMIC_READ(&fs_mgr.validSync_bits),
		FS_ATOMIC_READ(&fs_mgr.pf_ctrl_bits),
		FS_ATOMIC_READ(&fs_mgr.setup_complete_bits),
		fs_mgr.ft_mode[idx],
		FS_ATOMIC_READ(&fs_mgr.using_sa_ver),
		fs_user_sa_config(),
		FS_ATOMIC_READ(&fs_mgr.sa_bits),
		idx,
		flag);
#else
	LOG_MUST(
		"stat:%u, ready:%u, streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d, setup_complete:%d, ft_mode:%u, SA(%u/%u/%u), pw_ccu:%d(cnt:%u) [idx:%u, en:%u]\n",
		status,
		cnt,
		FS_ATOMIC_READ(&fs_mgr.streaming_bits),
		FS_ATOMIC_READ(&fs_mgr.enSync_bits),
		FS_ATOMIC_READ(&fs_mgr.validSync_bits),
		FS_ATOMIC_READ(&fs_mgr.pf_ctrl_bits),
		FS_ATOMIC_READ(&fs_mgr.setup_complete_bits),
		fs_mgr.ft_mode[idx],
		FS_ATOMIC_READ(&fs_mgr.using_sa_ver),
		fs_user_sa_config(),
		FS_ATOMIC_READ(&fs_mgr.sa_bits),
		FS_ATOMIC_READ(&fs_mgr.power_on_ccu_bits),
		frm_get_ccu_pwn_cnt(),
		idx,
		flag);
#endif // USING_CCU
}


static inline void fs_set_status_bits(
	unsigned int idx, unsigned int flag, FS_Atomic_T *bits)
{
	// TODO: add a lock to keep atomic op consistency.
	if (flag > 0)
		write_bit_atomic(idx, 1, bits);
	else
		write_bit_atomic(idx, 0, bits);

	// fs_update_status(idx, flag);
	// TODO: add a unlock to keep atomic op consistency.
}
#endif // ALL_USING_ATOMIC


static inline void fs_set_stream(unsigned int idx, unsigned int flag)
{
	fs_set_status_bits(idx, flag, &fs_mgr.streaming_bits);
}


static inline void fs_reset_ft_mode_data(unsigned int idx, unsigned int flag)
{
	if (flag > 0)
		return;

	fs_mgr.ft_mode[idx] = 0;
	fs_mgr.frame_cell_size[idx] = 0;
	fs_mgr.frame_tag[idx] = 0;
}


static void fs_reset_perframe_stage_data(
	const unsigned int idx, const unsigned int flag)
{
	struct fs_perframe_st clear_st = {0};

	if (flag > 0)
		return;

	fs_mgr.pf_ctrl[idx] = clear_st;

	fs_alg_reset_vsync_data(idx);
}


#ifdef SUPPORT_FS_NEW_METHOD
static inline void fs_check_sync_need_sa_mode(
	unsigned int idx, enum FS_FEATURE_MODE flag)
{
#if !defined(REDUCE_FS_DRV_LOG)
	struct SensorInfo info = {0}; // for log using
#endif // REDUCE_FS_DRV_LOG


	/* NOT FS_FT_MODE_NORMAL => using SA mode */
	if (flag != FS_FT_MODE_NORMAL) {
		write_bit_atomic(idx, 1, &fs_mgr.sa_bits);

#if !defined(REDUCE_FS_DRV_LOG)
		fs_get_reg_sensor_info(idx, &info);
		LOG_INF(
			"[%u] ID:%#x(sidx:%u), ft_mode:%u(need SA mode, except 0 for normal)   [sa_bits:%d]\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			flag,
			FS_ATOMIC_READ(&fs_mgr.sa_bits));
#endif // REDUCE_FS_DRV_LOG
	}
}


static inline void fs_decision_maker(unsigned int idx)
{
	int sa_bits_now = 0;
	int need_sa_ver = 0;
	int user_sa_en = 0;


	/* 1. check sync tag of the sensor need use SA mode or not */
	fs_check_sync_need_sa_mode(idx, fs_mgr.ft_mode[idx]);


	/* 2. check all sensor for using SA mode or not */
	do {
		sa_bits_now = FS_ATOMIC_READ(&fs_mgr.sa_bits);

		need_sa_ver = (FS_POPCOUNT(sa_bits_now) > 0) ? 1 : 0;

#ifdef FS_UT
	} while (!atomic_compare_exchange_weak(&fs_mgr.sa_bits,
			&sa_bits_now, sa_bits_now));
#else
	} while (atomic_cmpxchg(&fs_mgr.sa_bits,
			sa_bits_now, sa_bits_now) != sa_bits_now);
#endif // FS_UT


	/* 3. check user/custom SA config EN */
	user_sa_en = fs_user_sa_config();


	/* X. setup result of using SA mode or not */
	if (need_sa_ver > 0 || user_sa_en > 0)
		FS_ATOMIC_SET(1, &fs_mgr.using_sa_ver);
	else
		FS_ATOMIC_SET(0, &fs_mgr.using_sa_ver);


#if !defined(FORCE_USING_SA_MODE) || !defined(REDUCE_FS_DRV_LOG)
	LOG_MUST(
		"using_sa:%d (user_sa_en:%u), ft_mode:%u, sa_bits:%d [idx:%u]\n",
		FS_ATOMIC_READ(&fs_mgr.using_sa_ver),
		user_sa_en,
		fs_mgr.ft_mode[idx],
		FS_ATOMIC_READ(&fs_mgr.sa_bits),
		idx);
#endif
}


static inline void fs_sa_try_reset_master_idx(unsigned int idx)
{
	int master_idx = 0;

	do {
		master_idx = FS_ATOMIC_READ(&fs_mgr.master_idx);

#ifdef FS_UT
	} while (!atomic_compare_exchange_weak(&fs_mgr.master_idx,
			&master_idx,
			((idx == master_idx)
			? MASTER_IDX_NONE : master_idx)));
#else
	} while (atomic_cmpxchg(&fs_mgr.master_idx,
			master_idx,
			((idx == master_idx)
			? MASTER_IDX_NONE : master_idx))
		!= master_idx);
#endif // FS_UT
}
#endif // SUPPORT_FS_NEW_METHOD


static inline void fs_set_sync_status(unsigned int idx, unsigned int flag)
{
	struct SensorInfo info = {0}; // for log using

	fs_get_reg_sensor_info(idx, &info);


	/* unset sync => reset pf_ctrl_bits data of this idx */
	/* TODO: add a API for doing this */
	if (flag == 0) {
		FS_WRITE_BIT(idx, 0, &fs_mgr.pf_ctrl_bits);
		FS_WRITE_BIT(idx, 0, &fs_mgr.setup_complete_bits);


#if defined(USING_CCU) && defined(DELAY_CCU_OP)
		if (FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits) == 1) {
			FS_WRITE_BIT(idx, 0, &fs_mgr.power_on_ccu_bits);

#if !defined(REDUCE_FS_DRV_LOG)
			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%d (OFF)\n",
				idx,
				info.sensor_id,
				info.sensor_idx,
				FS_READ_BITS(&fs_mgr.power_on_ccu_bits));
#endif // REDUCE_FS_DRV_LOG

			frm_reset_ccu_vsync_timestamp(idx, 0);

			/* power off CCU */
			frm_power_on_ccu(0);

			fs_alg_reset_vsync_data(idx);
		}
#endif // USING_CCU && DELAY_CCU_OP
	}


#if defined(USING_CCU) && defined(DELAY_CCU_OP)
	if (flag > 0 && FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits) == 0) {
		FS_WRITE_BIT(idx, 1, &fs_mgr.power_on_ccu_bits);

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%d (ON)\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			FS_READ_BITS(&fs_mgr.power_on_ccu_bits));
#endif // REDUCE_FS_DRV_LOG

		fs_alg_reset_vsync_data(idx);

		/* power on CCU and get device handle */
		frm_power_on_ccu(1);

		frm_reset_ccu_vsync_timestamp(idx, 1);
	}
#endif // USING_CCU && DELAY_CCU_OP


	fs_set_status_bits(idx, flag, &fs_mgr.enSync_bits);


#if !defined(REDUCE_FS_DRV_LOG)
	/* log print info */
	LOG_INF("en:%u [%u] ID:%#x(sidx:%u)   [enSync_bits:%d]\n",
		flag,
		idx,
		info.sensor_id,
		info.sensor_idx,
		FS_READ_BITS(&fs_mgr.enSync_bits));
#endif // REDUCE_FS_DRV_LOG

}


static inline void fs_set_sync_idx(unsigned int idx, unsigned int flag)
{
	fs_set_sync_status(idx, flag);

	fs_alg_set_sync_type(idx, flag);

#if defined(FS_UT)
	fs_reset_ft_mode_data(idx, flag);
#endif // FS_UT

	fs_sa_try_reset_master_idx(idx);

	fs_decision_maker(idx);

	fs_update_status(idx, flag);
}


void fs_set_sync(unsigned int ident, unsigned int flag)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	if (check_idx_valid(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}


#if !defined(FS_UT)
	/* user cmd force disable frame-sync set_sync */
	if (fs_con_chk_force_to_ignore_set_sync()) {
		LOG_MUST(
			"NOTICE: [%u] USER set force to ignore frame-sync set sync, return\n",
			idx);
		return;
	}
#endif // FS_UT


	fs_set_sync_idx(idx, flag);
}


unsigned int fs_is_set_sync(unsigned int ident)
{
	unsigned int idx = 0, result = 0;
#if !defined(REDUCE_FS_DRV_LOG)
	struct SensorInfo info = {0}; // for log using
#endif // REDUCE_FS_DRV_LOG

	idx = fs_get_reg_sensor_pos(ident);

	if (check_idx_valid(idx) == 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_MUST("WARNING: [%u] %s is not register, ident:%u, return\n",
			idx, REG_INFO, ident);
#endif // REDUCE_FS_DRV_LOG

		return 0;
	}


	result = FS_CHECK_BIT(idx, &fs_mgr.enSync_bits);


#if !defined(REDUCE_FS_DRV_LOG)
	/* log print info */
	fs_get_reg_sensor_info(idx, &info);

	LOG_INF("%u [%u] ID:%#x(sidx:%u)   [enSync_bits:%d]\n",
		result,
		idx,
		info.sensor_id,
		info.sensor_idx,
		FS_READ_BITS(&fs_mgr.enSync_bits));
#endif // REDUCE_FS_DRV_LOG


	return result;
}


static void fs_set_sensor_driver_framelength_lc(
	unsigned int idx, unsigned int fl_lc)
{
	int ret = 0;
	struct callback_st *p_cb = &fs_mgr.cb_data[idx];
	callback_set_framelength cb_func = p_cb->func_ptr;

	struct SensorInfo info = {0}; // for log using

	fs_get_reg_sensor_info(idx, &info);


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


/*
 * all framelength operation must use this API
 */
static inline void fs_set_framelength_lc(unsigned int idx, unsigned int fl_lc)
{
#if !defined(TWO_STAGE_FS) //|| defined(QUERY_CCU_TS_AT_SOF)
	/* 0. update frame recorder data */
	frec_update_shutter_fl_lc(idx, 0, fl_lc);

	/* after all settings are updated, */
	/* setup frame monitor fmeas data before calling fs_alg_get_vsync_data */
	fs_alg_setup_frame_monitor_fmeas_data(idx);
#endif // TWO_STAGE_FS


#ifndef REDUCE_FS_DRV_LOG
	frec_dump_recorder(idx);
#endif //REDUCE_FS_DRV_LOG


	/* 1. using callback function to set framelength */
	fs_set_sensor_driver_framelength_lc(idx, fl_lc);


	/* 2. set the results to fs algo and frame monitor */
	// frec_notify_setting_frame_record_st_data(idx);
}


#ifdef SUPPORT_FS_NEW_METHOD
static inline unsigned int fs_check_n_1_status_ctrl(
	unsigned int idx, unsigned int en)
{
	enum FS_FEATURE_MODE mode_status = fs_mgr.ft_mode[idx];

	if (en) {
		/* only normal mode can turn ON N:1 mode */
		return (mode_status == 0) ? 1 : 0;
	}

	/* only FRAME_TAG and N_1_KEEP can turn OFF N:1 mode */
	return (mode_status
		& (FS_FT_MODE_FRAME_TAG | FS_FT_MODE_N_1_KEEP))
		? 1 : 0;
}


static inline void fs_check_n_1_status_extra_ctrl(unsigned int idx)
{
	enum FS_FEATURE_MODE old_ft_status;
	struct SensorInfo info = {0}; // for log using

	fs_get_reg_sensor_info(idx, &info);


	old_ft_status = fs_mgr.ft_mode[idx];

	if (fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_ON) {
		fs_mgr.ft_mode[idx] &= ~(FS_FT_MODE_N_1_ON);
		fs_mgr.ft_mode[idx] |= FS_FT_MODE_N_1_KEEP;

		LOG_MUST(
			"[%u] ID:%#x(sidx:%u), feature mode status change (%u->%u), ON:%u/KEEP:%u\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			old_ft_status,
			fs_mgr.ft_mode[idx],
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_ON,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_KEEP
		);

		/* turn off FS algo n_1_on_off flag */
		/* for calculating FL normally */
		fs_alg_set_n_1_on_off_flag(idx, 0);

	} else if (fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_OFF) {
		fs_mgr.ft_mode[idx] &= ~(FS_FT_MODE_N_1_OFF);
		fs_mgr.ft_mode[idx] &= ~(FS_FT_MODE_FRAME_TAG);
		fs_mgr.ft_mode[idx] |= FS_FT_MODE_NORMAL;

		LOG_MUST(
			"[%u] ID:%#x(sidx:%u), feature mode status change (%u->%u), OFF:%u/FRAME_TAG:%u/NORMAL:%u\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			old_ft_status,
			fs_mgr.ft_mode[idx],
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_OFF,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_FRAME_TAG,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_NORMAL
		);

		/* turn off FS algo n_1_on_off flag */
		/* for calculating FL normally */
		fs_alg_set_n_1_on_off_flag(idx, 0);
	}
}


static inline void fs_feature_status_extra_ctrl(unsigned int idx)
{
	fs_check_n_1_status_extra_ctrl(idx);
}


void fs_sa_request_switch_master(unsigned int idx)
{
	int sa_method = -1;


	sa_method = FS_ATOMIC_READ(&fs_mgr.sa_method);

	if (sa_method != FS_SA_ADAPTIVE_MASTER) {
		LOG_MUST(
			"ERROR: SA method:%d, but request (from idx:%u) switch master sensor, vdiff will not be adjusted\n",
			sa_method,
			idx);

		return;
	}


	FS_ATOMIC_SET(idx, &fs_mgr.master_idx);


#if !defined(REDUCE_FS_DRV_LOG)
	LOG_INF(
		"switch master instance idx to %d  (from idx:%u)\n",
		FS_ATOMIC_READ(&fs_mgr.master_idx),
		idx);
#endif // REDUCE_FS_DRV_LOG
}


static inline int fs_get_valid_master_instance_idx(void)
{
	int m_idx = MASTER_IDX_NONE, valid = 0, i = 0;

	m_idx = FS_ATOMIC_READ(&fs_mgr.master_idx);

	/* check case -> user not config master sensor */
	/* or config a non valid sensor as master */
	if (m_idx == MASTER_IDX_NONE
		|| FS_CHECK_BIT(m_idx, &fs_mgr.validSync_bits) == 0) {
		/* auto select a master sensor */
		/* using the sensor first valid for sync */
		valid = FS_READ_BITS(&fs_mgr.validSync_bits);

		while ((i < SENSOR_MAX_NUM) && (((valid >> i) & 0x01) != 1))
			i++;

		FS_ATOMIC_SET(i, &fs_mgr.master_idx);

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_INF(
			"NOTICE: current master_idx:%d is not valid for doing frame-sync, validSync_bits:%d, auto set to idx:%d\n",
			m_idx, valid, i);
#endif // REDUCE_FS_DRV_LOG

		return i;
	}

	return m_idx;
}


/*
 * return: (0/1): trigger (failed/successfully)
 */
static int fs_try_trigger_frame_sync_sa(unsigned int idx)
{
	unsigned int ret = 0;
	int valid_sync_bits = 0, m_idx = MASTER_IDX_NONE, sa_method = 0;
	unsigned int fl_lc = 0;
	struct SensorInfo info = {0}; // for log using

	fs_get_reg_sensor_info(idx, &info);


	if (FS_CHECK_BIT(idx, &fs_mgr.validSync_bits) == 0) {
#if !defined(REDUCE_FS_DRV_LOG)
		LOG_INF(
			"WARNING: [%u] ID:%#x(sidx:%u), not valid for sync:%d(streaming:%d/enSync:%d), return\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			FS_READ_BITS(&fs_mgr.validSync_bits),
			FS_READ_BITS(&fs_mgr.streaming_bits),
			FS_READ_BITS(&fs_mgr.enSync_bits));
#endif // REDUCE_FS_DRV_LOG

		return 0;
	}

	if (FS_CHECK_BIT(idx, &fs_mgr.pf_ctrl_bits) == 0) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u), wait for getting pf_ctrl:%d, return\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits));

		return 0;
	}

	if (FS_CHECK_BIT(idx, &fs_mgr.setup_complete_bits) == 0) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u), wait for setup_coplete:%d, return\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			FS_READ_BITS(&fs_mgr.setup_complete_bits));

		return 0;
	}


	/* FS standalone trigger for calculating frame length */
	m_idx = fs_get_valid_master_instance_idx();
	valid_sync_bits = FS_READ_BITS(&fs_mgr.validSync_bits);
	sa_method = FS_ATOMIC_READ(&fs_mgr.sa_method);

	ret = fs_alg_solve_frame_length_sa(
		idx, m_idx, valid_sync_bits, sa_method, &fl_lc);

	if (ret == 0) { // 0 => no error
		/* set framelength */
		/* all framelength operation must use this API */
		fs_set_framelength_lc(idx, fl_lc);


		/* clear status bit */
		FS_WRITE_BIT(idx, 0, &fs_mgr.pf_ctrl_bits);
		FS_WRITE_BIT(idx, 0, &fs_mgr.setup_complete_bits);


		/* check/change feature mode status */
		fs_feature_status_extra_ctrl(idx);
	}

	return 1;
}


static void fs_reset_frame_tag(unsigned int idx)
{
	fs_mgr.frame_tag[idx] = 0;

	fs_alg_set_frame_tag(idx, fs_mgr.frame_tag[idx]);
}


static inline void fs_try_set_auto_frame_tag(unsigned int idx)
{
	unsigned int f_tag = 0, f_cell = 0;

	if (((fs_mgr.ft_mode[idx] & FS_FT_MODE_ASSIGN_FRAME_TAG) == 0)
		&& (fs_mgr.ft_mode[idx] & FS_FT_MODE_FRAME_TAG)) {
		/* N:1 case */
		if (fs_mgr.frame_cell_size[idx] == 0) {
			LOG_MUST(
				"NOTICE: [%u] call set auto frame_tag, feature_mode:%u, but frame_cell_size:%u not valid, return\n",
				idx,
				fs_mgr.ft_mode[idx],
				fs_mgr.frame_cell_size[idx]
			);

			return;
		}

		fs_alg_set_frame_tag(idx, fs_mgr.frame_tag[idx]);

		/* update new frame tag */
		f_tag = (fs_mgr.frame_tag[idx] + 1);
		f_cell = (fs_mgr.frame_cell_size[idx]);
		fs_mgr.frame_tag[idx] = f_tag % f_cell;
	}
}


void fs_set_frame_tag(unsigned int ident, unsigned int f_tag)
{
#if defined(TWO_STAGE_FS) && !defined(QUERY_CCU_TS_AT_SOF)
	unsigned int f_cell;
#endif // TWO_STAGE_FS && !QUERY_CCU_TS_AT_SOF

	unsigned int idx = fs_get_reg_sensor_pos(ident);

	if (check_idx_valid(idx) == 0) {
		LOG_MUST(
			"ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident
		);

		return;
	}


	if ((fs_mgr.ft_mode[idx] & FS_FT_MODE_ASSIGN_FRAME_TAG)
		&& (fs_mgr.ft_mode[idx] & FS_FT_MODE_FRAME_TAG)) {
		/* M-Stream case */
#if !defined(TWO_STAGE_FS) || defined(QUERY_CCU_TS_AT_SOF)
		fs_mgr.frame_tag[idx] = f_tag;
#else // TWO_STAGE_FS
		f_cell = fs_mgr.frame_cell_size[idx];

		if (f_cell != 0)
			fs_mgr.frame_tag[idx] = (f_tag + 1) % f_cell;
		else {
			fs_mgr.frame_tag[idx] = 0;

			LOG_MUST(
				"WARNING: [%u] input f_tag:%u, re-set to %u, because f_cell:%u (TWO_STAGE_FS)\n",
				idx, f_tag, fs_mgr.frame_tag[idx], f_cell);
		}
#endif // !TWO_STAGE_FS || QUERY_CCU_TS_AT_SOF

		fs_alg_set_frame_tag(idx, fs_mgr.frame_tag[idx]);

	} else {
		LOG_MUST(
			"WARNING: [%u] call set frame_tag:%u, but feature_mode:%u not allow\n",
			idx,
			f_tag,
			fs_mgr.ft_mode[idx]
		);
	}
}


void fs_n_1_en(unsigned int ident, unsigned int n, unsigned int en)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);
	struct SensorInfo info = {0}; // for log using

	if (check_idx_valid(idx) == 0) {
		LOG_MUST(
			"ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident
		);

		return;
	}

	fs_get_reg_sensor_info(idx, &info);


	if (fs_check_n_1_status_ctrl(idx, en) == 0) {
		/* feature mode status is non valid for this ctrl */
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), set N:%u, but ft ctrl non valid, feature_mode:%u (FRAME_TAG:%u/ON:%u/KEEP:%u/OFF:%u), return  [en:%u]\n",
			idx,
			info.sensor_id,
			info.sensor_idx,
			n,
			fs_mgr.ft_mode[idx],
			fs_mgr.ft_mode[idx] & FS_FT_MODE_FRAME_TAG,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_ON,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_KEEP,
			fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_OFF,
			en
		);

		return;
	}

	if (en) {
		fs_mgr.frame_cell_size[idx] = n;
		fs_mgr.ft_mode[idx] =
			(FS_FT_MODE_FRAME_TAG | FS_FT_MODE_N_1_ON);
	} else {
		fs_mgr.frame_cell_size[idx] = 0;
		fs_mgr.ft_mode[idx] =
			(FS_FT_MODE_FRAME_TAG | FS_FT_MODE_N_1_OFF);
	}

	fs_alg_set_frame_cell_size(idx, fs_mgr.frame_cell_size[idx]);


	/* reset frame tag cnt */
	// fs_set_frame_tag(idx, 0);
	fs_reset_frame_tag(idx);


	/* notify frame-sync algo at ON/OFF case for setting correct FL */
	/* this will be turn off when FL be calculated done */
	fs_alg_set_n_1_on_off_flag(idx, 1);


	LOG_MUST(
		"[%u] ID:%#x(sidx:%u), N:%u, feature_mode:%u (FRAME_TAG:%u/ON:%u/OFF:%u)  [en:%u]\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		n,
		fs_mgr.ft_mode[idx],
		fs_mgr.ft_mode[idx] & FS_FT_MODE_FRAME_TAG,
		fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_ON,
		fs_mgr.ft_mode[idx] & FS_FT_MODE_N_1_OFF,
		en
	);
}


void fs_mstream_en(unsigned int ident, unsigned int en)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);
	struct SensorInfo info = {0}; // for log using

	if (check_idx_valid(idx) == 0) {
		LOG_MUST(
			"ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident
		);

		return;
	}

	fs_get_reg_sensor_info(idx, &info);


	fs_n_1_en(ident, 2, en);

	if (en) {
		/* set M-Stream mode */
		fs_mgr.ft_mode[idx] |= FS_FT_MODE_ASSIGN_FRAME_TAG;
	} else {
		/* unset M-Stream mode */
		fs_mgr.ft_mode[idx] &= ~(FS_FT_MODE_ASSIGN_FRAME_TAG);
	}


	LOG_MUST(
		"[%u] ID:%#x(sidx:%u), N:2, feature_mode:%u (ASSIGN_FRAME_TAG:%u)  [en:%u]\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		fs_mgr.ft_mode[idx],
		fs_mgr.ft_mode[idx] & FS_FT_MODE_ASSIGN_FRAME_TAG,
		en
	);
}
#endif // SUPPORT_FS_NEW_METHOD


void fs_set_extend_framelength(
	unsigned int ident, unsigned int ext_fl_lc, unsigned int ext_fl_us)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	if (check_idx_valid(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}

	fs_alg_set_extend_framelength(idx, ext_fl_lc, ext_fl_us);
}


void fs_seamless_switch(unsigned int ident)
{
	unsigned int idx = fs_get_reg_sensor_pos(ident);

	if (check_idx_valid(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}

	fs_alg_seamless_switch(idx);
}


/*
 * update fs_streaming_st data
 *     (for cam_mux switch & sensor stream on before cam mux setup)
 */
void fs_update_tg(unsigned int ident, unsigned int tg)
{
	struct SensorInfo info = {0};
	unsigned int idx = fs_get_reg_sensor_pos(ident);


	if (check_idx_valid(idx) == 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_MUST("WARNING: [%u] %s is not register, ident:%u, return\n",
			idx, REG_INFO, ident);
#endif // REDUCE_FS_DRV_LOG

		return;
	}


#ifdef USING_CCU
	/* 0. check ccu pwr ON, and disable INT(previous tg) */
	if (FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits))
		frm_reset_ccu_vsync_timestamp(idx, 0);
#endif // USING_CCU

	/* 0-1 convert cammux id to ccu tg id */
	tg = frm_convert_cammux_tg_to_ccu_tg(tg);


	/* 1. update the fs_streaming_st data */
	fs_alg_update_tg(idx, tg);
	frm_update_tg(idx, tg);


	fs_get_reg_sensor_info(idx, &info);


#if !defined(REDUCE_FS_DRV_LOG)
	LOG_MUST(
		"[%u] ID:%#x(sidx:%u), updated tg:%u (fs_alg, frm)\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		tg
	);
#endif // REDUCE_FS_DRV_LOG


#ifdef USING_CCU
	/* 2. on the fly change cam_mux/tg */
	/*    => reset/re-init frame info data for after using */
	if (FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits)) {
		// frm_reset_frame_info(idx);

		// frm_init_frame_info_st_data(idx,
		//	info.sensor_id, info.sensor_idx,
		//	tg);

		frm_reset_ccu_vsync_timestamp(idx, 1);
	}
#endif // USING_CCU
}


static inline void fs_reset_idx_ctx(unsigned int idx)
{
	/* unset sync */
	fs_set_sync_idx(idx, 0);

	/* reset frm frame info data */
	frm_reset_frame_info(idx);

	/* reset fs instance data (algo -> fs_inst[idx]) */
	fs_alg_reset_fs_inst(idx);

	/* reset frame recorder data */
	frec_reset_recorder(idx);

	/* clear/reset perframe stage data */
	fs_reset_perframe_stage_data(idx, 0);

	/* unset stream (stream off) */
	fs_set_stream(idx, 0);

	/* clear/reset callback function pointer */
	fs_reset_cb_data(idx);
}


/*
 * return: (0/uint_t) for (no error/error)
 *     P.S: 1 -> register failed
 *
 * input:
 *     flag: "non 0" -> stream on; "0" -> stream off;
 *     sensor_info: struct fs_streaming_st*
 */
unsigned int fs_streaming(
	unsigned int flag, struct fs_streaming_st (*sensor_info))
{
	unsigned int idx = 0;


	/* 1. register this sensor */
	struct SensorInfo info = {
		.sensor_id = sensor_info->sensor_id,
		.sensor_idx = sensor_info->sensor_idx,
	};

	/* register this sensor and check return idx/position value */
	idx = fs_register_sensor(&info, REGISTER_METHOD);
	if (check_idx_valid(idx) == 0) {
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


#if defined(USING_CCU) && !defined(DELAY_CCU_OP)
		if (FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits) == 0) {
			FS_WRITE_BIT(idx, 1, &fs_mgr.power_on_ccu_bits);

			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%d (ON)\n",
				idx,
				sensor_info->sensor_id,
				sensor_info->sensor_idx,
				FS_READ_BITS(&fs_mgr.power_on_ccu_bits));

			/* power on CCU and get device handle */
			frm_power_on_ccu(1);
		}
#endif // USING_CCU && !DELAY_CCU_OP


		/* convert cammux id to ccu tg id */
		sensor_info->tg =
			frm_convert_cammux_tg_to_ccu_tg(sensor_info->tg);

		/* set data to frm, fs algo, and frame recorder */
		frm_init_frame_info_st_data(idx,
			sensor_info->sensor_id, sensor_info->sensor_idx,
			sensor_info->tg);

		fs_alg_set_streaming_st_data(idx, sensor_info);

#if !defined(FS_UT)
		hw_fs_alg_set_streaming_st_data(idx, sensor_info);
#endif // FS_UT

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


		fs_reset_ft_mode_data(idx, flag);


#if defined(USING_CCU) && !defined(DELAY_CCU_OP)
		if (FS_CHECK_BIT(idx, &fs_mgr.power_on_ccu_bits) == 1) {
			FS_WRITE_BIT(idx, flag, &fs_mgr.power_on_ccu_bits);

			LOG_INF("[%u] ID:%#x(sidx:%u), pw_ccu:%d (OFF)\n",
				idx,
				sensor_info->sensor_id,
				sensor_info->sensor_idx,
				FS_READ_BITS(&fs_mgr.power_on_ccu_bits));

			/* power off CCU */
			frm_power_on_ccu(0);
		}
#endif // USING_CCU && !DELAY_CCU_OP
	}


#ifndef REDUCE_FS_DRV_LOG
	/* log print info */
	fs_get_reg_sensor_info(idx, &info);
	LOG_INF("en:%u, [%u] ID:%#x(sidx:%u)   [streaming_bits:%d]\n",
		flag,
		idx,
		info.sensor_id,
		info.sensor_idx,
		FS_READ_BITS(&fs_mgr.streaming_bits));
#endif // REDUCE_FS_DRV_LOG


#if !defined(FS_UT)
	if (flag > 0 && fs_con_chk_default_en_set_sync()) {
		LOG_MUST(
			"NOTICE: [%u] USER set default enable frame-sync set sync\n",
			idx);

		fs_set_sync_idx(idx, 1);
	}
#endif // FS_UT

	return 0;
}


static inline void fs_notify_sensor_ctrl_setup_complete(unsigned int idx)
{
	int is_streaming = 0;

#ifndef REDUCE_FS_DRV_LOG
	struct SensorInfo info = {0}; // for log using
#endif // REDUCE_FS_DRV_LOG

#if !defined(FS_UT)
	/* for checking if use HW sensor sync */
	unsigned int solveIdxs[1] = {idx};
#endif // FS_UT

	is_streaming = FS_CHECK_BIT(idx, &fs_mgr.streaming_bits);

	if (is_streaming == 1)
		FS_WRITE_BIT(idx, 1, &fs_mgr.setup_complete_bits);

#ifndef REDUCE_FS_DRV_LOG
	fs_get_reg_sensor_info(idx, &info);

	if (is_streaming == 0) {
		LOG_INF("WARNING: [%u] ID:%#x(sidx:%u) is not streaming ON\n",
			idx,
			info.sensor_id,
			info.sensor_idx);
	}

	LOG_INF(
		"[%u] ID:%#x(sidx:%u)  [setup_complete_bits:%d]\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		FS_READ_BITS(&fs_mgr.setup_complete_bits));
#endif // REDUCE_FS_DRV_LOG


#ifdef SUPPORT_FS_NEW_METHOD
#if !defined(FS_UT)
	/* check if use HW sensor sync */
	if (handle_by_hw_sensor_sync(solveIdxs, 1))
		return;
#endif // FS_UT

	/* setup compeleted => tirgger FS standalone method */
	if (FS_ATOMIC_READ(&fs_mgr.using_sa_ver))
		fs_try_trigger_frame_sync_sa(idx);
#endif // SUPPORT_FS_NEW_METHOD
}


/*
 * update fs_perframe_st data
 *     (for v4l2_ctrl_request_setup order)
 *     (set_framelength is called after set_anti_flicker)
 */
void fs_update_auto_flicker_mode(unsigned int ident, unsigned int en)
{
#if !defined(REDUCE_FS_DRV_LOG)
	struct SensorInfo info = {0};
#endif // REDUCE_FS_DRV_LOG

	unsigned int idx = fs_get_reg_sensor_pos(ident);


	if (check_idx_valid(idx) == 0) {
		LOG_PR_WARN("ERROR: [%u] %s is not register, ident:%u\n",
			idx, REG_INFO, ident);

		return;
	}


	/* 1. update the fs_perframe_st data in fs algo */
	fs_alg_set_anti_flicker(idx, en);


#if !defined(REDUCE_FS_DRV_LOG)
	fs_get_reg_sensor_info(idx, &info);

	LOG_INF(
		"[%u] ID:%#x(sidx:%u), updated flicker_en:%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		en
	);
#endif // REDUCE_FS_DRV_LOG


	/* if this is the last ctrl needed by FrameSync, notify setup complete */
	/* NOTE: don't bind here, flicker is NOT per-frame ctrl */
	// fs_notify_sensor_ctrl_setup_complete(idx);
}


/*
 * update fs_perframe_st data
 *     (for v4l2_ctrl_request_setup order)
 *     (set_max_framerate_by_scenario is called after set_shutter)
 */
void fs_update_min_framelength_lc(unsigned int ident, unsigned int min_fl_lc)
{
#if !defined(REDUCE_FS_DRV_LOG)
	struct SensorInfo info = {0};
#endif // REDUCE_FS_DRV_LOG

	unsigned int idx = fs_get_reg_sensor_pos(ident);


	if (check_idx_valid(idx) == 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_MUST("WARNING: [%u] %s is not register, ident:%u, return\n",
			idx, REG_INFO, ident);
#endif // REDUCE_FS_DRV_LOG

		return;
	}


	/* 1. update the fs_perframe_st data in fs algo */
	fs_alg_update_min_fl_lc(idx, min_fl_lc);

#if !defined(FS_UT)
	hw_fs_alg_update_min_fl_lc(idx, min_fl_lc);
#endif // FS_UT


#if !defined(REDUCE_FS_DRV_LOG)
	fs_get_reg_sensor_info(idx, &info);

	LOG_INF(
		"[%u] ID:%#x(sidx:%u), updated min_fl_lc:%u\n",
		idx,
		info.sensor_id,
		info.sensor_idx,
		min_fl_lc
	);
#endif // REDUCE_FS_DRV_LOG


#ifndef USING_V4L2_CTRL_REQUEST_SETUP
	/* if this is the last ctrl needed by FrameSync, notify setup complete */
	fs_notify_sensor_ctrl_setup_complete(idx);
#endif // USING_V4L2_CTRL_REQUEST_SETUP
}


/*
 * Run Frame Sync Method (for pair ctrl trigger)
 *
 * ex:
 *     1. hardware sensor frame sync
 *        => call hw fs algo API to solve framelength
 *
 *     2. software frame sync
 *        => call fs algo API to solve framelength
 */
static inline unsigned int fs_run_frame_sync_proc(
	unsigned int solveIdxs[], unsigned int framelength[], unsigned int len)
{
	unsigned int ret = 0;

#if !defined(FS_UT)
	if (handle_by_hw_sensor_sync(solveIdxs, len))
		ret = hw_fs_alg_solve_frame_length(solveIdxs, framelength, len);
	else
#endif // FS_UT
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
	fs_act = (
		FS_READ_BITS(&fs_mgr.validSync_bits) ==
		FS_READ_BITS(&fs_mgr.pf_ctrl_bits))
		? 1 : 0;

	/* add for checking all ctrl needed by FrameSync have been setup */
	if (fs_act == 1) {
		fs_act = (
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits) ==
			FS_READ_BITS(&fs_mgr.setup_complete_bits))
			? 1 : 0;

		if (fs_act == 0) {
			LOG_MUST(
				"WARNING: sensor ctrl has not been setup yet, validSync:%2d, pf_ctrl:%2d, setup_complete:%2d\n",
				FS_READ_BITS(&fs_mgr.validSync_bits),
				FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.setup_complete_bits));
		}
	}


	if (fs_act == 1) {
		LOG_INF("fs_activate:%u, validSync:%2d, pf_ctrl:%2d, cnt:%u\n",
			fs_act,
			FS_READ_BITS(&fs_mgr.validSync_bits),
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
			++fs_mgr.act_cnt);


		/* 1 pick up validSync sensor information correctlly */
		for (i = 0; i < SENSOR_MAX_NUM; ++i) {
			if (FS_CHECK_BIT(i, &fs_mgr.validSync_bits) == 1) {
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
		LOG_MUST(
			"wait for other pf_ctrl setup, validSync:%2d, pf_ctrl:%2d, setup_complete:%2d\n",
			FS_READ_BITS(&fs_mgr.validSync_bits),
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
			FS_READ_BITS(&fs_mgr.setup_complete_bits));
	}


	return fs_act;
}


static void fs_check_frame_sync_ctrl(
	unsigned int idx, struct fs_perframe_st (*frameCtrl))
{
	/* before doing frame sync, check some situation */
	unsigned int ret = 0;


	/* 1. check this sensor is valid for sync or not */
	ret = FS_CHECK_BIT(idx, &fs_mgr.validSync_bits);

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
	ret = FS_CHECK_BIT(idx, &fs_mgr.pf_ctrl_bits);

	if (ret == 1) {
		LOG_PR_WARN(
			"WARNING: Set same sensor, return. [%u] ID:%#x  [pf_ctrl_bits:%d]\n",
			idx,
			frameCtrl->sensor_id,
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits));

		return;

	} else {
		FS_WRITE_BIT(idx, 1, &fs_mgr.pf_ctrl_bits);

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


/*
 * description:
 *     call by sensor driver,
 *     fs_perframe_st is a settings for a certain sensor.
 *
 * input:
 *     frameCtrl: struct fs_perframe_st*
 */
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

	if (check_idx_valid(idx) == 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_MUST("WARNING: [%u] %s is not register, ident:%u, return\n",
			idx, REG_INFO, ident);
#endif // REDUCE_FS_DRV_LOG

		return;
	}

	if (FS_CHECK_BIT(idx, &fs_mgr.validSync_bits) == 0) {
		/* no start frame sync, return */
		return;
	}

	/* check streaming status, due to maybe calling by non-sync flow */
	if (FS_CHECK_BIT(idx, &fs_mgr.streaming_bits) == 0) {
		LOG_INF("WARNING: [%u] is stream off. ID:%#x(sidx:%u), return\n",
			idx,
			frameCtrl->sensor_id,
			frameCtrl->sensor_idx);

		return;
	}


	//fs_dump_pf_info(frameCtrl);


	fs_mgr.pf_ctrl[idx] = *frameCtrl;


#if !defined(FORCE_USING_SA_MODE)
#if defined(SUPPORT_AUTO_EN_SA_MODE) && !defined(FS_UT)
	/* check needed SA */
	if (frameCtrl->hdr_exp.mode_exp_cnt > 0)
		fs_mgr.ft_mode[idx] |= FS_FT_MODE_STG_HDR;
	else
		fs_mgr.ft_mode[idx] &= ~FS_FT_MODE_STG_HDR;


	fs_decision_maker(idx);
#endif
#endif // FORCE_USING_SA_MODE


	/* for FPS non 1-1 frame-sync case */
	fs_try_set_auto_frame_tag(idx);


	/* 1. set perframe ctrl data to fs algo */
	fs_alg_set_perframe_st_data(idx, frameCtrl);

#if !defined(FS_UT)
	hw_fs_alg_set_perframe_st_data(idx, frameCtrl);
#endif // FS_UT


#ifdef FS_UT
	/* only do when FS_UT */
	/* In normal run, use data send from sensor driver directly */
	/* 2. call fs algo api to simulate the result of write_shutter() */
	frameCtrl->out_fl_lc = fs_alg_write_shutter(idx);
#endif // FS_UT


#if !defined(QUERY_CCU_TS_AT_SOF)
	/* 3. push frame settings into frame recorder */
	frec_push_record(idx, frameCtrl->shutter_lc, frameCtrl->out_fl_lc);
#else
	/* 3. update frame recorder data */
	frec_update_shutter_fl_lc(idx,
		frameCtrl->shutter_lc, frameCtrl->out_fl_lc);
#endif // QUERY_CCU_TS_AT_SOF


	/* 4. frame sync ctrl */
	fs_check_frame_sync_ctrl(idx, frameCtrl);


#ifdef USING_V4L2_CTRL_REQUEST_SETUP
	/* if this is the last ctrl needed by FrameSync, notify setup complete*/
	/* for N3D and not using v4l2_ctrl_request_setup*/
	fs_notify_sensor_ctrl_setup_complete(idx);
#endif // USING_V4L2_CTRL_REQUEST_SETUP
}


void fs_update_shutter(struct fs_perframe_st (*frameCtrl))
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

	if (check_idx_valid(idx) == 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		LOG_MUST("WARNING: [%u] %s is not register, ident:%u, return\n",
			idx, REG_INFO, ident);
#endif // REDUCE_FS_DRV_LOG

		return;
	}

	if (FS_CHECK_BIT(idx, &fs_mgr.validSync_bits) == 0) {
		/* no start frame sync, return */
		return;
	}

	/* check streaming status, due to maybe calling by non-sync flow */
	if (FS_CHECK_BIT(idx, &fs_mgr.streaming_bits) == 0) {
		LOG_INF("WARNING: [%u] is stream off. ID:%#x(sidx:%u), return\n",
			idx,
			frameCtrl->sensor_id,
			frameCtrl->sensor_idx);

		return;
	}


	//fs_dump_pf_info(frameCtrl);


	fs_mgr.pf_ctrl[idx] = *frameCtrl;


	/* update frame recorder data */
	frec_update_shutter_fl_lc(idx, 0, frameCtrl->out_fl_lc);
}


void fs_notify_vsync(unsigned int ident)
{
#if !defined(FS_UT)
	unsigned int listen_vsync_usr = fs_con_get_usr_listen_ext_vsync();
	unsigned int listen_vsync_alg = fs_con_get_listen_vsync_alg_cfg();
	unsigned int idx = fs_get_reg_sensor_pos(ident);


	if (check_idx_valid(idx) == 0) {
		/* not register/streaming on */
		return;
	}

	if (FS_CHECK_BIT(idx, &fs_mgr.validSync_bits) == 0) {
		/* no start frame sync, return */
		return;
	}

	/* check listen to ext vysnc or not */
	if (!(listen_vsync_usr || listen_vsync_alg))
		return;


#if !defined(QUERY_CCU_TS_AT_SOF)
	fs_set_shutter(&fs_mgr.pf_ctrl[idx]);
#else
	fs_alg_sa_notify_setup_all_frame_info(idx);

	frec_notify_vsync(idx);
	fs_alg_sa_notify_vsync(idx);
#endif // QUERY_CCU_TS_AT_SOF


#endif // FS_UT
}


/*
 * return:
 *     0 for error,
 *     non 0 for (Start -> cnt of validSync_bits;
 *                  End -> cnt of pf_ctrl_bits)
 *
 * input:
 *     flag: "non 0" -> Start sync frame; "0" -> End sync frame;
 *
 * header file:
 *     frame_sync_camsys.h
 */
unsigned int fs_sync_frame(unsigned int flag)
{
	enum FS_STATUS status = get_fs_status();
	unsigned int triggered = 0;

#ifdef ALL_USING_ATOMIC
	int valid_sync = 0;
	int last_pf_ctrl = 0, last_setup_complete = 0;


	valid_sync = FS_ATOMIC_READ(&fs_mgr.validSync_bits);
#endif // ALL_USING_ATOMIC


#if !defined(FS_UT)
	/* user cmd force disable frame-sync set_sync */
	if (fs_con_chk_force_to_ignore_set_sync())
		return 0;
#endif // FS_UT


#ifdef SUPPORT_FS_NEW_METHOD
	/* check using FS standalone mode => do nothing here */
	if (FS_ATOMIC_READ(&fs_mgr.using_sa_ver) != 0) {

#if !defined(REDUCE_FS_DRV_LOG)
		/* (LOG) show new status and pf_ctrl_bits value */
		LOG_MUST(
			"[Start:%u] streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d(last:%d), setup_complete:%d(last:%d) [FS SA mode]\n",
			flag,
			status,
			FS_READ_BITS(&fs_mgr.streaming_bits),
			FS_READ_BITS(&fs_mgr.enSync_bits),
			FS_READ_BITS(&fs_mgr.validSync_bits),
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
			FS_READ_BITS(&fs_mgr.last_pf_ctrl_bits),
			FS_READ_BITS(&fs_mgr.setup_complete_bits),
			FS_READ_BITS(&fs_mgr.last_setup_complete_bits));
#endif // REDUCE_FS_DRV_LOG

		return 0;
	}
#endif // SUPPORT_FS_NEW_METHOD


	/* check sync frame start/end */
	/*     flag > 0  : start sync frame */
	/*     flag == 0 : end sync frame */
	if (flag > 0) {
		/* check status is ready for starting sync frame or not */
		if (status < FS_WAIT_FOR_SYNCFRAME_START) {
			LOG_MUST(
				"[Start:%u] ERROR: stat:%u, streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d(last:%d), setup_complete:%d(last:%d)\n",
				flag,
				status,
				FS_READ_BITS(&fs_mgr.streaming_bits),
				FS_READ_BITS(&fs_mgr.enSync_bits),
				FS_READ_BITS(&fs_mgr.validSync_bits),
				FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.last_pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.setup_complete_bits),
				FS_READ_BITS(&fs_mgr.last_setup_complete_bits));

			return 0;
		}

		/* ready for sync -> update status */
		change_fs_status(FS_START_TO_GET_PERFRAME_CTRL);


		/* log --- show new status and all bits value */
		status = get_fs_status();

		LOG_INF(
			"[Start:%u] stat:%u, streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d(last:%d), setup_complete:%d(last:%d)\n",
			flag,
			status,
			FS_READ_BITS(&fs_mgr.streaming_bits),
			FS_READ_BITS(&fs_mgr.enSync_bits),
			FS_READ_BITS(&fs_mgr.validSync_bits),
			FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
			FS_READ_BITS(&fs_mgr.last_pf_ctrl_bits),
			FS_READ_BITS(&fs_mgr.setup_complete_bits),
			FS_READ_BITS(&fs_mgr.last_setup_complete_bits));


		/* return the number of valid sync sensors */
#ifndef ALL_USING_ATOMIC
		return FS_POPCOUNT(fs_mgr.validSync_bits);
#else
		return FS_POPCOUNT(valid_sync);
#endif // ALL_USING_ATOMIC

	} else {
		/* fs_sync_frame(0) flow */

		/* 1. check fs status */
		if (status != FS_START_TO_GET_PERFRAME_CTRL) {
			LOG_MUST(
				"[Start:%u] ERROR: stat:%u, streaming:%d, enSync:%d, validSync:%d, pf_ctrl:%d(%d), setup_complete:%d(%d)\n",
				flag,
				status,
				FS_READ_BITS(&fs_mgr.streaming_bits),
				FS_READ_BITS(&fs_mgr.enSync_bits),
				FS_READ_BITS(&fs_mgr.validSync_bits),
				FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.last_pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.setup_complete_bits),
				FS_READ_BITS(&fs_mgr.last_setup_complete_bits));

			return 0;
		}


		/* 2. check for running frame sync processing or not */
		triggered = fs_try_trigger_frame_sync();

		if (triggered) {
			/* 2-1. update status */
			change_fs_status(FS_WAIT_FOR_SYNCFRAME_START);


			/* 2-2. framesync trigger complete */
			/*      , clear pf_ctrl_bits, setup_complete_bits */
#ifndef ALL_USING_ATOMIC
			fs_mgr.last_pf_ctrl_bits = fs_mgr.pf_ctrl_bits;
			fs_mgr.last_setup_complete_bits =
						fs_mgr.setup_complete_bits;

			clear_all_bit(&fs_mgr.pf_ctrl_bits);
			clear_all_bit(&fs_mgr.setup_complete_bits);
#else
			last_pf_ctrl =
				FS_ATOMIC_XCHG(0, &fs_mgr.pf_ctrl_bits);
			FS_ATOMIC_SET(last_pf_ctrl,
				&fs_mgr.last_pf_ctrl_bits);

			last_setup_complete =
				FS_ATOMIC_XCHG(0, &fs_mgr.setup_complete_bits);
			FS_ATOMIC_SET(last_setup_complete,
				&fs_mgr.last_setup_complete_bits);
#endif // ALL_USING_ATOMIC


			/* 2-3. (LOG) show new status and pf_ctrl_bits value */
			status = get_fs_status();

			LOG_INF(
				"[Start:%u] stat:%u, pf_ctrl:%d->%d, ctrl_setup_complete:%d->%d\n",
				flag,
				status,
				FS_READ_BITS(&fs_mgr.last_pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.pf_ctrl_bits),
				FS_READ_BITS(&fs_mgr.last_setup_complete_bits),
				FS_READ_BITS(&fs_mgr.setup_complete_bits));
		}


		/* return the number of perframe ctrl sensors */
#ifndef ALL_USING_ATOMIC
		return FS_POPCOUNT(fs_mgr.last_pf_ctrl_bits);
#else
		return FS_POPCOUNT(last_pf_ctrl);
#endif // ALL_USING_ATOMIC
	}
}
#if !defined(FS_UT)
EXPORT_SYMBOL(fs_sync_frame);
#endif // FS_UT
/******************************************************************************/





#ifdef FS_SENSOR_CCU_IT
/******************************************************************************/
// Frame Sync IT (FS_Drv + FRM + Adaptor + Sensor Driver + CCU) function
/******************************************************************************/
static void fs_single_cam_IT(unsigned int idx, unsigned int line_time_ns)
{
	unsigned int idxs[TG_MAX_NUM] = {0};
	unsigned int fl_lc = 0;

	struct SensorInfo info = {0}; // for log using


	/* 1. query timestamp from CCU */
	idxs[0] = idx;

	/* get Vsync data by calling fs_algo func to call Frame Monitor */
	if (fs_alg_get_vsync_data(idxs, 1))
		LOG_INF("[FS IT] ERROR: Get Vsync data ERROR\n");


	/* 2. set FL regularly for testing */
	fl_lc = fl_table[fl_table_idxs[idx]];
	fl_lc = convert2LineCount(line_time_ns, fl_lc);

	/* log print info */
	fs_get_reg_sensor_info(idx, &info);
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
// Frame Sync entry function
/******************************************************************************/
// export some Frame Sync member function.
static struct FrameSync frameSync = {
	fs_streaming,
	fs_set_sync,
	fs_sync_frame,
	fs_set_shutter,
	fs_update_shutter,
	fs_update_tg,
	fs_update_auto_flicker_mode,
	fs_update_min_framelength_lc,
	fs_set_extend_framelength,
	fs_seamless_switch,
	fs_set_using_sa_mode,
	fs_set_frame_tag,
	fs_n_1_en,
	fs_mstream_en,
	fs_notify_vsync,
	fs_is_set_sync
};


/*
 * Frame Sync init function.
 *
 *    init FrameSync object.
 *    get FrameSync function for operation.
 *
 *    return: (0 / 1) => (no error / error)
 */
#if !defined(FS_UT)
unsigned int FrameSyncInit(struct FrameSync **pframeSync, struct device *dev)
#else // FS_UT
unsigned int FrameSyncInit(struct FrameSync (**pframeSync))
#endif // FS_UT
{
	int ret = 0;

	/* check NULL pointer */
	if (pframeSync == NULL) {
		LOG_MUST(
			"ERROR: pframeSync is NULL, return");
		ret = 1;

		return ret;
	}


	fs_init();
	*pframeSync = &frameSync;

#if !defined(FS_UT)
	ret = fs_con_create_sysfs_file(dev);
#endif // FS_UT

	return ret;
}


#if !defined(FS_UT)
void FrameSyncUnInit(struct device *dev)
{
	fs_con_remove_sysfs_file(dev);
}
#endif // FS_UT
/******************************************************************************/
