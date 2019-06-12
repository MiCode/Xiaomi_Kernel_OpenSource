/******************** (C) COPYRIGHT 2016 Goodix ********************
* File Name          : tp_micro_def.h
* Author             : Bob Huang
* Version            : V1.0.0
* Date               : 12/31/2017
* Description        : touch panel device define
*******************************************************************************/
#ifndef TP_MICRO_DEF_H
#define TP_MICRO_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "user_test_type_def.h"
#include "tp_product_id_def.h"

#define TP_MAX_KEY_NUM			(15)
#define TP_MAX_SEN_NUM			(152)
#define TP_MAX_DRV_NUM			(152)
#define TP_MAX_CHN_NUM			(TP_MAX_SEN_NUM + TP_MAX_DRV_NUM)
#define TP_MAX_NODE_NUM			(TP_MAX_SEN_NUM * TP_MAX_DRV_NUM)

#define TP_MAX_BIN_SIZE					(256*1024)
#define TP_MAX_SHORT_BIN_SIZE			(128*1024)
#define TP_MAX_SHORT_OPT_PARAM_SIZE		(256)
#define TP_MAX_OPT_MESS_SIZE			(2*1024)

#define TP_MAX_CUSTOM_INFO_SIZE (64)

#define TP_MAX_NC_LEN			(TP_MAX_SEN_NUM * TP_MAX_DRV_NUM)
#define TP_MAX_KEY_NC_LEN		(TP_MAX_KEY_NUM * TP_MAX_CHN_NUM)

#define TP_FLAG_NEED_CHECK			(0x01)
#define TP_FLAG_NOT_NEED_CHECK		(!TP_FLAG_NEED_CHECK)
#define TP_FLAG_KEY_NEED_CHECK		(0x01)
#define TP_FLAG_KEY_NOT_NEED_CHECK	(!TP_FLAG_KEY_NEED_CHECK)

/*need move to other place*/
#define TP_INVALID_SNODE		(0xFFFF)

/*cfg pack num*/
#define TP_MAX_CFG_PACK_NUM		(100)

/* cfg length*/
#define TP_MAX_CFG_LEN			(2048)
#define TP_MAX_I2C_ADDRESS_NUM	(127)
#define TP_MAX_CMD_LEN			(10)
#define TP_DEV_CMD_LEN			(3)

#define TP_MAX_TOUCH_NUM		(10)
#define TP_MAX_INT_MODE_NUM		(4)

/*channel define */
#define  GT_9P_DRV_NUM_MAX		(32)
#define  GT_9P_SEN_NUM_MAX		(32)

#define  GT_9PT_DRV_NUM_MAX GT_9P_DRV_NUM_MAX
#define  GT_9PT_SEN_NUM_MAX GT_9P_SEN_NUM_MAX

#ifdef __cplusplus
}
#endif
#endif
