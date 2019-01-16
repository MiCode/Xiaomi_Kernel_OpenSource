/*
 Silicon Image Driver Extension

 Copyright (C) 2012 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/* !file     si_mdt_inputdev.h */
/* !brief    Silicon Image implementation of MDT function. */
/*  */

#ifndef _SI_MDT_INPUTDEV_H_
#define _SI_MDT_INPUTDEV_H_

#include <linux/input.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define ICS_BeagleboardxM

#define MDT_MAX_TOUCH_CONTACTS		4

#define HID_INPUT_REPORT		7
#define KEYBOARD			2
#define MOUSE				1
#define WAITING_FOR_HEADER		0
#define RECEIVING_PAYLOAD		1
#define RECEIVED			4
#define MEDIA_DATA_TUNNEL_SUPPORT

#define MDT_DPAD_UP			0x01
#define MDT_DPAD_RIGHT			0x02
#define MDT_DPAD_DOWN			0x04
#define MDT_DPAD_LEFT			0x08

#define MDT_GAMEXYRZ_X			0
#define MDT_GAMEXYRZ_Y			1
#define MDT_GAMEXYRZ_Z			2
#define MDT_GAMEXYRZ_Rz			3

#define MDT_HID_DPAD_000_DEGREES	0
#define MDT_HID_DPAD_045_DEGREES	1
#define MDT_HID_DPAD_090_DEGREES	2
#define MDT_HID_DPAD_135_DEGREES	3
#define MDT_HID_DPAD_180_DEGREES	4
#define MDT_HID_DPAD_225_DEGREES	5
#define MDT_HID_DPAD_270_DEGREES	6
#define MDT_HID_DPAD_315_DEGREES	7
#define MDT_HID_DPAD_IDLE		8

#define MDT_DPAD_CENTER			0x80
#define MDT_DPAD_ERROR_ALLOWANCE	5	/* Ignore DPAD value variance of 5 from center */
#define MDT_DPAD_NORMALIZE_RANGE_TO_5	24	/* 0x80 / 5 = 25; 24 is close enough */

#define MDT_OTHER_BUTTONS_4		0x08
#define MDT_OTHER_BUTTONS_5		0x10

#define TS_TOUCHED			1	/* additional macros for PhoenixBlade support */
#define KEY_PRESSED			1
#define KEY_RELEASED			0

#define MDT_BUTTON_LEFT			1
#define MDT_BUTTON_RIGHT		2
#define MDT_BUTTON_MIDDLE		4

#define MDT_ERROR			0xFF

#define MDT_TOUCH_X			0
#define MDT_TOUCH_Y			1

#define BYTE_LOW			0
#define BYTE_HIGH			1

#define MDT_TOUCH_X_LOW			BYTE_LOW
#define MDT_TOUCH_X_HIGH		BYTE_HIGH
#define MDT_TOUCH_Y_LOW			BYTE_LOW
#define MDT_TOUCH_Y_HIGH		BYTE_HIGH

#define SUO_TOUCH

#ifdef SUO_TOUCH		/* normalize to match native resolution */

#define X_CORNER_RIGHT_LOWER		1240	/* relative to physical coordinates with 0,0 in top-left */
#define Y_CORNER_RIGHT_LOWER		698	/* relative to physical coordinates with 0,0 in top-left */

#if	defined(ICS_GTi9250)
#define MDT_SUPPORT_WORKAROUND
#define KERNEL_2_6_38_AND_LATER
#define X_MAX				720
#define Y_MAX				1280
#define X_SCALE				0
#define Y_SCALE				0
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define SINGLE_TOUCH			0
#define X_SHIFT				0
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			1
#define SWAP_UPDOWN			0
#define SWAP_XY				1
#define CORNER_BUTTON			0

#elif	defined(ICS_BeagleboardxM)	/* ICS for Beagleboard xM is in tablet mode. */
#define X_MAX				1640	/* value experimentally selected for physical max = 1280 */
#define Y_MAX				720	/* value experimentally selected for physical max =  720 */
#define X_SCALE				0
#define Y_SCALE				0
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define SINGLE_TOUCH			1
#define X_SHIFT				0
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			0
#define SWAP_UPDOWN			0
#define SWAP_XY				0
#define CORNER_BUTTON			1

