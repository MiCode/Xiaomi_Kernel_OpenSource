// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/dma-contiguous.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_sensor_util.h"
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

int32_t cam_ois_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 1;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_setting[0].seq_type = SENSOR_VAF;
	power_info->power_setting[0].seq_val = CAM_VAF;
	power_info->power_setting[0].config_val = 1;
	power_info->power_setting[0].delay = 2;

	power_info->power_down_setting_size = 1;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting),
			GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	power_info->power_down_setting[0].seq_type = SENSOR_VAF;
	power_info->power_down_setting[0].seq_val = CAM_VAF;
	power_info->power_down_setting[0].config_val = 0;

	return rc;

free_power_settings:
	kfree(power_info->power_setting);
	power_info->power_setting = NULL;
	power_info->power_setting_size = 0;
	return rc;
}

#define OIS_TRANS_SIZE 64
#define LC124EP3_OIS_TRANS_SIZE 5 * 12

/**
 * cam_ois_get_dev_handle - get device handle
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int cam_ois_get_dev_handle(struct cam_ois_ctrl_t *o_ctrl,
	void *arg)
{
	struct cam_sensor_acquire_dev    ois_acq_dev;
	struct cam_create_dev_hdl        bridge_params;
	struct cam_control              *cmd = (struct cam_control *)arg;

	if (o_ctrl->bridge_intf.device_hdl != -1) {
		CAM_ERR(CAM_OIS, "Device is already acquired");
		return -EFAULT;
	}
	if (copy_from_user(&ois_acq_dev, u64_to_user_ptr(cmd->handle),
		sizeof(ois_acq_dev)))
		return -EFAULT;

	bridge_params.session_hdl = ois_acq_dev.session_handle;
	bridge_params.ops = &o_ctrl->bridge_intf.ops;
	bridge_params.v4l2_sub_dev_flag = 0;
	bridge_params.media_entity_flag = 0;
	bridge_params.priv = o_ctrl;

	ois_acq_dev.device_handle =
		cam_create_device_hdl(&bridge_params);
	o_ctrl->bridge_intf.device_hdl = ois_acq_dev.device_handle;
	o_ctrl->bridge_intf.session_hdl = ois_acq_dev.session_handle;

	CAM_DBG(CAM_OIS, "Device Handle: %d", ois_acq_dev.device_handle);
	if (copy_to_user(u64_to_user_ptr(cmd->handle), &ois_acq_dev,
		sizeof(struct cam_sensor_acquire_dev))) {
		CAM_ERR(CAM_OIS, "ACQUIRE_DEV: copy to user failed");
		return -EFAULT;
	}
	return 0;
}

static int cam_ois_power_up(struct cam_ois_ctrl_t *o_ctrl)
{
	int                             rc = 0;
	struct cam_hw_soc_info          *soc_info =
		&o_ctrl->soc_info;
	struct cam_ois_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t  *power_info;

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_OIS,
			"Using default power settings");
		rc = cam_ois_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Construct default ois power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"failed to fill vreg params for power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_OIS, "failed in ois power up rc %d", rc);
		return rc;
	}

	rc = camera_io_init(&o_ctrl->io_master_info);
	if (rc)
		CAM_ERR(CAM_OIS, "cci_init failed: rc: %d", rc);

	return rc;
}

/**
 * cam_ois_power_down - power down OIS device
 * @o_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int cam_ois_power_down(struct cam_ois_ctrl_t *o_ctrl)
{
	int32_t                         rc = 0;
	struct cam_sensor_power_ctrl_t  *power_info;
	struct cam_hw_soc_info          *soc_info =
		&o_ctrl->soc_info;
	struct cam_ois_soc_private *soc_private;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "failed: o_ctrl %pK", o_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &o_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_OIS, "failed: power_info %pK", power_info);
		return -EINVAL;
	}

	rc = cam_sensor_util_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_OIS, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&o_ctrl->io_master_info);

	return rc;
}

static int cam_ois_apply_settings(struct cam_ois_ctrl_t *o_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;
	uint32_t i, size;

	if (o_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_OIS, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		if (i2c_list->op_code ==  CAM_SENSOR_I2C_WRITE_RANDOM) {
			rc = camera_io_dev_write(&(o_ctrl->io_master_info),
				&(i2c_list->i2c_settings));
			if (rc < 0) {
				CAM_ERR(CAM_OIS,
					"Failed in Applying i2c wrt settings");
				return rc;
			}
		} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
			size = i2c_list->i2c_settings.size;
			for (i = 0; i < size; i++) {
				rc = camera_io_dev_poll(
				&(o_ctrl->io_master_info),
				i2c_list->i2c_settings.reg_setting[i].reg_addr,
				i2c_list->i2c_settings.reg_setting[i].reg_data,
				i2c_list->i2c_settings.reg_setting[i].data_mask,
				i2c_list->i2c_settings.addr_type,
				i2c_list->i2c_settings.data_type,
				i2c_list->i2c_settings.reg_setting[i].delay);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
						"i2c poll apply setting Fail");
					return rc;
				}
			}
		}
	}

	return rc;
}

static int cam_ois_slaveInfo_pkt_parser(struct cam_ois_ctrl_t *o_ctrl,
	uint32_t *cmd_buf, size_t len)
{
	int32_t rc = 0;
	struct cam_cmd_ois_info *ois_info;

	if (!o_ctrl || !cmd_buf || len < sizeof(struct cam_cmd_ois_info)) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	ois_info = (struct cam_cmd_ois_info *)cmd_buf;
	if (o_ctrl->io_master_info.master_type == CCI_MASTER) {
		o_ctrl->io_master_info.cci_client->i2c_freq_mode =
			ois_info->i2c_freq_mode;
		o_ctrl->io_master_info.cci_client->sid =
			ois_info->slave_addr >> 1;
		o_ctrl->ois_fw_flag = ois_info->ois_fw_flag;
		o_ctrl->is_ois_calib = ois_info->is_ois_calib;
		o_ctrl->is_ois_pre_init = ois_info->is_ois_pre_init; //xiaomi add
		memcpy(o_ctrl->ois_name, ois_info->ois_name, OIS_NAME_LEN);
		o_ctrl->ois_name[OIS_NAME_LEN - 1] = '\0';
		o_ctrl->io_master_info.cci_client->retries = 3;
		o_ctrl->io_master_info.cci_client->id_map = 0;
		/* xiaomi add disable cci optmz for OIS by default */
		o_ctrl->io_master_info.cci_client->disable_optmz = 1;
		memcpy(&(o_ctrl->opcode), &(ois_info->opcode),
			sizeof(struct cam_ois_opcode));
		CAM_DBG(CAM_OIS, "Slave addr: 0x%x Freq Mode: %d, disable optmz %d",
			ois_info->slave_addr, ois_info->i2c_freq_mode,
			o_ctrl->io_master_info.cci_client->disable_optmz);
	} else if (o_ctrl->io_master_info.master_type == I2C_MASTER) {
		o_ctrl->io_master_info.client->addr = ois_info->slave_addr;
		CAM_DBG(CAM_OIS, "Slave addr: 0x%x", ois_info->slave_addr);
	} else {
		CAM_ERR(CAM_OIS, "Invalid Master type : %d",
			o_ctrl->io_master_info.master_type);
		rc = -EINVAL;
	}

	return rc;
}

