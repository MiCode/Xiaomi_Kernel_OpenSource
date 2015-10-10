/*
 * Atmel maXTouch Touchscreen driver Plug in
 *
 * Copyright (C) 2013 Atmel Co.Ltd
 * Author: Pitter Liao <pitter.liao@atmel.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/****************************************************************
	Pitter Liao add for macro for the global platform
	email:  pitter.liao@atmel.com
	mobile: 13244776877
-----------------------------------------------------------------*/
#define PLUG_CAL_T37_VERSION 0x0106
/*----------------------------------------------------------------
0.106
1 check max grad only at reset
2 fixed bug in grad cache min value renew
3 step 2 check failed only try 1 time after resume
4 fixed bugs of T8 set in each step

0.105
1 set grad cache instead grad block cache
2 modify iron reference check
0.104
1 set movement check for calibration
2 change T100 dymatic thold for T8 calibration
3 set soft T8
4 support proximity calibration
5 support T65 disable

0.103
1 proximity support
2 move zero check out of LOW
0.102
1 add x,y grad accumulator auto select
2 add movement check in pending
0.101
1 add mxt_surface_delta_high_check()
2 improve function check_touched_and_moving()
3 improve step2 single touch
4 improve step3 for stable delta check
0.100
1 add auto suspend
0.99
1 change gradient algorithm
2 support bad pointt with 2 algorithm
3 step wait state machine changed

0.97
1 add iron/high point check
2 change interval of each step, return if interval is not reached
0.96
1 add watcher in step 2 which only depend t8 before
2 add drift in step 3 which will detect weak signal
3 change MAX_TRACE_POINTS to 10
4 delete high limit in grad cache renew
5 add x line ref drop check
0.95
1 t80 support
0.94
1 t38 protect support
0.93
1 xline check change
2 supper big check change
3 signal check strange is different with ref check

0.92
1 fix the report rate dropping issue

0.91
1 fixed bugs at signal check

0.90
1 max value for 9 area
2 noise/very noise increase min and max value
3 supper big grident check
4 signal gradient check (include key)
5 t65 firmware bug check
6 t100/delta firmware bug recovery

0.84
1 add noise compensation for grad_block_low_limit
0.83
1 change back the moving algorithm
0.82
1 support t100
2 fixed some bugs at matrix check
3 increase single touch check
4 change movement algorithm
0.81
1 fixed bug at resume, the step 2 will wait first touch release
0.8
1 fixed step 3 start time issue
0.7
1 fiexed issue in step2 check ref 2
2 add T37_FLAG_TOUCH_ALL_RELEASED flag
3 clear step_cmd when calibration/resume/reset
0.6
1 fixed bugs at step2 check grad max
0.5:
1 fixed bugs at t100 hook
2 fixed bugs at step 4 status
3 fiexed bugs at show()
0.4:
1 fixed bugs(chedck single touch) in step 2
0.3
1 gradient reference check add at step 2
2 touch points trace at t9_hook
0.2
1 modify palm weak condition, make a step weaker
0.11
1 version for simple workaround without debug message
0.1
1 first version of t37 plugin
*/
#include "plug.h"

enum{
	//delta start
	T37_DELTA0 = 0,
	T37_DELTA1,
	T37_DELTA_S,
	//ref start
	T37_REF0,
	T37_SIGNAL,
	T37_TRANS,
	T37_GRAD0_X,
	T37_GRAD1_X,
	T37_GRAD0_Y,
	T37_GRAD1_Y,
	T37_GRAD0_K,
	T37_GRAD1_K,
	T37_IRON,
	T37_TEMP,
	T37_BUFFER_NUMBER,
	T37_BUFFER_INVALID = T37_BUFFER_NUMBER
};

static char *t37_sf_name[T37_BUFFER_NUMBER] = {
	"DELTA0",
	"DELTA1",
	"DELTA_S",
	"REF0",
	"SIGNAL",
	"GRAND_TRANS",
	"GRAD0_X",
	"GRAD1_X",
	"GRAD0_Y",
	"GRAD1_Y",
	"GRAD0_K",
	"GRAD1_K",
	"IRON",
	"TEMP",
};

#define T37_PAD_NUMBER 4

//RAW
//pad0:   TBD(avg)
//pad1:   TBD(max)
//pad2:   TBD(min)
//pad3:   RAW_MAX / RAW_MIN / GRAD_MAX/ GRAD_MIN / MODULES
enum{
	T37_PAD3_RAW_MAX,
	T37_PAD3_RAW_MIN,
	T37_PAD3_RAW_NUMBER
};

//GRAD:
//pad0:   TBD(avg)
//pad1:
//pad2:   horizontal direction avg value(after transpose)
enum{
	T37_PAD2_AVG_H_LINE,
};
//pad3:   GRAD_MAX/ GRAD_MIN / MODULES
enum{
	T37_PAD3_GRAD_MIN = 0,//nine block
	T37_PAD3_GRAD_MAX = 10,
	T37_PAD3_GRAD_MODULES = 20,
	T37_PAD3_GRAD_NUMBER
};
#define T37_PAD3_GRAD_BLOCKS (T37_PAD3_GRAD_MAX - T37_PAD3_GRAD_MIN - 1)

//DIGITAL
//pad0:   TBD
//pad1:   TBD
//pad2:   TBD
//pad3:   TBD
enum{
	T37_PAD3_DIGITAL_HYSTERISTER,
	T37_PAD3_DIGITAL_THRESHOLD,
	T37_PAD3_DIGITAL_INTERGAL_THESHOLD,
	T37_PAD3_DIGITAL_NUMBER
};

//invlid high point table
enum{
	GRAD_INVALID_3 = 0,
	GRAD_INVALID_5,
	GRAD_INVALID_8,
	GRAD_INVALID_CNT_NUM,
};

// step 0: calibrating ~ calibration end
#define T37_STEP_0_ENTER			(1<<0)
#define T37_STEP_0_IN					0xf

// step 1: calibration end ~ touch
#define T37_STEP_1_ENTER			(1<<4)
#define T37_STEP_1_IN					0xf0

// step 2: single touch ~ single touch release
#define T37_STEP_2_ENTER			(1<<8)
#define T37_STEP_2_CHECK_REF_1			(1<<9)
#define T37_STEP_2_CHECK_REF_2			(1<<10)//do calibration at touch release
#define T37_STEP_2_CHECK_SINGLE_TOUCH		(1<<11)//touch unlock
#define T37_STEP_2_IN					0xf00

// step 3: single touch release ~ 5s
#define T37_STEP_3_ENTER			(1<<12)
#define T37_STEP_3_CHECK_DELTA			(1<<13)
#define T37_STEP_3_CHECK_DELTA_PENDING		(1<<14)
#define T37_STEP_3_CHECK_SIGNAL_FIRST		(1<<15)
#define T37_STEP_3_CHECK_SIGNAL_MIDDLE		(1<<16)
#define T37_STEP_3_AUTO_SLEEP			(1<<17)
#define T37_STEP_3_IN					0xff000

// step 4 stop workaround
#define T37_STEP_4_ENTER			(1<<20)
#define T37_STEP_4_IN					0xf00000

#define T37_STEP_END				(1<<31)

#define T37_STEP_MASK					(-1)

//flag
#define T37_FLAG_RESUME				(1<<0)
#define T37_FLAG_RESET				(1<<1)
#define T37_FLAG_CAL				(1<<2)

#define T37_FLAG_TOUCH_RELEASED			(1<<8)
#define T37_FLAG_TOUCH_ALL_RELEASED		(1<<9)
#define T37_FLAG_TOUCH_MOVING			(1<<10)
#define T37_FLAG_TOUCH_PRESSED			(1<<11)
#define T37_FLAG_TOUCH_MOV_PENDING		(1<<12)

#define T37_FLAG_SUPPRESSED			(1<<15)

#define T37_FLAG_PHONE_ON			(1<<16)

#define T37_FLAG_SURFACE_GRADIENT_SMOOTH	(1<<30)
#define T37_FLAG_WORKAROUND_HALT		(1<<31)

#define T37_FLAG_TAG_MASK_LOW				0x0000ff
#define T37_FLAG_TAG_MASK_NORMAL			0x00ff00
#define T37_FLAG_TAG_MASK_MIDDLE			0xff0000
#define T37_FLAG_MASK					(-1)

//state
#define T37_STATE_TOUCH_RELEASED		(1<<0)
#define T37_STATE_TOUCH_MOV_PENDING		(1<<1)
#define T37_STATE_TOUCH_MOVING			(1<<2)
#define T37_STATE_TOUCH_PRESSED			(1<<3)

#define T37_STATE_MASK					(-1)

struct point{
	int x;
	int y;
};

struct surface_threshold {
	s16 diff_thld_t9_pos;
	s16 diff_thld_t15_pos;
	s16 diff_thld_t9_neg;
	s16 diff_thld_t15_neg;
	s16 percent_thld;
	s16 percent_hys;
};

struct surface_diff_result {
	int correct;
	int uncorrect;
	int misable;
	int misable_edge;
	int strength_sf;
	int strength_cal;
	int strength_cal_edge;
};

struct surface_count{
	int count;
	int good;
	int bad;
	int pend;
	int sleep;
	int retry;
};

enum{
	GRAD_TRACE_ALL,
	GRAD_TRACE_TOUCH_RELEASE,
	GRAD_TRACE_SINGLE_TOUCH,
	GRAD_TRACE_NUMBER
};

enum{
	GRAD_STATE_WAIT = 0,
	GRAD_STATE_NOT,
	GRAD_STATE_NUMBER
};

struct point_trace{
	unsigned long  flag;
	int count;//clear after checked
	struct point first;
	struct point last;
	struct point distance;
	unsigned long time_touch_st;//touch start time
	unsigned long time_touch_end;//touch end time
	unsigned long time_point_renw;
};

enum{
	GRAD_DRIFT_SLOW = 0,
	GRAD_DRIFT_NORM,
	GRAD_DRIFT_FAST,
	GRAD_DRIFT_NUMBER
};

struct grad_cache_type{
	s16 * x_axis;
	s16 * y_axis;
};

#define GRAD_DRIFT_ITEMS 6

struct t37_observer{
	void *obs_buf;

	s16 *buf[T37_BUFFER_NUMBER];
	struct point sf_info_list[T37_BUFFER_NUMBER];

	unsigned long  step;
	unsigned long  step_cmd;
	unsigned long  flag;
#define GRAD_STATE_LIST 2
	unsigned long  step2_wait_state[GRAD_STATE_LIST][GRAD_STATE_NUMBER];

	struct surface_count sf_cnt[T37_BUFFER_NUMBER];

	unsigned long time_touch_st;//touch start time
	unsigned long time_touch_end;//touch end time
	unsigned long time_delta_check_st;//delta checked time
	unsigned long time_delta_check_st_2;//delta checked time after first touch
	unsigned long time_gradient_check_st;
	unsigned long time_next_st;
	unsigned long time_phone_off;

#define MAX_TRACE_POINTS 10
	struct point_trace trace[MAX_TRACE_POINTS];//assume touch number less than 10
	unsigned long touch_id_list;

	struct surface_count gradient[GRAD_TRACE_NUMBER];
	struct grad_cache_type grad_cache[T72_NOISE_STATE_TYPES];//this is abs value both min and max, each time resume/calibration will renew this
						//last one negative mean grad changable
						//3 cache for noise, 2 group for x/y axis
	int accumulator_ref;
	int accumulator_ref_min;
};

enum{
	MOVEMENT_SHORT = 0,
	MOVEMENT_MIDDLE,
	MOVEMENT_LONG,
	MOVEMENT_HUGE_LONG,
	MOVEMENT_NUM
};

struct t37_config{

	unsigned long interval_delta_check;
	unsigned long interval_delta_middle_check;
	unsigned long interval_delta_check_end;
	unsigned long interval_delta_check_end2;
	unsigned long interval_grad_check;
	unsigned long interval_distance_check;
	unsigned long interval_step_0;
	unsigned long interval_step_1;
	unsigned long interval_step_2;
	unsigned long interval_step_3;
	unsigned long interval_step_4;
	unsigned long interval_normal;
	unsigned long interval_slow;
	unsigned long interval_fast;
	unsigned long interval_zero;

	unsigned long interval_step2_pressed_short;
	unsigned long interval_step2_pressed_long;

	int step1_single_count;
	int step2_single_count;
	int step2_grad_ref_check1_count;
	int step2_grad_ref_check2_count;
	int step2_single_touch_check_count;
	int step2_check_point_count;
	int step3_sleep_count;

	int distance_touch_move[MOVEMENT_NUM];
	struct point movement[MOVEMENT_NUM];

#define T37_COMPARE_DIRECTION_NEG	(1<<0)
#define T37_COMPARE_DIRECTION_POS	(1<<1)
#define T37_COMPARE_DIRECTION_BOTH		(T37_COMPARE_DIRECTION_NEG|T37_COMPARE_DIRECTION_POS)

#define T37_REF_MIN	0x4000
#define T37_REF_MAX	0x7fff

#define T37_DELTA_MIN		(-0x1000)
#define T37_DELTA_MAX		0x1000
#define T37_SKIP_LIMIT_COMPARE		(1<<2)
	int surface_retry_times;

	int delta_tiny_limit_ratio;
	int delta_low_limit_ratio;
	int delta_mid_limit_ratio;
	int delta_high_limit_ratio;
	int delta_highest_limit_ratio;
	int delta_stable_limit_ratio;

	int delta_good_times;
	int delta_bad_times;
	int delta_repeat_times_shift;

	int node_large_area;

	int grad_good_times;
	int grad_bad_times;
	int grad_pend_times;

	int grad_accumulator_low;//signal lower than this will do calibration
	int grad_accumulator_high;//ref higher than this will do single touch check
	int ref_signal_ratio_shift;//signal mutilpy this shit, if less than ref , then cal

	s16 grad_default_cache[T72_NOISE_STATE_TYPES][T37_PAD3_GRAD_BLOCKS];

//bugs area
	int xline_check_percent;
	int reference_drop_percent;
	int soft_t8_internal_thld_percent;
	unsigned long delta_line_check_flag;
	int delta_line_check_skip;
	int delta_line_high_count;

	struct rect edge;//edge position
#define NUM_GRAD_LEVEL_LIST 6
	struct stat_grad_config (*grad_level_array)[NUM_GRAD_LEVEL_LIST];
	int num_grad_level_array;
	int grad_block_low_limit;//8bit mode
	int grad_block_high_limit;//8bit mode
	int grad_block_high_limit_list[T37_PAD3_GRAD_BLOCKS]; //normal mode
	int grad_block_highest_limit_ratio;
	s16 grad_t_modules;
	s16 grad_k_modules;
#define ACCUMULATOR_GRAD_X		(1<<3)
#define ACCUMULATOR_GRAD_Y		(1<<4)
#define ACCUMULATOR_GRAD_K		(1<<5)
#define ACCUMULATOR_GRAD_DUALX		(1<<6)
#define ACCUMULATOR_GRAD_DIFF		(1<<7)
#define ACCUMULATOR_GRAD_COMP		(1<<8)
#define ACCUMULATOR_GRAD_INVALUE	(1<<9)
#define ACCUMULATOR_GRAD_CACULATE	(1<<10)

	unsigned long grad_acc_flag;

#define ACCUMULATOR_GRAD_DRIFT_FAST	(1<<12)
#define ACCUMULATOR_GRAD_DRIFT_NORM	(1<<13)
#define ACCUMULATOR_GRAD_DRIFT_MASK	0xf000
	int (*grad_drift_coef_array)[GRAD_DRIFT_ITEMS];

#define BUGS_CHECK_T65_BURST		(1<<16)
#define BUGS_CHECK_ZERO_BURST		(1<<17)
#define BUGS_CHECK_SOFT_T8		(1<<18)
#define BUGS_CHECK_T100_REPORT		(1<<19)
#define BUGS_CHECK_REFERENCE_DROP	(1<<20)
#define BUGS_CHECK_DELTA_HIGH		(1<<21)
#define BUGS_CHECK_REFERENCE_IRON	(1<<22)

	int *grad_invalid_count_array;

#define WK_KEEP_T80_ALWAYS_ON		(1<<26)
#define WK_KEEP_T65_ALWAYS_ON		(1<<27)
#define WK_KEEP_SPEED_AT_PRESSED	(1<<28)

};

#define caculate_percent(t_val,t_ratio) ((s16)((s32)(t_val) * (t_ratio) / 100))
#define caculate_threshlod(t_val,t_ratio) ((s16)((s32)(t_val) * (t_ratio) / 25 * 2))

static int mxt_process_obj_recovery(struct plugin_cal *p,unsigned long pl_flag);
static void gradient_cache_reset(struct plugin_cal *p);

long step_interval(struct plugin_cal *p, unsigned long step)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_config *cfg = p->cfg;
	unsigned long interval;

	if (test_flag(T37_STEP_0_IN, &step))
		interval = cfg->interval_step_0;
	else if (test_flag(T37_STEP_1_IN, &step)) {
		interval = cfg->interval_step_1;
	} else if (test_flag(T37_STEP_2_IN, &step)) {
		interval = cfg->interval_step_2;
		if (test_flag(T37_STEP_2_CHECK_REF_2 | T37_STEP_2_CHECK_SINGLE_TOUCH, &step)) {
			interval = cfg->interval_zero;
		}
	} else if (test_flag(T37_STEP_3_IN, &step)) {
		interval = cfg->interval_step_3;
		if (test_flag(T37_STEP_3_AUTO_SLEEP,&step))
			interval = cfg->interval_zero;
	} else if (test_flag(T37_STEP_4_IN, &step))
		interval = cfg->interval_step_4;
	else
		interval = MAX_SCHEDULE_TIMEOUT;

	dev_dbg2(dev, "mxt interval %lu\n",interval);
	return (long)interval;
}

void clear_grad_record(struct t37_observer *obs, int type, bool reset)
{
	int i,start,end;

	if (type < GRAD_TRACE_NUMBER) {
		start = end = type;
	} else {
		start = 0;
		end = GRAD_TRACE_NUMBER - 1;
	}

	for (i = start; i <= end; i++) {
		obs->gradient[i].bad = 0;
		obs->gradient[i].good = 0;
		if (reset)
			obs->gradient[i].count = 0;
	}
}

static void proc_cal_msg(struct plugin_cal *p)
{
	struct t37_observer *obs = p->obs;

	memset(obs->sf_info_list, 0 ,sizeof(obs->sf_info_list));
	memset(obs->sf_cnt , 0 ,sizeof(obs->sf_cnt));
	memset(obs->step2_wait_state,0,sizeof(obs->step2_wait_state));
	memset(&obs->trace, 0, sizeof(obs->trace));
	obs->accumulator_ref = 0;
	obs->time_next_st = jiffies;
}

static void proc_reset_msg(struct plugin_cal *p)
{
	struct t37_observer *obs = p->obs;

	gradient_cache_reset(p);

	memset(obs->gradient,0,sizeof(obs->gradient));
	//memset(obs->state,0,sizeof(obs->state));
	clear_flag(T37_STEP_MASK, &obs->step_cmd);
	obs->accumulator_ref = obs->accumulator_ref_min = 0;
}

static void proc_resume_msg(struct plugin_cal *p)
{
	struct t37_observer *obs = p->obs;

	clear_flag(T37_STEP_MASK, &obs->step_cmd);
	clear_grad_record(obs, GRAD_TRACE_NUMBER, false);
}

int get_t8_state(unsigned long step, unsigned long flag, unsigned long pl_flag, int step_t8, int num_t8_config)
{
	int state = T8_WEAK_PALM;

	if (test_flag(T37_STEP_0_IN,&step)) {
		state = T8_NORMAL;
	} else if (test_flag(T37_STEP_1_IN, &step)) {
		if (test_flag(T37_FLAG_PHONE_ON,&flag))
		state = T8_NORMAL;
		else {
			if (test_flag(PL_STATUS_FLAG_T8_SET,&pl_flag))
				state = T8_NORMAL;
			else
				state = T8_MIDDLE;
		}
	} else if (test_flag(T37_STEP_2_IN, &step)) {
		if (test_flag(T37_STEP_2_CHECK_REF_2|T37_STEP_2_CHECK_SINGLE_TOUCH, &step)) {
			if (test_flag(PL_STATUS_FLAG_NOISE,&pl_flag))
				state = T8_NOISE;
			else if (test_flag(PL_STATUS_FLAG_VERY_NOISE,&pl_flag))
				state = T8_VERY_NOISE;
			else
				state = T8_NORMAL;
		}
	} else if (test_flag(T37_STEP_3_IN, &step)) {
		if (step_t8 < 0) {
			if (test_flag(T37_FLAG_PHONE_ON,&flag))
				state = T8_NORMAL;
			else
				state = T8_MIDDLE;
		} else {
			if (test_flag(T37_FLAG_PHONE_ON,&flag))
				state = T8_NORMAL;
			else {
				if (test_flag(T37_STEP_3_CHECK_SIGNAL_MIDDLE, &step))
					state = T8_HALT;
				else {
					if (num_t8_config > 0) {
						if (state + step_t8 < num_t8_config) {
							state += step_t8;
						} else {
							state = T8_HALT;
						}
					} else {
						//this is forbid
					}
				}
			}
		}
	} else {
		state = T8_HALT;
	}

	return state;
}

int get_t9_t100_state(unsigned long step, unsigned long flag, unsigned long pl_flag)
{
	int state = T9_T100_NORMAL;

	if (test_flag(T37_FLAG_PHONE_ON,&flag))
		state = T9_T100_SINGLE_TOUCH;
	else {
		if (test_flag(T37_STEP_0_IN,&step)) {
		}else if (test_flag(T37_STEP_1_IN, &step)) {
			if (test_flag(PL_STATUS_FLAG_NOISE,&pl_flag))
				state = T9_T100_THLD_NOISE_STEP1;
			else if (test_flag(PL_STATUS_FLAG_VERY_NOISE,&pl_flag))
				state = T9_T100_THLD_VERY_NOISE_STEP1;
			else
				state = T9_T100_NORMAL_STEP1;
		}else {
			if (test_flag(PL_STATUS_FLAG_NOISE,&pl_flag))
				state = T9_T100_THLD_NOISE;
			else if (test_flag(PL_STATUS_FLAG_VERY_NOISE,&pl_flag))
				state = T9_T100_THLD_VERY_NOISE;
			else
				state = T9_T100_NORMAL;
		}
	}

	return state;
}

int get_t55_state(unsigned long step, unsigned long flag, unsigned long pl_flag)
{
	int state = T55_DISABLE;

	if (test_flag(T37_STEP_0_IN,&step)) {
	} else if (test_flag(T37_STEP_1_IN, &step)) {
	} else {
		if (test_flag(PL_STATUS_FLAG_NOISE,&pl_flag))
			state = T55_DISABLE;
		else if (test_flag(PL_STATUS_FLAG_VERY_NOISE,&pl_flag))
			state = T55_DISABLE;
		else
			state = T55_NORMAL;
	}

	return state;
}

