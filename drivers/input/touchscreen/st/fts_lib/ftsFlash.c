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
 *                        FTS API for Flashing the IC                     *
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
#include "../fts.h"//needed for including the define FW_H_FILE

#ifdef FW_H_FILE
#include <../fts_fw.h>
#define LOAD_FW_FROM 1
#else
#define LOAD_FW_FROM 0
#endif

#define FTS_LATEST_VERSION 0x1101

static char tag[8] = "[ FTS ]\0";

int getFirmwareVersion(u16 *fw_vers, u16 *config_id)
{
	u8 fwvers[DCHIP_FW_VER_BYTE];
	u8 confid[CONFIG_ID_BYTE];
	int res;

	res = readCmdU16(FTS_CMD_HW_REG_R, DCHIP_FW_VER_ADDR,
			fwvers, DCHIP_FW_VER_BYTE, DUMMY_HW_REG);
	if (res < OK) {
		logError(1,
		"%s %s:unable to read fw_version ERROR %02X\n",
		tag, __func__, ERROR_FW_VER_READ);
		return (res | ERROR_FW_VER_READ);
	}

	u8ToU16(fwvers, fw_vers); //fw version use big endian
	if (*fw_vers != 0) {
	// if fw_version is 00 00 means that there is
	//no firmware running in the chip therefore will be
	//impossible find the config_id
		res = readB2(CONFIG_ID_ADDR, confid, CONFIG_ID_BYTE);
		if (res < OK) {
			logError(1, "%s %s:unable to read config_id ",
				tag, __func__);
			logError(1, "ERROR %02X\n", ERROR_FW_VER_READ);
			return (res | ERROR_FW_VER_READ);
		}
		u8ToU16(confid, config_id); //config id use little endian
	} else {
		*config_id = 0x0000;
	}

	logError(0, "%s FW VERS = %04X\n", tag, *fw_vers);
	logError(0, "%s CONFIG ID = %04X\n", tag, *config_id);
	return OK;
}

int getFWdata(const char *pathToFile, u8 **data, int *size, int from)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int res;

	logError(0, "%s %s starting...\n", tag, __func__);
	switch (from) {
#ifdef FW_H_FILE
	case 1:
		logError(1, "%s Read FW from .h file!\n", tag);
		*size = FW_SIZE_NAME;
		*data = (u8 *)kmalloc_array((*size), sizeof(u8), GFP_KERNEL);
		if (*data == NULL) {
			logError(1, "%s %s:Impossible to allocate memory! ",
				tag, __func__);
			logError(1, "ERROR %08X\n", ERROR_ALLOC);

			return ERROR_ALLOC;
		}
		memcpy(*data, (u8 *)FW_ARRAY_NAME, (*size));
		break;
#endif
	default:
		logError(0, "%s Read FW from BIN file!\n", tag);

		if (ftsInfo.u16_fwVer == FTS_LATEST_VERSION)
			return ERROR_FW_NO_UPDATE;

		dev = getDev();

		if (dev != NULL) {
			res = firmware_request_nowarn(&fw, pathToFile, dev);
			if (res == 0) {
				*size = fw->size;
				*data = (u8 *)kmalloc_array((*size), sizeof(u8),
					GFP_KERNEL);
				if (*data == NULL) {
					logError(1, "%s %s:Impossible to ",
						tag, __func__);
					logError(1, "%allocate! %08X\n",
						ERROR_ALLOC);
					release_firmware(fw);
					return ERROR_ALLOC;
				}
				memcpy(*data, (u8 *)fw->data, (*size));
				release_firmware(fw);
			} else {
				logError(0, "%s %s:No File found! ERROR %08X\n",
					tag, __func__, ERROR_FILE_NOT_FOUND);
				return ERROR_FILE_NOT_FOUND;
			}
		} else {
			logError(1, "%s %s:No device found! ERROR %08X\n",
			tag, __func__, ERROR_OP_NOT_ALLOW);
			return ERROR_OP_NOT_ALLOW;
		}
		/* break; */
	}

	logError(0, "%s %s:Finshed!\n", tag, __func__);
	return OK;
}

