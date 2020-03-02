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

#ifndef __MTK_UART_EXPORTED_API__
#define __MTK_UART_EXPORTED_API__

int request_uart_to_sleep(void);
int request_uart_to_wakeup(void);

void stop_log(void);
void dump_uart_history(void);

#endif /* __MTK_UART_EXPORTED_API__ */
