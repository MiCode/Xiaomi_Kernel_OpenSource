/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/camera.h>
#include <mach/clk.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "msm_ispif.h"
#include "msm.h"

#define DBG_ISPIF 0
/* ISPIF registers */

#define ISPIF_RST_CMD_ADDR                        0X00
#define ISPIF_INTF_CMD_ADDR                       0X04
#define ISPIF_CTRL_ADDR                           0X08
#define ISPIF_INPUT_SEL_ADDR                      0X0C
#define ISPIF_PIX_INTF_CID_MASK_ADDR              0X10
#define ISPIF_RDI_INTF_CID_MASK_ADDR              0X14
#define ISPIF_PIX_1_INTF_CID_MASK_ADDR            0X38
#define ISPIF_RDI_1_INTF_CID_MASK_ADDR            0X3C
#define ISPIF_PIX_STATUS_ADDR                     0X24
#define ISPIF_RDI_STATUS_ADDR                     0X28
#define ISPIF_RDI_1_STATUS_ADDR                   0X64
#define ISPIF_IRQ_MASK_ADDR                     0X0100
#define ISPIF_IRQ_CLEAR_ADDR                    0X0104
#define ISPIF_IRQ_STATUS_ADDR                   0X0108
#define ISPIF_IRQ_MASK_1_ADDR                   0X010C
#define ISPIF_IRQ_CLEAR_1_ADDR                  0X0110
#define ISPIF_IRQ_STATUS_1_ADDR                 0X0114

/*ISPIF RESET BITS*/

#define VFE_CLK_DOMAIN_RST           31
#define RDI_CLK_DOMAIN_RST           30
#define PIX_CLK_DOMAIN_RST           29
#define AHB_CLK_DOMAIN_RST           28
#define RDI_1_CLK_DOMAIN_RST         27
#define RDI_1_VFE_RST_STB            13
#define RDI_1_CSID_RST_STB           12
#define RDI_VFE_RST_STB              7
#define RDI_CSID_RST_STB             6
#define PIX_VFE_RST_STB              4
#define PIX_CSID_RST_STB             3
#define SW_REG_RST_STB               2
#define MISC_LOGIC_RST_STB           1
#define STROBED_RST_EN               0

#define PIX_INTF_0_OVERFLOW_IRQ      12
#define RAW_INTF_0_OVERFLOW_IRQ      25
#define RAW_INTF_1_OVERFLOW_IRQ      25
#define RESET_DONE_IRQ               27

#define MAX_CID 15
DEFINE_MUTEX(msm_ispif_mut);

static struct resource *ispif_mem;
static struct resource *ispif_irq;
static struct resource *ispifio;
void __iomem *ispifbase;
static uint32_t global_intf_cmd_mask = 0xFFFFFFFF;
#if DBG_ISPIF
static inline void msm_ispif_read_irq_status(struct ispif_irq_status *out)
{
	uint32_t *temp;
	memset(out, 0, sizeof(struct ispif_irq_status));
	temp = (uint32_t *)(ispifbase + ISPIF_IRQ_STATUS_ADDR);
	out->ispifIrqStatus0 = msm_io_r(temp);
	pr_err("ispif_irq: Irq_status0 = 0x%x\n",
		out->ispifIrqStatus0);
	msm_io_w(out->ispifIrqStatus0, ispifbase + ISPIF_IRQ_CLEAR_ADDR);
}

static irqreturn_t msm_io_ispif_irq(int irq_num, void *data)
{
	struct ispif_irq_status irq;
	msm_ispif_read_irq_status(&irq);
	return IRQ_HANDLED;
}
#endif
int msm_ispif_init(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_ispif_fns ispif_fns;

	ispif_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ispif");
	if (!ispif_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		return -ENODEV;
	}
	ispif_irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "ispif");
	if (!ispif_irq) {
		pr_err("%s: no irq resource?\n", __func__);
		return -ENODEV;
	}

	ispifio  = request_mem_region(ispif_mem->start,
		resource_size(ispif_mem), pdev->name);
	if (!ispifio)
		return -EBUSY;
	ispifbase = ioremap(ispif_mem->start,
		resource_size(ispif_mem));
	if (!ispifbase) {
		rc = -ENOMEM;
		goto ispif_no_mem;
	}
#if DBG_ISPIF
	rc = request_irq(ispif_irq->start, msm_io_ispif_irq,
		IRQF_TRIGGER_RISING, "ispif", 0);
	if (rc < 0)
		goto ispif_irq_fail;
#endif
	global_intf_cmd_mask = 0xFFFFFFFF;
	ispif_fns.ispif_config = msm_ispif_config;
	ispif_fns.ispif_start_intf_transfer =
		msm_ispif_start_intf_transfer;
	rc = msm_ispif_register(&ispif_fns);
	if (rc < 0)
		goto ispif_irq_fail;

	msm_ispif_reset();
	return 0;

