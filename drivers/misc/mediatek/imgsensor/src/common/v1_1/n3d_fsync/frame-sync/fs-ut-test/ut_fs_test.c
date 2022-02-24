// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "ut_fs_platform_arg.h"
#include "ut_fs_perframe_ctrl_info.h"
#include "../frame_monitor.h"


/* CMD printf color */
#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"


// #define MANUALLY_SET_CURRENT_TIMESTAMP


static struct FrameSync *frameSync;


/******************************************************************************/
struct UT_Timestamp {
	unsigned int idx;
	unsigned int timestamp[VSYNCS_MAX];
};


static struct UT_Timestamp ut_vts[SENSOR_MAX_NUM];


static unsigned int set_synced_sensors[SENSOR_MAX_NUM] = {0};

static struct ut_fs_streaming_sensor_list
			streaming_sensors[SENSOR_MAX_NUM] = {0};

static struct ut_fs_perframe_sensor_mode_list
			streaming_sensors_modes_list[SENSOR_MAX_NUM] = {0};


#define VDIFF_SYNC_SUCCESS_TH 1000
static unsigned int vdiff_sync_success_th;

static struct vsync_rec v_rec = {0};
/******************************************************************************/

/* AE shutter value */
#define EXP_TABLE_SIZE 6
static unsigned int exp_table_idx;
static unsigned int lock_exp_table_idx;
static const unsigned int exp_table[EXP_TABLE_SIZE] = {
	10002,
	19997,
	29996,
	40005,
	50004,
	60002,
};
static unsigned int lock_exp;
static unsigned int lock_flk;


/* flicker enable table */
static const unsigned int flk_en_table[10] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static unsigned int flk_en_table_idx;


static unsigned int simulation_passed_vsyncs;
static unsigned int passed_vsyncs_ratio;
static unsigned int max_pass_cnt;
/******************************************************************************/





/******************************************************************************/
// Test FrameSync Driver behavior
/******************************************************************************/
static void test_fs_register_sensor_behavior(void)
{
	/* Test fs_register_sensor -> fs_push_sensor */
	printf("\n");
	printf(
		"/*************************************************************/\n"
		);
	printf("/* --- Test fs_register_sensor function behavior. --- */\n");


	switch (REGISTER_METHOD) {
	case BY_SENSOR_ID:
	{
		printf("/* [--- REGISTER_BY_SENSOR_ID ---] */\n\n");

		struct fs_streaming_st push_sensor[5];
		struct fs_streaming_st clear_st = {0};


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
	}
		break;

	case BY_SENSOR_IDX:
	{
		printf("/* [--- REGISTER_BY_SENSOR_IDX ---] */\n\n");

		struct fs_streaming_st ut_test = {
			.sensor_id = 0x999,
		};
		struct fs_streaming_st push_sensor[2] = {ut_test, ut_test};
		struct fs_streaming_st clear_st = {0};


		printf(">>>>>>>>>>>>>>>>>>>> Test Run 1 <<<<<<<<<<<<<<<<<<<<\n");
		printf(
			"[--- Already register 3 sensors, register idx 1 and 3... ---]\n\n"
			);

		for (unsigned int i = 0; i < 2; ++i) {
			printf("\n=== Test register:%2d ===\n", i);
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
/******************************************************************************/


void *processing(void *ut_fs_argument)
{
	struct ut_fs_argument *arg = (struct ut_fs_argument *)ut_fs_argument;

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
		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);
		if (ret == 0)
			frameSync->fs_set_sync(arg->sensor->sensor_idx, 1);

		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);

		break;

	default:
		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);
		if (ret == 0)
			frameSync->fs_set_sync(arg->sensor->sensor_idx, 1);

		ret = frameSync->fs_is_set_sync(arg->sensor->sensor_idx);

		break;
	}


	pthread_exit(NULL);
}


