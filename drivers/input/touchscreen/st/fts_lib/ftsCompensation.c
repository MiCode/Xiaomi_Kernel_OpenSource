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
 *               FTS functions for getting Initialization Data            *
 *                                                                        *
 **************************************************************************
 **************************************************************************
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
//#include <linux/sec_sysfs.h>

#include "ftsCrossCompile.h"
#include "ftsCompensation.h"
#include "ftsError.h"
#include "ftsFrame.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTool.h"


static char tag[8] = "[ FTS ]\0";
struct chipInfo ftsInfo;

int requestCompensationData(u16 type)
{
	int retry = 0;
	int ret;
	char *temp = NULL;
	u16 answer;

	int event_to_search[3];
	u8 readEvent[FIFO_EVENT_SIZE];

	u8 cmd[3] = { FTS_CMD_REQU_COMP_DATA, 0x00, 0x00};
	/* B8 is the command for asking compensation data*/
	u16ToU8(type, &cmd[1]);

	event_to_search[0] = (int)EVENTID_COMP_DATA_READ;
	event_to_search[1] = cmd[1];
	event_to_search[2] = cmd[2];

	while (retry < COMP_DATA_READ_RETRY) {
		temp =  printHex("Command = ", cmd, 3);
		if (temp != NULL)
			logError(0, "%s %s", tag, temp);
		kfree(temp);
		ret = fts_writeFwCmd(cmd, 3);
		/*send the request to the chip to load*/
		/*in memory the Compensation Data*/
		if (ret < OK) {
			logError(1, "%s %s:ERROR %02X\n",
				tag, __func__, ERROR_I2C_W);
			return ERROR_I2C_W;
		}
		ret = pollForEvent(event_to_search, 3, readEvent,
				TIMEOUT_REQU_COMP_DATA);
		if (ret < OK) {
			logError(0, "%s Event did not Found at %d attemp!\n",
				tag, retry + 1);
			retry += 1;
		} else {
			retry = 0;
			break;
		}
	}

	if (retry == COMP_DATA_READ_RETRY) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	u8ToU16_le(&readEvent[1], &answer);

	if (answer == type)
		return OK;

	logError(1, "%sThe event found has a different type of ", tag);
	logError(1, "Compensation data %02X\n", ERROR_DIFF_COMP_TYPE);
	return ERROR_DIFF_COMP_TYPE;

}

int readCompensationDataHeader(u16 type, struct DataHeader *header,
	u16 *address)
{
	u16 offset = ADDR_FRAMEBUFFER_DATA;
	u16 answer;
	u8 data[COMP_DATA_HEADER];

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, offset, data, COMP_DATA_HEADER,
		DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);
		return ERROR_I2C_R;
	}
	logError(0, "%s Read Data Header done!\n", tag);

	if (data[0] != HEADER_SIGNATURE) {
		logError(1, "%s %s:%02X The Header Signature was wrong!",
			tag, __func__, ERROR_WRONG_COMP_SIGN);
		logError(1, "%02X != %02X\n", data[0], HEADER_SIGNATURE);

		return ERROR_WRONG_COMP_SIGN;
	}


	u8ToU16_le(&data[1], &answer);

	if (answer != type) {
		logError(1, "%s %s:ERROR %02X\n",
			tag, __func__, ERROR_DIFF_COMP_TYPE);

		return ERROR_DIFF_COMP_TYPE;
	}

	logError(0, "%s Type of Compensation data OK!\n", tag);

	header->type = type;
	header->force_node = (int)data[4];
	header->sense_node = (int)data[5];

	*address = offset + COMP_DATA_HEADER;

	return OK;

}

int readMutualSenseGlobalData(u16 *address, struct MutualSenseData *global)
{

	u8 data[COMP_DATA_GLOBAL];

	logError(0, "%s Address for Global data= %02X\n", tag, *address);

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, *address, data,
		COMP_DATA_GLOBAL, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);

		return ERROR_I2C_R;
	}
	logError(0, "%s Global data Read!\n", tag);

	global->tuning_ver = data[0];
	global->cx1 = data[1];

	logError(0, "%s tuning_ver = %d  CX1 = %d\n",
		tag, global->tuning_ver, global->cx1);

	*address += COMP_DATA_GLOBAL;
	return OK;
}



