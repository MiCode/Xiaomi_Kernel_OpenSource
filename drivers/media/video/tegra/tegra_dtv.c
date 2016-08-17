
/*
 * tegra_dtv.c - Tegra DTV interface driver
 *
 * Author: Adam Jiang <chaoj@nvidia.com>
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/pm_qos.h>
#include <media/tegra_dtv.h>

#include <linux/uaccess.h>
#include <mach/iomap.h>
#include <mach/dma.h>
#include <mach/dtv.h>

#define TEGRA_DTV_NAME "tegra_dtv"

/* default sw config */
#define DTV_BUF_SIZE_ORDER                PAGE_SHIFT
#define DTV_BUF_SIZE                      (1 << DTV_BUF_SIZE_ORDER)
#define DTV_MAX_BUF_SIZE                  (4 << DTV_BUF_SIZE_ORDER)
#define DTV_NUM_BUFS                      4
#define DTV_MAX_NUM_BUFS                  8

#define DTV_CPU_BOOST_MAX_MHZ             400
#define DTV_TS_MIN_BITRATE                416
#define DTV_TS_MAX_BITRATE                (416 * 13)

#define DTV_FIFO_ATN_LVL_LOW_GEAR         0
#define DTV_FIFO_ATN_LVL_SECOND_GEAR      1
#define DTV_FIFO_ATN_LVL_THIRD_GEAR       2
#define DTV_FIFO_ATN_LVL_TOP_GEAR         3

struct dtv_buffer {
	void			*data;
	dma_addr_t		 data_phy;
	struct completion	 comp;
	struct tegra_dma_req	 dma_req;
};

struct dtv_stream {
	struct mutex	mtx;

	bool			 xferring;	/* is DMA in progress */
	unsigned		 num_bufs;
	struct dtv_buffer	*bufs;
	int			 last_queued;

	int	fifo_atn_level;

	struct tegra_dma_channel	*dma_chan;
	bool				 stopped;
	struct completion		 stop_completion;
	spinlock_t			 dma_req_lock;
	size_t				 buf_size;

	struct work_struct	work;
	struct wake_lock	wake_lock;
	char			wake_lock_name[16];

	struct work_struct	cpu_boost_work;
	int			cpu_boost_flag;
};

struct tegra_dtv_context {
	struct tegra_dtv_hw_config config;
	struct tegra_dtv_profile   profile;
	struct clk                *clk;
	int                        clk_enabled;

	struct clk                *sclk;
	struct clk                *emc_clk;

	phys_addr_t                phys;
	void * __iomem base;
	unsigned long              dma_req_sel;

	struct dtv_stream          stream;
	/* debugfs */
	struct dentry             *d;
	/* for refer back */
	struct platform_device    *pdev;
	struct miscdevice          miscdev;

	struct pm_qos_request      min_cpufreq;
	struct pm_qos_request      cpudma_lat;
};

static inline struct tegra_dtv_context *to_ctx(struct dtv_stream *s)
{
	return container_of(s, struct tegra_dtv_context, stream);
}

/* access control */
static atomic_t tegra_dtv_instance_nr = ATOMIC_INIT(1);

static inline u32 tegra_dtv_readl(struct tegra_dtv_context *dtv,
					    unsigned long reg)
{
	BUG_ON(!dtv->clk_enabled);
	return readl(dtv->base + reg);
}

static inline void tegra_dtv_writel(struct tegra_dtv_context *dtv,
				    u32 val, unsigned long reg)
{
	BUG_ON(!dtv->clk_enabled);
	writel(val, dtv->base + reg);
}

/* process */
static inline void prevent_suspend(struct dtv_stream *s)
{
	pr_debug("%s called.\n", __func__);
	cancel_work_sync(&s->work);
	wake_lock(&s->wake_lock);
}

