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

/**
   @file si_mdt_inputdev.c
*/
#include "sii_hal.h"

#ifdef MEDIA_DATA_TUNNEL_SUPPORT

#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/hrtimer.h>

#include "si_fw_macros.h"
#include "si_mhl_defs.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#include "si_mdt_inputdev.h"
#include "mhl_linux_tx.h"
#include "platform.h"
#ifdef KERNEL_2_6_38_AND_LATER
#include <linux/input/mt.h>
#endif
#include <linux/kernel.h>

/* keycode map from usbkbd.c */
uint8_t usb_kbd_keycode[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	 65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,150,155,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	200,201,207,208,213,215,216,217,226,139,172,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};

// Local, helper functions
static bool is_mdt_dev_active(struct mhl_dev_context *dev_context, enum mdt_dev_types_e dev_type) {
	if (dev_context->mdt_devs.is_dev_registered[dev_type]==INPUT_ACTIVE) 
		return true;
	else 
		return false;
}

static bool is_mdt_dev_waiting(struct mhl_dev_context *dev_context, enum mdt_dev_types_e dev_type) {
	if (dev_context->mdt_devs.is_dev_registered[dev_type]==INPUT_WAITING_FOR_REGISTRATION) 
		return true;
	else 
		return false;
}

static void destroy_mouse(struct mhl_dev_context *dev_context)
{
	if (dev_context->mdt_devs.dev_mouse == NULL) 
		return;

	MHL_TX_DBG_INFO(dev_context, "Unregistering mouse: %p\n",dev_context->mdt_devs.dev_mouse);
	input_unregister_device(dev_context->mdt_devs.dev_mouse);
	MHL_TX_DBG_INFO(dev_context, "Freeing mouse: %p\n",dev_context->mdt_devs.dev_mouse);
	input_free_device(dev_context->mdt_devs.dev_mouse);
	dev_context->mdt_devs.dev_mouse = NULL;	
}

static void destroy_keyboard(struct mhl_dev_context *dev_context)
{
	if (dev_context->mdt_devs.dev_keyboard == NULL)
		return;

	MHL_TX_DBG_INFO(dev_context, "Unregistering keyboard: %p\n",dev_context->mdt_devs.dev_keyboard);
	input_unregister_device(dev_context->mdt_devs.dev_keyboard);
	MHL_TX_DBG_INFO(dev_context, "Freeing keyboard: %p\n",dev_context->mdt_devs.dev_keyboard);
	input_free_device(dev_context->mdt_devs.dev_keyboard);
	dev_context->mdt_devs.dev_keyboard = NULL;
}

static void destroy_touchscreen(struct mhl_dev_context *dev_context)
{
	if (dev_context->mdt_devs.dev_touchscreen == NULL) 
		return;

	MHL_TX_DBG_INFO(dev_context, "Unregistering mouse: %p\n",dev_context->mdt_devs.dev_touchscreen);
	input_unregister_device(dev_context->mdt_devs.dev_touchscreen);
	MHL_TX_DBG_INFO(dev_context, "Freeing mouse: %p\n",dev_context->mdt_devs.dev_touchscreen);
	input_free_device(dev_context->mdt_devs.dev_touchscreen);
	dev_context->mdt_devs.dev_touchscreen 		= NULL;
	memset(dev_context->mdt_devs.prior_touch_events,	0, 
		MAX_TOUCH_CONTACTS *	sizeof(dev_context->mdt_devs.prior_touch_events[0]));
}

int init_mdt_keyboard(struct mhl_dev_context *dev_context)
{
	int i;
	uint8_t error;
	struct input_dev	*dev_keyboard;

	dev_keyboard = input_allocate_device();
	if (!dev_keyboard) {
		MHL_TX_DBG_ERR(dev_context, "Not enough memory\n");
		return -ENOMEM;
	}
	MHL_TX_DBG_INFO(dev_context, "Allocated keyboard: %p\n",dev_keyboard);

	set_bit(EV_KEY, dev_keyboard->evbit);
	set_bit(EV_REP, dev_keyboard->evbit);

	dev_keyboard->phys 		 = "mdt_kbd/input0";
	dev_keyboard->name		 = "MDTkeyboard";
	dev_keyboard->keycode 	 = usb_kbd_keycode;
	dev_keyboard->keycodesize= sizeof(unsigned char);
	dev_keyboard->keycodemax = ARRAY_SIZE(usb_kbd_keycode);

	for (i = 1; i < 256; i++)
		set_bit(usb_kbd_keycode[i], dev_keyboard->keybit);

	dev_keyboard->id.bustype = BUS_VIRTUAL;
	dev_keyboard->id.vendor  = 0x1095;
	dev_keyboard->id.product = MHL_PRODUCT_NUM;

	/* Use version to distinguish between devices */
	dev_keyboard->id.version = 0xA;

	error = input_register_device(dev_keyboard);
	if (error) {
		MHL_TX_DBG_ERR(dev_context, "Failed to register device\n");
		input_free_device(dev_keyboard);
		return error;
	}

	MHL_TX_DBG_INFO(dev_context, "Registered keyboard: %p\n",dev_keyboard);

	dev_context->mdt_devs.dev_keyboard = dev_keyboard;

	return 0;
}

