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

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <soc/qcom/camera2.h>
#include "qseecom_kernel.h"
#include "msm_camera_tz_util.h"

#define EMPTY_QSEECOM_HANDLE    NULL
#define QSEECOM_SBUFF_SIZE      SZ_128K

#define MSM_CAMERA_TZ_UTIL_VERBOSE

#define MSM_CAMERA_TZ_BOOT_PROTECTED (false)

/* Update version major number in case the HLOS-TA interface is changed*/
#define TA_IF_VERSION_MAJ	    1
#define TA_IF_VERSION_MIN	    2

#undef CDBG
#ifdef MSM_CAMERA_TZ_UTIL_VERBOSE
	#define CDBG(fmt, args...) \
		pr_info(CONFIG_MSM_SEC_CCI_TA_NAME "::%s:%d - " fmt,\
		__func__, __LINE__, ##args)
#else /* MSM_CAMERA_TZ_UTIL_VERBOSE */
	#define CDBG(fmt, args...) \
		pr_debug("%s:%d - " fmt,  __func__, __LINE__, ##args)
#endif /* MSM_CAMERA_TZ_UTIL_VERBOSE */

#pragma pack(push, msm_camera_tz_util, 1)

/* MSM_CAMERA_TZ_CMD_GET_IF_VERSION */
#define msm_camera_tz_i2c_get_if_version_req_t msm_camera_tz_generic_req_t

struct msm_camera_tz_i2c_get_if_version_rsp_t {
	enum msm_camera_tz_status_t rc;
	uint32_t                    if_version_maj;
	uint32_t                    if_version_min;
};

/* MSM_CAMERA_TZ_CMD_SET_MODE */
struct msm_camera_tz_set_mode_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	uint32_t                    mode;
	uint32_t                    hw_block;
};

#define msm_camera_tz_set_mode_rsp_t msm_camera_tz_generic_rsp_t

#pragma pack(pop, msm_camera_tz_util)

/* TA communication control structure */
struct msm_camera_tz_ctrl_t {
	uint32_t                ta_enabled;
	struct qseecom_handle   *ta_qseecom_handle;
	const char              *ta_name;
	uint32_t                secure_hw_blocks;
};

static struct msm_camera_tz_ctrl_t msm_camera_tz_ctrl = {
	0, NULL, CONFIG_MSM_CAMERA_TZ_TA_NAME, 0
};

static DEFINE_MUTEX(msm_camera_tz_util_lock);

struct qseecom_handle *msm_camera_tz_get_ta_handle()
{
	return msm_camera_tz_ctrl.ta_qseecom_handle;
}

void msm_camera_tz_lock(void)
{
	mutex_lock(&msm_camera_tz_util_lock);
}

void msm_camera_tz_unlock(void)
{
	mutex_unlock(&msm_camera_tz_util_lock);
}

int32_t get_cmd_rsp_buffers(
	struct qseecom_handle *ta_qseecom_handle,
	void **cmd,	int *cmd_len,
	void **rsp,	int *rsp_len)
{
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
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_GET_IF_VERSION;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Unable to get if version info, rc=%d\n",
				__func__, __LINE__,
				rc);
			return rc;
		}

		if (rsp->rc < 0) {
			CDBG("TZ App error, rc=%d\n", rsp->rc);
			rc = -EFAULT;
		} else {
			*if_version_maj = rsp->if_version_maj;
			*if_version_min = rsp->if_version_min;
			CDBG("TZ If version %d.%d\n", *if_version_maj,
				*if_version_min);
		}
	}
	return rc;
}

int32_t msm_camera_tz_load_ta(void)
{
	int32_t rc = 0;

	if (MSM_CAMERA_TZ_BOOT_PROTECTED &&
		msm_camera_tz_ctrl.ta_enabled > 0) {
		CDBG("TA loaded from boot(TA %s - %d)\n",
			msm_camera_tz_ctrl.ta_name,
			msm_camera_tz_ctrl.ta_enabled);
		return 0;
	}

	CDBG("Enter (TA name = %s, %d)\n",
		msm_camera_tz_ctrl.ta_name,
		msm_camera_tz_ctrl.ta_enabled);

	msm_camera_tz_lock();
	if (msm_camera_tz_ctrl.ta_enabled == 0) {
		ktime_t startTime = ktime_get();

		/* Start the TA */
		if ((msm_camera_tz_ctrl.ta_qseecom_handle == NULL) &&
			(msm_camera_tz_ctrl.ta_name != NULL) &&
			('\0' != msm_camera_tz_ctrl.ta_name[0])) {
			uint32_t if_version_maj = 0;
			uint32_t if_version_min = 0;

			rc = qseecom_start_app(
				&msm_camera_tz_ctrl.ta_qseecom_handle,
				(char *)msm_camera_tz_ctrl.ta_name,
				QSEECOM_SBUFF_SIZE);
			if (!rc)
				rc = msm_camera_tz_i2c_ta_get_if_version(
					msm_camera_tz_ctrl.ta_qseecom_handle,
					&if_version_maj, &if_version_min);

			if (!rc) {
				if (if_version_maj != TA_IF_VERSION_MAJ) {
					CDBG("TA ver mismatch %d.%d != %d.%d\n",
						if_version_maj, if_version_min,
						TA_IF_VERSION_MAJ,
						TA_IF_VERSION_MIN);
					rc = qseecom_shutdown_app(
						&msm_camera_tz_ctrl.
							ta_qseecom_handle);
					msm_camera_tz_ctrl.ta_qseecom_handle
						= EMPTY_QSEECOM_HANDLE;
					rc = -EFAULT;
				} else {
					msm_camera_tz_ctrl.ta_enabled = 1;
				}
			}
		}
		CDBG("Load TA %s - %s(%d) - %lluus\n",
			msm_camera_tz_ctrl.ta_name,
			(msm_camera_tz_ctrl.ta_enabled)?"Ok" :
			"Failed", rc,
			ktime_us_delta(ktime_get(),	startTime));
	} else {
		msm_camera_tz_ctrl.ta_enabled++;
		CDBG("TA already loaded (TA %s - %d)\n",
			msm_camera_tz_ctrl.ta_name,
			msm_camera_tz_ctrl.ta_enabled);
	}
	msm_camera_tz_unlock();
	return rc;
}

