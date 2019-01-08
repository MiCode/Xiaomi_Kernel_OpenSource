/* Copyright (c) 2016, 2017-2018, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/camera2.h>
#include "qseecom_kernel.h"
#include "msm_camera_i2c.h"
#include "msm_camera_tz_util.h"
#include "msm_cci.h"
#include "msm_sensor.h"

#undef CDBG

#ifdef CONFIG_MSM_SEC_CCI_DEBUG
	#define TZ_I2C_FN_RETURN(ret, i2c_fn, ...) \
		((ret < 0) ? i2c_fn(__VA_ARGS__):ret)
#else /* CONFIG_MSM_SEC_CCI_DEBUG */
	#define TZ_I2C_FN_RETURN(ret, i2c_fn, ...) \
			((ret < 0) ? -EFAULT:ret)
#endif /* CONFIG_MSM_SEC_CCI_DEBUG */

#ifdef MSM_CAMERA_TZ_I2C_VERBOSE
	#define CDBG(fmt, args...) \
		pr_info(CONFIG_MSM_SEC_CCI_TA_NAME "::%s:%d - " fmt, \
		__func__, __LINE__, ##args)
#else /* MSM_CAMERA_TZ_I2C_VERBOSE */
	#define CDBG(fmt, args...) \
		pr_debug("%s:%d - " fmt,  __func__, __LINE__, ##args)
#endif /* MSM_CAMERA_TZ_I2C_VERBOSE */

#pragma pack(push, msm_camera_tz_i2c, 1)

struct msm_camera_tz_i2c_cci_generic_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
	enum msm_camera_tz_cmd_id_t cci_cmd_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
};

#define msm_camera_tz_i2c_cci_generic_rsp_t msm_camera_tz_generic_rsp_t

/* MSM_CAMERA_TZ_CMD_POWER_UP */
struct msm_camera_tz_i2c_power_up_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
};

#define msm_camera_tz_i2c_power_up_rsp_t msm_camera_tz_generic_rsp_t

/* MSM_CAMERA_TZ_CMD_POWER_DOWN */
struct msm_camera_tz_i2c_power_down_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
};

#define msm_camera_tz_i2c_power_down_rsp_t msm_camera_tz_generic_rsp_t

/* MSM_CAMERA_TZ_CMD_CCI_READ */
struct msm_camera_tz_i2c_cci_read_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint32_t                    addr;
	uint32_t                    data_type;
};

struct msm_camera_tz_i2c_cci_read_rsp_t {
	enum msm_camera_tz_status_t rc;
	uint16_t                    data;
};

/* MSM_CAMERA_TZ_CMD_CCI_WRITE */
struct msm_camera_tz_i2c_cci_write_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint32_t                    addr;
	uint16_t                    data;
	uint32_t                    data_type;
};

#define msm_camera_tz_i2c_cci_write_rsp_t msm_camera_tz_generic_rsp_t

/* MSM_CAMERA_TZ_CMD_CCI_UTIL */
struct msm_camera_tz_i2c_cci_util_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	int32_t                     sensor_id;
	uint32_t                    cci_i2c_master;
	uint16_t                    sid;
	uint16_t                    cid;
	uint16_t                    cci_cmd;
};

#define msm_camera_tz_i2c_cci_util_rsp_t msm_camera_tz_generic_rsp_t

#pragma pack(pop, msm_camera_tz_i2c)

/* Camera control structure */
struct msm_camera_tz_i2c_sensor_info_t {
	struct msm_sensor_ctrl_t    *s_ctrl;
	struct msm_camera_i2c_fn_t  *saved_sensor_i2c_fn;
	uint32_t                    secure;
	uint32_t                    ready;
};

static struct msm_camera_tz_i2c_sensor_info_t sensor_info[MAX_CAMERAS];

static int32_t msm_camera_tz_i2c_is_sensor_secure(
	struct msm_camera_i2c_client *client)
{
	uint32_t index;

	if (client == NULL) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

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