int get_t65_state(unsigned long step, unsigned long flag, unsigned long pl_flag)
{
	int state = T65_ZERO_GRADTHR;

	if (test_flag(T37_STEP_0_IN,&step)) {
	} else if (test_flag(T37_STEP_1_IN, &step)) {
	} else if (test_flag(T37_STEP_2_IN, &step)) {
	} else if (test_flag(T37_STEP_3_IN, &step)) {
		if (test_flag(PL_STATUS_FLAG_PHONE_ON,&pl_flag))
			state = T65_ZERO_GRADTHR;
		else {
		  if (test_flag(T37_STEP_3_CHECK_SIGNAL_FIRST|T37_STEP_3_CHECK_SIGNAL_MIDDLE, &step))
			state = T65_NORMAL;
		}
	} else {
		state = T65_NORMAL;
	}

	return state;
}

int get_t80_state(unsigned long step, unsigned long flag, unsigned long pl_flag)
{
	int state = T80_LOW_GAIN;

	if (test_flag(T37_STEP_0_IN,&step)) {
	} else if (test_flag(T37_STEP_1_IN, &step)) {
	} else if (test_flag(T37_STEP_2_IN, &step)) {
	} else if (test_flag(T37_STEP_3_IN, &step)) {
		if (test_flag(T37_STEP_3_CHECK_SIGNAL_FIRST|T37_STEP_3_CHECK_SIGNAL_MIDDLE, &step))
			state = T80_NORMAL;
	}else {
		state = T80_NORMAL;
	}

	return state;
}

int get_noise_cache_id(unsigned long pl_flag)
{
	int id;

	if (test_flag(PL_STATUS_FLAG_NOISE,&pl_flag))
		id = 1;
	else if (test_flag(PL_STATUS_FLAG_VERY_NOISE,&pl_flag))
		id = 2;
	else
		id = 0;
	return id;
}

unsigned long get_gradient_flag(unsigned long flag,unsigned long pl_flag)
{
	unsigned long o_flag = 0;

	if (!test_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH,&flag))
		o_flag |= ACCUMULATOR_GRAD_COMP;

	if (test_flag(PL_STATUS_FLAG_DUALX,&pl_flag))
		o_flag |= ACCUMULATOR_GRAD_DUALX;

	return o_flag;
}

static unsigned long check_touched_and_moving(struct point_trace *t, int num, struct point *m, int *ppoints, int *pcount_min, unsigned long *pinterval_min)
{
	int i;
	int points = 0, count_min = 0;
	unsigned long interval,interval_min = 0;
	unsigned long flag = 0;

	for (i = 0; i < num; i++) {
		if (t[i].count) {
			points++;
			if (t[i].count && (!count_min || count_min > t[i].count))
				count_min = t[i].count;

			if (test_flag(T37_FLAG_TOUCH_PRESSED,&t[i].flag))
				interval = jiffies;
			else
				interval = t[i].time_touch_end;
			interval = (unsigned long)((long)interval - (long)t[i].time_touch_st);
			if (interval > 0 && (!interval_min || interval_min > interval))
				interval_min = interval;

			if (test_flag(T37_FLAG_TOUCH_PRESSED,&t[i].flag)) {
				set_flag(T37_STATE_TOUCH_PRESSED, &flag);
			} else {
				set_flag(T37_STATE_TOUCH_RELEASED, &flag);
				t[i].count = 0;
			}

			if (test_flag(T37_FLAG_TOUCH_MOV_PENDING,&t[i].flag))
				set_flag(T37_STATE_TOUCH_MOV_PENDING, &flag);

			if (m) {
				if (t[i].distance.x >= m->x || t[i].distance.y >= m->y)
					set_flag(T37_STATE_TOUCH_MOVING, &flag);
				#if (DBG_LEVEL > 1)
				printk(KERN_INFO "[mxt] distance (%d, %d) m(%d,%d)\n",
					t[i].distance.x,t[i].distance.y,
					m->x,m->y);
				#endif
			}
		}
	}

	if (ppoints)
		*ppoints = points;

	if (pcount_min)
		*pcount_min = count_min;

	if (pinterval_min)
		*pinterval_min = interval_min;

	return flag;
}


#define FLAG_TRACE_POINT_FLAG		(1<<0)
#define FLAG_TRACE_POINT_COUNT		(1<<1)
#define FLAG_TRACE_POINT_FIRST		(1<<2)
#define FLAG_TRACE_POINT_LAST		(1<<3)
#define FLAG_TRACE_POINT_FIRST_LAST		(FLAG_TRACE_POINT_FIRST|FLAG_TRACE_POINT_LAST)
#define FLAG_TRACE_POINT_DISTANCE	(1<<4)
#define FLAG_TRACE_POINT_TOUCH_ST	(1<<5)
#define FLAG_TRACE_POINT_TOUCH_END	(1<<6)
#define FLAG_TRACE_POINT_RENEW		(1<<7)
#define FLAG_TRACE_POINT_TOUCH_ST_END		(FLAG_TRACE_POINT_TOUCH_ST|FLAG_TRACE_POINT_TOUCH_END)
#define FLAG_TRACE_POINT_MASK			(-1)

static void clear_trace_point(struct point_trace *t, int num, unsigned long flag)
{
	int i;

	for (i = 0; i < num; i++) {
		if (test_flag(FLAG_TRACE_POINT_FLAG, &flag))
			t[i].flag = 0;
		if (test_flag(FLAG_TRACE_POINT_COUNT, &flag))
			t[i].count= 0;
		if (test_flag(FLAG_TRACE_POINT_FIRST, &flag))
			memset(&t[i].first, 0, sizeof(t[i].first));
		if (test_flag(FLAG_TRACE_POINT_LAST, &flag))
			memset(&t[i].last, 0, sizeof(t[i].last));
		if (test_flag(FLAG_TRACE_POINT_DISTANCE, &flag))
			memset(&t[i].distance, 0, sizeof(t[i].distance));
		if (test_flag(FLAG_TRACE_POINT_TOUCH_ST, &flag))
			t[i].time_touch_st = 0;
		if (test_flag(FLAG_TRACE_POINT_TOUCH_END, &flag))
			t[i].time_touch_end = 0;
		if (test_flag(FLAG_TRACE_POINT_RENEW, &flag))
			t[i].time_point_renw = 0;
	}
}

#define P_AA		(1<<0)
#define P_PAD0		(1<<1)
#define P_PAD1		(1<<2)
#define P_PAD2		(1<<3)
#define P_PAD3		(1<<4)
void print_matrix_surface(int sf,s16 *buf,const struct rect *surface, unsigned long flag)
{
	int x_size,y_size;
	int pad0,pad1,pad2,pad3;

	char *prefix = NULL;

	if (!flag)
		flag = P_AA|P_PAD0|P_PAD3;

	x_size = surface->x1 - surface->x0 + 1;
	y_size = surface->y1 - surface->y0 + 1;

	pad0 = x_size * y_size;
	pad1 = pad0 + max(x_size,y_size);
	pad2 = pad1 + max(x_size,y_size);
	pad3 = pad2 + max(x_size,y_size);//user defined

	if (sf < T37_BUFFER_NUMBER) {
		if (test_flag(P_AA, &flag))
			print_matrix(t37_sf_name[sf], buf, x_size, y_size);
		else
			prefix = t37_sf_name[sf];
		if (test_flag(P_PAD0, &flag))
			print_matrix(prefix, buf + pad0, 1, max(x_size,y_size));
		if (test_flag(P_PAD1, &flag))
			print_matrix(prefix, buf + pad1, 1, max(x_size,y_size));
		if (test_flag(P_PAD2, &flag))
			print_matrix(prefix, buf + pad2, 1, max(x_size,y_size));
		if (test_flag(P_PAD3, &flag))
			print_matrix(prefix, buf + pad3, 1, max3(T37_PAD3_RAW_NUMBER,T37_PAD3_GRAD_NUMBER,T37_PAD3_DIGITAL_NUMBER));
	}
}
//src and dst can not overlap
static void transpose_matrix(s16 *dst_buf,s16 *src_buf,const struct rect *surface)
{
	int x_size,y_size;
	int i,j,pos0,pos1;

	x_size = surface->x1 - surface->x0 + 1;
	y_size = surface->y1 - surface->y0 + 1;

	for (i = 0; i < x_size; i++) {
		for (j = 0; j < y_size; j++) {
			pos0 = i * y_size + j;
			pos1 = j * x_size + i;
			dst_buf[pos1] = src_buf[pos0];
		}
	}
}

#define SURFACE_MERGE_AVG		(1<<0)
#define SURFACE_MERGE_AVG_HYSIS		(1<<1)
#define SURFACE_MERGE_AVG_HYSIS2	(1<<2)
#define SURFACE_MERGE_AVG_HYSIS3	(1<<3)
#define SURFACE_MERGE_ADD		(1<<4)
#define SURFACE_MERGE_COPY		(1<<5)
#define SURFACE_MERGE_SUB		(1<<6)
static int merge_matrix(s16 *dst_buf,s16 *src0_buf, s16 *src1_buf,int x_size,int y_size,int flag)
{
	int i,j,pos;
	s16 d0,d1;

	for (i = 0; i < x_size; i++) {
		for (j = 0; j < y_size; j++) {
			pos = i * y_size + j;
			if (flag & SURFACE_MERGE_AVG)
				dst_buf[pos] = (src0_buf[pos] + src1_buf[pos]) >> 1;
			else if (flag & SURFACE_MERGE_AVG_HYSIS)
				dst_buf[pos] = src0_buf[pos] / 3 + src1_buf[pos] * 2 / 3;
			else if (flag & SURFACE_MERGE_AVG_HYSIS2)
				dst_buf[pos] = (src0_buf[pos] >> 3) + (src1_buf[pos] >> 3) * 7;
			else if (flag & SURFACE_MERGE_AVG_HYSIS3) {
				if (abs(src0_buf[pos]) > abs(src1_buf[pos])) {
					d0 = src0_buf[pos];
					d1 = src1_buf[pos];
				} else {
					d0 = src1_buf[pos];
					d1 = src0_buf[pos];
				}
				dst_buf[pos] = (((d0 * 3)>>2) + (d1 >> 2));
			} else if (flag & SURFACE_MERGE_ADD)
				dst_buf[pos] = src0_buf[pos] + src1_buf[pos];
			else if (flag & SURFACE_MERGE_COPY)
				memcpy(dst_buf,src0_buf,
						x_size * y_size * sizeof(dst_buf[0]));
			else if (flag & SURFACE_MERGE_SUB)
				dst_buf[pos] = src0_buf[pos] - src1_buf[pos];
		}
	}

	return 0;
}

void module_coef(int *coef, int num, int numerator,int denominator)
{
	int i;
	for (i = 0; i < num; i++) {
		if (numerator >= denominator) {
			coef[i] = 0;
		} else {
			if (numerator) {
				coef[i] = coef[i] * (denominator - numerator);
				if (denominator)
					coef[i] /= denominator;
			}
		}
	}
}

int surface_check_and_fix_invalid_value(s16 *src_buf,const struct rect *area,const struct rect *surface,int x, int y, s16 low_limit, s16 high_limit, const int *invalid_count_array)
{
	struct rect adjcent;
	int y_size;
	int i,j,pos;
	s16 val,grad;

	int count,high_count,sum;
	bool ret = 0;

	y_size = surface->y1 - surface->y0 + 1;
	adjcent.x0 = adjcent.x1 = x;
	adjcent.y0 = adjcent.y1 = y;

	if (adjcent.x0 > area->x0)
		adjcent.x0--;
	if (adjcent.x1 < area->x1)
		adjcent.x1++;
	if (adjcent.y0 > area->y0)
		adjcent.y0--;
	if (adjcent.y1 < area->y1)
		adjcent.y1++;

	pos = x * y_size + y;
	val = src_buf[pos];
	sum = 0;
	count = high_count = 0;
	for (i = adjcent.x0; i <= adjcent.x1; i++) {
		for (j = adjcent.y0; j <= adjcent.y1; j++) {
			if (i == x && j == y)
				continue;
			pos = i * y_size + j;
			grad = src_buf[pos] - val;
			if (grad > high_limit || grad < -high_limit) {
				high_count++;
				sum += src_buf[pos];
			}
			count++;
		}
	}

	if (high_count) {
		switch (count) {
			case 3:
				if (high_count >= invalid_count_array[GRAD_INVALID_3])
					ret = -EINVAL;
				break;
			case 5:
				if (high_count >= invalid_count_array[GRAD_INVALID_5])
					ret = -EINVAL;
				break;
			case 8:
			default:
				if (high_count >= invalid_count_array[GRAD_INVALID_8])
					ret = -EINVAL;
				break;
		}

		#if (DBG_LEVEL > 1)
		printk(KERN_DEBUG"[mxt]point(%d,%d) has high(%d),adj(%d) ret %d\n",
			x,y,high_count,count,ret);
		#endif
		if (ret) {
			pos = x * y_size + y;
			src_buf[pos] = (s16)(sum / high_count);
			#if (DBG_LEVEL > 1)
			printk(KERN_INFO "[mxt]point(%d,%d) has high(%d),adj(%d) change %d->%d\n",
				x,y,high_count,count,val,src_buf[pos]);
			#endif
		}
	}
	return ret;
}

static int get_block_area_id(int x, int y, const struct rect *area,const struct rect *edge,const struct rect *surface)
{
	int y_size;
	struct rect center;
	int i,j,pos,id;

	y_size = surface->y1 - surface->y0 + 1;

	center.x0 = area->x0 + edge->x0;
	center.y0 = area->y0 + edge->y0;
	center.x1 = area->x1 - edge->x1;
	center.y1 = area->y1 - edge->y1 - 1;//y direction gradient, so y will decrease one

	//1.0 caculate jitter value as x axis
	for (id = 0, i = area->x0; i <= area->x1; i++) {
		for (j = area->y0; j < area->y1; j++) {
			pos = i * y_size + j;

			//block 0
			if (i < center.x0 &&
				j < center.y0) {
				id = 0;
				break;
			}

			//block 1
			if (i < center.x0 &&
				j >= center.y0 && j <= center.y1) {
				id = 1;
				break;
			}

			//block 2
			if (i < center.x0 &&
				j > center.y1) {
				id = 2;
				break;
			}

			//block 3
			if (i >= center.x0 && i <= center.x1 &&
				j < center.y0) {
				id = 3;
				break;
			}

			//block 4
			if (i >= center.x0 && i <= center.x1 &&
				j >= center.y0 && j <= center.y1) {
				id = 4;
				break;
			}
			//block 5
			if (i >= center.x0 && i <= center.x1 &&
				j >= center.y1) {
				id = 5;
				break;
			}
			//block 6
			if (i > center.x1 &&
				j < center.y0) {
				id = 6;
				break;
			}
			//block 7
			if (i > center.x1 &&
				j >= center.y0 && j <= center.y1) {
				id = 7;
				break;
			}
			//block 8
			if (i > center.x1 &&
				j > center.y1) {
				id = 8;
				break;
			}
		}
		pos = i * y_size + j;
	}

	return id;
}


static int caculate_gradient(s16 *dst_buf,s16 *src_buf,const s16 *diff_buf,const s16 *comp_buf,const struct rect *area,const struct rect *edge,const struct rect *surface,s16 grad_modules,s16 low_limit,s16 high_limit,s16 comp_limit,const int *invalid_count_array)
{
	int x_size,y_size;
	struct rect center;
	int pad0,pad1,pad2,pad3;
	int i,j,pos,sum;
	s16 diff;
	int result0, result1;
	bool comp;

	x_size = surface->x1 - surface->x0 + 1;
	y_size = surface->y1 - surface->y0 + 1;

	center.x0 = area->x0 + edge->x0;
	center.y0 = area->y0 + edge->y0;
	center.x1 = area->x1 - edge->x1;
	center.y1 = area->y1 - edge->y1 - 1;//y direction gradient, so y will decrease one

	pad0 = x_size * y_size;
	pad1 = pad0 + max(x_size,y_size);
	pad2 = pad1 + max(x_size,y_size);
	pad3 = pad2 + max(x_size,y_size);//user defined

	memset(dst_buf + pad3, 0, T37_PAD3_GRAD_NUMBER * sizeof(s16));

	//1.0 caculate jitter value as x axis
	for (i = area->x0; i <= area->x1; i++) {
		sum = 0;
		for (j = area->y0; j < area->y1; j++) {
			pos = i * y_size + j;
			sum += src_buf[pos];
			if (high_limit) {
				result0 = result1 = 0;
				if (invalid_count_array) {
					if (j == area->y0)
						result0 = surface_check_and_fix_invalid_value(src_buf,area,surface,
							i,j,low_limit,high_limit,invalid_count_array);
						result1 = surface_check_and_fix_invalid_value(src_buf,area,surface,
							i,j + 1,low_limit,high_limit,invalid_count_array);
					#if (DBG_LEVEL > 1)
					if (result0 || result1) {
						printk(KERN_INFO "[mxt]pos(%d,%d) val %d %d, %d %d\n",i,j,src_buf[pos],src_buf[pos + 1],result0,result1);
					}
					#endif
				}
			}
			dst_buf[pos] = (src_buf[pos] - src_buf[pos + 1]);
			if (grad_modules > 1)
				dst_buf[pos] /= grad_modules;
			if (diff_buf) {
				comp = true;
				if (comp_buf) {
					#if (DBG_LEVEL > 1)
					if (abs(dst_buf[pos]) > low_limit)
						printk(KERN_DEBUG "[mxt] comp pos(%d,%d) dst %d diff %d comp %d limit %d comp %d\n",
								i,j,dst_buf[pos],diff_buf[pos],comp_buf[pos],comp_limit,comp);
					#endif
					if (abs(comp_buf[pos]) < comp_limit)
						comp = false;
				}

				if (comp) {
					if (abs(dst_buf[pos]) > high_limit)
						dst_buf[pos] -= diff_buf[pos];
					else if (abs(dst_buf[pos]) > low_limit && abs(diff_buf[pos]) > low_limit) {
						diff = dst_buf[pos] - diff_buf[pos];
						if (abs(diff) < abs(dst_buf[pos]))
							dst_buf[pos] = diff;
					}
				}

				#if (DBG_LEVEL > 1)
				if (abs(dst_buf[pos]) > low_limit) {
					if (comp_buf)
						printk(KERN_DEBUG "[mxt] diff pos(%d,%d) dst %d diff %d comp %d, comp = %d limit(%d,%d,%d)\n",
								i,j,dst_buf[pos],diff_buf[pos],comp_buf[pos],comp,high_limit,low_limit,comp_limit);
					else
						printk(KERN_DEBUG "[mxt] diff pos(%d,%d) dst %d diff %d comp x, comp = %d limit(%d,%d,%d)\n",
								i,j,dst_buf[pos],diff_buf[pos],comp,high_limit,low_limit,comp_limit);
				}
				#endif
			}

			//block 0
			if (i < center.x0 && j < center.y0) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 0] < dst_buf[pos])//if max is negative, we set to zero
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 0] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 0] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 0] = dst_buf[pos];
			}

			//block 1
			if (i < center.x0 && j >= center.y0 && j <= center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 1] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 1] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 1] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 1] = dst_buf[pos];
			}

			//block 2
			if (i < center.x0 && j > center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 2] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 2] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 2] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 2] = dst_buf[pos];
			}

			//block 3
			if (i >= center.x0 && i <= center.x1 && j < center.y0) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 3] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 3] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 3] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 3] = dst_buf[pos];
			}

			//block 4
			if (i >= center.x0 && i <= center.x1 && j >= center.y0 && j <= center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 4] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 4] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 4] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 4] = dst_buf[pos];
			}
			//block 5
			if (i >= center.x0 && i <= center.x1 && j >= center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 5] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 5] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 5] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 5] = dst_buf[pos];
			}
			//block 6
			if (i > center.x1 && j < center.y0) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 6] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 6] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 6] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 6] = dst_buf[pos];
			}
			//block 7
			if (i > center.x1 && j >= center.y0 && j <= center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 7] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 7] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 7] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 7] = dst_buf[pos];
			}
			//block 8
			if (i > center.x1 && j > center.y1) {
				if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 8] < dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MAX + 8] = dst_buf[pos];
				if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 8] > dst_buf[pos])
					dst_buf[pad3 + T37_PAD3_GRAD_MIN + 8] = dst_buf[pos];
			}

			//Max/Min value
			if (dst_buf[pad3 + T37_PAD3_GRAD_MAX + 9] < dst_buf[pos])
				dst_buf[pad3 + T37_PAD3_GRAD_MAX + 9] = dst_buf[pos];
			if (dst_buf[pad3 + T37_PAD3_GRAD_MIN + 9] > dst_buf[pos])
				dst_buf[pad3 + T37_PAD3_GRAD_MIN + 9] = dst_buf[pos];
		}
		pos = i * y_size + j;
		sum += src_buf[pos];
		if (j)
			sum /= j;
		dst_buf[pad2 + T37_PAD2_AVG_H_LINE + i] = sum;
	}

	dst_buf[pad3 + T37_PAD3_GRAD_MODULES] = grad_modules;

	return 0;
}

static void gradient_cache_reset(struct plugin_cal *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	struct rect surface;
	int x_size,y_size;
	int i;

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	x_size = surface.x1 - surface.x0 + 1;
	y_size = surface.y1 - surface.y0 + 1;

	for (i = 0; i < T72_NOISE_STATE_TYPES; i++) {
			memset(obs->grad_cache[i].x_axis, -1, x_size * y_size * sizeof(s16));
			memset(obs->grad_cache[i].y_axis, -1, x_size * y_size * sizeof(s16));
	}
}

//output: max_array:
static int gradient_cache_renew(s16 *grad_buf, const struct rect *area, const struct rect *edge, const struct rect *surface, s16 *cache, const int *drift_coef, s16 low_limit, const int * grad_block_high_limit_list)
{
	int y_size;
	int i,j,pos;
	s16 val,diff,val_grad,val_drift,high_limit;
	int id,drift;

	if (!grad_buf)
		return -EINVAL;

	y_size = surface->y1 - surface->y0 + 1;

	for (i = area->x0; i <= area->x1; i++) {
		for (j = area->y0; j < area->y1; j++) {//y direction gradient, so y will subtract one
			pos = i * y_size + j;
			val = abs(grad_buf[pos]);
			val_grad = cache[pos];
			if (val_grad != val || (val_grad < 0)) {//grad_level is more than zero
				if (val_grad < 0) {
					val_grad = val + (low_limit >> 1);
					if(val_grad < low_limit)
						val_grad = low_limit;
					#if (DBG_LEVEL > 1)
					printk(KERN_DEBUG "[mxt]cache[%d,%d]: %d^ (val %d, limit %d)\n",
						i, j, val_grad,grad_buf[pos],low_limit);
					#endif
				}else {
					id = get_block_area_id(i,j,area,edge,surface);
					high_limit = grad_block_high_limit_list[id];

					diff = val_grad - val;
					diff = abs(diff);

					if (val < low_limit) {
						if (val_grad < val)
							drift = 0;
						else
							drift = 1;
					}else if (val >= low_limit && diff <= low_limit) {
						if (val_grad < val)
							drift = 2;
						else
							drift = 3;
					}else {
						if (val_grad < val)
							drift = 4;
						else
							drift = 5;
					}

					val_drift = (((int)diff * drift_coef[drift]) >> 10);
					if (val_grad > val)
						val_drift = -val_drift;

					val_grad += val_drift;

					if (val_grad < low_limit)
						val_grad = low_limit;

					#if (DBG_LEVEL > 1)
					printk(KERN_DEBUG "[mxt]cache[%d,%d]: %d* (cache %d, val %d, diff %d, limit %d %d) drift(%d,%d,%d)\n", 
						i, j, val_grad, cache[pos],val, diff, low_limit,high_limit,val_drift, drift, drift_coef[drift]);
					#endif
				}

				cache[pos] = val_grad;
			}else {
				#if (DBG_LEVEL > 1)
				printk(KERN_DEBUG "[mxt]cache[%d]: %d\n", i, cache[pos]);
				#endif
			}
		}
	}

	return 0;
}

