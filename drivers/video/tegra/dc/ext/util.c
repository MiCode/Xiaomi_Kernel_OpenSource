/*
 * drivers/video/tegra/dc/ext/util.c
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/err.h>
#include <linux/types.h>

#include <mach/dc.h>
#include <linux/nvmap.h>

/* ugh */
#include "../../nvmap/nvmap.h"

#include "tegra_dc_ext_priv.h"

int tegra_dc_ext_pin_window(struct tegra_dc_ext_user *user, u32 id,
			    struct nvmap_handle_ref **handle,
			    dma_addr_t *phys_addr)
{
	struct tegra_dc_ext *ext = user->ext;
	struct nvmap_handle_ref *win_dup;
	struct nvmap_handle *win_handle;
	dma_addr_t phys;

	if (!id) {
		*handle = NULL;
		*phys_addr = -1;

		return 0;
	}

	/*
	 * Take a reference to the buffer using the user's nvmap context, to
	 * make sure they have permissions to access it.
	 */
	win_handle = nvmap_get_handle_id(user->nvmap, id);
	if (!win_handle)
		return -EACCES;

	/*
	 * Duplicate the buffer's handle into the dc_ext driver's nvmap
	 * context, to ensure that the handle won't be freed as long as it is
	 * in use by display.
	 */
	win_dup = nvmap_duplicate_handle_id(ext->nvmap, id);

	/* Release the reference we took in the user's context above */
	nvmap_handle_put(win_handle);

	if (IS_ERR(win_dup))
		return PTR_ERR(win_dup);

	phys = nvmap_pin(ext->nvmap, win_dup);
	/* XXX this isn't correct for non-pointers... */
	if (IS_ERR((void *)phys)) {
		nvmap_free(ext->nvmap, win_dup);
		return PTR_ERR((void *)phys);
	}

	*phys_addr = phys;
	*handle = win_dup;

	return 0;
}
