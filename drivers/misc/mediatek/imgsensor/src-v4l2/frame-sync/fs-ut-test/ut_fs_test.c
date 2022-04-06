// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>    /* Needed by sleep() function */

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_info.h"
#include "ut_fs_test_config.h"

#include "ut_fs_perframe_ctrl_sample_main_0.h"
#include "ut_fs_perframe_ctrl_sample_main_1.h"
#include "ut_fs_perframe_ctrl_sample_main_2.h"

#include "../frame_sync_util.h"
#include "../frame_monitor.h"


#define REDUCE_UT_DEBUG_PRINTF
#define USING_PRIVATE_CTRL_ORDER


/******************************************************************************/
// CMD printf color
/******************************************************************************/
#define NONE           "\033[m"
#define RED            "\033[0;32;31m"
#define LIGHT_RED      "\033[1;31m"
#define GREEN          "\033[0;32;32m"
#define LIGHT_GREEN    "\033[1;32m"
#define BLUE           "\033[0;32;34m"
#define LIGHT_BLUE     "\033[1;34m"
#define DARY_GRAY      "\033[1;30m"
#define CYAN           "\033[0;36m"
#define LIGHT_CYAN     "\033[1;36m"
#define PURPLE         "\033[0;35m"
#define LIGHT_PURPLE   "\033[1;35m"
#define BROWN          "\033[0;33m"
#define YELLOW         "\033[1;33m"
#define LIGHT_GRAY     "\033[0;37m"
#define WHITE          "\033[1;37m"
/******************************************************************************/


/******************************************************************************/
#define SHOW_TEXT() \
({ \
	printf(LIGHT_CYAN \
		">=============================================================================<\n"\
		NONE); \
	printf(LIGHT_CYAN \
		"!!!                        FrameSync UT Test Program                        !!!\n"\
		NONE); \
	printf(LIGHT_CYAN \
		">=============================================================================<\n"\
		NONE); \
})
/******************************************************************************/


static struct FrameSync *frameSync;


/******************************************************************************/
// about sensor data
/******************************************************************************/
static unsigned int set_synced_sensors[SENSOR_MAX_NUM] = {0};
static unsigned int g_sync_tag;

static struct ut_fs_streaming_sensor_list
			streaming_sensors[SENSOR_MAX_NUM] = {0};

static struct ut_fs_perframe_sensor_mode_list
			streaming_sensors_modes_list[SENSOR_MAX_NUM] = {0};

static unsigned int sensor_mode[SENSOR_MAX_NUM] = {0};


/* EXT CTRL config */
static struct ut_fs_test_ext_ctrl_cfg
			ext_ctrls[SENSOR_MAX_NUM] = {0};
/******************************************************************************/


/******************************************************************************/
// about timestamp
/******************************************************************************/
struct UT_Timestamp {
	unsigned int idx;
	unsigned int timestamp[VSYNCS_MAX];

	unsigned int curr_bias;
	unsigned int next_bias;
};
static struct UT_Timestamp ut_vts[SENSOR_MAX_NUM] = {0};

#define VDIFF_SYNC_SUCCESS_TH 1000
static unsigned int vdiff_sync_success_th;

// #define MANUALLY_SET_CURRENT_TIMESTAMP

static struct vsync_rec v_rec = {0};

static unsigned int query_tg_cnt;
/******************************************************************************/


/******************************************************************************/
// about per-frame ctrl and AE shutter
/******************************************************************************/
/* 1: CTRL Pair / 2: SA(StandAlone) */
static unsigned int g_user_alg_method;
static unsigned int g_alg_method;

static unsigned int g_auto_run;
static unsigned int g_run_times;
static unsigned int g_counter;
static unsigned int g_broke_at_counter;

// static unsigned int g_trigger_ext_fl;
// static unsigned int g_trigger_seamless_switch;


/* auto-run shutter */
static unsigned int g_shutter;
static unsigned int g_hdr_shutter[FS_HDR_MAX] = {0};
static const unsigned int g_hdr_divided_ratio[FS_HDR_MAX] = {
	1,
	2,
	4,
	5,
	6,
};

static unsigned int lock_exp;

#define EXP_TABLE_SIZE 6
static unsigned int exp_table_idx;
static const unsigned int exp_table[EXP_TABLE_SIZE] = {
	10002,
	19997,
	29996,
	40005,
	50004,
	60002,
};


/* flicker enable table */
static unsigned int lock_flk;
static const unsigned int flk_en_table[10] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static unsigned int flk_en_table_idx;


static unsigned int simulation_passed_vsyncs;
static unsigned int passed_vsyncs_ratio;
static unsigned int max_pass_cnt;
/******************************************************************************/


/******************************************************************************/
// about N:1 FrameSync mode
/******************************************************************************/
static struct ut_fs_test_n_1_mode_cfg n_1_cfg[SENSOR_MAX_NUM] = {0};
static unsigned int g_n_1_status[SENSOR_MAX_NUM] = {0};
static unsigned int g_n_1_min_fl_us[SENSOR_MAX_NUM] = {0};
static unsigned int g_n_1_f_cell_size[SENSOR_MAX_NUM] = {0};
/******************************************************************************/


/******************************************************************************/
// about FrameSync Algorithm stability test
/******************************************************************************/
static unsigned int g_fs_alg_stability_test_flag;
/******************************************************************************/





/******************************************************************************/
// FrameSync UT basic / utility functions
/******************************************************************************/
static inline int ut_get_sidx_by_sensor_id(
	unsigned int id, unsigned int *iidx)
{
	unsigned int i = 0;

	for (i = 0; ; ++i) {
		if (streaming_sensors[i].sensor == NULL)
			break;

		if (streaming_sensors[i].sensor->sensor_id == id) {
			*iidx = i;
			return streaming_sensors[i].sensor_idx;
		}
	}

	*iidx = -1;
	return -1;
}


static inline int ut_get_sid_by_sensor_idx(
	unsigned int idx, unsigned int *iidx)
{
	unsigned int i = 0;

	for (i = 0; ; ++i) {
		if (streaming_sensors[i].sensor == NULL)
			break;

		if (streaming_sensors[i].sensor_idx == idx) {
			*iidx = i;
			return streaming_sensors[i].sensor->sensor_id;
		}
	}

	*iidx = -1;
	return -1;
}


static inline void ut_dump_vsync_rec(struct vsync_rec (*pData))
{
	unsigned int i = 0;

	printf(GREEN
		"[UT dump_vsync_rec] buf->ids:%u, buf->cur_tick:%u, buf->tick_factor:%u\n"
		NONE,
		pData->ids,
		pData->cur_tick,
		pData->tick_factor);

	for (i = 0; i < pData->ids; ++i) {
		printf(GREEN
			"[UT dump_vsync_rec] buf->recs[%u]: id:%u (TG), vsyncs:%u, vts:(%u/%u/%u/%u)\n"
			NONE,
			i,
			pData->recs[i].id,
			pData->recs[i].vsyncs,
			pData->recs[i].timestamps[0],
			pData->recs[i].timestamps[1],
			pData->recs[i].timestamps[2],
			pData->recs[i].timestamps[3]);
	}
}


static inline void ut_select_frame_sync_algorithm(void)
{
	printf(GREEN
		"\n\n\n>>> Please choose FrameSync algorithm when testing! (1: CTRL Pair / 2: SA(StandAlone)) <<<\n"
		NONE);
	printf(LIGHT_PURPLE
		">>> (Input 1 integer) \"select a algorithm\" : "
		NONE);
	scanf("%u", &g_user_alg_method);
}


static inline unsigned int ut_auto_choose_fs_alg_method(void)
{
	return (g_alg_method == 0)
		? (g_user_alg_method == 2) ? 1 : 0
		: (g_alg_method == 2) ? 1 : 0;
}
/******************************************************************************/





