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
 *
 * @filename smi130_gyro.h
 * @date    2013/11/25
 * @Modification Date 2018/08/28 18:20
 * @id       "8fcde22"
 * @version  1.5
 *
 * @brief    Header of SMI130_GYRO API
*/

/* user defined code to be added here ... */
#ifndef __SMI130_GYRO_H__
#define __SMI130_GYRO_H__

#ifdef __KERNEL__
#define SMI130_GYRO_U16 unsigned short       /* 16 bit achieved with short */
#define SMI130_GYRO_S16 signed short
#define SMI130_GYRO_S32 signed int           /* 32 bit achieved with int   */
#else
#include <limits.h> /*needed to test integer limits */


/* find correct data type for signed/unsigned 16 bit variables \
by checking max of unsigned variant */
#if USHRT_MAX == 0xFFFF
		/* 16 bit achieved with short */
		#define SMI130_GYRO_U16 unsigned short
		#define SMI130_GYRO_S16 signed short
#elif UINT_MAX == 0xFFFF
		/* 16 bit achieved with int */
		#define SMI130_GYRO_U16 unsigned int
		#define SMI130_GYRO_S16 signed int
#else
		#error SMI130_GYRO_U16 and SMI130_GYRO_S16 could not be
		#error defined automatically, please do so manually
#endif

/* find correct data type for signed 32 bit variables */
#if INT_MAX == 0x7FFFFFFF
		/* 32 bit achieved with int */
		#define SMI130_GYRO_S32 signed int
#elif LONG_MAX == 0x7FFFFFFF
		/* 32 bit achieved with long int */
		#define SMI130_GYRO_S32 signed long int
#else
		#error SMI130_GYRO_S32 could not be
		#error defined automatically, please do so manually
#endif
#endif

/**\brief defines the calling parameter types of the SMI130_GYRO_WR_FUNCTION */
#define SMI130_GYRO_BUS_WR_RETURN_TYPE char

/**\brief links the order of parameters defined in
SMI130_GYRO_BUS_WR_PARAM_TYPE to function calls used inside the API*/
#define SMI130_GYRO_BUS_WR_PARAM_TYPES unsigned char, unsigned char,\
unsigned char *, unsigned char

/**\brief links the order of parameters defined in
SMI130_GYRO_BUS_WR_PARAM_TYPE to function calls used inside the API*/
#define SMI130_GYRO_BUS_WR_PARAM_ORDER(device_addr, register_addr,\
register_data, wr_len)

/* never change this line */
#define SMI130_GYRO_BUS_WRITE_FUNC(device_addr, register_addr,\
register_data, wr_len) bus_write(device_addr, register_addr,\
register_data, wr_len)
/**\brief defines the return parameter type of the SMI130_GYRO_RD_FUNCTION
*/
#define SMI130_GYRO_BUS_RD_RETURN_TYPE char
/**\brief defines the calling parameter types of the SMI130_GYRO_RD_FUNCTION
*/
#define SMI130_GYRO_BUS_RD_PARAM_TYPES unsigned char, unsigned char,\
unsigned char *, unsigned char
/**\brief links the order of parameters defined in \
SMI130_GYRO_BUS_RD_PARAM_TYPE to function calls used inside the API
*/
#define SMI130_GYRO_BUS_RD_PARAM_ORDER (device_addr, register_addr,\
register_data)
/* never change this line */
#define SMI130_GYRO_BUS_READ_FUNC(device_addr, register_addr,\
register_data, rd_len)bus_read(device_addr, register_addr,\
register_data, rd_len)
/**\brief defines the return parameter type of the SMI130_GYRO_RD_FUNCTION
*/
#define SMI130_GYRO_BURST_RD_RETURN_TYPE char
/**\brief defines the calling parameter types of the SMI130_GYRO_RD_FUNCTION
*/
#define SMI130_GYRO_BURST_RD_PARAM_TYPES unsigned char,\
unsigned char, unsigned char *, signed int
/**\brief links the order of parameters defined in \
SMI130_GYRO_BURST_RD_PARAM_TYPE to function calls used inside the API
*/
#define SMI130_GYRO_BURST_RD_PARAM_ORDER (device_addr, register_addr,\
register_data)
/* never change this line */
#define SMI130_GYRO_BURST_READ_FUNC(device_addr, register_addr,\
register_data, rd_len)burst_read(device_addr, \
register_addr, register_data, rd_len)
/**\brief defines the return parameter type of the SMI130_GYRO_DELAY_FUNCTION
*/
#define SMI130_GYRO_DELAY_RETURN_TYPE void
/* never change this line */
#define SMI130_GYRO_DELAY_FUNC(delay_in_msec)\
		delay_func(delay_in_msec)
#define SMI130_GYRO_RETURN_FUNCTION_TYPE			int
/**< This refers SMI130_GYRO return type as char */

#define	SMI130_GYRO_I2C_ADDR1				0x68
#define	SMI130_GYRO_I2C_ADDR					SMI130_GYRO_I2C_ADDR1
#define	SMI130_GYRO_I2C_ADDR2				0x69



/*Define of registers*/

/* Hard Wired */
#define SMI130_GYRO_CHIP_ID_ADDR						0x00
/**<Address of Chip ID Register*/


/* Data Register */
#define SMI130_GYRO_RATE_X_LSB_ADDR                   0x02
/**<        Address of X axis Rate LSB Register       */
#define SMI130_GYRO_RATE_X_MSB_ADDR                   0x03
/**<        Address of X axis Rate MSB Register       */
#define SMI130_GYRO_RATE_Y_LSB_ADDR                   0x04
/**<        Address of Y axis Rate LSB Register       */
#define SMI130_GYRO_RATE_Y_MSB_ADDR                   0x05
/**<        Address of Y axis Rate MSB Register       */
#define SMI130_GYRO_RATE_Z_LSB_ADDR                   0x06
/**<        Address of Z axis Rate LSB Register       */
#define SMI130_GYRO_RATE_Z_MSB_ADDR                   0x07
/**<        Address of Z axis Rate MSB Register       */
#define SMI130_GYRO_TEMP_ADDR                        0x08
/**<        Address of Temperature Data LSB Register  */

/* Status Register */
#define SMI130_GYRO_INT_STATUS0_ADDR                 0x09
/**<        Address of Interrupt status Register 0    */
#define SMI130_GYRO_INT_STATUS1_ADDR                 0x0A
/**<        Address of Interrupt status Register 1    */
#define SMI130_GYRO_INT_STATUS2_ADDR                 0x0B
/**<        Address of Interrupt status Register 2    */
#define SMI130_GYRO_INT_STATUS3_ADDR                 0x0C
/**<        Address of Interrupt status Register 3    */
#define SMI130_GYRO_FIFO_STATUS_ADDR                 0x0E
/**<        Address of FIFO status Register           */

/* Control Register */
#define SMI130_GYRO_RANGE_ADDR                  0x0F
/**<        Address of Range address Register     */
#define SMI130_GYRO_BW_ADDR                     0x10
/**<        Address of Bandwidth Register         */
#define SMI130_GYRO_MODE_LPM1_ADDR              0x11
/**<        Address of Mode LPM1 Register         */
#define SMI130_GYRO_MODE_LPM2_ADDR              0x12
/**<        Address of Mode LPM2 Register         */
#define SMI130_GYRO_RATED_HBW_ADDR              0x13
/**<        Address of Rate HBW Register          */
#define SMI130_GYRO_BGW_SOFTRESET_ADDR          0x14
/**<        Address of BGW Softreset Register      */
#define SMI130_GYRO_INT_ENABLE0_ADDR            0x15
/**<        Address of Interrupt Enable 0             */
#define SMI130_GYRO_INT_ENABLE1_ADDR            0x16
/**<        Address of Interrupt Enable 1             */
#define SMI130_GYRO_INT_MAP_0_ADDR              0x17
/**<        Address of Interrupt MAP 0                */
#define SMI130_GYRO_INT_MAP_1_ADDR              0x18
/**<        Address of Interrupt MAP 1                */
#define SMI130_GYRO_INT_MAP_2_ADDR              0x19
/**<        Address of Interrupt MAP 2                */
#define SMI130_GYRO_INT_0_ADDR                  0x1A
/**<        Address of Interrupt 0 register   */
#define SMI130_GYRO_INT_1_ADDR                  0x1B
/**<        Address of Interrupt 1 register   */
#define SMI130_GYRO_INT_2_ADDR                  0x1C
/**<        Address of Interrupt 2 register   */
#define SMI130_GYRO_INT_4_ADDR                  0x1E
/**<        Address of Interrupt 4 register   */
#define SMI130_GYRO_RST_LATCH_ADDR              0x21
/**<        Address of Reset Latch Register           */
#define SMI130_GYRO_HIGH_TH_X_ADDR              0x22
/**<        Address of High Th x Address register     */
#define SMI130_GYRO_HIGH_DUR_X_ADDR             0x23
/**<        Address of High Dur x Address register    */
#define SMI130_GYRO_HIGH_TH_Y_ADDR              0x24
/**<        Address of High Th y  Address register    */
#define SMI130_GYRO_HIGH_DUR_Y_ADDR             0x25
/**<        Address of High Dur y Address register    */
#define SMI130_GYRO_HIGH_TH_Z_ADDR              0x26
/**<        Address of High Th z Address register  */
#define SMI130_GYRO_HIGH_DUR_Z_ADDR             0x27
/**<        Address of High Dur z Address register  */
#define SMI130_GYRO_SOC_ADDR                        0x31
/**<        Address of SOC register        */
#define SMI130_GYRO_A_FOC_ADDR                      0x32
/**<        Address of A_FOC Register        */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR          0x33
/**<        Address of Trim NVM control register      */
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR           0x34
/**<        Address of BGW SPI3,WDT Register           */


