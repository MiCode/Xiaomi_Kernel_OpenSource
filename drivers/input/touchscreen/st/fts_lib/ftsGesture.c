/*

**************************************************************************
**						STMicroelectronics							**
**************************************************************************
**						marco.cali@st.com								**
**************************************************************************
*																		*
*					 FTS Gesture Utilities								 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"
#include "ftsError.h"
#include "ftsGesture.h"
#include "ftsIO.h"
#include "ftsTool.h"

static char tag[8] = "[ FTS ]\0";

static u8 gesture_mask[GESTURE_MASK_SIZE] = { 0 };
static u8 custom_gestures[GESTURE_CUSTOM_NUMBER][GESTURE_CUSTOM_POINTS];
static u8 custom_gesture_index[GESTURE_CUSTOM_NUMBER] = { 0 };

int enableGesture(u8 *mask, int size)
{
	u8 cmd[size+2];
	u8 readData[FIFO_EVENT_SIZE] = {0};
	int i, res;
	int event_to_search[4] = {EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_ENABLE};

	logError(0, "%s Trying to enable gesture...\n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_ENABLE;

	if (size <= GESTURE_MASK_SIZE) {
	if (mask != NULL) {
		for (i = 0; i < size; i++) {
			cmd[i + 2] = mask[i];
			gesture_mask[i] = gesture_mask[i]|mask[i];
			/* back up of the gesture enabled */
		}
		while (i < GESTURE_MASK_SIZE) {
			cmd[i + 2] = gesture_mask[i];
			i++;
		}
	} else {
		for (i = 0; i < GESTURE_MASK_SIZE; i++) {
			cmd[i + 2] = gesture_mask[i];
		}
	}

	res = fts_writeFwCmd(cmd, GESTURE_MASK_SIZE + 2);
	if (res < OK) {
		logError(1, "%s enableGesture: ERROR %08X\n", tag, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s enableGesture: pollForEvent ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[4] != 0x00) {
		logError(1, "%s enableGesture: ERROR %08X\n", tag, ERROR_GESTURE_ENABLE_FAIL);
		return ERROR_GESTURE_ENABLE_FAIL;
	}

	logError(0, "%s enableGesture DONE!\n", tag);
	return OK;
	} else {
		logError(1, "%s enableGesture: Size not valid! %d > %d ERROR %08X\n", tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}

}

int disableGesture(u8 *mask, int size)
{
	u8 cmd[2+GESTURE_MASK_SIZE];
	u8 readData[FIFO_EVENT_SIZE] = {0};
	u8 temp;
	int i, res;
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_DISABLE };

	logError(0, "%s Trying to disable gesture...\n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_DISABLE;

	if (size <= GESTURE_MASK_SIZE) {
	if (mask != NULL) {
		for (i = 0; i < size; i++) {
			cmd[i + 2] = mask[i];
			temp = gesture_mask[i] ^ mask[i];
			/*  enabled XOR disabled */
			gesture_mask[i] = temp & gesture_mask[i];
			/* disable the gestures that were enabled */
		}
		while (i < GESTURE_MASK_SIZE) {
			cmd[i + 2] = gesture_mask[i];
			/* disable all the other gesture not specified */
			gesture_mask[i] = 0x00;
			i++;
		}
	} else {
		for (i = 0; i < GESTURE_MASK_SIZE; i++) {
			cmd[i + 2] = gesture_mask[i];
		}
	}

	res = fts_writeFwCmd(cmd, 2 + GESTURE_MASK_SIZE);
	if (res < OK) {
		logError(1, "%s disableGesture: ERROR %08X\n", tag, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s disableGesture: pollForEvent ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[4] != 0x00) {
		logError(1, "%s disableGesture: ERROR %08X\n", tag, ERROR_GESTURE_ENABLE_FAIL);
		return ERROR_GESTURE_ENABLE_FAIL;
	}

	logError(0, "%s disableGesture DONE!\n", tag);
	return OK;
	} else {
		logError(1, "%s disableGesture: Size not valid! %d > %d ERROR %08X\n", tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}
}

int startAddCustomGesture(u8 gestureID)
{
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_START_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE] = {0};
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GESTURE_START_ADD };

	res = fts_writeFwCmd(cmd, 3);
	if (res < OK) {
		logError(1, "%s startAddCustomGesture: Impossible to start adding custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s startAddCustomGesture: start add event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) { /* check of gestureID is redundant */
		logError(1, "%s startAddCustomGesture: start add event status not OK! ERROR %08X\n", tag, readData[4]);
		return ERROR_GESTURE_START_ADD;
	}

	return OK;
}

int finishAddCustomGesture(u8 gestureID)
{
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_FINISH_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE] = {0};
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GESTURE_FINISH_ADD };

	res = fts_writeFwCmd(cmd, 3);
	if (res < OK) {
		logError(1, "%s finishAddCustomGesture: Impossible to finish adding custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s finishAddCustomGesture: finish add event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {	/* check of gestureID is redundant */
		logError(1, "%s finishAddCustomGesture: finish add event status not OK! ERROR %08X\n", tag, readData[4]);
		return ERROR_GESTURE_FINISH_ADD;
	}

	return OK;

}

