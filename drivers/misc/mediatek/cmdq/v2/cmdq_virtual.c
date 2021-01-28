// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_device.h"
#include "cmdq_virtual.h"
#include <linux/seq_file.h>
#ifdef CMDQ_CONFIG_SMI
#include "smi_debug.h"
#endif
#ifdef CMDQ_CG_M4U_LARB0
#include "m4u.h"
#endif

static struct cmdqCoreFuncStruct gFunctionPointer;

uint64_t cmdq_virtual_flag_from_scenario_default(
	enum CMDQ_SCENARIO_ENUM scn)
{
	uint64_t flag = 0;

	switch (scn) {
	case CMDQ_SCENARIO_JPEG_DEC:
		flag = (1LL << CMDQ_ENG_JPEG_DEC);
		break;

	case CMDQ_SCENARIO_SUB_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1));
		break;

	case CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL:
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
		flag = 0LL;
		break;
	case CMDQ_SCENARIO_TRIGGER_LOOP:
	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
		/* Trigger loop does not related to any HW by itself. */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_USER_SPACE:
		/* user space case, engine flag is passed seprately */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DEBUG:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		/* ESD check uses separate thread (not config, not trigger) */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* color path */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		/* secure path */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_SECURE_NOTIFY_LOOP:
		flag = 0LL;
		break;

	default:
		if (scn < 0 || scn >= CMDQ_MAX_SCENARIO_COUNT) {
			/* Error status print */
			CMDQ_ERR("Unknown scenario type %d\n", scn);
		}
		flag = 0LL;
		break;
	}

	return flag;
}

const char *cmdq_virtual_parse_module_from_reg_addr_legacy(
	uint32_t reg_addr)
{
	const uint32_t addr_base_and_page = (reg_addr & 0xFFFFF000);

	/* for well-known base, we check them with 12-bit mask */
	/* defined in mt_reg_base.h */
	/* TODO: comfirm with SS if IO_VIRT_TO_PHYS */
	/* workable when enable device tree? */
	switch (addr_base_and_page) {
	case 0x14000000:
		return "MMSYS";
	case 0x14001000:
		return "MDP_RDMA0";
	case 0x14002000:
		return "MDP_RDMA1";
	case 0x14003000:
		return "MDP_RSZ0";
	case 0x14004000:
		return "MDP_RSZ1";
	case 0x14005000:
		return "MDP_RSZ2";
	case 0x14006000:
		return "MDP_WDMA";
	case 0x14007000:
		return "MDP_WROT0";
	case 0x14008000:
		return "MDP_WROT1";
	case 0x14009000:
		return "MDP_TDSHP0";
	case 0x1400A000:
		return "MDP_TDSHP1";
	case 0x1400B000:
		return "MDP_CROP";
	case 0x1400C000:
		return "DISP_OVL0";
	case 0x1400D000:
		return "DISP_OVL1";
	case 0x14013000:
		return "COLOR0";
	case 0x14014000:
		return "COLOR1";
	case 0x14015000:
		return "AAL";
	case 0x14016000:
		return "GAMA";
	case 0x14020000:
		return "MMSYS_MUTEX";
	case 0x18000000:
		return "VENC_GCON";
	case 0x18002000:
		return "VENC";
	case 0x18003000:
		return "JPGENC";
	case 0x18004000:
		return "JPGDEC";
	}

	/* for other register address we rely */
	/* on GCE subsys to group them with */
	/* 16-bit mask. */
	return cmdq_core_parse_subsys_from_reg_addr(reg_addr);
}

uint64_t cmdq_virtual_flag_from_scenario_legacy(
	enum CMDQ_SCENARIO_ENUM scn)
{
	uint64_t flag = 0;

	switch (scn) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
		flag = (1LL << CMDQ_ENG_DISP_OVL0) |
		    (1LL << CMDQ_ENG_DISP_COLOR0) |
		    (1LL << CMDQ_ENG_DISP_AAL) |
		    (1LL << CMDQ_ENG_DISP_RDMA0) |
		    (1LL << CMDQ_ENG_DISP_UFOE) |
		    (1LL << CMDQ_ENG_DISP_DSI0_CMD);
		break;
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
			(1LL << CMDQ_ENG_DISP_WDMA0));
		break;
	case CMDQ_SCENARIO_PRIMARY_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
			(1LL << CMDQ_ENG_DISP_WDMA0) |
			(1LL << CMDQ_ENG_DISP_COLOR0) |
			(1LL << CMDQ_ENG_DISP_AAL) |
			(1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) |
			(1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_SUB_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
#ifdef CONFIG_MTK_CMDQ_TAB
	case CMDQ_SCENARIO_SUB_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1));
		break;