static int gradient_percent(s16 grad_val,s16 grad_base,struct stat_grad_config *grad_level_list,int num_grad_level_list)
{
	int j;
	int section,val;
	int percent = 0;


	grad_val = abs(grad_val);
	if (grad_val >= grad_base + grad_level_list[0].thld) { //grad_level_list store as ascending order
		for (j = num_grad_level_list - 1; j >= 0; j--) {
			if (grad_level_list[j].percent > 0) {
				if (grad_val >= grad_base + grad_level_list[j].thld) {
					if (j < num_grad_level_list - 1) {
						section = grad_level_list[j + 1].thld - grad_level_list[j].thld;
						val = grad_level_list[j + 1].percent - grad_level_list[j].percent;
					} else {
						if (num_grad_level_list > 1) {
							section = grad_level_list[j].thld - grad_level_list[j - 1].thld;
							val = grad_level_list[j].percent - grad_level_list[j - 1].percent;
						} else {
							section = 0;
						}
					}
					if (section) {
						percent = (grad_val - (grad_base + grad_level_list[j].thld)) * val / section + grad_level_list[j].percent;
					} else {
						percent = grad_level_list[j].percent;
					}

					#if (DBG_LEVEL > 1)
					printk(KERN_INFO "[mxt]val %hd base %hd %d[%d %hu] percent %d\n",
						grad_val, grad_base , j, grad_level_list[j].thld, grad_level_list[j].percent,percent);
					#endif
					break;
				}
			}
		}
	}

	return percent;
}

static int caculate_gradient_accumulator(s16 *grad_buf,const struct rect *area, const struct rect *edge, const struct rect *surface,struct stat_grad_config *grad_level_list,int num_grad_level_list,s16 *cache)
{
	int y_size;
	int i,j,pos;
	int accu, accumulator= 0;

	if (!grad_buf)
		return -EINVAL;;

	y_size = surface->y1 - surface->y0 + 1;

	//1.0 caculate jitter value as x axis 
	for (i = area->x0; i <= area->x1; i++) {
		for (j = area->y0; j < area->y1; j++) {//y direction gradient, so y will subtract one
			pos = i * y_size + j;
			accu = gradient_percent(grad_buf[pos], cache[pos], grad_level_list, num_grad_level_list);
			accumulator += accu;
			#if (DBG_LEVEL > 1)
			if (accu) {
				printk(KERN_INFO "[mxt][%d,%d]val %hd base %hd accu %d sum %d\n",
					i,j,grad_buf[pos], cache[pos], accu, accumulator);
			}
			#endif
		}
	}

	return accumulator;
}

int mxt_get_grad_val_max(struct plugin_cal *p,int sf, int block_id)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	const struct rect *surface;

	s16 *grad_buf;
	int x_size,y_size;
	int pad0,pad1,pad2,pad3;
	s16 val;

	surface = &dcfg->m[MX_AA];
	x_size = surface->x1 - surface->x0 + 1;
	y_size = surface->y1 - surface->y0 + 1;

	pad0 = x_size * y_size;
	pad1 = pad0 + max(x_size,y_size);
	pad2 = pad1 + max(x_size,y_size);
	pad3 = pad2 + max(x_size,y_size);//user defined

	grad_buf = obs->buf[sf];
	val = abs(grad_buf[pad3 + T37_PAD3_GRAD_MIN + block_id]);
	val = max(val,grad_buf[pad3 + T37_PAD3_GRAD_MAX  + block_id]);

	return (int)val;
}

int mxt_get_grad_val_max_all(struct plugin_cal *p, int grad_id, int block_id, unsigned long flag)
{
	int  val, max = 0x7fffffff;

	if (test_flag(ACCUMULATOR_GRAD_X, &flag)) {
		val = mxt_get_grad_val_max(p, T37_GRAD0_X + grad_id, block_id);
		if (max > val)
			max = val;
	}

	if (test_flag(ACCUMULATOR_GRAD_Y, &flag)) {
		val = mxt_get_grad_val_max(p, T37_GRAD0_Y + grad_id, block_id);
		if (max > val)
			max = val;
	}

	if (test_flag(ACCUMULATOR_GRAD_K, &flag)) {
		val = mxt_get_grad_val_max(p, T37_GRAD0_K + grad_id, block_id);
		if (max > val)
			max = val;
	}

	return max;
}

bool check_gradient_block_limit(struct plugin_cal *p, int grad_id, int * grad_block_high_limit_list, unsigned long grad_acc_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int grad_val;
	int i;

	for (i = 0; i < T37_PAD3_GRAD_BLOCKS; i++) {
		if (grad_block_high_limit_list[i]) {
			grad_val = mxt_get_grad_val_max_all(p, grad_id, i, grad_acc_flag);
			dev_dbg2(dev, "gradient block %d grad %d limit %d\n",
					i,
					grad_val,
					grad_block_high_limit_list[i]);
			if (grad_val > grad_block_high_limit_list[i]) {
				dev_info2(dev, "gradient block %d grad %d over limit %d\n",
						i,
						grad_val,
						grad_block_high_limit_list[i]);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int mxt_surface_gradient_statistics(struct plugin_cal *p, unsigned long pl_flag, int sf_x, int sf_y,
					int sf_c, int sf_grad_x, int sf_grad_y, int sf_grad_k,
					struct stat_grad_config *grad_level_list, int num_grad_level_list,
					const struct grad_cache_type *gd_cache,int drift,unsigned long grad_acc_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	const struct plug_config *pl_cfg = &pl->config;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;

	struct rect area,area_t,edge,edge_t,surface,surface_t;
	const struct rect *p_area,*p_edge,*p_surface;
	s16 *src_x_buf,*src_y_buf,*cx_buf,*cy_buf,*ck_buf,*c_buf,*diff_buf,*comp_buf,*cache = NULL;
	s16 grad_t_modules,grad_k_modules;
	int drift2,accu,accu_x,accu_y,accumulator = 0;
	int *grad_drift_coef,grad_drift_coef2[GRAD_DRIFT_ITEMS];
	int *grad_invalid_count_array;

	int state,grad_block_low_limit,grad_block_highest_limit,comp_limit;

	grad_t_modules = cfg->grad_t_modules;
	grad_k_modules = cfg->grad_k_modules;
	grad_drift_coef = cfg->grad_drift_coef_array[drift];
	if (test_flag(ACCUMULATOR_GRAD_DRIFT_FAST, &grad_acc_flag))
		drift2 = GRAD_DRIFT_FAST;
	else if (test_flag(ACCUMULATOR_GRAD_DRIFT_NORM, &grad_acc_flag))
		drift2 = GRAD_DRIFT_NORM;
	else
		drift2 = GRAD_DRIFT_SLOW;
	memcpy(&grad_drift_coef2, &cfg->grad_drift_coef_array[drift2], sizeof(grad_drift_coef2));

	src_x_buf = obs->buf[sf_x];
	src_y_buf = obs->buf[sf_y];
	if (test_flag(ACCUMULATOR_GRAD_COMP, &grad_acc_flag))
		comp_buf = obs->buf[sf_c];
	else
		comp_buf = NULL;

	if (test_flag(ACCUMULATOR_GRAD_INVALUE, &grad_acc_flag)) {
		grad_invalid_count_array = cfg->grad_invalid_count_array;
	} else {
		grad_invalid_count_array = NULL;
	}
	cx_buf = obs->buf[sf_grad_x];
	cy_buf = obs->buf[sf_grad_y];
	ck_buf = obs->buf[sf_grad_k];

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	memcpy(&edge, &cfg->edge, sizeof(struct rect));
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &grad_acc_flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		if (edge.x1 > 1)
			edge.x1--;
		//surface.x1--;
	}
	memcpy(&surface_t, &surface, sizeof(struct rect));
	memcpy(&area_t, &area, sizeof(struct rect));
	memcpy(&edge_t, &edge, sizeof(struct rect));
	swap(surface_t.x0,surface_t.y0);
	swap(surface_t.x1,surface_t.y1);
	swap(area_t.x0,area_t.y0);
	swap(area_t.x1,area_t.y1);
	swap(edge_t.x0,edge_t.y0);
	swap(edge_t.x1,edge_t.y1);

	state = get_t9_t100_state(obs->step, 0, pl_flag);
	grad_block_low_limit = cfg->grad_block_low_limit;
	if (state != T9_T100_NORMAL) {
		grad_block_low_limit += (int)((pl_cfg->t9_t100_cfg[state].threshold - pl_cfg->t9_t100_cfg[T9_T100_NORMAL].threshold) >> 1);
	}
	grad_block_low_limit = caculate_threshlod(grad_block_low_limit,100);
	grad_block_highest_limit = caculate_threshlod(cfg->grad_block_high_limit,cfg->grad_block_highest_limit_ratio);
	comp_limit = caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_mid_limit_ratio);

	dev_dbg2(dev, "grad x,y,z (%d,%d,%d), flag 0x%lx \n",
		sf_grad_x,sf_grad_y,sf_grad_k,grad_acc_flag);

	accu = accu_x = accu_y = 0;
	c_buf = NULL;
	if (test_flag(ACCUMULATOR_GRAD_X, &grad_acc_flag)) {
		//touch x axis
		if (test_flag(ACCUMULATOR_GRAD_DIFF, &grad_acc_flag))
			diff_buf = obs->buf[sf_grad_x - 1];
		else
			diff_buf = NULL;
		cache = gd_cache->x_axis;

		//print_matrix_surface(sf_x,src_x_buf,&surface,/*P_AA|*/P_PAD3);
		caculate_gradient(cx_buf,src_x_buf,diff_buf,comp_buf,&area,&edge,&surface,grad_t_modules,grad_block_low_limit,grad_block_highest_limit,comp_limit,grad_invalid_count_array);
		#if (DBG_LEVEL > 1)
		print_matrix_surface(sf_grad_x,cx_buf,&surface,/*P_PAD2|*/P_PAD3);
		#endif
		//here caculate grad statistic value as x-axis
		if (!test_flag(ACCUMULATOR_GRAD_DIFF|ACCUMULATOR_GRAD_CACULATE, &grad_acc_flag))
			gradient_cache_renew(cx_buf,&area,&edge,&surface,
				cache,grad_drift_coef,(s16)grad_block_low_limit,cfg->grad_block_high_limit_list);
		accu_x = caculate_gradient_accumulator(cx_buf,&area,&edge,&surface,
				grad_level_list,num_grad_level_list,cache);

		accu = accu_x;
		c_buf = cx_buf;
		p_area = &area;
		p_edge = &edge;
		p_surface = &surface;
	}
	//touch y axis
	transpose_matrix(src_y_buf,src_x_buf,&surface);
	if (test_flag(ACCUMULATOR_GRAD_Y, &grad_acc_flag)) {
		if (test_flag(ACCUMULATOR_GRAD_DIFF, &grad_acc_flag))
			diff_buf = obs->buf[sf_grad_y - 1];
		else
			diff_buf = NULL;
		cache = gd_cache->y_axis;

		//print_matrix_surface(sf_y,src_y_buf,&surface_t,P_AA|P_PAD3);
		caculate_gradient(cy_buf,src_y_buf,diff_buf,comp_buf,&area_t,&edge_t,&surface_t,
			grad_t_modules,grad_block_low_limit,grad_block_highest_limit,comp_limit,grad_invalid_count_array);
		#if (DBG_LEVEL > 1)
		print_matrix_surface(sf_grad_y,cy_buf,&surface_t,P_AA|P_PAD2|P_PAD3);
		#endif

		//here caculate grad statistic value as y-axis
		if (!test_flag(ACCUMULATOR_GRAD_DIFF|ACCUMULATOR_GRAD_CACULATE, &grad_acc_flag))
			gradient_cache_renew(cy_buf,&area_t,&edge_t,&surface_t,
				cache,grad_drift_coef,(s16)grad_block_low_limit,cfg->grad_block_high_limit_list);
		accu_y = caculate_gradient_accumulator(cy_buf,&area_t,&edge_t,&surface_t,
				grad_level_list,num_grad_level_list,cache);

		accu = accu_y;
		c_buf = cy_buf;
		p_area = &area_t;
		p_edge = &edge_t;
		p_surface = &surface_t;
	}

	//auto select x,y gradient
	if (test_flag(ACCUMULATOR_GRAD_X, &grad_acc_flag) && 
		test_flag(ACCUMULATOR_GRAD_Y, &grad_acc_flag)) {
		if (accu_x < accu_y) {
			accu = accu_x;
			c_buf = cx_buf;
			p_area = &area;
			p_edge = &edge;
			p_surface = &surface;
			cache = gd_cache->x_axis;
		}else{
			accu = accu_y;
			c_buf = cy_buf;
			p_area = &area_t;
			p_edge = &edge_t;
			p_surface = &surface_t;
			cache = gd_cache->y_axis;
		}
	}
	if (test_flag(ACCUMULATOR_GRAD_X, &grad_acc_flag) || 
		test_flag(ACCUMULATOR_GRAD_Y, &grad_acc_flag)) {
		if ((accu < cfg->grad_accumulator_high) && test_flag(ACCUMULATOR_GRAD_DRIFT_MASK, &grad_acc_flag)&&
			!test_flag(ACCUMULATOR_GRAD_DIFF|ACCUMULATOR_GRAD_CACULATE, &grad_acc_flag)) {
			module_coef(grad_drift_coef2,GRAD_DRIFT_ITEMS,accu,cfg->grad_accumulator_high);
			gradient_cache_renew(c_buf,p_area,p_edge,p_surface,
				cache,grad_drift_coef2,(s16)grad_block_low_limit,cfg->grad_block_high_limit_list);
			if (accu < cfg->grad_accumulator_low)
				accu = caculate_gradient_accumulator(c_buf,p_area,p_edge,p_surface,
					grad_level_list,num_grad_level_list,cache);
		}
	}
	accumulator += accu;

	//touch key
	if (dcfg->t15.xsize) {
		cache = gd_cache->y_axis;

		memcpy(&area_t, &dcfg->m[MX_K], sizeof(struct rect));
		swap(area_t.x0,area_t.y0);
		swap(area_t.x1,area_t.y1);
		memset(&edge_t,0,sizeof(struct rect));

		if (test_flag(ACCUMULATOR_GRAD_DIFF, &grad_acc_flag))
			diff_buf = obs->buf[sf_grad_k - 1];
		else
			diff_buf = NULL;
		caculate_gradient(ck_buf,src_y_buf,diff_buf,comp_buf,&area_t,&edge_t,&surface_t,
			grad_k_modules,grad_block_low_limit,grad_block_highest_limit,comp_limit,grad_invalid_count_array);
		#if (DBG_LEVEL > 1)
		print_matrix_surface(sf_grad_k,ck_buf,&surface_t,P_AA|P_PAD2|P_PAD3);
		#endif

		//here caculate grad statistic value as k-axis
		if (!test_flag(ACCUMULATOR_GRAD_DIFF|ACCUMULATOR_GRAD_CACULATE, &grad_acc_flag))
			gradient_cache_renew(ck_buf,&area_t,&edge_t,&surface_t,
				cache,grad_drift_coef,(s16)grad_block_low_limit,cfg->grad_block_high_limit_list);
		if (test_flag(ACCUMULATOR_GRAD_K, &grad_acc_flag))
			accumulator += caculate_gradient_accumulator(ck_buf,&area_t,&edge_t,&surface_t,
					grad_level_list,num_grad_level_list,cache);
	}
	#if (DBG_LEVEL > 0)
	dev_info2(dev, "sf %d flag 0x%lx accumulator(%d, x %d y %d)\n",sf_x,grad_acc_flag,accumulator,accu_x,accu_y);
	#endif
	return accumulator;
}

//bugs with trigger by t65
//when the finger at each side of the screen, calibration then release fingers, the center of this x line will burst 100~400 delta
static int mxt_surface_x_line_check_t65_burst(struct plugin_cal *p, unsigned long flag, int sf, s16 thld_l, s16 thld_h, int percnet)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	struct rect surface,area;
	int y_size;
	s16 *src_x_buf,d_max,d_min,d_first,d_last;
	int i,j,pos;
	int count_x,count_y;
	bool edge_match;

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	y_size = surface.y1 - surface.y0 + 1;
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}
	src_x_buf = obs->buf[sf];

	count_x = 0;
	for (i = area.x0; i <= area.x1; i++) {
		count_y = 0;
		d_max = 0;
		d_min = 0x7fff;
		d_first = d_last = 0;
		edge_match = false;
		for (j = area.y0 + 1; j < area.y1; j++) {//exclude boarder
			pos = i * y_size + j;

			d_min = min_t(s16, src_x_buf[pos], d_min);
			d_max = max_t(s16, src_x_buf[pos], d_max);
			if ((d_max ^ d_min) < 0)
				break;

			if (src_x_buf[pos] >= thld_l || src_x_buf[pos] <= -thld_l) {
				count_y++;
			}
		}

		dev_dbg2(dev, "mxt_surface_each_line_check x%d: %d percent %d\n",
				i,(d_max ^ d_min),
				count_y * 100 / (area.y1 -  area.y0));

		if ((d_max ^ d_min) >= 0 &&
				(count_y * 100 >= (area.y1 -  area.y0)*percnet)) {

			pos = i * y_size + area.y0;
			d_first = src_x_buf[pos];
			if ((d_first ^ d_max) < 0 && (/*d_first >= thld_h ||*/ d_first <= -thld_h))
				edge_match = true;

			pos = i * y_size + area.y1;
			d_last = src_x_buf[pos];
			if ((d_last ^ d_max) < 0 && (/*d_last >= thld_h ||*/ d_last <= -thld_h))
				edge_match = true;

			if (edge_match)
				count_x++;
		}

		dev_dbg2(dev, "mxt_surface_each_line_check x%d: count %d, max(%d,%d), first(%d,%d) match %d\n",
				i,count_y,d_max,d_min,d_first,d_last,edge_match);
	}
	dev_dbg2(dev, "mxt_surface_each_line_check %d\n",count_x);

	if (count_x)
		return -EINVAL;

	return 0;
}


//bugs with trigger by t65 + t80
//when the palm is on screen with clothes, sometime the x line is zero in center and very big in 4 edge
static int mxt_surface_x_line_check_zero_burst(struct plugin_cal *p, unsigned long flag, int sf, s16 thld_l)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	struct rect surface,area;
	s16 *src_x_buf;
	int y_size;
	int i,j,pos;
	int count_x;

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	y_size = surface.y1 - surface.y0 + 1;
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}
	src_x_buf = obs->buf[sf];

	count_x = 0;
	for (i = area.x0; i <= area.x1; i++) {
		for (j = area.y0; j <= area.y1; j++) {
			pos = i * y_size + j;
			if (abs(src_x_buf[pos]) > thld_l) {
				break;
			}
		}

		if (j == area.y1 + 1) {
			if (i< area.x1)
				count_x++;
			dev_info2(dev, "mxt_surface_each_line_check x%d zero count %d\n",
				 i,count_x);
		}
		dev_dbg(dev, "mxt_surface_each_line_check x%d count %d\n",
			 i,j);
	}

	if (count_x) {
		dev_info2(dev, "mxt_surface_each_line_check zero count %d\n",
			 count_x);
		return -EINVAL;
	}
	return 0;
}

//bugs with T8 anti-touch
//T8 won't trigger calibration if thld it too high
static int mxt_surface_soft_check_anti_touch(struct plugin_cal *p, unsigned long flag, int sf, const struct t9_t100_config *t9_t100_cfg, const struct t8_config *t8_cfg)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;

	struct rect surface,area;
	s16 *src_x_buf;
	int y_size;
	int i,j,pos;
	s16 thld_l, thld_suspend;
	int area_thld, anti_ratio;
	int percent,suspend,area_t,area_t_anti,area_sum;

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	y_size = surface.y1 - surface.y0 + 1;
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}
	src_x_buf = obs->buf[sf];


	thld_l = caculate_threshlod(t9_t100_cfg->internal_threshold, cfg->soft_t8_internal_thld_percent);
	thld_l += caculate_threshlod(t9_t100_cfg->threshold, 100 - cfg->soft_t8_internal_thld_percent);
	thld_suspend = caculate_threshlod(t8_cfg->atchcalsthr, 100);
	area_thld = t8_cfg->atchfrccalthr;
	anti_ratio = t8_cfg->atchfrccalratio + 128;

	percent = 0;
	suspend = 0;
	area_t = area_t_anti = 0;
	for (i = area.x0; i <= area.x1; i++) {
		for (j = area.y0; j <= area.y1; j++) {
			pos = i * y_size + j;

			if (src_x_buf[pos] > thld_suspend)
				suspend++;

			if (src_x_buf[pos] > thld_l)
				area_t++;

			if (src_x_buf[pos] < -thld_l)
				area_t_anti++;
		}
	}

	if (area_t_anti) {
		area_sum = area_t + area_t_anti;
		#if (DBG_LEVEL > 1)
		dev_info2(dev, "mxt_surface_soft_check_anti_touch area(%d,%d,%d) thld(%d,%d,%d,%d)\n",
			 area_t,area_t_anti,suspend,
			 thld_l,thld_suspend,area_thld,anti_ratio);
		#endif
		if (anti_ratio && area_thld && (area_sum > area_thld) && (area_sum * anti_ratio < (area_t_anti << 8))) {
			dev_info2(dev, "mxt_surface_soft_check_anti_touch area(%d,%d,%d) ratio %d area %d\n",
				 area_t,area_t_anti,suspend,anti_ratio,area_thld);
			return -EINVAL;
		}

		if (thld_suspend && (area_sum > (area_thld >> 1)) && (!suspend && area_t_anti)) {
			dev_info2(dev, "mxt_surface_soft_check_anti_touch touch(%d,%d,%d)\n",
				 area_t,area_t_anti,suspend);
			return -EINVAL;
		}
	}

	return 0;
}