/******************************************************************************/
// Test FrameSync Driver behavior
/******************************************************************************/
static void test_fs_register_sensor_behavior(void)
{
	/* Test fs_register_sensor -> fs_push_sensor */
	printf(
		"/*************************************************************/\n"
		);
	printf("/* --- Test fs_register_sensor function behavior. --- */\n");
	printf("\n\n\n");
	printf(">>> Scenario : already register 3 sensors. (sidx:0/2/4)\n");
	printf("\n\n\n");

	frameSync->fs_streaming(1, main0.sensor);
	printf("\n");
	frameSync->fs_streaming(1, main1.sensor);
	printf("\n");
	frameSync->fs_streaming(1, main2.sensor);

	printf("\n\n\n");


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
	{
		printf("/* [--- REGISTER_BY_SENSOR_ID ---] */\n\n");

		struct fs_streaming_st *push_sensor;
		struct fs_streaming_st clear_st = {0};

		push_sensor = calloc(SENSOR_MAX_NUM,
					sizeof(struct fs_streaming_st));


		for (int run = 0; run < 2; ++run) {
			printf(
				">>>>>>>>>>>>>>>>>>>> Test Run %d <<<<<<<<<<<<<<<<<<<<\n"
				, run+1);

			printf(
				"[--- Already register %d sensors, register more... ---]\n\n"
				, run+3);

			for (unsigned int i = 0; i < SENSOR_MAX_NUM; ++i) {
				printf("\n=== Test register:%2d ===\n", i);
				push_sensor[i].sensor_id = i + run * 10;
				frameSync->fs_streaming(1, &push_sensor[i]);
			}

			printf(
				"\n=== Test Run %d DONE, unregister ===\n",
				run+1);

			for (unsigned int i = 0; i < SENSOR_MAX_NUM; ++i) {
				printf("\n=== Test unregister:%2d ===\n", i);
				clear_st.sensor_id = i + run * 10;
				frameSync->fs_streaming(0, &clear_st);
			}
			printf("\n\n");
		}

		printf(
			">>>>>>>>>>>>>>>>>>>> Test Run 3 (register EXSIST sensor) <<<<<<<<<<<<<<<<<<<<\n\n"
			);
		for (unsigned int i = 0; i < SENSOR_MAX_NUM; ++i) {
			printf("\n=== Test register:%2d ===\n", i);
			push_sensor[i].sensor_id = i;
			frameSync->fs_streaming(1, &push_sensor[i]);
		}

		printf("\n=== Test Run 3 DONE, unregister ===\n");

		for (unsigned int i = 0; i < SENSOR_MAX_NUM; ++i) {
			printf("\n=== Test unregister:%2d ===\n", i);
			clear_st.sensor_id = i;
			frameSync->fs_streaming(0, &clear_st);
		}


		free(push_sensor);
	}
		break;

	case BY_SENSOR_IDX:
	{
		printf("/* [--- REGISTER_BY_SENSOR_IDX ---] */\n\n");

		struct fs_streaming_st *push_sensor;
		struct fs_streaming_st clear_st = {0};

		push_sensor = calloc(SENSOR_MAX_NUM,
					sizeof(struct fs_streaming_st));


		printf(">>>>>>>>>>>>>>>>>>>> Test Run 1 <<<<<<<<<<<<<<<<<<<<\n");
		printf(
			"[--- Already register 3 sensors, register idx 1 and 3... ---]\n\n"
			);

		for (unsigned int i = 0; i < SENSOR_MAX_NUM; ++i) {
			printf("\n=== Test register:%2d ===\n", i);
			push_sensor[i].sensor_id = 0x999;
			push_sensor[i].sensor_idx = i * 2 + 1;
			frameSync->fs_streaming(1, &push_sensor[i]);
		}

		printf("\n=== Test Run 1 DONE, unregister ===\n");

		for (unsigned int i = 0; i < 2; ++i) {
			printf("\n=== Test unregister:%2d ===\n", i);
			clear_st.sensor_idx = i * 2 + 1;
			frameSync->fs_streaming(0, &clear_st);
		}

		printf("\n\n\n");

		printf(
			">>>>>>>>>>>>>>>>>>>> Test Run 2 (register EXSIST sensor) <<<<<<<<<<<<<<<<<<<<\n\n"
			);
		for (unsigned int i = 0; i < 2; ++i) {
			printf("\n=== Test register:%2d ===\n", i);
			push_sensor[i].sensor_idx = 3;
			frameSync->fs_streaming(1, &push_sensor[i]);
		}

		printf("\n=== Test Run 2 DONE, unregister ===\n");

		for (unsigned int i = 0; i < 2; ++i) {
			printf("\n=== Test unregister:%2d ===\n", i);
			clear_st.sensor_idx = 3;
			frameSync->fs_streaming(0, &clear_st);
		}

		printf("\n\n");
	}
		break;

	default:
		printf(
			"\n=== Run in defalut case, not assign register method ===\n");
		break;
	}


	printf("\n\n");

	printf("//--- Test fs_register_sensor function behavior END! ---//\n");
}


static void test_streaming_behavior(void)
{
	/* Test streaming */
	printf("\n");
	printf(
		"/*************************************************************/\n"
		);
	printf("//--- Test streaming behavior. ---//\n");

	// Test stream off.
	printf("\n--- Test stream off behavior. ---\n");

	struct fs_streaming_st sensor_info;

	sensor_info.sensor_id = 0x30D5;
	sensor_info.sensor_idx = 2;
	frameSync->fs_streaming(0, &sensor_info);
	// restore.
	printf("\n--- Test stream off behavior.(restore) ---\n");
	frameSync->fs_streaming(1, main1.sensor);


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		frameSync->fs_set_sync(sensor_info.sensor_id, 1);
		break;

	case BY_SENSOR_IDX:
		frameSync->fs_set_sync(sensor_info.sensor_idx, 1);
		break;

	default:
		frameSync->fs_set_sync(sensor_info.sensor_idx, 1);
		break;
	}


	printf("//--- Test stream off behavior END! ---//\n");
}


static void test_syncFrame_behavior(void)
{
	/* Test fs_sync_frame behavior. */
	printf("\n");
	printf(
		"/*************************************************************/\n"
		);
	printf("//--- Test fs_sync_frame behavior. ---//\n");

	printf("\n//=== Not sync start, call sync end. ===//\n");
	frameSync->fs_sync_frame(0);
	printf("\n\n");

	printf("//=== unset some sensor sync ===//\n");


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		frameSync->fs_set_sync(0x481, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(0x481, 1);
		frameSync->fs_set_sync(0x481, 0);
		frameSync->fs_set_sync(0x30D5, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(0x481, 1);
		frameSync->fs_set_sync(0x30D5, 1);

		break;

	case BY_SENSOR_IDX:
		frameSync->fs_set_sync(4, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(4, 1);
		frameSync->fs_set_sync(4, 0);
		frameSync->fs_set_sync(2, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(4, 1);
		frameSync->fs_set_sync(2, 1);

		break;

	default:
		frameSync->fs_set_sync(4, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(4, 1);
		frameSync->fs_set_sync(4, 0);
		frameSync->fs_set_sync(2, 0);
		printf("\n");
		frameSync->fs_sync_frame(1);
		printf("\n");
		frameSync->fs_set_sync(4, 1);
		frameSync->fs_set_sync(2, 1);

		break;
	}


	printf("//--- Test fs_sync_frame behavior END! ---//\n");
}


void run_fs_control_test(void)
{
	unsigned int ret = 0;

	ret = FrameSyncInit(&frameSync);
	if (ret != 0) {
		printf(RED ">>> frameSync Init failed !\n" NONE);
		return;
	}


	printf("\n");
	printf(
		"/*************************************************************/\n"
		);
	printf(
		"/* ---         Run FrameSync Driver Control Test         --- */\n"
		);

	test_fs_register_sensor_behavior();
	test_streaming_behavior();
	test_syncFrame_behavior();

	printf("\n\n\n");
	printf(
		"/* ---         END FrameSync Driver Control Test         --- */\n"
		);
	printf(
		"/*************************************************************/\n"
		);
	printf("\n");
}


void *ut_test_data_racing(void *ut_fs_test_sensor_cfg)
{
	struct ut_fs_test_sensor_cfg *arg =
		(struct ut_fs_test_sensor_cfg *) ut_fs_test_sensor_cfg;

	unsigned int ret;


	ret = FrameSyncInit(&frameSync);

	printf("***### frameSync addr = %p (sensor_idx=%2d) ###***\n",
		frameSync,
		arg->sensor_idx);

	//frameSync->fs_streaming(arg->sensor_idx, 1, (arg->sensor));
	frameSync->fs_streaming(1, (arg->sensor));

	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_id);
		if (ret == 0)
			frameSync->fs_set_sync(arg->sensor->sensor_id, 1);

		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_id);
		break;

	case BY_SENSOR_IDX:
	default:
		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);
		if (ret == 0)
			frameSync->fs_set_sync(arg->sensor->sensor_idx, 1);

		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);
		break;
	}


	pthread_exit(NULL);
}


void run_fs_data_racing_test(void)
{
	pthread_t thread[3];

	pthread_create(&thread[0], NULL, ut_test_data_racing, (void *)&main0);
	pthread_create(&thread[1], NULL, ut_test_data_racing, (void *)&main1);
	pthread_create(&thread[2], NULL, ut_test_data_racing, (void *)&main2);

	pthread_join(thread[0], NULL);
	pthread_join(thread[1], NULL);
	pthread_join(thread[2], NULL);

	/* for triggering print out newest status */
	frameSync->fs_sync_frame(1);
}
/******************************************************************************/


/**
 * called by exe_fs_alg_stability_test_item(),
 * in exe_fs_alg_stability_test() using multi-thread
 */
void *ut_set_fs_streaming_and_synced(void *ut_fs_test_sensor_cfg)
{
	unsigned int ret;
	unsigned int sync_tag = 0, alg_method = 0;
	struct ut_fs_test_sensor_cfg *sensor_cfg =
		(struct ut_fs_test_sensor_cfg *)ut_fs_test_sensor_cfg;
	struct fs_streaming_st s_sensor = {0};


	ret = FrameSyncInit(&frameSync);
	if (ret != 0) {
		printf(RED ">>> frameSync Init failed !\n" NONE);
		pthread_exit(NULL);
	}

	printf(GREEN
		">>> frameSync addr = %p (sensor_idx=%2d)\n"
		NONE,
		frameSync,
		sensor_cfg->sensor_idx);


	/* 0. setup ut streaming data */
	s_sensor = *sensor_cfg->sensor;
	s_sensor.sensor_idx = sensor_cfg->sensor_idx;
	s_sensor.tg = sensor_cfg->tg;


	/* 1. call fs_streaming() */
	frameSync->fs_streaming(1, &s_sensor);


	/* 2. call fs_set_sync() */
	/* setup FrameSync algorithm for running */
	alg_method = ut_auto_choose_fs_alg_method();
	frameSync->fs_set_using_sa_mode(alg_method);

	sync_tag = g_sync_tag;
	set_synced_sensors[sensor_cfg->sensor_idx] = 1;

	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		frameSync->fs_set_sync(s_sensor.sensor_id, sync_tag);
		// ret = frameSync->fs_is_set_sync(s_sensor.sensor_id);
		break;

	case BY_SENSOR_IDX:
		frameSync->fs_set_sync(s_sensor.sensor_idx, sync_tag);
		// ret = frameSync->fs_is_set_sync(s_sensor.sensor_idx);
		break;

	default:
		frameSync->fs_set_sync(s_sensor.sensor_idx, sync_tag);
		// ret = frameSync->fs_is_set_sync(s_sensor.sensor_idx);
		break;
	}


	pthread_exit(NULL);
}


