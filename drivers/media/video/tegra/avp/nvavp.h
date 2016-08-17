/*
 * Copyright (C) 2011 Nvidia Corp
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MEDIA_VIDEO_TEGRA_NVAVP_H
#define __MEDIA_VIDEO_TEGRA_NVAVP_H

#include <linux/tegra_avp.h>

struct tegra_avp_info;

int tegra_avp_open(struct tegra_avp_info **avp);
int tegra_avp_release(struct tegra_avp_info *avp);
int tegra_avp_load_lib(struct tegra_avp_info *avp, struct tegra_avp_lib *lib);
int tegra_avp_unload_lib(struct tegra_avp_info *avp, unsigned long handle);


#include <linux/tegra_sema.h>

struct tegra_sema_info;

int tegra_sema_open(struct tegra_sema_info **sema);
int tegra_sema_release(struct tegra_sema_info *sema);
int tegra_sema_wait(struct tegra_sema_info *sema, long* timeout);
int tegra_sema_signal(struct tegra_sema_info *sema);


#include <linux/tegra_rpc.h>

struct tegra_rpc_info;

int tegra_rpc_open(struct tegra_rpc_info **rpc);
int tegra_rpc_release(struct tegra_rpc_info *rpc);
int tegra_rpc_port_create(struct tegra_rpc_info *rpc, char *name,
			  struct tegra_sema_info *sema);
int tegra_rpc_get_name(struct tegra_rpc_info *rpc, char* name);
int tegra_rpc_port_connect(struct tegra_rpc_info *rpc, long timeout);
int tegra_rpc_port_listen(struct tegra_rpc_info *rpc, long timeout);
int tegra_rpc_write(struct tegra_rpc_info *rpc, u8* buf, size_t size);
int tegra_rpc_read(struct tegra_rpc_info *rpc, u8 *buf, size_t max);


#endif
