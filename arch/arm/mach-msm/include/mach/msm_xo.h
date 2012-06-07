/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MACH_MSM_XO_H
#define __MACH_MSM_XO_H

enum msm_xo_ids {
	MSM_XO_TCXO_D0,
	MSM_XO_TCXO_D1,
	MSM_XO_TCXO_A0,
	MSM_XO_TCXO_A1,
	MSM_XO_TCXO_A2,
	MSM_XO_CORE,
	NUM_MSM_XO_IDS
};

enum msm_xo_modes {
	MSM_XO_MODE_OFF,
	MSM_XO_MODE_PIN_CTRL,
	MSM_XO_MODE_ON,
	NUM_MSM_XO_MODES
};

struct msm_xo_voter;

#ifdef CONFIG_MSM_XO
struct msm_xo_voter *msm_xo_get(enum msm_xo_ids xo_id, const char *voter);
void msm_xo_put(struct msm_xo_voter *xo_voter);
int msm_xo_mode_vote(struct msm_xo_voter *xo_voter, enum msm_xo_modes xo_mode);
int __init msm_xo_init(void);
#else
static inline struct msm_xo_voter *msm_xo_get(enum msm_xo_ids xo_id,
		const char *voter)
{
	return NULL;
}

static inline void msm_xo_put(struct msm_xo_voter *xo_voter) { }

static inline int msm_xo_mode_vote(struct msm_xo_voter *xo_voter,
		enum msm_xo_modes xo_mode)
{
	return 0;
}
static inline int msm_xo_init(void) { return 0; }
#endif /* CONFIG_MSM_XO */

#endif
