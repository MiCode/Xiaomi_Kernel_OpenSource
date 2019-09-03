/*
* @section LICENSE
* (C) Copyright 2011~2018 Bosch Sensortec GmbH All Rights Reserved
*
* This software program is licensed subject to the GNU General
* Public License (GPL).Version 2,June 1991,
* available at http://www.fsf.org/copyleft/gpl.html

*
* @filename bmi160.c
* @Date: 2015/04/02
* @id       836294d
* @Revision: 2.0.9 $
*
* Usage: Sensor Driver for BMI160 sensor
*
****************************************************************************
* \section Disclaimer
*
* Common:
* Bosch Sensortec products are developed for the consumer goods industry.
* They may only be used within the parameters of the respective valid
* product data sheet.  Bosch Sensortec products are provided with the
* express understanding that there is no warranty of fitness for a
* particular purpose.They are not fit for use in life-sustaining,
* safety or security sensitive systems or any system or device
* that may lead to bodily harm or property damage if the system
* or device malfunctions. In addition,Bosch Sensortec products are
* not fit for use in products which interact with motor vehicle systems.
* The resale and or use of products are at the purchasers own risk and
* his own responsibility. The examination of fitness for the intended use
* is the sole responsibility of the Purchaser.
*
* The purchaser shall indemnify Bosch Sensortec from all third party
* claims, including any claims for incidental, or consequential damages,
* arising from any product use not covered by the parameters of
* the respective valid product data sheet or not approved by
* Bosch Sensortec and reimburse Bosch Sensortec for all costs in
* connection with such claims.
*
* The purchaser must monitor the market for the purchased products,
* particularly with regard to product safety and inform Bosch Sensortec
* without delay of all security relevant incidents.
*
* Engineering Samples are marked with an asterisk (*) or (e).
* Samples may vary from the valid technical specifications of the product
* series. They are therefore not intended or fit for resale to third
* parties or for use in end products. Their sole purpose is internal
* client testing. The testing of an engineering sample may in no way
* replace the testing of a product series. Bosch Sensortec assumes
* no liability for the use of engineering samples.
* By accepting the engineering samples, the Purchaser agrees to indemnify
* Bosch Sensortec from all claims arising from the use of engineering
* samples.
*
* Special:
* This software module (hereinafter called "Software") and any information
* on application-sheets (hereinafter called "Information") is provided
* free of charge for the sole purpose to support your application work.
* The Software and Information is subject to the following
* terms and conditions:
*
* The Software is specifically designed for the exclusive use for
* Bosch Sensortec products by personnel who have special experience
* and training. Do not use this Software if you do not have the
* proper experience or training.
*
* This Software package is provided `` as is `` and without any expressed
* or implied warranties,including without limitation, the implied warranties
* of merchantability and fitness for a particular purpose.
*
* Bosch Sensortec and their representatives and agents deny any liability
* for the functional impairment
* of this Software in terms of fitness, performance and safety.
* Bosch Sensortec and their representatives and agents shall not be liable
* for any direct or indirect damages or injury, except as
* otherwise stipulated in mandatory applicable law.
*
* The Information provided is believed to be accurate and reliable.
* Bosch Sensortec assumes no responsibility for the consequences of use
* of such Information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of Bosch. Specifications mentioned in the Information are
* subject to change without notice.
**************************************************************************/
/*! file <BMI160 >
    brief <Sensor driver for BMI160> */
#include "bmi160.h"
#include <linux/kernel.h>

/* user defined code to be added here ... */
struct bmi160_t *p_bmi160;
/* used for reading the mag trim values for compensation*/
struct trim_data_t mag_trim;
/* the following variable used for avoiding the selecting of auto mode
when it is running in the manual mode of BMM150 mag interface*/
u8 V_bmm150_maual_auto_condition_u8 = BMI160_INIT_VALUE;
/* used for reading the AKM compensating data */
struct bst_akm_sensitivity_data_t akm_asa_data;
/* Assign the fifo time */
u32 V_fifo_time_U32 = BMI160_INIT_VALUE;

