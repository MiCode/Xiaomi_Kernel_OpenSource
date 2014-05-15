
/*
 *  sst.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10	Intel Corp
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This driver enumerates the SST audio engine as a PCI or ACPI device and
 *  provides interface to the platform driver to interact with the SST audio
 *  Firmware.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/async.h>
#include <linux/lnw_gpio.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <asm/intel-mid.h>
#include <asm/platform_sst_audio.h>
#include <asm/platform_sst.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

#define CREATE_TRACE_POINTS
#include "sst_trace.h"

MODULE_AUTHOR("Vinod Koul <vinod.koul@intel.com>");
MODULE_AUTHOR("Harsha Priya <priya.harsha@intel.com>");
MODULE_AUTHOR("Dharageswari R <dharageswari.r@intel.com>");
MODULE_AUTHOR("KP Jeeja <jeeja.kp@intel.com>");
MODULE_DESCRIPTION("Intel (R) SST(R) Audio Engine Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SST_DRIVER_VERSION);

struct intel_sst_drv *sst_drv_ctx;
static struct mutex drv_ctx_lock;

/*
 *  * ioctl32 compat
 *   */
#ifdef CONFIG_COMPAT
#include "sst_app_compat_interface.c"
#else
#define intel_sst_ioctl_compat NULL
#endif

static const struct file_operations intel_sst_fops_cntrl = {
	.owner = THIS_MODULE,
	.open = intel_sst_open_cntrl,
	.release = intel_sst_release_cntrl,
	.unlocked_ioctl = intel_sst_ioctl,
	.compat_ioctl = intel_sst_ioctl_compat,
};

struct miscdevice lpe_ctrl = {
	.minor = MISC_DYNAMIC_MINOR,/* dynamic allocation */
	.name = "intel_sst_ctrl",/* /dev/intel_sst_ctrl */
	.fops = &intel_sst_fops_cntrl
};

static inline void set_imr_interrupts(struct intel_sst_drv *ctx, bool enable)
{
	union interrupt_reg imr;

	spin_lock(&ctx->ipc_spin_lock);
	imr.full = sst_shim_read(ctx->shim, SST_IMRX);
	if (enable) {
		imr.part.done_interrupt = 0;
		imr.part.busy_interrupt = 0;
	} else {
		imr.part.done_interrupt = 1;
		imr.part.busy_interrupt = 1;
	}
	sst_shim_write(ctx->shim, SST_IMRX, imr.full);
	spin_unlock(&ctx->ipc_spin_lock);
}

#define SST_IS_PROCESS_REPLY(header) ((header & PROCESS_MSG) ? true : false)
#define SST_VALIDATE_MAILBOX_SIZE(size) ((size <= SST_MAILBOX_SIZE) ? true : false)

static irqreturn_t intel_sst_interrupt_mrfld(int irq, void *context)
{
	union interrupt_reg_mrfld isr;
	union ipc_header_mrfld header;
	union sst_imr_reg_mrfld imr;
	struct ipc_post *msg = NULL;
	unsigned int size = 0;
	struct intel_sst_drv *drv = (struct intel_sst_drv *) context;
	irqreturn_t retval = IRQ_HANDLED;

	/* Interrupt arrived, check src */
	isr.full = sst_shim_read64(drv->shim, SST_ISRX);
	if (isr.part.done_interrupt) {
		/* Clear done bit */
		spin_lock(&drv->ipc_spin_lock);
		header.full = sst_shim_read64(drv->shim,
					drv->ipc_reg.ipcx);
		header.p.header_high.part.done = 0;
		sst_shim_write64(drv->shim, drv->ipc_reg.ipcx, header.full);
		/* write 1 to clear status register */;
		isr.part.done_interrupt = 1;
		sst_shim_write64(drv->shim, SST_ISRX, isr.full);
		spin_unlock(&drv->ipc_spin_lock);
		trace_sst_ipc("ACK   <-", header.p.header_high.full,
					  header.p.header_low_payload,
					  header.p.header_high.part.drv_id);
		queue_work(drv->post_msg_wq, &drv->ipc_post_msg.wq);
		retval = IRQ_HANDLED;
	}
	if (isr.part.busy_interrupt) {
		spin_lock(&drv->ipc_spin_lock);
		imr.full = sst_shim_read64(drv->shim, SST_IMRX);
		imr.part.busy_interrupt = 1;
		sst_shim_write64(drv->shim, SST_IMRX, imr.full);
		spin_unlock(&drv->ipc_spin_lock);
		header.full =  sst_shim_read64(drv->shim, drv->ipc_reg.ipcd);
		if (sst_create_ipc_msg(&msg, header.p.header_high.part.large)) {
			pr_err("No memory available\n");
			drv->ops->clear_interrupt();
			return IRQ_HANDLED;
		}
		if (header.p.header_high.part.large) {
			size = header.p.header_low_payload;
			if (SST_VALIDATE_MAILBOX_SIZE(size)) {
				memcpy_fromio(msg->mailbox_data,
					drv->mailbox + drv->mailbox_recv_offset, size);
			} else {
				pr_err("Mailbox not copied, payload siz is: %u\n", size);
				header.p.header_low_payload = 0;
			}
		}
		msg->mrfld_header = header;
		msg->is_process_reply =
			SST_IS_PROCESS_REPLY(header.p.header_high.part.msg_id);
		trace_sst_ipc("REPLY <-", msg->mrfld_header.p.header_high.full,
					  msg->mrfld_header.p.header_low_payload,
					  msg->mrfld_header.p.header_high.part.drv_id);
		spin_lock(&drv->rx_msg_lock);
		list_add_tail(&msg->node, &drv->rx_list);
		spin_unlock(&drv->rx_msg_lock);
		drv->ops->clear_interrupt();
		retval = IRQ_WAKE_THREAD;
	}
	return retval;
}

