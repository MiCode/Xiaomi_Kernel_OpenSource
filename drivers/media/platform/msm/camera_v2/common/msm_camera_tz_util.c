/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <soc/qcom/camera2.h>
#include <soc/qcom/scm.h>
#include "qseecom_kernel.h"
#include "msm_camera_io_util.h"
#include "msm_camera_tz_util.h"

#define MSM_CAMERA_TZ_DEFERRED_SIZE  40
#define EMPTY_QSEECOM_HANDLE    NULL
#define QSEECOM_SBUFF_SIZE      SZ_64K

#define MSM_CAMERA_TZ_OK        0
#define MSM_CAMERA_TZ_FAULT     (-EFAULT)
#define MSM_CAMERA_TZ_TRUE      1
#define MSM_CAMERA_TZ_FALSE     0

#define MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID             24
#define MSM_CAMERA_TZ_SVC_CAMERASS_VER_SHIFT           28
#define MSM_CAMERA_TZ_SVC_CAMERASS_CURRENT_VER         0x1
#define MSM_CAMERA_TZ_SVC_CAMERASS_SECURITY_STATUS     0x1
#define MSM_CAMERA_TZ_SVC_CAMERASS_REG_READ            0x2
#define MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE           0x3
#define MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE_BULK      0x4
#define MSM_CAMERA_TZ_SVC_CAMERASS_RESET_HW_BLOCK      0x5
#define MSM_CAMERA_TZ_SVC_CAMERASS_HW_BLOCK_SHIFT      21

#undef MSM_CAMERA_TZ_UTIL_VERBOSE

#define MSM_CAMERA_TZ_BOOT_PROTECTED (false)
#define MSM_CAMERA_TZ_PROTECTION_LEVEL 1

#if MSM_CAMERA_TZ_PROTECTION_LEVEL == 2
	#define MSM_CAMERA_TZ_SECURED_HW_BLOCKS \
		(MSM_CAMERA_TZ_HW_BLOCK_CSIDCORE)
#elif MSM_CAMERA_TZ_PROTECTION_LEVEL == 1
	#define MSM_CAMERA_TZ_SECURED_HW_BLOCKS \
		(MSM_CAMERA_TZ_HW_BLOCK_CSIDCORE)
#else
	#define MSM_CAMERA_TZ_SECURED_HW_BLOCKS (0)
#endif

/* Update version major number in case the HLOS-TA interface is changed*/
#define TA_IF_VERSION_MAJ	    1
#define TA_IF_VERSION_MIN	    4