/**
 * currently no one use this api
 */
void test_sync_2_sensors(void)
{
	// TODO: run times should not be hardcode.
	for (unsigned int i = 0; i < 4; ++i) {
		struct vsync_rec rec = {0};

		rec.ids = 2;
		rec.tick_factor = TICK_FACTOR;
		rec.recs[0].id = main0.sensor->tg;
		rec.recs[0].vsyncs = pf_settings_main0[i].vsyncs;
		rec.recs[0].timestamps[0] =
			pf_settings_main0[i].last_vsync_timestamp;

		rec.recs[1].id = main1.sensor->tg;
		rec.recs[1].vsyncs = pf_settings_main1[i].vsyncs;
		rec.recs[1].timestamps[0] =
			pf_settings_main1[i].last_vsync_timestamp;

		rec.cur_tick = (
			rec.recs[0].timestamps[0] > rec.recs[1].timestamps[0])
			? rec.recs[0].timestamps[0] + 100
			: rec.recs[1].timestamps[0] + 100;
		rec.cur_tick *= rec.tick_factor;

		frm_debug_set_last_vsync_data(&rec);

		printf("//--- fs_sync_frame:%d ---//\n", i+1);
		frameSync->fs_sync_frame(1);
		frameSync->fs_set_shutter(pf_settings_main0[i].pf_settings);
		frameSync->fs_set_shutter(pf_settings_main1[i].pf_settings);
		frameSync->fs_set_shutter(&frameCtrl_main2[0]);
		frameSync->fs_sync_frame(0);
		printf("//--- fs_sync_frame:%d END!!! ---//\n", i+1);
		printf("\n\n\n");
	}
}
/******************************************************************************/


/*
 * called by ut_generate_vsync_data_pf_auto()
 * only update data query by the TG requested
 *
 * input:
 *     idx: idx of vsync_rec->recs[] (0 ~ TG_MAX_NUM)
 *     vsyncs: how many vsync want to let it passed (pf => 1)
 *
 * output:
 *     p_v_rec: pointer of output data structure
 */
static void ut_gen_timestamps_data(
	unsigned int idx, unsigned int vsyncs,
	struct vsync_rec *(p_v_rec))
{
	unsigned int i = 0, index = 0, choose = 0;
	unsigned int fl_us[2] = {0}, sensor_curr_fl_us = 0;
	unsigned int ref_idx = 0, ref_idx_next = 0;
	unsigned int cur_tick = 0;


	if (p_v_rec->recs[idx].id == 0)
		return;

	index = frm_get_instance_idx_by_tg(p_v_rec->recs[idx].id);

	if (index < 0)
		return;


	frm_get_predicted_fl_us(index, fl_us, &sensor_curr_fl_us);


	ut_vts[idx].curr_bias = ut_vts[idx].next_bias;
	frm_get_next_vts_bias_us(index, &ut_vts[idx].next_bias);


	/* calculate timestamp according to pass vsyncs count */
	for (i = 0; i < vsyncs; ++i) {
		ref_idx = ut_vts[idx].idx;
		ref_idx_next = (ref_idx + 1) % VSYNCS_MAX;

		if (i < 2) {
			choose = (i > 0) ? 1 : 0;
			ut_vts[idx].timestamp[ref_idx_next] =
					ut_vts[idx].timestamp[ref_idx] +
					fl_us[choose];
		} else {
			ut_vts[idx].timestamp[ref_idx_next] =
					ut_vts[idx].timestamp[ref_idx] +
					sensor_curr_fl_us;
		}

		ut_vts[idx].idx = ref_idx_next;
	}


	/* update cur_tick for a query only query "1" TG data */
	/* UT default set to (SOF + 100) us */
	cur_tick =
		(ut_vts[idx].timestamp[ut_vts[idx].idx] + 100) *
		p_v_rec->tick_factor;

	p_v_rec->cur_tick =
		check_tick_b_after_a(cur_tick, p_v_rec->cur_tick)
		? p_v_rec->cur_tick
		: cur_tick;



#if defined(REDUCE_UT_DEBUG_PRINTF)
	printf(GREEN
		"[ut_gen_timestamps_data] [%u] FRM pred_fl(c:%u, n:%u)/vts_bias(c:%u, n:%u), ut_vts[%u].ts(%u/%u/%u/%u), vsyncs:%u, cur_tick:%u  [p_v_rec->recs[%u].id:%u (TG)]\n"
		NONE,
		idx,
		fl_us[0], fl_us[1],
		ut_vts[idx].curr_bias, ut_vts[idx].next_bias,
		idx,
		ut_vts[idx].timestamp[0],
		ut_vts[idx].timestamp[1],
		ut_vts[idx].timestamp[2],
		ut_vts[idx].timestamp[3],
		vsyncs,
		p_v_rec->cur_tick,
		idx,
		p_v_rec->recs[idx].id);
#endif


	/* copy data out */
	ref_idx = ut_vts[idx].idx;
	p_v_rec->recs[idx].vsyncs = vsyncs;
	for (i = 0; i < VSYNCS_MAX; ++i) {
		p_v_rec->recs[idx].timestamps[i] =
				ut_vts[idx].timestamp[ref_idx];

		ref_idx = (ref_idx + (VSYNCS_MAX - 1)) % VSYNCS_MAX;
	}


#if !defined(REDUCE_UT_DEBUG_PRINTF)
	/* for monitor global static struct data */
	ut_dump_vsync_rec(p_v_rec);
#endif
}


/*
 * called by ut_set_fs_set_shutter()
 * for faster setup pf_ctrl data structure
 *
 * input:
 *     idx: idx of streaming_sensors[]
 *     mode: mode number from user selected
 *
 * input/output:
 *     p_pf_ctrl: copy needed variable out
 */
static inline void set_pf_ctrl_s_mode(
	struct fs_perframe_st *p_pf_ctrl,
	unsigned int idx, unsigned int mode)
{
	*p_pf_ctrl = streaming_sensors_modes_list[idx]
				.mode_list[mode];

	/* overwrite default idx & id data */
	p_pf_ctrl->sensor_id =
		streaming_sensors[idx].sensor->sensor_id;
	p_pf_ctrl->sensor_idx =
		streaming_sensors[idx].sensor_idx;
}



static void generate_shutter_value(void)
{
	unsigned int i = 0;

	/* 0 => idx-- ; 1 => idx++ ; 2 => idx+0 */
	unsigned int exp_table_idx_dir = 0;


	if (lock_exp)
		goto SETUP_AUTO_TEST_EXP;


	exp_table_idx_dir = rand() % 3;


	/* update exp_table_idx value */
	if (exp_table_idx_dir == 1) { // idx++
		// boundary check
		if (exp_table_idx < (EXP_TABLE_SIZE - 1))
			exp_table_idx++;
	} else if (exp_table_idx_dir == 0) { // idx--
		// boundary check
		if (exp_table_idx > 0)
			exp_table_idx--;
	}


SETUP_AUTO_TEST_EXP:
	/* for normal sensor auto-run shutter value*/
	g_shutter = exp_table[exp_table_idx];


	/* for hdr auto-run shutter values */
	for (i = 0; i < FS_HDR_MAX; ++i) {
		g_hdr_shutter[i] =
			exp_table[exp_table_idx] / g_hdr_divided_ratio[i];
	}
}


static int
print_and_select_sensor(struct ut_fs_streaming_sensor_list s_list[])
{
	unsigned int i = 0;
	int select = 0;

	/* print out sensors */
	for (i = 0; s_list[i].sensor != NULL; ++i) {
		printf(GREEN "[%d] %s\t" NONE,
				i, s_list[i].sensor_name);
		if ((i >= 10) && (i % 10 == 0))
			printf("\n");
	}
	printf(GREEN "[-1] END\n" NONE);


	/* select a sensor */
	printf(LIGHT_PURPLE
		">>> (Input 1 integer) \"select a sensor\" : "
		NONE);
	scanf("%d", &select);

	while (select > 0 && select >= i) { // no such idx's sensor in array
		printf(RED
			">>> NO such idx's sensor in list, please select again !\n"
			NONE);

		/* select a sensor */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"select a sensor\" : "
			NONE);
		scanf("%d", &select);
	}

	return select;
}


static unsigned int
print_and_select_s_mode(
	struct ut_fs_perframe_sensor_mode_list s_mode_list[],
	int idx)
{
	unsigned int i = 0, select = 0;

	/* print out sensor's mode list */
	for (i = 0;
		streaming_sensors_modes_list[idx]
			.mode_list[i].sensor_id != 0; ++i) {

		printf(GREEN
			"[%d] ID:%#x (sidx:%u), margin_lc:%3u, lineTimeInNs:%7u, pclk:%10u, linelength:%6u, hdr_exp.mode_exp_cnt:%u\n"
			NONE,
			i,
			streaming_sensors_modes_list[idx]
					.mode_list[i].sensor_id,
			streaming_sensors_modes_list[idx]
					.sensor_idx,
			streaming_sensors_modes_list[idx]
					.mode_list[i].margin_lc,
			streaming_sensors_modes_list[idx]
					.mode_list[i].lineTimeInNs,
			streaming_sensors_modes_list[idx]
					.mode_list[i].pclk,
			streaming_sensors_modes_list[idx]
					.mode_list[i].linelength,
			streaming_sensors_modes_list[idx]
					.mode_list[i].hdr_exp.mode_exp_cnt);
	}

	/* select sensor mode */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) \"select a sensor mode\" : "
		NONE);
	scanf("%d", &select);

	while (select < 0 || select >= i) { // no such idx's sensor mode in above list
		printf(RED
			">>> NO such idx's sensor in list, please select again !\n"
			NONE);

		/* select a sensor */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"select a sensor mode\" : "
			NONE);
		scanf("%d", &select);
	}

	return select;
}