static irqreturn_t intel_sst_irq_thread_mfld(int irq, void *context)
{
	struct intel_sst_drv *drv = (struct intel_sst_drv *) context;
	struct ipc_post *__msg, *msg = NULL;
	unsigned long irq_flags;

	if (list_empty(&drv->rx_list))
		return IRQ_HANDLED;

	spin_lock_irqsave(&drv->rx_msg_lock, irq_flags);
	list_for_each_entry_safe(msg, __msg, &drv->rx_list, node) {

		list_del(&msg->node);
		spin_unlock_irqrestore(&drv->rx_msg_lock, irq_flags);
		if (msg->is_process_reply)
			drv->ops->process_message(msg);
		else
			drv->ops->process_reply(msg);

		if (msg->is_large)
			kfree(msg->mailbox_data);
		kfree(msg);
		spin_lock_irqsave(&drv->rx_msg_lock, irq_flags);
	}
	spin_unlock_irqrestore(&drv->rx_msg_lock, irq_flags);
	return IRQ_HANDLED;
}
/**
* intel_sst_interrupt - Interrupt service routine for SST
*
* @irq:	irq number of interrupt
* @context: pointer to device structre
*
* This function is called by OS when SST device raises
* an interrupt. This will be result of write in IPC register
* Source can be busy or done interrupt
*/
static irqreturn_t intel_sst_intr_mfld(int irq, void *context)
{
	union interrupt_reg isr;
	union ipc_header header;
	irqreturn_t retval = IRQ_HANDLED;
	struct ipc_post *msg = NULL;
	unsigned int size = 0;
	struct intel_sst_drv *drv = (struct intel_sst_drv *) context;

	/* Interrupt arrived, check src */
	isr.full = sst_shim_read(drv->shim, SST_ISRX);
	if (isr.part.done_interrupt) {
		/* Mask all interrupts till this one is processsed */
		set_imr_interrupts(drv, false);
		/* Clear done bit */
		spin_lock(&drv->ipc_spin_lock);
		header.full = sst_shim_read(drv->shim, drv->ipc_reg.ipcx);
		header.part.done = 0;
		sst_shim_write(drv->shim, drv->ipc_reg.ipcx, header.full);
		/* write 1 to clear status register */;
		isr.part.done_interrupt = 1;
		sst_shim_write(drv->shim, SST_ISRX, isr.full);
		spin_unlock(&drv->ipc_spin_lock);
		queue_work(drv->post_msg_wq, &sst_drv_ctx->ipc_post_msg.wq);

		/* Un mask done and busy intr */
		set_imr_interrupts(drv, true);
		retval = IRQ_HANDLED;
	}
	if (isr.part.busy_interrupt) {
		/* Mask all interrupts till we process it in bottom half */
		set_imr_interrupts(drv, false);
		header.full = sst_shim_read(drv->shim, drv->ipc_reg.ipcd);
		if (sst_create_ipc_msg(&msg, header.part.large)) {
			pr_err("No memory available\n");
			drv->ops->clear_interrupt();
			return IRQ_HANDLED;
		}
		if (header.part.large) {
			size = header.part.data;
			if (SST_VALIDATE_MAILBOX_SIZE(size)) {
				memcpy_fromio(msg->mailbox_data,
					drv->mailbox + drv->mailbox_recv_offset + 4, size);
			} else {
				pr_err("Mailbox not copied, payload siz is: %u\n", size);
				header.part.data = 0;
			}
		}
		msg->header = header;
		msg->is_process_reply =
				SST_IS_PROCESS_REPLY(msg->header.part.msg_id);
		spin_lock(&drv->rx_msg_lock);
		list_add_tail(&msg->node, &drv->rx_list);
		spin_unlock(&drv->rx_msg_lock);
		drv->ops->clear_interrupt();
		retval = IRQ_WAKE_THREAD;
	}
	return retval;
}

static int sst_save_dsp_context_v2(struct intel_sst_drv *sst)
{
	unsigned int pvt_id;
	struct ipc_post *msg = NULL;
	struct ipc_dsp_hdr dsp_hdr;
	struct sst_block *block;

	/*send msg to fw*/
	pvt_id = sst_assign_pvt_id(sst);
	if (sst_create_block_and_ipc_msg(&msg, true, sst, &block,
				IPC_CMD, pvt_id)) {
		pr_err("msg/block alloc failed. Not proceeding with context save\n");
		return 0;
	}

	sst_fill_header_mrfld(&msg->mrfld_header, IPC_CMD,
			      SST_TASK_ID_MEDIA, 1, pvt_id);
	msg->mrfld_header.p.header_low_payload = sizeof(dsp_hdr);
	msg->mrfld_header.p.header_high.part.res_rqd = 1;
	sst_fill_header_dsp(&dsp_hdr, IPC_PREP_D3, PIPE_RSVD, pvt_id);
	memcpy(msg->mailbox_data, &dsp_hdr, sizeof(dsp_hdr));

	sst_add_to_dispatch_list_and_post(sst, msg);
	/*wait for reply*/
	if (sst_wait_timeout(sst, block)) {
		pr_err("sst: err fw context save timeout  ...\n");
		pr_err("not suspending FW!!!");
		sst_free_block(sst, block);
		return -EIO;
	}
	if (block->ret_code) {
		pr_err("fw responded w/ error %d", block->ret_code);
		sst_free_block(sst, block);
		return -EIO;
	}

	sst_free_block(sst, block);
	return 0;
}

static int sst_save_dsp_context(struct intel_sst_drv *sst)
{
	struct snd_sst_ctxt_params fw_context;
	unsigned int pvt_id;
	struct ipc_post *msg = NULL;
	struct sst_block *block;
	pr_debug("%s: Enter\n", __func__);

	/*send msg to fw*/
	pvt_id = sst_assign_pvt_id(sst_drv_ctx);
	if (sst_create_block_and_ipc_msg(&msg, true, sst_drv_ctx, &block,
				IPC_IA_GET_FW_CTXT, pvt_id)) {
		pr_err("msg/block alloc failed. Not proceeding with context save\n");
		return -ENOMEM;
	}
	sst_fill_header(&msg->header, IPC_IA_GET_FW_CTXT, 1, pvt_id);
	msg->header.part.data = sizeof(fw_context) + sizeof(u32);
	fw_context.address = virt_to_phys((void *)sst_drv_ctx->fw_cntx);
	fw_context.size = FW_CONTEXT_MEM;
	memcpy(msg->mailbox_data, &msg->header, sizeof(u32));
	memcpy(msg->mailbox_data + sizeof(u32),
				&fw_context, sizeof(fw_context));
	sst_add_to_dispatch_list_and_post(sst, msg);
	/*wait for reply*/
	if (sst_wait_timeout(sst_drv_ctx, block))
		pr_err("sst: err fw context save timeout  ...\n");
	pr_debug("fw context saved  ...\n");
	if (block->ret_code)
		sst_drv_ctx->fw_cntx_size = 0;
	else
		sst_drv_ctx->fw_cntx_size = *sst_drv_ctx->fw_cntx;
	pr_debug("fw copied data %x\n", sst_drv_ctx->fw_cntx_size);
	sst_free_block(sst_drv_ctx, block);
	return 0;
}

