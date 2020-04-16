/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _IO_h_
#define _IO_h_
#include "mcu.h"
/* #include "main.h" */
#include "itx_typedef.h"
#include "utility.h"
/* /////////////////////////////////////////////////////////////////////////////
 */
/* Start: I2C for 8051 */
/* /////////////////////////////////////////////////////////////////////////////
 */
/* /////////////////////////////////////////////////////////////////////////////
 */
/* I2C for original function call */
/* /////////////////////////////////////////////////////////////////////////////
 */
int it66121_i2c_read_byte(u8 addr, u8 *data);
int it66121_i2c_write_byte(u8 addr, u8 data);

int it66121_i2c_read_block(u8 addr, u8 *data, int len);
int it66121_i2c_write_block(u8 addr, u8 *data, int len);

unsigned char HDMITX_ReadI2C_Byte(unsigned char RegAddr);
SYS_STATUS HDMITX_WriteI2C_Byte(unsigned char RegAddr, unsigned char d);
SYS_STATUS HDMITX_ReadI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				int N);
SYS_STATUS HDMITX_WriteI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				 int N);
SYS_STATUS HDMITX_SetI2C_Byte(unsigned char Reg, unsigned char Mask,
			      unsigned char Value);
SYS_STATUS HDMITX_ToggleBit(unsigned char Reg, unsigned char n);

/*unsigned char CEC_ReadI2C_Byte(unsigned char RegAddr);*/
/*SYS_STATUS CEC_WriteI2C_Byte(unsigned char RegAddr,unsigned char d);*/
/*SYS_STATUS CEC_ReadI2C_ByteN(unsigned char RegAddr,unsigned char *pData,int
 * N);
 */
/*SYS_STATUS CEC_WriteI2C_ByteN(unsigned char RegAddr,unsigned char _CODE
 * *pData,int N);
 */
/*SYS_STATUS CEC_SetI2C_Byte(unsigned char Reg,unsigned char Mask,unsigned char
 * Value);
 */
/*SYS_STATUS CEC_ToggleBit(unsigned char Reg,unsigned char n);*/

#endif