int readFwFile(const char *path, struct Firmware *fw, int keep_cx)
{
	int res;
	int orig_size;
	u8 *orig_data = NULL;


	res = getFWdata(path, &orig_data, &orig_size, LOAD_FW_FROM);
	if (res < OK) {
		logError(0, "%s %s:impossible retrieve FW... ERROR %08X\n",
			tag, __func__, ERROR_MEMH_READ);

		return (res | ERROR_MEMH_READ);
	}
	res = parseBinFile(orig_data, orig_size, fw, keep_cx);

	if (res < OK) {
		logError(1, "%s %s:impossible parse ERROR %08X\n",
			tag, __func__, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	return OK;
}

int flashProcedure(const char *path, int force, int keep_cx)
{
	struct Firmware fw;
	int res;

	fw.data = NULL;
	logError(0, "%s Reading Fw file...\n", tag);
	res = readFwFile(path, &fw, keep_cx);
	if (res < OK) {
		logError(0, "%s %s: ERROR %02X\n",
			tag, __func__, (res | ERROR_FLASH_PROCEDURE));
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s Fw file read COMPLETED!\n", tag);

	logError(0, "%s Starting flashing procedure...\n", tag);
	res = flash_burn(&fw, force, keep_cx);
	if (res < OK && res != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED)) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_FLASH_PROCEDURE);
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}
	logError(0, "%s flashing procedure Finished!\n", tag);
	kfree(fw.data);

	return res;
}

#ifdef FTM3_CHIP
int flash_status(void)
{
	u8 cmd[2] = {FLASH_CMD_READSTATUS, 0x00};
	u8 readData = 0;

	logError(0, "%s %s:Reading ...\n", tag, __func__);
	if (fts_readCmd(cmd, 2, &readData, FLASH_STATUS_BYTES) < 0) {
		logError(1, "%s %s: ERROR % 02X\n", tag, __func__, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	readData &= 0x01;
	logError(0, "%s %s = %d\n", tag, __func__, readData);
	return (int) readData;
}

int flash_status_ready(void)
{

	int status = flash_status();

	if (status == ERROR_I2C_R) {
		logError(1, "%s %s: ERROR % 02X\n", tag, __func__, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	if (status != FLASH_READY) {
		//logError(1,
		//"%s %s:flash busy or unknown STATUS = % 02X\n",
		//tag, status);
		return ERROR_FLASH_UNKNOWN;
	}

	return FLASH_READY;
}

int wait_for_flash_ready(void)
{
	int status;
	int (*code)(void);

	code = flash_status_ready;

	logError(0, "%s Waiting for flash ready...\n", tag);
	status = attempt_function(code, FLASH_WAIT_BEFORE_RETRY,
			FLASH_RETRY_COUNT);

	if (status != FLASH_READY) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	logError(0, "%s Flash ready!\n", tag);
	return OK;
}

int flash_unlock(void)
{
	int status;
	//write the command to perform the unlock
	u8 cmd[3] = {FLASH_CMD_UNLOCK, FLASH_UNLOCK_CODE0,
		FLASH_UNLOCK_CODE1};

	logError(0, "%s Try to unlock flash...\n", tag);
	status = wait_for_flash_ready();

	if (status != OK) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_FLASH_NOT_READY);
		//Flash not ready within the chosen time, better exit!
		return (status | ERROR_FLASH_NOT_READY);
	}

	logError(0, "%s Command unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready();

	if (status != OK) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_FLASH_NOT_READY);
		//Flash not ready within the chosen time,
		//better exit!
		return (status | ERROR_FLASH_NOT_READY);
	}
	logError(0, "%s Unlock flash DONE!\n", tag);

	return OK;
}

int parseBinFile(u8 *fw_data, int fw_size, Firmware *fwData, int keep_cx)
{
	int dimension;

	if (keep_cx) {
		dimension = FW_SIZE - FW_CX_SIZE;
		logError(1, "%s %s: Selected 124k Configuration!\n",
			tag, __func__);
	} else {
		dimension = FW_SIZE;
		logError(1, "%s %s: Selected 128k Configuration!\n",
			tag, __func__);
	}

	if (fw_size - FW_HEADER_SIZE != FW_SIZE || fw_data == NULL) {
		logError(1, "%s %s:Read only %d instead of %d... ERROR %02X\n",
			tag, __func__,
			fw_size - FW_HEADER_SIZE,
			FW_SIZE, ERROR_FILE_PARSE);
		kfree(fw_data);
		return ERROR_FILE_PARSE;
	}

	fwData->data = (u8 *)kmalloc_array(dimension, sizeof(u8), GFP_KERNEL);
	if (fwData->data == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);

		kfree(fw_data);
		return ERROR_ALLOC;
	}

	memcpy(fwData->data, ((u8 *)(fw_data) + FW_HEADER_SIZE),
				dimension);
	fwData->data_size = dimension;

	fwData->fw_ver = (u16)(((fwData->data[FW_VER_MEMH_BYTE1] & 0x00FF) << 8)
				+ (fwData->data[FW_VER_MEMH_BYTE0] & 0x00FF));

	fwData->config_id = (u16)(((fwData->data[(FW_CODE_SIZE)
				+ FW_OFF_CONFID_MEMH_BYTE1] & 0x00FF) << 8)
			+ (fwData->data[(FW_CODE_SIZE) +
				FW_OFF_CONFID_MEMH_BYTE0] & 0x00FF));

	logError(0, "%s %s: FW VERS File = %04X\n",
			tag, __func__, fwData->fw_ver);
	logError(0, "%s %s: CONFIG ID File = %04X\n",
			tag, __func__, fwData->config_id);

	logError(0, "%s READ FW DONE %d bytes!\n", tag, fwData->data_size);

	kfree(fw_data);
		return OK;
}