static struct intel_sst_ops mrfld_ops = {
	.interrupt = intel_sst_interrupt_mrfld,
	.irq_thread = intel_sst_irq_thread_mfld,
	.clear_interrupt = intel_sst_clear_intr_mrfld,
	.start = sst_start_mrfld,
	.reset = intel_sst_reset_dsp_mrfld,
	.post_message = sst_post_message_mrfld,
	.sync_post_message = sst_sync_post_message_mrfld,
	.process_message = sst_process_message_mrfld,
	.process_reply = sst_process_reply_mrfld,
	.save_dsp_context =  sst_save_dsp_context_v2,
	.alloc_stream = sst_alloc_stream_mrfld,
	.post_download = sst_post_download_mrfld,
	.do_recovery = sst_do_recovery_mrfld,
};

static struct intel_sst_ops mrfld_32_ops = {
	.interrupt = intel_sst_intr_mfld,
	.irq_thread = intel_sst_irq_thread_mfld,
	.clear_interrupt = intel_sst_clear_intr_mfld,
	.start = sst_start_mrfld,
	.reset = intel_sst_reset_dsp_mrfld,
	.post_message = sst_post_message_mfld,
	.sync_post_message = sst_sync_post_message_mfld,
	.process_message = sst_process_message_mfld,
	.process_reply = sst_process_reply_mfld,
	.save_dsp_context =  sst_save_dsp_context,
	.restore_dsp_context = sst_restore_fw_context,
	.alloc_stream = sst_alloc_stream_ctp,
	.post_download = sst_post_download_byt,
	.do_recovery = sst_do_recovery,
};

static struct intel_sst_ops ctp_ops = {
	.interrupt = intel_sst_intr_mfld,
	.irq_thread = intel_sst_irq_thread_mfld,
	.clear_interrupt = intel_sst_clear_intr_mfld,
	.start = sst_start_mfld,
	.reset = intel_sst_reset_dsp_mfld,
	.post_message = sst_post_message_mfld,
	.sync_post_message = sst_sync_post_message_mfld,
	.process_message = sst_process_message_mfld,
	.process_reply = sst_process_reply_mfld,
	.set_bypass = intel_sst_set_bypass_mfld,
	.save_dsp_context =  sst_save_dsp_context,
	.restore_dsp_context = sst_restore_fw_context,
	.alloc_stream = sst_alloc_stream_ctp,
	.post_download = sst_post_download_ctp,
	.do_recovery = sst_do_recovery,
};

int sst_driver_ops(struct intel_sst_drv *sst)
{

	switch (sst->pci_id) {
#if 0
	case SST_MRFLD_PCI_ID:
	case PCI_DEVICE_ID_INTEL_SST_MOOR:
	case SST_CHT_PCI_ID:
		sst->tstamp = SST_TIME_STAMP_MRFLD;
		sst->ops = &mrfld_ops;

		/* Override the recovery ops for CHT platforms */
		if (sst->pci_id == SST_CHT_PCI_ID)
			sst->ops->do_recovery = sst_do_recovery;
		/* For MOFD platforms disable/enable recovery based on
		 * platform data
		 */
		if (sst->pci_id == PCI_DEVICE_ID_INTEL_SST_MOOR) {
			if (!sst->pdata->enable_recovery) {
				pr_debug("Recovery disabled for this mofd platform\n");
				sst->ops->do_recovery = sst_do_recovery;
			} else
				pr_debug("Recovery enabled for this mofd platform\n");
		}

		return 0;
#endif
	case SST_BYT_PCI_ID:
		sst->tstamp = SST_TIME_STAMP_BYT;
		sst->ops = &mrfld_32_ops;
		/* Override ops for DPCM architecture */
#ifdef CONFIG_SST_DPCM
		sst->tstamp = SST_TIME_STAMP_MRFLD;
		sst->ops = &mrfld_ops;
		sst->ops->do_recovery = sst_do_recovery;
#endif

		return 0;
	case SST_CLV_PCI_ID:
		sst->tstamp =  SST_TIME_STAMP;
		sst->ops = &ctp_ops;
		return 0;
	default:
		pr_err("SST Driver capablities missing for pci_id: %x", sst->pci_id);
		return -EINVAL;
	};
}

int sst_alloc_drv_context(struct device *dev)
{
	struct intel_sst_drv *ctx;
	mutex_lock(&drv_ctx_lock);
	if (sst_drv_ctx) {
		pr_err("Only one sst handle is supported\n");
		mutex_unlock(&drv_ctx_lock);
		return -EBUSY;
	}
	pr_debug("%s: %d", __func__, __LINE__);
	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("malloc fail\n");
		mutex_unlock(&drv_ctx_lock);
		return -ENOMEM;
	}
	sst_drv_ctx = ctx;
	mutex_unlock(&drv_ctx_lock);
	return 0;
}

static ssize_t sst_sysfs_get_recovery(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ctx->sst_state);
}


static ssize_t sst_sysfs_set_recovery(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	long val;
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	if (kstrtol(buf, 0, &val))
		return -EINVAL;

	if (val == 1) {
		if (!atomic_read(&ctx->pm_usage_count)) {
			pr_debug("%s: set sst state to RESET...\n", __func__);
			sst_set_fw_state_locked(ctx, SST_RESET);
		} else {
			pr_err("%s: not setting sst state... %d\n", __func__,
					atomic_read(&ctx->pm_usage_count));
			pr_err("Unrecoverable state....\n");
			BUG();
			return -EPERM;
		}
	}

	return len;
}

static DEVICE_ATTR(audio_recovery, S_IRUGO | S_IWUSR,
			sst_sysfs_get_recovery, sst_sysfs_set_recovery);

