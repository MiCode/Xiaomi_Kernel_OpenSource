/*
 * drivers/video/tegra/host/vic/vic03.c
 *
 * Tegra VIC03 Module Support
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>         /* for kzalloc */
#include <asm/byteorder.h>      /* for parsing ucode image wrt endianness */
#include <linux/delay.h>	/* for udelay */
#include <linux/export.h>
#include <linux/scatterlist.h>
#include <linux/nvmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/tegra-powergate.h>
#include <linux/tegra-soc.h>

#include "dev.h"
#include "class_ids.h"
#include "bus_client.h"
#include "nvhost_as.h"
#include "nvhost_acm.h"
#include "nvhost_scale.h"

#include "host1x/host1x_hwctx.h"

#include "vic03.h"
#include "hw_flcn_vic03.h"
#include "hw_tfbif_vic03.h"

#include "t124/hardware_t124.h" /* for nvhost opcodes*/
#include "t124/t124.h"

#include <mach/pm_domains.h>

#include "../../../../../arch/arm/mach-tegra/iomap.h"

static inline struct vic03 *get_vic03(struct platform_device *dev)
{
	return (struct vic03 *)nvhost_get_private_data(dev);
}
static inline void set_vic03(struct platform_device *dev, struct vic03 *vic03)
{
	nvhost_set_private_data(dev, vic03);
}

/* caller is responsible for freeing */
static char *vic_get_fw_name(struct platform_device *dev)
{
	char *fw_name;
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* note size here is a little over...*/
	fw_name = kzalloc(32, GFP_KERNEL);
	if (!fw_name)
		return NULL;

	decode_vic_ver(pdata->version, &maj, &min);
	sprintf(fw_name, "vic%02d_ucode.bin", maj);
	dev_info(&dev->dev, "fw name:%s\n", fw_name);

	return fw_name;
}

#define VIC_IDLE_TIMEOUT_DEFAULT	10000	/* 10 milliseconds */
#define VIC_IDLE_CHECK_PERIOD	10		/* 10 usec */
static int vic03_flcn_wait_idle(struct platform_device *pdev,
				u32 *timeout)
{
	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = VIC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, VIC_IDLE_CHECK_PERIOD, *timeout);
		u32 w = host1x_readl(pdev, flcn_idlestate_r());

		if (!w) {
			nvhost_dbg_fn("done");
			return 0;
		}
		udelay(VIC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	dev_err(&pdev->dev, "vic03 flcn idle timeout");

	return -1;
}

static int vic03_flcn_dma_wait_idle(struct platform_device *pdev, u32 *timeout)
{
	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = VIC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, VIC_IDLE_CHECK_PERIOD, *timeout);
		u32 dmatrfcmd = host1x_readl(pdev, flcn_dmatrfcmd_r());
		u32 idle_v = flcn_dmatrfcmd_idle_v(dmatrfcmd);

		if (flcn_dmatrfcmd_idle_true_v() == idle_v) {
			nvhost_dbg_fn("done");
			return 0;
		}
		udelay(VIC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	dev_err(&pdev->dev, "vic03 dma idle timeout");

	return -1;
}


static int vic03_flcn_dma_pa_to_internal_256b(struct platform_device *pdev,
					      phys_addr_t pa,
					      u32 internal_offset,
					      bool imem)
{
	u32 cmd = flcn_dmatrfcmd_size_256b_f();
	u32 pa_offset =  flcn_dmatrffboffs_offs_f(pa);
	u32 i_offset = flcn_dmatrfmoffs_offs_f(internal_offset);
	u32 timeout = 0; /* default*/

	if (imem)
		cmd |= flcn_dmatrfcmd_imem_true_f();

	host1x_writel(pdev, flcn_dmatrfmoffs_r(), i_offset);
	host1x_writel(pdev, flcn_dmatrffboffs_r(), pa_offset);
	host1x_writel(pdev, flcn_dmatrfcmd_r(), cmd);

	return vic03_flcn_dma_wait_idle(pdev, &timeout);

}

