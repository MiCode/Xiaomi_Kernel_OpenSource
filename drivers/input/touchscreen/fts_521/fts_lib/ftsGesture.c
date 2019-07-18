/*

**************************************************************************
**                        STMicroelectronics 							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                     FTS Gesture Utilities								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsGesture.c
* \brief Contains all the functions and variable to handle the Gesture Detection features
*/

#include "ftsSoftware.h"
#include "ftsCore.h"
#include "ftsError.h"
#include "ftsGesture.h"
#include "ftsIO.h"
#include "ftsTime.h"
#include "ftsTool.h"

static u8 gesture_mask[GESTURE_MASK_SIZE] = { 0 };
u16 gesture_coordinates_x[GESTURE_MAX_COORDS_PAIRS_REPORT] = { 0 };
u16 gesture_coordinates_y[GESTURE_MAX_COORDS_PAIRS_REPORT] = { 0 };
int gesture_coords_reported = ERROR_OP_NOT_ALLOW;
static u8 refreshGestureMask;
struct mutex gestureMask_mutex;

/**
 * Update the gesture mask stored in the driver and have to be used in gesture mode
 * @param mask pointer to a byte array which store the gesture mask update that want to be performed.
 * @param size dimension in byte of mask. This size can be <= GESTURE_MASK_SIZE. If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos and starting from the less significant byte.
 * @param en 0 = enable the gestures set in mask, 1 = disable the gestures set in mask
 * @return OK if success or an error code which specify the type of error encountered
 */
int updateGestureMask(u8 *mask, int size, int en)
{
	u8 temp;
	int i;

	if (mask != NULL) {
		if (size <= GESTURE_MASK_SIZE) {
			if (en == FEAT_ENABLE) {
				mutex_lock(&gestureMask_mutex);
				logError(0,
					 "%s updateGestureMask: setting gesture mask to enable...\n",
					 tag);
				if (mask != NULL) {
					for (i = 0; i < size; i++) {
						gesture_mask[i] = gesture_mask[i] | mask[i];
					}
				}
				refreshGestureMask = 1;
				logError(0,
					 "%s updateGestureMask: gesture mask to enable SET! \n",
					 tag);
				mutex_unlock(&gestureMask_mutex);
				return OK;
			}

			else if (en == FEAT_DISABLE) {
				mutex_lock(&gestureMask_mutex);
				logError(0,
					 "%s updateGestureMask: setting gesture mask to disable...\n",
					 tag);
				for (i = 0; i < size; i++) {
					temp = gesture_mask[i] ^ mask[i];
					gesture_mask[i] =
					    temp & gesture_mask[i];
				}
				logError(0,
					 "%s updateGestureMask: gesture mask to disable SET! \n",
					 tag);
				refreshGestureMask = 1;
				mutex_unlock(&gestureMask_mutex);
				return OK;
			} else {
				logError(1,
					 "updateGestureMask: Enable parameter Invalid! %d != %d or %d ERROR %08X",
					 tag, FEAT_DISABLE, FEAT_ENABLE,
					 ERROR_OP_NOT_ALLOW);
				return ERROR_OP_NOT_ALLOW;
			}
		} else {
			logError(1,
				 "%s updateGestureMask: Size not valid! %d > %d ERROR %08X \n",
				 tag, size, GESTURE_MASK_SIZE);
			return ERROR_OP_NOT_ALLOW;
		}
	} else {
		logError(1, "%s updateGestureMask: Mask NULL! ERROR %08X \n",
			 tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

}

/**
 * Enable in the FW the gesture mask to be used in gesture mode
 * @param mask pointer to a byte array which store the gesture mask update that want to be sent to the FW, if NULL, will be used gesture_mask set previously without any changes.
 * @param size dimension in byte of mask. This size can be <= GESTURE_MASK_SIZE. If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos and starting from the less significant byte.
 * @return OK if success or an error code which specify the type of error encountered
 */
int enableGesture(u8 *mask, int size)
{
	int i, res;

	logError(0, "%s Trying to enable gesture... \n", tag);

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size; i++) {
				gesture_mask[i] = gesture_mask[i] | mask[i];
			}
		}

		res =
		    setFeatures(FEAT_SEL_GESTURE, gesture_mask,
				GESTURE_MASK_SIZE);
		if (res < OK) {
			logError(1, "%s enableGesture: ERROR %08X \n", tag,
				 res);
			goto END;
		}

		logError(0, "%s enableGesture DONE! \n", tag);
		res = OK;

END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		logError(1,
			 "%s enableGesture: Size not valid! %d > %d ERROR %08X \n",
			 tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}

}

/**
 * Disable in the FW the gesture mask to be used in gesture mode
 * @param mask pointer to a byte array which store the gesture mask update that want to be sent to the FW, if NULL, all the gestures will be disabled.
 * @param size dimension in byte of mask. This size can be <= GESTURE_MASK_SIZE. If size < GESTURE_MASK_SIZE the bytes of mask are considering continuos and starting from the less significant byte.
 * @return OK if success or an error code which specify the type of error encountered
 */
int disableGesture(u8 *mask, int size)
{
	u8 temp;
	int i, res;
	u8 *pointer;

	logError(0, "%s Trying to disable gesture... \n", tag);

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size; i++) {

				temp = gesture_mask[i] ^ mask[i];
				gesture_mask[i] = temp & gesture_mask[i];
			}

			pointer = gesture_mask;
		} else {
			i = 0;
			pointer = (u8 *) & i;
		}

		res = setFeatures(FEAT_SEL_GESTURE, pointer, GESTURE_MASK_SIZE);
		if (res < OK) {
			logError(1, "%s disableGesture: ERROR %08X \n", tag,
				 res);
			goto END;
		}

		logError(0, "%s disableGesture DONE! \n", tag);

		res = OK;