int sst_request_firmware_async(struct intel_sst_drv *ctx)
{
	int ret = 0;

	snprintf(ctx->firmware_name, sizeof(ctx->firmware_name),
			"%s%04x%s", "fw_sst_",
			ctx->pci_id, ".bin");
	pr_debug("Requesting FW %s now...\n", ctx->firmware_name);

	trace_sst_fw_download("Request firmware async", ctx->sst_state);

	ret = request_firmware_nowait(THIS_MODULE, 1, ctx->firmware_name,
			ctx->dev, GFP_KERNEL, ctx, sst_firmware_load_cb);
	if (ret)
		pr_err("could not load firmware %s error %d\n", ctx->firmware_name, ret);

	return ret;
}
/*
* intel_sst_probe - PCI probe function
*
* @pci:	PCI device structure
* @pci_id: PCI device ID structure
*
* This function is called by OS when a device is found
* This enables the device, interrupt etc
*/
static int intel_sst_probe(struct pci_dev *pci,
			const struct pci_device_id *pci_id)
{
	int i, ret = 0;
	struct intel_sst_ops *ops;
	struct sst_platform_info *sst_pdata = pci->dev.platform_data;
	int ddr_base;
	u32 ssp_base_add;
	u32 dma_base_add;
	u32 len;



	pr_debug("Probe for DID %x\n", pci->device);
	ret = sst_alloc_drv_context(&pci->dev);
	if (ret)
		return ret;

	sst_drv_ctx->dev = &pci->dev;
	sst_drv_ctx->pci_id = pci->device;
	if (!sst_pdata)
		return -EINVAL;
	sst_drv_ctx->pdata = sst_pdata;

	if (!sst_drv_ctx->pdata->probe_data)
		return -EINVAL;
	memcpy(&sst_drv_ctx->info, sst_drv_ctx->pdata->probe_data,
					sizeof(sst_drv_ctx->info));

	sst_drv_ctx->use_32bit_ops = sst_drv_ctx->pdata->ipc_info->use_32bit_ops;
	sst_drv_ctx->mailbox_recv_offset = sst_drv_ctx->pdata->ipc_info->mbox_recv_off;

	if (0 != sst_driver_ops(sst_drv_ctx))
		return -EINVAL;
	ops = sst_drv_ctx->ops;
	mutex_init(&sst_drv_ctx->stream_lock);
	mutex_init(&sst_drv_ctx->sst_lock);
	mutex_init(&sst_drv_ctx->mixer_ctrl_lock);
	mutex_init(&sst_drv_ctx->csr_lock);

	sst_drv_ctx->stream_cnt = 0;
	sst_drv_ctx->fw_in_mem = NULL;
	sst_drv_ctx->vcache.file1_in_mem = NULL;
	sst_drv_ctx->vcache.file2_in_mem = NULL;
	sst_drv_ctx->vcache.size1 = 0;
	sst_drv_ctx->vcache.size2 = 0;

	/* we use dma, so set to 1*/
	sst_drv_ctx->use_dma = 0;
	sst_drv_ctx->use_lli = 0;

	INIT_LIST_HEAD(&sst_drv_ctx->memcpy_list);
	INIT_LIST_HEAD(&sst_drv_ctx->libmemcpy_list);

	INIT_LIST_HEAD(&sst_drv_ctx->ipc_dispatch_list);
	INIT_LIST_HEAD(&sst_drv_ctx->block_list);
	INIT_LIST_HEAD(&sst_drv_ctx->rx_list);
	INIT_WORK(&sst_drv_ctx->ipc_post_msg.wq, ops->post_message);
	init_waitqueue_head(&sst_drv_ctx->wait_queue);

	sst_drv_ctx->mad_wq = create_singlethread_workqueue("sst_mad_wq");
	if (!sst_drv_ctx->mad_wq) {
		ret = -EINVAL;
		goto do_free_drv_ctx;
	}
	sst_drv_ctx->post_msg_wq =
		create_singlethread_workqueue("sst_post_msg_wq");
	if (!sst_drv_ctx->post_msg_wq) {
		ret = -EINVAL;
		goto free_mad_wq;
	}

	spin_lock_init(&sst_drv_ctx->ipc_spin_lock);
	spin_lock_init(&sst_drv_ctx->block_lock);
	spin_lock_init(&sst_drv_ctx->pvt_id_lock);
	spin_lock_init(&sst_drv_ctx->rx_msg_lock);

	sst_drv_ctx->ipc_reg.ipcx = SST_IPCX + sst_drv_ctx->pdata->ipc_info->ipc_offset;
	sst_drv_ctx->ipc_reg.ipcd = SST_IPCD + sst_drv_ctx->pdata->ipc_info->ipc_offset;
	pr_debug("ipcx 0x%x ipxd 0x%x", sst_drv_ctx->ipc_reg.ipcx,
					sst_drv_ctx->ipc_reg.ipcd);

	pr_info("Got drv data max stream %d\n",
				sst_drv_ctx->info.max_streams);
	for (i = 1; i <= sst_drv_ctx->info.max_streams; i++) {
		struct stream_info *stream = &sst_drv_ctx->streams[i];
		memset(stream, 0, sizeof(*stream));
		stream->pipe_id = PIPE_RSVD;
		mutex_init(&stream->lock);
	}

	ret = sst_request_firmware_async(sst_drv_ctx);
	if (ret) {
		pr_err("Firmware download failed:%d\n", ret);
		goto do_free_mem;
	}
	/* Init the device */
	ret = pci_enable_device(pci);
	if (ret) {
		pr_err("device can't be enabled\n");
		goto do_free_mem;
	}
	sst_drv_ctx->pci = pci_dev_get(pci);
	ret = pci_request_regions(pci, SST_DRV_NAME);
	if (ret)
		goto do_disable_device;
	/* map registers */
	/* SST Shim */

	if (sst_drv_ctx->pci_id == SST_MRFLD_PCI_ID) { /* ||
			(sst_drv_ctx->pci_id == PCI_DEVICE_ID_INTEL_SST_MOOR)) {*/
		sst_drv_ctx->ddr_base = pci_resource_start(pci, 0);
		/*
		* check that the relocated IMR base matches with FW Binary
		* put temporary check till better soln is available for FW
		*/
		ddr_base = relocate_imr_addr_mrfld(sst_drv_ctx->ddr_base);
		if (!sst_drv_ctx->pdata->lib_info) {
			pr_err("%s:lib_info pointer NULL\n", __func__);
			ret = -EINVAL;
			goto do_release_regions;
		}
		if (ddr_base != sst_drv_ctx->pdata->lib_info->mod_base) {
			pr_err("FW LSP DDR BASE does not match with IFWI\n");
			ret = -EINVAL;
			goto do_release_regions;
		}
		sst_drv_ctx->ddr_end = pci_resource_end(pci, 0);

		sst_drv_ctx->ddr = pci_ioremap_bar(pci, 0);
		if (!sst_drv_ctx->ddr) {
			ret = -EINVAL;
			goto do_unmap_ddr;
		}
		pr_debug("sst: DDR Ptr %p\n", sst_drv_ctx->ddr);
	} else {
		sst_drv_ctx->ddr = NULL;
	}

	/* SHIM */
	sst_drv_ctx->shim_phy_add = pci_resource_start(pci, 1);
	sst_drv_ctx->shim = pci_ioremap_bar(pci, 1);
	if (!sst_drv_ctx->shim) {
		ret = -EINVAL;
		goto do_release_regions;
	}
	pr_debug("SST Shim Ptr %p\n", sst_drv_ctx->shim);

	/* Shared SRAM */
	sst_drv_ctx->mailbox_add = pci_resource_start(pci, 2);
	sst_drv_ctx->mailbox = pci_ioremap_bar(pci, 2);
	if (!sst_drv_ctx->mailbox) {
		ret = -EINVAL;
		goto do_unmap_shim;
	}
	pr_debug("SRAM Ptr %p\n", sst_drv_ctx->mailbox);

	/* IRAM */
	sst_drv_ctx->iram_end = pci_resource_end(pci, 3);
	sst_drv_ctx->iram_base = pci_resource_start(pci, 3);
	sst_drv_ctx->iram = pci_ioremap_bar(pci, 3);
	if (!sst_drv_ctx->iram) {
		ret = -EINVAL;
		goto do_unmap_sram;
	}
	pr_debug("IRAM Ptr %p\n", sst_drv_ctx->iram);

	/* DRAM */
	sst_drv_ctx->dram_end = pci_resource_end(pci, 4);
	sst_drv_ctx->dram_base = pci_resource_start(pci, 4);
	sst_drv_ctx->dram = pci_ioremap_bar(pci, 4);
	if (!sst_drv_ctx->dram) {
		ret = -EINVAL;
		goto do_unmap_iram;
	}
	pr_debug("DRAM Ptr %p\n", sst_drv_ctx->dram);

	if ((sst_pdata->pdata != NULL) &&
			(sst_pdata->debugfs_data != NULL)) {
		if (sst_pdata->ssp_data != NULL) {
			/* SSP Register */
			ssp_base_add = sst_pdata->ssp_data->base_add;
			len = sst_pdata->debugfs_data->ssp_reg_size;
			for (i = 0; i < sst_pdata->debugfs_data->num_ssp; i++) {
				sst_drv_ctx->debugfs.ssp[i] =
					devm_ioremap(&pci->dev,
						ssp_base_add + (len * i), len);
				if (!sst_drv_ctx->debugfs.ssp[i]) {
					pr_warn("ssp ioremap failed\n");
					continue;
				}

				pr_debug("\n ssp io 0x%p ssp 0x%x size 0x%x",
					sst_drv_ctx->debugfs.ssp[i],
						ssp_base_add, len);
			}
		}

		/* DMA Register */
		dma_base_add = sst_pdata->pdata->sst_dma_base[0];
		len = sst_pdata->debugfs_data->dma_reg_size;
		for (i = 0; i < sst_pdata->debugfs_data->num_dma; i++) {
			sst_drv_ctx->debugfs.dma_reg[i] =
				devm_ioremap(&pci->dev,
					dma_base_add + (len * i), len);
			if (!sst_drv_ctx->debugfs.dma_reg[i]) {
				pr_warn("dma ioremap failed\n");
				continue;
			}

			pr_debug("\n dma io 0x%p ssp 0x%x size 0x%x",
				sst_drv_ctx->debugfs.dma_reg[i],
					dma_base_add, len);
		}
	}

	/* Do not access iram/dram etc before LPE is reset */

	sst_drv_ctx->dump_buf.iram_buf.size = pci_resource_len(pci, 3);
	sst_drv_ctx->dump_buf.iram_buf.buf = kzalloc(sst_drv_ctx->dump_buf.iram_buf.size,
						GFP_KERNEL);
	if (!sst_drv_ctx->dump_buf.iram_buf.buf) {
		pr_err("%s: no memory\n", __func__);
		ret = -ENOMEM;
		goto do_unmap_dram;
	}

	sst_drv_ctx->dump_buf.dram_buf.size = pci_resource_len(pci, 4);
	sst_drv_ctx->dump_buf.dram_buf.buf = kzalloc(sst_drv_ctx->dump_buf.dram_buf.size,
						GFP_KERNEL);
	if (!sst_drv_ctx->dump_buf.dram_buf.buf) {
		pr_err("%s: no memory\n", __func__);
		ret = -ENOMEM;
		goto do_free_iram_buf;
	}

	pr_debug("\niram len 0x%x dram len 0x%x",
			sst_drv_ctx->dump_buf.iram_buf.size,
			sst_drv_ctx->dump_buf.dram_buf.size);

	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID) {
		sst_drv_ctx->probe_bytes = kzalloc(SST_MAX_BIN_BYTES, GFP_KERNEL);
		if (!sst_drv_ctx->probe_bytes) {
			pr_err("%s: no memory\n", __func__);
			ret = -ENOMEM;
			goto do_free_dram_buf;
		}
	}

	sst_set_fw_state_locked(sst_drv_ctx, SST_RESET);
	sst_drv_ctx->irq_num = pci->irq;
	/* Register the ISR */
	ret = request_threaded_irq(pci->irq, sst_drv_ctx->ops->interrupt,
		sst_drv_ctx->ops->irq_thread, 0, SST_DRV_NAME,
		sst_drv_ctx);
	if (ret)
		goto do_free_probe_bytes;
	pr_debug("Registered IRQ 0x%x\n", pci->irq);

	/*Register LPE Control as misc driver*/
	ret = misc_register(&lpe_ctrl);
	if (ret) {
		pr_err("couldn't register control device\n");
		goto do_free_irq;
	}
	/* default intr are unmasked so set this as masked */
	if (sst_drv_ctx->pci_id == SST_MRFLD_PCI_ID) /* ||
			(sst_drv_ctx->pci_id == PCI_DEVICE_ID_INTEL_SST_MOOR)) */
		sst_shim_write64(sst_drv_ctx->shim, SST_IMRX, 0xFFFF0038);

	if (sst_drv_ctx->use_32bit_ops) {
		pr_debug("allocate mem for context save/restore\n ");
		/*allocate mem for fw context save during suspend*/
		sst_drv_ctx->fw_cntx = kzalloc(FW_CONTEXT_MEM, GFP_KERNEL);
		if (!sst_drv_ctx->fw_cntx) {
			ret = -ENOMEM;
			goto do_free_misc;
		}
		/*setting zero as that is valid mem to restore*/
		sst_drv_ctx->fw_cntx_size = 0;
	}
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID) {
		u32 csr;
		u32 csr2;
		u32 clkctl;

		/*set lpe start clock and ram size*/
		csr = sst_shim_read(sst_drv_ctx->shim, SST_CSR);
		csr |= 0x30000;
		/*make sure clksel set to OSC for SSP0,1 (default)*/
		csr &= 0xFFFFFFF3;
		sst_shim_write(sst_drv_ctx->shim, SST_CSR, csr);

		/*set clock output enable for SSP0,1,3*/
		clkctl = sst_shim_read(sst_drv_ctx->shim, SST_CLKCTL);
		if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
			clkctl |= (0x7 << 16);
		else
			clkctl |= ((1<<16)|(1<<17));
		sst_shim_write(sst_drv_ctx->shim, SST_CLKCTL, clkctl);

		/* set SSP0 & SSP1 disable DMA Finish*/
		csr2 = sst_shim_read(sst_drv_ctx->shim, SST_CSR2);
		/*set SSP3 disable DMA finsh for SSSP3 */
		csr2 |= BIT(1)|BIT(2);
		sst_shim_write(sst_drv_ctx->shim, SST_CSR2, csr2);
	}
	if (sst_drv_ctx->pdata->ssp_data) {
		if (sst_drv_ctx->pdata->ssp_data->gpio_in_use)
			sst_set_gpio_conf(&sst_drv_ctx->pdata->ssp_data->gpio);
	}
	pci_set_drvdata(pci, sst_drv_ctx);
	pm_runtime_allow(sst_drv_ctx->dev);
	pm_runtime_put_noidle(sst_drv_ctx->dev);
	register_sst(sst_drv_ctx->dev);
	sst_debugfs_init(sst_drv_ctx);
	sst_drv_ctx->qos = kzalloc(sizeof(struct pm_qos_request),
				GFP_KERNEL);
	if (!sst_drv_ctx->qos) {
		ret = -EINVAL;
		goto do_free_misc;
	}
	pm_qos_add_request(sst_drv_ctx->qos, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	ret = device_create_file(sst_drv_ctx->dev, &dev_attr_audio_recovery);
	if (ret) {
		pr_err("could not create sysfs %s file\n",
			dev_attr_audio_recovery.attr.name);
		goto do_free_qos;
	}

	pr_info("%s successfully done!\n", __func__);
	return ret;

do_free_qos:
	pm_qos_remove_request(sst_drv_ctx->qos);
	kfree(sst_drv_ctx->qos);
do_free_misc:
	misc_deregister(&lpe_ctrl);
do_free_irq:
	free_irq(pci->irq, sst_drv_ctx);
do_free_probe_bytes:
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		kfree(sst_drv_ctx->probe_bytes);
do_free_dram_buf:
#ifdef CONFIG_DEBUG_FS
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		kfree(sst_drv_ctx->dump_buf.dram_buf.buf);
do_free_iram_buf:
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		kfree(sst_drv_ctx->dump_buf.iram_buf.buf);
#endif
do_unmap_dram:
	iounmap(sst_drv_ctx->dram);
do_unmap_iram:
	iounmap(sst_drv_ctx->iram);
do_unmap_sram:
	iounmap(sst_drv_ctx->mailbox);
do_unmap_shim:
	iounmap(sst_drv_ctx->shim);

do_unmap_ddr:
	if (sst_drv_ctx->ddr)
		iounmap(sst_drv_ctx->ddr);

do_release_regions:
	pci_release_regions(pci);
do_disable_device:
	pci_disable_device(pci);
do_free_mem:
	destroy_workqueue(sst_drv_ctx->post_msg_wq);
free_mad_wq:
	destroy_workqueue(sst_drv_ctx->mad_wq);
do_free_drv_ctx:
	sst_drv_ctx = NULL;
	pr_err("Probe failed with %d\n", ret);
	return ret;
}

