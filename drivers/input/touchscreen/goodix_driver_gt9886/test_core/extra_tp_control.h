/******************** (C) COPYRIGHT 2018 Goodix ********************
* File Name			: extra_tp_control.h
* Author			:
* Version			: V1.0.0
* Date				: 30/03/2018
* Description		: tp control func
*******************************************************************************/
#ifndef EXTRA_TP_CONTROL_H
#define EXTRA_TP_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tp_dev_def.h"

/*************************************Public methods start********************************************/
extern s32 read_sensor_id(PST_TP_DEV p_tp_dev, u8 *sensor_id);
extern s32 read_chip_original_cfg(PST_TP_DEV p_tp_dev);
extern void update_cfg_checksum(PST_TP_DEV p_tp_dev, u8 *test_cfg,
				u16 len);
/*************************************Public methods end********************************************/

#ifdef __cplusplus
}
#endif
#endif
