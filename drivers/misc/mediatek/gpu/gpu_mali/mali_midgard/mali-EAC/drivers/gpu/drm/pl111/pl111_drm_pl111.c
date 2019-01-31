/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_pl111.c
 * PL111 specific functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "pl111_drm.h"

/* This can't be called from IRQ context, due to clk_get() and board->enable */
static int clcd_enable(struct drm_framebuffer *fb)
{
	__u32 cntl;
	struct clcd_board *board;

	pr_debug("DRM %s\n", __func__);

	clk_prepare_enable(priv.clk);

	/* Enable and Power Up */
	cntl = CNTL_LCDEN | CNTL_LCDTFT | CNTL_LCDPWR | CNTL_LCDVCOMP(1);
	DRM_DEBUG_KMS("fb->bits_per_pixel = %d\n", fb->bits_per_pixel);
	if (fb->bits_per_pixel == 16)
		cntl |= CNTL_LCDBPP16_565;
	else if (fb->bits_per_pixel == 32 && fb->depth == 24)
		cntl |= CNTL_LCDBPP24;
	else
		BUG_ON(1);

	cntl |= CNTL_BGR;

	writel(cntl, priv.regs + CLCD_PL111_CNTL);

	if (priv.amba_dev->dev.platform_data) {
		board = priv.amba_dev->dev.platform_data;

		if (board->enable)
			board->enable(NULL);
	}

	/* Enable Interrupts */
	writel(CLCD_IRQ_NEXTBASE_UPDATE, priv.regs + CLCD_PL111_IENB);

	return 0;
}

int clcd_disable(struct drm_crtc *crtc)
{
	struct clcd_board *board;
	struct pl111_drm_crtc *pl111_crtc = to_pl111_crtc(crtc);
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	unsigned long flags;
#endif

	pr_debug("DRM %s\n", __func__);

	/* Disable Interrupts */
	writel(0x00000000, priv.regs + CLCD_PL111_IENB);

	if (priv.amba_dev->dev.platform_data) {
		board = priv.amba_dev->dev.platform_data;

		if (board->disable)
			board->disable(NULL);
	}

	/* Disable and Power Down */
	writel(0, priv.regs + CLCD_PL111_CNTL);

	/* Disable clock */
	clk_disable_unprepare(priv.clk);

	pl111_crtc->last_bpp = 0;
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	spin_lock_irqsave(&pl111_crtc->current_displaying_lock, flags);
	/* Release the previous buffers */
	if (pl111_crtc->old_kds_res_set != NULL)
		kds_resource_set_release(&pl111_crtc->old_kds_res_set);

	pl111_crtc->old_kds_res_set = NULL;
	spin_unlock_irqrestore(&pl111_crtc->current_displaying_lock, flags);
#endif
	return 0;
}

/*
 * To avoid a possible race where "pl111_crtc->current_update_res" has
 * been updated (non NULL) but the corresponding scanout buffer has not been
 * written to the base registers we must always call this function holding
 * the "base_update_lock" spinlock with IRQs disabled (spin_lock_irqsave()).
 */
void do_flip_to_res(struct pl111_drm_flip_resource *flip_res)
{
	struct pl111_drm_crtc *pl111_crtc = to_pl111_crtc(flip_res->crtc);
	struct drm_framebuffer *fb;
	struct pl111_gem_bo *bo;
	size_t min_size;
	fb = flip_res->fb;
	bo = PL111_BO_FROM_FRAMEBUFFER(fb);



	min_size = (fb->height - 1) * fb->pitches[0]
	         + fb->width * (fb->bits_per_pixel >> 3);

	BUG_ON(bo->gem_object.size < min_size);

	/* Don't even attempt PL111_BOT_SHM, it's not contiguous */
	BUG_ON(bo->type != PL111_BOT_DMA);

	/*
	 * Note the buffer for releasing after IRQ, and don't allow any more
	 * updates until then.
	 *
	 * This clcd controller latches the new address on next vsync. Address
	 * latching is indicated by CLCD_IRQ_NEXTBASE_UPDATE, and so we must
	 * wait for that before releasing the previous buffer's kds
	 * resources. Otherwise, we'll allow writers to write to the old buffer
	 * whilst it is still being displayed
	 */
	pl111_crtc->current_update_res = flip_res;

	DRM_DEBUG_KMS("Displaying fb 0x%p, dumb_bo 0x%p, physaddr %.8x\n",
			fb, bo, bo->backing_data.dma.fb_dev_addr);
	
	if (drm_vblank_get(pl111_crtc->crtc.dev, pl111_crtc->crtc_index) < 0)
		DRM_ERROR("Could not get vblank reference for crtc %d\n",
				pl111_crtc->crtc_index);

	/* Set the scanout buffer */
	writel(bo->backing_data.dma.fb_dev_addr, priv.regs + CLCD_UBAS);
	writel(bo->backing_data.dma.fb_dev_addr +
		((fb->height - 1) * fb->pitches[0]), priv.regs + CLCD_LBAS);
}

