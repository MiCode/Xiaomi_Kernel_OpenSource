/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/firmware.h>
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_sensor_util.h"
#include "cam_trace.h"
#include "cam_res_mgr_api.h"

#define FIRMWARE_NAME "bu64748gwz.prog"
#define ACTUATOR_TRANS_SIZE 32

#ifdef CONFIG_USE_ROHM_BU64753
extern uint8_t g_eeprom_mapdata[EEPROM_MAP_DATA_CNT];
#endif



int32_t cam_actuator_construct_default_power_setting(
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0;

	power_info->power_setting_size = 1;
	power_info->power_setting =
		(struct cam_sensor_power_setting *)
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
		(struct cam_sensor_power_setting *)
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
	return rc;
}

#ifdef CONFIG_USE_ROHM_BU64753
struct cam_sensor_i2c_reg_array actuator_driver_init0_0 = {
		0x07, 0x0000, 0x00, 0x0
};
struct cam_sensor_i2c_reg_array actuator_driver_init0_1 = {
		0x07, 0x0080, 0x00, 0x0
};
struct cam_sensor_i2c_reg_array actuator_driver_init1 = {
		0x52, 0x8000, 0x00, 0x0
};
struct cam_sensor_i2c_reg_array actuator_driver_init2 = {
		0x50, 0x0000, 0x00, 0x0
};
struct cam_sensor_i2c_reg_array actuator_driver_init3 = {
		0x50, 0x000E, 0x00, 0x0
};
struct cam_sensor_i2c_reg_array actuator_driver_init4 = {
		0x50, 0x002F, 0x00, 0x0
};

struct cam_sensor_i2c_reg_array actuator_driver_init6[] = {
		{0xF1, 0xFD00, 0x0, 0x0},
		{0xF2, 0x1878, 0x0, 0x0},
		{0xF3, 0x0400, 0x0, 0x0},
		{0xF5, 0xFCC0, 0x0, 0x0},
		{0xF8, 0x0000, 0x0, 0x0},
		{0xDE, 0x0AAA, 0x0, 0x0},
		{0xDF, 0x6000, 0x0, 0x0},
		{0xBE, 0x5800, 0x0, 0x0},
		{0xCE, 0x8500, 0x0, 0x0},
		{0xB0, 0x1999, 0x0, 0x0},
		{0xB1, 0x7FFF, 0x0, 0x0},
		{0xB2, 0x7FFF, 0x0, 0x0},
		{0xB3, 0x8400, 0x0, 0x0},
		{0xD0, 0x0298, 0x0, 0x0},
		{0xD1, 0x63C7, 0x0, 0x0},
		{0xD2, 0xAE14, 0x0, 0x0},
		{0xD3, 0x7FFF, 0x0, 0x0},
		{0xD4, 0x7FFF, 0x0, 0x0},
		{0xD9, 0x0404, 0x0, 0x0},
};

static int cam_actuator_eeprom_drive_write(
						struct cam_sensor_i2c_reg_array array,
						struct cam_actuator_ctrl_t *a_ctrl,
						uint8_t i2c_type)
{
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	write_setting.size = 1;
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = i2c_type;
	write_setting.delay = 0;
	write_setting.reg_setting = &array;

	rc = camera_io_dev_write(&(a_ctrl->io_master_info),
		&write_setting);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying actuator wrt init");
		return rc;
	}

	return rc;
}

static int cam_actuator_write_power_off_cmd(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_array cam_vcm_reset_info;
	cam_vcm_reset_info.reg_addr = 0x07;
	cam_vcm_reset_info.reg_data = 0x0000;
	cam_vcm_reset_info.delay = 0;
	cam_vcm_reset_info.data_mask = 0;

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	rc = cam_actuator_eeprom_drive_write(cam_vcm_reset_info, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "eeprom driver write failed:%d", rc);
	}
	return rc;
}

