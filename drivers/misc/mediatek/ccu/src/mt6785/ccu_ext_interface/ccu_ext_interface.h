/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CCU_INTERFACE__
#define __CCU_INTERFACE__

#include "ccu_ext_interface/ccu_types.h"

extern MBOOL sec_vsync_pushed;

/*****************************************************************************/
/* Task definition */
/*****************************************************************************/
enum ccu_msg_id {
	/*Receive by CCU*/
	/*CCU Task*/
	MSG_TO_CCU_IDLE = 0x00000000,
	MSG_TO_CCU_SENSOR_INIT,
	MSG_TO_CCU_AE_INIT,
	MSG_TO_CCU_AE_START,
	MSG_TO_CCU_AE_STOP,
	MSG_TO_CCU_SET_AP_AE_FRAME_SYNC_DATA,
	MSG_TO_CCU_SET_AP_AE_ONCHANGE_DATA,
	MSG_TO_CCU_GET_CCU_OUTPUT,
	MSG_TO_CCU_SET_MAX_FRAMERATE,
	MSG_TO_CCU_GET_AE_DEBUG_INFO,
	MSG_TO_CCU_SET_3A_SYNC_STATE,
	MSG_TO_CCU_SYNC_AE_SETTING,
	MSG_TO_CCU_START_FRAME_SYNC,
	MSG_TO_CCU_STOP_FRAME_SYNC,
	MSG_TO_CCU_QUERY_FRAME_SYNC_DONE,
	MSG_TO_CCU_SUSPEND,
	MSG_TO_CCU_RESUME,
	MSG_TO_CCU_SHUTDOWN,
    // for new framesync
	MSG_TO_CCU_FRAME_SYNC_SET_SENSOR_INFO,
	MSG_TO_CCU_FRAME_SYNC_SET_AE_INFO,
	MSG_TO_CCU_FS_START_FRAME_SYNC,
	MSG_TO_CCU_FS_STOP_FRAME_SYNC,
	MSG_TO_CCU_FS_QUERY_FRAME_SYNC_DONE,
	MSG_TO_CCU_FRAME_SYNC_PERFRAME_CTRL,
	MSG_TO_CCU_UPDATE_TG_SENSOR_MAP,
	MSG_TO_CCU_INVALIDATE_TG_SENSOR_MAP,

#ifdef CCU_AF_ENABLE
	MSG_TO_CCU_AF_INIT,
	MSG_TO_CCU_AF_START,
	MSG_TO_CCU_AF_ALGO_DONE,
	MSG_TO_CCU_AF_ACQUIRE_AFO_BUFFER,
	MSG_TO_CCU_AF_SET_HW_REG,
	MSG_TO_CCU_AF_STOP,
	MSG_TO_CCU_AF_ABORT,
#endif /*CCU_AF_ENABLE*/

	/*To identify ccu control msg count*/
	DELIMITER_CCU_CONTROL_CNT_1,
	/*CCU internal task*/
#ifdef CCU_AF_ENABLE
	MSG_TO_CCU_AF_CQ0,
	MSG_TO_CCU_AF_AFO,
	MSG_TO_CCU_AF_CQ0B,
	MSG_TO_CCU_AF_AFOB,
#endif /*CCU_AF_ENABLE*/
	MSG_CCU_INTERNAL_BANK_AE_ALGO,
	MSG_CCU_INTERNAL_FRAME_AE_ALGO,
	MSG_CCU_INTERNAL_VSYNC_AE_SYNC,
	MSG_CCU_INTERNAL_FRAME_SYNC,
	MSG_CCU_INTERNAL_PR_LOG,
	/*To identify ccu control msg count*/
	DELIMITER_CCU_CONTROL_CNT_2,