static int vic03_setup_ucode_image(struct platform_device *dev,
				   u32 *ucode_ptr,
				   const struct firmware *ucode_fw)
{
	struct vic03 *v = get_vic03(dev);
	/* image data is little endian. */
	struct ucode_v1_vic03 ucode;
	int w;

	/* copy the whole thing taking into account endianness */
	for (w = 0; w < ucode_fw->size/sizeof(u32); w++)
		ucode_ptr[w] = le32_to_cpu(((u32 *)ucode_fw->data)[w]);

	ucode.bin_header = (struct ucode_bin_header_v1_vic03 *)ucode_ptr;
	/* endian problems would show up right here */
	if (ucode.bin_header->bin_magic != 0x10de) {
		dev_err(&dev->dev,
			   "failed to get vic03 firmware magic");
		return -EINVAL;
	}
	if (ucode.bin_header->bin_ver != 1) {
		dev_err(&dev->dev,
			   "unsupported firmware version");
		return -ENOENT;
	}
	/* shouldn't be bigger than what firmware thinks */
	if (ucode.bin_header->bin_size > ucode_fw->size) {
		dev_err(&dev->dev,
			   "ucode image size inconsistency");
		return -EINVAL;
	}

	nvhost_dbg_info("vic03 ucode bin header: magic:0x%x ver:%d size:%d",
			ucode.bin_header->bin_magic,
			ucode.bin_header->bin_ver,
			ucode.bin_header->bin_size);
	nvhost_dbg_info("vic03 ucode bin header: os bin (header,data) offset size: 0x%x, 0x%x %d",
			ucode.bin_header->os_bin_header_offset,
			ucode.bin_header->os_bin_data_offset,
			ucode.bin_header->os_bin_size);
	nvhost_dbg_info("vic03 ucode bin header: fce bin (header,data) offset size: 0x%x, 0x%x %d",
			ucode.bin_header->fce_bin_header_offset,
			ucode.bin_header->fce_bin_data_offset,
			ucode.bin_header->fce_bin_size);

	ucode.os_header = (struct ucode_os_header_v1_vic03 *)
		(((void *)ucode_ptr) + ucode.bin_header->os_bin_header_offset);

	nvhost_dbg_info("vic03 os ucode header: os code (offset,size): 0x%x, 0x%x",
			ucode.os_header->os_code_offset,
			ucode.os_header->os_code_size);
	nvhost_dbg_info("vic03 os ucode header: os data (offset,size): 0x%x, 0x%x",
			ucode.os_header->os_data_offset,
			ucode.os_header->os_data_size);
	nvhost_dbg_info("vic03 os ucode header: num apps: %d", ucode.os_header->num_apps);

	ucode.fce_header = (struct ucode_fce_header_v1_vic03 *)
		(((void *)ucode_ptr) + ucode.bin_header->fce_bin_header_offset);

	nvhost_dbg_info("vic03 fce ucode header: offset, buffer_size, size: 0x%x 0x%x 0x%x",
			ucode.fce_header->fce_ucode_offset,
			ucode.fce_header->fce_ucode_buffer_size,
			ucode.fce_header->fce_ucode_size);

	v->ucode.os.size = ucode.bin_header->os_bin_size;
	v->ucode.os.bin_data_offset = ucode.bin_header->os_bin_data_offset;
	v->ucode.os.code_offset = ucode.os_header->os_code_offset;
	v->ucode.os.data_offset = ucode.os_header->os_data_offset;
	v->ucode.os.data_size   = ucode.os_header->os_data_size;

	v->ucode.fce.size        = ucode.fce_header->fce_ucode_size;
	v->ucode.fce.data_offset = ucode.bin_header->fce_bin_data_offset;

	return 0;
}

