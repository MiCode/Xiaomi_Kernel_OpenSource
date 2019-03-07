/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                     FTS Utility Functions				 *
*                                                                        *
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
#include "../fts.h"
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
	char *temp = NULL;
	u8 *init_outBuf = outBuf;
	u16 init_addr = address;
	u8 readEvent[FIFO_EVENT_SIZE] = {0};
	u8 cmd[4] = { FTS_CMD_REQU_FW_CONF, 0x00, 0x00, (u8)len };

	if (readEvent == NULL) {
		log_error("%s readB2: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	u16ToU8_be(address, &cmd[1]);
	temp = printHex("Command B2 = ", cmd, 4);
	if (temp != NULL) {
		log_debug("%s %s", tag, temp);
		kfree(temp);
	}

	do {
		remaining = len;
		ret = fts_writeFwCmd(cmd, 4);

		if (ret < 0) {
			log_error("%s readB2: ERROR %02X\n", tag, ERROR_I2C_W);
			return ret;
		}

		log_debug("%s Command to FW sent!\n", tag);
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

			ret = pollForEvent(event_to_search, 3, readEvent, GENERAL_TIMEOUT);

			if (ret >= OK) {
				memcpy(outBuf, &readEvent[3], toRead);
				retry = 0;
				outBuf += toRead;
			} else {
				retry += 1;
				break;
			}

			address += B2_DATA_BYTES;
		}
		log_debug("%s readB2: B2 failed... attempt = %d\n", tag, retry);
		outBuf = init_outBuf;
		address = init_addr;
	} while (retry < B2_RETRY && retry != 0);

	kfree(readEvent);

	if (retry == B2_RETRY) {
		log_error("%s readB2: ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	log_debug("%s B2 read %d bytes\n", tag, len);
	return OK;
}

int readB2U16(u16 address, u8 *outBuf, int byteToRead)
{
	int remaining = byteToRead;
	int toRead = 0;
	int ret;
	u8 *buff = (u8 *)kmalloc((B2_CHUNK + 1) * sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		log_error("%s readB2U16: ERROR %02X\n", tag, ERROR_ALLOC);
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
	log_debug("%s releaseInformation started... Chip INFO:\n", tag);
	ret = fts_writeFwCmd(cmd, 1);

	if (ret < OK) {
		log_error("%s releaseInformation: ERROR %02X\n", tag, ret);
		return ret;
	}

	ret = pollForEvent(event_to_search, 1, &readEvent[0], RELEASE_INFO_TIMEOUT);

	if (ret < OK) {
		log_error("%s releaseInformation: ERROR %02X\n", tag, ret);
		return ret;
	}

	log_debug("%s releaseInformation: Finished!\n", tag);
	return OK;
}

char *printHex(char *label, u8 *buff, int count)
{
	int i, offset;
	char *result = NULL;
	offset = strlen(label);
	result = (char *)kmalloc(((offset + 3 * count) + 1) * sizeof(char), GFP_KERNEL);
	if (result != NULL) {
		strlcpy(result, label, (3 * count + offset + 1));

		for (i = 0; i < count; i++) {
			snprintf(&result[offset + i * 3], 4, "%02X ", buff[i]);
		}

		strlcat(result, "\n", (3 * count + offset + 1));
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
		if (readData[0] == EVENTID_ERROR_EVENT) {
			temp = printHex("ERROR EVENT = ", readData, FIFO_EVENT_SIZE);
			if (temp != NULL) {
				log_error("%s %s\n", tag, temp);
				kfree(temp);
			}
			count_err++;
			err_handling = errorHandler(readData, FIFO_EVENT_SIZE);

			if ((err_handling & 0xF0FF0000) == ERROR_HANDLER_STOP_PROC) {
				log_error("%s pollForEvent: forced to be stopped! ERROR %08X\n", tag, err_handling);
				return err_handling;
			}
		} else {
			if (readData[0] != EVENTID_NO_EVENT) {
				temp = printHex("READ EVENT = ", readData, FIFO_EVENT_SIZE);
				if (temp != NULL) {
					log_debug("%s %s\n", tag, temp);
					kfree(temp);
				}
			}

			if (readData[0] == EVENTID_CONTROL_READY && event_to_search[0] != EVENTID_CONTROL_READY) {
				log_error("%s pollForEvent: Unmanned Controller Ready Event! Setting reset flags...\n", tag);
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
		mdelay(TIMEOUT_RESOLUTION);
	}

	stopStopWatch(&clock);

	if ((retry >= time_to_count) && find != 1) {
		log_error("%s pollForEvent: ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	} else if (find == 1) {
		temp = printHex("FOUND EVENT = ", readData, FIFO_EVENT_SIZE);

		if (temp != NULL) {
			log_debug("%s %s\n", tag, temp);
			kfree(temp);
		}
		log_debug("%s Event found in %d ms (%d iterations)! Number of errors found = %d\n", tag, elapsedMillisecond(&clock), retry, count_err);
		return count_err;
	} else {
		log_error("%s pollForEvent: ERROR %02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}
}

int flushFIFO(void)
{
	u8 cmd = FIFO_CMD_FLUSH;

	if (fts_writeCmd(&cmd, 1) < 0) {
		log_error("%s flushFIFO: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s FIFO flushed!\n", tag);
	return OK;
}

int fts_disableInterrupt(void)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_DISABLE };
	u16ToU8_be(IER_ADDR, &cmd[1]);

	if (fts_writeCmd(cmd, 4) < OK) {
		log_error("%s fts_disableInterrupt: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s Interrupt Disabled!\n", tag);
	return OK;
}


int fts_enableInterrupt(void)
{
	u8 cmd[4] = { FTS_CMD_HW_REG_W, 0x00, 0x00, IER_ENABLE };
	u16ToU8_be(IER_ADDR, &cmd[1]);

	if (fts_writeCmd(cmd, 4) < 0) {
		log_error("%s fts_enableInterrupt: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s Interrupt Enabled!\n", tag);
	return OK;
}

int u8ToU16n(u8 *src, int src_length, u16 *dst)
{
	int i, j;
	int ret = -1;

	if (src_length % 2 != 0)
		return ret;
	else {
		j = 0;
		dst = (u16 *)kmalloc((src_length / 2) * sizeof(u16), GFP_KERNEL);

		for (i = 0; i < src_length; i += 2) {
			dst[j] = ((src[i + 1] & 0x00FF) << 8) + (src[i] & 0x00FF);
			j++;
		}
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
	dst = (u8 *)kmalloc((2 * src_length) * sizeof(u8), GFP_KERNEL);
	j = 0;

	for (i = 0; i < src_length; i++) {
		dst[j] = (u8)(src[i] & 0xFF00) >> 8;
		dst[j + 1] = (u8)(src[i] & 0x00FF);
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
	*dst = (u32)(((src[3] & 0xFF) << 24) + ((src[2] & 0xFF) << 16) + ((src[1] & 0xFF) << 8) + (src[0] & 0xFF));
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
		mdelay(wait_before_retry);
	} while (count < retry_count && result < 0);

	if (count == retry_count)
		return (result | ERROR_TIMEOUT);
	else
		return result;
}

void setResetGpio(int gpio)
{
	reset_gpio = gpio;
	log_debug("%s setResetGpio: reset_gpio = %d\n", tag, reset_gpio);
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
	log_debug("%s System resetting...\n", tag);

	for (i = 0; i < SYSTEM_RESET_RETRY && res < 0; i++) {
		if (reset_gpio == GPIO_NOT_DEFINED) {
			res = fts_writeCmd(cmd, 4);
		} else {
			gpio_set_value(reset_gpio, 0);
			mdelay(10);
			gpio_set_value(reset_gpio, 1);
			res = OK;
		}

		if (res < OK) {
			log_error("%s fts_system_reset: ERROR %02X\n", tag, ERROR_I2C_W);
		} else {
			res = pollForEvent(&event_to_search, 1, readData, GENERAL_TIMEOUT);

			if (res < OK) {
				log_error("%s fts_system_reset: ERROR %02X\n", tag, res);
			}
		}
	}

	if (res < OK) {
		log_error("%s fts_system_reset...failed after 3 attempts: ERROR %02X\n", tag, (res | ERROR_SYSTEM_RESET_FAIL));
		return (res | ERROR_SYSTEM_RESET_FAIL);
	} else {
		log_debug("%s System reset DONE!\n", tag);
		system_resetted_down = 1;
		system_resetted_up = 1;
		return OK;
	}
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
		log_error("%s senseOn: ERROR %02X\n", tag, ERROR_SENSE_ON_FAIL);
		return (ret | ERROR_SENSE_ON_FAIL);
	}

	log_debug("%s senseOn: SENSE ON\n", tag);
	return OK;
}

int senseOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_MT_SENSE_OFF };
	ret = fts_writeFwCmd(cmd, 1);

	if (ret < OK) {
		log_error("%s senseOff: ERROR %02X\n", tag, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}

	log_debug("%s senseOff: SENSE OFF\n", tag);
	return OK;
}

int keyOn(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_ON };
	ret = fts_writeFwCmd(cmd, 1);

	if (ret < OK) {
		log_error("%s keyOn: ERROR %02X\n", tag, ERROR_SENSE_ON_FAIL);
		return (ret | ERROR_SENSE_ON_FAIL);
	}

	log_debug("%s keyOn: KEY ON\n", tag);
	return OK;
}

int keyOff(void)
{
	int ret;
	u8 cmd[1] = { FTS_CMD_MS_KEY_OFF };
	ret = fts_writeFwCmd(cmd, 1);

	if (ret < OK) {
		log_error("%s keyOff: ERROR %02X\n", tag, ERROR_SENSE_OFF_FAIL);
		return (ret | ERROR_SENSE_OFF_FAIL);
	}

	log_debug("%s keyOff: KEY OFF\n", tag);
	return OK;
}

int cleanUp(int enableTouch)
{
	int res;
	log_debug("%s cleanUp: system reset...\n", tag);
	res = fts_system_reset();

	if (res < OK)
		return res;

	if (enableTouch) {
		log_debug("%s cleanUp: enabling touches...\n", tag);
		res = senseOn();

		if (res < OK)
			return res;

#ifdef PHONE_KEY
		res = keyOn();

		if (res < OK)
			return res;

#endif
		log_debug("%s cleanUp: enabling interrupts...\n", tag);
		res = fts_enableInterrupt();

		if (res < OK)
			return res;
	}

	return OK;
}

int checkEcho(u8 *cmd, int size)
{
	int ret, i;
	int event_to_search[size + 1];
	u8 readData[FIFO_EVENT_SIZE];

	if ((ftsInfo.u32_echoEn & 0x00000001) != ECHO_ENABLED) {
		log_debug("%s ECHO Not Enabled!\n", tag);
		return OK;
	}

	if (size < 1) {
		log_error("%s checkEcho: Error Size = %d not valid! or ECHO not Enabled! ERROR %08X\n", tag, size, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	} else {
		if ((size + 2) > FIFO_EVENT_SIZE)
			size = FIFO_EVENT_SIZE - 2;

		event_to_search[0] = EVENTID_ECHO;

		for (i = 1; i <= size; i++) {
			event_to_search[i] = cmd[i - 1];
		}

		ret = pollForEvent(event_to_search, size + 1, readData, GENERAL_TIMEOUT);

		if (ret < OK) {
			log_error("%s checkEcho: Echo Event not found! ERROR %02X\n", tag, ret);
			return (ret | ERROR_CHECK_ECHO_FAIL);
		}

		log_debug("%s ECHO OK!\n", tag);
		return OK;
	}
}

int featureEnableDisable(int on_off, u8 feature)
{
	int ret;
	u8 cmd[2] = { 0x00, feature };

	if (on_off == FEAT_ENABLE) {
		cmd[0] = FTS_CMD_FEATURE_ENABLE;
		log_debug("%s featureEnableDisable: Enabling feature %02X ...\n", tag, feature);
	} else {
		cmd[0] = FTS_CMD_FEATURE_DISABLE;
		log_debug("%s featureEnableDisable: Disabling feature %02X ...\n", tag, feature);
	}

	ret = fts_writeCmd(cmd, 2);

	if (ret < OK) {
		log_error("%s featureEnableDisable: ERROR %02X\n", tag, ret);
		return (ret | ERROR_FEATURE_ENABLE_DISABLE);
	}

	log_debug("%s featureEnableDisable: DONE!\n", tag);
	return OK;
}

short **array1dTo2d_short(short *data, int size, int columns)
{
	int i;
	short **matrix = (short **)kmalloc(((int)(size / columns)) * sizeof(short *), GFP_KERNEL);

	if (matrix != NULL) {
		for (i = 0; i < (int)(size / columns); i++) {
			matrix[i] = (short *)kmalloc(columns * sizeof(short), GFP_KERNEL);
		}

		for (i = 0; i < size; i++) {
			matrix[i / columns][i % columns] = data[i];
		}
	}

	return matrix;
}

u8 **array1dTo2d_u8(u8 *data, int size, int columns)
{
	int i;
	u8 **matrix = (u8 **)kmalloc(((int)(size / columns)) * sizeof(u8 *), GFP_KERNEL);

	if (matrix != NULL) {
		for (i = 0; i < (int)(size / columns); i++) {
			matrix[i] = (u8 *)kmalloc(columns * sizeof(u8), GFP_KERNEL);
		}

		for (i = 0; i < size; i++) {
			matrix[i / columns][i % columns] = data[i];
		}
	}

	return matrix;
}

void print_frame_short(char *label, short **matrix, int row, int column)
{
	int i, j;
	log_error("%s %s\n", tag, label);

	for (i = 0; i < row; i++) {
		log_debug("%s ", tag);

		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}

		log_debug("\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

void print_frame_u8(char *label, u8 **matrix, int row, int column)
{
	int i, j;
	log_error("%s %s\n", tag, label);

	for (i = 0; i < row; i++) {
		log_debug("%s ", tag);

		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}

		log_debug("\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

void print_frame_u32(char *label, u32 **matrix, int row, int column)
{
	int i, j;
	log_error("%s %s\n", tag, label);

	for (i = 0; i < row; i++) {
		log_debug("%s ", tag);

		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}

		log_debug("\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}


void print_frame_int(char *label, int **matrix, int row, int column)
{
	int i, j;
	log_error("%s %s\n", tag, label);

	for (i = 0; i < row; i++) {
		log_debug("%s ", tag);

		for (j = 0; j < column; j++) {
			printk("%d ", matrix[i][j]);
		}

		log_debug("\n");
		kfree(matrix[i]);
	}
	kfree(matrix);
}

int fts_get_lockdown_info(u8 *lockdata)
{
	int ret = 0, retry = 0, toRead = 0, byteToRead;
	u8 cmd = FTS_CMD_LOCKDOWN_READ;
	int event_to_search[3] = {EVENTID_LOCKDOWN_INFO_READ, -1, 0x00};
	u8 readEvent[FIFO_EVENT_SIZE];
	char *temp = NULL;
	int size;

	log_debug("%s %s: Reading Lockdown code from the IC ...\n", tag, __func__);
	ret = fts_disableInterrupt();
	if (ret < OK) {
		log_error("%s %s: ERROR %08X\n", tag, __func__, ret);
		ret = (ret | ERROR_LOCKDOWN_CODE);
		goto ERROR;
	}
	for (retry = 0; retry < LOCKDOWN_CODE_RETRY; retry++) {
		event_to_search[2] = 0x00;
		log_debug("%s %s: Read Lockdown data... (%d attempt) \n", tag, __func__, retry + 1);
		ret = fts_writeFwCmd(&cmd, 1);
		if (ret < OK) {
			log_error("%s %s: Unable to send Lockdown data write command... ERROR %08X\n", tag, __func__, ret);
			ret = (ret | ERROR_LOCKDOWN_CODE);
			continue;
		}

		ret = pollForEvent(event_to_search, 3, &readEvent[0], GENERAL_TIMEOUT);
		if (ret >= OK) {
			byteToRead = readEvent[1];
			size = byteToRead;
			log_debug("%s %s: Lockdown Code size = %d\n", tag, __func__, size);
			if (lockdata == NULL) {
				log_error("%s %s: Unable to allocate lockdata... ERROR %08X\n", tag, __func__, ERROR_ALLOC);
				ret = (ERROR_ALLOC | ERROR_LOCKDOWN_CODE);
				continue;
			}
			while (byteToRead > 0) {
				if ((readEvent[1] - readEvent[2]) > LOCKDOWN_CODE_READ_CHUNK)
					toRead = LOCKDOWN_CODE_READ_CHUNK;
				else
					toRead = readEvent[1] - readEvent[2];
				byteToRead -= toRead;
				memcpy(&lockdata[readEvent[2]], &readEvent[3], toRead);
				event_to_search[2] += toRead;
				if (byteToRead > 0) {
					ret = pollForEvent(event_to_search, 3, &readEvent[0], GENERAL_TIMEOUT);
					if (ret < OK) {
						log_error("%s %s: Can not find lockdown code read reply event with offset %02X ! ERROR %08X\n", tag, __func__, event_to_search[2], ret);
						ret = (ERROR_ALLOC | ERROR_LOCKDOWN_CODE);
						break;
					}
				}
			}
			if (byteToRead != 0) {
				log_error("%s %s: Read Lockdown code FAIL! ERROR %08X\n", tag, __func__, ret);
				continue;
			} else {
				log_debug("%s %s: Lockdown Code read DONE!\n", tag, __func__);
				ret = OK;
				temp = printHex("Lockdown Code = ", lockdata, size);
				if (temp != NULL) {
					log_debug("%s %s: %s", tag, __func__, temp);
					kfree(temp);
				}
				break;
			}
		} else {
			log_error( "%s %s: Can not find first lockdown code read reply event! ERROR %08X\n", tag, __func__, ret);
		}
	}
ERROR:
		if (fts_enableInterrupt() < OK)
			log_error( "%s %s: Error while re-enabling the interrupt!\n", tag, __func__);
		return ret;
}

int writeNoiseParameters(u8 *noise)
{
	int ret, i;
	u8 cmd[2 + NOISE_PARAMETERS_SIZE];
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[2] = {EVENTID_NOISE_WRITE, NOISE_PARAMETERS};

	log_debug("%s writeNoiseParameters: Writing noise parameters to the IC ...\n", tag);
	ret = fts_disableInterrupt();
	if (ret < OK) {
		log_error("%s readNoiseParameters: ERROR %08X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	cmd[0] = FTS_CMD_NOISE_WRITE;
	cmd[1] = NOISE_PARAMETERS;
	log_debug("%s writeNoiseParameters: Noise parameters = ", tag);
	for (i = 0; i < NOISE_PARAMETERS_SIZE; i++) {
		cmd[2 + i] = noise[i];
		log_debug("%02X ", cmd[2 + i]);
	}
	log_debug("\n");
	ret = fts_writeCmd(cmd, NOISE_PARAMETERS_SIZE + 2);
	if (ret < OK) {
		log_error("%s writeNoiseParameters: impossible write command... ERROR %02X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	ret = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);
	if (ret < OK) {
		log_error("%s writeNoiseParameters: polling FIFO ERROR %02X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	if (readData[2] != 0x00) {
		log_error("%s writeNoiseParameters: Event check FAIL! %02X != 0x00 ERROR %02X\n", tag, readData[2], ERROR_NOISE_PARAMETERS);
		ret = ERROR_NOISE_PARAMETERS;
		goto ERROR;
	}
	log_debug("%s writeNoiseParameters: DONE!\n", tag);
	ret = OK;
ERROR:
	ret = fts_enableInterrupt();
	if (ret < OK) {
		log_error("%s readNoiseParameters: ERROR %02X\n", tag, ret);
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

	log_debug("%s ReadNoiseParameters: Reading noise parameters from the IC ...\n", tag);
	ret = fts_disableInterrupt();
	if (ret < OK) {
		log_error("%s readNoiseParameters: ERROR %02X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	cmd[0] = FTS_CMD_NOISE_READ;
	cmd[1] = NOISE_PARAMETERS;
	ret = fts_writeCmd(cmd, 2);
	if (ret < OK) {
		log_error("%s readNoiseParameters: impossible write command... ERROR %02X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	ret = pollForEvent(event_to_search, 2, readData, GENERAL_TIMEOUT);
	if (ret < OK) {
		log_error("%s readNoiseParameters: polling FIFO ERROR %02X\n", tag, ret);
		ret = (ret | ERROR_NOISE_PARAMETERS);
		goto ERROR;
	}
	log_debug("%s readNoiseParameters: Noise parameters = ", tag);
	for (i = 0; i < NOISE_PARAMETERS_SIZE; i++) {
		noise[i] = readData[2 + i];
		log_debug("%02X ", noise[i]);
	}
	log_debug("\n");
	log_debug("%s readNoiseParameters: DONE!\n", tag);
	ret = OK;
ERROR:
	ret = fts_enableInterrupt();
	if (ret < OK) {
		log_error("%s readNoiseParameters: ERROR %02X\n", tag, ret);
		return (ret | ERROR_NOISE_PARAMETERS);
	}
	return ret;
}
