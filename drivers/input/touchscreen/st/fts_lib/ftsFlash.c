/*

 **************************************************************************
 **                        STMicroelectronics		                **
 **************************************************************************
 **                        marco.cali@st.com				**
 **************************************************************************
 *                                                                        *
 *			FTS API for Flashing the IC			 *
 *                                                                        *
 **************************************************************************
 **************************************************************************

 */

#include "ftsCrossCompile.h"
#include "ftsCompensation.h"
#include "ftsError.h"
#include "ftsFlash.h"
#include "ftsFrame.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTest.h"
#include "ftsTime.h"
#include "ftsTool.h"
#include "../fts.h"	/* needed for the FW_H_FILE define */

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
#include <linux/time.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
/* #include <linux/sec_sysfs.h> */

#ifdef FW_H_FILE
#include <../fts_fw.h>
#endif

/* static char tag[8] = "[ FTS ]\0"; */
extern chipInfo ftsInfo;

int getFirmwareVersion(u16 *fw_vers, u16 *config_id)
{
	u8 fwvers[DCHIP_FW_VER_BYTE];
	u8 confid[CONFIG_ID_BYTE];
	int res;

	res = readCmdU16(FTS_CMD_HW_REG_R, DCHIP_FW_VER_ADDR, fwvers, DCHIP_FW_VER_BYTE, DUMMY_HW_REG);
	if (res < OK) {
		logError(1, "%s getFirmwareVersion: unable to read fw_version ERROR %02X\n", tag, ERROR_FW_VER_READ);
		return (res | ERROR_FW_VER_READ);
	}

	u8ToU16(fwvers, fw_vers); /* fw version use big endian */
	if (*fw_vers != 0) { /*  if fw_version is 00 00 means that there is no firmware running in the chip therefore will be impossible find the config_id */
		res = readB2(CONFIG_ID_ADDR, confid, CONFIG_ID_BYTE);
		if (res < OK) {
			logError(1, "%s getFirmwareVersion: unable to read config_id ERROR %02X\n", tag, ERROR_FW_VER_READ);
			return (res | ERROR_FW_VER_READ);
		}
		u8ToU16(confid, config_id); /* config id use little endian */
	} else {
		*config_id = 0x0000;
	}

	logError(0, "%s FW VERS = %04X\n", tag, *fw_vers);
	logError(0, "%s CONFIG ID = %04X\n", tag, *config_id);
	return OK;

}

#ifdef FTM3_CHIP