static int cam_actuator_eeprom_data_write(
						uint8_t *mapdata,
						struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t i = 0, j = 0;
	int32_t rc = 0;
	uint8_t id_vcm[2] = {0};
	uint16_t checksum;
	uint16_t actuator_data[30] = {0};
	struct cam_sensor_i2c_reg_array cam_temp;
	uint8_t cnt = 0;

	if (!mapdata || !a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	for (j = 0; j < EEPROM_MAP_DATA_CNT; j += 2) {
		actuator_data[i] = mapdata[j]<<8 | mapdata[j+1];
		i++;
	}


	rc = cam_actuator_eeprom_drive_write(actuator_driver_init0_0, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying i2c wrt reset ic NVL data");
		return rc;
	}

	rc = cam_actuator_eeprom_drive_write(actuator_driver_init0_1, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying i2c wrt reset ic NVL data");
		return rc;
	}

	msleep(1);

	//2. read checksum data
	rc = camera_io_dev_read_seq(&(a_ctrl->io_master_info), 0xF7, id_vcm, CAMERA_SENSOR_I2C_TYPE_BYTE, 2);
	CAM_ERR(CAM_ACTUATOR, "check sum 0xF7 value [0x%x] id_vcm[0] = 0x%x, id_vcm[1] = 0x%x", (id_vcm[0] << 8) | id_vcm[1], id_vcm[0], id_vcm[1]);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "check sum value error");
	}

	checksum = (id_vcm[0] << 8) | id_vcm[1];


	if (checksum != 0x0004) {
		CAM_ERR(CAM_ACTUATOR, "start ----- Applying i2c wrt settings");

		rc = cam_actuator_eeprom_drive_write(actuator_driver_init1, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in Applying i2c wrt init to actuator settings");
			return rc;
		}

		cam_temp.delay = 0;
		cam_temp.data_mask = 0;

		for (cnt = 0x0000; cnt <= 0x000E; cnt++) {
			if (cnt == 0x000E) {
				rc = cam_actuator_eeprom_drive_write(actuator_driver_init3, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}
				cam_temp.reg_addr = 0x51;
				cam_temp.reg_data = actuator_data[cnt];
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}
			} else {
				cam_temp.reg_addr = 0x50;
				cam_temp.reg_data = cnt;
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}

				cam_temp.reg_addr = 0x51;
				cam_temp.reg_data = actuator_data[cnt];
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}
			}
		}

		for (i = 0x0021; i <= 0x002F; i++) {
			if (i == 0x002F) {
				rc = cam_actuator_eeprom_drive_write(actuator_driver_init4, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}

				cam_temp.reg_addr = 0x51;
				cam_temp.reg_data = actuator_data[cnt++];
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}

			} else {
				cam_temp.reg_addr = 0x50;
				cam_temp.reg_data = i;
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}

				cam_temp.reg_addr = 0x51;
				cam_temp.reg_data = actuator_data[cnt++];
				rc = cam_actuator_eeprom_drive_write(cam_temp, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
						"Failed in Applying i2c wrt init to actuator settings");
					return rc;
				}

			}
		}

		rc = cam_actuator_eeprom_drive_write(actuator_driver_init0_0, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in Applying i2c wrt reset data to actuator settings");
			return rc;
		}

		rc = cam_actuator_eeprom_drive_write(actuator_driver_init0_1, a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in Applying i2c wrt reset data to actuator settings");
			return rc;
		}
		msleep(1);
	}
	//4. write init data
	for (i = 0; i < sizeof(actuator_driver_init6)/sizeof(struct cam_sensor_i2c_reg_array); i++) {
		rc = cam_actuator_eeprom_drive_write(actuator_driver_init6[i], a_ctrl, CAMERA_SENSOR_I2C_TYPE_WORD);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in Applying i2c wrt init data to actuator settings");
			return rc;
		}
	}
	return rc;
}
#endif

#ifdef CONFIG_USE_BU64748
struct cam_sensor_i2c_reg_array actuator_init0_array[] = {
	{0x82ef, 0x0000, 0x00, 0x0},
	{0x82ef, 0x8000, 0x00, 0x0},
};

