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
#include "Sem1215.h"

void I2C_Read_Data(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t length, uint8_t *data)
{
	int32_t rc = 0;
	int32_t i = 0;
	rc = camera_io_dev_read_seq(&o_ctrl->io_master_info,
		addr, data,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE,
		length);
	for (i = 0; i < length; i++) {
		CAM_DBG(CAM_OIS, "[OISDEBUG] read addr 0x%04x[%d] = 0x%02x", addr, i, data[i]);
	}
	if (rc) {
		CAM_ERR(CAM_EEPROM, "read failed rc %d",
			rc);
	}
}

void I2C_Write_Data(struct cam_ois_ctrl_t *o_ctrl,uint32_t addr, uint32_t length,uint8_t* data, uint32_t delay)
{
	static struct cam_sensor_i2c_reg_array w_data[256] = { {0} };
	struct cam_sensor_i2c_reg_setting write_setting;
	uint32_t i = 0;
	int32_t rc = 0;
	if (!data || !o_ctrl || (length < 1)) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return;
	}

	for (i = 0;i < length && i < 256; i++) {
		w_data[i].reg_addr = addr;
		w_data[i].reg_data = data[i];
		w_data[i].delay = 0;
		w_data[i].data_mask = 0;
	}
	write_setting.size = length;
	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = delay;
	write_setting.reg_setting = w_data;

	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&write_setting, 0);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS I2C_Write_Data write failed %d", rc);
	}
	for (i = 0; i < length && i < 256; i+=4) {
		CAM_DBG(CAM_OIS, "[OISDEBUG] write addr 0x%04x = 0x%02x 0x%02x 0x%02x 0x%02x", w_data[i].reg_addr, data[i], data[i+1], data[i+2], data[i+3]);
	}
}

uint8_t load_fw_buff(
	struct cam_ois_ctrl_t *o_ctrl,
	char* firmware_name,
	uint8_t *read_data,
	uint32_t read_length)
{

	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, i;
	const struct firmware             *fw = NULL;
	const char                        *fw_name = NULL;
	struct device                     *dev = &(o_ctrl->pdev->dev);

	fw_name = firmware_name;

	rc = request_firmware(&fw, fw_name, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name);
	} else {
		total_bytes = fw->size;
		ptr = (uint8_t *)fw->data;
		if (read_data) {
			for (i = 0; i < read_length; i++) {
				read_data[i] = *(ptr + i);
			}
		}
	}
	release_firmware(fw);
	return 0;
}