/* Trim Register */
#define SMI130_GYRO_OFC1_ADDR                   0x36
/**<        Address of OFC1 Register          */
#define SMI130_GYRO_OFC2_ADDR                       0x37
/**<        Address of OFC2 Register          */
#define SMI130_GYRO_OFC3_ADDR                   0x38
/**<        Address of OFC3 Register          */
#define SMI130_GYRO_OFC4_ADDR                   0x39
/**<        Address of OFC4 Register          */
#define SMI130_GYRO_TRIM_GP0_ADDR               0x3A
/**<        Address of Trim GP0 Register              */
#define SMI130_GYRO_TRIM_GP1_ADDR               0x3B
/**<        Address of Trim GP1 Register              */
#define SMI130_GYRO_SELF_TEST_ADDR              0x3C
/**<        Address of BGW Self test Register           */

/* Control Register */
#define SMI130_GYRO_FIFO_CGF1_ADDR              0x3D
/**<        Address of FIFO CGF0 Register             */
#define SMI130_GYRO_FIFO_CGF0_ADDR              0x3E
/**<        Address of FIFO CGF1 Register             */

/* Data Register */
#define SMI130_GYRO_FIFO_DATA_ADDR              0x3F
/**<        Address of FIFO Data Register             */

/* Rate X LSB Register */
#define SMI130_GYRO_RATE_X_LSB_VALUEX__POS        0

/**< Last 8 bits of RateX LSB Registers */
#define SMI130_GYRO_RATE_X_LSB_VALUEX__LEN        8
#define SMI130_GYRO_RATE_X_LSB_VALUEX__MSK        0xFF
#define SMI130_GYRO_RATE_X_LSB_VALUEX__REG        SMI130_GYRO_RATE_X_LSB_ADDR

/* Rate Y LSB Register */
/**<  Last 8 bits of RateY LSB Registers */
#define SMI130_GYRO_RATE_Y_LSB_VALUEY__POS        0
#define SMI130_GYRO_RATE_Y_LSB_VALUEY__LEN        8
#define SMI130_GYRO_RATE_Y_LSB_VALUEY__MSK        0xFF
#define SMI130_GYRO_RATE_Y_LSB_VALUEY__REG        SMI130_GYRO_RATE_Y_LSB_ADDR

/* Rate Z LSB Register */
/**< Last 8 bits of RateZ LSB Registers */
#define SMI130_GYRO_RATE_Z_LSB_VALUEZ__POS        0
#define SMI130_GYRO_RATE_Z_LSB_VALUEZ__LEN        8
#define SMI130_GYRO_RATE_Z_LSB_VALUEZ__MSK        0xFF
#define SMI130_GYRO_RATE_Z_LSB_VALUEZ__REG        SMI130_GYRO_RATE_Z_LSB_ADDR

/* Interrupt status 0 Register */
   /**< 2th bit of Interrupt status 0 register */
#define SMI130_GYRO_INT_STATUS0_ANY_INT__POS     2
#define SMI130_GYRO_INT_STATUS0_ANY_INT__LEN     1
#define SMI130_GYRO_INT_STATUS0_ANY_INT__MSK     0x04
#define SMI130_GYRO_INT_STATUS0_ANY_INT__REG     SMI130_GYRO_INT_STATUS0_ADDR

/**< 1st bit of Interrupt status 0 register */
#define SMI130_GYRO_INT_STATUS0_HIGH_INT__POS    1
#define SMI130_GYRO_INT_STATUS0_HIGH_INT__LEN    1
#define SMI130_GYRO_INT_STATUS0_HIGH_INT__MSK    0x02
#define SMI130_GYRO_INT_STATUS0_HIGH_INT__REG    SMI130_GYRO_INT_STATUS0_ADDR

 /**< 1st and 2nd bit of Interrupt status 0 register */
#define SMI130_GYRO_INT_STATUSZERO__POS    1
#define SMI130_GYRO_INT_STATUSZERO__LEN    2
#define SMI130_GYRO_INT_STATUSZERO__MSK    0x06
#define SMI130_GYRO_INT_STATUSZERO__REG    SMI130_GYRO_INT_STATUS0_ADDR

/* Interrupt status 1 Register */
/**< 7th bit of Interrupt status 1 register */
#define SMI130_GYRO_INT_STATUS1_DATA_INT__POS           7
#define SMI130_GYRO_INT_STATUS1_DATA_INT__LEN           1
#define SMI130_GYRO_INT_STATUS1_DATA_INT__MSK           0x80
#define SMI130_GYRO_INT_STATUS1_DATA_INT__REG           SMI130_GYRO_INT_STATUS1_ADDR

 /**< 6th bit of Interrupt status 1 register */
#define SMI130_GYRO_INT_STATUS1_AUTO_OFFSET_INT__POS    6
#define SMI130_GYRO_INT_STATUS1_AUTO_OFFSET_INT__LEN    1
#define SMI130_GYRO_INT_STATUS1_AUTO_OFFSET_INT__MSK    0x40
#define SMI130_GYRO_INT_STATUS1_AUTO_OFFSET_INT__REG    SMI130_GYRO_INT_STATUS1_ADDR

/**< 5th bit of Interrupt status 1 register */
#define SMI130_GYRO_INT_STATUS1_FAST_OFFSET_INT__POS    5
#define SMI130_GYRO_INT_STATUS1_FAST_OFFSET_INT__LEN    1
#define SMI130_GYRO_INT_STATUS1_FAST_OFFSET_INT__MSK    0x20
#define SMI130_GYRO_INT_STATUS1_FAST_OFFSET_INT__REG    SMI130_GYRO_INT_STATUS1_ADDR

/**< 4th bit of Interrupt status 1 register */
#define SMI130_GYRO_INT_STATUS1_FIFO_INT__POS           4
#define SMI130_GYRO_INT_STATUS1_FIFO_INT__LEN           1
#define SMI130_GYRO_INT_STATUS1_FIFO_INT__MSK           0x10
#define SMI130_GYRO_INT_STATUS1_FIFO_INT__REG           SMI130_GYRO_INT_STATUS1_ADDR

/**< MSB 4 bits of Interrupt status1 register */
#define SMI130_GYRO_INT_STATUSONE__POS           4
#define SMI130_GYRO_INT_STATUSONE__LEN           4
#define SMI130_GYRO_INT_STATUSONE__MSK           0xF0
#define SMI130_GYRO_INT_STATUSONE__REG           SMI130_GYRO_INT_STATUS1_ADDR

/* Interrupt status 2 Register */
/**< 3th bit of Interrupt status 2 register */
#define SMI130_GYRO_INT_STATUS2_ANY_SIGN_INT__POS     3
#define SMI130_GYRO_INT_STATUS2_ANY_SIGN_INT__LEN     1
#define SMI130_GYRO_INT_STATUS2_ANY_SIGN_INT__MSK     0x08
#define SMI130_GYRO_INT_STATUS2_ANY_SIGN_INT__REG     SMI130_GYRO_INT_STATUS2_ADDR

/**< 2th bit of Interrupt status 2 register */
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTZ_INT__POS   2
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTZ_INT__LEN   1
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTZ_INT__MSK   0x04
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTZ_INT__REG   SMI130_GYRO_INT_STATUS2_ADDR

/**< 1st bit of Interrupt status 2 register */
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTY_INT__POS   1
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTY_INT__LEN   1
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTY_INT__MSK   0x02
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTY_INT__REG   SMI130_GYRO_INT_STATUS2_ADDR

/**< 0th bit of Interrupt status 2 register */
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTX_INT__POS   0
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTX_INT__LEN   1
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTX_INT__MSK   0x01
#define SMI130_GYRO_INT_STATUS2_ANY_FIRSTX_INT__REG   SMI130_GYRO_INT_STATUS2_ADDR

/**< 4 bits of Interrupt status 2 register */
#define SMI130_GYRO_INT_STATUSTWO__POS   0
#define SMI130_GYRO_INT_STATUSTWO__LEN   4
#define SMI130_GYRO_INT_STATUSTWO__MSK   0x0F
#define SMI130_GYRO_INT_STATUSTWO__REG   SMI130_GYRO_INT_STATUS2_ADDR

/* Interrupt status 3 Register */
/**< 3th bit of Interrupt status 3 register */
#define SMI130_GYRO_INT_STATUS3_HIGH_SIGN_INT__POS     3
#define SMI130_GYRO_INT_STATUS3_HIGH_SIGN_INT__LEN     1
#define SMI130_GYRO_INT_STATUS3_HIGH_SIGN_INT__MSK     0x08
#define SMI130_GYRO_INT_STATUS3_HIGH_SIGN_INT__REG     SMI130_GYRO_INT_STATUS3_ADDR

