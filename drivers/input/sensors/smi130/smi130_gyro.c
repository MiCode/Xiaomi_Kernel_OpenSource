/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * (C) Modification Copyright 2018 Robert Bosch Kft  All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * Special: Description of the Software:
 *
 * This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. 
 *
 * As such, the Software is merely an experimental software, not tested for
 * safety in the field and only intended for inspiration for further development 
 * and testing. Any usage in a safety-relevant field of use (like automotive,
 * seafaring, spacefaring, industrial plants etc.) was not intended, so there are
 * no precautions for such usage incorporated in the Software.
 * 
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 * 
 * This Software package is provided as is and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 * 
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 * 
 *------------------------------------------------------------------------------
 * The following Product Disclaimer does not apply to the BSX4-HAL-4.1NoFusion Software 
 * which is licensed under the Apache License, Version 2.0 as stated above.  
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Product Disclaimer
 *
 * Common:
 *
 * Assessment of Products Returned from Field
 *
 * Returned products are considered good if they fulfill the specifications / 
 * test data for 0-mileage and field listed in this document.
 *
 * Engineering Samples
 * 
 * Engineering samples are marked with (e) or (E). Samples may vary from the
 * valid technical specifications of the series product contained in this
 * data sheet. Therefore, they are not intended or fit for resale to
 * third parties or for use in end products. Their sole purpose is internal
 * client testing. The testing of an engineering sample may in no way replace
 * the testing of a series product. Bosch assumes no liability for the use
 * of engineering samples. The purchaser shall indemnify Bosch from all claims
 * arising from the use of engineering samples.
 *
 * Intended use
 *
 * Provided that SMI130 is used within the conditions (environment, application,
 * installation, loads) as described in this TCD and the corresponding
 * agreed upon documents, Bosch ensures that the product complies with
 * the agreed properties. Agreements beyond this require
 * the written approval by Bosch. The product is considered fit for the intended
 * use when the product successfully has passed the tests
 * in accordance with the TCD and agreed upon documents.
 *
 * It is the responsibility of the customer to ensure the proper application
 * of the product in the overall system/vehicle.
 *
 * Bosch does not assume any responsibility for changes to the environment
 * of the product that deviate from the TCD and the agreed upon documents 
 * as well as all applications not released by Bosch
  *
 * The resale and/or use of products are at the purchaser’s own risk and 
 * responsibility. The examination and testing of the SMI130 
 * is the sole responsibility of the purchaser.
 *
 * The purchaser shall indemnify Bosch from all third party claims 
 * arising from any product use not covered by the parameters of 
 * this product data sheet or not approved by Bosch and reimburse Bosch 
 * for all costs and damages in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety, and inform Bosch without delay
 * of all security relevant incidents.
 *
 * Application Examples and Hints
 *
 * With respect to any application examples, advice, normal values
 * and/or any information regarding the application of the device,
 * Bosch hereby disclaims any and all warranties and liabilities of any kind,
 * including without limitation warranties of
 * non-infringement of intellectual property rights or copyrights
 * of any third party.
 * The information given in this document shall in no event be regarded 
 * as a guarantee of conditions or characteristics. They are provided
 * for illustrative purposes only and no evaluation regarding infringement
 * of intellectual property rights or copyrights or regarding functionality,
 * performance or error has been made.
 * @filename smi130_gyro.c
 * @date    2013/11/25
 * @Modification Date 2018/08/28 18:20
 * @id       "8fcde22"
 * @version  1.5
 *
 * @brief    SMI130_GYROAPI
*/

#include "smi130_gyro.h"
static struct smi130_gyro_t *p_smi130_gyro;


/*****************************************************************************
 * Description: *//**brief API Initialization routine
 *
 *
 *
 *
* \param smi130_gyro_t *smi130_gyro
 *      Pointer to a structure.
 *
 *       structure members are
 *
 *       unsigned char chip_id;
 *       unsigned char dev_addr;
 *       SMI130_GYRO_BRD_FUNC_PTR;
 *       SMI130_GYRO_WR_FUNC_PTR;
 *       SMI130_GYRO_RD_FUNC_PTR;
 *       void(*delay_msec)( SMI130_GYRO_MDELAY_DATA_TYPE );
 *
 *
 *
 *
 *
 *  \return result of communication routines
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_init(struct smi130_gyro_t *smi130_gyro)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	p_smi130_gyro = smi130_gyro;

	p_smi130_gyro->dev_addr = SMI130_GYRO_I2C_ADDR;

	/*Read CHIP_ID */
	comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
	 SMI130_GYRO_CHIP_ID_ADDR, &a_data_u8r, 1);
	p_smi130_gyro->chip_id = a_data_u8r;
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads Rate dataX from location 02h and 03h
 * registers
 *
 *
 *
 *
 *  \param
 *      SMI130_GYRO_S16  *data_x   :  Address of data_x
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataX(SMI130_GYRO_S16 *data_x)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATE_X_LSB_VALUEX__REG, a_data_u8r, 2);
		a_data_u8r[0] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[0],
		SMI130_GYRO_RATE_X_LSB_VALUEX);
		*data_x = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[1])) <<
		SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[0]));
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads rate dataY from location 04h and 05h
 * registers
 *
 *
 *
 *
 *  \param
 *      SMI130_GYRO_S16  *data_y   :  Address of data_y
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataY(SMI130_GYRO_S16 *data_y)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATE_Y_LSB_VALUEY__REG, a_data_u8r, 2);
		a_data_u8r[0] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[0],
		SMI130_GYRO_RATE_Y_LSB_VALUEY);
		*data_y = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[1]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[0]));
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads rate dataZ from location 06h and 07h
 * registers
 *
 *
 *
 *
 *  \param
 *      SMI130_GYRO_S16  *data_z   :  Address of data_z
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataZ(SMI130_GYRO_S16 *data_z)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r[2] = {0, 0};
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATE_Z_LSB_VALUEZ__REG, a_data_u8r, 2);
		a_data_u8r[0] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[0],
		SMI130_GYRO_RATE_Z_LSB_VALUEZ);
		*data_z = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[1]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[0]));
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads data X,Y and Z from location 02h to 07h
 *
 *
 *
 *
 *  \param
 *      smi130_gyro_data_t *data   :  Address of smi130_gyro_data_t
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataXYZ(struct smi130_gyro_data_t *data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r[6] = {0, 0, 0, 0, 0, 0};
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATE_X_LSB_VALUEX__REG, a_data_u8r, 6);
		/* Data X */
		a_data_u8r[0] =
		SMI130_GYRO_GET_BITSLICE(a_data_u8r[0], SMI130_GYRO_RATE_X_LSB_VALUEX);
		data->datax = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[1]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[0]));
		/* Data Y */
		a_data_u8r[2] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[2],
		SMI130_GYRO_RATE_Y_LSB_VALUEY);
		data->datay = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[3]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[2]));
		/* Data Z */
		a_data_u8r[4] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[4],
		SMI130_GYRO_RATE_Z_LSB_VALUEZ);
		data->dataz = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[5]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[4]));
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads data X,Y,Z and Interrupts
 *							from location 02h to 07h
 *
 *
 *
 *
 *  \param
 *      smi130_gyro_data_t *data   :  Address of smi130_gyro_data_t
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataXYZI(struct smi130_gyro_data_t *data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char a_data_u8r[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATE_X_LSB_VALUEX__REG, a_data_u8r, 12);
		/* Data X */
		a_data_u8r[0] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[0],
		SMI130_GYRO_RATE_X_LSB_VALUEX);
		data->datax = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[1]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[0]));
		/* Data Y */
		a_data_u8r[2] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[2],
		SMI130_GYRO_RATE_Y_LSB_VALUEY);
		data->datay = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[3]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[2]));
		/* Data Z */
		a_data_u8r[4] = SMI130_GYRO_GET_BITSLICE(a_data_u8r[4],
		SMI130_GYRO_RATE_Z_LSB_VALUEZ);
		data->dataz = (SMI130_GYRO_S16)
		((((SMI130_GYRO_S16)((signed char)a_data_u8r[5]))
		<< SMI130_GYRO_SHIFT_8_POSITION) | (a_data_u8r[4]));
		data->intstatus[0] = a_data_u8r[7];
		data->intstatus[1] = a_data_u8r[8];
		data->intstatus[2] = a_data_u8r[9];
		data->intstatus[3] = a_data_u8r[10];
		data->intstatus[4] = a_data_u8r[11];
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads Temperature from location 08h
 *
 *
 *
 *
 *  \param
 *      unsigned char *temp   :  Address of temperature
 *
 *
 *  \return
 *      result of communication routines
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_Temperature(unsigned char *temperature)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_TEMP_ADDR, &v_data_u8r, 1);
		*temperature = v_data_u8r;
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the data from the given register
 *
 *
 *
 *
 *\param unsigned char addr, unsigned char *data unsigned char len
 *                       addr -> Address of the register
 *                       data -> address of the variable, read value will be
 *								kept
 *						len -> No of byte to be read.
 *  \return  results of bus communication function
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_read_register(unsigned char addr,
unsigned char *data, unsigned char len)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr, addr, data, len);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the data from the given register
 *
 *
 *
 *
 *\param unsigned char addr, unsigned char *data SMI130_GYRO_S32 len
 *                       addr -> Address of the register
 *                       data -> address of the variable, read value will be
 *								kept
 *						len -> No of byte to be read.
 *  \return  results of bus communication function
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_burst_read(unsigned char addr,
unsigned char *data, SMI130_GYRO_S32 len)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BURST_READ_FUNC(p_smi130_gyro->dev_addr,
		addr, data, len);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API given data to the given register
 *
 *
 *
 *
 *\param unsigned char addr, unsigned char data,unsigned char len
 *                   addr -> Address of the register
 *                   data -> Data to be written to the register
 *					len -> No of byte to be read.
 *
 *  \return Results of bus communication function
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_write_register(unsigned char addr,
unsigned char *data, unsigned char len)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
		(p_smi130_gyro->dev_addr, addr, data, len);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt status 0 register byte from 09h
 *
 *
 *
 *
 *  \param
 *      unsigned char *status0_data : Address of status 0 register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_0(
unsigned char *status0_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_STATUSZERO__REG, &v_data_u8r, 1);
		*status0_data =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_INT_STATUSZERO);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt status 1 register byte from 0Ah
 *
 *
 *
 *
 *  \param
 *      unsigned char *status1_data : Address of status register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_1(
unsigned char *status1_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr, SMI130_GYRO_INT_STATUSONE__REG,
		&v_data_u8r, 1);
		*status1_data =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_INT_STATUSONE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt status register byte from 0Bh
 *
 *
 *
 *
 *  \param
 *      unsigned char *status2_data : Address of status 2 register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_2(
unsigned char *status2_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_STATUSTWO__REG, &v_data_u8r, 1);
		*status2_data =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_INT_STATUSTWO);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt status 3 register byte from 0Ch
 *
 *
 *
 *
 *  \param
 *      unsigned char *status3_data : Address of status 3 register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_3(
unsigned char *status3_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_STATUSTHREE__REG, &v_data_u8r, 1);
		*status3_data =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_INT_STATUSTHREE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the range from register 0x0Fh of
 * (0 to 2) bits
 *
 *
 *
 *
 *\param unsigned char *range
 *      Range[0....7]
 *      0 2000/s
 *      1 1000/s
 *      2 500/s
 *      3 250/s
 *      4 125/s
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_range_reg(unsigned char *range)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RANGE_ADDR_RANGE__REG, &v_data_u8r, 1);
		*range =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_RANGE_ADDR_RANGE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API sets the range register 0x0Fh
 * (0 to 2 bits)
 *
 *
 *
 *
 *\param unsigned char range
 *
 *      Range[0....7]
 *      0 2000/s
 *      1 1000/s
 *      2 500/s
 *      3 250/s
 *      4 125/s
 *
 *
 *
 *
 *  \return Communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_range_reg(unsigned char range)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (range < C_SMI130_GYRO_Five_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RANGE_ADDR_RANGE__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_RANGE_ADDR_RANGE,
			range);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
			(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RANGE_ADDR_RANGE__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the high resolution bit of 0x10h
 * Register 7th bit
 *
 *
 *
 *
 *\param unsigned char *high_res
 *                      Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_res(unsigned char *high_res)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr,
		SMI130_GYRO_BW_ADDR_HIGH_RES__REG, &v_data_u8r, 1);
		*high_res =
		SMI130_GYRO_GET_BITSLICE(v_data_u8r, SMI130_GYRO_BW_ADDR_HIGH_RES);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the bandwidth register of 0x10h 0 to
 *  3 bits
 *
 *
 *
 *
