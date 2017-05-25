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
 */

#include <linux/ktime.h>
#include <linux/mutex.h>
#include <soc/qcom/ais.h>
#include "qseecom_kernel.h"
#include "msm_camera_i2c.h"
#include "msm_camera_io_util.h"
#include "msm_cci.h"
#include "msm_sensor.h"

#define QSEECOM_SBUFF_SIZE      SZ_128K
#define MAX_TA_NAME             32
#define EMPTY_QSEECOM_HANDLE    NULL

#ifndef CONFIG_MSM_AIS_SEC_CCI_TA_NAME
	#define CONFIG_MSM_AIS_SEC_CCI_TA_NAME  "seccamdemo64"
#endif /* CONFIG_MSM_SEC_CCI_TA_NAME */

/* Update version major number in case the HLOS-TA interface is changed*/
#define TA_IF_VERSION_MAJ	    0
#define TA_IF_VERSION_MIN	    1

#undef CDBG
#ifdef CONFIG_MSM_AIS_SEC_CCI_DEBUG

#define CDBG(fmt, args...) \
	pr_info(CONFIG_MSM_AIS_SEC_CCI_TA_NAME "::%s:%d - " fmt,\
	__func__, __LINE__, ##args)
#define TZ_I2C_FN_RETURN(ret, i2c_fn, ...) \
	((ret < 0) ? i2c_fn(__VA_ARGS__):ret)

#else /* CONFIG_MSM_AIS_SEC_CCI_DEBUG */

#define CDBG(fmt, args...) \
		pr_info("%s:%d - " fmt,  __func__, __LINE__, ##args)
#define TZ_I2C_FN_RETURN(ret, i2c_fn, ...) \
		((ret < 0) ? -EFAULT:ret)

#endif /* CONFIG_MSM_AIS_SEC_CCI_DEBUG */

#pragma pack(push, msm_camera_tz_i2c, 1)

enum msm_camera_tz_i2c_cmd_id_t {
	TZ_I2C_CMD_GET_NONE,
	TZ_I2C_CMD_GET_IF_VERSION,
	TZ_I2C_CMD_POWER_UP,
	TZ_I2C_CMD_POWER_DOWN,
	TZ_I2C_CMD_CCI_GENERIC,
	TZ_I2C_CMD_CCI_READ,
	TZ_I2C_CMD_CCI_READ_SEQ,
	TZ_I2C_CMD_CCI_WRITE,
	TZ_I2C_CMD_CCI_WRITE_SEQ,
	TZ_I2C_CMD_CCI_WRITE_TABLE_ASYNC,
	TZ_I2C_CMD_CCI_WRITE_TABLE_SYNC,
	TZ_I2C_CMD_CCI_WRITE_TABLE_SYNC_BLOCK,
	TZ_I2C_CMD_CCI_WRITE_TABLE,
	TZ_I2C_CMD_CCI_WRITE_SEQ_TABLE,
	TZ_I2C_CMD_CCI_WRITE_TABLE_W_MICRODELAY,
	TZ_I2C_CMD_CCI_POLL,
	TZ_I2C_CMD_CCI_WRITE_CONF_TBL,
	TZ_I2C_CMD_CCI_UTIL,
};

enum msm_camera_tz_i2c_status_t {
	TZ_I2C_STATUS_SUCCESS = 0,
	TZ_I2C_STATUS_GENERAL_FAILURE = -1,
	TZ_I2C_STATUS_INVALID_INPUT_PARAMS = -2,
	TZ_I2C_STATUS_INVALID_SENSOR_ID = -3,
	TZ_I2C_STATUS_BYPASS = -4,
	TZ_I2C_STATUS_ERR_SIZE = 0x7FFFFFFF
};

struct msm_camera_tz_i2c_generic_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
};

struct msm_camera_tz_i2c_generic_rsp_t {
	enum msm_camera_tz_i2c_status_t  rc;
};

#define msm_camera_tz_i2c_get_if_version_req_t msm_camera_tz_i2c_generic_req_t

struct msm_camera_tz_i2c_get_if_version_rsp_t {
	enum msm_camera_tz_i2c_status_t  rc;
	uint32_t                    if_version_maj;
	uint32_t                    if_version_min;
};

struct msm_camera_tz_i2c_power_up_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
};