/**< 2th bit of Interrupt status 3 register */
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTZ_INT__POS   2
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTZ_INT__LEN   1
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTZ_INT__MSK   0x04
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTZ_INT__REG  SMI130_GYRO_INT_STATUS3_ADDR

/**< 1st bit of Interrupt status 3 register */
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTY_INT__POS   1
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTY_INT__LEN   1
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTY_INT__MSK   0x02
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTY_INT__REG   SMI130_GYRO_INT_STATUS3_ADDR

/**< 0th bit of Interrupt status 3 register */
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTX_INT__POS   0
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTX_INT__LEN   1
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTX_INT__MSK   0x01
#define SMI130_GYRO_INT_STATUS3_HIGH_FIRSTX_INT__REG   SMI130_GYRO_INT_STATUS3_ADDR

/**< LSB 4 bits of Interrupt status 3 register */
#define SMI130_GYRO_INT_STATUSTHREE__POS   0
#define SMI130_GYRO_INT_STATUSTHREE__LEN   4
#define SMI130_GYRO_INT_STATUSTHREE__MSK   0x0F
#define SMI130_GYRO_INT_STATUSTHREE__REG   SMI130_GYRO_INT_STATUS3_ADDR

/* SMI130_GYRO FIFO Status Register */
/**< 7th bit of FIFO status Register */
#define SMI130_GYRO_FIFO_STATUS_OVERRUN__POS         7
#define SMI130_GYRO_FIFO_STATUS_OVERRUN__LEN         1
#define SMI130_GYRO_FIFO_STATUS_OVERRUN__MSK         0x80
#define SMI130_GYRO_FIFO_STATUS_OVERRUN__REG         SMI130_GYRO_FIFO_STATUS_ADDR

/**< First 7 bits of FIFO status Register */
#define SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER__POS   0
#define SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER__LEN   7
#define SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER__MSK   0x7F
#define SMI130_GYRO_FIFO_STATUS_FRAME_COUNTER__REG   SMI130_GYRO_FIFO_STATUS_ADDR

/**< First 3 bits of range Registers */
#define SMI130_GYRO_RANGE_ADDR_RANGE__POS           0
#define SMI130_GYRO_RANGE_ADDR_RANGE__LEN           3
#define SMI130_GYRO_RANGE_ADDR_RANGE__MSK           0x07
#define SMI130_GYRO_RANGE_ADDR_RANGE__REG           SMI130_GYRO_RANGE_ADDR

/**< Last bit of Bandwidth Registers */
#define SMI130_GYRO_BW_ADDR_HIGH_RES__POS       7
#define SMI130_GYRO_BW_ADDR_HIGH_RES__LEN       1
#define SMI130_GYRO_BW_ADDR_HIGH_RES__MSK       0x80
#define SMI130_GYRO_BW_ADDR_HIGH_RES__REG       SMI130_GYRO_BW_ADDR

/**< First 3 bits of Bandwidth Registers */
#define SMI130_GYRO_BW_ADDR__POS             0
#define SMI130_GYRO_BW_ADDR__LEN             3
#define SMI130_GYRO_BW_ADDR__MSK             0x07
#define SMI130_GYRO_BW_ADDR__REG             SMI130_GYRO_BW_ADDR

/**< 6th bit of Bandwidth Registers */
#define SMI130_GYRO_BW_ADDR_IMG_STB__POS             6
#define SMI130_GYRO_BW_ADDR_IMG_STB__LEN             1
#define SMI130_GYRO_BW_ADDR_IMG_STB__MSK             0x40
#define SMI130_GYRO_BW_ADDR_IMG_STB__REG             SMI130_GYRO_BW_ADDR

/**< 5th and 7th bit of LPM1 Register */
#define SMI130_GYRO_MODE_LPM1__POS             5
#define SMI130_GYRO_MODE_LPM1__LEN             3
#define SMI130_GYRO_MODE_LPM1__MSK             0xA0
#define SMI130_GYRO_MODE_LPM1__REG             SMI130_GYRO_MODE_LPM1_ADDR

/**< 1st to 3rd bit of LPM1 Register */
#define SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__POS              1
#define SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__LEN              3
#define SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__MSK              0x0E
#define SMI130_GYRO_MODELPM1_ADDR_SLEEPDUR__REG              SMI130_GYRO_MODE_LPM1_ADDR

/**< 7th bit of Mode LPM2 Register */
#define SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP__POS         7
#define SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP__LEN         1
#define SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP__MSK         0x80
#define SMI130_GYRO_MODE_LPM2_ADDR_FAST_POWERUP__REG         SMI130_GYRO_MODE_LPM2_ADDR

/**< 6th bit of Mode LPM2 Register */
#define SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING__POS      6
#define SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING__LEN      1
#define SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING__MSK      0x40
#define SMI130_GYRO_MODE_LPM2_ADDR_ADV_POWERSAVING__REG      SMI130_GYRO_MODE_LPM2_ADDR

/**< 4th & 5th bit of Mode LPM2 Register */
#define SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__POS          4
#define SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__LEN          2
#define SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__MSK          0x30
#define SMI130_GYRO_MODE_LPM2_ADDR_EXT_TRI_SEL__REG          SMI130_GYRO_MODE_LPM2_ADDR

/**< 0th to 2nd bit of LPM2 Register */
#define SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__POS  0
#define SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__LEN  3
#define SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__MSK  0x07
#define SMI130_GYRO_MODE_LPM2_ADDR_AUTOSLEEPDUR__REG  SMI130_GYRO_MODE_LPM2_ADDR

/**< 7th bit of HBW Register */
#define SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__POS         7
#define SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__LEN         1
#define SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__MSK         0x80
#define SMI130_GYRO_RATED_HBW_ADDR_DATA_HIGHBW__REG         SMI130_GYRO_RATED_HBW_ADDR

/**< 6th bit of HBW Register */
#define SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__POS          6
#define SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__LEN          1
#define SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__MSK          0x40
#define SMI130_GYRO_RATED_HBW_ADDR_SHADOW_DIS__REG          SMI130_GYRO_RATED_HBW_ADDR

/**< 7th bit of Interrupt Enable 0 Registers */
#define SMI130_GYRO_INT_ENABLE0_DATAEN__POS               7
#define SMI130_GYRO_INT_ENABLE0_DATAEN__LEN               1
#define SMI130_GYRO_INT_ENABLE0_DATAEN__MSK               0x80
#define SMI130_GYRO_INT_ENABLE0_DATAEN__REG               SMI130_GYRO_INT_ENABLE0_ADDR

/**< 6th bit of Interrupt Enable 0 Registers */
#define SMI130_GYRO_INT_ENABLE0_FIFOEN__POS               6
#define SMI130_GYRO_INT_ENABLE0_FIFOEN__LEN               1
#define SMI130_GYRO_INT_ENABLE0_FIFOEN__MSK               0x40
#define SMI130_GYRO_INT_ENABLE0_FIFOEN__REG               SMI130_GYRO_INT_ENABLE0_ADDR

/**< 2nd bit of Interrupt Enable 0 Registers */
#define SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__POS        2
#define SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__LEN        1
#define SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__MSK        0x04
#define SMI130_GYRO_INT_ENABLE0_AUTO_OFFSETEN__REG        SMI130_GYRO_INT_ENABLE0_ADDR

/**< 3rd bit of Interrupt Enable 1 Registers */
#define SMI130_GYRO_INT_ENABLE1_IT2_OD__POS               3
#define SMI130_GYRO_INT_ENABLE1_IT2_OD__LEN               1
#define SMI130_GYRO_INT_ENABLE1_IT2_OD__MSK               0x08
#define SMI130_GYRO_INT_ENABLE1_IT2_OD__REG               SMI130_GYRO_INT_ENABLE1_ADDR

/**< 2nd bit of Interrupt Enable 1 Registers */
#define SMI130_GYRO_INT_ENABLE1_IT2_LVL__POS              2
#define SMI130_GYRO_INT_ENABLE1_IT2_LVL__LEN              1
#define SMI130_GYRO_INT_ENABLE1_IT2_LVL__MSK              0x04
#define SMI130_GYRO_INT_ENABLE1_IT2_LVL__REG              SMI130_GYRO_INT_ENABLE1_ADDR

/**< 1st bit of Interrupt Enable 1 Registers */
#define SMI130_GYRO_INT_ENABLE1_IT1_OD__POS               1
#define SMI130_GYRO_INT_ENABLE1_IT1_OD__LEN               1
#define SMI130_GYRO_INT_ENABLE1_IT1_OD__MSK               0x02
#define SMI130_GYRO_INT_ENABLE1_IT1_OD__REG               SMI130_GYRO_INT_ENABLE1_ADDR

/**< 0th bit of Interrupt Enable 1 Registers */
#define SMI130_GYRO_INT_ENABLE1_IT1_LVL__POS              0
#define SMI130_GYRO_INT_ENABLE1_IT1_LVL__LEN              1
#define SMI130_GYRO_INT_ENABLE1_IT1_LVL__MSK              0x01
#define SMI130_GYRO_INT_ENABLE1_IT1_LVL__REG              SMI130_GYRO_INT_ENABLE1_ADDR