static int cam_ois_fw_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, cnt, i;
	uint32_t                           fw_size;
	uint32_t                           fw_size_xm;
	uint32_t                           prog_addr;
	uint32_t                           coeff_addr;
	uint32_t                           mem_addr;
	uint32_t                           pheripheral_addr;
	const struct firmware             *fw = NULL;
	const struct firmware             *fw_xm = NULL;
	const char                        *fw_name_prog = NULL;
	const char                        *fw_name_coeff = NULL;
	const char                        *fw_name_mem = NULL;
	const char                        *fw_name_ph = NULL;
	char                               name_prog[32] = {0};
	char                               name_coeff[32] = {0};
	char                               name_mem[32] = {0};
	char                               name_ph[32] = {0};
	struct device                     *dev = &(o_ctrl->pdev->dev);
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
	struct page                       *page_xm = NULL;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	prog_addr = o_ctrl->opcode.prog;
	coeff_addr = o_ctrl->opcode.coeff;
	mem_addr = o_ctrl->opcode.memory;
	pheripheral_addr = o_ctrl->opcode.pheripheral;

	snprintf(name_coeff, 32, "%s.coeff", o_ctrl->ois_name);

	snprintf(name_prog, 32, "%s.prog", o_ctrl->ois_name);

	snprintf(name_mem, 32, "%s.mem", o_ctrl->ois_name);

	snprintf(name_ph, 32, "%s.ph", o_ctrl->ois_name);

	/* cast pointer as const pointer*/
	fw_name_prog = name_prog;
	fw_name_coeff = name_coeff;
	fw_name_mem = name_mem;
	fw_name_ph = name_ph;
	/* Load FW */
	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_prog);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = o_ctrl->opcode.fw_addr_type;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	for (i = 0, ptr = (uint8_t *)fw->data; i < total_bytes;) {
		for (cnt = 0; cnt < OIS_TRANS_SIZE && i < total_bytes;
			cnt++, ptr++, i++) {
			i2c_reg_setting.reg_setting[cnt].reg_addr = prog_addr;
			i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
			i2c_reg_setting.reg_setting[cnt].delay = 0;
			i2c_reg_setting.reg_setting[cnt].data_mask = 0;
		}
		i2c_reg_setting.size = cnt;
		if (o_ctrl->opcode.is_addr_increase)
			prog_addr += cnt;
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&i2c_reg_setting, 1);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
			goto release_firmware;
		}
	}
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	page = NULL;
	fw_size = 0;
	release_firmware(fw);

	rc = request_firmware(&fw, fw_name_coeff, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_coeff);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = o_ctrl->opcode.fw_addr_type;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	for (i = 0, ptr = (uint8_t *)fw->data; i < total_bytes;) {
		for (cnt = 0; cnt < OIS_TRANS_SIZE && i < total_bytes;
			cnt++, ptr++, i++) {
			i2c_reg_setting.reg_setting[cnt].reg_addr = coeff_addr;
			i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
			i2c_reg_setting.reg_setting[cnt].delay = 0;
			i2c_reg_setting.reg_setting[cnt].data_mask = 0;
		}
		i2c_reg_setting.size = cnt;
		if (o_ctrl->opcode.is_addr_increase)
			coeff_addr += cnt;
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&i2c_reg_setting, 1);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
	}

	/* Load xxx.mem added by xiaomi*/
	rc = request_firmware(&fw_xm, fw_name_mem, dev);
	if (rc) {
		CAM_INFO(CAM_OIS, "no fw named %s, skip", fw_name_mem);
		rc = 0;
	} else {
		total_bytes = fw_xm->size;
		i2c_reg_setting.addr_type = o_ctrl->opcode.fw_addr_type;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.size = total_bytes;
		i2c_reg_setting.delay = 0;
		fw_size_xm = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
			total_bytes) >> PAGE_SHIFT;
		page_xm = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
			fw_size_xm, 0, GFP_KERNEL);
		if (!page_xm) {
			CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
			release_firmware(fw_xm);
			return -ENOMEM;
		}

		i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
			page_address(page_xm));

		for (i = 0, ptr = (uint8_t *)fw_xm->data; i < total_bytes;) {
			for (cnt = 0; cnt < OIS_TRANS_SIZE && i < total_bytes;
				cnt++, ptr++, i++) {
				i2c_reg_setting.reg_setting[cnt].reg_addr = mem_addr;
				i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
				i2c_reg_setting.reg_setting[cnt].delay = 0;
				i2c_reg_setting.reg_setting[cnt].data_mask = 0;
			}
			i2c_reg_setting.size = cnt;
			if (o_ctrl->opcode.is_addr_increase)
				mem_addr += cnt;
			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0)
				CAM_ERR(CAM_OIS, "OIS FW Memory download failed %d", rc);
		}
		cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
			page_xm, fw_size_xm);
		page_xm = NULL;
		fw_size_xm = 0;
		release_firmware(fw_xm);
	}

	/* Load xxx.ph added by xiaomi, not used by now*/
	/*
	rc = request_firmware(&fw_xm, fw_name_ph, dev);
	if (rc) {
		CAM_INFO(CAM_OIS, "Failed to locate %s, not error", fw_name_ph);
		rc = 0;
	} else {
		total_bytes = fw_xm->size;
		i2c_reg_setting.addr_type = o_ctrl->opcode.fw_addr_type;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.size = total_bytes;
		i2c_reg_setting.delay = 0;
		fw_size_xm = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
			total_bytes) >> PAGE_SHIFT;
		page_xm = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
			fw_size_xm, 0, GFP_KERNEL);
		if (!page_xm) {
			CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
			release_firmware(fw_xm);
			return -ENOMEM;
		}

		i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
			page_address(page_xm));

		for (i = 0, ptr = (uint8_t *)fw_xm->data; i < total_bytes;) {
				for (cnt = 0; cnt < OIS_TRANS_SIZE && i < total_bytes;
					cnt++, ptr++, i++) {
					i2c_reg_setting.reg_setting[cnt].reg_addr = pheripheral_addr;
					i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
					i2c_reg_setting.reg_setting[cnt].delay = 0;
					i2c_reg_setting.reg_setting[cnt].data_mask = 0;
				}
			i2c_reg_setting.size = cnt;
			if (o_ctrl->opcode.is_addr_increase)
				pheripheral_addr += cnt;
			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0)
				CAM_ERR(CAM_OIS, "OIS FW Memory download failed %d", rc);
		}
		cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
			page_xm, fw_size_xm);
		page_xm = NULL;
		fw_size_xm = 0;
		release_firmware(fw_xm);
	}
	*/

