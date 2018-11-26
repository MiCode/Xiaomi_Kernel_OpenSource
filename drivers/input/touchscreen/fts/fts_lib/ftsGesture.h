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

#define	GESTURE_MASK_SIZE		8

#define GESTURE_CUSTOM_POINTS	(30 * 2)
#define GESTURE_CUSTOM_NUMBER	5

#define TEMPLATE_CHUNK			(10 * 2)



#define	GES_ID_DBLTAP			0x01
#define GES_ID_O				0x02
#define GES_ID_C				0x03
#define GES_ID_M				0x04
#define GES_ID_W				0x05
#define GES_ID_E				0x06
#define GES_ID_HFLIP_L2R		0x07
#define GES_ID_HFLIP_R2L		0x08
#define GES_ID_VFLIP_T2D		0x09
#define GES_ID_VFLIP_D2T		0x0A
#define GES_ID_L				0x0B
#define GES_ID_F				0x0C
#define GES_ID_V				0x0D
#define GES_ID_AT				0x0E
#define GES_ID_S				0x0F
#define GES_ID_Z				0x10
#define GES_ID_CUST1			0x11
#define GES_ID_CUST2			0x12
#define GES_ID_CUST3			0x13
#define GES_ID_CUST4			0x14
#define GES_ID_CUST5			0x15
#define GES_ID_LEFTBRACE		0x20
#define GES_ID_RIGHTBRACE		0x21
#define GESTURE_CUSTOM_OFFSET		GES_ID_CUST1

#define GESTURE_ENABLE			0x01
#define GESTURE_DISABLE			0x02
#define GESTURE_ENB_CHECK		0x03
#define GESTURE_START_ADD		0x10
#define GESTURE_DATA_ADD		0x11
#define	GESTURE_FINISH_ADD		0x12
#define GETURE_REMOVE_CUSTOM		0x13
#define GESTURE_CHECK_CUSTOM		0x14


#define EVENT_TYPE_ENB			0x04
#define EVENT_TYPE_CHECK_ENB		0x03
#define	EVENT_TYPE_GESTURE_DTC1		0x01
#define	EVENT_TYPE_GESTURE_DTC2		0x02


int disableGesture(u8 *mask, int size);
int enableGesture(u8 *mask, int size);
int startAddCustomGesture(u8 gestureID);
int finishAddCustomGesture(u8 gestureID);
int loadCustomGesture(u8 *template, u8 gestureID);
int reloadCustomGesture(void);
int enterGestureMode(int reload);
int updateGestureMask(u8 *mask, int size, int en);
