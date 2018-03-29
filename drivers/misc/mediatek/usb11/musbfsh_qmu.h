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

#ifndef _MUSBFSH_QMU_H_
#define _MUSBFSH_QMU_H_

#include "musbfsh_core.h"		/* for struct musb */

#define MUSBFSH_QMUBASE	(0x800)
#define MUSBFSH_QISAR	(0xc00)
#define MUSBFSH_QIMR	(0xc04)

extern void __iomem *musbfsh_qmu_base;

extern int musbfsh_qmu_init(struct musbfsh *musbfsh);
extern void musbfsh_qmu_exit(struct musbfsh *musbfsh);
extern void musbfsh_disable_q_all(struct musbfsh *musbfsh);
extern irqreturn_t musbfsh_q_irq(struct musbfsh *musbfsh);
extern void musbfsh_flush_qmu(u32 ep_num, u8 isRx);
extern void musbfsh_restart_qmu(struct musbfsh *musbfsh, u32 ep_num, u8 isRx);
extern bool musbfsh_is_qmu_stop(u32 ep_num, u8 isRx);
extern void musbfsh_tx_zlp_qmu(struct musbfsh *musbfsh, u32 ep_num);
extern void mtk11_qmu_enable(struct musbfsh *musbfsh, u8 EP_Num, u8 isRx);
extern int mtk11_kick_CmdQ(struct musbfsh *musbfsh, int isRx, struct musbfsh_qh *qh, struct urb *urb);
#endif