/**
 * called by test_frame_sync_proc(), exe_fs_alg_stability_test()
 * before all processing going
 */
static inline void reset_ut_test_variables(void)
{
	unsigned int i = 0, j = 0;

	struct ut_fs_streaming_sensor_list s_sensor_clear_st = {0};
	struct ut_fs_perframe_sensor_mode_list s_sensor_mode_clear_st = {0};


	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		set_synced_sensors[i] = 0;

		streaming_sensors[i] = s_sensor_clear_st;
		streaming_sensors_modes_list[i] = s_sensor_mode_clear_st;

		sensor_mode[i] = 0;


		ut_vts[i].idx = 0;
		for (j = 0; j < VSYNCS_MAX; ++j)
			ut_vts[i].timestamp[j] = 0;
		ut_vts[i].curr_bias = 0;
		ut_vts[i].next_bias = 0;


		g_n_1_status[i] = 0;
		g_n_1_min_fl_us[i] = 0;
		g_n_1_f_cell_size[i] = 0;
	}


	exp_table_idx = 0;
	flk_en_table_idx = 0;
}


static bool ut_check_pf_sync_result(struct vsync_rec *(p_v_rec))
{
	unsigned int i = 0, biggest_vts = 0;
	unsigned int vts[TG_MAX_NUM] = {0};
	unsigned int vdiff[SENSOR_MAX_NUM] = {0};
	bool is_success = true;


	/* 0. prepare vsync timestamp data for using */
	for (i = 0; i < TG_MAX_NUM; ++i) {
		vts[i] =
			p_v_rec->recs[i].timestamps[0] +
			// p_v_rec->recs[i].timestamps[1] +
			ut_vts[i].curr_bias;
	}


	/* 1. find out lastest vsync timestamp */
	for (i = 0; i < TG_MAX_NUM; ++i)
		if (vts[i] > biggest_vts)
			biggest_vts = vts[i];


	/* 2. check each vsync timestamp diff and */
	/*    check if sync successfully */
	for (i = 0; i < TG_MAX_NUM; ++i) {
		if (vts[i] == 0)
			continue;

		vdiff[i] = biggest_vts - vts[i];

	//  for debug
	//  printf(RED
	//  ">>> UT: [%u] vdiff:%u, biggest_vts:%u, vts:%u, bias(c:%u, n:%u)\n\n\n"
	//  NONE,
	//  i, vdiff[i], biggest_vts,
	//  p_v_rec->recs[i].timestamps[0],
	//  ut_vts[i].curr_bias,
	//  ut_vts[i].next_bias);

		if (vdiff[i] > vdiff_sync_success_th)
			is_success = false;
	}


	/* 3. highlight result for checking if sync failed */
	if (is_success)
		printf(GREEN ">>> UT: PF sync PASS\n\n\n" NONE);
	else {
		printf(RED ">>> UT: PF sync FAIL\n" NONE);

		for (i = 0; i < TG_MAX_NUM; ++i) {
			printf(RED
				">>> UT: [%u] tg:%u, vsyncs:%u, vdiff:%u, vts:%u, bias(c:%u, n:%u)\n"
				NONE,
				i,
				p_v_rec->recs[i].id,
				p_v_rec->recs[i].vsyncs,
				vdiff[i],
				p_v_rec->recs[i].timestamps[0],
				ut_vts[i].curr_bias,
				ut_vts[i].next_bias);
		}
		printf("\n\n\n");
	}


	return is_success;
}


static void
ut_generate_vsync_data_pf_auto(struct vsync_rec *(p_v_rec))
{
	unsigned int i = 0;
	unsigned int biggest_vts = 0, passed_vsyncs = 0;


	passed_vsyncs = rand() % 100;
	passed_vsyncs = (passed_vsyncs < passed_vsyncs_ratio) ? 1 : 0;
	if (passed_vsyncs)
		passed_vsyncs = rand() % max_pass_cnt;
	passed_vsyncs++; // to 1~2


	for (i = 0; i < TG_MAX_NUM; ++i) {
		ut_gen_timestamps_data(i, passed_vsyncs, p_v_rec);

		// if (p_v_rec->recs[i].timestamps[0] > biggest_vts)
		if (check_tick_b_after_a(
				biggest_vts, p_v_rec->recs[i].timestamps[0]))
			biggest_vts = p_v_rec->recs[i].timestamps[0];
	}

	p_v_rec->cur_tick = (biggest_vts + 100) * p_v_rec->tick_factor;


	printf(GREEN
		"[UT generate_vsync_data_pf_auto] query ids:%u, p_v_rec->cur_tick:%u, tick_factor:%u\n"
		NONE,
		p_v_rec->ids,
		p_v_rec->cur_tick,
		p_v_rec->tick_factor);


#if !defined(REDUCE_UT_DEBUG_PRINTF)
	/* for monitor global static struct data */
	ut_dump_vsync_rec(p_v_rec);
#endif


	frm_debug_set_last_vsync_data(p_v_rec);

	printf("\n\n\n");
}


static struct vsync_rec
ut_generate_vsync_data_manually(
	struct ut_fs_streaming_sensor_list     s_list[],
	struct ut_fs_perframe_sensor_mode_list m_list[])
{
	unsigned int i = 0, j = 0;
	unsigned int idx = 0, biggest_vts = 0;
	long long input = 0;

	struct vsync_rec v_rec = {0};

	v_rec.tick_factor = TICK_FACTOR;


	printf(GREEN
		"\n\n\n>>> UT Generate Vsync Data \"Manually\" <<<\n"
		NONE);
	printf(LIGHT_RED
		">>> !!! Please set Vsync data carefully !!!\n\n\n"
		NONE);


	for (i = 0; ; ++i) {
		/* print sensor which you can select */
		printf(GREEN
			"Please select bellow sensors for \"setting vsync data\"\n"
			NONE);

		/* using CLI for choise */
		input = print_and_select_sensor(s_list);

		if (input < 0) {
			printf("\n\n\n");
			break;
		}

		idx = input;



		/* 1. set tg */
		v_rec.ids++;
		// v_rec.recs[idx].id = s_list[idx].sensor->tg;
		v_rec.recs[idx].id = streaming_sensors[i].tg;


		/* 2. set passed Vsyncs count */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"set passed vsync count (min:1)\" : "
			NONE);
		scanf("%lld", &input);

		if (input <= 0) {
			printf(RED
				">>> You set vsync passed count less than 1, so auto force to 1 !\n"
				NONE);

			// force vsync count to 1.
			input = 1;
		}

		v_rec.recs[idx].vsyncs = input;


		/* 3. set lastest Vsync timestamp value */
		// for (unsigned int i = 0; i < VSYNCS_MAX; ++i) {
		for (j = 0; j < 1; ++j) {
			printf(LIGHT_PURPLE
				">>> (Input 1 integer) \"timestamps[%d]\" : "
				NONE, j);
			scanf("%lld", &input);
			v_rec.recs[idx].timestamps[j] = input;
			ut_vts[idx].timestamp[j] = input;

			//if (input > biggest_vts)
			if (check_tick_b_after_a(biggest_vts, input))
				biggest_vts = input;
		}


		printf("\n\n\n");
	}


#ifdef MANUALLY_SET_CURRENT_TIMESTAMP

	printf(GREEN
		"Please reference bellow timestamp to \"determine and input current timestamp\"\n"
		NONE);
	for (j = 0; s_list[j].sensor != NULL; ++j) {
		printf(GREEN
			"[%d] %s, last Vsync timestamp:%u\n"
			NONE,
			j, s_list[j].sensor_name, v_rec.recs[j].timestamps[0]);
	}

	printf(LIGHT_PURPLE
		">>> (Input 1 integer) \"set current timestamp\" : "
		NONE);
	scanf("%lld", &input);

	v_rec.cur_tick = input * v_rec.tick_factor;

#else // END manual set current timestamp

	/* (biggest_vts + 1) => prevent overflow */
	v_rec.cur_tick = (biggest_vts + 1) * v_rec.tick_factor;

#endif // MANUALLY_SET_CURRENT_TIMESTAMP

	// printf("[0] tg:%u, [1] tg:%u, [2] tg:%u\n",
	//	v_rec.recs[0].id,
	//	v_rec.recs[1].id,
	//	v_rec.recs[2].id);

	printf(GREEN
		">>> frame monitor set vsync data, call frm_debug_set_last_vsync_data()...\n\n\n"
		NONE);

	frm_debug_set_last_vsync_data(&v_rec);


	printf("\n\n\n");

	return v_rec;
}


/**
 * called by test_frame_sync_proc()
 * for step by step setting to test frame-sync
 */
