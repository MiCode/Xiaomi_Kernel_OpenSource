/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PORT_CTLMSG_H__
#define __PORT_CTLMSG_H__

#include "ccci_core.h"

/****************************************************************************************************************/
/* External API Region called by port ctl object */
/****************************************************************************************************************/
extern int mdee_ctlmsg_handler(struct ccci_port *port, struct sk_buff *skb);
#endif	/* __PORT_CTLMSG_H__ */
