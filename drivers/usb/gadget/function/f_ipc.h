/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef	__F_IPC_H
#define	__F_IPC_H

void *ipc_setup(void);
void ipc_cleanup(void *fi);
struct usb_function *ipc_bind_config(struct usb_function_instance *fi);

#endif	/* __F_IPC_H */