static int vic03_read_ucode(struct platform_device *dev, const char *fw_name)
{
	struct vic03 *v = get_vic03(dev);
	const struct firmware *ucode_fw;
	int err;
	DEFINE_DMA_ATTRS(attrs);

	v->ucode.dma_addr = 0;
	v->ucode.mapped = NULL;

	ucode_fw = nvhost_client_request_firmware(dev, fw_name);
	if (!ucode_fw) {
		nvhost_dbg_fn("request firmware failed");
		dev_err(&dev->dev, "failed to get vic03 firmware\n");
		err = -ENOENT;
		return err;
	}

	v->ucode.size = ucode_fw->size;
	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);

	v->ucode.mapped = dma_alloc_attrs(&dev->dev,
				v->ucode.size, &v->ucode.dma_addr,
				GFP_KERNEL, &attrs);
	if (!v->ucode.mapped) {
		dev_err(&dev->dev, "dma memory allocation failed");
		err = -ENOMEM;
		goto clean_up;
	}

	err = vic03_setup_ucode_image(dev, v->ucode.mapped, ucode_fw);
	if (err) {
		dev_err(&dev->dev, "failed to parse firmware image\n");
		goto clean_up;
	}

	v->ucode.valid = true;

	release_firmware(ucode_fw);

	return 0;

 clean_up:
	if (v->ucode.mapped) {
		dma_free_attrs(&dev->dev,
			v->ucode.size, v->ucode.mapped,
			v->ucode.dma_addr, &attrs);
		v->ucode.mapped = NULL;
		v->ucode.dma_addr = 0;
	}
	release_firmware(ucode_fw);
	return err;
}

static int vic03_boot(struct platform_device *pdev)
{
	struct vic03 *v = get_vic03(pdev);
	u32 timeout;
	u32 offset;
	int err = 0;

	/* check if firmware is loaded or not */
	if (!v || !v->ucode.valid)
		return -ENOMEDIUM;

	if (v->is_booted)
		return 0;

	host1x_writel(pdev, flcn_dmactl_r(), 0);

	host1x_writel(pdev, flcn_dmatrfbase_r(),
			(v->ucode.dma_addr + v->ucode.os.bin_data_offset) >> 8);

	for (offset = 0; offset < v->ucode.os.data_size; offset += 256)
		vic03_flcn_dma_pa_to_internal_256b(pdev,
					   v->ucode.os.data_offset + offset,
					   offset, false);

	vic03_flcn_dma_pa_to_internal_256b(pdev, v->ucode.os.code_offset,
					   0, true);

	/* setup falcon interrupts and enable interface */
	host1x_writel(pdev, flcn_irqmset_r(), (flcn_irqmset_ext_f(0xff)    |
					   flcn_irqmset_swgen1_set_f() |
					   flcn_irqmset_swgen0_set_f() |
					   flcn_irqmset_exterr_set_f() |
					   flcn_irqmset_halt_set_f()   |
					   flcn_irqmset_wdtmr_set_f()));
	host1x_writel(pdev, flcn_irqdest_r(), (flcn_irqdest_host_ext_f(0xff) |
					   flcn_irqdest_host_swgen1_host_f() |
					   flcn_irqdest_host_swgen0_host_f() |
					   flcn_irqdest_host_exterr_host_f() |
					   flcn_irqdest_host_halt_host_f()));
	host1x_writel(pdev, flcn_itfen_r(), (flcn_itfen_mthden_enable_f() |
					flcn_itfen_ctxen_enable_f()));

	/* boot falcon */
	host1x_writel(pdev, flcn_bootvec_r(), flcn_bootvec_vec_f(0));
	host1x_writel(pdev, flcn_cpuctl_r(), flcn_cpuctl_startcpu_true_f());

	timeout = 0; /* default */

	err = vic03_flcn_wait_idle(pdev, &timeout);
	if (err != 0) {
		dev_err(&pdev->dev, "boot failed due to timeout");
		return err;
	}

	v->is_booted = true;

	return 0;
}

int nvhost_vic03_init(struct platform_device *dev)
{
	int err = 0;
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);
	struct vic03 *v = get_vic03(dev);
	char *fw_name;

	nvhost_dbg_fn("in dev:%p v:%p", dev, v);

	fw_name = vic_get_fw_name(dev);
	if (!fw_name) {
		dev_err(&dev->dev, "couldn't determine firmware name");
		return -EINVAL;
	}

	if (!v) {
		nvhost_dbg_fn("allocating vic03 support");
		v = kzalloc(sizeof(*v), GFP_KERNEL);
		if (!v) {
			dev_err(&dev->dev, "couldn't alloc vic03 support");
			err = -ENOMEM;
			goto clean_up;
		}
		set_vic03(dev, v);
		v->is_booted = false;
	}
	nvhost_dbg_fn("primed dev:%p v:%p", dev, v);

	v->host = nvhost_get_host(dev);

	if (!v->ucode.valid)
		err = vic03_read_ucode(dev, fw_name);
	if (err)
		goto clean_up;

	kfree(fw_name);
	fw_name = NULL;

	nvhost_module_busy(dev);
	err = vic03_boot(dev);
	nvhost_module_idle(dev);

	if (pdata->scaling_init)
		nvhost_scale_hw_init(dev);

	return 0;

 clean_up:
	kfree(fw_name);
	nvhost_err(&dev->dev, "failed");

	return err;
}

