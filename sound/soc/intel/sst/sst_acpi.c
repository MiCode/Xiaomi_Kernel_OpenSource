
/* sst_acpi.c - SST (LPE) driver init file for ACPI enumeration.
 *
 * Copyright (c) 2013, Intel Corporation.
 *
 *  Authors:	Ramesh Babu K V <Ramesh.Babu@intel.com>
 *  Authors:	Omair Mohammed Abdullah <omair.m.abdullah@intel.com>
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
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <asm/platform_byt_audio.h>
#include <asm/platform_sst.h>
#include <linux/acpi.h>
#include <acpi/acbuffer.h>
#include <acpi/platform/acenv.h>
#include <acpi/platform/aclinux.h>
#include <acpi/actypes.h>
#include <acpi/acpi_bus.h>
#include <sound/intel_sst_ioctl.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

extern struct miscdevice lpe_ctrl;

static const struct sst_platform_config_data sst_byt_pdata = {
	.sst_sram_buff_base	= 0xffffffff,
	.sst_dma_base[0]	= SST_BYT_DMA0_PHY_ADDR,
	.sst_dma_base[1]	= SST_BYT_DMA1_PHY_ADDR,
};

/* use array[0] for ssp_platform_data even though SSP2 is used */
static const struct sst_board_config_data sst_byt_rvp_bdata = {
	.active_ssp_ports = 1,
	.platform_id = 3,
	.board_id = 1,
	.ihf_num_chan = 2,
	.osc_clk_freq = 25000000,
	.ssp_platform_data = {
		[0] = {
			.ssp_cfg_sst = 1,
			.port_number = 2,
			.is_master = 1,
			.pack_mode = 1,
			.num_slots_per_frame = 2,
			.num_bits_per_slot = 24,
			.active_tx_map = 3,
			.active_rx_map = 3,
			.ssp_frame_format = 3,
			.frame_polarity = 1,
			.serial_bitrate_clk_mode = 0,
			.frame_sync_width = 24,
			.dma_handshake_interface_tx = 5,
			.dma_handshake_interface_rx = 4,
			.network_mode = 0,
			.start_delay = 1,
			.ssp_base_add = SST_BYT_SSP2_PHY_ADDR,
		},
	},
};

static const struct sst_board_config_data sst_byt_ffrd8_bdata = {
	.active_ssp_ports = 1,
	.platform_id = 3,
	.board_id = 1,
	.ihf_num_chan = 2,
	.osc_clk_freq = 25000000,
	.ssp_platform_data = {
		[0] = {
			.ssp_cfg_sst = 1,
			.port_number = 0,
			.is_master = 1,
			.pack_mode = 1,
			.num_slots_per_frame = 2,
			.num_bits_per_slot = 24,
			.active_tx_map = 3,
			.active_rx_map = 3,
			.ssp_frame_format = 3,
			.frame_polarity = 1,
			.serial_bitrate_clk_mode = 0,
			.frame_sync_width = 24,
			.dma_handshake_interface_tx = 1,
			.dma_handshake_interface_rx = 0,
			.network_mode = 0,
			.start_delay = 1,
			.ssp_base_add = SST_BYT_SSP0_PHY_ADDR,
		},
	},
};

static const struct sst_board_config_data sst_byt_crv2_bdata = {
	.active_ssp_ports = 1,
	.platform_id = 3,
	.board_id = 1,
	.ihf_num_chan = 1,
	.osc_clk_freq = 25000000,
	.ssp_platform_data = {
		[0] = {
			.ssp_cfg_sst = 1,
			.port_number = 0,
			.is_master = 1,
			.pack_mode = 1,
			.num_slots_per_frame = 2,
			.num_bits_per_slot = 24,
			.active_tx_map = 3,
			.active_rx_map = 3,
			.ssp_frame_format = 3,
			.frame_polarity = 1,
			.serial_bitrate_clk_mode = 0,
			.frame_sync_width = 24,
			.dma_handshake_interface_tx = 1,
			.dma_handshake_interface_rx = 0,
			.network_mode = 0,
			.start_delay = 1,
			.ssp_base_add = SST_BYT_SSP0_PHY_ADDR,
		},
	},
};

