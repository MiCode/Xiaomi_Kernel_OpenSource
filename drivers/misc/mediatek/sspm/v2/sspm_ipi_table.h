/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "sspm_ipi_id.h"
#include "sspm_mbox_pin.h"

extern void sspm_ipi_timeout_cb(int ipi_id);

extern struct mtk_mbox_info sspm_mbox_table[];
extern struct mtk_mbox_pin_send sspm_mbox_pin_send[];
extern struct mtk_mbox_pin_recv sspm_mbox_pin_recv[];
extern struct mtk_mbox_device sspm_mboxdev;
extern struct mtk_ipi_device sspm_ipidev;
