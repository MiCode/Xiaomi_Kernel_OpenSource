/*
 * drivers/video/tegra/host/msenc/msenc.c
 *
 * Tegra MSENC Module Support
 *
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

#include <linux/slab.h>         /* for kzalloc */
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/clk/tegra.h>
#include <asm/byteorder.h>      /* for parsing ucode image wrt endianness */
#include <linux/delay.h>	/* for udelay */
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <mach/pm_domains.h>

#include "dev.h"
#include "msenc.h"
#include "hw_msenc.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "nvhost_scale.h"
#include "chip_support.h"
#include "nvhost_memmgr.h"
#include "t114/t114.h"
#include "t148/t148.h"
#include "t124/t124.h"

#define MSENC_IDLE_TIMEOUT_DEFAULT	10000	/* 10 milliseconds */
#define MSENC_IDLE_CHECK_PERIOD		10	/* 10 usec */

#define get_msenc(ndev) ((struct msenc *)(ndev)->dev.platform_data)
#define set_msenc(ndev, f) ((ndev)->dev.platform_data = f)

/* caller is responsible for freeing */
static char *msenc_get_fw_name(struct platform_device *dev)
{
	char *fw_name;
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* note size here is a little over...*/
	fw_name = kzalloc(32, GFP_KERNEL);
	if (!fw_name)
		return NULL;

	decode_msenc_ver(pdata->version, &maj, &min);
	switch (maj) {
	case 2:
		/* there are no minor versions so far for maj==2 */
		sprintf(fw_name, "nvhost_msenc02.fw");
		break;
	case 3:
		sprintf(fw_name, "nvhost_msenc03%d.fw", min);
		break;
	default:
		kfree(fw_name);
		return NULL;
	}

	dev_info(&dev->dev, "fw name:%s\n", fw_name);

	return fw_name;
}

static int msenc_dma_wait_idle(struct platform_device *dev, u32 *timeout)
{
	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = MSENC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, MSENC_IDLE_CHECK_PERIOD, *timeout);
		u32 dmatrfcmd = host1x_readl(dev, msenc_dmatrfcmd_r());
		u32 idle_v = msenc_dmatrfcmd_idle_v(dmatrfcmd);

		if (msenc_dmatrfcmd_idle_true_v() == idle_v) {
			nvhost_dbg_fn("done");
			return 0;
		}

		udelay(MSENC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	dev_err(&dev->dev, "dma idle timeout");

	return -1;
}

static int msenc_dma_pa_to_internal_256b(struct platform_device *dev,
		u32 offset, u32 internal_offset, bool imem)
{
	u32 cmd = msenc_dmatrfcmd_size_256b_f();
	u32 pa_offset =  msenc_dmatrffboffs_offs_f(offset);
	u32 i_offset = msenc_dmatrfmoffs_offs_f(internal_offset);
	u32 timeout = 0; /* default*/

	if (imem)
		cmd |= msenc_dmatrfcmd_imem_true_f();

	host1x_writel(dev, msenc_dmatrfmoffs_r(), i_offset);
	host1x_writel(dev, msenc_dmatrffboffs_r(), pa_offset);
	host1x_writel(dev, msenc_dmatrfcmd_r(), cmd);

	return msenc_dma_wait_idle(dev, &timeout);

}

static int msenc_wait_idle(struct platform_device *dev, u32 *timeout)
{
	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = MSENC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, MSENC_IDLE_CHECK_PERIOD, *timeout);
		u32 w = host1x_readl(dev, msenc_idlestate_r());

		if (!w) {
			nvhost_dbg_fn("done");
			return 0;
		}
		udelay(MSENC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout);

	return -1;
}