static const struct sst_info byt_fwparse_info = {
	.use_elf	= true,
#ifndef CONFIG_SST_DPCM
	.max_streams	= 4,
#else
	.max_streams	= 25,
#endif
	.dma_max_len	= SST_MAX_DMA_LEN_MRFLD,
	.iram_start	= SST_BYT_IRAM_PHY_START,
	.iram_end	= SST_BYT_IRAM_PHY_END,
	.iram_use	= true,
	.dram_start	= SST_BYT_DRAM_PHY_START,
	.dram_end	= SST_BYT_DRAM_PHY_END,
	.dram_use	= true,
	.imr_start	= SST_BYT_IMR_VIRT_START,
	.imr_end	= SST_BYT_IMR_VIRT_END,
	.imr_use	= true,
	.mailbox_start	= SST_BYT_MBOX_PHY_ADDR,
	.num_probes	= 0,
	.lpe_viewpt_rqd  = true,
};


static const struct sst_info cht_fwparse_info = {
	.use_elf	= false,
	.max_streams	= MAX_NUM_STREAMS_MRFLD,
	.dma_max_len	= SST_MAX_DMA_LEN_MRFLD,
	.iram_start	= SST_BYT_IRAM_PHY_START,
	.iram_end	= SST_BYT_IRAM_PHY_END,
	.iram_use	= true,
	.dram_start	= SST_BYT_DRAM_PHY_START,
	.dram_end	= SST_BYT_DRAM_PHY_END,
	.dram_use	= true,
	.imr_start	= SST_BYT_IMR_VIRT_START,
	.imr_end	= SST_BYT_IMR_VIRT_END,
	.imr_use	= true,
	.mailbox_start	= SST_BYT_MBOX_PHY_ADDR,
	.num_probes	= 0,
	.lpe_viewpt_rqd = true,
};

static const struct sst_ipc_info byt_ipc_info = {
#ifndef CONFIG_SST_DPCM
	.use_32bit_ops = true,
	.ipc_offset = 4,
#else
	.use_32bit_ops = false,
	.ipc_offset = 0,
#endif
	.mbox_recv_off = 0x400,
};

static const struct sst_lib_dnld_info  byt_lib_dnld_info = {
	.mod_base           = SST_BYT_IMR_VIRT_START,
	.mod_end            = SST_BYT_IMR_VIRT_END,
	.mod_table_offset   = BYT_FW_MOD_TABLE_OFFSET,
	.mod_table_size     = BYT_FW_MOD_TABLE_SIZE,
#ifndef CONFIG_SND_SST_SW_CODECS
	.mod_ddr_dnld       = true,
#else
	.mod_ddr_dnld       = false,
#endif
};

static const struct sst_lib_dnld_info  cht_lib_dnld_info = {
	.mod_ddr_dnld = false,
};

static const struct sst_ipc_info cht_ipc_info = {
	.use_32bit_ops = false,
	.ipc_offset = 0,
	.mbox_recv_off = 0x1000,
};

struct sst_platform_info cht_platform_data = {
	.probe_data = &cht_fwparse_info,
	.ssp_data = NULL,
	.bdata = NULL,
	.pdata = NULL,
	.ipc_info = &cht_ipc_info,
	.lib_info = &cht_lib_dnld_info,
	.start_recovery_timer = false,
};

struct sst_platform_info byt_rvp_platform_data = {
	.probe_data = &byt_fwparse_info,
	.ssp_data = NULL,
	.bdata = &sst_byt_rvp_bdata,
	.pdata = &sst_byt_pdata,
	.ipc_info = &byt_ipc_info,
	.lib_info = &byt_lib_dnld_info,
	.start_recovery_timer = true,
};

struct sst_platform_info byt_ffrd8_platform_data = {
	.probe_data = &byt_fwparse_info,
	.ssp_data = NULL,
	.bdata = &sst_byt_ffrd8_bdata,
	.pdata = &sst_byt_pdata,
	.ipc_info = &byt_ipc_info,
	.lib_info = &byt_lib_dnld_info,
	.enable_recovery = true,
	.start_recovery_timer = true,
};

int sst_workqueue_init(struct intel_sst_drv *ctx)
{
	pr_debug("%s", __func__);

	INIT_LIST_HEAD(&ctx->memcpy_list);
	INIT_LIST_HEAD(&ctx->libmemcpy_list);
	INIT_LIST_HEAD(&sst_drv_ctx->rx_list);
	INIT_LIST_HEAD(&ctx->ipc_dispatch_list);
	INIT_LIST_HEAD(&ctx->block_list);
	INIT_WORK(&ctx->ipc_post_msg.wq, ctx->ops->post_message);
	init_waitqueue_head(&ctx->wait_queue);

	ctx->mad_wq = create_singlethread_workqueue("sst_mad_wq");
	if (!ctx->mad_wq)
		goto err_wq;
	ctx->post_msg_wq =
		create_singlethread_workqueue("sst_post_msg_wq");
	if (!ctx->post_msg_wq)
		goto err_wq;
	return 0;
err_wq:
	return -EBUSY;
}