static int cam_actuator_fw_init0(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	write_setting.size = sizeof(actuator_init0_array)/sizeof(struct cam_sensor_i2c_reg_array);
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.delay = 0;
	write_setting.reg_setting = actuator_init0_array;

	rc = camera_io_dev_write(&(a_ctrl->io_master_info),
		&write_setting);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying i2c wrt settings");
		return rc;
	}

	return rc;
}

struct cam_sensor_i2c_reg_array actuator_init1_array[] = {
	{0x8c, 0x01, 0x0, 0x0},
};

static int cam_actuator_fw_init1(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	write_setting.size =  sizeof(actuator_init1_array)/sizeof(struct cam_sensor_i2c_reg_array);
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 0;
	write_setting.reg_setting = actuator_init1_array;

	rc = camera_io_dev_write(&(a_ctrl->io_master_info),
		&write_setting);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying i2c wrt settings");
		return rc;
	}

	return rc;
}

struct cam_sensor_i2c_reg_array actuator_init2_array[] = {
	{0x8430, 0x0d00, 0x0, 0x0},
};

static int cam_actuator_fw_init2(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	write_setting.size =  sizeof(actuator_init2_array)/sizeof(struct cam_sensor_i2c_reg_array);
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.delay = 0;
	write_setting.reg_setting = actuator_init2_array;

	rc = camera_io_dev_write(&(a_ctrl->io_master_info),
		&write_setting);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed in Applying i2c wrt settings");
		return rc;
	}

	return rc;
}

static int cam_actuator_fw_download(struct cam_actuator_ctrl_t *a_ctrl)
{
	uint16_t total_bytes = 0;
	uint8_t *ptr = NULL;
	int32_t rc = 0;
	int32_t cnt = 0;
	int32_t i = 0;
	uint32_t fw_size;
	const struct firmware *fw = NULL;
	const char *fw_name_prog = NULL;
	struct device *dev = NULL;
	struct cam_sensor_i2c_reg_setting write_setting;
	struct page *page = NULL;
	uint8_t id[2] = {0};

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	rc = cam_actuator_fw_init0(a_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "init0 failed exit");
		return rc;
	}

	rc = camera_io_dev_read_seq(&(a_ctrl->io_master_info), 0x825f, id, CAMERA_SENSOR_I2C_TYPE_WORD, 2);

	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "check version 0x825f value [0x%x] error", (id[0] << 8) | id[1]);
	}

/* Load FW */
	fw_name_prog = FIRMWARE_NAME;
	dev = &(a_ctrl->pdev->dev);
	if (!dev) {
		CAM_ERR(CAM_ACTUATOR, "Invalid dev is null");
		return -EINVAL;
	}

	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Failed to locate %s", fw_name_prog);
		return rc;
	}

	total_bytes = fw->size;
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.size = total_bytes;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
			total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area(a_ctrl->soc_info.dev),
		fw_size, 0);
	if (!page) {
		CAM_ERR(CAM_ACTUATOR, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	write_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)(
			page_address(page));


	write_setting.delay = 0;
	for (i = 0, ptr = (uint8_t *)fw->data; i < total_bytes;) {
		for (cnt = 0; cnt < ACTUATOR_TRANS_SIZE && i < total_bytes;
			cnt++, ptr++, i++) {
			write_setting.reg_setting[cnt].reg_addr = 0x80;
			write_setting.reg_setting[cnt].reg_data = *ptr;
			write_setting.reg_setting[cnt].delay = 0;
			write_setting.reg_setting[cnt].data_mask = 0;
		}
		write_setting.size = cnt;

		rc = camera_io_dev_write_continuous(&(a_ctrl->io_master_info),
			&write_setting, 1);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "FW download failed %d", rc);
	}
	CAM_ERR(CAM_ACTUATOR, "ACTUATOR FW download over..., i = %d", i);

	cma_release(dev_get_cma_area(a_ctrl->soc_info.dev),
		page, fw_size);
	page = NULL;
	fw_size = 0;
	release_firmware(fw);

	cam_actuator_fw_init1(a_ctrl);
	cam_actuator_fw_init2(a_ctrl);

	return rc;
}
#endif

