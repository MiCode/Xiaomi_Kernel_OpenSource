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

#ifndef __PORT_CFG_H__
#define __PORT_CFG_H__
#include "port_proxy.h"

/* external: port ops  mapping */
extern struct ccci_port_ops char_port_ops;
extern struct ccci_port_ops net_port_ops;
extern struct ccci_port_ops rpc_port_ops;
extern struct ccci_port_ops sys_port_ops;
extern struct ccci_port_ops poller_port_ops;
extern struct ccci_port_ops ctl_port_ops;
extern struct ccci_port_ops ipc_port_ack_ops;
extern struct ccci_port_ops ipc_kern_port_ops;
#endif /* __PORT_CFG_H__ */
