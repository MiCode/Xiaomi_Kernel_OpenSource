/*
***************************************************************************
*						STMicroelectronics
**************************************************************************
*						marco.cali@st.com
**************************************************************************
*
*				  FTS error/info kernel log reporting
*
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

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "../fts.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsIO.h"
#include "ftsTool.h"

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
	if (((error & 0x000000FF) >= (ERROR_I2C_R & 0x000000FF)) && ((error & 0x000000FF) <= (ERROR_I2C_O & 0x000000FF)))
		return 1;
	else
		return 0;
}

int errorHandler(u8 *event, int size)
{
	int res = OK;
	struct fts_ts_info *info = NULL;

	if (getClient() != NULL)
		info = i2c_get_clientdata(getClient());

	if (info != NULL && event != NULL && size > 1 && event[0] == EVENTID_ERROR_EVENT) {
		logError(1, "%s errorHandler: Starting handling...\n", tag);
		switch (event[1])
		/* TODO: write an error log for undefinied command subtype 0xBA*/
		{
		case EVENT_TYPE_ESD_ERROR:	/* esd */
		res = fts_chip_powercycle(info);
		if (res < OK) {
			logError(1, "%s errorHandler: Error performing powercycle ERROR %08X\n", tag, res);
		}

		res = fts_system_reset();
		if (res < OK) {
			logError(1, "%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);
		}
		res = (ERROR_HANDLER_STOP_PROC|res);
		break;

		case EVENT_TYPE_WATCHDOG_ERROR:	/* watchdog */
		res = fts_system_reset();
			if (res < OK) {
				logError(1, "%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);
			}
		res = (ERROR_HANDLER_STOP_PROC|res);
		break;

		default:
			logError(1, "%s errorHandler: No Action taken! \n", tag);
		break;

		}
		logError(1, "%s errorHandler: handling Finished! res = %08X\n", tag, res);
		return res;
	}
		logError(1, "%s errorHandler: event Null or not correct size! ERROR %08X \n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
}