void
show_framebuffer_on_crtc_cb_internal(struct pl111_drm_flip_resource *flip_res,
					struct drm_framebuffer *fb)
{
	unsigned long irq_flags;
	struct pl111_drm_crtc *pl111_crtc = to_pl111_crtc(flip_res->crtc);

	spin_lock_irqsave(&pl111_crtc->base_update_lock, irq_flags);
	if (list_empty(&pl111_crtc->update_queue) &&
			!pl111_crtc->current_update_res) {
		do_flip_to_res(flip_res);
	} else {
		/*
		 * Enqueue the update to occur on a future IRQ
		 * This only happens on triple-or-greater buffering
		 */
		DRM_DEBUG_KMS("Deferring 3+ buffered flip to fb %p to IRQ\n",
				fb);
		list_add_tail(&flip_res->link, &pl111_crtc->update_queue);
	}
	spin_unlock_irqrestore(&pl111_crtc->base_update_lock, irq_flags);

	if (!flip_res->page_flip && (pl111_crtc->last_bpp == 0 ||
			pl111_crtc->last_bpp != fb->bits_per_pixel ||
			!drm_mode_equal(pl111_crtc->new_mode,
					pl111_crtc->current_mode))) {
		struct clcd_regs timing;

		pl111_convert_drm_mode_to_timing(pl111_crtc->new_mode, &timing);

		DRM_DEBUG_KMS("Set timing: %08X:%08X:%08X:%08X clk=%ldHz\n",
				timing.tim0, timing.tim1, timing.tim2,
				timing.tim3, timing.pixclock);

		/* This is the actual mode setting part */
		clk_set_rate(priv.clk, timing.pixclock);

		writel(timing.tim0, priv.regs + CLCD_TIM0);
		writel(timing.tim1, priv.regs + CLCD_TIM1);
		writel(timing.tim2, priv.regs + CLCD_TIM2);
		writel(timing.tim3, priv.regs + CLCD_TIM3);

		clcd_enable(fb);
		pl111_crtc->last_bpp = fb->bits_per_pixel;
	}

	if (!flip_res->page_flip) {
		drm_mode_destroy(flip_res->crtc->dev, pl111_crtc->current_mode);
		pl111_crtc->current_mode = pl111_crtc->new_mode;
		pl111_crtc->new_mode = NULL;
	}

	BUG_ON(!pl111_crtc->current_mode);

	/*
	 * If IRQs weren't enabled before, they are now. This will eventually
	 * cause flip_res to be released via pl111_common_irq, which updates
	 * every time the Base Address is latched (i.e. every frame, regardless
	 * of whether we update the base address or not)
	 */
}

irqreturn_t pl111_irq(int irq, void *data)
{
	u32 irq_stat;
	struct pl111_drm_crtc *pl111_crtc = priv.pl111_crtc;

	irq_stat = readl(priv.regs + CLCD_PL111_MIS);

	if (!irq_stat)
		return IRQ_NONE;

	if (irq_stat & CLCD_IRQ_NEXTBASE_UPDATE)
		pl111_common_irq(pl111_crtc);

	/* Clear the interrupt once done */
	writel(irq_stat, priv.regs + CLCD_PL111_ICR);

	return IRQ_HANDLED;
}

int pl111_device_init(struct drm_device *dev)
{
	struct pl111_drm_dev_private *priv = dev->dev_private;
	int ret;

	if (priv == NULL) {
		pr_err("%s no private data\n", __func__);
		return -EINVAL;
	}

	if (priv->amba_dev == NULL) {
		pr_err("%s no amba device found\n", __func__);
		return -EINVAL;
	}

	/* set up MMIO for register access */
	priv->mmio_start = priv->amba_dev->res.start;
	priv->mmio_len = resource_size(&priv->amba_dev->res);

	DRM_DEBUG_KMS("mmio_start=%lu, mmio_len=%u\n", priv->mmio_start,
			priv->mmio_len);

	priv->regs = ioremap(priv->mmio_start, priv->mmio_len);
	if (priv->regs == NULL) {
		pr_err("%s failed mmio\n", __func__);
		return -EINVAL;
	}

	/* turn off interrupts */
	writel(0, priv->regs + CLCD_PL111_IENB);

	ret = request_irq(priv->amba_dev->irq[0], pl111_irq, 0,
				"pl111_irq_handler", NULL);
	if (ret != 0) {
		pr_err("%s failed %d\n", __func__, ret);
		goto out_mmio;
	}

	goto finish;

out_mmio:
	iounmap(priv->regs);
finish:
	DRM_DEBUG_KMS("pl111_device_init returned %d\n", ret);
	return ret;
}