void test_sync_2_sensors(void)
{
	// TODO: run times should not be hardcode.
	for (unsigned int i = 0; i < 4; ++i) {
		struct vsync_rec rec = {0};

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


static void get_ut_timestamps_pf(
	unsigned int idx, unsigned int vsyncs,
	struct vsync_rec *(p_v_rec))
{
	unsigned int i = 0, choose = 0;
	unsigned int fl_us[2] = {0}, sensor_curr_fl_us = 0;
	unsigned int ref_idx = 0, ref_idx_next = 0;


	frm_get_predicted_fl_us(idx, fl_us, &sensor_curr_fl_us);


	// for debug
	// printf("UT get predicted fl( 0:%u, 1:%u )\n", fl_us[0], fl_us[1]);


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


	// for debug
	// printf("UT dump timestamp(%u/%u/%u/%u)\n",
	//	ut_vts[idx].timestamp[0],
	//	ut_vts[idx].timestamp[1],
	//	ut_vts[idx].timestamp[2],
	//	ut_vts[idx].timestamp[3]);


	ref_idx = ut_vts[idx].idx;
	p_v_rec->recs[idx].vsyncs = vsyncs;
	// printf("UT vsyncs:%u\n", vsyncs);
	for (i = 0; i < VSYNCS_MAX; ++i) {
		p_v_rec->recs[idx].timestamps[i] =
				ut_vts[idx].timestamp[ref_idx];

		ref_idx = (ref_idx + (VSYNCS_MAX - 1)) % VSYNCS_MAX;
	}
}


static void set_pf_ctrl_s_mode(
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



static unsigned int get_shutter_value(void)
{
	/* 0 => idx-- ; 1 => idx++ ; 2 => idx+0 */
	unsigned int exp_table_idx_dir = 0;

	if (lock_exp)
		return exp_table[lock_exp_table_idx];


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


	return exp_table[exp_table_idx];
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
			"[%d] ID:%#x (sidx:%u), margin_lc:%3u, lineTimeInNs:%7u, pclk:%10u, linelength:%6u\n"
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
					.mode_list[i].linelength);
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


static bool ut_check_pf_sync_result(struct vsync_rec *(p_v_rec))
{
	unsigned int i = 0, biggest_vts = 0;
	unsigned int vdiff[SENSOR_MAX_NUM] = {0};
	bool is_success = true;


	/* 1. find out lastest vsync timestamp */
	for (i = 0; i < TG_MAX_NUM; ++i)
		if (p_v_rec->recs[i].timestamps[0] > biggest_vts)
			biggest_vts = p_v_rec->recs[i].timestamps[0];


	/* 2. check each vsync timestamp diff and */
	/*    check if sync successfully */
	for (i = 0; i < TG_MAX_NUM; ++i) {
		if (p_v_rec->recs[i].timestamps[0] == 0)
			continue;

		vdiff[i] = biggest_vts - p_v_rec->recs[i].timestamps[0];

	// for debug
	//	printf(RED ">>> UT: [%u] vdiff:%u, biggest_vts:%u, vts:%u\n\n\n" NONE,
	//			i, vdiff[i], biggest_vts, p_v_rec->recs[i].timestamps[0]);

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
				">>> UT: [%u] tg:%u, vsyncs:%u, vdiff:%u, vts:%u\n"
				NONE,
				i,
				p_v_rec->recs[i].id,
				p_v_rec->recs[i].vsyncs,
				vdiff[i],
				p_v_rec->recs[i].timestamps[0]);
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


	/* for workaround, only set last vsync timestamp */
	for (i = 0; i < TG_MAX_NUM; ++i) {

	// for workaround, only set last vsync timestamp
	//	p_v_rec->recs[i].vsyncs = 1;
	//	p_v_rec->recs[i].timestamps[0] += frm_get_predicted_curr_fl_us(i);

		get_ut_timestamps_pf(i, passed_vsyncs, p_v_rec);

		if (p_v_rec->recs[i].timestamps[0] > biggest_vts)
			biggest_vts = p_v_rec->recs[i].timestamps[0];
	}

	p_v_rec->cur_tick = (biggest_vts + 100) * p_v_rec->tick_factor;


	printf("\n\n\n");
	printf(GREEN
		">>> frame monitor set vsync data (pf auto) ...\n\n\n"
		NONE);
	printf(GREEN
		">>> call frame monitor, frm_debug_set_last_vsync_data()...\n\n\n"
		NONE);

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
		// v_rec.ids++; // seems not set is fine.
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

			if (input > biggest_vts)
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

#endif

	printf("[0] tg:%u, [1] tg:%u, [2] tg:%u\n",
		v_rec.recs[0].id,
		v_rec.recs[1].id,
		v_rec.recs[2].id);
	printf(GREEN
		">>> frame monitor set vsync data, call frm_debug_set_last_vsync_data()...\n\n\n"
		NONE);

	frm_debug_set_last_vsync_data(&v_rec);


	printf("\n\n\n");

	return v_rec;
}


static void ut_set_fs_streaming(void)
{
	int select = 2147483647, input;

	unsigned int idx = 0, tg = 0;
	unsigned int i = 0, j = 0;


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


static void ut_set_fs_set_sync(void)
{
	int select = 2147483647, input;

	unsigned int i = 0;


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


static void ut_trigger_pf_ctrl_manually(void)
{
	int select = 2147483647, input;

	unsigned int i = 0;


	/* for trigger frame sync PF CTRL */
	printf(GREEN
		">>> FrameSync per-frame ctrl process, framesync sync frame(1)...\n\n\n"
		NONE);

	frameSync->fs_sync_frame(1);

	printf("\n\n\n");


	for (i = 0; ; ++i) {
		struct fs_perframe_st pf_ctrl = {0};

		/* 1. select a sensor which you will set it in bellow flow */
		/* print sensor which you can select */
		printf(GREEN
			"Please select bellow sensors for \"setting shutter (perframe_st)\"\n"
			NONE);

		/* using CLI for choise */
		select = print_and_select_sensor(streaming_sensors);

		/* all fs_shutter() has been set, call fs_sync_frame(0) */
		if (select < 0) {
			printf(LIGHT_PURPLE
				"FrameSync per-frame ctrl process, framesync sync frame(0)...\n\n\n"
				NONE);

			frameSync->fs_sync_frame(0);

			printf("\n\n\n");



			/* check if you want to go next run */
			printf(LIGHT_PURPLE
				">>> (Input 1 integer) Next run? (Yes:1 / No:0) : "
				NONE);
			scanf("%d", &input);

			/* need not to go for next run, so break the for loop */
			if (input <= 0)
				break;


			/* 6. generate vsync data */
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

			printf("\n\n\n");

			continue;
		}

		printf("\n\n\n");


		/* 2. select the sensor's mode */
		/* print sensor mode which you can select */
		printf(GREEN
			"Please select a sensor mode bellow :\n"
			NONE);

		/* using CLI for choise */
		input = print_and_select_s_mode(
				streaming_sensors_modes_list,
				select);

		set_pf_ctrl_s_mode(&pf_ctrl, select, input);


		/* 3. set anti-flicker or not */
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"flicker_en\" : "
			NONE,
			select,	pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
		scanf("%d", &input);

		pf_ctrl.flicker_en = input;


		// hardcode min framelength to 33350 us
		// printf(LIGHT_PURPLE
		//	">>> (Input one integers) [%d] ID:%#x (sidx:%u), set \"min_fl us\" : "
		//	NONE,
		//	select,	pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
		// scanf("%d", &input);

		// pf_ctrl.min_fl_lc = US_TO_LC(input, pf_ctrl.lineTimeInNs);


		/* 4. set shutter time (us) */
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) [%d] ID:%#x (sidx:%u), set \"shutter (us)\" : "
			NONE,
			select,	pf_ctrl.sensor_id, pf_ctrl.sensor_idx);
		scanf("%d", &input);

		pf_ctrl.shutter_lc = US_TO_LC(input, pf_ctrl.lineTimeInNs);


		/* 5. call fs_set_shutter() */
		printf(GREEN
			">>> framesync set shutter, call fs_set_shutter()...\n\n"
			NONE);

		frameSync->fs_set_shutter(&pf_ctrl);


		/* 6. call fs_update_auto_flicker_mode() */
		switch (REGISTER_METHOD) {
		case BY_SENSOR_ID:
			frameSync->fs_update_auto_flicker_mode(
					pf_ctrl.sensor_id,
					pf_ctrl.flicker_en);

			break;

		case BY_SENSOR_IDX:
			frameSync->fs_update_auto_flicker_mode(
					pf_ctrl.sensor_idx,
					pf_ctrl.flicker_en);

			break;

		default:
			frameSync->fs_update_auto_flicker_mode(
					pf_ctrl.sensor_idx,
					pf_ctrl.flicker_en);
			break;
		}


		/* 7. call fs_update_min_framelength_lc() */
		switch (REGISTER_METHOD) {
		case BY_SENSOR_ID:
			frameSync->fs_update_min_framelength_lc(
					pf_ctrl.sensor_id,
					pf_ctrl.min_fl_lc);

			break;

		case BY_SENSOR_IDX:
			frameSync->fs_update_min_framelength_lc(
					pf_ctrl.sensor_idx,
					pf_ctrl.min_fl_lc);

			break;

		default:
			frameSync->fs_update_min_framelength_lc(
					pf_ctrl.sensor_idx,
					pf_ctrl.min_fl_lc);

			break;
		}


		printf("\n\n\n");
	}
}


static void ut_trigger_pf_ctrl_auto_run_normal(void)
{
	unsigned int i = 0, counter = 0, run_times = 0;
	unsigned int s_mode[SENSOR_MAX_NUM] = {0};

	bool first_sync = false, break_sync = false, re_sync = false;
	bool pf_result = false;

	unsigned int break_sync_count = 0, re_sync_count = 0;

	bool is_ut_pass = false;


	/* set auto run times */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) auto run times (suggest min:10000) : "
		NONE);
	scanf("%u", &run_times);

	while (run_times < 0) {
		printf(RED
			">>> run_times is a negative value, please input it again !\n"
			NONE);

		/* set auto run times */
		printf(LIGHT_PURPLE
			">>> (Input 1 integers) auto run times (suggest min:10000) : "
			NONE);
		scanf("%u", &run_times);
	}
	// printf("\n\n\n");


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
		scanf("%u", &lock_exp_table_idx);

		if (lock_exp_table_idx >= EXP_TABLE_SIZE) {
			lock_exp_table_idx = EXP_TABLE_SIZE - 1;

			printf(LIGHT_PURPLE
				">>> set non valid value, lock shutter at idx:%u automatically\n"
				NONE,
				lock_exp_table_idx);
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

		s_mode[i] = print_and_select_s_mode(
					streaming_sensors_modes_list,
					i);

		printf("\n\n\n");
	}


	/* auto run pf ctrl test */
	while (counter++ < run_times) {
		unsigned int shutter = 0;

		/* 0. get shutter for AE exp sync */
		shutter = get_shutter_value();


		/* 1. start => same request ID settings will be set */
		frameSync->fs_sync_frame(1);


		/* 2. call fs_set_shutter() all sensor */
		for (i = 0; streaming_sensors[i].sensor != NULL; ++i) {
			struct fs_perframe_st pf_ctrl = {0};

			/* 2-1. set sensor mode */
			set_pf_ctrl_s_mode(&pf_ctrl, i, s_mode[i]);


			/* 2-2. set anti-flicker or not */
			// TODO:
			//    flicker enable/disable automatically for auto test
			//    current case is disable for all.
			if ((pf_ctrl.sensor_idx == 0) && (!lock_flk)) {
				// only main cam
				pf_ctrl.flicker_en =
					flk_en_table[flk_en_table_idx];

				flk_en_table_idx = (flk_en_table_idx + 1) % 10;
			} else
				pf_ctrl.flicker_en = 0;


			// hardcode min framelength to 33350 us
			// pf_ctrl.min_fl_lc = US_TO_LC(input, pf_ctrl.lineTimeInNs);


			/* 2-3. set shutter time (us) */
			pf_ctrl.shutter_lc =
					US_TO_LC(shutter, pf_ctrl.lineTimeInNs);


			/* 2-4. call fs_set_shutter() */
			frameSync->fs_set_shutter(&pf_ctrl);


			/* 2-5. call fs_update_auto_flicker_mode() */
			switch (REGISTER_METHOD) {
			case BY_SENSOR_ID:
				frameSync->fs_update_auto_flicker_mode(
						pf_ctrl.sensor_id,
						pf_ctrl.flicker_en);

				break;

			case BY_SENSOR_IDX:
				frameSync->fs_update_auto_flicker_mode(
						pf_ctrl.sensor_idx,
						pf_ctrl.flicker_en);

				break;

			default:
				frameSync->fs_update_auto_flicker_mode(
						pf_ctrl.sensor_idx,
						pf_ctrl.flicker_en);

				break;
			}


			/* 2-6. call fs_update_min_framelength_lc() */
			switch (REGISTER_METHOD) {
			case BY_SENSOR_ID:
				frameSync->fs_update_min_framelength_lc(
						pf_ctrl.sensor_id,
						pf_ctrl.min_fl_lc);

				break;

			case BY_SENSOR_IDX:
				frameSync->fs_update_min_framelength_lc(
						pf_ctrl.sensor_idx,
						pf_ctrl.min_fl_lc);

				break;

			default:
				frameSync->fs_update_min_framelength_lc(
						pf_ctrl.sensor_idx,
						pf_ctrl.min_fl_lc);

				break;
			}
		}


		/* 3. end => end this request ID settings */
		frameSync->fs_sync_frame(0);


		/* 4. generate vsync data */
		ut_generate_vsync_data_pf_auto(&v_rec);


		/* 5. check sync result */
		pf_result = ut_check_pf_sync_result(&v_rec);

		/* first sync (from non-sync to sync) */
		if (pf_result && !first_sync) {
			first_sync = true; // if first_sync is true, never be false
			printf(GREEN
				"UT: First Sync, pf_count:%u\n"
				NONE, counter);
		}

		/* sync is broken */
		if (!pf_result && first_sync && !break_sync) {
			break_sync = true;
			break_sync_count++;
			printf(RED
				"UT: Sync Broken, pf_count:%u, times:%u\n"
				NONE,
				counter, break_sync_count);

			printf(RED ">>> Press any key to continue... "NONE);
			getchar();
			printf("\n");
		}

		/* re-sync (from broken sync to sync) */
		if (pf_result && break_sync) {
			break_sync = false;
			re_sync = true;
			re_sync_count++;
			printf(GREEN
				"UT: Re-Sync, pf_count:%u, times:%u\n"
				NONE,
				counter, re_sync_count);
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
		counter-1, break_sync_count,
		(first_sync) ? "true" : "false",
		(break_sync) ? "true" : "false",
		(re_sync) ? "true" : "false");
}


static void ut_trigger_pf_ctrl_auto_run(void)
{
	unsigned int input = 0;

	printf(LIGHT_PURPLE
		">>> (Input 1 integers) \"set sync helper TH\" (determin sync successfully or not, def:1000) : "
		NONE);
	scanf("%u", &input);
	vdiff_sync_success_th = (input > 0) ? input : VDIFF_SYNC_SUCCESS_TH;


	/* for shutter automatically generated */
	srand(time(NULL));

	// TODO: add auto test case for choice
	ut_trigger_pf_ctrl_auto_run_normal();
}


static void ut_trigger_pf_ctrl(void)
{
	unsigned int input = 0;


	printf(LIGHT_CYAN
		"\n\n\n>>> UT Trigger FrameSync PF CTRL <<<\n\n\n"
		NONE);


	/* per-frame ctrl with auto run / manually set */
	printf(LIGHT_PURPLE
		">>> (Input 1 integers) PF CTRL with \"Auto run:1\" OR \"Manually set:0\" : "
		NONE);
	scanf("%u", &input);

	if (input > 0)
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


int main(void)
{
	printf(LIGHT_CYAN ">===================================<\n" NONE);
	printf(LIGHT_CYAN "!!!   FrameSync UT Test Program   !!!\n" NONE);
	printf(LIGHT_CYAN ">===================================<\n" NONE);
	printf("\n\n\n");


	/* run test processing */
	test_frame_sync_proc();


	return 0;
}
