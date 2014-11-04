/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
#include <linux/firmware.h>
#include <linux/intel_mid_pm.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>

#include <asm/intel-mid.h>

#include <media/v4l2-event.h>
#include <media/videobuf-vmalloc.h>

#include "atomisp_cmd.h"
#include "atomisp_common.h"
#include "atomisp_fops.h"
#include "atomisp_internal.h"
#include "atomisp_ioctl.h"
#include "atomisp-regs.h"
#include "atomisp_tables.h"
#include "atomisp_acc.h"
#include "atomisp_compat.h"
#include "atomisp_subdev.h"

#include "hrt/hive_isp_css_mm_hrt.h"

#include "sh_css_hrt.h"
#include "sh_css_defs.h"
#include "system_global.h"
#include "sh_css_internal.h"
#include "sh_css_sp.h"
#include "gp_device.h"
#include "device_access.h"
#include "irq.h"

#ifdef CSS20
#ifndef CSS21
#include "ia_css_accelerate.h"
#endif /* !CSS21 */
#include "ia_css_types.h"
#else /* CSS20 */
#include "sh_css_accelerate.h"
#include "sh_css.h"
#include "sh_css_types.h"
#endif /* CSS20 */

#include "hrt/bits.h"

/* We should never need to run the flash for more than 2 frames.
 * At 15fps this means 133ms. We set the timeout a bit longer.
 * Each flash driver is supposed to set its own timeout, but
 * just in case someone else changed the timeout, we set it
 * here to make sure we don't damage the flash hardware. */
#define FLASH_TIMEOUT 800 /* ms */

union host {
	struct {
		void *kernel_ptr;
		void __user *user_ptr;
		int size;
	} scalar;
	struct {
		void *hmm_ptr;
	} ptr;
};

/*
 * atomisp_kernel_malloc: chooses whether kmalloc() or vmalloc() is preferable.
 *
 * It is also a wrap functions to pass into css framework.
 */
void *atomisp_kernel_malloc(size_t bytes)
{
	/* vmalloc() is preferable if allocating more than 1 page */
	if (bytes > PAGE_SIZE)
		return vmalloc(bytes);

	return kmalloc(bytes, GFP_KERNEL);
}

/*
 * atomisp_kernel_zalloc: chooses whether set 0 to the allocated memory.
 *
 * It is also a wrap functions to pass into css framework.
 */
void *atomisp_kernel_zalloc(size_t bytes, bool zero_mem)
{
	void *ptr = atomisp_kernel_malloc(bytes);

	if (ptr && zero_mem)
		memset(ptr, 0, bytes);

	return ptr;
}

/*
 * Free buffer allocated with atomisp_kernel_malloc()/atomisp_kernel_zalloc
 * helper
 */
void atomisp_kernel_free(void *ptr)
{
	/* Verify if buffer was allocated by vmalloc() or kmalloc() */
	if (is_vmalloc_addr(ptr))
		vfree(ptr);
	else
		kfree(ptr);
}

/*
 * get sensor:dis71430/ov2720 related info from v4l2_subdev->priv data field.
 * subdev->priv is set in mrst.c
 */
struct camera_mipi_info *atomisp_to_sensor_mipi_info(struct v4l2_subdev *sd)
{
	return (struct camera_mipi_info *)v4l2_get_subdev_hostdata(sd);
}

/*
 * get struct atomisp_video_pipe from v4l2 video_device
 */
struct atomisp_video_pipe *atomisp_to_video_pipe(struct video_device *dev)
{
	return (struct atomisp_video_pipe *)
	    container_of(dev, struct atomisp_video_pipe, vdev);
}

/* This is just a draft rules, should be tuned when sensor is ready*/
static struct atomisp_freq_scaling_rule dfs_rules_isp2400[] = {
	/*
	 * TODO: SDV maybe have to use 457MHz on TNG B0,
	 * add 457MHz option later together with SDV.
	 */
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_400MHZ,
		.run_mode = ATOMISP_RUN_MODE_VIDEO,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_400MHZ,
		.run_mode = ATOMISP_RUN_MODE_STILL_CAPTURE,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_400MHZ,
		.run_mode = ATOMISP_RUN_MODE_CONTINUOUS_CAPTURE,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_400MHZ,
		.run_mode = ATOMISP_RUN_MODE_PREVIEW,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_457MHZ,
		.run_mode = ATOMISP_RUN_MODE_SDV,
	},
};

static struct atomisp_freq_scaling_rule dfs_rules_isp2401[] = {
	/*
	 * TODO: SDV maybe have to use 457MHz on TNG B0,
	 * add 457MHz option later together with SDV.
	 */
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_320MHZ,
		.run_mode = ATOMISP_RUN_MODE_VIDEO,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_320MHZ,
		.run_mode = ATOMISP_RUN_MODE_STILL_CAPTURE,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_320MHZ,
		.run_mode = ATOMISP_RUN_MODE_CONTINUOUS_CAPTURE,
	},
	{
		.width = ISP_FREQ_RULE_ANY,
		.height = ISP_FREQ_RULE_ANY,
		.fps = ISP_FREQ_RULE_ANY,
		.isp_freq = ISP_FREQ_320MHZ,
		.run_mode = ATOMISP_RUN_MODE_PREVIEW,
	},
};

static unsigned short atomisp_get_sensor_fps(struct atomisp_sub_device *asd)
{
	struct v4l2_subdev_frame_interval frame_interval;
	struct atomisp_device *isp = asd->isp;
	unsigned short fps;

	if (v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
		video, g_frame_interval, &frame_interval)) {
		fps = 0;
	} else {
		if (frame_interval.interval.numerator)
			fps = frame_interval.interval.denominator /
			    frame_interval.interval.numerator;
		else
			fps = 0;
	}
	return fps;
}
/*
 * DFS progress is shown as follows:
 * 1. Target frequency is calculated according to FPS/Resolution/ISP running
 *    mode.
 * 2. Ratio is calculated using formula: 2 * HPLL / target frequency - 1
 *    with proper rounding.
 * 3. Set ratio to ISPFREQ40, 1 to FREQVALID and ISPFREQGUAR40
 *    to 200MHz in ISPSSPM1.
 * 4. Wait for FREQVALID to be cleared by P-Unit.
 * 5. Wait for field ISPFREQSTAT40 in ISPSSPM1 turn to ratio set in 3.
 */
static int write_target_freq_to_hw(struct atomisp_device *isp,
				   unsigned int new_freq)
{
	unsigned int ratio, timeout;
	u32 isp_sspm1 = 0;

	isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
	if (isp_sspm1 & ISP_FREQ_VALID_MASK) {
		dev_dbg(isp->dev, "clearing ISPSSPM1 valid bit.\n");
		intel_mid_msgbus_write32(PUNIT_PORT, ISPSSPM1,
				    isp_sspm1 & ~(1 << ISP_FREQ_VALID_OFFSET));
	}

	ratio =  (2 * HPLL_FREQ + new_freq / 2) / new_freq - 1;
	isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
	isp_sspm1 &= ~(0x1F << ISP_REQ_FREQ_OFFSET);
	intel_mid_msgbus_write32(PUNIT_PORT, ISPSSPM1,
				   isp_sspm1
				   | ratio << ISP_REQ_FREQ_OFFSET
				   | 1 << ISP_FREQ_VALID_OFFSET
				   | 0xF << ISP_REQ_GUAR_FREQ_OFFSET);

	isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
	timeout = 10;
	while ((isp_sspm1 & ISP_FREQ_VALID_MASK) && timeout) {
		isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
		dev_dbg(isp->dev, "waiting for ISPSSPM1 valid bit to be 0.\n");
		udelay(100);
		timeout--;
	}
	if (timeout == 0) {
		dev_err(isp->dev, "DFS failed due to HW error.\n");
		return -EINVAL;
	}

	isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
	timeout = 10;
	while (((isp_sspm1 >> ISP_FREQ_STAT_OFFSET) != ratio) && timeout) {
		isp_sspm1 = intel_mid_msgbus_read32(PUNIT_PORT, ISPSSPM1);
		dev_dbg(isp->dev, "waiting for ISPSSPM1 status bit to be "
				"0x%x.\n", new_freq);
		udelay(100);
		timeout--;
	}
	if (timeout == 0) {
		dev_err(isp->dev, "DFS target freq is rejected by HW.\n");
		return -EINVAL;
	}

	return 0;
}
int atomisp_freq_scaling(struct atomisp_device *isp, enum atomisp_dfs_mode mode)
{
	/* FIXME! Only use subdev[0] status yet */
	struct atomisp_sub_device *asd = &isp->asd[0];
	unsigned int new_freq;
	struct atomisp_freq_scaling_rule curr_rules;
	struct atomisp_freq_scaling_rule *dfs_rules;
	unsigned int rule_number;
	int i, ret;
	unsigned short fps = 0;

	if (isp->sw_contex.power_state != ATOM_ISP_POWER_UP) {
		dev_err(isp->dev, "DFS cannot proceed due to no power.\n");
		return -EINVAL;
	}

	if (mode == ATOMISP_DFS_MODE_LOW) {
		new_freq = ISP_FREQ_200MHZ;
		goto done;
	}

	if (mode == ATOMISP_DFS_MODE_MAX) {
		new_freq = ISP_FREQ_MAX;
		goto done;
	}

	fps = atomisp_get_sensor_fps(asd);
	if (fps == 0)
		return -EINVAL;

	curr_rules.width = asd->fmt[asd->capture_pad].fmt.width;
	curr_rules.height = asd->fmt[asd->capture_pad].fmt.height;
	curr_rules.fps = fps;
	curr_rules.run_mode = asd->run_mode->val;
	/*
	 * For continuous mode, we need to make the capture setting applied
	 * since preview mode, because there is no chance to do this when
	 * starting image capture.
	 */
	if (asd->continuous_mode->val) {
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)
			curr_rules.run_mode = ATOMISP_RUN_MODE_SDV;
		else
			curr_rules.run_mode = ATOMISP_RUN_MODE_STILL_CAPTURE;
	}

	if (IS_ISP2401(isp)) {
		dfs_rules = dfs_rules_isp2401;
		rule_number = ARRAY_SIZE(dfs_rules_isp2401);
	} else {
		dfs_rules = dfs_rules_isp2400;
		rule_number = ARRAY_SIZE(dfs_rules_isp2400);
	}

	/* search for the target frequency by looping freq rules*/
	for (i = 0; i < rule_number; i++) {
		if (curr_rules.width != dfs_rules[i].width
			&& dfs_rules[i].width != ISP_FREQ_RULE_ANY)
			continue;
		if (curr_rules.height != dfs_rules[i].height
			&& dfs_rules[i].height != ISP_FREQ_RULE_ANY)
			continue;
		if (curr_rules.fps != dfs_rules[i].fps
			&& dfs_rules[i].fps != ISP_FREQ_RULE_ANY)
			continue;
		if (curr_rules.run_mode != dfs_rules[i].run_mode
			&& dfs_rules[i].run_mode != ISP_FREQ_RULE_ANY)
			continue;
		break;
	}
	if (i == rule_number)
		new_freq = ISP_FREQ_320MHZ;
	else
		new_freq = dfs_rules[i].isp_freq;

done:
	dev_dbg(isp->dev, "DFS target frequency=%d.\n", new_freq);

	if (new_freq == isp->sw_contex.running_freq) {
		dev_dbg(isp->dev, "ignoring DFS target freq.\n");
		return 0;
	}
	ret = write_target_freq_to_hw(isp, new_freq);
	if (!ret)
		isp->sw_contex.running_freq = new_freq;
	return ret;
}

/*
 * reset and restore ISP
 */
int atomisp_reset(struct atomisp_device *isp)
{
	/* Reset ISP by power-cycling it */
	int ret = 0;

	dev_dbg(isp->dev, "%s\n", __func__);
	atomisp_css_suspend();
	ret = pm_runtime_put_sync(isp->dev);
	if (ret < 0) {
		dev_err(isp->dev, "can not disable ISP power\n");
	} else {
		ret = pm_runtime_get_sync(isp->dev);
		if (ret < 0)
			dev_err(isp->dev, "can not enable ISP power\n");
	}
	ret = atomisp_css_resume(isp);
	if (ret)
		isp->isp_fatal_error = true;

	return ret;
}

/*
 * interrupt enable/disable functions
 */
static void enable_isp_irq(enum hrt_isp_css_irq irq, bool enable)
{
	if (enable) {
		irq_enable_channel(IRQ0_ID, irq);
		/*sh_css_hrt_irq_enable(irq, true, false);*/
		switch (irq) { /*We only have sp interrupt right now*/
		case hrt_isp_css_irq_sp:
			/*sh_css_hrt_irq_enable_sp(true);*/
			cnd_sp_irq_enable(SP0_ID, true);
			break;
		default:
			break;
		}

	} else {
		/*sh_css_hrt_irq_disable(irq);*/
		irq_disable_channel(IRQ0_ID, irq);
		switch (irq) {
		case hrt_isp_css_irq_sp:
			/*sh_css_hrt_irq_enable_sp(false);*/
			cnd_sp_irq_enable(SP0_ID, false);
			break;
		default:
			break;
		}
	}
}

/*
 * interrupt clean function
 */
static void clear_isp_irq(enum hrt_isp_css_irq irq)
{
	irq_clear_all(IRQ0_ID);
}

void atomisp_msi_irq_init(struct atomisp_device *isp, struct pci_dev *dev)
{
	u32 msg32;
	u16 msg16;

	pci_read_config_dword(dev, PCI_MSI_CAPID, &msg32);
	msg32 |= 1 << MSI_ENABLE_BIT;
	pci_write_config_dword(dev, PCI_MSI_CAPID, msg32);

	msg32 = (1 << INTR_IER) | (1 << INTR_IIR);
	pci_write_config_dword(dev, PCI_INTERRUPT_CTRL, msg32);

	pci_read_config_word(dev, PCI_COMMAND, &msg16);
	msg16 |= (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
		  PCI_COMMAND_INTX_DISABLE);
	pci_write_config_word(dev, PCI_COMMAND, msg16);
}

void atomisp_msi_irq_uninit(struct atomisp_device *isp, struct pci_dev *dev)
{
	u32 msg32;
	u16 msg16;

	pci_read_config_dword(dev, PCI_MSI_CAPID, &msg32);
	msg32 &=  ~(1 << MSI_ENABLE_BIT);
	pci_write_config_dword(dev, PCI_MSI_CAPID, msg32);

	msg32 = 0x0;
	pci_write_config_dword(dev, PCI_INTERRUPT_CTRL, msg32);

	pci_read_config_word(dev, PCI_COMMAND, &msg16);
	msg16 &= ~(PCI_COMMAND_MASTER);
	pci_write_config_word(dev, PCI_COMMAND, msg16);
}

static void atomisp_sof_event(struct atomisp_sub_device *asd)
{
	struct v4l2_event event = {0};

	event.type = V4L2_EVENT_FRAME_SYNC;
	event.u.frame_sync.frame_sequence = atomic_read(&asd->sof_count);

	v4l2_event_queue(asd->subdev.devnode, &event);
}

static void atomisp_3a_stats_ready_event(struct atomisp_sub_device *asd)
{
	struct v4l2_event event = {0};

	event.type = V4L2_EVENT_ATOMISP_3A_STATS_READY;
	event.u.frame_sync.frame_sequence = atomic_read(&asd->sequence);

	v4l2_event_queue(asd->subdev.devnode, &event);
}

static void print_csi_rx_errors(struct atomisp_device *isp)
{
	u32 infos = 0;

	atomisp_css_rx_get_irq_info(&infos);

	dev_err(isp->dev, "CSI Receiver errors:\n");
	if (infos & CSS_RX_IRQ_INFO_BUFFER_OVERRUN)
		dev_err(isp->dev, "  buffer overrun");
	if (infos & CSS_RX_IRQ_INFO_ERR_SOT)
		dev_err(isp->dev, "  start-of-transmission error");
	if (infos & CSS_RX_IRQ_INFO_ERR_SOT_SYNC)
		dev_err(isp->dev, "  start-of-transmission sync error");
	if (infos & CSS_RX_IRQ_INFO_ERR_CONTROL)
		dev_err(isp->dev, "  control error");
	if (infos & CSS_RX_IRQ_INFO_ERR_ECC_DOUBLE)
		dev_err(isp->dev, "  2 or more ECC errors");
	if (infos & CSS_RX_IRQ_INFO_ERR_CRC)
		dev_err(isp->dev, "  CRC mismatch");
	if (infos & CSS_RX_IRQ_INFO_ERR_UNKNOWN_ID)
		dev_err(isp->dev, "  unknown error");
	if (infos & CSS_RX_IRQ_INFO_ERR_FRAME_SYNC)
		dev_err(isp->dev, "  frame sync error");
	if (infos & CSS_RX_IRQ_INFO_ERR_FRAME_DATA)
		dev_err(isp->dev, "  frame data error");
	if (infos & CSS_RX_IRQ_INFO_ERR_DATA_TIMEOUT)
		dev_err(isp->dev, "  data timeout");
	if (infos & CSS_RX_IRQ_INFO_ERR_UNKNOWN_ESC)
		dev_err(isp->dev, "  unknown escape command entry");
	if (infos & CSS_RX_IRQ_INFO_ERR_LINE_SYNC)
		dev_err(isp->dev, "  line sync error");
}

/* Clear irq reg */
static void clear_irq_reg(struct atomisp_device *isp)
{
	u32 msg_ret;
	pci_read_config_dword(isp->pdev, PCI_INTERRUPT_CTRL, &msg_ret);
	msg_ret |= 1 << INTR_IIR;
	pci_write_config_dword(isp->pdev, PCI_INTERRUPT_CTRL, msg_ret);
}


