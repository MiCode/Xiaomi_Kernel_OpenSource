/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __PORT_CHAR__
#define __PORT_CHAR__
#include "ccci_core.h"
#include "port_t.h"
/* External API called by port_char object */
extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
		unsigned int length);
#endif	/*__PORT_CHAR__*/