static int32_t cam_actuator_power_up(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	struct cam_hw_soc_info  *soc_info =
		&a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	if ((power_info->power_setting == NULL) &&
		(power_info->power_down_setting == NULL)) {
		CAM_INFO(CAM_ACTUATOR,
			"Using default power settings");
		rc = cam_actuator_construct_default_power_setting(power_info);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Construct default actuator power setting failed.");
			return rc;
		}
	}

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params for power up rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&a_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed to fill vreg params power down rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR,
			"failed in actuator power up rc %d", rc);
		return rc;
	}

	/* VREG needs some delay to power up */
	usleep_range(10000, 10050);

	rc = camera_io_init(&a_ctrl->io_master_info);
	if (rc < 0)
		CAM_ERR(CAM_ACTUATOR, "cci init failed: rc: %d", rc);
	CAM_ERR(CAM_ACTUATOR, " cam sensor io init");

	return rc;
}

static int32_t cam_actuator_power_down(struct cam_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	struct cam_actuator_soc_private  *soc_private;

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "failed: a_ctrl %pK", a_ctrl);
		return -EINVAL;
	}

#ifdef CONFIG_USE_ROHM_BU64753

	if (a_ctrl->io_master_info.cci_client->sid == ROHM_ACTUATOR_II2_ADDR)
		rc = cam_actuator_write_power_off_cmd(a_ctrl);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "eeprom driver write failed:%d", rc);
	}
#endif

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &a_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_ACTUATOR, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = msm_camera_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_ACTUATOR, "power down the core is failed:%d", rc);
		return rc;
	}

	camera_io_release(&a_ctrl->io_master_info);

	return rc;
}

static int32_t cam_actuator_i2c_modes_util(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_RANDOM) {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_SEQ) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			0);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
			}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_WRITE_BURST) {
		rc = camera_io_dev_write_continuous(
			io_master_info,
			&(i2c_list->i2c_settings),
			1);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	} else if (i2c_list->op_code == CAM_SENSOR_I2C_POLL) {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.
				reg_setting[i].reg_addr,
			i2c_list->i2c_settings.
				reg_setting[i].reg_data,
			i2c_list->i2c_settings.
				reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
				i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.
				reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t cam_actuator_slaveInfo_pkt_parser(struct cam_actuator_ctrl_t *a_ctrl,
	uint32_t *cmd_buf)
{
	int32_t rc = 0;
	struct cam_cmd_i2c_info *i2c_info;

	if (!a_ctrl || !cmd_buf) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	if (a_ctrl->io_master_info.master_type == CCI_MASTER) {
		a_ctrl->io_master_info.cci_client->cci_i2c_master =
			a_ctrl->cci_i2c_master;
		a_ctrl->io_master_info.cci_client->i2c_freq_mode =
			i2c_info->i2c_freq_mode;
		a_ctrl->io_master_info.cci_client->sid =
			i2c_info->slave_addr >> 1;
#ifdef CONFIG_USE_BU64748
		a_ctrl->io_master_info.cci_client->retries = 3;
		a_ctrl->io_master_info.cci_client->id_map = 0;
#endif
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x Freq Mode: %d",
			i2c_info->slave_addr, i2c_info->i2c_freq_mode);
	} else if (a_ctrl->io_master_info.master_type == I2C_MASTER) {
		a_ctrl->io_master_info.client->addr = i2c_info->slave_addr;
		CAM_DBG(CAM_ACTUATOR, "Slave addr: 0x%x", i2c_info->slave_addr);
	} else {
		CAM_ERR(CAM_ACTUATOR, "Invalid Master type: %d",
			a_ctrl->io_master_info.master_type);
		 rc = -EINVAL;
	}

	return rc;
}