ispif_irq_fail:
	iounmap(ispifbase);
ispif_no_mem:
	release_mem_region(ispif_mem->start, resource_size(ispif_mem));
	return rc;
}

void msm_ispif_release(struct platform_device *pdev)
{
	CDBG("%s, free_irq\n", __func__);
#if DBG_ISPIF
	free_irq(ispif_irq->start, 0);
#endif
	iounmap(ispifbase);
	release_mem_region(ispif_mem->start, resource_size(ispif_mem));
}

void msm_ispif_intf_reset(uint8_t intftype)
{
	uint32_t data = 0x01 , data1 = 0x01;

	msm_io_w(0x1<<STROBED_RST_EN, ispifbase + ISPIF_RST_CMD_ADDR);
	switch (intftype) {
	case PIX0:
		data |= 0x1 << PIX_VFE_RST_STB;
		msm_io_w(data, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		data1 |= 0x1 << PIX_CSID_RST_STB;
		msm_io_w(data1, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		break;

	case RDI0:
		data |= 0x1 << RDI_VFE_RST_STB;
		msm_io_w(data, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		data1 |= 0x1 << RDI_CSID_RST_STB;
		msm_io_w(data1, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		break;

	case RDI1:
		data |= 0x1 << RDI_1_VFE_RST_STB;
		msm_io_w(data, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		data1 |= 0x1 << RDI_1_CSID_RST_STB;
		msm_io_w(data1, ispifbase + ISPIF_RST_CMD_ADDR);
		usleep_range(11000, 12000);
		break;

	default:
		break;
	}
}

void msm_ispif_swreg_misc_reset(void)
{
	uint32_t data = 0x01, data1 = 0x01;

	data |= 0x1 << SW_REG_RST_STB;
	msm_io_w(data, ispifbase + ISPIF_RST_CMD_ADDR);
	usleep_range(11000, 12000);
	data1 |= 0x1 << MISC_LOGIC_RST_STB;
	msm_io_w(data1, ispifbase + ISPIF_RST_CMD_ADDR);
	usleep_range(11000, 12000);
}

void msm_ispif_reset(void)
{
	msm_ispif_swreg_misc_reset();
	msm_ispif_intf_reset(PIX0);
	msm_ispif_intf_reset(RDI0);
}

void msm_ispif_sel_csid_core(uint8_t intftype, uint8_t csid)
{
	uint32_t data;
	data = msm_io_r(ispifbase + ISPIF_INPUT_SEL_ADDR);
	data |= csid<<(intftype*4);
	msm_io_w(data, ispifbase + ISPIF_INPUT_SEL_ADDR);
}

static void
msm_ispif_intf_cmd(uint8_t intftype, uint16_t cid_mask, uint8_t intf_cmd_mask)
{
	uint8_t vc = 0, val = 0;
	while (cid_mask != 0) {
		if ((cid_mask & 0xf) != 0x0) {
			val = (intf_cmd_mask>>(vc*2)) & 0x3;
			global_intf_cmd_mask &= ~((0x3 & ~val)
				<<((vc*2)+(intftype*8)));
			CDBG("intf cmd  0x%x\n", global_intf_cmd_mask);
			msm_io_w(global_intf_cmd_mask,
				ispifbase + ISPIF_INTF_CMD_ADDR);
		}
		vc++;
		cid_mask >>= 4;
	}
}

void msm_ispif_enable_intf_cids(uint8_t intftype, uint16_t cid_mask)
{
	uint32_t data;
	mutex_lock(&msm_ispif_mut);
	switch (intftype) {
	case PIX0:
		data = msm_io_r(ispifbase + ISPIF_PIX_INTF_CID_MASK_ADDR);
		data |= cid_mask;
		msm_io_w(data, ispifbase + ISPIF_PIX_INTF_CID_MASK_ADDR);
		break;

	case RDI0:
		data = msm_io_r(ispifbase + ISPIF_RDI_INTF_CID_MASK_ADDR);
		data |= cid_mask;
		msm_io_w(data, ispifbase + ISPIF_RDI_INTF_CID_MASK_ADDR);
		break;

	case RDI1:
		data = msm_io_r(ispifbase + ISPIF_RDI_1_INTF_CID_MASK_ADDR);
		data |= cid_mask;
		msm_io_w(data, ispifbase + ISPIF_RDI_1_INTF_CID_MASK_ADDR);
		break;
	}
	mutex_unlock(&msm_ispif_mut);
}

int msm_ispif_abort_intf_transfer(struct msm_ispif_params *ispif_params)
{
	int rc = 0;
	uint8_t intf_cmd_mask = 0xAA;

	CDBG("abort stream request\n");
	mutex_lock(&msm_ispif_mut);
	msm_ispif_intf_cmd(ispif_params->intftype, ispif_params->cid_mask,
		 intf_cmd_mask);
	msm_ispif_intf_reset(ispif_params->intftype);
	global_intf_cmd_mask |= 0xFF<<(ispif_params->intftype * 8);
	mutex_unlock(&msm_ispif_mut);
	return rc;
}

int msm_ispif_start_intf_transfer(struct msm_ispif_params *ispif_params)
{
	uint32_t data;
	uint8_t intf_cmd_mask = 0x55;
	int rc = 0;

	CDBG("start stream request\n");
	mutex_lock(&msm_ispif_mut);
	switch (ispif_params->intftype) {
	case PIX0:
		data = msm_io_r(ispifbase + ISPIF_PIX_STATUS_ADDR);
		if ((data & 0xf) != 0xf) {
			CDBG("interface is busy\n");
			mutex_unlock(&msm_ispif_mut);
			return -EBUSY;
		}
		break;

	case RDI0:
		data  = msm_io_r(ispifbase + ISPIF_RDI_STATUS_ADDR);
		break;

	case RDI1:
		data  = msm_io_r(ispifbase + ISPIF_RDI_1_STATUS_ADDR);
		break;
	}
	msm_ispif_intf_cmd(ispif_params->intftype,
		ispif_params->cid_mask, intf_cmd_mask);
	mutex_unlock(&msm_ispif_mut);
	return rc;
}

int msm_ispif_stop_intf_transfer(struct msm_ispif_params *ispif_params)
{
	int rc = 0;
	uint8_t intf_cmd_mask = 0x00;
	CDBG("stop stream request\n");
	mutex_lock(&msm_ispif_mut);
	msm_ispif_intf_cmd(ispif_params->intftype,
		ispif_params->cid_mask, intf_cmd_mask);
	switch (ispif_params->intftype) {
	case PIX0:
		while ((msm_io_r(ispifbase + ISPIF_PIX_STATUS_ADDR) & 0xf)
			!= 0xf) {
			CDBG("Wait for Idle\n");
		}
		break;

	case RDI0:
		while ((msm_io_r(ispifbase + ISPIF_RDI_STATUS_ADDR) & 0xf)
			!= 0xf) {
			CDBG("Wait for Idle\n");
		}
		break;
	default:
		break;
	}
	global_intf_cmd_mask |= 0xFF<<(ispif_params->intftype * 8);
	mutex_unlock(&msm_ispif_mut);
	return rc;
}

int msm_ispif_config(struct msm_ispif_params *ispif_params, uint8_t num_of_intf)
{
	uint32_t data, data1;
	int rc = 0, i = 0;
	CDBG("Enable interface\n");
	data = msm_io_r(ispifbase + ISPIF_PIX_STATUS_ADDR);
	data1 = msm_io_r(ispifbase + ISPIF_RDI_STATUS_ADDR);
	if (((data & 0xf) != 0xf) || ((data1 & 0xf) != 0xf))
		return -EBUSY;
	msm_io_w(0x00000000, ispifbase + ISPIF_IRQ_MASK_ADDR);
	for (i = 0; i < num_of_intf; i++) {
		msm_ispif_sel_csid_core(ispif_params[i].intftype,
			ispif_params[i].csid);
		msm_ispif_enable_intf_cids(ispif_params[i].intftype,
			ispif_params[i].cid_mask);
	}
	msm_io_w(0x0BFFFFFF, ispifbase + ISPIF_IRQ_MASK_ADDR);
	msm_io_w(0x0BFFFFFF, ispifbase + ISPIF_IRQ_CLEAR_ADDR);
	return rc;
}

void msm_ispif_vfe_get_cid(uint8_t intftype, char *cids, int *num)
{
	uint32_t data = 0;
	int i = 0, j = 0;
	switch (intftype) {
	case PIX0:
		data = msm_io_r(ispifbase +
			ISPIF_PIX_INTF_CID_MASK_ADDR);
		break;

	case RDI0:
		data = msm_io_r(ispifbase +
			ISPIF_RDI_INTF_CID_MASK_ADDR);
		break;

	case RDI1:
		data = msm_io_r(ispifbase +
			ISPIF_RDI_1_INTF_CID_MASK_ADDR);
		break;

	default:
		break;
	}
	for (i = 0; i <= MAX_CID; i++) {
		if ((data & 0x1) == 0x1) {
			cids[j++] = i;
			(*num)++;
		}
		data >>= 1;
	}
}