//1 caculate reference average each x line(skip max/min)
//2 if avg(x0) - avg(x1) less than -high_limit
//3 if each x line elements abs_diff is more than thld_h then this is drop line
static int x_line_check_buf_ref_drop(s16 *r_buf,s16 *d_buf,const struct rect *area,const struct rect *surface, s16 thld_h, s16 high_limit)
{
	int x_size,y_size;
	int pad0,pad1,pad2;
	int i,j,pos,pos0,pos1,sum;
	s16 max,min,diff;
	bool delta_high;

	x_size = surface->x1 - surface->x0 + 1;
	y_size = surface->y1 - surface->y0 + 1;

	pad0 = x_size * y_size;
	pad1 = pad0 + max(x_size,y_size);
	pad2 = pad1 + max(x_size,y_size);

	memset(r_buf + pad2, 0, y_size * sizeof(s16));

	//1 caculate average of surface line
	for (i = area->x0; i <= area->x1; i++) {
		sum = 0;
		max = min = 0;
		for (j = area->y0; j <= area->y1; j++) {
			pos = i * y_size + j;
			sum += r_buf[pos];
			min = min_t(s16, r_buf[pos], min);
			max = min_t(s16, r_buf[pos], max);
		}

		if (j > 3) {
			sum -= max + min;
			sum /= j - 2;
		} else if (j)
			sum /= j;
		r_buf[pad2 + T37_PAD2_AVG_H_LINE + i] = sum;
	}

	//2 check drop line
	for (i = area->x0; i <= area->x1; i++) {
		pos0 = pad2 + T37_PAD2_AVG_H_LINE + i;
		if (i == area->x1)
			pos1 = pad2 + T37_PAD2_AVG_H_LINE;
		else
			pos1 = pos0 + 1;
		diff = r_buf[pos0] - r_buf[pos1];
		if (diff < -high_limit) {
			delta_high = false;
			for (j = area->y0; j <= area->y1; j++) {
				pos = i * y_size + j;
				diff = r_buf[pos] - r_buf[pos1];
				diff = abs(diff);
				if (diff < high_limit)
					break;
				if (d_buf[pos] > thld_h || d_buf[pos] < -thld_h)
					delta_high = true;
			}

			if (j == area->y1) {
				#if (DBG_LEVEL > 1)
				printk(KERN_INFO "[mxt]found a drop line %d\n", i);
				print_matrix("drop ref",&r_buf[i * y_size],1,y_size);
				print_matrix("drop delta",&d_buf[i * y_size],1,y_size);
				#endif
				if (delta_high)
					return -EINVAL;
			}
		}
	}

	return 0;
}


static int mxt_surface_x_line_check_ref_drop(struct plugin_cal *p, int sf_r, int sf_d, s16 thld_h, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;

	struct rect area,surface;
	s16 *d_buf,*r_buf;

	int grad_block_highest_limit;
	int ret;

	r_buf = obs->buf[sf_r];
	d_buf = obs->buf[sf_d];

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}

	grad_block_highest_limit = caculate_threshlod(cfg->grad_block_high_limit,cfg->grad_block_highest_limit_ratio);
	if (test_flag(ACCUMULATOR_GRAD_Y, &flag)) {
		swap(surface.x0,surface.y0);
		swap(surface.x1,surface.y1);
		swap(area.x0,area.y0);
		swap(area.x1,area.y1);
	}

	ret = x_line_check_buf_ref_drop(r_buf,d_buf,&area,&surface,thld_h,grad_block_highest_limit);

	return ret;
}

//signal drop low more than percent will do calibration
static int mxt_surface_ref_iron_check(struct plugin_cal *p, unsigned long flag, int diff_sf, int d_sf, s16 thld_l, s16 thld_m, s16 thld_h, int percnet)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;

	struct rect surface,area;
	int x_size,y_size;
	s16 *diff_buf,*d_buf;
	int i,j,pos;
	int ref_low,delta_high,invalid;
	bool found;

	diff_buf = obs->buf[diff_sf];
	d_buf = obs->buf[d_sf];

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	y_size = surface.y1 - surface.y0 + 1;

	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}

	x_size = area.x1 - area.x0 + 1;

	ref_low = delta_high = invalid = 0;
	found = false;
	for (i = area.x0; i <= area.x1; i++) {
		for (j = area.y0 ; j <= area.y1; j++) {
			pos = i * y_size + j;

			if (diff_buf[pos] < -thld_l)
				ref_low++;
			else if (diff_buf[pos] > -thld_l && d_buf[pos] > thld_m)
				delta_high++;

			if (d_buf[pos] > thld_m && abs(d_buf[pos]) > thld_h)
				invalid++;
		}
	}
	#if (DBG_LEVEL > 1)
	if (invalid) {
		printk(KERN_INFO "[mxt]found a drop surface ref %d delta %d inv %d thld(%hd,%hd,%hd)\n", 
			ref_low,delta_high,invalid,thld_l,thld_m,thld_h);
	}
	#endif

	if ((ref_low + delta_high) * 100 >= x_size * y_size * percnet && invalid) {
		#if (DBG_LEVEL > 0)
		printk(KERN_INFO "[mxt]found a drop surface ref %d delta %d invalid %d\n", ref_low,delta_high,invalid);
		#endif
		return -EINVAL;
	}

	return 0;
}

//T80 burst
//delta high
//in one y line, abs(delta) > high_limit &&  number(delta > thld_h) >= y_line - 2 && signal same
static int surface_delta_high_check(s16 *d_buf,const struct rect *area, const struct rect *surface, s16 thld, s16 thld_h, int count_skip)
{
	int y_size;
	s16 d_max,d_min;
	int i,j,pos;
	int high_line,count_d;

	y_size = surface->y1 - surface->y0 + 1;

	high_line = 0;
	for (i = area->x0 ; i <= area->x1; i++) {
		count_d = 0;
		d_max = d_min = 0;
		for (j = area->y0; j <= area->y1; j++) {
			pos = i * y_size + j;

			if (d_buf[pos] > thld || d_buf[pos] < -thld) {

				d_min = min_t(s16, d_buf[pos], d_min);
				d_max = max_t(s16, d_buf[pos], d_max);

				count_d++;
				#if (DBG_LEVEL > 1)
				printk(KERN_INFO "[mxt](%d,%d) %d count_d %d\n", 
					i,j,d_buf[pos],count_d);
				#endif
			}
		}

		if ((d_min ^ d_min) >= 0 &&
			count_d + count_skip >= y_size &&
			(d_max > thld_h || d_min < -thld_h)) {
			high_line++;

			#if (DBG_LEVEL > 0)
			printk(KERN_INFO "[mxt]check line %d delta count %d %d\n", i,count_d,high_line);
			#endif
		}

		#if (DBG_LEVEL > 1)
		printk(KERN_INFO "[mxt]check line %d delta count %d %d\n", i,count_d,high_line);
		#endif
	}

	return high_line;
}

static int mxt_surface_delta_high_check(struct plugin_cal *p, unsigned long flag, int sf, s16 thld, s16 thld_h, int skip_num)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;

	int x_size,y_size;
	struct rect surface,area;
	s16 *d_buf;
	int high_line;

	d_buf = obs->buf[sf];

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_T], sizeof(struct rect));
	x_size = surface.x1 - surface.x0 + 1;;
	y_size = surface.y1 - surface.y0 + 1;

	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
	}

	high_line = 0;
	if (test_flag(ACCUMULATOR_GRAD_X, &flag))
		high_line = surface_delta_high_check(d_buf, &area, &surface, thld, thld_h, skip_num);

	if (test_flag(ACCUMULATOR_GRAD_Y, &flag)) {
		transpose_matrix(d_buf,d_buf,&surface);
		swap(surface.x0,surface.y0);
		swap(surface.x1,surface.y1);
		swap(area.x0,area.y0);
		swap(area.x1,area.y1);

		high_line += surface_delta_high_check(d_buf, &area, &surface, thld, thld_h, skip_num);
	}
	return high_line;
}

static int mxt_surface_compare_part(struct plugin_cal *p, const struct point *from, const struct point *to, int sf, int compare,s16 diff_threshold_t9,s16 diff_threshold_t15, s16 *diff_out, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	struct device *dev = dcfg->dev;
	const struct rect *surface,*area_t,*area_k;
	int i,j,pos,y_size;
	s16 diff;
	s16 *c0_buf,*c1_buf;

	dev_dbg2(dev, "mxt compare surface %d,%d (%d,%d)~(%d,%d)\n",
			sf,compare,
			from->x,from->y,to->x,to->y);

	surface = &dcfg->m[MX_AA];
	area_t = &dcfg->m[MX_T];
	area_k = &dcfg->m[MX_K];

	y_size = surface->y1 - surface->y0 + 1;
	c0_buf = obs->buf[sf];
	c1_buf = obs->buf[compare];

	for (i = from->x; i <= to->x; i++) {
		for (j = 0; j < y_size; j++) {

			if (j == 0 && i == from->x) {//first element
				j += from->y;
			}

			if (i == to->x) {//last element
				if (j >= to->y)
					break;
			}

			pos = i * y_size + j;
			if (sf != compare)
				diff = c0_buf[pos] - c1_buf[pos];
			else
				diff = c0_buf[pos];

			if ((i >= area_t->x0) && (i <= area_t->x1)&&
					(j >= area_t->y0)&&(j <= area_t->y1)) {

				dev_dbg2(dev, "T(%d,%d) %d is %d v %d \n",i,j,pos,c0_buf[pos],c1_buf[pos]);

				if (((flag & T37_COMPARE_DIRECTION_POS) && (diff > diff_threshold_t9)) ||
						((flag & T37_COMPARE_DIRECTION_NEG)&& (diff < -diff_threshold_t9))) {
					dev_dbg2(dev, "mxt surface compare part %d %d compare T(%d,%d) diff %d surface0 %d %d thrd %d\n",
						sf,compare,i,j,diff,c0_buf[pos],c1_buf[pos],diff_threshold_t9);
					if (diff_out)
						*diff_out = diff;
					return -EINVAL;
				}
			} else if ((i >= area_k->x0) && (i <= area_k->x1)&&
					(j >= area_k->y0)&&(j <= area_k->y1)) {

				dev_dbg2(dev, "K(%d,%d) %d is %d v %d \n",i,j,pos,c0_buf[pos],c1_buf[pos]);

				if (((flag & T37_COMPARE_DIRECTION_POS) && (diff > diff_threshold_t15)) ||
						((flag & T37_COMPARE_DIRECTION_NEG) && (diff < -diff_threshold_t15))) {
					dev_dbg2(dev, "mxt surface compare part %d %d compare K(%d,%d) diff %d surface0 %d %d thrd %d\n",sf,compare,i,j,diff,c0_buf[pos],c1_buf[pos],diff_threshold_t15);
					if (diff_out)
						*diff_out = diff;
					return -EINVAL;
				}
			} else {
				dev_dbg2(dev, "X(%d,%d) %d is %d v %d \n",i,j,pos,c0_buf[pos],c1_buf[pos]);
			}
		}
	}

	dev_dbg2(dev, "surface %d %d compare (%d,%d)-(%d,%d) successful\n",sf,compare,from->x,from->y,to->x,to->y);

	if (diff_out)
		*diff_out = 0;

	return 0;
}

static int mxt_surface_data_check_part(struct plugin_cal *p, const struct point *from, const struct point *to, int sf, int max, int min)
{
	struct device *dev = p->dcfg->dev;
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	const struct rect *surface,*area_t,*area_k;
	int i,j,pos,y_size;
	s16 *c0_buf;

	dev_dbg2(dev,"mxt check surface %d (%d,%d)~(%d,%d)\n",
		sf,from->x,from->y,to->x,to->y);

	surface = &dcfg->m[MX_AA];
	area_t = &dcfg->m[MX_T];
	area_k = &dcfg->m[MX_K];

	y_size = surface->y1 - surface->y0 + 1;
	c0_buf = obs->buf[sf];

	for (i = from->x; i <= to->x; i++) {
		for (j = 0; j < y_size; j++) {

			if (j == 0 && i == from->x) {//first element
				j += from->y;
			}

			if (i == to->x) {//last element
				if (j >= to->y)
					break;
			}

			pos = i * y_size + j;

			if ((i >= area_t->x0) && (i <= area_t->x1)&&
					(j >= area_t->y0)&&(j <= area_t->y1)) {

				if (c0_buf[pos] > max || c0_buf[pos] < min) {
					dev_err(dev, "mxt surface %d check part T(%d,%d) val %d (%d,%d)\n",
							sf,i,j,c0_buf[pos],max,min);
					return -EINVAL;
				}
			} else if ((i >= area_k->x0) && (i <= area_k->x1)&&
					(j >= area_k->y0)&&(j <= area_k->y1)) {

				if (c0_buf[pos] > max || c0_buf[pos] < min) {
					dev_err(dev, "mxt surface %d check part K(%d,%d) val %d (%d,%d)\n",
							sf,i,j,c0_buf[pos],max,min);
					return -EINVAL;
				}
			} else {
				dev_dbg2(dev, "X(%d,%d) %d is %d \n",i,j,pos,c0_buf[pos]);
			}
		}
	}

	return 0;
}

static int mxt_surface_area_check_max(struct plugin_cal *p, const struct rect *a, int sf, s16 *out_max, s16 *out_min, s16 * out_abs_max, s16 thld, int * out_nodes,unsigned long flag)
{
	struct device *dev = p->dcfg->dev;
	const struct mxt_config *dcfg = p->dcfg;
	struct t37_observer *obs = p->obs;
	const struct rect *surface,*area_t,*area_k;
	struct rect area_c;
	int i,j,pos,y_size;
	s16 *c0_buf,max,min,abs_max;
	int nodes;

	dev_dbg2(dev, "mxt check surface %d max/min (%d,%d)~(%d,%d)\n",
		sf,a->x0,a->y0,a->x1,a->y1);

	surface = &dcfg->m[MX_AA];
	area_t = &dcfg->m[MX_T];
	area_k = &dcfg->m[MX_K];
	memcpy(&area_c, a, sizeof(area_c));

	y_size = surface->y1 - surface->y0 + 1;
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area_c.x1 == surface->x1)
			area_c.x1--;
		//surface.x1--;
		//x_size--;
	}

	c0_buf = obs->buf[sf];

	max = abs_max = 0;
	min = -1;
	nodes = 0;
	for (i = area_c.x0; i <= area_c.x1; i++) {
		for (j = 0; j < y_size; j++) {

			if (j == 0 && i == area_c.x0) {//first element
				j += area_c.y0;
			}

			if (i == area_c.x1) {//last element
				if (j >= area_c.y1)
					break;
			}

			pos = i * y_size + j;

			if ((i >= area_t->x0) && (i <= area_t->x1)&&
					(j >= area_t->y0)&&(j <= area_t->y1)) {

				if (c0_buf[pos] > max)
					max = c0_buf[pos];
				if (c0_buf[pos] < min)
					min = c0_buf[pos];
				if (abs(c0_buf[pos]) > abs_max)
					abs_max = abs(c0_buf[pos]);

				if (abs(c0_buf[pos]) >= thld)
					nodes++;
			} else if ((i >= area_k->x0) && (i <= area_k->x1)&&
					(j >= area_k->y0)&&(j <= area_k->y1)) {

				if (c0_buf[pos] > max)
					max = c0_buf[pos];
				if (c0_buf[pos] < min)
					min = c0_buf[pos];
				if (abs(c0_buf[pos]) > abs_max)
					abs_max = abs(c0_buf[pos]);

			} else {
				dev_dbg2(dev, "X(%d,%d) %d is %d \n",i,j,pos,c0_buf[pos]);
			}
		}
	}

	if (out_max)
		*out_max = max;
	if (out_min)
		*out_min = min;
	if (out_abs_max)
		*out_abs_max = abs_max;
	if (out_nodes)
		*out_nodes = nodes;
	return 0;
}


static int mxt_surface_aquire_and_compare(struct plugin_cal *p, int sf, int compare,s16 diff_threshold_t9,s16 diff_threshold_t15,int no_wait, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct point *sf_info,new_sf_info;
	struct rect surface, area, range;
	s8 command;
	s16 *buf;
	int pos,x_size,y_size,data_max,data_min;
	int ret;

	dev_dbg(dev,"mxt surface aquire and_compare %d %d, thld(%d,%d) no_wait %d flag 0x%lx\n",
			sf,compare,diff_threshold_t9,diff_threshold_t15,no_wait,flag);

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_AA], sizeof(struct rect));//here area set as whole AA
	x_size = surface.x1 - surface.x0 + 1;
	y_size = surface.y1 - surface.y0 + 1;

	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
		x_size--;
	}

	sf_info = &obs->sf_info_list[sf];
	if (sf_info->x >= x_size) {
		dev_err(dev, "mxt surface aquire and compare pos error(%d,%d) x_size %d\n",
				sf_info->x,sf_info->y,x_size);

		memset(sf_info,0,sizeof(*sf_info));
	}

	if (sf == T37_REF0) {
		command = MXT_T6_DEBUG_REF;
		data_max = T37_REF_MAX;
		data_min = T37_REF_MIN;
	} else if (sf == T37_DELTA0) {
		command = MXT_T6_DEBUG_DELTA;
		data_max = T37_DELTA_MAX;
		data_min = T37_DELTA_MIN;
	} else {
		dev_err(dev, "mxt surface aquire and compare invlid sf %d\n",
				sf);
		return -EINVAL;
	}
	do {
		pos = sf_info->x * y_size + sf_info->y;
		buf = obs->buf[sf] + pos;
		range.x0 = dcfg->m[MX_POS].x0 + sf_info->x;
		range.x1 = dcfg->m[MX_POS].x0 + area.x1;
		range.y0 = dcfg->m[MX_POS].y0;
		range.y1 = dcfg->m[MX_POS].y0 + area.y1;
		ret = mxt_surface_page_aquire(pl->dev,buf,command,&surface,&range,sf_info->y, &pos, no_wait);
		if (ret < 0) {
			dev_err(dev, "mxt surface aquire and compare a get pos (%d,%d) data failed %d\n",
					sf_info->x,sf_info->y,ret);
		}

		new_sf_info.x = range.x1 - dcfg->m[MX_POS].x0;
		new_sf_info.y = range.y1 - dcfg->m[MX_POS].y0;

		if (new_sf_info.x > sf_info->x ||
				((new_sf_info.x == sf_info->x) && (new_sf_info.y > sf_info->y))) {

			if (!test_flag(T37_SKIP_LIMIT_COMPARE, &flag)) {
				ret = mxt_surface_data_check_part(p,sf_info,&new_sf_info,sf,data_max,data_min);
				if (ret == -EINVAL) {
					dev_err(dev, "mxt surface aquire and compare data invalid %d\n",ret);
					ret = -EAGAIN;
					return ret;
				}
			}
			if (compare < T37_BUFFER_NUMBER) {
				ret = mxt_surface_compare_part(p,sf_info,&new_sf_info,sf,compare,diff_threshold_t9,diff_threshold_t15,NULL,flag);
				if (ret) {
					dev_err(dev, "mxt surface aquire and compare data failed %d\n",ret);
					sf_info->x = new_sf_info.x;
					sf_info->y = new_sf_info.y;
					return ret;
				}
			}

			sf_info->x = new_sf_info.x;
			sf_info->y = new_sf_info.y;
		} else {
			dev_err(dev, "surface %d %d aquire sf(%d,%d)->(%d,%d),result %d\n",sf,compare,sf_info->x,sf_info->y,new_sf_info.x,new_sf_info.y,ret);
			if (ret == 0)
				ret = -EINVAL;
		}
	} while(sf_info->x < x_size && !ret);

	dev_info2(dev, "surface %d %d compare (%d,%d) result %d\n",sf,compare,sf_info->x,sf_info->y,ret);

	return ret;
}

static int mxt_surface_aquire_and_compare_auto(struct plugin_cal *p,int sf,int compare,s16 diff_threshold_t9,s16 diff_threshold_t15,int no_wait, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct point *sf_info,new_sf_info;
	struct rect surface, area, range;
	s8 command_register[2];
	s16 *buf;
	int x_size,y_size,pos,data_max,data_min;
	int ret = 0;

	dev_dbg2(dev,"mxt surface aquire and_compare auto %d %d, thld(%d,%d) no_wait %d flag 0x%lx\n",
		sf,compare,diff_threshold_t9,diff_threshold_t15,no_wait,flag);

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area, &dcfg->m[MX_AA], sizeof(struct rect));//here area set as whole AA
	x_size = surface.x1 - surface.x0 + 1; 
	y_size = surface.y1 - surface.y0 + 1;
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		if (area.x1 == surface.x1)
			area.x1--;
		//surface.x1--;
		x_size--;
	}

	if (sf >= T37_BUFFER_NUMBER) {
		ret = mxt_get_current_page_info(pl->dev, command_register);
		if (ret) {
			dev_err(dev, "mxt surface aquire and compare auto check diag register failed %d\n",ret);
			return ret;
		}
		if (command_register[0] == MXT_T6_DEBUG_REF)
			sf = T37_REF0;
		else
			sf = T37_DELTA0;
		dev_warn(dev, "mxt surface auto aquire sf %d\n",sf);
	} else {
		if (sf == T37_REF0)
			command_register[0] = MXT_T6_DEBUG_REF;
		else if (sf == T37_DELTA0)
			command_register[0] = MXT_T6_DEBUG_DELTA;
		else {
			dev_warn(dev, "mxt surface unkonwn command, sf %d\n",sf);
			ret = -EAGAIN;
			return ret;
		}
	}

	if (command_register[0] == MXT_T6_DEBUG_REF) {
		data_max = T37_REF_MAX;
		data_min = T37_REF_MIN;
	} else {
		data_max = T37_DELTA_MAX;
		data_min = T37_DELTA_MIN;
	}

	sf_info = &obs->sf_info_list[sf];
	if (sf_info->x < x_size) {
		pos = sf_info->x * y_size + sf_info->y;
		buf = obs->buf[sf] + pos;
		range.x0 = dcfg->m[MX_POS].x0 + sf_info->x;
		range.x1 = dcfg->m[MX_POS].x0 + area.x1;
		range.y0 = dcfg->m[MX_POS].y0;
		range.y1 = dcfg->m[MX_POS].y0 + area.y1;
		ret = mxt_surface_page_aquire(pl->dev,buf,command_register[0],&surface, &range, sf_info->y,&pos ,no_wait);
		if (ret < 0) {
			dev_dbg(dev, "mxt surface aquire and compare auto get a ref page data failed %d\n",ret);
			//return ret;
		}

		new_sf_info.x = range.x1 - dcfg->m[MX_POS].x0;
		new_sf_info.y = range.y1 - dcfg->m[MX_POS].y0;

		dev_dbg2(dev, "mxt surface aquire and compare auto page data at pos %d (%d,%d) old(%d,%d) ret %d\n",
				pos,
				new_sf_info.x,
				new_sf_info.y,
				sf_info->x,
				sf_info->y,
				ret);

		if (new_sf_info.x > sf_info->x ||
				((new_sf_info.x == sf_info->x) && (new_sf_info.y > sf_info->y))) {

			if (!test_flag(T37_SKIP_LIMIT_COMPARE, &flag)) {
				ret = mxt_surface_data_check_part(p,sf_info,&new_sf_info,sf,data_max,data_min);
				if (ret == -EINVAL) {
					dev_err(dev, "mxt surface aquire and compare auto data invalid %d\n",ret);
					ret = -EAGAIN;
					return ret;
				}
			}

			if (compare < T37_BUFFER_NUMBER) {
				ret = mxt_surface_compare_part(p,sf_info,&new_sf_info,sf,compare,diff_threshold_t9,diff_threshold_t15,NULL,flag);
				if (ret) {
					dev_err(dev, "mxt surface aquire and compare auto data failed %d\n",ret);
					sf_info->x = new_sf_info.x;
					sf_info->y = new_sf_info.y;
					return ret;
				}
			}

			sf_info->x = new_sf_info.x;
			sf_info->y = new_sf_info.y;
		} else {
			if (ret != -EAGAIN)
				dev_err(dev, "surface %d %d aquire auto sf(%d,%d)->(%d,%d),result %d\n",
					sf,compare,sf_info->x,sf_info->y,new_sf_info.x,new_sf_info.y,ret);

			if (ret == 0)
				ret = -EINVAL;
		}
	}

	dev_dbg2(dev, "surface %d %d compare_auto (%d,%d) result %d\n",sf,compare,sf_info->x,sf_info->y,ret);
	return ret;
}