#define msm_camera_tz_i2c_power_up_rsp_t msm_camera_tz_i2c_generic_rsp_t

struct msm_camera_tz_i2c_power_down_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
};

#define msm_camera_tz_i2c_power_down_rsp_t msm_camera_tz_i2c_generic_rsp_t

struct msm_camera_tz_i2c_cci_generic_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
	enum msm_camera_tz_i2c_cmd_id_t  cci_cmd_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
};

#define msm_camera_tz_i2c_cci_generic_rsp_t msm_camera_tz_i2c_generic_rsp_t

struct msm_camera_tz_i2c_cci_read_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint32_t                    addr;
	uint32_t                    data_type;
};

struct msm_camera_tz_i2c_cci_read_rsp_t {
	enum msm_camera_tz_i2c_status_t  rc;
	uint16_t                    data;
};

struct msm_camera_tz_i2c_cci_write_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint32_t                    addr;
	uint16_t                    data;
	uint32_t                    data_type;
};

#define msm_camera_tz_i2c_cci_write_rsp_t msm_camera_tz_i2c_generic_rsp_t

struct msm_camera_tz_i2c_cci_util_req_t {
	enum msm_camera_tz_i2c_cmd_id_t  cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint16_t                    cci_cmd;
};

#define msm_camera_tz_i2c_cci_util_rsp_t msm_camera_tz_i2c_generic_rsp_t

#pragma pack(pop, msm_camera_tz_i2c)

struct msm_camera_tz_i2c_sensor_info_t {
	struct msm_sensor_ctrl_t    *s_ctrl;
	struct msm_camera_i2c_fn_t  *saved_sensor_i2c_fn;
	uint32_t                    secure;
	uint32_t                    ta_enabled;
	struct qseecom_handle       *ta_qseecom_handle;
	const char                  *ta_name;
};

struct msm_camera_tz_i2c_ctrl_t {
	struct mutex lock;
	uint32_t lock_ready;
	uint32_t secure_mode;
};

static struct msm_camera_tz_i2c_ctrl_t msm_camera_tz_i2c_ctrl;

static struct msm_camera_tz_i2c_sensor_info_t sensor_info[MAX_CAMERAS] = {
	{NULL, NULL, 0, 0, NULL, CONFIG_MSM_AIS_SEC_CCI_TA_NAME},
	{NULL, NULL, 0, 0, NULL, CONFIG_MSM_AIS_SEC_CCI_TA_NAME},
	{NULL, NULL, 0, 0, NULL, CONFIG_MSM_AIS_SEC_CCI_TA_NAME},
	{NULL, NULL, 0, 0, NULL, CONFIG_MSM_AIS_SEC_CCI_TA_NAME},
};

static int32_t msm_camera_tz_i2c_is_sensor_secure(
	struct msm_camera_i2c_client *client)
{
	uint32_t index;

	if (client == NULL) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	CDBG("Enter\n");
	for (index = 0; index < MAX_CAMERAS; index++) {
		if ((sensor_info[index].s_ctrl != NULL) &&
			sensor_info[index].secure &&
			(sensor_info[index].s_ctrl->sensor_i2c_client ==
				client)) {
			CDBG("Found secure sensor ID = %d\n",
				sensor_info[index].s_ctrl->id);
			return sensor_info[index].s_ctrl->id;
		}
	}
	return -EINVAL;
}