/* interrupt handling function*/
irqreturn_t atomisp_isr(int irq, void *dev)
{
	struct atomisp_device *isp = (struct atomisp_device *)dev;
	struct atomisp_sub_device *asd;
	unsigned int irq_infos = 0;
	unsigned long flags;
	unsigned int i;
	int err;

	spin_lock_irqsave(&isp->lock, flags);
	if (isp->sw_contex.power_state != ATOM_ISP_POWER_UP) {
		spin_unlock_irqrestore(&isp->lock, flags);
		return IRQ_HANDLED;
	}
	err = atomisp_css_irq_translate(isp, &irq_infos);
	if (err) {
		spin_unlock_irqrestore(&isp->lock, flags);
		return IRQ_NONE;
	}

	dev_dbg(isp->dev, "irq:0x%x\n", irq_infos);

	clear_irq_reg(isp);

	if (!atomisp_streaming_count(isp) && !isp->acc.pipeline)
		goto out_nowake;

	for (i = 0; i < isp->num_of_streams; i++) {
		asd = &isp->asd[i];
		if (asd->streaming != ATOMISP_DEVICE_STREAMING_ENABLED)
			continue;
		/*
		 * Current SOF only support one stream, so the SOF only valid
		 * either solely one stream is running
		 */
		if (irq_infos & CSS_IRQ_INFO_CSS_RECEIVER_SOF) {
			atomic_inc(&asd->sof_count);
			atomisp_sof_event(asd);

			/* If sequence_temp and sequence are the same
			 * there where no frames lost so we can increase
			 * sequence_temp.
			 * If not then processing of frame is still in progress
			 * and driver needs to keep old sequence_temp value.
			 * NOTE: There is assumption here that ISP will not
			 * start processing next frame from sensor before old
			 * one is completely done. */
			if (atomic_read(&asd->sequence) == atomic_read(
						&asd->sequence_temp))
				atomic_set(&asd->sequence_temp,
						atomic_read(&asd->sof_count));

			/* signal streamon after delayed init is done */
			if (asd->delayed_init ==
					ATOMISP_DELAYED_INIT_WORK_DONE) {
				asd->delayed_init = ATOMISP_DELAYED_INIT_DONE;
				complete(&asd->init_done);
			}
		}
		if (irq_infos & CSS_IRQ_INFO_EVENTS_READY)
			atomic_set(&asd->sequence,
					atomic_read(&asd->sequence_temp));
	}

	if (irq_infos & CSS_IRQ_INFO_CSS_RECEIVER_SOF)
		irq_infos &= ~CSS_IRQ_INFO_CSS_RECEIVER_SOF;

#if defined(ISP2400) || defined(ISP2400B0) || defined(ISP2401)
	if ((irq_infos & CSS_IRQ_INFO_INPUT_SYSTEM_ERROR) ||
		(irq_infos & CSS_IRQ_INFO_IF_ERROR)) {
#else
	if (irq_infos & CSS_IRQ_INFO_CSS_RECEIVER_ERROR) {
#endif
		/* handle mipi receiver error */
		u32 rx_infos;

		print_csi_rx_errors(isp);
		atomisp_css_rx_get_irq_info(&rx_infos);
		atomisp_css_rx_clear_irq_info(rx_infos);
	}
#if defined(CSS15) && defined(ISP2300)
	if (irq_infos & CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW) {
		atomic_inc(&isp->fast_reset);
		queue_work(isp->wdt_work_queue, &isp->wdt_work);
		goto out_nowake;
	}
#endif
#ifndef CSS20
	if (irq_infos & CSS_IRQ_INFO_INVALID_FIRST_FRAME) {
		isp->sw_contex.invalid_frame = 1;
		isp->sw_contex.invalid_vf_frame = 1;
		isp->sw_contex.invalid_s3a = 1;
		isp->sw_contex.invalid_dis = 1;
	}
#endif /* CSS20 */

	spin_unlock_irqrestore(&isp->lock, flags);

	return IRQ_WAKE_THREAD;

out_nowake:
	spin_unlock_irqrestore(&isp->lock, flags);

	return IRQ_HANDLED;
}

/*
 * Background:
 * The IUNITPHY register CSI_CONTROL bit definition was changed since PNW C0.
 * For PNW A0 and B0, CSI4_TERM_EN_COUNT is bit 23:20 (4 bits).
 * Starting from PWN C0, including all CLV and CLV+ steppings,
 * CSI4_TERM_EN_COUNT is bit 30:24 (7 bits).
 *
 * ------------------------------------------
 * Silicon	Stepping	PCI revision
 * Penwell	A0		0x00
 * Penwell	B0		0x04
 * Penwell	C0		0x06
 * Penwell	D0		0x06
 * Penwell	D1		0x06
 * Penwell	D2		0x06
 * Cloverview	A0		0x06
 * Cloverview	B0		0x05
 * Cloverview	C0		0x04
 * Cloverview+	A0		0x08
 * Cloverview+	B0		0x0C
 *
 */

#define TERM_EN_COUNT_1LANE_OFFSET		16	/* bit 22:16 */
#define TERM_EN_COUNT_1LANE_MASK		0x7f0000
#define TERM_EN_COUNT_4LANE_OFFSET		24	/* bit 30:24 */
#define TERM_EN_COUNT_4LANE_MASK		0x7f000000
#define TERM_EN_COUNT_4LANE_PWN_B0_OFFSET	20	/* bit 23:20 */
#define TERM_EN_COUNT_4LANE_PWN_B0_MASK		0xf00000

void atomisp_set_term_en_count(struct atomisp_device *isp)
{
	uint32_t val;
	int pwn_b0 = 0;

	/* For MRFLD, there is no Tescape-clock cycles control. */
	if (IS_ISP24XX(isp))
		return;

	if (IS_MFLD && isp->pdev->device == 0x0148 &&
		isp->pdev->revision < 0x6)
		pwn_b0 = 1;

	val = intel_mid_msgbus_read32(MFLD_IUNITPHY_PORT, MFLD_CSI_CONTROL);

	/* set TERM_EN_COUNT_1LANE to 0xf */
	val &= ~TERM_EN_COUNT_1LANE_MASK;
	val |= 0xf << TERM_EN_COUNT_1LANE_OFFSET;

	/* set TERM_EN_COUNT_4LANE to 0xf */
	val &= pwn_b0 ? ~TERM_EN_COUNT_4LANE_PWN_B0_MASK :
				~TERM_EN_COUNT_4LANE_MASK;
	val |= 0xf << (pwn_b0 ? TERM_EN_COUNT_4LANE_PWN_B0_OFFSET :
				TERM_EN_COUNT_4LANE_OFFSET);

	intel_mid_msgbus_write32(MFLD_IUNITPHY_PORT, MFLD_CSI_CONTROL, val);
}

void atomisp_clear_css_buffer_counters(struct atomisp_sub_device *asd)
{
	memset(asd->s3a_bufs_in_css, 0, sizeof(asd->s3a_bufs_in_css));
	asd->dis_bufs_in_css = 0;
	asd->video_out_capture.buffers_in_css = 0;
	asd->video_out_vf.buffers_in_css = 0;
	asd->video_out_preview.buffers_in_css = 0;
	asd->video_out_video_capture.buffers_in_css = 0;
}

bool atomisp_buffers_queued(struct atomisp_sub_device *asd)
{
	return asd->video_out_capture.buffers_in_css ||
		asd->video_out_vf.buffers_in_css ||
		asd->video_out_preview.buffers_in_css ||
		asd->video_out_video_capture.buffers_in_css ?
		    true : false;
}

/* 0x100000 is the start of dmem inside SP */
#define SP_DMEM_BASE	0x100000

void dump_sp_dmem(struct atomisp_device *isp, unsigned int addr,
		  unsigned int size)
{
	unsigned int data = 0;
	unsigned int size32 = DIV_ROUND_UP(size, sizeof(u32));

	dev_dbg(isp->dev, "atomisp_io_base:%p\n", atomisp_io_base);
	dev_dbg(isp->dev, "%s, addr:0x%x, size: %d, size32: %d\n", __func__,
			addr, size, size32);
	if (size32 * 4 + addr > 0x4000) {
		dev_err(isp->dev, "illegal size (%d) or addr (0x%x)\n",
				size32, addr);
		return;
	}
	addr += SP_DMEM_BASE;
	do {
		data = _hrt_master_port_uload_32(addr);

		dev_dbg(isp->dev, "%s, \t [0x%x]:0x%x\n", __func__, addr, data);
		addr += sizeof(unsigned int);
		size32 -= 1;
	} while (size32 > 0);
}

static struct videobuf_buffer *atomisp_css_frame_to_vbuf(
	struct atomisp_video_pipe *pipe, struct atomisp_css_frame *frame)
{
	struct videobuf_vmalloc_memory *vm_mem;
	struct atomisp_css_frame *handle;
	int i;

	for (i = 0; pipe->capq.bufs[i]; i++) {
		vm_mem = pipe->capq.bufs[i]->priv;
		handle = vm_mem->vaddr;
		if (handle && handle->data == frame->data)
			return pipe->capq.bufs[i];
	}

	return NULL;
}

static void get_buf_timestamp(struct timeval *tv)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}

static void atomisp_flush_video_pipe(struct atomisp_sub_device *asd,
				     struct atomisp_video_pipe *pipe)
{
	unsigned long irqflags;
	int i;

	if (!pipe->users)
		return;

	for (i = 0; pipe->capq.bufs[i]; i++) {
		spin_lock_irqsave(&pipe->irq_lock, irqflags);
		if (pipe->capq.bufs[i]->state == VIDEOBUF_ACTIVE ||
		    pipe->capq.bufs[i]->state == VIDEOBUF_QUEUED) {
			get_buf_timestamp(&pipe->capq.bufs[i]->ts);
			pipe->capq.bufs[i]->field_count =
				atomic_read(&asd->sequence) << 1;
			dev_dbg(asd->isp->dev, "release buffers on device %s\n",
				pipe->vdev.name);
			if (pipe->capq.bufs[i]->state == VIDEOBUF_QUEUED)
				list_del_init(&pipe->capq.bufs[i]->queue);
			pipe->capq.bufs[i]->state = VIDEOBUF_ERROR;
			wake_up(&pipe->capq.bufs[i]->done);
		}
		spin_unlock_irqrestore(&pipe->irq_lock, irqflags);
	}
}

/* Returns queued buffers back to video-core */
void atomisp_flush_bufs_and_wakeup(struct atomisp_sub_device *asd)
{
	atomisp_flush_video_pipe(asd, &asd->video_out_capture);
	atomisp_flush_video_pipe(asd, &asd->video_out_vf);
	atomisp_flush_video_pipe(asd, &asd->video_out_preview);
	atomisp_flush_video_pipe(asd, &asd->video_out_video_capture);
}


/* find atomisp_video_pipe with css pipe id, buffer type and atomisp run_mode */
static struct atomisp_video_pipe *__atomisp_get_pipe(
		struct atomisp_sub_device *asd,
		enum atomisp_input_stream_id stream_id,
		enum atomisp_css_pipe_id css_pipe_id,
		enum atomisp_css_buffer_type buf_type)
{
	if (css_pipe_id == CSS_PIPE_ID_COPY) {
		switch (stream_id) {
		case ATOMISP_INPUT_STREAM_PREVIEW:
			return &asd->video_out_preview;
		case ATOMISP_INPUT_STREAM_POSTVIEW:
			return &asd->video_out_vf;
		case ATOMISP_INPUT_STREAM_VIDEO:
			return &asd->video_out_video_capture;
		case ATOMISP_INPUT_STREAM_CAPTURE:
		default:
			return &asd->video_out_capture;
		}
	}
	/* video is same in online as in continuouscapture mode */
	if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_LOWLAT) {
		/*
		 * Disable vf_pp and run CSS in still capture mode. In this
		 * mode, CSS does not cause extra latency with buffering, but
		 * scaling is not available.
		 */
		return &asd->video_out_capture;
	} else if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER) {
		/*
		 * Disable vf_pp and run CSS in video mode. This allows using
		 * ISP scaling but it has one frame delay due to CSS internal
		 * buffering.
		 */
		return &asd->video_out_video_capture;
	} else if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO) {
		if (css_pipe_id == CSS_PIPE_ID_VIDEO) {
			if (buf_type == CSS_BUFFER_TYPE_OUTPUT_FRAME)
				return &asd->video_out_video_capture;
			return &asd->video_out_preview;
		} else {
			if (buf_type == CSS_BUFFER_TYPE_OUTPUT_FRAME)
				return &asd->video_out_capture;
			return &asd->video_out_vf;
		}
	} else if (buf_type == CSS_BUFFER_TYPE_OUTPUT_FRAME) {
		if (css_pipe_id == CSS_PIPE_ID_PREVIEW)
			return &asd->video_out_preview;
		return &asd->video_out_capture;
	/* statistic buffers are needed only in css capture & preview pipes */
	} else if (buf_type == CSS_BUFFER_TYPE_3A_STATISTICS ||
		   buf_type == CSS_BUFFER_TYPE_DIS_STATISTICS) {
		if (css_pipe_id == CSS_PIPE_ID_PREVIEW)
			return &asd->video_out_preview;
		return &asd->video_out_capture;
	}
	return &asd->video_out_vf;
}

void atomisp_buf_done(struct atomisp_sub_device *asd, int error,
		      enum atomisp_css_buffer_type buf_type,
		      enum atomisp_css_pipe_id css_pipe_id,
		      bool q_buffers, enum atomisp_input_stream_id stream_id)
{
	struct videobuf_buffer *vb = NULL;
	struct atomisp_video_pipe *pipe = NULL;
	struct atomisp_css_buffer buffer;
	bool requeue = false;
	int err;
	unsigned long irqflags;
	struct atomisp_css_frame *frame = NULL;
	struct atomisp_device *isp = asd->isp;

	if (buf_type != CSS_BUFFER_TYPE_3A_STATISTICS &&
	    buf_type != CSS_BUFFER_TYPE_DIS_STATISTICS &&
	    buf_type != CSS_BUFFER_TYPE_OUTPUT_FRAME &&
	    buf_type != CSS_BUFFER_TYPE_RAW_OUTPUT_FRAME &&
	    buf_type != CSS_BUFFER_TYPE_VF_OUTPUT_FRAME) {
		dev_err(isp->dev, "%s, unsupported buffer type: %d\n",
			__func__, buf_type);
		return;
	}

	memset(&buffer, 0, sizeof(struct atomisp_css_buffer));
	buffer.css_buffer.type = buf_type;
	err = atomisp_css_dequeue_buffer(asd, stream_id, css_pipe_id,
			buf_type, &buffer);
	if (err) {
		dev_err(isp->dev,
			"atomisp_css_dequeue_buffer failed: 0x%x\n", err);
		return;
	}

	/* need to know the atomisp pipe for frame buffers */
	pipe = __atomisp_get_pipe(asd, stream_id, css_pipe_id, buf_type);
	if (pipe == NULL) {
		dev_err(isp->dev, "error getting atomisp pipe\n");
		return;
	}

	switch (buf_type) {
		case CSS_BUFFER_TYPE_3A_STATISTICS:
			/* ignore error in case of 3a statistics for now */
			if (isp->sw_contex.invalid_s3a) {
				requeue = true;
				isp->sw_contex.invalid_s3a = 0;
				break;
			}
			/* update the 3A data to ISP context */
			if (!error)
				atomisp_css_get_3a_statistics(asd, &buffer);

			asd->s3a_bufs_in_css[css_pipe_id]--;

			atomisp_3a_stats_ready_event(asd);
			break;
		case CSS_BUFFER_TYPE_DIS_STATISTICS:
			/* ignore error in case of dis statistics for now */
			if (isp->sw_contex.invalid_dis) {
				requeue = true;
				isp->sw_contex.invalid_dis = 0;
				break;
			}
			if (!error)
				atomisp_css_get_dis_statistics(asd, &buffer);

			asd->dis_bufs_in_css--;
			break;
		case CSS_BUFFER_TYPE_VF_OUTPUT_FRAME:
			if (isp->sw_contex.invalid_vf_frame) {
				error = true;
				isp->sw_contex.invalid_vf_frame = 0;
				dev_dbg(isp->dev, "%s css has marked this "
					"vf frame as invalid\n", __func__);
			}

			pipe->buffers_in_css--;
			frame = buffer.css_buffer.data.frame;
			if (!frame) {
				WARN_ON(1);
				break;
			}
#ifdef CSS20
			if (!frame->valid)
				error = true;
#endif

			if (asd->params.flash_state ==
			    ATOMISP_FLASH_ONGOING) {
				if (frame->flash_state
				    == CSS_FRAME_FLASH_STATE_PARTIAL)
					dev_dbg(isp->dev, "%s thumb partially "
						"flashed\n", __func__);
				else if (frame->flash_state
					 == CSS_FRAME_FLASH_STATE_FULL)
					dev_dbg(isp->dev, "%s thumb completely "
						"flashed\n", __func__);
				else
					dev_dbg(isp->dev, "%s thumb no flash "
						"in this frame\n", __func__);
			}
			vb = atomisp_css_frame_to_vbuf(pipe, frame);
			WARN_ON(!vb);
			break;
		case CSS_BUFFER_TYPE_OUTPUT_FRAME:
			if (isp->sw_contex.invalid_frame) {
				error = true;
				isp->sw_contex.invalid_frame = 0;
				dev_dbg(isp->dev, "%s css has marked this "
					"frame as invalid\n", __func__);
			}
			pipe->buffers_in_css--;
			frame = buffer.css_buffer.data.frame;
			if (!frame) {
				WARN_ON(1);
				break;
			}

#ifdef CSS20
			if (!frame->valid)
				error = true;
#endif
			vb = atomisp_css_frame_to_vbuf(pipe, frame);
			if (!vb) {
				WARN_ON(1);
				break;
			}

			if (asd->params.flash_state ==
			    ATOMISP_FLASH_ONGOING) {
				if (frame->flash_state
				    == CSS_FRAME_FLASH_STATE_PARTIAL) {
					asd->frame_status[vb->i] =
						ATOMISP_FRAME_STATUS_FLASH_PARTIAL;
					dev_dbg(isp->dev,
						 "%s partially flashed\n",
						 __func__);
				} else if (frame->flash_state
					   == CSS_FRAME_FLASH_STATE_FULL) {
					asd->frame_status[vb->i] =
						ATOMISP_FRAME_STATUS_FLASH_EXPOSED;
					asd->params.num_flash_frames--;
					dev_dbg(isp->dev,
						 "%s completely flashed\n",
						 __func__);
				} else {
					asd->frame_status[vb->i] =
						ATOMISP_FRAME_STATUS_OK;
					dev_dbg(isp->dev,
						 "%s no flash in this frame\n",
						 __func__);
				}

				/* Check if flashing sequence is done */
				if (asd->frame_status[vb->i] ==
					ATOMISP_FRAME_STATUS_FLASH_EXPOSED)
					asd->params.flash_state =
						ATOMISP_FLASH_DONE;
			} else {
				asd->frame_status[vb->i] =
					ATOMISP_FRAME_STATUS_OK;
			}

			asd->params.last_frame_status =
				asd->frame_status[vb->i];

			if (asd->continuous_mode->val) {
				unsigned int exp_id = frame->exp_id;

				if (css_pipe_id == CSS_PIPE_ID_PREVIEW ||
				    css_pipe_id == CSS_PIPE_ID_VIDEO) {
					asd->latest_preview_exp_id = exp_id;
				} else if (css_pipe_id ==
						CSS_PIPE_ID_CAPTURE) {
					if (asd->run_mode->val ==
					    ATOMISP_RUN_MODE_VIDEO)
					    dev_dbg(isp->dev,
						    "SDV capture raw buffer id: %u\n",
						    exp_id);
					else
					    dev_dbg(isp->dev,
						    "ZSL capture raw buffer id: %u\n",
						    exp_id);
				}
			}
			break;
		default:
			break;
	}
	if (vb) {
		get_buf_timestamp(&vb->ts);
		vb->field_count = atomic_read(&asd->sequence) << 1;
		/*mark videobuffer done for dequeue*/
		spin_lock_irqsave(&pipe->irq_lock, irqflags);
		vb->state = !error ? VIDEOBUF_DONE : VIDEOBUF_ERROR;
		spin_unlock_irqrestore(&pipe->irq_lock, irqflags);

		/*
		 * Frame capture done, wake up any process block on
		 * current active buffer
		 * possibly hold by videobuf_dqbuf()
		 */
		wake_up(&vb->done);
	}

	/*
	 * Requeue should only be done for 3a and dis buffers.
	 * Queue/dequeue order will change if driver recycles image buffers.
	 */
	if (requeue) {
		err = atomisp_css_queue_buffer(asd,
					stream_id, css_pipe_id,
					buf_type, &buffer);
		if (err)
			dev_err(isp->dev, "%s, q to css fails: %d\n",
					__func__, err);
		return;
	}
	if (!error && q_buffers)
		atomisp_qbuffers_to_css(asd);
}

