/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*					 FTS Utility Functions				 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsCompensation.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTime.h"
#include "ftsTool.h"
#include "../fts.h" /* needed for the PHONE_KEY define */

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

/* static char tag[8]="[ FTS ]\0"; */
static int reset_gpio = GPIO_NOT_DEFINED;
static int system_resetted_up;
static int system_resetted_down;
extern chipInfo ftsInfo;

int readB2(u16 address, u8 *outBuf, int len)
{
	int remaining = len;
	int toRead = 0;
	int retry = 0;
	int ret;
	int event_to_search[3];
	u8 *readEvent = (u8 *)kmalloc(FIFO_EVENT_SIZE*sizeof(u8), GFP_KERNEL);
		u8 cmd[4] = { FTS_CMD_REQU_FW_CONF, 0x00, 0x00, (u8)len };

	if (readEvent == NULL) {
		logError(1, "%s readB2: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

		u16ToU8_be(address, &cmd[1]);

		logError(0, "%s %s", tag, printHex("Command B2 = ", cmd, 4));
		do {
			remaining = len;
			ret = fts_writeFwCmd(cmd, 4);
			if (ret < 0) {
				logError(1, "%s readB2: ERROR %02X\n", tag, ERROR_I2C_W);
				return ret;
			} /* ask to the FW the data */
			logError(0, "%s Command to FW sent!\n", tag);
			event_to_search[0] = (int)EVENTID_FW_CONFIGURATION;

			while (remaining > OK) {
				event_to_search[1] = (int)((address & 0xFF00)>>8);
				event_to_search[2] = (int) (address & 0x00FF);
					if (remaining > B2_DATA_BYTES) {
						toRead = B2_DATA_BYTES;
						remaining -= B2_DATA_BYTES;
					} else {
						toRead = remaining;
						remaining = 0;
					}

				ret = pollForEvent(event_to_search, 3, readEvent, GENERAL_TIMEOUT);
				if (ret >= OK) { /* start the polling for reading the reply */
					memcpy(outBuf, &readEvent[3], toRead);
					retry = 0;
					outBuf += toRead;

				} else {
					retry += 1;
					break;
				}
				address += B2_DATA_BYTES;
			}

		} while (retry < B2_RETRY && retry != 0);

		kfree(readEvent);
		if (retry == B2_RETRY) {
			logError(1, "%s readB2: ERROR %02X\n", tag, ERROR_TIMEOUT);
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

	u8 *buff = (u8 *)kmalloc((B2_CHUNK + 1)*sizeof(u8), GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s readB2U16: ERROR %02X\n", tag, ERROR_ALLOC);
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
		if (ret < 0)
			return ret;
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

	logError(0, "%s releaseInformation started... Chip INFO:\n", tag);

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s releaseInformation: ERROR %02X\n", tag, ret);
		return ret;
	}

	ret = pollForEvent(event_to_search, 1, &readEvent[0], RELEASE_INFO_TIMEOUT);
	/* start the polling for reading the reply */
	if (ret < OK) {
		logError(1, "%s releaseInformation: ERROR %02X\n", tag, ret);
		return ret;
	}

	logError(0, "%s releaseInformation: Finished!\n", tag, ret);
	return OK;

}

char *printHex(char *label, u8 *buff, int count)
{
	int i, offset;
	char *result = NULL;

	offset = strlen(label);
	result = (char *)kmalloc(((offset + 3 * count) + 1)*sizeof(char), GFP_KERNEL);
	if (result != NULL) {
		strlcpy(result, label, sizeof(result));

		for (i = 0; i < count; i++) {
			snprintf(&result[offset + i * 3], 4, "%02X ", buff[i]);
		}
		strlcat(result, "\n", sizeof(result));
	}
	return result;
}

int pollForEvent(int *event_to_search, int event_bytes, u8 *readData, int time_to_wait)
{
	int i, find, retry, count_err;
	int time_to_count;
	int err_handling = OK;
	StopWatch clock;

	u8 cmd[1] = { FIFO_CMD_READONE };
	char *temp = NULL;

	find = 0;
	retry = 0;
	count_err = 0;
	time_to_count = time_to_wait / TIMEOUT_RESOLUTION;

	startStopWatch(&clock);
	while (find != 1 && retry < time_to_count && fts_readCmd(cmd, 1, readData, FIFO_EVENT_SIZE) >= 0) {
		/* Log of errors */
		if (readData[0] == EVENTID_ERROR_EVENT) {
			logError(1, "%s %s", tag, printHex("ERROR EVENT = ", readData, FIFO_EVENT_SIZE));
			count_err++;
			err_handling = errorHandler(readData, FIFO_EVENT_SIZE);
			if ((err_handling&0xF0FF0000) == ERROR_HANDLER_STOP_PROC) {
				logError(1, "%s pollForEvent: forced to be stopped! ERROR %08X\n", tag, err_handling);
				return err_handling;
			}
		} else {
			if (readData[0] != EVENTID_NO_EVENT) {
				logError(1, "%s %s", tag, printHex("READ EVENT = ", readData, FIFO_EVENT_SIZE));
			}
			if (readData[0] == EVENTID_CONTROL_READY && event_to_search[0] != EVENTID_CONTROL_READY) {
				logError(1, "%s pollForEvent: Unmanned Controller Ready Event! Setting reset flags...\n", tag);
				setSystemResettedUp(1);
					setSystemResettedDown(1);
			}
		}

		find = 1;

		for (i = 0; i < event_bytes; i++) {

			if (event_to_search[i] != -1 && (int)readData[i] != event_to_search[i]) {
				find = 0;
				break;
			}
		}

		retry++;
		msleep(TIMEOUT_RESOLUTION);
	}
	stopStopWatch(&clock);
	if ((retry >= time_to_count) && find != 1) {
		logError(1, "%s pollForEvent: ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	} else if (find == 1) {
		temp = printHex("FOUND EVENT = ", readData, FIFO_EVENT_SIZE);
		if (temp != NULL)
			logError(0, "%s %s", tag, temp);
		kfree(temp);
		logError(0, "%s Event found in %d ms (%d iterations)! Number of errors found = %d\n", tag, elapsedMillisecond(&clock), retry, count_err);
		return count_err;
	}
	logError(1, "%s pollForEvent: ERROR %02X\n", tag, ERROR_I2C_R);
	return ERROR_I2C_R;
}

int flushFIFO(void)
{

	u8 cmd = FIFO_CMD_FLUSH; /* flush the FIFO */
	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s flushFIFO: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s FIFO flushed!\n", tag);
	return OK;

}

int fts_disableInterrupt(void)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_DISABLE }; /* disable interrupt */
	u16ToU8_be(IER_ADDR, &cmd[1]);

	if (fts_writeCmd(cmd, 4) < OK) {
		logError(1, "%s fts_disableInterrupt: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	logError(0, "%s Interrupt Disabled!\n", tag);
	return OK;
}

int fts_enableInterrupt(void)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_ENABLE }; /* enable interrupt */
	u16ToU8_be(IER_ADDR, &cmd[1]);
		if (fts_writeCmd(cmd, 4) < 0) {
		logError(1, "%s fts_enableInterrupt: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	logError(0, "%s Interrupt Enabled!\n", tag);
	return OK;
}

int u8ToU16n(u8 *src, int src_length, u16 *dst)
{
	int i, j;

	if (src_length % 2 != 0) {
		return 0;
	}
	j = 0;
	dst = (u16 *)kmalloc((src_length / 2)*sizeof(u16), GFP_KERNEL);
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
	dst = (u8 *)kmalloc((2 * src_length)*sizeof(u8), GFP_KERNEL);
	j = 0;
	for (i = 0; i < src_length; i++) {
		dst[j] = (u8) (src[i] & 0xFF00)>>8;
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
	*dst = (u32)(((src[3] & 0x000000FF) << 24) + ((src[2] & 0x000000FF) << 16) + ((src[1] & 0x000000FF) << 8) + (src[0] & 0x000000FF));
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

int attempt_function(int(*code)(void), unsigned long wait_before_retry, int retry_count)
{
	int result;
	int count = 0;

	do {
		result = code();
		count++;
		msleep(wait_before_retry);
	} while (count < retry_count && result < 0);

	if (count == retry_count)
		return (result | ERROR_TIMEOUT);
	else
		return result;

}

void setResetGpio(int gpio)
{
	reset_gpio = gpio;
	logError(1, "%s setResetGpio: reset_gpio = %d\n", tag, reset_gpio);
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
			res = fts_writeCmd(cmd, 4);
		} else {
			gpio_set_value(reset_gpio, 0);
			msleep(10);
			gpio_set_value(reset_gpio, 1);
			res = OK;
		}
		if (res < OK) {
			logError(1, "%s fts_system_reset: ERROR %02X\n", tag, ERROR_I2C_W);
		} else {
			res = pollForEvent(&event_to_search, 1, readData, GENERAL_TIMEOUT);
			if (res < OK) {
				logError(1, "%s fts_system_reset: ERROR %02X\n", tag, res);
			}
		}
	}
	if (res < OK) {
		logError(1, "%s fts_system_reset...failed after 3 attempts: ERROR %02X\n", tag, (res | ERROR_SYSTEM_RESET_FAIL));
		return (res | ERROR_SYSTEM_RESET_FAIL);
	}
	logError(0, "%s System reset DONE!\n", tag);
	system_resetted_down = 1;
	system_resetted_up = 1;
	return OK;

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
		logError(1, "%s senseOn: ERROR %02X\n", tag, ERROR_SENSE_ON_FAIL);
		return (ret|ERROR_SENSE_ON_FAIL);
	}

	logError(0, "%s senseOn: SENSE ON\n", tag);
	return OK;
}

int senseOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_MT_SENSE_OFF };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s senseOff: ERROR %02X\n", tag, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}

	logError(0, "%s senseOff: SENSE OFF\n", tag);
	return OK;

}

int keyOn(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_ON };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s keyOn: ERROR %02X\n", tag, ERROR_SENSE_ON_FAIL);
		return (ret | ERROR_SENSE_ON_FAIL);
	}

	logError(0, "%s keyOn: KEY ON\n", tag);
	return OK;

}

int keyOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_OFF };

	ret = fts_writeFwCmd(cmd, 1);
	if (ret < OK) {
		logError(1, "%s keyOff: ERROR %02X\n", tag, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}

	logError(0, "%s keyOff: KEY OFF\n", tag);
	return OK;

}

int cleanUp(int enableTouch)
{
	int res;

	logError(0, "%s cleanUp: system reset...\n", tag);
	res = fts_system_reset();
	if (res < OK)
		return res;
	if (enableTouch) {
		logError(0, "%s cleanUp: enabling touches...\n", tag);
		res = senseOn();
		if (res < OK)
			return res;
#ifdef PHONE_KEY
		res = keyOn();
		if (res < OK)
			return res;
#endif
		logError(0, "%s cleanUp: enabling interrupts...\n", tag);
		res = fts_enableInterrupt();
		if (res < OK)
			return res;
	}
	return OK;

}

int checkEcho(u8 *cmd, int size)
{
	int ret, i;
	int event_to_search[size+1];
	u8 readData[FIFO_EVENT_SIZE];

	if ((ftsInfo.u32_echoEn & 0x00000001) != ECHO_ENABLED) {
		logError(1, "%s ECHO Not Enabled!\n", tag);
		return OK;
	}
	if (size < 1) {
		logError(1, "%s checkEcho: Error Size = %d not valid! or ECHO not Enabled! ERROR %08X\n", tag, size, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
	if ((size+2) > FIFO_EVENT_SIZE)
		size = FIFO_EVENT_SIZE-2;
	/* Echo event EC xx xx xx xx xx xx fifo_status therefore for command
	*with more than 6 bytes will echo only the first 6
	*/
	event_to_search[0] = EVENTID_ECHO;
	for (i = 1; i <= size; i++) {
		event_to_search[i] = cmd[i-1];
	}
	ret = pollForEvent(event_to_search, size+1, readData, GENERAL_TIMEOUT);
	if (ret < OK) {
			logError(1, "%s checkEcho: Echo Event not found! ERROR %02X\n", tag, ret);
			return (ret | ERROR_CHECK_ECHO_FAIL);
	}

	logError(0, "%s ECHO OK!\n", tag);
	return OK;
}

int featureEnableDisable(int on_off, u8 feature)
{
	int ret;
	u8 cmd[2] = { 0x00, feature };

	if (on_off == FEAT_ENABLE) {
		cmd[0] = FTS_CMD_FEATURE_ENABLE;
		logError(0, "%s featureEnableDisable: Enabling feature %02X ...\n", tag, feature);
	} else {
		cmd[0] = FTS_CMD_FEATURE_DISABLE;
		logError(0, "%s featureEnableDisable: Disabling feature %02X ...\n", tag, feature);
	}

	ret = fts_writeCmd(cmd, 2);			/* not use writeFwCmd because this function can be called also during interrupt enable and should be fast */
	if (ret < OK) {
		logError(1, "%s featureEnableDisable: ERROR %02X\n", tag, ret);
		return (ret | ERROR_FEATURE_ENABLE_DISABLE);
	}

	logError(0, "%s featureEnableDisable: DONE!\n", tag);
	return OK;

}

short **array1dTo2d_short(short *data, int size, int columns)
{

	int i;
	short **matrix = (short **)kmalloc(((int)(size / columns))*sizeof(short *), GFP_KERNEL);
	if (matrix != NULL) {
		for (i = 0; i < (int)(size / columns); i++) {
			matrix[i] = (short *)kmalloc(columns*sizeof(short), GFP_KERNEL);
		}

		for (i = 0; i < size; i++)
			matrix[i / columns][i % columns] = data[i];
	}

	return matrix;
}

u8 **array1dTo2d_u8(u8 *data, int size, int columns)
{

	int i;
	u8 **matrix = (u8 **)kmalloc(((int)(size / columns))*sizeof(u8 *), GFP_KERNEL);
	if (matrix != NULL) {
		for (i = 0; i < (int)(size / columns); i++) {
			matrix[i] = (u8 *)kmalloc(columns*sizeof(u8), GFP_KERNEL);
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
		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}
		logError(0, "\n");
		kfree(matrix[i]);
	}
}

void print_frame_u8(char *label, u8 **matrix, int row, int column)
{
	int i, j;
	logError(0, "%s %s\n", tag, label);
		for (i = 0; i < row; i++) {
			logError(0, "%s ", tag);
			for (j = 0; j < column; j++) {
				printk("%d ", matrix[i][j]);
			}
			logError(0, "\n");
			kfree(matrix[i]);
		}
}

void print_frame_u32(char *label, u32 **matrix, int row, int column)
{
	int i, j;
	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}
		logError(0, "\n");
		kfree(matrix[i]);
	}
}

void print_frame_int(char *label, int **matrix, int row, int column)
{
	int i, j;
	logError(0, "%s %s\n", tag, label);
	for (i = 0; i < row; i++) {
		logError(0, "%s ", tag);
		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}
		logError(0, "\n");
		kfree(matrix[i]);
	}
}