static int mxt_surface_aquire_reset(struct plugin_cal *p, int sf,int no_wait)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg); 
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct point *sf_info;
	u8 command;

	sf_info = &obs->sf_info_list[sf];

	if (sf == T37_REF0) {
		command = MXT_T6_DEBUG_REF;
	} else if (sf == T37_DELTA0) {
		command = MXT_T6_DEBUG_DELTA;
	} else {
		dev_err(dev, "mxt surface aquire reset invlid sf %d\n",
				sf);
		return -EINVAL;
	}
	memset(sf_info, 0, sizeof(*sf_info));

	return mxt_diagnostic_reset_page(pl->dev, command, no_wait);
}

static void plugin_cal_t37_hook_t6(struct plugin_cal *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_info2(dev, "T37 hook T6 0x%x\n", status);
		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(T37_FLAG_CAL,
					T37_FLAG_TAG_MASK_NORMAL,&obs->flag);
		}
		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(T37_FLAG_RESET,
					T37_FLAG_TAG_MASK_LOW|T37_FLAG_TAG_MASK_NORMAL,&obs->flag);
		}
		set_and_clr_flag(T37_STEP_0_ENTER,
				T37_STEP_MASK,&obs->step);
	} else {
		if (test_flag(T37_STEP_0_IN,&obs->step)) {
			dev_info2(dev, "T37 hook T6 end\n");
			set_and_clr_flag(T37_STEP_1_ENTER,T37_STEP_0_IN,&obs->step);
		}
	}

	dev_info2(dev, "mxt t37 step=0x%lx flag=0x%lx\n",
			obs->step, obs->flag);
}

static void plugin_cal_t37_hook_t9(struct plugin_cal *p, int id, int x, int y, u8 status)
{
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;
	int distance;

	if (id >= MAX_TRACE_POINTS)
		return;

	if (status & MXT_T9_DETECT) {
		if (!obs->touch_id_list) {
			obs->time_touch_st = obs->time_touch_end = jiffies;
			clear_flag(T37_FLAG_TOUCH_ALL_RELEASED,&obs->flag);
		}

		if (!__test_and_set_bit(id, &obs->touch_id_list)) {
			obs->trace[id].count = 0;
			obs->trace[id].distance.x = 0;
			obs->trace[id].distance.y = 0;
			obs->trace[id].first.x = obs->trace[id].last.x = x;
			obs->trace[id].first.y = obs->trace[id].last.y = y;
			obs->trace[id].time_touch_st = obs->trace[id].time_touch_end = obs->trace[id].time_point_renw = jiffies;
			set_and_clr_flag(T37_FLAG_TOUCH_PRESSED,T37_FLAG_TOUCH_RELEASED,&obs->trace[id].flag);
			set_and_clr_flag(T37_FLAG_TOUCH_PRESSED,T37_FLAG_TOUCH_RELEASED,&obs->flag);
		}

		obs->trace[id].count++;
		obs->trace[id].last.x = x;
		obs->trace[id].last.y = y;

		distance = obs->trace[id].last.x - obs->trace[id].first.x;
		distance = abs(distance);
		if (distance > obs->trace[id].distance.x)
			obs->trace[id].distance.x = distance;

		distance = obs->trace[id].last.y - obs->trace[id].first.y;
		distance = abs(distance);
		if (distance > obs->trace[id].distance.y)
			obs->trace[id].distance.y = distance;

		set_flag(T37_FLAG_TOUCH_MOV_PENDING,&obs->trace[id].flag);
		if (time_after_eq(jiffies, obs->trace[id].time_point_renw + cfg->interval_distance_check)) {
			obs->trace[id].time_point_renw = jiffies;
			obs->trace[id].first.x = obs->trace[id].last.x;
			obs->trace[id].first.y = obs->trace[id].last.y;
			clear_flag(T37_FLAG_TOUCH_MOV_PENDING,&obs->trace[id].flag);
		}
	} else {
		if (__test_and_clear_bit(id, &obs->touch_id_list)) {
			if (!obs->touch_id_list) {
				obs->time_touch_end = jiffies;
				set_flag(T37_FLAG_TOUCH_ALL_RELEASED,&obs->flag);
			}
			obs->trace[id].time_touch_end = jiffies;
			set_and_clr_flag(T37_FLAG_TOUCH_RELEASED,T37_FLAG_TOUCH_PRESSED,&obs->trace[id].flag);
			set_and_clr_flag(T37_FLAG_TOUCH_RELEASED,T37_FLAG_TOUCH_PRESSED,&obs->flag);
			clear_flag(T37_FLAG_TOUCH_MOV_PENDING,&obs->trace[id].flag);
		}
	}
}

static void plugin_cal_t37_hook_t100(struct plugin_cal *p, int id, int x, int y, u8 status)
{
	plugin_cal_t37_hook_t9(p, id, x, y, (status & MXT_T100_DETECT) ? MXT_T9_DETECT : 0);
}

static void plugin_cal_t37_reset_slots_hook(struct plugin_cal *p)
{
	struct t37_observer *obs = p->obs;

	obs->touch_id_list = 0;
	memset(&obs->trace, 0, sizeof(obs->trace));
}

static int plugin_cal_t37_check_and_calibrate(struct plugin_cal *p, bool check_sf, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	struct device *dev = dcfg->dev;
	const struct plug_config *pl_cfg = &pl->config;
	struct t37_config *cfg = p->cfg;
	struct t37_observer * obs = p->obs;
	s16 thld_t9,thld_t15;
	int sf,state;
	unsigned long flag;
	int ret = -EACCES;

	if (check_sf) {
		sf = T37_DELTA0;
		mxt_surface_aquire_reset(p,sf,0);
		state = get_t9_t100_state(obs->step, 0, pl_flag);
		flag = get_gradient_flag(obs->flag,pl_flag);
		flag |= T37_COMPARE_DIRECTION_BOTH;
		thld_t9 = caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold,cfg->delta_low_limit_ratio);
		thld_t15 = caculate_threshlod(dcfg->t15.threshold,cfg->delta_low_limit_ratio);
		ret = mxt_surface_aquire_and_compare(p, sf, sf, thld_t9,thld_t15,0,flag);
		if (ret == 0)
			dev_info2(dev, "mxt t37 check and calibration, peaceful\n");
		else {
			//double check
#if defined(CONFIG_MXT_CHECK_AND_CAL_DOUBLE_CHECK)
			mxt_surface_aquire_reset(p,sf,0);
			ret = mxt_surface_aquire_and_compare(p, sf, sf, thld_t9,thld_t15,0,T37_COMPARE_DIRECTION_BOTH);
			if (ret == 0) {
				dev_info2(dev, "mxt t37 check and calibration, peaceful both 2\n");
			}
#endif
		}
	}

	if (ret) {
		dev_info(dev, "mxt t37 check and calibration, calibratting pl_flag=0x%lx\n",pl_flag);

		state = get_t8_state(T37_STEP_0_ENTER, 0, pl_flag, 0, 0);
		p->set_t8_cfg(p->dev, state, 0);
		if (state >= T8_WEAK_PALM || state == T8_HALT)
			p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
		else
		p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);

		p->set_t6_cal(p->dev);
	}

	return 0;
}

static void plugin_cal_t37_start(struct plugin_cal *p, bool resume)
{
	struct t37_observer *obs = p->obs;

	clear_flag(T37_FLAG_WORKAROUND_HALT, &obs->flag);

	if (resume)
		set_flag(T37_FLAG_RESUME, &obs->flag);
}

static void plugin_cal_t37_stop(struct plugin_cal *p)
{
	struct t37_observer *obs = p->obs;

	set_and_clr_flag(T37_FLAG_WORKAROUND_HALT,T37_FLAG_RESUME, &obs->flag);
}


static int mxt_proc_step_0_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	#if (DBG_LEVEL > 1)
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	dev_info2(dev, "mxt t37 at mxt_proc_step_0_msg step 0x%lx flag 0x%lx pl_flag 0x%lx\n",obs->step,obs->flag,pl_flag);
	#endif
	return 0;
}

static int mxt_proc_step_1_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	unsigned long  flag;
	#if (DBG_LEVEL > 1)
	dev_info2(dev, "mxt t37 at mxt_proc_step_1_msg flag 0x%lx\n",obs->flag);
	#endif
	flag = obs->flag;

	if (test_and_clear_flag(T37_FLAG_RESUME|T37_FLAG_RESET|T37_FLAG_CAL, 
				&obs->flag)) {
		//reset state machine
		plugin_cal_t37_reset_slots_hook(p);
		if (test_flag(T37_FLAG_CAL, &flag)) {
			dev_info(dev, "mxt t37 cal msg\n");
			proc_cal_msg(p);
			mxt_surface_aquire_reset(p,T37_REF0,1);
		}
		if (test_flag(T37_FLAG_RESUME, &flag)||
				test_flag(PL_STATUS_FLAG_NOISE_CHANGE, &pl_flag)) {
			dev_info(dev, "mxt t37 resume msg\n");
			proc_resume_msg(p);
		}

		if (test_flag(T37_FLAG_RESET, &flag)) {
			dev_info(dev, "mxt t37 reset msg\n");
			proc_reset_msg(p);
		}
		//clear last step
		clear_flag(T37_STEP_MASK, &obs->step_cmd);

		mxt_process_obj_recovery(p,pl_flag);
	}

	if (test_flag(T37_STEP_MASK, &obs->step_cmd)) {
		#if (DBG_LEVEL > 1)
		dev_info2(dev, "mxt t37 found command 0x%lx\n",obs->step_cmd);
		#endif
		set_and_clr_flag(obs->step_cmd, T37_STEP_MASK, &obs->step);
		clear_flag(T37_STEP_MASK, &obs->step_cmd);
	} else {
		set_and_clr_flag(T37_STEP_2_ENTER,T37_STEP_1_IN, &obs->step);
	}

	p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_NOISE_CHANGE|PL_STATUS_FLAG_CAL_END);

	return 0;
}

static int mxt_proc_step_2_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;
	struct stat_grad_config *grad_level_list;
	struct grad_cache_type grad_cache;
	int i,j,id,points,count;
	struct point *sf_info;
	struct surface_count *sf_cnt;
	struct rect surface;
	unsigned long  state,state_wait,state_not,flag,interval;
	int sf, accumulator, x_size, y_size, punish;
	bool wait;
	int ret = -EINVAL;

	dev_dbg2(dev, "mxt t37 at mxt_proc_step_2_msg step 0x%lx\n",obs->step);

	//gradient check
	if (test_flag(T37_STEP_2_ENTER, &obs->step)) {
		set_and_clr_flag(T37_STEP_2_CHECK_REF_1,T37_STEP_2_IN, &obs->step);
		obs->time_gradient_check_st = jiffies;
	}

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	x_size = surface.x1 - surface.x0 + 1;
	y_size = surface.y1 - surface.y0 + 1;

	flag = get_gradient_flag(obs->flag,pl_flag);
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		/*
		if (area.x1 == surface.x1)
			area.x1--;
		surface.x1--;
		*/
		x_size--;
	}

	if (test_flag(T37_STEP_2_CHECK_REF_1, &obs->step)) {
		sf = T37_REF0;
		sf_info = &obs->sf_info_list[sf];
		sf_cnt = &obs->sf_cnt[sf];

		if (sf_info->x >= x_size)
			mxt_surface_aquire_reset(p,sf,0);

		if (sf_info->x < x_size) {
			if (sf_cnt->retry > cfg->surface_retry_times)
				flag |= T37_SKIP_LIMIT_COMPARE;

			ret = mxt_surface_aquire_and_compare_auto(p,sf,T37_BUFFER_INVALID,0,0,1,flag);
			if (ret == 0) {
				if (sf_info->x < x_size) {
					dev_dbg2(dev, "step_2_msg read ref %d, current x is %d\n", 
							x_size,sf_info->x);
					ret = -EAGAIN;
				} else
					sf_cnt->count++;
				sf_cnt->retry = 0;
			} else if (ret == -EAGAIN) {
				sf_cnt->retry++;
				dev_dbg(dev, "mxt sf %d (good %d pend %d bad %d retry %d)\n",
					sf, sf_cnt->good,sf_cnt->pend,sf_cnt->bad,sf_cnt->retry);
			}
		}

		if (ret == 0) {
			id = get_noise_cache_id(pl_flag);
			memcpy(&grad_cache, &obs->grad_cache[id], sizeof(struct grad_cache_type));
			#if (DBG_LEVEL > 1)
			dev_info2(dev, "step_2_msg grad trace (cnt %d, bad %d, good %d)\n",
				obs->gradient[GRAD_TRACE_ALL].count,
				obs->gradient[GRAD_TRACE_ALL].bad,
				obs->gradient[GRAD_TRACE_ALL].good);
			#endif
			if (obs->gradient[GRAD_TRACE_ALL].bad < cfg->num_grad_level_array) {
				grad_level_list = cfg->grad_level_array[obs->gradient[GRAD_TRACE_ALL].bad];
				flag = get_gradient_flag(obs->flag,pl_flag);
				flag |= cfg->grad_acc_flag/*|ACCUMULATOR_GRAD_INVALUE*/;
				if (obs->gradient[GRAD_TRACE_ALL].good >= cfg->step2_grad_ref_check1_count)
					flag |=ACCUMULATOR_GRAD_DRIFT_NORM;
				accumulator = mxt_surface_gradient_statistics(p, pl_flag, T37_REF0, T37_TRANS, T37_DELTA0, T37_GRAD0_X, T37_GRAD0_Y, T37_GRAD0_K, grad_level_list, NUM_GRAD_LEVEL_LIST, &grad_cache, GRAD_DRIFT_NORM, flag);
				if (accumulator >= cfg->grad_accumulator_high) {
					obs->gradient[GRAD_TRACE_ALL].bad++;
					ret = -EINVAL;
				}
				obs->accumulator_ref = accumulator;
				obs->gradient[GRAD_TRACE_ALL].count++;
				memset(&obs->step2_wait_state,0,sizeof(obs->step2_wait_state));
				if (obs->gradient[GRAD_TRACE_ALL].count >= cfg->step2_grad_ref_check1_count) {
					if (ret == 0) {
						obs->gradient[GRAD_TRACE_ALL].good++;
						//obs->gradient[GRAD_TRACE_ALL].bad = 0;

						if (check_gradient_block_limit(p, 0, cfg->grad_block_high_limit_list, cfg->grad_acc_flag)) {
							dev_info2(dev, "step_2_msg gradient blocks over limit\n");
							obs->gradient[GRAD_TRACE_ALL].bad++;
							if(!test_flag(PL_STATUS_FLAG_POWERUP,&pl_flag) ||
								(obs->gradient[GRAD_TRACE_ALL].count < cfg->step2_grad_ref_check2_count)){
								obs->gradient[GRAD_TRACE_ALL].bad++;
							ret = -EBUSY;
							}
						}else
							set_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH, &obs->flag);
						}

					if(ret){
							dev_info(dev, "step_2_msg gradient statistics failed %d\n", obs->gradient[GRAD_TRACE_ALL].bad);
							obs->gradient[GRAD_TRACE_ALL].bad++;
							set_and_clr_flag(T37_STEP_2_CHECK_REF_2,T37_STEP_2_IN, &obs->step);


								set_and_clr_flag(T37_STATE_TOUCH_RELEASED,T37_STATE_MASK, &obs->step2_wait_state[0][GRAD_STATE_WAIT]);

								set_and_clr_flag(T37_STATE_TOUCH_PRESSED,T37_STATE_MASK, &obs->step2_wait_state[1][GRAD_STATE_WAIT]);
								set_and_clr_flag(T37_STATE_TOUCH_MOVING|T37_STATE_TOUCH_MOV_PENDING,T37_STATE_MASK, &obs->step2_wait_state[1][GRAD_STATE_NOT]);
								//
					}
				} else {
					dev_info(dev, "step_2_msg gradient first time(%d), wait release calibration\n", obs->gradient[GRAD_TRACE_ALL].count);
					set_and_clr_flag(T37_STEP_2_CHECK_REF_2,T37_STEP_2_IN, &obs->step);
					set_and_clr_flag(T37_STATE_TOUCH_RELEASED,T37_STATE_MASK, &obs->step2_wait_state[0][GRAD_STATE_WAIT]);

					clear_trace_point(&obs->trace[0],MAX_TRACE_POINTS,FLAG_TRACE_POINT_MASK);

					merge_matrix(obs->buf[T37_IRON],obs->buf[T37_REF0], NULL,
							x_size,y_size,SURFACE_MERGE_COPY);

					ret = -EBUSY;
				}
			} else {
				dev_info(dev, "step_2_msg gradient statistics all failed (%d,%d)\n", obs->gradient[GRAD_TRACE_ALL].bad,cfg->num_grad_level_array);
				set_and_clr_flag(T37_STEP_2_CHECK_SINGLE_TOUCH,T37_STEP_2_IN, &obs->step);
				clear_grad_record(obs, GRAD_TRACE_NUMBER, false);
				ret = -EINVAL;
			}
		} else if (ret == -EAGAIN) {
			dev_dbg2(dev, "step_2_msg aquire ref again\n");
		} else if (ret == -EINVAL) {
			dev_err(dev, "step_2_msg aquire ref failed\n");
		}
	}

	if (test_flag(T37_STEP_2_CHECK_REF_2, &obs->step)) {
		if (ret && ret != -EAGAIN) {
			for (i = 0; i < cfg->step2_check_point_count; i++) {
				state = check_touched_and_moving(&obs->trace[i], 1, &cfg->movement[MOVEMENT_SHORT],&points,&count,&interval);
				if (!test_flag(T37_STATE_TOUCH_MOV_PENDING, &state))
					clear_trace_point(&obs->trace[i],1,FLAG_TRACE_POINT_DISTANCE);

				wait = false;
				for (j = 0; j < GRAD_STATE_LIST; j++) {
					state_wait = obs->step2_wait_state[j][GRAD_STATE_WAIT];
					state_not = obs->step2_wait_state[j][GRAD_STATE_NOT];

					dev_dbg2(dev, "step_2_msg check point %d - %d :state 0x%lx wait 0x%lx not 0x%lx\n",
							i,j,state,state_wait,state_not);

					if (state_wait || state_not) {
						wait = true;
						break;
					}
				}

				#if (DBG_LEVEL > 1)
				if (state)
					dev_info2(dev, "step_2_msg ref check state 0x%lx, id %d count %d interval %ld wait %d\n",
							state,i,count,interval,wait);
				#endif
				if (wait) {
					for (j = 0; j < GRAD_STATE_LIST; j++) {
						state_wait = obs->step2_wait_state[j][GRAD_STATE_WAIT];
						state_not = obs->step2_wait_state[j][GRAD_STATE_NOT];
						if (!state_wait && !state_not)
							continue;
						#if (DBG_LEVEL > 1)
						if (state || (state_not && (state & state_not)))
							dev_info2(dev, "step_2_msg state wait point %d - %d :state 0x%lx, wait 0x%lx, not 0x%lx\n",
									i,j,state,state_wait,state_not);
						#endif
						if ((!state_wait|| test_flag(state_wait, &state)) &&
								(!test_flag(state_not, &state))) {
							obs->gradient[GRAD_TRACE_TOUCH_RELEASE].count++;
							punish = obs->gradient[GRAD_TRACE_ALL].bad << 1;
							punish = 0;
							if (punish >= MAX_TRACE_POINTS - i)
								punish = 0;
							obs->gradient[GRAD_TRACE_TOUCH_RELEASE].bad += (1 << (MAX_TRACE_POINTS - i - punish));
							dev_info2(dev, "step_2_msg trace state bad %d\n",obs->gradient[GRAD_TRACE_TOUCH_RELEASE].bad);
							if (obs->gradient[GRAD_TRACE_TOUCH_RELEASE].bad >= (1<<MAX_TRACE_POINTS)) {
								dev_info2(dev, "step_2_msg wait state set cal\n");
								set_and_clr_flag(T37_STEP_2_CHECK_REF_1,T37_STEP_2_IN, &obs->step);
								p->set_t6_cal(p->dev);
								break;
							}
						}
					}
					if (j != GRAD_STATE_LIST)
						break;
				} else {
					dev_info2(dev, "step_2_msg no wait set cal\n");
					set_and_clr_flag(T37_STEP_2_CHECK_REF_1,T37_STEP_2_IN, &obs->step);
					p->set_t6_cal(p->dev);
					break;
				}
			}
		}
	}

	//single touch check
	if (test_flag(T37_STEP_2_CHECK_SINGLE_TOUCH, &obs->step)) {
		#if (DBG_LEVEL > 1)
		dev_info2(dev, "step_2_msg check single touch");
		#endif
		for (i = 0; i < cfg->step2_check_point_count; i++) {
			state = check_touched_and_moving(&obs->trace[i], 1, &cfg->movement[MOVEMENT_LONG],&points,&count,&interval);
			#if (DBG_LEVEL > 1)
			if (state)
				dev_info2(dev, "step_2_msg point %d interval %ld count %d distance(%d,%d) state 0x%ld\n",i,interval,count,
						obs->trace[i].distance.x,
						obs->trace[i].distance.y,
						state);
			#endif
			if (!test_flag(T37_STATE_TOUCH_MOV_PENDING, &state))
				clear_trace_point(&obs->trace[i],1,FLAG_TRACE_POINT_DISTANCE);
			if ((i == 0) && test_flag(T37_STATE_TOUCH_RELEASED, &state) &&
					test_flag(T37_STATE_TOUCH_MOVING, &state)) {//moving released
				obs->gradient[GRAD_TRACE_SINGLE_TOUCH].count++;
				if (count >= cfg->step2_single_count) {
					obs->gradient[GRAD_TRACE_SINGLE_TOUCH].good++;
					dev_info2(dev, "step_2_msg check single touch count %d, check %d",
							obs->gradient[GRAD_TRACE_SINGLE_TOUCH].good,
							cfg->step2_single_touch_check_count);
					if (obs->gradient[GRAD_TRACE_SINGLE_TOUCH].good >= cfg->step2_single_touch_check_count) {
						dev_info2(dev, "step_2_msg check single touch passed");
						ret = 0;
					} else {
						ret = -EINVAL;
					}
				}
				obs->gradient[GRAD_TRACE_SINGLE_TOUCH].bad = 0;
			} else if (test_flag(T37_STATE_TOUCH_PRESSED, &state)) {
				if (interval > (cfg->interval_step2_pressed_long >> i)) {
					obs->gradient[GRAD_TRACE_SINGLE_TOUCH].bad += (1 << (MAX_TRACE_POINTS - i));
					clear_trace_point(&obs->trace[i],1,FLAG_TRACE_POINT_TOUCH_ST_END);
					dev_info2(dev, "step_2_msg trace single touch bad %d\n",obs->gradient[GRAD_TRACE_SINGLE_TOUCH].bad);
					if (obs->gradient[GRAD_TRACE_SINGLE_TOUCH].bad >= (1<<MAX_TRACE_POINTS)) {
						dev_info2(dev, "step_2_msg single touch set cal\n");
						p->set_t6_cal(p->dev);
						break;
					}
				}
			}
		}
	}

	if (ret == 0) {
		dev_info2(dev, "mxt t37 step 2 end\n");
		set_and_clr_flag(T37_STEP_3_ENTER,T37_STEP_2_IN, &obs->step);
		clear_grad_record(obs, GRAD_TRACE_NUMBER, false);
		mxt_process_obj_recovery(p,pl_flag);
	} else {
		if (test_flag(T37_STEP_2_CHECK_REF_2|T37_STEP_2_CHECK_SINGLE_TOUCH, &obs->step)) {
			if (!test_flag(PL_STATUS_FLAG_T8_SET, &pl_flag)) {
				state = get_t8_state(obs->step, obs->flag, pl_flag, obs->gradient[GRAD_TRACE_ALL].bad, 0);
				p->set_t8_cfg(p->dev, state, 0);
				if (state >= T8_WEAK_PALM || state == T8_HALT)
					p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
				else
				p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);
			}
		}
		set_and_clr_flag(obs->step, T37_STEP_MASK, &obs->step_cmd);
	}
	return ret;
}

