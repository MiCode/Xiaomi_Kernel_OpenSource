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


#ifndef _SIA81XX_SOCKET_H
#define _SIA81XX_SOCKET_H

int sia81xx_sock_init(void);
void sia81xx_sock_exit(void);
int sia81xx_open_sock_server(void);
int sia81xx_close_sock_server(void);


#endif /* _SIA81XX_SOCKET_H */