/**
* intel_sst_remove - PCI remove function
*
* @pci:	PCI device structure
*
* This function is called by OS when a device is unloaded
* This frees the interrupt etc
*/
static void intel_sst_remove(struct pci_dev *pci)
{
	struct intel_sst_drv *sst_drv_ctx = pci_get_drvdata(pci);
	sst_debugfs_exit(sst_drv_ctx);
	pm_runtime_get_noresume(sst_drv_ctx->dev);
	pm_runtime_forbid(sst_drv_ctx->dev);
	unregister_sst(sst_drv_ctx->dev);
	pci_dev_put(sst_drv_ctx->pci);
	sst_set_fw_state_locked(sst_drv_ctx, SST_SHUTDOWN);
	misc_deregister(&lpe_ctrl);
	free_irq(pci->irq, sst_drv_ctx);

	iounmap(sst_drv_ctx->dram);
	iounmap(sst_drv_ctx->iram);
	iounmap(sst_drv_ctx->mailbox);
	iounmap(sst_drv_ctx->shim);
#ifdef CONFIG_DEBUG_FS
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID) {
		kfree(sst_drv_ctx->dump_buf.iram_buf.buf);
		kfree(sst_drv_ctx->dump_buf.dram_buf.buf);
	}
#endif
	if (sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		kfree(sst_drv_ctx->probe_bytes);

	device_remove_file(sst_drv_ctx->dev, &dev_attr_audio_recovery);
	kfree(sst_drv_ctx->fw_cntx);
	kfree(sst_drv_ctx->runtime_param.param.addr);
	flush_scheduled_work();
	destroy_workqueue(sst_drv_ctx->post_msg_wq);
	destroy_workqueue(sst_drv_ctx->mad_wq);
	pm_qos_remove_request(sst_drv_ctx->qos);
	kfree(sst_drv_ctx->qos);
	kfree(sst_drv_ctx->fw_sg_list.src);
	kfree(sst_drv_ctx->fw_sg_list.dst);
	sst_drv_ctx->fw_sg_list.list_len = 0;
	kfree(sst_drv_ctx->fw_in_mem);
	sst_drv_ctx->fw_in_mem = NULL;
	sst_memcpy_free_resources();
	sst_drv_ctx = NULL;
	pci_release_regions(pci);
	pci_disable_device(pci);
	pci_set_drvdata(pci, NULL);
}

