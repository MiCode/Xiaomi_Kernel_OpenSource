/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*				  FTS functions for getting frames			 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsCrossCompile.h"
#include "ftsCompensation.h"
#include "ftsError.h"
#include "ftsFrame.h"
#include "ftsHardware.h"
#include "ftsIO.h"
#include "ftsSoftware.h"
#include "ftsTool.h"
#include "ftsTime.h"

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

static char tag[8] = "[ FTS ]\0";
static int sense_len, force_len;

/*int getOffsetFrame(u16 address, u16 *offset)
{

	u8 data[2];
	u8 cmd = { FTS_CMD_FRAMEBUFFER_R };

	if (readCmdU16(cmd, address, data, OFFSET_LENGTH, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s getOffsetFrame: ERROR %02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}
	else {
		u8ToU16(data, offset);
		logError(0, "%s %s", tag, printHex("Offest = ", data, OFFSET_LENGTH));
		return OK;
	}

}*/

int getChannelsLength(void)
{

	int ret;
	u8 *data = (u8 *)kmalloc(2*sizeof(u8), GFP_KERNEL);

	if (data == NULL) {
		logError(1, "%s getChannelsLength: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = readB2(ADDR_SENSE_LEN, data, 2);
	if (ret < OK) {
		 logError(1, "%s getChannelsLength: ERROR %02X\n", tag, ERROR_READ_B2);
		 return (ret|ERROR_READ_B2);
	}

	sense_len = (int)data[0];
	force_len = (int)data[1];

	logError(0, "%s Force_len = %d   Sense_Len = %d\n", tag, force_len, sense_len);

	kfree(data);

	return OK;
}

int getFrameData(u16 address, int size, short **frame)
{
	int i, j, ret;
	u8 *data = (u8 *)kmalloc(size*sizeof(u8), GFP_KERNEL);
	if (data == NULL) {
		logError(1, "%s getFrameData: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = readCmdU16(FTS_CMD_FRAMEBUFFER_R, address, data, size, DUMMY_FRAMEBUFFER);
	if (ret < OK) {
		logError(1, "%s getFrameData: ERROR %02X\n", tag, ERROR_I2C_R);
				kfree(data);
		return ERROR_I2C_R;
	}
	j = 0;
	for (i = 0; i < size; i += 2) {
		(*frame)[j] = (short)((data[i + 1] << 8) + data[i]);
		j++;
	}
	kfree(data);
	return OK;
}

/*int getMSFrame(u16 type, short **frame, int keep_first_row)
{
	u16 offset;
	int size, ret;

	if (getSenseLen() == 0 || getForceLen() == 0) {
		ret=getChannelsLength();
		if (ret<OK) {
			logError(1, "%s getMSFrame: ERROR %02X\n", tag,ERROR_CH_LEN);
			return (ret|ERROR_CH_LEN);
		}
	}

	ret = getOffsetFrame(type, &offset);
	if (ret<OK) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_GET_OFFSET);
		return (ret | ERROR_GET_OFFSET);
	}

	switch (type) {
		case ADDR_RAW_TOUCH:
		case ADDR_FILTER_TOUCH:
		case ADDR_NORM_TOUCH:
		case ADDR_CALIB_TOUCH:
			if (keep_first_row ==1)
				size = ((force_len+1)*sense_len);
			else {
				size = ((force_len)*sense_len);
				offset+= (sense_len * BYTES_PER_NODE);
			}
			break;

		default:
			logError(1, "%s getMSFrame: ERROR % 02X\n", tag, ERROR_OP_NOT_ALLOW);
			return ERROR_OP_NOT_ALLOW;
	}

	*frame = (short*)kmalloc(size*sizeof(short), GFP_KERNEL);
	if (*frame==NULL) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret=getFrameData(offset, size*BYTES_PER_NODE, frame);
	if (ret<OK) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_GET_FRAME_DATA);
		return (ret|ERROR_GET_FRAME_DATA);
	}

	logError(0, "%s Frame acquired!\n", tag);
	return size;

}

int getMSKeyFrame(u16 type, short **frame) {
	u16 offset;
	int size, ret;
	u16 address;
	MutualSenseData data;

	if (type != ADDR_RAW_MS_KEY) {
		logError(1, "%s getMSKeyFrame: ERROR % 02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = getOffsetFrame(type, &offset);
	if (ret<OK) {
		logError(1, "%s getMSKeyFrame: ERROR %02X\n", tag, ERROR_GET_OFFSET);
		return (ret | ERROR_GET_OFFSET);
	}

	ret = requestCompensationData(MS_KEY);
	if (ret < OK) {
		logError(1, "%s getMSKeyFrame: readMutualSenseCompensationData ERROR %02X\n", tag, ERROR_REQU_COMP_DATA);
		return (ret | ERROR_REQU_COMP_DATA);
	}

	ret = readCompensationDataHeader(MS_KEY, &(data.header), &address);
	if (ret < OK) {
		logError(1, "%s getMSKeyFrame: readMutualSenseCompensationData ERROR %02X\n", tag, ERROR_COMP_DATA_HEADER);
		return (ret | ERROR_COMP_DATA_HEADER);
	}

	if (data.header.force_node>data.header.sense_node)
		size = data.header.force_node;
	else
		size = data.header.sense_node;

	*frame = (short*)kmalloc(size*sizeof(short), GFP_KERNEL);
	if (frame == NULL) {
		logError(1, "%s getMSKeyFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = getFrameData(offset, size*BYTES_PER_NODE, frame);
	if (ret<OK) {
		logError(1, "%s getMSKeyFrame: ERROR %02X\n", tag, ERROR_GET_FRAME_DATA);
		return (ret | ERROR_GET_FRAME_DATA);
	}

	logError(0, "%s Frame acquired!\n", tag);
}

int getSSFrame(u16 type, short **frame) {
	u16 offset;
	int size, ret;

	if (getSenseLen() == 0 || getForceLen() == 0) {
		ret = getChannelsLength();
		if (ret<0) {
			logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_CH_LEN);
			return (ret | ERROR_CH_LEN);
		}
	}

	switch (type) {
	case ADDR_RAW_HOVER_FORCE:
	case ADDR_FILTER_HOVER_FORCE:
	case ADDR_NORM_HOVER_FORCE:
	case ADDR_CALIB_HOVER_FORCE:
	case ADDR_RAW_PRX_FORCE:
	case ADDR_FILTER_PRX_FORCE:
	case ADDR_NORM_PRX_FORCE:
	case ADDR_CALIB_PRX_FORCE:
		size = ((force_len)* 1);
		break;

	case ADDR_RAW_HOVER_SENSE:
	case ADDR_FILTER_HOVER_SENSE:
	case ADDR_NORM_HOVER_SENSE:
	case ADDR_CALIB_HOVER_SENSE:
	case ADDR_RAW_PRX_SENSE:
	case ADDR_FILTER_PRX_SENSE:
	case ADDR_NORM_PRX_SENSE:
	case ADDR_CALIB_PRX_SENSE:
		size = ((1)*sense_len);
		break;

	default:
		logError(1, "%s getSSFrame: ERROR % 02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;

	}

	ret = getOffsetFrame(type, &offset);
	if (ret<OK) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_GET_OFFSET);
		return (ret | ERROR_GET_OFFSET);
	}

	*frame = (short*)kmalloc(size*sizeof(short), GFP_KERNEL);
	if (*frame == NULL) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = getFrameData(offset, size*BYTES_PER_NODE, frame);
	if (ret<OK) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_GET_FRAME_DATA);
		return (ret | ERROR_GET_FRAME_DATA);
	}

	logError(0, "%s Frame acquired!\n", tag);
	return size;

}

int getNmsFrame(u16 type, short ***frames, int *size, int keep_first_row, int fs, int n) {
	int i;
	StopWatch global, local;
	int temp;

	*frames = (short **)kmalloc(n*sizeof(short *), GFP_KERNEL);

	if (*frames == NULL) {
		logError(1, "%s getNmsFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	fs = (1*1000 / fs) ;

	startStopWatch(&global);
	for (i = 0; i < n; i++) {

		startStopWatch(&local);

		*size = getMSFrame(type, ((*frames)+i), keep_first_row);
		if (*size < OK) {
			logError(1, "%s getNFrame: getFrame failed\n",tag);
			return *size;
		}

		stopStopWatch(&local);
		temp = elapsedMillisecond(&local);
		logError(0, "%s Iteration %d performed in %d ms... the process wait for %ld ms\n\n", tag, i, temp, (unsigned long)(fs - temp));

		if (temp < fs)
			msleep((unsigned long)(fs - temp));

	}

	stopStopWatch(&global);
	temp = elapsedMillisecond(&global);
	logError(0, "%s Global Iteration performed in %d ms\n", tag, temp);
	temp /= n;
	logError(0, "%s Mean Iteration performed in %d ms\n", tag, temp);
	return (1000 / (temp));

}*/

int getSenseLen(void)
{
	int ret;
	if (sense_len != 0)
		return sense_len;
	ret = getChannelsLength();
	if (ret < OK)
		return ret;
	else
		return sense_len;
}

int getForceLen(void)
{
	int ret;
	if (force_len != 0)
		return force_len;
	ret = getChannelsLength();
	if (ret < OK)
		return ret;
	else
		return force_len;
}

int requestFrame(u16 type)
{
	int retry = 0;
	int ret;
	u16 answer;

	int event_to_search[1];
	u8 readEvent[FIFO_EVENT_SIZE];

	u8 cmd[3] = { FTS_CMD_REQU_FRAME_DATA, 0x00, 0x00 };
	/*  B7 is the command for asking frame data */
	event_to_search[0] = (int)EVENTID_FRAME_DATA_READ;

	u16ToU8(type, &cmd[1]);

	while (retry < FRAME_DATA_READ_RETRY) {
		logError(0, "%s %s", tag, printHex("Command = ", cmd, 3));
		ret = fts_writeFwCmd(cmd, 3);
		/* send the request to the chip to load in memory the Frame Data */
		if (ret < OK) {
			logError(1, "%s requestFrame:  ERROR %02X\n", tag, ERROR_I2C_W);
			return ERROR_I2C_W;
		}
		ret = pollForEvent(event_to_search, 1, readEvent, TIMEOUT_REQU_COMP_DATA);
		if (ret < OK) {
			logError(0, "%s Event did not Found at %d attemp!\n", tag, retry + 1);
			retry += 1;
		} else {
			retry = 0;
			break;
		}
	}

	if (retry == FRAME_DATA_READ_RETRY) {
		logError(1, "%s requestFrame: ERROR %02X\n", tag, ERROR_TIMEOUT);
		return ERROR_TIMEOUT;
	}

	u8ToU16_le(&readEvent[1], &answer);

	if (answer == type)
		return OK;
	logError(1, "%s The event found has a different type of Frame data ERROR %02X\n", tag, ERROR_DIFF_COMP_TYPE);
	return ERROR_DIFF_COMP_TYPE;

}

int readFrameDataHeader(u16 type, DataHeader *header)
{

	u16 offset = ADDR_FRAMEBUFFER_DATA;
	u16 answer;
	u8 data[FRAME_DATA_HEADER];

	if (readCmdU16(FTS_CMD_FRAMEBUFFER_R, offset, data, FRAME_DATA_HEADER, DUMMY_FRAMEBUFFER) < 0) {
		logError(1, "%s  readFrameDataHeader: ERROR %02X\n", tag, ERROR_I2C_R);
		return ERROR_I2C_R;
	}
	logError(0, "%s Read Data Header done!\n", tag);

	if (data[0] != FRAME_HEADER_SIGNATURE) {
		logError(1, "%s readFrameDataHeader: ERROR %02X The Header Signature was wrong!  %02X != %02X\n", tag, ERROR_WRONG_COMP_SIGN, data[0], HEADER_SIGNATURE);
		return ERROR_WRONG_COMP_SIGN;
	}

	u8ToU16_le(&data[1], &answer);

	if (answer != type) {
		logError(1, "%s readFrameDataHeader:  ERROR %02X\n", tag, ERROR_DIFF_COMP_TYPE);
		return ERROR_DIFF_COMP_TYPE;
	}

	logError(0, "%s Type of Frame data OK!\n", tag);

	header->type = type;
	header->force_node = (int)data[4];
	header->sense_node = (int)data[5];

	return OK;

}

int getMSFrame2(u16 type, MutualSenseFrame *frame)
{
	u16 offset = ADDR_FRAMEBUFFER_DATA+FRAME_DATA_HEADER;
	int size, ret;

	if (!(type == MS_TOUCH_ACTIVE || type == MS_TOUCH_LOW_POWER || type == MS_TOUCH_ULTRA_LOW_POWER || type == MS_KEY)) {
		logError(1, "%s getMSFrame: Choose a MS type of frame data ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = requestFrame(type);
	if (ret < 0) {
		logError(1, "%s readMutualSenseCompensationData: ERROR %02X\n", tag, ERROR_REQU_COMP_DATA);
		return (ret | ERROR_REQU_COMP_DATA);
	}

	ret = readFrameDataHeader(type, &(frame->header));
	if (ret < 0) {
		logError(1, "%s readMutualSenseCompensationData: ERROR %02X\n", tag, ERROR_COMP_DATA_HEADER);
		return (ret | ERROR_COMP_DATA_HEADER);
	}

	switch (type) {
	case MS_TOUCH_ACTIVE:
	case MS_TOUCH_LOW_POWER:
	case MS_TOUCH_ULTRA_LOW_POWER:
		size = frame->header.force_node*frame->header.sense_node;
		break;
	case MS_KEY:
		if (frame->header.force_node > frame->header.sense_node)
		/* or use directly the number in the ftsChip */
			size = frame->header.force_node;
		else
			size = frame->header.sense_node;
				frame->header.force_node = 1;
		frame->header.sense_node = size;
		break;

	default:
		logError(1, "%s getMSFrame: ERROR % 02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	frame->node_data = (short *)kmalloc(size*sizeof(short), GFP_KERNEL);
	if (frame->node_data == NULL) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = getFrameData(offset, size*BYTES_PER_NODE, &(frame->node_data));
	if (ret < OK) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_GET_FRAME_DATA);
		return (ret | ERROR_GET_FRAME_DATA);
	}
	/*  if you want to access one node i,j, you should compute the offset like: offset = i*columns + j = > frame[i, j] */

	logError(0, "%s Frame acquired!\n", tag);
	frame->node_data_size = size;
	return size; /* return the number of data put inside frame */

}

int getSSFrame2(u16 type, SelfSenseFrame *frame)
{
	u16 offset = ADDR_FRAMEBUFFER_DATA + FRAME_DATA_HEADER;
	int size, ret;
	short *temp = NULL;

	if (!(type == SS_TOUCH || type == SS_KEY || type == SS_HOVER || type == SS_PROXIMITY)) {
		logError(1, "%s getSSFrame: Choose a SS type of frame data ERROR %02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	ret = requestFrame(type);
	if (ret < 0) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_REQU_COMP_DATA);
		return (ret | ERROR_REQU_COMP_DATA);
	}

	ret = readFrameDataHeader(type, &(frame->header));
	if (ret < 0) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_COMP_DATA_HEADER);
		return (ret | ERROR_COMP_DATA_HEADER);
	}

	switch (type) {
	case SS_TOUCH:
	case SS_HOVER:
	case SS_PROXIMITY:
		size = frame->header.force_node+frame->header.sense_node;
		break;

	default:
		logError(1, "%s getSSFrame: ERROR % 02X\n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	temp = (short *)kmalloc(size*sizeof(short), GFP_KERNEL);
	if (temp == NULL) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	ret = getFrameData(offset, size*BYTES_PER_NODE, &temp);
	if (ret < OK) {
		logError(1, "%s getMSFrame: ERROR %02X\n", tag, ERROR_GET_FRAME_DATA);
		return (ret | ERROR_GET_FRAME_DATA);
	}

	frame->force_data = (short *)kmalloc(frame->header.force_node*sizeof(short), GFP_KERNEL);
	if (frame->force_data == NULL) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	memcpy(frame->force_data, temp, frame->header.force_node*sizeof(short));

	frame->sense_data = (short *)kmalloc(frame->header.sense_node*sizeof(short), GFP_KERNEL);
	if (frame->sense_data == NULL) {
		logError(1, "%s getSSFrame: ERROR %02X\n", tag, ERROR_ALLOC);
		return ERROR_ALLOC;
	}

	memcpy(frame->sense_data, &temp[frame->header.force_node], frame->header.sense_node*sizeof(short));

	logError(0, "%s Frame acquired!\n", tag);
	kfree(temp);
	return size;	/* return the number of data put inside frame */

}