static int32_t get_cmd_rsp_buffers(
	struct qseecom_handle *ta_qseecom_handle,
	void **cmd,	int *cmd_len,
	void **rsp,	int *rsp_len)
{

	CDBG("Enter\n");
	if ((ta_qseecom_handle == NULL) ||
		(cmd == NULL) || (cmd_len == NULL) ||
		(rsp == NULL) || (rsp_len == NULL)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	if (*rsp_len & QSEECOM_ALIGN_MASK)
		*rsp_len = QSEECOM_ALIGN(*rsp_len);

	if ((*rsp_len + *cmd_len) > QSEECOM_SBUFF_SIZE) {
		pr_err("%s:%d - Shared buffer too small to hold cmd=%d and rsp=%d\n",
			__func__, __LINE__,
			*cmd_len, *rsp_len);
		return -ENOMEM;
	}

	*cmd = ta_qseecom_handle->sbuf;
	*rsp = ta_qseecom_handle->sbuf + *cmd_len;
	return 0;
}

static int32_t msm_camera_tz_i2c_ta_get_if_version(
	struct qseecom_handle *ta_qseecom_handle,
	uint32_t *if_version_maj,
	uint32_t *if_version_min)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_get_if_version_req_t *cmd;
	struct msm_camera_tz_i2c_get_if_version_rsp_t *rsp;
	int32_t rc = 0;

	CDBG("Enter\n");
	if ((ta_qseecom_handle == NULL) ||
		(if_version_maj == NULL) || (if_version_min == NULL)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	cmd_len = sizeof(struct msm_camera_tz_i2c_get_if_version_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_get_if_version_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_GET_IF_VERSION;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Unable to get if version info, rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}

		if (rsp->rc < 0) {
			CDBG("TZ I2C App error, rc=%d\n", rsp->rc);
			rc = -EFAULT;
		} else {
			*if_version_maj = rsp->if_version_maj;
			*if_version_min = rsp->if_version_min;
			CDBG("TZ I2C If version %d.%d\n", *if_version_maj,
				*if_version_min);
		}
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_power_up(
	struct qseecom_handle *ta_qseecom_handle,
	int32_t sensor_id,
	uint32_t *sensor_secure)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_power_up_req_t *cmd;
	struct msm_camera_tz_i2c_power_up_rsp_t *rsp;
	int32_t rc = 0;

	CDBG("Enter\n");

	if (sensor_secure == NULL)
		return -EINVAL;

	*sensor_secure = 0;
	if ((ta_qseecom_handle == NULL) ||
		(sensor_secure == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	cmd_len = sizeof(struct msm_camera_tz_i2c_power_up_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_power_up_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_POWER_UP;
		cmd->sensor_id = sensor_id;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Unable to get sensor secure status, rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}

		if (rsp->rc == TZ_I2C_STATUS_SUCCESS)
			*sensor_secure = 1;
		CDBG("Sensor %d is %s\n", sensor_id,
			(*sensor_secure)?"SECURE":"NON-SECURE");
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_power_down(
	struct qseecom_handle *ta_qseecom_handle,
	int32_t sensor_id)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_power_down_req_t *cmd;
	struct msm_camera_tz_i2c_power_down_rsp_t *rsp;
	int32_t rc = 0;

	CDBG("Enter\n");

	if ((ta_qseecom_handle == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	cmd_len = sizeof(struct msm_camera_tz_i2c_power_down_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_power_down_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_POWER_DOWN;
		cmd->sensor_id = sensor_id;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed: rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_cci_generic(
	struct msm_camera_i2c_client *client,
	enum msm_camera_tz_i2c_cmd_id_t cci_cmd_id)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_cci_generic_req_t *cmd;
	struct msm_camera_tz_i2c_cci_generic_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, cci_cmd_id=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		cci_cmd_id);

	ta_qseecom_handle = sensor_info[sensor_id].ta_qseecom_handle;
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_generic_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_generic_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_CCI_GENERIC;
		cmd->sensor_id = sensor_id;
		cmd->cci_cmd_id = cci_cmd_id;
		cmd->cci_i2c_master = client->cci_client->cci_i2c_master;
		cmd->sid = client->cci_client->sid;
		cmd->cid = client->cci_client->cid;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed: rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}
		rc = rsp->rc;
		CDBG("Done: rc=%d, cci_cmd_id=%d\n", rc, cci_cmd_id);
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_cci_read(
	struct msm_camera_i2c_client *client,
	uint32_t addr,
	uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_cci_read_req_t *cmd;
	struct msm_camera_tz_i2c_cci_read_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if ((client == NULL) ||
		(data == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, Addr=0x%X, Type=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr,
		data_type);

	ta_qseecom_handle = sensor_info[sensor_id].ta_qseecom_handle;
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_read_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_read_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_CCI_READ;
		cmd->sensor_id = sensor_id;
		cmd->cci_i2c_master = client->cci_client->cci_i2c_master;
		cmd->sid = client->cci_client->sid;
		cmd->cid = client->cci_client->cid;
		cmd->addr = addr;
		cmd->data_type = data_type;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed: rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}
		rc = rsp->rc;
		*data = rsp->data;

		CDBG("Done: rc=%d, addr=0x%X, data=0x%X\n", rc,
			addr, *data);
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_cci_write(
	struct msm_camera_i2c_client *client,
	uint32_t addr,
	uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_cci_write_req_t *cmd;
	struct msm_camera_tz_i2c_cci_write_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, Addr=0x%X, Data=0x%X Type=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr,
		data,
		data_type);

	ta_qseecom_handle = sensor_info[sensor_id].ta_qseecom_handle;
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_write_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_write_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_CCI_WRITE;
		cmd->sensor_id = sensor_id;
		cmd->cci_i2c_master = client->cci_client->cci_i2c_master;
		cmd->sid = client->cci_client->sid;
		cmd->cid = client->cci_client->cid;
		cmd->addr = addr;
		cmd->data = data;
		cmd->data_type = data_type;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed:, rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}
		rc = rsp->rc;

		CDBG("Done: rc=%d, addr=0x%X, data=0x%X\n", rc,
			addr, data);
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_cci_util(
	struct msm_camera_i2c_client *client,
	uint16_t cci_cmd)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_cci_util_req_t *cmd;
	struct msm_camera_tz_i2c_cci_util_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, cci_cmd=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		cci_cmd);

	ta_qseecom_handle = sensor_info[sensor_id].ta_qseecom_handle;
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_util_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_util_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = TZ_I2C_CMD_CCI_UTIL;
		cmd->sensor_id = sensor_id;
		cmd->cci_i2c_master = client->cci_client->cci_i2c_master;
		cmd->sid = client->cci_client->sid;
		cmd->cid = client->cci_client->cid;
		cmd->cci_cmd = cci_cmd;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed: rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}
		rc = rsp->rc;
		CDBG("Done: rc=%d, cci_cmd=%d\n", rc, cci_cmd);
	}
	return rc;
}

static int32_t msm_camera_tz_i2c_ta_probe(
	struct msm_camera_i2c_client *client)
{
	int32_t sensor_id = -1;

	CDBG("Enter\n");
	sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);
	if ((sensor_id >= 0) && sensor_info[sensor_id].ta_enabled
		&& msm_camera_tz_i2c_ctrl.lock_ready) {
		mutex_lock(&msm_camera_tz_i2c_ctrl.lock);
		return sensor_id;
	}
	return -EINVAL;
}