* \param unsigned char *bandwidth
 *              pointer to a variable passed as a parameter
 *
 *              0 no filter(523 Hz)
 *              1 230Hz
 *              2 116Hz
 *              3 47Hz
 *              4 23Hz
 *              5 12Hz
 *              6 64Hz
 *              7 32Hz
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_bw(unsigned char *bandwidth)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
		(p_smi130_gyro->dev_addr, SMI130_GYRO_BW_ADDR__REG, &v_data_u8r, 1);
		*bandwidth = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BW_ADDR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes the Bandwidth register (0x10h of 0
 * to 3 bits)
 *
 *
 *
 *
 *\param unsigned char bandwidth,
 *              The bandwidth to be set passed as a parameter
 *
 *              0 no filter(523 Hz)
 *              1 230Hz
 *              2 116Hz
 *              3 47Hz
 *              4 23Hz
 *              5 12Hz
 *              6 64Hz
 *              7 32Hz
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_bw(unsigned char bandwidth)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_mode_u8r  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_autosleepduration  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (bandwidth < C_SMI130_GYRO_Eight_U8X) {
			smi130_gyro_get_mode(&v_mode_u8r);
			if (v_mode_u8r == SMI130_GYRO_MODE_ADVANCEDPOWERSAVING) {
				smi130_gyro_get_autosleepdur(&v_autosleepduration);
				smi130_gyro_set_autosleepdur(v_autosleepduration,
				bandwidth);
			}
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BW_ADDR__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_BW_ADDR, bandwidth);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BW_ADDR__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the status of External Trigger
 * selection bits (4 and 5) of 0x12h registers
 *
 *
 *
 *
 *\param unsigned char *pwu_ext_tri_sel
 *                      Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return Communication Results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_pmu_ext_tri_sel(
unsigned char *pwu_ext_tri_sel)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__REG, &v_data_u8r, 1);
		*pwu_ext_tri_sel = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes the External Trigger selection
 * bits (4 and 5) of 0x12h registers
 *
 *
 *
 *
 *\param unsigned char pwu_ext_tri_sel
 *               Value to be written passed as a parameter
 *
 *
 *
 *  \return Communication Results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_pmu_ext_tri_sel(
unsigned char pwu_ext_tri_sel)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL, pwu_ext_tri_sel);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief  This API is used to get data high bandwidth
 *
 *
 *
 *
 *\param unsigned char *high_bw : Address of high_bw
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_bw(unsigned char *high_bw)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__REG, &v_data_u8r, 1);
		*high_bw = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set data high bandwidth
 *
 *
 *
 *
 *\param unsigned char high_bw:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_bw(unsigned char high_bw)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (high_bw < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW, high_bw);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__REG,
			&v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get shadow dis
 *
 *
 *
 *
 *\param unsigned char *shadow_dis : Address of shadow_dis
 *                       Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_shadow_dis(unsigned char *shadow_dis)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__REG, &v_data_u8r, 1);
		*shadow_dis = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set shadow dis
 *
 *
 *
 *
 *\param unsigned char shadow_dis
 *         Value to be written passed as a parameter
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_shadow_dis(unsigned char shadow_dis)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (shadow_dis < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS, shadow_dis);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief
 *               This function is used for the soft reset
 *     The soft reset register will be written with 0xB6.
 *
 *
 *