/**< 3rd bit of Interrupt MAP 0 Registers */
#define SMI130_GYRO_INT_MAP_0_INT1_HIGH__POS            3
#define SMI130_GYRO_INT_MAP_0_INT1_HIGH__LEN            1
#define SMI130_GYRO_INT_MAP_0_INT1_HIGH__MSK            0x08
#define SMI130_GYRO_INT_MAP_0_INT1_HIGH__REG            SMI130_GYRO_INT_MAP_0_ADDR

/**< 1st bit of Interrupt MAP 0 Registers */
#define SMI130_GYRO_INT_MAP_0_INT1_ANY__POS             1
#define SMI130_GYRO_INT_MAP_0_INT1_ANY__LEN             1
#define SMI130_GYRO_INT_MAP_0_INT1_ANY__MSK             0x02
#define SMI130_GYRO_INT_MAP_0_INT1_ANY__REG             SMI130_GYRO_INT_MAP_0_ADDR

/**< 7th bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT2_DATA__POS                  7
#define SMI130_GYRO_MAP_1_INT2_DATA__LEN                  1
#define SMI130_GYRO_MAP_1_INT2_DATA__MSK                  0x80
#define SMI130_GYRO_MAP_1_INT2_DATA__REG                  SMI130_GYRO_INT_MAP_1_ADDR

/**< 6th bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__POS           6
#define SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__LEN           1
#define SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__MSK           0x40
#define SMI130_GYRO_MAP_1_INT2_FAST_OFFSET__REG           SMI130_GYRO_INT_MAP_1_ADDR

/**< 5th bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT2_FIFO__POS                  5
#define SMI130_GYRO_MAP_1_INT2_FIFO__LEN                  1
#define SMI130_GYRO_MAP_1_INT2_FIFO__MSK                  0x20
#define SMI130_GYRO_MAP_1_INT2_FIFO__REG                  SMI130_GYRO_INT_MAP_1_ADDR

/**< 4th bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__POS           4
#define SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__LEN           1
#define SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__MSK           0x10
#define SMI130_GYRO_MAP_1_INT2_AUTO_OFFSET__REG           SMI130_GYRO_INT_MAP_1_ADDR

/**< 3rd bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__POS           3
#define SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__LEN           1
#define SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__MSK           0x08
#define SMI130_GYRO_MAP_1_INT1_AUTO_OFFSET__REG           SMI130_GYRO_INT_MAP_1_ADDR

/**< 2nd bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT1_FIFO__POS                  2
#define SMI130_GYRO_MAP_1_INT1_FIFO__LEN                  1
#define SMI130_GYRO_MAP_1_INT1_FIFO__MSK                  0x04
#define SMI130_GYRO_MAP_1_INT1_FIFO__REG                  SMI130_GYRO_INT_MAP_1_ADDR

/**< 1st bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__POS           1
#define SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__LEN           1
#define SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__MSK           0x02
#define SMI130_GYRO_MAP_1_INT1_FAST_OFFSET__REG           SMI130_GYRO_INT_MAP_1_ADDR

/**< 0th bit of MAP_1Registers */
#define SMI130_GYRO_MAP_1_INT1_DATA__POS                  0
#define SMI130_GYRO_MAP_1_INT1_DATA__LEN                  1
#define SMI130_GYRO_MAP_1_INT1_DATA__MSK                  0x01
#define SMI130_GYRO_MAP_1_INT1_DATA__REG                  SMI130_GYRO_INT_MAP_1_ADDR

/**< 3rd bit of Interrupt Map 2 Registers */
#define SMI130_GYRO_INT_MAP_2_INT2_HIGH__POS            3
#define SMI130_GYRO_INT_MAP_2_INT2_HIGH__LEN            1
#define SMI130_GYRO_INT_MAP_2_INT2_HIGH__MSK            0x08
#define SMI130_GYRO_INT_MAP_2_INT2_HIGH__REG            SMI130_GYRO_INT_MAP_2_ADDR

/**< 1st bit of Interrupt Map 2 Registers */
#define SMI130_GYRO_INT_MAP_2_INT2_ANY__POS             1
#define SMI130_GYRO_INT_MAP_2_INT2_ANY__LEN             1
#define SMI130_GYRO_INT_MAP_2_INT2_ANY__MSK             0x02
#define SMI130_GYRO_INT_MAP_2_INT2_ANY__REG             SMI130_GYRO_INT_MAP_2_ADDR

/**< 5th bit of Interrupt 0 Registers */
#define SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__POS          5
#define SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__LEN          1
#define SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__MSK          0x20
#define SMI130_GYRO_INT_0_ADDR_SLOW_OFFSET_UNFILT__REG          SMI130_GYRO_INT_0_ADDR

/**< 3rd bit of Interrupt 0 Registers */
#define SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__POS            3
#define SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__LEN            1
#define SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__MSK            0x08
#define SMI130_GYRO_INT_0_ADDR_HIGH_UNFILT_DATA__REG            SMI130_GYRO_INT_0_ADDR

/**< 1st bit of Interrupt 0 Registers */
#define SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__POS             1
#define SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__LEN             1
#define SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__MSK             0x02
#define SMI130_GYRO_INT_0_ADDR_ANY_UNFILT_DATA__REG             SMI130_GYRO_INT_0_ADDR

/**< 7th bit of INT_1  Registers */
#define SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__POS            7
#define SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__LEN            1
#define SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__MSK            0x80
#define SMI130_GYRO_INT_1_ADDR_FAST_OFFSET_UNFILT__REG            SMI130_GYRO_INT_1_ADDR

/**< First 7 bits of INT_1  Registers */
#define SMI130_GYRO_INT_1_ADDR_ANY_TH__POS                       0
#define SMI130_GYRO_INT_1_ADDR_ANY_TH__LEN                       7
#define SMI130_GYRO_INT_1_ADDR_ANY_TH__MSK                       0x7F
#define SMI130_GYRO_INT_1_ADDR_ANY_TH__REG                       SMI130_GYRO_INT_1_ADDR

/**< Last 2 bits of INT 2Registers */
#define SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__POS          6
#define SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__LEN          2
#define SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__MSK          0xC0
#define SMI130_GYRO_INT_2_ADDR_AWAKE_DUR__REG          SMI130_GYRO_INT_2_ADDR

/**< 4th & 5th bit of INT 2Registers */
#define SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__POS      4
#define SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__LEN      2
#define SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__MSK      0x30
#define SMI130_GYRO_INT_2_ADDR_ANY_DURSAMPLE__REG      SMI130_GYRO_INT_2_ADDR

/**< 2nd bit of INT 2Registers */
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__POS           2
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__LEN           1
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__MSK           0x04
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Z__REG           SMI130_GYRO_INT_2_ADDR

/**< 1st bit of INT 2Registers */
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__POS           1
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__LEN           1
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__MSK           0x02
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_Y__REG           SMI130_GYRO_INT_2_ADDR

/**< 0th bit of INT 2Registers */
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_X__POS           0
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_X__LEN           1
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_X__MSK           0x01
#define SMI130_GYRO_INT_2_ADDR_ANY_EN_X__REG           SMI130_GYRO_INT_2_ADDR

/**< Last bit of INT 4 Registers */
#define SMI130_GYRO_INT_4_FIFO_WM_EN__POS           7
#define SMI130_GYRO_INT_4_FIFO_WM_EN__LEN           1
#define SMI130_GYRO_INT_4_FIFO_WM_EN__MSK           0x80
#define SMI130_GYRO_INT_4_FIFO_WM_EN__REG           SMI130_GYRO_INT_4_ADDR

/**< Last bit of Reset Latch Registers */
#define SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__POS           7
#define SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__LEN           1
#define SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__MSK           0x80
#define SMI130_GYRO_RST_LATCH_ADDR_RESET_INT__REG           SMI130_GYRO_RST_LATCH_ADDR

/**< 6th bit of Reset Latch Registers */
#define SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__POS        6
#define SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__LEN        1
#define SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__MSK        0x40
#define SMI130_GYRO_RST_LATCH_ADDR_OFFSET_RESET__REG        SMI130_GYRO_RST_LATCH_ADDR

/**< 4th bit of Reset Latch Registers */
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__POS        4
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__LEN        1
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__MSK        0x10
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_STATUS__REG        SMI130_GYRO_RST_LATCH_ADDR

/**< First 4 bits of Reset Latch Registers */
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__POS           0
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__LEN           4
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__MSK           0x0F
#define SMI130_GYRO_RST_LATCH_ADDR_LATCH_INT__REG           SMI130_GYRO_RST_LATCH_ADDR

/**< Last 2 bits of HIGH_TH_X Registers */
#define SMI130_GYRO_HIGH_HY_X__POS        6
#define SMI130_GYRO_HIGH_HY_X__LEN        2
#define SMI130_GYRO_HIGH_HY_X__MSK        0xC0
#define SMI130_GYRO_HIGH_HY_X__REG        SMI130_GYRO_HIGH_TH_X_ADDR