int32_t cam_actuator_apply_settings(struct cam_actuator_ctrl_t *a_ctrl,
	struct i2c_settings_array *i2c_set)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0;

	if (a_ctrl == NULL || i2c_set == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	if (i2c_set->is_settings_valid != 1) {
		CAM_ERR(CAM_ACTUATOR, " Invalid settings");
		return -EINVAL;
	}

	list_for_each_entry(i2c_list,
		&(i2c_set->list_head), list) {
		rc = cam_actuator_i2c_modes_util(
			&(a_ctrl->io_master_info),
			i2c_list);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to apply settings: %d",
				rc);
			return rc;
		}
	}

	return rc;
}

int32_t cam_actuator_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t rc = 0, request_id, del_req_id;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!apply) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Input Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(apply->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	request_id = apply->request_id % MAX_PER_FRAME_ARRAY;

	trace_cam_apply_req("Actuator", apply->request_id);

	CAM_DBG(CAM_ACTUATOR, "Request Id: %lld", apply->request_id);

	if ((apply->request_id ==
		a_ctrl->i2c_data.per_frame[request_id].request_id) &&
		(a_ctrl->i2c_data.per_frame[request_id].is_settings_valid)
		== 1) {
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.per_frame[request_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in applying the request: %lld\n",
				apply->request_id);
			return rc;
		}
	}
	del_req_id = (request_id +
		MAX_PER_FRAME_ARRAY - MAX_SYSTEM_PIPELINE_DELAY) %
		MAX_PER_FRAME_ARRAY;

	if (apply->request_id >
		a_ctrl->i2c_data.per_frame[del_req_id].request_id) {
		a_ctrl->i2c_data.per_frame[del_req_id].request_id = 0;
		rc = delete_request(&a_ctrl->i2c_data.per_frame[del_req_id]);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Fail deleting the req: %d err: %d\n",
				del_req_id, rc);
			return rc;
		}
	} else {
		CAM_DBG(CAM_ACTUATOR, "No Valid Req to clean Up");
	}

	return rc;
}

int32_t cam_actuator_establish_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_actuator_ctrl_t *a_ctrl = NULL;

	if (!link) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}
	if (link->link_enable) {
		a_ctrl->bridge_intf.link_hdl = link->link_hdl;
		a_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.crm_cb = NULL;
	}

	return 0;
}

int32_t cam_actuator_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	if (!info) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_ACTUATOR;
	strlcpy(info->name, CAM_ACTUATOR_NAME, sizeof(info->name));
	info->p_delay = 0;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return 0;
}