#endif
	case CMDQ_SCENARIO_SUB_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
	case CMDQ_SCENARIO_MHL_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_RDMA0_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) |
			(1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_RDMA2_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA2) |
			(1LL << CMDQ_ENG_DISP_DPI));
		break;
	default:
		flag = 0LL;
		break;
	}

	return flag;
}

/*
 * GCE capability
 */
uint32_t cmdq_virtual_get_subsys_LSB_in_arg_a(void)
{
	return 16;
}

/* HW thread related */
bool cmdq_virtual_is_a_secure_thread(const int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if ((thread >= CMDQ_MIN_SECURE_THREAD_ID) &&
		(thread < CMDQ_MIN_SECURE_THREAD_ID +
		CMDQ_MAX_SECURE_THREAD_COUNT)) {
		return true;
	}
#endif
	return false;
}

bool cmdq_virtual_is_valid_notify_thread_for_secure_path(
	const int32_t thread)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
	return (thread == 15) ? (true) : (false);
#else
	return false;
#endif
}

/**
 * Scenario related
 *
 */
bool cmdq_virtual_is_disp_scenario(
	const enum CMDQ_SCENARIO_ENUM scenario)
{
	bool dispScenario = false;

	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	case CMDQ_SCENARIO_TRIGGER_LOOP:
	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		dispScenario = true;
		break;
	default:
		break;
	}
	/* freely dispatch */
	return dispScenario;
}

bool cmdq_virtual_should_enable_prefetch(
	enum CMDQ_SCENARIO_ENUM scenario)
{
	bool shouldPrefetch = false;

	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* any path that connects to Primary DISP HW */
		/* should enable prefetch. */
		/* MEMOUT scenarios does not. */
		/* Also, since thread 0/1 shares one prefetch buffer, */
		/* we allow only PRIMARY path to use prefetch. */
		shouldPrefetch = true;
		break;
	default:
		break;
	}

	return shouldPrefetch;
}

bool cmdq_virtual_should_profile(enum CMDQ_SCENARIO_ENUM scenario)
{
	bool shouldProfile = false;

#ifdef CMDQ_GPR_SUPPORT
	switch (scenario) {
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
	case CMDQ_SCENARIO_DEBUG:
		return true;
	default:
		break;
	}
#else
	/* note command profile method depends on GPR */
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
	return shouldProfile;
}

int cmdq_virtual_disp_thread(enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* primary config: thread 0 */
		return 0;

	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_SUB_MEMOUT:
#ifdef CMDQ_DISP_LEGACY_SUB_SCENARIO
		/* when HW thread 0 enables pre-fetch, */
		/* any thread 1 operation will let HW */
		/* thread 0's behavior abnormally */
		/* forbid thread 1 */
		return 5;
#else
		return 1;
#endif

	case CMDQ_SCENARIO_MHL_DISP:
		return 5;

	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
		return 2;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
		return 6;

	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		return 3;

	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		return 4;
	default:
		/* freely dispatch */
		return CMDQ_INVALID_THREAD;
	}
	/* freely dispatch */
	return CMDQ_INVALID_THREAD;
}

int cmdq_virtual_get_thread_index(enum CMDQ_SCENARIO_ENUM scenario,
	const bool secure)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
	if (!secure && CMDQ_SCENARIO_SECURE_NOTIFY_LOOP == scenario)
		return 15;
#endif

	if (!secure)
		return cmdq_get_func()->dispThread(scenario);

	/* dispatch secure thread according to scenario */
	switch (scenario) {
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* CMDQ_MIN_SECURE_THREAD_ID */
		return CMDQ_THREAD_SEC_PRIMARY_DISP;
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
		/* because mirror mode and sub disp never use */
		/* at the same time in secure path, */
		/* dispatch to same HW thread */
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	/* tablet use */
	case CMDQ_SCENARIO_SUB_MEMOUT:
		return CMDQ_THREAD_SEC_SUB_DISP;
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_USER_SPACE:
	case CMDQ_SCENARIO_DEBUG:
		/* because there is one input engine for MDP, */
		/* reserve one secure thread is enough */
		return CMDQ_THREAD_SEC_MDP;
	default:
		CMDQ_ERR("no dedicated secure thread for scenario:%d\n",
			scenario);
		return CMDQ_INVALID_THREAD;
	}
}

enum CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_virtual_priority_from_scenario(
	enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		/* currently, a prefetch thread is always in high priority. */
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

		/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return CMDQ_THR_PRIO_DISPLAY_ESD;

	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERHIGH;

	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERLOW;

	default:
		/* other cases need exta logic, see below. */
		break;
	}

	if (cmdq_get_func()->is_disp_loop(scenario))
		return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	else
		return CMDQ_THR_PRIO_NORMAL;
}

