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
#include "mcu.h"
#include "hdmitx.h"
/* #include "TimerProcess.h" */

void delay1ms(unsigned short ms)
{
	mdelay(ms);
}


void HoldSystem(void)
{
}

#if 1				/* def SUPPORT_UART_CMD */

/* I2C for original function call */


void DumpReg(unsigned char dumpAddress)
{
}
#endif
