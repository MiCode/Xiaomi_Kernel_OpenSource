/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
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
