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
 *                     FTS Utility Functions                              *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 *
 */

#ifndef __FTS_TOOL_H
#define __FTS_TOOL_H

#define GPIO_NOT_DEFINED              -1
#define TIMEOUT_RESOLUTION            10 //ms
#define GENERAL_TIMEOUT               (50*TIMEOUT_RESOLUTION) //ms
#define RELEASE_INFO_TIMEOUT          (15*TIMEOUT_RESOLUTION) //ms


#define FEAT_ENABLE                   1
#define FEAT_DISABLE                  0

#define SYSTEM_RESET_RETRY            3

#define B2_RETRY                      2
//for FTM4 can not be greater than 13 bytes
#define LOCKDOWN_CODE_SIZE            10

#define LOCKDOWN_CODE_MAX_SIZE        63
#define LOCKDOWN_CODE_WRITE_CHUNK     12
#define LOCKDOWN_CODE_READ_CHUNK      4
#define LOCKDOWN_CODE_RETRY           2

int readB2(u16 address, u8 *outBuf, int len);
int readB2U16(u16 address, u8 *outBuf, int byteToRead);
int releaseInformation(void);
int lockDownInfo(u8 *data, int len);
int calculateCRC8(u8 *u8_srcBuff, int size, u8 *crc);
int writeLockDownInfo(u8 *data, int size);
int rewriteLockDownInfo(u8 *data, int size);
int readLockDownInfo(u8 *lockData, int *size);
char *printHex(char *label, u8 *buff, int count);
int pollForEvent(int *event_to_search, int event_bytes,
	u8 *readData, int time_to_wait);
int fts_disableInterrupt(void);
int fts_enableInterrupt(void);
int u8ToU16(u8 *src, u16 *dst);
int u8ToU16_le(u8 *src, u16 *dst);
int u8ToU16n(u8 *src, int src_length, u16 *dst);
int u16ToU8(u16 src, u8 *dst);
int u16ToU8_le(u16 src, u8 *dst);
int u16ToU8_be(u16 src, u8 *dst);
int u16ToU8n(u16 *src, int src_length, u8 *dst);
int u8ToU32(u8 *src, u32 *dst);
int u32ToU8(u32 src, u8 *dst);
int attempt_function(int(*code)(void), unsigned long wait_before_retry,
	int retry_count);
void setResetGpio(int gpio);
int fts_system_reset(void);
int isSystemResettedUp(void);
int isSystemResettedDown(void);
void setSystemResettedUp(int val);
void setSystemResettedDown(int val);
int senseOn(void);
int senseOff(void);
int keyOn(void);
int keyOff(void);
int featureEnableDisable(int on_off, u32 feature);
int writeNoiseParameters(u8 *noise);
int readNoiseParameters(u8 *noise);
int checkEcho(u8 *cmd, int size);
void print_frame_short(char *label, short **matrix, int row, int column);
short **array1dTo2d_short(short *data, int size, int columns);
u8 **array1dTo2d_u8(u8 *data, int size, int columns);
void print_frame_u8(char *label, u8 **matrix, int row, int column);
void print_frame_u32(char *label, u32 **matrix, int row, int column);
void print_frame_int(char *label, int **matrix, int row, int column);
int cleanUp(int enableTouch);
int flushFIFO(void);

#endif