static int32_t msm_camera_tz_i2c_ta_done(void)
{
	int32_t rc = 0;

	CDBG("Enter\n");
	if (msm_camera_tz_i2c_ctrl.lock_ready)
		mutex_unlock(&msm_camera_tz_i2c_ctrl.lock);
	return rc;
}

int32_t msm_camera_tz_i2c_power_up(
	struct msm_camera_i2c_client *client)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if (!msm_camera_tz_i2c_ctrl.lock_ready) {
		msm_camera_tz_i2c_ctrl.lock_ready = 1;
		mutex_init(&msm_camera_tz_i2c_ctrl.lock);
	}

	CDBG("Enter (sensor_id=%d)\n", sensor_id);
	if (sensor_id >= 0) {
		ktime_t startTime;

		mutex_lock(&msm_camera_tz_i2c_ctrl.lock);
		if (msm_camera_tz_i2c_ctrl.secure_mode) {
			mutex_unlock(&msm_camera_tz_i2c_ctrl.lock);
			return rc;
		}
		startTime = ktime_get();

		CDBG("Switch to secure mode (secure sensor=%d)\n",
			sensor_id);
		/* Start the TA */
		if ((sensor_info[sensor_id].ta_qseecom_handle == NULL)
			&& (sensor_info[sensor_id].ta_name != NULL) &&
			('\0' != sensor_info[sensor_id].ta_name[0])) {
			uint32_t if_version_maj = 0;
			uint32_t if_version_min = 0;

			sensor_info[sensor_id].ta_enabled = 0;
			rc = qseecom_start_app(
				&sensor_info[sensor_id].ta_qseecom_handle,
				(char *)sensor_info[sensor_id].ta_name,
				QSEECOM_SBUFF_SIZE);
			if (!rc) {
				rc = msm_camera_tz_i2c_ta_get_if_version(
					sensor_info[sensor_id].
					ta_qseecom_handle,
					&if_version_maj, &if_version_min);
			}

			if (!rc) {
				if (if_version_maj != TA_IF_VERSION_MAJ) {
					CDBG("TA ver mismatch %d.%d != %d.%d\n",
						if_version_maj, if_version_min,
						TA_IF_VERSION_MAJ,
						TA_IF_VERSION_MIN);
					rc = qseecom_shutdown_app(
						&sensor_info[sensor_id].
						ta_qseecom_handle);
					sensor_info[sensor_id].ta_qseecom_handle
						= EMPTY_QSEECOM_HANDLE;
					rc = -EFAULT;
				} else {
					uint32_t sensor_secure = 0;
					/* Notify TA */
					/* Get sensor secure status */
					rc = msm_camera_tz_i2c_ta_power_up(
						sensor_info[sensor_id].
						ta_qseecom_handle,
						sensor_id,
						&sensor_secure);
					if (!rc && sensor_secure)
						/* Sensor validated by TA*/
						sensor_info[sensor_id].
							ta_enabled = 1;
					else {
						qseecom_shutdown_app(
							&sensor_info[sensor_id].
							ta_qseecom_handle);
						sensor_info[sensor_id].
							ta_qseecom_handle
							= EMPTY_QSEECOM_HANDLE;
						rc = -EFAULT;
					}
				}
			}
		}
		CDBG("Init TA %s - %s(%d) - %llu\n",
			sensor_info[sensor_id].ta_name,
			(sensor_info[sensor_id].ta_enabled)?"Ok" :
			"Failed", rc, ktime_us_delta(ktime_get(),
			startTime));
		if (!rc)
			msm_camera_tz_i2c_ctrl.secure_mode++;
		mutex_unlock(&msm_camera_tz_i2c_ctrl.lock);
	}
	return rc;
}