int init_mdt_mouse(struct mhl_dev_context *dev_context)
{
	uint8_t error;
	struct input_dev	*dev_mouse;

	dev_mouse = input_allocate_device();
	if (!dev_mouse) {
		MHL_TX_DBG_ERR(dev_context, "Not enough memory\n");
		return -ENOMEM;
	}
	MHL_TX_DBG_INFO(dev_context, "Allocated mouse: %p\n",dev_mouse);

	set_bit(EV_REL, 	dev_mouse->evbit);
	set_bit(EV_KEY,		dev_mouse->evbit);
	set_bit(BTN_LEFT,	dev_mouse->keybit);
	set_bit(BTN_RIGHT,	dev_mouse->keybit);
	set_bit(BTN_MIDDLE,	dev_mouse->keybit);
	set_bit(BTN_SIDE,	dev_mouse->keybit);
	set_bit(BTN_EXTRA,	dev_mouse->keybit);
	set_bit(REL_X,		dev_mouse->relbit);
	set_bit(REL_Y,		dev_mouse->relbit);
	set_bit(REL_WHEEL,	dev_mouse->relbit);
	#if (RIGHT_MOUSE_BUTTON_IS_ESC == 1) 
	set_bit(KEY_ESC,	dev_mouse->keybit);
	dev_context->mdt_devs.prior_right_button 	= 0;
	#endif
	
	dev_mouse->phys		  = "mdt_mouse/input0";
	dev_mouse->name	      = "MDTmouse";
	dev_mouse->id.bustype = BUS_VIRTUAL;
	dev_mouse->id.vendor  = 0x1095;
	dev_mouse->id.product = MHL_PRODUCT_NUM;

	/* Use version to distinguish between devices */
	dev_mouse->id.version = 0xB;

	error = input_register_device(dev_mouse);
	if (error) {
		MHL_TX_DBG_ERR(dev_context, "Failed to register device\n");
		input_free_device(dev_mouse);
		return error;
	}

	MHL_TX_DBG_INFO(dev_context, "Registered mouse: %p\n",dev_mouse);

	dev_context->mdt_devs.dev_mouse = dev_mouse;

	return 0;
}