void pl111_device_fini(struct drm_device *dev)
{
	struct pl111_drm_dev_private *priv = dev->dev_private;
	u32 cntl;

	if (priv == NULL || priv->regs == NULL)
		return;

	free_irq(priv->amba_dev->irq[0], NULL);

	cntl = readl(priv->regs + CLCD_PL111_CNTL);

	cntl &= ~CNTL_LCDEN;
	writel(cntl, priv->regs + CLCD_PL111_CNTL);

	cntl &= ~CNTL_LCDPWR;
	writel(cntl, priv->regs + CLCD_PL111_CNTL);

	iounmap(priv->regs);
}

int pl111_amba_probe(struct amba_device *dev, const struct amba_id *id)
{
	struct clcd_board *board = dev->dev.platform_data;
	int ret;
	pr_debug("DRM %s\n", __func__);

	if (!board)
		dev_warn(&dev->dev, "board data not available\n");

	ret = amba_request_regions(dev, NULL);
	if (ret != 0) {
		DRM_ERROR("CLCD: unable to reserve regs region\n");
		goto out;
	}

	priv.amba_dev = dev;

	priv.clk = clk_get(&priv.amba_dev->dev, NULL);
	if (IS_ERR(priv.clk)) {
		DRM_ERROR("CLCD: unable to get clk.\n");
		ret = PTR_ERR(priv.clk);
		goto clk_err;
	}

	return 0;

clk_err:
	amba_release_regions(dev);
out:
	return ret;
}

int pl111_amba_remove(struct amba_device *dev)
{
	DRM_DEBUG_KMS("DRM %s\n", __func__);

	clk_put(priv.clk);

	amba_release_regions(dev);

	priv.amba_dev = NULL;

	return 0;
}

void pl111_convert_drm_mode_to_timing(struct drm_display_mode *mode,
					struct clcd_regs *timing)
{
	unsigned int ppl, hsw, hfp, hbp;
	unsigned int lpp, vsw, vfp, vbp;
	unsigned int cpl;

	memset(timing, 0, sizeof(struct clcd_regs));

	ppl = (mode->hdisplay / 16) - 1;
	hsw = mode->hsync_end - mode->hsync_start - 1;
	hfp = mode->hsync_start - mode->hdisplay - 1;
	hbp = mode->htotal - mode->hsync_end - 1;

	lpp = mode->vdisplay - 1;
	vsw = mode->vsync_end - mode->vsync_start - 1;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;

	cpl = mode->hdisplay - 1;

	timing->tim0 = (ppl << 2) | (hsw << 8) | (hfp << 16) | (hbp << 24);
	timing->tim1 = lpp | (vsw << 10) | (vfp << 16) | (vbp << 24);
	timing->tim2 = TIM2_IVS | TIM2_IHS | TIM2_IPC | TIM2_BCD | (cpl << 16);
	timing->tim3 = 0;

	timing->pixclock = mode->clock * 1000;
}

void pl111_convert_timing_to_drm_mode(struct clcd_regs *timing,
					struct drm_display_mode *mode)
{
	unsigned int ppl, hsw, hfp, hbp;
	unsigned int lpp, vsw, vfp, vbp;

	ppl = (timing->tim0 >> 2) & 0x3f;
	hsw = (timing->tim0 >> 8) & 0xff;
	hfp = (timing->tim0 >> 16) & 0xff;
	hbp = (timing->tim0 >> 24) & 0xff;

	lpp = timing->tim1 & 0x3ff;
	vsw = (timing->tim1 >> 10) & 0x3f;
	vfp = (timing->tim1 >> 16) & 0xff;
	vbp = (timing->tim1 >> 24) & 0xff;

	mode->hdisplay    = (ppl + 1) * 16;
	mode->hsync_start = ((ppl + 1) * 16) + hfp + 1;
	mode->hsync_end   = ((ppl + 1) * 16) + hfp + hsw + 2;
	mode->htotal      = ((ppl + 1) * 16) + hfp + hsw + hbp + 3;
	mode->hskew       = 0;

	mode->vdisplay    = lpp + 1;
	mode->vsync_start = lpp + vfp + 1;
	mode->vsync_end   = lpp + vfp + vsw + 2;
	mode->vtotal      = lpp + vfp + vsw + vbp + 2;

	mode->flags = 0;

	mode->width_mm = 0;
	mode->height_mm = 0;

	mode->clock = timing->pixclock / 1000;
	mode->hsync = timing->pixclock / mode->htotal;
	mode->vrefresh = mode->hsync / mode->vtotal;
}
