/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

/* Qualcomm Technologies, Inc. EMAC SGMII Controller driver.
 */

#include "emac_sgmii.h"
#include "emac_hw.h"

int emac_sgmii_config(struct platform_device *pdev, struct emac_adapter *adpt)
{
	struct emac_sgmii *sgmii;
	struct resource *res;
	int ret;

	sgmii = devm_kzalloc(&pdev->dev, sizeof(*sgmii), GFP_KERNEL);
	if (!sgmii)
		return -ENOMEM;

	ret = platform_get_irq_byname(pdev, "emac_sgmii_irq");
	if (ret < 0)
		return ret;

	sgmii->irq = ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emac_sgmii");
	if (!res) {
		emac_err(adpt,
			 "error platform_get_resource_byname(emac_sgmii)\n");
		return -ENOENT;
	}

	sgmii->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sgmii->base)) {
		emac_err(adpt,
			 "error:%ld devm_ioremap_resource(start:0x%lx size:0x%lx)\n",
			 PTR_ERR(sgmii->base), (ulong)res->start,
			 (ulong)resource_size(res));
		return -ENOMEM;
	}

	adpt->phy.private = sgmii;

	return 0;
}

void emac_sgmii_reset_prepare(struct emac_adapter *adpt)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 val;

	val = readl_relaxed(sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	writel_relaxed(((val & ~PHY_RESET) | PHY_RESET),
		       sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	/* Ensure phy-reset command is written to HW before the release cmd */
	wmb();
	msleep(50);
	val = readl_relaxed(sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	writel_relaxed((val & ~PHY_RESET),
		       sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	/* Ensure phy-reset release command is written to HW before initializing
	 * SGMII
	 */
	wmb();
	msleep(50);
}

/* LINK */
int emac_sgmii_init_link(struct emac_adapter *adpt, u32 speed, bool autoneg,
			 bool fc)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 val;
	u32 speed_cfg = 0;

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);

	if (autoneg) {
		val &= ~(FORCE_AN_RX_CFG | FORCE_AN_TX_CFG);
		val |= AN_ENABLE;
		writel_relaxed(val, sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	} else {
		switch (speed) {
		case EMAC_LINK_SPEED_10_HALF:
			speed_cfg = SPDMODE_10;
			break;
		case EMAC_LINK_SPEED_10_FULL:
			speed_cfg = SPDMODE_10 | DUPLEX_MODE;
			break;
		case EMAC_LINK_SPEED_100_HALF:
			speed_cfg = SPDMODE_100;
			break;
		case EMAC_LINK_SPEED_100_FULL:
			speed_cfg = SPDMODE_100 | DUPLEX_MODE;
			break;
		case EMAC_LINK_SPEED_1GB_FULL:
			speed_cfg = SPDMODE_1000 | DUPLEX_MODE;
			break;
		default:
			return -EINVAL;
		}
		val &= ~AN_ENABLE;
		writel_relaxed(speed_cfg,
			       sgmii->base + EMAC_SGMII_PHY_SPEED_CFG1);
		writel_relaxed(val, sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	}
	/* Ensure Auto-Neg setting are written to HW before leaving */
	wmb();

	return 0;
}

int emac_hw_clear_sgmii_intr_status(struct emac_adapter *adpt, u32 irq_bits)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 status;
	int i;

	writel_relaxed(irq_bits, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_CLEAR);
	writel_relaxed(IRQ_GLOBAL_CLEAR, sgmii->base + EMAC_SGMII_PHY_IRQ_CMD);
	/* Ensure interrupt clear command is written to HW */
	wmb();

	/* After set the IRQ_GLOBAL_CLEAR bit, the status clearing must
	 * be confirmed before clearing the bits in other registers.
	 * It takes a few cycles for hw to clear the interrupt status.
	 */
	for (i = 0; i < SGMII_PHY_IRQ_CLR_WAIT_TIME; i++) {
		udelay(1);
		status = readl_relaxed(sgmii->base +
				       EMAC_SGMII_PHY_INTERRUPT_STATUS);
		if (!(status & irq_bits))
			break;
	}
	if (status & irq_bits) {
		emac_err(adpt,
			 "failed to clear SGMII irq: status 0x%x bits 0x%x\n",
			 status, irq_bits);
		return -EIO;
	}

	/* Finalize clearing procedure */
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_IRQ_CMD);
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_CLEAR);
	/* Ensure that clearing procedure finalization is written to HW */
	wmb();

	return 0;
}

int emac_sgmii_init_ephy_nop(struct emac_adapter *adpt)
{
	return 0;
}

