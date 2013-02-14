/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __QCI_KBD_H__
#define __QCI_KBD_H__

/**
 * struct qci_kbd_platform_data - platform data for keyboard
 * @repeat: enable or disable key repeate feature
 *
 * platform data structure for QCI keyboard driver.
 */
struct qci_kbd_platform_data {
	bool repeat;
	bool standard_scancodes;
	bool kb_leds;
};

#endif /*__QCI_KBD_H__*/