int32_t msm_camera_tz_unload_ta(void)
{
	int32_t rc = -EFAULT;

	if (MSM_CAMERA_TZ_BOOT_PROTECTED) {
		CDBG("TA loaded from boot(TA %s - %d)\n",
			msm_camera_tz_ctrl.ta_name,
			msm_camera_tz_ctrl.ta_enabled);
		return 0;
	}

	CDBG("Enter (TA name = %s, %d)\n",
		msm_camera_tz_ctrl.ta_name,
		msm_camera_tz_ctrl.ta_enabled);

	msm_camera_tz_lock();
	if (msm_camera_tz_ctrl.ta_enabled == 1) {
		ktime_t startTime = ktime_get();

		rc = qseecom_shutdown_app(&msm_camera_tz_ctrl.
			ta_qseecom_handle);
		msm_camera_tz_ctrl.ta_qseecom_handle
			= EMPTY_QSEECOM_HANDLE;
		msm_camera_tz_ctrl.ta_enabled = 0;
		CDBG("Unload TA %s - %s(%d) - %lluus\n",
			msm_camera_tz_ctrl.ta_name,
			(!rc)?"Ok":"Failed", rc,
			ktime_us_delta(ktime_get(), startTime));
	} else {
		msm_camera_tz_ctrl.ta_enabled--;
		CDBG("TA still loaded (TA %s - %d)\n",
			msm_camera_tz_ctrl.ta_name,
			msm_camera_tz_ctrl.ta_enabled);
	}
	msm_camera_tz_unlock();
	return rc;
}

int32_t msm_camera_tz_ta_set_mode(uint32_t mode,
	uint32_t hw_block)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_set_mode_req_t *cmd;
	struct msm_camera_tz_set_mode_rsp_t *rsp;
	int32_t rc = 0;
	struct qseecom_handle *ta_qseecom_handle =
		msm_camera_tz_get_ta_handle();
	ktime_t startTime = ktime_get();

	if (ta_qseecom_handle == NULL) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	cmd_len = sizeof(struct msm_camera_tz_set_mode_req_t);
	rsp_len = sizeof(struct msm_camera_tz_set_mode_rsp_t);

	msm_camera_tz_lock();
	rc = get_cmd_rsp_buffers(ta_qseecom_handle,
		(void **)&cmd, &cmd_len, (void **)&rsp, &rsp_len);
	if (!rc)  {
		cmd->cmd_id = MSM_CAMERA_TZ_CMD_SET_MODE;
		cmd->mode = mode;
		cmd->hw_block = hw_block;

		rc = qseecom_send_command(ta_qseecom_handle,
			(void *)cmd, cmd_len, (void *)rsp, rsp_len);

		if (rc < 0) {
			pr_err("%s:%d - Failed: rc=%d\n",
				__func__, __LINE__,
				rc);
			msm_camera_tz_unlock();
			return rc;
		}
		rc = rsp->rc;
	}
	msm_camera_tz_unlock();
	CDBG("Done: rc=%d, Mode=0x%08X - %lluus\n",
		rc, mode,
		ktime_us_delta(ktime_get(), startTime));
	return rc;
}

uint32_t msm_camera_tz_set_mode(uint32_t mode,
	uint32_t hw_block)
{
	uint32_t rc = 0;

	switch (mode) {
	case MSM_CAMERA_TZ_MODE_SECURE:
		rc = msm_camera_tz_load_ta();
		if (!rc) {
			rc = msm_camera_tz_ta_set_mode(mode, hw_block);
			if (rc)
				msm_camera_tz_ctrl.secure_hw_blocks |= hw_block;
		}
		break;
	case MSM_CAMERA_TZ_MODE_NON_SECURE:
		msm_camera_tz_ta_set_mode(mode, hw_block);
		if (rc)
			msm_camera_tz_ctrl.secure_hw_blocks &= ~hw_block;
		rc = msm_camera_tz_unload_ta();
		break;
	default:
		pr_err("%s:%d - Incorrect mode: %d (hw: 0x%08X)\n",
			__func__, __LINE__,
			mode, hw_block);
		return -EINVAL;
	}
	CDBG("Set Mode - rc=%d, Mode: 0x%08X\n",
		rc, mode);
	return rc;
}
