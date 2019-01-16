/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

/*
   @file si_mdt_inputdev.h
*/

#ifndef _SI_MDT_INPUTDEV_H_
#define _SI_MDT_INPUTDEV_H_

#define HID_INPUT_REPORT		7
#define MAX_TOUCH_CONTACTS		4

/* MDT header byte bit definitions */
#define REGISTRATION_SUCCESS	0
#define REGISTRATION_ERROR		1
#define KEY_PRESSED				1
#define KEY_RELEASED			0
#define MDT_TOUCH_INACTIVE		1
#define MDT_TOUCH_ACTIVE		2


/* Common header bit definitions */
#define MDT_HDR_IS_HID			0x80
#define MDT_HDR_IS_PORT_B		0x40
#define MDT_HDR_IS_KEYBOARD		0x20
#define MDT_HDR_IS_NOT_LAST		0x10
#define MDT_HDR_IS_NOT_MOUSE	0x08

/* Keyboard event specific header bit definitions */
#define MDT_HDR_KBD_LEFT_ALT	0x04
#define MDT_HDR_KBD_LEFT_SHIFT	0x02
#define MDT_HDR_KBD_LEFT_CTRL	0x01

/* Mouse event specific header bit definitions */
#define MDT_HDR_MOUSE_BUTTON_3		0x04
#define MDT_HDR_MOUSE_BUTTON_2		0x02
#define MDT_HDR_MOUSE_BUTTON_1		0x01
#define MDT_HDR_MOUSE_BUTTON_MASK	0x07

/* Touch pad event specific header bit definitions */
#define MDT_HDR_TOUCH_IS_TOUCHED		0x01
#define MDT_HDR_TOUCH_CONTACT_ID_MASK	0x06

/* Game controller event specific header bit definitions */
#define MDT_HDR_GAME_BUTTON_3	0x04
#define MDT_HDR_GAME_BUTTON_2	0x02
#define MDT_HDR_GAME_BUTTON_1	0x01

/* MDT hot-plug prefix and event information */
#define MDT_VERSION				1
#define M_CHAR					'M'
#define D_CHAR					'D'
#define T_CHAR					'T'
#define NOTICE_DEV_PLUG			'R'
#define NOTICE_DEV_UNPLUG		'U'
#define RESPONSE_ACK			'A'	
#define RESPONSE_NACK			'N'

/* MDT Touch screen resources and parameters */

#define MDT_TOUCH_X				0
#define MDT_TOUCH_Y				1
#define BYTE_LOW				0
#define BYTE_HIGH				1
#define MDT_TOUCH_X_LOW			BYTE_LOW
#define MDT_TOUCH_X_HIGH		BYTE_HIGH
#define MDT_TOUCH_Y_LOW			BYTE_LOW
#define MDT_TOUCH_Y_HIGH		BYTE_HIGH

/* support 11 bit absolute addressing        */
#define X_CORNER_RIGHT_LOWER		1870	
#define Y_CORNER_RIGHT_LOWER		1870	
#define ICS_BeagleboardxM			1
#define X_MAX						1920
#define Y_MAX						1920
#define SCALE_X_RAW					0
#define SCALE_X_SCREEN				0
#define SCALE_Y_RAW					0
#define SCALE_Y_SCREEN				0
#define X_SHIFT						0
#define Y_SHIFT						0
#define SWAP_LEFTRIGHT				0
#define SWAP_UPDOWN					0
#define SWAP_XY						0
#define SINGLE_TOUCH				1
#define CORNER_BUTTON				1
#define ICS_BAR						0
#define RIGHT_MOUSE_BUTTON_IS_ESC	1
/* requires installation of IDC file */	    
//#define KERNEL_2_6_38_AND_LATER
/* as of JB the IDC file is needed but, doesn't
	guarantee acess to virtual buttons. */