int init_mdt_touchscreen(struct mhl_dev_context *dev_context)
{
	uint8_t error;
	struct input_dev	*dev_touchscreen;

	dev_touchscreen = input_allocate_device();
	if (!dev_touchscreen) {
		MHL_TX_DBG_ERR(dev_context, "Not enough memory\n");
		return -ENOMEM;
	}

	MHL_TX_DBG_INFO(dev_context, "Allocated touch screen: %p\n",dev_touchscreen);
	
	#if !defined(SINGLE_TOUCH) && defined(KERNEL_2_6_38_AND_LATER)
	input_mt_init_slots (dev_touchscreen, MAX_TOUCH_CONTACTS);
	#endif
	
	dev_touchscreen->phys	    = "mdt_touch/input0";
	dev_touchscreen->name	    = "MDTtouchscreen";
	dev_touchscreen->id.bustype = BUS_VIRTUAL;
	dev_touchscreen->id.vendor  = 0x1095;
	dev_touchscreen->id.product = MHL_PRODUCT_NUM;
	
	/* use version to distinguish between devices */
	dev_touchscreen->id.version = 0xC;

	
#if defined(SINGLE_TOUCH)
	__set_bit(EV_ABS, 	   dev_touchscreen->evbit);
	__set_bit(ABS_X,	   dev_touchscreen->absbit);
	__set_bit(ABS_Y,	   dev_touchscreen->absbit);
	__set_bit(EV_KEY, 	   dev_touchscreen->evbit);
	#if     (CORNER_BUTTON == 1)
	__set_bit(KEY_ESC,	   dev_touchscreen->keybit);
	#endif
	__set_bit(BTN_TOUCH,	   dev_touchscreen->keybit);
	#ifdef  KERNEL_2_6_38_AND_LATER
	__set_bit(INPUT_PROP_DIRECT, dev_touchscreen->propbit);
	#endif
	input_set_abs_params(	   dev_touchscreen, ABS_X,		0, dev_context->mdt_devs.x_max, 0, 0);	
	input_set_abs_params(	   dev_touchscreen, ABS_Y, 		0, dev_context->mdt_devs.y_max, 0, 0);
#else
	
	__set_bit(EV_ABS,          	dev_touchscreen->evbit);
	__set_bit(EV_KEY, 	  		dev_touchscreen->evbit);
	#ifdef KERNEL_2_6_38_AND_LATER
	__set_bit(EV_SYN, 	    	dev_touchscreen->evbit);
	__set_bit(MT_TOOL_FINGER,   dev_touchscreen->keybit);
	__set_bit(INPUT_PROP_DIRECT,dev_touchscreen->propbit);
	input_mt_init_slots (dev_touchscreen,  MAX_TOUCH_CONTACTS);
	#else
	__set_bit(BTN_TOUCH,	   	dev_touchscreen->keybit);
	input_set_abs_params(	   	dev_touchscreen, ABS_MT_WIDTH_MAJOR, 0, 3,	  0, 0);
	input_set_abs_params(	   	dev_touchscreen, ABS_MT_TRACKING_ID, 0, 3,	  0, 0);
	#endif
	#if (CORNER_BUTTON == 1)
	__set_bit(KEY_ESC,	   dev_touchscreen->keybit);
	#endif
	input_set_abs_params(	   dev_touchscreen, ABS_MT_TOUCH_MAJOR, 0, 30,	  0, 0);
	input_set_abs_params(	   dev_touchscreen, ABS_MT_PRESSURE, 	0, 255,	  0, 0);
	input_set_abs_params(      dev_touchscreen, ABS_MT_POSITION_X,  0, dev_context->mdt_devs.x_max, 0, 0);
	input_set_abs_params(	   dev_touchscreen, ABS_MT_POSITION_Y,  0, dev_context->mdt_devs.y_max, 0, 0);

#endif

#if (JB_421 == 1) && (ICS_BAR == 1)
	#if (Y_BUTTON_RECENTAPPS_TOP != 0)
	__set_bit(KEY_MENU, dev_touchscreen->keybit);
	#endif
	#if (Y_BUTTON_HOME_TOP  	 != 0)
	__set_bit(KEY_HOMEPAGE, dev_touchscreen->keybit);
	#endif
	#if (Y_BUTTON_BACK_TOP 		 != 0)
	__set_bit(KEY_BACK, dev_touchscreen->keybit);
	#endif
#endif


	error = input_register_device(dev_touchscreen);
    if (error) {
		MHL_TX_DBG_ERR(dev_context, "Failed to register device\n");
		input_free_device(dev_touchscreen);
		return error;
	}
	MHL_TX_DBG_INFO(dev_context, "Registered touchscreen: %p\n",dev_touchscreen);
	
	dev_context->mdt_devs.dev_touchscreen = dev_touchscreen;

	/* initialize history; in parcitular initialize state elements with MDT_TOUCH_INACTIVE */
	memset(dev_context->mdt_devs.prior_touch_events, 0, MAX_TOUCH_CONTACTS * sizeof(dev_context->mdt_devs.prior_touch_events[0]));

	return 0; 
}

static int destroy_device(struct mhl_dev_context *dev_context, enum mdt_dev_types_e mdt_device_type)
{
     if ((mdt_device_type >= MDT_TYPE_COUNT) ||
		(is_mdt_dev_active(dev_context, mdt_device_type) == 0)) {
		MHL_TX_DBG_INFO(dev_context, "FAILURE. Invalid disconnect request. mdt_device_type=0x%x\n", mdt_device_type);
		return REGISTRATION_ERROR;
	}

    switch (mdt_device_type) {
		case MDT_TYPE_MOUSE:
			destroy_mouse(dev_context);		
		break;
		case MDT_TYPE_KEYBOARD:
			destroy_keyboard(dev_context);
		break;
		case MDT_TYPE_TOUCHSCREEN:
			destroy_touchscreen(dev_context);
		break;		
		#if 0
		case MDT_TYPE_GAME:							
			if (!cancel_delayed_work( &(dev_context->mdt_devs.repeat_for_gamepad) ))
				flush_workqueue(dev_context->mdt_devs.mdt_joystick_wq);
			
			/* deregister only if not shared */
			if (is_mdt_dev_active(dev_context, MDT_TYPE_MOUSE) == 0)
				destroy_mouse(dev_context);

			if (is_mdt_dev_active(dev_context, MDT_TYPE_KEYBOARD) == 0)
				destroy_keyboard(dev_context);
		break;
		#endif		
		case MDT_TYPE_COUNT:
			/* 
			 * This case is out of range.
			 * Code is included to avoid compiler warning.
			 */
			break;
     }

    dev_context->mdt_devs.is_dev_registered[mdt_device_type] = INPUT_WAITING_FOR_REGISTRATION;	

    MHL_TX_DBG_INFO(dev_context, "SUCCESS. Disconnect event handled for %d device type.\n", mdt_device_type);
	
    return REGISTRATION_SUCCESS;
}