void nvhost_vic03_deinit(struct platform_device *dev)
{
	struct vic03 *v = get_vic03(dev);
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);

	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_READ_ONLY, &attrs);

	if (!v)
		return;

	if (pdata->scaling_init)
		nvhost_scale_hw_deinit(dev);

	if (v->ucode.mapped) {
		dma_free_attrs(&dev->dev,
			v->ucode.size, v->ucode.mapped,
			v->ucode.dma_addr, &attrs);
		v->ucode.mapped = NULL;
		v->ucode.dma_addr = 0;
	}

	/* zap, free */
	set_vic03(dev, NULL);
	kfree(v);
}

static struct nvhost_hwctx *vic03_alloc_hwctx(struct nvhost_hwctx_handler *h,
		struct nvhost_channel *ch)
{
	struct host1x_hwctx_handler *p = to_host1x_hwctx_handler(h);

	struct vic03 *v = get_vic03(ch->dev);
	struct host1x_hwctx *ctx;
	u32 *ptr;
	u32 syncpt = nvhost_get_devdata(ch->dev)->syncpts[0];
	u32 nvhost_vic03_restore_size = 10; /* number of words written below */

	nvhost_dbg_fn("");

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->restore_size = nvhost_vic03_restore_size;

	ctx->cpuva = dma_alloc_writecombine(&ch->dev->dev,
						ctx->restore_size * 4,
						&ctx->iova,
						GFP_KERNEL);
	if (!ctx->cpuva) {
		dev_err(&ch->dev->dev, "memory allocation failed\n");
		goto fail;
	}

	ptr = ctx->cpuva;

	/* set app id, fce ucode size, offset */
	ptr[0] = nvhost_opcode_incr(VIC_UCLASS_METHOD_OFFSET, 2);
	ptr[1] = NVA0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID  >> 2;
	ptr[2] = 1;

	ptr[3] = nvhost_opcode_incr(VIC_UCLASS_METHOD_OFFSET, 2);
	ptr[4] = NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_SIZE >> 2;
	ptr[5] = v->ucode.fce.size;

	ptr[6] = nvhost_opcode_incr(VIC_UCLASS_METHOD_OFFSET, 2);
	ptr[7] = NVA0B6_VIDEO_COMPOSITOR_SET_FCE_UCODE_OFFSET >> 2;
	ptr[8] = (v->ucode.dma_addr + v->ucode.fce.data_offset) >> 8;

	/* syncpt increment to track restore gather. */
	ptr[9] = nvhost_opcode_imm_incr_syncpt(
			host1x_uclass_incr_syncpt_cond_op_done_v(),
			syncpt);

	kref_init(&ctx->hwctx.ref);
	ctx->hwctx.h = &p->h;
	ctx->hwctx.channel = ch;
	ctx->hwctx.valid = true; /* this is a preconditioning sequence... */
	ctx->hwctx.save_incrs = 0;
	ctx->hwctx.save_slots = 0;

	ctx->hwctx.restore_incrs = 1;

	return &ctx->hwctx;

 fail:
	kfree(ctx);
	return NULL;
}

static void vic03_free_hwctx(struct kref *ref)
{
	struct nvhost_hwctx *nctx = container_of(ref, struct nvhost_hwctx, ref);
	struct host1x_hwctx *ctx = to_host1x_hwctx(nctx);

	if (ctx->cpuva) {
		dma_free_writecombine(&nctx->channel->dev->dev,
					ctx->restore_size * 4,
					ctx->cpuva,
					ctx->iova);
		ctx->cpuva = NULL;
		ctx->iova = 0;
	}
	kfree(ctx);
}

static void vic03_get_hwctx (struct nvhost_hwctx *ctx)
{
	nvhost_dbg_fn("");
	kref_get(&ctx->ref);
}
static void vic03_put_hwctx (struct nvhost_hwctx *ctx)
{
	nvhost_dbg_fn("");
	kref_put(&ctx->ref, vic03_free_hwctx);
}
static void vic03_save_push_hwctx ( struct nvhost_hwctx *ctx, struct nvhost_cdma *cdma)
{
	nvhost_dbg_fn("");
}

