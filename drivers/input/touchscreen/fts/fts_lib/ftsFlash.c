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

#ifdef FW_H_FILE
#include <../fts_fw.h>
#define LOAD_FW_FROM 1
#else
#define LOAD_FW_FROM 0
#endif



extern chipInfo ftsInfo;

int getFirmwareVersion(u16 *fw_vers, u16 *config_id)
{
	u8 fwvers[DCHIP_FW_VER_BYTE];
	u8 confid[CONFIG_ID_BYTE];
	int res;
	res = readCmdU16(FTS_CMD_HW_REG_R, DCHIP_FW_VER_ADDR, fwvers, DCHIP_FW_VER_BYTE, DUMMY_HW_REG);

	if (res < OK) {
		log_error("%s getFirmwareVersion: unable to read fw_version ERROR %02X\n", tag, ERROR_FW_VER_READ);
		return (res | ERROR_FW_VER_READ);
	}

	u8ToU16(fwvers, fw_vers);

	if (*fw_vers != 0) {
		res = readB2(CONFIG_ID_ADDR, confid, CONFIG_ID_BYTE);

		if (res < OK) {
			log_error("%s getFirmwareVersion: unable to read config_id ERROR %02X\n", tag, ERROR_FW_VER_READ);
			return (res | ERROR_FW_VER_READ);
		}

		u8ToU16(confid, config_id);
	} else
		*config_id = 0x0000;

	log_debug("%s FW VERS = %04X\n", tag, *fw_vers);
	log_debug("%s CONFIG ID = %04X\n", tag, *config_id);
	return OK;
}

int getFWdata(const char *pathToFile, u8 **data, int *size, int from)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	int res;
	log_debug("%s getFWdata starting ...\n", tag);

	switch (from) {
#ifdef FW_H_FILE

	case 1:
		log_debug("%s Read FW from .h file!\n", tag);
		*size = FW_SIZE_NAME;
		*data = (u8 *)kmalloc((*size) * sizeof(u8), GFP_KERNEL);

		if (*data == NULL) {
			log_error("%s getFWdata: Impossible to allocate memory! ERROR %08X\n", tag, ERROR_ALLOC);
			return ERROR_ALLOC;
		}

		memcpy(*data, (u8 *)FW_ARRAY_NAME, (*size));
		break;
#endif

	default:
		log_debug("%s Read FW from BIN file!\n", tag);
		dev = getDev();

		if (dev != NULL) {
			res = request_firmware(&fw, pathToFile, dev);

			if (res == 0) {
				*size = fw->size;
				*data = (u8 *)kmalloc((*size) * sizeof(u8), GFP_KERNEL);

				if (*data == NULL) {
					log_error("%s getFWdata: Impossible to allocate memory! ERROR %08X\n", tag, ERROR_ALLOC);
					return ERROR_ALLOC;
				}

				memcpy(*data, (u8 *)fw->data, (*size));
				release_firmware(fw);
			} else {
				log_error("%s getFWdata: No File found! ERROR %08X\n", tag, ERROR_FILE_NOT_FOUND);
				return ERROR_FILE_NOT_FOUND;
			}
		} else {
			log_error("%s getFWdata: No device found! ERROR %08X\n", tag, ERROR_OP_NOT_ALLOW);
			return ERROR_OP_NOT_ALLOW;
		}
	}

	log_debug("%s getFWdata Finshed!\n", tag);
	return OK;
}

