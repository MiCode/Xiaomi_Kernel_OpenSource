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
#ifndef __PORT_CFG_H__
#define __PORT_CFG_H__
#include "port_t.h"

/* external: port ops  mapping */
extern struct port_ops char_port_ops;
extern struct port_ops net_port_ops;
extern struct port_ops rpc_port_ops;
extern struct port_ops sys_port_ops;
extern struct port_ops poller_port_ops;
extern struct port_ops ctl_port_ops;
extern struct port_ops ipc_port_ops;
extern struct port_ops smem_port_ops;
extern struct port_ops ccci_udc_port_ops;
#endif /* __PORT_CFG_H__ */
