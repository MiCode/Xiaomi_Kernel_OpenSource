/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                     FTS Gesture Utilities								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"
#include "ftsError.h"
#include "ftsGesture.h"
#include "ftsIO.h"
#include "ftsTool.h"


static char tag[8] = "[ FTS ]\0";

static u8 gesture_mask[GESTURE_MASK_SIZE] = {0};
static u8 custom_gestures[GESTURE_CUSTOM_NUMBER][GESTURE_CUSTOM_POINTS];
static u8 custom_gesture_index[GESTURE_CUSTOM_NUMBER] = {0};
static int refreshGestureMask;
struct mutex gestureMask_mutex;

int updateGestureMask(u8 *mask, int size, int en)
{
	u8 temp;
	int i;

	if (mask != NULL) {
		if (size <= GESTURE_MASK_SIZE) {
			if (en == FEAT_ENABLE) {
				mutex_lock(&gestureMask_mutex);
				log_error("%s updateGestureMask: setting gesture mask to enable...\n", tag);
				if (mask != NULL) {
					for (i = 0; i < size; i++) {
						gesture_mask[i] = gesture_mask[i] | mask[i];
					}
				}
				refreshGestureMask = 1;
				log_error("%s updateGestureMask: gesture mask to enable SET! \n", tag);
				mutex_unlock(&gestureMask_mutex);
				return OK;
			}

			else if (en == FEAT_DISABLE) {
				mutex_lock(&gestureMask_mutex);
				log_error("%s updateGestureMask: setting gesture mask to disable...\n", tag);
				for (i = 0; i < size; i++) {
					temp = gesture_mask[i] ^ mask[i];
					gesture_mask[i] = temp & gesture_mask[i];
				}
				log_error("%s updateGestureMask: gesture mask to disable SET! \n", tag);
				refreshGestureMask = 1;
				mutex_unlock(&gestureMask_mutex);
				return OK;
			} else {
				log_error("%s updateGestureMask: Enable parameter Invalid! %d != %d ERROR %08X", tag, FEAT_DISABLE, FEAT_ENABLE, ERROR_OP_NOT_ALLOW);
				return ERROR_OP_NOT_ALLOW;
			}
		} else {
			log_error("%s updateGestureMask: Size not valid! %d ERROR %08X \n", tag, size, GESTURE_MASK_SIZE);
			return ERROR_OP_NOT_ALLOW;
		}
	} else {
		log_error("%s updateGestureMask: Mask NULL! ERROR %08X \n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}


}

int enableGesture(u8 *mask, int size)
{
	u8 cmd[GESTURE_MASK_SIZE + 2];
	u8 readData[FIFO_EVENT_SIZE];
	int i, res;
	int event_to_search[4] = {EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_ENABLE};
	log_debug("%s Trying to enable gesture...\n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_ENABLE;

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size; i++) {
				cmd[i + 2] = mask[i];
				gesture_mask[i] = gesture_mask[i] | mask[i];
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
			log_error("%s enableGesture: ERROR %08X\n", tag, res);
			goto END;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

		if (res < OK) {
			log_error("%s enableGesture: pollForEvent ERROR %08X\n", tag, res);
			goto END;
		}

		if (readData[4] != 0x00) {
			log_error("%s enableGesture: ERROR %08X\n", tag, ERROR_GESTURE_ENABLE_FAIL);
			res = ERROR_GESTURE_ENABLE_FAIL;
			goto END;
		}

		log_debug("%s enableGesture DONE!\n", tag);
		res = OK;
	END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		log_error("%s enableGesture: Size not valid! %d > %d ERROR %08X\n", tag, size, GESTURE_MASK_SIZE, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

int disableGesture(u8 *mask, int size)
{
	u8 cmd[2 + GESTURE_MASK_SIZE];
	u8 readData[FIFO_EVENT_SIZE];
	u8 temp;
	int i, res;
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_DISABLE };
	log_debug("%s Trying to disable gesture...\n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_DISABLE;

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size; i++) {
				cmd[i + 2] = mask[i];
				temp = gesture_mask[i] ^ mask[i];
				gesture_mask[i] = temp & gesture_mask[i];
			}

			while (i < GESTURE_MASK_SIZE) {
				cmd[i + 2] = 0x00;
				i++;
			}
		} else {
			for (i = 0; i < GESTURE_MASK_SIZE; i++) {
				cmd[i + 2] = 0xFF;
			}
		}

		res = fts_writeFwCmd(cmd, 2 + GESTURE_MASK_SIZE);

		if (res < OK) {
			log_error("%s disableGesture: ERROR %08X\n", tag, res);
			goto END;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

		if (res < OK) {
			log_error("%s disableGesture: pollForEvent ERROR %08X\n", tag, res);
			goto END;
		}

		if (readData[4] != 0x00) {
			log_error("%s disableGesture: ERROR %08X\n", tag, ERROR_GESTURE_ENABLE_FAIL);
			res = ERROR_GESTURE_ENABLE_FAIL;
			goto END;
		}

		log_debug("%s disableGesture DONE!\n", tag);
		res = OK;
	END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		log_error("%s disableGesture: Size not valid! %d > %d ERROR %08X\n", tag, size, GESTURE_MASK_SIZE, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}
}

int startAddCustomGesture(u8 gestureID)
{
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_START_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GESTURE_START_ADD };
	res = fts_writeFwCmd(cmd, 3);

	if (res < OK) {
		log_error("%s startAddCustomGesture: Impossible to start adding custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

	if (res < OK) {
		log_error("%s startAddCustomGesture: start add event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {
		log_error("%s startAddCustomGesture: start add event status not OK! ERROR %08X\n", tag, readData[4]);
		return ERROR_GESTURE_START_ADD;
	}

	return OK;
}

int finishAddCustomGesture(u8 gestureID)
{
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_FINISH_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GESTURE_FINISH_ADD };
	res = fts_writeFwCmd(cmd, 3);

	if (res < OK) {
		log_error("%s finishAddCustomGesture: Impossible to finish adding custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

	if (res < OK) {
		log_error("%s finishAddCustomGesture: finish add event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {
		log_error("%s finishAddCustomGesture: finish add event status not OK! ERROR %08X\n", tag, readData[4]);
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
	u8 readData[FIFO_EVENT_SIZE];
	log_debug("%s Starting adding custom gesture procedure...\n", tag);
	res = startAddCustomGesture(gestureID);

	if (res < OK) {
		log_error("%s loadCustomGesture: unable to start adding procedure! ERROR %08X\n", tag, res);
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
			log_error("%s loadCustomGesture: unable to start adding procedure! ERROR %08X\n", tag, res);
			return res;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

		if (res < OK) {
			log_error("%s loadCustomGesture: add event not found! ERROR %08X\n", tag, res);
			return res;
		}

		if (readData[2] != gestureID || readData[4] != 0x00) {
			log_error("%s loadCustomGesture: add event status not OK! ERROR %08X\n", tag, readData[4]);
			return ERROR_GESTURE_DATA_ADD;
		}

		remaining -= toWrite;
		offset += toWrite / 2;
	}

	res = finishAddCustomGesture(gestureID);

	if (res < OK) {
		log_error("%s loadCustomGesture: unable to finish adding procedure! ERROR %08X\n", tag, res);
		return res;
	}

	log_debug("%s Adding custom gesture procedure DONE!\n", tag);
	return OK;
}

int reloadCustomGesture(void)
{
	int res, i;
	log_debug("%s Starting reload Gesture Template...\n", tag);

	for (i = 0; i < GESTURE_CUSTOM_NUMBER; i++) {
		if (custom_gesture_index[i] == 1) {
			res = loadCustomGesture(custom_gestures[i], GESTURE_CUSTOM_OFFSET + i);

			if (res < OK) {
				log_error("%s reloadCustomGesture: Impossible to load custom gesture ID = %02X! ERROR %08X\n", tag, GESTURE_CUSTOM_OFFSET + i, res);
				return res;
			}
		}
	}

	log_debug("%s Reload Gesture Template DONE!\n", tag);
	return OK;
}

int enterGestureMode(int reload)
{
	u8 cmd = FTS_CMD_GESTURE_MODE;
	int res, ret;
	res = fts_disableInterrupt();

	if (res < OK) {
		log_error("%s enterGestureMode: ERROR %08X\n", tag, res | ERROR_DISABLE_INTER);
		return res | ERROR_DISABLE_INTER;
	}
	if (reload == 1 || refreshGestureMask == 1) {
		if (reload == 1) {
			res = reloadCustomGesture();

			if (res < OK) {
				log_error("%s enterGestureMode: impossible reload custom gesture! ERROR %08X\n", tag, res);
				goto END;
			}

			res = disableGesture(NULL, 0);

			if (res < OK) {
				log_error("%s enterGestureMode: disableGesture ERROR %08X\n", tag, res);
				goto END;
			}

			res = enableGesture(NULL, 0);

			if (res < OK) {
				log_error("%s enterGestureMode: enableGesture ERROR %08X\n", tag, res);
				goto END;
			}
		}
	  refreshGestureMask = 0;
	}

	res = fts_writeFwCmd(&cmd, 1);

	if (res < OK) {
		log_error("%s enterGestureMode: enter gesture mode ERROR %08X\n", tag, res);
		goto END;
	}

	res = OK;
END:
	ret = fts_enableInterrupt();

	if (ret < OK) {
		log_error("%s enterGestureMode: fts_enableInterrupt ERROR %08X\n", tag, res | ERROR_ENABLE_INTER);
		res |= ret | ERROR_ENABLE_INTER;
	}

	return res;
}

int addCustomGesture(u8 *data, int size, u8 gestureID)
{
	int index, res, i;
	index = gestureID - GESTURE_CUSTOM_OFFSET;
	log_debug("%s Starting Custom Gesture Adding procedure...\n", tag);

	if (size != GESTURE_CUSTOM_POINTS || (gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID != GES_ID_CUST5)) {
		log_error("%s addCustomGesture: Invalid size (%d) or Custom GestureID (%02X)! ERROR %08X\n", tag, size, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	for (i = 0; i < GESTURE_CUSTOM_POINTS; i++) {
		custom_gestures[index][i] = data[i];
	}

	res = loadCustomGesture(custom_gestures[index], gestureID);

	if (res < OK) {
		log_error("%s addCustomGesture: impossible to load the custom gesture! ERROR %08X\n", tag, res);
		return res;
	}

	custom_gesture_index[index] = 1;
	log_debug("%s Custom Gesture Adding procedure DONE!\n", tag);
	return OK;
}

int removeCustomGesture(u8 gestureID)
{
	int res, index;
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GETURE_REMOVE_CUSTOM, gestureID };
	int event_to_search[4] = {EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GETURE_REMOVE_CUSTOM };
	u8 readData[FIFO_EVENT_SIZE];
	index = gestureID - GESTURE_CUSTOM_OFFSET;
	log_debug("%s Starting Custom Gesture Removing procedure...\n", tag);

	if (gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID && GES_ID_CUST5) {
		log_error("%s removeCustomGesture: Invalid size or Custom GestureID (%02X)! ERROR %08X\n", tag, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	res = fts_writeFwCmd(cmd, 3);

	if (res < OK) {
		log_error("%s removeCustomGesture: Impossible to remove custom gesture ID = %02X! ERROR %08X\n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);

	if (res < OK) {
		log_error("%s removeCustomGesture: remove event not found! ERROR %08X\n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {
		log_error("%s removeCustomGesture: remove event status not OK! ERROR %08X\n", tag, readData[4]);
		return ERROR_GESTURE_REMOVE;
	}

	custom_gesture_index[index] = 0;
	log_debug("%s Custom Gesture Remove procedure DONE!\n", tag);
	return OK;
}


int isAnyGestureActive(void)
{
	int res = 0;

	while (res < (GESTURE_MASK_SIZE-1) && gesture_mask[res] == 0) {
		res++;
	}

	if (gesture_mask[res] != 0) {
		log_error("%s %s: Active Gestures Found! gesture_mask[%d] = %02X !\n", tag, __func__, res, gesture_mask[res]);
		return FEAT_ENABLE;
	} else {
		log_error("%s %s: All Gestures Disabled!\n", tag, __func__);
		return FEAT_DISABLE;
	}
}

int gestureIDtoGestureMask(u8 id, u8 *mask)
{
	log_error("%s %s: Index = %d Position = %d !\n", tag, __func__, ((int)((id) / 8)), (id % 8));
	mask[((int)((id) / 8))] |= 0x01 << (id % 8);
	return OK;
}