int32_t cam_actuator_i2c_pkt_parse(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int32_t  rc = 0;
	int32_t  i = 0;
	uint32_t total_cmd_buf_in_bytes = 0;
	size_t   len_of_buff = 0;
	uint32_t *offset = NULL;
	uint32_t *cmd_buf = NULL;
	uint64_t generic_ptr;
	struct common_header      *cmm_hdr = NULL;
	struct cam_control        *ioctl_ctrl = NULL;
	struct cam_packet         *csl_packet = NULL;
	struct cam_config_dev_cmd config;
	struct i2c_data_settings  *i2c_data = NULL;
	struct i2c_settings_array *i2c_reg_settings = NULL;
	struct cam_cmd_buf_desc   *cmd_desc = NULL;
	struct cam_req_mgr_add_request  add_req;
	struct cam_actuator_soc_private *soc_private = NULL;
	struct cam_sensor_power_ctrl_t  *power_info = NULL;

#ifdef CONFIG_USE_ROHM_BU64753
	struct cam_sensor_cci_client *cci_client = NULL;
	struct camera_io_master *io_master_info = NULL;
#endif

	if (!a_ctrl || !arg) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;

	power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&config, (void __user *) ioctl_ctrl->handle,
		sizeof(config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(config.packet_handle,
		(uint64_t *)&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Error in converting command Handle %d",
			rc);
		return rc;
	}

	if (config.offset > len_of_buff) {
		CAM_ERR(CAM_ACTUATOR,
			"offset is out of bounds: offset: %lld len: %zu",
			config.offset, len_of_buff);
		return -EINVAL;
	}

	csl_packet = (struct cam_packet *)(generic_ptr + config.offset);
	CAM_DBG(CAM_ACTUATOR, "Pkt opcode: %d", csl_packet->header.op_code);

	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_ACTUATOR_PACKET_OPCODE_INIT:
		offset = (uint32_t *)&csl_packet->payload;
		offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);

		/* Loop through multiple command buffers */
		for (i = 0; i < csl_packet->num_cmd_buf; i++) {
			total_cmd_buf_in_bytes = cmd_desc[i].length;
			if (!total_cmd_buf_in_bytes)
				continue;
			rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
					(uint64_t *)&generic_ptr, &len_of_buff);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Failed to get cpu buf");
				return rc;
			}
			cmd_buf = (uint32_t *)generic_ptr;
			if (!cmd_buf) {
				CAM_ERR(CAM_ACTUATOR, "invalid cmd buf");
				return -EINVAL;
			}
			cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
			cmm_hdr = (struct common_header *)cmd_buf;

			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				CAM_DBG(CAM_ACTUATOR,
					"Received slave info buffer");
				rc = cam_actuator_slaveInfo_pkt_parser(
					a_ctrl, cmd_buf);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed to parse slave info: %d", rc);
					return rc;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				CAM_DBG(CAM_ACTUATOR,
					"Received power settings buffer");
				rc = cam_sensor_update_power_settings(
					cmd_buf,
					total_cmd_buf_in_bytes,
					power_info);
				if (rc) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse power settings: %d",
					rc);
					return rc;
				}
				break;
			default:
				CAM_DBG(CAM_ACTUATOR,
					"Received initSettings buffer");
				i2c_data = &(a_ctrl->i2c_data);
				i2c_reg_settings =
					&i2c_data->init_settings;

				i2c_reg_settings->request_id = 0;
				i2c_reg_settings->is_settings_valid = 1;
				rc = cam_sensor_i2c_command_parser(
					i2c_reg_settings,
					&cmd_desc[i], 1);
				if (rc < 0) {
					CAM_ERR(CAM_ACTUATOR,
					"Failed:parse init settings: %d",
					rc);
					return rc;
				}
				break;
			}
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_ACQUIRE) {
			rc = cam_actuator_power_up(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					" Actuator Power up failed");
				return rc;
			}
			a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;

#ifdef CONFIG_USE_ROHM_BU64753
		io_master_info = &(a_ctrl->io_master_info);
		cci_client = io_master_info->cci_client;

		if ((cci_client != NULL) && ((cci_client->sid) == ROHM_ACTUATOR_II2_ADDR)) {
			rc = cam_actuator_eeprom_data_write(g_eeprom_mapdata, a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Write Init Driver Data To Actuator failed");
			}
		}
#endif

#ifdef CONFIG_USE_BU64748

			if (a_ctrl->io_master_info.cci_client->sid == 0xEC/2)
			{
				rc = cam_actuator_fw_download(a_ctrl);

				if (rc) {
					CAM_ERR(CAM_ACTUATOR, "Failed ACTUATOR FW Download");
					return rc;
				}
			}
#endif
		}

#ifdef CONFIG_USE_BU64748

		CAM_ERR(CAM_ACTUATOR, "before init setting dac sid num %x ", a_ctrl->io_master_info.cci_client->sid);
		if (a_ctrl->io_master_info.cci_client->sid == 0x18/2)
		{
			rc = cam_actuator_apply_settings(a_ctrl,
					&a_ctrl->i2c_data.init_settings);
			CAM_ERR(CAM_ACTUATOR, "init setting dac");
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR, "Cannot apply Init settings");
				return rc;
			}
		}
#endif

#ifndef CONFIG_USE_BU64748
		rc = cam_actuator_apply_settings(a_ctrl,
			&a_ctrl->i2c_data.init_settings);
		CAM_ERR(CAM_ACTUATOR, "init setting dac");
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Cannot apply Init settings");
			return rc;
		}
