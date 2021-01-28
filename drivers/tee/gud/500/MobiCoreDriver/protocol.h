/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_PROTOCOL_H_
#define _MC_PROTOCOL_H_

/* Returns 1 to if probe and start are consumed and init should stop */
int protocol_early_init(int (*probe)(void), int (*start)(void));
int protocol_init(void);
void protocol_exit(void);
int protocol_start(void);
void protocol_stop(void);
const char *protocol_vm_id(void);
bool protocol_is_be(void);
bool protocol_fe_uses_pages_and_vas(void);

#endif /* _MC_PROTOCOL_H_ */
