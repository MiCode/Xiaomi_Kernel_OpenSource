/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_CAMERA_COMMON_H_
#define _TEGRA_CAMERA_COMMON_H_

#include <linux/videodev2.h>

#include <media/videobuf2-dma-contig.h>
#include <media/soc_camera.h>

/* Buffer for one video frame */
struct tegra_camera_buffer {
	struct vb2_buffer		vb; /* v4l buffer must be first */
	struct list_head		queue;
	struct soc_camera_device	*icd;
	int				output_channel;

	/*
	 * Various buffer addresses shadowed so we don't have to recalculate
	 * per frame. These are calculated during videobuf_prepare.
	 */
	dma_addr_t			buffer_addr;
	dma_addr_t			buffer_addr_u;
	dma_addr_t			buffer_addr_v;
	dma_addr_t			start_addr;
	dma_addr_t			start_addr_u;
	dma_addr_t			start_addr_v;
};

static struct tegra_camera_buffer *to_tegra_vb(struct vb2_buffer *vb)
{
	return container_of(vb, struct tegra_camera_buffer, vb);
}

struct tegra_camera_dev;

struct tegra_camera_clk {
	const char	*name;
	struct clk	*clk;
	u32		freq;
	int		use_devname;
};

struct tegra_camera_ops {
	int (*clks_init)(struct tegra_camera_dev *cam);
	void (*clks_deinit)(struct tegra_camera_dev *cam);
	void (*clks_enable)(struct tegra_camera_dev *cam);
	void (*clks_disable)(struct tegra_camera_dev *cam);

	void (*capture_clean)(struct tegra_camera_dev *vi2_cam);
	int (*capture_setup)(struct tegra_camera_dev *vi2_cam);
	int (*capture_start)(struct tegra_camera_dev *vi2_cam,
			     struct tegra_camera_buffer *buf);
	int (*capture_stop)(struct tegra_camera_dev *vi2_cam, int port);

	void (*incr_syncpts)(struct tegra_camera_dev *vi2_cam);
	void (*save_syncpts)(struct tegra_camera_dev *vi2_cam);

	void (*activate)(struct tegra_camera_dev *vi2_cam);
	void (*deactivate)(struct tegra_camera_dev *vi2_cam);
	int (*port_is_valid)(int port);
};

struct tegra_camera_dev {
	struct soc_camera_host		ici;
	struct platform_device		*ndev;
	struct nvhost_device_data	*ndata;

	struct regulator		*reg;
	const char			*regulator_name;

	struct tegra_camera_clk		*clks;
	int				num_clks;

	struct tegra_camera_ops		*ops;

	void __iomem			*reg_base;
	spinlock_t			videobuf_queue_lock;
	struct list_head		capture;
	struct vb2_buffer		*active;
	struct vb2_alloc_ctx		*alloc_ctx;
	enum v4l2_field			field;
	int				sequence_a;
	int				sequence_b;

	struct work_struct		work;
	struct mutex			work_mutex;

	u32				syncpt_csi_a;
	u32				syncpt_csi_b;
	u32				syncpt_vip;

	/* Debug */
	int				num_frames;
	int				enable_refcnt;

	/* Test Pattern Generator mode */
	int				tpg_mode;
};

#define TC_VI_REG_RD(dev, offset) readl(dev->reg_base + offset)
#define TC_VI_REG_WT(dev, offset, val) writel(val, dev->reg_base + offset)

int vi2_register(struct tegra_camera_dev *cam);
int vi_register(struct tegra_camera_dev *cam);

#endif