#endif

		/* Delete the request even if the apply is failed */
		rc = delete_request(&a_ctrl->i2c_data.init_settings);
		if (rc < 0) {
			CAM_WARN(CAM_ACTUATOR,
				"Fail in deleting the Init settings");
			rc = 0;
		}
		break;
	case CAM_ACTUATOR_PACKET_AUTO_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			return rc;
		}
		a_ctrl->setting_apply_state = ACT_APPLY_SETTINGS_NOW;

		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->init_settings;

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Auto move lens parsing failed: %d", rc);
			return rc;
		}
		break;
	case CAM_ACTUATOR_PACKET_MANUAL_MOVE_LENS:
		if (a_ctrl->cam_act_state < CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Not in right state to move lens: %d",
				a_ctrl->cam_act_state);
			return rc;
		}
		i2c_data = &(a_ctrl->i2c_data);
		i2c_reg_settings = &i2c_data->per_frame[
			csl_packet->header.request_id % MAX_PER_FRAME_ARRAY];

		i2c_data->init_settings.request_id =
			csl_packet->header.request_id;
		i2c_reg_settings->is_settings_valid = 1;
		offset = (uint32_t *)&csl_packet->payload;
		offset += csl_packet->cmd_buf_offset / sizeof(uint32_t);
		cmd_desc = (struct cam_cmd_buf_desc *)(offset);
		rc = cam_sensor_i2c_command_parser(i2c_reg_settings,
			cmd_desc, 1);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR,
				"Manual move lens parsing failed: %d", rc);
			return rc;
		}
		break;
	}

	if ((csl_packet->header.op_code & 0xFFFFFF) !=
		CAM_ACTUATOR_PACKET_OPCODE_INIT) {
		add_req.link_hdl = a_ctrl->bridge_intf.link_hdl;
		add_req.req_id = csl_packet->header.request_id;
		add_req.dev_hdl = a_ctrl->bridge_intf.device_hdl;
		add_req.skip_before_applying = 0;
		if (a_ctrl->bridge_intf.crm_cb &&
			a_ctrl->bridge_intf.crm_cb->add_req)
			a_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		CAM_DBG(CAM_ACTUATOR, "Req Id: %lld added to Bridge",
			add_req.req_id);
	}

	return rc;
}

void cam_actuator_shutdown(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc;

	if (a_ctrl->cam_act_state == CAM_ACTUATOR_INIT)
		return;

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG) {
		rc = cam_actuator_power_down(a_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "Actuator Power down failed");
	}

	if (a_ctrl->cam_act_state >= CAM_ACTUATOR_ACQUIRE) {
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying  dhdl failed");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
	}
	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
}