* \param None
 *
 *
 *
 *  \return Communication results.
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_soft_reset()
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_SoftReset_u8r  = C_SMI130_GYRO_Zero_U8X;
	v_SoftReset_u8r = 0xB6;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_BGW_SOFTRESET_ADDR, &v_SoftReset_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get data enable data
 *
 *
 *
 *
 *\param unsigned char *data_en : Address of data_en
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_data_enable(unsigned char *data_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_ENABLE0_DATAEN__REG, &v_data_u8r, 1);
		*data_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE0_DATAEN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set data enable data
 *
 *
 *
 *
 *  \param unsigned char data_en:
 *          Value to be written passed as a \parameter
 *           0 --> Disable
 *           1 --> Enable
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_data_en(unsigned char data_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_ENABLE0_DATAEN__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_ENABLE0_DATAEN, data_en);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
			(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_ENABLE0_DATAEN__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get fifo enable bit
 *
 *
 *
 *
 *  \param unsigned char *fifo_en : Address of fifo_en
 *                         Pointer to a variable passed as a parameter

 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_enable(unsigned char *fifo_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_ENABLE0_FIFOEN__REG, &v_data_u8r, 1);
		*fifo_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE0_FIFOEN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set fifo enable bit
 *
 *
 *
 *
 *  \param unsigned char fifo_en:
 *          Value to be written passed as a parameter
 *           0 --> Disable
 *           1 --> Enable
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_enable(unsigned char fifo_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (fifo_en < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE0_FIFOEN__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE0_FIFOEN, fifo_en);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE0_FIFOEN__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API reads the status of the Auto offset
 * Enable bit
 *                      (0x15 Reg 3rd Bit)
 *
 *
 *
 *
 *  \param unsigned char *offset_en
 *              address of a variable,
 *
 *
 *
 *  \return   Communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_auto_offset_en(
unsigned char *offset_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__REG, &v_data_u8r, 1);
		*offset_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API sets the Auto offset enable bit
 *                      (Reg 0x15 3rd Bit)
 *
 *
 *
 *
 *  \param unsigned char offset_en
 *                      0 --> Disable
 *                      1 --> Enable
 *
 *  \return  Communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_auto_offset_en(unsigned char offset_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN, offset_en);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the output type status
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_od
 *                  SMI130_GYRO_INT1    ->   0
 *                  SMI130_GYRO_INT2    ->   1
 *                  int_od : open drain   ->   1
 *                           push pull    ->   0
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_od(unsigned char param,
unsigned char *int_od)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_INT1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_INT_ENABLE1_IT1_OD__REG, &v_data_u8r, 1);
			*int_od = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT1_OD);
			break;
		case SMI130_GYRO_INT2:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_INT_ENABLE1_IT2_OD__REG, &v_data_u8r, 1);
			*int_od = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT2_OD);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the output type status
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_od
 *                  SMI130_GYRO_INT1    ->   0
 *                  SMI130_GYRO_INT2    ->   1
 *                  int_od : open drain   ->   1
 *                           push pull    ->   0
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_od(unsigned char param,
unsigned char int_od)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_INT1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT1_OD__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT1_OD, int_od);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT1_OD__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_INT2:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT2_OD__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT2_OD, int_od);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT2_OD__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Active Level status
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_lvl
 *                  SMI130_GYRO_INT1    ->    0
 *                  SMI130_GYRO_INT2    ->    1
 *                  int_lvl : Active HI   ->   1
 *                            Active LO   ->   0
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_lvl(unsigned char param,
unsigned char *int_lvl)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_INT1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_INT_ENABLE1_IT1_LVL__REG, &v_data_u8r, 1);
			*int_lvl = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT1_LVL);
			break;
		case SMI130_GYRO_INT2:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_INT_ENABLE1_IT2_LVL__REG, &v_data_u8r, 1);
			*int_lvl = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT2_LVL);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Active Level status
 *
 *
 *
 *
 *  \param unsigned char channel,unsigned char *int_lvl
 *                  SMI130_GYRO_INT1    ->    0
 *                  SMI130_GYRO_INT2    ->    1
 *                  int_lvl : Active HI   ->   1
 *                            Active LO   ->   0
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_lvl(unsigned char param,
unsigned char int_lvl)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_INT1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT1_LVL__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT1_LVL, int_lvl);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT1_LVL__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_INT2:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT2_LVL__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_ENABLE1_IT2_LVL, int_lvl);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_ENABLE1_IT2_LVL__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get High Interrupt1
 *
 *
 *
 *
 *  \param unsigned char *int1_high : Address of high_bw
 *                         Pointer to a variable passed as a parameter

 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_high(unsigned char *int1_high)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_MAP_0_INT1_HIGH__REG, &v_data_u8r, 1);
		*int1_high = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_MAP_0_INT1_HIGH);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set High Interrupt1
 *
 *
 *
 *
 *  \param unsigned char int1_high
 *                  0 -> Disable
 *                  1 -> Enable
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_high(unsigned char int1_high)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_0_INT1_HIGH__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_MAP_0_INT1_HIGH, int1_high);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_0_INT1_HIGH__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Any Interrupt1
 *
 *
 *
 *
 *  \param unsigned char *int1_any : Address of high_bw
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_any(unsigned char *int1_any)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_MAP_0_INT1_ANY__REG, &v_data_u8r, 1);
		*int1_any = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_MAP_0_INT1_ANY);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Any Interrupt1
 *
 *
 *
 *
 *\param unsigned char int1_any
 *                   0 -> Disable
 *                   1 -> Enable
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_any(unsigned char int1_any)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_0_INT1_ANY__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_MAP_0_INT1_ANY, int1_any);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_0_INT1_ANY__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get data Interrupt1 and data
 * Interrupt2
 *
 *
 *
 *
 *  \param unsigned char axis,unsigned char *int_data
 *                       axis :
 *                       SMI130_GYRO_INT1_DATA -> 0
 *                       SMI130_GYRO_INT2_DATA -> 1
 *                       int_data :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_data(unsigned char axis,
unsigned char *int_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_INT1_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT1_DATA__REG, &v_data_u8r, 1);
			*int_data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_MAP_1_INT1_DATA);
			break;
		case SMI130_GYRO_INT2_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT2_DATA__REG, &v_data_u8r, 1);
			*int_data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_MAP_1_INT2_DATA);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set data Interrupt1 and data
 * Interrupt2
 *
 *
 *
 *
 * \param unsigned char axis,unsigned char *int_data
 *                       axis :
 *                       SMI130_GYRO_INT1_DATA -> 0
 *                       SMI130_GYRO_INT2_DATA -> 1
 *                       int_data :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_data(unsigned char axis,
unsigned char int_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	}   else {
			switch (axis) {
			case SMI130_GYRO_INT1_DATA:
				comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
					(p_smi130_gyro->dev_addr,
				SMI130_GYRO_MAP_1_INT1_DATA__REG, &v_data_u8r, 1);
				v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_MAP_1_INT1_DATA, int_data);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
					(p_smi130_gyro->dev_addr,
				SMI130_GYRO_MAP_1_INT1_DATA__REG, &v_data_u8r, 1);
				break;
			case SMI130_GYRO_INT2_DATA:
				comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
					(p_smi130_gyro->dev_addr,
				SMI130_GYRO_MAP_1_INT2_DATA__REG, &v_data_u8r, 1);
				v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_MAP_1_INT2_DATA, int_data);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
					(p_smi130_gyro->dev_addr,
				SMI130_GYRO_MAP_1_INT2_DATA__REG, &v_data_u8r, 1);
				break;
			default:
				comres = E_SMI130_GYRO_OUT_OF_RANGE;
				break;
			}
		}
		return comres;
	}

