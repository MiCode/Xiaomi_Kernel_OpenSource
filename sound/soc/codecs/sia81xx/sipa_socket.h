/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#ifndef _SIPA_SOCKET_H
#define _SIPA_SOCKET_H

int sipa_sock_init(void);
void sipa_sock_exit(void);
int sipa_open_sock_server(void);
int sipa_close_sock_server(void);


#endif /* _SIPA_SOCKET_H */