int32_t cam_actuator_driver_cmd(struct cam_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;

	if (!a_ctrl || !cmd) {
		CAM_ERR(CAM_ACTUATOR, " Invalid Args");
		return -EINVAL;
	}

	CAM_DBG(CAM_ACTUATOR, "Opcode to Actuator: %d", cmd->op_code);

	mutex_lock(&(a_ctrl->actuator_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev actuator_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (a_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_ACTUATOR, "Device is already acquired");
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = copy_from_user(&actuator_acq_dev,
			(void __user *) cmd->handle,
			sizeof(actuator_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copying from user\n");
			goto release_mutex;
		}

		bridge_params.session_hdl = actuator_acq_dev.session_handle;
		bridge_params.ops = &a_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = a_ctrl;

		actuator_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		a_ctrl->bridge_intf.device_hdl = actuator_acq_dev.device_handle;
		a_ctrl->bridge_intf.session_hdl =
			actuator_acq_dev.session_handle;

		CAM_DBG(CAM_ACTUATOR, "Device Handle: %d",
			actuator_acq_dev.device_handle);
		if (copy_to_user((void __user *) cmd->handle, &actuator_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}

		a_ctrl->cam_act_state = CAM_ACTUATOR_ACQUIRE;
	}
		break;
	case CAM_RELEASE_DEV: {
		if (a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
				"Cant release actuator: in start state");
			goto release_mutex;
		}

		if (a_ctrl->cam_act_state == CAM_ACTUATOR_CONFIG) {
			rc = cam_actuator_power_down(a_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Actuator Power down failed");
				goto release_mutex;
			}
		}

		if (a_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_ACTUATOR, "link hdl: %d device hdl: %d",
				a_ctrl->bridge_intf.device_hdl,
				a_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(a_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_ACTUATOR, "destroying the device hdl");
		a_ctrl->bridge_intf.device_hdl = -1;
		a_ctrl->bridge_intf.link_hdl = -1;
		a_ctrl->bridge_intf.session_hdl = -1;
		a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_actuator_query_cap actuator_cap = {0};

		actuator_cap.slot_info = a_ctrl->soc_info.index;
		if (copy_to_user((void __user *) cmd->handle, &actuator_cap,
			sizeof(struct cam_actuator_query_cap))) {
			CAM_ERR(CAM_ACTUATOR, "Failed Copy to User");
			rc = -EFAULT;
			goto release_mutex;
		}
	}
		break;
	case CAM_START_DEV: {
		if (a_ctrl->cam_act_state != CAM_ACTUATOR_CONFIG) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to start : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}
		a_ctrl->cam_act_state = CAM_ACTUATOR_START;
	}
		break;
	case CAM_STOP_DEV: {
		struct i2c_settings_array *i2c_set = NULL;
		int i;

		if (a_ctrl->cam_act_state != CAM_ACTUATOR_START) {
			rc = -EINVAL;
			CAM_WARN(CAM_ACTUATOR,
			"Not in right state to stop : %d",
			a_ctrl->cam_act_state);
			goto release_mutex;
		}

		for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
			i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

			if (i2c_set->is_settings_valid == 1) {
				rc = delete_request(i2c_set);
				if (rc < 0)
					CAM_ERR(CAM_SENSOR,
						"delete request: %lld rc: %d",
						i2c_set->request_id, rc);
			}
		}
		a_ctrl->cam_act_state = CAM_ACTUATOR_CONFIG;
	}
		break;
	case CAM_CONFIG_DEV: {
		a_ctrl->setting_apply_state =
			ACT_APPLY_SETTINGS_LATER;
		rc = cam_actuator_i2c_pkt_parse(a_ctrl, arg);
		if (rc < 0) {
			CAM_ERR(CAM_ACTUATOR, "Failed in actuator Parsing");
		}

		if (a_ctrl->setting_apply_state ==
			ACT_APPLY_SETTINGS_NOW) {
			rc = cam_actuator_apply_settings(a_ctrl,
				&a_ctrl->i2c_data.init_settings);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"Cannot apply Update settings");

			/* Delete the request even if the apply is failed */
			rc = delete_request(&a_ctrl->i2c_data.init_settings);
			if (rc < 0) {
				CAM_ERR(CAM_ACTUATOR,
					"Failed in Deleting the Init Pkt: %d",
					rc);
				goto release_mutex;
			}
		}
	}
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid Opcode %d", cmd->op_code);
	}

release_mutex:
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return rc;
}

int32_t cam_actuator_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t rc = 0, i;
	uint32_t cancel_req_id_found = 0;
	struct cam_actuator_ctrl_t *a_ctrl = NULL;
	struct i2c_settings_array *i2c_set = NULL;

	if (!flush_req)
		return -EINVAL;

	a_ctrl = (struct cam_actuator_ctrl_t *)
		cam_get_device_priv(flush_req->dev_hdl);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Device data is NULL");
		return -EINVAL;
	}

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++) {
		i2c_set = &(a_ctrl->i2c_data.per_frame[i]);

		if ((flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ)
				&& (i2c_set->request_id != flush_req->req_id))
			continue;

		if (i2c_set->is_settings_valid == 1) {
			rc = delete_request(i2c_set);
			if (rc < 0)
				CAM_ERR(CAM_ACTUATOR,
					"delete request: %lld rc: %d",
					i2c_set->request_id, rc);

			if (flush_req->type ==
				CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ) {
				cancel_req_id_found = 1;
				break;
			}
		}
	}

	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_CANCEL_REQ &&
		!cancel_req_id_found)
		CAM_DBG(CAM_ACTUATOR,
			"Flush request id:%lld not found in the pending list",
			flush_req->req_id);
	return rc;
}