static void ut_set_fs_streaming(void)
{
	int select = 2147483647, input;

	unsigned int idx = 0, tg = 0;
	unsigned int i = 0, j = 0;


	printf("\n\n\n");

	/* for fs_streaming(), streaming on sensors */
	for (i = 0; ; ++i) {
		/* print sensor info which in ut_fs_streaming_info.h */
		printf(LIGHT_CYAN ">>> fs_streaming() ...\n" NONE);
		printf(GREEN
			"Please select bellow sensors for \"streaming on\"\n"
			NONE);

		/* using CLI for choise */
		select = print_and_select_sensor(ut_fs_s_list);

		if (select < 0) { // => END streaming on sensor
			streaming_sensors[i].sensor_name = "NULL";
			streaming_sensors[i].sensor = NULL;

			streaming_sensors_modes_list[i]
						.sensor_name = "NULL";
			streaming_sensors_modes_list[i]
						.mode_list = NULL;

			printf("\n\n\n");

			break;
		}


		/* set sensor idx which you select on bellow step */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"set sensor_idx\" : "
			NONE);
		scanf("%d", &input);
		idx = input;

		streaming_sensors[i].sensor_name =
					ut_fs_s_list[select].sensor_name;

		streaming_sensors[i].sensor =
					ut_fs_s_list[select].sensor;

		streaming_sensors[i].sensor->sensor_idx = idx;

		/* !!! Becareful !!! */
		/* prevent choose same sensor with different sensor idx */
		/* fs_set_sync() will choose wrong sensor idx */
		/* because "fs_streaming_st" memory address is the same */
		streaming_sensors[i].sensor_idx = idx;


		/* set sensor tg */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"set TG\" : "
			NONE);
		scanf("%d", &input);
		tg = input;

		streaming_sensors[i].sensor->tg = tg;

		/* !!! Becareful !!! */
		/* prevent choose same sensor with different tg */
		/* fs aglo, fs monitor will be error */
		/* because "fs_streaming_st" memory address is the same */
		streaming_sensors[i].tg = tg;



		/* query sensor mode info for ut test using */
		/* (in ut_fs_perframe_ctrl_info.h file) */
		for (j = 0; ut_fs_pf_s_mode_list[j].mode_list != NULL; ++j) {
			if (strcmp(
				streaming_sensors[i].sensor_name,
				ut_fs_pf_s_mode_list[j].sensor_name) == 0) {

				streaming_sensors_modes_list[i].sensor_name =
					ut_fs_pf_s_mode_list[j].sensor_name;

				streaming_sensors_modes_list[i].sensor_idx = idx;

				streaming_sensors_modes_list[i].mode_list =
					ut_fs_pf_s_mode_list[j].mode_list;
			}
		}


		/* set whether streaming on or not */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"set streaming ON:1 / OFF:0\" : "
			NONE);
		scanf("%d", &input);

		if (input > 0) {
			printf(GREEN
				">>> framesync streaming ON, call fs_streaming(1, )...\n\n\n"
				NONE);

			frameSync->fs_streaming(1, streaming_sensors[i].sensor);
		} else {
			printf(GREEN
				">>> framesync streaming OFF, call fs_streaming(0, )...\n\n\n"
				NONE);

			frameSync->fs_streaming(0, streaming_sensors[i].sensor);
		}
		printf("\n\n\n");
	}
	/* END for fs_streaming(), streaming on sensors */
}


/**
 * called by test_frame_sync_proc()
 * for step by step setting to test frame-sync
 */
static void ut_set_fs_set_sync(void)
{
	int select = 2147483647, input;

	unsigned int i = 0, alg_method = 0;


	alg_method = ut_auto_choose_fs_alg_method();
	frameSync->fs_set_using_sa_mode(alg_method);

	/* for fs_set_sync(), set sensor for doing frame sync */
	for (i = 0; ; ++i) {
		/* print sensor which you can select */
		printf(LIGHT_CYAN ">>> fs_set_sync() ...\n" NONE);
		printf(GREEN
			"Please select bellow sensors for \"doing frame sync\"\n"
			NONE);

		/* using CLI for choise */
		select = print_and_select_sensor(streaming_sensors);

		if (select < 0) { // => END set sensor sync
			printf("\n\n\n");
			break;
		}


		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"set sync (Yes:1 / No:0)\" : "
			NONE);
		scanf("%d", &input);
		set_synced_sensors[select] = input;

		if (input > 0) {
			printf(GREEN
				">>> framesync set sync, call fs_set_sync( ,1)...\n\n\n"
				NONE);
		} else {
			printf(GREEN
				">>> framesync unset sync, call fs_set_sync( ,0)...\n\n\n"
				NONE);
		}


		switch (REGISTER_METHOD) {
		case BY_SENSOR_ID:
			frameSync->fs_set_sync(
				streaming_sensors[select].sensor->sensor_id,
				set_synced_sensors[select]);

			break;

		case BY_SENSOR_IDX:
			frameSync->fs_set_sync(
				streaming_sensors[select].sensor_idx,
				set_synced_sensors[select]);

			break;

		default:
			frameSync->fs_set_sync(
				streaming_sensors[select].sensor_idx,
				set_synced_sensors[select]);

			break;
		}

		printf("\n\n\n");
	}
	/* END for fs_set_sync(), set sensor for doing frame sync */
}


static inline void
ut_set_fs_update_auto_flicker_mode(struct fs_perframe_st *pf_ctrl)
{
	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		frameSync->fs_update_auto_flicker_mode(
				pf_ctrl->sensor_id,
				pf_ctrl->flicker_en);

		break;

	case BY_SENSOR_IDX:
		frameSync->fs_update_auto_flicker_mode(
				pf_ctrl->sensor_idx,
				pf_ctrl->flicker_en);

		break;

	default:
		frameSync->fs_update_auto_flicker_mode(
				pf_ctrl->sensor_idx,
				pf_ctrl->flicker_en);

		break;
	}
}


static inline void
ut_set_fs_update_min_framelength_lc(struct fs_perframe_st *pf_ctrl)
{
	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
		frameSync->fs_update_min_framelength_lc(
				pf_ctrl->sensor_id,
				pf_ctrl->min_fl_lc);

		break;

	case BY_SENSOR_IDX:
		frameSync->fs_update_min_framelength_lc(
				pf_ctrl->sensor_idx,
				pf_ctrl->min_fl_lc);

		break;

	default:
		frameSync->fs_update_min_framelength_lc(
				pf_ctrl->sensor_idx,
				pf_ctrl->min_fl_lc);

		break;
	}
}


static inline void
ut_auto_set_anti_flicker(struct fs_perframe_st *pf_ctrl)
{
	if ((pf_ctrl->sensor_idx == 0) && (!lock_flk)) {
		// only main cam
		pf_ctrl->flicker_en =
			flk_en_table[flk_en_table_idx];

		flk_en_table_idx = (flk_en_table_idx + 1) % 10;
	} else
		pf_ctrl->flicker_en = 0;
}


static inline void ut_setup_n_1_cfg(
	struct ut_fs_test_n_1_mode_cfg *n_1_cfg_arr)
{
	unsigned int i = 0;
	struct ut_fs_test_n_1_mode_cfg clear_st = {0};


	if (n_1_cfg_arr == NULL) {
		for (i = 0; i < SENSOR_MAX_NUM; ++i)
			n_1_cfg[i] = clear_st;

		return;
	}

	for (i = 0;
		(n_1_cfg_arr[i].turn_on_at_n_run != 0
			&& n_1_cfg_arr[i].turn_off_at_n_run != 0); ++i) {

		n_1_cfg[i] = n_1_cfg_arr[i];
	}
}


static inline void ut_check_turn_on_off_n_1_mode(unsigned int run_times)
{
	unsigned int i = 0, iidx = -1;
	int sensor_id = -1;


	for (i = 0;
		((i < SENSOR_MAX_NUM) &&
		((n_1_cfg[i].turn_on_at_n_run != 0)
		&& (n_1_cfg[i].turn_off_at_n_run != 0)));
		++i) {

		sensor_id = ut_get_sid_by_sensor_idx(
				n_1_cfg[i].sensor_idx, &iidx);

		if (sensor_id < 0) {
			printf(RED
				"ERROR: get sensor_id failed, i:%u, sensor_idx:%u\n"
				NONE,
				i, n_1_cfg[i].sensor_idx
			);

			continue;
		}


#if defined(REDUCE_UT_DEBUG_PRINTF)
		printf(GREEN
			"[UT check_turn_on_off_n_1_mode] run_times:%u, i:%u[sidx:%u/ON:%u/OFF:%u], ID:%#x/iidx:%u\n"
			NONE,
			run_times, i,
			n_1_cfg[i].sensor_idx,
			n_1_cfg[i].turn_on_at_n_run,
			n_1_cfg[i].turn_off_at_n_run,
			sensor_id,
			iidx
		);
#endif


		switch (REGISTER_METHOD) {
		case BY_SENSOR_ID:
			if (n_1_cfg[i].turn_on_at_n_run == run_times) {
				frameSync->fs_n_1_en(sensor_id,
						n_1_cfg[i].n, 1);


				g_n_1_status[iidx] = 1; // 1 for ON
				g_n_1_min_fl_us[iidx] = n_1_cfg[i].on_min_fl_us;
				g_n_1_f_cell_size[iidx] = n_1_cfg[i].n;

			} else if (n_1_cfg[i].turn_off_at_n_run == run_times) {
				frameSync->fs_n_1_en(sensor_id,
						n_1_cfg[i].n, 0);


				g_n_1_status[iidx] = 2; // 2 for OFF
				g_n_1_min_fl_us[iidx] = n_1_cfg[i].off_min_fl_us;
				g_n_1_f_cell_size[iidx] = 0;
			}

			break;

		case BY_SENSOR_IDX:
		default:
			if (n_1_cfg[i].turn_on_at_n_run == run_times) {
				frameSync->fs_n_1_en(n_1_cfg[i].sensor_idx,
						n_1_cfg[i].n, 1);


				g_n_1_status[iidx] = 1; // 1 for ON
				g_n_1_min_fl_us[iidx] = n_1_cfg[i].on_min_fl_us;
				g_n_1_f_cell_size[iidx] = n_1_cfg[i].n;

			} else if (n_1_cfg[i].turn_off_at_n_run == run_times) {
				frameSync->fs_n_1_en(n_1_cfg[i].sensor_idx,
						n_1_cfg[i].n, 0);


				g_n_1_status[iidx] = 2; // 2 for OFF
				g_n_1_min_fl_us[iidx] = n_1_cfg[i].off_min_fl_us;
				g_n_1_f_cell_size[iidx] = 0;
			}

			break;
		}
	}
}


