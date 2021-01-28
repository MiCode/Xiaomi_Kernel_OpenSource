/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_INTERFACE__
#define __CCU_INTERFACE__

#include "ccu_ext_interface/ccu_types.h"

extern MBOOL sec_vsync_pushed;

/********************************************************************
 * Task definition
 ********************************************************************/
enum ccu_msg_id {
		/*Receive by CCU */
		/*CCU Task*/
		MSG_TO_CCU_IDLE = 0x00000000,
		MSG_TO_CCU_SENSOR_INIT,
		MSG_TO_CCU_AE_INIT,
		MSG_TO_CCU_AE_START,
		MSG_TO_CCU_AE_STOP,
		MSG_TO_CCU_SET_SKIP_ALGO,
		MSG_TO_CCU_SET_AP_AE_FRAME_SYNC_DATA,
		MSG_TO_CCU_SET_AP_AE_ONCHANGE_DATA,
		MSG_TO_CCU_GET_CCU_OUTPUT,
		MSG_TO_CCU_SET_AP_AE_OUTPUT,
		MSG_TO_CCU_SET_AE_ROI,/*9*/
		MSG_TO_CCU_SET_AE_EV,
		MSG_TO_CCU_SET_MAX_FRAMERATE,
		MSG_TO_CCU_SHUTDOWN,
		/*todo-add timer sync task */
		DELIMITER_CCU_CONTROL_CNT_1,

		/*APMCU Ack*/
		/*CCU internal task*/
		MSG_CCU_INTERNAL_BANK_AE_ALGO,/*14*/
		MSG_CCU_INTERNAL_FRAME_AE_ALGO,
		MSG_CCU_INTERNAL_VSYNC_AE_SYNC,
		DELIMITER_CCU_CONTROL_CNT_2,
		/*Send to APMCU */
		/*CCU Ack/Done*/
		MSG_CCU_DONE_SHUTDOWN,/*18*/
		MSG_CCU_DONE_SENSOR_INIT,
		MSG_CCU_DONE_AE_INIT,
		MSG_CCU_ACK_AE_START,
		MSG_CCU_ACK_AE_STOP,/*22*/
		MSG_CCU_ACK_SET_SKIP_ALGO,/*22*/
		MSG_CCU_ACK_SET_AP_AE_READY,
		MSG_CCU_ACK_SET_AP_AE_FRAME_SYNC_DATA,
		MSG_CCU_ACK_SET_AP_AE_ONCHANGE_DATA,
		MSG_CCU_ACK_GET_CCU_OUTPUT,/*26*/
		MSG_CCU_ACK_SET_AP_AE_OUTPUT,
		MSG_CCU_ACK_SET_AE_ROI,
		MSG_CCU_ACK_SET_AE_EV,
		MSG_CCU_ACK_SET_MAX_FRAMERATE,

		/*APMCU Task*/
		MSG_TO_APMCU_FLUSH_LOG,/*31*/
		MSG_TO_APMCU_CCU_ASSERT,
		MSG_TO_APMCU_CCU_WARNING,
};

enum ccu_tg_info {
	CCU_CAM_TG_NONE = 0x0,
	CCU_CAM_TG_1    = 0x1,
	CCU_CAM_TG_2    = 0x2,
	CAM_CCU_TG_MAX
};

struct ccu_msg_t { /*16bytes*/
		enum ccu_msg_id msg_id;
		 MUINT32 in_data_ptr;
		 MUINT32 out_data_ptr;
		enum ccu_tg_info tg_info;
};

/********************************************************************
 * Special isr task (execute in isr)
 ********************************************************************/
#define ISR_SP_TASK_SHUTDOWN 0x000000FF
#define ISR_SP_TASK_TRG_I2C_DONE 0x000000C1
#define ISR_SP_TASK_RST_I2C_DONE 0x000000B9

/********************************************************************
 * Special ap isr task (execute in apmcu_isr)
 ********************************************************************/
#define APISR_SP_TASK_TRIGGER_I2C 0x000000A1
#define APISR_SP_TASK_RESET_I2C   0x000000A2

/********************************************************************
 * Error code definition
 ********************************************************************/
#define CCU_ERROR_NO               (0)
#define CCU_ERROR_QUEUE_FULL       (1)

/********************************************************************
 * Status definition
 ********************************************************************/
#define CCU_STATUS_INIT_DONE              0xffff0000
#define CCU_STATUS_INIT_DONE_2            0xffff00a5

/********************************************************************
 * Direct-link source definition
 ********************************************************************/
#define AE_STREAM_DL_SRC_AAO  0x00000000
#define AE_STREAM_DL_SRC_PSO  0x00000001

#endif
