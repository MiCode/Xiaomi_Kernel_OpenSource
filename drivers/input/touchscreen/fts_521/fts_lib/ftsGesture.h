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
* \file ftsGesture.h
* \brief Contains all the macro and prototypes to handle the Gesture Detection features
*/

#ifndef  FTS_GESTURE_H_
#define FTS_GESTURE_H_

#include "ftsHardware.h"

#define	GESTURE_MASK_SIZE					4

#define	GESTURE_MAX_COORDS_PAIRS_REPORT		100

int updateGestureMask(u8 *mask, int size, int en);
int disableGesture(u8 *mask, int size);
int enableGesture(u8 *mask, int size);
int enterGestureMode(int reload);
int isAnyGestureActive(void);
int readGestureCoords(u8 *event);
int getGestureCoords(u16  **x, u16  **y);

#endif
