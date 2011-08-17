/* drivers/usb/gadget/f_diag.h
 *
 * Diag Function Device - Route DIAG frames between SMD and USB
 *
 * Copyright (C) 2008-2009 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __F_DIAG_H
#define __F_DIAG_H

int diag_function_add(struct usb_configuration *c, const char *);

#endif /* __F_DIAG_H */

