/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                  FTS error/info kernel log reporting					 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsError.c
* \brief Contains all the function which handle with Error conditions
*/

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../fts.h"
#include "ftsCore.h"
#include "ftsError.h"
#include "ftsIO.h"
#include "ftsTool.h"
#include "ftsCompensation.h"

static ErrorList errors;

/**
* Print messages in the kernel log
* @param force if 1, the log is printed always otherwise only if DEBUG is defined, the log will be printed
* @param msg string containing the message to print
* @param ... additional parameters that are used in msg according the format of printf
*/
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

/**
* Check if an error code is related to an I2C failure
* @param error error code to check
* @return 1 if the first level error code is I2C related otherwise 0
*/
int isI2cError(int error)
{
	if (((error & 0x000000FF) >= (ERROR_BUS_R & 0x000000FF)) &&
	    ((error & 0x000000FF) <= (ERROR_BUS_O & 0x000000FF)))
		return 1;
	else
		return 0;
}

/**
 * Dump in the kernel log some debug info in case of FW hang
 * @param outBuf (optional)pointer to bytes array where to copy the debug info, if NULL the data will just printed on the kernel log
 * @param size dimension in bytes of outBuf, if > ERROR_DUMP_ROW_SIZE*ERROR_DUMP_COL_SIZE, only the first ERROR_DUMP_ROW_SIZE*ERROR_DUMP_COL_SIZE bytes will be copied
 * @return OK if success or an error code which specify the type of error encountered
 */
int dumpErrorInfo(u8 *outBuf, int size)
{
	int ret, i;
	u8 data[ERROR_DUMP_ROW_SIZE * ERROR_DUMP_COL_SIZE] = { 0 };
	u32 sign = 0;

	logError(0, "%s %s: Starting dump of error info...\n", tag, __func__);

	ret =
	    fts_writeReadU8UX(FTS_CMD_FRAMEBUFFER_R, BITS_16, ADDR_ERROR_DUMP,
			      data, ERROR_DUMP_ROW_SIZE * ERROR_DUMP_COL_SIZE,
			      DUMMY_FRAMEBUFFER);
	if (ret < OK) {
		logError(1, "%s %s: reading data ERROR %08X\n", tag, __func__,
			 ret);
		return ret;
	} else {
		if (outBuf != NULL) {
			sign =
			    size >
			    ERROR_DUMP_ROW_SIZE *
			    ERROR_DUMP_COL_SIZE ? ERROR_DUMP_ROW_SIZE *
			    ERROR_DUMP_COL_SIZE : size;
			memcpy(outBuf, data, sign);
			logError(0,
				 "%s %s: error info copied in the buffer! \n",
				 tag, __func__);
		}
		logError(1, "%s %s: Error Info = \n", tag, __func__);
		u8ToU32(data, &sign);
		if (sign != ERROR_DUMP_SIGNATURE)
			logError(1,
				 "%s %s: Wrong Error Signature! Data may be invalid! \n",
				 tag, __func__);
		else
			logError(1,
				 "%s %s: Error Signature OK! Data are valid! \n",
				 tag, __func__);

		for (i = 0; i < ERROR_DUMP_ROW_SIZE * ERROR_DUMP_COL_SIZE; i++) {
			if (i % ERROR_DUMP_COL_SIZE == 0) {
				logError(1, KERN_ERR "\n%s %s: %d) ", tag,
					 __func__, i / ERROR_DUMP_COL_SIZE);
			}
			logError(1, "%02X ", data[i]);
		}
		logError(1, "\n");

		logError(0, "%s %s: dump of error info FINISHED!\n", tag,
			 __func__);
		return OK;
	}

}