	if (sensor_secure == NULL) {
		pr_err("%s:%d - Bad parameter\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	*sensor_secure = 0;

	if ((ta_qseecom_handle == NULL) ||
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
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_POWER_UP;
		cmd->sensor_id = sensor_id;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Unable to get sensor secure status, rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}

		if (rsp->rc == MSM_CAMERA_TZ_STATUS_SUCCESS)
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
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_POWER_DOWN;
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
	enum msm_camera_tz_cmd_id_t cci_cmd_id)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_i2c_cci_generic_req_t *cmd;
	struct msm_camera_tz_i2c_cci_generic_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);
	ktime_t startTime = ktime_get();

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ta_qseecom_handle = msm_camera_tz_get_ta_handle();
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_generic_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_generic_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_CCI_GENERIC;
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
	}
	CDBG("Done: rc=%d, SN=%d, MS=%d, SID=%d, CID=%d, CMD=%d - %lluus\n",
		rc,	sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		cci_cmd_id,
		ktime_us_delta(ktime_get(), startTime));

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
	ktime_t startTime = ktime_get();

	if ((client == NULL) ||
		(data == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ta_qseecom_handle = msm_camera_tz_get_ta_handle();
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_read_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_read_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_CCI_READ;
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
	}
	CDBG("Done: rc=%d, SN=%d, MS=%d, SID=%d, CID=%d, ", rc,
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	CDBG("Addr=0x%X, Type=%d, Data=0x%X - %lluus\n",
		addr, data_type, *data,
		ktime_us_delta(ktime_get(), startTime));

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
	ktime_t startTime = ktime_get();

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ta_qseecom_handle = msm_camera_tz_get_ta_handle();
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_write_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_write_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_CCI_WRITE;
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
	}
	CDBG("Done: rc=%d, SN=%d, MS=%d, SID=%d, CID=%d, ", rc,
		sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid);

	CDBG("Addr=0x%X, Data=0x%X Type=%d - %lluus\n",
		addr, data,	data_type,
		ktime_us_delta(ktime_get(), startTime));

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
	ktime_t startTime = ktime_get();

	if ((client == NULL) ||
		(sensor_id < 0) ||
		(sensor_id >= MAX_CAMERAS)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ta_qseecom_handle = msm_camera_tz_get_ta_handle();
	cmd_len = sizeof(struct msm_camera_tz_i2c_cci_util_req_t);
	rsp_len = sizeof(struct msm_camera_tz_i2c_cci_util_rsp_t);

	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_CCI_UTIL;
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
	}
	CDBG("Done: rc=%d, SN=%d, MS=%d, SID=%d, CID=%d, CMD=%d - %lluus\n",
		rc,	sensor_id,
		client->cci_client->cci_i2c_master,
		client->cci_client->sid,
		client->cci_client->cid,
		cci_cmd,
		ktime_us_delta(ktime_get(), startTime));

	return rc;
}

static int32_t msm_camera_tz_i2c_ta_probe(
	struct msm_camera_i2c_client *client)
{
	int32_t sensor_id = -1;

	CDBG("Enter\n");
	sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);
	if ((sensor_id >= 0) &&
		(sensor_id < MAX_CAMERAS) &&
		(sensor_info[sensor_id].ready != 0)) {
		msm_camera_tz_lock();
		return sensor_id;
	}
	return -EINVAL;
}

static int32_t msm_camera_tz_i2c_ta_done(void)
{
	CDBG("Enter\n");
	msm_camera_tz_unlock();
	return 0;
}

int32_t msm_camera_tz_i2c_power_up(
	struct msm_camera_i2c_client *client)
{
	int32_t rc = 0;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);
	ktime_t startTime = ktime_get();

	CDBG("Enter (sensor_id=%d)\n", sensor_id);
	if ((sensor_id >= 0) && (sensor_id < MAX_CAMERAS)) {
		rc = msm_camera_tz_load_ta();
		if (!rc) {
			uint32_t sensor_secure = 0;

			msm_camera_tz_lock();
			/* Notify TA & get sensor secure status */
			rc = msm_camera_tz_i2c_ta_power_up(
				msm_camera_tz_get_ta_handle(),
				sensor_id,
				&sensor_secure);
			if (!rc && sensor_secure)
				/* Sensor validated by TA*/
				sensor_info[sensor_id].ready++;
			else {
				msm_camera_tz_unload_ta();
				rc = -EFAULT;
			}
			msm_camera_tz_unlock();
		}
	} else
		rc = -EFAULT;
	CDBG("Power UP sensor = %d, %s(%d) - %lluus\n",
		sensor_id,
		(!rc)?"Ok":"Failed", rc,
		ktime_us_delta(ktime_get(), startTime));
	return rc;
}

int32_t msm_camera_tz_i2c_power_down(
	struct msm_camera_i2c_client *client)
{
	int32_t rc = 0;
	int32_t sensor_id = msm_camera_tz_i2c_is_sensor_secure(client);
	ktime_t startTime = ktime_get();

	CDBG("Enter (sensor_id=%d)\n", sensor_id);
	if ((sensor_id >= 0) &&
		(sensor_id < MAX_CAMERAS) &&
		(sensor_info[sensor_id].ready != 0)) {

		msm_camera_tz_lock();
		rc = msm_camera_tz_i2c_ta_power_down(
			msm_camera_tz_get_ta_handle(),
			sensor_id);
		sensor_info[sensor_id].ready--;
		msm_camera_tz_unlock();
		if (!sensor_info[sensor_id].ready)
			rc = msm_camera_tz_unload_ta();
	} else
		rc = -EFAULT;
	CDBG("Power DOWN sensor = %d, %s(%d) - %lluus\n",
		sensor_id,
		(!rc)?"Ok":"Failed", rc,
		ktime_us_delta(ktime_get(), startTime));
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
			client, MSM_CAMERA_TZ_CMD_CCI_READ_SEQ);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_SEQ);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_ASYNC);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_SYNC);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_SYNC_BLOCK);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_SEQ_TABLE);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_TABLE_W_MICRODELAY);
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
			client, MSM_CAMERA_TZ_CMD_CCI_POLL);
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
			client, MSM_CAMERA_TZ_CMD_CCI_WRITE_CONF_TBL);
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