int emac_sgmii_autoneg_check(struct emac_adapter *adpt, u32 *speed,
			     bool *link_up)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 autoneg0, autoneg1, status;

	autoneg0 = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG0_STATUS);
	autoneg1 = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG1_STATUS);
	status   = ((autoneg1 & 0xff) << 8) | (autoneg0 & 0xff);

	if (!(status & TXCFG_LINK)) {
		*link_up = false;
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		return 0;
	}

	*link_up = true;

	switch (status & TXCFG_MODE_BMSK) {
	case TXCFG_1000_FULL:
		*speed = EMAC_LINK_SPEED_1GB_FULL;
		break;
	case TXCFG_100_FULL:
		*speed = EMAC_LINK_SPEED_100_FULL;
		break;
	case TXCFG_100_HALF:
		*speed = EMAC_LINK_SPEED_100_HALF;
		break;
	case TXCFG_10_FULL:
		*speed = EMAC_LINK_SPEED_10_FULL;
		break;
	case TXCFG_10_HALF:
		*speed = EMAC_LINK_SPEED_10_HALF;
		break;
	default:
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		break;
	}
	return 0;
}

int emac_sgmii_link_check_no_ephy(struct emac_adapter *adpt, u32 *speed,
				  bool *link_up)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 val;

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	if (val & AN_ENABLE)
		return emac_sgmii_autoneg_check(adpt, speed, link_up);

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_SPEED_CFG1);
	val &= DUPLEX_MODE | SPDMODE_BMSK;
	switch (val) {
	case DUPLEX_MODE | SPDMODE_1000:
		*speed = EMAC_LINK_SPEED_1GB_FULL;
		break;
	case DUPLEX_MODE | SPDMODE_100:
		*speed = EMAC_LINK_SPEED_100_FULL;
		break;
	case SPDMODE_100:
		*speed = EMAC_LINK_SPEED_100_HALF;
		break;
	case DUPLEX_MODE | SPDMODE_10:
		*speed = EMAC_LINK_SPEED_10_FULL;
		break;
	case SPDMODE_10:
		*speed = EMAC_LINK_SPEED_10_HALF;
		break;
	default:
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		break;
	}
	*link_up = true;
	return 0;
}

irqreturn_t emac_sgmii_isr(int _irq, void *data)
{
	struct emac_adapter *adpt = data;
	struct emac_sgmii *sgmii = adpt->phy.private;
	u32 status;

	emac_dbg(adpt, intr, "receive sgmii interrupt\n");

	do {
		status = readl_relaxed(sgmii->base +
				       EMAC_SGMII_PHY_INTERRUPT_STATUS) &
				       SGMII_ISR_MASK;
		if (!status)
			break;

		if (status & SGMII_PHY_INTERRUPT_ERR) {
			SET_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ);
			if (!TEST_FLAG(adpt, ADPT_STATE_DOWN))
				emac_task_schedule(adpt);
		}

		if (status & SGMII_ISR_AN_MASK)
			emac_check_lsc(adpt);

		if (emac_hw_clear_sgmii_intr_status(adpt, status) != 0) {
			/* reset */
			SET_FLAG(adpt, ADPT_TASK_REINIT_REQ);
			emac_task_schedule(adpt);
			break;
		}
	} while (1);

	return IRQ_HANDLED;
}

int emac_sgmii_up(struct emac_adapter *adpt)
{
	struct emac_sgmii *sgmii = adpt->phy.private;
	int ret;

	ret = request_irq(sgmii->irq, emac_sgmii_isr, IRQF_TRIGGER_RISING,
			  "sgmii_irq", adpt);
	if (ret)
		emac_err(adpt,
			 "error:%d on request_irq(%d:sgmii_irq)\n", ret,
			 sgmii->irq);

	/* enable sgmii irq */
	writel_relaxed(SGMII_ISR_MASK,
		       sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);

	return ret;
}

void emac_sgmii_down(struct emac_adapter *adpt)
{
	struct emac_sgmii *sgmii = adpt->phy.private;

	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);
	synchronize_irq(sgmii->irq);
	free_irq(sgmii->irq, adpt);
}

void emac_sgmii_tx_clk_set_rate_nop(struct emac_adapter *adpt)
{
}

/* Check SGMII for error */
void emac_sgmii_periodic_check(struct emac_adapter *adpt)
{
	struct emac_sgmii *sgmii = adpt->phy.private;

	if (!TEST_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ))
		return;
	CLR_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ);

	/* ensure that no reset is in progress while link task is running */
	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		goto sgmii_task_done;

	if (readl_relaxed(sgmii->base + EMAC_SGMII_PHY_RX_CHK_STATUS) & 0x40)
		goto sgmii_task_done;

	emac_err(adpt, "SGMII CDR not locked\n");

sgmii_task_done:
	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
}