/**
* Implement recovery strategies to be used when an error event is found while polling the FIFO
* @param event error event found during the polling
* @param size size of event
* @return OK if the error event doesn't require any action or the recovery strategy doesn't have any impact in the possible procedure that trigger the error, otherwise return an error code which specify the kind of error encountered. If ERROR_HANDLER_STOP_PROC the calling function must stop!
*/
int errorHandler(u8 *event, int size)
{
	int res = OK;
	struct fts_ts_info *info = NULL;

	if (getDev() != NULL)
		info = dev_get_drvdata(getDev());

	if (info != NULL && event != NULL && size > 1
	    && event[0] == EVT_ID_ERROR) {
		logError(1, "%s errorHandler: Starting handling...\n", tag);
		addErrorIntoList(event, size);
		switch (event[1]) {
		case EVT_TYPE_ERROR_ESD:
			res = fts_chip_powercycle(info);
			if (res < OK) {
				logError(1,
					 "%s errorHandler: Error performing powercycle ERROR %08X\n",
					 tag, res);
			}

			res = fts_system_reset();
			if (res < OK) {
				logError(1,
					 "%s errorHandler: Cannot reset the device ERROR %08X\n",
					 tag, res);
			}
			res = (ERROR_HANDLER_STOP_PROC | res);
			break;

		case EVT_TYPE_ERROR_WATCHDOG:
			dumpErrorInfo(NULL, 0);
			res = fts_system_reset();
			if (res < OK) {
				logError(1,
					 "%s errorHandler: Cannot reset the device ERROR %08X\n",
					 tag, res);
			}
			res = (ERROR_HANDLER_STOP_PROC | res);
			break;

		case EVT_TYPE_ERROR_ITO_FORCETOGND:
			logError(1, "%s errorHandler: Force Short to GND!\n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_SENSETOGND:
			logError(1, "%s errorHandler: Sense short to GND! \n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_FORCETOVDD:
			logError(1, "%s errorHandler: Force short to VDD!\n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_SENSETOVDD:
			logError(1, "%s errorHandler: Sense short to VDD!\n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_FORCE_P2P:
			logError(1,
				 "%s errorHandler: Force Pin to Pin Short!\n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_SENSE_P2P:
			logError(1,
				 "%s errorHandler: Sense Pin to Pin Short!\n",
				 tag);
			break;
		case EVT_TYPE_ERROR_ITO_FORCEOPEN:
			logError(1, "%s errorHandler: Force Open !\n", tag);
			break;
		case EVT_TYPE_ERROR_ITO_SENSEOPEN:
			logError(1, "%s errorHandler: Sense Open !\n", tag);
			break;
		case EVT_TYPE_ERROR_ITO_KEYOPEN:
			logError(1, "%s errorHandler: Key Open !\n", tag);
			break;

		default:
			logError(1, "%s errorHandler: No Action taken! \n",
				 tag);
			break;

		}
		logError(1, "%s errorHandler: handling Finished! res = %08X\n",
			 tag, res);
		return res;
	} else {
		logError(1,
			 "%s errorHandler: event Null or not correct size! ERROR %08X \n",
			 tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

}

/**
* Add an error event into the Error List
* @param event error event to add
* @param size size of event
* @return OK
*/
int addErrorIntoList(u8 *event, int size)
{
	int i = 0;

	logError(0, "%s Adding error in to ErrorList... \n", tag);

	memcpy(&errors.list[errors.last_index * FIFO_EVENT_SIZE], event, size);
	i = FIFO_EVENT_SIZE - size;
	if (i > 0) {
		logError(0,
			 "%s Filling last %d bytes of the event with zero...\n",
			 tag, i);
		memset(&errors.list[errors.last_index * FIFO_EVENT_SIZE + size],
		       0, i);
	}
	logError(0, "%s Adding error in to ErrorList... FINISHED!\n", tag);

	errors.count += 1;
	if (errors.count > FIFO_DEPTH)
		logError(1,
			 "%s ErrorList is going in overflow... the first %d event(s) were override!\n",
			 tag, errors.count - FIFO_DEPTH);
	errors.last_index = (errors.last_index + 1) % FIFO_DEPTH;

	return OK;
}

/**
* Reset the Error List setting the count and last_index to 0.
* @return OK
*/
int resetErrorList(void)
{
	errors.count = 0;
	errors.last_index = 0;
	memset(errors.list, 0, FIFO_DEPTH * FIFO_EVENT_SIZE);
	return OK;
}

/**
* Get the number of error events copied into the Error List
* @return the number of error events into the Error List
*/
int getErrorListCount(void)
{
	if (errors.count > FIFO_DEPTH)
		return FIFO_DEPTH;
	else
		return errors.count;
}


/**
* Scroll the Error List looking for the event specified
* @param event_to_search event_to_search pointer to an array of int where each element correspond to a byte of the event to find. If the element of the array has value -1, the byte of the event, in the same position of the element is ignored.
* @param event_bytes size of event_to_search
* @return a value >=0 if the event is found which represent the index of the Error List where the event is located otherwise an error code
*/
int pollErrorList(int *event_to_search, int event_bytes)
{
	int i = 0, j = 0, find = 0;
	int count = getErrorListCount();

	logError(1, "%s Starting to poll ErrorList... \n", tag);
	while (find != 1 && i < count) {
		find = 1;
		for (j = 0; j < event_bytes; j++) {

			if (event_to_search[i] != -1
			    && (int)errors.list[i * FIFO_EVENT_SIZE + j] !=
			    event_to_search[i]) {
				find = 0;
				break;
			}
		}
		i++;
	}
	if (find == 1) {
		logError(1, "%s Error Found into ErrorList! \n", tag);
		return i - 1;
	} else {
		logError(1, "%s Error Not Found into ErrorList! ERROR %08X \n",
			 tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}
}

/**
* Poll the Error List looking for any error types passed in the arguments. Return at the first match!
* @param list pointer to a list of error types to look for
* @param size size of list
* @return error type found if success or ERROR_TIMEOUT
*/
int pollForErrorType(u8 *list, int size)
{
	int i = 0, j = 0, find = 0;
	int count = getErrorListCount();

	logError(1, "%s %s: Starting to poll ErrorList... count = %d \n", tag,
		 __func__, count);
	while (find != 1 && i < count) {
		for (j = 0; j < size; j++) {
			if (list[j] == errors.list[i * FIFO_EVENT_SIZE + 1]) {
				find = 1;
				break;
			}
		}
		i++;
	}
	if (find == 1) {
		logError(1, "%s %s: Error Type %02X into ErrorList! \n", tag,
			 __func__, list[j]);
		return list[j];
	} else {
		logError(1,
			 "%s %s: Error Type Not Found into ErrorList! ERROR %08X \n",
			 tag, __func__, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}
}