static int mxt_proc_step_3_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	struct device *dev = dcfg->dev;
	const struct plug_config *pl_cfg = &pl->config;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;
	struct stat_grad_config *grad_level_list_low,*grad_level_list_high;
	struct grad_cache_type grad_cache;
	struct rect surface,surface_t,area_t,area_k;
	struct point sf_from,*sf_info,*delta_info,*ref_info;
	struct surface_count *sf_cnt,*delta_cnt,*ref_cnt;
	enum{TINY=0, LOW, MID, HIGH_POS, HIGH_NEG, HIGHEST, DELTA, NUM_SF_CHECK};
	s16 thld_t9[NUM_SF_CHECK],thld_t15[NUM_SF_CHECK],diff[NUM_SF_CHECK];
	int result[NUM_SF_CHECK];
	int id,state,count,node,sf,sf_delta,sf_ref,drift,ref_diff,accumulator,accumulator0,accumulator1,x_size,y_size,step_t8,pend_times,points;
	unsigned long flag,state_move,interval;
	bool enable_t8 = false;
	int ret = 0;

	dev_dbg2(dev, "mxt t37 at mxt_proc_step_3_msg\n");

	memset(&sf_from, 0, sizeof(sf_from));

	sf_delta  = T37_DELTA0;
	delta_info = &obs->sf_info_list[sf_delta];
	delta_cnt = &obs->sf_cnt[sf_delta];

	sf_ref = T37_REF0;
	ref_info = &obs->sf_info_list[sf_ref];
	ref_cnt = &obs->sf_cnt[sf_ref];

	if (test_flag(T37_STEP_3_ENTER, &obs->step)) {
		set_and_clr_flag(T37_STEP_3_CHECK_DELTA,T37_STEP_3_IN, &obs->step);
		obs->time_delta_check_st = jiffies;
		delta_cnt->count = 0;
		if (ref_cnt->count)
			ref_cnt->count = 1;
		clear_trace_point(&obs->trace[0],MAX_TRACE_POINTS,FLAG_TRACE_POINT_MASK);
	}

	if (test_flag(T37_STEP_3_AUTO_SLEEP,&obs->step)) {
		if (obs->touch_id_list) {
			delta_cnt->sleep = 0;
			ref_cnt->sleep = 0;
			clear_flag(T37_STEP_3_AUTO_SLEEP,&obs->step);
		}
	}else {
		if (delta_cnt->sleep >= cfg->step3_sleep_count &&
			ref_cnt->sleep >= cfg->step3_sleep_count)
			set_flag(T37_STEP_3_AUTO_SLEEP,&obs->step);
	}

	state_move = check_touched_and_moving(&obs->trace[0], MAX_TRACE_POINTS, &cfg->movement[MOVEMENT_MIDDLE],&points,NULL,NULL);
	if (!test_flag(T37_STATE_TOUCH_MOV_PENDING, &state_move))
		clear_trace_point(&obs->trace[0],MAX_TRACE_POINTS,FLAG_TRACE_POINT_DISTANCE);
	if (!test_flag(T37_STEP_3_CHECK_DELTA_PENDING,&obs->step)) {
		obs->time_delta_check_st_2 = jiffies;
		if (points == 1 && test_flag(/*T37_STATE_TOUCH_RELEASED*/T37_STATE_TOUCH_MOVING, &state_move)) {
			dev_info2(dev, "step_3_msg set T37_STEP_3_CHECK_DELTA_PENDING\n");
			set_flag(T37_STEP_3_CHECK_DELTA_PENDING,&obs->step);
		}
	}

	if (delta_cnt->good > cfg->delta_good_times)
		step_t8 = delta_cnt->good - cfg->delta_good_times;
	else
		step_t8 = 0;

	if (step_t8) {
		step_t8 >>= cfg->delta_repeat_times_shift;
		step_t8++;
	}
	if (test_flag(PL_STATUS_FLAG_T8_SET, &pl_flag)) {//recovery t8 to last value if set
		state = get_t8_state(obs->step, obs->flag, pl_flag, step_t8, pl->config.num_t8_config);
			p->set_t8_cfg(p->dev, state, 0);
		if (state >= T8_WEAK_PALM || state == T8_HALT)
			p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
		else
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);
	}

	memcpy(&surface, &dcfg->m[MX_AA], sizeof(struct rect));
	memcpy(&area_t, &dcfg->m[MX_T], sizeof(struct rect));
	memcpy(&area_k, &dcfg->m[MX_K], sizeof(struct rect));
	x_size = surface.x1 - surface.x0 + 1;
	y_size = surface.y1 - surface.y0 + 1;

	flag = get_gradient_flag(obs->flag,pl_flag);
	if (test_flag(ACCUMULATOR_GRAD_DUALX, &flag)) {
		/*
		if (area.x1 == surface.x1)
			area.x1--;
		surface.x1--;
		*/
		x_size--;
	}

	id = get_noise_cache_id(pl_flag);
	memcpy(&grad_cache, &obs->grad_cache[id], sizeof(struct grad_cache_type));
	if (test_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH, &obs->flag)) {
		grad_level_list_low = cfg->grad_level_array[(cfg->num_grad_level_array * 3) >> 2];
		grad_level_list_high = cfg->grad_level_array[cfg->num_grad_level_array >> 2];
	} else {
		grad_level_list_high = grad_level_list_low = cfg->grad_level_array[(cfg->num_grad_level_array * 3) >> 2];
	}
	if (delta_cnt->count <=  ref_cnt->count) {
		sf = sf_delta;
		sf_info = delta_info;
		sf_cnt = delta_cnt;
	} else {
		sf = sf_ref;
		sf_info = ref_info;
		sf_cnt = ref_cnt;
	}

	if (sf_info->x >= x_size)
		mxt_surface_aquire_reset(p,sf,0);

	if (sf_info->x < x_size) {
		if (sf_cnt->retry > cfg->surface_retry_times)
			flag |= T37_SKIP_LIMIT_COMPARE;

		ret = mxt_surface_aquire_and_compare_auto(p,sf,T37_BUFFER_INVALID,0,0,1,flag);
		if (ret == -EAGAIN) {
			sf_cnt->retry++;
			dev_dbg(dev, "mxt sf %d (good %d pend %d bad %d retry %d)\n",
					sf, sf_cnt->good,sf_cnt->pend,sf_cnt->bad,sf_cnt->retry);
		} else
			sf_cnt->retry = 0;
	}

	if (sf_info->x >= x_size)
		sf_cnt->count++;

	if (ret == 0) {
		do {
			if (sf_info->x < x_size)
				break;

			if (sf == sf_delta) {
				//compare low level
				state = get_t9_t100_state(obs->step, 0, pl_flag);
				thld_t9[TINY] = caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_tiny_limit_ratio);
				thld_t9[LOW] = caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_low_limit_ratio);
				thld_t9[MID] = caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_mid_limit_ratio);
				thld_t9[HIGH_POS] = thld_t9[HIGH_NEG] =
					caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_high_limit_ratio);
				thld_t9[HIGHEST] =
					caculate_threshlod(pl_cfg->t9_t100_cfg[state].threshold, cfg->delta_highest_limit_ratio);
				mxt_surface_area_check_max(p, &area_t, T37_DELTA0, NULL, NULL, &thld_t9[DELTA], thld_t9[HIGH_POS], &node, flag);
				thld_t9[DELTA] =
					caculate_percent(thld_t9[DELTA], cfg->delta_stable_limit_ratio);

				thld_t15[TINY] = caculate_threshlod(dcfg->t15.threshold,cfg->delta_tiny_limit_ratio);
				thld_t15[LOW] = caculate_threshlod(dcfg->t15.threshold,cfg->delta_low_limit_ratio);
				thld_t15[MID] = caculate_threshlod(dcfg->t15.threshold,cfg->delta_mid_limit_ratio);
				thld_t15[HIGH_POS] = thld_t15[HIGH_NEG] =
					caculate_threshlod(dcfg->t15.threshold,cfg->delta_high_limit_ratio);
				thld_t15[HIGHEST] = caculate_threshlod(dcfg->t15.threshold,cfg->delta_highest_limit_ratio);
				mxt_surface_area_check_max(p, &area_k, T37_DELTA0, NULL, NULL, &thld_t15[DELTA], 0, NULL, flag);
				thld_t15[DELTA] =
					caculate_percent(thld_t15[DELTA], cfg->delta_stable_limit_ratio);

				result[TINY] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[TINY], thld_t15[TINY], &diff[TINY], T37_COMPARE_DIRECTION_BOTH);
				result[LOW] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[LOW], thld_t15[LOW], &diff[LOW], T37_COMPARE_DIRECTION_BOTH);
				result[MID] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[MID], thld_t15[MID], &diff[MID],T37_COMPARE_DIRECTION_BOTH);
				result[HIGH_POS] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[HIGH_POS], thld_t15[HIGH_POS], &diff[HIGH_POS],T37_COMPARE_DIRECTION_POS);
				result[HIGH_NEG] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[HIGH_NEG], thld_t15[HIGH_NEG], &diff[HIGH_NEG],T37_COMPARE_DIRECTION_NEG);
				result[HIGHEST] = mxt_surface_compare_part(p, &sf_from, sf_info, sf, sf, thld_t9[HIGHEST], thld_t15[HIGHEST], &diff[HIGHEST],T37_COMPARE_DIRECTION_POS);
				result[DELTA] = mxt_surface_compare_part(p, &sf_from, sf_info, T37_DELTA0, T37_DELTA1, thld_t9[DELTA], thld_t15[DELTA], &diff[DELTA],T37_COMPARE_DIRECTION_BOTH);

				merge_matrix(obs->buf[T37_DELTA1],obs->buf[T37_DELTA0],NULL,
						x_size,y_size,SURFACE_MERGE_COPY);
				#if (DBG_LEVEL > 0)
				dev_info2(dev, "step_3_msg touch 0x%lx state (%d %lx) node %d thld(%d,%d,%d,%d,%d,%d,%d)compare result(%d,%d,%d,%d,%d,%d,%d) diff(%d,%d,%d,%d,%d,%d,%d)\n",
					obs->touch_id_list,state,state_move,node,
					thld_t9[TINY],thld_t9[LOW],thld_t9[MID],thld_t9[HIGH_POS],thld_t9[HIGH_NEG],thld_t9[HIGHEST],thld_t9[DELTA],
					result[TINY],result[LOW],result[MID],result[HIGH_POS],result[HIGH_NEG],result[HIGHEST],result[DELTA],
					diff[TINY],diff[LOW],diff[MID],diff[HIGH_POS],diff[HIGH_NEG],diff[HIGHEST],diff[DELTA]);

				dev_dbg2(dev, "mxt t8 step %d %d delta(good %d pend %d bad %d retry %d) ref(%d ,%d , %d, %d)\n",
						step_t8,result[LOW],
						delta_cnt->good,delta_cnt->pend,delta_cnt->bad,delta_cnt->retry,
						ref_cnt->good,ref_cnt->pend,ref_cnt->bad,ref_cnt->retry);
				#endif
				if (!result[LOW]) {
					if (!obs->touch_id_list && !test_flag(T37_FLAG_SUPPRESSED, &obs->flag)) {//no touch or touch released
						sf_cnt->bad = 0;
						if (step_t8 +  T8_WEAK_PALM < pl->config.num_t8_config) {
							#if (DBG_LEVEL > 1)
							dev_info2(dev, "mxt t8 state %d step %d\n",state,step_t8);
							#endif
							sf_cnt->good++;
							state = get_t8_state(obs->step, obs->flag, pl_flag, step_t8, pl->config.num_t8_config);
							p->set_t8_cfg(p->dev, state, 0);
							if (state >= T8_WEAK_PALM || state == T8_HALT)
								p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
							else
								p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);
						} else if (step_t8 +  T8_WEAK_PALM == pl->config.num_t8_config) {
							sf_cnt->good++;
							if (!test_flag(T37_STEP_3_CHECK_SIGNAL_MIDDLE, &obs->step)) {
								dev_dbg2(dev, "mxt t8 state halt\n");
								if (!test_flag(T37_STEP_3_CHECK_SIGNAL_MIDDLE, &obs->step)) {
									set_flag(T37_STEP_3_CHECK_SIGNAL_MIDDLE, &obs->step);
									mxt_process_obj_recovery(p,pl_flag);
									merge_matrix(obs->buf[T37_IRON],obs->buf[T37_IRON],obs->buf[T37_REF0],
										x_size,y_size,SURFACE_MERGE_AVG_HYSIS2);
								}
							}
						} else {
							if (!sf_cnt->pend)
								sf_cnt->good++;

							if (test_flag(PL_STATUS_FLAG_POWERUP,&pl_flag))
								interval = cfg->interval_delta_check_end;
							else
								interval = cfg->interval_delta_check_end2;
							if (time_after_eq(jiffies, obs->time_delta_check_st + interval)) {
								//end workaround
								dev_info2(dev, "mxt t37 step 3 end by time\n");
								set_and_clr_flag(T37_STEP_4_ENTER, T37_STEP_3_IN, &obs->step);
								mxt_process_obj_recovery(p,pl_flag);
								break;
							} else {
								dev_info2(dev, "step_3_msg wait %ld to end\n",
										obs->time_delta_check_st + cfg->interval_delta_check_end - jiffies);
							}
						}
					} else {
						if (obs->time_delta_check_st_2) {
							if (time_before(jiffies, obs->time_delta_check_st_2 + cfg->interval_delta_check)) {
								sf_cnt->bad++;
								dev_info2(dev, "step_3_msg check delta is at low set delta bad(%d %d)\n",sf_cnt->bad,cfg->delta_bad_times);
							}
						}
					}

					if (time_after(jiffies, obs->time_delta_check_st_2 + cfg->interval_delta_middle_check)) {
						if (!test_flag(T37_STEP_3_CHECK_SIGNAL_FIRST, &obs->step)) {
							set_flag(T37_STEP_3_CHECK_SIGNAL_FIRST, &obs->step);
							mxt_process_obj_recovery(p,pl_flag);
						}
					}
					if (test_flag(T37_STEP_3_CHECK_SIGNAL_FIRST|T37_STEP_3_CHECK_SIGNAL_MIDDLE, &obs->step))
						sf_cnt->sleep++;
				} else if (result[LOW]) {
					sf_cnt->sleep = 0;
					if (!result[MID] && time_before(jiffies, obs->time_delta_check_st_2 + cfg->interval_delta_check)) {
						sf_cnt->bad++;
						dev_info2(dev, "step_3_msg check delta is at low ~ high level set delta bad (%d max %d)\n",sf_cnt->bad,cfg->delta_bad_times);
					} else {
						if (ref_cnt->count) {
							if (!(test_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH, &obs->flag))) {
								id = T37_TEMP;
								memcpy(&surface_t,&surface,sizeof(surface_t));
								swap(surface_t.x0,surface_t.y0);
								swap(surface_t.x1,surface_t.y1);
								transpose_matrix(obs->buf[id],obs->buf[T37_TRANS],&surface_t);
							} else {
								id = T37_REF0;
							}
							merge_matrix(obs->buf[T37_SIGNAL],obs->buf[id],obs->buf[T37_DELTA0],
									x_size,y_size,SURFACE_MERGE_SUB);
							flag = get_gradient_flag(obs->flag,pl_flag);
							flag |= cfg->grad_acc_flag | ACCUMULATOR_GRAD_K /*| ACCUMULATOR_GRAD_DIFF*/;
							flag |= ACCUMULATOR_GRAD_DRIFT_FAST;
							if (sf_cnt->bad || sf_cnt->pend)
								drift = GRAD_DRIFT_NORM;
							else
								drift = GRAD_DRIFT_SLOW;
							accumulator0 = mxt_surface_gradient_statistics(p, pl_flag, T37_SIGNAL, T37_TRANS, T37_DELTA0, T37_GRAD1_X, T37_GRAD1_Y, T37_GRAD1_K, grad_level_list_high, NUM_GRAD_LEVEL_LIST, &grad_cache, drift, flag);

							flag = get_gradient_flag(obs->flag,pl_flag);
							flag |= cfg->grad_acc_flag | ACCUMULATOR_GRAD_K | ACCUMULATOR_GRAD_DIFF;
							//flag |= ACCUMULATOR_GRAD_DRIFT_NORM;
							accumulator1 = mxt_surface_gradient_statistics(p, pl_flag, T37_SIGNAL, T37_TRANS, T37_DELTA0, T37_GRAD1_X, T37_GRAD1_Y, T37_GRAD1_K, grad_level_list_high, NUM_GRAD_LEVEL_LIST, &grad_cache, GRAD_DRIFT_SLOW, flag);
							accumulator = min_t(int,accumulator0,accumulator1);
							if (accumulator <= obs->accumulator_ref) {
								#if (DBG_LEVEL > 1)
								dev_info2(dev, "step_3_msg accumulator(%d,%d, ref %d %d) touch 0x%lx bad %d pend %d mid %d high_pos %d highest %d\n",
										accumulator0,accumulator1,obs->accumulator_ref,obs->accumulator_ref_min,obs->touch_id_list,ref_cnt->bad,ref_cnt->pend,result[MID],result[HIGH_POS],result[HIGHEST]);
								#endif
								if (result[MID])
									ref_cnt->good >>= 1;
								if (accumulator < cfg->grad_accumulator_low && accumulator0 < obs->accumulator_ref) {
									if (accumulator0 < cfg->grad_accumulator_low) {
										ref_cnt->good = 0;
										ref_cnt->bad++;
										if (!result[DELTA])
											ref_cnt->bad++;
										if (accumulator < (obs->accumulator_ref >> cfg->ref_signal_ratio_shift))
											ref_cnt->bad++;
										if (obs->accumulator_ref > cfg->grad_accumulator_high)
											ref_cnt->bad++;
										enable_t8 =true;
									} else {
										if (!result[DELTA]/* || accumulator0 < cfg->grad_accumulator_high*/) {
											ref_cnt->pend++;
											enable_t8 =true;
										}
									}
									#if (DBG_LEVEL > 0)
									dev_info2(dev, "step_3_msg signal check accumulator(%d %d, ref %d %d) set bad %d pend %d delta %d high_pos %d\n",
										accumulator0,accumulator1,obs->accumulator_ref,obs->accumulator_ref_min,ref_cnt->bad,ref_cnt->pend,result[DELTA],result[HIGH_POS]);
									#endif
								} else {
									if (test_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH, &obs->flag)) {
										if (result[MID] && accumulator < cfg->grad_accumulator_low && !obs->accumulator_ref) {
											if ((!result[HIGHEST] || node >= cfg->node_large_area)  && !result[DELTA] && !accumulator) {
												ref_cnt->pend++;
												dev_info2(dev, "step_3_msg signal check accumulator zero, set pend %d middle %d highest %d\n",ref_cnt->pend,result[MID],result[HIGHEST]);
											}
											enable_t8 =true;
										} else if (accumulator < (obs->accumulator_ref >> cfg->ref_signal_ratio_shift)) {
											if ((result[HIGHEST] || node >= cfg->node_large_area ) && !result[DELTA]) {
												ref_cnt->pend++;
												dev_info2(dev, "step_3_msg signal check accumulator small(%d,%d,%d), set pend %d middle %d highest %d\n",accumulator0,accumulator1,obs->accumulator_ref,ref_cnt->pend,result[MID],result[HIGHEST]);
											}
											enable_t8 =true;
										}
									} else {
										if (accumulator > cfg->grad_accumulator_high) {
											ref_diff = accumulator - obs->accumulator_ref_min; //signed value
											if ((ref_diff < cfg->grad_accumulator_high * cfg->delta_highest_limit_ratio) && result[HIGH_POS]) {
												if (accumulator0 + cfg->grad_accumulator_high < obs->accumulator_ref ||
														accumulator1 + cfg->grad_accumulator_high < obs->accumulator_ref_min) {
													if (!result[DELTA])
														ref_cnt->pend++;
													enable_t8 = true;
													dev_info2(dev, "step_3_msg signal check accumulator min(%d %d %d), set pend %d highest %d\n",
															accumulator,ref_diff,obs->accumulator_ref_min,ref_cnt->pend,result[HIGHEST]);
												} else {
													if (accumulator0 < obs->accumulator_ref)
														enable_t8 = true;
												}
											}
										}
									}
								}

								if (accumulator0 < obs->accumulator_ref) {
									dev_dbg2(dev, "step_3_msg accumulator(%d %d) enable t8 \n",accumulator0,obs->accumulator_ref);
									merge_matrix(obs->buf[T37_IRON],obs->buf[T37_IRON],obs->buf[T37_REF0],
										x_size,y_size,SURFACE_MERGE_AVG_HYSIS);
									enable_t8 = true;
								}

								if (enable_t8) {
									//pulse t8
									if (!test_flag(PL_STATUS_FLAG_T8_SET, &pl_flag)) {
											state = get_t8_state(obs->step, obs->flag, pl_flag, -1, pl->config.num_t8_config);
											p->set_t8_cfg(p->dev, state, 0);
										if (state == T8_HALT)
											p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
										else
											p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);
									}
								}
							} else {
								if (ref_cnt->bad && ref_cnt->good > (ref_cnt->bad << 1)) {
									ref_cnt->bad--;
								}
								if (ref_cnt->pend && ref_cnt->good > (ref_cnt->pend << 1)) {
									ref_cnt->pend--;
								}

								//signal is similiar with delta
								//opposite increase end speed
								if (test_flag(WK_KEEP_SPEED_AT_PRESSED,&cfg->grad_acc_flag)) {
									if (accumulator > (obs->accumulator_ref << cfg->ref_signal_ratio_shift) &&
											accumulator > cfg->grad_accumulator_high &&
											obs->accumulator_ref < cfg->grad_accumulator_low) {
										if (test_flag(PL_STATUS_FLAG_T65_SET, &pl_flag) &&
												test_flag(PL_STATUS_FLAG_T80_SET, &pl_flag))
											ref_cnt->good++;
									}
								}
							}
						}
					}

					if (result[MID]) {
						if (sf_cnt->bad)
							sf_cnt->bad--;
						//for chip bug: 336s 2.1.AA(T65)
						if (test_flag(BUGS_CHECK_T65_BURST,&cfg->grad_acc_flag)) {
							flag = get_gradient_flag(obs->flag,pl_flag);
							merge_matrix(obs->buf[T37_DELTA_S],obs->buf[T37_DELTA0],obs->buf[T37_DELTA_S],
									x_size,y_size,SURFACE_MERGE_AVG_HYSIS3);
							ret = mxt_surface_x_line_check_t65_burst(p, flag, T37_DELTA_S, thld_t9[TINY],thld_t9[MID], cfg->xline_check_percent);
							if (!ret)
								ret = mxt_surface_x_line_check_t65_burst(p, flag, T37_DELTA0, thld_t9[TINY],thld_t9[HIGH_NEG], cfg->xline_check_percent);
							if (ret) {
								dev_info2(dev, "step_3_msg delta check x line t65 burst set cal\n");
								p->set_t6_cal(p->dev);
								break;
							}
						}

						if (test_flag(BUGS_CHECK_REFERENCE_DROP,&cfg->grad_acc_flag)) {
							flag = get_gradient_flag(obs->flag,pl_flag);
							ret = mxt_surface_x_line_check_ref_drop(p, T37_REF0, T37_DELTA0, thld_t9[HIGH_POS], flag);
							if (ret) {
								dev_info2(dev, "step_3_msg delta check x line ref drop set cal\n");
								p->set_t6_cal(p->dev);
								break;
							}
						}
					}

					if (result[HIGH_POS]) {
						sf_cnt->bad = 0;
						if (test_flag(BUGS_CHECK_T100_REPORT,&cfg->grad_acc_flag)) {
							if (!obs->touch_id_list) { //no touch or touch released
								dev_err(dev, "step_3_msg high pos found bug not points");
								mxt_surface_aquire_reset(p,sf_ref,0);
							}
						}
						if (test_flag(BUGS_CHECK_DELTA_HIGH,&cfg->grad_acc_flag)) {
							merge_matrix(obs->buf[T37_TEMP], obs->buf[T37_DELTA0],NULL,
								x_size,y_size,SURFACE_MERGE_COPY);
							flag = get_gradient_flag(obs->flag,pl_flag);
							flag |= cfg->delta_line_check_flag;
							count = mxt_surface_delta_high_check(p,flag,T37_TEMP,thld_t9[MID],thld_t9[HIGHEST],cfg->delta_line_check_skip);
							if (count >= cfg->delta_line_high_count) {
								dev_info2(dev, "step_3_msg delta check high delta set cal, count %d\n",count);
								p->set_t6_cal(p->dev);
								break;
							}
						}
						if (result[HIGHEST]) {
							if (!test_flag(T37_STEP_3_CHECK_DELTA_PENDING,&obs->step)) {
								obs->time_delta_check_st_2 = jiffies;
								if (test_flag(T37_STATE_TOUCH_RELEASED, &state_move)) {
									dev_info2(dev, "step_3_msg set T37_STEP_3_CHECK_DELTA_PENDING #2\n");
									set_flag(T37_STEP_3_CHECK_DELTA_PENDING,&obs->step);
								}
							}
						}
					}
				}
				if (test_flag(BUGS_CHECK_ZERO_BURST,&cfg->grad_acc_flag)) {
					flag = get_gradient_flag(obs->flag,pl_flag);
					ret = mxt_surface_x_line_check_zero_burst(p, flag, T37_DELTA0, 0);
					if (ret) {
						dev_info2(dev, "step_3_msg delta check x line zero burst set cal\n");
						p->set_t6_cal(p->dev);
						break;
					}
				}

				if (test_flag(BUGS_CHECK_REFERENCE_IRON,&cfg->grad_acc_flag)) {

					if (obs->accumulator_ref < cfg->grad_accumulator_low) {
						merge_matrix(obs->buf[T37_TEMP], obs->buf[T37_REF0],obs->buf[T37_IRON],
							x_size,y_size,SURFACE_MERGE_SUB);
						flag = get_gradient_flag(obs->flag,pl_flag);
						ret = mxt_surface_ref_iron_check(p,flag,T37_TEMP,T37_DELTA0,thld_t9[TINY]>>2,thld_t9[TINY],thld_t9[MID],cfg->reference_drop_percent);
						if (ret) {
							merge_matrix(obs->buf[T37_TEMP], obs->buf[T37_TEMP],obs->buf[T37_DELTA0],
								x_size,y_size,SURFACE_MERGE_SUB);

							flag = get_gradient_flag(obs->flag,pl_flag);
							flag &= ~ACCUMULATOR_GRAD_COMP;
							flag |= cfg->grad_acc_flag | ACCUMULATOR_GRAD_CACULATE;
							accumulator = mxt_surface_gradient_statistics(p, pl_flag, T37_TEMP, T37_TRANS, T37_DELTA0, T37_GRAD1_X, T37_GRAD1_Y, T37_GRAD1_K, grad_level_list_high, NUM_GRAD_LEVEL_LIST, &grad_cache, 0, flag);
							dev_info2(dev, "step_3_msg delta check ref iron acc %d\n",accumulator);
							if (accumulator < cfg->grad_accumulator_low) {
								dev_info2(dev, "step_3_msg delta check ref iron set cal\n");
								p->set_t6_cal(p->dev);
								break;
							}
						}
					}
				}

				if (test_flag(BUGS_CHECK_SOFT_T8,&cfg->grad_acc_flag)) {
					flag = get_gradient_flag(obs->flag,pl_flag);

					state = get_t8_state(obs->step, obs->flag, pl_flag, step_t8, pl->config.num_t8_config);
					ret = mxt_surface_soft_check_anti_touch(p, flag, T37_DELTA0, 
						&pl_cfg->t9_t100_cfg[T9_T100_NORMAL],
						&pl_cfg->t8_cfg[state]);
					if (ret) {
						dev_info2(dev, "step_3_msg delta check anti touch set cal\n");
						p->set_t6_cal(p->dev);
						break;
					}
				}
			} else {  //ref round
				flag = get_gradient_flag(obs->flag,pl_flag);
				flag |= cfg->grad_acc_flag;
				if (delta_cnt->good && !delta_cnt->bad) {
					flag |= ACCUMULATOR_GRAD_DRIFT_NORM;
				}
				/*
				if (!test_flag(T37_FLAG_SURFACE_GRADIENT_SMOOTH,&obs->flag))
					flag |= ACCUMULATOR_GRAD_INVALUE;
				*/
				accumulator = mxt_surface_gradient_statistics(p, pl_flag, T37_REF0, T37_TRANS, T37_DELTA0, T37_GRAD0_X, T37_GRAD0_Y, T37_GRAD0_K, grad_level_list_low, NUM_GRAD_LEVEL_LIST, &grad_cache, GRAD_DRIFT_SLOW, flag);
				if (accumulator <= cfg->grad_accumulator_low) {
					sf_cnt->good++;
					sf_cnt->sleep++;
					if (delta_cnt->pend > 0)
						delta_cnt->pend--;
					dev_dbg2(dev, "step_3_msg ref0 check accumulator %d ref(%d %d %d) delta(%d %d %d)\n",
							accumulator,
							sf_cnt->good,sf_cnt->pend,sf_cnt->bad,
							delta_cnt->good,delta_cnt->pend,delta_cnt->bad);
					if (test_flag(T37_STEP_3_CHECK_SIGNAL_FIRST, &obs->step)) {
						if (sf_cnt->good >= cfg->grad_good_times &&
								delta_cnt->good >= cfg->grad_good_times &&
								!delta_cnt->bad) {
							dev_info2(dev, "mxt t37 step 3 end by peaceful surface\n");
							set_and_clr_flag(T37_STEP_4_ENTER, T37_STEP_3_IN, &obs->step);
							mxt_process_obj_recovery(p,pl_flag);
							break;
						}
					}
				} else {
					if (sf_cnt->good)
						sf_cnt->good--;
					delta_cnt->pend = 1;
					sf_cnt->sleep = 0;
				}
				obs->accumulator_ref = accumulator;
				if (!obs->accumulator_ref_min ||  obs->accumulator_ref_min > accumulator) {//reset when zero
					dev_dbg2(dev, "mxt t37 step 3 set ref min %d\n",obs->accumulator_ref_min);
					obs->accumulator_ref_min = accumulator;
				}
			}
		} while (0);

		//speed up if there reference is sucipicous
		if (ref_cnt->pend || ref_cnt->bad) {
			if (ref_cnt->bad) {
				if (ref_cnt->bad >= cfg->grad_bad_times) {
					dev_info2(dev, "step_3_msg signal check accumulator bad set cal (bad %d pend %d)\n",ref_cnt->bad,ref_cnt->pend);
					p->set_t6_cal(p->dev);
				}
			}

			pend_times = ref_cnt->pend + ref_cnt->bad;
			 /*
			 if (test_flag(PL_STATUS_FLAG_T65_SET, &pl_flag) &&
				 test_flag(PL_STATUS_FLAG_T80_SET, &pl_flag) && pend_times)
				 pend_times--;
			 */
			 if (test_flag(T37_STATE_TOUCH_MOVING|T37_STATE_TOUCH_MOV_PENDING, &state_move) && pend_times)
				 pend_times--;
			if (pend_times)
				dev_dbg2(dev, "step_3_msg (bad %d pend %d %d mov 0x%lx)\n", 
						ref_cnt->bad,ref_cnt->pend,pend_times,state_move);
			if (pend_times >= cfg->grad_pend_times) {
					dev_info2(dev, "step_3_msg signal check accumulator zero set cal (bad %d pend %d %d mov 0x%lx)\n",ref_cnt->bad,ref_cnt->pend,pend_times,state_move);
					p->set_t6_cal(p->dev);
				}

			if (ret == 0)
				ret = -EAGAIN;
		}

		if (delta_cnt->pend || delta_cnt->bad) {
			pend_times = delta_cnt->bad;
			if (test_flag(T37_STATE_TOUCH_MOVING|T37_STATE_TOUCH_MOV_PENDING, &state_move) && pend_times)
				pend_times--;
			if (pend_times >= cfg->delta_bad_times) {
				dev_info2(dev, "step_3_msg delta check set cal\n");
				p->set_t6_cal(p->dev);
			}
			if (ret == 0)
				ret = -EAGAIN;
		}

		dev_dbg2(dev, "step_3_msg list 0x%lx (cnt %d, good %d, bad %d) sf %d pos(%d,%d) size(%d,%d) ret %d\n",
				obs->touch_id_list,
				sf_cnt->count,
				sf_cnt->good,
				sf_cnt->bad,
				sf,
				sf_info->x,sf_info->y,
				x_size,y_size,
				ret);
	} else if (ret == -EAGAIN) {

	} else if (ret == -EINVAL) {
		dev_info2(dev, "step_3_msg surface aquire %d failed \n",sf);
	}

	return ret;
}