int loadCustomGesture(u8 *template, u8 gestureID)
{
	int res, i;
	int remaining = GESTURE_CUSTOM_POINTS;
	int toWrite, offset = 0;
	u8 cmd[TEMPLATE_CHUNK + 5];
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GESTURE_DATA_ADD };
	u8 readData[FIFO_EVENT_SIZE] = {0};

	logError(0, "%s Starting adding custom gesture procedure...\n", tag);

	res = startAddCustomGesture(gestureID);
	if (res < OK) {
		logError(1, "%s loadCustomGesture: unable to start adding procedure! ERROR %08X\n", tag, res);
		return res;
	}

	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_DATA_ADD;
	cmd[2] = gestureID;
	while (remaining > 0) {
		if (remaining > TEMPLATE_CHUNK) {
			toWrite = TEMPLATE_CHUNK;
		} else {
			toWrite = remaining;
		}

		cmd[3] = toWrite;
		cmd[4] = offset;
		for (i = 0; i < toWrite; i++) {
			cmd[i + 5] = template[i];
		}

		res = fts_writeFwCmd(cmd, toWrite + 5);
		if (res < OK) {
			logError(1, "%s loadCustomGesture: unable to start adding procedure! ERROR %08X\n", tag, res);
			return res;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
		if (res < OK) {
			logError(1, "%s loadCustomGesture: add event not found! ERROR %08X\n", tag, res);
			return res;
		}

		if (readData[2] != gestureID || readData[4] != 0x00) { /* check of gestureID is redundant */
			logError(1, "%s loadCustomGesture: add event status not OK! ERROR %08X\n", tag, readData[4]);
			return ERROR_GESTURE_DATA_ADD;
		}

		remaining -= toWrite;
		offset += toWrite / 2;
	}

	res = finishAddCustomGesture(gestureID);
	if (res < OK) {
		logError(1, "%s loadCustomGesture: unable to finish adding procedure! ERROR %08X\n", tag, res);
		return res;
	}

	logError(0, "%s Adding custom gesture procedure DONE!\n", tag);
	return OK;

}

int reloadCustomGesture(void)
{
	int res, i;

	logError(0, "%s Starting reload Gesture Template...\n", tag);

	for (i = 0; i < GESTURE_CUSTOM_NUMBER; i++) {
		if (custom_gesture_index[i] == 1) {
			res = loadCustomGesture(custom_gestures[i], GESTURE_CUSTOM_OFFSET+i);
			if (res < OK) {
				logError(1, "%s reloadCustomGesture: Impossible to load custom gesture ID = %02X! ERROR %08X\n", tag, GESTURE_CUSTOM_OFFSET + i, res);
				return res;
			}
		}
	}

	logError(0, "%s Reload Gesture Template DONE!\n", tag);
	return OK;

}