int readMutualSenseNodeData(u16 address, struct MutualSenseData *node)
{
	int size = node->header.force_node*node->header.sense_node;

	logError(0, "%s Address for Node data = %02X\n", tag, address);

	node->node_data = (u8 *)kmalloc_array(size, (sizeof(u8)), GFP_KERNEL);

	if (node->node_data == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	logError(0, "%s Node Data to read %d bytes\n", tag, size);

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, address, node->node_data,
		size, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s:ERROR %02X\n", tag, __func__, ERROR_I2C_R);
		kfree(node->node_data);
		return ERROR_I2C_R;
	}
	node->node_data_size = size;

	logError(0, "%s Read node data ok!\n", tag);

	return size;
}


int readMutualSenseCompensationData(u16 type, struct MutualSenseData *data)
{
	int ret;
	u16 address;

	data->node_data = NULL;

	if (!(type == MS_TOUCH_ACTIVE || type == MS_TOUCH_LOW_POWER
		|| type == MS_TOUCH_ULTRA_LOW_POWER || type == MS_KEY)) {
		logError(1, "%s %s: Choose a MS type of compensation data ",
			tag, __func__);
		logError(1, "ERROR %02X\n", ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = requestCompensationData(type);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_REQU_COMP_DATA);
		return (ret|ERROR_REQU_COMP_DATA);
	}

	ret = readCompensationDataHeader(type, &(data->header), &address);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_HEADER);
		return (ret | ERROR_COMP_DATA_HEADER);
	}

	ret = readMutualSenseGlobalData(&address, data);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_GLOBAL);
		return (ret|ERROR_COMP_DATA_GLOBAL);
	}

	ret = readMutualSenseNodeData(address, data);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
		tag, __func__, ERROR_COMP_DATA_NODE);
		return (ret | ERROR_COMP_DATA_NODE);
	}

	return OK;
}


int readSelfSenseGlobalData(u16 *address, struct SelfSenseData *global)
{
	u8 data[COMP_DATA_GLOBAL];

	logError(0, "%s Address for Global data= %02X\n", tag, *address);

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, *address, data,
		COMP_DATA_GLOBAL, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n",
		tag, __func__, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	logError(0, "%s Global data Read!\n", tag);

	global->tuning_ver = data[0];
	global->f_ix1 = data[1];
	global->s_ix1 = data[2];
	global->f_cx1 = data[3];
	global->s_cx1 = data[4];
	global->f_max_n = data[5];
	global->s_max_n = data[6];

	logError(0,
		"%stuning_ver = %df_ix1 = %ds_ix1 = %df_cx1 = %d s_cx1 = %d\n",
		tag, global->tuning_ver, global->f_ix1,
		global->s_ix1, global->f_cx1, global->s_cx1);
	logError(0, "%s max_n = %d   s_max_n = %d\n",
		tag, global->f_max_n, global->s_max_n);

	*address += COMP_DATA_GLOBAL;
	return OK;
}

