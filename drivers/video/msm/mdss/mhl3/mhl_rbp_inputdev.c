/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */
#if (INCLUDE_RBP == 1)
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/hrtimer.h>
#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "platform.h"
#include "mhl_rbp_inputdev.h"

enum rbp_state_e {
	ph0_idle,
	ph3_press_and_hold_button,
	ph8_hold_mode,
	num_rbp_states
};

static char *state_strings[num_rbp_states] = {
	"idle",
	"press_and_hold_button",
	"hold_mode"
};

enum rbp_event_e {
	rbp_normal_button_press,
	rbp_normal_button_press_same,
	rbp_normal_button_release,
	rbp_normal_button_release_same,
	rbp_press_and_hold_button_press,
	rbp_press_and_hold_button_press_same,
	rbp_press_and_hold_button_release,
	rbp_press_and_hold_button_release_same,
	rbp_T_hold_maintain_expired,
	rbp_T_press_mode_expired,
	num_rbp_events
};

static char *event_strings[num_rbp_events] = {
	"normal_button_press",
	"normal_button_press_same",
	"normal_button_release",
	"normal_button_release_same",
	"press_and_hold_button_press",
	"press_and_hold_button_press_same",
	"press_and_hold_button_release",
	"press_and_hold_button_release_same",
	"rbp_T_hold_maintain_expired",
	"rbp_T_press_mode_expired"
};

enum rbp_state_e current_rbp_state = ph0_idle;
uint8_t rbp_previous_button = 0, rbp_current_button = 0;

static int rbp_trigger_button_action(struct mhl_dev_context *dev_context,
				     uint8_t index, bool press_release)
{
	int status = -EINVAL;

	if (dev_context->rbp_input_dev) {
		input_report_key(dev_context->rbp_input_dev, index,
				 press_release);
		input_sync(dev_context->rbp_input_dev);
		status = 0;
	}
	return status;
}

static int handle_rbp_event(struct mhl_dev_context *dev_context,
	uint8_t current_button, uint8_t prev_button, enum rbp_event_e event)
{
	int status = 0;
	uint8_t current_index = current_button & MHL_RBP_BUTTON_ID_MASK;
	uint8_t prev_index = prev_button & MHL_RBP_BUTTON_ID_MASK;

	MHL_TX_DBG_ERR("received 0x%02x: %s(%d) in state: %s(%d)\n",
		       current_button, event_strings[event], event,
		       state_strings[current_rbp_state], current_rbp_state);
	/* now process the event according to the current state */
	switch (current_rbp_state) {
	case ph0_idle:
		switch (event) {
		case rbp_normal_button_press:
		case rbp_normal_button_press_same:
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 1);
			/* no update for current_rbp_state */
			break;
		case rbp_normal_button_release:
		case rbp_normal_button_release_same:
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 0);
			/* no update for current_rbp_state */
			break;
		case rbp_press_and_hold_button_press:
		case rbp_press_and_hold_button_press_same:
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_press_mode,
					   T_PRESS_MODE);
			current_rbp_state = ph3_press_and_hold_button;
			break;

		case rbp_press_and_hold_button_release:
		case rbp_press_and_hold_button_release_same:
			MHL_TX_DBG_ERR("unexpected %s(%d) in state: %s(%d)\n",
				       event_strings[event], event,
				       state_strings[current_rbp_state],
				       current_rbp_state);
			break;
		default:
			MHL_TX_DBG_ERR("unexpected event: %d in state: %d\n",
				       event, current_rbp_state);
			/* no update for current_rbp_state */
			status = -EINVAL;
		}
		break;
	case ph3_press_and_hold_button:
		switch (event) {
		case rbp_normal_button_press:
		case rbp_normal_button_press_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_press_mode);
			rbp_trigger_button_action(dev_context, prev_index, 0);
			/* OK to overwrite status */
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 1);
			current_rbp_state = ph0_idle;
			break;
		case rbp_normal_button_release:
		case rbp_normal_button_release_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_press_mode);
			rbp_trigger_button_action(dev_context, prev_index, 0);
			rbp_trigger_button_action(dev_context, current_index,
						  1);
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 0);
			current_rbp_state = ph0_idle;
			break;
		case rbp_press_and_hold_button_press:
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_press_mode,
					   T_PRESS_MODE);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      1);
			/* no update for current_rbp_state */
			break;
		case rbp_press_and_hold_button_press_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_press_mode);
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_hold_maintain,
					   T_HOLD_MAINTAIN);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      1);
			current_rbp_state = ph8_hold_mode;
			break;
		case rbp_press_and_hold_button_release:
		case rbp_press_and_hold_button_release_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_press_mode);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      0);
			current_rbp_state = ph0_idle;
			break;
		case rbp_T_press_mode_expired:
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_hold_maintain,
					   T_HOLD_MAINTAIN);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      0);
			current_rbp_state = ph8_hold_mode;
			break;
		default:
			MHL_TX_DBG_ERR("unexpected event: %d in state: %d\n",
				       event, current_rbp_state);
			/* no update for current_rbp_state */
			status = -EINVAL;
		}
		break;
	case ph8_hold_mode:
		switch (event) {
		case rbp_normal_button_press:
		case rbp_normal_button_press_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_hold_maintain);
			rbp_trigger_button_action(dev_context, prev_index, 0);
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 1);
			current_rbp_state = ph0_idle;
			break;
		case rbp_normal_button_release:
		case rbp_normal_button_release_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_hold_maintain);
			rbp_trigger_button_action(dev_context, prev_index, 0);
			rbp_trigger_button_action(dev_context, current_index,
						  1);
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 0);
			current_rbp_state = ph0_idle;
			break;
		case rbp_press_and_hold_button_press:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_hold_maintain);
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_press_mode,
					   T_PRESS_MODE);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      1);
			current_rbp_state = ph3_press_and_hold_button;
			break;
		case rbp_press_and_hold_button_press_same:
			mhl_tx_start_timer(dev_context,
					   dev_context->timer_T_hold_maintain,
					   T_HOLD_MAINTAIN);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      1);
			/* no update for current_rbp_state */
			break;
		case rbp_press_and_hold_button_release:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_hold_maintain);
			rbp_trigger_button_action(dev_context, prev_index, 0);
			rbp_trigger_button_action(dev_context, current_index,
						  1);
			status =
			    rbp_trigger_button_action(dev_context,
						      current_index, 0);
			current_rbp_state = ph0_idle;
			break;
		case rbp_press_and_hold_button_release_same:
			mhl_tx_stop_timer(dev_context,
					  dev_context->timer_T_hold_maintain);
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      0);
			current_rbp_state = ph0_idle;
			break;
		case rbp_T_hold_maintain_expired:
			status =
			    rbp_trigger_button_action(dev_context, prev_index,
						      0);
			current_rbp_state = ph0_idle;
			break;
		default:
			MHL_TX_DBG_ERR("unexpected event: %d in state: %d\n",
				       event, current_rbp_state);
			/* no update for current_rbp_state */
			status = -EINVAL;
		}
		break;
	default:
		MHL_TX_DBG_ERR("irrational state value:%d\n",
			current_rbp_state);
	}
	return status;
}

