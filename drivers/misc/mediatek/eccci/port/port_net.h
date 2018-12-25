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
#ifndef __PORT_NET_H__
#define __PORT_NET_H__
#include "ccmni.h"
#include "port_t.h"
extern struct ccmni_dev_ops ccmni_ops;

extern int mbim_start_xmit(struct sk_buff *skb, int ifid);
#endif /*__PORT_NET_H__*/
