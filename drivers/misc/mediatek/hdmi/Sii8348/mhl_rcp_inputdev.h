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

#ifndef _MHL_RCP_INPUTDEV_H_
#define _MHL_RCP_INPUTDEV_H_

struct mhl_dev_context;


int generate_rcp_input_event(struct mhl_dev_context *dev_context,
							 uint8_t rcp_keycode);

uint8_t init_rcp_input_dev(struct mhl_dev_context *dev_context);

void destroy_rcp_input_dev(struct mhl_dev_context *dev_context);

#endif /* #ifndef _MHL_RCP_INPUTDEV_H_ */