int msenc_boot(struct platform_device *dev)
{
	u32 timeout;
	u32 offset;
	int err = 0;
	struct msenc *m = get_msenc(dev);

	/* check if firmware is loaded or not */
	if (!m || !m->valid)
		return -ENOMEDIUM;

	host1x_writel(dev, msenc_dmactl_r(), 0);
	host1x_writel(dev, msenc_dmatrfbase_r(),
		(m->phys + m->os.bin_data_offset) >> 8);

	for (offset = 0; offset < m->os.data_size; offset += 256)
		msenc_dma_pa_to_internal_256b(dev,
					   m->os.data_offset + offset,
					   offset, false);

	msenc_dma_pa_to_internal_256b(dev, m->os.code_offset, 0, true);

	/* setup msenc interrupts and enable interface */
	host1x_writel(dev, msenc_irqmset_r(),
			(msenc_irqmset_ext_f(0xff) |
				msenc_irqmset_swgen1_set_f() |
				msenc_irqmset_swgen0_set_f() |
				msenc_irqmset_exterr_set_f() |
				msenc_irqmset_halt_set_f()   |
				msenc_irqmset_wdtmr_set_f()));
	host1x_writel(dev, msenc_irqdest_r(),
			(msenc_irqdest_host_ext_f(0xff) |
				msenc_irqdest_host_swgen1_host_f() |
				msenc_irqdest_host_swgen0_host_f() |
				msenc_irqdest_host_exterr_host_f() |
				msenc_irqdest_host_halt_host_f()));
	host1x_writel(dev, msenc_itfen_r(),
			(msenc_itfen_mthden_enable_f() |
				msenc_itfen_ctxen_enable_f()));

	/* boot msenc */
	host1x_writel(dev, msenc_bootvec_r(), msenc_bootvec_vec_f(0));
	host1x_writel(dev, msenc_cpuctl_r(),
			msenc_cpuctl_startcpu_true_f());

	timeout = 0; /* default */

	err = msenc_wait_idle(dev, &timeout);
	if (err != 0) {
		dev_err(&dev->dev, "boot failed due to timeout");
		return err;
	}

	return 0;
}

static int msenc_setup_ucode_image(struct platform_device *dev,
		u32 *ucode_ptr,
		const struct firmware *ucode_fw)
{
	struct msenc *m = get_msenc(dev);
	/* image data is little endian. */
	struct msenc_ucode_v1 ucode;
	int w;

	/* copy the whole thing taking into account endianness */
	for (w = 0; w < ucode_fw->size / sizeof(u32); w++)
		ucode_ptr[w] = le32_to_cpu(((u32 *)ucode_fw->data)[w]);

	ucode.bin_header = (struct msenc_ucode_bin_header_v1 *)ucode_ptr;
	/* endian problems would show up right here */
	if (ucode.bin_header->bin_magic != 0x10de) {
		dev_err(&dev->dev,
			   "failed to get firmware magic");
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

	nvhost_dbg_info("ucode bin header: magic:0x%x ver:%d size:%d",
		ucode.bin_header->bin_magic,
		ucode.bin_header->bin_ver,
		ucode.bin_header->bin_size);
	nvhost_dbg_info("ucode bin header: os bin (header,data) offset size: 0x%x, 0x%x %d",
		ucode.bin_header->os_bin_header_offset,
		ucode.bin_header->os_bin_data_offset,
		ucode.bin_header->os_bin_size);
	ucode.os_header = (struct msenc_ucode_os_header_v1 *)
		(((void *)ucode_ptr) + ucode.bin_header->os_bin_header_offset);

	nvhost_dbg_info("os ucode header: os code (offset,size): 0x%x, 0x%x",
		ucode.os_header->os_code_offset,
		ucode.os_header->os_code_size);
	nvhost_dbg_info("os ucode header: os data (offset,size): 0x%x, 0x%x",
		ucode.os_header->os_data_offset,
		ucode.os_header->os_data_size);
	nvhost_dbg_info("os ucode header: num apps: %d",
		ucode.os_header->num_apps);

	m->os.size = ucode.bin_header->os_bin_size;
	m->os.bin_data_offset = ucode.bin_header->os_bin_data_offset;
	m->os.code_offset = ucode.os_header->os_code_offset;
	m->os.data_offset = ucode.os_header->os_data_offset;
	m->os.data_size   = ucode.os_header->os_data_size;

	return 0;
}

int msenc_read_ucode(struct platform_device *dev, const char *fw_name)
{
	struct msenc *m = get_msenc(dev);
	const struct firmware *ucode_fw;
	int err;

	m->phys = 0;
	m->mapped = NULL;
	init_dma_attrs(&m->attrs);

	ucode_fw  = nvhost_client_request_firmware(dev, fw_name);
	if (!ucode_fw) {
		dev_err(&dev->dev, "failed to get msenc firmware\n");
		err = -ENOENT;
		return err;
	}

	m->size = ucode_fw->size;
	dma_set_attr(DMA_ATTR_READ_ONLY, &m->attrs);

	m->mapped = dma_alloc_attrs(&dev->dev,
				m->size, &m->phys,
				GFP_KERNEL, &m->attrs);
	if (!m->mapped) {
		dev_err(&dev->dev, "dma memory allocation failed");
		err = -ENOMEM;
		goto clean_up;
	}

	err = msenc_setup_ucode_image(dev, m->mapped, ucode_fw);
	if (err) {
		dev_err(&dev->dev, "failed to parse firmware image\n");
		goto clean_up;
	}

	m->valid = true;

	release_firmware(ucode_fw);

	return 0;

clean_up:
	if (m->mapped) {
		dma_free_attrs(&dev->dev,
			m->size, m->mapped,
			m->phys, &m->attrs);
		m->mapped = NULL;
	}
	release_firmware(ucode_fw);
	return err;
}

int nvhost_msenc_init(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	int err = 0;
	struct msenc *m;
	char *fw_name;

	nvhost_dbg_fn("in dev:%p", dev);

	fw_name = msenc_get_fw_name(dev);
	if (!fw_name) {
		dev_err(&dev->dev, "couldn't determine firmware name");
		return -EINVAL;
	}

	m = kzalloc(sizeof(struct msenc), GFP_KERNEL);
	if (!m) {
		dev_err(&dev->dev, "couldn't alloc ucode");
		kfree(fw_name);
		return -ENOMEM;
	}
	set_msenc(dev, m);
	nvhost_dbg_fn("primed dev:%p", dev);

	err = msenc_read_ucode(dev, fw_name);
	kfree(fw_name);
	fw_name = 0;

	if (err || !m->valid) {
		dev_err(&dev->dev, "ucode not valid");
		goto clean_up;
	}

	nvhost_module_busy(dev);
	msenc_boot(dev);
	nvhost_module_idle(dev);

	if (pdata->scaling_init)
		nvhost_scale_hw_init(dev);

	return 0;

clean_up:
	dev_err(&dev->dev, "failed");
	return err;
}

void nvhost_msenc_deinit(struct platform_device *dev)
{
	struct msenc *m = get_msenc(dev);
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (pdata->scaling_init)
		nvhost_scale_hw_deinit(dev);

	if (!m)
		return;

	/* unpin, free ucode memory */
	if (m->mapped) {
		dma_free_attrs(&dev->dev,
			m->size, m->mapped,
			m->phys, &m->attrs);
		m->mapped = NULL;
	}
	m->valid = false;
	kfree(m);
	set_msenc(dev, NULL);
}

int nvhost_msenc_finalize_poweron(struct platform_device *dev)
{
	return msenc_boot(dev);
}

static struct of_device_id tegra_msenc_of_match[] = {
#ifdef TEGRA_11X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra114-msenc",
		.data = (struct nvhost_device_data *)&t11_msenc_info },
#endif
#ifdef TEGRA_14X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra148-msenc",
		.data = (struct nvhost_device_data *)&t14_msenc_info },
