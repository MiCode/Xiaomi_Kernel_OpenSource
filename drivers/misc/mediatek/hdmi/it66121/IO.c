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
#include "IO.h"
#include "itx_typedef.h"
#include "mcu.h"

unsigned char HDMITX_ReadI2C_Byte(unsigned char RegAddr)
{
	unsigned char p_data;

	it66121_i2c_read_byte(RegAddr, &p_data);
	return p_data;
}

SYS_STATUS HDMITX_WriteI2C_Byte(unsigned char RegAddr, unsigned char d)
{
	bool flag;

	flag = it66121_i2c_write_byte(RegAddr, d);

	return flag;
}

SYS_STATUS HDMITX_ReadI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				int N)
{
	bool flag;

	flag = it66121_i2c_read_block(RegAddr, pData, N);

	return flag;
}

SYS_STATUS HDMITX_WriteI2C_ByteN(unsigned char RegAddr, unsigned char *pData,
				 int N)
{
	bool flag;

	flag = it66121_i2c_write_block(RegAddr, pData, N);
	return flag;
}

SYS_STATUS HDMITX_SetI2C_Byte(unsigned char Reg, unsigned char Mask,
			      unsigned char Value)
{
	unsigned char Temp;

	if (Mask != 0xFF) {
		Temp = HDMITX_ReadI2C_Byte(Reg);
		Temp &= (~Mask);
		Temp |= Value & Mask;
	} else {
		Temp = Value;
	}
	return HDMITX_WriteI2C_Byte(Reg, Temp);
}

SYS_STATUS HDMITX_ToggleBit(unsigned char Reg, unsigned char n)
{
	unsigned char Temp;

	Temp = HDMITX_ReadI2C_Byte(Reg);
	/* HDMITX_DEBUG_PRINTF(("INVERVIT  0x%bx[%bx]",Reg,n)); */
	/* IT66121_LOG("reg%02X = %02X -> toggle %dth bit
	 * ->",(int)Reg,(int)Temp,(int)n) ;
	 */
	Temp ^= (1 << n);
	/* IT66121_LOG(" %02X\n",(int)Temp) ; */

	/* HDMITX_DEBUG_PRINTF(("0x%bx\n",Temp)); */
	return HDMITX_WriteI2C_Byte(Reg, Temp);
}