#undef CDBG
#ifdef MSM_CAMERA_TZ_UTIL_VERBOSE
	#define CDBG(fmt, args...) \
		pr_info("%s:%d - " fmt, __func__, __LINE__, ##args)
#else /* MSM_CAMERA_TZ_UTIL_VERBOSE */
	#define CDBG(fmt, args...) \
		pr_debug("%s:%d - " fmt,  __func__, __LINE__, ##args)
#endif /* MSM_CAMERA_TZ_UTIL_VERBOSE */

#pragma pack(push, msm_camera_tz_util, 1)

/* MSM_CAMERA_TZ_CMD_GET_IF_VERSION */
#define msm_camera_tz_get_if_version_req_t msm_camera_tz_generic_req_t

struct msm_camera_tz_get_if_version_rsp_t {
	enum msm_camera_tz_status_t rc;
	uint32_t                    if_version_maj;
	uint32_t                    if_version_min;
};

#define msm_camera_tz_set_mode_rsp_t msm_camera_tz_generic_rsp_t

/* MSM_CAMERA_TZ_CMD_SET_MODE */
struct msm_camera_tz_set_mode_req_t {
	enum msm_camera_tz_cmd_id_t cmd_id;
	uint32_t                    mode;
	uint32_t                    hw_block;
};

#pragma pack(pop, msm_camera_tz_util)

/* TEE communication control structure
 * TZBSP Status format:
 *	bit 0-1     : reserved
 *	bit 2-20    : corresponding SCM call status, 1 - supported
 *	bit 21-27   : protected HW blocks
 *	bit 28-31   : API version
 */
struct msm_camera_tz_ctrl_t {
	uint32_t                tzbsp_status;
	uint32_t                ta_enabled;
	struct qseecom_handle   *ta_qseecom_handle;
	const char              *ta_name;
	uint32_t                secure_hw_blocks;
};

static struct msm_camera_tz_ctrl_t msm_camera_tz_ctrl = {
	0, 0, NULL, CONFIG_MSM_CAMERA_TZ_TA_NAME, 0
};

/* Register accsess relay */
struct msm_camera_tz_register_t {
	uint32_t    offset;
	uint32_t    data;
};

struct msm_camera_tz_reg_ctrl_t {
	uint32_t    num_of_deffered_registers;
	enum msm_camera_tz_io_region_t deferred_region;
	void __iomem *deferred_base_addr;
	struct msm_camera_tz_register_t
		deferred_registers[MSM_CAMERA_TZ_DEFERRED_SIZE];
};

static struct msm_camera_tz_reg_ctrl_t msm_camera_tz_reg_ctrl = {
	0,
	MSM_CAMERA_TZ_IO_REGION_LAST,
	NULL
};

static DEFINE_MUTEX(msm_camera_tz_util_lock);

static int32_t msm_camera_tz_tzbsp_reg_write(uint32_t data, uint32_t offset,
	enum msm_camera_tz_io_region_t region);

static const char *msm_camera_tz_scm_call_name(uint32_t call_id)
{
	switch (call_id) {
	case MSM_CAMERA_TZ_SVC_CAMERASS_SECURITY_STATUS:
		return "STATUS";
	case MSM_CAMERA_TZ_SVC_CAMERASS_REG_READ:
		return "REG_READ";
	case MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE:
		return "REG_WRITE";
	case MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE_BULK:
		return "REG_WRITE_BULK";
	case MSM_CAMERA_TZ_SVC_CAMERASS_RESET_HW_BLOCK:
		return "RESET_HW_BLOCK";
	default:
		return "N/A";
	}
};

static const char *msm_camera_tz_region_name(
	enum msm_camera_tz_io_region_t region)
{
	switch (region) {
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE0:
		return "CSIDCORE0";
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE1:
		return "CSIDCORE1";
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE2:
		return "CSIDCORE2";
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE3:
		return "CSIDCORE3";
	default:
		return "N/A";
	}
};

uint32_t msm_camera_tz_region_to_hw_block(
	enum msm_camera_tz_io_region_t region)
{
	switch (region) {
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE0:
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE1:
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE2:
	case MSM_CAMERA_TZ_IO_REGION_CSIDCORE3:
		return MSM_CAMERA_TZ_HW_BLOCK_CSIDCORE;
	default:
		break;
	}

	return 0;
}

static uint32_t msm_camera_tz_get_tzbsp_status(uint32_t status_mask)
{
	if (msm_camera_tz_ctrl.tzbsp_status == 0) {
		struct scm_desc desc = {
			.arginfo = SCM_ARGS(0),
		};
		ktime_t startTime = ktime_get();

		int32_t scmcall_status = scm_call2(
			SCM_SIP_FNID(
				MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID,
				MSM_CAMERA_TZ_SVC_CAMERASS_SECURITY_STATUS),
			&desc);
		if (scmcall_status) {
			CDBG("SCM call %s failed - %d\n",
				msm_camera_tz_scm_call_name(
				MSM_CAMERA_TZ_SVC_CAMERASS_SECURITY_STATUS),
				scmcall_status);
			msm_camera_tz_ctrl.tzbsp_status = 0xFFFFFFFF;
		} else {
			msm_camera_tz_ctrl.tzbsp_status = desc.ret[0];
		}
		CDBG("Done: status=0x%08X, - %lluus\n",
		msm_camera_tz_ctrl.tzbsp_status,
			ktime_us_delta(ktime_get(), startTime));
	}
	if ((msm_camera_tz_ctrl.tzbsp_status != 0xFFFFFFFF) &&
		(msm_camera_tz_ctrl.tzbsp_status & status_mask))
		return MSM_CAMERA_TZ_TRUE;

	/* TZBSP implementation is not available */
	return MSM_CAMERA_TZ_FALSE;
}

void msm_camera_tz_clear_tzbsp_status(void)
{
	msm_camera_tz_ctrl.tzbsp_status = 0;
}

uint32_t msm_camera_tz_is_secured(
	enum msm_camera_tz_io_region_t region)
{
	/* Check TZBSP API version */
	if (msm_camera_tz_get_tzbsp_status(
		MSM_CAMERA_TZ_SVC_CAMERASS_CURRENT_VER <<
			MSM_CAMERA_TZ_SVC_CAMERASS_VER_SHIFT) !=
				MSM_CAMERA_TZ_TRUE)
		return MSM_CAMERA_TZ_FALSE;

	/* Check if the region is boot protected */
	if (msm_camera_tz_get_tzbsp_status(
		(msm_camera_tz_region_to_hw_block(region) <<
			MSM_CAMERA_TZ_SVC_CAMERASS_HW_BLOCK_SHIFT)) ==
				MSM_CAMERA_TZ_TRUE)
		return MSM_CAMERA_TZ_TRUE;

	return MSM_CAMERA_TZ_FALSE;
}

struct qseecom_handle *msm_camera_tz_get_ta_handle(void)
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

static int32_t msm_camera_tz_ta_get_if_version(
	struct qseecom_handle *ta_qseecom_handle,
	uint32_t *if_version_maj,
	uint32_t *if_version_min)
{
	int32_t cmd_len, rsp_len;
	struct msm_camera_tz_get_if_version_req_t *cmd;
	struct msm_camera_tz_get_if_version_rsp_t *rsp;
	int32_t rc = 0;

	CDBG("Enter\n");
	if ((ta_qseecom_handle == NULL) ||
		(if_version_maj == NULL) || (if_version_min == NULL)) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	cmd_len = sizeof(struct msm_camera_tz_get_if_version_req_t);
	rsp_len = sizeof(struct msm_camera_tz_get_if_version_rsp_t);

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
				rc = msm_camera_tz_ta_get_if_version(
					msm_camera_tz_ctrl.ta_qseecom_handle,
					&if_version_maj, &if_version_min);

			if (!rc) {
				if (if_version_maj != TA_IF_VERSION_MAJ) {
					CDBG("TA ver mismatch %d.%d != %d.%d\n",
						if_version_maj, if_version_min,
						TA_IF_VERSION_MAJ,
						TA_IF_VERSION_MIN);
				rc = qseecom_shutdown_app(
					&msm_camera_tz_ctrl.ta_qseecom_handle);
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

		rc = qseecom_shutdown_app(
			&msm_camera_tz_ctrl.ta_qseecom_handle);
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

static int32_t msm_camera_tz_ta_set_mode(uint32_t mode,
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

static int32_t msm_camera_tz_tzbsp_reg_write_bulk(
	enum msm_camera_tz_io_region_t region)
{
	int32_t rc = 0;
	struct scm_desc desc = {0};
	uint32_t *offsets = NULL;
	uint32_t *data = NULL;
	uint32_t index;
	uint32_t buffer_size = 0;
	ktime_t startTime = ktime_get();

	if (msm_camera_tz_reg_ctrl.num_of_deffered_registers == 0 ||
		msm_camera_tz_reg_ctrl.num_of_deffered_registers >
			MSM_CAMERA_TZ_DEFERRED_SIZE) {
		pr_err("%s:%d - Bad parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	buffer_size = sizeof(uint32_t) *
		msm_camera_tz_reg_ctrl.num_of_deffered_registers;
	offsets = kzalloc(buffer_size, GFP_KERNEL);
	if (!offsets)
		return -ENOMEM;

	data = kzalloc(buffer_size, GFP_KERNEL);
	if (!data) {
		kfree(offsets);
		return -ENOMEM;
	}

	for (index = 0;
		index < msm_camera_tz_reg_ctrl.num_of_deffered_registers;
		index++) {
		offsets[index] =
			msm_camera_tz_reg_ctrl.deferred_registers[index].offset;
		data[index] =
			msm_camera_tz_reg_ctrl.deferred_registers[index].data;
	}

	desc.arginfo = SCM_ARGS(6,
		SCM_VAL, SCM_VAL, SCM_RO, SCM_VAL, SCM_RO, SCM_VAL);
	desc.args[0] = region;
	desc.args[1] = msm_camera_tz_reg_ctrl.num_of_deffered_registers;
	desc.args[2] = SCM_BUFFER_PHYS(offsets);
	desc.args[3] = buffer_size;
	desc.args[4] = SCM_BUFFER_PHYS(data);
	desc.args[5] = buffer_size;

	dmac_flush_range(offsets, offsets +
		msm_camera_tz_reg_ctrl.num_of_deffered_registers);
	dmac_flush_range(data, data +
		msm_camera_tz_reg_ctrl.num_of_deffered_registers);
	rc = scm_call2(
		SCM_SIP_FNID(
			MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID,
			MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE_BULK),
		&desc);
	kfree(offsets);
	kfree(data);
	if (rc) {
		CDBG("SCM call %s failed - %d\n",
			msm_camera_tz_scm_call_name(
				MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE_BULK),
			rc);
	}

	CDBG("Done: rc=%d, region=%s, num_of=%d - %lluus\n", rc,
		msm_camera_tz_region_name(region),
		msm_camera_tz_reg_ctrl.num_of_deffered_registers,
		ktime_us_delta(ktime_get(), startTime));

	return rc;
}

static void msm_camera_tz_flush_deferred(void)
{
	if (msm_camera_tz_reg_ctrl.num_of_deffered_registers) {
		int32_t rc = MSM_CAMERA_TZ_FAULT;
		uint32_t region = msm_camera_tz_reg_ctrl.deferred_region;
		uint32_t index;

		CDBG("Flush %d deffered registers for %s\n",
			msm_camera_tz_reg_ctrl.num_of_deffered_registers,
			msm_camera_tz_region_name(region));

		if (msm_camera_tz_is_secured(region) &&
			msm_camera_tz_get_tzbsp_status((1 <<
				MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE_BULK)))
			rc = msm_camera_tz_tzbsp_reg_write_bulk(region);
		if (rc && msm_camera_tz_is_secured(region) &&
			msm_camera_tz_get_tzbsp_status(
				(1 << MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE))) {
			for (index = 0; index <
			msm_camera_tz_reg_ctrl.num_of_deffered_registers;
			index++) {
				rc = msm_camera_tz_tzbsp_reg_write(
				msm_camera_tz_reg_ctrl.deferred_registers[
								index].data,
				msm_camera_tz_reg_ctrl.deferred_registers[
								index].offset,
								region);
				if (rc < MSM_CAMERA_TZ_OK) {
					CDBG("Failed to flush deffered ");
					CDBG("register: %08X, rc=%d\n",
				msm_camera_tz_reg_ctrl.deferred_registers[
					index].offset, rc);
				}
			}
			rc = MSM_CAMERA_TZ_OK;
		}
		msm_camera_tz_reg_ctrl.num_of_deffered_registers = 0;
		msm_camera_tz_reg_ctrl.deferred_region =
			MSM_CAMERA_TZ_IO_REGION_LAST;
		msm_camera_tz_reg_ctrl.deferred_base_addr = NULL;
	}
}

static int32_t msm_camera_tz_tzbsp_reg_read(uint32_t offset, uint32_t *data,
	enum msm_camera_tz_io_region_t region)
{
	ktime_t startTime = ktime_get();
	struct scm_desc desc = {
			.args[0] = region,
			.args[1] = offset,
			.arginfo = SCM_ARGS(2),
	};

	int32_t rc = scm_call2(
		SCM_SIP_FNID(
			MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID,
			MSM_CAMERA_TZ_SVC_CAMERASS_REG_READ),
		&desc);
	if (rc)
		CDBG("SCM call %s failed - %d\n",
			msm_camera_tz_scm_call_name(
				MSM_CAMERA_TZ_SVC_CAMERASS_REG_READ),
			rc);
	else
		*data = desc.ret[0];

	CDBG("Done: rc=%d, region=%s, offset=0x%08X, data=0x%08X - %lluus\n",
		rc, msm_camera_tz_region_name(region),
		offset, *data,
		ktime_us_delta(ktime_get(), startTime));

	return rc;
}

uint32_t msm_camera_tz_r(void __iomem *base_addr, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	uint32_t data = 0xDEADDEAD;

	if (msm_camera_tz_is_secured(region)) {
		int32_t rc = MSM_CAMERA_TZ_FAULT;

		msm_camera_tz_flush_deferred();
		if (msm_camera_tz_is_secured(region) &&
			msm_camera_tz_get_tzbsp_status(
			(1 << MSM_CAMERA_TZ_SVC_CAMERASS_REG_READ)))
			rc = msm_camera_tz_tzbsp_reg_read(offset, &data,
				region);
		return data;
	}
	data = msm_camera_io_r(base_addr + offset);

	return data;
}

static int32_t msm_camera_tz_tzbsp_reg_write(uint32_t data, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	ktime_t startTime = ktime_get();
	struct scm_desc desc = {
			.args[0] = region,
			.args[1] = offset,
			.args[2] = data,
			.arginfo = SCM_ARGS(3),
	};

	int32_t rc = scm_call2(
		SCM_SIP_FNID(
			MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID,
			MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE),
		&desc);
	if (rc)
		CDBG("SCM call %s failed - %d\n",
			msm_camera_tz_scm_call_name(
			MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE),
			rc);

	CDBG("Done: rc=%d, region=%s, offset=0x%08X, data=0x%08X - %lluus\n",
		rc, msm_camera_tz_region_name(region),
		offset, data,
		ktime_us_delta(ktime_get(), startTime));

	return rc;
}

static void msm_camera_tz_write(uint32_t data,
	void __iomem *base_addr, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	int32_t rc = MSM_CAMERA_TZ_FAULT;

	if (msm_camera_tz_reg_ctrl.num_of_deffered_registers > 0 &&
		msm_camera_tz_reg_ctrl.num_of_deffered_registers <
			MSM_CAMERA_TZ_DEFERRED_SIZE &&
		msm_camera_tz_reg_ctrl.deferred_region == region &&
		msm_camera_tz_reg_ctrl.deferred_region !=
			MSM_CAMERA_TZ_IO_REGION_LAST) {
		msm_camera_tz_w_deferred(data, base_addr, offset, region);
		msm_camera_tz_flush_deferred();
	} else {
		msm_camera_tz_flush_deferred();
		if (msm_camera_tz_is_secured(region) &&
			msm_camera_tz_get_tzbsp_status((1 <<
				MSM_CAMERA_TZ_SVC_CAMERASS_REG_WRITE)))
			rc = msm_camera_tz_tzbsp_reg_write(data, offset,
				region);
	}
}

void msm_camera_tz_w_mb(uint32_t data,
	void __iomem *base_addr, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	if (msm_camera_tz_is_secured(region)) {
		CDBG("%s::W_MB(%d) - %pK + 0x%08X(0x%08X)\n",
			msm_camera_tz_region_name(region),
			msm_camera_tz_is_secured(region),
			base_addr, offset, data);

		msm_camera_tz_write(data, base_addr, offset, region);
	} else {
		msm_camera_io_w_mb(data, base_addr + offset);
	}
}

void msm_camera_tz_w(uint32_t data,
	void __iomem *base_addr, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	if (msm_camera_tz_is_secured(region)) {
		CDBG("%s::W(%d) - %pK + 0x%08X(0x%08X)\n",
			msm_camera_tz_region_name(region),
			msm_camera_tz_is_secured(region),
			base_addr, offset, data);

		msm_camera_tz_write(data, base_addr, offset, region);
	} else {
		msm_camera_io_w(data, base_addr + offset);
	}
}

void msm_camera_tz_w_deferred(uint32_t data,
	void __iomem *base_addr, uint32_t offset,
	enum msm_camera_tz_io_region_t region)
{
	if (msm_camera_tz_is_secured(region)) {
		CDBG("%s::W(%d) - %pK + 0x%08X(0x%08X)\n",
			msm_camera_tz_region_name(region),
			msm_camera_tz_is_secured(region),
			base_addr, offset, data);

		if ((msm_camera_tz_reg_ctrl.deferred_region != region &&
				msm_camera_tz_reg_ctrl.deferred_region !=
				MSM_CAMERA_TZ_IO_REGION_LAST) ||
			 msm_camera_tz_reg_ctrl.num_of_deffered_registers >=
				MSM_CAMERA_TZ_DEFERRED_SIZE) {
			CDBG("Force flush deferred registers");
			msm_camera_tz_flush_deferred();
		}
		if (msm_camera_tz_reg_ctrl.num_of_deffered_registers == 0) {
			msm_camera_tz_reg_ctrl.deferred_region = region;
			msm_camera_tz_reg_ctrl.deferred_base_addr = base_addr;
		}
		msm_camera_tz_reg_ctrl.deferred_registers[
		msm_camera_tz_reg_ctrl.num_of_deffered_registers].offset =
									offset;
		msm_camera_tz_reg_ctrl.deferred_registers[
		msm_camera_tz_reg_ctrl.num_of_deffered_registers].data =
									data;
		msm_camera_tz_reg_ctrl.num_of_deffered_registers++;
		return;
	}
	msm_camera_io_w(data, base_addr + offset);
}

static int32_t msm_camera_tz_tzbsp_reset_hw_block(uint32_t mask,
	enum msm_camera_tz_io_region_t region)
{
	uint32_t status = -1;
	ktime_t startTime = ktime_get();
	struct scm_desc desc = {
			.args[0] = region,
			.args[1] = mask,
			.arginfo = SCM_ARGS(2),
	};

	int32_t rc = scm_call2(
		SCM_SIP_FNID(
			MSM_CAMERA_TZ_SVC_CAMERASS_CALL_ID,
			MSM_CAMERA_TZ_SVC_CAMERASS_RESET_HW_BLOCK),
		&desc);
	if (rc) {
		CDBG("SCM call %s failed - %d\n",
			msm_camera_tz_scm_call_name(
				MSM_CAMERA_TZ_SVC_CAMERASS_RESET_HW_BLOCK),
			rc);
		status = rc;
	} else {
		status = desc.ret[0];
		CDBG("SCM call returned: %d\n", status);
		if (!status) {
			/* To emulate success by wait_for_completion_timeout
			 * the return value should be > 0
			 */
			status = 1;
		}
	}

	CDBG("Done: staus=%d, region=%s - %lluus\n",
		status,
		msm_camera_tz_region_name(region),
		ktime_us_delta(ktime_get(), startTime));

	return status;
}

int32_t msm_camera_tz_reset_hw_block(
	uint32_t mask,
	enum msm_camera_tz_io_region_t region)
{
	int32_t rc = MSM_CAMERA_TZ_FAULT;

	CDBG("%s\n", msm_camera_tz_region_name(region));

	if (msm_camera_tz_is_secured(region) && msm_camera_tz_get_tzbsp_status(
		(1 << MSM_CAMERA_TZ_SVC_CAMERASS_RESET_HW_BLOCK)))
		rc = msm_camera_tz_tzbsp_reset_hw_block(mask, region);

	/* 0 - timeout (error) */
	return rc;
}