inline void sst_save_shim64(struct intel_sst_drv *ctx,
			    void __iomem *shim,
			    struct sst_shim_regs64 *shim_regs)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&ctx->ipc_spin_lock, irq_flags);

	shim_regs->csr = sst_shim_read64(shim, SST_CSR),
	shim_regs->pisr = sst_shim_read64(shim, SST_PISR),
	shim_regs->pimr = sst_shim_read64(shim, SST_PIMR),
	shim_regs->isrx = sst_shim_read64(shim, SST_ISRX),
	shim_regs->isrd = sst_shim_read64(shim, SST_ISRD),
	shim_regs->imrx = sst_shim_read64(shim, SST_IMRX),
	shim_regs->imrd = sst_shim_read64(shim, SST_IMRD),
	shim_regs->ipcx = sst_shim_read64(shim, ctx->ipc_reg.ipcx),
	shim_regs->ipcd = sst_shim_read64(shim, ctx->ipc_reg.ipcd),
	shim_regs->isrsc = sst_shim_read64(shim, SST_ISRSC),
	shim_regs->isrlpesc = sst_shim_read64(shim, SST_ISRLPESC),
	shim_regs->imrsc = sst_shim_read64(shim, SST_IMRSC),
	shim_regs->imrlpesc = sst_shim_read64(shim, SST_IMRLPESC),
	shim_regs->ipcsc = sst_shim_read64(shim, SST_IPCSC),
	shim_regs->ipclpesc = sst_shim_read64(shim, SST_IPCLPESC),
	shim_regs->clkctl = sst_shim_read64(shim, SST_CLKCTL),
	shim_regs->csr2 = sst_shim_read64(shim, SST_CSR2);

	spin_unlock_irqrestore(&ctx->ipc_spin_lock, irq_flags);
}