bool cmdq_virtual_force_loop_irq(enum CMDQ_SCENARIO_ENUM scenario)
{
	bool force_loop = false;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (scenario == CMDQ_SCENARIO_SECURE_NOTIFY_LOOP) {
		/* For secure notify loop, we need IRQ to update secure task */
		force_loop = true;
	}
#endif
	if (scenario == CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP
		|| scenario == CMDQ_SCENARIO_LOWP_TRIGGER_LOOP) {
		/* For monitor thread loop, */
		/* we need IRQ to set callback function */
		force_loop = true;
	}

	return force_loop;
}

bool cmdq_virtual_is_disp_loop(enum CMDQ_SCENARIO_ENUM scenario)
{
	bool is_disp_loop = false;

	if (scenario == CMDQ_SCENARIO_TRIGGER_LOOP)
		is_disp_loop = true;

	return is_disp_loop;
}

/**
 * Module dependent
 *
 */
void cmdq_virtual_get_reg_id_from_hwflag(uint64_t hwflag,
	enum CMDQ_DATA_REGISTER_ENUM *valueRegId,
	enum CMDQ_DATA_REGISTER_ENUM *destRegId,
	enum CMDQ_EVENT_ENUM *regAccessToken)
{
	*regAccessToken = CMDQ_SYNC_TOKEN_INVALID;

	if (hwflag & (1LL << CMDQ_ENG_JPEG_ENC)) {
		*valueRegId = CMDQ_DATA_REG_JPEG;
		*destRegId = CMDQ_DATA_REG_JPEG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_0;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_0;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_0_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_1;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP1)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_1;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_1_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_2;
	} else if (hwflag & ((1LL << CMDQ_ENG_DISP_COLOR0 |
			(1LL << CMDQ_ENG_DISP_COLOR1)))) {
		*valueRegId = CMDQ_DATA_REG_PQ_COLOR;
		*destRegId = CMDQ_DATA_REG_PQ_COLOR_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_3;
	} else {
		/* assume others are debug cases */
		*valueRegId = CMDQ_DATA_REG_DEBUG;
		*destRegId = CMDQ_DATA_REG_DEBUG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	}
}

const char *cmdq_virtual_module_from_event_id(const int32_t event,
	struct CmdqCBkStruct *groupCallback, uint64_t engineFlag)
{
	const char *module = "CMDQ";
	enum CMDQ_GROUP_ENUM group = CMDQ_MAX_GROUP_COUNT;

