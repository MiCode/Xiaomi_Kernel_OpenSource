/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_UART_EXPORTED_API__
#define __MTK_UART_EXPORTED_API__

int request_uart_to_sleep(void);
int request_uart_to_wakeup(void);

void stop_log(void);
void dump_uart_history(void);

#endif /* __MTK_UART_EXPORTED_API__ */
