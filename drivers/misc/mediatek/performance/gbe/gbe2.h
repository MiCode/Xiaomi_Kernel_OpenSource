/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GBE2_H
#define GBE2_H

#if defined(CONFIG_MTK_GBE)
void fpsgo_comp2gbe_frame_update(int pid, unsigned long long bufID);
#else
static inline void fpsgo_comp2gbe_frame_update(int pid,
	unsigned long long bufID) { }
#endif
extern void gbe2_exit(void);

extern int gbe2_init(void);

#endif

