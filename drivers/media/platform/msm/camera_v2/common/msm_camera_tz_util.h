/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CAMERA_TZ_UTIL_H
#define __MSM_CAMERA_TZ_UTIL_H

#include <soc/qcom/camera2.h>

#ifndef CONFIG_MSM_CAMERA_TZ_TA_NAME
#define CONFIG_MSM_CAMERA_TZ_TA_NAME  "seccamdemo64"
#endif /* CONFIG_MSM_CAMERA_TZ_TA_NAME */

#define MSM_CAMERA_TZ_MODE_NON_SECURE   0x0000000000
#define MSM_CAMERA_TZ_MODE_SECURE       0x0000000001

#define MSM_CAMERA_TZ_HW_BLOCK_CSIDCORE 0x0000000001
#define MSM_CAMERA_TZ_HW_BLOCK_ISPIF    0x0000000002
#define MSM_CAMERA_TZ_HW_BLOCK_CCI      0x0000000004
#define MSM_CAMERA_TZ_HW_BLOCK_ISP      0x0000000008
#define MSM_CAMERA_TZ_HW_BLOCK_CPP      0x0000000010

enum msm_camera_tz_cmd_id_t {
	MSM_CAMERA_TZ_CMD_NONE = 56000,
	MSM_CAMERA_TZ_CMD_GET_IF_VERSION,
	MSM_CAMERA_TZ_CMD_POWER_UP,
	MSM_CAMERA_TZ_CMD_POWER_DOWN,
	MSM_CAMERA_TZ_CMD_CCI_GENERIC,
	MSM_CAMERA_TZ_CMD_CCI_READ,
	MSM_CAMERA_TZ_CMD_CCI_READ_SEQ,
	MSM_CAMERA_TZ_CMD_CCI_WRITE,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_SEQ,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_ASYNC,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_SYNC,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_SYNC_BLOCK,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_SEQ_TABLE,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_W_MICRODELAY,
	MSM_CAMERA_TZ_CMD_CCI_POLL,
	MSM_CAMERA_TZ_CMD_CCI_WRITE_CONF_TBL,
	MSM_CAMERA_TZ_CMD_CCI_UTIL,
	MSM_CAMERA_TZ_CMD_SET_MODE,
	MSM_CAMERA_TZ_CMD_FRAME_NOTIFICATION,
};

enum msm_camera_tz_status_t {
	MSM_CAMERA_TZ_STATUS_SUCCESS = 0,
	MSM_CAMERA_TZ_STATUS_GENERAL_FAILURE = -1,
	MSM_CAMERA_TZ_STATUS_INVALID_INPUT_PARAMS = -2,
	MSM_CAMERA_TZ_STATUS_INVALID_SENSOR_ID = -3,
	MSM_CAMERA_TZ_STATUS_BYPASS = -4,
	MSM_CAMERA_TZ_STATUS_TIMEOUT = -5,

	MSM_CAMERA_TZ_STATUS_RESET_DONE = 1,
	MSM_CAMERA_TZ_STATUS_ERR_SIZE = 0x7FFFFFFF
};

#pragma pack(push, msm_camera_tz, 1)

struct msm_camera_tz_generic_req_t {
	enum msm_camera_tz_cmd_id_t  cmd_id;
};

struct msm_camera_tz_generic_rsp_t {
	enum msm_camera_tz_status_t  rc;
};

#pragma pack(pop, msm_camera_tz)

uint32_t msm_camera_tz_set_mode(
	uint32_t mode, uint32_t hw_block);

struct qseecom_handle *msm_camera_tz_get_ta_handle(void);
int32_t get_cmd_rsp_buffers(
	struct qseecom_handle *ta_qseecom_handle,
	void **cmd, int *cmd_len,
	void **rsp, int *rsp_len);
int32_t msm_camera_tz_load_ta(void);
int32_t msm_camera_tz_unload_ta(void);
void msm_camera_tz_lock(void);
void msm_camera_tz_unlock(void);

#endif /* __MSM_CAMERA_TZ_UTIL_H */