/******************************************************************************/
// return:
//     1: continue
//    -1: break
//     0: do nothing
/******************************************************************************/
static int ut_set_fs_set_shutter_select_sensor_manually(int *select)
{
	unsigned int input = 0;


	/* select a sensor which you will set it in bellow flow */
	/* print sensor which you can select */
	printf(GREEN
		"Please select bellow sensors for \"setting shutter (perframe_st)\"\n"
		NONE);

	/* using CLI for choise */
	*select = print_and_select_sensor(streaming_sensors);

	/* all fs_shutter() has been set, call fs_sync_frame(0) */
	if (*select < 0) {
		printf(LIGHT_PURPLE
			"FrameSync per-frame ctrl process, framesync sync frame(0)...\n\n\n"
			NONE);

		frameSync->fs_sync_frame(0);

		printf("\n\n");



		/* check if you want to go next run */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) Next run? (Yes:1 / No:0) : "
			NONE);
		scanf("%d", &input);

		/* need not to go for next run, so break the for loop */
		if (input == 0)
			return -1;


		/* generate vsync data */
		printf(LIGHT_PURPLE
			">>> (Input 1 integer) generate vsync data \"pf auto:1\" OR \"manually:0\" : "
			NONE);
		scanf("%d", &input);

		if (input > 0) {
			ut_generate_vsync_data_pf_auto(
				&v_rec);
		} else {
			v_rec = ut_generate_vsync_data_manually(
				streaming_sensors,
				streaming_sensors_modes_list);
		}

		printf(GREEN
			">>> FrameSync per-frame ctrl process, framesync sync frame(1)...\n\n\n"
			NONE);

		frameSync->fs_sync_frame(1);

		printf("\n\n");

		return 1;
	}

	printf("\n\n");
	return 0;
}


static void ut_set_fs_set_shutter(void)
{
	int select = 2147483647, input = 0;
	unsigned int i = 0;
	unsigned int hdr_mode = 0, ae_exp_cnt = 0, exp_i = 0;


	for (i = 0; ; ++i) {
		if (g_auto_run && streaming_sensors[i].sensor == NULL)
			break;


		struct fs_perframe_st pf_ctrl = {0};


		/* 1. set sensor mode */
		/*    => setup some perframe_st data */
		if (!g_auto_run) {
			unsigned int ret = 0;

			ret = ut_set_fs_set_shutter_select_sensor_manually(
				&select);

			if (ret == 1)
				continue;
			else if (ret == -1)
				break;


			/* print sensor mode which you can select */
			printf(GREEN
				"Please select a sensor mode bellow :\n"
				NONE);

			/* using CLI for choise */
			input = print_and_select_s_mode(
					streaming_sensors_modes_list,
					select);

			set_pf_ctrl_s_mode(&pf_ctrl, select, input);
		} else {
			/* auto run will use mode data have been prepared */
			set_pf_ctrl_s_mode(&pf_ctrl, i, sensor_mode[i]);
		}


		/* 2. set anti-flicker */
		if (!g_auto_run) {
			printf(LIGHT_PURPLE
				">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"flicker_en\" : "
				NONE,
				select, pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
			scanf("%d", &input);

			pf_ctrl.flicker_en = input;
		} else {
			/* TODO: let the behavior of auto set anti flicker like ISP7 */
			ut_auto_set_anti_flicker(&pf_ctrl);
		}


		/* 3. set min framelength */
		if (!g_auto_run) {
			printf(LIGHT_PURPLE
				">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"min_fl (us)\" : "
				NONE,
				select, pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
			scanf("%d", &input);

			pf_ctrl.min_fl_lc =
				US_TO_LC(input, pf_ctrl.lineTimeInNs);
		} else {
			// hardcode min framelength to 33350 us
			if ((g_n_1_status[i] == 1) || (g_n_1_status[i] == 2)) {
				pf_ctrl.min_fl_lc =	US_TO_LC(
					g_n_1_min_fl_us[i], pf_ctrl.lineTimeInNs);
			} else {
				pf_ctrl.min_fl_lc = US_TO_LC(
					33350, pf_ctrl.lineTimeInNs);
			}
		}


		/* 4. set shutter time (us) */
		if (!g_auto_run) {
			printf(LIGHT_PURPLE
				">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"shutter (us)\" : "
				NONE,
				select, pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
			scanf("%d", &input);

			pf_ctrl.shutter_lc =
				US_TO_LC(input, pf_ctrl.lineTimeInNs);
		} else {
			pf_ctrl.shutter_lc =
				US_TO_LC(g_shutter, pf_ctrl.lineTimeInNs);
		}


		/* 5. set hdr shutter time (us) */
		if (!g_auto_run) {
			hdr_mode = streaming_sensors_modes_list[select]
					.mode_list->hdr_exp.mode_exp_cnt;

			printf(GREEN
				">>> HDR:exp[] => LE:[0] / ME:[1] / SE:[2] / SSE:[3] / SSSE:[4], mode_exp_cnt:%u\n"
				NONE,
				hdr_mode);

			printf(LIGHT_PURPLE
				">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"AE exp cnt\" : "
				NONE,
				select, pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
			scanf("%u", &ae_exp_cnt);
		} else {
			hdr_mode = streaming_sensors_modes_list[i]
					.mode_list->hdr_exp.mode_exp_cnt;

			ae_exp_cnt = streaming_sensors_modes_list[i]
					.mode_list->hdr_exp.ae_exp_cnt;
		}

		for (exp_i = 0; exp_i < ae_exp_cnt; ++exp_i) {
			int hdr_idx = 0;

			hdr_idx = hdr_exp_idx_map[ae_exp_cnt][exp_i];

			if (!g_auto_run) {
				printf(LIGHT_PURPLE
					">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"HDR exp[%u] (us)\" : "
					NONE,
					select, pf_ctrl.sensor_id,
					pf_ctrl.sensor_idx, exp_i);
				scanf("%d", &input);

				pf_ctrl.hdr_exp.exp_lc[hdr_idx] =
					US_TO_LC(input, pf_ctrl.lineTimeInNs);
			} else {
				pf_ctrl.hdr_exp.exp_lc[hdr_idx] =
					US_TO_LC(g_hdr_shutter[hdr_idx],
						pf_ctrl.lineTimeInNs);

				/* TODO: fix hdr_exp hardcode variable */
				pf_ctrl.hdr_exp.readout_len_lc = 2374*2;
				pf_ctrl.hdr_exp.read_margin_lc = 10*2;
			}
			// printf("hdr_idx:%u, exp:%u\n", hdr_idx, pf_ctrl.hdr_exp.exp_lc[hdr_idx]);
		}


#if defined(USING_PRIVATE_CTRL_ORDER)
		/* N-2. call fs_update_auto_flicker_mode() */
		ut_set_fs_update_auto_flicker_mode(&pf_ctrl);


		/* N-1. call fs_update_min_framelength_lc() */
		ut_set_fs_update_min_framelength_lc(&pf_ctrl);
#endif


#if defined(REDUCE_UT_DEBUG_PRINTF)
		printf(GREEN
			"[UT set_fs_set_shutter] i:%u, sensor_id:%#x, sensor_idx:%u\n"
			NONE,
			i, pf_ctrl.sensor_id, pf_ctrl.sensor_idx
		);
#endif


		/* TODO: for N:N/Mstream:Mstream case, need a correct method */
		if ((g_n_1_status[i] == 1)
			&& (g_counter >= (n_1_cfg[i].turn_on_at_n_run + 2))
			&& (g_counter < (n_1_cfg[i].turn_off_at_n_run + 2))) {
			/* N:1 */
			input = 0;
			while (++input < g_n_1_f_cell_size[i]) {
				struct fs_perframe_st pf_ctrl_n_1 = pf_ctrl;
				unsigned int exp_us =
					US_TO_LC(5000, pf_ctrl.lineTimeInNs);


				if (pf_ctrl_n_1.shutter_lc > exp_us * input)
					pf_ctrl_n_1.shutter_lc -= exp_us * input;
				else
					pf_ctrl_n_1.shutter_lc = exp_us;

				frameSync->fs_set_shutter(&pf_ctrl_n_1);

				ut_gen_timestamps_data(i, 1, &v_rec);

				frm_debug_set_last_vsync_data(&v_rec);
			}
		}


		/* N. call fs_set_shutter() */
		if (!g_auto_run)
			printf("\n\n");

		frameSync->fs_set_shutter(&pf_ctrl);

		if (!g_auto_run)
			printf("\n\n");


#if !defined(USING_PRIVATE_CTRL_ORDER)
		/* N+1. call fs_update_auto_flicker_mode() */
		ut_set_fs_update_auto_flicker_mode(&pf_ctrl);


		/* N+2. call fs_update_min_framelength_lc() */
		ut_set_fs_update_min_framelength_lc(&pf_ctrl);
#endif
	}
}


static void ut_trigger_ext_ctrl(unsigned int run_times)
{
	unsigned int i = 0;

	for (i = 0;
		(ext_ctrls[i].do_ext_fl_at_n_run != 0 &&
		ext_ctrls[i].do_seamless_switch_at_n_run != 0); ++i) {

		// printf("ext_ctrls.ext_at:%u/seamless_at:%u\n",
		//	ext_ctrls[i].do_ext_fl_at_n_run,
		//	ext_ctrls[i].do_seamless_switch_at_n_run);

		switch (REGISTER_METHOD) {
		case BY_SENSOR_IDX:
		default:
			if (ext_ctrls[i].do_ext_fl_at_n_run == run_times) {
				frameSync->fs_set_extend_framelength(
						ext_ctrls[i].sensor_idx,
						0,
						ext_ctrls[i].ext_fl_us);
			}


			if (ext_ctrls[i].do_seamless_switch_at_n_run == run_times) {
				frameSync->fs_seamless_switch(
						ext_ctrls[i].sensor_idx);
			}

			break;
		}
	}
}


static void ut_trigger_pf_ctrl_manually(void)
{
	/* for trigger frame sync PF CTRL */
	printf(GREEN
		">>> FrameSync per-frame ctrl process, framesync sync frame(1)...\n\n\n"
		NONE);

	frameSync->fs_sync_frame(1);

	printf("\n\n\n");

	/* ut call fs_set_shutter() */
	ut_set_fs_set_shutter();
}


static void ut_trigger_pf_ctrl_auto_run_normal(void)
{
	unsigned int i = 0;

	bool first_sync = false, break_sync = false, re_sync = false;
	bool pf_result = false;

	int c = 0;
	unsigned int break_sync_count = 0, re_sync_count = 0;

	bool is_ut_pass = false;


	if (g_fs_alg_stability_test_flag)
		goto RUN_PF_CTRL_AUTO_NORMAL;


	/* set auto run times */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) auto run times (suggest min:10000) : "
		NONE);
	scanf("%u", &g_run_times);

	while (g_run_times < 0) {
		printf(RED
			">>> g_run_times is a negative value, please input it again !\n"
			NONE);

		/* set auto run times */
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) auto run times (suggest min:10000) : "
			NONE);
		scanf("%u", &g_run_times);
	}
	// printf("\n\n\n");


	/* set vdiff sync success threshold */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) \"set sync helper TH\" (determin sync successfully or not, def:1000) : "
		NONE);
	scanf("%u", &vdiff_sync_success_th);
	vdiff_sync_success_th = (vdiff_sync_success_th > 0)
		? vdiff_sync_success_th
		: VDIFF_SYNC_SUCCESS_TH;


	/* set simulation for passed more than one vsync */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) simulation passed vsync(s) (Y:1 / N:0): "
		NONE);
	scanf("%u", &simulation_passed_vsyncs);
	if (simulation_passed_vsyncs) {
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) passed vsync(s) ratio (0-100) : "
			NONE);
		scanf("%u", &passed_vsyncs_ratio);
		if (passed_vsyncs_ratio > 100)
			passed_vsyncs_ratio = 100;


		printf(LIGHT_PURPLE
			">>> (Input 1 integers) max passed vsync(s) cnt : "
			NONE);
		scanf("%u", &max_pass_cnt);
		if (max_pass_cnt < 1)
			max_pass_cnt = 1;
	}


	/* if you want to lock exp / flk when auto run test */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) lock shutter value (Y:1 / N:0): "
		NONE);
	scanf("%u", &lock_exp);

	if (lock_exp) {
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) lock shutter at idx (0:10002 / 1:19997 / 2:29996 / 3:40005 / 4:50004 / 5:60002) : "
			NONE);
		scanf("%u", &exp_table_idx);

		if (exp_table_idx >= EXP_TABLE_SIZE) {
			exp_table_idx = EXP_TABLE_SIZE - 1;

			printf(LIGHT_PURPLE
				">>> set non valid value, lock shutter at idx:%u automatically\n"
				NONE,
				exp_table_idx);
		}
	}

	printf(LIGHT_PURPLE
		">>> (Input 1 integers) lock anti-flicker (Y:1 / N:0): "
		NONE);
	scanf("%u", &lock_flk);


	printf("\n\n\n");


	for (i = 0; streaming_sensors[i].sensor != NULL; ++i) {
		printf(GREEN
			"Please select bellow sensor's MODE for auto run using :\n"
			NONE);

		sensor_mode[i] = print_and_select_s_mode(
					streaming_sensors_modes_list,
					i);

		printf("\n\n\n");
	}