/* FIFO data read for 1024 bytes of data */
u8 v_fifo_data_u8[FIFO_FRAME] = {BMI160_INIT_VALUE};
/* YAMAHA-YAS532*/
/* value of coeff*/
static const int yas532_version_ac_coef[] = {YAS532_VERSION_AC_COEF_X,
YAS532_VERSION_AC_COEF_Y1, YAS532_VERSION_AC_COEF_Y2};
/* used for reading the yas532 calibration data*/
struct yas532_t yas532_data;
/* used for reading the yas537 calibration data*/
struct yas537_t yas537_data;
/*!
 *	@brief
 *	This function is used for initialize
 *	bus read and bus write functions
 *	assign the chip id and device address
 *	chip id is read in the register 0x00 bit from 0 to 7
 *
 *	@param bmi160 : structure pointer
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *	@note
 *	While changing the parameter of the bmi160_t
 *	consider the following point:
 *	Changing the reference value of the parameter
 *	will changes the local copy or local reference
 *	make sure your changes will not
 *	affect the reference value of the parameter
 *	(Better case don't change the reference value of the parameter)
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_init(struct bmi160_t *bmi160)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_pmu_data_u8 = BMI160_INIT_VALUE;
	/* assign bmi160 ptr */
	p_bmi160 = bmi160;
	com_rslt =
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_CHIP_ID__REG,
	&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* read Chip Id */
	p_bmi160->chip_id = v_data_u8;
	/* To avoid gyro wakeup it is required to write 0x00 to 0x6C*/
	com_rslt += bmi160_write_reg(BMI160_USER_PMU_TRIGGER_ADDR,
	&v_pmu_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	return com_rslt;
}
/*!
 * @brief
 *	This API write the data to
 *	the given register
 *
 *
 *	@param v_addr_u8 -> Address of the register
 *	@param v_data_u8 -> The data from the register
 *	@param v_len_u8 -> no of bytes to read
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_write_reg(u8 v_addr_u8,
u8 *v_data_u8, u8 v_len_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write data from register*/
			com_rslt =
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			v_addr_u8, v_data_u8, v_len_u8);
		}
	return com_rslt;
}
/*!
 * @brief
 *	This API reads the data from
 *	the given register
 *
 *
 *	@param v_addr_u8 -> Address of the register
 *	@param v_data_u8 -> The data from the register
 *	@param v_len_u8 -> no of bytes to read
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_read_reg(u8 v_addr_u8,
u8 *v_data_u8, u8 v_len_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* Read data from register*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			v_addr_u8, v_data_u8, v_len_u8);
		}
	return com_rslt;
}
/*!
 *	@brief This API used to reads the fatal error
 *	from the Register 0x02 bit 0
 *	This flag will be reset only by power-on-reset and soft reset
 *
 *
 *  @param v_fatal_err_u8 : The status of fatal error
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fatal_err(u8
*v_fatal_err_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* reading the fatal error status*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FATAL_ERR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fatal_err_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FATAL_ERR);
		}
	return com_rslt;
}
/*!
 *	@brief This API used to read the error code
 *	from register 0x02 bit 1 to 4
 *
 *
 *  @param v_err_code_u8 : The status of error codes
 *	error_code  |    description
 *  ------------|---------------
 *	0x00        |no error
 *	0x01        |ACC_CONF error (accel ODR and bandwidth not compatible)
 *	0x02        |GYR_CONF error (Gyroscope ODR and bandwidth not compatible)
 *	0x03        |Under sampling mode and interrupt uses pre filtered data
 *	0x04        |reserved
 *	0x05        |Selected trigger-readout offset in
 *    -         |MAG_IF greater than selected ODR
 *	0x06        |FIFO configuration error for header less mode
 *	0x07        |Under sampling mode and pre filtered data as FIFO source
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_err_code(u8
*v_err_code_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ERR_CODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_err_code_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ERR_CODE);
		}
	return com_rslt;
}
/*!
 *	@brief This API Reads the i2c error code from the
 *	Register 0x02 bit 5.
 *	This error occurred in I2C master detected
 *
 *  @param v_i2c_err_code_u8 : The status of i2c fail error
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_fail_err(u8
*v_i2c_err_code_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_FAIL_ERR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_i2c_err_code_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_I2C_FAIL_ERR);
		}
	return com_rslt;
}
 /*!
 *	@brief This API Reads the dropped command error
 *	from the register 0x02 bit 6
 *
 *
 *  @param v_drop_cmd_err_u8 : The status of drop command error
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_drop_cmd_err(u8
*v_drop_cmd_err_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DROP_CMD_ERR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_drop_cmd_err_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_DROP_CMD_ERR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the magnetometer data ready
 *	interrupt not active.
 *	It reads from the error register 0x0x2 bit 7
 *
 *
 *
 *
 *  @param v_mag_data_rdy_err_u8 : The status of mag data ready interrupt
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_dada_rdy_err(
u8 *v_mag_data_rdy_err_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_MAG_DADA_RDY_ERR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_data_rdy_err_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_DADA_RDY_ERR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the error status
 *	from the error register 0x02 bit 0 to 7
 *
 *  @param v_mag_data_rdy_err_u8 : The status of mag data ready interrupt
 *  @param v_fatal_er_u8r : The status of fatal error
 *  @param v_err_code_u8 : The status of error code
 *  @param v_i2c_fail_err_u8 : The status of I2C fail error
 *  @param v_drop_cmd_err_u8 : The status of drop command error
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_error_status(u8 *v_fatal_er_u8r,
u8 *v_err_code_u8, u8 *v_i2c_fail_err_u8,
u8 *v_drop_cmd_err_u8, u8 *v_mag_data_rdy_err_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the error codes*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ERR_STAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			/* fatal error*/
			*v_fatal_er_u8r =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FATAL_ERR);
			/* user error*/
			*v_err_code_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ERR_CODE);
			/* i2c fail error*/
			*v_i2c_fail_err_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_I2C_FAIL_ERR);
			/* drop command error*/
			*v_drop_cmd_err_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_DROP_CMD_ERR);
			/* mag data ready error*/
			*v_mag_data_rdy_err_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_DADA_RDY_ERR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the magnetometer power mode from
 *	PMU status register 0x03 bit 0 and 1
 *
 *  @param v_mag_power_mode_stat_u8 : The value of mag power mode
 *	mag_powermode    |   value
 * ------------------|----------
 *    SUSPEND        |   0x00
 *    NORMAL         |   0x01
 *   LOW POWER       |   0x02
 *
 *
 * @note The power mode of mag set by the 0x7E command register
 * @note using the function "bmi160_set_command_register()"
 *  value    |   mode
 *  ---------|----------------
 *   0x18    | MAG_MODE_SUSPEND
 *   0x19    | MAG_MODE_NORMAL
 *   0x1A    | MAG_MODE_LOWPOWER
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_power_mode_stat(u8
*v_mag_power_mode_stat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_POWER_MODE_STAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_power_mode_stat_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_POWER_MODE_STAT);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the gyroscope power mode from
 *	PMU status register 0x03 bit 2 and 3
 *
 *  @param v_gyro_power_mode_stat_u8 :	The value of gyro power mode
 *	gyro_powermode   |   value
 * ------------------|----------
 *    SUSPEND        |   0x00
 *    NORMAL         |   0x01
 *   FAST POWER UP   |   0x03
 *
 * @note The power mode of gyro set by the 0x7E command register
 * @note using the function "bmi160_set_command_register()"
 *  value    |   mode
 *  ---------|----------------
 *   0x14    | GYRO_MODE_SUSPEND
 *   0x15    | GYRO_MODE_NORMAL
 *   0x17    | GYRO_MODE_FASTSTARTUP
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_power_mode_stat(u8
*v_gyro_power_mode_stat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYRO_POWER_MODE_STAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_power_mode_stat_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_GYRO_POWER_MODE_STAT);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the accelerometer power mode from
 *	PMU status register 0x03 bit 4 and 5
 *
 *
 *  @param v_accel_power_mode_stat_u8 :	The value of accel power mode
 *	accel_powermode  |   value
 * ------------------|----------
 *    SUSPEND        |   0x00
 *    NORMAL         |   0x01
 *  LOW POWER        |   0x02
 *
 * @note The power mode of accel set by the 0x7E command register
 * @note using the function "bmi160_set_command_register()"
 *  value    |   mode
 *  ---------|----------------
 *   0x11    | ACCEL_MODE_NORMAL
 *   0x12    | ACCEL_LOWPOWER
 *   0x10    | ACCEL_SUSPEND
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_power_mode_stat(u8
*v_accel_power_mode_stat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_POWER_MODE_STAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_power_mode_stat_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_POWER_MODE_STAT);
		}
	return com_rslt;
}
/*!
 *	@brief This API switch mag interface to normal mode
 *	and confirm whether the mode switching done successfully or not
*
 *	@return results of bus communication function and current MAG_PMU result
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_interface_normal(void)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	/* aim to check the result of switching mag normal */
	u8 v_try_times_u8 = BMI160_MAG_NOAMRL_SWITCH_TIMES;
	u8 v_mag_pum_status_u8 = BMI160_INIT_VALUE;

	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt = bmi160_set_command_register(MAG_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	while (v_try_times_u8) {
		com_rslt = bmi160_get_mag_power_mode_stat(&v_mag_pum_status_u8);
		if (v_mag_pum_status_u8 == MAG_INTERFACE_PMU_ENABLE)
			break;
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		v_try_times_u8--;
	}
	if (v_mag_pum_status_u8 == MAG_INTERFACE_PMU_ENABLE)
		com_rslt += SUCCESS;
	else
		com_rslt += E_BMI160_COMM_RES;

	return com_rslt;
}
/*!
 *	@brief This API reads magnetometer data X values
 *	from the register 0x04 and 0x05
 *	@brief The mag sensor data read form auxiliary mag
 *
 *  @param v_mag_x_s16 : The value of mag x
 *  @param v_sensor_select_u8 : Mag selection value
 *  value    |   sensor
 *  ---------|----------------
 *   0       | BMM150
 *   1       | AKM09911 or AKM09912
 *
 *	@note For mag data output rate configuration use the following function
 *	@note bmi160_set_mag_output_data_rate()
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_x(s16 *v_mag_x_s16,
u8 v_sensor_select_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the mag X lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_MAG_X_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_sensor_select_u8) {
		case BST_BMM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_MAG_X_LSB__REG,
			v_data_u8, BMI160_MAG_X_DATA_LENGTH);
			/* X axis*/
			v_data_u8[BMI160_MAG_X_LSB_BYTE] =
			BMI160_GET_BITSLICE(v_data_u8[BMI160_MAG_X_LSB_BYTE],
			BMI160_USER_DATA_MAG_X_LSB);
			*v_mag_x_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS) |
			(v_data_u8[BMI160_MAG_X_LSB_BYTE]));
		break;
		case BST_AKM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_0_MAG_X_LSB__REG,
			v_data_u8, BMI160_MAG_X_DATA_LENGTH);
			*v_mag_x_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_MAG_X_LSB_BYTE]));
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads magnetometer data Y values
 *	from the register 0x06 and 0x07
 *	@brief The mag sensor data read form auxiliary mag
 *
 *  @param v_mag_y_s16 : The value of mag y
 *  @param v_sensor_select_u8 : Mag selection value
 *  value    |   sensor
 *  ---------|----------------
 *   0       | BMM150
 *   1       | AKM09911 or AKM09912
 *
 *	@note For mag data output rate configuration use the following function
 *	@note bmi160_set_mag_output_data_rate()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_y(s16 *v_mag_y_s16,
u8 v_sensor_select_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_OUT_OF_RANGE;
	/* Array contains the mag Y lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_MAG_Y_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_sensor_select_u8) {
		case BST_BMM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_MAG_Y_LSB__REG,
			v_data_u8, BMI160_MAG_Y_DATA_LENGTH);
			/*Y-axis lsb value shifting*/
			v_data_u8[BMI160_MAG_Y_LSB_BYTE] =
			BMI160_GET_BITSLICE(v_data_u8[BMI160_MAG_Y_LSB_BYTE],
			BMI160_USER_DATA_MAG_Y_LSB);
			*v_mag_y_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS) |
			(v_data_u8[BMI160_MAG_Y_LSB_BYTE]));
		break;
		case BST_AKM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_2_MAG_Y_LSB__REG,
			v_data_u8, BMI160_MAG_Y_DATA_LENGTH);
			*v_mag_y_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_MAG_Y_LSB_BYTE]));
		break;
		default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads magnetometer data Z values
 *	from the register 0x08 and 0x09
 *	@brief The mag sensor data read form auxiliary mag
 *
 *  @param v_mag_z_s16 : The value of mag z
 *  @param v_sensor_select_u8 : Mag selection value
 *  value    |   sensor
 *  ---------|----------------
 *   0       | BMM150
 *   1       | AKM09911 or AKM09912
 *
 *	@note For mag data output rate configuration use the following function
 *	@note bmi160_set_mag_output_data_rate()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_z(s16 *v_mag_z_s16,
u8 v_sensor_select_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the mag Z lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_MAG_Z_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_sensor_select_u8) {
		case BST_BMM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_MAG_Z_LSB__REG,
			v_data_u8, BMI160_MAG_Z_DATA_LENGTH);
			/*Z-axis lsb value shifting*/
			v_data_u8[BMI160_MAG_Z_LSB_BYTE] =
			BMI160_GET_BITSLICE(v_data_u8[BMI160_MAG_Z_LSB_BYTE],
			BMI160_USER_DATA_MAG_Z_LSB);
			*v_mag_z_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_07_BITS) |
			(v_data_u8[BMI160_MAG_Z_LSB_BYTE]));
		break;
		case BST_AKM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_4_MAG_Z_LSB__REG,
			v_data_u8, BMI160_MAG_Z_DATA_LENGTH);
			*v_mag_z_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (
			v_data_u8[BMI160_MAG_Z_LSB_BYTE]));
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads magnetometer data RHALL values
 *	from the register 0x0A and 0x0B
 *
 *
 *  @param v_mag_r_s16 : The value of BMM150 r data
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_r(s16 *v_mag_r_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the mag R lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_MAG_R_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_6_RHALL_LSB__REG,
			v_data_u8, BMI160_MAG_R_DATA_LENGTH);
			/*R-axis lsb value shifting*/
			v_data_u8[BMI160_MAG_R_LSB_BYTE] =
			BMI160_GET_BITSLICE(v_data_u8[BMI160_MAG_R_LSB_BYTE],
			BMI160_USER_DATA_MAG_R_LSB);
			*v_mag_r_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_MAG_R_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS) |
			(v_data_u8[BMI160_MAG_R_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads magnetometer data X,Y,Z values
 *	from the register 0x04 to 0x09
 *
 *	@brief The mag sensor data read form auxiliary mag
 *
 *  @param mag : The value of mag xyz data
 *  @param v_sensor_select_u8 : Mag selection value
 *  value    |   sensor
 *  ---------|----------------
 *   0       | BMM150
 *   1       | AKM09911 or AKM09912
 *
 *	@note For mag data output rate configuration use the following function
 *	@note bmi160_set_mag_output_data_rate()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyz(
struct bmi160_mag_t *mag, u8 v_sensor_select_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the mag XYZ lSB and MSB data
		v_data_u8[0] - X-LSB
		v_data_u8[1] - X-MSB
		v_data_u8[0] - Y-LSB
		v_data_u8[1] - Y-MSB
		v_data_u8[0] - Z-LSB
		v_data_u8[1] - Z-MSB
		*/
	u8 v_data_u8[BMI160_MAG_XYZ_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_sensor_select_u8) {
		case BST_BMM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_DATA_MAG_X_LSB__REG,
			v_data_u8, BMI160_MAG_XYZ_DATA_LENGTH);
			/*X-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE] =
			BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE],
			BMI160_USER_DATA_MAG_X_LSB);
			/* Data X */
			mag->x = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE]));
			/* Data Y */
			/*Y-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE] =
			BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE],
			BMI160_USER_DATA_MAG_Y_LSB);
			mag->y = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE]));

			/* Data Z */
			/*Z-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE]
			= BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE],
			BMI160_USER_DATA_MAG_Z_LSB);
			mag->z = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_07_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE]));
		break;
		case BST_AKM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_0_MAG_X_LSB__REG,
			v_data_u8, BMI160_MAG_XYZ_DATA_LENGTH);
			/* Data X */
			mag->x = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE]));
			/* Data Y */
			mag->y  = ((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE]));
			/* Data Z */
			mag->z = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE]));
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
 /*!*
 *	@brief This API reads magnetometer data X,Y,Z,r
 *	values from the register 0x04 to 0x0B
 *
 *	@brief The mag sensor data read form auxiliary mag
 *
 *  @param mag : The value of mag-BMM150 xyzr data
 *
 *	@note For mag data output rate configuration use the following function
 *	@note bmi160_set_mag_output_data_rate()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_mag_xyzr(
struct bmi160_mag_xyzr_t *mag)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8[BMI160_MAG_XYZR_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_MAG_X_LSB__REG,
			v_data_u8, BMI160_MAG_XYZR_DATA_LENGTH);

			/* Data X */
			/*X-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE]
			= BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE],
			BMI160_USER_DATA_MAG_X_LSB);
			mag->x = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_MAG_X_LSB_BYTE]));
			/* Data Y */
			/*Y-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE]
			= BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_Y_LSB_BYTE],
			BMI160_USER_DATA_MAG_Y_LSB);
			mag->y = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_05_BITS)
			| (v_data_u8[
			BMI160_DATA_FRAME_MAG_Y_LSB_BYTE]));

			/* Data Z */
			/*Z-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE]
			= BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE],
			BMI160_USER_DATA_MAG_Z_LSB);
			mag->z = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_07_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_MAG_Z_LSB_BYTE]));

			/* RHall */
			/*R-axis lsb value shifting*/
			v_data_u8[BMI160_DATA_FRAME_MAG_R_LSB_BYTE]
			= BMI160_GET_BITSLICE(
			v_data_u8[BMI160_DATA_FRAME_MAG_R_LSB_BYTE],
			BMI160_USER_DATA_MAG_R_LSB);
			mag->r = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_MAG_R_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_MAG_R_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads gyro data X values
 *	form the register 0x0C and 0x0D
 *
 *
 *
 *
 *  @param v_gyro_x_s16 : The value of gyro x data
 *
 *	@note Gyro Configuration use the following function
 *	@note bmi160_set_gyro_output_data_rate()
 *	@note bmi160_set_gyro_bw()
 *	@note bmi160_set_gyro_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_x(s16 *v_gyro_x_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the gyro X lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[MSB_ONE] - MSB*/
	u8 v_data_u8[BMI160_GYRO_X_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG,
			v_data_u8, BMI160_GYRO_DATA_LENGTH);

			*v_gyro_x_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_GYRO_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_GYRO_X_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads gyro data Y values
 *	form the register 0x0E and 0x0F
 *
 *
 *
 *
 *  @param v_gyro_y_s16 : The value of gyro y data
 *
 *	@note Gyro Configuration use the following function
 *	@note bmi160_set_gyro_output_data_rate()
 *	@note bmi160_set_gyro_bw()
 *	@note bmi160_set_gyro_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error result of communication routines
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_y(s16 *v_gyro_y_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the gyro Y lSB and MSB data
		v_data_u8[LSB_ZERO] - LSB
		v_data_u8[MSB_ONE] - MSB*/
	u8 v_data_u8[BMI160_GYRO_Y_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro y data*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_10_GYRO_Y_LSB__REG,
			v_data_u8, BMI160_GYRO_DATA_LENGTH);

			*v_gyro_y_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_GYRO_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_GYRO_Y_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads gyro data Z values
 *	form the register 0x10 and 0x11
 *
 *
 *
 *
 *  @param v_gyro_z_s16 : The value of gyro z data
 *
 *	@note Gyro Configuration use the following function
 *	@note bmi160_set_gyro_output_data_rate()
 *	@note bmi160_set_gyro_bw()
 *	@note bmi160_set_gyro_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_z(s16 *v_gyro_z_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the gyro Z lSB and MSB data
		v_data_u8[LSB_ZERO] - LSB
		v_data_u8[MSB_ONE] - MSB*/
	u8 v_data_u8[BMI160_GYRO_Z_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro z data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_12_GYRO_Z_LSB__REG,
			v_data_u8, BMI160_GYRO_DATA_LENGTH);

			*v_gyro_z_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_GYRO_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_GYRO_Z_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads gyro data X,Y,Z values
 *	from the register 0x0C to 0x11
 *
 *
 *
 *
 *  @param gyro : The value of gyro xyz
 *
 *	@note Gyro Configuration use the following function
 *	@note bmi160_set_gyro_output_data_rate()
 *	@note bmi160_set_gyro_bw()
 *	@note bmi160_set_gyro_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_gyro_xyz(struct bmi160_gyro_t *gyro)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the mag XYZ lSB and MSB data
		v_data_u8[0] - X-LSB
		v_data_u8[1] - X-MSB
		v_data_u8[0] - Y-LSB
		v_data_u8[1] - Y-MSB
		v_data_u8[0] - Z-LSB
		v_data_u8[1] - Z-MSB
		*/
	u8 v_data_u8[BMI160_GYRO_XYZ_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro xyz data*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG,
			v_data_u8, BMI160_GYRO_XYZ_DATA_LENGTH);

			/* Data X */
			gyro->x = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_GYRO_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_GYRO_X_LSB_BYTE]));
			/* Data Y */
			gyro->y = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_GYRO_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_GYRO_Y_LSB_BYTE]));

			/* Data Z */
			gyro->z = (s16)
			((((s32)((s8)v_data_u8[
			BMI160_DATA_FRAME_GYRO_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_DATA_FRAME_GYRO_Z_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads accelerometer data X values
 *	form the register 0x12 and 0x13
 *
 *
 *
 *
 *  @param v_accel_x_s16 : The value of accel x
 *
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_x(s16 *v_accel_x_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel X lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_ACCEL_X_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_14_ACCEL_X_LSB__REG,
			v_data_u8, BMI160_ACCEL_DATA_LENGTH);

			*v_accel_x_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_ACCEL_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_ACCEL_X_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads accelerometer data Y values
 *	form the register 0x14 and 0x15
 *
 *
 *
 *
 *  @param v_accel_y_s16 : The value of accel y
 *
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_y(s16 *v_accel_y_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel Y lSB and MSB data
		v_data_u8[0] - LSB
		v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_ACCEL_Y_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_16_ACCEL_Y_LSB__REG,
			v_data_u8, BMI160_ACCEL_DATA_LENGTH);

			*v_accel_y_s16 = (s16)
			((((s32)((s8)v_data_u8[BMI160_ACCEL_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (v_data_u8[BMI160_ACCEL_Y_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads accelerometer data Z values
 *	form the register 0x16 and 0x17
 *
 *
 *
 *
 *  @param v_accel_z_s16 : The value of accel z
 *
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_z(s16 *v_accel_z_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel Z lSB and MSB data
		a_data_u8r[LSB_ZERO] - LSB
		a_data_u8r[MSB_ONE] - MSB*/
	u8 a_data_u8r[BMI160_ACCEL_Z_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_18_ACCEL_Z_LSB__REG,
			a_data_u8r, BMI160_ACCEL_DATA_LENGTH);

			*v_accel_z_s16 = (s16)
			((((s32)((s8)a_data_u8r[BMI160_ACCEL_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_ACCEL_Z_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads accelerometer data X,Y,Z values
 *	from the register 0x12 to 0x17
 *
 *
 *
 *
 *  @param accel :The value of accel xyz
 *
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_xyz(
struct bmi160_accel_t *accel)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel XYZ lSB and MSB data
	a_data_u8r[0] - X-LSB
	a_data_u8r[1] - X-MSB
	a_data_u8r[0] - Y-LSB
	a_data_u8r[1] - Y-MSB
	a_data_u8r[0] - Z-LSB
	a_data_u8r[1] - Z-MSB
	*/
	u8 a_data_u8r[BMI160_ACCEL_XYZ_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_DATA_14_ACCEL_X_LSB__REG,
			a_data_u8r, BMI160_ACCEL_XYZ_DATA_LENGTH);

			/* Data X */
			accel->x = (s16)
			((((s32)((s8)a_data_u8r[
			BMI160_DATA_FRAME_ACCEL_X_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_DATA_FRAME_ACCEL_X_LSB_BYTE]));
			/* Data Y */
			accel->y = (s16)
			((((s32)((s8)a_data_u8r[
			BMI160_DATA_FRAME_ACCEL_Y_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_DATA_FRAME_ACCEL_Y_LSB_BYTE]));

			/* Data Z */
			accel->z = (s16)
			((((s32)((s8)a_data_u8r[
			BMI160_DATA_FRAME_ACCEL_Z_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_DATA_FRAME_ACCEL_Z_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads sensor_time from the register
 *	0x18 to 0x1A
 *
 *
 *  @param v_sensor_time_u32 : The value of sensor time
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_sensor_time(u32 *v_sensor_time_u32)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the sensor time it is 32 bit data
	a_data_u8r[0] - sensor time
	a_data_u8r[1] - sensor time
	a_data_u8r[0] - sensor time
	*/
	u8 a_data_u8r[BMI160_SENSOR_TIME_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_SENSORTIME_0_SENSOR_TIME_LSB__REG,
			a_data_u8r, BMI160_SENSOR_TIME_LENGTH);

			*v_sensor_time_u32 = (u32)
			((((u32)a_data_u8r[BMI160_SENSOR_TIME_MSB_BYTE])
			<< BMI160_SHIFT_BIT_POSITION_BY_16_BITS)
			|(((u32)a_data_u8r[BMI160_SENSOR_TIME_XLSB_BYTE])
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_SENSOR_TIME_LSB_BYTE]));
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the Gyroscope self test
 *	status from the register 0x1B bit 1
 *
 *
 *  @param v_gyro_selftest_u8 : The value of gyro self test status
 *  value    |   status
 *  ---------|----------------
 *   0       | Gyroscope self test is running or failed
 *   1       | Gyroscope self test completed successfully
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_selftest(u8
*v_gyro_selftest_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_GYRO_SELFTEST_OK__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_selftest_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_GYRO_SELFTEST_OK);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of
 *	mag manual interface operation form the register 0x1B bit 2
 *
 *
 *
 *  @param v_mag_manual_stat_u8 : The value of mag manual operation status
 *  value    |   status
 *  ---------|----------------
 *   0       | Indicates no manual magnetometer
 *   -       | interface operation is ongoing
 *   1       | Indicates manual magnetometer
 *   -       | interface operation is ongoing
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_manual_operation_stat(u8
*v_mag_manual_stat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read manual operation*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_MAG_MANUAL_OPERATION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_manual_stat_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_MAG_MANUAL_OPERATION);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the fast offset compensation
 *	status form the register 0x1B bit 3
 *
 *
 *  @param v_foc_rdy_u8 : The status of fast compensation
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_rdy(u8
*v_foc_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the FOC status*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_FOC_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_foc_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_FOC_RDY);
		}
	return com_rslt;
}
/*!
 * @brief This API Reads the nvm_rdy status from the
 *	resister 0x1B bit 4
 *
 *
 *  @param v_nvm_rdy_u8 : The value of NVM ready status
 *  value    |   status
 *  ---------|----------------
 *   0       | NVM write operation in progress
 *   1       | NVM is ready to accept a new write trigger
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_rdy(u8
*v_nvm_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the nvm ready status*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_NVM_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_nvm_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_NVM_RDY);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of mag data ready
 *	from the register 0x1B bit 5
 *	The status get reset when one mag data register is read out
 *
 *  @param v_data_rdy_u8 : The value of mag data ready status
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_data_rdy_mag(u8
*v_data_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_DATA_RDY_MAG__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_data_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_DATA_RDY_MAG);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of gyro data ready form the
 *	register 0x1B bit 6
 *	The status get reset when gyro data register read out
 *
 *
 *	@param v_data_rdy_u8 :	The value of gyro data ready
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_data_rdy(u8
*v_data_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_DATA_RDY_GYRO__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_data_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_DATA_RDY_GYRO);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of accel data ready form the
 *	register 0x1B bit 7
 *	The status get reset when accel data register read out
 *
 *
 *	@param v_data_rdy_u8 :	The value of accel data ready status
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_data_rdy(u8
*v_data_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/*reads the status of accel data ready*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STAT_DATA_RDY_ACCEL__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_data_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_DATA_RDY_ACCEL);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the step detector interrupt status
 *	from the register 0x1C bit 0
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_step_intr_u8 : The status of step detector interrupt
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_step_intr(u8
*v_step_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_STEP_INTR__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_step_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_STEP_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the
 *	significant motion interrupt status
 *	from the register 0x1C bit 1
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *
 *  @param v_significant_intr_u8 : The status of step
 *	motion interrupt
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_significant_intr(u8
*v_significant_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_significant_intr_u8  = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR);
		}
	return com_rslt;
}
 /*!
 *	@brief This API reads the any motion interrupt status
 *	from the register 0x1C bit 2
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *  @param v_any_motion_intr_u8 : The status of any-motion interrupt
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_any_motion_intr(u8
*v_any_motion_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_ANY_MOTION__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_any_motion_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_ANY_MOTION);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the power mode trigger interrupt status
 *	from the register 0x1C bit 3
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *
 *  @param v_pmu_trigger_intr_u8 : The status of power mode trigger interrupt
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_pmu_trigger_intr(u8
*v_pmu_trigger_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_PMU_TRIGGER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_pmu_trigger_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_PMU_TRIGGER);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the double tab status
 *	from the register 0x1C bit 4
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_double_tap_intr_u8 :The status of double tab interrupt
 *
 *	@note Double tap interrupt can be configured by the following functions
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_double_tap()
 *	@note AXIS MAPPING
 *	@note bmi160_get_stat2_tap_first_x()
 *	@note bmi160_get_stat2_tap_first_y()
 *	@note bmi160_get_stat2_tap_first_z()
 *	@note DURATION
 *	@note bmi160_set_intr_tap_durn()
 *	@note THRESHOLD
 *	@note bmi160_set_intr_tap_thres()
 *	@note TAP QUIET
 *	@note bmi160_set_intr_tap_quiet()
 *	@note TAP SHOCK
 *	@note bmi160_set_intr_tap_shock()
 *	@note TAP SOURCE
 *	@note bmi160_set_intr_tap_source()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_double_tap_intr(u8
*v_double_tap_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_DOUBLE_TAP_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_double_tap_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_DOUBLE_TAP_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the single tab status
 *	from the register 0x1C bit 5
 *	flag is associated with a specific interrupt function.
 *	It is set when the single tab interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt
 *	signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_single_tap_intr_u8 :The status of single tap interrupt
 *
 *	@note Single tap interrupt can be configured by the following functions
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_single_tap()
 *	@note AXIS MAPPING
 *	@note bmi160_get_stat2_tap_first_x()
 *	@note bmi160_get_stat2_tap_first_y()
 *	@note bmi160_get_stat2_tap_first_z()
 *	@note DURATION
 *	@note bmi160_set_intr_tap_durn()
 *	@note THRESHOLD
 *	@note bmi160_set_intr_tap_thres()
 *	@note TAP QUIET
 *	@note bmi160_set_intr_tap_quiet()
 *	@note TAP SHOCK
 *	@note bmi160_set_intr_tap_shock()
 *	@note TAP SOURCE
 *	@note bmi160_set_intr_tap_source()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_single_tap_intr(u8
*v_single_tap_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_SINGLE_TAP_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_single_tap_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_SINGLE_TAP_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the orient status
 *	from the register 0x1C bit 6
 *	flag is associated with a specific interrupt function.
 *	It is set when the orient interrupt triggers. The
 *	setting of INT_LATCH controls if the
 *	interrupt signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_orient_intr_u8 : The status of orient interrupt
 *
 *	@note For orient interrupt configuration use the following functions
 *	@note STATUS
 *	@note bmi160_get_stat0_orient_intr()
 *	@note AXIS MAPPING
 *	@note bmi160_get_stat3_orient_xy()
 *	@note bmi160_get_stat3_orient_z()
 *	@note bmi160_set_intr_orient_axes_enable()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_orient()
 *	@note INTERRUPT OUTPUT
 *	@note bmi160_set_intr_orient_ud_enable()
 *	@note THETA
 *	@note bmi160_set_intr_orient_theta()
 *	@note HYSTERESIS
 *	@note bmi160_set_intr_orient_hyst()
 *	@note BLOCKING
 *	@note bmi160_set_intr_orient_blocking()
 *	@note MODE
 *	@note bmi160_set_intr_orient_mode()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_orient_intr(u8
*v_orient_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_ORIENT__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_ORIENT);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the flat interrupt status
 *	from the register 0x1C bit 7
 *	flag is associated with a specific interrupt function.
 *	It is set when the flat interrupt triggers. The
 *	setting of INT_LATCH controls if the
 *	interrupt signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_flat_intr_u8 : The status of  flat interrupt
 *
 *	@note For flat configuration use the following functions
 *	@note STATS
 *	@note bmi160_get_stat0_flat_intr()
 *	@note bmi160_get_stat3_flat()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_flat()
 *	@note THETA
 *	@note bmi160_set_intr_flat_theta()
 *	@note HOLD TIME
 *	@note bmi160_set_intr_flat_hold()
 *	@note HYSTERESIS
 *	@note bmi160_set_intr_flat_hyst()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat0_flat_intr(u8
*v_flat_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_0_FLAT__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_flat_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_0_FLAT);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the high_g interrupt status
 *	from the register 0x1D bit 2
 *	flag is associated with a specific interrupt function.
 *	It is set when the high g  interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt signal and hence the
 *	respective interrupt flag will be permanently
 *	latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_high_g_intr_u8 : The status of high_g interrupt
 *
 *	@note High_g interrupt configured by following functions
 *	@note STATUS
 *	@note bmi160_get_stat1_high_g_intr()
 *	@note AXIS MAPPING
 *	@note bmi160_get_stat3_high_g_first_x()
 *	@note bmi160_get_stat3_high_g_first_y()
 *	@note bmi160_get_stat3_high_g_first_z()
 *	@note SIGN MAPPING
 *	@note bmi160_get_stat3_high_g_first_sign()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_high_g()
  *	@note HYSTERESIS
 *	@note bmi160_set_intr_high_g_hyst()
 *	@note DURATION
 *	@note bmi160_set_intr_high_g_durn()
 *	@note THRESHOLD
 *	@note bmi160_set_intr_high_g_thres()
 *	@note SOURCE
 *	@note bmi160_set_intr_low_high_source()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_high_g_intr(u8
*v_high_g_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_HIGH_G_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_HIGH_G_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the low g interrupt status
 *	from the register 0x1D bit 3
 *	flag is associated with a specific interrupt function.
 *	It is set when the low g  interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_low_g_intr_u8 : The status of low_g interrupt
 *
 *	@note Low_g interrupt configured by following functions
 *	@note STATUS
 *	@note bmi160_get_stat1_low_g_intr()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_low_g()
 *	@note SOURCE
 *	@note bmi160_set_intr_low_high_source()
 *	@note DURATION
 *	@note bmi160_set_intr_low_g_durn()
 *	@note THRESHOLD
 *	@note bmi160_set_intr_low_g_thres()
 *	@note HYSTERESIS
 *	@note bmi160_set_intr_low_g_hyst()
 *	@note MODE
 *	@note bmi160_set_intr_low_g_mode()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_low_g_intr(u8
*v_low_g_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_LOW_G_INTR__REG, &v_data_u8,
			 BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_g_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_LOW_G_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads data ready interrupt status
 *	from the register 0x1D bit 4
 *	flag is associated with a specific interrupt function.
 *	It is set when the  data ready  interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_data_rdy_intr_u8 : The status of data ready interrupt
 *
 *	@note Data ready interrupt configured by following functions
 *	@note STATUS
 *	@note bmi160_get_stat1_data_rdy_intr()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_data_rdy()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_data_rdy_intr(u8
*v_data_rdy_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_DATA_RDY_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_data_rdy_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_DATA_RDY_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads data ready FIFO full interrupt status
 *	from the register 0x1D bit 5
 *	flag is associated with a specific interrupt function.
 *	It is set when the FIFO full interrupt triggers. The
 *	setting of INT_LATCH controls if the
 *	interrupt signal and hence the
 *	respective interrupt flag will
 *	be permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_fifo_full_intr_u8 : The status of fifo full interrupt
 *
 *	@note FIFO full interrupt can be configured by following functions
 *	@note bmi160_set_intr_fifo_full()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_fifo_full_intr(u8
*v_fifo_full_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_FIFO_FULL_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_full_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_FIFO_FULL_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads data
 *	 ready FIFO watermark interrupt status
 *	from the register 0x1D bit 6
 *	flag is associated with a specific interrupt function.
 *	It is set when the FIFO watermark interrupt triggers. The
 *	setting of INT_LATCH controls if the
 *	interrupt signal and hence the
 *	respective interrupt flag will be
 *	permanently latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_fifo_wm_intr_u8 : The status of fifo water mark interrupt
 *
 *	@note FIFO full interrupt can be configured by following functions
 *	@note bmi160_set_intr_fifo_wm()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_fifo_wm_intr(u8
*v_fifo_wm_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_FIFO_WM_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_wm_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_FIFO_WM_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads data ready no motion interrupt status
 *	from the register 0x1D bit 7
 *	flag is associated with a specific interrupt function.
 *	It is set when the no motion  interrupt triggers. The
 *	setting of INT_LATCH controls if the interrupt signal and hence the
 *	respective interrupt flag will be permanently
 *	latched, temporarily latched
 *	or not latched.
 *
 *
 *
 *
 *  @param v_nomotion_intr_u8 : The status of no motion interrupt
 *
 *	@note No motion interrupt can be configured by following function
 *	@note STATUS
 *	@note bmi160_get_stat1_nomotion_intr()
 *	@note INTERRUPT MAPPING
 *	@note bmi160_set_intr_nomotion()
 *	@note DURATION
 *	@note bmi160_set_intr_slow_no_motion_durn()
 *	@note THRESHOLD
 *	@note bmi160_set_intr_slow_no_motion_thres()
 *	@note SLOW/NO MOTION SELECT
 *	@note bmi160_set_intr_slow_no_motion_select()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat1_nomotion_intr(u8
*v_nomotion_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the no motion interrupt*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_1_NOMOTION_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_nomotion_intr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_1_NOMOTION_INTR);
		}
	return com_rslt;
}
/*!
 *@brief This API reads the status of any motion first x
 *	from the register 0x1E bit 0
 *
 *
 *@param v_anymotion_first_x_u8 : The status of any motion first x interrupt
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by x axis
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_any_motion_first_x(u8
*v_anymotion_first_x_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the any motion first x interrupt*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_anymotion_first_x_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_X);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of any motion first y interrupt
 *	from the register 0x1E bit 1
 *
 *
 *
 *@param v_any_motion_first_y_u8 : The status of any motion first y interrupt
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_any_motion_first_y(u8
*v_any_motion_first_y_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the any motion first y interrupt*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_any_motion_first_y_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Y);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of any motion first z interrupt
 *	from the register 0x1E bit 2
 *
 *
 *
 *
 *@param v_any_motion_first_z_u8 : The status of any motion first z interrupt
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_any_motion_first_z(u8
*v_any_motion_first_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the any motion first z interrupt*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_any_motion_first_z_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Z);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the any motion sign status from the
 *	register 0x1E bit 3
 *
 *
 *
 *
 *  @param v_anymotion_sign_u8 : The status of any motion sign
 *  value     |  sign
 * -----------|-------------
 *   0        | positive
 *   1        | negative
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_any_motion_sign(u8
*v_anymotion_sign_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read any motion sign interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_SIGN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_anymotion_sign_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_ANY_MOTION_SIGN);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the any motion tap first x status from the
 *	register 0x1E bit 4
 *
 *
 *
 *
 *  @param v_tap_first_x_u8 :The status of any motion tap first x
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by x axis
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_tap_first_x(u8
*v_tap_first_x_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap first x interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_first_x_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_X);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the tap first y interrupt status from the
 *	register 0x1E bit 5
 *
 *
 *
 *
 *  @param v_tap_first_y_u8 :The status of tap first y interrupt
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_tap_first_y(u8
*v_tap_first_y_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap first y interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_first_y_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_Y);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the tap first z interrupt status  from the
 *	register 0x1E bit 6
 *
 *
 *
 *
 *  @param v_tap_first_z_u8 :The status of tap first z interrupt
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_tap_first_z(u8
*v_tap_first_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap first z interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_first_z_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_TAP_FIRST_Z);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the tap sign status from the
 *	register 0x1E bit 7
 *
 *
 *
 *
 *  @param v_tap_sign_u8 : The status of tap sign
 *  value     |  sign
 * -----------|-------------
 *   0        | positive
 *   1        | negative
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat2_tap_sign(u8
*v_tap_sign_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap_sign interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_2_TAP_SIGN__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_sign_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_2_TAP_SIGN);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the high_g first x status from the
 *	register 0x1F bit 0
 *
 *
 *
 *
 *  @param v_high_g_first_x_u8 :The status of high_g first x
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by x axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_high_g_first_x(u8
*v_high_g_first_x_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read highg_x interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_first_x_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_X);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the high_g first y status from the
 *	register 0x1F bit 1
 *
 *
 *
 *
 *  @param v_high_g_first_y_u8 : The status of high_g first y
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_high_g_first_y(u8
*v_high_g_first_y_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read highg_y interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_first_y_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_Y);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the high_g first z status from the
 *	register 0x1F bit 3
 *
 *
 *
 *
 *  @param v_high_g_first_z_u8 : The status of high_g first z
 *  value     |  status
 * -----------|-------------
 *   0        | not triggered
 *   1        | triggered by z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_high_g_first_z(u8
*v_high_g_first_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read highg_z interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_first_z_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_HIGH_G_FIRST_Z);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the high sign status from the
 *	register 0x1F bit 3
 *
 *
 *
 *
 *  @param v_high_g_sign_u8 :The status of high sign
 *  value     |  sign
 * -----------|-------------
 *   0        | positive
 *   1        | negative
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_high_g_sign(u8
*v_high_g_sign_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read highg_sign interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_HIGH_G_SIGN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_sign_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_HIGH_G_SIGN);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of orient_xy plane
 *	from the register 0x1F bit 4 and 5
 *
 *
 *  @param v_orient_xy_u8 :The status of orient_xy plane
 *  value     |  status
 * -----------|-------------
 *   0x00     | portrait upright
 *   0x01     | portrait upside down
 *   0x02     | landscape left
 *   0x03     | landscape right
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_orient_xy(u8
*v_orient_xy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orient plane xy interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_ORIENT_XY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_xy_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_ORIENT_XY);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the status of orient z plane
 *	from the register 0x1F bit 6
 *
 *
 *  @param v_orient_z_u8 :The status of orient z
 *  value     |  status
 * -----------|-------------
 *   0x00     | upward looking
 *   0x01     | downward looking
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_orient_z(u8
*v_orient_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orient z plane interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_ORIENT_Z__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_z_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_ORIENT_Z);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the flat status from the register
 *	0x1F bit 7
 *
 *
 *  @param v_flat_u8 : The status of flat interrupt
 *  value     |  status
 * -----------|-------------
 *   0x00     | non flat
 *   0x01     | flat position
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_stat3_flat(u8
*v_flat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read flat interrupt status */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_INTR_STAT_3_FLAT__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_flat_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_STAT_3_FLAT);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the temperature of the sensor
 *	from the register 0x21 bit 0 to 7
 *
 *
 *
 *  @param v_temp_s16 : The value of temperature
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_temp(s16
*v_temp_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the temperature lSB and MSB data
	v_data_u8[0] - LSB
	v_data_u8[1] - MSB*/
	u8 v_data_u8[BMI160_TEMP_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read temperature data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_TEMP_LSB_VALUE__REG, v_data_u8,
			BMI160_TEMP_DATA_LENGTH);
			*v_temp_s16 =
			(s16)(((s32)((s8) (v_data_u8[BMI160_TEMP_MSB_BYTE]) <<
			BMI160_SHIFT_BIT_POSITION_BY_08_BITS))
			| v_data_u8[BMI160_TEMP_LSB_BYTE]);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the  of the sensor
 *	form the register 0x23 and 0x24 bit 0 to 7 and 0 to 2
 *	@brief this byte counter is updated each time a complete frame
 *	was read or writtern
 *
 *
 *  @param v_fifo_length_u32 : The value of fifo byte counter
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_length(u32 *v_fifo_length_u32)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the fifo length data
	v_data_u8[0] - fifo length
	v_data_u8[1] - fifo length*/
	u8 a_data_u8r[BMI160_FIFO_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read fifo length*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG, a_data_u8r,
			 BMI160_FIFO_DATA_LENGTH);

			a_data_u8r[BMI160_FIFO_LENGTH_MSB_BYTE] =
			BMI160_GET_BITSLICE(
			a_data_u8r[BMI160_FIFO_LENGTH_MSB_BYTE],
			BMI160_USER_FIFO_BYTE_COUNTER_MSB);

			*v_fifo_length_u32 =
			(u32)(((u32)((u8) (
			a_data_u8r[BMI160_FIFO_LENGTH_MSB_BYTE]) <<
			BMI160_SHIFT_BIT_POSITION_BY_08_BITS))
			| a_data_u8r[BMI160_FIFO_LENGTH_LSB_BYTE]);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads the fifo data of the sensor
 *	from the register 0x24
 *	@brief Data format depends on the setting of register FIFO_CONFIG
 *
 *
 *
 *  @param v_fifodata_u8 : Pointer holding the fifo data
 *  @param fifo_length_u16 : The value of fifo length maximum
 *	1024
 *
 *	@note For reading FIFO data use the following functions
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_fifo_data(
u8 *v_fifodata_u8, u16 v_fifo_length_u16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read fifo data*/
			com_rslt =
			p_bmi160->BMI160_BURST_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DATA__REG,
			v_fifodata_u8, v_fifo_length_u16);

		}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the
 *	accel output date rate form the register 0x40 bit 0 to 3
 *
 *
 *  @param  v_output_data_rate_u8 :The value of accel output date rate
 *  value |  output data rate
 * -------|--------------------------
 *	 0    |	BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED
 *	 1	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_0_78HZ
 *	 2	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1_56HZ
 *	 3    |	BMI160_ACCEL_OUTPUT_DATA_RATE_3_12HZ
 *	 4    | BMI160_ACCEL_OUTPUT_DATA_RATE_6_25HZ
 *	 5	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_12_5HZ
 *	 6	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_25HZ
 *	 7	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_50HZ
 *	 8	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ
 *	 9	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_200HZ
 *	 10	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_400HZ
 *	 11	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_800HZ
 *	 12	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_output_data_rate(
u8 *v_output_data_rate_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel output data rate*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_output_data_rate_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the
 *	accel output date rate form the register 0x40 bit 0 to 3
 *
 *
 *  @param  v_output_data_rate_u8 :The value of accel output date rate
 *  value |  output data rate
 * -------|--------------------------
 *	 0    |	BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED
 *	 1	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_0_78HZ
 *	 2	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1_56HZ
 *	 3    |	BMI160_ACCEL_OUTPUT_DATA_RATE_3_12HZ
 *	 4    | BMI160_ACCEL_OUTPUT_DATA_RATE_6_25HZ
 *	 5	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_12_5HZ
 *	 6	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_25HZ
 *	 7	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_50HZ
 *	 8	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ
 *	 9	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_200HZ
 *	 10	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_400HZ
 *	 11	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_800HZ
 *	 12	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_output_data_rate(
u8 v_output_data_rate_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* accel output data rate selection */
		if ((v_output_data_rate_u8 != BMI160_INIT_VALUE) &&
		(v_output_data_rate_u8 <= BMI160_MAX_ACCEL_OUTPUT_DATA_RATE)) {
			/* write accel output data rate */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE,
				v_output_data_rate_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the
 *	accel bandwidth from the register 0x40 bit 4 to 6
 *	@brief bandwidth parameter determines filter configuration(acc_us=0)
 *	and averaging for under sampling mode(acc_us=1)
 *
 *
 *  @param  v_bw_u8 : The value of accel bandwidth
 *
 *	@note accel bandwidth depends on under sampling parameter
 *	@note under sampling parameter cab be set by the function
 *	"BMI160_SET_ACCEL_UNDER_SAMPLING_PARAMETER"
 *
 *	@note Filter configuration
 *  accel_us  | Filter configuration
 * -----------|---------------------
 *    0x00    |  OSR4 mode
 *    0x01    |  OSR2 mode
 *    0x02    |  normal mode
 *    0x03    |  CIC mode
 *    0x04    |  Reserved
 *    0x05    |  Reserved
 *    0x06    |  Reserved
 *    0x07    |  Reserved
 *
 *	@note accel under sampling mode
 *  accel_us  | Under sampling mode
 * -----------|---------------------
 *    0x00    |  no averaging
 *    0x01    |  average 2 samples
 *    0x02    |  average 4 samples
 *    0x03    |  average 8 samples
 *    0x04    |  average 16 samples
 *    0x05    |  average 32 samples
 *    0x06    |  average 64 samples
 *    0x07    |  average 128 samples
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_bw(u8 *v_bw_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel bandwidth */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_ACCEL_BW__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_bw_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_CONFIG_ACCEL_BW);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the
 *	accel bandwidth from the register 0x40 bit 4 to 6
 *	@brief bandwidth parameter determines filter configuration(acc_us=0)
 *	and averaging for under sampling mode(acc_us=1)
 *
 *
 *  @param  v_bw_u8 : The value of accel bandwidth
 *
 *	@note accel bandwidth depends on under sampling parameter
 *	@note under sampling parameter cab be set by the function
 *	"BMI160_SET_ACCEL_UNDER_SAMPLING_PARAMETER"
 *
 *	@note Filter configuration
 *  accel_us  | Filter configuration
 * -----------|---------------------
 *    0x00    |  OSR4 mode
 *    0x01    |  OSR2 mode
 *    0x02    |  normal mode
 *    0x03    |  CIC mode
 *    0x04    |  Reserved
 *    0x05    |  Reserved
 *    0x06    |  Reserved
 *    0x07    |  Reserved
 *
 *	@note accel under sampling mode
 *  accel_us  | Under sampling mode
 * -----------|---------------------
 *    0x00    |  no averaging
 *    0x01    |  average 2 samples
 *    0x02    |  average 4 samples
 *    0x03    |  average 8 samples
 *    0x04    |  average 16 samples
 *    0x05    |  average 32 samples
 *    0x06    |  average 64 samples
 *    0x07    |  average 128 samples
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_bw(u8 v_bw_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* select accel bandwidth*/
		if (v_bw_u8 <= BMI160_MAX_ACCEL_BW) {
			/* write accel bandwidth*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_ACCEL_BW__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_ACCEL_CONFIG_ACCEL_BW,
				v_bw_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACCEL_CONFIG_ACCEL_BW__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the accel
 *	under sampling parameter form the register 0x40 bit 7
 *
 *
 *
 *
 *	@param  v_accel_under_sampling_u8 : The value of accel under sampling
 *	value    | under_sampling
 * ----------|---------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_under_sampling_parameter(
u8 *v_accel_under_sampling_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel under sampling parameter */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_under_sampling_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the accel
 *	under sampling parameter form the register 0x40 bit 7
 *
 *
 *
 *
 *	@param  v_accel_under_sampling_u8 : The value of accel under sampling
 *	value    | under_sampling
 * ----------|---------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_under_sampling_parameter(
u8 v_accel_under_sampling_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_accel_under_sampling_u8 <= BMI160_MAX_UNDER_SAMPLING) {
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			/* write the accel under sampling parameter */
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING,
			v_accel_under_sampling_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
/*!
 *	@brief This API is used to get the ranges
 *	(g values) of the accel from the register 0x41 bit 0 to 3
 *
 *
 *
 *
 *  @param v_range_u8 : The value of accel g range
 *	value    | g_range
 * ----------|-----------
 *   0x03    | BMI160_ACCEL_RANGE_2G
 *   0x05    | BMI160_ACCEL_RANGE_4G
 *   0x08    | BMI160_ACCEL_RANGE_8G
 *   0x0C    | BMI160_ACCEL_RANGE_16G
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_range(
u8 *v_range_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel range*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_RANGE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_range_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_ACCEL_RANGE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the ranges
 *	(g values) of the accel from the register 0x41 bit 0 to 3
 *
 *
 *
 *
 *  @param v_range_u8 : The value of accel g range
 *	value    | g_range
 * ----------|-----------
 *   0x03    | BMI160_ACCEL_RANGE_2G
 *   0x05    | BMI160_ACCEL_RANGE_4G
 *   0x08    | BMI160_ACCEL_RANGE_8G
 *   0x0C    | BMI160_ACCEL_RANGE_16G
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_range(u8 v_range_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if ((v_range_u8 == BMI160_ACCEL_RANGE0) ||
			(v_range_u8 == BMI160_ACCEL_RANGE1) ||
			(v_range_u8 == BMI160_ACCEL_RANGE3) ||
			(v_range_u8 == BMI160_ACCEL_RANGE4)) {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_ACCEL_RANGE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8  = BMI160_SET_BITSLICE(
				v_data_u8, BMI160_USER_ACCEL_RANGE,
				v_range_u8);
				/* write the accel range*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACCEL_RANGE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the
 *	gyroscope output data rate from the register 0x42 bit 0 to 3
 *
 *
 *
 *
 *  @param  v_output_data_rate_u8 :The value of gyro output data rate
 *  value     |      gyro output data rate
 * -----------|-----------------------------
 *   0x00     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x01     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x02     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x03     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x04     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x05     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x06     | BMI160_GYRO_OUTPUT_DATA_RATE_25HZ
 *   0x07     | BMI160_GYRO_OUTPUT_DATA_RATE_50HZ
 *   0x08     | BMI160_GYRO_OUTPUT_DATA_RATE_100HZ
 *   0x09     | BMI160_GYRO_OUTPUT_DATA_RATE_200HZ
 *   0x0A     | BMI160_GYRO_OUTPUT_DATA_RATE_400HZ
 *   0x0B     | BMI160_GYRO_OUTPUT_DATA_RATE_800HZ
 *   0x0C     | BMI160_GYRO_OUTPUT_DATA_RATE_1600HZ
 *   0x0D     | BMI160_GYRO_OUTPUT_DATA_RATE_3200HZ
 *   0x0E     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x0F     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_output_data_rate(
u8 *v_output_data_rate_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro output data rate*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYRO_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_output_data_rate_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_GYRO_CONFIG_OUTPUT_DATA_RATE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the
 *	gyroscope output data rate from the register 0x42 bit 0 to 3
 *
 *
 *
 *
 *  @param  v_output_data_rate_u8 :The value of gyro output data rate
 *  value     |      gyro output data rate
 * -----------|-----------------------------
 *   0x00     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x01     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x02     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x03     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x04     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x05     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x06     | BMI160_GYRO_OUTPUT_DATA_RATE_25HZ
 *   0x07     | BMI160_GYRO_OUTPUT_DATA_RATE_50HZ
 *   0x08     | BMI160_GYRO_OUTPUT_DATA_RATE_100HZ
 *   0x09     | BMI160_GYRO_OUTPUT_DATA_RATE_200HZ
 *   0x0A     | BMI160_GYRO_OUTPUT_DATA_RATE_400HZ
 *   0x0B     | BMI160_GYRO_OUTPUT_DATA_RATE_800HZ
 *   0x0C     | BMI160_GYRO_OUTPUT_DATA_RATE_1600HZ
 *   0x0D     | BMI160_GYRO_OUTPUT_DATA_RATE_3200HZ
 *   0x0E     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *   0x0F     | BMI160_GYRO_OUTPUT_DATA_RATE_RESERVED
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_output_data_rate(
u8 v_output_data_rate_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* select the gyro output data rate*/
		if ((v_output_data_rate_u8 <  BMI160_OUTPUT_DATA_RATE6) &&
		(v_output_data_rate_u8 != BMI160_INIT_VALUE)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE1)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE2)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE3)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE4)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE5)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE6)
		&& (v_output_data_rate_u8 !=  BMI160_OUTPUT_DATA_RATE7)) {
			/* write the gyro output data rate */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYRO_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_CONFIG_OUTPUT_DATA_RATE,
				v_output_data_rate_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_GYRO_CONFIG_OUTPUT_DATA_RATE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the
 *	data of gyro from the register 0x42 bit 4 to 5
 *
 *
 *
 *
 *  @param  v_bw_u8 : The value of gyro bandwidth
 *  value     | gyro bandwidth
 *  ----------|----------------
 *   0x00     | BMI160_GYRO_OSR4_MODE
 *   0x01     | BMI160_GYRO_OSR2_MODE
 *   0x02     | BMI160_GYRO_NORMAL_MODE
 *   0x03     | BMI160_GYRO_CIC_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_bw(u8 *v_bw_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro bandwidth*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYRO_CONFIG_BW__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_bw_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_GYRO_CONFIG_BW);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the
 *	data of gyro from the register 0x42 bit 4 to 5
 *
 *
 *
 *
 *  @param  v_bw_u8 : The value of gyro bandwidth
 *  value     | gyro bandwidth
 *  ----------|----------------
 *   0x00     | BMI160_GYRO_OSR4_MODE
 *   0x01     | BMI160_GYRO_OSR2_MODE
 *   0x02     | BMI160_GYRO_NORMAL_MODE
 *   0x03     | BMI160_GYRO_CIC_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_bw(u8 v_bw_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_bw_u8 <= BMI160_MAX_GYRO_BW) {
			/* write the gyro bandwidth*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_GYRO_CONFIG_BW__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_CONFIG_BW, v_bw_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_CONFIG_BW__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads the range
 *	of gyro from the register 0x43 bit 0 to 2
 *
 *  @param  v_range_u8 : The value of gyro range
 *   value    |    range
 *  ----------|-------------------------------
 *    0x00    | BMI160_GYRO_RANGE_2000_DEG_SEC
 *    0x01    | BMI160_GYRO_RANGE_1000_DEG_SEC
 *    0x02    | BMI160_GYRO_RANGE_500_DEG_SEC
 *    0x03    | BMI160_GYRO_RANGE_250_DEG_SEC
 *    0x04    | BMI160_GYRO_RANGE_125_DEG_SEC
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_range(u8 *v_range_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro range */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYRO_RANGE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_range_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_GYRO_RANGE);
		}
	return com_rslt;
}
/*!
 *	@brief This API set the range
 *	of gyro from the register 0x43 bit 0 to 2
 *
 *  @param  v_range_u8 : The value of gyro range
 *   value    |    range
 *  ----------|-------------------------------
 *    0x00    | BMI160_GYRO_RANGE_2000_DEG_SEC
 *    0x01    | BMI160_GYRO_RANGE_1000_DEG_SEC
 *    0x02    | BMI160_GYRO_RANGE_500_DEG_SEC
 *    0x03    | BMI160_GYRO_RANGE_250_DEG_SEC
 *    0x04    | BMI160_GYRO_RANGE_125_DEG_SEC
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_range(u8 v_range_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_range_u8 <= BMI160_MAX_GYRO_RANGE) {
			/* write the gyro range value */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_GYRO_RANGE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_RANGE,
				v_range_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_GYRO_RANGE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the
 *	output data rate of magnetometer from the register 0x44 bit 0 to 3
 *
 *
 *
 *
 *  @param  v_output_data_rat_u8e : The value of mag output data rate
 *  value   |    mag output data rate
 * ---------|---------------------------
 *  0x00    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED
 *  0x01    |BMI160_MAG_OUTPUT_DATA_RATE_0_78HZ
 *  0x02    |BMI160_MAG_OUTPUT_DATA_RATE_1_56HZ
 *  0x03    |BMI160_MAG_OUTPUT_DATA_RATE_3_12HZ
 *  0x04    |BMI160_MAG_OUTPUT_DATA_RATE_6_25HZ
 *  0x05    |BMI160_MAG_OUTPUT_DATA_RATE_12_5HZ
 *  0x06    |BMI160_MAG_OUTPUT_DATA_RATE_25HZ
 *  0x07    |BMI160_MAG_OUTPUT_DATA_RATE_50HZ
 *  0x08    |BMI160_MAG_OUTPUT_DATA_RATE_100HZ
 *  0x09    |BMI160_MAG_OUTPUT_DATA_RATE_200HZ
 *  0x0A    |BMI160_MAG_OUTPUT_DATA_RATE_400HZ
 *  0x0B    |BMI160_MAG_OUTPUT_DATA_RATE_800HZ
 *  0x0C    |BMI160_MAG_OUTPUT_DATA_RATE_1600HZ
 *  0x0D    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED0
 *  0x0E    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED1
 *  0x0F    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED2
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_output_data_rate(
u8 *v_output_data_rat_u8e)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the mag data output rate*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_output_data_rat_u8e = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_CONFIG_OUTPUT_DATA_RATE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set the
 *	output data rate of magnetometer from the register 0x44 bit 0 to 3
 *
 *
 *
 *
 *  @param  v_output_data_rat_u8e : The value of mag output data rate
 *  value   |    mag output data rate
 * ---------|---------------------------
 *  0x00    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED
 *  0x01    |BMI160_MAG_OUTPUT_DATA_RATE_0_78HZ
 *  0x02    |BMI160_MAG_OUTPUT_DATA_RATE_1_56HZ
 *  0x03    |BMI160_MAG_OUTPUT_DATA_RATE_3_12HZ
 *  0x04    |BMI160_MAG_OUTPUT_DATA_RATE_6_25HZ
 *  0x05    |BMI160_MAG_OUTPUT_DATA_RATE_12_5HZ
 *  0x06    |BMI160_MAG_OUTPUT_DATA_RATE_25HZ
 *  0x07    |BMI160_MAG_OUTPUT_DATA_RATE_50HZ
 *  0x08    |BMI160_MAG_OUTPUT_DATA_RATE_100HZ
 *  0x09    |BMI160_MAG_OUTPUT_DATA_RATE_200HZ
 *  0x0A    |BMI160_MAG_OUTPUT_DATA_RATE_400HZ
 *  0x0B    |BMI160_MAG_OUTPUT_DATA_RATE_800HZ
 *  0x0C    |BMI160_MAG_OUTPUT_DATA_RATE_1600HZ
 *  0x0D    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED0
 *  0x0E    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED1
 *  0x0F    |BMI160_MAG_OUTPUT_DATA_RATE_RESERVED2
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_output_data_rate(
u8 v_output_data_rat_u8e)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* select the mag data output rate*/
		if ((v_output_data_rat_u8e
		<= BMI160_MAX_ACCEL_OUTPUT_DATA_RATE)
		&& (v_output_data_rat_u8e
		!= BMI160_OUTPUT_DATA_RATE0)
		&& (v_output_data_rat_u8e
		!=  BMI160_OUTPUT_DATA_RATE6)
		&& (v_output_data_rat_u8e
		!=  BMI160_OUTPUT_DATA_RATE7)) {
			/* write the mag data output rate*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_CONFIG_OUTPUT_DATA_RATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_MAG_CONFIG_OUTPUT_DATA_RATE,
				v_output_data_rat_u8e);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_MAG_CONFIG_OUTPUT_DATA_RATE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API is used to read Down sampling
 *	for gyro (2**downs_gyro) in the register 0x45 bit 0 to 2
 *
 *
 *
 *
 *  @param v_fifo_down_gyro_u8 :The value of gyro fifo down
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_down_gyro(
u8 *v_fifo_down_gyro_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro fifo down*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWN_GYRO__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_down_gyro_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_DOWN_GYRO);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to set Down sampling
 *	for gyro (2**downs_gyro) in the register 0x45 bit 0 to 2
 *
 *
 *
 *
 *  @param v_fifo_down_gyro_u8 :The value of gyro fifo down
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_down_gyro(
u8 v_fifo_down_gyro_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the gyro fifo down*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWN_GYRO__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(
				v_data_u8,
				BMI160_USER_FIFO_DOWN_GYRO,
				v_fifo_down_gyro_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_DOWN_GYRO__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read gyro fifo filter data
 *	from the register 0x45 bit 3
 *
 *
 *
 *  @param v_gyro_fifo_filter_data_u8 :The value of gyro filter data
 *  value      |  gyro_fifo_filter_data
 * ------------|-------------------------
 *    0x00     |  Unfiltered data
 *    0x01     |  Filtered data
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_fifo_filter_data(
u8 *v_gyro_fifo_filter_data_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro fifo filter data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILTER_GYRO__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_fifo_filter_data_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_FILTER_GYRO);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set gyro fifo filter data
 *	from the register 0x45 bit 3
 *
 *
 *
 *  @param v_gyro_fifo_filter_data_u8 :The value of gyro filter data
 *  value      |  gyro_fifo_filter_data
 * ------------|-------------------------
 *    0x00     |  Unfiltered data
 *    0x01     |  Filtered data
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_fifo_filter_data(
u8 v_gyro_fifo_filter_data_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_fifo_filter_data_u8
		<= BMI160_MAX_VALUE_FIFO_FILTER) {
			/* write the gyro fifo filter data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILTER_GYRO__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(
				v_data_u8,
				BMI160_USER_FIFO_FILTER_GYRO,
				v_gyro_fifo_filter_data_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_FILTER_GYRO__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to read Down sampling
 *	for accel (2*downs_accel) from the register 0x45 bit 4 to 6
 *
 *
 *
 *
 *  @param v_fifo_down_u8 :The value of accel fifo down
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_down_accel(
u8 *v_fifo_down_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel fifo down data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWN_ACCEL__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_down_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_DOWN_ACCEL);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to set Down sampling
 *	for accel (2*downs_accel) from the register 0x45 bit 4 to 6
 *
 *
 *
 *
 *  @param v_fifo_down_u8 :The value of accel fifo down
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_down_accel(
u8 v_fifo_down_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the accel fifo down data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_DOWN_ACCEL__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_DOWN_ACCEL, v_fifo_down_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_DOWN_ACCEL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read accel fifo filter data
 *	from the register 0x45 bit 7
 *
 *
 *
 *  @param v_accel_fifo_filter_u8 :The value of accel filter data
 *  value      |  accel_fifo_filter_data
 * ------------|-------------------------
 *    0x00     |  Unfiltered data
 *    0x01     |  Filtered data
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_fifo_filter_data(
u8 *v_accel_fifo_filter_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel fifo filter data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILTER_ACCEL__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_fifo_filter_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_FILTER_ACCEL);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set accel fifo filter data
 *	from the register 0x45 bit 7
 *
 *
 *
 *  @param v_accel_fifo_filter_u8 :The value of accel filter data
 *  value      |  accel_fifo_filter_data
 * ------------|-------------------------
 *    0x00     |  Unfiltered data
 *    0x01     |  Filtered data
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_fifo_filter_data(
u8 v_accel_fifo_filter_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_accel_fifo_filter_u8 <= BMI160_MAX_VALUE_FIFO_FILTER) {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_FILTER_ACCEL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				/* write accel fifo filter data */
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_FILTER_ACCEL,
				v_accel_fifo_filter_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_FILTER_ACCEL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to Trigger an interrupt
 *	when FIFO contains water mark level from the register 0x46 bit 0 to 7
 *
 *
 *
 *  @param  v_fifo_wm_u8 : The value of fifo water mark level
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_wm(
u8 *v_fifo_wm_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the fifo water mark level*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_WM__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_wm_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_WM);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to Trigger an interrupt
 *	when FIFO contains water mark level from the register 0x46 bit 0 to 7
 *
 *
 *
 *  @param  v_fifo_wm_u8 : The value of fifo water mark level
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_wm(
u8 v_fifo_wm_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the fifo water mark level*/
			com_rslt =
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_WM__REG,
			&v_fifo_wm_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief This API reads fifo sensor time
 *	frame after the last valid data frame form the register  0x47 bit 1
 *
 *
 *
 *
 *  @param v_fifo_time_enable_u8 : The value of sensor time
 *  value      |  fifo sensor time
 * ------------|-------------------------
 *    0x00     |  do not return sensortime frame
 *    0x01     |  return sensortime frame
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_time_enable(
u8 *v_fifo_time_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the fifo sensor time*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TIME_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_time_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_TIME_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API set fifo sensor time
 *	frame after the last valid data frame form the register  0x47 bit 1
 *
 *
 *
 *
 *  @param v_fifo_time_enable_u8 : The value of sensor time
 *  value      |  fifo sensor time
 * ------------|-------------------------
 *    0x00     |  do not return sensortime frame
 *    0x01     |  return sensortime frame
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_time_enable(
u8 v_fifo_time_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_time_enable_u8 <= BMI160_MAX_VALUE_FIFO_TIME) {
			/* write the fifo sensor time*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TIME_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_TIME_ENABLE,
				v_fifo_time_enable_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TIME_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads FIFO tag interrupt2 enable status
 *	from the resister 0x47 bit 2
 *
 *  @param v_fifo_tag_intr2_u8 : The value of fifo tag interrupt
 *	value    | fifo tag interrupt
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_intr2_enable(
u8 *v_fifo_tag_intr2_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the fifo tag interrupt2*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INTR2_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_tag_intr2_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_TAG_INTR2_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API set FIFO tag interrupt2 enable status
 *	from the resister 0x47 bit 2
 *
 *  @param v_fifo_tag_intr2_u8 : The value of fifo tag interrupt
 *	value    | fifo tag interrupt
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_intr2_enable(
u8 v_fifo_tag_intr2_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_tag_intr2_u8 <= BMI160_MAX_VALUE_FIFO_INTR) {
			/* write the fifo tag interrupt2*/
			com_rslt = bmi160_set_input_enable(1,
			v_fifo_tag_intr2_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INTR2_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_TAG_INTR2_ENABLE,
				v_fifo_tag_intr2_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TAG_INTR2_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API get FIFO tag interrupt1 enable status
 *	from the resister 0x47 bit 3
 *
 *  @param v_fifo_tag_intr1_u8 :The value of fifo tag interrupt1
 *	value    | fifo tag interrupt
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_tag_intr1_enable(
u8 *v_fifo_tag_intr1_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read fifo tag interrupt*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INTR1_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_tag_intr1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_TAG_INTR1_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API set FIFO tag interrupt1 enable status
 *	from the resister 0x47 bit 3
 *
 *  @param v_fifo_tag_intr1_u8 :The value of fifo tag interrupt1
 *	value    | fifo tag interrupt
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_tag_intr1_enable(
u8 v_fifo_tag_intr1_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_tag_intr1_u8 <= BMI160_MAX_VALUE_FIFO_INTR) {
			/* write the fifo tag interrupt*/
			com_rslt = bmi160_set_input_enable(BMI160_INIT_VALUE,
			v_fifo_tag_intr1_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_TAG_INTR1_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_TAG_INTR1_ENABLE,
				v_fifo_tag_intr1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_TAG_INTR1_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads FIFO frame
 *	header enable from the register 0x47 bit 4
 *
 *  @param v_fifo_header_u8 :The value of fifo header
 *	value    | fifo header
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_header_enable(
u8 *v_fifo_header_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read fifo header */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_HEADER_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_header_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_HEADER_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API set FIFO frame
 *	header enable from the register 0x47 bit 4
 *
 *  @param v_fifo_header_u8 :The value of fifo header
 *	value    | fifo header
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_header_enable(
u8 v_fifo_header_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_header_u8 <= BMI160_MAX_VALUE_FIFO_HEADER) {
			/* write the fifo header */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_HEADER_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_HEADER_ENABLE,
				v_fifo_header_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_HEADER_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to read stored
 *	magnetometer data in FIFO (all 3 axes) from the register 0x47 bit 5
 *
 *  @param v_fifo_mag_u8 : The value of fifo mag enble
 *	value    | fifo mag
 * ----------|-------------------
 *  0x00     |  no magnetometer data is stored
 *  0x01     |  magnetometer data is stored
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_mag_enable(
u8 *v_fifo_mag_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the fifo mag enable*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_MAG_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_mag_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_MAG_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set stored
 *	magnetometer data in FIFO (all 3 axes) from the register 0x47 bit 5
 *
 *  @param v_fifo_mag_u8 : The value of fifo mag enble
 *	value    | fifo mag
 * ----------|-------------------
 *  0x00     |  no magnetometer data is stored
 *  0x01     |  magnetometer data is stored
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_mag_enable(
u8 v_fifo_mag_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			if (v_fifo_mag_u8 <= BMI160_MAX_VALUE_FIFO_MAG) {
				/* write the fifo mag enable*/
				com_rslt =
				p_bmi160->BMI160_BUS_READ_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FIFO_MAG_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
				if (com_rslt == SUCCESS) {
					v_data_u8 =
					BMI160_SET_BITSLICE(v_data_u8,
					BMI160_USER_FIFO_MAG_ENABLE,
					v_fifo_mag_u8);
					com_rslt +=
					p_bmi160->BMI160_BUS_WRITE_FUNC
					(p_bmi160->dev_addr,
					BMI160_USER_FIFO_MAG_ENABLE__REG,
					&v_data_u8,
					BMI160_GEN_READ_WRITE_DATA_LENGTH);
				}
			} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read stored
 *	accel data in FIFO (all 3 axes) from the register 0x47 bit 6
 *
 *  @param v_fifo_accel_u8 : The value of fifo accel enble
 *	value    | fifo accel
 * ----------|-------------------
 *  0x00     |  no accel data is stored
 *  0x01     |  accel data is stored
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_accel_enable(
u8 *v_fifo_accel_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel fifo enable*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_ACCEL_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_accel_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_ACCEL_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set stored
 *	accel data in FIFO (all 3 axes) from the register 0x47 bit 6
 *
 *  @param v_fifo_accel_u8 : The value of fifo accel enble
 *	value    | fifo accel
 * ----------|-------------------
 *  0x00     |  no accel data is stored
 *  0x01     |  accel data is stored
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_accel_enable(
u8 v_fifo_accel_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_accel_u8 <= BMI160_MAX_VALUE_FIFO_ACCEL) {
			/* write the fifo mag enables*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_ACCEL_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_ACCEL_ENABLE, v_fifo_accel_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_ACCEL_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to read stored
 *	 gyro data in FIFO (all 3 axes) from the resister 0x47 bit 7
 *
 *
 *  @param v_fifo_gyro_u8 : The value of fifo gyro enble
 *	value    | fifo gyro
 * ----------|-------------------
 *  0x00     |  no gyro data is stored
 *  0x01     |  gyro data is stored
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_fifo_gyro_enable(
u8 *v_fifo_gyro_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read fifo gyro enable */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_GYRO_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_fifo_gyro_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FIFO_GYRO_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set stored
 *	gyro data in FIFO (all 3 axes) from the resister 0x47 bit 7
 *
 *
 *  @param v_fifo_gyro_u8 : The value of fifo gyro enble
 *	value    | fifo gyro
 * ----------|-------------------
 *  0x00     |  no gyro data is stored
 *  0x01     |  gyro data is stored
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_fifo_gyro_enable(
u8 v_fifo_gyro_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_fifo_gyro_u8 <= BMI160_MAX_VALUE_FIFO_GYRO) {
			/* write fifo gyro enable*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_FIFO_GYRO_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FIFO_GYRO_ENABLE, v_fifo_gyro_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FIFO_GYRO_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to read
 *	I2C device address of auxiliary mag from the register 0x4B bit 1 to 7
 *
 *
 *
 *
 *  @param v_i2c_device_addr_u8 : The value of mag I2C device address
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_device_addr(
u8 *v_i2c_device_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the mag I2C device address*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_DEVICE_ADDR__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_i2c_device_addr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_I2C_DEVICE_ADDR);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	I2C device address of auxiliary mag from the register 0x4B bit 1 to 7
 *
 *
 *
 *
 *  @param v_i2c_device_addr_u8 : The value of mag I2C device address
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_device_addr(
u8 v_i2c_device_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the mag I2C device address*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_I2C_DEVICE_ADDR__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_I2C_DEVICE_ADDR,
				v_i2c_device_addr_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_I2C_DEVICE_ADDR__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read
 *	Burst data length (1,2,6,8 byte) from the register 0x4C bit 0 to 1
 *
 *
 *
 *
 *  @param v_mag_burst_u8 : The data of mag burst read lenth
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_burst(
u8 *v_mag_burst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read mag burst mode length*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_BURST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_burst_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_BURST);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	Burst data length (1,2,6,8 byte) from the register 0x4C bit 0 to 1
 *
 *
 *
 *
 *  @param v_mag_burst_u8 : The data of mag burst read lenth
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_burst(
u8 v_mag_burst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write mag burst mode length*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_BURST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_MAG_BURST, v_mag_burst_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_MAG_BURST__REG, &v_data_u8,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read
 *	trigger-readout offset in units of 2.5 ms. If set to zero,
 *	the offset is maximum, i.e. after readout a trigger
 *	is issued immediately. from the register 0x4C bit 2 to 5
 *
 *
 *
 *
 *  @param v_mag_offset_u8 : The value of mag offset
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_offset(
u8 *v_mag_offset_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_OFFSET__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_offset_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_OFFSET);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	trigger-readout offset in units of 2.5 ms. If set to zero,
 *	the offset is maximum, i.e. after readout a trigger
 *	is issued immediately. from the register 0x4C bit 2 to 5
 *
 *
 *
 *
 *  @param v_mag_offset_u8 : The value of mag offset
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_offset(
u8 v_mag_offset_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_MAG_OFFSET__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_OFFSET, v_mag_offset_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_OFFSET__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	}
return com_rslt;
}
/*!
 *	@brief This API is used to read
 *	Enable register access on MAG_IF[2] or MAG_IF[3] writes.
 *	This implies that the DATA registers are not updated with
 *	magnetometer values. Accessing magnetometer requires
 *	the magnetometer in normal mode in PMU_STATUS.
 *	from the register 0x4C bit 7
 *
 *
 *
 *  @param v_mag_manual_u8 : The value of mag manual enable
 *	value    | mag manual
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_manual_enable(
u8 *v_mag_manual_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read mag manual */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_MANUAL_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_manual_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_MANUAL_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	Enable register access on MAG_IF[2] or MAG_IF[3] writes.
 *	This implies that the DATA registers are not updated with
 *	magnetometer values. Accessing magnetometer requires
 *	the magnetometer in normal mode in PMU_STATUS.
 *	from the register 0x4C bit 7
 *
 *
 *
 *  @param v_mag_manual_u8 : The value of mag manual enable
 *	value    | mag manual
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_manual_enable(
u8 v_mag_manual_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* write the mag manual*/
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_MAG_MANUAL_ENABLE__REG, &v_data_u8,
		BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		if (com_rslt == SUCCESS) {
			/* set the bit of mag manual enable*/
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_MAG_MANUAL_ENABLE, v_mag_manual_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_MAG_MANUAL_ENABLE__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		if (com_rslt == SUCCESS)
			p_bmi160->mag_manual_enable = v_mag_manual_u8;
		else
			p_bmi160->mag_manual_enable = E_BMI160_COMM_RES;
	}
return com_rslt;
}
/*!
 *	@brief This API is used to read data
 *	magnetometer address to read from the register 0x4D bit 0 to 7
 *	@brief It used to provide mag read address of auxiliary mag
 *
 *
 *
 *
 *  @param  v_mag_read_addr_u8 : The value of address need to be read
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_read_addr(
u8 *v_mag_read_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the written address*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_READ_ADDR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_read_addr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_READ_ADDR);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	magnetometer write address from the register 0x4D bit 0 to 7
 *	@brief mag write address writes the address of auxiliary mag to write
 *
 *
 *
 *  @param v_mag_read_addr_u8:
 *	The data of auxiliary mag address to write data
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_read_addr(
u8 v_mag_read_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the mag read address*/
			com_rslt =
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_READ_ADDR__REG, &v_mag_read_addr_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read
 *	magnetometer write address from the register 0x4E bit 0 to 7
 *	@brief mag write address writes the address of auxiliary mag to write
 *
 *
 *
 *  @param  v_mag_write_addr_u8:
 *	The data of auxiliary mag address to write data
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_addr(
u8 *v_mag_write_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the address of last written */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_ADDR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_write_addr_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_WRITE_ADDR);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set
 *	magnetometer write address from the register 0x4E bit 0 to 7
 *	@brief mag write address writes the address of auxiliary mag to write
 *
 *
 *
 *  @param  v_mag_write_addr_u8:
 *	The data of auxiliary mag address to write data
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_addr(
u8 v_mag_write_addr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the data of mag address to write data */
			com_rslt =
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_ADDR__REG, &v_mag_write_addr_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read magnetometer write data
 *	form the resister 0x4F bit 0 to 7
 *	@brief This writes the data will be wrote to mag
 *
 *
 *
 *  @param  v_mag_write_data_u8: The value of mag data
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_mag_write_data(
u8 *v_mag_write_data_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_DATA__REG, &v_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_mag_write_data_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_WRITE_DATA);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to set magnetometer write data
 *	form the resister 0x4F bit 0 to 7
 *	@brief This writes the data will be wrote to mag
 *
 *
 *
 *  @param  v_mag_write_data_u8: The value of mag data
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_mag_write_data(
u8 v_mag_write_data_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt =
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
			BMI160_USER_WRITE_DATA__REG, &v_mag_write_data_u8,
			BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief  This API is used to read
 *	interrupt enable from the register 0x50 bit 0 to 7
 *
 *
 *
 *
 *	@param v_enable_u8 : Value to decided to select interrupt
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_ANY_MOTION_X_ENABLE
 *       1         | BMI160_ANY_MOTION_Y_ENABLE
 *       2         | BMI160_ANY_MOTION_Z_ENABLE
 *       3         | BMI160_DOUBLE_TAP_ENABLE
 *       4         | BMI160_SINGLE_TAP_ENABLE
 *       5         | BMI160_ORIENT_ENABLE
 *       6         | BMI160_FLAT_ENABLE
 *
 *	@param v_intr_enable_zero_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_enable_0(
u8 v_enable_u8, u8 *v_intr_enable_zero_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* select interrupt to read*/
		switch (v_enable_u8) {
		case BMI160_ANY_MOTION_X_ENABLE:
			/* read the any motion interrupt x data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE);
		break;
		case BMI160_ANY_MOTION_Y_ENABLE:
			/* read the any motion interrupt y data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE);
		break;
		case BMI160_ANY_MOTION_Z_ENABLE:
			/* read the any motion interrupt z data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE);
		break;
		case BMI160_DOUBLE_TAP_ENABLE:
			/* read the double tap interrupt data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE);
		break;
		case BMI160_SINGLE_TAP_ENABLE:
			/* read the single tap interrupt data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE);
		break;
		case BMI160_ORIENT_ENABLE:
			/* read the orient interrupt data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE);
		break;
		case BMI160_FLAT_ENABLE:
			/* read the flat interrupt data */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_zero_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE);
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  This API is used to set
 *	interrupt enable from the register 0x50 bit 0 to 7
 *
 *
 *
 *
 *	@param v_enable_u8 : Value to decided to select interrupt
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_ANY_MOTION_X_ENABLE
 *       1         | BMI160_ANY_MOTION_Y_ENABLE
 *       2         | BMI160_ANY_MOTION_Z_ENABLE
 *       3         | BMI160_DOUBLE_TAP_ENABLE
 *       4         | BMI160_SINGLE_TAP_ENABLE
 *       5         | BMI160_ORIENT_ENABLE
 *       6         | BMI160_FLAT_ENABLE
 *
 *	@param v_intr_enable_zero_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_enable_0(
u8 v_enable_u8, u8 v_intr_enable_zero_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_enable_u8) {
	case BMI160_ANY_MOTION_X_ENABLE:
		/* write any motion x*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_ANY_MOTION_Y_ENABLE:
		/* write any motion y*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_ANY_MOTION_Z_ENABLE:
		/* write any motion z*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ANY_MOTION_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_DOUBLE_TAP_ENABLE:
		/* write double tap*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_DOUBLE_TAP_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_SINGLE_TAP_ENABLE:
		/* write single tap */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_SINGLE_TAP_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_ORIENT_ENABLE:
		/* write orient interrupt*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_ORIENT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_FLAT_ENABLE:
		/* write flat interrupt*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE,
			v_intr_enable_zero_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_0_FLAT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
/*!
 *	@brief  This API is used to read
 *	interrupt enable byte1 from the register 0x51 bit 0 to 6
 *	@brief It read the high_g_x,high_g_y,high_g_z,low_g_enable
 *	data ready, fifo full and fifo water mark.
 *
 *
 *
 *  @param  v_enable_u8 :  The value of interrupt enable
 *	@param v_enable_u8 : Value to decided to select interrupt
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_HIGH_G_X_ENABLE
 *       1         | BMI160_HIGH_G_Y_ENABLE
 *       2         | BMI160_HIGH_G_Z_ENABLE
 *       3         | BMI160_LOW_G_ENABLE
 *       4         | BMI160_DATA_RDY_ENABLE
 *       5         | BMI160_FIFO_FULL_ENABLE
 *       6         | BMI160_FIFO_WM_ENABLE
 *
 *	@param v_intr_enable_1_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_enable_1(
u8 v_enable_u8, u8 *v_intr_enable_1_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_enable_u8) {
		case BMI160_HIGH_G_X_ENABLE:
			/* read high_g_x interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_X_ENABLE);
			break;
		case BMI160_HIGH_G_Y_ENABLE:
			/* read high_g_y interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Y_ENABLE);
			break;
		case BMI160_HIGH_G_Z_ENABLE:
			/* read high_g_z interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Z_ENABLE);
			break;
		case BMI160_LOW_G_ENABLE:
			/* read low_g interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE);
			break;
		case BMI160_DATA_RDY_ENABLE:
			/* read data ready interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_DATA_RDY_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_DATA_RDY_ENABLE);
			break;
		case BMI160_FIFO_FULL_ENABLE:
			/* read fifo full interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_FIFO_FULL_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_FIFO_FULL_ENABLE);
			break;
		case BMI160_FIFO_WM_ENABLE:
			/* read fifo water mark interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_1_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  This API is used to set
 *	interrupt enable byte1 from the register 0x51 bit 0 to 6
 *	@brief It read the high_g_x,high_g_y,high_g_z,low_g_enable
 *	data ready, fifo full and fifo water mark.
 *
 *
 *
 *  @param  v_enable_u8 :  The value of interrupt enable
 *	@param v_enable_u8 : Value to decided to select interrupt
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_HIGH_G_X_ENABLE
 *       1         | BMI160_HIGH_G_Y_ENABLE
 *       2         | BMI160_HIGH_G_Z_ENABLE
 *       3         | BMI160_LOW_G_ENABLE
 *       4         | BMI160_DATA_RDY_ENABLE
 *       5         | BMI160_FIFO_FULL_ENABLE
 *       6         | BMI160_FIFO_WM_ENABLE
 *
 *	@param v_intr_enable_1_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_enable_1(
u8 v_enable_u8, u8 v_intr_enable_1_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_enable_u8) {
		case BMI160_HIGH_G_X_ENABLE:
			/* write high_g_x interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_X_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_X_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_HIGH_G_Y_ENABLE:
			/* write high_g_y interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_Y_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_Y_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_HIGH_G_Z_ENABLE:
			/* write high_g_z interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_HIGH_G_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_Z_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_HIGH_G_Z_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_LOW_G_ENABLE:
			/* write low_g interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_DATA_RDY_ENABLE:
			/* write data ready interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_DATA_RDY_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_DATA_RDY_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_DATA_RDY_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_FIFO_FULL_ENABLE:
			/* write fifo full interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_1_FIFO_FULL_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_FIFO_FULL_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_FIFO_FULL_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_FIFO_WM_ENABLE:
			/* write fifo water mark interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE,
				v_intr_enable_1_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_ENABLE_1_FIFO_WM_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  This API is used to read
 *	interrupt enable byte2 from the register bit 0x52 bit 0 to 3
 *	@brief It reads no motion x,y and z
 *
 *
 *
 *	@param v_enable_u8: The value of interrupt enable
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_NOMOTION_X_ENABLE
 *       1         | BMI160_NOMOTION_Y_ENABLE
 *       2         | BMI160_NOMOTION_Z_ENABLE
 *
 *	@param v_intr_enable_2_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_enable_2(
u8 v_enable_u8, u8 *v_intr_enable_2_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_enable_u8) {
		case BMI160_NOMOTION_X_ENABLE:
			/* read no motion x */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_2_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_X_ENABLE);
			break;
		case BMI160_NOMOTION_Y_ENABLE:
			/* read no motion y */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_2_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Y_ENABLE);
			break;
		case BMI160_NOMOTION_Z_ENABLE:
			/* read no motion z */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_enable_2_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Z_ENABLE);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  This API is used to set
 *	interrupt enable byte2 from the register bit 0x52 bit 0 to 3
 *	@brief It reads no motion x,y and z
 *
 *
 *
 *	@param v_enable_u8: The value of interrupt enable
 *   v_enable_u8   |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_NOMOTION_X_ENABLE
 *       1         | BMI160_NOMOTION_Y_ENABLE
 *       2         | BMI160_NOMOTION_Z_ENABLE
 *
 *	@param v_intr_enable_2_u8 : The interrupt enable value
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_enable_2(
u8 v_enable_u8, u8 v_intr_enable_2_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_enable_u8) {
	case BMI160_NOMOTION_X_ENABLE:
		/* write no motion x */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr,
		BMI160_USER_INTR_ENABLE_2_NOMOTION_X_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_X_ENABLE,
			v_intr_enable_2_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_X_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_NOMOTION_Y_ENABLE:
		/* write no motion y */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr,
		BMI160_USER_INTR_ENABLE_2_NOMOTION_Y_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Y_ENABLE,
			v_intr_enable_2_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Y_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_NOMOTION_Z_ENABLE:
		/* write no motion z */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr,
		BMI160_USER_INTR_ENABLE_2_NOMOTION_Z_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Z_ENABLE,
			v_intr_enable_2_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_NOMOTION_Z_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API is used to read
 *	interrupt enable step detector interrupt from
 *	the register bit 0x52 bit 3
 *
 *
 *
 *
 *	@param v_step_intr_u8 : The value of step detector interrupt enable
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_step_detector_enable(
u8 *v_step_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the step detector interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_step_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to set
 *	interrupt enable step detector interrupt from
 *	the register bit 0x52 bit 3
 *
 *
 *
 *
 *	@param v_step_intr_u8 : The value of step detector interrupt enable
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_detector_enable(
u8 v_step_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr,
		BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE,
			v_step_intr_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr,
			BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	}
	return com_rslt;
}
/*!
 *	@brief  Configure trigger condition of interrupt1
 *	and interrupt2 pin from the register 0x53
 *	@brief interrupt1 - bit 0
 *	@brief interrupt2 - bit 4
 *
 *  @param v_channel_u8: The value of edge trigger selection
 *   v_channel_u8  |   Edge trigger
 *  ---------------|---------------
 *       0         | BMI160_INTR1_EDGE_CTRL
 *       1         | BMI160_INTR2_EDGE_CTRL
 *
 *	@param v_intr_edge_ctrl_u8 : The value of edge trigger enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_EDGE
 *  0x00     |  BMI160_LEVEL
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_edge_ctrl(
u8 v_channel_u8, u8 *v_intr_edge_ctrl_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_EDGE_CTRL:
			/* read the edge trigger interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_EDGE_CTRL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_edge_ctrl_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_EDGE_CTRL);
			break;
		case BMI160_INTR2_EDGE_CTRL:
			/* read the edge trigger interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_EDGE_CTRL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_edge_ctrl_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_EDGE_CTRL);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  Configure trigger condition of interrupt1
 *	and interrupt2 pin from the register 0x53
 *	@brief interrupt1 - bit 0
 *	@brief interrupt2 - bit 4
 *
 *  @param v_channel_u8: The value of edge trigger selection
 *   v_channel_u8  |   Edge trigger
 *  ---------------|---------------
 *       0         | BMI160_INTR1_EDGE_CTRL
 *       1         | BMI160_INTR2_EDGE_CTRL
 *
 *	@param v_intr_edge_ctrl_u8 : The value of edge trigger enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_EDGE
 *  0x00     |  BMI160_LEVEL
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_edge_ctrl(
u8 v_channel_u8, u8 v_intr_edge_ctrl_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_EDGE_CTRL:
			/* write the edge trigger interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_EDGE_CTRL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR1_EDGE_CTRL,
				v_intr_edge_ctrl_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR1_EDGE_CTRL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_INTR2_EDGE_CTRL:
			/* write the edge trigger interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_EDGE_CTRL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR2_EDGE_CTRL,
				v_intr_edge_ctrl_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR2_EDGE_CTRL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  API used for get the Configure level condition of interrupt1
 *	and interrupt2 pin form the register 0x53
 *	@brief interrupt1 - bit 1
 *	@brief interrupt2 - bit 5
 *
 *  @param v_channel_u8: The value of level condition selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_LEVEL
 *       1         | BMI160_INTR2_LEVEL
 *
 *	@param v_intr_level_u8 : The value of level of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_LEVEL_HIGH
 *  0x00     |  BMI160_LEVEL_LOW
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_level(
u8 v_channel_u8, u8 *v_intr_level_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_LEVEL:
			/* read the interrupt1 level*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_LEVEL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_level_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_LEVEL);
			break;
		case BMI160_INTR2_LEVEL:
			/* read the interrupt2 level*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_LEVEL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_level_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_LEVEL);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  API used for set the Configure level condition of interrupt1
 *	and interrupt2 pin form the register 0x53
 *	@brief interrupt1 - bit 1
 *	@brief interrupt2 - bit 5
 *
 *  @param v_channel_u8: The value of level condition selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_LEVEL
 *       1         | BMI160_INTR2_LEVEL
 *
 *	@param v_intr_level_u8 : The value of level of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_LEVEL_HIGH
 *  0x00     |  BMI160_LEVEL_LOW
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_level(
u8 v_channel_u8, u8 v_intr_level_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_LEVEL:
			/* write the interrupt1 level*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_LEVEL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR1_LEVEL, v_intr_level_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR1_LEVEL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_INTR2_LEVEL:
			/* write the interrupt2 level*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_LEVEL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR2_LEVEL, v_intr_level_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR2_LEVEL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  API used to get configured output enable of interrupt1
 *	and interrupt2 from the register 0x53
 *	@brief interrupt1 - bit 2
 *	@brief interrupt2 - bit 6
 *
 *
 *  @param v_channel_u8: The value of output type enable selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_OUTPUT_TYPE
 *       1         | BMI160_INTR2_OUTPUT_TYPE
 *
 *	@param v_intr_output_type_u8 :
 *	The value of output type of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_OPEN_DRAIN
 *  0x00     |  BMI160_PUSH_PULL
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_output_type(
u8 v_channel_u8, u8 *v_intr_output_type_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_OUTPUT_TYPE:
			/* read the output type of interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_OUTPUT_TYPE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_output_type_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_OUTPUT_TYPE);
			break;
		case BMI160_INTR2_OUTPUT_TYPE:
			/* read the output type of interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_OUTPUT_TYPE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_output_type_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_OUTPUT_TYPE);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief  API used to set output enable of interrupt1
 *	and interrupt2 from the register 0x53
 *	@brief interrupt1 - bit 2
 *	@brief interrupt2 - bit 6
 *
 *
 *  @param v_channel_u8: The value of output type enable selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_OUTPUT_TYPE
 *       1         | BMI160_INTR2_OUTPUT_TYPE
 *
 *	@param v_intr_output_type_u8 :
 *	The value of output type of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_OPEN_DRAIN
 *  0x00     |  BMI160_PUSH_PULL
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_output_type(
u8 v_channel_u8, u8 v_intr_output_type_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_OUTPUT_TYPE:
			/* write the output type of interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_OUTPUT_TYPE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR1_OUTPUT_TYPE,
				v_intr_output_type_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR1_OUTPUT_TYPE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_INTR2_OUTPUT_TYPE:
			/* write the output type of interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_OUTPUT_TYPE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR2_OUTPUT_TYPE,
				v_intr_output_type_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR2_OUTPUT_TYPE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief API used to get the Output enable for interrupt1
 *	and interrupt1 pin from the register 0x53
 *	@brief interrupt1 - bit 3
 *	@brief interrupt2 - bit 7
 *
 *  @param v_channel_u8: The value of output enable selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_OUTPUT_TYPE
 *       1         | BMI160_INTR2_OUTPUT_TYPE
 *
 *	@param v_output_enable_u8 :
 *	The value of output enable of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_INPUT
 *  0x00     |  BMI160_OUTPUT
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_output_enable(
u8 v_channel_u8, u8 *v_output_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_OUTPUT_ENABLE:
			/* read the output enable of interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_OUTPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_output_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_OUTPUT_ENABLE);
			break;
		case BMI160_INTR2_OUTPUT_ENABLE:
			/* read the output enable of interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_OUTPUT_EN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_output_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_OUTPUT_EN);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief API used to set the Output enable for interrupt1
 *	and interrupt1 pin from the register 0x53
 *	@brief interrupt1 - bit 3
 *	@brief interrupt2 - bit 7
 *
 *  @param v_channel_u8: The value of output enable selection
 *   v_channel_u8  |   level selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_OUTPUT_TYPE
 *       1         | BMI160_INTR2_OUTPUT_TYPE
 *
 *	@param v_output_enable_u8 :
 *	The value of output enable of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_INPUT
 *  0x00     |  BMI160_OUTPUT
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_output_enable(
u8 v_channel_u8, u8 v_output_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_OUTPUT_ENABLE:
			/* write the output enable of interrupt1*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_OUTPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR1_OUTPUT_ENABLE,
				v_output_enable_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR1_OUTPUT_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_INTR2_OUTPUT_ENABLE:
			/* write the output enable of interrupt2*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_OUTPUT_EN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR2_OUTPUT_EN,
				v_output_enable_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR2_OUTPUT_EN__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
*	@brief This API is used to get the latch duration
*	from the register 0x54 bit 0 to 3
*	@brief This latch selection is not applicable for data ready,
*	orientation and flat interrupts.
*
*
*
*  @param v_latch_intr_u8 : The value of latch duration
*	Latch Duration                      |     value
* --------------------------------------|------------------
*    BMI160_LATCH_DUR_NONE              |      0x00
*    BMI160_LATCH_DUR_312_5_MICRO_SEC   |      0x01
*    BMI160_LATCH_DUR_625_MICRO_SEC     |      0x02
*    BMI160_LATCH_DUR_1_25_MILLI_SEC    |      0x03
*    BMI160_LATCH_DUR_2_5_MILLI_SEC     |      0x04
*    BMI160_LATCH_DUR_5_MILLI_SEC       |      0x05
*    BMI160_LATCH_DUR_10_MILLI_SEC      |      0x06
*    BMI160_LATCH_DUR_20_MILLI_SEC      |      0x07
*    BMI160_LATCH_DUR_40_MILLI_SEC      |      0x08
*    BMI160_LATCH_DUR_80_MILLI_SEC      |      0x09
*    BMI160_LATCH_DUR_160_MILLI_SEC     |      0x0A
*    BMI160_LATCH_DUR_320_MILLI_SEC     |      0x0B
*    BMI160_LATCH_DUR_640_MILLI_SEC     |      0x0C
*    BMI160_LATCH_DUR_1_28_SEC          |      0x0D
*    BMI160_LATCH_DUR_2_56_SEC          |      0x0E
*    BMI160_LATCHED                     |      0x0F
*
*
*
*	@return results of bus communication function
*	@retval 0 -> Success
*	@retval -1 -> Error
*
*
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_latch_intr(
u8 *v_latch_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the latch duration value */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_LATCH__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_latch_intr_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LATCH);
		}
	return com_rslt;
}
/*!
*	@brief This API is used to set the latch duration
*	from the register 0x54 bit 0 to 3
*	@brief This latch selection is not applicable for data ready,
*	orientation and flat interrupts.
*
*
*
*  @param v_latch_intr_u8 : The value of latch duration
*	Latch Duration                      |     value
* --------------------------------------|------------------
*    BMI160_LATCH_DUR_NONE              |      0x00
*    BMI160_LATCH_DUR_312_5_MICRO_SEC   |      0x01
*    BMI160_LATCH_DUR_625_MICRO_SEC     |      0x02
*    BMI160_LATCH_DUR_1_25_MILLI_SEC    |      0x03
*    BMI160_LATCH_DUR_2_5_MILLI_SEC     |      0x04
*    BMI160_LATCH_DUR_5_MILLI_SEC       |      0x05
*    BMI160_LATCH_DUR_10_MILLI_SEC      |      0x06
*    BMI160_LATCH_DUR_20_MILLI_SEC      |      0x07
*    BMI160_LATCH_DUR_40_MILLI_SEC      |      0x08
*    BMI160_LATCH_DUR_80_MILLI_SEC      |      0x09
*    BMI160_LATCH_DUR_160_MILLI_SEC     |      0x0A
*    BMI160_LATCH_DUR_320_MILLI_SEC     |      0x0B
*    BMI160_LATCH_DUR_640_MILLI_SEC     |      0x0C
*    BMI160_LATCH_DUR_1_28_SEC          |      0x0D
*    BMI160_LATCH_DUR_2_56_SEC          |      0x0E
*    BMI160_LATCHED                     |      0x0F
*
*
*
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*
*
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_latch_intr(u8 v_latch_intr_u8)
{
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_latch_intr_u8 <= BMI160_MAX_LATCH_INTR) {
			/* write the latch duration value */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_LATCH__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_LATCH, v_latch_intr_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr, BMI160_USER_INTR_LATCH__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief API used to get input enable for interrupt1
 *	and interrupt2 pin from the register 0x54
 *	@brief interrupt1 - bit 4
 *	@brief interrupt2 - bit 5
 *
 *  @param v_channel_u8: The value of input enable selection
 *   v_channel_u8  |   input selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_INPUT_ENABLE
 *       1         | BMI160_INTR2_INPUT_ENABLE
 *
 *	@param v_input_en_u8 :
 *	The value of input enable of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_INPUT
 *  0x00     |  BMI160_OUTPUT
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_input_enable(
u8 v_channel_u8, u8 *v_input_en_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read input enable of interrup1 and interrupt2*/
		case BMI160_INTR1_INPUT_ENABLE:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_INPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_input_en_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_INPUT_ENABLE);
			break;
		case BMI160_INTR2_INPUT_ENABLE:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_INPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_input_en_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_INPUT_ENABLE);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief API used to set input enable for interrupt1
 *	and interrupt2 pin from the register 0x54
 *	@brief interrupt1 - bit 4
 *	@brief interrupt2 - bit 5
 *
 *  @param v_channel_u8: The value of input enable selection
 *   v_channel_u8  |   input selection
 *  ---------------|---------------
 *       0         | BMI160_INTR1_INPUT_ENABLE
 *       1         | BMI160_INTR2_INPUT_ENABLE
 *
 *	@param v_input_en_u8 :
 *	The value of input enable of interrupt enable
 *	value    | Behaviour
 * ----------|-------------------
 *  0x01     |  BMI160_INPUT
 *  0x00     |  BMI160_OUTPUT
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_input_enable(
u8 v_channel_u8, u8 v_input_en_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write input enable of interrup1 and interrupt2*/
	case BMI160_INTR1_INPUT_ENABLE:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR1_INPUT_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR1_INPUT_ENABLE, v_input_en_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR1_INPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	case BMI160_INTR2_INPUT_ENABLE:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR2_INPUT_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR2_INPUT_ENABLE, v_input_en_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR2_INPUT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return com_rslt;
}
 /*!
 *	@brief reads the Low g interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 0 in the register 0x55
 *	@brief interrupt2 bit 0 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of low_g selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_LOW_G
 *       1         | BMI160_INTR2_MAP_LOW_G
 *
 *	@param v_intr_low_g_u8 : The value of low_g enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_g(
u8 v_channel_u8, u8 *v_intr_low_g_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the low_g interrupt */
		case BMI160_INTR1_MAP_LOW_G:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_low_g_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_LOW_G);
			break;
		case BMI160_INTR2_MAP_LOW_G:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_low_g_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_LOW_G);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief set the Low g interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 0 in the register 0x55
 *	@brief interrupt2 bit 0 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of low_g selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_LOW_G
 *       1         | BMI160_INTR2_MAP_LOW_G
 *
 *	@param v_intr_low_g_u8 : The value of low_g enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_g(
u8 v_channel_u8, u8 v_intr_low_g_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 v_step_cnt_stat_u8 = BMI160_INIT_VALUE;
u8 v_step_det_stat_u8 = BMI160_INIT_VALUE;

/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	/* check the step detector interrupt enable status*/
	com_rslt = bmi160_get_step_detector_enable(&v_step_det_stat_u8);
	/* disable the step detector interrupt */
	if (v_step_det_stat_u8 != BMI160_INIT_VALUE)
		com_rslt += bmi160_set_step_detector_enable(BMI160_INIT_VALUE);
	/* check the step counter interrupt enable status*/
	com_rslt += bmi160_get_step_counter_enable(&v_step_cnt_stat_u8);
	/* disable the step counter interrupt */
	if (v_step_cnt_stat_u8 != BMI160_INIT_VALUE)
			com_rslt += bmi160_set_step_counter_enable(
			BMI160_INIT_VALUE);
	switch (v_channel_u8) {
	/* write the low_g interrupt*/
	case BMI160_INTR1_MAP_LOW_G:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_LOW_G, v_intr_low_g_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_INTR2_MAP_LOW_G:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_LOW_G, v_intr_low_g_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the HIGH g interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 1 in the register 0x55
 *	@brief interrupt2 bit 1 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of high_g selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_HIGH_G
 *       1         | BMI160_INTR2_MAP_HIGH_G
 *
 *	@param v_intr_high_g_u8 : The value of high_g enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_high_g(
u8 v_channel_u8, u8 *v_intr_high_g_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read the high_g interrupt*/
		switch (v_channel_u8) {
		case BMI160_INTR1_MAP_HIGH_G:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_HIGH_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_high_g_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_HIGH_G);
		break;
		case BMI160_INTR2_MAP_HIGH_G:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_HIGH_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_high_g_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_HIGH_G);
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the HIGH g interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 1 in the register 0x55
 *	@brief interrupt2 bit 1 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of high_g selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_HIGH_G
 *       1         | BMI160_INTR2_MAP_HIGH_G
 *
 *	@param v_intr_high_g_u8 : The value of high_g enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_high_g(
u8 v_channel_u8, u8 v_intr_high_g_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write the high_g interrupt*/
	case BMI160_INTR1_MAP_HIGH_G:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_HIGH_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_HIGH_G, v_intr_high_g_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_HIGH_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	case BMI160_INTR2_MAP_HIGH_G:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_HIGH_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_HIGH_G, v_intr_high_g_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_HIGH_G__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the Any motion interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 2 in the register 0x55
 *	@brief interrupt2 bit 2 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of any motion selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_ANY_MOTION
 *       1         | BMI160_INTR2_MAP_ANY_MOTION
 *
 *	@param v_intr_any_motion_u8 : The value of any motion enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_any_motion(
u8 v_channel_u8, u8 *v_intr_any_motion_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the any motion interrupt */
		case BMI160_INTR1_MAP_ANY_MOTION:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_any_motion_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION);
		break;
		case BMI160_INTR2_MAP_ANY_MOTION:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_any_motion_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION);
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the Any motion interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 2 in the register 0x55
 *	@brief interrupt2 bit 2 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of any motion selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_ANY_MOTION
 *       1         | BMI160_INTR2_MAP_ANY_MOTION
 *
 *	@param v_intr_any_motion_u8 : The value of any motion enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_any_motion(
u8 v_channel_u8, u8 v_intr_any_motion_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 sig_mot_stat = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	/* read the status of significant motion interrupt */
	com_rslt = bmi160_get_intr_significant_motion_select(&sig_mot_stat);
	/* disable the significant motion interrupt */
	if (sig_mot_stat != BMI160_INIT_VALUE)
		com_rslt += bmi160_set_intr_significant_motion_select(
		BMI160_INIT_VALUE);
	switch (v_channel_u8) {
	/* write the any motion interrupt */
	case BMI160_INTR1_MAP_ANY_MOTION:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION,
			v_intr_any_motion_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	case BMI160_INTR2_MAP_ANY_MOTION:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION,
			v_intr_any_motion_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the No motion interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 3 in the register 0x55
 *	@brief interrupt2 bit 3 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of no motion selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_NOMO
 *       1         | BMI160_INTR2_MAP_NOMO
 *
 *	@param v_intr_nomotion_u8 : The value of no motion enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_nomotion(
u8 v_channel_u8, u8 *v_intr_nomotion_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the no motion interrupt*/
		case BMI160_INTR1_MAP_NOMO:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_NOMOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_nomotion_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_NOMOTION);
			break;
		case BMI160_INTR2_MAP_NOMO:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_NOMOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_nomotion_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_NOMOTION);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the No motion interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 3 in the register 0x55
 *	@brief interrupt2 bit 3 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of no motion selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_NOMO
 *       1         | BMI160_INTR2_MAP_NOMO
 *
 *	@param v_intr_nomotion_u8 : The value of no motion enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_nomotion(
u8 v_channel_u8, u8 v_intr_nomotion_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write the no motion interrupt*/
	case BMI160_INTR1_MAP_NOMO:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_NOMOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_NOMOTION,
			v_intr_nomotion_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_NOMOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_INTR2_MAP_NOMO:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_NOMOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_NOMOTION,
			v_intr_nomotion_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_NOMOTION__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the Double Tap interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 4 in the register 0x55
 *	@brief interrupt2 bit 4 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of double tap interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_DOUBLE_TAP
 *       1         | BMI160_INTR2_MAP_DOUBLE_TAP
 *
 *	@param v_intr_double_tap_u8 : The value of double tap enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_double_tap(
u8 v_channel_u8, u8 *v_intr_double_tap_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		case BMI160_INTR1_MAP_DOUBLE_TAP:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_DOUBLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_double_tap_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_DOUBLE_TAP);
			break;
		case BMI160_INTR2_MAP_DOUBLE_TAP:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_DOUBLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_double_tap_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_DOUBLE_TAP);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the Double Tap interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 4 in the register 0x55
 *	@brief interrupt2 bit 4 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of double tap interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_DOUBLE_TAP
 *       1         | BMI160_INTR2_MAP_DOUBLE_TAP
 *
 *	@param v_intr_double_tap_u8 : The value of double tap enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_double_tap(
u8 v_channel_u8, u8 v_intr_double_tap_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* set the double tap interrupt */
	case BMI160_INTR1_MAP_DOUBLE_TAP:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_DOUBLE_TAP__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_DOUBLE_TAP,
			v_intr_double_tap_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_DOUBLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_INTR2_MAP_DOUBLE_TAP:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_DOUBLE_TAP__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_DOUBLE_TAP,
			v_intr_double_tap_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_DOUBLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the Single Tap interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 5 in the register 0x55
 *	@brief interrupt2 bit 5 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of single tap interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_SINGLE_TAP
 *       1         | BMI160_INTR2_MAP_SINGLE_TAP
 *
 *	@param v_intr_single_tap_u8 : The value of single tap  enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_single_tap(
u8 v_channel_u8, u8 *v_intr_single_tap_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* reads the single tap interrupt*/
		case BMI160_INTR1_MAP_SINGLE_TAP:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_SINGLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_single_tap_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_SINGLE_TAP);
			break;
		case BMI160_INTR2_MAP_SINGLE_TAP:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_SINGLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_single_tap_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_SINGLE_TAP);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the Single Tap interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 5 in the register 0x55
 *	@brief interrupt2 bit 5 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of single tap interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_SINGLE_TAP
 *       1         | BMI160_INTR2_MAP_SINGLE_TAP
 *
 *	@param v_intr_single_tap_u8 : The value of single tap  enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_single_tap(
u8 v_channel_u8, u8 v_intr_single_tap_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write the single tap interrupt */
	case BMI160_INTR1_MAP_SINGLE_TAP:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_SINGLE_TAP__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_SINGLE_TAP,
			v_intr_single_tap_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_SINGLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_INTR2_MAP_SINGLE_TAP:
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_SINGLE_TAP__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_SINGLE_TAP,
			v_intr_single_tap_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_SINGLE_TAP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads the Orient interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 6 in the register 0x55
 *	@brief interrupt2 bit 6 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of orient interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_ORIENT
 *       1         | BMI160_INTR2_MAP_ORIENT
 *
 *	@param v_intr_orient_u8 : The value of orient enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient(
u8 v_channel_u8, u8 *v_intr_orient_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the orientation interrupt*/
		case BMI160_INTR1_MAP_ORIENT:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ORIENT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_orient_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_ORIENT);
			break;
		case BMI160_INTR2_MAP_ORIENT:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ORIENT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_orient_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_ORIENT);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write the Orient interrupt
 *	interrupt mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 6 in the register 0x55
 *	@brief interrupt2 bit 6 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of orient interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_ORIENT
 *       1         | BMI160_INTR2_MAP_ORIENT
 *
 *	@param v_intr_orient_u8 : The value of orient enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient(
u8 v_channel_u8, u8 v_intr_orient_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write the orientation interrupt*/
	case BMI160_INTR1_MAP_ORIENT:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ORIENT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_ORIENT, v_intr_orient_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_ORIENT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	case BMI160_INTR2_MAP_ORIENT:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ORIENT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_ORIENT, v_intr_orient_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_ORIENT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}
}
return com_rslt;
}
 /*!
 *	@brief Reads the Flat interrupt
 *	mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 7 in the register 0x55
 *	@brief interrupt2 bit 7 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of flat interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FLAT
 *       1         | BMI160_INTR2_MAP_FLAT
 *
 *	@param v_intr_flat_u8 : The value of flat enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_flat(
u8 v_channel_u8, u8 *v_intr_flat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the flat interrupt*/
		case BMI160_INTR1_MAP_FLAT:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_FLAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_flat_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_0_INTR1_FLAT);
			break;
		case BMI160_INTR2_MAP_FLAT:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_FLAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_flat_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_2_INTR2_FLAT);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief Write the Flat interrupt
 *	mapped to interrupt1
 *	and interrupt2 from the register 0x55 and 0x57
 *	@brief interrupt1 bit 7 in the register 0x55
 *	@brief interrupt2 bit 7 in the register 0x57
 *
 *
 *	@param v_channel_u8: The value of flat interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FLAT
 *       1         | BMI160_INTR2_MAP_FLAT
 *
 *	@param v_intr_flat_u8 : The value of flat enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_flat(
u8 v_channel_u8, u8 v_intr_flat_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* write the flat interrupt */
		case BMI160_INTR1_MAP_FLAT:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_0_INTR1_FLAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_0_INTR1_FLAT,
				v_intr_flat_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_0_INTR1_FLAT__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_INTR2_MAP_FLAT:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_2_INTR2_FLAT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_2_INTR2_FLAT,
				v_intr_flat_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_2_INTR2_FLAT__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Reads PMU trigger interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 0 and 4
 *	@brief interrupt1 bit 0 in the register 0x56
 *	@brief interrupt2 bit 4 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of pmu trigger selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_PMUTRIG
 *       1         | BMI160_INTR2_MAP_PMUTRIG
 *
 *	@param v_intr_pmu_trig_u8 : The value of pmu trigger enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_pmu_trig(
u8 v_channel_u8, u8 *v_intr_pmu_trig_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the pmu trigger interrupt*/
		case BMI160_INTR1_MAP_PMUTRIG:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_PMU_TRIG__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_pmu_trig_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_PMU_TRIG);
			break;
		case BMI160_INTR2_MAP_PMUTRIG:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_PMU_TRIG__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_pmu_trig_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_PMU_TRIG);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write PMU trigger interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 0 and 4
 *	@brief interrupt1 bit 0 in the register 0x56
 *	@brief interrupt2 bit 4 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of pmu trigger selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_PMUTRIG
 *       1         | BMI160_INTR2_MAP_PMUTRIG
 *
 *	@param v_intr_pmu_trig_u8 : The value of pmu trigger enable
 *	value    | trigger enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_pmu_trig(
u8 v_channel_u8, u8 v_intr_pmu_trig_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/* write the pmu trigger interrupt */
	case BMI160_INTR1_MAP_PMUTRIG:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_1_INTR1_PMU_TRIG__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_PMU_TRIG,
			v_intr_pmu_trig_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_PMU_TRIG__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	case BMI160_INTR2_MAP_PMUTRIG:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_1_INTR2_PMU_TRIG__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_PMU_TRIG,
			v_intr_pmu_trig_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_PMU_TRIG__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return com_rslt;
}
/*!
 *	@brief Reads FIFO Full interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 5 and 1
 *	@brief interrupt1 bit 5 in the register 0x56
 *	@brief interrupt2 bit 1 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of fifo full interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FIFO_FULL
 *       1         | BMI160_INTR2_MAP_FIFO_FULL
 *
 *	@param v_intr_fifo_full_u8 : The value of fifo full interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_fifo_full(
u8 v_channel_u8, u8 *v_intr_fifo_full_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the fifo full interrupt */
		case BMI160_INTR1_MAP_FIFO_FULL:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_FIFO_FULL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_fifo_full_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_FIFO_FULL);
		break;
		case BMI160_INTR2_MAP_FIFO_FULL:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_FIFO_FULL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_fifo_full_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_FIFO_FULL);
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write FIFO Full interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 5 and 1
 *	@brief interrupt1 bit 5 in the register 0x56
 *	@brief interrupt2 bit 1 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of fifo full interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FIFO_FULL
 *       1         | BMI160_INTR2_MAP_FIFO_FULL
 *
 *	@param v_intr_fifo_full_u8 : The value of fifo full interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_fifo_full(
u8 v_channel_u8, u8 v_intr_fifo_full_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* write the fifo full interrupt */
		case BMI160_INTR1_MAP_FIFO_FULL:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_FIFO_FULL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_1_INTR1_FIFO_FULL,
				v_intr_fifo_full_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_1_INTR1_FIFO_FULL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		case BMI160_INTR2_MAP_FIFO_FULL:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_FIFO_FULL__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_1_INTR2_FIFO_FULL,
				v_intr_fifo_full_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_1_INTR2_FIFO_FULL__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Reads FIFO Watermark interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 6 and 2
 *	@brief interrupt1 bit 6 in the register 0x56
 *	@brief interrupt2 bit 2 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of fifo Watermark interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FIFO_WM
 *       1         | BMI160_INTR2_MAP_FIFO_WM
 *
 *	@param v_intr_fifo_wm_u8 : The value of fifo Watermark interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_fifo_wm(
u8 v_channel_u8, u8 *v_intr_fifo_wm_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* read the fifo water mark interrupt */
		case BMI160_INTR1_MAP_FIFO_WM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_fifo_wm_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM);
			break;
		case BMI160_INTR2_MAP_FIFO_WM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_fifo_wm_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write FIFO Watermark interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56 bit 6 and 2
 *	@brief interrupt1 bit 6 in the register 0x56
 *	@brief interrupt2 bit 2 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of fifo Watermark interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_FIFO_WM
 *       1         | BMI160_INTR2_MAP_FIFO_WM
 *
 *	@param v_intr_fifo_wm_u8 : The value of fifo Watermark interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_fifo_wm(
u8 v_channel_u8, u8 v_intr_fifo_wm_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/* write the fifo water mark interrupt */
		case BMI160_INTR1_MAP_FIFO_WM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM,
				v_intr_fifo_wm_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_1_INTR1_FIFO_WM__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		case BMI160_INTR2_MAP_FIFO_WM:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM,
				v_intr_fifo_wm_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
				dev_addr,
				BMI160_USER_INTR_MAP_1_INTR2_FIFO_WM__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Reads Data Ready interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56
 *	@brief interrupt1 bit 7 in the register 0x56
 *	@brief interrupt2 bit 3 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of data ready interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_DATA_RDY
 *       1         | BMI160_INTR2_MAP_DATA_RDY
 *
 *	@param v_intr_data_rdy_u8 : The value of data ready interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_data_rdy(
u8 v_channel_u8, u8 *v_intr_data_rdy_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		switch (v_channel_u8) {
		/*Read Data Ready interrupt*/
		case BMI160_INTR1_MAP_DATA_RDY:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_DATA_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_data_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_DATA_RDY);
			break;
		case BMI160_INTR2_MAP_DATA_RDY:
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_DATA_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_data_rdy_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_DATA_RDY);
			break;
		default:
			com_rslt = E_BMI160_OUT_OF_RANGE;
			break;
		}
	}
	return com_rslt;
}
/*!
 *	@brief Write Data Ready interrupt mapped to interrupt1
 *	and interrupt2 form the register 0x56
 *	@brief interrupt1 bit 7 in the register 0x56
 *	@brief interrupt2 bit 3 in the register 0x56
 *
 *
 *	@param v_channel_u8: The value of data ready interrupt selection
 *   v_channel_u8  |   interrupt
 *  ---------------|---------------
 *       0         | BMI160_INTR1_MAP_DATA_RDY
 *       1         | BMI160_INTR2_MAP_DATA_RDY
 *
 *	@param v_intr_data_rdy_u8 : The value of data ready interrupt enable
 *	value    | interrupt enable
 * ----------|-------------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_data_rdy(
u8 v_channel_u8, u8 v_intr_data_rdy_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	switch (v_channel_u8) {
	/*Write Data Ready interrupt*/
	case BMI160_INTR1_MAP_DATA_RDY:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_1_INTR1_DATA_RDY__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR1_DATA_RDY,
			v_intr_data_rdy_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR1_DATA_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	case BMI160_INTR2_MAP_DATA_RDY:
		com_rslt =
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->
		dev_addr, BMI160_USER_INTR_MAP_1_INTR2_DATA_RDY__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MAP_1_INTR2_DATA_RDY,
			v_intr_data_rdy_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
			dev_addr, BMI160_USER_INTR_MAP_1_INTR2_DATA_RDY__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	break;
	default:
	com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API reads data source for the interrupt
 *	engine for the single and double tap interrupts from the register
 *	0x58 bit 3
 *
 *
 *  @param v_tap_source_u8 : The value of the tap source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_tap_source(u8 *v_tap_source_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the tap source interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_0_INTR_TAP_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_source_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_DATA_0_INTR_TAP_SOURCE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write data source for the interrupt
 *	engine for the single and double tap interrupts from the register
 *	0x58 bit 3
 *
 *
 *  @param v_tap_source_u8 : The value of the tap source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_tap_source(
u8 v_tap_source_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_tap_source_u8 <= BMI160_MAX_VALUE_SOURCE_INTR) {
			/* write the tap source interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_0_INTR_TAP_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_DATA_0_INTR_TAP_SOURCE,
				v_tap_source_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_DATA_0_INTR_TAP_SOURCE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API Reads Data source for the
 *	interrupt engine for the low and high g interrupts
 *	from the register 0x58 bit 7
 *
 *  @param v_low_high_source_u8 : The value of the tap source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_high_source(
u8 *v_low_high_source_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the high_low_g source interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_0_INTR_LOW_HIGH_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_high_source_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_DATA_0_INTR_LOW_HIGH_SOURCE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write Data source for the
 *	interrupt engine for the low and high g interrupts
 *	from the register 0x58 bit 7
 *
 *  @param v_low_high_source_u8 : The value of the tap source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_high_source(
u8 v_low_high_source_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_low_high_source_u8 <= BMI160_MAX_VALUE_SOURCE_INTR) {
		/* write the high_low_g source interrupt */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_DATA_0_INTR_LOW_HIGH_SOURCE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_DATA_0_INTR_LOW_HIGH_SOURCE,
			v_low_high_source_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_0_INTR_LOW_HIGH_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API reads Data source for the
 *	interrupt engine for the nomotion and anymotion interrupts
 *	from the register 0x59 bit 7
 *
 *  @param v_motion_source_u8 :
 *	The value of the any/no motion interrupt source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_motion_source(
u8 *v_motion_source_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the any/no motion interrupt  */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_1_INTR_MOTION_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_motion_source_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_DATA_1_INTR_MOTION_SOURCE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write Data source for the
 *	interrupt engine for the nomotion and anymotion interrupts
 *	from the register 0x59 bit 7
 *
 *  @param v_motion_source_u8 :
 *	The value of the any/no motion interrupt source
 *	value    | Description
 * ----------|-------------------
 *  0x01     |  UNFILTER_DATA
 *  0x00     |  FILTER_DATA
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_motion_source(
u8 v_motion_source_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_motion_source_u8 <= BMI160_MAX_VALUE_SOURCE_INTR) {
			/* write the any/no motion interrupt  */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_DATA_1_INTR_MOTION_SOURCE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_DATA_1_INTR_MOTION_SOURCE,
				v_motion_source_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INTR_DATA_1_INTR_MOTION_SOURCE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API is used to read the low_g duration from register
 *	0x5A bit 0 to 7
 *
 *
 *
 *
 *  @param v_low_g_durn_u8 : The value of low_g duration
 *
 *	@note Low_g duration trigger trigger delay according to
 *	"(v_low_g_durn_u8 * 2.5)ms" in a range from 2.5ms to 640ms.
 *	the default corresponds delay is 20ms
 *	@note When low_g data source of interrupt is unfiltered
 *	the sensor must not be in low power mode
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_g_durn(
u8 *v_low_g_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the low_g interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_0_INTR_LOW_DURN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_g_durn_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_0_INTR_LOW_DURN);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to write the low_g duration from register
 *	0x5A bit 0 to 7
 *
 *
 *
 *
 *  @param v_low_g_durn_u8 : The value of low_g duration
 *
 *	@note Low_g duration trigger trigger delay according to
 *	"(v_low_g_durn_u8 * 2.5)ms" in a range from 2.5ms to 640ms.
 *	the default corresponds delay is 20ms
 *	@note When low_g data source of interrupt is unfiltered
 *	the sensor must not be in low power mode
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_g_durn(u8 v_low_g_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the low_g interrupt */
			com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_0_INTR_LOW_DURN__REG,
			&v_low_g_durn_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read Threshold
 *	definition for the low-g interrupt from the register 0x5B bit 0 to 7
 *
 *
 *
 *
 *  @param v_low_g_thres_u8 : The value of low_g threshold
 *
 *	@note Low_g interrupt trigger threshold according to
 *	(v_low_g_thres_u8 * 7.81)mg for v_low_g_thres_u8 > 0
 *	3.91 mg for v_low_g_thres_u8 = 0
 *	The threshold range is form 3.91mg to 2.000mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_g_thres(
u8 *v_low_g_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read low_g threshold */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_1_INTR_LOW_THRES__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_g_thres_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_1_INTR_LOW_THRES);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to write Threshold
 *	definition for the low-g interrupt from the register 0x5B bit 0 to 7
 *
 *
 *
 *
 *  @param v_low_g_thres_u8 : The value of low_g threshold
 *
 *	@note Low_g interrupt trigger threshold according to
 *	(v_low_g_thres_u8 * 7.81)mg for v_low_g_thres_u8 > 0
 *	3.91 mg for v_low_g_thres_u8 = 0
 *	The threshold range is form 3.91mg to 2.000mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_g_thres(
u8 v_low_g_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write low_g threshold */
			com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_1_INTR_LOW_THRES__REG,
			&v_low_g_thres_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
 /*!
 *	@brief This API Reads Low-g interrupt hysteresis
 *	from the register 0x5C bit 0 to 1
 *
 *  @param v_low_hyst_u8 :The value of low_g hysteresis
 *
 *	@note Low_g hysteresis calculated by v_low_hyst_u8*125 mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_g_hyst(
u8 *v_low_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read low_g hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_hyst_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_HYST);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write Low-g interrupt hysteresis
 *	from the register 0x5C bit 0 to 1
 *
 *  @param v_low_hyst_u8 :The value of low_g hysteresis
 *
 *	@note Low_g hysteresis calculated by v_low_hyst_u8*125 mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_g_hyst(
u8 v_low_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write low_g hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_HYST,
				v_low_hyst_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_HYST__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API reads Low-g interrupt mode
 *	from the register 0x5C bit 2
 *
 *  @param v_low_g_mode_u8 : The value of low_g mode
 *	Value    |  Description
 * ----------|-----------------
 *	   0     | single-axis
 *     1     | axis-summing
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_low_g_mode(u8 *v_low_g_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/*read Low-g interrupt mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_low_g_mode_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_MODE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write Low-g interrupt mode
 *	from the register 0x5C bit 2
 *
 *  @param v_low_g_mode_u8 : The value of low_g mode
 *	Value    |  Description
 * ----------|-----------------
 *	   0     | single-axis
 *     1     | axis-summing
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_low_g_mode(
u8 v_low_g_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_low_g_mode_u8 <= BMI160_MAX_VALUE_LOW_G_MODE) {
			/*write Low-g interrupt mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_MODE,
				v_low_g_mode_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_INTR_LOWHIGH_2_INTR_LOW_G_MODE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API reads High-g interrupt hysteresis
 *	from the register 0x5C bit 6 and 7
 *
 *  @param v_high_g_hyst_u8 : The value of high hysteresis
 *
 *	@note High_g hysteresis changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | high_g hysteresis
 *  ----------------|---------------------
 *      2g          |  high_hy*125 mg
 *      4g          |  high_hy*250 mg
 *      8g          |  high_hy*500 mg
 *      16g         |  high_hy*1000 mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_high_g_hyst(
u8 *v_high_g_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read high_g hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_HIGH_G_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_hyst_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_2_INTR_HIGH_G_HYST);
		}
	return com_rslt;
}
/*!
 *	@brief This API write High-g interrupt hysteresis
 *	from the register 0x5C bit 6 and 7
 *
 *  @param v_high_g_hyst_u8 : The value of high hysteresis
 *
 *	@note High_g hysteresis changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | high_g hysteresis
 *  ----------------|---------------------
 *      2g          |  high_hy*125 mg
 *      4g          |  high_hy*250 mg
 *      8g          |  high_hy*500 mg
 *      16g         |  high_hy*1000 mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_high_g_hyst(
u8 v_high_g_hyst_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* write high_g hysteresis*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_INTR_LOWHIGH_2_INTR_HIGH_G_HYST__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_2_INTR_HIGH_G_HYST,
			v_high_g_hyst_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_2_INTR_HIGH_G_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	}
return com_rslt;
}
/*!
 *	@brief This API is used to read Delay
 *	time definition for the high-g interrupt from the register
 *	0x5D bit 0 to 7
 *
 *
 *
 *  @param  v_high_g_durn_u8 :  The value of high duration
 *
 *	@note High_g interrupt delay triggered according to
 *	v_high_g_durn_u8 * 2.5ms in a range from 2.5ms to 640ms
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_high_g_durn(
u8 *v_high_g_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read high_g duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_3_INTR_HIGH_G_DURN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_durn_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_3_INTR_HIGH_G_DURN);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to write Delay
 *	time definition for the high-g interrupt from the register
 *	0x5D bit 0 to 7
 *
 *
 *
 *  @param  v_high_g_durn_u8 :  The value of high duration
 *
 *	@note High_g interrupt delay triggered according to
 *	v_high_g_durn_u8 * 2.5ms in a range from 2.5ms to 640ms
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_high_g_durn(
u8 v_high_g_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write high_g duration*/
			com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_3_INTR_HIGH_G_DURN__REG,
			&v_high_g_durn_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to read Threshold
 *	definition for the high-g interrupt from the register 0x5E 0 to 7
 *
 *
 *
 *
 *  @param  v_high_g_thres_u8 : Pointer holding the value of Threshold
 *	@note High_g threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | high_g threshold
 *  ----------------|---------------------
 *      2g          |  v_high_g_thres_u8*7.81 mg
 *      4g          |  v_high_g_thres_u8*15.63 mg
 *      8g          |  v_high_g_thres_u8*31.25 mg
 *      16g         |  v_high_g_thres_u8*62.5 mg
 *	@note when v_high_g_thres_u8 = 0
 *   accel_range    | high_g threshold
 *  ----------------|---------------------
 *      2g          |  3.91 mg
 *      4g          |  7.81 mg
 *      8g          |  15.63 mg
 *      16g         |  31.25 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_high_g_thres(
u8 *v_high_g_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_LOWHIGH_4_INTR_HIGH_THRES__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_high_g_thres_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_LOWHIGH_4_INTR_HIGH_THRES);
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to write Threshold
 *	definition for the high-g interrupt from the register 0x5E 0 to 7
 *
 *
 *
 *
 *  @param  v_high_g_thres_u8 : Pointer holding the value of Threshold
 *	@note High_g threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | high_g threshold
 *  ----------------|---------------------
 *      2g          |  v_high_g_thres_u8*7.81 mg
 *      4g          |  v_high_g_thres_u8*15.63 mg
 *      8g          |  v_high_g_thres_u8*31.25 mg
 *      16g         |  v_high_g_thres_u8*62.5 mg
 *	@note when v_high_g_thres_u8 = 0
 *   accel_range    | high_g threshold
 *  ----------------|---------------------
 *      2g          |  3.91 mg
 *      4g          |  7.81 mg
 *      8g          |  15.63 mg
 *      16g         |  31.25 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_high_g_thres(
u8 v_high_g_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_INTR_LOWHIGH_4_INTR_HIGH_THRES__REG,
		&v_high_g_thres_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
	return com_rslt;
}
/*!
 *	@brief This API reads any motion duration
 *	from the register 0x5F bit 0 and 1
 *
 *  @param v_any_motion_durn_u8 : The value of any motion duration
 *
 *	@note Any motion duration can be calculated by "v_any_motion_durn_u8 + 1"
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_any_motion_durn(
u8 *v_any_motion_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read any motion duration*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_0_INTR_ANY_MOTION_DURN__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_any_motion_durn_u8 = BMI160_GET_BITSLICE
		(v_data_u8,
		BMI160_USER_INTR_MOTION_0_INTR_ANY_MOTION_DURN);
	}
	return com_rslt;
}
/*!
 *	@brief This API write any motion duration
 *	from the register 0x5F bit 0 and 1
 *
 *  @param v_any_motion_durn_u8 : The value of any motion duration
 *
 *	@note Any motion duration can be calculated by "v_any_motion_durn_u8 + 1"
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_any_motion_durn(
u8 v_any_motion_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* write any motion duration*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_0_INTR_ANY_MOTION_DURN__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MOTION_0_INTR_ANY_MOTION_DURN,
			v_any_motion_durn_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_MOTION_0_INTR_ANY_MOTION_DURN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read Slow/no-motion
 *	interrupt trigger delay duration from the register 0x5F bit 2 to 7
 *
 *  @param v_slow_no_motion_u8 :The value of slow no motion duration
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *	@note
 *	@note v_slow_no_motion_u8(5:4)=0b00 ->
 *	[v_slow_no_motion_u8(3:0) + 1] * 1.28s (1.28s-20.48s)
 *	@note v_slow_no_motion_u8(5:4)=1 ->
 *	[v_slow_no_motion_u8(3:0)+5] * 5.12s (25.6s-102.4s)
 *	@note v_slow_no_motion_u8(5)='1' ->
 *	[(v_slow_no_motion_u8:0)+11] * 10.24s (112.64s-430.08s);
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_slow_no_motion_durn(
u8 *v_slow_no_motion_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* read slow no motion duration*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_0_INTR_SLOW_NO_MOTION_DURN__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_slow_no_motion_u8 = BMI160_GET_BITSLICE
		(v_data_u8,
		BMI160_USER_INTR_MOTION_0_INTR_SLOW_NO_MOTION_DURN);
	}
return com_rslt;
}
 /*!
 *	@brief This API write Slow/no-motion
 *	interrupt trigger delay duration from the register 0x5F bit 2 to 7
 *
 *  @param v_slow_no_motion_u8 :The value of slow no motion duration
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *	@note
 *	@note v_slow_no_motion_u8(5:4)=0b00 ->
 *	[v_slow_no_motion_u8(3:0) + 1] * 1.28s (1.28s-20.48s)
 *	@note v_slow_no_motion_u8(5:4)=1 ->
 *	[v_slow_no_motion_u8(3:0)+5] * 5.12s (25.6s-102.4s)
 *	@note v_slow_no_motion_u8(5)='1' ->
 *	[(v_slow_no_motion_u8:0)+11] * 10.24s (112.64s-430.08s);
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_slow_no_motion_durn(
u8 v_slow_no_motion_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	/* write slow no motion duration*/
	com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
	(p_bmi160->dev_addr,
	BMI160_USER_INTR_MOTION_0_INTR_SLOW_NO_MOTION_DURN__REG,
	&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data_u8 = BMI160_SET_BITSLICE
		(v_data_u8,
		BMI160_USER_INTR_MOTION_0_INTR_SLOW_NO_MOTION_DURN,
		v_slow_no_motion_u8);
		com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_0_INTR_SLOW_NO_MOTION_DURN__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
}
return com_rslt;
}
/*!
 *	@brief This API is used to read threshold
 *	definition for the any-motion interrupt
 *	from the register 0x60 bit 0 to 7
 *
 *
 *  @param  v_any_motion_thres_u8 : The value of any motion threshold
 *
 *	@note any motion threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | any motion threshold
 *  ----------------|---------------------
 *      2g          |  v_any_motion_thres_u8*3.91 mg
 *      4g          |  v_any_motion_thres_u8*7.81 mg
 *      8g          |  v_any_motion_thres_u8*15.63 mg
 *      16g         |  v_any_motion_thres_u8*31.25 mg
 *	@note when v_any_motion_thres_u8 = 0
 *   accel_range    | any motion threshold
 *  ----------------|---------------------
 *      2g          |  1.95 mg
 *      4g          |  3.91 mg
 *      8g          |  7.81 mg
 *      16g         |  15.63 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_any_motion_thres(
u8 *v_any_motion_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read any motion threshold*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_any_motion_thres_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to write threshold
 *	definition for the any-motion interrupt
 *	from the register 0x60 bit 0 to 7
 *
 *
 *  @param  v_any_motion_thres_u8 : The value of any motion threshold
 *
 *	@note any motion threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | any motion threshold
 *  ----------------|---------------------
 *      2g          |  v_any_motion_thres_u8*3.91 mg
 *      4g          |  v_any_motion_thres_u8*7.81 mg
 *      8g          |  v_any_motion_thres_u8*15.63 mg
 *      16g         |  v_any_motion_thres_u8*31.25 mg
 *	@note when v_any_motion_thres_u8 = 0
 *   accel_range    | any motion threshold
 *  ----------------|---------------------
 *      2g          |  1.95 mg
 *      4g          |  3.91 mg
 *      8g          |  7.81 mg
 *      16g         |  15.63 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_any_motion_thres(
u8 v_any_motion_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* write any motion threshold*/
		com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_1_INTR_ANY_MOTION_THRES__REG,
		&v_any_motion_thres_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
	return com_rslt;
}
 /*!
 *	@brief This API is used to read threshold
 *	for the slow/no-motion interrupt
 *	from the register 0x61 bit 0 to 7
 *
 *
 *
 *
 *  @param v_slow_no_motion_thres_u8 : The value of slow no motion threshold
 *	@note slow no motion threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | slow no motion threshold
 *  ----------------|---------------------
 *      2g          |  v_slow_no_motion_thres_u8*3.91 mg
 *      4g          |  v_slow_no_motion_thres_u8*7.81 mg
 *      8g          |  v_slow_no_motion_thres_u8*15.63 mg
 *      16g         |  v_slow_no_motion_thres_u8*31.25 mg
 *	@note when v_slow_no_motion_thres_u8 = 0
 *   accel_range    | slow no motion threshold
 *  ----------------|---------------------
 *      2g          |  1.95 mg
 *      4g          |  3.91 mg
 *      8g          |  7.81 mg
 *      16g         |  15.63 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_slow_no_motion_thres(
u8 *v_slow_no_motion_thres_u8)
{
BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* read slow no motion threshold*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_2_INTR_SLOW_NO_MOTION_THRES__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_slow_no_motion_thres_u8 =
		BMI160_GET_BITSLICE(v_data_u8,
		BMI160_USER_INTR_MOTION_2_INTR_SLOW_NO_MOTION_THRES);
	}
return com_rslt;
}
 /*!
 *	@brief This API is used to write threshold
 *	for the slow/no-motion interrupt
 *	from the register 0x61 bit 0 to 7
 *
 *
 *
 *
 *  @param v_slow_no_motion_thres_u8 : The value of slow no motion threshold
 *	@note slow no motion threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | slow no motion threshold
 *  ----------------|---------------------
 *      2g          |  v_slow_no_motion_thres_u8*3.91 mg
 *      4g          |  v_slow_no_motion_thres_u8*7.81 mg
 *      8g          |  v_slow_no_motion_thres_u8*15.63 mg
 *      16g         |  v_slow_no_motion_thres_u8*31.25 mg
 *	@note when v_slow_no_motion_thres_u8 = 0
 *   accel_range    | slow no motion threshold
 *  ----------------|---------------------
 *      2g          |  1.95 mg
 *      4g          |  3.91 mg
 *      8g          |  7.81 mg
 *      16g         |  15.63 mg
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_slow_no_motion_thres(
u8 v_slow_no_motion_thres_u8)
{
BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* write slow no motion threshold*/
		com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_2_INTR_SLOW_NO_MOTION_THRES__REG,
		&v_slow_no_motion_thres_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
return com_rslt;
}
 /*!
 *	@brief This API is used to read
 *	the slow/no-motion selection from the register 0x62 bit 0
 *
 *
 *
 *
 *  @param  v_intr_slow_no_motion_select_u8 :
 *	The value of slow/no-motion select
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  SLOW_MOTION
 *  0x01     |  NO_MOTION
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_slow_no_motion_select(
u8 *v_intr_slow_no_motion_select_u8)
{
BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* read slow no motion select*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_3_INTR_SLOW_NO_MOTION_SELECT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_intr_slow_no_motion_select_u8 =
		BMI160_GET_BITSLICE(v_data_u8,
		BMI160_USER_INTR_MOTION_3_INTR_SLOW_NO_MOTION_SELECT);
	}
return com_rslt;
}
 /*!
 *	@brief This API is used to write
 *	the slow/no-motion selection from the register 0x62 bit 0
 *
 *
 *
 *
 *  @param  v_intr_slow_no_motion_select_u8 :
 *	The value of slow/no-motion select
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  SLOW_MOTION
 *  0x01     |  NO_MOTION
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_slow_no_motion_select(
u8 v_intr_slow_no_motion_select_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
} else {
if (v_intr_slow_no_motion_select_u8 <= BMI160_MAX_VALUE_NO_MOTION) {
	/* write slow no motion select*/
	com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
	(p_bmi160->dev_addr,
	BMI160_USER_INTR_MOTION_3_INTR_SLOW_NO_MOTION_SELECT__REG,
	&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
		BMI160_USER_INTR_MOTION_3_INTR_SLOW_NO_MOTION_SELECT,
		v_intr_slow_no_motion_select_u8);
		com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_MOTION_3_INTR_SLOW_NO_MOTION_SELECT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
} else {
com_rslt = E_BMI160_OUT_OF_RANGE;
}
}
return com_rslt;
}
 /*!
 *	@brief This API is used to select
 *	the significant or any motion interrupt from the register 0x62 bit 1
 *
 *
 *
 *
 *  @param  v_intr_significant_motion_select_u8 :
 *	the value of significant or any motion interrupt selection
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  ANY_MOTION
 *  0x01     |  SIGNIFICANT_MOTION
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_significant_motion_select(
u8 *v_intr_significant_motion_select_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the significant or any motion interrupt*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_intr_significant_motion_select_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to write, select
 *	the significant or any motion interrupt from the register 0x62 bit 1
 *
 *
 *
 *
 *  @param  v_intr_significant_motion_select_u8 :
 *	the value of significant or any motion interrupt selection
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  ANY_MOTION
 *  0x01     |  SIGNIFICANT_MOTION
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_significant_motion_select(
u8 v_intr_significant_motion_select_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_intr_significant_motion_select_u8 <=
	BMI160_MAX_VALUE_SIGNIFICANT_MOTION) {
		/* write the significant or any motion interrupt*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT,
			v_intr_significant_motion_select_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API is used to read
 *	the significant skip time from the register 0x62 bit  2 and 3
 *
 *
 *
 *
 *  @param  v_int_sig_mot_skip_u8 : the value of significant skip time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  skip time 1.5 seconds
 *  0x01     |  skip time 3 seconds
 *  0x02     |  skip time 6 seconds
 *  0x03     |  skip time 12 seconds
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_significant_motion_skip(
u8 *v_int_sig_mot_skip_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read significant skip time*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_int_sig_mot_skip_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to write
 *	the significant skip time from the register 0x62 bit  2 and 3
 *
 *
 *
 *
 *  @param  v_int_sig_mot_skip_u8 : the value of significant skip time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  skip time 1.5 seconds
 *  0x01     |  skip time 3 seconds
 *  0x02     |  skip time 6 seconds
 *  0x03     |  skip time 12 seconds
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_significant_motion_skip(
u8 v_int_sig_mot_skip_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_int_sig_mot_skip_u8 <= BMI160_MAX_UNDER_SIG_MOTION) {
			/* write significant skip time*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP,
				v_int_sig_mot_skip_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_SIGNIFICANT_MOTION_SKIP__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API is used to read
 *	the significant proof time from the register 0x62 bit  4 and 5
 *
 *
 *
 *
 *  @param  v_significant_motion_proof_u8 :
 *	the value of significant proof time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  proof time 0.25 seconds
 *  0x01     |  proof time 0.5 seconds
 *  0x02     |  proof time 1 seconds
 *  0x03     |  proof time 2 seconds
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_significant_motion_proof(
u8 *v_significant_motion_proof_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read significant proof time */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_significant_motion_proof_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF);
		}
	return com_rslt;
}
 /*!
 *	@brief This API is used to write
 *	the significant proof time from the register 0x62 bit  4 and 5
 *
 *
 *
 *
 *  @param  v_significant_motion_proof_u8 :
 *	the value of significant proof time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     |  proof time 0.25 seconds
 *  0x01     |  proof time 0.5 seconds
 *  0x02     |  proof time 1 seconds
 *  0x03     |  proof time 2 seconds
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_significant_motion_proof(
u8 v_significant_motion_proof_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_significant_motion_proof_u8
		<= BMI160_MAX_UNDER_SIG_MOTION) {
			/* write significant proof time */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF,
				v_significant_motion_proof_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_SIGNIFICANT_MOTION_PROOF__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API is used to get the tap duration
 *	from the register 0x63 bit 0 to 2
 *
 *
 *
 *  @param v_tap_durn_u8 : The value of tap duration
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_DURN_50MS
 *  0x01     | BMI160_TAP_DURN_100MS
 *  0x03     | BMI160_TAP_DURN_150MS
 *  0x04     | BMI160_TAP_DURN_200MS
 *  0x05     | BMI160_TAP_DURN_250MS
 *  0x06     | BMI160_TAP_DURN_375MS
 *  0x07     | BMI160_TAP_DURN_700MS
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_tap_durn(
u8 *v_tap_durn_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_DURN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_durn_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_INTR_TAP_0_INTR_TAP_DURN);
		}
	return com_rslt;
}
/*!
 *	@brief This API is used to write the tap duration
 *	from the register 0x63 bit 0 to 2
 *
 *
 *
 *  @param v_tap_durn_u8 : The value of tap duration
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_DURN_50MS
 *  0x01     | BMI160_TAP_DURN_100MS
 *  0x03     | BMI160_TAP_DURN_150MS
 *  0x04     | BMI160_TAP_DURN_200MS
 *  0x05     | BMI160_TAP_DURN_250MS
 *  0x06     | BMI160_TAP_DURN_375MS
 *  0x07     | BMI160_TAP_DURN_700MS
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_tap_durn(
u8 v_tap_durn_u8)
{
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_tap_durn_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_tap_durn_u8 <= BMI160_MAX_TAP_TURN) {
			switch (v_tap_durn_u8) {
			case BMI160_TAP_DURN_50MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_50MS;
				break;
			case BMI160_TAP_DURN_100MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_100MS;
				break;
			case BMI160_TAP_DURN_150MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_150MS;
				break;
			case BMI160_TAP_DURN_200MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_200MS;
				break;
			case BMI160_TAP_DURN_250MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_250MS;
				break;
			case BMI160_TAP_DURN_375MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_375MS;
				break;
			case BMI160_TAP_DURN_500MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_500MS;
				break;
			case BMI160_TAP_DURN_700MS:
				v_data_tap_durn_u8 = BMI160_TAP_DURN_700MS;
				break;
			default:
				break;
			}
			/* write tap duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_DURN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_TAP_0_INTR_TAP_DURN,
				v_data_tap_durn_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_TAP_0_INTR_TAP_DURN__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read the
 *	tap shock duration from the register 0x63 bit 2
 *
 *  @param v_tap_shock_u8 :The value of tap shock
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_SHOCK_50MS
 *  0x01     | BMI160_TAP_SHOCK_75MS
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_tap_shock(
u8 *v_tap_shock_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap shock duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_SHOCK__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_shock_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_TAP_0_INTR_TAP_SHOCK);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write the
 *	tap shock duration from the register 0x63 bit 2
 *
 *  @param v_tap_shock_u8 :The value of tap shock
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_SHOCK_50MS
 *  0x01     | BMI160_TAP_SHOCK_75MS
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_tap_shock(u8 v_tap_shock_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_tap_shock_u8 <= BMI160_MAX_VALUE_TAP_SHOCK) {
			/* write tap shock duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_SHOCK__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_TAP_0_INTR_TAP_SHOCK,
				v_tap_shock_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_TAP_0_INTR_TAP_SHOCK__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read
 *	tap quiet duration from the register 0x63 bit 7
 *
 *
 *  @param v_tap_quiet_u8 : The value of tap quiet
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_QUIET_30MS
 *  0x01     | BMI160_TAP_QUIET_20MS
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_tap_quiet(
u8 *v_tap_quiet_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap quiet duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_QUIET__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_quiet_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_INTR_TAP_0_INTR_TAP_QUIET);
		}
	return com_rslt;
}
/*!
 *	@brief This API write
 *	tap quiet duration from the register 0x63 bit 7
 *
 *
 *  @param v_tap_quiet_u8 : The value of tap quiet
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | BMI160_TAP_QUIET_30MS
 *  0x01     | BMI160_TAP_QUIET_20MS
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_tap_quiet(u8 v_tap_quiet_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_tap_quiet_u8 <= BMI160_MAX_VALUE_TAP_QUIET) {
			/* write tap quiet duration*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_0_INTR_TAP_QUIET__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_TAP_0_INTR_TAP_QUIET,
				v_tap_quiet_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_TAP_0_INTR_TAP_QUIET__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read Threshold of the
 *	single/double tap interrupt from the register 0x64 bit 0 to 4
 *
 *
 *	@param v_tap_thres_u8 : The value of single/double tap threshold
 *
 *	@note single/double tap threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | single/double tap threshold
 *  ----------------|---------------------
 *      2g          |  ((v_tap_thres_u8 + 1) * 62.5)mg
 *      4g          |  ((v_tap_thres_u8 + 1) * 125)mg
 *      8g          |  ((v_tap_thres_u8 + 1) * 250)mg
 *      16g         |  ((v_tap_thres_u8 + 1) * 500)mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_tap_thres(
u8 *v_tap_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read tap threshold*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_1_INTR_TAP_THRES__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_tap_thres_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_TAP_1_INTR_TAP_THRES);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write Threshold of the
 *	single/double tap interrupt from the register 0x64 bit 0 to 4
 *
 *
 *	@param v_tap_thres_u8 : The value of single/double tap threshold
 *
 *	@note single/double tap threshold changes according to accel g range
 *	accel g range can be set by the function ""
 *   accel_range    | single/double tap threshold
 *  ----------------|---------------------
 *      2g          |  ((v_tap_thres_u8 + 1) * 62.5)mg
 *      4g          |  ((v_tap_thres_u8 + 1) * 125)mg
 *      8g          |  ((v_tap_thres_u8 + 1) * 250)mg
 *      16g         |  ((v_tap_thres_u8 + 1) * 500)mg
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_tap_thres(
u8 v_tap_thres_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write tap threshold*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_TAP_1_INTR_TAP_THRES__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_TAP_1_INTR_TAP_THRES,
				v_tap_thres_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_TAP_1_INTR_TAP_THRES__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
 /*!
 *	@brief This API read the threshold for orientation interrupt
 *	from the register 0x65 bit 0 and 1
 *
 *  @param v_orient_mode_u8 : The value of threshold for orientation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | symmetrical
 *  0x01     | high-asymmetrical
 *  0x02     | low-asymmetrical
 *  0x03     | symmetrical
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_mode(
u8 *v_orient_mode_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orientation threshold*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_mode_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_MODE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write the threshold for orientation interrupt
 *	from the register 0x65 bit 0 and 1
 *
 *  @param v_orient_mode_u8 : The value of threshold for orientation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | symmetrical
 *  0x01     | high-asymmetrical
 *  0x02     | low-asymmetrical
 *  0x03     | symmetrical
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_mode(
u8 v_orient_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_orient_mode_u8 <= BMI160_MAX_ORIENT_MODE) {
			/* write orientation threshold*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_MODE,
				v_orient_mode_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_MODE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read the orient blocking mode
 *	that is used for the generation of the orientation interrupt.
 *	from the register 0x65 bit 2 and 3
 *
 *  @param v_orient_blocking_u8 : The value of orient blocking mode
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | No blocking
 *  0x01     | Theta blocking or acceleration in any axis > 1.5g
 *  0x02     | Theta blocking or acceleration slope in any axis >
 *   -       | 0.2g or acceleration in any axis > 1.5g
 *  0x03     | Theta blocking or acceleration slope in any axis >
 *   -       | 0.4g or acceleration in any axis >
 *   -       | 1.5g and value of orient is not stable
 *   -       | for at least 100 ms
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_blocking(
u8 *v_orient_blocking_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orient blocking mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_BLOCKING__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_blocking_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_BLOCKING);
		}
	return com_rslt;
}
/*!
 *	@brief This API write the orient blocking mode
 *	that is used for the generation of the orientation interrupt.
 *	from the register 0x65 bit 2 and 3
 *
 *  @param v_orient_blocking_u8 : The value of orient blocking mode
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | No blocking
 *  0x01     | Theta blocking or acceleration in any axis > 1.5g
 *  0x02     | Theta blocking or acceleration slope in any axis >
 *   -       | 0.2g or acceleration in any axis > 1.5g
 *  0x03     | Theta blocking or acceleration slope in any axis >
 *   -       | 0.4g or acceleration in any axis >
 *   -       | 1.5g and value of orient is not stable
 *   -       | for at least 100 ms
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_blocking(
u8 v_orient_blocking_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_orient_blocking_u8 <= BMI160_MAX_ORIENT_BLOCKING) {
		/* write orient blocking mode*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_BLOCKING__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_BLOCKING,
			v_orient_blocking_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_BLOCKING__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
/*!
 *	@brief This API read Orient interrupt
 *	hysteresis, from the register 0x64 bit 4 to 7
 *
 *
 *
 *  @param v_orient_hyst_u8 : The value of orient hysteresis
 *
 *	@note 1 LSB corresponds to 62.5 mg,
 *	irrespective of the selected accel range
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_hyst(
u8 *v_orient_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orient hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_hyst_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_HYST);
		}
	return com_rslt;
}
/*!
 *	@brief This API write Orient interrupt
 *	hysteresis, from the register 0x64 bit 4 to 7
 *
 *
 *
 *  @param v_orient_hyst_u8 : The value of orient hysteresis
 *
 *	@note 1 LSB corresponds to 62.5 mg,
 *	irrespective of the selected accel range
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_hyst(
u8 v_orient_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write orient hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_HYST,
				v_orient_hyst_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_ORIENT_0_INTR_ORIENT_HYST__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
 /*!
 *	@brief This API read Orient
 *	blocking angle (0 to 44.8) from the register 0x66 bit 0 to 5
 *
 *  @param v_orient_theta_u8 : The value of Orient blocking angle
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_theta(
u8 *v_orient_theta_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read Orient blocking angle*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_THETA__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_theta_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_THETA);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write Orient
 *	blocking angle (0 to 44.8) from the register 0x66 bit 0 to 5
 *
 *  @param v_orient_theta_u8 : The value of Orient blocking angle
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_theta(
u8 v_orient_theta_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_orient_theta_u8 <= BMI160_MAX_ORIENT_THETA) {
		/* write Orient blocking angle*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_THETA__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_THETA,
			v_orient_theta_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_THETA__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
/*!
 *	@brief This API read orient change
 *	of up/down bit from the register 0x66 bit 6
 *
 *  @param v_orient_ud_u8 : The value of orient change of up/down
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | Is ignored
 *  0x01     | Generates orientation interrupt
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_ud_enable(
u8 *v_orient_ud_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orient up/down enable*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_UD_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_ud_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_UD_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write orient change
 *	of up/down bit from the register 0x66 bit 6
 *
 *  @param v_orient_ud_u8 : The value of orient change of up/down
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | Is ignored
 *  0x01     | Generates orientation interrupt
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_ud_enable(
u8 v_orient_ud_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_orient_ud_u8 <= BMI160_MAX_VALUE_ORIENT_UD) {
		/* write orient up/down enable */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_UD_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_UD_ENABLE,
			v_orient_ud_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_UD_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API read orientation axes changes
 *	from the register 0x66 bit 7
 *
 *  @param v_orient_axes_u8 : The value of orient axes assignment
 *	value    |       Behaviour    | Name
 * ----------|--------------------|------
 *  0x00     | x = x, y = y, z = z|orient_ax_noex
 *  0x01     | x = y, y = z, z = x|orient_ax_ex
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_orient_axes_enable(
u8 *v_orient_axes_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read orientation axes changes  */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_AXES_EX__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_orient_axes_u8 = BMI160_GET_BITSLICE
			(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_AXES_EX);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write orientation axes changes
 *	from the register 0x66 bit 7
 *
 *  @param v_orient_axes_u8 : The value of orient axes assignment
 *	value    |       Behaviour    | Name
 * ----------|--------------------|------
 *  0x00     | x = x, y = y, z = z|orient_ax_noex
 *  0x01     | x = y, y = z, z = x|orient_ax_ex
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_orient_axes_enable(
u8 v_orient_axes_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
	if (v_orient_axes_u8 <= BMI160_MAX_VALUE_ORIENT_AXES) {
		/*write orientation axes changes  */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_AXES_EX__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_AXES_EX,
			v_orient_axes_u8);
			com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_ORIENT_1_INTR_ORIENT_AXES_EX__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
return com_rslt;
}
 /*!
 *	@brief This API read Flat angle (0 to 44.8) for flat interrupt
 *	from the register 0x67 bit 0 to 5
 *
 *  @param v_flat_theta_u8 : The value of flat angle
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_flat_theta(
u8 *v_flat_theta_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read Flat angle*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_0_INTR_FLAT_THETA__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_flat_theta_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_FLAT_0_INTR_FLAT_THETA);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write Flat angle (0 to 44.8) for flat interrupt
 *	from the register 0x67 bit 0 to 5
 *
 *  @param v_flat_theta_u8 : The value of flat angle
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_flat_theta(
u8 v_flat_theta_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_flat_theta_u8 <= BMI160_MAX_FLAT_THETA) {
			/* write Flat angle */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_0_INTR_FLAT_THETA__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_FLAT_0_INTR_FLAT_THETA,
				v_flat_theta_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_FLAT_0_INTR_FLAT_THETA__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read Flat interrupt hold time;
 *	from the register 0x68 bit 4 and 5
 *
 *  @param v_flat_hold_u8 : The value of flat hold time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | 0ms
 *  0x01     | 512ms
 *  0x01     | 1024ms
 *  0x01     | 2048ms
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_flat_hold(
u8 *v_flat_hold_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read flat hold time*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HOLD__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_flat_hold_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HOLD);
		}
	return com_rslt;
}
/*!
 *	@brief This API write Flat interrupt hold time;
 *	from the register 0x68 bit 4 and 5
 *
 *  @param v_flat_hold_u8 : The value of flat hold time
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | 0ms
 *  0x01     | 512ms
 *  0x01     | 1024ms
 *  0x01     | 2048ms
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_flat_hold(
u8 v_flat_hold_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_flat_hold_u8 <= BMI160_MAX_FLAT_HOLD) {
			/* write flat hold time*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HOLD__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_FLAT_1_INTR_FLAT_HOLD,
				v_flat_hold_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_FLAT_1_INTR_FLAT_HOLD__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read flat interrupt hysteresis
 *	from the register 0x68 bit 0 to 3
 *
 *  @param v_flat_hyst_u8 : The value of flat hysteresis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_intr_flat_hyst(
u8 *v_flat_hyst_u8)
{
	/* variable used to return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the flat hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_flat_hyst_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HYST);
		}
	return com_rslt;
}
/*!
 *	@brief This API write flat interrupt hysteresis
 *	from the register 0x68 bit 0 to 3
 *
 *  @param v_flat_hyst_u8 : The value of flat hysteresis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_intr_flat_hyst(
u8 v_flat_hyst_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_flat_hyst_u8 <= BMI160_MAX_FLAT_HYST) {
			/* read the flat hysteresis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_INTR_FLAT_1_INTR_FLAT_HYST__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_FLAT_1_INTR_FLAT_HYST,
				v_flat_hyst_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_INTR_FLAT_1_INTR_FLAT_HYST__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read accel offset compensation
 *	target value for z-axis from the register 0x69 bit 0 and 1
 *
 *  @param v_foc_accel_z_u8 : the value of accel offset compensation z axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_accel_z(u8 *v_foc_accel_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel offset compensation for z axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_foc_accel_z_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FOC_ACCEL_Z);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write accel offset compensation
 *	target value for z-axis from the register 0x69 bit 0 and 1
 *
 *  @param v_foc_accel_z_u8 : the value of accel offset compensation z axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_accel_z(
u8 v_foc_accel_z_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write the accel offset compensation for z axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_Z,
				v_foc_accel_z_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Z__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel offset compensation
 *	target value for y-axis
 *	from the register 0x69 bit 2 and 3
 *
 *  @param v_foc_accel_y_u8 : the value of accel offset compensation y axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_accel_y(u8 *v_foc_accel_y_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the accel offset compensation for y axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_foc_accel_y_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FOC_ACCEL_Y);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel offset compensation
 *	target value for y-axis
 *	from the register 0x69 bit 2 and 3
 *
 *  @param v_foc_accel_y_u8 : the value of accel offset compensation y axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x02     | -1g
 *  0x03     | 0g
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_accel_y(u8 v_foc_accel_y_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_foc_accel_y_u8 <= BMI160_MAX_ACCEL_FOC) {
			/* write the accel offset compensation for y axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_Y,
				v_foc_accel_y_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Y__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel offset compensation
 *	target value for x-axis is
 *	from the register 0x69 bit 4 and 5
 *
 *  @param v_foc_accel_x_u8 : the value of accel offset compensation x axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x02     | -1g
 *  0x03     | 0g
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_accel_x(u8 *v_foc_accel_x_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read the accel offset compensation for x axis*/
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_USER_FOC_ACCEL_X__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_foc_accel_x_u8 = BMI160_GET_BITSLICE(v_data_u8,
		BMI160_USER_FOC_ACCEL_X);
	}
	return com_rslt;
}
/*!
 *	@brief This API write accel offset compensation
 *	target value for x-axis is
 *	from the register 0x69 bit 4 and 5
 *
 *  @param v_foc_accel_x_u8 : the value of accel offset compensation x axis
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_accel_x(u8 v_foc_accel_x_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_foc_accel_x_u8 <= BMI160_MAX_ACCEL_FOC) {
			/* write the accel offset compensation for x axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_X,
				v_foc_accel_x_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_X__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API writes accel fast offset compensation
 *	from the register 0x69 bit 0 to 5
 *	@brief This API writes each axis individually
 *	FOC_X_AXIS - bit 4 and 5
 *	FOC_Y_AXIS - bit 2 and 3
 *	FOC_Z_AXIS - bit 0 and 1
 *
 *  @param  v_foc_accel_u8: The value of accel offset compensation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *  @param  v_axis_u8: The value of accel offset axis selection
  *	value    | axis
 * ----------|-------------------
 *  0        | FOC_X_AXIS
 *  1        | FOC_Y_AXIS
 *  2        | FOC_Z_AXIS
 *
 *	@param v_accel_offset_s8: The accel offset value
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_foc_trigger(u8 v_axis_u8,
u8 v_foc_accel_u8, s8 *v_accel_offset_s8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
s8 v_status_s8 = SUCCESS;
u8 v_timeout_u8 = BMI160_INIT_VALUE;
s8 v_foc_accel_offset_x_s8  = BMI160_INIT_VALUE;
s8 v_foc_accel_offset_y_s8 =  BMI160_INIT_VALUE;
s8 v_foc_accel_offset_z_s8 =  BMI160_INIT_VALUE;
u8 focstatus = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
} else {
	v_status_s8 = bmi160_set_accel_offset_enable(
	ACCEL_OFFSET_ENABLE);
	if (v_status_s8 == SUCCESS) {
		switch (v_axis_u8) {
		case FOC_X_AXIS:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_X,
				v_foc_accel_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_X__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* trigger the
			FOC need to write
			0x03 in the register 0x7e*/
			com_rslt +=
			bmi160_set_command_register(
			START_FOC_ACCEL_GYRO);

			com_rslt +=
			bmi160_get_foc_rdy(&focstatus);
			if ((com_rslt != SUCCESS) ||
			(focstatus != BMI160_FOC_STAT_HIGH)) {
				while ((com_rslt != SUCCESS) ||
				(focstatus != BMI160_FOC_STAT_HIGH
				&& v_timeout_u8 <
				BMI160_MAXIMUM_TIMEOUT)) {
					p_bmi160->delay_msec(
					BMI160_DELAY_SETTLING_TIME);
					com_rslt = bmi160_get_foc_rdy(
					&focstatus);
					v_timeout_u8++;
				}
			}
			if ((com_rslt == SUCCESS) &&
				(focstatus == BMI160_FOC_STAT_HIGH)) {
				com_rslt +=
				bmi160_get_accel_offset_compensation_xaxis(
				&v_foc_accel_offset_x_s8);
				*v_accel_offset_s8 =
				v_foc_accel_offset_x_s8;
			}
		break;
		case FOC_Y_AXIS:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_Y,
				v_foc_accel_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Y__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* trigger the FOC
			need to write 0x03
			in the register 0x7e*/
			com_rslt +=
			bmi160_set_command_register(
			START_FOC_ACCEL_GYRO);

			com_rslt +=
			bmi160_get_foc_rdy(&focstatus);
			if ((com_rslt != SUCCESS) ||
			(focstatus != BMI160_FOC_STAT_HIGH)) {
				while ((com_rslt != SUCCESS) ||
				(focstatus != BMI160_FOC_STAT_HIGH
				&& v_timeout_u8 <
				BMI160_MAXIMUM_TIMEOUT)) {
					p_bmi160->delay_msec(
					BMI160_DELAY_SETTLING_TIME);
					com_rslt = bmi160_get_foc_rdy(
					&focstatus);
					v_timeout_u8++;
				}
			}
			if ((com_rslt == SUCCESS) &&
			(focstatus == BMI160_FOC_STAT_HIGH)) {
				com_rslt +=
				bmi160_get_accel_offset_compensation_yaxis(
				&v_foc_accel_offset_y_s8);
				*v_accel_offset_s8 =
				v_foc_accel_offset_y_s8;
			}
		break;
		case FOC_Z_AXIS:
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_ACCEL_Z,
				v_foc_accel_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Z__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* trigger the FOC need to write
			0x03 in the register 0x7e*/
			com_rslt +=
			bmi160_set_command_register(
			START_FOC_ACCEL_GYRO);

			com_rslt +=
			bmi160_get_foc_rdy(&focstatus);
			if ((com_rslt != SUCCESS) ||
			(focstatus != BMI160_FOC_STAT_HIGH)) {
				while ((com_rslt != SUCCESS) ||
				(focstatus != BMI160_FOC_STAT_HIGH
				&& v_timeout_u8 <
				BMI160_MAXIMUM_TIMEOUT)) {
					p_bmi160->delay_msec(
					BMI160_DELAY_SETTLING_TIME);
					com_rslt = bmi160_get_foc_rdy(
					&focstatus);
					v_timeout_u8++;
				}
			}
			if ((com_rslt == SUCCESS) &&
			(focstatus == BMI160_FOC_STAT_HIGH)) {
				com_rslt +=
				bmi160_get_accel_offset_compensation_zaxis(
				&v_foc_accel_offset_z_s8);
				*v_accel_offset_s8 =
				v_foc_accel_offset_z_s8;
			}
		break;
		default:
		break;
		}
	} else {
	com_rslt =  ERROR;
	}
}
return com_rslt;
}
/*!
 *	@brief This API write fast accel offset compensation
 *	it writes all axis together.To the register 0x69 bit 0 to 5
 *	FOC_X_AXIS - bit 4 and 5
 *	FOC_Y_AXIS - bit 2 and 3
 *	FOC_Z_AXIS - bit 0 and 1
 *
 *  @param  v_foc_accel_x_u8: The value of accel offset x compensation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *  @param  v_foc_accel_y_u8: The value of accel offset y compensation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *  @param  v_foc_accel_z_u8: The value of accel offset z compensation
 *	value    | Behaviour
 * ----------|-------------------
 *  0x00     | disable
 *  0x01     | +1g
 *  0x01     | -1g
 *  0x01     | 0g
 *
 *  @param  v_accel_off_x_s8: The value of accel offset x axis
 *  @param  v_accel_off_y_s8: The value of accel offset y axis
 *  @param  v_accel_off_z_s8: The value of accel offset z axis
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_accel_foc_trigger_xyz(u8 v_foc_accel_x_u8,
u8 v_foc_accel_y_u8, u8 v_foc_accel_z_u8, s8 *v_accel_off_x_s8,
s8 *v_accel_off_y_s8, s8 *v_accel_off_z_s8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 focx = BMI160_INIT_VALUE;
u8 focy = BMI160_INIT_VALUE;
u8 focz = BMI160_INIT_VALUE;
s8 v_foc_accel_offset_x_s8 = BMI160_INIT_VALUE;
s8 v_foc_accel_offset_y_s8 = BMI160_INIT_VALUE;
s8 v_foc_accel_offset_z_s8 = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
u8 v_timeout_u8 = BMI160_INIT_VALUE;
u8 focstatus = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		v_status_s8 = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (v_status_s8 == SUCCESS) {
			/* foc x axis*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_X__REG,
			&focx, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				focx = BMI160_SET_BITSLICE(focx,
				BMI160_USER_FOC_ACCEL_X,
				v_foc_accel_x_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_X__REG,
				&focx, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* foc y axis*/
			com_rslt +=
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Y__REG,
			&focy, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				focy = BMI160_SET_BITSLICE(focy,
				BMI160_USER_FOC_ACCEL_Y,
				v_foc_accel_y_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Y__REG,
				&focy, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* foc z axis*/
			com_rslt +=
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_ACCEL_Z__REG,
			&focz, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				focz = BMI160_SET_BITSLICE(focz,
				BMI160_USER_FOC_ACCEL_Z,
				v_foc_accel_z_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_FOC_ACCEL_Z__REG,
				&focz, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* trigger the FOC need to
			write 0x03 in the register 0x7e*/
			com_rslt += bmi160_set_command_register(
			START_FOC_ACCEL_GYRO);

			com_rslt += bmi160_get_foc_rdy(
			&focstatus);
			if ((com_rslt != SUCCESS) ||
			(focstatus != BMI160_FOC_STAT_HIGH)) {
				while ((com_rslt != SUCCESS) ||
				(focstatus != BMI160_FOC_STAT_HIGH
				&& v_timeout_u8 <
				BMI160_MAXIMUM_TIMEOUT)) {
					p_bmi160->delay_msec(
					BMI160_DELAY_SETTLING_TIME);
					com_rslt = bmi160_get_foc_rdy(
					&focstatus);
					v_timeout_u8++;
				}
			}
			if ((com_rslt == SUCCESS) &&
			(focstatus == BMI160_GEN_READ_WRITE_DATA_LENGTH)) {
				com_rslt +=
				bmi160_get_accel_offset_compensation_xaxis(
				&v_foc_accel_offset_x_s8);
				*v_accel_off_x_s8 =
				v_foc_accel_offset_x_s8;
				com_rslt +=
				bmi160_get_accel_offset_compensation_yaxis(
				&v_foc_accel_offset_y_s8);
				*v_accel_off_y_s8 =
				v_foc_accel_offset_y_s8;
				com_rslt +=
				bmi160_get_accel_offset_compensation_zaxis(
				&v_foc_accel_offset_z_s8);
				*v_accel_off_z_s8 =
				v_foc_accel_offset_z_s8;
			}
		} else {
		com_rslt =  ERROR;
		}
	}
return com_rslt;
}
/*!
 *	@brief This API read gyro fast offset enable
 *	from the register 0x69 bit 6
 *
 *  @param v_foc_gyro_u8 : The value of gyro fast offset enable
 *  value    |  Description
 * ----------|-------------
 *    0      | fast offset compensation disabled
 *    1      |  fast offset compensation enabled
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_foc_gyro_enable(
u8 *v_foc_gyro_u8)
{
	/* used for return the status of bus communication */
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the gyro fast offset enable*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_FOC_GYRO_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_foc_gyro_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_FOC_GYRO_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro fast offset enable
 *	from the register 0x69 bit 6
 *
 *  @param v_foc_gyro_u8 : The value of gyro fast offset enable
 *  value    |  Description
 * ----------|-------------
 *    0      | fast offset compensation disabled
 *    1      |  fast offset compensation enabled
 *
 *	@param v_gyro_off_x_s16 : The value of gyro fast offset x axis data
 *	@param v_gyro_off_y_s16 : The value of gyro fast offset y axis data
 *	@param v_gyro_off_z_s16 : The value of gyro fast offset z axis data
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_foc_gyro_enable(
u8 v_foc_gyro_u8, s16 *v_gyro_off_x_s16,
s16 *v_gyro_off_y_s16, s16 *v_gyro_off_z_s16)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
u8 v_timeout_u8 = BMI160_INIT_VALUE;
s16 offsetx = BMI160_INIT_VALUE;
s16 offsety = BMI160_INIT_VALUE;
s16 offsetz = BMI160_INIT_VALUE;
u8 focstatus = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		v_status_s8 = bmi160_set_gyro_offset_enable(
		GYRO_OFFSET_ENABLE);
		if (v_status_s8 == SUCCESS) {
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_FOC_GYRO_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_FOC_GYRO_ENABLE,
				v_foc_gyro_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_FOC_GYRO_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			/* trigger the FOC need to write 0x03
			in the register 0x7e*/
			com_rslt += bmi160_set_command_register
			(START_FOC_ACCEL_GYRO);

			com_rslt += bmi160_get_foc_rdy(&focstatus);
			if ((com_rslt != SUCCESS) ||
			(focstatus != BMI160_FOC_STAT_HIGH)) {
				while ((com_rslt != SUCCESS) ||
				(focstatus != BMI160_FOC_STAT_HIGH
				&& v_timeout_u8 <
				BMI160_MAXIMUM_TIMEOUT)) {
					p_bmi160->delay_msec(
					BMI160_DELAY_SETTLING_TIME);
					com_rslt = bmi160_get_foc_rdy(
					&focstatus);
					v_timeout_u8++;
				}
			}
			if ((com_rslt == SUCCESS) &&
			(focstatus == BMI160_FOC_STAT_HIGH)) {
				com_rslt +=
				bmi160_get_gyro_offset_compensation_xaxis
				(&offsetx);
				*v_gyro_off_x_s16 = offsetx;

				com_rslt +=
				bmi160_get_gyro_offset_compensation_yaxis
				(&offsety);
				*v_gyro_off_y_s16 = offsety;

				com_rslt +=
				bmi160_get_gyro_offset_compensation_zaxis(
				&offsetz);
				*v_gyro_off_z_s16 = offsetz;
			}
		} else {
		com_rslt = ERROR;
		}
	}
return com_rslt;
}
 /*!
 *	@brief This API read NVM program enable
 *	from the register 0x6A bit 1
 *
 *  @param v_nvm_prog_u8 : The value of NVM program enable
 *  Value  |  Description
 * --------|-------------
 *   0     |  DISABLE
 *   1     |  ENABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_prog_enable(
u8 *v_nvm_prog_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read NVM program*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_CONFIG_NVM_PROG_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_nvm_prog_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_CONFIG_NVM_PROG_ENABLE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write NVM program enable
 *	from the register 0x6A bit 1
 *
 *  @param v_nvm_prog_u8 : The value of NVM program enable
 *  Value  |  Description
 * --------|-------------
 *   0     |  DISABLE
 *   1     |  ENABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_prog_enable(
u8 v_nvm_prog_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_nvm_prog_u8 <= BMI160_MAX_VALUE_NVM_PROG) {
			/* write the NVM program*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_CONFIG_NVM_PROG_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_CONFIG_NVM_PROG_ENABLE,
				v_nvm_prog_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_CONFIG_NVM_PROG_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 * @brief This API read to configure SPI
 * Interface Mode for primary and OIS interface
 * from the register 0x6B bit 0
 *
 *  @param v_spi3_u8 : The value of SPI mode selection
 *  Value  |  Description
 * --------|-------------
 *   0     |  SPI 4-wire mode
 *   1     |  SPI 3-wire mode
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi3(
u8 *v_spi3_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read SPI mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_SPI3__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_spi3_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_IF_CONFIG_SPI3);
		}
	return com_rslt;
}
/*!
 * @brief This API write to configure SPI
 * Interface Mode for primary and OIS interface
 * from the register 0x6B bit 0
 *
 *  @param v_spi3_u8 : The value of SPI mode selection
 *  Value  |  Description
 * --------|-------------
 *   0     |  SPI 4-wire mode
 *   1     |  SPI 3-wire mode
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi3(
u8 v_spi3_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_spi3_u8 <= BMI160_MAX_VALUE_SPI3) {
			/* write SPI mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_SPI3__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_IF_CONFIG_SPI3,
				v_spi3_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONFIG_SPI3__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read I2C Watchdog timer
 *	from the register 0x70 bit 1
 *
 *  @param v_i2c_wdt_u8 : The value of I2C watch dog timer
 *  Value  |  Description
 * --------|-------------
 *   0     |  I2C watchdog v_timeout_u8 after 1 ms
 *   1     |  I2C watchdog v_timeout_u8 after 50 ms
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_select(
u8 *v_i2c_wdt_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read I2C watch dog timer */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_I2C_WDT_SELECT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_i2c_wdt_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_IF_CONFIG_I2C_WDT_SELECT);
		}
	return com_rslt;
}
/*!
 *	@brief This API write I2C Watchdog timer
 *	from the register 0x70 bit 1
 *
 *  @param v_i2c_wdt_u8 : The value of I2C watch dog timer
 *  Value  |  Description
 * --------|-------------
 *   0     |  I2C watchdog v_timeout_u8 after 1 ms
 *   1     |  I2C watchdog v_timeout_u8 after 50 ms
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_wdt_select(
u8 v_i2c_wdt_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_i2c_wdt_u8 <= BMI160_MAX_VALUE_I2C_WDT) {
			/* write I2C watch dog timer */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_I2C_WDT_SELECT__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_IF_CONFIG_I2C_WDT_SELECT,
				v_i2c_wdt_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONFIG_I2C_WDT_SELECT__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read I2C watchdog enable
 *	from the register 0x70 bit 2
 *
 *  @param v_i2c_wdt_u8 : The value of I2C watchdog enable
 *  Value  |  Description
 * --------|-------------
 *   0     |  DISABLE
 *   1     |  ENABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_i2c_wdt_enable(
u8 *v_i2c_wdt_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read i2c watch dog eneble */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_I2C_WDT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_i2c_wdt_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_IF_CONFIG_I2C_WDT_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write I2C watchdog enable
 *	from the register 0x70 bit 2
 *
 *  @param v_i2c_wdt_u8 : The value of I2C watchdog enable
 *  Value  |  Description
 * --------|-------------
 *   0     |  DISABLE
 *   1     |  ENABLE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_i2c_wdt_enable(
u8 v_i2c_wdt_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_i2c_wdt_u8 <= BMI160_MAX_VALUE_I2C_WDT) {
			/* write i2c watch dog eneble */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_I2C_WDT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_IF_CONFIG_I2C_WDT_ENABLE,
				v_i2c_wdt_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONFIG_I2C_WDT_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 * @brief This API read I2C interface configuration(if) moe
 * from the register 0x6B bit 4 and 5
 *
 *  @param  v_if_mode_u8 : The value of interface configuration mode
 *  Value  |  Description
 * --------|-------------
 *   0x00  |  Primary interface:autoconfig / secondary interface:off
 *   0x01  |  Primary interface:I2C / secondary interface:OIS
 *   0x02  |  Primary interface:autoconfig/secondary interface:Magnetometer
 *   0x03  |   Reserved
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_if_mode(
u8 *v_if_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read if mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_IF_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_if_mode_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_IF_CONFIG_IF_MODE);
		}
	return com_rslt;
}
/*!
 * @brief This API write I2C interface configuration(if) moe
 * from the register 0x6B bit 4 and 5
 *
 *  @param  v_if_mode_u8 : The value of interface configuration mode
 *  Value  |  Description
 * --------|-------------
 *   0x00  |  Primary interface:autoconfig / secondary interface:off
 *   0x01  |  Primary interface:I2C / secondary interface:OIS
 *   0x02  |  Primary interface:autoconfig/secondary interface:Magnetometer
 *   0x03  |   Reserved
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_if_mode(
u8 v_if_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_if_mode_u8 <= BMI160_MAX_IF_MODE) {
			/* write if mode*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_IF_CONFIG_IF_MODE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_IF_CONFIG_IF_MODE,
				v_if_mode_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_IF_CONFIG_IF_MODE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read gyro sleep trigger
 *	from the register 0x6C bit 0 to 2
 *
 *  @param v_gyro_sleep_trigger_u8 : The value of gyro sleep trigger
 *  Value  |  Description
 * --------|-------------
 *   0x00  | nomotion: no / Not INT1 pin: no / INT2 pin: no
 *   0x01  | nomotion: no / Not INT1 pin: no / INT2 pin: yes
 *   0x02  | nomotion: no / Not INT1 pin: yes / INT2 pin: no
 *   0x03  | nomotion: no / Not INT1 pin: yes / INT2 pin: yes
 *   0x04  | nomotion: yes / Not INT1 pin: no / INT2 pin: no
 *   0x05  | anymotion: yes / Not INT1 pin: no / INT2 pin: yes
 *   0x06  | anymotion: yes / Not INT1 pin: yes / INT2 pin: no
 *   0x07  | anymotion: yes / Not INT1 pin: yes / INT2 pin: yes
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_trigger(
u8 *v_gyro_sleep_trigger_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro sleep trigger */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_sleep_trigger_u8 =
			BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_GYRO_SLEEP_TRIGGER);
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro sleep trigger
 *	from the register 0x6C bit 0 to 2
 *
 *  @param v_gyro_sleep_trigger_u8 : The value of gyro sleep trigger
 *  Value  |  Description
 * --------|-------------
 *   0x00  | nomotion: no / Not INT1 pin: no / INT2 pin: no
 *   0x01  | nomotion: no / Not INT1 pin: no / INT2 pin: yes
 *   0x02  | nomotion: no / Not INT1 pin: yes / INT2 pin: no
 *   0x03  | nomotion: no / Not INT1 pin: yes / INT2 pin: yes
 *   0x04  | nomotion: yes / Not INT1 pin: no / INT2 pin: no
 *   0x05  | anymotion: yes / Not INT1 pin: no / INT2 pin: yes
 *   0x06  | anymotion: yes / Not INT1 pin: yes / INT2 pin: no
 *   0x07  | anymotion: yes / Not INT1 pin: yes / INT2 pin: yes
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_trigger(
u8 v_gyro_sleep_trigger_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_sleep_trigger_u8 <= BMI160_MAX_GYRO_SLEEP_TIGGER) {
			/* write gyro sleep trigger */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_SLEEP_TRIGGER,
				v_gyro_sleep_trigger_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SLEEP_TRIGGER__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read gyro wakeup trigger
 *	from the register 0x6C bit 3 and 4
 *
 *  @param v_gyro_wakeup_trigger_u8 : The value of gyro wakeup trigger
 *  Value  |  Description
 * --------|-------------
 *   0x00  | anymotion: no / INT1 pin: no
 *   0x01  | anymotion: no / INT1 pin: yes
 *   0x02  | anymotion: yes / INT1 pin: no
 *   0x03  | anymotion: yes / INT1 pin: yes
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_trigger(
u8 *v_gyro_wakeup_trigger_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro wakeup trigger */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_wakeup_trigger_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_GYRO_WAKEUP_TRIGGER);
	  }
	return com_rslt;
}
/*!
 *	@brief This API write gyro wakeup trigger
 *	from the register 0x6C bit 3 and 4
 *
 *  @param v_gyro_wakeup_trigger_u8 : The value of gyro wakeup trigger
 *  Value  |  Description
 * --------|-------------
 *   0x00  | anymotion: no / INT1 pin: no
 *   0x01  | anymotion: no / INT1 pin: yes
 *   0x02  | anymotion: yes / INT1 pin: no
 *   0x03  | anymotion: yes / INT1 pin: yes
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_trigger(
u8 v_gyro_wakeup_trigger_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_wakeup_trigger_u8
		<= BMI160_MAX_GYRO_WAKEUP_TRIGGER) {
			/* write gyro wakeup trigger */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_WAKEUP_TRIGGER,
				v_gyro_wakeup_trigger_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_WAKEUP_TRIGGER__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read Target state for gyro sleep mode
 *	from the register 0x6C bit 5
 *
 *  @param v_gyro_sleep_state_u8 : The value of gyro sleep mode
 *  Value  |  Description
 * --------|-------------
 *   0x00  | Sleep transition to fast wake up state
 *   0x01  | Sleep transition to suspend state
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_sleep_state(
u8 *v_gyro_sleep_state_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro sleep state*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_STATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_sleep_state_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_GYRO_SLEEP_STATE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write Target state for gyro sleep mode
 *	from the register 0x6C bit 5
 *
 *  @param v_gyro_sleep_state_u8 : The value of gyro sleep mode
 *  Value  |  Description
 * --------|-------------
 *   0x00  | Sleep transition to fast wake up state
 *   0x01  | Sleep transition to suspend state
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_sleep_state(
u8 v_gyro_sleep_state_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_sleep_state_u8 <= BMI160_MAX_VALUE_SLEEP_STATE) {
			/* write gyro sleep state*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SLEEP_STATE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_SLEEP_STATE,
				v_gyro_sleep_state_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SLEEP_STATE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read gyro wakeup interrupt
 *	from the register 0x6C bit 6
 *
 *  @param v_gyro_wakeup_intr_u8 : The valeu of gyro wakeup interrupt
 *  Value  |  Description
 * --------|-------------
 *   0x00  | DISABLE
 *   0x01  | ENABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_wakeup_intr(
u8 *v_gyro_wakeup_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro wakeup interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_wakeup_intr_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_GYRO_WAKEUP_INTR);
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro wakeup interrupt
 *	from the register 0x6C bit 6
 *
 *  @param v_gyro_wakeup_intr_u8 : The valeu of gyro wakeup interrupt
 *  Value  |  Description
 * --------|-------------
 *   0x00  | DISABLE
 *   0x01  | ENABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_wakeup_intr(
u8 v_gyro_wakeup_intr_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_wakeup_intr_u8 <= BMI160_MAX_VALUE_WAKEUP_INTR) {
			/* write gyro wakeup interrupt */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_WAKEUP_INTR__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_WAKEUP_INTR,
				v_gyro_wakeup_intr_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_WAKEUP_INTR__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 * @brief This API read accel select axis to be self-test
 *
 *  @param v_accel_selftest_axis_u8 :
 *	The value of accel self test axis selection
 *  Value  |  Description
 * --------|-------------
 *   0x00  | disabled
 *   0x01  | x-axis
 *   0x02  | y-axis
 *   0x03  | z-axis
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_selftest_axis(
u8 *v_accel_selftest_axis_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel self test axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACCEL_SELFTEST_AXIS__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_selftest_axis_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_ACCEL_SELFTEST_AXIS);
		}
	return com_rslt;
}
/*!
 * @brief This API write accel select axis to be self-test
 *
 *  @param v_accel_selftest_axis_u8 :
 *	The value of accel self test axis selection
 *  Value  |  Description
 * --------|-------------
 *   0x00  | disabled
 *   0x01  | x-axis
 *   0x02  | y-axis
 *   0x03  | z-axis
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_axis(
u8 v_accel_selftest_axis_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_accel_selftest_axis_u8
		<= BMI160_MAX_ACCEL_SELFTEST_AXIS) {
			/* write accel self test axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACCEL_SELFTEST_AXIS__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_ACCEL_SELFTEST_AXIS,
				v_accel_selftest_axis_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACCEL_SELFTEST_AXIS__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel self test axis sign
 *	from the register 0x6D bit 2
 *
 *  @param v_accel_selftest_sign_u8: The value of accel self test axis sign
 *  Value  |  Description
 * --------|-------------
 *   0x00  | negative
 *   0x01  | positive
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_selftest_sign(
u8 *v_accel_selftest_sign_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel self test axis sign*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_selftest_sign_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_ACCEL_SELFTEST_SIGN);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel self test axis sign
 *	from the register 0x6D bit 2
 *
 *  @param v_accel_selftest_sign_u8: The value of accel self test axis sign
 *  Value  |  Description
 * --------|-------------
 *   0x00  | negative
 *   0x01  | positive
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_sign(
u8 v_accel_selftest_sign_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_accel_selftest_sign_u8 <=
		BMI160_MAX_VALUE_SELFTEST_SIGN) {
			/* write accel self test axis sign*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_ACCEL_SELFTEST_SIGN,
				v_accel_selftest_sign_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel self test amplitude
 *	from the register 0x6D bit 3
 *        select amplitude of the selftest deflection:
 *
 *  @param v_accel_selftest_amp_u8 : The value of accel self test amplitude
 *  Value  |  Description
 * --------|-------------
 *   0x00  | LOW
 *   0x01  | HIGH
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_selftest_amp(
u8 *v_accel_selftest_amp_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read  self test amplitude*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_SELFTEST_AMP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_selftest_amp_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_SELFTEST_AMP);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel self test amplitude
 *	from the register 0x6D bit 3
 *        select amplitude of the selftest deflection:
 *
 *  @param v_accel_selftest_amp_u8 : The value of accel self test amplitude
 *  Value  |  Description
 * --------|-------------
 *   0x00  | LOW
 *   0x01  | HIGH
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_amp(
u8 v_accel_selftest_amp_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_accel_selftest_amp_u8 <=
		BMI160_MAX_VALUE_SELFTEST_AMP) {
			/* write  self test amplitude*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_SELFTEST_AMP__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_SELFTEST_AMP,
				v_accel_selftest_amp_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_SELFTEST_AMP__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read gyro self test trigger
 *
 *	@param v_gyro_selftest_start_u8: The value of gyro self test start
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_selftest_start(
u8 *v_gyro_selftest_start_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro self test start */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SELFTEST_START__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_selftest_start_u8 = BMI160_GET_BITSLICE(
			v_data_u8,
			BMI160_USER_GYRO_SELFTEST_START);
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro self test trigger
 *
 *	@param v_gyro_selftest_start_u8: The value of gyro self test start
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_selftest_start(
u8 v_gyro_selftest_start_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_gyro_selftest_start_u8 <=
		BMI160_MAX_VALUE_SELFTEST_START) {
			/* write gyro self test start */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_GYRO_SELFTEST_START__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_GYRO_SELFTEST_START,
				v_gyro_selftest_start_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_GYRO_SELFTEST_START__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 * @brief This API read primary interface selection I2C or SPI
 *	from the register 0x70 bit 0
 *
 *  @param v_spi_enable_u8: The value of Interface selection
 *  Value  |  Description
 * --------|-------------
 *   0x00  | I2C Enable
 *   0x01  | I2C DISBALE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spi_enable(u8 *v_spi_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read interface section*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_SPI_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_spi_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_NV_CONFIG_SPI_ENABLE);
		}
	return com_rslt;
}
 /*!
 * @brief This API write primary interface selection I2C or SPI
 *	from the register 0x70 bit 0
 *
 *  @param v_spi_enable_u8: The value of Interface selection
 *  Value  |  Description
 * --------|-------------
 *   0x00  | I2C Enable
 *   0x01  | I2C DISBALE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spi_enable(u8 v_spi_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write interface section*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_SPI_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_NV_CONFIG_SPI_ENABLE,
				v_spi_enable_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_NV_CONFIG_SPI_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
 /*!
 *	@brief This API read the spare zero
 *	form register 0x70 bit 3
 *
 *
 *  @param v_spare0_trim_u8: The value of spare zero
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_spare0_trim(u8 *v_spare0_trim_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read spare zero*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_SPARE0__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_spare0_trim_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_NV_CONFIG_SPARE0);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write the spare zero
 *	form register 0x70 bit 3
 *
 *
 *  @param v_spare0_trim_u8: The value of spare zero
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_spare0_trim(u8 v_spare0_trim_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write  spare zero*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_SPARE0__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_NV_CONFIG_SPARE0,
				v_spare0_trim_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONFIG_SPARE0__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
 /*!
 *	@brief This API read the NVM counter
 *	form register 0x70 bit 4 to 7
 *
 *
 *  @param v_nvm_counter_u8: The value of NVM counter
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_nvm_counter(u8 *v_nvm_counter_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read NVM counter*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_NVM_COUNTER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_nvm_counter_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_NV_CONFIG_NVM_COUNTER);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write the NVM counter
 *	form register 0x70 bit 4 to 7
 *
 *
 *  @param v_nvm_counter_u8: The value of NVM counter
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_nvm_counter(
u8 v_nvm_counter_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write NVM counter*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_NV_CONFIG_NVM_COUNTER__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_NV_CONFIG_NVM_COUNTER,
				v_nvm_counter_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_NV_CONFIG_NVM_COUNTER__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API read accel manual offset compensation of x axis
 *	from the register 0x71 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_x_s8:
 *	The value of accel manual offset compensation of x axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_compensation_xaxis(
s8 *v_accel_off_x_s8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel manual offset compensation of x axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_0_ACCEL_OFF_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_off_x_s8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_OFFSET_0_ACCEL_OFF_X);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel manual offset compensation of x axis
 *	from the register 0x71 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_x_s8:
 *	The value of accel manual offset compensation of x axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_xaxis(
s8 v_accel_off_x_s8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* enable accel offset */
		v_status_s8 = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (v_status_s8 == SUCCESS) {
			/* write accel manual offset compensation of x axis*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_0_ACCEL_OFF_X__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(
				v_data_u8,
				BMI160_USER_OFFSET_0_ACCEL_OFF_X,
				v_accel_off_x_s8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_0_ACCEL_OFF_X__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt =  ERROR;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel manual offset compensation of y axis
 *	from the register 0x72 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_y_s8:
 *	The value of accel manual offset compensation of y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_compensation_yaxis(
s8 *v_accel_off_y_s8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel manual offset compensation of y axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_1_ACCEL_OFF_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_off_y_s8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_OFFSET_1_ACCEL_OFF_Y);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel manual offset compensation of y axis
 *	from the register 0x72 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_y_s8:
 *	The value of accel manual offset compensation of y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_yaxis(
s8 v_accel_off_y_s8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* enable accel offset */
		v_status_s8 = bmi160_set_accel_offset_enable(
		ACCEL_OFFSET_ENABLE);
		if (v_status_s8 == SUCCESS) {
			/* write accel manual offset compensation of y axis*/
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_1_ACCEL_OFF_Y__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(
				v_data_u8,
				BMI160_USER_OFFSET_1_ACCEL_OFF_Y,
				v_accel_off_y_s8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_1_ACCEL_OFF_Y__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = ERROR;
		}
	}
	return com_rslt;
}
/*!
 *	@brief This API read accel manual offset compensation of z axis
 *	from the register 0x73 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_z_s8:
 *	The value of accel manual offset compensation of z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_compensation_zaxis(
s8 *v_accel_off_z_s8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel manual offset compensation of z axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_2_ACCEL_OFF_Z__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_off_z_s8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_OFFSET_2_ACCEL_OFF_Z);
		}
	return com_rslt;
}
/*!
 *	@brief This API write accel manual offset compensation of z axis
 *	from the register 0x73 bit 0 to 7
 *
 *
 *
 *  @param v_accel_off_z_s8:
 *	The value of accel manual offset compensation of z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_compensation_zaxis(
s8 v_accel_off_z_s8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_status_s8 = SUCCESS;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* enable accel offset */
			v_status_s8 = bmi160_set_accel_offset_enable(
			ACCEL_OFFSET_ENABLE);
			if (v_status_s8 == SUCCESS) {
				/* write accel manual offset
				compensation of z axis*/
				com_rslt =
				p_bmi160->BMI160_BUS_READ_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_2_ACCEL_OFF_Z__REG,
				&v_data_u8,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
				if (com_rslt == SUCCESS) {
					v_data_u8 =
					BMI160_SET_BITSLICE(v_data_u8,
					BMI160_USER_OFFSET_2_ACCEL_OFF_Z,
					v_accel_off_z_s8);
					com_rslt +=
					p_bmi160->BMI160_BUS_WRITE_FUNC(
					p_bmi160->dev_addr,
					BMI160_USER_OFFSET_2_ACCEL_OFF_Z__REG,
					&v_data_u8,
					BMI160_GEN_READ_WRITE_DATA_LENGTH);
				}
			} else {
			com_rslt = ERROR;
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API read gyro manual offset compensation of x axis
 *	from the register 0x74 bit 0 to 7 and 0x77 bit 0 and 1
 *
 *
 *
 *  @param v_gyro_off_x_s16:
 *	The value of gyro manual offset compensation of x axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_compensation_xaxis(
s16 *v_gyro_off_x_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	s16 v_data3_u8r, v_data4_u8r = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro offset x*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_3_GYRO_OFF_X__REG,
			&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data1_u8r = BMI160_GET_BITSLICE(v_data1_u8r,
			BMI160_USER_OFFSET_3_GYRO_OFF_X);
			com_rslt += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_X__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data2_u8r = BMI160_GET_BITSLICE(v_data2_u8r,
			BMI160_USER_OFFSET_6_GYRO_OFF_X);
			v_data3_u8r = v_data2_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_14_BITS;
			v_data4_u8r =  v_data1_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
			v_data3_u8r = v_data3_u8r | v_data4_u8r;
			*v_gyro_off_x_s16 = v_data3_u8r
			>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro manual offset compensation of x axis
 *	from the register 0x74 bit 0 to 7 and 0x77 bit 0 and 1
 *
 *
 *
 *  @param v_gyro_off_x_s16:
 *	The value of gyro manual offset compensation of x axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_xaxis(
s16 v_gyro_off_x_s16)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data1_u8r, v_data2_u8r = BMI160_INIT_VALUE;
u16 v_data3_u8r = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* write gyro offset x*/
		v_status_s8 = bmi160_set_gyro_offset_enable(
		GYRO_OFFSET_ENABLE);
		if (v_status_s8 == SUCCESS) {
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_3_GYRO_OFF_X__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data1_u8r =
				((s8) (v_gyro_off_x_s16 &
				BMI160_GYRO_MANUAL_OFFSET_0_7));
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_3_GYRO_OFF_X,
				v_data1_u8r);
				/* write 0x74 bit 0 to 7*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_3_GYRO_OFF_X__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			com_rslt += p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_X__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data3_u8r =
				(u16) (v_gyro_off_x_s16 &
				BMI160_GYRO_MANUAL_OFFSET_8_9);
				v_data1_u8r = (u8)(v_data3_u8r
				>> BMI160_SHIFT_BIT_POSITION_BY_08_BITS);
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_6_GYRO_OFF_X,
				v_data1_u8r);
				/* write 0x77 bit 0 and 1*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYRO_OFF_X__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		return ERROR;
		}
	}
return com_rslt;
}
/*!
 *	@brief This API read gyro manual offset compensation of y axis
 *	from the register 0x75 bit 0 to 7 and 0x77 bit 2 and 3
 *
 *
 *
 *  @param v_gyro_off_y_s16:
 *	The value of gyro manual offset compensation of y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_compensation_yaxis(
s16 *v_gyro_off_y_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	s16 v_data3_u8r, v_data4_u8r = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro offset y*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_4_GYRO_OFF_Y__REG,
			&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data1_u8r = BMI160_GET_BITSLICE(v_data1_u8r,
			BMI160_USER_OFFSET_4_GYRO_OFF_Y);
			com_rslt += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_Y__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data2_u8r = BMI160_GET_BITSLICE(v_data2_u8r,
			BMI160_USER_OFFSET_6_GYRO_OFF_Y);
			v_data3_u8r = v_data2_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_14_BITS;
			v_data4_u8r =  v_data1_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
			v_data3_u8r = v_data3_u8r | v_data4_u8r;
			*v_gyro_off_y_s16 = v_data3_u8r
			>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro manual offset compensation of y axis
 *	from the register 0x75 bit 0 to 7 and 0x77 bit 2 and 3
 *
 *
 *
 *  @param v_gyro_off_y_s16:
 *	The value of gyro manual offset compensation of y axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_yaxis(
s16 v_gyro_off_y_s16)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data1_u8r, v_data2_u8r = BMI160_INIT_VALUE;
u16 v_data3_u8r = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* enable gyro offset bit */
		v_status_s8 = bmi160_set_gyro_offset_enable(
		GYRO_OFFSET_ENABLE);
		/* write gyro offset y*/
		if (v_status_s8 == SUCCESS) {
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_4_GYRO_OFF_Y__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data1_u8r =
				((s8) (v_gyro_off_y_s16 &
				BMI160_GYRO_MANUAL_OFFSET_0_7));
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_4_GYRO_OFF_Y,
				v_data1_u8r);
				/* write 0x75 bit 0 to 7*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_4_GYRO_OFF_Y__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			com_rslt += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_Y__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data3_u8r =
				(u16) (v_gyro_off_y_s16 &
				BMI160_GYRO_MANUAL_OFFSET_8_9);
				v_data1_u8r = (u8)(v_data3_u8r
				>> BMI160_SHIFT_BIT_POSITION_BY_08_BITS);
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_6_GYRO_OFF_Y,
				v_data1_u8r);
				/* write 0x77 bit 2 and 3*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYRO_OFF_Y__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		return ERROR;
		}
	}
return com_rslt;
}
/*!
 *	@brief This API read gyro manual offset compensation of z axis
 *	from the register 0x76 bit 0 to 7 and 0x77 bit 4 and 5
 *
 *
 *
 *  @param v_gyro_off_z_s16:
 *	The value of gyro manual offset compensation of z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_compensation_zaxis(
s16 *v_gyro_off_z_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	s16 v_data3_u8r, v_data4_u8r = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro manual offset z axis*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_5_GYRO_OFF_Z__REG,
			&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data1_u8r = BMI160_GET_BITSLICE
			(v_data1_u8r,
			BMI160_USER_OFFSET_5_GYRO_OFF_Z);
			com_rslt +=
			p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_Z__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			v_data2_u8r = BMI160_GET_BITSLICE(
			v_data2_u8r,
			BMI160_USER_OFFSET_6_GYRO_OFF_Z);
			v_data3_u8r = v_data2_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_14_BITS;
			v_data4_u8r =  v_data1_u8r
			<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
			v_data3_u8r = v_data3_u8r | v_data4_u8r;
			*v_gyro_off_z_s16 = v_data3_u8r
			>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS;
		}
	return com_rslt;
}
/*!
 *	@brief This API write gyro manual offset compensation of z axis
 *	from the register 0x76 bit 0 to 7 and 0x77 bit 4 and 5
 *
 *
 *
 *  @param v_gyro_off_z_s16:
 *	The value of gyro manual offset compensation of z axis
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_compensation_zaxis(
s16 v_gyro_off_z_s16)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data1_u8r, v_data2_u8r = BMI160_INIT_VALUE;
u16 v_data3_u8r = BMI160_INIT_VALUE;
u8 v_status_s8 = SUCCESS;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
	} else {
		/* enable gyro offset*/
		v_status_s8 = bmi160_set_gyro_offset_enable(
		GYRO_OFFSET_ENABLE);
		/* write gyro manual offset z axis*/
		if (v_status_s8 == SUCCESS) {
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_5_GYRO_OFF_Z__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data1_u8r =
				((u8) (v_gyro_off_z_s16 &
				BMI160_GYRO_MANUAL_OFFSET_0_7));
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_5_GYRO_OFF_Z,
				v_data1_u8r);
				/* write 0x76 bit 0 to 7*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_5_GYRO_OFF_Z__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}

			com_rslt += p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_Z__REG,
			&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data3_u8r =
				(u16) (v_gyro_off_z_s16 &
				BMI160_GYRO_MANUAL_OFFSET_8_9);
				v_data1_u8r = (u8)(v_data3_u8r
				>> BMI160_SHIFT_BIT_POSITION_BY_08_BITS);
				v_data2_u8r = BMI160_SET_BITSLICE(
				v_data2_u8r,
				BMI160_USER_OFFSET_6_GYRO_OFF_Z,
				v_data1_u8r);
				/* write 0x77 bit 4 and 5*/
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYRO_OFF_Z__REG,
				&v_data2_u8r,
				BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		return ERROR;
		}
	}
return com_rslt;
}
/*!
 *	@brief This API read the accel offset enable bit
 *	from the register 0x77 bit 6
 *
 *
 *
 *  @param v_accel_off_enable_u8: The value of accel offset enable
 *  value    |  Description
 * ----------|--------------
 *   0x01    | ENABLE
 *   0x00    | DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_offset_enable(
u8 *v_accel_off_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read accel offset enable */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_ACCEL_OFF_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_accel_off_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_OFFSET_6_ACCEL_OFF_ENABLE);
		}
	return com_rslt;
}
/*!
 *	@brief This API write the accel offset enable bit
 *	from the register 0x77 bit 6
 *
 *
 *
 *  @param v_accel_off_enable_u8: The value of accel offset enable
 *  value    |  Description
 * ----------|--------------
 *   0x01    | ENABLE
 *   0x00    | DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_offset_enable(
u8 v_accel_off_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
			} else {
			/* write accel offset enable */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_ACCEL_OFF_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_OFFSET_6_ACCEL_OFF_ENABLE,
				v_accel_off_enable_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_ACCEL_OFF_ENABLE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API read the accel offset enable bit
 *	from the register 0x77 bit 7
 *
 *
 *
 *  @param v_gyro_off_enable_u8: The value of gyro offset enable
 *  value    |  Description
 * ----------|--------------
 *   0x01    | ENABLE
 *   0x00    | DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_offset_enable(
u8 *v_gyro_off_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read gyro offset*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_EN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_gyro_off_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_OFFSET_6_GYRO_OFF_EN);
		}
	return com_rslt;
}
/*!
 *	@brief This API write the accel offset enable bit
 *	from the register 0x77 bit 7
 *
 *
 *
 *  @param v_gyro_off_enable_u8: The value of gyro offset enable
 *  value    |  Description
 * ----------|--------------
 *   0x01    | ENABLE
 *   0x00    | DISABLE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_offset_enable(
u8 v_gyro_off_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write gyro offset*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_OFFSET_6_GYRO_OFF_EN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
				BMI160_USER_OFFSET_6_GYRO_OFF_EN,
				v_gyro_off_enable_u8);
				com_rslt += p_bmi160->BMI160_BUS_WRITE_FUNC(
				p_bmi160->dev_addr,
				BMI160_USER_OFFSET_6_GYRO_OFF_EN__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}
/*!
 *	@brief This API reads step counter value
 *	form the register 0x78 and 0x79
 *
 *
 *
 *
 *  @param v_step_cnt_s16 : The value of step counter
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_step_count(u16 *v_step_cnt_s16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* array having the step counter LSB and MSB data
	v_data_u8[0] - LSB
	v_data_u8[1] - MSB*/
	u8 a_data_u8r[BMI160_STEP_COUNT_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read step counter */
			com_rslt =
			p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
			BMI160_USER_STEP_COUNT_LSB__REG,
			a_data_u8r, BMI160_STEP_COUNTER_LENGTH);

			*v_step_cnt_s16 = (s16)
			((((s32)((s8)a_data_u8r[BMI160_STEP_COUNT_MSB_BYTE]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
			| (a_data_u8r[BMI160_STEP_COUNT_LSB_BYTE]));
		}
	return com_rslt;
}
 /*!
 *	@brief This API Reads
 *	step counter configuration
 *	from the register 0x7A bit 0 to 7
 *	and from the register 0x7B bit 0 to 2 and 4 to 7
 *
 *
 *  @param v_step_config_u16 : The value of step configuration
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_step_config(
u16 *v_step_config_u16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	u16 v_data3_u8r = BMI160_INIT_VALUE;
	/* Read the 0 to 7 bit*/
	com_rslt =
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ZERO__REG,
	&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Read the 8 to 10 bit*/
	com_rslt +=
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ONE_CNF1__REG,
	&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	v_data2_u8r = BMI160_GET_BITSLICE(v_data2_u8r,
	BMI160_USER_STEP_CONFIG_ONE_CNF1);
	v_data3_u8r = ((u16)((((u32)
	((u8)v_data2_u8r))
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (v_data1_u8r)));
	/* Read the 11 to 14 bit*/
	com_rslt +=
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ONE_CNF2__REG,
	&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	v_data1_u8r = BMI160_GET_BITSLICE(v_data1_u8r,
	BMI160_USER_STEP_CONFIG_ONE_CNF2);
	*v_step_config_u16 = ((u16)((((u32)
	((u8)v_data1_u8r))
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | (v_data3_u8r)));

	return com_rslt;
}
 /*!
 *	@brief This API write
 *	step counter configuration
 *	from the register 0x7A bit 0 to 7
 *	and from the register 0x7B bit 0 to 2 and 4 to 7
 *
 *
 *  @param v_step_config_u16   :
 *	the value of  Enable step configuration
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_config(
u16 v_step_config_u16)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	u16 v_data3_u16 = BMI160_INIT_VALUE;

	/* write the 0 to 7 bit*/
	v_data1_u8r = (u8)(v_step_config_u16 &
	BMI160_STEP_CONFIG_0_7);
	p_bmi160->BMI160_BUS_WRITE_FUNC
	(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ZERO__REG,
	&v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* write the 8 to 10 bit*/
	com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
	(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ONE_CNF1__REG,
	&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data3_u16 = (u16) (v_step_config_u16 &
		BMI160_STEP_CONFIG_8_10);
		v_data1_u8r = (u8)(v_data3_u16
		>> BMI160_SHIFT_BIT_POSITION_BY_08_BITS);
		v_data2_u8r = BMI160_SET_BITSLICE(v_data2_u8r,
		BMI160_USER_STEP_CONFIG_ONE_CNF1, v_data1_u8r);
		p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_STEP_CONFIG_ONE_CNF1__REG,
		&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
	/* write the 11 to 14 bit*/
	com_rslt += p_bmi160->BMI160_BUS_READ_FUNC
	(p_bmi160->dev_addr,
	BMI160_USER_STEP_CONFIG_ONE_CNF2__REG,
	&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data3_u16 = (u16) (v_step_config_u16 &
		BMI160_STEP_CONFIG_11_14);
		v_data1_u8r = (u8)(v_data3_u16
		>> BMI160_SHIFT_BIT_POSITION_BY_12_BITS);
		v_data2_u8r = BMI160_SET_BITSLICE(v_data2_u8r,
		BMI160_USER_STEP_CONFIG_ONE_CNF2, v_data1_u8r);
		p_bmi160->BMI160_BUS_WRITE_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_STEP_CONFIG_ONE_CNF2__REG,
		&v_data2_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}

	return com_rslt;
}
 /*!
 *	@brief This API read enable step counter
 *	from the register 0x7B bit 3
 *
 *
 *  @param v_step_counter_u8 : The value of step counter enable
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_step_counter_enable(
u8 *v_step_counter_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the step counter */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_step_counter_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write enable step counter
 *	from the register 0x7B bit 3
 *
 *
 *  @param v_step_counter_u8 : The value of step counter enable
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_counter_enable(u8 v_step_counter_u8)
{
/* variable used for return the status of communication result*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
if (p_bmi160 == BMI160_NULL) {
	return E_BMI160_NULL_PTR;
} else {
	if (v_step_counter_u8 <= BMI160_MAX_GYRO_STEP_COUNTER) {
		/* write the step counter */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
		(p_bmi160->dev_addr,
		BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		if (com_rslt == SUCCESS) {
			v_data_u8 =
			BMI160_SET_BITSLICE(v_data_u8,
			BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE,
			v_step_counter_u8);
			com_rslt +=
			p_bmi160->BMI160_BUS_WRITE_FUNC
			(p_bmi160->dev_addr,
			BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	} else {
	com_rslt = E_BMI160_OUT_OF_RANGE;
	}
}
	return com_rslt;
}
 /*!
 *	@brief This API set Step counter modes
 *
 *
 *  @param  v_step_mode_u8 : The value of step counter mode
 *  value    |   mode
 * ----------|-----------
 *   0       | BMI160_STEP_NORMAL_MODE
 *   1       | BMI160_STEP_SENSITIVE_MODE
 *   2       | BMI160_STEP_ROBUST_MODE
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_mode(u8 v_step_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	switch (v_step_mode_u8) {
	case BMI160_STEP_NORMAL_MODE:
		com_rslt = bmi160_set_step_config(
		STEP_CONFIG_NORMAL);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_STEP_SENSITIVE_MODE:
		com_rslt = bmi160_set_step_config(
		STEP_CONFIG_SENSITIVE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_STEP_ROBUST_MODE:
		com_rslt = bmi160_set_step_config(
		STEP_CONFIG_ROBUST);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}

	return com_rslt;
}
/*!
 *	@brief This API used to trigger the  signification motion
 *	interrupt
 *
 *
 *  @param  v_significant_u8 : The value of interrupt selection
 *  value    |  interrupt
 * ----------|-----------
 *   0       |  BMI160_MAP_INTR1
 *   1       |  BMI160_MAP_INTR2
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_map_significant_motion_intr(
u8 v_significant_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_sig_motion_u8 = BMI160_INIT_VALUE;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_any_motion_intr1_stat_u8 = BMI160_ENABLE_ANY_MOTION_INTR1;
	u8 v_any_motion_intr2_stat_u8 = BMI160_ENABLE_ANY_MOTION_INTR2;
	u8 v_any_motion_axis_stat_u8 = BMI160_ENABLE_ANY_MOTION_AXIS;
	/* enable the significant motion interrupt */
	com_rslt = bmi160_get_intr_significant_motion_select(&v_sig_motion_u8);
	if (v_sig_motion_u8 != BMI160_SIG_MOTION_STAT_HIGH)
		com_rslt += bmi160_set_intr_significant_motion_select(
		BMI160_SIG_MOTION_INTR_ENABLE);
	switch (v_significant_u8) {
	case BMI160_MAP_INTR1:
		/* interrupt */
		com_rslt += bmi160_read_reg(
		BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_any_motion_intr1_stat_u8;
		/* map the signification interrupt to any-motion interrupt1*/
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_MAP_0_INTR1_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* axis*/
		com_rslt = bmi160_read_reg(BMI160_USER_INTR_ENABLE_0_ADDR,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_any_motion_axis_stat_u8;
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_ENABLE_0_ADDR,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;

	case BMI160_MAP_INTR2:
		/* map the signification interrupt to any-motion interrupt2*/
		com_rslt += bmi160_read_reg(
		BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_any_motion_intr2_stat_u8;
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_MAP_2_INTR2_ANY_MOTION__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* axis*/
		com_rslt = bmi160_read_reg(BMI160_USER_INTR_ENABLE_0_ADDR,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_any_motion_axis_stat_u8;
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_ENABLE_0_ADDR,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;

	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;

	}
	return com_rslt;
}
/*!
 *	@brief This API used to trigger the step detector
 *	interrupt
 *
 *
 *  @param  v_step_detector_u8 : The value of interrupt selection
 *  value    |  interrupt
 * ----------|-----------
 *   0       |  BMI160_MAP_INTR1
 *   1       |  BMI160_MAP_INTR2
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_map_step_detector_intr(
u8 v_step_detector_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_step_det_u8 = BMI160_INIT_VALUE;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_low_g_intr_u81_stat_u8 = BMI160_LOW_G_INTR_STAT;
	u8 v_low_g_intr_u82_stat_u8 = BMI160_LOW_G_INTR_STAT;
	u8 v_low_g_enable_u8 = BMI160_ENABLE_LOW_G;
	/* read the v_status_s8 of step detector interrupt*/
	com_rslt = bmi160_get_step_detector_enable(&v_step_det_u8);
	if (v_step_det_u8 != BMI160_STEP_DET_STAT_HIGH)
		com_rslt += bmi160_set_step_detector_enable(
		BMI160_STEP_DETECT_INTR_ENABLE);
	switch (v_step_detector_u8) {
	case BMI160_MAP_INTR1:
		com_rslt += bmi160_read_reg(
		BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_low_g_intr_u81_stat_u8;
		/* map the step detector interrupt
		to Low-g interrupt 1*/
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_MAP_0_INTR1_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* Enable the Low-g interrupt*/
		com_rslt = bmi160_read_reg(
		BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_low_g_enable_u8;
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);

		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAP_INTR2:
		/* map the step detector interrupt
		to Low-g interrupt 1*/
		com_rslt += bmi160_read_reg(
		BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_low_g_intr_u82_stat_u8;

		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_MAP_2_INTR2_LOW_G__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* Enable the Low-g interrupt*/
		com_rslt = bmi160_read_reg(
		BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_data_u8 |= v_low_g_enable_u8;
		com_rslt += bmi160_write_reg(
		BMI160_USER_INTR_ENABLE_1_LOW_G_ENABLE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
	return com_rslt;
}
 /*!
 *	@brief This API used to clear the step counter interrupt
 *	interrupt
 *
 *
 *  @param  : None
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_clear_step_counter(void)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* clear the step counter*/
	com_rslt = bmi160_set_command_register(RESET_STEP_COUNTER);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);

	return com_rslt;

}
 /*!
 *	@brief This API writes value to the register 0x7E bit 0 to 7
 *
 *
 *  @param  v_command_reg_u8 : The value to write command register
 *  value   |  Description
 * ---------|--------------------------------------------------------
 *	0x00	|	Reserved
 *  0x03	|	Starts fast offset calibration for the accel and gyro
 *	0x10	|	Sets the PMU mode for the Accelerometer to suspend
 *	0x11	|	Sets the PMU mode for the Accelerometer to normal
 *	0x12	|	Sets the PMU mode for the Accelerometer Lowpower
 *  0x14	|	Sets the PMU mode for the Gyroscope to suspend
 *	0x15	|	Sets the PMU mode for the Gyroscope to normal
 *	0x16	|	Reserved
 *	0x17	|	Sets the PMU mode for the Gyroscope to fast start-up
 *  0x18	|	Sets the PMU mode for the Magnetometer to suspend
 *	0x19	|	Sets the PMU mode for the Magnetometer to normal
 *	0x1A	|	Sets the PMU mode for the Magnetometer to Lowpower
 *	0xB0	|	Clears all data in the FIFO
 *  0xB1	|	Resets the interrupt engine
 *	0xB2	|	step_cnt_clr Clears the step counter
 *	0xB6	|	Triggers a reset
 *	0x37	|	See extmode_en_last
 *	0x9A	|	See extmode_en_last
 *	0xC0	|	Enable the extended mode
 *  0xC4	|	Erase NVM cell
 *	0xC8	|	Load NVM cell
 *	0xF0	|	Reset acceleration data path
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_command_register(u8 v_command_reg_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write command register */
			com_rslt = p_bmi160->BMI160_BUS_WRITE_FUNC(
			p_bmi160->dev_addr,
			BMI160_CMD_COMMANDS__REG,
			&v_command_reg_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		}
	return com_rslt;
}
 /*!
 *	@brief This API read target page from the register 0x7F bit 4 and 5
 *
 *  @param v_target_page_u8: The value of target page
 *  value   |  page
 * ---------|-----------
 *   0      |  User data/configure page
 *   1      |  Chip level trim/test page
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_target_page(u8 *v_target_page_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* read the page*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
			p_bmi160->dev_addr,
			BMI160_CMD_TARGET_PAGE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			*v_target_page_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_CMD_TARGET_PAGE);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write target page from the register 0x7F bit 4 and 5
 *
 *  @param v_target_page_u8: The value of target page
 *  value   |  page
 * ---------|-----------
 *   0      |  User data/configure page
 *   1      |  Chip level trim/test page
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_target_page(u8 v_target_page_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_target_page_u8 <= BMI160_MAX_TARGET_PAGE) {
			/* write the page*/
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_CMD_TARGET_PAGE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_CMD_TARGET_PAGE,
				v_target_page_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_CMD_TARGET_PAGE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read page enable from the register 0x7F bit 7
 *
 *
 *
 *  @param v_page_enable_u8: The value of page enable
 *  value   |  page
 * ---------|-----------
 *   0      |  DISABLE
 *   1      |  ENABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_paging_enable(u8 *v_page_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read the page enable */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_CMD_PAGING_EN__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_page_enable_u8 = BMI160_GET_BITSLICE(v_data_u8,
		BMI160_CMD_PAGING_EN);
		}
	return com_rslt;
}
 /*!
 *	@brief This API write page enable from the register 0x7F bit 7
 *
 *
 *
 *  @param v_page_enable_u8: The value of page enable
 *  value   |  page
 * ---------|-----------
 *   0      |  DISABLE
 *   1      |  ENABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_paging_enable(
u8 v_page_enable_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		if (v_page_enable_u8 <= BMI160_MAX_VALUE_PAGE) {
			/* write the page enable */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_CMD_PAGING_EN__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_CMD_PAGING_EN,
				v_page_enable_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_CMD_PAGING_EN__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
		com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}
 /*!
 *	@brief This API read
 *	pull up configuration from the register 0X85 bit 4 an 5
 *
 *
 *
 *  @param v_control_pullup_u8: The value of pull up register
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_pullup_configuration(
u8 *v_control_pullup_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt  = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read pull up value */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(
		p_bmi160->dev_addr,
		BMI160_COM_C_TRIM_FIVE__REG,
		&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_control_pullup_u8 = BMI160_GET_BITSLICE(v_data_u8,
		BMI160_COM_C_TRIM_FIVE);
		}
	return com_rslt;

}
 /*!
 *	@brief This API write
 *	pull up configuration from the register 0X85 bit 4 an 5
 *
 *
 *
 *  @param v_control_pullup_u8: The value of pull up register
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_pullup_configuration(
u8 v_control_pullup_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
			/* write  pull up value */
			com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
			(p_bmi160->dev_addr,
			BMI160_COM_C_TRIM_FIVE__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				BMI160_SET_BITSLICE(v_data_u8,
				BMI160_COM_C_TRIM_FIVE,
				v_control_pullup_u8);
				com_rslt +=
				p_bmi160->BMI160_BUS_WRITE_FUNC
				(p_bmi160->dev_addr,
				BMI160_COM_C_TRIM_FIVE__REG,
				&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
	return com_rslt;
}

/*!
 *	@brief This function used for read the compensated value of mag
 *	Before start reading the mag compensated data's
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bmm150_mag_compensate_xyz(
struct bmi160_mag_xyz_s32_t *mag_comp_xyz)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	struct bmi160_mag_xyzr_t mag_xyzr;
	com_rslt = bmi160_read_mag_xyzr(&mag_xyzr);
	if (com_rslt)
		return com_rslt;
	/* Compensation for X axis */
	mag_comp_xyz->x = bmi160_bmm150_mag_compensate_X(
	mag_xyzr.x, mag_xyzr.r);

	/* Compensation for Y axis */
	mag_comp_xyz->y = bmi160_bmm150_mag_compensate_Y(
	mag_xyzr.y, mag_xyzr.r);

	/* Compensation for Z axis */
	mag_comp_xyz->z = bmi160_bmm150_mag_compensate_Z(
	mag_xyzr.z, mag_xyzr.r);

	return com_rslt;
}

/*!
 *	@brief This function used for read the compensated value of mag
 *	Before start reading the mag compensated data's
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bmm150_mag_compensate_xyz_raw(
struct bmi160_mag_xyz_s32_t *mag_comp_xyz, struct bmi160_mag_xyzr_t mag_xyzr)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	/* Compensation for X axis */
	mag_comp_xyz->x = bmi160_bmm150_mag_compensate_X(
	mag_xyzr.x, mag_xyzr.r);

	/* Compensation for Y axis */
	mag_comp_xyz->y = bmi160_bmm150_mag_compensate_Y(
	mag_xyzr.y, mag_xyzr.r);

	/* Compensation for Z axis */
	mag_comp_xyz->z = bmi160_bmm150_mag_compensate_Z(
	mag_xyzr.z, mag_xyzr.r);

	return com_rslt;
}
/*!
 *	@brief This API used to get the compensated BMM150-X data
 *	the out put of X as s32
 *	Before start reading the mag compensated X data
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *
 *  @param  v_mag_data_x_s16 : The value of mag raw X data
 *  @param  v_data_r_u16 : The value of mag R data
 *
 *	@return results of compensated X data value output as s32
 *
 */
s32 bmi160_bmm150_mag_compensate_X(s16 v_mag_data_x_s16, u16 v_data_r_u16)
{
s32 inter_retval = BMI160_INIT_VALUE;
/* no overflow */
if (v_mag_data_x_s16 != BMI160_MAG_FLIP_OVERFLOW_ADCVAL) {
	if ((v_data_r_u16 != 0)
	&& (mag_trim.dig_xyz1 != 0)) {
		inter_retval = ((s32)(((u16)
		((((s32)mag_trim.dig_xyz1)
		<< BMI160_SHIFT_BIT_POSITION_BY_14_BITS)/
		 (v_data_r_u16 != 0 ?
		 v_data_r_u16 : mag_trim.dig_xyz1))) -
		((u16)0x4000)));
	} else {
		inter_retval = BMI160_MAG_OVERFLOW_OUTPUT;
		return inter_retval;
	}
	inter_retval = ((s32)((((s32)v_mag_data_x_s16) *
			((((((((s32)mag_trim.dig_xy2) *
			((((s32)inter_retval) *
			((s32)inter_retval))
			>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS)) +
			 (((s32)inter_retval) *
			  ((s32)(((s16)mag_trim.dig_xy1)
			  << BMI160_SHIFT_BIT_POSITION_BY_07_BITS))))
			  >> BMI160_SHIFT_BIT_POSITION_BY_09_BITS) +
		   ((s32)0x100000)) *
		  ((s32)(((s16)mag_trim.dig_x2) +
		  ((s16)0xA0))))
		  >> BMI160_SHIFT_BIT_POSITION_BY_12_BITS))
		  >> BMI160_SHIFT_BIT_POSITION_BY_13_BITS)) +
		(((s16)mag_trim.dig_x1)
		<< BMI160_SHIFT_BIT_POSITION_BY_03_BITS);
	/* check the overflow output */
	if (inter_retval == (s32)BMI160_MAG_OVERFLOW_OUTPUT)
		inter_retval = BMI160_MAG_OVERFLOW_OUTPUT_S32;
} else {
	/* overflow */
	inter_retval = BMI160_MAG_OVERFLOW_OUTPUT;
}
return inter_retval;
}
/*!
 *	@brief This API used to get the compensated BMM150-Y data
 *	the out put of Y as s32
 *	Before start reading the mag compensated Y data
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *
 *  @param  v_mag_data_y_s16 : The value of mag raw Y data
 *  @param  v_data_r_u16 : The value of mag R data
 *
 *	@return results of compensated Y data value output as s32
 */
s32 bmi160_bmm150_mag_compensate_Y(s16 v_mag_data_y_s16, u16 v_data_r_u16)
{
s32 inter_retval = BMI160_INIT_VALUE;
/* no overflow */
if (v_mag_data_y_s16 != BMI160_MAG_FLIP_OVERFLOW_ADCVAL) {
	if ((v_data_r_u16 != 0)
	&& (mag_trim.dig_xyz1 != 0)) {
		inter_retval = ((s32)(((u16)(((
		(s32)mag_trim.dig_xyz1)
		<< BMI160_SHIFT_BIT_POSITION_BY_14_BITS) /
		(v_data_r_u16 != 0 ?
		 v_data_r_u16 : mag_trim.dig_xyz1))) -
		((u16)0x4000)));
		} else {
			inter_retval = BMI160_MAG_OVERFLOW_OUTPUT;
			return inter_retval;
		}
	inter_retval = ((s32)((((s32)v_mag_data_y_s16) * ((((((((s32)
		mag_trim.dig_xy2) * ((((s32) inter_retval) *
		((s32)inter_retval)) >> BMI160_SHIFT_BIT_POSITION_BY_07_BITS))
		+ (((s32)inter_retval) *
		((s32)(((s16)mag_trim.dig_xy1)
		<< BMI160_SHIFT_BIT_POSITION_BY_07_BITS))))
		>> BMI160_SHIFT_BIT_POSITION_BY_09_BITS) +
		((s32)0x100000))
		* ((s32)(((s16)mag_trim.dig_y2)
		+ ((s16)0xA0))))
		>> BMI160_SHIFT_BIT_POSITION_BY_12_BITS))
		>> BMI160_SHIFT_BIT_POSITION_BY_13_BITS)) +
		(((s16)mag_trim.dig_y1)
		<< BMI160_SHIFT_BIT_POSITION_BY_03_BITS);
	/* check the overflow output */
	if (inter_retval == (s32)BMI160_MAG_OVERFLOW_OUTPUT)
		inter_retval = BMI160_MAG_OVERFLOW_OUTPUT_S32;
} else {
	/* overflow */
	inter_retval = BMI160_MAG_OVERFLOW_OUTPUT;
}
return inter_retval;
}
/*!
 *	@brief This API used to get the compensated BMM150-Z data
 *	the out put of Z as s32
 *	Before start reading the mag compensated Z data
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *
 *  @param  v_mag_data_z_s16 : The value of mag raw Z data
 *  @param  v_data_r_u16 : The value of mag R data
 *
 *	@return results of compensated Z data value output as s32
 */
s32 bmi160_bmm150_mag_compensate_Z(s16 v_mag_data_z_s16, u16 v_data_r_u16)
{
	s32 retval = BMI160_INIT_VALUE;

	if (v_mag_data_z_s16 != BMI160_MAG_HALL_OVERFLOW_ADCVAL) {
		if ((v_data_r_u16 != 0)
		   && (mag_trim.dig_z2 != 0)
		/*   && (mag_trim.dig_z3 != 0)*/
		   && (mag_trim.dig_z1 != 0)
		   && (mag_trim.dig_xyz1 != 0)) {
			retval = (((((s32)(v_mag_data_z_s16 - mag_trim.dig_z4))
			<< BMI160_SHIFT_BIT_POSITION_BY_15_BITS) -
			((((s32)mag_trim.dig_z3) *
			((s32)(((s16)v_data_r_u16) -
			((s16)mag_trim.dig_xyz1))))
			>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS))/
			(mag_trim.dig_z2 +
			((s16)(((((s32)mag_trim.dig_z1) *
			((((s16)v_data_r_u16)
			<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT))) +
			(1 << BMI160_SHIFT_BIT_POSITION_BY_15_BITS))
			>> BMI160_SHIFT_BIT_POSITION_BY_16_BITS))));
		}
	} else {
		retval = BMI160_MAG_OVERFLOW_OUTPUT;
	}
		return retval;
}
 /*!
 *	@brief This function used for initialize the bmm150 sensor
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bmm150_mag_interface_init(void)
{
	/* This variable used for provide the communication
	results*/
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	u8 v_pull_value_u8 = BMI160_INIT_VALUE;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* accel operation mode to normal*/
	com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* write the mag power mode as NORMAL*/
	com_rslt += bmi160_set_mag_interface_normal();

	/* register 0x7E write the 0x37, 0x9A and 0x30*/
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_ONE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_TWO);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_THREE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*switch the page1*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	com_rslt += bmi160_set_paging_enable(BMI160_WRITE_ENABLE_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_paging_enable(&v_data_u8);
	/* enable the pullup configuration from
	the register 0x05 bit 4 and 5 as 10*/
	bmi160_get_pullup_configuration(&v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	v_pull_value_u8 = v_pull_value_u8 | BMI160_PULL_UP_DATA;
	com_rslt += bmi160_set_pullup_configuration(v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*switch the page0*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE0);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	/* Write the BMM150 i2c address*/
	com_rslt += bmi160_set_i2c_device_addr(BMI160_AUX_BMM150_I2C_ADDRESS);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable the mag interface to manual mode*/
	com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_ENABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	/*Enable the MAG interface */
	com_rslt += bmi160_set_if_mode(BMI160_ENABLE_MAG_IF_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_if_mode(&v_data_u8);
	/* Mag normal mode*/
	com_rslt += bmi160_bmm150_mag_wakeup();
	printk(KERN_INFO "com_rslt:%d, <%s><%d>\n",
		com_rslt, __func__, __LINE__);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Read the BMM150 device id is 0x32*/
	/*com_rslt += bmi160_set_mag_read_addr(BMI160_BMM150_CHIP_ID);*/
	/*p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);*/
	/*com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);*/
	/**v_chip_id_u8 = v_data_u8;*/
	/*p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);*/
	/* write the power mode register*/
	com_rslt += bmi160_set_mag_write_data(BMI160_BMM_POWER_MODE_REG);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*write 0x4C register to write set power mode to normal*/
	com_rslt += bmi160_set_mag_write_addr(
	BMI160_BMM150_POWE_MODE_REG);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read the mag trim values*/
	com_rslt += bmi160_read_bmm150_mag_trim();
	printk(KERN_INFO "com_rslt:%d, <%s><%d>\n",
		com_rslt, __func__, __LINE__);
	/* To avoid the auto mode enable when manual mode operation running*/
	V_bmm150_maual_auto_condition_u8 = BMI160_MANUAL_ENABLE;
	/* write the XY and Z repetitions*/
	com_rslt += bmi160_set_bmm150_mag_presetmode(
	BMI160_MAG_PRESETMODE_REGULAR);
	printk(KERN_INFO "com_rslt:%d, <%s><%d>\n",
		com_rslt, __func__, __LINE__);
	/* To avoid the auto mode enable when manual mode operation running*/
	V_bmm150_maual_auto_condition_u8 = BMI160_MANUAL_DISABLE;
	/* Set the power mode of mag as force mode*/
	/* The data have to write for the register
	It write the value in the register 0x4F */
	com_rslt += bmi160_set_mag_write_data(BMI160_BMM150_FORCE_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	printk(KERN_INFO "com_rslt:%d, <%s><%d>\n",
		com_rslt, __func__, __LINE__);
	/* write into power mode register*/
	com_rslt += bmi160_set_mag_write_addr(
	BMI160_BMM150_POWE_MODE_REG);
	/* write the mag v_data_bw_u8 as 25Hz*/
	com_rslt += bmi160_set_mag_output_data_rate(
	BMI160_MAG_OUTPUT_DATA_RATE_25HZ);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	/* When mag interface is auto mode - The mag read address
	starts the register 0x42*/
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_BMM150_DATA_REG);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable mag interface to auto mode*/
	com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_DISABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	return com_rslt;
}
 /*!
 *	@brief This function used for set the mag power control
 *	bit enable
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bmm150_mag_wakeup(void)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	u8 v_try_times_u8 = BMI160_BMM150_MAX_RETRY_WAKEUP;
	u8 v_power_control_bit_u8 = BMI160_INIT_VALUE;
	u8 i = BMI160_INIT_VALUE;

	for (i = BMI160_INIT_VALUE; i < v_try_times_u8; i++) {
		com_rslt = bmi160_set_mag_write_data(BMI160_BMM150_POWER_ON);
		p_bmi160->delay_msec(BMI160_BMM150_WAKEUP_DELAY1);
		/*write 0x4B register to enable power control bit*/
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_CONTROL_REG);
		p_bmi160->delay_msec(BMI160_BMM150_WAKEUP_DELAY2);
		com_rslt += bmi160_set_mag_read_addr(
		BMI160_BMM150_POWE_CONTROL_REG);
		/* 0x04 is secondary read mag x lsb register */
		p_bmi160->delay_msec(BMI160_BMM150_WAKEUP_DELAY3);
		com_rslt += bmi160_read_reg(BMI160_USER_DATA_0_ADDR,
		&v_power_control_bit_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		v_power_control_bit_u8 = BMI160_BMM150_SET_POWER_CONTROL
		& v_power_control_bit_u8;
		if (v_power_control_bit_u8 == BMI160_BMM150_POWER_ON)
			break;
	}
	com_rslt = (i >= v_try_times_u8) ?
	BMI160_BMM150_POWER_ON_FAIL : BMI160_BMM150_POWER_ON_SUCCESS;
	return com_rslt;
}
 /*!
 *	@brief This function used for set the magnetometer
 *	power mode.
 *	@note
 *	Before set the mag power mode
 *	make sure the following two point is addressed
 *		Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *
 *	@param v_mag_sec_if_pow_mode_u8 : The value of mag power mode
 *  value    |  mode
 * ----------|------------
 *   0       | BMI160_MAG_FORCE_MODE
 *   1       | BMI160_MAG_SUSPEND_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_bmm150_mag_and_secondary_if_power_mode(
u8 v_mag_sec_if_pow_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	/* set the accel power mode to NORMAL*/
	com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
	/* set mag interface manual mode*/
	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)	{
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_ENABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
	com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);

	switch (v_mag_sec_if_pow_mode_u8) {
	case BMI160_MAG_FORCE_MODE:
		/* set the secondary mag power mode as NORMAL*/
		com_rslt += bmi160_set_mag_interface_normal();
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		/* set the mag power mode as FORCE mode*/
		com_rslt += bmi160_bmm150_mag_set_power_mode(FORCE_MODE);
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAG_SUSPEND_MODE:
		/* set the mag power mode as SUSPEND mode*/
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		com_rslt += bmi160_bmm150_mag_set_power_mode(SUSPEND_MODE);
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* set the secondary mag power mode as SUSPEND*/
		com_rslt += bmi160_set_command_register(MAG_MODE_SUSPEND);
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE) {
		/* set mag interface auto mode*/
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_DISABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
	com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
	return com_rslt;
}
/*!
 *	@brief This function used for set the magnetometer
 *	power mode.
 *	@note
 *	Before set the mag power mode
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@param v_mag_pow_mode_u8 : The value of mag power mode
 *  value    |  mode
 * ----------|------------
 *   0       | FORCE_MODE
 *   1       | SUSPEND_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bmm150_mag_set_power_mode(
u8 v_mag_pow_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	u8 manual_enable_status = 0;
	/* set mag interface manual mode*/
	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE) {
		com_rslt = bmi160_set_mag_manual_enable(
		BMI160_MANUAL_ENABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_get_mag_manual_enable(&manual_enable_status);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		printk(KERN_INFO "1com_rslt:%d, manual:%d, manual_read:%d\n",
		com_rslt, p_bmi160->mag_manual_enable, manual_enable_status);
	}
	printk(KERN_INFO "2com_rslt:%d, manual:%d, manual_read:%d\n",
	com_rslt, p_bmi160->mag_manual_enable, manual_enable_status);

	switch (v_mag_pow_mode_u8) {
	case FORCE_MODE:
		/* Set the power control bit enabled */
		com_rslt = bmi160_bmm150_mag_wakeup();
		/* write the mag power mode as FORCE mode*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_BMM150_FORCE_MODE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_MODE_REG);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		/* To avoid the auto mode enable when manual
		mode operation running*/
		V_bmm150_maual_auto_condition_u8 = BMI160_MANUAL_ENABLE;
		/* set the preset mode */
		com_rslt += bmi160_set_bmm150_mag_presetmode(
		BMI160_MAG_PRESETMODE_REGULAR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* To avoid the auto mode enable when manual
		mode operation running*/
		V_bmm150_maual_auto_condition_u8 = BMI160_MANUAL_DISABLE;
		/* set the mag read address to data registers*/
		com_rslt += bmi160_set_mag_read_addr(
		BMI160_BMM150_DATA_REG);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case SUSPEND_MODE:
		printk(KERN_INFO "3com_rslt:%d, manual:%d, read_manual:%d\n",
		com_rslt, p_bmi160->mag_manual_enable, manual_enable_status);
		/* Set the power mode of mag as suspend mode*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_BMM150_POWER_OFF);
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_CONTROL_REG);
		printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
		com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
	printk(KERN_INFO "4com_rslt:%d, manual:%d, manual_read:%d\n",
	com_rslt, p_bmi160->mag_manual_enable, manual_enable_status);
	/* set mag interface auto mode*/
	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE) {
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_DISABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_get_mag_manual_enable(&manual_enable_status);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_INFO "5com_rslt:%d, manual:%d, manual_read:%d\n",
	com_rslt, p_bmi160->mag_manual_enable, manual_enable_status);
	return com_rslt;
}
/*!
 *	@brief This API used to set the pre-set modes of bmm150
 *	The pre-set mode setting is depend on data rate and xy and z repetitions
 *
 *	@note
 *	Before set the mag preset mode
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param  v_mode_u8: The value of pre-set mode selection value
 *  value    |  pre_set mode
 * ----------|------------
 *   1       | BMI160_MAG_PRESETMODE_LOWPOWER
 *   2       | BMI160_MAG_PRESETMODE_REGULAR
 *   3       | BMI160_MAG_PRESETMODE_HIGHACCURACY
 *   4       | BMI160_MAG_PRESETMODE_ENHANCED
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_bmm150_mag_presetmode(u8 v_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	switch (v_mode_u8) {
	case BMI160_MAG_PRESETMODE_LOWPOWER:
		/* write the XY and Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt = bmi160_set_mag_write_data(
		BMI160_MAG_LOWPOWER_REPXY);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_XY_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* write the Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_LOWPOWER_REPZ);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_Z_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* set the mag v_data_u8 rate as 10 to the register 0x4C*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_LOWPOWER_DR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_MODE_REG);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAG_PRESETMODE_REGULAR:
		/* write the XY and Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt = bmi160_set_mag_write_data(
		BMI160_MAG_REGULAR_REPXY);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_XY_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* write the Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_REGULAR_REPZ);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_Z_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* set the mag v_data_u8 rate as 10 to the register 0x4C*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_REGULAR_DR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_MODE_REG);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAG_PRESETMODE_HIGHACCURACY:
		/* write the XY and Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt = bmi160_set_mag_write_data(
		BMI160_MAG_HIGHACCURACY_REPXY);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_XY_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* write the Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_HIGHACCURACY_REPZ);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_Z_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* set the mag v_data_u8 rate as 20 to the register 0x4C*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_HIGHACCURACY_DR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_MODE_REG);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAG_PRESETMODE_ENHANCED:
		/* write the XY and Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt = bmi160_set_mag_write_data(
		BMI160_MAG_ENHANCED_REPXY);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_XY_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* write the Z repetitions*/
		/* The v_data_u8 have to write for the register
		It write the value in the register 0x4F*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_ENHANCED_REPZ);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_Z_REP);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* set the mag v_data_u8 rate as 10 to the register 0x4C*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_MAG_ENHANCED_DR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_BMM150_POWE_MODE_REG);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}

	return com_rslt;
}
 /*!
 *	@brief This function used for read the trim values of magnetometer
 *
 *	@note
 *	Before reading the mag trimming values
 *	make sure the following two points are addressed
 *	@note
 *	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note
 *	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_bmm150_mag_trim(void)
{
	/* This variable used for provide the communication
	results*/
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array holding the bmm150 trim data
	*/
	u8 v_data_u8[BMI160_MAG_TRIM_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE};
	/* read dig_x1 value */
	com_rslt = bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_X1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_X1],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_x1 = v_data_u8[BMI160_BMM150_DIG_X1];
	/* read dig_y1 value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_Y1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Y1],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_y1 = v_data_u8[BMI160_BMM150_DIG_Y1];

	/* read dig_x2 value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_X2);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_X2],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_x2 = v_data_u8[BMI160_BMM150_DIG_X2];
	/* read dig_y2 value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_Y2);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Y3],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_y2 = v_data_u8[BMI160_BMM150_DIG_Y3];

	/* read dig_xy1 value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_XY1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_XY1],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_xy1 = v_data_u8[BMI160_BMM150_DIG_XY1];
	/* read dig_xy2 value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_XY2);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 ls register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_XY2],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_xy2 = v_data_u8[BMI160_BMM150_DIG_XY2];

	/* read dig_z1 lsb value */
	com_rslt += bmi160_set_mag_read_addr(
	BMI160_MAG_DIG_Z1_LSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Z1_LSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read dig_z1 msb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z1_MSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 msb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Z1_MSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_z1 =
	(u16)((((u32)((u8)v_data_u8[BMI160_BMM150_DIG_Z1_MSB]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_BMM150_DIG_Z1_LSB]));

	/* read dig_z2 lsb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z2_LSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Z2_LSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read dig_z2 msb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z2_MSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 msb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_Z2_MSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_z2 =
	(s16)((((s32)((s8)v_data_u8[BMI160_BMM150_DIG_Z2_MSB]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_BMM150_DIG_Z2_LSB]));

	/* read dig_z3 lsb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z3_LSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_Z3_LSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read dig_z3 msb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z3_MSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 msb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_Z3_MSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_z3 =
	(s16)((((s32)((s8)v_data_u8[BMI160_BMM150_DIG_DIG_Z3_MSB]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_BMM150_DIG_DIG_Z3_LSB]));

	/* read dig_z4 lsb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z4_LSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_Z4_LSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read dig_z4 msb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_Z4_MSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 msb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_Z4_MSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_z4 =
	(s16)((((s32)((s8)v_data_u8[BMI160_BMM150_DIG_DIG_Z4_MSB]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_BMM150_DIG_DIG_Z4_LSB]));

	/* read dig_xyz1 lsb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_XYZ1_LSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_XYZ1_LSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* read dig_xyz1 msb value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_MAG_DIG_XYZ1_MSB);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is v_mag_x_s16 msb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[BMI160_BMM150_DIG_DIG_XYZ1_MSB],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	mag_trim.dig_xyz1 =
	(u16)((((u32)((u8)v_data_u8[BMI160_BMM150_DIG_DIG_XYZ1_MSB]))
			<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			(v_data_u8[BMI160_BMM150_DIG_DIG_XYZ1_LSB]));

	return com_rslt;
}
 /*!
 *	@brief This function used for initialize
 *	the AKM09911 and AKM09912 sensor
 *
 *
 *	@param v_akm_i2c_address_u8: The value of device address
 *	AKM sensor   |  Slave address
 * --------------|---------------------
 *  AKM09911     |  AKM09911_I2C_ADDR_1
 *     -         |  and AKM09911_I2C_ADDR_2
 *  AKM09912     |  AKM09912_I2C_ADDR_1
 *     -         |  AKM09912_I2C_ADDR_2
 *     -         |  AKM09912_I2C_ADDR_3
 *     -         |  AKM09912_I2C_ADDR_4
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_akm_mag_interface_init(
u8 v_akm_i2c_address_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_pull_value_u8 = BMI160_INIT_VALUE;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_akm_chip_id_u8 = BMI160_INIT_VALUE;
	/* accel operation mode to normal*/
	com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(MAG_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_AKM_INIT_DELAY);
	bmi160_get_mag_power_mode_stat(&v_data_u8);
	/* register 0x7E write the 0x37, 0x9A and 0x30*/
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_ONE);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_TWO);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_THREE);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/*switch the page1*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_paging_enable(BMI160_WRITE_ENABLE_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_paging_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable the pullup configuration from
	the register 0x05 bit 4 and 5  to 10*/
	bmi160_get_pullup_configuration(&v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	v_pull_value_u8 = v_pull_value_u8 | BMI160_PULL_UP_DATA;
	com_rslt += bmi160_set_pullup_configuration(v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	/*switch the page0*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE0);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Write the AKM09911 0r AKM09912 i2c address*/
	com_rslt += bmi160_set_i2c_device_addr(v_akm_i2c_address_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable the mag interface to manual mode*/
	com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_ENABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*Enable the MAG interface */
	com_rslt += bmi160_set_if_mode(BMI160_ENABLE_MAG_IF_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_if_mode(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	/* Set the AKM Fuse ROM mode */
	/* Set value for fuse ROM mode*/
	com_rslt += bmi160_set_mag_write_data(AKM_FUSE_ROM_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* AKM mode address is 0x31*/
	com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* Read the Fuse ROM v_data_u8 from registers
	0x60,0x61 and 0x62*/
	/* ASAX v_data_u8 */
	com_rslt += bmi160_read_bst_akm_sensitivity_data();
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* read the device id of the AKM sensor
	if device id is 0x05 - AKM09911
	if device id is 0x04 - AKM09912*/
	com_rslt += bmi160_set_mag_read_addr(AKM09912_CHIP_ID_REG);
	/* 0x04 is mag_x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_akm_chip_id_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	printk(KERN_INFO "bmi160,addr:0x%x, akm_chip_id:0x%x",
	v_akm_i2c_address_u8, v_akm_chip_id_u8);
	/* Set value power down mode mode*/
	com_rslt += bmi160_set_mag_write_data(AKM_POWER_DOWN_MODE_DATA);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* AKM mode address is 0x31*/
	com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* Set AKM Force mode*/
	com_rslt += bmi160_set_mag_write_data(
	AKM_SINGLE_MEASUREMENT_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* AKM mode address is 0x31*/
	com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* Set the AKM read xyz v_data_u8 address*/
	com_rslt += bmi160_set_mag_read_addr(AKM_DATA_REGISTER);
	/* write the mag v_data_bw_u8 as 25Hz*/
	com_rslt += bmi160_set_mag_output_data_rate(
	BMI160_MAG_OUTPUT_DATA_RATE_25HZ);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Enable mag interface to auto mode*/
	com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_DISABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	return com_rslt;
}
 /*!
 *	@brief This function used for read the sensitivity data of
 *	AKM09911 and AKM09912
 *
 *	@note Before reading the mag sensitivity values
 *	make sure the following two points are addressed
 *	@note	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_bst_akm_sensitivity_data(void)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array holding the sensitivity ax,ay and az data*/
	u8 v_data_u8[BMI160_AKM_SENSITIVITY_DATA_SIZE] = {
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* read asax value */
	com_rslt = bmi160_set_mag_read_addr(BMI160_BST_AKM_ASAX);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[AKM_ASAX],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	akm_asa_data.asax = v_data_u8[AKM_ASAX];
	/* read asay value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_BST_AKM_ASAY);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[AKM_ASAY],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	akm_asa_data.asay = v_data_u8[AKM_ASAY];
	/* read asaz value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_BST_AKM_ASAZ);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[AKM_ASAZ],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	akm_asa_data.asaz = v_data_u8[AKM_ASAZ];

	return com_rslt;
}
/*!
 *	@brief This API used to get the compensated X data
 *	of AKM09911 the out put of X as s32
 *	@note	Before start reading the mag compensated X data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_x_s16 : The value of X data
 *
 *	@return results of compensated X data value output as s32
 *
 */
s32 bmi160_bst_akm09911_compensate_X(s16 v_bst_akm_x_s16)
{
	/*Return value of AKM x compensated v_data_u8*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw v_data_u8 into compensated v_data_u8*/
	retval = (v_bst_akm_x_s16 *
	((akm_asa_data.asax/AKM09911_SENSITIVITY_DIV) +
	BMI160_GEN_READ_WRITE_DATA_LENGTH));
	return retval;
}
/*!
 *	@brief This API used to get the compensated Y data
 *	of AKM09911 the out put of Y as s32
 *	@note	Before start reading the mag compensated Y data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_y_s16 : The value of Y data
 *
 *	@return results of compensated Y data value output as s32
 *
 */
s32 bmi160_bst_akm09911_compensate_Y(s16 v_bst_akm_y_s16)
{
	/*Return value of AKM y compensated v_data_u8*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw v_data_u8 into compensated v_data_u8*/
	retval = (v_bst_akm_y_s16 *
	((akm_asa_data.asay/AKM09911_SENSITIVITY_DIV) +
	BMI160_GEN_READ_WRITE_DATA_LENGTH));
	return retval;
}
/*!
 *	@brief This API used to get the compensated Z data
 *	of AKM09911 the out put of Z as s32
 *	@note	Before start reading the mag compensated Z data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_z_s16 : The value of Z data
 *
 *	@return results of compensated Z data value output as s32
 *
 */
s32 bmi160_bst_akm09911_compensate_Z(s16 v_bst_akm_z_s16)
{
	/*Return value of AKM z compensated v_data_u8*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw v_data_u8 into compensated v_data_u8*/
	retval = (v_bst_akm_z_s16 *
	((akm_asa_data.asaz/AKM09911_SENSITIVITY_DIV) +
	BMI160_GEN_READ_WRITE_DATA_LENGTH));
	return retval;
}
/*!
 *	@brief This API used to get the compensated X data
 *	of AKM09912 the out put of X as s32
 *	@note	Before start reading the mag compensated X data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_x_s16 : The value of X data
 *
 *	@return results of compensated X data value output as s32
 *
 */
s32 bmi160_bst_akm09912_compensate_X(s16 v_bst_akm_x_s16)
{
	/*Return value of AKM x compensated data*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw data into compensated data*/
	retval = v_bst_akm_x_s16 *
	(akm_asa_data.asax + AKM09912_SENSITIVITY)
	/ AKM09912_SENSITIVITY_DIV;
	return retval;
}
/*!
 *	@brief This API used to get the compensated Y data
 *	of AKM09912 the out put of Y as s32
 *	@note	Before start reading the mag compensated Y data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_y_s16 : The value of Y data
 *
 *	@return results of compensated Y data value output as s32
 *
 */
s32 bmi160_bst_akm09912_compensate_Y(s16 v_bst_akm_y_s16)
{
	/*Return value of AKM y compensated data*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw data into compensated data*/
	retval = v_bst_akm_y_s16 *
	(akm_asa_data.asax + AKM09912_SENSITIVITY)
	/ AKM09912_SENSITIVITY_DIV;
	return retval;
}
/*!
 *	@brief This API used to get the compensated Z data
 *	of AKM09912 the out put of Z as s32
 *	@note	Before start reading the mag compensated Z data
 *			make sure the following two points are addressed
 *	@note 1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note 2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *
 *  @param v_bst_akm_z_s16 : The value of Z data
 *
 *	@return results of compensated Z data value output as s32
 *
 */
s32 bmi160_bst_akm09912_compensate_Z(s16 v_bst_akm_z_s16)
{
	/*Return value of AKM z compensated data*/
	s32 retval = BMI160_INIT_VALUE;
	/* Convert raw data into compensated data*/
	retval = v_bst_akm_z_s16 *
	(akm_asa_data.asax + AKM09912_SENSITIVITY)
	/ AKM09912_SENSITIVITY_DIV;
	return retval;
}
 /*!
 *	@brief This function used for read the compensated value of
 *	AKM09911
 *	@note Before start reading the mag compensated data's
 *	make sure the following two points are addressed
 *	@note	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.

 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_akm09911_compensate_xyz(
struct bmi160_mag_xyz_s32_t *bst_akm_xyz)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	struct bmi160_mag_t mag_xyz;

	com_rslt = bmi160_read_mag_xyz(&mag_xyz, BST_AKM);
	/* Compensation for X axis */
	bst_akm_xyz->x = bmi160_bst_akm09911_compensate_X(mag_xyz.x);

	/* Compensation for Y axis */
	bst_akm_xyz->y = bmi160_bst_akm09911_compensate_Y(mag_xyz.y);

	/* Compensation for Z axis */
	bst_akm_xyz->z = bmi160_bst_akm09911_compensate_Z(mag_xyz.z);

	return com_rslt;
}
 /*!
 *	@brief This function used for read the compensated value of
 *	AKM09912
 *	@note Before start reading the mag compensated data's
 *	make sure the following two points are addressed
 *	@note	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.

 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_akm09912_compensate_xyz(
struct bmi160_mag_xyz_s32_t *bst_akm_xyz)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	struct bmi160_mag_t mag_xyz;

	com_rslt = bmi160_read_mag_xyz(&mag_xyz, BST_AKM);
	printk(KERN_INFO "akm09912_raw_x:%d, %d, %d, <%s>,<%d>",
	mag_xyz.x, mag_xyz.y, mag_xyz.z, __func__, __LINE__);
	/* Compensation for X axis */
	bst_akm_xyz->x = bmi160_bst_akm09912_compensate_X(mag_xyz.x);

	/* Compensation for Y axis */
	bst_akm_xyz->y = bmi160_bst_akm09912_compensate_Y(mag_xyz.y);

	/* Compensation for Z axis */
	bst_akm_xyz->z = bmi160_bst_akm09912_compensate_Z(mag_xyz.z);
	return com_rslt;
}
 /*!
 *	@brief This function used for read the compensated value of
 *	AKM09912
 *	@note Before start reading the mag compensated data's
 *	make sure the following two points are addressed
 *	@note	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.

 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_akm09912_compensate_xyz_raw(
struct bmi160_mag_xyz_s32_t *bst_akm_xyz)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Compensation for X axis */
	bst_akm_xyz->x = bmi160_bst_akm09912_compensate_X(bst_akm_xyz->x);

	/* Compensation for Y axis */
	bst_akm_xyz->y = bmi160_bst_akm09912_compensate_Y(bst_akm_xyz->y);

	/* Compensation for Z axis */
	bst_akm_xyz->z = bmi160_bst_akm09912_compensate_Z(bst_akm_xyz->z);

	return com_rslt;
}
/*!
 *	@brief This function used for set the AKM09911 and AKM09912
 *	power mode.
 *	@note Before set the AKM power mode
 *	make sure the following two points are addressed
 *	@note	1.	Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *	@note	2.	And also confirm the secondary-interface power mode
 *		is not in the SUSPEND mode.
 *		by using the function bmi160_get_mag_pmu_status().
 *		If the secondary-interface power mode is in SUSPEND mode
 *		set the value of 0x19(NORMAL mode)by using the
 *		bmi160_set_command_register(0x19) function.
 *
 *	@param v_akm_pow_mode_u8 : The value of akm power mode
 *  value   |    Description
 * ---------|--------------------
 *    0     |  AKM_POWER_DOWN_MODE
 *    1     |  AKM_SINGLE_MEAS_MODE
 *    2     |  FUSE_ROM_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_akm_set_powermode(
u8 v_akm_pow_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	/* set mag interface manual mode*/
	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE) {
		com_rslt = bmi160_set_mag_manual_enable(
		BMI160_MANUAL_ENABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_INFO "com_rslt:%d, manual:%d, <%s>\n",
	com_rslt, p_bmi160->mag_manual_enable, __func__);
	switch (v_akm_pow_mode_u8) {
	case AKM_POWER_DOWN_MODE:
		/* Set the power mode of AKM as power down mode*/
		com_rslt += bmi160_set_mag_write_data(AKM_POWER_DOWN_MODE_DATA);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	break;
	case AKM_SINGLE_MEAS_MODE:
		/* Set the power mode of AKM as
		single measurement mode*/
		com_rslt += bmi160_set_mag_write_data
		(AKM_SINGLE_MEASUREMENT_MODE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_read_addr(AKM_DATA_REGISTER);
	break;
	case FUSE_ROM_MODE:
		/* Set the power mode of AKM as
		Fuse ROM mode*/
		com_rslt += bmi160_set_mag_write_data(AKM_FUSE_ROM_MODE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		/* Sensitivity v_data_u8 */
		com_rslt += bmi160_read_bst_akm_sensitivity_data();
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		/* power down mode*/
		com_rslt += bmi160_set_mag_write_data(AKM_POWER_DOWN_MODE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(AKM_POWER_MODE_REG);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
	/* set mag interface auto mode*/
	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE) {
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_DISABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_INFO "com_rslt:%d, manual:%d, <%s><%d>\n",
	com_rslt, p_bmi160->mag_manual_enable, __func__, __LINE__);
	return com_rslt;
}
 /*!
 *	@brief This function used for set the magnetometer
 *	power mode of AKM09911 and AKM09912
 *	@note Before set the mag power mode
 *	make sure the following two point is addressed
 *		Make sure the mag interface is enabled or not,
 *		by using the bmi160_get_if_mode() function.
 *		If mag interface is not enabled set the value of 0x02
 *		to the function bmi160_get_if_mode(0x02)
 *
 *	@param v_mag_sec_if_pow_mode_u8 : The value of secondary if power mode
 *  value   |    Description
 * ---------|--------------------
 *    0     |  BMI160_MAG_FORCE_MODE
 *    1     |  BMI160_MAG_SUSPEND_MODE
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_bst_akm_and_secondary_if_powermode(
u8 v_mag_sec_if_pow_mode_u8)
{
	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* accel operation mode to normal*/
	com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* set mag interface manual mode*/
	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE) {
		com_rslt = bmi160_set_mag_manual_enable(
		BMI160_MANUAL_ENABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	}
	printk(KERN_ERR "com_rslt:%d, manual:%d,after setacc normal mode\n",
	com_rslt, p_bmi160->mag_manual_enable);
	switch (v_mag_sec_if_pow_mode_u8) {
	case BMI160_MAG_FORCE_MODE:
		/* set the secondary mag power mode as NORMAL*/
		com_rslt += bmi160_set_mag_interface_normal();
		/* set the akm power mode as single measurement mode*/
		com_rslt += bmi160_bst_akm_set_powermode(AKM_SINGLE_MEAS_MODE);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_read_addr(AKM_DATA_REGISTER);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	break;
	case BMI160_MAG_SUSPEND_MODE:
		/* set the akm power mode as power down mode*/
		com_rslt += bmi160_bst_akm_set_powermode(AKM_POWER_DOWN_MODE);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		/* set the secondary mag power mode as SUSPEND*/
		com_rslt += bmi160_set_command_register(MAG_MODE_SUSPEND);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
	break;
	}
	/* set mag interface auto mode*/
	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE)
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_DISABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	return com_rslt;
}
/*!
 *	@brief This function used for read the YAMAH-YAS532 init
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas532_mag_interface_init(
void)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_pull_value_u8 = BMI160_INIT_VALUE;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 i = BMI160_INIT_VALUE;
	/* accel operation mode to normal*/
	com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* write mag power mode as NORMAL*/
	com_rslt += bmi160_set_mag_interface_normal();
	/* register 0x7E write the 0x37, 0x9A and 0x30*/
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_ONE);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_TWO);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_THREE);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/*switch the page1*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_paging_enable(BMI160_WRITE_ENABLE_PAGE1);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_paging_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable the pullup configuration from
	the register 0x05 bit 4 and 5 as 10*/
	bmi160_get_pullup_configuration(&v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	v_pull_value_u8 = v_pull_value_u8 | BMI160_PULL_UP_DATA;
	com_rslt += bmi160_set_pullup_configuration(v_pull_value_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*switch the page0*/
	com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE0);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_target_page(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Write the YAS532 i2c address*/
	com_rslt += bmi160_set_i2c_device_addr(BMI160_AUX_YAS532_I2C_ADDRESS);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* enable the mag interface to manual mode*/
	com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_ENABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/*Enable the MAG interface */
	com_rslt += bmi160_set_if_mode(BMI160_ENABLE_MAG_IF_MODE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_if_mode(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	v_data_u8 = BMI160_MANUAL_DISABLE;
	/* Read the YAS532 device id is 0x02*/
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS_DEVICE_ID_REG);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Read the YAS532 calibration data*/
	com_rslt += bmi160_bst_yamaha_yas532_calib_values();
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* Assign the data acquisition mode*/
	yas532_data.measure_state = YAS532_MAG_STATE_INIT_COIL;
	/* Set the default offset as invalid offset*/
	set_vector(yas532_data.v_hard_offset_s8, INVALID_OFFSET);
	/* set the transform to zero */
	yas532_data.transform = BMI160_NULL;
	/* Assign overflow as zero*/
	yas532_data.overflow = 0;
	#if YAS532_MAG_LOG < YAS532_MAG_TEMPERATURE_LOG
		yas532_data.temp_data.num =
		yas532_data.temp_data.idx = 0;
	#endif
	/* Assign the coef value*/
	for (i = 0; i < 3; i++) {
		yas532_data.coef[i] = yas532_version_ac_coef[i];
		yas532_data.last_raw[i] = 0;
	}
	yas532_data.last_raw[3] = 0;
	/* Set the initial values of yas532*/
	com_rslt += bmi160_bst_yas532_set_initial_values();
	/* write the mag v_data_bw_u8 as 25Hz*/
	com_rslt += bmi160_set_mag_output_data_rate(
	BMI160_MAG_OUTPUT_DATA_RATE_25HZ);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* Enable mag interface to auto mode*/
	com_rslt += bmi160_set_mag_manual_enable(
	BMI160_MANUAL_DISABLE);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	bmi160_get_mag_manual_enable(&v_data_u8);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	return com_rslt;
}
/*!
 *	@brief This function used to set the YAS532 initial values
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_set_initial_values(void)
{
/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* write testr1 as 0x00*/
	com_rslt = bmi160_set_mag_write_data(
	BMI160_YAS532_WRITE_TESTR1);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_TESTR1);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* write testr2 as 0x00*/
	com_rslt += bmi160_set_mag_write_data(
	BMI160_YAS532_WRITE_TESTR2);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_TESTR2);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* write Rcoil as 0x00*/
	com_rslt += bmi160_set_mag_write_data(
	BMI160_YAS532_WRITE_RCOIL);
	p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_RCOIL);
	p_bmi160->delay_msec(BMI160_YAS532_SET_INITIAL_VALUE_DELAY);
	/* check the valid offset*/
	if (is_valid_offset(yas532_data.v_hard_offset_s8)) {
		com_rslt += bmi160_bst_yas532_set_offset(
		yas532_data.v_hard_offset_s8);
		yas532_data.measure_state = YAS532_MAG_STATE_NORMAL;
	} else {
		/* set the default offset as invalid offset*/
		set_vector(yas532_data.v_hard_offset_s8, INVALID_OFFSET);
		/*Set the default measure state for offset correction*/
		yas532_data.measure_state = YAS532_MAG_STATE_MEASURE_OFFSET;
	}
	return com_rslt;
}
/*!
 *	@brief This function used for YAS532 offset correction
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_magnetic_measure_set_offset(
void)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* used for offset value set to the offset register*/
	s8 v_hard_offset_s8[BMI160_HARD_OFFSET_DATA_SIZE] = {
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* offset correction factors*/
	static const u8 v_correct_u8[BMI160_YAS_CORRECT_DATA_SIZE] = {
	16, 8, 4, 2, 1};
	/* used for the temperature */
	u16 v_temp_u16 = BMI160_INIT_VALUE;
	/* used for the xy1y2 read*/
	u16 v_xy1y2_u16[BMI160_YAS_XY1Y2_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* local flag for assign the values*/
	s32 v_flag_s32[BMI160_YAS_FLAG_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	u8 i, j, v_busy_u8, v_overflow_u8 = BMI160_INIT_VALUE;

	for (i = 0; i < 5; i++) {
		/* set the offset values*/
		com_rslt = bmi160_bst_yas532_set_offset(v_hard_offset_s8);
		/* read the sensor data*/
		com_rslt += bmi160_bst_yas532_normal_measurement_data(
		BMI160_YAS532_ACQ_START, &v_busy_u8, &v_temp_u16,
		v_xy1y2_u16, &v_overflow_u8);
		/* check the sensor busy status*/
		if (v_busy_u8)
			return E_BMI160_BUSY;
		/* calculate the magnetic correction with
		offset and assign the values
		to the offset register */
		for (j = 0; j < 3; j++) {
			if (YAS532_DATA_CENTER == v_xy1y2_u16[j])
				v_flag_s32[j] = 0;
			if (YAS532_DATA_CENTER < v_xy1y2_u16[j])
				v_flag_s32[j] = 1;
			if (v_xy1y2_u16[j] < YAS532_DATA_CENTER)
				v_flag_s32[j] = -1;
		}
		for (j = 0; j < 3; j++) {
			if (v_flag_s32[j])
				v_hard_offset_s8[j] = (s8)(v_hard_offset_s8[j]
				+ v_flag_s32[j] * v_correct_u8[i]);
		}
	}
	/* set the offset */
	com_rslt += bmi160_bst_yas532_set_offset(v_hard_offset_s8);
	return com_rslt;
}
/*!
 *	@brief This function used for read the
 *	YAMAHA YAS532 calibration data
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas532_calib_values(void)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array holding the YAS532 calibration values */
	u8 v_data_u8[BMI160_YAS532_CALIB_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* Read the DX value */
	com_rslt = bmi160_set_mag_read_addr(BMI160_YAS532_CALIB_CX);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[0], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	yas532_data.calib_yas532.cx = (s32)((v_data_u8[0]
	* 10) - 1280);
	/* Read the DY1 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB_CY1);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[1], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	yas532_data.calib_yas532.cy1 =
	(s32)((v_data_u8[1] * 10) - 1280);
	/* Read the DY2 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB_CY2);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[2], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	yas532_data.calib_yas532.cy2 =
	(s32)((v_data_u8[2] * 10) - 1280);
	/* Read the D2 and D3 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB1);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[3], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	yas532_data.calib_yas532.a2 =
	(s32)(((v_data_u8[3] >>
	BMI160_SHIFT_BIT_POSITION_BY_02_BITS)
	& 0x03F) - 32);
	/* Read the D3 and D4 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB2);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[4], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a3*/
	yas532_data.calib_yas532.a3 = (s32)((((v_data_u8[3] <<
	BMI160_SHIFT_BIT_POSITION_BY_02_BITS) & 0x0C) |
	((v_data_u8[4]
	>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS)
	& 0x03)) - 8);
	/* calculate a4*/
	yas532_data.calib_yas532.a4 = (s32)((v_data_u8[4]
	& 0x3F) - 32);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
    /* Read the D5 and D6 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB3);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[5], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a5*/
	yas532_data.calib_yas532.a5 =
	(s32)(((v_data_u8[5]
	>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS)
	& 0x3F) + 38);
	/* Read the D6 and D7 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB4);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[6], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a6*/
	yas532_data.calib_yas532.a6 =
	(s32)((((v_data_u8[5]
	<< BMI160_SHIFT_BIT_POSITION_BY_04_BITS)
	& 0x30) | ((v_data_u8[6] >>
	 BMI160_SHIFT_BIT_POSITION_BY_04_BITS)
	 & 0x0F)) - 32);
	 /* Read the D7 and D8 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB5);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[7], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a7*/
	yas532_data.calib_yas532.a7 = (s32)((((v_data_u8[6]
	<< BMI160_SHIFT_BIT_POSITION_BY_03_BITS)
	& 0x78) |
	((v_data_u8[7]
	>> BMI160_SHIFT_BIT_POSITION_BY_05_BITS) &
	0x07)) - 64);
	/* Read the D8 and D9 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CLAIB6);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[8], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a8*/
	yas532_data.calib_yas532.a8 = (s32)((((v_data_u8[7] <<
	BMI160_GEN_READ_WRITE_DATA_LENGTH) & 0x3E) |
	((v_data_u8[8] >>
	BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01)) -
	32);

	/* Read the D8 and D9 value */
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB7);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[9], BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* calculate a9*/
	yas532_data.calib_yas532.a9 = (s32)(((v_data_u8[8] <<
	BMI160_GEN_READ_WRITE_DATA_LENGTH) & 0xFE) |
	 ((v_data_u8[9] >>
	 BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01));
	/* calculate k*/
	yas532_data.calib_yas532.k = (s32)((v_data_u8[9] >>
	BMI160_SHIFT_BIT_POSITION_BY_02_BITS) & 0x1F);
	/* Read the  value from register 0x9A*/
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB8);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[10],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Read the  value from register 0x9B*/
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIIB9);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[11],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Read the  value from register 0x9C*/
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB10);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[12],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Read the  value from register 0x9D*/
	com_rslt += bmi160_set_mag_read_addr(BMI160_YAS532_CALIB11);
	/* 0x04 is secondary read mag x lsb register */
	com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
	&v_data_u8[13],
	BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Calculate the fxy1y2 and rxy1y1*/
	yas532_data.calib_yas532.fxy1y2[0] =
	(u8)(((v_data_u8[10]
	& 0x01)
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	| ((v_data_u8[11] >>
	BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01));
	yas532_data.calib_yas532.rxy1y2[0] =
	((s8)(((v_data_u8[10]
	>> BMI160_SHIFT_BIT_POSITION_BY_01_BIT) & 0x3F)
	<< BMI160_SHIFT_BIT_POSITION_BY_02_BITS))
	>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS;
	yas532_data.calib_yas532.fxy1y2[1] =
	(u8)(((v_data_u8[11] & 0x01)
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	 | ((v_data_u8[12] >>
	 BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01));
	yas532_data.calib_yas532.rxy1y2[1] =
	((s8)(((v_data_u8[11]
	>> BMI160_SHIFT_BIT_POSITION_BY_01_BIT) & 0x3F)
	<< BMI160_SHIFT_BIT_POSITION_BY_02_BITS))
	>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS;
	yas532_data.calib_yas532.fxy1y2[2] =
	(u8)(((v_data_u8[12] & 0x01)
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	| ((v_data_u8[13]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01));
	yas532_data.calib_yas532.rxy1y2[2] =
	((s8)(((v_data_u8[12]
	>> BMI160_SHIFT_BIT_POSITION_BY_01_BIT) & 0x3F)
	 << BMI160_SHIFT_BIT_POSITION_BY_02_BITS))
	 >> BMI160_SHIFT_BIT_POSITION_BY_02_BITS;

	return com_rslt;
}
/*!
 *	@brief This function used for calculate the
 *	YAS532 read the linear data
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_xy1y2_to_linear(
u16 *v_xy1y2_u16, s32 *xy1y2_linear)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = SUCCESS;
	static const u16 v_calib_data[] = {
	3721, 3971, 4221, 4471};
	u8 i = BMI160_INIT_VALUE;

	for (i = 0; i < 3; i++)
		xy1y2_linear[i] = v_xy1y2_u16[i] -
		 v_calib_data[yas532_data.calib_yas532.fxy1y2[i]]
			+ (yas532_data.v_hard_offset_s8[i] -
			yas532_data.calib_yas532.rxy1y2[i])
			* yas532_data.coef[i];
	return com_rslt;
}
/*!
 *	@brief This function used for read the YAS532 sensor data
 *	@param	v_acquisition_command_u8: used to set the data acquisition
 *	acquisition_command  |   operation
 *  ---------------------|-------------------------
 *         0x17          | turn on the acquisition coil
 *         -             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Deferred acquisition mode
 *        0x07           | turn on the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Normal acquisition mode
 *        0x11           | turn OFF the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as plus(+))
 *         _             | Deferred acquisition mode
 *       0x01            | turn OFF the acquisition coil
 *        _              | set direction of the coil
 *        _              | (x and y as plus(+))
 *        _              | Normal acquisition mode
 *
 *	@param	v_busy_u8 : used to get the busy flay for sensor data read
 *	@param	v_temp_u16 : used to get the temperature data
 *	@param	v_xy1y2_u16 : used to get the sensor xy1y2 data
 *	@param	v_overflow_u8 : used to get the overflow data
 *
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_normal_measurement_data(
u8 v_acquisition_command_u8, u8 *v_busy_u8,
u16 *v_temp_u16, u16 *v_xy1y2_u16, u8 *v_overflow_u8)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	/* Array holding the YAS532 xyy1 data*/
	u8 v_data_u8[BMI160_YAS_XY1Y2T_DATA_SIZE] = {
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	u8 i = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
		} else {
		/* read the sensor data */
		com_rslt = bmi160_bst_yas532_acquisition_command_register(
		v_acquisition_command_u8);
		com_rslt +=
		p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
		BMI160_USER_DATA_MAG_X_LSB__REG,
		v_data_u8, BMI160_MAG_YAS_DATA_LENGTH);
		/* read the xyy1 data*/
		*v_busy_u8 =
		((v_data_u8[0]
		>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS) & 0x01);
		*v_temp_u16 =
		(u16)((((s32)v_data_u8[0]
		<< BMI160_SHIFT_BIT_POSITION_BY_03_BITS)
		& 0x3F8) | ((v_data_u8[1]
		>> BMI160_SHIFT_BIT_POSITION_BY_05_BITS) & 0x07));
		v_xy1y2_u16[0] =
		(u16)((((s32)v_data_u8[2]
		<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS) & 0x1FC0)
		| ((v_data_u8[3] >>
		BMI160_SHIFT_BIT_POSITION_BY_02_BITS) & 0x3F));
		v_xy1y2_u16[1] =
		(u16)((((s32)v_data_u8[4]
		<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS)
		& 0x1FC0)
		| ((v_data_u8[5]
		>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS) & 0x3F));
		v_xy1y2_u16[2] =
		(u16)((((s32)v_data_u8[6]
		<< BMI160_SHIFT_BIT_POSITION_BY_06_BITS)
		& 0x1FC0)
		| ((v_data_u8[7]
		>> BMI160_SHIFT_BIT_POSITION_BY_02_BITS) & 0x3F));
		*v_overflow_u8 = 0;
		for (i = 0; i < 3; i++) {
			if (v_xy1y2_u16[i] == YAS532_DATA_OVERFLOW)
				*v_overflow_u8 |= (1 << (i * 2));
			if (v_xy1y2_u16[i] == YAS532_DATA_UNDERFLOW)
				*v_overflow_u8 |= (1 << (i * 2 + 1));
		}
	}
	return com_rslt;
}
/*!
 *	@brief This function used for YAS532 sensor data
 *	@param	v_acquisition_command_u8	:	the value of CMDR
 *	acquisition_command  |   operation
 *  ---------------------|-------------------------
 *         0x17          | turn on the acquisition coil
 *         -             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Deferred acquisition mode
 *        0x07           | turn on the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Normal acquisition mode
 *        0x11           | turn OFF the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as plus(+))
 *         _             | Deferred acquisition mode
 *       0x01            | turn OFF the acquisition coil
 *        _              | set direction of the coil
 *        _              | (x and y as plus(+))
 *        _              | Normal acquisition mode
 *
 * @param xyz_data : the vector xyz output
 * @param v_overflow_s8 : the value of overflow
 * @param v_temp_correction_u8 : the value of temperate correction enable
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_measurement_xyz_data(
struct yas532_vector *xyz_data, u8 *v_overflow_s8, u8 v_temp_correction_u8,
u8 v_acquisition_command_u8)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = BMI160_INIT_VALUE;
	/* Array holding the linear calculation output*/
	s32 v_xy1y2_linear_s32[BMI160_YAS_XY1Y2_DATA_SIZE] = {
	BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* Array holding the temperature data */
	s32 v_xyz_tmp_s32[BMI160_YAS_TEMP_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	s32 tmp = BMI160_INIT_VALUE;
	s32 sx, sy1, sy2, sy, sz = BMI160_INIT_VALUE;
	u8 i, v_busy_u8 = BMI160_INIT_VALUE;
	u16 v_temp_u16 = BMI160_INIT_VALUE;
	/* Array holding the xyy1 sensor raw data*/
	u16 v_xy1y2_u16[BMI160_YAS_XY1Y2_DATA_SIZE] = {BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	#if YAS532_MAG_LOG < YAS532_MAG_TEMPERATURE_LOG
	s32 sum = BMI160_INIT_VALUE;
	#endif
	*v_overflow_s8 = BMI160_INIT_VALUE;
	switch (yas532_data.measure_state) {
	case YAS532_MAG_STATE_INIT_COIL:
		if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)
			com_rslt = bmi160_set_mag_manual_enable(
			BMI160_MANUAL_ENABLE);
		/* write Rcoil*/
		com_rslt += bmi160_set_mag_write_data(
		BMI160_YAS_DISABLE_RCOIL);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_RCOIL);
		p_bmi160->delay_msec(BMI160_YAS532_MEASUREMENT_DELAY);
		if (!yas532_data.overflow && is_valid_offset(
		yas532_data.v_hard_offset_s8))
			yas532_data.measure_state = 0;
	break;
	case YAS532_MAG_STATE_MEASURE_OFFSET:
		com_rslt = bmi160_bst_yas532_magnetic_measure_set_offset();
		yas532_data.measure_state = 0;
	break;
	default:
	break;
	}
	/* Read sensor data*/
	com_rslt += bmi160_bst_yas532_normal_measurement_data(
	v_acquisition_command_u8, &v_busy_u8, &v_temp_u16,
	v_xy1y2_u16, v_overflow_s8);
	/* Calculate the linear data*/
	com_rslt += bmi160_bst_yas532_xy1y2_to_linear(v_xy1y2_u16,
	v_xy1y2_linear_s32);
	/* Calculate temperature correction */
	#if YAS532_MAG_LOG < YAS532_MAG_TEMPERATURE_LOG
		yas532_data.temp_data.log[yas532_data.temp_data.idx++] =
		v_temp_u16;
	if (YAS532_MAG_TEMPERATURE_LOG <= yas532_data.temp_data.idx)
		yas532_data.temp_data.idx = 0;
		yas532_data.temp_data.num++;
	if (YAS532_MAG_TEMPERATURE_LOG <= yas532_data.temp_data.num)
		yas532_data.temp_data.num = YAS532_MAG_TEMPERATURE_LOG;
	for (i = 0; i < yas532_data.temp_data.num; i++)
		sum += yas532_data.temp_data.log[i];
		tmp = sum * 10 / yas532_data.temp_data.num
		- YAS532_TEMP20DEGREE_TYPICAL * 10;
	#else
		tmp = (v_temp_u16 - YAS532_TEMP20DEGREE_TYPICAL)
		* 10;
	#endif
	sx  = v_xy1y2_linear_s32[0];
	sy1 = v_xy1y2_linear_s32[1];
	sy2 = v_xy1y2_linear_s32[2];
	/* Temperature correction */
	if (v_temp_correction_u8) {
		sx  -= (yas532_data.calib_yas532.cx  * tmp)
		/ 1000;
		sy1 -= (yas532_data.calib_yas532.cy1 * tmp)
		/ 1000;
		sy2 -= (yas532_data.calib_yas532.cy2 * tmp)
		/ 1000;
	}
	sy = sy1 - sy2;
	sz = -sy1 - sy2;

	xyz_data->yas532_vector_xyz[0] = yas532_data.calib_yas532.k *
	((100 * sx + yas532_data.calib_yas532.a2 * sy +
	yas532_data.calib_yas532.a3 * sz) / 10);
	xyz_data->yas532_vector_xyz[1] = yas532_data.calib_yas532.k *
	((yas532_data.calib_yas532.a4 * sx + yas532_data.calib_yas532.a5 * sy +
	yas532_data.calib_yas532.a6 * sz) / 10);
	xyz_data->yas532_vector_xyz[2] = yas532_data.calib_yas532.k *
	((yas532_data.calib_yas532.a7 * sx + yas532_data.calib_yas532.a8 * sy +
	yas532_data.calib_yas532.a9 * sz) / 10);
	if (yas532_data.transform != BMI160_NULL) {
		for (i = 0; i < 3; i++) {
				v_xyz_tmp_s32[i] = yas532_data.transform[i
				* 3] *
				xyz_data->yas532_vector_xyz[0]
				+ yas532_data.transform[i * 3 + 1] *
				xyz_data->yas532_vector_xyz[1]
				+ yas532_data.transform[i * 3 + 2] *
				xyz_data->yas532_vector_xyz[2];
		}
		set_vector(xyz_data->yas532_vector_xyz, v_xyz_tmp_s32);
	}
	for (i = 0; i < 3; i++) {
		xyz_data->yas532_vector_xyz[i] -=
		xyz_data->yas532_vector_xyz[i] % 10;
		if (*v_overflow_s8 & (1
		<< (i * 2)))
			xyz_data->yas532_vector_xyz[i] +=
			1; /* set overflow */
		if (*v_overflow_s8 & (1 <<
		(i * 2 + 1)))
			xyz_data->yas532_vector_xyz[i] += 2; /* set underflow */
	}


if (v_busy_u8)
		return com_rslt;
	if (0 < *v_overflow_s8) {
		if (!yas532_data.overflow)
			yas532_data.overflow = 1;
		yas532_data.measure_state = YAS532_MAG_STATE_INIT_COIL;
	} else
		yas532_data.overflow = 0;
	for (i = 0; i < 3; i++)
		yas532_data.last_raw[i] = v_xy1y2_u16[i];
	  yas532_data.last_raw[i] = v_temp_u16;
	return com_rslt;
}
/*!
 *	@brief This function used for YAS532 write data acquisition
 *	command register write
 *	@param	v_command_reg_data_u8	:	the value of data acquisition
 *	acquisition_command  |   operation
 *  ---------------------|-------------------------
 *         0x17          | turn on the acquisition coil
 *         -             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Deferred acquisition mode
 *        0x07           | turn on the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Normal acquisition mode
 *        0x11           | turn OFF the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as plus(+))
 *         _             | Deferred acquisition mode
 *       0x01            | turn OFF the acquisition coil
 *        _              | set direction of the coil
 *        _              | (x and y as plus(+))
 *        _              | Normal acquisition mode
 *
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_acquisition_command_register(
u8 v_command_reg_data_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)
			com_rslt = bmi160_set_mag_manual_enable(
			BMI160_MANUAL_ENABLE);

		com_rslt = bmi160_set_mag_write_data(v_command_reg_data_u8);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* YAMAHA YAS532-0x82*/
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_YAS532_COMMAND_REGISTER);
		p_bmi160->delay_msec(BMI160_YAS_ACQ_COMMAND_DELAY);
		com_rslt += bmi160_set_mag_read_addr(
		BMI160_YAS532_DATA_REGISTER);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE)
		com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_DISABLE);

	return com_rslt;

}
/*!
 *	@brief This function used write offset of YAS532
 *
 *	@param	p_offset_s8	: The value of offset to write
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas532_set_offset(
const s8 *p_offset_s8)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)
		com_rslt = bmi160_set_mag_manual_enable(BMI160_MANUAL_ENABLE);
		p_bmi160->delay_msec(BMI160_YAS532_OFFSET_DELAY);

	    /* Write offset X data*/
		com_rslt = bmi160_set_mag_write_data(p_offset_s8[0]);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* YAS532 offset x write*/
		com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_OFFSET_X);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

		/* Write offset Y data*/
		com_rslt = bmi160_set_mag_write_data(p_offset_s8[1]);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* YAS532 offset y write*/
		com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_OFFSET_Y);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

		/* Write offset Z data*/
		com_rslt = bmi160_set_mag_write_data(p_offset_s8[2]);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* YAS532 offset z write*/
		com_rslt += bmi160_set_mag_write_addr(BMI160_YAS532_OFFSET_Z);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		set_vector(yas532_data.v_hard_offset_s8, p_offset_s8);

	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE)
		com_rslt = bmi160_set_mag_manual_enable(BMI160_MANUAL_DISABLE);
	return com_rslt;
}
/*!
 *	@brief This function used to init the YAMAH-YAS537
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas537_mag_interface_init(
void)
{
/* This variable used for provide the communication
results*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
u8 v_pull_value_u8 = BMI160_INIT_VALUE;
u8 v_data_u8 = BMI160_INIT_VALUE;
u8 i = BMI160_INIT_VALUE;
/* accel operation mode to normal*/
com_rslt = bmi160_set_command_register(ACCEL_MODE_NORMAL);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* write mag power mode as NORMAL*/
com_rslt += bmi160_set_mag_interface_normal();
/* register 0x7E write the 0x37, 0x9A and 0x30*/
com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_ONE);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_TWO);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_command_register(BMI160_COMMAND_REG_THREE);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/*switch the page1*/
com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE1);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_target_page(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_paging_enable(BMI160_WRITE_ENABLE_PAGE1);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_paging_enable(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* enable the pullup configuration from
the register 0x05 bit 4 and 5 as 10*/
bmi160_get_pullup_configuration(&v_pull_value_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
v_pull_value_u8 = v_pull_value_u8 | BMI160_PULL_UP_DATA;
com_rslt += bmi160_set_pullup_configuration(v_pull_value_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/*switch the page0*/
com_rslt += bmi160_set_target_page(BMI160_WRITE_TARGET_PAGE0);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_target_page(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* Write the YAS532 i2c address*/
com_rslt += bmi160_set_i2c_device_addr(BMI160_YAS537_I2C_ADDRESS);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* enable the mag interface to manual mode*/
com_rslt += bmi160_set_mag_manual_enable(BMI160_MANUAL_ENABLE);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_mag_manual_enable(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/*Enable the MAG interface */
com_rslt += bmi160_set_if_mode(BMI160_ENABLE_MAG_IF_MODE);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_if_mode(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
v_data_u8 = BMI160_MANUAL_DISABLE;
/* Read the YAS537 device id*/
com_rslt += bmi160_set_mag_read_addr(BMI160_YAS_DEVICE_ID_REG);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
yas537_data.dev_id = v_data_u8;
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* Read the YAS532 calibration data*/
com_rslt +=
bmi160_bst_yamaha_yas537_calib_values(
BMI160_GEN_READ_WRITE_DATA_LENGTH);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/* set the mode to NORMAL*/
yas537_data.measure_state = YAS537_MAG_STATE_NORMAL;
/* set the transform to zero */
yas537_data.transform = BMI160_NULL;
yas537_data.average = 32;
for (i = 0; i < 3; i++) {
	yas537_data.hard_offset[i] = -128;
	yas537_data.last_after_rcoil[i] = 0;
}
for (i = 0; i < 4; i++)
	yas537_data.last_raw[i] = 0;
/* write the mag bandwidth as 25Hz*/
com_rslt += bmi160_set_mag_output_data_rate(
BMI160_MAG_OUTPUT_DATA_RATE_25HZ);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* Enable mag interface to auto mode*/
com_rslt += bmi160_set_mag_manual_enable(
BMI160_MANUAL_DISABLE);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
bmi160_get_mag_manual_enable(&v_data_u8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
return com_rslt;
}
/*!
*	@brief This function used for read the
*	YAMAHA YAS537 calibration data
*
*
*	@param v_rcoil_u8 : The value of r coil
*
*
*	@return results of bus communication function
*	@retval 0 -> Success
*	@retval -1 -> Error
*
*
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas537_calib_values(
u8 v_rcoil_u8)
{
/* This variable used for provide the communication
results*/
BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
/* Array holding the YAS532 calibration values */
u8 a_data_u8[BMI160_YAS537_CALIB_DATA_SIZE] = {
BMI160_INIT_VALUE, BMI160_INIT_VALUE,
BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
};
static const u8 v_avrr_u8[] = {0x50, 0x60, 0x70};
u8 v_cal_valid_u8 = BMI160_INIT_VALUE, i;
/* write soft reset as 0x02*/
com_rslt = bmi160_set_mag_write_data(
YAS537_SRSTR_DATA);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_mag_write_addr(YAS537_REG_SRSTR);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/* Read the DX value */
com_rslt = bmi160_set_mag_read_addr(YAS537_REG_CALR_C0);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[0], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the DY1 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C1);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[1], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the DY2 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C2);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[2], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D2 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C3);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[3], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D3 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C4);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[4], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D4 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C5);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[5], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D5 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C6);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[6], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D6 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C7);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[7], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D7 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C8);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[8], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D8 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_C9);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[9], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the D9 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CA);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[10], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the RX value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CB);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[11], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the RY1 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CC);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[12], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the RY2 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CD);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[13], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the RY2 value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CE);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[14], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the CHF value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_CF);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[15], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* Read the VER value */
com_rslt += bmi160_set_mag_read_addr(YAS537_REG_CALR_DO);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
/* 0x04 is secondary read mag x lsb register */
com_rslt += bmi160_read_reg(BMI160_MAG_DATA_READ_REG,
&a_data_u8[16], BMI160_GEN_READ_WRITE_DATA_LENGTH);
/* get the calib ver*/
yas537_data.calib_yas537.ver =
(a_data_u8[16] >> BMI160_SHIFT_BIT_POSITION_BY_06_BITS);
for (i = 0; i < 17; i++) {
	if (((i < 16 && a_data_u8[i]) != 0))
		v_cal_valid_u8 = 1;
	if ((i < 16 &&
	(a_data_u8[i] & 0x3F)) != 0)
		v_cal_valid_u8 = 1;
}
if (!v_cal_valid_u8)
	return ERROR;