release_firmware:
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	release_firmware(fw);

	return rc;
}


struct cam_sensor_i2c_reg_array ois_pm_add_array[]= {
	{0x30, 0x00, 0x0, 0x0},
	{0x30, 0x10, 0x0, 0x0},
	{0x30, 0x00, 0x0, 0x0},
	{0x30, 0x00, 0x0, 0x0},
};

struct cam_sensor_i2c_reg_array ois_pm_length_array[]= {
	{0xF0, 0x0A, 0x0, 0x0},
	{0xF0, 0x07, 0x0, 0x0},
	{0xF0, 0x59, 0x0, 0x0},
};

static int cam_lc898124_ois_fw_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, cnt, i;
	uint32_t                           fw_size;
	uint32_t                           fw_size_xm;
	uint32_t                           prog_addr;
	uint32_t                           coeff_addr;
	uint32_t                           mem_addr;
	const struct firmware             *fw = NULL;
	const struct firmware             *fw_xm = NULL;
	const char                        *fw_name_prog = NULL;
	const char                        *fw_name_coeff = NULL;
	const char                        *fw_name_mem = NULL;
	char                               name_prog[32] = {0};
	char                               name_coeff[32] = {0};
	char                               name_mem[32] = {0};
	struct device                     *dev = &(o_ctrl->pdev->dev);
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
	struct page                       *page_xm = NULL;
	int32_t rtc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	uint32_t  DMA_ByteSize = 0x0054;
	uint32_t  DMB_ByteSize = 0x0498;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	prog_addr = o_ctrl->opcode.prog;
	coeff_addr = o_ctrl->opcode.coeff;
	mem_addr = o_ctrl->opcode.memory;

	snprintf(name_coeff, 32, "%s.coeff", o_ctrl->ois_name);

	snprintf(name_prog, 32, "%s.prog", o_ctrl->ois_name);

	snprintf(name_mem, 32, "%s.mem", o_ctrl->ois_name);

	/* cast pointer as const pointer*/
	fw_name_prog = name_prog;
	fw_name_coeff = name_coeff;
	fw_name_mem = name_mem;

	/* PM data address write */
	write_setting.size=  sizeof(ois_pm_add_array)/sizeof(struct cam_sensor_i2c_reg_array);
	write_setting.addr_type =CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type= CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay=0;
	write_setting.reg_setting= ois_pm_add_array;

	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&write_setting, 1);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS ois pm add failed %d", rc);
	}

	/* Load FW */
	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_prog);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	for (i = 0, ptr = (uint8_t *)fw->data; i < total_bytes;) {
		CAM_DBG(CAM_OIS, "download  %s prog_addr =0x%02x total_bytes=%d", fw_name_prog, prog_addr,total_bytes);
		for (cnt = 0; cnt < LC124EP3_OIS_TRANS_SIZE && i < total_bytes;
			cnt++, ptr++, i++) {
			i2c_reg_setting.reg_setting[cnt].reg_addr = prog_addr;
			i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
			i2c_reg_setting.reg_setting[cnt].delay = 0;
			i2c_reg_setting.reg_setting[cnt].data_mask = 0;
		}
		i2c_reg_setting.size = cnt;
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&i2c_reg_setting, 1);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
			goto release_firmware;
		}
	}

	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	page = NULL;
	fw_size = 0;
	release_firmware(fw);

    /* write 0xF00A PM size*/

	CAM_DBG(CAM_OIS, "PM size write");
	write_setting.size=  sizeof(ois_pm_length_array)/sizeof(struct cam_sensor_i2c_reg_array);
	write_setting.addr_type =CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type= CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay=0;
	write_setting.reg_setting= ois_pm_length_array;

		rtc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&write_setting, 1);
		if (rtc < 0) {
			CAM_ERR(CAM_OIS, "OIS 0xF00A PM size failed %d", rc);
		}

 /* load coeff download*/
	rc = request_firmware(&fw, fw_name_coeff, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_coeff);
		return rc;
	}

	total_bytes = fw->size;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 0;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	if( total_bytes == ((DMA_ByteSize + DMB_ByteSize) * 6 / 4))
	{
		ptr = (uint8_t *)fw->data;
		coeff_addr = ptr[0];
		for (i = 0 ; i < (DMA_ByteSize *6 /4);) {
			CAM_DBG(CAM_OIS, "download DMA %s,coeff_addr=0x%04x", fw_name_coeff,coeff_addr);
			for (cnt = 0; cnt < LC124EP3_OIS_TRANS_SIZE && i < (DMA_ByteSize *6 /4);
				cnt++, i++) {
				i2c_reg_setting.reg_setting[cnt].reg_addr = coeff_addr;
				i2c_reg_setting.reg_setting[cnt].reg_data = ptr[i+1];
				i2c_reg_setting.reg_setting[cnt].delay = 0;
				i2c_reg_setting.reg_setting[cnt].data_mask = 0;
			}
			i2c_reg_setting.size = cnt;
			coeff_addr = ptr[0+i];
			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
				goto release_firmware;
			}
		}
		for (i = (DMA_ByteSize *6 /4); i < total_bytes;) {
			CAM_DBG(CAM_OIS, "download DMB %s,coeff_addr=0x%04x", fw_name_coeff,coeff_addr);
			for (cnt = 0; cnt < LC124EP3_OIS_TRANS_SIZE && i < total_bytes;
				cnt++, i++) {
				i2c_reg_setting.reg_setting[cnt].reg_addr = coeff_addr;
				i2c_reg_setting.reg_setting[cnt].reg_data = ptr[i+1];
				i2c_reg_setting.reg_setting[cnt].delay = 0;
				i2c_reg_setting.reg_setting[cnt].data_mask = 0;
			}
			i2c_reg_setting.size = cnt;
			coeff_addr = ptr[i];
			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "OIS FW download failed %d", rc);
				goto release_firmware;
			}
		}
	}else{
		CAM_ERR(CAM_OIS, "OIS FW DM download failed %d", rc);
		goto release_firmware;
	}


	/* Load xxx.mem added by xiaomi*/
	rc = request_firmware(&fw_xm, fw_name_mem, dev);
	if (rc) {
		CAM_INFO(CAM_OIS, "no fw named %s, skip", fw_name_mem);
		rc = 0;
	} else {
		total_bytes = fw_xm->size;
		i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.size = total_bytes;
		i2c_reg_setting.delay = 0;
		fw_size_xm = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
			total_bytes) >> PAGE_SHIFT;
		page_xm = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
			fw_size_xm, 0, GFP_KERNEL);
		if (!page_xm) {
			CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
			release_firmware(fw);
			return -ENOMEM;
		}

		i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
			page_address(page_xm));

		ptr = (uint8_t *)fw_xm->data;
		mem_addr = ptr[0];
		for (i = 0; i < total_bytes;) {
			CAM_DBG(CAM_OIS, "download	%s,mem_addr=0x%04x", fw_name_mem,mem_addr);
			for (cnt = 0; cnt < LC124EP3_OIS_TRANS_SIZE && i < total_bytes;
				cnt++, i++) {
				i2c_reg_setting.reg_setting[cnt].reg_addr = mem_addr;
				i2c_reg_setting.reg_setting[cnt].reg_data = ptr[i+1];
				i2c_reg_setting.reg_setting[cnt].delay = 0;
				i2c_reg_setting.reg_setting[cnt].data_mask = 0;
			}
			i2c_reg_setting.size = cnt;
				mem_addr = ptr[i];
			rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
				&i2c_reg_setting, 1);
			if (rc < 0)
				CAM_ERR(CAM_OIS, "OIS FW Memory download failed %d", rc);
		}
		cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
			page_xm, fw_size_xm);
		page_xm = NULL;
		fw_size_xm = 0;
		release_firmware(fw_xm);
	}


