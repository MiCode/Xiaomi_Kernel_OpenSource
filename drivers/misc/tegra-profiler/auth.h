/*
 * drivers/misc/tegra-profiler/auth.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_AUTH_H__
#define __QUADD_AUTH_H__

struct quadd_ctx;

int quadd_auth_is_debuggable(const char *package_name);
int quadd_auth_is_auth_open(void);

int quadd_auth_init(struct quadd_ctx *quadd_ctx);
void quadd_auth_deinit(void);

#endif	/* __QUADD_AUTH_H__ */
