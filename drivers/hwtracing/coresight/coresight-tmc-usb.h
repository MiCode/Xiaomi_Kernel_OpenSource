/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef _CORESIGHT_TMC_USB_H
#define _CORESIGHT_TMC_USB_H

#include <linux/usb_bam.h>
#include <linux/amba/bus.h>
#include <linux/msm-sps.h>
#include <linux/usb/usb_qdss.h>
#include <linux/iommu.h>

#define TMC_USB_BAM_PIPE_INDEX	0
#define TMC_USB_BAM_NR_PIPES	2

enum tmc_etr_usb_mode {
	TMC_ETR_USB_NONE,
	TMC_ETR_USB_BAM_TO_BAM,
	TMC_ETR_USB_SW,
};

struct tmc_usb_bam_data {
	struct sps_bam_props	props;
	unsigned long		handle;
	struct sps_pipe		*pipe;
	struct sps_connect	connect;
	uint32_t		src_pipe_idx;
	unsigned long		dest;
	uint32_t		dest_pipe_idx;
	struct sps_mem_buffer	desc_fifo;
	struct sps_mem_buffer	data_fifo;
	bool			enable;
};

struct tmc_usb_data {
	struct usb_qdss_ch	*usbch;
	struct tmc_usb_bam_data	*bamdata;
	bool			enable_to_bam;
	enum tmc_etr_usb_mode	usb_mode;
	struct tmc_drvdata	*tmcdrvdata;
};

extern int tmc_usb_enable(struct tmc_usb_data *usb_data);
extern void tmc_usb_disable(struct tmc_usb_data *usb_data);

#endif
