#include "msm_sd.h"
#include "msm_eeprom.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include<linux/kernel.h>

static uint16_t ov13850_eeprom_sensor_readreg(
	struct msm_eeprom_ctrl_t *s_ctrl, uint32_t reg_addr)
{
	uint16_t reg_value = 0;
	s_ctrl->i2c_client.i2c_func_tbl->i2c_read(
				&(s_ctrl->i2c_client),
				reg_addr,
				&reg_value, MSM_CAMERA_I2C_BYTE_DATA);
	return reg_value ;
}

int ov13850_eeprom_sensor_writereg(
	struct msm_eeprom_ctrl_t *s_ctrl, uint32_t reg_addr, uint32_t reg_value, uint32_t delay)
{
	int rc = 0;
	rc = s_ctrl->i2c_client.i2c_func_tbl->i2c_write(
		&(s_ctrl->i2c_client), reg_addr, reg_value, MSM_CAMERA_I2C_BYTE_DATA);
	msleep(delay);
	return rc;
}

int eeprom_init_ov13850_reg_otp(struct msm_eeprom_ctrl_t *e_ctrl)
{
	int rc = 0, temp = 0;
	if (!e_ctrl) {
		pr_err("%s e_ctrl is NULL\n", __func__);
		return -EINVAL;
	}

	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	e_ctrl->i2c_client.cci_client->sid = 0x20 >> 1;


	rc = ov13850_eeprom_sensor_writereg(e_ctrl, 0x0100, 0x01, 1);


	temp = ov13850_eeprom_sensor_readreg(e_ctrl, 0x5002);
	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x5002, (temp&(~0x02)), 0);
	printk("%s %d E temp=0x%x\n", __func__, __LINE__, temp);


	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x3d88, 0x72, 0);
	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x3d89, 0x20, 0);
	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x3d8a, 0x73, 0);
	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x3d8b, 0xb9, 0);
	rc += ov13850_eeprom_sensor_writereg(e_ctrl, 0x3d81, 0x01, 10);

	if (rc < 0) {
		pr_err("i2c write faild\n");
		return rc;
	}

	return rc;
}