release_firmware:
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	release_firmware(fw);

	return rc;

}

#ifdef ENABLE_OIS_EIS
static int cam_ois_get_data(struct cam_ois_ctrl_t *o_ctrl,
		struct cam_packet *csl_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0;
	int                   rc = 0;
	uintptr_t             buf_addr;
	size_t                buf_size;
	uint8_t               *read_buffer;
	uint32_t num_data = sizeof(o_ctrl->ois_data.data);
	struct timespec64     ts64;
	cycles_t              t_now;
	uint64_t              boottime64;

	memset(&o_ctrl->ois_data, 0, sizeof(struct ois_data_eis_t));
	get_monotonic_boottime64(&ts64);
	t_now = get_cycles();
	boottime64 = (uint64_t)((ts64.tv_sec * 1000000000) + ts64.tv_nsec);

	rc = camera_io_dev_read_seq(&(o_ctrl->io_master_info),
			OIS_DATA_ADDR, o_ctrl->ois_data.data,
			CAMERA_SENSOR_I2C_TYPE_BYTE, CAMERA_SENSOR_I2C_TYPE_BYTE,
			num_data);
	o_ctrl->ois_data.data_timestamp = (uint64_t)(t_now*10000/192);//< QTimer Freq = 19.2 MHz

	if (rc < 0) {
		CAM_ERR(CAM_OIS, "read failed");
	} else {
		CAM_DBG(CAM_OIS, "ois_data count=%d,data_timestamp=%llu,boottime64=%llu,t_now=%llu",
				o_ctrl->ois_data.data[0], o_ctrl->ois_data.data_timestamp, boottime64, t_now);
	}

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
			&csl_packet->payload +
			csl_packet->io_configs_offset);

	CAM_DBG(CAM_OIS, "number of IO configs: %d:",
			csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_DBG(CAM_OIS, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
					&buf_addr, &buf_size);
			if (rc) {
				CAM_ERR(CAM_OIS, "Fail in get buffer: %d",
						rc);
				return rc;
			}

			CAM_DBG(CAM_OIS, "buf_addr : %pK, buf_size : %zu\n",
					(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_OIS,
						"invalid buffer to copy data");
				rc = -EINVAL;
				return rc;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size != sizeof(struct ois_data_eis_t)) {
				CAM_ERR(CAM_OIS,
						"failed to copy, Invalid size");
				rc = -EINVAL;
				return rc;
			}

			CAM_DBG(CAM_OIS, "copy the data, len:%d",
					num_data);
			memcpy(read_buffer, &o_ctrl->ois_data, sizeof(struct ois_data_eis_t));

		} else {
			CAM_ERR(CAM_OIS, "Invalid direction");
			rc = -EINVAL;
		}
	}

	return rc;
}
#endif