static void ctxvic03_restore_push(struct nvhost_hwctx *nctx,
		struct nvhost_cdma *cdma)
{
	struct host1x_hwctx *ctx = to_host1x_hwctx(nctx);
	nvhost_cdma_push(cdma,
		nvhost_opcode_setclass(NV_GRAPHICS_VIC_CLASS_ID, 0, 0),
		NVHOST_OPCODE_NOOP);
	_nvhost_cdma_push_gather(cdma,
		ctx->cpuva,
		ctx->iova,
		0,
		nvhost_opcode_gather(ctx->restore_size),
		ctx->iova);
}

struct nvhost_hwctx_handler *nvhost_vic03_alloc_hwctx_handler(u32 syncpt,
	u32 waitbase, struct nvhost_channel *ch)
{
	struct host1x_hwctx_handler *p;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	p->h.syncpt = syncpt;
	p->h.waitbase = waitbase;

	p->h.alloc = vic03_alloc_hwctx;
	p->h.get   = vic03_get_hwctx;
	p->h.put   = vic03_put_hwctx;
	p->h.save_push = vic03_save_push_hwctx;
	p->h.restore_push = ctxvic03_restore_push;

	return &p->h;
}

int nvhost_vic03_finalize_poweron(struct platform_device *pdev)
{
	host1x_writel(pdev, flcn_slcg_override_high_a_r(), 0);
	host1x_writel(pdev, flcn_cg_r(),
		     flcn_cg_idle_cg_dly_cnt_f(4) |
		     flcn_cg_idle_cg_en_f(1) |
		     flcn_cg_wakeup_dly_cnt_f(4));
	return vic03_boot(pdev);
}

int nvhost_vic03_prepare_poweroff(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);
	struct vic03 *v;
	struct nvhost_channel *ch = pdata->channel;

	if (ch) {
		mutex_lock(&ch->submitlock);
		ch->cur_ctx = NULL;
		mutex_unlock(&ch->submitlock);
	}

	v = get_vic03(pdata->pdev);
	if (v)
		v->is_booted = false;

	return 0;
}

static struct of_device_id tegra_vic_of_match[] = {
	{ .compatible = "nvidia,tegra124-vic",
		.data = (struct nvhost_device_data *)&t124_vic_info },
	{ },
};

static int vic03_probe(struct platform_device *dev)
{
	int err;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_vic_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	if (!pdata) {
		dev_err(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	nvhost_dbg_fn("dev:%p pdata:%p", dev, pdata);

	pdata->pdev = dev;
	mutex_init(&pdata->lock);
	platform_set_drvdata(dev, pdata);

	err = nvhost_client_device_get_resources(dev);
	if (err)
		return err;

	dev->dev.platform_data = NULL;

	nvhost_module_init(dev);

#ifdef CONFIG_PM_GENERIC_DOMAINS
	pdata->pd.name = "vic03";

	err = nvhost_module_add_domain(&pdata->pd, dev);
#endif

	err = nvhost_client_device_init(dev);
	if (err) {
		nvhost_dbg_fn("failed to init client device for %s",
			      dev->name);
		pm_runtime_put(&dev->dev);
		return err;
	}

	err = nvhost_as_init_device(dev);
	if (err) {
		nvhost_dbg_fn("failed to init client address space"
			      " device for %s", dev->name);
		pm_runtime_put(&dev->dev);
		return err;
	}

	return 0;
}

static int __exit vic03_remove(struct platform_device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put(&dev->dev);
	pm_runtime_disable(&dev->dev);
#else
	nvhost_module_disable_clk(&dev->dev);
#endif
	return 0;
}

static struct platform_driver vic03_driver = {
	.probe = vic03_probe,
	.remove = __exit_p(vic03_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "vic03",
#ifdef CONFIG_OF
		.of_match_table = tegra_vic_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	}
};

static int __init vic03_init(void)
{
	return platform_driver_register(&vic03_driver);
}

static void __exit vic03_exit(void)
{
	platform_driver_unregister(&vic03_driver);
}

module_init(vic03_init);
module_exit(vic03_exit);