static inline void sst_restore_shim64(struct intel_sst_drv *ctx,
				      void __iomem *shim,
				      struct sst_shim_regs64 *shim_regs)
{
	unsigned long irq_flags;
	spin_lock_irqsave(&ctx->ipc_spin_lock, irq_flags);
	sst_shim_write64(shim, SST_IMRX, shim_regs->imrx),
	spin_unlock_irqrestore(&ctx->ipc_spin_lock, irq_flags);
}

/*
 * The runtime_suspend/resume is pretty much similar to the legacy
 * suspend/resume with the noted exception below: The PCI core takes care of
 * taking the system through D3hot and restoring it back to D0 and so there is
 * no need to duplicate that here.
 */
static int intel_sst_runtime_suspend(struct device *dev)
{
	union config_status_reg csr;
	int ret = 0;
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	pr_info("runtime_suspend called\n");
	if (ctx->sst_state == SST_RESET) {
		pr_debug("LPE is already in RESET state, No action");
		return 0;
	}
	/*save fw context*/
	if (ctx->ops->save_dsp_context(ctx))
		return -EBUSY;

	if (ctx->pci_id == SST_CLV_PCI_ID) {
		/*Assert RESET on LPE Processor*/
		csr.full = sst_shim_read(ctx->shim, SST_CSR);
		ctx->csr_value = csr.full;
		csr.full = csr.full | 0x2;
		sst_shim_write(ctx->shim, SST_CSR, csr.full);
	}

	/* Move the SST state to Reset */
	sst_set_fw_state_locked(ctx, SST_RESET);

	flush_workqueue(ctx->post_msg_wq);
	synchronize_irq(ctx->irq_num);

	if (ctx->pci_id == SST_BYT_PCI_ID || ctx->pci_id == SST_CHT_PCI_ID) {
		/* save the shim registers because PMC doesn't save state */
		sst_save_shim64(ctx, ctx->shim, ctx->shim_regs64);
	}
	return ret;
}

static int intel_sst_runtime_resume(struct device *dev)
{
	u32 csr;
	int ret = 0;
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	pr_info("runtime_resume called\n");

	if (ctx->pci_id == SST_BYT_PCI_ID || ctx->pci_id == SST_CHT_PCI_ID) {
		/* wait for device power up a/c to PCI spec */
		usleep_range(10000, 11000);
		sst_restore_shim64(ctx, ctx->shim, ctx->shim_regs64);
	}

	if (ctx->pci_id == SST_CLV_PCI_ID) {
		csr = sst_shim_read(ctx->shim, SST_CSR);
		/*
		 * To restore the csr_value after S0ix and S3 states.
		 * The value 0x30000 is to enable LPE dram high and low addresses.
		 * Reference:
		 * Penwell Audio Voice Module HAS 1.61 Section - 13.12.1 -
		 * CSR - Configuration and Status Register.
		 */
		csr |= (ctx->csr_value | 0x30000);
		sst_shim_write(ctx->shim, SST_CSR, csr);
		if (sst_drv_ctx->pdata->ssp_data) {
			if (ctx->pdata->ssp_data->gpio_in_use)
				sst_set_gpio_conf(&ctx->pdata->ssp_data->gpio);
		}
	}
	/* When fw_clear_cache is set, clear the cached firmware copy */
	/* fw_clear_cache is set through debugfs support */
	if (atomic_read(&ctx->fw_clear_cache) && ctx->fw_in_mem) {
		pr_debug("Clearing the cached firmware\n");
		kfree(ctx->fw_in_mem);
		ctx->fw_in_mem = NULL;
		atomic_set(&ctx->fw_clear_cache, 0);
	}

	sst_set_fw_state_locked(ctx, SST_RESET);

	return ret;
}