#elif	defined(FROYO_SGHi997)
#define GINGERBREAD
#define X_MAX				720
#define Y_MAX				1280
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define SINGLE_TOUCH			0
#define X_SHIFT				0
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			1
#define SWAP_UPDOWN			0
#define SWAP_XY				1
#define CORNER_BUTTON			1

#elif defined(ICS_GTi9100)	/* depends on an IDC file with: devices.internal = 0 */
#define KERNEL_2_6_38_AND_LATER	/* touch.deviceType = touchscreen */
#define X_MAX				800	/* touch.orientationAware = 0 */
#define Y_MAX				400
#define SCALE_X_RAW			2100
#define SCALE_X_SCREEN		800	/* scaling factor = SCREEN/RAW */
#define SCALE_Y_RAW			1080
#define SCALE_Y_SCREEN		400
#define SINGLE_TOUCH			0
#define X_SHIFT				0
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			0
#define SWAP_UPDOWN			0
#define SWAP_XY				0
#define CORNER_BUTTON			1


#elif defined(ICS_GTi9100_WORKAROUND)
#define MDT_SUPPORT_WORKAROUND
#define KERNEL_2_6_38_AND_LATER
#define X_MAX				479	/* MDT_KORVUS_X_MAX */
#define Y_MAX				799	/* MDT_KORVUS_Y_MAX */
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define SINGLE_TOUCH			0
#define X_SHIFT				5
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			1
#define SWAP_UPDOWN			0
#define SWAP_XY				1
#define CORNER_BUTTON			1


#elif defined(GB_GTi9100)
#define GINGERBREAD
#define X_MAX				720
#define Y_MAX				1280
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define SINGLE_TOUCH			0
#define X_SHIFT				0
#define Y_SHIFT				0
#define SWAP_LEFTRIGHT			1
#define SWAP_UPDOWN			0
#define SWAP_XY				1
#define CORNER_BUTTON			1

#else				/* AR1100 control */
#define SINGLE_TOUCH			1
#define X_SHIFT				0xFF
#define Y_SHIFT				0xFF
#define SCALE_X_RAW			0
#define SCALE_X_SCREEN		0
#define SCALE_Y_RAW			0
#define SCALE_Y_SCREEN		0
#define X_MAX				0xE00
#define Y_MAX				0xE00
#define SWAP_LEFTRIGHT			1
#define SWAP_UPDOWN			1
#define CORNER_SUPPORT			1
#define RELEASE_SUPPORT			1
#define X_CORNER_LEFT_UPPER		0xE00
#define Y_CORNER_LEFT_UPPER		0x150
#define X_CORNER_LEFT_LOWER		0x2FF
#define Y_CORNER_LEFT_LOWER		0x1C0
#define X_CORNER_RIGHT_UPPER
#define Y_CORNER_RIGHT_UPPER
#define X_CORNER_RIGHT_LOWER		0x100
#define Y_CORNER_RIGHT_LOWER		0xD00
#endif
#endif

#ifdef __KERNEL__
typedef struct input_dev si_input_dev;

#define si_input_report_key(x, y, z)	input_report_key(x, y, z)
#define si_input_report_rel(x, y, z)	input_report_rel(x, y, z)
#define si_input_sync(x)		input_sync(x);
#define si_input_allocate_device()	input_allocate_device()
#define si_set_bit(x, y)			set_bit(x, y)

#define SI_MDT_DEBUG_PRINT(x)
#else
typedef unsigned char si_input_dev;

#include "..\..\platform\api\si_osdebug.h"

#define SI_MDT_DEBUG_PRINT(x)		CBUS_DEBUG_PRINT(x)

#define si_input_report_key(x, y, z)	SI_MDT_DEBUG_PRINT(("MDTkeyboard  event %02x %02x\n", (int)y, (int)z))
#define si_input_report_rel(x, y, z)	SI_MDT_DEBUG_PRINT(("MDTnon-mouse event %02x %02x\n", (int)y, (int)z))
#define si_input_sync(x)		SI_MDT_DEBUG_PRINT(("Submit HID event\n"))
#define si_input_allocate_device()	0
#define si_set_bit(x, y)			SI_MDT_DEBUG_PRINT(("MDTkeyboard config %02x\n", (int)y))