int generate_rbp_input_event(struct mhl_dev_context *dev_context,
	uint8_t rbp_buttoncode)
{
	/*
	   Since, in MHL, bit 7 == 1 indicates button release,
	   and, in Linux, zero means button release,
	   we use XOR (^) to invert the sense.
	 */
	enum rbp_event_e event;
	int mhl_button_press;
	int status = -EINVAL;
	int index = rbp_buttoncode & MHL_RBP_BUTTON_ID_MASK;

	switch (index) {
	case RBP_CALL_ANSWER:
	case RBP_CALL_END:
	case RBP_CALL_TOGGLE:
	case RBP_CALL_MUTE:
	case RBP_CALL_DECLINE:
	case RBP_OCTOTHORPE:
	case RBP_ASTERISK:
	case RBP_ROTATE_CLKWISE:
	case RBP_ROTATE_COUNTERCLKWISE:
	case RBP_SCREEN_PAGE_NEXT:
	case RBP_SCREEN_PAGE_PREV:
	case RBP_SCREEN_PAGE_UP:
	case RBP_SCREEN_PAGE_DN:
	case RBP_SCREEN_PAGE_LEFT:
	case RBP_SCREEN_PAGE_RIGHT:
		break;
	default:
		return 1;
	}

	mhl_button_press =
		(rbp_buttoncode & MHL_RBP_BUTTON_RELEASED_MASK) ? 0 : 1;

	if (mhl_button_press) {
		if (index == rbp_previous_button)
			event = rbp_press_and_hold_button_press_same;
		else
			event = rbp_press_and_hold_button_press;
	} else {
		if (index == rbp_previous_button)
			event = rbp_press_and_hold_button_release_same;
		else
			event = rbp_press_and_hold_button_release;
	}

	status = handle_rbp_event(dev_context, rbp_buttoncode,
		rbp_current_button, event);

	rbp_previous_button = rbp_current_button;
	rbp_current_button = rbp_buttoncode;

	return status;
}
#endif

