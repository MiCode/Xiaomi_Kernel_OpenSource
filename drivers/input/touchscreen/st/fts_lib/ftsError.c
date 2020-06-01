// SPDX-License-Identifier: GPL-2.0-only
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
 *                  FTS error/info kernel log reporting                   *
 *                                                                        *
 **************************************************************************
 **************************************************************************
 */


#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/completion.h>
//#include <linux/wakelock.h>
#include <linux/pm_wakeup.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "../fts.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsIO.h"
#include "ftsTool.h"
#include "ftsCompensation.h"

static char tag[8] = "[ FTS ]\0";


void logError(int force, const char *msg, ...)
{

	if (force == 1
#ifdef DEBUG
		|| 1
#endif
	) {
		va_list args;

		va_start(args, msg);
		vprintk(msg, args);
		va_end(args);
	}
}

int isI2cError(int error)
{
	if (((error & 0x000000FF) >= (ERROR_I2C_R & 0x000000FF))
		&& ((error & 0x000000FF) <= (ERROR_I2C_O & 0x000000FF)))
		return 1;
	else
		return 0;
}

int dumpErrorInfo(void)
{
	int ret, i;
	u8 data[ERROR_INFO_SIZE] = {0};
	u32 sign = 0;

	logError(0, "%s %s: Starting dump of error info...\n", tag, __func__);
	if (ftsInfo.u16_errOffset == INVALID_ERROR_OFFS)  {
		logError(1, "%s %s: Invalid error offset ERROR %02X\n",
			tag, __func__, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = readCmdU16(FTS_CMD_FRAMEBUFFER_R, ftsInfo.u16_errOffset,
		data, ERROR_INFO_SIZE, DUMMY_FRAMEBUFFER);
	if (ret < OK) {
		logError(1, "%s %s: reading data ERROR %02X\n",
			tag,  __func__, ret);
		return ret;
	}
	logError(0, "%s %s: Error Info =\n", tag, __func__);
	u8ToU32(data, &sign);
	if (sign != ERROR_SIGNATURE)
		logError(1, "%s %s:Wrong Signature! Data may be invalid!\n",
			tag, __func__);
	else
		logError(1, "%s %s: Error Signature OK! Data are valid!\n",
			tag, __func__);

	for (i = 0; i < ERROR_INFO_SIZE; i++) {
		if (i % 4 == 0)
			logError(1, KERN_ERR "\n%s %s: %d) ",
				tag, __func__, i / 4);
		logError(1, "%02X ", data[i]);
	}
	logError(1, "\n");

	logError(0, "%s %s: dump of error info FINISHED!\n", tag, __func__);
	return OK;

}

int errorHandler(u8 *event, int size)
{
	int res = OK;
	struct fts_ts_info *info = NULL;

	if (getClient() != NULL)
		info = i2c_get_clientdata(getClient());

	if (info == NULL || event == NULL || size <= 1 || event[0] !=
		EVENTID_ERROR_EVENT) {
		logError(1, "%s %s: event Null or not correct size! ",
			tag, __func__,  ERROR_OP_NOT_ALLOW);
		logError(1, "ERROR %08X\n", ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	logError(0, "%s %s: Starting handling...\n", tag, __func__);
	//TODO: write an error log for undefinied command subtype 0xBA
	switch (event[1]) {
	case EVENT_TYPE_ESD_ERROR:	//esd
		res = fts_chip_powercycle(info);
		if (res < OK) {
			logError(1, "%s %s: ", tag, res);
			logError(1, "Error performing powercycle ERROR %08X\n");
		}

		res = fts_system_reset();
		if (res < OK)
			logError(1, "%s %s:Cannot reset device ERROR%08X\n",
				tag, __func__, res);
		res = (ERROR_HANDLER_STOP_PROC | res);
		break;

	case EVENT_TYPE_WATCHDOG_ERROR:	//watchdog
		dumpErrorInfo();
		res = fts_system_reset();
		if (res < OK)
			logError(1, "%s %s:Cannot reset device:ERROR%08X\n",
				tag, __func__, res);
		res = (ERROR_HANDLER_STOP_PROC | res);
		break;
	case EVENT_TYPE_CHECKSUM_ERROR: //CRC ERRORS
		switch (event[2]) {
		case CRC_CONFIG_SIGNATURE:
			logError(1, "%s %s: Config Signature ERROR!\n",
				tag, __func__);
			break;
		case CRC_CONFIG:
			logError(1, "%s %s:Config CRC ERROR!\n", tag, __func__);
			break;
		case CRC_CX_MEMORY:
			logError(1, "%s %s: CX CRC ERROR!\n", tag, __func__);
			break;
		}
		break;
	case EVENT_TYPE_LOCKDOWN_ERROR:
		//res = (ERROR_HANDLER_STOP_PROC|res);
		//stop lockdown code routines in order to retry
		switch (event[2]) {
		case 0x01:
			logError(1, "%s %s:Lockdown code alredy ",
				tag, __func__);
			logError(1, "written into the IC!\n");
			break;
		case 0x02:
			logError(1, "%s %s:Lockdown CRC ", tag, __func__);
			logError(1, "check fail during a WRITE!\n");
			break;

		case 0x03:
			logError(1,
				"%s %s:Lockdown WRITE command format wrong!\n",
				tag, __func__);
			break;
		case 0x04:
			pr_err("Lockdown Memory Corrupted!\n");
			logError(1, "%s %s:Please contact ST for support!\n",
				tag, __func__);
			break;
		case 0x11:
			logError(1,
				"%s %s:NO Lockdown code to READ into the IC!\n",
				tag, __func__);
			break;
		case 0x12:
			logError(1,
				"%s %s:Lockdown code data corrupted\n",
				tag, __func__);
			break;
		case 0x13:
			logError(1,
				"%s %s:Lockdown READ command format wrong!\n",
				tag, __func__);
			break;
		case 0x21:
			pr_err("Exceeded maximum number of\n");
			logError(1,
				"%s %s:Lockdown code REWRITE into IC!\n",
				tag, __func__);
			break;
		case 0x22:
			logError(1, "%s %s:Lockdown CRC check", tag, __func__);
			logError(1, " fail during a REWRITE!\n");
			break;
		case 0x23:
			logError(1, "%s %s:", tag, __func__);
			logError(1, "Lockdown REWRITE command format wrong!\n");
			break;
		case 0x24:
			pr_err("Lockdown Memory Corrupted!\n");
			logError(1, "%s %s:Please contact ST for support!\n",
				tag, __func__);
			break;
		default:
			logError(1, "%s %s:No valid error type for LOCKDOWN!\n",
				tag, __func__);
		}
		break;

	default:
		logError(0,  "%s %s: No Action taken!\n", tag, __func__);
	break;
	}
	logError(0, "%s %s: handling Finished! res = %08X\n",
		tag, __func__, res);
	return res;
}

