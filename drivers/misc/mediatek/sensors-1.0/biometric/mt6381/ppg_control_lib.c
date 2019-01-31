/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include "mt6381.h"
#include "ppg_control.h"
#include "ppg_control_lib_setting.h"

#if defined(PPG_CTRL_DEBUG_FLAG)
#include "stdio.h"
#endif


/* Setting variables */
INT32 ppg_control_adjust_flag[PPG_CTRL_CH_MAX];
INT32 ppg_control_led_current[PPG_CTRL_CH_MAX];

INT32 ppg_dc_limit_h;
INT32 ppg_dc_limit_l;

/* data input buffer */
INT32 ppg_ctrl_buf_idx[PPG_CTRL_CH_MAX];	/* buffer index */
INT32 ppg_ctrl_in_count[PPG_CTRL_CH_MAX];	/* down sample use */
INT32 ppg_ctrl_buf[PPG_CTRL_CH_MAX][PPG_CTRL_BUF_SIZE];

/* saturate check */
INT32 ppg_ctrl_cnt_pos_edge[PPG_CTRL_CH_MAX];
INT32 ppg_ctrl_cnt_neg_edge[PPG_CTRL_CH_MAX];

/* Filter variables */
INT32 ppg_control_hpf[PPG_CTRL_CH_MAX];

/* state */
INT32 ppg_ctrl_cur_state[PPG_CTRL_CH_MAX];
INT32 ppg_ctrl_pre_state[PPG_CTRL_CH_MAX];

/* timer control */
INT32 ppg_control_timer[PPG_CTRL_CH_MAX];
INT32 ppg_ctrl_time_limit;
INT32 ppg_control_init_flag[PPG_CTRL_CH_MAX];


/* functions */
UINT32 ppg_control_get_version(void)
{
	return (PPG_CTRL_VER_CODE_1 << 24) + (PPG_CTRL_VER_CODE_2 << 16) +
	    (PPG_CTRL_VER_CODE_3 << 8) + (PPG_CTRL_VER_CODE_4);
}

enum ppg_control_status_t ppg_control_init(void)
{
	INT32 j, ch;

	for (ch = 0; ch < PPG_CTRL_CH_MAX; ch++) {
		ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_RESET;
		ppg_ctrl_pre_state[ch] = PPG_CONTROL_STATE_RESET;
		ppg_control_timer[ch] = 0;
		ppg_control_init_flag[ch] = 0;
		ppg_ctrl_cnt_pos_edge[ch] = 0;
		ppg_ctrl_cnt_neg_edge[ch] = 0;

		/* ppg buffer control */
		ppg_ctrl_buf_idx[ch] = 0;
		ppg_ctrl_in_count[ch] = 0;

		for (j = 0; j < PPG_CTRL_BUF_SIZE; j++)
			ppg_ctrl_buf[ch][j] = 0;
	}

	/* set AFE boundary */
	ppg_dc_limit_h = PPG_DC_MAXP;
	ppg_dc_limit_l = PPG_DC_MAXN;

	/* set time limit */
	ppg_ctrl_time_limit = PPG_CONTROL_TIME_LIMIT;

	/* initial setting */
	ppg_control_led_current[0] = PPG_INIT_LED1;
	ppg_control_led_current[1] = PPG_INIT_LED2;

	/* set driver */
#if defined(PPG_CTRL_DRIVER_ON)
	ppg_ctrl_set_driver_ch(0);
#endif				/* #if defined(PPG_CTRL_DRIVER_ON) */

	return PPG_CONTROL_STATUS_OK;
}