void atomisp_delayed_init_work(struct work_struct *work)
{
	struct atomisp_sub_device *asd = container_of(work,
			struct atomisp_sub_device,
			delayed_init_work);
	atomisp_css_allocate_continuous_frames(false, asd);
	atomisp_css_update_continuous_frames(asd);
	asd->delayed_init = ATOMISP_DELAYED_INIT_WORK_DONE;
}

static void __atomisp_css_recover(struct atomisp_device *isp)
{
	enum atomisp_css_pipe_id css_pipe_id;
	bool stream_restart[MAX_STREAM_NUM] = {0};
	int i, ret;

	if (!isp->sw_contex.file_input) {
		atomisp_css_irq_enable(isp,
				CSS_IRQ_INFO_CSS_RECEIVER_SOF, false);
#if defined(CSS15) && defined(ISP2300)
		atomisp_css_irq_enable(isp,
				CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW, false);
#endif
	}


	for (i = 0; i < isp->num_of_streams; i++) {
		struct atomisp_sub_device *asd = &isp->asd[i];

		if (asd->streaming !=
				ATOMISP_DEVICE_STREAMING_ENABLED)
			continue;
		if (asd->delayed_init == ATOMISP_DELAYED_INIT_QUEUED)
			cancel_work_sync(&asd->delayed_init_work);

		complete(&asd->init_done);
		asd->delayed_init = ATOMISP_DELAYED_INIT_NOT_QUEUED;

		stream_restart[asd->index] = true;

		asd->streaming = ATOMISP_DEVICE_STREAMING_STOPPING;

		css_pipe_id = atomisp_get_css_pipe_id(asd);
		atomisp_css_stop(asd, css_pipe_id, true);

		atomisp_acc_unload_extensions(asd);

		/* stream off sensor */
		ret = v4l2_subdev_call(
				isp->inputs[asd->input_curr].
				camera, video, s_stream, 0);
		if (ret)
			dev_warn(isp->dev,
					"can't stop streaming on sensor!\n");

		atomisp_clear_css_buffer_counters(asd);
		asd->streaming = ATOMISP_DEVICE_STREAMING_DISABLED;
	}

	/* clear irq */
	enable_isp_irq(hrt_isp_css_irq_sp, false);
	clear_isp_irq(hrt_isp_css_irq_sp);

	/* reset ISP and restore its state */
	isp->isp_timeout = true;
	atomisp_reset(isp);
	isp->isp_timeout = false;

	/* The following frame after an ISP timeout
	 * may be corrupted, so mark it so. */
	isp->sw_contex.invalid_frame = 1;
	isp->sw_contex.invalid_vf_frame = 1;
	isp->sw_contex.invalid_s3a = 1;
	isp->sw_contex.invalid_dis = 1;

	for (i = 0; i < isp->num_of_streams; i++) {
		struct atomisp_sub_device *asd = &isp->asd[i];

		if (!stream_restart[i])
			continue;

		if (atomisp_acc_load_extensions(asd) < 0)
			dev_err(isp->dev,
					"acc extension failed to reload\n");

		if (isp->inputs[asd->input_curr].type != FILE_INPUT)
			atomisp_css_input_set_mode(asd,
					CSS_INPUT_MODE_SENSOR);

		css_pipe_id = atomisp_get_css_pipe_id(asd);
		atomisp_css_start(asd, css_pipe_id, true);

		asd->streaming = ATOMISP_DEVICE_STREAMING_ENABLED;
	}

	if (!isp->sw_contex.file_input) {
		atomisp_css_irq_enable(isp,
				CSS_IRQ_INFO_CSS_RECEIVER_SOF, true);
#if defined(CSS15) && defined(ISP2300)
		atomisp_css_irq_enable(isp,
				CSS_IRQ_INFO_CSS_RECEIVER_FIFO_OVERFLOW, true);
#endif

		atomisp_set_term_en_count(isp);

		if (IS_ISP24XX(isp) &&
		    atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_AUTO) < 0)
			dev_dbg(isp->dev, "dfs failed!\n");
	} else {
		if (IS_ISP24XX(isp) &&
		    atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_MAX) < 0)
			dev_dbg(isp->dev, "dfs failed!\n");
	}

	for (i = 0; i < isp->num_of_streams; i++) {
		struct atomisp_sub_device *asd = &isp->asd[i];

		if (!stream_restart[i])
			continue;

		ret = v4l2_subdev_call(
				isp->inputs[asd->input_curr].camera, video,
				s_stream, 1);
		if (ret)
			dev_warn(isp->dev,
					"can't start streaming on sensor!\n");

		if (asd->continuous_mode->val &&
		    asd->delayed_init == ATOMISP_DELAYED_INIT_NOT_QUEUED) {
			INIT_COMPLETION(asd->init_done);
			asd->delayed_init = ATOMISP_DELAYED_INIT_QUEUED;
			queue_work(asd->delayed_init_workq,
					&asd->delayed_init_work);
		}
		/*
		 * dequeueing buffers is not needed. CSS will recycle
		 * buffers that it has.
		 */
		atomisp_flush_bufs_and_wakeup(asd);
	}

}

void atomisp_wdt_work(struct work_struct *work)
{
	struct atomisp_device *isp = container_of(work, struct atomisp_device,
						  wdt_work);
	int i;

	mutex_lock(&isp->mutex);
	if (!atomisp_streaming_count(isp)) {
		mutex_unlock(&isp->mutex);
		return;
	}

	if (atomic_read(&isp->fast_reset)) {
		atomisp_set_stop_timeout(0);
		goto process_timeout;
	}

	dev_err(isp->dev, "timeout %d of %d\n",
		atomic_read(&isp->wdt_count) + 1,
		ATOMISP_ISP_MAX_TIMEOUT_COUNT);

	if (atomic_inc_return(&isp->wdt_count) <
			ATOMISP_ISP_MAX_TIMEOUT_COUNT) {
		atomisp_css_debug_set_dtrace_level(CSS_DTRACE_VERBOSITY_TIMEOUT);
		atomisp_css_debug_dump_sp_sw_debug_info();
		atomisp_css_debug_dump_debug_info(__func__);
		atomisp_css_debug_set_dtrace_level(CSS_DTRACE_VERBOSITY_LEVEL);
		for (i = 0; i < isp->num_of_streams; i++) {
			struct atomisp_sub_device *asd = &isp->asd[i];

			if (asd->streaming != ATOMISP_DEVICE_STREAMING_ENABLED)
				continue;
			dev_err(isp->dev, "%s, vdev %s buffers in css: %d\n",
				__func__,
				asd->video_out_capture.vdev.name,
				asd->video_out_capture.
				buffers_in_css);
			dev_err(isp->dev,
				"%s, vdev %s buffers in css: %d\n",
				__func__,
				asd->video_out_vf.vdev.name,
				asd->video_out_vf.
				buffers_in_css);
			dev_err(isp->dev,
				"%s, vdev %s buffers in css: %d\n",
				__func__,
				asd->video_out_preview.vdev.name,
				asd->video_out_preview.
				buffers_in_css);
			dev_err(isp->dev,
				"%s, vdev %s buffers in css: %d\n",
				__func__,
				asd->video_out_video_capture.vdev.name,
				asd->video_out_video_capture.
				buffers_in_css);
			dev_err(isp->dev,
				"%s, s3a buffers in css preview pipe:%d\n",
				__func__,
				asd->
				s3a_bufs_in_css[CSS_PIPE_ID_PREVIEW]);
			dev_err(isp->dev,
				"%s, s3a buffers in css capture pipe:%d\n",
				__func__, asd->
				s3a_bufs_in_css[CSS_PIPE_ID_CAPTURE]);
			dev_err(isp->dev,
				"%s, s3a buffers in css video pipe:%d\n",
				__func__, asd->
				s3a_bufs_in_css[CSS_PIPE_ID_VIDEO]);
			dev_err(isp->dev,
				"%s, dis buffers in css: %d\n",
				__func__, asd->dis_bufs_in_css);
		}

		/*sh_css_dump_sp_state();*/
		/*sh_css_dump_isp_state();*/
	} else {
		for (i = 0; i < isp->num_of_streams; i++) {
			struct atomisp_sub_device *asd = &isp->asd[i];
			if (asd->streaming ==
			    ATOMISP_DEVICE_STREAMING_ENABLED) {
				atomisp_clear_css_buffer_counters(asd);
				atomisp_flush_bufs_and_wakeup(asd);
				complete(&asd->init_done);
			}
		}

		atomic_set(&isp->wdt_count, 0);
		isp->isp_fatal_error = true;

		mutex_unlock(&isp->mutex);
		return;
	}
process_timeout:
	__atomisp_css_recover(isp);
	atomic_set(&isp->fast_reset, 0);
	atomisp_set_stop_timeout(ATOMISP_CSS_STOP_TIMEOUT_US);
	dev_err(isp->dev, "timeout recovery handling done\n");

	mutex_unlock(&isp->mutex);
}

void atomisp_css_flush(struct atomisp_device *isp)
{
	if (!atomisp_streaming_count(isp))
		return;

	/* Disable wdt */
	del_timer_sync(&isp->wdt);
	cancel_work_sync(&isp->wdt_work);

	/* Start recover */
	__atomisp_css_recover(isp);

	/* Restore wdt */
	if (isp->sw_contex.file_input)
		isp->wdt_duration = ATOMISP_ISP_FILE_TIMEOUT_DURATION;
	else
		isp->wdt_duration = ATOMISP_ISP_TIMEOUT_DURATION;

	mod_timer(&isp->wdt, jiffies + isp->wdt_duration);

	dev_dbg(isp->dev, "atomisp css flush done\n");
}

void atomisp_wdt(unsigned long isp_addr)
{
	struct atomisp_device *isp = (struct atomisp_device *)isp_addr;

	queue_work(isp->wdt_work_queue, &isp->wdt_work);
}

void atomisp_setup_flash(struct atomisp_sub_device *asd)
{
	struct atomisp_device *isp = asd->isp;
	struct v4l2_control ctrl;

	if (asd->params.flash_state != ATOMISP_FLASH_REQUESTED &&
	    asd->params.flash_state != ATOMISP_FLASH_DONE)
		return;

	if (asd->params.num_flash_frames) {
		/* make sure the timeout is set before setting flash mode */
		ctrl.id = V4L2_CID_FLASH_TIMEOUT;
		ctrl.value = FLASH_TIMEOUT;

		if (v4l2_subdev_call(isp->flash, core, s_ctrl, &ctrl)) {
			dev_err(isp->dev, "flash timeout configure failed\n");
			return;
		}

		atomisp_css_request_flash(asd);
		asd->params.flash_state = ATOMISP_FLASH_ONGOING;
	} else {
		asd->params.flash_state = ATOMISP_FLASH_IDLE;
	}
}

irqreturn_t atomisp_isr_thread(int irq, void *isp_ptr)
{
	struct atomisp_device *isp = isp_ptr;
	unsigned long flags;
	bool frame_done_found[MAX_STREAM_NUM] = {0};
	bool css_pipe_done[MAX_STREAM_NUM] = {0};
	bool reset_wdt_timer = false;
	unsigned int i;
	struct atomisp_sub_device *asd = &isp->asd[0];

	dev_dbg(isp->dev, ">%s\n", __func__);
	mutex_lock(&isp->mutex);

	spin_lock_irqsave(&isp->lock, flags);

	if (!atomisp_streaming_count(isp) && !isp->acc.pipeline) {
		spin_unlock_irqrestore(&isp->lock, flags);
		goto out;
	}
	spin_unlock_irqrestore(&isp->lock, flags);
	/*
	 * The standard CSS2.0 API tells the following calling sequence of
	 * dequeue ready buffers:
	 * while (ia_css_dequeue_event(...)) {
	 *	switch (event.type) {
	 *	...
	 *	ia_css_pipe_dequeue_buffer()
	 *	}
	 * }
	 * That is, dequeue event and buffer are one after another.
	 *
	 * But the following implementation is to first deuque all the event
	 * to a FIFO, then process the event in the FIFO.
	 * This will not have issue in single stream mode, but it do have some
	 * issue in multiple stream case. The issue is that
	 * ia_css_pipe_dequeue_buffer() will not return the corrent buffer in
	 * a specific pipe.
	 *
	 * This is due to ia_css_pipe_dequeue_buffer() does not take the
	 * ia_css_pipe parameter.
	 *
	 * So:
	 * For CSS2.0: we change the way to not dequeue all the event at one
	 * time, instead, dequue one and process one, then another
	 *
	 * For CSS1.5: still keep previous implementation.
	 */
	if (atomisp_css_isr_thread(isp, frame_done_found, css_pipe_done,
				   &reset_wdt_timer))
		goto out;

	for (i = 0; i < isp->num_of_streams; i++) {
		asd = &isp->asd[i];
		if (asd->streaming != ATOMISP_DEVICE_STREAMING_ENABLED)
			continue;
		if (frame_done_found[asd->index] &&
		    asd->params.css_update_params_needed) {
			atomisp_css_update_isp_params(asd);
			asd->params.css_update_params_needed = false;
			frame_done_found[asd->index] = false;
		}
		atomisp_setup_flash(asd);

		/* If there are no buffers queued then
		 * delete wdt timer. */
		if (!atomisp_buffers_queued(asd)) {
			del_timer(&isp->wdt);
		} else if (reset_wdt_timer) {
			/* SOF irq should not reset wdt timer. */
			mod_timer(&isp->wdt, jiffies +
				  isp->wdt_duration);
			atomic_set(&isp->wdt_count, 0);
		}
	}
out:
	mutex_unlock(&isp->mutex);
	for (i = 0; i < isp->num_of_streams; i++) {
		asd = &isp->asd[i];
		if (asd->streaming == ATOMISP_DEVICE_STREAMING_ENABLED
		    && css_pipe_done[asd->index]
		    && isp->sw_contex.file_input)
			v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					 video, s_stream, 1);
		/* FIXME! FIX ACC implementation */
		if (isp->acc.pipeline && css_pipe_done[asd->index])
			atomisp_css_acc_done(asd);
	}
	dev_dbg(isp->dev, "<%s\n", __func__);

	return IRQ_HANDLED;
}

/*
 * utils for buffer allocation/free
 */

int atomisp_get_frame_pgnr(struct atomisp_device *isp,
			   const struct atomisp_css_frame *frame, u32 *p_pgnr)
{
	if (!frame) {
		dev_err(isp->dev, "%s: NULL frame pointer ERROR.\n", __func__);
		return -EINVAL;
	}

	*p_pgnr = DIV_ROUND_UP(frame->data_bytes, PAGE_SIZE);
	return 0;
}

/*
 * Get internal fmt according to V4L2 fmt
 */
static enum atomisp_css_frame_format
v4l2_fmt_to_sh_fmt(u32 fmt)
{
	switch (fmt) {
	case V4L2_PIX_FMT_YUV420:
		return CSS_FRAME_FORMAT_YUV420;
	case V4L2_PIX_FMT_YVU420:
		return CSS_FRAME_FORMAT_YV12;
	case V4L2_PIX_FMT_YUV422P:
		return CSS_FRAME_FORMAT_YUV422;
	case V4L2_PIX_FMT_YUV444:
		return CSS_FRAME_FORMAT_YUV444;
	case V4L2_PIX_FMT_NV12:
		return CSS_FRAME_FORMAT_NV12;
	case V4L2_PIX_FMT_NV21:
		return CSS_FRAME_FORMAT_NV21;
	case V4L2_PIX_FMT_NV16:
		return CSS_FRAME_FORMAT_NV16;
	case V4L2_PIX_FMT_NV61:
		return CSS_FRAME_FORMAT_NV61;
	case V4L2_PIX_FMT_UYVY:
		return CSS_FRAME_FORMAT_UYVY;
	case V4L2_PIX_FMT_YUYV:
		return CSS_FRAME_FORMAT_YUYV;
	case V4L2_PIX_FMT_RGB24:
		return CSS_FRAME_FORMAT_PLANAR_RGB888;
	case V4L2_PIX_FMT_RGB32:
		return CSS_FRAME_FORMAT_RGBA888;
	case V4L2_PIX_FMT_RGB565:
		return CSS_FRAME_FORMAT_RGB565;
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return CSS_FRAME_FORMAT_RAW;
	default:
		return -EINVAL;
	}
}
/*
 * raw format match between SH format and V4L2 format
 */
static int raw_output_format_match_input(u32 input, u32 output)
{
	if ((input == CSS_FORMAT_RAW_12) &&
	    ((output == V4L2_PIX_FMT_SRGGB12) ||
	     (output == V4L2_PIX_FMT_SGRBG12) ||
	     (output == V4L2_PIX_FMT_SBGGR12) ||
	     (output == V4L2_PIX_FMT_SGBRG12)))
		return 0;

	if ((input == CSS_FORMAT_RAW_10) &&
	    ((output == V4L2_PIX_FMT_SRGGB10) ||
	     (output == V4L2_PIX_FMT_SGRBG10) ||
	     (output == V4L2_PIX_FMT_SBGGR10) ||
	     (output == V4L2_PIX_FMT_SGBRG10)))
		return 0;

	if ((input == CSS_FORMAT_RAW_8) &&
	    ((output == V4L2_PIX_FMT_SRGGB8) ||
	     (output == V4L2_PIX_FMT_SGRBG8) ||
	     (output == V4L2_PIX_FMT_SBGGR8) ||
	     (output == V4L2_PIX_FMT_SGBRG8)))
		return 0;

	if ((input == CSS_FORMAT_RAW_16) && (output == V4L2_PIX_FMT_SBGGR16))
		return 0;

	return -EINVAL;
}

static u32 get_pixel_depth(u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YVU420:
		return 12;
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		return 16;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_YUV444:
		return 24;
	case V4L2_PIX_FMT_RGB32:
		return 32;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return 8;
	default:
		return 8 * 2;	/* raw type now */
	}
}

bool atomisp_is_mbuscode_raw(uint32_t code)
{
	return code >= 0x3000 && code < 0x4000;
}

/*
 * ISP features control function
 */

/*
 * Set ISP capture mode based on current settings
 */
static void atomisp_update_capture_mode(struct atomisp_sub_device *asd)
{
	if (asd->params.gdc_cac_en)
		atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_ADVANCED);
	else if (asd->params.low_light)
		atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_LOW_LIGHT);
	else if (asd->video_out_capture.sh_fmt == CSS_FRAME_FORMAT_RAW)
		atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_RAW);
	else
		atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_PRIMARY);
}

/*
 * Function to enable/disable lens geometry distortion correction (GDC) and
 * chromatic aberration correction (CAC)
 */
int atomisp_gdc_cac(struct atomisp_sub_device *asd, int flag,
		    __s32 *value)
{
	struct atomisp_device *isp = asd->isp;

	if (flag == 0) {
		*value = asd->params.gdc_cac_en;
		return 0;
	}

	asd->params.gdc_cac_en = !!*value;
	if (asd->params.gdc_cac_en) {
		atomisp_css_set_morph_table(asd,
				isp->inputs[asd->input_curr].morph_table);
	} else {
		atomisp_css_set_morph_table(asd, NULL);
	}
	asd->params.css_update_params_needed = true;
	atomisp_update_capture_mode(asd);
	return 0;
}

/*
 * Function to enable/disable low light mode including ANR
 */
int atomisp_low_light(struct atomisp_sub_device *asd, int flag,
		      __s32 *value)
{
	if (flag == 0) {
		*value = asd->params.low_light;
		return 0;
	}

	asd->params.low_light = (*value != 0);
	atomisp_update_capture_mode(asd);
	return 0;
}