uint8_t *memscan(uint8_t *str_a, uint8_t key, uint8_t length);

#define ENOMEM 0x1
#define EV_REP 0x2
#endif

#define mdt_event_header		\
	uint8_t				isNotLast:1;	\
	uint8_t				isKeyboard:1;	\
	uint8_t				isPortB:1;	\
	uint8_t				isHID:1;

#define mdt_rawevent_header		\
	uint8_t				isRW:1;		\
	uint8_t				isRequest:1;	\
	uint8_t				isPriority:1;	\
	uint8_t				isHID:1;


#define mdt_cursor_suffix		\
	uint8_t				reserved:1;	\
	uint8_t				isGame:1;


struct mdt_touch_bits_t {
	uint8_t isTouched:1;
	uint8_t contactID:2;
};

struct mdt_mouse_XYZ_t {
	signed char x_byteLen;
	signed char y_byteLen;
	signed char z_byteLen;
	uint8_t reserved[3];
};

struct mdt_mouse_XYZ_VS_t {
	uint8_t xyz_byteLen[3];
	struct {
		uint8_t byte_8bit[2];
		uint8_t byte_6bit:6;
		 mdt_cursor_suffix	/* common, 2 bit suffix */
	} vendor_specific;
};

struct mdt_touch_XYZ_t {
	uint8_t xy_wordLen[2][2];
	struct {
		uint8_t byte_8bit;
		uint8_t byte_6bit:6;
		 mdt_cursor_suffix	/* common, 2 bit suffix */
	} vendor_specific;
};

struct mdt_game_XYZRz_t {
	uint8_t xyzRz_byteLen[4];
	uint8_t buttons_ex;
	uint8_t dPad:4;
	uint8_t deviceID:2;
	 mdt_cursor_suffix	/* common, 2 bit suffix */
};

struct mdt_suffix {
	uint8_t other_data[5];
	uint8_t other_data_bits:6;
	 mdt_cursor_suffix	/* common, 2 bit suffix */
};


struct mdt_non_mouse_cursorheader_t {
	uint8_t contactID:2;
	uint8_t isTouched:1;
	uint8_t isNotMouse:1;
	 mdt_event_header	/* common, 4 bit header nibble */
};

struct mdt_mouse_cursorheader_t {
	uint8_t button:3;
	uint8_t isNotMouse:1;
	 mdt_event_header	/* common, 4 bit header nibble */
};


struct mdt_cursor_mouse_t {	/* 4 bytes or 7 bytes in length */
	struct mdt_mouse_cursorheader_t header;	/* use when (!IsNotMouse) */
	union {
		struct mdt_mouse_XYZ_t XYZ;	/* use when (!IsNotLast) */
		struct mdt_mouse_XYZ_VS_t XYZ_VS;	/* use when (!IsNotMouse) && (IsNotLast) */
		unsigned char raw[6];
	} body;
};

struct mdt_cursor_other_t {	/* 4 bytes or 7 bytes in length */
	union {
		struct mdt_non_mouse_cursorheader_t touch;	/* use when (IsNotMouse) */
		struct mdt_mouse_cursorheader_t game;
		unsigned char raw;
	} header;
	union {
		struct mdt_touch_XYZ_t touchXYZ;	/* use when (IsNotMouse) && (!IsGame) */
		struct mdt_game_XYZRz_t gameXYZRz;	/* use wehn (IsNotMouse) && (IsGame) */
		struct mdt_suffix suffix;
		unsigned char raw[6];
	} body;
};

struct mdt_keyboard_header_t {
	uint8_t modifier_keys:3;
	uint8_t reserved:1;	/* set to 0 */
 mdt_event_header};

struct mdt_raw_with_header {
	uint8_t ls_nibble:4;
 mdt_rawevent_header};

struct mdt_keyboard_event_t {	/* 4 bytes or 7 bytes in length */
	struct mdt_keyboard_header_t header;	/* to avoid wasting space, all bit fields must be in the same struct */
	union {
		struct {
			uint8_t keycodes_firstThree[3];
			uint8_t reserved[3];
		} truncated;
		uint8_t keycodes_all[6];
	} body;
};

struct mdt_header {		/* 2012-05-10 - add RAW packet structure */
	uint8_t ls_nibble:4;
	 mdt_event_header uint8_t other_data[13];
};


