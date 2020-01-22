/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2019, STMicroelectronics Limited.
 * Authors: AMG(Analog Mems Group) <marco.cali@st.com>
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

/**
 *
 **************************************************************************
 **                        STMicroelectronics                            **
 **************************************************************************
 **                        marco.cali@st.com                             **
 **************************************************************************
 *                                                                        *
 *                     I2C/SPI Communication                             **
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */

#ifndef __FTS_IO_H
#define __FTS_IO_H

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "ftsSoftware.h"
#include "ftsCrossCompile.h"

#define I2C_RETRY               3 //number of retry
#define I2C_WAIT_BEFORE_RETRY   2 //ms

int openChannel(struct i2c_client *clt);
struct device *getDev(void);
struct i2c_client *getClient(void);
int fts_readCmd(u8 *cmd, int cmdLenght, u8 *outBuf, int byteToRead);
int fts_writeCmd(u8 *cmd, int cmdLenght);
int fts_writeFwCmd(u8 *cmd, int cmdLenght);
int writeReadCmd(u8 *writeCmd, int writeCmdLenght, u8 *readCmd,
	int readCmdLenght, u8 *outBuf, int byteToRead);
int readCmdU16(u8 cmd, u16 address, u8 *outBuf, int byteToRead,
	int hasDummyByte);
int writeCmdU16(u8 WriteCmd, u16 address, u8 *dataToWrite, int byteToWrite);
int writeCmdU32(u8 writeCmd1, u8 writeCmd2, u32 address, u8 *dataToWrite,
	int byteToWrite);
int writeReadCmdU32(u8 wCmd, u8 rCmd, u32 address, u8 *outBuf, int byteToRead,
	int hasDummyByte);

#endif