void sst_init_locks(struct intel_sst_drv *ctx)
{
	mutex_init(&ctx->stream_lock);
	mutex_init(&ctx->sst_lock);
	mutex_init(&ctx->mixer_ctrl_lock);
	mutex_init(&ctx->csr_lock);
	spin_lock_init(&sst_drv_ctx->rx_msg_lock);
	spin_lock_init(&ctx->ipc_spin_lock);
	spin_lock_init(&ctx->block_lock);
	spin_lock_init(&ctx->pvt_id_lock);
}

int sst_destroy_workqueue(struct intel_sst_drv *ctx)
{
	pr_debug("%s", __func__);
	if (ctx->mad_wq)
		destroy_workqueue(ctx->mad_wq);
	if (ctx->post_msg_wq)
		destroy_workqueue(ctx->post_msg_wq);
	if (ctx->recovery_wq)
		destroy_workqueue(ctx->recovery_wq);
	return 0;
}

#if IS_ENABLED(CONFIG_ACPI)
static int sst_platform_get_resources_fdk(struct intel_sst_drv *ctx,
				      struct platform_device *pdev)
{
	struct resource *rsrc;

	pr_debug("%s", __func__);

	/* All ACPI resource request here */
	/* Get DDR addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsrc) {
		pr_err("Invalid DDR base from IFWI");
		return -EIO;
	}
	ctx->ddr_base = rsrc->start;
	ctx->ddr_end = rsrc->end;
	pr_debug("DDR base: %#x", ctx->ddr_base);
	ctx->ddr = devm_ioremap_nocache(ctx->dev, ctx->ddr_base,
					resource_size(rsrc));
	if (!ctx->ddr) {
		pr_err("unable to map DDR");
		return -EIO;
	}

	/* Get Shim addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!rsrc) {
		pr_err("Invalid SHIM base from IFWI");
		return -EIO;
	}
	ctx->shim_phy_add = rsrc->start;
	pr_debug("SHIM base: %#x", ctx->shim_phy_add);
	ctx->shim = devm_ioremap_nocache(ctx->dev, ctx->shim_phy_add,
					 resource_size(rsrc));
	if (!ctx->shim) {
		pr_err("unable to map SHIM");
		return -EIO;
	}
	/* reassign physical address to LPE viewpoint address */
	ctx->shim_phy_add = SST_BYT_SHIM_PHY_ADDR;

	/* Get mailbox addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!rsrc) {
		pr_err("Invalid Mailbox base from IFWI");
		return -EIO;
	}
	ctx->mailbox_add = rsrc->start;
	pr_debug("Mailbox base: %#x", ctx->mailbox_add);
	ctx->mailbox = devm_ioremap_nocache(ctx->dev, ctx->mailbox_add,
					    resource_size(rsrc));
	if (!ctx->mailbox) {
		pr_err("unable to map mailbox");
		return -EIO;
	}
	/* reassign physical address to LPE viewpoint address */
	ctx->mailbox_add = sst_drv_ctx->info.mailbox_start;

	/* Get iram/iccm addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!rsrc) {
		pr_err("Invalid IRAM base from IFWI");
		return -EIO;
	}
	ctx->iram_base = rsrc->start;
	ctx->iram_end =  rsrc->end;
	pr_debug("IRAM base: %#x", ctx->iram_base);
	ctx->iram = devm_ioremap_nocache(ctx->dev, ctx->iram_base,
					 resource_size(rsrc));
	if (!ctx->iram) {
		pr_err("unable to map IRAM");
		return -EIO;
	}

	/* Get dram/dccm addr from platform resource table */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!rsrc) {
		pr_err("Invalid DRAM base from IFWI");
		return -EIO;
	}
	ctx->dram_base = rsrc->start;
	ctx->dram_end = rsrc->end;
	pr_debug("DRAM base: %#x", ctx->dram_base);
	ctx->dram = devm_ioremap_nocache(ctx->dev, ctx->dram_base,
					 resource_size(rsrc));
	if (!ctx->dram) {
		pr_err("unable to map DRAM");
		return -EIO;
	}

	/* Register the ISR */
	ctx->irq_num = platform_get_irq(pdev, 0);
	pr_debug("irq from pdev is:%d", ctx->irq_num);
	return 0;
}