static void dtv_cpu_boost_worker(struct work_struct *work)
{
	struct dtv_stream *s = container_of(
		work, struct dtv_stream, cpu_boost_work);
	struct tegra_dtv_context *dtv_ctx = to_ctx(s);

	if (s->cpu_boost_flag) {
		pr_info("%s: Boost CPU frequency to %dMHz.",
			__func__, dtv_ctx->profile.cpuboost);
		pm_qos_update_request(&dtv_ctx->min_cpufreq,
				      dtv_ctx->profile.cpuboost * 1000);
	} else {
		pr_info("%s: Release CPU frequency boost.", __func__);
		pm_qos_update_request(&dtv_ctx->min_cpufreq,
				      PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
	}
}

static void tegra_dtv_worker(struct work_struct *w)
{
	struct dtv_stream *s = container_of(w, struct dtv_stream, work);
	pr_debug("%s called.\n", __func__);
	wake_unlock(&s->wake_lock);
}

static inline void dtv_boost_cpu(struct dtv_stream *s)
{
	schedule_work(&s->cpu_boost_work);
}

static inline void wakeup_suspend(struct dtv_stream *s)
{
	schedule_work(&s->work);
}

static inline bool wait_till_stopped(struct dtv_stream *s)
{
	int ret;

	pr_debug("%s: wait for completion\n", __func__);

	ret = wait_for_completion_timeout(
		&s->stop_completion, HZ);
	if (!ret)
		pr_err("%s: wait timed out", __func__);
	if (ret < 0)
		pr_err("%s: wait error %d\n", __func__, ret);

	wakeup_suspend(s);

	pr_debug("%s: done: %d\n", __func__, ret);

	return true;
}

/* dma transfer */
static inline bool are_xfers_pending(struct dtv_stream *s)
{
	int i;
	struct dtv_buffer *buf;

	pr_debug("%s called\n", __func__);

	for (i = 0; i < s->num_bufs; i++) {
		buf = &s->bufs[i];
		if (!completion_done(&buf->comp))
			return true;
	}

	return false;
}

static void tegra_dtv_rx_dma_complete(struct tegra_dma_req *req)
{
	unsigned long flags;
	unsigned req_num;
	struct dtv_stream *s = req->dev;
	struct dtv_buffer *buf = container_of(req, struct dtv_buffer,
					      dma_req);

	spin_lock_irqsave(&s->dma_req_lock, flags);

	pr_debug("%s called.\n", __func__);

	req_num = buf - s->bufs;
	pr_debug("%s: complete buffer %d size %d bytes\n",
		 __func__, req_num, req->bytes_transferred);
	BUG_ON(req_num >= s->num_bufs);

	complete(&buf->comp);

	if (req->bytes_transferred > s->buf_size
	    && are_xfers_pending(s)) {
		dtv_boost_cpu(s);
		pr_warn("%s: DMA buffer overlapped", __func__);
	}

	spin_unlock_irqrestore(&s->dma_req_lock, flags);
}

/* hw */
static inline void _dtv_enable_protocol(struct tegra_dtv_context *dtv_ctx)
{
	u32 val;

	val = tegra_dtv_readl(dtv_ctx, DTV_MODE);
	val &= ~0x01;
	val |= DTV_MODE_PRTL_ENABLE;
	tegra_dtv_writel(dtv_ctx, val, DTV_MODE);
}

static inline void _dtv_disable_protocol(struct tegra_dtv_context *dtv_ctx)
{
	u32 val;

	val = tegra_dtv_readl(dtv_ctx, DTV_MODE);
	val &= ~DTV_MODE_PRTL_ENABLE;
	tegra_dtv_writel(dtv_ctx, val, DTV_MODE);
}

static inline u32 _dtv_get_status(struct tegra_dtv_context *dtv_ctx)
{
	return tegra_dtv_readl(dtv_ctx, DTV_STATUS);
}

static inline void _dtv_set_attn_level(struct tegra_dtv_context *dtv_ctx)
{
	/* TODO: consider have this set to corresponding transfer request */
	u32 val;

	val = tegra_dtv_readl(dtv_ctx, DTV_CTRL);
	val &= ~DTV_CTRL_FIFO_ATTN_LEVEL_MASK;
	val |= DTV_CTRL_FIFO_ATTN_FOUR_WORD;
	tegra_dtv_writel(dtv_ctx, val, DTV_CTRL);
}

/* ioctl */
static inline void _dtv_set_hw_params(struct tegra_dtv_context *dtv_ctx)
{
	u32 val = 0;
	u32 reg = 0;
	struct tegra_dtv_hw_config *cfg = &dtv_ctx->config;

	/* program DTV_MODE */
	val = (cfg->byte_swz_enabled << DTV_MODE_BYTE_SWIZZLE_SHIFT) |
		(cfg->bit_swz_enabled << DTV_MODE_BIT_SWIZZLE_SHIFT) |
		(cfg->clk_edge << DTV_MODE_CLK_EDGE_SHIFT) |
		(cfg->protocol_sel << DTV_MODE_PRTL_SEL_SHIFT) |
		(cfg->clk_mode << DTV_MODE_CLK_MODE_SHIFT);
	reg = tegra_dtv_readl(dtv_ctx, DTV_MODE);
	reg &= ~(DTV_MODE_BYTE_SWIZZLE_MASK |
		 DTV_MODE_BIT_SWIZZLE |
		 DTV_MODE_CLK_EDGE_MASK |
		 DTV_MODE_PRTL_SEL_MASK |
		 DTV_MODE_CLK_MODE_MASK);
	reg |= val;
	tegra_dtv_writel(dtv_ctx, reg, DTV_MODE);

	/* program DTV_CTRL */
	reg = 0;
	val = (cfg->fec_size << DTV_CTRL_FEC_SIZE_SHIFT) |
		(cfg->body_size << DTV_CTRL_BODY_SIZE_SHIFT) |
		(cfg->body_valid_sel << DTV_CTRL_BODY_VALID_SEL_SHIFT) |
		(cfg->start_sel << DTV_CTRL_START_SEL_SHIFT) |
		(cfg->err_pol << DTV_CTRL_ERROR_POLARITY_SHIFT) |
		(cfg->psync_pol << DTV_CTRL_PSYNC_POLARITY_SHIFT) |
		(cfg->valid_pol << DTV_CTRL_VALID_POLARITY_SHIFT);
	reg = tegra_dtv_readl(dtv_ctx, DTV_CTRL);
	reg &= ~(DTV_CTRL_FEC_SIZE_MASK |
		 DTV_CTRL_BODY_SIZE_MASK |
		 DTV_CTRL_BODY_VALID_SEL_MASK |
		 DTV_CTRL_START_SEL_MASK |
		 DTV_CTRL_ERROR_POLARITY_MASK |
		 DTV_CTRL_PSYNC_POLARITY_MASK |
		 DTV_CTRL_VALID_POLARITY_MASK);
	reg |= val;
	tegra_dtv_writel(dtv_ctx, reg, DTV_CTRL);
}

#define DTV_GET_REG_VAL(x, reg, seg)				\
	((x & reg##_##seg##_MASK) >> reg##_##seg##_SHIFT)

static inline void _dtv_get_hw_params(struct tegra_dtv_context *dtv_ctx,
				      struct tegra_dtv_hw_config *cfg)
{
	u32 reg;

	reg = tegra_dtv_readl(dtv_ctx, DTV_MODE);
	cfg->clk_edge = DTV_GET_REG_VAL(reg, DTV_MODE, CLK_EDGE);
	cfg->protocol_sel = DTV_GET_REG_VAL(reg, DTV_MODE, PRTL_SEL);
	cfg->clk_mode = DTV_GET_REG_VAL(reg, DTV_MODE, CLK_MODE);

	reg = tegra_dtv_readl(dtv_ctx, DTV_CTRL);
	cfg->fec_size = DTV_GET_REG_VAL(reg, DTV_CTRL, FEC_SIZE);
	cfg->body_size = DTV_GET_REG_VAL(reg, DTV_CTRL, BODY_SIZE);
	cfg->body_valid_sel = DTV_GET_REG_VAL(reg, DTV_CTRL, BODY_VALID_SEL);
	cfg->start_sel = DTV_GET_REG_VAL(reg, DTV_CTRL, START_SEL);
	cfg->err_pol = DTV_GET_REG_VAL(reg, DTV_CTRL, ERROR_POLARITY);
	cfg->psync_pol = DTV_GET_REG_VAL(reg, DTV_CTRL, PSYNC_POLARITY);
	cfg->valid_pol = DTV_GET_REG_VAL(reg, DTV_CTRL, VALID_POLARITY);
}

static void dtv_debugfs_exit(struct tegra_dtv_context *dtv_ctx);
static int reconfig_stream(struct tegra_dtv_context *dtv_ctx,
			   struct tegra_dtv_profile *new);
static void reconfig_pm_qos(struct tegra_dtv_context *dtv_ctx,
			    struct tegra_dtv_profile *new);

static int _dtv_set_profile(struct tegra_dtv_context *dtv_ctx,
			    struct tegra_dtv_profile *new)
{
	int ret = 0;

	if (dtv_ctx->profile.bufsize != new->bufsize ||
	    dtv_ctx->profile.bufnum != new->bufnum) {
		ret = reconfig_stream(dtv_ctx, new);
		if (ret < 0) {
			/* unregister misc device if failed */
			dtv_debugfs_exit(dtv_ctx);
			misc_deregister(&dtv_ctx->miscdev);
		}
		dtv_ctx->profile.bufsize = new->bufsize;
		dtv_ctx->profile.bufnum = new->bufnum;
	}

	if (dtv_ctx->profile.cpuboost != new->cpuboost ||
	    dtv_ctx->profile.bitrate != new->bitrate) {
		reconfig_pm_qos(dtv_ctx, new);
	}

	return ret;
}

/* must call with stream->dma_req_lock held. */
static int stop_xfer_unsafe(struct dtv_stream *s)
{
	int spin = 0;
	struct tegra_dtv_context *dtv_ctx = to_ctx(s);

	pr_debug("%s called\n", __func__);

	tegra_dma_cancel(s->dma_chan);

	/* stop CPU boost */
	s->cpu_boost_flag = 0;
	schedule_work(&s->cpu_boost_work);

	/* release restriction on CPU-DMA latency */
	pm_qos_update_request(&dtv_ctx->cpudma_lat,
			      PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE);

	_dtv_disable_protocol(dtv_ctx);
	while ((_dtv_get_status(dtv_ctx) & DTV_STATUS_RXF_FULL) &&
	       spin < 100) {
		udelay(10);
		if (spin++ > 50)
			pr_info("%s : spin %d\n", __func__, spin);
	}
	if (spin == 100)
		pr_warn("%s : spinny\n", __func__);

	return 0;
}

/* must call with stream->mtx held */
static void __force_xfer_stop(struct dtv_stream *s)
{
	int i;
	struct dtv_buffer *buf;

	pr_debug("%s called.\n", __func__);

	if (!s->stopped) {
		s->stopped = true;
		if (are_xfers_pending(s))
			wait_till_stopped(s);
		for (i = 0; i < s->num_bufs; i++) {
			buf = &s->bufs[i];
			init_completion(&buf->comp);
			complete(&buf->comp);
		}
	}

	tegra_dma_cancel(s->dma_chan);
	s->xferring = false;

	pr_debug("%s: done\n", __func__);
}

static long tegra_dtv_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct tegra_dtv_context *dtv_ctx;
	struct dtv_stream *s;

	dtv_ctx = (struct tegra_dtv_context *) file->private_data;
	s = &dtv_ctx->stream;

	/* process may sleep on this */
	mutex_lock(&s->mtx);

	switch (cmd) {
	case TEGRA_DTV_IOCTL_START:
		pr_debug("%s: run serial ts handling.\n", __func__);
		s->stopped = false;
		break;
	case TEGRA_DTV_IOCTL_STOP:
		pr_debug("%s: stop serial ts handling.\n", __func__);
		if (s->xferring) {
			stop_xfer_unsafe(s);
			complete(&s->stop_completion);
			__force_xfer_stop(s);
			s->stopped = true;
		}
		break;
	case TEGRA_DTV_IOCTL_SET_HW_CONFIG:
	{
		struct tegra_dtv_hw_config cfg;

		if (s->xferring) {
			pr_err("%s: tranfering is in progress.\n", __func__);
			ret = -EBUSY;
			break;
		}

		if (copy_from_user(&cfg, (const void __user  *) arg,
				   sizeof(cfg))) {
			ret = -EFAULT;
			break;
		}

		dtv_ctx->config = cfg;
		_dtv_set_hw_params(dtv_ctx);
		break;
	}
	case TEGRA_DTV_IOCTL_GET_HW_CONFIG:
	{
		struct tegra_dtv_hw_config cfg;

		_dtv_get_hw_params(dtv_ctx, &cfg);

		if (copy_to_user((void __user  *)arg, &cfg,
				 sizeof(cfg)))
			ret = -EFAULT;
		break;
	}
	case TEGRA_DTV_IOCTL_GET_PROFILE:
	{
		if (copy_to_user((void __user *) arg, &dtv_ctx->profile,
				 sizeof(struct tegra_dtv_profile)))
			ret = -EFAULT;
		break;
	}
	case TEGRA_DTV_IOCTL_SET_PROFILE:
	{
		struct tegra_dtv_profile profile;

		if (s->xferring) {
			pr_err("%s: tranfering is in progress.\n", __func__);
			ret = -EBUSY;
			break;
		}

		if (copy_from_user(&profile, (const void __user *) arg,
				   sizeof(profile))) {
			ret = -EFAULT;
			break;
		}

		ret = _dtv_set_profile(dtv_ctx, &profile);
		if (ret) {
			pr_err("%s: reconfiguration failed. DTV is down\n",
			       __func__);
			ret = -ENODEV;
			break;
		}
		break;
	}
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&s->mtx);

	return ret;
}

/* must call with stream->dma_req_lock held. */
static int start_xfer_unsafe(struct dtv_stream *s, size_t size)
{
	int i;
	u32 reg;
	struct tegra_dtv_context *dtv_ctx = to_ctx(s);
	struct dtv_buffer *buf;

	BUG_ON(are_xfers_pending(s));

	pr_debug("%s called.\n", __func__);

	for (i = 0; i < s->num_bufs; i++) {
		buf = &s->bufs[i];
		init_completion(&buf->comp);
		buf->dma_req.dest_addr = buf->data_phy;
		buf->dma_req.size = size;
		tegra_dma_enqueue_req(s->dma_chan, &buf->dma_req);
	}

	/* set boost cpu enabled */
	s->cpu_boost_flag = 1;
	dtv_boost_cpu(s);

	/* set the bottom line of cpu-dma latency
	 *
	 * The value of this parameter is about cpu snooping on DMAed
	 * memory. It means that CPU should keep snooping to mark
	 * corresponding entries invalid in cache if any entry is
	 * associated to DMAed memory.
	 *
	 * DTV device really doesn't like DMA snoopying takes longer
	 * than
	 *
	 *  (bufsize / bitrate of signal) second
	 *
	 * . For example, if we have 4096 bytes available in each buffer
	 * and capture ISDB-T full-seg data, latency should never be
	 * longer than
	 *
	 *  4K bytes / (416 * 13)kbps ~= 6usec
	 *
	 * This will push a lot of pressure on DMA controller and CPU,
	 * especially on Tegra 3 SoC. Please set buffer size to 8192
	 * bytes at lease in this case.
	 */
	pm_qos_update_request(&dtv_ctx->cpudma_lat,
			      s->buf_size / dtv_ctx->profile.bitrate);

	s->last_queued = s->num_bufs - 1;

	/* too late ? */
	_dtv_set_attn_level(dtv_ctx);
	_dtv_enable_protocol(dtv_ctx);

	reg = tegra_dtv_readl(dtv_ctx, DTV_MODE);
	pr_debug("DTV_MODE = 0x%08x\n", reg);

	return 0;
}

static int try_start_fill_buf(struct dtv_stream *s, size_t size)
{
	int ret = 0;
	unsigned long flags;

	pr_debug("%s called\n", __func__);

	prevent_suspend(s);

	spin_lock_irqsave(&s->dma_req_lock, flags);
	if (!s->stopped && !are_xfers_pending(s)) {
		ret = start_xfer_unsafe(s, size);
		if (ret) {
			pr_err("%s: start tranfer failed.\n", __func__);
			/* let process not wait stupid */
			wakeup_suspend(s);
		}
	}
	spin_unlock_irqrestore(&s->dma_req_lock, flags);

	return ret;
}

static ssize_t tegra_dtv_read(struct file *file, char __user *buf,
			      size_t size, loff_t *off)
{
	ssize_t ret;
	ssize_t xfer_size = 0;
	int     buf_no;
	struct tegra_dma_req *req;
	struct tegra_dtv_context *dtv_ctx;
	struct dtv_buffer *dtv_buf;

	dtv_ctx = (struct tegra_dtv_context *) file->private_data;

	mutex_lock(&dtv_ctx->stream.mtx);

	if (!IS_ALIGNED(size, 4) || size < 4 ||
	    size != dtv_ctx->stream.buf_size) {
		pr_err("%s: invalid user size %d\n", __func__, size);
		ret = -EINVAL;
		mutex_unlock(&dtv_ctx->stream.mtx);
		return ret;
	}

	pr_debug("%s: read %d bytes.\n", __func__, size);

	if (dtv_ctx->stream.stopped) {
		pr_debug("%s: tegra dtv transferring is stopped.\n",
			 __func__);
		ret = 0;
		mutex_unlock(&dtv_ctx->stream.mtx);
		return ret;
	}

	/* start dma transfer it was not started yet */
	if (unlikely(!dtv_ctx->stream.xferring)) {
		ret = try_start_fill_buf(&dtv_ctx->stream, size);
		if (ret < 0 && ret != -EALREADY) {
			pr_err("%s: could not start recording.\n", __func__);
			mutex_unlock(&dtv_ctx->stream.mtx);
			return ret;
		}
		dtv_ctx->stream.xferring = true;
	}

	buf_no = (dtv_ctx->stream.last_queued + 1) % dtv_ctx->stream.num_bufs;
	pr_debug("%s: buf_no = %d\n", __func__, buf_no);
	dtv_buf = &dtv_ctx->stream.bufs[buf_no];

	/* Wait for the buffers to be filled up. The maximum timeout
	 * value should be caculated dynamically based on
	 * buf_size(dtv_ctx->stream).buf_size. For isdb-t 1seg signal,
	 * it bit rate is 300 - 456 kpbs, if buf_size = 4096 bytes, then
	 * to fill up one buffer takes ~77ms.
	 */
	ret = wait_for_completion_interruptible_timeout(&dtv_buf->comp, HZ);
	if (!ret) {
		pr_err("%s: timeout", __func__);
		ret = -ETIMEDOUT;
		mutex_unlock(&dtv_ctx->stream.mtx);
		return ret;
	} else if (ret < 0) {
		pr_err("%s: wait error %d", __func__, ret);
		mutex_unlock(&dtv_ctx->stream.mtx);
		return ret;
	}

	req = &dtv_buf->dma_req;

	/* xfer cannot exceed buffer size */
	xfer_size = size > req->size ? req->size : size;
	req->size = size;

	dma_sync_single_for_cpu(NULL,
				dtv_buf->dma_req.dest_addr,
				dtv_buf->dma_req.size,
				DMA_FROM_DEVICE);
	ret = copy_to_user(buf, dtv_buf->data, xfer_size);
	if (ret) {
		ret = -EFAULT;
		mutex_unlock(&dtv_ctx->stream.mtx);
		return ret;
	}

	/* not stopped, reinitial stop */
	init_completion(&dtv_ctx->stream.stop_completion);

	dtv_ctx->stream.last_queued = buf_no;

	/* refill copied buffer */
	ret = tegra_dma_enqueue_req(dtv_ctx->stream.dma_chan, req);
	BUG_ON(ret);

	ret = xfer_size;
	*off += xfer_size;

	mutex_unlock(&dtv_ctx->stream.mtx);

	pr_debug("%s : done with ret = %d\n", __func__, ret);

	return ret;
}

static int tegra_dtv_open(struct inode *inode, struct file *file)
{
	int i;
	struct dtv_buffer *buf;
	struct miscdevice *miscdev = file->private_data;
	struct tegra_dtv_context *dtv_ctx =
		container_of(miscdev, struct tegra_dtv_context, miscdev);
	struct platform_device *pdev;
	file->private_data = dtv_ctx;

	/* hold system bus clock and EMC clock to ensure DTV driver has
	 * enought bandwidth.
	 *
	 * The frequencies for these clocks should be set up on platform
	 * bias in board files.
	 */
	pdev = dtv_ctx->pdev;
	if (clk_enable(dtv_ctx->sclk) < 0) {
		dev_err(&pdev->dev, "cannot enable SBus clock.\n");
		return -ENOSYS;
	}

	if (clk_enable(dtv_ctx->emc_clk) < 0) {
		dev_err(&pdev->dev, "cannot enable EMC clock.\n");
		clk_disable(dtv_ctx->sclk);
		return -ENOSYS;
	}

	dtv_ctx = (struct tegra_dtv_context *) file->private_data;

	pr_debug("%s called\n", __func__);

	/* can be opened once */
	if (!atomic_dec_and_test(&tegra_dtv_instance_nr)) {
		atomic_inc(&tegra_dtv_instance_nr);
		pr_err("tegra_dtv device can only be opened once.\n");
		return -EBUSY;
	}

	mutex_lock(&dtv_ctx->stream.mtx);

	dtv_ctx->stream.stopped = false;

	/* cleanup completion */
	for (i = 0; i < dtv_ctx->stream.num_bufs; i++) {
		buf = &dtv_ctx->stream.bufs[i];
		init_completion(&buf->comp);
		/* complete all */
		complete(&buf->comp);
	}

	mutex_unlock(&dtv_ctx->stream.mtx);

	return 0;
}

static int tegra_dtv_release(struct inode *inode, struct file *file)
{
	struct tegra_dtv_context *dtv_ctx =
		(struct tegra_dtv_context *) file->private_data;

	pr_debug("%s called\n", __func__);

	atomic_inc(&tegra_dtv_instance_nr);

	mutex_lock(&dtv_ctx->stream.mtx);
	if (dtv_ctx->stream.xferring) {
		stop_xfer_unsafe(&dtv_ctx->stream);
		/* clean up stop condition */
		complete(&dtv_ctx->stream.stop_completion);
		__force_xfer_stop(&dtv_ctx->stream);
	}

	clk_disable(dtv_ctx->sclk);
	clk_disable(dtv_ctx->emc_clk);

	/* wakeup any pending process */
	wakeup_suspend(&dtv_ctx->stream);
	mutex_unlock(&dtv_ctx->stream.mtx);

	pr_debug("%s : done\n", __func__);

	return 0;
}

static const struct file_operations tegra_dtv_fops = {
	.owner = THIS_MODULE,
	.open = tegra_dtv_open,
	.read = tegra_dtv_read,
	.unlocked_ioctl = tegra_dtv_ioctl,
	.release = tegra_dtv_release,
};

#ifdef CONFIG_DEBUG_FS
static int dtv_reg_show(struct seq_file *s, void *unused)
{
	struct tegra_dtv_context *dtv_ctx = s->private;

	seq_printf(s, "tegra_dtv register list\n");
	seq_printf(s, "-------------------------------\n");
	seq_printf(s, "DTV_SPI_CONTROL_0: 0x%08x\n",
		   tegra_dtv_readl(dtv_ctx, DTV_SPI_CONTROL));
	seq_printf(s, "DTV_MODE_0:        0x%08x\n",
		   tegra_dtv_readl(dtv_ctx, DTV_MODE));
	seq_printf(s, "DTV_CONTROL:       0x%08x\n",
		   tegra_dtv_readl(dtv_ctx, DTV_CTRL));
	seq_printf(s, "DTV_STATUS:        0x%08x\n",
		   tegra_dtv_readl(dtv_ctx, DTV_STATUS));

	return 0;

}

static int dtv_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dtv_reg_show, inode->i_private);
}