// The recursive piece of the registeration function.
static int registration_helper(struct mhl_dev_context *dev_context, enum mdt_dev_types_e mdt_device_type)
{
	switch (mdt_device_type) {
		case MDT_TYPE_KEYBOARD:
			if (dev_context->mdt_devs.dev_keyboard != 0) 
				return REGISTRATION_SUCCESS;				
			return init_mdt_keyboard(dev_context);
		break;
		case MDT_TYPE_MOUSE:
			if (dev_context->mdt_devs.dev_mouse != 0 ) 			
				return REGISTRATION_SUCCESS;
			if (init_mdt_mouse(dev_context) != REGISTRATION_SUCCESS)
				return REGISTRATION_ERROR;
		
			/* Do not support both a pointer and touch. */
			destroy_device(dev_context, MDT_TYPE_TOUCHSCREEN);
		break;
		case MDT_TYPE_TOUCHSCREEN:
			if (dev_context->mdt_devs.dev_touchscreen != 0) 			
				return REGISTRATION_SUCCESS;
			if (init_mdt_touchscreen(dev_context) != REGISTRATION_SUCCESS) 
				return REGISTRATION_ERROR;
			
			/* Do not support both a pointer and touch. */
			destroy_device(dev_context, MDT_TYPE_MOUSE);
		break;
		#if 0
		case MDT_TYPE_GAME:
			if (registration_helper(MDT_TYPE_KEYBOARD) == REGISTRATION_ERROR) 
				return REGISTRATION_ERROR;
			if (registration_helper(MDT_TYPE_MOUSE) == REGISTRATION_ERROR)	  
				return REGISTRATION_ERROR;
		break;
		#endif
		case MDT_TYPE_COUNT:
			/* 
			 * This case is out of range.
			 * Code is included to avoid compiler warning.
			 */
			break;
    }

    return REGISTRATION_SUCCESS;
}

static int register_device(struct mhl_dev_context *dev_context, enum mdt_dev_types_e mdt_device_type)
{
    uint8_t error = 0;

    if ((mdt_device_type >= MDT_TYPE_COUNT) ||
		(is_mdt_dev_waiting(dev_context, mdt_device_type) == false)) 
		return REGISTRATION_ERROR;

    /* Call recursive part of the function. 
	   Don't update is_dev_registered there. */	
	error = registration_helper(dev_context, mdt_device_type);		

    if (error != REGISTRATION_SUCCESS) {
		dev_context->mdt_devs.is_dev_registered[mdt_device_type] = INPUT_DISABLED;
		MHL_TX_DBG_INFO(dev_context, "SUCCESS. Device type %d registered.\n", mdt_device_type);		
    } else {
		dev_context->mdt_devs.is_dev_registered[mdt_device_type] = INPUT_ACTIVE;
		MHL_TX_DBG_INFO(dev_context, "FAILURE. Device type %d registration failed.\n", mdt_device_type);
	}
	
    return error;
}

void generate_event_keyboard(struct mhl_dev_context *dev_context,
							 struct mdt_packet *keyboard_packet)
{
	struct input_dev	*dev_keyboard	= dev_context->mdt_devs.dev_keyboard;
	uint8_t				*keycodes_new	= dev_context->mdt_devs.keycodes_new;
	uint8_t				*keycodes_old	= dev_context->mdt_devs.keycodes_old;
	int					i;

	//register_device(dev_context, MDT_TYPE_KEYBOARD);

	memcpy(keycodes_new, &keyboard_packet->header, HID_INPUT_REPORT);
	MHL_TX_DBG_INFO(dev_context, "Key (scancode %02X) asserted.\n",
					keycodes_new[1]);

	if (dev_keyboard == 0) {
		MHL_TX_DBG_INFO(dev_context, "MDT_ERR_NOKEY\n");
		return;
	}