/**< 5 bits of HIGH_TH_X Registers */
#define SMI130_GYRO_HIGH_TH_X__POS        1
#define SMI130_GYRO_HIGH_TH_X__LEN        5
#define SMI130_GYRO_HIGH_TH_X__MSK        0x3E
#define SMI130_GYRO_HIGH_TH_X__REG        SMI130_GYRO_HIGH_TH_X_ADDR

/**< 0th bit of HIGH_TH_X Registers */
#define SMI130_GYRO_HIGH_EN_X__POS        0
#define SMI130_GYRO_HIGH_EN_X__LEN        1
#define SMI130_GYRO_HIGH_EN_X__MSK        0x01
#define SMI130_GYRO_HIGH_EN_X__REG        SMI130_GYRO_HIGH_TH_X_ADDR

/**< Last 2 bits of HIGH_TH_Y Registers */
#define SMI130_GYRO_HIGH_HY_Y__POS        6
#define SMI130_GYRO_HIGH_HY_Y__LEN        2
#define SMI130_GYRO_HIGH_HY_Y__MSK        0xC0
#define SMI130_GYRO_HIGH_HY_Y__REG        SMI130_GYRO_HIGH_TH_Y_ADDR

/**< 5 bits of HIGH_TH_Y Registers */
#define SMI130_GYRO_HIGH_TH_Y__POS        1
#define SMI130_GYRO_HIGH_TH_Y__LEN        5
#define SMI130_GYRO_HIGH_TH_Y__MSK        0x3E
#define SMI130_GYRO_HIGH_TH_Y__REG        SMI130_GYRO_HIGH_TH_Y_ADDR

/**< 0th bit of HIGH_TH_Y Registers */
#define SMI130_GYRO_HIGH_EN_Y__POS        0
#define SMI130_GYRO_HIGH_EN_Y__LEN        1
#define SMI130_GYRO_HIGH_EN_Y__MSK        0x01
#define SMI130_GYRO_HIGH_EN_Y__REG        SMI130_GYRO_HIGH_TH_Y_ADDR

/**< Last 2 bits of HIGH_TH_Z Registers */
#define SMI130_GYRO_HIGH_HY_Z__POS        6
#define SMI130_GYRO_HIGH_HY_Z__LEN        2
#define SMI130_GYRO_HIGH_HY_Z__MSK        0xC0
#define SMI130_GYRO_HIGH_HY_Z__REG        SMI130_GYRO_HIGH_TH_Z_ADDR

/**< 5 bits of HIGH_TH_Z Registers */
#define SMI130_GYRO_HIGH_TH_Z__POS        1
#define SMI130_GYRO_HIGH_TH_Z__LEN        5
#define SMI130_GYRO_HIGH_TH_Z__MSK        0x3E
#define SMI130_GYRO_HIGH_TH_Z__REG        SMI130_GYRO_HIGH_TH_Z_ADDR

/**< 0th bit of HIGH_TH_Z Registers */
#define SMI130_GYRO_HIGH_EN_Z__POS        0
#define SMI130_GYRO_HIGH_EN_Z__LEN        1
#define SMI130_GYRO_HIGH_EN_Z__MSK        0x01
#define SMI130_GYRO_HIGH_EN_Z__REG        SMI130_GYRO_HIGH_TH_Z_ADDR

/**< Last 3 bits of INT OFF0 Registers */
#define SMI130_GYRO_SLOW_OFFSET_TH__POS          6
#define SMI130_GYRO_SLOW_OFFSET_TH__LEN          2
#define SMI130_GYRO_SLOW_OFFSET_TH__MSK          0xC0
#define SMI130_GYRO_SLOW_OFFSET_TH__REG          SMI130_GYRO_SOC_ADDR

/**< 2  bits of INT OFF0 Registers */
#define SMI130_GYRO_SLOW_OFFSET_DUR__POS         3
#define SMI130_GYRO_SLOW_OFFSET_DUR__LEN         3
#define SMI130_GYRO_SLOW_OFFSET_DUR__MSK         0x38
#define SMI130_GYRO_SLOW_OFFSET_DUR__REG         SMI130_GYRO_SOC_ADDR

/**< 2nd bit of INT OFF0 Registers */
#define SMI130_GYRO_SLOW_OFFSET_EN_Z__POS        2
#define SMI130_GYRO_SLOW_OFFSET_EN_Z__LEN        1
#define SMI130_GYRO_SLOW_OFFSET_EN_Z__MSK        0x04
#define SMI130_GYRO_SLOW_OFFSET_EN_Z__REG        SMI130_GYRO_SOC_ADDR

/**< 1st bit of INT OFF0 Registers */
#define SMI130_GYRO_SLOW_OFFSET_EN_Y__POS        1
#define SMI130_GYRO_SLOW_OFFSET_EN_Y__LEN        1
#define SMI130_GYRO_SLOW_OFFSET_EN_Y__MSK        0x02
#define SMI130_GYRO_SLOW_OFFSET_EN_Y__REG        SMI130_GYRO_SOC_ADDR

/**< 0th bit of INT OFF0 Registers */
#define SMI130_GYRO_SLOW_OFFSET_EN_X__POS        0
#define SMI130_GYRO_SLOW_OFFSET_EN_X__LEN        1
#define SMI130_GYRO_SLOW_OFFSET_EN_X__MSK        0x01
#define SMI130_GYRO_SLOW_OFFSET_EN_X__REG        SMI130_GYRO_SOC_ADDR

/**< Last 2 bits of INT OFF1 Registers */
#define SMI130_GYRO_AUTO_OFFSET_WL__POS        6
#define SMI130_GYRO_AUTO_OFFSET_WL__LEN        2
#define SMI130_GYRO_AUTO_OFFSET_WL__MSK        0xC0
#define SMI130_GYRO_AUTO_OFFSET_WL__REG        SMI130_GYRO_A_FOC_ADDR

/**< 2  bits of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_WL__POS        4
#define SMI130_GYRO_FAST_OFFSET_WL__LEN        2
#define SMI130_GYRO_FAST_OFFSET_WL__MSK        0x30
#define SMI130_GYRO_FAST_OFFSET_WL__REG        SMI130_GYRO_A_FOC_ADDR

/**< 3nd bit of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_EN__POS        3
#define SMI130_GYRO_FAST_OFFSET_EN__LEN        1
#define SMI130_GYRO_FAST_OFFSET_EN__MSK        0x08
#define SMI130_GYRO_FAST_OFFSET_EN__REG        SMI130_GYRO_A_FOC_ADDR

/**< 2nd bit of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_EN_Z__POS      2
#define SMI130_GYRO_FAST_OFFSET_EN_Z__LEN      1
#define SMI130_GYRO_FAST_OFFSET_EN_Z__MSK      0x04
#define SMI130_GYRO_FAST_OFFSET_EN_Z__REG      SMI130_GYRO_A_FOC_ADDR

/**< 1st bit of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_EN_Y__POS      1
#define SMI130_GYRO_FAST_OFFSET_EN_Y__LEN      1
#define SMI130_GYRO_FAST_OFFSET_EN_Y__MSK      0x02
#define SMI130_GYRO_FAST_OFFSET_EN_Y__REG      SMI130_GYRO_A_FOC_ADDR

/**< 0th bit of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_EN_X__POS      0
#define SMI130_GYRO_FAST_OFFSET_EN_X__LEN      1
#define SMI130_GYRO_FAST_OFFSET_EN_X__MSK      0x01
#define SMI130_GYRO_FAST_OFFSET_EN_X__REG      SMI130_GYRO_A_FOC_ADDR

/**< 0 to 2 bits of INT OFF1 Registers */
#define SMI130_GYRO_FAST_OFFSET_EN_XYZ__POS      0
#define SMI130_GYRO_FAST_OFFSET_EN_XYZ__LEN      3
#define SMI130_GYRO_FAST_OFFSET_EN_XYZ__MSK      0x07
#define SMI130_GYRO_FAST_OFFSET_EN_XYZ__REG      SMI130_GYRO_A_FOC_ADDR

/**< Last 4 bits of Trim NVM control Registers */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN__POS        4
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN__LEN        4
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN__MSK        0xF0
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_REMAIN__REG        \
SMI130_GYRO_TRIM_NVM_CTRL_ADDR

/**< 3rd bit of Trim NVM control Registers */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__POS          3
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__LEN          1
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__MSK          0x08
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_LOAD__REG          \
SMI130_GYRO_TRIM_NVM_CTRL_ADDR

/**< 2nd bit of Trim NVM control Registers */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY__POS           2
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY__LEN           1
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY__MSK           0x04
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_RDY__REG           \
SMI130_GYRO_TRIM_NVM_CTRL_ADDR

 /**< 1st bit of Trim NVM control Registers */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__POS     1
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__LEN     1
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__MSK     0x02
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_TRIG__REG     \
SMI130_GYRO_TRIM_NVM_CTRL_ADDR

/**< 0th bit of Trim NVM control Registers */
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__POS     0
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__LEN     1
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__MSK     0x01
#define SMI130_GYRO_TRIM_NVM_CTRL_ADDR_NVM_PROG_MODE__REG     \
SMI130_GYRO_TRIM_NVM_CTRL_ADDR

 /**< 2nd bit of SPI3 WDT Registers */
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__POS      2
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__LEN      1
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__MSK      0x04
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_EN__REG      \
SMI130_GYRO_BGW_SPI3_WDT_ADDR

 /**< 1st bit of SPI3 WDT Registers */
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__POS     1
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__LEN     1
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__MSK     0x02
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_I2C_WDT_SEL__REG     \
SMI130_GYRO_BGW_SPI3_WDT_ADDR