	/*Send to APMCU*/
	/*CCU Ack/Done*/
	/**< Ack cmd for #MSG_TO_CCU_SENSOR_INIT */
	MSG_CCU_DONE_SENSOR_INIT,
	/**< Ack cmd for #MSG_TO_CCU_AE_INIT */
	MSG_CCU_DONE_AE_INIT,
	/**< Ack cmd for #MSG_TO_CCU_AE_START */
	MSG_CCU_ACK_AE_START,
	/**< Ack cmd for #MSG_TO_CCU_AE_STOP */
	MSG_CCU_ACK_AE_STOP,
	/**< Ack cmd for #MSG_TO_CCU_SET_AP_AE_FRAME_SYNC_DATA */
	MSG_CCU_ACK_SET_AP_AE_FRAME_SYNC_DATA,
	/**< Ack cmd for #MSG_TO_CCU_SET_AP_AE_ONCHANGE_DATA */
	MSG_CCU_ACK_SET_AP_AE_ONCHANGE_DATA,
	/**< Ack cmd for #MSG_TO_CCU_GET_CCU_OUTPUT */
	MSG_CCU_ACK_GET_CCU_OUTPUT,
	/**< Ack cmd for #MSG_TO_CCU_SET_MAX_FRAMERATE */
	MSG_CCU_ACK_SET_MAX_FRAMERATE,
	/**< Ack cmd for #MSG_TO_CCU_GET_AE_DEBUG_INFO */
	MSG_CCU_ACK_GET_AE_DEBUG_INFO,
	/**< Ack cmd for #MSG_TO_CCU_SET_3A_SYNC_STATE */
	MSG_CCU_ACK_SET_3A_SYNC_STATE,
	/**< Ack cmd for #MSG_TO_CCU_SYNC_AE_SETTING */
	MSG_CCU_ACK_SYNC_AE_SETTING,
	MSG_CCU_ACK_START_FRAME_SYNC,
	MSG_CCU_ACK_STOP_FRAME_SYNC,
	MSG_CCU_ACK_QUERY_FRAME_SYNC_DONE,
	/**< Ack cmd for #MSG_TO_CCU_SUSPEND */
	MSG_CCU_ACK_SUSPEND,
	/**< Ack cmd for #MSG_TO_CCU_RESUME */
	MSG_CCU_ACK_RESUME,
	/**< Ack cmd for #MSG_TO_CCU_SHUTDOWN */
	MSG_CCU_DONE_SHUTDOWN,

#ifdef CCU_AF_ENABLE
	MSG_CCU_ACK_AF_INIT,
	MSG_CCU_ACK_AF_START,
	MSG_CCU_ACK_AF_ALGO_DONE,
	MSG_CCU_ACK_AF_ACQUIRE_AFO_BUFFER,
	MSG_CCU_ACK_AF_SET_HW_REG,
	MSG_CCU_ACK_AF_STOP,
	MSG_CCU_ACK_AF_ABORT,
#endif /*CCU_AF_ENABLE*/
	/*APMCU Task*/
	MSG_TO_APMCU_FLUSH_LOG,
	MSG_TO_APMCU_CCU_ASSERT,
	MSG_TO_APMCU_CCU_WARNING
#ifdef CCU_AF_ENABLE
	, MSG_TO_APMCU_CAM_A_AFO_i,
	MSG_TO_APMCU_CAM_B_AFO_i
#endif /*CCU_AF_ENABLE*/
};

enum ccu_tg_info {
	CCU_CAM_TG_NONE = 0x0,
	CCU_CAM_TG_MIN  = 0x1,
	CCU_CAM_TG_1    = 0x1,
	CCU_CAM_TG_2    = 0x2,
	CCU_CAM_TG_MAX
};
#define CCU_CAM_TG_CNT (CCU_CAM_TG_MAX - CCU_CAM_TG_MIN)

struct ccu_msg { /*16bytes*/
	enum ccu_msg_id msg_id;
	MUINT32 in_data_ptr;
	MUINT32 out_data_ptr;
	enum ccu_tg_info tg_info;
};

struct ccu_control_info {
	enum ccu_tg_info tg_info; //new
	uint32_t msg_id;
	uint64_t inDataPtr;
	uint32_t inDataSize;
	uint64_t outDataPtr;
	uint32_t outDataSize;
};

struct ap2ccu_ipc_t {
	MUINT32 write_cnt;
	MUINT32 read_cnt;
	struct ccu_msg msg;
	MBOOL ack;
};

struct exif_data_addrs_s {
	MUINT32 ae_algo_data_addr;
	MUINT32 ae_init_data_addr;
	MUINT32 ae_vsync_info_addr;
	MUINT32 aesync_algo_in_addr;
	MUINT32 aesync_algo_out_addr;
};

struct shared_buf_map {
	MUINT32 ipc_in_data_addr_ccu;
	MUINT32 ipc_out_data_addr_ccu;
	MUINT32 ipc_in_data_base_offset;
	MUINT32 ipc_out_data_base_offset;
	MUINT32 ipc_base_offset;
	struct exif_data_addrs_s exif_data_addrs[CCU_CAM_TG_CNT];
};


/*****************************************************************************/
/* Special isr task (execute in isr) */
/*****************************************************************************/
#define ISR_SP_TASK_SHUTDOWN 0x000000FF
#define ISR_SP_TASK_TRG_I2C_DONE 0x000000C1
#define ISR_SP_TASK_RST_I2C_DONE 0x000000B9

/*****************************************************************************/
/* Special ap isr task (execute in apmcu_isr) */
/*****************************************************************************/
#define APISR_SP_TASK_TRIGGER_I2C 0x000000A1
#define APISR_SP_TASK_RESET_I2C   0x000000A2

/*****************************************************************************/
/* Error code definition */
/*****************************************************************************/
#define CCU_ERROR_NO               (0)
#define CCU_ERROR_QUEUE_FULL       (1)

/*****************************************************************************/
/* Status definition */
/*****************************************************************************/
#define CCU_STATUS_INIT_DONE              0xffff0000
#define CCU_STATUS_INIT_DONE_2            0xffff00a5

/*****************************************************************************/
/* Direct-link source definition */
/*****************************************************************************/
#define AE_STREAM_DL_SRC_AAO  0x00000000
#define AE_STREAM_DL_SRC_PSO  0x00000001

#endif