#ifdef FTM3_CHIP
int flash_status(void)
{
	u8 cmd[2] = {FLASH_CMD_READSTATUS, 0x00};
	u8 readData;
	log_debug("%s Reading flash_status...\n", tag);

	if (fts_readCmd(cmd, 2, &readData, FLASH_STATUS_BYTES) < 0) {
		log_error("%s flash_status: ERROR %02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	readData &= 0x01;
	return (int)readData;
}

int flash_status_ready(void)
{
	int status = flash_status();

	if (status == ERROR_I2C_R) {
		log_error("%s flash_status_ready: ERROR %02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	if (status != FLASH_READY) {
		log_error("%s flash_status_ready: flash busy or unknown STATUS = %02X\n", tag, status);
		return ERROR_FLASH_UNKNOWN;
	}

	return FLASH_READY;
}

int wait_for_flash_ready(void)
{
	int status;
	int (*code)(void);
	code = flash_status_ready;
	log_debug("%s Waiting for flash ready...\n", tag);
	status = attempt_function(code, FLASH_WAIT_BEFORE_RETRY, FLASH_RETRY_COUNT);

	if (status != FLASH_READY) {
		log_error("%s wait_for_flash_ready: ERROR %02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	log_debug("%s Flash ready!\n", tag);
	return OK;
}

int flash_unlock(void)
{
	int status;
	u8 cmd[3] = {FLASH_CMD_UNLOCK, FLASH_UNLOCK_CODE0, FLASH_UNLOCK_CODE1};
	log_debug("%s Try to unlock flash...\n", tag);
	status = wait_for_flash_ready();

	if (status != OK) {
		log_error("%s flash_unlock: ERROR %02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	log_debug("%s Command unlock ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_unlock: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready();

	if (status != OK) {
		log_error("%s flash_unlock: ERROR %02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	log_debug("%s Unlock flash DONE!\n", tag);
	return OK;
}

int parseBinFile(u8 *fw_data, int fw_size, Firmware *fwData, int keep_cx)
{
	int dimension;

	if (keep_cx) {
		dimension = FW_SIZE - FW_CX_SIZE;
		log_error("%s parseBinFile: Selected 124k Configuration!\n", tag);
	} else {
		dimension = FW_SIZE;
		log_error("%s parseBinFile: Selected 128k Configuration!\n", tag);
	}

	if (fw_size - FW_HEADER_SIZE != FW_SIZE || fw_data == NULL) {
		log_error("%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, FW_SIZE, ERROR_FILE_PARSE);
		kfree(fw_data);
		return ERROR_FILE_PARSE;
	} else {
		fwData->data = (u8 *)kmalloc(dimension * sizeof(u8), GFP_KERNEL);

		if (fwData->data == NULL) {
			log_error("%s parseBinFile: ERROR %02X\n", tag, ERROR_ALLOC);
			kfree(fw_data);
			return ERROR_ALLOC;
		}

		memcpy(fwData->data, ((u8 *)(fw_data) + FW_HEADER_SIZE), dimension);
		fwData->data_size = dimension;
		fwData->fw_ver = (u16)(((fwData->data[FW_VER_MEMH_BYTE1] & 0x00FF) << 8) + (fwData->data[FW_VER_MEMH_BYTE0] & 0x00FF));
		fwData->config_id = (u16)(((fwData->data[(FW_CODE_SIZE) + FW_OFF_CONFID_MEMH_BYTE1] & 0x00FF) << 8) + (fwData->data[(FW_CODE_SIZE) + FW_OFF_CONFID_MEMH_BYTE0] & 0x00FF));
		log_debug("%s parseBinFile: FW VERS File = %04X\n", tag, fwData->fw_ver);
		log_debug("%s parseBinFile: CONFIG ID File = %04X\n", tag, fwData->config_id);
		log_debug("%s READ FW DONE %d bytes!\n", tag, fwData->data_size);
		kfree(fw_data);
		return OK;
	}
}

int readFwFile(const char *path, Firmware *fw, int keep_cx)
{
	int res;
	int orig_size;
	u8 *orig_data;
	res = getFWdata(path, &orig_data, &orig_size, LOAD_FW_FROM);

	if (res < OK) {
		log_error("%s readFwFile: impossible retrieve FW... ERROR %08X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	res = parseBinFile(orig_data, orig_size, fw, keep_cx);

	if (res < OK) {
		log_error("%s readFwFile: impossible parse ERROR %08X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	return OK;
}

int fillMemory(u32 address, u8 *data, int size)
{
	int remaining = size;
	int toWrite = 0;
	u8 *buff = (u8 *)kmalloc((MEMORY_CHUNK + 3) * sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		log_error("%s fillMemory: ERROR %02X\n", tag, ERROR_ALLOC);
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

		buff[1] = (u8)((address & 0x0000FF00) >> 8);
		buff[2] = (u8)(address & 0x000000FF);
		memcpy(buff + 3, data, toWrite);
		log_debug("%s Command = %02X , address = %02X %02X, bytes = %d\n", tag, buff[0], buff[1], buff[2], toWrite);

		if (fts_writeCmd(buff, 3 + toWrite) < 0) {
			log_error("%s fillMemory: ERROR %02X\n", tag, ERROR_I2C_W);
			kfree(buff);
			return ERROR_I2C_W;
		}

		address += toWrite;
		data += toWrite;
	}
	kfree(buff);
	return OK;
}

int flash_burn(Firmware fw, int force_burn, int keep_cx)
{
	u8 cmd;
	int res;

	if (!force_burn && (ftsInfo.u16_fwVer >= fw.fw_ver) && (ftsInfo.u16_cfgId >= fw.config_id)) {
		log_error("%s flash_burn: Firmware in the chip newer or equal to the one to burn! NO UPDATE ERROR %02X\n", tag, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s Programming Procedure for flashing started:\n\n", tag);
	log_debug("%s 1) SYSTEM RESET:\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s    system reset FAILED!\n", tag);

		if (res != (ERROR_SYSTEM_RESET_FAIL | ERROR_TIMEOUT))
			return (res | ERROR_FLASH_BURN_FAILED);
	} else
		log_debug("%s   system reset COMPLETED!\n\n", tag);

	log_debug("%s 2) FLASH UNLOCK:\n", tag);
	res = flash_unlock();

	if (res < 0) {
		log_error("%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	} else {
		log_debug("%s   flash unlock COMPLETED!\n\n", tag);
	}

	log_debug("%s 3) PREPARING DATA FOR FLASH BURN:\n", tag);
	res = fillMemory(FLASH_ADDR_CODE, fw.data, fw.data_size);

	if (res < 0) {
		log_error("%s   Error During filling the memory! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s   Data copy COMPLETED!\n\n", tag);
	log_debug("%s 4) ERASE FLASH:\n", tag);
	res = wait_for_flash_ready();

	if (res < 0) {
		log_error("%s   Flash not ready! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s Command erase ...\n", tag);
	cmd = FLASH_CMD_ERASE;

	if (fts_writeCmd(&cmd, 1) < 0) {
		log_error("%s   Error during erasing flash! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();

	if (res < 0) {
		log_error("%s   Flash not ready 2! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s   Flash erase COMPLETED!\n\n", tag);
	log_debug("%s 5) BURN FLASH:\n", tag);
	log_debug("%s Command burn ...\n", tag);
	cmd = FLASH_CMD_BURN;

	if (fts_writeCmd(&cmd, 1) < 0) {
		log_error("%s   Error during burning data! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (ERROR_I2C_W | ERROR_FLASH_BURN_FAILED);
	}

	res = wait_for_flash_ready();

	if (res < 0) {
		log_error("%s   Flash not ready! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s   Flash burn COMPLETED!\n\n", tag);
	log_debug("%s 6) SYSTEM RESET:\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s    system reset FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_debug("%s   system reset COMPLETED!\n\n", tag);
	log_debug("%s 7) FINAL CHECK:\n", tag);
	res = readChipInfo(0);

	if (res < 0) {
		log_error("%s flash_burn: Unable to retrieve Chip INFO! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	if ((ftsInfo.u16_fwVer != fw.fw_ver) && (ftsInfo.u16_cfgId != fw.config_id)) {
		log_error("%s   Firmware in the chip different from the one that was burn! fw: %x != %x , conf: %x != %x\n", tag, ftsInfo.u16_fwVer, fw.fw_ver, ftsInfo.u16_cfgId, fw.config_id);
		return ERROR_FLASH_BURN_FAILED;
	}

	log_debug("%s   Final check OK! fw: %02X , conf: %02X\n", tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);
	return OK;
}

int flashProcedure(const char *path, int force, int keep_cx)
{
	Firmware fw;
	int res;
	fw.data = NULL;
	log_debug("%s Reading Fw file...\n", tag);
	res = readFwFile(path, &fw, keep_cx);

	if (res < OK) {
		log_error("%s flashProcedure: ERROR %02X\n", tag, (res | ERROR_FLASH_PROCEDURE));
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}

	log_debug("%s Fw file read COMPLETED!\n", tag);
	log_debug("%s Starting flashing procedure...\n", tag);
	res = flash_burn(fw, force, keep_cx);

	if (res < OK && res != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED)) {
		log_error("%s flashProcedure: ERROR %02X\n", tag, ERROR_FLASH_PROCEDURE);
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}

	log_debug("%s flashing procedure Finished!\n", tag);
	kfree(fw.data);
	return res;
}

#else

int wait_for_flash_ready(u8 type)
{
	u8 cmd[2] = {FLASH_CMD_READ_REGISTER, type};
	u8 readData;
	int i, res = -1;
	log_debug("%s Waiting for flash ready ...\n", tag);

	for (i = 0; i < FLASH_RETRY_COUNT && res != 0; i++) {
		if (fts_readCmd(cmd, sizeof(cmd), &readData, 1) < 0) {
			log_error("%s wait_for_flash_ready: ERROR %02X\n", tag, ERROR_I2C_W);
		} else {
			res = readData & 0x80;
		}

		mdelay(FLASH_WAIT_BEFORE_RETRY);
	}

	if (i == FLASH_RETRY_COUNT && res != 0) {
		log_error("%s Wait for flash TIMEOUT! ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	log_debug("%s Flash READY!\n", tag);
	return OK;
}

int fts_warm_boot(void)
{
	u8 cmd[4] = {FTS_CMD_HW_REG_W, 0x00, 0x00, WARM_BOOT_VALUE};
	u16ToU8_be(ADDR_WARM_BOOT, &cmd[1]);
	log_debug("%s Command warm boot ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_unlock: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s Warm boot DONE!\n", tag);
	return OK;
}

int parseBinFile(u8 *data, int fw_size, Firmware *fwData, int keep_cx)
{
	int dimension, index = 0;
	u32 temp;
	int res, i;

	if (fw_size < FW_HEADER_SIZE + FW_BYTES_ALLIGN || data == NULL) {
		log_error("%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, FW_HEADER_SIZE + FW_BYTES_ALLIGN, ERROR_FILE_PARSE);
		res = ERROR_FILE_PARSE;
		goto END;
	} else {
		u8ToU32(&data[index], &temp);

		if (temp != FW_HEADER_SIGNATURE) {
			log_error("%s parseBinFile: Wrong Signature %08X ... ERROR %02X\n", tag, temp, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

		log_debug("%s parseBinFile: Fw Signature OK!\n", tag);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);

		if (temp != FW_FTB_VER) {
			log_error("%s parseBinFile: Wrong ftb_version %08X ... ERROR %02X\n", tag, temp, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

		log_debug("%s parseBinFile: ftb_version OK!\n", tag);
		index += FW_BYTES_ALLIGN;

		if (data[index] != DCHIP_ID_0 || data[index + 1] != DCHIP_ID_1) {
			log_error("%s parseBinFile: Wrong target %02X != %02X  %02X != %02X ... ERROR %08X\n", tag, data[index], DCHIP_ID_0, data[index + 1], DCHIP_ID_1, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		log_error("%s parseBinFile: Fw ID = %08X\n", tag, temp);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		fwData->fw_ver = temp;
		log_error("%s parseBinFile: FILE Fw Version = %04X\n", tag, fwData->fw_ver);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		fwData->config_id = temp;
		log_error("%s parseBinFile: FILE Config ID = %08X\n", tag, temp);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		log_error("%s parseBinFile: Config Version = %08X\n", tag, temp);
		index += FW_BYTES_ALLIGN * 2;
		index += FW_BYTES_ALLIGN;

		for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++) {
			fwData->externalRelease[i] = data[index++];
		}

		u8ToU32(&data[index], &temp);
		fwData->sec0_size = temp;
		log_debug("%s parseBinFile:  sec0_size = %08X (%d bytes)\n", tag, fwData->sec0_size, fwData->sec0_size);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec1_size = temp;
		log_debug("%s parseBinFile:  sec1_size = %08X (%d bytes)\n", tag, fwData->sec1_size, fwData->sec1_size);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec2_size = temp;
		log_debug("%s parseBinFile:  sec2_size = %08X (%d bytes)\n", tag, fwData->sec2_size, fwData->sec2_size);
		index += FW_BYTES_ALLIGN;
		u8ToU32(&data[index], &temp);
		fwData->sec3_size = temp;
		log_debug("%s parseBinFile:  sec3_size = %08X (%d bytes)\n", tag, fwData->sec3_size, fwData->sec3_size);
		index += FW_BYTES_ALLIGN;

		if (!keep_cx) {
			dimension = fwData->sec0_size + fwData->sec1_size + fwData->sec2_size + fwData->sec3_size;
			temp = fw_size;
		} else {
			dimension = fwData->sec0_size + fwData->sec1_size;
			temp = fw_size - fwData->sec2_size - fwData->sec3_size;
			fwData->sec2_size = 0;
			fwData->sec3_size = 0;
		}

		if (dimension + FW_HEADER_SIZE + FW_BYTES_ALLIGN != temp) {
			log_error("%s parseBinFile: Read only %d instead of %d... ERROR %02X\n", tag, fw_size, dimension + FW_HEADER_SIZE + FW_BYTES_ALLIGN, ERROR_FILE_PARSE);
			res = ERROR_FILE_PARSE;
			goto END;
		}

		fwData->data = (u8 *)kmalloc(dimension * sizeof(u8), GFP_KERNEL);

		if (fwData->data == NULL) {
			log_error("%s parseBinFile: ERROR %02x\n", tag, ERROR_ALLOC);
			res = ERROR_ALLOC;
			goto END;
		}

		index += FW_BYTES_ALLIGN;
		memcpy(fwData->data, &data[index], dimension);
		fwData->data_size = dimension;
		log_debug("%s READ FW DONE %d bytes!\n", tag, fwData->data_size);
		res = OK;
		goto END;
	}

END:
	kfree(data);
	return res;
}

int readFwFile(const char *path, Firmware *fw, int keep_cx)
{
	int res;
	int orig_size;
	u8 *orig_data;
	res = getFWdata(path, &orig_data, &orig_size, LOAD_FW_FROM);

	if (res < OK) {
		log_error("%s readFwFile: impossible retrieve FW... ERROR %08X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	res = parseBinFile(orig_data, orig_size, fw, keep_cx);

	if (res < OK) {
		log_error("%s readFwFile: ERROR %02X\n", tag, ERROR_MEMH_READ);
		return (res | ERROR_MEMH_READ);
	}

	return OK;
}

int flash_unlock(void)
{
	u8 cmd[3] = {FLASH_CMD_UNLOCK, FLASH_UNLOCK_CODE0, FLASH_UNLOCK_CODE1};
	log_debug("%s Command unlock ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_unlock: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s Unlock flash DONE!\n", tag);
	return OK;
}

int flash_erase_unlock(void)
{
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_UNLOCK_CODE0, FLASH_ERASE_UNLOCK_CODE1};
	log_debug("%s Try to erase unlock flash...\n", tag);
	log_debug("%s Command erase unlock ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_erase_unlock: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	log_debug("%s Erase Unlock flash DONE!\n", tag);
	return OK;
}

int flash_full_erase(void)
{
	int status;
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_CODE0, FLASH_ERASE_CODE1};
	log_debug("%s Command full erase sent ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_full_erase: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_ERASE_CODE0);

	if (status != OK) {
		log_error("%s flash_full_erase: ERROR %02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	log_debug("%s Full Erase flash DONE!\n", tag);
	return OK;
}

int flash_erase_page_by_page(int keep_cx)
{

    u8 status, i = 0;
    u8 cmd[4] = {FLASH_CMD_WRITE_REGISTER, FLASH_ERASE_CODE0, 0x00, 0x00};
	for (i = 0; i < FLASH_NUM_PAGE; i++) {
		if (i >= FLASH_CX_PAGE_START && i <= FLASH_CX_PAGE_END && keep_cx == 1) {
			log_debug("%s Skipping erase page %d! \n", tag, i);
			continue;
	}
	log_debug("%s Command erase page %d sent ... \n", tag, i);
	cmd[2] = (0x3F & i) | FLASH_ERASE_START;
	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s flash_erase_page_by_page: ERROR %08X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}
	status = wait_for_flash_ready(FLASH_ERASE_CODE0);
	if (status != OK) {
		log_error("%s flash_erase_page_by_page: ERROR %08X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}
	}
	log_debug("%s Erase flash page by page DONE! \n", tag);
	return OK;

}

int start_flash_dma(void)
{
	int status;
	u8 cmd[3] = {FLASH_CMD_WRITE_REGISTER, FLASH_DMA_CODE0, FLASH_DMA_CODE1};
	log_debug("%s Command flash DMA ...\n", tag);

	if (fts_writeCmd(cmd, sizeof(cmd)) < 0) {
		log_error("%s start_flash_dma: ERROR %02X\n", tag, ERROR_I2C_W);
		return ERROR_I2C_W;
	}

	status = wait_for_flash_ready(FLASH_DMA_CODE0);

	if (status != OK) {
		log_error("%s start_flash_dma: ERROR %02X\n", tag, ERROR_FLASH_NOT_READY);
		return (status | ERROR_FLASH_NOT_READY);
	}

	log_debug("%s flash DMA DONE!\n", tag);
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
	u8 buff1[9];
	u8 *buff = (u8 *)kzalloc((DMA_CHUNK + 3) * sizeof(u8), GFP_KERNEL);

	if (buff == NULL) {
		log_error("%s fillFlash: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	while (remaining > 0) {
		byteBlock = 0;
		addr = 0;

		while (byteBlock < FLASH_CHUNK && remaining > 0) {
			buff[0] = FLASH_CMD_WRITE_64K;

			if (remaining >= DMA_CHUNK) {
				if ((byteBlock + DMA_CHUNK) <= FLASH_CHUNK) {
					toWrite = DMA_CHUNK;
					remaining -= DMA_CHUNK;
					byteBlock += DMA_CHUNK;
				} else {
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			} else {
				if ((byteBlock + remaining) <= FLASH_CHUNK) {
					toWrite = remaining;
					byteBlock += remaining;
					remaining = 0;
				} else {
					delta = FLASH_CHUNK - byteBlock;
					toWrite = delta;
					remaining -= delta;
					byteBlock += delta;
				}
			}

			buff[1] = (u8)((addr & 0x0000FF00) >> 8);
			buff[2] = (u8)(addr & 0x000000FF);
			memcpy(&buff[3], data, toWrite);

			if (fts_writeCmd(buff, 3 + toWrite) < 0) {
				log_error("%s fillFlash: ERROR %02X\n", tag, ERROR_I2C_W);
				kfree(buff);
				return ERROR_I2C_W;
			}

			addr += toWrite;
			data += toWrite;
		}

		byteBlock = byteBlock / 4 - 1;
		buff1[0] = FLASH_CMD_WRITE_REGISTER;
		buff1[1] = FLASH_DMA_CONFIG;
		buff1[2] = 0x00;
		buff1[3] = 0x00;
		addr = address + ((wheel * FLASH_CHUNK) / 4);
		buff1[4] = (u8)((addr & 0x000000FF));
		buff1[5] = (u8)((addr & 0x0000FF00) >> 8);
		buff1[6] = (u8)(byteBlock & 0x000000FF);
		buff1[7] = (u8)((byteBlock & 0x0000FF00) >> 8);
		buff1[8] = 0x00;
		log_debug("%s Command = %02X , address = %02X %02X, words =  %02X %02X\n", tag, buff1[0], buff1[5], buff1[4], buff1[7], buff1[6]);

		if (fts_writeCmd(buff1, 9) < OK) {
			log_error("%s   Error during filling Flash! ERROR %02X\n", tag, ERROR_I2C_W);
			kfree(buff);
			return ERROR_I2C_W;
		}

		res = start_flash_dma();

		if (res < OK) {
			log_error("%s   Error during flashing DMA! ERROR %02X\n", tag, res);
			kfree(buff);
			return res;
		}

		wheel++;
	}

	kfree(buff);
	return OK;
}

int flash_burn(Firmware fw, int force_burn, int keep_cx)
{
	int res;

	if (!force_burn) {
		if ((ftsInfo.u16_fwVer != fw.fw_ver) || (ftsInfo.u16_cfgId != fw.config_id)) {
			goto start;
		}
		log_error("%s flash_burn: Firmware in the chip equal to the one to burn! NO UPDATE ERROR %02X\n", tag, ERROR_FW_NO_UPDATE);
		return (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED);
	}

start:
	log_debug("%s Programming Procedure for flashing started:\n\n", tag);
	log_debug("%s 1) SYSTEM RESET:\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s    system reset FAILED!\n", tag);

		if (res != (ERROR_SYSTEM_RESET_FAIL | ERROR_TIMEOUT))
			return (res | ERROR_FLASH_BURN_FAILED);
	} else
		log_debug("%s   system reset COMPLETED!\n\n", tag);

	log_debug("%s 2) WARM BOOT:\n", tag);
	res = fts_warm_boot();

	if (res < OK) {
		log_error("%s    warm boot FAILED!\n", tag);
		return (res | ERROR_FLASH_BURN_FAILED);
	} else
		log_debug("%s    warm boot COMPLETED!\n\n", tag);

	log_debug("%s 3) FLASH UNLOCK:\n", tag);
	res = flash_unlock();

	if (res < OK) {
		log_error("%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	} else {
		log_debug("%s   flash unlock COMPLETED!\n\n", tag);
	}

	log_debug("%s 4) FLASH ERASE UNLOCK:\n", tag);
	res = flash_erase_unlock();

	if (res < 0) {
		log_error("%s   flash unlock FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	} else {
		log_debug("%s   flash unlock COMPLETED!\n\n", tag);
	}

	log_debug("%s 5) FLASH ERASE:\n", tag);
	if (keep_cx == 1)
		res = flash_erase_page_by_page(keep_cx);
	else
		res = flash_full_erase();

	if (res < 0) {
		log_error("%s   flash erase FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	} else {
		log_debug("%s   flash erase COMPLETED!\n\n", tag);
	}

	log_debug("%s 6) LOAD PROGRAM:\n", tag);
	res = fillFlash(FLASH_ADDR_CODE, &fw.data[0], fw.sec0_size);

	if (res < OK) {
		log_error("%s   load program ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_error("%s   load program DONE!\n", tag);
	log_debug("%s 7) LOAD CONFIG:\n", tag);
	res = fillFlash(FLASH_ADDR_CONFIG, &(fw.data[fw.sec0_size]), fw.sec1_size);

	if (res < OK) {
		log_error("%s   load config ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_error("%s   load config DONE!\n", tag);
	log_debug("%s   Flash burn COMPLETED!\n\n", tag);
	log_debug("%s 8) SYSTEM RESET:\n", tag);
	res = fts_system_reset();

	if (res < 0) {
		log_error("%s    system reset FAILED! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	log_error("%s   system reset COMPLETED!\n\n", tag);
	log_debug("%s 9) FINAL CHECK:\n", tag);
	res = readChipInfo(0);

	if (res < 0) {
		log_error("%s flash_burn: Unable to retrieve Chip INFO! ERROR %02X\n", tag, ERROR_FLASH_BURN_FAILED);
		return (res | ERROR_FLASH_BURN_FAILED);
	}

	for (res = 0; res < EXTERNAL_RELEASE_INFO_SIZE; res++) {
		if (fw.externalRelease[res] != ftsInfo.u8_extReleaseInfo[res]) {
			log_error("%s  Firmware in the chip different from the one that was burn! fw: %x != %x , conf: %x != %x\n", tag, ftsInfo.u16_fwVer, fw.fw_ver, ftsInfo.u16_cfgId, fw.config_id);
			return ERROR_FLASH_BURN_FAILED;
		}
	}

	log_error("%s   Final check OK! fw: %02X , conf: %02X\n", tag, ftsInfo.u16_fwVer, ftsInfo.u16_cfgId);
	return OK;
}

int flashProcedure(const char *path, int force, int keep_cx)
{
	Firmware fw;
	int res;
	fw.data = NULL;
	log_debug("%s Reading Fw file...\n", tag);
	res = readFwFile(path, &fw, keep_cx);

	if (res < OK) {
		log_error("%s flashProcedure: ERROR %02X\n", tag, (res | ERROR_FLASH_PROCEDURE));
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}

	log_debug("%s Fw file read COMPLETED!\n", tag);
	log_debug("%s Starting flashing procedure...\n", tag);
	res = flash_burn(fw, force, keep_cx);

	if (res < OK && res != (ERROR_FW_NO_UPDATE | ERROR_FLASH_BURN_FAILED)) {
		log_error("%s flashProcedure: ERROR %02X\n", tag, ERROR_FLASH_PROCEDURE);
		kfree(fw.data);
		return (res | ERROR_FLASH_PROCEDURE);
	}

	log_debug("%s flashing procedure Finished!\n", tag);
	kfree(fw.data);
	return res;
}
#endif