#define LPE_IRAM_OFFSET 0x0C0000
#define LPE_IRAM_SIZE 0x040000
#define LPE_DRAM_OFFSET 0x100000
#define LPE_DRAM_SIZE 0x040000
#define LPE_SHIM_OFFSET 0x140000
#define LPE_SHIM_SIZE 0x004000
#define LPE_MBOX_OFFSET 0x144000
#define LPE_MBOX_SIZE 0x004000

static int sst_platform_get_resources_edk(struct intel_sst_drv *ctx,
				      struct platform_device *pdev)
{
	struct resource *rsrc;

	pr_err("%s", __func__);

	/* All ACPI resource request here */
	/* Get Shim addr */
	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsrc) {
		pr_err("Invalid SHIM base from IFWI");
		return -EIO;
	}
	pr_err("LPE base: %#x size:%#x", (unsigned int) rsrc->start,
					(unsigned int)resource_size(rsrc));

	ctx->ssp0 = devm_ioremap_nocache(ctx->dev, (rsrc->start + 0xa0000),
					 0x1000);
	if (!ctx->ssp0)
		pr_err("unable to map ssp0");
#if 0
	ctx->dma0 = devm_ioremap_nocache(ctx->dev, (rsrc->start + 0x98000),
					 0x4000);
	if (!ctx->dma0)
		pr_err("unable to map DMA0");

	ctx->dma1 = devm_ioremap_nocache(ctx->dev, (rsrc->start + 0x9c000),
					 0x4000);
	if (!ctx->dma1)
		pr_err("unable to map DMA1");
#endif

	ctx->iram_base = rsrc->start + LPE_IRAM_OFFSET;
	ctx->iram_end =  ctx->iram_base + LPE_IRAM_SIZE - 1;
	pr_err("IRAM base: %#x", ctx->iram_base);
	ctx->iram = devm_ioremap_nocache(ctx->dev, ctx->iram_base,
					 LPE_IRAM_SIZE);
	if (!ctx->iram) {
		pr_err("unable to map IRAM");
		return -EIO;
	}

	ctx->dram_base = rsrc->start + LPE_DRAM_OFFSET;
	ctx->dram_end = ctx->dram_base + LPE_DRAM_SIZE - 1;
	pr_err("DRAM base: %#x", ctx->dram_base);
	ctx->dram = devm_ioremap_nocache(ctx->dev, ctx->dram_base,
					 LPE_DRAM_SIZE);
	if (!ctx->dram) {
		pr_err("unable to map DRAM");
		return -EIO;
	}

	ctx->shim_phy_add = rsrc->start + LPE_SHIM_OFFSET;
	pr_err("SHIM base: %#x", ctx->shim_phy_add);
	ctx->shim = devm_ioremap_nocache(ctx->dev, ctx->shim_phy_add,
							LPE_SHIM_SIZE);
	if (!ctx->shim) {
		pr_err("unable to map SHIM");
		return -EIO;
	}
	/* reassign physical address to LPE viewpoint address */
	ctx->shim_phy_add = SST_BYT_SHIM_PHY_ADDR;

	/* Get mailbox addr */
	ctx->mailbox_add = rsrc->start + LPE_MBOX_OFFSET;
	pr_err("Mailbox base: %#x", ctx->mailbox_add);
	ctx->mailbox = devm_ioremap_nocache(ctx->dev, ctx->mailbox_add,
					    LPE_MBOX_SIZE);
	if (!ctx->mailbox) {
		pr_err("unable to map mailbox");
		return -EIO;
	}

	/* reassign physical address to LPE viewpoint address */
	ctx->mailbox_add = sst_drv_ctx->info.mailbox_start;

	rsrc = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!rsrc) {
		pr_err("Invalid DDR base from IFWI");
		return -EIO;
	}
	ctx->ddr_base = rsrc->start;
	ctx->ddr_end = rsrc->end;
	pr_err("DDR base: %#x", ctx->ddr_base);
	ctx->ddr = devm_ioremap_nocache(ctx->dev, ctx->ddr_base,
					resource_size(rsrc));
	if (!ctx->ddr) {
		pr_err("unable to map DDR");
		return -EIO;
	}
	/* Register the ISR */
	if (!strncmp(ctx->hid, "80860F28", 8))
		ctx->irq_num = 0x1d;
	else if (!strncmp(ctx->hid, "808622A8", 8)) {
		/* FIXME: IRQ number will be moved to 0 once the BIOS fix is done */
		ctx->irq_num = platform_get_irq(pdev, 5);
	} else
		return -EINVAL;
	pr_debug("sst irq_num = %d\n", ctx->irq_num);
	return 0;
}

