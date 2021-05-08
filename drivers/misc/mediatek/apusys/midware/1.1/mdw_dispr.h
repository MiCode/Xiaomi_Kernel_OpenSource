/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __APUSYS_MDW_DISPR_H__
#define __APUSYS_MDW_DISPR_H__

int mdw_dispr_check(void);
int mdw_dispr_norm(struct mdw_apu_sc *sc);
int mdw_dispr_multi(struct mdw_apu_sc *sc);
int mdw_dispr_pack(struct mdw_apu_sc *sc);
int mdw_dispr_init(void);
void mdw_dispr_exit(void);

#endif