	/* following code was copied from usbkbd.c */
	/* generate events for CRL, SHIFT, and ALT keys */
	for (i = 0; i < 3; i++)
		input_report_key(dev_keyboard,
						 usb_kbd_keycode[i + 224],
						 (keycodes_new[0] >> i) & 1);

	/*
	 * Generate key press/release events for the
	 * remaining bytes in the input packet
	 */
	for (i = 1; i < 7; i++) {
		/* If keycode in pervious HID payload doesn't appear
		 * in NEW HID payload, generate de-assertion event
		 */
		if ((keycodes_old[i] > 3) &&
			((uint8_t *)memscan(keycodes_new + 1, keycodes_old[i], 6) ==
			((uint8_t *)(keycodes_new) + 7))) {
			if (usb_kbd_keycode[keycodes_old[i]]) {
				input_report_key(dev_keyboard, usb_kbd_keycode[keycodes_old[i]], 0);
			} else {
				MHL_TX_DBG_INFO(dev_context, "Unknown key (scancode %#x) "
								"released.\n", keycodes_old[i]);
			}
		}

		/* If keycode in NEW HID paylaod doesn't appear in previous
		 * HID payload, generate assertion event
		 */
		if (keycodes_new[i] > 3 &&
			memscan(keycodes_old + 1, keycodes_new[i], 6) == keycodes_old + 7) {
			if (usb_kbd_keycode[keycodes_new[i]]) {
				input_report_key(dev_keyboard, usb_kbd_keycode[keycodes_new[i]], 1);
			} else {
				MHL_TX_DBG_INFO(dev_context, "Unknown key (scancode %#x) "
								"pressed.\n", keycodes_new[i]);
			}
		}
     }

	input_sync(dev_keyboard);

	/* NEW HID payload is now OLD */
	memcpy(keycodes_old, keycodes_new, HID_INPUT_REPORT);
}				

static void mdt_toggle_keycode(struct input_dev *hid_device
								,unsigned char keycode) 
{
    if (NULL == hid_device) 
		return;

    input_report_key(hid_device, keycode, KEY_PRESSED);
    input_sync(hid_device);

    input_report_key(hid_device, keycode, KEY_RELEASED);
    input_sync(hid_device);
}

void mdt_toggle_keyboard_keycode(struct mhl_dev_context *dev_context
								,unsigned char keycode)
{
	mdt_toggle_keycode(dev_context->mdt_devs.dev_keyboard, keycode);
}		

void generate_event_mouse(struct mhl_dev_context *dev_context,
						  struct mdt_packet *mousePacket)
{
	struct input_dev	*dev_mouse = dev_context->mdt_devs.dev_mouse;
	
	//register_device(dev_context, MDT_TYPE_MOUSE);
	
	MHL_TX_DBG_INFO(dev_context, "mouse buttons (0x%02x)\n",
					mousePacket->header & MDT_HDR_MOUSE_BUTTON_MASK);

	if (dev_mouse == 0) {
		MHL_TX_DBG_ERR(dev_context, "MDT_ERR_NOMOUSE\n");
		return;
	}

	/* Translate and report mouse button changes */
	input_report_key(dev_mouse, BTN_LEFT,
					 mousePacket->header & MDT_HDR_MOUSE_BUTTON_1);
	
	#if (RIGHT_MOUSE_BUTTON_IS_ESC == 1)
	if (mousePacket->header & MDT_HDR_MOUSE_BUTTON_2) {
		if (!dev_context->mdt_devs.prior_right_button) {
			dev_context->mdt_devs.prior_right_button = 1;
			mdt_toggle_keycode(dev_mouse, KEY_ESC);
		}
	} else
		dev_context->mdt_devs.prior_right_button = 0;
	#else
	input_report_key(dev_mouse, BTN_RIGHT,
					 mousePacket->header & MDT_HDR_MOUSE_BUTTON_2);
	#endif
	input_report_key(dev_mouse, BTN_MIDDLE,
					 mousePacket->header & MDT_HDR_MOUSE_BUTTON_3);

	input_report_rel(dev_mouse, REL_X,
					 mousePacket->event.mouse.x_displacement);

	input_report_rel(dev_mouse, REL_Y,
					 mousePacket->event.mouse.y_displacement);

	input_report_rel(dev_mouse, REL_WHEEL,
					 mousePacket->event.mouse.z_displacement);

	input_sync(dev_mouse);
}

