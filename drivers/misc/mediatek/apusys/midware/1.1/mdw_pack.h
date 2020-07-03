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

#ifndef __APUSYS_MDW_PACK_H__
#define __APUSYS_MDW_PACK_H__

int mdw_pack_check(void);
int mdw_pack_dispatch(struct mdw_apu_sc *sc);
int mdw_pack_init(void);
void mdw_pack_exit(void);

#endif