int fillMemory(u32 address, u8 *data, int size)
{
	int remaining = size;
	int toWrite = 0;
	int delta;

	u8 *buff = (u8 *)kmalloc_array((MEMORY_CHUNK + 3), sizeof(u8),
				GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);
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
					delta = FLASH_ADDR_SWITCH_CMD - address;

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
					delta = FLASH_ADDR_SWITCH_CMD - address;

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
		//logError(0,
		//"%s Command = %02X , address = %02X %02X, bytes = %d\n",
		//tag, buff[0], buff[1], buff[2], toWrite);
		if (fts_writeCmd(buff, 3 + toWrite) < 0) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
			kfree(buff);
			return ERROR_I2C_W;
		}
		address += toWrite;
		data += toWrite;
	}
	kfree(buff);
	return OK;
}

int flash_burn(Firmware *fw, int force_burn, int keep_cx)
{
	u8 cmd;
	int res;

	if (!force_burn && (ftsInfo.u16_fwVer >= fw->fw_ver)
		&& (ftsInfo.u16_cfgId >= fw->config_id)) {
		logError(0, "Firmware in the chip newer");
		logError(0, " or equal to the one to burn! ");
		logError(0, "%s %s:NO UPDATE ERROR %02X\n",
			tag, __func__, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

	//programming procedure start

	logError(0, "%s Programming Procedure for flashing started:\n", tag);

	logError(0, "%s 1) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s system reset FAILED!\n", tag);
		//if there is no firmware i will not
		//get the controller ready event and
		//there will be a timeout but i can
		//keep going, but if there is
		//an I2C error i have to exit
		if (res != (ERROR_SYSTEM_RESET_FAIL | ERROR_TIMEOUT))
			return (res | ERROR_FLASH_BURN_FAILED);
	} else
		logError(0, "%s system reset COMPLETED!\n\n", tag);

	logError(0, "%s 2) FLASH UNLOCK:\n", tag);
	res = flash_unlock();
	if (res < 0) {
		logError(1, "%s flash unlock FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s flash unlock COMPLETED!\n\n", tag);


	//Write the lower part of the Program RAM
	logError(0, "%s 3) PREPARING DATA FOR FLASH BURN:\n", tag);

	res = fillMemory(FLASH_ADDR_CODE, fw->data, fw->data_size);
	if (res < 0) {
		logError(1, "%s Error During filling the memory!%02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s Data copy COMPLETED!\n\n", tag);

	logError(0, "%s 4) ERASE FLASH:\n", tag);
	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s Flash not ready! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s Command erase ...\n", tag);
	cmd = FLASH_CMD_ERASE;
	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s Error during erasing flash! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s Flash not ready 2! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s Flash erase COMPLETED!\n\n", tag);

	logError(0, "%s 5) BURN FLASH:\n", tag);
	logError(0, "%s Command burn ...\n", tag);
	cmd = FLASH_CMD_BURN;
	if (fts_writeCmd(&cmd, 1) < 0) {
		logError(1, "%s Error during burning data! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();
	if (res < 0) {
		logError(1, "%s Flash not ready! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	logError(0, "%s Flash burn COMPLETED!\n\n", tag);

	logError(0, "%s 6) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s system reset FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s system reset COMPLETED!\n\n", tag);


	logError(0, "%s 7) FINAL CHECK:\n", tag);
	res = readChipInfo(0);
	if (res < 0) {
		logError(1, "%s %s:Unable to retrieve Chip INFO!%02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	if ((ftsInfo.u16_fwVer != fw->fw_ver)
		&& (ftsInfo.u16_cfgId != fw->config_id)) {
		logError(1, "Firmware in the chip different");
		logError(1, " from the one that was burn!");
		logError(1, "%s fw: %x != %x , conf: %x != %x\n",
			tag, ftsInfo.u16_fwVer,
			fw->fw_ver,
			ftsInfo.u16_cfgId,
			fw->config_id);
		return ERROR_FLASH_BURN_FAILED;
	}

	logError(0, "%s Final check OK! fw: %02X, conf: %02X\n",
		tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);

	return OK;
}

#else

int wait_for_flash_ready(u8 type)
{
	u8 cmd[2] = {FLASH_CMD_READ_REGISTER, type};
	u8 readData = 0;
	int i, res = -1;

	logError(0, "%s Waiting for flash ready ...\n", tag);
	for (i = 0; i < FLASH_RETRY_COUNT && res != 0; i++) {
		if (fts_readCmd(cmd, sizeof(cmd), &readData, 1) < 0) {
			logError(1, "%s %s: ERROR % 02X\n",
				tag, __func__, ERROR_I2C_R);
		} else {
			res = readData & 0x80;
			//logError(0, "%s flash status = %d\n", tag, res);
		}
		msleep(FLASH_WAIT_BEFORE_RETRY);
	}

	if (i == FLASH_RETRY_COUNT && res != 0) {
		logError(1, "%s Wait for flash TIMEOUT! ERROR %02X\n",
			tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	logError(0, "%s Flash READY!\n", tag);
	return OK;
}

int fts_warm_boot(void)
{
	//write the command to perform the warm boot
	u8 cmd[4] = {FTS_CMD_HW_REG_W, 0x00, 0x00, WARM_BOOT_VALUE};

	u16ToU8_be(ADDR_WARM_BOOT, &cmd[1]);

	logError(0, "%s Command warm boot ...\n", tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s flash_unlock: ERROR % 02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s Warm boot DONE!\n", tag);

	return OK;
}

int parseBinFile(u8 *data, int fw_size,
		struct Firmware *fwData, int keep_cx)
{
	int dimension, index = 0;
	u32 temp;
	int res, i;

	//the file should contain at least the header plus the content_crc
	if (fw_size < FW_HEADER_SIZE+FW_BYTES_ALIGN || data == NULL) {
		logError(1, "%s %s:Read only %d instead of %d...ERROR %02X\n",
			tag, __func__, fw_size,
			FW_HEADER_SIZE + FW_BYTES_ALIGN,
			ERROR_FILE_PARSE);
		res = ERROR_FILE_PARSE;
		goto END;
	} else {
		//start parsing of bytes
		u8ToU32(&data[index], &temp);
		if (temp != FW_HEADER_SIGNATURE) {
			logError(1, "%s %s:Wrong Signature %08X...ERROR %02X\n",
				tag, __func__, temp, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

		logError(0, "%s %s: Fw Signature OK!\n", tag, __func__);
		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		if (temp != FW_FTB_VER) {
			logError(1, "%s %s:Wrong ftb_version %08X.ERROR %02X\n",
			tag, __func__, temp, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}
		logError(0, "%s %s:ftb_version OK!\n", __func__, tag);
		index += FW_BYTES_ALIGN;
		if (data[index] != DCHIP_ID_0 || data[index+1] != DCHIP_ID_1) {
			logError(1, "%s %s:Wrong target %02X != %02X ",
				tag, __func__, data[index]);
			logError(1, "%%02X != %02X:%08X\n",
				DCHIP_ID_0, data[index+1],
				DCHIP_ID_1, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}
		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		logError(0, "%s %s: Fw ID = %08X\n", tag, __func__, temp);
		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->fw_ver = temp;
		logError(0, "%s %s:FILE Fw Version = %04X\n",
			tag, __func__, fwData->fw_ver);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->config_id = temp;
		logError(0, "%s %s:FILE Config ID = %04X\n",
			tag, __func__, fwData->config_id);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		logError(0, "%s %s:Config Version = %08X\n",
			tag, __func__, temp);
		//skip reserved data
		index += FW_BYTES_ALIGN * 2;
		index += FW_BYTES_ALIGN;
		logError(0, "%s %s:File External Release =  ",
			tag, __func__);
		for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++) {
			fwData->externalRelease[i] = data[index++];
			logError(0, "%02X", fwData->externalRelease[i]);
		}
		logError(0, "\n");

		//index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec0_size = temp;
		logError(0, "%s %s:sec0_size = %08X (%d bytes)\n",
			tag, __func__, fwData->sec0_size, fwData->sec0_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec1_size = temp;
		logError(0, "%s %s:sec1_size = %08X (%d bytes)\n",
			tag, __func__, fwData->sec1_size, fwData->sec1_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec2_size = temp;
		logError(0, "%s %s:sec2_size = %08X (%d bytes)\n",
			tag, __func__, fwData->sec2_size, fwData->sec2_size);

		index += FW_BYTES_ALIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec3_size = temp;
		logError(0, "%s %s:sec3_size = %08X (%d bytes)\n",
			tag, __func__, fwData->sec3_size, fwData->sec3_size);

		//skip header crc
		index += FW_BYTES_ALIGN;
		if (!keep_cx) {
			dimension = fwData->sec0_size + fwData->sec1_size
				+ fwData->sec2_size + fwData->sec3_size;
			temp = fw_size;
		} else {
			//sec2 may contain cx data (future implementation)
			//sec3 atm not used
			dimension = fwData->sec0_size + fwData->sec1_size;
			temp = fw_size - fwData->sec2_size - fwData->sec3_size;
			fwData->sec2_size = 0;
			fwData->sec3_size = 0;
		}
		if (dimension + FW_HEADER_SIZE + FW_BYTES_ALIGN != temp) {
			logError(1, "%s %s:Read only %d instead of %d...",
				tag, __func__, fw_size,
				dimension + FW_HEADER_SIZE + FW_BYTES_ALIGN);
			logError(1, "ERROR %02X\n", ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}
		fwData->data = (u8 *)kmalloc_array(dimension, sizeof(u8),
					GFP_KERNEL);
		if (fwData->data == NULL) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_ALLOC);
			res = ERROR_ALLOC;
			goto END;
		}
		index += FW_BYTES_ALIGN;
		memcpy(fwData->data, &data[index], dimension);
		fwData->data_size = dimension;
		logError(0, "%s READ FW DONE %d bytes!\n",
			tag, fwData->data_size);
		res = OK;
		goto END;
	}
END:
	kfree(data);
	return res;
}

int flash_unlock(void)
{
	//write the command to perform the unlock
	u8 cmd[3] = {FLASH_CMD_UNLOCK,
		FLASH_UNLOCK_CODE0, FLASH_UNLOCK_CODE1};
	logError(0, "%s Command unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s %s: ERROR % 02X\n", tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	//mdelay(FLASH_WAIT_TIME);
	logError(0, "%s Unlock flash DONE!\n", tag);

	return OK;
}

int flash_erase_unlock(void)
{
	//write the command to perform
	//the unlock for erasing the flash
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_UNLOCK_CODE0,
		FLASH_ERASE_UNLOCK_CODE1};

	logError(0, "%s Try to erase unlock flash...\n", tag);

	logError(0, "%s Command erase unlock ...\n", tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s %s:ERROR % 02X\n", tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	logError(0, "%s Erase Unlock flash DONE!\n", tag);

	return OK;
}

int flash_full_erase(void)
{
	int status;
	//write the command to erase the flash
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_CODE0,
			FLASH_ERASE_CODE1};

	logError(0, "%s Command full erase sent...\n",
		tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s %s:ERROR % 02X\n", tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_ERASE_CODE0);

	if (status != OK) {
		logError(1, "%s %s:ERROR % 02X\n",
			tag, __func__, ERROR_FLASH_NOT_READY);
		//Flash not ready within the chosen time,
		//better exit!
		return (status | ERROR_FLASH_NOT_READY);
	}

	logError(0, "%s Full Erase flash DONE!\n", tag);

	return OK;
}

int flash_erase_page_by_page(int keep_cx)
{
	u8 status, i = 0;
	//write the command to erase the flash
	u8 cmd[4] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_CODE0, 0x00, 0x00};

	for (i = 0; i < FLASH_NUM_PAGE; i++) {
		if (i >= FLASH_CX_PAGE_START && i <= FLASH_CX_PAGE_END
			&& keep_cx == 1) {
			logError(0, "%s Skipping erase page %d!\n", tag, i);
			continue;
		}
		cmd[2] = (0x3F & i) | FLASH_ERASE_START;
		logError(0, "Command erase page %d sent", i);
		logError(0, "%s:%02X %02X %02X %02X\n",
			tag, i, cmd[0], cmd[1], cmd[2], cmd[3]);
		if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
			logError(1,
			"%s %s:ERROR % 08X\n",
			tag, __func__, ERROR_I2C_W);
			return ERROR_I2C_W;
		}

		status = wait_for_flash_ready(FLASH_ERASE_CODE0);
		if (status != OK) {
			logError(1, "%s %s:ERROR % 08X\n",
				tag, __func__, ERROR_FLASH_NOT_READY);
			//Flash not ready within the chosen time,
			//better exit!
			return (status | ERROR_FLASH_NOT_READY);
		}
	}

	logError(0, "%s Erase flash page by page DONE!\n", tag);

	return OK;
}

int start_flash_dma(void)
{
	int status;
	//write the command to erase the flash
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_DMA_CODE0,
			FLASH_DMA_CODE1};

	logError(0, "%s Command flash DMA ...\n", tag);
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_DMA_CODE0);

	if (status != OK) {
		logError(1, "%s %s: ERROR % 02X\n",
			tag, __func__, ERROR_FLASH_NOT_READY);
		//Flash not ready within the chosen time, better exit!
		return (status | ERROR_FLASH_NOT_READY);
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
	u8 *buff = NULL;
	u8 buff2[9] = {0};


	buff = (u8 *)kmalloc_array((DMA_CHUNK + 3), sizeof(u8), GFP_KERNEL);
	if (buff == NULL) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
		byteBlock = 0;
		addr = 0;
		while (byteBlock < FLASH_CHUNK && remaining > 0) {
			buff[0] = FLASH_CMD_WRITE_64K;
			if (remaining >= DMA_CHUNK) {
				if ((byteBlock + DMA_CHUNK) <= FLASH_CHUNK) {
					//logError(1, "%s fillFlash:1\n", tag);
					toWrite = DMA_CHUNK;
					remaining -= DMA_CHUNK;
					byteBlock += DMA_CHUNK;
				} else {
					//logError(1, "%s fillFlash:2\n", tag);
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			} else {
				if ((byteBlock + remaining) <= FLASH_CHUNK) {
					//logError(1, "%s fillFlash:3\n", tag);
					toWrite = remaining;
					byteBlock += remaining;
					remaining = 0;
				} else {
					//logError(1, "%s fillFlash:4\n", tag);
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			}

			buff[1] = (u8) ((addr & 0x0000FF00) >> 8);
			buff[2] = (u8) (addr & 0x000000FF);
			memcpy(&buff[3], data, toWrite);
			//logError(0,
			//"%s Command = %02X, address = %02X %02X,
			//bytes = %d, data = %02X %02X, %02X %02X\n",
			//tag, buff[0], buff[1], buff[2], toWrite,
			//buff[3], buff[4], buff[3 + toWrite-2],
			//buff[3 + toWrite-1]);
			if (fts_writeCmd(buff, 3 + toWrite) < 0) {
				logError(1, "%s %s: ERROR %02X\n",
					tag, __func__, ERROR_I2C_W);
				kfree(buff);
				return ERROR_I2C_W;
			}

			addr += toWrite;
			data += toWrite;
		}
		//configuring the DMA
		byteBlock = byteBlock / 4 - 1;

		buff2[0] = FLASH_CMD_WRITE_REGISTER;
		buff2[1] = FLASH_DMA_CONFIG;
		buff2[2] = 0x00;
		buff2[3] = 0x00;

		addr = address + ((wheel * FLASH_CHUNK)/4);
		buff2[4] = (u8) ((addr & 0x000000FF));
		buff2[5] = (u8) ((addr & 0x0000FF00) >> 8);
		buff2[6] = (u8) (byteBlock & 0x000000FF);
		buff2[7] = (u8) ((byteBlock & 0x0000FF00) >> 8);
		buff2[8] = 0x00;

		logError(0, "%s:Command:%02X, address:%02X %02X, ",
			tag, buff2[0], buff2[5], buff2[4]);
		logError(0, "words:%02X %02X\n", buff2[7], buff2[6]);
		if (fts_writeCmd(buff2, 9) < OK) {
			logError(1, "%s Error during filling Flash!:%02X\n",
				tag, ERROR_I2C_W);
			kfree(buff);
			return ERROR_I2C_W;
		}
		//mdelay(FLASH_WAIT_TIME);
		res = start_flash_dma();
		if (res < OK) {
			logError(1, "%s Error during flashing DMA!:%02X\n",
				tag, res);
			kfree(buff);
			return res;
		}
		wheel++;
	}
	kfree(buff);
	return OK;
}

int flash_burn(struct Firmware *fw, int force_burn, int keep_cx)
{
	int res;

	if (!force_burn && (ftsInfo.u16_fwVer >= fw->fw_ver)
		&& (ftsInfo.u16_cfgId >= fw->config_id)) {
		for (res = EXTERNAL_RELEASE_INFO_SIZE-1; res >= 0; res--) {
			if (fw->externalRelease[res] >
				ftsInfo.u8_extReleaseInfo[res])
				goto start;
		}

		logError(0, "Firmware in the chip newer or ");
		logError(0, "equal to the one to burn!");
		logError(0, "%s %s:NO UPDATE ERROR %02X\n",
			tag, __func__, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

	//programming procedure start
start:
	logError(0, "%s Programming Procedure for flashing started:\n\n", tag);

	logError(0, "%s 1) SYSTEM RESET:\n", tag);

	logError(0, "%s 2) WARM BOOT:\n", tag);
	res = fts_warm_boot();
	if (res < OK) {
		logError(1, "%s warm boot FAILED!\n", tag);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s warm boot COMPLETED!\n\n", tag);

	//mdelay(FLASH_WAIT_TIME);
	logError(0, "%s 3) FLASH UNLOCK:\n", tag);
	res = flash_unlock();
	if (res < OK) {
		logError(1, "%s flash unlock FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s flash unlock COMPLETED!\n\n", tag);

	//mdelay(200);
	logError(0, "%s 4) FLASH ERASE UNLOCK:\n", tag);
	res = flash_erase_unlock();
	if (res < 0) {
		logError(1, "%s flash unlock FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s flash unlock COMPLETED!\n\n", tag);

	//mdelay(FLASH_WAIT_TIME);
	logError(0, "%s 5) FLASH ERASE:\n", tag);
	if (keep_cx == 1)
		res = flash_erase_page_by_page(keep_cx);
	else
		res = flash_full_erase();
	if (res < 0) {
		logError(1, "%s flash erase FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s flash erase COMPLETED!\n\n", tag);


	//mdelay(FLASH_WAIT_TIME);
	logError(0, "%s 6) LOAD PROGRAM:\n", tag);
	res = fillFlash(FLASH_ADDR_CODE, (u8 *)(&fw->data[0]),
					fw->sec0_size);
	if (res < OK) {
		logError(1, "%s   load program ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   load program DONE!\n", tag);
	logError(0, "%s 7) LOAD CONFIG:\n", tag);
	res = fillFlash(FLASH_ADDR_CONFIG,
		&(fw->data[fw->sec0_size]), fw->sec1_size);
	if (res < OK) {
		logError(1, "%s   load config ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   load config DONE!\n", tag);

	logError(0, "%s   Flash burn COMPLETED!\n\n", tag);

	logError(0, "%s 8) SYSTEM RESET:\n", tag);
	res = fts_system_reset();
	if (res < 0) {
		logError(1, "%s system reset FAILED! ERROR %02X\n",
			tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}
	logError(0, "%s   system reset COMPLETED!\n\n", tag);


	logError(0, "%s 9) FINAL CHECK:\n", tag);
	res = readChipInfo(0);
	if (res < 0) {
		logError(1, "%s %s:Unable to retrieve Chip INFO!:%02X\n",
			tag, __func__, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	if ((ftsInfo.u16_fwVer != fw->fw_ver)
		&& (ftsInfo.u16_cfgId != fw->config_id)) {
		pr_err("Firmware is different from the old!\n");
		logError(1, "%s fw: %x != %x, conf: %x != %x\n",
			tag, ftsInfo.u16_fwVer, fw->fw_ver,
			ftsInfo.u16_cfgId, fw->config_id);
		return ERROR_FLASH_BURN_FAILED;
	}

	logError(0, "%s Final check OK! fw: %02X , conf: %02X\n",
		tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);

	return OK;
}

#endif