static uint8_t process_touch_packet(struct mhl_dev_context *dev_context,
									struct mdt_packet *touchPacket)
{
    struct mdt_touch_history_t	*prior_event;
	int							abs_x, abs_y;
	uint8_t						isTouched = (touchPacket->header & 0x01);
    uint8_t						contactID = ((touchPacket->header & 0x06) >> 1);
	
    prior_event	= (struct mdt_touch_history_t *)&(dev_context->mdt_devs.prior_touch_events[contactID]);    

    abs_x  = touchPacket->event.touch_pad.x_abs_coordinate[MDT_TOUCH_X_LOW] |
		(touchPacket->event.touch_pad.x_abs_coordinate[MDT_TOUCH_X_HIGH] << 8);
    abs_y  = touchPacket->event.touch_pad.y_abs_coordinate[MDT_TOUCH_Y_LOW] |
		(touchPacket->event.touch_pad.y_abs_coordinate[MDT_TOUCH_Y_HIGH] << 8);

	#if (CORNER_BUTTON == 1)
    /* Handle LOWER RIGHT corner like a EXIT button (ESC key) */
	if (( abs_x > X_CORNER_RIGHT_LOWER ) && ( abs_y > Y_CORNER_RIGHT_LOWER )) {
		if (isTouched != dev_context->mdt_devs.prior_touch_button)
		{
			dev_context->mdt_devs.prior_touch_button = isTouched;
			if (isTouched)
				mdt_toggle_keycode(dev_context->mdt_devs.dev_touchscreen, KEY_ESC);
		}
		return 0xFF;
    }
	#elif (ICS_BAR == 1)	
	/* JB421 doesn't allow this driver to trigger buttons on the bar.
			implement custom buttons to workaround the problem. */
	if ((isTouched != dev_context->mdt_devs.prior_touch_button)
		&& ( abs_x >= X_BUTTON_BAR_START)) {
		if (( abs_y > Y_BUTTON_RECENTAPPS_TOP) && ( abs_y < Y_BUTTON_RECENTAPPS_BOTTOM))
			mdt_toggle_keycode(dev_context->mdt_devs.dev_touchscreen, KEY_MENU);
		else if (( abs_y > Y_BUTTON_HOME_TOP) && ( abs_y < Y_BUTTON_HOME_BOTTOM))
			mdt_toggle_keycode(dev_context->mdt_devs.dev_touchscreen, KEY_HOMEPAGE);
		else if (( abs_y > Y_BUTTON_BACK_TOP) && ( abs_y < Y_BUTTON_BACK_BOTTOM))
			mdt_toggle_keycode(dev_context->mdt_devs.dev_touchscreen, KEY_BACK);
		return 0xFF;
	}
	#endif
	
	/* support dynamic configuration through ATTRIBUTES */
	if (dev_context->mdt_devs.swap_xy != 0) {
		prior_event->abs_x	= abs_y;
		prior_event->abs_y  = abs_x;
	} else {
		prior_event->abs_x	= abs_x;
		prior_event->abs_y  = abs_y;
	}

	if ((dev_context->mdt_devs.x_raw != 0) &&
		(dev_context->mdt_devs.x_screen != 0) && (prior_event->abs_x != 0)) {
		  prior_event->abs_x *= dev_context->mdt_devs.x_screen;
		  prior_event->abs_x /= dev_context->mdt_devs.x_raw;
	}

	if ((dev_context->mdt_devs.y_raw != 0) && 
		(dev_context->mdt_devs.y_screen != 0) && (prior_event->abs_y != 0)) {
		  prior_event->abs_y *= dev_context->mdt_devs.y_screen;
		  prior_event->abs_y /= dev_context->mdt_devs.y_raw;
	}

	if ((dev_context->mdt_devs.swap_leftright) &&
		(dev_context->mdt_devs.x_max >= prior_event->abs_x))
		prior_event->abs_x = (dev_context->mdt_devs.x_max - prior_event->abs_x);

	if ((dev_context->mdt_devs.swap_updown) && 
		(dev_context->mdt_devs.y_max >= prior_event->abs_y))
		prior_event->abs_y = (dev_context->mdt_devs.y_max - prior_event->abs_y);

	prior_event->abs_x += dev_context->mdt_devs.x_shift;
	prior_event->abs_y += dev_context->mdt_devs.y_shift;

    if (isTouched == 0) {
	    if (prior_event->isTouched == 0) 
	    /* Multiple release events; declare contact inactive & ignore */
		prior_event->state 	= MDT_TOUCH_INACTIVE;
    } else {
	    prior_event->state 	= MDT_TOUCH_ACTIVE;
    }
    prior_event->isTouched	= isTouched;

