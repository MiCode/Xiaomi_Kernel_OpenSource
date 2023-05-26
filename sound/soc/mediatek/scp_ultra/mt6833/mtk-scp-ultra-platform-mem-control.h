/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_ULTRA_PLATFORM_MEM_CONTROL_H
#define _MTK_SCP_ULTRA_PLATFORM_MEM_CONTROL_H

#define SCP_IRQ_SHIFT_BIT (16)

const int get_scp_ultra_memif_id(int scp_spk_task_id);

const int set_afe_clock(bool enable, struct mtk_base_afe *afe);

#endif
