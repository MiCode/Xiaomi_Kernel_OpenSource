/*
 * drivers/video/tegra/host/gk20a/debug_gk20a.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

void gk20a_debug_show_channel_cdma(struct nvhost_master *m,
				struct nvhost_channel *ch,
				struct output *o, int chid);
void gk20a_debug_show_channel_fifo(struct nvhost_master *m,
				struct nvhost_channel *ch,
				struct output *o, int chid);