/*
 * Function to enable/disable extra noise reduction (XNR) in low light
 * condition
 */
int atomisp_xnr(struct atomisp_sub_device *asd, int flag,
		int *xnr_enable)
{
	if (flag == 0) {
		*xnr_enable = asd->params.xnr_en;
		return 0;
	}

	atomisp_css_capture_enable_xnr(asd, !!*xnr_enable);

	return 0;
}

/*
 * Function to configure bayer noise reduction
 */
int atomisp_nr(struct atomisp_sub_device *asd, int flag,
	       struct atomisp_nr_config *arg)
{
	if (flag == 0) {
		/* Get nr config from current setup */
		if (atomisp_css_get_nr_config(asd, arg))
			return -EINVAL;
	} else {
		/* Set nr config to isp parameters */
		memcpy(&asd->params.nr_config, arg,
			sizeof(struct atomisp_css_nr_config));
		atomisp_css_set_nr_config(asd, &asd->params.nr_config);
		asd->params.css_update_params_needed = true;
	}
	return 0;
}

/*
 * Function to configure temporal noise reduction (TNR)
 */
int atomisp_tnr(struct atomisp_sub_device *asd, int flag,
		struct atomisp_tnr_config *config)
{
	/* Get tnr config from current setup */
	if (flag == 0) {
		/* Get tnr config from current setup */
		if (atomisp_css_get_tnr_config(asd, config))
			return -EINVAL;
	} else {
		/* Set tnr config to isp parameters */
		memcpy(&asd->params.tnr_config, config,
			sizeof(struct atomisp_css_tnr_config));
		atomisp_css_set_tnr_config(asd, &asd->params.tnr_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to configure black level compensation
 */
int atomisp_black_level(struct atomisp_sub_device *asd, int flag,
			struct atomisp_ob_config *config)
{
	if (flag == 0) {
		/* Get ob config from current setup */
		if (atomisp_css_get_ob_config(asd, config))
			return -EINVAL;
	} else {
		/* Set ob config to isp parameters */
		memcpy(&asd->params.ob_config, config,
			sizeof(struct atomisp_css_ob_config));
		atomisp_css_set_ob_config(asd, &asd->params.ob_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to configure edge enhancement
 */
int atomisp_ee(struct atomisp_sub_device *asd, int flag,
	       struct atomisp_ee_config *config)
{
	if (flag == 0) {
		/* Get ee config from current setup */
		if (atomisp_css_get_ee_config(asd, config))
			return -EINVAL;
	} else {
		/* Set ee config to isp parameters */
		memcpy(&asd->params.ee_config, config,
		       sizeof(asd->params.ee_config));
		atomisp_css_set_ee_config(asd, &asd->params.ee_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to update Gamma table for gamma, brightness and contrast config
 */
int atomisp_gamma(struct atomisp_sub_device *asd, int flag,
		  struct atomisp_gamma_table *config)
{
	if (flag == 0) {
		/* Get gamma table from current setup */
		if (atomisp_css_get_gamma_table(asd, config))
			return -EINVAL;
	} else {
		/* Set gamma table to isp parameters */
		memcpy(&asd->params.gamma_table, config,
		       sizeof(asd->params.gamma_table));
		atomisp_css_set_gamma_table(asd, &asd->params.gamma_table);
	}

	return 0;
}

/*
 * Function to update Ctc table for Chroma Enhancement
 */
int atomisp_ctc(struct atomisp_sub_device *asd, int flag,
		struct atomisp_ctc_table *config)
{
	if (flag == 0) {
		/* Get ctc table from current setup */
		if (atomisp_css_get_ctc_table(asd, config))
			return -EINVAL;
	} else {
		/* Set ctc table to isp parameters */
		memcpy(&asd->params.ctc_table, config,
			sizeof(asd->params.ctc_table));
		atomisp_css_set_ctc_table(asd, &asd->params.ctc_table);
	}

	return 0;
}

/*
 * Function to update gamma correction parameters
 */
int atomisp_gamma_correction(struct atomisp_sub_device *asd, int flag,
	struct atomisp_gc_config *config)
{
	if (flag == 0) {
		/* Get gamma correction params from current setup */
		if (atomisp_css_get_gc_config(asd, config))
			return -EINVAL;
	} else {
		/* Set gamma correction params to isp parameters */
		memcpy(&asd->params.gc_config, config,
			sizeof(asd->params.gc_config));
		atomisp_css_set_gc_config(asd, &asd->params.gc_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

void atomisp_free_internal_buffers(struct atomisp_sub_device *asd)
{
	struct atomisp_css_morph_table *tab;
	struct atomisp_device *isp = asd->isp;

	tab = isp->inputs[asd->input_curr].morph_table;
	if (tab) {
		atomisp_css_morph_table_free(tab);
		isp->inputs[asd->input_curr].morph_table = NULL;
	}
	if (asd->raw_output_frame) {
		atomisp_css_frame_free(asd->raw_output_frame);
		asd->raw_output_frame = NULL;
	}
}

void atomisp_free_3a_dis_buffers(struct atomisp_sub_device *asd)
{
	atomisp_css_free_3a_dis_buffers(asd);
}

static void atomisp_update_grid_info(struct atomisp_sub_device *asd,
				enum atomisp_css_pipe_id pipe_id, int source_pad)
{
	struct atomisp_device *isp = asd->isp;
	int err;

	if (atomisp_css_get_grid_info(asd, pipe_id, source_pad))
		return;

	/* We must free all buffers because they no longer match
	   the grid size. */
	atomisp_free_3a_dis_buffers(asd);

	err = atomisp_alloc_css_stat_bufs(asd);
	if (err) {
		dev_err(isp->dev, "stat_buf allocate error\n");
		goto err_3a;
	}

	if (atomisp_alloc_3a_output_buf(asd))
		goto err_3a;

	if (atomisp_alloc_dis_coef_buf(asd))
		goto err_dis;

	return;

	/* Failure for 3A buffers does not influence DIS buffers */
err_3a:
	if (asd->params.s3a_output_bytes != 0) {
		/* For SOC sensor happens s3a_output_bytes == 0,
		*  using if condition to exclude false error log */
		dev_err(isp->dev, "Failed allocate memory for 3A statistics\n");
	}
	atomisp_free_3a_dis_buffers(asd);
	return;

err_dis:
	dev_err(isp->dev, "Failed allocate memory for DIS statistics\n");
	atomisp_free_3a_dis_buffers(asd);
}

static void atomisp_curr_user_grid_info(struct atomisp_sub_device *asd,
				    struct atomisp_grid_info *info)
{
#ifndef CSS20
	info->isp_in_width          = asd->params.curr_grid_info.isp_in_width;
	info->isp_in_height         =
		asd->params.curr_grid_info.isp_in_height;
	info->s3a_width             =
		asd->params.curr_grid_info.s3a_grid.width;
	info->s3a_height            =
		asd->params.curr_grid_info.s3a_grid.height;
	info->s3a_bqs_per_grid_cell =
		asd->params.curr_grid_info.s3a_grid.bqs_per_grid_cell;

	info->dis_width          = asd->params.curr_grid_info.dvs_grid.width;
	info->dis_aligned_width  =
		asd->params.curr_grid_info.dvs_grid.aligned_width;
	info->dis_height         = asd->params.curr_grid_info.dvs_grid.height;
	info->dis_aligned_height =
		asd->params.curr_grid_info.dvs_grid.aligned_height;
	info->dis_bqs_per_grid_cell =
		asd->params.curr_grid_info.dvs_grid.bqs_per_grid_cell;
	info->dis_hor_coef_num      =
		asd->params.curr_grid_info.dvs_hor_coef_num;
	info->dis_ver_coef_num      =
		asd->params.curr_grid_info.dvs_ver_coef_num;
#else /* CSS20 */
	memcpy(info, &asd->params.curr_grid_info.s3a_grid,
			sizeof(struct atomisp_css_3a_grid_info));
#endif /* CSS20 */
}

int atomisp_compare_grid(struct atomisp_sub_device *asd,
				struct atomisp_grid_info *atomgrid)
{
	struct atomisp_grid_info tmp = {0};

	atomisp_curr_user_grid_info(asd, &tmp);
	return memcmp(atomgrid, &tmp, sizeof(tmp));
}

/*
 * Function to update Gdc table for gdc
 */
int atomisp_gdc_cac_table(struct atomisp_sub_device *asd, int flag,
			  struct atomisp_morph_table *config)
{
	int ret;
	int i;
	struct atomisp_device *isp = asd->isp;

	if (flag == 0) {
		/* Get gdc table from current setup */
		struct atomisp_css_morph_table tab = {0};
		atomisp_css_get_morph_table(asd, &tab);

		config->width = tab.width;
		config->height = tab.height;

		for (i = 0; i < CSS_MORPH_TABLE_NUM_PLANES; i++) {
			ret = copy_to_user(config->coordinates_x[i],
				tab.coordinates_x[i], tab.height *
				tab.width * sizeof(*tab.coordinates_x[i]));
			if (ret) {
				dev_err(isp->dev,
					"Failed to copy to User for x\n");
				return -EFAULT;
			}
			ret = copy_to_user(config->coordinates_y[i],
				tab.coordinates_y[i], tab.height *
				tab.width * sizeof(*tab.coordinates_y[i]));
			if (ret) {
				dev_err(isp->dev,
					"Failed to copy to User for y\n");
				return -EFAULT;
			}
		}
	} else {
		struct atomisp_css_morph_table *tab =
			isp->inputs[asd->input_curr].morph_table;

		/* free first if we have one */
		if (tab) {
			atomisp_css_morph_table_free(tab);
			isp->inputs[asd->input_curr].morph_table = NULL;
		}

		/* allocate new one */
		tab = atomisp_css_morph_table_allocate(config->width,
						  config->height);

		if (!tab) {
			dev_err(isp->dev, "out of memory\n");
			return -EINVAL;
		}

		for (i = 0; i < CSS_MORPH_TABLE_NUM_PLANES; i++) {
			ret = copy_from_user(tab->coordinates_x[i],
				config->coordinates_x[i],
				config->height * config->width *
				sizeof(*config->coordinates_x[i]));
			if (ret) {
				dev_err(isp->dev,
				"Failed to copy from User for x, ret %d\n",
				ret);
				atomisp_css_morph_table_free(tab);
				return -EFAULT;
			}
			ret = copy_from_user(tab->coordinates_y[i],
				config->coordinates_y[i],
				config->height * config->width *
				sizeof(*config->coordinates_y[i]));
			if (ret) {
				dev_err(isp->dev,
				"Failed to copy from User for y, ret is %d\n",
				ret);
				atomisp_css_morph_table_free(tab);
				return -EFAULT;
			}
		}
		isp->inputs[asd->input_curr].morph_table = tab;
		if (asd->params.gdc_cac_en)
			atomisp_css_set_morph_table(asd, tab);
	}

	return 0;
}

int atomisp_macc_table(struct atomisp_sub_device *asd, int flag,
		       struct atomisp_macc_config *config)
{
	struct atomisp_css_macc_table *macc_table;

	switch (config->color_effect) {
	case V4L2_COLORFX_NONE:
		macc_table = &asd->params.macc_table;
		break;
	case V4L2_COLORFX_SKY_BLUE:
		macc_table = &blue_macc_table;
		break;
	case V4L2_COLORFX_GRASS_GREEN:
		macc_table = &green_macc_table;
		break;
	case V4L2_COLORFX_SKIN_WHITEN_LOW:
		macc_table = &skin_low_macc_table;
		break;
	case V4L2_COLORFX_SKIN_WHITEN:
		macc_table = &skin_medium_macc_table;
		break;
	case V4L2_COLORFX_SKIN_WHITEN_HIGH:
		macc_table = &skin_high_macc_table;
		break;
	default:
		return -EINVAL;
	}

	if (flag == 0) {
		/* Get macc table from current setup */
		memcpy(&config->table, macc_table,
		       sizeof(struct atomisp_css_macc_table));
	} else {
		memcpy(macc_table, &config->table,
		       sizeof(struct atomisp_css_macc_table));
		if (config->color_effect == asd->params.color_effect)
			atomisp_css_set_macc_table(asd, macc_table);
	}

	return 0;
}

int atomisp_set_dis_vector(struct atomisp_sub_device *asd,
			   struct atomisp_dis_vector *vector)
{
	atomisp_css_video_set_dis_vector(asd, vector);

	asd->params.dis_proj_data_valid = false;
	asd->params.css_update_params_needed = true;
	return 0;
}

/*
 * Function to set/get image stablization statistics
 */
int atomisp_get_dis_stat(struct atomisp_sub_device *asd,
			 struct atomisp_dis_statistics *stats)
{
	return atomisp_css_get_dis_stat(asd, stats);
}

int atomisp_set_dis_coefs(struct atomisp_sub_device *asd,
			  struct atomisp_dis_coefficients *coefs)
{
	return atomisp_css_set_dis_coefs(asd, coefs);
}

/*
 * Function to set/get 3A stat from isp
 */
int atomisp_3a_stat(struct atomisp_sub_device *asd, int flag,
		    struct atomisp_3a_statistics *config)
{
	unsigned long ret;
	struct atomisp_device *isp = asd->isp;

	if (flag != 0)
		return -EINVAL;

	/* sanity check to avoid writing into unallocated memory. */
	if (asd->params.s3a_output_bytes == 0)
		return -EINVAL;

	if (atomisp_compare_grid(asd, &config->grid_info) != 0) {
		/* If the grid info in the argument differs from the current
		   grid info, we tell the caller to reset the grid size and
		   try again. */
		return -EAGAIN;
	}

	/* This is done in the atomisp_s3a_buf_done() */
	if (!asd->params.s3a_buf_data_valid) {
		dev_err(isp->dev, "3a statistics is not valid.\n");
		return -EAGAIN;
	}

#ifdef CSS20
	ret = copy_to_user(config->data, asd->params.s3a_user_stat->data,
			   asd->params.s3a_output_bytes);
#else /* CSS20 */
	ret = copy_to_user(config->data, asd->params.s3a_output_buf,
			   asd->params.s3a_output_bytes);
#endif /* CSS20 */
	if (ret) {
		dev_err(isp->dev, "copy to user failed: copied %lu bytes\n",
				ret);
		return -EFAULT;
	}
	return 0;
}

static int __atomisp_set_general_isp_parameters(
					struct atomisp_sub_device *asd,
					struct atomisp_parameters *arg)
{

	/* TODO: add cnr_config when it's ready */
	if (arg->wb_config) {
		if (copy_from_user(&asd->params.wb_config, arg->wb_config,
				   sizeof(struct atomisp_css_wb_config)))
			return -EFAULT;
		atomisp_css_set_wb_config(asd, &asd->params.wb_config);
	}

	if (arg->ob_config) {
		if (copy_from_user(&asd->params.ob_config, arg->ob_config,
				   sizeof(struct atomisp_css_ob_config)))
			return -EFAULT;
		atomisp_css_set_ob_config(asd, &asd->params.ob_config);
	}

	if (arg->dp_config) {
		if (copy_from_user(&asd->params.dp_config, arg->dp_config,
				   sizeof(struct atomisp_css_dp_config)))
			return -EFAULT;
		atomisp_css_set_dp_config(asd, &asd->params.dp_config);
	}

	if (arg->de_config) {
		if (copy_from_user(&asd->params.de_config, arg->de_config,
				   sizeof(struct atomisp_css_de_config)))
			return -EFAULT;
		atomisp_css_set_de_config(asd, &asd->params.de_config);
	}

	if (arg->ce_config) {
		if (copy_from_user(&asd->params.ce_config, arg->ce_config,
				   sizeof(struct atomisp_css_ce_config)))
			return -EFAULT;
		atomisp_css_set_ce_config(asd, &asd->params.ce_config);
	}

	if (arg->nr_config) {
		if (copy_from_user(&asd->params.nr_config, arg->nr_config,
				   sizeof(struct atomisp_css_nr_config)))
			return -EFAULT;
		atomisp_css_set_nr_config(asd, &asd->params.nr_config);
	}

	if (arg->ee_config) {
		if (copy_from_user(&asd->params.ee_config, arg->ee_config,
				   sizeof(struct atomisp_css_ee_config)))
			return -EFAULT;
		atomisp_css_set_ee_config(asd, &asd->params.ee_config);
	}

	if (arg->tnr_config) {
		if (copy_from_user(&asd->params.tnr_config, arg->tnr_config,
				   sizeof(struct atomisp_css_tnr_config)))
			return -EFAULT;
		atomisp_css_set_tnr_config(asd, &asd->params.tnr_config);
	}

	if (arg->cc_config) {
		if (copy_from_user(&asd->params.cc_config, arg->cc_config,
				   sizeof(struct atomisp_css_cc_config)))
			return -EFAULT;
		atomisp_css_set_cc_config(asd, &asd->params.cc_config);
	}

	if (arg->ctc_table) {
		if (copy_from_user(&asd->params.ctc_table,
				   arg->ctc_table,
				   sizeof(asd->params.ctc_table)))
			return -EFAULT;
		atomisp_css_set_ctc_table(asd, &asd->params.ctc_table);
	}

	if (arg->gc_config) {
		if (copy_from_user(&asd->params.gc_config, arg->gc_config,
				   sizeof(asd->params.gc_config)))
			return -EFAULT;
		atomisp_css_set_gc_config(asd, &asd->params.gc_config);
	}

	if (arg->a3a_config) {
		if (copy_from_user(&asd->params.s3a_config, arg->a3a_config,
				   sizeof(asd->params.s3a_config)))
			return -EFAULT;
		atomisp_css_set_3a_config(asd, &asd->params.s3a_config);
	}

	if (arg->macc_config) {
		if (copy_from_user(&asd->params.macc_table,
			&arg->macc_config->table,
			sizeof(struct atomisp_css_macc_table)))
			return -EFAULT;
		asd->params.color_effect = arg->macc_config->color_effect;
		atomisp_css_set_macc_table(asd, &asd->params.macc_table);
	}

	if (arg->gamma_table) {
		if (copy_from_user(&asd->params.gamma_table, arg->gamma_table,
			sizeof(asd->params.gamma_table)))
			return -EFAULT;
		atomisp_css_set_gamma_table(asd, &asd->params.gamma_table);
	}

#ifdef CSS20
	if (arg->ctc_config) {
		if (copy_from_user(&asd->params.ctc_config, arg->ctc_config,
					sizeof(struct atomisp_css_ctc_config)))
			return -EFAULT;
		atomisp_css_set_ctc_config(asd, &asd->params.ctc_config);
	}

	if (arg->cnr_config) {
		if (copy_from_user(&asd->params.cnr_config, arg->cnr_config,
				   sizeof(struct atomisp_css_cnr_config)))
			return -EFAULT;
		atomisp_css_set_cnr_config(asd, &asd->params.cnr_config);
	}

	if (arg->ecd_config) {
		if (copy_from_user(&asd->params.ecd_config, arg->ecd_config,
				   sizeof(struct atomisp_css_ecd_config)))
			return -EFAULT;
		atomisp_css_set_ecd_config(asd, &asd->params.ecd_config);
	}

	if (arg->ynr_config) {
		if (copy_from_user(&asd->params.ynr_config, arg->ynr_config,
				   sizeof(struct atomisp_css_ynr_config)))
			return -EFAULT;
		atomisp_css_set_ynr_config(asd, &asd->params.ynr_config);
	}

	if (arg->fc_config) {
		if (copy_from_user(&asd->params.fc_config, arg->fc_config,
				   sizeof(struct atomisp_css_fc_config)))
			return -EFAULT;
		atomisp_css_set_fc_config(asd, &asd->params.fc_config);
	}

	if (arg->macc_config) {
		if (copy_from_user(&asd->params.macc_config, arg->macc_config,
				   sizeof(struct atomisp_css_macc_config)))
			return -EFAULT;
		atomisp_css_set_macc_config(asd, &asd->params.macc_config);
	}

	if (arg->aa_config) {
		if (copy_from_user(&asd->params.aa_config, arg->aa_config,
				   sizeof(struct atomisp_css_aa_config)))
			return -EFAULT;
		atomisp_css_set_aa_config(asd, &asd->params.aa_config);
	}

	if (arg->anr_config) {
		if (copy_from_user(&asd->params.anr_config, arg->anr_config,
				   sizeof(struct atomisp_css_anr_config)))
			return -EFAULT;
		atomisp_css_set_anr_config(asd, &asd->params.anr_config);
	}

	if (arg->xnr_config) {
		if (copy_from_user(&asd->params.xnr_config, arg->xnr_config,
				   sizeof(struct atomisp_css_xnr_config)))
			return -EFAULT;
		atomisp_css_set_xnr_config(asd, &asd->params.xnr_config);
	}

	if (arg->yuv2rgb_cc_config) {
		if (copy_from_user(&asd->params.yuv2rgb_cc_config,
				   arg->yuv2rgb_cc_config,
				   sizeof(struct atomisp_css_cc_config)))
			return -EFAULT;
		atomisp_css_set_yuv2rgb_cc_config(asd,
					&asd->params.yuv2rgb_cc_config);
	}

	if (arg->rgb2yuv_cc_config) {
		if (copy_from_user(&asd->params.rgb2yuv_cc_config,
				   arg->rgb2yuv_cc_config,
				   sizeof(struct atomisp_css_cc_config)))
			return -EFAULT;
		atomisp_css_set_rgb2yuv_cc_config(asd,
					&asd->params.rgb2yuv_cc_config);
	}

	if (arg->macc_table) {
		if (copy_from_user(&asd->params.macc_table, arg->macc_table,
				   sizeof(struct atomisp_css_macc_table)))
			return -EFAULT;
		atomisp_css_set_macc_table(asd, &asd->params.macc_table);
	}

	if (arg->xnr_table) {
		if (copy_from_user(&asd->params.xnr_table, arg->xnr_table,
				   sizeof(struct atomisp_css_xnr_table)))
			return -EFAULT;
		atomisp_css_set_xnr_table(asd, &asd->params.xnr_table);
	}

	if (arg->r_gamma_table) {
		if (copy_from_user(&asd->params.r_gamma_table,
				   arg->r_gamma_table,
				   sizeof(struct atomisp_css_rgb_gamma_table)))
			return -EFAULT;
		atomisp_css_set_r_gamma_table(asd, &asd->params.
					      r_gamma_table);
	}

	if (arg->g_gamma_table) {
		if (copy_from_user(&asd->params.g_gamma_table,
				   arg->g_gamma_table,
				   sizeof(struct atomisp_css_rgb_gamma_table)))
			return -EFAULT;
		atomisp_css_set_g_gamma_table(asd, &asd->params.
					      g_gamma_table);
	}

	if (arg->b_gamma_table) {
		if (copy_from_user(&asd->params.b_gamma_table,
				   arg->b_gamma_table,
				   sizeof(struct atomisp_css_rgb_gamma_table)))
			return -EFAULT;
		atomisp_css_set_b_gamma_table(asd, &asd->params.
					      b_gamma_table);
	}

	if (arg->anr_thres) {
		if (copy_from_user(&asd->params.anr_thres, arg->anr_thres,
				   sizeof(struct atomisp_css_anr_thres)))
			return -EFAULT;
		atomisp_css_set_anr_thres(asd, &asd->params.anr_thres);
	}
#endif /* CSS20 */
	return 0;
}

static int __atomisp_set_lsc_table(struct atomisp_sub_device *asd,
			struct atomisp_shading_table *user_st)
{
	unsigned int i;
	unsigned int len_table;
	struct atomisp_css_shading_table *shading_table;
	struct atomisp_css_shading_table *old_table;
	struct atomisp_device *isp = asd->isp;

	if (!user_st)
		return 0;

	old_table = isp->inputs[asd->input_curr].shading_table;

	/* user config is to disable the shading table. */
	if (!user_st->enable) {
		shading_table = NULL;
		goto set_lsc;
	}

	/* Setting a new table. Validate first - all tables must be set */
	for (i = 0; i < ATOMISP_NUM_SC_COLORS; i++) {
		if (!user_st->data[i])
			return -EINVAL;
	}

	/* Shading table size per color */
	if (user_st->width > SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR ||
		user_st->height > SH_CSS_MAX_SCTBL_HEIGHT_PER_COLOR)
		return -EINVAL;

	shading_table = atomisp_css_shading_table_alloc(user_st->width,
							user_st->height);
	if (!shading_table)
			return -ENOMEM;

	len_table = user_st->width * user_st->height * ATOMISP_SC_TYPE_SIZE;
	for (i = 0; i < ATOMISP_NUM_SC_COLORS; i++) {
		if (copy_from_user(shading_table->data[i],
			user_st->data[i], len_table)) {
			atomisp_css_shading_table_free(shading_table);
			return -EFAULT;
		}

	}
	shading_table->sensor_width = user_st->sensor_width;
	shading_table->sensor_height = user_st->sensor_height;
	shading_table->fraction_bits = user_st->fraction_bits;
#ifdef CSS20
	shading_table->enable = user_st->enable;

	/* No need to update shading table if it is the same */
	if (old_table != NULL &&
		old_table->sensor_width == shading_table->sensor_width &&
		old_table->sensor_height == shading_table->sensor_height &&
		old_table->width == shading_table->width &&
		old_table->height == shading_table->height &&
		old_table->fraction_bits == shading_table->fraction_bits &&
		old_table->enable == shading_table->enable) {
		bool data_is_same = true;

		for (i = 0; i < ATOMISP_NUM_SC_COLORS; i++) {
			if (memcmp(shading_table->data[i], old_table->data[i],
				len_table) != 0) {
				data_is_same = false;
				break;
			}
		}

		if (data_is_same) {
			atomisp_css_shading_table_free(shading_table);
			return 0;
		}
	}
#endif

set_lsc:
	/* set LSC to CSS */
	isp->inputs[asd->input_curr].shading_table = shading_table;
	atomisp_css_set_shading_table(asd, shading_table);
	asd->params.sc_en = shading_table != NULL;

	if (old_table)
		atomisp_css_shading_table_free(old_table);

	return 0;
}

#ifdef CSS20
#include "ia_css_stream.h"	/* FIXME */
int atomisp_set_dvs_6axis_config(struct atomisp_sub_device *asd,
					  struct atomisp_dvs_6axis_config
					  *user_6axis_config)
{
	struct atomisp_css_dvs_6axis_config *dvs_6axis_config;
	struct atomisp_css_dvs_6axis_config *old_6axis_config;
	struct ia_css_stream *stream = asd->stream_env[ATOMISP_INPUT_STREAM_GENERAL].stream;
	struct ia_css_dvs_6axis_config *stream_dvs_config =
	    stream->isp_params_configs->dvs_6axis_config;
	int ret = -EFAULT;

	if (!user_6axis_config)
		return 0;

	if (!stream_dvs_config)
		return -EAGAIN;

	if (stream_dvs_config->width_y != user_6axis_config->width_y ||
	    stream_dvs_config->height_y != user_6axis_config->height_y ||
	    stream_dvs_config->width_uv != user_6axis_config->width_uv ||
	    stream_dvs_config->height_uv != user_6axis_config->height_uv) {
		dev_err(asd->isp->dev, "%s: mismatch 6axis config!", __func__);
		dev_err(asd->isp->dev, "CSS expected:width_y:%d, height_y:%d, width_uv:%d, height_uv:%d.\n",
			 stream_dvs_config->width_y,
			 stream_dvs_config->height_y,
			 stream_dvs_config->width_uv,
			 stream_dvs_config->height_uv);
		dev_err(asd->isp->dev, "User space:width_y:%d, height_y:%d, width_uv:%d, height_uv:%d.\n",
			 user_6axis_config->width_y,
			 user_6axis_config->height_y,
			 user_6axis_config->width_uv,
			 user_6axis_config->height_uv);
		return -EINVAL;
	}

	/* check whether need to reallocate for 6 axis config */
	old_6axis_config = asd->params.dvs_6axis;
	dvs_6axis_config = old_6axis_config;
	if (old_6axis_config &&
	    (old_6axis_config->width_y != user_6axis_config->width_y ||
	     old_6axis_config->height_y != user_6axis_config->height_y ||
	     old_6axis_config->width_uv != user_6axis_config->width_uv ||
	     old_6axis_config->height_uv != user_6axis_config->height_uv)) {
		ia_css_dvs2_6axis_config_free(asd->params.dvs_6axis);
		asd->params.dvs_6axis = NULL;

		dvs_6axis_config = ia_css_dvs2_6axis_config_allocate(stream);
		if (!dvs_6axis_config)
			return -ENOMEM;
	} else if (!dvs_6axis_config) {
		dvs_6axis_config = ia_css_dvs2_6axis_config_allocate(stream);
		if (!dvs_6axis_config)
			return -ENOMEM;
	}

	dvs_6axis_config->exp_id = user_6axis_config->exp_id;

	if (copy_from_user(dvs_6axis_config->xcoords_y,
			   user_6axis_config->xcoords_y,
			   user_6axis_config->width_y *
			   user_6axis_config->height_y *
			   sizeof(*user_6axis_config->xcoords_y)))
		goto error;
	if (copy_from_user(dvs_6axis_config->ycoords_y,
			   user_6axis_config->ycoords_y,
			   user_6axis_config->width_y *
			   user_6axis_config->height_y *
			   sizeof(*user_6axis_config->ycoords_y)))
		goto error;
	if (copy_from_user(dvs_6axis_config->xcoords_uv,
			   user_6axis_config->xcoords_uv,
			   user_6axis_config->width_uv *
			   user_6axis_config->height_uv *
			   sizeof(*user_6axis_config->xcoords_uv)))
		goto error;
	if (copy_from_user(dvs_6axis_config->ycoords_uv,
			   user_6axis_config->ycoords_uv,
			   user_6axis_config->width_uv *
			   user_6axis_config->height_uv *
			   sizeof(*user_6axis_config->ycoords_uv)))
		goto error;

	asd->params.dvs_6axis = dvs_6axis_config;
	atomisp_css_set_dvs_6axis(asd, asd->params.dvs_6axis);
	asd->params.css_update_params_needed = true;

	return 0;

error:
	if (dvs_6axis_config)
		ia_css_dvs2_6axis_config_free(dvs_6axis_config);
	return ret;
}
#endif

static int __atomisp_set_morph_table(struct atomisp_sub_device *asd,
				struct atomisp_morph_table *user_morph_table)
{
	int ret = -EFAULT;
	unsigned int i;
	struct atomisp_css_morph_table *morph_table;
	struct atomisp_css_morph_table *old_morph_table;
	struct atomisp_device *isp = asd->isp;

	if (!user_morph_table)
		return 0;

	old_morph_table = isp->inputs[asd->input_curr].morph_table;

	morph_table = atomisp_css_morph_table_allocate(user_morph_table->width,
				user_morph_table->height);
	if (!morph_table)
		return -ENOMEM;

	for (i = 0; i < CSS_MORPH_TABLE_NUM_PLANES; i++) {
		if (copy_from_user(morph_table->coordinates_x[i],
			user_morph_table->coordinates_x[i],
			user_morph_table->height * user_morph_table->width *
			sizeof(*user_morph_table->coordinates_x[i])))
			goto error;

		if (copy_from_user(morph_table->coordinates_y[i],
			user_morph_table->coordinates_y[i],
			user_morph_table->height * user_morph_table->width *
			sizeof(*user_morph_table->coordinates_y[i])))
			goto error;
	}

	isp->inputs[asd->input_curr].morph_table = morph_table;
	if (asd->params.gdc_cac_en)
		atomisp_css_set_morph_table(asd, morph_table);

	if (old_morph_table)
		atomisp_css_morph_table_free(old_morph_table);

	return 0;

error:
	if (morph_table)
		atomisp_css_morph_table_free(morph_table);
	return ret;
}

/*
* Function to configure ISP parameters
*/
int atomisp_set_parameters(struct atomisp_sub_device *asd,
			struct atomisp_parameters *arg)
{
	int ret;

	ret = __atomisp_set_general_isp_parameters(asd, arg);
	if (ret)
		return ret;

	ret = __atomisp_set_lsc_table(asd, arg->shading_table);
	if (ret)
		return ret;

	ret = __atomisp_set_morph_table(asd, arg->morph_table);
	if (ret)
		return ret;

	/* indicate to CSS that we have parametes to be updated */
	asd->params.css_update_params_needed = true;

#ifdef CSS20
	if (asd->stream_env[ATOMISP_INPUT_STREAM_GENERAL].stream
		&& (asd->stream_env[ATOMISP_INPUT_STREAM_GENERAL].stream_state != CSS_STREAM_STARTED
		|| asd->run_mode->val
			== ATOMISP_RUN_MODE_STILL_CAPTURE)) {
		atomisp_css_update_isp_params(asd);
		asd->params.css_update_params_needed = false;
	}
#endif
	return 0;
}

/*
 * Function to set/get isp parameters to isp
 */
int atomisp_param(struct atomisp_sub_device *asd, int flag,
		  struct atomisp_parm *config)
{
	struct atomisp_device *isp = asd->isp;

	/* Read parameter for 3A binary info */
	if (flag == 0) {
		if (&config->info == NULL) {
			dev_err(isp->dev, "ERROR: NULL pointer in grid_info\n");
			return -EINVAL;
		}
		atomisp_curr_user_grid_info(asd, &config->info);
#ifdef CSS20
		/* update dvs grid info */
		memcpy(&config->dvs_grid, &asd->params.curr_grid_info.dvs_grid,
			sizeof(struct atomisp_css_dvs_grid_info));
		/* update dvs envelop info */
		config->dvs_envelop.width =
		    asd->stream_env[ATOMISP_INPUT_STREAM_GENERAL].pipe_configs[IA_CSS_PIPE_ID_VIDEO].
		    dvs_envelope.width;
		config->dvs_envelop.height =
		    asd->stream_env[ATOMISP_INPUT_STREAM_GENERAL].pipe_configs[IA_CSS_PIPE_ID_VIDEO].
		    dvs_envelope.height;
#endif
		return 0;
	}

	memcpy(&asd->params.wb_config, &config->wb_config,
	       sizeof(struct atomisp_css_wb_config));
	memcpy(&asd->params.ob_config, &config->ob_config,
	       sizeof(struct atomisp_css_ob_config));
	memcpy(&asd->params.dp_config, &config->dp_config,
	       sizeof(struct atomisp_css_dp_config));
	memcpy(&asd->params.de_config, &config->de_config,
	       sizeof(struct atomisp_css_de_config));
	memcpy(&asd->params.ce_config, &config->ce_config,
	       sizeof(struct atomisp_css_ce_config));
	memcpy(&asd->params.nr_config, &config->nr_config,
	       sizeof(struct atomisp_css_nr_config));
	memcpy(&asd->params.ee_config, &config->ee_config,
	       sizeof(struct atomisp_css_ee_config));
	memcpy(&asd->params.tnr_config, &config->tnr_config,
	       sizeof(struct atomisp_css_tnr_config));

	if (asd->params.color_effect == V4L2_COLORFX_NEGATIVE) {
		config->cc_config.matrix[3] = -config->cc_config.matrix[3];
		config->cc_config.matrix[4] = -config->cc_config.matrix[4];
		config->cc_config.matrix[5] = -config->cc_config.matrix[5];
		config->cc_config.matrix[6] = -config->cc_config.matrix[6];
		config->cc_config.matrix[7] = -config->cc_config.matrix[7];
		config->cc_config.matrix[8] = -config->cc_config.matrix[8];
	}

	if (asd->params.color_effect != V4L2_COLORFX_SEPIA &&
	    asd->params.color_effect != V4L2_COLORFX_BW) {
		memcpy(&asd->params.cc_config, &config->cc_config,
		       sizeof(struct atomisp_css_cc_config));
		atomisp_css_set_cc_config(asd, &asd->params.cc_config);
	}

	atomisp_css_set_wb_config(asd, &asd->params.wb_config);
	atomisp_css_set_ob_config(asd, &asd->params.ob_config);
	atomisp_css_set_de_config(asd, &asd->params.de_config);
	atomisp_css_set_ce_config(asd, &asd->params.ce_config);
	atomisp_css_set_dp_config(asd, &asd->params.dp_config);
	atomisp_css_set_nr_config(asd, &asd->params.nr_config);
	atomisp_css_set_ee_config(asd, &asd->params.ee_config);
	atomisp_css_set_tnr_config(asd, &asd->params.tnr_config);
	asd->params.css_update_params_needed = true;

	return 0;
}

/*
 * Function to configure color effect of the image
 */
int atomisp_color_effect(struct atomisp_sub_device *asd, int flag,
			 __s32 *effect)
{
	struct atomisp_css_cc_config *cc_config = NULL;
	struct atomisp_css_macc_table *macc_table = NULL;
	struct atomisp_css_ctc_table *ctc_table = NULL;
	int ret = 0;
	struct v4l2_control control;
	struct atomisp_device *isp = asd->isp;

	if (flag == 0) {
		*effect = asd->params.color_effect;
		return 0;
	}


	control.id = V4L2_CID_COLORFX;
	control.value = *effect;
	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				core, s_ctrl, &control);
	/*
	 * if set color effect to sensor successfully, return
	 * 0 directly.
	 */
	if (!ret) {
		asd->params.color_effect = (u32)*effect;
		return 0;
	}

	if (*effect == asd->params.color_effect)
		return 0;

#ifndef CSS20
	/*
	 * restore the default cc and ctc table config:
	 * when change from sepia/mono to macc effect, the default
	 * cc and ctc table should be used.
	 */
	cc_config = asd->params.default_cc_config;
	ctc_table = asd->params.default_ctc_table;

	/*
	 * set macc table to default when change from macc to
	 * sepia/mono,
	 */
	macc_table = asd->params.default_macc_table;
#endif /* CSS20 */
	/*
	 * isp_subdev->params.macc_en should be set to false.
	 */
	asd->params.macc_en = false;

	switch (*effect) {
	case V4L2_COLORFX_NONE:
		macc_table = &asd->params.macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_SEPIA:
		cc_config = &sepia_cc_config;
		break;
	case V4L2_COLORFX_NEGATIVE:
		cc_config = &nega_cc_config;
		break;
	case V4L2_COLORFX_BW:
		cc_config = &mono_cc_config;
		break;
	case V4L2_COLORFX_SKY_BLUE:
		macc_table = &blue_macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_GRASS_GREEN:
		macc_table = &green_macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_SKIN_WHITEN_LOW:
		macc_table = &skin_low_macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_SKIN_WHITEN:
		macc_table = &skin_medium_macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_SKIN_WHITEN_HIGH:
		macc_table = &skin_high_macc_table;
		asd->params.macc_en = true;
		break;
	case V4L2_COLORFX_VIVID:
		ctc_table = &vivid_ctc_table;
		break;
	default:
		return -EINVAL;
	}
	atomisp_update_capture_mode(asd);

	if (cc_config)
		atomisp_css_set_cc_config(asd, cc_config);
	if (macc_table)
		atomisp_css_set_macc_table(asd, macc_table);
	if (ctc_table)
		atomisp_css_set_ctc_table(asd, ctc_table);
	asd->params.color_effect = (u32)*effect;
	asd->params.css_update_params_needed = true;
	return 0;
}

/*
 * Function to configure bad pixel correction
 */
int atomisp_bad_pixel(struct atomisp_sub_device *asd, int flag,
		      __s32 *value)
{

	if (flag == 0) {
		*value = asd->params.bad_pixel_en;
		return 0;
	}
	asd->params.bad_pixel_en = !!*value;

	return 0;
}

/*
 * Function to configure bad pixel correction params
 */
int atomisp_bad_pixel_param(struct atomisp_sub_device *asd, int flag,
			    struct atomisp_dp_config *config)
{
	if (flag == 0) {
		/* Get bad pixel from current setup */
		if (atomisp_css_get_dp_config(asd, config))
			return -EINVAL;
	} else {
		/* Set bad pixel to isp parameters */
		memcpy(&asd->params.dp_config, config,
			sizeof(asd->params.dp_config));
		atomisp_css_set_dp_config(asd, &asd->params.dp_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to enable/disable video image stablization
 */
int atomisp_video_stable(struct atomisp_sub_device *asd, int flag,
			 __s32 *value)
{
	if (flag == 0)
		*value = asd->params.video_dis_en;
	else
		asd->params.video_dis_en = !!*value;

	return 0;
}

/*
 * Function to configure fixed pattern noise
 */
int atomisp_fixed_pattern(struct atomisp_sub_device *asd, int flag,
			  __s32 *value)
{

	if (flag == 0) {
		*value = asd->params.fpn_en;
		return 0;
	}

	if (*value == 0) {
		asd->params.fpn_en = 0;
		return 0;
	}

	/* Add function to get black from from sensor with shutter off */
	return 0;
}

static unsigned int
atomisp_bytesperline_to_padded_width(unsigned int bytesperline,
				     enum atomisp_css_frame_format format)
{
	switch (format) {
	case CSS_FRAME_FORMAT_UYVY:
	case CSS_FRAME_FORMAT_YUYV:
	case CSS_FRAME_FORMAT_RAW:
	case CSS_FRAME_FORMAT_RGB565:
		return bytesperline/2;
	case CSS_FRAME_FORMAT_RGBA888:
		return bytesperline/4;
	/* The following cases could be removed, but we leave them
	   in to document the formats that are included. */
	case CSS_FRAME_FORMAT_NV11:
	case CSS_FRAME_FORMAT_NV12:
	case CSS_FRAME_FORMAT_NV16:
	case CSS_FRAME_FORMAT_NV21:
	case CSS_FRAME_FORMAT_NV61:
	case CSS_FRAME_FORMAT_YV12:
	case CSS_FRAME_FORMAT_YV16:
	case CSS_FRAME_FORMAT_YUV420:
	case CSS_FRAME_FORMAT_YUV420_16:
	case CSS_FRAME_FORMAT_YUV422:
	case CSS_FRAME_FORMAT_YUV422_16:
	case CSS_FRAME_FORMAT_YUV444:
	case CSS_FRAME_FORMAT_YUV_LINE:
	case CSS_FRAME_FORMAT_PLANAR_RGB888:
	case CSS_FRAME_FORMAT_QPLANE6:
	case CSS_FRAME_FORMAT_BINARY_8:
	default:
		return bytesperline;
	}
}

static int
atomisp_v4l2_framebuffer_to_css_frame(const struct v4l2_framebuffer *arg,
					 struct atomisp_css_frame **result)
{
	struct atomisp_css_frame *res;
	unsigned int padded_width;
	enum atomisp_css_frame_format sh_format;
	char *tmp_buf = NULL;
	int ret = 0;

	sh_format = v4l2_fmt_to_sh_fmt(arg->fmt.pixelformat);
	padded_width = atomisp_bytesperline_to_padded_width(
					arg->fmt.bytesperline, sh_format);

	/* Note: the padded width on an atomisp_css_frame is in elements, not in
	   bytes. The RAW frame we use here should always be a 16bit RAW
	   frame. This is why we bytesperline/2 is equal to the padded with */
	if (atomisp_css_frame_allocate(&res, arg->fmt.width, arg->fmt.height,
				  sh_format, padded_width, 0)) {
		ret = -ENOMEM;
		goto err;
	}

	tmp_buf = vmalloc(arg->fmt.sizeimage);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto err;
	}
	if (copy_from_user(tmp_buf, (void __user __force *)arg->base,
			   arg->fmt.sizeimage)) {
		ret = -EFAULT;
		goto err;
	}

	if (hmm_store(res->data, tmp_buf, arg->fmt.sizeimage)) {
		ret = -EINVAL;
		goto err;
	}

err:
	if (ret && res)
		atomisp_css_frame_free(res);
	if (tmp_buf)
		vfree(tmp_buf);
	if (ret == 0)
		*result = res;
	return ret;
}

/*
 * Function to configure fixed pattern noise table
 */
int atomisp_fixed_pattern_table(struct atomisp_sub_device *asd,
				struct v4l2_framebuffer *arg)
{
	struct atomisp_css_frame *raw_black_frame = NULL;
	int ret;

	if (arg == NULL)
		return -EINVAL;

	ret = atomisp_v4l2_framebuffer_to_css_frame(arg, &raw_black_frame);
	if (ret)
		return ret;
	if (atomisp_css_set_black_frame(asd, raw_black_frame))
		ret = -ENOMEM;

	atomisp_css_frame_free(raw_black_frame);
	return ret;
}

/*
 * Function to configure false color correction
 */
int atomisp_false_color(struct atomisp_sub_device *asd, int flag,
			__s32 *value)
{
	/* Get nr config from current setup */
	if (flag == 0) {
		*value = asd->params.false_color;
		return 0;
	}

	/* Set nr config to isp parameters */
	if (*value) {
		atomisp_css_set_default_de_config(asd);
	} else {
		asd->params.de_config.pixelnoise = 0;
		atomisp_css_set_de_config(asd, &asd->params.de_config);
	}
	asd->params.css_update_params_needed = true;
	asd->params.false_color = *value;
	return 0;
}

/*
 * Function to configure bad pixel correction params
 */
int atomisp_false_color_param(struct atomisp_sub_device *asd, int flag,
			      struct atomisp_de_config *config)
{
	if (flag == 0) {
		/* Get false color from current setup */
		if (atomisp_css_get_de_config(asd, config))
			return -EINVAL;
	} else {
		/* Set false color to isp parameters */
		memcpy(&asd->params.de_config, config,
				sizeof(asd->params.de_config));
		atomisp_css_set_de_config(asd, &asd->params.de_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to configure white balance params
 */
int atomisp_white_balance_param(struct atomisp_sub_device *asd, int flag,
	struct atomisp_wb_config *config)
{
	if (flag == 0) {
		/* Get white balance from current setup */
		if (atomisp_css_get_wb_config(asd, config))
			return -EINVAL;
	} else {
		/* Set white balance to isp parameters */
		memcpy(&asd->params.wb_config, config,
				sizeof(asd->params.wb_config));
		atomisp_css_set_wb_config(asd, &asd->params.wb_config);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

int atomisp_3a_config_param(struct atomisp_sub_device *asd, int flag,
			    struct atomisp_3a_config *config)
{
	struct atomisp_device *isp = asd->isp;

	dev_dbg(isp->dev, ">%s %d\n", __func__, flag);

	if (flag == 0) {
		/* Get white balance from current setup */
		if (atomisp_css_get_3a_config(asd, config))
			return -EINVAL;
	} else {
		/* Set white balance to isp parameters */
		memcpy(&asd->params.s3a_config, config,
				sizeof(asd->params.s3a_config));
		atomisp_css_set_3a_config(asd, &asd->params.s3a_config);
		asd->params.css_update_params_needed = true;
		/* isp_subdev->params.s3a_buf_data_valid = false; */
	}

	dev_dbg(isp->dev, "<%s %d\n", __func__, flag);
	return 0;
}

/*
 * Function to enable/disable lens shading correction
 */
int atomisp_shading_correction(struct atomisp_sub_device *asd, int flag,
				       __s32 *value)
{
	struct atomisp_device *isp = asd->isp;

	if (flag == 0) {
		*value = asd->params.sc_en;
		return 0;
	}

	if (*value == 0)
		atomisp_css_set_shading_table(asd, NULL);
	else
		atomisp_css_set_shading_table(asd,
			isp->inputs[asd->input_curr].shading_table);

	asd->params.sc_en = *value;

	return 0;
}

/*
 * Function to setup digital zoom
 */
int atomisp_digital_zoom(struct atomisp_sub_device *asd, int flag,
			 __s32 *value)
{
	u32 zoom;
	struct atomisp_device *isp = asd->isp;

	unsigned int max_zoom =
		IS_ISP24XX(isp) ? MRFLD_MAX_ZOOM_FACTOR : MFLD_MAX_ZOOM_FACTOR;

	if (flag == 0) {
		atomisp_css_get_zoom_factor(asd, &zoom);
		*value = max_zoom - zoom;
	} else {
		if (*value < 0)
			return -EINVAL;

		zoom = max_zoom - min_t(u32, max_zoom - 1, *value);

		dev_dbg(isp->dev, "%s, zoom: %d\n", __func__, zoom);
		atomisp_css_set_zoom_factor(asd, zoom);
		asd->params.css_update_params_needed = true;
	}

	return 0;
}

/*
 * Function to get sensor specific info for current resolution,
 * which will be used for auto exposure conversion.
 */
int atomisp_get_sensor_mode_data(struct atomisp_sub_device *asd,
				 struct atomisp_sensor_mode_data *config)
{
	struct camera_mipi_info *mipi_info;
	struct atomisp_device *isp = asd->isp;

	mipi_info = atomisp_to_sensor_mipi_info(
		isp->inputs[asd->input_curr].camera);
	if (mipi_info == NULL)
		return -EINVAL;

	memcpy(config, &mipi_info->data, sizeof(*config));
	return 0;
}

int atomisp_get_fmt(struct video_device *vdev, struct v4l2_format *f)
{
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	f->fmt.pix = pipe->pix;

	return 0;
}


/* This function looks up the closest available resolution. */
int atomisp_try_fmt(struct video_device *vdev, struct v4l2_format *f,
						bool *res_overflow)
{
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct v4l2_mbus_framefmt snr_mbus_fmt;
	const struct atomisp_format_bridge *fmt;
	int ret;

	if (isp->inputs[asd->input_curr].camera == NULL)
		return -EINVAL;

	fmt = atomisp_get_format_bridge(f->fmt.pix.pixelformat);
	if (fmt == NULL) {
		dev_err(isp->dev, "unsupported pixelformat!\n");
		fmt = atomisp_output_fmts;
	}

	snr_mbus_fmt.code = fmt->mbus_code;
	snr_mbus_fmt.width = f->fmt.pix.width;
	snr_mbus_fmt.height = f->fmt.pix.height;

	dev_dbg(isp->dev, "try_mbus_fmt: asking for %ux%u\n",
		snr_mbus_fmt.width, snr_mbus_fmt.height);

	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
			video, try_mbus_fmt, &snr_mbus_fmt);
	if (ret)
		return ret;

	dev_dbg(isp->dev, "try_mbus_fmt: got %ux%u\n",
		snr_mbus_fmt.width, snr_mbus_fmt.height);

	fmt = atomisp_get_format_bridge_from_mbus(snr_mbus_fmt.code);
	if (fmt == NULL) {
		dev_err(isp->dev, "unknown sensor format 0x%8.8x\n",
			snr_mbus_fmt.code);
		return -EINVAL;
	}

	f->fmt.pix.pixelformat = fmt->pixelformat;

	if (snr_mbus_fmt.width < f->fmt.pix.width
	    && snr_mbus_fmt.height < f->fmt.pix.height) {
		f->fmt.pix.width = snr_mbus_fmt.width;
		f->fmt.pix.height = snr_mbus_fmt.height;
		/* Set the flag when resolution requested is
		 * beyond the max value supported by sensor
		 */
		if (res_overflow != NULL)
			*res_overflow = true;
	}

	/* app vs isp */
	f->fmt.pix.width = rounddown(
		clamp_t(u32, f->fmt.pix.width, ATOM_ISP_MIN_WIDTH,
			ATOM_ISP_MAX_WIDTH), ATOM_ISP_STEP_WIDTH);
	f->fmt.pix.height = rounddown(
		clamp_t(u32, f->fmt.pix.height, ATOM_ISP_MIN_HEIGHT,
			ATOM_ISP_MAX_HEIGHT), ATOM_ISP_STEP_HEIGHT);

	return 0;
}

static int
atomisp_try_fmt_file(struct atomisp_device *isp, struct v4l2_format *f)
{
	u32 width = f->fmt.pix.width;
	u32 height = f->fmt.pix.height;
	u32 pixelformat = f->fmt.pix.pixelformat;
	enum v4l2_field field = f->fmt.pix.field;
	u32 depth;

	if (!atomisp_get_format_bridge(pixelformat)) {
		dev_err(isp->dev, "Wrong output pixelformat\n");
		return -EINVAL;
	}

	depth = get_pixel_depth(pixelformat);

	if (!field || field == V4L2_FIELD_ANY)
		field = V4L2_FIELD_NONE;
	else if (field != V4L2_FIELD_NONE) {
		dev_err(isp->dev, "Wrong output field\n");
		return -EINVAL;
	}

	f->fmt.pix.field = field;
	f->fmt.pix.width = clamp_t(u32,
				   rounddown(width, (u32)ATOM_ISP_STEP_WIDTH),
				   ATOM_ISP_MIN_WIDTH, ATOM_ISP_MAX_WIDTH);
	f->fmt.pix.height = clamp_t(u32, rounddown(height,
						   (u32)ATOM_ISP_STEP_HEIGHT),
				    ATOM_ISP_MIN_HEIGHT, ATOM_ISP_MAX_HEIGHT);
	f->fmt.pix.bytesperline = (width * depth) >> 3;

	return 0;
}

static mipi_port_ID_t __get_mipi_port(struct atomisp_device *isp,
				      enum atomisp_camera_port port)
{
	switch (port) {
	case ATOMISP_CAMERA_PORT_PRIMARY:
		return MIPI_PORT0_ID;
	case ATOMISP_CAMERA_PORT_SECONDARY:
		return MIPI_PORT1_ID;
	case ATOMISP_CAMERA_PORT_TERTIARY:
		if (MIPI_PORT1_ID + 1 != N_MIPI_PORT_ID)
			return MIPI_PORT1_ID + 1;
		/* go through down for else case */
	default:
		dev_err(isp->dev, "unsupported port: %d\n", port);
		return MIPI_PORT0_ID;
	}
}

static inline void atomisp_set_sensor_mipi_to_isp(
				struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				struct camera_mipi_info *mipi_info)
{
	/* Compatibility for sensors which provide no media bus code
	 * in s_mbus_framefmt() nor support pad formats. */
	if (mipi_info->input_format != -1) {
		atomisp_css_input_set_bayer_order(asd, stream_id,
						mipi_info->raw_bayer_order);
		atomisp_css_input_set_format(asd, stream_id,
						mipi_info->input_format);
	}
	atomisp_css_input_configure_port(asd, __get_mipi_port(asd->isp,
							mipi_info->port),
					mipi_info->num_lanes, 0xffff4);
}

static int __enable_continuous_mode(struct atomisp_sub_device *asd,
				    bool enable)
{
	struct atomisp_device *isp = asd->isp;

	dev_dbg(isp->dev,
		"continuous mode %d, raw buffers %d, stop preview %d\n",
		enable, asd->continuous_raw_buffer_size->val,
		!asd->continuous_viewfinder->val);
	atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_PRIMARY);
	/* in case of ANR, force capture pipe to offline mode */
	atomisp_css_capture_enable_online(asd,
			asd->params.low_light ? false : !enable);
	atomisp_css_preview_enable_online(asd, !enable);
	atomisp_css_enable_continuous(asd, enable);
	atomisp_css_enable_cont_capt(enable,
				!asd->continuous_viewfinder->val);

	if (atomisp_css_continuous_set_num_raw_frames(asd,
			asd->continuous_raw_buffer_size->val)) {
		dev_err(isp->dev, "css_continuous_set_num_raw_frames failed\n");
		return -EINVAL;
	}

	if (!enable) {
		atomisp_css_enable_raw_binning(asd, false);
		atomisp_css_input_set_two_pixels_per_clock(asd, false);
	}

	if (isp->inputs[asd->input_curr].type != FILE_INPUT)
		atomisp_css_input_set_mode(asd, CSS_INPUT_MODE_SENSOR);

	return atomisp_update_run_mode(asd);
}

int configure_pp_input_nop(struct atomisp_sub_device *asd,
			   unsigned int width, unsigned int height)
{
	return 0;
}

int configure_output_nop(struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format sh_fmt)
{
	return 0;
}

int get_frame_info_nop(struct atomisp_sub_device *asd,
				struct atomisp_css_frame_info *finfo)
{
	return 0;
}

/**
 * Resets CSS parameters that depend on input resolution.
 *
 * Update params like CSS RAW binning, 2ppc mode and pp_input
 * which depend on input size, but are not automatically
 * handled in CSS when the input resolution is changed.
 */
static int css_input_resolution_changed(struct atomisp_device *isp,
		struct v4l2_mbus_framefmt *ffmt)
{
	struct atomisp_sub_device *asd = &isp->asd[0];
	int ret = 0;

	dev_dbg(isp->dev, "css_input_resolution_changed to %ux%u\n",
		ffmt->width, ffmt->height);

	if (asd->continuous_mode->val) {
		/* Note for all checks: ffmt includes pad_w+pad_h */
		if (IS_ISP24XX(isp)) {
			if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO ||
			    (ffmt->width >= 2048 || ffmt->height >= 1536)) {
				/*
				 * For preview pipe, enable only if resolution
				 * is >= 3M for ISP2400.
				 */
				atomisp_css_enable_raw_binning(asd, true);
				atomisp_css_input_set_two_pixels_per_clock(asd,
									false);
			}
		} else {
			/* enable raw binning for >= 5M */
			if (ffmt->width >= 2560 || ffmt->height >= 1920)
				atomisp_css_enable_raw_binning(asd, true);
			/* enable 2ppc for CTP if >= 9M */
			if (ffmt->width >= 3648 || ffmt->height >= 2736)
				atomisp_css_input_set_two_pixels_per_clock(
					asd, true);
		}
	}

#ifndef CSS20
	ret = atomisp_css_capture_configure_pp_input(asd, ffmt->width,
				ffmt->height);
	if (ret)
		dev_err(isp->dev, "configure_pp_input %ux%u\n",
			ffmt->width, ffmt->height);
#endif
	return ret;

	/*
	 * TODO: atomisp_css_preview_configure_pp_input() not
	 *       reset due to CSS bug tracked as PSI BZ 115124
	 */
}

static int atomisp_set_fmt_to_isp(struct video_device *vdev,
			struct atomisp_css_frame_info *output_info,
			struct atomisp_css_frame_info *raw_output_info,
			struct v4l2_pix_format *pix,
			unsigned int source_pad)
{
	struct camera_mipi_info *mipi_info;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	const struct atomisp_format_bridge *format;
	struct v4l2_rect *isp_sink_crop;
	enum atomisp_css_pipe_id pipe_id;
	struct v4l2_subdev_fh fh;
	int (*configure_output)(struct atomisp_sub_device *asd,
				unsigned int width, unsigned int height,
				unsigned int min_width,
				enum atomisp_css_frame_format sh_fmt) =
							configure_output_nop;
	int (*get_frame_info)(struct atomisp_sub_device *asd,
				struct atomisp_css_frame_info *finfo) =
							get_frame_info_nop;
	int (*configure_pp_input)(struct atomisp_sub_device *asd,
				  unsigned int width, unsigned int height) =
							configure_pp_input_nop;
	uint16_t stream_index = atomisp_source_pad_to_stream_id(asd, source_pad);
	int ret;

	v4l2_fh_init(&fh.vfh, vdev);

	isp_sink_crop = atomisp_subdev_get_rect(
		&asd->subdev, NULL, V4L2_SUBDEV_FORMAT_ACTIVE,
		ATOMISP_SUBDEV_PAD_SINK, V4L2_SEL_TGT_CROP);

	format = atomisp_get_format_bridge(pix->pixelformat);
	if (format == NULL) {
		dev_err(isp->dev, "format %d is not supported\n", pix->pixelformat);
		return -EINVAL;
	}

	if (isp->inputs[asd->input_curr].type != TEST_PATTERN &&
		isp->inputs[asd->input_curr].type != FILE_INPUT) {
		mipi_info = atomisp_to_sensor_mipi_info(
			isp->inputs[asd->input_curr].camera);
		if (!mipi_info) {
			dev_err(isp->dev, "mipi_info is NULL\n");
			return -EINVAL;
		}
		atomisp_set_sensor_mipi_to_isp(asd, stream_index, mipi_info);

		if (format->sh_fmt == CSS_FRAME_FORMAT_RAW &&
		     raw_output_format_match_input(
						   mipi_info->input_format, pix->pixelformat)) {
			dev_err(isp->dev,
				"Input format 0x%x is not match with the output format 0x%x\n",
				mipi_info->input_format,
				pix->pixelformat);
			return -EINVAL;
		}
	}

	/*
	 * Configure viewfinder also when vfpp is disabled: the
	 * CSS still requires viewfinder configuration.
	 */
	if (asd->fmt_auto->val ||
	    asd->vfpp->val != ATOMISP_VFPP_ENABLE) {
		struct v4l2_rect vf_size = {0};
		struct v4l2_mbus_framefmt vf_ffmt = {0};

		if (pix->width < 640 || pix->height < 480) {
			vf_size.width = pix->width;
			vf_size.height = pix->height;
		} else {
			vf_size.width = 640;
			vf_size.height = 480;
		}

		/* FIXME: proper format name for this one. See
		   atomisp_output_fmts[] in atomisp_v4l2.c */
		vf_ffmt.code = 0x8001;

		atomisp_subdev_set_selection(&asd->subdev, &fh,
					     V4L2_SUBDEV_FORMAT_ACTIVE,
					     ATOMISP_SUBDEV_PAD_SOURCE_VF,
					     V4L2_SEL_TGT_COMPOSE, 0, &vf_size);
		atomisp_subdev_set_ffmt(&asd->subdev, &fh,
					V4L2_SUBDEV_FORMAT_ACTIVE,
					ATOMISP_SUBDEV_PAD_SOURCE_VF, &vf_ffmt);
#ifdef CSS20
		asd->video_out_vf.sh_fmt = CSS_FRAME_FORMAT_NV12;
#else
		asd->video_out_vf.sh_fmt = CSS_FRAME_FORMAT_YUV420;
#endif
		if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER) {
			atomisp_css_video_configure_viewfinder(asd,
				vf_size.width, vf_size.height, 0,
				asd->video_out_vf.sh_fmt);
		} else if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO) {
			if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW ||
			    source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VIDEO)
				atomisp_css_video_configure_viewfinder(asd,
					vf_size.width, vf_size.height, 0,
					asd->video_out_vf.sh_fmt);
			else
				atomisp_css_capture_configure_viewfinder(asd,
					vf_size.width, vf_size.height, 0,
					asd->video_out_vf.sh_fmt);
		} else if (source_pad != ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW ||
			 asd->vfpp->val == ATOMISP_VFPP_DISABLE_LOWLAT) {
			atomisp_css_capture_configure_viewfinder(asd,
				vf_size.width, vf_size.height, 0,
				asd->video_out_vf.sh_fmt);
		}
	}

	if (asd->continuous_mode->val) {
		ret = __enable_continuous_mode(asd, true);
		if (ret) {
			dev_err(isp->dev, "enable continuous mode failed\n");
			return -EINVAL;
		}
	}

	atomisp_css_input_set_mode(asd, CSS_INPUT_MODE_SENSOR);
	atomisp_css_disable_vf_pp(asd,
			asd->vfpp->val != ATOMISP_VFPP_ENABLE);

#if defined(CSS21) && defined(ISP2401_NEW_INPUT_SYSTEM)
	/* ISP2401 new input system need to use copy pipe */
	if (format->sh_fmt == CSS_FRAME_FORMAT_RAW ||
			isp->inputs[asd->input_curr].type == SOC_CAMERA) {
		pipe_id = CSS_PIPE_ID_COPY;
	} else
#endif
	/* video same in continuouscapture and online modes */
	if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER) {
		configure_output = atomisp_css_video_configure_output;
		get_frame_info = atomisp_css_video_get_output_frame_info;
		pipe_id = CSS_PIPE_ID_VIDEO;
	} else if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO) {
		if (!asd->continuous_mode->val) {
			configure_output = atomisp_css_video_configure_output;
			get_frame_info =
				atomisp_css_video_get_output_frame_info;
			pipe_id = CSS_PIPE_ID_VIDEO;
		} else {
			if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW ||
			    source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VIDEO) {
				configure_output =
					atomisp_css_video_configure_output;
				get_frame_info =
					atomisp_css_video_get_output_frame_info;
				configure_pp_input =
					atomisp_css_video_configure_pp_input;
				pipe_id = CSS_PIPE_ID_VIDEO;
			} else {
				configure_output =
					atomisp_css_capture_configure_output;
				get_frame_info =
					atomisp_css_capture_get_output_frame_info;
				configure_pp_input =
					atomisp_css_capture_configure_pp_input;
				pipe_id = CSS_PIPE_ID_CAPTURE;

				atomisp_update_capture_mode(asd);
				atomisp_css_capture_enable_online(asd, false);
			}
		}
	} else if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW) {
		configure_output = atomisp_css_preview_configure_output;
		get_frame_info = atomisp_css_preview_get_output_frame_info;
		configure_pp_input = atomisp_css_preview_configure_pp_input;
		pipe_id = CSS_PIPE_ID_PREVIEW;
	} else {
		/* CSS doesn't support low light mode on SOC cameras, so disable
		 * it. FIXME: if this is done elsewhere, it gives corrupted
		 * colors into thumbnail image.
		 */
		if (isp->inputs[asd->input_curr].type == SOC_CAMERA)
			asd->params.low_light = false;

		if (format->sh_fmt == CSS_FRAME_FORMAT_RAW) {
			atomisp_css_capture_set_mode(asd, CSS_CAPTURE_MODE_RAW);
			atomisp_css_enable_dz(asd, false);
		} else {
			atomisp_update_capture_mode(asd);
		}

		if (!asd->continuous_mode->val)
			/* in case of ANR, force capture pipe to offline mode */
			atomisp_css_capture_enable_online(asd,
					asd->params.low_light ?
					false : asd->params.online_process);

		configure_output = atomisp_css_capture_configure_output;
		get_frame_info = atomisp_css_capture_get_output_frame_info;
		configure_pp_input = atomisp_css_capture_configure_pp_input;
		pipe_id = CSS_PIPE_ID_CAPTURE;

		if (!asd->params.online_process &&
		    !asd->continuous_mode->val) {
			ret = atomisp_css_capture_get_output_raw_frame_info(asd,
							raw_output_info);
			if (ret)
				return ret;
		}
		if (!asd->continuous_mode->val && asd->run_mode->val
		    != ATOMISP_RUN_MODE_STILL_CAPTURE) {
			dev_err(isp->dev,
				    "Need to set the running mode first\n");
			asd->run_mode->val = ATOMISP_RUN_MODE_STILL_CAPTURE;
		}
	}
#if defined(CSS21) && defined(ISP2401_NEW_INPUT_SYSTEM)
	/* ISP2401 new input system need to use copy pipe */
	if (format->sh_fmt == CSS_FRAME_FORMAT_RAW ||
			isp->inputs[asd->input_curr].type == SOC_CAMERA)
		ret = atomisp_css_copy_configure_output(asd, stream_index,
				pix->width, pix->height, format->sh_fmt);
	else
		ret = configure_output(asd, pix->width, pix->height,
				       format->planar ? pix->bytesperline :
				       pix->bytesperline * 8 / format->depth,
				       format->sh_fmt);
#else
	ret = configure_output(asd, pix->width, pix->height,
			       format->planar ? pix->bytesperline :
			       pix->bytesperline * 8 / format->depth,
			       format->sh_fmt);
#endif
	if (ret) {
		dev_err(isp->dev, "configure_output %ux%u, format %8.8x\n",
			pix->width, pix->height, format->sh_fmt);
		return -EINVAL;
	}

	if (asd->continuous_mode->val &&
	    (configure_pp_input == atomisp_css_preview_configure_pp_input ||
	     configure_pp_input == atomisp_css_video_configure_pp_input)) {
#ifdef CSS20
		/* for isp 2.2, configure pp input is available for continuous
		 * mode */
		ret = configure_pp_input(asd, isp_sink_crop->width,
					 isp_sink_crop->height);
		if (ret) {
			dev_err(isp->dev, "configure_pp_input %ux%u\n",
				isp_sink_crop->width,
				isp_sink_crop->height);
			return -EINVAL;
		}
#else
		/* See PSI BZ 115124. preview_configure_pp_input()
		 * API does not work correctly in continuous mode and
		 * and must be disabled by setting it to (0, 0).
		 */
		configure_pp_input(asd, 0, 0);
#endif
	} else {
		ret = configure_pp_input(asd, isp_sink_crop->width,
					 isp_sink_crop->height);
		if (ret) {
			dev_err(isp->dev, "configure_pp_input %ux%u\n",
				isp_sink_crop->width, isp_sink_crop->height);
			return -EINVAL;
		}
	}
#if defined(CSS21) && defined(ISP2401_NEW_INPUT_SYSTEM)
	/* ISP2401 new input system need to use copy pipe */
	if (format->sh_fmt == CSS_FRAME_FORMAT_RAW ||
			isp->inputs[asd->input_curr].type == SOC_CAMERA)
		ret = atomisp_css_copy_get_output_frame_info(asd, stream_index,
				output_info);
	else
		ret = get_frame_info(asd, output_info);
#else
	ret = get_frame_info(asd, output_info);
#endif
	if (ret) {
		dev_err(isp->dev, "get_frame_info %ux%u (padded to %u)\n",
			pix->width, pix->height, pix->bytesperline);
		return -EINVAL;
	}

	atomisp_update_grid_info(asd, pipe_id, source_pad);

	/* Free the raw_dump buffer first */
	atomisp_css_frame_free(asd->raw_output_frame);
	asd->raw_output_frame = NULL;

	if (!asd->continuous_mode->val &&
	    !asd->params.online_process && !isp->sw_contex.file_input &&
		atomisp_css_frame_allocate_from_info(&asd->raw_output_frame,
							raw_output_info))
		return -ENOMEM;

	return 0;
}

static void atomisp_get_dis_envelop(struct atomisp_sub_device *asd,
			    unsigned int width, unsigned int height,
			    unsigned int *dvs_env_w, unsigned int *dvs_env_h)
{
	struct atomisp_device *isp = asd->isp;

	/* if subdev type is SOC camera,we do not need to set DVS */
	if (isp->inputs[asd->input_curr].type == SOC_CAMERA)
		asd->params.video_dis_en = 0;

	if (asd->params.video_dis_en &&
	    asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO) {
		/* envelope is 20% of the output resolution */
		/*
		 * dvs envelope cannot be round up.
		 * it would cause ISP timeout and color switch issue
		 */
		*dvs_env_w = rounddown(width / 5, ATOM_ISP_STEP_WIDTH);
		*dvs_env_h = rounddown(height / 5, ATOM_ISP_STEP_HEIGHT);
	}

	asd->params.dis_proj_data_valid = false;
	asd->params.css_update_params_needed = true;
}

static int atomisp_set_fmt_to_snr(struct video_device *vdev,
		struct v4l2_format *f, unsigned int pixelformat,
		unsigned int padding_w, unsigned int padding_h,
		unsigned int dvs_env_w, unsigned int dvs_env_h)
{
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	const struct atomisp_format_bridge *format;
	struct v4l2_mbus_framefmt ffmt, req_ffmt;
	struct atomisp_device *isp = asd->isp;
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info *)ffmt.reserved;
	uint16_t stream_index = ATOMISP_INPUT_STREAM_GENERAL;
	int source_pad = atomisp_subdev_source_pad(vdev);
	struct v4l2_subdev_fh fh;
	int ret;

	v4l2_fh_init(&fh.vfh, vdev);

	stream_index = atomisp_source_pad_to_stream_id(asd, source_pad);

	format = atomisp_get_format_bridge(pixelformat);
	if (format == NULL)
		return -EINVAL;

	v4l2_fill_mbus_format(&ffmt, &f->fmt.pix, format->mbus_code);
	ffmt.height += padding_h + dvs_env_h;
	ffmt.width += padding_w + dvs_env_w;

	dev_dbg(isp->dev, "s_mbus_fmt: ask %ux%u (padding %ux%u, dvs %ux%u)\n",
		ffmt.width, ffmt.height, padding_w, padding_h,
		dvs_env_w, dvs_env_h);

	stream_info->enable = 1;
	stream_info->stream = stream_index;
	stream_info->ch_id = 0;

	req_ffmt = ffmt;

	/* Disable dvs if resolution can't be supported by sensor */
	if (asd->params.video_dis_en) {
		ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
			video, try_mbus_fmt, &ffmt);
		if (ret)
			return ret;
		if (ffmt.width < req_ffmt.width ||
				ffmt.height < req_ffmt.height) {
			req_ffmt.height -= dvs_env_h;
			req_ffmt.width -= dvs_env_w;
			ffmt = req_ffmt;
			dev_warn(isp->dev,
			  "can not enable video dis due to sensor limitation.");
			asd->params.video_dis_en = 0;
		}
	}
	dev_dbg(isp->dev, "sensor width: %d, height: %d\n",
		ffmt.width, ffmt.height);

	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera, video,
			       s_mbus_fmt, &ffmt);
	if (ret)
		return ret;

#if defined(CSS21) && defined(ISP2401_NEW_INPUT_SYSTEM)
	/* assign virtual channel id return from sensor driver query */
	asd->stream_env[stream_index].ch_id = stream_info->ch_id;
#endif
	dev_dbg(isp->dev, "sensor width: %d, height: %d\n",
		ffmt.width, ffmt.height);

	if (ffmt.width < ATOM_ISP_STEP_WIDTH ||
	    ffmt.height < ATOM_ISP_STEP_HEIGHT)
			return -EINVAL;

	if (asd->params.video_dis_en && (ffmt.width < req_ffmt.width ||
	    ffmt.height < req_ffmt.height)) {
		dev_warn(isp->dev,
			 "can not enable video dis due to sensor limitation.");
		asd->params.video_dis_en = 0;
	}

	atomisp_subdev_set_ffmt(&asd->subdev, &fh,
				V4L2_SUBDEV_FORMAT_ACTIVE,
				ATOMISP_SUBDEV_PAD_SINK, &ffmt);

	return css_input_resolution_changed(isp, &ffmt);
}

int atomisp_set_fmt(struct video_device *vdev, struct v4l2_format *f)
{
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	const struct atomisp_format_bridge *format_bridge;
	struct atomisp_css_frame_info output_info, raw_output_info;
	struct v4l2_format snr_fmt = *f;
	unsigned int dvs_env_w = 0, dvs_env_h = 0;
	unsigned int padding_w = pad_w, padding_h = pad_h;
	bool res_overflow = false, crop_needs_override = false;
	struct v4l2_mbus_framefmt isp_sink_fmt;
	struct v4l2_mbus_framefmt isp_source_fmt = {0};
	struct v4l2_rect isp_sink_crop;
	uint16_t source_pad = atomisp_subdev_source_pad(vdev);
	struct v4l2_subdev_fh fh;
	int ret;

	dev_dbg(isp->dev,
		"setting resolution %ux%u on pad %u, bytesperline %u\n",
		f->fmt.pix.width, f->fmt.pix.height, source_pad,
		f->fmt.pix.bytesperline);

	v4l2_fh_init(&fh.vfh, vdev);

	format_bridge = atomisp_get_format_bridge(f->fmt.pix.pixelformat);
	if (format_bridge == NULL)
		return -EINVAL;

	pipe->sh_fmt = format_bridge->sh_fmt;
	pipe->pix.pixelformat = f->fmt.pix.pixelformat;

#if defined(CSS21) && defined(ISP2401_NEW_INPUT_SYSTEM)
	if (isp->inputs[asd->input_curr].camera_caps->sensor[asd->sensor_curr].
			stream_num < 2 &&
			(source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VF ||
			 (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW
		&& asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO))) {
#else
	if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VF ||
	    (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW
		&& asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)) {
#endif
		if (asd->fmt_auto->val) {
			struct v4l2_rect *capture_comp;
			struct v4l2_rect r = {0};

			r.width = f->fmt.pix.width;
			r.height = f->fmt.pix.height;

			if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW)
			    capture_comp = atomisp_subdev_get_rect(
					&asd->subdev, NULL,
					V4L2_SUBDEV_FORMAT_ACTIVE,
					ATOMISP_SUBDEV_PAD_SOURCE_VIDEO,
					V4L2_SEL_TGT_COMPOSE);
			else
			    capture_comp = atomisp_subdev_get_rect(
					&asd->subdev, NULL,
					V4L2_SUBDEV_FORMAT_ACTIVE,
					ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE,
					V4L2_SEL_TGT_COMPOSE);

			if (capture_comp->width < r.width
			    || capture_comp->height < r.height) {
				r.width = capture_comp->width;
				r.height = capture_comp->height;
			}

			atomisp_subdev_set_selection(
				&asd->subdev, &fh,
				V4L2_SUBDEV_FORMAT_ACTIVE, source_pad,
				V4L2_SEL_TGT_COMPOSE, 0, &r);

			f->fmt.pix.width = r.width;
			f->fmt.pix.height = r.height;
		}

		if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW) {
			atomisp_css_video_configure_viewfinder(asd,
				f->fmt.pix.width, f->fmt.pix.height,
				format_bridge->planar ? f->fmt.pix.bytesperline
				: f->fmt.pix.bytesperline * 8
				/ format_bridge->depth,	format_bridge->sh_fmt);
			atomisp_css_video_get_viewfinder_frame_info(asd,
								&output_info);
		} else {
			atomisp_css_capture_configure_viewfinder(asd,
				f->fmt.pix.width, f->fmt.pix.height,
				format_bridge->planar ? f->fmt.pix.bytesperline
				: f->fmt.pix.bytesperline * 8
				/ format_bridge->depth,	format_bridge->sh_fmt);
			atomisp_css_capture_get_viewfinder_frame_info(asd,
								&output_info);
		}

		goto done;
	}
	/*
	 * Check whether main resolution configured smaller
	 * than snapshot resolution. If so, force main resolution
	 * to be the same as snapshot resolution
	 */
	if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE) {
		struct v4l2_rect *r;

		r = atomisp_subdev_get_rect(
			&asd->subdev, NULL,
			V4L2_SUBDEV_FORMAT_ACTIVE,
			ATOMISP_SUBDEV_PAD_SOURCE_VF, V4L2_SEL_TGT_COMPOSE);

		if (r->width && r->height
		    && (r->width > f->fmt.pix.width
			|| r->height > f->fmt.pix.height))
			dev_warn(isp->dev,
				 "Main Resolution config smaller then Vf Resolution. Force to be equal with Vf Resolution.");
	}

	/* Pipeline configuration done through subdevs. Bail out now. */
	if (!asd->fmt_auto->val)
		goto set_fmt_to_isp;

	/* get sensor resolution and format */
	ret = atomisp_try_fmt(vdev, &snr_fmt, &res_overflow);
	if (ret)
		return ret;
	f->fmt.pix.width = snr_fmt.fmt.pix.width;
	f->fmt.pix.height = snr_fmt.fmt.pix.height;

	format_bridge = atomisp_get_format_bridge(snr_fmt.fmt.pix.pixelformat);
	if (format_bridge == NULL)
		return -EINVAL;

	atomisp_subdev_get_ffmt(&asd->subdev, NULL,
				V4L2_SUBDEV_FORMAT_ACTIVE,
				ATOMISP_SUBDEV_PAD_SINK)->code =
		format_bridge->mbus_code;

	isp_sink_fmt = *atomisp_subdev_get_ffmt(&asd->subdev, NULL,
					    V4L2_SUBDEV_FORMAT_ACTIVE,
					    ATOMISP_SUBDEV_PAD_SINK);
	format_bridge = atomisp_get_format_bridge(f->fmt.pix.pixelformat);
	if (format_bridge == NULL)
		return -EINVAL;

	isp_source_fmt.code = format_bridge->mbus_code;
	atomisp_subdev_set_ffmt(&asd->subdev, &fh,
				V4L2_SUBDEV_FORMAT_ACTIVE,
				source_pad, &isp_source_fmt);

	if (!atomisp_subdev_format_conversion(asd, source_pad))
		padding_w = 0, padding_h = 0;

	if (IS_BYT) {
		padding_w = 12;
		padding_h = 12;
	}

	/* construct resolution supported by isp */
	if (res_overflow && !asd->continuous_mode->val) {
		f->fmt.pix.width = rounddown(
			clamp_t(u32, f->fmt.pix.width - padding_w,
				ATOM_ISP_MIN_WIDTH,
				ATOM_ISP_MAX_WIDTH), ATOM_ISP_STEP_WIDTH);
		f->fmt.pix.height = rounddown(
			clamp_t(u32, f->fmt.pix.height - padding_h,
				ATOM_ISP_MIN_HEIGHT,
				ATOM_ISP_MAX_HEIGHT), ATOM_ISP_STEP_HEIGHT);
	}

	atomisp_get_dis_envelop(asd, f->fmt.pix.width, f->fmt.pix.height,
				&dvs_env_w, &dvs_env_h);

	if (asd->continuous_mode->val)
		asd->capture_pad = ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE;
	else
		asd->capture_pad = source_pad;

	/*
	 * set format info to sensor
	 * In continuous mode, resolution is set only if it is higher than
	 * existing value. This because preview pipe will be configured after
	 * capture pipe and usually has lower resolution than capture pipe.
	 */
	if (!asd->continuous_mode->val ||
	    isp_sink_fmt.width < (f->fmt.pix.width + padding_w + dvs_env_w) ||
	     isp_sink_fmt.height < (f->fmt.pix.height + padding_h +
				    dvs_env_h)) {
		ret = atomisp_set_fmt_to_snr(vdev, f, f->fmt.pix.pixelformat,
					     padding_w, padding_h,
					     dvs_env_w, dvs_env_h);
		if (ret)
			return -EINVAL;

		crop_needs_override = true;
	}

	isp_sink_crop = *atomisp_subdev_get_rect(&asd->subdev, NULL,
						 V4L2_SUBDEV_FORMAT_ACTIVE,
						 ATOMISP_SUBDEV_PAD_SINK,
						 V4L2_SEL_TGT_CROP);

	/* Try to enable YUV downscaling if ISP input is 10 % (either
	 * width or height) bigger than the desired result. */
	if (isp_sink_crop.width * 9 / 10 < f->fmt.pix.width ||
	    isp_sink_crop.height * 9 / 10 < f->fmt.pix.height ||
	    (atomisp_subdev_format_conversion(asd, source_pad) &&
	     ((asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO &&
	       !asd->continuous_mode->val) ||
	      asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER))) {
		/* for continuous mode, preview size might be smaller than
		 * still capture size. if preview size still needs crop,
		 * pick the larger one between crop size of preview and
		 * still capture.
		 */
		if (asd->continuous_mode->val
		    && source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW
		    && !crop_needs_override) {
			isp_sink_crop.width =
				max_t(unsigned int, f->fmt.pix.width,
				      isp_sink_crop.width);
			isp_sink_crop.height =
				max_t(unsigned int, f->fmt.pix.height,
				      isp_sink_crop.height);
		} else {
			isp_sink_crop.width = f->fmt.pix.width;
			isp_sink_crop.height = f->fmt.pix.height;
		}

		atomisp_subdev_set_selection(&asd->subdev, &fh,
					     V4L2_SUBDEV_FORMAT_ACTIVE,
					     ATOMISP_SUBDEV_PAD_SINK,
					     V4L2_SEL_TGT_CROP,
					     V4L2_SEL_FLAG_KEEP_CONFIG,
					     &isp_sink_crop);
		atomisp_subdev_set_selection(&asd->subdev, &fh,
					     V4L2_SUBDEV_FORMAT_ACTIVE,
					     source_pad, V4L2_SEL_TGT_COMPOSE,
					     0, &isp_sink_crop);
	} else {
		struct v4l2_rect main_compose = {0};

		main_compose.width = isp_sink_crop.width - padding_w;
		main_compose.height =
			DIV_ROUND_UP(main_compose.width * f->fmt.pix.height,
				     f->fmt.pix.width);
		if (main_compose.height > isp_sink_crop.height - padding_h) {
			main_compose.height = isp_sink_crop.height - padding_h;
			main_compose.width =
				DIV_ROUND_UP(main_compose.height *
					     f->fmt.pix.width,
					     f->fmt.pix.height);
		}

		if (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VIDEO)
			atomisp_subdev_set_selection(&asd->subdev, &fh,
					     V4L2_SUBDEV_FORMAT_ACTIVE,
					     ATOMISP_SUBDEV_PAD_SOURCE_VIDEO,
					     V4L2_SEL_TGT_COMPOSE, 0,
					     &main_compose);
		else
			atomisp_subdev_set_selection(&asd->subdev, &fh,
					     V4L2_SUBDEV_FORMAT_ACTIVE,
					     ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE,
					     V4L2_SEL_TGT_COMPOSE, 0,
					     &main_compose);
	}

set_fmt_to_isp:
	ret = atomisp_set_fmt_to_isp(vdev, &output_info, &raw_output_info,
				     &f->fmt.pix, source_pad);
	if (ret)
		return -EINVAL;
done:
	pipe->pix.width = f->fmt.pix.width;
	pipe->pix.height = f->fmt.pix.height;
	pipe->pix.pixelformat = f->fmt.pix.pixelformat;
	if (format_bridge->planar) {
		pipe->pix.bytesperline = output_info.padded_width;
		pipe->pix.sizeimage = PAGE_ALIGN(f->fmt.pix.height *
			DIV_ROUND_UP(format_bridge->depth *
				     output_info.padded_width, 8));
	} else {
		pipe->pix.bytesperline =
			DIV_ROUND_UP(format_bridge->depth *
				     output_info.padded_width, 8);
		pipe->pix.sizeimage =
			PAGE_ALIGN(f->fmt.pix.height * pipe->pix.bytesperline);

	}
	if (f->fmt.pix.field == V4L2_FIELD_ANY)
		f->fmt.pix.field = V4L2_FIELD_NONE;
	pipe->pix.field = f->fmt.pix.field;

	f->fmt.pix = pipe->pix;
	f->fmt.pix.priv = PAGE_ALIGN(pipe->pix.width *
				     pipe->pix.height * 2);

	pipe->capq.field = f->fmt.pix.field;

	/*
	 * If in video 480P case, no GFX throttle
	 */
	if (asd->run_mode->val == ATOMISP_SUBDEV_PAD_SOURCE_VIDEO &&
	    f->fmt.pix.width == 720 && f->fmt.pix.height == 480)
		isp->need_gfx_throttle = false;
	else
		isp->need_gfx_throttle = true;

	return 0;
}

int atomisp_set_fmt_file(struct video_device *vdev, struct v4l2_format *f)
{
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct v4l2_mbus_framefmt ffmt = {0};
	const struct atomisp_format_bridge *format_bridge;
	struct v4l2_subdev_fh fh;
	int ret;

	v4l2_fh_init(&fh.vfh, vdev);

	dev_dbg(isp->dev, "setting fmt %ux%u 0x%x for file inject\n",
		f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.pixelformat);
	ret = atomisp_try_fmt_file(isp, f);
	if (ret) {
		dev_err(isp->dev, "atomisp_try_fmt_file err: %d\n", ret);
		return ret;
	}

	format_bridge = atomisp_get_format_bridge(f->fmt.pix.pixelformat);
	if (format_bridge == NULL) {
		dev_dbg(isp->dev, "atomisp_get_format_bridge err! fmt:0x%x\n",
				f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	pipe->pix = f->fmt.pix;
	atomisp_css_input_set_mode(asd, CSS_INPUT_MODE_FIFO);
	atomisp_css_input_configure_port(asd,
		__get_mipi_port(isp, ATOMISP_CAMERA_PORT_PRIMARY), 2, 0xffff4);

	ffmt.width = f->fmt.pix.width;
	ffmt.height = f->fmt.pix.height;
	ffmt.code = format_bridge->mbus_code;

	atomisp_subdev_set_ffmt(&asd->subdev, &fh, V4L2_SUBDEV_FORMAT_ACTIVE,
				ATOMISP_SUBDEV_PAD_SINK, &ffmt);

	return 0;
}

void atomisp_free_all_shading_tables(struct atomisp_device *isp)
{
	int i;

	for (i = 0; i < isp->input_cnt; i++) {
		if (isp->inputs[i].shading_table == NULL)
			continue;
		atomisp_css_shading_table_free(isp->inputs[i].shading_table);
		isp->inputs[i].shading_table = NULL;
	}
}

int atomisp_set_shading_table(struct atomisp_sub_device *asd,
		struct atomisp_shading_table *user_shading_table)
{
	struct atomisp_css_shading_table *shading_table;
	struct atomisp_css_shading_table *free_table;
	unsigned int len_table;
	struct atomisp_device *isp = asd->isp;
	int i;
	int ret = 0;

	if (!user_shading_table)
		return -EINVAL;

#ifndef CSS20
	if (user_shading_table->flags & ATOMISP_SC_FLAG_QUERY) {
		user_shading_table->enable = asd->params.sc_en;
		return 0;
	}
#endif

	if (!user_shading_table->enable) {
		atomisp_css_set_shading_table(asd, NULL);
		asd->params.sc_en = 0;
		return 0;
	}

	/* If enabling, all tables must be set */
	for (i = 0; i < ATOMISP_NUM_SC_COLORS; i++) {
		if (!user_shading_table->data[i])
			return -EINVAL;
	}

	/* Shading table size per color */
	if (user_shading_table->width > SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR ||
	    user_shading_table->height > SH_CSS_MAX_SCTBL_HEIGHT_PER_COLOR)
		return -EINVAL;

	shading_table = atomisp_css_shading_table_alloc(
			user_shading_table->width, user_shading_table->height);
	if (!shading_table)
		return -ENOMEM;

	len_table = user_shading_table->width * user_shading_table->height *
		    ATOMISP_SC_TYPE_SIZE;
	for (i = 0; i < ATOMISP_NUM_SC_COLORS; i++) {
		ret = copy_from_user(shading_table->data[i],
				     user_shading_table->data[i], len_table);
		if (ret) {
			free_table = shading_table;
			ret = -EFAULT;
			goto out;
		}
	}
	shading_table->sensor_width = user_shading_table->sensor_width;
	shading_table->sensor_height = user_shading_table->sensor_height;
	shading_table->fraction_bits = user_shading_table->fraction_bits;

	free_table = isp->inputs[asd->input_curr].shading_table;
	isp->inputs[asd->input_curr].shading_table = shading_table;
	atomisp_css_set_shading_table(asd, shading_table);
	asd->params.sc_en = 1;

out:
	if (free_table != NULL)
		atomisp_css_shading_table_free(free_table);

	return ret;
}

/*Turn off ISP dphy */
int atomisp_ospm_dphy_down(struct atomisp_device *isp)
{
	unsigned long flags;
	u32 pwr_cnt = 0;
	int timeout = 100;
	bool idle;
	u32 reg;

	dev_dbg(isp->dev, "%s\n", __func__);

	/* if ISP timeout, we can force powerdown */
	if (isp->isp_timeout)
		goto done;

	if (!atomisp_dev_users(isp))
		goto done;

	idle = sh_css_hrt_system_is_idle();
	dev_dbg(isp->dev, "%s system_is_idle:%d\n", __func__, idle);
	while (!idle && timeout--) {
		udelay(20);
		idle = sh_css_hrt_system_is_idle();
	}

	if (timeout < 0) {
		dev_err(isp->dev, "Timeout to stop ISP HW\n");
		/* force power down here */
		/* return -EINVAL; */
	}

	spin_lock_irqsave(&isp->lock, flags);
	isp->sw_contex.power_state = ATOM_ISP_POWER_DOWN;
	spin_unlock_irqrestore(&isp->lock, flags);
done:
	if (IS_ISP24XX(isp)) {
		/*
		 * MRFLD IUNIT DPHY is located in an always-power-on island
		 * MRFLD HW design need all CSI ports are disabled before
		 * powering down the IUNIT.
		 */
		pci_read_config_dword(isp->pdev, MRFLD_PCI_CSI_CONTROL, &reg);
		reg |= MRFLD_ALL_CSI_PORTS_OFF_MASK;
		pci_write_config_dword(isp->pdev, MRFLD_PCI_CSI_CONTROL, reg);
	} else {
		/* power down DPHY */
		pwr_cnt = intel_mid_msgbus_read32(MFLD_IUNITPHY_PORT,
							MFLD_CSI_CONTROL);
		pwr_cnt |= 0x300;
		intel_mid_msgbus_write32(MFLD_IUNITPHY_PORT,
						MFLD_CSI_CONTROL, pwr_cnt);
	}

	return 0;
}

/*Turn on ISP dphy */
int atomisp_ospm_dphy_up(struct atomisp_device *isp)
{
	u32 pwr_cnt = 0;
	unsigned long flags;
	dev_dbg(isp->dev, "%s\n", __func__);

	/* MRFLD IUNIT DPHY is located in an always-power-on island */
	if (!IS_ISP24XX(isp)) {
		/* power on DPHY */
		pwr_cnt = intel_mid_msgbus_read32(MFLD_IUNITPHY_PORT,
							MFLD_CSI_CONTROL);
		pwr_cnt &= ~0x300;
		intel_mid_msgbus_write32(MFLD_IUNITPHY_PORT,
						MFLD_CSI_CONTROL, pwr_cnt);
	}
	spin_lock_irqsave(&isp->lock, flags);
	isp->sw_contex.power_state = ATOM_ISP_POWER_UP;
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}


int atomisp_exif_makernote(struct atomisp_sub_device *asd,
			   struct atomisp_makernote_info *config)
{
	struct v4l2_control ctrl;
	struct atomisp_device *isp = asd->isp;

	ctrl.id = V4L2_CID_FOCAL_ABSOLUTE;
	if (v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				 core, g_ctrl, &ctrl))
		dev_warn(isp->dev, "failed to g_ctrl for focal length\n");
	else
		config->focal_length = ctrl.value;

	ctrl.id = V4L2_CID_FNUMBER_ABSOLUTE;
	if (v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				core, g_ctrl, &ctrl))
		dev_warn(isp->dev, "failed to g_ctrl for f-number\n");
	else
		config->f_number_curr = ctrl.value;

	ctrl.id = V4L2_CID_FNUMBER_RANGE;
	if (v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				core, g_ctrl, &ctrl))
		dev_warn(isp->dev, "failed to g_ctrl for f number range\n");
	else
		config->f_number_range = ctrl.value;

	return 0;
}

int atomisp_offline_capture_configure(struct atomisp_sub_device *asd,
			      struct atomisp_cont_capture_conf *cvf_config)
{
	asd->params.offline_parm = *cvf_config;

