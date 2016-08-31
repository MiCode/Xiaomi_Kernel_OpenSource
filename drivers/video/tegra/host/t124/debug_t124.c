/*
 * drivers/video/tegra/host/t124/debug_t124.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_cdma.h"

#include "t124.h"
#include "hardware_t124.h"
#include "gk20a/gk20a.h"
#include "gk20a/debug_gk20a.h"

#include "chip_support.h"

#include "host1x/host1x_debug.c"

static void t124_debug_show_channel_cdma(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	nvhost_dbg_fn("");

#if defined(CONFIG_TEGRA_GK20A)
	if (is_gk20a_module(ch->dev))
		gk20a_debug_show_channel_cdma(m, ch, o, chid);
	else
#endif
		t20_debug_show_channel_cdma(m, ch, o, chid);
}

void t124_debug_show_channel_fifo(struct nvhost_master *m,
	struct nvhost_channel *ch, struct output *o, int chid)
{
	nvhost_dbg_fn("");

#if defined(CONFIG_TEGRA_GK20A)
	if (is_gk20a_module(ch->dev))
		gk20a_debug_show_channel_fifo(m, ch, o, chid);
	else
#endif
		t20_debug_show_channel_fifo(m, ch, o, chid);
}

static void t124_debug_show_mlocks(struct nvhost_master *m, struct output *o)
{
	u32 __iomem *mlo_regs = m->sync_aperture +
		host1x_sync_mlock_owner_0_r();
	int i;

	nvhost_debug_output(o, "---- mlocks ----\n");
	for (i = 0; i < NV_HOST1X_NB_MLOCKS; i++) {
		u32 owner = readl(mlo_regs + i * 4);
		if (owner & 0x1)
			nvhost_debug_output(o, "%d: locked by channel %d\n",
					    i, (owner >> 8) & 0xf);
		else if (owner & 0x2)
			nvhost_debug_output(o, "%d: locked by cpu\n", i);
		else
			nvhost_debug_output(o, "%d: unlocked\n", i);
	}
	nvhost_debug_output(o, "\n");
}

int nvhost_init_t124_debug_support(struct nvhost_chip_support *op)
{
	op->debug.show_channel_cdma = t124_debug_show_channel_cdma;
	op->debug.show_channel_fifo = t124_debug_show_channel_fifo;
	op->debug.show_mlocks = t124_debug_show_mlocks;

	return 0;
}
