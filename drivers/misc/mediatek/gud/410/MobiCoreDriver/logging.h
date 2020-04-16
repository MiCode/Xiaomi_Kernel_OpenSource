/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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

#ifndef _MC_LOGGING_H_
#define _MC_LOGGING_H_

void logging_run(void);
int logging_init(phys_addr_t *buffer, u32 *size);
void logging_exit(bool buffer_busy);

#endif /* _MC_LOGGING_H_ */
