/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

void mc_logging_run(void);
int  mc_logging_init(void);
void mc_logging_exit(void);
int mc_logging_start(void);
void mc_logging_stop(void);

#endif /* _MC_LOGGING_H_ */