	switch (event) {
	case CMDQ_EVENT_DISP_RDMA0_SOF:
	case CMDQ_EVENT_DISP_RDMA1_SOF:
	case CMDQ_EVENT_DISP_RDMA2_SOF:
	case CMDQ_EVENT_DISP_RDMA0_EOF:
	case CMDQ_EVENT_DISP_RDMA1_EOF:
	case CMDQ_EVENT_DISP_RDMA2_EOF:
	case CMDQ_EVENT_DISP_RDMA0_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA1_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA2_UNDERRUN:
		module = "DISP_RDMA";
		group = CMDQ_GROUP_DISP;
		break;

	case CMDQ_EVENT_DISP_WDMA0_SOF:
	case CMDQ_EVENT_DISP_WDMA1_SOF:
	case CMDQ_EVENT_DISP_WDMA0_EOF:
	case CMDQ_EVENT_DISP_WDMA1_EOF:
		module = "DISP_WDMA";
		group = CMDQ_GROUP_DISP;
		break;

	case CMDQ_EVENT_DISP_OVL0_SOF:
	case CMDQ_EVENT_DISP_OVL1_SOF:
	case CMDQ_EVENT_DISP_2L_OVL0_SOF:
	case CMDQ_EVENT_DISP_2L_OVL1_SOF:
	case CMDQ_EVENT_DISP_OVL0_EOF:
	case CMDQ_EVENT_DISP_OVL1_EOF:
	case CMDQ_EVENT_DISP_2L_OVL0_EOF:
	case CMDQ_EVENT_DISP_2L_OVL1_EOF:
		module = "DISP_OVL";
		group = CMDQ_GROUP_DISP;
		break;

	case CMDQ_EVENT_UFOD_RAMA0_L0_SOF ... CMDQ_EVENT_UFOD_RAMA1_L3_SOF:
	case CMDQ_EVENT_UFOD_RAMA0_L0_EOF ... CMDQ_EVENT_UFOD_RAMA1_L3_EOF:
		module = "DISP_UFOD";
		group = CMDQ_GROUP_DISP;
		break;

	case CMDQ_EVENT_DSI_TE:
	case CMDQ_EVENT_DSI0_TE:
	case CMDQ_EVENT_DSI1_TE:
	case CMDQ_EVENT_MDP_DSI0_TE_SOF:
	case CMDQ_EVENT_MDP_DSI1_TE_SOF:
	case CMDQ_EVENT_DISP_DSI0_EOF:
	case CMDQ_EVENT_DISP_DSI1_EOF:
	case CMDQ_EVENT_DISP_DPI0_EOF:
	case CMDQ_EVENT_DISP_COLOR_SOF ... CMDQ_EVENT_DISP_DSC_SOF:
	case CMDQ_EVENT_DISP_COLOR_EOF ... CMDQ_EVENT_DISP_DSC_EOF:
	case CMDQ_EVENT_MUTEX0_STREAM_EOF ... CMDQ_EVENT_MUTEX4_STREAM_EOF:
		module = "DISP";
		group = CMDQ_GROUP_DISP;
		break;
	case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
	case CMDQ_SYNC_TOKEN_STREAM_EOF:
		module = "DISP";
		group = CMDQ_GROUP_DISP;
		break;

	case CMDQ_EVENT_MDP_RDMA0_SOF ... CMDQ_EVENT_MDP_CROP_SOF:
	case CMDQ_EVENT_MDP_RDMA0_EOF ... CMDQ_EVENT_MDP_CROP_EOF:
	case CMDQ_EVENT_MUTEX5_STREAM_EOF ... CMDQ_EVENT_MUTEX9_STREAM_EOF:
		module = "MDP";
		group = CMDQ_GROUP_MDP;
		break;

	case CMDQ_EVENT_ISP_PASS2_2_EOF ... CMDQ_EVENT_ISP_PASS1_0_EOF:
	case CMDQ_EVENT_DIP_CQ_THREAD0_EOF ... CMDQ_EVENT_DIP_CQ_THREAD14_EOF:
	case CMDQ_EVENT_DPE_EOF:
	case CMDQ_EVENT_WMF_EOF:
	case CMDQ_EVENT_ISP_SENINF_CAM1_2_3_FULL:
	case CMDQ_EVENT_ISP_SENINF_CAM0_FULL:
	case CMDQ_EVENT_ISP_FRAME_DONE_A:
	case CMDQ_EVENT_ISP_FRAME_DONE_B:
	case CMDQ_EVENT_ISP_CAMSV_0_PASS1_DONE:
	case CMDQ_EVENT_ISP_CAMSV_1_PASS1_DONE:
	case CMDQ_EVENT_ISP_CAMSV_2_PASS1_DONE:
	case CMDQ_EVENT_SENINF_0_FIFO_FULL ... CMDQ_EVENT_SENINF_7_FIFO_FULL:
		module = "ISP";
		group = CMDQ_GROUP_ISP;
		break;

	case CMDQ_EVENT_JPEG_ENC_EOF:
	case CMDQ_EVENT_JPEG_ENC_PASS2_EOF:
	case CMDQ_EVENT_JPEG_ENC_PASS1_EOF:
	case CMDQ_EVENT_JPEG_DEC_EOF:
		module = "JPGE";
		group = CMDQ_GROUP_JPEG;
		break;

	case CMDQ_EVENT_VENC_EOF:
	case CMDQ_EVENT_VENC_MB_DONE:
	case CMDQ_EVENT_VENC_128BYTE_CNT_DONE:
		module = "VENC";
		group = CMDQ_GROUP_VENC;
		break;

	default:
		module = "CMDQ";
		group = CMDQ_MAX_GROUP_COUNT;
		break;
	}

	if (group < CMDQ_MAX_GROUP_COUNT && groupCallback[group].dispatchMod)
		module = groupCallback[group].dispatchMod(engineFlag);

	return module;
}

const char *cmdq_virtual_parse_module_from_reg_addr(
	uint32_t reg_addr)
{
#ifdef CMDQ_USE_LEGACY
	return cmdq_virtual_parse_module_from_reg_addr_legacy(reg_addr);
#else
	const uint32_t addr_base_and_page = (reg_addr & 0xFFFFF000);

	/* for well-known base, we check them with 12-bit mask */
	/* defined in mt_reg_base.h */
	/* TODO: comfirm with SS if IO_VIRT_TO_PHYS */
	/* workable when enable device tree? */
	switch (addr_base_and_page) {
	case 0x14001000: /* MDP_RDMA */
	case 0x14002000: /* MDP_RSZ0 */
	case 0x14003000: /* MDP_RSZ1 */
	case 0x14004000: /* MDP_WDMA */
	case 0x14005000: /* MDP_WROT */
	case 0x14006000: /* MDP_TDSHP */
		return "MDP";
	case 0x1400C000: /* DISP_COLOR */
		return "COLOR";
	case 0x1400D000: /* DISP_COLOR */
		return "CCORR";
	case 0x14007000: /* DISP_OVL0 */
		return "OVL0";
	case 0x14008000: /* DISP_OVL1 */
		return "OVL1";
	case 0x1400E000: /* DISP_AAL */
	case 0x1400F000: /* DISP_GAMMA */
		return "AAL";
	case 0x17002FFF: /* VENC */
		return "VENC";
	case 0x17003FFF: /* JPGENC */
		return "JPGENC";
	case 0x17004FFF: /* JPGDEC */
		return "JPGDEC";
	}

	/* for other register address we rely */
	/* on GCE subsys to group them with */
	/* 16-bit mask. */
	return cmdq_core_parse_subsys_from_reg_addr(reg_addr);
#endif
}

