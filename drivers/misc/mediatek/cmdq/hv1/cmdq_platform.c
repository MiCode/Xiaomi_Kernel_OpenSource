/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "cmdq_platform.h"
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_device.h"

#include <linux/vmalloc.h>
/* #include <mach/mt_clkmgr.h> */
#include <linux/seq_file.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_chip.h>

const bool cmdq_core_support_sync_non_suspendable(void)
{
	return true;
}

const bool cmdq_core_support_wait_and_receive_event_in_same_tick(void)
{
	enum chip_sw_ver ver = mt_get_chip_sw_ver();
	bool support = false;

	if (CHIP_SW_VER_02 <= ver) {
		/* SW V2 */
		support = true;
	} else if (CHIP_SW_VER_01 <= ver) {
		support = false;
	}

	return support;
}

const uint32_t cmdq_core_get_subsys_LSB_in_argA(void)
{
	return 16;
}

bool cmdq_core_is_request_from_user_space(const enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_USER_DISP_COLOR:
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_USER_SPACE:	/* phased out */
		return true;
	default:
		return false;
	}
	return false;
}

bool cmdq_core_is_disp_scenario(const enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
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

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		return true;
	default:
		return false;
	}
	/* freely dispatch */
	return false;
}

bool cmdq_core_should_enable_prefetch(enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:	/* HACK: force debug into 0/1 thread */
		/* any path that connects to Primary DISP HW */
		/* should enable prefetch. */
		/* MEMOUT scenarios does not. */
		/* Also, since thread 0/1 shares one prefetch buffer, */
		/* we allow only PRIMARY path to use prefetch. */
		return true;

	default:
		return false;
	}

	return false;
}

bool cmdq_core_should_profile(enum CMDQ_SCENARIO_ENUM scenario)
{
#ifdef CMDQ_GPR_SUPPORT
		/* may need to modify
	switch (scenario) {
		   case CMDQ_SCENARIO_PRIMARY_DISP:
		   case CMDQ_SCENARIO_PRIMARY_ALL:
		   case CMDQ_SCENARIO_DEBUG_PREFETCH:
		   case CMDQ_SCENARIO_DEBUG:
		   return true;
	default:
		return false;
	}
		 */
	return false;
#else
	/* note command profile method depends on GPR */
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return false;
#endif
}

const bool cmdq_core_is_a_secure_thread(const int32_t thread)
{
/*
**	secure HW Thread 12/13/14
**	12:disp primary path
**	13:disp subdisplay path
**	14:MDP path
*/
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if ((CMDQ_MIN_SECURE_THREAD_ID <= thread) &&
	    (CMDQ_MIN_SECURE_THREAD_ID + CMDQ_MAX_SECURE_THREAD_COUNT > thread)) {
		return true;
	}
#endif
	return false;
}

/*
**	HW thread 15 is normal world & secure world IRQ notify thread
*/
const bool cmdq_core_is_valid_notify_thread_for_secure_path(const int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	return (15 == thread) ? (true) : (false);
#else
	return false;
#endif
}

int cmdq_core_get_thread_index_from_scenario_and_secure_data(enum CMDQ_SCENARIO_ENUM scenario,
							     const bool secure)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (!secure && CMDQ_SCENARIO_SECURE_NOTIFY_LOOP == scenario)
		return 15;

#endif

	if (!secure)
		return cmdq_core_disp_thread_index_from_scenario(scenario);


	/* dispatch secure thread according to scenario */
	switch (scenario) {
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* CMDQ_MIN_SECURE_THREAD_ID */
		return 12;
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
		/* because mirror mode and sub disp never use at the same time in secure path, */
		/* dispatch to same HW thread */
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		return 13;
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_USER_SPACE:
	case CMDQ_SCENARIO_DEBUG:
		/* because there is one input engine for MDP, reserve one secure thread is enough */
		return 14;
	default:
		CMDQ_ERR("no dedicated secure thread for senario:%d\n", scenario);
		return CMDQ_INVALID_THREAD;
	}
}

