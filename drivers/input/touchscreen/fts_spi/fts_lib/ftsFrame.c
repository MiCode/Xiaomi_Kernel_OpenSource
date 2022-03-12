/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                  FTS functions for getting frames						 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsFrame.c
* \brief Contains all the functions to work with frames
*/

#include "ftsCompensation.h"
#include "ftsCore.h"
#include "ftsError.h"
#include "ftsFrame.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTool.h"
#include "ftsTime.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <stdarg.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/ctype.h>

extern SysInfo systemInfo;

/**
 * Read the channels lengths from the config memory
 * @return OK if success or an error code which specify the type of error encountered
 */
int getChannelsLength(void)
{
	int ret;
	u8 data[10];


	ret = readConfig(ADDR_CONFIG_SENSE_LEN, data, 2);
	if (ret < OK) {
		logError(1, "%s getChannelsLength: ERROR %08X\n", tag, ret);

		return ret;
	}

	systemInfo.u8_scrRxLen = (int)data[0];
	systemInfo.u8_scrTxLen = (int)data[1];

	logError(0, "%s Force_len = %d   Sense_Len = %d \n", tag,
		 systemInfo.u8_scrTxLen, systemInfo.u8_scrRxLen);

	return OK;
}

/**
* Read and pack the frame data related to the nodes
* @param address address in memory when the frame data node start
* @param size amount of data to read
* @param frame pointer to an array of bytes which will contain the frame node data
* @return OK if success or an error code which specify the type of error encountered
*/
int getFrameData(u16 address, int size, short *frame)
{
	int i, j, ret;
	u8 *data = (u8 *) kmalloc(size * sizeof(u8), GFP_KERNEL);
	if (data == NULL) {
		logError(1, "%s getFrameData: ERROR %08X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret =
	    fts_writeReadU8UX(FTS_CMD_FRAMEBUFFER_R, BITS_16, address, data,
			      size, DUMMY_FRAMEBUFFER);
	if (ret < OK) {
		logError(1, "%s getFrameData: ERROR %08X\n", tag, ERROR_BUS_R);
		kfree(data);
		return ERROR_BUS_R;
	}
	j = 0;
	for (i = 0; i < size; i += 2) {
		frame[j] = (short)((data[i + 1] << 8) + data[i]);
		j++;
	}
	kfree(data);
	return OK;
}

/**
 * Return the number of Sense Channels (Rx)
 * @return number of Rx channels
 */
int getSenseLen(void)
{
	if (systemInfo.u8_scrRxLen == 0) {
		getChannelsLength();
	}
	return systemInfo.u8_scrRxLen;
}

/**
 * Return the number of Force Channels (Tx)
 * @return number of Tx channels
 */
int getForceLen(void)
{
	if (systemInfo.u8_scrTxLen == 0) {
		getChannelsLength();
	}
	return systemInfo.u8_scrTxLen;
}

/********************    New API     **************************/

/**
* Read a MS Frame from frame buffer memory
* @param type type of MS frame to read
* @param frame pointer to MutualSenseFrame variable which will contain the data
* @return OK if success or an error code which specify the type of error encountered
*/
int getMSFrame3(MSFrameType type, MutualSenseFrame *frame)
{
	u16 offset;
	int ret, force_len, sense_len;

	force_len = getForceLen();
	sense_len = getSenseLen();

	frame->node_data = NULL;

	logError(0, "%s %s: Starting to get frame %02X \n", tag, __func__,
		 type);
	switch (type) {
	case MS_RAW:
		offset = systemInfo.u16_msTchRawAddr;
		goto LOAD_NORM;
	case MS_FILTER:
		offset = systemInfo.u16_msTchFilterAddr;

		goto LOAD_NORM;
	case MS_STRENGTH:
		offset = systemInfo.u16_msTchStrenAddr;
		goto LOAD_NORM;
	case MS_BASELINE:
		offset = systemInfo.u16_msTchBaselineAddr;
LOAD_NORM:
		if (force_len == 0 || sense_len == 0) {
			logError(1,
				 "%s %s: number of channels not initialized ERROR %08X\n",
				 tag, __func__, ERROR_CH_LEN);
			return (ERROR_CH_LEN | ERROR_GET_FRAME);
		}

		break;

	case MS_KEY_RAW:
		offset = systemInfo.u16_keyRawAddr;
		goto LOAD_KEY;
	case MS_KEY_FILTER:
		offset = systemInfo.u16_keyFilterAddr;
		goto LOAD_KEY;
	case MS_KEY_STRENGTH:
		offset = systemInfo.u16_keyStrenAddr;
		goto LOAD_KEY;
	case MS_KEY_BASELINE:
		offset = systemInfo.u16_keyBaselineAddr;
LOAD_KEY:
		if (systemInfo.u8_keyLen == 0) {
			logError(1,
				 "%s %s: number of channels not initialized ERROR %08X\n",
				 tag, __func__, ERROR_CH_LEN);
			return (ERROR_CH_LEN | ERROR_GET_FRAME);
		}
		force_len = 1;
		sense_len = systemInfo.u8_keyLen;
		break;

	case FRC_RAW:
		offset = systemInfo.u16_frcRawAddr;
		goto LOAD_FRC;
	case FRC_FILTER:
		offset = systemInfo.u16_frcFilterAddr;
		goto LOAD_FRC;
	case FRC_STRENGTH:
		offset = systemInfo.u16_frcStrenAddr;
		goto LOAD_FRC;
	case FRC_BASELINE:
		offset = systemInfo.u16_frcBaselineAddr;
LOAD_FRC:
		if (force_len == 0) {
			logError(1,
				 "%s %s: number of channels not initialized ERROR %08X\n",
				 tag, __func__, ERROR_CH_LEN);
			return (ERROR_CH_LEN | ERROR_GET_FRAME);
		}
		sense_len = 1;
		break;
	default:
		logError(1, "%s %s: Invalid type ERROR %08X\n", tag, __func__,
			 ERROR_OP_NOT_ALLOW | ERROR_GET_FRAME);
		return ERROR_OP_NOT_ALLOW | ERROR_GET_FRAME;
	}

	frame->node_data_size = ((force_len) * sense_len);
	frame->header.force_node = force_len;
	frame->header.sense_node = sense_len;
	frame->header.type = type;

	logError(0, "%s %s: Force_len = %d Sense_len = %d Offset = %04X \n",
		 tag, __func__, force_len, sense_len, offset);

	frame->node_data =
	    (short *)kmalloc(frame->node_data_size * sizeof(short), GFP_KERNEL);
	if (frame->node_data == NULL) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__,
			 ERROR_ALLOC | ERROR_GET_FRAME);
		return ERROR_ALLOC | ERROR_GET_FRAME;
	}

	ret =
	    getFrameData(offset, frame->node_data_size * BYTES_PER_NODE,
			 (frame->node_data));
	if (ret < OK) {
		logError(1, "%s %s: ERROR %08X\n", tag, __func__,
			 ERROR_GET_FRAME_DATA);
		kfree(frame->node_data);
		frame->node_data = NULL;
		return (ret | ERROR_GET_FRAME_DATA | ERROR_GET_FRAME);
	}
	logError(0, "%s Frame acquired! \n", tag);
	return frame->node_data_size;

}