int32_t msm_camera_tz_i2c_power_down(
	struct msm_camera_i2c_client *client)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);

	if (!msm_camera_tz_i2c_ctrl.lock_ready) {
		msm_camera_tz_i2c_ctrl.lock_ready = 1;
		mutex_init(&msm_camera_tz_i2c_ctrl.lock);
	}

	CDBG("Enter (sensor_id=%d)\n", sensor_id);
	if ((sensor_id >= 0) && (msm_camera_tz_i2c_ctrl.secure_mode != 0)) {
		mutex_lock(&msm_camera_tz_i2c_ctrl.lock);
		if (msm_camera_tz_i2c_ctrl.secure_mode == 1) {
			ktime_t startTime = ktime_get();

			CDBG("Switch to non-secure mode (secure sensor=%d)\n",
				sensor_id);
			/* Shutdown the TA */
			if (sensor_info[sensor_id].ta_qseecom_handle != NULL) {
				msm_camera_tz_i2c_ta_power_down(
					sensor_info[sensor_id].
						ta_qseecom_handle,
					sensor_id);
				rc = qseecom_shutdown_app(&sensor_info[
					sensor_id].ta_qseecom_handle);
				sensor_info[sensor_id].ta_qseecom_handle
					= EMPTY_QSEECOM_HANDLE;
			}
			CDBG("Unload TA %s - %s(%d) - %llu\n",
				sensor_info[sensor_id].ta_name,
				(!rc)?"Ok":"Failed", rc,
				ktime_us_delta(ktime_get(), startTime));
		}
		msm_camera_tz_i2c_ctrl.secure_mode--;
		mutex_unlock(&msm_camera_tz_i2c_ctrl.lock);
	}
	return rc;
}