    return contactID;
}
#if defined(SINGLE_TOUCH)
static void	submit_touchscreen_events_as_single_touch(struct mhl_dev_context *dev_context)
{
	struct input_dev			*dev_ts = dev_context->mdt_devs.dev_touchscreen; 
    struct mdt_touch_history_t 	*prior_event;

    prior_event = &(dev_context->mdt_devs.prior_touch_events[0]);
    input_report_key(dev_ts, BTN_TOUCH, prior_event->isTouched);
    input_report_abs(dev_ts, ABS_X, prior_event->abs_x);
    input_report_abs(dev_ts, ABS_Y, prior_event->abs_y);
}										
#elif defined(KERNEL_2_6_38_AND_LATER)
static void submit_touchscreen_events_with_proto_B(struct mhl_dev_context *dev_context,
											uint8_t contactID)
{
	struct input_dev			*dev_ts = dev_context->mdt_devs.dev_touchscreen; 
	struct mdt_touch_history_t 	*prior_event;
	uint8_t 					i;
    uint8_t						counter = 0;

	for (i=0; i< MAX_TOUCH_CONTACTS; i++) {

		prior_event = &(dev_context->mdt_devs.prior_touch_events[i]);

		if (prior_event->state == MDT_TOUCH_INACTIVE) 
			continue;

		input_mt_slot(dev_ts, i);
		input_mt_report_slot_state(dev_ts, MT_TOOL_FINGER, prior_event->isTouched);

			/* Event already handled; don't handle it again. */
		if (prior_event->isTouched == 0) {
			prior_event->state = MDT_TOUCH_INACTIVE;
		} else {
			counter++;
			input_report_abs(dev_ts, ABS_MT_TOUCH_MAJOR,15);
			input_report_abs(dev_ts, ABS_MT_PRESSURE, 	50);
			input_report_abs(dev_ts, ABS_MT_POSITION_X,	prior_event->abs_x);
			input_report_abs(dev_ts, ABS_MT_POSITION_Y,	prior_event->abs_y);
		}

		/* BTN_TOUCH breaks support brokend as of JB42 */
		#if !defined(JB_421)
		if (counter == 1)
			input_report_key(dev_ts, BTN_TOUCH, 1);
		else
			input_report_key(dev_ts, BTN_TOUCH, 0);
		#endif
}
#else
static void submit_touchscreen_events_with_proto_A(struct mhl_dev_context *dev_context,
											uint8_t contactID)
{
	struct input_dev			*dev_ts = dev_context->mdt_devs.dev_touchscreen; 
    struct mdt_touch_history_t 	*prior_event;
    uint8_t 					i;
    uint8_t 					count = 0;

    for (i=0; i< MAX_TOUCH_CONTACTS; i++) {		
		prior_event = &(dev_context->mdt_devs.prior_touch_events[i]);
	
		if (prior_event->state == MDT_TOUCH_INACTIVE)
			continue;

		count++;

		if (prior_event->isTouched == 0)				//Event handled; don't handle it again.			
			prior_event->state = MDT_TOUCH_INACTIVE;

        input_report_key(dev_ts, BTN_TOUCH, 			prior_event->isTouched);
        input_report_abs(dev_ts, ABS_MT_TOUCH_MAJOR, 	prior_event->isTouched);
		input_report_abs(dev_ts, ABS_MT_TRACKING_ID, 	i);
		input_report_abs(dev_ts, ABS_MT_WIDTH_MAJOR,  	1);
		input_report_abs(dev_ts, ABS_MT_POSITION_X,	prior_event->abs_x);
		input_report_abs(dev_ts,ABS_MT_POSITION_Y,		prior_event->abs_y);
		input_mt_sync(dev_ts);
    }

    if (count == 0)
		input_mt_sync(dev_ts);
}
#endif

static void generate_event_touchscreen(struct mhl_dev_context *dev_context,
										struct mdt_packet *touchPacket)
{    
	struct input_dev			*dev_ts = dev_context->mdt_devs.dev_touchscreen;    
	uint8_t 					contactID; 

    //register_device(dev_context, MDT_TYPE_TOUCHSCREEN);

    if (dev_ts == 0 ) {
		MHL_TX_DBG_ERR(dev_context, "MDT_ERR_NOTOUCHSCREEN\n");
		return;
	}

	/* process touch packet into prior_touch_events */
    contactID = process_touch_packet(dev_context, touchPacket);
    if (contactID == 0xFF)
		return;

	#if defined(SINGLE_TOUCH)
	submit_touchscreen_events_as_single_touch(dev_context);
	#elif defined(KERNEL_2_6_38_AND_LATER)
	submit_touchscreen_events_with_proto_B(dev_context, contactID);
	#else
	submit_touchscreen_events_with_proto_A(dev_context, contactID);			
	#endif		
	/* generate touchscreen assertion */
	input_sync(dev_ts);
}

static bool process_hotplug_packet(struct mhl_dev_context *dev_context, struct mdt_packet *hotplug_packet)
{
	/* 'M' previoulsy found to be in the header byte */
	if ((hotplug_packet->event.hotplug.sub_header_d != D_CHAR) &&
    	(hotplug_packet->event.hotplug.sub_header_t != T_CHAR) &&
    	(hotplug_packet->event.hotplug.mdt_version  != MDT_VERSION))
		return false;

	/* in the future, support response with ACK or NACK */		
	MHL_TX_DBG_INFO(dev_context, "HP packet. Device type: %02x. Event: %02x.\n",
		hotplug_packet->event.hotplug.device_type, hotplug_packet->event.hotplug.event_code );
	switch (hotplug_packet->event.hotplug.event_code) {
		case NOTICE_DEV_PLUG:			
			register_device(dev_context, hotplug_packet->event.hotplug.device_type);
		break;
		case NOTICE_DEV_UNPLUG:
			destroy_device(dev_context, hotplug_packet->event.hotplug.device_type);
		break;
		default:
			return false;
	}
	return true;
}


bool si_mhl_tx_mdt_process_packet(struct mhl_dev_context *dev_context,void *packet)
{
	struct mdt_packet	*mdt_event_packet=(struct mdt_packet *)packet;

	if (!(MDT_HDR_IS_HID & mdt_event_packet->header)) {
		if (M_CHAR == mdt_event_packet->header)
			return process_hotplug_packet(dev_context, mdt_event_packet);
		
		MHL_TX_DBG_INFO(dev_context, "Ignoring non-HID packet\n");
		return false;
	}

	if (MDT_HDR_IS_KEYBOARD & mdt_event_packet->header) {
		generate_event_keyboard(dev_context, mdt_event_packet);

	} else if (!(MDT_HDR_IS_KEYBOARD & mdt_event_packet->header) &&
			(!(MDT_HDR_IS_NOT_MOUSE & mdt_event_packet->header))) {
		generate_event_mouse(dev_context, mdt_event_packet);

	} else if (!(MDT_HDR_IS_KEYBOARD & mdt_event_packet->header) &&
			(MDT_HDR_IS_NOT_MOUSE & mdt_event_packet->header)) {
		generate_event_touchscreen(dev_context, mdt_event_packet);

	} else if (!(MDT_HDR_IS_KEYBOARD & mdt_event_packet->header) &&
			(MDT_HDR_IS_NOT_MOUSE & mdt_event_packet->header) &&
			(MDT_HDR_IS_NOT_LAST & mdt_event_packet->header)) {

			MHL_TX_DBG_INFO(dev_context, "Unsupported gaming controller "\
							"event received\n");
	} else {
		MHL_TX_DBG_INFO(dev_context, "Event is either not an HID event or "\
						"is an an unknown HID event type\n");
		return false;
	}

	/* Consume the write burst event as an MDT event */
	return true;
}

/*
int mdt_init(struct mhl_dev_context *dev_context)
{
	int	status;
    MHL_TX_DBG_INFO(dev_context, "mdt_init!!!!!!!!!!!!!!.\n");

	status = init_mdt_keyboard(dev_context);
	if (status < 0){
		MHL_TX_DBG_INFO(dev_context, "Keyboard registration failure.\n");
		goto exit;
	}
	
	status = init_mdt_mouse(dev_context);
	if (status < 0){
		MHL_TX_DBG_INFO(dev_context, "Mouse registration failure.\n");
		MHL_TX_DBG_INFO(dev_context, "Unregistering keyboard: %p\n",dev_context->mdt_devs.dev_keyboard);
		input_unregister_device(dev_context->mdt_devs.dev_keyboard);
	}	
exit:
	return status;
}
*/


void mdt_destroy(struct mhl_dev_context *dev_context)
{
    MHL_TX_DBG_INFO(dev_context, "mdt_destroy!!!!!!!!!!!!!!.\n");
	destroy_device(dev_context, MDT_TYPE_KEYBOARD);
	destroy_device(dev_context, MDT_TYPE_MOUSE);
	destroy_device(dev_context, MDT_TYPE_TOUCHSCREEN);
}

#endif
