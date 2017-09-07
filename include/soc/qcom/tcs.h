/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __SOC_QCOM_TCS_H__
#define __SOC_QCOM_TCS_H__

#define MAX_RPMH_PAYLOAD	16

struct tcs_cmd {
	u32 addr;		/* slv_id:18:16 | offset:0:15 */
	u32 data;		/* data for resource (or read response) */
	bool complete;		/* wait for completion before sending next */
};

enum rpmh_state {
	RPMH_SLEEP_STATE,	/* Sleep */
	RPMH_WAKE_ONLY_STATE,	/* Wake only */
	RPMH_ACTIVE_ONLY_STATE,	/* Active only (= AMC) */
	RPMH_AWAKE_STATE,	/* Use Wake TCS for Wake & Active (AMC = 0) */
};

struct tcs_mbox_msg {
	enum rpmh_state state;	/* request state */
	bool is_complete;	/* wait for resp from accelerator */
	bool is_read;		/* expecting a response from RPMH */
	bool is_control;	/* private control messages */
	bool invalidate;	/* invalidate sleep/wake commands */
	u32 num_payload;	/* Limited to MAX_RPMH_PAYLOAD in one msg */
	struct tcs_cmd *payload;/* array of tcs_cmds */
};

#endif /* __SOC_QCOM_TCS_H__ */
