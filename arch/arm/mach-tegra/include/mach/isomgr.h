/*
 * include/mach/isomgr.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _INCLUDE_MACH_ISOMGR_H
#define _INCLUDE_MACH_ISOMGR_H

enum tegra_iso_client {
	TEGRA_ISO_CLIENT_DISP_0,
	TEGRA_ISO_CLIENT_DISP_1,
	TEGRA_ISO_CLIENT_VI_0,
	TEGRA_ISO_CLIENT_VI_1,
	TEGRA_ISO_CLIENT_ISP_A,
	TEGRA_ISO_CLIENT_ISP_B,
	TEGRA_ISO_CLIENT_BBC_0,
	TEGRA_ISO_CLIENT_COUNT
};

/* handle to identify registered client */
#define tegra_isomgr_handle void *

/* callback to client to renegotiate ISO BW allocation */
typedef void (*tegra_isomgr_renegotiate)(void *priv,
					 u32 avail_bw); /* KB/sec */

#if defined(CONFIG_TEGRA_ISOMGR)
/* Register an ISO BW client */
tegra_isomgr_handle tegra_isomgr_register(enum tegra_iso_client client,
					  u32 dedicated_bw,	/* KB/sec */
					  tegra_isomgr_renegotiate renegotiate,
					  void *priv);

/* Unregister an ISO BW client */
void tegra_isomgr_unregister(tegra_isomgr_handle handle);

/* Reserve ISO BW on behalf of client - don't apply, rval is dvfs thresh usec */
u32 tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 bw,	/* KB/sec */
			 u32 lt);	/* usec */

/* Realize client reservation - apply settings, rval is dvfs thresh usec */
u32 tegra_isomgr_realize(tegra_isomgr_handle handle);

/* This sets bw aside for the client specified. */
int tegra_isomgr_set_margin(enum tegra_iso_client client, u32 bw, bool wait);

int tegra_isomgr_get_imp_time(enum tegra_iso_client, u32 bw);

/* returns available in iso bw in KB/sec */
u32 tegra_isomgr_get_available_iso_bw(void);

/* returns total iso bw in KB/sec */
u32 tegra_isomgr_get_total_iso_bw(void);

/* Initialize isomgr.
 * This api would be called by .init_machine during boot.
 * isomgr clients, don't call this api.
 */
int __init isomgr_init(void);
#else
static inline tegra_isomgr_handle tegra_isomgr_register(
					  enum tegra_iso_client client,
					  u32 dedicated_bw,
					  tegra_isomgr_renegotiate renegotiate,
					  void *priv)
{
	static tegra_isomgr_handle h;
	/* return a dummy handle to allow client function
	 * as if isomgr were enabled.
	 */
	return h;
}

static inline void tegra_isomgr_unregister(tegra_isomgr_handle handle) {}

static inline u32 tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 bw, u32 lt)
{
	return 1;
}

static inline u32 tegra_isomgr_realize(tegra_isomgr_handle handle)
{
	return 1;
}

static inline int tegra_isomgr_set_margin(enum tegra_iso_client client, u32 bw)
{
	return 0;
}

static inline int tegra_isomgr_get_imp_time(enum tegra_iso_client client,
	u32 bw)
{
	return 0;
}

static inline u32 tegra_isomgr_get_available_iso_bw(void)
{
	return UINT_MAX;
}

static inline u32 tegra_isomgr_get_total_iso_bw(void)
{
	return UINT_MAX;
}

static inline int isomgr_init(void)
{
	return 0;
}
#endif
#endif /* _INCLUDE_MACH_ISOMGR_H */
