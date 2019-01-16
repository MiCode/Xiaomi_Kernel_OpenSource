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
/* !file     si_mdt_inputdev.c */
/* !brief    Silicon Image implementation of MDT function. */
/*  */

#ifdef __KERNEL__		/* these includes must precede others */
#include <linux/input.h>
#ifdef KERNEL_2_6_38_AND_LATER
#include <linux/input/mt.h>
#endif
#else
#include "si_c99support.h"
#endif

#include "si_drv_mdt_tx.h"
#include "si_mdt_inputdev.h"

#ifdef MDT_SUPPORT

#ifndef __KERNEL__		/* 2012-05-11 - remove headers to support driver compilation with Linux kernel */
#include "si_mhl_tx_api.h"	/* Are these necessary ? */
#include "si_mhl_defs.h"	/* This include must preceede si_mhl_tx.h */
#include "si_mhl_tx.h"		/*  */
#include "string.h"		/* String.h location varies depending on enviornment. */
#else
#include <linux/string.h>
#endif

										/* This is a workaround for Android ICS 4.0.4. */
#ifdef	MDT_SUPPORT_WORKAROUND	/* The workaround may be necessary for earlier ICS and JB. */
struct input_dev *get_native_touchscreen_dev(void);	/* In ICS 4.0.4, a MT input device created in this driver */
void release_native_touchscreen_dev(void);	/* will incorrectly generate uevents. */
#endif				/* Workaround with input_dev from native touch screen driver. */

#ifdef MEDIA_DATA_TUNNEL_SUPPORT