END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	} else {
		logError(1,
			 "%s disableGesture: Size not valid! %d > %d ERROR %08X \n",
			 tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}
}

/**
 * Perform all the steps required to put the chip in gesture mode
 * @param reload if set to 1, before entering in gesture mode it will re-enable in the FW the last defined gesture mask
 * @return OK if success or an error code which specify the type of error encountered
 */
int enterGestureMode(int reload)
{
	int res, ret;

	res = fts_disableInterruptNoSync();
	if (res < OK) {
		logError(1, "%s enterGestureMode: ERROR %08X \n", tag,
			 res | ERROR_DISABLE_INTER);
		return res | ERROR_DISABLE_INTER;
	}

	if (reload == 1 || refreshGestureMask == 1) {

		res = enableGesture(NULL, 0);
		if (res < OK) {
			logError(1,
				 "%s enterGestureMode: enableGesture ERROR %08X \n",
				 tag, res);
			goto END;
		}

		refreshGestureMask = 0;
	}

	res = setScanMode(SCAN_MODE_LOW_POWER, 0);
	if (res < OK) {
		logError(1,
			 "%s enterGestureMode: enter gesture mode ERROR %08X \n",
			 tag, res);
		goto END;
	}

	res = OK;
END:
	ret = fts_enableInterrupt();
	if (ret < OK) {
		logError(1,
			 "%s enterGestureMode: fts_enableInterrupt ERROR %08X \n",
			 tag, res | ERROR_ENABLE_INTER);
		res |= ret | ERROR_ENABLE_INTER;
	}

	return res;
}

/**
 * Check if one or more Gesture IDs are currently enabled in gesture_mask
 * @return FEAT_ENABLE if one or more gesture ids are enabled, FEAT_DISABLE if all the gesture ids are currently disabled
 */
int isAnyGestureActive(void)
{
	int res = 0;

	while (res < (GESTURE_MASK_SIZE - 1) && gesture_mask[res] == 0) {
		res++;
	}

	if (gesture_mask[res] != 0) {
		logError(0,
			 "%s %s: Active Gestures Found! gesture_mask[%d] = %02X !\n",
			 tag, __func__, res, gesture_mask[res]);
		return FEAT_ENABLE;
	} else {
		logError(0, "%s %s: All Gestures Disabled!\n", tag, __func__);
		return FEAT_DISABLE;
	}
}

/**
 * Read from the frame buffer the gesture coordinates pairs of the points draw by an user when a gesture is detected
 * @param event pointer to a byte array which contains the gesture event reported by the fw when a gesture is detected
 * @return OK if success or an error code which specify the type of error encountered
 */
int readGestureCoords(u8 *event)
{
	int i = 0;
	u64 address = 0;
	int res;

	u8 val[GESTURE_MAX_COORDS_PAIRS_REPORT * 4];

	if (event[0] == EVT_ID_USER_REPORT && event[1] == EVT_TYPE_USER_GESTURE) {
		address = (event[4] << 8) | event[3];
		gesture_coords_reported = event[5];
		if (gesture_coords_reported > GESTURE_MAX_COORDS_PAIRS_REPORT) {
			logError(1,
				 "%s %s:  FW reported more than %d points for the gestures! Decreasing to %d \n",
				 tag, __func__, gesture_coords_reported,
				 GESTURE_MAX_COORDS_PAIRS_REPORT);
			gesture_coords_reported =
			    GESTURE_MAX_COORDS_PAIRS_REPORT;
		}

		logError(1, "%s %s: Offset: %08X , coords pairs = %d\n", tag,
			 __func__, address, gesture_coords_reported);

		res = fts_writeReadU8UX(FTS_CMD_FRAMEBUFFER_R, BITS_16, address, val, (gesture_coords_reported * 2 * 2), DUMMY_FRAMEBUFFER);
		if (res < OK) {
			logError(1,
				 "%s %s: Cannot read the coordinates! ERROR %08X  \n",
				 tag, __func__, res);
			gesture_coords_reported = ERROR_OP_NOT_ALLOW;
			return res;
		}

		for (i = 0; i < gesture_coords_reported; i++) {
			gesture_coordinates_x[i] =
			    (((u16) val[i * 2 + 1]) & 0x0F) << 8 |
			    (((u16) val[i * 2]) & 0xFF);
			gesture_coordinates_y[i] = (((u16)
						     val[gesture_coords_reported
							 * 2 + i * 2 +
							 1]) & 0x0F) << 8 |
			    (((u16)
			      val[gesture_coords_reported * 2 + i * 2]) & 0xFF);
		}

		logError(1, "%s %s: Reading Gesture Coordinates DONE!  \n", tag,
			 __func__);
		return OK;

	} else {
		logError(1,
			 "%s %s: The event passsed as argument is invalid! ERROR %08X  \n",
			 tag, __func__, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

}

/**
 * Return the coordinates of the points stored during the last detected gesture
 * @param x output parameter which will store the address of the array containing the x coordinates
 * @param y output parameter which will store the address of the array containing the y coordinates
 * @return the number of points (x,y) stored and therefore the size of the x and y array returned.
 */
int getGestureCoords(u16  **x, u16  **y)
{
	*x = gesture_coordinates_x;
	*y = gesture_coordinates_y;
	logError(1,
		 "%s %s: Number of gesture coordinates pairs returned = %d  \n",
		 tag, __func__, gesture_coords_reported);
	return gesture_coords_reported;
}