/**< 0th bit of SPI3 WDT Registers */
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__POS            0
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__LEN            1
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__MSK            0x01
#define SMI130_GYRO_BGW_SPI3_WDT_ADDR_SPI3__REG            \
SMI130_GYRO_BGW_SPI3_WDT_ADDR

/**< 4th bit of Self test Registers */
#define SMI130_GYRO_SELF_TEST_ADDR_RATEOK__POS            4
#define SMI130_GYRO_SELF_TEST_ADDR_RATEOK__LEN            1
#define SMI130_GYRO_SELF_TEST_ADDR_RATEOK__MSK            0x10
#define SMI130_GYRO_SELF_TEST_ADDR_RATEOK__REG            \
SMI130_GYRO_SELF_TEST_ADDR

/**< 2nd bit of Self test Registers */
#define SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL__POS          2
#define SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL__LEN          1
#define SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL__MSK          0x04
#define SMI130_GYRO_SELF_TEST_ADDR_BISTFAIL__REG          \
SMI130_GYRO_SELF_TEST_ADDR

/**< 1st bit of Self test Registers */
#define SMI130_GYRO_SELF_TEST_ADDR_BISTRDY__POS           1
#define SMI130_GYRO_SELF_TEST_ADDR_BISTRDY__LEN           1
#define SMI130_GYRO_SELF_TEST_ADDR_BISTRDY__MSK           0x02
#define SMI130_GYRO_SELF_TEST_ADDR_BISTRDY__REG           \
SMI130_GYRO_SELF_TEST_ADDR

/**< 0th bit of Self test Registers */
#define SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST__POS          0
#define SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST__LEN          1
#define SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST__MSK          0x01
#define SMI130_GYRO_SELF_TEST_ADDR_TRIGBIST__REG          \
SMI130_GYRO_SELF_TEST_ADDR

/**< 7th bit of FIFO CGF1 Registers */
#define SMI130_GYRO_FIFO_CGF1_ADDR_TAG__POS     7
#define SMI130_GYRO_FIFO_CGF1_ADDR_TAG__LEN     1
#define SMI130_GYRO_FIFO_CGF1_ADDR_TAG__MSK     0x80
#define SMI130_GYRO_FIFO_CGF1_ADDR_TAG__REG     SMI130_GYRO_FIFO_CGF1_ADDR

/**< First 7 bits of FIFO CGF1 Registers */
#define SMI130_GYRO_FIFO_CGF1_ADDR_WML__POS     0
#define SMI130_GYRO_FIFO_CGF1_ADDR_WML__LEN     7
#define SMI130_GYRO_FIFO_CGF1_ADDR_WML__MSK     0x7F
#define SMI130_GYRO_FIFO_CGF1_ADDR_WML__REG     SMI130_GYRO_FIFO_CGF1_ADDR

/**< Last 2 bits of FIFO CGF0 Addr Registers */
#define SMI130_GYRO_FIFO_CGF0_ADDR_MODE__POS         6
#define SMI130_GYRO_FIFO_CGF0_ADDR_MODE__LEN         2
#define SMI130_GYRO_FIFO_CGF0_ADDR_MODE__MSK         0xC0
#define SMI130_GYRO_FIFO_CGF0_ADDR_MODE__REG         SMI130_GYRO_FIFO_CGF0_ADDR

/**< First 2 bits of FIFO CGF0 Addr Registers */
#define SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__POS     0
#define SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__LEN     2
#define SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__MSK     0x03
#define SMI130_GYRO_FIFO_CGF0_ADDR_DATA_SEL__REG     SMI130_GYRO_FIFO_CGF0_ADDR

 /**< Last 2 bits of INL Offset MSB Registers */
#define SMI130_GYRO_OFC1_ADDR_OFFSET_X__POS       6
#define SMI130_GYRO_OFC1_ADDR_OFFSET_X__LEN       2
#define SMI130_GYRO_OFC1_ADDR_OFFSET_X__MSK       0xC0
#define SMI130_GYRO_OFC1_ADDR_OFFSET_X__REG       SMI130_GYRO_OFC1_ADDR

/**< 3 bits of INL Offset MSB Registers */
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Y__POS       3
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Y__LEN       3
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Y__MSK       0x38
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Y__REG       SMI130_GYRO_OFC1_ADDR

/**< First 3 bits of INL Offset MSB Registers */
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Z__POS       0
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Z__LEN       3
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Z__MSK       0x07
#define SMI130_GYRO_OFC1_ADDR_OFFSET_Z__REG       SMI130_GYRO_OFC1_ADDR

/**< 4 bits of Trim GP0 Registers */
#define SMI130_GYRO_TRIM_GP0_ADDR_GP0__POS            4
#define SMI130_GYRO_TRIM_GP0_ADDR_GP0__LEN            4
#define SMI130_GYRO_TRIM_GP0_ADDR_GP0__MSK            0xF0
#define SMI130_GYRO_TRIM_GP0_ADDR_GP0__REG            SMI130_GYRO_TRIM_GP0_ADDR

/**< 2 bits of Trim GP0 Registers */
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__POS       2
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__LEN       2
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__MSK       0x0C
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_X__REG       SMI130_GYRO_TRIM_GP0_ADDR

/**< 1st bit of Trim GP0 Registers */
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__POS       1
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__LEN       1
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__MSK       0x02
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Y__REG       SMI130_GYRO_TRIM_GP0_ADDR

/**< First bit of Trim GP0 Registers */
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__POS       0
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__LEN       1
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__MSK       0x01
#define SMI130_GYRO_TRIM_GP0_ADDR_OFFSET_Z__REG       SMI130_GYRO_TRIM_GP0_ADDR

/* For Axis Selection   */
/**< It refers SMI130_GYRO X-axis */
#define SMI130_GYRO_X_AXIS           0
/**< It refers SMI130_GYRO Y-axis */
#define SMI130_GYRO_Y_AXIS           1
/**< It refers SMI130_GYRO Z-axis */
#define SMI130_GYRO_Z_AXIS           2

/* For Mode Settings    */
#define SMI130_GYRO_MODE_NORMAL              0
#define SMI130_GYRO_MODE_DEEPSUSPEND         1
#define SMI130_GYRO_MODE_SUSPEND             2
#define SMI130_GYRO_MODE_FASTPOWERUP			3
#define SMI130_GYRO_MODE_ADVANCEDPOWERSAVING 4