#endif
#ifdef TEGRA_12X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra124-msenc",
		.data = (struct nvhost_device_data *)&t124_msenc_info },
#endif
	{ },
};

static int msenc_probe(struct platform_device *dev)
{
	int err = 0;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_msenc_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	pdata->pdev = dev;
	mutex_init(&pdata->lock);
	platform_set_drvdata(dev, pdata);
	err = nvhost_client_device_get_resources(dev);
	if (err)
		return err;

	dev->dev.platform_data = NULL;

	/* get the module clocks to sane state */
	nvhost_module_init(dev);

#ifdef CONFIG_PM_GENERIC_DOMAINS
	pdata->pd.name = "msenc";

	/* add module power domain and also add its domain
	 * as sub-domain of MC domain */
	err = nvhost_module_add_domain(&pdata->pd, dev);
#endif

	err = nvhost_client_device_init(dev);

	return 0;
}

static int __exit msenc_remove(struct platform_device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put(&dev->dev);
	pm_runtime_disable(&dev->dev);
#else
	nvhost_module_disable_clk(&dev->dev);
#endif
	return 0;
}

static struct platform_driver msenc_driver = {
	.probe = msenc_probe,
	.remove = __exit_p(msenc_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "msenc",
#ifdef CONFIG_OF
		.of_match_table = tegra_msenc_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	}
};

static int __init msenc_init(void)
{
	return platform_driver_register(&msenc_driver);
}

static void __exit msenc_exit(void)
{
	platform_driver_unregister(&msenc_driver);
}

module_init(msenc_init);
module_exit(msenc_exit);