int32_t msm_camera_tz_i2c_register_sensor(
	void *s_ctrl_p)
{
	struct msm_sensor_ctrl_t *s_ctrl = (struct msm_sensor_ctrl_t *)s_ctrl_p;

	if (s_ctrl == NULL) {
		pr_err("%s:%d - invalid parameter)\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	if (s_ctrl->id >= MAX_CAMERAS) {
		pr_err("%s:%d - invalid ID: %d\n",
			__func__, __LINE__, s_ctrl->id);
		return -EINVAL;
	}

	CDBG("id=%d, client=%pK\n", s_ctrl->id, s_ctrl);
	sensor_info[s_ctrl->id].s_ctrl = s_ctrl;
	sensor_info[s_ctrl->id].secure = s_ctrl->is_secure;
	return 0;
}

int32_t msm_camera_tz_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, addr=0x%08X\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_read(
			client, addr, data, data_type);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_read, client, addr, data, data_type);
}

int32_t msm_camera_tz_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, addr=0x%08X, num=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr,
		num_byte);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_READ_SEQ);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_read_seq, client, addr, data, num_byte);
}

int32_t msm_camera_tz_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, addr=0x%08X\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_write(
			client, addr, data, data_type);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write, client, addr, data, data_type);
}

int32_t msm_camera_tz_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, addr=0x%08X, num=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		addr,
		num_byte);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_SEQ);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_seq, client, addr, data, num_byte);
}

int32_t msm_camera_tz_i2c_write_table_async(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_TABLE_ASYNC);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_table_async, client, write_setting);
}

int32_t msm_camera_tz_i2c_write_table_sync(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_TABLE_SYNC);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_table_sync, client, write_setting);
}

int32_t msm_camera_tz_i2c_write_table_sync_block(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_TABLE_SYNC_BLOCK);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_table_sync_block, client,
			write_setting);
}

int32_t msm_camera_tz_i2c_write_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_TABLE);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_table, client, write_setting);
}

int32_t msm_camera_tz_i2c_write_seq_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_SEQ_TABLE);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_seq_table, client, write_setting);
}

int32_t msm_camera_tz_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_TABLE_W_MICRODELAY);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_table_w_microdelay, client,
			write_setting);
}

int32_t msm_camera_tz_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_POLL);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_poll, client, addr, data, data_type);
}

int32_t msm_camera_tz_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_generic(
			client, TZ_I2C_CMD_CCI_WRITE_CONF_TBL);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_camera_cci_i2c_write_conf_tbl, client, reg_conf_tbl, size,
			data_type);
}

int32_t msm_sensor_tz_i2c_util(struct msm_camera_i2c_client *client,
	uint16_t cci_cmd)
{
	int32_t rc = -EFAULT;
	int32_t sensor_id = msm_camera_tz_i2c_ta_probe(client);

	CDBG("Sensor=%d, MS=%d, SID=%d, CID=%d, cci_cmd=%d\n",
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid, cci_cmd);

	if (sensor_id >= 0) {
		rc = msm_camera_tz_i2c_ta_cci_util(client, cci_cmd);
		msm_camera_tz_i2c_ta_done();
	}
	return TZ_I2C_FN_RETURN(rc,
		msm_sensor_cci_i2c_util, client, cci_cmd);
}