union mdt_event_t {		/* 4 bytes 7 bytes in length */
	struct mdt_cursor_mouse_t event_mouse;
	struct mdt_cursor_other_t event_cursor;
	struct mdt_keyboard_event_t event_keyboard;
	struct mdt_header header;
	uint8_t bytes[14];
};

struct mdt_burst_01_t {		/* 6, 9, 13, or 16 bytes */
	uint8_t ADOPTER_ID[2];
	union mdt_event_t events[2];
};


/* ------------------------------------------------------------------------------- */
/* INPUT device instance definition */

enum {
	INPUT_DISABLED, INPUT_WAITING_FOR_REGISTRATION, INPUT_ACTIVE
};

enum {
	DEV_TYPE_MOUSE, DEV_TYPE_KEYBOARD, DEV_TYPE_TOUCH, DEV_TYPE_GAME, DEV_TYPE_COUNT
};

enum {
	MDT_TOUCH_INACTIVE, MDT_TOUCH_ACTIVE
};


struct gamepad_history_t {
	int abs_x;
	int abs_y;
	int x_delta;
	int y_delta;
	uint8_t dpad_event;
	uint8_t other_buttons;
};

struct touch_history_t {
	int abs_x;		/* Cached coordinate values for multi-touch HIDs */
	int abs_y;		/* the array is limited to 3 members since MDT packet */
	uint8_t isTouched;	/* structure only has 2 bits to ID the contact */
	uint8_t state;
};

struct si_mdt_inputdevs_t {
	struct device *g_mdt_dev;	/* Debug variables */
	struct class *g_mdt_class;

	struct workqueue_struct *mdt_joystick_wq;
	struct delayed_work repeat_for_gamepad;
	uint8_t prior_mouse_buttons;
	struct gamepad_history_t prior_game_event;
	struct touch_history_t prior_touch_events[MDT_MAX_TOUCH_CONTACTS];
	uint8_t prior_touch_button;
#ifdef PHOENIX_BLADE
	unsigned char touch_debounce_counter;
	enum phoenix_blade_demo_state_e phoenix_blade_state;
	struct mdt_touch_history_t prior_native_touch;
	struct mdt_double_touch_t double_touch;
	struct mdt_simulated_buttons_t simulated;
#endif
	uint8_t is_dev_registered[DEV_TYPE_COUNT];	/* Instance tracking variables */
	uint8_t keycodes_old[HID_INPUT_REPORT];	/* Prior HID input report */
	uint8_t keycodes_new[HID_INPUT_REPORT];	/* Current HID input report */

	/* note: Unsuccesfully tried to use an array of pointers for si_input_dev. This didn't work. */
	si_input_dev * dev_touchscreen;
	si_input_dev *dev_keyboard;	/* Input devices are event generating interfaces in */
	si_input_dev *dev_mouse;	/* the Linux input subsystem. Such devices */
	/* are typically located under /dev/input/<xyz> */
};				/* Linux file system. These devices can be read but, */
									/* cannot be written. When read, the data retrieved */
									/* will reflect event time, source, & value. */

#define MDT_DISCOVERY_SIZE		4
#define MDT_DISCOVERY_DISABLE		1

#define MDT_MIN_PACKET_LENGTH		4
#define MDT_KEYBOARD_PACKET_TAIL_LENGTH	3
#define MDT_KEYBOARD_PACKET_LENGTH	(MDT_MIN_PACKET_LENGTH + MDT_KEYBOARD_PACKET_TAIL_LENGTH)
#define MDT_MAX_PACKET_LENGTH		MDT_KEYBOARD_PACKET_LENGTH

#define MDT_EVENT_HANDLED		1

uint8_t mdt_input_init(void);
void mdt_input_deregister(void);

void mdt_generate_event_keyboard(struct mdt_keyboard_event_t *keyboardPacket);
void mdt_generate_event_mouse(struct mdt_cursor_mouse_t *mousePacket);
void mdt_generate_event_gamepad(struct mdt_cursor_other_t *gamePacket);
unsigned char mdt_generate_event_touchscreen(struct mdt_cursor_other_t *touchPacket,
					     uint8_t submitEvent);

#endif