int cmdq_core_disp_thread_index_from_scenario(enum CMDQ_SCENARIO_ENUM scenario)
{
	if (cmdq_core_should_enable_prefetch(scenario))
		return 0;


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
	case CMDQ_SCENARIO_DEBUG_PREFETCH:	/* HACK: force debug into 0/1 thread */
		/* primary config: thread 0 */
		return 0;

	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
		/* when HW thread 0 enables pre-fetch, */
		/* any thread 1 operation will let HW thread 0's behavior abnormally */
		/* forbid thread 1 */
		return 5;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
		return 2;

	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		return 3;

	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		return 4;
	default:
		/* freely dispatch */
		return CMDQ_INVALID_THREAD;
	}

	/* freely dispatch */
	return CMDQ_INVALID_THREAD;
}

enum CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL ... CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* secure path * */
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

	default:
		/* other cases need exta logic, see below. */
		break;
	}

	if (CMDQ_SCENARIO_TRIGGER_LOOP == scenario)
		return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	else
		return CMDQ_THR_PRIO_NORMAL;

}

ssize_t cmdq_core_print_status_clock(char *buf)
{
	int32_t length = 0;
	char *pBuffer = buf;

#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	pBuffer +=
	    sprintf(pBuffer, "MT_CG_INFRA_GCE: %d\n", cmdq_core_clock_is_on(CMDQ_CLK_INFRA_GCE));
#endif
	length = pBuffer - buf;
	return length;
}

void cmdq_core_print_status_seq_clock(struct seq_file *m)
{
#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	seq_printf(m, "MT_CG_INFRA_GCE: %d\n", cmdq_core_clock_is_on(CMDQ_CLK_INFRA_GCE));
#endif
}

void cmdq_core_enable_gce_clock_locked_impl(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock\n");
		cmdq_core_enable_cmdq_clock_locked_impl(enable, CMDQ_DRIVER_DEVICE_NAME);
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock\n");
		cmdq_core_enable_cmdq_clock_locked_impl(enable, CMDQ_DRIVER_DEVICE_NAME);
	}
#endif				/* CMDQ_PWR_AWARE */
}

void cmdq_core_enable_cmdq_clock_locked_impl(bool enable, char *deviceName)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		/* enable_clock(MT_CG_INFRA_GCE, deviceName); */
		cmdq_core_enable_ccf_clk(CMDQ_CLK_INFRA_GCE);

	} else {
		/* disable_clock(MT_CG_INFRA_GCE, deviceName); */
		cmdq_core_disable_ccf_clk(CMDQ_CLK_INFRA_GCE);
	}
#endif				/* CMDQ_PWR_AWARE */
}

const char *cmdq_core_parse_error_module_by_hwflag_impl(struct TaskStruct *pTask)
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
	else if ((ISP_ONLY[0] == pTask->engineFlag) || (ISP_ONLY[1] == pTask->engineFlag))
		module = "ISP_ONLY";


	/* for secure path, use HW flag is sufficient */
	do {
		if (false == pTask->secData.isSecure) {
			/* normal path, need parse current running instruciton for more detail */
			break;
		}

		/* check module group to dispatch it */
		if (cmdq_core_is_disp_scenario(pTask->scenario)) {
			module = "DISP";
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(pTask->engineFlag)) {
			module = "MDP";
			break;
		}

		module = "CMDQ";
	} while (0);

	/* other case, we need to analysis instruction for more detail */
	return module;
}

void cmdq_core_dump_clock_gating(void)
{
	uint32_t value[3] = { 0 };

	value[0] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE_VA + 0x100);
	value[1] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE_VA + 0x110);
	value[2] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE_VA + 0x890);
	CMDQ_ERR("MMSYS_CG_CON0(deprecated): 0x%08x, MMSYS_CG_CON1: 0x%08x\n", value[0], value[1]);
	CMDQ_ERR("MMSYS_DUMMY_REG: 0x%08x\n", value[2]);