int readSelfSenseNodeData(u16 address, struct SelfSenseData *node)
{
	int size = node->header.force_node * 2 + node->header.sense_node * 2;
	u8 *data;

	node->ix2_fm = (u8 *)kmalloc_array(node->header.force_node,
				sizeof(u8), GFP_KERNEL);
	if (node->ix2_fm == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	node->cx2_fm = (u8 *)kmalloc_array(node->header.force_node,
				sizeof(u8), GFP_KERNEL);
	if (node->cx2_fm == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		kfree(node->ix2_fm);
		return ERROR_ALLOC;
	}
	node->ix2_sn = (u8 *)kmalloc_array(node->header.sense_node,
				sizeof(u8), GFP_KERNEL);
	if (node->ix2_sn == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		kfree(node->ix2_fm);
		kfree(node->cx2_fm);
		return ERROR_ALLOC;
	}
	node->cx2_sn = (u8 *)kmalloc_array(node->header.sense_node,
				sizeof(u8), GFP_KERNEL);
	if (node->cx2_sn == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		kfree(node->ix2_fm);
		kfree(node->cx2_fm);
		kfree(node->ix2_sn);
		return ERROR_ALLOC;
	}

	logError(0, "%s Address for Node data = %02X\n", tag, address);

	logError(0, "%s Node Data to read %d bytes\n", tag, size);

	data = (u8 *)kmalloc_array(size, sizeof(u8), GFP_KERNEL);
	if (data == NULL) {
		logError(1, "%s %s: ERROR %02X", tag, __func__, ERROR_ALLOC);
		kfree(node->ix2_fm);
		kfree(node->cx2_fm);
		kfree(node->ix2_sn);
		kfree(node->cx2_sn);
		return ERROR_ALLOC;
	}

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, address, data, size,
		DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);
		kfree(node->ix2_fm);
		kfree(node->cx2_fm);
		kfree(node->ix2_sn);
		kfree(node->cx2_sn);
		kfree(data);
		return ERROR_I2C_R;
	}

	logError(0, "%s Read node data ok!\n", tag);

	memcpy(node->ix2_fm, data, node->header.force_node);
	memcpy(node->ix2_sn, &data[node->header.force_node],
		node->header.sense_node);
	memcpy(node->cx2_fm,
		&data[node->header.force_node + node->header.sense_node],
		node->header.force_node);
	memcpy(node->cx2_sn,
		&data[node->header.force_node * 2 + node->header.sense_node],
		node->header.sense_node);

	kfree(data);

	return OK;
}

int readSelfSenseCompensationData(u16 type, struct SelfSenseData *data)
{

	int ret;
	u16 address;

	data->ix2_fm = NULL;
	data->cx2_fm = NULL;
	data->ix2_sn = NULL;
	data->cx2_sn = NULL;

	if (!(type == SS_TOUCH || type == SS_KEY || type == SS_HOVER
		|| type == SS_PROXIMITY)) {
		logError(1, "%s %s:Choose a SS type of compensation data ",
			tag, __func__);
		logError(1, "ERROR %02X\n", ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = requestCompensationData(type);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_REQU_COMP_DATA);
		return (ret | ERROR_REQU_COMP_DATA);
	}

	ret = readCompensationDataHeader(type, &(data->header), &address);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_HEADER);
		return (ret|ERROR_COMP_DATA_HEADER);
	}

	ret = readSelfSenseGlobalData(&address, data);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_GLOBAL);
		return (ret | ERROR_COMP_DATA_GLOBAL);
	}

	ret = readSelfSenseNodeData(address, data);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_NODE);
		return (ret | ERROR_COMP_DATA_NODE);
	}

	return OK;
}


int readGeneralGlobalData(u16 address, struct GeneralData *global)
{
	u8 data[COMP_DATA_GLOBAL];

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, address, data, COMP_DATA_GLOBAL,
		DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);
		return ERROR_I2C_R;
	}

	global->ftsd_lp_timer_cal0 = data[0];
	global->ftsd_lp_timer_cal1 = data[1];
	global->ftsd_lp_timer_cal2 = data[2];
	global->ftsd_lp_timer_cal3 = data[3];
	global->ftsa_lp_timer_cal0 = data[4];
	global->ftsa_lp_timer_cal1 = data[5];

	return OK;
}