INT32 ppg_control_process(struct ppg_control_t *ppg_control_input)
{
	INT32 *input = ppg_control_input->input;
	INT32 *input_amb = ppg_control_input->input_amb;
	INT32 fs_input = ppg_control_input->input_fs;
	INT32 ppg_sample_length = ppg_control_input->input_length;
	INT32 ch = (ppg_control_input->input_source == 1) ? 0 : 1;

	/* variables */
	INT32 i, j;
	INT32 value = 0;
	INT32 value_ac = 0;
	INT32 down_ratio;

	/* timing control */
	INT32 flag_check_time_limit = 0;
	INT32 led_step = 0;

	/* window value */
	INT32 window_ac_min = PPG_DC_POS_EDGE << 1;
	INT32 window_ac_max = PPG_DC_NEG_EDGE << 1;
	INT32 window_dc_min = PPG_DC_POS_EDGE << 1;
	INT32 window_dc_max = PPG_DC_NEG_EDGE << 1;

	/* reset flag */
	ppg_control_adjust_flag[ch] = 0;

	/* timer update */
	ppg_control_timer[ch] += ppg_sample_length;

#if defined(LOG_PPG_CONTROL_ENABLE)
	pr_notice("%s, timer%d=%d, state=%d, led=%d, idx=%d, fs=%d, l=%d\n",
		"MT6381_AGC start", ch,
		ppg_control_timer[ch], ppg_ctrl_cur_state[ch],
		ppg_control_led_current[ch],
		ppg_ctrl_buf_idx[ch], fs_input, ppg_sample_length);
#endif				/* #if defined(LOG_PPG_CONTROL_ENABLE) */

	/* down sample step size calc. */
	switch (fs_input) {
	case 128:
		down_ratio = 1 << 3;
		break;
	case 256:
		down_ratio = 1 << 4;
		break;
	case 512:
		down_ratio = 1 << 5;
		break;
	default:
		down_ratio = 1;
		break;
	}

	/* buffer control and DC&AC estimation */
	for (i = 0; i < ppg_sample_length; i++) {

		/* Downsampling. PPG DC estimation */
		if (ppg_ctrl_in_count[ch] == 0) {
			/* current input is LED-AMB,
			 *value will become LED phase
			 */
			value = input[i] + input_amb[i];

#if defined(LOG_PPG_CONTROL_ENABLE)
			pr_notice("%s, PPG%d = %d, LED = %d, AMB = %d, idx = %d\n",
				"MT6381_AGC input", ch,
				input[i], value, input_amb[i],
				ppg_ctrl_buf_idx[ch]);
#endif

			/* buffer update */
			ppg_ctrl_buf[ch][ppg_ctrl_buf_idx[ch]] = value;

			/* saturate count */
			if (value > ppg_dc_limit_h)
				ppg_ctrl_cnt_pos_edge[ch]++;
			else if (value < ppg_dc_limit_l)
				ppg_ctrl_cnt_neg_edge[ch]++;
			/* init filter setting */
			if (ppg_control_init_flag[ch] == 0) {
				ppg_control_hpf[ch] =
					value << PPG_CONTROL_HPF_ORDER;
				ppg_control_init_flag[ch] = 1;
			}
			/* buffer pointer update */
			ppg_ctrl_buf_idx[ch]++;
			if (down_ratio == 1)
				ppg_ctrl_in_count[ch] = 0;
			else
				ppg_ctrl_in_count[ch]++;
		} else if (ppg_ctrl_in_count[ch] >= down_ratio - 1) {
			ppg_ctrl_in_count[ch] = 0;
		} else {
			ppg_ctrl_in_count[ch]++;
		}

		/* check saturate */
		if (ppg_ctrl_cnt_pos_edge[ch] > PPG_SATURATE_HANDLE_COUNT) {
			ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_SAT_P;
			ppg_control_adjust_flag[ch] = 1;

		} else if (ppg_ctrl_cnt_neg_edge[ch] >
			PPG_SATURATE_HANDLE_COUNT) {
			ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_SAT_N;
			ppg_control_adjust_flag[ch] = 1;

		} else if (ppg_ctrl_buf_idx[ch] == PPG_CTRL_FS_OPERATE) {
			ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_NON_SAT;

			for (j = 0; j < PPG_CTRL_FS_OPERATE; j++) {
				/* find dc max */
				value = ppg_ctrl_buf[ch][j];
				if (value > window_dc_max)
					window_dc_max = value;
				if (value < window_dc_min)
					window_dc_min = value;
				/* find ac max */
				ppg_control_hpf[ch] +=
				    ((ppg_ctrl_buf[ch][j] * 4) -
				     ppg_control_hpf[ch]) >>
				     PPG_CONTROL_HPF_ORDER;
				value_ac =
				    ppg_ctrl_buf[ch][j] -
				    (ppg_control_hpf[ch] >>
				    PPG_CONTROL_HPF_ORDER);
				if (value_ac > window_ac_max)
					window_ac_max = value_ac;
				if (value_ac < window_ac_min)
					window_ac_min = value_ac;
			}	/* end AC update */
			value_ac = window_ac_max - window_ac_min;

			/* if (window_dc_max < PPG_DC_ENLARGE_BOUND &&
			 *	value_ac < PPG_AC_TARGET) {
			 *	ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_INC;
			 *	ppg_control_adjust_flag[ch] = 1;
			 * }	else if(window_dc_min >
			 *	(PPG_DC_POS_EDGE - (PPG_DC_POS_EDGE>>3))) {
			 *	 ppg_ctrl_cur_state[ch] = PPG_CONTROL_STATE_DEC;
			 *	 ppg_control_adjust_flag[ch] = 1;
			 *	 }
			 */
		}
		/* reset */
		if (ppg_control_adjust_flag[ch] == 1 ||
			ppg_ctrl_buf_idx[ch] >= PPG_CTRL_FS_OPERATE) {
			ppg_ctrl_buf_idx[ch] = 0;
			ppg_ctrl_cnt_pos_edge[ch] = 0;
			ppg_ctrl_cnt_neg_edge[ch] = 0;
		}
	}			/* end i: buffer control and state check */

	/* timer check */
	if (ppg_ctrl_time_limit == PPG_CONTROL_ALWAYS_ON
	    || (ppg_control_timer[ch] < (ppg_ctrl_time_limit * fs_input))) {
		flag_check_time_limit = 0;
	} else {
		flag_check_time_limit = 1;	/* stop adjustment */
	}

	/* Adjustment stage */
	if (flag_check_time_limit == 0 && ppg_control_adjust_flag[ch] == 1) {

		if (ppg_ctrl_cur_state[ch] == PPG_CONTROL_STATE_SAT_P) {
			/* LED decrease */
			if (ppg_ctrl_pre_state[ch] == PPG_CONTROL_STATE_SAT_N ||
				ppg_ctrl_pre_state[ch] ==
				PPG_CONTROL_STATE_INC) {
				led_step = -PPG_LED_STEP_FINE;
			} else {
				led_step = -PPG_LED_STEP_COARSE;
			}
		} else if (ppg_ctrl_cur_state[ch] == PPG_CONTROL_STATE_SAT_N) {
			/* LED increase */
			if (ppg_ctrl_pre_state[ch] == PPG_CONTROL_STATE_SAT_P ||
				ppg_ctrl_pre_state[ch] ==
				PPG_CONTROL_STATE_DEC) {
				led_step = PPG_LED_STEP_FINE;
			} else {
				led_step = PPG_LED_STEP_COARSE;
			}
		} else if (ppg_ctrl_cur_state[ch] == PPG_CONTROL_STATE_INC) {
			/* LED increase */
			if (window_dc_max < (PPG_DC_NEG_EDGE >> 2))
				led_step = PPG_LED_STEP_COARSE;
			else if (window_dc_max < (PPG_DC_NEG_EDGE >> 1))
				led_step = PPG_LED_STEP_FINE;
			else
				led_step = PPG_LED_STEP_MIN;
		} else if (ppg_ctrl_cur_state[ch] == PPG_CONTROL_STATE_DEC)
			;
		/* reset buffers/timers */
		ppg_ctrl_buf_idx[ch] = 0;

		/* check upper bound */
		ppg_control_led_current[ch] += led_step;
		if (ppg_control_led_current[ch] > PPG_MAX_LED_CURRENT)
			ppg_control_led_current[ch] = PPG_MAX_LED_CURRENT;
		else if (ppg_control_led_current[ch] < PPG_MIN_LED_CURRENT)
			ppg_control_led_current[ch] = PPG_MIN_LED_CURRENT;

#if defined(LOG_PPG_CONTROL_ENABLE)
		pr_notice("MT6381_AGC write, state%d = %d->%d, led = %d, step = %d\n",
			ch, ppg_ctrl_pre_state[ch], ppg_ctrl_cur_state[ch],
			ppg_control_led_current[ch], led_step);
#endif

#if defined(PPG_CTRL_DRIVER_ON)
		/*if(ch==0) { //PPG1
		 * vsm_driver_set_led_current(VSM_LED_1,
		 *	VSM_SIGNAL_PPG1, ppg_control_led_current[ch]);
		 * } else {
		 * vsm_driver_set_led_current(VSM_LED_2,
		 *	VSM_SIGNAL_PPG2, ppg_control_led_current[ch]);
		 * }
		 */
		ppg_ctrl_set_driver_ch(ch);
		ppg_ctrl_pre_state[ch] = ppg_ctrl_cur_state[ch];
#endif				/* #if defined(PPG_CTRL_DRIVER_ON) */

	}

	return ppg_control_adjust_flag[ch];
}


#if defined(PPG_CTRL_DRIVER_ON)

void ppg_ctrl_set_driver_ch(INT32 ch)
{
	UINT32 ppg_reg_value;
	struct bus_data_t ppg_reg_info;

	/* write register: LED current (MT6381: 0x332C) */
	ppg_reg_value = ppg_control_led_current[0] +
		(ppg_control_led_current[1] << 8);
	ppg_reg_info.addr = 0x33;
	ppg_reg_info.reg = 0x2C;
	ppg_reg_info.data_buf = (uint8_t *) &ppg_reg_value;
	ppg_reg_info.length = sizeof(ppg_reg_value);
	vsm_driver_write_register(&ppg_reg_info);
	vsm_driver_update_register();
}

#endif


INT32 ppg_control_get_status(INT32 ppg_control_internal_config)
{
	INT32 status_out;

	switch (ppg_control_internal_config) {
	default:
		status_out = 0;
	}

	return status_out;
}