#ifndef CONFIG_MTK_FPGA
/* CMDQ_ERR("ISPSys clock state %d\n", subsys_is_on(SYS_ISP)); */
/* CMDQ_ERR("DisSys clock state %d\n", subsys_is_on(SYS_DIS)); */
/* CMDQ_ERR("VDESys clock state %d\n", subsys_is_on(SYS_VDE)); */
/*
	CMDQ_ERR("ISPSys clock state %d\n", cmdq_core_subsys_is_on(CMDQ_SUBSYSCLK_SYS_ISP));
	CMDQ_ERR("DisSys clock state %d\n", cmdq_core_subsys_is_on(CMDQ_SUBSYSCLK_SYS_DIS));
	CMDQ_ERR("VDESys clock state %d\n", cmdq_core_subsys_is_on(CMDQ_SUBSYSCLK_SYS_VDE));
*/
#endif
}

int cmdq_core_dump_smi(const int showSmiDump)
{
	int isSMIHang = 0;
#if defined(CMDQ_CONFIG_SMI) && !defined(CONFIG_MTK_FPGA)
	/*isSMIHang = smi_debug_bus_hanging_detect(
	   SMI_DBG_DISPSYS | SMI_DBG_VDEC | SMI_DBG_IMGSYS | SMI_DBG_VENC | SMI_DBG_MJC,
	   showSmiDump); */
	CMDQ_ERR("SMI Hang? = %d\n", isSMIHang);
#else
	CMDQ_LOG("[WARNING]not enable SMI dump now\n");
#endif
	return isSMIHang;
}

void cmdq_core_dump_secure_metadata(cmdqSecDataStruct *pSecData)
{
	uint32_t i = 0;
	cmdqSecAddrMetadataStruct *pAddr = NULL;

	if (NULL == pSecData)
		return;


	pAddr = (cmdqSecAddrMetadataStruct *) (CMDQ_U32_PTR(pSecData->addrMetadatas));

	CMDQ_LOG("========= pSecData: %p dump =========\n", pSecData);
	CMDQ_LOG("metaData count:%d(%d), enginesNeedDAPC:0x%llx, enginesPortSecurity:0x%llx\n",
		 pSecData->addrMetadataCount, pSecData->addrMetadataMaxCount,
		 pSecData->enginesNeedDAPC, pSecData->enginesNeedPortSecurity);

	if (NULL == pAddr)
		return;


	for (i = 0; i < pSecData->addrMetadataCount; i++) {
		CMDQ_MSG
		    ("meta idx:%d, inst idx:%d, type:%d, baseHandle:%x, offset:0x%08x, size:%d, port:%d\n",
		     i, pAddr[i].instrIndex, pAddr[i].type, pAddr[i].baseHandle, pAddr[i].offset,
		     pAddr[i].size, pAddr[i].port);
	}
}


uint64_t cmdq_rec_flag_from_scenario(enum CMDQ_SCENARIO_ENUM scn)
{
	uint64_t flag = 0;

	switch (scn) {
	case CMDQ_SCENARIO_JPEG_DEC:
		flag = (1LL << CMDQ_ENG_JPEG_DEC);
		break;
	case CMDQ_SCENARIO_PRIMARY_DISP:
		flag = (1LL << CMDQ_ENG_DISP_OVL0) |
		    (1LL << CMDQ_ENG_DISP_COLOR0) |
		    (1LL << CMDQ_ENG_DISP_AAL) |
		    (1LL << CMDQ_ENG_DISP_RDMA0) |
		    (1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD);
		break;
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) | (1LL << CMDQ_ENG_DISP_WDMA0));
		break;
	case CMDQ_SCENARIO_PRIMARY_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
			(1LL << CMDQ_ENG_DISP_WDMA0) |
			(1LL << CMDQ_ENG_DISP_COLOR0) |
			(1LL << CMDQ_ENG_DISP_AAL) |
			(1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_SUB_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
	case CMDQ_SCENARIO_SUB_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) | (1LL << CMDQ_ENG_DISP_WDMA1));
		break;
	case CMDQ_SCENARIO_SUB_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
	case CMDQ_SCENARIO_MHL_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_RDMA0_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_RDMA2_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA2) | (1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_TRIGGER_LOOP:
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
		CMDQ_ERR("Unknown scenario type %d\n", scn);
		flag = 0LL;
		break;
	}

	return flag;
}