RUN_PF_CTRL_AUTO_NORMAL:

	/* auto run pf ctrl test */
	g_counter = 0;

	while (++g_counter < g_run_times) {
		/* 0. generate shutter for AE exp sync */
		generate_shutter_value();


		/* 1. start => same request ID settings will be set */
		frameSync->fs_sync_frame(1);

		/* 1.x turn on/off N:1 mode or not */
		ut_check_turn_on_off_n_1_mode(g_counter);


		/* 2. ut call fs_set_shutter() */
		ut_set_fs_set_shutter();


		/* 2.x do ext ctrl if needed */
		ut_trigger_ext_ctrl(g_counter);


		/* 3. end => end this request ID settings */
		frameSync->fs_sync_frame(0);


		/* 4. generate vsync data */
		ut_generate_vsync_data_pf_auto(&v_rec);


		/* 5. check sync result */
		pf_result = ut_check_pf_sync_result(&v_rec);


		if (g_counter >= 20 && !first_sync) {
			printf(RED
				"UT: Can NOT sync, pf_count:%u, first_sync:%s\n"
				NONE,
				g_counter,
				(first_sync) ? "true" : "false");

			printf(RED ">>> Press \'n\' key to continue... "NONE);
			while (((c = getchar()) != 'n') && (c != EOF))
				;
			printf("\n");
		}

		if ((g_counter >= (g_broke_at_counter+10)) && break_sync && first_sync) {
			printf(RED
				"UT: Can NOT sync from broke sync, pf_count:%u, break_sync:%s, broke at:%u\n"
				NONE,
				g_counter,
				(first_sync) ? "true" : "false",
				g_broke_at_counter);

			printf(RED ">>> Press \'n\' key to continue... "NONE);
			while (((c = getchar()) != 'n') && (c != EOF))
				;
			printf("\n");
		}

		/* first sync (from non-sync to sync) */
		if (pf_result && !first_sync) {
			first_sync = true; // if first_sync is true, never be false
			printf(GREEN
				"UT: First Sync, pf_count:%u\n"
				NONE, g_counter);
		}

		/* sync is broken */
		if (!pf_result && first_sync && !break_sync) {
			break_sync = true;
			break_sync_count++;
			g_broke_at_counter = g_counter;
			printf(RED
				"UT: Sync Broken, pf_count:%u, times:%u\n"
				NONE,
				g_counter, break_sync_count);

			printf(RED ">>> Press \'n\' key to continue... "NONE);
			while (((c = getchar()) != 'n') && (c != EOF))
				;
			printf("\n");
		}

		/* re-sync (from broken sync to sync) */
		if (pf_result && break_sync) {
			break_sync = false;
			re_sync = true;
			re_sync_count++;
			g_broke_at_counter = 0;
			printf(GREEN
				"UT: Re-Sync, pf_count:%u, times:%u\n"
				NONE,
				g_counter, re_sync_count);
		}
	}


	/* print auto test result */
	is_ut_pass = (first_sync && !break_sync) ? true : false;
	printf(
		(is_ut_pass) ?
		GREEN
		">>> UT Result: PASS (Run %u times, Broken %u times ! first_sync:%s, break_sync:%s, re_sync:%s)\n"
		NONE :
		RED
		">>> UT Result: FAIL (Run %u times, Broken %u times ! first_sync:%s, break_sync:%s, re_sync:%s)\n"
		NONE,
		g_counter, break_sync_count,
		(first_sync) ? "true" : "false",
		(break_sync) ? "true" : "false",
		(re_sync) ? "true" : "false");
}


static void ut_trigger_pf_ctrl_auto_run(void)
{
	/* for shutter automatically generated */
	srand(time(NULL));

	// TODO: add auto test case for choice
	ut_trigger_pf_ctrl_auto_run_normal();
}


static void ut_trigger_pf_ctrl(void)
{
	printf(LIGHT_CYAN
		"\n\n\n>>> UT Trigger FrameSync PF CTRL <<<\n\n\n"
		NONE);


	/* N:1 mode cfg */
	ut_setup_n_1_cfg(n_1_cfg_2_1_60_main_0);


	/* per-frame ctrl with auto run / manually set */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) PF CTRL with \"Auto run:1\" OR \"Manually set:0\" : "
		NONE);
	scanf("%u", &g_auto_run);

	if (g_auto_run > 0)
		ut_trigger_pf_ctrl_auto_run();
	else {
		printf("\n\n\n");
		ut_trigger_pf_ctrl_manually();
	}


	/* END */
	printf(GREEN
		"\n\n\n>>> END UT Trigger FrameSync PF CTRL <<<\n\n\n"
		NONE);
}


