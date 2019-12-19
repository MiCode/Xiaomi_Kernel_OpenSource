/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                  FTS error/info kernel log reporting			 *
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

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "../fts.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsIO.h"
#include "ftsTool.h"
#include "ftsCompensation.h"

extern chipInfo ftsInfo;




int isI2cError(int error)
{
	if (((error & 0x000000FF) >= (ERROR_I2C_R & 0x000000FF)) && ((error & 0x000000FF) <= (ERROR_I2C_O & 0x000000FF)))
		return 1;
	else
		return 0;
}

int dumpErrorInfo(void)
{
	int ret, i;
	u8 data[ERROR_INFO_SIZE] = {0};
	u32 sign = 0;
	log_debug("%s %s: Starting dump of error info...\n", tag, __func__);
	if (ftsInfo.u16_errOffset != INVALID_ERROR_OFFS) {
		ret = readCmdU16(FTS_CMD_FRAMEBUFFER_R, ftsInfo.u16_errOffset, data, ERROR_INFO_SIZE, DUMMY_FRAMEBUFFER);
		if (ret < OK) {
			log_error("%s %s: reading data ERROR %02X\n", tag, __func__, ret);
			return ret;
		} else {
			log_error("%s %s: Error Info = \n", tag, __func__);
			u8ToU32(data, &sign);
			if (sign != ERROR_SIGNATURE)
				log_error("%s %s: Wrong Error Signature! Data may be invalid! \n", tag, __func__);
			else
				log_error("%s %s: Error Signature OK! Data are valid! \n", tag, __func__);

			for (i = 0; i < ERROR_INFO_SIZE; i++) {
				if (i % 4 == 0) {
					log_error("%s %s: %d)", tag, __func__, i / 4);
				}
				log_error("%s %02X ", tag, data[i]);
			}
			log_error("\n");

			log_debug("%s %s: dump of error info FINISHED!\n", tag, __func__);
			return OK;
		}
	} else {
		log_error("%s %s: Invalid error offset ERROR %02X\n", tag, __func__, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

int errorHandler(u8 *event, int size)
{
	int res = OK;
	struct fts_ts_info *info = NULL;

	if (getClient() != NULL)
		info = i2c_get_clientdata(getClient());

	if (info != NULL && event != NULL && size > 1 && event[0] == EVENTID_ERROR_EVENT) {
		log_error("%s errorHandler: Starting handling...\n", tag);

		switch (event[1]) {
		case EVENT_TYPE_ESD_ERROR:
			res = fts_chip_powercycle(info);

			if (res < OK)
				log_error("%s errorHandler: Error performing powercycle ERROR %08X\n", tag, res);

			res = fts_system_reset();

			if (res < OK)
				log_error("%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);

			res = (ERROR_HANDLER_STOP_PROC | res);
			break;

		case EVENT_TYPE_WATCHDOG_ERROR:
			dumpErrorInfo();
			res = fts_system_reset();
			if (res < OK)
				log_error("%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);

			res = (ERROR_HANDLER_STOP_PROC | res);
			break;
		case EVENT_TYPE_CHECKSUM_ERROR:
			switch (event[2]) {
			case CRC_CONFIG_SIGNATURE:
				log_error("%s errorHandler: Config Signature ERROR !\n", tag);
				break;

			case CRC_CONFIG:
				log_error("%s errorHandler: Config CRC ERROR !\n", tag);
				break;

			case CRC_CX_MEMORY:
				log_error("%s errorHandler: CX CRC ERROR !\n", tag);
				break;
			}

			break;

		default:
			log_error("%s errorHandler: No Action taken!\n", tag);
			break;
		}

		log_error("%s errorHandler: handling Finished! res = %08X\n", tag, res);
		return res;
	} else {
		log_error("%s errorHandler: event Null or not correct size! ERROR %08X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}