/**
* Read a SS Frame from frame buffer
* @param type type of SS frame to read
* @param frame pointer to SelfSenseFrame variable which will contain the data
* @return OK if success or an error code which specify the type of error encountered
*/
int getSSFrame3(SSFrameType type, SelfSenseFrame *frame)
{
	u16 offset_force, offset_sense;
	int ret;

	frame->force_data = NULL;
	frame->sense_data = NULL;

	frame->header.force_node = getForceLen();
	frame->header.sense_node = getSenseLen();

	if (frame->header.force_node == 0 || frame->header.sense_node == 0) {
		logError(1,
			 "%s %s: number of channels not initialized ERROR %08X\n",
			 tag, __func__, ERROR_CH_LEN);
		return (ERROR_CH_LEN | ERROR_GET_FRAME);
	}

	logError(0, "%s %s: Starting to get frame %02X \n", tag, __func__,
		 type);
	switch (type) {
	case SS_RAW:
		offset_force = systemInfo.u16_ssTchTxRawAddr;
		offset_sense = systemInfo.u16_ssTchRxRawAddr;
		break;
	case SS_FILTER:
		offset_force = systemInfo.u16_ssTchTxFilterAddr;
		offset_sense = systemInfo.u16_ssTchRxFilterAddr;
		break;
	case SS_STRENGTH:
		offset_force = systemInfo.u16_ssTchTxStrenAddr;
		offset_sense = systemInfo.u16_ssTchRxStrenAddr;
		break;
	case SS_BASELINE:
		offset_force = systemInfo.u16_ssTchTxBaselineAddr;
		offset_sense = systemInfo.u16_ssTchRxBaselineAddr;
		break;

	case SS_HVR_RAW:
		offset_force = systemInfo.u16_ssHvrTxRawAddr;
		offset_sense = systemInfo.u16_ssHvrRxRawAddr;
		break;
	case SS_HVR_FILTER:
		offset_force = systemInfo.u16_ssHvrTxFilterAddr;
		offset_sense = systemInfo.u16_ssHvrRxFilterAddr;
		break;
	case SS_HVR_STRENGTH:
		offset_force = systemInfo.u16_ssHvrTxStrenAddr;
		offset_sense = systemInfo.u16_ssHvrRxStrenAddr;
		break;
	case SS_HVR_BASELINE:
		offset_force = systemInfo.u16_ssHvrTxBaselineAddr;
		offset_sense = systemInfo.u16_ssHvrRxBaselineAddr;
		break;

	case SS_PRX_RAW:
		offset_force = systemInfo.u16_ssPrxTxRawAddr;
		offset_sense = systemInfo.u16_ssPrxRxRawAddr;
		break;
	case SS_PRX_FILTER:
		offset_force = systemInfo.u16_ssPrxTxFilterAddr;
		offset_sense = systemInfo.u16_ssPrxRxFilterAddr;
		break;
	case SS_PRX_STRENGTH:
		offset_force = systemInfo.u16_ssPrxTxStrenAddr;
		offset_sense = systemInfo.u16_ssPrxRxStrenAddr;
		break;
	case SS_PRX_BASELINE:
		offset_force = systemInfo.u16_ssPrxTxBaselineAddr;
		offset_sense = systemInfo.u16_ssPrxRxBaselineAddr;
		break;

	default:
		logError(1, "%s %s: Invalid type ERROR %08X\n", tag, __func__,
			 ERROR_OP_NOT_ALLOW | ERROR_GET_FRAME);
		return ERROR_OP_NOT_ALLOW | ERROR_GET_FRAME;
	}

	frame->header.type = type;

	logError(0,
		 "%s %s: Force_len = %d Sense_len = %d Offset_force = %04X Offset_sense = %04X \n",
		 tag, __func__, frame->header.force_node,
		 frame->header.sense_node, offset_force, offset_sense);

	frame->force_data =
	    (short *)kmalloc(frame->header.force_node * sizeof(short),
			     GFP_KERNEL);
	if (frame->force_data == NULL) {
		logError(1, "%s %s: can not allocate force_data ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC | ERROR_GET_FRAME);
		return ERROR_ALLOC | ERROR_GET_FRAME;
	}

	frame->sense_data =
	    (short *)kmalloc(frame->header.sense_node * sizeof(short),
			     GFP_KERNEL);
	if (frame->sense_data == NULL) {
		kfree(frame->force_data);
		frame->force_data = NULL;
		logError(1, "%s %s: can not allocate sense_data ERROR %08X\n",
			 tag, __func__, ERROR_ALLOC | ERROR_GET_FRAME);
		return ERROR_ALLOC | ERROR_GET_FRAME;
	}

	ret =
	    getFrameData(offset_force,
			 frame->header.force_node * BYTES_PER_NODE,
			 (frame->force_data));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading force data ERROR %08X\n",
			 tag, __func__, ERROR_GET_FRAME_DATA);
		kfree(frame->force_data);
		frame->force_data = NULL;
		kfree(frame->sense_data);
		frame->sense_data = NULL;
		return (ret | ERROR_GET_FRAME_DATA | ERROR_GET_FRAME);
	}

	ret =
	    getFrameData(offset_sense,
			 frame->header.sense_node * BYTES_PER_NODE,
			 (frame->sense_data));
	if (ret < OK) {
		logError(1,
			 "%s %s: error while reading sense data ERROR %08X\n",
			 tag, __func__, ERROR_GET_FRAME_DATA);
		kfree(frame->force_data);
		frame->force_data = NULL;
		kfree(frame->sense_data);
		frame->sense_data = NULL;
		return (ret | ERROR_GET_FRAME_DATA | ERROR_GET_FRAME);
	}

	logError(0, "%s Frame acquired! \n", tag);
	return frame->header.force_node + frame->header.sense_node;

}
