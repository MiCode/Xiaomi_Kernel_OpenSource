// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_DISPR_H__
#define __APUSYS_MDW_DISPR_H__

int mdw_dispr_check(void);
int mdw_dispr_norm(struct mdw_apu_sc *sc);
int mdw_dispr_multi(struct mdw_apu_sc *sc);
int mdw_dispr_pack(struct mdw_apu_sc *sc);
int mdw_dispr_init(void);
void mdw_dispr_exit(void);

#endif
