/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef __PORT_CHAR__
#define __PORT_CHAR__
#include "ccci_core.h"
#include "port_t.h"
/* External API called by port_char object */
extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
		unsigned int length);
#endif	/*__PORT_CHAR__*/