static int intel_sst_suspend(struct device *dev)
{
	int retval = 0, usage_count;
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	usage_count = atomic_read(&ctx->pm_usage_count);
	if (usage_count) {
		pr_err("Ret error for suspend:%d\n", usage_count);
		return -EBUSY;
	}
	retval = intel_sst_runtime_suspend(dev);

	return retval;
}

static int intel_sst_runtime_idle(struct device *dev)
{
	struct intel_sst_drv *ctx = dev_get_drvdata(dev);

	pr_info("runtime_idle called\n");
	if (ctx->sst_state != SST_RESET) {
		pm_schedule_suspend(dev, SST_SUSPEND_DELAY);
		return -EBUSY;
	} else {
		return 0;
	}
	return -EBUSY;

}

static void sst_do_shutdown(struct intel_sst_drv *ctx)
{
	int retval = 0;
	unsigned int pvt_id;
	struct ipc_post *msg = NULL;
	struct sst_block *block = NULL;

	pr_debug(" %s called\n", __func__);
	if ((atomic_read(&ctx->pm_usage_count) == 0) ||
		ctx->sst_state == SST_RESET) {
		sst_set_fw_state_locked(ctx, SST_SHUTDOWN);
		pr_debug("sst is already in suspended/RESET state\n");
		return;
	}
	if (!ctx->use_32bit_ops)
		return;

	sst_set_fw_state_locked(ctx, SST_SHUTDOWN);
	flush_workqueue(ctx->post_msg_wq);
	pvt_id = sst_assign_pvt_id(ctx);
	retval = sst_create_block_and_ipc_msg(&msg, false,
			ctx, &block,
			IPC_IA_PREPARE_SHUTDOWN, pvt_id);
	if (retval) {
		pr_err("sst_create_block returned error!\n");
		return;
	}
	sst_fill_header(&msg->header, IPC_IA_PREPARE_SHUTDOWN, 0, pvt_id);
	sst_add_to_dispatch_list_and_post(ctx, msg);
	sst_wait_timeout(ctx, block);
	sst_free_block(ctx, block);
}


/**
* sst_pci_shutdown - PCI shutdown function
*
* @pci:        PCI device structure
*
* This function is called by OS when a device is shutdown/reboot
*
*/

static void sst_pci_shutdown(struct pci_dev *pci)
{
	struct intel_sst_drv *ctx = pci_get_drvdata(pci);

	pr_debug(" %s called\n", __func__);

	sst_do_shutdown(ctx);
	disable_irq_nosync(pci->irq);
}

/**
* sst_acpi_shutdown - platform shutdown function
*
* @pci:        Platform device structure
*
* This function is called by OS when a device is shutdown/reboot
*
*/
static void sst_acpi_shutdown(struct platform_device *pdev)
{
	struct intel_sst_drv *ctx = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	pr_debug(" %s called\n", __func__);

	sst_do_shutdown(ctx);
	disable_irq_nosync(irq);
}

static const struct dev_pm_ops intel_sst_pm = {
	.suspend = intel_sst_suspend,
	.resume = intel_sst_runtime_resume,
	.runtime_suspend = intel_sst_runtime_suspend,
	.runtime_resume = intel_sst_runtime_resume,
	.runtime_idle = intel_sst_runtime_idle,
};

static const struct acpi_device_id sst_acpi_ids[];

struct sst_platform_info *sst_get_acpi_driver_data(const char *hid)
{
	const struct acpi_device_id *id;

	pr_debug("%s", __func__);
	for (id = sst_acpi_ids; id->id[0]; id++)
		if (!strncmp(id->id, hid, 16))
			return (struct sst_platform_info *)id->driver_data;
	return NULL;
}

/* PCI Routines */
static DEFINE_PCI_DEVICE_TABLE(intel_sst_ids) = {
	{ PCI_VDEVICE(INTEL, SST_CLV_PCI_ID), 0},
	{ PCI_VDEVICE(INTEL, SST_MRFLD_PCI_ID), 0},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, intel_sst_ids);

static const struct acpi_device_id sst_acpi_ids[] = {
	{ "LPE0F28",  (kernel_ulong_t) &byt_rvp_platform_data },
	{ "LPE0F281", (kernel_ulong_t) &byt_ffrd8_platform_data },
	{ "80860F28", (kernel_ulong_t) &byt_ffrd8_platform_data },
	{ "808622A8", (kernel_ulong_t) &cht_platform_data },
	{ },
};
MODULE_DEVICE_TABLE(acpi, sst_acpi_ids);

static struct pci_driver driver = {
	.name = SST_DRV_NAME,
	.id_table = intel_sst_ids,
	.probe = intel_sst_probe,
	.remove = intel_sst_remove,
	.shutdown = sst_pci_shutdown,
#ifdef CONFIG_PM
	.driver = {
		.pm = &intel_sst_pm,
	},
#endif
};

static struct platform_driver sst_acpi_driver = {
	.driver = {
		.name			= "intel_sst_acpi",
		.owner			= THIS_MODULE,
		.acpi_match_table	= ACPI_PTR(sst_acpi_ids),
		.pm			= &intel_sst_pm,
	},
	.probe	= sst_acpi_probe,
	.remove	= sst_acpi_remove,
	.shutdown = sst_acpi_shutdown,
};


/**
* intel_sst_init - Module init function
*
* Registers with PCI
* Registers with /dev
* Init all data strutures
*/
static int __init intel_sst_init(void)
{
	/* Init all variables, data structure etc....*/
	int ret = 0;
	pr_info("INFO: ******** SST DRIVER loading.. Ver: %s\n",
				       SST_DRIVER_VERSION);

	mutex_init(&drv_ctx_lock);
	/* Register with PCI */
	ret = pci_register_driver(&driver);
	if (ret)
		pr_err("PCI register failed\n");

	ret = platform_driver_register(&sst_acpi_driver);
	if (ret)
		pr_err("ACPI register failed\n");
	return ret;
}

/**
* intel_sst_exit - Module exit function
*
* Unregisters with PCI
* Unregisters with /dev
* Frees all data strutures
*/
static void __exit intel_sst_exit(void)
{
	pci_unregister_driver(&driver);
	platform_driver_unregister(&sst_acpi_driver);

	pr_debug("driver unloaded\n");
	sst_drv_ctx = NULL;
	return;
}

module_init(intel_sst_init);
module_exit(intel_sst_exit);