int32_t cmdq_virtual_can_module_entry_suspend(
	struct EngineStruct *engineList)
{
	int32_t status = 0;
	int i;
	enum CMDQ_ENG_ENUM e = 0;

	enum CMDQ_ENG_ENUM mdpEngines[] = {
		CMDQ_ENG_ISP_IMGI,
		CMDQ_ENG_MDP_RDMA0,
		CMDQ_ENG_MDP_RDMA1,
		CMDQ_ENG_MDP_RSZ0,
		CMDQ_ENG_MDP_RSZ1,
		CMDQ_ENG_MDP_RSZ2,
		CMDQ_ENG_MDP_TDSHP0,
		CMDQ_ENG_MDP_TDSHP1,
		CMDQ_ENG_MDP_COLOR0,
		CMDQ_ENG_MDP_WROT0,
		CMDQ_ENG_MDP_WROT1,
		CMDQ_ENG_MDP_WDMA
	};

	for (i = 0; i < ARRAY_SIZE(mdpEngines); ++i) {
		e = mdpEngines[i];
		if (engineList[e].userCount != 0) {
			CMDQ_ERR("suspend but eng:%d userCount:%d,owner:%d\n",
				 e, engineList[e].userCount,
				 engineList[e].currOwner);
			status = -EBUSY;
		}
	}

	return status;
}

ssize_t cmdq_virtual_print_status_clock(char *buf)
{
	int32_t length = 0;
	char *pBuffer = buf;

#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	pBuffer += sprintf(pBuffer, "MT_CG_INFRA_GCE: %d\n",
		cmdq_dev_gce_clock_is_enable());

	pBuffer += sprintf(pBuffer, "\n");
#endif

	length = pBuffer - buf;
	return length;
}

void cmdq_virtual_print_status_seq_clock(struct seq_file *m)
{
#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	seq_printf(m, "MT_CG_INFRA_GCE: %d", cmdq_dev_gce_clock_is_enable());

	seq_puts(m, "\n");
#endif
}

void cmdq_virtual_enable_common_clock_locked(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable SMI & LARB0 Clock\n");
		cmdq_dev_enable_clock_SMI_COMMON(enable);
#ifdef CMDQ_CG_M4U_LARB0
		m4u_larb0_enable("CMDQ_MDP");
#else
		cmdq_dev_enable_clock_SMI_LARB0(enable);
#endif
#ifdef CONFIG_MTK_CMDQ_TAB
		cmdq_mdp_get_func()->mdpSmiLarbEnableClock(enable);
#endif
#ifdef CMDQ_USE_LEGACY
		CMDQ_VERBOSE("[CLOCK] enable MT_CG_DISP0_MUTEX_32K\n");
		cmdq_mdp_get_func()->mdpEnableClockMutex32k(enable);
#endif
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable SMI & LARB0 Clock\n");
		/* disable, reverse the sequence */
#ifdef CMDQ_CG_M4U_LARB0
		m4u_larb0_disable("CMDQ_MDP");
#else
		cmdq_dev_enable_clock_SMI_LARB0(enable);
#endif
		cmdq_dev_enable_clock_SMI_COMMON(enable);
#ifdef CMDQ_USE_LEGACY
		CMDQ_VERBOSE("[CLOCK] disable MT_CG_DISP0_MUTEX_32K\n");
		cmdq_mdp_get_func()->mdpEnableClockMutex32k(enable);
#endif
#ifdef CONFIG_MTK_CMDQ_TAB
		cmdq_mdp_get_func()->mdpSmiLarbEnableClock(enable);
#endif
	}
#endif				/* CMDQ_PWR_AWARE */
}

void cmdq_virtual_enable_gce_clock_locked(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock\n");
		cmdq_dev_enable_gce_clock(enable);
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock\n");
		cmdq_dev_enable_gce_clock(enable);
	}
#endif
}

