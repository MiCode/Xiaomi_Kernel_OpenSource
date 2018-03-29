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
 *    File	: lgtp_platform_api_i2c.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_PLATFORM_I2C_H_)
#define _LGTP_PLATFORM_I2C_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/
#include <linux/async.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/


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
struct i2c_client *Touch_Get_I2C_Handle(void);
int Touch_I2C_Read(u16 addr, u8 *rxbuf, int len);
int Touch_I2C_Write(u16 addr, u8 *txbuf, int len);
int Mit300_I2C_Write(struct i2c_client *client, u8 *writeBuf, u32 write_len);
int Mit300_I2C_Read(struct i2c_client *client, u8 *addr, u8 addrLen, u8 *rxbuf, int len);
int FT8707_I2C_Write(struct i2c_client *client, u8 *writeBuf, u32 write_len);
int FT8707_I2C_Read(struct i2c_client *client, u8 *addr, u8 addrLen, u8 *rxbuf, int len);
int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue);
int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue);
#endif				/* _LGTP_PLATFORM_I2C_H_ */

/* End Of File */
