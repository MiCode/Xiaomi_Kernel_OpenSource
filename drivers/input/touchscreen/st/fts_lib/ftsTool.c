// SPDX-License-Identifier: GPL-2.0-only
/*
 * FTS Capacitive touch screen controller (FingerTipS)
 *
 * Copyright (C) 2016-2018, STMicroelectronics Limited.
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

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/gpio.h>

#include "ftsCompensation.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTime.h"
#include "ftsFlash.h"
#include "ftsTool.h"
#include "../fts.h"

static char tag[8] = "[ FTS ]\0";
static int reset_gpio = GPIO_NOT_DEFINED;
static int system_resetted_up;
static int system_resetted_down;

int readB2(u16 address, u8 *outBuf, int len)
{
	int remaining = len;
	int toRead = 0;
	int retry = 0;
	int ret;
	int event_to_search[3];
	char *temp = NULL;
	u8 *init_outBuf = outBuf;
	u16 init_addr = address;
	u8 readEvent[FIFO_EVENT_SIZE] = {0};
	u8 cmd[4] = { FTS_CMD_REQU_FW_CONF, 0x00, 0x00, (u8)len };

	if (readEvent == NULL) {
		logError(1, "%s %s:ERROR %02X\n", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}
	u16ToU8_be(address, &cmd[1]);
	temp = printHex("Command B2 = ", cmd, 4);
	if (temp != NULL)
		logError(0, "%s %s", tag, temp);
	kfree(temp);
	do {
		remaining = len;
		ret = fts_writeFwCmd(cmd, 4);
		if (ret < 0) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
			return ret;
		}
		//ask to the FW the data
		logError(0, "%s Command to FW sent!\n", tag);
		event_to_search[0] = (int)EVENTID_FW_CONFIGURATION;
		while (remaining > OK) {
			event_to_search[1] = (int)((address & 0xFF00) >> 8);
			event_to_search[2] = (int)(address & 0x00FF);
			if (remaining > B2_DATA_BYTES) {
				toRead = B2_DATA_BYTES;
				remaining -= B2_DATA_BYTES;
			} else {
				toRead = remaining;
				remaining = 0;
			}

			ret = pollForEvent(event_to_search, 3,
				readEvent, GENERAL_TIMEOUT);
			if (ret >= OK) {
				//start the polling for reading the reply
				memcpy(outBuf, &readEvent[3], toRead);
				retry = 0;
				outBuf += toRead;
			} else {
				retry += 1;
				break;
			}
			address += B2_DATA_BYTES;
		}
		logError(0, "%s %s:B2 failed...attempt = %d\n",
			tag, __func__, retry);
		outBuf = init_outBuf;
		address = init_addr;
	} while (retry < B2_RETRY && retry != 0);

	if (retry == B2_RETRY) {
		logError(1, "%s %s:ERROR %02X\n", tag, __func__, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}
	logError(0, "%s B2 read %d bytes\n", tag, len);

	return OK;
}

int readB2U16(u16 address, u8 *outBuf, int byteToRead)
{
	int remaining = byteToRead;
	int toRead = 0;
	int ret;

	u8 *buff = (u8 *)kmalloc_array((B2_CHUNK + 1), sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
		if (remaining >= B2_CHUNK) {
			toRead = B2_CHUNK;
			remaining -= B2_CHUNK;
		} else {
			toRead = remaining;
			remaining = 0;
		}

		ret = readB2(address, buff, toRead);
		if (ret < 0) {
			kfree(buff);
			return ret;
		}
		memcpy(outBuf, buff, toRead);
		address += toRead;
		outBuf += toRead;
	}
	kfree(buff);
	return OK;
}


int releaseInformation(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_RELEASE_INFO };
	int event_to_search[1];
	u8 readEvent[FIFO_EVENT_SIZE];

	event_to_search[0] = (int)EVENTID_RELEASE_INFO;

	logError(0, "%s %s: started... Chip INFO:\n", tag, __func__);

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		return ret;
	}

	ret = pollForEvent(event_to_search, 1, &readEvent[0],
			RELEASE_INFO_TIMEOUT);
	//start the polling for reading the reply
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		return ret;
	}

	logError(0, "%s %s: Finished! %d\n", tag, __func__, ret);
	return OK;
}

int lockDownInfo(u8 *data, int len)
{
	int ret;
	int i = 0, num_event;
	u8 cmd[1] = { FTS_CMD_LOCKDOWN_CMD };
	int event_to_search[3] = {EVENTID_LOCKDOWN_INFO,
			EVENT_TYPE_LOCKDOWN, 0x00};
	u8 readEvent[FIFO_EVENT_SIZE];

	logError(0, "%s %s:started...\n", tag, __func__);
	if (len <= 0)
		return ERROR_OP_NOT_ALLOW;

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s:ERROR %02X\n", tag, __func__, ret);
		return ret;
	}

	num_event = (len + 3) / 4;
	logError(0, "%s %s:num_event = %d\n", tag, __func__, num_event);

	for (i = 0; i < num_event; i++) {
		ret = pollForEvent(event_to_search, 3,
			&readEvent[0], GENERAL_TIMEOUT);
		//start the polling for reading the reply
		if (ret < OK) {
			logError(1, "%s %s:ERROR %02X\n", tag, __func__, ret);
			return ret;
		}
		data[i * 4] = readEvent[3];
		data[i * 4 + 1] = readEvent[4];
		data[i * 4 + 2] = readEvent[5];
		data[i * 4 + 3] = readEvent[6];
		event_to_search[2] += 4;
		//logError(0, "%02X %02X %02X %02X ", readEvent[3],
		//readEvent[4], readEvent[5], readEvent[6]);
	}

	logError(0, "%s %s:Finished! %d\n", tag, __func__, ret);
	return OK;
}


int calculateCRC8(u8 *u8_srcBuff, int size, u8 *crc)
{
	u8 u8_remainder;
	u8 bit;
	int i = 0;

	u8_remainder = 0x00;
	logError(0, "%s %s: Start CRC computing...\n", tag, __func__);

	if (size == 0 || u8_srcBuff == NULL) {
		logError(1, "Arguments passed not valid!");
		logError(1, "%s %s:Data pointer = NULL ", tag, __func__);
		logError(1, "or size = 0 (%d) ERROR %08X\n",
			 size, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	// Perform modulo-2 division, a byte at a time.
	//Bring the next byte into the remainder.
	for (i = 0; i < size; i++) {
		//Perform modulo-2 division, a bit at a time.
		u8_remainder ^= u8_srcBuff[i];
		//Try to divide the current data bit.
		for (bit = 8; bit > 0; --bit) {
			if (u8_remainder & (0x1 << 7))
				u8_remainder = (u8_remainder << 1) ^ 0x9B;
			else
				u8_remainder = (u8_remainder << 1);
		}
	} //The final remainder is the CRC result.
	*crc = u8_remainder;
	logError(0, "%s %s: CRC value = %02X\n", tag, __func__, *crc);
	return OK;
}

int writeLockDownInfo(u8 *data, int size)
{
	int ret, i, toWrite, retry = 0, offset = size;
	u8 cmd[2 + LOCKDOWN_CODE_WRITE_CHUNK] = {FTS_CMD_LOCKDOWN_FILL, 0x00};
	u8 crc = 0;
	int event_to_search[2] = {EVENTID_STATUS_UPDATE,
		EVENT_TYPE_LOCKDOWN_WRITE};
	u8 readEvent[FIFO_EVENT_SIZE];
	char *temp = NULL;

	logError(0, "%s %s: Writing Lockdown code into the IC...\n",
		tag, __func__);

	ret = fts_disableInterrupt();
	if (ret < OK) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__, ret);
		ret = (ret | ERROR_LOCKDOWN_CODE);
		goto ERROR;
	}
	if (size > LOCKDOWN_CODE_MAX_SIZE) {
		logError(1, "%s %s: Lockdown data to write too big! ",
			tag, __func__);
		logError(1, "%d>%d ERROR %08X\n",
			size, LOCKDOWN_CODE_MAX_SIZE, ret);
		ret = (ERROR_OP_NOT_ALLOW | ERROR_LOCKDOWN_CODE);
		goto ERROR;
	}

	temp = printHex("Lockdown Code = ", data, size);
	if (temp != NULL) {
		logError(0, "%s %s: %s", tag, __func__, temp);
		kfree(temp);
	}

	for (retry = 0; retry < LOCKDOWN_CODE_RETRY; retry++) {
		logError(0, "%s %s: Filling FW buffer...\n", tag, __func__);
		i = 0;
		offset = size;
		cmd[0] = FTS_CMD_LOCKDOWN_FILL;
		while (offset > 0) {
			if (offset > LOCKDOWN_CODE_WRITE_CHUNK)
				toWrite = LOCKDOWN_CODE_WRITE_CHUNK;
			else
				toWrite = offset;
			memcpy(&cmd[2], &data[i], toWrite);
			cmd[1] = i;

			temp = printHex("Commmand = ", cmd, 2 + toWrite);
			if (temp != NULL) {
				logError(0, "%s %s: %s", tag, __func__, temp);
				kfree(temp);
			}
			ret = fts_writeFwCmd(cmd, 2 + toWrite);
			if (ret < OK) {
				logError(1, "Unable to write Lockdown data ");
				logError(1, "%s %s:Lockdown data at %d ",
					tag, __func__, i);
				logError(1, "iteration.%08X\n", ret);
				ret = (ret | ERROR_LOCKDOWN_CODE);
				continue;
			}
			i += toWrite;//update the offset
			offset -= toWrite;
		}
		logError(0, "%s %s: Compute 8bit CRC...\n", tag, __func__);
		ret = calculateCRC8(data, size, &crc);
		if (ret < OK) {
			logError(1, "%s %s:Unable to compute CRC..ERROR %08X\n",
				tag, __func__, ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
			continue;
		}
		cmd[0] = FTS_CMD_LOCKDOWN_WRITE;
		cmd[1] = 0x00;
		cmd[2] = (u8)size;
		cmd[3] = crc;
		logError(0, "%s %s: Write Lockdown data...\n",
			tag, __func__);
		temp = printHex("Commmand = ", cmd, 4);
		if (temp != NULL) {
			logError(0, "%s %s: %s", tag, __func__, temp);
			kfree(temp);
		}
		ret = fts_writeFwCmd(cmd, 4);
		if (ret < OK) {
			logError(1, "%s%s:Unable to send Lockdown data ",
				tag, __func__);
			logError(1, "write command%08X\n", ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
			continue;
		}

		ret = pollForEvent(event_to_search,
			2,
			&readEvent[0],
			GENERAL_TIMEOUT);
		//start the polling for reading the reply

		if (ret < OK) {
			logError(1, "%s%s:Cann't find lockdown code ",
				tag, __func__);
			logError(1, "%write reply %08X\n", ret);
			continue;
		}

		if (readEvent[2] != 0x00) {
			logError(1, "%s %s:Event check FAIL!%02X != 0x00 ",
				tag, __func__, readEvent[2]);
			logError(1, "%ERR%08X\n", ERROR_LOCKDOWN_CODE);
			ret = ERROR_LOCKDOWN_CODE;
			continue;
		} else {
			logError(0, "%s %s:Lockdown Code write DONE!\n",
				tag, __func__);
			ret = OK;
			break;
		}
	}

ERROR:
	//ret = fts_enableInterrupt();
	//ensure that the interrupt are always renabled when exit from funct
	if (fts_enableInterrupt() < OK) {
		logError(1, "%s %s: Error while re-enabling the interrupt!\n",
			tag, __func__);
	}
	return ret;
}

int rewriteLockDownInfo(u8 *data, int size)
{
	int ret, i, toWrite, retry = 0, offset = size;
	u8 cmd[2 + LOCKDOWN_CODE_WRITE_CHUNK] = {FTS_CMD_LOCKDOWN_FILL, 0x00};
	u8 crc = 0;
	int event_to_search[2] = {EVENTID_STATUS_UPDATE,
			EVENT_TYPE_LOCKDOWN_WRITE};
	u8 readEvent[FIFO_EVENT_SIZE];
	char *temp = NULL;

	logError(0, "%s %s: ReWriting Lockdown code into the IC start ...\n",
		tag, __func__);

	ret = fts_disableInterrupt();
	if (ret < OK) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__, ret);
		ret = (ret | ERROR_LOCKDOWN_CODE);
			goto ERROR;
	}
	if (size > LOCKDOWN_CODE_MAX_SIZE) {
		logError(1, "%s %s: Lockdown data to write too big! ",
			tag, __func__);
		logError(1, "%d>%d  ERROR %08X\n",
			size, LOCKDOWN_CODE_MAX_SIZE, ret);
		ret = (ERROR_OP_NOT_ALLOW | ERROR_LOCKDOWN_CODE);
			goto ERROR;
	}

	temp = printHex("Lockdown Code = ", data, size);
	if (temp != NULL) {
		logError(0, "%s %s: %s", tag, __func__, temp);
		kfree(temp);
	}

	for (retry = 0; retry < LOCKDOWN_CODE_RETRY; retry++) {
		logError(0, "%s %s: Filling FW buffer ...\n", tag, __func__);
		i = 0;
		offset = size;
		cmd[0] = FTS_CMD_LOCKDOWN_FILL;
		while (offset > 0) {
			if (offset > LOCKDOWN_CODE_WRITE_CHUNK)
				toWrite = LOCKDOWN_CODE_WRITE_CHUNK;
			else
				toWrite = offset;
			memcpy(&cmd[2], &data[i], toWrite);
			cmd[1] = i;
			temp = printHex("Commmand = ", cmd, 2 + toWrite);
			if (temp != NULL) {
				logError(0, "%s %s: %s", tag, __func__, temp);
				kfree(temp);
			}
			ret = fts_writeFwCmd(cmd, 2 + toWrite);
			if (ret < OK) {
				logError(1, "Unable to rewrite Lockdown data");
				logError(1, "%s %s: at %d iteration ",
					tag, __func__, i);
				logError(1, "ERROR %08X\n", ret);
				ret = (ret | ERROR_LOCKDOWN_CODE);
					continue;
			}
			i += toWrite;//update the offset
			offset -= toWrite;
		}
		logError(0, "%s %s: Compute 8bit CRC...\n", tag, __func__);
		ret = calculateCRC8(data, size, &crc);
		if (ret < OK) {
			logError(1, "%s %s:Unable to compute CRC.. ",
				tag, __func__);
			logError(1, "ERROR %08X\n", ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
				continue;
		}
		cmd[0] = FTS_CMD_LOCKDOWN_WRITE;
		cmd[1] = 0x01;
		cmd[2] = (u8)size;
		cmd[3] = crc;

		temp = printHex("Commmand = ", cmd, 4);
		if (temp != NULL) {
			logError(0, "%s %s: %s", tag, __func__, temp);
			kfree(temp);
		}
		logError(0, "%s %s: ReWrite Lockdown data...\n", tag, __func__);
		ret = fts_writeFwCmd(cmd, 4);
		if (ret < OK) {
			logError(1, "Unable to send Lockdown data");
			logError(1, "%s %s:rewrite command... ERROR %08X\n",
				tag, __func__, ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
			continue;
		}

		//start the polling for reading the reply
		ret = pollForEvent(event_to_search, 2,
			&readEvent[0], GENERAL_TIMEOUT);
		if (ret >= OK) {
			if (readEvent[2] < 0x00) {
				logError(1, "%s %s:Event check FAIL! ",
					tag, __func__);
				logError(1, "%02X != 0x00 %08X\n",
					readEvent[2], ERROR_LOCKDOWN_CODE);
				ret = ERROR_LOCKDOWN_CODE;
					continue;
			} else {
				logError(0, "%s %s: Lockdown Code ",
					tag, __func__);
				logError(0, "rewrite DONE!\n");
				ret = OK;
				break;
			}
		} else {
			logError(1, "Can not find lockdown code write ");
			logError(1, "reply event!%s %s: ERROR %08X\n",
				tag, __func__, ret);
		}
	}
ERROR:
	//ret = fts_enableInterrupt();
	//ensure that the interrupt are always renabled when exit from funct
	if (fts_enableInterrupt() < OK) {
		logError(1, "%s %s: Error while re-enabling the interrupt!\n",
			tag, __func__);
	}
	return ret;
}

int readLockDownInfo(u8 *lockData, int *size)
{
	int ret, retry = 0, toRead = 0, byteToRead;
	u8 cmd = FTS_CMD_LOCKDOWN_READ;
	int event_to_search[3] = {EVENTID_LOCKDOWN_INFO_READ, -1, 0x00};
	u8 readEvent[FIFO_EVENT_SIZE];
	char *temp = NULL;

	lockData = NULL;
	logError(0, "%s %s: Reading Lockdown code from the IC...\n",
		tag, __func__);

	ret = fts_disableInterrupt();
	if (ret < OK) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__, ret);
		ret = (ret | ERROR_LOCKDOWN_CODE);
			goto ERROR;
	}
	for (retry = 0; retry < LOCKDOWN_CODE_RETRY; retry++) {
		event_to_search[2] = 0x00;
		logError(0, "%s %s: Read Lockdown data.(%d attempt)\n",
			tag, __func__, retry + 1);
		ret = fts_writeFwCmd(&cmd, 1);

		if (ret < OK) {
			logError(1, "%s%s:Unable to send Lockdown data ",
				tag, __func__);
			logError(1, "write CMD %08X\n", ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
			continue;
		}

		//start the polling for reading the reply
		ret = pollForEvent(event_to_search, 3,
			&readEvent[0], GENERAL_TIMEOUT);

		if (ret < OK) {
			logError(1, "Cann't find first lockdown code read");
			logError(1, "%s %s:reply event! ERROR %08X\n",
				tag, __func__, ret);
			continue;
		}

		byteToRead = readEvent[1];
		*size = byteToRead;
		logError(0, "%s %s:Lockdown Code size = %d\n",
			tag, __func__, *size);
		lockData = (u8 *)kmalloc_array((byteToRead),
			sizeof(u8), GFP_KERNEL);
		if (lockData == NULL) {
			logError(1, "%s %s:Unable to allocate lockData %08X\n",
				tag, __func__, ERROR_ALLOC);
			ret = (ERROR_ALLOC | ERROR_LOCKDOWN_CODE);
			continue;
		}
		while (byteToRead > 0) {
			if ((readEvent[1] - readEvent[2])
				> LOCKDOWN_CODE_READ_CHUNK) {
				toRead = LOCKDOWN_CODE_READ_CHUNK;
			} else {
				toRead = readEvent[1] - readEvent[2];
			}
			byteToRead -= toRead;
			memcpy(&lockData[readEvent[2]],
				&readEvent[3], toRead);
			event_to_search[2] += toRead;
			if (byteToRead <= 0)
				continue;

			ret = pollForEvent(event_to_search,
				3,
				&readEvent[0],
				GENERAL_TIMEOUT);

			//start polling for reading reply
			if (ret < OK) {
				logError(1, "Can not find lockdow");
				logError(1, "code read reply event ");
				logError(1, "%s%s:offset%02X%08X\n",
					tag, __func__, event_to_search[2], ret);
				ret = (ERROR_ALLOC | ERROR_LOCKDOWN_CODE);
				break;
			}
		}
		if (byteToRead != 0) {
			logError(1, "%s %s:Read Lockdown code FAIL! ",
				tag, __func__);
			logError(1, "ERROR %08X\n", ret);
			continue;
		} else {
			logError(0, "%s %s: Lockdown Code read DONE!\n",
				tag, __func__);
			ret = OK;
			temp = printHex("Lockdown Code = ", lockData, *size);
			if (temp != NULL) {
				logError(0, "%s %s: %s", tag, __func__, temp);
				kfree(temp);
			}
			break;
		}
	}
ERROR:
	//ret = fts_enableInterrupt();
	//ensure that the interrupt are always
	//renabled when exit from funct
	if (fts_enableInterrupt() < OK) {
		logError(1, "%s %s:Error while re-enabling the interrupt!\n",
			tag, __func__);
	}
	return ret;
}

char *printHex(char *label, u8 *buff, int count)
{
	int i, offset;
	char *result = NULL;
	size_t len = 0;

	offset = strlen(label);
	len = (offset + 3 * count) + 2;
	result = (char *)kmalloc_array(len, sizeof(char), GFP_KERNEL);
	if (result != NULL) {
		strlcpy(result, label, len);
		for (i = 0; i < count; i++)
			snprintf(&result[offset + i * 3], 4, "%02X ", buff[i]);
		strlcat(result, "\n", len);
	}
	return result;
}

int pollForEvent(int *event_to_search, int event_bytes,
	u8 *readData, int time_to_wait)
{
	int i, find, retry, count_err;
	int time_to_count;
	int err_handling = OK;
	struct StopWatch clock;

	u8 cmd[1] = { FIFO_CMD_READONE };
	char *temp = NULL;

	find = 0;
	retry = 0;
	count_err = 0;
	time_to_count = time_to_wait / TIMEOUT_RESOLUTION;

	startStopWatch(&clock);
	while (find != 1 && retry < time_to_count
		&& fts_readCmd(cmd, 1, readData, FIFO_EVENT_SIZE) >= 0) {

		if (readData[0] == EVENTID_ERROR_EVENT) {
			temp = printHex("ERROR EVENT = ",
				readData, FIFO_EVENT_SIZE);
			if (temp != NULL)
				logError(0, "%s %s", tag, temp);
			kfree(temp);
			count_err++;
			err_handling = errorHandler(readData, FIFO_EVENT_SIZE);
			if ((err_handling & 0xF0FF0000)
				== ERROR_HANDLER_STOP_PROC) {
				logError(1, "%s %s: forced to be stopped! ",
					tag, __func__);
				logError(1, "ERROR %08X\n", err_handling);
				return err_handling;
			}
		} else {
			if (readData[0] != EVENTID_NO_EVENT) {
				temp = printHex("READ EVENT = ",
					readData, FIFO_EVENT_SIZE);
				if (temp != NULL)
					logError(0, "%s %s", tag, temp);
				kfree(temp);
			}
			if (readData[0] == EVENTID_CONTROL_READY &&
				event_to_search[0] != EVENTID_CONTROL_READY) {
				logError(0, "Unmanned Controller Ready Event!");
				logError(0, "%s %s:Setting reset flags...\n",
					tag, __func__);
				setSystemResettedUp(1);
				setSystemResettedDown(1);
			}
		}
		find = 1;

		for (i = 0; i < event_bytes; i++) {
			if (event_to_search[i] != -1
				&& (int)readData[i] != event_to_search[i]) {
				find = 0;
				break;
			}
		}

		retry++;
		msleep(TIMEOUT_RESOLUTION);
	}
	stopStopWatch(&clock);
	if ((retry >= time_to_count) && find != 1) {
		logError(0, "%s %s: ERROR %02X\n",
			tag, __func__,  ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}
	if (find == 1) {
		temp = printHex("FOUND EVENT = ", readData, FIFO_EVENT_SIZE);
		if (temp != NULL)
			logError(0, "%s %s", tag, temp);
		kfree(temp);
		logError(0, "%s Event found in %d ms (%d iterations)!\n",
			tag, elapsedMillisecond(&clock), retry);
		logError(0, "Number of errors found = %d\n", count_err);
		return count_err;
	}
	logError(0, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);
	return ERROR_I2C_R;
}

int flushFIFO(void)
{
	u8 cmd = FIFO_CMD_FLUSH;

	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s FIFO flushed!\n", tag);
	return OK;
}

int fts_disableInterrupt(void)
{
	//disable interrupt
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_DISABLE };

	u16ToU8_be(IER_ADDR, &cmd[1]);

	if (fts_writeCmd(cmd, 4) < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	logError(0, "%s Interrupt Disabled!\n", tag);
	return OK;
}


int fts_enableInterrupt(void)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_ENABLE };
	int ret = 0;

	u16ToU8_be(IER_ADDR, &cmd[1]);
	ret = fts_writeCmd(cmd, 4);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %d\n", tag, __func__, ret);
		return ret;
	}
	logError(0, "%s Interrupt Enabled!\n", tag);
	return OK;
}

int u8ToU16n(u8 *src, int src_length, u16 *dst)
{
	int i, j;
	u16 *buf;

	if (src_length % 2 != 0)
		return -EINVAL;

	j = 0;
	buf = (u16 *)kmalloc_array((src_length / 2), sizeof(u16), GFP_KERNEL);
	if (!buf) {
		dst = NULL;
		return -EINVAL;
	}
	dst = buf;
	for (i = 0; i < src_length; i += 2) {
		dst[j] = ((src[i+1] & 0x00FF) << 8) + (src[i] & 0x00FF);
		j++;
	}

	return (src_length / 2);
}

int u8ToU16(u8 *src, u16 *dst)
{
	*dst = (u16)(((src[1] & 0x00FF) << 8) + (src[0] & 0x00FF));
	return 0;
}

int u8ToU16_le(u8 *src, u16 *dst)
{
	*dst = (u16)(((src[0] & 0x00FF) << 8) + (src[1] & 0x00FF));
	return 0;
}

int u16ToU8n(u16 *src, int src_length, u8 *dst)
{
	int i, j;
	u8 *buf = (u8 *)kmalloc_array(2 * src_length, sizeof(u8), GFP_KERNEL);

	if (!buf) {
		dst = NULL;
		return -EINVAL;
	}
	dst = buf;
	j = 0;
	for (i = 0; i < src_length; i++) {
		dst[j] = (u8) (src[i] & 0xFF00) >> 8;
		dst[j+1] = (u8) (src[i] & 0x00FF);
		j += 2;
	}

	return src_length * 2;
}

int u16ToU8(u16 src, u8 *dst)
{
	dst[0] = (u8)((src & 0xFF00) >> 8);
	dst[1] = (u8)(src & 0x00FF);
	return 0;
}

int u16ToU8_be(u16 src, u8 *dst)
{
	dst[0] = (u8)((src & 0xFF00) >> 8);
	dst[1] = (u8)(src & 0x00FF);
	return 0;
}

int u16ToU8_le(u16 src, u8 *dst)
{
	dst[1] = (u8)((src & 0xFF00) >> 8);
	dst[0] = (u8)(src & 0x00FF);
	return 0;
}

int u8ToU32(u8 *src, u32 *dst)
{
	*dst = (u32)(((src[3] & 0xFF) << 24) + ((src[2] & 0xFF) << 16)
		+ ((src[1] & 0xFF) << 8) + (src[0] & 0xFF));
	return 0;
}

int u32ToU8(u32 src, u8 *dst)
{
	dst[3] = (u8)((src & 0xFF000000) >> 24);
	dst[2] = (u8)((src & 0x00FF0000) >> 16);
	dst[1] = (u8)((src & 0x0000FF00) >> 8);
	dst[0] = (u8)(src & 0x000000FF);
	return 0;
}

int attempt_function(int(*code)(void), unsigned long wait_before_retry,
	int retry_count)
{
	int result;
	int count = 0;

	do {
		result = code();
		count++;
		msleep(wait_before_retry);
	} while (count < retry_count && result < 0);

	if (count == retry_count)
		result |= ERROR_TIMEOUT;

	return result;
}

void setResetGpio(int gpio)
{
	reset_gpio = gpio;
	logError(0, "%s %s: reset_gpio = %d\n", tag, __func__, reset_gpio);
}

int fts_system_reset(void)
{
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search;
	int res = -1;
	int i;
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, SYSTEM_RESET_VALUE };

	event_to_search = (int)EVENTID_CONTROL_READY;

	u16ToU8_be(SYSTEM_RESET_ADDRESS, &cmd[1]);
	logError(0, "%s System resetting...\n", tag);
	for (i = 0; i < SYSTEM_RESET_RETRY && res < 0; i++) {
		if (reset_gpio == GPIO_NOT_DEFINED) {
#ifndef FTM3_CHIP
			res |= fts_warm_boot();
#endif
			res = fts_writeCmd(cmd, 4);
		} else {
			gpio_set_value(reset_gpio, 0);
			msleep(20);
			gpio_set_value(reset_gpio, 1);
			res = OK;
		}
		if (res < OK) {
			logError(1, "%s %s:ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
		} else {
			res = pollForEvent(&event_to_search, 1,
					readData, GENERAL_TIMEOUT);
			if (res < OK) {
				logError(0, "%s %s: ERROR %02X\n",
					tag, __func__, res);
			}
		}
	}
	if (res < OK) {
		logError(1, "%s %s:failed after 3 attempts: ERROR %02X\n",
			tag, __func__, (res | ERROR_SYSTEM_RESET_FAIL));
		res = (res | ERROR_SYSTEM_RESET_FAIL);
	} else {
		logError(0, "%s System reset DONE!\n", tag);
		system_resetted_down = 1;
		system_resetted_up = 1;
		res = OK;
	}
	return res;
}

int isSystemResettedDown(void)
{
	return system_resetted_down;
}

int isSystemResettedUp(void)
{
	return system_resetted_up;
}

void setSystemResettedDown(int val)
{
	system_resetted_down = val;
}

void setSystemResettedUp(int val)
{
	system_resetted_up = val;
}

int senseOn(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_MT_SENSE_ON };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s:ERROR %02X\n",
			tag, __func__, ERROR_SENSE_ON_FAIL);
		return (ret|ERROR_SENSE_ON_FAIL);
	}
	logError(0, "%s %s: SENSE ON\n", tag, __func__);
	return OK;
}

int senseOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_MT_SENSE_OFF };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s:ERROR %02X\n",
			tag, __func__, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}
	logError(0, "%s %s: SENSE OFF\n", tag, __func__);
	return OK;
}

int keyOn(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_ON };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s:ERROR %02X\n",
			tag, __func__, ERROR_SENSE_ON_FAIL);
		return (ret | ERROR_SENSE_ON_FAIL);
	}

	logError(0, "%s %s: KEY ON\n", tag, __func__);
	return OK;
}

int keyOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_OFF };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s %s:ERROR %02X\n",
			tag, __func__, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}

	logError(0, "%s %s: KEY OFF\n", tag, __func__);
	return OK;
}

int cleanUp(int enableTouch)
{
	int res;

	logError(0, "%s %s: system reset...\n", tag, __func__);
	res = fts_system_reset();
	if (res < OK)
		return res;

	if (enableTouch) {
		logError(0, "%s %s:enabling touches...\n", tag, __func__);
		res = senseOn();
		if (res < OK)
			return res;
#ifdef PHONE_KEY
		res = keyOn();
		if (res < OK)
			return res;
#endif
		logError(0, "%s %s:enabling interrupts...\n", tag, __func__);
		res = fts_enableInterrupt();
		if (res < OK)
			return res;
	}
	return OK;
}

int checkEcho(u8 *cmd, int size)
{
	int ret, i;
	int event_to_search[FIFO_EVENT_SIZE + 1];
	u8 readData[FIFO_EVENT_SIZE];

	if ((ftsInfo.u32_echoEn & 0x00000001) != ECHO_ENABLED) {
		logError(0, "%s ECHO Not Enabled!\n", tag);
		return OK;
	}
	if (size < 1) {
		logError(1, "%s:Error Size = %d not valid!", tag, size);
		logError(1, " or ECHO not Enabled!%08X\n", ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
	if ((size + 2) > FIFO_EVENT_SIZE)
		size = FIFO_EVENT_SIZE - 2;
	//Echo event EC xx xx xx xx xx xx
	//fifo_status therefore for command
	//with more than 6 bytes will echo only the first 6
	event_to_search[0] = EVENTID_ECHO;
	for (i = 1; i <= size; i++)
		event_to_search[i] = cmd[i - 1];
	ret = pollForEvent(event_to_search, size + 1,
				readData, GENERAL_TIMEOUT);
	if (ret < OK) {
		logError(1, "%s %s:Echo Event not found! ERROR %02X\n",
			tag, __func__, ret);
		return (ret | ERROR_CHECK_ECHO_FAIL);
	}

	logError(0, "%s ECHO OK!\n", tag);
	ret = OK;
	return ret;
}

int featureEnableDisable(int on_off, u32 feature)
{
	int ret;
	u8 cmd[5];

	if (on_off == FEAT_ENABLE) {
		cmd[0] = FTS_CMD_FEATURE_ENABLE;
		logError(0, "%s %s: Enabling feature %08X ...\n",
			tag, __func__, feature);
	} else {
		cmd[0] = FTS_CMD_FEATURE_DISABLE;
		logError(0, "%s %s: Disabling feature %08X ...\n",
			tag, __func__, feature);
	}
	u32ToU8(feature, &cmd[1]);

	//not use writeFwCmd because this function can be
	//called also during interrupt enable and should be fast
	ret = fts_writeCmd(cmd, 5);
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		return (ret | ERROR_FEATURE_ENABLE_DISABLE);
	}

	logError(0, "%s %s: DONE!\n", tag, __func__);
	return OK;
}

int writeNoiseParameters(u8 *noise)
{
	int ret, i;
	u8 cmd[2+NOISE_PARAMETERS_SIZE];
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[2] = {EVENTID_NOISE_WRITE, NOISE_PARAMETERS};

	logError(0, "%s %s: Writing noise parameters to the IC ...\n",
		tag, __func__);
	ret = fts_disableInterrupt();
	if (ret < OK) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}
	cmd[0] = FTS_CMD_NOISE_WRITE;
	cmd[1] = NOISE_PARAMETERS;
	logError(0, "%s %s: Noise parameters = ", tag, __func__);
	for (i = 0; i < NOISE_PARAMETERS_SIZE; i++) {
		cmd[2 + i] = noise[i];
		logError(0, "%02X", cmd[2 + i]);
	}

	logError(0, "\n");
	ret = fts_writeCmd(cmd, NOISE_PARAMETERS_SIZE + 2);
	//not use writeFwCmd because this function should be fast
	if (ret < OK) {
		logError(0, "%s %s:impossible write command... ERROR %02X\n",
			tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}

	ret = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);
	if (ret < OK) {
		logError(0, "%s %s: polling FIFO ERROR %02X\n",
			tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}

	if (readData[2] != 0x00) {
		logError(1, "%s %s:Event check FAIL! %02X != 0x00 ERROR%02X\n",
			tag, __func__, readData[2], ERROR_NOISE_PARAMETERS);
		ret = ERROR_NOISE_PARAMETERS;
			goto ERROR;
	}

	logError(0, "%s %s:DONE!\n", tag, __func__);
	ret = OK;
ERROR:
	ret = fts_enableInterrupt();
	//ensure that the interrupt are always renabled when exit from funct
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		return (ret | ERROR_NOISE_PARAMETERS);
	}
	return ret;
}

int readNoiseParameters(u8 *noise)
{
	int ret, i;
	u8 cmd[2];
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[2] = {EVENTID_NOISE_READ, NOISE_PARAMETERS};

	logError(0, "%s %s:Reading noise parameters from the IC ...\n",
		tag, __func__);
	ret = fts_disableInterrupt();
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}
	cmd[0] = FTS_CMD_NOISE_READ;
	cmd[1] = NOISE_PARAMETERS;
	ret = fts_writeCmd(cmd, 2);//not use writeFwCmd should be fast
	if (ret < OK) {
		logError(0, "%s %s:impossible write command... ERROR %02X\n",
			tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}

	ret = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);
	if (ret < OK) {
		logError(0, "%s %s: polling FIFO ERROR %02X\n",
			tag, __func__, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
			goto ERROR;
	}

	logError(0, "%s %s: Noise parameters = ", tag, __func__);
	for (i = 0; i < NOISE_PARAMETERS_SIZE; i++) {
		noise[i] = readData[2 + i];
		logError(0, "%02X ", noise[i]);
	}

	logError(0, "\n");
	logError(0, "%s %s: DONE!\n", tag, __func__);
	ret = OK;
ERROR:
	ret = fts_enableInterrupt();
	//ensure that the interrupt are always renabled when exit from funct
	if (ret < OK) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ret);
		return (ret | ERROR_NOISE_PARAMETERS);
	}
	return ret;
}

short **array1dTo2d_short(short *data, int size, int columns)
{
	int i;
	int count = size / columns;
	short **matrix = (short **)kmalloc_array(count,
				sizeof(short *), GFP_KERNEL);
	if (matrix != NULL) {
		for (i = 0; i < count; i++) {
			matrix[i] = (short *)kmalloc_array(columns,
				sizeof(short), GFP_KERNEL);
		}

		for (i = 0; i < size; i++)
			matrix[i / columns][i % columns] = data[i];
	}

	return matrix;
}

u8 **array1dTo2d_u8(u8 *data, int size, int columns)
{
	int i;
	int count = size / columns;
	u8 **matrix = (u8 **)kmalloc_array(count,
				sizeof(u8 *), GFP_KERNEL);
	if (matrix != NULL) {
		for (i = 0; i < count; i++) {
			matrix[i] = (u8 *)kmalloc_array(columns,
				sizeof(u8), GFP_KERNEL);
		}

		for (i = 0; i < size; i++)
			matrix[i / columns][i % columns] = data[i];
	}

	return matrix;
}

void print_frame_short(char *label, short **matrix, int row, int column)
{
	int i, j;

	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++)
			logError(0, "%d", matrix[i][j]);
		logError(0, "\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

void print_frame_u8(char *label, u8 **matrix, int row, int column)
{
	int i, j;

	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++)
			logError(0, "%d ", matrix[i][j]);
		logError(0, "\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

void print_frame_u32(char *label, u32 **matrix, int row, int column)
{
	int i, j;

	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++)
			logError(0, "%d ", matrix[i][j]);
		logError(0, "\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

void print_frame_int(char *label, int **matrix, int row, int column)
{
	int i, j;

	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++)
			logError(0, "%d ", matrix[i][j]);
		logError(0, "\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}
