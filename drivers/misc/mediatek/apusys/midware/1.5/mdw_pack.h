/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_PACK_H__
#define __APUSYS_MDW_PACK_H__

int mdw_pack_check(void);
int mdw_pack_dispatch(struct mdw_apu_sc *sc);
int mdw_pack_init(void);
void mdw_pack_exit(void);

#endif