/**
 * cam_ois_pkt_parse - Parse csl packet
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int cam_ois_pkt_parse(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int32_t                         rc = 0;
	int32_t                         i = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uintptr_t                       generic_ptr;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	struct i2c_settings_array      *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc        *cmd_desc = NULL;
	uintptr_t                       generic_pkt_addr;
	size_t                          pkt_len;
	size_t                          remain_len = 0;
	struct cam_packet              *csl_packet = NULL;
	size_t                          len_of_buff = 0;
	uint32_t                       *offset = NULL, *cmd_buf;
	struct cam_ois_soc_private     *soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t  *power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&dev_config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(dev_config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(dev_config.packet_handle,
		&generic_pkt_addr, &pkt_len);
	if (rc) {
		CAM_ERR(CAM_OIS,
			"error in converting command Handle Error: %d", rc);
		return rc;
	}

	remain_len = pkt_len;
	if ((sizeof(struct cam_packet) > pkt_len) ||
		((size_t)dev_config.offset >= pkt_len -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_OIS,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), pkt_len);
		return -EINVAL;
	}

	remain_len -= (size_t)dev_config.offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_OIS, "Invalid packet params");
		return -EINVAL;
	}


	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_OIS_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		/* Loop through multiple command buffers */
		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;

			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
				&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "Failed to get cpu buf : 0x%x",
					cmd_desc[i].mem_handle);
				return rc;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_OIS, "invalid cmd buf");
				return -EINVAL;
			}

			if ((len_of_buff < sizeof(struct common_header)) ||
				(cmd_desc[i].offset > (len_of_buff -
				sizeof(struct common_header)))) {
				CAM_ERR(CAM_OIS,
					"Invalid length for sensor cmd");
				return -EINVAL;
			}
			remain_len = len_of_buff - cmd_desc[i].offset;
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				rc = cam_ois_slaveInfo_pkt_parser(
					o_ctrl, cmd_buf, remain_len);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
					"Failed in parsing slave info");
					return rc;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_OIS,
					"Received power settings buffer");
				rc = cam_sensor_update_power_settings(
					cmd_buf,
					total_cmd_buf_in_bytes,
					power_info, remain_len);
				if (rc) {
					CAM_ERR(CAM_OIS,
					"Failed: parse power settings");
					return rc;
				}
				break;
			default:
			if (o_ctrl->i2c_init_data.is_settings_valid == 0) {
				CAM_DBG(CAM_OIS,
				"Received init settings");
				i2c_reg_settings =
					&(o_ctrl->i2c_init_data);
				i2c_reg_settings->is_settings_valid = 1;
				i2c_reg_settings->request_id = 0;
				rc = cam_sensor_i2c_command_parser(
					&o_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
					"init parsing failed: %d", rc);
					return rc;
				}
			} else if ((o_ctrl->is_ois_calib != 0) &&
				(o_ctrl->i2c_calib_data.is_settings_valid ==
				0)) {
				CAM_DBG(CAM_OIS,
					"Received calib settings");
				i2c_reg_settings = &(o_ctrl->i2c_calib_data);
				i2c_reg_settings->is_settings_valid = 1;
				i2c_reg_settings->request_id = 0;
				rc = cam_sensor_i2c_command_parser(
					&o_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
						"Calib parsing failed: %d", rc);
					return rc;
				}
			} else if ((o_ctrl->is_ois_pre_init != 0) && //xiaomi add begin
				(o_ctrl->i2c_pre_init_data.is_settings_valid ==
				0)) {
				CAM_DBG(CAM_OIS,
					"Received pre init settings");
				i2c_reg_settings = &(o_ctrl->i2c_pre_init_data);
				i2c_reg_settings->is_settings_valid = 1;
				i2c_reg_settings->request_id = 0;
				rc = cam_sensor_i2c_command_parser(
					&o_ctrl->io_master_info,
					i2c_reg_settings,
					&cmd_desc[i], 1);
				if (rc < 0) {
					CAM_ERR(CAM_OIS,
						"pre init settings parsing failed: %d", rc);
					return rc;
				}
			} //xiaomi add end
			break;
			}
		}

		if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
			rc = cam_ois_power_up(o_ctrl);
			if (rc) {
				CAM_ERR(CAM_OIS, " OIS Power up failed");
				return rc;
			}
			o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
		}

		//xiaomi add begin
		if (o_ctrl->is_ois_pre_init) {
			CAM_DBG(CAM_OIS, "apply pre init settings");
			rc = cam_ois_apply_settings(o_ctrl,
				&o_ctrl->i2c_pre_init_data);
			if (rc) {
				CAM_ERR(CAM_OIS, "Cannot apply pre init data");
				goto pwr_dwn;
			}
		} //xiaomi add end

		if (o_ctrl->ois_fw_flag) {
			/* xiaomi add begin */
			if(o_ctrl->opcode.is_addr_indata) {
				CAM_DBG(CAM_OIS, "apply lc898124 ois_fw settings");
				rc = cam_lc898124_ois_fw_download(o_ctrl);
			/* xiaomi add end */
			} else {
				CAM_DBG(CAM_OIS, "apply ois_fw settings");
				rc = cam_ois_fw_download(o_ctrl);
			}
			if (rc) {
				CAM_ERR(CAM_OIS, "Failed OIS FW Download");
				goto pwr_dwn;
			}
		}

		CAM_DBG(CAM_OIS, "apply init settings");
		rc = cam_ois_apply_settings(o_ctrl, &o_ctrl->i2c_init_data);
		if ((rc == -EAGAIN) &&
			(o_ctrl->io_master_info.master_type == CCI_MASTER)) {
			CAM_WARN(CAM_OIS,
				"CCI HW is restting: Reapplying INIT settings");
			usleep_range(1000, 1010);
			rc = cam_ois_apply_settings(o_ctrl,
				&o_ctrl->i2c_init_data);
		}
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Cannot apply Init settings: rc = %d",
				rc);
			goto pwr_dwn;
		}

		if (o_ctrl->is_ois_calib) {
			rc = cam_ois_apply_settings(o_ctrl,
				&o_ctrl->i2c_calib_data);
			if (rc) {
				CAM_ERR(CAM_OIS, "Cannot apply calib data");
				goto pwr_dwn;
			}
		}


		// xiaomi add begin
		rc = delete_request(&o_ctrl->i2c_pre_init_data);
		if (rc < 0) {
			CAM_WARN(CAM_OIS,
				"Fail deleting Pre Init data: rc: %d", rc);
			rc = 0;
		} //xiaomi add end

		rc = delete_request(&o_ctrl->i2c_init_data);
		if (rc < 0) {
			CAM_WARN(CAM_OIS,
				"Fail deleting Init data: rc: %d", rc);
			rc = 0;
		}
		rc = delete_request(&o_ctrl->i2c_calib_data);
		if (rc < 0) {
			CAM_WARN(CAM_OIS,
				"Fail deleting Calibration data: rc: %d", rc);
			rc = 0;
		}
		break;
	case CAM_OIS_PACKET_OPCODE_OIS_CONTROL:
		if (o_ctrl->cam_ois_state < CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
				"Not in right state to control OIS: %d",
				o_ctrl->cam_ois_state);
			return rc;
		}
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		i2c_reg_settings = &(o_ctrl->i2c_mode_data);
		i2c_reg_settings->is_settings_valid = 1;
		i2c_reg_settings->request_id = 0;
		rc = cam_sensor_i2c_command_parser(&o_ctrl->io_master_info,
			i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS pkt parsing failed: %d", rc);
			return rc;
		}

		rc = cam_ois_apply_settings(o_ctrl, i2c_reg_settings);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "Cannot apply mode settings");
			return rc;
		}

		rc = delete_request(i2c_reg_settings);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
				"Fail deleting Mode data: rc: %d", rc);
			return rc;
		}
		break;
