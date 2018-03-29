/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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

#ifndef TLCTUI_H_
#define TLCTUI_H_

void reset_global_command_id(void);
int tlc_wait_cmd(uint32_t *cmd_id);
int tlc_ack_cmd(struct tlc_tui_response_t *rsp_id);
bool tlc_notify_event(uint32_t event_type);

#endif /* TLCTUI_H_ */
