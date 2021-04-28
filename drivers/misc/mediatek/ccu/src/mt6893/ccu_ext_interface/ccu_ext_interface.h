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

#ifndef __CCU_INTERFACE__
#define __CCU_INTERFACE__

#include "ccu_ext_interface/ccu_types.h"

extern MBOOL sec_vsync_pushed;

/******************************************************************************
 * Task definition
 *****************************************************************************/
enum ccu_msg_id {
	DELIMITER_SYSCTRL_MSG_MIN = 0, /*To identify ccu control msg count*/
	/*Receive by CCU*/
	MSG_TO_CCU_IDLE = DELIMITER_SYSCTRL_MSG_MIN,
	MSG_TO_CCU_SUSPEND, /*Request CCU to suspend on corresponding TG*/
	MSG_TO_CCU_RESUME, /*Request CCU to resume on corresponding TG*/
	MSG_TO_CCU_SHUTDOWN, /*Request CCU to shutdown*/
	MSG_TO_CCU_UPDATE_TG_SENSOR_MAP,

	DELIMITER_SYSCTRL_MSG_1, /*To identify ccu control msg count*/
	/*CCU internal task generated in HW isr*/
	MSG_CCU_INTERNAL_VSYNC_SYS = DELIMITER_SYSCTRL_MSG_1,
	MSG_CCU_INTERNAL_P1_DONE_SYS,
	MSG_CCU_INTERNAL_PR_LOG,
	MSG_CCU_INTERNAL_FORCE_SHUTDOWN, /*Request CCU to shutdown*/

	DELIMITER_SYSCTRL_MSG_MAX /*To identify ccu control msg count*/
};

enum ccu_to_ap_msg_id {
	MSG_TO_APMCU_FLUSH_LOG, //CCU Request APMCU to print out CCU logs
	MSG_TO_APMCU_CCU_ASSERT, //CCU inform APMCU that CCU ASSERT occurs
	MSG_TO_APMCU_CCU_WARNING, //CCU inform APMCU that CCU WARNING occurs
	MSG_TO_APMCU_CAM_AFO_i
};

enum ccu_feature_type {
	CCU_FEATURE_UNDEF = 0x0,
	CCU_FEATURE_MIN = 0x1,
	CCU_FEATURE_AE = CCU_FEATURE_MIN,
	CCU_FEATURE_AF,
	CCU_FEATURE_LTM,
	CCU_FEATURE_3ACTRL,
	CCU_FEATURE_SYSCTRL,
	CCU_FEATURE_MAX,
};

enum ccu_tg_info {
	CCU_CAM_TG_NONE = 0x0,
	CCU_CAM_TG_MIN  = 0x1,
	CCU_CAM_TG_1    = 0x1,
	CCU_CAM_TG_2    = 0x2,
	CCU_CAM_TG_3    = 0x3,
	CCU_CAM_TG_MAX
};

struct ccu_msg {
	enum ccu_feature_type feature_type;
	uint32_t msg_id;
	MUINT32 in_data_ptr;
	MUINT32 out_data_ptr;
	enum ccu_tg_info tg_info;
	uint32_t sensor_idx;
};

struct ccu_control_info {
	enum ccu_feature_type feature_type;
	uint32_t sensor_idx; //new
	uint32_t msg_id;
	void *inDataPtr;
	uint32_t inDataSize;
	void *outDataPtr;
	uint32_t outDataSize;
};

struct ap2ccu_ipc_t {
	MUINT32 write_cnt;
	MUINT32 read_cnt;
	struct ccu_msg msg;
	MBOOL ack;
};

/******************************************************************************
 * Status definition
 *****************************************************************************/
#define CCU_STATUS_INIT_DONE              0xffff0000
#define CCU_STATUS_INIT_DONE_2            0xffff00a5

#endif