const char *cmdq_virtual_parse_error_module_by_hwflag_impl(
	const struct TaskStruct *pTask)
{
	const char *module = NULL;
	const uint32_t ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) |
		 (1LL << CMDQ_ENG_ISP_IMGO))
	};

	/* common part for both normal and secure path */
	/* for JPEG scenario, use HW flag is sufficient */
	if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
		module = "JPGENC";
	else if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
		module = "JPGDEC";
	else if ((ISP_ONLY[0] == pTask->engineFlag) ||
		(ISP_ONLY[1] == pTask->engineFlag))
		module = "ISP_ONLY";
	else if (cmdq_get_func()->isDispScenario(pTask->scenario))
		module = "DISP";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (false == pTask->secData.is_secure) {
			/* normal path, need parse current */
			/* running instruciton for more detail */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(pTask->engineFlag)) {
			module = "MDP";
			break;
		} else if (CMDQ_ENG_DPE_GROUP_FLAG(pTask->engineFlag)) {
			module = "DPE";
			break;
		}

		module = "CMDQ";
	} while (0);

	/* other case, we need to analysis instruction for more detail */
	return module;
}

/**
 * Debug
 *
 */
void cmdq_virtual_dump_mmsys_config(void)
{
	/* do nothing */
}

void cmdq_virtual_dump_clock_gating(void)
{
	uint32_t value[3] = { 0 };

	value[0] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x100);
	value[1] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x110);
	CMDQ_ERR("MMSYS_CG_CON0(deprecated): 0x%08x, MMSYS_CG_CON1: 0x%08x\n",
		value[0], value[1]);
#ifdef CMDQ_USE_LEGACY
	value[2] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x890);
	CMDQ_ERR("MMSYS_DUMMY_REG: 0x%08x\n", value[2]);
#endif
}

int cmdq_virtual_dump_smi(const int showSmiDump)
{
	int isSMIHang = 0;

#if defined(CMDQ_CONFIG_SMI) && !defined(CONFIG_MTK_FPGA) && \
	!defined(CONFIG_MTK_SMI_VARIANT)
	isSMIHang =
		smi_debug_bus_hanging_detect_ext2(SMI_DBG_DISPSYS |
		SMI_DBG_VDEC | SMI_DBG_IMGSYS |
		SMI_DBG_VENC | SMI_DBG_MJC,
		showSmiDump, showSmiDump, 1);
	CMDQ_ERR("SMI Hang? = %d\n", isSMIHang);
#else
	CMDQ_LOG("[WARNING]not enable SMI dump now\n");
#endif

	return isSMIHang;
}

void cmdq_virtual_dump_gpr(void)
{
	int i = 0;
	long offset = 0;
	uint32_t value = 0;

	CMDQ_LOG("========= GPR dump =========\n");
	for (i = 0; i < 16; i++) {
		offset = CMDQ_GPR_R32(i);
		value = CMDQ_REG_GET32(offset);
		CMDQ_LOG("[GPR %2d]+0x%lx = 0x%08x\n", i, offset, value);
	}
	CMDQ_LOG("========= GPR dump =========\n");
}


/**
 * Record usage
 *
 */

uint64_t cmdq_virtual_flag_from_scenario(enum CMDQ_SCENARIO_ENUM scn)
{
	uint64_t flag = 0;

#ifdef CMDQ_USE_LEGACY
	flag = cmdq_virtual_flag_from_scenario_legacy(scn);
#else
	switch (scn) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
		flag = (1LL << CMDQ_ENG_DISP_OVL0) |
		    (1LL << CMDQ_ENG_DISP_COLOR0) |
		    (1LL << CMDQ_ENG_DISP_AAL) |
		    (1LL << CMDQ_ENG_DISP_GAMMA) |
		    (1LL << CMDQ_ENG_DISP_RDMA0) |
		    (1LL << CMDQ_ENG_DISP_UFOE) |
		    (1LL << CMDQ_ENG_DISP_DSI0_CMD);
		break;
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		flag = 0LL;
		break;
	case CMDQ_SCENARIO_PRIMARY_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
			(1LL << CMDQ_ENG_DISP_WDMA0) |
			(1LL << CMDQ_ENG_DISP_COLOR0) |
			(1LL << CMDQ_ENG_DISP_AAL) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) |
			(1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_SUB_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_SUB_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1) |
			(1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_RDMA0_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_COLOR0) |
			(1LL << CMDQ_ENG_DISP_AAL) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_UFOE) |
			(1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA1_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA1) |
			(1LL << CMDQ_ENG_DISP_DPI));
		break;
	default:
		flag = 0LL;
		break;
	}
#endif
	if (flag == 0)
		cmdq_virtual_flag_from_scenario_default(scn);

	return flag;
}

/**
 * Event backup
 *
 */
struct cmdq_backup_event_struct {
	enum CMDQ_EVENT_ENUM EventID;
	uint32_t BackupValue;
};