static int sst_platform_get_resources(const char *hid,
		struct intel_sst_drv *ctx, struct platform_device *pdev)
{

	pr_debug("%s", __func__);

	if (!strncmp(hid, "LPE0F281", 8)) {
		ctx->pci_id = SST_BYT_PCI_ID;
		return sst_platform_get_resources_fdk(ctx, pdev);
	}
	if (!strncmp(hid, "808622A8", 8)) {
		ctx->pci_id = SST_CHT_PCI_ID;
		return sst_platform_get_resources_edk(ctx, pdev);
	}
	if (!strncmp(hid, "80860F28", 8)) {
		ctx->pci_id = SST_BYT_PCI_ID;
		return sst_platform_get_resources_edk(ctx, pdev);
	} else if (!strncmp(hid, "LPE0F28", 7)) {
		ctx->pci_id = SST_BYT_PCI_ID;
		return sst_platform_get_resources_fdk(ctx, pdev);
	} else {
		pr_err("Invalid device\n");
		return -EINVAL;
	}
}

#if IS_ENABLED(CONFIG_SND_INTEL_HDMI)
struct platform_device byt_hdmi_dev = {
	.name           = "hdmi-audio",
	.id             = -1,
	.num_resources  = 0,
};
#endif

int sst_acpi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *device;
	const char *hid;
	int i, ret = 0;
	struct intel_sst_drv *ctx;

#if IS_ENABLED(CONFIG_SND_INTEL_HDMI)
	ret = platform_device_register(&byt_hdmi_dev);
	if (ret < 0)
		pr_debug("%s: Could not register HDMI device\n", __func__);
#endif

	ret = acpi_bus_get_device(handle, &device);
	if (ret) {
		pr_err("%s: could not get acpi device - %d\n", __func__, ret);
		return -ENODEV;
	}

	if (acpi_bus_get_status(device) || !device->status.present) {
		pr_err("%s: device has invalid status", __func__);
		return -ENODEV;
	}

	hid = acpi_device_hid(device);
	pr_debug("%s for %s", __func__, hid);
	ret = sst_alloc_drv_context(dev);
	if (ret)
		return ret;
	ctx = sst_drv_ctx;
	ctx->dev = dev;
	ctx->hid = hid;

	ret = sst_platform_get_resources(hid, ctx, pdev);
	if (ret)
		return ret;
	/* need to save shim registers in BYT */
	ctx->shim_regs64 = devm_kzalloc(dev, sizeof(*ctx->shim_regs64),
					GFP_KERNEL);
	if (!ctx->shim_regs64)
		return -ENOMEM;

	ret = sst_driver_ops(ctx);
	if (ret != 0)
		return -EINVAL;

	sst_init_locks(ctx);

	ctx->stream_cnt = 0;
	ctx->fw_in_mem = NULL;
	ctx->use_dma = 0;
	ctx->use_lli = 0;

	if (sst_workqueue_init(ctx)) {
		ret = -EINVAL;
		goto do_free_wq;
	}

	ctx->pdata = sst_get_acpi_driver_data(hid);
	if (!ctx->pdata)
		return -EINVAL;
	/* bdata update; only for BYT-CR platform*/
	if (ctx->pci_id == SST_BYT_PCI_ID)
		ctx->pdata->bdata = &sst_byt_crv2_bdata;
#if 0
	if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, CRV2)) {
		/* BYT-CR V2 has only mono speaker, while
		 * byt has stereo speaker, for both
		 * HID is same, so platform data also is
		 * same, hence overriding bdata based on spid
		 */
		ctx->pdata->bdata = &sst_byt_crv2_bdata;
		pr_info("Overriding bdata for byt-crv2\n");
	}
