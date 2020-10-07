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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __FPSGO_UBOOST_H__
#define __FPSGO_UBOOST_H__

#include "fpsgo_base.h"

void fpsgo_base2uboost_compute(struct render_info *render, unsigned long long ts);
void fpsgo_base2uboost_init(struct render_info *obj);
void fpsgo_base2uboost_cancel(struct render_info *obj);

int __init fpsgo_uboost_init(void);
void __exit fpsgo_uboost_exit(void);

#endif
