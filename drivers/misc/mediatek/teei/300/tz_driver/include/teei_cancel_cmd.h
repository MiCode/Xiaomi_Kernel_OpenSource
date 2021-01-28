/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
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

#ifndef TEEI_CANCEL_CMD_H
#define TEEI_CANCEL_CMD_H

extern unsigned long cancel_message_buff;
extern struct semaphore fp_lock;

unsigned long create_cancel_fdrv(int buff_size);
int send_cancel_command(unsigned long share_memory_size);
int __send_cancel_command(unsigned long share_memory_size);

#endif /* end of TEEI_CANCEL_CMD_H */
