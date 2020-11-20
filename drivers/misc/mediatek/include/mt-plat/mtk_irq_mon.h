/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MTK_IRQ_MON__
#define __MTK_IRQ_MON__

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
extern void mt_aee_dump_irq_info(void);
#else
#define mt_aee_dump_irq_info() do {} while (0)
#endif

#endif

