/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef _TUI_HAL_H_
#define _TUI_HAL_H_

#include <linux/types.h>

uint32_t hal_tui_init(void);
void hal_tui_exit(void);
uint32_t hal_tui_deactivate(void);
uint32_t hal_tui_activate(void);

#endif