/* get bit slice  */
#define SMI130_GYRO_GET_BITSLICE(regvar, bitname)\
((regvar & bitname##__MSK) >> bitname##__POS)

/* Set bit slice */
#define SMI130_GYRO_SET_BITSLICE(regvar, bitname, val)\
((regvar&~bitname##__MSK)|((val<<bitname##__POS)&bitname##__MSK))
/* Constants */

#define SMI130_GYRO_NULL                             0
/**< constant declaration of NULL */
#define SMI130_GYRO_DISABLE                          0
/**< It refers SMI130_GYRO disable */
#define SMI130_GYRO_ENABLE                           1
/**< It refers SMI130_GYRO enable */
#define SMI130_GYRO_OFF                              0
/**< It refers SMI130_GYRO OFF state */
#define SMI130_GYRO_ON                               1
/**< It refers SMI130_GYRO ON state  */


#define SMI130_GYRO_TURN1                            0
/**< It refers SMI130_GYRO TURN1 */
#define SMI130_GYRO_TURN2                            1
/**< It refers SMI130_GYRO TURN2 */

#define SMI130_GYRO_INT1                             0
/**< It refers SMI130_GYRO INT1 */
#define SMI130_GYRO_INT2                             1
/**< It refers SMI130_GYRO INT2 */

#define SMI130_GYRO_SLOW_OFFSET                      0
/**< It refers SMI130_GYRO Slow Offset */
#define SMI130_GYRO_AUTO_OFFSET                      1
/**< It refers SMI130_GYRO Auto Offset */
#define SMI130_GYRO_FAST_OFFSET                      2
/**< It refers SMI130_GYRO Fast Offset */
#define SMI130_GYRO_S_TAP                            0
/**< It refers SMI130_GYRO Single Tap */
#define SMI130_GYRO_D_TAP                            1
/**< It refers SMI130_GYRO Double Tap */
#define SMI130_GYRO_INT1_DATA                        0
/**< It refers SMI130_GYRO Int1 Data */
#define SMI130_GYRO_INT2_DATA                        1
/**< It refers SMI130_GYRO Int2 Data */
#define SMI130_GYRO_TAP_UNFILT_DATA                   0
/**< It refers SMI130_GYRO Tap unfilt data */
#define SMI130_GYRO_HIGH_UNFILT_DATA                  1
/**< It refers SMI130_GYRO High unfilt data */
#define SMI130_GYRO_CONST_UNFILT_DATA                 2
/**< It refers SMI130_GYRO Const unfilt data */
#define SMI130_GYRO_ANY_UNFILT_DATA                   3
/**< It refers SMI130_GYRO Any unfilt data */
#define SMI130_GYRO_SHAKE_UNFILT_DATA                 4
/**< It refers SMI130_GYRO Shake unfilt data */
#define SMI130_GYRO_SHAKE_TH                         0
/**< It refers SMI130_GYRO Shake Threshold */
#define SMI130_GYRO_SHAKE_TH2                        1
/**< It refers SMI130_GYRO Shake Threshold2 */
#define SMI130_GYRO_AUTO_OFFSET_WL                   0
/**< It refers SMI130_GYRO Auto Offset word length */
#define SMI130_GYRO_FAST_OFFSET_WL                   1
/**< It refers SMI130_GYRO Fast Offset word length */
#define SMI130_GYRO_I2C_WDT_EN                       0
/**< It refers SMI130_GYRO I2C WDT En */
#define SMI130_GYRO_I2C_WDT_SEL                      1
/**< It refers SMI130_GYRO I2C WDT Sel */
#define SMI130_GYRO_EXT_MODE                         0
/**< It refers SMI130_GYRO Ext Mode */
#define SMI130_GYRO_EXT_PAGE                         1
/**< It refers SMI130_GYRO Ext page */
#define SMI130_GYRO_START_ADDR                       0
/**< It refers SMI130_GYRO Start Address */
#define SMI130_GYRO_STOP_ADDR                        1
/**< It refers SMI130_GYRO Stop Address */
#define SMI130_GYRO_SLOW_CMD                         0
/**< It refers SMI130_GYRO Slow Command */
#define SMI130_GYRO_FAST_CMD                         1
/**< It refers SMI130_GYRO Fast Command */
#define SMI130_GYRO_TRIM_VRA                         0
/**< It refers SMI130_GYRO Trim VRA */
#define SMI130_GYRO_TRIM_VRD                         1
/**< It refers SMI130_GYRO Trim VRD */
#define SMI130_GYRO_LOGBIT_EM                        0
/**< It refers SMI130_GYRO LogBit Em */
#define SMI130_GYRO_LOGBIT_VM                        1
/**< It refers SMI130_GYRO LogBit VM */
#define SMI130_GYRO_GP0                              0
/**< It refers SMI130_GYRO GP0 */
#define SMI130_GYRO_GP1                              1
/**< It refers SMI130_GYRO GP1*/
#define SMI130_GYRO_LOW_SPEED                        0
/**< It refers SMI130_GYRO Low Speed Oscillator */
#define SMI130_GYRO_HIGH_SPEED                       1
/**< It refers SMI130_GYRO High Speed Oscillator */
#define SMI130_GYRO_DRIVE_OFFSET_P                   0
/**< It refers SMI130_GYRO Drive Offset P */
#define SMI130_GYRO_DRIVE_OFFSET_N                   1
/**< It refers SMI130_GYRO Drive Offset N */
#define SMI130_GYRO_TEST_MODE_EN                     0
/**< It refers SMI130_GYRO Test Mode Enable */
#define SMI130_GYRO_TEST_MODE_REG                    1
/**< It refers SMI130_GYRO Test Mode reg */
#define SMI130_GYRO_IBIAS_DRIVE_TRIM                 0
/**< It refers SMI130_GYRO IBIAS Drive Trim */
#define SMI130_GYRO_IBIAS_RATE_TRIM                  1
/**< It refers SMI130_GYRO IBIAS Rate Trim */
#define SMI130_GYRO_BAA_MODE                         0
/**< It refers SMI130_GYRO BAA Mode Trim */
#define SMI130_GYRO_SMI_ACC_MODE                         1
/**< It refers SMI130_GYRO SMI_ACC Mode Trim */
#define SMI130_GYRO_PI_KP                            0
/**< It refers SMI130_GYRO PI KP */
#define SMI130_GYRO_PI_KI                            1
/**< It refers SMI130_GYRO PI KI */


#define C_SMI130_GYRO_SUCCESS						0
/**< It refers SMI130_GYRO operation is success */
#define C_SMI130_GYRO_FAILURE						1
/**< It refers SMI130_GYRO operation is Failure */

#define SMI130_GYRO_SPI_RD_MASK                      0x80
/**< Read mask **/
#define SMI130_GYRO_READ_SET                         0x01
/**< Setting for rading data **/

#define SMI130_GYRO_SHIFT_1_POSITION                 1
/**< Shift bit by 1 Position **/
#define SMI130_GYRO_SHIFT_2_POSITION                 2
/**< Shift bit by 2 Position **/
#define SMI130_GYRO_SHIFT_3_POSITION                 3
/**< Shift bit by 3 Position **/
#define SMI130_GYRO_SHIFT_4_POSITION                 4
/**< Shift bit by 4 Position **/
#define SMI130_GYRO_SHIFT_5_POSITION                 5
/**< Shift bit by 5 Position **/
#define SMI130_GYRO_SHIFT_6_POSITION                 6
/**< Shift bit by 6 Position **/
#define SMI130_GYRO_SHIFT_7_POSITION                 7
/**< Shift bit by 7 Position **/
#define SMI130_GYRO_SHIFT_8_POSITION                 8
/**< Shift bit by 8 Position **/
#define SMI130_GYRO_SHIFT_12_POSITION                12
/**< Shift bit by 12 Position **/

#define         C_SMI130_GYRO_Null_U8X                              0
#define         C_SMI130_GYRO_Zero_U8X                              0
#define         C_SMI130_GYRO_One_U8X                               1
#define         C_SMI130_GYRO_Two_U8X                               2
#define         C_SMI130_GYRO_Three_U8X                             3
#define         C_SMI130_GYRO_Four_U8X                              4
#define         C_SMI130_GYRO_Five_U8X                              5
#define         C_SMI130_GYRO_Six_U8X                               6
#define         C_SMI130_GYRO_Seven_U8X                             7
#define         C_SMI130_GYRO_Eight_U8X                             8
#define         C_SMI130_GYRO_Nine_U8X                              9
#define         C_SMI130_GYRO_Ten_U8X                               10
#define         C_SMI130_GYRO_Eleven_U8X                            11
#define         C_SMI130_GYRO_Twelve_U8X                            12
#define         C_SMI130_GYRO_Thirteen_U8X                          13
#define         C_SMI130_GYRO_Fifteen_U8X                           15
#define         C_SMI130_GYRO_Sixteen_U8X                           16
#define         C_SMI130_GYRO_TwentyTwo_U8X                         22
#define         C_SMI130_GYRO_TwentyThree_U8X                       23
#define         C_SMI130_GYRO_TwentyFour_U8X                        24
#define         C_SMI130_GYRO_TwentyFive_U8X                        25
#define         C_SMI130_GYRO_ThirtyTwo_U8X                         32
#define         C_SMI130_GYRO_Hundred_U8X                           100
#define         C_SMI130_GYRO_OneTwentySeven_U8X                    127
#define         C_SMI130_GYRO_OneTwentyEight_U8X                    128
#define         C_SMI130_GYRO_TwoFiftyFive_U8X                      255
#define         C_SMI130_GYRO_TwoFiftySix_U16X                      256

#define         E_SMI130_GYRO_NULL_PTR               (signed char)(-127)
#define         E_SMI130_GYRO_COMM_RES               (signed char)(-1)
#define         E_SMI130_GYRO_OUT_OF_RANGE           (signed char)(-2)

#define	C_SMI130_GYRO_No_Filter_U8X			0
#define	C_SMI130_GYRO_BW_230Hz_U8X			1
#define	C_SMI130_GYRO_BW_116Hz_U8X			2
#define	C_SMI130_GYRO_BW_47Hz_U8X			3
#define	C_SMI130_GYRO_BW_23Hz_U8X			4
#define	C_SMI130_GYRO_BW_12Hz_U8X			5
#define	C_SMI130_GYRO_BW_64Hz_U8X			6
#define	C_SMI130_GYRO_BW_32Hz_U8X			7

#define C_SMI130_GYRO_No_AutoSleepDur_U8X	0
#define	C_SMI130_GYRO_4ms_AutoSleepDur_U8X	1
#define	C_SMI130_GYRO_5ms_AutoSleepDur_U8X	2
#define	C_SMI130_GYRO_8ms_AutoSleepDur_U8X	3
#define	C_SMI130_GYRO_10ms_AutoSleepDur_U8X	4
#define	C_SMI130_GYRO_15ms_AutoSleepDur_U8X	5
#define	C_SMI130_GYRO_20ms_AutoSleepDur_U8X	6
#define	C_SMI130_GYRO_40ms_AutoSleepDur_U8X	7




#define SMI130_GYRO_WR_FUNC_PTR int (*bus_write)\
(unsigned char, unsigned char, unsigned char *, unsigned char)
#define SMI130_GYRO_RD_FUNC_PTR int (*bus_read)\
(unsigned char, unsigned char, unsigned char *, unsigned char)
#define SMI130_GYRO_BRD_FUNC_PTR int (*burst_read)\
(unsigned char, unsigned char, unsigned char *, SMI130_GYRO_S32)
#define SMI130_GYRO_MDELAY_DATA_TYPE SMI130_GYRO_U16




/*user defined Structures*/
struct smi130_gyro_data_t {
		SMI130_GYRO_S16 datax;
		SMI130_GYRO_S16 datay;
		SMI130_GYRO_S16 dataz;
		char intstatus[5];
};


struct smi130_gyro_offset_t {
		SMI130_GYRO_U16 datax;
		SMI130_GYRO_U16 datay;
		SMI130_GYRO_U16 dataz;
};


struct smi130_gyro_t {
		unsigned char chip_id;
		unsigned char dev_addr;
		SMI130_GYRO_BRD_FUNC_PTR;
		SMI130_GYRO_WR_FUNC_PTR;
		SMI130_GYRO_RD_FUNC_PTR;
		void(*delay_msec)(SMI130_GYRO_MDELAY_DATA_TYPE);
};

/***************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_init(struct smi130_gyro_t *p_smi130_gyro);
/***************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ***************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataX(SMI130_GYRO_S16 *data_x);
/****************************************************************
 * Description: *//**\brief
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
 ****************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataY(SMI130_GYRO_S16 *data_y);
/***************************************************************
 * Description: *//**\brief
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
 ***************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 *************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataZ(SMI130_GYRO_S16 *data_z);
/************************************************************
 * Description: *//**\brief
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
 *************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ***************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataXYZ(struct smi130_gyro_data_t *data);
/***************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ********************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_dataXYZI(struct smi130_gyro_data_t *data);
/********************************************************************
 * Description: *//**\brief
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
 ********************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_Temperature(unsigned char *temperature);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_FIFO_data_reg
(unsigned char *fifo_data);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_read_register(unsigned char addr,
unsigned char *data, unsigned char len);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_burst_read(unsigned char addr,
unsigned char *data, SMI130_GYRO_S32 len);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_write_register(unsigned char addr,
unsigned char *data, unsigned char len);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_0
(unsigned char *status0_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_1
(unsigned char *status1_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_2
(unsigned char *status2_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_interrupt_status_reg_3
(unsigned char *status3_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/

SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifostatus_reg
(unsigned char *fifo_status);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_range_reg
(unsigned char *range);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_range_reg
(unsigned char range);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_res
(unsigned char *high_res);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_res
(unsigned char high_res);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_bw(unsigned char *bandwidth);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_bw(unsigned char bandwidth);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_pmu_ext_tri_sel
(unsigned char *pwu_ext_tri_sel);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_pmu_ext_tri_sel
(unsigned char pwu_ext_tri_sel);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_bw
(unsigned char *high_bw);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_bw
(unsigned char high_bw);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_shadow_dis
(unsigned char *shadow_dis);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_shadow_dis
(unsigned char shadow_dis);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_soft_reset(void);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_data_enable(unsigned char *data_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_data_en(unsigned char data_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_enable(unsigned char *fifo_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_enable(unsigned char fifo_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset_enable
(unsigned char mode, unsigned char *offset_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_enable
(unsigned char mode, unsigned char offset_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_od
(unsigned char param, unsigned char *int_od);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_od
(unsigned char param, unsigned char int_od);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_lvl
(unsigned char param, unsigned char *int_lvl);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_lvl
(unsigned char param, unsigned char int_lvl);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_high
(unsigned char *int1_high);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_high
(unsigned char int1_high);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_any
(unsigned char *int1_any);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_any
(unsigned char int1_any);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_data
(unsigned char axis, unsigned char *int_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_data
(unsigned char axis, unsigned char int_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_offset
(unsigned char axis, unsigned char *int2_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_offset
(unsigned char axis, unsigned char int2_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_offset
(unsigned char axis, unsigned char *int1_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_offset
(unsigned char axis, unsigned char int1_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int_fifo(unsigned char *int_fifo);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int_fifo
(unsigned char axis, unsigned char int_fifo);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_high
(unsigned char *int2_high);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_high
(unsigned char int2_high);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_any
(unsigned char *int2_any);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_any
(unsigned char int2_any);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset_unfilt
(unsigned char param, unsigned char *offset_unfilt);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_unfilt
(unsigned char param, unsigned char offset_unfilt);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_unfilt_data
(unsigned char param, unsigned char *unfilt_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_unfilt_data
(unsigned char param, unsigned char unfilt_data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_th
(unsigned char *any_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_th
(unsigned char any_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_awake_dur
(unsigned char *awake_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_awake_dur
(unsigned char awake_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_dursample
(unsigned char *dursample);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_dursample
(unsigned char dursample);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_any_en_ch
(unsigned char channel, unsigned char *data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_any_en_ch
(unsigned char channel, unsigned char data);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_watermark_enable
(unsigned char *fifo_wn_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_watermark_enable
(unsigned char fifo_wn_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_reset_int
(unsigned char reset_int);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_reset
(unsigned char offset_reset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_latch_status
(unsigned char *latch_status);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_latch_status
(unsigned char latch_status);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_latch_int
(unsigned char *latch_int);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_latch_int
(unsigned char latch_int);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_hy
(unsigned char channel, unsigned char *high_hy);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_hy
(unsigned char channel, unsigned char high_hy);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_th
(unsigned char channel, unsigned char *high_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_th
(unsigned char channel, unsigned char high_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_en_ch
(unsigned char channel, unsigned char *high_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_en_ch
(unsigned char channel, unsigned char high_en);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_high_dur_ch
(unsigned char channel, unsigned char *high_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_high_dur_ch
(unsigned char channel, unsigned char high_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_th
(unsigned char *offset_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_th
(unsigned char offset_th);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_dur
(unsigned char *offset_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_dur
(unsigned char offset_dur);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_slow_offset_en_ch
(unsigned char channel, unsigned char *slow_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_slow_offset_en_ch
(unsigned char channel, unsigned char slow_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset_wl
(unsigned char channel, unsigned char *offset_wl);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset_wl
(unsigned char channel, unsigned char offset_wl);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fast_offset_en
(unsigned char fast_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fast_offset_en_ch
(unsigned char *fast_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fast_offset_en_ch
(unsigned char channel, unsigned char fast_offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_enable_fast_offset(void);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_remain
(unsigned char *nvm_remain);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_load
(unsigned char nvm_load);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_rdy
(unsigned char *nvm_rdy);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_prog_trig
(unsigned char prog_trig);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_nvm_prog_mode
(unsigned char *prog_mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_nvm_prog_mode
(unsigned char prog_mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_i2c_wdt
(unsigned char i2c_wdt, unsigned char *prog_mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_i2c_wdt
(unsigned char i2c_wdt, unsigned char prog_mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_spi3(unsigned char *spi3);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_spi3(unsigned char spi3);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_tag(unsigned char *tag);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_tag(unsigned char tag);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_watermarklevel
(unsigned char *water_mark_level);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_watermarklevel
(unsigned char water_mark_level);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_mode
(unsigned char *mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_mode(unsigned char mode);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_data_sel
(unsigned char *data_sel);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_fifo_data_sel
(unsigned char data_sel);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_offset
(unsigned char axis, SMI130_GYRO_S16 *offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_offset
(unsigned char axis, SMI130_GYRO_S16 offset);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_gp
(unsigned char param, unsigned char *value);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_gp
(unsigned char param, unsigned char value);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_framecount
(unsigned char *fifo_framecount);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_fifo_overrun
(unsigned char *fifo_overrun);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int2_fifo
(unsigned char *int_fifo);
/****************************************************************************
 * Description: *//**\brief
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
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_int1_fifo
(unsigned char *int_fifo);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int2_fifo
(unsigned char fifo_int2);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_int1_fifo
(unsigned char fifo_int1);
/****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_mode(unsigned char *mode);
/*****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_mode(unsigned char mode);
/*****************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *  \return
 *
 *
 ****************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ****************************************************************************/
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_selftest(unsigned char *result);
/*****************************************************************************
 * Description: *//**\brief  This API is used to get data auto sleep duration
 *
 *
 *
 *
 *  \param unsigned char *duration : Address of auto sleep duration
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
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_autosleepdur(unsigned char *duration);
/*****************************************************************************
 * Description: *//**\brief This API is used to set duration
 *
 *
 *
 *
 *  \param unsigned char duration:
 *          Value to be written passed as a parameter
 *		   unsigned char bandwidth:
 *			Value to be written passed as a parameter
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
unsigned char bandwith);
/*****************************************************************************
 * Description: *//**\brief  This API is used to get data sleep duration
 *
 *
 *
 *
 *  \param unsigned char *duration : Address of sleep duration
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
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_sleepdur(unsigned char *duration);
/*****************************************************************************
 * Description: *//**\brief This API is used to set duration
 *
 *
 *
 *
 *  \param unsigned char duration:
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
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_sleepdur(unsigned char duration);
/*****************************************************************************
 * Description: *//**\brief This API is used to set auto offset
 *
 *
 *
 *
 *  \param unsigned char duration:
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
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_set_auto_offset_en(unsigned char offset_en);
/*****************************************************************************
 * Description: *//**\brief This API is used to get auto offset
 *
 *
 *
 *
 *  \param unsigned char duration:
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
SMI130_GYRO_RETURN_FUNCTION_TYPE smi130_gyro_get_auto_offset_en(
unsigned char *offset_en);
#endif