int enterGestureMode(int reload)
{
	u8 cmd = FTS_CMD_GESTURE_MODE;
	int res, ret;

	res = fts_disableInterrupt();
	if (res < OK) {
		logError(1, "%s enterGestureMode: ERROR %08X\n", tag, res|ERROR_DISABLE_INTER);
		return res | ERROR_DISABLE_INTER;
	}

	if (reload == 1) {

		res = reloadCustomGesture();
		if (res < OK) {
			logError(1, "%s enterGestureMode: impossible reload custom gesture! ERROR %08X\n", tag, res);
			goto END;
		}

		res = disableGesture(NULL, 0);
		if (res < OK) {
			logError(1, "%s enterGestureMode: disableGesture ERROR %08X\n", tag, res);
			goto END;
		}

		res = enableGesture(NULL, 0);
		if (res < OK) {
			logError(1, "%s enterGestureMode: enableGesture ERROR %08X\n", tag, res);
			goto END;
		}
	}

	res = fts_writeFwCmd(&cmd, 1);
	if (res < OK) {
		logError(1, "%s enterGestureMode: enter gesture mode ERROR %08X\n", tag, res);
		goto END;
	}

	res = OK;
END:
	ret = fts_enableInterrupt();
	if (ret < OK) {
		logError(1, "%s enterGestureMode: fts_enableInterrupt ERROR %08X\n", tag, res | ERROR_ENABLE_INTER);
		res |= ret | ERROR_ENABLE_INTER;
	}

	return res;
}

int addCustomGesture(u8 *data, int size, u8 gestureID)
{
	int index, res, i;

	index = gestureID - GESTURE_CUSTOM_OFFSET;

	logError(0, "%s Starting Custom Gesture Adding procedure...\n", tag);
	if (size != GESTURE_CUSTOM_POINTS && gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID && GES_ID_CUST5) {
		logError(1, "%s addCustomGesture: Invalid size (%d) or Custom GestureID (%02X)! ERROR %08X\n", tag, size, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	for (i = 0; i < GESTURE_CUSTOM_POINTS; i++) {
		custom_gestures[index][i] = data[i];
	}

	res = loadCustomGesture(custom_gestures[index], gestureID);
	if (res < OK) {
		logError(1, "%s addCustomGesture: impossible to load the custom gesture! ERROR %08X\n", tag, res);
		return res;
	}

	custom_gesture_index[index] = 1;
	logError(0, "%s Custom Gesture Adding procedure DONE!\n", tag);
	return OK;
}

int removeCustomGesture(u8 gestureID)
{
	int res, index;
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GETURE_REMOVE_CUSTOM, gestureID };
	int event_to_search[4] = {EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GETURE_REMOVE_CUSTOM };
	u8 readData[FIFO_EVENT_SIZE] = {0};

	index = gestureID - GESTURE_CUSTOM_OFFSET;

	logError(0, "%s Starting Custom Gesture Removing procedure...\n", tag);
	if (gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID && GES_ID_CUST5) {
		logError(1, "%s removeCustomGesture: Invalid size (%d) or Custom GestureID (%02X)! ERROR %08X\n", tag, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	res = fts_writeFwCmd(cmd, 3);/* when a gesture is removed, it is also disabled automatically */
	if (res < OK) {
		logError(1, "%s removeCustomGesture: Impossible to remove custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s removeCustomGesture: remove event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) { /* check of gestureID is redundant */
		logError(1, "%s removeCustomGesture: remove event status not OK! ERROR %08X\n", tag, readData[4]);
		return ERROR_GESTURE_REMOVE;
	}

	custom_gesture_index[index] = 0;
	logError(0, "%s Custom Gesture Remove procedure DONE!\n", tag);
	return OK;

}