/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get fast offset and auto
 * offset Interrupt2
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char *int2_offset
 *                       axis :
 *                       SMI130_GYRO_AUTO_OFFSET -> 1
 *                       SMI130_GYRO_FAST_OFFSET -> 2
 *                       int2_offset :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_offset(unsigned char axis,
unsigned char *int2_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__REG, &v_data_u8r, 1);
			*int2_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_FAST_OFFSET);
			break;
		case SMI130_GYRO_AUTO_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__REG, &v_data_u8r, 1);
			*int2_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set fast offset and auto
 * offset Interrupt2
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char *int2_offset
 *                       axis :
 *                       SMI130_GYRO_AUTO_OFFSET -> 1
 *                       SMI130_GYRO_FAST_OFFSET -> 2
 *                       int2_offset :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_offset(unsigned char axis,
unsigned char int2_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_FAST_OFFSET, int2_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_AUTO_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET, int2_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get fast offset and auto
 * offset Interrupt1
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char *int1_offset
 *                       axis :
 *                       SMI130_GYRO_AUTO_OFFSET -> 1
 *                       SMI130_GYRO_FAST_OFFSET -> 2
 *                       int2_offset :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_offset(unsigned char axis,
unsigned char *int1_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__REG, &v_data_u8r, 1);
			*int1_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_FAST_OFFSET);
			break;
		case SMI130_GYRO_AUTO_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__REG, &v_data_u8r, 1);
			*int1_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set fast offset and auto
 * offset Interrupt1
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char *int1_offset
 *                       axis :
 *                       SMI130_GYRO_AUTO_OFFSET -> 1
 *                       SMI130_GYRO_FAST_OFFSET -> 2
 *                       int2_offset :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_offset(unsigned char axis,
unsigned char int1_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_FAST_OFFSET, int1_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_AUTO_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET, int1_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get status of FIFO Interrupt
 *
 *
 *
 *
 *\param unsigned char *int_fifo : Address of int_fifo
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_fifo(unsigned char *int_fifo)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_INT_STATUS1_FIFO_INT__REG, &v_data_u8r, 1);
		*int_fifo = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_STATUS1_FIFO_INT);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get FIFO Interrupt2
 *
 *
 *
 *
 *\param unsigned char *int_fifo
 *                  int_fifo :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_fifo(unsigned char *int_fifo)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_MAP_1_INT2_FIFO__REG, &v_data_u8r, 1);
		*int_fifo = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_FIFO);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get FIFO Interrupt1
 *
 *
 *
 *
 *\param unsigned char *int_fifo
 *                  int_fifo :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_fifo(unsigned char *int_fifo)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_MAP_1_INT1_FIFO__REG, &v_data_u8r, 1);
		*int_fifo = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_FIFO);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_fifo(unsigned char axis,
unsigned char int_fifo)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_INT1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			 SMI130_GYRO_MAP_1_INT1_FIFO__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_FIFO, int_fifo);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_FIFO__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_INT2:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FIFO__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_FIFO, int_fifo);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FIFO__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set FIFO Interrupt1
 *
 *
 *
 *
 *\param unsigned char *fifo_int1
 *                  fifo_int1 :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_fifo(unsigned char fifo_int1)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (fifo_int1 < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_FIFO__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT1_FIFO, fifo_int1);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT1_FIFO__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set FIFO Interrupt2
 *
 *
 *
 *
 *\param unsigned char *fifo_int2
 *                  fifo_int2 :
 *                       Disable     -> 0
 *                       Enable      -> 1
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_fifo(unsigned char fifo_int2)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (fifo_int2 < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FIFO__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MAP_1_INT2_FIFO, fifo_int2);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MAP_1_INT2_FIFO__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get High Interrupt2
 *
 *
 *
 *
 *\param unsigned char *int2_high : Address of int2_high
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_high(unsigned char *int2_high)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_HIGH__REG, &v_data_u8r, 1);
		*int2_high = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_MAP_2_INT2_HIGH);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get High Interrupt2
 *
 *
 *
 *
 *\param unsigned char int2_high
 *                  0 -> Disable
 *                  1 -> Enable
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_high(unsigned char int2_high)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_HIGH__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_MAP_2_INT2_HIGH, int2_high);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_HIGH__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Any Interrupt2
 *
 *
 *
 *
 *\param unsigned char *int2_any : Address of int2_any
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_any(unsigned char *int2_any)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_ANY__REG, &v_data_u8r, 1);
		*int2_any = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_MAP_2_INT2_ANY);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Any Interrupt2
 *
 *
 *
 *
 *\param unsigned char int2_any
 *                  0 -> Disable
 *                  1 -> Enable
 *
 *
 *
 *
 * \return  communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_any(unsigned char int2_any)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_ANY__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_MAP_2_INT2_ANY, int2_any);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_MAP_2_INT2_ANY__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get slow offset and fast
 * offset unfilt data
 *
 *
 *
 *\param unsigned char param,unsigned char *offset_unfilt
 *                  param :
 *                  SMI130_GYRO_SLOW_OFFSET -> 0
 *                  SMI130_GYRO_FAST_OFFSET -> 2
 *                  offset_unfilt: Enable  -> 1
 *                                Disable -> 0
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset_unfilt(unsigned char param,
unsigned char *offset_unfilt)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_SLOW_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			*offset_unfilt = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT);
			break;
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			*offset_unfilt = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set slow offset and fast
 * offset unfilt data
 *
 *
 *
 *
 *\param unsigned char param,unsigned char *offset_unfilt
 *                  param :
 *                  SMI130_GYRO_SLOW_OFFSET -> 0
 *                  SMI130_GYRO_FAST_OFFSET -> 2
 *                  offset_unfilt: Enable  -> 1
 *                                Disable -> 0
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_unfilt(unsigned char param,
unsigned char offset_unfilt)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_SLOW_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT, offset_unfilt);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			break;
		case SMI130_GYRO_FAST_OFFSET:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT, offset_unfilt);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__REG,
			&v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Tap, High, Constant, Any,
 * Shake unfilt data
 *
 *
 *
 *
 *\param unsigned char param,unsigned char *unfilt_data
 *                  param :
 *
 *                  SMI130_GYRO_HIGH_UNFILT_DATA      -> 1
 *                  SMI130_GYRO_ANY_UNFILT_DATA       -> 3
 *
 *                  unfilt_data:   Enable  -> 1
 *                                Disable -> 0
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_unfilt_data(unsigned char param,
unsigned char *unfilt_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_HIGH_UNFILT_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__REG,
			&v_data_u8r, 1);
			*unfilt_data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA);
			break;
		case SMI130_GYRO_ANY_UNFILT_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__REG, &v_data_u8r, 1);
			*unfilt_data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Tap, High, Constant, Any,
 * Shake unfilt data
 *
 *
 *
 *
 *\param unsigned char param,unsigned char *unfilt_data
 *                  param :
 *
 *                  SMI130_GYRO_HIGH_UNFILT_DATA      -> 1
 *                  SMI130_GYRO_ANY_UNFILT_DATA       -> 3
 *
 *                  unfilt_data:   Enable  -> 1
 *                                Disable -> 0
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_unfilt_data(unsigned char param,
unsigned char unfilt_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_HIGH_UNFILT_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA, unfilt_data);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__REG,
			&v_data_u8r, 1);
			break;
		case SMI130_GYRO_ANY_UNFILT_DATA:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA, unfilt_data);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Any Threshold
 *
 *
 *
 *
 *\param unsigned char *any_th : Address of any_th
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_th(unsigned char *any_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_1_ADDR_ANY_TH__REG, &v_data_u8r, 1);
		*any_th = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_1_ADDR_ANY_TH);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Any Threshold
 *
 *
 *
 *
 *\param unsigned char any_th:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_th(unsigned char any_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_1_ADDR_ANY_TH__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_1_ADDR_ANY_TH, any_th);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_1_ADDR_ANY_TH__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Awake Duration
 *
 *
 *
 *
 *\param unsigned char *awake_dur : Address of awake_dur
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_awake_dur(unsigned char *awake_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__REG, &v_data_u8r, 1);
		*awake_dur = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_2_ADDR_AWAKE_DUR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Awake Duration
 *
 *
 *
 *
 *\param unsigned char awake_dur:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************
 * Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_awake_dur(unsigned char awake_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_2_ADDR_AWAKE_DUR, awake_dur);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Any Duration Sample
 *
 *
 *
 *
 *\param unsigned char *dursample : Address of dursample
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_dursample(unsigned char *dursample)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__REG, &v_data_u8r, 1);
		*dursample = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Any Duration Sample
 *
 *
 *
 *
 *\param unsigned char dursample:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_dursample(unsigned char dursample)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE, dursample);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of Any Enable
 * Channel X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *data
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       data :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_en_ch(unsigned char channel,
unsigned char *data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_X__REG, &v_data_u8r, 1);
			*data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_X);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__REG, &v_data_u8r, 1);
			*data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_INT_2_ADDR_ANY_EN_Y);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__REG, &v_data_u8r, 1);
			*data = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_INT_2_ADDR_ANY_EN_Z);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of Any Enable
 * Channel X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *data
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       data :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_en_ch(unsigned char channel,
unsigned char data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_X, data);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Y, data);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Z, data);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of FIFO WM
 * Enable
 *
 *
 *
 *
 *\param unsigned char *fifo_wn_en
 *                       Enable  -> 1
 *                       Disable -> 0
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_watermark_enable(
unsigned char *fifo_wn_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_INT_4_FIFO_WM_EN__REG, &v_data_u8r, 1);
		*fifo_wn_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_4_FIFO_WM_EN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set FIFO WM Enable
 *
 *
 *
 *
 *\param unsigned char *fifo_wn_en
 *                       Enable  -> 1
 *                       Disable -> 0
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_watermark_enable(
unsigned char fifo_wn_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (fifo_wn_en < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_4_FIFO_WM_EN__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_INT_4_FIFO_WM_EN, fifo_wn_en);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_INT_4_FIFO_WM_EN__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the Interrupt Reset
 *
 *
 *
 *
 *\param unsigned char reset_int
 *                    1 -> Reset All Interrupts
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_reset_int(unsigned char reset_int)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_RESET_INT, reset_int);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the Offset Reset
 *
 *
 *
 *
 *\param unsigned char offset_reset
 *                  1 -> Resets All the Offsets
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_reset(
unsigned char offset_reset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET, offset_reset);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the Latch Status
 *
 *
 *
 *
 *\param unsigned char *latch_status : Address of latch_status
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_latch_status(
unsigned char *latch_status)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__REG, &v_data_u8r, 1);
		*latch_status = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the Latch Status
 *
 *
 *
 *
 *\param unsigned char latch_status:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_latch_status(
unsigned char latch_status)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS, latch_status);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the Latch Interrupt
 *
 *
 *
 *
 *\param unsigned char *latch_int : Address of latch_int
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_latch_int(unsigned char *latch_int)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__REG, &v_data_u8r, 1);
		*latch_int = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the Latch Interrupt
 *
 *
 *
 *
 *\param unsigned char latch_int:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_latch_int(unsigned char latch_int)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT, latch_int);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of High
 * Hysteresis X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_hy
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_hy :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_hy(unsigned char channel,
unsigned char *high_hy)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_X__REG, &v_data_u8r, 1);
			*high_hy = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_HY_X);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Y__REG, &v_data_u8r, 1);
			*high_hy = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_HY_Y);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Z__REG, &v_data_u8r, 1);
			*high_hy = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_HY_Z);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of High
 * Hysteresis X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_hy
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_hy :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_hy(unsigned char channel,
unsigned char high_hy)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_HIGH_HY_X, high_hy);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_HIGH_HY_Y, high_hy);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_HIGH_HY_Z, high_hy);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_HY_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of High
 * Threshold X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_th
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_th :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_th(unsigned char channel,
unsigned char *high_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_X__REG, &v_data_u8r, 1);
			*high_th = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_X);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Y__REG, &v_data_u8r, 1);
			*high_th = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_Y);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Z__REG, &v_data_u8r, 1);
			*high_th = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_Z);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of High
 * Threshold X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_th
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_th :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_th(unsigned char channel,
unsigned char high_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_X, high_th);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_Y, high_th);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_TH_Z, high_th);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_TH_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of High Enable
 * Channel X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_en
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_en :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_en_ch(unsigned char channel,
unsigned char *high_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_X__REG, &v_data_u8r, 1);
			*high_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_X);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Y__REG, &v_data_u8r, 1);
			*high_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_Y);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Z__REG, &v_data_u8r, 1);
			*high_en = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_Z);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of High Enable
 * Channel X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_en
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_en :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_en_ch(unsigned char channel,
unsigned char high_en)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_X, high_en);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_Y, high_en);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_HIGH_EN_Z, high_en);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_EN_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get High Duration
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_dur
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       *high_dur : Address of high_bw
 *                                   Pointer to a variable passed as a
 *                                   parameter
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_dur_ch(unsigned char channel,
unsigned char *high_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_X_ADDR, &v_data_u8r, 1);
			*high_dur = v_data_u8r;
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_Y_ADDR, &v_data_u8r, 1);
			*high_dur = v_data_u8r;
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_Z_ADDR, &v_data_u8r, 1);
			*high_dur = v_data_u8r;
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set High Duration
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *high_dur
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       high_dur : Value to be written passed as a parameter
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_dur_ch(unsigned char channel,
unsigned char high_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			v_data_u8r = high_dur;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_X_ADDR, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			v_data_u8r = high_dur;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_Y_ADDR, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			v_data_u8r = high_dur;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_HIGH_DUR_Z_ADDR, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Slow Offset Threshold
 *
 *
 *
 *
 *\param unsigned char *offset_th : Address of offset_th
 *                         Pointer to a variable passed as a parameter

 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_th(
unsigned char *offset_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_TH__REG, &v_data_u8r, 1);
		*offset_th = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_TH);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Slow Offset Threshold
 *
 *
 *
 *
 *\param unsigned char offset_th:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_th(unsigned char offset_th)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_TH__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_SLOW_OFFSET_TH, offset_th);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_TH__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Slow Offset Duration
 *
 *
 *
 *
 *\param unsigned char *offset_dur : Address of offset_dur
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_dur(
unsigned char *offset_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_DUR__REG, &v_data_u8r, 1);
		*offset_dur = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_DUR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Slow Offset Duration
 *
 *
 *
 *
 *\param unsigned char offset_dur:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_dur(
unsigned char offset_dur)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_DUR__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_SLOW_OFFSET_DUR, offset_dur);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_SLOW_OFFSET_DUR__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Slow Offset Enable channel
 * X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *slow_offset
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       slow_offset :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_en_ch(
unsigned char channel, unsigned char *slow_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_X__REG, &v_data_u8r, 1);
			*slow_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_EN_X);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Y__REG, &v_data_u8r, 1);
			*slow_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_EN_Y);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Z__REG, &v_data_u8r, 1);
			*slow_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_EN_Z);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Slow Offset Enable channel
 * X,Y,Z
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *slow_offset
 *                       channel :
 *                       SMI130_GYRO_X_AXIS -> 0
 *                       SMI130_GYRO_Y_AXIS -> 1
 *                       SMI130_GYRO_Z_AXIS -> 2
 *                       slow_offset :
 *                       Enable  -> 1
 *                       disable -> 0
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_en_ch(
unsigned char channel, unsigned char slow_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_EN_X, slow_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_SLOW_OFFSET_EN_Y, slow_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
				SMI130_GYRO_SLOW_OFFSET_EN_Z,
			slow_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_SLOW_OFFSET_EN_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Fast Offset WordLength and
 * Auto Offset WordLength
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *offset_wl
 *                       channel :
 *                       SMI130_GYRO_AUTO_OFFSET_WL -> 0
 *                       SMI130_GYRO_FAST_OFFSET_WL -> 1
 *                       *offset_wl : Address of high_bw
 *                                    Pointer to a variable passed as a
 *                                    parameter
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset_wl(unsigned char channel,
unsigned char *offset_wl)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_AUTO_OFFSET_WL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_AUTO_OFFSET_WL__REG, &v_data_u8r, 1);
			*offset_wl = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_AUTO_OFFSET_WL);
			break;
		case SMI130_GYRO_FAST_OFFSET_WL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_WL__REG, &v_data_u8r, 1);
			*offset_wl = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_FAST_OFFSET_WL);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Fast Offset WordLength and
 *  Auto Offset WordLength
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *offset_wl
 *                       channel :
 *                       SMI130_GYRO_AUTO_OFFSET_WL -> 0
 *                       SMI130_GYRO_FAST_OFFSET_WL -> 1
 *                       offset_wl : Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_wl(
unsigned char channel, unsigned char offset_wl)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_AUTO_OFFSET_WL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_AUTO_OFFSET_WL__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_AUTO_OFFSET_WL, offset_wl);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_AUTO_OFFSET_WL__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_FAST_OFFSET_WL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_WL__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FAST_OFFSET_WL, offset_wl);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_WL__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to enable fast offset
 *
 *
 *
 *