#endif

	ctx->use_32bit_ops = ctx->pdata->ipc_info->use_32bit_ops;
	ctx->mailbox_recv_offset = ctx->pdata->ipc_info->mbox_recv_off;
	if (ctx->pci_id == SST_CHT_PCI_ID) {
		ctx->mailbox_size = SST_MAILBOX_SIZE_CHT;
		ctx->ipc_mailbox = ctx->ddr + SST_DDR_MAILBOX_BASE;
	} else {
		ctx->mailbox_size = SST_MAILBOX_SIZE;
		ctx->ipc_mailbox = ctx->mailbox;
	}

	memcpy(&ctx->info, ctx->pdata->probe_data, sizeof(ctx->info));

	ctx->ipc_reg.ipcx = SST_IPCX + ctx->pdata->ipc_info->ipc_offset;
	ctx->ipc_reg.ipcd = SST_IPCD + ctx->pdata->ipc_info->ipc_offset;

	pr_debug("Got drv data max stream %d\n",
				ctx->info.max_streams);
	for (i = 1; i <= ctx->info.max_streams; i++) {
		struct stream_info *stream = &ctx->streams[i];
		mutex_init(&stream->lock);
	}
	ret = sst_request_firmware_async(ctx);
	if (ret) {
		pr_err("Request Firmware failed:%d\n", ret);
		goto do_free_wq;
	}

	ret = devm_request_threaded_irq(ctx->dev, ctx->irq_num, ctx->ops->interrupt,
					ctx->ops->irq_thread, 0, SST_DRV_NAME,
					ctx);
	if (ret)
		return ret;
	pr_debug("Registered IRQ %#x\n", ctx->irq_num);

	/*Register LPE Control as misc driver*/
	ret = misc_register(&lpe_ctrl);
	if (ret) {
		pr_err("couldn't register control device\n");
		goto do_free_wq;
	}
	/* mask all SSP and DMA irq to IA - enabled in acpi kernel driver */
	sst_shim_write64(ctx->shim, SST_IMRX, 0xFFFF0038);

	if (ctx->use_32bit_ops) {
		pr_debug("allocate mem for context save/restore\n ");
		/*allocate mem for fw context save during suspend*/
		ctx->fw_cntx = devm_kzalloc(ctx->dev, FW_CONTEXT_MEM, GFP_KERNEL);
		if (!ctx->fw_cntx) {
			ret = -ENOMEM;
			goto do_free_misc;
		}
		/*setting zero as that is valid mem to restore*/
		ctx->fw_cntx_size = 0;
	}

	platform_set_drvdata(pdev, ctx);
	pm_runtime_set_autosuspend_delay(dev, SST_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	register_sst(dev);
	sst_debugfs_init(ctx);

	if (ctx->pdata->start_recovery_timer) {
		ret = sst_recovery_init(ctx);
		if (ret) {
			pr_err("%s:sst recovery intialization failed", __func__);
			goto do_free_misc;
		}
	}

	sst_set_fw_state_locked(ctx, SST_RESET);
	sst_save_shim64(ctx, ctx->shim, ctx->shim_regs64);
	pr_debug("%s successfully done!\n", __func__);
	return ret;

do_free_misc:
	misc_deregister(&lpe_ctrl);
do_free_wq:
	sst_destroy_workqueue(ctx);

	sst_drv_ctx = NULL;
	platform_set_drvdata(pdev, NULL);
	pr_err("%s: failed with %d\n", __func__, ret);
	return ret;
}

/**
* intel_sst_remove - remove function
*
* @pdev:	platform device structure
*
* This function is called by OS when a device is unloaded
* This frees the interrupt etc
*/
int sst_acpi_remove(struct platform_device *pdev)
{
	struct intel_sst_drv *ctx;

	ctx = platform_get_drvdata(pdev);
	sst_recovery_free(ctx);
	sst_debugfs_exit(ctx);
	pm_runtime_get_noresume(ctx->dev);
	pm_runtime_disable(ctx->dev);
	unregister_sst(ctx->dev);
	sst_set_fw_state_locked(ctx, SST_SHUTDOWN);
	misc_deregister(&lpe_ctrl);
	kfree(ctx->runtime_param.param.addr);
	flush_scheduled_work();
	sst_destroy_workqueue(ctx);
	kfree(ctx->fw_sg_list.src);
	kfree(ctx->fw_sg_list.dst);
	ctx->fw_sg_list.list_len = 0;
	kfree(ctx->fw_in_mem);
	ctx->fw_in_mem = NULL;
	sst_memcpy_free_resources();
	sst_drv_ctx = NULL;
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#else
int sst_acpi_probe(struct platform_device *pdev)
{
	return -EINVAL;
}

int sst_acpi_remove(struct platform_device *pdev)
{
	return -EINVAL;
}
#endif

MODULE_DESCRIPTION("Intel (R) SST(R) Audio Engine ACPI Driver");
MODULE_AUTHOR("Ramesh Babu K V");
MODULE_AUTHOR("Omair Mohammed Abdullah");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("sst");