#define JB_421						0
#if (JB_421 == 1)
#define X_BUTTON_BAR_START			0x4F0
#define Y_BUTTON_RECENTAPPS_TOP		0x050
#define Y_BUTTON_RECENTAPPS_BOTTOM	0x165
#define Y_BUTTON_HOME_TOP			0x185
#define Y_BUTTON_HOME_BOTTOM		0x2C0
#define Y_BUTTON_BACK_TOP			0x2E0
#define Y_BUTTON_BACK_BOTTOM		0x3E0
#endif


enum mdt_dev_state_e {
	  INPUT_DISABLED
	, INPUT_WAITING_FOR_REGISTRATION
	, INPUT_ACTIVE
};

enum mdt_dev_types_e {
	  MDT_TYPE_MOUSE
	, MDT_TYPE_KEYBOARD
	, MDT_TYPE_TOUCHSCREEN
#if 0 
	, MDT_TYPE_GAME
#endif	
	, MDT_TYPE_COUNT
};

struct mdt_touch_history_t{
	uint32_t				abs_x;
	uint32_t				abs_y;
	uint8_t					isTouched;
	uint8_t					state;
};

struct mdt_inputdevs {
	uint8_t						keycodes_old[HID_INPUT_REPORT];		/* Prior HID input report */
	uint8_t						keycodes_new[HID_INPUT_REPORT]; 	/* Current HID input report */
	struct input_dev			*dev_keyboard;
	struct input_dev			*dev_mouse;	
	struct input_dev			*dev_touchscreen;	
	uint8_t 					is_dev_registered[MDT_TYPE_COUNT]; /*Instance tracking variable*/
	struct mdt_touch_history_t	prior_touch_events[MAX_TOUCH_CONTACTS];
	unsigned char				prior_touch_button;
	
	#if (RIGHT_MOUSE_BUTTON_IS_ESC == 1)
	unsigned char				prior_right_button;
	#endif
	
	/* ser overrides to allow runtime calibration*/		
	uint32_t					x_max, y_max;
	uint32_t					x_screen, x_raw, x_shift;
	uint32_t					y_screen, y_raw, y_shift;
	uint32_t					swap_xy, swap_updown, swap_leftright;	
};


struct keyboard_event_data {
	uint8_t	first_key[3];
	uint8_t	second_key[3];
};

struct mouse_event_data {
	int8_t	x_displacement;
	int8_t	y_displacement;
	int8_t	z_displacement;
	uint8_t	vendor_specific[2];
	uint8_t	vendor_specific_game_flag;
};

struct touch_pad_event_data {
	uint8_t	x_abs_coordinate[2];
	uint8_t	y_abs_coordinate[2];
	uint8_t	vendor_specific;
	uint8_t	vendor_specific_game_flag;
};

struct gaming_controller {
	int8_t	x_rel_displacement;
	int8_t	y_rel_displacement;
	int8_t	z_rel_displacement;
	int8_t	y2_rel_displacement;
	uint8_t	buttons_ext;
	uint8_t	id_dpad;
};

struct mdt_hotplug_data {
	uint8_t	sub_header_d;
	uint8_t	sub_header_t;
	uint8_t	event_code;
	uint8_t	device_type;	
	uint8_t	mdt_version;
	uint8_t reserved;	
};

struct mdt_packet {
	uint8_t	adopter_id_h;
	uint8_t	adopter_id_l;
	uint8_t	header;
	union {
		struct keyboard_event_data	keyboard;
		struct mouse_event_data		mouse;
		struct touch_pad_event_data	touch_pad;
		struct gaming_controller	game_controller;
		struct mdt_hotplug_data		hotplug;			
		uint8_t						bytes[6];
	} event;
};

struct mhl_dev_context;
extern struct attribute_group mdt_attr_group;
void mdt_toggle_keyboard_keycode(struct mhl_dev_context *dev_context
								,unsigned char keycode);
bool si_mhl_tx_mdt_process_packet(struct mhl_dev_context *dev_context,void *packet);

//int mdt_init(struct mhl_dev_context *dev_context);

void mdt_destroy(struct mhl_dev_context *dev_context);

#endif /* #ifndef _SI_MDT_INPUTDEV_H_ */