* \param smi130_gyro_enable_fast_offset
 *                 Enable  -> 1
 *                 Disable -> 0
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_enable_fast_offset()
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FAST_OFFSET_EN__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_FAST_OFFSET_EN, 1);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FAST_OFFSET_EN__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API read the Fast offset en status from the
 * 0x32h of 0 to 2 bits.
 *
 *
 *
 *
 *\param unsigned char *fast_offset
 *             Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return Communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fast_offset_en_ch(
unsigned char *fast_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FAST_OFFSET_EN_XYZ__REG, &v_data_u8r, 1);
		*fast_offset = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FAST_OFFSET_EN_XYZ);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API writes the Fast offset enable bit based
 * on the Channel selection 0x32h of (0 to 2 bits)
 *
 *
 *
 *
* \param  unsigned char channel,unsigned char fast_offset
 *
 *                      channel --> SMI130_GYRO_X_AXIS,SMI130_GYRO_Y_AXIS,SMI130_GYRO_Z_AXIS
 *                      fast_offset --> 0 - Disable
 *                                      1 - Enable
 *
 *
 *
 *  \return Communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fast_offset_en_ch(
unsigned char channel, unsigned char fast_offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres  = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (channel) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_X__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FAST_OFFSET_EN_X, fast_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_X__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_Y__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FAST_OFFSET_EN_Y, fast_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_Y__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_Z__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FAST_OFFSET_EN_Z, fast_offset);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FAST_OFFSET_EN_Z__REG, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of nvm program
 * remain
 *
 *
 *
 *
 *\param unsigned char *nvm_remain
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_remain(unsigned char *nvm_remain)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN__REG, &v_data_u8r, 1);
		*nvm_remain = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of nvm load
 *
 *
 *
 *
 *\param unsigned char nvm_load
 *              1 -> load offset value from NVM
 *              0 -> no action
 *
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_load(unsigned char nvm_load)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD, nvm_load);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of nvmprogram
 * ready
 *
 *
 *
 *
 *\param unsigned char *nvm_rdy
 *             1 -> program seq finished
 *             0 -> program seq in progress
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_rdy(unsigned char *nvm_rdy)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY__REG, &v_data_u8r, 1);
		*nvm_rdy = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of nvm program
 * trigger
 *
 *
 *
 *
 *\param unsigned char trig
 *            1 -> trig program seq (wo)
 *            0 -> No Action
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_prog_trig(unsigned char prog_trig)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG, prog_trig);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of nvm program
 * mode
 *
 *
 *
 *