int readGeneralCompensationData(u16 type, struct GeneralData *data)
{
	int ret;
	u16 address;

	if (!(type == GENERAL_TUNING)) {
		logError(1, "%s %s:Choose a GENERAL type of compensation data ",
			tag);
		logError(1, "ERROR %02X\n", ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = requestCompensationData(type);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_REQU_COMP_DATA);
		return ERROR_REQU_COMP_DATA;
	}

	ret = readCompensationDataHeader(type, &(data->header), &address);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_HEADER);
		return ERROR_COMP_DATA_HEADER;
	}

	ret = readGeneralGlobalData(address, data);
	if (ret < 0) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_COMP_DATA_GLOBAL);
		return ERROR_COMP_DATA_GLOBAL;
	}

	return OK;

}


int defaultChipInfo(int i2cError)
{
	int i;

	logError(0, "%s Setting default Chip Info...\n", tag);
	ftsInfo.u32_echoEn = 0x00000000;
	ftsInfo.u8_msScrConfigTuneVer = 0;
	ftsInfo.u8_ssTchConfigTuneVer = 0;
	ftsInfo.u8_msScrCxmemTuneVer = 0;
	ftsInfo.u8_ssTchCxmemTuneVer = 0;
	if (i2cError == 1) {
		ftsInfo.u16_fwVer = 0xFFFF;
		ftsInfo.u16_cfgId = 0xFFFF;
		for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++)
			ftsInfo.u8_extReleaseInfo[i] = 0xFF;
	} else {
		ftsInfo.u16_fwVer = 0x0000;
		ftsInfo.u16_cfgId = 0x0000;
		for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++)
			ftsInfo.u8_extReleaseInfo[i] = 0x00;
	}
	ftsInfo.u32_mpPassFlag = INIT_FIELD;
	ftsInfo.u16_errOffset = INVALID_ERROR_OFFS;
	logError(0, "%s default Chip Info DONE!\n", tag);
	return OK;
}