	if (asd->params.offline_parm.num_captures) {
		if (asd->streaming == ATOMISP_DEVICE_STREAMING_DISABLED)
			/* TODO: this can be removed once user-space
			 *       has been updated to use control API */
			asd->continuous_raw_buffer_size->val =
				min_t(int, ATOMISP_CONT_RAW_FRAMES,
				      asd->params.offline_parm.
				      num_captures + 3);

		asd->continuous_mode->val = true;
	} else {
		asd->continuous_mode->val = false;
		__enable_continuous_mode(asd, false);
	}

	return 0;
}

int atomisp_flash_enable(struct atomisp_sub_device *asd, int num_frames)
{
	struct atomisp_device *isp = asd->isp;

	if (num_frames < 0) {
		dev_dbg(isp->dev, "%s ERROR: num_frames: %d\n", __func__,
				num_frames);
		return -EINVAL;
	}
	/* a requested flash is still in progress. */
	if (num_frames && asd->params.flash_state != ATOMISP_FLASH_IDLE) {
		dev_dbg(isp->dev, "%s flash busy: %d frames left: %d\n",
				__func__, asd->params.flash_state,
				asd->params.num_flash_frames);
		return -EBUSY;
	}

	asd->params.num_flash_frames = num_frames;
	asd->params.flash_state = ATOMISP_FLASH_REQUESTED;
	return 0;
}

int atomisp_source_pad_to_stream_id(struct atomisp_sub_device *asd,
					   uint16_t source_pad)
{
	int stream_id;
	struct atomisp_device *isp = asd->isp;

	if (isp->inputs[asd->input_curr].camera_caps->
			sensor[asd->sensor_curr].stream_num == 1)
		return ATOMISP_INPUT_STREAM_GENERAL;

	if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO &&
			source_pad == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE)
		stream_id = ATOMISP_INPUT_STREAM_VIDEO;
	else {
		switch (source_pad) {
			case ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE:
				stream_id = ATOMISP_INPUT_STREAM_CAPTURE;
				break;
			case ATOMISP_SUBDEV_PAD_SOURCE_VF:
				stream_id = ATOMISP_INPUT_STREAM_POSTVIEW;
				break;
			case ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW:
				stream_id = ATOMISP_INPUT_STREAM_PREVIEW;
				break;
			default:
				stream_id = ATOMISP_INPUT_STREAM_GENERAL;
		}
	}

	return stream_id;
}
