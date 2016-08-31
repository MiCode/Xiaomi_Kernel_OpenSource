/*
 * hdmi_state_machine.h
 *
 * HDMI library support functions for Nvidia Tegra processors.
 *
 * Copyright (C) 2013 Google - http://www.google.com/
 * Copyright (C) 2013, NVIDIA CORPORATION. All rights reserved.
 * Authors:	John Grossman <johngro@google.com>
 * Authors:	Mike J. Chen <mjchen@google.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TEGRA_HDMI_STATE_MACHINE_H
#define __TEGRA_HDMI_STATE_MACHINE_H

#include "hdmi.h"

enum {
	/* The initial state for the state machine.  When entering RESET, we
	 * shut down all output and then proceed to the CHECK_PLUG state after a
	 * short debounce delay.
	 */
	HDMI_STATE_RESET = 0,

	/* After the debounce delay, check the status of the HPD line.  If its
	 * low, then the cable is unplugged and we go directly to DONE_DISABLED.
	 * If it is high, then the cable is plugged and we proceed to CHECK_EDID
	 * in order to read the EDID and figure out the next step.
	 */
	HDMI_STATE_CHECK_PLUG_STATE,

	/* CHECK_EDID is the state we stay in attempting to read the EDID
	 * information after we check the plug state and discover that we are
	 * plugged in.  If we max out our retries and fail to read the EDID, we
	 * move to DONE_DISABLED.  If we successfully read the EDID, we move on
	 * to DONE_ENABLE, set an initial video mode, then signal to the high
	 * level that we are ready for final mode selection.
	 */
	HDMI_STATE_CHECK_EDID,

	/* DONE_DISABLED is the state we stay in after being reset and either
	 * discovering that no cable is plugged in or after we think a cable is
	 * plugged in but fail to read EDID.
	 */
	HDMI_STATE_DONE_DISABLED,

	/* DONE_ENABLED is the state we say in after being reset and disovering
	 * a valid EDID at the other end of a plugged cable.
	 */
	HDMI_STATE_DONE_ENABLED,

	/* Some sinks will drop HPD as soon as the TMDS signals start up.  They
	 * will hold HPD low for about second and then re-assert it.  If the
	 * source simply holds steady and does not disable the TMDS lines, the
	 * sink seems to accept the video mode after having gone out to lunch
	 * for a bit.  This seems to be the behavior of various sources which
	 * work with panels like this, so it is the behavior we emulate here.
	 * If HPD drops while we are in DONE_ENABLED, set a timer for 1.5
	 * seconds and transition to WAIT_FOR_HPD_REASSERT.  If HPD has not come
	 * back within this time limit, then go ahead and transition to RESET
	 * and shut the system down.  If HPD does come back within this time
	 * limit, then check the EDID again.  If it has not changed, then we
	 * assume that we are still hooked to the same panel and just go back to
	 * DONE_ENABLED.  If the EDID fails to read or has changed, we
	 * transition to RESET and start the system over again.
	 */
	HDMI_STATE_DONE_WAIT_FOR_HPD_REASSERT,

	/* RECHECK_EDID is the state we stay in while attempting to re-read the
	 * EDID following an HPD drop and re-assert which occurs while we are in
	 * the DONE_ENABLED state.  see HDMI_STATE_DONE_WAIT_FOR_HPD_REASSERT
	 * for more details.
	 */
	HDMI_STATE_DONE_RECHECK_EDID,

	/* STATE_COUNT must be the final state in the enum.
	 * 1) Do not add states after STATE_COUNT.
	 * 2) Do not assign explicit values to the states.
	 * 3) Do not reorder states in the list without reordering the dispatch
	 *    table in hdmi_state_machine.c
	 */
	HDMI_STATE_COUNT,
};

void hdmi_state_machine_init(struct tegra_dc_hdmi_data *hdmi);
void hdmi_state_machine_shutdown(void);
void hdmi_state_machine_set_pending_hpd(void);
int hdmi_state_machine_get_state(void);

#endif  /* __TEGRA_HDMI_STATE_MACHINE_H */