int readChipInfo(int doRequest)
{
	int ret, i;
	u16 answer;
	u8 data[CHIP_INFO_SIZE + 3];
	/*+3 because need to read all the field of*/
	/*the struct plus the signature and 2 address bytes*/
	int index = 0;

	logError(0, "%s Starting Read Chip Info...\n", tag);
	if (doRequest == 1) {
		ret = requestCompensationData(CHIP_INFO);
		if (ret < 0) {
			logError(1, "%s %s: ERROR %02X\n",
				tag, __func__, ERROR_REQU_COMP_DATA);
			ret = (ret | ERROR_REQU_COMP_DATA);
			goto FAIL;
		}
	}

	logError(0, "%s Byte to read = %d bytes\n", tag, CHIP_INFO_SIZE + 3);

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, ADDR_FRAMEBUFFER_DATA, data,
		CHIP_INFO_SIZE + 3, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s %s: ERROR %02X\n", tag, __func__, ERROR_I2C_R);
		ret = ERROR_I2C_R;
		goto FAIL;
	}

	logError(0, "%s Read data ok!\n", tag);

	logError(0, "%s Starting parsing of data...\n", tag);

	if (data[0] != HEADER_SIGNATURE) {
		logError(1, "%s %s:ERROR ", tag, __func__);
		logError(1, "%02X The Header Signature is wrong!%02X != %02X\n",
			ERROR_WRONG_COMP_SIGN, data[0], HEADER_SIGNATURE);
		ret = ERROR_WRONG_COMP_SIGN;
		goto FAIL;
	}

	u8ToU16_le(&data[1], &answer);

	if (answer != CHIP_INFO) {
		logError(1, "%s %s: ERROR %02X\n",
			tag, __func__, ERROR_DIFF_COMP_TYPE);
		ret = ERROR_DIFF_COMP_TYPE;
		goto FAIL;
	}

	index += 3;
	ftsInfo.u8_loadCnt = data[index++];
	ftsInfo.u8_infoVer = data[index++];
	u8ToU16(&data[index], &ftsInfo.u16_ftsdId);
	index += 2;
	ftsInfo.u8_ftsdVer = data[index++];
	ftsInfo.u8_ftsaId = data[index++];
	ftsInfo.u8_ftsaVer = data[index++];
	ftsInfo.u8_tchRptVer = data[index++];

	logError(0, "%s External Release =  ", tag);
	for (i = 0; i < EXTERNAL_RELEASE_INFO_SIZE; i++) {
		ftsInfo.u8_extReleaseInfo[i] = data[index++];
		logError(0, "%02X ", ftsInfo.u8_extReleaseInfo[i]);
	}
	logError(0, "\n");

	for (i = 0; i < sizeof(ftsInfo.u8_custInfo); i++)
		ftsInfo.u8_custInfo[i] = data[index++];

	u8ToU16(&data[index], &ftsInfo.u16_fwVer);
	index += 2;
	logError(1, "%s FW VERSION = %04X\n", tag, ftsInfo.u16_fwVer);

	u8ToU16(&data[index], &ftsInfo.u16_cfgId);
	index += 2;
	logError(1, "%s CONFIG ID = %04X\n", tag, ftsInfo.u16_cfgId);

	ftsInfo.u32_projId = ((data[index + 3] & 0x000000FF) << 24) +
				((data[index + 2] & 0x000000FF) << 16) +
				((data[index + 1] & 0x000000FF) << 8) +
				(data[index] & 0x000000FF);
	index += 4;

	u8ToU16(&data[index], &ftsInfo.u16_scrXRes);
	index += 2;

	u8ToU16(&data[index], &ftsInfo.u16_scrYRes);
	index += 2;

	ftsInfo.u8_scrForceLen = data[index++];
	logError(0, "%s Force Len = %d\n", tag, ftsInfo.u8_scrForceLen);

	ftsInfo.u8_scrSenseLen = data[index++];
	logError(0, "%s Sense Len = %d\n", tag, ftsInfo.u8_scrSenseLen);

	for (i = 0; i < 8; i++)
		ftsInfo.u64_scrForceEn[i] = data[index++];

	for (i = 0; i < 8; i++)
		ftsInfo.u64_scrSenseEn[i] = data[index++];

	ftsInfo.u8_msKeyLen = data[index++];
	logError(0, "%s MS Key Len = %d\n", tag, ftsInfo.u8_msKeyLen);

	for (i = 0; i < 8; i++)
		ftsInfo.u64_msKeyForceEn[i] = data[index++];

	for (i = 0; i < 8; i++)
		ftsInfo.u64_msKeySenseEn[i] = data[index++];

	ftsInfo.u8_ssKeyLen = data[index++];
	logError(0, "%s SS Key Len = %d\n", tag, ftsInfo.u8_ssKeyLen);

	for (i = 0; i < 8; i++)
		ftsInfo.u64_ssKeyForceEn[i] = data[index++];

	for (i = 0; i < 8; i++)
		ftsInfo.u64_ssKeySenseEn[i] = data[index++];

	ftsInfo.u8_frcTchXLen = data[index++];

	ftsInfo.u8_frcTchYLen = data[index++];

	for (i = 0; i < 8; i++)
		ftsInfo.u64_frcTchForceEn[i] = data[index++];

	for (i = 0; i < 8; i++)
		ftsInfo.u64_frcTchSenseEn[i] = data[index++];


	ftsInfo.u8_msScrConfigTuneVer = data[index++];
	logError(0, "%s CFG MS TUNING VERSION = %02X\n",
		tag, ftsInfo.u8_msScrConfigTuneVer);
	ftsInfo.u8_msScrLpConfigTuneVer = data[index++];
	ftsInfo.u8_msScrHwulpConfigTuneVer = data[index++];
	ftsInfo.u8_msKeyConfigTuneVer = data[index++];
	ftsInfo.u8_ssTchConfigTuneVer = data[index++];
	logError(0, "%s CFG SS TUNING VERSION = %02X\n",
		tag, ftsInfo.u8_ssTchConfigTuneVer);
	ftsInfo.u8_ssKeyConfigTuneVer = data[index++];
	ftsInfo.u8_ssHvrConfigTuneVer = data[index++];
	ftsInfo.u8_frcTchConfigTuneVer = data[index++];
	ftsInfo.u8_msScrCxmemTuneVer = data[index++];
	logError(0, "%s CX MS TUNING VERSION = %02X\n",
		tag, ftsInfo.u8_msScrCxmemTuneVer);
	ftsInfo.u8_msScrLpCxmemTuneVer = data[index++];
	ftsInfo.u8_msScrHwulpCxmemTuneVer = data[index++];
	ftsInfo.u8_msKeyCxmemTuneVer = data[index++];
	ftsInfo.u8_ssTchCxmemTuneVer = data[index++];
	logError(0, "%s CX SS TUNING VERSION = %02X\n",
		tag, ftsInfo.u8_ssTchCxmemTuneVer);
	ftsInfo.u8_ssKeyCxmemTuneVer = data[index++];
	ftsInfo.u8_ssHvrCxmemTuneVer = data[index++];
	ftsInfo.u8_frcTchCxmemTuneVer = data[index++];
	ftsInfo.u32_mpPassFlag = ((data[index + 3] & 0x000000FF) << 24)
				+ ((data[index + 2] & 0x000000FF) << 16) +
				((data[index + 1] & 0x000000FF) << 8) +
				(data[index] & 0x000000FF);
	index += 4;
	logError(0, "%s MP SIGNATURE = %08X\n", tag, ftsInfo.u32_mpPassFlag);
	ftsInfo.u32_featEn = ((data[index + 3] & 0x000000FF) << 24) +
				((data[index + 2] & 0x000000FF) << 16) +
				((data[index + 1] & 0x000000FF) << 8) +
				(data[index] & 0x000000FF);
	index += 4;
	ftsInfo.u32_echoEn = ((data[index + 3] & 0x000000FF) << 24) +
				((data[index + 2] & 0x000000FF) << 16) +
				((data[index + 1] & 0x000000FF) << 8) +
				(data[index] & 0x000000FF);
	index += 4;
	logError(0, "%s FEATURES = %08X\n", tag, ftsInfo.u32_echoEn);
	ftsInfo.u8_sideTchConfigTuneVer = data[index++];
	ftsInfo.u8_sideTchCxmemTuneVer = data[index++];
	ftsInfo.u8_sideTchForceLen = data[index++];
	logError(0, "%s Side Touch Force Len = %d\n",
		tag, ftsInfo.u8_sideTchForceLen);
	ftsInfo.u8_sideTchSenseLen = data[index++];
	logError(0, "%s Side Touch Sense Len = %d\n",
		tag, ftsInfo.u8_sideTchSenseLen);
	for (i = 0; i < 8; i++)
		ftsInfo.u64_sideTchForceEn[i] = data[index++];
	for (i = 0; i < 8; i++)
		ftsInfo.u64_sideTchSenseEn[i] = data[index++];
	ftsInfo.u8_errSign = data[index++];
	logError(0, "%s ERROR SIGNATURE = %02X\n", tag, ftsInfo.u8_errSign);
	if (ftsInfo.u8_errSign == ERROR_SIGN_HEAD) {
		logError(0, "%s Correct Error Signature found!\n", tag);
		u8ToU16(&data[index], &ftsInfo.u16_errOffset);
	} else {
		logError(1, "%s Error Signature NOT FOUND!\n", tag);
		ftsInfo.u16_errOffset = INVALID_ERROR_OFFS;
	}
	logError(0, "%s ERROR OFFSET = %04X\n", tag, ftsInfo.u16_errOffset);
	index += 2;
	logError(0, "%s Parsed %d bytes!\n", tag, index);


	if (index != CHIP_INFO_SIZE + 3) {
		logError(1, "%s %s: index = %d different from %d ERROR %02X\n",
			tag, __func__, index, CHIP_INFO_SIZE + 3,
			ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	logError(0, "%s Chip Info Read DONE!\n", tag);
	return OK;

FAIL:
	defaultChipInfo(isI2cError(ret));
	return ret;
}