static int mxt_proc_step_4_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	dev_info2(dev, "mxt t37 at mxt_proc_step_4_msg\n");

	//mxt_process_obj_recovery(p, pl_flag);
	p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_CAL_END, 0);
	set_and_clr_flag(T37_STEP_END, T37_STEP_4_IN, &obs->step);

	return 0;
}

static int mxt_proc_step_x_msg(struct plugin_cal *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	const struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg); 
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	if (test_flag(PL_STATUS_FLAG_PHONE_ON,&pl_flag)) {
		if (test_flag(T37_FLAG_RESUME,&obs->flag)) {
			set_flag(T37_FLAG_PHONE_ON,&obs->flag);
			obs->touch_id_list &= (1 << pl->config.t9_t100_cfg[T9_T100_SINGLE_TOUCH].num_touch) - 1;
			obs->time_phone_off = 0;
		}
	} else {
		if (test_and_clear_flag(T37_FLAG_PHONE_ON,&obs->flag)) {
			dev_info2(dev, "step_x_msg phone off set calibration\n");
			mxt_process_obj_recovery(p,pl_flag);
			p->set_t6_cal(p->dev);
		}
	}

	if (test_flag(PL_STATUS_FLAG_PROXIMITY_REMOVED, &pl_flag)) {
		dev_info2(dev, "step_x_msg proximity set calibration\n");
		p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_PROXIMITY_REMOVED);
		p->set_t6_cal(p->dev);
	}

	if (test_flag(T37_STEP_3_IN, &obs->step)) {
		if (test_flag(T37_FLAG_PHONE_ON,&obs->flag)) {
			clear_flag(T37_STEP_3_CHECK_DELTA_PENDING,&obs->step);
			obs->time_delta_check_st = obs->time_delta_check_st_2 = jiffies;
		}
	}

	return 0;
}

static int mxt_process_obj_recovery(struct plugin_cal *p,unsigned long pl_flag)
{
	struct t37_config *cfg = p->cfg;
	struct t37_observer *obs = p->obs;
	int state;
	unsigned long unset;

	//t8
	state = get_t8_state(obs->step, obs->flag, pl_flag, 0, 0);
	p->set_t8_cfg(p->dev, state, 0);
	if (state >= T8_WEAK_PALM || state == T8_HALT)
		p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T8_SET);
	else
		p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T8_SET, 0);

	//t9/t100
	state = get_t9_t100_state(obs->step, obs->flag, pl_flag);
	p->set_t9_t100_cfg(p->dev, state, BIT_MASK(MXT_T9_T100_MASK_MRGTHR)|BIT_MASK(MXT_T9_T100_MASK_MRGHYST)/*|BIT_MASK(MXT_T9_T100_MASK_N_TOUCH)*/);

	//t42

	//t55
	state = get_t55_state(obs->step, obs->flag, pl_flag);
	p->set_t55_adp_thld(p->dev, state);

	//t65
	if (test_flag(WK_KEEP_T65_ALWAYS_ON, &cfg->grad_acc_flag))
		unset = BIT_MASK(MXT_T65_MASK_CTRL);
	else
		unset = BIT_MASK(MXT_T65_MASK_GRADTHR) | BIT_MASK(MXT_T65_MASK_LPFILTER);
	state = get_t65_state(obs->step, obs->flag, pl_flag);
	p->set_t65_cfg(p->dev, state, unset);
	if (state == T65_NORMAL)
		p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T65_SET);
	else
		p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T65_SET, 0);

	//t80
	if (test_flag(WK_KEEP_T80_ALWAYS_ON, &cfg->grad_acc_flag))
		unset = BIT_MASK(MXT_T80_MASK_CTRL);
	else
		unset = BIT_MASK(MXT_T80_MASK_COMP_GAIN);
	state = get_t80_state(obs->step, obs->flag, pl_flag);
	p->set_t80_cfg(p->dev, state, unset);
	if (state == T80_NORMAL)
		p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_T80_SET);
	else
		p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_T80_SET, 0);

	return 0;
}

static void plugin_cal_t37_pre_process_messages(struct plugin_cal *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;

	dev_dbg2(dev, "mxt t37(pre) touch = 0x%lx step=0x%lx flag=0x%lx pl_flag=0x%lx\n",
			obs->touch_id_list,obs->step, obs->flag, pl_flag);

	if (test_flag(T37_FLAG_WORKAROUND_HALT,&obs->flag))
		return;

	mxt_proc_step_x_msg(p, pl_flag);
}

static long plugin_cal_t37_post_process_messages(struct plugin_cal *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;


	long next_interval = MAX_SCHEDULE_TIMEOUT;
	int ret = 0;

	dev_dbg2(dev, "mxt t37 touch= 0x%lx step=0x%lx flag=0x%lx pl_flag=0x%lx\n",
			obs->touch_id_list,obs->step, obs->flag, pl_flag);

	if (test_flag(T37_FLAG_WORKAROUND_HALT,&obs->flag))
		return next_interval;

	if (test_flag(T37_STEP_0_IN,&obs->step)) {
		dev_dbg2(dev, "mxt t37 at T37_STEP_0_IN\n");
		//Here won't enter unless debug purpose...
		ret = mxt_proc_step_0_msg(p, pl_flag);
	} else if (test_flag(T37_STEP_1_IN, &obs->step)) {
		dev_dbg2(dev, "mxt t37 at T37_STEP_1_IN\n");
		ret = mxt_proc_step_1_msg(p, pl_flag);
	} else {
		if (time_after_eq(jiffies,obs->time_next_st)) {
			if (test_flag(T37_STEP_2_IN, &obs->step)) {
				dev_dbg2(dev, "mxt t37 at T37_STEP_2_IN\n");
				ret = mxt_proc_step_2_msg(p, pl_flag);
			} else if (test_flag(T37_STEP_3_IN, &obs->step)) {
				dev_dbg2(dev, "mxt t37 at T37_STEP_3_IN\n");
				ret = mxt_proc_step_3_msg(p, pl_flag);
			} else if (test_flag(T37_STEP_4_IN, &obs->step)) {
				dev_dbg2(dev, "mxt t37 at T37_STEP_4_IN\n");
				ret = mxt_proc_step_4_msg(p, pl_flag);
			}
		} else {
			dev_dbg2(dev, "mxt wait %lu\n", obs->time_next_st - jiffies);
			ret = -ETIME;
		}
	}

	if (ret == -ETIME)
		next_interval = obs->time_next_st - jiffies + 1;
	if (ret == -EAGAIN)
		next_interval = cfg->interval_fast;
	else {
		next_interval = step_interval(p,obs->step);
	}

	if (ret != -ETIME) {
		obs->time_next_st = jiffies;
		if (next_interval != MAX_SCHEDULE_TIMEOUT)
			obs->time_next_st = jiffies + next_interval;
		else
			obs->time_next_st = next_interval;
	}

	if (!next_interval)
		next_interval = MAX_SCHEDULE_TIMEOUT; //just wait interrupt if zero

	dev_dbg2(dev, "t37 mxt interval %lu wait %ld\n",next_interval,obs->time_next_st - jiffies);
	return next_interval;
}

struct stat_grad_config config_grad_level_array[][NUM_GRAD_LEVEL_LIST] =
{
	//{ascending order}
	{{40,3},{60,8},{80,12},{100,15},{120,20},{150,30}},
	{{40,3},{60,5},{80,10},{100,15},{120,20},{150,30}},
	{{40,3},{60,5},{80,8},{100,10},{120,15},{150,20}},
	{{40,3},{60,5},{80,8},{100,10},{120,15},{150,20}},
	{{40,2},{60,4},{80,6},{100,8},{120,12},{150,18}},
	{{40,2},{60,4},{80,6},{100,8},{120,10},{150,15}},
};
int config_grad_block_high_limit_array[T37_PAD3_GRAD_BLOCKS] = {
	120,	50,	 120,
	100,	0,	 100,
	120,	50,	 120
};

int config_grad_drift_coef_array[GRAD_DRIFT_NUMBER][GRAD_DRIFT_ITEMS] = {
	{20,40,30,50,20,40},
	{50,80,50,80,50,80},
	{700,950,800,1000,700,950},
};

int config_grad_invalid_count_array[GRAD_INVALID_CNT_NUM] = {
	2,3,5
	/*3,5,8*/
};

static int init_t37_object(struct plugin_cal *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct plug_interface *pl = container_of(dcfg, struct plug_interface, init_cfg);
	struct device *dev = dcfg->dev;
	const struct plug_config *pl_cfg = &pl->config;
	struct t37_observer *obs = p->obs;
	struct t37_config *cfg = p->cfg;
	void *buf;
	int buffer_size,pad_size,i;
	int x_matrix_size,y_matrix_size;
	int state,grad_cache_add[T72_NOISE_STATE_TYPES],grad_block_low_limit;
	//allocate buffer

	x_matrix_size = dcfg->m[MX].x1 - dcfg->m[MX].x0 + 1;
	y_matrix_size = dcfg->m[MX].y1 - dcfg->m[MX].y0 + 1;
	buf = obs->obs_buf;
	if (buf) {
		kfree(buf);
		obs->obs_buf = NULL;
	}
	pad_size = max_t(u8,x_matrix_size , y_matrix_size);
	pad_size *= (T37_PAD_NUMBER -1);  //last one pad is user defined
	pad_size += max3(T37_PAD3_RAW_NUMBER,T37_PAD3_GRAD_NUMBER,T37_PAD3_DIGITAL_NUMBER);

	buffer_size = x_matrix_size * y_matrix_size;
	buffer_size += pad_size;
	buffer_size *= sizeof(s16);

	buf = kzalloc(
			buffer_size * (T37_BUFFER_NUMBER + T72_NOISE_STATE_TYPES * 2),
			GFP_KERNEL);
	if (!buf) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	obs->obs_buf = buf;

	dev_dbg(dev, "allocated t37 buffer at %p, node number %d\n",
			buf,buffer_size * T37_BUFFER_NUMBER);

	for (i = 0; i < T37_BUFFER_NUMBER; i++) {
		obs->buf[i] = buf;
		buf += buffer_size;
	}

	for (i = 0; i < T72_NOISE_STATE_TYPES; i++) {
		obs->grad_cache[i].x_axis = buf;
		buf += buffer_size;
		obs->grad_cache[i].y_axis = buf;
		buf += buffer_size;
	}

	cfg->interval_delta_check = HZ * 0;
	cfg->interval_delta_middle_check = HZ * 8;
	cfg->interval_delta_check_end = 40 * HZ;
	cfg->interval_delta_check_end2 = 50 * HZ;
	cfg->interval_grad_check = HZ * 30;
	cfg->interval_distance_check = HZ * 2;
	cfg->interval_step_0 = HZ / 10;
	cfg->interval_step_1 = HZ / 10;
	cfg->interval_step_2 = HZ / 6;//20
	cfg->interval_step_3 = HZ / 10;//20
	cfg->interval_step_4 = HZ / 10;
	cfg->interval_slow = HZ / 3;
	cfg->interval_normal = HZ / 8;
	cfg->interval_fast = 1;
	cfg->interval_zero = 0;

	cfg->interval_step2_pressed_short = HZ * 2;
	cfg->interval_step2_pressed_long = HZ * 6;

	cfg->step1_single_count = 10;
	cfg->step2_single_count = 6;
	cfg->step2_grad_ref_check1_count = 2;
	cfg->step2_grad_ref_check2_count = 3;
	cfg->step2_single_touch_check_count = 1;
	cfg->step2_check_point_count = MAX_TRACE_POINTS - 2;
	cfg->step3_sleep_count = 3;

	cfg->distance_touch_move[MOVEMENT_SHORT] = 10;//percent
	cfg->distance_touch_move[MOVEMENT_MIDDLE] = 20;
	cfg->distance_touch_move[MOVEMENT_LONG] = 30;
	cfg->distance_touch_move[MOVEMENT_HUGE_LONG] = 75;
	for (i = 0; i < MOVEMENT_NUM; i++) {
		cfg->movement[i].x = caculate_percent(dcfg->max_x, cfg->distance_touch_move[i]);
		cfg->movement[i].y = caculate_percent(dcfg->max_y, cfg->distance_touch_move[i]);
	}

	cfg->surface_retry_times = 8;

	cfg->delta_tiny_limit_ratio = 25;
	cfg->delta_low_limit_ratio = 35;
	cfg->delta_mid_limit_ratio = 50;
	cfg->delta_high_limit_ratio = 85;
	cfg->delta_highest_limit_ratio = 150;
	cfg->delta_stable_limit_ratio = 75;

	cfg->delta_good_times = 1;
	cfg->delta_bad_times = 2;
	cfg->delta_repeat_times_shift = 0;

	cfg->grad_good_times = 120;
	cfg->grad_bad_times = 2;
	cfg->grad_pend_times = 2;

	cfg->grad_level_array = &config_grad_level_array[0];
	cfg->num_grad_level_array = ARRAY_SIZE(config_grad_level_array);

	cfg->grad_accumulator_low = cfg->grad_level_array[0][1].percent;
	cfg->grad_accumulator_high = 100;

	cfg->ref_signal_ratio_shift = 5;
	cfg->node_large_area = 25;

	cfg->grad_drift_coef_array = &config_grad_drift_coef_array[0];

	cfg->grad_invalid_count_array = &config_grad_invalid_count_array[0];

	cfg->edge.x0 = 2;
	cfg->edge.y0 = 1;
	cfg->edge.x1 = 1;
	cfg->edge.y1 = 1;
	cfg->grad_t_modules = 1;
	cfg->grad_k_modules = 1;
	cfg->grad_acc_flag = ACCUMULATOR_GRAD_X|ACCUMULATOR_GRAD_Y;
	//336s
	cfg->grad_acc_flag |= BUGS_CHECK_T65_BURST | WK_KEEP_SPEED_AT_PRESSED;
	//540s
	//cfg->grad_acc_flag |= BUGS_CHECK_T100_REPORT|BUGS_CHECK_REFERENCE_DROP|BUGS_CHECK_DELTA_HIGH|BUGS_CHECK_REFERENCE_IRON;
	cfg->grad_acc_flag |= WK_KEEP_T65_ALWAYS_ON|WK_KEEP_T80_ALWAYS_ON|BUGS_CHECK_SOFT_T8|BUGS_CHECK_REFERENCE_IRON;
	cfg->grad_acc_flag |= BUGS_CHECK_DELTA_HIGH;

	if (dcfg->t38.data[MXT_T38_MGWD] == MXT_T38_MAGIC_WORD) {
		cfg->grad_block_low_limit= (int)dcfg->t38.data[MXT_T38_BLOCK_LOW_LIMIT_LEVEL];
		cfg->grad_block_high_limit = (int)dcfg->t38.data[MXT_T38_BLOCK_HIGH_LIMIT_LEVEL];
		if (cfg->grad_block_high_limit < cfg->grad_block_low_limit)
			cfg->grad_block_high_limit = cfg->grad_block_low_limit << 1;
		grad_cache_add[0] = 0;
		state = get_t9_t100_state(obs->step, 0, PL_STATUS_FLAG_NOISE);
		grad_block_low_limit = (int)((pl_cfg->t9_t100_cfg[state].threshold - pl_cfg->t9_t100_cfg[T9_T100_NORMAL].threshold) >> 1);
		grad_cache_add[1] = caculate_threshlod(grad_block_low_limit,100);
		state = get_t9_t100_state(obs->step, 0, PL_STATUS_FLAG_VERY_NOISE);
		grad_block_low_limit = (int)((pl_cfg->t9_t100_cfg[state].threshold - pl_cfg->t9_t100_cfg[T9_T100_NORMAL].threshold) >> 1);
		grad_cache_add[2] = caculate_threshlod(grad_block_low_limit,100);
		for (i = 0; i < T37_PAD3_GRAD_BLOCKS; i++) {
			cfg->grad_block_high_limit_list[i] = (cfg->grad_block_high_limit << 3) + config_grad_block_high_limit_array[i];
		}
	} else {
		cfg->grad_block_low_limit = (200 >> 3);
		cfg->grad_block_high_limit = (500 >> 3);
		for (i = 0; i < T37_PAD3_GRAD_BLOCKS; i++) {
			cfg->grad_block_high_limit_list[i] = (cfg->grad_block_high_limit << 3) + config_grad_block_high_limit_array[i];
		}
	}
	cfg->grad_block_highest_limit_ratio = 150;
	gradient_cache_reset(p);
	//bugs t65 bend whole surface
	cfg->reference_drop_percent = 90;
	cfg->xline_check_percent = 80;

	cfg->delta_line_check_flag = ACCUMULATOR_GRAD_X|ACCUMULATOR_GRAD_Y;
	cfg->delta_line_check_skip = 0;
	cfg->delta_line_high_count = 2;

	cfg->soft_t8_internal_thld_percent = 50;

	obs->time_next_st = jiffies;

	return 0;
}