#ifdef ENABLE_OIS_EIS
	case CAM_OIS_PACKET_OPCODE_OIS_GETDATA:
		if (o_ctrl->cam_ois_state < CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_ERR(CAM_OIS,
					"Not in right state to control OIS: %d",
					o_ctrl->cam_ois_state);
			return rc;
		}
		rc = cam_ois_get_data(o_ctrl, csl_packet);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,
					"Fail ois_get_data: rc: %d", rc);
			return rc;
		}
		break;
#endif
	default:
		CAM_ERR(CAM_OIS, "Invalid Opcode: %d",
			(csl_packet->header.op_code & 0xFFFFFF));
		return -EINVAL;
	}

	if (!rc)
		return rc;
pwr_dwn:
	cam_ois_power_down(o_ctrl);
	return rc;
}

void cam_ois_shutdown(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	struct cam_ois_soc_private *soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	if (o_ctrl->cam_ois_state == CAM_OIS_INIT)
		return;

	if (o_ctrl->cam_ois_state >= CAM_OIS_CONFIG) {
		rc = cam_ois_power_down(o_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "OIS Power down failed");
		o_ctrl->cam_ois_state = CAM_OIS_ACQUIRE;
	}

	if (o_ctrl->cam_ois_state >= CAM_OIS_ACQUIRE) {
		rc = cam_destroy_device_hdl(o_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "destroying the device hdl");
		o_ctrl->bridge_intf.device_hdl = -1;
		o_ctrl->bridge_intf.link_hdl = -1;
		o_ctrl->bridge_intf.session_hdl = -1;
	}

	if (o_ctrl->i2c_mode_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_mode_data);

	if (o_ctrl->i2c_calib_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_calib_data);

	if (o_ctrl->i2c_init_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_init_data);

	// xiaomi add
	if (o_ctrl->i2c_pre_init_data.is_settings_valid == 1)
		delete_request(&o_ctrl->i2c_pre_init_data);

	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;

	o_ctrl->cam_ois_state = CAM_OIS_INIT;
}