static struct cmdq_backup_event_struct g_cmdq_backup_event[] = {
#ifdef CMDQ_EVENT_NEED_BACKUP
	{CMDQ_SYNC_TOKEN_VENC_EOF, 0,},
	{CMDQ_SYNC_TOKEN_VENC_INPUT_READY, 0,},
#endif				/* CMDQ_EVENT_NEED_BACKUP */
#ifdef CMDQ_EVENT_SVP_BACKUP
	{CMDQ_SYNC_DISP_OVL0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_OVL1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_2LOVL0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_2LOVL1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_RDMA0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_RDMA1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_WDMA0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_WDMA1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_EXT_STREAM_EOF, 0,},
#endif				/* CMDQ_EVENT_SVP_BACKUP */
};


void cmdq_virtual_event_backup(void)
{
	int i;
	int array_size = (sizeof(g_cmdq_backup_event) /
		sizeof(struct cmdq_backup_event_struct));

	for (i = 0; i < array_size; i++) {
		if (g_cmdq_backup_event[i].EventID < 0 ||
			g_cmdq_backup_event[i].EventID >= CMDQ_SYNC_TOKEN_MAX)
			continue;

		g_cmdq_backup_event[i].BackupValue =
			cmdqCoreGetEvent(g_cmdq_backup_event[i].EventID);
		CMDQ_MSG("[backup event] event: %s, value: %d\n",
			cmdq_core_get_event_name_ENUM(
				g_cmdq_backup_event[i].EventID),
				g_cmdq_backup_event[i].BackupValue);
	}
}

void cmdq_virtual_event_restore(void)
{
	int i;
	int array_size = (sizeof(g_cmdq_backup_event) /
		sizeof(struct cmdq_backup_event_struct));

	for (i = 0; i < array_size; i++) {
		if (g_cmdq_backup_event[i].EventID < 0 ||
			g_cmdq_backup_event[i].EventID >= CMDQ_SYNC_TOKEN_MAX)
			continue;

		CMDQ_MSG("[restore event] event: %s, value: %d\n",
			cmdq_core_get_event_name_ENUM(
				g_cmdq_backup_event[i].EventID),
			g_cmdq_backup_event[i].BackupValue);

		if (g_cmdq_backup_event[i].BackupValue == 1)
			cmdqCoreSetEvent(g_cmdq_backup_event[i].EventID);
		else if (g_cmdq_backup_event[i].BackupValue == 0)
			cmdqCoreClearEvent(g_cmdq_backup_event[i].EventID);
	}
}

/**
 * Test
 *
 */
void cmdq_virtual_test_setup(void)
{
	/* unconditionally set CMDQ_SYNC_TOKEN_CONFIG_ALLOW */
	/* and mutex STREAM_DONE */
	/* so that DISPSYS scenarios may pass check. */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);
}

void cmdq_virtual_test_cleanup(void)
{
	/* do nothing */
}

void cmdq_virtual_init_module_PA_stat(void)
{
#if defined(CMDQ_OF_SUPPORT) && defined(CMDQ_INSTRUCTION_COUNT)
	int32_t i;
	CmdqModulePAStatStruct *modulePAStat =
		cmdq_core_Initial_and_get_module_stat();

	/* Get MM_SYS config registers range */
	cmdq_dev_get_module_PA("mediatek,mmsys_config", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MMSYS_CONFIG],
		&modulePAStat->end[CMDQ_MODULE_STAT_MMSYS_CONFIG]);
	/* Get MDP module registers range */
	cmdq_dev_get_module_PA("mediatek,mdp_rdma", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_RDMA],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_RDMA]);
	cmdq_dev_get_module_PA("mediatek,mdp_rsz0", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_RSZ0],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_RSZ0]);
	cmdq_dev_get_module_PA("mediatek,mdp_rsz1", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_RSZ1],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_RSZ1]);
	cmdq_dev_get_module_PA("mediatek,mdp_wdma", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_WDMA],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_WDMA]);
	cmdq_dev_get_module_PA("mediatek,mdp_wrot", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_WROT],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_WROT]);
	cmdq_dev_get_module_PA("mediatek,mdp_tdshp", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MDP_TDSHP],
		&modulePAStat->end[CMDQ_MODULE_STAT_MDP_TDSHP]);
	cmdq_dev_get_module_PA("mediatek,MM_MUTEX", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_MM_MUTEX],
		&modulePAStat->end[CMDQ_MODULE_STAT_MM_MUTEX]);
	cmdq_dev_get_module_PA("mediatek,VENC", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_VENC],
		&modulePAStat->end[CMDQ_MODULE_STAT_VENC]);
	/* Get DISP module registers range */
	for (i = CMDQ_MODULE_STAT_DISP_OVL0;
		i <= CMDQ_MODULE_STAT_DISP_DPI0; i++) {
		cmdq_dev_get_module_PA("mediatek,DISPSYS",
			(i - CMDQ_MODULE_STAT_DISP_OVL0),
			&modulePAStat->start[i], &modulePAStat->end[i]);
	}
	cmdq_dev_get_module_PA("mediatek,DISPSYS", 31,
		&modulePAStat->start[CMDQ_MODULE_STAT_DISP_OD],
		&modulePAStat->end[CMDQ_MODULE_STAT_DISP_OD]);
	/* Get CAM module registers range */
	cmdq_dev_get_module_PA("mediatek,CAM0", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_CAM0],
		&modulePAStat->end[CMDQ_MODULE_STAT_CAM0]);
	cmdq_dev_get_module_PA("mediatek,CAM1", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_CAM1],
		&modulePAStat->end[CMDQ_MODULE_STAT_CAM1]);
	cmdq_dev_get_module_PA("mediatek,CAM2", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_CAM2],
		&modulePAStat->end[CMDQ_MODULE_STAT_CAM2]);
	cmdq_dev_get_module_PA("mediatek,CAM3", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_CAM3],
		&modulePAStat->end[CMDQ_MODULE_STAT_CAM3]);
	/* Get SODI registers range */
	cmdq_dev_get_module_PA("mediatek,SLEEP", 0,
		&modulePAStat->start[CMDQ_MODULE_STAT_SODI],
		&modulePAStat->end[CMDQ_MODULE_STAT_SODI]);