* \param unsigned char *prog_mode : Address of *prog_mode
 *                  1 -> Enable program mode
 *                  0 -> Disable program mode
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_prog_mode(unsigned char *prog_mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__REG, &v_data_u8r, 1);
		*prog_mode = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/******************************************************************************
 * Description: *//**brief This API is used to set the status of nvmprogram
 * mode
 *
 *
 *
 *
* \param(unsigned char prog_mode)
 *                   1 -> Enable program mode
 *                   0 -> Disable program mode
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_prog_mode(unsigned char prog_mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE, prog_mode);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of i2c wdt
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char *prog_mode
 *            SMI130_GYRO_I2C_WDT_SEL               1
 *            SMI130_GYRO_I2C_WDT_EN                0
 *            *prog_mode : Address of prog_mode
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_i2c_wdt(unsigned char i2c_wdt,
unsigned char *prog_mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (i2c_wdt) {
		case SMI130_GYRO_I2C_WDT_EN:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__REG,
			&v_data_u8r, 1);
			*prog_mode = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN);
			break;
		case SMI130_GYRO_I2C_WDT_SEL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__REG,
			&v_data_u8r, 1);
			*prog_mode = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of i2c wdt
 *
 *
 *
 *
 *\param unsigned char channel,unsigned char prog_mode
 *            SMI130_GYRO_I2C_WDT_SEL               1
 *            SMI130_GYRO_I2C_WDT_EN                0
 *            prog_mode : Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_i2c_wdt(unsigned char i2c_wdt,
unsigned char prog_mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (i2c_wdt) {
		case SMI130_GYRO_I2C_WDT_EN:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN, prog_mode);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__REG,
			&v_data_u8r, 1);
			break;
		case SMI130_GYRO_I2C_WDT_SEL:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL, prog_mode);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__REG,
			&v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief  This API is used to get the status of spi3
 *
 *
 *
 *
* \param unsigned char *spi3 : Address of spi3
 *                                Pointer to a variable passed as a parameter
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_spi3(unsigned char *spi3)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__REG, &v_data_u8r, 1);
		*spi3 = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of spi3
 *
 *
 *
 *
 *\param unsigned char spi3
 *
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_spi3(unsigned char spi3)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__REG, &v_data_u8r, 1);
		v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
		SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3, spi3);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__REG, &v_data_u8r, 1);
	}
	return comres;
}
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_tag(unsigned char *tag)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_CGF1_ADDR_TAG__REG, &v_data_u8r, 1);
		*tag = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF1_ADDR_TAG);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of Tag
 *
 *
 *
 *
 *\param unsigned char tag
 *                  Enable  -> 1
 *                  Disable -> 0
 *
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_tag(unsigned char tag)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (tag < C_SMI130_GYRO_Two_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF1_ADDR_TAG__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF1_ADDR_TAG, tag);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF1_ADDR_TAG__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get Water Mark Level
 *
 *
 *
 *
 *\param unsigned char *water_mark_level : Address of water_mark_level
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_watermarklevel(
unsigned char *water_mark_level)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_CGF1_ADDR_WML__REG, &v_data_u8r, 1);
		*water_mark_level = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_FIFO_CGF1_ADDR_WML);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set Water Mark Level
 *
 *
 *
 *
 *\param unsigned char water_mark_level:
 *          Value to be written passed as a parameter

 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_watermarklevel(
unsigned char water_mark_level)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (water_mark_level < C_SMI130_GYRO_OneTwentyEight_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF1_ADDR_WML__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF1_ADDR_WML, water_mark_level);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF1_ADDR_WML__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of offset
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char *offset
 *                         axis ->
 *                   SMI130_GYRO_X_AXIS     ->      0
 *                   SMI130_GYRO_Y_AXIS     ->      1
 *                   SMI130_GYRO_Z_AXIS     ->      2
 *                   offset -> Any valid value
 *
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset(unsigned char axis,
SMI130_GYRO_S16 *offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data1_u8r = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data2_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_X_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__REG, &v_data1_u8r, 1);
			v_data1_u8r = SMI130_GYRO_GET_BITSLICE(v_data1_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_X__REG, &v_data2_u8r, 1);
			v_data2_u8r = SMI130_GYRO_GET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_X);
			v_data2_u8r = ((v_data2_u8r <<
			SMI130_GYRO_SHIFT_2_POSITION) | v_data1_u8r);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr, SMI130_GYRO_OFC2_ADDR, &v_data1_u8r, 1);
			*offset = (SMI130_GYRO_S16)((((SMI130_GYRO_S16)
				((signed char)v_data1_u8r))
			<< SMI130_GYRO_SHIFT_4_POSITION) | (v_data2_u8r));
			break;
		case SMI130_GYRO_Y_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__REG, &v_data1_u8r, 1);
			v_data1_u8r = SMI130_GYRO_GET_BITSLICE(v_data1_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Y__REG, &v_data2_u8r, 1);
			v_data2_u8r = SMI130_GYRO_GET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Y);
			v_data2_u8r = ((v_data2_u8r <<
			SMI130_GYRO_SHIFT_1_POSITION) | v_data1_u8r);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC3_ADDR, &v_data1_u8r, 1);
			*offset = (SMI130_GYRO_S16)((((SMI130_GYRO_S16)
				((signed char)v_data1_u8r))
			<< SMI130_GYRO_SHIFT_4_POSITION) | (v_data2_u8r));
			break;
		case SMI130_GYRO_Z_AXIS:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__REG, &v_data1_u8r, 1);
			v_data1_u8r = SMI130_GYRO_GET_BITSLICE(v_data1_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Z__REG, &v_data2_u8r, 1);
			v_data2_u8r = SMI130_GYRO_GET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Z);
			v_data2_u8r = ((v_data2_u8r << SMI130_GYRO_SHIFT_1_POSITION)
				| v_data1_u8r);
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC4_ADDR, &v_data1_u8r, 1);
			*offset = (SMI130_GYRO_S16)((((SMI130_GYRO_S16)
				((signed char)v_data1_u8r))
			<< SMI130_GYRO_SHIFT_4_POSITION) | (v_data2_u8r));
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of offset
 *
 *
 *
 *
 *\param unsigned char axis,unsigned char offset
 *                         axis ->
 *                   SMI130_GYRO_X_AXIS     ->      0
 *                   SMI130_GYRO_Y_AXIS     ->      1
 *                   SMI130_GYRO_Z_AXIS     ->      2
 *                   offset -> Any valid value
 *
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset(
unsigned char axis, SMI130_GYRO_S16 offset)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data1_u8r = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data2_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (axis) {
		case SMI130_GYRO_X_AXIS:
			v_data1_u8r = ((signed char) (offset & 0x0FF0))
			>> SMI130_GYRO_SHIFT_4_POSITION;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC2_ADDR, &v_data1_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x000C);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_X, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_X__REG, &v_data2_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x0003);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__REG, &v_data2_u8r, 1);
			break;
		case SMI130_GYRO_Y_AXIS:
			v_data1_u8r = ((signed char) (offset & 0x0FF0)) >>
			SMI130_GYRO_SHIFT_4_POSITION;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC3_ADDR, &v_data1_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x000E);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Y, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Y__REG, &v_data2_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x0001);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__REG, &v_data2_u8r, 1);
			break;
		case SMI130_GYRO_Z_AXIS:
			v_data1_u8r = ((signed char) (offset & 0x0FF0)) >>
			SMI130_GYRO_SHIFT_4_POSITION;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC4_ADDR, &v_data1_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x000E);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Z, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_OFC1_ADDR_OFFSET_Z__REG, &v_data2_u8r, 1);

			v_data1_u8r = (unsigned char) (offset & 0x0001);
			v_data2_u8r = SMI130_GYRO_SET_BITSLICE(v_data2_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z, v_data1_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__REG, &v_data2_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of general
 * purpose register
 *
 *
 *
 *
 *\param unsigned char param,unsigned char *value
 *             param ->
 *              SMI130_GYRO_GP0                      0
 *              SMI130_GYRO_GP0                      1
 *               *value -> Address of high_bw
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_gp(unsigned char param,
unsigned char *value)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_GP0:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_GP0__REG, &v_data_u8r, 1);
			*value = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
				SMI130_GYRO_TRIM_GP0_ADDR_GP0);
			break;
		case SMI130_GYRO_GP1:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP1_ADDR, &v_data_u8r, 1);
			*value = v_data_u8r;
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of general
 * purpose register
 *
 *
 *
 *
 *\param unsigned char param,unsigned char value
 *             param ->
 *              SMI130_GYRO_GP0                      0
 *              SMI130_GYRO_GP0                      1
 *             value -> Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_gp(unsigned char param,
unsigned char value)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		switch (param) {
		case SMI130_GYRO_GP0:
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_GP0__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_TRIM_GP0_ADDR_GP0, value);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP0_ADDR_GP0__REG, &v_data_u8r, 1);
			break;
		case SMI130_GYRO_GP1:
			v_data_u8r = value;
			comres = p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_TRIM_GP1_ADDR, &v_data_u8r, 1);
			break;
		default:
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
			break;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads FIFI data from location 3Fh
 *
 *
 *
 *
 *  \param
 *      unsigned char *fifo_data : Address of FIFO data bits
 *
 *
 *
 *
 *  \return result of communication routines
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_FIFO_data_reg(unsigned char *fifo_data)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_DATA_ADDR, &v_data_u8r, 1);
		*fifo_data = v_data_u8r;
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt fifo status register byte from 0Eh
 *
 *
 *
 *
 *  \param
 *      unsigned char *fifo_status : Address of Fifo status register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifostatus_reg(
unsigned char *fifo_status)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_STATUS_ADDR, fifo_status, 1);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt fifo status register byte from 0Eh
 *
 *
 *
 *
 *  \param
 *      unsigned char *fifo_framecount: Address of FIFO status register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_framecount(
unsigned char *fifo_framecount)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r  = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER__REG, &v_data_u8r, 1);
		*fifo_framecount = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief Reads interrupt fifo status register byte from 0Eh
 *
 *
 *
 *
 *  \param
 *      unsigned char *fifo_overrun: Address of FIFO status register
 *
 *
 *  \return
 *      Result of bus communication function
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_overrun(
unsigned char *fifo_overrun)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_STATUS_OVERRUN__REG, &v_data_u8r, 1);
		*fifo_overrun = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_FIFO_STATUS_OVERRUN);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of fifo mode
 *
 *
 *
 *
 *\param unsigned char *mode : Address of mode
 *                         fifo_mode  0 --> Bypass
 *                         1 --> FIFO
 *                         2 --> Stream
 *                         3 --> Reserved
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_mode(unsigned char *mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_CGF0_ADDR_MODE__REG, &v_data_u8r, 1);
		*mode = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF0_ADDR_MODE);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used set to FIFO mode
 *
 *
 *
 *
* \param             0 --> BYPASS
 *                      1 --> FIFO
 *                      2 --> STREAM
 *
 *
 *  \return Communication Results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_mode(unsigned char mode)
{
	int comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (mode < C_SMI130_GYRO_Four_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF0_ADDR_MODE__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF0_ADDR_MODE, mode);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF0_ADDR_MODE__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the status of fifo data
 * sel
 *
 *
 *
 *
 *\param unsigned char *data_sel : Address of data_sel
 *         data_sel --> [0:3]
 *         0 --> X,Y and Z (DEFAULT)
 *         1 --> X only
 *         2 --> Y only
 *         3 --> Z only
 *
 *
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_data_sel(unsigned char *data_sel)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__REG, &v_data_u8r, 1);
		*data_sel = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the status of fifo data
 * sel
 *
 *
 *
 *
 *\param unsigned char data_sel
 *         data_sel --> [0:3]
 *         0 --> X,Y and Z (DEFAULT)
 *         1 --> X only
 *         2 --> Y only
 *         3 --> Z only
 *
 *
 *
 *  \return communication results
 *
 *
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_data_sel(unsigned char data_sel)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (data_sel < C_SMI130_GYRO_Four_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__REG, &v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL, data_sel);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__REG, &v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to get the operating modes of the
 * sensor
 *
 *
 *
 *
 *\param unsigned char * mode : Address of mode
 *                       0 -> NORMAL
 *                       1 -> SUSPEND
 *                       2 -> DEEP SUSPEND
 *						 3 -> FAST POWERUP
 *						 4 -> ADVANCED POWERSAVING
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_mode(unsigned char *mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char data1 = C_SMI130_GYRO_Zero_U8X;
	unsigned char data2 = C_SMI130_GYRO_Zero_U8X;
	unsigned char data3 = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == C_SMI130_GYRO_Zero_U8X) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
		comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		SMI130_GYRO_MODE_LPM2_ADDR, &data2, C_SMI130_GYRO_One_U8X);
		data1  = (data1 & 0xA0) >> 5;
		data3  = (data2 & 0x40) >> 6;
		data2  = (data2 & 0x80) >> 7;
		if (data3 == 0x01) {
			*mode  = SMI130_GYRO_MODE_ADVANCEDPOWERSAVING;
		} else {
			if ((data1 == 0x00) && (data2 == 0x00)) {
				*mode  = SMI130_GYRO_MODE_NORMAL;
				} else {
				if ((data1 == 0x01) || (data1 == 0x05)) {
					*mode  = SMI130_GYRO_MODE_DEEPSUSPEND;
					} else {
					if ((data1 == 0x04) &&
					(data2 == 0x00)) {
						*mode  = SMI130_GYRO_MODE_SUSPEND;
					} else {
					if ((data1 == 0x04) &&
						(data2 == 0x01))
							*mode  =
							SMI130_GYRO_MODE_FASTPOWERUP;
						}
					}
				}
			}
		}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set the operating Modes of the
 * sensor
 *
 *
 *
 *
 *\param unsigned char Mode
 *                       0 -> NORMAL
 *                       1 -> DEEPSUSPEND
 *                       2 -> SUSPEND
 *						 3 -> Fast Powerup
 *						 4 -> Advance Powerup
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_mode(unsigned char mode)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char data1 = C_SMI130_GYRO_Zero_U8X;
	unsigned char data2 = C_SMI130_GYRO_Zero_U8X;
	unsigned char data3 = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_autosleepduration = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_bw_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == C_SMI130_GYRO_Zero_U8X) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (mode < C_SMI130_GYRO_Five_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data2, C_SMI130_GYRO_One_U8X);
			switch (mode) {
			case SMI130_GYRO_MODE_NORMAL:
				data1  = SMI130_GYRO_SET_BITSLICE(data1,
				SMI130_GYRO_MODE_LPM1, C_SMI130_GYRO_Zero_U8X);
				data2  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP,
				C_SMI130_GYRO_Zero_U8X);
				data3  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING,
				C_SMI130_GYRO_Zero_U8X);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			p_smi130_gyro->delay_msec(1);/*A minimum delay of atleast
			450us is required for Multiple write.*/
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data3, C_SMI130_GYRO_One_U8X);
				break;
			case SMI130_GYRO_MODE_DEEPSUSPEND:
				data1  = SMI130_GYRO_SET_BITSLICE(data1,
				SMI130_GYRO_MODE_LPM1, C_SMI130_GYRO_One_U8X);
				data2  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP,
				C_SMI130_GYRO_Zero_U8X);
				data3  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING,
				C_SMI130_GYRO_Zero_U8X);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			p_smi130_gyro->delay_msec(1);/*A minimum delay of atleast
			450us is required for Multiple write.*/
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data3, C_SMI130_GYRO_One_U8X);
				break;
			case SMI130_GYRO_MODE_SUSPEND:
				data1  = SMI130_GYRO_SET_BITSLICE(data1,
				SMI130_GYRO_MODE_LPM1, C_SMI130_GYRO_Four_U8X);
				data2  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP,
				C_SMI130_GYRO_Zero_U8X);
				data3  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING,
				C_SMI130_GYRO_Zero_U8X);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			p_smi130_gyro->delay_msec(1);/*A minimum delay of atleast
			450us is required for Multiple write.*/
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data3, C_SMI130_GYRO_One_U8X);
				break;
			case SMI130_GYRO_MODE_FASTPOWERUP:
				data1  = SMI130_GYRO_SET_BITSLICE(data1,
				SMI130_GYRO_MODE_LPM1, C_SMI130_GYRO_Four_U8X);
				data2  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP,
				C_SMI130_GYRO_One_U8X);
				data3  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING,
				C_SMI130_GYRO_Zero_U8X);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			p_smi130_gyro->delay_msec(1);/*A minimum delay of atleast
			450us is required for Multiple write.*/
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data3, C_SMI130_GYRO_One_U8X);
				break;
			case SMI130_GYRO_MODE_ADVANCEDPOWERSAVING:
				/* Configuring the proper settings for auto
				sleep duration */
				smi130_gyro_get_bw(&v_bw_u8r);
				smi130_gyro_get_autosleepdur(&v_autosleepduration);
				smi130_gyro_set_autosleepdur(v_autosleepduration,
				v_bw_u8r);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
					(p_smi130_gyro->dev_addr,
				SMI130_GYRO_MODE_LPM2_ADDR, &data2,
				C_SMI130_GYRO_One_U8X);
				/* Configuring the advanced power saving mode*/
				data1  = SMI130_GYRO_SET_BITSLICE(data1,
				SMI130_GYRO_MODE_LPM1, C_SMI130_GYRO_Zero_U8X);
				data2  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP,
				C_SMI130_GYRO_Zero_U8X);
				data3  = SMI130_GYRO_SET_BITSLICE(data2,
				SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING,
				C_SMI130_GYRO_One_U8X);
				comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM1_ADDR, &data1, C_SMI130_GYRO_One_U8X);
			p_smi130_gyro->delay_msec(1);/*A minimum delay of atleast
			450us is required for Multiple write.*/
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR, &data3, C_SMI130_GYRO_One_U8X);
				break;
				}
		} else {
		comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to to do selftest to sensor
 * sensor
 *
 *
 *
 *
 *\param unsigned char *result
 *
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_selftest(unsigned char *result)
	{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char data1 = C_SMI130_GYRO_Zero_U8X;
	unsigned char data2 = C_SMI130_GYRO_Zero_U8X;

	comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
	SMI130_GYRO_SELF_TEST_ADDR, &data1, C_SMI130_GYRO_One_U8X);
	data2  = SMI130_GYRO_GET_BITSLICE(data1, SMI130_GYRO_SELF_TEST_ADDR_RATEOK);
	data1  = SMI130_GYRO_SET_BITSLICE(data1, SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST,
	C_SMI130_GYRO_One_U8X);
	comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC(p_smi130_gyro->dev_addr,
	SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST__REG, &data1, C_SMI130_GYRO_One_U8X);

	/* Waiting time to complete the selftest process */
	p_smi130_gyro->delay_msec(10);

	/* Reading Selftest result bir bist_failure */
	comres += p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
	SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL__REG, &data1, C_SMI130_GYRO_One_U8X);
	data1  = SMI130_GYRO_GET_BITSLICE(data1, SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL);
	if ((data1 == 0x00) && (data2 == 0x01))
		*result = C_SMI130_GYRO_SUCCESS;
	else
		*result = C_SMI130_GYRO_FAILURE;
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief  This API is used to get data auto sleep duration
 *
 *
 *
 *
 *\param unsigned char *duration : Address of auto sleep duration
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_autosleepdur(unsigned char *duration)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__REG, &v_data_u8r, 1);
		*duration = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set duration
 *
 *
 *
 *
 *\param unsigned char duration:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_autosleepdur(unsigned char duration,
unsigned char bandwith)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_autosleepduration_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
			(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__REG,
			&v_data_u8r, 1);
			if (duration < C_SMI130_GYRO_Eight_U8X) {
				switch (bandwith) {
				case C_SMI130_GYRO_No_Filter_U8X:
					if (duration >
					C_SMI130_GYRO_4ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_4ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_230Hz_U8X:
					if (duration >
					C_SMI130_GYRO_4ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_4ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_116Hz_U8X:
					if (duration >
					C_SMI130_GYRO_4ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_4ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_47Hz_U8X:
					if (duration >
					C_SMI130_GYRO_5ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_5ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_23Hz_U8X:
					if (duration >
					C_SMI130_GYRO_10ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_10ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_12Hz_U8X:
					if (duration >
					C_SMI130_GYRO_20ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
					v_autosleepduration_u8r =
					C_SMI130_GYRO_20ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_64Hz_U8X:
					if (duration >
					C_SMI130_GYRO_10ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_10ms_AutoSleepDur_U8X;
					break;
				case C_SMI130_GYRO_BW_32Hz_U8X:
					if (duration >
					C_SMI130_GYRO_20ms_AutoSleepDur_U8X)
						v_autosleepduration_u8r =
						duration;
					else
						v_autosleepduration_u8r =
						C_SMI130_GYRO_20ms_AutoSleepDur_U8X;
					break;
				default:
				if (duration >
					C_SMI130_GYRO_4ms_AutoSleepDur_U8X)
					v_autosleepduration_u8r =
						duration;
					else
					v_autosleepduration_u8r =
					C_SMI130_GYRO_4ms_AutoSleepDur_U8X;
					break;
				}
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR,
			v_autosleepduration_u8r);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__REG,
			&v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief  This API is used to get data sleep duration
 *
 *
 *
 *
 *\param unsigned char *duration : Address of sleep duration
 *                         Pointer to a variable passed as a parameter
 *
 *
 *
 *  \return
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_sleepdur(unsigned char *duration)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC(p_smi130_gyro->dev_addr,
		 SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__REG, &v_data_u8r, 1);
		*duration = SMI130_GYRO_GET_BITSLICE(v_data_u8r,
		SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR);
	}
	return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*****************************************************************************
 * Description: *//**brief This API is used to set duration
 *
 *
 *
 *
 *\param unsigned char duration:
 *          Value to be written passed as a parameter
 *
 *
 *
 *  \return communication results
 *
 *
 *****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_sleepdur(unsigned char duration)
{
	SMI130_GYRO_RETURN_FUNCTION_TYPE comres = C_SMI130_GYRO_Zero_U8X;
	unsigned char v_data_u8r = C_SMI130_GYRO_Zero_U8X;
	if (p_smi130_gyro == SMI130_GYRO_NULL) {
		return  E_SMI130_GYRO_NULL_PTR;
	} else {
		if (duration < C_SMI130_GYRO_Eight_U8X) {
			comres = p_smi130_gyro->SMI130_GYRO_BUS_READ_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__REG,
			&v_data_u8r, 1);
			v_data_u8r = SMI130_GYRO_SET_BITSLICE(v_data_u8r,
			SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR, duration);
			comres += p_smi130_gyro->SMI130_GYRO_BUS_WRITE_FUNC
				(p_smi130_gyro->dev_addr,
			SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__REG,
			&v_data_u8r, 1);
		} else {
			comres = E_SMI130_GYRO_OUT_OF_RANGE;
		}
	}
	return comres;
}

