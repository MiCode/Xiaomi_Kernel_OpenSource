/* Copyright (c) 2009-2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_MSM_RPC_PMAPP_H
#define __ASM_ARCH_MSM_RPC_PMAPP_H

#include <mach/msm_rpcrouter.h>

/* Clock voting ids */
enum {
	PMAPP_CLOCK_ID_DO = 0,
	PMAPP_CLOCK_ID_D1,
	PMAPP_CLOCK_ID_A0,
	PMAPP_CLOCK_ID_A1,
};

/* Clock voting types */
enum {
	PMAPP_CLOCK_VOTE_OFF = 0,
	PMAPP_CLOCK_VOTE_ON,
	PMAPP_CLOCK_VOTE_PIN_CTRL,
};

/* vreg ids */
enum {
	PMAPP_VREG_LDO22 = 14,
	PMAPP_VREG_S3 = 21,
	PMAPP_VREG_S2 = 23,
	PMAPP_VREG_S4 = 24,
};

/* SMPS clock voting types */
enum {
	PMAPP_SMPS_CLK_VOTE_DONTCARE = 0,
	PMAPP_SMPS_CLK_VOTE_2P74,	/* 2.74 MHz */
	PMAPP_SMPS_CLK_VOTE_1P6,	/* 1.6 MHz */
};

/* SMPS mode voting types */
enum {
	PMAPP_SMPS_MODE_VOTE_DONTCARE = 0,
	PMAPP_SMPS_MODE_VOTE_PWM,
	PMAPP_SMPS_MODE_VOTE_PFM,
	PMAPP_SMPS_MODE_VOTE_AUTO
};

int msm_pm_app_rpc_init(void(*callback)(int online));
void msm_pm_app_rpc_deinit(void(*callback)(int online));
int msm_pm_app_register_vbus_sn(void (*callback)(int online));
void msm_pm_app_unregister_vbus_sn(void (*callback)(int online));
int msm_pm_app_enable_usb_ldo(int);
int pmic_vote_3p3_pwr_sel_switch(int boost);

int pmapp_display_clock_config(uint enable);

int pmapp_clock_vote(const char *voter_id, uint clock_id, uint vote);
int pmapp_smps_clock_vote(const char *voter_id, uint vreg_id, uint vote);
int pmapp_vreg_level_vote(const char *voter_id, uint vreg_id, uint level);
int pmapp_smps_mode_vote(const char *voter_id, uint vreg_id, uint mode);
int pmapp_vreg_pincntrl_vote(const char *voter_id, uint vreg_id,
					uint clock_id, uint vote);
int pmapp_disp_backlight_set_brightness(int value);
void pmapp_disp_backlight_init(void);
int pmapp_vreg_lpm_pincntrl_vote(const char *voter_id, uint vreg_id,
					uint clock_id, uint vote);
#endif