if (yas537_data.calib_yas537.ver == 0) {
	for (i = 0; i < 17; i++) {
		if (i < 12) {
			/* write offset*/
			com_rslt += bmi160_set_mag_write_data(
			a_data_u8[i]);
			p_bmi160->delay_msec(
			BMI160_GEN_READ_WRITE_DELAY);
			com_rslt += bmi160_set_mag_write_addr(
			YAS537_REG_MTCR + i);
			p_bmi160->delay_msec(
			BMI160_GEN_READ_WRITE_DELAY);
		} else if (i < 15) {
			/* write offset correction*/
			com_rslt += bmi160_set_mag_write_data(
			a_data_u8[i]);
			p_bmi160->delay_msec(
			BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
			com_rslt += bmi160_set_mag_write_addr((
			(YAS537_REG_OXR + i) - 12));
			p_bmi160->delay_msec(
			BMI160_GEN_READ_WRITE_DELAY);
			yas537_data.hard_offset[i - 12]
			= a_data_u8[i];
		} else {
			/* write offset correction*/
			com_rslt += bmi160_set_mag_write_data(
			a_data_u8[i]);
			p_bmi160->delay_msec(
			BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
			com_rslt += bmi160_set_mag_write_addr((
			(YAS537_REG_OXR + i) - 11));
			p_bmi160->delay_msec(
			BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		}

}
} else if (yas537_data.calib_yas537.ver == 1) {
	for (i = 0; i < 3; i++) {
		/* write offset*/
		com_rslt += bmi160_set_mag_write_data(
		a_data_u8[i]);
		p_bmi160->delay_msec(
		BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(
		YAS537_REG_MTCR + i);
		p_bmi160->delay_msec(
		BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		if (com_rslt == SUCCESS) {
			/* write offset*/
			com_rslt += bmi160_set_mag_write_data(
			a_data_u8[i + 12]);
			p_bmi160->delay_msec(
			BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
			com_rslt += bmi160_set_mag_write_addr(
			YAS537_REG_OXR + i);
			p_bmi160->delay_msec(
			BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
			yas537_data.hard_offset[i] =
			a_data_u8[i + 12];
		} else {
			com_rslt = ERROR;
		}
	}
	/* write offset*/
	com_rslt += bmi160_set_mag_write_data(
	((a_data_u8[i] & 0xE0) | 0x10));
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(
	YAS537_REG_MTCR + i);
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* write offset*/
	com_rslt += bmi160_set_mag_write_data(
	((a_data_u8[15]
	>> BMI160_SHIFT_BIT_POSITION_BY_03_BITS)
	& 0x1E));
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(YAS537_REG_HCKR);
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* write offset*/
	com_rslt += bmi160_set_mag_write_data(
	((a_data_u8[15] << 1) & 0x1E));
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(YAS537_REG_LCKR);
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* write offset*/
	com_rslt += bmi160_set_mag_write_data(
	(a_data_u8[16] & 0x3F));
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(YAS537_REG_OCR);
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);

	/* Assign the calibration values*/
	/* a2 */
	yas537_data.calib_yas537.a2 =
	((((a_data_u8[3]
	<< BMI160_SHIFT_BIT_POSITION_BY_02_BITS)
	& 0x7C)
	| (a_data_u8[4]
	>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS)) - 64);
	/* a3 */
	yas537_data.calib_yas537.a3 =
	((((a_data_u8[4] << BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	& 0x7E)
	| (a_data_u8[5]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS)) - 64);
	/* a4 */
	yas537_data.calib_yas537.a4 =
	((((a_data_u8[5]
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	& 0xFE)
	| (a_data_u8[6]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS))
	- 128);
	/* a5 */
	yas537_data.calib_yas537.a5 =
	((((a_data_u8[6]
	<< BMI160_SHIFT_BIT_POSITION_BY_02_BITS)
	& 0x1FC)
	| (a_data_u8[7]
	>> BMI160_SHIFT_BIT_POSITION_BY_06_BITS))
	- 112);
	/* a6 */
	yas537_data.calib_yas537.a6 =
	((((a_data_u8[7]
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	& 0x7E)
	| (a_data_u8[8]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS)) - 64);
	/* a7 */
	yas537_data.calib_yas537.a7 =
	((((a_data_u8[8]
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT)
	& 0xFE)
	| (a_data_u8[9]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS))
	- 128);
	/* a8 */
	yas537_data.calib_yas537.a8 = ((a_data_u8[9] &
	0x7F) - 64);
	/* a9 */
	yas537_data.calib_yas537.a9 = ((((a_data_u8[10]
	<< BMI160_SHIFT_BIT_POSITION_BY_01_BIT) & 0x1FE)
	| (a_data_u8[11]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS))
	- 112);
	/* k */
	yas537_data.calib_yas537.k = (
	a_data_u8[11] & 0x7F);
	} else {
		return ERROR;
	}
/* write A/D converter*/
com_rslt += bmi160_set_mag_write_data(
YAS537_WRITE_A_D_CONVERTER);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_mag_write_addr(YAS537_REG_ADCCALR);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/* write A/D converter second register*/
com_rslt += bmi160_set_mag_write_data(
YAS537_WRITE_A_D_CONVERTER2);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_mag_write_addr(YAS537_REG_ADCCALR_ONE);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/* write temperature calibration register*/
com_rslt += bmi160_set_mag_write_data(YAS537_WRITE_TEMP_CALIB);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_mag_write_addr(YAS537_REG_TRMR);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
/* write average filter register*/
com_rslt += bmi160_set_mag_write_data(
v_avrr_u8[yas537_data.average]);
p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
com_rslt += bmi160_set_mag_write_addr(YAS537_REG_AVRR);
p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
if (v_rcoil_u8) {
	/* write average; filter register*/
	com_rslt += bmi160_set_mag_write_data(
	YAS537_WRITE_FILTER);
	p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
	com_rslt += bmi160_set_mag_write_addr(YAS537_REG_CONFR);
	p_bmi160->delay_msec(
	BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
}

return com_rslt;

}
/*!
 *	@brief This function used for YAS537 write data acquisition
 *	command register write
 *	@param	v_command_reg_data_u8	:	the value of data acquisition
 *	acquisition_command  |   operation
 *  ---------------------|-------------------------
 *         0x17          | turn on the acquisition coil
 *         -             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Deferred acquisition mode
 *        0x07           | turn on the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as minus(-))
 *         _             | Normal acquisition mode
 *        0x11           | turn OFF the acquisition coil
 *         _             | set direction of the coil
 *         _             | (x and y as plus(+))
 *         _             | Deferred acquisition mode
 *       0x01            | turn OFF the acquisition coil
 *        _              | set direction of the coil
 *        _              | (x and y as plus(+))
 *        _              | Normal acquisition mode
 *
 *
 *
  *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yas537_acquisition_command_register(
u8 v_command_reg_data_u8)
{
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)
			com_rslt = bmi160_set_mag_manual_enable(
			BMI160_MANUAL_ENABLE);
			p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

		com_rslt = bmi160_set_mag_write_data(v_command_reg_data_u8);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		/* YAMAHA YAS532-0x82*/
		com_rslt += bmi160_set_mag_write_addr(
		BMI160_REG_YAS537_CMDR);
		/* set the mode to RECORD*/
		yas537_data.measure_state = YAS537_MAG_STATE_RECORD_DATA;
		p_bmi160->delay_msec(BMI160_YAS_ACQ_COMMAND_DELAY);
		com_rslt += bmi160_set_mag_read_addr(
		YAS537_REG_TEMPERATURE_0);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE)
		com_rslt += bmi160_set_mag_manual_enable(
		BMI160_MANUAL_DISABLE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);

	return com_rslt;

}
/*!
 *	@brief This function used for read the
 *	YAMAHA YAS537 xy1y2 data
 *
 *	@param xy1y2: The value of raw xy1y2 data
 *	@param xyz: The value of  xyz data
 *
 *
 *	@return None
 *
 *
 */
static void xy1y2_to_xyz(u16 *xy1y2, s32 *xyz)
{
	xyz[0] = ((xy1y2[0] - 8192)
	* 300);
	xyz[1] = (((xy1y2[1] - xy1y2[2])
	* 1732) / 10);
	xyz[2] = (((-xy1y2[2] - xy1y2[2])
	+ 16384) * 300);
}
/*!
 *	@brief This function used for read the
 *	YAMAHA YAS537 xy1y2 data
 *
 *	@param v_coil_stat_u8: The value of R coil status
 *	@param v_busy_u8: The value of busy status
 *	@param v_temperature_u16: The value of temperature
 *	@param xy1y2: The value of raw xy1y2 data
 *	@param v_ouflow_u8: The value of overflow
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas537_read_xy1y2_data(
u8 *v_coil_stat_u8, u8 *v_busy_u8,
u16 *v_temperature_u16, u16 *xy1y2, u8 *v_ouflow_u8)
{
	/* This variable used for provide the communication
	results*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array holding the YAS532 calibration values */
	u8 a_data_u8[BMI160_YAS_XY1Y2T_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE,
	};
	u8 i = BMI160_INIT_VALUE;
	s32 a_h_s32[BMI160_YAS_H_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	s32 a_s_s32[BMI160_YAS_S_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	/* set command register*/
	com_rslt = bmi160_bst_yas537_acquisition_command_register(
	YAS537_SET_COMMAND_REGISTER);
	/* read the yas537 sensor data of xy1y2*/
	com_rslt +=
	p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
	BMI160_USER_DATA_MAG_X_LSB__REG,
	a_data_u8, BMI160_MAG_YAS_DATA_LENGTH);
	/* read the busy flag*/
	*v_busy_u8 = a_data_u8[2]
	>> BMI160_SHIFT_BIT_POSITION_BY_07_BITS;
	/* read the coil status*/
	*v_coil_stat_u8 =
	((a_data_u8[2] >>
	BMI160_SHIFT_BIT_POSITION_BY_06_BITS) & 0X01);
	/* read temperature data*/
	*v_temperature_u16 = (u16)((a_data_u8[0]
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS) | a_data_u8[1]);
	/* read x data*/
	xy1y2[0] = (u16)(((a_data_u8[2] &
	0x3F)
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
	| (a_data_u8[3]));
	/* read y1 data*/
	xy1y2[1] = (u16)((a_data_u8[4]
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
	| a_data_u8[5]);
	/* read y2 data*/
	xy1y2[2] = (u16)((a_data_u8[6]
	<< BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
	| a_data_u8[7]);
	for (i = 0; i < 3; i++)
		yas537_data.last_raw[i] = xy1y2[i];
	yas537_data.last_raw[i] = *v_temperature_u16;
	if (yas537_data.calib_yas537.ver == 1) {
		for (i = 0; i < 3; i++)
			a_s_s32[i] = xy1y2[i] - 8192;
		/* read hx*/
		a_h_s32[0] = ((yas537_data.calib_yas537.k * (
		(128 * a_s_s32[0]) +
		(yas537_data.calib_yas537.a2 * a_s_s32[1]) +
		(yas537_data.calib_yas537.a3 * a_s_s32[2])))
		/ (8192));
		/* read hy1*/
		a_h_s32[1] = ((yas537_data.calib_yas537.k * (
		(yas537_data.calib_yas537.a4 * a_s_s32[0]) +
		(yas537_data.calib_yas537.a5 * a_s_s32[1]) +
		(yas537_data.calib_yas537.a6 * a_s_s32[2])))
		/ (8192));
		/* read hy2*/
		a_h_s32[2] = ((yas537_data.calib_yas537.k * (
		(yas537_data.calib_yas537.a7 * a_s_s32[0]) +
		(yas537_data.calib_yas537.a8 * a_s_s32[1]) +
		(yas537_data.calib_yas537.a9 * a_s_s32[2])))
		/ (8192));

		for (i = 0; i < 3; i++) {
			if (a_h_s32[i] < -8192)
				a_h_s32[i] = -8192;

			if (8192 < a_h_s32[i])
				a_h_s32[i] = 8192;

			xy1y2[i] = a_h_s32[i] + 8192;

		}
	}
	*v_ouflow_u8 = 0;
	for (i = 0; i < 3; i++) {
		if (YAS537_DATA_OVERFLOW <= xy1y2[i])
			*v_ouflow_u8 |= (1 << (i * 2));
		if (xy1y2[i] == YAS537_DATA_UNDERFLOW)
			*v_ouflow_u8 |= (1 << (i * 2 + 1));
	}

	return com_rslt;

}
/*!
 *	@brief This function used for read the
 *	YAMAHA YAS537 xy1y2 data
 *
 *	@param v_ouflow_u8: The value of overflow
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
static BMI160_RETURN_FUNCTION_TYPE invalid_magnetic_field(
u16 *v_cur_u16, u16 *v_last_u16)
{
	s16 invalid_thresh[] = {1500, 1500, 1500};
	u8 i = BMI160_INIT_VALUE;

	for (i = 0; i < 3; i++)
		if (invalid_thresh[i] < ABS(v_cur_u16[i] - v_last_u16[i]))
			return 1;
	return 0;
}
/*!
 *	@brief This function used for read the
 *	YAMAHA YAS537 xy1y2 data
 *
 *	@param v_ouflow_u8: The value of overflow
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
 */
BMI160_RETURN_FUNCTION_TYPE bmi160_bst_yamaha_yas537_measure_xyz_data(
u8 *v_ouflow_u8, struct yas_vector *vector_xyz)
{
	s32 a_xyz_tmp_s32[BMI160_YAS_TEMP_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	u8 i = BMI160_INIT_VALUE;
	s8 com_rslt = BMI160_INIT_VALUE;
	u8 v_busy_u8 = BMI160_INIT_VALUE;
	u8 v_rcoil_u8 = BMI160_INIT_VALUE;
	u16 v_temperature_u16 = BMI160_INIT_VALUE;
	u16 a_xy1y2_u16[BMI160_YAS_XY1Y2_DATA_SIZE] = {
	BMI160_INIT_VALUE, BMI160_INIT_VALUE, BMI160_INIT_VALUE};
	*v_ouflow_u8 = 0;
	/* read the yas537 xy1y2 data*/
	com_rslt = bmi160_bst_yamaha_yas537_read_xy1y2_data(
	&v_rcoil_u8, &v_busy_u8,
	&v_temperature_u16, a_xy1y2_u16, v_ouflow_u8);
	/* linear calculation*/
	xy1y2_to_xyz(a_xy1y2_u16, vector_xyz->yas537_vector_xyz);
	if (yas537_data.transform != BMI160_NULL) {
		for (i = 0; i < 3; i++) {
			a_xyz_tmp_s32[i] = ((
			yas537_data.transform[i + 3]
			* vector_xyz->yas537_vector_xyz[0])
			+ (yas537_data.transform[
			i * 3 + 1]
			* vector_xyz->yas537_vector_xyz[1])
			+ (yas537_data.transform[
			i * 3 + 2]
			* vector_xyz->yas537_vector_xyz[2]));
		}
		yas537_set_vector(
		vector_xyz->yas537_vector_xyz, a_xyz_tmp_s32);
	}
	for (i = 0; i < 3; i++) {
		vector_xyz->yas537_vector_xyz[i] -=
		vector_xyz->yas537_vector_xyz[i] % 10;
		if (*v_ouflow_u8 & (1 <<
		(i * 2)))
			vector_xyz->yas537_vector_xyz[i] +=
			1; /* set overflow */
		if (*v_ouflow_u8 & (1 << (i * 2 + 1)))
			/* set underflow */
			vector_xyz->yas537_vector_xyz[i] += 2;
	}
	if (v_busy_u8)
		return ERROR;
	switch (yas537_data.measure_state) {
	case YAS537_MAG_STATE_INIT_COIL:
		if (p_bmi160->mag_manual_enable != BMI160_MANUAL_ENABLE)
			com_rslt = bmi160_set_mag_manual_enable(
			BMI160_MANUAL_ENABLE);
		com_rslt += bmi160_set_mag_write_data(YAS537_WRITE_CONFR);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		com_rslt += bmi160_set_mag_write_addr(YAS537_REG_CONFR);
		p_bmi160->delay_msec(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
		yas537_data.measure_state = YAS537_MAG_STATE_RECORD_DATA;
		if (p_bmi160->mag_manual_enable == BMI160_MANUAL_ENABLE)
			com_rslt = bmi160_set_mag_manual_enable(
			BMI160_MANUAL_DISABLE);
	break;
	case YAS537_MAG_STATE_RECORD_DATA:
		if (v_rcoil_u8)
			break;
		yas537_set_vector(yas537_data.last_after_rcoil, a_xy1y2_u16);
		yas537_data.measure_state = YAS537_MAG_STATE_NORMAL;
	break;
	case YAS537_MAG_STATE_NORMAL:
		if (BMI160_INIT_VALUE < v_ouflow_u8
		|| invalid_magnetic_field(a_xy1y2_u16,
		yas537_data.last_after_rcoil)) {
			yas537_data.measure_state = YAS537_MAG_STATE_INIT_COIL;
			for (i = 0; i < 3; i++) {
				if (!*v_ouflow_u8)
					vector_xyz->yas537_vector_xyz[i] += 3;
			}
		}
	break;
	}

	return com_rslt;
}
/*!
 *	@brief This function used for reading
 *	bmi160_t structure
 *
 *  @return the reference and values of bmi160_t
 *
 *
*/
struct bmi160_t *bmi160_get_ptr(void)
{
	return  p_bmi160;
}