#ifdef __KERNEL__
uint8_t g_dpad_keys[4] = {
#else
code const uint8_t g_dpad_keys[4] = {	/* 8051 memory / code space allows */
#endif
	KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT
};

#ifdef __KERNEL__
uint8_t usb_kbd_keycode[256] = {
#else
code const uint8_t usb_kbd_keycode[256] = {	/* 8051 memory / code space allows */
#endif				/* an array to live in code and 8051 doens't call Linux input */
	/* keycode map from usbmouse.c */
	0, 0, 0, 0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
	50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 2, 3,
	4, 5, 6, 7, 8, 9, 10, 11, 28, 1, 14, 15, 57, 12, 13, 26,
	27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 87, 88, 99, 70, 119, 110, 102, 104, 111, 107, 109, 106,
	105, 108, 103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	72, 73, 82, 83, 86, 127, 116, 117, 183, 184, 185, 186, 187, 188, 189, 190,
	191, 192, 193, 194, 134, 138, 130, 132, 128, 129, 131, 137, 133, 135, 136, 113,
	115, 114, 0, 0, 0, 121, 0, 89, 93, 124, 92, 94, 95, 0, 0, 0,
	122, 123, 90, 91, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	200, 201, 207, 208, 213, 215, 216, 217, 226, 139, 172, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	29, 42, 56, 125, 97, 54, 100, 126, 164, 166, 165, 163, 161, 115, 114, 113,
	150, 158, 159, 128, 136, 177, 178, 176, 142, 152, 173, 140
};

struct si_mdt_inputdevs_t mdt;	/* 20 bytes in 8051 firmware */

static uint8_t init_keyboard(void);
static uint8_t init_mouse(void);
static uint8_t init_touchscreen(void);
static uint8_t is_mdt_dev_waiting(uint8_t dev_type);
static uint8_t is_mdt_dev_disabled(uint8_t dev_type);
static uint8_t do_touchscreen_work(struct mdt_cursor_other_t *touchPacket);
static void mouse_deregister(unsigned char isReset);
static void touch_deregister(unsigned char isReset);
#if (SINGLE_TOUCH == 1)
static void submit_touchscreen_events_with_as_single_touch(void);
#elif defined(GINGERBREAD)
static void submit_touchscreen_events_with_protocol_A(uint8_t contactID);
#elif defined(KERNEL_2_6_38_AND_LATER)
static void submit_touchscreen_events_with_protocol_B(uint8_t contactID);
#endif
#ifndef __KERNEL__
static uint8_t *memscan(uint8_t *str_a, uint8_t key, uint8_t length)	/* 11 bytes in 8051 firmware */
{

	uint8_t *tail = str_a + length;
	uint8_t *pStr = str_a;

	while (pStr != tail) {
		if (*pStr == key)
			return pStr;
		pStr++;
	}

	return 0;
}
#endif

static uint8_t is_keyboard_ready(void)
{
	si_input_dev *dev_keyboard = mdt.dev_keyboard;

	if (is_mdt_dev_disabled(DEV_TYPE_KEYBOARD))
		return 0;

	if (is_mdt_dev_waiting(DEV_TYPE_KEYBOARD)) {
		if (init_keyboard()) {
			mdt.is_dev_registered[DEV_TYPE_KEYBOARD] = INPUT_DISABLED;
			return 0;
		} else
			mdt.is_dev_registered[DEV_TYPE_KEYBOARD] = INPUT_ACTIVE;

	}

	if (dev_keyboard == 0) {
		SI_MDT_DEBUG_PRINT("MDT_ERR_NOKEY\n");
		return 0;
	}

	return 1;
}

static uint8_t is_mouse_ready(void)
{
	si_input_dev *dev_mouse = mdt.dev_mouse;

	if (is_mdt_dev_disabled(DEV_TYPE_MOUSE))
		return 0;

	if (is_mdt_dev_waiting(DEV_TYPE_MOUSE)) {
		touch_deregister(1);
		if (init_mouse()) {
			mdt.is_dev_registered[DEV_TYPE_MOUSE] = INPUT_DISABLED;
			return 0;
		} else
			mdt.is_dev_registered[DEV_TYPE_MOUSE] = INPUT_ACTIVE;

	}

	if (dev_mouse == 0) {
		SI_MDT_DEBUG_PRINT(("MDT_ERR_NOMOUSE\n"));
		return 0;
	}

	return 1;

}

static uint8_t is_touch_ready(void)
{
	si_input_dev *dev_touchscreen = mdt.dev_touchscreen;

	if (is_mdt_dev_disabled(DEV_TYPE_TOUCH))
		return 0;

	if (is_mdt_dev_waiting(DEV_TYPE_TOUCH)) {
		mouse_deregister(1);
		if (init_touchscreen()) {
			mdt.is_dev_registered[DEV_TYPE_TOUCH] = INPUT_DISABLED;
			return 0;
		} else
			mdt.is_dev_registered[DEV_TYPE_TOUCH] = INPUT_ACTIVE;

	}

	if (dev_touchscreen == 0) {
		SI_MDT_DEBUG_PRINT(("MDT_ERR_NOTOUCHSCREEN\n"));
		return 0;
	}

	return 1;

}



static void input_event_mouse(uint8_t buttons, int x, int y, int z)
{
	si_input_dev *dev_mouse = mdt.dev_mouse;

	if (is_mouse_ready() == 0)
		return;
/*
    printk( "MDT mouse event: %x %x %x %x\n",
		x,
		y,
		z,
		buttons);
*/
	if (buttons != mdt.prior_mouse_buttons) {
		si_input_report_key(dev_mouse, BTN_LEFT, buttons & MDT_BUTTON_LEFT);	/* decode data read in to determine */
		si_input_report_key(dev_mouse, BTN_RIGHT, buttons & MDT_BUTTON_RIGHT);	/* which buttons where asserted */
		si_input_report_key(dev_mouse, BTN_MIDDLE, buttons & MDT_BUTTON_MIDDLE);
		mdt.prior_mouse_buttons = buttons;
	}

	if (x)
		si_input_report_rel(dev_mouse, REL_X, x);	/* convereted to a signed type for further */
	if (y)
		si_input_report_rel(dev_mouse, REL_Y, y);
	if (z)
		si_input_report_rel(dev_mouse, REL_WHEEL, z);
	si_input_sync(dev_mouse);	/* generate event */

}

/* This function generates keyboard events */
static void repeat_for_gamepad_func(struct work_struct *p)
{

#define MDT_DPAD_CENTER			0x80
#define MDT_DPAD_ERROR_ALLOWANCE	5
	if ((mdt.prior_game_event.abs_x < (MDT_DPAD_CENTER - MDT_DPAD_ERROR_ALLOWANCE)) ||
	    (mdt.prior_game_event.abs_x > (MDT_DPAD_CENTER + MDT_DPAD_ERROR_ALLOWANCE)) ||
	    (mdt.prior_game_event.abs_y < (MDT_DPAD_CENTER - MDT_DPAD_ERROR_ALLOWANCE)) ||
	    (mdt.prior_game_event.abs_y > (MDT_DPAD_CENTER + MDT_DPAD_ERROR_ALLOWANCE))) {
		input_event_mouse(mdt.prior_mouse_buttons, mdt.prior_game_event.x_delta,
				  mdt.prior_game_event.y_delta, 0);
		queue_delayed_work(mdt.mdt_joystick_wq, &(mdt.repeat_for_gamepad),
				   msecs_to_jiffies(10));
	} else {
		mdt.prior_game_event.x_delta = 0;
		mdt.prior_game_event.y_delta = 0;
	}
}

/* This function can be called from 3rd party drivers to */
/* generate affect a keyboard event. */
static void toggle_keyboard_keycode(unsigned char keycode)
{
	if (is_keyboard_ready() == 0)
		return;

	input_report_key(mdt.dev_keyboard, keycode, KEY_PRESSED);
	input_sync(mdt.dev_keyboard);

	input_report_key(mdt.dev_keyboard, keycode, KEY_RELEASED);
	input_sync(mdt.dev_keyboard);
}


/* This function generates game controller events using keyboard and mouse devices */
void mdt_generate_event_gamepad(struct mdt_cursor_other_t *gamePacket)
{
	struct mdt_game_XYZRz_t *gamePayload = &(gamePacket->body.gameXYZRz);
	int deltaX = 0;
	int deltaY = 0;
	/* int               deltaZ          = 0; */
	/* int               deltaRz         = 0; */
	uint8_t mouse_buttons = gamePacket->header.game.button;
	uint8_t dpad_key_bits = 0;
	uint8_t i;
	uint8_t other_buttons = gamePayload->buttons_ex;
	uint8_t dpad_event = gamePacket->body.suffix.other_data_bits;
	uint8_t X = gamePayload->xyzRz_byteLen[MDT_GAMEXYRZ_X];	/* Retrieve absolute X */
	uint8_t Y = gamePayload->xyzRz_byteLen[MDT_GAMEXYRZ_Y];	/* Retrieve absolute Y */

	printk(KERN_ERR "MDT gamepad: %x %x %x %x %x\n",
	       X, Y, dpad_event, other_buttons, mouse_buttons);


	/* First handle the case in which the directional pad is not asserted. */
	if ((mdt.prior_game_event.dpad_event == MDT_HID_DPAD_IDLE)
	    && (dpad_event == MDT_HID_DPAD_IDLE)) {

		deltaX = ((X - MDT_DPAD_CENTER) / MDT_DPAD_NORMALIZE_RANGE_TO_5);
		deltaY = ((Y - MDT_DPAD_CENTER) / MDT_DPAD_NORMALIZE_RANGE_TO_5);

		if ((mdt.prior_game_event.abs_x != X) || (mdt.prior_game_event.abs_y != Y)) {
			if (!cancel_delayed_work(&(mdt.repeat_for_gamepad)))
				flush_workqueue(mdt.mdt_joystick_wq);

			if (mdt.prior_game_event.x_delta != 0)
				deltaX -= mdt.prior_game_event.x_delta;
			if (mdt.prior_game_event.y_delta != 0)
				deltaY -= mdt.prior_game_event.y_delta;

			mdt.prior_game_event.abs_x = X;
			mdt.prior_game_event.abs_y = Y;
			mdt.prior_game_event.x_delta = deltaX;
			mdt.prior_game_event.y_delta = deltaY;

			queue_delayed_work(mdt.mdt_joystick_wq, &(mdt.repeat_for_gamepad),
					   msecs_to_jiffies(10));
		}

		input_event_mouse(mouse_buttons, deltaX, deltaY, 0);

		/* Place holder. */
		/* Retrieve relative Z  offset for scroll wheel */
		/* Retrieve relative Rz offset */

	} else {

		if (is_keyboard_ready() == 0)
			return;

		switch (dpad_event) {
		case MDT_HID_DPAD_000_DEGREES:	/* 0 degrees */
			dpad_key_bits = MDT_DPAD_UP;
			break;
		case MDT_HID_DPAD_045_DEGREES:	/* 45 degrees */
			dpad_key_bits = (MDT_DPAD_UP | MDT_DPAD_RIGHT);
			break;
		case MDT_HID_DPAD_090_DEGREES:	/* 90 degrees */
			dpad_key_bits = MDT_DPAD_RIGHT;
			break;
		case MDT_HID_DPAD_135_DEGREES:	/* 135 degress */
			dpad_key_bits = (MDT_DPAD_RIGHT | MDT_DPAD_DOWN);
			break;
		case MDT_HID_DPAD_180_DEGREES:	/* 180 degress */
			dpad_key_bits = MDT_DPAD_DOWN;
			break;
		case MDT_HID_DPAD_225_DEGREES:	/* 225 degrees */
			dpad_key_bits = MDT_DPAD_DOWN | MDT_DPAD_LEFT;
			break;
		case MDT_HID_DPAD_270_DEGREES:	/* 270 degress */
			dpad_key_bits = MDT_DPAD_LEFT;
			break;
		case MDT_HID_DPAD_315_DEGREES:	/* 315 degress */
			dpad_key_bits = MDT_DPAD_LEFT | MDT_DPAD_UP;
			break;
		case 8:
			dpad_key_bits = 0;
		}
		for (i = 0; i < 4; i++)	/* generate up, down, left, right arrow key events */
			input_report_key(mdt.dev_keyboard, g_dpad_keys[i], (dpad_key_bits & (1 << i)));	/* HID byte */

		input_sync(mdt.dev_keyboard);
	}

	if (other_buttons != mdt.prior_game_event.other_buttons) {
		if (other_buttons & MDT_OTHER_BUTTONS_4)
			toggle_keyboard_keycode(KEY_ENTER);
		if (other_buttons & MDT_OTHER_BUTTONS_5)
			toggle_keyboard_keycode(KEY_ESC);
	}

	input_event_mouse(mouse_buttons, deltaX, deltaY, 0);

	mdt.prior_game_event.dpad_event = dpad_event;
	mdt.prior_game_event.other_buttons = other_buttons;
}

unsigned char mdt_generate_event_touchscreen(struct mdt_cursor_other_t *touchPacket,
					     uint8_t submitEvent)
{
	uint8_t contactID;

	if (is_touch_ready() == 0)
		return 0xFF;

	if (!cancel_delayed_work(&(mdt.repeat_for_gamepad)))	/* Abort any pending attempts by a game controller to move the cursor. */
		flush_workqueue(mdt.mdt_joystick_wq);

	contactID = do_touchscreen_work(touchPacket);

	if (contactID == 0xFF)
		return 0xFF;

	if (submitEvent) {
#if	(SINGLE_TOUCH == 1)
		submit_touchscreen_events_with_as_single_touch();
#elif defined(GINGERBREAD)
		submit_touchscreen_events_with_protocol_A(contactID);
#elif	defined(KERNEL_2_6_38_AND_LATER)
		submit_touchscreen_events_with_protocol_B(contactID);
#else
		return 0;
#endif
		input_sync(mdt.dev_touchscreen);	/* Generate touchscreen assertion */
	}
	return 0;
}

#ifdef PHOENIX_BLADE
static unsigned char doubletouch_triggered_mouse_button_simulation(unsigned char isTouched)
{

	si_input_dev *dev_mouse = mdt.dev_mouse;

	/* Double touch support */
	/* Update double touch state machine */
	switch (mdt.double_touch.state) {
	case MDT_PB_WAIT_FOR_TOUCH_RELEASE:
		if (isTouched == 1)
			break;	/* wait for touch relesae */
		mdt.double_touch.last_release = jiffies_to_msecs(jiffies);
		mdt.double_touch.state = MDT_PB_WAIT_FOR_1ST_TOUCH;
		break;
	case MDT_PB_WAIT_FOR_1ST_TOUCH:
		if ((isTouched == 0) ||	/* wait for 1st touch in a double touch */
		    (mdt.touch_debounce_counter != 1))
			break;	/* debounce; only transition once */
		mdt.double_touch.last_touch = jiffies_to_msecs(jiffies);
		mdt.double_touch.state = MDT_PB_WAIT_FOR_1ST_TOUCH_RELEASE;
		break;
	case MDT_PB_WAIT_FOR_1ST_TOUCH_RELEASE:
		if (isTouched == 1)
			break;	/* wait for 1st touch release */
		mdt.double_touch.duration_touch =
		    (jiffies_to_msecs(jiffies) - mdt.double_touch.last_release);
		mdt.double_touch.last_release = jiffies_to_msecs(jiffies);
		if (mdt.double_touch.duration_touch < ONE_SECOND)	/* only recognize short contacts */
			mdt.double_touch.state = MDT_PB_WAIT_FOR_2ND_TOUCH;
		else		/* touch was too long; start over */
			mdt.double_touch.state = MDT_PB_WAIT_FOR_1ST_TOUCH;
		break;
	case MDT_PB_WAIT_FOR_2ND_TOUCH:
		if ((isTouched == 0) ||	/* wait for 2nd touch of a double touch */
		    (mdt.touch_debounce_counter != 1))
			break;	/* debounce; only transition once */
		mdt.double_touch.duration_release =
		    (jiffies_to_msecs(jiffies) - mdt.double_touch.last_touch);
		mdt.double_touch.last_touch = jiffies_to_msecs(jiffies);
		if (mdt.double_touch.duration_release < ONE_SECOND)	/* only recognize short intervals between contacts */
			mdt.double_touch.state = MDT_PB_WAIT_FOR_2ND_TOUCH_RELEASE;
		else		/* release was too long; start over */
			mdt.double_touch.state = MDT_PB_WAIT_FOR_TOUCH_RELEASE;
		break;
	case MDT_PB_WAIT_FOR_2ND_TOUCH_RELEASE:	/* wait for final release in a double touch */
		if (isTouched == 1)
			break;	/* wait for 2nd touch release */
		mdt.double_touch.duration_touch =
		    (jiffies_to_msecs(jiffies) - mdt.double_touch.last_release);
		mdt.double_touch.last_release = jiffies_to_msecs(jiffies);
		if (mdt.double_touch.duration_touch < ONE_SECOND) {	/* only recognize short contacts */
			si_input_report_key(dev_mouse, BTN_LEFT, 1);	/* generate contact */
			si_input_report_key(dev_mouse, BTN_LEFT, 0);
			si_input_sync(dev_mouse);
			break;
		}
		/* start over. wait for next touch */
		mdt.double_touch.state = MDT_PB_WAIT_FOR_1ST_TOUCH;
	}

	return 1;
}

unsigned char mdt_generate_event_mouse_from_nativetouch(struct mdt_touch_history_t *touch_event)
{
	int deltaX = 0;
	int deltaY = 0;
	unsigned char mouse_buttons = 0;

	/* mouse pointer simulation is disabled */
	if (mdt.phoenix_blade_state == MDT_PB_NATIVE_TOUCH_SCREEN)
		return 0xFF;

	/* if contact made per isTouched, convert touch to mouse */
	if (touch_event->isTouched == 0)
		mdt.touch_debounce_counter = 0;	/* reset counter when released */
	else {
		if (mdt.touch_debounce_counter < 5) {	/* debounce; ignore the first few touches */
			mdt.touch_debounce_counter++;
		} else {	/* only move if this is a subsequent touch */


			deltaX = (int)(touch_event->abs_x - mdt.prior_native_touch.abs_x);
			deltaY = (int)(touch_event->abs_y - mdt.prior_native_touch.abs_y);


			/* printk(KERN_ERR "mdt x: %d %d %x %x y: %d %x %x", */
			/* (int)(touch_event->abs_x - mdt.prior_native_touch.abs_x), */
			/* (int)deltaX, */
			/* touch_event->abs_x, */

			/* mdt.prior_native_touch.abs_x, */
			/* (int)(touch_event->abs_y - mdt.prior_native_touch.abs_y), */
			/* deltaY, */
			/* touch_event->abs_y, */
			/* mdt.prior_native_touch.abs_y); */


			/* reflect cached button state for pre-ICS phones */
			if (mdt.simulated.left != 0)
				mouse_buttons |= MDT_BUTTON_LEFT;
			if (mdt.simulated.middle != 0)
				mouse_buttons |= MDT_BUTTON_MIDDLE;
			if (mdt.simulated.right != 0)
				mouse_buttons |= MDT_BUTTON_RIGHT;

#ifdef PHOENIX_BLADE_V1
			input_event_mouse(mouse_buttons, deltaX, deltaY, 0);
#else				/* support every screen orientation */
			switch (mdt.phoenix_blade_state) {
			case MDT_PB_SIMULATED_MOUSE_0:
				deltaX *= -1;
				deltaY *= -1;
			case MDT_PB_SIMULATED_MOUSE_90:
				input_event_mouse(mouse_buttons, deltaX, deltaY, 0);
				break;
			case MDT_PB_SIMULATED_MOUSE_180:
				deltaX *= -1;
				input_event_mouse(mouse_buttons, deltaX, deltaY, 0);
				break;
			case MDT_PB_SIMULATED_MOUSE_270:
				deltaY *= -1;
				input_event_mouse(mouse_buttons, deltaX, deltaY, 0);
				break;
			case MDT_PB_NATIVE_TOUCH_SCREEN:	/* do nothing for native touch screen mode */
				break;
			}
#endif
		}

		mdt.prior_native_touch.abs_x = touch_event->abs_x;
		mdt.prior_native_touch.abs_y = touch_event->abs_y;
	}

	return doubletouch_triggered_mouse_button_simulation(touch_event->isTouched);
}
#endif

static uint8_t do_touchscreen_work(struct mdt_cursor_other_t *touchPacket)
{
	int abs_x = 0, abs_y = 0;
	struct touch_history_t *prior_event;
	uint8_t contactID = touchPacket->header.touch.contactID;

/* printk(KERN_ERR "MDT touch: %x %x %x %x %x %x %x\n", */
/* touchPacket->header.raw, */
/* touchPacket->body.raw[0], */
/* touchPacket->body.raw[1], */
/* touchPacket->body.raw[2], */
/* touchPacket->body.raw[3], */
/* touchPacket->body.raw[4], */
/* touchPacket->body.raw[5]); */


#if (SINGLE_TOUCH == 1)
	contactID = 0;
#else
	if (contactID == 0) {	/* error condition */
		printk(KERN_ERR "MDTerror unexpected touch packet: %x %x %x %x %x %x %x\n",
		       touchPacket->header.raw,
		       touchPacket->body.raw[0],
		       touchPacket->body.raw[1],
		       touchPacket->body.raw[2],
		       touchPacket->body.raw[3],
		       touchPacket->body.raw[4], touchPacket->body.raw[5]);

		return 0xFF;
	}
	contactID--;
#endif
	prior_event = (struct touch_history_t *)&(mdt.prior_touch_events[contactID]);

	abs_x = touchPacket->body.touchXYZ.xy_wordLen[MDT_TOUCH_X][MDT_TOUCH_X_LOW] |
	    (touchPacket->body.touchXYZ.xy_wordLen[MDT_TOUCH_X][MDT_TOUCH_X_HIGH] << 8);
	abs_y = touchPacket->body.touchXYZ.xy_wordLen[MDT_TOUCH_Y][MDT_TOUCH_Y_LOW] |
	    (touchPacket->body.touchXYZ.xy_wordLen[MDT_TOUCH_Y][MDT_TOUCH_Y_HIGH] << 8);

/* printk(KERN_ERR "MDT x y : %x %x\n", */
/* abs_x, */
/* abs_y); */


#if (CORNER_BUTTON == 1)
	if ((abs_x > X_CORNER_RIGHT_LOWER) && (abs_y > Y_CORNER_RIGHT_LOWER)) {	/* Handle LOWER RIGHT corner like a EXIT button (ESC key) */
		if (touchPacket->header.touch.isTouched != mdt.prior_touch_button) {
			mdt.prior_touch_button = touchPacket->header.touch.isTouched;
			if (touchPacket->header.touch.isTouched) {
#if defined(MDT_SUPPORT_WORKAROUND)
				if (is_keyboard_ready()) {
					input_report_key(mdt.dev_keyboard, KEY_ESC, 1);
					input_report_key(mdt.dev_keyboard, KEY_ESC, 0);
					input_sync(mdt.dev_keyboard);
				}
#else
				input_report_key(mdt.dev_touchscreen, KEY_ESC, 1);
				input_report_key(mdt.dev_touchscreen, KEY_ESC, 0);
				input_sync(mdt.dev_touchscreen);
#endif
			}
		}
	}
#endif

#if (SWAP_XY == 0)
	/* remember retrieved data for future multi touch use */
	prior_event->abs_x = abs_x;
	prior_event->abs_y = abs_y;
#else
	prior_event->abs_x = abs_y;
	prior_event->abs_y = abs_x;
#endif

#if (SCALE_X_RAW != 0) && (SCALE_X_SCREEN != 0)
	prior_event->abs_x *= SCALE_X_SCREEN;
	prior_event->abs_x /= SCALE_X_RAW;
#endif

#if (SCALE_Y_RAW != 0) && (SCALE_Y_SCREEN != 0)
	prior_event->abs_y *= SCALE_Y_SCREEN;
	prior_event->abs_y /= SCALE_Y_RAW;
#endif

#if (SWAP_LEFTRIGHT == 1)
	prior_event->abs_x = (int)(X_MAX) - prior_event->abs_x;
#endif

#if (SWAP_UPDOWN == 1)
	prior_event->abs_y = (int)(Y_MAX) - prior_event->abs_y;
#endif

	prior_event->abs_x += X_SHIFT;
	prior_event->abs_y += Y_SHIFT;


/* printk(KERN_ERR "MDT touch: %x %x %x %x\n", */
/* touchPacket->header.touch.isTouched, */
/* prior_event->abs_x, */
/* prior_event->abs_y, */
/* prior_event->state); */


	if (touchPacket->header.touch.isTouched == 0) {
		if (prior_event->isTouched == 0)
			prior_event->state = MDT_TOUCH_INACTIVE;	/* Multiple release events; declare contact inactive & ignore */
	} else {
		prior_event->state = MDT_TOUCH_ACTIVE;
	}
	prior_event->isTouched = touchPacket->header.touch.isTouched;
	return contactID;
}

#if (SINGLE_TOUCH == 1)
static void submit_touchscreen_events_with_as_single_touch(void)
{
	struct touch_history_t *prior_event;

	prior_event = &(mdt.prior_touch_events[0]);
	input_report_key(mdt.dev_touchscreen, BTN_TOUCH, prior_event->isTouched);
	input_report_abs(mdt.dev_touchscreen, ABS_X, prior_event->abs_x);
	input_report_abs(mdt.dev_touchscreen, ABS_Y, prior_event->abs_y);
}
#elif defined(GINGERBREAD)
static void submit_touchscreen_events_with_protocol_A(uint8_t contactID)
{
	struct touch_history_t *prior_event;
	uint8_t i;
	uint8_t count = 0;

	for (i = 0; i < MDT_MAX_TOUCH_CONTACTS; i++) {
		prior_event = &(mdt.prior_touch_events[i]);

		if (prior_event->state == MDT_TOUCH_INACTIVE)
			continue;

		/* printk(KERN_ERR "MDT %x %x %x %x\n", */
		/* i, */
		/* prior_event->isTouched, */
		/* prior_event->abs_x, */
		/* prior_event->abs_y); */

		count++;

		if (prior_event->isTouched == 0)	/* Event handled; don't handle it again. */
			prior_event->state = MDT_TOUCH_INACTIVE;

		input_report_key(mdt.dev_touchscreen, BTN_TOUCH, prior_event->isTouched);
		input_report_abs(mdt.dev_touchscreen, ABS_MT_TOUCH_MAJOR, prior_event->isTouched);
		input_report_abs(mdt.dev_touchscreen, ABS_MT_TRACKING_ID, i);

#ifndef FROYO_SGHi997
		input_report_abs(mdt.dev_touchscreen, ABS_MT_WIDTH_MAJOR, 1);
#else
		input_report_abs(mdt.dev_touchscreen, ABS_MT_WIDTH_MAJOR, (i << 8) | 1);
#endif
		input_report_abs(mdt.dev_touchscreen, ABS_MT_POSITION_X, prior_event->abs_x);
		input_report_abs(mdt.dev_touchscreen, ABS_MT_POSITION_Y, prior_event->abs_y);
		input_mt_sync(mdt.dev_touchscreen);
	}

	if (count == 0)
		input_mt_sync(mdt.dev_touchscreen);
}
#elif defined(KERNEL_2_6_38_AND_LATER)
static void submit_touchscreen_events_with_protocol_B(uint8_t contactID)
{
	struct touch_history_t *prior_event;
	uint8_t i;
	uint8_t counter = 0;

	for (i = 0; i < MDT_MAX_TOUCH_CONTACTS; i++) {

		prior_event = &(mdt.prior_touch_events[i]);

		if (prior_event->state == MDT_TOUCH_INACTIVE)
			continue;

		input_mt_slot(mdt.dev_touchscreen, i);
		input_mt_report_slot_state(mdt.dev_touchscreen, MT_TOOL_FINGER,
					   prior_event->isTouched);

		if (prior_event->isTouched == 0) {	/* Event handled; don't handle it again. */
			prior_event->state = MDT_TOUCH_INACTIVE;
		} else {
			counter++;
			input_report_abs(mdt.dev_touchscreen, ABS_MT_TOUCH_MAJOR, 15);
			input_report_abs(mdt.dev_touchscreen, ABS_MT_PRESSURE, 50);


			input_report_abs(mdt.dev_touchscreen, ABS_MT_POSITION_X,
					 prior_event->abs_x);
			input_report_abs(mdt.dev_touchscreen, ABS_MT_POSITION_Y,
					 prior_event->abs_y);
		}

#if !defined(MDT_SUPPORT_WORKAROUND)
		if (counter)
			input_report_key(mdt.dev_touchscreen, BTN_TOUCH, 1);
		else
			input_report_key(mdt.dev_touchscreen, BTN_TOUCH, 0);
#endif
	}
}
#endif

/* This function generates mouse events */
void mdt_generate_event_keyboard(struct mdt_keyboard_event_t *keyboardPacket)
{
	int i;
	si_input_dev *dev_keyboard = mdt.dev_keyboard;
	uint8_t *keycodes_new = mdt.keycodes_new;
	uint8_t *keycodes_old = mdt.keycodes_old;
	printk(KERN_INFO "MHL keyboard info\n");

	if (is_keyboard_ready() == 0)
		return;

	/* printk(KERN_ERR "MDT kbd: %x %x %x\n", */
	/* keycodes_new[0], */
	/* keycodes_new[1], */
	/* keycodes_new[2]); */
	keycodes_new[0] = keyboardPacket->header.modifier_keys;
	memcpy((keycodes_new + 1), keyboardPacket->body.keycodes_all, 6);

	/* following code was copied from usbkeyboard.c */
	for (i = 0; i < 3; i++)	/* generate events for CRL, SHIFT, in the first */
		si_input_report_key(dev_keyboard, usb_kbd_keycode[i + 224], (keycodes_new[0] >> i) & 1);	/* HID byte */

	for (i = 1; i < 7; i++) {	/* generate events for the subsequent bytes */
		/* if keycode in pervious HID payload doens't appear in NEW HID payload, generate deassertion event */
		if ((keycodes_old[i] > 3)
		    && ((uint8_t *) memscan(keycodes_new + 1, keycodes_old[i], 6) ==
			((uint8_t *) (keycodes_new) + 7))) {
			if (usb_kbd_keycode[keycodes_old[i]]) {
				switch (usb_kbd_keycode[keycodes_old[i]]) {
#ifdef PHOENIX_BLADE
				case KEY_F1:
				case KEY_F2:
				case KEY_F3:
				case KEY_F4:
				case KEY_F12:
					break;
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
					mdt.simulated.left = 0;
					break;
#endif
				default:
					si_input_report_key(dev_keyboard,
							    usb_kbd_keycode[keycodes_old[i]], 0);
				}
			} else {
				SI_MDT_DEBUG_PRINT(("Unknown key (scancode %#x) released.\n",
						    keycodes_old[i]));
			}
		}
		/* if keycode in NEW HID paylaod doesn't appear in previous HID payload, generate assertion event */
		if (keycodes_new[i] > 3
		    && memscan(keycodes_old + 1, keycodes_new[i], 6) == keycodes_old + 7) {
			if (usb_kbd_keycode[keycodes_new[i]]) {
				switch (usb_kbd_keycode[keycodes_new[i]]) {
#ifdef PHOENIX_BLADE
				case KEY_F1:
					mdt.phoenix_blade_state = MDT_PB_SIMULATED_MOUSE_0;
					is_mouse_ready();
					break;
				case KEY_F2:
					mdt.phoenix_blade_state = MDT_PB_SIMULATED_MOUSE_90;
					is_mouse_ready();
					break;
				case KEY_F3:
					mdt.phoenix_blade_state = MDT_PB_SIMULATED_MOUSE_180;
					is_mouse_ready();
					break;
				case KEY_F4:
					mdt.phoenix_blade_state = MDT_PB_SIMULATED_MOUSE_270;
					is_mouse_ready();
					break;
				case KEY_F12:
					mdt.phoenix_blade_state = MDT_PB_NATIVE_TOUCH_SCREEN;
					mouse_deregister(1);
					break;
				case KEY_LEFTCTRL:
				case KEY_RIGHTCTRL:
					mdt.simulated.left = 1;
					break;
#endif
				default:
					si_input_report_key(dev_keyboard,
							    usb_kbd_keycode[keycodes_new[i]], 1);
				}
			} else {
				SI_MDT_DEBUG_PRINT((KERN_ERR
						    "Unknown key (scancode %#x) pressed.\n",
						    keycodes_new[i]));
			}
		}
	}

	si_input_sync(dev_keyboard);	/* generate event */
	memcpy(keycodes_old, keycodes_new, 7);	/* NEW HID payload is now OLD */
}

/* This function is a wrapper for input_event_mouse used to generate mouse events */
void mdt_generate_event_mouse(struct mdt_cursor_mouse_t *mousePacket)
{
	input_event_mouse(mousePacket->header.button,
			  mousePacket->body.XYZ.x_byteLen,
			  mousePacket->body.XYZ.y_byteLen, mousePacket->body.XYZ.z_byteLen);
}


/* Local, keyboard specific helper function for initialize keybaord input_dev */
static uint8_t init_keyboard(void)
{
#ifdef __KERNEL__
	int i;
	int error;
#endif
	si_input_dev *dev_keyboard;

	dev_keyboard = si_input_allocate_device();
	if (!dev_keyboard) {
		SI_MDT_DEBUG_PRINT(("MDTdev_keyboard: Not enough memory\n"));
		return -ENOMEM;
	}
#ifdef __KERNEL__
	set_bit(EV_KEY, dev_keyboard->evbit);
	set_bit(EV_REP, dev_keyboard->evbit);	/* driver doesn't use this but, can in the future */

	dev_keyboard->phys = "atakbd/input0";
	dev_keyboard->name = "MDTkeyboard";
	dev_keyboard->id.bustype = BUS_HOST;
	dev_keyboard->keycode = usb_kbd_keycode;
	dev_keyboard->keycodesize = sizeof(unsigned char);
	dev_keyboard->keycodemax = ARRAY_SIZE(usb_kbd_keycode);

	for (i = 1; i < 256; i++)
		set_bit(usb_kbd_keycode[i], dev_keyboard->keybit);

	dev_keyboard->id.bustype = BUS_USB;
	dev_keyboard->id.vendor = 0x1095;
	dev_keyboard->id.product = 0x8240;
	dev_keyboard->id.version = 0xA;	/* use version to distinguish mouse from keyboard */

	error = input_register_device(dev_keyboard);
	if (error) {
		SI_MDT_DEBUG_PRINT("MDTkeyboard: Failed to register device\n");
		return error;
	}
	mdt.dev_keyboard = dev_keyboard;
#endif
	SI_MDT_DEBUG_PRINT(("MDTdev_keyboard: driver loaded\n"));
	return 0;
}

/* Local, mouse specific helper function for initialize mouse input_dev */
static uint8_t init_mouse(void)
{
	si_input_dev *dev_mouse;
#ifdef __KERNEL__
	int error;
#endif

	dev_mouse = si_input_allocate_device();
	if (!dev_mouse) {
		SI_MDT_DEBUG_PRINT((KERN_ERR "MDTdev_mouse: Not enough memory\n"));
		return -ENOMEM;
	}
#ifdef __KERNEL__
	set_bit(EV_REL, dev_mouse->evbit);
	set_bit(EV_KEY, dev_mouse->evbit);
	set_bit(BTN_LEFT, dev_mouse->keybit);
	set_bit(BTN_RIGHT, dev_mouse->keybit);
	set_bit(BTN_MIDDLE, dev_mouse->keybit);
	set_bit(BTN_SIDE, dev_mouse->keybit);
	set_bit(BTN_EXTRA, dev_mouse->keybit);
	set_bit(REL_X, dev_mouse->relbit);
	set_bit(REL_Y, dev_mouse->relbit);
	set_bit(REL_WHEEL, dev_mouse->relbit);

	dev_mouse->name = "MDTmouse";
	dev_mouse->id.bustype = BUS_USB;
	dev_mouse->id.vendor = 0x1095;
	dev_mouse->id.product = 0x8240;
	dev_mouse->id.version = 0xB;	/* use version to distinguish mouse from keyboard */

	error = input_register_device(dev_mouse);
	if (error) {
		SI_MDT_DEBUG_PRINT(("MDTmouse: Failed to register device\n"));
		return error;
	}
	mdt.dev_mouse = dev_mouse;

	mdt.prior_game_event.x_delta = 0;
	mdt.prior_game_event.y_delta = 0;
	mdt.prior_game_event.abs_x = 0;
	mdt.prior_game_event.abs_y = 0;
	mdt.prior_game_event.dpad_event = 0;
	mdt.prior_game_event.other_buttons = 0;
	mdt.prior_mouse_buttons = 0;

#endif

	SI_MDT_DEBUG_PRINT(("MDTmouse: driver loaded\n"));
	return 0;
}

/* Local, touchscreen specific helper function for initialize touchscreen input_dev */
static uint8_t init_touchscreen(void)
{
#ifdef MDT_SUPPORT_WORKAROUND
	mdt.dev_touchscreen = get_native_touchscreen_dev();

	if (mdt.dev_touchscreen != 0)
		return 0;
	else
		return 0xFF;
#else
	si_input_dev *dev_touchscreen;
#ifdef __KERNEL__
	int error;
#endif
	dev_touchscreen = si_input_allocate_device();
	if (!dev_touchscreen) {
		SI_MDT_DEBUG_PRINT((KERN_ERR "MDTdev_touchscreen: Not enough memory\n"));
		return -ENOMEM;
	}
#ifdef __KERNEL__
#if (SINGLE_TOUCH == 0)
#ifdef KERNEL_2_6_38_AND_LATER
	input_mt_init_slots(dev_touchscreen, MDT_MAX_TOUCH_CONTACTS);
#endif
#endif
	dev_touchscreen->name = "MDTtouchscreen";
	dev_touchscreen->id.bustype = BUS_USB;
	dev_touchscreen->id.vendor = 0x1095;
	dev_touchscreen->id.product = 0x8240;
	dev_touchscreen->id.version = 0xC;	/* use version to distinguish touchscreen from keyboard and mouse */

#if (SINGLE_TOUCH == 1)
	__set_bit(EV_ABS, dev_touchscreen->evbit);
	__set_bit(ABS_X, dev_touchscreen->absbit);
	__set_bit(ABS_Y, dev_touchscreen->absbit);
	__set_bit(EV_KEY, dev_touchscreen->evbit);
#if     (CORNER_BUTTON == 1)
	__set_bit(KEY_ESC, dev_touchscreen->keybit);
#endif
	__set_bit(BTN_TOUCH, dev_touchscreen->keybit);
#ifdef KERNEL_2_6_38_AND_LATER
	__set_bit(INPUT_PROP_DIRECT, dev_touchscreen->propbit);
#endif
	input_set_abs_params(dev_touchscreen, ABS_X, 0, X_MAX, 0, 0);
	input_set_abs_params(dev_touchscreen, ABS_Y, 0, Y_MAX, 0, 0);
#else
	__set_bit(EV_ABS, dev_touchscreen->evbit);
	__set_bit(EV_KEY, dev_touchscreen->evbit);
#ifdef KERNEL_2_6_38_AND_LATER
	__set_bit(EV_SYN, dev_touchscreen->evbit);
	__set_bit(MT_TOOL_FINGER, dev_touchscreen->keybit);
	__set_bit(INPUT_PROP_DIRECT, dev_touchscreen->propbit);
	input_mt_init_slots(dev_touchscreen, MDT_MAX_TOUCH_CONTACTS);
#else
	__set_bit(BTN_TOUCH, dev_touchscreen->keybit);
	input_set_abs_params(dev_touchscreen, ABS_MT_WIDTH_MAJOR, 0, 3, 0, 0);
	input_set_abs_params(dev_touchscreen, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#endif
#if     (CORNER_BUTTON == 1)
	__set_bit(KEY_ESC, dev_touchscreen->keybit);
#endif
	input_set_abs_params(dev_touchscreen, ABS_MT_TOUCH_MAJOR, 0, 30, 0, 0);
	/* #if defined(MDT_SUPPORT_WORKAROUND) */
	/* #if !defined(FROYO_SGHi997) */
	input_set_abs_params(dev_touchscreen, ABS_MT_PRESSURE, 0, 255, 0, 0);
	/* #endif */
	/* #endif */
	input_set_abs_params(dev_touchscreen, ABS_MT_POSITION_X, 0, X_MAX, 0, 0);
	input_set_abs_params(dev_touchscreen, ABS_MT_POSITION_Y, 0, Y_MAX, 0, 0);
#endif

	error = input_register_device(dev_touchscreen);
	if (error) {
		SI_MDT_DEBUG_PRINT(("MDTtouch: Failed to register device\n"));
		return error;
	}
	mdt.dev_touchscreen = dev_touchscreen;
#endif
#endif
	/* need to initialize history; in parcitular initialize state elements with MDT_TOUCH_INACTIVE */
	memset(mdt.prior_touch_events, 0,
	       MDT_MAX_TOUCH_CONTACTS * sizeof(mdt.prior_touch_events[0]));

#ifdef PHOENIX_BLADE
	mdt.phoenix_blade_state = MDT_PB_NATIVE_TOUCH_SCREEN;
#endif

	SI_MDT_DEBUG_PRINT(("MDTtouchscreen: driver loaded\n"));

	return 0;
}


/* Local, helper functions to initialize input_dev */
static uint8_t is_mdt_dev_disabled(uint8_t dev_type)
{
	if (mdt.is_dev_registered[dev_type] == INPUT_DISABLED)
		return 1;
	else
		return 0;
}

static uint8_t is_mdt_dev_waiting(uint8_t dev_type)
{
	if (mdt.is_dev_registered[dev_type] == INPUT_WAITING_FOR_REGISTRATION)
		return 1;
	else
		return 0;
}

uint8_t mdt_input_init(void)
{
	memset(mdt.is_dev_registered, INPUT_WAITING_FOR_REGISTRATION, DEV_TYPE_COUNT);
	memset(mdt.keycodes_old, 0, HID_INPUT_REPORT);
	memset(mdt.keycodes_new, 0, HID_INPUT_REPORT);
	/* need to initialize history; in parcitular initialize state elements with MDT_TOUCH_INACTIVE */
	memset(mdt.prior_touch_events, 0,
	       MDT_MAX_TOUCH_CONTACTS * sizeof(mdt.prior_touch_events[0]));
	memset(&mdt.prior_game_event, 0, sizeof(mdt.prior_game_event));

	mdt.dev_mouse = 0;
	mdt.dev_keyboard = 0;
	mdt.dev_touchscreen = 0;
	mdt.mdt_joystick_wq = 0;

	mdt.prior_game_event.x_delta = 0;
	mdt.prior_game_event.y_delta = 0;
	mdt.prior_game_event.abs_x = 0;
	mdt.prior_game_event.abs_y = 0;
	mdt.prior_game_event.dpad_event = 0;
	mdt.prior_game_event.other_buttons = 0;
	mdt.prior_mouse_buttons = 0;

#ifdef PHOENIX_BLADE
	mdt.phoenix_blade_state = MDT_PB_NATIVE_TOUCH_SCREEN;
	mdt.touch_debounce_counter = 0;
	mdt.prior_native_touch.abs_x = 0;
	mdt.prior_native_touch.abs_y = 0;
	/* mdt.prior_native_touch.isTouched= 0;                          // These 2 fields are not used for pior_native_touch */
	/* mdt.prior_native_touch.state  = 0; */

	mdt.simulated.left = 0;
	mdt.simulated.middle = 0;
	mdt.simulated.right = 0;

	mdt.double_touch.duration_release = 0;
	mdt.double_touch.duration_touch = 0;
	mdt.double_touch.last_release = 0;
	mdt.double_touch.last_touch = 0;
	mdt.double_touch.state = MDT_PB_WAIT_FOR_TOUCH_RELEASE;
#endif

	mdt.mdt_joystick_wq = create_singlethread_workqueue("mdt_joystick_wq");
	INIT_DELAYED_WORK(&(mdt.repeat_for_gamepad), repeat_for_gamepad_func);

#ifdef MDT_SUPPORT_WORKAROUND
	release_native_touchscreen_dev();
#endif

	return 0;
}

static void mouse_deregister(unsigned char isReset)
{
	if (mdt.dev_mouse == 0)
		return;

	input_unregister_device(mdt.dev_mouse);
	mdt.prior_mouse_buttons = 0;
	mdt.dev_mouse = 0;

	if (isReset == 0)
		mdt.is_dev_registered[DEV_TYPE_MOUSE] = INPUT_DISABLED;
	else
		mdt.is_dev_registered[DEV_TYPE_MOUSE] = INPUT_WAITING_FOR_REGISTRATION;

}

static void touch_deregister(unsigned char isReset)
{
	if (mdt.dev_touchscreen == 0)
		return;

#ifdef PHOENIX_BLADE
	mdt.phoenix_blade_state = MDT_PB_NATIVE_TOUCH_SCREEN;
#endif

#ifdef MDT_SUPPORT_WORKAROUND
	release_native_touchscreen_dev();
#else
	input_unregister_device(mdt.dev_touchscreen);
#endif
	memset(mdt.prior_touch_events, 0,
	       MDT_MAX_TOUCH_CONTACTS * sizeof(mdt.prior_touch_events[0]));
	mdt.dev_touchscreen = 0;

	if (isReset == 0)
		mdt.is_dev_registered[DEV_TYPE_TOUCH] = INPUT_DISABLED;
	else
		mdt.is_dev_registered[DEV_TYPE_TOUCH] = INPUT_WAITING_FOR_REGISTRATION;
}



void mdt_input_deregister(void)
{

	mouse_deregister(0);
	touch_deregister(0);

	memset(mdt.is_dev_registered, INPUT_DISABLED, DEV_TYPE_COUNT);

	if (mdt.dev_keyboard)
		input_unregister_device(mdt.dev_keyboard);
	if (mdt.dev_mouse)
		input_unregister_device(mdt.dev_mouse);
	if (mdt.mdt_joystick_wq) {
		if (!cancel_delayed_work(&(mdt.repeat_for_gamepad)))
			flush_workqueue(mdt.mdt_joystick_wq);
		destroy_workqueue(mdt.mdt_joystick_wq);
	}
#ifdef MDT_SUPPORT_WORKAROUND
	release_native_touchscreen_dev();
#else
	if (mdt.dev_touchscreen)
		input_unregister_device(mdt.dev_touchscreen);
#endif

	mdt.dev_keyboard = 0;
	mdt.dev_mouse = 0;
	mdt.dev_touchscreen = 0;
	mdt.mdt_joystick_wq = 0;
}


#ifdef PHOENIX_BLADE
unsigned char get_mdtdemo_simulated_btn(unsigned char button)
{
	switch (button) {
	case (unsigned char)BTN_LEFT:
		return mdt.simulated.left;
		break;
	case (unsigned char)BTN_MIDDLE:
		return mdt.simulated.middle;
		break;
	case (unsigned char)BTN_RIGHT:
		return mdt.simulated.right;
		break;
	};
	return 0xFF;
}

void set_mdtdemo_simulated_btn(unsigned char button, unsigned char value, unsigned char do_sync)
{

	si_input_dev *dev_mouse = mdt.dev_mouse;

	switch (button) {
	case (unsigned char)BTN_LEFT:
		mdt.simulated.left = value;
		break;
	case (unsigned char)BTN_MIDDLE:
		mdt.simulated.middle = value;
		break;
	case (unsigned char)BTN_RIGHT:
		mdt.simulated.right = value;
		break;
	};

	if (do_sync) {
		if (is_mouse_ready() == 0)
			return;

		if ((value & mdt.prior_mouse_buttons) == 0) {
			si_input_report_key(dev_mouse, button, value);
			si_input_sync(dev_mouse);
			if (value)
				mdt.prior_mouse_buttons |= button;
			else
				mdt.prior_mouse_buttons &= ~button;
		}

	}

}

unsigned char get_phoenix_blade_state(void)
{
	return (unsigned char)mdt.phoenix_blade_state;
}

void set_phoenix_blade_state(unsigned char value)
{
	mdt.phoenix_blade_state = value;
	if (value == MDT_PB_NATIVE_TOUCH_SCREEN)
		mouse_deregister(1);
	else
		is_mouse_ready();
}

#endif
#endif
#endif