static int deinit_t37_object(struct plugin_cal *p)
{
	struct t37_observer * obs = p->obs;

	if (obs->obs_buf) {
		kfree(obs->obs_buf);
		obs->obs_buf = NULL;
	}
	return 0;
}

static int plugin_cal_t37_show(struct plugin_cal *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer * obs = p->obs;
	struct t37_config *cfg = p->cfg;
	struct rect surface,surface_t;
	int x_size,y_size;
	int i,j;

	dev_info(dev, "[mxt]PLUG_CAL_T37_VERSION: 0x%x\n",PLUG_CAL_T37_VERSION);

	if (!p->init)
		return 0;

	memcpy(&surface,&dcfg->m[MX_AA],sizeof(struct rect));
	memcpy(&surface_t,&dcfg->m[MX_AA],sizeof(struct rect));
	swap(surface_t.x0,surface_t.y0);
	swap(surface_t.x1,surface_t.y1);
	x_size = surface.x1 - surface.x0 + 1;
	y_size = surface.y1 - surface.y0 + 1;

	dev_info(dev, "[mxt]T37 cfg :\n");
	dev_info(dev, "[mxt]interval: Dcheck=%ld,Dtmidend=%ld,Dend=%ld,Dend2=%ld,Ggcheck=%ld,Gdcheck=%ld,S0=%ld,S1=%ld,S2=%ld,S3=%ld,S4=%ld,Sf=%ld,Sz=%ld,Sn=%ld,Ss=%ld,S2s=%ld,S2l=%ld",
			cfg->interval_delta_check,
			cfg->interval_delta_middle_check,
			cfg->interval_delta_check_end,
			cfg->interval_delta_check_end2,
			cfg->interval_grad_check,
			cfg->interval_distance_check,
			cfg->interval_step_0,
			cfg->interval_step_1,
			cfg->interval_step_2,
			cfg->interval_step_3,
			cfg->interval_step_4,
			cfg->interval_fast,
			cfg->interval_zero,
			cfg->interval_normal,
			cfg->interval_slow,
			cfg->interval_step2_pressed_short,
			cfg->interval_step2_pressed_long);
	dev_info(dev, "[mxt]step: S1cnt=%d,S2cnt=%d,G1cnt=%d,G2cnt=%d,G2scnt=%d,G2ccnt=%d,G3scnt=%d\n",
			cfg->step1_single_count,
			cfg->step2_single_count,
			cfg->step2_grad_ref_check1_count,
			cfg->step2_grad_ref_check2_count,
			cfg->step2_single_touch_check_count,
		cfg->step2_check_point_count,
		cfg->step3_sleep_count);
	dev_info(dev, "[mxt]retry: Sufrace=%d\n",
			cfg->surface_retry_times);
	dev_info(dev, "[mxt]ratio: Tiny=%d,Low=%d,Mid=%d,High=%d,Highest=%d,stable=%d\n",
			cfg->delta_tiny_limit_ratio,
			cfg->delta_low_limit_ratio,
			cfg->delta_mid_limit_ratio,
			cfg->delta_high_limit_ratio,
			cfg->delta_highest_limit_ratio,
			cfg->delta_stable_limit_ratio);
	dev_info(dev, "[mxt]delta times: Good=%d,Bad=%d,Repeat=%d\n",
			cfg->delta_good_times,
			cfg->delta_bad_times,
			cfg->delta_repeat_times_shift);
	dev_info(dev, "[mxt]grad times: Good=%d,Bad=%d Pend=%d\n",
			cfg->grad_good_times,
			cfg->grad_bad_times,
			cfg->grad_pend_times);
	dev_info(dev, "[mxt]grad: modules(k)=%hd,modules(t)=%hd,b_limit=(low %d high %d highest %d),acc=0x%lx\n",
			cfg->grad_t_modules,cfg->grad_t_modules,cfg->grad_block_low_limit,cfg->grad_block_high_limit,cfg->grad_block_highest_limit_ratio,
			cfg->grad_acc_flag);
	dev_info(dev, "[mxt]grad edge: (%d,%d),(%d,%d)\n",
			cfg->edge.x0,
			cfg->edge.y0,
			cfg->edge.x1,
			cfg->edge.y1);
	dev_info(dev, "[mxt]grad acc: High=%d,Low=%d\n",
			cfg->grad_accumulator_high,
			cfg->grad_accumulator_low);

	dev_info(dev, "[mxt]grad ref-signal ratio: %d\n",
			cfg->ref_signal_ratio_shift);

	dev_info(dev, "[mxt]node large area: %d\n",
		cfg->node_large_area);

	dev_info(dev, "[mxt]xline: Percent=%d %d\n",
		cfg->xline_check_percent,
		cfg->reference_drop_percent);

	dev_info(dev, "[mxt]soft t8: Percent=%d\n",
		cfg->soft_t8_internal_thld_percent);

	dev_info(dev, "[mxt]delta line: high=%d skip=%d flag=0x%lx\n",
		cfg->delta_line_high_count,
		cfg->delta_line_check_skip,
		cfg->delta_line_check_flag);
	for (i = 0; i < T37_PAD3_GRAD_BLOCKS; i++) {
		dev_info(dev, "[mxt]grad limit(%d): %d\n",i,cfg->grad_block_high_limit_list[i]);
	}

	dev_info(dev, "[mxt]grad: \n");
	for (i = 0; i < cfg->num_grad_level_array; i++) {
		for (j = 0; j < NUM_GRAD_LEVEL_LIST; j++) {
			dev_info(dev, "[mxt]grad: cfg(%d-%d) {%d,%d}\n",
					i, j, cfg->grad_level_array[i][j].thld, cfg->grad_level_array[i][j].percent);
		}
	}

	for (i = 0; i < GRAD_DRIFT_NUMBER; i++) {
		dev_info(dev, "[mxt]grad drift(%d): (%d,%d)(%d,%d)(%d,%d)\n",
				i,
				cfg->grad_drift_coef_array[i][0],cfg->grad_drift_coef_array[i][1],
				cfg->grad_drift_coef_array[i][2],cfg->grad_drift_coef_array[i][3],
				cfg->grad_drift_coef_array[i][4],cfg->grad_drift_coef_array[i][5]);
	}

	for (i = 0; i < GRAD_INVALID_CNT_NUM; i++) {
		dev_info(dev, "[mxt]grad invalid count: %d %d %d\n",
				cfg->grad_invalid_count_array[GRAD_INVALID_3],
				cfg->grad_invalid_count_array[GRAD_INVALID_5],
				cfg->grad_invalid_count_array[GRAD_INVALID_8]);
	}

	dev_info(dev, "[mxt]move distance: \n");
	for (i = 0; i < MOVEMENT_NUM; i++) {
		dev_info(dev, "[mxt]dist: M%d(%d,%d)\n",
				i,
				cfg->movement[i].x,
				cfg->movement[i].y);
	}

	dev_info(dev, "[mxt]\n");

	dev_info(dev, "[mxt]T37 obs :\n");
	dev_info(dev, "[mxt]status: Step=0x%08lx,Flag=0x%08lx\n",
			obs->step,obs->flag);
	dev_info(dev, "[mxt]time:  Tstart=%lu Tend=%lu,Tdchk=%lu,Tgchk=%lu,Tnext=%lu,Tphone=%lu\n",
			obs->time_touch_st,
			obs->time_touch_end,
			obs->time_delta_check_st,
			obs->time_gradient_check_st,
			obs->time_next_st,
			obs->time_phone_off);
	dev_info(dev, "[mxt]point:  List=0x%lx",
			obs->touch_id_list);
	for (i = 0; i < dcfg->t9_t100.num_touch; i++) {
		dev_info(dev, "[mxt]point[%d]:  flag = 0x%lx cnt=%d,first(%d,%d),last(%d,%d),distance(%d,%d) time(%ld,%ld,%ld)\n",
				i,
				obs->trace[i].flag,
				obs->trace[i].count,
				obs->trace[i].first.x,
				obs->trace[i].first.y,
				obs->trace[i].last.x,
				obs->trace[i].last.y,
				obs->trace[i].distance.x,
				obs->trace[i].distance.y,
				obs->trace[i].time_touch_st,
				obs->trace[i].time_touch_end,
				obs->trace[i].time_point_renw);
	}
	dev_info(dev, "[mxt]grad trace: \n");
	for (i = 0; i < GRAD_TRACE_NUMBER; i++) {
		dev_info(dev, "[mxt]gradient[%d] trace(cnt %d, good %d,bad %d pend %d) \n",
			i,
			obs->gradient[i].count,
			obs->gradient[i].good,
			obs->gradient[i].bad,
			obs->gradient[i].pend);
	}
	for (i = 0; i < GRAD_STATE_NUMBER; i++) {
		dev_info(dev, "[mxt]grad state[%d]: wait 0x%lx not 0x%lx\n",
				i,
				obs->step2_wait_state[i][GRAD_STATE_WAIT],obs->step2_wait_state[i][GRAD_STATE_NOT]);
	}

	for (i = 0; i < T37_BUFFER_NUMBER; i++) {
		dev_info(dev, "[mxt]sf[%d]: count=%d,good=%d,bad=%d,pend=%d,retry=%d\n",
				i,
				obs->sf_cnt[i].count,
				obs->sf_cnt[i].good,
				obs->sf_cnt[i].bad,
				obs->sf_cnt[i].pend,
				obs->sf_cnt[i].retry);
		dev_info(dev, "[mxt]sf[%d]: x %d, y %d\n",
				i,
				obs->sf_info_list[i].x,
				obs->sf_info_list[i].y);
		if (i == T37_TRANS || i == T37_TRANS || i == T37_GRAD0_Y || i == T37_GRAD1_Y ||
			i == T37_GRAD0_K || i == T37_GRAD1_K)
			print_matrix_surface(i,
					obs->buf[i],
					&surface_t,0);
		else
			print_matrix_surface(i,
					obs->buf[i],
					&surface,0);
	}

	for (i = 0; i < T72_NOISE_STATE_TYPES; i++) {
		dev_info(dev, "[mxt]grad cache %d\n",
			i);
		print_matrix(NULL, obs->grad_cache[i].x_axis, x_size, y_size);
		dev_info(dev, " ");
		print_matrix(NULL, obs->grad_cache[i].y_axis, y_size, x_size);
	}

	dev_info(dev, "[mxt] accumulator ref = %d min = %d\n",
			obs->accumulator_ref,
			obs->accumulator_ref_min);

	printk("\n");

	return 0;
}


static int plugin_cal_t37_store(struct plugin_cal *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t37_observer * obs = p->obs;
	struct t37_config *cfg = p->cfg;
	struct surface_count sf_cnt;
	struct point movement;
	int i,j, config[6];

	dev_info(dev, "[mxt]t37 store:%s\n",buf);

	if (!p->init)
		return 0;

	if (sscanf(buf, "interval: Dcheck=%ld,Dtmidend=%ld,Dend=%ld,Dend2=%ld,Ggcheck=%ld,Gdcheck=%ld,S0=%ld,S1=%ld,S2=%ld,S3=%ld,S4=%ld,Sf=%ld,Sz=%ld,Sn=%ld,Ss=%ld,S2s=%ld,S2l=%ld",
				&cfg->interval_delta_check,
				&cfg->interval_delta_middle_check,
				&cfg->interval_delta_check_end,
				&cfg->interval_delta_check_end2,
				&cfg->interval_grad_check,
				&cfg->interval_distance_check,
				&cfg->interval_step_0,
				&cfg->interval_step_1,
				&cfg->interval_step_2,
				&cfg->interval_step_3,
				&cfg->interval_step_4,
				&cfg->interval_fast,
				&cfg->interval_zero,
				&cfg->interval_normal,
				&cfg->interval_slow,
				&cfg->interval_step2_pressed_short,
				&cfg->interval_step2_pressed_long) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else if (sscanf(buf, "step: S1cnt=%d,S2cnt=%d,G1cnt=%d,G2cnt=%d,G2scnt=%d,G2ccnt=%d,G3scnt=%d\n",
				&cfg->step1_single_count,
				&cfg->step2_single_count,
				&cfg->step2_grad_ref_check1_count,
				&cfg->step2_grad_ref_check2_count,
				&cfg->step2_single_touch_check_count,
		&cfg->step2_check_point_count,
		&cfg->step3_sleep_count) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "retry: Sufrace=%d\n",
				&cfg->surface_retry_times)) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "ratio: Tiny=%d,Low=%d,Mid=%d,High=%d,Highest=%d,Stable=%d\n",
				&cfg->delta_tiny_limit_ratio,
				&cfg->delta_low_limit_ratio,
				&cfg->delta_mid_limit_ratio,
				&cfg->delta_high_limit_ratio,
				&cfg->delta_highest_limit_ratio,
				&cfg->delta_stable_limit_ratio) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "delta times: Good=%d,Bad=%d,Repeat=%d\n",
				&cfg->delta_good_times,&cfg->delta_bad_times,&cfg->delta_repeat_times_shift) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "grad times: Good=%d,Bad=%d,Pend=%d\n",
				&cfg->grad_good_times,
				&cfg->grad_bad_times,
				&cfg->grad_pend_times) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "grad acc: High=%d,Low=%d\n",
				&cfg->grad_accumulator_high,
				&cfg->grad_accumulator_low) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "[mxt]grad ref-signal ratio: %d\n",
				&cfg->ref_signal_ratio_shift) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else if (sscanf(buf, "[mxt]node large area: %d\n",
		&cfg->node_large_area) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else if (sscanf(buf, "xline: Percent=%d %d\\n",
		&cfg->xline_check_percent,
		&cfg->reference_drop_percent) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else if (sscanf(buf, "soft t8: Percent=%d\n",
		&cfg->soft_t8_internal_thld_percent) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else if (sscanf(buf, "[mxt]delta line: high=%d skip=%d flag=0x%lx\n",
		&cfg->delta_line_high_count,
		&cfg->delta_line_check_skip,
		&cfg->delta_line_check_flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "grad: cfg(%d-%d) {%d,%d}\n",
				&i,
				&j,
				&config[0],
				&config[1])== 4) {
		if (i >=0 && i < cfg->num_grad_level_array && j < NUM_GRAD_LEVEL_LIST) {
			cfg->grad_level_array[i][j].thld = (s16)config[0];
			cfg->grad_level_array[i][j].percent= (u16)config[1];
			dev_info(dev, "[mxt]OK\n");
		} else {
			dev_err(dev, "[mxt]Invalid grad type %d\n", i);
			return -EINVAL;
		}
	} else if (sscanf(buf, "[mxt]grad drift(%d): (%d,%d)(%d,%d)(%d,%d)\n",
				&i,
				&config[0],
				&config[1],
				&config[2],
				&config[3],
				&config[4],
				&config[5])== 7) {
		if (i >=0 && i < GRAD_DRIFT_NUMBER) {
			cfg->grad_drift_coef_array[i][0] = config[0];
			cfg->grad_drift_coef_array[i][1] = config[1];
			cfg->grad_drift_coef_array[i][2] = config[2];
			cfg->grad_drift_coef_array[i][3] = config[3];
			cfg->grad_drift_coef_array[i][4] = config[4];
			cfg->grad_drift_coef_array[i][5] = config[5];
			dev_info(dev, "[mxt]OK\n");
		} else {
			dev_err(dev, "[mxt]Invalid grad type %d\n", i);
			return -EINVAL;
		}
	} else if (sscanf(buf, "grad: modules(k)=%hd,modules(t)=%hd,b_limit=(low %d high %d highest %d),acc=0x%lx\n",
				&cfg->grad_t_modules,
				&cfg->grad_t_modules,
				&cfg->grad_block_low_limit,
				&cfg->grad_block_high_limit,
				&cfg->grad_block_highest_limit_ratio,
				&cfg->grad_acc_flag)) {
		dev_info(dev, "[mxt]OK\n");
	} else if (sscanf(buf, "grad edge: (%d,%d),(%d,%d)\n",
				&cfg->edge.x0,
				&cfg->edge.y0,
				&cfg->edge.x1,
				&cfg->edge.y1) == 4) {
		dev_info(dev, "[mxt]OK\n");
	} else if (sscanf(buf, "grad limit(%d): %d\n",
				&i,
				&config[0]) == 2) {
		if (i >= 0 && i < T37_PAD3_GRAD_BLOCKS) {
			cfg->grad_block_high_limit_list[i] = config[0];
			dev_info(dev, "[mxt]OK\n");
		}else {
			dev_err(dev, "[mxt]Invalid grad list type %d\n", i);
			return -EINVAL;
		}
	} else if (sscanf(buf, "status: Step=0x%lx,Flag=0x%lx\n",
				&obs->step,
				&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "sf[%d]: count=%d,good=%d,bad=%d,pend=%d\n",
				&i,
				&sf_cnt.count,
				&sf_cnt.good,
				&sf_cnt.bad,
				&sf_cnt.pend) > 0) {
		if (i >= 0 && i < T37_BUFFER_NUMBER) {
			memcpy(&obs->sf_cnt[i], &sf_cnt, sizeof(sf_cnt));
			dev_info(dev, "[mxt] OK\n");
		} else {
			dev_err(dev, "[mxt] Invalid i %d\n", i);
		}
	} else if (sscanf(buf, "grad invalid count: %d %d %d\n",
				&cfg->grad_invalid_count_array[GRAD_INVALID_3],
				&cfg->grad_invalid_count_array[GRAD_INVALID_5],
				&cfg->grad_invalid_count_array[GRAD_INVALID_8]
			) == 3) {
		dev_info(dev, "[mxt]OK\n");
	} else if (sscanf(buf, "[mxt]dist: M%d(%d,%d)\n",
				&i,
				&movement.x,
				&movement.y) == 3) {
		if (i < MOVEMENT_NUM) {
			memcpy(&cfg->movement[i], &movement, sizeof(movement));
			dev_info(dev, "[mxt]OK\n");
		} else {
			dev_err(dev, "[mxt] Invalid i %d\n", i);
		}
	} else {
		dev_err(dev, "[mxt] BAD: %s\n",buf);
	}

	return 0;
}

static int plugin_cal_t37_init(struct plugin_cal *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin cal t37 version 0x%x\n",
			__func__,PLUG_CAL_T37_VERSION);

	p->obs = kzalloc(sizeof(struct t37_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for t37 observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct t37_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for t37 cfg\n");
		kfree(p->obs);
		return -ENOMEM;
	}

	if (init_t37_object(p) != 0) {
		dev_err(dev, "Failed to allocate memory for t37 cfg\n");
		kfree(p->obs);
		kfree(p->cfg);
	}

	return  0;
}

static void plugin_cal_t37_deinit(struct plugin_cal *p)
{
	if (p->obs) {
		deinit_t37_object(p);
		kfree(p->obs);
	}
	if (p->cfg)
		kfree(p->cfg);
}

struct plugin_cal mxt_plugin_cal_t37 =
{
	.init = plugin_cal_t37_init,
	.deinit = plugin_cal_t37_deinit,
	.start = plugin_cal_t37_start,
	.stop = plugin_cal_t37_stop,
	.hook_t6 = plugin_cal_t37_hook_t6,
	.hook_t9 = plugin_cal_t37_hook_t9,
	.hook_t100 = plugin_cal_t37_hook_t100,
	.hook_reset_slots = plugin_cal_t37_reset_slots_hook,
	.pre_process = plugin_cal_t37_pre_process_messages,
	.post_process = plugin_cal_t37_post_process_messages,
	.check_and_calibrate = plugin_cal_t37_check_and_calibrate,
	.show = plugin_cal_t37_show,
	.store = plugin_cal_t37_store,
};

int plugin_cal_init(struct plugin_cal *p)
{
	memcpy(p, &mxt_plugin_cal_t37, sizeof(struct plugin_cal));

	return 0;
}

