/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File  : lgtp_model_config_i2c.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_MODEL_CONFIG_I2C_H_)
#define _LGTP_MODEL_CONFIG_I2C_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/
#if defined(TOUCH_PLATFORM_MTK)

#if defined(TOUCH_MODEL_Y70)
#define TOUCH_I2C_BUS_NUM 0
#define TOUCH_S3320_I2C_SLAVE_ADDR 0x20

#elif defined(TOUCH_MODEL_LION_3G)
#define TOUCH_I2C_USE
#define TOUCH_I2C_BUS_NUM 0
#define TOUCH_I2C_ADDRESS_8BIT
#define TOUCH_TD4191_I2C_SLAVE_ADDR 0x20

#elif defined(TOUCH_MODEL_M1V) || defined(TOUCH_MODEL_M4)
#define TOUCH_I2C_USE
#define TOUCH_I2C_BUS_NUM 1
#define TOUCH_I2C_ADDRESS_8BIT
#define TOUCH_MIT300_I2C_SLAVE_ADDR 0x34

#elif defined(TOUCH_MODEL_K7)
#define TOUCH_I2C_USE
#define TOUCH_I2C_BUS_NUM 1
#define TOUCH_I2C_ADDRESS_8BIT
#define TOUCH_FT8707_I2C_SLAVE_ADDR 0x38

#else
#error "Model should be defined"
#endif

#endif

#if defined(TOUCH_MODEL_Y30)
#define TOUCH_I2C_USE
#define TOUCH_I2C_ADDRESS_16BIT
#define TOUCH_I2C_BUS_NUM 1

#elif defined(TOUCH_MODEL_P1C)
#define TOUCH_I2C_USE
#define TOUCH_I2C_ADDRESS_8BIT
#define TOUCH_I2C_BUS_NUM 5
#endif

/****************************************************************************
* Type Definitions
****************************************************************************/


/****************************************************************************
* Exported Variables
****************************************************************************/


/****************************************************************************
* Macros
****************************************************************************/


/****************************************************************************
* Global Function Prototypes
****************************************************************************/
#if defined(TOUCH_PLATFORM_MTK)
int TouchGetDeviceSlaveAddress(int index);
#endif

#if defined(TOUCH_PLATFORM_QCT) || defined(CONFIG_USE_OF)
struct of_device_id *TouchGetDeviceMatchTable(int index);
#endif


#endif				/* _LGTP_MODEL_CONFIG_I2C_H_ */

/* End Of File */