int flash_status(void)
{
	u8 cmd[2] = {FLASH_CMD_READSTATUS, 0x00};
	u8 readData = 0;

	logError(0, "%s Reading flash_status...\n", tag);
	if (fts_readCmd(cmd, 2, &readData, FLASH_STATUS_BYTES) < 0) {
		logError(1, "%s flash_status: ERROR % 02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	readData &= 0x01;
	/* logError(0, "%s flash_status = %d\n", tag,readData); */
	return (int) readData;

}

int flash_status_ready(void)
{

	int status = flash_status();

	if (status == ERROR_I2C_R) {
		logError(1, "%s flash_status_ready: ERROR % 02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	if (status != FLASH_READY) {
		logError(1, "%s flash_status_ready: flash busy or unknown STATUS = % 02X\n", tag, status);
		return ERROR_FLASH_UNKNOWN;
	}

	return FLASH_READY;

}

int wait_for_flash_ready(void)
{
	int status;
	int(*code)(void);

	code = flash_status_ready;

	logError(0, "%s Waiting for flash ready...\n", tag);
	status = attempt_function(code, FLASH_WAIT_BEFORE_RETRY, FLASH_RETRY_COUNT);

	if (status != FLASH_READY) {
		logError(1, "%s wait_for_flash_ready: ERROR % 02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	logError(0, "%s Flash ready!\n", tag);
	return OK;
}

int flash_unlock(void)
{

	int status;
	u8 cmd[3] = {FLASH_CMD_UNLOCK, FLASH_UNLOCK_CODE0, FLASH_UNLOCK_CODE1}; /* write the comand to perform the unlock */

	logError(0, "%s Try to unlock flash...\n", tag);
	status = wait_for_flash_ready();

	if (status != OK) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY); /* Flash not ready within the choosen time, better exit! */
	}

	logError(0, "%s Command unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready();

	if (status != OK) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY); /* Flash not ready within the choosen time, better exit! */
	}

	logError(0, "%s Unlock flash DONE!\n", tag);

	return OK;

}

/*int parseMemhFile(const char *pathToFile, u8** data, int* length, int dimension)
{

	int i = 0;
	unsigned long ul;
	u8* buff = NULL;
	int fd = -1;
	int n, size, pointer = 0;
	char *data_file, *line;
	const struct firmware *fw = NULL;
	struct device *dev = NULL;

	line = (char *) kmalloc(11 * sizeof (char), GFP_KERNEL);
	if (line == NULL) {
		logError(1, "%s parseMemhFile: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	logError(0, "%s parseMemhFile: allocating %d bytes\n", tag, dimension);
	buff = (u8*) kmalloc(dimension * sizeof (u8), GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s parseMemhFile: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	dev = getDev();
	if (dev != NULL)
		fd = request_firmware(&fw, pathToFile, dev);

	if (fd == 0) {
		size = fw->size;
		logError(0, "%s The size of the firmware file is %d bytes...\n", tag, size);
		data_file = (char *) fw->data;
		logError(0, "%s Start to reading %s...\n", tag, pathToFile);

		while (size - pointer > 0 && (i * 4 + 4) <= dimension) {
			if (readLine(&data_file[pointer], &line, size - pointer, &n) < 0) {
				break;
			}
			pointer += n;
			logError(0, "%s Pointer= %d riga = %s\n", tag, pointer, line);
			ul = simple_strtoul(line, NULL, 16);

			buff[i * 4] = (u8) ((ul & 0x000000FF) >> 0);
			buff[i * 4 + 1] = (u8) ((ul & 0x0000FF00) >> 8);
			buff[i * 4 + 2] = (u8) ((ul & 0x00FF0000) >> 16);
			buff[i * 4 + 3] = (u8) ((ul & 0xFF000000) >> 24);
			i++;
		}

		kfree(line);

		*length = i * 4;
		if (*length < dimension) {
			logError(1, "%s parseMemhFile: Read only %d instead of %d... ERROR %02X\n", tag, *length, dimension, ERROR_FILE_PARSE);
			release_firmware(fw);
			return ERROR_FILE_PARSE;
		}
		*data = buff;

		logError(0, "%s READ DONE %d bytes!\n", tag, *length);
		release_firmware(fw);
		return OK;
	} else {
		logError(1, "%s parseProductionTestLimits: ERROR %02X\n", tag, ERROR_FILE_NOT_FOUND);
		return ERROR_FILE_NOT_FOUND;
	}

}*/

int parseBinFile(const char *pathToFile, u8 **data, int *length, int dimension)
{

	int fd = -1;
	int fw_size = 0;
	u8 *fw_data = NULL;

#ifndef FW_H_FILE
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	dev = getDev();

	if (dev != NULL)
		fd = request_firmware(&fw, pathToFile, dev);
	else {
		logError(1, "%s parseBinFile: No device found! ERROR %02X\n", ERROR_FILE_PARSE);
		return ERROR_FILE_PARSE;
	}
#else
	fd = 0;
#endif

	if (fd == 0) {
#ifndef FW_H_FILE
	fw_size = fw->size;
	fw_data = (u8 *) (fw->data);
#else
	fw_size = FW_SIZE_NAME;
	fw_data = (u8 *) FW_ARRAY_NAME;
#endif
		if (fw_size - FW_HEADER_SIZE != FW_SIZE) {
			logError(1, "%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, FW_SIZE, ERROR_FILE_PARSE);
#ifndef FW_H_FILE
			release_firmware(fw);
#endif
			return ERROR_FILE_PARSE;
		}
		*data = (u8 *) kmalloc(dimension * sizeof (u8), GFP_KERNEL);
		if (*data == NULL) {
			logError(1, "%s parseBinFile: ERROR %02X\n", tag, ERROR_ALLOC);
#ifndef FW_H_FILE
			release_firmware(fw);
#endif
			return ERROR_ALLOC;
		}

		memcpy(*data, ((u8 *) (fw_data) + FW_HEADER_SIZE), dimension);
		*length = dimension;

		logError(0, "%s READ FW DONE %d bytes!\n", tag, *length);
#ifndef FW_H_FILE
		release_firmware(fw);
#endif
		return OK;
		}
	logError(1, "%s parseBinFile: File Not Found! ERROR %02X\n", tag, ERROR_FILE_NOT_FOUND);
	return ERROR_FILE_NOT_FOUND;
}

int readFwFile(const char *path, Firmware *fw, int keep_cx)
{
	int res;
	int size;

	if (keep_cx) {
		size = FW_SIZE - FW_CX_SIZE;
		logError(1, "%s readFwFile: Selected 124k Configuration!\n", tag);
	} else {
		size = FW_SIZE;
		logError(1, "%s readFwFile: Selected 128k Configuration!\n", tag);
	}

	/* res = parseMemhFile(path, &(fw->data), &(fw->data_size), size); */
	res = parseBinFile(path, &(fw->data), &(fw->data_size), size);
	if (res < OK) {
		logError(1, "%s readFwFile: ERROR %02X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	fw->fw_ver = (u16) (((fw->data[FW_VER_MEMH_BYTE1] & 0x00FF) << 8) + (fw->data[FW_VER_MEMH_BYTE0] & 0x00FF));
	fw->config_id = (u16) (((fw->data[(FW_CODE_SIZE) + FW_OFF_CONFID_MEMH_BYTE1] & 0x00FF) << 8) + (fw->data[(FW_CODE_SIZE) + FW_OFF_CONFID_MEMH_BYTE0] & 0x00FF));

	logError(0, "%s FW VERS File = %04X\n", tag, fw->fw_ver);
	logError(0, "%s CONFIG ID File = %04X\n", tag, fw->config_id);
	return OK;

}

int fillMemory(u32 address, u8 *data, int size)
{

	int remaining = size;
	int toWrite = 0;

	u8 *buff = (u8 *) kmalloc((MEMORY_CHUNK + 3) * sizeof (u8), GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s fillMemory: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
		if (remaining >= MEMORY_CHUNK) {
			if ((address + MEMORY_CHUNK) < FLASH_ADDR_SWITCH_CMD) {
				buff[0] = FLASH_CMD_WRITE_LOWER_64;
				toWrite = MEMORY_CHUNK;
				remaining -= MEMORY_CHUNK;
			} else {
				if (address < FLASH_ADDR_SWITCH_CMD) {
					int delta = FLASH_ADDR_SWITCH_CMD - address;
					buff[0] = FLASH_CMD_WRITE_LOWER_64;
					toWrite = delta;
					remaining -= delta;
				} else {
					buff[0] = FLASH_CMD_WRITE_UPPER_64;
					toWrite = MEMORY_CHUNK;
					remaining -= MEMORY_CHUNK;
				}
			}
		} else {
			if ((address + remaining) < FLASH_ADDR_SWITCH_CMD) {
				buff[0] = FLASH_CMD_WRITE_LOWER_64;
				toWrite = remaining;
				remaining = 0;
			} else {
				if (address < FLASH_ADDR_SWITCH_CMD) {
					int delta = FLASH_ADDR_SWITCH_CMD - address;
					buff[0] = FLASH_CMD_WRITE_LOWER_64;
					toWrite = delta;
					remaining -= delta;
				} else {
					buff[0] = FLASH_CMD_WRITE_UPPER_64;
					toWrite = remaining;
					remaining = 0;
				}
			}
		}

		buff[1] = (u8) ((address & 0x0000FF00) >> 8);
		buff[2] = (u8) (address & 0x000000FF);
		memcpy(buff + 3, data, toWrite);
		logError(0, "%s Command = %02X , address = %02X %02X, bytes = %d\n", tag, buff[0], buff[1], buff[2], toWrite);
		if (fts_writeCmd(buff, 3 + toWrite) < 0) {
			logError(1, "%s fillMemory: ERROR %02X\n", tag, ERROR_I2C_W);
			return ERROR_I2C_W;
		}

		address += toWrite;
		data += toWrite;

	}
	return OK;
}

int flash_burn(Firmware fw, int force_burn)
{
	u8 cmd;
	int res;

	if (!force_burn && (ftsInfo.u16_fwVer >= fw.fw_ver) && (ftsInfo.u16_cfgId >= fw.config_id)) {
		logError(1, "%s flash_burn: Firmware in the chip newer or equal to the one to burn! NO UPDATE ERROR %02X\n", tag, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

	/* programming procedure start */

	logError(0, "%s Programming Procedure for flashing started:\n\n", tag);

	logError(0, "%s 1) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s	system reset FAILED!\n", tag);
		if (res != (ERROR_SYSTEM_RESET_FAIL | ERROR_TIMEOUT)) /* if there is no firmware i will not get the controller ready event and there will be a timeout but i can keep going, but if there is an I2C error i have to exit */
			return (res | ERROR_FLASH_BURN_FAILED);
	} else
		logError(0, "%s   system reset COMPLETED!\n\n", tag);

	logError(0, "%s 2) FLASH UNLOCK:\n", tag);
	res = flash_unlock();
	if (res < 0) {
		logError(1, "%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
		logError(0, "%s   flash unlock COMPLETED!\n\n", tag);

	/* Write the lower part of the Program RAM */
	logError(0, "%s 3) PREPARING DATA FOR FLASH BURN:\n", tag);

	res = fillMemory(FLASH_ADDR_CODE, fw.data, fw.data_size);
	if (res < 0) {
		logError(1, "%s   Error During filling the memory! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   Data copy COMPLETED!\n\n", tag);

	logError(0, "%s 4) ERASE FLASH:\n", tag);
	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s   Flash not ready! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s Command erase ...\n", tag);
	cmd = FLASH_CMD_ERASE;
	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s   Error during erasing flash! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s   Flash not ready 2! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s   Flash erase COMPLETED!\n\n", tag);

	logError(0, "%s 5) BURN FLASH:\n", tag);
	logError(0, "%s Command burn ...\n", tag);
	cmd = FLASH_CMD_BURN;
	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s   Error during burning data! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s   Flash not ready! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s   Flash burn COMPLETED!\n\n", tag);

	logError(0, "%s 6) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s	system reset FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   system reset COMPLETED!\n\n", tag);

	logError(0, "%s 7) FINAL CHECK:\n", tag);
	res = readChipInfo(0);
	if (res < 0) {
		logError(1, "%s flash_burn: Unable to retrieve Chip INFO! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	if ((ftsInfo.u16_fwVer != fw.fw_ver) && (ftsInfo.u16_cfgId != fw.config_id)) {
		logError(1, "%s   Firmware in the chip different from the one that was burn! fw: %x != %x , conf: %x != %x\n", tag, ftsInfo.u16_fwVer, fw.fw_ver, ftsInfo.u16_cfgId, fw.config_id);
		return ERROR_FLASH_BURN_FAILED;
	}

	logError(0, "%s   Final check OK! fw: %02X , conf: %02X\n", tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);

	return OK;
}

int flashProcedure(const char *path, int force, int keep_cx)
{
	Firmware fw;
	int res;

	fw.data = NULL;
	logError(0, "%s Reading Fw file...\n", tag);
	res = readFwFile(path, &fw, keep_cx);
	if (res < OK) {
		logError(1, "%s flashProcedure: ERROR %02X\n", tag, (res | ERROR_FLASH_PROCEDURE));
	kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s Fw file read COMPLETED!\n", tag);

	logError(0, "%s Starting flashing procedure...\n", tag);
	res = flash_burn(fw, force);
	if (res < OK && res != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED)) {
		logError(1, "%s flashProcedure: ERROR %02X\n", tag, ERROR_FLASH_PROCEDURE);
	kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s flashing procedure Finished!\n", tag);
	kfree(fw.data);

	/* cleanUp(0); //after talking with Kusuma, the SenseOn should be issued only at the very end of the initialization process, if senso on here it can trigger autotune protection */
	return res;
}

#else

int wait_for_flash_ready(u8 type)
{
	u8 cmd[2] = {FLASH_CMD_READ_REGISTER, type};
	u8 readData;
	int i, res = -1;

	logError(0, "%s Waiting for flash ready ...\n", tag);
	for (i = 0; i < FLASH_RETRY_COUNT && res != 0; i++) {
		if (fts_readCmd(cmd, sizeof (cmd), &readData, 1) < 0) {
			logError(1, "%s wait_for_flash_ready: ERROR % 02X\n", tag, ERROR_I2C_W);
		} else{
			res = readData & 0x80;
		/* logError(0, "%s flash status = %d \n", tag, res); */
	}
		msleep(FLASH_WAIT_BEFORE_RETRY);
	}

	if (i == FLASH_RETRY_COUNT && res != 0) {
		logError(1, "%s Wait for flash TIMEOUT! ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	logError(0, "%s Flash READY!\n", tag);
	return OK;
}

int fts_warm_boot(void)
{

	u8 cmd[4] = {FTS_CMD_HW_REG_W, 0x00, 0x00, WARM_BOOT_VALUE}; /* write the command to perform the warm boot */
	u16ToU8_be(ADDR_WARM_BOOT, &cmd[1]);

	logError(0, "%s Command warm boot ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s Warm boot DONE!\n", tag);

	return OK;
}

int parseBinFile(const char *pathToFile, Firmware *fwData, int keep_cx)
{

	int fd = -1;
	int dimension, index = 0;
	u32 temp;
	u8 *data;
	int res, i, fw_size;

#ifndef FW_H_FILE
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	dev = getDev();

	if (dev != NULL)
		fd = request_firmware(&fw, pathToFile, dev);
	else {
		logError(1, "%s parseBinFile: No device found! ERROR %02X\n", ERROR_FILE_PARSE);
		return ERROR_FILE_PARSE;
	}

	fw_size = fw->size;
#else
fd = 0;
fw_size = SIZE_NAME;
#endif

	if (fd == 0 && fw_size > 0) {		/* the file should contain at least the header plus the content_crc */
		if (fw_size < FW_HEADER_SIZE+FW_BYTES_ALIGN) {
			logError(1, "%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, FW_HEADER_SIZE+FW_BYTES_ALIGN, ERROR_FILE_PARSE);
		res = ERROR_FILE_PARSE;
		goto END;
		} else {
		/* start parsing of bytes */
#ifndef FW_H_FILE
		data = (u8 *) (fw->data);
#else
		data = (u8 *) (ARRAY_NAME);
#endif
		u8ToU32(&data[index], &temp);
		if (temp != FW_HEADER_SIGNATURE) {
		logError(1, "%s parseBinFile: Wrong Signature %08X ... ERROR %02X\n", tag, temp, ERROR_FILE_PARSE);
		 res = ERROR_FILE_PARSE;
			goto END;
		}
		logError(0, "%s parseBinFile: Fw Signature OK!\n", tag);
		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		if (temp != FW_FTB_VER) {
		logError(1, "%s parseBinFile: Wrong ftb_version %08X ... ERROR %02X\n", tag, temp, ERROR_FILE_PARSE);
		 res = ERROR_FILE_PARSE;
			goto END;
		}
		logError(0, "%s parseBinFile: ftb_version OK!\n", tag);
		index += FW_BYTES_ALIGN;
		if (data[index] != DCHIP_ID_0 || data[index+1] != DCHIP_ID_1) {
		logError(1, "%s parseBinFile: Wrong target %02X != %02X  %02X != %02X ... ERROR %08X\n", tag, data[index], DCHIP_ID_0, data[index+1], DCHIP_ID_1, ERROR_FILE_PARSE);
		res = ERROR_FILE_PARSE;
			goto END;
		}
		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		logError(1, "%s parseBinFile: Fw ID = %08X\n", tag, temp);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->fw_ver = temp;
		logError(1, "%s parseBinFile: FILE Fw Version = %04X\n", tag, fwData->fw_ver);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->config_id = temp;
		logError(1, "%s parseBinFile: FILE Config ID = %08X\n", tag, temp);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		logError(1, "%s parseBinFile: Config Version = %08X\n", tag, temp);

		index += FW_BYTES_ALIGN*2;			/* skip reserved data */

		index += FW_BYTES_ALIGN;
		logError(1, "%s parseBinFile: File External Release =  ", tag);
		for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++) {
			fwData->externalRelease[i] = data[index++];
			logError(1, "%02X ", fwData->externalRelease[i]);
		}
		logError(1, "\n");

		/* index += FW_BYTES_ALIGN; */
		u8ToU32(&data[index], &temp);
		fwData->sec0_size = temp;
		logError(1, "%s parseBinFile:  sec0_size = %08X (%d bytes)\n", tag, fwData->sec0_size, fwData->sec0_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec1_size = temp;
		logError(1, "%s parseBinFile:  sec1_size = %08X (%d bytes)\n", tag, fwData->sec1_size, fwData->sec1_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec2_size = temp;
		logError(1, "%s parseBinFile:  sec2_size = %08X (%d bytes)\n", tag, fwData->sec2_size, fwData->sec2_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec3_size = temp;
		logError(1, "%s parseBinFile:  sec3_size = %08X (%d bytes)\n", tag, fwData->sec3_size, fwData->sec3_size);

		index += FW_BYTES_ALIGN;		/* skip header crc */

		if (!keep_cx) {
				dimension = fwData->sec0_size + fwData->sec1_size + fwData->sec2_size + fwData->sec3_size;
		temp = fw_size;
		} else {
		dimension = fwData->sec0_size + fwData->sec1_size;					/* sec2 may contain cx data (future implementation) sec3 atm not used */
		temp = fw_size - fwData->sec2_size - fwData->sec3_size;
		}

		if (dimension+FW_HEADER_SIZE+FW_BYTES_ALIGN != temp) {
		logError(1, "%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, dimension+FW_HEADER_SIZE+FW_BYTES_ALIGN, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

			fwData->data = (u8 *) kmalloc(dimension * sizeof (u8), GFP_KERNEL);
			if (fwData->data == NULL) {
				logError(1, "%s parseBinFile: ERROR %02X\n", tag, ERROR_ALLOC);
				res = ERROR_ALLOC;
		goto END;
			}

		index += FW_BYTES_ALIGN;
			memcpy(fwData->data, &data[index], dimension);
			fwData->data_size = dimension;

			logError(0, "%s READ FW DONE %d bytes!\n", tag, fwData->data_size);
			res = OK;
		goto END;
		}
	} else {
		logError(1, "%s parseBinFile: File Not Found! ERROR %02X\n", tag, ERROR_FILE_NOT_FOUND);
		return ERROR_FILE_NOT_FOUND;
	}

END:
#ifndef FW_H_FILE
	release_firmware(fw);
#endif
		return res;
}

int readFwFile(const char *path, Firmware *fw, int keep_cx)
{
	int res;

	res = parseBinFile(path, fw, keep_cx);
	if (res < OK) {
		logError(1, "%s readFwFile: ERROR %02X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	return OK;

}

int flash_unlock(void)
{
	u8 cmd[3] = {FLASH_CMD_UNLOCK, FLASH_UNLOCK_CODE0, FLASH_UNLOCK_CODE1}; /* write the command to perform the unlock */

	logError(0, "%s Command unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	/* msleep(FLASH_WAIT_TIME); */
	logError(0, "%s Unlock flash DONE!\n", tag);

	return OK;

}

int flash_erase_unlock(void)
{
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_UNLOCK_CODE0, FLASH_ERASE_UNLOCK_CODE1}; /* write the command to perform the unlock for erasing the flash */

	logError(0, "%s Try to erase unlock flash...\n", tag);

	logError(0, "%s Command erase unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s flash_erase_unlock: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s Erase Unlock flash DONE!\n", tag);

	return OK;

}

int flash_full_erase(void)
{

	int status;
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_CODE0, FLASH_ERASE_CODE1};
	/* write the command to erase the flash */

	logError(0, "%s Command full erase sent ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s flash_full_erase: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_ERASE_CODE0);

	if (status != OK) {
		logError(1, "%s flash_full_erase: ERROR % 02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
		/* Flash not ready within the chosen time, better exit! */
	}

	logError(0, "%s Full Erase flash DONE!\n", tag);

	return OK;

}

int start_flash_dma(void)
{
	int status;
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_DMA_CODE0, FLASH_DMA_CODE1};
	/* write the command to erase the flash */

	logError(0, "%s Command flash DMA ...\n", tag);
	if (fts_writeCmd(cmd, sizeof (cmd)) < 0) {
		logError(1, "%s start_flash_dma: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_DMA_CODE0);

	if (status != OK) {
		logError(1, "%s start_flash_dma: ERROR % 02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
		/* Flash not ready within the chosen time, better exit! */
	}

	logError(0, "%s flash DMA DONE!\n", tag);

	return OK;
}

int fillFlash(u32 address, u8 *data, int size)
{

	int remaining = size;
	int toWrite = 0;
	int byteBlock = 0;
	int wheel = 0;
	u32 addr = 0;
	int res;
	int delta;

	u8 *buff = (u8 *) kmalloc((DMA_CHUNK + 3) * sizeof (u8), GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s fillFlash: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
	byteBlock = 0;
		addr = 0;
		while (byteBlock < FLASH_CHUNK && remaining > 0) {
			buff[0] = FLASH_CMD_WRITE_64K;
			if (remaining >= DMA_CHUNK) {
				if ((byteBlock + DMA_CHUNK) <= FLASH_CHUNK) {
			/* logError(1, "%s fillFlash: 1\n", tag); */
					toWrite = DMA_CHUNK;
					remaining -= DMA_CHUNK;
					byteBlock += DMA_CHUNK;
				} else {
			/* logError(1, "%s fillFlash: 2\n", tag); */
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			} else {
				if ((byteBlock + remaining) <= FLASH_CHUNK) {
			/* logError(1, "%s fillFlash: 3\n", tag); */
					toWrite = remaining;
			byteBlock += remaining;
					remaining = 0;

				} else {
			/* logError(1, "%s fillFlash: 4\n", tag); */
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			}

			buff[1] = (u8) ((addr & 0x0000FF00) >> 8);
			buff[2] = (u8) (addr & 0x000000FF);
			memcpy(&buff[3], data, toWrite);
			if (fts_writeCmd(buff, 3 + toWrite) < 0) {
				logError(1, "%s fillFlash: ERROR %02X\n", tag, ERROR_I2C_W);
				return ERROR_I2C_W;
			}

			addr += toWrite;
			data += toWrite;
		}

		kfree(buff);

		/* configuring the DMA */
		byteBlock = byteBlock / 4 - 1;

		buff = (u8 *) kmalloc((9) * sizeof (u8), GFP_KERNEL);
		buff[0] = FLASH_CMD_WRITE_REGISTER;
		buff[1] = FLASH_DMA_CONFIG;
		buff[2] = 0x00;
		buff[3] = 0x00;

	addr = address + ((wheel * FLASH_CHUNK)/4);
		buff[4] = (u8) ((addr & 0x000000FF));
		buff[5] = (u8) ((addr & 0x0000FF00) >> 8);
		buff[6] = (u8) (byteBlock & 0x000000FF);
		buff[7] = (u8) ((byteBlock & 0x0000FF00) >> 8);
	buff[8] = 0x00;

	logError(0, "%s Command = %02X , address = %02X %02X, words =  %02X %02X\n", tag, buff[0], buff[5], buff[4], buff[7], buff[6]);
		if (fts_writeCmd(buff, 9) < OK) {
			logError(1, "%s   Error during filling Flash! ERROR %02X\n", tag, ERROR_I2C_W);
			return ERROR_I2C_W;
		}

	/* msleep(FLASH_WAIT_TIME); */
		res = start_flash_dma();
		if (res < OK) {
			logError(1, "%s   Error during flashing DMA! ERROR %02X\n", tag, res);
			return res;
		}
		wheel++;
	}
	return OK;
}

int flash_burn(Firmware fw, int force_burn)
{
	int res;

	if (!force_burn) {
	for (res = EXTERNAL_RELEASE_INFO_SIZE-1; res >= 0; res--) {
		if (fw.externalRelease[res] > ftsInfo.u8_extReleaseInfo[res])
			goto start;
	}
		logError(1, "%s flash_burn: Firmware in the chip newer or equal to the one to burn! NO UPDATE ERROR %02X\n", tag, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

	/* programming procedure start */
start:
	logError(0, "%s Programming Procedure for flashing started:\n\n", tag);

	logError(0, "%s 1) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s	system reset FAILED!\n", tag);
		if (res != (ERROR_SYSTEM_RESET_FAIL | ERROR_TIMEOUT))
		/* if there is no firmware i will not get the controller
		*ready event and there will be a timeout but i can keep going,
		*but if there is an I2C error i have to exit
		*/
			return (res | ERROR_FLASH_BURN_FAILED);
	} else
		logError(0, "%s   system reset COMPLETED!\n\n", tag);

	logError(0, "%s 2) WARM BOOT:\n", tag);
	res = fts_warm_boot();
	if (res < OK) {
		logError(1, "%s	warm boot FAILED!\n", tag);
	return (res | ERROR_FLASH_BURN_FAILED);
	} else
		logError(0, "%s	warm boot COMPLETED!\n\n", tag);

	/* msleep(FLASH_WAIT_TIME); */
	logError(0, "%s 3) FLASH UNLOCK:\n", tag);
	res = flash_unlock();
	if (res < OK) {
		logError(1, "%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
		logError(0, "%s   flash unlock COMPLETED!\n\n", tag);

	/* msleep(200); */
	logError(0, "%s 4) FLASH ERASE UNLOCK:\n", tag);
	res = flash_erase_unlock();
	if (res < 0) {
		logError(1, "%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
		logError(0, "%s   flash unlock COMPLETED!\n\n", tag);

	/* msleep(FLASH_WAIT_TIME); */
	logError(0, "%s 5) FLASH ERASE:\n", tag);
	res = flash_full_erase();
	if (res < 0) {
		logError(1, "%s   flash erase FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
		logError(0, "%s   flash erase COMPLETED!\n\n", tag);

	/* msleep(FLASH_WAIT_TIME); */
	logError(0, "%s 6) LOAD PROGRAM:\n", tag);
	res = fillFlash(FLASH_ADDR_CODE, &fw.data[0], fw.sec0_size);
	if (res < OK) {
		logError(1, "%s   load program ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(1, "%s   load program DONE!\n", tag);

	logError(0, "%s 7) LOAD CONFIG:\n", tag);
	res = fillFlash(FLASH_ADDR_CONFIG, &(fw.data[fw.sec0_size]), fw.sec1_size);
	if (res < OK) {
		logError(1, "%s   load config ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	 logError(1, "%s   load config DONE!\n", tag);

	logError(0, "%s   Flash burn COMPLETED!\n\n", tag);

	logError(0, "%s 8) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s	system reset FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   system reset COMPLETED!\n\n", tag);

	logError(0, "%s 9) FINAL CHECK:\n", tag);
	res = readChipInfo(0);
	if (res < 0) {
		logError(1, "%s flash_burn: Unable to retrieve Chip INFO! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	for (res = 0; res < EXTERNAL_RELEASE_INFO_SIZE; res++) {
		if (fw.externalRelease[res] != ftsInfo.u8_extReleaseInfo[res]) {
		/* external release is prined during readChipInfo */
				logError(1, "%s  Firmware in the chip different from the one that was burn! fw: %x != %x , conf: %x != %x\n", tag, ftsInfo.u16_fwVer, fw.fw_ver, ftsInfo.u16_cfgId, fw.config_id);
				return ERROR_FLASH_BURN_FAILED;
		}
	}

	logError(0, "%s   Final check OK! fw: %02X, conf: %02X\n", tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);

	return OK;
}

int flashProcedure(const char *path, int force, int keep_cx)
{
	Firmware fw;
	int res;

	fw.data = NULL;
	logError(0, "%s Reading Fw file...\n", tag);
	res = readFwFile(path, &fw, keep_cx);
	if (res < OK) {
		logError(1, "%s flashProcedure: ERROR %02X\n", tag, (res | ERROR_FLASH_PROCEDURE));
	kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s Fw file read COMPLETED!\n", tag);

	logError(0, "%s Starting flashing procedure...\n", tag);
	res = flash_burn(fw, force);
	if (res < OK && res != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED)) {
		logError(1, "%s flashProcedure: ERROR %02X\n", tag, ERROR_FLASH_PROCEDURE);
	kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s flashing procedure Finished!\n", tag);
	kfree(fw.data);
	return res;
}

#endif