#endif
}

void cmdq_virtual_function_setting(void)
{
	struct cmdqCoreFuncStruct *pFunc;

	pFunc = &(gFunctionPointer);

	/*
	 * GCE capability
	 */
	pFunc->getSubsysLSBArgA = cmdq_virtual_get_subsys_LSB_in_arg_a;

	/* HW thread related */
	pFunc->isSecureThread = cmdq_virtual_is_a_secure_thread;
	pFunc->isValidNotifyThread =
		cmdq_virtual_is_valid_notify_thread_for_secure_path;

	/**
	 * Scenario related
	 *
	 */
	pFunc->isDispScenario = cmdq_virtual_is_disp_scenario;
	pFunc->shouldEnablePrefetch = cmdq_virtual_should_enable_prefetch;
	pFunc->shouldProfile = cmdq_virtual_should_profile;
	pFunc->dispThread = cmdq_virtual_disp_thread;
	pFunc->getThreadID = cmdq_virtual_get_thread_index;
	pFunc->priority = cmdq_virtual_priority_from_scenario;
	pFunc->force_loop_irq = cmdq_virtual_force_loop_irq;
	pFunc->is_disp_loop = cmdq_virtual_is_disp_loop;

	/**
	 * Module dependent
	 *
	 */
	pFunc->getRegID = cmdq_virtual_get_reg_id_from_hwflag;
	pFunc->moduleFromEvent = cmdq_virtual_module_from_event_id;
	pFunc->parseModule = cmdq_virtual_parse_module_from_reg_addr;
	pFunc->moduleEntrySuspend = cmdq_virtual_can_module_entry_suspend;
	pFunc->printStatusClock = cmdq_virtual_print_status_clock;
	pFunc->printStatusSeqClock = cmdq_virtual_print_status_seq_clock;
	pFunc->enableCommonClockLocked =
		cmdq_virtual_enable_common_clock_locked;
	pFunc->enableGCEClockLocked = cmdq_virtual_enable_gce_clock_locked;
	pFunc->parseErrorModule =
		cmdq_virtual_parse_error_module_by_hwflag_impl;

	/**
	 * Debug
	 *
	 */
	pFunc->dumpMMSYSConfig = cmdq_virtual_dump_mmsys_config;
	pFunc->dumpClockGating = cmdq_virtual_dump_clock_gating;
	pFunc->dumpSMI = cmdq_virtual_dump_smi;
	pFunc->dumpGPR = cmdq_virtual_dump_gpr;

	/**
	 * Record usage
	 *
	 */
	pFunc->flagFromScenario = cmdq_virtual_flag_from_scenario;

	/**
	 * Event backup
	 *
	 */
	pFunc->eventBackup = cmdq_virtual_event_backup;
	pFunc->eventRestore = cmdq_virtual_event_restore;

	/**
	 * Test
	 *
	 */
	pFunc->testSetup = cmdq_virtual_test_setup;
	pFunc->testCleanup = cmdq_virtual_test_cleanup;
	pFunc->initModulePAStat = cmdq_virtual_init_module_PA_stat;
}

struct cmdqCoreFuncStruct *cmdq_get_func(void)
{
	return &gFunctionPointer;
}
