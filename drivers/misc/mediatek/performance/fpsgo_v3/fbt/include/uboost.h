/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