static const struct file_operations dtv_debugfs_fops = {
	.open = dtv_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dtv_debugfs_init(struct tegra_dtv_context *dtv_ctx)
{
	struct dentry *d;

	d = debugfs_create_file("tegra_dtv", S_IRUGO, NULL, dtv_ctx,
				&dtv_debugfs_fops);
	if (!d)
		return -ENOMEM;

	dtv_ctx->d = d;

	return 0;
}

static void dtv_debugfs_exit(struct tegra_dtv_context *dtv_ctx)
{
	debugfs_remove(dtv_ctx->d);
}
#else
static int dtv_debugfs_init(struct tegra_dtv_context *dtv_ctx) { return 0; }
static void dtv_debugfs_exit(struct tegra_dtv_context *dtv_ctx) {};
#endif

static void setup_dma_rx_request(struct tegra_dma_req *req,
			    struct dtv_stream *s)
{
	struct tegra_dtv_context *dtv_ctx;

	pr_debug("%s before to_ctx\n", __func__);
	dtv_ctx = to_ctx(s);

	pr_debug("%s called\n", __func__);

	memset(req, 0, sizeof(*req));

	req->complete = tegra_dtv_rx_dma_complete;
	req->dev = s;
	req->to_memory = true;
	req->req_sel = TEGRA_DMA_REQ_SEL_DTV;

	req->source_addr = dtv_ctx->phys + DTV_RX_FIFO;
	req->source_wrap = 4;
	req->source_bus_width = 32;
	req->fixed_burst_size = 1;

	req->dest_wrap = 0;
	req->dest_bus_width = 32;
}

static void tear_down_dma(struct tegra_dtv_context *dtv_ctx);

static int setup_dma(struct tegra_dtv_context *dtv_ctx)
{
	int ret = 0;
	int i;
	struct dtv_buffer *buf;
	struct dtv_stream *stream = &dtv_ctx->stream;
	struct device *dev = &dtv_ctx->pdev->dev;

	pr_debug("%s called\n", __func__);

	for (i = 0; i < stream->num_bufs; i++) {
		buf = &stream->bufs[i];
		buf->data_phy = dma_map_single(
			dev, buf->data,
			stream->buf_size, DMA_FROM_DEVICE);
		BUG_ON(!buf->data_phy);
		setup_dma_rx_request(&buf->dma_req, stream);
		buf->dma_req.dest_addr = buf->data_phy;
	}
	dtv_ctx->stream.dma_chan = tegra_dma_allocate_channel(
		TEGRA_DMA_MODE_CONTINUOUS_DOUBLE,
		"tegra_dtv_rx", dtv_ctx->dma_req_sel);
	if (!dtv_ctx->stream.dma_chan) {
		pr_err("%s : cannot allocate input DMA channel: %ld\n",
		       __func__, PTR_ERR(dtv_ctx->stream.dma_chan));
		ret = -ENODEV;
		/* release */
		tear_down_dma(dtv_ctx);

		return ret;
	}

	return ret;
}

static void tear_down_dma(struct tegra_dtv_context *dtv_ctx)
{
	int i;
	struct dtv_buffer *buf;
	struct dtv_stream *stream = &dtv_ctx->stream;
	struct device *dev = &dtv_ctx->pdev->dev;

	pr_debug("%s called\n", __func__);

	for (i = 0; i < dtv_ctx->stream.num_bufs; i++) {
		buf = &stream->bufs[i];
		dma_unmap_single(dev,
				 buf->data_phy,
				 stream->buf_size,
				 DMA_FROM_DEVICE);
		buf->data_phy = 0;
	}
	tegra_dma_free_channel(stream->dma_chan);
	dtv_ctx->stream.dma_chan = 0;
}

static void free_dtv_buffer(struct dtv_buffer *buf)
{
	kfree(buf->data);
}

static int alloc_dtv_buffer(struct dtv_buffer *buf, struct dtv_stream *s)
{
	buf->data = kmalloc(s->buf_size, GFP_KERNEL | GFP_DMA);
	if (!buf->data)
		return -ENOMEM;

	init_completion(&buf->comp);
	/* complete all at this moment */
	complete(&buf->comp);
	buf->data_phy = 0;

	return 0;
}

static void deinit_stream_buffers(struct dtv_stream *s)
{
	int i;

	for (i = 0; i < s->num_bufs; ++i)
		free_dtv_buffer(&s->bufs[i]);
}

static int init_stream_buffers(struct dtv_stream *s)
{
	int ret;
	int i, j;
	struct dtv_buffer *buf;

	for (i = 0; i < s->num_bufs; i++) {
		buf = &s->bufs[i];
		ret = alloc_dtv_buffer(buf, s);
		if (ret < 0) {
			pr_err("%s: failed to allocate buffer.", __func__);
			for  (j = i - 1; j >= 0; j--)
				free_dtv_buffer(&s->bufs[j]);
			return ret;
		}
	}
	return 0;
}

static void destroy_stream(struct dtv_stream *stream)
{
	deinit_stream_buffers(stream);
	kfree(stream->bufs);
}

static int setup_stream(struct dtv_stream *stream,
			struct tegra_dtv_profile *profile)
{
	int ret = 0;

	pr_debug("%s called\n", __func__);

	stream->xferring = false;
	mutex_init(&stream->mtx);
	init_completion(&stream->stop_completion);
	spin_lock_init(&stream->dma_req_lock);
	stream->dma_chan = NULL;
	stream->fifo_atn_level = DTV_FIFO_ATN_LVL_TOP_GEAR;

	stream->buf_size = profile->bufsize > DTV_MAX_BUF_SIZE ?
		DTV_MAX_BUF_SIZE : profile->bufsize;
	stream->num_bufs = profile->bufnum > DTV_MAX_NUM_BUFS ?
		DTV_MAX_NUM_BUFS : profile->bufnum;

	pr_info("%s: bufsize = %d, bufnum = %d", __func__,
		stream->buf_size, stream->num_bufs);

	/* init refs to buffers */
	stream->bufs = kmalloc(stream->num_bufs * sizeof(struct dtv_buffer),
			       GFP_KERNEL);
	if (!stream->bufs)
		return -ENOMEM;

	/* init each buffer */
	ret = init_stream_buffers(stream);
	if (ret < 0) {
		kfree(stream->bufs);
		return ret;
	}

	stream->last_queued = 0;

	INIT_WORK(&stream->work, tegra_dtv_worker);
	wake_lock_init(&stream->wake_lock, WAKE_LOCK_SUSPEND, "tegra_dtv");

	INIT_WORK(&stream->cpu_boost_work, dtv_cpu_boost_worker);

	return ret;
}

static int reconfig_stream(struct tegra_dtv_context *dtv_ctx,
			   struct tegra_dtv_profile *new)
{
	int ret = 0;

	destroy_stream(&dtv_ctx->stream);
	tear_down_dma(dtv_ctx);

	ret = setup_stream(&dtv_ctx->stream, new);
	if (ret < 0)
		goto fail_setup_stream;

	ret = setup_dma(dtv_ctx);
	if (ret < 0)
		goto fail_setup_dma;

	return ret;

fail_setup_stream:
	destroy_stream(&dtv_ctx->stream);
fail_setup_dma:
	tear_down_dma(dtv_ctx);

	return ret;
}

static inline void reconfig_pm_qos(struct tegra_dtv_context *dtv_ctx,
			   struct tegra_dtv_profile *new)
{
	if (new->cpuboost > DTV_CPU_BOOST_MAX_MHZ)
		dtv_ctx->profile.cpuboost = DTV_CPU_BOOST_MAX_MHZ;
	else
		dtv_ctx->profile.cpuboost = new->cpuboost;

	if (new->bitrate < DTV_TS_MIN_BITRATE)
		dtv_ctx->profile.bitrate = DTV_TS_MIN_BITRATE;
	else if (new->bitrate > DTV_TS_MAX_BITRATE)
		dtv_ctx->profile.bitrate = DTV_TS_MAX_BITRATE;
	else
		dtv_ctx->profile.bitrate = new->bitrate;

	pr_info("%s: cpuboost = %d, bitrate = %d", __func__,
		dtv_ctx->profile.cpuboost, dtv_ctx->profile.bitrate);
}

static int tegra_dtv_probe(struct platform_device *pdev)
{
	int ret;
	struct tegra_dtv_context *dtv_ctx;
	struct tegra_dtv_platform_data *pdata;
	struct clk *clk;
	struct resource *res;

	pr_info("%s: probing dtv.\n", __func__);

	dtv_ctx = devm_kzalloc(&pdev->dev, sizeof(struct tegra_dtv_context),
			       GFP_KERNEL);
	if (!dtv_ctx) {
		pr_err("%s: Failed to allocate memory for dtv context.\n",
		       __func__);
		ret = -ENOMEM;
		return ret;
	}
	platform_set_drvdata(pdev, dtv_ctx);

	pdata = pdev->dev.platform_data;

	/* for refer back */
	dtv_ctx->pdev = pdev;

	/* enable clk for dtv */
	clk = clk_get(&pdev->dev, NULL);
	if (!clk) {
		dev_err(&pdev->dev, "cannot get clock for tegra_dtv.\n");
		ret = -EIO;
		goto fail_no_clk;
	}
	dtv_ctx->clk = clk;
	ret = clk_prepare_enable(clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot enable clk for tegra_dtv.\n");
		goto fail_clk_enable;
	}
	dtv_ctx->clk_enabled = 1;

	/* get shared system bus clock and emc clock */
	dtv_ctx->sclk = clk_get(&pdev->dev, "sclk");
	if (IS_ERR_OR_NULL(dtv_ctx->sclk)) {
		dev_err(&pdev->dev,
			"cannot get SBus clock for tegra_dtv.\n");
		ret = -EIO;
		goto fail_no_clk;
	}
	dtv_ctx->emc_clk = clk_get(&pdev->dev, "emc");
	if (IS_ERR_OR_NULL(dtv_ctx->emc_clk)) {
		dev_err(&pdev->dev,
			"cannot get EMC clock for tegra_dtv.\n");
		ret = -EIO;
		goto fail_no_clk;
	}

	/* add PM QoS request but leave it as default value */
	pm_qos_add_request(&dtv_ctx->min_cpufreq, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&dtv_ctx->cpudma_lat, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE);

	/* get resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(!res)) {
		pr_err("%s: Failed to get resource for dtv.\n",
		       __func__);
		ret = -ENODEV;
		goto fail_no_res;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), dev_name(&pdev->dev))) {
		ret = -EBUSY;
		return ret;
	}
	dtv_ctx->phys = res->start;
	dtv_ctx->base = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
	if (!dtv_ctx->base) {
		dev_err(&pdev->dev, "cannot ioremap iomem.\n");
		ret = -ENOMEM;
		return ret;
	}

	dtv_ctx->profile.bufsize = DTV_BUF_SIZE;
	dtv_ctx->profile.bufnum = DTV_NUM_BUFS;
	dtv_ctx->profile.cpuboost = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;
	dtv_ctx->profile.bitrate = DTV_TS_MIN_BITRATE;

	ret = setup_stream(&dtv_ctx->stream, &dtv_ctx->profile);
	if (ret < 0)
		goto fail_setup_stream;

	ret = setup_dma(dtv_ctx);
	if (ret < 0)
		goto fail_setup_dma;

	/* register as a misc device */
	dtv_ctx->miscdev.minor = MISC_DYNAMIC_MINOR;
	dtv_ctx->miscdev.name = TEGRA_DTV_NAME;
	dtv_ctx->miscdev.fops = &tegra_dtv_fops;
	ret = misc_register(&dtv_ctx->miscdev);
	if (ret) {
		pr_err("%s: Unable to register misc device.\n",
		       __func__);
		ret = -ENODEV;
		goto fail_misc_reg;
	}

	ret = dtv_debugfs_init(dtv_ctx);
	if (ret) {
		pr_err("%s: Unable to register debugfs entry.\n",
		       __func__);
		goto fail_debugfs_reg;
	}

	return 0;

fail_debugfs_reg:
	dtv_debugfs_exit(dtv_ctx);
fail_misc_reg:
	misc_deregister(&dtv_ctx->miscdev);
fail_setup_stream:
	destroy_stream(&dtv_ctx->stream);
fail_setup_dma:
	tear_down_dma(dtv_ctx);
fail_no_res:
fail_clk_enable:
fail_no_clk:
	if (clk)
		clk_put(clk);

	return ret;
}

static int __devexit tegra_dtv_remove(struct platform_device *pdev)
{
	struct tegra_dtv_context *dtv_ctx;

	pr_info("%s: remove dtv.\n", __func__);

	dtv_ctx = platform_get_drvdata(pdev);

	dtv_debugfs_exit(dtv_ctx);
	tear_down_dma(dtv_ctx);
	destroy_stream(&dtv_ctx->stream);

	clk_put(dtv_ctx->clk);

	misc_deregister(&dtv_ctx->miscdev);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_dtv_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_dtv_context *dtv_ctx;

	pr_info("%s: suspend dtv.\n", __func__);

	dtv_ctx = platform_get_drvdata(pdev);

	/* stop xferring */
	mutex_lock(&dtv_ctx->stream.mtx);
	if (dtv_ctx->stream.xferring) {
		stop_xfer_unsafe(&dtv_ctx->stream);
		/* clean up stop condition */
		complete(&dtv_ctx->stream.stop_completion);
		__force_xfer_stop(&dtv_ctx->stream);
	}
	/* wakeup any pending process */
	wakeup_suspend(&dtv_ctx->stream);
	mutex_unlock(&dtv_ctx->stream.mtx);

	clk_disable_unprepare(dtv_ctx->clk);

	return 0;
}

static int tegra_dtv_resume(struct platform_device *pdev)
{
	struct tegra_dtv_context *dtv_ctx;

	pr_info("%s: resume dtv.\n", __func__);

	dtv_ctx = platform_get_drvdata(pdev);
	clk_prepare_enable(dtv_ctx->clk);

	return 0;
}
#endif /* CONFIG_PM */

static struct platform_driver tegra_dtv_driver = {
	.driver = {
		.name = TEGRA_DTV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra_dtv_probe,
	.remove = __devexit_p(tegra_dtv_remove),
#ifdef CONFIG_PM
	.suspend = tegra_dtv_suspend,
	.resume = tegra_dtv_resume,
#endif
};

static int __init tegra_dtv_init(void)
{
	return platform_driver_register(&tegra_dtv_driver);
}

static void __exit tegra_dtv_exit(void)
{
	platform_driver_unregister(&tegra_dtv_driver);
}

module_init(tegra_dtv_init);
module_exit(tegra_dtv_exit);

MODULE_AUTHOR("Adam Jiang <chaoj@nvidia.com>");
MODULE_DESCRIPTION("Tegra DTV interface driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" TEGRA_DTV_NAME);