/**
 * cam_ois_driver_cmd - Handle ois cmds
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int cam_ois_driver_cmd(struct cam_ois_ctrl_t *o_ctrl, void *arg)
{
	int                              rc = 0;
	struct cam_ois_query_cap_t       ois_cap = {0};
	struct cam_control              *cmd = (struct cam_control *)arg;
	struct cam_ois_soc_private      *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

	if (!o_ctrl || !cmd) {
		CAM_ERR(CAM_OIS, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_OIS, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	mutex_lock(&(o_ctrl->ois_mutex));
	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		ois_cap.slot_info = o_ctrl->soc_info.index;

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&ois_cap,
			sizeof(struct cam_ois_query_cap_t))) {
			CAM_ERR(CAM_OIS, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
		CAM_DBG(CAM_OIS, "ois_cap: ID: %d", ois_cap.slot_info);
		break;
	case CAM_ACQUIRE_DEV:
		rc = cam_ois_get_dev_handle(o_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_OIS, "Failed to acquire dev");
			goto release_mutex;
		}

		o_ctrl->cam_ois_state = CAM_OIS_ACQUIRE;
		break;
	case CAM_START_DEV:
		if (o_ctrl->cam_ois_state != CAM_OIS_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
			"Not in right state for start : %d",
			o_ctrl->cam_ois_state);
			goto release_mutex;
		}
		o_ctrl->cam_ois_state = CAM_OIS_START;
		break;
	case CAM_CONFIG_DEV:
		rc = cam_ois_pkt_parse(o_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_OIS, "Failed in ois pkt Parsing");
			goto release_mutex;
		}
		break;
	case CAM_RELEASE_DEV:
		if (o_ctrl->cam_ois_state == CAM_OIS_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
				"Cant release ois: in start state");
			goto release_mutex;
		}

		if (o_ctrl->cam_ois_state == CAM_OIS_CONFIG) {
			rc = cam_ois_power_down(o_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_OIS, "OIS Power down failed");
				goto release_mutex;
			}
		}

		if (o_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_OIS, "link hdl: %d device hdl: %d",
				o_ctrl->bridge_intf.device_hdl,
				o_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(o_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "destroying the device hdl");
		o_ctrl->bridge_intf.device_hdl = -1;
		o_ctrl->bridge_intf.link_hdl = -1;
		o_ctrl->bridge_intf.session_hdl = -1;
		o_ctrl->cam_ois_state = CAM_OIS_INIT;

		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_down_setting_size = 0;
		power_info->power_setting_size = 0;

		if (o_ctrl->i2c_mode_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_mode_data);

		if (o_ctrl->i2c_calib_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_calib_data);

		if (o_ctrl->i2c_init_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_init_data);

		// xiaomi add
		if (o_ctrl->i2c_pre_init_data.is_settings_valid == 1)
			delete_request(&o_ctrl->i2c_pre_init_data);

		break;
	case CAM_STOP_DEV:
		if (o_ctrl->cam_ois_state != CAM_OIS_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_OIS,
			"Not in right state for stop : %d",
			o_ctrl->cam_ois_state);
		}
		o_ctrl->cam_ois_state = CAM_OIS_CONFIG;
		break;
	default:
		CAM_ERR(CAM_OIS, "invalid opcode");
		goto release_mutex;
	}
release_mutex:
	mutex_unlock(&(o_ctrl->ois_mutex));
	return rc;
}
