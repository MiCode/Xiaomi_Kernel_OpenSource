/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _DSI_NOTIFY_H_
#define _DSI_NOTIFY_H_
extern int dsi_panel_register_client(struct notifier_block *nb);
extern int dsi_panel_unregister_client(struct notifier_block *nb);
extern int dsi_panel_notifier_call_chain(unsigned long val, void *v);
#endif
