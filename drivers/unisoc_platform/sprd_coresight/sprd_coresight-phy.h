/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CORESIGHT_PHY_H
#define _CORESIGHT_PHY_H

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/miscdevice.h>
#include "sprd_coresight-priv.h"
#include "sprd_coresight-tmc.h"

#define CS_ELEMENT_NUM		20
#define CS_FUNNEL_REGS_NUM	2
#define CS_TMC_REGS_NUM		12
#define CS_BUF_SZ		0xc400 /* 49k,0xc038+0x200 */

enum CS_TYPE_E {
	CS_FUNNEL,
	CS_TMC,
};

struct cs_element_t {
	char *name;
	enum CS_TYPE_E type;
	unsigned int phy_start;
	unsigned int size;
	void __iomem *vir_start;
	bool is_subsys;
	unsigned int pmu_check;
	void __iomem *vir_pmu_check;
	unsigned int pmu_mask;
};

struct cs_context_t {
	struct cs_element_t config;
	struct tmc_drvdata context;
};

/**
 * struct tmc_drvdata_group - specifics associated to an TMC group component
 * @base:	memory mapped base address for this component.
 * @dev:	the device entity associated to this component.
 * @csdev:	component vitals needed by the framework.
 * @miscdev:	specifics to handle "/dev/xyz.tmc" entry.
 * @spinlock:	only one at a time pls.
 * @buf:	area of memory where trace data get sent.
 * @paddr:	DMA start location in RAM.
 * @vaddr:	virtual representation of @paddr.
 * @size:	trace buffer size.
 * @len:	size of the available trace.
 * @mode:	how this TMC is being used.
 * @config_type: TMC variant, must be of type @tmc_config_type.
 * @memwidth:	width of the memory interface databus, in bytes.
 * @trigger_cntr: amount of words to store after a trigger.
 * @etr_caps:	Bitmask of capabilities of the TMC ETR, inferred from the
 *		device configuration register (DEVID)
 */
struct tmc_drvdata_group {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	spinlock_t		spinlock;
	bool			reading;
	char			*buf;
	dma_addr_t		paddr;
	void __iomem		*vaddr;
	u32			size;
	u32			len;
	u32			mode;
	enum tmc_config_type	config_type;
	enum tmc_mem_intf_width	memwidth;
	u32			trigger_cntr;
	u32			etr_caps;
	char			etf_name[16];
	u32			cs_element_num;
	struct cs_context_t	cs_context[CS_ELEMENT_NUM];
};

int cs_group_size(void);
struct cs_element_t *cs_group_ptr(void);

#endif