static void test_frame_sync_proc(void)
{
	unsigned int ret = 0;


	/* frame sync drv init, get it's address */
	ret = FrameSyncInit(&frameSync);
	printf(GREEN "\n\n\n>>> Test FrameSync Process! <<<\n" NONE);
	printf(GREEN ">>> frameSync addr = %p\n\n\n" NONE, frameSync);
	if (ret != 0) {
		printf(RED ">>> frameSync Init failed !\n" NONE);
		return;
	}


	/* 0. reset ut test variable before using */
	reset_ut_test_variables();
	ut_select_frame_sync_algorithm();


	/* 1. fs_streaming() */
	ut_set_fs_streaming();


	/* 2. fs_set_sync() */
	ut_set_fs_set_sync();


	/* 3. set initial vsync diff manually */
	printf(GREEN
		">>> Before running PF CTRL test, please set vsync data for first run\n"
		NONE);
	printf(GREEN
		">>> => set initial vsync diff\n\n\n"
		NONE);

	v_rec = ut_generate_vsync_data_manually(
		streaming_sensors, streaming_sensors_modes_list);


	/* 4. trigger FrameSync PF CTRL */
	ut_trigger_pf_ctrl();


	printf(GREEN "\n\n\n>>> END Test FrameSync Process! <<<\n" NONE);
}


static void setup_ut_streaming_data(
	unsigned int i,
	struct ut_fs_test_sensor_cfg *p_sensor_cfg)
{
	streaming_sensors[i].sensor_name =
		p_sensor_cfg[i].sensor_name;

	streaming_sensors[i].sensor_idx =
		p_sensor_cfg[i].sensor_idx;

	streaming_sensors[i].tg =
		p_sensor_cfg[i].tg;

	streaming_sensors[i].sensor = p_sensor_cfg[i].sensor;


	streaming_sensors_modes_list[i].sensor_name =
		p_sensor_cfg[i].sensor_name;

	streaming_sensors_modes_list[i].sensor_idx =
		p_sensor_cfg[i].sensor_idx;

	streaming_sensors_modes_list[i].mode_list =
		p_sensor_cfg[i].mode;

	streaming_sensors_modes_list[i]
		.mode_list[p_sensor_cfg[i].mode_idx]
		.sensor_idx = p_sensor_cfg[i].sensor_idx;


	sensor_mode[i] = p_sensor_cfg[i].mode_idx;
}


static void ut_setup_ext_ctrl_cfg(
	struct ut_fs_test_ext_ctrl_cfg *ctrls)
{
	unsigned int i = 0;

	if (ctrls == NULL) {
		struct ut_fs_test_ext_ctrl_cfg clear_st = {0};

		for (i = 0; i < SENSOR_MAX_NUM; ++i)
			ext_ctrls[i] = clear_st;

		return;
	}

	for (i = 0;
		(ctrls[i].do_ext_fl_at_n_run != 0 &&
		ctrls[i].do_seamless_switch_at_n_run != 0); ++i) {

		ext_ctrls[i] = ctrls[i];
	}
}


static void setup_fs_alg_stability_test_env_cfg(unsigned int test_id)
{
	g_run_times = test_list[test_id].env_cfg->run_times;
	vdiff_sync_success_th = test_list[test_id].env_cfg->sync_th;
	simulation_passed_vsyncs = test_list[test_id].env_cfg->passed_vsync;
	passed_vsyncs_ratio = test_list[test_id].env_cfg->passed_vsync_ratio;
	max_pass_cnt = test_list[test_id].env_cfg->passed_vsync_max_cnt;
	lock_exp = test_list[test_id].env_cfg->lock_exp;
	// lock_exp = 1;
	exp_table_idx = test_list[test_id].env_cfg->lock_exp_table_idx;
	lock_flk = test_list[test_id].env_cfg->lock_flk;

	/* EXT CTRL */
	ut_setup_ext_ctrl_cfg(test_list[test_id].env_cfg->ext_ctrls);

	/* N:1 mode cfg */
	ut_setup_n_1_cfg(test_list[test_id].n_1_cfg);
}


static void exe_fs_alg_stability_test_item(unsigned int test_id)
{
	unsigned int i = 0;
	struct ut_fs_test_sensor_cfg *p_sensor_cfg = NULL;
	unsigned int biggest_vts = 0;

	pthread_t thread[SENSOR_MAX_NUM];

	printf(GREEN
		"\n\n\n>>> Execute FS alg stability Test, Test_ID:%u!\n\n\n<<<\n"
		NONE,
		test_id);


	g_sync_tag = test_list[test_id].sync_tag;
	g_alg_method = test_list[test_id].alg_method;

	query_tg_cnt = 0;

	/* 1. call fs_streaming() with fs_set_sync(), by multi-thread */
	p_sensor_cfg = test_list[test_id].sensor_cfg;

	for (i = 0; p_sensor_cfg[i].sensor != NULL; ++i) {
		pthread_create(&thread[i], NULL,
			ut_set_fs_streaming_and_synced,
			(void *)&p_sensor_cfg[i]);


		setup_ut_streaming_data(i, p_sensor_cfg);


		/* 1.2 setup initial ut timestamp data */
		/* TODO : non-hardcode */
		query_tg_cnt++;

		v_rec.recs[i].id =
			streaming_sensors[i].tg;

		v_rec.recs[i].vsyncs = 1;

		v_rec.recs[i].timestamps[0] = p_sensor_cfg[i].first_vts_value;
		ut_vts[i].timestamp[0] = p_sensor_cfg[i].first_vts_value;

		if (p_sensor_cfg[i].first_vts_value > biggest_vts)
			biggest_vts = p_sensor_cfg[i].first_vts_value;
	}

	for (i = 0; p_sensor_cfg[i].sensor != NULL; ++i)
		pthread_join(thread[i], NULL);


	/* 1.3 setup initial ut timestamp data */
	v_rec.ids = query_tg_cnt;
	v_rec.tick_factor = TICK_FACTOR;
	v_rec.cur_tick = (biggest_vts + 1) * v_rec.tick_factor;


	// printf("[0] tg:%u, [1] tg:%u, [2] tg:%u\n",
	// v_rec.recs[0].id,
	// v_rec.recs[1].id,
	// v_rec.recs[2].id);


	/* 2. check if in stability test mode */
	if (g_fs_alg_stability_test_flag) {
		/* 2.1 copy fs test env cfg settings */
		setup_fs_alg_stability_test_env_cfg(test_id);


		/* 2.2 call frm API set vsync data */
		frm_debug_set_last_vsync_data(&v_rec);


		/* 2.3 pf ctrl auto run */
		ut_trigger_pf_ctrl_auto_run();
	}


	printf(GREEN
		"\n\n\n>>> END Execute FS alg stability Test, Test_ID:%u! <<<\n\n\n"
		NONE,
		test_id);

	printf(LIGHT_CYAN
		"\n\n\n>>> Test PASS : [%2u] %s <<<\n\n\n\n\n\n"
		NONE,
		(test_id + 1),
		test_list[test_id].test_name);

	for (i = 0; i < 4; ++i) {
		printf(GREEN
			"waiting for running next test case... %u(4)\n"
			NONE, i);

		sleep(5);
	}

	printf("\n\n\n");
}


static void exe_fs_alg_stability_test(void)
{
	unsigned int i = 0;
	int select = 1;

	g_fs_alg_stability_test_flag = 1;
	g_auto_run = 1;


	ut_select_frame_sync_algorithm();


	while (true) {
		printf(GREEN
				"\n\n\n>>> Please choose FrameSync algorithm stability test case bellow! <<<\n"
				NONE);
		printf(GREEN
			"[ 0] Run all case (all must run case, i.e per-frame ctrl case, except EXT CTRL)\n"
			NONE);

		for (i = 0; test_list[i].sensor_cfg != NULL; ++i) {
			printf(GREEN "[%2u] %s\n" NONE,
				i + 1,
				test_list[i].test_name);
		}
		printf(GREEN "[-1] End FrameSync algorithm stability test\n" NONE);

		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"select a case\" : "
			NONE);
		scanf("%d", &select);

		if (select <= 0)
			break;


		reset_ut_test_variables();

		exe_fs_alg_stability_test_item(select-1);
	}

	if (select < 0) {
		printf("\n\n\n");

		g_fs_alg_stability_test_flag = 0;
		g_auto_run = 0;

		return;
	}


	for (i = 0; test_list[i].sensor_cfg != NULL; ++i) {
		if (test_list[i].exe_all_skip_ext_ctrl_test == 1)
			continue;

		if (test_list[i].auto_test_must_run != 1)
			continue;

		reset_ut_test_variables();

		exe_fs_alg_stability_test_item(i);
	}

	printf("\n\n\n");


	g_fs_alg_stability_test_flag = 0;
	g_auto_run = 0;
}


int main(void)
{
	unsigned int select_ut_case = 0, terminated = 0;


	while (!terminated) {
		SHOW_TEXT();

		printf(GREEN
			"\n\n\n>>> Please choose FrameSync UT Test case bellow! <<<\n"
			NONE);
		printf(GREEN
			">>> Run : [1] FrameSync algorithm stability test\n"
			NONE);
		printf(GREEN
			">>> Run : [2] FrameSync processing test\n"
			NONE);
		printf(GREEN
			">>> Run : [3] FrameSync control flow test\n"
			NONE);
		printf(GREEN
			">>> Run : [4] FrameSync data racing test\n"
			NONE);
		printf(GREEN
			">>> Run : [X] End UT test\n"
			NONE);

		printf(LIGHT_PURPLE
			">>> (Input 1 integer) \"select a case id in []\" : "
			NONE);
		scanf("%u", &select_ut_case);


		switch (select_ut_case) {
		case 1:
			/* run fs algorithm stability test */
			exe_fs_alg_stability_test();
			break;

		case 2:
			/* run test fs processing */
			test_frame_sync_proc();
			break;

		case 3:
			/* run fs control test */
			run_fs_control_test();
			break;

		case 4:
			/* run fs data racing test */
			run_fs_data_racing_test();
			break;

		default:
			terminated = 1;
			break;
		}
	}


	return 0;
}
